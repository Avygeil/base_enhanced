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
	if (level.intermissiontime)
		return;
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

	// count these as rets, i guess; no sense punishing someone's fckill efficiency due to resetflags vote
	if (level.redPlayerWhoKilledBlueCarrierOfRedFlag)
		level.redPlayerWhoKilledBlueCarrierOfRedFlag->fcKillsResultingInRets++;
	if (level.bluePlayerWhoKilledRedCarrierOfBlueFlag)
		level.bluePlayerWhoKilledRedCarrierOfBlueFlag->fcKillsResultingInRets++;

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
	int isHeads = rand() % 2;

	int now = trap_Milliseconds();

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client)
			continue;

		char *color = "^3";
		team_t team;
		int headsTime = 0, tailsTime = 0;

		// color the result for this person if they chose something, or they are ingame, or they are following someone who is ingame

		if (!ent->client->pers.cointossHeadsTime && !ent->client->pers.cointossTailsTime &&
			ent->client->sess.sessionTeam == TEAM_SPECTATOR && ent->client->sess.spectatorState == SPECTATOR_FOLLOW &&
			ent->client->sess.spectatorClient >= 0 && ent->client->sess.spectatorClient < MAX_CLIENTS &&
			g_entities[ent->client->sess.spectatorClient].inuse && g_entities[ent->client->sess.spectatorClient].client &&
			g_entities[ent->client->sess.spectatorClient].client->sess.sessionTeam != TEAM_FREE) {
			team = g_entities[ent->client->sess.spectatorClient].client->sess.sessionTeam;
			headsTime = g_entities[ent->client->sess.spectatorClient].client->pers.cointossHeadsTime;
			tailsTime = g_entities[ent->client->sess.spectatorClient].client->pers.cointossTailsTime;
		}
		else {
			team = ent->client->sess.sessionTeam;
			headsTime = ent->client->pers.cointossHeadsTime;
			tailsTime = ent->client->pers.cointossTailsTime;
		}

		if (headsTime && now - headsTime > VOTE_TIME)
			headsTime = 0;
		if (tailsTime && now - tailsTime > VOTE_TIME)
			tailsTime = 0;

		if (team == TEAM_RED || team == TEAM_BLUE || headsTime || tailsTime) {
			if (isHeads) {
				if (headsTime)
					color = "^2";
				else
					color = "^1";
			}
			else {
				if (tailsTime)
					color = "^2";
				else
					color = "^1";
			}
		}

		trap_SendServerCommand(i, va("cp \"%s%s!\"", color, isHeads ? "Heads" : "Tails"));
		trap_SendServerCommand(i, va("print \"Coin toss result: %s%s^7\n\"", color, isHeads ? "Heads" : "Tails"));
		ent->client->pers.cointossHeadsTime = ent->client->pers.cointossTailsTime = 0;
	}

	G_LogPrintf("Cointoss result was %s.\n", isHeads ? "heads" : "tails");
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

	trap_SendServerCommand( -1, va( "print \"%sVote forced to %s.^7\n\"", pass ? "^2" : "^1", pass ? "pass" : "fail" ) );

	if ( pass ) {
		// set the delay
		if (!Q_stricmpn(level.voteString, "pause", 5))
			level.voteExecuteTime = level.time; // allow pause votes to take affect immediately
		else
			level.voteExecuteTime = level.time + 3000;
	}

	if ( level.multiVoting ) {
		// reset global cp so it doesnt show up more than needed since we forced the vote to end
		G_GlobalTickedCenterPrint( "", 0, qfalse );
	}

	if ( !level.multiVoting ) {
		g_entities[level.lastVotingClient].client->lastCallvoteTime = level.time;
	} else if ( !pass ) {
		level.voteAutoPassOnExpiration = qfalse;
		level.multiVoting = qfalse;
		level.voteBanPhase = qfalse;
		level.voteBanPhaseCompleted = qfalse;
		level.inRunoff = qfalse;
		level.multiVoteChoices = 0;
		level.multiVoteHasWildcard = qfalse;
		level.multivoteWildcardMapFileName[0] = '\0';
		level.mapsThatCanBeVotedBits = 0;
		level.multiVoteTimeExtensions = 0;
		level.runoffSurvivors = level.runoffLosers = level.successfulRerollVoters = 0llu;
		memset( level.multiVotes, 0, sizeof( level.multiVotes ) );
		memset( level.multiVoteBanVotes, 0, sizeof( level.multiVoteBanVotes) );
		memset(level.bannedMapNames, 0, sizeof(level.bannedMapNames));
		memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
		memset(&level.multiVoteMapShortNames, 0, sizeof(level.multiVoteMapShortNames));
		memset(&level.multiVoteMapFileNames, 0, sizeof(level.multiVoteMapFileNames));
		ListClear(&level.rememberedMultivoteMapsList);
		level.runoffRoundsCompletedIncludingRerollRound = 0;
		level.mapsRerolled = qfalse;
	}

	level.voteStartTime = 0;
	level.voteTime = 0;
	trap_SetConfigstring( CS_VOTE_TIME, "" );
}

static qboolean MapTierDataMatches(genericNode_t *node, void *userData) {
	mapTierData_t *existing = (mapTierData_t *)node;
	const char *thisMap = (const char *)userData;

	if (!existing || !thisMap)
		return qfalse;

	if (!Q_stricmp(existing->mapFileName, thisMap))
		return qtrue;

	return qfalse;
}

extern int g_numArenas;
extern char *g_arenaInfos[MAX_ARENAS];

// checks arena data to see if map exists, rather than touching the .bsp file (should be faster)
qboolean MapExistsQuick(const char *mapFileName) {
	if (!VALIDSTRING(mapFileName))
		return qfalse;

	for (int i = 0; i < g_numArenas; i++) {
		char *value = Info_ValueForKey(g_arenaInfos[i], "map");
		if (!Q_stricmp(mapFileName, value))
			return qtrue; // found it
	}

	return qfalse;
}

void Svcmd_MapAlias_f(void) {
	if (trap_Argc() < 2) {
		Com_Printf("^7Usage:\n"
			"^7mapalias list                   - lists current map aliases\n"
			"^9mapalias add [filename] [alias] - adds a map filename with a specified alias to the list\n"
			"^7mapalias delete [filename]      - deletes a map filename from the list\n"
			"^9mapalias live [filename]        - sets a map as the live version of its alias\n"
			"^7use the ^5listmaps^7 command to view a list of all maps.\n");
		return;
	}

	char arg1[MAX_STRING_CHARS] = { 0 }, arg2[MAX_STRING_CHARS] = { 0 }, arg3[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, arg1, sizeof(arg1));
	Q_strlwr(arg1);
	Q_StripColor(arg1);
	COM_StripExtension((const char *)arg1, arg1);
	if (trap_Argc() >= 3) {
		trap_Argv(2, arg2, sizeof(arg2));
		Q_strlwr(arg2);
		Q_StripColor(arg2);
		COM_StripExtension((const char *)arg2, arg2);
		if (trap_Argc() >= 4) {
			trap_Argv(3, arg3, sizeof(arg3));
			Q_strlwr(arg3);
			Q_StripColor(arg3);
		}
	}

	if (!Q_stricmp(arg1, "list") || !Q_stricmp(arg1, "view")) {
		G_DBListMapAliases();
	}
	else if (!Q_stricmp(arg1, "add") || !Q_stricmp(arg1, "forceadd")) {
		if (!arg2[0]) {
			Com_Printf("Usage: mapalias add [filename] [alias] <optional 'live'>\n");
			return;
		}

		if (Q_stricmp(arg1, "forceadd") && !MapExistsQuick(arg2)) {
			Com_Printf("%s does not currently exist on the server. Are you sure you entered the correct filename? You can use ^5mapalias forceadd^7 to bypass this warning.\n", arg2);
			return;
		}

		if (!Q_stricmpn(arg3, "mp/", 3)) {
			Com_Printf("Alias should not contain the mp/ prefix.\n");
			return;
		}

		qboolean setLive = qfalse;
		if (trap_Argc() >= 5) {
			char buf[MAX_STRING_CHARS] = { 0 };
			trap_Argv(4, buf, sizeof(buf));
			if (!Q_stricmp(buf, "live"))
				setLive = qtrue;
		}

		if (G_DBSetMapAlias(arg2, arg3, setLive))
			Com_Printf("Successfully added filename ^5%s^7 with alias ^5%s^7%s.\n", arg2, arg3, setLive ? " and set it to live" : "");
		else
			Com_Printf("Error adding filename ^5%s^7 with ^5%s^7%s!\n", arg2, arg3, setLive ? " and setting it to live" : "");
	}
	else if (!Q_stricmp(arg1, "delete") || !Q_stricmp(arg1, "forcedelete")) {
		if (!arg2[0]) {
			Com_Printf("Usage: mapalias delete [filename]\n");
			return;
		}

		if (Q_stricmp(arg1, "forcedelete") && !MapExistsQuick(arg2)) {
			Com_Printf("%s does not currently exist on the server. Are you sure you entered the correct filename? You can use ^5mapalias forcedelete^7 to bypass this warning.\n", arg2);
			return;
		}

		if (G_DBClearMapAlias(arg2))
			Com_Printf("Successfully deleted alias for filename ^5%s^7.\n", arg2);
		else
			Com_Printf("Error setting alias for filename ^5%s^7!\n", arg2);
	}
	else if (!Q_stricmp(arg1, "live") || !Q_stricmp(arg1, "forcelive")) {
		if (!arg2[0]) {
			Com_Printf("Usage: mapalias live [filename]\n");
			return;
		}

		if (Q_stricmp(arg1, "forcelive") && !MapExistsQuick(arg2)) {
			Com_Printf("%s does not currently exist on the server. Are you sure you entered the correct filename? You can use ^5mapalias forcelive^7 to bypass this warning.\n", arg2);
			return;
		}

		char alias[MAX_QPATH] = { 0 };
		if (G_DBSetMapAliasLive(arg2, alias, sizeof(alias)))
			Com_Printf("Successfully set filename ^5%s^7 as the live filename for alias ^5%s^7.\n", arg2, alias);
		else
			Com_Printf("Error setting filename ^5%s^7 as live!\n", arg2);
	}
	else {
		Com_Printf("^7Usage:\n"
			"^7mapalias list                   - lists current map aliases\n"
			"^9mapalias add [filename] [alias] - adds a map filename with a specified alias to the list\n"
			"^7mapalias delete [filename]      - deletes a map filename from the list\n"
			"^9mapalias live [filename]        - sets a map as the live version of its alias\n"
			"^7use the ^5listmaps^7 command to view a list of all maps.\n");
		return;
	}
}

void Svcmd_ListMaps_f(void) {
	list_t list = { 0 };
	int numGotten = 0;
	for (int i = g_numArenas - 1; i >= 0; i--) { // go in backwards order so it's alphabetized...nice video game
		char *value = Info_ValueForKey(g_arenaInfos[i], "type");
		if (VALIDSTRING(value) && strstr(value, "ctf")) {
			mapAlias_t *add = (mapAlias_t *)ListAdd(&list, sizeof(mapAlias_t));
			value = Info_ValueForKey(g_arenaInfos[i], "map");
			Q_strncpyz(add->filename, value, sizeof(add->filename));
			char alias[MAX_QPATH] = { 0 };
			qboolean isLive = qfalse;
			if (G_DBGetAliasForMapName(value, alias, sizeof(alias), &isLive)) {
				Q_strncpyz(add->alias, alias, sizeof(add->alias));
				Q_strncpyz(add->live, isLive ? "^2Yes" : "No", sizeof(add->live));
			}
			else {
				add->alias[0] = ' ';
				add->live[0] = ' ';
			}
			numGotten++;
		}
	}

	if (!numGotten) {
		Com_Printf("Unable to find any CTF maps!\n");
		return;
	}

	iterator_t iter;
	ListIterate(&list, &iter, qfalse);
	Table *t = Table_Initialize(qtrue);
	while (IteratorHasNext(&iter)) {
		mapAlias_t *ma = (mapAlias_t *)IteratorNext(&iter);
		Table_DefineRow(t, ma);
	}

	mapAlias_t ma = { 0 };
	Table_DefineColumn(t, "Filename", GenericTableStringCallback, (void *)((unsigned int)(&ma.filename) - (unsigned int)&ma), qtrue, -1, MAX_QPATH);
	Table_DefineColumn(t, "Alias", GenericTableStringCallback, (void *)((unsigned int)(&ma.alias) - (unsigned int)&ma), qtrue, -1, MAX_QPATH);
	Table_DefineColumn(t, "Live", GenericTableStringCallback, (void *)((unsigned int)(&ma.live) - (unsigned int)&ma), qtrue, -1, MAX_QPATH);

	int bufSize = 1024 * numGotten;
	char *buf = calloc(bufSize, sizeof(char));
	Table_WriteToBuffer(t, buf, bufSize, qtrue, -1);
	Com_Printf("CTF maps currently on server:\n");

	// should write a function to do this stupid chunked printing
	char *remaining = buf;
	int totalLen = strlen(buf);
	while (*remaining && remaining < buf + totalLen) {
		char temp[4096] = { 0 };
		Q_strncpyz(temp, remaining, sizeof(temp));
		Com_Printf(temp);
		int copied = strlen(temp);
		remaining += copied;
	}

	free(buf);
	Table_Destroy(t);
	ListClear(&list);
}

void Svcmd_Tier_f(void) {
	if (trap_Argc() < 2) {
		Com_Printf("^7Usage%s:\n"
			"^7rcon tier add [map]    - whitelist a map so that it can be chosen in votes\n"
			"^9rcon tier remove [map] - remove a map from the whitelist\n"
			"^7rcon tier list         - view which maps currently are, or are not, whitelisted\n", g_vote_tierlist.integer ? "" : " (note: tierlist is currently disabled, enable with g_vote_tierlist)");
		return;
	}

	char arg1[MAX_STRING_CHARS] = { 0 }, arg2[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, arg1, sizeof(arg1));
	trap_Argv(2, arg2, sizeof(arg2));
	if (!arg1[0] || isspace(arg1[0]) || (trap_Argc() >= 3 && (!arg2[0] || isspace(arg2[0])))) {
		Com_Printf("^7Usage%s:\n"
			"^7rcon tier add [map]    - whitelist a map so that it can be chosen in votes\n"
			"^9rcon tier remove [map] - remove a map from the whitelist\n"
			"^7rcon tier list         - view which maps currently are, or are not, whitelisted\n", g_vote_tierlist.integer ? "" : " (note: tierlist is currently disabled, enable with g_vote_tierlist)");
		return;
	}

	Q_strlwr(arg1);
	Q_strlwr(arg2);

	if (!Q_stricmp(arg1, "add") && arg2[0] && !isspace(arg2[0])) {
		char mapFileName[MAX_QPATH] = { 0 };
		if (!GetMatchingMap(arg2, mapFileName, sizeof(mapFileName))) {
			Com_Printf("Unable to find any map matching '%s^7'. If you still want to whitelist this map anyway, enter ^5tier forceadd [filename]^7. Be sure to use the actual map filename (e.g. ^5mp/ctf_kejim^7).\n", arg2);
			return;
		}

		if (G_DBAddMapToTierWhitelist(mapFileName))
			Com_Printf("Successfully added %s^7 (^9%s^7) to the whitelist.\n", arg2, mapFileName);
		else
			Com_Printf("Error adding %s^7 (^9%s^7) to the whitelist!\n", arg2, mapFileName);
	}
	else if (!Q_stricmp(arg1, "forceadd") && arg2[0] && !isspace(arg2[0])) {
		COM_StripExtension((const char *)arg2, arg2);
		if (G_DBAddMapToTierWhitelist(arg2))
			Com_Printf("Successfully force-added %s^7 to the whitelist.\n", arg2);
		else
			Com_Printf("Error force-adding %s^7 to the whitelist!\n", arg2);
	}
	else if (!Q_stricmp(arg1, "remove") && arg2[0] && !isspace(arg2[0])) {
		char mapFileName[MAX_QPATH] = { 0 };
		if (!GetMatchingMap(arg2, mapFileName, sizeof(mapFileName))) {
			Com_Printf("Unable to find any map matching '%s^7'. If you still want to unwhitelist this map anyway, enter ^5tier forceremove [filename]^7. Be sure to use the actual map filename (e.g. ^5mp/ctf_kejim^7).\n", arg2);
			return;
		}

		if (G_DBRemoveMapFromTierWhitelist(mapFileName))
			Com_Printf("Successfully removed %s^7 (^9%s^7) from the whitelist.\n", arg2, mapFileName);
		else
			Com_Printf("Error removing %s^7 (^9%s^7) from the whitelist!\n", arg2, mapFileName);
	}
	else if (!Q_stricmp(arg1, "forceremove") && arg2[0] && !isspace(arg2[0])) {
		COM_StripExtension((const char *)arg2, arg2);
		if (G_DBRemoveMapFromTierWhitelist(arg2))
			Com_Printf("Successfully force-removed %s^7 from the whitelist.\n", arg2);
		else
			Com_Printf("Error force-removing %s^7 from the whitelist!\n", arg2);
	}
	else if (!Q_stricmp(arg1, "view") || !Q_stricmp(arg1, "list") || !Q_stricmp(arg1, "whitelist") || !Q_stricmp(arg1, "current") || !Q_stricmp(arg1, "community")) {
		char mapsListStr[4096] = { 0 };
		list_t *whitelistedMapsList = G_DBGetTierWhitelistedMaps();
		if (whitelistedMapsList) {
			iterator_t iter;
			ListIterate(whitelistedMapsList, &iter, qfalse);
			int numGotten = 0;
			while (IteratorHasNext(&iter)) {
				mapTierData_t *data = (mapTierData_t *)IteratorNext(&iter);
				char mapShortName[MAX_QPATH] = { 0 };
				GetShortNameForMapFileName(data->mapFileName, mapShortName, sizeof(mapShortName));
				Q_strcat(mapsListStr, sizeof(mapsListStr), va("%s%s", numGotten ? ", " : "", mapShortName));
				numGotten++;
			}
			if (numGotten) {
				Com_Printf("^2Maps currently whitelisted: ^7");
				Com_Printf("%s^7\n", mapsListStr);
				Com_Printf("^3----------------------------------------^7\n");
			}
			else {
				Com_Printf("^2No maps are currently whitelisted.^7\n");
				Com_Printf("^3----------------------------------------^7\n");
				whitelistedMapsList = NULL;
			}
		}
		else {
			Com_Printf("^2No maps are currently whitelisted.^7\n");
			Com_Printf("^3----------------------------------------^7\n");
		}

		char allMapsStr[4096] = { 0 };
		list_t allMapsList = { 0 };
		int numGotten = 0;
		for (int i = g_numArenas - 1; i >= 0; i--) { // go in backwards order so it's alphabetized...nice video game
			char *value = Info_ValueForKey(g_arenaInfos[i], "type");
			if (VALIDSTRING(value) && strstr(value, "ctf")) {
				value = Info_ValueForKey(g_arenaInfos[i], "map");
				qboolean isWhitelisted = qfalse;
				if (whitelistedMapsList) {
					mapTierData_t *found = ListFind(whitelistedMapsList, MapTierDataMatches, value, NULL);
					if (found)
						isWhitelisted = qtrue;
				}

				if (!isWhitelisted) {
					char mapShortName[MAX_QPATH] = { 0 };
					GetShortNameForMapFileName(value, mapShortName, sizeof(mapShortName));
					Q_strcat(allMapsStr, sizeof(allMapsStr), va("%s%s", numGotten ? ", " : "", mapShortName));
					numGotten++;
				}
			}
		}
		if (numGotten) {
			Com_Printf("^1Maps currently NOT whitelisted: ^7");
			Com_Printf("%s^7\n", allMapsStr);
		}

		if (whitelistedMapsList)
			free(whitelistedMapsList);
	}
	else {
		Com_Printf("^7Usage%s:\n"
			"^7rcon tier add [map]    - whitelist a map so that it can be chosen in votes\n"
			"^9rcon tier remove [map] - remove a map from the whitelist\n"
			"^7rcon tier list         - view which maps currently are, or are not, whitelisted\n", g_vote_tierlist.integer ? "" : " (note: tierlist is currently disabled, enable with g_vote_tierlist)");
		return;
	}
}

static void PrintFixSwapHelp(void) {
	Com_Printf(	"Usage:\n"
				"fixswap list                  - lists pugs where someone has played more than one position\n"
				"fixswap fix [record #] [pos]  - set a record to, or merges it into, a specified position\n"
	);
}

