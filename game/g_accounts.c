#include "g_local.h"
#include "g_database.h"

#include "cJSON.h"

// These are the cached memory locations client->session and client->account will always point to.
// It is necessary because multiple clients may point to the same session (multiple connections from the same computer)
// and multiple sessions may point to the same account, and this way changing stuff updates every reference.
// These arrays will contain at most level.clients actively used entries, but less in case of multiple connections
// from the same client or multiple sessions pointing to the same account.
// The indexes in the arrays do not always correspond to the client num! They are stored in sess->sessionCacheNum
// and sess->accountCacheNum so that they can be retrieved when gclient_t is cleared every spawn.
// The arrays are saved and loaded through a file just like client->sess ; while this is not optimal,
// it is safer than doing it through db for now since manually updating db could mess with the cache.
static session_t clientSessions[MAX_CLIENTS];
static account_t clientAccounts[MAX_CLIENTS];

static const char* cacheFilename = "accounts.dat";

void G_SaveAccountsCache( void ) {
	fileHandle_t cacheFile;
	trap_FS_FOpenFile( cacheFilename, &cacheFile, FS_WRITE );

	if ( !cacheFile ) {
		return;
	}

	int i;
	for ( i = 0; i < MAX_CLIENTS; ++i ) {
		trap_FS_Write( &clientSessions[i], sizeof( clientSessions[i] ), cacheFile );
		trap_FS_Write( &clientAccounts[i], sizeof( clientAccounts[i] ), cacheFile );
	}

	trap_FS_FCloseFile( cacheFile );
}

qboolean G_ReadAccountsCache( void ) {
	fileHandle_t cacheFile;
	const int fileSize = trap_FS_FOpenFile( cacheFilename, &cacheFile, FS_READ );

	if ( !cacheFile ) {
		return qfalse;
	}

	if ( fileSize != MAX_CLIENTS * ( sizeof( session_t ) + sizeof( account_t ) ) ) {
		return qfalse;
	}

	int i;
	for ( i = 0; i < MAX_CLIENTS; ++i ) {
		trap_FS_Read( &clientSessions[i], sizeof( clientSessions[i] ), cacheFile );
		trap_FS_Read( &clientAccounts[i], sizeof( clientAccounts[i] ), cacheFile );
	}

	trap_FS_FCloseFile( cacheFile );

	return qtrue;
}

// this must be called to rebuild pointers and cache when sessions are hotlinked
static void RelinkAccounts( void ) {

	// do this in two steps so that all account ptrs are properly freed for all clients when unlinking, so that we can't run out of cache slots

	int i;
	gclient_t *client;

	for ( i = 0; i < level.maxclients; ++i ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
			continue;
		}

		client = &level.clients[i];

		// if no session, reset account in case they just somehow lost their session
		if ( !client->session ) {
			client->account = NULL;
			client->sess.accountCacheNum = ACCOUNT_ID_UNLINKED;
			continue;
		}

		if ( client->session->accountId <= 0 && client->account ) {
			// their session has no account id but has an account ptr (just unlinked?)
			client->account = NULL;
			client->sess.accountCacheNum = ACCOUNT_ID_UNLINKED;
		}
	}

	for ( i = 0; i < level.maxclients; ++i ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
			continue;
		}

		client = &level.clients[i];

		if ( client->session && client->session->accountId > 0 && !client->account ) {
			// their session has an account id but no account ptr (just linked?)
			G_InitClientAccount( client );
		}
	}
}

// writes the account id to ctx for each element in the session list so that we can get the latest
static void GetLatestAccountId( void* ctx, session_t* session, qboolean referenced ) {
	if (session->accountId > 0) {
		*( ( int* )ctx ) = session->accountId;
	}
}

