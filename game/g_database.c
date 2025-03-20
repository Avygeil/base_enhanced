#include "g_database.h"
#include "sqlite3.h"
#include "time.h"

#include "cJSON.h"

#include "g_database_schema.h"

static sqlite3* dbPtr = NULL;

static qboolean didUpgrade = qfalse;

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

void G_DBLoadDatabase( void *serverDbPtr )
{
	if (!serverDbPtr) {
		Com_Error(ERR_DROP, "Null db pointer from server");
	}

	dbPtr = serverDbPtr;

	// register trace callback if needed
	if ( g_traceSQL.integer ) {
#if 1
		Com_Error(ERR_FATAL, "tracesql needs to use trap calls for sqlite3_trace_v2, sqlite3_expanded_sql, and sqlite3_free (not yet implemented)");
#else
		sqlite3_trace_v2( dbPtr, SQLITE_TRACE_STMT | SQLITE_TRACE_PROFILE, TraceCallback, NULL );
#endif
	}

	// more db options
	trap_sqlite3_exec( dbPtr, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL );
	trap_sqlite3_exec( dbPtr, "PRAGMA automatic_index = OFF;", NULL, NULL, NULL );

	// get version and call the upgrade routine

	char s[16];
	G_DBGetMetadata( "schema_version", s, sizeof( s ) );
	int version = VALIDSTRING( s ) ? atoi( s ) : 0;

	// setup tables if database was just created
	// NOTE: this means g_database_schema.h must always reflect the latest version
	if (!version) {
		trap_sqlite3_exec(dbPtr, sqlCreateTables, 0, 0, 0);
		version = DB_SCHEMA_VERSION;
	}

	if ( !G_DBUpgradeDatabaseSchema( version, dbPtr, &didUpgrade ) ) {
		// don't let server load if an upgrade failed

		/*if (dbPtr != diskDb) {
			// close in memory db immediately
			sqlite3_close(dbPtr);
			dbPtr = NULL;
		}*/

		Com_Error(ERR_DROP, "Failed to upgrade database, shutting down to avoid data corruption\n");
	}

	G_DBSetMetadata( "schema_version", DB_SCHEMA_VERSION_STR );

	if (!g_notFirstMap.integer)
		G_DBDeleteMetadataStartingWith("remindpos_account_");

	// optimize the db if needed

	G_DBGetMetadata( "last_optimize", s, sizeof( s ) );

	const time_t currentTime = time( NULL );
	time_t last_optimize = VALIDSTRING( s ) ? strtoll( s, NULL, 10 ) : 0;

	if ( last_optimize + DB_OPTIMIZE_INTERVAL < currentTime ) {
		Com_Printf( "Automatically optimizing database...\n" );

		trap_sqlite3_exec( dbPtr, "PRAGMA optimize;", NULL, NULL, NULL );

		G_DBSetMetadata( "last_optimize", va( "%lld", currentTime ) );
	}

	// if the server is empty, vacuum the db if needed

	if ( !level.numConnectedClients ) {
		G_DBGetMetadata( "last_vacuum", s, sizeof( s ) );

		time_t last_autoclean = VALIDSTRING( s ) ? strtoll( s, NULL, 10 ) : 0;

		if ( last_autoclean + DB_VACUUM_INTERVAL < currentTime ) {
			Com_Printf( "Automatically running vacuum on database...\n" );

			trap_sqlite3_exec( dbPtr, "VACUUM;", NULL, NULL, NULL );

			G_DBSetMetadata( "last_vacuum", va( "%lld", currentTime ) );
		}
	}	
}

void G_DBUnloadDatabase( void )
{

}

// =========== METADATA ========================================================

const char* const sqlGetMetadata =
"SELECT value FROM metadata WHERE metadata.key = ?1 LIMIT 1;                   ";

const char* const sqlSetMetadata =
"INSERT OR REPLACE INTO metadata (key, value) VALUES( ?1, ?2 );                ";

const char *const sqlDeleteMetadata =
"DELETE FROM metadata WHERE key = ?1;                ";

const char *const sqlDeleteMetadataStartingWith =
"DELETE FROM metadata WHERE key LIKE ?1 || '%';";

void G_DBGetMetadata( const char *key,
	char *outValue,
	size_t outValueBufSize )
{
	assert(VALIDSTRING(key) && outValue && outValueBufSize > 0);
	sqlite3_stmt* statement;

	outValue[0] = '\0';

	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlGetMetadata, -1, &statement, 0);

	sqlite3_bind_text( statement, 1, key, -1, SQLITE_STATIC );

	rc = trap_sqlite3_step( statement );
	while ( rc == SQLITE_ROW ) {
		const char *value = ( const char* )sqlite3_column_text( statement, 0 );
		Q_strncpyz( outValue, value, outValueBufSize );

		rc = trap_sqlite3_step( statement );
	}

	trap_sqlite3_finalize( statement );
}

// returns 0 if nonexistent
int G_DBGetMetadataInteger(const char *key) {
	assert(VALIDSTRING(key));
	char s[MAX_STRING_CHARS] = { 0 };

	G_DBGetMetadata(key, s, sizeof(s));

	if (!s[0])
		return 0;

	return atoi(s);
}

void G_DBSetMetadata( const char *key,
	const char *value )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlSetMetadata, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, key, -1, SQLITE_STATIC );
	sqlite3_bind_text( statement, 2, value, -1, SQLITE_STATIC );

	trap_sqlite3_step( statement );

	trap_sqlite3_finalize( statement );
}

void G_DBDeleteMetadata(const char *key)
{
	sqlite3_stmt *statement;

	trap_sqlite3_prepare_v2(dbPtr, sqlDeleteMetadata, -1, &statement, 0);

	sqlite3_bind_text(statement, 1, key, -1, SQLITE_STATIC);

	trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);
}

void G_DBDeleteMetadataStartingWith(const char *key)
{
	sqlite3_stmt *statement;

	trap_sqlite3_prepare_v2(dbPtr, sqlDeleteMetadataStartingWith, -1, &statement, 0);

	sqlite3_bind_text(statement, 1, key, -1, SQLITE_STATIC);

	trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);
}

// =========== ACCOUNTS ========================================================

static const char* sqlGetAccountByID =
"SELECT name, created_on, properties, flags, bannedPos                       \n"
"FROM accounts                                                               \n"
"WHERE accounts.account_id = ?1;                                               ";

static const char* sqlGetAccountByName =
"SELECT account_id, name, created_on, properties, flags, bannedPos           \n"
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

static const char *sqlSetPropertiesForAccountId =
"UPDATE accounts                                                             \n"
"SET properties = ?1, bannedPos = ?2                                         \n"
"WHERE accounts.account_id = ?3;                                               ";

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

// note: does not read bannedPos from db
static void ReadAccountProperties(const char *propertiesIn, account_t *accOut, const int bannedPos) {
	cJSON *root = VALIDSTRING(propertiesIn) ? cJSON_Parse(propertiesIn) : NULL;
	positionPreferences_t positionPreferences = { 0 };
	if (root) {
		qboolean gotSexOrGuidAutoLink = qfalse;

		cJSON *autolink_sex = cJSON_GetObjectItemCaseSensitive(root, "autolink_sex");
		if (cJSON_IsString(autolink_sex) && VALIDSTRING(autolink_sex->valuestring)) {
			Q_strncpyz(accOut->autoLink.sex, autolink_sex->valuestring, sizeof(accOut->autoLink.sex));
			gotSexOrGuidAutoLink = qtrue;
		}
		else {
			accOut->autoLink.sex[0] = '\0';
		}

		cJSON *autolink_guid = cJSON_GetObjectItemCaseSensitive(root, "autolink_guid");
		if (cJSON_IsString(autolink_guid) && VALIDSTRING(autolink_guid->valuestring)) {
			Q_strncpyz(accOut->autoLink.guid, autolink_guid->valuestring, sizeof(accOut->autoLink.guid));
			gotSexOrGuidAutoLink = qtrue;
		}
		else {
			accOut->autoLink.guid[0] = '\0';
		}

		if (gotSexOrGuidAutoLink) {
			cJSON *autolink_country = cJSON_GetObjectItemCaseSensitive(root, "autolink_country");
			if (cJSON_IsString(autolink_country) && VALIDSTRING(autolink_country->valuestring))
				Q_strncpyz(accOut->autoLink.country, autolink_country->valuestring, sizeof(accOut->autoLink.country));
			else
				accOut->autoLink.country[0] = '\0';
		}
		else {
			accOut->autoLink.sex[0] = accOut->autoLink.guid[0] = accOut->autoLink.country[0] = '\0';
		}

		{
			cJSON *pospref_first = cJSON_GetObjectItemCaseSensitive(root, "pospref_first");
			if (cJSON_IsNumber(pospref_first))
				positionPreferences.first = pospref_first->valueint & ALL_CTF_POSITIONS;
		}

		{
			cJSON *pospref_second = cJSON_GetObjectItemCaseSensitive(root, "pospref_second");
			if (cJSON_IsNumber(pospref_second))
				positionPreferences.second = pospref_second->valueint & ALL_CTF_POSITIONS;
		}

		{
			cJSON *pospref_third = cJSON_GetObjectItemCaseSensitive(root, "pospref_third");
			if (cJSON_IsNumber(pospref_third))
				positionPreferences.third = pospref_third->valueint & ALL_CTF_POSITIONS;
		}

		{
			cJSON *pospref_avoid = cJSON_GetObjectItemCaseSensitive(root, "pospref_avoid");
			if (cJSON_IsNumber(pospref_avoid))
				positionPreferences.avoid = pospref_avoid->valueint & ALL_CTF_POSITIONS;
		}
	}
	else {
		accOut->autoLink.sex[0] = accOut->autoLink.country[0] = '\0';
	}
	memcpy(&accOut->expressedPref, &positionPreferences, sizeof(positionPreferences_t));
	ValidateAndCopyPositionPreferences(&positionPreferences, &accOut->validPref, bannedPos);
	cJSON_Delete(root);
}

qboolean G_DBGetAccountByID( const int id,
	account_t* account )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlGetAccountByID, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, id );

	qboolean found = qfalse;
	int rc = trap_sqlite3_step( statement );

	if ( rc == SQLITE_ROW ) {
		const char* name = ( const char* )sqlite3_column_text( statement, 0 );
		const int created_on = sqlite3_column_int( statement, 1 );
		const int64_t flags = sqlite3_column_int64( statement, 3 );
		const int64_t bannedPos = sqlite3_column_int64( statement, 4 );

		account->id = id;
		Q_strncpyz( account->name, name, sizeof( account->name ) );
		account->creationDate = created_on;
		account->flags = flags;
		account->bannedPos = bannedPos;

		if ( sqlite3_column_type( statement, 2 ) != SQLITE_NULL ) {
			const char* properties = ( const char* )sqlite3_column_text( statement, 2 );
			ReadAccountProperties(properties, account, bannedPos);
		} else {
			account->autoLink.sex[0] = account->autoLink.country[0] = '\0';
			memset(&account->expressedPref, 0, sizeof(positionPreferences_t));
			memset(&account->validPref, 0, sizeof(positionPreferences_t));
		}

		found = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return found;
}

qboolean G_DBGetAccountByName( const char* name,
	account_t* account )
{
	if (!VALIDSTRING(name))
		return qfalse;

	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlGetAccountByName, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, name, -1, 0 );

	qboolean found = qfalse;
	int rc = trap_sqlite3_step( statement );

	if ( rc == SQLITE_ROW ) {
		const int account_id = sqlite3_column_int( statement, 0 );
		const char* name = ( const char* )sqlite3_column_text( statement, 1 );
		const int created_on = sqlite3_column_int( statement, 2 );
		const int64_t flags = sqlite3_column_int64( statement, 4 );
		const int64_t bannedPos = sqlite3_column_int64( statement, 5 );

		account->id = account_id;
		Q_strncpyz( account->name, name, sizeof( account->name ) );
		account->creationDate = created_on;
		account->flags = flags;
		account->bannedPos = bannedPos;

		if (sqlite3_column_type(statement, 3) != SQLITE_NULL) {
			const char *properties = (const char *)sqlite3_column_text(statement, 3);
			ReadAccountProperties(properties, account, bannedPos);
		}
		else {
			account->autoLink.sex[0] = account->autoLink.country[0] = '\0';
			memset(&account->expressedPref, 0, sizeof(positionPreferences_t));
			memset(&account->validPref, 0, sizeof(positionPreferences_t));
		}

		found = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return found;
}

void G_DBCreateAccount( const char* name )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlCreateAccount, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, name, -1, 0 );

	trap_sqlite3_step( statement );

	trap_sqlite3_finalize( statement );
}

qboolean G_DBDeleteAccount( account_t* account )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlDeleteAccount, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, account->id );

	trap_sqlite3_step( statement );

	qboolean success = sqlite3_changes( dbPtr ) != 0;

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBGetSessionByID( const int id,
	session_t* session )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlGetSessionByID, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, id );

	qboolean found = qfalse;
	int rc = trap_sqlite3_step( statement );

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

	trap_sqlite3_finalize( statement );

	return found;
}

qboolean G_DBGetSessionByHash( const sessionInfoHash_t hash,
	session_t* session )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlGetSessionByHash, -1, &statement, 0 );

	sqlite3_bind_int64( statement, 1, hash );

	qboolean found = qfalse;
	int rc = trap_sqlite3_step( statement );

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

	trap_sqlite3_finalize( statement );

	return found;
}

void G_DBCreateSession( const sessionInfoHash_t hash,
	const char* info )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlCreateSession, -1, &statement, 0 );

	sqlite3_bind_int64( statement, 1, hash );
	sqlite3_bind_text( statement, 2, info, -1, 0 );

	trap_sqlite3_step( statement );

	trap_sqlite3_finalize( statement );
}

void G_DBLinkAccountToSession( session_t* session,
	account_t* account )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlLinkAccountToSession, -1, &statement, 0 );

	if ( account ) {
		sqlite3_bind_int( statement, 1, account->id );
	} else {
		sqlite3_bind_null( statement, 1 );
	}
	
	sqlite3_bind_int( statement, 2, session->id );

	trap_sqlite3_step( statement );

	// link in the struct too if successful
	if ( sqlite3_changes( dbPtr ) != 0 ) {
		session->accountId = account ? account->id : ACCOUNT_ID_UNLINKED;
	}

	trap_sqlite3_finalize( statement );
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

	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlListSessionIdsForAccount, -1, &statement, 0 );

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int( statement, 1, account->id );
	sqlite3_bind_int(statement, 2, limit);
	sqlite3_bind_int(statement, 3, offset);

	rc = trap_sqlite3_step( statement );
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

		rc = trap_sqlite3_step( statement );
	}

	trap_sqlite3_finalize( statement );
}

void G_DBListSessionsForInfo( const char* key,
	const char* value,
	pagination_t pagination,
	DBListSessionsCallback callback,
	void* ctx )
{
	sqlite3_stmt* statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListSessionIdsForInfo, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_text(statement, 1, va( "$.%s", key ), -1, SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, value, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 3, limit);
	sqlite3_bind_int(statement, 4, offset);

	rc = trap_sqlite3_step(statement);
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

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
}

void G_DBSetAccountFlags( account_t* account,
	const int64_t flags )
{
	sqlite3_stmt* statement;

	trap_sqlite3_prepare_v2( dbPtr, sqlSetFlagsForAccountId, -1, &statement, 0 );

	sqlite3_bind_int64( statement, 1, flags );
	sqlite3_bind_int( statement, 2, account->id );

	trap_sqlite3_step( statement );

	// link in the struct too if successful
	if ( sqlite3_changes( dbPtr ) != 0 ) {
		account->flags = flags;
	}

	trap_sqlite3_finalize( statement );
}

// also sets bannedPos
void G_DBSetAccountProperties(account_t *account)
{
	cJSON *root = cJSON_CreateObject();
	char *str = NULL;
	if (root) {
		qboolean print = qfalse;

		qboolean gotSexOrGuidAutoLink = qfalse;
		if (account->autoLink.sex[0]) {
			cJSON_AddStringToObject(root, "autolink_sex", account->autoLink.sex);
			print = qtrue;
			gotSexOrGuidAutoLink = qtrue;
		}

		if (account->autoLink.guid[0]) {
			cJSON_AddStringToObject(root, "autolink_guid", account->autoLink.guid);
			print = qtrue;
			gotSexOrGuidAutoLink = qtrue;
		}

		if (gotSexOrGuidAutoLink) {
			if (account->autoLink.country[0])
				cJSON_AddStringToObject(root, "autolink_country", account->autoLink.country);
		}

		if (account->expressedPref.first) {
			cJSON_AddNumberToObject(root, "pospref_first", account->expressedPref.first);
			print = qtrue;
		}

		if (account->expressedPref.second) {
			cJSON_AddNumberToObject(root, "pospref_second", account->expressedPref.second);
			print = qtrue;
		}

		if (account->expressedPref.third) {
			cJSON_AddNumberToObject(root, "pospref_third", account->expressedPref.third);
			print = qtrue;
		}

		if (account->expressedPref.avoid) {
			cJSON_AddNumberToObject(root, "pospref_avoid", account->expressedPref.avoid);
			print = qtrue;
		}

		if (print)
			str = cJSON_PrintUnformatted(root);
	}

	sqlite3_stmt *statement;

	trap_sqlite3_prepare_v2(dbPtr, sqlSetPropertiesForAccountId, -1, &statement, 0);

	if (VALIDSTRING(str))
		sqlite3_bind_text(statement, 1, str, -1, 0);
	else
		sqlite3_bind_null(statement, 1);
	sqlite3_bind_int(statement, 2, account->bannedPos);
	sqlite3_bind_int(statement, 3, account->id);

	trap_sqlite3_step(statement);

	cJSON_Delete(root);
	trap_sqlite3_finalize(statement);
}

const char *const sqlGetAutoLinks = "SELECT account_id, properties FROM accounts WHERE properties IS NOT NULL;";
void G_DBCacheAutoLinks(void) {
	ListClear(&level.autoLinksList);
	sqlite3_stmt *statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetAutoLinks, -1, &statement, 0);

	rc = trap_sqlite3_step(statement);
	int gotten = 0;
	while (rc == SQLITE_ROW) {
		const int accountId = sqlite3_column_int(statement, 0);
		const char *properties = (const char *)sqlite3_column_text(statement, 1);
		account_t acc = { 0 };
		ReadAccountProperties(properties, &acc, 0);

		if (acc.autoLink.sex[0] || acc.autoLink.guid[0]) {
			++gotten;
			autoLink_t *add = ListAdd(&level.autoLinksList, sizeof(autoLink_t));
			add->accountId = accountId;
			if (acc.autoLink.sex[0])
				Q_strncpyz(add->sex, acc.autoLink.sex, sizeof(add->sex));
			if (acc.autoLink.guid[0])
				Q_strncpyz(add->guid, acc.autoLink.guid, sizeof(add->guid));
			if (acc.autoLink.country[0])
				Q_strncpyz(add->country, acc.autoLink.country, sizeof(add->country));
		}

		rc = trap_sqlite3_step(statement);
	}
	
	trap_sqlite3_finalize(statement);
	Com_Printf("Loaded %d autolinks from database.\n", gotten);
}

int G_DBGetAccountPlaytime( account_t* account )
{
	sqlite3_stmt* statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetPlaytimeForAccountId, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, account->id);

	int result = 0;

	rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const int playtime = sqlite3_column_int(statement, 0);

		result = playtime;

		rc = trap_sqlite3_step(statement);
	} else if (rc != SQLITE_DONE) {
		result = -1;
	}

	trap_sqlite3_finalize(statement);

	return result;
}

void G_DBListTopUnassignedSessionIDs(pagination_t pagination,
	DBListUnassignedSessionIDsCallback callback,
	void* ctx)
{
	sqlite3_stmt* statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListTopUnassignedSessionIds, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int(statement, 1, limit);
	sqlite3_bind_int(statement, 2, offset);

	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const int session_id = sqlite3_column_int(statement, 0);
		const char* alias = (const char*)sqlite3_column_text(statement, 1);
		const int playtime = sqlite3_column_int(statement, 2);
		const qboolean referenced = !!sqlite3_column_int(statement, 3);

		callback(ctx, session_id, alias, playtime, referenced);

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
}

void G_DBPurgeUnassignedSessions(void)
{
	trap_sqlite3_exec(dbPtr, sqlPurgeUnreferencedSessions, NULL, NULL, NULL);
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
	trap_sqlite3_prepare_v2(dbPtr, sqlLogNickname1, -1, &statement1, 0);
	trap_sqlite3_prepare_v2(dbPtr, sqlLogNickname2, -1, &statement2, 0);

	sqlite3_bind_int(statement1, 1, sessionId);
	sqlite3_bind_text(statement1, 2, name, -1, 0);
	trap_sqlite3_step(statement1);

	sqlite3_bind_int(statement2, 1, duration);
	sqlite3_bind_int(statement2, 2, sessionId);
	sqlite3_bind_text(statement2, 3, name, -1, 0);
	trap_sqlite3_step(statement2);

	trap_sqlite3_finalize(statement1);
	trap_sqlite3_finalize(statement2);
}

void G_DBGetMostUsedNicknames(const int sessionId,
	const int numNicknames,
	nicknameEntry_t* outNicknames)
{
	sqlite3_stmt* statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetTopNicknames, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, sessionId);
	sqlite3_bind_int(statement, 2, numNicknames);

	int i = 0;
	memset(outNicknames, 0, sizeof(*outNicknames) * numNicknames);

	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW && i < numNicknames) {
		const char* name = (const char*)sqlite3_column_text(statement, 0);
		const int duration = sqlite3_column_int(statement, 1);

		nicknameEntry_t* nickname = outNicknames + i++;

		Q_strncpyz(nickname->name, name, sizeof(nickname->name));
		nickname->duration = duration;

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
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
const char *const sqlSaveFastcapRecordExistingCheck =
"SELECT capture_time FROM fastcaps WHERE mapname = ? AND type = ? AND session_id = ?;";
const char* const sqlSaveFastcapRecord2 =
"UPDATE fastcaps SET capture_time = ?1, date = ?2, extra = ?3                          \n"
"WHERE mapname = ?4 AND type = ?5 AND session_id = ?6;                                   ";
const char *const sqlSaveFastcapRecord2Min =
"UPDATE fastcaps SET capture_time = MIN(?1, capture_time), date = ?2, extra = ?3                          \n"
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

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetFastcapRecord, -1, &statement, 0);

	qboolean error = qfalse;

	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 2, type);
	sqlite3_bind_int(statement, 3, sessionId);

	memset(outRecord, 0, sizeof(*outRecord));

	rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const int capture_time = sqlite3_column_int(statement, 0);
		const time_t date = sqlite3_column_int64(statement, 1);
		const char* extra = (const char*)sqlite3_column_text(statement, 2);
		
		outRecord->time = capture_time;
		outRecord->date = date;
		ReadExtraRaceInfo(extra, &outRecord->extra);

		rc = trap_sqlite3_step(statement);
	} else if (rc != SQLITE_DONE) {
		error = qtrue;
	}

	trap_sqlite3_finalize(statement);

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

	trap_sqlite3_prepare_v2(dbPtr, sqlSaveFastcapRecord1, -1, &statement1, 0);

	qboolean error = qfalse;

	sqlite3_bind_text(statement1, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement1, 2, type);
	sqlite3_bind_int(statement1, 3, sessionId);
	trap_sqlite3_step(statement1);

	char* extra;
	WriteExtraRaceInfo(&inRecord->extra, &extra);

	// force saving in lowercase
	char* mapnameLowercase = strdup(mapname);
	Q_strlwr(mapnameLowercase);

	sqlite3_stmt *checkStatement;
	trap_sqlite3_prepare_v2(dbPtr, sqlSaveFastcapRecordExistingCheck, -1, &checkStatement, 0);
	sqlite3_bind_text(checkStatement, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(checkStatement, 2, type);
	sqlite3_bind_int(checkStatement, 3, sessionId);
	int checkResult = trap_sqlite3_step(checkStatement);
	int existing = 0;
	if (checkResult == SQLITE_ROW)
		existing = sqlite3_column_int(checkStatement, 0);

	trap_sqlite3_prepare_v2(dbPtr, existing > 0 ? sqlSaveFastcapRecord2Min : sqlSaveFastcapRecord2, -1, &statement2, 0);
	sqlite3_bind_int(statement2, 1, inRecord->time);
	sqlite3_bind_int(statement2, 2, inRecord->date);
	sqlite3_bind_text(statement2, 3, extra, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement2, 4, mapnameLowercase, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement2, 5, type);
	sqlite3_bind_int(statement2, 6, sessionId);
	int rc = trap_sqlite3_step(statement2);

	error = rc != SQLITE_DONE;

	free(extra);
	free(mapnameLowercase);

	trap_sqlite3_finalize(statement1);
	trap_sqlite3_finalize(checkStatement);
	trap_sqlite3_finalize(statement2);

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

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetFastcapLoggedPersonalBest, -1, &statement, 0);

	qboolean errored = qfalse;

	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 2, type);
	sqlite3_bind_int(statement, 3, accountId);

	if (outRank)
		*outRank = 0;
	if (outTime)
		*outTime = 0;

	rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const int rank = sqlite3_column_int(statement, 0);
		const int capture_time = sqlite3_column_int(statement, 1);

		if (outRank)
			*outRank = rank;
		if (outTime)
			*outTime = capture_time;

		rc = trap_sqlite3_step(statement);
	} else if (rc != SQLITE_DONE) {
		if (outRank)
			*outRank = -1;
		if (outTime)
			*outTime = -1;

		errored = qtrue;
	}

	trap_sqlite3_finalize(statement);

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

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListFastcapsLoggedRecords, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 2, type);
	sqlite3_bind_int(statement, 3, limit);
	sqlite3_bind_int(statement, 4, offset);

	rc = trap_sqlite3_step(statement);
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

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
}

// lists only the best records for a type sorted by mapname
void G_DBListRaceTop(const raceType_t type,
	const pagination_t pagination,
	ListRaceTopCallback callback,
	void* context)
{
	sqlite3_stmt* statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListFastcapsLoggedTop, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int(statement, 1, type);
	sqlite3_bind_int(statement, 2, limit);
	sqlite3_bind_int(statement, 3, offset);

	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char* mapname = (const char*)sqlite3_column_text(statement, 0);
		const char* name = (const char*)sqlite3_column_text(statement, 1);
		const int capture_time = sqlite3_column_int(statement, 2);
		const time_t date = sqlite3_column_int64(statement, 3);

		callback(context, mapname, type, name, capture_time, date);

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
}

// lists logged in leaderboard sorted by golds, silvers, bronzes
void G_DBListRaceLeaderboard(const raceType_t type,
	const pagination_t pagination,
	ListRaceLeaderboardCallback callback,
	void* context)
{
	sqlite3_stmt* statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListFastcapsLoggedLeaderboard, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int(statement, 1, type);
	sqlite3_bind_int(statement, 2, limit);
	sqlite3_bind_int(statement, 3, offset);

	int rank = 1 + offset;

	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char* name = (const char*)sqlite3_column_text(statement, 0);
		const int golds = sqlite3_column_int(statement, 1);
		const int silvers = sqlite3_column_int(statement, 2);
		const int bronzes = sqlite3_column_int(statement, 3);

		callback(context, type, rank++, name, golds, silvers, bronzes);

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
}

// lists logged in latest records across all maps/types
void G_DBListRaceLatest(const pagination_t pagination,
	ListRaceLatestCallback callback,
	void* context)
{
	sqlite3_stmt* statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListFastcapsLoggedLatest, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int(statement, 1, limit);
	sqlite3_bind_int(statement, 2, offset);

	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char* mapname = (const char*)sqlite3_column_text(statement, 0);
		const int type = sqlite3_column_int(statement, 1);
		const char* name = (const char*)sqlite3_column_text(statement, 2);
		const int rank = sqlite3_column_int(statement, 3);
		const int capture_time = sqlite3_column_int(statement, 4);
		const time_t date = sqlite3_column_int64(statement, 5);

		callback(context, mapname, type, rank, name, capture_time, date);

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
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
	trap_sqlite3_prepare_v2( dbPtr, sqlIsIpWhitelisted, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );

	trap_sqlite3_step( statement );
	int count = sqlite3_column_int( statement, 0 );

	if ( count == 0 )
	{
		Q_strncpyz( reasonBuffer, "IP address not on whitelist", reasonBufferSize );
		filtered = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return filtered;
}

qboolean G_DBAddToWhitelist( unsigned int ip,
	unsigned int mask,
	const char* notes )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlAddToWhitelist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	sqlite3_bind_text( statement, 3, notes, -1, 0 );

	rc = trap_sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBRemoveFromWhitelist( unsigned int ip,
	unsigned int mask )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlremoveFromWhitelist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	rc = trap_sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	trap_sqlite3_finalize( statement );

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
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlIsIpBlacklisted, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );

	rc = trap_sqlite3_step( statement );

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

	trap_sqlite3_finalize( statement );

	return filtered;
}

void G_DBListBlacklist( BlackListCallback callback )
{
	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlListBlacklist, -1, &statement, 0 );

	rc = trap_sqlite3_step( statement );
	while ( rc == SQLITE_ROW )
	{
		unsigned int ip = sqlite3_column_int( statement, 0 );
		unsigned int mask = sqlite3_column_int( statement, 1 );
		const char* notes = ( const char* )sqlite3_column_text( statement, 2 );
		const char* reason = ( const char* )sqlite3_column_text( statement, 3 );
		const char* banned_since = ( const char* )sqlite3_column_text( statement, 4 );
		const char* banned_until = ( const char* )sqlite3_column_text( statement, 5 );

		callback( ip, mask, notes, reason, banned_since, banned_until );

		rc = trap_sqlite3_step( statement );
	}

	trap_sqlite3_finalize( statement );
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
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlAddToBlacklist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	sqlite3_bind_text( statement, 3, notes, -1, 0 );
	sqlite3_bind_text( statement, 4, reason, -1, 0 );

	sqlite3_bind_int( statement, 5, hours );

	rc = trap_sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBRemoveFromBlacklist( unsigned int ip,
	unsigned int mask )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlRemoveFromBlacklist, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ip );
	sqlite3_bind_int( statement, 2, mask );

	rc = trap_sqlite3_step( statement );

	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	trap_sqlite3_finalize( statement );

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
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlListPools, -1, &statement, 0 );

	rc = trap_sqlite3_step( statement );
	while ( rc == SQLITE_ROW )
	{
		int pool_id = sqlite3_column_int( statement, 0 );
		const char* short_name = ( const char* )sqlite3_column_text( statement, 1 );
		const char* long_name = ( const char* )sqlite3_column_text( statement, 2 );

		callback( context, pool_id, short_name, long_name );

		rc = trap_sqlite3_step( statement );
	}

	trap_sqlite3_finalize( statement );
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
	int rc = trap_sqlite3_prepare_v2( dbPtr, cooldownSeconds > 0 ? sqlListMapsInPoolWithCooldown : sqlListMapsInPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );
	sqlite3_bind_text( statement, 2, ignore, -1, 0 ); // ignore map, we
	if (cooldownSeconds > 0)
		sqlite3_bind_int(statement, 3, cooldownSeconds);

	rc = trap_sqlite3_step( statement );
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

		rc = trap_sqlite3_step( statement );
	}

	trap_sqlite3_finalize( statement );
}

qboolean G_DBFindPool( const char* short_name,
	PoolInfo* poolInfo )
{
	qboolean found = qfalse;

	sqlite3_stmt* statement;

	// prepare blacklist check statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlFindPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );

	rc = trap_sqlite3_step( statement );

	// blacklisted => we forbid it
	if ( rc == SQLITE_ROW )
	{
		int pool_id = sqlite3_column_int( statement, 0 );
		const char* long_name = ( const char* )sqlite3_column_text( statement, 1 );

		Q_strncpyz( poolInfo->long_name, long_name, sizeof( poolInfo->long_name ) );
		poolInfo->pool_id = pool_id;

		found = qtrue;
	}

	trap_sqlite3_finalize( statement );

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
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlCreatePool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );
	sqlite3_bind_text( statement, 2, long_name, -1, 0 );

	rc = trap_sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBPoolDeleteAllMaps( const char* short_name )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlDeleteAllMapsInPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );

	rc = trap_sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBPoolDelete( const char* short_name )
{
	qboolean success = qfalse;

	if ( G_DBPoolDeleteAllMaps( short_name ) )
	{
		sqlite3_stmt* statement;
		// prepare insert statement
		int rc = trap_sqlite3_prepare_v2( dbPtr, sqlDeletePool, -1, &statement, 0 );

		sqlite3_bind_text( statement, 1, short_name, -1, 0 );

		rc = trap_sqlite3_step( statement );
		if ( rc == SQLITE_DONE )
		{
			int changes = sqlite3_changes( dbPtr );
			if ( changes != 0 )
			{
				success = qtrue;
			}
		}

		trap_sqlite3_finalize( statement );
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
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlAddMapToPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, mapname, -1, 0 );
	sqlite3_bind_int( statement, 2, weight );
	sqlite3_bind_text( statement, 3, short_name, -1, 0 );

	rc = trap_sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		success = qtrue;
	}

	trap_sqlite3_finalize( statement );

	return success;
}

qboolean G_DBPoolMapRemove( const char* short_name,
	const char* mapname )
{
	qboolean success = qfalse;

	sqlite3_stmt* statement;
	// prepare insert statement
	int rc = trap_sqlite3_prepare_v2( dbPtr, sqlRemoveMapToPool, -1, &statement, 0 );

	sqlite3_bind_text( statement, 1, short_name, -1, 0 );
	sqlite3_bind_text( statement, 2, mapname, -1, 0 );

	rc = trap_sqlite3_step( statement );
	if ( rc == SQLITE_DONE )
	{
		int changes = sqlite3_changes( dbPtr );
		if ( changes != 0 )
		{
			success = qtrue;
		}
	}

	trap_sqlite3_finalize( statement );

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

static qboolean RememberedMapMatches(genericNode_t *node, void *userData) {
	const rememberedMultivoteMap_t *remembered = (const rememberedMultivoteMap_t *)node;
	const char *checkMap = (const char *)userData;

	if (!remembered || !checkMap)
		return qfalse;

	if (!Q_stricmp(remembered->mapFilename, checkMap))
		return qtrue;

	return qfalse;
}

// Versatile method to retrieve a tier list in the form of a malloc'd list_t pointer, with several customizable parameters.
// commaSeparatedAccountIds: null for zero accounts, e.g. "1" for one account, e.g. "1,2,3,4" for multiple accounts.
// singleMapFileName: null for all maps; otherwise specify a map to just check one specific map.
// requireMultipleVotes: qtrue means at least 2 votes from among the accounts being evaluated are required for a map to make the list.
// notPlayedWithinLastMinutes: only return maps that have not been pugged on within the last X minutes (use 0 to not use this).
// ignoreMapFileName: null to not ignore any maps; otherwise specify a map to ignore.
// randomize: randomize the list order.
// optionalInfoOut: tierListInfo_t pointer you can supply to output additional info.
// IMPORTANT: ListClear and free the result if not null!
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
			char *str4 = cooldownSeconds > 0 ? va("LEFT JOIN lastplayedmaporalias ON tierlistmaps.map = lastplayedmaporalias.map WHERE (lastplayedmaporalias.map IS NULL OR strftime('%%s', 'now') - lastplayedmaporalias.datetime > %d) AND account_id IN (", cooldownSeconds) : "WHERE account_id IN (";
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
			char *str4 = cooldownSeconds > 0 ? va("LEFT JOIN lastplayedmaporalias ON tierlistmaps.map = lastplayedmaporalias.map WHERE (lastplayedmaporalias.map IS NULL OR strftime('%%s', 'now') - lastplayedmaporalias.datetime > %d) AND account_id IN (", cooldownSeconds) : "WHERE account_id IN (";
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
			char *str4 = cooldownSeconds > 0 ? va("LEFT JOIN lastplayedmaporalias ON tierlistmaps.map = lastplayedmaporalias.map WHERE (lastplayedmaporalias.map IS NULL OR strftime('%%s', 'now') - lastplayedmaporalias.datetime > %d) ", cooldownSeconds) : "";
			char *str5 = VALIDSTRING(ignoreMapFileName) ? (cooldownSeconds > 0 ? "AND tierlistmaps.map != ? " : "WHERE tierlistmaps.map != ? ") : "";
			char *str6 = "GROUP BY tierlistmaps.map ORDER BY ";
			char *str7 = randomize ? "RANDOM();" : "tierlistmaps.map;";
			query = va("%s%s%s%s%s%s%s", str1, str2, str3, str4, str5, str6, str7);
		}
		else { // select maps even if they just have a single vote
			char *str1 = "SELECT tierlistmaps.map, AVG(tierlistmaps.tier) FROM (SELECT DISTINCT tierlistmaps.map FROM tierlistmaps JOIN tierwhitelist ON tierwhitelist.map = tierlistmaps.map";
			char *str2 = VALIDSTRING(singleMapFileName) ? " WHERE tierlistmaps.map = ?) " : ") ";
			char *str3 = "m JOIN tierlistmaps ON m.map = tierlistmaps.map ";
			char *str4 = cooldownSeconds > 0 ? va("LEFT JOIN lastplayedmaporalias ON tierlistmaps.map = lastplayedmaporalias.map WHERE (lastplayedmaporalias.map IS NULL OR strftime('%%s', 'now') - lastplayedmaporalias.datetime > %d) ", cooldownSeconds) : "";
			char *str5 = VALIDSTRING(ignoreMapFileName) ? (cooldownSeconds > 0 ? "AND tierlistmaps.map != ? " : "WHERE tierlistmaps.map != ? ") : "";
			char *str6 = "GROUP BY tierlistmaps.map ORDER BY ";
			char *str7 = randomize ? "RANDOM();" : "tierlistmaps.map;";
			query = va("%s%s%s%s%s%s%s", str1, str2, str3, str4, str5, str6, str7);
		}
	}

	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, query, -1, &statement, 0);
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

	rc = trap_sqlite3_step(statement);

	list_t *mapsList = malloc(sizeof(list_t));
	memset(mapsList, 0, sizeof(list_t));
	int numGotten = 0;

	qboolean needToForceInclude = qfalse;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		mapTier_t tier = MapTierForDouble(average);

		// check if map is banned right now
		rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, (char *)mapFileName, NULL);
		if (remembered) {
			if (remembered->forceInclude && (tier == MAPTIER_S || tier == MAPTIER_A)) {
				Com_DebugPrintf("Forcibly including %s due to being remembered as included map\n", mapFileName);
				needToForceInclude = qtrue;
			}
			else {
				Com_DebugPrintf("Forcibly excluding %s due to being remembered as excluded map\n", mapFileName);
				rc = trap_sqlite3_step(statement);
				continue;
			}
		}

		if (VALIDSTRING(mapFileName) && tier >= MAPTIER_F && tier <= MAPTIER_S) {
			mapTierData_t *data = (mapTierData_t *)ListAdd(mapsList, sizeof(mapTierData_t));
			data->tier = tier;
			Q_strncpyz(data->mapFileName, mapFileName, sizeof(data->mapFileName));
			if (optionalInfoOut)
				optionalInfoOut->numMapsOfTier[tier]++;
			numGotten++;
		}
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
	if (!numGotten) {
		ListClear(mapsList);
		return NULL;
	}

	if (needToForceInclude) {
		list_t temp = { 0 };
		ListCopy(mapsList, &temp, sizeof(mapTierData_t));
		ListClear(mapsList);

		// add the force included one to the top of the list
		{
			iterator_t iter;
			ListIterate(&temp, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				mapTierData_t *data = (mapTierData_t *)IteratorNext(&iter);
				rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, data->mapFileName, NULL);
				if (!(remembered && remembered->forceInclude && (data->tier == MAPTIER_S || data->tier == MAPTIER_A)))
					continue;
				mapTierData_t *add = (mapTierData_t *)ListAdd(mapsList, sizeof(mapTierData_t));
				add->tier = data->tier;
				Q_strncpyz(add->mapFileName, data->mapFileName, sizeof(add->mapFileName));
			}
		}

		// add all the others
		{
			iterator_t iter;
			ListIterate(&temp, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				mapTierData_t *data = (mapTierData_t *)IteratorNext(&iter);
				rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, data->mapFileName, NULL);
				if (remembered && remembered->forceInclude && (data->tier == MAPTIER_S || data->tier == MAPTIER_A))
					continue;
				mapTierData_t *add = (mapTierData_t *)ListAdd(mapsList, sizeof(mapTierData_t));
				add->tier = data->tier;
				Q_strncpyz(add->mapFileName, data->mapFileName, sizeof(add->mapFileName));
			}
		}

		ListClear(&temp);
	}

	return mapsList;
}