void Svcmd_FixSwap_f(void) {
	if (trap_Argc() < 2) {
		PrintFixSwapHelp();
		return;
	}

	char arg1[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, arg1, sizeof(arg1));

	if (!Q_stricmp(arg1, "list")) {
		G_DBFixSwap_List();
	}
	else if (!Q_stricmp(arg1, "fix")) {
		char arg2[MAX_STRING_CHARS] = { 0 };
		trap_Argv(2, arg2, sizeof(arg2));
		if (!arg2[0] || !Q_isanumber(arg2)) {
			PrintFixSwapHelp();
			return;
		}
		int recordId = atoi(arg2);

		char arg3[MAX_STRING_CHARS] = { 0 };
		trap_Argv(3, arg3, sizeof(arg3));
		if (!arg3[0]) {
			PrintFixSwapHelp();
			return;
		}

		int pos = (int)CtfPositionFromString(arg3);
		if (!pos) {
			PrintFixSwapHelp();
			return;
		}

		if (G_DBFixSwap_Fix(recordId, pos)) {
			Com_Printf("Successful.\n");
			if (G_DBGetMetadataInteger("shouldReloadPlayerPugStats") != 2)
				G_DBSetMetadata("shouldReloadPlayerPugStats", "1");
		}
		else {
			Com_Printf("Failed!\n");
		}
	}
	else {
		PrintFixSwapHelp();
	}
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

#define REMOVEDVOTE_REROLLVOTE	(69)
static char reinstateVotes[MAX_CLIENTS] = { 0 }, removedVotes[MAX_CLIENTS] = { 0 };

// starts a multiple choices vote using some of the binary voting logic
static void StartMultiMapVote( const int numMaps, const qboolean hasWildcard, const char *listOfMaps ) {
	if ( level.voteTime ) {
		// stop the current vote because we are going to replace it
		level.voteStartTime = 0;
		level.voteTime = 0;
		g_entities[level.lastVotingClient].client->lastCallvoteTime = level.time;
	}

	G_LogPrintf( "A multi map vote was started: %s\n", listOfMaps );

	int numRedPlayers = 0, numBluePlayers = 0;
	CountPlayers(NULL, &numRedPlayers, &numBluePlayers, NULL, NULL, NULL, NULL);

	// start a "fake vote" so that we can use most of the logic that already exists
	Com_sprintf( level.voteString, sizeof( level.voteString ), "map_multi_vote %s", listOfMaps );
	if (!level.voteBanPhaseCompleted && g_vote_banMap.integer && ((numRedPlayers == 4 && numBluePlayers == 4) || (numRedPlayers == 5 && numBluePlayers == 5))) {
		Q_strncpyz(level.voteDisplayString, "^8Vote to ban a map in console", sizeof(level.voteDisplayString));
		level.voteBanPhase = qtrue;
	}
	else {
		Q_strncpyz(level.voteDisplayString, "^5Vote for a map in console", sizeof(level.voteDisplayString));
		level.voteBanPhase = qfalse;
	}
	level.voteStartTime = level.time;
	level.voteTime = level.time;

	if (g_vote_runoff.integer && g_vote_runoffTimeModifier.integer) { // allow shorter or longer time for runoff votes according to the cvar
		int timeChange = Com_Clampi(-15, 30, g_vote_runoffTimeModifier.integer);
		timeChange *= 1000;
		level.voteTime += timeChange;
	}
	else if (level.voteBanPhase && g_vote_banPhaseTimeModifier.integer) { // allow shorter or longer time for runoff votes according to the cvar
		int timeChange = Com_Clampi(-15, 30, g_vote_banPhaseTimeModifier.integer);
		timeChange *= 1000;
		level.voteTime += timeChange;
	}

	level.voteAutoPassOnExpiration = qfalse;
	level.voteYes = 0;
	level.voteNo = 0;
	// we don't set lastVotingClient since this isn't a "normal" vote
	level.multiVoting = qtrue;
	//level.inRunoff = qfalse;
	level.multiVoteHasWildcard = hasWildcard;
	level.multiVoteChoices = numMaps;
	memset( &( level.multiVotes ), 0, sizeof( level.multiVotes ) );
	memset(&(level.multiVoteBanVotes), 0, sizeof(level.multiVoteBanVotes));

	fixVoters( qfalse, 0 ); // racers can never vote in multi map votes

	// now reinstate votes, if needed
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!reinstateVotes[i] || level.clients[i].pers.connected != CON_CONNECTED ||
			(g_gametype.integer != GT_DUEL && g_gametype.integer != GT_POWERDUEL && level.clients[i].sess.sessionTeam == TEAM_SPECTATOR) ||
			(g_entities[i].r.svFlags & SVF_BOT))
			continue;
		int index = strchr(level.multiVoteMapChars, reinstateVotes[i]) - level.multiVoteMapChars;
		if (index >= 0) {
			level.multiVotes[i] = (strchr(level.multiVoteMapChars, reinstateVotes[i]) - level.multiVoteMapChars) + 1;
			G_LogPrintf("Client %d (%s) auto-voted for choice %s\n", i, level.clients[i].pers.netname, level.multiVoteMapShortNames[((int)reinstateVotes[i]) - 1]);
			level.voteYes++;
			trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
			level.clients[i].mGameFlags |= PSG_VOTED;
		}
	}
	memset(&reinstateVotes, 0, sizeof(reinstateVotes));

	trap_SetConfigstring( CS_VOTE_TIME, va( "%i", level.voteTime ) );
	trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );
	trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
	trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
}

typedef struct {
	char listOfMaps[MAX_STRING_CHARS];
	char printMessage[MAX_CLIENTS][MAX_STRING_CHARS]; // everyone gets a unique message
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

	if (!selection->announceMultiVote)
		return;

	char oneLineBannedMapStr[MAX_STRING_CHARS] = { 0 };
	if (level.voteBanPhaseCompleted) {
		if (level.bannedMapNames[TEAM_RED][0] && level.bannedMapNames[TEAM_BLUE][0]) {
			if (!Q_stricmp(level.bannedMapNames[TEAM_RED], level.bannedMapNames[TEAM_BLUE]))
				Com_sprintf(oneLineBannedMapStr, sizeof(oneLineBannedMapStr), "%s was banned by both teams\n", level.bannedMapNames[TEAM_RED]);
			else
				Com_sprintf(oneLineBannedMapStr, sizeof(oneLineBannedMapStr), "^1%s^7 and ^4%s^7 were banned\n", level.bannedMapNames[TEAM_RED], level.bannedMapNames[TEAM_BLUE]);
		}
		else if (level.bannedMapNames[TEAM_RED][0]) {
			Com_sprintf(oneLineBannedMapStr, sizeof(oneLineBannedMapStr), "^1%s^7 was banned\n", level.bannedMapNames[TEAM_RED]);
		}
		else if (level.bannedMapNames[TEAM_BLUE][0]) {
			Com_sprintf(oneLineBannedMapStr, sizeof(oneLineBannedMapStr), "^4%s^7 was banned\n", level.bannedMapNames[TEAM_BLUE]);
		}
	}

	if (selection->numSelected == 1) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (level.runoffSurvivors & (1llu << (unsigned long long)i)) {
				Q_strncpyz(selection->printMessage[i], va("Runoff vote: waiting for other players to vote...\nMaps still in contention:"), sizeof(selection->printMessage[i]));
			}
			else if (level.runoffLosers & (1llu << (unsigned long long)i)) {
				if (removedVotes[i] > 0 && removedVotes[i] != REMOVEDVOTE_REROLLVOTE) {
					char map[MAX_QPATH] = { 0 };
					if (level.multivoteWildcardMapFileName[0] && !Q_stricmp(level.multiVoteMapFileNames[((int)removedVotes[i]) - 1], level.multivoteWildcardMapFileName))
						Q_strncpyz(map, "Random map", sizeof(map));
					else
						Q_strncpyz(map, level.multiVoteMapShortNames[((int)removedVotes[i]) - 1], sizeof(map));
					Q_strncpyz(selection->printMessage[i], va("Runoff vote:\n"S_COLOR_RED"%s"S_COLOR_RED" was eliminated\n"S_COLOR_YELLOW"Please vote again"S_COLOR_WHITE, map), sizeof(selection->printMessage[i]));
				}
				else if (removedVotes[i] == REMOVEDVOTE_REROLLVOTE) {
					Q_strncpyz(selection->printMessage[i], "Runoff vote:\n"S_COLOR_RED"Reroll"S_COLOR_RED" was eliminated\n"S_COLOR_YELLOW"Please vote again"S_COLOR_WHITE, sizeof(selection->printMessage[i]));
				}
				else {
					Q_strncpyz(selection->printMessage[i], va("Runoff vote: "S_COLOR_YELLOW"please vote again"S_COLOR_WHITE), sizeof(selection->printMessage[i]));
				}
			}
			else if (level.voteBanPhase) {
				Q_strncpyz(selection->printMessage[i], va("Vote to ban your team's worst map:"), sizeof(selection->printMessage[i]));
			}
			else {
				if (level.successfulRerollVoters & (1llu << (unsigned long long)i))
					Q_strncpyz(selection->printMessage[i], va("%s%s%sote for a map%s:", oneLineBannedMapStr, level.mapsRerolled ? "^6Map choices rerolled^7\n" : "", level.inRunoff ? "Runoff v" : "V", g_vote_rng.integer ? " to increase its probability" : ""), sizeof(selection->printMessage[i]));
				else if (level.survivingRerollMapVoters & (1llu << (unsigned long long)i))
					Q_strncpyz(selection->printMessage[i], va("%s%s%sote for a map%s:", oneLineBannedMapStr, level.mapsRerolled ? "^6Map choices rerolled^7\n" : "", level.inRunoff ? "Runoff v" : "V", g_vote_rng.integer ? " to increase its probability" : ""), sizeof(selection->printMessage[i]));
				else
					Q_strncpyz(selection->printMessage[i], va("%s%s%sote for a map%s:", oneLineBannedMapStr, level.mapsRerolled ? "^6Map choices rerolled^7\n" : "", level.inRunoff ? "Runoff v" : "V", g_vote_rng.integer ? " to increase its probability" : ""), sizeof(selection->printMessage[i]));
			}
		}
	}

	// try to print the full map name to players (if a full name doesn't appear, map has no .arena or isn't installed)
	char *mapDisplayName = NULL;

	if (selection->hasWildcard && selection->mapsToRandomize == selection->numSelected) {
		// this will only happen on the first vote
		mapDisplayName = "^3A random map not above^7";
		Q_strncpyz(level.multivoteWildcardMapFileName, mapname, sizeof(level.multivoteWildcardMapFileName));
	}
	else {
		const char *arenaInfo = NULL;

		char overrideMapName[MAX_QPATH] = { 0 };
		if (g_vote_printLiveVersionFullName.integer && G_DBGetLiveMapNameForMapName(mapname, overrideMapName, sizeof(overrideMapName)) && overrideMapName[0])
			arenaInfo = G_GetArenaInfoByMap(overrideMapName);
		if (!arenaInfo)
			arenaInfo = G_GetArenaInfoByMap(mapname);

		if (arenaInfo) {
			mapDisplayName = Info_ValueForKey(arenaInfo, "longname");
			Q_CleanStr(mapDisplayName);
		}

		if (!VALIDSTRING(mapDisplayName)) {
			mapDisplayName = mapname;
		}
	}

	int thisMapVoteNum = 0;
	if (level.inRunoff || level.voteBanPhaseCompleted) { // find the *original* number
		for (int i = 0; i < MAX_MULTIVOTE_MAPS; i++) {
			if (!Q_stricmp(level.multiVoteMapFileNames[i], mapname)) {
				thisMapVoteNum = i + 1;
				break;
			}
		}
		if (!thisMapVoteNum)
			thisMapVoteNum = selection->numSelected; // should never happen
	}
	else {
		level.multiVoteMapChars[selection->numSelected - 1] = selection->numSelected;
		thisMapVoteNum = selection->numSelected;

		char mapShortName[MAX_QPATH] = { 0 };
		GetShortNameForMapFileName(mapname, mapShortName, sizeof(mapShortName));
		mapShortName[0] = toupper((unsigned char)mapShortName[0]);
		Q_strncpyz(level.multiVoteMapShortNames[selection->numSelected - 1], mapShortName, sizeof(level.multiVoteMapShortNames[selection->numSelected - 1]));
		Q_strncpyz(level.multiVoteMapFileNames[selection->numSelected - 1], mapname, sizeof(level.multiVoteMapFileNames[selection->numSelected - 1]));
	}

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (level.runoffSurvivors & (1llu << (unsigned long long)i)) {
			Q_strcat(selection->printMessage[i], sizeof(selection->printMessage[i]),
				va("\n"S_COLOR_WHITE"%d - %s", thisMapVoteNum, mapDisplayName)
			);
		}
		else {
			if (level.voteBanPhase) {
				Q_strcat(selection->printMessage[i], sizeof(selection->printMessage[i]),
					va("\n^8/ban %d ^7 - %s", thisMapVoteNum, mapDisplayName));
			}
			else {
				Q_strcat(selection->printMessage[i], sizeof(selection->printMessage[i]),
					va("\n^5/vote %d ^7 - %s", thisMapVoteNum, mapDisplayName));
			}
		}
	}

	level.mapsThatCanBeVotedBits |= (1 << (thisMapVoteNum - 1));
}

extern void CountPlayersIngame( int *total, int *ingame, int *inrace );
void Svcmd_MapVote_f(const char *overrideMaps);

void Svcmd_MapRandom_f()
{
	char pool[64];
	int mapsToRandomize;

	if (trap_Argc() < 2) {
		return;
	}

	level.autoStartPending = qfalse;

    trap_Argv( 1, pool, sizeof( pool ) );

	if (g_redirectPoolVoteToTierListVote.string[0] && pool[0] && !Q_stricmp(pool, g_redirectPoolVoteToTierListVote.string) && g_vote_tierlist.integer) {
		Svcmd_MapVote_f(NULL);
		return;
	}

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
	
	MapSelectionContext *context = malloc(sizeof(MapSelectionContext));
	memset(context, 0, sizeof(MapSelectionContext));

	if ( mapsToRandomize > 1 ) { // if we are randomizing more than one map, there will be a second vote
		if ( level.multiVoting && level.voteTime ) {
			free(context);
			return; // theres already a multi vote going on, retard
		}

		if ( level.voteExecuteTime && level.voteExecuteTime < level.time ) {
			free(context);
			return; // another vote just passed and is waiting to execute, don't interrupt it...
		}

		context->announceMultiVote = qtrue;

		// in order to make the option work, we randomize the wildcard map here along with the other choices
		// this avoids having to randomize it later while excluding the other choices
		// so even though the wildcard option is actually "pre-determined", but hidden, the effect is the same
		if ( g_enable_maprandom_wildcard.integer ) {
			context->hasWildcard = qtrue;
			++mapsToRandomize;
		}
	}

	context->mapsToRandomize = mapsToRandomize;
	level.mapsThatCanBeVotedBits = 0;


	int numRedPlayers = 0, numBluePlayers = 0;
	CountPlayers(NULL, &numRedPlayers, &numBluePlayers, NULL, NULL, NULL, NULL);
	if (!level.voteBanPhaseCompleted && g_vote_banMap.integer && ((numRedPlayers == 4 && numBluePlayers == 4) || (numRedPlayers == 5 && numBluePlayers == 5)))
		level.voteBanPhase = qtrue;

	if (G_DBSelectMapsFromPool(pool, currentMap, context->mapsToRandomize, mapSelectedCallback, context)) {
		if (context->numSelected != context->mapsToRandomize) {
			G_Printf("Could not randomize this many maps! Expected %d, but randomized %d\n", context->mapsToRandomize, context->numSelected);
		}

		// print in console and do a global prioritized center print
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (g_vote_runoffRerollOption.integer && !g_vote_banMap.integer && !level.runoffRoundsCompletedIncludingRerollRound && !stristr(context->printMessage[i], "\n"S_COLOR_MAGENTA"/vote r "S_COLOR_WHITE" - Reroll choices"))
				Q_strcat(context->printMessage[i], sizeof(context->printMessage[i]), "\n"S_COLOR_MAGENTA"/vote r "S_COLOR_WHITE" - Reroll choices");

			if (!level.inRunoff) // don't spam the console for non-first rounds
				trap_SendServerCommand(i, va("print \"%s\n\"", context->printMessage[i]));

			// prepend newlines after printing in console but before centerprinting
			Com_sprintf(context->printMessage[i], sizeof(context->printMessage[i]), "\n\n%s", context->printMessage[i]);
		}
		G_UniqueTickedCenterPrint(context->printMessage, sizeof(context->printMessage[0]), 20000, qtrue); // give them 20s to see the options

		if (context->numSelected > 1) {
			// we are going to need another vote for this...
			StartMultiMapVote(context->numSelected, context->hasWildcard, context->listOfMaps);
		}
		else {
			// we have 1 map, this means listOfMaps only contains 1 randomized map. Just change to it straight away.
			char overrideMapName[MAX_QPATH] = { 0 };
			G_DBGetLiveMapNameForMapName(context->listOfMaps, overrideMapName, sizeof(overrideMapName));
			if (overrideMapName[0] && Q_stricmp(overrideMapName, context->listOfMaps)) {
				Com_Printf("Overriding %s via map alias to %s\n", context->listOfMaps, overrideMapName);
#ifdef _DEBUG
				trap_SendConsoleCommand(EXEC_APPEND, va("devmap %s\n", overrideMapName));
#else
				trap_SendConsoleCommand(EXEC_APPEND, va("map %s\n", overrideMapName));
#endif
			}
			else {
#ifdef _DEBUG
				trap_SendConsoleCommand(EXEC_APPEND, va("devmap %s\n", context->listOfMaps));
#else
				trap_SendConsoleCommand(EXEC_APPEND, va("map %s\n", context->listOfMaps));
#endif
			}
		}
		free(context);
		return;
	}
	free(context);
    G_Printf( "Map pool '%s' not found\n", pool );
}

