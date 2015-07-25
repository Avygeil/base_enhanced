#include "g_database_log.h"
#include "sqlite3.h"
#include "time.h"

static sqlite3* db = 0;

const char* const logDbFileName = "jka_log.db";

const char* const sqlCreateLogDb =
"CREATE TABLE [sessions] (                                                      "
"    [session_id] INTEGER PRIMARY KEY AUTOINCREMENT,                            "
"    [session_start] DATETIME,                                                  "
"    [session_end] DATETIME,                                                    "
"    [ip_address_id] INTEGER REFERENCES [ip_address]([ip_address_id]),          "
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
"CREATE TABLE[session_stats](                                                   "
"    [session_id] INTEGER REFERENCES[sessions]( [session_id] ),                 "
"    [accuracy_hits] INTEGER,                                                   "
"    [accuracy_shots] INTEGER );                                                "
"                                                                               "
"                                                                               "
"CREATE TABLE[ip_address](                                                      "
"    [ip_address_id] INTEGER PRIMARY KEY AUTOINCREMENT,                         "
"    [ip_A] INTEGER,                                                            "
"    [ip_B] INTEGER,                                                            "
"    [ip_C] INTEGER,                                                            "
"    [ip_D] INTEGER );                                                          "
"                                                                               "
"                                                                               "
"CREATE TABLE [nicknames] (                                                     "
"    [ip_address_id] INTEGER REFERENCES [ip_address]([ip_address_id]),          "
"    [name] TEXT,                                                               "
"    [duration] TIME);                                                          ";

const char* const sqlLogLevelStart =
"INSERT INTO levels (level_start, mapname, restart) "
"VALUES (datetime('now'),?, ?)                      ";
         
const char* const sqlLogLevelEnd =
"UPDATE levels                      "
"SET level_end = datetime( 'now' )  "
"WHERE level_id = ? ;               ";       

const char* const sqllogSessionStart =
"INSERT INTO sessions (session_start, ip_address_id, ip_port)    "
"VALUES (datetime('now'),?,?)                                    ";

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
"INSERT INTO nicknames (ip_address_id, name, duration)            "
"VALUES (?,?,?)                                                   ";

const char* const sqlFindIpAddress =
"SELECT ip_address_id                                  "
"FROM ip_address                                       "
"WHERE ip_A = ? AND ip_B = ? AND ip_C = ? AND ip_D = ? ";

const char* const sqlAddIpAddress =
"INSERT INTO ip_address (ip_A, ip_B, ip_C, ip_D)            "
"VALUES (?,?,?, ?)                                          ";

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

    if ( rc != SQLITE_OK )
    {
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
int G_LogDbLogSessionStart( const char* ip )
{
    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqllogSessionStart, -1, &statement, 0 );

    int ipA = 0, ipB = 0, ipC = 0, ipD = 0, ipPort = 0;
    qboolean success = qfalse;

    // parse ip address and mask
    sscanf( ip, "%d.%d.%d.%d:%d", &ipA, &ipB, &ipC, &ipD, &ipPort );

    int ipAddressId = G_LogDbGetIpAddressId( ipA, ipB, ipC, ipD );     

    sqlite3_bind_int( statement, 1, ipAddressId );
    sqlite3_bind_int( statement, 2, ipPort );

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
//  G_LogDbFindIpAddressId
// 
//  Checks if IP address' exists in ip adresses table
//
int G_LogDbFindIpAddressId( int ipA, int ipB, int ipC, int ipD )
{
    int ip_address_id = -1;
    sqlite3_stmt* statement;

    // prepare blacklist check statement
    int rc = sqlite3_prepare( db, sqlFindIpAddress, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipA );
    sqlite3_bind_int( statement, 2, ipB );
    sqlite3_bind_int( statement, 3, ipC );
    sqlite3_bind_int( statement, 4, ipD );

    rc = sqlite3_step( statement );

    // blacklisted => we forbid it
    if ( rc == SQLITE_ROW )
    {
        ip_address_id = sqlite3_column_int( statement, 0 );
    }

    sqlite3_finalize( statement );

    return ip_address_id;
}

//
//  G_LogDbAddIpAddress
// 
//  Adds IP address' id in global ip adresses table
//
int G_LogDbAddIpAddress( int ipA, int ipB, int ipC, int ipD )
{
    sqlite3_stmt* statement;

    // prepare blacklist check statement
    int rc = sqlite3_prepare( db, sqlAddIpAddress, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipA );
    sqlite3_bind_int( statement, 2, ipB );
    sqlite3_bind_int( statement, 3, ipC );
    sqlite3_bind_int( statement, 4, ipD );

    rc = sqlite3_step( statement );

    int ip_address_id = sqlite3_last_insert_rowid( db );

    sqlite3_finalize( statement );

    return ip_address_id;
}

//
//  G_LogDbGetIpAddressId
// 
//  Gets IP address' id in global ip adresses table (creates if necessary)
//
int G_LogDbGetIpAddressId( int ipA, int ipB, int ipC, int ipD )
{
    int ip_address_id = G_LogDbFindIpAddressId( ipA, ipB, ipC, ipD );
    
    if ( ip_address_id == -1 )
    {
        ip_address_id = G_LogDbAddIpAddress( ipA, ipB, ipC, ipD );
    }

    return ip_address_id;    
}

//
//  G_LogDbLogNickname
// 
//  Logs players nickname
//
void G_LogDbLogNickname( const char* ip,
    const char* name,
    int duration)
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0, ipPort = 0;
    qboolean success = qfalse;

    // parse ip address and mask
    sscanf( ip, "%d.%d.%d.%d:%d", &ipA, &ipB, &ipC, &ipD, &ipPort );

    int ipAddressId = G_LogDbGetIpAddressId( ipA, ipB, ipC, ipD );
     
    sqlite3_stmt* statement;

    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlAddName, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipAddressId );
    sqlite3_bind_text( statement, 2, name, -1, 0 );
    sqlite3_bind_int( statement, 3, duration );

    rc = sqlite3_step( statement );

    sqlite3_finalize( statement );

}
