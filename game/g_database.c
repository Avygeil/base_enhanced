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
	sqlite3_exec( dbPtr, "PRAGMA automatic_index = OFF;", NULL, NULL, NULL );

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

	sqlite3_prepare_v2( dbPtr, sqlSetMetadata, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, key, -1, SQLITE_STATIC );
	sqlite3_bind_text( statement, 2, value, -1, SQLITE_STATIC );

	sqlite3_step( statement );

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
"WHERE sessions_info.account_id = ?1                                         \n"
"LIMIT ?2                                                                    \n"
"OFFSET ?3;                                                                    ";

static const char* sqlListSessionIdsForInfo =
"SELECT session_id, hash, info, account_id, referenced                       \n"
"FROM sessions_info                                                          \n"
"WHERE json_extract(info, ?1) = ?2                                           \n"
"LIMIT ?3                                                                    \n"
"OFFSET ?4;                                                                    ";

static const char* sqlSetFlagsForAccountId =
"UPDATE accounts                                                             \n"
"SET flags = ?1                                                              \n"
"WHERE accounts.account_id = ?2;                                               ";

static const char* sqlGetPlaytimeForAccountId =
"SELECT playtime FROM player_aliases WHERE account_id = ?1 LIMIT 1;            ";

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
	pagination_t pagination,
	DBListSessionsCallback callback,
	void* ctx )
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare( dbPtr, sqlListSessionIdsForAccount, -1, &statement, 0 );

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int( statement, 1, account->id );
	sqlite3_bind_int(statement, 2, limit);
	sqlite3_bind_int(statement, 3, offset);

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

		callback( ctx, &session, referenced );

		rc = sqlite3_step( statement );
	}

	sqlite3_finalize( statement );
}

void G_DBListSessionsForInfo( const char* key,
	const char* value,
	pagination_t pagination,
	DBListSessionsCallback callback,
	void* ctx )
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlListSessionIdsForInfo, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_text(statement, 1, va( "$.%s", key ), -1, SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, value, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 3, limit);
	sqlite3_bind_int(statement, 4, offset);

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

		callback(ctx, &session, referenced);

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

int G_DBGetAccountPlaytime( account_t* account )
{
	sqlite3_stmt* statement;

	int rc = sqlite3_prepare(dbPtr, sqlGetPlaytimeForAccountId, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, account->id);

	int result = 0;

	rc = sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const int playtime = sqlite3_column_int(statement, 0);

		result = playtime;

		rc = sqlite3_step(statement);
	} else if (rc != SQLITE_DONE) {
		result = -1;
	}

	sqlite3_finalize(statement);

	return result;
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