const char *sqlTierlistMapIsWhitelisted = "SELECT COUNT(*) FROM tierwhitelist WHERE map = ?1;";
const char *sqlTierlistMapIsWhitelistedAnyVersion =
"WITH relevant_maps AS ("
"SELECT filename "
"FROM mapaliases "
"WHERE alias = ("
"SELECT alias "
"FROM mapaliases "
"WHERE filename = ?1"
") OR filename = ?1"
" UNION "
"SELECT ?1"
") "
"SELECT COUNT(*), COALESCE(tw.map, ?1) AS whitelisted_filename, "
"COALESCE((SELECT filename FROM mapaliases WHERE islive = 1 AND alias = (SELECT alias FROM mapaliases WHERE filename = ?1)), ?1) AS live_filename "
"FROM tierwhitelist tw "
"WHERE tw.map IN (SELECT filename FROM relevant_maps) "
"UNION "
"SELECT 0, ?1, ?1 "
"WHERE NOT EXISTS (SELECT * FROM tierwhitelist WHERE map IN (SELECT filename FROM relevant_maps));";



qboolean G_DBTierlistMapIsWhitelisted(const char *mapName, qboolean anyVersion,
	char *whitelistedFilenameOut, size_t whitelistedFilenameOutSize,
	char *liveFilenameOut, size_t liveFilenameOutSize) {
	if (!VALIDSTRING(mapName))
		return qfalse;

	char mapFileName[MAX_QPATH] = { 0 };
	GetMatchingMap(mapName, mapFileName, sizeof(mapFileName));
	if (!mapFileName[0])
		return qfalse;

	qboolean found = qfalse;
	sqlite3_stmt *statement;

	if (whitelistedFilenameOut && whitelistedFilenameOutSize)
		*whitelistedFilenameOut = '\0';
	if (liveFilenameOut && liveFilenameOutSize)
		*liveFilenameOut = '\0';

	if (anyVersion) {
		trap_sqlite3_prepare_v2(dbPtr, sqlTierlistMapIsWhitelistedAnyVersion, -1, &statement, 0);
		sqlite3_bind_text(statement, 1, mapFileName, -1, 0);
		int rc = trap_sqlite3_step(statement);
		if (rc == SQLITE_ROW)
			found = !!(sqlite3_column_int(statement, 0));

		if (found) {
			const char *whitelistedFilename = sqlite3_column_text(statement, 1);
			if (VALIDSTRING(whitelistedFilename) && whitelistedFilenameOut && whitelistedFilenameOutSize)
				Q_strncpyz(whitelistedFilenameOut, whitelistedFilename, whitelistedFilenameOutSize);

			const char *liveFilename = sqlite3_column_text(statement, 2);
			if (VALIDSTRING(liveFilename) && liveFilenameOut && liveFilenameOutSize)
				Q_strncpyz(liveFilenameOut, liveFilename, liveFilenameOutSize);
		}
	}
	else {
		trap_sqlite3_prepare_v2(dbPtr, sqlTierlistMapIsWhitelisted, -1, &statement, 0);
		sqlite3_bind_text(statement, 1, mapFileName, -1, 0);
		int rc = trap_sqlite3_step(statement);
		if (rc == SQLITE_ROW)
			found = !!(sqlite3_column_int(statement, 0));
	}

	trap_sqlite3_finalize(statement);

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
	if (list) {
		ListClear(list);
		free(list);
	}

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
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlAddMapTier, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, accountId);
	sqlite3_bind_text(statement, 2, lowercase, -1, 0);
	sqlite3_bind_int(statement, 3, (int)tier);

	rc = trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);

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
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlRemoveMapTier, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, accountId);
	sqlite3_bind_text(statement, 2, lowercase, -1, 0);

	rc = trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlClearTierList = "DELETE FROM tierlistmaps WHERE account_id = ?1;";
qboolean G_DBClearTierList(int accountId) {
	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlClearTierList, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, accountId);

	rc = trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlClearTierListTier = "DELETE FROM tierlistmaps WHERE account_id = ?1 AND tier = ?2;";
qboolean G_DBClearTierListTier(int accountId, mapTier_t tier) {
	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlClearTierListTier, -1, &statement, 0);

	sqlite3_bind_int(statement, 1, accountId);
	sqlite3_bind_int(statement, 2, tier);

	rc = trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlGetTiersOfMap = "SELECT accounts.name, tierlistmaps.account_id, tierlistmaps.tier FROM tierlistmaps JOIN accounts ON accounts.account_id = tierlistmaps.account_id WHERE tierlistmaps.map = ?1 ORDER BY accounts.name;";
// returns a list_t of players and their tiers for a single map
// IMPORTANT: free the result if not null!
static list_t *GetTiersOfMap(const char *mapFileName) {
	if (!VALIDSTRING(mapFileName))
		return NULL;
	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetTiersOfMap, -1, &statement, 0);

	char lowercase[MAX_QPATH] = { 0 };
	Q_strncpyz(lowercase, mapFileName, sizeof(lowercase));
	Q_strlwr(lowercase);
	sqlite3_bind_text(statement, 1, lowercase, -1, 0);

	list_t *mapsList = malloc(sizeof(list_t));
	memset(mapsList, 0, sizeof(list_t));
	int numGotten = 0;

	rc = trap_sqlite3_step(statement);
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

		rc = trap_sqlite3_step(statement);
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
			if (ent->inuse && ent->client && ent->client->pers.connected != CON_DISCONNECTED && ent->client->account && ent->client->account->id == data->accountId &&
				!(ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(ent->client->pers.netname, "elo BOT"))) {
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
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlMapsNotRatedByPlayer, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, accountId);
	rc = trap_sqlite3_step(statement);

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
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
	if (!numGotten) {
		ListClear(mapsList);
		return NULL;
	}

	return mapsList;
}

const char *sqlShouldTellPlayerToRateCurrentMap =
"WITH relevant_maps AS ("
"SELECT filename "
"FROM mapaliases "
"WHERE alias = ("
"SELECT alias "
"FROM mapaliases "
"WHERE filename = ?1"
") OR filename = ?1 "
"UNION "
"SELECT ?1"
") "
"SELECT CASE "
"WHEN EXISTS ("
"SELECT 1 "
"FROM tierwhitelist "
"WHERE map IN (SELECT filename FROM relevant_maps)"
") AND NOT EXISTS ("
"SELECT 1 "
"FROM tierlistmaps tlm "
"WHERE tlm.account_id = ?2 AND tlm.map IN (SELECT filename FROM relevant_maps)"
") THEN 1 "
"ELSE 0 "
"END;";

// whitelisted and not rated by this player ==> qtrue
// not whitelisted, or IS rated by this player ==> qfalse
qboolean G_DBShouldTellPlayerToRateCurrentMap(int accountId, const char *overrideMap) {
	if (accountId == ACCOUNT_ID_UNLINKED)
		return qfalse;

	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlShouldTellPlayerToRateCurrentMap, -1, &statement, 0);
	sqlite3_bind_text(statement, 1, VALIDSTRING(overrideMap) ? overrideMap : level.mapname, -1, 0);
	sqlite3_bind_int(statement, 2, accountId);
	rc = trap_sqlite3_step(statement);

	qboolean shouldTell = qfalse;
	if (rc == SQLITE_ROW)
		shouldTell = !!(sqlite3_column_int(statement, 0));

	trap_sqlite3_finalize(statement);

	return shouldTell;
}

const char *sqlNumMapsRatedByPlayer = "SELECT COUNT(map) FROM tierlistmaps WHERE account_id = ? AND map IN (SELECT map FROM tierwhitelist);";
int G_GetNumberOfMapsRatedByPlayer(int accountId) {
	if (accountId == ACCOUNT_ID_UNLINKED)
		return 0;

	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlNumMapsRatedByPlayer, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, accountId);
	rc = trap_sqlite3_step(statement);

	int numRated = 0;
	if (rc == SQLITE_ROW)
		numRated = sqlite3_column_int(statement, 0);

	trap_sqlite3_finalize(statement);

	return numRated;
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

qboolean PolynomialFit (double *dependentValues,
	double *independentValues,
	unsigned int countOfElements,
	unsigned int order,
	double *coefficients) {
	enum { maxOrder = 5 };

	double B[maxOrder + 1] = { 0.0f };
	double P[((maxOrder + 1) * 2) + 1] = { 0.0f };
	double A[(maxOrder + 1) * 2 * (maxOrder + 1)] = { 0.0f };

	double x, y, powx;

	unsigned int ii, jj, kk;

	if (countOfElements <= order)
		return qfalse;

	if (order > maxOrder)
		return qfalse;

	for (ii = 0; ii < countOfElements; ii++) {
		x = dependentValues[ii];
		y = independentValues[ii];
		powx = 1;

		for (jj = 0; jj < (order + 1); jj++)  {
			B[jj] = B[jj] + (y * powx);
			powx = powx * x;
		}
	}

	P[0] = countOfElements;

	for (ii = 0; ii < countOfElements; ii++) {
		x = dependentValues[ii];
		powx = dependentValues[ii];

		for (jj = 1; jj < ((2 * (order + 1)) + 1); jj++) {
			P[jj] = P[jj] + powx;
			powx = powx * x;
		}
	}

	for (ii = 0; ii < (order + 1); ii++) {
		for (jj = 0; jj < (order + 1); jj++) {
			A[(ii * (2 * (order + 1))) + jj] = P[ii + jj];
		}

		A[(ii * (2 * (order + 1))) + (ii + (order + 1))] = 1;
	}

	for (ii = 0; ii < (order + 1); ii++) {
		x = A[(ii * (2 * (order + 1))) + ii];
		if (x != 0) {
			for (kk = 0; kk < (2 * (order + 1)); kk++) {
				A[(ii * (2 * (order + 1))) + kk] =
					A[(ii * (2 * (order + 1))) + kk] / x;
			}

			for (jj = 0; jj < (order + 1); jj++) {
				if ((jj - ii) != 0) {
					y = A[(jj * (2 * (order + 1))) + ii];
					for (kk = 0; kk < (2 * (order + 1)); kk++) {
						A[(jj * (2 * (order + 1))) + kk] =
							A[(jj * (2 * (order + 1))) + kk] -
							y * A[(ii * (2 * (order + 1))) + kk];
					}
				}
			}
		}
		else {
			return qfalse;
		}
	}

	for (ii = 0; ii < (order + 1); ii++) {
		for (jj = 0; jj < (order + 1); jj++) {
			x = 0;
			for (kk = 0; kk < (order + 1); kk++) {
				x = x + (A[(ii * (2 * (order + 1))) + (kk + (order + 1))] *
					B[kk]);
			}
			coefficients[ii] = x;
		}
	}

	return qtrue;
}

const char *sqlTierStatsNumRatings = "SELECT COUNT(tierlistmaps.map) FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map;";
const char *sqlTierStatsNumMaps = "SELECT COUNT(*) FROM (SELECT tierlistmaps.map FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map GROUP BY tierlistmaps.map);";
const char *sqlTierStatsNumPlayers = "SELECT COUNT(*) FROM (SELECT accounts.account_id FROM accounts JOIN (SELECT * FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map) m ON accounts.account_id = m.account_id GROUP BY accounts.account_id);";
const char *sqlGetBestMaps = "SELECT tierlistmaps.map, AVG(tier) averageTier FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map GROUP BY tierlistmaps.map ORDER BY averageTier DESC LIMIT 10;";
const char *sqlGetWorstMaps = "SELECT tierlistmaps.map, AVG(tier) averageTier FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map GROUP BY tierlistmaps.map ORDER BY averageTier ASC LIMIT 10;";
const char *sqlGetLowestVariance = "SELECT tierlistmaps.map, AVG(tier), AVG(tier*tier) - AVG(tier)*AVG(tier) AS var FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map GROUP BY tierlistmaps.map HAVING COUNT(*) >= 2 ORDER BY var ASC, tierlistmaps.map LIMIT 5;";
const char *sqlGetHighestVariance = "SELECT tierlistmaps.map, AVG(tier), AVG(tier*tier) - AVG(tier)*AVG(tier) AS var FROM tierlistmaps JOIN tierwhitelist ON tierlistmaps.map = tierwhitelist.map GROUP BY tierlistmaps.map HAVING COUNT(*) >= 2 ORDER BY var DESC, tierlistmaps.map LIMIT 5;";
const char *sqlGetMostPlayedMaps = "SELECT num_played_view.map, avgTier, numPlayed FROM num_played_view JOIN tierwhitelist ON num_played_view.map = tierwhitelist.map ORDER BY numPlayed DESC, avgTier DESC, num_played_view.map ASC LIMIT 10;";
const char *sqlGetLeastPlayedMaps = "SELECT num_played_view.map, avgTier, numPlayed FROM num_played_view JOIN tierwhitelist ON num_played_view.map = tierwhitelist.map ORDER BY numPlayed ASC, avgTier DESC, num_played_view.map ASC LIMIT 10;";
const char *sqlGetMostConformingPlayers = "WITH a AS (SELECT *, abs(round(avgTier, 0) - round(tier, 0)) diff FROM tierlistmaps JOIN num_played_view ON num_played_view.map = tierlistmaps.map), filteredPlayers AS (SELECT account_id FROM tierlistmaps GROUP BY account_id HAVING COUNT(map) >= 10) SELECT name, avg(diff) avgDiff FROM a JOIN accounts ON a.account_id = accounts.account_id JOIN filteredPlayers ON filteredPlayers.account_id = a.account_id GROUP BY a.account_id ORDER BY avgDiff ASC LIMIT 5;";
const char *sqlGetLeastConformingPlayers = "WITH a AS (SELECT *, abs(round(avgTier, 0) - round(tier, 0)) diff FROM tierlistmaps JOIN num_played_view ON num_played_view.map = tierlistmaps.map), filteredPlayers AS (SELECT account_id FROM tierlistmaps GROUP BY account_id HAVING COUNT(map) >= 10) SELECT name, avg(diff) avgDiff FROM a JOIN accounts ON a.account_id = accounts.account_id JOIN filteredPlayers ON filteredPlayers.account_id = a.account_id GROUP BY a.account_id ORDER BY avgDiff DESC LIMIT 5;";
const char *sqlGetNumPlayedCount = "SELECT COUNT(*) FROM num_played_view ORDER BY map ASC;";
const char *sqlGetNumPlayed = "SELECT * FROM num_played_view ORDER BY map ASC;";

typedef struct {
	double	rating;
	double	numPlayed;
	double	expected;
	double	diff;
	char	mapFileName[MAX_QPATH];
} ratedMap_t;

int RatedMapCompareFunc(const void *a, const void *b) {
	ratedMap_t *aa = (ratedMap_t *)a, *bb = (ratedMap_t *)b;
	if (aa->diff < bb->diff)
		return -1;
	else if (aa->diff > bb->diff)
		return 1;
	return 0;
}

