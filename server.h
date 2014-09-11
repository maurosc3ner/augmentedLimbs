#ifndef SERVER_H
#define SERVER_H

#include <cstdlib>
#include <iostream>
#include <string>
#include <winsock2.h>

#include <pthread/pthread.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

class MyThreadClass
{
public:
   MyThreadClass() {/* empty */}
   virtual ~MyThreadClass() {/* empty */}

   /** Returns true if the thread was successfully started, false if there was an error starting the thread */
   bool StartInternalThread()
   {
      return (pthread_create(&_thread, NULL, InternalThreadEntryFunc, this) == 0);
   }

   /** Will not return until the internal thread has exited. */
   void WaitForInternalThreadToExit()
   {
      (void) pthread_join(_thread, NULL);
   }

protected:
   /** Implement this method in your subclass with the code you want your thread to run. */
   virtual void InternalThreadEntry() = 0;

private:
   static void * InternalThreadEntryFunc(void * This) {((MyThreadClass *)This)->InternalThreadEntry(); return NULL;}

   pthread_t _thread;
};


class Server: public MyThreadClass {
public:
	Server(int serverPort, int backLog, int dataSize);
	int initServer();
	//void * listenEMF(void *);
	string getMessage();
	void InternalThreadEntry();

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