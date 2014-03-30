#ifndef __ACCOUNTS__
#define __ACCOUNTS__

#include "g_local.h"
#include "sha1.h"

#include <curl/curl.h>

vmCvar_t	db_url; //url address of db php api
vmCvar_t	db_serverid; //serverid from db servers table
vmCvar_t	db_debug; //NYI, this should allow curl debug print
vmCvar_t	db_log; //shall we log authorizations to db.log file?

qboolean isDBLoaded;

//handle types
enum{
	HANDLE_AUTH = 0, //player authorization
	HANDLE_HEARTBEAT, //tell db about current users list
	                  //used on players join,disconnect and
					  //periodically (if server is non empty)
	HANDLE_MATCH_REPORT, //reports matches results
	HANDLE_COUNT
};

qboolean isHandleBusy[HANDLE_COUNT];

#define HASH_LENGTH 40 //sha hash length
//moved MAX_USERNAME_SIZE to g_local.h since it is also needed somewhere else
//#define MAX_USERNAME_SIZE 32 //username size
#define MAX_PASSWORD_SIZE HASH_LENGTH+1 //password buffer size

//local authorized/denied list
#define MAX_LOCAL_AUTHORIZED_COUNT 32
#define MAX_REASON_SIZE 32

enum{
	STATUS_WAITING = 0,
	STATUS_AUTHORIZED,
	STATUS_DENIED
};

typedef struct{
	char username[MAX_USERNAME_SIZE];
	char password[MAX_PASSWORD_SIZE];
	char reason[MAX_REASON_SIZE];

	int status; //STATUS_DENIED, STATUS_AUTHORIZED
	int expire; //until what level.time does this apply
	            //after then the query must be resent
	unsigned reconnectIp; //for fast reconnecting
} AuthorizationResult;

//temporary query of authorization results
AuthorizationResult authorized[MAX_LOCAL_AUTHORIZED_COUNT];
int authorizedCount;
int getFreeSlotIndex(); 

enum{
	ATTR_USERNAME = 0,
	ATTR_PASSWORD,
	ATTR_COUNT
};


#define MAX_QUEUE_SIZE 16 //how many usernames can wait for authorization

typedef struct{
	char username[MAX_USERNAME_SIZE];
	char password[MAX_PASSWORD_SIZE];
} AuthorizationRequest;

//circular queue of proccessing authorizations, problems 
//with space most likely when db fails
AuthorizationRequest authorizationQueue[MAX_QUEUE_SIZE];
int queueBegin;
int queueEnd; //index to member after last one
int queueSize();

#define MAX_ANSWER_SIZE 1000 //temporary, in real we will need less
char php_answers[HANDLE_COUNT][MAX_ANSWER_SIZE];

CURL *php_handles[HANDLE_COUNT];
CURLM *multi_handle;

//sha1 hash
//output buffer needs to have size at least 41
//xxxxxxxxXXXXXXXXxxxxxxxxXXXXXXXXxxxxxxxx\0
void hash(const char* string, char* output);

//should be called once on each new map/server start
//as it sets up all libcurl handles
void initDB();
void cleanDB();

//checks local authorized list if specific combination of
//(username, password, ip) is allowed to join
int getAuthorizationStatus(const char* username, const char* password, unsigned ip, char** reason);

//authorization queue procedures
void enqueueAuthorization(const char* username, const char* password);
void dequeueAuthorization(char* username, char* password);

int nextHeartBeatTime;
//isNextHeartBeatForced - do we send next heartbeat even
//though there is no one on the server?
//(for proper DB update after last players disconnects)
qboolean isNextHeartBeatForced; 

//match report
//expecting resultquery in post format something=something&etc=etc...
void sendMatchResult(const char* resultquery);

//procedures to deal with DB answers
size_t readAnswer( void *ptr, size_t size, size_t nmemb, void *userdata);
void readHeartbeatAnswer();
void readAuthAnswer();
void readAnswers();

//process periodic events (if they are needed)
void sendAuthorizations();
void processHeartBeats();
void processDatabase();

#endif //_WIN32