void Svcmd_MapVote_f(const char *overrideMaps) {
	if (!g_vote_tierlist.integer && !level.inRunoff && !level.voteBanPhaseCompleted && !VALIDSTRING(overrideMaps)) {
		Com_Printf("g_vote_tierlist is disabled.\n");
		return;
	}

	int mapsToRandomize = Com_Clampi(2, MAX_MULTIVOTE_MAPS, g_vote_tierlist_totalMaps.integer);
	if (!level.inRunoff && !level.voteBanPhaseCompleted && !VALIDSTRING(overrideMaps) &&
		((g_vote_tierlist_s_max.integer + g_vote_tierlist_a_max.integer + g_vote_tierlist_b_max.integer +
		g_vote_tierlist_c_max.integer + g_vote_tierlist_f_max.integer < mapsToRandomize) ||
		(g_vote_tierlist_s_min.integer + g_vote_tierlist_a_min.integer + g_vote_tierlist_b_min.integer +
		g_vote_tierlist_c_min.integer + g_vote_tierlist_f_min.integer > mapsToRandomize))) {
		Com_Printf("Error: min/max conflict with g_vote_tierlist_totalMaps! Unable to use tierlist voting.\n");
		return;
	}

	int ingame;
	CountPlayersIngame(NULL, &ingame, NULL);
	qboolean justPickOneMap = qfalse;
	if (ingame < 2) { // how? this should have been handled in callvote code, maybe some stupid admin directly called it with nobody in game..?
						// or it could also be caused by people joining spec before map_random passes... so inform them, i guess
		G_Printf("Not enough people in game to start a multi vote, a single map will be randomized\n");
		justPickOneMap = qtrue;
	}

	level.autoStartPending = qfalse;

	const char *currentMap = level.mapname;

	if (level.multiVoting && level.voteTime) {
		return; // theres already a multi vote going on, retard
	}

	if (level.voteExecuteTime && level.voteExecuteTime < level.time) {
		return; // another vote just passed and is waiting to execute, don't interrupt it...
	}

	MapSelectionContext *context = malloc(sizeof(MapSelectionContext));
	memset(context, 0, sizeof(MapSelectionContext));

	context->announceMultiVote = qtrue;
	context->hasWildcard = qfalse; // tierlist doesn't use wildcard
	context->mapsToRandomize = mapsToRandomize;
	level.mapsThatCanBeVotedBits = 0;

	int numRedPlayers = 0, numBluePlayers = 0;
	CountPlayers(NULL, &numRedPlayers, &numBluePlayers, NULL, NULL, NULL, NULL);
	if (!level.voteBanPhaseCompleted && g_vote_banMap.integer && ((numRedPlayers == 4 && numBluePlayers == 4) || (numRedPlayers == 5 && numBluePlayers == 5)))
		level.voteBanPhase = qtrue;

	if (VALIDSTRING(overrideMaps)) {
		int len = strlen(overrideMaps);
		for (int i = 0; i < len; i++) {
			mapSelectedCallback(context, level.multiVoteMapFileNames[((int)overrideMaps[i]) - 1]);
		}
	}
	else {
		if (!G_DBSelectTierlistMaps(mapSelectedCallback, context)) {
			Com_Printf("Error selecting tier list maps.\n");
			free(context);
			return;
		}
	}

	context->mapsToRandomize = g_vote_tierlist_totalMaps.integer;

	if (!level.inRunoff && !level.voteBanPhaseCompleted && context->numSelected != context->mapsToRandomize) {
		G_Printf("Could not randomize this many maps! Expected %d, but randomized %d\n", context->mapsToRandomize, context->numSelected);
	}

	// print in console and do a global prioritized center print
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (g_vote_runoffRerollOption.integer && !g_vote_banMap.integer && !level.runoffRoundsCompletedIncludingRerollRound && !stristr(context->printMessage[i], "\n"S_COLOR_MAGENTA"/vote r "S_COLOR_WHITE" - Reroll choices"))
			Q_strcat(context->printMessage[i], sizeof(context->printMessage[i]), "\n"S_COLOR_MAGENTA"/vote r "S_COLOR_WHITE" - Reroll choices");

		if (!level.inRunoff) // don't spam the console for non-first rounds
			trap_SendServerCommand(i, va("print \"%s\n\"", context->printMessage[i]));

		// prepend newlines after printing in console but before centerprinting
		Com_sprintf(context->printMessage[i], sizeof(context->printMessage[i]), "\n\n%s", context->printMessage[i]);
	}
	G_UniqueTickedCenterPrint(context->printMessage, sizeof(context->printMessage[0]), 20000, qtrue); // give them 20s to see the options

	if (!level.inRunoff && !level.voteBanPhaseCompleted && g_vote_tierlist_reminders.integer) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (g_entities[i].inuse && level.clients[i].pers.connected == CON_CONNECTED && !(g_entities[i].r.svFlags & SVF_BOT) && level.clients[i].account &&
				(level.clients[i].sess.sessionTeam == TEAM_RED || level.clients[i].sess.sessionTeam == TEAM_BLUE)) {
				int numRated = G_GetNumberOfMapsRatedByPlayer(level.clients[i].account->id);
				if (!numRated)
					PrintIngame(i, "*%s^7, because you haven't rated any maps, you aren't influencing the map selection system! To rate maps, enter ^5tier set <map> <rating>^7 in the console (valid tiers are S, A, B, C, F).\n", level.clients[i].account->name);
				else if (numRated < 10)
					PrintIngame(i, "*%s^7, you can better influence the map selection system by rating more maps. To rate maps, enter ^5tier set <map> <rating>^7 in the console (valid tiers are S, A, B, C, F).\n", level.clients[i].account->name);
			}
		}
	}

	if (justPickOneMap || context->numSelected == 1) {
		// we have 1 map, just change to it straight away.
		char *space = strchr(context->listOfMaps, ' ');
		if (space)
			*space = '\0';
		char overrideMapName[MAX_QPATH] = { 0 };
		G_DBGetLiveMapNameForMapName(context->listOfMaps, overrideMapName, sizeof(overrideMapName));
		if (overrideMapName[0] && Q_stricmp(overrideMapName, context->listOfMaps)) {
			Com_Printf("Overriding %s via map alias to %s\n", context->listOfMaps, overrideMapName);
#ifdef _DEBUG
			trap_SendConsoleCommand(EXEC_APPEND, va("devmap %s\n", overrideMapName));
#else
			trap_SendConsoleCommand(EXEC_APPEND, va("map %s\n", overrideMapName));
#endif
		}
		else {
#ifdef _DEBUG
			trap_SendConsoleCommand(EXEC_APPEND, va("devmap %s\n", context->listOfMaps));
#else
			trap_SendConsoleCommand(EXEC_APPEND, va("map %s\n", context->listOfMaps));
#endif
		}
	}
	else {
		// we are going to need another vote for this...
		StartMultiMapVote(context->numSelected, context->hasWildcard, context->listOfMaps);
	}

	free(context);
}

qboolean DoRunoff(void) {
	if (!g_vote_runoff.integer && !level.inRunoff)
		return qfalse;
	int numChoices = 0;
	for (int i = 0; i < MAX_MULTIVOTE_MAPS; i++)
		if (level.multiVoteMapFileNames[i][0])
			numChoices++;

	level.runoffSurvivors = level.runoffLosers = level.successfulRerollVoters = 0llu;

	// count the votes
	int numVoters = qfalse;
	int numVotesForMap[64] = { 0 }, numVotesToReroll = 0;
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		int voteId = level.multiVotes[i];
		if (voteId > 0 && voteId <= numChoices) {
			numVotesForMap[voteId - 1]++;
			numVoters++;
		}
		else if (g_vote_runoffRerollOption.integer && !g_vote_banMap.integer && voteId == -1 && !level.runoffRoundsCompletedIncludingRerollRound) {
			numVotesToReroll++;
			numVoters++;
		}
	}

	if (!numVoters)
		return qfalse; // nobody has voted whatsoever; don't do anything

	// determine the highest and lowest numbers
	int lowestNumVotes = 0x7FFFFFFF, highestNumVotes = 0;
	for (int i = 0; i < numChoices; i++) {
		if (numVotesForMap[i] > 0 && numVotesForMap[i] < lowestNumVotes)
			lowestNumVotes = numVotesForMap[i];
		if (numVotesForMap[i] > highestNumVotes)
			highestNumVotes = numVotesForMap[i];
	}

	// determine which maps are tied for highest/lowest/zero
	int numMapsTiedForFirst = 0, numMapsNOTTiedForFirst = 0, numMapsTiedForLast = 0;
	for (int i = 0; i < numChoices; i++) {
		if (numVotesForMap[i] == highestNumVotes)
			numMapsTiedForFirst++;
		else if (numVotesForMap[i] > 0 && numVotesForMap[i] < highestNumVotes) {
			numMapsNOTTiedForFirst++;
			if (numVotesForMap[i] == lowestNumVotes) {
				numMapsTiedForLast++;
			}
		}
	}

	// if reroll is enabled and ANY person voted for it, we handle all of that here and then return
	if (g_vote_runoffRerollOption.integer && !g_vote_banMap.integer && !level.runoffRoundsCompletedIncludingRerollRound && numVotesToReroll) {
		if (numVotesToReroll > highestNumVotes && numVotesToReroll > (numVoters / 2)) {
			// reroll has a majority (not just plurality); do it

			// note both:
			// - a maximum of one map tied for the most votes, so we can make sure it is still part of the list after rerolling (provided S or A tier) - forceInclude qtrue
			// - all other maps, so we can make sure they are NOT part of the list after rerolling - forceInclude qfalse
			int survivorIndex = -1;
			for (int i = 0; i < numChoices; i++) {
				rememberedMultivoteMap_t *remember = ListAdd(&level.rememberedMultivoteMapsList, sizeof(rememberedMultivoteMap_t));
				Q_strncpyz(remember->mapFilename, level.multiVoteMapFileNames[i], sizeof(remember->mapFilename));
				remember->forceInclude = !!(numVotesForMap[i] > 1 && numVotesForMap[i] == highestNumVotes && survivorIndex == -1);
				if (remember->forceInclude) {
					remember->position = i; // note position of map that will survive the reroll, so we can put it in the same place after the reroll
					survivorIndex = i;
				}
			}

			// note the victors and losers so we can display unique messages to people,
			// and so we can reinstate votes of people whose map is going to survive the reroll
			memset(&reinstateVotes, 0, sizeof(reinstateVotes));
			unsigned long long failedNonRerollers = 0llu;
			for (int i = 0; i < MAX_CLIENTS; ++i) {
				int voteId = level.multiVotes[i];
				if (voteId == -1) {
					level.successfulRerollVoters |= (1llu << (unsigned long long)i);
				}
				else if (voteId > 0) {
					if (numVotesForMap[voteId - 1] > 1 && numVotesForMap[voteId - 1] == highestNumVotes && survivorIndex == voteId - 1) {
						reinstateVotes[i] = level.multiVoteMapChars[level.multiVotes[i] - 1];
						level.survivingRerollMapVoters |= (1llu << (unsigned long long)i);
					}
					else {
						failedNonRerollers |= (1llu << (unsigned long long)i);
					}
				}
			}

			// reset
			G_GlobalTickedCenterPrint("", 0, qfalse);
			g_entities[level.lastVotingClient].client->lastCallvoteTime = level.time;
			level.voteAutoPassOnExpiration = qfalse;
			level.multiVoting = qfalse;
			//level.voteBanPhase = qfalse;
			level.inRunoff = qfalse;
			level.multiVoteChoices = 0;
			level.multiVoteHasWildcard = qfalse;
			level.multivoteWildcardMapFileName[0] = '\0';
			level.mapsThatCanBeVotedBits = 0;
			//level.multiVoteTimeExtensions = 0;
			level.runoffSurvivors = level.runoffLosers/* = level.successfulRerollVoters*/ = 0llu;
			memset(level.multiVotes, 0, sizeof(level.multiVotes));
			memset(&(level.multiVoteBanVotes), 0, sizeof(level.multiVoteBanVotes));
			memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
			memset(&level.multiVoteMapShortNames, 0, sizeof(level.multiVoteMapShortNames));
			level.voteStartTime = 0;
			level.voteTime = 0;
			trap_SetConfigstring(CS_VOTE_TIME, "");

			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (level.clients[i].pers.connected != CON_CONNECTED || g_entities[i].r.svFlags & SVF_BOT)
					continue;
				if (failedNonRerollers & (1llu << (unsigned long long)i))
					trap_SendServerCommand(i, va("print \"Map choices rerolled. You must re-vote.\n\""));
				else if (level.survivingRerollMapVoters & (1llu << (unsigned long long)i))
					trap_SendServerCommand(i, va("print \"Map choices rerolled.\n\""));
				else
					trap_SendServerCommand(i, va("print \"Map choices rerolled. You may re-vote.\n\""));
			}

			// start new vote
			++level.runoffRoundsCompletedIncludingRerollRound;
			level.mapsRerolled = qtrue;
			Com_Printf("Map choices rerolled.\n");
			Svcmd_MapVote_f(NULL);
		}
		else {
			++level.runoffRoundsCompletedIncludingRerollRound;
			Com_Printf("Completed reroll round without reroll.\n");

			int numFailedRerollers = 0;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				int voteId = level.multiVotes[i];
				if (voteId == -1) { // this guy's reroll vote was removed
					level.clients[i].mGameFlags &= ~PSG_VOTED;
					level.runoffLosers |= (1llu << (unsigned long long)i);
					removedVotes[i] = REMOVEDVOTE_REROLLVOTE;
					numFailedRerollers++;
					G_LogPrintf("Client %d (%s) had their \"%s\" reroll vote removed.\n", i, level.clients[i].pers.netname, level.multiVoteMapShortNames[((int)level.multiVoteMapChars[voteId - 1]) - 1]);
					NotifyTeammatesOfVote(&g_entities[i], "'s reroll vote was removed", "^5");
				}
				else { // this guy's map survived; his vote will be reinstated
					reinstateVotes[i] = level.multiVoteMapChars[level.multiVotes[i] - 1];
					level.runoffSurvivors |= (1llu << (unsigned long long)i);
				}
			}

			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (level.clients[i].pers.connected != CON_CONNECTED || g_entities[i].r.svFlags & SVF_BOT)
					continue;
				if (level.runoffLosers & (1llu << (unsigned long long)i)) {
					if (numFailedRerollers == 1)
						trap_SendServerCommand(i, va("print \"Reroll option eliminated. You may re-vote.\n\""));
					else
						trap_SendServerCommand(i, va("print \"Reroll option eliminated. You and %d other player%s may re-vote.\n\"", numFailedRerollers - 1, numFailedRerollers - 1 == 1 ? "" : "s"));
				}
				else {
					trap_SendServerCommand(i, va("print \"Reroll option eliminated. %d player%s may re-vote.\n\"", numFailedRerollers, numFailedRerollers == 1 ? "" : "s"));
				}
			}

			// start the next round of voting
			level.multiVoting = qfalse;
			level.voteBanPhase = qfalse; // sanity check
			level.inRunoff = qtrue;
			++level.runoffRoundsCompletedIncludingRerollRound;
			Svcmd_MapVote_f(level.multiVoteMapChars);
			return qtrue;
		}
		return qtrue;
	}

	if (!numMapsNOTTiedForFirst && numMapsTiedForFirst == 2) {
		// it's a tie between two maps; let rng decide
		return qfalse;
	}

	if (numMapsTiedForFirst == 1 && numMapsNOTTiedForFirst <= 1) {
		// exactly one map is in first place AND exactly one map is in last place
		// or exactly one map got votes
		// let the normal routine pick the first place map as winner
		return qfalse;
	}

	// if we got here, at least one map will be eliminated
	// determine which maps made the cut and which will be eliminated
	char newMapChars[MAX_MULTIVOTE_MAPS + 1] = { 0 }, choppingBlock[MAX_MULTIVOTE_MAPS + 1] = { 0 };
	for (int i = 0; i < numChoices; i++) {
		if (numVotesForMap[i] <= 0) { // this map has exactly 0 votes; remove it
			continue;
		}
		else if (highestNumVotes == lowestNumVotes) { // a tie; add this one to the chopping block
			char buf[2] = { 0 };
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(choppingBlock, sizeof(choppingBlock), buf);
		}
		else if (numVotesForMap[i] >= highestNumVotes) { // this map is tied for first; add it
			char buf[2] = { 0 };
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(newMapChars, sizeof(newMapChars), buf);
		}
		else if (numVotesForMap[i] == lowestNumVotes) { // this map is non-zero and tied for last
			if (numMapsTiedForLast <= 1) { // only one map is tied for last; remove it
				continue;
			}
			else { // multiple maps are tied for last; add this one to the chopping block
				char buf[2] = { 0 };
				buf[0] = level.multiVoteMapChars[i];
				Q_strcat(choppingBlock, sizeof(choppingBlock), buf);
			}
		}
		else { // this map is non-zero, not in first place, but not in last place; add it
			char buf[2] = { 0 };
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(newMapChars, sizeof(newMapChars), buf);
		}
	}

	// if needed, randomly remove one map from the chopping block and add the others
	int numOnChoppingBlock = strlen(choppingBlock);
	if (numOnChoppingBlock > 0) {
		// try to remove a less played map rather than a random one
		int leastPlayedCount = INT_MAX;
		int leastPlayedIndex = -1;
		if (g_vote_lessPlayedMapsDisfavoredInRunoffEliminations.integer > 0 && numOnChoppingBlock > 1) {
			for (int i = 0; i < numOnChoppingBlock; i++) {
				int mapIndex = strchr(level.multiVoteMapChars, choppingBlock[i]) - level.multiVoteMapChars;
				if (mapIndex >= 0 && mapIndex < MAX_MULTIVOTE_MAPS && level.multiVoteMapFileNames[mapIndex][0]) {
					const char *mapFilename = level.multiVoteMapFileNames[mapIndex];
					int playCount = G_DBNumTimesPlayedSingleMap(mapFilename);
					if (playCount < leastPlayedCount && !DB_IsTopMap(mapFilename, g_vote_lessPlayedMapsDisfavoredInRunoffEliminations.integer)) {
						leastPlayedCount = playCount;
						leastPlayedIndex = i;
					}
				}
			}
		}

		if (leastPlayedIndex >= 0) {
			// remove a specific least played map by copying to newChoppingBlock
			char newChoppingBlock[MAX_MULTIVOTE_MAPS + 1] = { 0 };
			int newChoppingBlockIndex = 0;
			for (int i = 0; i < numOnChoppingBlock; i++) {
				if (i != leastPlayedIndex) {
					newChoppingBlock[newChoppingBlockIndex++] = choppingBlock[i];
				}
			}
			// concatenate remaining maps to newMapChars
			Q_strcat(newMapChars, sizeof(newMapChars), newChoppingBlock);
		}
		else {
			// no valid least played map found, fallback to random removal: put them in a random order and null out the last one
			for (int i = numOnChoppingBlock - 1; i >= 1; i--) { // fisher-yates
				int j = rand() % (i + 1);
				int temp = choppingBlock[i];
				choppingBlock[i] = choppingBlock[j];
				choppingBlock[j] = temp;
			}
			choppingBlock[numOnChoppingBlock - 1] = '\0';
			Q_strcat(newMapChars, sizeof(newMapChars), choppingBlock);
		}
	}

	// re-order the remaining maps to match the original order
	// and note which maps got eliminated (including ones with zero votes)
	int numOldMaps = strlen(level.multiVoteMapChars);
	char temp[MAX_MULTIVOTE_MAPS + 1] = { 0 }, eliminatedMaps[MAX_MULTIVOTE_MAPS + 1] = { 0 };
	for (int i = 0; i < numOldMaps; i++) {
		char *p = strchr(newMapChars, level.multiVoteMapChars[i]), buf[2] = { 0 };
		if (VALIDSTRING(p)) { // this map made the cut
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(temp, sizeof(temp), buf);
		}
		else { // this map was eliminated
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(eliminatedMaps, sizeof(eliminatedMaps), buf);
			G_LogPrintf("Choice %s was eliminated from contention.\n", level.multiVoteMapShortNames[((int)buf[0]) - 1]);
		}
	}
	if (temp[0])
		memcpy(newMapChars, temp, sizeof(newMapChars));
	int numRemainingMaps = strlen(newMapChars);

	// determine which people will be happy or sad
	// mark still-valid choices as needing to be reinstated in the next round
	memset(&reinstateVotes, 0, sizeof(reinstateVotes));
	memset(&removedVotes, 0, sizeof(removedVotes));
	int numRunoffLosers = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		int voteId = level.multiVotes[i];
		if (level.clients[i].pers.connected == CON_CONNECTED && voteId > 0 && voteId <= numChoices) {
			if (!strchr(newMapChars, level.multiVoteMapChars[voteId - 1])) { // this guy's map was removed
				level.clients[i].mGameFlags &= ~PSG_VOTED;
				level.runoffLosers |= (1llu << (unsigned long long)i);
				removedVotes[i] = level.multiVoteMapChars[level.multiVotes[i] - 1];
				numRunoffLosers++;
				G_LogPrintf("Client %d (%s) had their \"%s\" vote removed.\n", i, level.clients[i].pers.netname, level.multiVoteMapShortNames[((int)level.multiVoteMapChars[voteId - 1]) - 1]);
				NotifyTeammatesOfVote(&g_entities[i], "'s vote was removed", "^5");
			}
			else { // this guy's map survived; his vote will be reinstated
				reinstateVotes[i] = level.multiVoteMapChars[level.multiVotes[i] - 1];
				level.runoffSurvivors |= (1llu << (unsigned long long)i);
			}
		}
		if (numRemainingMaps > 1)
			level.multiVotes[i] = 0; // if there will be another round, reset everyone's vote (some will be reinstated anyway)
	}

	// notify players which map(s) got removed
	if (numRemainingMaps > 1) {
		int numEliminatedMaps = strlen(eliminatedMaps);
		if (numEliminatedMaps > 0) {
			// create the string containing the eliminated map names
			char removedMapNamesStr[MAX_STRING_CHARS] = { 0 };
			for (int i = 0; i < numEliminatedMaps; i++) {
				char buf[MAX_QPATH] = { 0 };
				if (level.multiVoteHasWildcard && (int)(eliminatedMaps[i]) == numChoices)
					Q_strncpyz(buf, "Random map", sizeof(buf));
				else
					Q_strncpyz(buf, level.multiVoteMapShortNames[((int)eliminatedMaps[i]) - 1], sizeof(buf));
				if (numEliminatedMaps == 2 && i == 1) {
					Q_strcat(removedMapNamesStr, sizeof(removedMapNamesStr), " and ");
				}
				else if (numEliminatedMaps > 2 && i > 0) {
					Q_strcat(removedMapNamesStr, sizeof(removedMapNamesStr), ", ");
					if (i == numEliminatedMaps - 1)
						Q_strcat(removedMapNamesStr, sizeof(removedMapNamesStr), "and ");
				}
				Q_strcat(removedMapNamesStr, sizeof(removedMapNamesStr), buf);
			}
			Q_strcat(removedMapNamesStr, sizeof(removedMapNamesStr), numEliminatedMaps == 1 ? " was" : " were");
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (level.clients[i].pers.connected != CON_CONNECTED || g_entities[i].r.svFlags & SVF_BOT)
					continue;
				if (level.runoffLosers & (1llu << (unsigned long long)i)) {
					if (numRunoffLosers == 1)
						trap_SendServerCommand(i, va("print \"%s eliminated from contention. You may re-vote.\n\"", removedMapNamesStr));
					else
						trap_SendServerCommand(i, va("print \"%s eliminated from contention. You and %d other player%s may re-vote.\n\"", removedMapNamesStr, numRunoffLosers - 1, numRunoffLosers - 1 == 1 ? "" : "s"));
				}
				else {
					trap_SendServerCommand(i, va("print \"%s eliminated from contention. %d player%s may re-vote.\n\"", removedMapNamesStr, numRunoffLosers, numRunoffLosers == 1 ? "" : "s"));
				}
			}
		}
		// start the next round of voting
		level.multiVoting = qfalse;
		//level.voteBanPhase = qfalse;
		level.inRunoff = qtrue;
		++level.runoffRoundsCompletedIncludingRerollRound;
		Svcmd_MapVote_f(newMapChars);
		return qtrue;
	}
	else {
		// everything was eliminated except one map
		// let the normal routine select it as the winner
		return qfalse;
	}
}

