#ifndef G_DATABASE_H
#define G_DATABASE_H

#include "g_local.h"

#define DB_FILENAME				"enhanced.db"
#define DB_SCHEMA_VERSION		18
#define DB_SCHEMA_VERSION_STR	"18"
#define DB_OPTIMIZE_INTERVAL	( 60*60*3 ) // every 3 hours
#define DB_VACUUM_INTERVAL		( 60*60*24*7 ) // every week

// common helper struct
typedef struct pagination_s {
	int numPerPage;
	int numPage;
} pagination_t;

void G_DBLoadDatabase( void *serverDbPtr );
void G_DBUnloadDatabase( void );

qboolean G_DBUpgradeDatabaseSchema( int versionFrom, void* db );

// =========== ACCOUNTS ========================================================

typedef void( *DBListSessionsCallback )( void *ctx,
	session_t* session,
	qboolean referenced );

typedef void( *DBListUnassignedSessionIDsCallback )( void *ctx,
	const int sessionId,
	const char* topAlias,
	const int playtime,
	const qboolean referenced );

qboolean G_DBGetAccountByID( const int id,
	account_t* account );

qboolean G_DBGetAccountByName( const char* name,
	account_t* account );

void G_DBCreateAccount( const char* name );

qboolean G_DBDeleteAccount( account_t* account );

qboolean G_DBGetSessionByID( const int id,
	session_t* session );

qboolean G_DBGetSessionByHash( const sessionInfoHash_t hash,
	session_t* session );

void G_DBCreateSession( const sessionInfoHash_t hash,
	const char* info );

void G_DBLinkAccountToSession( session_t* session,
	account_t* account );

void G_DBUnlinkAccountFromSession( session_t* session );

void G_DBListSessionsForAccount( account_t* account,
	pagination_t pagination,
	DBListSessionsCallback callback,
	void* ctx );

void G_DBListSessionsForInfo( const char* key,
	const char* value,
	pagination_t pagination,
	DBListSessionsCallback callback,
	void* ctx );

void G_DBSetAccountFlags( account_t* account,
	const int flags );

void G_DBSetAccountProperties(account_t *account);
void G_DBCacheAutoLinks(void);

int G_DBGetAccountPlaytime( account_t* account );

void G_DBListTopUnassignedSessionIDs( pagination_t pagination,
	DBListUnassignedSessionIDsCallback callback,
	void* ctx );

void G_DBPurgeUnassignedSessions( void );

// =========== METADATA ========================================================

void G_DBGetMetadata( const char *key,
	char *outValue,
	size_t outValueBufSize );

void G_DBSetMetadata( const char *key,
	const char *value );

// =========== NICKNAMES =======================================================

typedef struct nicknameEntry_s {
	char	name[MAX_NETNAME];
	int		duration;
} nicknameEntry_t;

void G_DBLogNickname(const int sessionId,
	const char* name,
	int duration);

void G_DBGetMostUsedNicknames(const int sessionId,
	const int numNicknames,
	nicknameEntry_t* outNicknames);

void G_DBGetTopNickname(const int sessionId,
	nicknameEntry_t* outNickname);

// =========== FASTCAPS ========================================================

typedef void(*ListRaceRecordsCallback)(void* context,
	const char* mapname,
	const raceType_t type,
	const int rank,
	const char* name,
	const raceRecord_t* record);

typedef void(*ListRaceTopCallback)(void* context,
	const char* mapname,
	const raceType_t type,
	const char* name,
	const int captureTime,
	const time_t date);

typedef void(*ListRaceLeaderboardCallback)(void* context,
	const raceType_t type,
	const int rank,
	const char* name,
	const int golds,
	const int silvers,
	const int bronzes);

typedef void(*ListRaceLatestCallback)(void* context,
	const char* mapname,
	const raceType_t type,
	const int rank,
	const char* name,
	const int captureTime,
	const time_t date);

qboolean G_DBLoadRaceRecord(const int sessionId,
	const char* mapname,
	const raceType_t type,
	raceRecord_t* outRecord);

qboolean G_DBSaveRaceRecord(const int sessionId,
	const char* mapname,
	const raceType_t type,
	const raceRecord_t* inRecord);

qboolean G_DBGetAccountPersonalBest(const int accountId,
	const char* mapname,
	const raceType_t type,
	int* outRank,
	int* outTime);

void G_DBListRaceRecords(const char* mapname,
	const raceType_t type,
	const pagination_t pagination,
	ListRaceRecordsCallback callback,
	void* context);

void G_DBListRaceTop(const raceType_t type,
	const pagination_t pagination,
	ListRaceTopCallback callback,
	void* context);

void G_DBListRaceLeaderboard(const raceType_t type,
	const pagination_t pagination,
	ListRaceLeaderboardCallback callback,
	void* context);

