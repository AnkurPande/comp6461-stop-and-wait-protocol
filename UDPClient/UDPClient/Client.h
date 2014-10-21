//Client Header File

#define STIMER 0  //Lower limit of timer
#define UTIMER 300000//Upper limit of timer
#define CLIENT_PORT 5000 //Client Port Number
#define REMOTE_PORT 7000 //Remote Host Port Number
#define TRACE1 //Log File
#define MAX_RETRIES 10 //Maximum number of retries to send a packet
#define INPÜT_LENGTH 40 //Maximum length of client input
#define HOSTNAME_LENGTH 40 //Maximum length of hostname entered
#define USERNAME_LENGTH 40 //Maximum length of username eneterd
#define FILENAME_LENGTH 40 //Maximum length of filename
#define MAX_FRAME_SIZE 1024 //Maximum Frame size
#define MAX_RANDOM 256 //Upper limit for random numbers
#define SEQUENCE_WIDTH 1 // bits width for sequence in stop and wait protocol
 
#include <winsock.h>
#include<fstream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")

typedef enum{GET=1,PUT,DEL,LIST}Direction; 
typedef enum{HANDSHAKE=1,FRAME,FRAME_ACK}PacketType ;
typedef enum{TIMEOUT=1,INCOMING_PACKET,RECIEVE_ERROR}RecieveResult;
typedef enum{CLIENT_REQ=1,ACK_CLIENT_NUM,ACK_SERVER_NUM,FILE_NOT_EXIST,INVALID}HandshakeType;
typedef enum{INITIAL_DATA=1,DATA,FINAL_DATA}MessageFrameHeader;

typedef struct
{
	PacketType packet_type;
	int number;
}Acknowledgement;

typedef struct{
	PacketType packet_type;
	HandshakeType handshake;
	Direction direction;
	char hostname[HOSTNAME_LENGTH];
	char filename[FILENAME_LENGTH];
	char username[USERNAME_LENGTH];
	int client_number;
	int server_number;
}ThreeWayHandshake;

typedef struct {
	PacketType packet_type;
	MessageFrameHeader header;
	unsigned int snwseq : SEQUENCE_WIDTH;
	char buffer[MAX_FRAME_SIZE];
	int buffer_length;
}MessageFrame;

class UDPClient{
	int sock;
	SOCKADDR_IN sa;			// client info, IP, port 5000
	SOCKADDR_IN sa_in;		// router info, IP, port 7000
	int sa_in_size;
	timeval timeouts;
	ThreeWayHandshake handshake;
	int random;
	WSADATA wsadata;
	
private:
	std::ofstream fout;

public:
	UDPClient(char * fn = "ClientLogFile.txt"); // constructor
	~UDPClient(); //Destructor
	
	void run();
	bool sendFile(int, char *, char *,int); //Send the file after the hanshake succeeded
	int sendRequest(int, ThreeWayHandshake *, SOCKADDR_IN *); //Start the initiation of hanshake
	int sendFrame(int, MessageFrame *); // Send the frame of data
	int sendFileAck(int, Acknowledgement *); // Send the File Acknowledgment

	bool recieveFile(int, char *, char *, int); // Recieve the File from remote host after the handshake
	RecieveResult ReceiveResponse(int, ThreeWayHandshake *); // Recieve the response after the initiation of handshake  
	RecieveResult ReceiveFrame(int, MessageFrame *); // Recieve Frame of data
	RecieveResult ReceiveFileAck(int, Acknowledgement *); // Recieve File Acknowledgment 

};