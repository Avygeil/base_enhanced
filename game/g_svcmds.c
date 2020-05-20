// Copyright (C) 1999-2000 Id Software, Inc.
//

// this file holds commands that can be executed by the server console, but not remote clients

#include "g_local.h"
#include "g_database.h"

void Team_ResetFlags( void );
/*
==============================================================================

PACKET FILTERING
 

You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

g_filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.


==============================================================================
*/

typedef struct ipFilter_s
{
	unsigned	mask;
	unsigned	compare;
    char        comment[32];
} ipFilter_t;

// VVFIXME - We don't need this at all, but this is the quick way.
#ifdef _XBOX
#define	MAX_IPFILTERS	1
#else
#define	MAX_IPFILTERS	1024
#endif

// getstatus/getinfo bans
static ipFilter_t	getstatusIpFilters[MAX_IPFILTERS];
static int			getstatusNumIPFilters;

/*

ACCOUNT/PASSWORD system

*/

//#define MAX_ACCOUNTS	256
//
//typedef struct {
//	char username[MAX_USERNAME];
//	char password[MAX_PASSWORD];
//	int  inUse;
//	int  toDelete;
//} AccountItem;

//static AccountItem accounts[MAX_ACCOUNTS];
//static int accountsCount;

/*
int validateAccount(const char* username, const char* password, int num){
	int i;

	for(i=0;i<accountsCount;++i){
		if (!strcmp(accounts[i].username,username)
			&& (!password || !strcmp(accounts[i].password,password)) ){

			if ((accounts[i].inUse > -1) &&
				(((g_entities[ accounts[i].inUse ].client->ps.ping < 999) &&
				g_entities[ accounts[i].inUse ].client->ps.ping != -1)
				&& g_entities[ accounts[i].inUse ].client->pers.connected == CON_CONNECTED
				&& !strcmp(username,g_entities[ accounts[i].inUse ].client->pers.username)
				)){
				//check if that someone isnt 999
				return 2; 
			}

			//if ((accounts[i].inUse > -1))
			//	Com_Printf("data: %i %i %i",
			//		accounts[i].inUse,
			//		g_entities[ num ].client->ps.ping,
			//		g_entities[ num ].client->pers.connected);

			if (accounts[i].toDelete)
				return 1; //this account doesnt exist anymore

			accounts[i].inUse = num;
			return 0;
		}
	}

	return 1;
}
*/

/*
//save updated actual accounts to file
void saveAccounts(){
	fileHandle_t f;
	char entry[64]; // MAX_USERNAME + MAX_PASSWORD + 2 <= 64 !!! 
	int i;

	trap_FS_FOpenFile(g_accountsFile.string, &f, FS_WRITE);

	for(i=0;i<accountsCount;++i){
		if (accounts[i].toDelete)
			continue;

		Com_sprintf(entry,sizeof(entry),"%s:%s ",accounts[i].username,accounts[i].password);
		trap_FS_Write(entry, strlen(entry), f);

		//Com_Printf("Saved account %s %s to file.",accounts[i].username,accounts[i].password);
	}

	trap_FS_FCloseFile(f);
}
*/

/*
void unregisterUser(const char* username){
	int i;

	for(i=0;i<accountsCount;++i){
		if (!strcmp(accounts[i].username,username)){
			accounts[i].inUse = -1;
			return ;
		}
	}
}
*/

/*
qboolean addAccount(const char* username, const char* password, qboolean updateFile, qboolean checkExists){
	int i;

	if (checkExists){
		for(i=0;i<accountsCount;++i){
			if (!strcmp(accounts[i].username,username) )
				return qfalse; //account with this username already exists
		}
	}

	if (accountsCount >= MAX_ACCOUNTS)
		return qfalse; //too many accounts

	Q_strncpyz(accounts[accountsCount].username,username,MAX_USERNAME);
	Q_strncpyz(accounts[accountsCount].password,password,MAX_PASSWORD);
	accounts[accountsCount].inUse = -1;
	accounts[accountsCount].toDelete = qfalse;
	++accountsCount;

	if (updateFile)
		saveAccounts();

	return qtrue;
}
*/

/*
qboolean changePassword(const char* username, const char* password){
	int i;

	for(i=0;i<accountsCount;++i){
		if (!strcmp(accounts[i].username,username) ){
			Q_strncpyz(accounts[i].password,password,MAX_PASSWORD);

			G_LogPrintf("Account %s password changed.",username);
			saveAccounts();
		}
			
	}

	return qfalse; //not found

}
*/

/*
void removeAccount(const char* username){
	int i;

	for(i=0;i<accountsCount;++i){
		if (!strcmp(accounts[i].username,username)){
			accounts[i].toDelete = qtrue;
			saveAccounts();
			G_LogPrintf("Account %s removed.",username);
			return;
		}
	}
}
*/

/*
void loadAccounts(){
	fileHandle_t f;
	int len;
	char buffer[4096];
	char* entryEnd;
	char* rest;
	char* delim;
	int i;

	len = trap_FS_FOpenFile(g_accountsFile.string, &f, FS_READ);

	if (!f || len >= sizeof(buffer))
		return;

	trap_FS_Read(buffer, len, f);
	trap_FS_FCloseFile(f);

	//process
	accountsCount = 0;
	rest = buffer;

	//Com_Printf("Account File Content: %s\n",buffer);

	while(rest - buffer < len){
		entryEnd = strchr(rest,' ');
		if (!entryEnd){
			entryEnd = buffer + len;
		}
		*entryEnd = '\0';

		delim = strchr(rest,':');
		if (!delim){
			//Com_Printf("NOT FOUND delimiter.\n");
			return;
		}
		*delim = '\0';
		++delim;

		addAccount(rest,delim,qfalse,qfalse);

		//Com_Printf("Loaded account %s %s\n",rest,delim);

		rest = entryEnd + 1;
	}

	//go through all clients and mark them as used
	for(i=0;i<MAX_CLIENTS;++i){
		if (!g_entities[i].inuse || !g_entities[i].client)
			continue;

		if (g_entities[i].client->pers.connected != CON_CONNECTED)
			continue;

		validateAccount(g_entities[i].client->pers.username,0,i);
	}

	Com_Printf("Loaded %i accounts.\n",accountsCount);
}
*/


/*
=================
StringToFilter
=================
*/
static qboolean StringToFilter (char *s, char* comment, ipFilter_t *f)
{
	char	num[128];
	int		i, j;
	byte	b[4];
	byte	m[4];
	
	for (i=0 ; i<4 ; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}
	
	for (i=0 ; i<4 ; i++)
	{
		if (*s < '0' || *s > '9')
		{
			G_Printf( "Bad filter address: %s\n", s );
			return qfalse;
		}
		
		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}
	
	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;      
    Q_strncpyz(f->comment,comment ? comment : "none", sizeof(f->comment));

	return qtrue;
}

/*
=================
GetstatusUpdateIPBans
=================
*/
static void GetstatusUpdateIPBans (void)
{
	byte	b[4];
	int		i;
	char	iplist[MAX_INFO_STRING];

	*iplist = 0;
	for (i = 0 ; i < getstatusNumIPFilters ; i++)
	{
		if (getstatusIpFilters[i].compare == 0xffffffff)
			continue;

		*(unsigned *)b = getstatusIpFilters[i].compare;
		Com_sprintf( iplist + strlen(iplist), sizeof(iplist) - strlen(iplist), 
			"%i.%i.%i.%i ", b[0], b[1], b[2], b[3]);
	}

	trap_Cvar_Set( "g_getstatusbanIPs", iplist );
}

/*
=================
G_FilterPacket
=================
*/
qboolean G_FilterPacket( char *from, char* reasonBuffer, int reasonBufferSize )
{
    unsigned int ip = 0;
    getIpFromString( from, &ip );

	if ( g_whitelist.integer ) {
		return G_DBIsFilteredByWhitelist( ip, reasonBuffer, reasonBufferSize );
	} else {
		return G_DBIsFilteredByBlacklist( ip, reasonBuffer, reasonBufferSize );
	}
}

/*
=================
G_FilterPacket
=================
*/
qboolean G_FilterGetstatusPacket (unsigned int ip)
{
	int				i = 0;

	for (i=0 ; i<getstatusNumIPFilters ; i++)
		if ( (ip & getstatusIpFilters[i].mask) == getstatusIpFilters[i].compare)
			return qtrue;

	return qfalse;
}

qboolean getIpFromString( const char* from, unsigned int* ip )
{    
    if ( !(*from) || !(ip) )
    {
        return qfalse;
    }

    qboolean success = qfalse;
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0;

    // parse ip address and mask
    if ( sscanf( from, "%d.%d.%d.%d", &ipA, &ipB, &ipC, &ipD ) == 4  )
    {
        *ip = ((ipA << 24) & 0xFF000000) |
            ((ipB << 16) & 0x00FF0000) |
            ((ipC << 8) & 0x0000FF00) |
            (ipD & 0x000000FF);

        success = qtrue;
    }

    return success;
}

qboolean getIpPortFromString( const char* from, unsigned int* ip, int* port )
{
    if ( !(*from) || !(ip) || !(port) )
    {
        return qfalse;
    }

    qboolean success = qfalse;
    int ipA = 0, ipB = 0, ipC = 0, ipD = 0, ipPort = 0;

    // parse ip address and mask
    if ( sscanf( from, "%d.%d.%d.%d:%d", &ipA, &ipB, &ipC, &ipD, &ipPort ) == 5 )
    {
        *ip = ((ipA << 24) & 0xFF000000) |
            ((ipB << 16) & 0x00FF0000) |
            ((ipC << 8) & 0x0000FF00) |
            (ipD & 0x000000FF);

        *port = ipPort;
        success = qtrue;
    }

    return success;
}

