#include "g_database.h"
#include "sqlite3.h"
#include "time.h"

static sqlite3* diskDb = NULL;
static sqlite3* dbPtr = NULL;

void InitMetadata( void );
void InitNicknames( void );
void InitFastcaps( void );
void InitWhitelist( void );
void InitBlacklist( void );
void InitPools( void );

void G_DBLoadDatabase( void )
{
    int rc;

    rc = sqlite3_initialize();

	if ( rc != SQLITE_OK ) {
		Com_Printf( "Failed to initialize SQLite3 (code: %d)\n", rc );
		return;
	}

    rc = sqlite3_open_v2( DB_FILENAME, &diskDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL );

	if ( rc != SQLITE_OK ) {
		Com_Printf( "Failed to open database file "DB_FILENAME" (code: %d)\n", rc );
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
			Com_Printf( "WARNING: Failed to load database into memory!\n" );
		}
	}

	// use disk db by default in any case
	if ( !dbPtr ) {
		Com_Printf( "Using on-disk database\n" );
		dbPtr = diskDb;
	}

	// init all modules
	InitMetadata();
	InitNicknames();
	InitFastcaps();
	InitWhitelist();
	InitBlacklist();
	InitPools();

	// get version and call the upgrade routine

	char schema_version[16];
	G_DBGetMetadata( "schema_version", schema_version, sizeof( schema_version ) );

	int version = VALIDSTRING( schema_version ) ? atoi( schema_version ) : 0;
	G_DBUpgradeDatabaseSchema( version, dbPtr );

	G_DBSetMetadata( "schema_version", DB_SCHEMA_VERSION_STR );
}

