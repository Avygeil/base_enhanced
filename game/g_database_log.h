#ifndef G_DATABASE_H
#define G_DATABASE_H

#include "g_local.h"

// main loading routines
void G_LogDbLoad();
void G_LogDbUnload();

// level stuff   
int G_LogDbLogLevelStart( qboolean isRestart );
void G_LogDbLogLevelEnd( int levelId );

typedef enum
{
    sessionEventNone,
    sessionEventConnected,
    sessionEventDisconnected,
    sessionEventName,
    sessionEventTeam,
} SessionEvent;

// session stuff
int G_LogDbLogSessionStart( const char* ip );
void G_LogDbLogSessionEnd( int sessionId );

int G_LogDbLogSessionEvent( int sessionId,
    int eventId, 
    const char* eventContext);   



#endif //G_DATABASE_H