void getStringFromIp( unsigned int ip, char* buffer, int size )
{
    Com_sprintf( buffer, size, "%d.%d.%d.%d",
        (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF );
}

/*
=================
GetstatusAddIP
=================
*/
static void GetstatusAddIP( char *str )
{
	int		i;

	for (i = 0 ; i < getstatusNumIPFilters ; i++)
		if (getstatusIpFilters[i].compare == 0xffffffff)
			break;		// free spot
	if (i == getstatusNumIPFilters)
	{
		if (getstatusNumIPFilters == MAX_IPFILTERS)
		{
			G_Printf ("Getstatus IP filter list is full\n");
			return;
		}
		getstatusNumIPFilters++;
	}
	
	if (!StringToFilter (str, 0, &getstatusIpFilters[i]))
		getstatusIpFilters[i].compare = 0xffffffffu;

	GetstatusUpdateIPBans();
}

/*
=================
G_ProcessGetstatusIPBans
=================
*/
void G_ProcessGetstatusIPBans(void) 
{
 	    char *s, *t;
	    char		str[MAX_TOKEN_CHARS];

	Q_strncpyz( str, g_getstatusbanIPs.string, sizeof(str) );

	for (t = s = g_getstatusbanIPs.string; *t; /* */ ) {
		    s = strchr(s, ' ');
		    if (!s)
			    break;
		    while (*s == ' ')
			    *s++ = 0;
		    if (*t)
			GetstatusAddIP( t );
		    t = s;
	    }      
    }

extern char	*ConcatArgs( int start );

/*
=================
Svcmd_AddIP_f
=================
*/
void Svcmd_AddIP_f (void)
{
    char ip[32];      
    char notes[32];
    char reason[32];
    int hours = 0;
    unsigned int ipInt;
    unsigned int maskInt;

    if ( trap_Argc() < 2 ) 
    {
        G_Printf( "Usage:  addip <ip> (mask) (notes) (reason) (hours)\n" );
        G_Printf( " ip - ip address in format X.X.X.X, do not use 0s!\n" );
        G_Printf( " mask - mask format X.X.X.X, defaults to 255.255.255.255\n" );
        G_Printf( " notes - notes only for admins, defaults to \"\"\n" );
        G_Printf( " reason - reason to be shown to banned player, defaults to \"Unknown\"\n" );
        G_Printf( " hours - duration in hours, defaults to g_defaultBanHoursDuration\n" );

        return;
    }   

    // set defaults
    maskInt = 0xFFFFFFFF;
    Q_strncpyz( notes, "", sizeof( notes ) );
    Q_strncpyz( reason, "Unknown", sizeof( reason ) );
    hours = g_defaultBanHoursDuration.integer;

    // set actuals
    trap_Argv( 1, ip, sizeof( ip ) );
    getIpFromString( ip, &ipInt );

    if ( trap_Argc() > 2 )
    {
        char mask[32];
        trap_Argv( 2, mask, sizeof( mask ) );
        getIpFromString( mask, &maskInt );
    }

    if ( trap_Argc() > 3 )
    {
        trap_Argv( 3, notes, sizeof( notes ) );
    }

    if ( trap_Argc() > 4 )
    {
        trap_Argv( 4, reason, sizeof( reason ) );
    }

    if ( trap_Argc() > 5 )
    {
        char hoursStr[16];
        trap_Argv( 5, hoursStr, sizeof( hoursStr ) );
        hours = atoi( hoursStr );        
    }                       

    if ( G_DBAddToBlacklist( ipInt, maskInt, notes, reason, hours ) )
    {
        G_Printf( "Added %s to blacklist successfuly.\n", ip );
        }
    else
    {
        G_Printf( "Failed to add %s to blacklist.\n", ip );
    }         
}

/*
=================
Svcmd_RemoveIP_f
=================
*/
void Svcmd_RemoveIP_f (void)
{
    char ip[32];
    unsigned int ipInt;
    unsigned int maskInt;

    if ( trap_Argc() < 2 )
    {
        G_Printf( "Usage:  removeip <ip> (mask)\n" );
        G_Printf( " ip - ip address in format X.X.X.X, do not use 0s!\n" );
        G_Printf( " mask - mask format X.X.X.X, defaults to 255.255.255.255\n" );

        return;
    }

    // set defaults
    maskInt = 0xFFFFFFFF;

    trap_Argv( 1, ip, sizeof( ip ) );
    getIpFromString( ip, &ipInt );

    if ( trap_Argc() > 2 )
    {
        char mask[32];
        trap_Argv( 2, mask, sizeof( mask ) );
        getIpFromString( mask, &maskInt );
    }   

    if ( G_DBRemoveFromBlacklist( ipInt, maskInt ) )
    {
        G_Printf( "Removed %s from blacklist successfuly.\n", ip );
    }
    else
    {
        G_Printf( "Failed to remove %s from blacklist.\n", ip );
    }
}


/*
=================
Svcmd_AddWhiteIP_f
=================
*/
void Svcmd_AddWhiteIP_f( void )
{
    char ip[32];
    char notes[32];
    unsigned int ipInt;
    unsigned int maskInt;

    if ( trap_Argc() < 2 )
    {
        G_Printf( "Usage:  addwhiteip <ip> (mask) (notes)\n" );
        G_Printf( " ip - ip address in format X.X.X.X, do not use 0s!\n" );
        G_Printf( " mask - mask format X.X.X.X, defaults to 255.255.255.255\n" );
        G_Printf( " notes - notes only for admins, defaults to \"\"\n" );

        return;
    }

    // set defaults
    maskInt = 0xFFFFFFFF;
    Q_strncpyz( notes, "", sizeof( notes ) );

    trap_Argv( 1, ip, sizeof( ip ) );
    getIpFromString( ip, &ipInt );

    if ( trap_Argc() > 2 )
    {
        char mask[32];
        trap_Argv( 2, mask, sizeof( mask ) );
        getIpFromString( mask, &maskInt );
    }

    if ( trap_Argc() > 3 )
    {
        trap_Argv( 3, notes, sizeof( notes ) );
    }          

    if ( G_DBAddToWhitelist( ipInt, maskInt, notes ) )
    {
        G_Printf( "Added %s to whitelist successfuly.\n", ip );
    }
    else
    {
        G_Printf( "Failed to add %s to whitelist.\n", ip );
    }
}

/*
=================
Svcmd_RemoveWhiteIP_f
=================
*/
void Svcmd_RemoveWhiteIP_f( void )
{
    char ip[32];
    
    unsigned int ipInt;
    unsigned int maskInt;

    if ( trap_Argc() < 2 )
    {
        G_Printf( "Usage:  removewhiteip <ip> (mask)\n" );
        G_Printf( " ip - ip address in format X.X.X.X, do not use 0s!\n" );
        G_Printf( " mask - mask format X.X.X.X, defaults to 255.255.255.255\n" );

        return;
    }

    // set defaults
    maskInt = 0xFFFFFFFF;

    trap_Argv( 1, ip, sizeof( ip ) );
    getIpFromString( ip, &ipInt );

    if ( trap_Argc() > 2 )
    {
        char mask[32];    
        trap_Argv( 2, mask, sizeof( mask ) );

        getIpFromString( mask, &maskInt );
    }         

    if ( G_DBRemoveFromWhitelist( ipInt, maskInt ) )
    {
        G_Printf( "Removed %s from whitelist successfuly.\n", ip );
    }
    else
    {
        G_Printf( "Failed to remove %s from whitelist.\n", ip );
    }
}


void (listCallback)( unsigned int ip,
    unsigned int mask,
    const char* notes,
    const char* reason,
    const char* banned_since,
    const char* banned_until )
{
    G_Printf( "%d.%d.%d.%d %d.%d.%d.%d \"%s\" \"%s\" %s %s\n",
        (ip >> 24) & 0xFF,
        (ip >> 16) & 0xFF,
        (ip >> 8) & 0xFF,
        (ip) & 0xFF,
        (mask >> 24) & 0xFF,
        (mask >> 16) & 0xFF,
        (mask >> 8) & 0xFF,
        (mask) & 0xFF,
        notes, reason, banned_since, banned_until );
}

/*
=================
Svcmd_Listip_f
=================
*/
void Svcmd_Listip_f (void)
{
    G_Printf( "ip mask notes reason banned_since banned_until\n" );   

    G_DBListBlacklist( listCallback );
}

		

/*
===================
Svcmd_EntityList_f
===================
*/
void	Svcmd_EntityList_f (void) {
	int			e;
	gentity_t		*check;

	check = g_entities+1;
	for (e = 1; e < level.num_entities ; e++, check++) {
		if ( !check->inuse ) {
			continue;
		}
		G_Printf("%3i:", e);
		switch ( check->s.eType ) {
		case ET_GENERAL:
			G_Printf("ET_GENERAL          ");
			break;
		case ET_PLAYER:
			G_Printf("ET_PLAYER           ");
			break;
		case ET_ITEM:
			G_Printf("ET_ITEM             ");
			break;
		case ET_MISSILE:
			G_Printf("ET_MISSILE          ");
			break;
		case ET_MOVER:
			G_Printf("ET_MOVER            ");
			break;
		case ET_BEAM:
			G_Printf("ET_BEAM             ");
			break;
		case ET_PORTAL:
			G_Printf("ET_PORTAL           ");
			break;
		case ET_SPEAKER:
			G_Printf("ET_SPEAKER          ");
			break;
		case ET_PUSH_TRIGGER:
			G_Printf("ET_PUSH_TRIGGER     ");
			break;
		case ET_TELEPORT_TRIGGER:
			G_Printf("ET_TELEPORT_TRIGGER ");
			break;
		case ET_INVISIBLE:
			G_Printf("ET_INVISIBLE        ");
			break;
		case ET_NPC:
			G_Printf("ET_NPC              ");
			break;
		default:
			G_Printf("%3i                 ", check->s.eType);
			break;
		}

		if ( check->classname ) {
			G_Printf("%s", check->classname);
		}
		G_Printf("\n");
	}
}

gclient_t	*ClientForString( const char *s ) {
	gclient_t	*cl;
	int			i;
	int			idnum;

	// numeric values are just slot numbers
	if ( s[0] >= '0' && s[0] <= '9' ) {
		idnum = atoi( s );
		if ( idnum < 0 || idnum >= level.maxclients ) {
			Com_Printf( "Bad client slot: %i\n", idnum );
			return NULL;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected == CON_DISCONNECTED ) {
			G_Printf( "Client %i is not connected\n", idnum );
			return NULL;
		}
		return cl;
	}

	// check for a name match
	for ( i=0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( !Q_stricmp( cl->pers.netname, s ) ) {
			return cl;
		}
	}

	G_Printf( "User %s is not on the server\n", s );

	return NULL;
}

/*
===================
Svcmd_ForceTeam_f

forceteam <player> <team>
===================
*/
void	Svcmd_ForceTeam_f( void ) {
	gentity_t	*ent;
	gclient_t	*cl;
	char		str[MAX_TOKEN_CHARS];

	// find the player
	trap_Argv( 1, str, sizeof( str ) );
	cl = ClientForString( str );
	if ( !cl ) {
		return;
	}

	ent = &g_entities[cl - level.clients];

	// set the team
	trap_Argv( 2, str, sizeof( str ) );

	// this always disables racemode
	G_SetRaceMode( ent, qfalse );

	if (cl->sess.canJoin) {
		SetTeam(ent, str);
	} else {
		cl->sess.canJoin = qtrue; // Admins can force passwordless spectators on a team
		SetTeam(ent, str);
		cl->sess.canJoin = qfalse;
	}
}

void Svcmd_ResetFlags_f(){
	gentity_t*	ent;
	int i;

	for (i = 0; i < 32; ++i ){
		ent = g_entities+i;
		
		if (!ent->inuse || !ent->client )
			continue;

		// no resetting for racemode clients
		if ( ent->client->sess.inRacemode ) {
			continue;
		}

		ent->client->ps.powerups[PW_BLUEFLAG] = 0;
		ent->client->ps.powerups[PW_REDFLAG] = 0;
	}
	Team_ResetFlags();
}

void Svcmd_AllReady_f(void) {
	if (!level.warmupTime) {
		G_Printf("allready is only available during warmup\n");
		return;
	}

	level.warmupTime = level.time;
}

void Svcmd_LockTeams_f( void ) {
	char	str[MAX_TOKEN_CHARS];
	int		n = -1;

	if ( trap_Argc() < 2 ) {
		return;
	}

	trap_Argv( 1, str, sizeof( str ) );

	// could do some advanced parsing, but since we only have to handle a few cases...
	if ( !Q_stricmp( str, "0" ) || !Q_stricmpn( str, "r", 1 ) ) {
		n = 0;
	} else if ( !Q_stricmp( str, "4s" ) ) {
		n = 8;
	} else if ( !Q_stricmp( str, "5s" ) ) {
		n = 10;
	}

	if ( n < 0 ) {
		return;
	}

	trap_Cvar_Set( "g_maxgameclients", va( "%i", n ) );

	if ( !n ) {
		trap_SendServerCommand( -1, "print \""S_COLOR_GREEN"Teams were unlocked.\n\"");
	} else {
		trap_SendServerCommand( -1, va( "print \""S_COLOR_RED"Teams were locked to %i vs %i.\n\"", n / 2, n / 2 ) );
	}
}

void Svcmd_Cointoss_f(void) 
{
	int cointoss = rand() % 2;

	trap_SendServerCommand(-1, va("cp \""S_COLOR_YELLOW"%s"S_COLOR_WHITE"!\"", cointoss ? "Heads" : "Tails"));
	trap_SendServerCommand(-1, va("print \"Coin Toss result: "S_COLOR_YELLOW"%s\n\"", cointoss ? "Heads" : "Tails"));
}

void Svcmd_ForceName_f(void) {
	if ( trap_Argc() < 4 ) {
		Com_Printf( "Usage: forcename <player> <duration> <new name>\n" );
		return;
	}

	gentity_t	*found = NULL;
	char		str[MAX_TOKEN_CHARS];
	int			duration;

	// find the player
	trap_Argv( 1, str, sizeof( str ) );
	found = G_FindClient( str );
	if ( !found || !found->client ) {
		Com_Printf( "Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", str );
		return;
	}

	trap_Argv( 2, str, sizeof( str ) );
	duration = atoi( str ) * 1000;

	SetNameQuick( found, ConcatArgs( 3 ), duration );
}

void Svcmd_ShadowMute_f( void ) {
	gentity_t	*found = NULL;
	gclient_t	*cl;
	char		str[MAX_TOKEN_CHARS];

	// find the player
	trap_Argv( 1, str, sizeof( str ) );
	found = G_FindClient(str);
	if (!found || !found->client) {
		Com_Printf("Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n", str);
		return;
	}
	cl = found->client;

	cl->sess.shadowMuted = !cl->sess.shadowMuted;

	if (cl->sess.shadowMuted) {
		G_Printf("Client %d (%s"S_COLOR_WHITE") is now shadowmuted.\n", cl - level.clients, cl->pers.netname);
	}
	else {
		G_Printf("Client %d (%s"S_COLOR_WHITE") is no longer shadowmuted.\n", cl - level.clients, cl->pers.netname);
	}
}

void Svcmd_VoteForce_f( qboolean pass ) {
	if ( !level.voteTime ) {
		G_Printf("No vote in progress.\n");
		return;
	}

	trap_SendServerCommand( -1, va( "print \""S_COLOR_RED"Vote forced to %s.\n\"", pass ? "pass" : "fail" ) );

	if ( pass ) {
		level.voteExecuteTime = level.time + 3000;
	}

	if ( level.multiVoting ) {
		// reset global cp so it doesnt show up more than needed since we forced the vote to end
		G_GlobalTickedCenterPrint( "", 0, qfalse );
	}

	if ( !level.multiVoting ) {
		g_entities[level.lastVotingClient].client->lastCallvoteTime = level.time;
	} else if ( !pass ) {
		level.multiVoting = qfalse;
		level.multiVoteChoices = 0;
		memset( level.multiVotes, 0, sizeof( level.multiVotes ) );
	}

	level.voteTime = 0;
	trap_SetConfigstring( CS_VOTE_TIME, "" );
}

#ifdef _WIN32
#define FS_RESTART_ADDR 0x416800
#else
#define FS_RESTART_ADDR 0x81313B4
#endif

#include "jp_engine.h"

//#ifdef _WIN32
	void (*FS_Restart)( int checksumFeed ) = (void (*)( int  ))FS_RESTART_ADDR;
//#else
//#endif

extern void G_LoadArenas( void );
void Svcmd_FSReload_f(){

#ifdef _WIN32
	int feed = rand();
	FS_Restart(feed);
#else
	//LINUX NOT YET SUPPORTED
	int feed = rand();
	FS_Restart(feed);

#endif

	G_LoadArenas();
}


#ifndef _WIN32
#include <sys/utsname.h>
#endif

void Svcmd_Osinfo_f(){

#ifdef _WIN32
	Com_Printf("System: Windows\n");
#else
	struct utsname buf;
	uname(&buf);
	Com_Printf("System: Linux\n");
	Com_Printf("System name: %s\n",buf.sysname);
	Com_Printf("Node: %s\n",buf.nodename);
	Com_Printf("Release: %s\n",buf.release);
	Com_Printf("Version: %s\n",buf.version);
	Com_Printf("Machine: %s\n",buf.machine);
#endif




}

void Svcmd_ClientBan_f(){
	char clientId[4];
    char ip[16];
	int id;

	trap_Argv(1,clientId,sizeof(clientId));
	id = atoi(clientId);

	if (id < 0 || id >= MAX_CLIENTS)
		return;

	if (g_entities[id].client->pers.connected == CON_DISCONNECTED)
		return;

	if (g_entities[id].r.svFlags & SVF_BOT)
		return;


    getStringFromIp( g_entities[id].client->sess.ip, ip, sizeof( ip ) );

    trap_SendConsoleCommand( EXEC_APPEND, va( "addip %s\n", ip ) );
	trap_SendConsoleCommand(EXEC_APPEND, va("clientkick %i\n",id));

}

/*
void Svcmd_AddAccount_f(){
	char username[MAX_USERNAME];
	char password[MAX_PASSWORD];

	if (trap_Argc() < 3){
		Com_Printf("Too few arguments. Usage: accountadd [username] [password]\n");
		return;
	}

	trap_Argv(1,username,MAX_USERNAME);
	trap_Argv(2,password,MAX_PASSWORD);

	if (addAccount(username,password,qtrue,qtrue))
		G_LogPrintf("Account %s added.",username);
}
*/

/*
void Svcmd_RemoveAccount_f(){
	char username[MAX_USERNAME];

	if (trap_Argc() < 2){
		Com_Printf("Too few arguments. Usage: accountremove [username]\n");
		return;
	}

	trap_Argv(1,username,MAX_USERNAME);

	removeAccount(username);
}
*/

/*
void Svcmd_ChangeAccount_f(){
	char username[MAX_USERNAME];
	char password[MAX_PASSWORD];

	if (trap_Argc() < 3){
		Com_Printf("Too few arguments. Usage: accountchange [username] [newpassword]\n");
		return;
	}

	trap_Argv(1,username,MAX_USERNAME);
	trap_Argv(2,password,MAX_PASSWORD);

	changePassword(username,password);
}
*/

/*
void Svcmd_ReloadAccount_f(){
	loadAccounts();
}
*/

#define Q_IsColorStringStats(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE && *((p)+1) <= '7' && *((p)+1) >= '0' )
int CG_DrawStrlenStats( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorStringStats( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

void Svcmd_AccountPrint_f(){
	int i;

	Com_Printf("id nick                             username                                 ip\n");

	for(i=0;i<level.maxclients;++i){
		if (!g_entities[i].inuse || !g_entities[i].client)
			continue;

		Com_Printf("%2i %-*s^7 %-*s %+*s\n",
			i,
			32 + strlen(g_entities[i].client->pers.netname) - CG_DrawStrlenStats(g_entities[i].client->pers.netname),g_entities[i].client->pers.netname,
			16 + strlen(g_entities[i].client->sess.username) - CG_DrawStrlenStats(g_entities[i].client->sess.username),g_entities[i].client->sess.username,
			26 + strlen(g_entities[i].client->sess.ipString) - CG_DrawStrlenStats(g_entities[i].client->sess.ipString),g_entities[i].client->sess.ipString
			);

	}

}

/*
int accCompare(const void* index1, const void* index2){
	return stricmp(accounts[*(int *)index1].username,accounts[*(int *)index2].username);
}
*/

/*
void Svcmd_AccountPrintAll_f(){
	int i;
	int sorted[MAX_ACCOUNTS];

	for(i=0;i<accountsCount;++i){
		sorted[i] = i;
	}
	qsort(sorted, accountsCount, sizeof(int),accCompare);


	for(i=0;i<accountsCount;++i){
		Com_Printf("%s\n",accounts[sorted[i]].username);
	}
	Com_Printf("%i accounts listed.\n",accountsCount);
}
*/

extern void fixVoters(qboolean allowRacers);

// starts a multiple choices vote using some of the binary voting logic
static void StartMultiMapVote( const int numMaps, const qboolean hasWildcard, const char *listOfMaps ) {
	if ( level.voteTime ) {
		// stop the current vote because we are going to replace it
		level.voteTime = 0;
		g_entities[level.lastVotingClient].client->lastCallvoteTime = level.time;
	}

	G_LogPrintf( "A multi map vote was started: %s\n", listOfMaps );

	// start a "fake vote" so that we can use most of the logic that already exists
	Com_sprintf( level.voteString, sizeof( level.voteString ), "map_multi_vote %s", listOfMaps );
	Q_strncpyz( level.voteDisplayString, S_COLOR_RED"Vote for a map in console", sizeof( level.voteDisplayString ) );
	level.voteTime = level.time;
	level.voteYes = 0;
	level.voteNo = 0;
	// we don't set lastVotingClient since this isn't a "normal" vote
	level.multiVoting = qtrue;
	level.multiVoteHasWildcard = hasWildcard;
	level.multiVoteChoices = numMaps;
	memset( &( level.multiVotes ), 0, sizeof( level.multiVotes ) );

	fixVoters( qfalse ); // racers can never vote in multi map votes

	trap_SetConfigstring( CS_VOTE_TIME, va( "%i", level.voteTime ) );
	trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );
	trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
	trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
}

typedef struct {
	char listOfMaps[MAX_STRING_CHARS];
	char printMessage[MAX_STRING_CHARS];
	qboolean announceMultiVote;
	qboolean hasWildcard;
	int mapsToRandomize;
	int numSelected;
} MapSelectionContext;

extern const char *G_GetArenaInfoByMap( const char *map );

static void mapSelectedCallback( void *context, char *mapname ) {
	MapSelectionContext *selection = ( MapSelectionContext* )context;

	// build a whitespace separated list ready to be passed as arguments in voteString if needed
	if ( VALIDSTRING( selection->listOfMaps ) ) {
		Q_strcat( selection->listOfMaps, sizeof( selection->listOfMaps ), " " );
		Q_strcat( selection->listOfMaps, sizeof( selection->listOfMaps ), mapname );
	} else {
		Q_strncpyz( selection->listOfMaps, mapname, sizeof( selection->listOfMaps ) );
	}

	selection->numSelected++;

	if ( selection->announceMultiVote ) {
		if ( selection->numSelected == 1 ) {
			Q_strncpyz( selection->printMessage, "Vote for a map to increase its probability:", sizeof( selection->printMessage ) );
		}

		if ( selection->hasWildcard && selection->mapsToRandomize == selection->numSelected ) {
			// the wildcard option is always the last option printed
			// the only difference behind the scenes is that it doesn't display the name of the map

			Q_strcat( selection->printMessage, sizeof( selection->printMessage ),
				va( "\n"S_COLOR_CYAN"/vote %d "S_COLOR_WHITE" - "S_COLOR_YELLOW"A random map not above", selection->numSelected )
			);
		} else {
			// otherwise this is a standard option
			// try to print the full map name to players (if a full name doesn't appear, map has no .arena or isn't installed)

			char *mapDisplayName = NULL;
			const char *arenaInfo = G_GetArenaInfoByMap( mapname );

			if ( arenaInfo ) {
				mapDisplayName = Info_ValueForKey( arenaInfo, "longname" );
				Q_CleanStr( mapDisplayName );
			}

			if ( !VALIDSTRING( mapDisplayName ) ) {
				mapDisplayName = mapname;
			}

			Q_strcat( selection->printMessage, sizeof( selection->printMessage ),
				va( "\n"S_COLOR_CYAN"/vote %d "S_COLOR_WHITE" - %s", selection->numSelected, mapDisplayName )
			);
		}
		
	}
}

extern void CountPlayersIngame( int *total, int *ingame, int *inrace );

void Svcmd_MapRandom_f()
{
	char pool[64];
	int mapsToRandomize;

	if (trap_Argc() < 2) {
		return;
	}

    trap_Argv( 1, pool, sizeof( pool ) );

	if ( trap_Argc() < 3 ) {
		// default to 1 map if no argument
		mapsToRandomize = 1;
	} else {
		// get it from the argument then
		char mapsToRandomizeBuf[64];
		trap_Argv( 2, mapsToRandomizeBuf, sizeof( mapsToRandomizeBuf ) );

		mapsToRandomize = atoi( mapsToRandomizeBuf );
		if ( mapsToRandomize < 1 ) mapsToRandomize = 1; // we don't want negative/zero maps. the upper limit is set in callvote code so rcon can do whatever
	}

	if ( mapsToRandomize > 1 ) {
		int ingame;
		CountPlayersIngame( NULL, &ingame, NULL );
		if ( ingame < 2 ) { // how? this should have been handled in callvote code, maybe some stupid admin directly called it with nobody in game..?
							// or it could also be caused by people joining spec before map_random passes... so inform them, i guess
			G_Printf( "Not enough people in game to start a multi vote, a single map will be randomized\n" );
			mapsToRandomize = 1;
		}
	}

	const char* currentMap = level.mapname;
	
	MapSelectionContext context;
	memset( &context, 0, sizeof( context ) );

	if ( mapsToRandomize > 1 ) { // if we are randomizing more than one map, there will be a second vote
		if ( level.multiVoting && level.voteTime ) {
			return; // theres already a multi vote going on, retard
		}

		if ( level.voteExecuteTime && level.voteExecuteTime < level.time ) {
			return; // another vote just passed and is waiting to execute, don't interrupt it...
		}

		context.announceMultiVote = qtrue;

		// in order to make the option work, we randomize the wildcard map here along with the other choices
		// this avoids having to randomize it later while excluding the other choices
		// so even though the wildcard option is actually "pre-determined", but hidden, the effect is the same
		if ( g_enable_maprandom_wildcard.integer ) {
			context.hasWildcard = qtrue;
			++mapsToRandomize;
		}
	}

	context.mapsToRandomize = mapsToRandomize;

	if ( G_DBSelectMapsFromPool( pool, currentMap, context.mapsToRandomize, mapSelectedCallback, &context ) )
    {
		if ( context.numSelected != context.mapsToRandomize ) {
			G_Printf( "Could not randomize this many maps! Expected %d, but randomized %d\n", context.mapsToRandomize, context.numSelected );
		}

		if ( VALIDSTRING( context.printMessage ) ) {
			// print in console and do a global prioritized center print
			trap_SendServerCommand( -1, va( "print \"%s\n\"", context.printMessage ) );
			G_GlobalTickedCenterPrint( context.printMessage, 10000, qtrue ); // give them 10s to see the options
		}

		if ( context.numSelected > 1 ) {
			// we are going to need another vote for this...
			StartMultiMapVote( context.numSelected, context.hasWildcard, context.listOfMaps );
		} else {
			// we have 1 map, this means listOfMaps only contains 1 randomized map. Just change to it straight away.
			trap_SendConsoleCommand( EXEC_APPEND, va( "map %s\n", context.listOfMaps ) );
		}

		return;
    }

    G_Printf( "Map pool '%s' not found\n", pool );
}

// allocates an array of length numChoices containing the sorted results from level.multiVoteChoices
// don't forget to FREE THE RESULT
int* BuildVoteResults( const int numChoices, int *numVotes, int *highestVoteCount ) {
	int i;
	int *voteResults = calloc( numChoices, sizeof( *voteResults ) ); // voteResults[i] = how many votes for the i-th choice

	if ( numVotes ) *numVotes = 0;
	if ( highestVoteCount ) *highestVoteCount = 0;

	for ( i = 0; i < MAX_CLIENTS; ++i ) {
		int voteId = level.multiVotes[i];

		if ( voteId > 0 && voteId <= numChoices ) {
			// one more valid vote...
			if ( numVotes ) ( *numVotes )++;
			voteResults[voteId - 1]++;

			if ( highestVoteCount && voteResults[voteId - 1] > *highestVoteCount ) {
				*highestVoteCount = voteResults[voteId - 1];
			}
		} else if ( voteId ) { // shouldn't happen since we check that in /vote...
			G_LogPrintf( "Invalid multi vote id for client %d: %d\n", i, voteId );
		}
	}

	return voteResults;
}

static void PickRandomMultiMap( const int *voteResults, const int numChoices, const int numVotingClients,
	const int numVotes, const int highestVoteCount, qboolean *hasWildcard, char *out, size_t outSize ) {
	int i;

	if ( highestVoteCount >= ( numVotingClients / 2 ) + 1 ) {
		// one map has a >50% majority, find it and pass it
		for ( i = 0; i < numChoices; ++i ) {
			if ( voteResults[i] == highestVoteCount ) {
				trap_Argv( i + 1, out, outSize );
				return;
			}
		}
	}

	// now, since the amount of votes is pretty low (always <= MAX_CLIENTS) we can just build a uniformly distributed function
	// this isn't the fastest but it is more readable
	int *udf;
	int random;

	if ( !numVotes ) {
		// if nobody voted, just give all maps a weight of 1, ie the same probability
		udf = malloc( sizeof( *udf ) * numChoices );

		for ( i = 0; i < numChoices; ++i ) {
			udf[i] = i + 1;
		}

		random = rand() % numChoices;
	} else {
		// make it an array where each item appears as many times as they were voted, thus giving weight (0 votes = 0%)
		int items = numVotes, currentItem = 0;
		udf = malloc( sizeof( *udf ) * items );

		for ( i = 0; i < numChoices; ++i ) {
			// 1. special case for wildcard vote: if it has the highest amount of votes, it either passed with >50% majority earlier,
			// or if we got here it could be tied with other votes for the highest amount of votes: for this case, rule out all
			// votes that aren't tied with it
			// 2. otherwise, if the wildcard vote doesn't have the highest amount of votes, discard its votes
			// 3. rule out very low vote counts relatively to the highest one and the max voting clients
			if (
				( *hasWildcard && voteResults[numChoices - 1] == highestVoteCount && voteResults[i] != highestVoteCount ) ||
				( *hasWildcard && voteResults[numChoices - 1] != highestVoteCount && i == numChoices - 1 ) ||
				( highestVoteCount - voteResults[i] > ( numVotingClients / 4 ) ) ) {
				items -= voteResults[i];
				udf = realloc( udf, sizeof( *udf ) * items );
				continue;
			}

			int j;

			for ( j = 0; j < voteResults[i]; ++j ) {
				udf[currentItem++] = i + 1;
			}
		}

		random = rand() % items;
	}

	// since the array is uniform, we can just pick an index to have a weighted map pick
	const int result = udf[random];
	free( udf );

	if ( *hasWildcard && result != numChoices ) {
		// if the wildcard vote didn't pass, change the upstream value to hide it from the results
		*hasWildcard = qfalse;
	}

	trap_Argv( result, out, outSize );
}

void Svcmd_MapMultiVote_f() {
	if ( !level.multiVoting || !level.multiVoteChoices ) {
		return; // this command should only be run by the callvote code
	}

	if ( trap_Argc() - 1 != level.multiVoteChoices ) { // wtf? this should never happen...
		G_LogPrintf( "MapMultiVote failed: argc=%d != multiVoteChoices=%d\n", trap_Argc() - 1, level.multiVoteChoices );
		return;
	}

	// get the results and pick a map
	int numVotes, highestVoteCount;
	int *voteResults = BuildVoteResults( level.multiVoteChoices, &numVotes, &highestVoteCount );

	char selectedMapname[MAX_MAP_NAME];
	qboolean hasWildcard = level.multiVoteHasWildcard; // changes based on result (since the wildcard vote can only either pass or be discarded)
	PickRandomMultiMap( voteResults, level.multiVoteChoices, level.numVotingClients, numVotes, highestVoteCount, &hasWildcard, selectedMapname, sizeof( selectedMapname ) );

	// build a string to show the idiots what they voted for
	int i;
	char resultString[MAX_STRING_CHARS] = S_COLOR_WHITE"Map voting results:";
	for ( i = 0; i < level.multiVoteChoices; ++i ) {
		// if this was a wildcard vote and the wildcard failed, hide the wildcard option from results
		if ( level.multiVoteHasWildcard && !hasWildcard && i == level.multiVoteChoices - 1 ) {
			continue;
		}

		char mapname[MAX_MAP_NAME];
		trap_Argv( 1 + i, mapname, sizeof( mapname ) );

		// get the full name of the map
		char *mapDisplayName = NULL;
		const char *arenaInfo = G_GetArenaInfoByMap( mapname );

		if ( arenaInfo ) {
			mapDisplayName = Info_ValueForKey( arenaInfo, "longname" );
			Q_CleanStr( mapDisplayName );
		}

		if ( !VALIDSTRING( mapDisplayName ) ) {
			mapDisplayName = &mapname[0];
		}

		Q_strcat( resultString, sizeof( resultString ), va( "\n%s%s - %d vote%s",
			!Q_stricmp(mapname, selectedMapname) ? S_COLOR_GREEN : S_COLOR_WHITE, // the selected map is green
			mapDisplayName, voteResults[i], voteResults[i] != 1 ? "s" : "" )
		);
	}

	free( voteResults );

	// do both a console print and global prioritized center print
	if ( VALIDSTRING( resultString ) ) {
		trap_SendServerCommand( -1, va( "print \"%s\n\"", resultString ) );
		G_GlobalTickedCenterPrint( resultString, 3000, qtrue ); // show it for 3s, the time before map changes
	}

	// re-use the vote timer so that there's a small delay before map change
	Q_strncpyz( level.voteString, va( "map %s", selectedMapname ), sizeof( level.voteString ) );
	level.voteExecuteTime = level.time + 3000;
	level.multiVoting = qfalse;
	level.multiVoteHasWildcard = qfalse;
	level.multiVoteChoices = 0;
	memset( level.multiVotes, 0, sizeof( level.multiVotes ) );
}

void Svcmd_SpecAll_f() {
    int i;

    for (i = 0; i < level.maxclients; i++) {
        if (!g_entities[i].inuse || !g_entities[i].client) {
            continue;
        }

        if ((g_entities[i].client->sess.sessionTeam == TEAM_BLUE) || (g_entities[i].client->sess.sessionTeam == TEAM_RED)) {
            trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i s\n", i));
        }
    }
}

void Svcmd_RandomCapts_f() {
    int ingame[32], spectator[32], i, numberOfIngamePlayers = 0, numberOfSpectators = 0, randNum1, randNum2;

	// TODO: ignore passwordless specs
    for (i = 0; i < level.maxclients; i++) {
        if (!g_entities[i].inuse || !g_entities[i].client) {
            continue;
        }

        if (g_entities[i].client->sess.sessionTeam == TEAM_SPECTATOR) {
            spectator[numberOfSpectators] = i;
            numberOfSpectators++;
        }
        else if ((g_entities[i].client->sess.sessionTeam == TEAM_BLUE) || (g_entities[i].client->sess.sessionTeam == TEAM_RED)) {
            ingame[numberOfIngamePlayers] = i;
            numberOfIngamePlayers++;
        }
    }

    if (numberOfIngamePlayers + numberOfSpectators < 2) {
        trap_SendServerCommand(-1, "print \"^1Not enough players on the server.\n\"");
        return;
    }

    if (numberOfIngamePlayers == 0) {
        randNum1 = rand() % numberOfSpectators;
        randNum2 = (randNum1 + 1 + (rand() % (numberOfSpectators - 1))) % numberOfSpectators;

        trap_SendServerCommand(-1, va("print \"^7The captain for team ^1RED ^7 is: %s\n\"", g_entities[spectator[randNum1]].client->pers.netname));
        trap_SendServerCommand(-1, va("print \"^7The captain for team ^4BLUE ^7 is: %s\n\"", g_entities[spectator[randNum2]].client->pers.netname));
    }
    else if (numberOfIngamePlayers == 1) {
        randNum1 = rand() % numberOfSpectators;

        if (g_entities[ingame[0]].client->sess.sessionTeam == TEAM_RED) {
            trap_SendServerCommand(-1, va("print \"^7The captain for team ^4BLUE ^7 is: %s\n\"", g_entities[spectator[randNum1]].client->pers.netname));
        }
        else if (g_entities[ingame[0]].client->sess.sessionTeam == TEAM_BLUE) {
            trap_SendServerCommand(-1, va("print \"^7The captain for team ^1RED ^7 is: %s\n\"", g_entities[spectator[randNum1]].client->pers.netname));
        }
    }
    else if (numberOfIngamePlayers == 2) {
        if (g_entities[ingame[0]].client->sess.sessionTeam != TEAM_RED) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i r\n", ingame[0]));
            SetTeam(&g_entities[ingame[0]], "red");
        }
        if (g_entities[ingame[1]].client->sess.sessionTeam != TEAM_BLUE) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i b\n", ingame[1]));
            SetTeam(&g_entities[ingame[1]], "blue");
        }
    }
    else {
        randNum1 = rand() % numberOfIngamePlayers;
        randNum2 = (randNum1 + 1 + (rand() % (numberOfIngamePlayers - 1))) % numberOfIngamePlayers;

        if (g_entities[ingame[randNum1]].client->sess.sessionTeam != TEAM_RED) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i r\n", ingame[randNum1]));
            SetTeam(&g_entities[ingame[randNum1]], "red");
        }
        if (g_entities[ingame[randNum2]].client->sess.sessionTeam != TEAM_BLUE) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i b\n", ingame[randNum2]));
            SetTeam(&g_entities[ingame[randNum2]], "blue");
        }

        int i;
        for ( i = 0; i < numberOfIngamePlayers; i++) {
            if ((i == randNum1) || (i == randNum2)) {
                continue;
            }

            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i s\n", ingame[i]));
            SetTeam(&g_entities[ingame[i]], "spectator");
        }
    }
}

