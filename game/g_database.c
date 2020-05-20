#include "g_database.h"
#include "sqlite3.h"
#include "time.h"

#include "cJSON.h"

#include "g_database_schema.h"


static sqlite3* diskDb = NULL;
static sqlite3* dbPtr = NULL;

static void ErrorCallback( void* ctx, int code, const char* msg ) {
	Com_Printf( "SQL error (code %d): %s\n", code, msg );
}

static int TraceCallback( unsigned int type, void* ctx, void* ptr, void* info ) {
	if ( !ptr || !info ) {
		return 0;
	}

	if ( type == SQLITE_TRACE_STMT ) {
		char* sql = ( char* )info;

		Com_Printf( "Executing SQL: \n" );

		if ( !Q_stricmpn( sql, "--", 2 ) ) {
			// a comment, which means this is a trigger, log it directly
			Com_Printf( "--------------------------------------------------------------------------------\n" );
			Com_Printf( "%s\n", sql );
			Com_Printf( "--------------------------------------------------------------------------------\n" );
		} else {
			// expand the sql before logging it so we can see parameters
			sqlite3_stmt* stmt = ( sqlite3_stmt* )ptr;
			sql = sqlite3_expanded_sql( stmt );
			Com_Printf( "--------------------------------------------------------------------------------\n" );
			Com_Printf( "%s\n", sql );
			Com_Printf( "--------------------------------------------------------------------------------\n" );
			sqlite3_free( sql );
		}
	} else if ( type == SQLITE_TRACE_PROFILE ) {
		unsigned long long nanoseconds = *( ( unsigned long long* )info );
		unsigned int ms = nanoseconds / 1000000;
		Com_Printf( "Executed in %ums\n", ms );
	}

	return 0;
}

void G_DBLoadDatabase( void )
{
	if (dbPtr) {
		return;
	}

    int rc;

	// db options

	sqlite3_config( SQLITE_CONFIG_SINGLETHREAD ); // we don't need multi threading
	sqlite3_config( SQLITE_CONFIG_MEMSTATUS, 0 ); // we don't need allocation statistics
	sqlite3_config( SQLITE_CONFIG_LOG, ErrorCallback, NULL ); // error logging

	// initialize db

    rc = sqlite3_initialize();

	if ( rc != SQLITE_OK ) {
		Com_Error( ERR_DROP, "Failed to initialize SQLite3 (code: %d)\n", rc );
		return;
	}

	Com_Printf("SQLite version: %s\n", sqlite3_libversion());

    rc = sqlite3_open_v2( DB_FILENAME, &diskDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL );

	if ( rc != SQLITE_OK ) {
		Com_Error( ERR_DROP, "Failed to open database file "DB_FILENAME" (code: %d)\n", rc );
		return;
	}

	Com_Printf( "Successfully opened database file "DB_FILENAME"\n" );

	if ( g_inMemoryDB.integer ) {
		Com_Printf( "Using in-memory database\n" );

		// open db in memory
		sqlite3* memoryDb = NULL;
		rc = sqlite3_open_v2( ":memory:", &memoryDb, SQLITE_OPEN_READWRITE, NULL );

		if ( rc == SQLITE_OK ) {
			sqlite3_backup *backup = sqlite3_backup_init( memoryDb, "main", diskDb, "main" );
			if ( backup ) {
				rc = sqlite3_backup_step( backup, -1 );
				if ( rc == SQLITE_DONE ) {
					rc = sqlite3_backup_finish( backup );
					if ( rc == SQLITE_OK ) {
						dbPtr = memoryDb;
					}
				}
			}
		}

		if ( !dbPtr ) {
			Com_Printf( "ERROR: Failed to load database into memory!\n" );
		}
	}

	// use disk db by default in any case
	if ( !dbPtr ) {
		Com_Printf( "Using on-disk database\n" );
		dbPtr = diskDb;
	}

	// register trace callback if needed
	if ( g_traceSQL.integer ) {
		sqlite3_trace_v2( dbPtr, SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, TraceCallback, NULL );
	}

	// more db options
	sqlite3_exec( dbPtr, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL );

	// get version and call the upgrade routine

	char s[16];
	G_DBGetMetadata( "schema_version", s, sizeof( s ) );
	int version = VALIDSTRING( s ) ? atoi( s ) : 0;

	// setup tables if database was just created
	// NOTE: this means g_database_schema.h must always reflect the latest version
	if (!version) {
		sqlite3_exec(dbPtr, sqlCreateTables, 0, 0, 0);
	}

	if ( !G_DBUpgradeDatabaseSchema( version, dbPtr ) ) {
		// don't let server load if an upgrade failed

		if (dbPtr != diskDb) {
			// close in memory db immediately
			sqlite3_close(dbPtr);
			dbPtr = NULL;
		}

		Com_Error(ERR_DROP, "Failed to upgrade database, shutting down to avoid data corruption\n");
	}

	G_DBSetMetadata( "schema_version", DB_SCHEMA_VERSION_STR );

	// optimize the db if needed

	G_DBGetMetadata( "last_optimize", s, sizeof( s ) );

	const time_t currentTime = time( NULL );
	time_t last_optimize = VALIDSTRING( s ) ? strtoll( s, NULL, 10 ) : 0;

	if ( last_optimize + DB_OPTIMIZE_INTERVAL < currentTime ) {
		Com_Printf( "Automatically optimizing database...\n" );

		sqlite3_exec( dbPtr, "PRAGMA optimize;", NULL, NULL, NULL );

		G_DBSetMetadata( "last_optimize", va( "%lld", currentTime ) );
	}

	// if the server is empty, vacuum the db if needed

	if ( !level.numConnectedClients ) {
		G_DBGetMetadata( "last_vacuum", s, sizeof( s ) );

		time_t last_autoclean = VALIDSTRING( s ) ? strtoll( s, NULL, 10 ) : 0;

		if ( last_autoclean + DB_VACUUM_INTERVAL < currentTime ) {
			Com_Printf( "Automatically running vacuum on database...\n" );

			sqlite3_exec( dbPtr, "VACUUM;", NULL, NULL, NULL );

			G_DBSetMetadata( "last_vacuum", va( "%lld", currentTime ) );
		}
	}	
}

