#include "g_database_log.h"
#include "sqlite3.h"
#include "time.h"

static sqlite3* db = 0;

const char* const logDbFileName = "jka_log.db";

const char* const sqlCreateLogDb =
"CREATE TABLE sessions (                                                        "
"    [session_id] INTEGER PRIMARY KEY AUTOINCREMENT,                            "
"    [session_start] DATETIME,                                                  "
"    [session_end] DATETIME,                                                    "
"    [ip_int] INTEGER,                                                          "
"    [ip_port] INTEGER,                                                         "
"    [client_id] INTEGER);                                                      "
"                                                                               "
"                                                                               "
"CREATE TABLE[hack_attempts](                                                   "
"    [session_id] INTEGER REFERENCES[sessions]( [session_id] ),                 "
"    [ip_text] TEXT,                                                            "
"    [description] TEXT );                                                      "
"                                                                               "
"                                                                               "
"CREATE TABLE [level_events] (                                                  "
"    [level_event_id] INTEGER PRIMARY KEY AUTOINCREMENT,                        "
"    [level_id] INTEGER REFERENCES [levels]([level_id]),                        "
"    [event_level_time] INTEGER,                                                "
"    [event_id] INTEGER,                                                        "
"    [event_context_i1] INTEGER,                                                "
"    [event_context_i2] INTEGER,                                                "
"    [event_context_i3] INTEGER,                                                "
"    [event_context_i4] INTEGER,                                                "
"    [event_context] TEXT);                                                     "
"                                                                               "
"                                                                               "
"CREATE TABLE [levels] (                                                        "
"  [level_id] INTEGER PRIMARY KEY AUTOINCREMENT,                                "
"  [level_start] DATETIME,                                                      "
"  [level_end] DATETIME,                                                        "
"  [mapname] TEXT,                                                              "
"  [restart] BOOL);                                                             "
"                                                                               "
"                                                                               "
"CREATE TABLE nicknames(                                                        "
"  [ip_int] INTEGER,                                                            "
"  [name] TEXT,                                                                 "
"  [duration] INTEGER,                                                          "
"  [cuid_hash2] TEXT );                                                         ";
                                                                               

const char* const sqlLogLevelStart =
"INSERT INTO levels (level_start, mapname, restart) "
"VALUES (datetime('now'),?, ?)                      ";
         
const char* const sqlLogLevelEnd =
"UPDATE levels                      "
"SET level_end = datetime( 'now' )  "
"WHERE level_id = ? ;               ";       

const char* const sqllogSessionStart =
"INSERT INTO sessions (session_start, ip_int, ip_port, client_id)    "
"VALUES (datetime('now'),?,?, ?)                                     ";

const char* const sqllogSessionEnd =
"UPDATE sessions                      "
"SET session_end = datetime( 'now' )  "
"WHERE session_id = ? ;               ";

const char* const sqlAddLevelEvent =
"INSERT INTO level_events (level_id, event_level_time, event_id,           "
"event_context_i1, event_context_i2, event_context_i3, event_context_i4,   "
"event_context)                                                            "
"VALUES (?,?,?,?,?,?,?,?)                                                  ";

const char* const sqlAddNameNM =
"INSERT INTO nicknames (ip_int, name, duration, cuid_hash2)     "
"VALUES (?,?,?,?)                                               ";

const char* const sqlAddName =
"INSERT INTO nicknames (ip_int, name, duration)                 "
"VALUES (?,?,?)                                                 ";

const char* const sqlGetAliases =
"SELECT name, SUM( duration ) AS time                           "
"FROM nicknames                                                 "
"WHERE nicknames.ip_int & ?2 = ?1 & ?2                          "
"GROUP BY name                                                  "
"ORDER BY time DESC                                             "
"LIMIT ?3                                                       ";

const char* const sqlCountAliases =
"SELECT COUNT(*) FROM (                                         "
"SELECT name, SUM( duration ) AS time                           "
"FROM nicknames                                                 "
"WHERE nicknames.ip_int & ?2 = ?1 & ?2                          "
"GROUP BY name                                                  "
"ORDER BY time DESC                                             "
"LIMIT ?3                                                       "
")                                                              ";