void G_DBListRaceLatest(const pagination_t pagination,
	ListRaceLatestCallback callback,
	void* context);

// =========== WHITELIST =======================================================

qboolean G_DBIsFilteredByWhitelist( unsigned int ip,
	char* reasonBuffer,
	int reasonBufferSize );

qboolean G_DBAddToWhitelist( unsigned int ip,
	unsigned int mask,
	const char* notes );

qboolean G_DBRemoveFromWhitelist( unsigned int ip,
	unsigned int mask );

// =========== BLACKLIST =======================================================

typedef void( *BlackListCallback )( unsigned int ip,
	unsigned int mask,
	const char* notes,
	const char* reason,
	const char* banned_since,
	const char* banned_until );

qboolean G_DBIsFilteredByBlacklist( unsigned int ip,
	char* reasonBuffer,
	int reasonBufferSize );

void G_DBListBlacklist( BlackListCallback callback );

qboolean G_DBAddToBlacklist( unsigned int ip,
	unsigned int mask,
	const char* notes,
	const char* reason,
	int hours );

qboolean G_DBRemoveFromBlacklist( unsigned int ip,
	unsigned int mask );

// =========== MAP POOLS =======================================================

typedef void( *ListPoolCallback )( void* context,
	int pool_id,
	const char* short_name,
	const char* long_name );

typedef void( *ListMapsPoolCallback )( void** context,
	const char* long_name,
	int pool_id,
	const char* mapname,
	int mapWeight );

typedef void( *MapSelectedCallback )( void *context,
	char* mapname );

typedef struct {
	int pool_id;
	char long_name[64];
} PoolInfo;

void G_DBListPools( ListPoolCallback callback,
	void* context );

void G_DBListMapsInPool( const char* short_name,
	const char* ignore,
	ListMapsPoolCallback callback,
	void** context,
	char *longNameOut,
	size_t longNameOutSize,
	int notPlayedWithinLastMinutes);

qboolean G_DBFindPool( const char* short_name,
	PoolInfo* poolInfo );

qboolean G_DBSelectMapsFromPool( const char* short_name,
	const char* ignoreMap,
	const int mapsToRandomize,
	MapSelectedCallback callback,
	void* context );

qboolean G_DBPoolCreate( const char* short_name,
	const char* long_name );

qboolean G_DBPoolDeleteAllMaps( const char* short_name );

qboolean G_DBPoolDelete( const char* short_name );

qboolean G_DBPoolMapAdd( const char* short_name,
	const char* mapname,
	int weight );

qboolean G_DBPoolMapRemove( const char* short_name,
	const char* mapname );

qboolean G_DBAddMapToTierList(int accountId, const char *mapFileName, mapTier_t tier);
qboolean G_DBRemoveMapFromTierList(int accountId, const char *mapFileName);
qboolean G_DBClearTierList(int accountId);
qboolean G_DBClearTierListTier(int accountId, mapTier_t tier);
qboolean G_DBPrintAllPlayerRatingsForSingleMap(const char *mapFileName, int printClientNum, const char *successPrologueMessage);
void G_DBTierStats(int clientNum);
qboolean G_DBPrintPlayerTierList(int accountId, int printClientNum, const char *successPrologueMessage);
qboolean G_DBTierListPlayersWhoHaveRatedMaps(int clientNum, const char *successPrologueMessage);
qboolean G_DBPrintTiersOfAllMaps(int printClientNum, qboolean useIngamePlayers, const char *successPrologueMessage);
qboolean G_DBAddMapToTierWhitelist(const char *mapFileName);
qboolean G_DBRemoveMapFromTierWhitelist(const char *mapFileName);
list_t *G_DBGetTierWhitelistedMaps(void);
qboolean G_DBTierlistMapIsWhitelisted(const char *mapName);
qboolean G_DBSelectTierlistMaps(MapSelectedCallback callback, void *context);
qboolean G_DBShouldTellPlayerToRateCurrentMap(int accountId);
int G_GetNumberOfMapsRatedByPlayer(int accountId);
qboolean G_DBPrintPlayerUnratedList(int accountId, int printClientNum, const char *successPrologueMessage);
mapTier_t G_DBGetTierOfSingleMap(const char *optionalAccountIdsStr, const char *mapFileName, qboolean requireMultipleVotes);
int GetAccountIdsStringOfIngamePlayers(char *outBuf, size_t outBufSize);
qboolean G_DBAddCurrentMapToPlayedMapsList(void);
int G_DBNumTimesPlayedSingleMap(const char *mapFileName);

// =========== TOPAIMS ========================================================

typedef void(*ListAimRecordsCallback)(void *context,
	const char *mapname,
	const char *packName,
	const int rank,
	const char *name,
	const aimRecord_t *record);

typedef void(*ListAimTopCallback)(void *context,
	const char *mapname,
	const char *packName,
	const char *name,
	const int captureTime,
	const time_t date);

