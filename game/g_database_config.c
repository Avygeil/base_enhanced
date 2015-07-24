#include "g_database_config.h"
#include "sqlite3.h"
#include "time.h"

static sqlite3* db = 0;

const char* const cfgDbFileName = "jka_config.db";

const char* const sqlCreateCfgDb =
"CREATE TABLE[accounts](                                                        "
"    [account_id] INTEGER PRIMARY KEY AUTOINCREMENT,                            "
"    [username] TEXT,                                                           "
"    [public_name] TEXT,                                                        "
"    [guid] TEXT );                                                             "
"                                                                               "
"                                                                               "
"CREATE TABLE[ip_blacklist](                                                    "
"    [ip_A] INTEGER( 255 ),                                                     "
"    [ip_B] INTEGER( 255 ),                                                     "
"    [ip_C] INTEGER( 255 ),                                                     "
"    [ip_D] INTEGER( 255 ),                                                     "
"    [mask_A] INTEGER( 255 ),                                                   "
"    [mask_B] INTEGER( 255 ),                                                   "
"    [mask_C] INTEGER( 255 ),                                                   "
"    [mask_D] INTEGER( 255 ),                                                   "
"    [notes] TEXT,                                                              "
"    [reason] TEXT,                                                             "
"    [banned_since] DATETIME,                                                   "
"    [banned_until] DATETIME );                                                 "
"                                                                               "
"                                                                               "
"CREATE TABLE[ip_whitelist](                                                    "
"    [ip_A] INTEGER( 255 ),                                                     "
"    [ip_B] INTEGER( 255 ),                                                     "
"    [ip_C] INTEGER( 255 ),                                                     "
"    [ip_D] INTEGER( 255 ),                                                     "
"    [mask_A] INTEGER( 255 ),                                                   "
"    [mask_B] INTEGER( 255 ),                                                   "
"    [mask_C] INTEGER( 255 ),                                                   "
"    [mask_D] INTEGER( 255 ),                                                   "
"    [notes] TEXT);                                                             "
"                                                                               "
"                                                                               "
"CREATE TABLE[pools](                                                           "
"    [pool_id] INTEGER PRIMARY KEY AUTOINCREMENT,                               "
"    [short_name] TEXT,                                                         "
"    [long_name] TEXT );                                                        " 
"                                                                               "
"                                                                               "
"CREATE TABLE[pool_has_map](                                                    "
"    [pool_id] INTEGER REFERENCES[pools]( [pool_id] ),                          "
"    [mapname] TEXT,                                                            "
"    [weight] INTEGER );                                                        "; 

const char* const sqlIsIpBlacklisted =
"SELECT reason                           "
"FROM ip_blacklist                       "
"WHERE( ip_A & mask_A ) = (? & mask_A)   "
"AND( ip_B & mask_B ) = (? & mask_B)     "
"AND( ip_C & mask_C ) = (? & mask_C)     "
"AND( ip_D & mask_D ) = (? & mask_D)     "
"AND banned_until >= datetime('now')     ";

const char* const sqlIsIpWhitelisted =
"SELECT COUNT(*)                         "
"FROM ip_whitelist                       "
"WHERE( ip_A & mask_A ) = (? & mask_A)   "
"AND( ip_B & mask_B ) = (? & mask_B)     "
"AND( ip_C & mask_C ) = (? & mask_C)     "
"AND( ip_D & mask_D ) = (? & mask_D)     ";

const char* const sqlListBlacklist =
"SELECT( ip_A || '.' || ip_B || '.' || ip_C || '.' || ip_D ) AS ip,   "
"(mask_A || '.' || mask_B || '.' || mask_C || '.' || mask_D) AS mask, "
"notes, reason, banned_since, banned_until                            "
"FROM ip_blacklist                                                    ";

const char* const sqlAddToBlacklist =
"INSERT INTO ip_blacklist (ip_A, ip_B, ip_C, ip_D,                              "
"mask_A, mask_B, mask_C, mask_D, notes, reason, banned_since, banned_until)     "
"VALUES (?,?,?,?,?,?,?,?,?,?,datetime('now'),datetime('now','+'||?||' hours'))  ";

const char* const sqlAddToWhitelist =
"INSERT INTO ip_whitelist (ip_A, ip_B, ip_C, ip_D, " 
"mask_A, mask_B, mask_C, mask_D, notes)            "
"VALUES (?,?,?,?,?,?,?,?,?)                        ";