void G_DBUnloadDatabase( void )
{
	int rc;

	if ( dbPtr && diskDb && dbPtr != diskDb ) {
		Com_Printf( "Saving in-memory database changes to disk\n" );

		// we are using in memory db, save changes to disk
		qboolean success = qfalse;
		sqlite3_backup *backup = sqlite3_backup_init( diskDb, "main", dbPtr, "main" );
		if ( backup ) {
			rc = sqlite3_backup_step( backup, -1 );
			if ( rc == SQLITE_DONE ) {
				rc = sqlite3_backup_finish( backup );
				if ( rc == SQLITE_OK ) {
					success = qtrue;
				}
			}
		}

		if ( !success ) {
			Com_Printf( "WARNING: Failed to backup in-memory database! Changes from this session have NOT been saved!\n" );
		}

		sqlite3_close( dbPtr );
	}

	if (diskDb) {
		sqlite3_close(diskDb);
		diskDb = dbPtr = NULL;
	}
}

// =========== METADATA ========================================================

const char* const sqlGetMetadata =
"SELECT value FROM metadata WHERE metadata.key = ?1 LIMIT 1;                   ";

const char* const sqlSetMetadata =
"INSERT OR REPLACE INTO metadata (key, value) VALUES( ?1, ?2 );                ";

void G_DBGetMetadata( const char *key,
	char *outValue,
	size_t outValueBufSize )
{
	sqlite3_stmt* statement;

	outValue[0] = '\0';

	int rc = sqlite3_prepare_v2( dbPtr, sqlGetMetadata, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, key, -1, SQLITE_STATIC );

	rc = sqlite3_step( statement );
	while ( rc == SQLITE_ROW ) {
		const char *value = ( const char* )sqlite3_column_text( statement, 0 );
		Q_strncpyz( outValue, value, outValueBufSize );

		rc = sqlite3_step( statement );
	}

	sqlite3_finalize( statement );
}

void G_DBSetMetadata( const char *key,
	const char *value )
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare_v2( dbPtr, sqlSetMetadata, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, key, -1, SQLITE_STATIC );
	sqlite3_bind_text( statement, 2, value, -1, SQLITE_STATIC );

	rc = sqlite3_step( statement );

	sqlite3_finalize( statement );
}

// =========== ACCOUNTS ========================================================

static const char* sqlGetAccountByID =
"SELECT name, created_on, usergroup, flags                                   \n"
"FROM accounts                                                               \n"
"WHERE accounts.account_id = ?1;                                               ";

static const char* sqlGetAccountByName =
"SELECT account_id, name, created_on, usergroup, flags                       \n"
"FROM accounts                                                               \n"
"WHERE accounts.name = ?1;                                                     ";

static const char* sqlCreateAccount =
"INSERT INTO accounts ( name ) VALUES ( ?1 );                                  ";

static const char* sqlDeleteAccount =
"DELETE FROM accounts WHERE accounts.account_id = ?1;                          ";

static const char* sqlGetSessionByID =
"SELECT hash, info, account_id                                               \n"
"FROM sessions                                                               \n"
"WHERE sessions.session_id = ?1;                                               ";

static const char* sqlGetSessionByHash =
"SELECT session_id, info, account_id                                         \n"
"FROM sessions                                                               \n"
"WHERE sessions.hash = ?1;                                                     ";

static const char* sqlCreateSession =
"INSERT INTO sessions ( hash, info ) VALUES ( ?1, ?2 );                        ";

static const char* sqlLinkAccountToSession =
"UPDATE sessions                                                             \n"
"SET account_id = ?1                                                         \n"
"WHERE session_id = ?2;                                                        ";

static const char* sqlListSessionIdsForAccount =
"SELECT session_id, hash, info, referenced                                   \n"
"FROM sessions_info                                                          \n"
"WHERE sessions_info.account_id = ?1;                                          ";

static const char* sqlListSessionIdsForInfo =
"SELECT session_id, hash, info, account_id, referenced                       \n"
"FROM sessions_info                                                          \n"
"WHERE json_extract(info, ?1) = ?2;                                            ";

static const char* sqlSetFlagsForAccountId =
"UPDATE accounts                                                             \n"
"SET flags = ?1                                                              \n"
"WHERE accounts.account_id = ?2;                                               ";

static const char* sqlListTopUnassignedSessionIds =
"SELECT sessions_info.session_id, player_aliases.alias, player_aliases.playtime, sessions_info.referenced \n"
"FROM sessions_info                                                                                       \n"
"INNER JOIN player_aliases ON sessions_info.session_id = player_aliases.session_id                        \n"
"WHERE sessions_info.account_id IS NULL                                                                   \n"
"ORDER BY sessions_info.referenced DESC, player_aliases.playtime DESC                                     \n"
"LIMIT ?1                                                                                                 \n"
"OFFSET ?2;                                                                                                 ";

static const char* sqlPurgeUnreferencedSessions =
"DELETE FROM sessions_info WHERE NOT referenced;                               ";

qboolean G_DBGetAccountByID( const int id,
	account_t* account )
{
	sqlite3_stmt* statement;

	sqlite3_prepare( dbPtr, sqlGetAccountByID, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, id );

	qboolean found = qfalse;
	int rc = sqlite3_step( statement );

	if ( rc == SQLITE_ROW ) {
		const char* name = ( const char* )sqlite3_column_text( statement, 0 );
		const int created_on = sqlite3_column_int( statement, 1 );
		const int flags = sqlite3_column_int( statement, 3 );

		account->id = id;
		Q_strncpyz( account->name, name, sizeof( account->name ) );
		account->creationDate = created_on;
		account->flags = flags;

		if ( sqlite3_column_type( statement, 2 ) != SQLITE_NULL ) {
			const char* usergroup = ( const char* )sqlite3_column_text( statement, 2 );
			Q_strncpyz( account->group, usergroup, sizeof( account->group ) );
		}
		else {
			account->group[0] = '\0';
		}

		found = qtrue;
	}

	sqlite3_finalize( statement );

	return found;
}

