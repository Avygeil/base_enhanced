#include "g_database.h"
#include "sqlite3.h"
#include "cJSON.h"

// ================ V2 UPGRADE =================================================

const char* const sqlAddSeedField =
"ALTER TABLE fastcapsv2 ADD COLUMN seed TEXT;";

static qboolean UpgradeDBToVersion2(sqlite3* dbPtr) {
	// add "seed" field to fastcaps for use with weekly challenges
	return trap_sqlite3_exec(dbPtr, sqlAddSeedField, NULL, NULL, NULL) == SQLITE_OK;
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
	return trap_sqlite3_exec(dbPtr, v3Upgrade, NULL, NULL, NULL) == SQLITE_OK;
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

static fileHandle_t csvFile;

static void V4WriteUnassignedToCsv(void* ctx, const int sessionId, const char* topAlias, const int playtime, const qboolean referenced) {
	sqlite3* dbPtr = (sqlite3*)ctx;

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

	char cuidString[64] = { 0 };
	sqlite3_stmt* statement = NULL;
	trap_sqlite3_prepare_v2(dbPtr, "SELECT COALESCE(json_extract(info, '$.cuid_hash2'), '') AS cuid FROM sessions WHERE session_id = ?1;", -1, &statement, 0);
	sqlite3_bind_int(statement, 1, sessionId);
	int rc = trap_sqlite3_step(statement);
	if (rc == SQLITE_ROW) {
		const char* cuid = (const char*)sqlite3_column_text(statement, 0);
		if (VALIDSTRING(cuid))
			Q_strncpyz(cuidString, cuid, sizeof(cuidString));
		trap_sqlite3_step(statement);
	}

	char* buf = va(
		"%d,%d,%s,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
		referenced ? 1 : 0,
		sessionId,
		cuidString,
		playtime,
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
	
	trap_FS_Write(buf, strlen(buf), csvFile);
}

extern void NormalizeName(const char* in, char* out, int outSize, int colorlessSize);

static qboolean UpgradeDBToVersion4(sqlite3* dbPtr) {
	if (trap_sqlite3_exec(dbPtr, v4NewTables, NULL, NULL, NULL) != SQLITE_OK)
		return qfalse;

	sqlite3_stmt* statement = NULL;
	sqlite3_stmt* statement2 = NULL;
	sqlite3_stmt* statement3 = NULL;
	int rc;

	// merge nicknames
	Com_Printf("Merging nicknames\n");
	trap_sqlite3_prepare_v2(dbPtr, "SELECT * FROM nicknames;", -1, &statement, 0);
	trap_sqlite3_prepare_v2(dbPtr, "INSERT OR IGNORE INTO nicknames_TMP(ip_int, name, duration, cuid_hash2) VALUES (?1, ?2, ?3, ?4);", -1, &statement2, 0);
	trap_sqlite3_prepare_v2(dbPtr, "UPDATE nicknames_TMP SET duration=duration+?1 WHERE ip_int=?2 AND name=?3 AND cuid_hash2=?4;", -1, &statement3, 0);
	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
		const int ip_int = sqlite3_column_int(statement, 0);
		const char* name = (const char*)sqlite3_column_text(statement, 1);
		const int duration = sqlite3_column_int(statement, 2);
		const char* cuid_hash2 = (const char*)sqlite3_column_text(statement, 3);

		// make sure even very old names are also normalized
		char cleanname[MAX_NETNAME];
		NormalizeName(name, cleanname, sizeof(cleanname), MAX_NAME_DISPLAYLENGTH);

		if (ip_int && VALIDSTRING(name)) {
			trap_sqlite3_reset(statement2);
			sqlite3_bind_int(statement2, 1, ip_int);
			sqlite3_bind_text(statement2, 2, cleanname, -1, 0);
			sqlite3_bind_int(statement2, 3, 0);
			if (VALIDSTRING(cuid_hash2)) {
				sqlite3_bind_text(statement2, 4, cuid_hash2, -1, 0);
			} else {
				sqlite3_bind_text(statement2, 4, "", -1, 0);
			}
			trap_sqlite3_step(statement2);

			trap_sqlite3_reset(statement3);
			sqlite3_bind_int(statement3, 1, duration);
			sqlite3_bind_int(statement3, 2, ip_int);
			sqlite3_bind_text(statement3, 3, cleanname, -1, 0);
			if (VALIDSTRING(cuid_hash2)) {
				sqlite3_bind_text(statement3, 4, cuid_hash2, -1, 0);
			} else {
				sqlite3_bind_text(statement3, 4, "", -1, 0);
			}
			trap_sqlite3_step(statement3);
		}

		rc = trap_sqlite3_step(statement);
	}
	trap_sqlite3_finalize(statement);
	trap_sqlite3_finalize(statement2);
	trap_sqlite3_finalize(statement3);

	// port nicknames by creating sessions for them
	Com_Printf("Port nicknames to sessions\n");
	trap_sqlite3_prepare_v2(dbPtr, "SELECT * FROM nicknames_TMP;", -1, &statement, 0);
	trap_sqlite3_prepare_v2(dbPtr, "INSERT OR IGNORE INTO nicknames_NEW (session_id, name) VALUES (?1, ?2);", -1, &statement2, 0);
	trap_sqlite3_prepare_v2(dbPtr, "UPDATE nicknames_NEW SET duration=duration+?1 WHERE session_id=?2 AND name=?3;", -1, &statement3, 0);
	rc = trap_sqlite3_step(statement);
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

			trap_sqlite3_reset(statement2);
			sqlite3_bind_int(statement2, 1, session.id);
			sqlite3_bind_text(statement2, 2, name, -1, 0);
			trap_sqlite3_step(statement2);

			trap_sqlite3_reset(statement3);
			sqlite3_bind_int(statement3, 1, duration);
			sqlite3_bind_int(statement3, 2, session.id);
			sqlite3_bind_text(statement3, 3, name, -1, 0);
			trap_sqlite3_step(statement3);
		}

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
	trap_sqlite3_finalize(statement2);
	trap_sqlite3_finalize(statement3);

	// port fastcaps to the new format and make new sessions if they weren't created with nicknames
	Com_Printf("Port fastcaps to new format\n");
	trap_sqlite3_prepare_v2(dbPtr, "SELECT * FROM fastcapsV2;", -1, &statement, 0);
	trap_sqlite3_prepare_v2(dbPtr, "INSERT OR IGNORE INTO fastcaps_NEW(mapname, type, session_id, capture_time, date, extra) VALUES(?1, ?2, ?3, ?4, ?5, ?6);", -1, &statement2, 0);
	trap_sqlite3_prepare_v2(dbPtr, "UPDATE fastcaps_NEW SET capture_time=MIN(capture_time, ?1) WHERE mapname=?2 AND type=?3 AND session_id=?4;", -1, &statement3, 0);
	rc = trap_sqlite3_step(statement);
	while (rc == SQLITE_ROW) {
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

			trap_sqlite3_reset(statement2);
			sqlite3_bind_text(statement2, 1, mapname, -1, 0);
			sqlite3_bind_int(statement2, 2, type);
			sqlite3_bind_int(statement2, 3, session.id);
			sqlite3_bind_int(statement2, 4, capture_time);
			sqlite3_bind_int64(statement2, 5, date);
			sqlite3_bind_text(statement2, 6, extra, -1, 0);
			trap_sqlite3_step(statement2);

			trap_sqlite3_reset(statement3);
			sqlite3_bind_int(statement3, 1, capture_time);
			sqlite3_bind_text(statement3, 2, mapname, -1, 0);
			sqlite3_bind_int(statement3, 3, type);
			sqlite3_bind_int(statement3, 4, session.id);
			trap_sqlite3_step(statement3);

			free(extra);
			cJSON_Delete(root2);
		}

		rc = trap_sqlite3_step(statement);
	}

	trap_sqlite3_finalize(statement);
	trap_sqlite3_finalize(statement2);
	trap_sqlite3_finalize(statement3);

	Com_Printf("Finalizing...\n");

	// delete non normal/weapon records
	if (trap_sqlite3_exec(dbPtr, "DELETE from fastcaps_NEW WHERE type > 1;", NULL, NULL, NULL) != SQLITE_OK)
		return qfalse;

	// remove the intermediate and old tables and rename the new ones
	if (trap_sqlite3_exec(dbPtr, v4TerminateMyLife, NULL, NULL, NULL) != SQLITE_OK)
		return qfalse;

	// create the final views
	if (trap_sqlite3_exec(dbPtr, v4CreateNewShit, NULL, NULL, NULL) != SQLITE_OK)
		return qfalse;

	// make sure to reduce the file size
	trap_sqlite3_exec(dbPtr, "VACUUM;", NULL, NULL, NULL);
	trap_sqlite3_exec(dbPtr, "PRAGMA optimize;", NULL, NULL, NULL);

	// write a detailed report about the new unassigned sessions for use by admins
	Com_Printf("Writing unassigned_sessions.csv...\n");
	trap_FS_FOpenFile("unassigned_sessions.csv", &csvFile, FS_WRITE);
	if (csvFile) {
		pagination_t pagination;
		pagination.numPerPage = INT_MAX;
		pagination.numPage = 1;

		G_DBListTopUnassignedSessionIDs(pagination, V4WriteUnassignedToCsv, dbPtr);
	}
	trap_FS_FCloseFile(csvFile);

	return qtrue;
}