void Svcmd_RandomTeams_f() {
    int i, j, temp, numberOfReadyPlayers = 0, numberOfOtherPlayers = 0;
    int otherPlayers[32], readyPlayers[32];
    int team1Count, team2Count;
    char count[2];

	// TODO: ignore passwordless specs
    if (trap_Argc() < 3) {
        return;
    }

    trap_Argv(1, count, sizeof(count));
    team1Count = atoi(count);

    trap_Argv(2, count, sizeof(count));
    team2Count = atoi(count);

    if ((team1Count <= 0) || (team2Count <= 0)) {
        return;
    }

    for (i = 0; i < level.maxclients; i++) {
        if (!g_entities[i].inuse || !g_entities[i].client) {
            continue;
        }

        if (!g_entities[i].client->pers.ready) {
            otherPlayers[numberOfOtherPlayers] = i;
            numberOfOtherPlayers++;
            continue;
        }

        readyPlayers[numberOfReadyPlayers] = i;
        numberOfReadyPlayers++;
    }

    if (numberOfReadyPlayers < team1Count + team2Count) {
        trap_SendServerCommand(-1, va("print \"^1Not enough ready players on the server: %i\n\"", numberOfReadyPlayers));
        return;
    }

    // fisher-yates shuffle algorithm
    for (i = numberOfReadyPlayers - 1; i >= 1; i--) {
        j = rand() % (i + 1);
        temp = readyPlayers[i];
        readyPlayers[i] = readyPlayers[j];
        readyPlayers[j] = temp;
    }

    for (i = 0; i < team1Count; i++) {
        if (g_entities[readyPlayers[i]].client->sess.sessionTeam != TEAM_RED) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i r\n", readyPlayers[i]));
            SetTeam(&g_entities[readyPlayers[i]], "red");
        }
    }
    for (i = team1Count; i < team1Count + team2Count; i++) {
        if (g_entities[readyPlayers[i]].client->sess.sessionTeam != TEAM_BLUE) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i b\n", readyPlayers[i]));
            SetTeam(&g_entities[readyPlayers[i]], "blue");
        }
    }
    for (i = team1Count + team2Count; i < numberOfReadyPlayers; i++) {
        if (g_entities[readyPlayers[i]].client->sess.sessionTeam != TEAM_SPECTATOR) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i s\n", readyPlayers[i]));
            SetTeam(&g_entities[readyPlayers[i]], "spectator");
        }
    }
    for (i = 0; i < numberOfOtherPlayers; i++) {
        if (g_entities[otherPlayers[i]].client->sess.sessionTeam != TEAM_SPECTATOR) {
            //trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i s\n", otherPlayers[i]));
            SetTeam(&g_entities[otherPlayers[i]], "spectator");
        }
    }

    trap_SendServerCommand(-1, va("print \"^2The captain in team ^1RED ^2is^7: %s\n\"", g_entities[readyPlayers[0]].client->pers.netname));
    trap_SendServerCommand(-1, va("print \"^2The captain in team ^4BLUE ^2is^7: %s\n\"", g_entities[readyPlayers[team1Count]].client->pers.netname));
}

