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
"    [ip_int] INTEGER PRIMARY KEY,                                              "
"    [mask_int] INTEGER,                                                        "
"    [notes] TEXT,                                                              "
"    [reason] TEXT,                                                             "
"    [banned_since] DATETIME,                                                   "
"    [banned_until] DATETIME );                                                 "
"                                                                               "
"                                                                               "
"CREATE TABLE[ip_whitelist](                                                    "
"    [ip_int] INTEGER PRIMARY KEY,                                              "
"    [mask_int] INTEGER,                                                        "
"    [notes] TEXT);                                                             "
"                                                                               "
"                                                                               "
"CREATE TABLE[pools](                                                           "
"    [pool_id] INTEGER PRIMARY KEY AUTOINCREMENT,                               "
"    [short_name] TEXT,                                                         "
"    [long_name] TEXT );                                                        " 
"                                                                               "
"                                                                               "
"CREATE TABLE [pool_has_map] (                                                  "
"    [pool_id] INTEGER REFERENCES [pools]([pool_id]) ON DELETE RESTRICT,        "
"    [mapname] TEXT,                                                            "
"    [weight] INTEGER);                                                         "; 

const char* const sqlIsIpBlacklisted =
"SELECT reason                                 "
"FROM ip_blacklist                             "
"WHERE( ip_int & mask_int ) = (? & mask_int)   "
"AND banned_until >= datetime('now')           ";

const char* const sqlIsIpWhitelisted =
"SELECT COUNT(*)                               "
"FROM ip_whitelist                             "
"WHERE( ip_int & mask_int ) = (? & mask_int)   ";

const char* const sqlListBlacklist =
"SELECT ip_int, mask_int, notes, reason, banned_since, banned_until   "
"FROM ip_blacklist                                                    ";

const char* const sqlAddToBlacklist =
"INSERT INTO ip_blacklist (ip_int,                                  "
"mask_int, notes, reason, banned_since, banned_until)               "
"VALUES (?,?,?,?,datetime('now'),datetime('now','+'||?||' hours'))  ";

const char* const sqlAddToWhitelist =
"INSERT INTO ip_whitelist (ip_int, mask_int, notes)  "
"VALUES (?,?,?)                                      ";

const char* const sqlRemoveFromBlacklist =
"DELETE FROM ip_blacklist   "
"WHERE ip_int = ?           "
"AND mask_int = ?           ";

const char* const sqlremoveFromWhitelist =
"DELETE FROM ip_whitelist   "
"WHERE ip_int = ?           "
"AND mask_int = ?           ";

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



//
//  G_CfgDbLoad
// 
//  Loads the database from disk, including creating of not exists
//  or if it is corrupted
//
void G_CfgDbLoad()
{    
    int rc = -1;    

    rc = sqlite3_initialize();
    rc = sqlite3_open_v2( cfgDbFileName, &db, SQLITE_OPEN_READWRITE, 0 );

	if (rc == SQLITE_OK)
		G_LogPrintf("Successfully loaded config database %s\n", cfgDbFileName);
	else {
		G_LogPrintf("Couldn't find config database %s, creating a new one\n", cfgDbFileName);
        // create new database
        rc = sqlite3_open_v2( cfgDbFileName, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0 );

        sqlite3_exec( db, sqlCreateCfgDb, 0, 0, 0 );
    }
}
    
//
//  G_CfgDbUnload
// 
//  Unloads the database from memory, includes flushing it 
//  to the file.
//
void G_CfgDbUnload()
{
    sqlite3_close( db );
}

//
//  G_CfgDbIsFiltered
// 
//  Checks if given ip is forbidden to join the game based
//  on blacklist/whitelist mechanisms.
//
qboolean G_CfgDbIsFiltered( unsigned int ip,
    char* reasonBuffer, 
    int reasonBufferSize )
{
    qboolean filtered = qfalse;

    if ( g_whitelist.integer )
    {
        filtered = G_CfgDbIsFilteredByWhitelist( ip, reasonBuffer, reasonBufferSize );
    }
    else
    {
        filtered = G_CfgDbIsFilteredByBlacklist( ip, reasonBuffer, reasonBufferSize );
    }

    return filtered;
}

