#include "g_database.h"
#include "sqlite3.h"

// =========== ADD 'RANK' COLUMN TO OLD JKA_LOG.DB =============================

const char* const old_sqlTestFastcapsV2RankColumn =
"PRAGMA table_info(fastcapsV2);";

const char* const old_sqlAddFastcapsV2RankColumn =
"ALTER TABLE fastcapsV2 ADD rank INTEGER;";

const char* const old_sqlListAllFastcapsV2Maps =
"SELECT DISTINCT mapname FROM fastcapsV2;";

const char* const old_sqlAddFastcapV2 =
"INSERT INTO fastcapsV2 (                                                       "
"    mapname, rank, type, player_name, player_ip_int, player_cuid_hash2,        "
"    capture_time, whose_flag, max_speed, avg_speed, date, match_id,            "
"    client_id, pickup_time)                                                    "
"VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)                                           ";

const char* const old_sqlremoveFastcapsV2 =
"DELETE FROM fastcapsV2 WHERE mapname = ?                                       ";

const char* const old_sqlGetFastcapsV2 =
"SELECT player_name, player_ip_int, player_cuid_hash2, capture_time,            "
"whose_flag, max_speed, avg_speed, date, match_id, client_id, pickup_time       "
"FROM fastcapsV2                                                                "
"WHERE fastcapsV2.mapname = ?1 AND fastcapsV2.type = ?2                         "
"ORDER BY capture_time                                                          "
"LIMIT ?3                                                                       ";

// returns how many records were loaded
static int LoadOldCaptureRecords(const char* mapname, CaptureRecordList* recordsToLoad, sqlite3* logDb)
{
	memset(recordsToLoad, 0, sizeof(*recordsToLoad));

	// make sure we always make a lower case map lookup
	Q_strncpyz(recordsToLoad->mapname, mapname, sizeof(recordsToLoad->mapname));
	Q_strlwr(recordsToLoad->mapname);

	sqlite3_stmt* statement;
	int i, rc = -1, loaded = 0;

	rc = sqlite3_prepare(logDb, old_sqlGetFastcapsV2, -1, &statement, 0);

	for (i = 0; i < CAPTURE_RECORD_NUM_TYPES; ++i) {
		sqlite3_reset(statement);
		sqlite3_clear_bindings(statement);

		sqlite3_bind_text(statement, 1, recordsToLoad->mapname, -1, 0);
		sqlite3_bind_int(statement, 2, i);
		sqlite3_bind_int(statement, 3, MAX_SAVED_RECORDS);

		rc = sqlite3_step(statement);

		int j = 0;
		while (rc == SQLITE_ROW && j < MAX_SAVED_RECORDS) {
			// get the fields from the query
			const char* player_name = (const char*)sqlite3_column_text(statement, 0);
			const unsigned int player_ip_int = sqlite3_column_int(statement, 1);
			const char* player_cuid_hash2 = (const char*)sqlite3_column_text(statement, 2);
			const int capture_time = sqlite3_column_int(statement, 3);
			const int whose_flag = sqlite3_column_int(statement, 4);
			const int max_speed = sqlite3_column_int(statement, 5);
			const int avg_speed = sqlite3_column_int(statement, 6);
			const time_t date = sqlite3_column_int64(statement, 7);
			const char* match_id = (const char*)sqlite3_column_text(statement, 8);
			const int client_id = sqlite3_column_int(statement, 9);
			const int pickup_time = sqlite3_column_int(statement, 10);

			// write them to the record list
			CaptureRecord* record = &recordsToLoad->records[i][j];

			Q_strncpyz(record->recordHolderName, player_name, sizeof(record->recordHolderName));
			record->recordHolderIpInt = player_ip_int;

			if (VALIDSTRING(player_cuid_hash2)) {
				Q_strncpyz(record->recordHolderCuid, player_cuid_hash2, sizeof(record->recordHolderCuid));
			}

			record->captureTime = capture_time;
			record->whoseFlag = whose_flag;
			record->maxSpeed = max_speed;
			record->avgSpeed = avg_speed;
			record->date = date;

			if (VALIDSTRING(match_id)) {
				Q_strncpyz(record->matchId, match_id, sizeof(record->matchId));
			}

			record->recordHolderClientId = client_id;
			record->pickupLevelTime = pickup_time;

			rc = sqlite3_step(statement);
			++loaded;
			++j;
		}
	}

	sqlite3_finalize(statement);

	// write the remaining global fields
	recordsToLoad->enabled = qtrue;

	return loaded;
}