static qboolean AutoLinkSession( gclient_t* client, session_t* session ) {
	int existingAccountId = ACCOUNT_ID_UNLINKED;

#ifdef NEWMOD_SUPPORT

	// first try to link with newmod id
	if ( client->sess.auth == AUTHENTICATED && VALIDSTRING( client->sess.cuidHash ) ) {
		pagination_t pagination;
		pagination.numPerPage = INT_MAX;
		pagination.numPage = 1;

		G_DBListSessionsForInfo( "cuid_hash2", client->sess.cuidHash, pagination, GetLatestAccountId, &existingAccountId);

		// then try to link with autolinks
		if (existingAccountId <= 0 && level.autoLinksList.size) {
			char userinfo[MAX_INFO_VALUE] = { 0 };
			trap_GetUserinfo(client - level.clients, userinfo, sizeof(userinfo));
			char *sex = Info_ValueForKey(userinfo, "sex");
			char *guid = Info_ValueForKey(userinfo, "ja_guid");

			if (VALIDSTRING(sex) || VALIDSTRING(guid)) {
				iterator_t iter;
				ListIterate(&level.autoLinksList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					autoLink_t *autoLink = IteratorNext(&iter);
					assert(autoLink->guid[0] || autoLink->sex[0]);
					if (autoLink->guid[0] && (!VALIDSTRING(guid) || Q_stricmp(autoLink->guid, guid)))
						continue;
					if (autoLink->sex[0] && (!VALIDSTRING(sex) || Q_stricmp(autoLink->sex, sex)))
						continue;
					if (autoLink->country[0] && (!client->sess.country[0] || Q_stricmp(autoLink->country, client->sess.country)))
						continue;

					// link them
					accountReference_t acc = G_GetAccountByID(autoLink->accountId, qfalse);
					if (!acc.ptr) {
						Com_Printf("Warning: new client session autolink attempt for account id %d failed! Does that account still exist?\n", autoLink->accountId);
						continue;
					}

					if (G_LinkAccountToSession(session, acc.ptr)) {
						Com_Printf("New client session for player %s^7 successfully autolinked to account '%s' (id: %d)\n", client->pers.netname, acc.ptr->name, acc.ptr->id);
						trap_Cvar_Set("g_shouldReloadPlayerPugStats", "1");
						return qtrue;
					}
				}
			}
		}
	}

#endif

	// if we found an account id, retrieve it and hotlink it
	// since we call this from G_InitSession, we don't keep a reference to that account and we don't
	// want to relink accounts since it is expected that G_InitAccounts is called after. This is only to
	// make sure that both the database and the session pointer are linked with the existing account
	if (existingAccountId > 0) {
		account_t existingAccount;
		
		if (G_DBGetAccountByID(existingAccountId, &existingAccount)) {
			G_DBLinkAccountToSession(session, &existingAccount);

			if (session->accountId > 0) {
				return qtrue;
			}
		}
	}

	return qfalse;
}

static void BuildSessionInfo( char* outInfo, size_t outInfoSize, gclient_t *client ) {
	outInfo[0] = '\0';

	// we need to convert ip:port to just ip...

	char ipBuf[32];
	unsigned int ip;
	int port;

	if ( !getIpPortFromString( client->sess.ipString, &ip, &port ) ) {
		Com_Printf( "ERROR: BuildSessionInfo called for a client with invalid ip string!\n" );
		return;
	}
	getStringFromIp( ip, ipBuf, sizeof( ipBuf ) );

	cJSON *root = cJSON_CreateObject();

	cJSON_AddStringToObject( root, "ip", ipBuf );

#ifdef NEWMOD_SUPPORT
	// authed newmod clients have a cuid
	if ( client->sess.auth == AUTHENTICATED && VALIDSTRING( client->sess.cuidHash ) ) {
		cJSON_AddStringToObject( root, "cuid_hash2", client->sess.cuidHash );
	}
#endif

	// TODO: more stuff can be added to the info here
	// only info that is guaranteed to be UNIQUE and DIFFICULT TO CHANGE must be put here

	// print it to the buffer
	char* result = cJSON_PrintUnformatted( root );
	Q_strncpyz( outInfo, result, outInfoSize );
	free( result );

	cJSON_Delete( root );
}

static sessionInfoHash_t HashSessionInfo( const char* info ) {
	return XXH64( info, strlen( info ), 0L );
}

static const int deBruijnSeq[32] = { 0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };

static ID_INLINE int GetLowestSetBitIndex( int n ) {
	return deBruijnSeq[( ( uint32_t )( ( n & -n ) * 0x077CB531U ) ) >> 27]; // what the fuck?
}