void Svcmd_ClientDesc_f( void ) {
	int i;

	for ( i = 0; i < level.maxclients; ++i ) {
		if ( level.clients[i].pers.connected != CON_DISCONNECTED && !( &g_entities[i] && g_entities[i].r.svFlags & SVF_BOT ) ) {
			char description[MAX_STRING_CHARS] = { 0 }, userinfo[MAX_INFO_STRING] = { 0 };
			trap_GetUserinfo( i, userinfo, sizeof( userinfo ) );

#ifdef NEWMOD_SUPPORT
			if ( *Info_ValueForKey( userinfo, "nm_ver" ) ) {
				// running newmod
				Q_strcat( description, sizeof( description ), "Newmod " );
				Q_strcat( description, sizeof( description ), Info_ValueForKey( userinfo, "nm_ver" ) );

				if ( level.clients[i].sess.auth == AUTHENTICATED ) {
					Q_strcat( description, sizeof( description ), S_COLOR_GREEN" (confirmed) "S_COLOR_WHITE );

					if ( level.clients[i].sess.cuidHash[0] ) {
						// valid cuid
						Q_strcat( description, sizeof( description ), va( "(cuid hash: "S_COLOR_CYAN"%s"S_COLOR_WHITE")", level.clients[i].sess.cuidHash ) );
					} else {
						// invalid cuid, should not happen
						Q_strcat( description, sizeof( description ), S_COLOR_RED"(invalid cuid!)"S_COLOR_WHITE );
					}
				} else if ( level.clients[i].sess.auth < AUTHENTICATED ) {
					Q_strcat( description, sizeof( description ), va( S_COLOR_YELLOW" (authing: %d)"S_COLOR_WHITE, level.clients[i].sess.auth ) );
				} else {
					Q_strcat( description, sizeof( description ), S_COLOR_RED" (auth failed)"S_COLOR_WHITE );
				}

				Q_strcat( description, sizeof( description ), ", " );
#else
			if ( *Info_ValueForKey( userinfo, "nm_ver" ) ) {
				// running newmod
				Q_strcat( description, sizeof( description ), "Newmod " );
				Q_strcat( description, sizeof( description ), Info_ValueForKey( userinfo, "nm_ver" ) );
				Q_strcat( description, sizeof( description ), ", " );
#endif
			} else if ( *Info_ValueForKey( userinfo, "smod_ver" ) ) {
				// running smod
				Q_strcat( description, sizeof( description ), "SMod " );
				Q_strcat( description, sizeof( description ), Info_ValueForKey( userinfo, "smod_ver" ) );
				Q_strcat( description, sizeof( description ), ", " );
			} else {
				// running another cgame mod
				Q_strcat( description, sizeof( description ), "Unknown mod, " );
			}

			if ( *Info_ValueForKey( userinfo, "ja_guid" ) ) {
				// running an openjk engine or fork
				Q_strcat( description, sizeof( description ), "OpenJK or derivate" );
			} else {
				Q_strcat( description, sizeof( description ), "Jamp or other" );
			}
			
			if ( ( g_antiWallhack.integer == 1 && !level.clients[i].sess.whTrustToggle )
				|| ( g_antiWallhack.integer >= 2 && level.clients[i].sess.whTrustToggle ) ) {
				// anti wallhack is enabled for this player
				Q_strcat( description, sizeof( description ), " ("S_COLOR_YELLOW"anti WH enabled"S_COLOR_WHITE")" );
			}

			if ( level.clients[i].sess.shadowMuted ) {
				Q_strcat( description, sizeof( description ), " ("S_COLOR_YELLOW"shadow muted"S_COLOR_WHITE")" );
			}

			G_Printf( "Client %i (%s"S_COLOR_WHITE"): %s\n", i, level.clients[i].pers.netname, description );
		}
	}
}

void Svcmd_WhTrustToggle_f( void ) {
	gentity_t	*ent;
	char		str[MAX_TOKEN_CHARS];
	char		*s;

	if ( !g_antiWallhack.integer ) {
		G_Printf( "Anti Wallhack is not enabled.\n" );
		return;
	}

	// find the player
	trap_Argv( 1, str, sizeof( str ) );
	ent = G_FindClient( str );

	if ( !ent ) {
		G_Printf( "Player not found.\n" );
		return;
	}

	ent->client->sess.whTrustToggle = !ent->client->sess.whTrustToggle;

	if ( g_antiWallhack.integer >= 2 ) {
		if ( ent->client->sess.whTrustToggle ) {
			s = "is now blacklisted (wallhack check is ACTIVE for him)";
		} else {
			s = "is no longer blacklisted (wallhack check is INACTIVE for him)";
		}
	} else {
		if ( ent->client->sess.whTrustToggle ) {
			s = "is now whitelisted (wallhack check is INACTIVE for him)";
		} else {
			s = "is no longer whitelisted (wallhack check is ACTIVE) for him";
		}
	}

	G_Printf( "Player %d (%s"S_COLOR_WHITE") %s\n", ent - g_entities, ent->client->pers.netname, s );
}

void Svcmd_PoolCreate_f()
{
    char short_name[64];
    char long_name[64];      

    if ( trap_Argc() < 3 )
    {
		G_Printf("Usage: pool_create <short name> <long name>\n");
		G_Printf("Related commands: pool_delete, pool_map_add, pool_map_remove, pool_list\n");
        return;
    }

    trap_Argv( 1, short_name, sizeof( short_name ) );
	Q_strncpyz(long_name, ConcatArgs(2), sizeof(long_name));

    if ( G_DBPoolCreate( short_name, long_name ) ){
        G_Printf( "Successfully created pool with short name ^5%s^7 and long name ^5%s^7.\n", short_name, long_name );
    }
	else {
		G_Printf("Failed to create pool with short name ^5%s^7 and long name ^5%s^7!\n", short_name, long_name);
	}
}

void Svcmd_PoolDelete_f()
{
    char short_name[64];

    if ( trap_Argc() < 2 )
    {
		G_Printf("Usage: pool_delete <short name>\n");
		G_Printf("Related commands: pool_create, pool_map_add, pool_map_remove, pool_list\n");
        return;
    }

    trap_Argv( 1, short_name, sizeof( short_name ) );

    if ( G_DBPoolDelete( short_name )) {
        G_Printf( "Sucessfully deleted pool '%s^7'.\n", short_name );
    }
	else {
		G_Printf("Failed to delete pool '%s^7'!\n", short_name);
	}


}

void Svcmd_PoolMapAdd_f()
{
    char short_name[64];
    char mapname[64];
    int  weight;

    if ( trap_Argc() < 3 )
    {
		G_Printf("Usage: pool_map_add <pool short name> <map filename> [weight]   (weight defaults to 1 if unspecified)\n");
		G_Printf("Related commands: pool_create, pool_delete, pool_map_remove, pool_list\n");
        return;
    }

    trap_Argv( 1, short_name, sizeof( short_name ) );
    trap_Argv( 2, mapname, sizeof( mapname ) );
	if (mapname[0]) {
		char *filter = stristr(mapname, "sertao");
		if (filter) {
			filter += 4;
			if (*filter == 'a')
				*filter = 'ã';
			else if (*filter == 'A')
				*filter = 'Ã';
		}
	}

    if ( trap_Argc() > 3 )
    {
        char weightStr[16];
        trap_Argv( 3, weightStr, sizeof( weightStr ) );

        weight = atoi( weightStr );
		if ( weight < 1 ) weight = 1;
    }
    else
    {
        weight = 1;
    } 

    if ( G_DBPoolMapAdd( short_name, mapname, weight ) ) {
        G_Printf( "Successfully added ^5%s^7 to pool ^5%s^7 with weight ^5%d^7.\n", mapname, short_name, weight );
    }
	else {
		G_Printf("Failed to add ^5%s^7 to pool ^5%s^7 with weight ^5%d^7!\n", mapname, short_name, weight);
	}
}

void Svcmd_PoolMapRemove_f()
{
    char short_name[64];
    char mapname[64];

    if ( trap_Argc() < 3 )
    {
		G_Printf("Usage: pool_map_remove <pool short name> <map filename>\n");
		G_Printf("Related commands: pool_create, pool_delete, pool_map_add, pool_list\n");
        return;
    } 

    trap_Argv( 1, short_name, sizeof( short_name ) );
    trap_Argv( 2, mapname, sizeof( mapname ) );
	if (mapname[0]) {
		char *filter = stristr(mapname, "sertao");
		if (filter) {
			filter += 4;
			if (*filter == 'a')
				*filter = 'ã';
			else if (*filter == 'A')
				*filter = 'Ã';
		}
	}

    if ( G_DBPoolMapRemove( short_name, mapname ) ) {
        G_Printf( "Sucessfully removed ^5%s^7 from pool ^5%s^7.\n", mapname, short_name );
    }
	else {
		G_Printf("Failed to remove ^5%s^7 from pool ^5%s^7!\n", mapname, short_name);
	}

}

void svListPools(void *context,
	int pool_id,
	const char *short_name,
	const char *long_name)
{
	ListPoolsContext *thisContext = (ListPoolsContext *)context;
	G_Printf("%s (%s)\n", short_name, long_name);
	++(thisContext->count);
}

void svListMapsInPools(void **context,
	const char *long_name,
	int pool_id,
	const char *mapname,
	int mapWeight)
{
	ListMapsInPoolContext *thisContext = *((ListMapsInPoolContext **)context);
	thisContext->pool_id = pool_id;
	thisContext->count++;
	Q_strncpyz(thisContext->long_name, long_name, sizeof(thisContext->long_name));
	G_Printf(" %s\n", mapname);
}


void Svcmd_MapPool_f(void) {
	if (trap_Argc() > 1)
	{
		ListMapsInPoolContext context;
		context.count = 0;
		ListMapsInPoolContext *ctxPtr = &context;

		char short_name[64];
		trap_Argv(1, short_name, sizeof(short_name));

		G_DBListMapsInPool(short_name, "", svListMapsInPools, (void **)&ctxPtr);

		G_Printf("Found %i maps for pool %s^7.\n", context.count, short_name, context.long_name);
	}
	else
	{
		ListPoolsContext context;
		context.count = 0;

		G_DBListPools(svListPools, &context);

		G_Printf("Found %i map pools.\n", context.count);
		G_Printf("To see a list of maps in a specific pool, use ^5pool_list <pool short name>^7\n");
		G_Printf("Related commands: pool_create, pool_delete, pool_map_add, pool_map_remove\n");
	}
}

typedef struct {
	char format[MAX_STRING_CHARS];
} SessionPrintCtx;

static void FormatAccountSessionList( void *ctx, const sessionReference_t sessionRef, const qboolean temporary ) {
	SessionPrintCtx* out = ( SessionPrintCtx* )ctx;

#ifdef NEWMOD_SUPPORT
	qboolean isNewmodSession = G_SessionInfoHasString( sessionRef.ptr, "cuid_hash2" );
#endif

	char sessFlags[16] = { 0 };
	if ( sessionRef.online ) Q_strcat( sessFlags, sizeof( sessFlags ), S_COLOR_GREEN"|" );
	if ( temporary ) Q_strcat( sessFlags, sizeof( sessFlags ), S_COLOR_YELLOW"|" );
#ifdef NEWMOD_SUPPORT
	if ( isNewmodSession ) Q_strcat( sessFlags, sizeof( sessFlags ), S_COLOR_CYAN"|" );
#endif

	Q_strcat( out->format, sizeof( out->format ), va( S_COLOR_WHITE"Session ID %d ", sessionRef.ptr->id ) );
	if ( VALIDSTRING( sessFlags ) ) Q_strcat( out->format, sizeof( out->format ), va( "%s ", sessFlags ) );
	Q_strcat( out->format, sizeof( out->format ), va( S_COLOR_WHITE"(hash: %llX)\n", sessionRef.ptr->hash ) );
}

static int AccountFlagName2Bitflag(const char* flagName) {
	if (!Q_stricmp(flagName, "Admin")) {
		return ACCOUNTFLAG_ADMIN;
	} else if (!Q_stricmp(flagName, "VerboseRcon")) {
		return ACCOUNTFLAG_RCONLOG;
	}

	return 0;
}

static const char* AccountBitflag2FlagName(int bitflag) {
	switch (bitflag) {
		case ACCOUNTFLAG_ADMIN: return "Admin";
		case ACCOUNTFLAG_RCONLOG: return "VerboseRcon";
		default: return NULL;
	}
}

void Svcmd_Account_f( void ) {
	qboolean printHelp = qfalse;

	if ( trap_Argc() > 1 ) {
		char s[64];
		trap_Argv( 1, s, sizeof( s ) );

		if ( !Q_stricmp( s, "create" ) ) {

			if ( trap_Argc() < 3 ) {
				G_Printf( "Usage: "S_COLOR_YELLOW"account create <username>\n" );
				return;
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv( 2, username, sizeof( username ) );
			Q_strlwr(username);

			// sanitize input
			char *p;
			for ( p = username; *p; ++p ) {
				if ( p == username ) {
					if ( !Q_isalpha( *p ) ) {
						G_Printf( "Username must start with an alphabetic character\n" );
						return;
					}
				} else if ( !*( p + 1 ) ) {
					if ( !Q_isalphanumeric( *p ) ) {
						G_Printf( "Username must end with an alphanumeric character\n" );
						return;
					}
				} else {
					if ( !Q_isalphanumeric( *p ) && *p != '-' && *p != '_' ) {
						G_Printf( "Valid characters are: a-z A-Z 0-9 - _\n" );
						return;
					}
				}
			}

			accountReference_t acc;

			if ( G_CreateAccount( username, &acc ) ) {
				G_Printf( "Account '%s' created successfully\n", acc.ptr->name );
			} else {
				if ( acc.ptr ) {
					char timestamp[32];
					G_FormatLocalDateFromEpoch( timestamp, sizeof( timestamp ), acc.ptr->creationDate );
					G_Printf( "Account '%s' was already created on %s\n", acc.ptr->name, timestamp );
				} else {
					G_Printf( "Failed to create account!\n" );
				}
			}

		} else if ( !Q_stricmp( s, "delete" ) ) {

			if ( trap_Argc() < 3 ) {
				G_Printf( "Usage: "S_COLOR_YELLOW"account delete <username>\n" );
				return;
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv( 2, username, sizeof( username ) );

			accountReference_t acc = G_GetAccountByName( username, qfalse );

			if ( !acc.ptr ) {
				G_Printf( "No account exists with this name\n" );
				return;
			}

			if ( G_DeleteAccount( acc.ptr ) ) {
				G_Printf( "Deleted account '%s' successfully\n", acc.ptr->name );
			} else {
				G_Printf( "Failed to delete account!\n" );
			}

		} else if ( !Q_stricmp( s, "info" ) ) {

			if ( trap_Argc() < 3 ) {
				G_Printf( "Usage: "S_COLOR_YELLOW"account info <username>\n" );
				return;
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv( 2, username, sizeof( username ) );

			accountReference_t acc = G_GetAccountByName( username, qfalse );

			if ( !acc.ptr ) {
				G_Printf( "Account '%s' does not exist\n", username );
				return;
			}

			SessionPrintCtx sessionsPrint = { 0 };
			G_ListSessionsForAccount( acc.ptr, FormatAccountSessionList, &sessionsPrint );

			char timestamp[32];
			G_FormatLocalDateFromEpoch( timestamp, sizeof( timestamp ), acc.ptr->creationDate );

			// TODO: pages

			char flagsStr[64] = { 0 };
			for ( int bitflag = 1; bitflag <= sizeof( int ) * 8; ++bitflag ) {
				if ( !( acc.ptr->flags & bitflag ) )
					continue;
				
				const char* flagName = AccountBitflag2FlagName( bitflag );

				if ( !VALIDSTRING( flagName ) )
					continue;

				if ( VALIDSTRING( flagsStr ) )
					Q_strcat( flagsStr, sizeof( flagsStr ), ", " );

				Q_strcat( flagsStr, sizeof( flagsStr ), flagName);
			}

			G_Printf(
				S_COLOR_YELLOW"Account Name: "S_COLOR_WHITE"%s\n"
				S_COLOR_YELLOW"Account ID: "S_COLOR_WHITE"%d\n"
				S_COLOR_YELLOW"Created on: "S_COLOR_WHITE"%s\n"
				S_COLOR_YELLOW"Group: "S_COLOR_WHITE"%s\n"
				S_COLOR_YELLOW"Flags: "S_COLOR_WHITE"%s\n"
				"\n",
				acc.ptr->name,
				acc.ptr->id,
				timestamp,
				acc.ptr->group,
				flagsStr
			);

			if ( VALIDSTRING( sessionsPrint.format ) ) {

#ifdef NEWMOD_SUPPORT
				G_Printf( "Sessions tied to this account: ( "S_COLOR_GREEN"| = online "S_COLOR_YELLOW"| = temporary "S_COLOR_CYAN"| = newmod "S_COLOR_WHITE")\n" );
#else
				G_Printf( "Sessions tied to this account: ( "S_COLOR_GREEN"| - online "S_COLOR_YELLOW"| - temporary "S_COLOR_WHITE")\n" );
#endif
				G_Printf( "%s", sessionsPrint.format );

			} else {
				G_Printf( "No session tied to this account yet.\n" );
			}

		} else if ( !Q_stricmp( s, "toggleflag" ) ) {

			if ( trap_Argc() < 4 ) {
				G_Printf( "Usage: "S_COLOR_YELLOW"account toggleflag <username> <flag>\n" );
				G_Printf( "Available flags: Admin, VerboseRcon\n" );
				return;
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv( 2, username, sizeof( username ) );

			accountReference_t acc = G_GetAccountByName( username, qfalse );

			if ( !acc.ptr ) {
				G_Printf( "Account '%s' does not exist\n", username );
				return;
			}

			char flagName[32];
			trap_Argv( 3, flagName, sizeof( flagName ) );

			int flag = AccountFlagName2Bitflag( flagName );

			if ( !flag ) {
				G_Printf( "Unknown account flag name '%s'\n", flagName );
				return;
			}

			if ( !( acc.ptr->flags & flag ) ) {
				// we are enabling the flag
				if ( G_SetAccountFlags( acc.ptr, flag, qtrue ) ) {
					G_Printf( "Flag '%s' enabled for account '%s' (id: %d)\n", flagName, acc.ptr->name, acc.ptr->id );
				} else {
					G_Printf( "Failed to enable flag '%s' for account '%s' (id: %d)\n", flagName, acc.ptr->name, acc.ptr->id );
				}
			} else {
				// we are disabling the flag
				if ( G_SetAccountFlags( acc.ptr, flag, qfalse ) ) {
					G_Printf( "Flag '%s' disabled for account '%s' (id: %d)\n", flagName, acc.ptr->name, acc.ptr->id );
				} else {
					G_Printf( "Failed to disable flag '%s' for account '%s' (id: %d)\n", flagName, acc.ptr->name, acc.ptr->id );
				}
			}

		} else if ( !Q_stricmp( s, "help" ) ) {
			printHelp = qtrue;
		} else {
			G_Printf( "Invalid subcommand.\n" );
			printHelp = qtrue;
		}
	} else {
		printHelp = qtrue;
	}

	if ( printHelp ) {
		G_Printf(
			"Valid subcommands:\n"
			S_COLOR_YELLOW"account create <username>"S_COLOR_WHITE": Creates a new account with the given name\n"
			S_COLOR_YELLOW"account delete <username>"S_COLOR_WHITE": Deletes the given account and unlinks all associated sessions\n"
			S_COLOR_YELLOW"account info <username>"S_COLOR_WHITE": Prints various information for the given account name\n"
			S_COLOR_YELLOW"account toggleflag <username> <flag>"S_COLOR_WHITE": Toggles an account flag for the given account name\n"
			S_COLOR_YELLOW"account help"S_COLOR_WHITE": Prints this message\n"
		);
	}
}

static void PrintUnassignedSessionIDsCallback(void* ctx, const int sessionId, const char* topAlias, const int playtime, const qboolean referenced) {
	char nameString[64] = { 0 };
	Q_strncpyz(nameString, topAlias, sizeof(nameString));
	int j;
	for (j = Q_PrintStrlen(nameString); j < MAX_NAME_DISPLAYLENGTH; ++j)
		Q_strcat(nameString, sizeof(nameString), " ");

	char durationString[64];
	G_FormatDuration(playtime, durationString, sizeof(durationString));

	char* referencedString = "";
	if (referenced)
		referencedString = " "S_COLOR_RED"/!\\";

	G_Printf(
		S_COLOR_WHITE"%s  "S_COLOR_WHITE"(%s) - session ID: %d%s\n",
		nameString, durationString, sessionId, referencedString
	);
}

#define NUM_UNASSIGNED_PER_PAGE 10
#define NUM_TOP_NICKNAMES		10

void Svcmd_Session_f( void ) {
	qboolean printHelp = qfalse;

	if ( trap_Argc() > 1 ) {
		char s[64];
		trap_Argv( 1, s, sizeof( s ) );

		if ( !Q_stricmp( s, "whois" ) ) {

			qboolean printed = qfalse;

			int i;
			for ( i = 0; i < level.maxclients; ++i ) {
				gclient_t *client = &level.clients[i];

				if ( client->pers.connected != CON_DISCONNECTED &&
					!( &g_entities[i] && g_entities[i].r.svFlags & SVF_BOT ) )
				{
					char* color;
					switch ( level.clients[i].sess.sessionTeam ) {
						case TEAM_RED: color = S_COLOR_RED; break;
						case TEAM_BLUE: color = S_COLOR_BLUE; break;
						case TEAM_FREE: color = S_COLOR_YELLOW; break;
						default: color = S_COLOR_WHITE;
					}

					printed = qtrue;

					if ( !client->session ) {
						G_Printf( "%sClient %d "S_COLOR_WHITE"(%s"S_COLOR_WHITE"): Uninitialized session\n", color, i, client->pers.netname );
						continue;
					}

					char line[MAX_STRING_CHARS];
					line[0] = '\0';

					Q_strcat( line, sizeof( line ), va( "%sClient %d "S_COLOR_WHITE"(%s"S_COLOR_WHITE"): Session ID %d (hash: %llX)",
						color, i, client->pers.netname, client->session->id, client->session->hash ) );

					if ( client->account ) {
						Q_strcat( line, sizeof( line ), va( " linked to Account ID %d '%s'", client->account->id, client->account->name ) );
					}

					Q_strcat( line, sizeof( line ), "\n" );

#ifdef _DEBUG
					Q_strcat( line, sizeof( line ), va( "=> Session cache num %d (ptr: 0x%lx)", client->sess.sessionCacheNum, ( uintptr_t )client->session ) );

					if ( client->account ) {
						Q_strcat( line, sizeof( line ), va( " ; Account cache num %d (ptr: 0x%lx)", client->sess.accountCacheNum, ( uintptr_t )client->account ) );
					}

					Q_strcat( line, sizeof( line ), "\n" );
#endif

					G_Printf( line );
				}
			}

			if ( !printed ) {
				G_Printf( "No player online\n" );
			}

		} else if ( !Q_stricmp( s, "unassigned" ) ) {

			int page = 1;

			if (trap_Argc() > 2) {
				char buf[8];
				trap_Argv(2, buf, sizeof(buf));
				page = Com_Clampi(1, 999, atoi(buf));
			}

			pagination_t pagination;
			pagination.numPerPage = NUM_UNASSIGNED_PER_PAGE;
			pagination.numPage = page;

			G_DBListTopUnassignedSessionIDs(pagination, PrintUnassignedSessionIDsCallback, NULL);

			G_Printf(
				S_COLOR_WHITE"Referenced sessions are marked with "S_COLOR_RED"/!\\ "S_COLOR_WHITE"and should be assigned ASAP.\n"
				S_COLOR_WHITE"Viewing page: %d\n",
				page
			);

		} else if ( !Q_stricmp( s, "nicknames" ) ) {

			if (trap_Argc() < 3) {
				G_Printf("Usage: "S_COLOR_YELLOW"session nicknames <session id>\n");
				return;
			}

			trap_Argv(2, s, sizeof(s));
			const int sessionId = atoi(s);
			if (sessionId <= 0) {
				G_Printf("Session ID must be a number > 1\n");
				return;
			}

			sessionReference_t sess = G_GetSessionByID(sessionId, qfalse);

			if (!sess.ptr) {
				G_Printf("No session found with this ID\n");
				return;
			}

			nicknameEntry_t nicknames[NUM_TOP_NICKNAMES];

			G_DBGetMostUsedNicknames(sess.ptr->id, NUM_TOP_NICKNAMES, &nicknames[0]);

			int i;
			for (i = 0; i < NUM_TOP_NICKNAMES && VALIDSTRING(nicknames[i].name); ++i) {
				char nameString[64] = { 0 };
				Q_strncpyz(nameString, nicknames[i].name, sizeof(nameString));
				int j;
				for (j = Q_PrintStrlen(nameString); j < MAX_NAME_DISPLAYLENGTH; ++j)
					Q_strcat(nameString, sizeof(nameString), " ");

				char durationString[64];
				G_FormatDuration(nicknames[i].duration, durationString, sizeof(durationString));

				G_Printf(
					S_COLOR_WHITE"%s  "S_COLOR_WHITE"(%s)\n",
					nameString, durationString
				);
			}

		} else if ( !Q_stricmp( s, "link" ) ) {

			if ( trap_Argc() < 4 ) {
				G_Printf( "Usage: "S_COLOR_YELLOW"session link <session id> <account name>\n" );
				return;
			}

			trap_Argv( 2, s, sizeof( s ) );
			const int sessionId = atoi( s );
			if ( sessionId <= 0 ) {
				G_Printf( "Session ID must be a number > 1\n" );
				return;
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv( 3, username, sizeof( username ) );

			sessionReference_t sess = G_GetSessionByID( sessionId, qfalse );

			if ( !sess.ptr ) {
				G_Printf( "No session found with this ID\n" );
				return;
			}

			if ( sess.ptr->accountId > 0 ) {
				accountReference_t acc = G_GetAccountByID( sess.ptr->accountId, qfalse );
				if ( acc.ptr ) G_Printf( "This session is already linked to account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id );
				return;
			}

			accountReference_t acc = G_GetAccountByName( username, qfalse );

			if ( !acc.ptr ) {
				G_Printf( "No account found with this name\n" );
				return;
			}

			if ( G_LinkAccountToSession( sess.ptr, acc.ptr ) ) {
				G_Printf( "Session successfully linked to account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id );
			} else {
				G_Printf( "Failed to link session to this account!\n" );
			}

		} else if ( !Q_stricmp( s, "linkingame" ) ) {

			if ( trap_Argc() < 4 ) {
				G_Printf( "Usage: "S_COLOR_YELLOW"session linkingame <client id> <account name>\n" );
				return;
			}

			trap_Argv( 2, s, sizeof( s ) );
			const int clientId = atoi( s );
			if ( !IN_CLIENTNUM_RANGE( clientId ) ||
				!g_entities[clientId].inuse ||
				level.clients[clientId].pers.connected != CON_CONNECTED ) {
				G_Printf( "You must enter a valid client ID of a connected client\n" );
				return;
			}

			gclient_t *client = &level.clients[clientId];

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv( 3, username, sizeof( username ) );

			if ( !client->session ) {
				return;
			}

			if ( client->account ) {
				G_Printf( "This session is already linked to account '%s' (id: %d)\n", client->account->name, client->account->id );
				return;
			}

			accountReference_t acc = G_GetAccountByName( username, qfalse );

			if ( !acc.ptr ) {
				G_Printf( "No account found with this name\n" );
				return;
			}

			if ( G_LinkAccountToSession( client->session, acc.ptr ) ) {
				G_Printf( "Client session successfully linked to account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id );
			} else {
				G_Printf( "Failed to link client session to this account!\n" );
			}

		} else if ( !Q_stricmp( s, "unlink" ) ) {

			if ( trap_Argc() < 3 ) {
				G_Printf( "Usage: "S_COLOR_YELLOW"session unlink <session id>\n" );
				return;
			}

			trap_Argv( 2, s, sizeof( s ) );
			const int sessionId = atoi( s );
			if ( sessionId <= 0 ) {
				G_Printf( "Session ID must be a number > 1\n" );
				return;
			}

			sessionReference_t sess = G_GetSessionByID( sessionId, qfalse );

			if ( !sess.ptr ) {
				G_Printf( "No session found with this ID\n" );
				return;
			}

			if ( sess.ptr->accountId <= 0 ) {
				G_Printf( "This session is not linked to any account\n" );
				return;
			}

			accountReference_t acc = G_GetAccountByID( sess.ptr->accountId, qfalse );

			if ( !acc.ptr ) {
				return;
			}

			if ( G_UnlinkAccountFromSession( sess.ptr ) ) {
				G_Printf( "Session successfully unlinked from account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id );
			} else {
				G_Printf( "Failed to unlink session from this account!\n" );
			}

		} else if ( !Q_stricmp( s, "unlinkingame" ) ) {

			if ( trap_Argc() < 3 ) {
				G_Printf( "Usage: "S_COLOR_YELLOW"session unlinkingame <client id>\n" );
				return;
			}

			trap_Argv( 2, s, sizeof( s ) );
			const int clientId = atoi( s );
			if ( !IN_CLIENTNUM_RANGE( clientId ) ||
				!g_entities[clientId].inuse ||
				level.clients[clientId].pers.connected != CON_CONNECTED ) {
				G_Printf( "You must enter a valid client ID of a connected client\n" );
				return;
			}

			gclient_t *client = &level.clients[clientId];

			if ( !client->session ) {
				return;
			}

			if ( !client->account ) {
				G_Printf( "This client's session is not linked to any account\n" );
				return;
			}

			// save a reference so we can print the name even after unlinking
			accountReference_t acc;
			acc.ptr = client->account;
			acc.online = qtrue;

			if ( G_UnlinkAccountFromSession( client->session ) ) {
				G_Printf( "Client session successfully unlinked from account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id );
			} else {
				G_Printf( "Failed to unlink Client session from this account!\n" );
			}

		} else if ( !Q_stricmp( s, "purge") ) {

			static int lastConfirmationTime = INT_MIN;

			if (level.time < lastConfirmationTime + 10000) {
				G_DBPurgeUnassignedSessions();
				G_Printf("Purge completed.\n");
			} else {
				G_Printf( 
					"You are about to delete unreferenced sessions from the database FOREVER.\n"
					"If you still wish to proceed, type the command again within 10 seconds.\n"
				);
			}

			lastConfirmationTime = level.time;

		} else if ( !Q_stricmp( s, "help" ) ) {
			printHelp = qtrue;
		} else {
			G_Printf( "Invalid subcommand.\n" );
			printHelp = qtrue;
		}
	} else {
		printHelp = qtrue;
	}

	if ( printHelp ) {
		G_Printf(
			"Valid subcommands:\n"
			S_COLOR_YELLOW"session whois"S_COLOR_WHITE": Lists the session currently in use by all in-game players\n"
			S_COLOR_YELLOW"session unassigned [page]"S_COLOR_WHITE": Lists the top unassigned sessions (may lag the server with too many sessions, so be cautious)\n"
			S_COLOR_YELLOW"session nicknames <session id>"S_COLOR_WHITE": Prints the top used nicknames for the given session ID\n"
			S_COLOR_YELLOW"session link <session id> <account name>"S_COLOR_WHITE": Links the given session ID to an existing account\n"
			S_COLOR_YELLOW"session linkingame <client id> <account name>"S_COLOR_WHITE": Shortcut command to link an in-game client's session to an existing account\n"
			S_COLOR_YELLOW"session unlink <session id>"S_COLOR_WHITE": Unlinks the account associated to the given session ID\n"
			S_COLOR_YELLOW"session unlinkingame <client id>"S_COLOR_WHITE": Shortcut command to unlink the account associated to an in-game client's session\n"
			S_COLOR_YELLOW"session purge"S_COLOR_WHITE": Deletes all unreferenced sessions from the database. USE WITH CAUTION!\n"
			S_COLOR_YELLOW"session help"S_COLOR_WHITE": Prints this message\n"
		);
	}
}

/*
=================
ConsoleCommand

=================
*/
void LogExit( const char *string );

qboolean	ConsoleCommand( void ) {
	char	cmd[MAX_TOKEN_CHARS];

	trap_Argv( 0, cmd, sizeof( cmd ) );

	if ( Q_stricmp (cmd, "entitylist") == 0 ) {
		Svcmd_EntityList_f();
		return qtrue;
	}

	if ( Q_stricmp (cmd, "forceteam") == 0 ) {
		Svcmd_ForceTeam_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "game_memory") == 0) {
		Svcmd_GameMem_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "addbot") == 0) {
		Svcmd_AddBot_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "botlist") == 0) {
		Svcmd_BotList_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "addip") == 0) {
		Svcmd_AddIP_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "removeip") == 0) {
		Svcmd_RemoveIP_f();
		return qtrue;
	}

    if ( Q_stricmp( cmd, "addwhiteip" ) == 0 )
    {
        Svcmd_AddWhiteIP_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "removewhiteip" ) == 0 )
    {
        Svcmd_RemoveWhiteIP_f();
        return qtrue;
    }    

	if (Q_stricmp (cmd, "listip") == 0) {
        Svcmd_Listip_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "svprint") == 0) {
		trap_SendServerCommand( -1, va("cp \"%s\n\"", ConcatArgs(1) ) );
		return qtrue;
	}

	if (Q_stricmp (cmd, "resetflags") == 0) {
		Svcmd_ResetFlags_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "fsreload") == 0) {
		Svcmd_FSReload_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "osinfo") == 0) {
		Svcmd_Osinfo_f();
		return qtrue;
	}	

	if (Q_stricmp (cmd, "clientban") == 0) {
		Svcmd_ClientBan_f();
		return qtrue;
	}

	if (Q_stricmp(cmd, "map_random") == 0) {
		Svcmd_MapRandom_f();
		return qtrue;
	}

	if ( Q_stricmp( cmd, "map_multi_vote" ) == 0 ) {
		Svcmd_MapMultiVote_f();
		return qtrue;
	}

    if ( Q_stricmp( cmd, "pool_create" ) == 0 || !Q_stricmp(cmd, "poolcreate") || !Q_stricmp(cmd, "createpool") || !Q_stricmp(cmd, "create_pool") )
    {
        Svcmd_PoolCreate_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "pool_delete" ) == 0 || !Q_stricmp(cmd, "pooldelete") || !Q_stricmp(cmd, "deletepool") || !Q_stricmp(cmd, "delete_pool") )
    {
        Svcmd_PoolDelete_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "pool_map_add" ) == 0 || !Q_stricmp(cmd, "poolmapadd") || !Q_stricmp(cmd, "pool_add_map") || !Q_stricmp(cmd, "pooladdmap") ||
		!Q_stricmp(cmd, "pool_add"))
    {
        Svcmd_PoolMapAdd_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "pool_map_remove" ) == 0 || !Q_stricmp(cmd, "poolmapremove") || !Q_stricmp(cmd, "pool_remove_map") || !Q_stricmp(cmd, "poolremovemap") ||
		!Q_stricmp(cmd, "pool_remove"))
    {
        Svcmd_PoolMapRemove_f();
        return qtrue;
    }

	if (!Q_stricmp(cmd, "mappool") || !Q_stricmp(cmd, "mappools") || !Q_stricmp(cmd, "listpool") || !Q_stricmp(cmd, "listpools") ||
		!Q_stricmp(cmd, "map_pool") || !Q_stricmp(cmd, "map_pools") || !Q_stricmp(cmd, "list_pool") || !Q_stricmp(cmd, "list_pools") ||
		!Q_stricmp(cmd, "poolmap") || !Q_stricmp(cmd, "pool_map") || !Q_stricmp(cmd, "poollist") || !Q_stricmp(cmd, "pool_list") ||
		!Q_stricmp(cmd, "poolmaps") || !Q_stricmp(cmd, "pool_maps") || !Q_stricmp(cmd, "pools") || !Q_stricmp(cmd, "pool")) {
		Svcmd_MapPool_f();
		return qtrue;
	}

	//if (Q_stricmp (cmd, "accountadd") == 0) {
	//	Svcmd_AddAccount_f();
	//	return qtrue;
	//}

	//if (Q_stricmp (cmd, "accountremove") == 0) {
	//	Svcmd_RemoveAccount_f();
	//	return qtrue;
	//}	

	//if (Q_stricmp (cmd, "accountchange") == 0) {
	//	Svcmd_ChangeAccount_f();
	//	return qtrue;
	//}	

	//if (Q_stricmp (cmd, "accountreload") == 0) {
	//	Svcmd_ReloadAccount_f();
	//	return qtrue;
	//}	

	if (Q_stricmp (cmd, "accountlist") == 0) {
		Svcmd_AccountPrint_f();
		return qtrue;
	}	
 
    //OSP: pause
    if ( !Q_stricmp( cmd, "pause" ) )
    {
        //if ( level.pause.state == PAUSE_NONE ) {
			char durationStr[4];
			int duration;

			trap_Argv(1,durationStr,sizeof(durationStr));
			duration = atoi(durationStr);
				
			if (duration == 0) // 2 minutes default
				duration = 2*60;
			else if (duration < 0) // second minimum
				duration = 1;
			else if ( duration > 5*60) // 5 minutes max
				duration = 5*60;

            level.pause.state = PAUSE_PAUSED;
			level.pause.time = level.time + duration*1000; // 5 seconds
		//}

        return qtrue;
    } 

    //OSP: unpause
    if ( !Q_stricmp( cmd, "unpause" ) )
    {
		if ( level.pause.state == PAUSE_PAUSED ) {
            level.pause.state = PAUSE_UNPAUSING;
			//level.pause.time = level.time + japp_unpauseTime.integer*1000;
            level.pause.time = level.time + 3*1000;
        }

        return qtrue;
    } 

    if ( !Q_stricmp( cmd, "endmatch" ) )
    {
#ifdef NEWMOD_SUPPORT
		trap_SendServerCommand(-1, "lchat \"em\"");
#endif
		trap_SendServerCommand( -1,  va("print \"Match forced to end.\n\""));
		LogExit( "Match forced to end." );
        return qtrue;
    } 

	if ( !Q_stricmp( cmd, "lockteams" ) )
	{
		Svcmd_LockTeams_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "allready"))
	{
		Svcmd_AllReady_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "cointoss"))
	{
		Svcmd_Cointoss_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "forcename"))
	{
		Svcmd_ForceName_f();
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "votepass" ) ) {
		Svcmd_VoteForce_f( qtrue );
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "votefail" ) ) {
		Svcmd_VoteForce_f( qfalse );
		return qtrue;
	}

    if (!Q_stricmp(cmd, "specall")) {
        Svcmd_SpecAll_f();
        return qtrue;
    }

    if (!Q_stricmp(cmd, "randomcapts")) {
        Svcmd_RandomCapts_f();
        return qtrue;
    }

    if (!Q_stricmp(cmd, "randomteams")) {
        Svcmd_RandomTeams_f();
        return qtrue;
    }
	
	if ( !Q_stricmp( cmd, "clientdesc" ) ) {
		Svcmd_ClientDesc_f();
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "whTrustToggle" ) ) {
		Svcmd_WhTrustToggle_f();
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "shadowmute" ) ) {
		Svcmd_ShadowMute_f();
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "account" ) ) {
		Svcmd_Account_f();
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "session" ) ) {
		Svcmd_Session_f();
		return qtrue;
	}

	if (g_dedicated.integer) {
		if (Q_stricmp (cmd, "say") == 0) {
			trap_SendServerCommand( -1, va("print \"server: %s\n\"", ConcatArgs(1) ) );
			return qtrue;
		}
		// everything else will also be printed as a say command
		if (!g_quietrcon.integer){
			trap_SendServerCommand( -1, va("print \"server: %s\n\"", ConcatArgs(0) ) );
			return qtrue;
		}
	}

	return qfalse;
}

