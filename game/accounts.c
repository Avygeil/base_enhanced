#include "accounts.h"


int getFreeSlotIndex(){
	int i;

	for(i=0;i<authorizedCount;++i){
		if (authorized[i].status != STATUS_WAITING &&
			authorized[i].expire <= level.time ){
			return i; //this slot expired, return it for use
		}
	}

	//no expired slot to use, check if we should reset the list
	//due too many authorizations in queue
	if (authorizedCount >= MAX_LOCAL_AUTHORIZED_COUNT){
		authorizedCount = 1; //reset list
		return 0;
	}

	//return new slot
	return authorizedCount++;
}

void readHeartbeatAnswer(){
	if (strncmp(php_answers[HANDLE_HEARTBEAT],"OK",2)){
		G_DBLog("Answer to heartbeat failed, sending another heartbeat within next 30 seconds.\n");
		G_DBLog("Fail reason: %s.\n",php_answers[HANDLE_HEARTBEAT]);

		if (nextHeartBeatTime > level.time + 30*1000){
			nextHeartBeatTime = level.time + 30*1000;
		}
	}
}

static char lastUsernameSent[MAX_USERNAME_SIZE];
static char lastPasswordSent[MAX_PASSWORD_SIZE];
void readAuthAnswer(){
	int freeSlot;

	if (!strncmp(php_answers[HANDLE_AUTH],"OK",2)){
		char username[MAX_USERNAME_SIZE];

		//extract username, so we know who did we authorize now
		strcpy(username,&php_answers[HANDLE_AUTH][4]);

		if (strncmp(username,lastUsernameSent,MAX_USERNAME_SIZE)){
			G_DBLog("DB_ERROR: authorized username DOES NOT match lastUsernameSent. %s %s\n",
				username,lastUsernameSent);
			return;
		}

		//authorize client
		freeSlot = getFreeSlotIndex();
		strncpy(authorized[freeSlot].username,lastUsernameSent,sizeof(authorized[freeSlot].username));
		strncpy(authorized[freeSlot].password,lastPasswordSent,sizeof(authorized[freeSlot].password));
		authorized[freeSlot].status = STATUS_AUTHORIZED;
		authorized[freeSlot].expire = level.time + 60*1000; //one minute gap allowed to actually login
		                                                    //without sending another authorization
	} else if (!strncmp(php_answers[HANDLE_AUTH],"DENIED",6)){
		char rest[MAX_USERNAME_SIZE];
		char reason[MAX_USERNAME_SIZE];
		int i, len;
		//extract reason
		strcpy(rest,&php_answers[HANDLE_AUTH][8]);

		//find space ' ' to see where username ends and reason for denial starts
		i = 0; len = strlen(rest);
		while (rest[i]!=' ' && i < len)
			++i;

		if (i == len){
			G_DBLog("DB_ERROR: parsing denied string failed. %s\n",rest);
			return;
		}

		rest[i] = '\0';
		strcpy(reason,&rest[i+1]);

		//rest now points to username
		//reason now contains reason
		if (strncmp(rest,lastUsernameSent,sizeof(lastUsernameSent))){
			G_DBLog("DB_ERROR: denied username DOES NOT match lastUsernameSent. %s %s\n",
				rest,lastUsernameSent);
			return;
		}

		freeSlot = getFreeSlotIndex();
		strncpy(authorized[freeSlot].username,lastUsernameSent,sizeof(authorized[freeSlot].username));
		strncpy(authorized[freeSlot].password,lastPasswordSent,sizeof(authorized[freeSlot].password));
		strncpy(authorized[freeSlot].reason,reason,MAX_REASON_SIZE);
		authorized[freeSlot].status = STATUS_DENIED;
		authorized[freeSlot].expire = level.time + 30*1000; //30 seconds
	} else {
        G_DBLog("DB_ERROR: unknown AUTH response from database: %s\n",php_answers[HANDLE_AUTH]);
	}

	//"clean" last sent username/password buffers
	lastUsernameSent[0] = '\0';
	lastPasswordSent[0] = '\0';
}

size_t readAnswer( void *ptr, size_t size, size_t nmemb, void *userdata){
	int actualSize;
	//char* target;

	//target = (char *)userdata;

	actualSize = (size*nmemb < MAX_ANSWER_SIZE) ? size*nmemb : MAX_ANSWER_SIZE;

	//target chosing seem to be bugged, let decide this from php answer
	if (!strncmp((char *)ptr,"auth ",5)){
		strncpy(php_answers[HANDLE_AUTH], (char *)ptr+5, actualSize-5);
		php_answers[HANDLE_AUTH][actualSize-5] = '\0';
	} else if (!strncmp((char *)ptr,"heartbeat ",10)){
		strncpy(php_answers[HANDLE_HEARTBEAT], (char *)ptr+10, actualSize-10);
		php_answers[HANDLE_HEARTBEAT][actualSize-10] = '\0';
	} else {
		G_DBLog("DB_ERROR: unknown answer '%s'.\n",(char *)ptr);
	}

	return actualSize;
}

