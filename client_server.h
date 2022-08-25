#include<iostream>
#include<iomanip>
#include<vector>
#include<unordered_map>
#include<set>
#include<queue>
#include<sstream>
#include<cmath>
#include<string.h>
#include<sys/poll.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/tcp.h>
#include<unistd.h>


//message types for subscriber messages
#define ID_MSG 2
#define SUB_MSG 1
#define UNSUB_MSG 0

//message types for server messages
#define INT_DATA 0
#define SHORT_REAL_DATA 1
#define FLOAT_DATA 2
#define STRING_DATA 3
//error msg
#define USED_ID 10
#define NO_ID_GIVEN 11

//max sizes
#define DATA_LENGTH 1500
#define TOPIC_LENGTH 50

//statuses
#define ONLINE 1
#define OFFLINE 0

//funcs
#define SIGN(a) (a ? "-" : "")

using namespace std;

//subscriber message for server
typedef struct {
	//length of the rest of the data(length field excluded)
	uint8_t length;
	uint8_t type;
	string id;
	string topic;
	bool SF;
} sub_message;

//server message for subscribers
typedef struct
{
	//length of the rest of the data(length field excluded)
	uint16_t length;
	uint8_t type;
	sockaddr_in sender_addr;
	char topic[51];
	char data[1500];
} server_message;

typedef struct {
	string id;
	sockaddr_in addr;
	int fd;
	//online/offline
	bool status;
} user_data;

/*reads n bytes received from TCP
returns:
->	-1 for errors
->   0 for reading less than n bytes
->	 n when n bytes have been read
*/
int read_n_bytes(int fd, char* recv_buffer, char* segment, int n);