void G_DBUnloadDatabase( void )
{
	int rc;

	if ( dbPtr != diskDb ) {
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

	sqlite3_close( diskDb );
	diskDb = dbPtr = NULL;
}

// =========== METADATA ========================================================

const char* const sqlCreateMetadataTable =
"CREATE TABLE IF NOT EXISTS [metadata] (                                       "
"  [key] TEXT COLLATE NOCASE PRIMARY KEY NOT NULL,                             "
"  [value] TEXT DEFAULT NULL                                                   "
");                                                                            ";

const char* const sqlGetMetadata =
"SELECT value FROM metadata WHERE metadata.key = ?1 LIMIT 1;                   ";

const char* const sqlSetMetadata =
"INSERT OR REPLACE INTO metadata (key, value) VALUES( ?1, ?2 );                ";

void InitMetadata( void )
{
	sqlite3_exec( dbPtr, sqlCreateMetadataTable, NULL, NULL, NULL );
}

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

// =========== NICKNAMES =======================================================

const char* const sqlCreateNicknamesTable =
"CREATE TABLE IF NOT EXISTS nicknames(                                          "
"  [ip_int] INTEGER,                                                            "
"  [name] TEXT,                                                                 "
"  [duration] INTEGER,                                                          "
"  [cuid_hash2] TEXT );                                                         ";

const char* const sqlAddName =
"INSERT INTO nicknames (ip_int, name, duration)                 "
"VALUES (?,?,?)                                                 ";

const char* const sqlAddNameNM =
"INSERT INTO nicknames (ip_int, name, duration, cuid_hash2)     "
"VALUES (?,?,?,?)                                               ";

const char* const sqlGetAliases =
"SELECT name, SUM( duration ) AS time                           "
"FROM nicknames                                                 "
"WHERE nicknames.ip_int & ?2 = ?1 & ?2                          "
"GROUP BY name                                                  "
"ORDER BY time DESC                                             "
"LIMIT ?3                                                       ";

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

void InitNicknames( void )
{
	sqlite3_exec( dbPtr, sqlCreateNicknamesTable, 0, 0, 0 );
}

void G_DBLogNickname( unsigned int ipInt,
	const char* name,
	int duration,
	const char* cuidHash )
{
	sqlite3_stmt* statement;

	// prepare insert statement
	sqlite3_prepare( dbPtr, VALIDSTRING( cuidHash ) ? sqlAddNameNM : sqlAddName, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, ipInt );
	sqlite3_bind_text( statement, 2, name, -1, 0 );
	sqlite3_bind_int( statement, 3, duration );
	if ( VALIDSTRING( cuidHash ) )
		sqlite3_bind_text( statement, 4, cuidHash, -1, 0 );

	sqlite3_step( statement );

	sqlite3_finalize( statement );
}

void G_DBListAliases( unsigned int ipInt,
	unsigned int ipMask,
	int limit,
	ListAliasesCallback callback,
	void* context,
	const char* cuidHash )
{
	sqlite3_stmt* statement;
	int rc;
	const char* name;
	int duration;
	if ( VALIDSTRING( cuidHash ) ) { // newmod user; check for cuid matches first before falling back to checking for unique id matches
		int numNMFound = 0;
		rc = sqlite3_prepare( dbPtr, sqlCountNMAliases, -1, &statement, 0 );
		sqlite3_bind_text( statement, 1, cuidHash, -1, 0 );
		sqlite3_bind_int( statement, 2, limit );

		rc = sqlite3_step( statement );
		while ( rc == SQLITE_ROW ) {
			numNMFound = sqlite3_column_int( statement, 0 );
			rc = sqlite3_step( statement );
		}
		sqlite3_reset( statement );

		if ( numNMFound ) { // we found some cuid matches; let's use these
			rc = sqlite3_prepare( dbPtr, sqlGetNMAliases, -1, &statement, 0 );
			sqlite3_bind_text( statement, 1, cuidHash, -1, 0 );
			sqlite3_bind_int( statement, 2, limit );

			rc = sqlite3_step( statement );
			while ( rc == SQLITE_ROW ) {
				name = ( const char* )sqlite3_column_text( statement, 0 );
				duration = sqlite3_column_int( statement, 1 );

				callback( context, name, duration );

				rc = sqlite3_step( statement );
			}
			sqlite3_finalize( statement );
		}
		else { // didn't find any cuid matches; use the old unique id method
			rc = sqlite3_prepare( dbPtr, sqlGetAliases, -1, &statement, 0 );
			sqlite3_bind_int( statement, 1, ipInt );
			sqlite3_bind_int( statement, 2, ipMask );
			sqlite3_bind_int( statement, 3, limit );

			rc = sqlite3_step( statement );
			while ( rc == SQLITE_ROW ) {
				name = ( const char* )sqlite3_column_text( statement, 0 );
				duration = sqlite3_column_int( statement, 1 );

				callback( context, name, duration );

				rc = sqlite3_step( statement );
			}
			sqlite3_finalize( statement );
		}
	}
	else { // non-newmod; just use the old unique id method
		sqlite3_stmt* statement;
		// prepare insert statement
		int rc = sqlite3_prepare( dbPtr, sqlGetAliases, -1, &statement, 0 );

		sqlite3_bind_int( statement, 1, ipInt );
		sqlite3_bind_int( statement, 2, ipMask );
		sqlite3_bind_int( statement, 3, limit );

		rc = sqlite3_step( statement );
		while ( rc == SQLITE_ROW ) {
			name = ( const char* )sqlite3_column_text( statement, 0 );
			duration = sqlite3_column_int( statement, 1 );

			callback( context, name, duration );

			rc = sqlite3_step( statement );
		}

		sqlite3_finalize( statement );
	}
}

// =========== FASTCAPS ========================================================

const char* const sqlCreateFastcapsV2Table =
"CREATE TABLE IF NOT EXISTS fastcapsV2 (                                        "
"    [fastcap_id] INTEGER PRIMARY KEY AUTOINCREMENT,                            "
"    [mapname] TEXT,                                                            "
"    [rank] INTEGER,                                                            "
"    [type] INTEGER,                                                            "
"    [player_name] TEXT,                                                        "
"    [player_ip_int] INTEGER,                                                   "
"    [player_cuid_hash2] TEXT,                                                  "
"    [capture_time] INTEGER,                                                    "
"    [whose_flag] INTEGER,                                                      "
"    [max_speed] INTEGER,                                                       "
"    [avg_speed] INTEGER,                                                       "
"    [date] INTEGER,                                                            "
"    [match_id] TEXT,                                                           "
"    [client_id] INTEGER,                                                       "
"    [pickup_time] INTEGER);                                                    ";

const char* const sqlAddFastcapV2 =
"INSERT INTO fastcapsV2 (                                                       "
"    mapname, rank, type, player_name, player_ip_int, player_cuid_hash2,        "
"    capture_time, whose_flag, max_speed, avg_speed, date, match_id,            "
"    client_id, pickup_time)                                                    "
"VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)                                           ";

const char* const sqlremoveFastcapsV2 =
"DELETE FROM fastcapsV2 WHERE mapname = ?                                       ";

const char* const sqlGetFastcapsV2 =
"SELECT player_name, player_ip_int, player_cuid_hash2, capture_time,            "
"whose_flag, max_speed, avg_speed, date, match_id, client_id, pickup_time       "
"FROM fastcapsV2                                                                "
"WHERE fastcapsV2.mapname = ?1 AND fastcapsV2.type = ?2                         "
"ORDER BY capture_time                                                          "
"LIMIT ?3                                                                       ";

const char* const sqlListBestFastcapsV2 =
"SELECT mapname, player_name, player_ip_int, player_cuid_hash2,                 "
"capture_time AS best_time, date                                                "
"FROM fastcapsV2                                                                "
"WHERE fastcapsV2.type = ?1 AND fastcapsV2.rank = 1                             "
"GROUP BY mapname                                                               "
"ORDER BY mapname ASC                                                           "
"LIMIT ?2                                                                       "
"OFFSET ?3                                                                      ";

const char* const sqlGetFastcapsV2Leaderboard =
"SELECT player_ip_int,                                                          "
"    SUM( CASE WHEN rank = 1 THEN 1 ELSE 0 END ) AS golds,                      "
"    SUM( CASE WHEN rank = 2 THEN 1 ELSE 0 END ) AS silvers,                    "
"    SUM( CASE WHEN rank = 3 THEN 1 ELSE 0 END ) AS bronzes                     "
"FROM fastcapsV2                                                                "
"WHERE fastcapsV2.type = ?1                                                     "
"GROUP BY player_ip_int                                                         "
"HAVING golds > 0 OR silvers > 0 OR bronzes > 0                                 "
"ORDER BY golds DESC, silvers DESC, bronzes DESC                                "
"LIMIT ?2                                                                       "
"OFFSET ?3                                                                      ";

const char* const sqlListLatestFastcapsV2 =
"SELECT mapname, rank, player_name, player_ip_int, player_cuid_hash2,           "
"capture_time, date                                                             "
"FROM fastcapsV2                                                                "
"WHERE fastcapsV2.type = ?1                                                     "
"ORDER BY date DESC                                                             "
"LIMIT ?2                                                                       "
"OFFSET ?3                                                                      ";

void InitFastcaps( void )
{
	sqlite3_exec( dbPtr, sqlCreateFastcapsV2Table, 0, 0, 0 );
}

// returns how many records were loaded
int G_DBLoadCaptureRecords( const char *mapname,
	CaptureRecordList *recordsToLoad )
{
	memset( recordsToLoad, 0, sizeof( *recordsToLoad ) );

	// make sure we always make a lower case map lookup
	Q_strncpyz( recordsToLoad->mapname, mapname, sizeof( recordsToLoad->mapname ) );
	Q_strlwr( recordsToLoad->mapname );

	sqlite3_stmt* statement;
	int i, rc = -1, loaded = 0;

	rc = sqlite3_prepare( dbPtr, sqlGetFastcapsV2, -1, &statement, 0 );

	for ( i = 0; i < CAPTURE_RECORD_NUM_TYPES; ++i ) {
		sqlite3_reset( statement );
		sqlite3_clear_bindings( statement );

		sqlite3_bind_text( statement, 1, recordsToLoad->mapname, -1, 0 );
		sqlite3_bind_int( statement, 2, i );
		sqlite3_bind_int( statement, 3, MAX_SAVED_RECORDS );

		rc = sqlite3_step( statement );

		int j = 0;
		while ( rc == SQLITE_ROW && j < MAX_SAVED_RECORDS ) {
			// get the fields from the query
			const char *player_name = ( const char* )sqlite3_column_text( statement, 0 );
			const unsigned int player_ip_int = sqlite3_column_int( statement, 1 );
			const char *player_cuid_hash2 = ( const char* )sqlite3_column_text( statement, 2 );
			const int capture_time = sqlite3_column_int( statement, 3 );
			const int whose_flag = sqlite3_column_int( statement, 4 );
			const int max_speed = sqlite3_column_int( statement, 5 );
			const int avg_speed = sqlite3_column_int( statement, 6 );
			const time_t date = sqlite3_column_int64( statement, 7 );
			const char *match_id = ( const char* )sqlite3_column_text( statement, 8 );
			const int client_id = sqlite3_column_int( statement, 9 );
			const int pickup_time = sqlite3_column_int( statement, 10 );

			// write them to the record list
			CaptureRecord *record = &recordsToLoad->records[i][j];

			Q_strncpyz( record->recordHolderName, player_name, sizeof( record->recordHolderName ) );
			record->recordHolderIpInt = player_ip_int;

			if ( VALIDSTRING( player_cuid_hash2 ) ) {
				Q_strncpyz( record->recordHolderCuid, player_cuid_hash2, sizeof( record->recordHolderCuid ) );
			}

			record->captureTime = capture_time;
			record->whoseFlag = whose_flag;
			record->maxSpeed = max_speed;
			record->avgSpeed = avg_speed;
			record->date = date;

			if ( VALIDSTRING( match_id ) ) {
				Q_strncpyz( record->matchId, match_id, sizeof( record->matchId ) );
			}

			record->recordHolderClientId = client_id;
			record->pickupLevelTime = pickup_time;

			rc = sqlite3_step( statement );
			++loaded;
			++j;
		}
	}

	sqlite3_finalize( statement );

	// write the remaining global fields
	recordsToLoad->enabled = qtrue;

	return loaded;
}

void G_DBListBestCaptureRecords( CaptureRecordType type,
	int limit,
	int offset,
	ListBestCapturesCallback callback,
	void *context )
{
	sqlite3_stmt* statement;
	int rc = sqlite3_prepare( dbPtr, sqlListBestFastcapsV2, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, type );
	sqlite3_bind_int( statement, 2, limit );
	sqlite3_bind_int( statement, 3, offset );

	rc = sqlite3_step( statement );
	while ( rc == SQLITE_ROW ) {
		const char *mapname = ( const char* )sqlite3_column_text( statement, 0 );
		const char *player_name = ( const char* )sqlite3_column_text( statement, 1 );
		const unsigned int player_ip_int = sqlite3_column_int( statement, 2 );
		const char *player_cuid_hash2 = ( const char* )sqlite3_column_text( statement, 3 );
		const int best_time = sqlite3_column_int( statement, 4 );
		const time_t date = sqlite3_column_int64( statement, 5 );

		callback( context, mapname, type, player_name, player_ip_int, player_cuid_hash2, best_time, date );

		rc = sqlite3_step( statement );
	}

	sqlite3_finalize( statement );
}

void G_DBGetCaptureRecordsLeaderboard( CaptureRecordType type,
	int limit,
	int offset,
	LeaderboardCapturesCallback callback,
	void *context )
{
	sqlite3_stmt* statement;
	int rc = sqlite3_prepare( dbPtr, sqlGetFastcapsV2Leaderboard, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, type );
	sqlite3_bind_int( statement, 2, limit );
	sqlite3_bind_int( statement, 3, offset );

	rc = sqlite3_step( statement );
	while ( rc == SQLITE_ROW ) {
		const unsigned int player_ip_int = sqlite3_column_int( statement, 0 );
		const int golds = sqlite3_column_int( statement, 1 );
		const int silvers = sqlite3_column_int( statement, 2 );
		const int bronzes = sqlite3_column_int( statement, 3 );

		callback( context, type, player_ip_int, golds, silvers, bronzes );

		rc = sqlite3_step( statement );
	}

	sqlite3_finalize( statement );
}

void G_DBListLatestCaptureRecords( CaptureRecordType type,
	int limit,
	int offset,
	ListLatestCapturesCallback callback,
	void *context )
{
	sqlite3_stmt* statement;
	int rc = sqlite3_prepare( dbPtr, sqlListLatestFastcapsV2, -1, &statement, 0 );

	sqlite3_bind_int( statement, 1, type );
	sqlite3_bind_int( statement, 2, limit );
	sqlite3_bind_int( statement, 3, offset );

	rc = sqlite3_step( statement );
	while ( rc == SQLITE_ROW ) {
		const char *mapname = ( const char* )sqlite3_column_text( statement, 0 );
		const int rank = sqlite3_column_int( statement, 1 );
		const char *player_name = ( const char* )sqlite3_column_text( statement, 2 );
		const unsigned int player_ip_int = sqlite3_column_int( statement, 3 );
		const char *player_cuid_hash2 = ( const char* )sqlite3_column_text( statement, 4 );
		const int capture_time = sqlite3_column_int( statement, 5 );
		const time_t date = sqlite3_column_int64( statement, 6 );

		callback( context, mapname, rank, type, player_name, player_ip_int, player_cuid_hash2, capture_time, date );

		rc = sqlite3_step( statement );
	}

	sqlite3_finalize( statement );
}

// returns how many records were written, or -1 if the capture record list is inactive (disabled)
int G_DBSaveCaptureRecords( CaptureRecordList *recordsToSave )
{
	if ( !recordsToSave->enabled ) {
		return -1;
	}

	if ( !recordsToSave->changed ) {
		return 0;
	}

	sqlite3_stmt* statement;

	// first, delete all of the old records for this map, even those that didn't change
	sqlite3_prepare( dbPtr, sqlremoveFastcapsV2, -1, &statement, 0 );
	sqlite3_bind_text( statement, 1, recordsToSave->mapname, -1, 0 );
	sqlite3_step( statement );
	sqlite3_finalize( statement );

	// rewrite everything
	sqlite3_prepare( dbPtr, sqlAddFastcapV2, -1, &statement, 0 );

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
			sqlite3_bind_int( statement, 2, j + 1 );
			sqlite3_bind_int( statement, 3, i );
			sqlite3_bind_text( statement, 4, record->recordHolderName, -1, 0 );
			sqlite3_bind_int( statement, 5, record->recordHolderIpInt );

			if ( VALIDSTRING( record->recordHolderCuid ) ) {
				sqlite3_bind_text( statement, 6, record->recordHolderCuid, -1, 0 );
			} else {
				sqlite3_bind_null( statement, 6 );
			}

			sqlite3_bind_int( statement, 7, record->captureTime );
			sqlite3_bind_int( statement, 8, record->whoseFlag );
			sqlite3_bind_int( statement, 9, record->maxSpeed );
			sqlite3_bind_int( statement, 10, record->avgSpeed );
			sqlite3_bind_int64( statement, 11, record->date );

			if ( VALIDSTRING( record->matchId ) ) {
				sqlite3_bind_text( statement, 12, record->matchId, -1, 0 );
			} else {
				sqlite3_bind_null( statement, 12 );
			}

			sqlite3_bind_int( statement, 13, record->recordHolderClientId );
			sqlite3_bind_int( statement, 14, record->pickupLevelTime);

			sqlite3_step( statement );
			++saved;
		}
	}

	sqlite3_finalize( statement );

	recordsToSave->changed = qfalse;

	return saved;
}