//
//  G_CfgDbIsFilteredByWhitelist
// 
//  Helper method to check if given ip address is white listed
//  according to database.
//
qboolean G_CfgDbIsFilteredByWhitelist( unsigned int ip,
    char* reasonBuffer, 
    int reasonBufferSize )
{
    qboolean filtered = qfalse;

    // check if ip is on white list
    sqlite3_stmt* statement;

    // prepare whitelist check statement
    sqlite3_prepare( db, sqlIsIpWhitelisted, -1, &statement, 0 );

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

//
//  G_CfgDbIsFilteredByBlacklist
// 
//  Helper method to check if given ip address is black listed
//  according to database.
//
qboolean G_CfgDbIsFilteredByBlacklist( unsigned int ip, char* reasonBuffer, int reasonBufferSize )
{
    qboolean filtered = qfalse;

    sqlite3_stmt* statement;

    // prepare blacklist check statement
    int rc = sqlite3_prepare( db, sqlIsIpBlacklisted, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ip );

    rc = sqlite3_step( statement );

    // blacklisted => we forbid it
    if ( rc == SQLITE_ROW )
    {
        const char* reason = (const char*)sqlite3_column_text( statement, 0 );
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
//  G_CfgDbAddToWhitelist
// 
//  Adds ip address to whitelist
//
qboolean G_CfgDbAddToWhitelist( unsigned int ip,
    unsigned int mask,
    const char* notes )
{
    qboolean success = qfalse;

    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlAddToWhitelist, -1, &statement, 0 );

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

//
//  G_CfgDbListBlacklist
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
        unsigned int ip = sqlite3_column_int( statement, 0 );
        unsigned int mask = sqlite3_column_int( statement, 1 );
        const char* notes = (const char*)sqlite3_column_text( statement, 2 );
        const char* reason = (const char*)sqlite3_column_text( statement, 3 );
        const char* banned_since = (const char*)sqlite3_column_text( statement, 4 );
        const char* banned_until = (const char*)sqlite3_column_text( statement, 5 );

        callback( ip, mask, notes, reason, banned_since, banned_until );

        rc = sqlite3_step( statement );
    }

    sqlite3_finalize( statement ); 
}
//
//  G_CfgDbAddToBlacklist
// 
//  Adds ip address to blacklist
//
qboolean G_CfgDbAddToBlacklist( unsigned int ip,
    unsigned int mask,
    const char* notes, 
    const char* reason,
    int hours)
{
    qboolean success = qfalse;

    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlAddToBlacklist, -1, &statement, 0 );

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

//
//  G_CfgDbRemoveFromBlacklist
// 
//  Removes ip address from blacklist
//
qboolean G_CfgDbRemoveFromBlacklist( unsigned int ip,
    unsigned int mask )
{
    qboolean success = qfalse;

    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlRemoveFromBlacklist, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ip );
    sqlite3_bind_int( statement, 2, mask );

    rc = sqlite3_step( statement );
    
    if ( rc == SQLITE_DONE )
    {
        int changes = sqlite3_changes( db );    
        if ( changes != 0 )
        {
            success = qtrue;
        }          
    }

    sqlite3_finalize( statement );

    return success;
}

//
//  G_CfgDbRemoveFromWhitelist
// 
//  Removes ip address from whitelist
//
qboolean G_CfgDbRemoveFromWhitelist( unsigned int ip,
    unsigned int mask )
{
    qboolean success = qfalse;

    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlremoveFromWhitelist, -1, &statement, 0 );

    sqlite3_bind_int( statement, 1, ip );
    sqlite3_bind_int( statement, 2, mask );

    rc = sqlite3_step( statement );
    if ( rc == SQLITE_DONE )
    {
        int changes = sqlite3_changes( db );
        if ( changes != 0 )
        {
            success = qtrue;
        }
    }

    sqlite3_finalize( statement );

    return success;
}
   
//
//  G_CfgDbListPools
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
        const char* short_name = (const char*)sqlite3_column_text( statement, 1 );
        const char* long_name = (const char*)sqlite3_column_text( statement, 2 );

        callback( context, pool_id, short_name, long_name );

        rc = sqlite3_step( statement );
    }

    sqlite3_finalize( statement );       
}

//
//  G_CfgDbListMapsInPool
// 
//  List maps in pool
//
void G_CfgDbListMapsInPool( const char* short_name,
    const char* ignore,
	ListMapsPoolCallback callback,
    void** context )
{
    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlListMapsInPool, -1, &statement, 0 );

    sqlite3_bind_text( statement, 1, short_name, -1, 0 );
    sqlite3_bind_text( statement, 2, ignore, -1, 0 ); // ignore map, we 

    rc = sqlite3_step( statement );
    while ( rc == SQLITE_ROW )
    {
        const char* long_name = (const char*)sqlite3_column_text( statement, 0 );
        int pool_id = sqlite3_column_int( statement, 1 );
        const char* mapname = (const char*)sqlite3_column_text( statement, 2 );
        int weight = sqlite3_column_int( statement, 3 );
		if ( weight < 1 ) weight = 1;

		if ( Q_stricmp( mapname, ignore ) ) {
			callback( context, long_name, pool_id, mapname, weight );
		}

        rc = sqlite3_step( statement );
    }

    sqlite3_finalize( statement );
}