qboolean G_DBGetAccountByName( const char* name,
	account_t* account )
{
	sqlite3_stmt* statement;

	sqlite3_prepare( dbPtr, sqlGetAccountByName, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, name, -1, 0 );

	qboolean found = qfalse;
	int rc = sqlite3_step( statement );

	if ( rc == SQLITE_ROW ) {
		const int account_id = sqlite3_column_int( statement, 0 );
		const char* name = ( const char* )sqlite3_column_text( statement, 1 );
		const int created_on = sqlite3_column_int( statement, 2 );
		const int flags = sqlite3_column_int( statement, 4 );

		account->id = account_id;
		Q_strncpyz( account->name, name, sizeof( account->name ) );
		account->creationDate = created_on;
		account->flags = flags;

		if ( sqlite3_column_type( statement, 3 ) != SQLITE_NULL ) {
			const char* usergroup = ( const char* )sqlite3_column_text( statement, 3 );
			Q_strncpyz( account->group, usergroup, sizeof( account->group ) );
		} else {
			account->group[0] = '\0';
		}

		found = qtrue;
	}

	sqlite3_finalize( statement );

	return found;
}

void G_DBCreateAccount( const char* name )
{
	sqlite3_stmt* statement;

	sqlite3_prepare( dbPtr, sqlCreateAccount, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, name, -1, 0 );

	sqlite3_step( statement );

	sqlite3_finalize( statement );
}

qboolean G_DBDeleteAccount( account_t* account )
{
	sqlite3_stmt* statement;

	sqlite3_prepare( dbPtr, sqlDeleteAccount, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, account->id );

	sqlite3_step( statement );

	qboolean success = sqlite3_changes( dbPtr ) != 0;

	sqlite3_finalize( statement );

	return success;
}

qboolean G_DBGetSessionByID( const int id,
	session_t* session )
{
	sqlite3_stmt* statement;

	sqlite3_prepare( dbPtr, sqlGetSessionByID, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, id );

	qboolean found = qfalse;
	int rc = sqlite3_step( statement );

	if ( rc == SQLITE_ROW ) {
		const sessionInfoHash_t hash = sqlite3_column_int64( statement, 0 );
		const char* info = ( const char* )sqlite3_column_text( statement, 1 );

		session->id = id;
		session->hash = hash;
		Q_strncpyz( session->info, info, sizeof( session->info ) );

		if ( sqlite3_column_type( statement, 2 ) != SQLITE_NULL ) {
			const int account_id = sqlite3_column_int( statement, 2 );
			session->accountId = account_id;
		} else {
			session->accountId = ACCOUNT_ID_UNLINKED;
		}

		found = qtrue;
	}

	sqlite3_finalize( statement );

	return found;
}

qboolean G_DBGetSessionByHash( const sessionInfoHash_t hash,
	session_t* session )
{
	sqlite3_stmt* statement;

	sqlite3_prepare( dbPtr, sqlGetSessionByHash, -1, &statement, 0 );

	sqlite3_bind_int64( statement, 1, hash );

	qboolean found = qfalse;
	int rc = sqlite3_step( statement );

	if ( rc == SQLITE_ROW ) {
		const int session_id = sqlite3_column_int( statement, 0 );
		const char* info = ( const char* )sqlite3_column_text( statement, 1 );
		
		session->id = session_id;
		session->hash = hash;
		Q_strncpyz( session->info, info, sizeof( session->info ) );

		if ( sqlite3_column_type( statement, 2 ) != SQLITE_NULL ) {
			const int account_id = sqlite3_column_int( statement, 2 );
			session->accountId = account_id;
		} else {
			session->accountId = ACCOUNT_ID_UNLINKED;
		}

		found = qtrue;
	}

	sqlite3_finalize( statement );

	return found;
}

void G_DBCreateSession( const sessionInfoHash_t hash,
	const char* info )
{
	sqlite3_stmt* statement;

	sqlite3_prepare( dbPtr, sqlCreateSession, -1, &statement, 0 );

	sqlite3_bind_int64( statement, 1, hash );
	sqlite3_bind_text( statement, 2, info, -1, 0 );

	sqlite3_step( statement );

	sqlite3_finalize( statement );
}

void G_DBLinkAccountToSession( session_t* session,
	account_t* account )
{
	sqlite3_stmt* statement;

	sqlite3_prepare( dbPtr, sqlLinkAccountToSession, -1, &statement, 0 );

	if ( account ) {
		sqlite3_bind_int( statement, 1, account->id );
	} else {
		sqlite3_bind_null( statement, 1 );
	}
	
	sqlite3_bind_int( statement, 2, session->id );

	sqlite3_step( statement );

	// link in the struct too if successful
	if ( sqlite3_changes( dbPtr ) != 0 ) {
		session->accountId = account ? account->id : ACCOUNT_ID_UNLINKED;
	}

	sqlite3_finalize( statement );
}

void G_DBUnlinkAccountFromSession( session_t* session )
{
	G_DBLinkAccountToSession( session, NULL );
}

void G_DBListSessionsForAccount( account_t* account,
	DBListSessionsCallback callback,
	void* ctx )
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare( dbPtr, sqlListSessionIdsForAccount, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, account->id );

	rc = sqlite3_step( statement );
	while ( rc == SQLITE_ROW ) {
		session_t session;

		const int session_id = sqlite3_column_int( statement, 0 );
		const sqlite3_int64 hash = sqlite3_column_int64( statement, 1 );
		const char* info = ( const char* )sqlite3_column_text( statement, 2 );
		const qboolean referenced = !!sqlite3_column_int( statement, 3 );

		session.id = session_id;
		session.hash = hash;
		Q_strncpyz( session.info, info, sizeof( session.info ) );
		session.accountId = account->id;

		callback( ctx, &session, !referenced );

		rc = sqlite3_step( statement );
	}

	sqlite3_finalize( statement );
}

void G_DBListSessionsForInfo( const char* key,
	const char* value,
	DBListSessionsCallback callback,
	void* ctx )
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlListSessionIdsForInfo, -1, &statement, 0);

	sqlite3_bind_text(statement, 1, va( "$.%s", key ), -1, SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, value, -1, SQLITE_STATIC);

	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		session_t session;

		const int session_id = sqlite3_column_int(statement, 0);
		const sqlite3_int64 hash = sqlite3_column_int64(statement, 1);
		const char* info = (const char*)sqlite3_column_text(statement, 2);
		const qboolean referenced = !!sqlite3_column_int(statement, 4);

		session.id = session_id;
		session.hash = hash;
		Q_strncpyz(session.info, info, sizeof(session.info));

		if ( sqlite3_column_type( statement, 3 ) != SQLITE_NULL ) {
			const int account_id = sqlite3_column_int(statement, 3);
			session.accountId = account_id;
		} else {
			session.accountId = ACCOUNT_ID_UNLINKED;
		}

		callback(ctx, &session, !referenced);

		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
}