void G_DBTierStats(int clientNum) {
	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlTierStatsNumRatings, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	int numRatings = 0;
	if (rc == SQLITE_ROW) {
		numRatings = sqlite3_column_int(statement, 0);
	}

	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlTierStatsNumMaps, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	int numMaps = 0;
	if (rc == SQLITE_ROW) {
		numMaps = sqlite3_column_int(statement, 0);
	}

	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlTierStatsNumPlayers, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	int numPlayers = 0;
	if (rc == SQLITE_ROW) {
		numPlayers = sqlite3_column_int(statement, 0);
	}

	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetBestMaps, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	char topMaps[10][MAX_QPATH] = { 0 };
	int numTopMaps = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		if (VALIDSTRING(mapFileName)) {
			char shortName[MAX_QPATH] = { 0 };
			GetShortNameForMapFileName(mapFileName, shortName, sizeof(shortName));
			Com_sprintf(topMaps[numTopMaps], sizeof(topMaps[numTopMaps]), "%s%s^7 (%0.2f)", GetTierColorForTier(MapTierForDouble(average)), shortName, average);
			numTopMaps++;
		}
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetWorstMaps, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	char worstMaps[10][MAX_QPATH] = { 0 };
	int numWorstMaps = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		if (VALIDSTRING(mapFileName)) {
			char shortName[MAX_QPATH] = { 0 };
			GetShortNameForMapFileName(mapFileName, shortName, sizeof(shortName));
			Com_sprintf(worstMaps[numWorstMaps], sizeof(worstMaps[numWorstMaps]), "%s%s^7 (%0.2f)", GetTierColorForTier(MapTierForDouble(average)), shortName, average);
			numWorstMaps++;
		}
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetLowestVariance, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	char leastControversialMaps[5][MAX_QPATH] = { 0 };
	int numLeastControversial = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		double variance = sqlite3_column_double(statement, 2);
		if (VALIDSTRING(mapFileName)) {
			char shortName[MAX_QPATH] = { 0 };
			GetShortNameForMapFileName(mapFileName, shortName, sizeof(shortName));
			Com_sprintf(leastControversialMaps[numLeastControversial], sizeof(leastControversialMaps[numLeastControversial]), "%s%s^7 (%0.2f)", GetTierColorForTier(MapTierForDouble(average)), shortName, sqrt(variance));
			numLeastControversial++;
		}
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetHighestVariance, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	char mostControversialMaps[5][MAX_QPATH] = { 0 };
	int numMostControversial = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		double variance = sqlite3_column_double(statement, 2);
		if (VALIDSTRING(mapFileName)) {
			char shortName[MAX_QPATH] = { 0 };
			GetShortNameForMapFileName(mapFileName, shortName, sizeof(shortName));
			Com_sprintf(mostControversialMaps[numMostControversial], sizeof(mostControversialMaps[numMostControversial]), "%s%s^7 (%0.2f)", GetTierColorForTier(MapTierForDouble(average)), shortName, sqrt(variance));
			numMostControversial++;
		}
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetLeastConformingPlayers, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	char leastConformingPlayers[5][MAX_QPATH] = { 0 };
	int numLeastConforming = 0;
	while (rc == SQLITE_ROW) {
		const char *playerName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		if (VALIDSTRING(playerName)) {
			Com_sprintf(leastConformingPlayers[numLeastConforming], sizeof(leastConformingPlayers[numLeastConforming]), "%s^7 (%0.2f)", playerName, average);
			numLeastConforming++;
		}
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetMostConformingPlayers, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	char mostConformingPlayers[5][MAX_QPATH] = { 0 };
	int numMostConforming = 0;
	while (rc == SQLITE_ROW) {
		const char *playerName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		if (VALIDSTRING(playerName)) {
			Com_sprintf(mostConformingPlayers[numMostConforming], sizeof(mostConformingPlayers[numMostConforming]), "%s^7 (%0.2f)", playerName, average);
			numMostConforming++;
		}
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetMostPlayedMaps, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	char mostPlayedMaps[10][MAX_QPATH] = { 0 };
	int numMostPlayedMaps = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		int count = sqlite3_column_int(statement, 2);
		if (VALIDSTRING(mapFileName)) {
			char shortName[MAX_QPATH];
			GetShortNameForMapFileName(mapFileName, shortName, sizeof(shortName));
			Com_sprintf(mostPlayedMaps[numMostPlayedMaps], sizeof(mostPlayedMaps[numMostPlayedMaps]), "%s%s^7 (%d)", GetTierColorForTier(MapTierForDouble(average)), shortName, count);
			numMostPlayedMaps++;
		}
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetLeastPlayedMaps, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);
	char leastPlayedMaps[10][MAX_QPATH] = { 0 };
	int numLeastPlayedMaps = 0;
	while (rc == SQLITE_ROW) {
		const char *mapFileName = (const char *)sqlite3_column_text(statement, 0);
		double average = sqlite3_column_double(statement, 1);
		int count = sqlite3_column_int(statement, 2);
		if (VALIDSTRING(mapFileName)) {
			char shortName[MAX_QPATH];
			GetShortNameForMapFileName(mapFileName, shortName, sizeof(shortName));
			Com_sprintf(leastPlayedMaps[numLeastPlayedMaps], sizeof(leastPlayedMaps[numLeastPlayedMaps]), "%s%s^7 (%d)", GetTierColorForTier(MapTierForDouble(average)), shortName, count);
			numLeastPlayedMaps++;
		}
		rc = trap_sqlite3_step(statement);
	}

	PrintIngame(clientNum, "There are %d map ratings across %d maps from %d players.\n", numRatings, numMaps, numPlayers);

	if (numTopMaps || numWorstMaps)
		PrintIngame(clientNum, "\nBy average rating (%s^7 = %d; %s^7 = %d):\n", GetTierStringForTier(MAPTIER_S), (int)MAPTIER_S, GetTierStringForTier(MAPTIER_F), (int)MAPTIER_F);
	if (numTopMaps) {
		char topMapsStr[1024] = { 0 };
		for (int i = 0; i < numTopMaps; i++)
			Q_strcat(topMapsStr, sizeof(topMapsStr), va("%s%s^7", i ? "^7, " : "", topMaps[i]));
		PrintIngame(clientNum, "  ^2Top^7 %d maps: %s\n", numTopMaps, topMapsStr);
	}
	if (numWorstMaps) {
		char worstMapsStr[1024] = { 0 };
		for (int i = 0; i < numWorstMaps; i++)
			Q_strcat(worstMapsStr, sizeof(worstMapsStr), va("%s%s^7", i ? "^7, " : "", worstMaps[i]));
		PrintIngame(clientNum, "  ^1Bottom^7 %d maps: %s\n", numWorstMaps, worstMapsStr);
	}

	if (numMostPlayedMaps || numLeastPlayedMaps)
		PrintIngame(clientNum, "\n");
	if (numMostPlayedMaps) {
		char mostPlayedStr[1024] = { 0 };
		for (int i = 0; i < numMostPlayedMaps; i++)
			Q_strcat(mostPlayedStr, sizeof(mostPlayedStr), va("%s%s^7", i ? "^7, " : "", mostPlayedMaps[i]));
		PrintIngame(clientNum, "  ^2Most played^7 %d maps: %s\n", numMostPlayedMaps, mostPlayedStr);
	}
	if (numLeastPlayedMaps) {
		char leastPlayedStr[1024] = { 0 };
		for (int i = 0; i < numLeastPlayedMaps; i++)
			Q_strcat(leastPlayedStr, sizeof(leastPlayedStr), va("%s%s^7", i ? "^7, " : "", leastPlayedMaps[i]));
		PrintIngame(clientNum, "  ^1Least played^7 %d maps: %s\n", numLeastPlayedMaps, leastPlayedStr);
	}

	if (numLeastControversial || numMostControversial)
		PrintIngame(clientNum, "\nBy standard deviation:\n");
	if (numMostControversial) {
		char mostControversialStr[1024] = { 0 };
		for (int i = 0; i < numMostControversial; i++)
			Q_strcat(mostControversialStr, sizeof(mostControversialStr), va("%s%s", i ? "^7, " : "", mostControversialMaps[i]));
		PrintIngame(clientNum, "  ^8Most controversial^7 %d maps: %s\n", numMostControversial, mostControversialStr);
	}
	if (numLeastControversial) {
		char leastControversialStr[1024] = { 0 };
		for (int i = 0; i < numLeastControversial; i++)
			Q_strcat(leastControversialStr, sizeof(leastControversialStr), va("%s%s", i ? "^7, " : "", leastControversialMaps[i]));
		PrintIngame(clientNum, "  ^5Least controversial^7 %d maps: %s\n", numLeastControversial, leastControversialStr);
	}

	if (numLeastConforming || numMostConforming)
		PrintIngame(clientNum, "\nBy average difference from community tier (minimum 10 ratings):\n");
	if (numMostConforming) {
		char mostConformingStr[1024] = { 0 };
		for (int i = 0; i < numMostConforming; i++)
			Q_strcat(mostConformingStr, sizeof(mostConformingStr), va("%s%s^7", i ? "^7, " : "", mostConformingPlayers[i]));
		PrintIngame(clientNum, "  ^5Most conformist^7 %d players: %s\n", numMostConforming, mostConformingStr);
	}
	if (numLeastConforming) {
		char leastConformingStr[1024] = { 0 };
		for (int i = 0; i < numLeastConforming; i++)
			Q_strcat(leastConformingStr, sizeof(leastConformingStr), va("%s%s^7", i ? "^7, " : "", leastConformingPlayers[i]));
		PrintIngame(clientNum, "  ^8Least conformist^7 %d players: %s\n", numLeastConforming, leastConformingStr);
	}

	trap_sqlite3_finalize(statement);
}

// display people who have rated the most maps first; break ties alphabetically
const char *sqlGetPlayersWhoHaveRatedMaps = "SELECT accounts.name, tierlistmaps.account_id, COUNT(tierlistmaps.account_id) AS num_ratings FROM tierlistmaps JOIN tierwhitelist ON tierwhitelist.map = tierlistmaps.map JOIN accounts ON accounts.account_id = tierlistmaps.account_id GROUP BY tierlistmaps.account_id ORDER BY COUNT(tierlistmaps.account_id) DESC, accounts.name;";
qboolean G_DBTierListPlayersWhoHaveRatedMaps(int clientNum, const char *successPrologueMessage) {
	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetPlayersWhoHaveRatedMaps, -1, &statement, 0);
	rc = trap_sqlite3_step(statement);

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
				if (ent->inuse && ent->client && ent->client->pers.connected != CON_DISCONNECTED && ent->client->account && ent->client->account->id == accountId &&
					!(ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(ent->client->pers.netname, "elo BOT"))) {
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
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);

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

	trap_sqlite3_prepare_v2(dbPtr, sqlAddMapToTierWhitelist, -1, &statement, 0);

	sqlite3_bind_text(statement, 1, mapFileName, -1, 0);

	int rc = trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlRemoveMapFromTierWhitelist = "DELETE FROM tierwhitelist WHERE map = ?1;";
qboolean G_DBRemoveMapFromTierWhitelist(const char *mapFileName) {
	if (!VALIDSTRING(mapFileName))
		return qfalse;

	sqlite3_stmt *statement;

	trap_sqlite3_prepare_v2(dbPtr, sqlRemoveMapFromTierWhitelist, -1, &statement, 0);

	sqlite3_bind_text(statement, 1, mapFileName, -1, 0);

	int rc = trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlGetTierWhitelistedMaps = "SELECT map FROM tierwhitelist ORDER BY map;";
// IMPORTANT: free the result if not null!
list_t *G_DBGetTierWhitelistedMaps(void) {
	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetTierWhitelistedMaps, -1, &statement, 0);

	rc = trap_sqlite3_step(statement);

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

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);

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

// note: this probably only works properly with e.g. mp/ctf_illimiran inputs, not ctf_illimiran_v2 alias map filenames
qboolean DB_IsTopMap(const char *mapFilename, int num) {
	if (!VALIDSTRING(mapFilename)) {
		assert(qfalse);
		return qfalse;
	}

	sqlite3_stmt *statement;
	qboolean result = qfalse;

	const char *sql = "SELECT map FROM ("
		"SELECT num_played_view.map AS map "
		"FROM num_played_view "
		"ORDER BY num_played_view.numPlayed DESC "
		"LIMIT ?1"
		") topmaps "
		"WHERE topmaps.map = ?2";

	trap_sqlite3_prepare_v2(dbPtr, sql, -1, &statement, NULL);
	sqlite3_bind_int(statement, 1, num);
	sqlite3_bind_text(statement, 2, mapFilename, -1, SQLITE_STATIC);

	int rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		result = qtrue;
	}

	trap_sqlite3_finalize(statement);

	return result;
}

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
	list_t *allMapsList = GetTierList(ingamePlayersStr, NULL, qtrue, g_vote_mapCooldownMinutes.integer > 0 ? g_vote_mapCooldownMinutes.integer : 0, NULL, qtrue, &info);

	const int totalMapsToChoose = Com_Clampi(2, MAX_MULTIVOTE_MAPS, g_vote_tierlist_totalMaps.integer);
	int numToSelectForEachTier[NUM_MAPTIERS] = { 0 };
	int totalToSelect = 0, tries = 0;

	
	qboolean usingCommunityRatings;
	for (usingCommunityRatings = qfalse; usingCommunityRatings <= qtrue; usingCommunityRatings++) {
		if (usingCommunityRatings) {
			if (g_vote_tierlist_debug.integer)
				G_LogPrintf("Unable to generate a working set of ingame-rated maps; trying again and including community-rated maps.\n");
			if (allMapsList) {
				ListClear(allMapsList);
				free(allMapsList);
			}
			allMapsList = GetTierList(NULL, NULL, qfalse, g_vote_mapCooldownMinutes.integer > 0 ? g_vote_mapCooldownMinutes.integer : 0, NULL, qtrue, &info);
		}
		if (!allMapsList || allMapsList->size < totalMapsToChoose)
			continue;

		if (g_vote_tierlist_debug.integer)
			G_LogPrintf("GetTierList returned %d S, %d A, %d B, %d C, %d F\n", info.numMapsOfTier[MAPTIER_S], info.numMapsOfTier[MAPTIER_A], info.numMapsOfTier[MAPTIER_B], info.numMapsOfTier[MAPTIER_C], info.numMapsOfTier[MAPTIER_F]);

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
			if (g_vote_tierlist_debug.integer)
				G_LogPrintf("Not enough maps of tier %s^7; going to try to use more maps of other tier(s).\n", GetTierStringForTier(t));
			mapTier_t checkOrder[4];
			switch (t) {
			case MAPTIER_S: checkOrder[0] = MAPTIER_A; checkOrder[1] = MAPTIER_B; checkOrder[2] = MAPTIER_C; checkOrder[3] = MAPTIER_F; break;
			case MAPTIER_A: checkOrder[0] = MAPTIER_B; checkOrder[1] = MAPTIER_S; checkOrder[2] = MAPTIER_C; checkOrder[3] = MAPTIER_F; break;
			case MAPTIER_B: checkOrder[0] = MAPTIER_C; checkOrder[1] = MAPTIER_A; checkOrder[2] = MAPTIER_F; checkOrder[3] = MAPTIER_S; break;
			case MAPTIER_C: checkOrder[0] = MAPTIER_F; checkOrder[1] = MAPTIER_B; checkOrder[2] = MAPTIER_A; checkOrder[3] = MAPTIER_S; break;
			case MAPTIER_F: checkOrder[0] = MAPTIER_C; checkOrder[1] = MAPTIER_B; checkOrder[2] = MAPTIER_A; checkOrder[3] = MAPTIER_S; break;
			}
			for (int i = 0; i < 4; i++) {
				int deficit = min[t] - info.numMapsOfTier[t];
				while (deficit > 0 && min[checkOrder[i]] < info.numMapsOfTier[checkOrder[i]] && min[checkOrder[i]] + 1 < MAX_MULTIVOTE_MAPS) {
					// we found a tier in the priority list that's not using all of its existent maps; allow it to use additional map(s)
					if (g_vote_tierlist_debug.integer)
						G_LogPrintf("Adding to the minimum of %s^7 to compensate for %s^7.\n", GetTierStringForTier(checkOrder[i]), GetTierStringForTier(t));
					++min[checkOrder[i]];

					// make sure we didn't exceed max for this tier by setting max higher if needed
					if (min[checkOrder[i]] > max[checkOrder[i]])
						max[checkOrder[i]] = min[checkOrder[i]];

					--deficit;

					--min[t];
				}
				if (deficit <= 0)
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
			if (g_vote_tierlist_debug.integer)
				G_LogPrintf("Sum of maxes (%d) < total maps to choose (%d); going to try to bump up tier(s).\n", sumOfMaxes, totalMapsToChoose);
			for (mapTier_t t = MAPTIER_S; t >= MAPTIER_F; t--) {
				while (sumOfMaxes < totalMapsToChoose && info.numMapsOfTier[t] > max[t]) {
					if (g_vote_tierlist_debug.integer)
						G_LogPrintf("Adding to the maximum of %s^7 to compensate.\n", GetTierStringForTier(t));
					++max[t];
					++sumOfMaxes;
				}
			}
		}
		if (sumOfMaxes < totalMapsToChoose)
			continue; // we somehow couldn't fix the max issue

		if (g_vote_tierlist_debug.integer)
			G_LogPrintf("Going to attempt randomization with mins/maxes: %d - %d S, %d - %d A, %d - %d B, %d - %d C, %d - %d F\n",
				min[MAPTIER_S], max[MAPTIER_S], min[MAPTIER_A], max[MAPTIER_A], min[MAPTIER_B], max[MAPTIER_B],
				min[MAPTIER_C], max[MAPTIER_C], min[MAPTIER_F], max[MAPTIER_F]);

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
	if (g_vote_tierlist_debug.integer)
		G_LogPrintf("Selecting %d S tier maps, %d A tier maps, %d B tier maps, %d C tier maps, and %d F tier maps (took %d randomizations).\n",
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
					if (g_vote_tierlist_debug.integer)
						G_LogPrintf("Selecting %s (%s^7)\n", mapShortName, GetTierStringForTier(t));
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

	// get a randomly ordered list of top 6 maps (except for those on cooldown)
	list_t top6Maps = { 0 };
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, "WITH topmaps AS (SELECT num_played_view.map map, lastplayedmaporalias.num num, lastplayedmaporalias.datetime lasttimeplayed FROM num_played_view LEFT JOIN lastplayedmaporalias ON lastplayedmaporalias.map = num_played_view.map ORDER BY num DESC LIMIT 6) SELECT map FROM topmaps WHERE num IS NULL OR strftime('%s', 'now') - lasttimeplayed > ? ORDER BY RANDOM();", -1, &statement, 0);
	const int cooldownSeconds = g_vote_mapCooldownMinutes.integer > 0 ? g_vote_mapCooldownMinutes.integer * 60 : 0;
	sqlite3_bind_int(statement, 1, cooldownSeconds);
	int rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char *map = (const char *)sqlite3_column_text(statement, 0);
		if (MapExistsQuick(map)) {
			rememberedMultivoteMap_t *add = ListAdd(&top6Maps, sizeof(rememberedMultivoteMap_t));
			Q_strncpyz(add->mapFilename, map, sizeof(add->mapFilename));
		}
		rc = trap_sqlite3_step(statement);
	}

	// get a randomly ordered list of top 10 maps (except for those on cooldown)
	list_t top10Maps = { 0 };
	trap_sqlite3_reset(statement);
	trap_sqlite3_prepare_v2(dbPtr, "WITH topmaps AS (SELECT num_played_view.map map, lastplayedmaporalias.num num, lastplayedmaporalias.datetime lasttimeplayed FROM num_played_view LEFT JOIN lastplayedmaporalias ON lastplayedmaporalias.map = num_played_view.map ORDER BY num DESC LIMIT 10) SELECT map FROM topmaps WHERE num IS NULL OR strftime('%s', 'now') - lasttimeplayed > ? ORDER BY RANDOM();", -1, &statement, 0);
	sqlite3_bind_int(statement, 1, cooldownSeconds);
	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char *map = (const char *)sqlite3_column_text(statement, 0);
		if (MapExistsQuick(map)) {
			rememberedMultivoteMap_t *add = ListAdd(&top10Maps, sizeof(rememberedMultivoteMap_t));
			Q_strncpyz(add->mapFilename, map, sizeof(add->mapFilename));
		}
		rc = trap_sqlite3_step(statement);
	}

	if (g_vote_tierlist_fixShittyPools.integer && numMapsPickedTotal >= 5) {
		// take stock of what we have chosen
		int numTop6MapsChosen = 0, numTop10MapsChosen = 0;
		for (int i = 0; i < numMapsPickedTotal; i++) {
			qboolean isTop6Map = !!(ListFind(&top6Maps, RememberedMapMatches, chosenMapNames[i], NULL));
			if (isTop6Map) {
				if (g_vote_tierlist_debug.integer)
					G_LogPrintf("g_vote_tierlist_fixShittyPools: %s is a top 6 map, and by extension a top 10 map\n", chosenMapNames[i]);
				numTop6MapsChosen++;
			}

			qboolean isTop10Map = !!(ListFind(&top10Maps, RememberedMapMatches, chosenMapNames[i], NULL));
			if (isTop10Map) {
				if (g_vote_tierlist_debug.integer && !isTop6Map)
					G_LogPrintf("g_vote_tierlist_fixShittyPools: %s is a top 10 map\n", chosenMapNames[i]);
				numTop10MapsChosen++;
			}
		}

		if (g_vote_tierlist_debug.integer) {
			G_LogPrintf("g_vote_tierlist_fixShittyPools: tentatively selected %d top 10 map%s, including %d top 6 map%s. top10Maps.size is %d, top6Maps.size is %d.\n",
				numTop10MapsChosen, numTop10MapsChosen == 1 ? "" : "s", numTop6MapsChosen, numTop6MapsChosen == 1 ? "" : "s", top10Maps.size, top6Maps.size);
		}

		assert(top10Maps.size >= top6Maps.size);

		if (numTop6MapsChosen >= 2) {
			// do nothing
		}
		else if (numTop6MapsChosen >= 1 && numTop10MapsChosen >= 2) {
			// do nothing
		}
		else if (numTop6MapsChosen == 1 && numTop10MapsChosen == 1) {
			// pick something other than the lone top 6 map and change it to a top 10 map
			if (top10Maps.size >= 2) {
				for (int i = numMapsPickedTotal - 1; i >= 0; i--) {
					qboolean isTop6Map = !!(ListFind(&top6Maps, RememberedMapMatches, chosenMapNames[i], NULL));
					rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, chosenMapNames[i], NULL);
					qboolean isForceIncludedMap = !!(remembered && remembered->forceInclude);

					if (isTop6Map || isForceIncludedMap)
						continue;

					rememberedMultivoteMap_t *addMap = (rememberedMultivoteMap_t *)top10Maps.head;
					qboolean thisMapAlreadyChosen, suitableMapFound = qfalse;

					do {
						thisMapAlreadyChosen = qfalse;

						for (int j = 0; j < numMapsPickedTotal; j++) {
							if (!Q_stricmp(addMap->mapFilename, chosenMapNames[j])) {
								thisMapAlreadyChosen = qtrue;
								break;
							}
						}

						if (!thisMapAlreadyChosen) { // make sure the map we are going to sub in wasn't already rerolled out
							rememberedMultivoteMap_t *addMapRemembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, addMap->mapFilename, NULL);
							qboolean addMapIsRememberedToBeForceExcluded = !!(addMapRemembered && !addMapRemembered->forceInclude);
							if (addMapIsRememberedToBeForceExcluded)
								thisMapAlreadyChosen = qtrue;
						}

						// If the current map is already chosen and there's a next map, move to the next map
						if (thisMapAlreadyChosen && addMap->node.next) {
							addMap = addMap->node.next;
						}
						else {
							// If the map isn't chosen, update newMapFound and break out of the loop
							if (!thisMapAlreadyChosen) {
								suitableMapFound = qtrue;
							}
							break; // Break out of the loop in any case
						}
					} while (1); // Keep looping until we break out


					if (suitableMapFound) {
						if (g_vote_tierlist_debug.integer) {
							char newShortName[MAX_QPATH] = { 0 }, oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(addMap->mapFilename, newShortName, sizeof(newShortName));
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_tierlist_fixShittyPools: swapping in top 10 map %s in place of %s\n",
								newShortName, oldShortName);
						}

						Q_strncpyz(chosenMapNames[i], addMap->mapFilename, sizeof(chosenMapNames[i]));
					}
					else {
						if (g_vote_tierlist_debug.integer) {
							char oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_tierlist_fixShittyPools: no suitable top 10 map to sub in for %s!\n", oldShortName);
						}
					}

					break;
				}
			}
		}
		else if (!numTop6MapsChosen && numTop10MapsChosen >= 2) {
			// pick anything and change it to a top 6 map
			if (top6Maps.size >= 1) {
				for (int i = numMapsPickedTotal - 1; i >= 0; i--) {
					rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, chosenMapNames[i], NULL);
					qboolean isForceIncludedMap = !!(remembered && remembered->forceInclude);

					if (isForceIncludedMap)
						continue;

					rememberedMultivoteMap_t *addMap = (rememberedMultivoteMap_t *)top6Maps.head;
					qboolean thisMapAlreadyChosen, suitableMapFound = qfalse;

					do {
						thisMapAlreadyChosen = qfalse;

						for (int j = 0; j < numMapsPickedTotal; j++) {
							if (!Q_stricmp(addMap->mapFilename, chosenMapNames[j])) {
								thisMapAlreadyChosen = qtrue;
								break;
							}
						}

						if (!thisMapAlreadyChosen) { // make sure the map we are going to sub in wasn't already rerolled out
							rememberedMultivoteMap_t *addMapRemembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, addMap->mapFilename, NULL);
							qboolean addMapIsRememberedToBeForceExcluded = !!(addMapRemembered && !addMapRemembered->forceInclude);
							if (addMapIsRememberedToBeForceExcluded)
								thisMapAlreadyChosen = qtrue;
						}

						// If the current map is already chosen and there's a next map, move to the next map
						if (thisMapAlreadyChosen && addMap->node.next) {
							addMap = addMap->node.next;
						}
						else {
							// If the map isn't chosen, update newMapFound and break out of the loop
							if (!thisMapAlreadyChosen) {
								suitableMapFound = qtrue;
							}
							break; // Break out of the loop in any case
						}
					} while (1); // Keep looping until we break out

					if (suitableMapFound) {
						if (g_vote_tierlist_debug.integer) {
							char newShortName[MAX_QPATH] = { 0 }, oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(addMap->mapFilename, newShortName, sizeof(newShortName));
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_tierlist_fixShittyPools: swapping in top 6 map %s in place of %s\n",
								newShortName, oldShortName);
						}

						Q_strncpyz(chosenMapNames[i], addMap->mapFilename, sizeof(chosenMapNames[i]));
					}
					else {
						if (g_vote_tierlist_debug.integer) {
							char oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_tierlist_fixShittyPools: no suitable top 6 map to sub in for %s!\n", oldShortName);
						}
					}

					break;
				}
			}
		}
		else if (!numTop6MapsChosen && numTop10MapsChosen == 1) {
			// pick something other than the lone top 10 map and change it to a top 6 map
			if (top6Maps.size >= 1) {
				for (int i = numMapsPickedTotal - 1; i >= 0; i--) {
					qboolean isTop10Map = !!(ListFind(&top10Maps, RememberedMapMatches, chosenMapNames[i], NULL));
					rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, chosenMapNames[i], NULL);
					qboolean isForceIncludedMap = !!(remembered && remembered->forceInclude);

					if (isTop10Map || isForceIncludedMap)
						continue;

					rememberedMultivoteMap_t *addMap = (rememberedMultivoteMap_t *)top6Maps.head;
					qboolean thisMapAlreadyChosen, suitableMapFound = qfalse;

					do {
						thisMapAlreadyChosen = qfalse;

						for (int j = 0; j < numMapsPickedTotal; j++) {
							if (!Q_stricmp(addMap->mapFilename, chosenMapNames[j])) {
								thisMapAlreadyChosen = qtrue;
								break;
							}
						}

						if (!thisMapAlreadyChosen) { // make sure the map we are going to sub in wasn't already rerolled out
							rememberedMultivoteMap_t *addMapRemembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, addMap->mapFilename, NULL);
							qboolean addMapIsRememberedToBeForceExcluded = !!(addMapRemembered && !addMapRemembered->forceInclude);
							if (addMapIsRememberedToBeForceExcluded)
								thisMapAlreadyChosen = qtrue;
						}

						// If the current map is already chosen and there's a next map, move to the next map
						if (thisMapAlreadyChosen && addMap->node.next) {
							addMap = addMap->node.next;
						}
						else {
							// If the map isn't chosen, update newMapFound and break out of the loop
							if (!thisMapAlreadyChosen) {
								suitableMapFound = qtrue;
							}
							break; // Break out of the loop in any case
						}
					} while (1); // Keep looping until we break out

					if (suitableMapFound) {
						if (g_vote_tierlist_debug.integer) {
							char newShortName[MAX_QPATH] = { 0 }, oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(addMap->mapFilename, newShortName, sizeof(newShortName));
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_tierlist_fixShittyPools: swapping in top 6 map %s in place of %s\n",
								newShortName, oldShortName);
						}

						Q_strncpyz(chosenMapNames[i], addMap->mapFilename, sizeof(chosenMapNames[i]));
					}
					else {
						if (g_vote_tierlist_debug.integer) {
							char oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_tierlist_fixShittyPools: no suitable top 6 map to sub in for %s!\n", oldShortName);
						}
					}

					break;
				}
			}
			else if (top10Maps.size >= 2) {
				// edge case: one top 10 map chosen, and no top 6 maps are available at all
				// pick something other than the lone top 10 map and change it to (another) top 10 map
				for (int i = numMapsPickedTotal - 1; i >= 0; i--) {
					qboolean isTop10Map = !!(ListFind(&top10Maps, RememberedMapMatches, chosenMapNames[i], NULL));
					rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, chosenMapNames[i], NULL);
					qboolean isForceIncludedMap = !!(remembered && remembered->forceInclude);

					if (isTop10Map || isForceIncludedMap)
						continue;

					rememberedMultivoteMap_t *addMap = (rememberedMultivoteMap_t *)top10Maps.head;
					qboolean thisMapAlreadyChosen, suitableMapFound = qfalse;

					do {
						thisMapAlreadyChosen = qfalse;

						for (int j = 0; j < numMapsPickedTotal; j++) {
							if (!Q_stricmp(addMap->mapFilename, chosenMapNames[j])) {
								thisMapAlreadyChosen = qtrue;
								break;
							}
						}

						if (!thisMapAlreadyChosen) { // make sure the map we are going to sub in wasn't already rerolled out
							rememberedMultivoteMap_t *addMapRemembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, addMap->mapFilename, NULL);
							qboolean addMapIsRememberedToBeForceExcluded = !!(addMapRemembered && !addMapRemembered->forceInclude);
							if (addMapIsRememberedToBeForceExcluded)
								thisMapAlreadyChosen = qtrue;
						}

						// If the current map is already chosen and there's a next map, move to the next map
						if (thisMapAlreadyChosen && addMap->node.next) {
							addMap = addMap->node.next;
						}
						else {
							// If the map isn't chosen, update newMapFound and break out of the loop
							if (!thisMapAlreadyChosen) {
								suitableMapFound = qtrue;
							}
							break; // Break out of the loop in any case
						}
					} while (1); // Keep looping until we break out


					if (suitableMapFound) {
						if (g_vote_tierlist_debug.integer) {
							char newShortName[MAX_QPATH] = { 0 }, oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(addMap->mapFilename, newShortName, sizeof(newShortName));
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_tierlist_fixShittyPools: swapping in top 10 map %s in place of %s\n",
								newShortName, oldShortName);
						}

						Q_strncpyz(chosenMapNames[i], addMap->mapFilename, sizeof(chosenMapNames[i]));
					}
					else {
						if (g_vote_tierlist_debug.integer) {
							char oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_tierlist_fixShittyPools: no suitable top 10 map to sub in for %s!\n", oldShortName);
						}
					}

					break;
				}
			}
		}
		else if (!numTop6MapsChosen && !numTop10MapsChosen) {
			// change one map to a top 6 map and another to a top 10 map
			if (top6Maps.size >= 1 && top10Maps.size >= 2) {
				qboolean canTryTop6Map = qtrue;
				int numFixed = 0;
				for (int i = numMapsPickedTotal - 1; i >= 0; i--) {
					rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, chosenMapNames[i], NULL);
					qboolean isForceIncludedMap = !!(remembered && remembered->forceInclude);

					if (isForceIncludedMap)
						continue;

					if (!numFixed && canTryTop6Map) {
						rememberedMultivoteMap_t *addMap = (rememberedMultivoteMap_t *)top6Maps.head;
						qboolean thisMapAlreadyChosen, suitableMapFound = qfalse;

						do {
							thisMapAlreadyChosen = qfalse;

							for (int j = 0; j < numMapsPickedTotal; j++) {
								if (!Q_stricmp(addMap->mapFilename, chosenMapNames[j])) {
									thisMapAlreadyChosen = qtrue;
									break;
								}
							}

							if (!thisMapAlreadyChosen) { // make sure the map we are going to sub in wasn't already rerolled out
								rememberedMultivoteMap_t *addMapRemembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, addMap->mapFilename, NULL);
								qboolean addMapIsRememberedToBeForceExcluded = !!(addMapRemembered && !addMapRemembered->forceInclude);
								if (addMapIsRememberedToBeForceExcluded)
									thisMapAlreadyChosen = qtrue;
							}

							// If the current map is already chosen and there's a next map, move to the next map
							if (thisMapAlreadyChosen && addMap->node.next) {
								addMap = addMap->node.next;
							}
							else {
								// If the map isn't chosen, update newMapFound and break out of the loop
								if (!thisMapAlreadyChosen) {
									suitableMapFound = qtrue;
								}
								break; // Break out of the loop in any case
							}
						} while (1); // Keep looping until we break out

						if (suitableMapFound) {
							if (g_vote_tierlist_debug.integer) {
								char newShortName[MAX_QPATH] = { 0 }, oldShortName[MAX_QPATH] = { 0 };
								GetShortNameForMapFileName(addMap->mapFilename, newShortName, sizeof(newShortName));
								GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
								G_LogPrintf("g_vote_tierlist_fixShittyPools: swapping in top 6 map %s in place of %s\n",
									newShortName, oldShortName);
							}

							Q_strncpyz(chosenMapNames[i], addMap->mapFilename, sizeof(chosenMapNames[i]));
							++numFixed;
						}
						else {
							if (g_vote_tierlist_debug.integer) {
								char oldShortName[MAX_QPATH] = { 0 };
								GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
								G_LogPrintf("g_vote_tierlist_fixShittyPools: no suitable top 6 map to sub in for %s!\n", oldShortName);
							}
							// since there was no top 6 map, try subbing in two top 10 maps instead
							i = numMapsPickedTotal; // the for loop will decrement this to numMapsPickedTotal - 1, so it's okay
							canTryTop6Map = qfalse;
						}

						// break;
					}
					else if (numFixed < 2) {
						rememberedMultivoteMap_t *addMap = (rememberedMultivoteMap_t *)top10Maps.head;
						qboolean thisMapAlreadyChosen, suitableMapFound = qfalse;

						do {
							thisMapAlreadyChosen = qfalse;

							for (int j = 0; j < numMapsPickedTotal; j++) {
								if (!Q_stricmp(addMap->mapFilename, chosenMapNames[j])) {
									thisMapAlreadyChosen = qtrue;
									break;
								}
							}

							if (!thisMapAlreadyChosen) { // make sure the map we are going to sub in wasn't already rerolled out
								rememberedMultivoteMap_t *addMapRemembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, addMap->mapFilename, NULL);
								qboolean addMapIsRememberedToBeForceExcluded = !!(addMapRemembered && !addMapRemembered->forceInclude);
								if (addMapIsRememberedToBeForceExcluded)
									thisMapAlreadyChosen = qtrue;
							}

							// If the current map is already chosen and there's a next map, move to the next map
							if (thisMapAlreadyChosen && addMap->node.next) {
								addMap = addMap->node.next;
							}
							else {
								// If the map isn't chosen, update newMapFound and break out of the loop
								if (!thisMapAlreadyChosen) {
									suitableMapFound = qtrue;
								}
								break; // Break out of the loop in any case
							}
						} while (1); // Keep looping until we break out

						if (suitableMapFound) {
							if (g_vote_tierlist_debug.integer) {
								char newShortName[MAX_QPATH] = { 0 }, oldShortName[MAX_QPATH] = { 0 };
								GetShortNameForMapFileName(addMap->mapFilename, newShortName, sizeof(newShortName));
								GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
								G_LogPrintf("g_vote_tierlist_fixShittyPools: swapping in top 10 map %s in place of %s\n",
									newShortName, oldShortName);
							}

							Q_strncpyz(chosenMapNames[i], addMap->mapFilename, sizeof(chosenMapNames[i]));
							++numFixed;
						}
						else {
							if (g_vote_tierlist_debug.integer) {
								char oldShortName[MAX_QPATH] = { 0 };
								GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
								G_LogPrintf("g_vote_tierlist_fixShittyPools: no suitable top 10 map to sub in for %s!\n", oldShortName);
							}
						}

						break;
					}

					//break;
				}
			}
			else if (top10Maps.size >= 2) {
				// edge case: no top 10 maps chosen, and no top 6 maps are available at all
				// try to put in two top 10 maps
				int numFixed = 0;
				for (int i = numMapsPickedTotal - 1; i >= 0; i--) {
					rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, chosenMapNames[i], NULL);
					qboolean isForceIncludedMap = !!(remembered && remembered->forceInclude);

					if (isForceIncludedMap)
						continue;

					if (numFixed < 2) {
						rememberedMultivoteMap_t *addMap = (rememberedMultivoteMap_t *)top10Maps.head;
						qboolean thisMapAlreadyChosen, suitableMapFound = qfalse;

						do {
							thisMapAlreadyChosen = qfalse;

							for (int j = 0; j < numMapsPickedTotal; j++) {
								if (!Q_stricmp(addMap->mapFilename, chosenMapNames[j])) {
									thisMapAlreadyChosen = qtrue;
									break;
								}
							}

							if (!thisMapAlreadyChosen) { // make sure the map we are going to sub in wasn't already rerolled out
								rememberedMultivoteMap_t *addMapRemembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, addMap->mapFilename, NULL);
								qboolean addMapIsRememberedToBeForceExcluded = !!(addMapRemembered && !addMapRemembered->forceInclude);
								if (addMapIsRememberedToBeForceExcluded)
									thisMapAlreadyChosen = qtrue;
							}

							// If the current map is already chosen and there's a next map, move to the next map
							if (thisMapAlreadyChosen && addMap->node.next) {
								addMap = addMap->node.next;
							}
							else {
								// If the map isn't chosen, update newMapFound and break out of the loop
								if (!thisMapAlreadyChosen) {
									suitableMapFound = qtrue;
								}
								break; // Break out of the loop in any case
							}
						} while (1); // Keep looping until we break out

						if (suitableMapFound) {
							if (g_vote_tierlist_debug.integer) {
								char newShortName[MAX_QPATH] = { 0 }, oldShortName[MAX_QPATH] = { 0 };
								GetShortNameForMapFileName(addMap->mapFilename, newShortName, sizeof(newShortName));
								GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
								G_LogPrintf("g_vote_tierlist_fixShittyPools: swapping in top 10 map %s in place of %s\n",
									newShortName, oldShortName);
							}

							Q_strncpyz(chosenMapNames[i], addMap->mapFilename, sizeof(chosenMapNames[i]));
							++numFixed;
						}
						else {
							if (g_vote_tierlist_debug.integer) {
								char oldShortName[MAX_QPATH] = { 0 };
								GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
								G_LogPrintf("g_vote_tierlist_fixShittyPools: no suitable top 10 map to sub in for %s!\n", oldShortName);
							}
						}

						//break;
					}

					//break;
				}
			}
		}

		// ensure there are at least 3 top 10 maps selected
		if (g_vote_tierlist_fixShittyPools.integer == 2 && numMapsPickedTotal >= 6) {
			int numTop10MapsChosen = 0;

			// count the number of top 10 maps chosen
			for (int i = 0; i < numMapsPickedTotal; i++) {
				qboolean isTop10Map = !!(ListFind(&top10Maps, RememberedMapMatches, chosenMapNames[i], NULL));
				if (isTop10Map) {
					numTop10MapsChosen++;
				}
			}

			// if fewer than 3 top 10 maps were chosen, replace a non-top10/top6 map with a top 10 map
			if (numTop10MapsChosen < 3 && top10Maps.size >= 1) {
				for (int i = numMapsPickedTotal - 1; i >= 0; i--) {
					qboolean isTop10Map = !!(ListFind(&top10Maps, RememberedMapMatches, chosenMapNames[i], NULL));
					qboolean isTop6Map = !!(ListFind(&top6Maps, RememberedMapMatches, chosenMapNames[i], NULL));
					rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, chosenMapNames[i], NULL);
					qboolean isForceIncludedMap = !!(remembered && remembered->forceInclude);

					if (isTop10Map || isTop6Map || isForceIncludedMap)
						continue;

					rememberedMultivoteMap_t *addMap = (rememberedMultivoteMap_t *)top10Maps.head;
					qboolean thisMapAlreadyChosen, suitableMapFound = qfalse;

					do {
						thisMapAlreadyChosen = qfalse;

						for (int j = 0; j < numMapsPickedTotal; j++) {
							if (!Q_stricmp(addMap->mapFilename, chosenMapNames[j])) {
								thisMapAlreadyChosen = qtrue;
								break;
							}
						}

						if (!thisMapAlreadyChosen) { // make sure the map we are going to sub in wasn't already rerolled out
							rememberedMultivoteMap_t *addMapRemembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, addMap->mapFilename, NULL);
							qboolean addMapIsRememberedToBeForceExcluded = !!(addMapRemembered && !addMapRemembered->forceInclude);
							if (addMapIsRememberedToBeForceExcluded)
								thisMapAlreadyChosen = qtrue;
						}

						// if the current map is already chosen and there's a next map, move to the next map
						if (thisMapAlreadyChosen && addMap->node.next) {
							addMap = addMap->node.next;
						}
						else {
							// if the map isn't chosen, update newMapFound and break out of the loop
							if (!thisMapAlreadyChosen) {
								suitableMapFound = qtrue;
							}
							break; // break out of the loop in any case
						}
					} while (1); // keep looping until we break out

					if (suitableMapFound) {
						if (g_vote_tierlist_debug.integer) {
							char newShortName[MAX_QPATH] = { 0 }, oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(addMap->mapFilename, newShortName, sizeof(newShortName));
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_tierlist_fixShittyPools: ensuring at least 3 top 10 maps by swapping in top 10 map %s in place of %s\n",
								newShortName, oldShortName);
						}

						Q_strncpyz(chosenMapNames[i], addMap->mapFilename, sizeof(chosenMapNames[i]));
						numTop10MapsChosen++;
						if (numTop10MapsChosen >= 3) {
							break;
						}
					}
					else {
						if (g_vote_tierlist_debug.integer) {
							char oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_tierlist_fixShittyPools: no suitable top 10 map to sub in for %s!\n", oldShortName);
						}
					}
				}
			}
		}

	}

	// force a beta map to be included among the choices by replacing one of the selected maps
	if (g_vote_betaMapForceInclude.string[0] && g_vote_betaMapForceInclude.string[0] != '0' && numMapsPickedTotal > 0) {
#define MAX_BETA_MAPS	(256)
		char betaMaps[MAX_BETA_MAPS][MAX_QPATH];
		int numBetaMaps = 0;

		// split the cvar into individual map names
		char *betaMapsSplit = strdup(g_vote_betaMapForceInclude.string);
		char *token = strtok(betaMapsSplit, ",");
		while (token != NULL && numBetaMaps < MAX_BETA_MAPS) {
			Q_strncpyz(betaMaps[numBetaMaps], token, sizeof(betaMaps[numBetaMaps]));
			numBetaMaps++;
			token = strtok(NULL, ",");
		}
		free(betaMapsSplit);

		// randomize the order
		FisherYatesShuffle(betaMaps, numBetaMaps, sizeof(betaMaps[0]));

		// iterate through the shuffled list of beta maps and select the first one that is available
		for (int i = 0; i < numBetaMaps; i++) {
			char *selectedBetaMap = betaMaps[i];
			if (!MapExistsQuick(betaMaps[i]))
				continue;

			// initial check 1: check if this one is already among the choices and thus doesn't need to be subbed in
			qboolean betaMapAlreadyIncluded = qfalse;
			for (int i = 0; i < numMapsPickedTotal; i++) {
				if (!Q_stricmp(chosenMapNames[i], selectedBetaMap)) {
					betaMapAlreadyIncluded = qtrue;
					break;
				}

				// also check the live version of each choice
				char liveFilename[MAX_QPATH] = { 0 };
				G_DBGetLiveMapNameForMapName(chosenMapNames[i], liveFilename, sizeof(liveFilename));
				if (liveFilename[0] && !Q_stricmp(liveFilename, selectedBetaMap)) {
					betaMapAlreadyIncluded = qtrue;
					break;
				}
			}

			// initial check 2: make sure this one wasn't already rerolled out
			qboolean betaMapIsRememberedToBeForceExcluded = qfalse;
			rememberedMultivoteMap_t *betaMapRemembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, selectedBetaMap, NULL);
			if (betaMapRemembered && !betaMapRemembered->forceInclude)
				betaMapIsRememberedToBeForceExcluded = qtrue;

			// initial check 3: make sure this one isn't on cooldown
			qboolean betaMapIsOnCooldown = qfalse;
			if (g_vote_mapCooldownMinutes.integer > 0) {
				trap_sqlite3_reset(statement);
				trap_sqlite3_prepare_v2(dbPtr, "SELECT ?1 AS map WHERE NOT EXISTS (SELECT 1 FROM lastplayedmaporalias WHERE map = ?1) OR EXISTS (SELECT 1 FROM lastplayedmaporalias WHERE map = ?1 AND (strftime('%s', 'now') - lastplayedmaporalias.datetime > ?2));", -1, &statement, 0);
				sqlite3_bind_text(statement, 1, selectedBetaMap, -1, SQLITE_STATIC);
				sqlite3_bind_int(statement, 2, cooldownSeconds);
				int rc = trap_sqlite3_step(statement);
				betaMapIsOnCooldown = !(rc == SQLITE_ROW);
			}

			
			if (g_vote_tierlist_debug.integer) {
				char betaShortName[MAX_QPATH] = { 0 };
				GetShortNameForMapFileName(selectedBetaMap, betaShortName, sizeof(betaShortName));
				G_LogPrintf("g_vote_betaMapForceInclude: for beta map %s: betaMapAlreadyIncluded %d, betaMapIsRememberedToBeForceExcluded %d, betaMapIsOnCooldown %d\n",
					betaShortName, (int)betaMapAlreadyIncluded, (int)betaMapIsRememberedToBeForceExcluded, (int)betaMapIsOnCooldown);
			}

			qboolean didReplacement = qfalse;
			if (!betaMapAlreadyIncluded && !betaMapIsRememberedToBeForceExcluded && !betaMapIsOnCooldown) {
				// if we got here, we have a valid beta map and we're going to replace one of the chosen maps with it

				// loop through the selected maps in three passes:
				// first pass: can replace anything that isn't a top 10 map
				// second pass: can replace anything that isn't a top 6 map
				// third pass: can replace anything
				// this means we avoid replacing top maps if possible but if it's not possible then we replace anyway
				for (int pass = 0; pass < 3 && !didReplacement; pass++) {
					for (int i = numMapsPickedTotal - 1; i >= 0 && !didReplacement; i--) {
						// skip over maps that survived rerolls (is this necessary?)
						rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, chosenMapNames[i], NULL);
						if (remembered && remembered->forceInclude)
							continue;

						// skip top maps if possible
						qboolean isTop6Map = !!(ListFind(&top6Maps, RememberedMapMatches, chosenMapNames[i], NULL));
						qboolean isTop10Map = !!(ListFind(&top10Maps, RememberedMapMatches, chosenMapNames[i], NULL));
						if (pass == 0 && (isTop6Map || isTop10Map))
							continue;
						if (pass == 1 && isTop6Map)
							continue;

						// replace this map
						if (g_vote_tierlist_debug.integer) {
							char newShortName[MAX_QPATH] = { 0 }, oldShortName[MAX_QPATH] = { 0 };
							GetShortNameForMapFileName(selectedBetaMap, newShortName, sizeof(newShortName));
							GetShortNameForMapFileName(chosenMapNames[i], oldShortName, sizeof(oldShortName));
							G_LogPrintf("g_vote_betaMapForceInclude: on pass %d, swapping in map %s in place of %s\n", pass, newShortName, oldShortName);
						}

						Q_strncpyz(chosenMapNames[i], selectedBetaMap, sizeof(chosenMapNames[i]));
						didReplacement = qtrue;
					}
				}
			}

			if (didReplacement)
				break;
		}
	}

	ListClear(&top6Maps);
	ListClear(&top10Maps);

	trap_sqlite3_finalize(statement);
	
	// fisher-yates shuffle so that the maps don't appear in order of their tiers,
	// preventing people from simply being biased toward the maps appearing at the top
	for (int i = numMapsPickedTotal - 1; i >= 1; i--) {
		int j = rand() % (i + 1);
		char *temp = strdup(chosenMapNames[i]);
		Q_strncpyz(chosenMapNames[i], chosenMapNames[j], sizeof(chosenMapNames[i]));
		Q_strncpyz(chosenMapNames[j], temp, sizeof(chosenMapNames[j]));
		free(temp);
	}

	// ensure ctf4 is always in the 4th slot because logic
	if (numMapsPickedTotal >= 4) {
		for (int i = 0; i < numMapsPickedTotal; i++) {
			if (i == 3)
				continue;
			if (!Q_stricmp(chosenMapNames[i], "mp/ctf4") && chosenMapNames[3][0]) {
				Q_strncpyz(chosenMapNames[i], chosenMapNames[3], sizeof(chosenMapNames[i]));
				Q_strncpyz(chosenMapNames[3], "mp/ctf4", sizeof(chosenMapNames[3]));
				break;
			}
		}
	}

	// try to put the force included map into its previous slot (trumps ctf4 check)
	// this is important because if someone's map survives the reroll, they will auto vote for that map (that slot) again after the reroll
	if (level.mapsRerolled && level.rememberedMultivoteMapsList.size > 0) {
		for (int i = 0; i < numMapsPickedTotal; i++) {
			rememberedMultivoteMap_t *remembered = ListFind(&level.rememberedMultivoteMapsList, RememberedMapMatches, chosenMapNames[i], NULL);
			if (remembered && remembered->forceInclude && remembered->position != i) {
				Q_strncpyz(chosenMapNames[i], chosenMapNames[remembered->position], sizeof(chosenMapNames[i]));
				Q_strncpyz(chosenMapNames[remembered->position], remembered->mapFilename, sizeof(chosenMapNames[remembered->position]));
				break; // maximum of one, so no need to keep going
			}
		}
	}

	// run the callbacks
	for (int i = 0; i < numMapsPickedTotal; i++) {
		if (chosenMapNames[i][0])
			callback(context, chosenMapNames[i]);
	}

	ListClear(allMapsList);
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

	trap_sqlite3_prepare_v2(dbPtr, sqlAddMapToPlayedMapsList, -1, &statement, 0);

	sqlite3_bind_text(statement, 1, lowercase, -1, 0);

	int rc = trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);

	return !!(rc == SQLITE_DONE);
}

const char *sqlGetNumPlayedSingleMap = "SELECT numPlayed FROM num_played_view WHERE map = ?1;";
int G_DBNumTimesPlayedSingleMap(const char *mapFileName) {
	if (!VALIDSTRING(mapFileName))
		return 0;

	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlGetNumPlayedSingleMap, -1, &statement, 0);
	sqlite3_bind_text(statement, 1, mapFileName, -1, 0);
	int rc = trap_sqlite3_step(statement);

	int numPlayed = 0;
	if (rc == SQLITE_ROW)
		numPlayed = sqlite3_column_int(statement, 0);

	trap_sqlite3_finalize(statement);

	return numPlayed;
}

// =========== TOPAIMS ========================================================

const char *const sqlGetTopaimRecord =
"SELECT DISTINCT capture_time, date, extra                                             \n"
"FROM topaims                                                                          \n"
"WHERE mapname = ?1 AND packname = ?2 AND session_id = ?3;                               ";

// TODO: this can be simplified with one UPSERT statement once linux sqlite can be updated
const char *const sqlSaveTopaimRecord1 =
"INSERT OR IGNORE INTO topaims(mapname, packname, session_id, capture_time, date, extra, hash)   \n"
"VALUES(?1, ?2, ?3, 0, 0, '', ?4)                                                           ";
const char *const sqlSaveTopaimRecord2 =
"UPDATE topaims SET capture_time = MAX(?1, capture_time), date = ?2, extra = ?3, hash = ?4                           \n"
"WHERE mapname = ?5 AND packname = ?6 AND session_id = ?7;                                  ";

const char *const sqlGetTopaimLoggedPersonalBest =
"SELECT DISTINCT rank, capture_time                                                    \n"
"FROM topaims_ranks WHERE mapname = ?1 AND packname = ?2 AND account_id = ?3;                ";

const char *const sqlListTopaimsLoggedRecords =
"SELECT name, rank, capture_time, date, extra                                          \n"
"FROM topaims_ranks                                                                    \n"
"WHERE mapname = ?1 AND packname = ?2                                                  \n"
"ORDER BY rank ASC                                                                     \n"
"LIMIT ?3                                                                              \n"
"OFFSET ?4;                                                                              ";

const char *const sqlListTopaimsLoggedTop =
"SELECT DISTINCT mapname, name, capture_time, date FROM topaims_ranks                  \n"
"WHERE packname = ?1 AND rank = 1                                                          \n"
"ORDER BY mapname DESC                                                                  \n"
"LIMIT ?2                                                                              \n"
"OFFSET ?3;                                                                              ";

const char *const sqlListTopaimsLoggedLeaderboard =
"SELECT name, golds, silvers, bronzes                                                  \n"
"FROM topaims_leaderboard                                                              \n"
"WHERE packname = ?1                                                                       \n"
"ORDER BY golds DESC, silvers DESC, bronzes DESC                                       \n"
"LIMIT ?2                                                                              \n"
"OFFSET ?3;                                                                             ";

const char *const sqlListTopaimsLoggedLatest =
"SELECT mapname, packname, name, rank, capture_time, date                              \n"
"FROM topaims_ranks                                                                    \n"
"ORDER BY date DESC                                                                    \n"
"LIMIT ?1                                                                              \n"
"OFFSET ?2;                                                                             ";

static void ReadExtraAimInfo(const char *inJson, aimRecordInfo_t *outInfo) {
	cJSON *root = cJSON_Parse(inJson);

	if (root) {
		cJSON *j;

		j = cJSON_GetObjectItemCaseSensitive(root, "match_id");
		if (j && cJSON_IsString(j))
			Q_strncpyz(outInfo->matchId, j->valuestring, sizeof(outInfo->matchId));
		j = cJSON_GetObjectItemCaseSensitive(root, "player_name");
		if (j && cJSON_IsString(j))
			Q_strncpyz(outInfo->playerName, j->valuestring, sizeof(outInfo->playerName));
		j = cJSON_GetObjectItemCaseSensitive(root, "client_id");
		if (j && cJSON_IsNumber(j))
			outInfo->clientId = j->valueint;
		j = cJSON_GetObjectItemCaseSensitive(root, "start_time");
		if (j && cJSON_IsNumber(j))
			outInfo->startTime = j->valueint;
		for (int i = 0; i < WP_NUM_WEAPONS; i++) {
			j = cJSON_GetObjectItemCaseSensitive(root, va("hits_%d", i));
			if (j && cJSON_IsNumber(j))
				outInfo->numHitsOfWeapon[i] = j->valueint;
		}
	}

	cJSON_Delete(root);
}