//
//  G_CfgDbFindPool
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
        const char* long_name = (const char*)sqlite3_column_text( statement, 1 );

        Q_strncpyz( poolInfo->long_name, long_name, sizeof( poolInfo->long_name) );
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

void BuildCumulativeWeight( void** context,
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
	} else {
		// otherwise, add it to the end of the list
		CumulativeMapWeight *n = *cdf;

		while ( n->next ) {
			n = n->next;
		}

		currentNode->cumulativeWeight += n->cumulativeWeight; // add the weight of the previous node
		n->next = currentNode;
	}
}

//
//  G_CfgDbSelectMapFromPool
// 
//  Selects map from pool
//
qboolean G_CfgDbSelectMapsFromPool( const char* short_name,
	const char* ignoreMap,
	const int mapsToRandomize,
	MapSelectedCallback callback,
	void* context )
{
    PoolInfo poolInfo;
    if ( G_CfgDbFindPool( short_name, &poolInfo ) )
    {
		// fill the cumulative density function of the map pool
		CumulativeMapWeight *cdf = NULL;
		G_CfgDbListMapsInPool( short_name, ignoreMap, BuildCumulativeWeight, ( void ** )&cdf );

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
				} else {
					n = cdf;
					while ( n->next && n->next != nodeToDelete ) {
						n = n->next;
					}
					weightToDelete = nodeToDelete->cumulativeWeight - n->cumulativeWeight;
				}

				// relink the list
				if ( !n ) {
					cdf = cdf->next; // we are deleting the head, so just rebase it (may be NULL if this was the last remaining element)
				} else {
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
   
//
//  G_CfgDbPoolCreate
// 
//  Creates pool
//
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

//
//  G_CfgDbPoolDeleteAllMaps
// 
//  Deletes all maps in pool
//
qboolean G_CfgDbPoolDeleteAllMaps( const char* short_name )
{
    qboolean success = qfalse;

    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlDeleteAllMapsInPool, -1, &statement, 0 );

    sqlite3_bind_text( statement, 1, short_name, -1, 0 );

    rc = sqlite3_step( statement );
    if ( rc == SQLITE_DONE )
    {
        int changes = sqlite3_changes( db );
        if ( changes != 0 )
        {
            success = qtrue;
        }
    }

    sqlite3_finalize( statement );

    return success;
}

//
//  G_CfgDbPoolDelete
// 
//  Deletes pool
//
qboolean G_CfgDbPoolDelete( const char* short_name )
{         
    qboolean success = qfalse;

    if ( G_CfgDbPoolDeleteAllMaps( short_name ) )
    {
        sqlite3_stmt* statement;
        // prepare insert statement
        int rc = sqlite3_prepare( db, sqlDeletePool, -1, &statement, 0 );

        sqlite3_bind_text( statement, 1, short_name, -1, 0 );

        rc = sqlite3_step( statement );
        if ( rc == SQLITE_DONE )
        {
            int changes = sqlite3_changes( db );
            if ( changes != 0 )
            {
                success = qtrue;
            }
        }

        sqlite3_finalize( statement );
    } 

    return success;
}

//
//  G_CfgDbPoolMapAdd
// 
//  Adds map into pool
//
qboolean G_CfgDbPoolMapAdd( const char* short_name, const char* mapname, int weight )
{    
    qboolean success = qfalse;

    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlAddMapToPool, -1, &statement, 0 );
                  
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

//
//  G_CfgDbPoolMapRemove
// 
//  Removes map from pool
//
qboolean G_CfgDbPoolMapRemove( const char* short_name, const char* mapname )
{
    qboolean success = qfalse;

    sqlite3_stmt* statement;
    // prepare insert statement
    int rc = sqlite3_prepare( db, sqlRemoveMapToPool, -1, &statement, 0 );

    sqlite3_bind_text( statement, 1, short_name, -1, 0 );
    sqlite3_bind_text( statement, 2, mapname, -1, 0 );

    rc = sqlite3_step( statement );
    if ( rc == SQLITE_DONE )
    {
        int changes = sqlite3_changes( db );
        if ( changes != 0 )
        {
            success = qtrue;
        }
    }

    sqlite3_finalize( statement );

    return success;
}