void G_InitClientSession( gclient_t *client ) {
	session_t* newSessionPtr = NULL;
	client->session = NULL;
	client->stats = NULL;

	// immediately get the cached session if possible
	if ( IN_CLIENTNUM_RANGE( client->sess.sessionCacheNum ) ) {
		newSessionPtr = &clientSessions[client->sess.sessionCacheNum];

		if ( newSessionPtr->id ) {
			client->session = newSessionPtr;
			InitClientStats(client);
#ifdef _DEBUG
			G_LogPrintf( "Reusing cached session for client %d (cache num: %d, sess id: %d)\n", client - level.clients, client->sess.sessionCacheNum, newSessionPtr->id );
#endif
			return;
		}
	}

	// we need to allocate a new session
	client->sess.sessionCacheNum = -1;

	// first, build the session info string for this client
	char sessionInfo[MAX_SESSIONINFO_LEN];
	BuildSessionInfo( sessionInfo, sizeof( sessionInfo ), client );

	// we can't work without valid session info
	if ( !VALIDSTRING( sessionInfo ) ) {
		return;
	}

	// compute the hash from the info
	sessionInfoHash_t infoHash = HashSessionInfo( sessionInfo );

	// find a free slot while reusing another client's session if possible

	unsigned int freeSlots = 0xffffffff;

	int i;
	for ( i = 0; i < level.maxclients; ++i ) {
		if ( level.clients[i].pers.connected >= CON_CONNECTING &&
			IN_CLIENTNUM_RANGE( level.clients[i].sess.sessionCacheNum ) &&
			level.clients[i].session )
		{
			// this slot is taken
			freeSlots &= ~( 1 << level.clients[i].sess.sessionCacheNum );

			if ( level.clients[i].session->hash == infoHash ) {
				// reuse this client's session data
#ifdef _DEBUG
				G_LogPrintf( "Using existing session from client %d for client %d (cache num: %d, sess id: %d)\n", i, client - level.clients, level.clients[i].sess.sessionCacheNum, level.clients[i].session->id );
#endif
				client->session = level.clients[i].session;
				client->sess.sessionCacheNum = level.clients[i].sess.sessionCacheNum;
				InitClientStats(client);
				return;
			}
		}
	}

	const int lowestSlotAvailable = GetLowestSetBitIndex( freeSlots );

	if ( !freeSlots || !IN_CLIENTNUM_RANGE( lowestSlotAvailable ) ) {
		G_LogPrintf( "Reached max slots for sessions cache!\n" );
		return;
	}

#ifdef _DEBUG
	G_LogPrintf( "Assigning new session slot for client %d (cache num: %d)\n", client - level.clients, lowestSlotAvailable );
#endif

	newSessionPtr = &clientSessions[lowestSlotAvailable];

	// try to pull the session from db
	if ( !G_DBGetSessionByHash( infoHash, newSessionPtr ) ) {

		// not found: create a new one and pull it immediately

		G_DBCreateSession( infoHash, sessionInfo );
		if ( !G_DBGetSessionByHash( infoHash, newSessionPtr ) ) {
			G_LogPrintf( "ERROR: Failed to create a new session for client %d\n", client - level.clients );
			return;
		}

		G_LogPrintf( "Created new session (id: %d) for client %d\n", newSessionPtr->id, client - level.clients );

		// try to automatically link the session to an account if possible
		if ( AutoLinkSession( client, newSessionPtr ) ) {
			G_LogPrintf( "New session (id: %d) was automatically linked to account (id: %d)\n", newSessionPtr->id, newSessionPtr->accountId );
		}

	} else {
		G_LogPrintf( "Found existing session (id: %d) for client %d\n", newSessionPtr->id, client - level.clients );
	}

	client->session = newSessionPtr;
	client->sess.sessionCacheNum = lowestSlotAvailable;
	InitClientStats(client);
}