void G_DBSetAccountFlags( account_t* account,
	const int flags )
{
	sqlite3_stmt* statement;

	sqlite3_prepare( dbPtr, sqlSetFlagsForAccountId, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, flags );
	sqlite3_bind_int( statement, 2, account->id );

	sqlite3_step( statement );

	// link in the struct too if successful
	if ( sqlite3_changes( dbPtr ) != 0 ) {
		account->flags = flags;
	}

	sqlite3_finalize( statement );
}

void G_DBListTopUnassignedSessionIDs(pagination_t pagination,
	DBListUnassignedSessionIDsCallback callback,
	void* ctx)
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlListTopUnassignedSessionIds, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int(statement, 1, limit);
	sqlite3_bind_int(statement, 2, offset);

	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const int session_id = sqlite3_column_int(statement, 0);
		const char* alias = (const char*)sqlite3_column_text(statement, 1);
		const int playtime = sqlite3_column_int(statement, 2);
		const qboolean referenced = !!sqlite3_column_int(statement, 3);

		callback(ctx, session_id, alias, playtime, referenced);

		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
}

void G_DBPurgeUnassignedSessions(void)
{
	sqlite3_exec(dbPtr, sqlPurgeUnreferencedSessions, NULL, NULL, NULL);
}

// =========== NICKNAMES =======================================================

// TODO: this can be simplified with one UPSERT statement once linux sqlite can be updated
const char* const sqlLogNickname1 =
"INSERT OR IGNORE INTO nicknames(session_id, name) VALUES (?1, ?2);        ";
const char* const sqlLogNickname2 =
"UPDATE nicknames SET duration=duration+?1 WHERE session_id=?2 AND name=?3;";

const char* const sqlGetTopNicknames =
"SELECT name, duration                                                   \n"
"FROM nicknames WHERE session_id = ?1                                    \n"
"ORDER BY duration DESC LIMIT ?2;                                          ";

void G_DBLogNickname(const int sessionId,
	const char* name,
	int duration)
{
	sqlite3_stmt* statement1;
	sqlite3_stmt* statement2;

	// prepare insert statement
	sqlite3_prepare(dbPtr, sqlLogNickname1, -1, &statement1, 0);
	sqlite3_prepare(dbPtr, sqlLogNickname2, -1, &statement2, 0);

	sqlite3_bind_int(statement1, 1, sessionId);
	sqlite3_bind_text(statement1, 2, name, -1, 0);
	sqlite3_step(statement1);

	sqlite3_bind_int(statement2, 1, duration);
	sqlite3_bind_int(statement2, 2, sessionId);
	sqlite3_bind_text(statement2, 3, name, -1, 0);
	sqlite3_step(statement2);

	sqlite3_finalize(statement1);
	sqlite3_finalize(statement2);
}

void G_DBGetMostUsedNicknames(const int sessionId,
	const int numNicknames,
	nicknameEntry_t* outNicknames)
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlGetTopNicknames, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, sessionId);
	sqlite3_bind_int(statement, 2, numNicknames);

	int i = 0;
	memset(outNicknames, 0, sizeof(*outNicknames) * numNicknames);

	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW && i < numNicknames) {
		const char* name = (const char*)sqlite3_column_text(statement, 0);
		const int duration = sqlite3_column_int(statement, 1);

		nicknameEntry_t* nickname = outNicknames + i++;

		Q_strncpyz(nickname->name, name, sizeof(nickname->name));
		nickname->duration = duration;

		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
}

void G_DBGetTopNickname(const int sessionId,
	nicknameEntry_t* outNickname)
{
	G_DBGetMostUsedNicknames(sessionId, 1, outNickname);
}

// =========== FASTCAPS ========================================================

const char* const sqlGetFastcapRecord =
"SELECT DISTINCT capture_time, date, extra                                             \n"
"FROM fastcaps                                                                         \n"
"WHERE mapname = ?1 AND type = ?2 AND session_id = ?3;                                   ";

// TODO: this can be simplified with one UPSERT statement once linux sqlite can be updated
const char* const sqlSaveFastcapRecord1 =
"INSERT OR IGNORE INTO fastcaps(mapname, type, session_id, capture_time, date, extra)  \n"
"VALUES(?1, ?2, ?3, 0, 0, '')                                                            ";
const char* const sqlSaveFastcapRecord2 =
"UPDATE fastcaps SET capture_time = ?1, date = ?2, extra = ?3                          \n"
"WHERE mapname = ?4 AND type = ?5 AND session_id = ?6;                                   ";

const char* const sqlGetFastcapLoggedPersonalBest =
"SELECT DISTINCT rank, capture_time                                                    \n"
"FROM fastcaps_ranks WHERE mapname = ?1 AND type = ?2 AND account_id = ?3;               ";

const char* const sqlListFastcapsLoggedRecords =
"SELECT name, rank, capture_time, date, extra                                          \n"
"FROM fastcaps_ranks                                                                   \n"
"WHERE mapname = ?1 AND type = ?2                                                      \n"
"ORDER BY rank ASC                                                                     \n"
"LIMIT ?3                                                                              \n"
"OFFSET ?4;                                                                              ";

const char* const sqlListFastcapsLoggedTop =
"SELECT DISTINCT mapname, name, capture_time, date FROM fastcaps_ranks                 \n"
"WHERE type = ?1 AND rank = 1                                                          \n"
"ORDER BY mapname ASC                                                                  \n"
"LIMIT ?2                                                                              \n"
"OFFSET ?3;                                                                               ";

const char* const sqlListFastcapsLoggedLeaderboard =
"SELECT name, golds, silvers, bronzes                                                  \n"
"FROM fastcaps_leaderboard                                                             \n"
"WHERE type = ?1                                                                       \n"
"ORDER BY golds DESC, silvers DESC, bronzes DESC                                       \n"
"LIMIT ?2                                                                              \n"
"OFFSET ?3;                                                                              ";