int debugCallback(CURL* handle, curl_infotype infotype, char* data, size_t size, void *config){
	static char buffer[4096];

	if (!size)
		return 0;

	strncpy(buffer, data, size);

	while(size >0 && buffer[size-1]=='\n')
		--size;

	buffer[size] = '\0';

	switch(infotype){
	case CURLINFO_TEXT:
		G_DBLog("DEBUG (TEXT): %s\n",buffer);
		break;
	case CURLINFO_HEADER_IN:
		G_DBLog("DEBUG (HEADER IN): %s\n",buffer);
		break;
	case CURLINFO_HEADER_OUT:
		G_DBLog("DEBUG (HEADER OUT): %s\n",buffer);
		break;
	case CURLINFO_DATA_IN:
		G_DBLog("DEBUG (DATA IN): %s\n",buffer);
		break;
	case CURLINFO_DATA_OUT:
		G_DBLog("DEBUG (DATA OUT): %s\n",buffer);
		break;

	default:
		break;
	}

	//if (buffer[size-1] == '\n')
	//	G_DBLog("  %s",buffer);
	//else
	//	G_DBLog("  %s\n",buffer);

	return 0;
}

void initDB(){
	int i;

	isDBLoaded = g_accounts.integer ? qtrue : qfalse;

    if (!isDBLoaded){
		return;
	}

	for(i = 0; i < HANDLE_COUNT;++i){
		php_handles[i] = curl_easy_init();

		if (!php_handles[i])
			G_DBLog("DB_ERROR: curl_easy_init() failed.\n");
	
		curl_easy_setopt(php_handles[i], CURLOPT_URL, db_url.string);
		curl_easy_setopt(php_handles[i], CURLOPT_WRITEDATA, (char *)php_answers[i]); 
		curl_easy_setopt(php_handles[i], CURLOPT_WRITEFUNCTION, readAnswer);

		curl_easy_setopt(php_handles[i], CURLOPT_USE_SSL, CURLUSESSL_NONE);

		//verbose log
		if (db_debug.integer && level.DBLogFile){
			curl_easy_setopt(php_handles[i], CURLOPT_VERBOSE, 1);
			curl_easy_setopt(php_handles[i], CURLOPT_DEBUGFUNCTION , debugCallback);
		}	

		isHandleBusy[i] = qfalse;
	}

	multi_handle = curl_multi_init();

	if (!multi_handle)
		G_DBLog("DB_ERROR: curl_multi_init() failed.\n");

	authorizedCount = 0;
	queueBegin = queueEnd = 0;
	nextHeartBeatTime = level.time + 5000; //send heart beat in few seconds after start
	                                       //when things are settled
	isNextHeartBeatForced = qtrue; //force next heartbeat to be sent even with no 
	                                    //players on server
}

void cleanDB(){
	int i;

    if (!isDBLoaded){
		return;
	}

	curl_multi_cleanup(multi_handle);

	for(i = 0; i < HANDLE_COUNT;++i){
		curl_easy_cleanup(php_handles[i]);
	}

}

void hash(const char* string, char* output){
    static SHA1Context sha;
	char* buffer;
	int i;

	//passwords hashed with sha1 
	SHA1Reset(&sha);
	SHA1Input(&sha,(const unsigned char*)string, strlen(string));

    if (!SHA1Result(&sha))  {
		G_DBLog("DB_ERROR: hash failed.\n");
		return;
    }
		
	buffer = output;

    for(i = 0; i < 5 ; i++)  {
        Com_sprintf(buffer, 8+1,"%08x", sha.Message_Digest[i]);
		buffer += 8;
    }

}

static qboolean isReconnecting(const char* username, unsigned ip){
	int numSorted;
	gclient_t	*cl;
	int j;

	numSorted = level.numConnectedClients;
	for(j=0;j<numSorted;++j){
		cl = &level.clients[level.sortedClients[j]];

		if (!cl || (g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT))
			continue;

		if ((cl->ps.ping >= 999 || cl->ps.ping==-1) 
			&& cl->pers.connected == CON_CONNECTED
			&& cl->sess.ip == ip
			&& !strcmp(username,cl->sess.username)	){

			trap_DropClient(cl->ps.clientNum,"Reconnected");
			return qtrue;
		}
	}

	return qfalse;
}

