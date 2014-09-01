#ifndef SERVER_H
#define SERVER_H

#include <cstdlib>
#include <iostream>
#include <string>
#include <winsock2.h>

#include <pthread/pthread.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;
class Server {
public:
	Server(int serverPort, int backLog, int dataSize);
	int initServer();
	void listenEMF();
	string getMessage();
protected:
	int port;
	int backlog;
	int datasize;
	
	int fd,fd2,num,n_rcv;
	struct sockaddr_in server;
	struct sockaddr_in client;
	char buf[3],dato[512];
	char buf_aux[3];
	int sin_size;	
	std::string msg;
};

#endif