// "scenario" is just for logging/debugging purposes
static void ChangeVote(int team, int mapId, int scenario) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || !ent->client->pers.connected || ent->client->sess.sessionTeam != team ||
			!(ent->client->mGameFlags & PSG_CANVOTE) ||
			level.multiVotes[i] == mapId) {
			continue;
		}

		level.multiVotes[i] = mapId;
		Com_Printf("[Troll vote scenario %d] Automatically made client %d troll vote for map %d with team\n", scenario, i, mapId);
		NotifyTeammatesOfVote(ent, va(" auto-forced to vote ^6%d^5%s",
			mapId,
			level.multiVoteMapShortNames[mapId - 1][0] ? va(" - %s", level.multiVoteMapShortNames[mapId - 1]) : ""), "^5");
	}
}

// allocates an array of length numChoices containing the sorted results from level.multiVoteChoices
// note that numVotes **INCLUDES*** numRerollVotes
// don't forget to FREE THE RESULT
int* BuildVoteResults( int numChoices, int *numVotes, int *highestVoteCount, qboolean *dontEndDueToMajority, int *numRerollVotes, qboolean canChangeAfkVotes) {
	if (level.inRunoff || level.voteBanPhaseCompleted) {
		numChoices = 0;
		for (int i = 0; i < MAX_MULTIVOTE_MAPS; i++)
			if (level.multiVoteMapFileNames[i][0])
				numChoices++;
	}
	int *voteResults = calloc( numChoices, sizeof( *voteResults ) ); // voteResults[i] = how many votes for the i-th choice

	// ensure that afk voter correction never happens if we are still rerolling
	qboolean someoneVotedToReroll = qfalse;
	for (int i = 0; i < MAX_CLIENTS; ++i) {
		int voteId = level.multiVotes[i];
		if (voteId == -1 && g_vote_runoffRerollOption.integer && !g_vote_banMap.integer) {
			someoneVotedToReroll = qtrue;
			break;
		}
	}
	if (someoneVotedToReroll)
		canChangeAfkVotes = qfalse;

	// fix troll cases in 4v4 which one team has 4 map A voters,
	// and the other team has 3 map B voters and either the last guy is a treasonous map A voter or he didn't vote at all
	if (g_vote_overrideTrollVoters.integer) {
		int numRedPlayers = 0, numBluePlayers = 0, numRedVotes = 0, numBlueVotes = 0, numMapsVotedByRedTeam = 0, numMapsVotedByBlueTeam = 0, highestVoteCountRedTeam = 0, highestVoteCountBlueTeam = 0;
		int mapIdWithHighestVoteCountRedTeam = 0, mapIdWithHighestVoteCountBlueTeam = 0;
		int secondPlaceMapIdRedTeam = 0, secondPlaceMapIdBlueTeam = 0;
		/*int secondPlaceMapIdRedTeam = 0, secondPlaceMapIdBlueTeam = 0;*/
		qboolean mapHas3OrMoreVotes = qfalse, mapHas4OrMoreVotes = qfalse, mapHas5OrMoreVotes = qfalse, mapHas6OrMoreVotes = qfalse;
		{
			int numVotesForMap[32] = { 0 }, numVotesForMapRedTeam[32] = { 0 }, numVotesForMapBlueTeam[32] = { 0 };
			int mapBitsVotedByRedTeam = 0, mapBitsVotedByBlueTeam = 0;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *ent = &g_entities[i];
				if (!ent->inuse || !ent->client || !ent->client->pers.connected ||
					(ent->client->sess.sessionTeam != TEAM_RED && ent->client->sess.sessionTeam != TEAM_BLUE) ||
					!(ent->client->mGameFlags & PSG_CANVOTE)) {
					continue;
				}

				if (ent->client->sess.sessionTeam == TEAM_RED)
					++numRedPlayers;
				else if (ent->client->sess.sessionTeam == TEAM_BLUE)
					++numBluePlayers;

				int voteId = level.multiVotes[i];
				if (voteId > 0 && voteId <= numChoices) {
					++numVotesForMap[voteId];
					if (numVotesForMap[voteId] >= 6) {
						mapHas6OrMoreVotes = mapHas5OrMoreVotes = mapHas4OrMoreVotes = mapHas3OrMoreVotes = qtrue;
					}
					else if (numVotesForMap[voteId] >= 5) {
						mapHas5OrMoreVotes = mapHas4OrMoreVotes = mapHas3OrMoreVotes = qtrue;
					}
					else if (numVotesForMap[voteId] >= 4) {
						mapHas4OrMoreVotes = mapHas3OrMoreVotes = qtrue;
					}
					else if (numVotesForMap[voteId] >= 3) {
						mapHas3OrMoreVotes = qtrue;
					}



					if (ent->client->sess.sessionTeam == TEAM_RED) {
						++numRedVotes;
						++numVotesForMapRedTeam[voteId];
						mapBitsVotedByRedTeam |= (1 << voteId);
					}
					else if (ent->client->sess.sessionTeam == TEAM_BLUE) {
						++numBlueVotes;
						++numVotesForMapBlueTeam[voteId];
						mapBitsVotedByBlueTeam |= (1 << voteId);
					}
				}
			}

			for (int i = 1; i <= numChoices; i++) {
				if (numVotesForMapRedTeam[i] > highestVoteCountRedTeam) {
					highestVoteCountRedTeam = numVotesForMapRedTeam[i];
					mapIdWithHighestVoteCountRedTeam = i;
				}
				if (numVotesForMapBlueTeam[i] > highestVoteCountBlueTeam) {
					highestVoteCountBlueTeam = numVotesForMapBlueTeam[i];
					mapIdWithHighestVoteCountBlueTeam = i;
				}
			}

			// get second place
			for (int i = 1; i <= numChoices; i++) {
				if (numVotesForMapRedTeam[i] > 0 && numVotesForMapRedTeam[i] < highestVoteCountRedTeam)
					secondPlaceMapIdRedTeam = i;
				if (numVotesForMapBlueTeam[i] > 0 && numVotesForMapBlueTeam[i] < highestVoteCountBlueTeam)
					secondPlaceMapIdBlueTeam = i;
			}
			for (int i = 1; i <= numChoices; i++) {
				if (secondPlaceMapIdRedTeam && numVotesForMapRedTeam[i] > 0 && numVotesForMapRedTeam[i] < highestVoteCountRedTeam && numVotesForMapRedTeam[i] > numVotesForMapRedTeam[secondPlaceMapIdRedTeam])
					secondPlaceMapIdRedTeam = i;
				if (secondPlaceMapIdBlueTeam && numVotesForMapBlueTeam[i] > 0 && numVotesForMapBlueTeam[i] < highestVoteCountBlueTeam && numVotesForMapBlueTeam[i] > numVotesForMapBlueTeam[secondPlaceMapIdBlueTeam])
					secondPlaceMapIdBlueTeam = i;
			}

			/*for (int i = 1; i <= numChoices; i++) {
				if (numVotesForMapRedTeam[i] > 0 && numVotesForMapRedTeam[i] < highestVoteCountRedTeam)
					secondPlaceMapIdRedTeam = i;
				if (numVotesForMapBlueTeam[i] > 0 && numVotesForMapBlueTeam[i] < highestVoteCountBlueTeam)
					secondPlaceMapIdBlueTeam = i;
			}*/

			for (int i = 1; i <= numChoices; i++) {
				if (mapBitsVotedByRedTeam & (1 << i))
					++numMapsVotedByRedTeam;
				if (mapBitsVotedByBlueTeam & (1 << i))
					++numMapsVotedByBlueTeam;
			}

			// if map A has exactly 5 votes and one team accounts for 4 of them and the other team accounts for 1 of them,
			// ordinarily we would terminate the vote here because a map has the majority.
			// however, in this particular case we don't want to terminate the vote yet because the other team might
			// eventually gather 3 votes for map B, in which case their lone A voter troll would be changed to vote for B
			if ((highestVoteCountRedTeam == 4 && numVotesForMapBlueTeam[mapIdWithHighestVoteCountRedTeam] == 1) ||
				(highestVoteCountBlueTeam == 4 && numVotesForMapRedTeam[mapIdWithHighestVoteCountBlueTeam] == 1)) {
				if (dontEndDueToMajority)
					*dontEndDueToMajority = qtrue;
			}
		}

#define ChangeRedVotes(n) ChangeVote(TEAM_RED, mapIdWithHighestVoteCountRedTeam, n);
#define ChangeBlueVotes(n) ChangeVote(TEAM_BLUE, mapIdWithHighestVoteCountBlueTeam, n);

		if (!mapHas6OrMoreVotes && numRedPlayers == 4 && numBluePlayers == 4 && numRedVotes + numBlueVotes >= 7) {
			
			if (numRedVotes == 4 && numMapsVotedByRedTeam == 1 && numBlueVotes == 3 && numMapsVotedByBlueTeam == 1) {
				ChangeBlueVotes(0);
			}
			else if (numBlueVotes == 4 && numMapsVotedByBlueTeam == 1 && numRedVotes == 3 && numMapsVotedByRedTeam == 1) {
				ChangeRedVotes(1);
			}
			else if (numRedVotes == 4 && numMapsVotedByRedTeam == 1 && numBlueVotes == 4 && numMapsVotedByBlueTeam == 2 && highestVoteCountBlueTeam == 3/* && secondPlaceMapIdBlueTeam == mapIdWithHighestVoteCountRedTeam*/) {
				ChangeBlueVotes(2);
			}
			else if (numBlueVotes == 4 && numMapsVotedByBlueTeam == 1 && numRedVotes == 4 && numMapsVotedByRedTeam == 2 && highestVoteCountRedTeam == 3/* && secondPlaceMapIdRedTeam == mapIdWithHighestVoteCountBlueTeam*/) {
				ChangeRedVotes(3);
			}
			else if (numRedVotes == 4 && numMapsVotedByRedTeam == 1 && numBlueVotes == 3 && numMapsVotedByBlueTeam == 2) {
				ChangeBlueVotes(4);
			}
			else if (numBlueVotes == 4 && numMapsVotedByBlueTeam == 1 && numRedVotes == 3 && numMapsVotedByRedTeam == 2) {
				ChangeRedVotes(5);
			}
			else if (numRedVotes == 3 && numMapsVotedByRedTeam == 1 && numBlueVotes == 4 && numMapsVotedByBlueTeam == 2 && highestVoteCountBlueTeam == 3 && secondPlaceMapIdBlueTeam == mapIdWithHighestVoteCountRedTeam) {
				ChangeRedVotes(6);
				ChangeBlueVotes(7);
			}
			else if (numBlueVotes == 3 && numMapsVotedByBlueTeam == 1 && numRedVotes == 4 && numMapsVotedByRedTeam == 2 && highestVoteCountRedTeam == 3 && secondPlaceMapIdRedTeam == mapIdWithHighestVoteCountBlueTeam) {
				ChangeRedVotes(8);
				ChangeBlueVotes(9);
			}
		}
		else if (!mapHas6OrMoreVotes && numRedPlayers == 4 && numBluePlayers == 4 && numRedVotes + numBlueVotes == 6 && canChangeAfkVotes) {
			if (numRedVotes == 4 && numMapsVotedByRedTeam == 1 && numBlueVotes == 2 && numMapsVotedByBlueTeam == 1) {
				ChangeBlueVotes(10);
			}
			else if (numBlueVotes == 4 && numMapsVotedByBlueTeam == 1 && numRedVotes == 2 && numMapsVotedByRedTeam == 1) {
				ChangeRedVotes(11);
			}
			else if (numRedVotes == 3 && numMapsVotedByRedTeam == 1 && numBlueVotes == 3 && numMapsVotedByBlueTeam == 2 && secondPlaceMapIdBlueTeam == mapIdWithHighestVoteCountRedTeam) {
				ChangeRedVotes(12);
				ChangeBlueVotes(13);
			}
			else if (numBlueVotes == 3 && numMapsVotedByBlueTeam == 1 && numRedVotes == 3 && numMapsVotedByRedTeam == 2 && secondPlaceMapIdRedTeam == mapIdWithHighestVoteCountBlueTeam) {
				ChangeRedVotes(14);
				ChangeBlueVotes(15);
			}
			else if (numRedVotes == 4 && numMapsVotedByRedTeam == 2 && highestVoteCountRedTeam == 3 && numBlueVotes == 2 && numMapsVotedByBlueTeam == 1 && secondPlaceMapIdRedTeam == mapIdWithHighestVoteCountBlueTeam) {
				ChangeRedVotes(32);
				ChangeBlueVotes(33);
			}
			else if (numBlueVotes == 4 && numMapsVotedByBlueTeam == 2 && highestVoteCountBlueTeam == 3 && numRedVotes == 2 && numMapsVotedByRedTeam == 1 && secondPlaceMapIdBlueTeam == mapIdWithHighestVoteCountRedTeam) {
				ChangeRedVotes(34);
				ChangeBlueVotes(35);
			}
		}
		else if (!mapHas5OrMoreVotes && numRedPlayers == 4 && numBluePlayers == 4 && numRedVotes + numBlueVotes == 5 && canChangeAfkVotes) {
			if (numRedVotes == 3 && numMapsVotedByRedTeam == 1 && numBlueVotes == 2 && numMapsVotedByBlueTeam == 1) {
				ChangeRedVotes(16);
				ChangeBlueVotes(17);
			}
			else if (numBlueVotes == 3 && numMapsVotedByBlueTeam == 1 && numRedVotes == 2 && numMapsVotedByRedTeam == 1) {
				ChangeRedVotes(18);
				ChangeBlueVotes(19);
			}
			else if (numRedVotes == 4 && numMapsVotedByRedTeam == 1 && numBlueVotes == 1 && numMapsVotedByBlueTeam == 1) {
				ChangeRedVotes(20);
				ChangeBlueVotes(21);
			}
			else if (numBlueVotes == 4 && numMapsVotedByBlueTeam == 1 && numRedVotes == 1 && numMapsVotedByRedTeam == 1) {
				ChangeRedVotes(22);
				ChangeBlueVotes(23);
			}
		}
		else if (!mapHas4OrMoreVotes && numRedPlayers == 4 && numBluePlayers == 4 && numRedVotes + numBlueVotes == 4 && canChangeAfkVotes) {
			if (numRedVotes == 3 && numMapsVotedByRedTeam == 1 && numBlueVotes == 1) {
				ChangeRedVotes(24);
				ChangeBlueVotes(25);
			}
			else if (numBlueVotes == 3 && numMapsVotedByBlueTeam == 1 && numRedVotes == 1) {
				ChangeRedVotes(26);
				ChangeBlueVotes(27);
			}
		}
		else if (!mapHas3OrMoreVotes && numRedPlayers == 4 && numBluePlayers == 4 && numRedVotes + numBlueVotes == 3 && canChangeAfkVotes) {
			if (numRedVotes == 2 && numMapsVotedByRedTeam == 1 && numBlueVotes == 1) {
				ChangeRedVotes(28);
				ChangeBlueVotes(29);
			}
			else if (numBlueVotes == 2 && numMapsVotedByBlueTeam == 1 && numRedVotes == 1) {
				ChangeRedVotes(30);
				ChangeBlueVotes(31);
			}
		}
	}

	if ( numVotes ) *numVotes = 0;
	if ( highestVoteCount ) *highestVoteCount = 0;

	for ( int i = 0; i < MAX_CLIENTS; ++i ) {
		int voteId = level.multiVotes[i];

		if ( voteId > 0 && voteId <= numChoices ) {
			// one more valid vote...
			if ( numVotes ) ( *numVotes )++;
			voteResults[voteId - 1]++;

			if ( highestVoteCount && voteResults[voteId - 1] > *highestVoteCount ) {
				*highestVoteCount = voteResults[voteId - 1];
			}
		} 
		else if (g_vote_runoffRerollOption.integer && !g_vote_banMap.integer && voteId == -1) {
			if (numVotes) (*numVotes)++;
			if (numRerollVotes) (*numRerollVotes)++;
		}
		else if ( voteId ) { // shouldn't happen since we check that in /vote...
			G_LogPrintf( "Invalid multi vote id for client %d: %d\n", i, voteId );
		}
	}

	return voteResults;
}

