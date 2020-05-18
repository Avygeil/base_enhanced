#include "g_database.h"
#include "sqlite3.h"
#include "cJSON.h"

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

static qboolean UpgradeFromOldVersions( sqlite3* dbPtr ) {
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

// ================ V3 UPGRADE =================================================

const char* const v3Upgrade =
"ALTER TABLE sessions RENAME TO tmp_sessions;                                \n"
"CREATE TABLE IF NOT EXISTS [sessions] (                                     \n"
"    [session_id] INTEGER NOT NULL,                                          \n"
"    [hash] INTEGER NOT NULL,                                                \n"
"    [info] TEXT NOT NULL,                                                   \n"
"    [account_id] INTEGER DEFAULT NULL,                                      \n"
"    PRIMARY KEY ( [session_id] ),                                           \n"
"    UNIQUE ( [hash] ),                                                      \n"
"    FOREIGN KEY ( [account_id] )                                            \n"
"        REFERENCES accounts ( [account_id] )                                \n"
"        ON DELETE SET NULL                                                  \n"
");                                                                          \n"
"INSERT INTO sessions(session_id, hash, info, account_id)                    \n"
"SELECT session_id, identifier, info, account_id FROM tmp_sessions;          \n"
"DROP TABLE tmp_sessions;                                                    \n"
"DROP VIEW sessions_info;                                                    \n"
"CREATE VIEW IF NOT EXISTS sessions_info AS                                  \n"
"SELECT                                                                      \n"
"    session_id,                                                             \n"
"    hash,                                                                   \n"
"    info,                                                                   \n"
"    account_id,                                                             \n"
"    CASE WHEN (                                                             \n"
"	     sessions.account_id IS NOT NULL                                     \n"
"    )                                                                       \n"
"    THEN 1 ELSE 0 END referenced                                            \n"
"FROM sessions;                                                              \n"
"UPDATE accounts SET name = LOWER(name);                                       ";

static qboolean UpgradeDBToVersion3(sqlite3* dbPtr) {
	// rename sessions column identifier -> hash
	// force lowercase for existing account names
	return sqlite3_exec(dbPtr, v3Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

// ================ V4 UPGRADE =================================================

const char* const v4NewTables =
"CREATE TABLE IF NOT EXISTS [nicknames_NEW] (                                                        \n"
"    [nickname_id] INTEGER NOT NULL,                                                                 \n"
"    [session_id] INTEGER NOT NULL,                                                                  \n"
"    [name] TEXT NOT NULL,                                                                           \n"
"    [duration] INTEGER NOT NULL DEFAULT 0,                                                          \n"
"    PRIMARY KEY ( [nickname_id] ),                                                                  \n"
"    UNIQUE ( [session_id], [name] ),                                                                \n"
"    FOREIGN KEY ( [session_id] )                                                                    \n"
"        REFERENCES sessions ( [session_id] )                                                        \n"
"        ON DELETE CASCADE                                                                           \n"
");                                                                                                  \n"
"                                                                                                    \n"
"CREATE TABLE IF NOT EXISTS [fastcaps_NEW] (                                                         \n"
"    [fastcap_id] INTEGER NOT NULL,                                                                  \n"
"    [mapname] TEXT NOT NULL,                                                                        \n"
"    [type] INTEGER NOT NULL,                                                                        \n"
"    [session_id] INTEGER NOT NULL,                                                                  \n"
"    [capture_time] INTEGER NOT NULL,                                                                \n"
"    [date] NOT NULL DEFAULT (strftime('%s', 'now')),                                                \n"
"    [extra] TEXT DEFAULT NULL,                                                                      \n"
"    PRIMARY KEY ( [fastcap_id] ),                                                                   \n"
"    UNIQUE ( [mapname], [type], [session_id] ),                                                     \n"
"    FOREIGN KEY ( [session_id] )                                                                    \n"
"        REFERENCES sessions ( [session_id] )                                                        \n"
");                                                                                                  \n"
"                                                                                                    \n"
"CREATE TABLE IF NOT EXISTS nicknames_TMP (                                                          \n"
"    [ip_int] INTEGER,                                                                               \n"
"    [name] TEXT,                                                                                    \n"
"    [duration] INTEGER,                                                                             \n"
"    [cuid_hash2] TEXT,                                                                               \n"
"    UNIQUE ( [ip_int], [name], [cuid_hash2] )                                                       \n"
");                                                                                                    ";

const char* const v4TerminateMyLife =
"DROP INDEX nicknames_ip_int_idx;                      \n"
"DROP INDEX nicknames_cuid_hash2_idx;                  \n"
"DROP VIEW sessions_info;                              \n"
"DROP TABLE nicknames_TMP;                             \n"
"DROP TABLE nicknames;                                 \n"
"DROP TABLE fastcapsV2;                                \n"
"DELETE from metadata WHERE key = 'oldWaypointNames';  \n"
"DELETE from metadata WHERE key = 'lastWaypointsHash'; \n"
"DELETE from metadata WHERE key = 'lastWaypointNames'; \n"
"ALTER TABLE fastcaps_NEW RENAME TO fastcaps;          \n"
"ALTER TABLE nicknames_NEW RENAME TO nicknames;          ";

const char* const v4CreateNewShit =
"CREATE VIEW IF NOT EXISTS sessions_info AS                                                          \n"
"SELECT                                                                                              \n"
"    session_id,                                                                                     \n"
"    hash,                                                                                           \n"
"    info,                                                                                           \n"
"    account_id,                                                                                     \n"
"    CASE WHEN ( sessions.account_id IS NOT NULL )                                                   \n"
"    THEN ( SELECT name FROM accounts WHERE accounts.account_id = sessions.account_id )              \n"
"    ELSE 'Padawan' END                                                                              \n"
"    display_name,                                                                                   \n"
"    CASE WHEN (                                                                                     \n"
"	     sessions.account_id IS NOT NULL                                                             \n"
"	     OR EXISTS( SELECT 1 FROM fastcaps WHERE fastcaps.session_id = sessions.session_id )         \n"
"    )                                                                                               \n"
"    THEN 1 ELSE 0 END                                                                               \n"
"    referenced                                                                                      \n"
"FROM sessions;                                                                                      \n"
"                                                                                                    \n"
"CREATE TRIGGER IF NOT EXISTS sessions_info_delete_trigger                                           \n"
"INSTEAD OF DELETE ON sessions_info                                                                  \n"
"BEGIN                                                                                               \n"
"    DELETE FROM sessions WHERE sessions.session_id = OLD.session_id;                                \n"
"END;                                                                                                \n"
"                                                                                                    \n"
"CREATE VIEW IF NOT EXISTS fastcaps_info AS                                                          \n"
"SELECT                                                                                              \n"
"    fastcap_id,                                                                                     \n"
"    mapname,                                                                                        \n"
"    type,                                                                                           \n"
"    CASE WHEN (                                                                                     \n"
"        ( SELECT account_id FROM sessions WHERE sessions.session_id = fastcaps.session_id )         \n"
"        IS NOT NULL                                                                                 \n"
"    )                                                                                               \n"
"    THEN 1 ELSE 0 END                                                                               \n"
"    logged_in,                                                                                      \n"
"    ( SELECT display_name FROM sessions_info WHERE sessions_info.session_id = fastcaps.session_id ) \n"
"    display_name,                                                                                   \n"
"    RANK() OVER (                                                                                   \n"
"	     PARTITION BY fastcaps.mapname, fastcaps.type                                                \n"
"	     ORDER BY fastcaps.capture_time ASC                                                          \n"
"    )                                                                                               \n"
"    rank,                                                                                           \n"
"    capture_time,                                                                                   \n"
"    date,                                                                                           \n"
"    extra                                                                                           \n"
"FROM fastcaps;                                                                                        ";

extern void NormalizeName(const char* in, char* out, int outSize, int colorlessSize);

static qboolean UpgradeDBToVersion4(sqlite3* dbPtr) {
	if (sqlite3_exec(dbPtr, v4NewTables, NULL, NULL, NULL) != SQLITE_OK)
		return qfalse;

	sqlite3_stmt* statement = NULL;
	sqlite3_stmt* statement2 = NULL;
	sqlite3_stmt* statement3 = NULL;
	int rc;

	// merge nicknames
	Com_Printf("Merging nicknames\n");
	sqlite3_prepare(dbPtr, "SELECT * FROM nicknames;", -1, &statement, 0);
	sqlite3_prepare(dbPtr, "INSERT OR IGNORE INTO nicknames_TMP(ip_int, name, duration, cuid_hash2) VALUES (?1, ?2, ?3, ?4);", -1, &statement2, 0);
	sqlite3_prepare(dbPtr, "UPDATE nicknames_TMP SET duration=duration+?1 WHERE ip_int=?2 AND name=?3 AND cuid_hash2=?4;", -1, &statement3, 0);
	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const int ip_int = sqlite3_column_int(statement, 0);
		const char* name = (const char*)sqlite3_column_text(statement, 1);
		const int duration = sqlite3_column_int(statement, 2);
		const char* cuid_hash2 = (const char*)sqlite3_column_text(statement, 3);

		// make sure even very old names are also normalized
		char cleanname[MAX_NETNAME];
		NormalizeName(name, cleanname, sizeof(cleanname), g_maxNameLength.integer);

		if (ip_int && VALIDSTRING(name)) {
			sqlite3_reset(statement2);
			sqlite3_bind_int(statement2, 1, ip_int);
			sqlite3_bind_text(statement2, 2, cleanname, -1, 0);
			sqlite3_bind_int(statement2, 3, 0);
			if (VALIDSTRING(cuid_hash2)) {
				sqlite3_bind_text(statement2, 4, cuid_hash2, -1, 0);
			} else {
				sqlite3_bind_text(statement2, 4, "", -1, 0);
			}
			sqlite3_step(statement2);

			sqlite3_reset(statement3);
			sqlite3_bind_int(statement3, 1, duration);
			sqlite3_bind_int(statement3, 2, ip_int);
			sqlite3_bind_text(statement3, 3, cleanname, -1, 0);
			if (VALIDSTRING(cuid_hash2)) {
				sqlite3_bind_text(statement3, 4, cuid_hash2, -1, 0);
			} else {
				sqlite3_bind_text(statement3, 4, "", -1, 0);
			}
			sqlite3_step(statement3);
		}

		rc = sqlite3_step(statement);
	}
	sqlite3_finalize(statement);
	sqlite3_finalize(statement2);
	sqlite3_finalize(statement3);

	// port nicknames by creating sessions for them
	Com_Printf("Port nicknames to sessions\n");
	sqlite3_prepare(dbPtr, "SELECT * FROM nicknames_TMP;", -1, &statement, 0);
	sqlite3_prepare(dbPtr, "INSERT OR IGNORE INTO nicknames_NEW (session_id, name) VALUES (?1, ?2);", -1, &statement2, 0);
	sqlite3_prepare(dbPtr, "UPDATE nicknames_NEW SET duration=duration+?1 WHERE session_id=?2 AND name=?3;", -1, &statement3, 0);
	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const int ip_int = sqlite3_column_int(statement, 0);
		const char* name = (const char*)sqlite3_column_text(statement, 1);
		const int duration = sqlite3_column_int(statement, 2);
		const char* cuid_hash2 = (const char*)sqlite3_column_text(statement, 3);

		if (duration >= 0) {
			sessionInfoHash_t hash;
			char sessionInfo[MAX_SESSIONINFO_LEN];

			unsigned int ip = (unsigned int)ip_int;
			char ipBuf[32];

			cJSON* root = cJSON_CreateObject();

			getStringFromIp(ip, ipBuf, sizeof(ipBuf));
			cJSON_AddStringToObject(root, "ip", ipBuf);
			if (VALIDSTRING(cuid_hash2))
				cJSON_AddStringToObject(root, "cuid_hash2", cuid_hash2);

			char* info = cJSON_PrintUnformatted(root);
			Q_strncpyz(sessionInfo, info, sizeof(sessionInfo));
			free(info);

			hash = XXH64(sessionInfo, strlen(sessionInfo), 0L);

			cJSON_Delete(root);

			session_t session;
			if (!G_DBGetSessionByHash(hash, &session)) {
				G_DBCreateSession(hash, sessionInfo);
				if (!G_DBGetSessionByHash(hash, &session)) {
					return qfalse;
				}
			}

			if (Q_stricmp(session.info, sessionInfo)) {
				Com_Printf("ERROR: Hash collision!!!\n%s\n%s\n", session.info, sessionInfo);
				return qfalse;
			}

			sqlite3_reset(statement2);
			sqlite3_bind_int(statement2, 1, session.id);
			sqlite3_bind_text(statement2, 2, name, -1, 0);
			sqlite3_step(statement2);

			sqlite3_reset(statement3);
			sqlite3_bind_int(statement3, 1, duration);
			sqlite3_bind_int(statement3, 2, session.id);
			sqlite3_bind_text(statement3, 3, name, -1, 0);
			sqlite3_step(statement3);
		}

		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
	sqlite3_finalize(statement2);
	sqlite3_finalize(statement3);

	// port fastcaps to the new format and make new sessions if they weren't created with nicknames
	Com_Printf("Port fastcaps to new format\n");
	sqlite3_prepare(dbPtr, "SELECT * FROM fastcapsV2;", -1, &statement, 0);
	sqlite3_prepare(dbPtr, "INSERT OR IGNORE INTO fastcaps_NEW(mapname, type, session_id, capture_time, date, extra) VALUES(?1, ?2, ?3, ?4, ?5, ?6);", -1, &statement2, 0);
	sqlite3_prepare(dbPtr, "UPDATE fastcaps_NEW SET capture_time=MIN(capture_time, ?1) WHERE mapname=?2 AND type=?3 AND session_id=?4;", -1, &statement3, 0);
	rc = sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const int fastcap_id = sqlite3_column_int(statement, 0);
		const char* mapname = (const char*)sqlite3_column_text(statement, 1);
		const int type = sqlite3_column_int(statement, 3);
		const char* player_name = (const char*)sqlite3_column_text(statement, 4);
		const int player_ip_int = sqlite3_column_int(statement, 5);
		const char* player_cuid_hash2 = (const char*)sqlite3_column_text(statement, 6);
		const int capture_time = sqlite3_column_int(statement, 7);
		const int whose_flag = sqlite3_column_int(statement, 8);
		const int max_speed = sqlite3_column_int(statement, 9);
		const int avg_speed = sqlite3_column_int(statement, 10);
		const time_t date = sqlite3_column_int64(statement, 11);
		const char* match_id = (const char*)sqlite3_column_text(statement, 12);
		const int client_id = sqlite3_column_int(statement, 13);
		const int pickup_time = sqlite3_column_int(statement, 14);

		{
			sessionInfoHash_t hash;
			char sessionInfo[MAX_SESSIONINFO_LEN];

			unsigned int ip = (unsigned int)player_ip_int;
			char ipBuf[32];

			cJSON* root = cJSON_CreateObject();

			getStringFromIp(ip, ipBuf, sizeof(ipBuf));
			cJSON_AddStringToObject(root, "ip", ipBuf);
			if (VALIDSTRING(player_cuid_hash2))
				cJSON_AddStringToObject(root, "cuid_hash2", player_cuid_hash2);

			char* info = cJSON_PrintUnformatted(root);
			Q_strncpyz(sessionInfo, info, sizeof(sessionInfo));
			free(info);

			hash = XXH64(sessionInfo, strlen(sessionInfo), 0L);

			cJSON_Delete(root);

			session_t session;
			if (!G_DBGetSessionByHash(hash, &session)) {
				G_DBCreateSession(hash, sessionInfo);
				if (!G_DBGetSessionByHash(hash, &session)) {
					return qfalse;
				}
			}

			if (Q_stricmp(session.info, sessionInfo)) {
				Com_Printf("ERROR: Hash collision!!!\n%s\n%s\n", session.info, sessionInfo);
				return qfalse;
			}

			// make sure even very old names are also normalized
			char cleanname[MAX_NETNAME];
			NormalizeName(player_name, cleanname, sizeof(cleanname), g_maxNameLength.integer);

			// build the extra string
			cJSON* root2 = cJSON_CreateObject();
			cJSON_AddStringToObject(root2, "match_id", match_id);
			cJSON_AddStringToObject(root2, "player_name", cleanname);
			cJSON_AddNumberToObject(root2, "client_id", client_id);
			cJSON_AddNumberToObject(root2, "pickup_time", pickup_time);
			cJSON_AddNumberToObject(root2, "whose_flag", whose_flag);
			cJSON_AddNumberToObject(root2, "max_speed", max_speed);
			cJSON_AddNumberToObject(root2, "avg_speed", avg_speed);

			char* extra = cJSON_PrintUnformatted(root2);

			sqlite3_reset(statement2);
			sqlite3_bind_text(statement2, 1, mapname, -1, 0);
			sqlite3_bind_int(statement2, 2, type);
			sqlite3_bind_int(statement2, 3, session.id);
			sqlite3_bind_int(statement2, 4, capture_time);
			sqlite3_bind_int64(statement2, 5, date);
			sqlite3_bind_text(statement2, 6, extra, -1, 0);
			sqlite3_step(statement2);

			sqlite3_reset(statement3);
			sqlite3_bind_int(statement3, 1, capture_time);
			sqlite3_bind_text(statement3, 2, mapname, -1, 0);
			sqlite3_bind_int(statement3, 3, type);
			sqlite3_bind_int(statement3, 4, session.id);
			sqlite3_step(statement3);

			free(extra);
			cJSON_Delete(root2);
		}

		rc = sqlite3_step(statement);
	}

	sqlite3_finalize(statement);
	sqlite3_finalize(statement2);
	sqlite3_finalize(statement3);

	Com_Printf("Finalizing...\n");

	// delete non normal/weapon records
	if (sqlite3_exec(dbPtr, "DELETE from fastcaps_NEW WHERE type > 1;", NULL, NULL, NULL) != SQLITE_OK)
		return qfalse;

	// remove the intermediate and old tables and rename the new ones
	if (sqlite3_exec(dbPtr, v4TerminateMyLife, NULL, NULL, NULL) != SQLITE_OK)
		return qfalse;

	// create the final views
	if (sqlite3_exec(dbPtr, v4CreateNewShit, NULL, NULL, NULL) != SQLITE_OK)
		return qfalse;

	// make sure to reduce the file size
	sqlite3_exec(dbPtr, "VACUUM;", NULL, NULL, NULL);
	sqlite3_exec(dbPtr, "PRAGMA optimize;", NULL, NULL, NULL);

	return qtrue;
}

// =============================================================================

static qboolean UpgradeDB( int versionTo, sqlite3* dbPtr ) {
	Com_Printf( "Upgrading database to version %d...\n", versionTo );

	switch ( versionTo ) {
		case 2: return UpgradeDBToVersion2( dbPtr );
		case 3: return UpgradeDBToVersion3( dbPtr );
		case 4: return UpgradeDBToVersion4( dbPtr );
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
	}

	sqlite3* dbPtr = (sqlite3*)db;

	if (versionFrom > 0) {
		// database already exists, we are just upgrading between versions

		while (versionFrom < DB_SCHEMA_VERSION) {
			if (!UpgradeDB(++versionFrom, dbPtr)) {
				return qfalse;
			}
		}

		Com_Printf("Database upgrade successful\n");
	} else {
		// special case: database was just created, try to upgrade from the even older versions
		// but don't fail if it's not possible
		Com_Printf("Attempting to convert jka_log.db and jka_config.db...\n");
		UpgradeFromOldVersions(dbPtr);
	}
	
	return qtrue;
}