const char* const sqlGetNMAliases =
"SELECT name, SUM( duration ) AS time                           "
"FROM nicknames                                                 "
"WHERE nicknames.cuid_hash2 = ?1                                "
"GROUP BY name                                                  "
"ORDER BY time DESC                                             "
"LIMIT ?2                                                       ";

const char* const sqlCountNMAliases =
"SELECT COUNT(*) FROM ("
"SELECT name, SUM( duration ) AS time                           "
"FROM nicknames                                                 "
"WHERE nicknames.cuid_hash2 = ?1                                "
"GROUP BY name                                                  "
"ORDER BY time DESC                                             "
"LIMIT ?2                                                       "
")                                                              ";

const char* const sqlTestCuidSupport =
"PRAGMA table_info(nicknames)                                   ";

const char* const sqlUpgradeToCuid2FromNoCuid =
"ALTER TABLE nicknames ADD cuid_hash2 TEXT                      ";

const char* const sqlUpgradeToCuid2FromCuid1 =
"BEGIN TRANSACTION;                                                                                      "
"CREATE TABLE nicknames_temp([ip_int] INTEGER, [name] TEXT, [duration] INTEGER);                         "
"INSERT INTO nicknames_temp (ip_int, name, duration) SELECT ip_int, name, duration FROM nicknames;       "
"DROP TABLE nicknames;                                                                                   "
"CREATE TABLE nicknames([ip_int] INTEGER, [name] TEXT, [duration] INTEGER, [cuid_hash2] TEXT);           "
"INSERT INTO nicknames (ip_int, name, duration) SELECT ip_int, name, duration FROM nicknames_temp;       "
"DROP TABLE nicknames_temp;                                                                              "
"COMMIT;                                                                                                 ";

const char* const sqlCreateFastcapsTable =
"CREATE TABLE IF NOT EXISTS fastcaps (                                          "
"    [fastcap_id] INTEGER PRIMARY KEY AUTOINCREMENT,                            "
"    [mapname] TEXT,                                                            "
"    [type] INTEGER,                                                            "
"    [player_name] TEXT,                                                        "
"    [player_ip_int] INTEGER,                                                   "
"    [player_cuid_hash2] TEXT,                                                  "
"    [match_id] TEXT,                                                           "
"    [client_id] INTEGER,                                                       "
"    [capture_time] INTEGER,                                                    "
"    [whose_flag] INTEGER,                                                      "
"    [pickup_time] INTEGER);                                                    ";

const char* const sqlAddFastcap =
"INSERT INTO fastcaps (mapname, type, player_name,           "
"player_ip_int, player_cuid_hash2, match_id, client_id,      "
"capture_time, whose_flag, pickup_time)                      "
"VALUES (?,?,?,?,?,?,?,?,?,?)                                ";

const char* const sqlremoveFastcaps =
"DELETE FROM fastcaps       "
"WHERE mapname = ?          ";

const char* const sqlGetFastcaps =
"SELECT player_name, player_ip_int, player_cuid_hash2,                 "
"match_id, client_id, capture_time, whose_flag, pickup_time            "
"FROM fastcaps                                                         "
"WHERE fastcaps.mapname = ?1 AND fastcaps.type = ?2                    "
"ORDER BY capture_time                                                 "
"LIMIT ?3                                                              ";