const char* const sqlRemoveFromBlacklist =
"DELETE FROM ip_blacklist                                   "
"WHERE ip_A = ? AND ip_B = ? AND ip_C = ? AND ip_D = ?      "
"AND mask_A = ? AND mask_B = ? AND mask_C = ? AND mask_D = ?";

const char* const sqlremoveFromWhitelist =
"DELETE FROM ip_whitelist                                   "
"WHERE ip_A = ? AND ip_B = ? AND ip_C = ? AND ip_D = ?      "
"AND mask_A = ? AND mask_B = ? AND mask_C = ? AND mask_D = ?";

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

const char* const sqlGetPoolWeight =
"SELECT SUM( weight ) AS sum                                                "
"FROM pools                                                                 "
"JOIN                                                                       "
"pool_has_map                                                               "
"ON pools.pool_id = pool_has_map.pool_id                                    "
"WHERE short_name = ? AND mapname <> ?                                      ";

const char* const sqlCreatePool =
"INSERT INTO pools (short_name, long_name) "
"VALUES (?,?)                              ";

const char* const sqlDeletePool =
"DELETE FROM pools          "
"WHERE short_name = ?;      ";

//
//  G_DbLoad
// 
//  Loads the database from disk, including creating of not exists
//  or if it is corrupted
//
void G_CfgDbLoad()
{    
    int rc = -1;    

    rc = sqlite3_initialize();
    rc = sqlite3_open_v2( cfgDbFileName, &db, SQLITE_OPEN_READWRITE, 0 );

    if ( rc != SQLITE_OK )
    {
        // create new database
        rc = sqlite3_open_v2( cfgDbFileName, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0 );

        sqlite3_exec( db, sqlCreateCfgDb, 0, 0, 0 );
    }
}
    
//
//  G_DbUnload
// 
//  Unloads the database from memory, includes flushing it 
//  to the file.
//
void G_CfgDbUnload()
{
    int rc = -1;

    rc = sqlite3_close( db );
}

//
//  G_DbIsFiltered
// 
//  Checks if given ip is forbidden to join the game based
//  on blacklist/whitelist mechanisms.
//
qboolean G_CfgDbIsFiltered( const char* ip, char* reasonBuffer, int reasonBufferSize )
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;
    int port = 0;
    int rc = -1;
    qboolean filtered = qfalse;

    // parse ip address
    if ( sscanf( ip, "%d.%d.%d.%d:%d", &ipA, &ipB, &ipC, &ipD, &port ) >= 4 )
    {
        if ( g_whitelist.integer )
        {
            filtered = G_CfgDbIsFilteredByWhitelist( ipA, ipB, ipC, ipD, reasonBuffer, reasonBufferSize );
        }
        else
        {
            filtered = G_CfgDbIsFilteredByBlacklist( ipA, ipB, ipC, ipD, reasonBuffer, reasonBufferSize );
        }
    }

    return filtered;
}

//
//  G_DbIsFilteredByWhitelist
// 
//  Helper method to check if given ip address is white listed
//  according to database.
//
qboolean G_CfgDbIsFilteredByWhitelist( int ipA, int ipB, int ipC, int ipD, char* reasonBuffer, int reasonBufferSize )
{
    qboolean filtered = qfalse;

    // check if ip is on white list
    sqlite3_stmt* statement;

    // prepare whitelist check statement
    int rc = sqlite3_prepare( db, sqlIsIpWhitelisted, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipA );
    sqlite3_bind_int( statement, 2, ipB );
    sqlite3_bind_int( statement, 3, ipC );
    sqlite3_bind_int( statement, 4, ipD );

    rc = sqlite3_step( statement );
    int count = sqlite3_column_int( statement, 0 );

    if ( count == 0 )
    {
        Q_strncpyz( reasonBuffer, "IP address not on whitelist", reasonBufferSize );
        filtered = qtrue;
    }

    sqlite3_finalize( statement );

    return filtered;
}

//
//  G_DbIsFilteredByBlacklist
// 
//  Helper method to check if given ip address is black listed
//  according to database.
//
qboolean G_CfgDbIsFilteredByBlacklist( int ipA, int ipB, int ipC, int ipD, char* reasonBuffer, int reasonBufferSize )
{
    qboolean filtered = qfalse;

    sqlite3_stmt* statement;

    // prepare blacklist check statement
    int rc = sqlite3_prepare( db, sqlIsIpBlacklisted, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ipA );
    sqlite3_bind_int( statement, 2, ipB );
    sqlite3_bind_int( statement, 3, ipC );
    sqlite3_bind_int( statement, 4, ipD );

    rc = sqlite3_step( statement );

    // blacklisted => we forbid it
    if ( rc == SQLITE_ROW )
    {
        const char* reason = sqlite3_column_text( statement, 0 ); 
        const char* prefix = "Banned: ";
        int prefixSize = strlen( prefix );

        Q_strncpyz( reasonBuffer, prefix, reasonBufferSize );
        Q_strncpyz( &reasonBuffer[prefixSize], reason, reasonBufferSize - prefixSize );
        filtered = qtrue;
    }

    sqlite3_finalize( statement );

    return filtered;
}

