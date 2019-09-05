#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<strings.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/time.h>
#include<unistd.h>
#include<arpa/inet.h>
#define MAXLINE 1000

char *EXIT_STRING = "/exit";

int tcp_connect(int, char*, unsigned short);
int first = 0;

void errquit(char *msg){
	perror(msg);
	exit(1);
}


int main(int argc, char *argv[]){
	char sendmsg[MAXLINE]; memset(sendmsg,0,MAXLINE);
	char recvmsg[MAXLINE]; memset(recvmsg,0,MAXLINE);
	int maxfdp, sock;
	
	char id[30]; //
	
	fd_set read_fds;

	if(argc != 4){
		printf("How to use : %s server_ip port name \n", argv[0]);
		exit(0);
	}
	
	sock = tcp_connect(AF_INET, argv[1], atoi(argv[2]));
	if(sock == -1)
		errquit("tcp_connect fail");
	printf("Connection Success\n");
	maxfdp = sock+1;	// the number of maximum file discriptor 
	FD_ZERO(&read_fds);
	
	  
	
	while(1){
		FD_SET(0, &read_fds);
		if(first ==0){
			
			memset(id,0,sizeof(id));
			memset(sendmsg,0,sizeof(sendmsg));
			strcat(id,argv[3]);
			strcat(sendmsg,id);
			send(sock,sendmsg,strlen(sendmsg),0);
			memset(sendmsg,0,sizeof(sendmsg));
			first =1;	
			continue;
		}

		FD_SET(sock, &read_fds);
		
		if((select(maxfdp, &read_fds, NULL, NULL, NULL)) <0)
			errquit("select fail");
		if(FD_ISSET(sock, &read_fds)){
			if( (recv(sock, recvmsg, MAXLINE, 0)) > 0){
				if(recvmsg[0] != 0){
					printf("%s \n", recvmsg);
				}
			}
			memset(recvmsg,0,sizeof(recvmsg));
		}
		if(FD_ISSET(0, &read_fds)){
			memset(sendmsg,0,sizeof(sendmsg));
			if(fgets(sendmsg, MAXLINE, stdin)){
				sendmsg[strlen(sendmsg)-1] = '\0';	// Removing \n
				if( (send(sock,sendmsg, strlen(sendmsg), 0)) < 0)
					puts("Error : Write error on socket.");	
				if( (strstr(sendmsg, EXIT_STRING)) != NULL){
					puts("Good bye ~");
					close(sock);
					exit(0);
				}
			memset(sendmsg,0,sizeof(sendmsg));
			}
		}
		
	}
}

int tcp_connect(int AF, char *servIP, unsigned short port){
	struct sockaddr_in servaddr;
	int sock;
	if( (sock = socket(AF, SOCK_STREAM, 0)) < 0 )
		return -1;
	bzero((char *)&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF;
	inet_pton(AF_INET, servIP, &servaddr.sin_addr);
	servaddr.sin_port = htons(port);
	if( (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr))) < 0 )
		return -1;
	
	return sock;
}
	