// this function assumes the arrays in currentRecords are sorted by captureTime and date
// returns 0 if this is not a record, or the newly assigned rank otherwise (1 = 1st, 2 = 2nd...)
int G_DBAddCaptureTime( unsigned int ipInt,
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

		// we know we have AT LEAST done better than our current record, so we will save something in any case
		// now, if we didn't already have the top record, check if we did less than the better records
		if ( newIndex > 0 ) {
			const int currentRecordIndex = newIndex;

			for ( ; newIndex > 0; --newIndex ) {
				if ( recordArray[newIndex - 1].captureTime && recordArray[newIndex - 1].captureTime <= captureTime ) {
					break; // this one is better or equal, so use the index after it
				}
			}

			if ( newIndex != currentRecordIndex ) {
				// we indeed did less than a record which was better than our former record. use its index and shift the array
				memmove( recordArray + newIndex + 1, recordArray + newIndex, ( currentRecordIndex - newIndex ) * sizeof( *recordArray ) );
			}
		}
	} else {
		// this player doesn't have a record in this category yet, so find an index by comparing times from the worst to the best
		for ( newIndex = MAX_SAVED_RECORDS; newIndex > 0; --newIndex ) {
			if ( recordArray[newIndex - 1].captureTime && recordArray[newIndex - 1].captureTime <= captureTime ) {
				break; // this one is better or equal, so use the index after it
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
	newElement->maxSpeed = maxSpeed;
	newElement->avgSpeed = avgSpeed;
	newElement->date = date;
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

	// marks changes to be saved in db
	currentRecords->changed = qtrue;

	// if we are using in memory db, we can afford saving all records straight away
	if ( g_inMemoryDB.integer ) {
		G_DBSaveCaptureRecords( currentRecords );
	}

	return newIndex + 1;
}

// =========== WHITELIST =======================================================

const char* const sqlCreateWhitelistTable =
"CREATE TABLE IF NOT EXISTS ip_whitelist (                                      "
"    [ip_int] INTEGER PRIMARY KEY,                                              "
"    [mask_int] INTEGER,                                                        "
"    [notes] TEXT);                                                             ";

const char* const sqlIsIpWhitelisted =
"SELECT COUNT(*)                               "
"FROM ip_whitelist                             "
"WHERE( ip_int & mask_int ) = (? & mask_int)   ";

const char* const sqlAddToWhitelist =
"INSERT INTO ip_whitelist (ip_int, mask_int, notes)  "
"VALUES (?,?,?)                                      ";

const char* const sqlremoveFromWhitelist =
"DELETE FROM ip_whitelist   "
"WHERE ip_int = ?           "
"AND mask_int = ?           ";

void InitWhitelist( void )
{
	sqlite3_exec( dbPtr, sqlCreateWhitelistTable, 0, 0, 0 );
}

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

const char* const sqlCreateBlacklistTable =
"CREATE TABLE IF NOT EXISTS ip_blacklist (                                      "
"    [ip_int] INTEGER PRIMARY KEY,                                              "
"    [mask_int] INTEGER,                                                        "
"    [notes] TEXT,                                                              "
"    [reason] TEXT,                                                             "
"    [banned_since] DATETIME,                                                   "
"    [banned_until] DATETIME);                                                  ";

const char* const sqlIsIpBlacklisted =
"SELECT reason                                 "
"FROM ip_blacklist                             "
"WHERE( ip_int & mask_int ) = (? & mask_int)   "
"AND banned_until >= datetime('now')           ";

const char* const sqlListBlacklist =
"SELECT ip_int, mask_int, notes, reason, banned_since, banned_until   "
"FROM ip_blacklist                                                    ";

const char* const sqlAddToBlacklist =
"INSERT INTO ip_blacklist (ip_int,                                  "
"mask_int, notes, reason, banned_since, banned_until)               "
"VALUES (?,?,?,?,datetime('now'),datetime('now','+'||?||' hours'))  ";

const char* const sqlRemoveFromBlacklist =
"DELETE FROM ip_blacklist   "
"WHERE ip_int = ?           "
"AND mask_int = ?           ";

void InitBlacklist( void )
{
	sqlite3_exec( dbPtr, sqlCreateBlacklistTable, 0, 0, 0 );
}

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

const char* const sqlCreatePoolsTables =
"CREATE TABLE IF NOT EXISTS pools (                                             "
"    [pool_id] INTEGER PRIMARY KEY AUTOINCREMENT,                               "
"    [short_name] TEXT,                                                         "
"    [long_name] TEXT );                                                        "
"                                                                               "
"CREATE TABLE IF NOT EXISTS pool_has_map (                                      "
"    [pool_id] INTEGER REFERENCES [pools]([pool_id]) ON DELETE RESTRICT,        "
"    [mapname] TEXT,                                                            "
"    [weight] INTEGER);                                                         ";

const char* const sqlListPools =
"SELECT pool_id, short_name, long_name   "
"FROM pools                              ";

const char* const sqlListMapsInPool =
"SELECT long_name, pools.pool_id, mapname, weight    "
"FROM pools                                          "
"JOIN pool_has_map                                   "
"ON pools.pool_id = pool_has_map.pool_id             "
"WHERE short_name = ? AND mapname <> ?               ";

const char* const sqlFindPool =
"SELECT pools.pool_id, long_name                                            "
"FROM pools                                                                 "
"JOIN                                                                       "
"pool_has_map                                                               "
"ON pools.pool_id = pool_has_map.pool_id                                    "
"WHERE short_name = ?                                                       ";

const char* const sqlCreatePool =
"INSERT INTO pools (short_name, long_name) "
"VALUES (?,?)                              ";

const char* const sqlDeleteAllMapsInPool =
"DELETE FROM pool_has_map                "
"WHERE pool_id                           "
"IN                                      "
"( SELECT pools.pool_id                  "
"FROM pools                              "
"JOIN pool_has_map                       "
"ON pools.pool_id = pool_has_map.pool_id "
"WHERE short_name = ? )                  ";

const char* const sqlDeletePool =
"DELETE FROM pools          "
"WHERE short_name = ?;      ";

const char* const sqlAddMapToPool =
"INSERT INTO pool_has_map (pool_id, mapname, weight)   "
"SELECT pools.pool_id, ?, ?                            "
"FROM pools                                            "
"WHERE short_name = ?                                  ";

const char* const sqlRemoveMapToPool =
"DELETE FROM pool_has_map                "
"WHERE pool_id                           "
"IN                                      "
"( SELECT pools.pool_id                  "
"FROM pools                              "
"JOIN pool_has_map                       "
"ON pools.pool_id = pool_has_map.pool_id "
"WHERE short_name = ? )                  "
"AND mapname = ? ;                       ";

void InitPools( void )
{
	sqlite3_exec( dbPtr, sqlCreatePoolsTables, 0, 0, 0 );
}

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