static void PickRandomMultiMap( const int *voteResults, int numChoices, const int numVotingClients,
	const int numVotes, const int highestVoteCount, qboolean *hasWildcard, char *out, size_t outSize ) {
	if (level.inRunoff || level.voteBanPhaseCompleted) {
		numChoices = 0;
		for (int i = 0; i < MAX_MULTIVOTE_MAPS; i++)
			if (level.multiVoteMapFileNames[i][0])
				numChoices++;
	}

	if ( highestVoteCount >= ( numVotingClients / 2 ) + 1 ) {
		// one map has a >50% majority, find it and pass it
		for ( int i = 0; i < numChoices; ++i ) {
			if ( voteResults[i] == highestVoteCount ) {
				if (level.inRunoff || level.voteBanPhaseCompleted)
					Q_strncpyz(out, level.multiVoteMapFileNames[i], outSize);
				else
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

		for ( int i = 0; i < numChoices; ++i ) {
			udf[i] = i + 1;
		}

		random = rand() % numChoices;
	} else {
		if (!g_vote_rng.integer) {
			// check if there is a single map with the most votes, in which case we simply choose that one
			int numWithHighestVoteCount = 0, mapWithHighestVoteCount = -1;
			for (int i = 0; i < numChoices; i++) {
				if (voteResults[i] == highestVoteCount) {
					numWithHighestVoteCount++;
					mapWithHighestVoteCount = i;
				}
			}
			if (numWithHighestVoteCount == 1 && mapWithHighestVoteCount != -1) {
				if (level.inRunoff || level.voteBanPhaseCompleted)
					Q_strncpyz(out, level.multiVoteMapFileNames[mapWithHighestVoteCount], outSize);
				else
					trap_Argv(mapWithHighestVoteCount + 1, out, outSize);
				return;
			}
		}

		// make it an array where each item appears as many times as they were voted, thus giving weight (0 votes = 0%)
		int items = numVotes, currentItem = 0;
		udf = malloc( sizeof( *udf ) * items );
		for ( int i = 0; i < numChoices; ++i ) {
			if (g_vote_rng.integer) {
				// 1. special case for wildcard vote: if it has the highest amount of votes, it either passed with >50% majority earlier,
				// or if we got here it could be tied with other votes for the highest amount of votes: for this case, rule out all
				// votes that aren't tied with it
				// 2. otherwise, if the wildcard vote doesn't have the highest amount of votes, discard its votes
				// 3. rule out very low vote counts relatively to the highest one and the max voting clients
				if (
					(*hasWildcard && voteResults[numChoices - 1] == highestVoteCount && voteResults[i] != highestVoteCount) ||
					(*hasWildcard && voteResults[numChoices - 1] != highestVoteCount && i == numChoices - 1) ||
					(highestVoteCount - voteResults[i] > (numVotingClients / 4))) {
					items -= voteResults[i];
					udf = realloc(udf, sizeof(*udf) * items);
					continue;
				}
			}
			else {
				// rng is disabled
				// simply rule out vote counts that are not tied for most votes
				if (voteResults[i] < highestVoteCount) {
					items -= voteResults[i];
					udf = realloc(udf, sizeof(*udf) * items);
					continue;
				}
			}

			for (int j = 0; j < voteResults[i]; ++j) {
				udf[currentItem++] = i + 1;
			}
		}

		if (g_vote_underdogTeamMapVoteTiebreakerThreshold.string[0] && (double)(g_vote_underdogTeamMapVoteTiebreakerThreshold.value) > 0.5) {
			int underdogTeam = 0;

			int missingPositions = 8, numRed = 0, numBlue = 0;
			int base[4], chase[4], offense1[4], offense2[4];
			memset(base, -1, sizeof(base));
			memset(chase, -1, sizeof(chase));
			memset(offense1, -1, sizeof(offense1));
			memset(offense2, -1, sizeof(offense2));
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *ent = &g_entities[i];
				if (!ent->inuse || !ent->client || !ent->client->account || ent->client->pers.connected != CON_CONNECTED)
					continue;
				if (IsRacerOrSpectator(ent))
					continue;

				if (ent->client->sess.sessionTeam == TEAM_RED)
					++numRed;
				else if (ent->client->sess.sessionTeam == TEAM_BLUE)
					++numBlue;
				else
					continue;

				const int team = ent->client->sess.sessionTeam;
				const int pos = GetRemindedPosOrDeterminedPos(ent);
				if (pos == CTFPOSITION_BASE && base[team] == -1) {
					base[team] = ent->client->account->id;
					--missingPositions;
				}
				else if (pos == CTFPOSITION_CHASE && chase[team] == -1) {
					chase[team] = ent->client->account->id;
					--missingPositions;
				}
				else if (pos == CTFPOSITION_OFFENSE) {
					if (offense1[team] == -1) {
						offense1[team] = ent->client->account->id;
						--missingPositions;
					}
					else if (offense2[team] == -1) {
						offense2[team] = ent->client->account->id;
						--missingPositions;
					}
				}
			}

			if (!missingPositions && numRed == 4 && numBlue == 4) {
				G_DBGetPlayerRatings();
				double teamTotal[4] = { 0 };
				for (int t = TEAM_RED; t <= TEAM_BLUE; t++) {
					teamTotal[t] += PlayerTierToRating(GetPlayerTierForPlayerOnPosition(base[t], CTFPOSITION_BASE, qtrue));
					teamTotal[t] += PlayerTierToRating(GetPlayerTierForPlayerOnPosition(chase[t], CTFPOSITION_CHASE, qtrue));
					teamTotal[t] += PlayerTierToRating(GetPlayerTierForPlayerOnPosition(offense1[t], CTFPOSITION_OFFENSE, qtrue));
					teamTotal[t] += PlayerTierToRating(GetPlayerTierForPlayerOnPosition(offense2[t], CTFPOSITION_OFFENSE, qtrue));
				}

				double totalOfBothTeams = teamTotal[TEAM_RED] + teamTotal[TEAM_BLUE];
				if (totalOfBothTeams) { // prevent divide by zero
					double relativeStrength[4];
					relativeStrength[TEAM_RED] = teamTotal[TEAM_RED] / totalOfBothTeams;
					relativeStrength[TEAM_BLUE] = teamTotal[TEAM_BLUE] / totalOfBothTeams;

					if (relativeStrength[TEAM_BLUE] >= (double)(g_vote_underdogTeamMapVoteTiebreakerThreshold.value) - 0.0001) {
						Com_Printf("Giving red team tiebreaker advantage because %g < %g\n", relativeStrength[TEAM_RED], relativeStrength[TEAM_BLUE]);
						underdogTeam = TEAM_RED;
					}
					else if (relativeStrength[TEAM_RED] >= (double)(g_vote_underdogTeamMapVoteTiebreakerThreshold.value) - 0.0001) {
						Com_Printf("Giving blue team tiebreaker advantage because %g < %g\n", relativeStrength[TEAM_BLUE], relativeStrength[TEAM_RED]);
						underdogTeam = TEAM_BLUE;
					}
				}
			}

			if (underdogTeam) {
				Com_Printf("Underdog team detected: %d\n", underdogTeam);

				// Determine the highest number of votes from the underdog team for maps not eliminated
				int highestUnderdogVotes = 0;
				int underdogVotes[MAX_MULTIVOTE_MAPS] = { 0 };

				// Count the votes for each map from the underdog team
				for (int j = 0; j < MAX_CLIENTS; ++j) {
					gentity_t *ent = &g_entities[j];
					if (ent->inuse && ent->client && ent->client->sess.sessionTeam == underdogTeam) {
						int voteId = level.multiVotes[j];
						if (voteId > 0 && voteId <= numChoices) {
							underdogVotes[voteId - 1]++;
							Com_Printf("Underdog client %d (team %d) voted for map %d\n", j, underdogTeam, voteId);
						}
					}
				}

				// Determine the highest number of votes from the underdog team
				for (int i = 0; i < numChoices; ++i) {
					int mapIndex = udf[i] - 1;
					if (underdogVotes[mapIndex] > highestUnderdogVotes) {
						highestUnderdogVotes = underdogVotes[mapIndex];
					}
				}
				Com_Printf("Highest votes from underdog team: %d\n", highestUnderdogVotes);

				// Eliminate maps that don't have the highest number of votes from the underdog team
				if (highestUnderdogVotes > 0) {
					int newItems = 0;
					for (int i = 0; i < items; ++i) {
						int mapIndex = udf[i] - 1;
						if (underdogVotes[mapIndex] == highestUnderdogVotes) {
							udf[newItems++] = udf[i];
						}
						else {
							Com_Printf("Vote for map %d eliminated; votes for it from underdog team: %d\n", udf[i], underdogVotes[mapIndex]);
						}
					}

					if (newItems > 0) {
						Com_DebugPrintf("Reallocating udf array to size %d\n", newItems);
						items = newItems;
						udf = realloc(udf, sizeof(*udf) * items);
					}
				}
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

	if (level.inRunoff || level.voteBanPhaseCompleted)
		Q_strncpyz(out, level.multiVoteMapFileNames[result - 1], outSize);
	else
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
	int *voteResults = BuildVoteResults( level.multiVoteChoices, &numVotes, &highestVoteCount, NULL, NULL, qtrue/*i guess?*/);

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

		int numVotesForThisMap = voteResults[i];
		int numRedVotesForThisMap = 0, numBlueVotesForThisMap = 0;
		for (int j = 0; j < MAX_MULTIVOTE_MAPS; j++) {
			if (Q_stricmp(level.multiVoteMapFileNames[j], mapname))
				continue;

			if (level.inRunoff || level.voteBanPhaseCompleted) // not sure why this condition is needed but i'm not deleting it at this point because i must have had a reason to write it before :madman_tomato: :spaghetti:
				numVotesForThisMap = voteResults[j];
				
			// see how many of each team voted for this map
			for (int k = 0; k < MAX_CLIENTS; k++) {
				if (level.multiVotes[k] != j + 1)
					continue;

				gentity_t *thisVoter = &g_entities[k];

				if (thisVoter->inuse && thisVoter->client && thisVoter->client->pers.connected == CON_CONNECTED && !IsRacerOrSpectator(thisVoter) && thisVoter->client->sess.sessionTeam == TEAM_RED)
					++numRedVotesForThisMap;
				else if (thisVoter->inuse && thisVoter->client && thisVoter->client->pers.connected == CON_CONNECTED && !IsRacerOrSpectator(thisVoter) && thisVoter->client->sess.sessionTeam == TEAM_BLUE)
					++numBlueVotesForThisMap;
				else if (thisVoter->inuse && thisVoter->client && thisVoter->client->pers.connected == CON_CONNECTED && IsRacerOrSpectator(thisVoter) && thisVoter->client->stats && thisVoter->client->stats->lastTeam == TEAM_RED)
					++numRedVotesForThisMap; // try to acccount for idiots who go spec (does their vote even count though?)
				else if (thisVoter->inuse && thisVoter->client && thisVoter->client->pers.connected == CON_CONNECTED && IsRacerOrSpectator(thisVoter) && thisVoter->client->stats && thisVoter->client->stats->lastTeam == TEAM_BLUE)
					++numBlueVotesForThisMap; // try to acccount for idiots who go spec (does their vote even count though?)
				// else case even possible?
			}
		}

		const char *colorStr = !Q_stricmp(mapname, selectedMapname) ? "^2" : "^7"; // the selected map is green
		const char *pluralStr = numVotesForThisMap != 1 ? "s" : "";
		if (numVotesForThisMap) {
			if (numRedVotesForThisMap && numBlueVotesForThisMap && numRedVotesForThisMap + numBlueVotesForThisMap == numVotesForThisMap) {
				Q_strcat(resultString, sizeof(resultString), va("\n%s%s - ^1%d^7+^4%d%s vote%s",
					colorStr,
					mapDisplayName,
					numRedVotesForThisMap,
					numBlueVotesForThisMap,
					colorStr,
					pluralStr)
				);
			}
			else if (numRedVotesForThisMap && numRedVotesForThisMap == numVotesForThisMap) {
				Q_strcat(resultString, sizeof(resultString), va("\n%s%s - ^1%d%s vote%s",
					colorStr,
					mapDisplayName,
					numVotesForThisMap,
					colorStr,
					pluralStr)
				);
			}
			else if (numBlueVotesForThisMap && numBlueVotesForThisMap == numVotesForThisMap) {
				Q_strcat(resultString, sizeof(resultString), va("\n%s%s - ^4%d%s vote%s",
					colorStr,
					mapDisplayName,
					numVotesForThisMap,
					colorStr,
					pluralStr)
				);
			}
			else { // ???
				Q_strcat(resultString, sizeof(resultString), va("\n%s%s - %d vote%s",
					colorStr,
					mapDisplayName,
					numVotesForThisMap,
					pluralStr)
				);
			}
		}
		else {
			Q_strcat(resultString, sizeof(resultString), va("\n%s%s - %d vote%s",
				colorStr,
				mapDisplayName,
				numVotesForThisMap,
				pluralStr)
			);
		}
	}

	free( voteResults );

	// do both a console print and global prioritized center print
	if ( VALIDSTRING( resultString ) ) {
		trap_SendServerCommand( -1, va( "print \"%s\n\"", resultString ) );
		G_GlobalTickedCenterPrint( resultString, 3000, qtrue ); // show it for 3s, the time before map changes
	}

	// re-use the vote timer so that there's a small delay before map change
	char overrideMapName[MAX_QPATH] = { 0 };
	G_DBGetLiveMapNameForMapName(selectedMapname, overrideMapName, sizeof(overrideMapName));
	if (overrideMapName[0] && Q_stricmp(overrideMapName, selectedMapname)) {
		Com_Printf("Overriding %s via map alias to %s\n", selectedMapname, overrideMapName);
#ifdef _DEBUG
		Q_strncpyz(level.voteString, va("devmap %s", overrideMapName), sizeof(level.voteString));
#else
		Q_strncpyz(level.voteString, va("map %s", overrideMapName), sizeof(level.voteString));
#endif
		trap_Cvar_Set("g_lastMapVotedMap", overrideMapName);
	}
	else {
#ifdef _DEBUG
		Q_strncpyz(level.voteString, va("devmap %s", selectedMapname), sizeof(level.voteString));
#else
		Q_strncpyz(level.voteString, va("map %s", selectedMapname), sizeof(level.voteString));
#endif
		trap_Cvar_Set("g_lastMapVotedMap", selectedMapname);
	}
	char timeBuf[MAX_STRING_CHARS] = { 0 };
	Com_sprintf(timeBuf, sizeof(timeBuf), "%d", (int)time(NULL));
	trap_Cvar_Set("g_lastMapVotedTime", timeBuf);
	level.voteExecuteTime = level.time + 3000;
	level.multiVoting = qfalse;
	level.voteBanPhase = qfalse;
	level.multiVoteHasWildcard = qfalse;
	level.multiVoteChoices = 0;
	level.multivoteWildcardMapFileName[0] = '\0';
	level.mapsThatCanBeVotedBits = 0;
	level.multiVoteTimeExtensions = 0;
	level.runoffSurvivors = level.runoffLosers = level.successfulRerollVoters = 0llu;
	memset(level.multiVotes, 0, sizeof(level.multiVotes));
	memset(&(level.multiVoteBanVotes), 0, sizeof(level.multiVoteBanVotes));
	memset(level.bannedMapNames, 0, sizeof(level.bannedMapNames));
	memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
	memset(&level.multiVoteMapShortNames, 0, sizeof(level.multiVoteMapShortNames));
	memset(&level.multiVoteMapFileNames, 0, sizeof(level.multiVoteMapFileNames));
	level.inRunoff = qfalse;
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

			if (g_unlagged.integer && level.clients[i].sess.unlagged) {
				Q_strcat(description, sizeof(description), " (unlagged)");
			}

			if (sv_passwordlessSpectators.integer && !level.clients[i].sess.canJoin) {
				Q_strcat(description, sizeof(description), " (^3no password^7)");
			}
			
			if ( ( g_antiWallhack.integer == 1 && !level.clients[i].sess.whTrustToggle )
				|| ( g_antiWallhack.integer >= 2 && level.clients[i].sess.whTrustToggle ) ) {
				// anti wallhack is enabled for this player
				Q_strcat( description, sizeof( description ), " (^3anti WH enabled^7)" );
			}

			if ( level.clients[i].sess.shadowMuted ) {
				Q_strcat( description, sizeof( description ), " (^3shadow muted^7)" );
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
				*filter = '�';
			else if (*filter == 'A')
				*filter = '�';
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
				*filter = '�';
			else if (*filter == 'A')
				*filter = '�';
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
	if (trap_Argc() > 1) { // print a list of the maps in a single pool
		char short_name[64] = { 0 };
		trap_Argv(1, short_name, sizeof(short_name));
		if (!short_name[0]) {
			Com_Printf("Please specify a pool short name.\n");
			return;
		}

		list_t mapList = { 0 };
		void *ctxPtr = &mapList;
		char poolLongName[64] = { 0 };
		G_DBListMapsInPool(short_name, "", listMapsInPools, (void **)&ctxPtr, &poolLongName[0], sizeof(poolLongName), 0);

		iterator_t iter;
		ListIterate(&mapList, &iter, qfalse);
		Table *t = Table_Initialize(qtrue);
		int numMaps = 0;
		while (IteratorHasNext(&iter)) {
			poolMap_t *map = (poolMap_t *)IteratorNext(&iter);
			Table_DefineRow(t, map);
			numMaps++;
		}

		if (numMaps) {
			Table_DefineColumn(t, "Map", TableCallback_MapName, NULL, qtrue, -1, 64);
			Table_DefineColumn(t, "Weight", TableCallback_MapWeight, NULL, qtrue, -1, 64);

			char buf[2048] = { 0 };
			Table_WriteToBuffer(t, buf, sizeof(buf), qtrue, -1);
			Q_strcat(buf, sizeof(buf), "\n");
			Com_Printf(buf);
		}
		Table_Destroy(t);
		ListClear(&mapList);

		Com_Printf("Found %i maps for pool with long name ^5%s^7 and short name ^7%s^7.\n", numMaps, poolLongName, short_name);
		Com_Printf("To see a list of all pools, use ^5pools^7 without any arguments.\n");
	}
	else { // print a list of pools
		list_t poolList = { 0 };

		G_DBListPools(listPools, &poolList);

		iterator_t iter;
		ListIterate(&poolList, &iter, qfalse);
		Table *t = Table_Initialize(qtrue);
		int numPools = 0;
		while (IteratorHasNext(&iter)) {
			pool_t *pool = (pool_t *)IteratorNext(&iter);
			Table_DefineRow(t, pool);
			numPools++;
		}

		if (numPools) {
			Table_DefineColumn(t, "Short Name", TableCallback_PoolShortName, NULL, qtrue, -1, 64);
			Table_DefineColumn(t, "Long Name", TableCallback_PoolLongName, NULL, qtrue, -1, 64);

			char buf[2048] = { 0 };
			Table_WriteToBuffer(t, buf, sizeof(buf), qtrue, -1);
			Q_strcat(buf, sizeof(buf), "\n");
			Com_Printf(buf);
		}
		Table_Destroy(t);
		ListClear(&poolList);

		Com_Printf("Found %i map pools.\n", numPools);
		Com_Printf("To see a list of maps in a specific pool, use ^5pools <pool short name>^7\n");
	}
}

typedef struct {
	char format[MAX_STRING_CHARS];
} SessionPrintCtx;

static void FormatAccountSessionList( void *ctx, const sessionReference_t sessionRef, const qboolean referenced ) {
	SessionPrintCtx* out = ( SessionPrintCtx* )ctx;

#ifdef NEWMOD_SUPPORT
	qboolean isNewmodSession = G_SessionInfoHasString( sessionRef.ptr, "cuid_hash2" );
#endif

	char sessFlags[16] = { 0 };
	if ( sessionRef.online ) Q_strcat( sessFlags, sizeof( sessFlags ), S_COLOR_GREEN"|" );
	if ( !referenced ) Q_strcat( sessFlags, sizeof( sessFlags ), S_COLOR_YELLOW"|" );
#ifdef NEWMOD_SUPPORT
	if ( isNewmodSession ) Q_strcat( sessFlags, sizeof( sessFlags ), S_COLOR_CYAN"|" );
#endif

	Q_strcat( out->format, sizeof( out->format ), va( S_COLOR_WHITE"Session ID %d ", sessionRef.ptr->id ) );
	if ( VALIDSTRING( sessFlags ) ) Q_strcat( out->format, sizeof( out->format ), va( "%s ", sessFlags ) );
	Q_strcat( out->format, sizeof( out->format ), va( S_COLOR_WHITE"(hash: %llx)\n", sessionRef.ptr->hash ) );
}

static int64_t AccountFlagName2Bitflag(const char* flagName) {
	if (!Q_stricmp(flagName, "Admin")) {
		return ACCOUNTFLAG_ADMIN;
	} else if (!Q_stricmp(flagName, "VerboseRcon")) {
		return ACCOUNTFLAG_RCONLOG;
	} else if (!Q_stricmp(flagName, "EnterSpammer")) {
		return ACCOUNTFLAG_ENTERSPAMMER;
	} else if (!Q_stricmp(flagName, "AimPackEditor")) {
		return ACCOUNTFLAG_AIMPACKEDITOR;
	} else if (!Q_stricmp(flagName, "AimPackAdmin")) {
		return ACCOUNTFLAG_AIMPACKADMIN;
	} else if (!Q_stricmp(flagName, "VoteTroll")) {
		return ACCOUNTFLAG_VOTETROLL;
	} else if (!Q_stricmp(flagName, "RatePlayers")) {
		return ACCOUNTFLAG_RATEPLAYERS;
	} else if (!Q_stricmp(flagName, "InstapauseBlacklist")) {
		return ACCOUNTFLAG_INSTAPAUSE_BLACKLIST;
	} else if (!Q_stricmp(flagName, "PermaBarred")) {
		return ACCOUNTFLAG_PERMABARRED;
	} else if (!Q_stricmp(flagName, "HardPermaBarred")) {
		return ACCOUNTFLAG_HARDPERMABARRED;
	} else if (!Q_stricmp(flagName, "NoCount")) {
		return ACCOUNTFLAG_RATEPLAYERS_NOCOUNT;
	} else if (!Q_stricmp(flagName, "AfkTroll")) {
		return ACCOUNTFLAG_AFKTROLL;
	} else if (!Q_stricmp(flagName, "EloBotSelfHost")) {
		return ACCOUNTFLAG_ELOBOTSELFHOST;
	} else if (!Q_stricmp(flagName, "GetTroll")) {
		return ACCOUNTFLAG_GETTROLL;
	} else if (!Q_stricmp(flagName, "RemindPosIncessantly")) {
		return ACCOUNTFLAG_REMINDPOSINCESSANTLY;
	} else if (!Q_stricmp(flagName, "VerificationLord")) {
		return ACCOUNTFLAG_VERIFICATIONLORD;
	} else if (!Q_stricmp(flagName, "SpawnFCBoost")) {
		return ACCOUNTFLAG_BOOST_SPAWNFCBOOST;
	} else if (!Q_stricmp(flagName, "SpawnGerBoost")) {
		return ACCOUNTFLAG_BOOST_SPAWNGERBOOST;
	} else if (!Q_stricmp(flagName, "SpawnClickBoost")) {
		return ACCOUNTFLAG_BOOST_SPAWNCLICKBOOST;
	} else if (!Q_stricmp(flagName, "BaseAutoTHTEBoost")) {
		return ACCOUNTFLAG_BOOST_BASEAUTOTHTEBOOST;
	} else if (!Q_stricmp(flagName, "SelfkillBoost")) {
		return ACCOUNTFLAG_BOOST_SELFKILLBOOST;
	} else if (!Q_stricmp(flagName, "TriggerBoost")) {
		return ACCOUNTFLAG_BOOST_TRIGGERBOOST;
	} else if (!Q_stricmp(flagName, "ItemPickupBoost")) {
		return ACCOUNTFLAG_BOOST_ITEMPICKUPBOOST;
	} else if (!Q_stricmp(flagName, "AimbotBoost")) {
		return ACCOUNTFLAG_BOOST_AIMBOTBOOST;
	} else if (!Q_stricmp(flagName, "HunGaslight")) {
		return ACCOUNTFLAG_HUN_GASLIGHT;
	} else if (!Q_stricmp(flagName, "LSAfkTroll")) {
		return ACCOUNTFLAG_LSAFKTROLL;
	} else if (!Q_stricmp(flagName, "FakeFCOverlay")) {
		return ACCOUNTFLAG_FAKEFCOVERLAY;
	} else if (!Q_stricmp(flagName, "SmodTroll")) {
		return ACCOUNTFLAG_SMODTROLL;
	} else if (!Q_stricmp(flagName, "OffenseAutoTHTEBoost")) {
		return ACCOUNTFLAG_BOOST_OFFENSEAUTOTHTEBOOST;
	} else if (!Q_stricmp(flagName, "ChaseAutoTHTEBoost")) {
		return ACCOUNTFLAG_BOOST_CHASEAUTOTHTEBOOST;
	} else if (!Q_stricmp(flagName, "SucksMassiveCockAtSaberingBoost")) {
		return ACCOUNTFLAG_BOOST_SUCKSMASSIVECOCKATSABERING;
	} else if (!Q_stricmp(flagName, "FixIdioticForceConfig")) {
		return ACCOUNTFLAG_BOOST_FIXIDIOTICFORCECONFIG;
	} else if (!Q_stricmp(flagName, "AutoSwitcher")) {
		return ACCOUNTFLAG_AUTOSWITCHER;
	} else if (!Q_stricmp(flagName, "FixInstaYawBoost")) {
		return ACCOUNTFLAG_BOOST_FIXINSTAYAW;
	} else if (!Q_stricmp(flagName, "ItemLord")) {
		return ACCOUNTFLAG_ITEMLORD;
	} else if (!Q_stricmp(flagName, "InstaVoteTroll")) {
		return ACCOUNTFLAG_INSTAVOTETROLL;
	} else if (!Q_stricmp(flagName, "NonVotingMemer")) {
		return ACCOUNTFLAG_NONVOTINGMEMER;
	} else if (!Q_stricmp(flagName, "ConcLord")) {
		return ACCOUNTFLAG_CONCLORD;
	} else if (!Q_stricmp(flagName, "Conc2")) {
		return ACCOUNTFLAG_CONC2;
	}

	return 0;
}

#define NUM_SESSIONS_PER_WHOIS_PAGE 5

const char* AccountBitflag2FlagName(int64_t bitflag) {
	switch (bitflag) {
		case ACCOUNTFLAG_ADMIN: return "Admin";
		case ACCOUNTFLAG_RCONLOG: return "VerboseRcon";
		case ACCOUNTFLAG_ENTERSPAMMER: return "EnterSpammer";
		case ACCOUNTFLAG_AIMPACKEDITOR: return "AimPackEditor";
		case ACCOUNTFLAG_AIMPACKADMIN: return "AimPackAdmin";
		case ACCOUNTFLAG_VOTETROLL: return "VoteTroll";
		case ACCOUNTFLAG_INSTAPAUSE_BLACKLIST: return "InstapauseBlacklist";
		case ACCOUNTFLAG_PERMABARRED: return "PermaBarred";
		case ACCOUNTFLAG_HARDPERMABARRED: return "HardPermaBarred";
		case ACCOUNTFLAG_AFKTROLL: return "AfkTroll";
		case ACCOUNTFLAG_ELOBOTSELFHOST: return "EloBotSelfHost";
		case ACCOUNTFLAG_GETTROLL: return "GetTroll";
		case ACCOUNTFLAG_REMINDPOSINCESSANTLY: return "RemindPosIncessantly";
		case ACCOUNTFLAG_VERIFICATIONLORD: return "VerificationLord";
		case ACCOUNTFLAG_BOOST_SPAWNFCBOOST: return "SpawnFCBoost";
		case ACCOUNTFLAG_BOOST_SPAWNGERBOOST: return "SpawnGerBoost";
		case ACCOUNTFLAG_BOOST_SPAWNCLICKBOOST: return "SpawnClickBoost";
		case ACCOUNTFLAG_BOOST_BASEAUTOTHTEBOOST: return "BaseAutoTHTEBoost";
		case ACCOUNTFLAG_BOOST_SELFKILLBOOST: return "SelfkillBoost";
		case ACCOUNTFLAG_BOOST_TRIGGERBOOST: return "TriggerBoost";
		case ACCOUNTFLAG_BOOST_ITEMPICKUPBOOST: return "ItemPickupBoost";
		case ACCOUNTFLAG_BOOST_AIMBOTBOOST: return "AimbotBoost";
		case ACCOUNTFLAG_LSAFKTROLL: return "LSAfkTroll";
		case ACCOUNTFLAG_FAKEFCOVERLAY: return "FakeFCOverlay";
		case ACCOUNTFLAG_SMODTROLL: return "SmodTroll";
		case ACCOUNTFLAG_BOOST_OFFENSEAUTOTHTEBOOST: return "OffenseAutoTHTEBoost";
		case ACCOUNTFLAG_BOOST_CHASEAUTOTHTEBOOST: return "ChaseAutoTHTEBoost";
		case ACCOUNTFLAG_BOOST_SUCKSMASSIVECOCKATSABERING: return "SucksMassiveCockAtSaberingBoost";
		case ACCOUNTFLAG_BOOST_FIXIDIOTICFORCECONFIG: return "FixIdioticForceConfig";
		case ACCOUNTFLAG_AUTOSWITCHER: return "AutoSwitcher";
		case ACCOUNTFLAG_BOOST_FIXINSTAYAW: return "FixInstaYawBoost";
		case ACCOUNTFLAG_ITEMLORD: return "ItemLord";
		case ACCOUNTFLAG_INSTAVOTETROLL: return "InstaVoteTroll";
		case ACCOUNTFLAG_NONVOTINGMEMER: return "NonVotingMemer";
		case ACCOUNTFLAG_CONCLORD: return "ConcLord";
		case ACCOUNTFLAG_CONC2: return "Conc2";
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
				G_Printf( "Usage:^3 account create <username>^7\n" );
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
				G_Printf( "Usage:^3 account delete <username>^7\n" );
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
				if (G_DBGetMetadataInteger("shouldReloadPlayerPugStats") != 2)
					G_DBSetMetadata("shouldReloadPlayerPugStats", "1");
			} else {
				G_Printf( "Failed to delete account!\n" );
			}

		} else if ( !Q_stricmp( s, "info" ) ) {

			if ( trap_Argc() < 3 ) {
				G_Printf( "Usage:^3 account info <username> [page]^7\n" );
				return;
			}

			int page = 1;

			if (trap_Argc() > 3) {
				char buf[8];
				trap_Argv(3, buf, sizeof(buf));
				page = Com_Clampi(1, 999, atoi(buf));
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv( 2, username, sizeof( username ) );

			accountReference_t acc = G_GetAccountByName( username, qfalse );

			if ( !acc.ptr ) {
				G_Printf( "Account '%s' does not exist\n", username );
				return;
			}

			SessionPrintCtx sessionsPrint = { 0 };
			G_ListSessionsForAccount( acc.ptr, NUM_SESSIONS_PER_WHOIS_PAGE, page, FormatAccountSessionList, &sessionsPrint );

			int playtime = G_DBGetAccountPlaytime( acc.ptr );

			char timestamp[32];
			G_FormatLocalDateFromEpoch( timestamp, sizeof( timestamp ), acc.ptr->creationDate );

			char flagsStr[MAX_STRING_CHARS] = { 0 };
			for ( int64_t i = 0; i < sizeof( i ) * 8; i++ ) {
				int64_t bit = (1ll << i);

				if ( !( acc.ptr->flags & bit ) )
					continue;
				const char* flagName = AccountBitflag2FlagName(bit);

				if ( !VALIDSTRING( flagName ) )
					continue;

				if ( VALIDSTRING( flagsStr ) )
					Q_strcat( flagsStr, sizeof( flagsStr ), ", " );

				Q_strcat( flagsStr, sizeof( flagsStr ), flagName);
			}

			G_Printf(
				"^3Account Name:^7 %s\n"
				"^3Account ID:^7 %d\n"
				"^3Created on:^7 %s\n"
				"%s"
				"^3Flags:^7 %s\n"
				"\n",
				acc.ptr->name,
				acc.ptr->id,
				timestamp,
				(acc.ptr->autoLink.sex[0] || acc.ptr->autoLink.guid[0]) ? va("^3Autolink:^7 sex %s guid %s country %s^7\n", acc.ptr->autoLink.sex[0] ? acc.ptr->autoLink.sex : "none", acc.ptr->autoLink.guid[0] ? acc.ptr->autoLink.guid : "none", acc.ptr->autoLink.country[0] ? acc.ptr->autoLink.country : "none") : "",
				flagsStr
			);

			if (playtime > 0) {
				char durationString[64];
				G_FormatDuration(playtime, durationString, sizeof(durationString));

				G_Printf(S_COLOR_ORANGE"Playtime: "S_COLOR_WHITE"%s\n\n", durationString);
			}

			if ( VALIDSTRING( sessionsPrint.format ) ) {

#ifdef NEWMOD_SUPPORT
				G_Printf( "Sessions tied to this account: ( "S_COLOR_GREEN"| = online "S_COLOR_YELLOW"| = unreferenced "S_COLOR_CYAN"| = newmod "S_COLOR_WHITE")\n" );
#else
				G_Printf( "Sessions tied to this account: ( "S_COLOR_GREEN"| - online "S_COLOR_YELLOW"| - unreferenced "S_COLOR_WHITE")\n" );
#endif
				G_Printf( "%s", sessionsPrint.format );

			}

			G_Printf("Viewing page: %d\n", page);

		} else if ( !Q_stricmp( s, "toggleflag" ) ) {

			if ( trap_Argc() < 4 ) {
				G_Printf( "Usage:^3 account toggleflag <username> <flag>^7\n" );
				G_Printf( "Available flags: Admin, VerboseRcon, AimPackEditor, AimPackAdmin, VoteTroll, InstapauseBlacklist, PermaBarred, HardPermaBarred, AfkTroll, EloBotSelfHost, GetTroll, RemindPosIncessantly, VerificationLord, SpawnFCBoost, SpawnGerBoost, SpawnClickBoost, BaseAutoTHTEBoost, ChaseAutoTHTEBoost, OffenseAutoTHTEBoost, SelfkillBoost, THTESwitchBoost, TriggerBoost, ItemPickupBoost, AimbotBoost, LSAfkTroll, AutoSwitcher\n" );
				return;
			}

			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv( 2, username, sizeof( username ) );

			accountReference_t acc = G_GetAccountByName( username, qfalse );

			if ( !acc.ptr ) {
				G_Printf( "Account '%s' does not exist\n", username );
				return;
			}

			char flagName[128] = { 0 };
			trap_Argv( 3, flagName, sizeof( flagName ) );

			int64_t flag = AccountFlagName2Bitflag( flagName );

			if ( !flag ) {
				G_Printf( "Unknown account flag name '%s'\n", flagName );
				return;
			}

			if ( !( acc.ptr->flags & flag ) ) {
				// we are enabling the flag
				if ( G_SetAccountFlags( acc.ptr, flag, qtrue ) ) {
					G_Printf( "Flag '%s' ^2enabled^7 for account '%s' (id: %d)\n", flagName, acc.ptr->name, acc.ptr->id );
					if (flag == ACCOUNTFLAG_PERMABARRED || flag == ACCOUNTFLAG_HARDPERMABARRED || flag == ACCOUNTFLAG_LSAFKTROLL) { // refresh userinfo of anyone connected on this account
						for (int i = 0; i < MAX_CLIENTS; i++) {
							if (g_entities[i].inuse && g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED && g_entities[i].client->account && g_entities[i].client->account->id == acc.ptr->id)
								ClientUserinfoChanged(i);
						}
					}
				} else {
					G_Printf( "Failed to enable flag '%s' for account '%s' (id: %d)\n", flagName, acc.ptr->name, acc.ptr->id );
				}
			} else {
				// we are disabling the flag
				if ( G_SetAccountFlags( acc.ptr, flag, qfalse ) ) {
					G_Printf( "Flag '%s' ^1disabled^7 for account '%s' (id: %d)\n", flagName, acc.ptr->name, acc.ptr->id );
					if (flag == ACCOUNTFLAG_PERMABARRED || flag == ACCOUNTFLAG_HARDPERMABARRED || flag == ACCOUNTFLAG_LSAFKTROLL) { // refresh userinfo of anyone connected on this account
						for (int i = 0; i < MAX_CLIENTS; i++) {
							if (g_entities[i].inuse && g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED && g_entities[i].client->account && g_entities[i].client->account->id == acc.ptr->id)
								ClientUserinfoChanged(i);
						}
					}
				} else {
					G_Printf( "Failed to disable flag '%s' for account '%s' (id: %d)\n", flagName, acc.ptr->name, acc.ptr->id );
				}
			}

		} else if ( !Q_stricmp( s, "autolink" ) ) {
			if (trap_Argc() < 6) {
				G_Printf("Usage:^3 account autolink <username> [sex] [ja_guid] [country]^7 (at least 1 of sex/guid required, country optional, use 0 to omit an argument)\nExample: account autolink broseph 0 ASDFASDF 0\nNote: only use this for repeatedly problematic people\n");
				return;
			}
			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv(2, username, sizeof(username));

			accountReference_t acc = G_GetAccountByName(username, qfalse);

			if (!acc.ptr) {
				G_Printf("Account '%s' does not exist\n", username);
				return;
			}

			char sex[32] = { 0 };
			trap_Argv(3, sex, sizeof(sex));
			if (!sex[0]) {
				G_Printf("Usage:^3 account autolink <username> [sex] [ja_guid] [country]^7 (at least 1 of sex/guid required, country optional, use 0 to omit an argument)\nExample: account autolink broseph 0 ASDFASDF 0\nNote: only use this for repeatedly problematic people\n");
				return;
			}

			qboolean gotSexOrGuid = qfalse;
			if (strcmp(sex, "0"))
				gotSexOrGuid = qtrue;

			char guid[33] = { 0 };
			trap_Argv(4, guid, sizeof(guid));
			if (!guid[0]) {
				G_Printf("Usage:^3 account autolink <username> [sex] [ja_guid] [country]^7 (at least 1 of sex/guid required, country optional, use 0 to omit an argument)\nExample: account autolink broseph 0 ASDFASDF 0\nNote: only use this for repeatedly problematic people\n");
				return;
			}

			if (strcmp(guid, "0"))
				gotSexOrGuid = qtrue;

			if (!gotSexOrGuid) {
				G_Printf("Usage:^3 account autolink <username> [sex] [ja_guid] [country]^7 (at least 1 of sex/guid required, country optional, use 0 to omit an argument)\nExample: account autolink broseph 0 ASDFASDF 0\nNote: only use this for repeatedly problematic people\n");
				return;
			}

			if (sex[0] && strcmp(sex, "0"))
				Q_strncpyz(acc.ptr->autoLink.sex, sex, sizeof(acc.ptr->autoLink.sex));
			if (guid[0] && strcmp(guid, "0"))
				Q_strncpyz(acc.ptr->autoLink.guid, guid, sizeof(acc.ptr->autoLink.guid));

			char *country = ConcatArgs(5);
			if (VALIDSTRING(country) && strcmp(country, "0"))
				Q_strncpyz(acc.ptr->autoLink.country, country, sizeof(acc.ptr->autoLink.country));

			G_DBSetAccountProperties(acc.ptr);
			G_DBCacheAutoLinks();
			G_Printf("Created autolink for %s with sex %s, guid %s, country %s\n", acc.ptr->name, acc.ptr->autoLink.sex[0] ? acc.ptr->autoLink.sex : "none", acc.ptr->autoLink.guid[0] ? acc.ptr->autoLink.guid : "none", acc.ptr->autoLink.country[0] ? acc.ptr->autoLink.country : "none");
		} else if (!Q_stricmp(s, "unautolink")) {
			if (trap_Argc() < 3) {
				G_Printf("Usage:^3 account autolink <username>^7\n");
				return;
			}
			char username[MAX_ACCOUNTNAME_LEN];
			trap_Argv(2, username, sizeof(username));

			accountReference_t acc = G_GetAccountByName(username, qfalse);

			if (!acc.ptr) {
				G_Printf("Account '%s' does not exist\n", username);
				return;
			}

			memset(&acc.ptr->autoLink, 0, sizeof(acc.ptr->autoLink));
			G_DBSetAccountProperties(acc.ptr);
			G_DBCacheAutoLinks();
			G_Printf("Removed autolink for account '%s'\n", username);
		} else if (!Q_stricmp(s, "help")) {
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
			S_COLOR_YELLOW"account list [page]"S_COLOR_WHITE": Prints a list of created accounts\n"
			S_COLOR_YELLOW"account info <username> [page]"S_COLOR_WHITE": Prints various information for the given account name\n"
			S_COLOR_YELLOW"account toggleflag <username> <flag>"S_COLOR_WHITE": Toggles an account flag for the given account name\n"
			S_COLOR_YELLOW"account autolink <username> <sex> [country]"S_COLOR_WHITE": Sets up autolinking for the given account name\n"
			S_COLOR_YELLOW"account unautolink <username>"S_COLOR_WHITE": Disables autolinking for the given account name\n"
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

	char countryStr[128] = { 0 };
	sessionReference_t sess = G_GetSessionByID(sessionId, qfalse);
	if (sess.ptr) {
		char ipStr[64] = { 0 };
		G_GetStringFromSessionInfo(sess.ptr, "ip", ipStr, sizeof(ipStr));
		if (ipStr[0])
			trap_GetCountry(ipStr, countryStr, sizeof(countryStr));
	}

	G_Printf(
		"^7%s  ^7(%s, %s) - session ID: %d%s\n",
		nameString, countryStr[0] ? countryStr : "Unknown country", durationString, sessionId, referencedString
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

					Q_strcat( line, sizeof( line ), va( "%sClient %d "S_COLOR_WHITE"(%s"S_COLOR_WHITE"): Session ID %d (hash: %llx)",
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

		} else if ( !Q_stricmp( s, "nicknames" ) || !Q_stricmp(s, "info")) {

			if (trap_Argc() < 3) {
				G_Printf("Usage: "S_COLOR_YELLOW"session info <session id>\n");
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

			G_Printf("^3Info for session %d^7\n", sessionId);
			char ipStr[64] = { 0 };
			G_GetStringFromSessionInfo(sess.ptr, "ip", ipStr, sizeof(ipStr));
			if (ipStr[0]) {
				char countryStr[128] = { 0 };
				trap_GetCountry(ipStr, countryStr, sizeof(countryStr));
				if (countryStr[0])
					G_Printf("^5Country: ^7%s\n", countryStr);
			}
			G_Printf("^5Top nicknames:^7\n");

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
					"^7%s  ^7(%s)\n",
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
				if (G_DBGetMetadataInteger("shouldReloadPlayerPugStats") != 2)
					G_DBSetMetadata("shouldReloadPlayerPugStats", "1");
				// todo: iterate through all currently connected clients and run ClientUserinfoChanged() on anyone we just linked
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
				client->sess.verifiedByVerifyCommand = qfalse;

				if (g_gametype.integer == GT_CTF) {
					char posBuf[8] = { 0 };
					G_DBGetMetadata(va("remindpos_account_%d", acc.ptr->id), posBuf, sizeof(posBuf));
					if (posBuf[0]) {
						int pos = atoi(posBuf);
						assert(pos >= CTFPOSITION_BASE && pos <= CTFPOSITION_OFFENSE);
						Com_DebugPrintf("Restored client %d position %d from database\n", client - level.clients, pos);
						client->sess.remindPositionOnMapChange.pos = pos;
						client->sess.remindPositionOnMapChange.valid = qtrue;
						if (client->stats)
							client->stats->remindedPosition = pos;
						switch (pos) {
						case CTFPOSITION_BASE: client->sess.remindPositionOnMapChange.score = 8000; break;
						case CTFPOSITION_CHASE: client->sess.remindPositionOnMapChange.score = 4000; break;
						case CTFPOSITION_OFFENSE: client->sess.remindPositionOnMapChange.score = 1000; break;
						default: assert(qfalse);
						}
					}
				}

				G_Printf( "Client session successfully linked to account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id );
				if (level.pause.state != PAUSE_NONE)
					RestoreDisconnectedPlayerData(&g_entities[clientId]);
				if (G_DBGetMetadataInteger("shouldReloadPlayerPugStats") != 2)
					G_DBSetMetadata("shouldReloadPlayerPugStats", "1");
				ClientUserinfoChanged(clientId);
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
				if (G_DBGetMetadataInteger("shouldReloadPlayerPugStats") != 2)
					G_DBSetMetadata("shouldReloadPlayerPugStats", "1");
				// todo: iterate through all currently connected clients and run ClientUserinfoChanged() on anyone we just unlinked
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

			if (client->sess.fuckOffHannah && (!Q_stricmp(client->account->name, "alpha") || !Q_stricmp(client->account->name, "duo"))) {
				G_Printf("Failed to unlink client session from this account!\n");
				return;
			}

			// save a reference so we can print the name even after unlinking
			accountReference_t acc;
			acc.ptr = client->account;
			acc.online = qtrue;

			if ( G_UnlinkAccountFromSession( client->session ) ) {
				client->sess.verifiedByVerifyCommand = qfalse;
				G_Printf( "Client session successfully unlinked from account '%s' (id: %d)\n", acc.ptr->name, acc.ptr->id );
				if (G_DBGetMetadataInteger("shouldReloadPlayerPugStats") != 2)
					G_DBSetMetadata("shouldReloadPlayerPugStats", "1");
				ClientUserinfoChanged(clientId);
			} else {
				G_Printf( "Failed to unlink client session from this account!\n" );
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
			S_COLOR_YELLOW"session info <session id>"S_COLOR_WHITE": Prints detailed info for a given session ID\n"
			S_COLOR_YELLOW"session link <session id> <account name>"S_COLOR_WHITE": Links the given session ID to an existing account\n"
			S_COLOR_YELLOW"session linkingame <client id> <account name>"S_COLOR_WHITE": Shortcut command to link an in-game client's session to an existing account\n"
			S_COLOR_YELLOW"session unlink <session id>"S_COLOR_WHITE": Unlinks the account associated to the given session ID\n"
			S_COLOR_YELLOW"session unlinkingame <client id>"S_COLOR_WHITE": Shortcut command to unlink the account associated to an in-game client's session\n"
			S_COLOR_YELLOW"session purge"S_COLOR_WHITE": Deletes all unreferenced sessions from the database. USE WITH CAUTION!\n"
			S_COLOR_YELLOW"session help"S_COLOR_WHITE": Prints this message\n"
		);
	}
}

static void Svcmd_AutoRestart_f(void) {
	if (g_gametype.integer != GT_CTF)
		return;

	if (!g_waitForAFK.integer) {
		Com_Printf("Auto start is disabled.\n");
		return;
	}

	// if there aren't 6+ people ingame, just do a regular restart
	int minPlayers = g_waitForAFKMinPlayers.integer;
	if (minPlayers <= 0)
		minPlayers = WAITFORAFK_MINPLAYERS_DEFAULT;
	minPlayers = Com_Clampi(WAITFORAFK_MINPLAYERS_MIN, WAITFORAFK_MINPLAYERS_MAX, minPlayers);
	if (TeamCount(-1, TEAM_RED) + TeamCount(-1, TEAM_BLUE) < minPlayers) {
		trap_SendConsoleCommand(EXEC_NOW, va("map_restart %d\n", Com_Clampi(0, 60, g_restart_countdown.integer)));
		level.autoStartPending = qfalse;
		return;
	}

	Com_Printf("Auto start initiated.\n");
	level.autoStartPending = qtrue;
}

static void Svcmd_AutoRestartCancel_f(void) {
	if (!level.autoStartPending)
		return;

	Com_Printf("Auto start cancelled.\n");
	level.autoStartPending = qfalse;
}

static void NotifyPlayerOfAdminPosChange(account_t *account, const char *str) {
#if 0
	assert(account && str);
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->client || ent->client->pers.connected == CON_DISCONNECTED || !ent->client->account || ent->client->account != account)
			continue;
		PrintIngame(i, str);
	}
#endif
}

// ent has a small penis
static void Svcmd_Pos_f(void) {
	if (trap_Argc() < 3) {
		Com_Printf(
			"Usage:\n"
			"^7pos view <account name>                          - view someone's position preferences\n"
			"^9pos set <account name> <pos> <1/2/3/avoid/clear> - set someone's position preferences\n"
			"^7pos ban <account name> <pos>                     - ban someone on a position\n"
			"^9pos unban <account name> <pos>                   - unban someone on a position\n"
			"^7"
		);
		return;
	}

	char arg1[MAX_ACCOUNTNAME_LEN] = { 0 }, arg2[MAX_STRING_CHARS] = { 0 }, arg3[MAX_STRING_CHARS] = { 0 }, arg4[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, arg1, sizeof(arg1));
	Q_strlwr(arg1);
	if (trap_Argc() >= 3) {
		trap_Argv(2, arg2, sizeof(arg2));
		if (trap_Argc() >= 4) {
			trap_Argv(3, arg3, sizeof(arg3));
			if (trap_Argc() >= 5) {
				trap_Argv(4, arg4, sizeof(arg4));
			}
		}
	}

	if (!arg1[0] || !arg2[0]) {
		Com_Printf(
			"Usage:\n"
			"^7pos view <account name>                          - view someone's position preferences\n"
			"^9pos set <account name> <pos> <1/2/3/avoid/clear> - set someone's position preferences\n"
			"^7pos ban <account name> <pos>                     - ban someone on a position\n"
			"^9pos unban <account name> <pos>                   - unban someone on a position\n"
			"^7"
		);
		return;
	}

	accountReference_t acc = G_GetAccountByName(arg2, qfalse);
	if (!acc.ptr) {
		G_Printf("Account '%s' does not exist.\n", arg2);
		return;
	}
	positionPreferences_t *pref = &acc.ptr->expressedPref;

	if (!Q_stricmp(arg1, "view") || !Q_stricmp(arg1, "list")) {
		char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%s's position preferences: \n^3       First choice:^7 ", acc.ptr->name);

		{
			qboolean gotOne = qfalse;
			for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
				if (pref->first & (1 << pos)) {
					Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
					gotOne = qtrue;
				}
			}
		}

		{
			Q_strcat(buf, sizeof(buf), "\n^9      Second choice:^7 ");
			qboolean gotOne = qfalse;
			for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
				if (pref->second & (1 << pos)) {
					Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
					gotOne = qtrue;
				}
			}
		}

		{
			Q_strcat(buf, sizeof(buf), "\n^8       Third choice:^7 ");
			qboolean gotOne = qfalse;
			for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
				if (pref->third & (1 << pos)) {
					Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
					gotOne = qtrue;
				}
			}
		}

		{
			Q_strcat(buf, sizeof(buf), "\n^1              Avoid:^7 ");
			qboolean gotOne = qfalse;
			for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
				if (pref->avoid & (1 << pos)) {
					Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
					gotOne = qtrue;
				}
			}
		}

		{
			Q_strcat(buf, sizeof(buf), "\n^9            Unrated:^7 ");
			qboolean gotOne = qfalse;
			for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
				if (pref->first & (1 << pos) || pref->second & (1 << pos) || pref->third & (1 << pos) || pref->avoid & (1 << pos))
					continue;
				Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
				gotOne = qtrue;
			}
		}

		{
			Q_strcat(buf, sizeof(buf), "\n^6[Admin only] Banned:^7 ");
			qboolean gotOne = qfalse;
			for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
				if (!(acc.ptr->bannedPos & (1 << pos)))
					continue;
				Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
				gotOne = qtrue;
			}
		}

		Q_strcat(buf, sizeof(buf), "\n");
		Com_Printf(buf);

		return;
	}
	else if (!Q_stricmp(arg1, "set") || !Q_stricmp(arg1, "change")) {
		if (!arg3[0] || !CtfPositionFromString(arg3)) {
			Com_Printf("Usage: pos set <account name> <pos> <1/2/3/avoid/clear>\n");
			return;
		}

		const int pos = CtfPositionFromString(arg3);
		const char *posStr = NameForPos(pos);

		enum {
			INTENTION_UNKNOWN = 0,
			INTENTION_FIRSTCHOICE,
			INTENTION_SECONDCHOICE,
			INTENTION_THIRDCHOICE,
			INTENTION_AVOID,
			INTENTION_CLEAR
		} intention = INTENTION_UNKNOWN;

		if (stristr(arg4, "1") || stristr(arg4, "pref") || stristr(arg4, "top"))
			intention = INTENTION_FIRSTCHOICE;
		else if (stristr(arg4, "2") || stristr(arg4, "sec"))
			intention = INTENTION_SECONDCHOICE;
		else if (stristr(arg4, "3") || stristr(arg4, "thi"))
			intention = INTENTION_THIRDCHOICE;
		else if (stristr(arg4, "av"))
			intention = INTENTION_AVOID;
		else if (stristr(arg4, "cl") || stristr(arg4, "rem") || stristr(arg4, "rm") || stristr(arg4, "del") || stristr(arg4, "res") || stristr(arg4, "0"))
			intention = INTENTION_CLEAR;

		if (!intention) {
			Com_Printf("'%s' is not a valid option. Enter 1/2/3/avoid/clear.\n", arg4);
			return;
		}

		if (intention == INTENTION_FIRSTCHOICE) {
			if (pref->first & (1 << pos)) {
				Com_Printf("%s is already %s's first choice.\n", posStr, acc.ptr->name);
				return;
			}

			qboolean printed = qfalse;

			if (pref->second & (1 << pos)) {
				Com_Printf("%s changed from %s's second choice to first choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from second choice to first choice.\n", posStr));
				pref->second &= ~(1 << pos);
				printed = qtrue;
			}

			if (pref->third & (1 << pos)) {
				Com_Printf("%s changed from %s's third choice to first choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from third choice to first choice.\n", posStr));
				pref->third &= ~(1 << pos);
				printed = qtrue;
			}

			if (pref->avoid & (1 << pos)) {
				Com_Printf("%s changed from avoided to %s's first choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from avoided to first choice.\n", posStr));
				pref->avoid &= ~(1 << pos);
				printed = qtrue;
			}

			pref->first |= (1 << pos);

			if (!printed) {
				Com_Printf("Set %s to %s's first choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was set by admin to first choice.\n", posStr));
			}
		}
		else if (intention == INTENTION_SECONDCHOICE) {
			if (pref->second & (1 << pos)) {
				Com_Printf("%s is already %s's second choice.\n", posStr, acc.ptr->name);
				return;
			}

			qboolean printed = qfalse;

			if (pref->first & (1 << pos)) {
				Com_Printf("%s changed from %s's first choice to second choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from first choice to second choice.\n", posStr));
				pref->first &= ~(1 << pos);
				printed = qtrue;
			}

			if (pref->third & (1 << pos)) {
				Com_Printf("%s changed from %s's third choice to second choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from third choice to second choice.\n", posStr));
				pref->third &= ~(1 << pos);
				printed = qtrue;
			}

			if (pref->avoid & (1 << pos)) {
				Com_Printf("%s changed from avoided to %s's second choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from avoided to second choice.\n", posStr));
				pref->avoid &= ~(1 << pos);
				printed = qtrue;
			}

			pref->second |= (1 << pos);

			if (!printed) {
				Com_Printf("Set %s to %s's second choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was set by admin to second choice.\n", posStr));
			}
		}
		else if (intention == INTENTION_THIRDCHOICE) {
			if (pref->third & (1 << pos)) {
				Com_Printf("%s is already %s's third choice.\n", posStr, acc.ptr->name);
				return;
			}

			qboolean printed = qfalse;

			if (pref->first & (1 << pos)) {
				Com_Printf("%s changed from %s's first choice to third choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from first choice to third choice.\n", posStr));
				pref->first &= ~(1 << pos);
				printed = qtrue;
			}

			if (pref->second & (1 << pos)) {
				Com_Printf("%s changed from %s's second choice to third choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from second choice to third choice.\n", posStr));
				pref->second &= ~(1 << pos);
				printed = qtrue;
			}

			if (pref->avoid & (1 << pos)) {
				Com_Printf("%s changed from avoided to %s's third choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from avoided to third choice.\n", posStr));
				pref->avoid &= ~(1 << pos);
				printed = qtrue;
			}

			pref->third |= (1 << pos);

			if (!printed) {
				Com_Printf("Set %s to %s's third choice.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was set by admin to third choice.\n", posStr));
			}
		}
		else if (intention == INTENTION_AVOID) {
			if (pref->avoid & (1 << pos)) {
				Com_Printf("%s is already avoided by %s.\n", posStr, acc.ptr->name);
				return;
			}

			qboolean printed = qfalse;

			if (pref->first & (1 << pos)) {
				Com_Printf("%s changed from %s's first choice to avoided.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from first choice to avoided.\n", posStr));
				pref->first &= ~(1 << pos);
				printed = qtrue;
			}

			if (pref->second & (1 << pos)) {
				Com_Printf("%s changed from %s's second choice to avoided.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from second choice to avoided.\n", posStr));
				pref->second &= ~(1 << pos);
				printed = qtrue;
			}

			if (pref->third & (1 << pos)) {
				Com_Printf("%s changed from %s's third choice to avoided.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was changed by admin from third choice to avoided.\n", posStr));
				pref->third &= ~(1 << pos);
				printed = qtrue;
			}

			pref->avoid |= (1 << pos);

			if (!printed) {
				Com_Printf("Set %s to avoided by %s.\n", posStr, acc.ptr->name);
				NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was set by admin to avoided.\n", posStr));
			}
		}
		else /*if (intention == INTENTION_CLEAR)*/ {
			Com_Printf("Cleared all preferences for %s on %s.\n", acc.ptr->name, posStr);
			NotifyPlayerOfAdminPosChange(acc.ptr, va("Your preference of %s was cleared by admin.\n", posStr));
			pref->first &= ~(1 << pos);
			pref->second &= ~(1 << pos);
			pref->third &= ~(1 << pos);
			pref->avoid &= ~(1 << pos);
		}

		if (!ValidateAndCopyPositionPreferences(pref, &acc.ptr->validPref, acc.ptr->bannedPos))
			Com_Printf("^3Warning^7: %s's position preferences are invalid. Check that they make sense (e.g. not having a position in more than one tier, not *only* having second choice preferences, etc.)\n", acc.ptr->name);
		G_DBSetAccountProperties(acc.ptr);

		return;
	}
	else if (!Q_stricmp(arg1, "ban")) {
		if (!arg3[0] || !CtfPositionFromString(arg3)) {
			Com_Printf("Usage: pos ban <account name> <pos>\n");
			return;
		}

		const int pos = CtfPositionFromString(arg3);
		const char *posStr = NameForPos(pos);
		if (acc.ptr->bannedPos & (1 << pos)) {
			Com_Printf("%s is already banned from %s.\n", acc.ptr->name, posStr);
			return;
		}

		acc.ptr->bannedPos |= (1 << pos);
		Com_Printf("%s is now banned from %s.\n", acc.ptr->name, posStr);

		ValidateAndCopyPositionPreferences(&acc.ptr->expressedPref, &acc.ptr->validPref, acc.ptr->bannedPos);
		G_DBSetAccountProperties(acc.ptr);
	}
	else if (!Q_stricmp(arg1, "unban")) {
		if (!arg3[0] || !CtfPositionFromString(arg3)) {
			Com_Printf("Usage: pos unban <account name> <pos>\n");
			return;
		}

		const int pos = CtfPositionFromString(arg3);
		const char *posStr = NameForPos(pos);
		if (!(acc.ptr->bannedPos & (1 << pos))) {
			Com_Printf("%s is already not banned from %s.\n", acc.ptr->name, posStr);
			return;
		}

		acc.ptr->bannedPos &= ~(1 << pos);
		Com_Printf("%s is now unbanned from %s.\n", acc.ptr->name, posStr);

		ValidateAndCopyPositionPreferences(&acc.ptr->expressedPref, &acc.ptr->validPref, acc.ptr->bannedPos);
		G_DBSetAccountProperties(acc.ptr);
	}
	else {
		Com_Printf(
			"Usage:\n"
			"^7pos view <account name>                          - view someone's position preferences\n"
			"^9pos set <account name> <pos> <1/2/3/avoid/clear> - set someone's position preferences\n"
			"^7pos ban <account name> <pos>                     - ban someone on a position\n"
			"^9pos unban <account name> <pos>                   - unban someone on a position\n"
			"^7"
		);
		return;
	}
}

