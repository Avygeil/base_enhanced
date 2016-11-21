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
"  [cuid_hash] BIGINT );                                                        ";
                                                                               

const char* const sqlLogLevelStart =
"INSERT INTO levels (level_start, mapname, restart) "
"VALUES (datetime('now'),?, ?)                      ";
         
const char* const sqlLogLevelEnd =
"UPDATE levels                      "
"SET level_end = datetime( 'now' )  "
"WHERE level_id = ? ;               ";       

const char* const sqllogSessionStart =
"INSERT INTO sessions (session_start, ip_int, ip_port, client_id)    "
"VALUES (datetime('now'),?,?, ?)                                            ";

const char* const sqllogSessionEnd =
"UPDATE sessions                      "
"SET session_end = datetime( 'now' )  "
"WHERE session_id = ? ;               ";

const char* const sqlAddLevelEvent =
"INSERT INTO level_events (level_id, event_level_time, event_id,           "
"event_context_i1, event_context_i2, event_context_i3, event_context_i4,   "
"event_context)                                                            "
"VALUES (?,?,?,?,?,?,?,?)                                                  ";

const char* const sqlAddName =
"INSERT INTO nicknames (ip_int, name, duration, cuid_hash)                   "
"VALUES (?,?,?,?)                                                            ";

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
"WHERE nicknames.cuid_hash & ?1                                 "
"GROUP BY name                                                  "
"ORDER BY time DESC                                             "
"LIMIT ?2                                                       ";

const char* const sqlCountNMAliases =
"SELECT COUNT(*) FROM ("
"SELECT name, SUM( duration ) AS time                           "
"FROM nicknames                                                 "
"WHERE nicknames.cuid_hash & ?1                                 "
"GROUP BY name                                                  "
"ORDER BY time DESC                                             "
"LIMIT ?2                                                       "
")                                                              ";

const char* const sqlTestCuidSupport =
"PRAGMA table_info(nicknames)                                   ";

const char* const sqlUpgradeCuidSupport =
"ALTER TABLE nicknames ADD cuid_hash BIGINT                     ";

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
		qboolean foundCuid = qfalse;
		while (rc == SQLITE_ROW) {
			const char *name = (const char*)sqlite3_column_text(statement, 1);
			if (name && *name && !Q_stricmp(name, "cuid_hash"))
				foundCuid = qtrue;
			rc = sqlite3_step(statement);
		}
		if (foundCuid) {
			//G_LogPrintf("Log database supports cuid_hash, no upgrade needed.\n");
		}
		else {
			G_LogPrintf("Automatically upgrading old log database to support cuid hashes.\n");
			sqlite3_reset(statement);
			rc = sqlite3_prepare(db, sqlUpgradeCuidSupport, -1, &statement, 0);
			rc = sqlite3_step(statement);
		}
		sqlite3_finalize(statement);
	}
	else {
		G_LogPrintf("Couldn't find log database %s, creating a new one\n", logDbFileName);
        // create new database
        rc = sqlite3_open_v2( logDbFileName, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0 );

        sqlite3_exec( db, sqlCreateLogDb, 0, 0, 0 );
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
    int rc = -1;

    rc = sqlite3_close( db );
}

//
//  G_LogDbLogLevelStart
// 
//  Logs current level (map) to the database
//
int G_LogDbLogLevelStart(qboolean isRestart)
{
    int rc = -1;
    sqlite3_stmt* statement;

    // load current map name
    char mapname[128];
    trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );

    // prepare insert statement
    rc = sqlite3_prepare( db, sqlLogLevelStart, -1, &statement, 0 );
    sqlite3_bind_text( statement, 1, mapname, -1, 0 );
    sqlite3_bind_int( statement, 2, isRestart ? 1 : 0 );

    rc = sqlite3_step( statement );

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
    int rc = -1;
    sqlite3_stmt* statement;

    // prepare update statement
    rc = sqlite3_prepare( db, sqlLogLevelEnd, -1, &statement, 0 );
    sqlite3_bind_int( statement, 1, levelId );

    rc = sqlite3_step( statement );

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
    int rc = sqlite3_prepare( db, sqlAddLevelEvent, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, levelId );
    sqlite3_bind_int( statement, 2, levelTime );
    sqlite3_bind_int( statement, 3, eventId );
    sqlite3_bind_int( statement, 4, context1 );
    sqlite3_bind_int( statement, 5, context2 );
    sqlite3_bind_int( statement, 6, context3 );
    sqlite3_bind_int( statement, 7, context4 );
    sqlite3_bind_text( statement, 8, contextText, -1, 0 );

    rc = sqlite3_step( statement );

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
    int rc = sqlite3_prepare( db, sqllogSessionStart, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipInt );
    sqlite3_bind_int( statement, 2, ipPort );
    sqlite3_bind_int( statement, 3, id );

    rc = sqlite3_step( statement );

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
    int rc = sqlite3_prepare( db, sqllogSessionEnd, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, sessionId );

    rc = sqlite3_step( statement );

    sqlite3_finalize( statement );
}

//
//  G_LogDbLogNickname
// 
//  Logs players nickname
//
void G_LogDbLogNickname( unsigned int ipInt,
    const char* name,
    int duration,
	unsigned long long cuidHash)
{
    sqlite3_stmt* statement;

    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlAddName, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipInt );
    sqlite3_bind_text( statement, 2, name, -1, 0 );
    sqlite3_bind_int( statement, 3, duration );
	sqlite3_bind_int64(statement, 4, (signed long long)cuidHash); // sql needs it signed

    rc = sqlite3_step( statement );

    sqlite3_finalize( statement ); 
}

void G_CfgDbListAliases( unsigned int ipInt,
    unsigned int ipMask,
    int limit,
    ListAliasesCallback callback,
    void* context,
	unsigned long long cuidHash)
{
	sqlite3_stmt* statement;
	int rc;
	const char* name;
	int duration;
	if (cuidHash) { // newmod user; check for cuid matches first before falling back to checking for unique id matches
		int numNMFound = 0;
		rc = sqlite3_prepare(db, sqlCountNMAliases, -1, &statement, 0);
		sqlite3_bind_int64(statement, 1, (signed long long)cuidHash); // sql needs it signed
		sqlite3_bind_int(statement, 2, limit);

		rc = sqlite3_step(statement);
		while (rc == SQLITE_ROW) {
			numNMFound = sqlite3_column_int(statement, 0);
			rc = sqlite3_step(statement);
		}
		sqlite3_reset(statement);

		if (numNMFound) { // we found some cuid matches; let's use these
			rc = sqlite3_prepare(db, sqlGetNMAliases, -1, &statement, 0);
			sqlite3_bind_int64(statement, 1, (signed long long)cuidHash); // sql needs it signed
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