const char *const sqlListMapsInPoolWithCooldown =
"SELECT long_name, pools.pool_id, mapname, weight                            \n"
"FROM pools                                                                  \n"
"JOIN pool_has_map                                                           \n"
"ON pools.pool_id = pool_has_map.pool_id                                     \n"
"LEFT JOIN lastplayedmap ON mapname = lastplayedmap.map                      \n"
"WHERE short_name = ? AND mapname <> ?                                       \n"
"AND (lastplayedmap.map IS NULL OR                                           \n"
"strftime('%s', 'now') - lastplayedmap.datetime > ?)                         \n";

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
	void** context,
	char *longNameOut,
	size_t longNameOutSize,
	int notPlayedWithinLastMinutes)
{
	int cooldownSeconds = notPlayedWithinLastMinutes > 0 ? (notPlayedWithinLastMinutes * 60) : 0;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = sqlite3_prepare( dbPtr, cooldownSeconds > 0 ? sqlListMapsInPoolWithCooldown : sqlListMapsInPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );
	sqlite3_bind_text( statement, 2, ignore, -1, 0 ); // ignore map, we
	if (cooldownSeconds > 0)
		sqlite3_bind_int(statement, 3, cooldownSeconds);

	rc = sqlite3_step( statement );
	while ( rc == SQLITE_ROW )
	{
		const char* long_name = ( const char* )sqlite3_column_text( statement, 0 );
		if (VALIDSTRING(long_name) && longNameOut && longNameOutSize)
			Q_strncpyz(longNameOut, long_name, longNameOutSize);
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
		G_DBListMapsInPool( short_name, ignoreMap, BuildCumulativeWeight, ( void ** )&cdf, NULL, 0, g_vote_mapCooldownMinutes.integer > 0 ? g_vote_mapCooldownMinutes.integer : 0 );

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

typedef struct {
	int numMapsOfTier[NUM_MAPTIERS];
} tierListInfo_t;

// check to make sure we just have numbers and commas
// wHy DoN't YoU jUsT bInD eVeRyTh- stfu
static void FilterAccountIdString(char *string) {
	char *out = string, *p = string, c;
	while ((c = *p++) != '\0') {
		if (c == ',' || (c >= '0' && c <= '9'))
			*out++ = c;
	}
	*out = '\0';
}

// Versatile method to retrieve a tier list in the form of a malloc'd list_t pointer, with several customizable parameters.
// commaSeparatedAccountIds: null for zero accounts, e.g. "1" for one account, e.g. "1,2,3,4" for multiple accounts.
// singleMapFileName: null for all maps; otherwise specify a map to just check one specific map.
// requireMultipleVotes: qtrue means at least 2 votes from among the accounts being evaluated are required for a map to make the list.
// notPlayedWithinLastMinutes: only return maps that have not been pugged on within the last X minutes (use 0 to not use this).
// ignoreMapFileName: null to not ignore any maps; otherwise specify a map to ignore.
// randomize: randomize the list order.
// optionalInfoOut: tierListInfo_t pointer you can supply to output additional info.
// IMPORTANT: free the result if not null!
static list_t *GetTierList(const char *commaSeparatedAccountIds, const char *singleMapFileName, qboolean requireMultipleVotes, int notPlayedWithinLastMinutes, const char *ignoreMapFileName, qboolean randomize, tierListInfo_t *optionalInfoOut) {
	int cooldownSeconds = notPlayedWithinLastMinutes > 0 ? (notPlayedWithinLastMinutes * 60) : 0;
	if (optionalInfoOut)
		memset(optionalInfoOut, 0, sizeof(tierListInfo_t));

	char *filteredAccountIds = NULL;
	if (VALIDSTRING(commaSeparatedAccountIds)) {
		filteredAccountIds = strdup(commaSeparatedAccountIds);
		FilterAccountIdString(filteredAccountIds);
	}

	char *query;
	if (VALIDSTRING(filteredAccountIds)) { // list for only a few specific people
		if (requireMultipleVotes) { // only select maps that have received 2+ votes from among the supplied account ids
			char *str1 = "SELECT tierlistmaps.map, AVG(tierlistmaps.tier) FROM (SELECT tierlistmaps.map FROM tierlistmaps JOIN tierwhitelist ON tierwhitelist.map = tierlistmaps.map WHERE tierlistmaps.account_id IN (";
			char *str2 = filteredAccountIds;
			char *str3 = ") GROUP BY tierlistmaps.map HAVING COUNT(*) >= 2) m JOIN tierlistmaps ON m.map = tierlistmaps.map ";
			char *str4 = cooldownSeconds > 0 ? va("LEFT JOIN lastplayedmap ON tierlistmaps.map = lastplayedmap.map WHERE (lastplayedmap.map IS NULL OR strftime('%%s', 'now') - lastplayedmap.datetime > %d) AND account_id IN (", cooldownSeconds) : "WHERE account_id IN (";
			char *str5 = filteredAccountIds;
			char *str6 = VALIDSTRING(singleMapFileName) ? ") AND tierlistmaps.map = ? " : ") ";
			char *str7 = VALIDSTRING(ignoreMapFileName) ? "AND tierlistmaps.map != ? " : "";
			char *str8 = "GROUP BY tierlistmaps.map ORDER BY ";
			char *str9 = randomize ? "RANDOM();" : "tierlistmaps.map;";
			query = va("%s%s%s%s%s%s%s%s%s", str1, str2, str3, str4, str5, str6, str7, str8, str9);
		}
		else { // select maps even if they just have a single vote
			char *str1 = "SELECT tierlistmaps.map, AVG(tierlistmaps.tier) FROM (SELECT DISTINCT tierlistmaps.map FROM tierlistmaps JOIN tierwhitelist ON tierwhitelist.map = tierlistmaps.map";
			char *str2 = VALIDSTRING(singleMapFileName) ? va(" WHERE tierlistmaps.map = ?") : "";
			char *str3 = ") m JOIN tierlistmaps ON m.map = tierlistmaps.map ";
			char *str4 = cooldownSeconds > 0 ? va("LEFT JOIN lastplayedmap ON tierlistmaps.map = lastplayedmap.map WHERE (lastplayedmap.map IS NULL OR strftime('%%s', 'now') - lastplayedmap.datetime > %d) AND account_id IN (", cooldownSeconds) : "WHERE account_id IN (";
			char *str5 = filteredAccountIds;
			char *str6 = VALIDSTRING(ignoreMapFileName) ? ") AND tierlistmaps.map != ? " : ") ";
			char *str7 = "GROUP BY tierlistmaps.map ORDER BY ";
			char *str8 = randomize ? "RANDOM();" : "tierlistmaps.map;";
			query = va("%s%s%s%s%s%s%s%s", str1, str2, str3, str4, str5, str6, str7, str8);
		}
		free(filteredAccountIds);
	}
	else { // community-wide list
		if (requireMultipleVotes) { // only select maps that have received 2+ votes
			char *str1 = "SELECT tierlistmaps.map, AVG(tierlistmaps.tier) FROM (SELECT DISTINCT tierlistmaps.map FROM tierlistmaps JOIN tierwhitelist ON tierwhitelist.map = tierlistmaps.map ";
			char *str2 = VALIDSTRING(singleMapFileName) ? "WHERE tierlistmaps.map = ? " : "";
			char *str3 = "GROUP BY tierlistmaps.map HAVING COUNT(*) >= 2) m JOIN tierlistmaps ON m.map = tierlistmaps.map ";
			char *str4 = cooldownSeconds > 0 ? va("LEFT JOIN lastplayedmap ON tierlistmaps.map = lastplayedmap.map WHERE (lastplayedmap.map IS NULL OR strftime('%%s', 'now') - lastplayedmap.datetime > %d) ", cooldownSeconds) : "";
			char *str5 = VALIDSTRING(ignoreMapFileName) ? (cooldownSeconds > 0 ? "AND tierlistmaps.map != ? " : "WHERE tierlistmaps.map != ? ") : "";
			char *str6 = "GROUP BY tierlistmaps.map ORDER BY tierlistmaps.map;";
			query = va("%s%s%s%s%s%s", str1, str2, str3, str4, str5, str6);
		}
		else { // select maps even if they just have a single vote
			char *str1 = "SELECT tierlistmaps.map, AVG(tierlistmaps.tier) FROM (SELECT DISTINCT tierlistmaps.map FROM tierlistmaps JOIN tierwhitelist ON tierwhitelist.map = tierlistmaps.map";
			char *str2 = VALIDSTRING(singleMapFileName) ? " WHERE tierlistmaps.map = ?) " : ") ";
			char *str3 = "m JOIN tierlistmaps ON m.map = tierlistmaps.map ";
			char *str4 = cooldownSeconds > 0 ? va("LEFT JOIN lastplayedmap ON tierlistmaps.map = lastplayedmap.map WHERE (lastplayedmap.map IS NULL OR strftime('%%s', 'now') - lastplayedmap.datetime > %d) ", cooldownSeconds) : "";
			char *str5 = VALIDSTRING(ignoreMapFileName) ? (cooldownSeconds > 0 ? "AND tierlistmaps.map != ? " : "WHERE tierlistmaps.map != ? ") : "";
			char *str6 = "GROUP BY tierlistmaps.map ORDER BY tierlistmaps.map;";
			query = va("%s%s%s%s%s%s", str1, str2, str3, str4, str5, str6);
		}
	}

	sqlite3_stmt *statement;
	int rc = sqlite3_prepare(dbPtr, query, -1, &statement, 0);
	int bindMe = 0;
	if (VALIDSTRING(singleMapFileName)) {
		char lowercase[MAX_QPATH] = { 0 };
		Q_strncpyz(lowercase, singleMapFileName, sizeof(lowercase));
		Q_strlwr(lowercase);
		sqlite3_bind_text(statement, ++bindMe, lowercase, -1, 0);
	}
	if (VALIDSTRING(ignoreMapFileName)) {
		char lowercase[MAX_QPATH] = { 0 };
		Q_strncpyz(lowercase, ignoreMapFileName, sizeof(lowercase));
		Q_strlwr(lowercase);
		sqlite3_bind_text(statement, ++bindMe, lowercase, -1, 0);
	}

	rc = sqlite3_step(statement);

	list_t *mapsList = malloc(sizeof(list_t));
	memset(mapsList, 0, sizeof(list_t));
	int numGotten = 0;

	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		mapTier_t tier = MapTierForDouble(average);
		if (VALIDSTRING(mapFileName) && tier >= MAPTIER_F && tier <= MAPTIER_S) {
			mapTierData_t *data = (mapTierData_t *)ListAdd(mapsList, sizeof(mapTierData_t));
			data->tier = tier;
			Q_strncpyz(data->mapFileName, mapFileName, sizeof(data->mapFileName));
			if (optionalInfoOut)
				optionalInfoOut->numMapsOfTier[tier]++;
			numGotten++;
		}
		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
	if (!numGotten) {
		ListClear(mapsList);
		return NULL;
	}

	return mapsList;
}

const char *sqlTierlistMapIsWhitelisted = "SELECT COUNT(*) FROM tierwhitelist WHERE map = ?1;";
qboolean G_DBTierlistMapIsWhitelisted(const char *mapName) {
	if (!VALIDSTRING(mapName))
		return qfalse;

	char mapFileName[MAX_QPATH] = { 0 };
	GetMatchingMap(mapName, mapFileName, sizeof(mapFileName));
	if (!mapFileName[0])
		return qfalse;

	sqlite3_stmt *statement;
	sqlite3_prepare(dbPtr, sqlTierlistMapIsWhitelisted, -1, &statement, 0);
	sqlite3_bind_text(statement, 1, mapFileName, -1, 0);
	int rc = sqlite3_step(statement);
	qboolean found = qfalse;
	if (rc == SQLITE_ROW)
		found = !!(sqlite3_column_int(statement, 0));

	sqlite3_finalize(statement);

	return found;
}

static qboolean PrintMapTierList(int printClientNum, list_t *mapList, qboolean useIngamePlayers, const char *successPrologueMessage) {
	if (!mapList)
		return qfalse;

	char sStr[2048] = { 0 }, aStr[2048] = { 0 }, bStr[2048] = { 0 }, cStr[2048] = { 0 }, fStr[2048] = { 0 };
	Com_sprintf(sStr, sizeof(sStr), "%s: ^7", GetTierStringForTier(MAPTIER_S));
	Com_sprintf(aStr, sizeof(aStr), "%s: ^7", GetTierStringForTier(MAPTIER_A));
	Com_sprintf(bStr, sizeof(bStr), "%s: ^7", GetTierStringForTier(MAPTIER_B));
	Com_sprintf(cStr, sizeof(cStr), "%s: ^7", GetTierStringForTier(MAPTIER_C));
	Com_sprintf(fStr, sizeof(fStr), "%s: ^7", GetTierStringForTier(MAPTIER_F));

	int numS = 0, numA = 0, numB = 0, numC = 0, numF = 0;

	iterator_t iter;
	ListIterate(mapList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		mapTierData_t *data = (mapTierData_t *)IteratorNext(&iter);
		char mapShortName[MAX_QPATH] = { 0 };
		GetShortNameForMapFileName(data->mapFileName, mapShortName, sizeof(mapShortName));

		if (VALIDSTRING(mapShortName)) {
			if (data->tier == MAPTIER_S) {
				Q_strcat(sStr, sizeof(sStr), va("%s%s", numS ? ", " : "", mapShortName));
				numS++;
			}
			else if (data->tier == MAPTIER_A) {
				Q_strcat(aStr, sizeof(aStr), va("%s%s", numA ? ", " : "", mapShortName));
				numA++;
			}
			else if (data->tier == MAPTIER_B) {
				Q_strcat(bStr, sizeof(bStr), va("%s%s", numB ? ", " : "", mapShortName));
				numB++;
			}
			else if (data->tier == MAPTIER_C) {
				Q_strcat(cStr, sizeof(cStr), va("%s%s", numC ? ", " : "", mapShortName));
				numC++;
			}
			else if (data->tier == MAPTIER_F) {
				Q_strcat(fStr, sizeof(fStr), va("%s%s", numF ? ", " : "", mapShortName));
				numF++;
			}
		}
	}

	if (!numS && !numA && !numB && !numC && !numF)
		return qfalse;

	if (printClientNum >= 0) {
		PrintIngame(printClientNum, va("%s^7\n", successPrologueMessage));
		PrintIngame(printClientNum, va("%s^7\n", sStr));
		PrintIngame(printClientNum, va("%s^7\n", aStr));
		PrintIngame(printClientNum, va("%s^7\n", bStr));
		PrintIngame(printClientNum, va("%s^7\n", cStr));
		PrintIngame(printClientNum, va("%s^7\n", fStr));
	}

	return qtrue;
}

mapTier_t G_DBGetTierOfSingleMap(const char *optionalAccountIdsStr, const char *mapFileName, qboolean requireMultipleVotes) {
	if (!VALIDSTRING(mapFileName))
		return MAPTIER_INVALID;

	if (VALIDSTRING(optionalAccountIdsStr) && Q_isanumber(optionalAccountIdsStr) && !strchr(optionalAccountIdsStr, ','))
		requireMultipleVotes = qfalse; // sanity check to help prevent accidentally requiring multiple votes for one person

	tierListInfo_t info = { 0 };
	list_t *list = GetTierList(optionalAccountIdsStr, mapFileName, requireMultipleVotes, 0, NULL, qfalse, &info);
	if (list)
		free(list);

	// instead of parsing the one or zero actual list items, we simply check the info struct
	for (mapTier_t t = MAPTIER_F; t <= MAPTIER_S; t++) {
		if (info.numMapsOfTier[t])
			return t;
	}
	return MAPTIER_INVALID;
}

const char *sqlAddMapTier = "INSERT OR REPLACE INTO tierlistmaps (account_id, map, tier) VALUES (?1, ?2, ?3);";
qboolean G_DBAddMapToTierList(int accountId, const char *mapFileName, mapTier_t tier) {
	if (!VALIDSTRING(mapFileName) || tier <= MAPTIER_INVALID || tier > MAPTIER_S /*|| !G_DBTierlistMapIsWhitelisted(mapFileName)*/) {
		Com_Printf("Unable to set tier %d for map %s for account id %d\n", tier, mapFileName, accountId);
		return qfalse;
	}

	char lowercase[MAX_QPATH] = { 0 };
	Q_strncpyz(lowercase, mapFileName, sizeof(lowercase));
	Q_strlwr(lowercase);

	sqlite3_stmt *statement;
	int rc = sqlite3_prepare(dbPtr, sqlAddMapTier, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, accountId);
	sqlite3_bind_text(statement, 2, lowercase, -1, 0);
	sqlite3_bind_int(statement, 3, (int)tier);

	rc = sqlite3_step(statement);

	sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlRemoveMapTier = "DELETE FROM tierlistmaps WHERE account_id = ?1 AND map = ?2;";
qboolean G_DBRemoveMapFromTierList(int accountId, const char *mapFileName) {
	if (!VALIDSTRING(mapFileName) /*|| !G_DBTierlistMapIsWhitelisted(mapFileName)*/) {
		Com_Printf("Unable to remove map %s for account id %d\n", mapFileName, accountId);
		return qfalse;
	}

	char lowercase[MAX_QPATH] = { 0 };
	Q_strncpyz(lowercase, mapFileName, sizeof(lowercase));
	Q_strlwr(lowercase);

	sqlite3_stmt *statement;
	int rc = sqlite3_prepare(dbPtr, sqlRemoveMapTier, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, accountId);
	sqlite3_bind_text(statement, 2, lowercase, -1, 0);

	rc = sqlite3_step(statement);

	sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlClearTierList = "DELETE FROM tierlistmaps WHERE account_id = ?1;";
qboolean G_DBClearTierList(int accountId) {
	sqlite3_stmt *statement;
	int rc = sqlite3_prepare(dbPtr, sqlClearTierList, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, accountId);

	rc = sqlite3_step(statement);

	sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlClearTierListTier = "DELETE FROM tierlistmaps WHERE account_id = ?1 AND tier = ?2;";
qboolean G_DBClearTierListTier(int accountId, mapTier_t tier) {
	sqlite3_stmt *statement;
	int rc = sqlite3_prepare(dbPtr, sqlClearTierListTier, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, accountId);
	sqlite3_bind_int(statement, 2, tier);

	rc = sqlite3_step(statement);

	sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlGetTiersOfMap = "SELECT accounts.name, tierlistmaps.account_id, tierlistmaps.tier FROM tierlistmaps JOIN accounts ON accounts.account_id = tierlistmaps.account_id WHERE tierlistmaps.map = ?1 ORDER BY accounts.name;";
// returns a list_t of players and their tiers for a single map
// IMPORTANT: free the result if not null!
static list_t *GetTiersOfMap(const char *mapFileName) {
	if (!VALIDSTRING(mapFileName))
		return NULL;
	sqlite3_stmt *statement;
	int rc = sqlite3_prepare(dbPtr, sqlGetTiersOfMap, -1, &statement, 0);

	char lowercase[MAX_QPATH] = { 0 };
	Q_strncpyz(lowercase, mapFileName, sizeof(lowercase));
	Q_strlwr(lowercase);
	sqlite3_bind_text(statement, 1, lowercase, -1, 0);

	list_t *mapsList = malloc(sizeof(list_t));
	memset(mapsList, 0, sizeof(list_t));
	int numGotten = 0;

	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char *accountName = (const char *)sqlite3_column_text(statement, 0);
		int accountId = sqlite3_column_int(statement, 1);
		mapTier_t tier = sqlite3_column_int(statement, 2);
		
		if (VALIDSTRING(accountName)) {
			mapTierData_t *data = (mapTierData_t *)ListAdd(mapsList, sizeof(mapTierData_t));
			Q_strncpyz(data->playerName, accountName, sizeof(data->playerName)); // we copy player name here to save needing to run an account_id/name query later
			data->accountId = accountId;
			data->tier = tier;
			numGotten++;
		}

		rc = sqlite3_step(statement);
	}

	if (!numGotten) {
		free(mapsList);
		return NULL;
	}

	return mapsList;
}

static qboolean PrintAllPlayerRatingsForSingleMap(int printClientNum, list_t *playerRatingList, qboolean useIngamePlayers, const char *successPrologueMessage) {
	if (!playerRatingList)
		return qfalse;

	char *prologueMessageCopied = VALIDSTRING(successPrologueMessage) ? strdup(successPrologueMessage) : NULL;

	char sStr[2048] = { 0 }, aStr[2048] = { 0 }, bStr[2048] = { 0 }, cStr[2048] = { 0 }, fStr[2048] = { 0 };
	Com_sprintf(sStr, sizeof(sStr), "%s: ^7", GetTierStringForTier(MAPTIER_S));
	Com_sprintf(aStr, sizeof(aStr), "%s: ^7", GetTierStringForTier(MAPTIER_A));
	Com_sprintf(bStr, sizeof(bStr), "%s: ^7", GetTierStringForTier(MAPTIER_B));
	Com_sprintf(cStr, sizeof(cStr), "%s: ^7", GetTierStringForTier(MAPTIER_C));
	Com_sprintf(fStr, sizeof(fStr), "%s: ^7", GetTierStringForTier(MAPTIER_F));

	int numS = 0, numA = 0, numB = 0, numC = 0, numF = 0;

	iterator_t iter;
	ListIterate(playerRatingList, &iter, qfalse);
	int numGotten = 0;
	while (IteratorHasNext(&iter)) {
		mapTierData_t *data = (mapTierData_t *)IteratorNext(&iter);
		// see if this player is online, in which case we use a different color
		const char *colorStr = "^7";
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (ent->inuse && ent->client && ent->client->pers.connected != CON_DISCONNECTED && ent->client->account && ent->client->account->id == data->accountId) {
				switch (ent->client->sess.sessionTeam) {
				case TEAM_RED: colorStr = "^1"; break;
				case TEAM_BLUE: colorStr = "^4"; break;
				default: colorStr = "^3"; break;
				}
				break;
			}
		}
		const char *appendName = va("%s%s", colorStr, data->playerName);

		if (data->tier == MAPTIER_S) {
			Q_strcat(sStr, sizeof(sStr), va("%s%s", numS ? "^7, " : "", appendName));
			numS++;
		}
		else if (data->tier == MAPTIER_A) {
			Q_strcat(aStr, sizeof(aStr), va("%s%s", numA ? "^7, " : "", appendName));
			numA++;
		}
		else if (data->tier == MAPTIER_B) {
			Q_strcat(bStr, sizeof(bStr), va("%s%s", numB ? "^7, " : "", appendName));
			numB++;
		}
		else if (data->tier == MAPTIER_C) {
			Q_strcat(cStr, sizeof(cStr), va("%s%s", numC ? "^7, " : "", appendName));
			numC++;
		}
		else if (data->tier == MAPTIER_F) {
			Q_strcat(fStr, sizeof(fStr), va("%s%s", numF ? "^7, " : "", appendName));
			numF++;
		}
	}

	if (!numS && !numA && !numB && !numC && !numF) {
		if (prologueMessageCopied)
			free(prologueMessageCopied);
		return qfalse;
	}

	if (printClientNum >= 0) {
		if (VALIDSTRING(prologueMessageCopied))
			PrintIngame(printClientNum, va("%s^7\n", prologueMessageCopied));
		PrintIngame(printClientNum, va("%s^7\n", sStr));
		PrintIngame(printClientNum, va("%s^7\n", aStr));
		PrintIngame(printClientNum, va("%s^7\n", bStr));
		PrintIngame(printClientNum, va("%s^7\n", cStr));
		PrintIngame(printClientNum, va("%s^7\n", fStr));
	}

	if (prologueMessageCopied)
		free(prologueMessageCopied);
	return qtrue;
}

qboolean G_DBPrintAllPlayerRatingsForSingleMap(const char *mapFileName, int printClientNum, const char *successPrologueMessage) {
	list_t *list = GetTiersOfMap(mapFileName);
	if (list) {
		PrintAllPlayerRatingsForSingleMap(printClientNum, list, qfalse, successPrologueMessage);
		ListClear(list);
		free(list);
		return qtrue;
	}
	else {
		return qfalse;
	}
}

qboolean G_DBPrintPlayerTierList(int accountId, int printClientNum, const char *successPrologueMessage) {
	list_t *list = GetTierList(va("%d", accountId), NULL, qfalse, 0, NULL, qfalse, NULL);
	if (list) {
		PrintMapTierList(printClientNum, list, qfalse, successPrologueMessage);
		ListClear(list);
		free(list);
		return qtrue;
	}
	else {
		return qfalse;
	}
}

const char *sqlMapsNotRatedByPlayer = "SELECT map FROM tierwhitelist WHERE map NOT IN (SELECT map FROM tierlistmaps WHERE account_id = ?1) ORDER BY map;";
// IMPORTANT: free the result if not null!
static list_t *GetMapsNotRatedByPlayerList(int accountId) {
	sqlite3_stmt *statement;
	int rc = sqlite3_prepare(dbPtr, sqlMapsNotRatedByPlayer, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, accountId);
	rc = sqlite3_step(statement);

	list_t *mapsList = malloc(sizeof(list_t));
	memset(mapsList, 0, sizeof(list_t));
	int numGotten = 0;

	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		if (VALIDSTRING(mapFileName)) {
			mapTierData_t *data = (mapTierData_t *)ListAdd(mapsList, sizeof(mapTierData_t));
			Q_strncpyz(data->mapFileName, mapFileName, sizeof(data->mapFileName));
			numGotten++;
		}
		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
	if (!numGotten) {
		ListClear(mapsList);
		return NULL;
	}

	return mapsList;
}

qboolean G_DBPrintPlayerUnratedList(int accountId, int printClientNum, const char *successPrologueMessage) {
	list_t *list = GetMapsNotRatedByPlayerList(accountId);
	if (list) {
		char *prologueMessageCopied = VALIDSTRING(successPrologueMessage) ? strdup(successPrologueMessage) : NULL;
		char mapsStr[4096] = { 0 };
		iterator_t iter;
		ListIterate(list, &iter, qfalse);
		int numAdded = 0;
		while (IteratorHasNext(&iter)) {
			mapTierData_t *data = (mapTierData_t *)IteratorNext(&iter);
			char mapShortName[MAX_QPATH] = { 0 };
			GetShortNameForMapFileName(data->mapFileName, mapShortName, sizeof(mapShortName));
			if (VALIDSTRING(mapShortName)) {
				Q_strcat(mapsStr, sizeof(mapsStr), va("%s%s", numAdded ? ", " : "", mapShortName));
				numAdded++;
			}
		}

		ListClear(list);
		free(list);

		if (numAdded) {
			if (printClientNum >= 0) {
				if (VALIDSTRING(prologueMessageCopied))
					PrintIngame(printClientNum, va("%s^7\n", prologueMessageCopied));
				PrintIngame(printClientNum, va("%s\n", mapsStr));
			}

			if (prologueMessageCopied)
				free(prologueMessageCopied);
			return qtrue;
		}
		else {
			if (prologueMessageCopied)
				free(prologueMessageCopied);
			return qfalse;
		}
	}
	else {
		return qfalse;
	}
}

const char *sqlTierStatsNumRatings = "SELECT COUNT(tierlistmaps.map) FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map;";
const char *sqlTierStatsNumMaps = "SELECT COUNT(*) FROM (SELECT tierlistmaps.map FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map GROUP BY tierlistmaps.map);";
const char *sqlTierStatsNumPlayers = "SELECT COUNT(*) FROM (SELECT accounts.account_id FROM accounts JOIN (SELECT * FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map) m ON accounts.account_id = m.account_id GROUP BY accounts.account_id);";
const char *sqlGetBestMaps = "SELECT tierlistmaps.map, AVG(tier) averageTier FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map GROUP BY tierlistmaps.map ORDER BY averageTier DESC LIMIT 5;";
const char *sqlGetWorstMaps = "SELECT tierlistmaps.map, AVG(tier) averageTier FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map GROUP BY tierlistmaps.map ORDER BY averageTier ASC LIMIT 5;";
const char *sqlGetLowestVariance = "SELECT tierlistmaps.map, AVG(tier*tier) - AVG(tier)*AVG(tier) AS var FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map GROUP BY tierlistmaps.map HAVING COUNT(*) >= 2 ORDER BY var ASC, tierlistmaps.map LIMIT 5;";
const char *sqlGetHighestVariance = "SELECT tierlistmaps.map, AVG(tier*tier) - AVG(tier)*AVG(tier) AS var FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map GROUP BY tierlistmaps.map HAVING COUNT(*) >= 2 ORDER BY var DESC, tierlistmaps.map LIMIT 5;";
const char *sqlGetMostPlayedMaps = "SELECT num_played_view.map, numPlayed FROM num_played_view JOIN tierwhitelist ON num_played_view.map = tierwhitelist.map ORDER BY numPlayed DESC, avgTier DESC, num_played_view.map ASC LIMIT 5;";
const char *sqlGetLeastPlayedMaps = "SELECT num_played_view.map, numPlayed FROM num_played_view JOIN tierwhitelist ON num_played_view.map = tierwhitelist.map ORDER BY numPlayed ASC, avgTier DESC, num_played_view.map ASC LIMIT 5;";
const char *sqlGetMostConformingPlayers = "WITH a AS (SELECT *, abs(round(avgTier, 0) - round(tier, 0)) diff FROM tierlistmaps JOIN num_played_view ON num_played_view.map = tierlistmaps.map), filteredPlayers AS (SELECT account_id FROM tierlistmaps GROUP BY account_id HAVING COUNT(map) >= 10) SELECT name, avg(diff) avgDiff FROM a JOIN accounts ON a.account_id = accounts.account_id JOIN filteredPlayers ON filteredPlayers.account_id = a.account_id GROUP BY a.account_id ORDER BY avgDiff ASC LIMIT 5;";
const char *sqlGetLeastConformingPlayers = "WITH a AS (SELECT *, abs(round(avgTier, 0) - round(tier, 0)) diff FROM tierlistmaps JOIN num_played_view ON num_played_view.map = tierlistmaps.map), filteredPlayers AS (SELECT account_id FROM tierlistmaps GROUP BY account_id HAVING COUNT(map) >= 10) SELECT name, avg(diff) avgDiff FROM a JOIN accounts ON a.account_id = accounts.account_id JOIN filteredPlayers ON filteredPlayers.account_id = a.account_id GROUP BY a.account_id ORDER BY avgDiff DESC LIMIT 5;";
void G_DBTierStats(int clientNum) {
	sqlite3_stmt *statement;
	int rc = sqlite3_prepare(dbPtr, sqlTierStatsNumRatings, -1, &statement, 0);
	rc = sqlite3_step(statement);
	int numRatings = 0;
	if (rc == SQLITE_ROW) {
		numRatings = sqlite3_column_int(statement, 0);
	}

	sqlite3_reset(statement);
	rc = sqlite3_prepare(dbPtr, sqlTierStatsNumMaps, -1, &statement, 0);
	rc = sqlite3_step(statement);
	int numMaps = 0;
	if (rc == SQLITE_ROW) {
		numMaps = sqlite3_column_int(statement, 0);
	}

	sqlite3_reset(statement);
	rc = sqlite3_prepare(dbPtr, sqlTierStatsNumPlayers, -1, &statement, 0);
	rc = sqlite3_step(statement);
	int numPlayers = 0;
	if (rc == SQLITE_ROW) {
		numPlayers = sqlite3_column_int(statement, 0);
	}

	sqlite3_reset(statement);
	rc = sqlite3_prepare(dbPtr, sqlGetBestMaps, -1, &statement, 0);
	rc = sqlite3_step(statement);
	char topMaps[5][MAX_QPATH] = { 0 };
	double topMapAverages[5] = { 0 };
	int numTopMaps = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		if (VALIDSTRING(mapFileName)) {
			GetShortNameForMapFileName(mapFileName, topMaps[numTopMaps], sizeof(topMaps[numTopMaps]));
			topMapAverages[numTopMaps] = average;
			numTopMaps++;
		}
		rc = sqlite3_step(statement);
	}

	sqlite3_reset(statement);
	rc = sqlite3_prepare(dbPtr, sqlGetWorstMaps, -1, &statement, 0);
	rc = sqlite3_step(statement);
	char worstMaps[5][MAX_QPATH] = { 0 };
	double worstMapAverages[5] = { 0 };
	int numWorstMaps = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		if (VALIDSTRING(mapFileName)) {
			GetShortNameForMapFileName(mapFileName, worstMaps[numWorstMaps], sizeof(worstMaps[numWorstMaps]));
			worstMapAverages[numWorstMaps] = average;
			numWorstMaps++;
		}
		rc = sqlite3_step(statement);
	}

	sqlite3_reset(statement);
	rc = sqlite3_prepare(dbPtr, sqlGetLowestVariance, -1, &statement, 0);
	rc = sqlite3_step(statement);
	char leastControversialMaps[5][MAX_QPATH] = { 0 };
	double leastControversialStdevs[5] = { 0 };
	int numLeastControversial = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double variance = sqlite3_column_double(statement, 1);
		if (VALIDSTRING(mapFileName)) {
			GetShortNameForMapFileName(mapFileName, leastControversialMaps[numLeastControversial], sizeof(leastControversialMaps[numLeastControversial]));
			leastControversialStdevs[numLeastControversial] = sqrt(variance);
			numLeastControversial++;
		}
		rc = sqlite3_step(statement);
	}

	sqlite3_reset(statement);
	rc = sqlite3_prepare(dbPtr, sqlGetHighestVariance, -1, &statement, 0);
	rc = sqlite3_step(statement);
	char mostControversialMaps[5][MAX_QPATH] = { 0 };
	double mostControversialStdevs[5] = { 0 };
	int numMostControversial = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double variance = sqlite3_column_double(statement, 1);
		if (VALIDSTRING(mapFileName)) {
			GetShortNameForMapFileName(mapFileName, mostControversialMaps[numMostControversial], sizeof(mostControversialMaps[numMostControversial]));
			mostControversialStdevs[numMostControversial] = sqrt(variance);
			numMostControversial++;
		}
		rc = sqlite3_step(statement);
	}

	sqlite3_reset(statement);
	rc = sqlite3_prepare(dbPtr, sqlGetLeastConformingPlayers, -1, &statement, 0);
	rc = sqlite3_step(statement);
	char leastConformingPlayers[5][MAX_QPATH] = { 0 };
	double leastConformingAverages[5] = { 0 };
	int numLeastConforming = 0;
	while (rc == SQLITE_ROW) {
		const char *playerName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		if (VALIDSTRING(playerName)) {
			Q_strncpyz(leastConformingPlayers[numLeastConforming], playerName, sizeof(leastConformingPlayers[numLeastConforming]));
			leastConformingAverages[numLeastConforming] = average;
			numLeastConforming++;
		}
		rc = sqlite3_step(statement);
	}

	sqlite3_reset(statement);
	rc = sqlite3_prepare(dbPtr, sqlGetMostConformingPlayers, -1, &statement, 0);
	rc = sqlite3_step(statement);
	char mostConformingPlayers[5][MAX_QPATH] = { 0 };
	double mostConformingAverages[5] = { 0 };
	int numMostConforming = 0;
	while (rc == SQLITE_ROW) {
		const char *playerName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		if (VALIDSTRING(playerName)) {
			Q_strncpyz(mostConformingPlayers[numMostConforming], playerName, sizeof(mostConformingPlayers[numMostConforming]));
			mostConformingAverages[numMostConforming] = average;
			numMostConforming++;
		}
		rc = sqlite3_step(statement);
	}

	sqlite3_reset(statement);
	rc = sqlite3_prepare(dbPtr, sqlGetMostPlayedMaps, -1, &statement, 0);
	rc = sqlite3_step(statement);
	char mostPlayedMaps[5][MAX_QPATH] = { 0 };
	int mostPlayedMapCounts[5] = { 0 };
	int numMostPlayedMaps = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		int count = sqlite3_column_int(statement, 1);
		if (VALIDSTRING(mapFileName)) {
			GetShortNameForMapFileName(mapFileName, mostPlayedMaps[numMostPlayedMaps], sizeof(mostPlayedMaps[numMostPlayedMaps]));
			mostPlayedMapCounts[numMostPlayedMaps] = count;
			numMostPlayedMaps++;
		}
		rc = sqlite3_step(statement);
	}

	sqlite3_reset(statement);
	rc = sqlite3_prepare(dbPtr, sqlGetLeastPlayedMaps, -1, &statement, 0);
	rc = sqlite3_step(statement);
	char leastPlayedMaps[5][MAX_QPATH] = { 0 };
	int leastPlayedMapCounts[5] = { 0 };
	int numLeastPlayedMaps = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		int count = sqlite3_column_int(statement, 1);
		if (VALIDSTRING(mapFileName)) {
			GetShortNameForMapFileName(mapFileName, leastPlayedMaps[numLeastPlayedMaps], sizeof(leastPlayedMaps[numLeastPlayedMaps]));
			leastPlayedMapCounts[numLeastPlayedMaps] = count;
			numLeastPlayedMaps++;
		}
		rc = sqlite3_step(statement);
	}

	PrintIngame(clientNum, "There are %d map ratings across %d maps from %d players.\n", numRatings, numMaps, numPlayers);

	if (numTopMaps || numWorstMaps)
		PrintIngame(clientNum, "\nBy average rating (%s^7 = %d; %s^7 = %d):\n", GetTierStringForTier(MAPTIER_S), (int)MAPTIER_S, GetTierStringForTier(MAPTIER_F), (int)MAPTIER_F);
	if (numTopMaps) {
		char topMapsStr[1024] = { 0 };
		for (int i = 0; i < numTopMaps; i++)
			Q_strcat(topMapsStr, sizeof(topMapsStr), va("%s%s%s^7 (%0.2f)", i ? "^7, " : "", GetTierColorForTier(MapTierForDouble(topMapAverages[i])), topMaps[i], topMapAverages[i]));
		PrintIngame(clientNum, "  ^2Top^7 %d maps: %s\n", numTopMaps, topMapsStr);
	}
	if (numWorstMaps) {
		char worstMapsStr[1024] = { 0 };
		for (int i = 0; i < numWorstMaps; i++)
			Q_strcat(worstMapsStr, sizeof(worstMapsStr), va("%s%s%s^7 (%0.2f)", i ? "^7, " : "", GetTierColorForTier(MapTierForDouble(worstMapAverages[i])), worstMaps[i], worstMapAverages[i]));
		PrintIngame(clientNum, "  ^1Bottom^7 %d maps: %s\n", numWorstMaps, worstMapsStr);
	}

	if (numLeastControversial || numMostControversial)
		PrintIngame(clientNum, "\nBy standard deviation:\n");
	if (numLeastControversial) {
		char leastControversialStr[1024] = { 0 };
		for (int i = 0; i < numLeastControversial; i++)
			Q_strcat(leastControversialStr, sizeof(leastControversialStr), va("%s%s (%0.2f)", i ? "^7, " : "", leastControversialMaps[i], leastControversialStdevs[i]));
		PrintIngame(clientNum, "  ^5Least controversial^7 %d maps: %s\n", numLeastControversial, leastControversialStr);
	}
	if (numMostControversial) {
		char mostControversialStr[1024] = { 0 };
		for (int i = 0; i < numMostControversial; i++)
			Q_strcat(mostControversialStr, sizeof(mostControversialStr), va("%s%s (%0.2f)", i ? "^7, " : "", mostControversialMaps[i], mostControversialStdevs[i]));
		PrintIngame(clientNum, "  ^8Most controversial^7 %d maps: %s\n", numMostControversial, mostControversialStr);
	}

	if (numMostPlayedMaps || numLeastPlayedMaps)
		PrintIngame(clientNum, "\n");
	if (numMostPlayedMaps) {
		char mostPlayedStr[1024] = { 0 };
		for (int i = 0; i < numMostPlayedMaps; i++)
			Q_strcat(mostPlayedStr, sizeof(mostPlayedStr), va("%s%s (%d)", i ? "^7, " : "", mostPlayedMaps[i], mostPlayedMapCounts[i]));
		PrintIngame(clientNum, "  ^2Most played^7 %d maps: %s\n", numMostPlayedMaps, mostPlayedStr);
	}
	if (numLeastPlayedMaps) {
		char leastPlayedStr[1024] = { 0 };
		for (int i = 0; i < numLeastPlayedMaps; i++)
			Q_strcat(leastPlayedStr, sizeof(leastPlayedStr), va("%s%s (%d)", i ? "^7, " : "", leastPlayedMaps[i], leastPlayedMapCounts[i]));
		PrintIngame(clientNum, "  ^1Least played^7 %d maps: %s\n", numLeastPlayedMaps, leastPlayedStr);
	}

	if (numLeastConforming || numMostConforming)
		PrintIngame(clientNum, "\nBy average difference from community tier:\n");
	if (numMostConforming) {
		char mostConformingStr[1024] = { 0 };
		for (int i = 0; i < numMostConforming; i++)
			Q_strcat(mostConformingStr, sizeof(mostConformingStr), va("%s%s (%0.2f)", i ? "^7, " : "", mostConformingPlayers[i], mostConformingAverages[i]));
		PrintIngame(clientNum, "  ^5Most conformist^7 %d players: %s\n", numMostConforming, mostConformingStr);
	}
	if (numLeastConforming) {
		char leastConformingStr[1024] = { 0 };
		for (int i = 0; i < numLeastConforming; i++)
			Q_strcat(leastConformingStr, sizeof(leastConformingStr), va("%s%s (%0.2f)", i ? "^7, " : "", leastConformingPlayers[i], leastConformingAverages[i]));
		PrintIngame(clientNum, "  ^8Least conformist^7 %d players: %s\n", numLeastConforming, leastConformingStr);
	}

	sqlite3_finalize(statement);
}