const char* const sqlListFastcapsLoggedLatest =
"SELECT mapname, type, name, rank, capture_time, date                                  \n"
"FROM fastcaps_ranks                                                                   \n"
"ORDER BY date DESC                                                                    \n"
"LIMIT ?1                                                                              \n"
"OFFSET ?2;                                                                              ";

static void ReadExtraRaceInfo(const char* inJson, raceRecordInfo_t* outInfo) {
	cJSON* root = cJSON_Parse(inJson);

	if (root) {
		cJSON* j;

		j = cJSON_GetObjectItemCaseSensitive(root, "match_id");
		if (j && cJSON_IsString(j))
			Q_strncpyz(outInfo->matchId, j->valuestring, sizeof(outInfo->matchId));
		j = cJSON_GetObjectItemCaseSensitive(root, "player_name");
		if (j && cJSON_IsString(j))
			Q_strncpyz(outInfo->playerName, j->valuestring, sizeof(outInfo->playerName));
		j = cJSON_GetObjectItemCaseSensitive(root, "client_id");
		if (j && cJSON_IsNumber(j))
			outInfo->clientId = j->valueint;
		j = cJSON_GetObjectItemCaseSensitive(root, "pickup_time");
		if (j && cJSON_IsNumber(j))
			outInfo->pickupTime = j->valueint;
		j = cJSON_GetObjectItemCaseSensitive(root, "whose_flag");
		if (j && cJSON_IsNumber(j))
			outInfo->whoseFlag = j->valueint;
		j = cJSON_GetObjectItemCaseSensitive(root, "max_speed");
		if (j && cJSON_IsNumber(j))
			outInfo->maxSpeed = j->valueint;
		j = cJSON_GetObjectItemCaseSensitive(root, "avg_speed");
		if (j && cJSON_IsNumber(j))
			outInfo->avgSpeed = j->valueint;
	}

	cJSON_Delete(root);
}

static void WriteExtraRaceInfo(const raceRecordInfo_t* inInfo, char** outJson) {
	*outJson = NULL;

	cJSON* root = cJSON_CreateObject();

	if (root) {
		if (VALIDSTRING(inInfo->matchId))
			cJSON_AddStringToObject(root, "match_id", inInfo->matchId);
		if (VALIDSTRING(inInfo->playerName))
			cJSON_AddStringToObject(root, "player_name", inInfo->playerName);
		if (IN_CLIENTNUM_RANGE(inInfo->clientId))
			cJSON_AddNumberToObject(root, "client_id", inInfo->clientId);
		if (inInfo->pickupTime >= 0)
			cJSON_AddNumberToObject(root, "pickup_time", inInfo->pickupTime);
		if (inInfo->whoseFlag == TEAM_RED || inInfo->whoseFlag == TEAM_BLUE)
			cJSON_AddNumberToObject(root, "whose_flag", inInfo->whoseFlag);
		if (inInfo->maxSpeed >= 0)
			cJSON_AddNumberToObject(root, "max_speed", inInfo->maxSpeed);
		if (inInfo->avgSpeed >= 0)
			cJSON_AddNumberToObject(root, "avg_speed", inInfo->avgSpeed);

		*outJson = cJSON_PrintUnformatted(root);
	}

	cJSON_Delete(root);

	static char* emptyJson = "";
	if (!*outJson) {
		*outJson = emptyJson;
	}
}

// returns the account-agnostic session-tied personal best record used for caching
qboolean G_DBLoadRaceRecord(const int sessionId,
	const char* mapname,
	const raceType_t type,
	raceRecord_t* outRecord)
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlGetFastcapRecord, -1, &statement, 0);

	qboolean error = qfalse;

	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 2, type);
	sqlite3_bind_int(statement, 3, sessionId);

	memset(outRecord, 0, sizeof(*outRecord));

	rc = sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const int capture_time = sqlite3_column_int(statement, 0);
		const time_t date = sqlite3_column_int64(statement, 1);
		const char* extra = (const char*)sqlite3_column_text(statement, 2);
		
		outRecord->time = capture_time;
		outRecord->date = date;
		ReadExtraRaceInfo(extra, &outRecord->extra);

		rc = sqlite3_step(statement);
	} else if (rc != SQLITE_DONE) {
		error = qtrue;
	}

	sqlite3_finalize(statement);

	return !error;
}

// saves an account-agnostic session-tied personal best record
qboolean G_DBSaveRaceRecord(const int sessionId,
	const char* mapname,
	const raceType_t type,
	const raceRecord_t* inRecord)
{
	sqlite3_stmt* statement1;
	sqlite3_stmt* statement2;

	sqlite3_prepare(dbPtr, sqlSaveFastcapRecord1, -1, &statement1, 0);
	sqlite3_prepare(dbPtr, sqlSaveFastcapRecord2, -1, &statement2, 0);

	qboolean error = qfalse;

	sqlite3_bind_text(statement1, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement1, 2, type);
	sqlite3_bind_int(statement1, 3, sessionId);
	sqlite3_step(statement1);

	char* extra;
	WriteExtraRaceInfo(&inRecord->extra, &extra);

	// force saving in lowercase
	char* mapnameLowercase = strdup(mapname);
	Q_strlwr(mapnameLowercase);

	sqlite3_bind_int(statement2, 1, inRecord->time);
	sqlite3_bind_int(statement2, 2, inRecord->date);
	sqlite3_bind_text(statement2, 3, extra, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement2, 4, mapnameLowercase, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement2, 5, type);
	sqlite3_bind_int(statement2, 6, sessionId);
	int rc = sqlite3_step(statement2);

	error = rc != SQLITE_DONE;

	free(extra);
	free(mapnameLowercase);

	sqlite3_finalize(statement1);
	sqlite3_finalize(statement2);

	return !error;
}

// returns the PB for a given map/run type for an account for non NULL arguments (0 for none, -1 for error)
qboolean G_DBGetAccountPersonalBest(const int accountId,
	const char* mapname,
	const raceType_t type,
	int* outRank,
	int* outTime)
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlGetFastcapLoggedPersonalBest, -1, &statement, 0);

	qboolean errored = qfalse;

	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 2, type);
	sqlite3_bind_int(statement, 3, accountId);

	if (outRank)
		*outRank = 0;
	if (outTime)
		*outTime = 0;

	rc = sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const int rank = sqlite3_column_int(statement, 0);
		const int capture_time = sqlite3_column_int(statement, 1);

		if (outRank)
			*outRank = rank;
		if (outTime)
			*outTime = capture_time;

		rc = sqlite3_step(statement);
	} else if (rc != SQLITE_DONE) {
		if (outRank)
			*outRank = -1;
		if (outTime)
			*outTime = -1;

		errored = qtrue;
	}

	sqlite3_finalize(statement);

	return !errored;
}

