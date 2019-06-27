#ifndef G_DATABASE_H
#define G_DATABASE_H

#include "g_local.h"

// main loading routines
void G_LogDbLoad();
void G_LogDbUnload();

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

int G_LogDbLoadCaptureRecords( const char *mapname,
	CaptureRecordList *recordsToLoad );

typedef void( *ListBestCapturesCallback )( void *context,
	const char *mapname,
	const CaptureRecordType type,
	const char *recordHolderName,
	const unsigned int recordHolderIpInt,
	const char *recordHolderCuid,
	const int bestTime,
	const time_t bestTimeDate );

typedef void( *LeaderboardCapturesCallback )( void *context,
	const CaptureRecordType type,
	const unsigned int playerIpInt,
	const int golds,
	const int silvers,
	const int bronzes );

typedef void( *ListLatestCapturesCallback )( void *context,
	const char *mapname,
	const int rank,
	const CaptureRecordType type,
	const char *recordHolderName,
	const unsigned int recordHolderIpInt,
	const char *recordHolderCuid,
	const int captureTime,
	const time_t captureTimeDate );

void G_LogDbListBestCaptureRecords( CaptureRecordType type,
	int limit,
	int offset,
	ListBestCapturesCallback callback,
	void *context );

void G_LogDbGetCaptureRecordsLeaderboard( CaptureRecordType type,
	int limit,
	int offset,
	LeaderboardCapturesCallback callback,
	void *context );

void G_LogDbListLatestCaptureRecords( CaptureRecordType type,
	int limit,
	int offset,
	ListLatestCapturesCallback callback,
	void *context );

int G_LogDbSaveCaptureRecords( CaptureRecordList *recordsToSave );

int G_LogDbCaptureTime( unsigned int ipInt,
	const char *netname,
	const char *cuid,
	const int clientId,
	const char *matchId,
	const int captureTime,
	const team_t whoseFlag,
	const int maxSpeed,
	const int avgSpeed,
	const time_t date,
	const int pickupLevelTime,
	const CaptureRecordType type,
	CaptureRecordList *currentRecords );

#endif //G_DATABASE_H