static int SaveOldCaptureRecords(CaptureRecordList* recordsToSave, sqlite3* logDb)
{
	if (!recordsToSave->enabled) {
		return -1;
	}

	if (!recordsToSave->changed) {
		return 0;
	}

	sqlite3_stmt* statement;

	// first, delete all of the old records for this map, even those that didn't change
	sqlite3_prepare(logDb, old_sqlremoveFastcapsV2, -1, &statement, 0);
	sqlite3_bind_text(statement, 1, recordsToSave->mapname, -1, 0);
	sqlite3_step(statement);
	sqlite3_finalize(statement);

	// rewrite everything
	sqlite3_prepare(logDb, old_sqlAddFastcapV2, -1, &statement, 0);

	int i, j, saved = 0;
	for (i = 0; i < CAPTURE_RECORD_NUM_TYPES; ++i) {
		for (j = 0; j < MAX_SAVED_RECORDS; ++j) {
			CaptureRecord* record = &recordsToSave->records[i][j];

			if (!record->captureTime) {
				continue; // not a valid record
			}

			sqlite3_reset(statement);
			sqlite3_clear_bindings(statement);

			sqlite3_bind_text(statement, 1, recordsToSave->mapname, -1, 0);
			sqlite3_bind_int(statement, 2, j + 1);
			sqlite3_bind_int(statement, 3, i);
			sqlite3_bind_text(statement, 4, record->recordHolderName, -1, 0);
			sqlite3_bind_int(statement, 5, record->recordHolderIpInt);

			if (VALIDSTRING(record->recordHolderCuid)) {
				sqlite3_bind_text(statement, 6, record->recordHolderCuid, -1, 0);
			}
			else {
				sqlite3_bind_null(statement, 6);
			}

			sqlite3_bind_int(statement, 7, record->captureTime);
			sqlite3_bind_int(statement, 8, record->whoseFlag);
			sqlite3_bind_int(statement, 9, record->maxSpeed);
			sqlite3_bind_int(statement, 10, record->avgSpeed);
			sqlite3_bind_int64(statement, 11, record->date);

			if (VALIDSTRING(record->matchId)) {
				sqlite3_bind_text(statement, 12, record->matchId, -1, 0);
			}
			else {
				sqlite3_bind_null(statement, 12);
			}

			sqlite3_bind_int(statement, 13, record->recordHolderClientId);
			sqlite3_bind_int(statement, 14, record->pickupLevelTime);

			sqlite3_step(statement);
			++saved;
		}
	}

	sqlite3_finalize(statement);

	recordsToSave->changed = qfalse;

	return saved;
}

static void CheckRankColumn(sqlite3* logDb) {
	// test whether we need to add the "rank" column to fastcapsV2 to jka_log.db's fastcaps table
// otherwise, the upgrade might have an error
// add a rank column if it doesn't have it (added late for better query performance)
	sqlite3_stmt* statement;
	int rc = sqlite3_prepare(logDb, old_sqlTestFastcapsV2RankColumn, -1, &statement, 0);
	rc = sqlite3_step(statement);
	qboolean foundRankColumn = qfalse;
	while (rc == SQLITE_ROW) {
		const char* name = (const char*)sqlite3_column_text(statement, 1);
		if (name && *name && !Q_stricmp(name, "rank"))
			foundRankColumn = qtrue;
		rc = sqlite3_step(statement);
	}
	sqlite3_finalize(statement);
	if (foundRankColumn) {
		//G_LogPrintf("Found 'rank' column in old jka_log.db fastcapsV2 table; not bothering to add it.\n");
	}
	else {
		G_LogPrintf("Adding 'rank' column to old jka_log.db fastcapsV2 table, indexing ranks automatically (this may take a while)\n");
		sqlite3_exec(logDb, old_sqlAddFastcapsV2RankColumn, 0, 0, 0);

		int recordsIndexed = 0;

		// for each map, load and save records so that rank indexing is done automatically (will take some time)
		rc = sqlite3_prepare(logDb, old_sqlListAllFastcapsV2Maps, -1, &statement, 0);
		rc = sqlite3_step(statement);
		while (rc == SQLITE_ROW) {
			const char* thisMapname = (const char*)sqlite3_column_text(statement, 0);
			if (VALIDSTRING(thisMapname)) {
				// load and save so ranks are generated
				CaptureRecordList thisMapRecords;
				LoadOldCaptureRecords(thisMapname, &thisMapRecords, logDb);
				thisMapRecords.changed = qtrue;
				recordsIndexed += SaveOldCaptureRecords(&thisMapRecords, logDb);
			}
			rc = sqlite3_step(statement);
		}
		sqlite3_finalize(statement);

		G_LogPrintf("Indexed %d records successfully\n", recordsIndexed);
	}
}



const char* const oldLogDbFilename = "jka_log.db";
const char* const oldCfgDbFilename = "jka_config.db";

// =========== METADATA ========================================================

