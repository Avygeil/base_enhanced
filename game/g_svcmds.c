// Copyright (C) 1999-2000 Id Software, Inc.
//

// this file holds commands that can be executed by the server console, but not remote clients

#include "g_local.h"
#include "g_database_log.h"
#include "g_database_config.h"

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
    return G_CfgDbIsFiltered( ip, reasonBuffer, reasonBufferSize );
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

    if ( G_CfgDbAddToBlacklist( ipInt, maskInt, notes, reason, hours ) )
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

    if ( G_CfgDbRemoveFromBlacklist( ipInt, maskInt ) )
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

    if ( G_CfgDbAddToWhitelist( ipInt, maskInt, notes ) )
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

    if ( G_CfgDbRemoveFromWhitelist( ipInt, maskInt ) )
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

    G_CfgDbListBlacklist( listCallback );
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
	gclient_t	*cl;
	char		str[MAX_TOKEN_CHARS];

	// find the player
	trap_Argv( 1, str, sizeof( str ) );
	cl = ClientForString( str );
	if ( !cl ) {
		return;
	}

	// set the team
	trap_Argv( 2, str, sizeof( str ) );
	SetTeam( &g_entities[cl - level.clients], str );
}

void Svcmd_ResetFlags_f(){
	gentity_t*	ent;
	int i;

	for (i = 0; i < 32; ++i ){
		ent = g_entities+i;
		
		if (!ent->inuse || !ent->client )
			continue;

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

void Svcmd_Cointoss_f(void) 
{
	int cointoss = rand() % 2;

	trap_SendServerCommand(-1, va("cp \""S_COLOR_YELLOW"%s"S_COLOR_WHITE"!\"", cointoss ? "Heads" : "Tails"));
	trap_SendServerCommand(-1, va("print \"Coin Toss result: "S_COLOR_YELLOW"%s\n\"", cointoss ? "Heads" : "Tails"));
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

void Svcmd_MapRandom_f()
{
	char pool[64];

	if (trap_Argc() < 1)
	{
		return;
	}       

    trap_Argv( 1, pool, sizeof( pool ) );
    static char currentMap[MAX_MAP_NAME];
    trap_Cvar_VariableStringBuffer( "mapname", currentMap, sizeof( currentMap ) );
    MapInfo mapInfo;

    if ( G_CfgDbSelectMapFromPool( pool, currentMap, &mapInfo ) )
    {
        trap_SendConsoleCommand( EXEC_APPEND, va( "map %s\n", mapInfo.mapname ) );
        return;
    }

    G_Printf( "Map pool '%s' not found\n", pool );
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

void Svcmd_PoolCreate_f()
{
    char short_name[64];
    char long_name[64];      

    if ( trap_Argc() < 3 )
    {
        return;
    }

    trap_Argv( 1, short_name, sizeof( short_name ) );
    trap_Argv( 2, long_name, sizeof( long_name ) );

    if ( !G_CfgDbPoolCreate( short_name, long_name ) )
    {
        G_Printf( "Could not create pool '%s'.\n", short_name );
    }
}

void Svcmd_PoolDelete_f()
{
    char short_name[64];

    if ( trap_Argc() < 2 )
    {
        return;
    }

    trap_Argv( 1, short_name, sizeof( short_name ) );

    if ( !G_CfgDbPoolDelete( short_name ))
    {
        G_Printf( "Failed to delete pool '%s'.\n", short_name );
    }


}

void Svcmd_PoolMapAdd_f()
{
    char short_name[64];
    char mapname[64];
    int  weight;

    if ( trap_Argc() < 3 )
    {
        return;
    }

    trap_Argv( 1, short_name, sizeof( short_name ) );
    trap_Argv( 2, mapname, sizeof( mapname ) );

    if ( trap_Argc() > 3 )
    {
        char weightStr[16];
        trap_Argv( 3, weightStr, sizeof( weightStr ) );

        weight = atoi( weightStr );
    }
    else
    {
        weight = 1;
    } 

    if ( !G_CfgDbPoolMapAdd( short_name, mapname, weight ) )
    {
        G_Printf( "Could not add map to pool '%s'.\n", short_name );
    }  
}

void Svcmd_PoolMapRemove_f()
{
    char short_name[64];
    char mapname[64];

    if ( trap_Argc() < 3 )
    {
        return;
    } 

    trap_Argv( 1, short_name, sizeof( short_name ) );
    trap_Argv( 2, mapname, sizeof( mapname ) );

    if ( !G_CfgDbPoolMapRemove( short_name, mapname ) )
    {
        G_Printf( "Could not remove map from pool '%s'.\n", short_name );
    }

}

char	*ConcatArgs( int start );

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

    if ( Q_stricmp( cmd, "pool_create" ) == 0 )
    {
        Svcmd_PoolCreate_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "pool_delete" ) == 0 )
    {
        Svcmd_PoolDelete_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "pool_map_add" ) == 0 )
    {
        Svcmd_PoolMapAdd_f();
        return qtrue;
    }

    if ( Q_stricmp( cmd, "pool_map_remove" ) == 0 )
    {
        Svcmd_PoolMapRemove_f();
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
		trap_SendServerCommand( -1,  va("print \"Match forced to end.\n\""));
		LogExit( "Match forced to end." );
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
	


	//if (Q_stricmp (cmd, "accountlistall") == 0) {
	//	Svcmd_AccountPrintAll_f();
	//	return qtrue;
	//}	
	
	

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