// display people who have rated the most maps first; break ties alphabetically
const char *sqlGetPlayersWhoHaveRatedMaps = "SELECT accounts.name, tierlistmaps.account_id, COUNT(tierlistmaps.account_id) AS num_ratings FROM tierlistmaps JOIN tierwhitelist ON tierwhitelist.map = tierlistmaps.map JOIN accounts ON accounts.account_id = tierlistmaps.account_id GROUP BY tierlistmaps.account_id ORDER BY COUNT(tierlistmaps.account_id) DESC, accounts.name;";
qboolean G_DBTierListPlayersWhoHaveRatedMaps(int clientNum, const char *successPrologueMessage) {
	sqlite3_stmt *statement;
	int rc = sqlite3_prepare(dbPtr, sqlGetPlayersWhoHaveRatedMaps, -1, &statement, 0);
	rc = sqlite3_step(statement);

	char nameList[4096] = { 0 };
	int numFound = 0;
	while (rc == SQLITE_ROW) {
		const char *accountName = (const char *)sqlite3_column_text(statement, 0);
		int accountId = sqlite3_column_int(statement, 1);
		//int numRatings = sqlite3_column_int(statement, 2);
		if (VALIDSTRING(accountName)) {
			// see if this player is online, in which case we use a different color
			const char *colorStr = "^7";
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *ent = &g_entities[i];
				if (ent->inuse && ent->client && ent->client->pers.connected != CON_DISCONNECTED && ent->client->account && ent->client->account->id == accountId) {
					switch (ent->client->sess.sessionTeam) {
					case TEAM_RED: colorStr = "^1"; break;
					case TEAM_BLUE: colorStr = "^4"; break;
					default: colorStr = "^3"; break;
					}
					break;
				}
			}
			const char *appendName = va("%s%s", colorStr, accountName);
			Q_strcat(nameList, sizeof(nameList), va("%s%s", numFound ? "^7, " : "", appendName));
			numFound++;
		}
		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);

	if (numFound) {
		if (VALIDSTRING(successPrologueMessage))
			PrintIngame(clientNum, va("%s\n", successPrologueMessage));
		PrintIngame(clientNum, va("%s^7\n", nameList));
		return qtrue;
	}
	else {
		return qfalse;
	}
}

