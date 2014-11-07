// UDP Client 
/* Client */


#include <winsock.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <time.h>
#include<tchar.h>
#include<vector>
#include "Client.h"
#include "Shlwapi.h"
#pragma comment(lib,"wsock32.lib")
#pragma comment (lib,"Shlwapi.lib") 

using namespace std;
using std::ofstream;

void deleteFile(string s)
{
	char  filename[150] = { "\0" };
	cout << "Enter the file name : ";
	cin >> filename;

	string s1 = s;
	string s2 = filename;
	s1.append("\\");
	s1.append(s2);
	s1.copy(filename, 150);
	cout << filename << endl;

	if (remove(filename) != 0)
		perror("\nError deleting file.\n");
	else
		puts("\nFile successfully deleted.\n");

}

void list(string s){

	WIN32_FIND_DATA file_data;
	HANDLE hFile;
	vector<string> files;

	string dir = s;
	

	hFile = FindFirstFile((dir + "/*").c_str(), &file_data);

	cout << file_data.cFileName;
	do{
		string fileName = file_data.cFileName;
		files.push_back(fileName);
	} while ((FindNextFile(hFile, &file_data)) != 0);

	cout << endl
		<< "========================" << endl;
	for (auto & i : files){
		cout <<i << endl;
	}
	cout << endl
		<< "========================" << endl;
}

bool FileExists(char * filename)
{
	int result;
	struct _stat stat_buf;
	result = _stat(filename, &stat_buf);
	switch (errno){
	case  ENOENT:
		cout << "File not exist";
	case EINVAL:
		cout << "Path invalid";
	default:
		//cout << "Unexpected error";
	}
	return (result == 0);
}

long GetFileSize(char * filename)
{
	int result;
	struct _stat stat_buf;
	result = _stat(filename, &stat_buf);
	if (result != 0) return 0;
	return stat_buf.st_size;
}