static void Svcmd_SetPos_f(void) {
	if (g_gametype.integer != GT_CTF) {
		return;
	}

	if (trap_Argc() < 3) {
		Com_Printf("Usage: setpos <account name> <pos/reset>    (sets current pos that is broadcast to clients and used for various serverside things)\n");
		return;
	}

	char arg1[MAX_STRING_CHARS] = { 0 }, arg2[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, arg1, sizeof(arg1));
	trap_Argv(2, arg2, sizeof(arg2));

	if (!arg1[0] || !arg2[0]) {
		Com_Printf("Usage: setpos <player> <pos>    (sets current pos that is broadcast to clients and used for various serverside things)\n");
		return;
	}

	int clientNum = -1;
	if (Q_isanumber(arg1)) {
		int num = atoi(arg1);
		if (num >= 0 && num < MAX_CLIENTS && g_entities[num].inuse && g_entities[num].client && g_entities[num].client->pers.connected != CON_DISCONNECTED)
			clientNum = num;
	}
	else {
		char lowercase[MAX_QPATH] = { 0 };
		Q_strncpyz(lowercase, arg1, sizeof(lowercase));
		Q_strlwr(lowercase);
		account_t account = { 0 };
		if (G_DBGetAccountByName(lowercase, &account)) {
			int accountId = account.id;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *checkEnt = &g_entities[i];
				if (!checkEnt->inuse || !checkEnt->client || !checkEnt->client->pers.connected == CON_DISCONNECTED)
					continue;
				if (!checkEnt->client->account || checkEnt->client->account->id != accountId)
					continue;
				clientNum = i;
				break;
			}
		}
		else {
			gentity_t *found = G_FindClient(arg1);
			if (found && found->client)
				clientNum = found - g_entities;
		}
	}

	if (clientNum == -1) {
		Com_Printf("Unable to find any player matching '%s'.\n", arg1);
		return;
	}

	ctfPosition_t pos = CtfPositionFromString(arg2);

	gentity_t *ent = &g_entities[clientNum];
	assert(ent && ent->client);

	if (pos == CTFPOSITION_UNKNOWN) {
		memset(&ent->client->sess.remindPositionOnMapChange, 0, sizeof(ent->client->sess.remindPositionOnMapChange));
		ClientUserinfoChanged(clientNum);
		Com_Printf("Reset client %d (%s)'s current position.\n", clientNum, ent->client->pers.netname);

		if (ent->client->account)
			G_DBDeleteMetadata(va("remindpos_account_%d", ent->client->account->id));
		return;
	}

	ent->client->sess.remindPositionOnMapChange.pos = pos;
	ent->client->sess.remindPositionOnMapChange.valid = qtrue;
	if (ent->client->stats)
		ent->client->stats->remindedPosition = pos;
	switch (pos) {
	case CTFPOSITION_BASE: ent->client->sess.remindPositionOnMapChange.score = 8000; break;
	case CTFPOSITION_CHASE: ent->client->sess.remindPositionOnMapChange.score = 4000; break;
	case CTFPOSITION_OFFENSE: ent->client->sess.remindPositionOnMapChange.score = 1000; break;
	default: assert(qfalse);
	}
	
	if (ent->client->account)
		G_DBSetMetadata(va("remindpos_account_%d", ent->client->account->id), va("%d", pos));

	ClientUserinfoChanged(clientNum);
	Com_Printf("Changed client %d (%s)'s current position to %s.\n", clientNum, ent->client->pers.netname, NameForPos(pos));

	level.posChecksTime = (level.time - level.startTime) + 30000; // bump automatic checks so this isn't undone just a couple seconds later
}