qboolean G_DBPrintTiersOfAllMaps(int printClientNum, qboolean useIngamePlayers, const char *successPrologueMessage) {
	char ingamePlayersStr[256] = { 0 };
	int numIngame = GetAccountIdsStringOfIngamePlayers(ingamePlayersStr, sizeof(ingamePlayersStr));

	if (useIngamePlayers && numIngame < 2)
		return qfalse;

	char *accountIdsString = NULL;
	if (useIngamePlayers)
		accountIdsString = useIngamePlayers ? ingamePlayersStr : NULL;

	list_t *list = GetTierList(accountIdsString, NULL, useIngamePlayers, 0, NULL, qfalse, NULL);
	if (list) {
		qboolean result = PrintMapTierList(printClientNum, list, useIngamePlayers, successPrologueMessage);
		ListClear(list);
		free(list);
		return result;
	}
	else {
		return qfalse;
	}
}

const char *sqlAddMapToTierWhitelist = "INSERT OR IGNORE INTO tierwhitelist (map) VALUES (?1);";
qboolean G_DBAddMapToTierWhitelist(const char *mapFileName) {
	if (!VALIDSTRING(mapFileName))
		return qfalse;

	sqlite3_stmt *statement;

	sqlite3_prepare(dbPtr, sqlAddMapToTierWhitelist, -1, &statement, 0);

	sqlite3_bind_text(statement, 1, mapFileName, -1, 0);

	int rc = sqlite3_step(statement);

	sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlRemoveMapFromTierWhitelist = "DELETE FROM tierwhitelist WHERE map = ?1;";
qboolean G_DBRemoveMapFromTierWhitelist(const char *mapFileName) {
	if (!VALIDSTRING(mapFileName))
		return qfalse;

	sqlite3_stmt *statement;

	sqlite3_prepare(dbPtr, sqlRemoveMapFromTierWhitelist, -1, &statement, 0);

	sqlite3_bind_text(statement, 1, mapFileName, -1, 0);

	int rc = sqlite3_step(statement);

	sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlGetTierWhitelistedMaps = "SELECT map FROM tierwhitelist ORDER BY map;";
// IMPORTANT: free the result if not null!
list_t *G_DBGetTierWhitelistedMaps(void) {
	sqlite3_stmt *statement;
	int rc = sqlite3_prepare(dbPtr, sqlGetTierWhitelistedMaps, -1, &statement, 0);

	rc = sqlite3_step(statement);

	list_t *mapsList = malloc(sizeof(list_t));
	memset(mapsList, 0, sizeof(list_t));

	int numGotten = 0;

	// loop through each map
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);

		if (VALIDSTRING(mapFileName)) {
			mapTierData_t *data = (mapTierData_t *)ListAdd(mapsList, sizeof(mapTierData_t));
			Q_strncpyz(data->mapFileName, mapFileName, sizeof(data->mapFileName));
			numGotten++;
		}

		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);

	if (!numGotten) {
		ListClear(mapsList);
		return NULL;
	}

	return mapsList;
}

