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

bool fileExist(char * filePath){
	struct _stat buf;
	int result;
	result = _stat(filePath, &buf);
	return (result == 0);
}

long getFileSize(char *filename){
	struct _stat buf;
	int result;
	result = _stat(filename, &buf);
	if (result == 0)
		return buf.st_size;
	else return 0;
}

bool UDPClient::sendFile(int sock, char * fileName, char * sending_hostname, int server_number){
	
	MessageFrame frame; frame.packet_type = FRAME;
	Acknowledgement ack; ack.number = -1;
	bool bFirstPacket = true, bFinished = false,bMaxAttempts =false;
	long bytes_counter =0, bytes_count =0;
	int bytes_read_total=0, bytes_read =0, bytes_sent =0;
	int packets_sent =0, packets_actual_needed=0;
	int nTries =0;
	int sequence_number = server_number % 2;
	
	cout << "\nSender started on " << sending_hostname;
	if (TRACE1){ fout << "\nSender started on " << sending_hostname; }

	FILE * file = fopen(fileName,"r+b");
	if (file != NULL)
	{
		bytes_counter = getFileSize(fileName);
		while (1)
		{
			if (bytes_counter > MAX_FRAME_SIZE)
			{
				frame.header = (bFirstPacket ? INITIAL_DATA : DATA);
				bytes_count = MAX_FRAME_SIZE;
			}
			else
			{
				bytes_count = bytes_counter;
				bFinished = true;
			}
			bytes_counter -= MAX_FRAME_SIZE;

			//read bytes in buffer
			bytes_read = fread(frame.buffer, sizeof(char), bytes_count, file);
			frame.buffer_length = bytes_count;
			frame.snwseq = sequence_number;
			bytes_read_total += bytes_read;

			nTries = 0;
			do
			{
				nTries++;//Only used for the last frame ie if no ack arrive and client may be gone.
				if (sendFrame(sock, &frame) != sizeof(frame))
					return false;
				if (nTries == 1)
					packets_actual_needed++; //Increment only if the packet sent once.
				packets_sent++;
				bytes_sent += sizeof(frame);//Keep counter of total bytes actual sent.
				if (bFinished && (nTries > MAX_RETRIES))
				{
					bMaxAttempts = true;
					break;
				}
			} while (receiveFileAck(sock, &ack) != INCOMING_PACKET || ack.number != sequence_number);
			
			if (bMaxAttempts)
			{//Max attempt achieved for the ACK of final packet.
				cout << "Sender: did not receive ACK " << sequence_number << " after " << MAX_RETRIES << " tries. Transfer finished." << endl;
				if (TRACE1) { fout << "Sender: did not receive ACK " << sequence_number << " after " << MAX_RETRIES << " tries. Transfer finished." << endl; }
			}
			else 
			{
				cout << "Sender: Received ACK " << sequence_number  << endl;
				if (TRACE1) { fout << "Sender: Received ACK " << sequence_number  << endl; }
			}
			
			/*False the counter for first packet*/
			bFirstPacket = false;
			
			/*Invert the sequence number for the secong dispatch*/
			sequence_number = (sequence_number == 0 ? 1 : 0);
			
			/*Break the loop if sending finished*/
			if (bFinished)
			{
				break;
			}
		}
		fclose(file);
		cout << endl
			<< "File Transferred Completely" << endl
			<< "Total no of bytes read :" << bytes_read_total << endl
			<< "Total no of bytes sent :" << bytes_sent << endl
			<< "Total no of packets actually required to sent :" << packets_actual_needed << endl
			<< "Total no of packets sent in actual :" << packets_sent << endl;
		if (TRACE1)
		{
			fout << endl
				<< "File Transferred Completely" << endl
				<< "Total no of bytes read :" << bytes_read_total << endl
				<< "Total no of bytes sent :" << bytes_sent << endl
				<< "Total no of packets actually required to sent :" << packets_actual_needed << endl
				<< "Total no of packets sent in actual :" << packets_sent << endl;
		}
		return true;
	}
	else 
	{
		cout << "Problem opening the file";
		if (TRACE1){ fout << "Problem opening the file"; }
		return false;
	}
}

int UDPClient::sendRequest(int sock, ThreeWayHandshake * ptr_handshake, struct sockaddr_in * sa_in)
{
	return sendto(sock, (const char *)ptr_handshake, sizeof(*ptr_handshake), 0, (struct sockaddr *)sa_in, sizeof(*sa_in));
}

int UDPClient::sendFrame(int sock, MessageFrame * frame)
{
	return sendto(sock,(const char*)frame,sizeof(*frame),0,(struct sockaddr*)&sa_in,sizeof(sa_in));
}
int UDPClient::sendFileAck(int sock, Acknowledgement * frame_ack)
{
	return sendto(sock,(const char*)frame_ack,sizeof(*frame_ack),0,(struct sockaddr *)&sa_in,sizeof(sa_in));
}

bool UDPClient :: recieveFile(int sock, char * fileName, char * sending_hostname, int client_number){}

RecieveResult UDPClient::receiveResponse(int sock, ThreeWayHandshake * ptr_handshake){
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
	int bytes_recvd = 0;
	int outfds = select(1, &readfds, 0, 0, &timeouts);
	if(outfds == 0){
		return TIMEOUT;
	}
	else if (outfds > 0){
		bytes_recvd = recvfrom(sock, (char*)ptr_handshake, sizeof(ptr_handshake), 0, (struct sockaddr*)&sa_in, &sa_in_size);
		return INCOMING_PACKET;
	}
	else 
	return RECIEVE_ERROR;
}
RecieveResult UDPClient::receiveFrame(int sock, MessageFrame * frame){
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(sock,&readfds);
	int bytes_recvd = 0;
	int outfds = select(1, &readfds, 0, 0, &timeouts);
	if (outfds > 0){
		bytes_recvd = recvfrom(sock, (char *)frame, sizeof(*frame), 0, (struct sockaddr*)&sa_in, &sa_in_size);
		return INCOMING_PACKET;
	}
	else if (outfds ==0){
		return TIMEOUT;
	}
	else 
	return RECIEVE_ERROR;
}
RecieveResult UDPClient::receiveFileAck(int sock, Acknowledgement * frame_ack){
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
	int bytes_recvd;
	int outfds = select(1, &readfds, 0, 0, &timeouts);
	if (outfds == 0){
		return TIMEOUT;
	}
	else if (outfds > 0){
		bytes_recvd = recvfrom(sock,(char *)frame_ack,sizeof(frame_ack),0,(struct sockaddr*)&sa_in,&sa_in_size);
		return INCOMING_PACKET;
	}
	else return RECIEVE_ERROR;
}

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
				if (fileExist(filename))
					handshake.direction = PUT;
				else printError("File Not Present At Client Side");
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
			} while (receiveResponse(sock, &handshake) != INCOMING_PACKET);

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
		cout << "\nClosing socket";
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