//
//  G_LogDbLoad
// 
//  Loads the database from disk, including creating of not exists
//  or if it is corrupted
//
void G_LogDbLoad()
{    
    int rc = -1;    

    rc = sqlite3_initialize();
    rc = sqlite3_open_v2( logDbFileName, &db, SQLITE_OPEN_READWRITE, 0 );

	if (rc == SQLITE_OK) {
		G_LogPrintf("Successfully loaded log database %s\n", logDbFileName);
		// if needed, upgrade legacy servers that don't support cuid_hash yet
		sqlite3_stmt* statement;
		rc = sqlite3_prepare(db, sqlTestCuidSupport, -1, &statement, 0);
		rc = sqlite3_step(statement);
		qboolean foundCuid2 = qfalse, foundCuid1 = qfalse;
		while (rc == SQLITE_ROW) {
			const char *name = (const char*)sqlite3_column_text(statement, 1);
			if (name && *name && !Q_stricmp(name, "cuid_hash2"))
				foundCuid2 = qtrue;
			if (name && *name && !Q_stricmp(name, "cuid_hash"))
				foundCuid1 = qtrue;
			rc = sqlite3_step(statement);
		}
		sqlite3_finalize(statement);
		if (foundCuid2) {
			//G_LogPrintf("Log database supports cuid 2.0, no upgrade needed.\n");
		}
		else if (foundCuid1) {
			G_LogPrintf("Automatically upgrading old log database: cuid 1.0 support ==> cuid 2.0 support.\n");
			sqlite3_exec(db, sqlUpgradeToCuid2FromCuid1, 0, 0, 0);
		}
		else {
			G_LogPrintf("Automatically upgrading old log database: no cuid support ==> cuid 2.0 support.\n");
			sqlite3_exec(db, sqlUpgradeToCuid2FromNoCuid, 0, 0, 0);
		}

		// create the database IF NEEDED, since it might have been created before the feature was added
		sqlite3_exec( db, sqlCreateFastcapsTable, 0, 0, 0 );
	}
	else {
		G_LogPrintf("Couldn't find log database %s, creating a new one\n", logDbFileName);
        // create new database
        rc = sqlite3_open_v2( logDbFileName, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0 );

        sqlite3_exec( db, sqlCreateLogDb, 0, 0, 0 );
		sqlite3_exec( db, sqlCreateFastcapsTable, 0, 0, 0 );
    }
}


//
//  G_LogDbUnload
// 
//  Unloads the database from memory, includes flushing it 
//  to the file.
//
void G_LogDbUnload()
{
    sqlite3_close( db );
}

//
//  G_LogDbLogLevelStart
// 
//  Logs current level (map) to the database
//
int G_LogDbLogLevelStart(qboolean isRestart)
{
    sqlite3_stmt* statement;

    // load current map name
    char mapname[128];
    trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );

    // prepare insert statement
    sqlite3_prepare( db, sqlLogLevelStart, -1, &statement, 0 );
    sqlite3_bind_text( statement, 1, mapname, -1, 0 );
    sqlite3_bind_int( statement, 2, isRestart ? 1 : 0 );

    sqlite3_step( statement );

    int levelId = sqlite3_last_insert_rowid( db );

    sqlite3_finalize( statement );   

    return levelId;
}

//
//  G_LogDbLogLevelEnd
// 
//  Logs current level (map) to the database
//
void G_LogDbLogLevelEnd( int levelId )
{
    sqlite3_stmt* statement;

    // prepare update statement
    sqlite3_prepare( db, sqlLogLevelEnd, -1, &statement, 0 );
    sqlite3_bind_int( statement, 1, levelId );

    sqlite3_step( statement );

    sqlite3_finalize( statement );
}

//
//  G_LogDbLogLevelEvent
// 
//  Logs level event
//
void G_LogDbLogLevelEvent( int levelId,
    int levelTime,
    int eventId,
    int context1,
    int context2,
    int context3,
    int context4,
    const char* contextText )
{
    sqlite3_stmt* statement;
    // prepare insert statement
    sqlite3_prepare( db, sqlAddLevelEvent, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, levelId );
    sqlite3_bind_int( statement, 2, levelTime );
    sqlite3_bind_int( statement, 3, eventId );
    sqlite3_bind_int( statement, 4, context1 );
    sqlite3_bind_int( statement, 5, context2 );
    sqlite3_bind_int( statement, 6, context3 );
    sqlite3_bind_int( statement, 7, context4 );
    sqlite3_bind_text( statement, 8, contextText, -1, 0 );

    sqlite3_step( statement );

    sqlite3_finalize( statement );
}
 
//
//  G_LogDbLogSessionStart
// 
//  Logs players connection session start
//
int G_LogDbLogSessionStart( unsigned int ipInt,
    int ipPort,
    int id )
{     
    sqlite3_stmt* statement;
    // prepare insert statement
    sqlite3_prepare( db, sqllogSessionStart, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipInt );
    sqlite3_bind_int( statement, 2, ipPort );
    sqlite3_bind_int( statement, 3, id );

    sqlite3_step( statement );

    int sessionId = sqlite3_last_insert_rowid( db );

    sqlite3_finalize( statement );
    
    return sessionId;
}

//
//  G_LogDbLogSessionEnd
// 
//  Logs players connection session end
//
void G_LogDbLogSessionEnd( int sessionId )
{
    sqlite3_stmt* statement;
    // prepare insert statement
    sqlite3_prepare( db, sqllogSessionEnd, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, sessionId );

    sqlite3_step( statement );

    sqlite3_finalize( statement );
}