int CvarForTier(mapTier_t tier, qboolean max) {
	switch (tier) {
	case MAPTIER_S: return max ? g_vote_tierlist_s_max.integer : g_vote_tierlist_s_min.integer;
	case MAPTIER_A: return max ? g_vote_tierlist_a_max.integer : g_vote_tierlist_a_min.integer;
	case MAPTIER_B: return max ? g_vote_tierlist_b_max.integer : g_vote_tierlist_b_min.integer;
	case MAPTIER_C: return max ? g_vote_tierlist_c_max.integer : g_vote_tierlist_c_min.integer;
	case MAPTIER_F: return max ? g_vote_tierlist_f_max.integer : g_vote_tierlist_f_min.integer;
	default: assert(qfalse); return 0;
	}
}

// e.g. account ids 13, 20, and 35 are playing ==> "13,20,35"
// returns the number of players detected
int GetAccountIdsStringOfIngamePlayers(char *outBuf, size_t outBufSize) {
	if (!outBuf || !outBufSize)
		return 0;

	char ingamePlayersBuf[256] = { 0 };
	int numGotten = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (ent->inuse && ent->client && ent->client->pers.connected != CON_DISCONNECTED && ent->client->account && (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE)) {
			Q_strcat(ingamePlayersBuf, sizeof(ingamePlayersBuf), va("%s%d", numGotten ? "," : "", ent->client->account->id));
			numGotten++;
		}
	}

	Q_strncpyz(outBuf, ingamePlayersBuf, outBufSize);
	return numGotten;
}