// lists logged in records sorted by rank for a map/type
void G_DBListRaceRecords(const char* mapname,
	const raceType_t type,
	const pagination_t pagination,
	ListRaceRecordsCallback callback,
	void* context)
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlListFastcapsLoggedRecords, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 2, type);
	sqlite3_bind_int(statement, 3, limit);
	sqlite3_bind_int(statement, 4, offset);

	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char* name = (const char*)sqlite3_column_text(statement, 0);
		const int rank = sqlite3_column_int(statement, 1);
		const int capture_time = sqlite3_column_int(statement, 2);
		const time_t date = sqlite3_column_int64(statement, 3);
		const char* extra = (const char*)sqlite3_column_text(statement, 4);

		raceRecord_t record;
		record.time = capture_time;
		record.date = date;
		ReadExtraRaceInfo(extra, &record.extra);

		callback(context, mapname, type, rank, name, &record);

		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
}

// lists only the best records for a type sorted by mapname
void G_DBListRaceTop(const raceType_t type,
	const pagination_t pagination,
	ListRaceTopCallback callback,
	void* context)
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlListFastcapsLoggedTop, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int(statement, 1, type);
	sqlite3_bind_int(statement, 2, limit);
	sqlite3_bind_int(statement, 3, offset);

	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char* mapname = (const char*)sqlite3_column_text(statement, 0);
		const char* name = (const char*)sqlite3_column_text(statement, 1);
		const int capture_time = sqlite3_column_int(statement, 2);
		const time_t date = sqlite3_column_int64(statement, 3);

		callback(context, mapname, type, name, capture_time, date);

		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
}

// lists logged in leaderboard sorted by golds, silvers, bronzes
void G_DBListRaceLeaderboard(const raceType_t type,
	const pagination_t pagination,
	ListRaceLeaderboardCallback callback,
	void* context)
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlListFastcapsLoggedLeaderboard, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int(statement, 1, type);
	sqlite3_bind_int(statement, 2, limit);
	sqlite3_bind_int(statement, 3, offset);

	int rank = 1 + offset;

	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char* name = (const char*)sqlite3_column_text(statement, 0);
		const int golds = sqlite3_column_int(statement, 1);
		const int silvers = sqlite3_column_int(statement, 2);
		const int bronzes = sqlite3_column_int(statement, 3);

		callback(context, type, rank++, name, golds, silvers, bronzes);

		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
}

// lists logged in latest records across all maps/types
void G_DBListRaceLatest(const pagination_t pagination,
	ListRaceLatestCallback callback,
	void* context)
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlListFastcapsLoggedLatest, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int(statement, 1, limit);
	sqlite3_bind_int(statement, 2, offset);

	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char* mapname = (const char*)sqlite3_column_text(statement, 0);
		const int type = sqlite3_column_int(statement, 1);
		const char* name = (const char*)sqlite3_column_text(statement, 2);
		const int rank = sqlite3_column_int(statement, 3);
		const int capture_time = sqlite3_column_int(statement, 4);
		const time_t date = sqlite3_column_int64(statement, 5);

		callback(context, mapname, type, rank, name, capture_time, date);

		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
}

// =========== WHITELIST =======================================================

const char* const sqlIsIpWhitelisted =
"SELECT COUNT(*)                                                             \n"
"FROM ip_whitelist                                                           \n"
"WHERE( ip_int & mask_int ) = (? & mask_int)                                   ";

const char* const sqlAddToWhitelist =
"INSERT INTO ip_whitelist (ip_int, mask_int, notes)                          \n"
"VALUES (?,?,?)                                                                ";

const char* const sqlremoveFromWhitelist =
"DELETE FROM ip_whitelist                                                    \n"
"WHERE ip_int = ?                                                            \n"
"AND mask_int = ?                                                              ";

qboolean G_DBIsFilteredByWhitelist( unsigned int ip,
	char* reasonBuffer,
	int reasonBufferSize )
{
	qboolean filtered = qfalse;

	// check if ip is on white list
	sqlite3_stmt* statement;

	// prepare whitelist check statement
	sqlite3_prepare( dbPtr, sqlIsIpWhitelisted, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );

	sqlite3_step( statement );
	int count = sqlite3_column_int( statement, 0 );

	if ( count == 0 )
	{
		Q_strncpyz( reasonBuffer, "IP address not on whitelist", reasonBufferSize );
		filtered = qtrue;
	}

	sqlite3_finalize( statement );

	return filtered;
}

qboolean G_DBAddToWhitelist( unsigned int ip,
	unsigned int mask,
	const char* notes )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlAddToWhitelist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	sqlite3_bind_text( statement, 3, notes, -1, 0 );

	rc = sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	sqlite3_finalize( statement );

	return success;
}

qboolean G_DBRemoveFromWhitelist( unsigned int ip,
	unsigned int mask )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlremoveFromWhitelist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	rc = sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	sqlite3_finalize( statement );

	return success;
}

// =========== BLACKLIST =======================================================

const char* const sqlIsIpBlacklisted =
"SELECT reason                                                               \n"
"FROM ip_blacklist                                                           \n"
"WHERE( ip_int & mask_int ) = (? & mask_int)                                 \n"
"AND banned_until >= datetime('now')                                           ";

const char* const sqlListBlacklist =
"SELECT ip_int, mask_int, notes, reason, banned_since, banned_until          \n"
"FROM ip_blacklist                                                             ";

const char* const sqlAddToBlacklist =
"INSERT INTO ip_blacklist (ip_int,                                           \n"
"mask_int, notes, reason, banned_since, banned_until)                        \n"
"VALUES (?,?,?,?,datetime('now'),datetime('now','+'||?||' hours'))             ";

const char* const sqlRemoveFromBlacklist =
"DELETE FROM ip_blacklist                                                    \n"
"WHERE ip_int = ?                                                            \n"
"AND mask_int = ?                                                              ";

