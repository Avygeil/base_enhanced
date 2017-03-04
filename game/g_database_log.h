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

void G_LogDbLogNickname(unsigned int ipInt,
	const char* name,
	int duration,
	const char* cuidHash);

typedef void( *ListAliasesCallback )(void* context,
    const char* name,
    int duration);

void G_CfgDbListAliases(unsigned int ipInt,
	unsigned int ipMask,
	int limit,
	ListAliasesCallback callback,
	void* context,
	const char* cuidHash);

void G_LogDbLoadCaptureRecords( const char *mapname,
	CaptureRecordList *recordsToLoad );

typedef void( *ListBestCapturesCallback )( void *context,
	const char *mapname,
	const CaptureRecordType type,
	const char *recordHolderName,
	unsigned int recordHolderIpInt,
	const char *recordHolderCuid,
	int bestTime );

void G_LogDbListBestCaptureRecords( CaptureRecordType type,
	int limit,
	int offset,
	ListBestCapturesCallback callback,
	void *context );

void G_LogDbSaveCaptureRecords( CaptureRecordList *recordsToSave );

int G_LogDbCaptureTime( unsigned int ipInt,
	const char *netname,
	const char *cuid,
	const int clientId,
	const char *matchId,
	const int captureTime,
	const team_t whoseFlag,
	const int pickupLevelTime,
	const CaptureRecordType type,
	CaptureRecordList *currentRecords );

#endif //G_DATABASE_H


