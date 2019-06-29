#include "g_database.h"
#include "sqlite3.h"

const char* const oldLogDbFilename = "jka_log.db";
const char* const oldCfgDbFilename = "jka_config.db";

// =========== METADATA ========================================================

const char* const sqlCopyOldTablesToNewV1DB =
"ATTACH DATABASE 'jka_log.db' AS logDb;                                      \n"
"ATTACH DATABASE 'jka_config.db' as cfgDb;                                   \n"
"BEGIN TRANSACTION;                                                          \n"
"INSERT INTO main.fastcapsV2 SELECT * FROM logDb.fastcapsV2;                 \n"
"INSERT INTO main.nicknames SELECT * FROM logDb.nicknames;                   \n"
"INSERT INTO main.ip_whitelist SELECT * FROM cfgDb.ip_whitelist;             \n"
"INSERT INTO main.ip_blacklist SELECT * FROM cfgDb.ip_blacklist;             \n"
"INSERT INTO main.pool_has_map SELECT * FROM cfgDb.pool_has_map;             \n"
"INSERT INTO main.pools SELECT * FROM cfgDb.pools;                           \n"
"COMMIT TRANSACTION;                                                         \n"
"DETACH DATABASE logDb;                                                      \n"
"DETACH DATABASE cfgDb;                                                        ";

static void UpgradeDBToVersion1( sqlite3* dbPtr ) {
	// special type of upgrade: upgrading from the two old file databases to version 1 of single database model

	int rc;

	// try to open both databases temporarily to make sure they exist, because ATTACH creates files that don't exist

	sqlite3* logDb;
	sqlite3* cfgDb;

	rc = sqlite3_open_v2( oldLogDbFilename, &logDb, SQLITE_OPEN_READONLY, NULL );

	if ( rc != SQLITE_OK ) {
		Com_Printf( "Failed to open logs database file %s for upgrading (code: %d)\n", oldLogDbFilename, rc );
		return;
	}


	rc = sqlite3_open_v2( oldCfgDbFilename, &cfgDb, SQLITE_OPEN_READONLY, NULL );

	if ( rc != SQLITE_OK ) {
		Com_Printf( "Failed to open config database file %s for upgrading (code: %d)\n", oldCfgDbFilename, rc );
		return;
	}

	sqlite3_close( logDb );
	sqlite3_close( cfgDb );

	// upgrade

	sqlite3_exec( dbPtr, sqlCopyOldTablesToNewV1DB, NULL, NULL, NULL );
}

// =============================================================================

static qboolean UpgradeDB( int versionTo, sqlite3* dbPtr ) {
	Com_Printf( "Upgrading database to version %d...\n", versionTo );

	switch ( versionTo ) {
		case 1: UpgradeDBToVersion1( dbPtr ); break;
		default:
			Com_Printf( "ERROR: Unsupported database upgrade routine\n" );
			return qfalse;
	}

	return qtrue;
}

qboolean G_DBUpgradeDatabaseSchema( int versionFrom, void* db ) {
	if ( versionFrom < 0 ) {
		versionFrom = 0;
	}

#ifndef _DEBUG
	if ( versionFrom > DB_SCHEMA_VERSION ) {
		Com_Printf( "WARNING: Database version is higher than the one used by this mod version!\n" );
	}
#endif

	if ( versionFrom >= DB_SCHEMA_VERSION ) {
		return qtrue;
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