static void WriteExtraAimInfo(const aimRecordInfo_t *inInfo, char **outJson) {
	*outJson = NULL;

	cJSON *root = cJSON_CreateObject();

	if (root) {
		if (VALIDSTRING(inInfo->matchId))
			cJSON_AddStringToObject(root, "match_id", inInfo->matchId);
		if (VALIDSTRING(inInfo->playerName))
			cJSON_AddStringToObject(root, "player_name", inInfo->playerName);
		if (IN_CLIENTNUM_RANGE(inInfo->clientId))
			cJSON_AddNumberToObject(root, "client_id", inInfo->clientId);
		if (inInfo->startTime >= 0)
			cJSON_AddNumberToObject(root, "start_time", inInfo->startTime);
		for (int i = 0; i < WP_NUM_WEAPONS; i++) {
			if (inInfo->numHitsOfWeapon[i] > 0)
				cJSON_AddNumberToObject(root, va("hits_%d", i), inInfo->numHitsOfWeapon[i]);
		}

		*outJson = cJSON_PrintUnformatted(root);
	}

	cJSON_Delete(root);

	static char *emptyJson = "";
	if (!*outJson) {
		*outJson = emptyJson;
	}
}

// returns the account-agnostic session-tied personal best record used for caching
qboolean G_DBTopAimLoadAimRecord(const int sessionId,
	const char *mapname,
	const aimPracticePack_t *pack,
	aimRecord_t *outRecord)
{
	sqlite3_stmt *statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetTopaimRecord, -1, &statement, 0);

	qboolean error = qfalse;

	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, pack->packName, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 3, sessionId);

	memset(outRecord, 0, sizeof(*outRecord));

	rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const int capture_time = sqlite3_column_int(statement, 0);
		const time_t date = sqlite3_column_int64(statement, 1);
		const char *extra = (const char *)sqlite3_column_text(statement, 2);

		outRecord->time = capture_time;
		outRecord->date = date;
		ReadExtraAimInfo(extra, &outRecord->extra);

		rc = trap_sqlite3_step(statement);
	}
	else if (rc != SQLITE_DONE) {
		error = qtrue;
	}

	trap_sqlite3_finalize(statement);

	return !error;
}

// ensures that each unique pack (in terms of gameplay) has a unique identifier in the db
static int GetHashForAimPack(const aimPracticePack_t *pack) {
	assert(pack);

	// create a temporary "pack" and only copy the relevant gameplay parts into it
	// (variant data, mapname, num reps per variant, weapon mode)
	static aimPracticePack_t hashMe = { 0 };
	memset(&hashMe, 0, sizeof(hashMe));
	memcpy(hashMe.variants, pack->variants, sizeof(aimVariant_t) * pack->numVariants);
	Q_strncpyz(hashMe.packName, level.mapname, sizeof(hashMe.packName)); // hax
	hashMe.numRepsPerVariant = pack->numRepsPerVariant;
	hashMe.weaponMode = pack->weaponMode;
	hashMe.maxSpeed = pack->maxSpeed;

	int hash;
	if (hashMe.maxSpeed)
		hash = (int)XXH32(&hashMe, sizeof(aimPracticePack_t), 0x69420);
	else
		hash = (int)XXH32(&hashMe, sizeof(aimPracticePack_t) - sizeof(hashMe.maxSpeed), 0x69420);

	return hash;
}

// saves an account-agnostic session-tied personal best record
qboolean G_DBTopAimSaveAimRecord(const int sessionId,
	const char *mapname,
	const aimRecord_t *inRecord,
	const aimPracticePack_t *pack)
{
	sqlite3_stmt *statement1;
	sqlite3_stmt *statement2;

	// force saving in lowercase
	char *mapnameLowercase = strdup(mapname);
	Q_strlwr(mapnameLowercase);

	sqlite3_stmt *getHashStatement;
	int realHash = GetHashForAimPack(pack);
	trap_sqlite3_prepare_v2(dbPtr, "SELECT hash FROM aimpacks WHERE mapname = ? AND name = ?;", -1, &getHashStatement, 0);
	sqlite3_bind_text(getHashStatement, 1, mapnameLowercase, -1, SQLITE_STATIC);
	sqlite3_bind_text(getHashStatement, 2, pack->packName, -1, SQLITE_STATIC);
	int hashRc = trap_sqlite3_step(getHashStatement);
	int hash;
	if (hashRc == SQLITE_ROW)
		hash = sqlite3_column_int(getHashStatement, 0);
	else
		hash = realHash;
	if (hash != realHash)
		Com_Printf("Warning: hash for %s is different (generated hash is %d, hash in db is %d)\n", pack->packName, realHash, hash);

	trap_sqlite3_prepare_v2(dbPtr, sqlSaveTopaimRecord1, -1, &statement1, 0);
	trap_sqlite3_prepare_v2(dbPtr, sqlSaveTopaimRecord2, -1, &statement2, 0);

	qboolean error = qfalse;

	sqlite3_bind_text(statement1, 1, mapnameLowercase, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement1, 2, pack->packName, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement1, 3, sessionId);
	sqlite3_bind_int(statement1, 4, hash);
	trap_sqlite3_step(statement1);

	char *extra;
	WriteExtraAimInfo(&inRecord->extra, &extra);

	sqlite3_bind_int(statement2, 1, inRecord->time);
	sqlite3_bind_int(statement2, 2, inRecord->date);
	sqlite3_bind_text(statement2, 3, extra, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement2, 4, hash);
	sqlite3_bind_text(statement2, 5, mapnameLowercase, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement2, 6, pack->packName, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement2, 7, sessionId);
	int rc = trap_sqlite3_step(statement2);

	error = rc != SQLITE_DONE;

	free(extra);
	free(mapnameLowercase);

	trap_sqlite3_finalize(getHashStatement);
	trap_sqlite3_finalize(statement1);
	trap_sqlite3_finalize(statement2);

	return !error;
}

// returns the PB for a given map/run type for an account for non NULL arguments (0 for none, -1 for error)
qboolean G_DBTopAimGetAccountPersonalBest(const int accountId,
	const char *mapname,
	const aimPracticePack_t *pack,
	int *outRank,
	int *outTime)
{
	sqlite3_stmt *statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetTopaimLoggedPersonalBest, -1, &statement, 0);

	qboolean errored = qfalse;

	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, pack->packName, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 3, accountId);

	if (outRank)
		*outRank = 0;
	if (outTime)
		*outTime = 0;

	rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const int rank = sqlite3_column_int(statement, 0);
		const int capture_time = sqlite3_column_int(statement, 1);

		if (outRank)
			*outRank = rank;
		if (outTime)
			*outTime = capture_time;

		rc = trap_sqlite3_step(statement);
	}
	else if (rc != SQLITE_DONE) {
		if (outRank)
			*outRank = -1;
		if (outTime)
			*outTime = -1;

		errored = qtrue;
	}

	trap_sqlite3_finalize(statement);

	return !errored;
}

// lists logged in records sorted by rank for a map/type
void G_DBTopAimListAimRecords(const char *mapname,
	const char *packName,
	const pagination_t pagination,
	ListAimRecordsCallback callback,
	void *context)
{
	sqlite3_stmt *statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListTopaimsLoggedRecords, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, packName, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 3, limit);
	sqlite3_bind_int(statement, 4, offset);

	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(statement, 0);
		const int rank = sqlite3_column_int(statement, 1);
		const int capture_time = sqlite3_column_int(statement, 2);
		const time_t date = sqlite3_column_int64(statement, 3);
		const char *extra = (const char *)sqlite3_column_text(statement, 4);

		aimRecord_t record = { 0 };
		record.time = capture_time;
		record.date = date;
		ReadExtraAimInfo(extra, &record.extra);

		callback(context, mapname, packName, rank, name, &record);

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
}

// lists only the best records for a type sorted by mapname
void G_DBTopAimListAimTop(const char *packName,
	const pagination_t pagination,
	ListAimTopCallback callback,
	void *context)
{
	sqlite3_stmt *statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListTopaimsLoggedTop, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_text(statement, 1, packName, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 2, limit);
	sqlite3_bind_int(statement, 3, offset);

	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char *mapname = (const char *)sqlite3_column_text(statement, 0);
		const char *name = (const char *)sqlite3_column_text(statement, 1);
		const int capture_time = sqlite3_column_int(statement, 2);
		const time_t date = sqlite3_column_int64(statement, 3);

		callback(context, mapname, packName, name, capture_time, date);

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
}

// lists logged in leaderboard sorted by golds, silvers, bronzes
void G_DBTopAimListAimLeaderboard(const char *packName,
	const pagination_t pagination,
	ListAimLeaderboardCallback callback,
	void *context)
{
	sqlite3_stmt *statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListTopaimsLoggedLeaderboard, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_text(statement, 1, packName, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 2, limit);
	sqlite3_bind_int(statement, 3, offset);

	int rank = 1 + offset;

	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(statement, 0);
		const int golds = sqlite3_column_int(statement, 1);
		const int silvers = sqlite3_column_int(statement, 2);
		const int bronzes = sqlite3_column_int(statement, 3);

		callback(context, packName, rank++, name, golds, silvers, bronzes);

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
}

// lists logged in latest records across all maps/types
void G_DBTopAimListAimLatest(const pagination_t pagination,
	ListAimLatestCallback callback,
	void *context)
{
	sqlite3_stmt *statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlListTopaimsLoggedLatest, -1, &statement, 0);

	const int limit = pagination.numPerPage;
	const int offset = (pagination.numPage - 1) * pagination.numPerPage;

	sqlite3_bind_int(statement, 1, limit);
	sqlite3_bind_int(statement, 2, offset);

	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char *mapname = (const char *)sqlite3_column_text(statement, 0);
		const char *packname = (const char *)sqlite3_column_text(statement, 1);
		const char *name = (const char *)sqlite3_column_text(statement, 2);
		const int rank = sqlite3_column_int(statement, 3);
		const int capture_time = sqlite3_column_int(statement, 4);
		const time_t date = sqlite3_column_int64(statement, 5);

		callback(context, mapname, packname, rank, name, capture_time, date);

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
}

const char *sqlSavePack = "INSERT OR REPLACE INTO aimpacks (mapname, owner_account_id, name, weaponMode, numVariants, data, numRepsPerVariant, hash, extra) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
qboolean G_DBTopAimSavePack(aimPracticePack_t *pack) {
	assert(pack);
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlSavePack, -1, &statement, 0);
	sqlite3_bind_text(statement, 1, level.mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 2, pack->ownerAccountId);
	sqlite3_bind_text(statement, 3, pack->packName, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 4, pack->weaponMode);
	sqlite3_bind_int(statement, 5, pack->numVariants);
	sqlite3_bind_blob(statement, 6, pack->variants, sizeof(aimVariant_t) * pack->numVariants, SQLITE_STATIC);
	sqlite3_bind_int(statement, 7, pack->numRepsPerVariant);
	sqlite3_bind_int(statement, 8, GetHashForAimPack(pack));

	cJSON *root = cJSON_CreateObject();
	char *str = NULL;
	if (root) {
		if (pack->maxSpeed) {
			cJSON_AddNumberToObject(root, "max_speed", pack->maxSpeed);
			str = cJSON_PrintUnformatted(root);
		}
	}
	cJSON_Delete(root);
	if (VALIDSTRING(str))
		sqlite3_bind_text(statement, 9, str, -1, 0);
	else
		sqlite3_bind_null(statement, 9);

	int rc = trap_sqlite3_step(statement);
	trap_sqlite3_finalize(statement);
	return !!(rc == SQLITE_DONE);
}

static void ReadAimPackExtraInfo(const char *extraIn, aimPracticePack_t *packOut) {
	cJSON *root = VALIDSTRING(extraIn) ? cJSON_Parse(extraIn) : NULL;
	if (root) {
		cJSON *maxSpeed = cJSON_GetObjectItemCaseSensitive(root, "max_speed");
		if (cJSON_IsNumber(maxSpeed)) {
			packOut->maxSpeed = maxSpeed->valueint;
		}
		else {
			packOut->maxSpeed = 0;
		}
	}
	else {
		packOut->maxSpeed = 0;
	}
	cJSON_Delete(root);
}

const char *sqlLoadPacksForMap = "SELECT name, owner_account_id, numVariants, data, numRepsPerVariant, weaponMode, extra FROM aimpacks WHERE mapname = ? ORDER BY dateCreated ASC;";
// IMPORTANT: free the result if not null!
list_t *G_DBTopAimLoadPacks(const char *mapname) {
	if (!VALIDSTRING(mapname))
		return NULL;
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlLoadPacksForMap, -1, &statement, 0);
	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	int rc = trap_sqlite3_step(statement);

	list_t *packList = malloc(sizeof(list_t));
	memset(packList, 0, sizeof(list_t));
	int numGotten = 0;
	while (rc == SQLITE_ROW) {
		aimPracticePack_t *newPack = ListAdd(packList, sizeof(aimPracticePack_t));
		const char *name = (const char *)sqlite3_column_text(statement, 0);
		Q_strncpyz(newPack->packName, name, sizeof(newPack->packName));

		newPack->ownerAccountId = sqlite3_column_int(statement, 1);

		newPack->numVariants = sqlite3_column_int(statement, 2);

		const void *data = sqlite3_column_blob(statement, 3);
		memcpy(newPack->variants, data, sizeof(aimVariant_t) * newPack->numVariants);

		newPack->numRepsPerVariant = sqlite3_column_int(statement, 4);
		newPack->weaponMode = sqlite3_column_int(statement, 5);

		// filter the weapons in case somehow there are invalid weapons in the db record
		newPack->weaponMode &= ~(1 << WP_NONE);
		newPack->weaponMode &= ~(1 << WP_MELEE);
		newPack->weaponMode &= ~(1 << WP_SABER);
		newPack->weaponMode &= ~(1 << WP_BLASTER);
		newPack->weaponMode &= ~(1 << WP_BOWCASTER);
		newPack->weaponMode &= ~(1 << WP_DEMP2);
		newPack->weaponMode &= ~(1 << WP_TRIP_MINE);
		newPack->weaponMode &= ~(1 << WP_DET_PACK);
		newPack->weaponMode &= ~(1 << WP_BRYAR_OLD);
		newPack->weaponMode &= ~(1 << WP_EMPLACED_GUN);
		newPack->weaponMode &= ~(1 << WP_TURRET);

		if (WeaponModeStringFromWeaponMode(newPack->weaponMode)) {
			++numGotten;
		}
		else {
			Com_Printf("Warning: pack %s from the database has invalid weapons %d! Skipping load.\n", newPack->packName, newPack->weaponMode);
			ListRemove(packList, newPack); // wtf? no valid weapons
		}

		const char *extra = (const char *)sqlite3_column_text(statement, 6);
		ReadAimPackExtraInfo(extra, newPack);

		rc = trap_sqlite3_step(statement);
	}

	if (!numGotten) {
		ListClear(packList);
		free(packList);
		return NULL;
	}

	return packList;
}

const char *quickLoadPacks = "SELECT aimpacks.name, accounts.name FROM aimpacks JOIN accounts ON owner_account_id = account_id WHERE mapname = ? ORDER BY dateCreated ASC;";
// IMPORTANT: free the result if not null!
list_t *G_DBTopAimQuickLoadPacks(const char *mapname) {
	if (!VALIDSTRING(mapname))
		return NULL;

	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, quickLoadPacks, -1, &statement, 0);
	sqlite3_bind_text(statement, 1, mapname, -1, SQLITE_STATIC);
	int rc = trap_sqlite3_step(statement);

	list_t *packList = malloc(sizeof(list_t));
	memset(packList, 0, sizeof(list_t));
	int numGotten = 0;
	while (rc == SQLITE_ROW) {
		aimPracticePackMetaData_t *data = ListAdd(packList, sizeof(aimPracticePackMetaData_t));
		const char *packName = (const char *)sqlite3_column_text(statement, 0);
		Q_strncpyz(data->packName, packName, sizeof(data->packName));

		const char *ownerName = (const char *)sqlite3_column_text(statement, 1);
		Q_strncpyz(data->ownerAccountName, ownerName, sizeof(data->ownerAccountName));

		++numGotten;
		rc = trap_sqlite3_step(statement);
	}

	if (!numGotten) {
		ListClear(packList);
		free(packList);
		return NULL;
	}

	return packList;
}

const char *sqlDeletePack = "DELETE FROM aimpacks WHERE mapname = ? AND name = ?;";
qboolean G_DBTopAimDeletePack(aimPracticePack_t *pack) {
	if (!pack)
		return qfalse;

	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlDeletePack, -1, &statement, 0);
	sqlite3_bind_text(statement, 1, level.mapname, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, pack->packName, -1, SQLITE_STATIC);
	trap_sqlite3_step(statement);
	qboolean success = sqlite3_changes(dbPtr) != 0;
	trap_sqlite3_finalize(statement);
	return success;
}

typedef struct {
	node_t			node;
	int				sessionId;
	qboolean		isBot;
	stats_t			*stats;
	team_t			team;
	ctfPosition_t	pos;
} gotPlayerWithPos_t;

static qboolean PlayerMatchesWithPos(genericNode_t *node, void *userData) {
	const gotPlayerWithPos_t *existing = (const gotPlayerWithPos_t *)node;
	const stats_t *thisGuy = (const stats_t *)userData;

	if (existing && thisGuy && thisGuy->sessionId == existing->sessionId && thisGuy->isBot == existing->isBot && thisGuy->lastTeam == existing->team && thisGuy->finalPosition == existing->pos)
		return qtrue;

	return qfalse;
}

qboolean FinishedPugPlayerMatches(genericNode_t *node, void *userData) {
	const finishedPugPlayer_t *existing = (const finishedPugPlayer_t *)node;
	const int thisGuyAccountId = *((const int *)userData);
	if (existing && existing->accountId == thisGuyAccountId)
		return qtrue;
	return qfalse;
}

const char *sqlWritePug = "INSERT INTO pugs (match_id, map, duration, boonexists, win_team, red_score, blue_score) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);";
const char *sqlAddPugPlayer = "INSERT INTO playerpugteampos (match_id, session_id, team, duration, name, pos, cap, ass, def, acc, air, tk, take, pitkil, pitdth, dmg, fcdmg, clrdmg, othrdmg, dmgtkn, fcdmgtkn, clrdmgtkn, othrdmgtkn, fckil, fckileff, ret, sk, ttlhold, maxhold, avgspd, topspd, boon, push, pull, heal, te, teeff, enemynrg, absorb, protdmg, prottime, rage, drain, drained, fs, bas, mid, eba, efs, gotte, grips, gripped, dark) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
extern void AddStatsToTotal(stats_t *player, stats_t *total, statsTableType_t type, stats_t *weaponStatsPtr);
qboolean G_DBWritePugStats(void) {
	// get the match id
	const char *matchIdStr = Cvar_VariableString("sv_matchid");
	if (!VALIDSTRING(matchIdStr)) {
		assert(qfalse);
		Com_Printf("Unable to get sv_matchid! Stats will not be written for this pug.\n");
		return qfalse;
	}
	int64_t matchId = strtoll(Cvar_VariableString("sv_matchid"), NULL, 16);

	// write the pug
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlWritePug, -1, &statement, 0);
	sqlite3_bind_int64(statement, 1, matchId);
	sqlite3_bind_text(statement, 2, level.mapname, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, 3, level.intermissiontime - level.startTime);
	sqlite3_bind_int(statement, 4, level.boonExists ? 1 : 0);
	int winTeam;
	if (level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE])
		winTeam = TEAM_RED;
	else if (level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED])
		winTeam = TEAM_BLUE;
	else
		winTeam = 0;
	sqlite3_bind_int(statement, 5, winTeam);
	sqlite3_bind_int(statement, 6, level.teamScores[TEAM_RED]);
	sqlite3_bind_int(statement, 7, level.teamScores[TEAM_BLUE]);
	int rc = trap_sqlite3_step(statement);
	if (rc != SQLITE_DONE) {
		trap_sqlite3_finalize(statement);
		Com_Printf("Failed to write pug with id %llx (%lld) to db! Stats will not be written for this pug.\n", matchId, matchId);
		return qfalse;
	}

	Com_Printf("Writing pug with id %llx (%lld) to db\n", matchId, matchId);

	// get each unique player+pos+team combination
	int totalBlocks = 0, totalPlayerPositions = 0, playerPositionsFailed = 0;
	list_t gotPlayerAtPosOnTeamList = { 0 }, combinedStatsList = { 0 };
	qboolean shouldReloadRust = qfalse;
	for (int i = 0; i < 2; i++) {
		iterator_t iter;
		ListIterate(!i ? &level.savedStatsList : &level.statsList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			stats_t *found = IteratorNext(&iter);

			if (!StatsValid(found) || found->isBot || found->lastTeam == TEAM_SPECTATOR || found->lastTeam == TEAM_FREE)
				continue;

			gotPlayerWithPos_t *gotPlayerAlready = ListFind(&gotPlayerAtPosOnTeamList, PlayerMatchesWithPos, found, NULL);
			if (gotPlayerAlready)
				continue;
			gotPlayerWithPos_t *add = ListAdd(&gotPlayerAtPosOnTeamList, sizeof(gotPlayerWithPos_t));
			add->stats = found;
			add->sessionId = found->sessionId;
			add->isBot = found->isBot;
			add->team = found->lastTeam;
			add->pos = found->finalPosition;

			stats_t *s = ListAdd(&combinedStatsList, sizeof(stats_t));
			AddStatsToTotal(found, s, STATS_TABLE_GENERAL, NULL);
			AddStatsToTotal(found, s, STATS_TABLE_FORCE, NULL);
			AddStatsToTotal(found, s, STATS_TABLE_ACCURACY, NULL);
			Q_strncpyz(s->name, found->name, sizeof(s->name));
			int ticksOnAnyPosition = s->ticksNotPaused = found->ticksNotPaused;
			s->blockNum = found->blockNum;

			// find all blocks that match this player+pos+team
			int numBlocks = 1;
			for (int i = 0; i < 2; i++) {
				iterator_t iter2;
				ListIterate(!i ? &level.savedStatsList : &level.statsList, &iter2, qfalse);
				while (IteratorHasNext(&iter2)) {
					stats_t *found2 = IteratorNext(&iter2);
					if (!StatsValid(found2) || found2->isBot || found2 == found ||
						found2->sessionId != found->sessionId || found2->lastTeam != found->lastTeam)
						continue;

					ticksOnAnyPosition += found2->ticksNotPaused;

					if (DetermineCTFPosition(found2, qfalse) != found->finalPosition)
						continue;

					if (found2->blockNum > s->blockNum) {
						// try to use their most recent name
						Q_strncpyz(s->name, found2->name, sizeof(s->name));
						s->blockNum = found2->blockNum;
					}
					AddStatsToTotal(found2, s, STATS_TABLE_GENERAL, NULL);
					AddStatsToTotal(found2, s, STATS_TABLE_FORCE, NULL);
					AddStatsToTotal(found2, s, STATS_TABLE_ACCURACY, NULL);
					s->ticksNotPaused += found2->ticksNotPaused;

					++numBlocks;
				}
			}

			// require at least 120 seconds of total ingame time
			if (ticksOnAnyPosition * (1000 / g_svfps.integer) < CTFPOSITION_MINIMUM_SECONDS * 1000) {
				Com_Printf("Skipping %d %s %s blocks (%d ms) for %s^7 because total time played was less than %d ms\n",
					numBlocks,
					found->lastTeam == TEAM_RED ? "red" : "blue",
					NameForPos(found->finalPosition),
					s->ticksNotPaused * (1000 / g_svfps.integer),
					s->name,
					CTFPOSITION_MINIMUM_SECONDS * 1000);
				continue;
			}
			
			// account-related stuff
			session_t session = { 0 };
			G_DBGetSessionByID(found->sessionId, &session);
			int accountId = session.accountId;
			if (accountId != ACCOUNT_ID_UNLINKED) {
				// rust
				if (!shouldReloadRust && IsPlayerRusty(accountId)) {
					G_DBSetMetadata("shouldReloadRust", "1");
					shouldReloadRust = qtrue;
				}

				// winstreaks
				if (level.teamScores[TEAM_RED] != level.teamScores[TEAM_BLUE] && g_gametype.integer >= GT_TEAM && g_gametype.integer) {
					finishedPugPlayer_t *finished = ListFind(&level.finishedPugPlayersList, FinishedPugPlayerMatches, &accountId, NULL);
					const qboolean won = (found->lastTeam == WinningTeam());
					if (finished) {
						if (finished->won != won) {
							// we already logged that this guy won/lost the match, and now we would be logging the opposite result
							// this means the guy played on both teams in this match
							finished->invalidBecausePlayedOnBothTeams = qtrue;
						}
					}
					else {
						finished = ListAdd(&level.finishedPugPlayersList, sizeof(finishedPugPlayer_t));
						finished->accountId = accountId;
						finished->won = won;
						account_t account = { 0 };
						G_DBGetAccountByID(accountId, &account);
						if (account.name[0])
							Q_strncpyz(finished->accountName, account.name, sizeof(finished->accountName));
					}
				}
			}

			trap_sqlite3_reset(statement);
			trap_sqlite3_prepare_v2(dbPtr, sqlAddPugPlayer, -1, &statement, 0);
			int num = 1;
			sqlite3_bind_int64(statement, num++, matchId);
			sqlite3_bind_int(statement, num++, found->sessionId);
			sqlite3_bind_int(statement, num++, found->lastTeam);
			sqlite3_bind_int(statement, num++, s->ticksNotPaused * (1000 / g_svfps.integer));
			sqlite3_bind_text(statement, num++, s->name, -1, SQLITE_STATIC);
			sqlite3_bind_int(statement, num++, found->finalPosition);
			sqlite3_bind_int(statement, num++, s->captures);
			sqlite3_bind_int(statement, num++, s->assists);
			sqlite3_bind_int(statement, num++, s->defends);
			sqlite3_bind_int(statement, num++, s->accuracy);
			sqlite3_bind_int(statement, num++, s->airs);
			sqlite3_bind_int(statement, num++, s->teamKills);
			sqlite3_bind_int(statement, num++, s->takes);
			sqlite3_bind_int(statement, num++, s->pits);
			sqlite3_bind_int(statement, num++, s->pitted);
			sqlite3_bind_int(statement, num++, s->damageDealtTotal);
			sqlite3_bind_int(statement, num++, s->flagCarrierDamageDealtTotal);
			sqlite3_bind_int(statement, num++, s->clearDamageDealtTotal);
			sqlite3_bind_int(statement, num++, s->otherDamageDealtTotal);
			sqlite3_bind_int(statement, num++, s->damageTakenTotal);
			sqlite3_bind_int(statement, num++, s->flagCarrierDamageTakenTotal);
			sqlite3_bind_int(statement, num++, s->clearDamageTakenTotal);
			sqlite3_bind_int(statement, num++, s->otherDamageTakenTotal);
			sqlite3_bind_int(statement, num++, s->fcKills);
			if (s->fcKills)
				sqlite3_bind_int(statement, num++, s->fcKillEfficiency);
			else
				sqlite3_bind_null(statement, num++);
			sqlite3_bind_int(statement, num++, s->rets);
			sqlite3_bind_int(statement, num++, s->selfkills);
			sqlite3_bind_int(statement, num++, s->totalFlagHold / 1000);
			sqlite3_bind_int(statement, num++, s->longestFlagHold / 1000);
			sqlite3_bind_int(statement, num++, s->averageSpeed);
			sqlite3_bind_int(statement, num++, s->topSpeed);
			if (level.boonExists)
				sqlite3_bind_int(statement, num++, s->boonPickups);
			else
				sqlite3_bind_null(statement, num++);
			sqlite3_bind_int(statement, num++, s->push);
			sqlite3_bind_int(statement, num++, s->pull);
			sqlite3_bind_int(statement, num++, s->healed);
			sqlite3_bind_int(statement, num++, s->energizedAlly);
			if (s->numEnergizes)
				sqlite3_bind_int(statement, num++, s->energizeEfficiency);
			else
				sqlite3_bind_null(statement, num++);
			sqlite3_bind_int(statement, num++, s->energizedEnemy);
			sqlite3_bind_int(statement, num++, s->absorbed);
			sqlite3_bind_int(statement, num++, s->protDamageAvoided);
			sqlite3_bind_int(statement, num++, s->protTimeUsed / 1000);
			sqlite3_bind_int(statement, num++, s->rageTimeUsed / 1000);
			sqlite3_bind_int(statement, num++, s->drain);
			sqlite3_bind_int(statement, num++, s->gotDrained);
			qboolean hasValidRegions = qfalse;
			for (ctfRegion_t r = CTFREGION_FLAGSTAND; r <= CTFREGION_ENEMYFLAGSTAND; r++)
				if (s->regionPercent[r]) { hasValidRegions = qtrue; break; }
			if (hasValidRegions) {
				sqlite3_bind_int(statement, num++, s->regionPercent[CTFREGION_FLAGSTAND]);
				sqlite3_bind_int(statement, num++, s->regionPercent[CTFREGION_BASE]);
				sqlite3_bind_int(statement, num++, s->regionPercent[CTFREGION_MID]);
				sqlite3_bind_int(statement, num++, s->regionPercent[CTFREGION_ENEMYBASE]);
				sqlite3_bind_int(statement, num++, s->regionPercent[CTFREGION_ENEMYFLAGSTAND]);
			}
			else {
				sqlite3_bind_null(statement, num++);
				sqlite3_bind_null(statement, num++);
				sqlite3_bind_null(statement, num++);
				sqlite3_bind_null(statement, num++);
				sqlite3_bind_null(statement, num++);
			}
			sqlite3_bind_int(statement, num++, s->gotEnergizedByAlly);
			sqlite3_bind_int(statement, num++, s->grips);
			sqlite3_bind_int(statement, num++, s->gotGripped);
			sqlite3_bind_int(statement, num++, s->darkPercent);

			rc = trap_sqlite3_step(statement);
			if (rc == SQLITE_DONE) {
				totalBlocks += numBlocks;
				++totalPlayerPositions;
				Com_Printf("Wrote %d %s %s blocks (%d ms) for %s^7 to db\n",
					numBlocks,
					found->lastTeam == TEAM_RED ? "red" : "blue",
					NameForPos(found->finalPosition),
					s->ticksNotPaused * (1000 / g_svfps.integer),
					s->name);
			}
			else {
				++playerPositionsFailed;
				Com_Printf("Failed to write %d %s blocks (%d ms) for %s^7 to db!\n",
					numBlocks,
					NameForPos(found->finalPosition),
					s->ticksNotPaused * (1000 / g_svfps.integer),
					s->name);
			}

			if (d_logTEData.integer && s->numEnergizes) {
				const char *sqlLogTeData =
					"INSERT INTO tedata ("
					"    match_id, "
					"    session_id, "
					"    team, "
					"    pos, "
					"    time, "
					"    numEnergizes, "
					"    energizedAlly, "
					"    numTEs, "
					"    numTEsWithLOS, "
					"    amountTE, "
					"    amountTEWithLOS, "
					"    numTEsFC, "
					"    numTEsFCWithLOS, "
					"    amountTEFC, "
					"    amountTEFCWithLOS, "
					"    numTEsChase, "
					"    numTEsChaseWithLOS, "
					"    amountTEChase, "
					"    amountTEChaseWithLOS"
					") VALUES ("
					"    ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?"
					");";
				trap_sqlite3_reset(statement);
				trap_sqlite3_prepare_v2(dbPtr, sqlLogTeData, -1, &statement, 0);
				int teDataNum = 1;
				sqlite3_bind_int64(statement, teDataNum++, matchId);
				sqlite3_bind_int(statement, teDataNum++, found->sessionId);
				sqlite3_bind_int(statement, teDataNum++, found->lastTeam);
				sqlite3_bind_int(statement, teDataNum++, found->finalPosition);
				sqlite3_bind_int(statement, teDataNum++, s->ticksNotPaused * (1000 / g_svfps.integer));
				sqlite3_bind_int(statement, teDataNum++, s->numEnergizes);
				sqlite3_bind_int(statement, teDataNum++, s->energizedAlly);
				sqlite3_bind_int(statement, teDataNum++, s->teData.numTEs);
				sqlite3_bind_int(statement, teDataNum++, s->teData.numTEsWithLOS);
				sqlite3_bind_int(statement, teDataNum++, s->teData.amountTE);
				sqlite3_bind_int(statement, teDataNum++, s->teData.amountTEWithLOS);
				sqlite3_bind_int(statement, teDataNum++, s->teData.numTEsFC);
				sqlite3_bind_int(statement, teDataNum++, s->teData.numTEsFCWithLOS);
				sqlite3_bind_int(statement, teDataNum++, s->teData.amountTEFC);
				sqlite3_bind_int(statement, teDataNum++, s->teData.amountTEFCWithLOS);
				sqlite3_bind_int(statement, teDataNum++, s->teData.numTEsChase);
				sqlite3_bind_int(statement, teDataNum++, s->teData.numTEsChaseWithLOS);
				sqlite3_bind_int(statement, teDataNum++, s->teData.amountTEChase);
				sqlite3_bind_int(statement, teDataNum++, s->teData.amountTEChaseWithLOS);
				rc = trap_sqlite3_step(statement);
				if (rc == SQLITE_DONE)
					Com_Printf("d_logTEData: wrote for %s^7 to db\n", s->name);
				else
					Com_Printf("d_logTEData: failed to write for %s^7 to db!\n", s->name);
			}
		}
	}
	ListClear(&gotPlayerAtPosOnTeamList);
	ListClear(&combinedStatsList);

	trap_sqlite3_finalize(statement);

	Com_Printf("Wrote pug with match id %llx (%lld) to db with %d blocks and %d player+pos+team combinations%s.\n", matchId, matchId, totalBlocks, totalPlayerPositions, playerPositionsFailed ? va(" (%d failures)", playerPositionsFailed) : "");
	return qtrue;
}

typedef struct {
	union {
		int valueInt;
		float valueFloat;
	};
	int rank;
	char name[32];
} stat_t;

typedef struct {
	node_t node;
	stat_t pugsplayed, wins, winrate, cap, ass, def, acc, air, tk, take, pitkil, pitdth, dmg, fcdmg, clrdmg, othrdmg, dmgtkn, fcdmgtkn, clrdmgtkn, othrdmgtkn, fckil, fckileff, ret, sk, ttlhold, maxhold, avgspd, topspd, boon, push, pull, heal, te, teeff, enemynrg, absorb, protdmg, prottime, rage, drain, drained, fs, bas, mid, eba, efs, gotte, grips, gripped, dark;
	int accountId;
	ctfPosition_t pos;
	char name[32];
} accountStats_t;

static char *RankSuffix(int rank) {
	switch (rank % 100) {
	case 11: case 12: case 13: return "th";
	}
	switch (rank % 10) {
	case 1: return "st";
	case 2: return "nd";
	case 3: return "rd";
	default: return "th";
	}
}

const char *PrintPositionStatIntCallback(void *rowContext, void *columnContext) {
	if (!columnContext)
		return NULL;
	stat_t *s = columnContext;
	return va("%d ^9(%d%s)", (int)s->valueInt, s->rank, RankSuffix(s->rank));
}

const char *PrintPositionStatFloatCallback(void *rowContext, void *columnContext) {
	if (!columnContext)
		return NULL;
	stat_t *s = columnContext;
	return va("%0.1f ^9(%d%s)", s->valueFloat, s->rank, RankSuffix(s->rank));
}

const char *PrintPositionStatPercentCallback(void *rowContext, void *columnContext) {
	if (!columnContext)
		return NULL;
	stat_t *s = columnContext;
	return va("%0.2f'/. ^9(%d%s)", s->valueFloat * 100, s->rank, RankSuffix(s->rank));
}

const char *PrintPositionStatTimeCallback(void *rowContext, void *columnContext) {
	if (!columnContext)
		return NULL;
	stat_t *s = columnContext;
	int secs = s->valueInt;
	int mins = secs / 60;
	if (secs >= 60) {
		secs %= 60;
		return va("%dm%02ds ^9(%d%s)", mins, secs, s->rank, RankSuffix(s->rank));
	}
	else {
		return va("%ds ^9(%d%s)", secs, s->rank, RankSuffix(s->rank));
	}
}

#define SetStatInt(field, title) \
do { \
	stats->field.valueInt = (int)round(sqlite3_column_double(statement, num++)); \
	stats->field.rank = sqlite3_column_int(statement, num++); \
	Table_DefineColumn(t, title, PrintPositionStatIntCallback, &stats->field, qfalse, -1, 32); \
} while (0)

#define SetStatFloat(field, title) \
do { \
	stats->field.valueFloat = sqlite3_column_double(statement, num++); \
	stats->field.rank = sqlite3_column_int(statement, num++); \
	Table_DefineColumn(t, title, PrintPositionStatFloatCallback, &stats->field, qfalse, -1, 32); \
} while (0)

#define SetStatPercent(field, title) \
do { \
	stats->field.valueFloat = sqlite3_column_double(statement, num++); \
	stats->field.rank = sqlite3_column_int(statement, num++); \
	Table_DefineColumn(t, title, PrintPositionStatPercentCallback, &stats->field, qfalse, -1, 32); \
} while (0)

