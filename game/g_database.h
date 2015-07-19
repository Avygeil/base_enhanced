#ifndef G_DATABASE_H
#define G_DATABASE_H

#include "g_local.h"

// main loading routines
void G_DbLoad();
void G_DbUnload();   

qboolean G_DbIsFiltered( const char* ip, char* reasonBuffer, int reasonBufferSize );

// whitelist stuff
qboolean G_DbAddToWhitelist( const char* ip,
    const char* mask,
    const char* notes );

qboolean G_DbRemoveFromWhitelist( const char* ip,
    const char* mask );

qboolean G_DbIsFilteredByWhitelist( int ipA, 
    int ipB,
    int ipC, 
    int ipD, 
    char* reasonBuffer, 
    int reasonBufferSize );

// blacklist stuff
void G_DbListBlacklist();

qboolean G_DbAddToBlacklist( const char* ip,
    const char* mask, 
    const char* notes,
    const char* reason,
    int hours );

qboolean G_DbRemoveFromBlacklist( const char* ip,
    const char* mask );                          

qboolean G_DbIsFilteredByBlacklist( int ipA, int ipB, int ipC, int ipD, char* reasonBuffer, int reasonBufferSize );

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


