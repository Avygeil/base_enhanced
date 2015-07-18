#ifndef G_DATABASE_H
#define G_DATABASE_H

#include "g_local.h"

// main loading routines
void G_DbLoad();
void G_DbUnload();   

qboolean G_DbIsIpForbidden( const char* ip );

// whitelist stuff
void G_DbAddToWhitelist( const char* ip, 
    const char* mask );

void G_DbRemoveFromWhitelist( const char* ip,
    const char* mask );

qboolean G_DbIsWhiteListed( int ipA, int ipB, int ipC, int ipD );

// blacklist stuff

void G_DbAddToBlacklist( const char* ip, 
    const char* mask, 
    const char* notes );

void G_DbRemoveFromBlacklist( const char* ip,
    const char* mask );                          

qboolean G_DbIsBlackListed( int ipA, int ipB, int ipC, int ipD );

// level stuff   
void G_DbLogLevel();

typedef enum
{
    sessionEventNone,
    sessionEventConnected,
    sessionEventDisconnected,
    sessionEventName,
    sessionEventTeam,
} SessionEvent;

// session stuff
int G_DbLogSession(const char* ip);

int G_DbLogSessionEvent(int sessionId, 
    int eventId, 
    const char* eventContext);



#endif //G_DATABASE_H


