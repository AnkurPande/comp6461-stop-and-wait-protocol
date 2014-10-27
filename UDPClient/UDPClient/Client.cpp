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
#include <tchar.h>

#pragma comment(lib,"wsock32.lib")
#include "Client.h"

using namespace std;
using std::ofstream;

bool UDPClient::sendFile(int sock, char * fileName, char * sending_hostname, int server_number){}
int UDPClient::sendRequest(int sock, ThreeWayHandshake * handshake, SOCKADDR_IN * sa){}
int UDPClient::sendFrame(int sock, MessageFrame * frame){}
int UDPClient::sendFileAck(int sock, Acknowledgement * frame_ack){}

bool UDPClient :: recieveFile(int sock, char * fileName, char * serding_hostname, int client_number){}
RecieveResult UDPClient::ReceiveResponse(int sock, ThreeWayHandshake * handshake){}
RecieveResult UDPClient::ReceiveFrame(int sock, MessageFrame * frame){}
RecieveResult UDPClient::ReceiveFileAck(int sock, Acknowledgement * frame_ack){}

void UDPClient::printError(TCHAR* msg) {
	DWORD eNum;
	TCHAR sysMsg[256];
	TCHAR* p;

	eNum = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, eNum,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		sysMsg, 256, NULL);

	// Trim the end of the line and terminate it with a null
	p = sysMsg;
	while ((*p > 31) || (*p == 9))
		++p;
	do { *p-- = 0; } while ((p >= sysMsg) &&
		((*p == '.') || (*p < 33)));

	// Display the message
	_tprintf(TEXT("\n\t%s failed with error %d (%s)"), msg, eNum, sysMsg);
}

void UDPClient::run(){
	char server[INPÜT_LENGTH];
	char direction[INPÜT_LENGTH];
	char filename[FILENAME_LENGTH];
	char remotehost[HOSTNAME_LENGTH];
	char username[USERNAME_LENGTH];
	char hostname[HOSTNAME_LENGTH];
	unsigned long  username_length = (long)INFO_BUFFER_SIZE;
	bool bContinue = true;

	/*Initialize WinSocket*/
	if (WSAStartup(0x0202, &wsadata) != 0)
	{
		WSACleanup();
		printError("Error in starting WSAStartup()");
	}

	/*Get Username of client*/
	if (!GetUserName((TCHAR*)username, &username_length))
	{
		printError("Cannot Get the username");
	}

	/*Get Hostname of client*/
	if (gethostname(hostname, (int)HOSTNAME_LENGTH) != 0)
	{
		printError("Cannot Get the hostname");
	}

	/* Display program header */
	cout<<"\nClient Initiated at machine : "<< hostname;

	cout << "\nEnter server name : "; cin >> server;	// prompt user for server name
	// loop until username inputs 'Quit' as servername

	while (!strcmp(server,"Quit")){
		/*Create UDP socket for client*/
		if (sock = socket(AF_INET, SOCK_DGRAM, 0) < 0){ printError("Socket Failed");}

		/*Fill in UDP Port and address info */
		memset(&sa,0,sizeof(sa)); // Zero out structure
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
		sa.sin_port = htonl(CLIENT_PORT);

		/*Bind the socket*/
		if (bind(sock,(LPSOCKADDR)&sa,sizeof(sa))==SOCKET_ERROR){ printError("Error in binding socket");}

		srand((unsigned)time(NULL));
		random = rand() % MAX_RANDOM;

		/*User Input*/
		cout << "\nEnter Router HostName : " ;
		cin >> remotehost;
		cout << "\nEnter filename : ";
		cin >> filename;
		cout << "\nEnter the direction : ";
		cout << "\nEnter GET for download from server \n Enter PUT for upload to server " << endl;
		cin >> direction;

		/*Copy User_input to handshake*/
		strcpy(handshake.username, username);
		strcpy(handshake.hostname, hostname);
		strcpy(handshake.filename, filename);
		
		if (bContinue){
			/*Router network Information*/
			struct hostent *rp;
			rp = gethostbyname(remotehost);
			memcpy(&sa_in.sin_addr, rp->h_addr, rp->h_length);
			sa_in.sin_family = AF_INET;
			sa_in.sin_port = htonl(REMOTE_PORT);
			sa_in.sin_addr.s_addr = htonl(INADDR_ANY);

			/*File Direction*/
			if (strcmp(direction, "GET") == 0){
				handshake.direction = GET;
			}
			else if (strcmp(direction, "PUT") == 0){
				//Check that file exist if(condition) {
				handshake.direction = PUT;
				//} else {cout<<"File not exist";}
			}
			else
				cout << "\nINVALID: Use GET/PUT only";
			
			//Setting up properties of handshake object.
			handshake.client_number = random;
			handshake.packet_type = HANDSHAKE;
			handshake.handshake_type = CLIENT_REQ;

			/*Initiating handshaking*/
			do{
				if (sendRequest(sock, &handshake, &sa) != sizeof(handshake))
					printError("\nError in starting handshake");

				cout << "\nClient sent successfully client number :C" << handshake.client_number;
				if (TRACE1) { fout << "\nClient sent successfully client number :C" << handshake.client_number; }
			} while (ReceiveResponse(sock, &handshake) != INCOMING_PACKET);

			//check how server responds
			if (handshake.handshake_type == FILE_NOT_EXIST){
				printError("Rquested file does not exist remotely");
			}
			else if (handshake.handshake_type == INVALID){
				printError("Error in recieving response");
			}

			//Continue with file transfer only if server acknowledge appropriately.
			if (handshake.handshake_type == ACK_CLIENT_NUM){//Sent the client acknowledgement
				cout
					<< "\nClient successfully recieved client number : C" << handshake.client_number
					<< "\nClient successfully recieved the server number :S" << handshake.server_number;
				if (TRACE1){
					fout
						<< "\nClient successfully recieved client number :C" << handshake.client_number
						<< "\nClient successfully recieved the server number :S" << handshake.server_number;
				}

				//Third Shake.
				handshake.handshake_type = ACK_SERVER_NUM;
				int sequence_number = handshake.server_number % 2;
				if (sendRequest(sock,&handshake,&sa)!=sizeof(handshake)){
					printError("Error in sending packet");

					cout
						<< "\nClient successfully sent the server number :S" << handshake.server_number;
					if (TRACE1){
						fout
							<< "\nClient successfully sent the server number :S" << handshake.server_number;
					}
				}
				//Checking Directions.
				switch (handshake.direction){
				case GET:
					if (!recieveFile(sock, filename, hostname, handshake.client_number))
						printError("Error in recieving file");
					break;
				case PUT:
					if (!sendFile(sock, filename, hostname, handshake.server_number))
						printError("Error in sending file");
					break;
				default:
					break;
				}//Switch Close.
			}//If close(Acknowledgment)			
		}//If close (bContinue)
		cout << "Closing socket";
		closesocket(sock);
	}//While loop Close
}
UDPClient::UDPClient(char * fn) // constructor
{
	/* For timeout timer */
	timeouts.tv_sec = STIMER;
	timeouts.tv_usec = UTIMER;

	/* Open the log file */
	fout.open(fn);
}

UDPClient::~UDPClient() // destructor
{
	/* Close the log file */
	fout.close();

	/* Uninstall winsock.dll */
	WSACleanup();
}
int main(int argc, char *argv[]) {

	UDPClient * cli = new UDPClient();
	cli->run();
	return 0;
}