#define SetStatTime(field, title) \
do { \
	stats->field.valueInt = (int)round(sqlite3_column_double(statement, num++)); \
	stats->field.rank = sqlite3_column_int(statement, num++); \
	Table_DefineColumn(t, title, PrintPositionStatTimeCallback, &stats->field, qfalse, -1, 32); \
} while (0)

typedef enum {
	CACHEDPLAYERPUGSTATSTYPE_WINRATES = 0,
	CACHEDPLAYERPUGSTATSTYPE_POSITIONSTATS
} cachedPlayerPugStatsType_t;

typedef struct {
	int matchups;
	int wins;
	float winrate;
	int rank;
	char name[32];
} winrate_t;

static qboolean CachedPlayerPugStatsMatches(genericNode_t *node, void *userData) {
	cachedPlayerPugStats_t *existing = (cachedPlayerPugStats_t *)node;
	cachedPlayerPugStats_t *thisOne = (cachedPlayerPugStats_t *)userData;
	if (!existing || existing->accountId != thisOne->accountId || existing->pos != thisOne->pos)
		return qfalse;
	return qtrue;
}

static void LoadPositionStatsFromDatabase(void) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, "SELECT account_id, pos, str FROM [cachedplayerstats] WHERE type = 1;", -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		cachedPlayerPugStats_t *c = ListAdd(&level.cachedPositionStats, sizeof(cachedPlayerPugStats_t));
		c->accountId = sqlite3_column_int(statement, 0);
		c->pos = sqlite3_column_int(statement, 1);
		const char *str = (const char *)sqlite3_column_text(statement, 2);
		c->strPtr = strdup(str);
		rc = trap_sqlite3_step(statement);
	}
	trap_sqlite3_finalize(statement);
}

const char *sqlGetPositionStatsForPlayer = "SELECT account_id, name, pos, pugs_played, pugs_played_rank, wins, wins_rank, winrate, winrate_rank, avg_cap, avg_cap_rank, avg_ass, avg_ass_rank, avg_def, avg_def_rank, avg_acc, avg_acc_rank, avg_air, avg_air_rank, avg_tk, avg_tk_rank, avg_take, avg_take_rank, avg_pitkil, avg_pitkil_rank, avg_pitdth, avg_pitdth_rank, avg_dmg, avg_dmg_rank, avg_fcdmg, avg_fcdmg_rank, avg_clrdmg, avg_clrdmg_rank, avg_othrdmg, avg_othrdmg_rank, avg_dmgtkn, avg_dmgtkn_rank, avg_fcdmgtkn, avg_fcdmgtkn_rank, avg_clrdmgtkn, avg_clrdmgtkn_rank, avg_othrdmgtkn, avg_othrdmgtkn_rank, avg_fckil, avg_fckil_rank, avg_fckileff, avg_fckileff_rank, avg_ret, avg_ret_rank, avg_sk, avg_sk_rank, avg_ttlhold, avg_ttlhold_rank, avg_maxhold, avg_maxhold_rank, avg_avgspd, avg_avgspd_rank, avg_topspd, avg_topspd_rank, avg_boon, avg_boon_rank, avg_push, avg_push_rank, avg_pull, avg_pull_rank, avg_heal, avg_heal_rank, avg_te, avg_te_rank, avg_teeff, avg_teeff_rank, avg_gotte, avg_gotte_rank, avg_enemynrg, avg_enemynrg_rank, avg_absorb, avg_absorb_rank, avg_protdmg, avg_protdmg_rank, avg_prottime, avg_prottime_rank, avg_rage, avg_rage_rank, avg_drain, avg_drain_rank, avg_drained, avg_drained_rank, avg_grips, avg_grips_rank, avg_gripped, avg_gripped_rank, avg_dark, avg_dark_rank, avg_fs, avg_fs_rank, avg_bas, avg_bas_rank, avg_mid, avg_mid_rank, avg_eba, avg_eba_rank, avg_efs, avg_efs_rank FROM accountstats;";
static void RecalculatePositionStats(void) {
	iterator_t iter;
	ListIterate(&level.cachedPositionStats, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		cachedPlayerPugStats_t *c = IteratorNext(&iter);
		if (c->strPtr)
			free(c->strPtr);
	}
	ListClear(&level.cachedPositionStats);
	ListClear(&level.cachedPositionStatsRaw);
	trap_sqlite3_exec(dbPtr, "DELETE FROM [cachedplayerstats] WHERE type = 1;", NULL, NULL, NULL);

	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlGetPositionStatsForPlayer, -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const int accountId = sqlite3_column_int(statement, 0);
		const char *name = (const char *)sqlite3_column_text(statement, 1);
		const ctfPosition_t pos = sqlite3_column_int(statement, 2);

		char *buf = calloc(16384, sizeof(char));
		accountStats_t *stats = ListAdd(&level.cachedPositionStatsRaw, sizeof(accountStats_t));
		stats->accountId = accountId;
		stats->pos = pos;
		Q_strncpyz(stats->name, name, sizeof(stats->name));
		int num = 3;
		Table *t = Table_Initialize(qfalse);
		Table_DefineRow(t, NULL);
		SetStatInt(pugsplayed, "^5Pugs"); SetStatInt(wins, "^5Wins"); SetStatPercent(winrate, "^5Winrate");
		SetStatFloat(cap, "^5Cap"); SetStatFloat(ass, "^5Ass"); SetStatFloat(def, "^5Def"); SetStatFloat(acc, "^5Acc"); SetStatFloat(air, "^5Air"); SetStatFloat(tk, "^5TK"); SetStatFloat(take, "^5Take");
		SetStatFloat(pitkil, "^6Pit"); SetStatFloat(pitdth, "^6Dth");
		Table_WriteToBuffer(t, buf, 16384, qtrue, -1);
		Table_Destroy(t);

		t = Table_Initialize(qfalse);
		Table_DefineRow(t, NULL);
		SetStatFloat(dmg, "^2Dmg"); SetStatFloat(fcdmg, "^2Fc"); SetStatFloat(clrdmg, "^2Clr"); SetStatFloat(othrdmg, "^2Othr");
		SetStatFloat(dmgtkn, "^1Tkn"); SetStatFloat(fcdmgtkn, "^1Fc"); SetStatFloat(clrdmgtkn, "^1Clr"); SetStatFloat(othrdmgtkn, "^1Othr"); SetStatFloat(fckil, "^6FcKil"); SetStatFloat(fckileff, "^6Eff");
		SetStatFloat(ret, "Ret"); SetStatFloat(sk, "SK");
		int len = strlen(buf);
		Table_WriteToBuffer(t, buf + len, 16384 - len, qtrue, -1);
		Table_Destroy(t);

		t = Table_Initialize(qfalse);
		Table_DefineRow(t, NULL);
		SetStatTime(ttlhold, "^3Hold"); SetStatTime(maxhold, "^3Max"); SetStatFloat(avgspd, "^6Spd"); SetStatFloat(topspd, "^6Top");
		SetStatFloat(boon, "^5Boon"); SetStatFloat(push, "^5Push"); SetStatFloat(pull, "^5Pull"); SetStatFloat(heal, "^5Heal"); SetStatFloat(te, "^6TE"); SetStatFloat(teeff, "^6Eff"); SetStatFloat(gotte, "^6Rcvd");
		len = strlen(buf);
		Table_WriteToBuffer(t, buf + len, 16384 - len, qtrue, -1);
		Table_Destroy(t);

		t = Table_Initialize(qfalse);
		Table_DefineRow(t, NULL);
		SetStatFloat(enemynrg, "^5EnemyNrg"); SetStatFloat(absorb, "^5Abs"); SetStatFloat(protdmg, "^2Prot"); SetStatTime(prottime, "^2Time"); SetStatTime(rage, "^5Rage"); SetStatFloat(drain, "^1Drn"); SetStatFloat(drained, "^1Drnd");
		SetStatFloat(grips, "^1Gr"); SetStatFloat(gripped, "^1Gd"); SetStatFloat(dark, "^1Drk");
		len = strlen(buf);
		Table_WriteToBuffer(t, buf + len, 16384 - len, qtrue, -1);
		Table_Destroy(t);

		t = Table_Initialize(qfalse);
		Table_DefineRow(t, NULL);
		SetStatFloat(fs, "^5Fs"); SetStatFloat(bas, "^5Bas"); SetStatFloat(mid, "^5Mid"); SetStatFloat(eba, "^5EBa"); SetStatFloat(efs, "^5EFs");
		len = strlen(buf);
		Table_WriteToBuffer(t, buf + len, 16384 - len, qtrue, -1);
		Table_Destroy(t);

		if (*buf) {
			// add it to the db
			sqlite3_stmt *innerStatement;
			trap_sqlite3_prepare_v2(dbPtr, "INSERT OR REPLACE INTO [cachedplayerstats] (account_id, type, pos, str) VALUES (?, 1, ?, ?);", -1, &innerStatement, 0);
			sqlite3_bind_int(innerStatement, 1, accountId);
			sqlite3_bind_int(innerStatement, 2, pos);
			sqlite3_bind_text(innerStatement, 3, buf, -1, SQLITE_STATIC);
			int rc = trap_sqlite3_step(innerStatement);
			trap_sqlite3_finalize(innerStatement);

			// also load it into the cache
			cachedPlayerPugStats_t *c = ListAdd(&level.cachedPositionStats, sizeof(cachedPlayerPugStats_t));
			c->accountId = accountId;
			c->pos = pos;
			c->strPtr = strdup(buf);
		}

		free(buf);
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
}

qboolean G_DBPrintPositionStatsForPlayer(int accountId, ctfPosition_t pos, int printClientNum, const char *name) {
	cachedPlayerPugStats_t x;
	x.accountId = accountId;
	x.pos = pos;
	cachedPlayerPugStats_t *found = ListFind(&level.cachedPositionStats, CachedPlayerPugStatsMatches, &x, NULL);
	if (found && VALIDSTRING(found->strPtr)) {
		PrintIngame(printClientNum, found->strPtr);
		return qtrue;
	}
	return qfalse;
}

const char *PrintTopPlayersPositionStatIntCallback(void *rowContext, void *columnContext) {
	int index = (int)rowContext;
	stat_t *s = ((stat_t *)columnContext) + index;
	return va("%s: %d ^9(%d%s)", s->name, s->valueInt, s->rank, RankSuffix(s->rank));
}

const char *PrintTopPlayersPositionStatFloatCallback(void *rowContext, void *columnContext) {
	int index = (int)rowContext;
	stat_t *s = ((stat_t *)columnContext) + index;
	return va("%s: %0.1f ^9(%d%s)", s->name, s->valueFloat, s->rank, RankSuffix(s->rank));
}

const char *PrintTopPlayersPositionStatTimeCallback(void *rowContext, void *columnContext) {
	int index = (int)rowContext;
	stat_t *s = ((stat_t *)columnContext) + index;
	int secs = (int)round(s->valueFloat);
	int mins = secs / 60;
	if (secs >= 60) {
		secs %= 60;
		return va("%s: %dm%02ds ^9(%d%s)", s->name, mins, secs, s->rank, RankSuffix(s->rank));
	}
	else {
		return va("%s: %ds ^9(%d%s)", s->name, secs, s->rank, RankSuffix(s->rank));
	}
}

const char *PrintTopPlayersPositionStatPercentCallback(void *rowContext, void *columnContext) {
	int index = (int)rowContext;
	stat_t *s = ((stat_t *)columnContext) + index;
	return va("%s: %0.2f'/. ^9(%d%s)", s->name, s->valueFloat * 100, s->rank, RankSuffix(s->rank));
}

static void GetStatForTableInt(const char *statName, ctfPosition_t pos, stat_t *rowsPtr, int *highestNumRowsPtr) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, va("SELECT name, %s, %s_rank FROM accountstats WHERE pos = %d ORDER BY %s DESC, name ASC LIMIT 10;", statName, statName, pos, statName), -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);
	int rowNum = 0;
	while (rc == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(statement, 0);
		if (VALIDSTRING(name))
			Q_strncpyz((rowsPtr + rowNum)->name, name, sizeof((rowsPtr + rowNum)->name));
		(rowsPtr + rowNum)->valueInt = sqlite3_column_int(statement, 1);
		(rowsPtr + rowNum)->rank = sqlite3_column_int(statement, 2);

		rc = trap_sqlite3_step(statement);
		++rowNum;
	}
	trap_sqlite3_finalize(statement);
	if (rowNum > *highestNumRowsPtr)
		*highestNumRowsPtr = rowNum;
}

static void GetStatForTableFloat(const char *statName, ctfPosition_t pos, stat_t *rowsPtr, int *highestNumRowsPtr) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, va("SELECT name, %s, %s_rank FROM accountstats WHERE pos = %d ORDER BY %s DESC, name ASC LIMIT 10;", statName, statName, pos, statName), -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);
	int rowNum = 0;
	while (rc == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(statement, 0);
		if (VALIDSTRING(name))
			Q_strncpyz((rowsPtr + rowNum)->name, name, sizeof((rowsPtr + rowNum)->name));
		(rowsPtr + rowNum)->valueFloat = sqlite3_column_double(statement, 1);
		(rowsPtr + rowNum)->rank = sqlite3_column_int(statement, 2);

		rc = trap_sqlite3_step(statement);
		++rowNum;
	}
	trap_sqlite3_finalize(statement);
	if (rowNum > *highestNumRowsPtr)
		*highestNumRowsPtr = rowNum;
}

#define NUM_TOPPLAYERS_TABLES	(7)
static char topPlayersBuf[3][16384] = { 0 };

static void LoadTopPlayersFromDatabase(void) {
	G_DBGetMetadata("topPlayers1", topPlayersBuf[0], sizeof(topPlayersBuf[0]));
	G_DBGetMetadata("topPlayers2", topPlayersBuf[1], sizeof(topPlayersBuf[1]));
	G_DBGetMetadata("topPlayers3", topPlayersBuf[2], sizeof(topPlayersBuf[2]));
}

int topStatMemoryOffset = 0;
ctfPosition_t topStatPos = CTFPOSITION_UNKNOWN;
typedef enum {
	STATTYPE_INTEGER = 0,
	STATTYPE_FLOAT,
	STATTYPE_PERCENT,
	STATTYPE_TIME
} topStatType_t;
topStatType_t topStatType = STATTYPE_INTEGER;

const char *NewTopDudesCallback(void *rowContext, void *columnContext) {
	int rowNum = (int)rowContext;
	accountStats_t *a = (accountStats_t *)columnContext;
	a += rowNum;
	if (a->pos != topStatPos)
		return NULL;
	unsigned int sAddr = (unsigned int)a;
	stat_t *s = (stat_t *)(sAddr + topStatMemoryOffset);

	char *theStat = NULL;
	if (topStatType == STATTYPE_INTEGER) {
		theStat = va("%d", s->valueInt);
	}
	else if (topStatType == STATTYPE_FLOAT) {
		theStat = va("%0.1f", s->valueFloat);
	}
	else if (topStatType == STATTYPE_PERCENT) {
		theStat = va("%0.2f'/.", s->valueFloat * 100.0f);
	}
	else if (topStatType == STATTYPE_TIME) {
		int secs = s->valueInt;
		int mins = secs / 60;
		if (secs >= 60) {
			secs %= 60;
			theStat = va("%dm%02ds", mins, secs);
		}
		else {
			theStat = va("%ds", secs);
		}
	}


	return va("%s: %s ^9(%d%s)", s->name, theStat, s->rank, RankSuffix(s->rank));
}

int CompareDudes(const void *a, const void *b) {
	unsigned int aAddr = (unsigned int)a;
	unsigned int bAddr = (unsigned int)b;
	accountStats_t *aa = (accountStats_t *)a;
	accountStats_t *bb = (accountStats_t *)b;

	if (aa->pos == topStatPos && bb->pos != topStatPos)
		return -1;
	if (bb->pos == topStatPos && aa->pos != topStatPos)
		return 1;
	if (aa->pos != topStatPos && bb->pos != topStatPos)
		return 0;

	if (topStatType == STATTYPE_INTEGER) {
		int aVal = *((int *)(aAddr + topStatMemoryOffset));
		int bVal = *((int *)(bAddr + topStatMemoryOffset));
		if (aVal < bVal)
			return 1;
		if (aVal > bVal)
			return -1;
	}
	else {
		float aVal = *((float *)(aAddr + topStatMemoryOffset));
		float bVal = *((float *)(bAddr + topStatMemoryOffset));
		if (aVal < bVal)
			return 1;
		if (aVal > bVal)
			return -1;
	}

	if (aa->name[0] > bb->name[0])
		return 1;
	if (aa->name[0] < bb->name[0])
		return -1;
	return 0;
}

#define MAX_STAT_COLUMNS (50)
#define SetTopStat(field, columnName, type) \
do { \
		assert(index < MAX_STAT_COLUMNS); \
		topStatMemoryOffset = (unsigned int)&arr->field - (unsigned int)&arr->node; \
		topStatType = type; \
		qsort(arr, size, sizeof(accountStats_t), CompareDudes); \
		memcpy(sorted, arr, sizeof(accountStats_t) * topNum); \
 \
		for (int i = 0; i < topNum; i++) { \
			accountStats_t *to = topTen[index] + i; \
			accountStats_t *from = sorted + i; \
			memcpy(&to->field, &from->field, sizeof(to->field)); \
			Q_strncpyz(to->field.name, from->name, sizeof(to->field.name)); \
			to->pos = from->pos; \
		} \
 \
		Table_DefineColumn(t, columnName, NewTopDudesCallback, topTen[index], qfalse, -1, 32); \
		index++; \
} while(0)

static void RecalculateTopPlayers(void) {
	memset(topPlayersBuf, 0, sizeof(topPlayersBuf));

	int size = level.cachedPositionStatsRaw.size;
	if (!size)
		return;

	accountStats_t *arr = calloc(size, sizeof(accountStats_t));
	int topNum = size < 10 ? size : 10;
	accountStats_t *sorted = calloc(topNum, sizeof(accountStats_t));
	accountStats_t *topTen[MAX_STAT_COLUMNS];
	for (int i = 0; i < MAX_STAT_COLUMNS; i++)
		topTen[i] = calloc(topNum, sizeof(accountStats_t));

	int index = 0;
	iterator_t iter;
	ListIterate(&level.cachedPositionStatsRaw, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		accountStats_t *stats = IteratorNext(&iter);
		memcpy(arr + index++, stats, sizeof(accountStats_t));
	}

	for (ctfPosition_t pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
		index = 0;
		topStatPos = pos;

		Table *t = Table_Initialize(qfalse);
		for (int i = 0; i < topNum; i++)
			Table_DefineRow(t, (void *)i);
		SetTopStat(pugsplayed, "^5Pugs", STATTYPE_INTEGER);
		SetTopStat(wins, "^5Wins", STATTYPE_INTEGER);
		SetTopStat(winrate, "^5Winrate", STATTYPE_PERCENT);
		SetTopStat(cap, "^5Cap", STATTYPE_FLOAT);
		SetTopStat(ass, "^5Ass", STATTYPE_FLOAT);
		SetTopStat(def, "^5Def", STATTYPE_FLOAT);
		SetTopStat(acc, "^5Acc", STATTYPE_FLOAT);
		Table_WriteToBuffer(t, topPlayersBuf[pos - 1], sizeof(topPlayersBuf[pos - 1]), qtrue, -1);
		Table_Destroy(t);

		t = Table_Initialize(qfalse);
		for (int i = 0; i < topNum; i++)
			Table_DefineRow(t, (void *)i);
		SetTopStat(air, "^5Airs", STATTYPE_FLOAT);
		SetTopStat(tk, "^5TK", STATTYPE_FLOAT);
		SetTopStat(take, "^5Take", STATTYPE_FLOAT);
		SetTopStat(pitkil, "^6Pit", STATTYPE_FLOAT);
		SetTopStat(pitdth, "^6Dth", STATTYPE_FLOAT);
		SetTopStat(dmg, "^2Dmg", STATTYPE_FLOAT);
		SetTopStat(fcdmg, "^2Fc", STATTYPE_FLOAT);
		int len = strlen(topPlayersBuf[pos - 1]);
		Table_WriteToBuffer(t, topPlayersBuf[pos - 1] + len, sizeof(topPlayersBuf[pos - 1]) - len , qtrue, -1);
		Table_Destroy(t);

		t = Table_Initialize(qfalse);
		for (int i = 0; i < topNum; i++)
			Table_DefineRow(t, (void *)i);
		SetTopStat(clrdmg, "^2Clr", STATTYPE_FLOAT);
		SetTopStat(othrdmg, "^2Othr", STATTYPE_FLOAT);
		SetTopStat(dmgtkn, "^1Tkn", STATTYPE_FLOAT);
		SetTopStat(fcdmgtkn, "^1Fc", STATTYPE_FLOAT);
		SetTopStat(clrdmgtkn, "^1Clr", STATTYPE_FLOAT);
		SetTopStat(othrdmgtkn, "^1Othr", STATTYPE_FLOAT);
		SetTopStat(fckil, "^6FcKil", STATTYPE_FLOAT);
		len = strlen(topPlayersBuf[pos - 1]);
		Table_WriteToBuffer(t, topPlayersBuf[pos - 1] + len, sizeof(topPlayersBuf[pos - 1]) - len, qtrue, -1);
		Table_Destroy(t);

		t = Table_Initialize(qfalse);
		for (int i = 0; i < topNum; i++)
			Table_DefineRow(t, (void *)i);
		SetTopStat(fckileff, "^6Eff", STATTYPE_FLOAT);
		SetTopStat(ret, "^5Ret", STATTYPE_FLOAT);
		SetTopStat(sk, "^5SK", STATTYPE_FLOAT);
		SetTopStat(ttlhold, "^3Hold", STATTYPE_TIME);
		SetTopStat(maxhold, "^3Max", STATTYPE_TIME);
		SetTopStat(avgspd, "^6Spd", STATTYPE_FLOAT);
		SetTopStat(topspd, "^6Top", STATTYPE_FLOAT);
		len = strlen(topPlayersBuf[pos - 1]);
		Table_WriteToBuffer(t, topPlayersBuf[pos - 1] + len, sizeof(topPlayersBuf[pos - 1]) - len, qtrue, -1);
		Table_Destroy(t);

		t = Table_Initialize(qfalse);
		for (int i = 0; i < topNum; i++)
			Table_DefineRow(t, (void *)i);
		SetTopStat(boon, "^5Boon", STATTYPE_FLOAT);
		SetTopStat(push, "^5Push", STATTYPE_FLOAT);
		SetTopStat(pull, "^5Pull", STATTYPE_FLOAT);
		SetTopStat(heal, "^5Heal", STATTYPE_FLOAT);
		SetTopStat(te, "^6TE", STATTYPE_FLOAT);
		SetTopStat(teeff, "^6Eff", STATTYPE_FLOAT);
		SetTopStat(gotte, "^6Rcvd", STATTYPE_FLOAT);
		len = strlen(topPlayersBuf[pos - 1]);
		Table_WriteToBuffer(t, topPlayersBuf[pos - 1] + len, sizeof(topPlayersBuf[pos - 1]) - len, qtrue, -1);
		Table_Destroy(t);

		t = Table_Initialize(qfalse);
		for (int i = 0; i < topNum; i++)
			Table_DefineRow(t, (void *)i);
		SetTopStat(enemynrg, "^5EnemyNrg", STATTYPE_FLOAT);
		SetTopStat(absorb, "^5Abs", STATTYPE_FLOAT);
		SetTopStat(protdmg, "^2Prot", STATTYPE_FLOAT);
		SetTopStat(prottime, "^2Time", STATTYPE_TIME);
		SetTopStat(rage, "^5Rage", STATTYPE_TIME);
		SetTopStat(drain, "^1Drn", STATTYPE_FLOAT);
		SetTopStat(drained, "^1Drned", STATTYPE_FLOAT);
		len = strlen(topPlayersBuf[pos - 1]);
		Table_WriteToBuffer(t, topPlayersBuf[pos - 1] + len, sizeof(topPlayersBuf[pos - 1]) - len, qtrue, -1);
		Table_Destroy(t);

		t = Table_Initialize(qfalse);
		for (int i = 0; i < topNum; i++)
			Table_DefineRow(t, (void *)i);
		SetTopStat(grips, "^1Gr", STATTYPE_FLOAT);
		SetTopStat(gripped, "^1Gd", STATTYPE_FLOAT);
		SetTopStat(dark, "^1Dk", STATTYPE_FLOAT);
		SetTopStat(fs, "^5Fs", STATTYPE_FLOAT);
		SetTopStat(bas, "^5Bas", STATTYPE_FLOAT);
		SetTopStat(mid, "^5Mid", STATTYPE_FLOAT);
		SetTopStat(eba, "^5EBa", STATTYPE_FLOAT);
		SetTopStat(efs, "^5EFs", STATTYPE_FLOAT);
		len = strlen(topPlayersBuf[pos - 1]);
		Table_WriteToBuffer(t, topPlayersBuf[pos - 1] + len, sizeof(topPlayersBuf[pos - 1]) - len, qtrue, -1);
		Table_Destroy(t);

		G_DBSetMetadata(va("topPlayers%d", (int)pos), topPlayersBuf[pos - 1]);
	}

	free(arr);
	free(sorted);
	for (int i = 0; i < MAX_STAT_COLUMNS; i++)
		free(topTen[i]);
}

static void GetWinrate(int accountId, ctfPosition_t pos, ctfPosition_t otherPos, winrate_t *rowsPtr, int *highestNumRowsPtr, qboolean lowest, qboolean noLimits) {
	char *query = va("WITH t2 AS ( "
		"WITH t AS ( "
		"WITH combinedgames AS ( "
		"WITH me AS "
		"(SELECT match_id, team AS myteam FROM playerpugteampos "
		"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
		"WHERE account_id = ?%s), "
		"others AS "
		"(SELECT accounts.account_id AS other_account_id, accounts.name AS other_name, match_id, team AS other_team FROM playerpugteampos "
		"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
		"JOIN accounts ON sessions.account_id = accounts.account_id "
		"WHERE accounts.account_id != ?%s) "
		"SELECT *, other_account_id, other_name FROM me "
		"JOIN others ON me.match_id = others.match_id "
		"WHERE myteam != other_team) "
		"SELECT other_account_id, other_name, win_team, myteam, other_team FROM pugs "
		"JOIN combinedgames ON pugs.match_id = combinedgames.match_id) "
		"SELECT other_name, SUM(1) AS matchups, SUM(CASE WHEN win_team == myteam THEN 1 ELSE 0 END) AS wins, CAST(SUM(CASE WHEN win_team == myteam THEN 1 ELSE 0 END) AS FLOAT) / COUNT(*) AS winrate "
		"FROM t "
		"GROUP BY other_account_id) "
		"SELECT *, RANK() OVER (ORDER BY winrate %s) winrate_rank "
		"FROM t2 "
#ifdef DEBUG_CTF_POSITION_STATS
		"WHERE winrate %s 0.5 "
#else
		"WHERE matchups >= 5 AND winrate %s 0.5 "
#endif
		"ORDER BY winrate %s LIMIT %d;", pos ? " AND pos = ?" : "", otherPos ? " AND pos = ?" : "", lowest ? "ASC" : "DESC", lowest ? "<=" : ">=", lowest ? "ASC" : "DESC", noLimits ? 10 : 5);

	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, query, -1, &statement, 0);
	int bind = 1;
	sqlite3_bind_int(statement, bind++, accountId);
	if (pos)
		sqlite3_bind_int(statement, bind++, pos);
	sqlite3_bind_int(statement, bind++, accountId);
	if (otherPos)
		sqlite3_bind_int(statement, bind++, otherPos);
	int rc = trap_sqlite3_step(statement);
	int rowNum = 0;
	while (rc == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(statement, 0);
		if (VALIDSTRING(name))
			Q_strncpyz((rowsPtr + rowNum)->name, name, sizeof((rowsPtr + rowNum)->name));
		(rowsPtr + rowNum)->matchups = sqlite3_column_int(statement, 1);
		(rowsPtr + rowNum)->wins = sqlite3_column_int(statement, 2);
		(rowsPtr + rowNum)->winrate = sqlite3_column_double(statement, 3);
		(rowsPtr + rowNum)->rank = sqlite3_column_int(statement, 4);

		rc = trap_sqlite3_step(statement);
		++rowNum;
	}
	trap_sqlite3_finalize(statement);
	if (rowNum > *highestNumRowsPtr)
		*highestNumRowsPtr = rowNum;
}

static qboolean CachedPerMapWinrateMatches(genericNode_t *node, void *userData) {
	cachedPerMapWinrate_t *existing = (cachedPerMapWinrate_t *)node;
	cachedPerMapWinrate_t *thisOne = (cachedPerMapWinrate_t *)userData;
	if (!existing || existing->accountId != thisOne->accountId || existing->pos != thisOne->pos)
		return qfalse;
	return qtrue;
}

typedef struct {
	node_t			node;
	int				accountId;
	char			filteredMapName[MAX_QPATH];
	int				plays;
	int				wins;
	double			winrate;
	int				winrateRank;
} perMapWinrate_t;

const char *const sqlGetPerMapWinrateAllPos =
"WITH t AS ( "
"WITH wins AS ( "
"SELECT account_id, playerpugteampos.pos, map, count(*) AS wincount "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"WHERE playerpugteampos.team = win_team "
"GROUP BY account_id, map), "
"pugs_played AS ( "
"SELECT account_id, playerpugteampos.pos, map, count(*) AS playcount "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"GROUP BY account_id, map) "
"SELECT wins.account_id, wins.map, playcount, wincount, CAST(wincount AS FLOAT) / CAST(playcount AS FLOAT) AS winrate FROM wins JOIN pugs_played ON wins.account_id = pugs_played.account_id AND wins.map = pugs_played.map "
"WHERE playcount >= 5 AND winrate %s 0.5 "
"ORDER BY wins.account_id ASC, winrate %s, playcount, wins.map) "
"SELECT account_id, map, playcount, wincount, winrate, RANK() OVER (PARTITION BY account_id ORDER BY winrate %s) AS winrate_rank FROM t;";

const char *const sqlGetPerMapWinrateSpecificPos =
"WITH t AS ( "
"WITH wins AS ( "
"SELECT account_id, playerpugteampos.pos, map, count(*) AS wincount "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"WHERE playerpugteampos.team = win_team AND pos = ?1 "
"GROUP BY account_id, map), "
"pugs_played AS ( "
"SELECT account_id, playerpugteampos.pos, map, count(*) AS playcount "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"WHERE pos = ?1 "
"GROUP BY account_id, map) "
"SELECT wins.account_id, wins.map, playcount, wincount, CAST(wincount AS FLOAT) / CAST(playcount AS FLOAT) AS winrate FROM wins JOIN pugs_played ON wins.account_id = pugs_played.account_id AND wins.map = pugs_played.map "
"WHERE playcount >= 5 AND winrate %s 0.5 "
"ORDER BY wins.account_id ASC, winrate %s, playcount, wins.map) "
"SELECT account_id, map, playcount, wincount, winrate, RANK() OVER (PARTITION BY account_id ORDER BY winrate %s) AS winrate_rank FROM t;";

const char *PerMapWinrateCallback(void *rowContext, void *columnContext) {
	int index = (int)rowContext;
	perMapWinrate_t *wr = ((perMapWinrate_t *)columnContext) + index;
	if (!wr->filteredMapName[0])
		return NULL;
	return va("%s: %0.2f'/. ^9(%d/%d, %d%s)", wr->filteredMapName, wr->winrate * 100, wr->wins, wr->plays, wr->winrateRank, RankSuffix(wr->winrateRank));
}

static void LoadPerMapWinratesFromDatabase(void) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, "SELECT account_id, pos, str FROM [cachedplayerstats] WHERE type = 2;", -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		cachedPerMapWinrate_t *c = ListAdd(&level.cachedPerMapWinrates, sizeof(cachedPerMapWinrate_t));
		c->accountId = sqlite3_column_int(statement, 0);
		c->pos = sqlite3_column_int(statement, 1);
		const char *str = (const char *)sqlite3_column_text(statement, 2);
		c->strPtr = strdup(str);
		rc = trap_sqlite3_step(statement);
	}
	trap_sqlite3_finalize(statement);
}

static void RecalculatePerMapWinRates(void) {
	iterator_t iter;
	ListIterate(&level.cachedPerMapWinrates, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		cachedPerMapWinrate_t *c = IteratorNext(&iter);
		if (c->strPtr)
			free(c->strPtr);
	}
	ListClear(&level.cachedPerMapWinrates);
	trap_sqlite3_exec(dbPtr, "DELETE FROM [cachedplayerstats] WHERE type = 2;", NULL, NULL, NULL);

	list_t winrateDataList[4][2];
	memset(winrateDataList, 0, sizeof(winrateDataList));

	sqlite3_stmt *statement;
	for (int pos = CTFPOSITION_UNKNOWN; pos <= CTFPOSITION_OFFENSE; pos++) {
		for (int best = qtrue; best >= qfalse; best--) {
			if (pos == CTFPOSITION_UNKNOWN) {
				trap_sqlite3_prepare_v2(dbPtr, va(sqlGetPerMapWinrateAllPos, best ? ">=" : "<=", best ? "DESC" : "ASC", best ? "DESC" : "ASC"), -1, &statement, 0);
			}
			else {
				trap_sqlite3_prepare_v2(dbPtr, va(sqlGetPerMapWinrateSpecificPos, best ? ">=" : "<=", best ? "DESC" : "ASC", best ? "DESC" : "ASC"), -1, &statement, 0);
				sqlite3_bind_int(statement, 1, pos);
			}

			int rc = trap_sqlite3_step(statement);
			while (rc == SQLITE_ROW) {
				perMapWinrate_t *add = ListAdd(&winrateDataList[pos][best], sizeof(perMapWinrate_t));
				add->accountId = sqlite3_column_int(statement, 0);
				const char *mapFileName = (const char *)sqlite3_column_text(statement, 1);
				GetShortNameForMapFileName(mapFileName, add->filteredMapName, sizeof(add->filteredMapName));
				if (!add->filteredMapName[0])
					Q_strncpyz(add->filteredMapName, mapFileName, sizeof(add->filteredMapName));
				add->plays = sqlite3_column_int(statement, 2);
				add->wins = sqlite3_column_int(statement, 3);
				add->winrate = sqlite3_column_double(statement, 4);
				add->winrateRank = sqlite3_column_int(statement, 5);
				rc = trap_sqlite3_step(statement);
			}

			trap_sqlite3_reset(statement);
		}
	}

	trap_sqlite3_prepare_v2(dbPtr, "SELECT accounts.name, accounts.account_id FROM [accounts] JOIN [sessions] ON accounts.account_id = sessions.account_id JOIN [playerpugteampos] ON sessions.session_id = playerpugteampos.session_id GROUP BY accounts.account_id;", -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) { // loop through each account
		const char *name = (const char *)sqlite3_column_text(statement, 0);
		const int accountId = sqlite3_column_int(statement, 1);

#define PERMAPWINRATE_MAXROWS	(10)
		perMapWinrate_t rows[4][2][PERMAPWINRATE_MAXROWS] = { 0 };
		for (int pos = CTFPOSITION_UNKNOWN; pos <= CTFPOSITION_OFFENSE; pos++) {
			int highestNumRows = 0;
			for (int best = qtrue; best >= qfalse; best--) {
				iterator_t iter;
				ListIterate(&winrateDataList[pos][best], &iter, qfalse);
				int rowNum = 0;
				while (IteratorHasNext(&iter)) {
					perMapWinrate_t *wr = IteratorNext(&iter);
					if (wr->accountId != accountId)
						continue;
					if (rowNum >= PERMAPWINRATE_MAXROWS)
						break;
					memcpy(&rows[pos][best][rowNum++], wr, sizeof(perMapWinrate_t));
				}
				if (rowNum > highestNumRows)
					highestNumRows = rowNum;
			}

			if (!highestNumRows)
				continue;

			Table *t = Table_Initialize(qfalse);
			for (int i = 0; i < highestNumRows; i++)
				Table_DefineRow(t, (void *)i);

			Table_DefineColumn(t, va("^2Best %s %smaps", name, pos ? va("%s ", NameForPos(pos)) : ""), PerMapWinrateCallback, &rows[pos][1][0], qfalse, -1, 32);
			Table_DefineColumn(t, va("^1Worst %s %smaps", name, pos ? va("%s ", NameForPos(pos)) : ""), PerMapWinrateCallback, &rows[pos][0][0], qfalse, -1, 32);

			char *buf = calloc(16384, sizeof(char));
			Table_WriteToBuffer(t, buf, 16384, qtrue, -1);
			Table_Destroy(t);

			if (*buf) {
				// add it to the db
				sqlite3_stmt *innerStatement;
				trap_sqlite3_prepare_v2(dbPtr, "INSERT OR REPLACE INTO [cachedplayerstats] (account_id, type, pos, str) VALUES (?, 2, ?, ?);", -1, &innerStatement, 0);
				sqlite3_bind_int(innerStatement, 1, accountId);
				sqlite3_bind_int(innerStatement, 2, pos);
				sqlite3_bind_text(innerStatement, 3, buf, -1, SQLITE_STATIC);
				int rc = trap_sqlite3_step(innerStatement);
				trap_sqlite3_finalize(innerStatement);

				// also load it into the cache
				cachedPerMapWinrate_t *c = ListAdd(&level.cachedPerMapWinrates, sizeof(cachedPerMapWinrate_t));
				c->accountId = accountId;
				c->pos = pos;
				c->strPtr = strdup(buf);
			}
			free(buf);
		}

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);

	for (int best = qtrue; best >= qfalse; best--) {
		for (int pos = CTFPOSITION_UNKNOWN; pos <= CTFPOSITION_OFFENSE; pos++) {
			ListClear(&winrateDataList[pos][best]);
		}
	}
}