const char *const v5Upgrade =
"CREATE TABLE IF NOT EXISTS [tierlistmaps] (                                                         \n"
"	[account_id] INTEGER NOT NULL,                                                                   \n"
"	[map] TEXT COLLATE NOCASE NOT NULL,                                                              \n"
"	[tier] INTEGER NOT NULL,                                                                         \n"
"   PRIMARY KEY(account_id, map)                                                                     \n"
"   FOREIGN KEY(account_id) REFERENCES accounts(account_id) ON DELETE CASCADE);                      \n"
"                                                                                                    \n"
"CREATE TABLE IF NOT EXISTS [tierwhitelist] ( [map] TEXT COLLATE NOCASE NOT NULL PRIMARY KEY );      \n"
"                                                                                                    \n"
"CREATE TABLE IF NOT EXISTS [lastplayedmap] (                                                        \n"
"	[map] TEXT COLLATE NOCASE NOT NULL PRIMARY KEY,                                                  \n"
"   [num] INTEGER NOT NULL DEFAULT 1,                                                                \n"
"	[datetime] NOT NULL DEFAULT (strftime('%s', 'now')));                                            \n";

static qboolean UpgradeDBToVersion5(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v5Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v6Upgrade =
"CREATE VIEW IF NOT EXISTS num_played_view AS                                                        \n"
"WITH t AS (SELECT map, AVG(tier) avgTier FROM tierlistmaps GROUP BY tierlistmaps.map)               \n"
"SELECT i.map, COALESCE(SUM(s.num), 0) numPlayed, avgTier                                            \n"
"FROM tierwhitelist i                                                                                \n"
"LEFT JOIN lastplayedmap s on s.map = i.map                                                          \n"
"LEFT JOIN t ON t.map = i.map                                                                        \n"
"GROUP BY i.map;                                                                                     \n";

static qboolean UpgradeDBToVersion6(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v6Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v7Upgrade =
"CREATE TABLE IF NOT EXISTS [topaims] (                                                              \n"
"    [topaim_id] INTEGER NOT NULL,                                                                   \n"
"    [hash] INTEGER NOT NULL, \n"
"    [mapname] TEXT COLLATE NOCASE NOT NULL,                                                         \n"
"[packname] TEXT COLLATE NOCASE NOT NULL,\n"
"    [session_id] INTEGER NOT NULL,                                                                  \n"
"    [capture_time] INTEGER NOT NULL,                                                                \n"
"    [date] NOT NULL DEFAULT (strftime('%s', 'now')),                                                \n"
"    [extra] TEXT DEFAULT NULL,                                                                      \n"
"    PRIMARY KEY ( [topaim_id] ),                                                                    \n"
"    UNIQUE ( [mapname], [packname], [session_id] ),                                                     \n"
"FOREIGN KEY ([hash]) REFERENCES aimpacks ([hash]) ON DELETE CASCADE,\n"
"    FOREIGN KEY ( [session_id] )                                                                    \n"
"        REFERENCES sessions ( [session_id] )                                                        \n"
");                                                                                                  \n"
"                                                                                                    \n"
"CREATE TABLE IF NOT EXISTS[aimpacks] (\n"
"[hash] INTEGER PRIMARY KEY,\n"
"[name] TEXT COLLATE NOCASE NOT NULL,\n"
"[mapname] TEXT COLLATE NOCASE NOT NULL,\n"
"[owner_account_id] INTEGER NOT NULL,\n"
"[dateCreated] NOT NULL DEFAULT (strftime('%s', 'now')),\n"
"[weaponMode] INTEGER NOT NULL,\n"
"[numVariants] INTEGER NOT NULL,\n"
"[data] BLOB NOT NULL,\n"
"[numRepsPerVariant] INTEGER NOT NULL,\n"
"UNIQUE ([name], [mapname]),\n"
"FOREIGN KEY ([owner_account_id]) REFERENCES accounts ([account_id])"
");\n"
"                                                                                                    \n"
"CREATE VIEW IF NOT EXISTS topaims_ranks AS                                                          \n"
"SELECT                                                                                              \n"
"    topaims.topaim_id,                                                                              \n"
"    topaims.mapname,                                                                                \n"
"    topaims.packname,                                                                                   \n"
"    player_aliases.account_id,                                                                      \n"
"    player_aliases.alias AS name,                                                                   \n"
"    RANK() OVER (                                                                                   \n"
"	     PARTITION BY topaims.mapname, topaims.packname                                                  \n"
"	     ORDER BY topaims.capture_time DESC, topaims.date ASC                                        \n"
"    )                                                                                               \n"
"    rank,                                                                                           \n"
"    MAX(topaims.capture_time) AS capture_time,                                                      \n"
"    topaims.date,                                                                                   \n"
"    topaims.extra                                                                                   \n"
"FROM topaims, player_aliases                                                                        \n"
"WHERE topaims.session_id = player_aliases.session_id AND player_aliases.has_account                 \n"
"GROUP BY topaims.mapname, topaims.packname, player_aliases.account_id;                                  \n"
"                                                                                                    \n"
// Aggregates golds, silvers and bronzes for all accounts for each type
"CREATE VIEW IF NOT EXISTS topaims_leaderboard AS                                                    \n"
"SELECT                                                                                              \n"
"    account_id,                                                                                     \n"
"    name,                                                                                           \n"
"    packname,                                                                                           \n"
"    SUM( CASE WHEN rank = 1 THEN 1 ELSE 0 END ) AS golds,                                           \n"
"    SUM( CASE WHEN rank = 2 THEN 1 ELSE 0 END ) AS silvers,                                         \n"
"    SUM( CASE WHEN rank = 3 THEN 1 ELSE 0 END ) AS bronzes                                          \n"
"FROM topaims_ranks                                                                                  \n"
"GROUP BY packname, account_id                                                                           \n"
"HAVING golds > 0 OR silvers > 0 OR bronzes > 0;                                                      ";

static qboolean UpgradeDBToVersion7(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v7Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v8Upgrade =
"CREATE TABLE IF NOT EXISTS [pugs] ( "
"[match_id] INTEGER PRIMARY KEY, "
"[datetime] NOT NULL DEFAULT (strftime('%s', 'now')), "
"[map] TEXT COLLATE NOCASE NOT NULL, "
"[duration] INTEGER NOT NULL, "
"[boonexists] INTEGER NOT NULL, "
"[win_team] INTEGER NOT NULL, "
"[red_score] INTEGER NOT NULL, "
"[blue_score] INTEGER NOT NULL "
"); "
" "
"CREATE TABLE IF NOT EXISTS [playerpugteampos] ( "
"[playerpugteampos_id] INTEGER PRIMARY KEY AUTOINCREMENT, "
"[match_id] INTEGER NOT NULL, "
"[session_id] INTEGER NOT NULL, "
"[team] INTEGER NOT NULL, "
"[duration] INTEGER NOT NULL, "
"[name] TEXT NOT NULL, "
"[pos] INTEGER, "
"[cap] INTEGER, "
"[ass] INTEGER, "
"[def] INTEGER, "
"[acc] INTEGER, "
"[air] INTEGER, "
"[tk] INTEGER, "
"[take] INTEGER, "
"[pitkil] INTEGER, "
"[pitdth] INTEGER, "
"[dmg] INTEGER, "
"[fcdmg] INTEGER, "
"[clrdmg] INTEGER, "
"[othrdmg] INTEGER, "
"[dmgtkn] INTEGER, "
"[fcdmgtkn] INTEGER, "
"[clrdmgtkn] INTEGER, "
"[othrdmgtkn] INTEGER, "
"[fckil] INTEGER, "
"[fckileff] INTEGER, "
"[ret] INTEGER, "
"[sk] INTEGER, "
"[ttlhold] INTEGER, "
"[maxhold] INTEGER, "
"[avgspd] INTEGER, "
"[topspd] INTEGER, "
"[boon] INTEGER, "
"[push] INTEGER, "
"[pull] INTEGER, "
"[heal] INTEGER, "
"[te] INTEGER, "
"[teeff] INTEGER, "
"[enemynrg] INTEGER, "
"[absorb] INTEGER, "
"[protdmg] INTEGER, "
"[prottime] INTEGER, "
"[rage] INTEGER, "
"[drain] INTEGER, "
"[drained] INTEGER, "
"[fs] INTEGER, "
"[bas] INTEGER, "
"[mid] INTEGER, "
"[eba] INTEGER, "
"[efs] INTEGER, "
"FOREIGN KEY([match_id]) REFERENCES pugs([match_id]) ON DELETE CASCADE, "
"FOREIGN KEY([session_id]) REFERENCES sessions([session_id]), "
"UNIQUE([match_id], [session_id], [pos]) "
"); "
""
"CREATE VIEW [accountstats] AS "
"WITH wins AS ( "
"SELECT playerpugteampos.session_id, playerpugteampos.pos, count() AS wins "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"WHERE pugs.win_team = playerpugteampos.team "
"GROUP BY account_id, playerpugteampos.pos), "
"pugs_played AS ( "
"SELECT playerpugteampos.session_id, playerpugteampos.pos, count(*) AS pugs_played "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"GROUP BY account_id, playerpugteampos.pos), "
"t AS ( "
"SELECT accounts.account_id, accounts.name, playerpugteampos.pos, "
"pugs_played, "
"ifnull(wins, 0) AS wins, "
"CAST(ifnull(wins, 0) AS FLOAT) / pugs_played AS winrate, "
"avg(cap * (1200000.0 / playerpugteampos.duration)) AS avg_cap, "
"avg(ass * (1200000.0 / playerpugteampos.duration)) AS avg_ass, "
"avg(def * (1200000.0 / playerpugteampos.duration)) AS avg_def, "
"CAST(sum((CASE WHEN acc IS NOT NULL THEN acc ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN acc IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_acc, "
"avg(air * (1200000.0 / playerpugteampos.duration)) AS avg_air, "
"avg(tk * (1200000.0 / playerpugteampos.duration)) AS avg_tk, "
"avg(take * (1200000.0 / playerpugteampos.duration)) AS avg_take, "
"avg(pitkil * (1200000.0 / playerpugteampos.duration)) AS avg_pitkil, "
"avg(pitdth * (1200000.0 / playerpugteampos.duration)) AS avg_pitdth, "
"avg(dmg * (1200000.0 / playerpugteampos.duration)) AS avg_dmg, "
"avg(fcdmg * (1200000.0 / playerpugteampos.duration)) AS avg_fcdmg, "
"avg(clrdmg * (1200000.0 / playerpugteampos.duration)) AS avg_clrdmg, "
"avg(othrdmg * (1200000.0 / playerpugteampos.duration)) AS avg_othrdmg, "
"avg(dmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_dmgtkn, "
"avg(fcdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_fcdmgtkn, "
"avg(clrdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_clrdmgtkn, "
"avg(othrdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_othrdmgtkn, "
"avg(fckil * (1200000.0 / playerpugteampos.duration)) AS avg_fckil, "
"CAST(sum((CASE WHEN fckileff IS NOT NULL THEN fckileff ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN fckileff IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_fckileff, "
"avg(ret * (1200000.0 / playerpugteampos.duration)) AS avg_ret, "
"avg(sk * (1200000.0 / playerpugteampos.duration)) AS avg_sk, "
"avg(ttlhold * (1200000.0 / playerpugteampos.duration)) AS avg_ttlhold, "
"avg(maxhold * (1200000.0 / playerpugteampos.duration)) AS avg_maxhold, "
"CAST(sum(avgspd * playerpugteampos.duration) AS FLOAT) / CAST(sum(playerpugteampos.duration) AS FLOAT) AS avg_avgspd, "
"CAST(sum(topspd * playerpugteampos.duration) AS FLOAT) / CAST(sum(playerpugteampos.duration) AS FLOAT) AS avg_topspd, "
"avg(boon * (1200000.0 / playerpugteampos.duration)) AS avg_boon, "
"avg(push * (1200000.0 / playerpugteampos.duration)) AS avg_push, "
"avg(pull * (1200000.0 / playerpugteampos.duration)) AS avg_pull, "
"avg(heal * (1200000.0 / playerpugteampos.duration)) AS avg_heal, "
"avg(te * (1200000.0 / playerpugteampos.duration)) AS avg_te, "
"CAST(sum((CASE WHEN teeff IS NOT NULL THEN teeff ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN teeff IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_teeff, "
"avg(enemynrg * (1200000.0 / playerpugteampos.duration)) AS avg_enemynrg, "
"avg(absorb * (1200000.0 / playerpugteampos.duration)) AS avg_absorb, "
"avg(protdmg * (1200000.0 / playerpugteampos.duration)) AS avg_protdmg, "
"avg(prottime * (1200000.0 / playerpugteampos.duration)) AS avg_prottime, "
"avg(rage * (1200000.0 / playerpugteampos.duration)) AS avg_rage, "
"avg(drain * (1200000.0 / playerpugteampos.duration)) AS avg_drain, "
"avg(drained * (1200000.0 / playerpugteampos.duration)) AS avg_drained, "
"CAST(sum((CASE WHEN fs IS NOT NULL THEN fs ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN fs IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_fs, "
"CAST(sum((CASE WHEN bas IS NOT NULL THEN bas ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN bas IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_bas, "
"CAST(sum((CASE WHEN mid IS NOT NULL THEN mid ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN mid IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_mid, "
"CAST(sum((CASE WHEN eba IS NOT NULL THEN eba ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN eba IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_eba, "
"CAST(sum((CASE WHEN efs IS NOT NULL THEN efs ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN efs IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_efs "
"FROM playerpugteampos "
"JOIN sessions ON playerpugteampos.session_id = sessions.session_id "
"JOIN accounts ON accounts.account_id = sessions.account_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"LEFT JOIN wins ON wins.session_id = sessions.session_id AND wins.pos = playerpugteampos.pos "
"JOIN pugs_played ON pugs_played.session_id = sessions.session_id AND pugs_played.pos = playerpugteampos.pos "
"GROUP BY playerpugteampos.pos, accounts.account_id "
"ORDER BY playerpugteampos.pos ASC, accounts.account_id ASC) "
"SELECT account_id, "
"name, "
"pos, "
"pugs_played, "
"RANK() OVER (PARTITION BY pos ORDER BY pugs_played DESC) pugs_played_rank, "
"wins, "
"RANK() OVER (PARTITION BY pos ORDER BY wins DESC) wins_rank, "
"winrate, "
"RANK() OVER (PARTITION BY pos ORDER BY winrate DESC) winrate_rank, "
"avg_cap, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_cap DESC) avg_cap_rank, "
"avg_ass, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ass DESC) avg_ass_rank, "
"avg_def, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_def DESC) avg_def_rank, "
"avg_acc, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_acc DESC) avg_acc_rank, "
"avg_air, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_air DESC) avg_air_rank, "
"avg_tk, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_tk DESC) avg_tk_rank, "
"avg_take, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_take DESC) avg_take_rank, "
"avg_pitkil, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pitkil DESC) avg_pitkil_rank, "
"avg_pitdth, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pitdth DESC) avg_pitdth_rank, "
"avg_dmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dmg DESC) avg_dmg_rank, "
"avg_fcdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fcdmg DESC) avg_fcdmg_rank, "
"avg_clrdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_clrdmg DESC) avg_clrdmg_rank, "
"avg_othrdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_othrdmg DESC) avg_othrdmg_rank, "
"avg_dmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dmgtkn DESC) avg_dmgtkn_rank, "
"avg_fcdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fcdmgtkn DESC) avg_fcdmgtkn_rank, "
"avg_clrdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_clrdmgtkn DESC) avg_clrdmgtkn_rank, "
"avg_othrdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_othrdmgtkn DESC) avg_othrdmgtkn_rank, "
"avg_fckil, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fckil DESC) avg_fckil_rank, "
"avg_fckileff, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fckileff DESC) avg_fckileff_rank, "
"avg_ret, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ret DESC) avg_ret_rank, "
"avg_sk, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_sk DESC) avg_sk_rank, "
"avg_ttlhold, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ttlhold DESC) avg_ttlhold_rank, "
"avg_maxhold, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_maxhold DESC) avg_maxhold_rank, "
"avg_avgspd, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_avgspd DESC) avg_avgspd_rank, "
"avg_topspd, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_topspd DESC) avg_topspd_rank, "
"avg_boon, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_boon DESC) avg_boon_rank, "
"avg_push, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_push DESC) avg_push_rank, "
"avg_pull, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pull DESC) avg_pull_rank, "
"avg_heal, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_heal DESC) avg_heal_rank, "
"avg_te, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_te DESC) avg_te_rank, "
"avg_teeff, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_teeff DESC) avg_teeff_rank, "
"avg_enemynrg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_enemynrg DESC) avg_enemynrg_rank, "
"avg_absorb, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_absorb DESC) avg_absorb_rank, "
"avg_protdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_protdmg DESC) avg_protdmg_rank, "
"avg_prottime, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_prottime DESC) avg_prottime_rank, "
"avg_rage, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_rage DESC) avg_rage_rank, "
"avg_drain, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_drain DESC) avg_drain_rank, "
"avg_drained, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_drained DESC) avg_drained_rank, "
"avg_fs, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fs DESC) avg_fs_rank, "
"avg_bas, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_bas DESC) avg_bas_rank, "
"avg_mid, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_mid DESC) avg_mid_rank, "
"avg_eba, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_eba DESC) avg_eba_rank, "
"avg_efs, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_efs DESC) avg_efs_rank "
"FROM t WHERE pugs_played >= 10;";

static qboolean UpgradeDBToVersion8(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v8Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v9Upgrade = "CREATE TABLE IF NOT EXISTS [cachedplayerstats] ( "
"[cachedplayerstats_id] INTEGER PRIMARY KEY AUTOINCREMENT, "
"[account_id] INTEGER NOT NULL, "
"[type] INTEGER, "
"[pos] INTEGER, "
"[str] TEXT, "
"FOREIGN KEY([account_id]) REFERENCES accounts([account_id]) ON DELETE CASCADE, "
"UNIQUE(account_id, type, pos) "
");";

static qboolean UpgradeDBToVersion9(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v9Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v10Upgrade = "CREATE TABLE IF NOT EXISTS [playerratings] ( "
"[rating_id] INTEGER PRIMARY KEY AUTOINCREMENT, "
"[rater_account_id] INTEGER NOT NULL, "
"[ratee_account_id] INTEGER NOT NULL, "
"[pos] INTEGER NOT NULL, "
"[rating] INTEGER NOT NULL, "
"FOREIGN KEY([rater_account_id]) REFERENCES accounts([account_id]) ON DELETE CASCADE, "
"FOREIGN KEY([ratee_account_id]) REFERENCES accounts([account_id]) ON DELETE CASCADE, "
"UNIQUE([rater_account_id], [ratee_account_id], [pos]) "
");";

static qboolean UpgradeDBToVersion10(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v10Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v11Upgrade = "ALTER TABLE accounts RENAME COLUMN usergroup TO properties;";

static qboolean UpgradeDBToVersion11(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v11Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v12Upgrade =
"DROP VIEW IF EXISTS [accountstats]; "
""
"CREATE VIEW [accountstats] AS "
"WITH wins AS ( "
"SELECT playerpugteampos.session_id, playerpugteampos.pos, count() AS wins "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"WHERE pugs.win_team = playerpugteampos.team "
"GROUP BY account_id, playerpugteampos.pos), "
"pugs_played AS ( "
"SELECT playerpugteampos.session_id, playerpugteampos.pos, count(*) AS pugs_played "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"GROUP BY account_id, playerpugteampos.pos), "
"t AS ( "
"SELECT accounts.account_id, accounts.name, playerpugteampos.pos, "
"pugs_played, "
"ifnull(wins, 0) AS wins, "
"CAST(ifnull(wins, 0) AS FLOAT) / pugs_played AS winrate, "
"avg(cap * (1200000.0 / playerpugteampos.duration)) AS avg_cap, "
"avg(ass * (1200000.0 / playerpugteampos.duration)) AS avg_ass, "
"avg(def * (1200000.0 / playerpugteampos.duration)) AS avg_def, "
"CAST(sum((CASE WHEN acc IS NOT NULL THEN acc ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN acc IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_acc, "
"avg(air * (1200000.0 / playerpugteampos.duration)) AS avg_air, "
"avg(tk * (1200000.0 / playerpugteampos.duration)) AS avg_tk, "
"avg(take * (1200000.0 / playerpugteampos.duration)) AS avg_take, "
"avg(pitkil * (1200000.0 / playerpugteampos.duration)) AS avg_pitkil, "
"avg(pitdth * (1200000.0 / playerpugteampos.duration)) AS avg_pitdth, "
"avg(dmg * (1200000.0 / playerpugteampos.duration)) AS avg_dmg, "
"avg(fcdmg * (1200000.0 / playerpugteampos.duration)) AS avg_fcdmg, "
"avg(clrdmg * (1200000.0 / playerpugteampos.duration)) AS avg_clrdmg, "
"avg(othrdmg * (1200000.0 / playerpugteampos.duration)) AS avg_othrdmg, "
"avg(dmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_dmgtkn, "
"avg(fcdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_fcdmgtkn, "
"avg(clrdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_clrdmgtkn, "
"avg(othrdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_othrdmgtkn, "
"avg(fckil * (1200000.0 / playerpugteampos.duration)) AS avg_fckil, "
"CAST(sum((CASE WHEN fckileff IS NOT NULL THEN fckileff ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN fckileff IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_fckileff, "
"avg(ret * (1200000.0 / playerpugteampos.duration)) AS avg_ret, "
"avg(sk * (1200000.0 / playerpugteampos.duration)) AS avg_sk, "
"avg(ttlhold * (1200000.0 / playerpugteampos.duration)) AS avg_ttlhold, "
"avg(maxhold * (1200000.0 / playerpugteampos.duration)) AS avg_maxhold, "
"CAST(sum(avgspd * playerpugteampos.duration) AS FLOAT) / CAST(sum(playerpugteampos.duration) AS FLOAT) AS avg_avgspd, "
"CAST(sum(topspd * playerpugteampos.duration) AS FLOAT) / CAST(sum(playerpugteampos.duration) AS FLOAT) AS avg_topspd, "
"avg(boon * (1200000.0 / playerpugteampos.duration)) AS avg_boon, "
"avg(push * (1200000.0 / playerpugteampos.duration)) AS avg_push, "
"avg(pull * (1200000.0 / playerpugteampos.duration)) AS avg_pull, "
"avg(heal * (1200000.0 / playerpugteampos.duration)) AS avg_heal, "
"avg(te * (1200000.0 / playerpugteampos.duration)) AS avg_te, "
"CAST(sum((CASE WHEN teeff IS NOT NULL THEN teeff ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN teeff IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_teeff, "
"avg(enemynrg * (1200000.0 / playerpugteampos.duration)) AS avg_enemynrg, "
"avg(absorb * (1200000.0 / playerpugteampos.duration)) AS avg_absorb, "
"avg(protdmg * (1200000.0 / playerpugteampos.duration)) AS avg_protdmg, "
"avg(prottime * (1200000.0 / playerpugteampos.duration)) AS avg_prottime, "
"avg(rage * (1200000.0 / playerpugteampos.duration)) AS avg_rage, "
"avg(drain * (1200000.0 / playerpugteampos.duration)) AS avg_drain, "
"avg(drained * (1200000.0 / playerpugteampos.duration)) AS avg_drained, "
"CAST(sum((CASE WHEN fs IS NOT NULL THEN fs ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN fs IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_fs, "
"CAST(sum((CASE WHEN bas IS NOT NULL THEN bas ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN bas IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_bas, "
"CAST(sum((CASE WHEN mid IS NOT NULL THEN mid ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN mid IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_mid, "
"CAST(sum((CASE WHEN eba IS NOT NULL THEN eba ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN eba IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_eba, "
"CAST(sum((CASE WHEN efs IS NOT NULL THEN efs ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN efs IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_efs "
"FROM playerpugteampos "
"JOIN sessions ON playerpugteampos.session_id = sessions.session_id "
"JOIN accounts ON accounts.account_id = sessions.account_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"LEFT JOIN wins ON wins.session_id = sessions.session_id AND wins.pos = playerpugteampos.pos "
"LEFT JOIN pugs_played ON pugs_played.session_id = sessions.session_id AND pugs_played.pos = playerpugteampos.pos "
"GROUP BY playerpugteampos.pos, accounts.account_id "
"ORDER BY playerpugteampos.pos ASC, accounts.account_id ASC) "
"SELECT account_id, "
"name, "
"pos, "
"pugs_played, "
"RANK() OVER (PARTITION BY pos ORDER BY pugs_played DESC) pugs_played_rank, "
"wins, "
"RANK() OVER (PARTITION BY pos ORDER BY wins DESC) wins_rank, "
"winrate, "
"RANK() OVER (PARTITION BY pos ORDER BY winrate DESC) winrate_rank, "
"avg_cap, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_cap DESC) avg_cap_rank, "
"avg_ass, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ass DESC) avg_ass_rank, "
"avg_def, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_def DESC) avg_def_rank, "
"avg_acc, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_acc DESC) avg_acc_rank, "
"avg_air, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_air DESC) avg_air_rank, "
"avg_tk, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_tk DESC) avg_tk_rank, "
"avg_take, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_take DESC) avg_take_rank, "
"avg_pitkil, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pitkil DESC) avg_pitkil_rank, "
"avg_pitdth, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pitdth DESC) avg_pitdth_rank, "
"avg_dmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dmg DESC) avg_dmg_rank, "
"avg_fcdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fcdmg DESC) avg_fcdmg_rank, "
"avg_clrdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_clrdmg DESC) avg_clrdmg_rank, "
"avg_othrdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_othrdmg DESC) avg_othrdmg_rank, "
"avg_dmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dmgtkn DESC) avg_dmgtkn_rank, "
"avg_fcdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fcdmgtkn DESC) avg_fcdmgtkn_rank, "
"avg_clrdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_clrdmgtkn DESC) avg_clrdmgtkn_rank, "
"avg_othrdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_othrdmgtkn DESC) avg_othrdmgtkn_rank, "
"avg_fckil, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fckil DESC) avg_fckil_rank, "
"avg_fckileff, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fckileff DESC) avg_fckileff_rank, "
"avg_ret, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ret DESC) avg_ret_rank, "
"avg_sk, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_sk DESC) avg_sk_rank, "
"avg_ttlhold, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ttlhold DESC) avg_ttlhold_rank, "
"avg_maxhold, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_maxhold DESC) avg_maxhold_rank, "
"avg_avgspd, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_avgspd DESC) avg_avgspd_rank, "
"avg_topspd, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_topspd DESC) avg_topspd_rank, "
"avg_boon, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_boon DESC) avg_boon_rank, "
"avg_push, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_push DESC) avg_push_rank, "
"avg_pull, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pull DESC) avg_pull_rank, "
"avg_heal, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_heal DESC) avg_heal_rank, "
"avg_te, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_te DESC) avg_te_rank, "
"avg_teeff, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_teeff DESC) avg_teeff_rank, "
"avg_enemynrg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_enemynrg DESC) avg_enemynrg_rank, "
"avg_absorb, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_absorb DESC) avg_absorb_rank, "
"avg_protdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_protdmg DESC) avg_protdmg_rank, "
"avg_prottime, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_prottime DESC) avg_prottime_rank, "
"avg_rage, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_rage DESC) avg_rage_rank, "
"avg_drain, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_drain DESC) avg_drain_rank, "
"avg_drained, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_drained DESC) avg_drained_rank, "
"avg_fs, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fs DESC) avg_fs_rank, "
"avg_bas, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_bas DESC) avg_bas_rank, "
"avg_mid, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_mid DESC) avg_mid_rank, "
"avg_eba, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_eba DESC) avg_eba_rank, "
"avg_efs, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_efs DESC) avg_efs_rank "
"FROM t WHERE pugs_played >= 10;";

static qboolean UpgradeDBToVersion12(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v12Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v13Upgrade =
"DROP VIEW IF EXISTS[accountstats]; "
""
"CREATE VIEW [accountstats] AS "
"WITH wins AS ( "
"SELECT account_id, playerpugteampos.pos, count() AS wins "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"WHERE pugs.win_team = playerpugteampos.team "
"GROUP BY account_id, playerpugteampos.pos), "
"pugs_played AS ( "
"SELECT account_id, playerpugteampos.pos, count(*) AS pugs_played "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"GROUP BY account_id, playerpugteampos.pos), "
"t AS ( "
"SELECT accounts.account_id, accounts.name, playerpugteampos.pos, "
"pugs_played, "
"ifnull(wins, 0) AS wins, "
"CAST(ifnull(wins, 0) AS FLOAT) / pugs_played AS winrate, "
"avg(cap * (1200000.0 / playerpugteampos.duration)) AS avg_cap, "
"avg(ass * (1200000.0 / playerpugteampos.duration)) AS avg_ass, "
"avg(def * (1200000.0 / playerpugteampos.duration)) AS avg_def, "
"CAST(sum((CASE WHEN acc IS NOT NULL THEN acc ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN acc IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_acc, "
"avg(air * (1200000.0 / playerpugteampos.duration)) AS avg_air, "
"avg(tk * (1200000.0 / playerpugteampos.duration)) AS avg_tk, "
"avg(take * (1200000.0 / playerpugteampos.duration)) AS avg_take, "
"avg(pitkil * (1200000.0 / playerpugteampos.duration)) AS avg_pitkil, "
"avg(pitdth * (1200000.0 / playerpugteampos.duration)) AS avg_pitdth, "
"avg(dmg * (1200000.0 / playerpugteampos.duration)) AS avg_dmg, "
"avg(fcdmg * (1200000.0 / playerpugteampos.duration)) AS avg_fcdmg, "
"avg(clrdmg * (1200000.0 / playerpugteampos.duration)) AS avg_clrdmg, "
"avg(othrdmg * (1200000.0 / playerpugteampos.duration)) AS avg_othrdmg, "
"avg(dmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_dmgtkn, "
"avg(fcdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_fcdmgtkn, "
"avg(clrdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_clrdmgtkn, "
"avg(othrdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_othrdmgtkn, "
"avg(fckil * (1200000.0 / playerpugteampos.duration)) AS avg_fckil, "
"CAST(sum((CASE WHEN fckileff IS NOT NULL THEN fckileff ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN fckileff IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_fckileff, "
"avg(ret * (1200000.0 / playerpugteampos.duration)) AS avg_ret, "
"avg(sk * (1200000.0 / playerpugteampos.duration)) AS avg_sk, "
"avg(ttlhold * (1200000.0 / playerpugteampos.duration)) AS avg_ttlhold, "
"avg(maxhold * (1200000.0 / playerpugteampos.duration)) AS avg_maxhold, "
"CAST(sum(avgspd * playerpugteampos.duration) AS FLOAT) / CAST(sum(playerpugteampos.duration) AS FLOAT) AS avg_avgspd, "
"CAST(sum(topspd * playerpugteampos.duration) AS FLOAT) / CAST(sum(playerpugteampos.duration) AS FLOAT) AS avg_topspd, "
"avg(boon * (1200000.0 / playerpugteampos.duration)) AS avg_boon, "
"avg(push * (1200000.0 / playerpugteampos.duration)) AS avg_push, "
"avg(pull * (1200000.0 / playerpugteampos.duration)) AS avg_pull, "
"avg(heal * (1200000.0 / playerpugteampos.duration)) AS avg_heal, "
"avg(te * (1200000.0 / playerpugteampos.duration)) AS avg_te, "
"CAST(sum((CASE WHEN teeff IS NOT NULL THEN teeff ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN teeff IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_teeff, "
"avg(enemynrg * (1200000.0 / playerpugteampos.duration)) AS avg_enemynrg, "
"avg(absorb * (1200000.0 / playerpugteampos.duration)) AS avg_absorb, "
"avg(protdmg * (1200000.0 / playerpugteampos.duration)) AS avg_protdmg, "
"avg(prottime * (1200000.0 / playerpugteampos.duration)) AS avg_prottime, "
"avg(rage * (1200000.0 / playerpugteampos.duration)) AS avg_rage, "
"avg(drain * (1200000.0 / playerpugteampos.duration)) AS avg_drain, "
"avg(drained * (1200000.0 / playerpugteampos.duration)) AS avg_drained, "
"CAST(sum((CASE WHEN fs IS NOT NULL THEN fs ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN fs IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_fs, "
"CAST(sum((CASE WHEN bas IS NOT NULL THEN bas ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN bas IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_bas, "
"CAST(sum((CASE WHEN mid IS NOT NULL THEN mid ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN mid IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_mid, "
"CAST(sum((CASE WHEN eba IS NOT NULL THEN eba ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN eba IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_eba, "
"CAST(sum((CASE WHEN efs IS NOT NULL THEN efs ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN efs IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_efs "
"FROM playerpugteampos "
"JOIN sessions ON playerpugteampos.session_id = sessions.session_id "
"JOIN accounts ON accounts.account_id = sessions.account_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"LEFT JOIN wins ON wins.account_id = sessions.account_id AND wins.pos = playerpugteampos.pos "
"LEFT JOIN pugs_played ON pugs_played.account_id = sessions.account_id AND pugs_played.pos = playerpugteampos.pos "
"GROUP BY playerpugteampos.pos, accounts.account_id "
"ORDER BY playerpugteampos.pos ASC, accounts.account_id ASC) "
"SELECT account_id, "
"name, "
"pos, "
"pugs_played, "
"RANK() OVER (PARTITION BY pos ORDER BY pugs_played DESC) pugs_played_rank, "
"wins, "
"RANK() OVER (PARTITION BY pos ORDER BY wins DESC) wins_rank, "
"winrate, "
"RANK() OVER (PARTITION BY pos ORDER BY winrate DESC) winrate_rank, "
"avg_cap, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_cap DESC) avg_cap_rank, "
"avg_ass, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ass DESC) avg_ass_rank, "
"avg_def, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_def DESC) avg_def_rank, "
"avg_acc, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_acc DESC) avg_acc_rank, "
"avg_air, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_air DESC) avg_air_rank, "
"avg_tk, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_tk DESC) avg_tk_rank, "
"avg_take, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_take DESC) avg_take_rank, "
"avg_pitkil, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pitkil DESC) avg_pitkil_rank, "
"avg_pitdth, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pitdth DESC) avg_pitdth_rank, "
"avg_dmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dmg DESC) avg_dmg_rank, "
"avg_fcdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fcdmg DESC) avg_fcdmg_rank, "
"avg_clrdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_clrdmg DESC) avg_clrdmg_rank, "
"avg_othrdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_othrdmg DESC) avg_othrdmg_rank, "
"avg_dmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dmgtkn DESC) avg_dmgtkn_rank, "
"avg_fcdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fcdmgtkn DESC) avg_fcdmgtkn_rank, "
"avg_clrdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_clrdmgtkn DESC) avg_clrdmgtkn_rank, "
"avg_othrdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_othrdmgtkn DESC) avg_othrdmgtkn_rank, "
"avg_fckil, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fckil DESC) avg_fckil_rank, "
"avg_fckileff, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fckileff DESC) avg_fckileff_rank, "
"avg_ret, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ret DESC) avg_ret_rank, "
"avg_sk, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_sk DESC) avg_sk_rank, "
"avg_ttlhold, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ttlhold DESC) avg_ttlhold_rank, "
"avg_maxhold, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_maxhold DESC) avg_maxhold_rank, "
"avg_avgspd, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_avgspd DESC) avg_avgspd_rank, "
"avg_topspd, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_topspd DESC) avg_topspd_rank, "
"avg_boon, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_boon DESC) avg_boon_rank, "
"avg_push, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_push DESC) avg_push_rank, "
"avg_pull, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pull DESC) avg_pull_rank, "
"avg_heal, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_heal DESC) avg_heal_rank, "
"avg_te, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_te DESC) avg_te_rank, "
"avg_teeff, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_teeff DESC) avg_teeff_rank, "
"avg_enemynrg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_enemynrg DESC) avg_enemynrg_rank, "
"avg_absorb, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_absorb DESC) avg_absorb_rank, "
"avg_protdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_protdmg DESC) avg_protdmg_rank, "
"avg_prottime, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_prottime DESC) avg_prottime_rank, "
"avg_rage, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_rage DESC) avg_rage_rank, "
"avg_drain, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_drain DESC) avg_drain_rank, "
"avg_drained, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_drained DESC) avg_drained_rank, "
"avg_fs, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fs DESC) avg_fs_rank, "
"avg_bas, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_bas DESC) avg_bas_rank, "
"avg_mid, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_mid DESC) avg_mid_rank, "
"avg_eba, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_eba DESC) avg_eba_rank, "
"avg_efs, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_efs DESC) avg_efs_rank "
"FROM t WHERE pugs_played >= 10;";

static qboolean UpgradeDBToVersion13(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v13Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v14Upgrade = "ALTER TABLE aimpacks ADD COLUMN [extra] TEXT DEFAULT NULL;";

static qboolean UpgradeDBToVersion14(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v14Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v15Upgrade = "ALTER TABLE playerpugteampos ADD COLUMN [gotte] INTEGER; "
"DROP VIEW [accountstats]; "
"CREATE VIEW [accountstats] AS "
"WITH wins AS ( "
"SELECT account_id, playerpugteampos.pos, count() AS wins "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"WHERE pugs.win_team = playerpugteampos.team "
"GROUP BY account_id, playerpugteampos.pos), "
"pugs_played AS ( "
"SELECT account_id, playerpugteampos.pos, count(*) AS pugs_played "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"GROUP BY account_id, playerpugteampos.pos), "
"t AS ( "
"SELECT accounts.account_id, accounts.name, playerpugteampos.pos, "
"pugs_played, "
"ifnull(wins, 0) AS wins, "
"CAST(ifnull(wins, 0) AS FLOAT) / pugs_played AS winrate, "
"avg(cap * (1200000.0 / playerpugteampos.duration)) AS avg_cap, "
"avg(ass * (1200000.0 / playerpugteampos.duration)) AS avg_ass, "
"avg(def * (1200000.0 / playerpugteampos.duration)) AS avg_def, "
"CAST(sum((CASE WHEN acc IS NOT NULL THEN acc ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN acc IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_acc, "
"avg(air * (1200000.0 / playerpugteampos.duration)) AS avg_air, "
"avg(tk * (1200000.0 / playerpugteampos.duration)) AS avg_tk, "
"avg(take * (1200000.0 / playerpugteampos.duration)) AS avg_take, "
"avg(pitkil * (1200000.0 / playerpugteampos.duration)) AS avg_pitkil, "
"avg(pitdth * (1200000.0 / playerpugteampos.duration)) AS avg_pitdth, "
"avg(dmg * (1200000.0 / playerpugteampos.duration)) AS avg_dmg, "
"avg(fcdmg * (1200000.0 / playerpugteampos.duration)) AS avg_fcdmg, "
"avg(clrdmg * (1200000.0 / playerpugteampos.duration)) AS avg_clrdmg, "
"avg(othrdmg * (1200000.0 / playerpugteampos.duration)) AS avg_othrdmg, "
"avg(dmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_dmgtkn, "
"avg(fcdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_fcdmgtkn, "
"avg(clrdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_clrdmgtkn, "
"avg(othrdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_othrdmgtkn, "
"avg(fckil * (1200000.0 / playerpugteampos.duration)) AS avg_fckil, "
"CAST(sum((CASE WHEN fckileff IS NOT NULL THEN fckileff ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN fckileff IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_fckileff, "
"avg(ret * (1200000.0 / playerpugteampos.duration)) AS avg_ret, "
"avg(sk * (1200000.0 / playerpugteampos.duration)) AS avg_sk, "
"avg(ttlhold * (1200000.0 / playerpugteampos.duration)) AS avg_ttlhold, "
"avg(maxhold * (1200000.0 / playerpugteampos.duration)) AS avg_maxhold, "
"CAST(sum(avgspd * playerpugteampos.duration) AS FLOAT) / CAST(sum(playerpugteampos.duration) AS FLOAT) AS avg_avgspd, "
"CAST(sum(topspd * playerpugteampos.duration) AS FLOAT) / CAST(sum(playerpugteampos.duration) AS FLOAT) AS avg_topspd, "
"avg(boon * (1200000.0 / playerpugteampos.duration)) AS avg_boon, "
"avg(push * (1200000.0 / playerpugteampos.duration)) AS avg_push, "
"avg(pull * (1200000.0 / playerpugteampos.duration)) AS avg_pull, "
"avg(heal * (1200000.0 / playerpugteampos.duration)) AS avg_heal, "
"avg(te * (1200000.0 / playerpugteampos.duration)) AS avg_te, "
"CAST(sum((CASE WHEN teeff IS NOT NULL THEN teeff ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN teeff IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_teeff, "
"avg(enemynrg * (1200000.0 / playerpugteampos.duration)) AS avg_enemynrg, "
"avg(absorb * (1200000.0 / playerpugteampos.duration)) AS avg_absorb, "
"avg(protdmg * (1200000.0 / playerpugteampos.duration)) AS avg_protdmg, "
"avg(prottime * (1200000.0 / playerpugteampos.duration)) AS avg_prottime, "
"avg(rage * (1200000.0 / playerpugteampos.duration)) AS avg_rage, "
"avg(drain * (1200000.0 / playerpugteampos.duration)) AS avg_drain, "
"avg(drained * (1200000.0 / playerpugteampos.duration)) AS avg_drained, "
"CAST(sum((CASE WHEN fs IS NOT NULL THEN fs ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN fs IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_fs, "
"CAST(sum((CASE WHEN bas IS NOT NULL THEN bas ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN bas IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_bas, "
"CAST(sum((CASE WHEN mid IS NOT NULL THEN mid ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN mid IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_mid, "
"CAST(sum((CASE WHEN eba IS NOT NULL THEN eba ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN eba IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_eba, "
"CAST(sum((CASE WHEN efs IS NOT NULL THEN efs ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN efs IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_efs, "
"avg(gotte * (1200000.0 / playerpugteampos.duration)) AS avg_gotte "
"FROM playerpugteampos "
"JOIN sessions ON playerpugteampos.session_id = sessions.session_id "
"JOIN accounts ON accounts.account_id = sessions.account_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"LEFT JOIN wins ON wins.account_id = sessions.account_id AND wins.pos = playerpugteampos.pos "
"LEFT JOIN pugs_played ON pugs_played.account_id = sessions.account_id AND pugs_played.pos = playerpugteampos.pos "
"GROUP BY playerpugteampos.pos, accounts.account_id "
"ORDER BY playerpugteampos.pos ASC, accounts.account_id ASC) "
"SELECT account_id, "
"name, "
"pos, "
"pugs_played, "
"RANK() OVER (PARTITION BY pos ORDER BY pugs_played DESC) pugs_played_rank, "
"wins, "
"RANK() OVER (PARTITION BY pos ORDER BY wins DESC) wins_rank, "
"winrate, "
"RANK() OVER (PARTITION BY pos ORDER BY winrate DESC) winrate_rank, "
"avg_cap, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_cap DESC) avg_cap_rank, "
"avg_ass, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ass DESC) avg_ass_rank, "
"avg_def, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_def DESC) avg_def_rank, "
"avg_acc, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_acc DESC) avg_acc_rank, "
"avg_air, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_air DESC) avg_air_rank, "
"avg_tk, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_tk DESC) avg_tk_rank, "
"avg_take, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_take DESC) avg_take_rank, "
"avg_pitkil, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pitkil DESC) avg_pitkil_rank, "
"avg_pitdth, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pitdth DESC) avg_pitdth_rank, "
"avg_dmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dmg DESC) avg_dmg_rank, "
"avg_fcdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fcdmg DESC) avg_fcdmg_rank, "
"avg_clrdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_clrdmg DESC) avg_clrdmg_rank, "
"avg_othrdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_othrdmg DESC) avg_othrdmg_rank, "
"avg_dmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dmgtkn DESC) avg_dmgtkn_rank, "
"avg_fcdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fcdmgtkn DESC) avg_fcdmgtkn_rank, "
"avg_clrdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_clrdmgtkn DESC) avg_clrdmgtkn_rank, "
"avg_othrdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_othrdmgtkn DESC) avg_othrdmgtkn_rank, "
"avg_fckil, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fckil DESC) avg_fckil_rank, "
"avg_fckileff, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fckileff DESC) avg_fckileff_rank, "
"avg_ret, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ret DESC) avg_ret_rank, "
"avg_sk, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_sk DESC) avg_sk_rank, "
"avg_ttlhold, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ttlhold DESC) avg_ttlhold_rank, "
"avg_maxhold, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_maxhold DESC) avg_maxhold_rank, "
"avg_avgspd, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_avgspd DESC) avg_avgspd_rank, "
"avg_topspd, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_topspd DESC) avg_topspd_rank, "
"avg_boon, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_boon DESC) avg_boon_rank, "
"avg_push, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_push DESC) avg_push_rank, "
"avg_pull, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pull DESC) avg_pull_rank, "
"avg_heal, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_heal DESC) avg_heal_rank, "
"avg_te, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_te DESC) avg_te_rank, "
"avg_teeff, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_teeff DESC) avg_teeff_rank, "
"avg_enemynrg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_enemynrg DESC) avg_enemynrg_rank, "
"avg_absorb, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_absorb DESC) avg_absorb_rank, "
"avg_protdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_protdmg DESC) avg_protdmg_rank, "
"avg_prottime, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_prottime DESC) avg_prottime_rank, "
"avg_rage, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_rage DESC) avg_rage_rank, "
"avg_drain, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_drain DESC) avg_drain_rank, "
"avg_drained, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_drained DESC) avg_drained_rank, "
"avg_fs, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fs DESC) avg_fs_rank, "
"avg_bas, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_bas DESC) avg_bas_rank, "
"avg_mid, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_mid DESC) avg_mid_rank, "
"avg_eba, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_eba DESC) avg_eba_rank, "
"avg_efs, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_efs DESC) avg_efs_rank, "
"avg_gotte, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_gotte DESC) avg_gotte_rank "
"FROM t WHERE pugs_played >= 10;";

static qboolean UpgradeDBToVersion15(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v15Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v16Upgrade = "UPDATE playerratings SET rating = rating + 2;";

static qboolean UpgradeDBToVersion16(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v16Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v17Upgrade =
"CREATE TABLE IF NOT EXISTS [mapaliases] ([filename] TEXT COLLATE NOCASE NOT NULL, [alias] TEXT COLLATE NOCASE NOT NULL, [islive] BOOLEAN DEFAULT NULL, UNIQUE(filename), UNIQUE(alias, islive)); "
"CREATE VIEW IF NOT EXISTS [lastplayedalias] AS SELECT alias, sum(num) num, max(datetime) datetime FROM lastplayedmap JOIN mapaliases ON lastplayedmap.map = mapaliases.filename GROUP BY alias; "
"CREATE VIEW IF NOT EXISTS [lastplayedmaporalias] AS WITH t AS (SELECT alias, sum(num) num, max(datetime) datetime FROM lastplayedalias) SELECT lastplayedmap.map, CASE WHEN t.num IS NOT NULL THEN t.num ELSE lastplayedmap.num END num, CASE WHEN t.datetime IS NOT NULL THEN t.datetime ELSE lastplayedmap.datetime END datetime FROM lastplayedmap LEFT JOIN mapaliases ON lastplayedmap.map = mapaliases.filename LEFT JOIN t ON mapaliases.alias = t.alias;";

static qboolean UpgradeDBToVersion17(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v17Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v18Upgrade = "UPDATE playerratings SET rating = rating + 2;";

static qboolean UpgradeDBToVersion18(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v18Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

const char *const v19Upgrade = "ALTER TABLE playerpugteampos ADD COLUMN [grips] INTEGER; "
"ALTER TABLE playerpugteampos ADD COLUMN [gripped] INTEGER; "
"ALTER TABLE playerpugteampos ADD COLUMN [dark] INTEGER; "
"DROP VIEW [accountstats]; "
"CREATE VIEW [accountstats] AS "
"WITH wins AS ( "
"SELECT account_id, playerpugteampos.pos, count() AS wins "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"WHERE pugs.win_team = playerpugteampos.team "
"GROUP BY account_id, playerpugteampos.pos), "
"pugs_played AS ( "
"SELECT account_id, playerpugteampos.pos, count(*) AS pugs_played "
"FROM playerpugteampos "
"JOIN sessions ON sessions.session_id = playerpugteampos.session_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"GROUP BY account_id, playerpugteampos.pos), "
"t AS ( "
"SELECT accounts.account_id, accounts.name, playerpugteampos.pos, "
"pugs_played, "
"ifnull(wins, 0) AS wins, "
"CAST(ifnull(wins, 0) AS FLOAT) / pugs_played AS winrate, "
"avg(cap * (1200000.0 / playerpugteampos.duration)) AS avg_cap, "
"avg(ass * (1200000.0 / playerpugteampos.duration)) AS avg_ass, "
"avg(def * (1200000.0 / playerpugteampos.duration)) AS avg_def, "
"CAST(sum((CASE WHEN acc IS NOT NULL THEN acc ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN acc IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_acc, "
"avg(air * (1200000.0 / playerpugteampos.duration)) AS avg_air, "
"avg(tk * (1200000.0 / playerpugteampos.duration)) AS avg_tk, "
"avg(take * (1200000.0 / playerpugteampos.duration)) AS avg_take, "
"avg(pitkil * (1200000.0 / playerpugteampos.duration)) AS avg_pitkil, "
"avg(pitdth * (1200000.0 / playerpugteampos.duration)) AS avg_pitdth, "
"avg(dmg * (1200000.0 / playerpugteampos.duration)) AS avg_dmg, "
"avg(fcdmg * (1200000.0 / playerpugteampos.duration)) AS avg_fcdmg, "
"avg(clrdmg * (1200000.0 / playerpugteampos.duration)) AS avg_clrdmg, "
"avg(othrdmg * (1200000.0 / playerpugteampos.duration)) AS avg_othrdmg, "
"avg(dmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_dmgtkn, "
"avg(fcdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_fcdmgtkn, "
"avg(clrdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_clrdmgtkn, "
"avg(othrdmgtkn * (1200000.0 / playerpugteampos.duration)) AS avg_othrdmgtkn, "
"avg(fckil * (1200000.0 / playerpugteampos.duration)) AS avg_fckil, "
"CAST(sum((CASE WHEN fckileff IS NOT NULL THEN fckileff ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN fckileff IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_fckileff, "
"avg(ret * (1200000.0 / playerpugteampos.duration)) AS avg_ret, "
"avg(sk * (1200000.0 / playerpugteampos.duration)) AS avg_sk, "
"avg(ttlhold * (1200000.0 / playerpugteampos.duration)) AS avg_ttlhold, "
"avg(maxhold * (1200000.0 / playerpugteampos.duration)) AS avg_maxhold, "
"CAST(sum(avgspd * playerpugteampos.duration) AS FLOAT) / CAST(sum(playerpugteampos.duration) AS FLOAT) AS avg_avgspd, "
"CAST(sum(topspd * playerpugteampos.duration) AS FLOAT) / CAST(sum(playerpugteampos.duration) AS FLOAT) AS avg_topspd, "
"avg(boon * (1200000.0 / playerpugteampos.duration)) AS avg_boon, "
"avg(push * (1200000.0 / playerpugteampos.duration)) AS avg_push, "
"avg(pull * (1200000.0 / playerpugteampos.duration)) AS avg_pull, "
"avg(heal * (1200000.0 / playerpugteampos.duration)) AS avg_heal, "
"avg(te * (1200000.0 / playerpugteampos.duration)) AS avg_te, "
"CAST(sum((CASE WHEN teeff IS NOT NULL THEN teeff ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN teeff IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_teeff, "
"avg(enemynrg * (1200000.0 / playerpugteampos.duration)) AS avg_enemynrg, "
"avg(absorb * (1200000.0 / playerpugteampos.duration)) AS avg_absorb, "
"avg(protdmg * (1200000.0 / playerpugteampos.duration)) AS avg_protdmg, "
"avg(prottime * (1200000.0 / playerpugteampos.duration)) AS avg_prottime, "
"avg(rage * (1200000.0 / playerpugteampos.duration)) AS avg_rage, "
"avg(drain * (1200000.0 / playerpugteampos.duration)) AS avg_drain, "
"avg(drained * (1200000.0 / playerpugteampos.duration)) AS avg_drained, "
"CAST(sum((CASE WHEN fs IS NOT NULL THEN fs ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN fs IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_fs, "
"CAST(sum((CASE WHEN bas IS NOT NULL THEN bas ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN bas IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_bas, "
"CAST(sum((CASE WHEN mid IS NOT NULL THEN mid ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN mid IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_mid, "
"CAST(sum((CASE WHEN eba IS NOT NULL THEN eba ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN eba IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_eba, "
"CAST(sum((CASE WHEN efs IS NOT NULL THEN efs ELSE 0 END) * playerpugteampos.duration) AS FLOAT) / CAST(sum(CASE WHEN efs IS NOT NULL THEN playerpugteampos.duration ELSE 0 END) AS FLOAT) AS avg_efs, "
"avg(gotte * (1200000.0 / playerpugteampos.duration)) AS avg_gotte, "
"avg(grips * (1200000.0 / playerpugteampos.duration)) AS avg_grips, "
"avg(gripped * (1200000.0 / playerpugteampos.duration)) AS avg_gripped, "
"avg(dark * (1200000.0 / playerpugteampos.duration)) AS avg_dark "
"FROM playerpugteampos "
"JOIN sessions ON playerpugteampos.session_id = sessions.session_id "
"JOIN accounts ON accounts.account_id = sessions.account_id "
"JOIN pugs ON pugs.match_id = playerpugteampos.match_id "
"LEFT JOIN wins ON wins.account_id = sessions.account_id AND wins.pos = playerpugteampos.pos "
"LEFT JOIN pugs_played ON pugs_played.account_id = sessions.account_id AND pugs_played.pos = playerpugteampos.pos "
"GROUP BY playerpugteampos.pos, accounts.account_id "
"ORDER BY playerpugteampos.pos ASC, accounts.account_id ASC) "
"SELECT account_id, "
"name, "
"pos, "
"pugs_played, "
"RANK() OVER (PARTITION BY pos ORDER BY pugs_played DESC) pugs_played_rank, "
"wins, "
"RANK() OVER (PARTITION BY pos ORDER BY wins DESC) wins_rank, "
"winrate, "
"RANK() OVER (PARTITION BY pos ORDER BY winrate DESC) winrate_rank, "
"avg_cap, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_cap DESC) avg_cap_rank, "
"avg_ass, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ass DESC) avg_ass_rank, "
"avg_def, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_def DESC) avg_def_rank, "
"avg_acc, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_acc DESC) avg_acc_rank, "
"avg_air, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_air DESC) avg_air_rank, "
"avg_tk, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_tk DESC) avg_tk_rank, "
"avg_take, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_take DESC) avg_take_rank, "
"avg_pitkil, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pitkil DESC) avg_pitkil_rank, "
"avg_pitdth, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pitdth DESC) avg_pitdth_rank, "
"avg_dmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dmg DESC) avg_dmg_rank, "
"avg_fcdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fcdmg DESC) avg_fcdmg_rank, "
"avg_clrdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_clrdmg DESC) avg_clrdmg_rank, "
"avg_othrdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_othrdmg DESC) avg_othrdmg_rank, "
"avg_dmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dmgtkn DESC) avg_dmgtkn_rank, "
"avg_fcdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fcdmgtkn DESC) avg_fcdmgtkn_rank, "
"avg_clrdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_clrdmgtkn DESC) avg_clrdmgtkn_rank, "
"avg_othrdmgtkn, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_othrdmgtkn DESC) avg_othrdmgtkn_rank, "
"avg_fckil, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fckil DESC) avg_fckil_rank, "
"avg_fckileff, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fckileff DESC) avg_fckileff_rank, "
"avg_ret, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ret DESC) avg_ret_rank, "
"avg_sk, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_sk DESC) avg_sk_rank, "
"avg_ttlhold, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_ttlhold DESC) avg_ttlhold_rank, "
"avg_maxhold, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_maxhold DESC) avg_maxhold_rank, "
"avg_avgspd, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_avgspd DESC) avg_avgspd_rank, "
"avg_topspd, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_topspd DESC) avg_topspd_rank, "
"avg_boon, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_boon DESC) avg_boon_rank, "
"avg_push, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_push DESC) avg_push_rank, "
"avg_pull, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_pull DESC) avg_pull_rank, "
"avg_heal, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_heal DESC) avg_heal_rank, "
"avg_te, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_te DESC) avg_te_rank, "
"avg_teeff, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_teeff DESC) avg_teeff_rank, "
"avg_enemynrg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_enemynrg DESC) avg_enemynrg_rank, "
"avg_absorb, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_absorb DESC) avg_absorb_rank, "
"avg_protdmg, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_protdmg DESC) avg_protdmg_rank, "
"avg_prottime, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_prottime DESC) avg_prottime_rank, "
"avg_rage, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_rage DESC) avg_rage_rank, "
"avg_drain, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_drain DESC) avg_drain_rank, "
"avg_drained, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_drained DESC) avg_drained_rank, "
"avg_fs, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_fs DESC) avg_fs_rank, "
"avg_bas, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_bas DESC) avg_bas_rank, "
"avg_mid, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_mid DESC) avg_mid_rank, "
"avg_eba, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_eba DESC) avg_eba_rank, "
"avg_efs, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_efs DESC) avg_efs_rank, "
"avg_gotte, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_gotte DESC) avg_gotte_rank, "
"avg_grips, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_grips DESC) avg_grips_rank, "
"avg_gripped, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_gripped DESC) avg_gripped_rank, "
"avg_dark, "
"RANK() OVER (PARTITION BY pos ORDER BY avg_dark DESC) avg_dark_rank "
"FROM t WHERE pugs_played >= 10;";

static qboolean UpgradeDBToVersion19(sqlite3 *dbPtr) {
	return trap_sqlite3_exec(dbPtr, v19Upgrade, NULL, NULL, NULL) == SQLITE_OK;
}

// =============================================================================

static qboolean UpgradeDB( int versionTo, sqlite3* dbPtr ) {
	Com_Printf( "Upgrading database to version %d...\n", versionTo );

	switch ( versionTo ) {
		case 2: return UpgradeDBToVersion2( dbPtr );
		case 3: return UpgradeDBToVersion3( dbPtr );
		case 4: return UpgradeDBToVersion4( dbPtr );
		case 5:	return UpgradeDBToVersion5( dbPtr );
		case 6: return UpgradeDBToVersion6( dbPtr );
		case 7: return UpgradeDBToVersion7(dbPtr);
		case 8: return UpgradeDBToVersion8(dbPtr);
		case 9: return UpgradeDBToVersion9(dbPtr);
		case 10: return UpgradeDBToVersion10(dbPtr);
		case 11: return UpgradeDBToVersion11(dbPtr);
		case 12: return UpgradeDBToVersion12(dbPtr);
		case 13: return UpgradeDBToVersion13(dbPtr);
		case 14: return UpgradeDBToVersion14(dbPtr);
		case 15: return UpgradeDBToVersion15(dbPtr);
		case 16: return UpgradeDBToVersion16(dbPtr);
		case 17: return UpgradeDBToVersion17(dbPtr);
		case 18: return UpgradeDBToVersion18(dbPtr);
		case 19: return UpgradeDBToVersion19(dbPtr);
;		default:
			Com_Printf( "ERROR: Unsupported database upgrade routine\n" );
	}

	return qfalse;
}

qboolean G_DBUpgradeDatabaseSchema( int versionFrom, void* db, qboolean *didUpgrade ) {
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
		if (didUpgrade)
			*didUpgrade = qtrue;
	}
	
	return qtrue;
}