//
//  G_LogDbLogNickname
// 
//  Logs a player's nickname (and cuid hash, if applicable)
//
void G_LogDbLogNickname( unsigned int ipInt,
    const char* name,
    int duration,
	const char* cuidHash)
{
    sqlite3_stmt* statement;

    // prepare insert statement
    sqlite3_prepare( db, VALIDSTRING(cuidHash) ? sqlAddNameNM : sqlAddName, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipInt );
    sqlite3_bind_text( statement, 2, name, -1, 0 );
    sqlite3_bind_int( statement, 3, duration );
	if (VALIDSTRING(cuidHash))
		sqlite3_bind_text(statement, 4, cuidHash, -1, 0);

    sqlite3_step( statement );

    sqlite3_finalize( statement ); 
}

void G_CfgDbListAliases( unsigned int ipInt,
    unsigned int ipMask,
    int limit,
    ListAliasesCallback callback,
    void* context,
	const char* cuidHash)
{
	sqlite3_stmt* statement;
	int rc;
	const char* name;
	int duration;
	if (VALIDSTRING(cuidHash)) { // newmod user; check for cuid matches first before falling back to checking for unique id matches
		int numNMFound = 0;
		rc = sqlite3_prepare(db, sqlCountNMAliases, -1, &statement, 0);
		sqlite3_bind_text(statement, 1, cuidHash, -1, 0);
		sqlite3_bind_int(statement, 2, limit);

		rc = sqlite3_step(statement);
		while (rc == SQLITE_ROW) {
			numNMFound = sqlite3_column_int(statement, 0);
			rc = sqlite3_step(statement);
		}
		sqlite3_reset(statement);

		if (numNMFound) { // we found some cuid matches; let's use these
			rc = sqlite3_prepare(db, sqlGetNMAliases, -1, &statement, 0);
			sqlite3_bind_text(statement, 1, cuidHash, -1, 0);
			sqlite3_bind_int(statement, 2, limit);

			rc = sqlite3_step(statement);
			while (rc == SQLITE_ROW) {
				name = (const char*)sqlite3_column_text(statement, 0);
				duration = sqlite3_column_int(statement, 1);

				callback(context, name, duration);

				rc = sqlite3_step(statement);
			}
			sqlite3_finalize(statement);
		}
		else { // didn't find any cuid matches; use the old unique id method
			rc = sqlite3_prepare(db, sqlGetAliases, -1, &statement, 0);
			sqlite3_bind_int(statement, 1, ipInt);
			sqlite3_bind_int(statement, 2, ipMask);
			sqlite3_bind_int(statement, 3, limit);

			rc = sqlite3_step(statement);
			while (rc == SQLITE_ROW) {
				name = (const char*)sqlite3_column_text(statement, 0);
				duration = sqlite3_column_int(statement, 1);

				callback(context, name, duration);

				rc = sqlite3_step(statement);
			}
			sqlite3_finalize(statement);
		}
	}
	else { // non-newmod; just use the old unique id method
		sqlite3_stmt* statement;
		// prepare insert statement
		int rc = sqlite3_prepare(db, sqlGetAliases, -1, &statement, 0);

		sqlite3_bind_int(statement, 1, ipInt);
		sqlite3_bind_int(statement, 2, ipMask);
		sqlite3_bind_int(statement, 3, limit);

		rc = sqlite3_step(statement);
		while (rc == SQLITE_ROW) {
			name = (const char*)sqlite3_column_text(statement, 0);
			duration = sqlite3_column_int(statement, 1);

			callback(context, name, duration);

			rc = sqlite3_step(statement);
		}

		sqlite3_finalize(statement);
	}
}