static void Svcmd_CtfStats_f(void) {
	char buf[16384] = { 0 };
	if (trap_Argc() < 2) { // display all types if none is specified, i guess
		Stats_Print(NULL, "general", buf, sizeof(buf), qfalse, NULL);
		if (buf[0]) { Com_Printf(buf); buf[0] = '\0'; }
	}
	else if (trap_Argc() == 2) {
		char subcmd[MAX_STRING_CHARS] = { 0 };
		trap_Argv(1, subcmd, sizeof(subcmd));
		Stats_Print(NULL, subcmd, buf, sizeof(buf), qfalse, NULL);
		if (buf[0]) { Com_Printf(buf); }
	}
	else {
		char subcmd[MAX_STRING_CHARS] = { 0 };
		trap_Argv(1, subcmd, sizeof(subcmd));

		if (!Q_stricmp(subcmd, "weapon")) {
			char weaponPlayerArg[MAX_STRING_CHARS] = { 0 };
			trap_Argv(2, weaponPlayerArg, sizeof(weaponPlayerArg));
			if (weaponPlayerArg[0]) {
				stats_t *found = GetStatsFromString(weaponPlayerArg);
				if (!found) {
					Com_Printf("Client %s^7 not found or ambiguous. Use client number or be more specific.\n", weaponPlayerArg);
					return;
				}
				Stats_Print(NULL, subcmd, buf, sizeof(buf), qfalse, found);
				if (buf[0]) { Com_Printf(buf); }
			}
			else {
				Stats_Print(NULL, subcmd, buf, sizeof(buf), qfalse, NULL);
				if (buf[0]) { Com_Printf(buf); }
			}
		}
		else {
			Stats_Print(NULL, subcmd, buf, sizeof(buf), qfalse, NULL);
			if (buf[0]) { Com_Printf(buf); }
		}
	}
}

extern void EpochToHumanReadable(double epochTime, char *buffer, size_t bufferSize);

const char *TableCallback_FilterID(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	filter_t *filter = rowContext;
	return va("%d", filter->id);
}

const char *TableCallback_FilterText(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	filter_t *filter = rowContext;
	return filter->filterText;
}

const char *TableCallback_FilterCreatedAt(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	filter_t *filter = rowContext;
	static char createdAtStr[128] = { 0 };
	time_t rawtime = filter->createdAt;
	struct tm *timeinfo = localtime(&rawtime);
	strftime(createdAtStr, sizeof(createdAtStr), "%Y-%m-%d %H:%M:%S", timeinfo);
	return createdAtStr;
}