qboolean G_DBIsFilteredByBlacklist( unsigned int ip,
	char* reasonBuffer,
	int reasonBufferSize )
{
	qboolean filtered = qfalse;

	sqlite3_stmt* statement;

	// prepare blacklist check statement
	int rc = sqlite3_prepare( dbPtr, sqlIsIpBlacklisted, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );

	rc = sqlite3_step( statement );

	// blacklisted => we forbid it
	if ( rc == SQLITE_ROW )
	{
		const char* reason = ( const char* )sqlite3_column_text( statement, 0 );
		const char* prefix = "Banned: ";
		int prefixSize = strlen( prefix );

		Q_strncpyz( reasonBuffer, prefix, reasonBufferSize );
		Q_strncpyz( &reasonBuffer[prefixSize], reason, reasonBufferSize - prefixSize );
		filtered = qtrue;
	}

	sqlite3_finalize( statement );

	return filtered;
}

void G_DBListBlacklist( BlackListCallback callback )
{
	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlListBlacklist, -1, &statement, 0 );

	rc = sqlite3_step( statement );
	while ( rc == SQLITE_ROW )
	{
		unsigned int ip = sqlite3_column_int( statement, 0 );
		unsigned int mask = sqlite3_column_int( statement, 1 );
		const char* notes = ( const char* )sqlite3_column_text( statement, 2 );
		const char* reason = ( const char* )sqlite3_column_text( statement, 3 );
		const char* banned_since = ( const char* )sqlite3_column_text( statement, 4 );
		const char* banned_until = ( const char* )sqlite3_column_text( statement, 5 );

		callback( ip, mask, notes, reason, banned_since, banned_until );

		rc = sqlite3_step( statement );
	}

	sqlite3_finalize( statement );
}

qboolean G_DBAddToBlacklist( unsigned int ip,
	unsigned int mask,
	const char* notes,
	const char* reason,
	int hours )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlAddToBlacklist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	sqlite3_bind_text( statement, 3, notes, -1, 0 );
	sqlite3_bind_text( statement, 4, reason, -1, 0 );

	sqlite3_bind_int( statement, 5, hours );

	rc = sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	sqlite3_finalize( statement );

	return success;
}

qboolean G_DBRemoveFromBlacklist( unsigned int ip,
	unsigned int mask )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlRemoveFromBlacklist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	rc = sqlite3_step( statement );

	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	sqlite3_finalize( statement );

	return success;
}

// =========== MAP POOLS =======================================================

const char* const sqlListPools =
"SELECT pool_id, short_name, long_name                                       \n"
"FROM pools                                                                    ";

const char* const sqlListMapsInPool =
"SELECT long_name, pools.pool_id, mapname, weight                            \n"
"FROM pools                                                                  \n"
"JOIN pool_has_map                                                           \n"
"ON pools.pool_id = pool_has_map.pool_id                                     \n"
"WHERE short_name = ? AND mapname <> ?                                         ";

const char* const sqlFindPool =
"SELECT pools.pool_id, long_name                                             \n"
"FROM pools                                                                  \n"
"JOIN                                                                        \n"
"pool_has_map                                                                \n"
"ON pools.pool_id = pool_has_map.pool_id                                     \n"
"WHERE short_name = ?                                                          ";

const char* const sqlCreatePool =
"INSERT INTO pools (short_name, long_name)                                   \n"
"VALUES (?,?)                                                                  ";

const char* const sqlDeleteAllMapsInPool =
"DELETE FROM pool_has_map                                                    \n"
"WHERE pool_id                                                               \n"
"IN                                                                          \n"
"( SELECT pools.pool_id                                                      \n"
"FROM pools                                                                  \n"
"JOIN pool_has_map                                                           \n"
"ON pools.pool_id = pool_has_map.pool_id                                     \n"
"WHERE short_name = ? )                                                        ";

const char* const sqlDeletePool =
"DELETE FROM pools                                                           \n"
"WHERE short_name = ?;                                                         ";

const char* const sqlAddMapToPool =
"INSERT INTO pool_has_map (pool_id, mapname, weight)                         \n"
"SELECT pools.pool_id, ?, ?                                                  \n"
"FROM pools                                                                  \n"
"WHERE short_name = ?                                                          ";

const char* const sqlRemoveMapToPool =
"DELETE FROM pool_has_map                                                    \n"
"WHERE pool_id                                                               \n"
"IN                                                                          \n"
"( SELECT pools.pool_id                                                      \n"
"FROM pools                                                                  \n"
"JOIN pool_has_map                                                           \n"
"ON pools.pool_id = pool_has_map.pool_id                                     \n"
"WHERE short_name = ? )                                                      \n"
"AND mapname = ? ;                                                             ";

void G_DBListPools( ListPoolCallback callback,
	void* context )
{
	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlListPools, -1, &statement, 0 );

	rc = sqlite3_step( statement );
	while ( rc == SQLITE_ROW )
	{
		int pool_id = sqlite3_column_int( statement, 0 );
		const char* short_name = ( const char* )sqlite3_column_text( statement, 1 );
		const char* long_name = ( const char* )sqlite3_column_text( statement, 2 );

		callback( context, pool_id, short_name, long_name );

		rc = sqlite3_step( statement );
	}

	sqlite3_finalize( statement );
}

void G_DBListMapsInPool( const char* short_name,
	const char* ignore,
	ListMapsPoolCallback callback,
	void** context )
{
	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlListMapsInPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );
	sqlite3_bind_text( statement, 2, ignore, -1, 0 ); // ignore map, we 

	rc = sqlite3_step( statement );
	while ( rc == SQLITE_ROW )
	{
		const char* long_name = ( const char* )sqlite3_column_text( statement, 0 );
		int pool_id = sqlite3_column_int( statement, 1 );
		const char* mapname = ( const char* )sqlite3_column_text( statement, 2 );
		int weight = sqlite3_column_int( statement, 3 );
		if ( weight < 1 ) weight = 1;

		if ( Q_stricmp( mapname, ignore ) ) {
			callback( context, long_name, pool_id, mapname, weight );
		}

		rc = sqlite3_step( statement );
	}

	sqlite3_finalize( statement );
}

