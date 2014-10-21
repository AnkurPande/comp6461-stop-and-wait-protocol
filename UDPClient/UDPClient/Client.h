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
 
using namespace std;

typedef enum{GET=1,PUT,DELETE,LIST}Direction; 
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