void G_LogRconCommand(const char* ipFrom, const char* command) {
	const char* displayStr;

	// try to find who used the command
	gclient_t* from = G_FindClientByIPPort(ipFrom);
	if (from) {
		if (from->account) {
			displayStr = from->account->name;
		} else {
			displayStr = from->pers.netname;
		}
	} else {
		displayStr = ipFrom;
	}

	// notify all online admins who also have the verbose rcon flag
	int i;
	for (i = 0; i < level.maxclients; ++i) {
		gclient_t* other = &level.clients[i];

		if (other->pers.connected != CON_CONNECTED)
			continue;
		if (!other->account)
			continue;
		if (!(other->account->flags & ACCOUNTFLAG_ADMIN) || !(other->account->flags & ACCOUNTFLAG_RCONLOG))
			continue;
		if (other == from)
			continue;

		// use out of band print so it doesn't get in demos
		trap_OutOfBandPrint(i, va("print\n"S_COLOR_GREEN"[Admin] "S_COLOR_WHITE"Rcon from "S_COLOR_GREEN"%s"S_COLOR_WHITE": /rcon %s\n", displayStr, command));
	}

	// also log it to disk if needed
	if (g_logrcon.integer) {
		G_RconLog("rcon from {%s}: %s\n", ipFrom, command);
	}
}