void G_InitClientAccount( gclient_t* client ) {
	account_t* newAccountPtr = NULL;
	client->account = NULL;

	// don't continue if they have an uninitialized session
	if ( !client->session ) {
		if ( IN_CLIENTNUM_RANGE( client->sess.accountCacheNum ) ) {
			G_LogPrintf( "WARNING: Client %d with uninitialized session carries valid account cache num %d\n", client - level.clients, client->sess.accountCacheNum );
			client->sess.accountCacheNum = -1;
		}

		return;
	}

	// immediately get the cached account if possible
	if ( IN_CLIENTNUM_RANGE( client->sess.accountCacheNum ) ) {
		newAccountPtr = &clientAccounts[client->sess.accountCacheNum];

		if ( newAccountPtr->id ) {
			client->account = newAccountPtr;
			if (client->stats) {
				client->stats->accountId = client->account->id;
				Q_strncpyz(client->stats->accountName, client->account->name, sizeof(client->stats->accountName));
			}
#ifdef _DEBUG
			G_LogPrintf( "Reusing cached account for client %d (cache num: %d, acc id: %d)\n", client - level.clients, client->sess.accountCacheNum, newAccountPtr->id );
#endif
			return;
		}
	}

	// don't continue if this session is not linked to an account
	if ( client->session->accountId <= 0 ) {
		return;
	}

	// we need to allocate a new account
	client->sess.accountCacheNum = -1;

	// find a free slot while reusing another client's account if possible

	unsigned int freeSlots = 0xffffffff;

	int i;
	for ( i = 0; i < level.maxclients; ++i ) {
		if ( level.clients[i].pers.connected >= CON_CONNECTING &&
			IN_CLIENTNUM_RANGE( level.clients[i].sess.accountCacheNum ) &&
			level.clients[i].account )
		{
			// this slot is taken
			freeSlots &= ~( 1 << level.clients[i].sess.accountCacheNum );

			if ( level.clients[i].account->id == client->session->accountId ) {
				// reuse this client's account data
#ifdef _DEBUG
				G_LogPrintf( "Using existing account from client %d for client %d (cache num: %d, acc id: %d)\n", i, client - level.clients, level.clients[i].sess.accountCacheNum, level.clients[i].account->id );
#endif
				client->account = level.clients[i].account;
				client->sess.accountCacheNum = level.clients[i].sess.accountCacheNum;
				if (client->stats) {
					client->stats->accountId = client->account->id;
					Q_strncpyz(client->stats->accountName, client->account->name, sizeof(client->stats->accountName));
				}
				return;
			}
		}
	}

	const int lowestSlotAvailable = GetLowestSetBitIndex( freeSlots );

	if ( !freeSlots || !IN_CLIENTNUM_RANGE( lowestSlotAvailable ) ) {
		G_LogPrintf( "Reached max slots for accounts cache!\n" );
		return;
	}

#ifdef _DEBUG
	G_LogPrintf( "Assigning new account slot for client %d (cache num: %d)\n", client - level.clients, lowestSlotAvailable );
#endif

	newAccountPtr = &clientAccounts[lowestSlotAvailable];

	// try to pull the account from db
	if ( !G_DBGetAccountByID( client->session->accountId, newAccountPtr ) ) {
		G_LogPrintf( "WARNING: Client %d with session id %d declares account id %d, but it couldn't be retrieved\n", client - level.clients, client->session->id, client->session->accountId  );
		return;
	} else {
		G_LogPrintf( "Found account '%s' (id: %d) for client %d\n", newAccountPtr->name, newAccountPtr->id, client - level.clients );
	}

	client->account = newAccountPtr;
	client->sess.accountCacheNum = lowestSlotAvailable;
	if (client->stats) {
		client->stats->accountId = client->account->id;
		Q_strncpyz(client->stats->accountName, client->account->name, sizeof(client->stats->accountName));
	}
}

// returns qtrue on creation success, qfalse on failure or if it already exists
// if out is not NULL, it will point to either the newly created account, or to the existing account
qboolean G_CreateAccount( const char* name, accountReference_t* out ) {
	accountReference_t result;

	// does it already exist?
	result = G_GetAccountByName( name, qfalse );

	if ( result.ptr ) {
		if ( out ) *out = result;
		return qfalse;
	}

	// create it
	G_DBCreateAccount( name );

	// was it successful?
	result = G_GetAccountByName( name, qfalse );

	if ( out ) *out = result;
	return result.ptr != NULL;
}

qboolean G_DeleteAccount( account_t* account ) {
	if ( !G_DBDeleteAccount( account ) ) {
		return qfalse;
	}

	// remove the reference to this account from all online sessions, then relink

	int i;
	for ( i = 0; i < level.maxclients; ++i ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
			continue;
		}

		gclient_t* client = &level.clients[i];

		if ( client->session && client->session->accountId == account->id ) {
			client->session->accountId = ACCOUNT_ID_UNLINKED;
		}
	}

	RelinkAccounts();

	return qtrue;
}

