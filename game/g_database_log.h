#ifndef G_DATABASE_H
#define G_DATABASE_H

#include "g_local.h"

// main loading routines
void G_LogDbLoad();
void G_LogDbUnload();

// level stuff   
int G_LogDbLogLevelStart( qboolean isRestart );
void G_LogDbLogLevelEnd( int levelId );

void G_LogDbLogLevelEvent( int levelId,
    int levelTime,
    int eventId,
    int context1,
    int context2,
    int context3,
    int context4,
    const char* contextText );

typedef enum
{
    levelEventNone,
    levelEventTeamChanged,
} LevelEvent;

// session stuff
int G_LogDbLogSessionStart( unsigned int ipInt,
    int ipPort,
    int id);

void G_LogDbLogSessionEnd( int sessionId );

void G_LogDbLogNickname( unsigned int ipInt,
    const char* name,
    int duration );

typedef void( *ListAliasesCallback )(void* context,
    const char* name,
    int duration);

void G_CfgDbListAliases( unsigned int ipInt,
    unsigned int ipMask,
    int limit,
    ListAliasesCallback callback,
    void* context );


#endif //G_DATABASE_H