/* SENDING FUNCTIONS */
bool UDPClient::SendFile(int sock, char * filename, char * sending_hostname, int server_number)
{

	MessageFrame frame; frame.packet_type = FRAME;
	Acknowledgement ack; ack.number = -1;
	long bytes_counter = 0, byte_count = 0;
	int bytes_sent = 0, bytes_read = 0, bytes_read_total = 0;
	int packetsSent = 0, packetsSentNeeded = 0;
	bool bFirstPacket = true, bFinished = false;
	int sequence_number = server_number % 2; // Initial bit-sequence expected by the client
	int nTries; bool bMaxAttempts = false;

	if (TRACE) { fout << "Sender started on host " << sending_hostname << endl; }

	/* Open file stream in read-only binary mode */
	FILE * stream = fopen(filename, "r+b");

	if (stream != NULL)
	{
		bytes_counter = GetFileSize(filename);
		while (1) // Send packets
		{
			if (bytes_counter > MAX_FRAME_SIZE)
			{
				frame.header = (bFirstPacket ? INITIAL_DATA : DATA);
				byte_count = MAX_FRAME_SIZE;
			}
			else // Last packet; there are at most MAX_FRAME_SIZE bytes left to send
			{
				byte_count = bytes_counter;			// last remaining bytes
				bFinished = true;
				frame.header = FINAL_DATA;
			}

			bytes_counter -= MAX_FRAME_SIZE; // Decrease byte counter

			// Read bytes into frame buffer
			bytes_read = fread(frame.buffer, sizeof(char), byte_count, stream);
			bytes_read_total += bytes_read;
			frame.buffer_length = byte_count;
			frame.snwseq = sequence_number;	// Set the frame SEQUENCE bit

			// Send frame
			nTries = 0; // only used for last frame, i.e., if no ACK comes back (and client may be gone)
			do {
				nTries++;
				if (SendFrame(sock, &frame) != sizeof(frame))
					return false;
				packetsSent++;
				if (nTries == 1)
					packetsSentNeeded++; // only increment if we've sent the packet ONCE
				bytes_sent += sizeof(frame); // Keep counter

				cout << "Sender: sent frame sequence number " << sequence_number << endl;
				if (TRACE) { fout << "Sender: sequence number " << sequence_number << endl; }

				if (bFinished && (nTries > MAX_RETRIES))
				{
					bMaxAttempts = true;
					break;
				}

			} while (ReceiveFileAck(sock, &ack) != INCOMING_PACKET || ack.number != sequence_number);

			if (bMaxAttempts) // Max attempts to receive an ACK for final packet
			{
				cout << "Sender: did not receive ACK " << sequence_number << " after " << MAX_RETRIES << " tries. Transfer finished." << endl;
				if (TRACE) { fout << "Sender: did not receive ACK " << sequence_number << " after " << MAX_RETRIES << " tries. Transfer finished." << endl; }
			}
			else
			{
				cout << "Sender: received ACK " << ack.number << endl;
				if (TRACE) { fout << "Sender: received ACK " << ack.number << endl; }
			}

			bFirstPacket = false;

			/* Invert sequence number for next dispatch */
			sequence_number = (sequence_number == 0 ? 1 : 0);

			/* Exit if last frame has been sent */
			if (bFinished)
				break;
		}

		fclose(stream);
		cout << "Sender: File is transferred Successfully." << endl;
		cout << "Sender: Total number of packets transmitted: " << packetsSent << endl;
		cout << "Sender: Total number of packets to be transmitted (needed): " << packetsSentNeeded << endl;
		cout << "Sender: Total number of bytes transmitted: " << bytes_sent << endl;
		cout << "Sender: Total number of bytes read: " << bytes_read_total << endl << endl;
		if (TRACE) {
			fout << "Sender: File is transferred Successfully." << endl;
			fout << "Sender: Total number of packets transmitted: " << packetsSent << endl;
			fout << "Sender: Total number of packets to be transmitted (needed): " << packetsSentNeeded << endl;
			fout << "Sender: Total number of bytes transmitted: " << bytes_sent << endl;
			fout << "Sender: Total number of bytes read: " << bytes_read_total << endl << endl;
		}
		return true;
	}
	else
	{
		cout << "Sender: problem opening the file." << endl;
		if (TRACE) { fout << "Sender: problem opening the file." << endl; }
		return false;
	}
}

int UDPClient::SendRequest(int sock, ThreeWayHandshake * ptr_handshake, struct sockaddr_in * sa_in)
{
	return sendto(sock, (const char *)ptr_handshake, sizeof(*ptr_handshake), 0, (struct sockaddr *)sa_in, sizeof(*sa_in));
}

int UDPClient::SendFrame(int sock, MessageFrame * ptr_message_frame)
{
	return sendto(sock, (const char*)ptr_message_frame, sizeof(*ptr_message_frame), 0, (struct sockaddr*)&sa_in, sizeof(sa_in));
}

int UDPClient::SendFileAck(int sock, Acknowledgement * ack)
{
	return sendto(sock, (const char*)ack, sizeof(*ack), 0, (struct sockaddr*)&sa_in, sizeof(sa_in));
}

