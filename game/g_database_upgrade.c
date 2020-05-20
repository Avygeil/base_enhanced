#include "g_database.h"
#include "sqlite3.h"
#include "cJSON.h"

// ================ V2 UPGRADE =================================================

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
"    [mapname] TEXT COLLATE NOCASE NOT NULL,                                                         \n"
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
"CREATE VIEW IF NOT EXISTS player_aliases AS                                                         \n"
"SELECT                                                                                              \n"
"    session_id,                                                                                     \n"
"    account_id,                                                                                     \n"
"    CASE WHEN ( sessions.account_id IS NOT NULL ) THEN 1 ELSE 0 END                                 \n"
"    has_account,                                                                                    \n"
"    CASE WHEN ( sessions.account_id IS NOT NULL )                                                   \n"
"    THEN ( SELECT name FROM accounts WHERE accounts.account_id = sessions.account_id )              \n"
"    ELSE COALESCE (                                                                                 \n"
"        ( SELECT name FROM nicknames WHERE nicknames.session_id = sessions.session_id               \n"
"        ORDER BY nicknames.duration DESC LIMIT 1 ),                                                 \n"
"        'Padawan'                                                                                   \n"
"    ) END                                                                                           \n"
"    alias,                                                                                          \n"
"    COALESCE(                                                                                       \n"
"        CASE WHEN ( sessions.account_id IS NOT NULL )                                               \n"
"        THEN (                                                                                      \n"
"            SELECT SUM(duration)                                                                    \n"
"            FROM nicknames                                                                          \n"
"            LEFT JOIN sessions AS sess ON nicknames.session_id = sess.session_id                    \n"
"            WHERE account_id = sessions.account_id                                                  \n"
"        ) ELSE(                                                                                     \n"
"            SELECT SUM(duration)                                                                    \n"
"            FROM nicknames                                                                          \n"
"            WHERE session_id = sessions.session_id                                                  \n"
"        ) END,                                                                                      \n"
"        0                                                                                           \n"
"    )                                                                                               \n"
"    playtime	                                                                                     \n"
"FROM sessions;                                                                                      \n"
"                                                                                                    \n"
"CREATE VIEW IF NOT EXISTS fastcaps_ranks AS                                                         \n"
"SELECT                                                                                              \n"
"    fastcaps.fastcap_id,                                                                            \n"
"    fastcaps.mapname,                                                                               \n"
"    fastcaps.type,                                                                                  \n"
"    player_aliases.account_id,                                                                      \n"
"    player_aliases.alias AS name,                                                                   \n"
"    RANK() OVER (                                                                                   \n"
"	     PARTITION BY fastcaps.mapname, fastcaps.type                                                \n"
"	     ORDER BY fastcaps.capture_time ASC, fastcaps.date ASC                                       \n"
"    )                                                                                               \n"
"    rank,                                                                                           \n"
"    MIN(fastcaps.capture_time) AS capture_time,                                                     \n"
"    fastcaps.date,                                                                                  \n"
"    fastcaps.extra                                                                                  \n"
"FROM fastcaps, player_aliases                                                                       \n"
"WHERE fastcaps.session_id = player_aliases.session_id AND player_aliases.has_account                \n"
"GROUP BY fastcaps.mapname, fastcaps.type, player_aliases.account_id;                                \n"
"                                                                                                    \n"
"CREATE VIEW IF NOT EXISTS fastcaps_leaderboard AS                                                   \n"
"SELECT                                                                                              \n"
"    account_id,                                                                                     \n"
"    name,                                                                                           \n"
"    type,                                                                                           \n"
"    SUM( CASE WHEN rank = 1 THEN 1 ELSE 0 END ) AS golds,                                           \n"
"    SUM( CASE WHEN rank = 2 THEN 1 ELSE 0 END ) AS silvers,                                         \n"
"    SUM( CASE WHEN rank = 3 THEN 1 ELSE 0 END ) AS bronzes                                          \n"
"FROM fastcaps_ranks                                                                                 \n"
"GROUP BY type, account_id                                                                           \n"
"HAVING golds > 0 OR silvers > 0 OR bronzes > 0;                                                       ";

static void V4WriteUnassignedToCsv(void* ctx, const int sessionId, const char* topAlias, const int playtime, const qboolean referenced) {
	fileHandle_t* f = (fileHandle_t*)ctx;

	nicknameEntry_t nicknames[10];

	G_DBGetMostUsedNicknames(sessionId, 10, &nicknames[0]);

	char strings[10][128];
	memset(strings, 0, sizeof(strings));

	int i;
	for (i = 0; i < 10 && VALIDSTRING(nicknames[i].name); ++i) {
		char durationString[64];
		G_FormatDuration(nicknames[i].duration, durationString, sizeof(durationString));

		Com_sprintf(strings[i], sizeof(strings[i]), "\"%s (%s)\"", nicknames[i].name, durationString);
	}

	char* buf = va(
		"%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
		referenced ? 1 : 0,
		sessionId,
		strings[0],
		strings[1],
		strings[2],
		strings[3],
		strings[4],
		strings[5],
		strings[6],
		strings[7],
		strings[8],
		strings[9]
	);
	
	trap_FS_Write(buf, strlen(buf), *f);
}

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
		NormalizeName(name, cleanname, sizeof(cleanname), MAX_NAME_DISPLAYLENGTH);

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
			NormalizeName(player_name, cleanname, sizeof(cleanname), MAX_NAME_DISPLAYLENGTH);

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

	// write a detailed report about the new unassigned sessions for use by admins
	Com_Printf("Writing unassigned_sessions.csv...\n");
	fileHandle_t f;
	trap_FS_FOpenFile("unassigned_sessions.csv", &f, FS_WRITE);
	if (f) {
		pagination_t pagination;
		pagination.numPerPage = INT_MAX;
		pagination.numPage = 1;

		G_DBListTopUnassignedSessionIDs(pagination, V4WriteUnassignedToCsv, &f);
	}
	trap_FS_FCloseFile(f);

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
	}
	
	return qtrue;
}