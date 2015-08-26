// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"

//#include "accounts.h"


/*
=======================================================================

  SESSION DATA

Session data is the only data that stays persistant across level loads
and tournament restarts.
=======================================================================
*/

/*
================
G_InitSessionData

Called on a first-time connect
================
*/
extern int getGlobalTime();
void G_InitSessionData( gclient_t *client, char *userinfo, qboolean isBot, qboolean firstTime ) {
	clientSession_t	*sess;
	const char		*value;

	sess = &client->sess;

	client->sess.siegeDesiredTeam = TEAM_FREE;

	// initial team determination
	if ( g_gametype.integer >= GT_TEAM ) {
		if ( g_teamAutoJoin.integer ) {
			sess->sessionTeam = PickTeam( -1 );
		} else {
			// always spawn as spectator in team games
			if (!isBot)
			{
				sess->sessionTeam = TEAM_SPECTATOR;	
			}
			else
			{ //Bots choose their team on creation
				value = Info_ValueForKey( userinfo, "team" );
				if (value[0] == 'r' || value[0] == 'R')
				{
					sess->sessionTeam = TEAM_RED;
				}
				else if (value[0] == 'b' || value[0] == 'B')
				{
					sess->sessionTeam = TEAM_BLUE;
				}
				else
				{
					sess->sessionTeam = PickTeam( -1 );
				}
			}
		}
	} else {
		value = Info_ValueForKey( userinfo, "team" );
		if ( value[0] == 's' ) {
			// a willing spectator, not a waiting-in-line
			sess->sessionTeam = TEAM_SPECTATOR;
		} else {
			switch ( g_gametype.integer ) {
			default:
			case GT_FFA:
			case GT_HOLOCRON:
			case GT_JEDIMASTER:
			case GT_SINGLE_PLAYER:
				if ( g_maxGameClients.integer > 0 && 
					level.numNonSpectatorClients >= g_maxGameClients.integer ) {
					sess->sessionTeam = TEAM_SPECTATOR;
				} else {
					sess->sessionTeam = TEAM_FREE;
				}
				break;
			case GT_DUEL:
				// if the game is full, go into a waiting mode
				if ( level.numNonSpectatorClients >= 2 ) {
					sess->sessionTeam = TEAM_SPECTATOR;
				} else {
					sess->sessionTeam = TEAM_FREE;
				}
				break;
			case GT_POWERDUEL:
				{
					int loners = 0;
					int doubles = 0;

					G_PowerDuelCount(&loners, &doubles, qtrue);

					if (!doubles || loners > (doubles/2))
					{
						sess->duelTeam = DUELTEAM_DOUBLE;
					}
					else
					{
						sess->duelTeam = DUELTEAM_LONE;
					}
				}
				sess->sessionTeam = TEAM_SPECTATOR;
				break;
			}
		}
	}

	if (firstTime){
		Q_strncpyz(sess->ipString, Info_ValueForKey( userinfo, "ip" ) , sizeof(sess->ipString));
	}

    if ( !getIpPortFromString( sess->ipString, &(sess->ip), &(sess->port) ) )
    {
        sess->ip = 0;
        sess->port = 0;
    }

    sess->nameChangeTime = getGlobalTime();

	// accounts system
	//if (isDBLoaded && !isBot){
	//	username = Info_ValueForKey(userinfo, "password");
	//	delimitator = strchr(username,':');
	//	if (delimitator){
	//		*delimitator = '\0'; //seperate username and password
	//		Q_strncpyz(client->sess.username,username,sizeof(client->sess.username));
	//	} else {
	//		client->sess.username[0] = '\0';
	//	}
	//} else 
	{
		client->sess.username[0] = '\0';
	}

	sess->isInkognito = qfalse;
	sess->ignoreFlags = 0;

	sess->spectatorState = SPECTATOR_FREE;
	sess->spectatorTime = level.time;
    sess->inactivityTime = getGlobalTime() + 1000 * g_spectatorInactivity.integer;

	sess->siegeClass[0] = 0;

	Q_strncpyz( sess->saberType, Info_ValueForKey( userinfo, "saber1" ), sizeof( sess->saberType ) );
	Q_strncpyz( sess->saber2Type, Info_ValueForKey( userinfo, "saber2" ), sizeof( sess->saber2Type ) );
}


/*
==================
G_InitWorldSession

==================
*/
void G_InitWorldSession( void ) {
	char	s[MAX_STRING_CHARS];
	int			gt;

    level.newSession = qfalse;

	trap_Cvar_VariableStringBuffer( "session", s, sizeof(s) );
	gt = atoi( s );
	
	// if the gametype changed since the last session, don't use any
	// client sessions
	if ( g_gametype.integer != gt ) {
		level.newSession = qtrue;
		G_Printf( "Gametype changed, clearing session data.\n" );
	}
}

/*
==================
G_WriteSessionData

==================
*/
void G_WriteSessionData( void )
{
    trap_Cvar_Set( "session", va( "%i", g_gametype.integer ) );

    fileHandle_t sessionFile;
    trap_FS_FOpenFile( "session.dat", &sessionFile, FS_WRITE );

    if ( !sessionFile )
    {
        return;
    }

    int i;
    for ( i = 0; i < level.maxclients; ++i )
    {
        gclient_t *client = g_entities[i].client;
        trap_FS_Write( &client->sess, sizeof( client->sess), sessionFile );
    }

    trap_FS_FCloseFile( sessionFile );
}  

void G_ReadSessionData()
{
    fileHandle_t sessionFile;
    int fileSize = trap_FS_FOpenFile( "session.dat", &sessionFile, FS_READ );

    if ( !sessionFile )
    {
        level.newSession = qtrue;
        return;
    }

    if ( fileSize != level.maxclients*sizeof( clientSession_t ) )
    {
        level.newSession = qtrue;
        return;
    }           

    int i;
    for ( i = 0; i < level.maxclients; ++i )
    {
        gclient_t *client = g_entities[i].client;

        if ( client )
        {
            trap_FS_Read( &client->sess, sizeof( clientSession_t ), sessionFile );
  
            client->ps.fd.saberAnimLevel = client->sess.saberLevel;
            client->ps.fd.saberDrawAnimLevel = client->sess.saberLevel;
            client->ps.fd.forcePowerSelected = client->sess.selectedFP;
        }
    }

    trap_FS_FCloseFile( sessionFile );
}
