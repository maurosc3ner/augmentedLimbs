#include <cstdlib>
#include <iostream>
#include <winsock2.h>

#include <pthread/pthread.h>
#pragma comment(lib, "ws2_32.lib")

//sockets
#define PORT 100
#define BACKLOG 2
#define DATASIZE 512

WSADATA WSAData;//We need to check the version.
WORD Version=MAKEWORD(2,2);
int fd,fd2,num,i,count=0,n_rcv;
struct sockaddr_in server;
struct sockaddr_in client;
char buf[3],dato[DATASIZE];
char buf_aux[3];
int sin_size;	

int initServer();
void * listenEMF(void *x_void_ptr);

/* init socket */
int initServer()
	//int initServer(WSADATA *wsa,WORD *v)
{
	if(WSAStartup(Version,&WSAData)!=0){
		std::cout<<"This version is not supported! - \n"<<WSAGetLastError()<<std::endl;
	}
	else{
		std::cout<<"Good - Everything fine!\n"<<std::endl;
	}

	if((fd=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		printf("error en socket()\n");
		return -1;
	}
	server.sin_family=AF_INET;
	server.sin_port=htons(PORT);
	server.sin_addr.s_addr=INADDR_ANY;

	if(bind(fd,(struct sockaddr*)&server,sizeof(struct sockaddr))==-1)
	{
		printf("error en bind()\n");
		return -1;
	}

	if(listen(fd,BACKLOG)==-1)
	{
		printf("error en listen()\n");
	}
	sin_size=sizeof(struct sockaddr_in);
	printf("Esperando conexion...\n");
	return 0;
}

void * listenEMF(void *x_void_ptr)
{
	printf("inicio pthread de accept() \n");

	while(1)
		{

			if((fd2=accept(fd,(struct sockaddr*)&client,&sin_size))==-1)
			{
					printf("error en accept() \n");
			}
			printf("Conexion establecida desde: %d\n",inet_ntoa(client.sin_addr));
			int dLeido;
			//Mientras el mismo cliente no cierre la conexion
			while(1)
			{
				buf[0]='\0';
				if((n_rcv=recv(fd2,buf,512,0))<=0)
				{
					printf("Conexion terminada desde: %d\n",inet_ntoa(client.sin_addr));
					printf("Esperando conexiones...\n");
					closesocket(fd2);
					break;
				}
				buf[n_rcv]='\0';
				fprintf(stderr,"Cliente envio: %s\n",buf);
				fprintf(stderr," %d \n",n_rcv); 
			}
		}
		closesocket(fd2);
		closesocket(fd);
		printf("fin pthread de accept() \n");
}