/* RECEIVING FUNCTIONS */
bool UDPClient::ReceiveFile(int sock, char * filename, char * receiving_hostname, int client_number)
{
	MessageFrame frame;
	Acknowledgement ack; ack.packet_type = FRAME_ACK;
	long byte_count = 0;
	int packetsSent = 0, packetsSentNeeded = 0;
	int bytes_received = 0, bytes_written = 0, bytes_written_total = 0;
	int sequence_number = client_number % 2; // Initial bit-sequence

	if (TRACE) { fout << "Receiver started on host " << receiving_hostname << endl; }

	/* Open file stream in writable binary mode */
	FILE * stream = fopen(filename, "w+b");

	if (stream != NULL)
	{
		while (1) // Receive packets
		{
			/* Block until a packet comes in */
			while (ReceiveFrame(sock, &frame) != INCOMING_PACKET) { ; }

			bytes_received += sizeof(frame); // Keep counter of total bytes received

			if (frame.packet_type == HANDSHAKE) // send last handshake again, server didnt receive properly
			{
				cout << "Receiver: received handshake Client " << handshake.client_number << " Server " << handshake.server_number << endl;
				if (TRACE) { fout << "Receiver: received handshake Client " << handshake.client_number << " Server " << handshake.server_number << endl; }
				if (SendRequest(sock, &handshake, &sa_in) != sizeof(handshake))
					printError("Error in sending packet.");
				cout << "Receiver: sent handshake Client " << handshake.client_number << " Server" << handshake.server_number << endl;
				if (TRACE) { fout << "Receiver: sent handshake Client " << handshake.client_number << " Server" << handshake.server_number << endl; }
			}
			else if (frame.packet_type == FRAME)
			{
				cout << "Receiver: received frame " << (int)frame.snwseq << endl;
				if (TRACE) { fout << "Receiver: received frame " << (int)frame.snwseq << endl; }

				if ((int)frame.snwseq != sequence_number) // Ignore this frame; already processed; but send ACK again
				{
					ack.number = (int)frame.snwseq;
					if (SendFileAck(sock, &ack) != sizeof(ack))
						return false;
					cout << "Receiver: sent ACK " << ack.number << " again" << endl;
					packetsSent++;
					if (TRACE) { fout << "Receiver: sent ACK " << ack.number << " again" << endl; }
				}
				else // New frame to process
				{
					/* Send ACK to server */
					ack.number = (int)frame.snwseq;
					if (SendFileAck(sock, &ack) != sizeof(ack))
						return false;
					cout << "Receiver: sent ACK " << ack.number << endl;
					if (TRACE) { fout << "Receiver: sent ACK " << ack.number << endl; }
					packetsSent++;
					packetsSentNeeded++;

					/* Write frame buffer to file stream */
					byte_count = frame.buffer_length;
					bytes_written = fwrite(frame.buffer, sizeof(char), byte_count, stream);
					bytes_written_total += bytes_written;

					/* Invert sequence number for verification in next reception */
					sequence_number = (sequence_number == 0 ? 1 : 0);

					/* Exit loop if last frame */
					if (frame.header == FINAL_DATA)
						break;
				}
			}
		}

		fclose(stream);
		cout << "Receiver: File Transferred Successfully" << endl;
		cout << "Receiver: Total number of packets transmitted: " << packetsSent << endl;
		cout << "Receiver: Total number of packets to be transmitted (needed): " << packetsSentNeeded << endl;
		cout << "Receiver: Total number of bytes received: " << bytes_received << endl;
		cout << "Receiver: Total number of bytes written: " << bytes_written_total << endl << endl;

		if (TRACE) {
			fout << "Receiver: File Transferred Successfully" << endl;
			fout << "Receiver: Total number of packets transmitted: " << packetsSent << endl;
			fout << "Receiver: Total number of packets to be transmitted (needed): " << packetsSentNeeded << endl;
			fout << "Receiver: Total number of bytes received: " << bytes_received << endl;
			fout << "Receiver: Total number of bytes written: " << bytes_written_total << endl << endl;
		}
		return true;
	}
	else
	{
		cout << "Receiver: problem opening the file." << endl;
		if (TRACE) { fout << "Receiver: problem opening the file." << endl; }
		return false;
	}
}

