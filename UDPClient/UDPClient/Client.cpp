// UDP Client 

#include <winsock.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <time.h>

#pragma comment(lib,"wsock32.lib")
#include "Client.h"

using namespace std;
using std::ofstream;

bool sendFile(int sock, char * fileName, char * sending_hostname, int server_number){}
int sendRequest(int sock, ThreeWayHandshake * handshake, SOCKADDR_IN * sa){}
int sendFrame(int sock, MessageFrame * frame){}
int sendFileAck(int sock, Acknowledgement * frame_ack){}

bool recieveFile(int sock, char * fileName, char * serding_hostname, int client_number){}
RecieveResult ReceiveResponse(int sock, ThreeWayHandshake * handshake){}
RecieveResult ReceiveFrame(int sock, MessageFrame * frame){}
RecieveResult ReceiveFileAck(int sock, Acknowledgement * frame_ack){}

void run(){}

int main(int argc, char *argv[]) {

	UDPClient * cli = new UDPClient();
	cli->run();
	return 0;
}