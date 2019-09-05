#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<fcntl.h>
#include<signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/select.h> // 최대 소켓 수(FD_SETSIZE : 1024)
#include <limits.h> // 최대 스레드 수 : 64
#define MAX_LINE 511
#define MAX_BUFSZ 1000
#define MAX_CHAT _POSIX_THREAD_THREADS_MAX - 1
#define MAX_SOCKET FD_SETSIZE
// global variables for inform
char *HOW_TO_USE = "\n====================[HOW TO USE]====================\nClient List : /list \nWisper Function : /w [client ID] (what you want to send) \nExit : /exit\n====================================================";
char *LIST1 = "\n===================[CLIENT LIST]====================\n";
char *LIST2 = "\n====================================================";
char *LIST_STRING = "/list";
char *WHISPER_STRING = "/w";
char *EXIT_STRING = "/exit";
char *START_STRING= "Welcome to Chat!";
char global_buf[MAX_LINE];


// global variables
pthread_mutex_t count_lock;
pthread_mutexattr_t mutex_attr;
// thread, socket, ip address, port  한번에 관리
typedef struct{
	int cli_sock;
	pthread_t tid;
	char ip_addr[20];
	char port[5];

}map_t;

typedef struct{
	int cli_sock;
	char id[20];
}cli_id;

typedef struct{
	int socket_id;
	struct sockaddr_in cliaddr;
}addClient_Pass_Type;

map_t cli_info[MAX_CHAT];
cli_id cli_list[MAX_CHAT];

int listen_sock;
int num_chat =0;
pthread_t tid[MAX_CHAT];
// functions 
void *addClient(void *);
void removeClient(int sock);
void recv_and_send(int sock);
void tcp_listen(int host,int port,int backlog);
int find_index(int socket);

void errquit(char *msg){
	perror(msg);
	exit(-1);
}

void thr_errquit(char *msg, int errcode){
	printf("%s %s\n",msg,strerror(errcode));
	pthread_exit(NULL);
}

// client socket initial
void s_init(){
	int i;
	for(i=0;i<MAX_CHAT;i++){
		cli_info[i].cli_sock= -1;
		cli_list[i].cli_sock= -1;
	}
}

int main(int argc, char *argv[]){
		int i,j,port,status;
	int accp_sock;
	unsigned int clilen;
	struct sockaddr_in cliaddr;
	
	// int s, sockaddr_in cliaddr;
	addClient_Pass_Type p;
	if(argc != 2){
		printf("사용법 %s port \n",argv[0]);
		exit(0);
	}
	port = atoi(argv[1]);
	clilen = sizeof(cliaddr);
		printf("\n\n\n");
	printf("====================================================\n");
	printf("\t\tChatting Sever Start \n");
	printf("\t\t-Version.0.1.06.13\n");
	printf("\t\t-Made by Kms Jjs \n");
	printf("\tMax Client Number in Server: %d\n", MAX_CHAT);
	printf("====================================================\n");
	
	s_init();
	pthread_mutexattr_init(&mutex_attr);	// mutex_attr == NULL
	pthread_mutex_init(&count_lock, &mutex_attr);
	tcp_listen(INADDR_ANY, port, 5);
	i=0;
		
		while(1){
		accp_sock = accept(listen_sock, (struct sockaddr *)&cliaddr, &clilen);
		if(accp_sock == -1)
			errquit("Accept fail\n");
			
		p.socket_id= accp_sock;
		// bcopy(source , destination, size)
		bcopy((struct sockaddr_in *)&cliaddr, &(p.cliaddr) ,clilen);
		// pthread_create(thread_id , attribute, function, parameter)
		// parameter p == ( p.s  = accp_sock, p.cliaddr = client_address)
		status = pthread_create(&tid[i], NULL, addClient, (void *)&p);
		pthread_detach(tid[i]);
		if(status != 0)
			thr_errquit("pthread create", status);
		
		//memset(&p,0,sizeof(addClient_Pass_Type));
		i++;
		memset(&cliaddr,0,sizeof(struct sockaddr_in)); // +
	}
}// socket ->bind -> listen 