const char* const sqlCopyOldTablesToNewV1DB =
"ATTACH DATABASE 'jka_log.db' AS logDb;                                      \n"
"ATTACH DATABASE 'jka_config.db' as cfgDb;                                   \n"
//"BEGIN TRANSACTION;                                                          \n"
"                                                                            \n"
"INSERT INTO main.fastcapsV2                                                 \n"
"    (fastcap_id, mapname, rank, type, player_name, player_ip_int,           \n"
"    player_cuid_hash2, capture_time, whose_flag, max_speed, avg_speed,      \n"
"    date, match_id, client_id, pickup_time)                                 \n"
"SELECT                                                                      \n"
"    fastcap_id, mapname, rank, type, player_name, player_ip_int,            \n"
"    player_cuid_hash2, capture_time, whose_flag, max_speed, avg_speed,      \n"
"    date, match_id, client_id, pickup_time                                  \n"
"FROM logDb.fastcapsV2;                                                      \n"
"                                                                            \n"
"INSERT INTO main.nicknames                                                  \n"
"    (ip_int, name, duration, cuid_hash2)                                    \n"
"SELECT                                                                      \n"
"    ip_int, name, duration, cuid_hash2                                      \n"
"FROM logDb.nicknames;                                                       \n"
"                                                                            \n"
"INSERT INTO main.ip_whitelist                                               \n"
"    (ip_int, mask_int, notes)                                               \n"
"SELECT                                                                      \n"
"    ip_int, mask_int, notes                                                 \n"
"FROM cfgDb.ip_whitelist;                                                    \n"
"                                                                            \n"
"INSERT INTO main.ip_blacklist                                               \n"
"    (ip_int, mask_int, notes, reason, banned_since, banned_until)           \n"
"SELECT                                                                      \n"
"    ip_int, mask_int, notes, reason, banned_since, banned_until             \n"
"FROM cfgDb.ip_blacklist;                                                    \n"
"                                                                            \n"
"INSERT INTO main.pools                                                      \n"
"    (pool_id, short_name, long_name)                                        \n"
"SELECT                                                                      \n"
"    pool_id, short_name, long_name                                          \n"
"FROM cfgDb.pools;                                                           \n"
"                                                                            \n"
"INSERT INTO main.pool_has_map                                               \n"
"    (pool_id, mapname, weight)                                              \n"
"SELECT                                                                      \n"
"    pool_id, mapname, weight                                                \n"
"FROM cfgDb.pool_has_map;                                                    \n"
"                                                                            \n"
//"COMMIT TRANSACTION;                                                         \n"
"DETACH DATABASE logDb;                                                      \n"
"DETACH DATABASE cfgDb;                                                        ";

static qboolean UpgradeDBToVersion1( sqlite3* dbPtr ) {
	// special type of upgrade: upgrading from the two old file databases to version 1 of single database model

	int rc;

	// try to open both databases temporarily to make sure they exist, because ATTACH creates files that don't exist

	sqlite3* logDb;
	sqlite3* cfgDb;

	rc = sqlite3_open_v2( oldLogDbFilename, &logDb, SQLITE_OPEN_READWRITE, NULL );

	if ( rc != SQLITE_OK ) {
		Com_Printf( "Failed to open logs database file %s for upgrading (code: %d)\n", oldLogDbFilename, rc );
		return qfalse;
	}


	rc = sqlite3_open_v2( oldCfgDbFilename, &cfgDb, SQLITE_OPEN_READONLY, NULL );

	if ( rc != SQLITE_OK ) {
		Com_Printf( "Failed to open config database file %s for upgrading (code: %d)\n", oldCfgDbFilename, rc );
		return qfalse;
	}

	// we have to perform a special check as to whether the existing jka_log.db fastcapsV2 table
	// had the "rank" column, and add+initialize it if not in order for the upgrade function to
	// work properly
	CheckRankColumn(logDb);

	sqlite3_close( logDb );
	sqlite3_close( cfgDb );

	// upgrade
	return sqlite3_exec( dbPtr, sqlCopyOldTablesToNewV1DB, NULL, NULL, NULL ) == SQLITE_OK;
}

const char* const sqlAddSeedField =
"ALTER TABLE fastcapsv2 ADD COLUMN seed TEXT;";

static qboolean UpgradeDBToVersion2(sqlite3* dbPtr) {
	// add "seed" field to fastcaps for use with weekly challenges
	return sqlite3_exec(dbPtr, sqlAddSeedField, NULL, NULL, NULL) == SQLITE_OK;
}

// =============================================================================

static qboolean UpgradeDB( int versionTo, sqlite3* dbPtr ) {
	Com_Printf( "Upgrading database to version %d...\n", versionTo );

	switch ( versionTo ) {
		case 1: return UpgradeDBToVersion1( dbPtr );
		case 2: return UpgradeDBToVersion2( dbPtr );
		default:
			Com_Printf( "ERROR: Unsupported database upgrade routine\n" );
	}

	return qfalse;
}

qboolean G_DBUpgradeDatabaseSchema( int versionFrom, void* db ) {
	if (versionFrom == DB_SCHEMA_VERSION) {
		// already up-to-date
		return qtrue;
	} else if (versionFrom > DB_SCHEMA_VERSION) {
		// don't let older servers load more recent databases
		Com_Printf("ERROR: Database version is higher than the one used by this mod version!\n");
		return qfalse;
	} else if (versionFrom < 0) {
		// ???
		versionFrom = 0;
	}

	sqlite3* dbPtr = ( sqlite3* )db;

	while ( versionFrom < DB_SCHEMA_VERSION ) {
		if ( !UpgradeDB( ++versionFrom, dbPtr ) ) {
			return qfalse;
		}
	}

	Com_Printf( "Database upgrade successful\n" );

	return qtrue;
}