// This family of functions return special reference structures
// If there is an online player with this session/account, online = qtrue and ptr points to safe memory (cached)
// Otherwise, online = qfalse and ptr points to static memory pulled from DB that will be cleared the next call to one of the functions
// If no data could be found, ptr = NULL

typedef qboolean ( *SessionValidator )( const session_t* session, const void* ctx );
typedef qboolean ( *SessionDBPull )( session_t* session, const void* ctx );

static sessionReference_t GetSession( SessionValidator validator, SessionDBPull dbPull, const void* ctx, qboolean onlineOnly ) {
	static session_t staticSession;

	sessionReference_t result;
	result.ptr = NULL;
	result.online = qfalse;

	// try to get it from one of the online players
	int i;
	for ( i = 0; i < level.maxclients; ++i ) {
		if ( level.clients[i].pers.connected >= CON_CONNECTING &&
			IN_CLIENTNUM_RANGE( level.clients[i].sess.sessionCacheNum ) &&
			level.clients[i].session &&
			validator( level.clients[i].session, ctx ) )
		{
			// found it
			result.ptr = level.clients[i].session;
			result.online = qtrue;
			return result;
		}
	}

	// try to get it from database
	if ( !onlineOnly && dbPull( &staticSession, ctx ) ) {
		result.ptr = &staticSession;
		return result;
	}

	// didn't find anything
	return result;
}

static qboolean ValidateSessionID( const session_t* session, const void* ctx ) {
	return session->id == *( ( const int* )ctx );
}

static qboolean PullSessionByID( session_t* session, const void* ctx ) {
	return G_DBGetSessionByID( *( ( const int* )ctx ), session );
}

sessionReference_t G_GetSessionByID( const int id, qboolean onlineOnly ) {
	return GetSession( ValidateSessionID, PullSessionByID, &id, onlineOnly );
}

static qboolean ValidateSessionHash( const session_t* session, const void* ctx ) {
	return session->hash == *( ( const sessionInfoHash_t* )ctx );
}

static qboolean PullSessionByHash( session_t* session, const void* ctx ) {
	return G_DBGetSessionByHash( *( ( const sessionInfoHash_t* )ctx ), session );
}

sessionReference_t G_GetSessionByHash( const sessionInfoHash_t hash, qboolean onlineOnly ) {
	return GetSession( ValidateSessionHash, PullSessionByHash, &hash, onlineOnly );
}

typedef qboolean ( *AccountValidator )( const account_t* account, const void* ctx );
typedef qboolean ( *AccountDBPull )( account_t* account, const void* ctx );

static accountReference_t GetAccount( AccountValidator validator, AccountDBPull dbPull, const void* ctx, qboolean onlineOnly ) {
	static account_t staticAccount;

	accountReference_t result;
	result.ptr = NULL;
	result.online = qfalse;

	// try to get it from one of the online players
	int i;
	for ( i = 0; i < level.maxclients; ++i ) {
		if ( level.clients[i].pers.connected >= CON_CONNECTING &&
			IN_CLIENTNUM_RANGE( level.clients[i].sess.accountCacheNum ) &&
			level.clients[i].account &&
			validator( level.clients[i].account, ctx ) )
		{
			// found it
			result.ptr = level.clients[i].account;
			result.online = qtrue;
			return result;
		}
	}

	// try to get it from database
	if ( !onlineOnly && dbPull( &staticAccount, ctx ) ) {
		result.ptr = &staticAccount;
		return result;
	}

	return result;
}

static qboolean ValidateAccountID( const account_t* account, const void* ctx ) {
	return account->id == *( ( const int* )ctx );
}

static qboolean PullAccountByID( account_t* account, const void* ctx ) {
	return G_DBGetAccountByID( *( ( const int* )ctx ), account );
}

accountReference_t G_GetAccountByID( const int id, qboolean onlineOnly ) {
	return GetAccount( ValidateAccountID, PullAccountByID, &id, onlineOnly );
}