void tcp_listen(int host, int port ,int backlog){
	struct sockaddr_in servaddr;
	int accp_sock;
	printf("LISTEN THREAD(main function thread id) : %lu \n", pthread_self());
	// create socket 
	if( (listen_sock = socket(AF_INET,SOCK_STREAM, 0)) < 0)
		errquit("SOCKET FAIL");
	bzero((char *)&servaddr, sizeof(servaddr));
	servaddr.sin_family =AF_INET;
	servaddr.sin_addr.s_addr = htonl(host); // host == INADDR_ANY 
	servaddr.sin_port = htons(port);	// port == argv's port number
	if(bind(listen_sock, (struct sockaddr *)&servaddr, sizeof(servaddr))<0){
		perror("bind fail");
		exit(1);
	}
	listen(listen_sock, backlog);
	printf("LISTEN SOCKET SUCCESS!!\n");
}// called_arg == client socket Id , client socket address

void *addClient(void* called_arg){
	int i, new_index;
	char notify_cli[55];
	// addClient_Pass_Type  == int s, struct sockaddr_in cliaddr;
	// my_arg : new Client socket Id, Client socket address(struct)
	addClient_Pass_Type *my_arg;
	my_arg = (addClient_Pass_Type*)called_arg;
	// write() = send data to another host "Welcome to Chat Server
	if( (write(my_arg->socket_id, START_STRING, strlen(START_STRING))) == -1){
		printf("THREAD (%lu): Start Send Error \n", pthread_self());
		pthread_exit(NULL);
	}
	pthread_mutex_lock(&count_lock);
	// checking the cli_info[MAX_CHAT-1] is full MAX_CHAT-1 == 63-1 == 62
	if(cli_info[MAX_CHAT-1].cli_sock != -1){
		printf("Chatting room is full\n");
		pthread_exit(NULL);
	}
	// update the socket number in the global variable cli_info
	for(i = 0;i < MAX_CHAT; i++){
		if(cli_info[i].cli_sock != -1)
			continue;
		else{
			new_index = i;
			break;
		}
	}
	// cli_info[new_index]'s variable input process
	// cli_info.sock
	cli_info[new_index].cli_sock = my_arg->socket_id;
	cli_list[new_index].cli_sock = my_arg->socket_id;
	// cli_info.tid
	cli_info[new_index].tid = pthread_self();
	// cli_info.ip_addr
	inet_ntop(AF_INET, 
			&(my_arg->cliaddr.sin_addr),
			cli_info[new_index].ip_addr,
			sizeof(cli_info[new_index].ip_addr));
	// cli_info.port
	sprintf(cli_info[new_index].port, "%d", ntohs(my_arg->cliaddr.sin_port));
	
	
	//print Information About new Client

	printf("\n\n===================[Announcement]===================\n"); 
	printf("New Client [Socket Nubmer : %d] \n",cli_info[new_index].cli_sock);
	printf("\t   [Thread Id : %lu] \n",cli_info[new_index].tid);
	printf("\t   [Client Address&Port : %s:%s]\n",cli_info[new_index].ip_addr, cli_info[new_index].port);
	printf("====================================================\n");
	// making the announcement 
	// strncpy ( destination, source, size)
	strncpy(notify_cli, cli_info[new_index].ip_addr, 20);
	strcat(notify_cli,":");
	strcat(notify_cli, cli_info[new_index].port);
	strcat(notify_cli," new client login");
	// notify_cli ="117.17.73.143:5678 new client login\n"
	// To inform all chat client that new Client is login
	for(i=0;i<MAX_CHAT;i++){
		if(num_chat == 0){
			break;
		}
		// checking sokcet nubmer if it is mine or empty
		if(cli_info[i].tid == cli_info[new_index].tid){
			continue;
		}
		else{
			if(cli_info[i].cli_sock != -1){	
			//	printf("Server Send New Connection [%s:%s] \n" ,cli_info[new_index].ip_addr, cli_info[new_index].port);
				if(write(cli_info[i].cli_sock,notify_cli,strlen(notify_cli)) <=0){
					perror("Write Error");
					pthread_exit(NULL);
				}
			}
		}
	}
	num_chat++;
	memset(&notify_cli,0,sizeof(notify_cli));
		pthread_mutex_unlock(&count_lock);
	recv_and_send(cli_info[new_index].cli_sock);
}