//
//  G_DbAddToWhitelist
// 
//  Adds ip address to whitelist
//
qboolean G_CfgDbAddToWhitelist( const char* ip,
    const char* mask, 
    const char* notes )
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;
    int maskA = 0, maskB = 0, maskC = 0, maskD = 0;
    qboolean success = qfalse;

    // parse ip address and mask
    if ( (sscanf( ip, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD ) >= 4)
        && (sscanf( mask, "%d.%d.%d.%d", &maskA, &maskB, &maskC, &maskD ) >= 4) )
    {
        sqlite3_stmt* statement;
        // prepare insert statement
        int rc = sqlite3_prepare( db, sqlAddToWhitelist, -1, &statement, 0 );

        sqlite3_bind_int( statement, 1, ipA );
        sqlite3_bind_int( statement, 2, ipB );
        sqlite3_bind_int( statement, 3, ipC );
        sqlite3_bind_int( statement, 4, ipD );

        sqlite3_bind_int( statement, 5, maskA );
        sqlite3_bind_int( statement, 6, maskB );
        sqlite3_bind_int( statement, 7, maskC );
        sqlite3_bind_int( statement, 8, maskD );

        sqlite3_bind_text( statement, 9, notes, -1, 0 );

        rc = sqlite3_step( statement ); 
        if ( rc == SQLITE_DONE )
        {
            success = qtrue;
        }

        sqlite3_finalize( statement );
    }  

    return success;
}

//
//  G_DbListBlacklist
// 
//  Lists contents of blacklist
//
void G_CfgDbListBlacklist( BlackListCallback  callback )
{
    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlListBlacklist, -1, &statement, 0 );

    rc = sqlite3_step( statement );
    while ( rc == SQLITE_ROW )
    {
        const char* ip = sqlite3_column_text( statement, 0 );
        const char* mask = sqlite3_column_text( statement, 1 );
        const char* notes = sqlite3_column_text( statement, 2 );
        const char* reason = sqlite3_column_text( statement, 3 );
        const char* banned_since = sqlite3_column_text( statement, 4 );
        const char* banned_until = sqlite3_column_text( statement, 5 );

        callback( ip, mask, notes, reason, banned_since, banned_until );

        rc = sqlite3_step( statement );
    }

    sqlite3_finalize( statement ); 
}
//
//  G_DbAddToBlacklist
// 
//  Adds ip address to blacklist
//
qboolean G_CfgDbAddToBlacklist( const char* ip,
    const char* mask, 
    const char* notes, 
    const char* reason,
    int hours)
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;
    int maskA = 0, maskB = 0, maskC = 0, maskD = 0;
    qboolean success = qfalse;

    // parse ip address and mask
    if ( (sscanf( ip, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD ) >= 4)
        && (sscanf( mask, "%d.%d.%d.%d", &maskA, &maskB, &maskC, &maskD ) >= 4) )
    {
        sqlite3_stmt* statement;
        // prepare insert statement
        int rc = sqlite3_prepare( db, sqlAddToBlacklist, -1, &statement, 0 );

        sqlite3_bind_int( statement, 1, ipA );
        sqlite3_bind_int( statement, 2, ipB );
        sqlite3_bind_int( statement, 3, ipC );
        sqlite3_bind_int( statement, 4, ipD );

        sqlite3_bind_int( statement, 5, maskA );
        sqlite3_bind_int( statement, 6, maskB );
        sqlite3_bind_int( statement, 7, maskC );
        sqlite3_bind_int( statement, 8, maskD );

        sqlite3_bind_text( statement, 9, notes, -1, 0 );
        sqlite3_bind_text( statement, 10, reason, -1, 0 );

        sqlite3_bind_int( statement, 11, hours ); 

        rc = sqlite3_step( statement ); 
        if ( rc == SQLITE_DONE )
        {
            success = qtrue;
        }

        sqlite3_finalize( statement );
    }

    return success;
}

