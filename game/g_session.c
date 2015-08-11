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
			BroadcastTeamChange( client, -1 );
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
				BroadcastTeamChange( client, -1 );
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
	sess->saberType[0] = 0;
	sess->saber2Type[0] = 0;
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

    FILE* sessionFile = fopen( "session.dat", "wb" );

    if ( !sessionFile )
    {
        return;
    }

    int i;
    for ( i = 0; i < level.maxclients; ++i )
    {
        gclient_t *client = g_entities[i].client;

        int			j = 0;
        char		siegeClass[64];
        char		saberType[64];
        char		saber2Type[64];

        strcpy( siegeClass, client->sess.siegeClass );

        while ( siegeClass[j] )
        { //sort of a hack.. we don't want spaces by siege class names have spaces so convert them all to unused chars
            if ( siegeClass[j] == ' ' )
            {
                siegeClass[j] = 1;
            }

            j++;
        }

        if ( !siegeClass[0] )
        { //make sure there's at least something
            strcpy( siegeClass, "none" );
        }

        //Do the same for the saber
        strcpy( saberType, client->sess.saberType );

        j = 0;
        while ( saberType[j] )
        {
            if ( saberType[j] == ' ' )
            {
                saberType[j] = 1;
            }

            j++;
        }

        strcpy( saber2Type, client->sess.saber2Type );

        j = 0;
        while ( saber2Type[j] )
        {
            if ( saber2Type[j] == ' ' )
            {
                saber2Type[j] = 1;
            }

            j++;
        }

        fwrite( &client->sess, 1, sizeof( client->sess ), sessionFile );
    }

    fclose( sessionFile );
}  

void G_ReadSessionData()
{
    static char buffer[8192];
    FILE* sessionFile = fopen( "session.dat", "rb" );

    if ( !sessionFile )
    {
        level.newSession = qtrue;
        return;
    }

    int fileSize = fread( buffer, 1, sizeof( buffer ), sessionFile );

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
            int			j = 0;

            memcpy( &client->sess, &buffer[i * sizeof( clientSession_t )], sizeof( client->sess ) );

            while ( client->sess.siegeClass[j] )
            { //convert back to spaces from unused chars, as session data is written that way.
                if ( client->sess.siegeClass[j] == 1 )
                {
                    client->sess.siegeClass[j] = ' ';
                }

                j++;
            }

            j = 0;
            //And do the same for the saber type
            while ( client->sess.saberType[j] )
            {
                if ( client->sess.saberType[j] == 1 )
                {
                    client->sess.saberType[j] = ' ';
                }

                j++;
            }

            j = 0;
            while ( client->sess.saber2Type[j] )
            {
                if ( client->sess.saber2Type[j] == 1 )
                {
                    client->sess.saber2Type[j] = ' ';
                }

                j++;
            }

            client->ps.fd.saberAnimLevel = client->sess.saberLevel;
            client->ps.fd.saberDrawAnimLevel = client->sess.saberLevel;
            client->ps.fd.forcePowerSelected = client->sess.selectedFP;
        }
    }

    fclose( sessionFile );
}
