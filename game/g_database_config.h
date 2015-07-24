#ifndef G_DATABASE_CONFIG_H
#define G_DATABASE_CONFIG_H

#include "g_local.h"

// main loading routines
void G_CfgDbLoad();
void G_CfgDbUnload();

qboolean G_CfgDbIsFiltered( const char* ip, char* reasonBuffer, int reasonBufferSize );

// whitelist stuff
qboolean G_CfgDbAddToWhitelist( const char* ip,
    const char* mask,
    const char* notes );

qboolean G_CfgDbRemoveFromWhitelist( const char* ip,
    const char* mask );

qboolean G_CfgDbIsFilteredByWhitelist( int ipA,
    int ipB,
    int ipC, 
    int ipD, 
    char* reasonBuffer, 
    int reasonBufferSize );

// blacklist stuff
typedef void( *BlackListCallback )(const char* ip,
    const char* mask,
    const char* notes,
    const char* reason,
    const char* banned_since,
    const char* banned_until);

void G_CfgDbListBlacklist();

qboolean G_CfgDbAddToBlacklist( const char* ip,
    const char* mask, 
    const char* notes,
    const char* reason,
    int hours );

qboolean G_CfgDbRemoveFromBlacklist( const char* ip,
    const char* mask );                          

qboolean G_CfgDbIsFilteredByBlacklist( int ipA, int ipB, int ipC, int ipD, char* reasonBuffer, int reasonBufferSize );
            
// pools stuff
typedef void( *ListPoolCallback )(void* context,
    int pool_id,
    const char* short_name,
    const char* long_name);

void G_CfgDbListPools( ListPoolCallback, void* context );

typedef void( *ListMapsPoolCallback )(void* context,
    const char* long_name,
    int pool_id,
    const char* mapname,
    int weight);

void G_CfgDbListMapsInPool( const char* short_name, 
    const char* ignore,
    ListMapsPoolCallback, 
    void* context );

typedef struct
{
    int pool_id;
    char long_name[64];

} PoolInfo;

qboolean G_CfgDbFindPool( const char* short_name, PoolInfo* poolInfo );

typedef struct
{
    char mapname[64];

} MapInfo;

int G_CfgDbGetPoolWeight( const char* short_name,
    const char* ignoreMap);

qboolean G_CfgDbSelectMapFromPool( const char* short_name,
    const char* ignoreMap, 
    MapInfo* mapInfo );

qboolean G_CfgDbPoolCreate( const char* short_name, const char* long_name );
qboolean G_CfgDbPoolDeleteAllMaps( const char* short_name );
qboolean G_CfgDbPoolDelete( const char* short_name );

qboolean G_CfgDbPoolMapAdd( const char* short_name, const char* mapname, int weight );
qboolean G_CfgDbPoolMapRemove( const char* short_name, const char* mapname );

#endif //G_DATABASE_H