RecieveResult UDPClient::ReceiveResponse(int sock, ThreeWayHandshake * ptr_handshake)
{
	fd_set readfds;			// fd_set is a type
	FD_ZERO(&readfds);		// initialize
	FD_SET(sock, &readfds);	// put the socket in the set
	int bytes_recvd;
	int outfds = select(1, &readfds, NULL, NULL, &timeouts);
	switch (outfds)
	{
	case 0:
		return TIMEOUT; break;
	case 1:
		bytes_recvd = recvfrom(sock, (char *)ptr_handshake, sizeof(*ptr_handshake), 0, (struct sockaddr*)&sa_in, &sa_in_size);
		return INCOMING_PACKET; break;
	default:
		return RECIEVE_ERROR; break;
	}
}

RecieveResult UDPClient::ReceiveFrame(int sock, MessageFrame * ptr_message_frame)
{
	fd_set readfds;			// fd_set is a type
	FD_ZERO(&readfds);		// initialize
	FD_SET(sock, &readfds);	// put the socket in the set	
	int bytes_recvd;
	int outfds = select(1, &readfds, NULL, NULL, &timeouts);
	switch (outfds)
	{
	case 0:
		return TIMEOUT; break;
	case 1:
		bytes_recvd = recvfrom(sock, (char *)ptr_message_frame, sizeof(*ptr_message_frame), 0, (struct sockaddr*)&sa_in, &sa_in_size);
		return INCOMING_PACKET; break;
	default:
		return RECIEVE_ERROR; break;
	}
}

RecieveResult UDPClient::ReceiveFileAck(int sock, Acknowledgement * ack)
{
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds); // put the socket in the set
	int bytes_recvd;
	int outfds = select(1, &readfds, NULL, NULL, &timeouts);
	switch (outfds)
	{
	case 0:
		return TIMEOUT; break;
	case 1:
		bytes_recvd = recvfrom(sock, (char *)ack, sizeof(*ack), 0, (struct sockaddr*)&sa_in, &sa_in_size);
		return INCOMING_PACKET; break;
	default:
		return RECIEVE_ERROR; break;
	}
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