void G_LogDbListCaptureRecords( const char *mapname,
	CaptureRecordList *recordsToLoad )
{
	memset( recordsToLoad, 0, sizeof( *recordsToLoad ) );

	if ( g_gametype.integer != GT_CTF || !g_saveCaptureRecords.integer ) {
		return;
	}

	sqlite3_stmt* statement;
	int i, rc = -1, loaded = 0;

	rc = sqlite3_prepare( db, sqlGetFastcaps, -1, &statement, 0 );

	for ( i = 0; i < CAPTURE_RECORD_NUM_TYPES; ++i ) {
		sqlite3_reset( statement );
		sqlite3_clear_bindings( statement );

		sqlite3_bind_text( statement, 1, mapname, -1, 0 );
		sqlite3_bind_int( statement, 2, i );
		sqlite3_bind_int( statement, 3, MAX_SAVED_RECORDS );

		rc = sqlite3_step( statement );

		int j = 0;
		while ( rc == SQLITE_ROW && j < MAX_SAVED_RECORDS ) {
			// get the fields from the query
			const char *player_name = ( const char* )sqlite3_column_text( statement, 0 );
			const unsigned int player_ip_int = sqlite3_column_int( statement, 1 );
			const char *player_cuid_hash2 = ( const char* )sqlite3_column_text( statement, 2 );
			const char *match_id = ( const char* )sqlite3_column_text( statement, 3 );
			const int client_id = sqlite3_column_int( statement, 4 );
			const int capture_time = sqlite3_column_int( statement, 5 );
			const int whose_flag = sqlite3_column_int( statement, 6 );
			const int pickup_time = sqlite3_column_int( statement, 7 );

			// write them to the record
			CaptureRecord *record = &recordsToLoad->records[i][j];

			Q_strncpyz( record->recordHolderName, player_name, sizeof( record->recordHolderName ) );
			record->recordHolderIpInt = player_ip_int;

			if ( VALIDSTRING( player_cuid_hash2 ) ) {
				Q_strncpyz( record->recordHolderCuid, player_cuid_hash2, sizeof( record->recordHolderCuid ) );
			}

			if ( VALIDSTRING( match_id ) ) {
				Q_strncpyz( record->matchId, match_id, sizeof( record->matchId ) );
			}

			record->recordHolderClientId = client_id;
			record->captureTime = capture_time;
			record->whoseFlag = whose_flag;
			record->pickupLevelTime = pickup_time;

			rc = sqlite3_step( statement );
			++loaded;
			++j;
		}
	}

	sqlite3_finalize( statement );

	// write the remaining global fields
	Q_strncpyz( recordsToLoad->mapname, mapname, sizeof( recordsToLoad->mapname ) );
	recordsToLoad->enabled = qtrue;

	G_Printf( "Loaded %d capture time records from database\n", loaded );
}

void G_LogDbSaveCaptureRecords( CaptureRecordList *recordsToSave )
{
	if ( g_gametype.integer != GT_CTF || !recordsToSave->enabled || !recordsToSave->changed ) {
		return;
	}

	sqlite3_stmt* statement;

	// first, delete all of the old records for this map, even those that didn't change
	sqlite3_prepare( db, sqlremoveFastcaps, -1, &statement, 0 );
	sqlite3_bind_text( statement, 1, recordsToSave->mapname, -1, 0 );
	sqlite3_step( statement );
	sqlite3_finalize( statement );

	// rewrite everything
	sqlite3_prepare( db, sqlAddFastcap, -1, &statement, 0 );

	int i, j, saved = 0;
	for ( i = 0; i < CAPTURE_RECORD_NUM_TYPES; ++i ) {
		for ( j = 0; j < MAX_SAVED_RECORDS; ++j ) {
			CaptureRecord *record = &recordsToSave->records[i][j];

			if ( !record->captureTime ) {
				continue; // not a valid record
			}

			sqlite3_reset( statement );
			sqlite3_clear_bindings( statement );

			sqlite3_bind_text( statement, 1, recordsToSave->mapname, -1, 0 );
			sqlite3_bind_int( statement, 2, i );
			sqlite3_bind_text( statement, 3, record->recordHolderName, -1, 0 );
			sqlite3_bind_int( statement, 4, record->recordHolderIpInt );

			if ( VALIDSTRING( record->recordHolderCuid ) ) {
				sqlite3_bind_text( statement, 5, record->recordHolderCuid, -1, 0 );
			} else {
				sqlite3_bind_null( statement, 5 );
			}

			if ( VALIDSTRING( record->matchId ) ) {
				sqlite3_bind_text( statement, 6, record->matchId, -1, 0 );
			} else {
				sqlite3_bind_null( statement, 6 );
			}

			sqlite3_bind_int( statement, 7, record->recordHolderClientId );
			sqlite3_bind_int( statement, 8, record->captureTime );
			sqlite3_bind_int( statement, 9, record->whoseFlag );
			sqlite3_bind_int( statement, 10, record->pickupLevelTime);

			sqlite3_step( statement );
			++saved;
		}
	}

	sqlite3_finalize( statement );

	G_Printf( "Saved %d capture time records to database\n", saved );
}