#define MAX_TIERLIST_RNG_TRIES	(8192) // prevent infinite loop

// This function selects maps for the multivote, just like G_DBSelectMapsFromPool does for pools.
// We try to do it using solely ratings from ingame players, but if we can't do that, we fall back to
// using community ratings. We do a bunch of checks at the beginning to make the potentially chaotic/horrible
// distribution of user ratings neatly fit the desired mins/maxes of each tier, to the extent possible.
// Most of the checks and fallbacks here will not really be used once enough people have rated maps, assuming
// those people rated them somewhat reasonably (e.g. every single person didn't rate every single map F tier).
// returns qtrue if successful; qfalse if not
qboolean G_DBSelectTierlistMaps(MapSelectedCallback callback, void *context) {
	// we know there are enough players ingame because we already checked in Svcmd_MapVote_f
	char ingamePlayersStr[256] = { 0 };
	GetAccountIdsStringOfIngamePlayers(ingamePlayersStr, sizeof(ingamePlayersStr));

	// try to get a tier list based on the current ingame players
	tierListInfo_t info;
	list_t *allMapsList = GetTierList(ingamePlayersStr, NULL, qtrue, g_vote_mapCooldownMinutes.integer > 0 ? g_vote_mapCooldownMinutes.integer : 0, level.mapname, qtrue, &info);

	const int totalMapsToChoose = Com_Clampi(2, MAX_MULTIVOTE_MAPS, g_vote_tierlist_totalMaps.integer);
	int numToSelectForEachTier[NUM_MAPTIERS] = { 0 };
	int totalToSelect = 0, tries = 0;

	
	qboolean usingCommunityRatings;
	for (usingCommunityRatings = qfalse; usingCommunityRatings <= qtrue; usingCommunityRatings++) {
		if (usingCommunityRatings) {
			Com_DebugPrintf("Unable to generate a working set of ingame-rated maps; trying again and including community-rated maps.\n");
			if (allMapsList)
				free(allMapsList);
			allMapsList = GetTierList(NULL, NULL, qfalse, g_vote_mapCooldownMinutes.integer > 0 ? g_vote_mapCooldownMinutes.integer : 0, level.mapname, qtrue, &info);
		}
		if (!allMapsList || allMapsList->size < totalMapsToChoose)
			continue;

		// initialize mins/maxes
		int min[NUM_MAPTIERS], max[NUM_MAPTIERS];
		for (mapTier_t t = MAPTIER_S; t >= MAPTIER_F; t--) {
			max[t] = Com_Clampi(0, totalMapsToChoose, CvarForTier(t, qtrue));
			min[t] = Com_Clampi(0, max[t], CvarForTier(t, qfalse));
		}

		// check whether we have enough maps to satisfy the minimum of each tier
		// if not, try to add map(s) of the next tier in the priority list
		for (mapTier_t t = MAPTIER_S; t >= MAPTIER_F; t--) {
			if (min[t] - info.numMapsOfTier[t] <= 0)
				continue;
			Com_DebugPrintf("Not enough maps of tier %s^7; going to try to use more maps of other tier(s).\n", GetTierStringForTier(t));
			mapTier_t checkOrder[4];
			switch (t) {
			case MAPTIER_S: checkOrder[0] = MAPTIER_A; checkOrder[1] = MAPTIER_B; checkOrder[2] = MAPTIER_C; checkOrder[3] = MAPTIER_F; break;
			case MAPTIER_A: checkOrder[0] = MAPTIER_B; checkOrder[1] = MAPTIER_S; checkOrder[2] = MAPTIER_C; checkOrder[3] = MAPTIER_F; break;
			case MAPTIER_B: checkOrder[0] = MAPTIER_C; checkOrder[1] = MAPTIER_A; checkOrder[2] = MAPTIER_F; checkOrder[3] = MAPTIER_S; break;
			case MAPTIER_C: checkOrder[0] = MAPTIER_F; checkOrder[1] = MAPTIER_B; checkOrder[2] = MAPTIER_A; checkOrder[3] = MAPTIER_S; break;
			case MAPTIER_F: checkOrder[0] = MAPTIER_C; checkOrder[1] = MAPTIER_B; checkOrder[2] = MAPTIER_A; checkOrder[3] = MAPTIER_S; break;
			}
			for (int i = 0; i < 4; i++) {
				while (min[t] - info.numMapsOfTier[t] > 0 && info.numMapsOfTier[checkOrder[i]] > max[checkOrder[i]] && max[checkOrder[i]] + 1 < MAX_MULTIVOTE_MAPS) {
					// we found a tier in the priority list that's not using all of its existent maps; allow it to use additional map(s)
					Com_DebugPrintf("Adding to the maximum of %s^7 to compensate.\n", GetTierStringForTier(checkOrder[i]));
					++max[checkOrder[i]];
					--min[t];
				}
				if (min[t] - info.numMapsOfTier[t] <= 0)
					break; // we're good, move on to check the next tier
			}
		}
		
		// we still have one more check to do before we can start randomizing the tiers.
		// we need to check that the sum of the maxes can achieve the total desired number of maps
		int sumOfMaxes = 0;
		for (mapTier_t t = MAPTIER_S; t >= MAPTIER_F; t--) {
			if (max[t] > info.numMapsOfTier[t])
				max[t] = info.numMapsOfTier[t]; // cap max at number of maps before adding it
			if (max[t] > totalMapsToChoose)
				max[t] = totalMapsToChoose; // also cap max at total (saves needless randomizations later)
			sumOfMaxes += max[t];
		}
		if (sumOfMaxes < totalMapsToChoose) {
			// even if we picked every tier at its maximum allowable number of maps, we still wouldn't hit the desired total.
			// starting from S tier and working toward F tier, see if any tier has unused maps, and bump up their maximum if so.
			Com_DebugPrintf("Sum of maxes (%d) < total maps to choose (%d); going to try to bump up tier(s).\n", sumOfMaxes, totalMapsToChoose);
			for (mapTier_t t = MAPTIER_S; t >= MAPTIER_F; t--) {
				while (sumOfMaxes < totalMapsToChoose && info.numMapsOfTier[t] > max[t]) {
					Com_DebugPrintf("Adding to the maximum of %s^7 to compensate.\n", GetTierStringForTier(t));
					++max[t];
					++sumOfMaxes;
				}
			}
		}
		if (sumOfMaxes < totalMapsToChoose)
			continue; // we somehow couldn't fix the max issue

		// if we got here, our mins and maxes are legit. now we try to find
		// a randomized set of numbers of maps that totals to g_vote_tierlist_totalMaps
		// (e.g. 2S, 2A, 1B, 0C, 0F for a total of 5 maps)
		tries = 0;
		do {
			memset(&numToSelectForEachTier, 0, sizeof(numToSelectForEachTier));
			for (mapTier_t t = MAPTIER_S; t >= MAPTIER_F; t--) {
				numToSelectForEachTier[t] = Q_irand(min[t], max[t]);
				if (tries > 2000 || (tries > 1000 && t != MAPTIER_F)) // sanity check, just start fudging the numbers if it takes too long
					numToSelectForEachTier[t] += Q_irand(-1 * (tries / 1000), 1 * (tries / 1000));
				numToSelectForEachTier[t] = Com_Clampi(0, info.numMapsOfTier[t], numToSelectForEachTier[t]); // check bounds
			}
			totalToSelect = 0;
			for (mapTier_t t = MAPTIER_S; t >= MAPTIER_F; t--)
				totalToSelect += numToSelectForEachTier[t];
			tries++;
		} while (totalToSelect != totalMapsToChoose && tries < MAX_TIERLIST_RNG_TRIES);

		if (totalToSelect == totalMapsToChoose)
			break; // we successfully found a working combination; use it
	}

	if (totalToSelect != totalMapsToChoose) {
		// WTF, we were STILL unable to get a good combination??? something has gone terribly wrong
		// perhaps not enough maps were rated by the entire community
		PrintIngame(-1, "Unable to generate a suitable set of maps for voting%s\n", !allMapsList || allMapsList->size < totalMapsToChoose ? ". You can help by rating maps with ^5tier set [map] [tier]^7." : ".");
		G_LogPrintf("Error: unable to generate a suitable set of maps for voting.\n");
		if (allMapsList)
			free(allMapsList);
		return qfalse;
	}

	// if we got here, we managed to not fuck it up, and we have a working set of numbers for each tier
	// time to start picking maps
	Com_DebugPrintf("Selecting %d S tier maps, %d A tier maps, %d B tier maps, %d C tier maps, and %d F tier maps (took %d randomizations).\n",
		numToSelectForEachTier[MAPTIER_S], numToSelectForEachTier[MAPTIER_A], numToSelectForEachTier[MAPTIER_B],
		numToSelectForEachTier[MAPTIER_C], numToSelectForEachTier[MAPTIER_F], tries);
	if (usingCommunityRatings)
		PrintIngame(-1, "Not enough maps rated by ingame players; using community-rated maps.\n");

	// loop through the tiers, picking the number of maps we determined for each one above
	int numMapsPickedTotal = 0;
	char chosenMapNames[MAX_MULTIVOTE_MAPS][MAX_QPATH] = { 0 }; // suck my dick, double pointer shit is aids
	for (mapTier_t t = MAPTIER_S; t >= MAPTIER_F; t--) {
		if (numMapsPickedTotal >= totalMapsToChoose)
			break; // should never happen

		// loop through the randomized map list until we find a map matching the current tier
		int numPickedForThisTier = 0;
		while (numPickedForThisTier < numToSelectForEachTier[t]) {
			iterator_t iter;
			ListIterate(allMapsList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				mapTierData_t *data = (mapTierData_t *)IteratorNext(&iter);
				if (data->tier != t)
					continue;

				// we found a map that matches the current tier; pick it
				if (MapExistsQuick(data->mapFileName)) { // one last double check to make sure the map actually exists
					char mapShortName[MAX_QPATH] = { 0 };
					GetShortNameForMapFileName(data->mapFileName, mapShortName, sizeof(mapShortName));
					Com_DebugPrintf("Selecting %s (%s^7)\n", mapShortName, GetTierStringForTier(t));
					Q_strncpyz(chosenMapNames[numMapsPickedTotal], data->mapFileName, sizeof(chosenMapNames[numMapsPickedTotal]));

					ListRemove(allMapsList, data);
					numPickedForThisTier++;
					numMapsPickedTotal++;

					goto continueOuterLoop; // suck my enormous nutsack
				}
			}
			continueOuterLoop:; // you bitch
		}
	}
	
	// fisher-yates shuffle so that the maps don't appear in order of their tiers,
	// preventing people from simply being biased toward the maps appearing at the top
	for (int i = numMapsPickedTotal - 1; i >= 1; i--) {
		int j = rand() % (i + 1);
		char *temp = strdup(chosenMapNames[i]);
		Q_strncpyz(chosenMapNames[i], chosenMapNames[j], sizeof(chosenMapNames[i]));
		Q_strncpyz(chosenMapNames[j], temp, sizeof(chosenMapNames[j]));
		free(temp);
	}

	// run the callbacks
	for (int i = 0; i < numMapsPickedTotal; i++) {
		if (chosenMapNames[i][0])
			callback(context, chosenMapNames[i]);
	}

	free(allMapsList);

	return !!(numMapsPickedTotal > 0);
}

const char *sqlAddMapToPlayedMapsList = "INSERT OR REPLACE INTO lastplayedmap (map, num) VALUES (?1, (SELECT num FROM lastplayedmap WHERE map = ?1) + 1);";
qboolean G_DBAddCurrentMapToPlayedMapsList(void) {
	if (!level.mapname[0])
		return qfalse;

	char lowercase[MAX_QPATH] = { 0 };
	Q_strncpyz(lowercase, level.mapname, sizeof(lowercase));
	Q_strlwr(lowercase);

	sqlite3_stmt *statement;

	sqlite3_prepare(dbPtr, sqlAddMapToPlayedMapsList, -1, &statement, 0);

	sqlite3_bind_text(statement, 1, lowercase, -1, 0);

	int rc = sqlite3_step(statement);

	sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}