typedef void(*ListAimLeaderboardCallback)(void *context,
	const char *packName,
	const int rank,
	const char *name,
	const int golds,
	const int silvers,
	const int bronzes);

typedef void(*ListAimLatestCallback)(void *context,
	const char *mapname,
	const char *packName,
	const int rank,
	const char *name,
	const int captureTime,
	const time_t date);

qboolean G_DBTopAimLoadAimRecord(const int sessionId,
	const char *mapname,
	const aimPracticePack_t *pack,
	aimRecord_t *outRecord);

qboolean G_DBTopAimSaveAimRecord(const int sessionId,
	const char *mapname,
	const aimRecord_t *inRecord,
	const aimPracticePack_t *pack);

qboolean G_DBTopAimGetAccountPersonalBest(const int accountId,
	const char *mapname,
	const aimPracticePack_t *pack,
	int *outRank,
	int *outTime);

void G_DBTopAimListAimRecords(const char *mapname,
	const char *packName,
	const pagination_t pagination,
	ListAimRecordsCallback callback,
	void *context);

void G_DBTopAimListAimTop(const char *packName,
	const pagination_t pagination,
	ListAimTopCallback callback,
	void *context);

void G_DBTopAimListAimLeaderboard(const char *packName,
	const pagination_t pagination,
	ListAimLeaderboardCallback callback,
	void *context);

void G_DBTopAimListAimLatest(const pagination_t pagination,
	ListAimLatestCallback callback,
	void *context);

qboolean G_DBTopAimSavePack(aimPracticePack_t *pack);
list_t *G_DBTopAimLoadPacks(const char *mapname);
list_t *G_DBTopAimQuickLoadPacks(const char *mapname);
qboolean G_DBTopAimDeletePack(aimPracticePack_t *pack);

typedef struct {
	node_t						node;
	int							accountId;
	ctfPosition_t				pos;
	char						*strPtr;
} cachedPlayerPugStats_t;

typedef struct {
	node_t						node;
	int							accountId;
	ctfPosition_t				pos;
	char						*strPtr;
} cachedPerMapWinrate_t;

qboolean G_DBWritePugStats(void);
qboolean G_DBPrintPositionStatsForPlayer(int accountId, ctfPosition_t pos, int printClientNum, const char *name);
void G_DBPrintTopPlayersForPosition(ctfPosition_t pos, int printClientNum);
void G_DBPrintPlayersWithStats(int printClientNum);
void G_DBPrintPerMapWinrates(int accountId, ctfPosition_t positionOptional, int printClientNum);
void G_DBPrintWinrates(int accountId, ctfPosition_t positionOptional, int printClientNum);
void G_DBInitializePugStatsCache(void);
typedef enum {
	TEAMGENERATORTYPE_FIRST = 0,
	TEAMGENERATORTYPE_MOSTPLAYED = TEAMGENERATORTYPE_FIRST,
	TEAMGENERATORTYPE_HIGHESTRATING,
	TEAMGENERATORTYPE_FAIREST,
	TEAMGENERATORTYPE_DESIREDPOS,
	TEAMGENERATORTYPE_INCLUSIVE,
	NUM_TEAMGENERATORTYPES
} teamGeneratorType_t;
void G_DBListRatingPlayers(int raterAccountId, int raterClientNum, ctfPosition_t pos);
qboolean G_DBRemovePlayerRating(int raterAccountId, int rateeAccountId, ctfPosition_t pos);
qboolean G_DBDeleteAllRatingsForPosition(int raterAccountId, ctfPosition_t pos);
qboolean G_DBSetPlayerRating(int raterAccountId, int rateeAccountId, ctfPosition_t pos, ctfPlayerTier_t tier);
void G_DBGetPlayerRatings(void);

void G_DBFixSwap_List(void);
qboolean G_DBFixSwap_Fix(int recordId, int newPos);

typedef struct {
	node_t		node;
	char		filename[MAX_QPATH];
	char		alias[MAX_QPATH];
	char		live[8];
} mapAlias_t;

const char *GenericTableStringCallback(void *rowContext, void *columnContext);

void G_DBListMapAliases(void);
qboolean G_DBSetMapAlias(const char *filename, const char *alias, qboolean setLive);
qboolean G_DBClearMapAlias(const char *filename);
qboolean G_DBSetMapAliasLive(const char *filename, char *aliasOut, size_t aliasOutSize);
qboolean G_DBGetLiveMapFilenameForAlias(const char *alias, char *result, size_t resultSize);
qboolean G_DBGetAliasForMapName(const char *filename, char *result, size_t resultSize, qboolean *isliveOut);
qboolean G_DBGetLiveMapNameForMapName(const char *filename, char *result, size_t resultSize);

#endif //G_DATABASE_H