qboolean G_DBFindPool( const char* short_name,
	PoolInfo* poolInfo )
{
	qboolean found = qfalse;

	sqlite3_stmt* statement;

	// prepare blacklist check statement
	int rc = sqlite3_prepare( dbPtr, sqlFindPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );

	rc = sqlite3_step( statement );

	// blacklisted => we forbid it
	if ( rc == SQLITE_ROW )
	{
		int pool_id = sqlite3_column_int( statement, 0 );
		const char* long_name = ( const char* )sqlite3_column_text( statement, 1 );

		Q_strncpyz( poolInfo->long_name, long_name, sizeof( poolInfo->long_name ) );
		poolInfo->pool_id = pool_id;

		found = qtrue;
	}

	sqlite3_finalize( statement );

	return found;
}

typedef struct CumulativeMapWeight {
	char mapname[MAX_MAP_NAME];
	int cumulativeWeight; // the sum of all previous weights in the list and the current one
	struct CumulativeMapWeight *next;
} CumulativeMapWeight;

static void BuildCumulativeWeight( void** context,
	const char* long_name,
	int pool_id,
	const char* mapname,
	int mapWeight )
{
	CumulativeMapWeight **cdf = ( CumulativeMapWeight** )context;

	// first, create the current node using parameters
	CumulativeMapWeight *currentNode = malloc( sizeof( *currentNode ) );
	Q_strncpyz( currentNode->mapname, mapname, sizeof( currentNode->mapname ) );
	currentNode->cumulativeWeight = mapWeight;
	currentNode->next = NULL;

	if ( !*cdf ) {
		// this is the first item, just assign it
		*cdf = currentNode;
	}
	else {
		// otherwise, add it to the end of the list
		CumulativeMapWeight *n = *cdf;

		while ( n->next ) {
			n = n->next;
		}

		currentNode->cumulativeWeight += n->cumulativeWeight; // add the weight of the previous node
		n->next = currentNode;
	}
}

qboolean G_DBSelectMapsFromPool( const char* short_name,
	const char* ignoreMap,
	const int mapsToRandomize,
	MapSelectedCallback callback,
	void* context )
{
	PoolInfo poolInfo;
	if ( G_DBFindPool( short_name, &poolInfo ) )
	{
		// fill the cumulative density function of the map pool
		CumulativeMapWeight *cdf = NULL;
		G_DBListMapsInPool( short_name, ignoreMap, BuildCumulativeWeight, ( void ** )&cdf );

		if ( cdf ) {
			CumulativeMapWeight *n = cdf;
			int i, numMapsInList = 0;

			while ( n ) {
				++numMapsInList; // if we got here, we have at least 1 map, so this will always be at least 1
				n = n->next;
			}

			// pick as many maps as needed from the pool while there are enough in the list
			for ( i = 0; i < mapsToRandomize && numMapsInList > 0; ++i ) {
				// first, get a random number based on the highest cumulative weight
				n = cdf;
				while ( n->next ) {
					n = n->next;
				}
				const int random = rand() % n->cumulativeWeight;

				// now, pick the map
				n = cdf;
				while ( n ) {
					if ( random < n->cumulativeWeight ) {
						break; // got it
					}
					n = n->next;
				}

				// let the caller do whatever they want with the map we picked
				callback( context, n->mapname );

				// delete the map from the cdf for further iterations
				CumulativeMapWeight *nodeToDelete = n;
				int weightToDelete;

				// get the node right before the node to delete and find the weight to delete
				if ( nodeToDelete == cdf ) {
					n = NULL;
					weightToDelete = cdf->cumulativeWeight; // the head's weight is equal to its cumulative weight
				}
				else {
					n = cdf;
					while ( n->next && n->next != nodeToDelete ) {
						n = n->next;
					}
					weightToDelete = nodeToDelete->cumulativeWeight - n->cumulativeWeight;
				}

				// relink the list
				if ( !n ) {
					cdf = cdf->next; // we are deleting the head, so just rebase it (may be NULL if this was the last remaining element)
				}
				else {
					n->next = nodeToDelete->next; // may be NULL if we are deleting the last element in the list
				}
				--numMapsInList;

				// rebuild the cumulative weights
				n = nodeToDelete->next;
				while ( n ) {
					n->cumulativeWeight -= weightToDelete;
					n = n->next;
				}

				free( nodeToDelete );
			}

			// free the remaining resources
			n = cdf;
			while ( n ) {
				CumulativeMapWeight *nodeToDelete = n;
				n = n->next;
				free( nodeToDelete );
			}

			return qtrue;
		}
	}

	return qfalse;
}

qboolean G_DBPoolCreate( const char* short_name,
	const char* long_name )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlCreatePool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );
	sqlite3_bind_text( statement, 2, long_name, -1, 0 );

	rc = sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	sqlite3_finalize( statement );

	return success;
}

qboolean G_DBPoolDeleteAllMaps( const char* short_name )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlDeleteAllMapsInPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );

	rc = sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	sqlite3_finalize( statement );

	return success;
}

qboolean G_DBPoolDelete( const char* short_name )
{
	qboolean success = qfalse;

	if ( G_DBPoolDeleteAllMaps( short_name ) )
	{
		sqlite3_stmt* statement;
		// prepare insert statement
		int rc = sqlite3_prepare( dbPtr, sqlDeletePool, -1, &statement, 0 );

		sqlite3_bind_text( statement, 1, short_name, -1, 0 );

		rc = sqlite3_step( statement );
		if ( rc == SQLITE_DONE )
		{
			int changes = sqlite3_changes( dbPtr );
			if ( changes != 0 )
			{
				success = qtrue;
			}
		}

		sqlite3_finalize( statement );
	}

	return success;
}

qboolean G_DBPoolMapAdd( const char* short_name,
	const char* mapname,
	int weight )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlAddMapToPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, mapname, -1, 0 );
	sqlite3_bind_int( statement, 2, weight );
	sqlite3_bind_text( statement, 3, short_name, -1, 0 );

	rc = sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	sqlite3_finalize( statement );

	return success;
}

qboolean G_DBPoolMapRemove( const char* short_name,
	const char* mapname )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, sqlRemoveMapToPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );
	sqlite3_bind_text( statement, 2, mapname, -1, 0 );

	rc = sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	sqlite3_finalize( statement );

	return success;
}