static qboolean ValidateAccountName( const account_t* account, const void* ctx ) {
	return !Q_stricmp( account->name, ( const char* )ctx );
}

static qboolean PullAccountByName( account_t* account, const void* ctx ) {
	return G_DBGetAccountByName( ( const char* )ctx, account );
}

accountReference_t G_GetAccountByName( const char* name, qboolean onlineOnly ) {
	return GetAccount( ValidateAccountName, PullAccountByName, name, onlineOnly );
}

qboolean G_LinkAccountToSession( session_t* session, account_t* account ) {
	G_DBLinkAccountToSession( session, account );

	if ( session->accountId > 0 ) {
		RelinkAccounts();
		return qtrue;
	}
	
	return qfalse;
}

qboolean G_UnlinkAccountFromSession( session_t* session ) {
	G_DBUnlinkAccountFromSession( session );

	if ( session->accountId <= 0 ) {
		RelinkAccounts();
		return qtrue;
	}

	return qfalse;
}

typedef struct {
	void *ctx;
	ListSessionsCallback callback;
} ReferencerCallbackProxy;

static void ListSessionsCallbackReferencer( void *ctx, session_t* session, qboolean referenced ) {
	ReferencerCallbackProxy* proxy = ( ReferencerCallbackProxy* )ctx;
	
	// make a reference out of that session_t: check if someone is online with that session

	sessionReference_t ref = G_GetSessionByID( session->id, qtrue );
	if ( !ref.ptr ) {
		ref.ptr = session;
		ref.online = qfalse;
	}

	proxy->callback( proxy->ctx, ref, referenced );
}

void G_ListSessionsForAccount( account_t* account, int numPerPage, int page, ListSessionsCallback callback, void* ctx ) {
	ReferencerCallbackProxy proxyCtx;
	proxyCtx.callback = callback;
	proxyCtx.ctx = ctx;

	pagination_t pagination;
	pagination.numPerPage = numPerPage;
	pagination.numPage = page;

	G_DBListSessionsForAccount( account, pagination, ListSessionsCallbackReferencer, &proxyCtx );
}

void G_ListSessionsForInfo( const char* key, const char* value, int numPerPage, int page, ListSessionsCallback callback, void* ctx ) {
	ReferencerCallbackProxy proxyCtx;
	proxyCtx.callback = callback;
	proxyCtx.ctx = ctx;

	pagination_t pagination;
	pagination.numPerPage = numPerPage;
	pagination.numPage = page;

	G_DBListSessionsForInfo( key, value, pagination, ListSessionsCallbackReferencer, &proxyCtx );
}

qboolean G_SessionInfoHasString( const session_t* session, const char* key ) {
	// epic hack
	char testValue[2];
	G_GetStringFromSessionInfo( session, key, testValue, sizeof( testValue ) );
	
	return VALIDSTRING( testValue );
}

void G_GetStringFromSessionInfo( const session_t* session, const char* key, char* outValue, size_t outValueSize ) {
	outValue[0] = '\0';

	if ( !VALIDSTRING( key ) ) {
		return;
	}

	if ( !VALIDSTRING( session->info ) ) {
		return;
	}

	cJSON *root = cJSON_Parse( session->info );

	if ( !root ) {
		G_LogPrintf( "Error: Failed to parse JSON\n" );

		const char *errorPtr = cJSON_GetErrorPtr();
		if ( errorPtr ) {
			G_LogPrintf( "JSON error before: %d\n", errorPtr );
		}

		return;
	}

	cJSON *value = cJSON_GetObjectItem( root, key );
	if ( cJSON_IsString( value ) && VALIDSTRING( value->valuestring ) ) {
		Q_strncpyz( outValue, value->valuestring, outValueSize );
	}

	cJSON_Delete( root );
}

qboolean G_SetAccountFlags( account_t* account, const int flags, qboolean flagsEnabled ) {
	const int oldFlags = account->flags;
	int newFlags = oldFlags;

	if (flagsEnabled) {
		newFlags |= flags;
	} else {
		newFlags &= ~flags;
	}

	// if there is no actual change, we don't need to do anything
	if (oldFlags == newFlags) {
		return qtrue;
	}

	G_DBSetAccountFlags( account, newFlags );

	return account->flags != oldFlags;
}