int getAuthorizationStatus(const char* username, const char* password, unsigned ip, char** reason){
	int i;

	*reason = 0;

    if (isReconnecting(username, ip))
		return STATUS_AUTHORIZED;

	for(i=0;i<authorizedCount;++i){
		if (!strcmp(authorized[i].username,username)){
			if (authorized[i].status == STATUS_DENIED){
				//if we are denied, check if we are denied with
				//this password
				if (strcmp(authorized[i].password,password)){
					//we werent denied with this password
					continue;
				}

				//denial time expired
				if (authorized[i].expire <= level.time)
					continue;
					
				*reason = authorized[i].reason;

			} else if (authorized[i].status == STATUS_AUTHORIZED) {

				if (authorized[i].reconnectIp != ip /*possible quick reconnect*/
					&& strcmp(authorized[i].password,password)){
					//we werent authorized for this passowrd
					continue;
				}

				//authorization expired
				if (authorized[i].expire <= level.time)
					continue;

				//ok this user will be allowed to join
				//but we make his authorization expire right now
				//so he cant repeat it within minute (exploit)
				authorized[i].expire = level.time;
			}

			return authorized[i].status;
		}
	}

	return STATUS_WAITING;
}

static int incrementCircular(int index){
	return (index < MAX_QUEUE_SIZE - 1) ? (index+1) : 0;
}

int queueSize(){
	if (queueEnd >= queueBegin)
		return queueEnd - queueBegin;

	return MAX_QUEUE_SIZE - queueBegin + queueEnd;
}

//queue
void enqueueAuthorization(const char* username, const char* password){
	int i;

	//we first check if this username isnt in queue already
	i = queueBegin;
	while(i < queueEnd){
		if (!strcmp(authorizationQueue[i].username,username))
			return;

		i = incrementCircular(i);
	}

	//we also check if this isnt current query
	if (isHandleBusy[HANDLE_AUTH] &&
		!strcmp(lastUsernameSent,username) &&
		!strcmp(lastPasswordSent,password))
		return;

	//not present in queue, add
	strncpy(authorizationQueue[queueEnd].username,username,MAX_USERNAME_SIZE);
	strncpy(authorizationQueue[queueEnd].password,password,MAX_PASSWORD_SIZE);

	//move end index after enquing
	queueEnd = incrementCircular(queueEnd);

	//did we reach begin index with end index?
	//if so, move begin index by one
	if (queueEnd == queueBegin)
		queueBegin = incrementCircular(queueBegin);

}

void dequeueAuthorization(char* username, char* password){
	if (queueSize() == 0)
		return;

	strncpy(username,authorizationQueue[queueBegin].username,MAX_USERNAME_SIZE);
	strncpy(password,authorizationQueue[queueBegin].password,MAX_PASSWORD_SIZE);

	queueBegin = incrementCircular(queueBegin);
}

#define MAX_QUERY_SIZE 2048 
static char query[MAX_QUERY_SIZE];
void sendAuthorizations(){
	char hashBuffer[HASH_LENGTH+1]; 

	//another autorization has already been sent
	if (isHandleBusy[HANDLE_AUTH])
		return;

	//no authorization in queue
	if (queueSize() == 0)
		return;

	//ok we have some authorization pending in queue
	//process one now

	dequeueAuthorization(lastUsernameSent,lastPasswordSent);
	hash(lastPasswordSent, hashBuffer);

	strncpy(query,
		va("serverid=%s&action=player_auth&username=%s&password=%s",
		   db_serverid.string,lastUsernameSent,hashBuffer),
		sizeof(query));

	G_DBLog("Sent authorization for %s.\n",lastUsernameSent);

	//G_DBLog("Buffer stats MAX_QUERY_SIZE: %i\n",strlen(query));

	curl_easy_setopt(php_handles[HANDLE_AUTH], CURLOPT_POSTFIELDS, query);
	curl_multi_add_handle(multi_handle, php_handles[HANDLE_AUTH]);

	isHandleBusy[HANDLE_AUTH] = qtrue;
}

void sendMatchResult(const char* resultquery){
	//char hashBuffer[HASH_LENGTH+1]; 

	//too bad
	if (isHandleBusy[HANDLE_MATCH_REPORT])
		return;

	strncpy(query,
		va("serverid=%s&action=match_report&%s",
		   db_serverid.string,resultquery),
		sizeof(query));

	G_DBLog("Sent match result %s.\n",resultquery);

	//G_DBLog("Buffer stats MAX_QUERY_SIZE: %i\n",strlen(query));

	curl_easy_setopt(php_handles[HANDLE_MATCH_REPORT], CURLOPT_POSTFIELDS, query);
	curl_multi_add_handle(multi_handle, php_handles[HANDLE_MATCH_REPORT]);

	isHandleBusy[HANDLE_MATCH_REPORT] = qtrue;
}