void G_DBPrintPerMapWinrates(int accountId, ctfPosition_t positionOptional, int printClientNum) {
	cachedPerMapWinrate_t x;
	x.accountId = accountId;
	x.pos = positionOptional;
	cachedPerMapWinrate_t *found = ListFind(&level.cachedPerMapWinrates, CachedPerMapWinrateMatches, &x, NULL);
	if (found && VALIDSTRING(found->strPtr))
		PrintIngame(printClientNum, found->strPtr);
}

const char *WinrateCallback(void *rowContext, void *columnContext) {
	int index = (int)rowContext;
	winrate_t *wr = ((winrate_t *)columnContext) + index;
	if (!VALIDSTRING(wr->name))
		return NULL;
	return va("%s: %0.2f'/. ^9(%d/%d, %d%s)", wr->name, wr->winrate * 100, wr->wins, wr->matchups, wr->rank, RankSuffix(wr->rank));
}

static void LoadWinratesFromDatabase(void) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, "SELECT account_id, pos, str FROM [cachedplayerstats] WHERE type = 0;", -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		cachedPlayerPugStats_t *c = ListAdd(&level.cachedWinrates, sizeof(cachedPlayerPugStats_t));
		c->accountId = sqlite3_column_int(statement, 0);
		c->pos = sqlite3_column_int(statement, 1);
		const char *str = (const char *)sqlite3_column_text(statement, 2);
		c->strPtr = strdup(str);
		rc = trap_sqlite3_step(statement);
	}
	trap_sqlite3_finalize(statement);
}

static void RecalculateWinRates(void) {
	iterator_t iter;
	ListIterate(&level.cachedWinrates, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		cachedPlayerPugStats_t *c = IteratorNext(&iter);
		if (c->strPtr)
			free(c->strPtr);
	}
	ListClear(&level.cachedWinrates);
	trap_sqlite3_exec(dbPtr, "DELETE FROM [cachedplayerstats] WHERE type = 0;", NULL, NULL, NULL);

	sqlite3_stmt *outerStatement;
	trap_sqlite3_prepare_v2(dbPtr, "SELECT accounts.name, accounts.account_id FROM [accounts] JOIN [sessions] ON accounts.account_id = sessions.account_id JOIN [playerpugteampos] ON sessions.session_id = playerpugteampos.session_id GROUP BY accounts.account_id;", -1, &outerStatement, 0);
	int outerRc = trap_sqlite3_step(outerStatement);
	while (outerRc == SQLITE_ROW) { // loop through each account
		const char *name = (const char *)sqlite3_column_text(outerStatement, 0);
		const int accountId = sqlite3_column_int(outerStatement, 1);

		for (ctfPosition_t pos = CTFPOSITION_UNKNOWN; pos <= CTFPOSITION_OFFENSE; pos++) {
			char *buf = calloc(16384, sizeof(char));
			for (qboolean lowest = qfalse; lowest <= qtrue; lowest++) {
				int highestNumRows = 0;
				winrate_t rows[4][50] = { 0 };
				for (int enemyPos = CTFPOSITION_UNKNOWN, num = 0; enemyPos <= CTFPOSITION_OFFENSE; enemyPos++, num++)
					GetWinrate(accountId, pos, enemyPos, &rows[num][0], &highestNumRows, lowest, /*noLimits*/qtrue);

				if (!highestNumRows)
					continue;

				Table *t = Table_Initialize(qfalse);
				for (int i = 0; i < highestNumRows; i++)
					Table_DefineRow(t, (void *)i);

				int num = 0;
				const char *worstBest = lowest ? "^1Worst" : "^2Best";
				Table_DefineColumn(t, va("%s %s %svs. any pos", worstBest, name, pos ? va("%s ", NameForPos(pos)) : ""), WinrateCallback, &rows[num++][0], qfalse, -1, 32);
				Table_DefineColumn(t, va("%s %s %svs. base", worstBest, name, pos ? va("%s ", NameForPos(pos)) : ""), WinrateCallback, &rows[num++][0], qfalse, -1, 32);
				Table_DefineColumn(t, va("%s %s %svs. chase", worstBest, name, pos ? va("%s ", NameForPos(pos)) : ""), WinrateCallback, &rows[num++][0], qfalse, -1, 32);
				Table_DefineColumn(t, va("%s %s %svs. offense", worstBest, name, pos ? va("%s ", NameForPos(pos)) : ""), WinrateCallback, &rows[num++][0], qfalse, -1, 32);

				size_t len = strlen(buf);
				Table_WriteToBuffer(t, buf + len, 16384 - len, qtrue, -1);
				Table_Destroy(t);
			}

			if (*buf) {
				// add it to the db
				sqlite3_stmt *statement;
				trap_sqlite3_prepare_v2(dbPtr, "INSERT OR REPLACE INTO [cachedplayerstats] (account_id, type, pos, str) VALUES (?, 0, ?, ?);", -1, &statement, 0);
				sqlite3_bind_int(statement, 1, accountId);
				sqlite3_bind_int(statement, 2, pos);
				sqlite3_bind_text(statement, 3, buf, -1, SQLITE_STATIC);
				int rc = trap_sqlite3_step(statement);
				trap_sqlite3_finalize(statement);

				// also load it into the cache
				cachedPlayerPugStats_t *c = ListAdd(&level.cachedWinrates, sizeof(cachedPlayerPugStats_t));
				c->accountId = accountId;
				c->pos = pos;
				c->strPtr = strdup(buf);
			}
			free(buf);
		}
		outerRc = trap_sqlite3_step(outerStatement);
	}

	trap_sqlite3_finalize(outerStatement);
}

void G_DBPrintTopPlayersForPosition(ctfPosition_t pos, int printClientNum) {
	if (topPlayersBuf[pos - 1][0]) {
		PrintIngame(printClientNum, "Top ^6%s^7 players:\n", NameForPos(pos));
		PrintIngame(printClientNum, topPlayersBuf[pos - 1]);
	}
	else {
		PrintIngame(printClientNum, "No %s players have pugstats yet!\n", NameForPos(pos));
	}
}

const char *sqlGetPlayersWithStats = "SELECT name, account_id FROM accountstats GROUP BY account_id ORDER BY name ASC;";
void G_DBPrintPlayersWithStats(int printClientNum) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlGetPlayersWithStats, -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);

	char nameList[4096] = { 0 };
	int numFound = 0;
	while (rc == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(statement, 0);
		const int accountId = sqlite3_column_int(statement, 1);

		const char *colorStr = "^7";
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (ent->inuse && ent->client && ent->client->pers.connected != CON_DISCONNECTED && ent->client->account && ent->client->account->id == accountId &&
				!(ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(ent->client->pers.netname, "elo BOT"))) {
				switch (ent->client->sess.sessionTeam) {
				case TEAM_RED: colorStr = "^1"; break;
				case TEAM_BLUE: colorStr = "^4"; break;
				default: colorStr = "^3"; break;
				}
				break;
			}
		}
		const char *appendName = va("%s%s", colorStr, name);
		Q_strcat(nameList, sizeof(nameList), va("%s%s", numFound ? "^7, " : "", appendName));
		numFound++;

		rc = trap_sqlite3_step(statement);
	}

	if (numFound)
		PrintIngame(printClientNum, "Players with pugstats: %s^7\n", nameList);
	else
		PrintIngame(printClientNum, "No players have pugstats yet!\n");

	trap_sqlite3_finalize(statement);
}

void G_DBPrintWinrates(int accountId, ctfPosition_t positionOptional, int printClientNum) {
	cachedPlayerPugStats_t x;
	x.accountId = accountId;
	x.pos = positionOptional;
	cachedPlayerPugStats_t *found = ListFind(&level.cachedWinrates, CachedPlayerPugStatsMatches, &x, NULL);
	if (found && VALIDSTRING(found->strPtr))
		PrintIngame(printClientNum, found->strPtr);
}

const char *const sqlGetRustiness = "SELECT datetime FROM playerpugteampos JOIN sessions ON playerpugteampos.session_id = sessions.session_id, pugs ON playerpugteampos.match_id = pugs.match_id WHERE sessions.account_id = ?1 ORDER BY pugs.datetime DESC LIMIT 1 OFFSET ?2;";

typedef struct {
	node_t		node;
	int			accountId;
} rustyPlayer_t;

static qboolean RustyPlayerMatches(genericNode_t *node, void *userData) {
	rustyPlayer_t *existing = (rustyPlayer_t *)node;
	int accountId = *((int *)userData);
	if (!existing || existing->accountId != accountId)
		return qfalse;
	return qtrue;
}

static int RecalculateRustiness(void) {
	ListClear(&level.rustyPlayersList);
	G_DBDeleteMetadataStartingWith("rusty_");

	if (g_vote_teamgen_rustWeeks.integer <= 0)
		return 0;

	sqlite3_stmt *outerStatement;
	trap_sqlite3_prepare_v2(dbPtr,
		"SELECT accounts.name, accounts.account_id, accounts.created_on, strftime('%s', 'now') FROM accounts",
		-1, &outerStatement, 0);
	int outerRc = trap_sqlite3_step(outerStatement);
	int count = 0;
	while (outerRc == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(outerStatement, 0);
		int accountId = sqlite3_column_int(outerStatement, 1);
		int created_on = sqlite3_column_int(outerStatement, 2);
		int now = sqlite3_column_int(outerStatement, 3);

		if (now - created_on >= (60 * 60 * 24 * 7 * g_vote_teamgen_rustWeeks.integer)) {
			qboolean isRusty = qfalse;
			sqlite3_stmt *innerStatement;
			trap_sqlite3_prepare_v2(dbPtr, sqlGetRustiness, -1, &innerStatement, 0);
			sqlite3_bind_int(innerStatement, 1, accountId);
			sqlite3_bind_int(innerStatement, 2, 2); // third most recent pug
			int innerRc = trap_sqlite3_step(innerStatement);
			if (innerRc == SQLITE_ROW) {
				int pastPugTime = sqlite3_column_int(innerStatement, 0);
				if (now - pastPugTime >= (60 * 60 * 24 * 7 * g_vote_teamgen_rustWeeks.integer))
					isRusty = qtrue;
			}
			else {
				isRusty = qfalse; // no pug history in db
			}
			trap_sqlite3_finalize(innerStatement);

			if (isRusty) {
				G_DBSetMetadata(va("rusty_%d", accountId), "1");

				rustyPlayer_t *add = ListAdd(&level.rustyPlayersList, sizeof(rustyPlayer_t));
				if (add)
					add->accountId = accountId;

				++count;
			}
		}
		outerRc = trap_sqlite3_step(outerStatement);
	}

	trap_sqlite3_finalize(outerStatement);

	return count;
}

static int GetCachedRustiness(void) {
	ListClear(&level.rustyPlayersList);

	const char *sqlGetAllRusty = "SELECT key, value FROM metadata WHERE key LIKE 'rusty_%';";
	sqlite3_stmt *statement = NULL;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetAllRusty, -1, &statement, 0);
	if (rc != SQLITE_OK) {
		assert(qfalse);
		return 0;
	}
	rc = trap_sqlite3_step(statement);
	int count = 0;
	while (rc == SQLITE_ROW) {
		const char *key = (const char *)sqlite3_column_text(statement, 0);
		const char *value = (const char *)sqlite3_column_text(statement, 1);
		if (VALIDSTRING(key) && VALIDSTRING(value) &&
			!Q_stricmpn(key, "rusty_", 6) && *(key + 6)) {
			int accountId = atoi(key + 6);
			if (atoi(value)) {
				rustyPlayer_t *entry = ListAdd(&level.rustyPlayersList, sizeof(rustyPlayer_t));
				if (entry)
					entry->accountId = accountId;
				++count;
			}
		}
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);

	return count;
}

typedef struct accountStreak_s {
	node_t node;
	int accountId;
	int currentStreak;
} accountStreak_t;

static int RecalculateAndRecacheStreaks(void) {
	ListClear(&level.streaksList);
	G_DBDeleteMetadataStartingWith("streak_");

	const char *sqlStreaksQuery =
		"WITH RECURSIVE "
		"ordered AS ( "
		"    SELECT "
		"        s.account_id, "
		"        p.match_id, "
		"        p.datetime, "
		"        CASE WHEN p.win_team = pptp.team THEN 1 ELSE -1 END AS wl, "
		"        ROW_NUMBER() OVER (PARTITION BY s.account_id ORDER BY p.datetime DESC) AS rn_desc "
		"    FROM sessions s "
		"    JOIN playerpugteampos pptp ON s.session_id = pptp.session_id "
		"    JOIN pugs p ON p.match_id = pptp.match_id "
		"    WHERE s.account_id IS NOT NULL "
		"      AND p.win_team IN (1,2) "
		"      AND NOT EXISTS ( "
		"          SELECT 1 "
		"          FROM playerpugteampos pptp2 "
		"          WHERE pptp2.session_id = s.session_id "
		"            AND pptp2.match_id = p.match_id "
		"            AND pptp2.team <> pptp.team "
		"      ) "
		"), "
		"rec(account_id, rn_desc, wl, count_matches) AS ( "
		"    SELECT "
		"        o.account_id, "
		"        o.rn_desc, "
		"        o.wl, "
		"        1 "
		"    FROM ordered o "
		"    WHERE o.rn_desc = 1 "
		"    UNION ALL "
		"    SELECT "
		"        r.account_id, "
		"        o.rn_desc, "
		"        r.wl, "
		"        r.count_matches + 1 "
		"    FROM rec r "
		"    JOIN ordered o "
		"      ON o.account_id = r.account_id "
		"     AND o.rn_desc   = r.rn_desc + 1 "
		"    WHERE o.wl = r.wl "
		") "
		"SELECT "
		"    account_id, "
		"    CASE WHEN wl = 1 THEN +1 ELSE -1 END * MAX(count_matches) AS current_streak "
		"FROM rec "
		"GROUP BY account_id, wl;";

	// we store the result in both in memory (level.streaksList) and also metadata as "streak_<accountId>"
	sqlite3_stmt *statement = NULL;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlStreaksQuery, -1, &statement, 0);
	if (rc != SQLITE_OK) {
		assert(qfalse);
		return 0;
	}

	rc = trap_sqlite3_step(statement);
	int count = 0;
	while (rc == SQLITE_ROW) {
		int accountId = sqlite3_column_int(statement, 0);
		int currentStreak = sqlite3_column_int(statement, 1);

		// add to in-memory list
		accountStreak_t *entry = (accountStreak_t *)ListAdd(&level.streaksList, sizeof(accountStreak_t));
		if (entry) {
			entry->accountId = accountId;
			entry->currentStreak = currentStreak;
		}

		// also save to metadata
		G_DBSetMetadata(va("streak_%d", accountId), va("%d", currentStreak));

		++count;
		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
	return count;
}

static int GetCachedStreaks(void) {
	ListClear(&level.streaksList);

	const char *sqlGetAllStreaks = "SELECT key, value FROM metadata WHERE key LIKE 'streak_%';";

	sqlite3_stmt *statement = NULL;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetAllStreaks, -1, &statement, 0);
	if (rc != SQLITE_OK) {
		assert(qfalse);
		return 0;
	}

	rc = trap_sqlite3_step(statement);
	int count = 0;
	while (rc == SQLITE_ROW) {
		const char *key = (const char *)sqlite3_column_text(statement, 0);
		const char *value = (const char *)sqlite3_column_text(statement, 1);

		if (!VALIDSTRING(key) || !VALIDSTRING(value)) {
			rc = trap_sqlite3_step(statement);
			continue;
		}

		// Parse out the account id from "streak_<accountId>"
		// The "streak_" prefix is 7 characters long:
		// e.g. "streak_42" => accountId = 42
		if (!Q_stricmpn(key, "streak_", 7) && *(key + 7)) {
			int accountId = atoi(key + 7);
			int currentStreak = atoi(value);

			accountStreak_t *entry = (accountStreak_t *)ListAdd(&level.streaksList, sizeof(accountStreak_t));
			if (entry) {
				entry->accountId = accountId;
				entry->currentStreak = currentStreak;
				++count;
			}
		}

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
	return count;
}

int GetStreakForAccountID(int accountId) {
	if (accountId == ACCOUNT_ID_UNLINKED)
		return 0;

	iterator_t it;
	ListIterate(&level.streaksList, &it, qfalse);

	accountStreak_t *entry;
	while ((entry = (accountStreak_t *)IteratorNext(&it)) != NULL) {
		if (entry->accountId == accountId) {
			return entry->currentStreak;
		}
	}

	return 0;
}

#ifdef _DEBUG
#define FAST_START // uncomment to force loading from cache instead of recalculating
#endif

static void SetFastestPossibleCaptureTime(void) {
	sqlite3_stmt *statement;
	int result = 5000; // default to 5 seconds

	int rc = trap_sqlite3_prepare_v2(dbPtr, "SELECT capture_time FROM fastcaps WHERE mapname = ?1 ORDER BY capture_time ASC LIMIT 1;", -1, &statement, 0);

	sqlite3_bind_text(statement, 1, level.mapname, -1, SQLITE_STATIC);

	rc = trap_sqlite3_step(statement);

	if (rc == SQLITE_ROW)
		result = sqlite3_column_int(statement, 0);

	trap_sqlite3_finalize(statement);

	level.fastestPossibleCaptureTime = result;
}

extern qboolean MostPlayedPosMatches(genericNode_t *node, void *userData);
const char *const sqlGetMostPlayedPos = "SELECT account_id, pos, RANK() OVER (PARTITION BY account_id ORDER BY pugs_played DESC, wins DESC) FROM accountstats;";
// returns the number of distinct accounts loaded
static int RecalculateMostPlayedPositions(void) {
	ListClear(&level.mostPlayedPositionsList);
	G_DBDeleteMetadataStartingWith("mostplayed_");

	int accountCount = 0;
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlGetMostPlayedPos, -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		int accountId = sqlite3_column_int(statement, 0);
		ctfPosition_t pos = sqlite3_column_int(statement, 1);
		if (pos < CTFPOSITION_BASE || pos > CTFPOSITION_OFFENSE) {
			assert(qfalse);
			rc = trap_sqlite3_step(statement);
			continue;
		}
		int rank = sqlite3_column_int(statement, 2);
		if (rank < 0 || rank > 3) {
			assert(qfalse);
			rc = trap_sqlite3_step(statement);
			continue;
		}

		mostPlayedPos_t findMe;
		findMe.accountId = accountId;
		mostPlayedPos_t *found = ListFind(&level.mostPlayedPositionsList, MostPlayedPosMatches, &findMe, NULL);
		if (!found) {
			found = ListAdd(&level.mostPlayedPositionsList, sizeof(mostPlayedPos_t));
			found->accountId = accountId;
			found->mostPlayed = found->secondMostPlayed = found->thirdMostPlayed = 0;
			accountCount++;
		}

		if (rank == 1) {
			found->mostPlayed = pos;
			G_DBSetMetadata(va("mostplayed_%d_1", accountId), va("%d", pos));
		}
		else if (rank == 2) {
			found->secondMostPlayed = pos;
			G_DBSetMetadata(va("mostplayed_%d_2", accountId), va("%d", pos));
		}
		else if (rank == 3) {
			found->thirdMostPlayed = pos;
			G_DBSetMetadata(va("mostplayed_%d_3", accountId), va("%d", pos));
		}
		rc = trap_sqlite3_step(statement);
	}
	trap_sqlite3_finalize(statement);
	return accountCount;
}

// expects metadata keys in the format "mostplayed_<accountId>_<rank>" and returns the number of distinct accounts loaded.
static int GetCachedMostPlayedPositions(void) {
	ListClear(&level.mostPlayedPositionsList);
	int accountCount = 0;
	sqlite3_stmt *statement;
	const char *sqlCached = "SELECT key, value FROM metadata WHERE key LIKE 'mostplayed_%';";
	trap_sqlite3_prepare_v2(dbPtr, sqlCached, -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const char *key = (const char *)sqlite3_column_text(statement, 0);
		const char *value = (const char *)sqlite3_column_text(statement, 1);
		int accountId, rank;
		if (sscanf(key, "mostplayed_%d_%d", &accountId, &rank) != 2) {
			rc = trap_sqlite3_step(statement);
			Com_Printf("GetCachedMostPlayedPositions: error parsing %s value %s\n", key, value);
			continue;
		}
		int pos = atoi(value);
		if (pos < CTFPOSITION_BASE || pos > CTFPOSITION_OFFENSE) {
			rc = trap_sqlite3_step(statement);
			Com_Printf("GetCachedMostPlayedPositions: error parsing position from %s value %s\n", key, value);
			continue;
		}
		mostPlayedPos_t findMe;
		findMe.accountId = accountId;
		mostPlayedPos_t *found = ListFind(&level.mostPlayedPositionsList, MostPlayedPosMatches, &findMe, NULL);
		if (!found) {
			found = ListAdd(&level.mostPlayedPositionsList, sizeof(mostPlayedPos_t));
			found->accountId = accountId;
			found->mostPlayed = found->secondMostPlayed = found->thirdMostPlayed = 0;
			accountCount++;
		}
		if (rank == 1) {
			found->mostPlayed = pos;
		}
		else if (rank == 2) {
			found->secondMostPlayed = pos;
		}
		else if (rank == 3) {
			found->thirdMostPlayed = pos;
		}
		rc = trap_sqlite3_step(statement);
	}
	trap_sqlite3_finalize(statement);
	return accountCount;
}

// we recalculate everything from scratch if and only if the server has just been restarted and there are changes actually pending (shouldReloadPlayerPugStats metadata is set)
// otherwise, we just load the cached strings from the database for speed
void G_DBInitializePugStatsCache(void) {
	int start = trap_Milliseconds();
	int lastTime = start;

	const qboolean serverStartup = (!level.wasRestarted && !g_notFirstMap.integer);

	// reload if either a pug was played (shouldReloadPlayerPugStats 2) -or- it's been a while since we recalculated and an account<->session link has changed (shouldReloadPlayerPugStats 1)
	qboolean pendingReload = qfalse;
	const time_t currentTime = time(NULL);
	if (serverStartup) {
#ifdef _DEBUG
		if (0)
#else
		if (didUpgrade)
#endif
		{
			pendingReload = qtrue;
			Com_Printf("Recalculating stats because db was upgraded.\n");
		}
		else {
			int reloadMetadata = G_DBGetMetadataInteger("shouldReloadPlayerPugStats");
			if (reloadMetadata == 2) {
				pendingReload = qtrue; // pug played; definitely recalculate
				Com_Printf("Pug played, so recalculating stats.\n");
			}
			else if (reloadMetadata == 1) {
				char lastStatsReloadStr[MAX_STRING_CHARS] = { 0 };
				G_DBGetMetadata("lastStatsReload", lastStatsReloadStr, sizeof(lastStatsReloadStr));
				time_t lastReloadTime = lastStatsReloadStr[0] ? strtoll(lastStatsReloadStr, NULL, 10) : 0;

				if (lastReloadTime + (60 * 60 * 6 /*every 6 hours i guess*/) < currentTime) {
					pendingReload = qtrue; // it's been a while since we recalculated and an account<->session link has changed
					Com_Printf("An account<->session link has changed and enough time has elapsed since last recalculation, so recalculating stats.\n");
				}
				else {
					Com_Printf("An account<->session link has changed, but not enough time has elapsed since last recalculation. Using cache instead.\n");
				}
			}
		}
	}

	qboolean recalc = (serverStartup && pendingReload);

#if defined(_DEBUG) && defined(FAST_START)
	if (didUpgrade) {
		Com_Printf("Debug build with FAST_START, but performed database upgrade, so forcing recalculation.\n");
		recalc = qtrue;
	}
	else {
		Com_Printf("Debug build; using FAST_START. Will use cached stats/winrates instead of recalculating.\n");
		recalc = qfalse;
	}
#endif

	// these first four are handled together
	// these are the most computationally expensive ones to recalculate, so they must only run on server startup
	
	// check that we actually have a cache to load from (if not, we'll force the recalculation anyway)
	if (!recalc) {
		LoadPositionStatsFromDatabase();
		Com_Printf("Loaded position stats cache from db (took %d ms)\n", trap_Milliseconds() - lastTime);
		lastTime = trap_Milliseconds();

		LoadTopPlayersFromDatabase();
		Com_Printf("Loaded top players cache from db (took %d ms)\n", trap_Milliseconds() - lastTime);
		lastTime = trap_Milliseconds();

		LoadPerMapWinratesFromDatabase();
		Com_Printf("Loaded per-map win rates cache from db (took %d ms)\n", trap_Milliseconds() - lastTime);
		lastTime = trap_Milliseconds();

		LoadWinratesFromDatabase();
		Com_Printf("Loaded win rates cache from db (took %d ms)\n", trap_Milliseconds() - lastTime);
		lastTime = trap_Milliseconds();

		if (!level.cachedPositionStats.size ||
			topPlayersBuf[0][0] == '\0' ||
			topPlayersBuf[1][0] == '\0' ||
			topPlayersBuf[2][0] == '\0' ||
			!level.cachedPerMapWinrates.size ||
			!level.cachedWinrates.size) {
			Com_Printf("Recalculating because at least one cache was empty.\n");
			recalc = qtrue;
		}
	}

	if (recalc && serverStartup) {
		RecalculatePositionStats();
		Com_Printf("Recalculated position stats (took %d ms)\n", trap_Milliseconds() - lastTime);
		lastTime = trap_Milliseconds();

		RecalculateTopPlayers();
		Com_Printf("Recalculated top players (took %d ms)\n", trap_Milliseconds() - lastTime);
		lastTime = trap_Milliseconds();

		RecalculatePerMapWinRates();
		Com_Printf("Recalculated per-map win rates (took %d ms)\n", trap_Milliseconds() - lastTime);
		lastTime = trap_Milliseconds();

		RecalculateWinRates();
		Com_Printf("Recalculated win rates (took %d ms)\n", trap_Milliseconds() - lastTime);
		lastTime = trap_Milliseconds();

		G_DBSetMetadata("lastStatsReload", va("%lld", currentTime));
		G_DBSetMetadata("shouldReloadPlayerPugStats", "0");
	}

	// these next ones are not so computationally expensive that they must only run on server startup
	
	// win streaks
	int reloadStreaks = G_DBGetMetadataInteger("shouldReloadStreaks");
	if (reloadStreaks || recalc || serverStartup) {
		const int recalculatedStreaks = RecalculateAndRecacheStreaks();
		Com_Printf("Recalculated %d win streaks from db (took %d ms)\n", recalculatedStreaks, trap_Milliseconds() - lastTime);
		G_DBSetMetadata("shouldReloadStreaks", "0");
		lastTime = trap_Milliseconds();
	}
	else {
		const int cachedStreaks = GetCachedStreaks();
		if (cachedStreaks) {
			Com_Printf("Loaded %d cached win streaks from db (took %d ms)\n", cachedStreaks, trap_Milliseconds() - lastTime);
			lastTime = trap_Milliseconds();
		}
		else {
			const int recalculatedStreaks = RecalculateAndRecacheStreaks();
			Com_Printf("Failed to load win streaks cache from db. Instead recalculated %d win streaks from db (took %d ms)\n", recalculatedStreaks, trap_Milliseconds() - lastTime);
			G_DBSetMetadata("shouldReloadStreaks", "0");
			lastTime = trap_Milliseconds();
		}
	}

	// rusty players
	int reloadRust = G_DBGetMetadataInteger("shouldReloadRust");
	if (reloadRust || recalc || serverStartup) {
		const int recalculatedRust = RecalculateRustiness();
		Com_Printf("Recalculated %d rusty players from db (took %d ms)\n", recalculatedRust, trap_Milliseconds() - lastTime);
		G_DBSetMetadata("shouldReloadRust", "0");
		lastTime = trap_Milliseconds();
	}
	else {
		const int cachedRust = GetCachedRustiness();
		if (cachedRust) {
			Com_Printf("Loaded %d cached rusty players from db (took %d ms)\n", cachedRust, trap_Milliseconds() - lastTime);
			lastTime = trap_Milliseconds();
		}
		else {
			const int recalculatedRust = RecalculateRustiness();
			Com_Printf("Failed to load rusty players cache from db. Instead recalculated %d rusty players from db (took %d ms)\n", recalculatedRust, trap_Milliseconds() - lastTime);
			G_DBSetMetadata("shouldReloadRust", "0");
			lastTime = trap_Milliseconds();
		}
	}

	// most played positions
	int reloadMostPlayed = G_DBGetMetadataInteger("shouldReloadMostPlayed");
	if (reloadMostPlayed || recalc || serverStartup) {
		const int recalculatedMostPlayed = RecalculateMostPlayedPositions();
		Com_Printf("Recalculated %d accounts' most played positions from db (took %d ms)\n",
			recalculatedMostPlayed, trap_Milliseconds() - lastTime);
		G_DBSetMetadata("shouldReloadMostPlayed", "0");
		lastTime = trap_Milliseconds();
	}
	else {
		const int cachedMostPlayed = GetCachedMostPlayedPositions();
		if (cachedMostPlayed) {
			Com_Printf("Loaded %d cached accounts' most played positions from db (took %d ms)\n",
				cachedMostPlayed, trap_Milliseconds() - lastTime);
			lastTime = trap_Milliseconds();
		}
		else {
			const int recalculatedMostPlayed = RecalculateMostPlayedPositions();
			Com_Printf("Failed to load cached accounts' most played positions from db. Instead recalculated %d accounts' most played positions from db (took %d ms)\n",
				recalculatedMostPlayed, trap_Milliseconds() - lastTime);
			G_DBSetMetadata("shouldReloadMostPlayed", "0");
			lastTime = trap_Milliseconds();
		}
	}

	// this is computationally cheap, so we can just do it every time
	SetFastestPossibleCaptureTime();

	Com_Printf("Finished initializing pug stats cache (took %d ms total)\n", trap_Milliseconds() - start);
}

qboolean IsPlayerRusty(int accountId) {
	if (accountId == ACCOUNT_ID_UNLINKED || !level.rustyPlayersList.size)
		return qfalse;

	return !!ListFind(&level.rustyPlayersList, RustyPlayerMatches, &accountId, NULL);
}

typedef struct {
	node_t				node;
	char				name[32];
	ctfPlayerTier_t		tier;
} ratedPlayer_t;

extern ctfPlayerTier_t PlayerTierFromRating(double num);

extern char *PlayerRatingToString(ctfPlayerTier_t tier);
const char *const sqlGetPlayerRatings = "WITH unrated AS (SELECT name, account_id, 0 AS rating FROM accounts WHERE account_id NOT IN (SELECT ratee_account_id FROM playerratings JOIN accounts ON accounts.account_id = playerratings.ratee_account_id WHERE rater_account_id = ?1 AND POS = ?2) ORDER BY accounts.name ASC), rated AS (SELECT name, account_id, rating FROM playerratings JOIN accounts ON accounts.account_id = playerratings.ratee_account_id WHERE rater_account_id = ?1 AND pos = ?2) SELECT name, rating, account_id FROM unrated UNION SELECT name, rating, account_id FROM rated ORDER BY name ASC;";
void G_DBListRatingPlayers(int raterAccountId, int raterClientNum, ctfPosition_t pos) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlGetPlayerRatings, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, raterAccountId);
	sqlite3_bind_int(statement, 2, pos);
	int rc = trap_sqlite3_step(statement);
	qboolean gotAny = qfalse;
	list_t playerList = { 0 };
	while (rc == SQLITE_ROW) {
		gotAny = qtrue;
		const char *name = (const char *)sqlite3_column_text(statement, 0);
		const int tier = sqlite3_column_int(statement, 1);
		if (tier < PLAYERRATING_UNRATED || tier >= NUM_PLAYERRATINGS) {
			assert(qfalse);
			rc = trap_sqlite3_step(statement);
			continue; // ???
		}

		ratedPlayer_t *add = ListAdd(&playerList, sizeof(ratedPlayer_t));
		Q_strncpyz(add->name, name, sizeof(add->name));

		int accountId = sqlite3_column_int(statement, 2);
		if (IsPlayerRusty(accountId))
			Q_strcat(add->name, sizeof(add->name), "*");

		add->tier = tier;

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);

	if (!gotAny) {
		OutOfBandPrint(raterClientNum, "You have not rated any %s^7 players.", NameForPos(pos));
		return;
	}

	char ratingStr[NUM_PLAYERRATINGS][1024];
	memset(&ratingStr, 0, sizeof(ratingStr));
	for (int i = 0; i < NUM_PLAYERRATINGS; i++)
		Com_sprintf(ratingStr[i], sizeof(ratingStr[i]), "%10s: ^7", PlayerRatingToString(i));

	int numOfRating[NUM_PLAYERRATINGS] = { 0 };

	iterator_t iter;
	ListIterate(&playerList, &iter, qfalse);
	qboolean gotOne = qfalse;
	while (IteratorHasNext(&iter)) {
		ratedPlayer_t *player = (ratedPlayer_t *)IteratorNext(&iter);
		ctfPlayerTier_t tier = player->tier;
		Q_strcat(ratingStr[tier], sizeof(ratingStr[tier]), va("%s%s", numOfRating[tier]++ ? ", " : "", player->name));
		gotOne = qtrue;
	}

	ListClear(&playerList);

	char combined[8192] = { 0 };
	for (int i = NUM_PLAYERRATINGS - 1; i >= PLAYERRATING_UNRATED; i--)
		Q_strcat(combined, sizeof(combined), va("%s\n", ratingStr[i]));

	OutOfBandPrint(raterClientNum, "Your ^5%s^7 ratings:\n%s", NameForPos(pos), combined);
}

const char *const sqlSetPlayerRating = "INSERT OR REPLACE INTO playerratings(rater_account_id, ratee_account_id, pos, rating, datetime) VALUES (?,?,?,?,strftime('%s', 'now'));";
const char *const sqlSetPlayerRating2 = "INSERT INTO playerratingshistory(rater_account_id, ratee_account_id, pos, rating, datetime) VALUES (?,?,?,?,strftime('%s', 'now'));";
qboolean G_DBSetPlayerRating(int raterAccountId, int rateeAccountId, ctfPosition_t pos, ctfPlayerTier_t tier) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlSetPlayerRating, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, raterAccountId);
	sqlite3_bind_int(statement, 2, rateeAccountId);
	sqlite3_bind_int(statement, 3, pos);
	sqlite3_bind_int(statement, 4, tier);
	int rc = trap_sqlite3_step(statement);

	trap_sqlite3_reset(statement);
	trap_sqlite3_prepare_v2(dbPtr, sqlSetPlayerRating2, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, raterAccountId);
	sqlite3_bind_int(statement, 2, rateeAccountId);
	sqlite3_bind_int(statement, 3, pos);
	sqlite3_bind_int(statement, 4, tier);
	rc = trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);
	return !!(rc == SQLITE_DONE);
}

const char *const sqlDeletePlayerRating = "DELETE FROM playerratings WHERE rater_account_id = ? AND ratee_account_id = ? AND pos = ?;";
qboolean G_DBRemovePlayerRating(int raterAccountId, int rateeAccountId, ctfPosition_t pos) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlDeletePlayerRating, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, raterAccountId);
	sqlite3_bind_int(statement, 2, rateeAccountId);
	sqlite3_bind_int(statement, 3, pos);
	int rc = trap_sqlite3_step(statement);
	trap_sqlite3_finalize(statement);
	return !!(rc == SQLITE_DONE);
}

const char *const sqlDeleteAllRatingsOnPosition = "DELETE FROM playerratings WHERE rater_account_id = ? AND pos = ?;";
const char *const sqlDeleteAllRatings = "DELETE FROM playerratings WHERE rater_account_id = ?;";
qboolean G_DBDeleteAllRatingsForPosition(int raterAccountId, ctfPosition_t pos) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, pos ? sqlDeleteAllRatingsOnPosition : sqlDeleteAllRatings, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, raterAccountId);
	if (pos)
		sqlite3_bind_int(statement, 2, pos);
	int rc = trap_sqlite3_step(statement);
	trap_sqlite3_finalize(statement);
	return !!(rc == SQLITE_DONE);
}

