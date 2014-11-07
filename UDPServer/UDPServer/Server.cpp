/* Server */

#pragma comment(lib,"wsock32.lib")
#include <winsock.h>
#include <iostream>
#include <fstream>
#include <windows.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <process.h>
#include <tchar.h>
#include "Server.h"

bool FileExists(char * filename)
{
	int result;
	struct _stat stat_buf;
	result = _stat(filename, &stat_buf);
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
bool UDPServer::sendFile(int sock, char * ptr_filename, char * ptr_sending_hostname, int client_number)
{
	MessageFrame frame; frame.packet_type = FRAME;
	Acknowledgement ack; ack.number = -1;
	long bytes_counter = 0, byte_count = 0;
	int bytes_sent = 0, bytes_read = 0, bytes_read_total = 0;
	int packetsSent = 0, packetsSentNeeded = 0;
	bool bFirstPacket = true, bFinished = false;
	int sequence_number = client_number % 2; // Initial bit-sequence expected by the client
	int nTries; bool bMaxAttempts = false;

	if (TRACE1) { fout << "Sender: started on host " << ptr_sending_hostname << endl; }

	/* Open file stream in read-only binary mode */
	FILE * stream = fopen(ptr_filename, "r+b");

	if (stream != NULL)
	{
		bytes_counter = GetFileSize(ptr_filename);
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
				if (sendFrame(sock, &frame) != sizeof(frame))
					return false;
				packetsSent++;
				if (nTries == 1)
					packetsSentNeeded++; // only increment if we've sent the packet ONCE
				bytes_sent += sizeof(frame); // Keep counter

				cout << "Sender: Sent Frame Sequence Number " << sequence_number << endl;
				if (TRACE1) { fout << "Sender: Frame Sequence Number " << sequence_number << endl; }

				if (bFinished && (nTries > MAX_RETRIES))
				{
					bMaxAttempts = true;
					break;
				}

			} while (receiveFileAck(sock, &ack) != INCOMING_PACKET || ack.number != sequence_number);

			if (bMaxAttempts) // Max attempts to receive an ACK for final packet
			{
				cout << "Sender: Did not receive ACKNOWLEDGEMENT " << sequence_number << " after " << MAX_RETRIES << " tries. Transfer finished." << endl;
				if (TRACE1) { fout << "Sender: Did not receive ACKNOWLEDGEMENT " << sequence_number << " after " << MAX_RETRIES << " tries. Transfer finished." << endl; }
			}
			else
			{
				cout << "Sender: received ACK " << ack.number << endl;
				if (TRACE1) { fout << "Sender: received ACK " << ack.number << endl; }
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
		if (TRACE1) {
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
		if (TRACE1) { fout << "Sender: problem opening the file." << endl; }
		return false;
	}
}

int UDPServer::sendRequest(int sock, ThreeWayHandshake * ptr_handshake, struct sockaddr_in * ptr_sa_in) // fills sa_in
{
	int bytes_sent = sendto(sock, (const char *)ptr_handshake, sizeof(*ptr_handshake), 0, (struct sockaddr *)ptr_sa_in, sizeof(*ptr_sa_in));
	return bytes_sent;
}

int UDPServer::sendFrame(int sock, MessageFrame * ptr_message_frame)
{
	int bytes_sent = sendto(sock, (const char*)ptr_message_frame, sizeof(*ptr_message_frame), 0, (struct sockaddr*)&sa_in, sizeof(sa_in));
	return bytes_sent;
}

int UDPServer::sendFileAck(int sock, Acknowledgement * ptr_ack)
{
	int bytes_sent = sendto(sock, (const char*)ptr_ack, sizeof(*ptr_ack), 0, (struct sockaddr*)&sa_in, sizeof(sa_in));
	return bytes_sent;
}

bool UDPServer::recieveFile(int sock, char * ptr_filename, char * ptr_receiving_hostname, int server_number)
{
	MessageFrame frame;
	Acknowledgement ack; ack.packet_type = FRAME_ACK;
	long byte_count = 0;
	int packetsSent = 0, packetsSentNeeded = 0;
	int bytes_received = 0, bytes_written = 0, bytes_written_total = 0;
	int sequence_number = server_number % 2; // Initial bit-sequence

	if (TRACE1) { fout << "Receiver started on host " << ptr_receiving_hostname << endl; }

	/* Open file stream in writable binary mode */
	FILE * stream = fopen(ptr_filename, "w+b");

	if (stream != NULL)
	{
		while (1) // Receive packets
		{
			/* Block until a packet comes in */
			while (receiveFrame(sock, &frame) != INCOMING_PACKET) { ; }

			bytes_received += sizeof(frame); // Keep counter of total bytes received

			if (frame.packet_type == HANDSHAKE) // server waits for last handshake if it has been lost, output
			{
				cout << "Receiver: received handshake Client: " << handshake.client_number << " Server: " << handshake.server_number << endl;
				if (TRACE1) { fout << "Receiver: received handshake Client: " << handshake.client_number << " Server: " << handshake.server_number << endl; }
			}
			else if (frame.packet_type == FRAME)
			{
				cout << "Receiver: received frame " << (int)frame.snwseq << endl;
				if (TRACE1) { fout << "Receiver: received frame " << (int)frame.snwseq << endl; }

				if ((int)frame.snwseq != sequence_number) // Ignore this frame; already processed; but send ACK again
				{
					ack.number = (int)frame.snwseq;
					if (sendFileAck(sock, &ack) != sizeof(ack))
						return false;
					cout << "Receiver: sent ACK " << ack.number << " again" << endl;
					if (TRACE1) { fout << "Receiver: sent ACK " << ack.number << " again" << endl; }
					packetsSent++;
				}
				else // New frame to process
				{
					/* Send ACK to server */
					ack.number = (int)frame.snwseq;
					if (sendFileAck(sock, &ack) != sizeof(ack))
						return false;
					cout << "Receiver: sent ack " << ack.number << endl;
					if (TRACE1) { fout << "Receiver: sent ack " << ack.number << endl; }
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

		if (TRACE1) {
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
		if (TRACE1) { fout << "Receiver: problem opening the file." << endl; }
		return false;
	}
}

/* RECEIVING FUNCTIONS */
RecieveResult UDPServer::receiveResponse(int sock, ThreeWayHandshake * ptr_handshake)
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

RecieveResult UDPServer::receiveFrame(int sock, MessageFrame * ptr_message_frame)
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

RecieveResult UDPServer::receiveFileAck(int sock, Acknowledgement * ptr_ack)
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
		bytes_recvd = recvfrom(sock, (char *)ptr_ack, sizeof(*ptr_ack), 0, (struct sockaddr*)&sa_in, &sa_in_size);
		return INCOMING_PACKET; break;
	default:
		return RECIEVE_ERROR; break;
	}
}

void UDPServer::run()
{
	char server[INPÜT_LENGTH];
	char direction[INPÜT_LENGTH];
	char filename[FILENAME_LENGTH];
	char remotehost[HOSTNAME_LENGTH];
	char username[USERNAME_LENGTH];
	char hostname[HOSTNAME_LENGTH];
	/* Initialize winsocket */
	if (WSAStartup(0x0202, &wsadata) != 0)
	{
		WSACleanup();
		printError("Error in starting WSAStartup()\n");
	}

	/* Get hostname of server */
	if (gethostname(server, HOSTNAME_LENGTH) != 0)
		printError("Server gethostname() error.");

	printf("Server Initiated at machine: [%s]\n", server);
	printf("Awaiting for File Transfer Request..");
	printf("\n\n");

	/* Create a datagram/UDP socket */
	if ((sock2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		printError("socket() failed");

	/* Fill in server network information */
	memset(&sa, 0, sizeof(sa));				// Zero out structure
	sa.sin_family = AF_INET;                // Internet address family
	sa.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
	sa.sin_port = htons(SERVER_PORT);       // local server port (5001)
	sa_in_size = sizeof(sa_in);

	/* Bind server socket to port 5001*/
	if (bind(sock2, (LPSOCKADDR)&sa, sizeof(sa)) < 0)
		printError("Socket binding error");

	/* Wait until a packet comes in, ignore if not CLIENT_REQ */
	while (receiveResponse(sock2, &handshake) != INCOMING_PACKET || handshake.handshake_type != CLIENT_REQ) { ; }

	cout << "Server: received handshake Client" << handshake.client_number << endl;
	if (TRACE1) { fout << "Server: received handshake Client" << handshake.client_number << endl; }

	if (handshake.direction == GET)
	{
		cout << "Server: user \"" << handshake.username << "\" on host \"" << handshake.hostname << "\" requests GET file: \"" << handshake.filename << "\"" << endl;
		if (TRACE1) { fout << "Server: user \"" << handshake.username << "\" on host \"" << handshake.hostname << "\" requests GET file: \"" << handshake.filename << "\"" << endl; }

		if (FileExists(handshake.filename))
			handshake.handshake_type = ACK_CLIENT_NUM; // server ACKs client's request
		else
		{
			handshake.handshake_type = FILE_NOT_EXIST;
			cout << "Server: requested file does not exist." << endl;
			if (TRACE1) { fout << "Server: requested file does not exist." << endl; }
		}
	}
	else if (handshake.direction == PUT)
	{
		cout << "Server: user \"" << handshake.username << "\" on host \"" << handshake.hostname << "\" requests PUT file: \"" << handshake.filename << "\"" << endl;
		if (TRACE1) { fout << "Server: user \"" << handshake.username << "\" on host \"" << handshake.hostname << "\" requests PUT file: \"" << handshake.filename << "\"" << endl; }
		handshake.handshake_type = ACK_CLIENT_NUM; // server ACKs client's request		
	}
	else
	{
		handshake.handshake_type = INVALID;
		cout << "Server: invalid request." << endl;
		if (TRACE1) { fout << "Server: invalid request." << endl; }
	}

	if (handshake.handshake_type != ACK_CLIENT_NUM) // just send, don't expect a reply.
	{
		if (sendRequest(sock2, &handshake, &sa_in) != sizeof(handshake))
			printError("Error in sending packet.");

		cout << "Server: sent error message to client. " << endl;
		if (TRACE1) { fout << "Server: sent error message to client. " << endl; }
	}
	else if (handshake.handshake_type == ACK_CLIENT_NUM)
	{
		srand((unsigned)time(NULL));
		random = rand() % MAX_RANDOM; // [0..255]	
		handshake.server_number = random;

		// keep sending reply until the final ack comes
		do {
			if (sendRequest(sock2, &handshake, &sa_in) != sizeof(handshake))
				printError("Error in sending packet.");

			cout << "Server: sent handshake C" << handshake.client_number << " S" << handshake.server_number << endl;
			if (TRACE1) { fout << "Server: sent handshake C" << handshake.client_number << " S" << handshake.server_number << endl; }

		} while (receiveResponse(sock2, &handshake) != INCOMING_PACKET || handshake.handshake_type != ACK_SERVER_NUM);

		cout << "Server: received handshake C" << handshake.client_number << " S" << handshake.server_number << endl;
		if (TRACE1) { fout << "Server: received handshake C" << handshake.client_number << " S" << handshake.server_number << endl; }

		if (handshake.handshake_type == ACK_SERVER_NUM)
		{
			switch (handshake.direction)
			{
			case GET:
				if (!sendFile(sock2, handshake.filename, server, handshake.client_number))
					printError("An error occurred while sending the file.");
				break;

			case PUT:
				if (!recieveFile(sock2, handshake.filename, server, handshake.server_number))
					printError("An error occurred while receiving the file.");
				break;

			default:
				break;
			}
		}
		else
		{
			cout << "Handshake error!" << endl;
			if (TRACE1) { fout << "Handshake error!" << endl; }
		}
	}
	cout << "Closing server socket." << endl;
	if (TRACE1) { fout << "Closing server socket." << endl; }
	closesocket(sock2);
	cout << "Press Enter to exit..." << endl; getchar();
}
void UDPServer::printError(TCHAR* msg) {
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


UDPServer::UDPServer(char * fn) // constructor
{
	/* For timer */
	timeouts.tv_sec = STIMER;
	timeouts.tv_usec = UTIMER;

	/* Open log file */
	fout.open(fn);
}

UDPServer::~UDPServer() // destructor
{
	/* Close log file */
	fout.close();

	/* Uninstall winsock.dll */
	WSACleanup();
}

int main(void)
{
	UDPServer * ser = new UDPServer();
	ser->run();
	return 0;
}