void readAnswers(){
	CURLMsg *msg;
	static int msgs_left;

	//lets see whats new
	while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {

		if (msg->msg == CURLMSG_DONE) {
			int idx, found = 0;
 
			/* Find out which handle this message is about */ 
			for (idx=0; idx<HANDLE_COUNT; idx++) {
				found = (msg->easy_handle == php_handles[idx]);
				if(found)
					break;
			}

			if (idx == HANDLE_COUNT){
				//this shouldnt happen
				continue;
			}

			//this handle should be now free
			isHandleBusy[idx] = qfalse;
 
			switch (idx) {
				case HANDLE_AUTH:
					curl_multi_remove_handle(multi_handle, php_handles[HANDLE_AUTH]);
					readAuthAnswer();

				break;
				case HANDLE_HEARTBEAT:
					curl_multi_remove_handle(multi_handle, php_handles[HANDLE_HEARTBEAT]);
					readHeartbeatAnswer();

				break;
			}
		}
	}
}

void processHeartBeats(){
	int i, j, numSorted, count;
	gclient_t	*cl;
	
	if (nextHeartBeatTime > level.time)
		return;

	//send stuff
	if (isHandleBusy[HANDLE_HEARTBEAT]){
		return;
	}

	strncpy(query,
		va("serverid=%s&action=heartbeat&minutes=5&users=",db_serverid.string),
		sizeof(query));

	//append users names to the query
	i = strlen(query);
	count = 0;
	numSorted = level.numConnectedClients;
	for(j=0;j<numSorted;++j){
		cl = &level.clients[level.sortedClients[j]];

		if (!cl || (g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT))
			continue;

		if (count > 0){
			query[i++] = 0x1E; //record separator
		}
		if (i>= sizeof(query)) G_DBLog("DB_ERROR: heartbeat user string too long.\n");

		strncpy(&query[i], cl->sess.username, MAX_USERNAME_SIZE);
		//G_LogPrintf("DEBUG DEST: [%s] i:%i znak_pred:%i procento:%i\n",&query[i],i,(int)query[i-1],(int)'%');
		++count;
		i += strlen(cl->sess.username);
		if (i>= sizeof(query)) G_DBLog("DB_ERROR: heartbeat user string too long.\n");

		query[i++] = 0x1E; //record separator

		strncpy(&query[i], cl->pers.netname, sizeof(cl->pers.netname));
		//G_LogPrintf("DEBUG DEST: [%s] i:%i znak_pred:%i procento:%i\n",&query[i],i,(int)query[i-1],(int)'%');
		++count;
		i += strlen(cl->pers.netname);
		if (i>= sizeof(query)) G_DBLog("DB_ERROR: heartbeat user string too long.\n");
	}
	query[i] = '\0';

	
	//G_DBLog("Buffer stats MAX_QUERY_SIZE: %i\n",strlen(query));

	//there are players on the server OR someone just disconnected =>
	//send heart beat with up to date information
	if (count || isNextHeartBeatForced){
		curl_easy_setopt(php_handles[HANDLE_HEARTBEAT], CURLOPT_POSTFIELDS, query);
		curl_multi_add_handle(multi_handle, php_handles[HANDLE_HEARTBEAT]);

		isNextHeartBeatForced = qfalse;
		//G_DBLog("Sent DB heartbeat.\n");
		Com_Printf("Sent DB heartbeat.\n");

		isHandleBusy[HANDLE_HEARTBEAT] = qtrue;
	}	

	nextHeartBeatTime = level.time + 5*60*1000; //next heart beat in 5 minutes
}

/*
extern char* getCurrentTime();
*/

void processDatabase(){
	static int still_running;
	int j;
	CURLMcode code;

	if (!isDBLoaded)
		return;

	//Let it work until it has work to do and it is not too long
	code = curl_multi_perform(multi_handle, &still_running);
	j = 0;
	while (code == CURLM_CALL_MULTI_PERFORM && j < 10){
		code = curl_multi_perform(multi_handle, &still_running);
		++j;
	}

	if (code != CURLM_OK){
		G_DBLog("DB_ERROR: Processing database error: %s\n",
			curl_multi_strerror(code));
	}

	readAnswers(); //handle received answers
	sendAuthorizations(); //send new client authorizations if possible
	processHeartBeats();
}