//
//  G_DbRemoveFromBlacklist
// 
//  Removes ip address from blacklist
//
qboolean G_CfgDbRemoveFromBlacklist( const char* ip,
    const char* mask )
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;
    int maskA = 0, maskB = 0, maskC = 0, maskD = 0;
    qboolean success = qfalse;

    // parse ip address and mask
    if ( (sscanf( ip, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD ) >= 4)
        && (sscanf( mask, "%d.%d.%d.%d", &maskA, &maskB, &maskC, &maskD ) >= 4) )
    {
        sqlite3_stmt* statement;
        // prepare insert statement
        int rc = sqlite3_prepare( db, sqlRemoveFromBlacklist, -1, &statement, 0 );

        sqlite3_bind_int( statement, 1, ipA );
        sqlite3_bind_int( statement, 2, ipB );
        sqlite3_bind_int( statement, 3, ipC );
        sqlite3_bind_int( statement, 4, ipD );

        sqlite3_bind_int( statement, 5, maskA );
        sqlite3_bind_int( statement, 6, maskB );
        sqlite3_bind_int( statement, 7, maskC );
        sqlite3_bind_int( statement, 8, maskD );

        rc = sqlite3_step( statement );
        if ( rc == SQLITE_DONE )
        {
            success = qtrue;
        }

        sqlite3_finalize( statement );

    }

    return success;
}

//
//  G_DbRemoveFromWhitelist
// 
//  Removes ip address from whitelist
//
qboolean G_CfgDbRemoveFromWhitelist( const char* ip,
    const char* mask )
{
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;
    int maskA = 0, maskB = 0, maskC = 0, maskD = 0;
    qboolean success = qfalse;

    // parse ip address and mask
    if ( (sscanf( ip, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD ) >= 4)
        && (sscanf( mask, "%d.%d.%d.%d", &maskA, &maskB, &maskC, &maskD ) >= 4) )
    {
        sqlite3_stmt* statement;
        // prepare insert statement
        int rc = sqlite3_prepare( db, sqlremoveFromWhitelist, -1, &statement, 0 );

        sqlite3_bind_int( statement, 1, ipA );
        sqlite3_bind_int( statement, 2, ipB );
        sqlite3_bind_int( statement, 3, ipC );
        sqlite3_bind_int( statement, 4, ipD );

        sqlite3_bind_int( statement, 5, maskA );
        sqlite3_bind_int( statement, 6, maskB );
        sqlite3_bind_int( statement, 7, maskC );
        sqlite3_bind_int( statement, 8, maskD );

        rc = sqlite3_step( statement );
        if ( rc == SQLITE_DONE )
        {
            success = qtrue;
        }

        sqlite3_finalize( statement );

    }

    return success;
}
   
//
//  G_DbListPools
// 
//  List all map pools
//
void G_CfgDbListPools( ListPoolCallback callback, void* context )
{
    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlListPools, -1, &statement, 0 );

    rc = sqlite3_step( statement );
    while ( rc == SQLITE_ROW )
    {
        int pool_id = sqlite3_column_int( statement, 0 );
        const char* short_name = sqlite3_column_text( statement, 1 );
        const char* long_name = sqlite3_column_text( statement, 2 );

        callback( context, pool_id, short_name, long_name );

        rc = sqlite3_step( statement );
    }

    sqlite3_finalize( statement );       
}

//
//  G_DbListPools
// 
//  List maps in pool
//
void G_CfgDbListMapsInPool( const char* short_name, 
    const char* ignore, 
    ListMapsPoolCallback callback, 
    void* context )
{
    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlListMapsInPool, -1, &statement, 0 );

    sqlite3_bind_text( statement, 1, short_name, -1, 0 );
    sqlite3_bind_text( statement, 2, ignore, -1, 0 ); // ignore map, we 

    rc = sqlite3_step( statement );
    while ( rc == SQLITE_ROW )
    {
        const char* long_name = sqlite3_column_text( statement, 0 );
        int pool_id = sqlite3_column_int( statement, 1 );
        const char* mapname = sqlite3_column_text( statement, 2 );
        int weight = sqlite3_column_int( statement, 3 );

        callback( context, long_name, pool_id, mapname, weight );

        rc = sqlite3_step( statement );
    }

    sqlite3_finalize( statement );
}