// rounding ratings to the nearest tier allows teams to be much more flexible/interchangeable
extern qboolean PlayerRatingAccountIdMatches(genericNode_t *node, void *userData);
const char *const sqlGetAverageRatings =
"WITH t AS ( "
"    SELECT "
"        account_id, "
"        created_on, "
"        COALESCE(bannedPos, 0) AS banned_positions "
"    FROM accounts "
"), "
"ratings AS ( "
"    SELECT ratee_account_id, pos, rating "
"    FROM playerratings "
"    JOIN accounts ON accounts.account_id = rater_account_id "
"    WHERE (accounts.flags & (1 << 6)) != 0 "
"      AND (accounts.flags & (1 << 10)) == 0 "
"), "
"default_ratings AS ( "
"    SELECT "
"        a.account_id AS ratee_account_id, "
"        3 AS pos, "
"        4 AS rating "
"    FROM accounts a "
"    WHERE NOT EXISTS ( "
"        SELECT 1 "
"        FROM playerratings pr "
"        WHERE pr.ratee_account_id = a.account_id "
"    ) "
") "
"SELECT "
"    r.ratee_account_id, "
"    r.pos, "
"    CAST(round(avg(r.rating)) AS INTEGER) AS avg_rating "
"FROM ( "
"    SELECT ratee_account_id, pos, rating FROM ratings "
"    UNION ALL "
"    SELECT ratee_account_id, pos, rating FROM default_ratings "
") r "
"JOIN t ON t.account_id = r.ratee_account_id "
"WHERE (t.banned_positions & (1 << (r.pos - 1))) = 0 "
"GROUP BY r.ratee_account_id, r.pos; ";

// loads player ratings, taking into account rust and pos bans
void G_DBGetPlayerRatings(void) {
	int start = trap_Milliseconds();
	ListClear(&level.ratingList);

	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlGetAverageRatings, -1, &statement, 0);
	int rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		int accountId = sqlite3_column_int(statement, 0);
		ctfPosition_t pos = sqlite3_column_int(statement, 1);
		if (pos < CTFPOSITION_BASE || pos > CTFPOSITION_OFFENSE) {
			assert(qfalse);
			rc = trap_sqlite3_step(statement);
			continue; // ???
		}
		int averageTier = sqlite3_column_int(statement, 2);

		qboolean isRusty = qfalse;
		if (g_vote_teamgen_rustWeeks.integer > 0 && averageTier > PLAYERRATING_MID_G) {
			rustyPlayer_t *found = ListFind(&level.rustyPlayersList, RustyPlayerMatches, &accountId, NULL);
			if (found) {
				averageTier -= 1;
				isRusty = qtrue;
#ifdef DEBUG_RUST
				Com_Printf("Player %d is rusty, so decreasing pos %d rating to %d (was %d)\n", accountId, pos, averageTier, averageTier + 1);
#endif
			}
		}

		double rating = PlayerTierToRating(averageTier);

		playerRating_t findMe;
		findMe.accountId = accountId;
		playerRating_t *found = ListFind(&level.ratingList, PlayerRatingAccountIdMatches, &findMe, NULL);
		if (!found) {
			found = ListAdd(&level.ratingList, sizeof(playerRating_t));
			found->accountId = accountId;
			found->isRusty = isRusty;
			memset(found->rating, 0, sizeof(found->rating));
		}

		found->rating[pos] = rating;

		rc = trap_sqlite3_step(statement);
	}
	trap_sqlite3_finalize(statement);
	int finish = trap_Milliseconds();
	Com_Printf("Recalculated player ratings (took %d ms)\n", finish - start);
}

void EpochToHumanReadable(double epochTime, char *buffer, size_t bufferSize) {
	time_t rawtime = (time_t)epochTime;
	struct tm *timeinfo = localtime(&rawtime);
	strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", timeinfo);
}

const char *winrateWithDays =
" WITH TimeWindow AS ("
"   SELECT CAST(strftime('%s', 'now') AS REAL) - ?3 AS start_of_window"
" ),"
" RelevantRecords AS ("
"     SELECT ptp.match_id, ptp.session_id, ptp.team, ptp.pos, ptp.duration"
"     FROM playerpugteampos ptp"
"     INNER JOIN sessions s ON ptp.session_id = s.session_id"
"     JOIN filteredpugslist fp ON ptp.match_id = fp.match_id"
"     WHERE s.account_id = ?1"
" ),"
" MaxDurationRecords AS ("
"     SELECT match_id, session_id, team, pos, MAX(duration) as max_duration"
"     FROM RelevantRecords"
"     GROUP BY match_id"
" ),"
" FilteredByPosition AS ("
"     SELECT *"
"     FROM MaxDurationRecords"
"     WHERE pos = ?2"
" ),"
" WinningMatches AS ("
"     SELECT count(*) AS wins"
"     FROM FilteredByPosition fbp"
"     JOIN pugs p ON fbp.match_id = p.match_id"
"     WHERE fbp.team = p.win_team"
" ),"
" TotalMatches AS ("
"     SELECT count(*) AS total"
"     FROM FilteredByPosition"
" )"
" SELECT "
"     (SELECT total FROM TotalMatches) AS number_of_pugs,"
"     (SELECT wins FROM WinningMatches) AS number_of_wins,"
"     (SELECT IFNULL(wins, 0) FROM WinningMatches) * 1.0 / (SELECT total FROM TotalMatches) AS winrate,"
"     (SELECT start_of_window FROM TimeWindow) AS start_of_window;";
const char *winrateSinceRated =
" WITH LatestRating AS ("
"     SELECT MAX(CAST(datetime AS REAL)) AS last_rated"
"     FROM playerratings"
"     WHERE ratee_account_id = ?1 AND pos = ?2"
" ),"
" FilteredPugs AS ("
"     SELECT match_id"
"     FROM pugs"
"     WHERE CAST(datetime AS REAL) >= COALESCE((SELECT last_rated FROM LatestRating), CAST('0' AS REAL)) "
"     AND red_score != blue_score"
" ),"
" TimeWindow AS ("
"     SELECT COALESCE((SELECT last_rated FROM LatestRating), CAST(strftime('%s', 'now') AS REAL)) AS start_of_window"
" ),"
" RelevantRecords AS ("
"     SELECT ptp.match_id, ptp.session_id, ptp.team, ptp.pos, ptp.duration"
"     FROM playerpugteampos ptp"
"     INNER JOIN sessions s ON ptp.session_id = s.session_id"
"     JOIN FilteredPugs fp ON ptp.match_id = fp.match_id"
"     WHERE s.account_id = ?1"
" ),"
" MaxDurationRecords AS ("
"     SELECT match_id, session_id, team, pos, MAX(duration) as max_duration"
"     FROM RelevantRecords"
"     GROUP BY match_id"
" ),"
" FilteredByPosition AS ("
"     SELECT *"
"     FROM MaxDurationRecords"
"     WHERE pos = ?2"
" ),"
" WinningMatches AS ("
"     SELECT count(*) AS wins"
"     FROM FilteredByPosition fbp"
"     JOIN pugs p ON fbp.match_id = p.match_id"
"     WHERE fbp.team = p.win_team"
" ),"
" TotalMatches AS ("
"     SELECT count(*) AS total"
"     FROM FilteredByPosition"
" )"
" SELECT "
"     (SELECT total FROM TotalMatches) AS number_of_pugs,"
"     (SELECT wins FROM WinningMatches) AS number_of_wins,"
"     (SELECT IFNULL(wins, 0) FROM WinningMatches) * 1.0 / (SELECT total FROM TotalMatches) AS winrate,"
"     (SELECT start_of_window FROM TimeWindow) AS start_of_window;";
qboolean G_DBGetWinrateSince(const char *name, const int accountId, const ctfPosition_t pos, const char *daysStr, const int raterClientNum) {

	if (VALIDSTRING(daysStr)) {
		trap_sqlite3_exec(dbPtr, "DROP TABLE IF EXISTS filteredpugslist;", NULL, NULL, NULL);
		sqlite3_stmt *statement;
		const char *sql = "CREATE TEMPORARY TABLE filteredpugslist AS "
			"SELECT match_id FROM pugs "
			"WHERE CAST(datetime AS REAL) >= (CAST(strftime('%s', 'now') AS REAL) - ?) "
			"AND red_score != blue_score;";
		trap_sqlite3_prepare_v2(dbPtr, sql, -1, &statement, NULL);
		double daysInSeconds = atof(daysStr) * 86400; // convert days to seconds
		sqlite3_bind_double(statement, 1, daysInSeconds);
		trap_sqlite3_step(statement);
		trap_sqlite3_finalize(statement);
	}

	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, VALIDSTRING(daysStr) ? winrateWithDays : winrateSinceRated, -1, &statement, NULL);
	sqlite3_bind_int(statement, 1, accountId);
	sqlite3_bind_int(statement, 2, pos);
	int rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		int total_matches = sqlite3_column_int(statement, 0);
		int wins = sqlite3_column_int(statement, 1);
		float winrate = (float)sqlite3_column_double(statement, 2);

		char sinceStr[256] = { 0 };
		if (VALIDSTRING(daysStr)) {
			double daysInSeconds = atof(daysStr) * 86400; // convert days to seconds
			double since = time(NULL) - daysInSeconds;
			EpochToHumanReadable(since, sinceStr, sizeof(sinceStr));
			OutOfBandPrint(raterClientNum, "^5%s %s ^7since ^3%s days ago^7 (%s):   ^5%d^7 pugs, ^5%d^7 wins, ^5%g^7'/. winrate\n", name, NameForPos(pos), daysStr, sinceStr, total_matches, wins, winrate * 100.0);
		}
		else {
			double date = sqlite3_column_double(statement, 3);
			EpochToHumanReadable(date, sinceStr, sizeof(sinceStr));
			double secondsDifference = difftime(time(NULL), (time_t)date);
			double days = secondsDifference / (24 * 3600);
			OutOfBandPrint(raterClientNum, "^5%s %s ^7since ^6last rated %.1f days ago^7 (on %s):   ^5%d^7 pugs, ^5%d^7 wins, ^5%g^7'/. winrate\n", name, NameForPos(pos), days, sinceStr, total_matches, wins, winrate * 100.0);
		}
	}
	else {
		OutOfBandPrint(raterClientNum, "No records found.\n");
	}

	trap_sqlite3_finalize(statement);
	return rc == SQLITE_ROW ? qtrue : qfalse;
}


typedef struct {
	node_t		node;
	int			recordId;
	char		recordIdStr[32];
	int64_t		matchId;
	int			duration;
	int			durationSeconds;
	qboolean	boonIsNull;
	qboolean	teEfficiencyIsNull;
	qboolean	fcKillEfficiencyIsNull;
	int			impliedFcKillsReturned;
	char		teamStr[4];
	char		posStr[4];
	stats_t		stats;
	char		dateTimeStr[64];
	char		mapName[MAX_QPATH];
} swapData_t;

const char *SwapIntegerCallback(void *rowContext, void *columnContext) {
	unsigned int swapAddr = (unsigned int)rowContext;
	unsigned int offset = (unsigned int)columnContext;
	int n = *((int *)(swapAddr + offset));
	return va("%d", n);
}

const char *SwapInt64Callback(void *rowContext, void *columnContext) {
	unsigned int swapAddr = (unsigned int)rowContext;
	unsigned int offset = (unsigned int)columnContext;
	int64_t n = *((int64_t *)(swapAddr + offset));
	return va("%llx", n);
}

const char *SwapFcKillEfficiencyCallback(void *rowContext, void *columnContext) {
	swapData_t *swap = (swapData_t *)rowContext;
	if (!swap->stats.fcKills)
		return "";
	return va("%d", swap->stats.fcKillEfficiency);
}

const char *SwapBoonCallback(void *rowContext, void *columnContext) {
	swapData_t *swap = (swapData_t *)rowContext;
	if (swap->boonIsNull)
		return "";
	return va("%d", swap->stats.boonPickups);
}

const char *SwapTEEfficiencyCallback(void *rowContext, void *columnContext) {
	swapData_t *swap = (swapData_t *)rowContext;
	if (swap->teEfficiencyIsNull)
		return "";
	return va("%d", swap->stats.energizeEfficiency);
}

const char *SwapTimeCallback(void *rowContext, void *columnContext) {
	unsigned int swapAddr = (unsigned int)rowContext;
	unsigned int offset = (unsigned int)columnContext;
	int s = *((int *)(swapAddr + offset));
	if (s >= 60) {
		int m = s / 60;
		s %= 60;
		return va("%02dm%ds", m, s);
	}
	else {
		return va("%ds", s);
	}
}

const char *GenericTableStringCallback(void *rowContext, void *columnContext) {
	unsigned int swapAddr = (unsigned int)rowContext;
	unsigned int offset = (unsigned int)columnContext;
	const char *string = ((const char *)(swapAddr + offset));
	return va("%s", string);
}

const char *const sqlGetFixSwapList = "WITH t AS (SELECT match_id, session_id FROM playerpugteampos GROUP BY match_id, session_id HAVING COUNT(*) > 1) SELECT * FROM playerpugteampos JOIN pugs ON pugs.match_id = playerpugteampos.match_id WHERE EXISTS (SELECT 1 FROM t WHERE t.match_id = playerpugteampos.match_id AND t.session_id = playerpugteampos.session_id) ORDER BY match_id, session_id, pos ASC;";
void G_DBFixSwap_List(void) {
	sqlite3_stmt *statement;

	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetFixSwapList, -1, &statement, 0);

	list_t swapDataList = { 0 };
	rc = trap_sqlite3_step(statement);
	int numGotten = 0;
	while (rc == SQLITE_ROW) {
		++numGotten;
		swapData_t *swap = ListAdd(&swapDataList, sizeof(swapData_t));
		int num = 0;
		swap->recordId = sqlite3_column_int(statement, num++);
		Com_sprintf(swap->recordIdStr, sizeof(swap->recordIdStr), "^3%d", swap->recordId);
		swap->matchId = sqlite3_column_int64(statement, num++);
		swap->stats.sessionId = sqlite3_column_int(statement, num++);
		swap->stats.lastTeam = sqlite3_column_int(statement, num++);
		if (swap->stats.lastTeam == TEAM_RED) {
			Q_strncpyz(swap->teamStr, "^1R", sizeof(swap->teamStr));
		}
		else if (swap->stats.lastTeam == TEAM_BLUE) {
			Q_strncpyz(swap->teamStr, "^4B", sizeof(swap->teamStr));
		}
		else {
			Q_strncpyz(swap->teamStr, "?", sizeof(swap->teamStr));
			assert(qfalse);
		}
		swap->duration = sqlite3_column_int(statement, num++);
		swap->durationSeconds = swap->duration / 1000;
		const char *name = (const char *)sqlite3_column_text(statement, num++);
		if (VALIDSTRING(name))
			Q_strncpyz(swap->stats.name, name, sizeof(swap->stats.name));
		swap->stats.finalPosition = sqlite3_column_int(statement, num++);
		if (swap->stats.finalPosition == CTFPOSITION_BASE) {
			Q_strncpyz(swap->posStr, "Bas", sizeof(swap->posStr));
		}
		else if (swap->stats.finalPosition == CTFPOSITION_CHASE) {
			Q_strncpyz(swap->posStr, "Cha", sizeof(swap->posStr));
		}
		else if (swap->stats.finalPosition == CTFPOSITION_OFFENSE) {
			Q_strncpyz(swap->posStr, "Off", sizeof(swap->posStr));
		}
		else {
			Q_strncpyz(swap->posStr, "???", sizeof(swap->posStr));
			assert(qfalse);
		}
		swap->stats.captures = sqlite3_column_int(statement, num++);
		swap->stats.assists = sqlite3_column_int(statement, num++);
		swap->stats.defends = sqlite3_column_int(statement, num++);
		swap->stats.accuracy = sqlite3_column_int(statement, num++);
		swap->stats.airs = sqlite3_column_int(statement, num++);
		swap->stats.teamKills = sqlite3_column_int(statement, num++);
		swap->stats.takes = sqlite3_column_int(statement, num++);
		swap->stats.pits = sqlite3_column_int(statement, num++);
		swap->stats.pitted = sqlite3_column_int(statement, num++);
		swap->stats.damageDealtTotal = sqlite3_column_int(statement, num++);
		swap->stats.flagCarrierDamageDealtTotal = sqlite3_column_int(statement, num++);
		swap->stats.clearDamageDealtTotal = sqlite3_column_int(statement, num++);
		swap->stats.otherDamageDealtTotal = sqlite3_column_int(statement, num++);
		swap->stats.damageTakenTotal = sqlite3_column_int(statement, num++);
		swap->stats.flagCarrierDamageTakenTotal = sqlite3_column_int(statement, num++);
		swap->stats.clearDamageTakenTotal = sqlite3_column_int(statement, num++);
		swap->stats.otherDamageTakenTotal = sqlite3_column_int(statement, num++);
		swap->stats.fcKills = sqlite3_column_int(statement, num++);
		if (swap->stats.fcKills > 0)
			swap->stats.fcKillEfficiency = sqlite3_column_int(statement, num/*++*/);
		++num;
		swap->stats.rets = sqlite3_column_int(statement, num++);
		swap->stats.selfkills = sqlite3_column_int(statement, num++);
		swap->stats.totalFlagHold = sqlite3_column_int(statement, num++);
		swap->stats.longestFlagHold = sqlite3_column_int(statement, num++);
		swap->stats.averageSpeed = sqlite3_column_int(statement, num++);
		swap->stats.displacementSamples = sqlite3_column_int(statement, num++); // actually top speed
		if (sqlite3_column_type(statement, num/*++*/) != SQLITE_NULL) {
			swap->stats.boonPickups = sqlite3_column_int(statement, num++);
		}
		else {
			swap->boonIsNull = qtrue;
		}
		++num;
		swap->stats.push = sqlite3_column_int(statement, num++);
		swap->stats.pull = sqlite3_column_int(statement, num++);
		swap->stats.healed = sqlite3_column_int(statement, num++);
		swap->stats.energizedAlly = sqlite3_column_int(statement, num++);
		if (sqlite3_column_type(statement, num/*++*/) != SQLITE_NULL) {
			swap->stats.energizeEfficiency = sqlite3_column_int(statement, num/*++*/);
		}
		else {
			swap->teEfficiencyIsNull = qtrue;
		}
		++num;
		swap->stats.energizedEnemy = sqlite3_column_int(statement, num++);
		swap->stats.absorbed = sqlite3_column_int(statement, num++);
		swap->stats.protDamageAvoided = sqlite3_column_int(statement, num++);
		swap->stats.protTimeUsed = sqlite3_column_int(statement, num++);
		swap->stats.rageTimeUsed = sqlite3_column_int(statement, num++);
		swap->stats.drain = sqlite3_column_int(statement, num++);
		swap->stats.gotDrained = sqlite3_column_int(statement, num++);
		swap->stats.regionPercent[CTFREGION_FLAGSTAND] = sqlite3_column_int(statement, num++);
		swap->stats.regionPercent[CTFREGION_BASE] = sqlite3_column_int(statement, num++);
		swap->stats.regionPercent[CTFREGION_MID] = sqlite3_column_int(statement, num++);
		swap->stats.regionPercent[CTFREGION_ENEMYBASE] = sqlite3_column_int(statement, num++);
		swap->stats.regionPercent[CTFREGION_ENEMYFLAGSTAND] = sqlite3_column_int(statement, num++);
		swap->stats.gotEnergizedByAlly = sqlite3_column_int(statement, num++);
		swap->stats.grips = sqlite3_column_int(statement, num++);
		swap->stats.gotGripped = sqlite3_column_int(statement, num++);
		swap->stats.darkPercent = sqlite3_column_int(statement, num++);
		num++; // skip match id from pugs table
		time_t dateTime = sqlite3_column_int64(statement, num++);
		G_FormatLocalDateFromEpoch(swap->dateTimeStr, sizeof(swap->dateTimeStr), dateTime);
		const char *mapName = (const char *)sqlite3_column_text(statement, num++);
		if (VALIDSTRING(mapName))
			GetShortNameForMapFileName(mapName, swap->mapName, sizeof(swap->mapName));
		rc = trap_sqlite3_step(statement);
	}
	trap_sqlite3_finalize(statement);

	if (!numGotten) {
		Com_Printf("No results found.\n");
		return;
	}

	iterator_t iter;
	ListIterate(&swapDataList, &iter, qfalse);
	Table *t = Table_Initialize(qtrue);
	while (IteratorHasNext(&iter)) {
		swapData_t *swap = (swapData_t *)IteratorNext(&iter);
		Table_DefineRow(t, swap);
	}

	swapData_t swap = { 0 };
	Table_DefineColumn(t, "^3Record#", GenericTableStringCallback, (void *)((unsigned int)(&swap.recordIdStr) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Match#", SwapInt64Callback, (void *)((unsigned int)(&swap.matchId) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Session#", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.sessionId) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Map", GenericTableStringCallback, (void *)((unsigned int)(&swap.mapName) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5DateTime", GenericTableStringCallback, (void *)((unsigned int)(&swap.dateTimeStr) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Team", GenericTableStringCallback, (void *)((unsigned int)(&swap.teamStr) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Dur", SwapTimeCallback, (void *)((unsigned int)(&swap.durationSeconds) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Name", GenericTableStringCallback, (void *)((unsigned int)(&swap.stats.name) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Pos", GenericTableStringCallback, (void *)((unsigned int)(&swap.posStr) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Cap", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.captures) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Ass", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.assists) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Def", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.defends) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Acc", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.accuracy) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Air", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.airs) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5TK", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.teamKills) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Take", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.takes) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^6Pit", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.pits) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^6Dth", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.pitted) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^2Dmg", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.damageDealtTotal) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^2Fc", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.flagCarrierDamageDealtTotal) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^2Clr", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.clearDamageDealtTotal) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^2Othr", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.otherDamageDealtTotal) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^1Tkn", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.damageTakenTotal) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^1Fc", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.flagCarrierDamageTakenTotal) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^1Clr", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.clearDamageTakenTotal) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^1Othr", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.otherDamageTakenTotal) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^6FcKil", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.fcKills) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^6Eff", SwapFcKillEfficiencyCallback, NULL, qfalse, -1, 32);
	Table_DefineColumn(t, "^5Ret", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.rets) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5SK", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.selfkills) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^3Hold", SwapTimeCallback, (void *)((unsigned int)(&swap.stats.totalFlagHold) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^3Max", SwapTimeCallback, (void *)((unsigned int)(&swap.stats.longestFlagHold) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^6Spd", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.averageSpeed) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^6Top", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats./*topSpeed*/displacementSamples) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Boon", SwapBoonCallback, NULL, qfalse, -1, 32);
	Table_DefineColumn(t, "^5Push", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.push) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Pull", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.pull) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Heal", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.healed) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^6TE", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.energizedAlly) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^6Eff", SwapTEEfficiencyCallback, NULL, qfalse, -1, 32);
	Table_DefineColumn(t, "^6Rcvd", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.gotEnergizedByAlly) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5EnemyNrg", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.energizedEnemy) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Abs", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.absorbed) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^2Prot", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.protDamageAvoided) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^2Time", SwapTimeCallback, (void *)((unsigned int)(&swap.stats.protTimeUsed) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Rage", SwapTimeCallback, (void *)((unsigned int)(&swap.stats.rageTimeUsed) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^1Drn", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.drain) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^1Drnd", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.gotDrained) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^1Gr", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.grips) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^1Gd", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.gotGripped) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^1Drk", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.darkPercent) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Fs", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.regionPercent[CTFREGION_FLAGSTAND]) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Bas", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.regionPercent[CTFREGION_BASE]) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Mid", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.regionPercent[CTFREGION_MID]) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5Eba", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.regionPercent[CTFREGION_ENEMYBASE]) - (unsigned int)&swap), qfalse, -1, 32);
	Table_DefineColumn(t, "^5EFs", SwapIntegerCallback, (void *)((unsigned int)(&swap.stats.regionPercent[CTFREGION_ENEMYFLAGSTAND]) - (unsigned int)&swap), qfalse, -1, 32);

	int bufSize = 1024 * numGotten;
	char *buf = calloc(bufSize, sizeof(char));
	Table_WriteToBuffer(t, buf, bufSize, qtrue, -1);
	Com_Printf("Players with multiple positions logged in the same pug:\n");

	// should write a function to do this stupid chunked printing
	char *remaining = buf;
	int totalLen = strlen(buf);
	while (*remaining && remaining < buf + totalLen) {
		char temp[4096] = { 0 };
		Q_strncpyz(temp, remaining, sizeof(temp));
		Com_Printf(temp);
		int copied = strlen(temp);
		remaining += copied;
	}

	Com_Printf("^3NOTE:^7 The goal is to distinguish between ^2legitimate swaps^7 and ^1false positives^7. ^3Do not simply try to eliminate the entire list.^7 Just try to find obvious false positives, e.g. someone supposedly playing base for 5 minutes but having 0 damage.\n");

	free(buf);
	Table_Destroy(t);
	ListClear(&swapDataList);
}

const char *const sqlGetSessionIdAndMatchIdFromRecordId = "SELECT session_id, match_id FROM playerpugteampos WHERE playerpugteampos_id = ?;";
const char *const sqlGetParticularSwapRecord = "SELECT * FROM playerpugteampos WHERE playerpugteampos_id = ?;";
const char *const sqlSimpleSwapUpdate = "UPDATE playerpugteampos SET pos = ?1 WHERE playerpugteampos_id = ?2;";
const char *const sqlGetParticularFixSwapList = "WITH t AS (SELECT * FROM playerpugteampos GROUP BY match_id, session_id HAVING COUNT(*) > 1) SELECT * FROM playerpugteampos WHERE EXISTS (SELECT 1 FROM t WHERE t.match_id = playerpugteampos.match_id AND t.session_id = playerpugteampos.session_id) AND session_id = ?1 AND match_id = ?2 AND pos = ?3 ORDER BY match_id, session_id, pos ASC;";
const char *const sqlUpdateSwap = "UPDATE playerpugteampos SET duration = ?, name = ?, pos = ?, cap = ?, ass = ?, def = ?, acc = ?, air = ?, tk = ?, take = ?, pitkil = ?, pitdth = ?, dmg = ?, fcdmg = ?, clrdmg = ?, othrdmg = ?, dmgtkn = ?, fcdmgtkn = ?, clrdmgtkn = ?, othrdmgtkn = ?, fckil = ?, fckileff = ?, ret = ?, sk = ?, ttlhold = ?, maxhold = ?, avgspd = ?, topspd = ?, boon = ?, push = ?, pull = ?, heal = ?, te = ?, teeff = ?, enemynrg = ?, absorb = ?, protdmg = ?, prottime = ?, rage = ?, drain = ?, drained = ?, fs = ?, bas = ?, mid = ?, eba = ?, efs = ?, gotte = ?, grips = ?, gripped = ?, dark = ? WHERE playerpugteampos_id = ?;";
const char *const sqlDeleteSwap = "DELETE FROM playerpugteampos WHERE playerpugteampos_id = ?;";
qboolean G_DBFixSwap_Fix(int recordId, int newPos) {
	// get the session id and match id of the record in question
	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetSessionIdAndMatchIdFromRecordId, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, recordId);
	rc = trap_sqlite3_step(statement);
	if (rc != SQLITE_ROW) {
		trap_sqlite3_finalize(statement);
		return qfalse;
	}
	const int sessionId = sqlite3_column_int(statement, 0);
	const int64_t matchId = sqlite3_column_int64(statement, 1);

	// get the record in question
	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetParticularSwapRecord, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, recordId);
	rc = trap_sqlite3_step(statement);
	if (rc != SQLITE_ROW) {
		trap_sqlite3_finalize(statement);
		return qfalse;
	}
	list_t swapDataList = { 0 };
	swapData_t *src = ListAdd(&swapDataList, sizeof(swapData_t));
	int num = 0;
	src->recordId = sqlite3_column_int(statement, num++);
	src->matchId = sqlite3_column_int64(statement, num++);
	src->stats.sessionId = sqlite3_column_int(statement, num++);
	src->stats.lastTeam = sqlite3_column_int(statement, num++);
	src->duration = sqlite3_column_int(statement, num++);
	src->durationSeconds = src->duration / 1000;/*
	const char *name = (const char *)sqlite3_column_text(statement, num++);
	if (VALIDSTRING(name))
		Q_strncpyz(src->stats.name, name, sizeof(src->stats.name));*/
	num++;
	src->stats.finalPosition = sqlite3_column_int(statement, num++);
	src->stats.captures = sqlite3_column_int(statement, num++);
	src->stats.assists = sqlite3_column_int(statement, num++);
	src->stats.defends = sqlite3_column_int(statement, num++);
	src->stats.accuracy = sqlite3_column_int(statement, num++);
	src->stats.airs = sqlite3_column_int(statement, num++);
	src->stats.teamKills = sqlite3_column_int(statement, num++);
	src->stats.takes = sqlite3_column_int(statement, num++);
	src->stats.pits = sqlite3_column_int(statement, num++);
	src->stats.pitted = sqlite3_column_int(statement, num++);
	src->stats.damageDealtTotal = sqlite3_column_int(statement, num++);
	src->stats.flagCarrierDamageDealtTotal = sqlite3_column_int(statement, num++);
	src->stats.clearDamageDealtTotal = sqlite3_column_int(statement, num++);
	src->stats.otherDamageDealtTotal = sqlite3_column_int(statement, num++);
	src->stats.damageTakenTotal = sqlite3_column_int(statement, num++);
	src->stats.flagCarrierDamageTakenTotal = sqlite3_column_int(statement, num++);
	src->stats.clearDamageTakenTotal = sqlite3_column_int(statement, num++);
	src->stats.otherDamageTakenTotal = sqlite3_column_int(statement, num++);
	src->stats.fcKills = sqlite3_column_int(statement, num++);
	if (src->stats.fcKills > 0) {
		src->stats.fcKillEfficiency = sqlite3_column_int(statement, num/*++*/);
		src->impliedFcKillsReturned = (int)round((double)src->stats.fcKillEfficiency / 100.0);
	}
	++num;
	src->stats.rets = sqlite3_column_int(statement, num++);
	src->stats.selfkills = sqlite3_column_int(statement, num++);
	src->stats.totalFlagHold = sqlite3_column_int(statement, num++);
	src->stats.longestFlagHold = sqlite3_column_int(statement, num++);
	src->stats.averageSpeed = sqlite3_column_int(statement, num++);
	src->stats.displacementSamples = sqlite3_column_int(statement, num++); // actually top speed
	if (sqlite3_column_type(statement, num/*++*/) != SQLITE_NULL) {
		src->stats.boonPickups = sqlite3_column_int(statement, num++);
	}
	else {
		src->boonIsNull = qtrue;
	}
	++num;
	src->stats.push = sqlite3_column_int(statement, num++);
	src->stats.pull = sqlite3_column_int(statement, num++);
	src->stats.healed = sqlite3_column_int(statement, num++);
	src->stats.energizedAlly = sqlite3_column_int(statement, num++);
	if (sqlite3_column_type(statement, num/*++*/) != SQLITE_NULL) {
		src->stats.energizeEfficiency = sqlite3_column_int(statement, num/*++*/);
	}
	else {
		src->teEfficiencyIsNull = qtrue;
	}
	++num;
	src->stats.energizedEnemy = sqlite3_column_int(statement, num++);
	src->stats.absorbed = sqlite3_column_int(statement, num++);
	src->stats.protDamageAvoided = sqlite3_column_int(statement, num++);
	src->stats.protTimeUsed = sqlite3_column_int(statement, num++);
	src->stats.rageTimeUsed = sqlite3_column_int(statement, num++);
	src->stats.drain = sqlite3_column_int(statement, num++);
	src->stats.gotDrained = sqlite3_column_int(statement, num++);
	src->stats.regionPercent[CTFREGION_FLAGSTAND] = sqlite3_column_int(statement, num++);
	src->stats.regionPercent[CTFREGION_BASE] = sqlite3_column_int(statement, num++);
	src->stats.regionPercent[CTFREGION_MID] = sqlite3_column_int(statement, num++);
	src->stats.regionPercent[CTFREGION_ENEMYBASE] = sqlite3_column_int(statement, num++);
	src->stats.regionPercent[CTFREGION_ENEMYFLAGSTAND] = sqlite3_column_int(statement, num++);
	src->stats.gotEnergizedByAlly = sqlite3_column_int(statement, num++);

	// see if there is a record that already exists in the desired position
	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetParticularFixSwapList, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, sessionId);
	sqlite3_bind_int64(statement, 2, matchId);
	sqlite3_bind_int(statement, 3, newPos);
	rc = trap_sqlite3_step(statement);
	if (rc != SQLITE_ROW) {
		// there is no record already existing in the desired position; just change this one
		trap_sqlite3_reset(statement);
		rc = trap_sqlite3_prepare_v2(dbPtr, sqlSimpleSwapUpdate, -1, &statement, 0);
		sqlite3_bind_int(statement, 1, recordId);
		sqlite3_bind_int(statement, 2, newPos);
		rc = trap_sqlite3_step(statement);
		trap_sqlite3_finalize(statement);
		return qtrue;
	}

	// there is already a record existing the desired position; we have to merge them
	// get the record that is already in the desired position
	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlGetParticularFixSwapList, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, sessionId);
	sqlite3_bind_int64(statement, 2, matchId);
	sqlite3_bind_int(statement, 3, newPos);
	rc = trap_sqlite3_step(statement);
	if (rc != SQLITE_ROW) { // ???
		trap_sqlite3_finalize(statement);
		ListClear(&swapDataList);
		return qfalse;
	}
	swapData_t *dest = ListAdd(&swapDataList, sizeof(swapData_t));
	num = 0;
	dest->recordId = sqlite3_column_int(statement, num++);
	dest->matchId = sqlite3_column_int64(statement, num++);
	dest->stats.sessionId = sqlite3_column_int(statement, num++);
	dest->stats.lastTeam = sqlite3_column_int(statement, num++);
	dest->duration = sqlite3_column_int(statement, num++);
	int totalDuration = dest->duration + src->duration;
	double destProportion = ((double)dest->duration) / ((double)totalDuration);
	double srcProportion = ((double)src->duration) / ((double)totalDuration);
	const char *name = (const char *)sqlite3_column_text(statement, num++);
	if (VALIDSTRING(name))
		Q_strncpyz(dest->stats.name, name, sizeof(dest->stats.name));
	dest->stats.finalPosition = sqlite3_column_int(statement, num++);
	dest->stats.captures = sqlite3_column_int(statement, num++) + src->stats.captures;
	dest->stats.assists = sqlite3_column_int(statement, num++) + src->stats.assists;
	dest->stats.defends = sqlite3_column_int(statement, num++) + src->stats.defends;
	dest->stats.accuracy = (int)((((double)sqlite3_column_int(statement, num++)) * destProportion) + (((double)src->stats.accuracy) * srcProportion));
	dest->stats.airs = sqlite3_column_int(statement, num++) + src->stats.airs;
	dest->stats.teamKills = sqlite3_column_int(statement, num++) + src->stats.teamKills;
	dest->stats.takes = sqlite3_column_int(statement, num++) + src->stats.takes;
	dest->stats.pits = sqlite3_column_int(statement, num++) + src->stats.pits;
	dest->stats.pitted = sqlite3_column_int(statement, num++) + src->stats.pitted;
	dest->stats.damageDealtTotal = sqlite3_column_int(statement, num++) + src->stats.damageDealtTotal;
	dest->stats.flagCarrierDamageDealtTotal = sqlite3_column_int(statement, num++) + src->stats.flagCarrierDamageDealtTotal;
	dest->stats.clearDamageDealtTotal = sqlite3_column_int(statement, num++) + src->stats.clearDamageDealtTotal;
	dest->stats.otherDamageDealtTotal = sqlite3_column_int(statement, num++) + src->stats.otherDamageDealtTotal;
	dest->stats.damageTakenTotal = sqlite3_column_int(statement, num++) + src->stats.damageTakenTotal;
	dest->stats.flagCarrierDamageTakenTotal = sqlite3_column_int(statement, num++) + src->stats.flagCarrierDamageTakenTotal;
	dest->stats.clearDamageTakenTotal = sqlite3_column_int(statement, num++) + src->stats.clearDamageTakenTotal;
	dest->stats.otherDamageTakenTotal = sqlite3_column_int(statement, num++) + src->stats.otherDamageTakenTotal;
	dest->stats.fcKills = sqlite3_column_int(statement, num++) + src->stats.fcKills;
	dest->stats.fcKillEfficiency = sqlite3_column_int(statement, num/*++*/);
	dest->impliedFcKillsReturned = (int)round((double)dest->stats.fcKillEfficiency / 100.0);
	if (dest->stats.fcKills > 0 && src->stats.fcKills > 0) {
		int totalFcKills = dest->stats.fcKills + src->stats.fcKills;
		int totalImpliedFcKillsReturned = dest->impliedFcKillsReturned + src->impliedFcKillsReturned;
		dest->stats.fcKillEfficiency = (int)(((double)totalImpliedFcKillsReturned) / ((double)totalFcKills) * 100.0);
	}
	else if (dest->stats.fcKills > 0) {
	}
	else if (src->stats.fcKills > 0) {
		dest->stats.fcKillEfficiency = src->stats.fcKillEfficiency;
	}
	else {
		dest->fcKillEfficiencyIsNull = qtrue;
	}
	++num;
	dest->stats.rets = sqlite3_column_int(statement, num++) + src->stats.rets;
	dest->stats.selfkills = sqlite3_column_int(statement, num++) + src->stats.selfkills;
	dest->stats.totalFlagHold = sqlite3_column_int(statement, num++) + src->stats.totalFlagHold;
	dest->stats.longestFlagHold = sqlite3_column_int(statement, num++);
	if (src->stats.longestFlagHold > dest->stats.longestFlagHold)
		dest->stats.longestFlagHold = src->stats.longestFlagHold;
	dest->stats.averageSpeed = (int)((((double)sqlite3_column_int(statement, num++)) * destProportion) + (((double)src->stats.averageSpeed) * srcProportion));
	dest->stats.displacementSamples = sqlite3_column_int(statement, num++); // actually top speed
	if (src->stats.displacementSamples > dest->stats.displacementSamples)
		dest->stats.displacementSamples = src->stats.displacementSamples;
	if (sqlite3_column_type(statement, num/*++*/) != SQLITE_NULL) {
		dest->stats.boonPickups = sqlite3_column_int(statement, num++);
	}
	else {
		dest->boonIsNull = qtrue;
	}
	if (src->boonIsNull)
		dest->boonIsNull = qtrue; // sanity check
	++num;
	dest->stats.push = sqlite3_column_int(statement, num++) + src->stats.push;
	dest->stats.pull = sqlite3_column_int(statement, num++) + src->stats.pull;
	dest->stats.healed = sqlite3_column_int(statement, num++) + src->stats.healed;
	dest->stats.energizedAlly = sqlite3_column_int(statement, num++) + src->stats.energizedAlly;
	if (sqlite3_column_type(statement, num/*++*/) != SQLITE_NULL) {
		dest->stats.energizeEfficiency = sqlite3_column_int(statement, num/*++*/);
	}
	else {
		dest->teEfficiencyIsNull = qtrue;
	}
	if (dest->teEfficiencyIsNull && src->teEfficiencyIsNull) { // no te in either

	}
	else if (!dest->teEfficiencyIsNull && !src->teEfficiencyIsNull) { // te in both
		dest->stats.fcKillEfficiency = (int)((((double)dest->stats.fcKillEfficiency) * destProportion) + (((double)src->stats.fcKillEfficiency) * srcProportion));
	}
	else if (src->teEfficiencyIsNull) { // only te in dest
	}
	else if (dest->teEfficiencyIsNull) { // only te in src
		dest->stats.fcKillEfficiency = src->stats.fcKillEfficiency;
	}
	++num;
	dest->stats.energizedEnemy = sqlite3_column_int(statement, num++) + src->stats.energizedEnemy;
	dest->stats.absorbed = sqlite3_column_int(statement, num++) + src->stats.absorbed;
	dest->stats.protDamageAvoided = sqlite3_column_int(statement, num++) + src->stats.protDamageAvoided;
	dest->stats.protTimeUsed = sqlite3_column_int(statement, num++) + src->stats.protTimeUsed;
	dest->stats.rageTimeUsed = sqlite3_column_int(statement, num++) + src->stats.rageTimeUsed;
	dest->stats.drain = sqlite3_column_int(statement, num++) + src->stats.drain;
	dest->stats.gotDrained = sqlite3_column_int(statement, num++) + src->stats.gotDrained;
	dest->stats.regionPercent[CTFREGION_FLAGSTAND] = (int)((((double)sqlite3_column_int(statement, num++)) * destProportion) + (((double)src->stats.regionPercent[CTFREGION_FLAGSTAND]) * srcProportion));
	dest->stats.regionPercent[CTFREGION_BASE] = (int)((((double)sqlite3_column_int(statement, num++)) * destProportion) + (((double)src->stats.regionPercent[CTFREGION_BASE]) * srcProportion));
	dest->stats.regionPercent[CTFREGION_MID] = (int)((((double)sqlite3_column_int(statement, num++)) * destProportion) + (((double)src->stats.regionPercent[CTFREGION_MID]) * srcProportion));
	dest->stats.regionPercent[CTFREGION_ENEMYBASE] = (int)((((double)sqlite3_column_int(statement, num++)) * destProportion) + (((double)src->stats.regionPercent[CTFREGION_ENEMYBASE]) * srcProportion));
	dest->stats.regionPercent[CTFREGION_ENEMYFLAGSTAND] = (int)((((double)sqlite3_column_int(statement, num++)) * destProportion) + (((double)src->stats.regionPercent[CTFREGION_ENEMYFLAGSTAND]) * srcProportion));
	dest->stats.gotEnergizedByAlly = sqlite3_column_int(statement, num++) + src->stats.gotEnergizedByAlly;

	// write the merged record to db
	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlUpdateSwap, -1, &statement, 0);
	num = 1;
	sqlite3_bind_int(statement, num++, totalDuration);
	sqlite3_bind_text(statement, num++, dest->stats.name, -1, SQLITE_STATIC);
	sqlite3_bind_int(statement, num++, dest->stats.finalPosition);
	sqlite3_bind_int(statement, num++, dest->stats.captures);
	sqlite3_bind_int(statement, num++, dest->stats.assists);
	sqlite3_bind_int(statement, num++, dest->stats.defends);
	sqlite3_bind_int(statement, num++, dest->stats.accuracy);
	sqlite3_bind_int(statement, num++, dest->stats.airs);
	sqlite3_bind_int(statement, num++, dest->stats.teamKills);
	sqlite3_bind_int(statement, num++, dest->stats.takes);
	sqlite3_bind_int(statement, num++, dest->stats.pits);
	sqlite3_bind_int(statement, num++, dest->stats.pitted);
	sqlite3_bind_int(statement, num++, dest->stats.damageDealtTotal);
	sqlite3_bind_int(statement, num++, dest->stats.flagCarrierDamageDealtTotal);
	sqlite3_bind_int(statement, num++, dest->stats.clearDamageDealtTotal);
	sqlite3_bind_int(statement, num++, dest->stats.otherDamageDealtTotal);
	sqlite3_bind_int(statement, num++, dest->stats.damageTakenTotal);
	sqlite3_bind_int(statement, num++, dest->stats.flagCarrierDamageTakenTotal);
	sqlite3_bind_int(statement, num++, dest->stats.clearDamageTakenTotal);
	sqlite3_bind_int(statement, num++, dest->stats.otherDamageTakenTotal);
	sqlite3_bind_int(statement, num++, dest->stats.fcKills);
	if (!dest->fcKillEfficiencyIsNull)
		sqlite3_bind_int(statement, num++, dest->stats.fcKillEfficiency);
	else
		sqlite3_bind_null(statement, num++);
	sqlite3_bind_int(statement, num++, dest->stats.rets);
	sqlite3_bind_int(statement, num++, dest->stats.selfkills);
	sqlite3_bind_int(statement, num++, dest->stats.totalFlagHold);
	sqlite3_bind_int(statement, num++, dest->stats.longestFlagHold);
	sqlite3_bind_int(statement, num++, dest->stats.averageSpeed);
	sqlite3_bind_int(statement, num++, dest->stats.displacementSamples); // actually top speed
	if (!dest->boonIsNull)
		sqlite3_bind_int(statement, num++, dest->stats.boonPickups);
	else
		sqlite3_bind_null(statement, num++);
	sqlite3_bind_int(statement, num++, dest->stats.push);
	sqlite3_bind_int(statement, num++, dest->stats.pull);
	sqlite3_bind_int(statement, num++, dest->stats.healed);
	sqlite3_bind_int(statement, num++, dest->stats.energizedAlly);
	if (!dest->teEfficiencyIsNull)
		sqlite3_bind_int(statement, num++, dest->stats.energizeEfficiency);
	else
		sqlite3_bind_null(statement, num++);
	sqlite3_bind_int(statement, num++, dest->stats.energizedEnemy);
	sqlite3_bind_int(statement, num++, dest->stats.absorbed);
	sqlite3_bind_int(statement, num++, dest->stats.protDamageAvoided);
	sqlite3_bind_int(statement, num++, dest->stats.protTimeUsed / 1000);
	sqlite3_bind_int(statement, num++, dest->stats.rageTimeUsed / 1000);
	sqlite3_bind_int(statement, num++, dest->stats.drain);
	sqlite3_bind_int(statement, num++, dest->stats.gotDrained);
	qboolean hasValidRegions = qfalse;
	for (ctfRegion_t r = CTFREGION_FLAGSTAND; r <= CTFREGION_ENEMYFLAGSTAND; r++)
		if (dest->stats.regionPercent[r]) { hasValidRegions = qtrue; break; }
	if (hasValidRegions) {
		sqlite3_bind_int(statement, num++, dest->stats.regionPercent[CTFREGION_FLAGSTAND]);
		sqlite3_bind_int(statement, num++, dest->stats.regionPercent[CTFREGION_BASE]);
		sqlite3_bind_int(statement, num++, dest->stats.regionPercent[CTFREGION_MID]);
		sqlite3_bind_int(statement, num++, dest->stats.regionPercent[CTFREGION_ENEMYBASE]);
		sqlite3_bind_int(statement, num++, dest->stats.regionPercent[CTFREGION_ENEMYFLAGSTAND]);
	}
	else {
		sqlite3_bind_null(statement, num++);
		sqlite3_bind_null(statement, num++);
		sqlite3_bind_null(statement, num++);
		sqlite3_bind_null(statement, num++);
		sqlite3_bind_null(statement, num++);
	}
	sqlite3_bind_int(statement, num++, dest->stats.gotEnergizedByAlly);
	sqlite3_bind_int(statement, num++, dest->stats.grips);
	sqlite3_bind_int(statement, num++, dest->stats.gotGripped);
	sqlite3_bind_int(statement, num++, dest->stats.darkPercent);
	sqlite3_bind_int(statement, num++, dest->recordId);
	rc = trap_sqlite3_step(statement);
	if (rc != SQLITE_DONE) {
		Com_Printf("Error updating record.\n");
		trap_sqlite3_finalize(statement);
		ListClear(&swapDataList);
		return qfalse;
	}

	// finally, delete the old one
	trap_sqlite3_reset(statement);
	rc = trap_sqlite3_prepare_v2(dbPtr, sqlDeleteSwap, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, recordId);
	rc = trap_sqlite3_step(statement);

	trap_sqlite3_finalize(statement);
	ListClear(&swapDataList);
	if (rc == SQLITE_DONE)
		return qtrue;
	Com_Printf("Error deleting old record.\n");
	return qfalse;
}