const char *TableCallback_FilterTimesFiltered(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	filter_t *filter = rowContext;
	return va("%d", filter->count);
}

const char *TableCallback_FilterLastFiltered(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	filter_t *filter = rowContext;
	if (!filter->count)
		return "Never";
	static char lastFilteredStr[128] = { 0 };
	time_t rawtime = filter->lastFiltered;
	struct tm *timeinfo = localtime(&rawtime);
	strftime(lastFilteredStr, sizeof(lastFilteredStr), "%Y-%m-%d %H:%M:%S", timeinfo);
	return lastFilteredStr;
}

const char *TableCallback_FilterDescription(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	filter_t *filter = rowContext;
	return filter->description;
}

static void Svcmd_Filter_f(void) {
	if (!g_filterUsers.integer) {
		Com_Printf("g_filterUsers is disabled.\n");
		return;
	}

	if (trap_Argc() < 2) {
		Com_Printf("^7Usage:\n"
			"^7filter list                                - list all loaded filters\n"
			"^9filter add [filter] <optional description> - add a new filter\n"
			"^7filter delete [id]                         - delete a filter\n"
			"Enter ^5filter add^7 with no arguments for additional help.\n");
		return;
	}

	char subcommand[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, subcommand, sizeof(subcommand));

	if (!Q_stricmp(subcommand, "list") || !Q_stricmp(subcommand, "status")) {
		if (!level.filtersList.size) {
			Com_Printf("No filters loaded.\n");
			return;
		}

		iterator_t iter;
		ListIterate(&level.filtersList, &iter, qfalse);
		Table *t = Table_Initialize(qtrue);
		int numFilters = 0;

		while (IteratorHasNext(&iter)) {
			filter_t *filter = (filter_t *)IteratorNext(&iter);
			Table_DefineRow(t, filter);
			numFilters++;
		}

		if (numFilters) {
			Table_DefineColumn(t, "ID", TableCallback_FilterID, NULL, qtrue, -1, 16);
			Table_DefineColumn(t, "Created", TableCallback_FilterCreatedAt, NULL, qtrue, -1, 64);
			Table_DefineColumn(t, "Hits", TableCallback_FilterTimesFiltered, NULL, qtrue, -1, 32);
			Table_DefineColumn(t, "Last hit", TableCallback_FilterLastFiltered, NULL, qtrue, -1, 64);
			Table_DefineColumn(t, "Description", TableCallback_FilterDescription, NULL, qtrue, -1, 128);
			Table_DefineColumn(t, "Filter text", TableCallback_FilterText, NULL, qtrue, -1, MAX_STRING_CHARS);

			char *buf = calloc(16384, sizeof(char));
			Table_WriteToBuffer(t, buf, 16384, qtrue, -1);
			Q_strcat(buf, 16384, "\n");
			Com_Printf(buf);
			free(buf);
		}
		Table_Destroy(t);
	}
	else if (!Q_stricmp(subcommand, "add") || !Q_stricmp(subcommand, "new") || !Q_stricmp(subcommand, "create") || !Q_stricmp(subcommand, "save")) {
		if (trap_Argc() < 3) {
			Com_Printf("Usage: filter add [filter] <optional description>\n");
			Com_Printf("Use ^5rcon userinfo^7 to get userinfo. Separate multiple required key/values with quadruple backslashes. Use * for wildcards (including 0, 1, or 2+ chars)\n");
			Com_Printf("Example usage: ^5filter add name\\f*cker\\\\\\\\model\\kyle annoying chat spammer^7\n");
			return;
		}

		char filterText[MAX_STRING_CHARS] = { 0 };
		trap_Argv(2, filterText, sizeof(filterText));
		if (!VALIDSTRING(filterText)) {
			Com_Printf("Usage: filter add [filter] <optional description>\n");
			Com_Printf("Use ^5rcon userinfo^7 to get userinfo. Separate multiple required key/values with quadruple backslashes. Use * for wildcards (including 0, 1, or 2+ chars)\n");
			Com_Printf("Example usage: ^5filter add name\\f*cker\\\\\\\\model\\kyle annoying chat spammer^7\n");
			return;
		}

		if (!strchr(filterText, '\\')) {
			Com_Printf("Make sure you include a backslash.\n");
			return;
		}
			
		char description[MAX_STRING_CHARS] = { 0 };
		if (trap_Argc() >= 4)
			Q_strncpyz(description, ConcatArgs(3), sizeof(description));

		filter_t *newFilter = ListAdd(&level.filtersList, sizeof(filter_t));
		Q_strncpyz(newFilter->filterText, filterText, sizeof(newFilter->filterText));
		if (description[0]) {
			Q_strncpyz(newFilter->description, description, sizeof(newFilter->description));
		}
		time(&newFilter->createdAt);

		int newFilterId = -1;
		if (SaveNewFilter(newFilter, &newFilterId)) {
			newFilter->id = newFilterId;
			Com_Printf("Successfully added new filter with ID %d.\n", newFilterId);
		}
		else {
			Com_Printf("Failed to add filter.\n");
			ListRemove(&level.filtersList, newFilter);
		}
	}
	else if (!Q_stricmpn(subcommand, "del", 3) || !Q_stricmpn(subcommand, "rem", 3) || !Q_stricmpn(subcommand, "rm", 2)) {
		if (trap_Argc() < 3) {
			Com_Printf("^7Usage: filter delete [id]\n");
			return;
		}

		char idStr[MAX_STRING_CHARS] = { 0 };
		trap_Argv(2, idStr, sizeof(idStr));
		if (!Q_isanumber(idStr)) {
			Com_Printf("^7Usage: filter delete [id]\n");
			return;
		}
		int filterId = atoi(idStr);
		iterator_t iter;
		ListIterate(&level.filtersList, &iter, qfalse);
		qboolean found = qfalse;
		while (IteratorHasNext(&iter)) {
			filter_t *filter = IteratorNext(&iter);
			if (filter->id == filterId) {
				found = qtrue;
				ListRemove(&level.filtersList, filter);
			}
		}

		if (found) {
			if (DeleteFilter(filterId))
				Com_Printf("Filter with ID %d successfully deleted from database.\n", filterId);
			else
				Com_Printf("Error deleting ID %d from database!\n", filterId);
		}
		else {
			Com_Printf("Filter not found.\n");
		}
	}
	else {
		Com_Printf("^7Usage:\n"
			"^7filter list                                - list all loaded filters\n"
			"^9filter add [filter] <optional description> - add a new filter\n"
			"^7filter delete [id]                         - delete a filter\n"
			"Enter ^5filter add^7 with no arguments for additional help.\n");
	}
}


#ifdef _DEBUG
// test tick tracking for discord webhook
static void Svcmd_DebugTicks_f(void) {
	if (!level.wasRestarted) {
		Com_Printf("Map was not restarted.\n");
		return;
	}
	iterator_t iter;
	Com_Printf("^1RED TEAM^7:\n");
	ListIterate(&level.redPlayerTickList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		tickPlayer_t *found = IteratorNext(&iter);
		if (found) {
			nicknameEntry_t nickname = { 0 };
			G_DBGetTopNickname(found->sessionId, &nickname);
			if (!nickname.name[0])
				Q_strncpyz(nickname.name, found->name, sizeof(nickname.name));

			qboolean isSub = qfalse;
			int subThreshold = (int)((float)level.numTeamTicks * WEBHOOK_INGAME_THRESHOLD_SUB);
			int thisGuyTicks = found->numTicks;
			if (thisGuyTicks < subThreshold)
				isSub = qtrue;

			qboolean isHere = qfalse;
			for (int i = 0; i < level.maxclients; i++) {
				if (level.clients[i].pers.connected != CON_CONNECTED)
					continue;
				if ((level.clients[i].account && level.clients[i].account->id != ACCOUNT_ID_UNLINKED && level.clients[i].account->id == found->accountId) ||
					(level.clients[i].session && level.clients[i].session->id == found->sessionId)) {
					isHere = qtrue;
					break;
				}
			}

			Com_Printf("%s^7 with client num %d, account id %d, session id %d, %d ticks%s%s\n", nickname.name, found->clientNum, found->accountId, found->sessionId, found->numTicks,
			isSub ? " (SUB)" : "", isHere ? "" : " (RQ)");
		}
	}
	Com_Printf("^4BLUE TEAM^7:\n");
	ListIterate(&level.bluePlayerTickList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		tickPlayer_t *found = IteratorNext(&iter);
		if (found) {
			nicknameEntry_t nickname = { 0 };
			G_DBGetTopNickname(found->sessionId, &nickname);
			if (!nickname.name[0])
				Q_strncpyz(nickname.name, found->name, sizeof(nickname.name));

			qboolean isSub = qfalse;
			int subThreshold = (int)((float)level.numTeamTicks * WEBHOOK_INGAME_THRESHOLD_SUB);
			int thisGuyTicks = found->numTicks;
			if (thisGuyTicks < subThreshold)
				isSub = qtrue;

			qboolean isHere = qfalse;
			for (int i = 0; i < level.maxclients; i++) {
				if (level.clients[i].pers.connected != CON_CONNECTED)
					continue;
				if ((level.clients[i].account && level.clients[i].account->id != ACCOUNT_ID_UNLINKED && level.clients[i].account->id == found->accountId) ||
					(level.clients[i].session && level.clients[i].session->id == found->sessionId)) {
					isHere = qtrue;
					break;
				}
			}

			Com_Printf("%s^7 with client num %d, account id %d, session id %d, %d ticks%s%s\n", nickname.name, found->clientNum, found->accountId, found->sessionId, found->numTicks,
				isSub ? " (SUB)" : "", isHere ? "" : " (RQ)");
		}
	}
}

void Svcmd_LinkByName_f(void) {
	int numLinked = 0, numAlreadyLinked = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client)
			continue;

		if (ent->client->account) {
			++numAlreadyLinked;
			continue;
		}

		char *clean = strdup(ent->client->pers.netname);
		Q_CleanStr(clean);
		accountReference_t acc = G_GetAccountByName(clean, qfalse);
		free(clean);

		if (!acc.ptr)
			continue;

		trap_SendConsoleCommand(EXEC_APPEND, va("session linkingame %d %s\n", i, acc.ptr->name));
		Com_Printf("Linked client %d to %s\n", i, acc.ptr->name);
		++numLinked;
	}
	Com_Printf("Linked %d clients. %d clients were already linked.\n", numLinked, numAlreadyLinked);
}

void Svmcd_GetMetadata_f(void) {
	if (trap_Argc() < 2) {
		Com_Printf("Usage: getmetadata <key>\n");
		return;
	}

	char key[MAX_TOKEN_CHARS] = { 0 };
	trap_Argv(1, key, sizeof(key));

	if (!key[0]) {
		Com_Printf("Usage: getmetadata <key>\n");
		return;
	}

	char *valueBuf = calloc(16384, sizeof(char));
	G_DBGetMetadata(key, valueBuf, 16384);

	if (valueBuf[0]) {
		Com_Printf("Metadata for key '%s' is: %s\n", key, valueBuf);
	}
	else {
		Com_Printf("No metadata found for key '%s'\n", key);
	}

	free(valueBuf);
}

void Svmcd_SetMetadata_f(void) {
	if (trap_Argc() < 3) {
		Com_Printf("Usage: setmetadata <key> <value>\n");
		return;
	}

	char key[MAX_TOKEN_CHARS] = { 0 };
	trap_Argv(1, key, sizeof(key));

	if (!key[0]) {
		Com_Printf("Usage: setmetadata <key> <value>\n");
		return;
	}

	char valueBuf[1024];
	Q_strncpyz(valueBuf, ConcatArgs(2), sizeof(valueBuf));

	if (!valueBuf[0]) {
		Com_Printf("Usage: setmetadata <key> <value>\n");
		return;
	}

	G_DBSetMetadata(key, valueBuf);
	Com_Printf("Set metadata key '%s' to: %s\n", key, valueBuf);
}
#endif

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

	if (!Q_stricmp(cmd, "mapvote")) {
		Svcmd_MapVote_f(NULL);
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
		if (!level.intermissiontime && !level.intermissionQueued) {
			char durationStr[4];
			int duration;

			trap_Argv(1, durationStr, sizeof(durationStr));
			duration = atoi(durationStr);

			if (duration == 0) // 2 minutes default
				duration = 2 * 60;
			else if (duration < 0) // second minimum
				duration = 1;
			else if (duration > 5 * 60) // 5 minutes max
				duration = 5 * 60;

			if (level.pause.state == PAUSE_NONE)
				DoPauseStartChecks();
			level.pause.state = PAUSE_PAUSED;
			level.pause.time = level.time + duration * 1000; // 5 seconds
		}

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

	if (!Q_stricmp(cmd, "drawmatch"))
	{
#ifdef NEWMOD_SUPPORT
		trap_SendServerCommand(-1, "kls -1 -1 \"em\" 1");
#endif
		trap_SendServerCommand(-1, va("print \"Match forced to end in a draw.\n\""));
		level.forceEndMatchInDraw = qtrue;
		level.teamScores[TEAM_RED] = level.teamScores[TEAM_BLUE] = 0;
		LogExit("Match forced to end in a draw.");
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

	if (!Q_stricmp(cmd, "tier") || !Q_stricmp(cmd, "tiers") || !Q_stricmp(cmd, "tierlist") || !Q_stricmp(cmd, "tierlists")) {
		Svcmd_Tier_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "listmaps") || !Q_stricmp(cmd, "listmap") || !Q_stricmp(cmd, "maplist") || !Q_stricmp(cmd, "mapslist")) {
		Svcmd_ListMaps_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "mapalias") || !Q_stricmp(cmd, "mapaliases")) {
		Svcmd_MapAlias_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "pug")) {
		Svcmd_Pug_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "fixswap")) {
		Svcmd_FixSwap_f();
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

	if (!Q_stricmp(cmd, "auto_restart")) {
		Svcmd_AutoRestart_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "auto_restart_cancel")) {
		Svcmd_AutoRestartCancel_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "pos")) {
		Svcmd_Pos_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "setpos")) {
		Svcmd_SetPos_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "ctfstats") || !Q_stricmp(cmd, "stats")) {
		Svcmd_CtfStats_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "filter")) {
		Svcmd_Filter_f();
		return qtrue;
	}

#ifdef _DEBUG
	if (!Q_stricmp(cmd, "debugticks")) {
		Svcmd_DebugTicks_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "linkbyname")) {
		Svcmd_LinkByName_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "getmetadata")) {
		Svmcd_GetMetadata_f();
		return qtrue;
	}

	if (!Q_stricmp(cmd, "setmetadata")) {
		Svmcd_SetMetadata_f();
		return qtrue;
	}
#endif

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
		OutOfBandPrint(i, S_COLOR_GREEN"[Admin] "S_COLOR_WHITE"Rcon from "S_COLOR_GREEN"%s"S_COLOR_WHITE": /rcon %s\n", displayStr, command);
	}

	// also log it to disk if needed
	if (g_logrcon.integer) {
		G_RconLog("rcon from {%s}: %s\n", ipFrom, command);
	}
}

static char *GetUptime(void) {
	static char buf[256] = { 0 };
	time_t currTime;

	time(&currTime);

	int secs = difftime(currTime, trap_Cvar_VariableIntegerValue("sv_startTime"));
	int mins = secs / 60;
	int hours = mins / 60;
	int days = hours / 24;

	secs %= 60;
	mins %= 60;
	hours %= 24;
	//days %= 365;

	buf[0] = '\0';

	if (days)
		Com_sprintf(buf, sizeof(buf), "%id%ih%im%is", days, hours, mins, secs);
	if (hours)
		Com_sprintf(buf, sizeof(buf), "%ih%im%is", hours, mins, secs);
	else if (mins)
		Com_sprintf(buf, sizeof(buf), "%im%is", mins, secs);
	else
		Com_sprintf(buf, sizeof(buf), "%is", secs);

	return buf;
}

const char *GetGametypeString(int gametype) {
	switch (gametype) {
	case GT_FFA: return "Free For All";
	case GT_HOLOCRON: return "Holocron";
	case GT_JEDIMASTER: return "Jedi Master";
	case GT_DUEL: return "Duel";
	case GT_POWERDUEL: return "Power Duel";
	case GT_SINGLE_PLAYER: return "Cooperative";
	case GT_TEAM: return "Team Free For All";
	case GT_SIEGE: return "Siege";
	case GT_CTF: return "Capture The Flag";
	case GT_CTY: return "Capture The Ysalimiri";
	default: return "Unknown Gametype";
	}
}

void G_Status(void) {
	int humans = 0, bots = 0;
	for (int i = 0; i < level.maxclients; i++) {
		if (level.clients[i].pers.connected) {
			if (g_entities[i].r.svFlags & SVF_BOT)
				bots++;
			else
				humans++;
		}
	}

#if defined(_WIN32)
#define STATUS_OS "Windows"
#elif defined(__linux__)
#define STATUS_OS "Linux"
#elif defined(MACOS_X)
#define STATUS_OS "OSX"
#else
#define STATUS_OS "Unknown OS"
#endif

	char *dedicatedStr;
	switch (trap_Cvar_VariableIntegerValue("dedicated")) {
	case 1: dedicatedStr = "LAN dedicated"; break;
	case 2: dedicatedStr = "Public dedicated"; break;
	default: dedicatedStr = "Listen"; break;
	}

	int days = level.time / 86400000;
	int hours = (level.time % 86400000) / 3600000;
	int mins = (level.time % 3600000) / 60000;
	int secs = (level.time % 60000) / 1000;
	char matchTime[64] = { 0 };
	if (days)
		Com_sprintf(matchTime, sizeof(matchTime), "%i day%s, %i:%02i:%02i", days, days > 1 ? "s" : "", hours, mins, secs);
	else if (hours)
		Com_sprintf(matchTime, sizeof(matchTime), "%i:%02i:%02i", hours, mins, secs);
	else
		Com_sprintf(matchTime, sizeof(matchTime), "%i:%02i", mins, secs);

	Com_Printf("Hostname:   %s^7\n", Cvar_VariableString("sv_hostname"));
	Com_Printf("UDP/IP:     %s:%i, %s, %s\n", Cvar_VariableString("net_ip"), trap_Cvar_VariableIntegerValue("net_port"), STATUS_OS, dedicatedStr);
	Com_Printf("Uptime:     %s\n", GetUptime());
	Com_Printf("Map:        %s (%s)\n", Cvar_VariableString("mapname"), GetGametypeString(g_gametype.integer));
	Com_Printf("Players:    %i%s / %i%s max\n",
		humans, bots ? va(" human%s + %d bot%s", humans > 1 ? "s" : "", bots, bots > 1 ? "s" : "") : "", level.maxclients - sv_privateclients.integer, sv_privateclients.integer ? va("+%i", sv_privateclients.integer) : "");
	Com_Printf("Match time: %s\n", matchTime);
	if (g_gametype.integer == GT_TEAM || g_gametype.integer == GT_CTF)
	Com_Printf("Score:      ^1%d^7 - ^4%d^7\n", level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE]);
	if (g_cheats.integer)
	Com_Printf("^3Cheats enabled^7\n");
	Com_Printf("\n");

	if (humans + bots == 0) {
		Com_Printf("No players are currently connected.\n\n");
		return;
	}

	Table *t = Table_Initialize(qtrue);

	for (int i = 0; i < level.maxclients; i++) {
		if (!level.clients[i].pers.connected)
			continue;
		Table_DefineRow(t, &level.clients[i]);
	}

	Table_DefineColumn(t, "#", TableCallback_ClientNum, NULL, qfalse, -1, 2);
	Table_DefineColumn(t, "Name", TableCallback_Name, NULL, qtrue, -1, MAX_NAME_DISPLAYLENGTH);
	Table_DefineColumn(t, "A", TableCallback_Account, NULL, qtrue, -1, 4);
	Table_DefineColumn(t, "Alias", TableCallback_Alias, NULL, qtrue, -1, MAX_NAME_DISPLAYLENGTH);
	Table_DefineColumn(t, "Country", TableCallback_Country, NULL, qtrue, -1, 64);
	Table_DefineColumn(t, "IP", TableCallback_IP, NULL, qtrue, -1, 21);
	Table_DefineColumn(t, "Qport", TableCallback_Qport, NULL, qfalse, -1, 5);
	Table_DefineColumn(t, "Mod", TableCallback_Mod, NULL, qtrue, -1, 64);
	Table_DefineColumn(t, "Ping", TableCallback_Ping, NULL, qfalse, -1, 4);
	Table_DefineColumn(t, "Score", TableCallback_Score, NULL, qfalse, -1, 5);
	Table_DefineColumn(t, "Shadowmuted", TableCallback_Shadowmuted, NULL, qtrue, -1, 64);

	char buf[4096] = { 0 };
	Table_WriteToBuffer(t, buf, sizeof(buf), qtrue, -1);
	Table_Destroy(t);

	Q_strcat(buf, sizeof(buf), "^7\n");
	Com_Printf(buf);
}