//
//  G_DbFindPool
// 
//  Finds pool
//
qboolean G_CfgDbFindPool( const char* short_name, PoolInfo* poolInfo )
{  
    qboolean found = qfalse;

    sqlite3_stmt* statement;

    // prepare blacklist check statement
    int rc = sqlite3_prepare( db, sqlFindPool, -1, &statement, 0 );

    sqlite3_bind_text( statement, 1, short_name, -1, 0 );

    rc = sqlite3_step( statement );

    // blacklisted => we forbid it
    if ( rc == SQLITE_ROW )
    {
        int pool_id = sqlite3_column_int( statement, 0 );
        const char* long_name = sqlite3_column_text( statement, 1 );

        Q_strncpyz( poolInfo->long_name, long_name, sizeof( poolInfo->long_name) );
        poolInfo->pool_id = pool_id;

        found = qtrue;
    }

    sqlite3_finalize( statement );

    return found;
}

typedef  struct
{
    int acc;
    int random;
    char selectedMap[64];
    char ignore[MAX_MAP_NAME];
    qboolean selected;
} 
SelectMapInfo;

void selectMapCallback( void* context,
    const char* long_name,
    int pool_id,
    const char* mapname,
    int weight )
{
    SelectMapInfo* selectMapInfo = (SelectMapInfo*)context;

    if ( (selectMapInfo->acc  <= selectMapInfo->random) 
        && (selectMapInfo->random < selectMapInfo->acc + weight) )
    {
        if ( Q_strncmp( mapname, selectMapInfo->ignore, sizeof( selectMapInfo->ignore ) ) == 0 )
        {
            selectMapInfo->random += weight;
        }
        else
        {
            Q_strncpyz( selectMapInfo->selectedMap, mapname, sizeof( selectMapInfo->selectedMap ) );
            selectMapInfo->selected = qtrue;
        }   
    }

    selectMapInfo->acc += weight;
}

//
//  G_DbGetPoolWeight
// 
//  Gets sum of weights of maps in pool
//
int G_CfgDbGetPoolWeight( const char* short_name,
    const char* ignoreMap)
{
    sqlite3_stmt* statement;
    int weight = 0;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlGetPoolWeight, -1, &statement, 0 );

    sqlite3_bind_text( statement, 1, short_name, -1, 0 );
    sqlite3_bind_text( statement, 2, ignoreMap, -1, 0 );

    rc = sqlite3_step( statement );
    if ( rc == SQLITE_ROW )
    {
        weight = sqlite3_column_int( statement, 0 );
    }

    sqlite3_finalize( statement ); 

    return weight;
}

//
//  G_DbSelectMapFromPool
// 
//  Selects map from pool
//
qboolean G_CfgDbSelectMapFromPool( const char* short_name,
    const char* ignoreMap,
    MapInfo* mapInfo )
{
    // TBD this function should make sure that current map will not be selected
    PoolInfo poolInfo;
    if ( G_CfgDbFindPool( short_name, &poolInfo ) )
    {
        SelectMapInfo selectMapInfo;
        selectMapInfo.acc = 0;
        selectMapInfo.selectedMap[0] = '\0';
        selectMapInfo.selected = qfalse;
        
        Q_strncpyz( selectMapInfo.ignore, ignoreMap, sizeof( selectMapInfo.ignore ) );     

        int weight = G_CfgDbGetPoolWeight( short_name, ignoreMap );

        if ( weight )
        {
            selectMapInfo.random = rand() % weight;

            G_CfgDbListMapsInPool( short_name, ignoreMap, selectMapCallback, &selectMapInfo );

            if ( selectMapInfo.selected )
            {
                Q_strncpyz( mapInfo->mapname, selectMapInfo.selectedMap, sizeof( mapInfo->mapname ) );
                return qtrue;
            }
        }    
    }

    return qfalse;   
}
   
qboolean G_CfgDbPoolCreate( const char* short_name, const char* long_name )
{
    qboolean success = qfalse;

    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlCreatePool, -1, &statement, 0 );

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

qboolean G_CfgDbPoolDelete( const char* short_name )
{         
    qboolean success = qfalse;

    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlDeletePool, -1, &statement, 0 );

    sqlite3_bind_text( statement, 1, short_name, -1, 0 );

    rc = sqlite3_step( statement );
    if ( rc == SQLITE_DONE )
    {
        success = qtrue;
    }

    sqlite3_finalize( statement );

    return success;
}

qboolean G_CfgDbPoolMapAdd( const char* short_name, const char* mapname, int weight )
{
    return qfalse;
}

qboolean G_CfgDbPoolMapRemove( const char* short_name, const char* mapname )
{
    return qfalse;
}