#include "Server.h"
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include "windows.h"
#endif

using namespace std;

Server *localServer;

/************************************
 *
 ************************************/
int main(int argc,char **argv)
{
	/// create server on port 100, max clients 2 and 512 of message length 
	localServer=new Server(100,2,512);

	/* start communication */
	if(localServer->initServer()==-1){
		fprintf(stderr, "Error creating server\n");
	}
	int xThread;

	localServer->StartInternalThread();
}