// this function assumes the arrays in currentRecords are sorted by captureTime
// returns 0 if this is not a record, or the newly assigned rank otherwise (1 = 1st, 2 = 2nd...)
int G_LogDbCaptureTime( unsigned int ipInt,
	const char *netname,
	const char *cuid,
	const int clientId,
	const char *matchId,
	const int captureTime,
	const team_t whoseFlag,
	const int pickupLevelTime,
	const CaptureRecordType type,
	CaptureRecordList *currentRecords )
{
	if ( g_gametype.integer != GT_CTF || !currentRecords->enabled ) {
		return 0;
	}

	if ( whoseFlag != TEAM_BLUE && whoseFlag != TEAM_RED ) {
		return 0; // someone tried to capture a neutral flag...
	}

	CaptureRecord *recordArray = &currentRecords->records[type][0];
	int newIndex;

	// we don't want more than one entry per category per player, so first, check if there is already one record for this player
	for ( newIndex = 0; newIndex < MAX_SAVED_RECORDS; ++newIndex ) {
		if ( !recordArray[newIndex].captureTime ) {
			continue; // not a valid record
		}

		// don't mix the two types of lookup
		if ( VALIDSTRING( cuid ) ) {
			// we have a cuid, use that to find an existing record
			if ( VALIDSTRING( recordArray[newIndex].recordHolderCuid ) && !Q_stricmp( cuid, recordArray[newIndex].recordHolderCuid ) ) {
				break;
			}
		} else {
			// fall back to the whois accuracy...
			if ( ipInt == recordArray[newIndex].recordHolderIpInt ) {
				break;
			}
		}
	}

	if ( newIndex < MAX_SAVED_RECORDS ) {
		// we found an existing record for this player, so just use its index to overwrite it if it's better
		if ( captureTime >= recordArray[newIndex].captureTime ) {
			return 0; // our existing record is better, so don't save anything to avoid record spam
		}
	} else {
		// this player doesn't have a record in this category yet, so find an index by comparing times from the worst to the best
		for ( newIndex = MAX_SAVED_RECORDS; newIndex > 0; --newIndex ) {
			if ( recordArray[newIndex - 1].captureTime && recordArray[newIndex - 1].captureTime < captureTime ) {
				break; // this one is better, so use the index after it
			}
		}

		// if the worst time is better, the index will point past the array, so don't bother
		if ( newIndex >= MAX_SAVED_RECORDS ) {
			return 0;
		}

		// shift the array to the right unless this is already the last element
		if ( newIndex < MAX_SAVED_RECORDS - 1 ) {
			memmove( recordArray + newIndex + 1, recordArray + newIndex, ( MAX_SAVED_RECORDS - newIndex - 1 ) * sizeof( *recordArray ) );
		}
	}

	// overwrite the selected element with the new record
	CaptureRecord *newElement = &recordArray[newIndex];
	Q_strncpyz( newElement->recordHolderName, netname, sizeof( newElement->recordHolderName ) );
	newElement->recordHolderIpInt = ipInt;
	newElement->captureTime = captureTime;
	newElement->whoseFlag = whoseFlag;
	newElement->recordHolderClientId = clientId;
	newElement->pickupLevelTime = pickupLevelTime;

	// cuid is optional, empty for clients without one
	if ( VALIDSTRING( cuid ) ) {
		Q_strncpyz( newElement->recordHolderCuid, cuid, sizeof( newElement->recordHolderCuid ) );
	} else {
		newElement->recordHolderCuid[0] = '\0';
	}

	// match id is optional, empty if sv_uniqueid is not implemented in this OpenJK version
	if ( VALIDSTRING( matchId ) && strlen( matchId ) == SV_UNIQUEID_LEN - 1 ) {
		Q_strncpyz( newElement->matchId, matchId, sizeof( newElement->matchId ) );
	} else {
		newElement->matchId[0] = '\0';
	}

	// save the changes later in db
	currentRecords->changed = qtrue;

	return newIndex + 1;
}