void G_DBListMapAliases(void) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, "SELECT filename, alias, CASE WHEN islive IS NOT NULL THEN 1 ELSE 0 END live, CASE WHEN (SELECT 1 FROM tierwhitelist WHERE tierwhitelist.map = mapaliases.alias) THEN 1 ELSE 0 END whitelisted FROM mapaliases ORDER BY alias ASC, filename ASC;", -1, &statement, 0);

	list_t list = { 0 };
	int rc = trap_sqlite3_step(statement), numGotten = 0;
	while (rc == SQLITE_ROW) {
		++numGotten;
		mapAlias_t *ma = ListAdd(&list, sizeof(swapData_t));
		const char *filename = (const char *)sqlite3_column_text(statement, 0);
		const char *alias = (const char *)sqlite3_column_text(statement, 1);
		const int live = sqlite3_column_int(statement, 2);
		const int whitelisted = sqlite3_column_int(statement, 3);

		Q_strncpyz(ma->filename, filename, sizeof(ma->filename));
		Q_strncpyz(ma->alias, alias, sizeof(ma->alias));
		Q_strncpyz(ma->live, live ? "^2Yes" : "No", sizeof(ma->live));

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);

	if (!numGotten) {
		Com_Printf("No map aliases are currently set.\n");
		return;
	}

	iterator_t iter;
	ListIterate(&list, &iter, qfalse);
	Table *t = Table_Initialize(qtrue);
	while (IteratorHasNext(&iter)) {
		mapAlias_t *ma = (mapAlias_t *)IteratorNext(&iter);
		Table_DefineRow(t, ma);
	}

	mapAlias_t ma = { 0 };
	Table_DefineColumn(t, "Filename", GenericTableStringCallback, (void *)((unsigned int)(&ma.filename) - (unsigned int)&ma), qtrue, -1, MAX_QPATH);
	Table_DefineColumn(t, "Alias", GenericTableStringCallback, (void *)((unsigned int)(&ma.alias) - (unsigned int)&ma), qtrue, -1, MAX_QPATH);
	Table_DefineColumn(t, "Live", GenericTableStringCallback, (void *)((unsigned int)(&ma.live) - (unsigned int)&ma), qtrue, -1, MAX_QPATH);

	int bufSize = 1024 * numGotten;
	char *buf = calloc(bufSize, sizeof(char));
	Table_WriteToBuffer(t, buf, bufSize, qtrue, -1);
	Com_Printf("Currently set map aliases:\n");

	// should write a function to do this stupid chunked printing
	char *remaining = buf;
	int totalLen = strlen(buf);
	while (*remaining && remaining < buf + totalLen) {
		char temp[4096] = { 0 };
		Q_strncpyz(temp, remaining, sizeof(temp));
		Com_Printf(temp);
		int copied = strlen(temp);
		remaining += copied;
	}

	free(buf);
	Table_Destroy(t);
	ListClear(&list);
}

qboolean G_DBSetMapAlias(const char *filename, const char *alias, qboolean setLive) {
	sqlite3_stmt *statement;

	if (setLive) {
		trap_sqlite3_prepare_v2(dbPtr, "UPDATE mapaliases SET islive = NULL WHERE alias = ?1;", -1, &statement, 0);
		sqlite3_bind_text(statement, 1, alias, -1, SQLITE_STATIC);
		trap_sqlite3_step(statement);
		trap_sqlite3_reset(statement);
	}

	trap_sqlite3_prepare_v2(dbPtr, va("INSERT INTO mapaliases (filename, alias, islive) VALUES (?1, ?2, %s);", setLive ? "TRUE" : "NULL"), -1, &statement, 0);
	sqlite3_bind_text(statement, 1, filename, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, alias, -1, SQLITE_STATIC);
	trap_sqlite3_step(statement);
	qboolean success = sqlite3_changes(dbPtr) != 0;
	trap_sqlite3_finalize(statement);
	return success;
}

qboolean G_DBClearMapAlias(const char *filename) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, "DELETE FROM mapaliases WHERE filename = ?1;", -1, &statement, 0);
	sqlite3_bind_text(statement, 1, filename, -1, SQLITE_STATIC);
	trap_sqlite3_step(statement);
	qboolean success = sqlite3_changes(dbPtr) != 0;
	trap_sqlite3_finalize(statement);
	return success;
}

qboolean G_DBSetMapAliasLive(const char *filename, char *aliasOut, size_t aliasOutSize) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, "SELECT alias FROM mapaliases WHERE filename = ?;", -1, &statement, 0);
	sqlite3_bind_text(statement, 1, filename, -1, SQLITE_STATIC);
	int rc = trap_sqlite3_step(statement);
	char alias[MAX_QPATH] = { 0 };
	if (rc == SQLITE_ROW) {
		const char *aliasStr = (const char *)sqlite3_column_text(statement, 0);
		Q_strncpyz(alias, aliasStr, sizeof(alias));
	}

	if (!alias[0]) {
		trap_sqlite3_finalize(statement);
		return qfalse;
	}

	trap_sqlite3_reset(statement);
	trap_sqlite3_prepare_v2(dbPtr, "UPDATE mapaliases SET islive = NULL WHERE alias = ?1;", -1, &statement, 0);
	sqlite3_bind_text(statement, 1, alias, -1, SQLITE_STATIC);
	trap_sqlite3_step(statement);
	
	trap_sqlite3_reset(statement);
	trap_sqlite3_prepare_v2(dbPtr, "UPDATE mapaliases SET islive = TRUE WHERE filename = ?1;", -1, &statement, 0);
	sqlite3_bind_text(statement, 1, filename, -1, SQLITE_STATIC);
	trap_sqlite3_step(statement);
	qboolean success = sqlite3_changes(dbPtr) != 0;
	trap_sqlite3_finalize(statement);

	if (aliasOut && aliasOutSize)
		Q_strncpyz(aliasOut, alias, aliasOutSize);

	return success;
}

qboolean G_DBGetLiveMapFilenameForAlias(const char *alias, char *result, size_t resultSize) {
	if (!result || !resultSize) {
		assert(qfalse);
		return qfalse;
	}

	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, "SELECT filename FROM mapaliases WHERE alias = ? AND islive IS NOT NULL;", -1, &statement, 0);
	sqlite3_bind_text(statement, 1, alias, -1, SQLITE_STATIC);
	int rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const char *value = (const char *)sqlite3_column_text(statement, 0);
		Q_strncpyz(result, value, resultSize);
		trap_sqlite3_finalize(statement);
		return qtrue;
	}

	trap_sqlite3_finalize(statement);
	return qfalse;
}

qboolean G_DBGetAliasForMapName(const char *filename, char *result, size_t resultSize, qboolean *isliveOut) {
	if (!result || !resultSize || !isliveOut) {
		assert(qfalse);
		return qfalse;
	}

	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, "SELECT alias, islive FROM mapaliases WHERE filename = ?;", -1, &statement, 0);
	sqlite3_bind_text(statement, 1, filename, -1, SQLITE_STATIC);
	int rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const char *value = (const char *)sqlite3_column_text(statement, 0);
		Q_strncpyz(result, value, resultSize);
		*isliveOut = sqlite3_column_int(statement, 1);
		trap_sqlite3_finalize(statement);
		return qtrue;
	}

	trap_sqlite3_finalize(statement);
	return qfalse;
}

qboolean G_DBGetLiveMapNameForMapName(const char *filename, char *result, size_t resultSize) {
	if (!result || !resultSize) {
		assert(qfalse);
		return qfalse;
	}

	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, "WITH t AS (SELECT alias FROM mapaliases WHERE filename = ?) SELECT filename FROM mapaliases JOIN t ON t.alias = mapaliases.alias WHERE islive IS NOT NULL;", -1, &statement, 0);
	sqlite3_bind_text(statement, 1, filename, -1, SQLITE_STATIC);
	int rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const char *value = (const char *)sqlite3_column_text(statement, 0);
		Q_strncpyz(result, value, resultSize);
		trap_sqlite3_finalize(statement);
		return qtrue;
	}

	trap_sqlite3_finalize(statement);
	return qfalse;
}

extern qboolean canSpawnItemStartsolid;
extern qboolean G_CallSpawn(gentity_t *ent);
void DB_LoadAddedItems(void) {
	const char *const sqlLoadAddedItems = "SELECT itemtype, COALESCE(owner_account_id, -1) AS owner_account_id, originX, originY, originZ, suspended FROM addeditems WHERE mapname = ?;";
	ListClear(&level.addedItemsList);

	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlLoadAddedItems, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		Com_Printf("DB_LoadAddedItems: failed to prepare statement!\n");
		return;
	}

	sqlite3_bind_text(statement, 1, level.mapname, -1, SQLITE_STATIC);

	rc = trap_sqlite3_step(statement);
	int loadedItemsCount = 0;
	while (rc == SQLITE_ROW) {
		if (loadedItemsCount >= ADDEDITEM_LIMIT_PER_MAP) {
			Com_Printf("DB_LoadAddedItems: reached limit of %d; discontinuing adding items.\n", ADDEDITEM_LIMIT_PER_MAP);
			break;
		}

		changedItem_t *newItem = ListAdd(&level.addedItemsList, sizeof(changedItem_t));

		newItem->id = MAX_GENTITIES + level.addedItemsList.size - 1;

		const char *itemType = (const char *)sqlite3_column_text(statement, 0);
		Q_strncpyz(newItem->itemType, itemType, sizeof(newItem->itemType));
		gentity_t *itemEnt = G_Spawn();
		itemEnt->classname = newItem->itemType;

		newItem->addedItemCreatorId = sqlite3_column_int(statement, 1);

		vec3_t origin;
		origin[0] = sqlite3_column_double(statement, 2);
		origin[1] = sqlite3_column_double(statement, 3);
		origin[2] = sqlite3_column_double(statement, 4);
		VectorCopy(origin, itemEnt->s.origin);
		VectorCopy(origin, itemEnt->s.pos.trBase);
		VectorCopy(origin, itemEnt->r.currentOrigin);

		newItem->isSuspended = sqlite3_column_int(statement, 5);

		newItem->addedItemSaved = qtrue;

		if (!G_CallSpawn(itemEnt)) {
			G_FreeEntity(itemEnt);
			Com_Printf("DB_LoadAddedItems: unable to spawn %s (%f, %f, %f)!\n", itemType, origin[0], origin[1], origin[2]);
			ListRemove(&level.addedItemsList, newItem);
			continue;
		}

		if (newItem->isSuspended)
			itemEnt->spawnflags |= 1; // ITMSF_SUSPEND from g_items.c; allow it to be in the air

		canSpawnItemStartsolid = qtrue;
		FinishSpawningItem(itemEnt);
		canSpawnItemStartsolid = qfalse;

		newItem->ent = itemEnt;

		++loadedItemsCount;

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);

	if (loadedItemsCount > 0) {
		Com_Printf("DB_LoadAddedItems: loaded %d added items from the database.\n", loadedItemsCount);
		SaveRegisteredItems(); // prevent bugged out clientside models e.g. if you put a conc on a non-conc map
	}
	else {
		Com_Printf("DB_LoadAddedItems: no added items.\n");
	}
}


qboolean DB_SaveAddedItem(changedItem_t *item) {
	if (!item || !VALIDSTRING(item->itemType) || !level.mapname[0] || !item->ent) {
		assert(qfalse);
		return qfalse;
	}

	const char *sql = "INSERT OR REPLACE INTO addeditems (mapname, itemtype, owner_account_id, originX, originY, originZ, suspended) VALUES (?, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt *stmt;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sql, -1, &stmt, NULL);

	if (rc != SQLITE_OK) {
		Com_Printf("DB_SaveAddedItem: Failed to prepare statement\n");
		return qfalse;
	}

	sqlite3_bind_text(stmt, 1, level.mapname, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, item->itemType, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 3, item->addedItemCreatorId);
	sqlite3_bind_double(stmt, 4, item->ent->r.currentOrigin[0]);
	sqlite3_bind_double(stmt, 5, item->ent->r.currentOrigin[1]);
	sqlite3_bind_double(stmt, 6, item->ent->r.currentOrigin[2]);
	sqlite3_bind_int(stmt, 7, item->isSuspended);

	rc = trap_sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		Com_Printf("DB_SaveAddedItem: Failed to execute statement\n");
		trap_sqlite3_finalize(stmt);
		return qfalse;
	}

	trap_sqlite3_finalize(stmt);
	return qtrue;
}

qboolean DB_DeleteAddedItem(changedItem_t *item) {
	if (!item) {
		assert(qfalse);
		return qfalse;
	}

	const char *sqlDelete =
		"DELETE FROM addeditems "
		"WHERE rowid = ("
		"SELECT rowid FROM addeditems "
		"WHERE itemtype = ?1 "
		"ORDER BY "
		"((originX - ?2) * (originX - ?2)) + "
		"((originY - ?3) * (originY - ?3)) + "
		"((originZ - ?4) * (originZ - ?4)) "
		"LIMIT 1"
		");";

	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlDelete, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		Com_Printf("DB_DeleteAddedItem: Failed to prepare delete statement\n");
		return qfalse;
	}

	sqlite3_bind_text(statement, 1, item->ent->classname, -1, SQLITE_STATIC);

	sqlite3_bind_double(statement, 2, item->ent->r.currentOrigin[0]);
	sqlite3_bind_double(statement, 3, item->ent->r.currentOrigin[1]);
	sqlite3_bind_double(statement, 4, item->ent->r.currentOrigin[2]);

	rc = trap_sqlite3_step(statement);
	trap_sqlite3_finalize(statement);

	if (rc != SQLITE_DONE) {
		Com_Printf("DB_DeleteAddedItem: Failed to execute delete statement\n");
		return qfalse;
	}

	return qtrue;
}

qboolean DB_BaseItemIsDeleted(changedItem_t *item, int *deleterAccountIdOut) {
	if (!item || !deleterAccountIdOut) {
		assert(qfalse);
		return qfalse;
	}

	const char *sqlCheckDeleted =
		"SELECT owner_account_id FROM deletedbaseitems "
		"WHERE mapname = ?1 AND itemtype = ?2 AND "
		"originX BETWEEN ?3 - 1 AND ?3 + 1 AND "
		"originY BETWEEN ?4 - 1 AND ?4 + 1 AND "
		"originZ BETWEEN ?5 - 1 AND ?5 + 1 LIMIT 1;";

	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlCheckDeleted, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		Com_Printf("DB_BaseItemIsDeleted: failed to prepare check statement\n");
		return qfalse;
	}

	sqlite3_bind_text(statement, 1, level.mapname, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, item->itemType, -1, SQLITE_STATIC);
	sqlite3_bind_double(statement, 3, item->origin[0]);
	sqlite3_bind_double(statement, 4, item->origin[1]);
	sqlite3_bind_double(statement, 5, item->origin[2]);

	rc = trap_sqlite3_step(statement);

	if (rc == SQLITE_ROW) {
		if (deleterAccountIdOut)
			*deleterAccountIdOut = sqlite3_column_int(statement, 0);
		trap_sqlite3_finalize(statement);
		return qtrue;
	}

	trap_sqlite3_finalize(statement);
	return qfalse;
}


qboolean DB_DeleteBaseItem(changedItem_t *item) {
	if (!item || !VALIDSTRING(item->itemType) || !level.mapname[0]) {
		assert(qfalse);
		return qfalse;
	}

	const char *sql = "INSERT INTO deletedbaseitems (mapname, itemtype, owner_account_id, originX, originY, originZ) VALUES (?, ?, ?, ?, ?, ?);";
	sqlite3_stmt *stmt;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sql, -1, &stmt, NULL);

	if (rc != SQLITE_OK) {
		Com_Printf("DB_DeleteBaseItem: failed to prepare statement\n");
		return qfalse;
	}

	sqlite3_bind_text(stmt, 1, level.mapname, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, item->itemType, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 3, item->baseItemDeleterId);
	sqlite3_bind_double(stmt, 4, item->origin[0]);
	sqlite3_bind_double(stmt, 5, item->origin[1]);
	sqlite3_bind_double(stmt, 6, item->origin[2]);

	rc = trap_sqlite3_step(stmt);
	trap_sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		Com_Printf("DB_DeleteBaseItem: failed to execute statement\n");
		return qfalse;
	}

	return qtrue;
}

qboolean DB_UndeleteBaseItem(changedItem_t *item) {
	if (!item || !VALIDSTRING(item->itemType) || !level.mapname[0]) {
		assert(qfalse);
		return qfalse;
	}

	const char *sqlUndelete =
		"DELETE FROM deletedbaseitems "
		"WHERE mapname = ?1 AND itemtype = ?2 AND "
		"originX BETWEEN ?3 - 1 AND ?3 + 1 AND "
		"originY BETWEEN ?4 - 1 AND ?4 + 1 AND "
		"originZ BETWEEN ?5 - 1 AND ?5 + 1";

	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlUndelete, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		Com_Printf("DB_UndeleteBaseItem: failed to prepare undelete statement\n");
		return qfalse;
	}

	sqlite3_bind_text(statement, 1, level.mapname, -1, SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, item->itemType, -1, SQLITE_STATIC);
	sqlite3_bind_double(statement, 3, item->origin[0]);
	sqlite3_bind_double(statement, 4, item->origin[1]);
	sqlite3_bind_double(statement, 5, item->origin[2]);

	rc = trap_sqlite3_step(statement);
	trap_sqlite3_finalize(statement);

	if (rc != SQLITE_DONE) {
		Com_Printf("DB_UndeleteBaseItem: failed to execute undelete statement\n");
		return qfalse;
	}

	return qtrue;
}

void DB_LoadFilters(void) {
	if (!g_filterUsers.integer) {
		Com_Printf("DB_LoadFilters: g_filterUsers is disabled, so not loading filters.\n");
		return;
	}
	const char *const sqlLoadFilters = "SELECT id, filter_text, created_at, description, count, last_filtered FROM filters ORDER BY id ASC;";
	ListClear(&level.filtersList);

	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlLoadFilters, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		Com_Printf("DB_LoadFilters: failed to prepare statement!\n");
		return;
	}

	int loadedFiltersCount = 0;
	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		filter_t *newFilter = ListAdd(&level.filtersList, sizeof(filter_t));

		newFilter->id = sqlite3_column_int(statement, 0);
		const char *filterText = (const char *)sqlite3_column_text(statement, 1);
		if (VALIDSTRING(filterText)) {
			Q_strncpyz(newFilter->filterText, filterText, sizeof(newFilter->filterText));
		}
		else {
			Com_Printf("DB_LoadFilters: empty filter text!\n");
			ListRemove(&level.filtersList, newFilter);
			continue;
		}

		newFilter->createdAt = sqlite3_column_int64(statement, 2);
		const char *description = (const char *)sqlite3_column_text(statement, 3);
		if (VALIDSTRING(description))
			Q_strncpyz(newFilter->description, description, sizeof(newFilter->description));
		newFilter->count = sqlite3_column_int(statement, 4);
		newFilter->lastFiltered = sqlite3_column_int64(statement, 5);


		++loadedFiltersCount;

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);

	if (loadedFiltersCount > 0)
		Com_Printf("DB_LoadFilters: loaded %d filters from the database.\n", loadedFiltersCount);
	else
		Com_Printf("DB_LoadFilters: no loaded filters.\n");
}

qboolean UpdateFilter(filter_t *filter) {
	if (!filter || filter->id <= 0) {
		assert(qfalse);
		return qfalse;
	}

	const char *sqlUpdate = "UPDATE filters SET count = ?, last_filtered = ? WHERE id = ?;";
	sqlite3_stmt *stmt;
	int rc;

	rc = trap_sqlite3_prepare_v2(dbPtr, sqlUpdate, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		Com_Printf("UpdateFilter: Failed to prepare update statement\n");
		return qfalse;
	}

	sqlite3_bind_int(stmt, 1, filter->count);
	sqlite3_bind_int64(stmt, 2, filter->lastFiltered);
	sqlite3_bind_int(stmt, 3, filter->id);

	rc = trap_sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		Com_Printf("UpdateFilter: Failed to execute update statement\n");
		trap_sqlite3_finalize(stmt);
		return qfalse;
	}

	trap_sqlite3_finalize(stmt);

	return qtrue;
}

qboolean SaveNewFilter(filter_t *filter, int *newFilterId) {
	if (!filter || !VALIDSTRING(filter->filterText)) {
		assert(qfalse);
		return qfalse;
	}

	const char *sqlInsert = "INSERT INTO filters (filter_text, description, created_at) VALUES (?, ?, ?);";
	sqlite3_stmt *stmt;
	int rc;

	rc = trap_sqlite3_prepare_v2(dbPtr, sqlInsert, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		Com_Printf("SaveNewFilter: Failed to prepare insert statement\n");
		return qfalse;
	}

	sqlite3_bind_text(stmt, 1, filter->filterText, -1, SQLITE_STATIC);
	if (VALIDSTRING(filter->description)) {
		sqlite3_bind_text(stmt, 2, filter->description, -1, SQLITE_STATIC);
	}
	else {
		sqlite3_bind_null(stmt, 2);
	}
	sqlite3_bind_int64(stmt, 3, filter->createdAt);

	rc = trap_sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		Com_Printf("SaveNewFilter: Failed to execute insert statement\n");
		trap_sqlite3_finalize(stmt);
		return qfalse;
	}

	// get the ID of the newly created filter (yes, there is a C func for this, no i'm not updating the engine to add a trap call for it)
	if (newFilterId) {
		trap_sqlite3_reset(stmt);
		const char *sqlSelectId = "SELECT id FROM filters WHERE rowid = last_insert_rowid();";
		rc = trap_sqlite3_prepare_v2(dbPtr, sqlSelectId, -1, &stmt, NULL);
		if (rc != SQLITE_OK) {
			Com_Printf("SaveNewFilter: Failed to prepare select ID statement\n");
			return qfalse;
		}

		rc = trap_sqlite3_step(stmt);
		if (rc == SQLITE_ROW) {
			*newFilterId = sqlite3_column_int(stmt, 0);
		}
		else {
			Com_Printf("SaveNewFilter: Failed to fetch new filter ID\n");
			trap_sqlite3_finalize(stmt);
			return qfalse;
		}
	}
	
	trap_sqlite3_finalize(stmt);

	return qtrue;
}


// note: does not delete from list
qboolean DeleteFilter(int filterId) {
	if (filterId <= 0) {
		assert(qfalse);
		return qfalse;
	}

	const char *sqlDelete = "DELETE FROM filters WHERE id = ?;";
	sqlite3_stmt *statement;
	int rc = trap_sqlite3_prepare_v2(dbPtr, sqlDelete, -1, &statement, NULL);
	if (rc != SQLITE_OK) {
		Com_Printf("DeleteFilter: Failed to prepare delete statement\n");
		return qfalse;
	}

	sqlite3_bind_int(statement, 1, filterId);

	rc = trap_sqlite3_step(statement);
	trap_sqlite3_finalize(statement);

	if (rc != SQLITE_DONE) {
		Com_Printf("DeleteFilter: Failed to execute delete statement\n");
		return qfalse;
	}

	return qtrue;
}

const char *const sqlGetPlayerRatingHistory =
"SELECT rating, IFNULL(datetime, 0) "
"FROM playerratingshistory "
"WHERE ratee_account_id = ?1 AND pos = ?2 AND rater_account_id = ?3 "
"ORDER BY datetime IS NULL DESC, datetime ASC;";

// warning: you must ListClear and free the result if non-null!
list_t *G_DBGetPlayerRatingHistory(int rateeAccountId, ctfPosition_t pos, int raterAccountId) {
	sqlite3_stmt *statement;
	trap_sqlite3_prepare_v2(dbPtr, sqlGetPlayerRatingHistory, -1, &statement, 0);
	sqlite3_bind_int(statement, 1, rateeAccountId);
	sqlite3_bind_int(statement, 2, pos);
	sqlite3_bind_int(statement, 3, raterAccountId);

	list_t *historyList = malloc(sizeof(list_t));
	memset(historyList, 0, sizeof(list_t));

	int rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		playerRatingHistoryEntry_t *entry = (playerRatingHistoryEntry_t *)ListAdd(historyList, sizeof(playerRatingHistoryEntry_t));
		entry->rating = sqlite3_column_int(statement, 0);
		entry->datetime = sqlite3_column_double(statement, 1);

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);

	if (historyList->size == 0) {
		free(historyList);
		return NULL;
	}

	return historyList;
}

static const char *const sqlGetWinrateBetweenDates =
"WITH FilteredPugs AS ("
"    SELECT match_id "
"    FROM pugs "
"    WHERE CAST(datetime AS REAL) >= ?1 "
"      AND CAST(datetime AS REAL) < ?2 "
"      AND red_score != blue_score"
"), "
"RelevantRecords AS ("
"    SELECT ptp.match_id, ptp.session_id, ptp.team, ptp.pos, ptp.duration "
"    FROM playerpugteampos ptp "
"    INNER JOIN sessions s ON ptp.session_id = s.session_id "
"    JOIN FilteredPugs fp ON ptp.match_id = fp.match_id "
"    WHERE s.account_id = ?3"
"), "
"MaxDurationRecords AS ("
"    SELECT match_id, session_id, team, pos, MAX(duration) AS max_duration "
"    FROM RelevantRecords "
"    GROUP BY match_id"
"), "
"FilteredByPosition AS ("
"    SELECT * "
"    FROM MaxDurationRecords "
"    WHERE pos = ?4"
"), "
"WinningMatches AS ("
"    SELECT COUNT(*) AS wins "
"    FROM FilteredByPosition fbp "
"    JOIN pugs p ON fbp.match_id = p.match_id "
"    WHERE fbp.team = p.win_team"
"), "
"TotalMatches AS ("
"    SELECT COUNT(*) AS total "
"    FROM FilteredByPosition"
") "
"SELECT "
"    IFNULL((SELECT total FROM TotalMatches), 0) AS totalMatches, "
"    IFNULL((SELECT wins FROM WinningMatches), 0) AS totalWins;";

qboolean G_DBGetWinrateBetweenDates(double startTime,
	double endTime,
	int accountId,
	ctfPosition_t pos,
	int *pugsOut,
	int *winsOut)
{
	if (!pugsOut || !winsOut) {
		// Defensive check in case someone passed in NULL
		return qfalse;
	}

	sqlite3_stmt *statement = NULL;

	// Prepare the query above
	trap_sqlite3_prepare_v2(dbPtr, sqlGetWinrateBetweenDates, -1, &statement, NULL);

	// Bind the parameters:
	//   ?1 = startTime
	//   ?2 = endTime
	//   ?3 = accountId
	//   ?4 = pos
	sqlite3_bind_double(statement, 1, startTime);
	sqlite3_bind_double(statement, 2, endTime);
	sqlite3_bind_int(statement, 3, accountId);
	sqlite3_bind_int(statement, 4, pos);

	int rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		// Column 0 is total matches, Column 1 is total wins
		*pugsOut = sqlite3_column_int(statement, 0);
		*winsOut = sqlite3_column_int(statement, 1);

		trap_sqlite3_finalize(statement);
		return qtrue; // We got valid data
	}

	// If we didn't get a row, or some error
	trap_sqlite3_finalize(statement);
	*pugsOut = 0;
	*winsOut = 0;
	return qfalse;
}