void recv_and_send(int sock){
	int i, n , alen, tlen;
	char sendmsg[MAX_BUFSZ];memset(sendmsg,0,sizeof(sendmsg));
	char recvmsg[MAX_BUFSZ];memset(recvmsg,0,sizeof(recvmsg));
	int login = 0; // variable to check it is first time 
	int index;
	char *whisper; // for checking the id to whisper
	
	while( (n = read(sock,recvmsg,MAX_BUFSZ)) > 0 ){

		index = find_index(sock);
		printf("Message from client[%s] > %s\n",cli_list[index].id  ,recvmsg);

		// checking the first time login for saving Id
		if(login  == 0){
			strcat(cli_list[index].id,recvmsg);
			memset(recvmsg,0,sizeof(recvmsg));
				
			strcat(sendmsg,HOW_TO_USE);
			write(cli_info[index].cli_sock,sendmsg,MAX_BUFSZ);
			memset(sendmsg,0,sizeof(sendmsg));
			login = 1;
			continue;	
		}

		// for send Client ID list 
		else if(strstr(recvmsg, LIST_STRING) != NULL){
			strcat(sendmsg,LIST1);
			for(i =0; i<=num_chat;i++){
				if(cli_list[i].cli_sock !=-1){
					strcat(sendmsg,cli_list[i].id);
					strcat(sendmsg,"\t");
				}
			}
			strcat(sendmsg,LIST2);
			write(cli_info[index].cli_sock,sendmsg,MAX_BUFSZ);
			memset(sendmsg,0,sizeof(sendmsg));

		}
		// for whispering 
		else if(strstr(recvmsg,WHISPER_STRING) != NULL){
             strcat(sendmsg,"Message from[");
             strcat(sendmsg,cli_list[index].id);
             strcat(sendmsg,"]");
			 whisper = strtok(recvmsg, " ");
		     whisper = strtok(NULL, " ");
			 
             strcat(sendmsg,&recvmsg[4+strlen(whisper)]);
 
             for(i=0;i<=num_chat;i++){

                 if(cli_info[i].tid == cli_info[index].tid){
                     continue;
                 }
 
                 else{
                     if(cli_info[i].cli_sock != -1 && strcmp(cli_list[i].id,whisper) == 0 ){
                         if(write(cli_info[i].cli_sock,sendmsg,MAX_BUFSZ) <= 0){
                             perror("Write Error");
                             pthread_exit(NULL);
                         }
 
                     }
                 }
             } 
 
             memset(sendmsg,0,sizeof(sendmsg));
             memset(recvmsg,0,sizeof(recvmsg));
         }




		// For exiting Chatting server 
		else if(strstr(recvmsg, EXIT_STRING) != NULL){
			printf("\n\n===================[Announcement]===================\n"); 
			printf("[%s:%s]: Exit Chatting room \n", cli_info[index].ip_addr, cli_info[index].port);
			printf("The Number of Remain Member : %d\n",num_chat-1);
			printf("====================================================\n");
			removeClient(sock);
			break;
		}

		// Sending Message to All client 
		else{
			strcat(sendmsg,"[");
			strcat(sendmsg,cli_list[index].id);
			strcat(sendmsg,"]");
			strcat(sendmsg,recvmsg);

			for(i=0;i<=num_chat;i++){

				if(cli_info[i].tid == cli_info[index].tid){
					continue;
				}

				else{

					if(cli_info[i].cli_sock != -1){	
						if(write(cli_info[i].cli_sock,sendmsg,MAX_BUFSZ) <= 0){
							perror("Write Error");
							pthread_exit(NULL);
						}

					}
				}
			}


			memset(sendmsg,0,sizeof(sendmsg));
			memset(recvmsg,0,sizeof(recvmsg));
		}
	}
}



void removeClient(int sock){
	int index;
	pthread_mutex_lock(&count_lock);
	index = find_index(sock);
	memset(&cli_info[index],0,sizeof(map_t));
	cli_info[index].cli_sock = -1;
	cli_list[index].cli_sock = -1;
	close(sock);
	num_chat--;
	pthread_mutex_unlock(&count_lock);
}



int find_index(int socket){
	int i, index;
	for(i=0;i<MAX_CHAT;i++){
		if(cli_info[i].cli_sock == socket){
			index = i;
			break;
		}
	}
	return index;
}
//int find_index(char )