void UDPClient::run()
{
	char server[INPÜT_LENGTH]; char filename[INPÜT_LENGTH]; char direction[INPÜT_LENGTH];
	char hostname[HOSTNAME_LENGTH]; char username[USERNAME_LENGTH]; char remotehost[HOSTNAME_LENGTH];
	unsigned long filename_length = (unsigned long)FILENAME_LENGTH;
	bool bContinue = true;

	/* Initialize winsocket */
	if (WSAStartup(0x0202, &wsadata) != 0)
	{
		WSACleanup();
		printError("Error in starting WSAStartup()\n");
	}

	/* Get username of client */
	if (!GetUserName((TCHAR*)username, &filename_length))
		printError("Cannot get the user name");

	/* Get hostname of client */
	if (gethostname(hostname, (int)HOSTNAME_LENGTH) != 0)
		printError("Cannot get the host name");

	/* Display program header */
	printf("Client Initiated at machine [%s]\n\n", hostname);

	cout << "Enter server name : "; cin >> server;	// prompt user for server name
	// loop until username inputs 'quit' as servername
	while ((strcmp(server, "Quit") != 0))
	{

		/* Create a datagram/UDP socket */
		if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
			printError("socket() failed");

		/* Client network information */
		memset(&sa, 0, sizeof(sa)); // zero out structure
		sa.sin_family = AF_INET; // Internet address family
		sa.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
		sa.sin_port = htons(CLIENT_PORT);

		/* Bind client socket to port 5000 */
		if (bind(sock, (LPSOCKADDR)&sa, sizeof(sa)) < 0)
			printError("Socket binding error");

		srand((unsigned)time(NULL));
		random = rand() % MAX_RANDOM; // [0..255]

		/* User input*/
		cout << "Enter router host : "; cin >> remotehost;
		cout << "Enter file name   : "; cin >> filename;
		cout << "Enter direction   : \n";
		cout << "'get' to request a file from Server.\n";
		cout << "'put' to put a file on Server.\n";
		cout << "Enter your choice : ";
		cin >> direction; cout << endl;

		/* Copy user-input into handshake */
		strcpy(handshake.hostname, hostname);
		strcpy(handshake.username, username);
		strcpy(handshake.filename, filename);

		if (bContinue)
		{
			/* Router network information */
			struct hostent *rp; // structure containing router
			rp = gethostbyname(remotehost);
			memset(&sa_in, 0, sizeof(sa_in));
			memcpy(&sa_in.sin_addr, rp->h_addr, rp->h_length); // fill sa_in with rp info
			sa_in.sin_family = rp->h_addrtype;
			sa_in.sin_port = htons(REMOTE_PORT);
			sa_in_size = sizeof(sa_in);

			handshake.client_number = random;
			handshake.type = CLIENT_REQ;
			handshake.packet_type = HANDSHAKE;

			/* File transfer direction */
			if (strcmp(direction, "get") == 0)
				handshake.direction = GET;
			else if (strcmp(direction, "put") == 0)
			{
				if (!FileExists(handshake.filename))
					printError("File does not exist on client side.");
				else
					handshake.direction = PUT;
			}
			else
				printError("Invalid direction. Use \"get\" or \"put\".");

			/* Initiate handshaking protocol */
			do
			{
				if (SendRequest(sock, &handshake, &sa_in) != sizeof(handshake))
					printError("Error in sending packet.");

				cout << "Client: sent handshake C" << handshake.client_number << endl;
				if (TRACE) { fout << "Client: sent handshake C" << handshake.client_number << endl; }

			} while (ReceiveResponse(sock, &handshake) != INCOMING_PACKET);

			/* Check how the server responds */
			if (handshake.type == FILE_NOT_EXIST)
			{
				cout << "File does not exist!" << endl;
				if (TRACE) { fout << "Client: requested file does not exist!" << endl; }
			}
			else if (handshake.type == INVALID)
			{
				cout << "Invalid request." << endl;
				if (TRACE) { fout << "Client: invalid request." << endl; }
			}

			/* Continue with file transfer only if the server has acknowledged the request */
			if (handshake.type == ACK_CLIENT_NUM) // server agrees to transfer
			{
				cout << "Client: received handshake C" << handshake.client_number << " S" << handshake.server_number << endl;
				if (TRACE) { fout << "Client: received handshake C" << handshake.client_number << " S" << handshake.server_number << endl; }

				// Third shake. Acknowledge server's number by sending back the handshake
				handshake.type = ACK_SERVER_NUM;
				int sequence_number = handshake.server_number % 2;
				if (SendRequest(sock, &handshake, &sa_in) != sizeof(handshake))
					printError("Error in sending packet.");

				cout << "Client: sent handshake C" << handshake.client_number << " S" << handshake.server_number << endl;
				if (TRACE) { fout << "Client: sent handshake C" << handshake.client_number << " S" << handshake.server_number << endl; }

				switch (handshake.direction)
				{
				case GET: // Client is receiving host, server will send client seq
					if (!ReceiveFile(sock, handshake.filename, hostname, handshake.client_number))
						printError("An error occurred while receiving the file.");
					break;
				case PUT: // Client is sending host, server expects seq
					if (!SendFile(sock, handshake.filename, hostname, handshake.server_number))
						printError("An error occurred while sending the file.");
					break;
				case DEL:

				case LIST:
				default:
					break;
				}
			}
		}
		cout << "Closing client socket." << endl;
		if (TRACE) { fout << "Closing client socket." << endl; }
		closesocket(sock);
		cout << "Type Quit to Exit: ";
		cin >> server;
		//cout << "Enter server name : "; cin >> server;		// prompt user for server name
		//cout << "Press Enter to exit..." << endl; getchar();
	}

}

unsigned long UDPClient::ResolveName(char name[])
{
	struct hostent *host; // Structure containing host information
	if ((host = gethostbyname(name)) == NULL)
		printError("gethostbyname() failed");
	return *((unsigned long *)host->h_addr_list[0]); // return the binary, network byte ordered address
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