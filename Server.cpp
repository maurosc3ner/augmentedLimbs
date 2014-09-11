#include "Server.h"



Server::Server(int serverPort=100, int backLog=2,int dataSize=512) {
  port=serverPort;
  backlog=backLog;
  datasize=dataSize;
  msg="";
}

/* init socket */
int Server::initServer()
{
	WSADATA WSAData;//We need to check the version.
	WORD Version=MAKEWORD(2,2);
	if(WSAStartup(Version,&WSAData)!=0){
		std::cout<<"This version is not supported! - \n"<<WSAGetLastError()<<endl;
	}
	else{
		cout<<"Good - Everything fine!\n"<<endl;
	}

	if((fd=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		printf("error en socket()\n");
		return -1;
	}
	server.sin_family=AF_INET;
	server.sin_port=htons(port);
	server.sin_addr.s_addr=INADDR_ANY;

	if(bind(fd,(struct sockaddr*)&server,sizeof(struct sockaddr))==-1)
	{
		printf("error en bind()\n");
		return -1;
	}

	if(listen(fd,backlog)==-1)
	{
		printf("error en listen()\n");
	}
	sin_size=sizeof(struct sockaddr_in);
	printf("Esperando conexion...\n");
	return 0;
}


void Server::InternalThreadEntry()
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
				//fprintf(stderr,"Cliente envio: %s\n",buf);
				msg=string(buf);
				
				//fprintf(stderr," %d \n",n_rcv); 
			}
		}
		closesocket(fd2);
		closesocket(fd);
		printf("fin pthread de accept() \n");
}

string Server::getMessage(){
	return msg;
}