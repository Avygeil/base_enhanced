// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"
#include "bg_saga.h"
#include "g_database.h"

#include "kdtree.h"
#include "cJSON.h"

#include "menudef.h"			// for the voice chats

//rww - for getting bot commands...
int AcceptBotCommand(char *cmd, gentity_t *pl);
//end rww

#include "namespace_begin.h"
void WP_SetSaber( int entNum, saberInfo_t *sabers, int saberNum, const char *saberName );
#include "namespace_end.h"

void Cmd_NPC_f( gentity_t *ent );
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin);

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage( gentity_t *ent ) {
	char		entry[1024];
	char		string[1400];
	int			stringlength;
	int			i, j;
	gclient_t	*cl;
	int			numSorted, accuracy;
	int         statsMix;

	// send the latest information on all clients
	string[0] = 0;
	stringlength = 0;

	numSorted = level.numConnectedClients;
	
	if (numSorted > MAX_CLIENT_SCORE_SEND)
	{
		numSorted = MAX_CLIENT_SCORE_SEND;
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->pers.connected == CON_CONNECTING ) {
			ping = -1;
		} else {
			//test


			ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

			if (ping == 0 && !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT))
				ping = 1;

		}

		if( cl->stats && cl->stats->accuracy_shots ) {
			accuracy = cl->stats->accuracy_hits * 100 / cl->stats->accuracy_shots;
		}
		else {
			accuracy = 0;
		}

		//lower 16 bits say average return time
		//higher 16 bits say how many times did player get the flag
		statsMix = cl->stats ? cl->stats->healed : 0;
		statsMix |= ((cl->stats ? cl->stats->energizedAlly : 0) << 16);

		// first 16 bits are unused
		// next 10 bits are real ping
		// next 5 bits are followed client #
		// final bit is whether you are following someone or not
		int specMix = 0; // this was previously scoreFlags
		if (cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam == TEAM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW &&
			!cl->sess.isInkognito) {
			specMix = 0b1;
			specMix |= ((cl->sess.spectatorClient & 0b11111) << 1);
		}
		int realPing;
		if (cl->pers.connected != CON_CONNECTED)
			realPing = -1;
		else if (cl->isLagging)
			realPing = 999;
		else
			realPing = Com_Clampi(-1, 999, cl->realPing);
		specMix |= ((realPing & 0b1111111111) << 6);

		int p = g_entities[level.sortedClients[i]].s.powerups;

		if (g_teamOverlayForceAlignment.integer && g_gametype.integer != GT_SIEGE) {
			// never send enemy force alignment
			qboolean sendForceAlignment = qtrue;
			if (g_gametype.integer >= GT_TEAM) {
				if (ent && ent->client && ent->client->sess.sessionTeam == TEAM_RED && cl->sess.sessionTeam == TEAM_BLUE)
					sendForceAlignment = qfalse;
				else if (ent && ent->client && ent->client->sess.sessionTeam == TEAM_BLUE && cl->sess.sessionTeam == TEAM_RED)
					sendForceAlignment = qfalse;
			}
			else {
				if (ent && ent->client && ent->client->sess.sessionTeam == TEAM_FREE && cl->sess.sessionTeam == TEAM_FREE)
					sendForceAlignment = qfalse;
			}

			if (sendForceAlignment) {
				if (g_entities[level.sortedClients[i]].health > 0 && cl->ps.fd.forcePowersKnown) {
					if (cl->ps.fd.forceSide == FORCE_LIGHTSIDE) {
						p |= (1 << PW_FORCE_ENLIGHTENED_LIGHT);
						p &= ~(1 << PW_FORCE_ENLIGHTENED_DARK);
					}
					else if (cl->ps.fd.forceSide == FORCE_DARKSIDE) {
						p |= (1 << PW_FORCE_ENLIGHTENED_DARK);
						p &= ~(1 << PW_FORCE_ENLIGHTENED_LIGHT);
					}
					else {
						p &= ~(1 << PW_FORCE_ENLIGHTENED_LIGHT);
						p &= ~(1 << PW_FORCE_ENLIGHTENED_DARK);
					}
				}
				else {
					p &= ~(1 << PW_FORCE_ENLIGHTENED_LIGHT);
					p &= ~(1 << PW_FORCE_ENLIGHTENED_DARK);
				}
			}
		}

		Com_sprintf (entry, sizeof(entry),
			" %i %i %i %i %i %i %i %i %i %i %i %i %i %i", level.sortedClients[i],
			cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime)/60000,
			specMix, p, accuracy, 
			
			cl->stats ? cl->stats->fcKills : 0, //this can be replaced
			                                       //but only here!
												   //server uses this value
			/*
			//sending number where
			//lower 16 bits say average return time
			//higher 16 bits say how many times did player get the flag

			statsMix,
			*/

			cl->stats ? cl->stats->totalFlagHold : 0,
			cl->stats ? cl->stats->rets : 0, 
			cl->ps.persistant[PERS_DEFEND_COUNT], 
			cl->ps.persistant[PERS_ASSIST_COUNT], 

			statsMix,
			/*
			perfect, //this can be replaced
			         //but how can we manipulate with it on client side?
			*/
			cl->ps.persistant[PERS_CAPTURES]);
		j = strlen(entry);
		if (stringlength + j > 1022)
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	//still want to know the total # of clients
	i = level.numConnectedClients;

	trap_SendServerCommand( ent-g_entities, va("scores %i %i %i%s", i, 
		level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
		string ) );
}


/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f( gentity_t *ent ) {
	DeathmatchScoreboardMessage( ent );
}

typedef struct version_s {
	int		major;
	int		minor;
	int		revision;
} version_t;

static void VersionStringToNumbers(const char *s, version_t *out) {
	assert(VALIDSTRING(s) && out);
	out->major = out->minor = out->revision = 0;

	if (!VALIDSTRING(s) || *s < '0' || *s > '9' || strlen(s) >= 16)
		return;

	char *r = (char *)s;
	for (int i = 0; i < 3; i++) {
		char temp[16] = { 0 }, *w;
		for (w = temp; *r && *r >= '0' && *r <= '9'; r++, w++) // loop through all number chars, copying to the temporary buffer
			*w = *r;
		switch (i) { // set the number
		case 0:		out->major = atoi(temp);		break;
		case 1:		out->minor = atoi(temp);		break;
		case 2:		out->revision = atoi(temp);		break;
		}
		while (*r < '0' || *r > '9') {
			if (!*r)
				return;
			r++;
		}
	}
}

// returns 1 if verStr is greater, 0 if same, -1 if verStr is older
int CompareVersions(const char *verStr, const char *compareToStr) {
	if (!VALIDSTRING(verStr) && !VALIDSTRING(compareToStr))
		return 0;
	if (!VALIDSTRING(verStr))
		return -1;
	if (!VALIDSTRING(compareToStr))
		return 1;

	version_t ver, compareTo;
	VersionStringToNumbers(verStr, &ver);
	VersionStringToNumbers(compareToStr, &compareTo);

	if (ver.major > compareTo.major)
		return 1;
	else if (ver.major < compareTo.major)
		return -1;

	// same major
	if (ver.minor > compareTo.minor)
		return 1;
	else if (ver.minor < compareTo.minor)
		return -1;

	// same major and same minor
	if (ver.revision > compareTo.revision)
		return 1;
	else if (ver.revision < compareTo.revision)
		return -1;

	// equal
	return 0;
}

void Cmd_Unlagged_f(gentity_t *ent) {
	if (!g_unlagged.integer) {
		trap_SendServerCommand(ent - g_entities, "print \"This server has completely disabled unlagged support.\n\"");
		return;
	}

	// only allow the command to be used by the unascended
	if (ent->client->sess.auth != INVALID && CompareVersions(ent->client->sess.nmVer, "1.5.6") >= 0) {
		trap_SendServerCommand(ent - g_entities, "print \"Invalid command. You must use the ^5cg_unlagged^7 cvar instead.\n\"");
		return;
	}

	if (g_unlagged.integer == 2) { // special testing mode, don't let people use it with the command
		trap_SendServerCommand(ent - g_entities, "print \"Invalid command.\n\"");
		return;
	}

	ent->client->sess.unlagged ^= UNLAGGED_COMMAND;

	if (ent->client->sess.unlagged & UNLAGGED_COMMAND) {
		char msg[MAX_STRING_CHARS] = "print \"Unlagged ^2enabled^7.";
		if (!ent->client->sess.basementNeckbeardsTriggered) {
			if (ent->client->sess.auth != INVALID)
				Q_strcat(msg, sizeof(msg), " For the best unlagged experience, update to Newmod 1.6.0 and enable cg_unlagged.");
			else
				Q_strcat(msg, sizeof(msg), " For the best unlagged experience, use Newmod and enable cg_unlagged.");
			ent->client->sess.basementNeckbeardsTriggered = qtrue;
		}
		Q_strcat(msg, sizeof(msg), "\n\"");
		trap_SendServerCommand(ent - g_entities, msg);
		Com_Printf("Client %d (%s^7) enabled unlagged\n", ent - g_entities, ent->client->pers.netname);
	}
	else {
		trap_SendServerCommand(ent - g_entities, "print \"Unlagged ^1disabled^7.\n\"");
		Com_Printf("Client %d (%s^7) disabled unlagged\n", ent - g_entities, ent->client->pers.netname);
	}
}

/*
==================
CheatsOk
==================
*/
qboolean	CheatsOk( gentity_t *ent ) {
	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return qfalse;
	}
	if ( ent->health <= 0 ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEALIVE")));
		return qfalse;
	}
	return qtrue;
}


/*
==================
ConcatArgs
==================
*/
char	*ConcatArgs( int start ) {
	int		i, c, tlen;
	static char	line[MAX_STRING_CHARS];
	int		len;
	char	arg[MAX_STRING_CHARS];

	len = 0;
	c = trap_Argc();
	for ( i = start ; i < c ; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );
		tlen = strlen( arg );
		if ( len + tlen >= MAX_STRING_CHARS - 1 ) {
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;
		if ( i != c - 1 ) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}

/*
==================
SanitizeString

Remove case and control characters
==================
*/
void SanitizeString( char *in, char *out ) {
	while ( *in ) {
		if ( *in == 27 ) {
			in += 2;		// skip color code
			continue;
		}
		if ( *in < 32 ) {
			in++;
			continue;
		}
		*out++ = tolower( (unsigned char) *in++ );
	}

	*out = 0;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString( gentity_t *to, char *s ) {
	gclient_t	*cl;
	int			idnum;
	char		s2[MAX_STRING_CHARS];
	char		n2[MAX_STRING_CHARS];

	// numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9') {
		idnum = atoi( s );
		if ( idnum < 0 || idnum >= level.maxclients ) {
			trap_SendServerCommand( to-g_entities, va("print \"Bad client slot: %i\n\"", idnum));
			return -1;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected != CON_CONNECTED ) {
			trap_SendServerCommand( to-g_entities, va("print \"Client %i is not active\n\"", idnum));
			return -1;
		}
		return idnum;
	}

	// check for a name match
	SanitizeString( s, s2 );
	for ( idnum=0,cl=level.clients ; idnum < level.maxclients ; idnum++,cl++ ) {
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		SanitizeString( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) {
			return idnum;
		}
	}

	trap_SendServerCommand( to-g_entities, va("print \"User %s is not on the server\n\"", s));
	return -1;
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f (gentity_t *cmdent, int baseArg)
{
	char		name[MAX_TOKEN_CHARS];
	gentity_t	*ent;
	gitem_t		*it;
	int			i;
	qboolean	give_all;
	gentity_t		*it_ent;
	trace_t		trace;
	char		arg[MAX_TOKEN_CHARS];

	if ( !CheatsOk( cmdent ) ) {
		return;
	}

	if (baseArg)
	{
		char otherindex[MAX_TOKEN_CHARS];

		trap_Argv( 1, otherindex, sizeof( otherindex ) );

		if (!otherindex[0])
		{
			Com_Printf("giveother requires that the second argument be a client index number.\n");
			return;
		}

		i = atoi(otherindex);

		if (i < 0 || i >= MAX_CLIENTS)
		{
			Com_Printf("%i is not a client index\n", i);
			return;
		}

		ent = &g_entities[i];

		if (!ent->inuse || !ent->client)
		{
			Com_Printf("%i is not an active client\n", i);
			return;
		}
	}
	else
	{
		ent = cmdent;
	}

	trap_Argv( 1+baseArg, name, sizeof( name ) );

	if (Q_stricmp(name, "all") == 0)
		give_all = qtrue;
	else
		give_all = qfalse;

	if (give_all)
	{
		i = 0;
		while (i < HI_NUM_HOLDABLE)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << i);
			i++;
		}
		i = 0;
	}

	if (give_all || Q_stricmp( name, "health") == 0)
	{
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			ent->health = atoi(arg);
			if (ent->health > ent->client->ps.stats[STAT_MAX_HEALTH]) {
				ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
			}
		}
		else {
			ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "force") == 0)
	{
		if (trap_Argc() == 3 + baseArg) {
			trap_Argv(2 + baseArg, arg, sizeof(arg));
			ent->client->ps.fd.forcePower = Com_Clampi(0, 999, atoi(arg));
		}
		else {
			ent->client->ps.fd.forcePower = ent->client->ps.fd.forcePowerMax;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		ent->client->ps.stats[STAT_WEAPONS] = (1 << (LAST_USEABLE_WEAPON+1))  - ( 1 << WP_NONE );
		if (!give_all)
			return;
	}
	
	if ( !give_all && Q_stricmp(name, "weaponnum") == 0 )
	{
		trap_Argv( 2+baseArg, arg, sizeof( arg ) );
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << atoi(arg));
		return;
	}

	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		int num = 999;
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			num = atoi(arg);
		}
		for ( i = 0 ; i < MAX_WEAPONS ; i++ ) {
			ent->client->ps.ammo[i] = num;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		if (trap_Argc() == 3+baseArg) {
			trap_Argv( 2+baseArg, arg, sizeof( arg ) );
			ent->client->ps.stats[STAT_ARMOR] = atoi(arg);
		} else {
			ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];
		}

		if (!give_all)
			return;
	}

	// spawn a specific item right on the player
	if ( !give_all ) {
		it = BG_FindItem (name);
		if (!it) {
			return;
		}

		it_ent = G_Spawn();
		VectorCopy( ent->r.currentOrigin, it_ent->s.origin );
		it_ent->classname = it->classname;
		G_SpawnItem (it_ent, it);
		FinishSpawningItem(it_ent );
		memset( &trace, 0, sizeof( trace ) );
		Touch_Item (it_ent, ent, &trace);
		if (it_ent->inuse) {
			G_FreeEntity( it_ent );
		}
	}

}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f (gentity_t *ent)
{
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent ) {
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent ) {
	char	*msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	if ( ent->client->noclip ) {
		msg = "noclip OFF\n";
	} else {
		msg = "noclip ON\n";
	}
	ent->client->noclip = !ent->client->noclip;

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent ) {
	if ( !CheatsOk( ent ) ) {
		return;
	}

	// doesn't work in single player
	if ( g_gametype.integer != 0 ) {
		trap_SendServerCommand( ent-g_entities, 
			"print \"Must be in g_gametype 0 for levelshot\n\"" );
		return;
	}

	BeginIntermission();
	trap_SendServerCommand( ent-g_entities, "clientLevelShot" );
}


/*
==================
Cmd_TeamTask_f

From TA.
==================
*/
void Cmd_TeamTask_f( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING];
	char		arg[MAX_TOKEN_CHARS];
	int task;
	int client = ent->client - level.clients;

	if ( trap_Argc() != 2 ) {
		return;
	}
	trap_Argv( 1, arg, sizeof( arg ) );
	task = atoi( arg );

	trap_GetUserinfo(client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "teamtask", va("%d", task));
	trap_SetUserinfo(client, userinfo);
	ClientUserinfoChanged(client);
}

extern qboolean isRedFlagstand(gentity_t *ent);
extern qboolean isBlueFlagstand(gentity_t *ent);

extern qboolean IsDroppedRedFlag(gentity_t *ent);
extern qboolean IsDroppedBlueFlag(gentity_t *ent);

// doesn't check for hp, fp of either player, same team, etc; just distance and PVS
qboolean InRangeAndPVSForTE(gentity_t *energizer, gentity_t *energizee) {
	if (!energizer || !energizer->client || !energizee || !energizee->client) {
		assert(qfalse);
		return qfalse;
	}

	float radius;
	switch (energizer->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE]) {
	case 1: radius = 256; break;
	case 2: radius = 256 * 1.5; break;
	case 3: radius = 256 * 2; break;
	default: return qfalse;
	}

	if (Distance(energizer->client->ps.origin, energizee->client->ps.origin) > radius)
		return qfalse;

	if (!trap_InPVS(energizer->client->ps.origin, energizee->client->ps.origin))
		return qfalse;

	return qtrue;
}

qboolean IsUsingForcePowerThatBlocksForceRegen(gentity_t *ent) {
	if (!ent || !ent->client) {
		assert(qfalse);
		return qfalse;
	}

	if (ent->client->ps.fd.forcePowersActive & (1 << FP_SPEED))
		return qtrue;
	if (ent->client->ps.fd.forcePowersActive & (1 << FP_RAGE))
		return qtrue;
	if (ent->client->ps.fd.forcePowersActive & (1 << FP_ABSORB))
		return qtrue;
	if (ent->client->ps.fd.forcePowersActive & (1 << FP_PROTECT))
		return qtrue;
	if (ent->client->ps.fd.forcePowersActive & (1 << FP_SEE))
		return qtrue;
	if (ent->client->ps.fd.forcePowersActive & (1 << FP_LIGHTNING))
		return qtrue;
	if (ent->client->ps.fd.forcePowersActive & (1 << FP_TELEPATHY))
		return qtrue;
	/*if (ent->client->ps.fd.forcePowersActive & (1 << FP_DRAIN))
		return qtrue;*/

	// doesn't check for bullshit like ysalimari, holding korriban crystals, etc

	return qfalse;
}

/*
=================
Cmd_Kill_f
=================
*/

void Cmd_Kill_f( gentity_t *ent ) {
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}
	if (ent->health <= 0) {
		return;
	}

    if ( ent->client->tempSpectate > level.time )
    {
        return;
    }

	if (ent->client->rockPaperScissorsBothChosenTime)
		return;

    //OSP: pause
    if ( level.pause.state != PAUSE_NONE && !ent->client->sess.inRacemode )
            return;

	// racemode - delete all fired projectiles
	if ( ent->client && ent->client->sess.inRacemode )
		G_DeletePlayerProjectiles( ent );

	if ((g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) &&
		level.numPlayingClients > 1 && !level.warmupTime)
	{
		if (!g_allowDuelSuicide.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "ATTEMPTDUELKILL")) );
			return;
		}
	}

	// don't allow boosted dude to sk with the flag in some cases
	if (g_gametype.integer == GT_CTF && ent->client && ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_BOOST_SELFKILLBOOST) &&
		!IsRacerOrSpectator(ent) && g_boost.integer && level.wasRestarted) {

		static qboolean flagstandsInitialized = qfalse, flagstandsValid = qfalse;
		static vec3_t redFs, blueFs;
		if (!flagstandsInitialized) {
			flagstandsInitialized = qtrue;
			gentity_t temp;
			VectorCopy(vec3_origin, temp.r.currentOrigin);
			gentity_t *redFsEnt = G_ClosestEntity(&temp, isRedFlagstand);
			gentity_t *blueFsEnt = G_ClosestEntity(&temp, isBlueFlagstand);
			if (redFsEnt && blueFsEnt) {
				flagstandsValid = qtrue;
				VectorCopy(redFsEnt->r.currentOrigin, redFs);
				VectorCopy(blueFsEnt->r.currentOrigin, blueFs);
			}
		}

		if (flagstandsValid) {
			if (HasFlag(ent)) {
				vec3_t enemyFs;
				if (ent->client->sess.sessionTeam == TEAM_RED)
					VectorCopy(blueFs, enemyFs);
				else if (ent->client->sess.sessionTeam == TEAM_BLUE)
					VectorCopy(redFs, enemyFs);
				else
					assert(qfalse); // should never happen due to IsRacerOrSpectator call above

				float closestAllyDistanceToEnemyFs = 999999, closestEnemyDistanceToEnemyFs = 999999;
				gentity_t *closestAllyToEnemyFs = NULL;
				float closestAllyDistanceToMe = 999999;
				gentity_t *closestAllyToMe = NULL;
				float enemyFcDistanceToEnemyFs = 999999;
				gentity_t *enemyFc = NULL;
				for (int i = 0; i < MAX_CLIENTS; i++) {
					gentity_t *otherGuy = &g_entities[i];
					if (!otherGuy->inuse || !otherGuy->client || otherGuy->client->pers.connected != CON_CONNECTED || otherGuy == ent || IsRacerOrSpectator(otherGuy) || otherGuy->health <= 0 || otherGuy->client->ps.fallingToDeath)
						continue;

					float otherGuyDistToEnemyFs = Distance(otherGuy->r.currentOrigin, enemyFs);
					if (otherGuy->client->sess.sessionTeam == ent->client->sess.sessionTeam) {
						if (otherGuyDistToEnemyFs < closestAllyDistanceToEnemyFs) {
							closestAllyDistanceToEnemyFs = otherGuyDistToEnemyFs;
							closestAllyToEnemyFs = otherGuy;
						}

						float otherGuyDistToMe = Distance(otherGuy->r.currentOrigin, ent->r.currentOrigin);
						if (otherGuyDistToMe < closestAllyDistanceToMe) {
							closestAllyDistanceToMe = otherGuyDistToMe;
							closestAllyToMe = otherGuy;
						}
					}
					else if (otherGuy->client->sess.sessionTeam == OtherTeam(ent->client->sess.sessionTeam)) {
						if (otherGuyDistToEnemyFs < closestEnemyDistanceToEnemyFs)
							closestEnemyDistanceToEnemyFs = otherGuyDistToEnemyFs;

						if (HasFlag(otherGuy)) {
							enemyFcDistanceToEnemyFs = otherGuyDistToEnemyFs;
							enemyFc = otherGuy;
						}
					}
				}

				/*if (closestAllyDistanceToMe <= 200 && closestAllyToMe && closestAllyToMe->health >= 40) {
					// 40hp+ ally seems to be trying to take from me
					// block the sk
					return;
				}
				else */if (enemyFcDistanceToEnemyFs <= 300 && (enemyFcDistanceToEnemyFs <= closestAllyDistanceToEnemyFs + 150 || (closestAllyToEnemyFs && closestAllyToEnemyFs->health < 20))) {
					// enemy fc is on their fs ready to cap and no 20hp+ ally is closer to their fs by a decent distance
					// block the sk
					return;
				}
				else if (closestAllyDistanceToEnemyFs <= 250 && closestEnemyDistanceToEnemyFs >= 1200 && closestAllyToEnemyFs && closestAllyToEnemyFs->health >= 60) {
					// 60hp+ camper has freerun
					// allow the sk
				}
				else if (GetCTFLocationValue(ent) <= 0.4f) {
					// i'm in my base
					// block the sk
					return;
				}
				else {
					// otherwise allow the sk
				}
			}
			
			if (!ent->client->ps.fallingToDeath) {
				// sigh...check for certain situations where he shouldn't sk despite not having the flag

				float distanceToClosestDroppedFlag = 999999;
				for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
					gentity_t *thisEnt = &g_entities[i];
					if (!IsDroppedRedFlag(thisEnt) && !IsDroppedBlueFlag(thisEnt))
						continue;

					float dist = Distance(thisEnt->r.currentOrigin, ent->client->ps.origin);
					if (dist < distanceToClosestDroppedFlag)
						distanceToClosestDroppedFlag = dist;
				}

				if (distanceToClosestDroppedFlag <= 400) {
					// i'm close to a dropped flag (of either team)
					return;
				}

				if (!HasFlag(ent) && ent->client->fakeForceAlignment != FAKEFORCEALIGNMENT_LIGHT &&
					ent->client->account->flags & ACCOUNTFLAG_BOOST_BASEAUTOTHTEBOOST && GetRemindedPosOrDeterminedPos(ent) == CTFPOSITION_BASE &&
					ent->client->fakeForceAlignment != FAKEFORCEALIGNMENT_LIGHT) {
					gentity_t *myFc = NULL;
					for (int i = 0; i < MAX_CLIENTS; i++) {
						gentity_t *otherGuy = &g_entities[i];
						if (!otherGuy->inuse || !otherGuy->client || otherGuy->client->pers.connected != CON_CONNECTED || otherGuy == ent || IsRacerOrSpectator(otherGuy) || otherGuy->health <= 0 || otherGuy->client->ps.fallingToDeath)
							continue;
						if (!HasFlag(otherGuy))
							continue;
						if (otherGuy->client->sess.sessionTeam != ent->client->sess.sessionTeam)
							continue;
						myFc = otherGuy;
						break;
					}

					if (myFc) {
						const qboolean fcNeedsTe = (myFc->client->ps.fd.forcePower <= 75);
						const int myForce = ent->client->ps.fd.forcePower;
						const qboolean myForceIsRecharging = !IsUsingForcePowerThatBlocksForceRegen(ent);
						const qboolean inTERangeOfMyFc = InRangeAndPVSForTE(ent, myFc);

						// note: assumes 25 force cost to TE
						if (fcNeedsTe && inTERangeOfMyFc && (myForce >= 25 || myForce >= 20 && myForceIsRecharging)) {
							// i'm close to an fc who needs te and i can imminently TE them
							return;
						}
					}
				}
			}
		}
	}

	if (ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_FORCE) && g_allowSkAutoThTe.integer & (1 << 0)) {
		ForceTeamForceReplenish(ent, qfalse);
	}
	else if (ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_HEAL) && g_allowSkAutoThTe.integer & (1 << 1)) {
		if (ent->client->sess.autoThOnSk) {
			ForceTeamHeal(ent, qfalse);
		}
		else if (trap_Argc() >= 2) {
			char arg[MAX_STRING_CHARS] = { 0 };
			trap_Argv(1, arg, sizeof(arg));
			if (arg[0] && !Q_stricmp(arg, "th")) {
				ForceTeamHeal(ent, qfalse);
			}
		}
	}

	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
	player_die (ent, ent, ent, 100000, MOD_SUICIDE);
	ent->client->pers.hasDoneSomething = qtrue;
}

gentity_t *G_GetDuelWinner(gclient_t *client)
{
	gclient_t *wCl;
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		wCl = &level.clients[i];
		
		if (wCl && wCl != client && /*wCl->ps.clientNum != client->ps.clientNum &&*/
			wCl->pers.connected == CON_CONNECTED && wCl->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return &g_entities[wCl->ps.clientNum];
		}
	}

	return NULL;
}

/*
=================
BroadCastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange( gclient_t *client, int oldTeam )
{
	char buffer[512];
	client->ps.fd.forceDoInit = 1; //every time we change teams make sure our force powers are set right

	*buffer = '\0';

	if ( client->sess.sessionTeam == TEAM_RED ) {
		Q_strncpyz(buffer, 
			va("%s%s" S_COLOR_WHITE " %s", NM_SerializeUIntToColor(client - level.clients),
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEREDTEAM")),
			sizeof(buffer));

	} else if ( client->sess.sessionTeam == TEAM_BLUE ) {
		Q_strncpyz(buffer, 
			va("%s%s" S_COLOR_WHITE " %s", NM_SerializeUIntToColor(client - level.clients),
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBLUETEAM")),
			sizeof(buffer));

	} else if ( client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		Q_strncpyz(buffer, 
			va("%s%s" S_COLOR_WHITE " %s", NM_SerializeUIntToColor(client - level.clients),
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHESPECTATORS")),
			sizeof(buffer));

	} else if ( client->sess.sessionTeam == TEAM_FREE ) {
		if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
		{
			//NOTE: Just doing a vs. once it counts two players up
		}
		else
		{
			Q_strncpyz(buffer, 
				va("%s%s" S_COLOR_WHITE " %s", NM_SerializeUIntToColor(client - level.clients),
				client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBATTLE")),
				sizeof(buffer));
		}
	}

	if (*buffer){
		trap_SendServerCommand( -1, va("cp \"%s\n\"",buffer) );
		trap_SendServerCommand( -1, va("print \"%s\n\"",buffer) );
	}

	G_LogPrintf ( "setteam:  %i %s %s\n",
				  client - &level.clients[0],
				  TeamName ( oldTeam ),
				  TeamName ( client->sess.sessionTeam ) );
}

qboolean G_PowerDuelCheckFail(gentity_t *ent)
{
	int			loners = 0;
	int			doubles = 0;

	if (!ent->client || ent->client->sess.duelTeam == DUELTEAM_FREE)
	{
		return qtrue;
	}

	G_PowerDuelCount(&loners, &doubles, qfalse);

	if (ent->client->sess.duelTeam == DUELTEAM_LONE && loners >= 1)
	{
		return qtrue;
	}

	if (ent->client->sess.duelTeam == DUELTEAM_DOUBLE && doubles >= 2)
	{
		return qtrue;
	}

	return qfalse;
}

/*
=================
SetTeam
=================
*/
qboolean g_dontPenalizeTeam = qfalse;
qboolean g_preventTeamBegin = qfalse;
void SetTeam( gentity_t *ent, char *s ) {
	int					team, oldTeam;
	gclient_t			*client;
	int					clientNum;
	spectatorState_t	specState;
	int					specClient;
	int					teamLeader;

	//base enhanced fix, sometimes we come here with invalid 
	//entity sloty and this procedure then creates fake player
	if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || !ent->client->stats) {
		return;
	}

	//
	// see what change is requested
	//
	client = ent->client;

	// safety checks
	if ( client->sess.inRacemode ) {
		G_LogPrintf( "WARNING: SetTeam called while in race mode!!!" );
		G_SetRaceMode( ent, qfalse );
	}

	clientNum = client - level.clients;
	specClient = 0;
	specState = SPECTATOR_NOT;
	if ( !Q_stricmp( s, "scoreboard" ) || !Q_stricmp( s, "score" )  ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_SCOREBOARD;
	} else if ( !Q_stricmp( s, "follow1" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -1;
	} else if ( !Q_stricmp( s, "follow2" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -2;
	} else if ( !Q_stricmpn( s, "s", 1 ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE;
	} else if ( g_gametype.integer >= GT_TEAM ) {
		// if running a team game, assign player to one of the teams
		specState = SPECTATOR_NOT;
		if ( !Q_stricmpn( s, "r", 1 ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmpn( s, "b", 1 ) ) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			//For now, don't do this. The legalize function will set powers properly now.
				team = PickTeam( clientNum );
			//}
		}

		if ( g_teamForceBalance.integer && !g_trueJedi.integer ) {
			int		counts[TEAM_NUM_TEAMS];

			counts[TEAM_BLUE] = TeamCount( ent-g_entities, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( ent-g_entities, TEAM_RED );

			// We allow a spread of two
			if ( team == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				{
					trap_SendServerCommand( ent->client->ps.clientNum, 
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED")) );
				}
				return; // ignore the request
			}
			if ( team == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				{
					trap_SendServerCommand( ent->client->ps.clientNum, 
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE")) );
				}
				return; // ignore the request
			}

			// It's ok, the team we are switching to has less or same number of players
		}
	} else {
		// force them to spectators if there aren't any spots free
		team = TEAM_FREE;
	}

	//pending server commands overflow might cause that this
	//client is no longer valid, lets check it for it here and few other places
	if (!ent->inuse){
		return;
	}

	if (g_gametype.integer == GT_SIEGE)
	{
		if (client->tempSpectate >= level.time &&
			team == TEAM_SPECTATOR)
		{ //sorry, can't do that.
			return;
		}

		client->sess.siegeDesiredTeam = team;

		if (client->sess.sessionTeam != TEAM_SPECTATOR &&
			team != TEAM_SPECTATOR)
		{ //not a spectator now, and not switching to spec, so you have to wait til you die.
			//trap_SendServerCommand( ent-g_entities, va("print \"You will be on the selected team the next time you respawn.\n\"") );
			qboolean doBegin;
			if (ent->client->tempSpectate >= level.time)
			{
				doBegin = qfalse;
			}
			else
			{
				doBegin = qtrue;
			}

			if (doBegin)
			{
				// Kill them so they automatically respawn in the team they wanted.
				if (ent->health > 0)
				{
					ent->flags &= ~FL_GODMODE;
					ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
					player_die( ent, ent, ent, 100000, MOD_TEAM_CHANGE ); 
				}
			}

			if (ent->client->sess.sessionTeam != ent->client->sess.siegeDesiredTeam)
			{
				SetTeamQuick(ent, ent->client->sess.siegeDesiredTeam, qfalse);
                BroadcastTeamChange( client, client->sess.sessionTeam );
			}

			return;
		}
	}

	// override decision if limiting the players
	if ( (g_gametype.integer == GT_DUEL)
		&& level.numNonSpectatorClients >= 2 )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( (g_gametype.integer == GT_POWERDUEL)
		&& (level.numPlayingClients >= 3 || G_PowerDuelCheckFail(ent)) )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( g_maxGameClients.integer > 0 && 
		level.numNonSpectatorClients >= g_maxGameClients.integer )
	{
		team = TEAM_SPECTATOR;
	}

	//
	// decide if we will allow the change
	//
	oldTeam = client->sess.sessionTeam;
	if ( team == oldTeam && team != TEAM_SPECTATOR ) {
		return;
	}

	// Only check one way, so you can join spec back if you were forced as a passwordless spectator
	if (team != TEAM_SPECTATOR && !client->sess.canJoin) {
		trap_SendServerCommand( ent - g_entities,
			"cp \"You may not join due to incorrect/missing password\nIf you know the password, just use /password\n\"" );
		trap_SendServerCommand( ent - g_entities,
			"print \"You may not join due to incorrect/missing password\nIf you know the password, just use /password\n\"" );

		return;
	}

	if (client->rockPaperScissorsBothChosenTime)
		return;

	if (client->pers.connected != CON_CONNECTED)
		return;

	//
	// execute the team change
	//

	if (team != TEAM_FREE)
		client->pers.aimPracticePackBeingEdited = NULL;

	// if the player was dead leave the body
	if ( client->ps.stats[STAT_HEALTH] <= 0 && client->sess.sessionTeam != TEAM_SPECTATOR ) {
		MaintainBodyQueue(ent);
	}

	// he starts at 'base'
	client->pers.teamState.state = TEAM_BEGIN;
	if ( oldTeam != TEAM_SPECTATOR ) {
		// Kill him (makes sure he loses flags, etc)
		ent->flags &= ~FL_GODMODE;
		ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
		g_dontPenalizeTeam = qtrue;
		player_die (ent, ent, ent, 100000, MOD_TEAM_CHANGE);
		g_dontPenalizeTeam = qfalse;

	}
	// they go to the end of the line for tournements
	if ( team == TEAM_SPECTATOR ) {
		if ( (g_gametype.integer != GT_DUEL) || (oldTeam != TEAM_SPECTATOR) )	{//so you don't get dropped to the bottom of the queue for changing skins, etc.
			client->sess.spectatorTime = level.time;
		}
	}

	// reset readiness when joining spectators if g_allowReady is 1
	if (g_allowReady.integer == 1 && (team == TEAM_SPECTATOR || team == TEAM_FREE))
		client->pers.ready = qfalse;

	client->sess.sessionTeam = team;
	client->sess.spectatorState = specState;
	client->sess.spectatorClient = specClient;
	if (team == TEAM_RED || team == TEAM_BLUE) { // we are only concerned about which ingame team they were last on
		if ((client->stats->lastTeam == TEAM_RED || client->stats->lastTeam == TEAM_BLUE) && client->stats->lastTeam != team) {
			// they were previously ingame but have changed teams
			// we need to assign them a new stats struct so that their stats are tracked separately per-team
			ChangeStatsTeam(ent->client);
		}

		client->stats->lastTeam = team;
	}

	client->sess.teamLeader = qfalse;
	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		teamLeader = TeamLeader( team );
		// if there is no team leader or the team leader is a bot and this client is not a bot
		if ( teamLeader == -1 || ( !(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT) ) ) {
		}
	}
	// make sure there is a team leader on the team the player came from
	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		CheckTeamLeader( oldTeam );
	}

	if (team == TEAM_SPECTATOR || team == TEAM_FREE || team != oldTeam)
		client->pers.killedAlliedFlagCarrier = qfalse;

	//pending server commands overflow might cause that this
	//client is no longer valid, lets check it for it here and few other places
	if (!ent->inuse){
		return;
	}

	BroadcastTeamChange( client, oldTeam );

	qboolean joinedOrLeftIngameTeam = !!(team == TEAM_RED || team == TEAM_BLUE || oldTeam == TEAM_RED || oldTeam == TEAM_BLUE);
	if (joinedOrLeftIngameTeam && g_gametype.integer == GT_CTF && level.wasRestarted && !level.someoneWasAFK && level.pause.state != PAUSE_NONE)
		ShowSubBalance();

	//make a disappearing effect where they were before teleporting them to the appropriate spawn point,
	//if we were not on the spec team
	if (oldTeam != TEAM_SPECTATOR)
	{
		gentity_t *tent = G_TempEntity( client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = clientNum;
		G_ApplyRaceBroadcastsToEvent( ent, tent );
	}

	// get and distribute relevent paramters
	ClientUserinfoChanged( clientNum );

	//pending server commands overflow might cause that this
	//client is no longer valid, lets check it for it here and few other places
	if (!ent->inuse){
		return;
	}

	if (!g_preventTeamBegin)
	{
		ClientBegin( clientNum, qfalse );
	}

	//vote delay
	if (team != oldTeam && oldTeam == TEAM_SPECTATOR){
		client->lastJoinedTime = level.time;
	}

	client->rockPaperScissorsOtherClientNum = ENTITYNUM_NONE;
	client->rockPaperScissorsChallengeTime = 0;
	client->rockPaperScissorsStartTime = 0;
	client->rockPaperScissorsBothChosenTime = 0;
	client->rockPaperScissorsChoice = '\0';
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (g_entities[i].inuse && g_entities[i].client && g_entities[i].client->rockPaperScissorsOtherClientNum == ent - g_entities) {
			g_entities[i].client->rockPaperScissorsOtherClientNum = ENTITYNUM_NONE;
			g_entities[i].client->rockPaperScissorsChallengeTime = 0;
			g_entities[i].client->rockPaperScissorsStartTime = 0;
			g_entities[i].client->rockPaperScissorsBothChosenTime = 0;
			g_entities[i].client->rockPaperScissorsChoice = '\0';
		}
	}
}

void SetNameQuick( gentity_t *ent, char *s, int renameDelay ) {
	char	userinfo[MAX_INFO_STRING];

	trap_GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );
	Info_SetValueForKey(userinfo, "name", s);
	trap_SetUserinfo( ent->s.number, userinfo );

	ent->client->pers.netnameTime = level.time - 1; // bypass delay
	ClientUserinfoChanged( ent->s.number );
	// TODO: display something else than "5 seconds" to the player
	ent->client->pers.netnameTime = level.time + ( renameDelay < 0 ? 0 : renameDelay );
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void StopFollowing( gentity_t *ent ) {
	vec3_t origin, viewangles;
	int commandTime;
	int ping;

	// save necessary values
	commandTime = ent->client->ps.commandTime;
	ping = ent->client->ps.ping;
	VectorCopy(ent->client->ps.origin,origin);
	VectorCopy(ent->client->ps.viewangles,viewangles);

	// clean spec's playerstate
	memset(&ent->client->ps,0,sizeof(ent->client->ps));

	// set necessary values
	ent->client->ps.commandTime = commandTime;
	ent->client->ps.ping = ping;
	VectorCopy(origin,ent->client->ps.origin);
	VectorCopy(viewangles,ent->client->ps.viewangles);

	ent->client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;	
	ent->client->sess.sessionTeam = TEAM_SPECTATOR;	
	ent->client->sess.spectatorState = SPECTATOR_FREE;
	ent->client->ps.clientNum = ent - g_entities;
	ent->client->ps.stats[STAT_HEALTH] = 100;
	ent->client->ps.cloakFuel = 100;
	ent->client->ps.jetpackFuel = 100;
	ent->client->ps.crouchheight = CROUCH_MAXS_2;
	ent->client->ps.standheight = DEFAULT_MAXS_2;
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Race_f( gentity_t *ent );
void Cmd_Team_f( gentity_t *ent ) {
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	oldTeam = ent->client->sess.sessionTeam;

	if ( trap_Argc() != 2 ) {		
		switch ( oldTeam ) {
		case TEAM_BLUE:
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")) );
			break;
		case TEAM_RED:
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")) );
			break;
		case TEAM_FREE:
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTFREETEAM")) );
			break;
		case TEAM_SPECTATOR:
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTSPECTEAM")) );
			break;
		}
		return;
	}

	trap_Argv( 1, s, sizeof( s ) );

	if ( ent->client->sess.inRacemode ) {
		// simulate /race if already in racemode and writing /team s
		if ( !Q_stricmpn( s, "s", 1 ) ) {
			Cmd_Race_f( ent );
		} else {
			trap_SendServerCommand( ent - g_entities, "print \"Cannot switch teams while in racemode!\n\"" );
		}

		return;
	}

	if ( ent->client->switchTeamTime > level.time ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	if (gEscaping)
	{
		return;
	}

	// if they are playing a tournement game, count as a loss
	if ( g_gametype.integer == GT_DUEL
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {//in a tournament game
		//disallow changing teams
		trap_SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Duel\n\"" );
		return;
		//FIXME: why should this be a loss???
	}

	if (g_gametype.integer == GT_POWERDUEL)
	{ //don't let clients change teams manually at all in powerduel, it will be taken care of through automated stuff
		trap_SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Power Duel\n\"" );
		return;
	}
	
	SetTeam( ent, s );

	if (oldTeam != ent->client->sess.sessionTeam) // *CHANGE 16* team change actually happend
		ent->client->switchTeamTime = level.time + 5000;
}

/*
=================
Cmd_DuelTeam_f
=================
*/
void Cmd_DuelTeam_f(gentity_t *ent)
{
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	if (g_gametype.integer != GT_POWERDUEL)
	{ //don't bother doing anything if this is not power duel
		return;
	}

	if ( trap_Argc() != 2 )
	{ //No arg so tell what team we're currently on.
		oldTeam = ent->client->sess.duelTeam;
		switch ( oldTeam )
		{
		case DUELTEAM_FREE:
			trap_SendServerCommand( ent-g_entities, va("print \"None\n\"") );
			break;
		case DUELTEAM_LONE:
			trap_SendServerCommand( ent-g_entities, va("print \"Single\n\"") );
			break;
		case DUELTEAM_DOUBLE:
			trap_SendServerCommand( ent-g_entities, va("print \"Double\n\"") );
			break;
		default:
			break;
		}
		return;
	}

	if ( ent->client->switchDuelTeamTime > level.time )
	{ //debounce for changing
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	trap_Argv( 1, s, sizeof( s ) );

	oldTeam = ent->client->sess.duelTeam;

	if (!Q_stricmp(s, "free"))
	{
		ent->client->sess.duelTeam = DUELTEAM_FREE;
	}
	else if (!Q_stricmp(s, "single"))
	{
		ent->client->sess.duelTeam = DUELTEAM_LONE;
	}
	else if (!Q_stricmp(s, "double"))
	{
		ent->client->sess.duelTeam = DUELTEAM_DOUBLE;
	}
	else
	{
		trap_SendServerCommand( ent-g_entities, va("print \"'%s' not a valid duel team.\n\"", s) );
	}

	if (oldTeam == ent->client->sess.duelTeam)
	{ //didn't actually change, so don't care.
		return;
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //ok..die
		int curTeam = ent->client->sess.duelTeam;
		ent->client->sess.duelTeam = oldTeam;
		G_Damage(ent, ent, ent, NULL, ent->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		ent->client->sess.duelTeam = curTeam;
	}
	//reset wins and losses
	ent->client->sess.wins = 0;
	ent->client->sess.losses = 0;

	//get and distribute relevent paramters
	ClientUserinfoChanged( ent->s.number );

	ent->client->switchDuelTeamTime = level.time + 5000;
}

int G_TeamForSiegeClass(const char *clName)
{
	int i = 0;
	int team = SIEGETEAM_TEAM1;
	siegeTeam_t *stm = BG_SiegeFindThemeForTeam(team);
	siegeClass_t *scl;

	if (!stm)
	{
		return 0;
	}

	while (team <= SIEGETEAM_TEAM2)
	{
		scl = stm->classes[i];

		if (scl && scl->name[0])
		{
			if (!Q_stricmp(clName, scl->name))
			{
				return team;
			}
		}

		i++;
		if (i >= MAX_SIEGE_CLASSES || i >= stm->numClasses)
		{
			if (team == SIEGETEAM_TEAM2)
			{
				break;
			}
			team = SIEGETEAM_TEAM2;
			stm = BG_SiegeFindThemeForTeam(team);
			i = 0;
		}
	}

	return 0;
}

void SetSiegeClass( gentity_t *ent, char* className)
{
	qboolean startedAsSpec = qfalse;
	int team = 0;
	int preScore;

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		startedAsSpec = qtrue;
	}

	team = G_TeamForSiegeClass( className );

	if ( !team )
	{ //not a valid class name
		return;
	}

	if ( ent->client->sess.sessionTeam != team )
	{ //try changing it then
		g_preventTeamBegin = qtrue;
		if ( team == TEAM_RED )
		{
			SetTeam( ent, "red" );
		}
		else if ( team == TEAM_BLUE )
		{
			SetTeam( ent, "blue" );
		}
		g_preventTeamBegin = qfalse;

		if ( ent->client->sess.sessionTeam != team )
		{ //failed, oh well
			if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ||
				ent->client->sess.siegeDesiredTeam != team )
			{
				trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOCLASSTEAM" ) ) );
				return;
			}
		}
	}

	//preserve 'is score
	preScore = ent->client->ps.persistant[PERS_SCORE];

	//Make sure the class is valid for the team
	BG_SiegeCheckClassLegality( team, className );

	//Set the session data
	strcpy( ent->client->sess.siegeClass, className );

	// get and distribute relevent paramters
	ClientUserinfoChanged( ent->s.number );

	if ( ent->client->tempSpectate < level.time )
	{
		// Kill him (makes sure he loses flags, etc)
		if ( ent->health > 0 && !startedAsSpec )
		{
			ent->flags &= ~FL_GODMODE;
			ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
			player_die( ent, ent, ent, 100000, MOD_SUICIDE );
		}

		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR || startedAsSpec )
		{ //respawn them instantly.
			ClientBegin( ent->s.number, qfalse );
		}
	}
	//set it back after we do all the stuff
	ent->client->ps.persistant[PERS_SCORE] = preScore;

	ent->client->switchClassTime = level.time + 5000;
}

/*
=================
Cmd_SiegeClass_f
=================
*/
void Cmd_SiegeClass_f( gentity_t *ent )
{
	char className[64];

	if (g_gametype.integer != GT_SIEGE)
	{ //classes are only valid for this gametype
		return;
	}

	if (!ent->client)
	{
		return;
	}

	if (trap_Argc() < 1)
	{
		return;
	}

	if ( ent->client->switchClassTime > level.time )
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSSWITCH")) );
		return;
	}	

	trap_Argv( 1, className, sizeof( className ) );

	SetSiegeClass( ent, className );  
}

void Cmd_Class_f( gentity_t *ent )
{
	char className[16];
	int classNumber = 0;
	siegeClass_t* siegeClass = 0;

	if ( !ent || !ent->client )
	{
		return;
	}

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		return;
	}

	if ( trap_Argc() < 1 )
	{
		trap_SendServerCommand( ent - g_entities, "print \"Invalid arguments. \n\"" );
		return;
	}

	if ( ent->client->switchClassTime > level.time )
	{
		trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOCLASSSWITCH" ) ) );
		return;
	}

	trap_Argv( 1, className, sizeof( className ) );

	if ( (className[0] >= '0') && (className[0] <= '9') )
	{
		classNumber = atoi( className );
	}
	else
	{
		 // funny way for pro siegers
		switch ( tolower(className[0]) )
		{
			case 'a': classNumber = 1; break;
			case 'h': classNumber = 2; break;
			case 'd': classNumber = 3; break;
			case 's': classNumber = 4; break;
			case 't': classNumber = 5; break;
			case 'j': classNumber = 6; break;
			default:
				trap_SendServerCommand( ent - g_entities, "print \"Invalid class identifier. \n\"" );
				return;	   
		}

	} 
	
	siegeClass = BG_SiegeGetClass( ent->client->sess.sessionTeam, classNumber);
	if ( !siegeClass )
	{	 		
		trap_SendServerCommand( ent - g_entities, "print \"Invalid class number. \n\"" );
		return;
	}

	SetSiegeClass( ent, siegeClass->name );												  
}

/*
=================
Cmd_ForceChanged_f
=================
*/
void Cmd_ForceChanged_f( gentity_t *ent )
{
	char fpChStr[1024];
	const char *buf;
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{ //if it's a spec, just make the changes now
		//No longer print it, as the UI calls this a lot.
		WP_InitForcePowers( ent );
		goto argCheck;
	}

	buf = G_GetStringEdString("MP_SVGAME", "FORCEPOWERCHANGED");

	strcpy(fpChStr, buf);

	trap_SendServerCommand( ent-g_entities, va("print \"%s%s\n\n\"", S_COLOR_GREEN, fpChStr) );

	ent->client->ps.fd.forceDoInit = 1;
argCheck:
	if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
	{ //If this is duel, don't even bother changing team in relation to this.
		return;
	}

	if (trap_Argc() > 1)
	{
		char	arg[MAX_TOKEN_CHARS];

		trap_Argv( 1, arg, sizeof( arg ) );

		if (/*arg &&*/ arg[0])
		{ //if there's an arg, assume it's a combo team command from the UI.
			Cmd_Team_f(ent);
		}
	}
}

extern qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
extern qboolean WP_UseFirstValidSaberStyle( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int *saberAnimLevel );
qboolean G_SetSaber(gentity_t *ent, int saberNum, char *saberName, qboolean siegeOverride)
{
	char truncSaberName[64];
	int i = 0;

	if (!siegeOverride &&
		g_gametype.integer == GT_SIEGE &&
		ent->client->siegeClass != -1 &&
		(
		 bgSiegeClasses[ent->client->siegeClass].saberStance ||
		 bgSiegeClasses[ent->client->siegeClass].saber1[0] ||
		 bgSiegeClasses[ent->client->siegeClass].saber2[0]
		))
	{ //don't let it be changed if the siege class has forced any saber-related things
        return qfalse;
	}

	while (saberName[i] && i < 64-1)
	{
        truncSaberName[i] = saberName[i];
		i++;
	}
	truncSaberName[i] = 0;

	if ( saberNum == 0 && (Q_stricmp( "none", truncSaberName ) == 0 || Q_stricmp( "remove", truncSaberName ) == 0) )
	{ //can't remove saber 0 like this
        strcpy(truncSaberName, "Kyle");
	}

	//Set the saber with the arg given. If the arg is
	//not a valid sabername defaults will be used.
	WP_SetSaber(ent->s.number, ent->client->saber, saberNum, truncSaberName);

	if (!ent->client->saber[0].model[0])
	{
		assert(0); //should never happen!
		strcpy(ent->client->sess.saberType, "none");
	}
	else
	{
		strcpy(ent->client->sess.saberType, ent->client->saber[0].name);
	}

	if (!ent->client->saber[1].model[0])
	{
		strcpy(ent->client->sess.saber2Type, "none");
	}
	else
	{
		strcpy(ent->client->sess.saber2Type, ent->client->saber[1].name);
	}

    if ( g_gametype.integer != GT_SIEGE )
    {
        if ( !WP_SaberStyleValidForSaber( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel ) )
        {
            WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel );
            ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
        }
    }

	return qtrue;
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent ) {
	int		i;
	char	arg[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 2 ) {
		if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent );
		}
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	i = ClientNumberFromString( ent, arg );
	if ( i == -1 ) {
		return;
	}

	// can't follow self
	if ( &level.clients[ i ] == ent->client ) {
		return;
	}

	// can't follow another spectator
	//if either g_followSpectator is turned off or targeted spectator is already following someoneelse
	if  (/*(!g_followSpectator.integer || level.clients[ i ].sess.spectatorState == SPECTATOR_FOLLOW)
		&&*/ level.clients[ i ].sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}

	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		//WTF???
		ent->client->sess.losses++;
	}

	// first set them to spectator
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeam( ent, "spectator" );
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		return; // we weren't able to go spec for some reason; stop here

	ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent, int dir ) {
	int		clientnum;
	int		original;

	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {\
		//WTF???
		ent->client->sess.losses++;
	}
	// first set them to spectator
	if ( ent->client->sess.spectatorState == SPECTATOR_NOT ) {
		if ( ent->client->sess.inRacemode ) {
			trap_SendServerCommand( ent - g_entities, "print \"Cannot switch teams while in racemode!\n\"" );
			return;
		}

		SetTeam( ent, "spectator" );
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		return; // we weren't able to go spec for some reason; stop here

	if ( dir != 1 && dir != -1 ) {
		G_Error( "Cmd_FollowCycle_f: bad dir %i", dir );
	}

	clientnum = ent->client->sess.spectatorClient;
	if (clientnum < 0) clientnum += level.maxclients;
	original = clientnum;
	do {
		clientnum += dir;
		if ( clientnum >= level.maxclients ) {
			clientnum = 0;
		}
		if ( clientnum < 0 ) {
			clientnum = level.maxclients - 1;
		}

		// can only follow connected clients
		if ( level.clients[ clientnum ].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if (/*(!g_followSpectator.integer || level.clients[ clientnum ].sess.spectatorState == SPECTATOR_FOLLOW)
			&&*/ level.clients[ clientnum ].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		// this is good, we can use it
		ent->client->sess.spectatorClient = clientnum;
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		return;
	} while ( clientnum != original );

	// leave it where it was
}

void Cmd_FollowFlag_f( gentity_t *ent ) 
{
	int		clientnum;
	int		original;

	// first set them to spectator
	if ( ent->client->sess.spectatorState == SPECTATOR_NOT ) {
		SetTeam( ent, "spectator" );
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		return; // we weren't able to go spec for some reason; stop here

	clientnum = ent->client->sess.spectatorClient;
	if (clientnum < 0) clientnum += level.maxclients;
	original = clientnum;
	do {
		++clientnum;
		if ( clientnum >= level.maxclients ) {
			clientnum = 0;
		}
		if ( clientnum < 0 ) {
			clientnum = level.maxclients - 1;
		}

		// can only follow connected clients
		if ( level.clients[ clientnum ].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if (/*(!g_followSpectator.integer || level.clients[ clientnum ].sess.spectatorState == SPECTATOR_FOLLOW)
			&&*/ level.clients[ clientnum ].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		if (level.clients[ clientnum ].ps.powerups[PW_REDFLAG] || level.clients[ clientnum ].ps.powerups[PW_BLUEFLAG]){
			if ( level.clients[clientnum].sess.inRacemode ) {
				continue; // no following clients in racemode with flags
			}

			// this is good, we can use it
			ent->client->sess.spectatorClient = clientnum;
			ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
			return;		
		}
	} while ( clientnum != original );
}

void Cmd_FollowTarget_f(gentity_t *ent) {
	// first set them to spectator
	if (ent->client->sess.spectatorState == SPECTATOR_NOT) {
		SetTeam(ent, "spectator");
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		return; // we weren't able to go spec for some reason; stop here

	// check who is eligible to be followed
	qboolean valid[MAX_CLIENTS] = { qfalse }, gotValid = qfalse;
	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (i == ent->s.number || !g_entities[i].inuse || level.clients[i].pers.connected != CON_CONNECTED || level.clients[i].sess.sessionTeam == TEAM_SPECTATOR)
			continue;
		if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW && ent->client->sess.spectatorClient == i)
			continue; // already following this guy
		valid[i] = qtrue;
		gotValid = qtrue;
	}
	if (!gotValid)
		return; // nobody to follow

	// check for aiming directly at someone
	trace_t tr;
	vec3_t start, end, forward;
	VectorCopy(ent->client->ps.origin, start);
	AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);
	VectorMA(start, 16384, forward, end);
	start[2] += ent->client->ps.viewheight;
	trap_G2Trace(&tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
	if (tr.entityNum >= 0 && tr.entityNum < MAX_CLIENTS && valid[tr.entityNum]) {
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		ent->client->sess.spectatorClient = tr.entityNum;
		return;
	}

	// see who was closest to where we aimed
	float closestDistance = -1;
	int closestPlayer = -1;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!valid[i])
			continue;
		vec3_t difference;
		VectorSubtract(level.clients[i].ps.origin, tr.endpos, difference);
		if (closestDistance == -1 || VectorLength(difference) < closestDistance) {
			closestDistance = VectorLength(difference);
			closestPlayer = i;
		}
	}

	if (closestDistance != -1 && closestPlayer != -1) {
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		ent->client->sess.spectatorClient = closestPlayer;
	}
}

/*
==================
G_Say
==================
*/
#define EC		"\x19"

static qboolean ChatLimitExceeded(gentity_t *ent, int mode) {
	if (!ent || !ent->client)
		return qfalse;

	int *sentTime, *sentCount, *sentTick, *allowedTick;
	int limit, tickWaitMinimum;

	if ( mode == -1 && ent->client->sess.canJoin ) { // -1 is voice chat
		sentTime = &ent->client->pers.voiceChatSentTime;
		sentCount = &ent->client->pers.voiceChatSentCount;
		sentTick = &ent->client->pers.voiceChatsentTick;
		allowedTick = &ent->client->pers.voiceChatAllowedTick;

		limit = g_voiceChatLimit.integer;
		tickWaitMinimum = g_voiceChatTickWaitMinimum.integer;
	} else if (mode == SAY_TEAM && ent->client->sess.canJoin && !IsRacerOrSpectator(ent)) { // using teamchat while ingame
		sentTime = &ent->client->pers.teamChatSentTime;
		sentCount = &ent->client->pers.teamChatSentCount;
		sentTick = &ent->client->pers.teamChatsentTick;
		allowedTick = &ent->client->pers.teamChatAllowedTick;

		limit = g_teamChatLimit.integer;
		tickWaitMinimum = g_teamChatTickWaitMinimum.integer;
	} else { // using all chat OR private chat (or anything for passwordless)
		sentTime = &ent->client->pers.chatSentTime;
		sentCount = &ent->client->pers.chatSentCount;
		sentTick = &ent->client->pers.chatsentTick;
		allowedTick = &ent->client->pers.chatAllowedTick;

		limit = g_chatLimit.integer;
		tickWaitMinimum = g_chatTickWaitMinimum.integer;
	}

	qboolean exceeded = qfalse;

	// first, check to make sure binds aren't simply held down, regardless of number of messages
	if (tickWaitMinimum && g_svfps.integer >= 30) {
		if (*allowedTick && level.framenum < *allowedTick) {
			// cooldown still running, can't send yet
			//Com_DebugPrintf("4 -- level.time %d level.framenum %d sentTime %d sentCount %d sentTick %d allowedTick %d\n", level.time, level.framenum, *sentTime, *sentCount, *sentTick, *allowedTick);
			exceeded = qtrue;
		}
		else if (*sentTick && level.framenum - *sentTick < 2) {
			// this message came within one tick of the previous one, which means this idiot is just holding down a bind. put them on cooldown
			//Com_DebugPrintf("5 -- level.time %d level.framenum %d sentTime %d sentCount %d sentTick %d allowedTick %d\n", level.time, level.framenum, *sentTime, *sentCount, *sentTick, *allowedTick);
			exceeded = qtrue;
			const int delay = 16; // idk
			*allowedTick = level.framenum + delay;
			if (limit > 3)
				limit = 3;
			//*sentTick = level.framenum;
		}
		else {
			// okay
			//Com_DebugPrintf("6 -- level.time %d level.framenum %d sentTime %d sentCount %d sentTick %d allowedTick %d\n", level.time, level.framenum, *sentTime, *sentCount, *sentTick, *allowedTick);
			*sentTick = level.framenum;
		}
	}

	qboolean forceExceeded = exceeded;

	// second, look at how many messages have been sent recently
	if (!ent->client->sess.canJoin && IsRacerOrSpectator(ent) && !ent->client->account) { // for passwordless specs, apply a more strict anti spam with a permanent lock, just like sv_floodprotect
		//Com_DebugPrintf("1 -- level.time %d level.framenum %d sentTime %d sentCount %d sentTick %d allowedTick %d\n", level.time, level.framenum, *sentTime, *sentCount, *sentTick, *allowedTick);
		exceeded = level.time - *sentTime < 1000;
	}
	else if (limit > 0 && *sentTime && (level.time - *sentTime) < 1000) { // we are in tracking second for current user, check limit
		//Com_DebugPrintf("2 -- level.time %d level.framenum %d sentTime %d sentCount %d sentTick %d allowedTick %d\n", level.time, level.framenum, *sentTime, *sentCount, *sentTick, *allowedTick);
		exceeded = *sentCount >= limit;
		++(*sentCount);
	}
	else { // this is the first and only message that has been sent in the last second
		//Com_DebugPrintf("3 -- level.time %d level.framenum %d sentTime %d sentCount %d sentTick %d allowedTick %d\n", level.time, level.framenum, *sentTime, *sentCount, *sentTick, *allowedTick);
		*sentCount = 1;
	}

	if (forceExceeded)
		exceeded = qtrue;

	// in any case, reset the timer so people have to unpress their spam binds while blocked to be unblocked
	*sentTime = level.time;

	return exceeded;
}

qboolean IsRacerOrSpectator(gentity_t *ent) {
	if (g_gametype.integer != GT_CTF || !ent || !ent->client || ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE)
		return qfalse;
	return qtrue;
}

qboolean ClientIsRacerOrSpectator(gclient_t *client) {
	if (g_gametype.integer != GT_CTF || !client || client->sess.sessionTeam == TEAM_RED || client->sess.sessionTeam == TEAM_BLUE)
		return qfalse;
	return qtrue;
}

void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message, char *locMsg )
{
	if (!other) {
		return;
	}
	if (!other->inuse) {
		return;
	}
	if (!other->client) {
		return;
	}
	if ( other->client->pers.connected != CON_CONNECTED ) {
		return;
	}
	if ( mode == SAY_TEAM  && !OnSameTeam(ent, other) && !(IsRacerOrSpectator(ent) && IsRacerOrSpectator(other)) ) {
		return;
	}
	if ((other->client->sess.ignoreFlags & (1<<(ent-g_entities)))
		&& (ent!=other)	){
		return;
	}

	// if this guy is shadow muted, don't let anyone see his messages except himself and other shadow muted clients
	if ( ent->client && ent->client->sess.shadowMuted && ent != other && !other->client->sess.shadowMuted ) {
		return;
	}

	// prevent certain individuals from jebaiting their fc into sking by writing "get" while someone else has the flag
	if (ent->client && ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_GETTROLL) && mode == SAY_TEAM &&
		ent != other && g_gametype.integer == GT_CTF && !IsRacerOrSpectator(ent) && level.wasRestarted &&
		Q_stristrclean(message, "get") && !HasFlag(ent) && level.pause.state == PAUSE_NONE) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisGuy = &g_entities[i];
			if (!thisGuy->inuse || !thisGuy->client || thisGuy == ent || thisGuy->client->pers.connected != CON_CONNECTED)
				continue;
			if (thisGuy->client->sess.sessionTeam != ent->client->sess.sessionTeam)
				continue;
			if (!HasFlag(thisGuy))
				continue;
			return;
		}
	}

	if (VALIDSTRING(message)) {
		char *filter = stristr(message, "sertao");
		if (filter) {
			filter += 4;
			if (*filter == 'a')
				*filter = 'ã';
			else if (*filter == 'A')
				*filter = 'Ã';
		}
	}

	//They've requested I take this out.

	if (locMsg && g_enableChatLocations.integer)
	{
		trap_SendServerCommand( other-g_entities, va("%s \"%s\" \"%s\" \"%c\" \"%s\" \"%i\"", 
			mode == SAY_TEAM ? "ltchat" : "lchat",
			name, locMsg, color, message, ent - g_entities));
	}
	else
	{
		trap_SendServerCommand( other-g_entities, va("%s \"%s%c%c%s\" \"%i\"", 
			mode == SAY_TEAM ? "tchat" : "chat",
			name, Q_COLOR_ESCAPE, color, message, ent - g_entities));
	}
}

char* GetSuffixId( gentity_t *ent ) {
	int i;
	gentity_t *other;
	static char buf[16];

	memset( buf, 0, sizeof( buf ) );

	if ( !g_duplicateNamesId.integer ) {
		return buf;
	}

	for ( i = 0; i < level.maxclients; i++ ) {
		other = &g_entities[i];

		if ( !other || other == ent || !other->inuse || !other->client ) {
			continue;
		}

		if ( !strcmp( ent->client->pers.netname, other->client->pers.netname ) ) {
			Com_sprintf( buf, sizeof( buf ), " (%i)", ent - g_entities );
			break;
		}
	}

	return buf;
}

#define NUM_SERIALIZE_BUFFERS	(4)
char* NM_SerializeUIntToColor( const unsigned int n ) {
	static char result[NUM_SERIALIZE_BUFFERS][32] = { 0 };
	static int index = -1;
	char buf[32] = { 0 };
	int i;

	index = (index + 1) % NUM_SERIALIZE_BUFFERS;

	Com_sprintf( buf, sizeof( buf ), "%o", n );
	result[index][0] = '\0';

	for ( i = 0; buf[i] != '\0'; ++i ) {
		Q_strcat( result[index], sizeof(result[index]), va("%c%c", Q_COLOR_ESCAPE, buf[i]));
	}

	if (debug_clientNumLog.integer)
		G_LogPrintf("clientNumLog: client %u (%s) --> %s\n", n, level.clients[n].pers.netname, result[index]);

	return &result[index][0];
}

static void WriteTextForToken( gentity_t *ent, const char token, char *buffer, size_t bufferSize ) {
	buffer[0] = '\0';

	if ( !ent || !ent->client ) {
		return;
	}

	gclient_t *cl = ent->client;

	char *s = NULL;
	int weapon = WP_NONE;

	switch ( token ) {
	case 'h': case 'H':
		Com_sprintf( buffer, bufferSize, "%d", Com_Clampi( 0, 999, cl->ps.stats[STAT_HEALTH] ) );
		break;
	case 'a': case 'A':
		Com_sprintf( buffer, bufferSize, "%d", Com_Clampi( 0, 999, cl->ps.stats[STAT_ARMOR] ) );
		break;
	case 'f': case 'F':
		Com_sprintf( buffer, bufferSize, "%d", Com_Clampi( 0, 999, cl->ps.fd.forcePowersKnown == 0 ? 0 : cl->ps.fd.forcePower ) );
		break;
	case 'o': case 'O':
		Com_sprintf(buffer, bufferSize, "%02d", Com_Clampi(0, 59, ((level.time - level.startTime + 30000) / 1000) % 60));
		break;
	case 'm': case 'M':
		if ((!weaponData[cl->ps.weapon].energyPerShot && !weaponData[cl->ps.weapon].altEnergyPerShot) || cl->ps.stats[STAT_HEALTH] <= 0 || (g_gametype.integer == GT_SIEGE && cl->tempSpectate > level.time))
			Com_sprintf(buffer, bufferSize, "0");
		else
			Com_sprintf(buffer, bufferSize, "%d", Com_Clampi(0, 999, cl->ps.ammo[weaponData[cl->ps.weapon].ammoIndex]));
		break;
	case 'l': case 'L':
		Team_GetLocation( ent, buffer, bufferSize, qtrue );
		break;
	case 't': case 'T':
		GetLocationPlayerIsAimingAt(ent, buffer, bufferSize);
		break;
	case 'd': case 'D':
		if (ent->client->pers.hasDied)
			GetLocationOfLastPlayerDeath(ent, buffer, bufferSize);
		else
			Team_GetLocation(ent, buffer, bufferSize, qtrue);
		break;
	case 'w': case 'W':
		if (ent->health > 0 && !(ent->client && ent->client->ps.pm_type == PM_SPECTATOR))
			weapon = ent->client->ps.weapon;
		else
			weapon = ent->client->pers.weaponLastDiedWith;
		switch (weapon) {
		case WP_STUN_BATON:			s = "Stun Baton";			break;
		case WP_MELEE:				s = "Melee";				break;
		case WP_SABER:				s = "Saber";				break;
		case WP_BRYAR_PISTOL:	case WP_BRYAR_OLD:				s = "Pistol";	break;
		case WP_BLASTER:			s = "Blaster";				break;
		case WP_DISRUPTOR:			s = "Disruptor";			break;
		case WP_BOWCASTER:			s = "Bowcaster";			break;
		case WP_REPEATER:			s = "Repeater";				break;
		case WP_DEMP2:				s = "Demp";					break;
		case WP_FLECHETTE:			s = "Golan";				break;
		case WP_ROCKET_LAUNCHER:	s = "Rockets";	break;
		case WP_THERMAL:			s = "Thermals";				break;
		case WP_TRIP_MINE:			s = "Mines";				break;
		case WP_DET_PACK:			s = "Detpacks";				break;
		case WP_CONCUSSION:			s = "Concussion";			break;
		case WP_EMPLACED_GUN:		s = "Emplaced";				break;
		default:					return;
		}
		Com_sprintf(buffer, bufferSize, s);
		break;
	default: return;
	}
}

#define TEAM_CHAT_TOKEN_CHAR '$'

static void TokenizeTeamChat( gentity_t *ent, char *dest, const char *src, size_t destsize ) {
	const char *p;
	char tokenText[64];
	int i = 0;

	if ( !ent || !ent->client ) {
		return;
	}

	for ( p = src; p && *p && destsize > 1; ++p ) {
		if ( *p == TEAM_CHAT_TOKEN_CHAR ) {
			WriteTextForToken( ent, *++p, tokenText, sizeof( tokenText ) );

			if ( !tokenText[0] && ( *p == 'b' || *p == 'B' ) && *++p ) { // special case for boon: write text after if i have it
				int len, offset = 0;
				char *s = strchr( p, TEAM_CHAT_TOKEN_CHAR );

				if ( s && *s ) {
					len = s - p;
				} else {
					len = strlen( p );
					offset = 1; // not terminated by a $, so point back to the char before
				}

				if ( !len ) { // retard terminated it immediately
					--p;
					continue;
				}

				if ( !ent->client->ps.powerups[PW_FORCE_BOON] ) {
					p += ( len - offset );
					continue;
				}

				destsize -= len;

				if ( destsize > 0 ) {
					Q_strcat( dest + i, len + 1, p );
					i += len;
					p += ( len - offset );
					continue;
				} else {
					break;
				}
			}

			if ( VALIDSTRING( tokenText ) ) {
				int len = strlen( tokenText );
				destsize -= len;

				if ( destsize > 0 ) {
					Q_strcat( dest + i, len + 1, tokenText );
					i += len;
					continue;
				} else {
					break; // don't write it at all if there is no room
				}
			}

			--p; // this token is invalid, go back to write the $ sign
		}

		dest[i++] = *p;
		--destsize;
	}

	dest[i] = '\0';
}

static qboolean ChatMessageShouldPause(const char *s) {
	if (!VALIDSTRING(s)) {
		assert(qfalse);
		return qfalse;
	}

	size_t len = strlen(s);

	if (!Q_stricmpn(s, "pause", 5) && len <= 6)
		return qtrue;

	if (!Q_stricmpn(s, "need to pause", 13) && len <= 14) // lg brain
		return qtrue;

	if (PauseConditions()) { // stricter requirement since people type these normally
		if (!Q_stricmpn(s, "sec", 3) && len <= 4) { // stricter requirement since this is so short
			const unsigned char c = (unsigned)*(s + 3);
			if (!c || isspace(c) || ispunct(c) || c == '\\' || c == '\'' || c == '~' || c == '`')
				return qtrue;
			return qfalse;
		}

		if (!Q_stricmpn(s, "1sec", 4) && len <= 5)
			return qtrue;

		if (!Q_stricmpn(s, "1 sec", 5) && len <= 6)
			return qtrue;
	}

	return qfalse;
}

qboolean IsInstapauser(gentity_t *ent) {
	if (!g_quickPauseMute.integer || level.pause.state == PAUSE_NONE || !level.pause.pauserClient.valid)
		return qfalse;

	if (level.pause.pauserClient.clientNum == ent - g_entities)
		return qtrue;

	if (ent->client && ent->client->account && ent->client->account->id == level.pause.pauserClient.accountId)
		return qtrue;

	return qfalse;
}

char *ReplaceString(const char *orig, char *rep, char *with) {
	char *result; // the return string
	char *ins;    // the next insert point
	char *tmp;    // varies
	int len_rep;  // length of rep (the string to remove)
	int len_with; // length of with (the string to replace rep with)
	int len_front; // distance between rep and end of last rep
	int count;    // number of replacements

	// sanity checks and initialization
	if (!orig || !rep)
		return NULL;
	len_rep = strlen(rep);
	if (len_rep == 0)
		return NULL; // empty rep causes infinite loop during count
	if (!with)
		with = "";
	len_with = strlen(with);

	// count the number of replacements needed
	ins = (char *)orig;
	for (count = 0; tmp = stristr(ins, rep); ++count) {
		ins = tmp + len_rep;
	}

	tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

	if (!result)
		return NULL;

	// first time through the loop, all the variable are set correctly
	// from here on,
	//    tmp points to the end of the result string
	//    ins points to the next occurrence of rep in orig
	//    orig points to the remainder of orig after "end of rep"
	while (count--) {
		ins = stristr(orig, rep);
		len_front = ins - orig;
		tmp = strncpy(tmp, orig, len_front) + len_front;
		tmp = strcpy(tmp, with) + len_with;
		orig += len_front + len_rep; // move to next "end of rep"
	}
	strcpy(tmp, orig);
	return result;
}

static qboolean IsSlur(const char *start, const char *slurFromList) {
	if (!VALIDSTRING(start) || !VALIDSTRING(slurFromList))
		return qfalse;

	static const char charMap[256] = {
		/* 0x00 */ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		/* 0x08 */ 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		/* 0x10 */ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		/* 0x18 */ 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
		/* 0x20 */ ' ',  '!',  '"',  '#',  '$',  '%',  '&',  '\'',
		/* 0x28 */ '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
		/* 0x30 */ 'o',  'i',  'z',  'e',  'a',  's',  'b',  't',
		/* 0x38 */ 'b',  'g',  ':',  ';',  '<',  '=',  '>',  '?',
		/* 0x40 */ '@',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
		/* 0x48 */ 'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
		/* 0x50 */ 'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
		/* 0x58 */ 'x',  'y',  'z',  '[',  '\\', ']',  '^',  '_',
		/* 0x60 */ '`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
		/* 0x68 */ 'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
		/* 0x70 */ 'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
		/* 0x78 */ 'x',  'y',  'z',  '{',  '|',  '}',  '~',  0x7F,
		/* 0x80 */ 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		/* 0x88 */ 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
		/* 0x90 */ 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
		/* 0x98 */ 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
		/* 0xA0 */ 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
		/* 0xA8 */ 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
		/* 0xB0 */ 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
		/* 0xB8 */ 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
		/* 0xC0 */ 'a',  'a',  'a',  'a',  'e',  'a',  'a',  'c',
		/* 0xC8 */ 'e',  'e',  'e',  'e',  'i',  'i',  'i',  'i',
		/* 0xD0 */ 'd',  'n',  'o',  'o',  'o',  'o',  'o',  0xD7,
		/* 0xD8 */ 'o',  'u',  'u',  'u',  'u',  'y',  0xDE, 0xDF,
		/* 0xE0 */ 'a',  'a',  'a',  'a',  'e',  'a',  'a',  'c',
		/* 0xE8 */ 'e',  'e',  'e',  'e',  'i',  'i',  'i',  'i',
		/* 0xF0 */ 'o',  'n',  'o',  'o',  'o',  'o',  'o',  0xF7,
		/* 0xF8 */ 'o',  'u',  'u',  'u',  'u',  'y',  0xFE, 0xFF
	};


	// compare each character, using the map for possible substitutions
	while (*start && *slurFromList) {
		char mappedChar = charMap[(unsigned char)*start];

		if (*slurFromList != mappedChar)
			return qfalse;

		++start;
		++slurFromList;
	}

	// If we reached the end of the pattern string, it's a match
	return (!*slurFromList);
}

qboolean HasSlur(const char *str) {
	if (!VALIDSTRING(str))
		return qfalse;

	iterator_t iter;
	ListIterate(&level.slurList, &iter, qfalse);

	while (IteratorHasNext(&iter)) {
		slur_t *slur = IteratorNext(&iter);
		const char *start = str;

		while (*start) {
			if (IsSlur(start, slur->text))
				return qtrue;
			++start;
		}
	}

	return qfalse;
}

void Cmd_CallVote_f(gentity_t *ent, int pause);
void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText, qboolean force ) {
	assert(ent && ent->client);
	int			j;
	gentity_t	*other;
	int			color;
	char		name[64];
	// don't let text be too long for malicious reasons
	char		text[MAX_SAY_TEXT] = { 0 };
	char		location[64];
	char		*locMsg = NULL;

	if (!Q_stricmpnclean(ent->client->pers.netname, "elo BOT", 7)) {
		if (mode != SAY_ALL)
			return;

		mode = SAY_TELL;
		target = ent;
	}

	// Check chat limit regardless of g_chatLimit since it now encapsulates
	// passwordless specs chat limit
	if ( ChatLimitExceeded( ent, mode ) ) {
		return;
	}

	if ( g_gametype.integer < GT_TEAM && mode == SAY_TEAM ) {
		mode = SAY_ALL;
	}

	if (mode == SAY_TEAM && VALIDSTRING(chatText) && Q_stristrclean(chatText, "TE please or TH if i am fully energized and health is low"))
		chatText = "^1$H ^5$F ^7$L";

	char *fixedMessage = NULL;
	if (VALIDSTRING(chatText)) {
		char *found = (char *)Q_stristrclean(chatText, "onasi");
		if (found && !strstr(chatText, "://")) {
			if (!stristr(chatText, "onasi"))
				Q_StripColor((char *)chatText);
			fixedMessage = ReplaceString(chatText, "onasi", "hannah");
			if (VALIDSTRING(fixedMessage)) {
				if (strlen(fixedMessage) >= MAX_SAY_TEXT)
					*(fixedMessage + MAX_SAY_TEXT - 1) = '\0';
				chatText = fixedMessage;
			}
		}
	}

	if (g_filterSlurs.integer && level.slurList.size > 0) {
		if (HasSlur(chatText)) {
			G_LogPrintf("Filtered slur %s from player %d (%s): %s\n",
				mode == SAY_TELL ? va("DM to player %d (%s)", target ? target - g_entities : -1, target && target->client ? target->client->pers.netname : "") : (mode == SAY_TEAM ? "teamchat" : "chat"),
				ent - g_entities,
				ent->client ? ent->client->pers.netname : "",
				chatText);
			return;
		}
	}

	// allow typing "pause" in the chat to instapause or call a pause vote
	qboolean forceMessage = qfalse;
	if (ChatMessageShouldPause(chatText) && ent->client->sess.sessionTeam != TEAM_SPECTATOR && !ent->client->sess.inRacemode && mode != SAY_TELL && g_quickPauseChat.integer) {// allow a small typo at the end
		// allow setting g_quickPauseChat to 2 for callvote-only mode
		if (g_quickPauseChat.integer != 2 && ent->client->account && !(ent->client->account->flags & ACCOUNTFLAG_INSTAPAUSE_BLACKLIST)) {
			// instapause
			if (g_quickPauseMute.integer && !level.pause.pauserChoice) {
				level.pause.time = level.time + 10000; // pause for 10 seconds
				level.pause.pauserClient.clientNum = ent - g_entities;
				level.pause.pauserClient.valid = qtrue;
				level.pause.pauserClient.accountId = ent->client->account->id;
			}
			else if (g_quickPauseMute.integer && level.pause.pauserChoice && level.pause.pauserClient.valid &&
				level.pause.pauserClient.clientNum == ent - g_entities && level.pause.state != PAUSE_NONE) {
				chatText = "I need to extend the pause a bit longer. Thank you for your continued patience. I sincerely apologize.";
				level.pause.time = level.time + 120000; // pause for 2 minutes
			}
			else {
				level.pause.time = level.time + 120000; // pause for 2 minutes
			}
			level.pause.state = PAUSE_PAUSED;
			PrintIngame(-1, "Pause requested by %s^7.\n", ent->client->pers.netname);
			Com_Printf("Pausing upon chat request by %s^7.\n", ent->client->pers.netname);
			forceMessage = qtrue;
		}
		else {
			// just call a vote
			Cmd_CallVote_f(ent, PAUSE_PAUSED);
		}
	}
	else if (!Q_stricmpn(chatText, "unpause", 7) && ent->client->sess.sessionTeam != TEAM_SPECTATOR && !ent->client->sess.inRacemode && mode != SAY_TELL && strlen(chatText) <= 8 && g_quickPauseChat.integer) { // allow a small typo at the end
		// unpause isn't time-sensitive, so we always call a vote
		Cmd_CallVote_f(ent, PAUSE_UNPAUSING);
		forceMessage = qtrue;
	}

	if (!force && ent->client->account && ent->client->account->flags & ACCOUNTFLAG_ENTERSPAMMER) {
		if (mode == SAY_ALL) {
			if (ent->client->pers.chatBuffer[0])
				Q_strcat(ent->client->pers.chatBuffer, sizeof(ent->client->pers.chatBuffer), " ");
			Q_strcat(ent->client->pers.chatBuffer, sizeof(ent->client->pers.chatBuffer), chatText);
			ent->client->pers.chatBufferCheckTime = trap_Milliseconds() + Com_Clampi(0, 10000, g_enterSpammerTime.integer * 1000);
			if (fixedMessage)
				free(fixedMessage);
			return;
		}
		else if (mode == SAY_TEAM && IsRacerOrSpectator(ent)) {
			if (ent->client->pers.specChatBuffer[0])
				Q_strcat(ent->client->pers.specChatBuffer, sizeof(ent->client->pers.specChatBuffer), " ");
			Q_strcat(ent->client->pers.specChatBuffer, sizeof(ent->client->pers.specChatBuffer), chatText);
			ent->client->pers.specChatBufferCheckTime = trap_Milliseconds() + Com_Clampi(0, 10000, g_enterSpammerTime.integer * 1000);
			if (fixedMessage)
				free(fixedMessage);
			return;
		}
	}

	char *newMessage = NULL;
	if (mode == SAY_ALL && *chatText == TEAMGEN_CHAT_COMMAND_CHARACTER && *(chatText + 1)) {
		if (TeamGenerator_CheckForChatCommand(ent, chatText + 1, &newMessage)) {
			if (fixedMessage)
				free(fixedMessage);
			return;
		}
		if (VALIDSTRING(newMessage))
			chatText = newMessage;
	}

	static char gaslightBuf[MAX_SAY_TEXT] = { 0 };
	if (!force && ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_HUN_GASLIGHT) && mode == SAY_ALL &&
		VALIDSTRING(chatText) && *chatText != '?' && Q_irand(1, 50) == 22 && !strstr(chatText, "://")) {
		int len = strlen(chatText);
		if (len >= 3 && len <= 140 && *(chatText + (len - 1)) != 'ú') {
			int numPlayers = 0;
			CountPlayers(&numPlayers, NULL, NULL, NULL, NULL, NULL, NULL);
			if (numPlayers >= 5) {
				static int lastGaslightTime = 0;
				int now = trap_Milliseconds();
				if (now - lastGaslightTime >= 300000) {
					lastGaslightTime = now;
					Com_sprintf(gaslightBuf, sizeof(gaslightBuf), "%sú", chatText);
					chatText = gaslightBuf;
				}
			}
		}
	}

	if (!forceMessage && IsInstapauser(ent)) {
		if (level.pause.pauserChoice) {
			SV_Tell(ent - g_entities, "You cannot chat during your own pause.");
			if (fixedMessage)
				free(fixedMessage);
			return;
		}

		char *reason = NULL;
		if (*chatText == '1') {
			level.pause.pauserChoice = 1;
			reason = "a minute or less";
			level.pause.time = level.time + 60000; // pause for 1 minutes
			level.pause.state = PAUSE_PAUSED;
		}
		else if (*chatText == '2') {
			level.pause.pauserChoice = 2;
			reason = "a couple minutes";
			level.pause.time = level.time + 120000; // pause for 2 minutes
			level.pause.state = PAUSE_PAUSED;
		}
		else if (*chatText == '3') {
			level.pause.pauserChoice = 3;
			reason = "several minutes";
			level.pause.time = level.time + 180000; // pause for 3 minutes
			level.pause.state = PAUSE_PAUSED;
		}

		if (!VALIDSTRING(reason)) {
			SV_Tell(ent - g_entities, "You cannot chat during your own pause.");
			if (fixedMessage)
				free(fixedMessage);
			return;
		}

		chatText = va("I'm terribly sorry. I unfortunately need to pause for %s. Thank you for your patience.", reason);
	}

	if (VALIDSTRING(chatText) && (!Q_stricmpn(chatText, "!who", 4) || !Q_stricmpn(chatText, "!elos", 5) || !Q_stricmpn(chatText, "!a", 2))) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *eloBotEnt = &g_entities[i];
			if (!eloBotEnt->inuse || !eloBotEnt->client || eloBotEnt->client->pers.connected == CON_DISCONNECTED || eloBotEnt == ent)
				continue;
			if (eloBotEnt->client->sess.nmVer[0] || Q_stricmpnclean(eloBotEnt->client->pers.netname, "elo BOT", 7))
				continue;

			G_SayTo(ent, ent, SAY_TELL, COLOR_MAGENTA, va("--> "EC"[%s%c%c"EC"]"EC": ", eloBotEnt->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE), chatText, NULL);
			TeamGenerator_QueueChatMessage(eloBotEnt - g_entities, ent - g_entities, "Communication with player database failed!", trap_Milliseconds() + 850 + Q_irand(-50, 150));

			if (fixedMessage)
				free(fixedMessage);

			return;
		}

		if (fixedMessage)
			free(fixedMessage);

		return;
	}

	if (VALIDSTRING(chatText) && !Q_stricmpn(chatText, "!teams", 6)) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *eloBotEnt = &g_entities[i];
			if (!eloBotEnt->inuse || !eloBotEnt->client || eloBotEnt->client->pers.connected == CON_DISCONNECTED || eloBotEnt == ent)
				continue;
			if (eloBotEnt->client->sess.nmVer[0] || Q_stricmpnclean(eloBotEnt->client->pers.netname, "elo BOT", 7))
				continue;

			G_SayTo(ent, ent, SAY_TELL, COLOR_MAGENTA, va("--> "EC"[%s%c%c"EC"]"EC": ", eloBotEnt->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE), chatText, NULL);
			if (!TeamGenerator_PrintBalance(ent, eloBotEnt))
				TeamGenerator_QueueChatMessage(eloBotEnt - g_entities, ent - g_entities, "Communication with player database failed!", trap_Milliseconds() + 850 + Q_irand(-50, 150));

			if (fixedMessage)
				free(fixedMessage);

			return;
		}

		if (fixedMessage)
			free(fixedMessage);

		return;
	}

	switch ( mode ) {
	default:
	case SAY_ALL:
		G_LogPrintf( "say: %i %s: %s\n", ent-g_entities, ent->client->pers.netname, chatText );
		Com_sprintf (name, sizeof(name), "%s%c%c%s"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, GetSuffixId( ent ) );
		color = COLOR_GREEN;
		break;
	case SAY_TEAM:
		G_LogPrintf( "sayteam: %i %s: %s\n", ent-g_entities, ent->client->pers.netname, chatText );
		if (Team_GetLocation(ent, location, sizeof(location), qtrue))
		{
			Com_sprintf (name, sizeof(name), EC"(%s%c%c%s"EC")"EC": ", 
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, GetSuffixId( ent ) );
			locMsg = location;
		}
		else
		{
			Com_sprintf (name, sizeof(name), EC"(%s%c%c%s"EC")"EC": ", 
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, GetSuffixId( ent ) );
		}
		color = COLOR_CYAN;
		break;
	case SAY_TELL:
        G_LogPrintf( "tell: %i %i %s to %s: %s\n", ent-g_entities, target-g_entities, 
            ent->client->pers.netname, target->client->pers.netname, chatText );
		Com_sprintf (name, sizeof(name), EC"[%s%c%c%s"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, GetSuffixId( ent ) );

		if (target && g_gametype.integer >= GT_TEAM &&
			target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
			Team_GetLocation(ent, location, sizeof(location), qtrue))
		{
			
			locMsg = location;
		}

		color = COLOR_MAGENTA;
		break;
	}

	qboolean doSk = qfalse;
	if ( mode == SAY_TEAM ) {
		if (VALIDSTRING(chatText) && (stristr(chatText, "$L") || stristr(chatText, "$F") || stristr(chatText, "$H")))
			doSk = qtrue;
		TokenizeTeamChat( ent, text, chatText, sizeof( text ) );
	} else {
		Q_strncpyz( text, chatText, sizeof( text ) );
	}


	if ( target ) {
		G_SayTo( ent, target, mode, color, name, text, locMsg );
		if (fixedMessage)
			free(fixedMessage);
		return;
	}

	// echo the text to the console
	// dont echo, it makes duplicated messages, already echoed with logprintf

	// send it to all the apropriate clients
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];
		G_SayTo( ent, other, mode, color, name, text, locMsg );
	}

	if (g_gametype.integer == GT_CTF && mode == SAY_TEAM && VALIDSTRING(text) && !IsRacerOrSpectator(ent) &&
		level.wasRestarted && level.pause.state == PAUSE_NONE && HasFlag(ent) && GetCTFLocationValue(ent) <= 0.55f && g_boost.integer && g_spawnboost_teamkill.integer && !ent->client->ps.fallingToDeath) {
		gentity_t *boostedBaseTeammate = NULL;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED || thisEnt == ent)
				continue;
			if (!thisEnt->client->account || !(thisEnt->client->account->flags & ACCOUNTFLAG_BOOST_SPAWNFCBOOST))
				continue;
			if (thisEnt->client->sess.sessionTeam != ent->client->sess.sessionTeam)
				continue;
			if (thisEnt->health <= 0)
				continue;
			if (GetRemindedPosOrDeterminedPos(thisEnt) != CTFPOSITION_BASE)
				continue;
			if (thisEnt->client->pers.lastSpawnTime >= level.time - 500)
				continue;
			if (thisEnt->client->ps.fd.forcePower >= 25 && (thisEnt->client->pers.lastSpawnTime >= level.time - 2000 || thisEnt->client->pers.lastForcedToSkTime >= level.time - 3000) && InFOVFloat(ent, thisEnt, 60, 60))
				continue;
			float dist = Distance(ent->client->ps.origin, thisEnt->client->ps.origin);
			if (thisEnt->client->ps.fd.forcePower >= 25 && dist <= 1800 && InFOVFloat(ent, thisEnt, 45, 60) && trap_InPVS(ent->client->ps.origin, thisEnt->client->ps.origin))
				continue;
			if (thisEnt->client->ps.fd.forcePower >= 25 && trap_InPVS(ent->client->ps.origin, thisEnt->client->ps.origin) && dist <= 512)
				continue;

			boostedBaseTeammate = thisEnt;
			break;
		}

		if (boostedBaseTeammate) {
			if (!doSk) {
				char *cleaned = strdup(text);
				Q_CleanStr(cleaned);
				if (VALIDSTRING(cleaned)) {
					if (!doSk) {
						char *TE = strstr(cleaned, "TE");
						if (TE && (!*(TE + 2) || isspace((unsigned)*(TE + 2)) || ispunct((unsigned)*(TE + 2))))
							doSk = qtrue;
					}

					if (!doSk && stristr(cleaned, "energize"))
						doSk = qtrue;

					if (!doSk) {
						char *TH = strstr(cleaned, "TH");
						if (TH && (!*(TH + 2) || isspace((unsigned)*(TH + 2)) || ispunct((unsigned)*(TH + 2))))
							doSk = qtrue;
					}
				}
				free(cleaned);
			}

			if (doSk) {
				boostedBaseTeammate->client->pers.lastForcedToSkTime = level.time;
				boostedBaseTeammate->flags &= ~FL_GODMODE;
				boostedBaseTeammate->client->ps.stats[STAT_HEALTH] = boostedBaseTeammate->health = -999;
				player_die(boostedBaseTeammate, boostedBaseTeammate, boostedBaseTeammate, 100000, MOD_SUICIDE);
			}
		}
	}

	if (fixedMessage)
		free(fixedMessage);
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f(gentity_t *ent, char *override) {
	char buffer[MAX_TOKEN_CHARS] = { 0 };
	char firstArg[MAX_TOKEN_CHARS] = { 0 };
	char *space;
	gentity_t* found = NULL;
	char *p;
	int len;

	if (ent && ent->client && !Q_stricmpnclean(ent->client->pers.netname, "elo BOT", 7))
		return;

	if (!override && trap_Argc() < 3)
	{
		trap_SendServerCommand(ent - g_entities,
			"print \"usage: tell <id/name> <message> (name can be just part of name, colors don't count)\n\"");
		return;
	}

	if (override && !override[0])
	{
		trap_SendServerCommand(ent - g_entities,
			"print \"usage: tell <id/name> <message> (name can be just part of name, colors don't count)\n\"" );
		return;
	}

	if (override && override[0])
	{
		space = strchr(override, ' ');
		if (space == NULL || !space[1])
		{
			trap_SendServerCommand(ent - g_entities,
				"print \"usage: tell <id/name> <message> (name can be just part of name, colors don't count)\n\"" );
			return;
		}
		Q_strncpyz(firstArg, override, sizeof(firstArg));
		firstArg[strlen(firstArg) - strlen(space)] = '\0';
		found = G_FindClient(firstArg);
	}
	else
	{
		trap_Argv(1, buffer, sizeof(buffer));
		found = G_FindClient(buffer);
	}

	if (!found || !found->client)
	{
		trap_SendServerCommand(
			ent - g_entities,
			va("print \"Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n\"",
				override && override[0] ? firstArg : buffer));
		return;
	}

	if (override && override[0])
	{
		p = space + 1; //need to remove the space itself
	}
	else
	{
		p = ConcatArgs(2);
	}

	/* *CHANGE 4* anti say aaaaaa for whispering */
	len = strlen(p);
	if (len > 150)
	{
		p[149] = 0;

		if (len > 255) { //report only real threats
			G_HackLog("Too long message: Client num %d (%s) from %s tried to send too long (%i chars) message (truncated: %s).\n",
				ent->client->pers.clientNum, ent->client->pers.netname, ent->client->sess.ipString, len, p);
		}
	}

	G_Say(ent, found, SAY_TELL, p, qtrue);
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if (ent != found && !(ent->r.svFlags & SVF_BOT) /*UNCOMMENT, JUST FOR DEBUG NOW*/) {
		char *fixedMessage = NULL;
		if (VALIDSTRING(p)) {
			char *found = (char *)Q_stristrclean(p, "onasi");
			if (found && !strstr(p, "://")) {
				if (!stristr(p, "onasi"))
					Q_StripColor((char *)p);
				fixedMessage = ReplaceString(p, "onasi", "hannah");
				if (VALIDSTRING(fixedMessage)) {
					if (strlen(fixedMessage) >= MAX_SAY_TEXT)
						*(fixedMessage + MAX_SAY_TEXT - 1) = '\0';
					p = fixedMessage;
				}
			}
		}
		G_SayTo(ent, ent, SAY_TELL, COLOR_MAGENTA, va("--> "EC"[%s%c%c"EC"]"EC": ", found->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE), p, NULL);
		if (fixedMessage)
			free(fixedMessage);
	}
}

static void MakeRockPaperScissorsChoice(gentity_t *ent, const char *cmd) {
	if (!ent || !ent->client || !VALIDSTRING(cmd))
		return;
	ent->client->rockPaperScissorsChoice = tolower(*cmd);
}

/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent, int mode, qboolean arg0 ) {
	char		*p;
	int len;

	if ( trap_Argc() < 2 && !arg0 ) {
		return;
	}

	if (arg0)
	{
		p = ConcatArgs( 0 );
	}
	else
	{
		p = ConcatArgs( 1 );
	}

	if ( p && strlen(p) > 2 && p[0] == '@' && p[1] == '@' )
	{
		//redirect this as a private message
		Cmd_Tell_f( ent, p + 2 );
		return;
	}

	/* *CHANGE 3* Anti say aaaaaaaaaaa */
	len = strlen(p);
	if ( len > 150 )
	{
		p[149] = 0 ;

		if (len > 255){ //report only real threats
			G_HackLog("Too long message: Client num %d (%s) from %s tried to send too long (%i chars) message (truncated: %s).\n",
                    ent->client->pers.clientNum, ent->client->pers.netname,	ent->client->sess.ipString,	len, p);
		}
	}

	if (ent->client->rockPaperScissorsOtherClientNum < MAX_CLIENTS &&
		ent->client->rockPaperScissorsStartTime && level.time - ent->client->rockPaperScissorsStartTime < ROCK_PAPER_SCISSORS_DURATION &&
		!ent->client->rockPaperScissorsBothChosenTime &&
		(!Q_stricmp(p, "r") || !Q_stricmp(p, "p") || !Q_stricmp(p, "s"))) {
		MakeRockPaperScissorsChoice(ent, p);
		return;
	}

	if (!Q_stricmp(p, "h") || !Q_stricmp(p, "heads")) {
		ent->client->pers.cointossHeadsTime = trap_Milliseconds();
		ent->client->pers.cointossTailsTime = 0;
	}
	else if (!Q_stricmp(p, "t") || !Q_stricmp(p, "tails")) {
		ent->client->pers.cointossHeadsTime = 0;
		ent->client->pers.cointossTailsTime = trap_Milliseconds();
	}

	G_Say( ent, NULL, mode, p, qfalse );
}

//siege voice command
static void Cmd_VoiceCommand_f(gentity_t *ent)
{
	gentity_t *te;
	static char arg[MAX_TOKEN_CHARS];
	char *s;
	int i = 0;

	if (g_gametype.integer < GT_TEAM)
	{
		return;
	}

	if (trap_Argc() < 2)
	{
		// list available arguments to help users bind stuff (otherwise the only way is to use assets/code)
		
		char voiceCmdList[MAX_STRING_CHARS];
		Q_strncpyz( voiceCmdList, S_COLOR_WHITE"Available voice_cmd commands:", sizeof( voiceCmdList ) );

		while ( i < MAX_CUSTOM_SIEGE_SOUNDS ) {
			if ( !bg_customSiegeSoundNames[i] ) {
				break;
			}

			if ( i % 8 == 0 ) { // line break every 8 commands
				Q_strcat( voiceCmdList, sizeof( voiceCmdList ), "\n    "S_COLOR_YELLOW );
			}

			Q_strcat( voiceCmdList, sizeof( voiceCmdList ), bg_customSiegeSoundNames[i] + 1 ); // skip the * character
			Q_strcat( voiceCmdList, sizeof( voiceCmdList ), " " );

			++i;
		}


		trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", voiceCmdList ) );
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
		ent->client->tempSpectate >= level.time)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOICECHATASSPEC")) );
		return;
	}

	trap_Argv(1, arg, sizeof(arg));

	if (arg[0] == '*')
	{ //hmm.. don't expect a * to be prepended already. maybe someone is trying to be sneaky.
		return;
	}

	s = va("*%s", arg);

	//now, make sure it's a valid sound to be playing like this.. so people can't go around
	//screaming out death sounds or whatever.
	while (i < MAX_CUSTOM_SIEGE_SOUNDS)
	{
		if (!bg_customSiegeSoundNames[i])
		{
			break;
		}
		if (!Q_stricmp(bg_customSiegeSoundNames[i], s))
		{ //it matches this one, so it's ok
			break;
		}
		i++;
	}

	if (i == MAX_CUSTOM_SIEGE_SOUNDS || !bg_customSiegeSoundNames[i])
	{ //didn't find it in the list
		return;
	}

	if ( ChatLimitExceeded( ent, -1 ) ) {
		return;
	}

	te = G_TempEntity(vec3_origin, EV_VOICECMD_SOUND);
	te->s.groundEntityNum = ent->s.number;
	te->s.eventParm = G_SoundIndex((char *)bg_customSiegeSoundNames[i]);
	te->r.svFlags |= SVF_BROADCAST;
	G_ApplyRaceBroadcastsToEvent( ent, te );
}

static char	*gc_orders[] = {
	"hold your position",
	"hold this position",
	"come here",
	"cover me",
	"guard location",
	"search and destroy",
	"report"
};

void Cmd_GameCommand_f( gentity_t *ent ) {
	int		player;
	int		order;
	static char	str[MAX_TOKEN_CHARS];

	trap_Argv( 1, str, sizeof( str ) );
	player = atoi( str );
	trap_Argv( 2, str, sizeof( str ) );
	order = atoi( str );

	if ((player >= level.maxclients && player < MAX_CLIENTS)
		||  order == sizeof(gc_orders)/sizeof(char *)){
		G_HackLog("Client %i (%s) from %s is attempting GameCommand crash.\n",
			ent-g_entities, ent->client->pers.netname,ent->client->sess.ipString);
	}

	// real version, use in release
	if ( player < 0 || player >= level.maxclients ) {
		return;
	}

	if ( order < 0 || order >= sizeof(gc_orders)/sizeof(char *) ) {
		return;
	}
	G_Say( ent, &g_entities[player], SAY_TELL, gc_orders[order], qtrue );
	G_Say( ent, ent, SAY_TELL, gc_orders[order], qtrue );
}

/*
==================
Cmd_Where_f
==================
*/
#define DotProduct2D(a,b)	((a)[0]*(b)[0]+(a)[1]*(b)[1])
extern qboolean isRedFlagstand(gentity_t *ent);
extern qboolean isBlueFlagstand(gentity_t *ent);
// gets your position relative to the flagstands
// 0 == on top of your own fs
// 1 == on top of the enemy fs
// < 0 == "behind" your fs
// > 1 == "behind" the enemy fs
float GetCTFLocationValue(gentity_t *ent) {
	if (!ent || !ent->client)
		return 0;

	static qboolean initialized = qfalse, valid = qfalse;
	static float redFs[2] = { 0, 0 }, blueFs[2] = { 0, 0 };
	if (!initialized) {
		initialized = qtrue;
		gentity_t temp;
		VectorCopy(vec3_origin, temp.r.currentOrigin);
		gentity_t *redFsEnt = G_ClosestEntity(&temp, isRedFlagstand);
		gentity_t *blueFsEnt = G_ClosestEntity(&temp, isBlueFlagstand);
		if (redFsEnt && blueFsEnt) {
			valid = qtrue;
			redFs[0] = redFsEnt->r.currentOrigin[0];
			redFs[1] = redFsEnt->r.currentOrigin[1];
			blueFs[0] = blueFsEnt->r.currentOrigin[0];
			blueFs[1] = blueFsEnt->r.currentOrigin[1];
		}
	}

	if (!valid)
		return 0;

	float *allyFs, *enemyFs;
	if (ent->client->sess.sessionTeam == TEAM_RED) {
		allyFs = &redFs[0];
		enemyFs = &blueFs[0];
	}
	else {
		allyFs = &blueFs[0];
		enemyFs = &redFs[0];
	}

	float rPrime[2] = { ent->r.currentOrigin[0] - allyFs[0], ent->r.currentOrigin[1] - allyFs[1] };
	float v[2] = { enemyFs[0] - allyFs[0], enemyFs[1] - allyFs[1] };
	return DotProduct2D(v, rPrime) / DotProduct2D(v, v);
}

void Cmd_Where_f( gentity_t *ent ) {
	if (!ent->client)
		return;

	char *extra = "";
	if (g_gametype.integer == GT_CTF) {
#ifdef _DEBUG
		extra = va("\nLocation value: %.3f", GetCTFLocationValue(ent));
#endif
		static qboolean initialized = qfalse, valid = qfalse;
		static float redFs[2] = { 0, 0 }, blueFs[2] = { 0, 0 };
		if (!initialized) {
			initialized = qtrue;
			gentity_t temp;
			VectorCopy(vec3_origin, temp.r.currentOrigin);
			gentity_t *redFsEnt = G_ClosestEntity(&temp, isRedFlagstand);
			gentity_t *blueFsEnt = G_ClosestEntity(&temp, isBlueFlagstand);
			if (redFsEnt && blueFsEnt) {
				valid = qtrue;
				redFs[0] = redFsEnt->r.currentOrigin[0];
				redFs[1] = redFsEnt->r.currentOrigin[1];
				blueFs[0] = blueFsEnt->r.currentOrigin[0];
				blueFs[1] = blueFsEnt->r.currentOrigin[1];
			}
		}
		if (valid) {
			extra = va("%s\nDistance between flagstands: %d", extra, (int)Distance2D(redFs, blueFs));
		}
	}

	trap_SendServerCommand( ent - g_entities, va( "print \"Origin: %s ; Yaw: %.3f degrees%s\n\"", vtos( ent->client->ps.origin ), ent->client->ps.viewangles[YAW], extra ) );
}

#ifdef _DEBUG
void Cmd_When_f(gentity_t *ent) {
	PrintIngame(ent - g_entities, "trap_Milliseconds(): %d. level.time: %d. level.startTime: %d. level.time - level.startTime: %d.\n",
		trap_Milliseconds(), level.time, level.startTime, level.time - level.startTime);
}
#endif

static const char *gameNames[] = {
	"Free For All",
	"Holocron FFA",
	"Jedi Master",
	"Duel",
	"Power Duel",
	"Single Player",
	"Team FFA",
	"Siege",
	"Capture the Flag",
	"Capture the Ysalamiri"
};

/*
==================
G_ClientNumberFromName

Finds the client number of the client with the given name
==================
*/
int G_ClientNumberFromName ( const char* name )
{
	static char		s2[MAX_STRING_CHARS];
	static char		n2[MAX_STRING_CHARS];
	int			i;
	gclient_t*	cl;

	// check for a name match
	SanitizeString( (char*)name, s2 );
	for ( i=0, cl=level.clients ; i < level.numConnectedClients ; i++, cl++ ) 
	{
		SanitizeString( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) 
		{
			return i;
		}
	}

	return -1;
}

/*
==================
SanitizeString2

Rich's revised version of SanitizeString
==================
*/
void SanitizeString2( char *in, char *out )
{
	int i = 0;
	int r = 0;

	while (in[i])
	{
		if (i >= MAX_NAME_LENGTH-1)
		{ //the ui truncates the name here..
			break;
		}

		if (in[i] == '^')
		{
			if (in[i+1] >= 48 && //'0'
				in[i+1] <= 57) //'9'
			{ //only skip it if there's a number after it for the color
				i += 2;
				continue;
			}
			else
			{ //just skip the ^
				i++;
				continue;
			}
		}

		if (in[i] < 32)
		{
			i++;
			continue;
		}

		out[r] = in[i];
		r++;
		i++;
	}
	out[r] = 0;
}

/*
==================
G_ClientNumberFromStrippedName

Same as above, but strips special characters out of the names before comparing.
==================
*/
int G_ClientNumberFromStrippedName ( const char* name )
{
	static char		s2[MAX_STRING_CHARS];
	static char		n2[MAX_STRING_CHARS];
	int			i;
	gclient_t*	cl;

	// check for a name match
	SanitizeString2( (char*)name, s2 );
	for ( i=0, cl=level.clients ; i < level.numConnectedClients ; i++, cl++ ) 
	{
		SanitizeString2( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) 
		{
			return i;
		}
	}

	return -1;
}

//MapPool pools[64];
//int poolNum = 0;
//
//void G_LoadMapPool(const char* filename)
//{
//	int				len;
//	fileHandle_t	f;
//	int             map;
//	static char		buf[MAX_POOLS_TEXT];
//	char			*line;
//
//	if (poolNum == MAX_MAPS_IN_POOL)
//		return;
//
//	if (strlen(filename) > MAX_MAP_POOL_ID)
//		return;
//
//	// open map pool file
//	len = trap_FS_FOpenFile(filename, &f, FS_READ);
//	if (!f) {
//		trap_Printf(va(S_COLOR_RED "file not found: %s\n", filename));
//		return;
//	}
//
//	if (len >= MAX_POOLS_TEXT) {
//		trap_Printf(va(S_COLOR_RED "file too large: %s is %i, max allowed is %i", filename, len, MAX_ARENAS_TEXT));
//		trap_FS_FCloseFile(f);
//		return;
//	}
//
//	trap_FS_Read(buf, len, f);
//	buf[len] = 0;
//	trap_FS_FCloseFile(f);
//
//	// store file name as an id
//	COM_StripExtension(filename, pools[poolNum].id);
//
//	line = strtok(buf, "\r\n");
//	if (!line)
//	{
//		Com_Printf("Pool %s is corrupted\n", filename);
//		return;
//	}
//
//	// long name for votes	
//	Q_strncpyz(pools[poolNum].longname, line, MAX_MAP_POOL_LONGNAME);
//
//	// cycle through all arenas
//	map = 0;
//	while ( (line = strtok(0, "\r\n")) )
//	{
//		const char* error;
//		Q_strncpyz(pools[poolNum].maplist[map], line, MAX_MAP_NAME);
//
//		// check the arena validity (exists and supports current gametype)
//		// TBD optimize arena search, hash map perhaps
//		error = G_DoesMapSupportGametype(pools[poolNum].maplist[map], g_gametype.integer);
//		if (!error)
//		{
//			++map;
//		}
//		else
//		{
//		}
//	}
//
//	pools[poolNum].mapsCount = map;
//
//	Com_Printf("Loaded map pool %s, maps %i\n", filename, pools[poolNum].mapsCount);
//
//	++poolNum;
//}

//void G_LoadVoteMapsPools()
//{
//	int			numdirs;
//	char		filename[128];
//	char		dirlist[4096];
//	char*		dirptr;
//	int			dirlen;
//	int         i;
//
//	// load all .pool files
//	numdirs = trap_FS_GetFileList("", ".pool", dirlist, sizeof(dirlist));
//	dirptr = dirlist;
//
//	for (i = 0; i < numdirs; i++, dirptr += dirlen + 1)
//	{
//		dirlen = strlen(dirptr);
//		strcpy(filename, dirptr);
//		G_LoadMapPool(filename);
//	}
//
//}

void fixVoters(qboolean allowRacers, int onlyThisTeamCanVote){
	int i;

	level.numVotingClients = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		level.clients[i].mGameFlags &= ~PSG_VOTED;
		level.clients[i].mGameFlags &= ~PSG_VOTEDNO;
		level.clients[i].mGameFlags &= ~PSG_VOTEDYES;
        level.clients[i].mGameFlags &= ~PSG_CANVOTE;
        level.clients[i].mGameFlags &= ~PSG_CALLEDVOTE;
	}

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected != CON_DISCONNECTED ) {
			// only let racers vote if specified
			if ( !allowRacers && level.clients[i].sess.inRacemode ) {
				continue;
			}

			if ( level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL ) {
				if (g_entities[i].r.svFlags & SVF_BOT)
					continue;

				if (onlyThisTeamCanVote && g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_SIEGE && !Q_stricmp(level.voteString, "endmatch") && level.clients[i].sess.sessionTeam != onlyThisTeamCanVote)
					continue;

				level.clients[i].mGameFlags |= PSG_CANVOTE;
				level.numVotingClients++;
			}
		}
	}
}

void CountPlayersIngame( int *total, int *ingame, int *inrace ) {
	gentity_t *ent;
	int i;

	// total = all non bot, fully connected clients ; ingame = non spectators and non racemode
	if ( total ) *total = 0;
	if ( ingame ) *ingame = 0;
	if ( inrace ) *inrace = 0;

	for ( i = 0; i < level.maxclients; i++ ) { // count clients that are connected and who are not bots
		ent = &g_entities[i];

		if ( !ent || !ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || ent->r.svFlags & SVF_BOT ) {
			continue;
		}

		if ( total ) ( *total )++;

		if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
			if ( !ent->client->sess.inRacemode ) {
				if ( ingame ) ( *ingame )++;
			} else {
				if ( inrace ) ( *inrace )++;
			}
		}
	}
}

/*
==================
Cmd_CallVote_f
==================
*/
extern void SiegeClearSwitchData(void); //g_saga.c
extern const char *G_GetArenaInfoByMap( const char *map );
int G_GetArenaNumber( const char *map );

static int      g_votedCounts[MAX_ARENAS];
void Cmd_Vote_f(gentity_t *ent, const char *forceVoteArg);

void Cmd_CallVote_f( gentity_t *ent, int pause ) {
	int		i;
	static char	arg1[MAX_STRING_TOKENS];
	char	arg2[MAX_CVAR_VALUE_STRING];
	char*		mapName = 0;
	const char*	arenaInfo;
    int argc;

	trap_Argv(1, arg1, sizeof(arg1));
	trap_Argv(2, arg2, sizeof(arg2));
	argc = trap_Argc();

	if (pause) {
		Q_strncpyz(arg1, pause == PAUSE_PAUSED ? "pause" : "unpause", sizeof(arg1));
		arg2[0] = '\0';
	}

	if ( !g_allowVote.integer ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")) );
		return;
	}

	if ( level.voteExecuteTime >= level.time ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEINPROGRESS")) );
		return;
	}

	if (level.voteTime) {
		// if they try to call a map_restart vote while a map_restart vote is already pending, just vote yes on it
		if (!Q_stricmpn(arg1, "map_restart", 11) && (!Q_stricmpn(level.voteString, "map_restart", 11) || !Q_stricmpn(level.voteString, "auto_restart", 12)))
			Cmd_Vote_f(ent, "yes");
		else
			trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEINPROGRESS")));
		return;
	}

	//check for delays
	if (level.startTime + g_callvotedelay.integer*1000 > level.time){
		trap_SendServerCommand( ent-g_entities, 
			va("print \"Cannot call vote %i seconds after map start.\n\"",g_callvotedelay.integer)  );
		return;
	}

	if (ent->client->lastCallvoteTime + g_callvotedelay.integer*1000 > level.time){
		trap_SendServerCommand( ent-g_entities, 
			va("print \"Cannot call vote %i seconds after previous attempt.\n\"",g_callvotedelay.integer)  );
		return;
	}

	if (ent->client->lastJoinedTime + g_callvotedelay.integer*1000 > level.time){
		trap_SendServerCommand( ent-g_entities, 
			va("print \"Cannot call vote %i seconds after joining the game.\n\"",g_callvotedelay.integer)  );
		return;
	}

	if (g_gametype.integer != GT_DUEL &&
		g_gametype.integer != GT_POWERDUEL)
	{
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")) );
			return;
		}
	}

	// make sure it is a valid command to vote on

	// *CHANGE 8a* anti callvote bug
	if ((g_protectCallvoteHack.integer && (strchr( arg1, '\n' ) || strchr( arg2, '\n' ) ||	strchr( arg1, '\r' ) || strchr( arg2, '\r' ))) ) {
		//lets replace line breaks with ; for better readability
		int j, len;
		len = strlen(arg1);
		for(j = 0; j < len; ++j){
			if(arg1[j]=='\n' || arg1[j]=='\r') 
				arg1[j] = ';';
		}

		len = strlen(arg2);
		for(j = 0; j < len; ++j){
			if(arg2[j]=='\n' || arg2[j]=='\r') 
				arg2[j] = ';';
		}

		G_HackLog("Callvote hack: Client num %d (%s) from %s tries to hack via callvote (callvote %s \"%s\").\n",
                ent->client->pers.clientNum, 
				ent->client->pers.netname,
				ent->client->sess.ipString,
                arg1, arg2);
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	if( strchr( arg1, ';'  ) || strchr( arg2, ';'  )){
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	if (!Q_stricmp(arg1, "nextmap") && !g_allow_vote_nextmap.integer &&
		g_redirectNextMapVote.string[0] && Q_stricmp(g_redirectNextMapVote.string, "0")) {

		Q_strncpyz(arg1, g_redirectNextMapVote.string, sizeof(arg1));
		char *space = strchr(arg1, ' ');
		if (space && *(space + 1)) { // everything after the first space is arg2
			*space = '\0';
			Q_strncpyz(arg2, space + 1, sizeof(arg2));
		}
		else {
			arg2[0] = '\0';
		}
	}
	else if (!Q_stricmp(arg1, "g_doWarmup") && !g_allow_vote_warmup.integer &&
		g_redirectDoWarmupVote.string[0] && Q_stricmp(g_redirectDoWarmupVote.string, "0")) {

		Q_strncpyz(arg1, g_redirectDoWarmupVote.string, sizeof(arg1));
		char *space = strchr(arg1, ' ');
		if (space && *(space + 1)) { // everything after the first space is arg2
			*space = '\0';
			Q_strncpyz(arg2, space + 1, sizeof(arg2));
		}
		else {
			arg2[0] = '\0';
		}
	}

	// racers can only call a very limited set of votes
	qboolean racersAllowVote = qfalse, racersAllowCallvote = qfalse;

	if ( !Q_stricmp( arg1, "map_restart" ) ) {
		racersAllowVote = qtrue;
		racersAllowCallvote = qtrue;
	} else if ( !Q_stricmp( arg1, "nextmap" ) ) {
		racersAllowVote = qtrue;
	} else if ( !Q_stricmp( arg1, "map" ) ) {
		racersAllowVote = qtrue;
		racersAllowCallvote = qtrue;
	} else if ( !Q_stricmp( arg1, "map_random" ) ) {
		racersAllowVote = qtrue;
	} else if ( !Q_stricmp( arg1, "mapvote" ) ) {
		racersAllowVote = qtrue;
	} else if ( !Q_stricmp( arg1, "g_gametype" ) ) {
		racersAllowVote = qtrue;
	} else if ( !Q_stricmp( arg1, "kick" ) ) {
		racersAllowVote = qtrue;
	} else if ( !Q_stricmp( arg1, "clientkick" ) ) {
		racersAllowVote = qtrue;
	} else if ( !Q_stricmp( arg1, "g_doWarmup" ) ) {
	} else if ( !Q_stricmp( arg1, "timelimit" ) ) {
	} else if ( !Q_stricmp( arg1, "fraglimit" ) ) {
	} else if ( !Q_stricmp( arg1, "resetflags" ) ) { //flag reset when they disappear (debug)
	} else if ( !Q_stricmp( arg1, "q" ) ) { //general question vote :)
		racersAllowVote = qtrue;
	} else if ( !Q_stricmp( arg1, "pause" ) ) {
	} else if ( !Q_stricmp( arg1, "unpause" ) ) { 
	} else if ( !Q_stricmp( arg1, "endmatch" ) ) {
	} else if ( !Q_stricmp ( arg1, "lockteams") ) {
	} else if ( !Q_stricmp( arg1, "cointoss")) {
    } else if ( !Q_stricmp( arg1, "randomcapts")) {
	} else if ( !Q_stricmp( arg1, "randomteams" ) ) {
	} else if ( !Q_stricmp( arg1, "capturedifflimit" ) ) {
	} else if ( !Q_stricmp( arg1, "enableboon" ) ) {
	} else if ( !Q_stricmp( arg1, "disableboon" ) ) {
	} else if ( !Q_stricmp( arg1, "ffa" ) ) {
	} else if ( !Q_stricmp( arg1, "duel" ) ) {
	} else if ( !Q_stricmp( arg1, "powerduel" ) ) {
	} else if ( !Q_stricmp( arg1, "tffa" ) ) {
	} else if ( !Q_stricmp( arg1, "siege" ) ) {
	} else if ( !Q_stricmp( arg1, "ctf" ) ) {
	} else if ( !Q_stricmp( arg1, "instagib" ) ) {
	} else {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap_SendServerCommand( ent-g_entities, "print \"Vote commands are: map_restart, nextmap, map <mapname>, g_gametype <n>, "
			"kick <player>, clientkick <clientnum>, g_doWarmup, timelimit <time>, fraglimit <frags>, "
			"resetflags, q <question>, pause, unpause, endmatch, randomcapts, randomteams <numRedPlayers> <numBluePlayers>, "
			"lockteams <numPlayers>, capturedifflimit <capLimit>, enableboon, disableboon, instagib, map_random, mapvote.\n\"" );
		return;
	}

	// cancel if this vote is not allowed in racemode and this is a racer
	if ( !racersAllowCallvote && ent->client->sess.inRacemode ) {
		trap_SendServerCommand( ent - g_entities, "print \"Cannot call this vote while in racemode.\n\"" );
		trap_SendServerCommand( ent - g_entities, "print \"Allowed commands are: map_restart, map <mapname>.\n\"" );

		return;
	}

	int clientsTotal, clientsInGame, clientsInRace;
	CountPlayersIngame( &clientsTotal, &clientsInGame, &clientsInRace );

	// racer majority is limited to 3 ig players at most
	qboolean racersHaveMajorityOrEqual = qfalse;
	if ( clientsInGame < 4 && clientsInRace >= clientsInGame ) {
		racersHaveMajorityOrEqual = qtrue;
	}

	if ( !racersHaveMajorityOrEqual ) {
		racersAllowVote = qfalse;
		racersAllowCallvote = qfalse;
	}

	// let them know if they can't callvote due to not having majority
	if ( !racersAllowCallvote && ent->client->sess.inRacemode ) {
		trap_SendServerCommand( ent - g_entities, "print \"Cannot call this vote due to too many in game players.\n\"" );

		return;
	}

	// if there is still a vote to be executed
	if ( level.voteExecuteTime ) {
		level.voteExecuteTime = 0;

		//special fix for siege status
		if (!Q_strncmp(level.voteString,"vstr nextmap",sizeof(level.voteString))){
			SiegeClearSwitchData();
		}

		if (!Q_stricmpn(level.voteString,"map_restart",11)){
			trap_Cvar_Set("g_wasRestarted", "1");
		}

		trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.voteString ) );
	}

	// allow e.g. /callvote ctf as an alias of /callvote g_gametype 8
	if (!Q_stricmp(arg1, "ffa")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "0", sizeof(arg2));
	}
	else if (!Q_stricmp(arg1, "duel")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "3", sizeof(arg2));
	}
	else if (!Q_stricmp(arg1, "powerduel")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "4", sizeof(arg2));
	}
	else if (!Q_stricmp(arg1, "tffa")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "6", sizeof(arg2));
	}
	else if (!Q_stricmp(arg1, "siege")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "7", sizeof(arg2));
	}
	else if (!Q_stricmp(arg1, "ctf")) {
		Q_strncpyz(arg1, "g_gametype", sizeof(arg1));
		Q_strncpyz(arg2, "8", sizeof(arg2));
	}

	int onlyThisTeamCanVote = 0;

	// special case for g_gametype, check for bad values
	if ( !Q_stricmp( arg1, "g_gametype" ) )
	{
		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_gametype.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Voting for any gametype is disabled.\n\"" );
			return;
		}

		i = atoi( arg2 );

		if( i == GT_SINGLE_PLAYER || i < GT_FFA || i >= GT_MAX_GAME_TYPE) {
			trap_SendServerCommand( ent-g_entities, "print \"Invalid gametype.\n\"" );
			return;
		}

		//check for specific gametype
		if (!(g_allow_vote_gametype.integer & (1 << i))){
			trap_SendServerCommand( ent-g_entities, "print \"Voting for this gametype is disabled.\n\"" );
			return;
		}

		level.votingGametype = qtrue;
		level.votingGametypeTo = i;

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %d", arg1, i );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1, gameNames[i] );
	}
	else if ( !Q_stricmp( arg1, "map" ) ) 
	{
		// special case for map changes, we want to reset the nextmap setting
		// this allows a player to change maps, but not upset the map rotation
		char	s[MAX_STRING_CHARS];
		const char    *result; //only constant strings are used
        int arenaNumber;

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_map.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote map is disabled.\n\"" );
			return;
		}

		if ( g_mapVoteThreshold.integer ) { // anti force map protection when the server is populated
			if ( clientsTotal >= g_mapVoteThreshold.integer && clientsInGame < g_mapVotePlayers.integer ) {
				trap_SendServerCommand( ent - g_entities, "print \"Not enough players in game to call this vote.\n\"" );
				return;
			}
		}

		if (arg2[0]) {
			char *filter = stristr(arg2, "sertao");
			if (filter) {
				filter += 4;
				if (*filter == 'a')
					*filter = 'ã';
				else if (*filter == 'A')
					*filter = 'Ã';
			}
		}

		char liveVersion[MAX_QPATH] = { 0 };
		if (g_vote_redirectMapVoteToLiveVersion.integer && Q_stricmpn(arg2, "mp/", 3) &&
			G_DBGetLiveMapFilenameForAlias(arg2, liveVersion, sizeof(liveVersion)) && liveVersion[0]) {
			Q_strncpyz(arg2, liveVersion, sizeof(arg2));
		}

		result = G_DoesMapSupportGametype(arg2, trap_Cvar_VariableIntegerValue("g_gametype"));
		if (result)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", result) );
			return;
		}

		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof(s) );
		if (*s) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"; set nextmap \"%s\"", arg1, arg2, s );
		} else {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"", arg1, arg2 );
		}
		
        arenaNumber = G_GetArenaNumber(arg2);
		arenaInfo	= G_GetArenaInfoByMap(arg2);
		if (arenaInfo)
		{
			mapName = Info_ValueForKey(arenaInfo, "longname");
		}

		if (!mapName || !mapName[0])
		{
			mapName = "ERROR";
		}   
        
        if ( (arenaNumber >= 0) && g_callvotemaplimit.integer)
        {
            if ( (g_votedCounts[arenaNumber] >= g_callvotemaplimit.integer))
            {
                trap_SendServerCommand( ent-g_entities, "print \"Map has been callvoted too many times, try another map.\n\"" );
                return;
            } 
            g_votedCounts[arenaNumber]++;
        }    
       
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "map %s", mapName);
	}
	else if (!Q_stricmp(arg1, "mapvote")) {
		if (!g_allow_vote_mapvote.integer || !g_vote_tierlist.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Mapvote is disabled.\n\"");
			return;
		}

		if (g_mapVoteThreshold.integer && clientsTotal >= g_mapVoteThreshold.integer && clientsInGame < g_mapVotePlayers.integer) {
			// anti force map protection when the server is populated
			trap_SendServerCommand(ent - g_entities, "print \"Not enough players in game to call this vote.\n\"");
			return;
		}

		const int totalMapsToChoose = Com_Clampi(2, MAX_MULTIVOTE_MAPS, g_vote_tierlist_totalMaps.integer);
		if ((g_vote_tierlist_s_max.integer + g_vote_tierlist_a_max.integer + g_vote_tierlist_b_max.integer +
			g_vote_tierlist_c_max.integer + g_vote_tierlist_f_max.integer < totalMapsToChoose) ||
			(g_vote_tierlist_s_min.integer + g_vote_tierlist_a_min.integer + g_vote_tierlist_b_min.integer +
				g_vote_tierlist_c_min.integer + g_vote_tierlist_f_min.integer > totalMapsToChoose)) {
			// we could just decrease the max, but let's just shame the idiots instead
			PrintIngame(ent - g_entities, "This server is not properly configured for tierlist voting. Please notify the admins.\n");
			Com_Printf("Error: min/max conflict with g_vote_tierlist_totalMaps! Unable to use tierlist voting.\n");
			return;
		}

		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "New Map Vote (%d Maps)", totalMapsToChoose);
		Com_sprintf(level.voteString, sizeof(level.voteString), "mapvote");
	}
	else if (!Q_stricmp(arg1, "map_random"))
	{
		if (!g_allow_vote_maprandom.integer){
			trap_SendServerCommand(ent - g_entities, "print \"Vote map_random is disabled.\n\"");
			return;
		}

		// the cvar value is the amount of maps to randomize up to 5 (so 1 is the old, instant random map behavior)
		int mapsToRandomize = Com_Clampi( 1, 5, g_allow_vote_maprandom.integer );

		if ( g_mapVoteThreshold.integer && clientsTotal >= g_mapVoteThreshold.integer && clientsInGame < g_mapVotePlayers.integer ) {
			// anti force map protection when the server is populated
			trap_SendServerCommand( ent - g_entities, "print \"Not enough players in game to call this vote.\n\"" );
			return;
		}

		if ( clientsInGame < 2 ) {
			mapsToRandomize = 1; // not enough clients for a multi vote, just randomize 1 map right away
		}

		// hardcode default to custom_4s for now
		if (!*arg2) {
			Q_strncpyz(arg2, "custom_4s", sizeof(arg2));
		}

        PoolInfo poolInfo;
        if ( !G_DBFindPool( arg2, &poolInfo ) )
        {
            trap_SendServerCommand( ent - g_entities, "print \"Pool not found.\n\"" );
            return;
        }

		if ( mapsToRandomize >= 2 ) {
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Randomize %d Maps: %s", mapsToRandomize, poolInfo.long_name );
		} else {
			Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Random Map: %s", poolInfo.long_name );
		}

		Com_sprintf( level.voteString, sizeof( level.voteString ), "map_random %s %d", arg2, mapsToRandomize ); // write map_random directly because it can come from a nextmap vote

		//// find the pool
		//for (i = 0; i < poolNum; ++i)
		//{
		//	if (!Q_stricmpn(pools[i].id, arg2, MAX_MAP_POOL_ID))
		//	{
		//		found = qtrue;
		//		break;
		//	}
		//}

		//if (!found)
		//{
		//	trap_SendServerCommand(ent - g_entities, "print \"Pool not found.\n\"");
		//	return;
		//}

		//Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Random Map: %s", pools[i].longname);
		//Com_sprintf(level.voteString, sizeof(level.voteString), "%s %s", arg1, arg2);
	}
	else if ( !Q_stricmp ( arg1, "clientkick" ) )
	{
		int n = atoi ( arg2 );

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_kick.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote kick is disabled.\n\"" );
			return;
		}

		if (ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_VOTETROLL)) {
			trap_SendServerCommand(ent - g_entities, "print \"Vote kick is disabled.\n\"");
			return;
		}

		if ( n < 0 || n >= MAX_CLIENTS )
		{
			trap_SendServerCommand( ent-g_entities, va("print \"invalid client number %d.\n\"", n ) );
			return;
		}

		if ( g_entities[n].client->pers.connected == CON_DISCONNECTED )
		{
			trap_SendServerCommand( ent-g_entities, va("print \"there is no client with the client number %d.\n\"", n ) );
			return;
		}
			
		Com_sprintf ( level.voteString, sizeof(level.voteString ), "%s %i", arg1, n );
		Com_sprintf ( level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s%s", NM_SerializeUIntToColor(n), g_entities[n].client->pers.netname );
	}
	else if ( !Q_stricmp ( arg1, "kick" ) )
	{
		int clientid = G_ClientNumberFromName ( arg2 );

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_kick.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote kick is disabled.\n\"" );
			return;
		}

		if (ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_VOTETROLL)) {
			trap_SendServerCommand(ent - g_entities, "print \"Vote kick is disabled.\n\"");
			return;
		}

		if ( clientid == -1 )
		{
			clientid = G_ClientNumberFromStrippedName(arg2);

			if (clientid == -1)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"there is no client named '%s' currently on the server.\n\"", arg2 ) );
				return;
			}
		}

		Com_sprintf ( level.voteString, sizeof(level.voteString ), "clientkick %d", clientid );
		Com_sprintf ( level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s%s", NM_SerializeUIntToColor(clientid), g_entities[clientid].client->pers.netname );
	}
	else if ( !Q_stricmp( arg1, "nextmap" ) ) 
	{
		char	s[MAX_STRING_CHARS];

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_nextmap.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote nextmap is disabled.\n\"" );
			return;
		}

		trap_Cvar_VariableStringBuffer( "nextmap", s, sizeof(s) );
		if (!*s) {
			trap_SendServerCommand( ent-g_entities, "print \"nextmap not set.\n\"" );
			return;
		}
		Com_sprintf( level.voteString, sizeof( level.voteString ), "vstr nextmap");
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	} 
	else if ( !Q_stricmp( arg1, "timelimit" )) 
	{
		int n = atoi(arg2);

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_timelimit.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote timelimit is disabled.\n\"" );
			return;
		}

		if (n < 0)
			n = 0;

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%i\"", arg1, n );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );

	} 
	else if ( !Q_stricmp( arg1, "fraglimit" )) 
	{
		int n = atoi(arg2);

		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_fraglimit.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote fraglimit is disabled.\n\"" );
			return;
		}

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );

	} 
	else if ( !Q_stricmp( arg1, "map_restart" )) 
	{
		// *CHANGE 22* - vote disabling
		if (!g_allow_vote_restart.integer){
			trap_SendServerCommand( ent-g_entities, "print \"Vote map_restart is disabled.\n\"" );
			return;
		}
		
		if (g_waitForAFK.integer && g_gametype.integer == GT_CTF) {
			if (level.autoStartPending) {
				Com_sprintf(level.voteString, sizeof(level.voteString), "auto_restart_cancel");
				Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Cancel Pending Map Restart");
			}
			else {
				Com_sprintf(level.voteString, sizeof(level.voteString), "auto_restart");
				Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "map_restart");
			}
		}
		else {
			Com_sprintf(level.voteString, sizeof(level.voteString), "map_restart \"%i\"", Com_Clampi(0, 60, g_restart_countdown.integer));
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
		}
	} 
	else if (!Q_stricmp(arg1, "allready"))
	{
		if (!level.warmupTime) {
			trap_SendServerCommand(ent - g_entities, "print \"allready is only available during warmup.\n\"");
			return;
		}

		Com_sprintf(level.voteString, sizeof(level.voteString), "%s", arg1);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
	}
	else if (!Q_stricmp(arg1, "cointoss"))
	{
		Com_sprintf(level.voteString, sizeof(level.voteString), "cointoss"); // write cointoss directly because it can come from a warmup vote
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Coin Toss");
	}
    else if (!Q_stricmp(arg1, "randomcapts"))
    {
        Com_sprintf(level.voteString, sizeof(level.voteString), "%s", arg1);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Random capts");
    }
    else if (!Q_stricmp(arg1, "randomteams"))
    {
        int team1Count, team2Count;
        char count[2];

        trap_Argv(2, count, sizeof(count));
        team1Count = atoi(count);

        trap_Argv(3, count, sizeof(count));
        team2Count = atoi(count);

        Com_sprintf(level.voteString, sizeof(level.voteString), "%s %i %i", arg1, team1Count, team2Count);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Random Teams - %i vs %i", team1Count, team2Count);
    }
	else if ( !Q_stricmp( arg1, "resetflags" )) 
	{
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s", arg1 );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Reset Flags" );
	}
	else if ( !Q_stricmp( arg1, "q" )) 
	{
	if (!arg2[0] || isspace(arg2[0])) {
		trap_SendServerCommand(ent - g_entities, "print \"You must specify a poll question.\n\"");
		return;
	}

	if (IsInstapauser(ent)) {
		PrintIngame(ent - g_entities, "You cannot call this vote during your own pause.\n");
		return;
	}

		NormalizeName(ConcatArgs(2), arg2, sizeof(arg2), sizeof(arg2));
		PurgeStringedTrolling(arg2, arg2, sizeof(arg2));

		if (g_filterSlurs.integer && level.slurList.size > 0 && HasSlur(arg2)) {
			G_LogPrintf("Filtered slur poll from player %d (%s): %s\n", ent - g_entities, ent->client ? ent->client->pers.netname : "", arg2);
			return;
		}

		Com_sprintf( level.voteString, sizeof( level.voteString ), ";" );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Poll: %s", arg2 );
	} 
	else if ( !Q_stricmp( arg1, "pause" )) 
	{
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s 150", arg1);
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Pause Game" );

	}
	else if ( !Q_stricmp( arg1, "unpause" )) 
	{
		if (level.pause.state == PAUSE_NONE) {
			trap_SendServerCommand(ent - g_entities, "print \"The game is not currently paused.\n\"");
			return;
		}
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s", arg1 );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Unpause Game" );
	}
	else if ( !Q_stricmp( arg1, "endmatch" )) 
	{
		int numRed, numBlue;
		CountPlayers(NULL, &numRed, &numBlue, NULL, NULL, NULL, NULL);
		if (g_losingTeamEndmatchTeamvote.integer > 0 && numRed >= 3 && numBlue >= 3 && numRed == numBlue && level.teamScores[TEAM_RED] != level.teamScores[TEAM_BLUE] &&
			abs(level.teamScores[TEAM_RED] - level.teamScores[TEAM_BLUE]) >= g_losingTeamEndmatchTeamvote.integer &&
			g_gametype.integer >= GT_TEAM && g_gametype.integer != GT_SIEGE) {

			const int losingTeam = level.teamScores[TEAM_RED] < level.teamScores[TEAM_BLUE] ? TEAM_RED : TEAM_BLUE;
			if (ent->client->sess.sessionTeam != losingTeam) {
				if (g_losingTeamEndmatchTeamvote.integer == 1)
					PrintIngame(ent - g_entities, "You cannot call endmatch votes while winning.\n");
				else
					PrintIngame(ent - g_entities, "You cannot call endmatch votes while winning by %d+ points.\n", g_losingTeamEndmatchTeamvote.integer);

				return;
			}

			Com_sprintf(level.voteString, sizeof(level.voteString), "%s", arg1);

			const char *losingTeamStr = losingTeam == TEAM_RED ? "^1red" : "^4blue";
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "End Match (only %s^7 team can vote)", losingTeamStr);

			onlyThisTeamCanVote = losingTeam;
		}
		else {
			Com_sprintf(level.voteString, sizeof(level.voteString), "%s", arg1);
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "End Match");
		}
	}
	else if ( !Q_stricmp( arg1, "lockteams" ) )
	{
		if (ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_VOTETROLL)) {
			trap_SendServerCommand(ent - g_entities, "print \"Lock teams is disabled.\n\"");
			return;
		}

		// hacky param whitelist but we aren't going to do any parsing anyway
		if ( argc >= 3 && ( !Q_stricmp( arg2, "0" ) || !Q_stricmpn( arg2, "r", 1 )
			|| !Q_stricmp( arg2, "4s" ) || !Q_stricmp( arg2, "5s" ) ) ) {
			Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
			
			if ( !Q_stricmp( arg2, "0" ) || !Q_stricmpn( arg2, "r", 1 ) ) {
				Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Unlock Teams" );
			} else {
				Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Lock Teams - %s", arg2 );
			}
		} else {
			trap_SendServerCommand( ent - g_entities, "print \"usage: /callvote lockteams 4s/5s/reset\n\"" );
			return;
		}
	}
	else if ( !Q_stricmp( arg1, "capturedifflimit" ) )
	{
		int n = Com_Clampi( 0, 100, atoi( arg2 ) );

		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%i\"", arg1, n );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "Capture Diff Limit: %i", n );
	}
	else if ( !Q_stricmp( arg1, "enableboon" ) )
	{
		if (!g_allow_vote_boon.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Vote enableboon is disabled.\n\"");
			return;
		}

		if (g_enableBoon.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Boon is already enabled.\n\"");
			return;
		}

		// Use the value of g_allow_vote_boon for g_enableboon so admins can switch between different behaviors
		Com_sprintf(level.voteString, sizeof(level.voteString), "g_enableBoon %d;svsay "S_COLOR_CYAN"Boon will be enabled next map_restart.", g_allow_vote_boon.integer);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Enable Boon", level.voteString);
	}
	else if ( !Q_stricmp( arg1, "disableboon" ) )
	{
		if (!g_allow_vote_boon.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Vote disableboon is disabled.\n\"");
			return;
		}

		if (!g_enableBoon.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Boon is already disabled.\n\"");
			return;
		}

		Com_sprintf(level.voteString, sizeof(level.voteString), "g_enableBoon %d;svsay "S_COLOR_CYAN"Boon will be disabled next map_restart.", 0);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Disable Boon", level.voteString);
	}
	else if (!Q_stricmp(arg1, "instagib"))
	{
		if (!g_allow_vote_instagib.integer) {
			trap_SendServerCommand(ent - g_entities, "print \"Vote instagib is disabled.\n\"");
			return;
		}

		if (g_instagib.integer) {
			Com_sprintf(level.voteString, sizeof(level.voteString), "g_instagib 0");
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Disable Instagib%s",
				level.instagibMap ? va("\n(Does not affect %s^7)", Cvar_VariableString("mapname")) : "");
		}
		else {
			Com_sprintf(level.voteString, sizeof(level.voteString), "g_instagib 1");
			Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Enable Instagib%s",
				level.instagibMap ? va("\n(Does not affect %s^7)", Cvar_VariableString("mapname")) : "");
		}
	}
	else if (!Q_stricmp(arg1, "g_doWarmup"))
	{
		int n = atoi(arg2);

        if (!g_allow_vote_warmup.integer){
            trap_SendServerCommand(ent - g_entities, "print \"Vote warmup is disabled.\n\"");
            return;
        }                

		Com_sprintf(level.voteString, sizeof(level.voteString), "%s %s", arg1, arg2);
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), n ? "Enable Warmup" : "Disable Warmup");
	}
	else
	{
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"", arg1, arg2 );
		Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	}

	trap_SendServerCommand( -1, va("print \"%s%s^7 %s\n\"", NM_SerializeUIntToColor(ent - g_entities), ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLCALLEDVOTE")) );

	// log the vote
	G_LogPrintf("Client %i (%s) called a vote: %s %s\n",
		ent-g_entities, ent->client->pers.netname, arg1, arg2);

	// start the voting, the caller autoamtically votes yes
	level.voteStartTime = level.time;
	level.voteTime = level.time;
	level.voteAutoPassOnExpiration = qfalse;
	level.voteYes = 1;
	level.voteNo = 0;
	level.lastVotingClient = ent-g_entities;
	level.multiVoting = qfalse;
	level.runoffSurvivors = level.runoffLosers = level.successfulRerollVoters = 0llu;
	level.inRunoff = qfalse;
	level.mapsThatCanBeVotedBits = 0;
	level.multiVoteChoices = 0;
	memset( &( level.multiVotes ), 0, sizeof( level.multiVotes ) );

	fixVoters( racersAllowVote, onlyThisTeamCanVote);

	ent->client->mGameFlags |= PSG_VOTED;
	ent->client->mGameFlags |= PSG_VOTEDYES;
	ent->client->mGameFlags |= PSG_CALLEDVOTE;

	trap_SetConfigstring( CS_VOTE_TIME, va("%i", level.voteTime ) );
	trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );	
	trap_SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
	trap_SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );	
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent, const char *forceVoteArg ) {
	char		msg[64];

	if ( !level.voteTime ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEINPROG")) );
		return;
	}

	if ( !(ent->client->mGameFlags & PSG_CANVOTE) ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", "You can't participate in this vote.") );
		return;
	}

	if (g_gametype.integer != GT_DUEL &&
		g_gametype.integer != GT_POWERDUEL)
	{
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
			return;
		}
	}

	if (VALIDSTRING(forceVoteArg))
		Q_strncpyz(msg, forceVoteArg, sizeof(msg));
	else
		trap_Argv( 1, msg, sizeof( msg ) );

	qboolean printVoteCast = qtrue;

	if ( !level.multiVoting ) {
		// not a special multi vote, use legacy behavior

		if (!(ent->client->mGameFlags & PSG_CALLEDVOTE) && (ent->client->mGameFlags & PSG_VOTED) && (ent->client->mGameFlags & PSG_VOTEDNO) && (msg[0] == 'y' || msg[1] == 'Y' || msg[1] == '1')) {
			G_LogPrintf("Client %i (%s) changed vote to YES\n", ent - g_entities, ent->client->pers.netname);
			trap_SendServerCommand(ent - g_entities, "print \"Vote changed to yes.\n\"");
			printVoteCast = qfalse;

			level.voteYes++;
			level.voteNo--;

			ent->client->mGameFlags |= PSG_VOTEDYES;
			ent->client->mGameFlags &= ~PSG_VOTEDNO;

			trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
			trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
		}
		else if (!(ent->client->mGameFlags & PSG_CALLEDVOTE) && (ent->client->mGameFlags & PSG_VOTED) && (ent->client->mGameFlags & PSG_VOTEDYES) && !(msg[0] == 'y' || msg[1] == 'Y' || msg[1] == '1')) {
			G_LogPrintf("Client %i (%s) changed vote to NO\n", ent - g_entities, ent->client->pers.netname);
			trap_SendServerCommand(ent - g_entities, "print \"Vote changed to no.\n\"");
			printVoteCast = qfalse;

			level.voteYes--;
			level.voteNo++;

			ent->client->mGameFlags &= ~PSG_VOTEDYES;
			ent->client->mGameFlags |= PSG_VOTEDNO;

			trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
			trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
		}
		else if (ent->client->mGameFlags & PSG_VOTED) {
			if (ent->client->mGameFlags & PSG_CALLEDVOTE)
				trap_SendServerCommand(ent - g_entities, "print \"You cannot change your vote because you called this vote.\n\"");
			else if (ent->client->mGameFlags & PSG_VOTEDNO)
				trap_SendServerCommand(ent - g_entities, "print \"You have already voted no.\n\"");
			else if (ent->client->mGameFlags & PSG_VOTEDYES)
				trap_SendServerCommand(ent - g_entities, "print \"You have already voted yes.\n\"");
			else
				trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEALREADY"))); // ???

			return;
		}
		else {
			if (msg[0] == 'y' || msg[1] == 'Y' || msg[1] == '1') {
				G_LogPrintf("Client %i (%s) voted YES\n", ent - g_entities, ent->client->pers.netname);
				level.voteYes++;
				ent->client->mGameFlags |= PSG_VOTEDYES;
				ent->client->mGameFlags &= ~PSG_VOTEDNO;
				trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
			}
			else {
				G_LogPrintf("Client %i (%s) voted NO\n", ent - g_entities, ent->client->pers.netname);
				level.voteNo++;
				ent->client->mGameFlags &= ~PSG_VOTEDYES;
				ent->client->mGameFlags |= PSG_VOTEDNO;
				trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
			}
		}
	} else {
		// multi map vote, only allow voting for valid choice ids
		int voteId = atoi( msg );

		if (g_vote_runoffRerollOption.integer && !level.runoffRoundsCompletedIncludingRerollRound && !Q_stricmp(msg, "r")) {
			level.multiVotes[ent - g_entities] = -1;

			if (!(ent->client->mGameFlags & PSG_VOTED)) { // first vote
				G_LogPrintf("Client %i (%s) voted to reroll\n", ent - g_entities, ent->client->pers.netname);
				level.voteYes++;
				trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
			}
			else { // changing vote
				G_LogPrintf("Client %i (%s) changed their vote to reroll\n", ent - g_entities, ent->client->pers.netname);
			}
		}
		else {
			int integerBits = 8 * sizeof(int);
			if (voteId <= 0 || /*voteId > level.multiVoteChoices || */voteId > integerBits || !(level.mapsThatCanBeVotedBits & (1 << (voteId - 1)))) {
				trap_SendServerCommand(ent - g_entities, "print \"Invalid choice, please use /vote (number) from console\n\"");
				return;
			}

			// we maintain an internal array of choice ids, and only use voteYes to show how many people voted

			if (!(ent->client->mGameFlags & PSG_VOTED)) { // first vote
				G_LogPrintf("Client %i (%s) voted for choice %d%s\n", ent - g_entities, ent->client->pers.netname, voteId,
					level.multiVoteMapShortNames[voteId][0] ? va(" (%s)", level.multiVoteMapShortNames[voteId - 1]) : "");
				level.voteYes++;
				trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
			}
			else { // changing vote
				G_LogPrintf("Client %i (%s) changed their vote to choice %d%s\n", ent - g_entities, ent->client->pers.netname, voteId,
					level.multiVoteMapShortNames[voteId][0] ? va(" (%s)", level.multiVoteMapShortNames[voteId - 1]) : "");
			}

			level.multiVotes[ent - g_entities] = voteId;
		}
	}

	if (printVoteCast)
		trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "PLVOTECAST" ) ) );
	ent->client->mGameFlags |= PSG_VOTED;

	// a majority will be determined in CheckVote, which will also account
	// for players entering or leaving
}

static void Cmd_Ready_f(gentity_t *ent) {
	/*
	if (!g_doWarmup.integer || level.restarted  )
		return;
	*/

	// g_allowReady 0 = no readying
	// g_allowReady 1 = only ingame dudes can ready
	// g_allowReady 2 = everyone (including specs/racers) can ready
	if (!g_allowReady.integer) {
		trap_SendServerCommand(ent - g_entities, "print \"Ready is disabled.\n\"");
		return;
	}

	if (g_allowReady.integer == 1 && (ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->sess.sessionTeam == TEAM_FREE)) {
		trap_SendServerCommand(ent - g_entities, "print \"You must be in-game to ready.\n\"");
		return;
	}

	if (ent->client->pers.readyTime > level.time - 500 /*2000*/)
		return;

	// if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
    //     return;

	ent->client->pers.ready = !ent->client->pers.ready;
	ent->client->pers.readyTime = level.time;

	if (ent->client->pers.ready) {
		trap_SendServerCommand(ent - g_entities, va("cp \"^2You are ready^7%s\"", ent - g_entities >= 15 ? "\n\nDue to a base JKA bug, you will NOT\nappear ready on the scoreboard.\nA fix is being worked on." : ""));
	}
	else
	{
		trap_SendServerCommand(ent - g_entities, va("cp \"^3You are NOT ready^7\""));
	}
}


/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f( gentity_t *ent ) {
	int		i, team, cs_offset;
	static char	arg1[MAX_STRING_TOKENS];
	static char	arg2[MAX_STRING_TOKENS];

	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !g_allowVote.integer ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")) );
		return;
	}

	if ( level.teamVoteTime[cs_offset] ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADY")) );
		return;
	}
	if ( ent->client->pers.teamVoteCount >= MAX_VOTE_COUNT ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MAXTEAMVOTES")) );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")) );
		return;
	}

	// make sure it is a valid command to vote on
	trap_Argv( 1, arg1, sizeof( arg1 ) );
	arg2[0] = '\0';
	for ( i = 2; i < trap_Argc(); i++ ) {
		if (i > 2)
			strcat(arg2, " ");
		trap_Argv( i, &arg2[strlen(arg2)], sizeof( arg2 ) - strlen(arg2) );
	}

	// *CHANGE 8b* anti callvote bug
	if ((g_protectCallvoteHack.integer && (strchr( arg1, '\n' ) || strchr( arg2, '\n' ) ||	strchr( arg1, '\r' ) || strchr( arg2, '\r' ))) ) {
		//lets replace line breaks with ; for better readability
		int len;
		for(len = 0; len < (int)strlen(arg1); ++len)
			if(arg1[len]=='\n' || arg1[len]=='\r') arg1[len] = ';';
		for(len = 0; len < (int)strlen(arg2); ++len)
			if(arg2[len]=='\n' || arg2[len]=='\r') arg2[len] = ';';

		G_HackLog("Callvote hack: Client num %d (%s) from %s tries to hack via callvote (callvote %s \"%s\").\n",
                ent->client->pers.clientNum, 
				ent->client->pers.netname,
				ent->client->sess.ipString,
                arg1, arg2);
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	if( strchr( arg1, ';'  ) || strchr( arg2, ';'  )) {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	if ( !Q_stricmp( arg1, "leader" ) ) {
		char netname[MAX_NETNAME], leader[MAX_NETNAME];

		if ( !arg2[0] ) {
			i = ent->client->ps.clientNum;
		}
		else {
			// numeric values are just slot numbers
			for (i = 0; i < 3; i++) {
				if ( !arg2[i] || arg2[i] < '0' || arg2[i] > '9' )
					break;
			}
			if ( i >= 3 || !arg2[i]) {
				i = atoi( arg2 );
				if ( i < 0 || i >= level.maxclients ) {
					trap_SendServerCommand( ent-g_entities, va("print \"Bad client slot: %i\n\"", i) );
					return;
				}

				if ( !g_entities[i].inuse ) {
					trap_SendServerCommand( ent-g_entities, va("print \"Client %i is not active\n\"", i) );
					return;
				}
			}
			else {
				Q_strncpyz(leader, arg2, sizeof(leader));
				Q_CleanStr(leader);
				for ( i = 0 ; i < level.maxclients ; i++ ) {
					if ( level.clients[i].pers.connected == CON_DISCONNECTED )
						continue;
					if (level.clients[i].sess.sessionTeam != team)
						continue;
					Q_strncpyz(netname, level.clients[i].pers.netname, sizeof(netname));
					Q_CleanStr(netname);
					if ( !Q_stricmp(netname, leader) ) {
						break;
					}
				}
				if ( i >= level.maxclients ) {
					trap_SendServerCommand( ent-g_entities, va("print \"%s is not a valid player on your team.\n\"", arg2) );
					return;
				}
			}
		}
		Com_sprintf(arg2, sizeof(arg2), "%d", i);
	} else {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap_SendServerCommand( ent-g_entities, "print \"Team vote commands are: leader <player>.\n\"" );
		return;
	}

	Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "%s %s", arg1, arg2 );

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if (level.clients[i].sess.sessionTeam == team)
			trap_SendServerCommand( i, va("print \"%s called a team vote.\n\"", ent->client->pers.netname ) );
	}

	// start the voting, the caller autoamtically votes yes
	level.teamVoteTime[cs_offset] = level.time;
	level.teamVoteYes[cs_offset] = 1;
	level.teamVoteNo[cs_offset] = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam == team)
			level.clients[i].mGameFlags &= ~PSG_TEAMVOTED;
	}
	ent->client->mGameFlags |= PSG_TEAMVOTED;

	trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va("%i", level.teamVoteTime[cs_offset] ) );
	trap_SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteString[cs_offset] );
	trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
}

/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f( gentity_t *ent ) {
	int			team, cs_offset;
	char		msg[64];

	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOTEAMVOTEINPROG")) );
		return;
	}
	if ( ent->client->mGameFlags & PSG_TEAMVOTED ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADYCAST")) );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
		return;
	}

	trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLTEAMVOTECAST")) );

	ent->client->mGameFlags |= PSG_TEAMVOTED;

	trap_Argv( 1, msg, sizeof( msg ) );

	if ( msg[0] == 'y' || msg[1] == 'Y' || msg[1] == '1' ) {
		G_LogPrintf("Client %i (%s) teamvoted YES\n", ent - g_entities, ent->client->pers.netname);
		level.teamVoteYes[cs_offset]++;
		trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	} else {
		G_LogPrintf("Client %i (%s) teamvoted NO\n", ent - g_entities, ent->client->pers.netname);
		level.teamVoteNo[cs_offset]++;
		trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );	
	}

	// a majority will be determined in TeamCheckVote, which will also account
	// for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent ) {
	vec3_t		origin, angles;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;

	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")));
		return;
	}
	if ( trap_Argc() != 5 ) {
		trap_SendServerCommand( ent-g_entities, va("print \"usage: setviewpos x y z yaw\n\""));
		return;
	}

	VectorClear( angles );
	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

	trap_Argv( 4, buffer, sizeof( buffer ) );
	angles[YAW] = atof( buffer );

	TeleportPlayer( ent, origin, angles );
}

void Cmd_Ignore_f( gentity_t *ent )
{			   
	char buffer[32];
	gentity_t* found = NULL;
	int id = 0;

	if ( !ent->client )
	{
		return;
	}

	if (!g_allowIgnore.integer)
		return;

	if (trap_Argc() < 2)
	{
		trap_SendServerCommand( ent-g_entities, 
			"print \"usage: ignore [id/name]  (name can be just part of name, colors dont count))  \n\"");
		return;
	}

	trap_Argv(1,buffer,sizeof(buffer));		

	if ( atoi(buffer) == -1)
	{
		if (ent->client->sess.ignoreFlags == 0xFFFFFFFF)
		{
			trap_SendServerCommand( ent-g_entities, 
				va("print \"All Clients messages are now "S_COLOR_GREEN"allowed"S_COLOR_WHITE".\n\""));
			ent->client->sess.ignoreFlags = 0;
		} 
		else 
		{
			trap_SendServerCommand( ent - g_entities, 
				va( "print \"All Clients messages are now "S_COLOR_RED"ignored"S_COLOR_WHITE".\n\"" ) );
			ent->client->sess.ignoreFlags = 0xFFFFFFFF;
		}

		return;
	} 

	found = G_FindClient( buffer );	  
	if ( !found || !found->client )
	{
		trap_SendServerCommand(
			ent - g_entities,
			va( "print \"Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n\"",
			buffer ) );
		return;
	}

	id = found - g_entities;  
	if ( ent->client->sess.ignoreFlags & (1 << id) )
	{
		trap_SendServerCommand( ent - g_entities,
			va( "print \"Client '%s"S_COLOR_WHITE"' messages are now "S_COLOR_GREEN"allowed"S_COLOR_WHITE".\n\"",
			found->client->pers.netname ) );
	}
	else
	{
		trap_SendServerCommand( ent - g_entities,
			va( "print \"Client '%s"S_COLOR_WHITE"' messages are now "S_COLOR_RED"ignored"S_COLOR_WHITE".\n\"",
			found->client->pers.netname ) );
	}
	ent->client->sess.ignoreFlags ^= (1 << id);

}

#define MST (-7)
#define UTC (0)
#define CCT (+8)

void Cmd_WhoIs_f( gentity_t* ent );

void Cmd_TestCmd_f( gentity_t *ent ) {
}

/*
=================
Cmd_TopTimes_f
=================
*/

static char* GetItemName(gentity_t* ent) {
	if (!ent || !ent->item) {
		assert(qfalse);
		return NULL;
	}

	if (ent->item->giType == IT_WEAPON) {
		switch (ent->item->giTag) {
		case WP_STUN_BATON: return "stun baton";
		case WP_MELEE: return "melee";
		case WP_SABER: return "saber";
		case WP_BRYAR_PISTOL: case WP_BRYAR_OLD: return "pistol";
		case WP_BLASTER: return "blaster";
		case WP_DISRUPTOR: return "disruptor";
		case WP_BOWCASTER: return "bowcaster";
		case WP_REPEATER: return "repeater";
		case WP_DEMP2: return "demp";
		case WP_FLECHETTE: return "golan";
		case WP_ROCKET_LAUNCHER: return "rocket launcher";
		case WP_THERMAL: return "thermals";
		case WP_TRIP_MINE: return "mines";
		case WP_DET_PACK: return "detpacks";
		case WP_CONCUSSION: return "concussion";
		}
	}
	else if (ent->item->giType == IT_AMMO) {
		switch (ent->item->giTag) {
		case AMMO_BLASTER: return "blaster ammo";
		case AMMO_POWERCELL: return "power cell";
		case AMMO_METAL_BOLTS: return "metallic bolts";
		case AMMO_ROCKETS: return "rocket ammo";
		case AMMO_THERMAL: return "thermal ammo";
		case AMMO_TRIPMINE: return "mine ammo";
		case AMMO_DETPACK: return "detpack ammo";
		}
	}
	else if (ent->item->giType == IT_ARMOR) {
		switch (ent->item->giTag) {
		case 1: return "small armor";
		case 2: return "large armor";
		}
	}
	else if (ent->item->giType == IT_HEALTH) {
		return "health";
	}
	else if (ent->item->giType == IT_POWERUP) {
		switch (ent->item->giTag) {
		case PW_FORCE_ENLIGHTENED_LIGHT: return "light enlightenment";
		case PW_FORCE_ENLIGHTENED_DARK: return "dark enlightenment";
		case PW_FORCE_BOON: return "boon";
		case PW_YSALAMIRI: return "ysalamiri";
		}
	}
	else if (ent->item->giType == IT_HOLDABLE) {
		switch (ent->item->giTag) {
		case HI_SEEKER: return "seeker";
		case HI_SHIELD: return "shield";
		case HI_MEDPAC: return "bacta";
		case HI_MEDPAC_BIG: return "big bacta";
		case HI_BINOCULARS: return "binoculars";
		case HI_SENTRY_GUN: return "sentry";
		case HI_JETPACK: return "jetpack"; // am i really doing this
		case HI_EWEB: return "eweb";
		case HI_CLOAK: return "cloak";
		}
	}

	return NULL;
}

// if one parameter is NULL, its value is added to the next non NULL parameter
void PartitionedTimer( const int time, int *mins, int *secs, int *millis ) {
	div_t qr;
	int pMins, pSecs, pMillis;

	qr = div( time, 1000 );
	pMillis = qr.rem;
	qr = div( qr.quot, 60 );
	pSecs = qr.rem;
	pMins = qr.quot;

	if ( mins ) {
		*mins = pMins;
	} else {
		pSecs += pMins * 60;
	}

	if ( secs ) {
		*secs = pSecs;
	} else {
		pMillis += pSecs * 1000;
	}

	if ( millis ) {
		*millis = pMillis;
	}
}

const char* GetLongNameForRecordType( raceType_t type ) {
	switch ( type ) {
		case RACE_TYPE_STANDARD: return "Standard";
		case RACE_TYPE_WEAPONS: return "Weapons";
		default: return "Unknown";
	}
}

const char* GetShortNameForRecordType( raceType_t type ) {
	switch ( type ) {
		case RACE_TYPE_STANDARD: return "std";
		case RACE_TYPE_WEAPONS: return "wpn";
		default: return "unknown";
	}
}

raceType_t GetRecordTypeForShortName( const char *name ) {
	if ( !Q_stricmpn( name, "s", 1 ) ) {
		return RACE_TYPE_STANDARD;
	}

	if ( !Q_stricmpn( name, "wea", 3 ) || !Q_stricmpn( name, "wp", 2 ) ) {
		return RACE_TYPE_WEAPONS;
	}

	return RACE_TYPE_INVALID;
}

static void PrintRaceRecordsCallback(void* context, const char* mapname, const raceType_t type, const int rank, const char* name, const raceRecord_t* record) {
	gentity_t* ent = (gentity_t*)context;
	
	char nameString[64] = { 0 };
	Q_strncpyz(nameString, name, sizeof(nameString));
	int j;
	for (j = Q_PrintStrlen(nameString); j < MAX_NAME_DISPLAYLENGTH; ++j)
		Q_strcat(nameString, sizeof(nameString), " ");

	int secs, millis;
	PartitionedTimer(record->time, NULL, &secs, &millis);

	char timeString[8];
	if (secs > 999) Com_sprintf(timeString, sizeof(timeString), "  >999 ");
	else Com_sprintf(timeString, sizeof(timeString), "%3d.%03d", secs, millis);

	char flagString[7];
	Com_sprintf(flagString, sizeof(flagString), "%s%s", TeamColorString(record->extra.whoseFlag), TeamName(record->extra.whoseFlag));

	char dateString[22];
	time_t now = time(NULL);
	if (now - record->date < 60 * 60 * 24) Q_strncpyz(dateString, S_COLOR_GREEN, sizeof(dateString));
	else Q_strncpyz(dateString, S_COLOR_WHITE, sizeof(dateString));
	G_FormatLocalDateFromEpoch(dateString + 2, sizeof(dateString) - 2, record->date);

	trap_SendServerCommand(ent - g_entities, va(
		"print \""S_COLOR_CYAN"%3d"S_COLOR_WHITE": "S_COLOR_WHITE"%s  "S_COLOR_YELLOW"%s   %-6s      "S_COLOR_YELLOW"%-6d      %-6d     %s\n\"",
		rank, nameString, timeString, flagString, Com_Clampi(1, 99999999, record->extra.maxSpeed), Com_Clampi(1, 9999999, record->extra.avgSpeed), dateString
	));
}

#define NUM_RECORDS_PER_PAGE 10

static void SubCmd_TopTimes_Records(gentity_t* ent, const char* mapname, const raceType_t type, const int page) {
	trap_SendServerCommand(ent - g_entities, va(
		"print \""S_COLOR_WHITE"Records for the "S_COLOR_YELLOW"%s "S_COLOR_WHITE"category on "S_COLOR_YELLOW"%s"S_COLOR_WHITE":\n"
		S_COLOR_CYAN"               Name              Time    Flag    Topspeed     Average           Date\n\"",
		GetLongNameForRecordType(type), mapname)
	);

	pagination_t pagination;
	pagination.numPerPage = NUM_RECORDS_PER_PAGE;
	pagination.numPage = page;

	G_DBListRaceRecords(mapname, type, pagination, PrintRaceRecordsCallback, ent);

	trap_SendServerCommand(ent - g_entities, va("print \""S_COLOR_WHITE"Viewing page: %d\n\"", page));
}

static void PrintRaceTopCallback(void* context, const char* mapname, const raceType_t type, const char* name, const int captureTime, const time_t date) {
	gentity_t* ent = (gentity_t*)context;

	int secs, millis;
	PartitionedTimer( captureTime, NULL, &secs, &millis );

	char timeString[8];
	if ( secs > 999 ) Com_sprintf( timeString, sizeof( timeString ), "  >999 " );
	else Com_sprintf( timeString, sizeof( timeString ), "%3d.%03d", secs, millis );

	char dateString[22];
	time_t now = time( NULL );
	if ( now - date < 60 * 60 * 24 ) Q_strncpyz(dateString, S_COLOR_GREEN, sizeof(dateString) );
	else Q_strncpyz(dateString, S_COLOR_WHITE, sizeof(dateString) );
	G_FormatLocalDateFromEpoch(dateString + 2, sizeof(dateString) - 2, date );

	trap_SendServerCommand( ent - g_entities, va(
		"print \""S_COLOR_WHITE"%-25s  "S_COLOR_YELLOW"%s   %s     "S_COLOR_WHITE"%s\n\"", mapname, timeString, dateString, name
	) );
}

#define NUM_TOP_PER_PAGE 15

static void SubCmd_TopTimes_Maplist(gentity_t* ent, const raceType_t type, const int page) {
	trap_SendServerCommand(ent - g_entities, va(
		"print \""S_COLOR_WHITE"Records for the "S_COLOR_YELLOW"%s "S_COLOR_WHITE"category:\n"
		S_COLOR_CYAN"           Map               Time           Date                 Name\n\"",
		GetLongNameForRecordType(type))
	);

	pagination_t pagination;
	pagination.numPerPage = NUM_TOP_PER_PAGE;
	pagination.numPage = page;

	G_DBListRaceTop(type, pagination, PrintRaceTopCallback, ent);

	trap_SendServerCommand(ent - g_entities, va("print \""S_COLOR_WHITE"Viewing page: %d\n\"", page));
}

static void PrintRaceLeaderboardCallback(void* context, const raceType_t type, const int rank, const char* name, const int golds, const int silvers, const int bronzes) {
	gentity_t* ent = (gentity_t*)context;

	char nameString[64] = { 0 };
	Q_strncpyz(nameString, name, sizeof(nameString));
	int j;
	for (j = Q_PrintStrlen(nameString); j < MAX_NAME_DISPLAYLENGTH; ++j)
		Q_strcat(nameString, sizeof(nameString), " ");

	trap_SendServerCommand( ent - g_entities, va(
		"print \""S_COLOR_CYAN"%3d"S_COLOR_WHITE": "S_COLOR_WHITE"%s    "S_COLOR_YELLOW"%2d        "S_COLOR_GREY"%2d        "S_COLOR_ORANGE"%2d\n\"",
		rank, nameString, golds, silvers, bronzes
	) );
}

#define NUM_LEADERBOARD_PER_PAGE 10

static void SubCmd_TopTimes_Ranks(gentity_t* ent, const raceType_t type, const int page) {
	trap_SendServerCommand(ent - g_entities, va(
		"print \""S_COLOR_WHITE"Leaderboard for the "S_COLOR_YELLOW"%s "S_COLOR_WHITE"category:\n"
		S_COLOR_CYAN"               Name             Golds    Silvers   Bronzes\n\"",
		GetLongNameForRecordType(type))
	);

	pagination_t pagination;
	pagination.numPerPage = NUM_LEADERBOARD_PER_PAGE;
	pagination.numPage = page;

	G_DBListRaceLeaderboard(type, pagination, PrintRaceLeaderboardCallback, ent);

	trap_SendServerCommand(ent - g_entities, va("print \""S_COLOR_WHITE"Viewing page: %d\n\"", page));
}

static void PrintRaceLatestCallback(void* context, const char* mapname, const raceType_t type, const int rank, const char* name, const int captureTime, const time_t date) {
	gentity_t* ent = (gentity_t*)context;

	char nameString[64] = { 0 };
	Q_strncpyz(nameString, name, sizeof(nameString));
	int j;
	for (j = Q_PrintStrlen(nameString); j < MAX_NAME_DISPLAYLENGTH; ++j)
		Q_strcat(nameString, sizeof(nameString), " ");

	// two chars for colors, max 2 for rank, 1 null terminator
	char rankString[5];
	char* color = rank == 1 ? S_COLOR_RED : S_COLOR_WHITE;
	Com_sprintf( rankString, sizeof( rankString ), "%s%d", color, rank );

	int secs, millis;
	PartitionedTimer( captureTime, NULL, &secs, &millis );

	char timeString[8];
	if ( secs > 999 ) Com_sprintf( timeString, sizeof( timeString ), "  >999 " );
	else Com_sprintf( timeString, sizeof( timeString ), "%3d.%03d", secs, millis );

	char dateString[22];
	time_t now = time( NULL );
	if ( now - date < 60 * 60 * 24 ) Q_strncpyz(dateString, S_COLOR_GREEN, sizeof(dateString) );
	else Q_strncpyz(dateString, S_COLOR_WHITE, sizeof(dateString) );
	G_FormatLocalDateFromEpoch(dateString + 2, sizeof(dateString) - 2, date );

	trap_SendServerCommand( ent - g_entities, va(
		"print \""S_COLOR_WHITE"%s     "S_COLOR_YELLOW"%-4s   %-4s    "S_COLOR_YELLOW"%s   %s     "S_COLOR_WHITE"%s\n\"",
		nameString, GetShortNameForRecordType(type), rankString, timeString, dateString, mapname
	) );
}

#define NUM_LATEST_PER_PAGE 15

static void SubCmd_TopTimes_Latest(gentity_t* ent, const int page) {
	trap_SendServerCommand(ent - g_entities, va(
		"print \""S_COLOR_WHITE"Latest records:\n"
		S_COLOR_CYAN"           Name             Type   Rank     Time           Date                 Map\n\"")
	);

	pagination_t pagination;
	pagination.numPerPage = NUM_LEADERBOARD_PER_PAGE;
	pagination.numPage = page;

	G_DBListRaceLatest(pagination, PrintRaceLatestCallback, ent);

	trap_SendServerCommand(ent - g_entities, va("print \""S_COLOR_WHITE"Viewing page: %d\n\"", page));
}

void Cmd_TopTimes_f( gentity_t *ent ) {
	if ( !ent || !ent->client ) {
		return;
	}

	if ( !level.racemodeRecordsEnabled ) {
		trap_SendServerCommand( ent - g_entities, "print \"Capture records are disabled.\n\"" );
		return;
	}

	if ( level.racemodeRecordsReadonly ) {
		trap_SendServerCommand( ent - g_entities, "print \""S_COLOR_YELLOW"WARNING: Server settings were modified, capture records are read-only!\n\"" );
	}

	// figure out the arguments to redirect the command:
	// subcommand - [mapname - type - page]
	// where the ones in [] can be in any order
	
	// default arguments
	char subcommand[32] = { 0 };
	qboolean gotMapname = qfalse;
	char mapname[MAX_QPATH];
	Q_strncpyz(mapname, level.mapname, sizeof(mapname));
	qboolean gotType = qfalse;
	raceType_t type = RACE_TYPE_STANDARD;
	qboolean gotPage = qfalse;
	int page = 1;

	if (trap_Argc() > 1) {
		char buf[64];
		trap_Argv(1, buf, sizeof(buf));

		int startOptionalsAt = 1;

		if (!Q_stricmp(buf, "help") ||
			!Q_stricmp(buf, "maplist") ||
			!Q_stricmp(buf, "ranks") ||
			!Q_stricmp(buf, "latest"))
		{
			// first arg is a subcommand
			Q_strncpyz(subcommand, buf, sizeof(subcommand));
			startOptionalsAt = 2;
		}

		int i;
		for (i = startOptionalsAt; i < trap_Argc() && i < 3 + startOptionalsAt; ++i) {
			trap_Argv(i, buf, sizeof(buf));

			if (!gotPage) {
				int testPage = atoi(buf);
				if (testPage > 0 && testPage < 999) {
					page = testPage;
					gotPage = qtrue;
					continue;
				}
			}
			if (!gotType) {
				raceType_t testType = GetRecordTypeForShortName(buf);
				if (testType != RACE_TYPE_INVALID) {
					type = testType;
					continue;
				}
			}
			if (!gotMapname) {
				// assume it's a map name otherwise
				Q_strncpyz(mapname, buf, sizeof(mapname));
				gotMapname = qtrue;
				continue;
			}
		}
	}
	
	if (!Q_stricmp(subcommand, "help")) {
		char* text =
			S_COLOR_WHITE"Show records for the current map: "S_COLOR_CYAN"/toptimes [mapname] [std | wpn] [page]\n"
			S_COLOR_WHITE"List all map records: "S_COLOR_CYAN"/toptimes maplist [std | wpn] [page]\n"
			S_COLOR_WHITE"Show player leaderboard: "S_COLOR_CYAN"/toptimes ranks [std | wpn] [page]\n"
			S_COLOR_WHITE"Show latest records:" S_COLOR_CYAN"/toptimes latest [page]";

		trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", text));

		return;
	} else if (!Q_stricmp(subcommand, "maplist")) {
		SubCmd_TopTimes_Maplist(ent, type, page);
	} else if (!Q_stricmp(subcommand, "ranks")) {
		SubCmd_TopTimes_Ranks(ent, type, page);
	} else if (!Q_stricmp(subcommand, "latest")) {
		SubCmd_TopTimes_Latest(ent, page);
	} else {
		SubCmd_TopTimes_Records(ent, mapname, type, page);
		trap_SendServerCommand(ent - g_entities, "print \"For a list of all subcommands: /toptimes help\n\"");
	}
}

#ifdef NEWMOD_SUPPORT
static qboolean StringIsOnlyNumbers(const char *s) {
	if (!VALIDSTRING(s))
		return qfalse;
	for (const char *p = s; *p; p++) {
		if (*p < '0' || *p > '9')
			return qfalse;
	}
	return qtrue;
}

static XXH32_hash_t GetVchatHash(const char *modName, const char *msg, const char *fileName) {
	char buf[MAX_TOKEN_CHARS] = { 0 };
	Com_sprintf(buf, sizeof(buf), "%s%s%s%s", level.mapname, modName, msg, fileName);
	return XXH32(buf, strlen(buf), 0);
}

typedef enum vchatType_s {
	VCHATTYPE_TEAMWORK = 0,
	VCHATTYPE_GENERAL,
	VCHATTYPE_MEME
} vchatType_t;

#define VCHAT_ESCAPE_CHAR '$'
void Cmd_Vchat_f(gentity_t *sender) {
	char hashBuf[MAX_TOKEN_CHARS] = { 0 };
	trap_Argv(1, hashBuf, sizeof(hashBuf));
	if (!hashBuf[0] || !StringIsOnlyNumbers(hashBuf))
		return;
	XXH32_hash_t hash = (XXH32_hash_t)strtoul(hashBuf, NULL, 10);

	char *s = ConcatArgs(2);
	if (!VALIDSTRING(s) || !strchr(s, VCHAT_ESCAPE_CHAR))
		return;

	qboolean teamOnly = qfalse;
	vchatType_t type = VCHATTYPE_MEME;
	char modName[MAX_QPATH] = { 0 }, fileName[MAX_QPATH] = { 0 }, msg[200] = { 0 };
	// parse each argument
	char *r;
	for ( r = s; *r; r++ ) {
		// skip to the next argument
		while ( *r && *r != VCHAT_ESCAPE_CHAR )
			r++;
		if ( !*r++ )
			break;

		// copy this argument into a buffer
		char buf[MAX_STRING_CHARS] = { 0 }, *w = buf;
		while ( *r && *r != VCHAT_ESCAPE_CHAR && w - buf < sizeof( buf ) - 1 )
			*w++ = *r++;
		if ( !buf[0] )
			break;

		if (!Q_stricmpn(buf, "mod=", 4) && buf[4])
			Q_strncpyz(modName, buf + 4, sizeof(modName));
		else if (!Q_stricmpn(buf, "file=", 5) && buf[5])
			Q_strncpyz(fileName, buf + 5, sizeof(fileName));
		else if (!Q_stricmpn(buf, "msg=", 4) && buf[4])
			Q_strncpyz(msg, buf + 4, sizeof(msg));
		else if (!Q_stricmpn(buf, "team=", 5) && buf[5])
			teamOnly = !!atoi(buf + 5);
		else if (!Q_stricmpn(buf, "t=", 2) && buf[2])
			type = atoi(buf + 2);

		if (!*r)
			break; // we reached the end of the line
				   // the for loop will increment r here, taking us past the current escape char
	}

	if (!modName[0] || !fileName[0] || !msg[0])
		return;

	if (ChatLimitExceeded(sender, teamOnly ? SAY_TEAM : -1))
		return;

	Q_CleanStr( modName );
	Q_CleanStr( fileName );
#if 1
	Q_CleanStr( msg );
#endif
	
	XXH32_hash_t expectedHash = GetVchatHash(modName, msg, fileName);
	if (hash != expectedHash)
		return;

	if (IsInstapauser(sender)) {
		SV_Tell(sender - g_entities, "You cannot chat during your own pause.");
		return;
	}

	// convert global teamwork vchats and teamwork vchats among specs to memes
	if (type == VCHATTYPE_TEAMWORK && (!teamOnly || sender->client->sess.sessionTeam == TEAM_SPECTATOR || sender->client->sess.sessionTeam == TEAM_FREE))
		type = VCHATTYPE_MEME;

	G_LogPrintf("vchat: %d %s (%s/%s): %s/%s: %s\n",
		sender - g_entities,
		sender->client->pers.netname,
		teamOnly ? "team" : "global",
		type == VCHATTYPE_TEAMWORK ? "teamwork" : (type == VCHATTYPE_GENERAL ? "general" : "meme"),
		modName,
		fileName,
		msg);

	int senderLocation = 0;
	char chatLocation[64] = { 0 };
	char chatMessage[200] = { 0 };
	Q_strncpyz( chatMessage, msg, sizeof( chatMessage ) );
#if 0 // disable locations in ctf
	// get his location (disable this section to never send locations)
	if ( g_gametype.integer >= GT_TEAM )
		senderLocation = Team_GetLocation( sender, chatLocation, sizeof( chatLocation ), qtrue );
#endif

	char chatSenderName[64] = { 0 };
	if ( teamOnly ) {
		Com_sprintf( chatSenderName, sizeof( chatSenderName ), EC"(%s%c%c"EC")"EC": ", sender->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
#if 0 // disable this to disable all the extra siege stuff
		if ( g_gametype.integer == GT_SIEGE ) {
			if ( level.inSiegeCountdown ) {
				if ( sender->client->siegeClass != -1 && sender->client->sess.sessionTeam == TEAM_SPECTATOR && sender->client->sess.siegeDesiredTeam &&
					( sender->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM1 || sender->client->sess.siegeDesiredTeam == SIEGETEAM_TEAM2 ) ) {
					// we are in spec (in countdown) with a desired team/class...put our class type as our "location"
					switch ( bgSiegeClasses[sender->client->siegeClass].playerClass ) {
					case 0:	Q_strncpyz( chatLocation, "Assault", sizeof( chatLocation ) );			break;
					case 1:	Q_strncpyz( chatLocation, "Scout", sizeof( chatLocation ) );			break;
					case 2:	Q_strncpyz( chatLocation, "Tech", sizeof( chatLocation ) );				break;
					case 3:	Q_strncpyz( chatLocation, "Jedi", sizeof( chatLocation ) );				break;
					case 4:	Q_strncpyz( chatLocation, "Demolitions", sizeof( chatLocation ) );		break;
					case 5:	Q_strncpyz( chatLocation, "Heavy Weapons", sizeof( chatLocation ) );	break;
					default:	senderLocation = 0;	chatLocation[0] = '\0';						break;
					}
				}
			}
			else { // not in siege countdown
				if ( sender->client->sess.sessionTeam == SIEGETEAM_TEAM1 || sender->client->sess.sessionTeam == SIEGETEAM_TEAM2 ) {
					if ( sender->client->tempSpectate > level.time || sender->health <= 0 ) { // ingame and dead
						Com_sprintf( chatMessage, sizeof( chatMessage ), "^0(DEAD) ^5%s", chatMessage );
					}
					else if ( g_improvedTeamchat.integer >= 2 && !level.intermissiontime ) {
						if ( sender->client->ps.stats[STAT_ARMOR] )
							Com_sprintf( chatMessage, sizeof( chatMessage ), "^7(%i/%i) ^5%s", sender->health, sender->client->ps.stats[STAT_ARMOR], chatMessage );
						else
							Com_sprintf( chatMessage, sizeof( chatMessage ), "^7(%i) ^5%s", sender->health, chatMessage );
					}
				}
			}

			if ( ( ( sender->client->sess.sessionTeam == TEAM_SPECTATOR &&
				( !level.inSiegeCountdown ||
				( sender->client->sess.siegeDesiredTeam != SIEGETEAM_TEAM1 && sender->client->sess.siegeDesiredTeam != SIEGETEAM_TEAM2 ) ) ||
				level.intermissiontime ) && g_improvedTeamchat.integer ) ||
				( level.zombies && sender->client->sess.sessionTeam == TEAM_BLUE ) ) {
				senderLocation = 0;
				chatLocation[0] = '\0';
			}
		}
#endif
	}
	else {
		Com_sprintf( chatSenderName, sizeof( chatSenderName ), "%s%c%c"EC": ", sender->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
	}

	int i;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		gclient_t *cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED )
			continue;

		if (sender->client != cl && cl->sess.ignoreFlags & (1 << (sender - g_entities))) {
			// this recipient has ignored the sender
			continue;
		}

		if (sender->client != cl && sender->client->sess.shadowMuted && !cl->sess.shadowMuted) {
			// the sender is shadowmuted and this recipient is not shadowmuted
			continue;
		}

		int locationToSend = 0;
		if ( teamOnly ) {
			team_t senderTeam = sender->client->sess.sessionTeam;
			team_t recipientTeam = cl->sess.sessionTeam;
			if ( senderTeam != recipientTeam ) { // it's a teamchat and this guy isn't on our team...
				if ( recipientTeam == TEAM_SPECTATOR && cl->ps.persistant[PERS_TEAM] == senderTeam ) {
					// but he is speccing our team, so it's okay
				} else if (IsRacerOrSpectator(sender) && IsRacerOrSpectator(&g_entities[i])) {
					// but they are both spec/racer, so it's okay
				} else {
					continue; // he is truly on a different team; don't send it to him
				}
			}
			locationToSend = senderLocation;
		}

		char *command;
		if (type == VCHATTYPE_MEME) {
			command = va("kls -1 -1 vcht \"cl=%d\" \"mod=%s\" \"file=%s\" \"msg=%s\" \"t=%d\" \"team=%d\"%s",
				sender - g_entities,
				modName,
				fileName,
				msg,
				type,
				teamOnly,
				teamOnly && locationToSend ? va(" \"loc=%d\"", locationToSend) : "");
		}
		else {
			if (teamOnly && locationToSend) {
				command = va("ltchat \"%s\" \"%s\" \"5\" \"%s\" \"%d\" \"vchat\" \"mod=%s\" \"file=%s\" \"msg=%s\" \"t=%d\" \"team=%d\" \"loc=%d\"",
					chatSenderName,
					chatLocation,
					chatMessage,
					sender - g_entities,
					modName,
					fileName,
					msg,
					type,
					teamOnly, // team only parameter is sent anyway so clients can display with team styling
					locationToSend);
			}
			else {
				if (teamOnly) {
					command = va("tchat \"%s^5%s\" \"%d\" \"vchat\" \"mod=%s\" \"file=%s\" \"msg=%s\" \"t=%d\" \"team=%d\"",
						chatSenderName,
						chatMessage,
						sender - g_entities,
						modName,
						fileName,
						msg,
						type,
						teamOnly); // team only parameter is sent anyway so clients can display with team styling);
				}
				else {
					command = va("chat \"%s^2%s\" \"%d\" \"vchat\" \"mod=%s\" \"file=%s\" \"msg=%s\" \"t=%d\" \"team=%d\"",
						chatSenderName,
						chatMessage,
						sender - g_entities,
						modName,
						fileName,
						msg,
						type,
						teamOnly); // team only parameter is sent anyway so clients can display with team styling);
				}
			}
		}
		trap_SendServerCommand( i, command );
	}
}

#endif

/*
=================
Cmd_Race_f
=================
*/
void Cmd_Race_f( gentity_t *ent ) {
	if ( !ent || !ent->client ) {
		return;
	}

	if ( g_gametype.integer != GT_CTF ) {
		return;
	}

	if ( !g_enableRacemode.integer ) {
		trap_SendServerCommand( ent - g_entities, "print \"Racemode is disabled.\n\"" );
		return;
	}

	if ( trap_Argc() > 1 ) {
		char s[MAX_TOKEN_CHARS];
		trap_Argv( 1, s, sizeof( s ) );

		if ( !Q_stricmp( "help", s ) ) {
			trap_SendServerCommand( ent - g_entities, "print \""
				S_COLOR_WHITE"Racemode lets you travel the map in a different dimension. You are invisible to ingame players.\n"
				S_COLOR_WHITE"You can submit fastcaps, practice your aim, and spectate an ongoing pug while in racemode.\n"
				S_COLOR_RED"=> "S_COLOR_WHITE"Type "S_COLOR_CYAN"/toptimes help "S_COLOR_WHITE"for more information about fastcaps\n"
				S_COLOR_RED"=> "S_COLOR_WHITE"Type "S_COLOR_CYAN"/topaim help "S_COLOR_WHITE"for more information about aim practice\n"
				S_COLOR_RED"=> "S_COLOR_CYAN"Pull "S_COLOR_WHITE"a bot to begin free aim training\n"
				S_COLOR_RED"=> "S_COLOR_CYAN"Push "S_COLOR_WHITE"a bot to begin timed aim training\n"
				S_COLOR_RED"=> "S_COLOR_WHITE"Press your "S_COLOR_YELLOW"Force Seeing "S_COLOR_WHITE"bind to toggle seeing in game players\n"
				S_COLOR_RED"=> "S_COLOR_WHITE"Change the visibility of other racers with "S_COLOR_CYAN"/hideRacers"S_COLOR_WHITE", "S_COLOR_CYAN"/showRacers "S_COLOR_WHITE"or just "S_COLOR_CYAN"/toggleRacers\n"
				S_COLOR_RED"=> "S_COLOR_WHITE"Change the visibility of aim practice bots with "S_COLOR_CYAN"/hideBots"S_COLOR_WHITE", "S_COLOR_CYAN"/showBots "S_COLOR_WHITE"or just "S_COLOR_CYAN"/toggleBots\n"
				S_COLOR_YELLOW"NB: These previous three commands also work while in game and lets you see racers if you want to\n"
				S_COLOR_RED"=> "S_COLOR_WHITE"Set your teleport point with "S_COLOR_CYAN"/amtelemark"S_COLOR_WHITE", and teleport to it with "S_COLOR_CYAN"/amtele\n"
				S_COLOR_RED"=> "S_COLOR_WHITE"Automatically use speed when teleporting or restarting aim practice with "S_COLOR_CYAN"/amautospeed\n\"" );

			return;
		}
	}

	team_t oldTeam = ent->client->sess.sessionTeam;

	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		// don't let them go in racemode from red/blue
		trap_SendServerCommand( ent - g_entities, "print \"You must be spectating to use this command.\n\"" );
		return;
	}

	if ( ent->client->switchTeamTime > level.time ) {
		trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOSWITCH" ) ) );
		return;
	}

	// FIXME: when we are sure safety logs can't be triggered, remove this
	// set this to false so safety logs aren't triggered in SetTeamQuick
	qboolean oldInRacemode = ent->client->sess.inRacemode;
	ent->client->sess.inRacemode = qfalse;

	if ( oldTeam != TEAM_FREE ) {
		// put them in race mode from spec
		SetTeamQuick( ent, TEAM_FREE, qfalse );
	} else {
		// put them in spec from race mode
		SetTeamQuick( ent, TEAM_SPECTATOR, qfalse );
	}

	ent->client->sess.inRacemode = oldInRacemode;

	team_t newTeam = ent->client->sess.sessionTeam;

	if ( oldTeam != newTeam ) {
		if ( newTeam == TEAM_FREE ) {
			trap_SendServerCommand( -1, va("print \"%s%s " S_COLOR_WHITE S_COLOR_WHITE S_COLOR_WHITE "entered racemode.\n\"", NM_SerializeUIntToColor( ent - g_entities ), ent->client->pers.netname ) );
			
			if ( !( ent->client->sess.racemodeFlags & RMF_ALREADYJOINED ) ) {
				trap_SendServerCommand( ent - g_entities, "print \""S_COLOR_WHITE"Type "S_COLOR_CYAN"/race help "S_COLOR_WHITE"for more information about racemode\n\"" );
			}
			
			G_SetRaceMode( ent, qtrue );
		} else {
			trap_SendServerCommand( -1, va( "print \"%s%s " S_COLOR_WHITE S_COLOR_WHITE S_COLOR_WHITE "left racemode.\n\"", NM_SerializeUIntToColor( ent - g_entities ), ent->client->pers.netname ) );
			G_SetRaceMode( ent, qfalse );
		}

		// do ClientBegin here and not in SetTeamQuick so that sess.inRacemode is set for initialization
		ClientBegin( ent->s.number, qfalse );

		ent->client->switchTeamTime = level.time + 5000;
	}
}

/*
=================
Cmd_RacerVisibility_f
=================
*/
void Cmd_RacerVisibility_f( gentity_t *ent, int mode ) {
	if ( !ent || !ent->client ) {
		return;
	}

	gclient_t *client = ent->client;

	// mode: 0 to hide, 1 to show, 2 to toggle
	qboolean show;

	if ( client->sess.inRacemode ) {
		// in racemode, use the flag

		if ( mode == 0 ) {
			show = qfalse;
		} else if ( mode == 1 ) {
			show = qtrue;
		} else {
			show = client->sess.racemodeFlags & RMF_HIDERACERS ? qtrue : qfalse;
		}

		if ( show ) {
			trap_SendServerCommand( ent - g_entities, "print \""S_COLOR_GREEN"Other racers VISIBLE\n\"" );
			client->sess.racemodeFlags &= ~RMF_HIDERACERS;
		} else {
			trap_SendServerCommand( ent - g_entities, "print \""S_COLOR_RED"Other racers HIDDEN\n\"" );
			client->sess.racemodeFlags |= RMF_HIDERACERS;
		}
	} else {
		// in game, use the separate variable

		if ( mode == 0 ) {
			show = qfalse;
		} else if ( mode == 1 ) {
			show = qtrue;
		} else {
			show = client->sess.seeRacersWhileIngame ? qfalse : qtrue;
		}

		if ( show ) {
			trap_SendServerCommand( ent - g_entities, "print \""S_COLOR_GREEN"Racers VISIBLE\n\"" );
			client->sess.seeRacersWhileIngame = qtrue;
		} else {
			trap_SendServerCommand( ent - g_entities, "print \""S_COLOR_RED"Racers HIDDEN\n\"" );
			client->sess.seeRacersWhileIngame = qfalse;
		}
	}
}

void Cmd_BotVisibility_f(gentity_t *ent, int mode) {
	if (!ent || !ent->client) {
		return;
	}

	gclient_t *client = ent->client;

	// mode: 0 to hide, 1 to show, 2 to toggle
	qboolean show;

	if (client->sess.inRacemode) {
		// in racemode, use the flag

		if (mode == 0) {
			show = qfalse;
		}
		else if (mode == 1) {
			show = qtrue;
		}
		else {
			show = client->sess.racemodeFlags & RMF_HIDEBOTS ? qtrue : qfalse;
		}

		if (show) {
			trap_SendServerCommand(ent - g_entities, "print \""S_COLOR_GREEN"Aim practice bots VISIBLE\n\"");
			client->sess.racemodeFlags &= ~RMF_HIDEBOTS;
		}
		else {
			trap_SendServerCommand(ent - g_entities, "print \""S_COLOR_RED"Aim practice bots HIDDEN\n\"");
			client->sess.racemodeFlags |= RMF_HIDEBOTS;
		}
	}
	else {
		// in game, use the separate variable

		if (mode == 0) {
			show = qfalse;
		}
		else if (mode == 1) {
			show = qtrue;
		}
		else {
			show = client->sess.seeAimBotsWhileIngame ? qfalse : qtrue;
		}

		if (show) {
			trap_SendServerCommand(ent - g_entities, "print \""S_COLOR_GREEN"Aim practice bots VISIBLE\n\"");
			client->sess.seeAimBotsWhileIngame = qtrue;
		}
		else {
			trap_SendServerCommand(ent - g_entities, "print \""S_COLOR_RED"Aim practice bots HIDDEN\n\"");
			client->sess.seeAimBotsWhileIngame = qfalse;
		}
	}
}

/*
=================
Cmd_Amtelemark_f
=================
*/
void Cmd_Amtelemark_f( gentity_t *ent ) {
	if ( !ent || !ent->client ) {
		return;
	}

	if ( ent->health <= 0 ) {
		return;
	}

	gclient_t *client = ent->client;

	if ( client->ps.fallingToDeath ) {
		return;
	}

	if ( !client->ps.stats[STAT_RACEMODE] ) {
		trap_SendServerCommand( ent - g_entities, "print \"You must be in racemode or spectating a racer to use this command.\n\"" );
		return;
	}

	const int argc = trap_Argc();
	char s[16];

	vec3_t newOrigin;
	float newYaw, newPitch;

	if ( argc >= 4 ) {
		// amtelemark <x> <y> <z>
		trap_Argv( 1, s, sizeof( s ) );
		newOrigin[0] = ( float )atoi( s );

		trap_Argv( 2, s, sizeof( s ) );
		newOrigin[1] = ( float )atoi( s );

		trap_Argv( 3, s, sizeof( s ) );
		newOrigin[2] = ( float )atoi( s );
	} else {
		// use current position
		VectorCopy( client->ps.origin, newOrigin );
	}

	if ( argc >= 5 ) {
		// amtelemark <x> <y> <z> <yaw>
		trap_Argv( 4, s, sizeof( s ) );
		newYaw = ( float )atoi( s );
	} else {
		// use current yaw
		newYaw = client->ps.viewangles[YAW];
	}

	if ( argc >= 6 ) {
		// amtelemark <x> <y> <z> <yaw> <pitch>
		trap_Argv( 5, s, sizeof( s ) );
		newPitch = ( float )atoi( s );
	} else {
		// use current pitch
		newPitch = client->ps.viewangles[PITCH];
	}

	VectorCopy( newOrigin, client->sess.telemarkOrigin );
	client->sess.telemarkYawAngle = newYaw;
	client->sess.telemarkPitchAngle = newPitch;

	trap_SendServerCommand( ent - g_entities, va( "print \"Teleport Marker: ^3<%i, %i, %i> %i, %i\n\"",
		( int )client->sess.telemarkOrigin[0],
		( int )client->sess.telemarkOrigin[1],
		( int )client->sess.telemarkOrigin[2],
		( int )client->sess.telemarkYawAngle,
		( int )client->sess.telemarkPitchAngle )
	);
}

/*
=================
Cmd_Amtele_f
=================
*/
void Cmd_Amtele_f( gentity_t *ent ) {
	if ( !ent || !ent->client ) {
		return;
	}

	gclient_t *client = ent->client;

	if ( !client->sess.inRacemode ) {
		trap_SendServerCommand( ent - g_entities, "print \"You cannot use this command outside of racemode\n\"" );
		return;
	}

	if (ent->aimPracticeEntBeingUsed && ent->aimPracticeEntBeingUsed->isAimPracticePack) { // pressed amtele bind while doing aim practice
		qboolean someoneElseUsingThisPack = qfalse;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED ||
				thisEnt == ent || !thisEnt->client->sess.inRacemode || thisEnt->aimPracticeEntBeingUsed != ent->aimPracticeEntBeingUsed) {
				continue;
			}
			someoneElseUsingThisPack = qtrue;
			break;
		}

		ent->numAimPracticeSpawns = 0;
		ent->numTotalAimPracticeHits = 0;
		memset(ent->numAimPracticeHitsOfWeapon, 0, sizeof(ent->numAimPracticeHitsOfWeapon));

		if (someoneElseUsingThisPack) { // someone else is using this pack; just reset their stats and start on the next respawn
			CenterPrintToPlayerAndFollowers(ent, "Restarting...");
		}
		else { // we are the only one using this pack; go ahead and restart it immediately so they don't have to wait
			RandomizeAndRestartPack(ent->aimPracticeEntBeingUsed->isAimPracticePack);
		}
		return;
	}

	if ( client->sess.telemarkOrigin[0] == 0 &&
		client->sess.telemarkOrigin[1] == 0 &&
		client->sess.telemarkOrigin[2] == 0 &&
		client->sess.telemarkYawAngle == 0 &&
		client->sess.telemarkPitchAngle == 0 ) {

		trap_SendServerCommand( ent - g_entities, "print \"No telemark set!\n\"" );
		return;
	}

	G_TeleportRacerToTelemark( ent );
}

/*
=================
Cmd_AmAutoSpeed_f
=================
*/
void Cmd_AmAutoSpeed_f( gentity_t *ent ) {
	if ( !ent || !ent->client ) {
		return;
	}

	gclient_t *client = ent->client;

	if ( !client->sess.inRacemode ) {
		trap_SendServerCommand( ent - g_entities, "print \"You cannot use this command outside of racemode\n\"" );
		return;
	}

	if ( !( client->sess.racemodeFlags & RMF_DONTTELEWITHSPEED) ) {
		trap_SendServerCommand( ent - g_entities, "print \""S_COLOR_RED"Auto speed on teleport OFF\n\"" );
		client->sess.racemodeFlags |= RMF_DONTTELEWITHSPEED;
	} else {
		trap_SendServerCommand( ent - g_entities, "print \""S_COLOR_GREEN"Auto speed on teleport ON\n\"" );
		client->sess.racemodeFlags &= ~RMF_DONTTELEWITHSPEED;
	}
}

/*
=================
Cmd_Emote_f
=================
*/
#define NUM_EMOTES				LEGS_TURN180
#define NUM_EMOTE_COMBINATIONS	(NUM_EMOTES * NUM_EMOTES)
#define MIN_EMOTE_TIME			1000
#define MAX_EMOTE_TIME			10000

void Cmd_Emote_f(gentity_t* ent) {
	if (!ent || !ent->client) {
		return;
	}

	if (!g_raceEmotes.integer) {
		trap_SendServerCommand(ent - g_entities, "print \"Emotes are disabled\n\"");
		return;
	}

	gclient_t* client = ent->client;

	if (!client->sess.inRacemode) {
		trap_SendServerCommand(ent - g_entities, "print \"You cannot use this command outside of racemode\n\"");
		return;
	}

	if (client->ps.stats[STAT_HEALTH] <= 0 ||
		client->ps.pm_type != PM_NORMAL ||
		client->ps.pm_flags & PMF_STUCK_TO_WALL)
	{
		return;
	}

	char p[MAX_TOKEN_CHARS] = { 0 };
	int extraTime = 0;

	if (trap_Argc() > 1) {
		trap_Argv(1, p, sizeof(p));

		if (trap_Argc() > 2) {
			char s[MAX_TOKEN_CHARS];
			trap_Argv(2, s, sizeof(s));
			extraTime = Com_Clampi(0, MAX_EMOTE_TIME, atoi(s));
		}
	}

	unsigned int hash;

	if (VALIDSTRING(p)) { // manually-specified animation string
		hash = XXH32(p, strlen(p), 0x69420) % (unsigned)NUM_EMOTE_COMBINATIONS;
	} else { // no arg == generate random animations
		hash = rand() & 0xff;
		hash |= (rand() & 0xff) << 8;
		hash |= (rand() & 0xff) << 16;
		hash &= (unsigned)NUM_EMOTE_COMBINATIONS;
	}

	unsigned int torso = hash / (unsigned)NUM_EMOTES;
	unsigned int legs = hash % (unsigned)NUM_EMOTES;

	client->ps.torsoAnim = (signed)torso;
	client->ps.legsAnim = (signed)legs;

	client->ps.torsoTimer = BG_AnimLength(ent->localAnimIndex, client->ps.torsoAnim);
	client->ps.legsTimer = BG_AnimLength(ent->localAnimIndex, client->ps.legsAnim);

	// let clients add some extra time to anims up to 10 secs in total
	client->ps.torsoTimer = Com_Clampi(MIN_EMOTE_TIME, MAX_EMOTE_TIME, client->ps.torsoTimer + extraTime);
	client->ps.legsTimer = Com_Clampi(MIN_EMOTE_TIME, MAX_EMOTE_TIME, client->ps.legsTimer + extraTime);

	// any run is invalid until amtele/selfkill
	client->runInvalid = qtrue;

	// if they have a flag, remove it from them so they can't confuse it with a valid run
	if (client->ps.powerups[PW_REDFLAG] || client->ps.powerups[PW_BLUEFLAG]) {
		client->ps.powerups[PW_REDFLAG] = client->ps.powerups[PW_BLUEFLAG] = 0;
	}
}

/*
=================
Cmd_Stats_f
=================
*/
void Cmd_Stats_f( gentity_t *ent ) {

	}

int G_ItemUsable(playerState_t *ps, int forcedUse)
{
	vec3_t fwd, fwdorg, dest, pos;
	vec3_t yawonly;
	vec3_t mins, maxs;
	vec3_t trtest;
	trace_t tr;

	//is client alive???
	if (ps->stats[STAT_HEALTH] <= 0){
		return 0;
	}

	if (ps->m_iVehicleNum)
	{
		return 0;
	}
	
	if (ps->pm_flags & PMF_USE_ITEM_HELD)
	{ //force to let go first
		return 0;
	}

	if (!forcedUse)
	{
		forcedUse = bg_itemlist[ps->stats[STAT_HOLDABLE_ITEM]].giTag;
	}

	if (!BG_IsItemSelectable(ps, forcedUse))
	{
		return 0;
	}

	switch (forcedUse)
	{
	case HI_MEDPAC:
	case HI_MEDPAC_BIG:
		if (ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH])
		{
			return 0;
		}

		if (ps->stats[STAT_HEALTH] <= 0)
		{
			return 0;
		}

		return 1;
	case HI_SEEKER:
		if (ps->eFlags & EF_SEEKERDRONE)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SEEKER_ALREADYDEPLOYED);
			return 0;
		}

		return 1;
	case HI_SENTRY_GUN:
		if (ps->fd.sentryDeployed)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_ALREADYPLACED);
			return 0;
		}

		yawonly[ROLL] = 0;
		yawonly[PITCH] = 0;
		yawonly[YAW] = ps->viewangles[YAW];

		VectorSet( mins, -8, -8, 0 );
		VectorSet( maxs, 8, 8, 24 );

		AngleVectors(yawonly, fwd, NULL, NULL);

		fwdorg[0] = ps->origin[0] + fwd[0]*64;
		fwdorg[1] = ps->origin[1] + fwd[1]*64;
		fwdorg[2] = ps->origin[2] + fwd[2]*64;

		trtest[0] = fwdorg[0] + fwd[0]*16;
		trtest[1] = fwdorg[1] + fwd[1]*16;
		trtest[2] = fwdorg[2] + fwd[2]*16;

		trap_Trace(&tr, ps->origin, mins, maxs, trtest, ps->clientNum, MASK_PLAYERSOLID);

		if ((tr.fraction != 1 && tr.entityNum != ps->clientNum) || tr.startsolid || tr.allsolid)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_NOROOM);
			return 0;
		}

		return 1;
	case HI_SHIELD:
		mins[0] = -8;
		mins[1] = -8;
		mins[2] = 0;

		maxs[0] = 8;
		maxs[1] = 8;
		maxs[2] = 8;

		AngleVectors (ps->viewangles, fwd, NULL, NULL);
		fwd[2] = 0;
		VectorMA(ps->origin, 64, fwd, dest);
		trap_Trace(&tr, ps->origin, mins, maxs, dest, ps->clientNum, MASK_SHOT );
		if (tr.fraction > 0.9 && !tr.startsolid && !tr.allsolid)
		{
			VectorCopy(tr.endpos, pos);
			VectorSet( dest, pos[0], pos[1], pos[2] - 4096 );
			trap_Trace( &tr, pos, mins, maxs, dest, ps->clientNum, MASK_SOLID );
			if ( !tr.startsolid && !tr.allsolid )
			{
				return 1;
			}
		}
		G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SHIELD_NOROOM);
		return 0;
	case HI_JETPACK: //do something?
		return 1;
	case HI_HEALTHDISP:
		return 1;
	case HI_AMMODISP:
		return 1;
	case HI_EWEB:
		return 1;
	case HI_CLOAK:
		return 1;
	default:
		return 1;
	}
}

void saberKnockDown(gentity_t *saberent, gentity_t *saberOwner, gentity_t *other);

void Cmd_ToggleSaber_f(gentity_t *ent)
{
	if (ent->client->ps.fd.forceGripCripple)
	{ //if they are being gripped, don't let them unholster their saber
		if (ent->client->ps.saberHolstered)
		{
			return;
		}
	}

	if (ent->client->ps.saberInFlight)
	{
		if (ent->client->ps.saberEntityNum)
		{ //turn it off in midair
			saberKnockDown(&g_entities[ent->client->ps.saberEntityNum], ent, ent);
		}
		return;
	}

	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.saberLockTime >= level.time)
	{
		return;
	}

	if (ent->client && ent->client->ps.weaponTime < 1)
	{
		if (ent->client->ps.saberHolstered == 2)
		{
			ent->client->ps.saberHolstered = 0;

			if (ent->client->saber[0].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			}
			if (ent->client->saber[1].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
			}
		}
		else
		{
			ent->client->ps.saberHolstered = 2;
			if (ent->client->saber[0].soundOff)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
			}
			if (ent->client->saber[1].soundOff &&
				ent->client->saber[1].model[0])
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
			}
			//prevent anything from being done for 400ms after holster
			ent->client->ps.weaponTime = 400;
		}
	}
}

#define ROCK_PAPER_SCISSORS_CHALLENGE_VALID_DURATION	(10000)
#define ROCK_PAPER_SCISSORS_CHALLENGE_COOLDOWN_TIME		(2000)
static qboolean RockPaperScissors(gentity_t *ent) {
	if (!g_rockPaperScissors.integer || level.intermissiontime || g_gametype.integer != GT_CTF)
		return qfalse;

	if (!ent || !ent->client || ent - g_entities >= MAX_CLIENTS || (ent->r.svFlags & SVF_BOT) || ent->health <= 0 || ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
		(ent->client->rockPaperScissorsOtherClientNum < MAX_CLIENTS && ent->client->rockPaperScissorsStartTime && level.time - ent->client->rockPaperScissorsStartTime < ROCK_PAPER_SCISSORS_DURATION) ||
		ent->client->ps.saberInFlight || ent->aimPracticeEntBeingUsed || ent->client->rockPaperScissorsBothChosenTime || ent->client->ps.fallingToDeath)
		return qfalse;

	// make sure they are standing still
	if (ent->client->ps.velocity[0] || ent->client->ps.velocity[1] || ent->client->ps.velocity[2])
		return qfalse;

	// don't allow it in live games
	if (level.wasRestarted && !level.someoneWasAFK)
		return qfalse;

	trace_t tr;
	vec3_t forward, fwdOrg;
	AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);
	const float duelrange = 1024;
	fwdOrg[0] = ent->client->ps.origin[0] + forward[0] * duelrange;
	fwdOrg[1] = ent->client->ps.origin[1] + forward[1] * duelrange;
	fwdOrg[2] = (ent->client->ps.origin[2] + ent->client->ps.viewheight) + forward[2] * duelrange;

	trap_Trace(&tr, ent->client->ps.origin, NULL, NULL, fwdOrg, ent->s.number, MASK_PLAYERSOLID);
	if (tr.fraction == 1 || tr.entityNum >= MAX_CLIENTS)
		return qfalse;

	gentity_t *targ = &g_entities[tr.entityNum];

	if (!targ || !targ->client || !targ->inuse ||
		targ->health < 1 || targ->client->ps.stats[STAT_HEALTH] < 1 ||
		targ->client->sess.sessionTeam == TEAM_SPECTATOR ||
		targ->client->ps.duelInProgress ||
		targ->client->ps.saberInFlight ||
		targ->client->sess.inRacemode != ent->client->sess.inRacemode ||
		(targ->r.svFlags & SVF_BOT) ||
		targ->aimPracticeEntBeingUsed ||
		targ->client->ps.fallingToDeath ||
		targ->client->rockPaperScissorsBothChosenTime ||
		(targ->client->rockPaperScissorsOtherClientNum < MAX_CLIENTS && targ->client->rockPaperScissorsStartTime && level.time - targ->client->rockPaperScissorsStartTime < ROCK_PAPER_SCISSORS_DURATION)
		) {
		return qfalse;
	}

	if (ent->client->rockPaperScissorsChallengeTime && level.time - ent->client->rockPaperScissorsChallengeTime < ROCK_PAPER_SCISSORS_CHALLENGE_COOLDOWN_TIME)
		return qtrue;

	//auto accept for testing, remove in release!
	if (targ->client->rockPaperScissorsOtherClientNum == ent - g_entities &&
		targ->client->rockPaperScissorsChallengeTime &&
		level.time - targ->client->rockPaperScissorsChallengeTime < ROCK_PAPER_SCISSORS_CHALLENGE_VALID_DURATION) {
		trap_SendServerCommand(ent - g_entities, "cp \"Enter ^5r^7, ^5p^7, or ^5s^7 in chat\"");
		trap_SendServerCommand(targ - g_entities, "cp \"Enter ^5r^7, ^5p^7, or ^5s^7 in chat\"");

		// don't print the exact same message for people following them, so that idiots don't try to type r/p/s in chat
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (thisEnt == ent || thisEnt == targ || !thisEnt->inuse || !thisEnt->client || thisEnt->client->sess.spectatorState != SPECTATOR_FOLLOW)
				continue;
			if (thisEnt->client->sess.spectatorClient != ent - g_entities && thisEnt->client->sess.spectatorClient != targ - g_entities)
				continue;
			trap_SendServerCommand(thisEnt - g_entities, "cp \"Waiting for players to choose...\"");
		}

		ent->client->rockPaperScissorsStartTime = targ->client->rockPaperScissorsStartTime = level.time;
	}
	else {
		CenterPrintToPlayerAndFollowers(targ, va("%s^7 has challenged you to rock paper scissors!", ent->client->pers.netname));
		CenterPrintToPlayerAndFollowers(ent, va("You challenged %s^7 to rock paper scissors", targ->client->pers.netname));
	}

	ent->client->ps.forceHandExtend = HANDEXTEND_DUELCHALLENGE;
	ent->client->ps.forceHandExtendTime = level.time + 1000;

	ent->client->rockPaperScissorsOtherClientNum = targ - g_entities;
	ent->client->rockPaperScissorsChallengeTime = level.time;
	return qtrue;
}

extern vmCvar_t		d_saberStanceDebug;

extern qboolean WP_SaberCanTurnOffSomeBlades( saberInfo_t *saber );
void Cmd_SaberAttackCycle_f(gentity_t *ent)
{
	int selectLevel = 0;
	qboolean usingSiegeStyle = qfalse;
	
	if ( !ent || !ent->client )
	{
		return;
	}

	if (RockPaperScissors(ent))
		return;

	if (ent->client->saber[0].model[0] && ent->client->saber[1].model[0])
	{ //no cycling for akimbo
		if ( WP_SaberCanTurnOffSomeBlades( &ent->client->saber[1] ) )
		{//can turn second saber off 
			if ( ent->client->ps.saberHolstered == 1 )
			{//have one holstered
				//unholster it
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
				ent->client->ps.saberHolstered = 0;
				//g_active should take care of this, but...
				ent->client->ps.fd.saberAnimLevel = SS_DUAL;
			}
			else if ( ent->client->ps.saberHolstered == 0 )
			{//have none holstered
				if ( (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
				{//can't turn it off manually
				}
				else if ( ent->client->saber[1].bladeStyle2Start > 0
					&& (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
				{//can't turn it off manually
				}
				else
				{
					//turn it off
					if (ent->client->ps.weapon == WP_SABER)
						G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
					ent->client->ps.saberHolstered = 1;
					//g_active should take care of this, but...
					ent->client->ps.fd.saberAnimLevel = SS_FAST;
				}
			}

			if (d_saberStanceDebug.integer)
			{
				trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle dual saber blade.\n\"") );
			}
			return;
		}
	}
	else if (ent->client->saber[0].numBlades > 1
		&& WP_SaberCanTurnOffSomeBlades( &ent->client->saber[0] ) )
	{ //use staff stance then.
		if ( ent->client->ps.saberHolstered == 1 )
		{//second blade off
			if ( ent->client->ps.saberInFlight )
			{//can't turn second blade back on if it's in the air, you naughty boy!
				if (d_saberStanceDebug.integer)
				{
					trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade in air.\n\"") );
				}
				return;
			}
			//turn it on
			G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			ent->client->ps.saberHolstered = 0;
			//g_active should take care of this, but...
			if ( ent->client->saber[0].stylesForbidden )
			{//have a style we have to use
				WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
				if ( ent->client->ps.weaponTime <= 0 )
				{ //not busy, set it now
					ent->client->ps.fd.saberAnimLevel = selectLevel;
				}
				else
				{ //can't set it now or we might cause unexpected chaining, so queue it
					ent->client->saberCycleQueue = selectLevel;
				}
			}
		}
		else if ( ent->client->ps.saberHolstered == 0 )
		{//both blades on
			if ( (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
			{//can't turn it off manually
			}
			else if ( ent->client->saber[0].bladeStyle2Start > 0
				&& (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
			{//can't turn it off manually
			}
			else
			{
				//turn second one off
				if (ent->client->ps.weapon == WP_SABER)
					G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				ent->client->ps.saberHolstered = 1;
				//g_active should take care of this, but...
				if ( ent->client->saber[0].singleBladeStyle != SS_NONE )
				{
					if ( ent->client->ps.weaponTime <= 0 )
					{ //not busy, set it now
						ent->client->ps.fd.saberAnimLevel = ent->client->saber[0].singleBladeStyle;
					}
					else
					{ //can't set it now or we might cause unexpected chaining, so queue it
						ent->client->saberCycleQueue = ent->client->saber[0].singleBladeStyle;
					}
				}
			}
		}
		if (d_saberStanceDebug.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade.\n\"") );
		}
		return;
	}

	if (ent->client->saberCycleQueue)
	{ //resume off of the queue if we haven't gotten a chance to update it yet
		selectLevel = ent->client->saberCycleQueue;
	}
	else
	{
		selectLevel = ent->client->ps.fd.saberAnimLevel;
	}

	if (g_gametype.integer == GT_SIEGE &&
		ent->client->siegeClass != -1 &&
		bgSiegeClasses[ent->client->siegeClass].saberStance)
	{ //we have a flag of useable stances so cycle through it instead
		int i = selectLevel+1;

		usingSiegeStyle = qtrue;

		while (i != selectLevel)
		{ //cycle around upward til we hit the next style or end up back on this one
			if (i >= SS_NUM_SABER_STYLES)
			{ //loop back around to the first valid
				i = SS_FAST;
			}

			if (bgSiegeClasses[ent->client->siegeClass].saberStance & (1 << i))
			{ //we can use this one, select it and break out.
				selectLevel = i;
				break;
			}
			i++;
		}

		if (d_saberStanceDebug.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle given class stance.\n\"") );
		}
	}
	else
	{
		int maxLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];
		selectLevel++;

		if ( g_balanceSaber.integer & SB_OFFENSE ) {
			if ( g_balanceSaber.integer & SB_OFFENSE_TAV_DES ) {
				maxLevel += 2; // level 2 gives SS_DESANN, level 3 gives SS_TAVION
			} else {
				maxLevel = SS_STRONG; // all stances are available regardless of the level
			}
		}

		if ( selectLevel > maxLevel )
		{
			selectLevel = FORCE_LEVEL_1;
		}
		if (d_saberStanceDebug.integer)
		{
			trap_SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle stance normally.\n\"") );
		}
	}

	if ( !usingSiegeStyle )
	{
		//make sure it's valid, change it if not
		WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
	}

	if (ent->client->ps.weaponTime <= 0)
	{ //not busy, set it now
		ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = selectLevel;
	}
	else
	{ //can't set it now or we might cause unexpected chaining, so queue it
		ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = selectLevel;
	}
}

qboolean G_OtherPlayersDueling(void)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->inuse && ent->client && ent->client->ps.duelInProgress)
		{
			return qtrue;
		}
		i++;
	}

	return qfalse;
}

#if 0
void listPools( void* context,
    int pool_id,
    const char* short_name,
    const char* long_name )
{
    ListPoolsContext* thisContext = (ListPoolsContext*)context;
    trap_SendServerCommand( thisContext->entity, va( "print \"%s^7 (%s^7)\n\"", short_name, long_name ) );
    ++(thisContext->count);
}

void listMapsInPools( void** context,
    const char* long_name,
    int pool_id,
    const char* mapname,
    int mapWeight )
{
	ListMapsInPoolContext* thisContext = *( ( ListMapsInPoolContext** )context );
    thisContext->pool_id = pool_id;
    thisContext->count++;
    Q_strncpyz( thisContext->long_name, long_name, sizeof( thisContext->long_name ) );
    trap_SendServerCommand( thisContext->entity, va( "print \" %s^7\n\"", mapname ) );
}
#endif

static void Cmd_MapPool_f(gentity_t* ent) {
	if (trap_Argc() > 1) { // print a list of the maps in a single pool
		char short_name[64] = { 0 };
        trap_Argv( 1, short_name, sizeof( short_name ) );
		if (!short_name[0]) {
			PrintIngame(ent - g_entities, "Please specify a pool short name.\n");
			return;
		}

		list_t mapList = { 0 };
		void *ctxPtr = &mapList;
		char poolLongName[64] = { 0 };
		G_DBListMapsInPool( short_name, "", listMapsInPools, (void **)&ctxPtr, (char *)poolLongName, sizeof(poolLongName), 0);

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

			char buf[4096] = { 0 };
			Table_WriteToBuffer(t, buf, sizeof(buf), qtrue, -1);
			Q_strcat(buf, sizeof(buf), "\n");
			PrintIngame(ent - g_entities, buf);
		}
		Table_Destroy(t);
		ListClear(&mapList);

        PrintIngame(ent - g_entities, va( "Found %i maps for pool with long name ^5%s^7 and short name ^5%s^7.\n", numMaps, poolLongName, short_name) );
		PrintIngame(ent - g_entities, "To see a list of all pools, use ^5pools^7 without any arguments.\n");
	}
	else { // print a list of pools
		list_t poolList = { 0 };

        G_DBListPools( listPools, &poolList );

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

			char buf[4096] = { 0 };
			Table_WriteToBuffer(t, buf, sizeof(buf), qtrue, -1);
			Q_strcat(buf, sizeof(buf), "\n");
			PrintIngame(ent - g_entities, buf);
		}
		Table_Destroy(t);
		ListClear(&poolList);

        PrintIngame( ent - g_entities, va( "Found %i map pools.\n", numPools) );
		PrintIngame(ent - g_entities, "To see a list of maps in a specific pool, use ^5pools <pool short name>^7\n");
	}
}

const char *GetTierStringForTier(mapTier_t tier) {
	switch (tier) {
	case MAPTIER_S:	return "^6S TIER";
	case MAPTIER_A:	return "^2A TIER";
	case MAPTIER_B:	return "^3B TIER";
	case MAPTIER_C:	return "^8C TIER";
	case MAPTIER_F:	return "^0F TIER";
	default:		return NULL;
	}
}

const char *GetTierColorForTier(mapTier_t tier) {
	switch (tier) {
	case MAPTIER_S:	return "^6";
	case MAPTIER_A:	return "^2";
	case MAPTIER_B:	return "^3";
	case MAPTIER_C:	return "^8";
	case MAPTIER_F:	return "^0";
	default:		return NULL;
	}
}

mapTier_t GetMapTierForChar(char c) {
	unsigned char u = tolower((unsigned char)c);
	switch (u) {
	case 's':	return MAPTIER_S;
	case 'a':	return MAPTIER_A;
	case 'b':	return MAPTIER_B;
	case 'c':	return MAPTIER_C;
	case 'f':	return MAPTIER_F;
	default:	return MAPTIER_INVALID;
	}
}

// get the casual nickname from a map filename
// e.g. "mp/ctf_kejim" outputs "kejim"
qboolean GetShortNameForMapFileName(const char *mapFileName, char *out, size_t outSize) {
	if (!VALIDSTRING(mapFileName))
		return qfalse;

	const char *ptr = mapFileName;

	if (!Q_stricmpn(mapFileName, "mp/", 3))
		ptr += 3;
	if (*ptr && !Q_stricmpn(ptr, "ctf_", 4))
		ptr += 4;

	if (*ptr) {
		Q_strncpyz(out, ptr, outSize);
		Q_strlwr(out);
		return qtrue;
	}
	else {
		return qfalse;
	}
}

// trims e.g. "_v69" from a string and writes to the supplied output buffer
void TrimMapVersion(const char *input, char *output, size_t length) {
	char *copy = strdup(input);

	// find last underscore
	const char *underscore = strrchr(copy, '_');

	if (!underscore) { // no underscore; just copy input
		Q_strncpyz(output, copy, length);
		free(copy);
		return;
	}

	// check if the underscore is followed by 'v' and one or more digits
	const char *v = underscore + 1;
	if (*v != 'v' && *v != 'V') {
		Q_strncpyz(output, copy, length);
		free(copy);
		return;
	}

	v++;
	while (isdigit(*v))
		v++;

	if (v == underscore + 1) { // no digits after underscore; just copy input
		Q_strncpyz(output, copy, length);
		free(copy);
		return;
	}

	// write to buffer
	size_t writeLen = underscore - copy;
	if (writeLen >= length)
		writeLen = length - 1;
	strncpy(output, copy, writeLen);
	output[writeLen] = '\0';
	free(copy);
}

// attempts to find a map filename from a casually typed name
// e.g. "kejim" and "ctf_kejim" both output "mp/ctf_kejim"
qboolean GetMatchingMap(const char *in, char *out, size_t outSize) {
	if (!VALIDSTRING(in) || !out || !outSize)
		return qfalse;

	char lowercase[MAX_QPATH] = { 0 };
	Q_strncpyz(lowercase, in, sizeof(lowercase));
	Q_strlwr(lowercase);

	char *str = va("mp/ctf_%s", lowercase);
	if (!G_DoesMapSupportGametype(str, GT_CTF)) { // returns 0 if it does
		Q_strncpyz(out, str, outSize);
		return qtrue;
	}

	str = va("ctf_%s", lowercase);
	if (!G_DoesMapSupportGametype(str, GT_CTF)) { // returns 0 if it does
		Q_strncpyz(out, str, outSize);
		return qtrue;
	}

	str = va("mp/%s", lowercase);
	if (!G_DoesMapSupportGametype(str, GT_CTF)) { // returns 0 if it does
		Q_strncpyz(out, str, outSize);
		return qtrue;
	}

	str = lowercase;
	if (!G_DoesMapSupportGametype(str, GT_CTF)) { // returns 0 if it does
		Q_strncpyz(out, str, outSize);
		return qtrue;
	}

	return qfalse;
}

// e.g. an input of 2.12345 returns MAPTIER_C (2)
mapTier_t MapTierForDouble(double average) {
	if (average < 1.0 || average > 5.0) { // should be impossible
		assert(qfalse);
		return MAPTIER_INVALID;
	}

	// in case of a tie, round to the higher tier (idk, this was an arbitrary decision)

	if (average < 1.5)
		return MAPTIER_F;
	else if (average < 2.5)
		return MAPTIER_C;
	else if (average < 3.5)
		return MAPTIER_B;
	else if (average < 4.5)
		return MAPTIER_A;
	else
		return MAPTIER_S;
}

static void PrintTiersHelp(int clientNum, qboolean hasAccount) {
	PrintIngame(clientNum, "Usage:\n" \
		"^7tier list                         - view the current 4v4 tier list\n"
		"^9tier community                    - view the overall community 4v4 tier list\n"
		"\n"
		"^7tier here                         - view ratings for the current map\n"
		"^9tier [map]                        - view ratings for a particular map\n"
		"\n"
		"^7tier [account name/player name/#] - view someone's tier list\n"
		"^9tier players                      - see which players have rated maps\n"
		"^7tier stats                        - view stats about map tiers^7\n"
		"\n"
		"^9tier mylist                       - view your personal tier list\n"
		"^7tier set [map] [tier]             - set a map's tier in your personal list\n"
		"^9tier remove [map]                 - remove a map's tier from your personal list\n"
		"^7tier reset <optional tier>        - reset your personal list (or just one tier)\n");
	if (!hasAccount)
		PrintIngame(clientNum, "^3You must have an account in order to create a tier list. Contact an admin for help setting up an account.^7\n");
}

static void Cmd_Tier_f(gentity_t *ent) {
	assert(ent);
	int clientNum = ent - g_entities;

	if (!g_vote_tierlist.integer) {
		PrintIngame(clientNum, "Tierlist is disabled.\n");
		return;
	}

	int args = trap_Argc() - 1;

	qboolean hasAccount = !!(ent->client && ent->client->account && VALIDSTRING(ent->client->account->name));

	char arg1[MAX_STRING_CHARS], arg2[MAX_STRING_CHARS], arg3[MAX_STRING_CHARS];
	trap_Argv(1, arg1, sizeof(arg1));
	trap_Argv(2, arg2, sizeof(arg2));
	trap_Argv(3, arg3, sizeof(arg3));

	if (args <= 0 || !arg1[0] || isspace(arg1[0]) || !Q_stricmp(arg1, "help")) {
		PrintTiersHelp(clientNum, hasAccount);
		return;
	}

	qboolean tryingUnknownSubcommand = qfalse;

	if (!Q_stricmp(arg1, "mylist")) {
		if (!hasAccount) {
			PrintIngame(clientNum, "^3You must have an account in order to use this feature. Contact an admin for help setting up an account.^7\n");
			return;
		}
		printMyOwnList:; // lick my nuts
		if (args > 1 && !Q_stricmp(arg2, "unrated")) {
			if (!G_DBPrintPlayerUnratedList(ent->client->account->id, clientNum, "Maps you have not rated:"))
				PrintIngame(clientNum, "There are no maps that you have not rated.\n");
			return;
		}
		if (!G_DBPrintPlayerTierList(ent->client->account->id, clientNum, "Your tier list:"))
			PrintIngame(clientNum, "You do not have a tier list. Enter ^5tier set [map] [tier]^7 to start rating maps.\n");
		if (G_DBPrintPlayerUnratedList(ent->client->account->id, -1, NULL))
			PrintIngame(clientNum, "Enter ^5tier mylist unrated^7 to see which maps you have not yet rated.\n");
		PrintIngame(clientNum, "^3NOTE: ratings should be based on 4v4 gameplay only. Do not rate based on 5s, etc.!^7\n");
	}
	else if (!Q_stricmp(arg1, "community") || (!Q_stricmp(arg1, "list") && args > 1 && !Q_stricmp(arg2, "community"))) {
		if (!G_DBPrintTiersOfAllMaps(clientNum, qfalse, "Community map ratings:")) {
			PrintIngame(clientNum, "No maps have received ratings yet.\n");
		}
	}
	else if ((!Q_stricmp(arg1, "list") || !Q_stricmp(arg1, "player")) && args > 1 && arg2[0] && !isspace(arg2[0])) {
		tryFindPlayerFromNoArgs:; // blow my scrote
		int accountId = -1;
		char name[64] = { 0 };
		char *getNameFrom = tryingUnknownSubcommand ? arg1 : arg2;
		if (Q_isanumber(getNameFrom)) {
			int num = atoi(getNameFrom);
			if (num >= 0 && num < MAX_CLIENTS && g_entities[num].inuse && g_entities[num].client && g_entities[num].client->pers.connected != CON_DISCONNECTED) {
				if (g_entities[num].client->account) {
					accountId = g_entities[num].client->account->id;
					Q_strncpyz(name, g_entities[num].client->pers.netname, sizeof(name));
				}
				else {
					PrintIngame(clientNum, "%s^7 does not have a tier list.\n", g_entities[num].client->pers.netname);
					return;
				}
			}
			else {
				if (tryingUnknownSubcommand)
					PrintIngame(clientNum, "Unable to find any map, player, or subcommand matching '%s^7'\n", getNameFrom);
				else
					PrintIngame(clientNum, "Unable to find a player matching '%s^7'.\n", getNameFrom);
				return;
			}
		}
		else {
			char lowercase[MAX_QPATH] = { 0 };
			Q_strncpyz(lowercase, getNameFrom, sizeof(lowercase));
			Q_strlwr(lowercase);
			account_t account = { 0 };
			if (G_DBGetAccountByName(lowercase, &account)) {
				accountId = account.id;
				Q_strncpyz(name, account.name, sizeof(name));
			}
			else {
				gentity_t *found = G_FindClient(getNameFrom);
				if (found && found->client) {
					if (found->client->account) {
						accountId = found->client->account->id;
						Q_strncpyz(name, va("%s^7 (%s^7)", found->client->pers.netname, found->client->account->name), sizeof(name));
					}
					else {
						PrintIngame(clientNum, "%s^7 does not have a tier list.\n", found->client->pers.netname);
						return;
					}
				}
				else {
					if (tryingUnknownSubcommand)
						PrintIngame(clientNum, "Unable to find any map, player, or subcommand matching '%s^7'\n", getNameFrom);
					else
						PrintIngame(clientNum, "Unable to find a player matching '%s^7'.\n", getNameFrom);
					return;
				}
			}
		}

		if (accountId == -1)
			return; // sanity check

		if (hasAccount && accountId == ent->client->account->id)
			goto printMyOwnList; // suck my fucking ballsack

		char *getUnratedFrom;
		if (tryingUnknownSubcommand && args > 1 && arg2[0] && !isspace(arg2[0]))
			getUnratedFrom = arg2;
		else if (args > 2 && arg3[0] && !isspace(arg3[0]))
			getUnratedFrom = arg3;
		else
			getUnratedFrom = NULL;

		if (VALIDSTRING(getUnratedFrom) && !Q_stricmp(getUnratedFrom, "unrated")) {
			if (!G_DBPrintPlayerUnratedList(accountId, clientNum, va("Maps %s^7 has not rated:", name)))
				PrintIngame(clientNum, "There are no maps that %s^7 has not rated.\n", name);
			return;
		}

		char *prologue = strdup(va("%s^7's tier list:", name));
		if (!G_DBPrintPlayerTierList(accountId, clientNum, prologue))
			PrintIngame(clientNum, "%s^7 does not have a tier list.\n", name);
		free(prologue);
		if (G_DBPrintPlayerUnratedList(accountId, -1, NULL))
			PrintIngame(clientNum, "Enter ^5tier %s^5 unrated^7 to see which maps %s^7 has not yet rated.\n", getNameFrom, name);
	}
	else if (!Q_stricmp(arg1, "players") || !Q_stricmp(arg1, "lists")) {
		if (G_DBTierListPlayersWhoHaveRatedMaps(clientNum, "Players who have rated maps, in order of # of ratings given:")) {
			PrintIngame(clientNum, "Enter ^5tier [name]^7 to view someone's tier list.\n");
		}
		else {
			PrintIngame(clientNum, "No players have rated maps yet.\n");
		}
	}
	else if (!Q_stricmp(arg1, "set") || !Q_stricmp(arg1, "add")) {
		// set a map's tier in their list
		if (!hasAccount) {
			PrintIngame(clientNum, "^3You must have an account in order to use this feature. Contact an admin for help setting up an account.^7\n");
			return;
		}

		if (args < 3 || !arg2[0] || isspace(arg2[0]) || !arg3[0] || isspace(arg3[0])) {
			PrintIngame(clientNum, "Usage:   tier set [map] [tier]\nExample: ^5tier set kejim s^7 ==> sets mp/ctf_kejim to S tier in your list. Valid tiers are S, A, B, C, F.\n"
			"^3NOTE: ratings should be based on 4v4 gameplay only. Do not rate based on 5s, etc.!^7\n");
			return;
		}

		const char *enteredMap, *enteredTier;
		if (strlen(arg2) == 1 && strlen(arg3) > 1) { // they typed the tier first and the map second
			enteredTier = arg2;
			enteredMap = arg3;
		}
		else { // they typed the map first and the tier second
			enteredMap = arg2;
			enteredTier = arg3;
		}

		mapTier_t tier = GetMapTierForChar(*enteredTier);
		if (tier == MAPTIER_INVALID) {
			PrintIngame(clientNum, "'%s^7' is not a valid tier. Valid tiers are S, A, B, C, F.\n", enteredTier);
			return;
		}

		char mapFileName[MAX_QPATH] = { 0 };
		if (!GetMatchingMap(enteredMap, mapFileName, sizeof(mapFileName))) {
			PrintIngame(clientNum, "Unable to find any map matching '%s^7'\n", enteredMap);
			return;
		}

		char whitelistedFileName[MAX_QPATH] = { 0 }, liveFileName[MAX_QPATH] = { 0 };
		if (!G_DBTierlistMapIsWhitelisted(mapFileName, qtrue, whitelistedFileName, sizeof(whitelistedFileName), liveFileName, sizeof(liveFileName))) {
			PrintIngame(clientNum, "^5%s^7 (^9%s^7) is not eligible for tierlists.\n", enteredMap, mapFileName);
			return;
		}

		mapTier_t existingTier = G_DBGetTierOfSingleMap(va("%d", ent->client->account->id), whitelistedFileName, qfalse);
		if (existingTier != MAPTIER_INVALID && existingTier == tier) {
			// trying to move it to the tier it's already in
			PrintIngame(clientNum, "^5%s^7 (^9%s^7) is already in %s^7.\n", enteredMap, liveFileName, GetTierStringForTier(tier));
			return;
		}

		if (existingTier == MAPTIER_INVALID) {
			// adding a map that is not already in a tier
			qboolean result = G_DBAddMapToTierList(ent->client->account->id, whitelistedFileName, tier);
			if (result) {
				PrintIngame(clientNum, "Added ^5%s^7 (^9%s^7) to %s^7.\n", enteredMap, liveFileName, GetTierStringForTier(tier));
			}
			else {
				PrintIngame(clientNum, "Error adding ^5%s^7 (^9%s^7) to %s^7!\n", enteredMap, liveFileName, GetTierStringForTier(tier));
				G_LogPrintf("Client %d (%s^7) encountered an error trying to add %s (%s) to %s^7\n", clientNum, ent->client->pers.netname, enteredMap, liveFileName, GetTierStringForTier(tier));
			}
		}
		else {
			// moving it from a different tier
			qboolean result = G_DBAddMapToTierList(ent->client->account->id, whitelistedFileName, tier);
			if (result) {
				PrintIngame(clientNum, "Moved ^5%s^7 (^9%s^7) to %s^7 (was in %s^7).\n", enteredMap, liveFileName, GetTierStringForTier(tier), GetTierStringForTier(existingTier));
			}
			else {
				PrintIngame(clientNum, "Error moving ^5%s^7 (^9%s^7) to %s^7!\n", enteredMap, liveFileName, GetTierStringForTier(tier));
				G_LogPrintf("Client %d (%s^7) encountered an error trying to move %s (%s) to %s^7\n", clientNum, ent->client->pers.netname, enteredMap, liveFileName, GetTierStringForTier(tier));
			}
		}
	}
	else if (!Q_stricmp(arg1, "remove") || !Q_stricmp(arg1, "delete")) {
		// remove a map from their list
		if (!hasAccount) {
			PrintIngame(clientNum, "^3You must have an account in order to use this feature. Contact an admin for help setting up an account.^7\n");
			return;
		}

		if (args < 2 || !arg2[0] || isspace(arg2[0])) {
			PrintIngame(clientNum, "Usage:   tier remove [map]\nExample: ^5tier remove kejim^7 ==> removes mp/ctf_kejim from your list\n");
			return;
		}

		char mapFileName[MAX_QPATH] = { 0 };
		if (!GetMatchingMap(arg2, mapFileName, sizeof(mapFileName))) {
			PrintIngame(clientNum, "Unable to find any map matching '%s^7'\n", arg2);
			return;
		}

		char liveFileName[MAX_QPATH] = { 0 };
		G_DBGetLiveMapNameForMapName(mapFileName, liveFileName, sizeof(liveFileName));
		if (!liveFileName[0])
			Q_strncpyz(liveFileName, mapFileName, sizeof(liveFileName));

		mapTier_t existingTier = G_DBGetTierOfSingleMap(va("%d", ent->client->account->id), mapFileName, qfalse);
		if (existingTier == MAPTIER_INVALID) {
			PrintIngame(clientNum, "^5%s^7 (^9%s^7) is already not in your list.\n", arg2, liveFileName);
			return;
		}

		qboolean result = G_DBRemoveMapFromTierList(ent->client->account->id, mapFileName);
		if (result) {
			PrintIngame(clientNum, "Removed ^5%s^7 (^9%s^7).\n", arg2, liveFileName);
		}
		else {
			PrintIngame(clientNum, "Error removing ^5%s^7 (^9%s^7)!\n", arg2, liveFileName);
			G_LogPrintf("Client %d (%s^7) encountered an error trying to remove %s (%s)\n", clientNum, ent->client->pers.netname, arg2, liveFileName);
		}
	}
	else if (!Q_stricmp(arg1, "reset") || !Q_stricmp(arg1, "clear") || !Q_stricmp(arg1, "clr")) {
		// reset their list, or just one particular tier
		if (!hasAccount) {
			PrintIngame(clientNum, "^3You must have an account in order to use this feature. Contact an admin for help setting up an account.^7\n");
			return;
		}

		if (args >= 2 && arg2[0] && !isspace(arg2[0])) {
			// reset a particular tier
			mapTier_t tier = GetMapTierForChar(arg2[0]);
			if (tier == MAPTIER_INVALID) {
				PrintIngame(clientNum, "'%c^7' is not a valid tier. Valid tiers are S, A, B, F.\n", arg2[0]);
				return;
			}

			if (G_DBClearTierListTier(ent->client->account->id, tier)) {
				PrintIngame(clientNum, "Cleared all %s maps.\n", GetTierStringForTier(tier));
			}
			else {
				PrintIngame(clientNum, "Error clearing %s maps!\n", GetTierStringForTier(tier));
				G_LogPrintf("Client %d (%s^7) encountered an error trying to clear %s^7 maps\n", clientNum, ent->client->pers.netname, GetTierStringForTier(tier));
			}
		}
		else {
			// reset their entire list
			if (G_DBClearTierList(ent->client->account->id)) {
				PrintIngame(clientNum, "Your tier list has been completely reset.\n");
			}
			else {
				PrintIngame(clientNum, "Error reseting your tier list!\n");
				G_LogPrintf("Client %d (%s^7) encountered an error trying to clear their entire tier list\n", clientNum, ent->client->pers.netname);
			}
		}
	}
	else if (!Q_stricmp(arg1, "stats")) {
		G_DBTierStats(clientNum);
	}
	else if (!Q_stricmp(arg1, "list") || !Q_stricmp(arg1, "ingame") || !Q_stricmp(arg1, "current")) {
		if (!G_DBPrintTiersOfAllMaps(clientNum, qtrue, "Map ratings of current ingame players:")) {
			int ingame;
			CountPlayersIngame(NULL, &ingame, NULL);
			if (!G_DBPrintTiersOfAllMaps(clientNum, qfalse, ingame > 0 ? "Not enough ratings from ingame players (requires at least 2).\nCommunity map ratings will be used:" :
				"Community map ratings:")) {
				PrintIngame(clientNum, "No maps have received ratings yet.\n");
			}
		}
	}
	else { // if we didn't match any of the preceding subcommands, assume it's a map or player name
		char mapFileName[MAX_QPATH] = { 0 };
		if (!Q_stricmp(arg1, "here") || !Q_stricmp(arg1, "map") || !Q_stricmp(arg1, "this")) { // view current map
			char live[MAX_QPATH] = { 0 };
			if (G_DBGetLiveMapNameForMapName(Cvar_VariableString("mapname"), live, sizeof(live)) && live[0])
				Q_strncpyz(mapFileName, live, sizeof(mapFileName));
			else
				Q_strncpyz(mapFileName, Cvar_VariableString("mapname"), sizeof(mapFileName));
		}
		else { // view a specific map name
			if (!GetMatchingMap(arg1, mapFileName, sizeof(mapFileName))) {
				tryingUnknownSubcommand = qtrue;
				goto tryFindPlayerFromNoArgs; // blow me fuckhead
			}
		}

		char mapShortName[MAX_QPATH] = { 0 };
		GetShortNameForMapFileName(mapFileName, mapShortName, sizeof(mapShortName));
		TrimMapVersion(mapShortName, mapShortName, sizeof(mapShortName));

		char whitelistedFileName[MAX_QPATH] = { 0 }, liveFileName[MAX_QPATH] = { 0 };
		if (!G_DBTierlistMapIsWhitelisted(mapFileName, qtrue, whitelistedFileName, sizeof(whitelistedFileName), liveFileName, sizeof(liveFileName))) {
			PrintIngame(clientNum, "^5%s^7 (^9%s^7) is not eligible for tierlists.\n", mapShortName, mapFileName);
			return;
		}

		PrintIngame(clientNum, "Stats for ^5%s ^7(^9%s^7):\n", mapShortName, liveFileName);

		int numPlays = G_DBNumTimesPlayedSingleMap(whitelistedFileName);
		PrintIngame(clientNum, "Matches played: %s\n", numPlays ? va("%d", numPlays) : "none");

		mapTier_t currentTier = MAPTIER_INVALID;
		char ingamePlayersStr[256] = { 0 };
		int numIngame = GetAccountIdsStringOfIngamePlayers(ingamePlayersStr, sizeof(ingamePlayersStr));
		if (numIngame >= 2)
			currentTier = G_DBGetTierOfSingleMap(ingamePlayersStr, whitelistedFileName, qtrue);
		mapTier_t communityTier = G_DBGetTierOfSingleMap(NULL, whitelistedFileName, qfalse);

		if (currentTier == MAPTIER_INVALID) { // not enough people ingame have rated this map
			if (numIngame) // only bother showing this "unrated" message if there are actually ingame players
				PrintIngame(clientNum, "Current rating: unrated (requires at least 2 ratings from ingame players)\n");
			PrintIngame(clientNum, "Community rating: %s\n", communityTier == MAPTIER_INVALID ? "unrated" : GetTierStringForTier(communityTier));
		}
		else {
			// enough people have rated this map, so display that rating as well as the community rating
			PrintIngame(clientNum, "Current rating: %s\n", currentTier == MAPTIER_INVALID ? "unrated" : GetTierStringForTier(currentTier));
			PrintIngame(clientNum, "Community rating: %s\n", communityTier == MAPTIER_INVALID ? "unrated" : GetTierStringForTier(communityTier));
		}

		G_DBPrintAllPlayerRatingsForSingleMap(whitelistedFileName, clientNum, NULL);
	}
}

static ctfPlayerTier_t StringToPlayerRating(const char *s) {
	if (!VALIDSTRING(s))
		return PLAYERRATING_UNRATED;

	qboolean high = qfalse, low = qfalse;
	if (stristr(s, "h") || stristr(s, "+"))
		high = qtrue;
	else if (stristr(s, "l") || stristr(s, "-"))
		low = qtrue;

	ctfPlayerTier_t tier;
	if (stristr(s, "s")) {
		tier = PLAYERRATING_MID_S;
		if (high)
			++tier;
		else if (low)
			--tier;
	}
	else if (stristr(s, "a")) {
		tier = PLAYERRATING_MID_A;
		if (high)
			++tier;
		else if (low)
			--tier;
	}
	else if (stristr(s, "b")) {
		tier = PLAYERRATING_MID_B;
		if (high)
			++tier;
		else if (low)
			--tier;
	}
	else if (stristr(s, "c")) {
		tier = PLAYERRATING_MID_C;
		if (high)
			++tier;
		else if (low)
			--tier;
	}
	else if (stristr(s, "d")) {
		tier = PLAYERRATING_MID_D;
		if (high)
			++tier;
		else if (low)
			--tier;
	}
	else if (stristr(s, "f")) {
		tier = PLAYERRATING_MID_F;
		if (high)
			++tier;
		else if (low)
			--tier;
	}
	else if (stristr(s, "g")) {
		tier = PLAYERRATING_MID_G;
		if (high)
			++tier;
		/*else if (low)
			--tier;*/
	}
	else {
		tier = PLAYERRATING_UNRATED;
	}

	return tier;
}

ctfPosition_t CtfPositionFromString(char *s) {
	if (!VALIDSTRING(s))
		return CTFPOSITION_UNKNOWN;

	if (!Q_stricmp(s, "base") || !Q_stricmp(s, "bas") || !Q_stricmp(s, "b"))
		return CTFPOSITION_BASE;

	if (!Q_stricmp(s, "chase") || !Q_stricmp(s, "cha") || !Q_stricmp(s, "c"))
		return CTFPOSITION_CHASE;

	if (!Q_stricmp(s, "offense") || !Q_stricmp(s, "off") || !Q_stricmp(s, "o"))
		return CTFPOSITION_OFFENSE;

	return CTFPOSITION_UNKNOWN;
}

static void PrintRateHelp(int clientNum) {
	OutOfBandPrint(clientNum, "Usage:\n" \
		"^7rating [pos]                     - view a list of your ratings for a certain position\n"
		"^9rating list                      - view lists of your ratings for all positions\n"
		"\n"
		"^7rating set [player] [pos] [tier] - set a player's rating on a certain position\n"
		"^9rating remove [player] [pos]     - delete a player's rating on a certain position\n"
		"^7rating reset [pos]               - delete all ratings for a certain position\n"
		"^9rating reset all                  - delete all ratings\n"
		"\n^7"
		"  - Skill levels should be equivalent between positions; i.e. S tier base == S tier chase == S tier offense. Your ratings may skew higher or lower for some positions -- do not simply assign tiers based on a curve for each position.\n"
		"  - Only rate players on positions you have observed them play at least once. Leave people unrated on positions you haven't seen them play.^7\n"
		"  - Consider re-rating players over time as their skills develop.\n"
		"  - Do not account for rust. Assume players are NOT rusty.\n");
}

static void Cmd_Rating_f(gentity_t *ent) {
	if (!ent || !ent->client)
		return;

	// redirect non-allowed dudes to the tier command since this may be a mistype anyway
	if (!ent->client->account || !(ent->client->account->flags & ACCOUNTFLAG_RATEPLAYERS)) {
		Cmd_Tier_f(ent);
		return;
	}

	int clientNum = ent - g_entities;
	int args = trap_Argc() - 1;

	if (!args) {
		PrintRateHelp(clientNum);
		return;
	}

	char arg1[MAX_STRING_CHARS] = { 0 }, arg2[MAX_STRING_CHARS] = { 0 }, arg3[MAX_STRING_CHARS] = { 0 }, arg4[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, arg1, sizeof(arg1));
	trap_Argv(2, arg2, sizeof(arg2));
	trap_Argv(3, arg3, sizeof(arg3));
	trap_Argv(4, arg4, sizeof(arg3));

	if (!arg1[0]) {
		PrintRateHelp(clientNum);
		return;
	}

	if (!Q_stricmp(arg1, "reset")) {
		ctfPosition_t pos = arg2[0] ? CtfPositionFromString(arg2) : CTFPOSITION_UNKNOWN;
		if (!pos && Q_stricmp(arg2, "all")) {
			OutOfBandPrint(clientNum, "Usage: rating reset [pos]\n");
			return;
		}
		if (G_DBDeleteAllRatingsForPosition(ent->client->account->id, pos))
			OutOfBandPrint(clientNum, va("Deleted all %s%sratings.\n", pos ? NameForPos(pos) : "", pos ? " " : ""));
		else
			OutOfBandPrint(clientNum, va("Error deleting all %s%sratings!\n", pos ? NameForPos(pos) : "", pos ? " " : ""));
	}
	else if (!Q_stricmp(arg1, "resetall")) {
		if (G_DBDeleteAllRatingsForPosition(ent->client->account->id, CTFPOSITION_UNKNOWN))
			OutOfBandPrint(clientNum, "Deleted all ratings.\n");
		else
			OutOfBandPrint(clientNum, "Error deleting all ratings!\n");
	}
	else if (CtfPositionFromString(arg1) == CTFPOSITION_BASE) {
		G_DBListRatingPlayers(ent->client->account->id, clientNum, CTFPOSITION_BASE);
		OutOfBandPrint(clientNum, "\n  - Skill levels should be equivalent between positions; i.e. S tier base == S tier chase == S tier offense. Your ratings may skew higher or lower for some positions -- do not simply assign tiers based on a curve for each position.\n"
		"  - Only rate players on positions you have observed them play at least once. Leave people unrated on positions you haven't seen them play.^7\n"
			"  - Consider re-rating players over time as their skills develop.\n"
			"  - Do not account for rust. Assume players are NOT rusty.\n");
	}
	else if (CtfPositionFromString(arg1) == CTFPOSITION_CHASE) {
		G_DBListRatingPlayers(ent->client->account->id, clientNum, CTFPOSITION_CHASE);
		OutOfBandPrint(clientNum, "\n  - Skill levels should be equivalent between positions; i.e. S tier base == S tier chase == S tier offense. Your ratings may skew higher or lower for some positions -- do not simply assign tiers based on a curve for each position.\n"
			"  - Only rate players on positions you have observed them play at least once. Leave people unrated on positions you haven't seen them play.^7\n"
			"  - Consider re-rating players over time as their skills develop.\n"
			"  - Do not account for rust. Assume players are NOT rusty.\n");
	}
	else if (CtfPositionFromString(arg1) == CTFPOSITION_OFFENSE) {
		G_DBListRatingPlayers(ent->client->account->id, clientNum, CTFPOSITION_OFFENSE);
		OutOfBandPrint(clientNum, "\n  - Skill levels should be equivalent between positions; i.e. S tier base == S tier chase == S tier offense. Your ratings may skew higher or lower for some positions -- do not simply assign tiers based on a curve for each position.\n"
			"  - Only rate players on positions you have observed them play at least once. Leave people unrated on positions you haven't seen them play.^7\n"
			"  - Consider re-rating players over time as their skills develop.\n"
			"  - Do not account for rust. Assume players are NOT rusty.\n");
	}
	else if (!Q_stricmp(arg1, "all") || !Q_stricmp(arg1, "list")) {
		G_DBListRatingPlayers(ent->client->account->id, clientNum, CTFPOSITION_BASE);
		G_DBListRatingPlayers(ent->client->account->id, clientNum, CTFPOSITION_CHASE);
		G_DBListRatingPlayers(ent->client->account->id, clientNum, CTFPOSITION_OFFENSE);
		OutOfBandPrint(clientNum, "\n  - Skill levels should be equivalent between positions; i.e. S tier base == S tier chase == S tier offense. Your ratings may skew higher or lower for some positions -- do not simply assign tiers based on a curve for each position.\n"
			"  - Only rate players on positions you have observed them play at least once. Leave people unrated on positions you haven't seen them play.^7\n"
			"  - Consider re-rating players over time as their skills develop.\n"
			"  - Do not account for rust. Assume players are NOT rusty.\n");
	}
	else if (!Q_stricmp(arg1, "set")) {
		if (!arg2[0] || !arg3[0] || !arg4[0]) {
			OutOfBandPrint(clientNum, "Usage: rating set [player] [pos] [tier]\n");
			return;
		}

		account_t acc = { 0 };
		qboolean found = G_DBGetAccountByName(arg2, &acc);
		if (!found) {
			OutOfBandPrint(clientNum, va("No account found matching '%s^7'. Try checking /rating list.\n", arg2));
			return;
		}

		ctfPosition_t pos = CtfPositionFromString(arg3);
		if (!pos) {
			OutOfBandPrint(clientNum, va("'%s^7' is not a valid position. Positions can be base, chase, or offense.\n", arg3));
			return;
		}

		char *tierStr = ConcatArgs(4);
		ctfPlayerTier_t tier = StringToPlayerRating(tierStr);
		if (!tier) {
			OutOfBandPrint(clientNum, "'%s^7' is not a valid tier.\n", tierStr);
			return;
		}

		if (G_DBSetPlayerRating(ent->client->account->id, acc.id, pos, tier))
			OutOfBandPrint(clientNum, "Set ^5%s^7 %s to %s^7.\n", acc.name, NameForPos(pos), PlayerRatingToString(tier));
		else
			OutOfBandPrint(clientNum, "Error setting ^5%s^7 %s to %s^7.\n", acc.name, NameForPos(pos), PlayerRatingToString(tier));
	}
	else if (!Q_stricmp(arg1, "remove") || !Q_stricmp(arg1, "rem") || !Q_stricmp(arg1, "rm") || !Q_stricmp(arg1, "delete") || !Q_stricmp(arg1, "del")) {
		if (!arg2[0] || !arg3[0]) {
			OutOfBandPrint(clientNum, "Usage: rating remove [player] [pos]\n");
			return;
		}

		account_t acc = { 0 };
		qboolean found = G_DBGetAccountByName(arg2, &acc);
		if (!found) {
			OutOfBandPrint(clientNum, "No account found matching '%s^7'. Try checking /rate players.\n", arg2);
			return;
		}

		ctfPosition_t pos = CtfPositionFromString(arg3);
		if (!pos) {
			OutOfBandPrint(clientNum, "'%s^7' is not a valid position.\n", arg3);
			return;
		}

		if (G_DBRemovePlayerRating(ent->client->account->id, acc.id, pos))
			OutOfBandPrint(clientNum, "Removed rating for %s on %s.\n", acc.name, NameForPos(pos));
		else
			OutOfBandPrint(clientNum, "Error removing rating for ^5%s^7 %s (they may have already been unrated).\n", acc.name, NameForPos(pos));
	}
	else {
		OutOfBandPrint(clientNum, "Unknown subcommand '%s^7'\n", arg1);
		PrintRateHelp(clientNum);
	}
}

// returns qtrue if valid; qfalse if they set something dumb (like a pos set on multiple choice levels, or all three pos on the second choice, etc)
qboolean ValidateAndCopyPositionPreferences(const positionPreferences_t *in, positionPreferences_t *out) {
	if (!in) {
		assert(qfalse);
		return qfalse;
	}

	positionPreferences_t temp = { 0 };
	memcpy(&temp, in, sizeof(positionPreferences_t));

	temp.first &= ALL_CTF_POSITIONS;
	temp.second &= ALL_CTF_POSITIONS;
	temp.third &= ALL_CTF_POSITIONS;
	temp.avoid &= ALL_CTF_POSITIONS;

	if (!temp.first && !temp.second && !temp.third && !temp.avoid) {
		if (out)
			memset(out, 0, sizeof(positionPreferences_t));
		return qtrue;
	}

	qboolean inputValid = qtrue;

	// has all three positions on second/third
	// clear it out
	if (temp.second == ALL_CTF_POSITIONS) {
		temp.second = 0;
		inputValid = qfalse;
	}
	if (temp.third == ALL_CTF_POSITIONS) {
		temp.third = 0;
		inputValid = qfalse;
	}
	if (temp.avoid == ALL_CTF_POSITIONS) {
		temp.avoid = 0;
		inputValid = qfalse;
	}

	// check that no positions repeat
	for (int i = CTFPOSITION_BASE; i <= CTFPOSITION_OFFENSE; i++) {
		int appearances = 0;
		if (temp.first & (1 << i))
			++appearances;
		if (temp.second & (1 << i))
			++appearances;
		if (temp.third & (1 << i))
			++appearances;
		if (temp.avoid & (1 << i))
			++appearances;

		if (appearances > 1) {
			temp.first &= ~(1 << i);
			temp.second &= ~(1 << i);
			temp.third &= ~(1 << i);
			temp.avoid &= ~(1 << i);
			inputValid = qfalse;
		}
	}

	if (!temp.first && temp.second && !temp.third) {
		// only second
		// promote it to first
		temp.first = temp.second;
		temp.second = 0;
		inputValid = qfalse;
	}
	else if (!temp.first && !temp.second && temp.third) {
		// only third
		// promote it to first
		temp.first = temp.third;
		temp.third = 0;
		inputValid = qfalse;
	}
	else if (temp.first && !temp.second && temp.third) {
		// only first and third
		// promote third to second
		temp.second = temp.third;
		temp.third = 0;
		inputValid = qfalse;
	}
	else if (!temp.first && temp.second && temp.third) {
		// only second and third
		// promote them both up by one
		temp.first = temp.second;
		temp.second = temp.third;
		temp.third = 0;
		inputValid = qfalse;
	}

	if (out)
		memcpy(out, &temp, sizeof(positionPreferences_t));

	return inputValid;
}

static void PrintPositionPreferences(gentity_t *ent) {
	assert(ent && ent->client && ent->client->account);

	positionPreferences_t *pref = &ent->client->account->expressedPref;

	if (!pref->first && !pref->second && !pref->third && !pref->avoid) {
		PrintIngame(ent - g_entities, "You have no position preferences.\n");
		return;
	}

	char buf[MAX_STRING_CHARS] = "Your position preferences: \n^3 First choice:^7 ";

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
		Q_strcat(buf, sizeof(buf), "\n^9Second choice:^7 ");
		qboolean gotOne = qfalse;
		for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
			if (pref->second & (1 << pos)) {
				Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
				gotOne = qtrue;
			}
		}
	}

	{
		Q_strcat(buf, sizeof(buf), "\n^8 Third choice:^7 ");
		qboolean gotOne = qfalse;
		for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
			if (pref->third & (1 << pos)) {
				Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
				gotOne = qtrue;
			}
		}
	}

	{
		Q_strcat(buf, sizeof(buf), "\n^1        Avoid:^7 ");
		qboolean gotOne = qfalse;
		for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
			if (pref->avoid & (1 << pos)) {
				Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
				gotOne = qtrue;
			}
		}
	}

	{
		Q_strcat(buf, sizeof(buf), "\n^9      Unrated:^7 ");
		qboolean gotOne = qfalse;
		for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
			if (pref->first & (1 << pos) || pref->second & (1 << pos) || pref->third & (1 << pos) || pref->avoid & (1 << pos))
				continue;
			Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
			gotOne = qtrue;
		}
	}

	if (ValidateAndCopyPositionPreferences(pref, &ent->client->account->validPref))
		Q_strcat(buf, sizeof(buf), "\n");
	else
		Q_strcat(buf, sizeof(buf), "\n^3Warning:^7 your position preferences are currently invalid. Check that they make sense (e.g. not having a position in more than one tier, not *only* having second choice preferences, etc.)\n");

	Q_strcat(buf, sizeof(buf), "\n^5Note: You do not need to set a preference for every position.^7 Leave a position unrated if you have no opinion on it (even rating a position as third choice may increase your chance of playing it, compared with not rating it at all).\n");

	PrintIngame(ent - g_entities, buf);
}

static void PrintPosHelp(gentity_t *ent) {
	assert(ent);
	PrintIngame(ent - g_entities,
		"Usage:  ^5pos [1/2/3/avoid/clear] <position>^7     (order doesn't matter)\n"
		"Examples:\n"
		"  ^7pos 1 base      - base is your first choice\n"
		"  ^9pos 1 chase     - chase is ^2also^9 your first choice\n"
		"  ^7pos 2 offense   - offense is your second choice\n"
		"  ^9pos 3 chase     - chase is your third choice\n"
		"  ^7pos avoid chase - you would like to avoid chase\n"
		"  ^9pos clear base  - clear all preferences for base\n"
		"  ^7pos clear       - clear all preferences\n"
		"  ^9pos [name/#]    - view someone else's preferences\n"
		"^5Note: You do not need to set a preference for every position.^7 Leave a position unrated if you have no opinion on it (even rating a position as third choice may increase your chance of playing it, compared with not rating it at all).\n"
	);
}

static void Cmd_Pos_f(gentity_t *ent) {
	if (!ent || !ent->client || ent->client->pers.connected != CON_CONNECTED)
		return;

	if (!ent->client->account) {
		PrintIngame(ent - g_entities, "You must have an account in order to use this feature. Please contact an admin for help setting up an account.\n");
		return;
	}

	if (trap_Argc() < 2) {
		PrintPositionPreferences(ent);
		PrintIngame(ent - g_entities, "Enter ^5pos help^7 for instructions on setting up position preferences.\n");
		return;
	}

	char arg1[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, arg1, sizeof(arg1));
	char arg2[MAX_STRING_CHARS] = { 0 };
	trap_Argv(2, arg2, sizeof(arg2));

	positionPreferences_t *pref = &ent->client->account->expressedPref;

	if (trap_Argc() < 3 || !arg1[0]) {
		if (arg1[0] && (stristr(arg1, "cl") || stristr(arg1, "del") || stristr(arg1, "rm") || stristr(arg1, "rem"))) {
			PrintIngame(ent - g_entities, "All preferences cleared.\n");
			pref->first = pref->second = pref->third = pref->avoid = 0;
			ValidateAndCopyPositionPreferences(pref, &ent->client->account->validPref);
			G_DBSetAccountProperties(ent->client->account);
		}
		else if (arg1[0] && stristr(arg1, "help")) {
			PrintPosHelp(ent);
		}
		else if (arg1[0] && stristr(arg1, "list")) {
			PrintPositionPreferences(ent);
		}
		else if (arg1[0] && !arg2[0]) {
			int accountId = -1;
			char name[64] = { 0 };
			if (Q_isanumber(arg1)) {
				int num = atoi(arg1);
				if (num >= 0 && num < MAX_CLIENTS && g_entities[num].inuse && g_entities[num].client && g_entities[num].client->pers.connected != CON_DISCONNECTED) {
					if (g_entities[num].client->account) {
						accountId = g_entities[num].client->account->id;
						Q_strncpyz(name, va("%s^7 (%s^7)", g_entities[num].client->pers.netname, g_entities[num].client->account->name), sizeof(name));
					}
					else {
						PrintIngame(ent - g_entities, "%s^7 does not have any pos preferences.\n", g_entities[num].client->pers.netname);
						PrintIngame(ent - g_entities, "Enter ^5pos help^7 for instructions on setting up position preferences.\n");
						return;
					}
				}
				else {
					PrintIngame(ent - g_entities, "Unable to find a player matching '%s^7'.\n", arg1);
					PrintIngame(ent - g_entities, "Enter ^5pos help^7 for instructions on setting up position preferences.\n");
					return;
				}
			}
			else {
				char lowercase[MAX_QPATH] = { 0 };
				Q_strncpyz(lowercase, arg1, sizeof(lowercase));
				Q_strlwr(lowercase);
				account_t account = { 0 };
				if (G_DBGetAccountByName(lowercase, &account)) {
					accountId = account.id;
					Q_strncpyz(name, account.name, sizeof(name));
				}
				else {
					gentity_t *found = G_FindClient(arg1);
					if (found && found->client) {
						if (found->client->account) {
							accountId = found->client->account->id;
							Q_strncpyz(name, va("%s^7 (%s^7)", found->client->pers.netname, found->client->account->name), sizeof(name));
						}
						else {
							PrintIngame(ent - g_entities, "%s^7 does not have any pos preferences.\n", found->client->pers.netname);
							PrintIngame(ent - g_entities, "Enter ^5pos help^7 for instructions on setting up position preferences.\n");
							return;
						}
					}
					else {
						PrintIngame(ent - g_entities, "Unable to find a player matching '%s^7'.\n", arg1);
						PrintIngame(ent - g_entities, "Enter ^5pos help^7 for instructions on setting up position preferences.\n");
						return;
					}
				}
			}

			if (accountId == -1)
				return;

			accountReference_t acc = G_GetAccountByID(accountId, qfalse);
			if (!acc.ptr)
				return;

			if (ent->client->account && ent->client->account->id == accountId) { // entered their own name?
				PrintPositionPreferences(ent);
				PrintIngame(ent - g_entities, "Enter ^5pos help^7 for instructions on setting up position preferences.\n");
				return;
			}

			pref = &acc.ptr->expressedPref;

			if (!name[0])
				Q_strncpyz(name, acc.ptr->name, sizeof(name));

			char buf[MAX_STRING_CHARS] = { 0 };
			Com_sprintf(buf, sizeof(buf), "%s^7's position preferences: \n^3 First choice:^7 ", name);

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
				Q_strcat(buf, sizeof(buf), "\n^9Second choice:^7 ");
				qboolean gotOne = qfalse;
				for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
					if (pref->second & (1 << pos)) {
						Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
						gotOne = qtrue;
					}
				}
			}

			{
				Q_strcat(buf, sizeof(buf), "\n^8 Third choice:^7 ");
				qboolean gotOne = qfalse;
				for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
					if (pref->third & (1 << pos)) {
						Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
						gotOne = qtrue;
					}
				}
			}

			{
				Q_strcat(buf, sizeof(buf), "\n^1        Avoid:^7 ");
				qboolean gotOne = qfalse;
				for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
					if (pref->avoid & (1 << pos)) {
						Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
						gotOne = qtrue;
					}
				}
			}

			{
				Q_strcat(buf, sizeof(buf), "\n^9      Unrated:^7 ");
				qboolean gotOne = qfalse;
				for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
					if (pref->first & (1 << pos) || pref->second & (1 << pos) || pref->third & (1 << pos) || pref->avoid & (1 << pos))
						continue;
					Q_strcat(buf, sizeof(buf), va("%s%s", gotOne ? ", " : "", NameForPos(pos)));
					gotOne = qtrue;
				}
			}

			Q_strcat(buf, sizeof(buf), "\n");
			PrintIngame(ent - g_entities, buf);
			PrintIngame(ent - g_entities, "Enter ^5pos help^7 for instructions on setting up position preferences.\n");
		}
		else {
			PrintPositionPreferences(ent);
			PrintIngame(ent - g_entities, "Enter ^5pos help^7 for instructions on setting up position preferences.\n");
		}

		return;
	}

	if (!arg2[0]) {
		PrintPositionPreferences(ent);
		PrintPosHelp(ent);
		return;
	}

	// allow typing the arguments in any order
	char *intentionStr = NULL;
	int pos = CtfPositionFromString(arg1);
	if (pos) {
		intentionStr = arg2;
	}
	else {
		pos = CtfPositionFromString(arg2);
		if (pos) {
			intentionStr = arg1;
		}
		else {
			PrintIngame(ent - g_entities, "You must enter a valid position.\n");
			PrintPosHelp(ent);
			return;
		}
	}
	char posStr[16] = { 0 };
	Com_sprintf(posStr, sizeof(posStr), "^5%s^7", NameForPos(pos));

	assert(intentionStr);
	enum {
		INTENTION_UNKNOWN = 0,
		INTENTION_FIRSTCHOICE,
		INTENTION_SECONDCHOICE,
		INTENTION_THIRDCHOICE,
		INTENTION_AVOID,
		INTENTION_CLEAR
	} intention = INTENTION_UNKNOWN;
	if (stristr(intentionStr, "1") || stristr(intentionStr, "pref") || stristr(intentionStr, "top"))
		intention = INTENTION_FIRSTCHOICE;
	else if (stristr(intentionStr, "2") || stristr(intentionStr, "sec"))
		intention = INTENTION_SECONDCHOICE;
	else if (stristr(intentionStr, "3") || stristr(intentionStr, "thi"))
		intention = INTENTION_THIRDCHOICE;
	else if (stristr(intentionStr, "av"))
		intention = INTENTION_AVOID;
	else if (stristr(intentionStr, "cl") || stristr(intentionStr, "rem") || stristr(intentionStr, "rm") || stristr(intentionStr, "del") || stristr(intentionStr, "res") || stristr(intentionStr, "0"))
		intention = INTENTION_CLEAR;
	if (!intention) {
		PrintIngame(ent - g_entities, "You must enter what you want to do, e.g. '1', '2', '3', 'avoid', or 'clear'\n");
		return;
	}

	if (intention == INTENTION_FIRSTCHOICE) {
		if (pref->first & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s is already first choice.\n", posStr);
			return;
		}

		qboolean printed = qfalse;

		if (pref->second & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from second choice to first choice.\n", posStr);
			pref->second &= ~(1 << pos);
			printed = qtrue;
		}

		if (pref->third & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from third choice to first choice.\n", posStr);
			pref->third &= ~(1 << pos);
			printed = qtrue;
		}

		if (pref->avoid & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from avoided to first choice.\n", posStr);
			pref->avoid &= ~(1 << pos);
			printed = qtrue;
		}

		pref->first |= (1 << pos);

		if (!printed)
			PrintIngame(ent - g_entities, "Set %s to first choice.\n", posStr);
	}
	else if (intention == INTENTION_SECONDCHOICE) {
		if (pref->second & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s is already second choice.\n", posStr);
			return;
		}

		qboolean printed = qfalse;

		if (pref->first & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from first choice to second choice.\n", posStr);
			pref->first &= ~(1 << pos);
			printed = qtrue;
		}

		if (pref->third & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from third choice to second choice.\n", posStr);
			pref->third &= ~(1 << pos);
			printed = qtrue;
		}

		if (pref->avoid & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from avoided to second choice.\n", posStr);
			pref->avoid &= ~(1 << pos);
			printed = qtrue;
		}

		pref->second |= (1 << pos);

		if (!printed)
			PrintIngame(ent - g_entities, "Set %s to second choice.\n", posStr);
	}
	else if (intention == INTENTION_THIRDCHOICE) {
		if (pref->third & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s is already third choice.\n", posStr);
			return;
		}

		qboolean printed = qfalse;

		if (pref->first & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from first choice to third choice.\n", posStr);
			pref->first &= ~(1 << pos);
			printed = qtrue;
		}

		if (pref->second & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from second choice to third choice.\n", posStr);
			pref->second &= ~(1 << pos);
			printed = qtrue;
		}

		if (pref->avoid & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from avoided to third choice.\n", posStr);
			pref->avoid &= ~(1 << pos);
			printed = qtrue;
		}

		pref->third |= (1 << pos);

		if (!printed)
			PrintIngame(ent - g_entities, "Set %s to third choice.\n", posStr);
	}
	else if (intention == INTENTION_AVOID) {
		if (pref->avoid & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s is already avoided.\n", posStr);
			return;
		}

		qboolean printed = qfalse;

		if (pref->first & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from first choice to avoided.\n", posStr);
			pref->first &= ~(1 << pos);
			printed = qtrue;
		}

		if (pref->second & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from second choice to avoided.\n", posStr);
			pref->second &= ~(1 << pos);
			printed = qtrue;
		}

		if (pref->third & (1 << pos)) {
			PrintIngame(ent - g_entities, "%s changed from third choice to avoided.\n", posStr);
			pref->third &= ~(1 << pos);
			printed = qtrue;
		}

		pref->avoid |= (1 << pos);

		if (!printed)
			PrintIngame(ent - g_entities, "Set %s to avoided.\n", posStr);
	}
	else /*if (intention == INTENTION_CLEAR)*/ {
		PrintIngame(ent - g_entities, "Cleared all preferences for %s.\n", posStr);
		pref->first &= ~(1 << pos);
		pref->second &= ~(1 << pos);
		pref->third &= ~(1 << pos);
		pref->avoid &= ~(1 << pos);
	}

	ValidateAndCopyPositionPreferences(pref, &ent->client->account->validPref);
	G_DBSetAccountProperties(ent->client->account);
}

char *ParseItemName(const char *input) {
	if (!VALIDSTRING(input)) {
		return NULL;
	}
	else if (stristr(input, "pow") || stristr(input, "cell")) {
		return "ammo_powercell";
	}
	else if (stristr(input, "metal") || stristr(input, "bolt")) {
		return "ammo_metallic_bolts";
	}
	else if (stristr(input, "amm")) {
		if (stristr(input, "blast") || stristr(input, "e11"))
			return "ammo_blaster";
		else if (stristr(input, "disr") || stristr(input, "snip") || stristr(input, "tenl") || stristr(input, "bow") || stristr(input, "cast") || stristr(input, "demp"))
			return "ammo_powercell";
		else if (stristr(input, "rep") || stristr(input, "gol") || stristr(input, "conc"))
			return "ammo_metallic_bolts";
		else if (stristr(input, "roc"))
			return "ammo_rockets";
		else if (stristr(input, "therm"))
			return "ammo_thermal";
		else if (stristr(input, "detp"))
			return "ammo_detpack";
		else if (stristr(input, "min"))
			return "ammo_tripmine";
	}
	else if (stristr(input, "blast") || stristr(input, "e11")) {
		return "weapon_blaster";
	}
	else if (stristr(input, "disr") || stristr(input, "snip") || stristr(input, "tenl")) {
		return "weapon_disruptor";
	}
	else if (stristr(input, "bow") || stristr(input, "cast")) {
		return "weapon_bowcaster";
	}
	else if (stristr(input, "rep")) {
		return "weapon_repeater";
	}
	else if (stristr(input, "gol") || stristr(input, "flec")) {
		return "weapon_flechette";
	}
	else if (stristr(input, "roc")) {
		return "weapon_rocket_launcher";
	}
	else if (stristr(input, "conc")) {
		return "weapon_concussion_rifle";
	}
	else if (stristr(input, "ther")) {
		return "weapon_thermal";
	}
	else if (stristr(input, "min")) {
		return "weapon_trip_mine";
	}
	else if (stristr(input, "detp") || stristr(input, "det_p")) {
		return "weapon_det_pack";
	}
	else if (stristr(input, "armor") || stristr(input, "shield") || stristr(input, "sheild")) {
		if (stristr(input, "large") || stristr(input, "big") || stristr(input, "lrg"))
			return "item_shield_lrg_instant";
		else
			return "item_shield_sm_instant";
	}
	else if (stristr(input, "med") || stristr(input, "health")) {
		return "item_medpak_instant";
	}
	else if (!Q_stricmpn(input, "ys", 2) || stristr(input, "mari") || stristr(input, "miri")) {
		return "item_ysalimari";
	}
	return NULL;
}

char *ValidateUserTypedItem(const char *input, const char *whitelist) {
	// parse what the user typed in
	char *parsedInput = ParseItemName(input);
	if (!parsedInput)
		return NULL;

	// if the whitelist is empty, return the result from above
	if (!VALIDSTRING(whitelist) || *whitelist == '0')
		return parsedInput;

	// parse each item in the whitelist and see if the one the user typed is whitelisted
	char *whitelistCopy = strdup(whitelist);
	char *token = strtok(whitelistCopy, " ");
	while (token) {
		char *parsedWhitelistItem = ParseItemName(token);
		if (parsedWhitelistItem && !strcmp(parsedInput, parsedWhitelistItem)) {
			free(whitelistCopy);
			return parsedInput;
		}
		token = strtok(NULL, " ");
	}
	free(whitelistCopy);

	return NULL; // not whitelisted
}

const char *TableCallback_ItemId(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	changedItem_t *item = rowContext;
	return va("%d", item->id);
}

const char *TableCallback_ItemName(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	changedItem_t *item = rowContext;
	return va("%s", item->itemType);
}

const char *TableCallback_ItemOwner(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	changedItem_t *item = rowContext;
	if (item->addedItemCreatorId == ACCOUNT_ID_UNLINKED)
		return NULL;

	account_t account = { 0 };
	G_DBGetAccountByID(item->addedItemCreatorId, &account);
	if (account.id == ACCOUNT_ID_UNLINKED)
		return NULL;

	return va("%s", account.name);
}

const char *TableCallback_ItemDeleted(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	changedItem_t *item = rowContext;
	if (!item->baseItemDeleted)
		return "No";

	account_t account = { 0 };
	G_DBGetAccountByID(item->baseItemDeleterId, &account);
	if (account.id == ACCOUNT_ID_UNLINKED)
		return "^1by <unknown>";

	return va("^1by %s", account.name);
}

const char *TableCallback_ItemSaved(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	changedItem_t *item = rowContext;
	return item->addedItemSaved ? "Yes" : "^1No";
}

const char *TableCallback_ItemCoordinateSuspended(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	changedItem_t *item = rowContext;
	return item->isSuspended ? "Air" : "Ground";
}

const char *TableCallback_ItemCoordinate(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	changedItem_t *item = rowContext;

	int axis = (int)columnContext;
	assert(axis >= 0 && axis <= 2);

	return va("%d", (int)roundf(item->origin[axis]));
}

const char *TableCallback_ItemDistanceToEntity(void *rowContext, void *columnContext) {
	if (!rowContext || !columnContext) {
		assert(qfalse);
		return NULL;
	}
	changedItem_t *item = rowContext;

	gentity_t *otherEnt = (gentity_t *)columnContext;
	return va("%d", (int)roundf(Distance(item->origin, otherEnt->r.currentOrigin)));
}

extern qboolean G_CallSpawn(gentity_t *ent);
extern qboolean canSpawnItemStartsolid;
static void Cmd_Item_f(gentity_t *player) {
	assert(player);

	if (!player->client || !player->client->account || !(player->client->account->flags & ACCOUNTFLAG_ITEMLORD)) {
		PrintIngame(player - g_entities, "You do not have permission to use this command.\n");
		return;
	}

	if (IsLivePug(0) && !level.someoneWasAFK) {
		PrintIngame(player - g_entities, "You cannot use this command during live pugs.\n");
		return;
	}

	if (InstagibEnabled()) {
		PrintIngame(player - g_entities, "You cannot use this command during instagib.\n");
		return;
	}

	const int args = trap_Argc();
	char arg1[MAX_STRING_CHARS];

	if (args < 2) {
		PrintIngame(player - g_entities,
			"Usage:\n" \
			"^7item list                                                   - list added items\n" \
			"^9item list base                                              - list base items, including deleted ones\n" \
			"^7item add <item name> [optional air/ground] [optional x y z] - add item\n" \
			"^9item save <added item id | all>                             - save added item(s)\n" \
			"^7item delete <item id | allbase | alladded | all>            - delete item(s)\n" \
			"^9item undelete <base item id | all>                          - undelete base item(s)^7\n"
		);
		return;
	}

	trap_Argv(1, arg1, sizeof(arg1));

	if (!Q_stricmp(arg1, "list") || !Q_stricmp(arg1, "view")) {
		qboolean listBaseItems = qfalse;
		if (args > 2) {
			char baseArgBuf[MAX_STRING_CHARS];
			trap_Argv(2, baseArgBuf, sizeof(baseArgBuf));
			if (!Q_stricmpn(baseArgBuf, "bas", 3))
				listBaseItems = qtrue;
		}

		if (listBaseItems) {
			if (!g_deleteBaseItems.integer) {
				PrintIngame(player - g_entities, "Deleted base items system is disabled.\n");
				return;
			}

			if (level.baseItemsList.size <= 0) {
				PrintIngame(player - g_entities, "There are no base items.\nTo view added items, enter ^5item list^7.\n");
				return;
			}
			else {
				PrintIngame(player - g_entities, "There are %d base items on %s:^7\n", level.baseItemsList.size, level.mapname);

				Table *t = Table_Initialize(qtrue);

				iterator_t iter;
				ListIterate(&level.baseItemsList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					changedItem_t *item = (changedItem_t *)IteratorNext(&iter);
					Table_DefineRow(t, item);
				}

				Table_DefineColumn(t, "Id #", TableCallback_ItemId, NULL, qfalse, -1, 64);
				Table_DefineColumn(t, "Name", TableCallback_ItemName, NULL, qtrue, -1, 64);
				Table_DefineColumn(t, "Deleted", TableCallback_ItemDeleted, NULL, qtrue, -1, 64);
				Table_DefineColumn(t, "X", TableCallback_ItemCoordinate, (void *)0, qtrue, -1, 64);
				Table_DefineColumn(t, "Y", TableCallback_ItemCoordinate, (void *)1, qtrue, -1, 64);
				Table_DefineColumn(t, "Z", TableCallback_ItemCoordinate, (void *)2, qtrue, -1, 64);

				gentity_t temp;
				VectorCopy(vec3_origin, temp.r.currentOrigin);
				gentity_t *redFsEnt = G_ClosestEntity(&temp, isRedFlagstand);
				if (redFsEnt)
					Table_DefineColumn(t, "Dist/^1red^7 FS", TableCallback_ItemDistanceToEntity, redFsEnt, qtrue, -1, 64);

				gentity_t *blueFsEnt = G_ClosestEntity(&temp, isBlueFlagstand);
				if (blueFsEnt)
					Table_DefineColumn(t, "Dist/^4blue^7 FS", TableCallback_ItemDistanceToEntity, blueFsEnt, qtrue, -1, 64);

				Table_DefineColumn(t, "Air/ground", TableCallback_ItemCoordinateSuspended, NULL, qtrue, -1, 64);

				const size_t bufSize = 16384;
				char *buf = malloc(bufSize);
				Table_WriteToBuffer(t, buf, bufSize, qtrue, -1);
				Table_Destroy(t);

				PrintIngame(player - g_entities, buf);
				free(buf);

				PrintIngame(player - g_entities, "To view added items, enter ^5item list^7.\n");
			}
		}
		else {
			if (!g_addItems.integer) {
				PrintIngame(player - g_entities, "Added items system is disabled.\n");
				return;
			}

			if (level.addedItemsList.size <= 0) {
				PrintIngame(player - g_entities, "There are no added items.\nTo view base items, enter ^5item list base^7.\n");
				return;
			}
			else {
				PrintIngame(player - g_entities, "There are %d added items on %s:^7\n", level.addedItemsList.size, level.mapname);

				Table *t = Table_Initialize(qtrue);

				iterator_t iter;
				ListIterate(&level.addedItemsList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					changedItem_t *item = (changedItem_t *)IteratorNext(&iter);
					Table_DefineRow(t, item);
				}

				Table_DefineColumn(t, "Id #", TableCallback_ItemId, NULL, qfalse, -1, 64);
				Table_DefineColumn(t, "Name", TableCallback_ItemName, NULL, qtrue, -1, 64);
				Table_DefineColumn(t, "Creator", TableCallback_ItemOwner, NULL, qtrue, -1, 64);
				Table_DefineColumn(t, "Saved", TableCallback_ItemSaved, NULL, qtrue, -1, 64);
				Table_DefineColumn(t, "X", TableCallback_ItemCoordinate, (void *)0, qtrue, -1, 64);
				Table_DefineColumn(t, "Y", TableCallback_ItemCoordinate, (void *)1, qtrue, -1, 64);
				Table_DefineColumn(t, "Z", TableCallback_ItemCoordinate, (void *)2, qtrue, -1, 64);

				gentity_t temp;
				VectorCopy(vec3_origin, temp.r.currentOrigin);
				gentity_t *redFsEnt = G_ClosestEntity(&temp, isRedFlagstand);
				if (redFsEnt)
					Table_DefineColumn(t, "Dist/^1red^7 FS", TableCallback_ItemDistanceToEntity, redFsEnt, qtrue, -1, 64);

				gentity_t *blueFsEnt = G_ClosestEntity(&temp, isBlueFlagstand);
				if (blueFsEnt)
					Table_DefineColumn(t, "Dist/^4blue^7 FS", TableCallback_ItemDistanceToEntity, blueFsEnt, qtrue, -1, 64);

				Table_DefineColumn(t, "Air/ground", TableCallback_ItemCoordinateSuspended, NULL, qtrue, -1, 64);

				const size_t bufSize = 16384;
				char *buf = malloc(bufSize);
				Table_WriteToBuffer(t, buf, bufSize, qtrue, -1);
				Table_Destroy(t);

				PrintIngame(player - g_entities, buf);
				free(buf);

				PrintIngame(player - g_entities, "To view base items, enter ^5item list base^7.\n");
			}
		}
	}
	else if (!Q_stricmp(arg1, "save")) {
		if (!g_addItems.integer) {
			PrintIngame(player - g_entities, "Saving added items is disabled.\n");
			return;
		}

		if (args < 3) {
			PrintIngame(player - g_entities, "Usage: item save <added item id>\n");
			return;
		}

		char itemIdBuf[MAX_STRING_CHARS];
		trap_Argv(2, itemIdBuf, sizeof(itemIdBuf));

		if (!Q_stricmpn(itemIdBuf, "all", 3)) {
			int numNotSaved = 0, savedCount = 0, errorCount = 0;

			if (level.addedItemsList.size <= 0) {
				PrintIngame(player - g_entities, "There are no added items. Add some items first.\n");
				return;
			}

			iterator_t iter;
			ListIterate(&level.addedItemsList, &iter, qfalse);

			while (IteratorHasNext(&iter)) {
				changedItem_t *item = (changedItem_t *)IteratorNext(&iter);
				if (item->addedItemSaved)
					continue;

				++numNotSaved;

				if (DB_SaveAddedItem(item)) {
					item->addedItemSaved = qtrue;
					++savedCount;
				}
				else {
					PrintIngame(player - g_entities, "Error saving added item %d (%s) to database!\n", item->id, item->itemType);
					++errorCount;
				}
			}

			if (numNotSaved)
				PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s saved %d of %d unsaved added items to the database. Encountered %d errors.\n", player->client->account->name, savedCount, numNotSaved, errorCount));
			else
				PrintIngame(player - g_entities, "All added items are already saved.\n");
		}
		else {
			if (!Q_isanumber(itemIdBuf)) {
				PrintIngame(player - g_entities, "Usage: item save <added item id>\n");
				return;
			}
			int enteredId = atoi(itemIdBuf);

			if (level.addedItemsList.size <= 0) {
				PrintIngame(player - g_entities, "There are no added items. Add some items first.\n");
				return;
			}

			iterator_t iter;
			ListIterate(&level.addedItemsList, &iter, qfalse);
			changedItem_t *found = NULL;
			while (IteratorHasNext(&iter)) {
				changedItem_t *item = (changedItem_t *)IteratorNext(&iter);
				if (item->id == enteredId) {
					found = item;
					break;
				}
			}

			if (!found) {
				PrintIngame(player - g_entities, "No added item found with id %d\n", enteredId);
				return;
			}

			if (found->addedItemSaved) {
				PrintIngame(player - g_entities, "Added item %d (%s at %d, %d, %d) is already saved.\n", found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2]));
				return;
			}

			if (DB_SaveAddedItem(found)) {
				PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s saved added item %d (%s at %d, %d, %d) to database.\n", player->client->account->name, found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2])));
				found->addedItemSaved = qtrue;
			}
			else {
				PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s encountered an error saving added item %d (%s at %d, %d, %d) to database!\n", player->client->account->name, found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2])));
			}
		}
	}
	else if (!Q_stricmp(arg1, "new") || !Q_stricmp(arg1, "create") || !Q_stricmp(arg1, "make") || !Q_stricmp(arg1, "spawn") || !Q_stricmp(arg1, "add")) {
		if (!g_addItems.integer) {
			PrintIngame(player - g_entities, "Adding items is disabled.\n");
			return;
		}

		if (args == 2 || args == 5 || args == 6) {
			PrintIngame(player - g_entities, "Usage: item new <item name> [optional air/ground] [optional x y z, otherwise uses your location]\n");
			return;
		}

		if (level.addedItemsList.size >= ADDEDITEM_LIMIT_PER_MAP) {
			PrintIngame(player - g_entities, "There are too many added items (limit: %d).\n", ADDEDITEM_LIMIT_PER_MAP);
			return;
		}

		char itemNameInput[MAX_STRING_CHARS];
		trap_Argv(2, itemNameInput, sizeof(itemNameInput));
		const char *itemName = ValidateUserTypedItem(itemNameInput, g_addItemsWhitelist.string);
		if (!VALIDSTRING(itemName)) {
			if (g_addItemsWhitelist.string[0] && g_addItemsWhitelist.string[0] != '0')
				PrintIngame(player - g_entities, "'%s' is an invalid or impermissible item name. Permissible items: %s\n", itemNameInput, g_addItemsWhitelist.string);
			else
				PrintIngame(player - g_entities, "'%s' is an invalid item name.\n", itemNameInput);
			return;
		}

		qboolean forceGround = qfalse, forceSuspended = qfalse;
		if (args >= 4) {
			char airGroundBuf[64];
			trap_Argv(3, airGroundBuf, sizeof(airGroundBuf));
			if (strchr(airGroundBuf, 'a'))
				forceGround = qtrue;
			else if (strchr(airGroundBuf, 'g'))
				forceSuspended = qtrue;
		}

		vec3_t origin;
		if (args >= 7) {
			char xCoordBuf[64], yCoordBuf[64], zCoordBuf[64];
			trap_Argv(4, xCoordBuf, sizeof(xCoordBuf));
			trap_Argv(5, yCoordBuf, sizeof(yCoordBuf));
			trap_Argv(6, zCoordBuf, sizeof(zCoordBuf));

			if (!Q_isanumber(xCoordBuf) || !Q_isanumber(yCoordBuf) || !Q_isanumber(zCoordBuf)) {
				PrintIngame(player - g_entities, "Coordinates must be numbers.\n");
				return;
			}

			origin[0] = atof(xCoordBuf);
			origin[1] = atof(yCoordBuf);
			origin[2] = atof(zCoordBuf);
		}
		else {
			if (player->client->ps.pm_type == PM_SPECTATOR)
				VectorCopy(player->client->ps.origin, origin);
			else
				VectorCopy(player->r.currentOrigin, origin);
		}

		gentity_t *itemEnt = G_Spawn();
		VectorCopy(origin, itemEnt->s.origin);
		VectorCopy(origin, itemEnt->s.pos.trBase);
		VectorCopy(origin, itemEnt->r.currentOrigin);
		itemEnt->classname = (char *)itemName;

		if (!G_CallSpawn(itemEnt)) {
			G_FreeEntity(itemEnt);
			PrintIngame(player - g_entities, "Error spawning added item!\n");
			return;
		}

		qboolean suspended;
		if (forceSuspended) {
			suspended = qtrue;
		}
		else if (forceGround) {
			suspended = qfalse;
		}
		else { // try to determine automatically whether it should be suspended
			if (args < 7 && player->client->ps.groundEntityNum == ENTITYNUM_NONE)
				suspended = qtrue;
			else if (args < 7 && player->client->ps.pm_type == PM_SPECTATOR)
				suspended = qtrue;
			else
				suspended = qfalse;
		}

		if (suspended)
			itemEnt->spawnflags |= 1; // ITMSF_SUSPEND from g_items.c; allow it to be in the air

		canSpawnItemStartsolid = qtrue;
		FinishSpawningItem(itemEnt);
		canSpawnItemStartsolid = qfalse;

		itemEnt->s.eFlags |= EF_NODRAW;
		itemEnt->r.svFlags |= SVF_NOCLIENT;
		itemEnt->r.contents = 0;
		itemEnt->nextthink = level.time + 1000;
		itemEnt->think = RespawnItem;

		int id = MAX_GENTITIES;
		if (level.addedItemsList.size) {
			iterator_t iter;
			ListIterate(&level.addedItemsList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				changedItem_t *thisItem = IteratorNext(&iter);
				if (thisItem->id > id)
					id = thisItem->id;
			}
			++id;
		}

		changedItem_t *add = ListAdd(&level.addedItemsList, sizeof(changedItem_t));
		add->ent = itemEnt;
		add->id = id;
		add->isSuspended = suspended;
		VectorCopy(origin, add->origin);
		add->addedItemCreatorId = player->client->account->id;
		Q_strncpyz(add->itemType, itemName, sizeof(add->itemType));
		itemEnt->classname = add->itemType;
		static qboolean printedWarning = qfalse;
		if (printedWarning) {
			PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s added %s with id %d at %d, %d, %d.\n", player->client->account->name, add->itemType, id, (int)roundf(add->origin[0]), (int)roundf(add->origin[1]), (int)roundf(add->origin[2])));
		}
		else {
			PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s added %s with id %d at %d, %d, %d. Items are not saved after map change/restart unless you enter ^5item save^7.\n", player->client->account->name, add->itemType, id, (int)roundf(add->origin[0]), (int)roundf(add->origin[1]), (int)roundf(add->origin[2])));
			printedWarning = qtrue;
		}
	}
	else if (!Q_stricmpn(arg1, "del", 3) || !Q_stricmpn(arg1, "rem", 3) || !Q_stricmpn(arg1, "rm", 2)) {
		if (args < 3) {
			PrintIngame(player - g_entities, "Usage: item delete <item id>\n");
			return;
		}

		char itemIdBuf[MAX_STRING_CHARS];
		trap_Argv(2, itemIdBuf, sizeof(itemIdBuf));

		qboolean deleteAllBase = !Q_stricmpn(itemIdBuf, "allbase", 7);
		qboolean deleteAllAdded = !Q_stricmpn(itemIdBuf, "alladded", 8);
		qboolean deleteAll = !Q_stricmpn(itemIdBuf, "all", 3) && !deleteAllAdded && !deleteAllBase;
		if (deleteAllBase || deleteAllAdded || deleteAll) {
			if (deleteAllBase || deleteAll) {
				if (!g_deleteBaseItems.integer) {
					PrintIngame(player - g_entities, "Deleting base items is disabled.\n");
				}
				else {
					if (level.baseItemsList.size <= 0) {
						PrintIngame(player - g_entities, "No base items available to delete.\n");
					}
					else {
						int nonDeletedBaseItemCount = 0, deletedCount = 0, errorCount = 0;
						iterator_t iter;
						ListIterate(&level.baseItemsList, &iter, qfalse);

						while (IteratorHasNext(&iter)) {
							changedItem_t *item = (changedItem_t *)IteratorNext(&iter);

							if (item->baseItemDeleted)
								continue;

							++nonDeletedBaseItemCount;

							if (g_deleteBaseItemsWhitelist.string[0] && g_deleteBaseItemsWhitelist.string[0] != '0') {
								char *whitelistResult = ValidateUserTypedItem(item->itemType, g_deleteBaseItemsWhitelist.string);
								if (!VALIDSTRING(whitelistResult)) {
									PrintIngame(player - g_entities, "Base item %d (%s) is not allowed to be deleted based on whitelist.\n", item->id, item->itemType);
									++errorCount;
									continue;
								}
							}

							G_FreeEntity(item->ent);
							item->baseItemDeleted = qtrue;
							item->baseItemDeleterId = player->client->account->id;
							item->ent = NULL;

							if (!DB_DeleteBaseItem(item)) {
								PrintIngame(player - g_entities, "Error in database operation for deleting base item %d (%s).\n", item->id, item->itemType);
								++errorCount;
							}
							else {
								++deletedCount;
							}
						}

						if (nonDeletedBaseItemCount == 0)
							PrintIngame(player - g_entities, "There are no non-deleted base items to delete.\n");
						else if (deletedCount > 0 || errorCount > 0)
							PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s deleted %d base items. Encountered %d errors.\n", player->client->account->name, deletedCount, errorCount));
					}
				}
			}



			if (deleteAllAdded || deleteAll) {
				if (!g_addItems.integer) {
					PrintIngame(player - g_entities, "Deleting added items is disabled.\n");
				}
				else {
					if (level.addedItemsList.size <= 0) {
						PrintIngame(player - g_entities, "No added items available to delete.\n");
					}
					else {
						int deletedCount = 0, errorCount = 0;
						iterator_t iter;
						ListIterate(&level.addedItemsList, &iter, qfalse);

						while (IteratorHasNext(&iter)) {
							changedItem_t *item = (changedItem_t *)IteratorNext(&iter);

							if (item->addedItemSaved) {
								if (!DB_DeleteAddedItem(item)) {
									PrintIngame(player - g_entities, "Error deleting saved added item %d (%s) from database.\n", item->id, item->itemType);
									++errorCount;
								}
								else {
									++deletedCount;
								}
							}
							else {
								++deletedCount;
							}

							G_FreeEntity(item->ent);
							ListRemove(&level.addedItemsList, item);
						}

						if (deletedCount > 0 || errorCount > 0)
							PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s deleted %d added items. Encountered %d errors.\n", player->client->account->name, deletedCount, errorCount));
					}
				}
			}

		}
		else {
			if (!Q_isanumber(itemIdBuf)) {
				PrintIngame(player - g_entities, "Usage: item delete <item id>\n");
				return;
			}
			int enteredId = atoi(itemIdBuf);

			if (enteredId < MAX_GENTITIES) { // deleting a base item
				if (level.baseItemsList.size <= 0) {
					PrintIngame(player - g_entities, "No item found with id %d.\n", enteredId);
					return;
				}

				iterator_t iter;
				ListIterate(&level.baseItemsList, &iter, qfalse);
				changedItem_t *found = NULL;
				while (IteratorHasNext(&iter)) {
					changedItem_t *item = (changedItem_t *)IteratorNext(&iter);
					if (item->id == enteredId) {
						found = item;
						break;
					}
				}

				if (!found) {
					PrintIngame(player - g_entities, "No item found with id %d.\n", enteredId);
					return;
				}

				if (!g_deleteBaseItems.integer) {
					PrintIngame(player - g_entities, "Deleting base items is disabled.\n");
					return;
				}

				if (found->baseItemDeleted) {
					PrintIngame(player - g_entities, "Base item %d (%s at %d %d %d) was already deleted. To undelete it, enter ^5item undelete %d^7.\n", found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2]), found->id);
					return;
				}

				if (g_deleteBaseItemsWhitelist.string[0] && g_deleteBaseItemsWhitelist.string[0] != '0') {
					char *whitelistResult = ValidateUserTypedItem(found->itemType, g_deleteBaseItemsWhitelist.string);
					if (!VALIDSTRING(whitelistResult)) {
						PrintIngame(player - g_entities, "The selected base item is of an invalid or impermissible type for deletion. Permissible base items for deletion: %s\n", g_deleteBaseItemsWhitelist.string);
						return;
					}
				}

				G_FreeEntity(found->ent);
				found->baseItemDeleted = qtrue;
				found->baseItemDeleterId = player->client->account->id;
				found->ent = NULL;

				if (DB_DeleteBaseItem(found))
					PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s successfully deleted base item %d (%s at %d, %d, %d).\n", player->client->account->name, found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2])));
				else
					PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s encountered an error deleting base item %d (%s at %d, %d, %d)!\n", player->client->account->name, found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2])));
			}
			else { // deleting an added item
				if (level.addedItemsList.size <= 0) {
					PrintIngame(player - g_entities, "No item found with id %d.\n", enteredId);
					return;
				}

				iterator_t iter;
				ListIterate(&level.addedItemsList, &iter, qfalse);
				changedItem_t *found = NULL;
				while (IteratorHasNext(&iter)) {
					changedItem_t *item = (changedItem_t *)IteratorNext(&iter);
					if (item->id == enteredId) {
						found = item;
						break;
					}
				}

				if (!found) {
					PrintIngame(player - g_entities, "No item found with id %d.\n", enteredId);
					return;
				}

				if (!g_addItems.integer) {
					PrintIngame(player - g_entities, "Deleting added items is disabled.\n");
					return;
				}

				if (found->addedItemSaved) {
					if (DB_DeleteAddedItem(found))
						PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s successfully deleted saved item %d (%s at %d, %d, %d) from database.\n", player->client->account->name, found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2])));
					else
						PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s encountered an error deleting saved item %d (%s at %d, %d, %d) from database!\n", player->client->account->name, found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2])));
				}
				else {
					PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s deleted usaved item %d (%s at %d, %d, %d).\n", player->client->account->name, found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2])));
				}

				G_FreeEntity(found->ent);
				ListRemove(&level.addedItemsList, found);
			}
		}
	}
	else if (!Q_stricmpn(arg1, "und", 3) || !Q_stricmpn(arg1, "unr", 3)) {
		if (!g_deleteBaseItems.integer) {
			PrintIngame(player - g_entities, "Undeleting base items is disabled.\n");
			return;
		}

		if (args < 3) {
			PrintIngame(player - g_entities, "Usage: item undelete <base item id>\n");
			return;
		}

		char itemIdBuf[MAX_STRING_CHARS];
		trap_Argv(2, itemIdBuf, sizeof(itemIdBuf));

		if (!Q_stricmpn(itemIdBuf, "all", 3)) {
			if (level.baseItemsList.size <= 0) {
				PrintIngame(player - g_entities, "No base items available to undelete.\n");
				return;
			}

			int numAvailableToUndelete = 0, undeletedCount = 0, errorCount = 0;
			iterator_t iter;
			ListIterate(&level.baseItemsList, &iter, qfalse);

			while (IteratorHasNext(&iter)) {
				changedItem_t *item = (changedItem_t *)IteratorNext(&iter);
				if (!item->baseItemDeleted)
					continue;

				++numAvailableToUndelete;
				gentity_t *itemEnt = G_Spawn();
				VectorCopy(item->origin, itemEnt->s.origin);
				VectorCopy(item->origin, itemEnt->s.pos.trBase);
				VectorCopy(item->origin, itemEnt->r.currentOrigin);
				itemEnt->classname = (char *)item->itemType;

				if (!G_CallSpawn(itemEnt)) {
					G_FreeEntity(itemEnt);
					PrintIngame(player - g_entities, "Error spawning newly undeleted base item %d (%s)!\n", item->id, item->itemType);
					++errorCount;
					continue;
				}

				if (item->isSuspended)
					itemEnt->spawnflags |= 1;

				canSpawnItemStartsolid = qtrue;
				FinishSpawningItem(itemEnt);
				canSpawnItemStartsolid = qfalse;

				itemEnt->s.eFlags |= EF_NODRAW;
				itemEnt->r.svFlags |= SVF_NOCLIENT;
				itemEnt->r.contents = 0;
				itemEnt->nextthink = level.time + 1000;
				itemEnt->think = RespawnItem;

				item->baseItemDeleted = qfalse;
				item->baseItemDeleterId = ACCOUNT_ID_UNLINKED;
				item->ent = itemEnt;

				if (DB_UndeleteBaseItem(item)) {
					++undeletedCount;
				}
				else {
					PrintIngame(player - g_entities, "Error in database operation for undeleting base item %d (%s).\n", item->id, item->itemType);
					++errorCount;
				}
			}

			if (numAvailableToUndelete)
				PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s successfully undeleted %d base items. Encountered %d errors.\n", player->client->account->name, undeletedCount, errorCount));
			else
				PrintIngame(player - g_entities, "No base items available to undelete.\n");
		}
		else {
			if (!Q_isanumber(itemIdBuf)) {
				PrintIngame(player - g_entities, "Usage: item undelete <base item id>\n");
				return;
			}
			int enteredId = atoi(itemIdBuf);

			if (enteredId >= MAX_GENTITIES) {
				PrintIngame(player - g_entities, "This command is for undeleting base items only.\n");
				return;
			}

			if (level.baseItemsList.size <= 0) {
				PrintIngame(player - g_entities, "No base item found with id %d.\n", enteredId);
				return;
			}

			iterator_t iter;
			ListIterate(&level.baseItemsList, &iter, qfalse);
			changedItem_t *found = NULL;
			while (IteratorHasNext(&iter)) {
				changedItem_t *item = (changedItem_t *)IteratorNext(&iter);
				if (item->id == enteredId) {
					found = item;
					break;
				}
			}

			if (!found) {
				PrintIngame(player - g_entities, "No base item found with id %d.\n", enteredId);
				return;
			}

			if (!found->baseItemDeleted) {
				PrintIngame(player - g_entities, "Base item %d (%s at %d %d %d) is not deleted. To delete it, enter ^5item delete %d^7.\n", found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2]), found->id);
				return;
			}

			gentity_t *itemEnt = G_Spawn();
			// NOTE: the new entity might have a different entity number (itemEnt - g_entities) from the original,
			// but the changedItem_t in level.baseItemsList retains its original id (the entity number of the original entity)
			VectorCopy(found->origin, itemEnt->s.origin);
			VectorCopy(found->origin, itemEnt->s.pos.trBase);
			VectorCopy(found->origin, itemEnt->r.currentOrigin);
			itemEnt->classname = (char *)found->itemType;

			if (!G_CallSpawn(itemEnt)) {
				G_FreeEntity(itemEnt);
				PrintIngame(player - g_entities, "Error spawning undeleted item!\n");
				return;
			}

			if (found->isSuspended)
				itemEnt->spawnflags |= 1; // ITMSF_SUSPEND from g_items.c; allow it to be in the air

			canSpawnItemStartsolid = qtrue;
			FinishSpawningItem(itemEnt);
			canSpawnItemStartsolid = qfalse;

			itemEnt->s.eFlags |= EF_NODRAW;
			itemEnt->r.svFlags |= SVF_NOCLIENT;
			itemEnt->r.contents = 0;
			itemEnt->nextthink = level.time + 1000;
			itemEnt->think = RespawnItem;

			found->baseItemDeleted = qfalse;
			found->baseItemDeleterId = ACCOUNT_ID_UNLINKED;
			found->ent = itemEnt;

			if (DB_UndeleteBaseItem(found))
				PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s successfully undeleted base item %d (%s at %d, %d, %d).\n", player->client->account->name, found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2])));
			else
				PrintBasedOnAccountFlags(ACCOUNTFLAG_ITEMLORD, va("%s encountered an error undeleting base item %d (%s at %d, %d, %d)!\n", player->client->account->name, found->id, found->itemType, (int)roundf(found->origin[0]), (int)roundf(found->origin[1]), (int)roundf(found->origin[2])));
		}
	}
	else {
		PrintIngame(player - g_entities,
			"Usage:\n" \
			"^7item list                                                   - list added items\n" \
			"^9item list base                                              - list base items, including deleted ones\n" \
			"^7item add <item name> [optional air/ground] [optional x y z] - add item\n" \
			"^9item save <added item id | all>                             - save added item(s)\n" \
			"^7item delete <item id | allbase | alladded>                  - delete item(s)\n" \
			"^9item undelete <base item id | all>                          - undelete base item(s)^7\n"
		);
		return;
	}
}


#ifdef NEWMOD_SUPPORT
static void Cmd_Svauth_f( gentity_t *ent ) {
	if ( trap_Argc() < 2 ) {
		return;
	}

	if ( !ent || !ent->client ) {
		return;
	}

	if ( !( ent->client->sess.auth > PENDING && ent->client->sess.auth < AUTHENTICATED ) ) {
		return;
	}

	do {
		char encryptedSvauth[CRYPTO_CIPHER_HEX_SIZE];
		trap_Argv( 1, encryptedSvauth, sizeof( encryptedSvauth ) );

		char decryptedSvauth[CRYPTO_CIPHER_RAW_SIZE];
		if ( Crypto_Decrypt( &level.publicKey, &level.secretKey, encryptedSvauth, decryptedSvauth, sizeof( decryptedSvauth ) ) ) {
			G_HackLog( S_COLOR_RED"Failed to decrypt svauth command from client %d\n", ent - g_entities );
			break;
		}

		char *s;

		if ( ent->client->sess.auth == CLANNOUNCE ) {
			int clientKeys[2];

			s = Info_ValueForKey( decryptedSvauth, "ck1" );
			if ( !VALIDSTRING( s ) ) {
				G_HackLog( S_COLOR_RED"svauth command for client %d misses ck1\n", ent - g_entities );
				break;
			}
			clientKeys[0] = atoi( s );

			s = Info_ValueForKey( decryptedSvauth, "ck2" );
			if ( !VALIDSTRING( s ) ) {
				G_HackLog( S_COLOR_RED"svauth command for client %d misses ck2\n", ent - g_entities );
				break;
			}
			clientKeys[1] = atoi( s );

#define RandomConfirmationKey()	( ( rand() << 16 ) ^ rand() ^ trap_Milliseconds() )
			ent->client->sess.serverKeys[0] = RandomConfirmationKey();
			ent->client->sess.serverKeys[1] = RandomConfirmationKey();

			trap_SendServerCommand( ent - g_entities, va( "kls -1 -1 \"clauth\" %d %d %d",
				clientKeys[0] ^ clientKeys[1], ent->client->sess.serverKeys[0], ent->client->sess.serverKeys[1] ) );
			ent->client->sess.auth++;
#ifdef _DEBUG
			G_Printf( "Got keys %d ^ %d = %d from client %d, sent %d and %d\n",
				clientKeys[0], clientKeys[1], clientKeys[0] ^ clientKeys[1], ent - g_entities, ent->client->sess.serverKeys[0], ent->client->sess.serverKeys[1]
			);
#endif
			return;
		} else if ( ent->client->sess.auth == CLAUTH ) {
			int serverKeysXor;

			s = Info_ValueForKey( decryptedSvauth, "skx" );
			if ( !VALIDSTRING( s ) ) {
				G_HackLog( S_COLOR_RED"svauth command for client %d misses skx\n", ent - g_entities );
				break;
			}
			serverKeysXor = atoi( s );

			s = Info_ValueForKey( decryptedSvauth, "cid" );
			if ( !VALIDSTRING( s ) ) {
				G_HackLog( S_COLOR_RED"svauth command for client %d misses cid\n", ent - g_entities );
				break;
			}

			if ( ( ent->client->sess.serverKeys[0] ^ ent->client->sess.serverKeys[1] ) != serverKeysXor ) {
				G_HackLog( S_COLOR_RED"Client %d failed the server key check!\n", ent - g_entities );
				break;
			}

			Crypto_Hash( s, ent->client->sess.cuidHash, sizeof( ent->client->sess.cuidHash ) );
			G_LogPrintf( "Newmod client %d authenticated successfully (cuid hash: %s)\n", ent - g_entities, ent->client->sess.cuidHash );
			ent->client->sess.auth++;

			// now that this client is fully authenticated, init session and account
			G_InitClientSession( ent->client );
			G_InitClientAccount( ent->client );
			G_InitClientRaceRecordsCache( ent->client );
			G_InitClientAimRecordsCache(ent->client);
			G_PrintWelcomeMessage( ent->client );
			if (ent->client->account)
				ent->client->sess.canJoin = qtrue; // assume anyone with an account can join
			TellPlayerToRateMap( ent->client );
			TellPlayerToSetPositions(ent->client);
			RestoreDisconnectedPlayerData(ent);
			ClientUserinfoChanged( ent - g_entities );

			return;
		}
	} while ( 0 );

	// if we got here, auth failed
	ent->client->sess.auth = INVALID;
	G_LogPrintf( "Client %d failed newmod authentication (at state: %d)\n", ent - g_entities, ent->client->sess.auth );
}

static void Cmd_VchatDl_f( gentity_t* ent ) {
	// no longer used
}

static char vchatListArgs[MAX_STRING_CHARS] = { 0 };

// use -1 for everyone
void SendVchatList(int clientNum) {
	if (!VALIDSTRING(vchatListArgs))
		return;

	trap_SendServerCommand(clientNum, va("kls -1 -1 vchl %s", vchatListArgs));
}

#define VCHATPACK_CONFIG_FILE	"vchatpacks.config"

void G_InitVchats() {
	fileHandle_t f;
	int len = trap_FS_FOpenFile(VCHATPACK_CONFIG_FILE, &f, FS_READ);

	memset(vchatListArgs, 0, sizeof(vchatListArgs));

	if (!f || len <= 0) {
		Com_Printf(VCHATPACK_CONFIG_FILE" not found, vchats will be unavailable\n");
		return;
	}

	char* buf = (char*)malloc(len + 1);
	trap_FS_Read(buf, len, f);
	buf[len] = '\0';
	trap_FS_FCloseFile(f);

	char parseBuf[MAX_STRING_CHARS] = { 0 };
	int numParsed = 0;

	cJSON* root = cJSON_Parse(buf);
	if (root) {
		cJSON* download_base = cJSON_GetObjectItemCaseSensitive(root, "download_base");
		if (cJSON_IsString(download_base) && VALIDSTRING(download_base->valuestring) && !strchr(download_base->valuestring, ' ')) {
			Q_strcat(parseBuf, sizeof(parseBuf), va("\"%s\"", download_base->valuestring));

			cJSON* packs = cJSON_GetObjectItemCaseSensitive(root, "packs");
			if (cJSON_IsArray(packs)) {
				int i;
				for (i = 0; i < cJSON_GetArraySize(packs); ++i) {
					cJSON* pack = cJSON_GetArrayItem(packs, i);

					cJSON* pack_name = cJSON_GetObjectItemCaseSensitive(pack, "pack_name");
					cJSON* file_name = cJSON_GetObjectItemCaseSensitive(pack, "file_name");
					cJSON* version = cJSON_GetObjectItemCaseSensitive(pack, "version");

					if (cJSON_IsString(pack_name) && VALIDSTRING(pack_name->valuestring) && !strchr(pack_name->valuestring, ' ') &&
						cJSON_IsString(file_name) && VALIDSTRING(file_name->valuestring) && !strchr(file_name->valuestring, ' ') &&
						cJSON_IsNumber(version) && version > 0)
					{
						Q_strcat(parseBuf, sizeof(parseBuf), va(" %s %s %d", pack_name->valuestring, file_name->valuestring, version->valueint));
						++numParsed;
					}
				}
			}
		}
	}

	cJSON_Delete(root);
	free(buf);

	if (!numParsed) {
		Com_Printf("WARNING: No vchat pack parsed in "VCHATPACK_CONFIG_FILE", invalid format?\n");
		return;
	}

	Com_Printf("Parsed %d vchat packs from "VCHATPACK_CONFIG_FILE"\n", numParsed);

	Q_strncpyz(vchatListArgs, parseBuf, sizeof(vchatListArgs));
	SendVchatList(-1);
}

void Cmd_VchatList_f(gentity_t* ent) {
	SendVchatList(ent - g_entities);
}
#endif

#define MAX_CHANGES_CHUNKS		4
#define CHANGES_CHUNK_SIZE		1000
#define MAX_CHANGES_SIZE		(MAX_CHANGES_CHUNKS * CHANGES_CHUNK_SIZE)
void Cmd_Changes_f(gentity_t *ent) {
	static char changes[MAX_CHANGES_SIZE] = { 0 }, map[MAX_CVAR_VALUE_STRING] = { 0 };
	static qboolean lookedForChanges = qfalse;
	static size_t len = 0;

	if (!lookedForChanges) {
		lookedForChanges = qtrue;
		fileHandle_t f;
		vmCvar_t	mapname;
		trap_Cvar_Register(&mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM);
		Q_strncpyz(map, mapname.string, sizeof(map));
		len = trap_FS_FOpenFile(va("maps/%s.changes", mapname.string), &f, FS_READ);
		if (f) {
			trap_FS_Read(changes, len, f);
			trap_FS_FCloseFile(f);
			if (len >= MAX_CHANGES_SIZE)
				G_LogPrintf("Warning: changelog for map %s is too long (%d chars, should be less than %d)\n", map, len, sizeof(changes));
		}
	}

	if (!changes[0] || !len) {
		trap_SendServerCommand(ent - g_entities, va("print \"No changelog could be found for %s.\n\"", map));
		return;
	}

	trap_SendServerCommand(ent - g_entities, va("print \"Changelog for %s"S_COLOR_WHITE":\n\"", map));

	char fixed[MAX_CHANGES_SIZE] = { 0 };
	char *r = changes, *w = fixed;
	int remaining = MAX_CHANGES_SIZE - 1;
	while (*r) {
		if (*r == '%' && r - changes && isdigit(*(r - 1)) && remaining > 8) {
			Q_strncpyz(w, " percent", remaining);
			remaining -= 8;
			w += 8;
		}
		else if (*r == '%' && remaining > 7) {
			Q_strncpyz(w, "percent", remaining);
			remaining -= 7;
			w += 7;
		}
		else if (*r == '"' && remaining > 2) {
			Q_strncpyz(w, "''", remaining);
			remaining -= 2;
			w += 2;
		}
		else {
			*w = *r;
			remaining--;
			w++;
		}
		r++;
	}
	len = strlen(fixed);

	if (len <= CHANGES_CHUNK_SIZE) { // no chunking necessary
		trap_SendServerCommand(ent - g_entities, va("print \"%s"S_COLOR_WHITE"\n\"", fixed));
	}
	else { // chunk it
		int i, chunks = len / CHANGES_CHUNK_SIZE;
		if (len % CHANGES_CHUNK_SIZE > 0)
			chunks++;
		for (i = 0; i < chunks && i < MAX_CHANGES_CHUNKS; i++) {
			char thisChunk[CHANGES_CHUNK_SIZE + 1];
			Q_strncpyz(thisChunk, fixed + (i * CHANGES_CHUNK_SIZE), sizeof(thisChunk));
			if (thisChunk[0])
				trap_SendServerCommand(ent - g_entities, va("print \"%s%s\"", thisChunk, i == chunks - 1 ? S_COLOR_WHITE"\n" : "")); // add ^7 and line break for last one
		}
	}
}

void Cmd_WhoIs_f( gentity_t* ent ) {
	int clientNum = -1;
	if (trap_Argc() >= 2) {
		char buf[64] = { 0 };
		trap_Argv(1, buf, sizeof(buf));
		gentity_t *found = found = G_FindClient(buf);

		if (!found || !found->client) {
			trap_SendServerCommand(ent - g_entities,
				va("print \"Client %s"S_COLOR_WHITE" not found or ambiguous. Use client number or be more specific.\n\"", buf));
			return;
		}
		clientNum = found - g_entities;
	}

	Table *t = Table_Initialize(qfalse);

	for (int i = 0; i < level.maxclients; i++) {
		if (!level.clients[i].pers.connected || (clientNum != -1 && i != clientNum))
			continue;
		Table_DefineRow(t, &level.clients[i]);
	}

	Table_DefineColumn(t, "#", TableCallback_ClientNum, NULL, qfalse, -1, 2);
	Table_DefineColumn(t, "Name", TableCallback_Name, NULL, qtrue, -1, MAX_NAME_DISPLAYLENGTH);
	Table_DefineColumn(t, "Alias", TableCallback_Alias, NULL, qtrue, -1, MAX_NAME_DISPLAYLENGTH);
	Table_DefineColumn(t, "Country", TableCallback_Country, NULL, qtrue, -1, 64);
	Table_DefineColumn(t, "Notes", TableCallback_Notes, NULL, qtrue, -1, MAX_NAME_DISPLAYLENGTH);

	char buf[4096] = { 0 };
	Table_WriteToBuffer(t, buf, sizeof(buf), qtrue, -1);
	Table_Destroy(t);

	PrintIngame(ent - g_entities, buf);
}

void Cmd_Verify_f(gentity_t *verifier) {
	if (!verifier || !verifier->client || !verifier->client->account || !(verifier->client->account->flags & ACCOUNTFLAG_VERIFICATIONLORD)) {
		return;
	}

	if (!(g_allowVerify.integer & (1 << 0))) {
		PrintIngame(verifier - g_entities, "Verify is disabled.\n");
		return;
	}

	if (trap_Argc() < 3) {
		PrintIngame(verifier - g_entities, "Usage: verify <#> <account name>\n");
		return;
	}

	char clientNumStr[4] = { 0 };
	trap_Argv(1, clientNumStr, sizeof(clientNumStr));

	if (!clientNumStr[0]) {
		PrintIngame(verifier - g_entities, "Usage: verify <#> <account name>\n");
		return;
	}

	const int clientId = atoi(clientNumStr);
	if (!IN_CLIENTNUM_RANGE(clientId) ||
		!g_entities[clientId].inuse ||
		level.clients[clientId].pers.connected != CON_CONNECTED) {
		PrintIngame(verifier - g_entities, "You must enter a valid client ID of a connected client\n");
		return;
	}

	gclient_t *client = &level.clients[clientId];
	char username[MAX_ACCOUNTNAME_LEN];
	trap_Argv(2, username, sizeof(username));

	if (!username[0]) {
		PrintIngame(verifier - g_entities, "Usage: verify <#> <account name>\n");
		return;
	}

	Q_strlwr(username);

	if (client->account) {
		PrintIngame(verifier - g_entities, "This session is already linked to %s.\n", client->account->name);
		return;
	}

	if (!client->session) {
		PrintIngame(verifier - g_entities, "Unable to find client session!\n");
		return;
	}

	accountReference_t acc = G_GetAccountByName(username, qfalse);

	if (!acc.ptr) {
		PrintIngame(verifier - g_entities, "No account found with name '%s'.\n", username);
		return;
	}

	if ((acc.ptr->flags & ACCOUNTFLAG_ADMIN) ||
		(acc.ptr->flags & ACCOUNTFLAG_RCONLOG)) {
		PrintIngame(verifier - g_entities, "Account %s cannot be verified except by rcon.\n", username);
		return;
	}

	if (G_LinkAccountToSession(client->session, acc.ptr)) {
		client->sess.verifiedByVerifyCommand = qtrue;

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

		PrintIngame(verifier - g_entities, "Verified client %d (%s^7) as account %s.\n", clientId, client->pers.netname, username);
		char *s = va("verify: session %d/client %d (%s^7) linked by verifier client %d (%s) to account '%s^7' (id: %d)\n",
			client->session->id, clientId, client->pers.netname, verifier - g_entities, verifier->client->account->name, acc.ptr->name, acc.ptr->id);
		G_Printf(s);
		PrintBasedOnAccountFlags(ACCOUNTFLAG_ADMIN, s);
		trap_Cvar_Set("g_shouldReloadPlayerPugStats", "1");
	}
	else {
		G_Printf("verify: client %d (%s) failed to link client %d to account %s!\n", verifier - g_entities, verifier->client->account->name, clientId, username);
	}
}

void Cmd_Unverify_f(gentity_t *verifier) {
	if (!verifier || !verifier->client || !verifier->client->account || !(verifier->client->account->flags & ACCOUNTFLAG_VERIFICATIONLORD)) {
		return;
	}

	if (!(g_allowVerify.integer & (1 << 1))) {
		PrintIngame(verifier - g_entities, "Unverify is disabled.\n");
		return;
	}

	if (trap_Argc() < 2) {
		PrintIngame(verifier - g_entities, "Usage: unverify <#>\n");
		return;
	}

	char clientNumStr[4] = { 0 };
	trap_Argv(1, clientNumStr, sizeof(clientNumStr));

	if (!clientNumStr[0]) {
		PrintIngame(verifier - g_entities, "Usage: unverify <#>\n");
		return;
	}

	const int clientId = atoi(clientNumStr);
	if (!IN_CLIENTNUM_RANGE(clientId) ||
		!g_entities[clientId].inuse ||
		level.clients[clientId].pers.connected != CON_CONNECTED) {
		PrintIngame(verifier - g_entities, "You must enter a valid client ID of a connected client\n");
		return;
	}

	gclient_t *client = &level.clients[clientId];
	if (client == verifier->client)
		return; // attempting to unverify self?

	if (!client->account) {
		PrintIngame(verifier - g_entities, "Client %d (%s^7) is already not verified.\n", clientId, client->pers.netname);
		return;
	}

	if (!client->session) {
		PrintIngame(verifier - g_entities, "Unable to find client session!\n");
		return;
	}

	if (!client->sess.verifiedByVerifyCommand) {
		PrintIngame(verifier - g_entities, "Unable to unverify accounts you did not verify.\n");
		return;
	}

	if ((client->account->flags & ACCOUNTFLAG_ADMIN) ||
		(client->account->flags & ACCOUNTFLAG_RCONLOG)) {
		PrintIngame(verifier - g_entities, "Account %s cannot be unverified except by rcon.\n", client->account->name);
		return;
	}

	// save a reference so we can print the name even after unlinking
	accountReference_t acc;
	acc.ptr = client->account;
	acc.online = qtrue;
	int sessionId = client->session->id;

	if (G_UnlinkAccountFromSession(client->session)) {
		client->sess.verifiedByVerifyCommand = qfalse;
		PrintIngame(verifier - g_entities, "Unverified client %d (%s^7).\n", clientId, client->pers.netname);
		char *s = va("unverify: session %d/client %d (%s^7) unlinked by verifier client %d (%s^7)\n",
			sessionId, clientId, client->pers.netname, verifier - g_entities, verifier->client->pers.netname);
		G_Printf(s);
		PrintBasedOnAccountFlags(ACCOUNTFLAG_ADMIN, s);
		trap_Cvar_Set("g_shouldReloadPlayerPugStats", "1");
	}
	else {
		G_Printf("unverify: client %d (%s) failed to unlink client %d!\n", verifier - g_entities, verifier->client->pers.netname, clientId);
	}

}

void Cmd_PrintStats_f(gentity_t *ent) {
	if (trap_Argc() < 2) {
		Stats_Print(ent, "general", NULL, 0, qtrue, NULL);
	}
	else if (trap_Argc() == 2) {
		char subcmd[MAX_STRING_CHARS] = { 0 };
		trap_Argv(1, subcmd, sizeof(subcmd));
		if (!Q_stricmpn(subcmd, "we", 2) || !Q_stricmpn(subcmd, "wpn", 3)) {
			if (ent->client && (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE))
				Stats_Print(ent, subcmd, NULL, 0, qtrue, ent->client->stats); // ingame player with no args; just print himself
			else
				Stats_Print(ent, subcmd, NULL, 0, qtrue, NULL);
		}
		else {
			Stats_Print(ent, subcmd, NULL, 0, qtrue, NULL);
		}
	}
	else {
		char subcmd[MAX_STRING_CHARS] = { 0 };
		trap_Argv(1, subcmd, sizeof(subcmd));

		if (!Q_stricmpn(subcmd, "we", 2) || !Q_stricmpn(subcmd, "wpn", 3)) {
			char weaponPlayerArg[MAX_STRING_CHARS] = { 0 };
			trap_Argv(2, weaponPlayerArg, sizeof(weaponPlayerArg));
			if (weaponPlayerArg[0]) {
				if (!Q_stricmp(weaponPlayerArg, "all") || !Q_stricmp(weaponPlayerArg, "-1")) {
					Stats_Print(ent, subcmd, NULL, 0, qtrue, NULL);
				}
				else {
					stats_t *found = GetStatsFromString(weaponPlayerArg);
					if (!found) {
						PrintIngame(ent - g_entities, "Client %s^7 not found or ambiguous. Use client number or be more specific.\n", weaponPlayerArg);
						return;
					}
					Stats_Print(ent, subcmd, NULL, 0, qtrue, found);
				}
			}
			else {
				if (ent->client && (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE))
					Stats_Print(ent, subcmd, NULL, 0, qtrue, ent->client->stats); // ingame player with no args; just print himself
				else
					Stats_Print(ent, subcmd, NULL, 0, qtrue, NULL);
			}
		}
		else {
			Stats_Print(ent, subcmd, NULL, 0, qtrue, NULL);
		}
	}
}

static int AccountFromString(char *s, char *nameOut, size_t nameOutSize) {
	if (!VALIDSTRING(s))
		return -1;

	int accountId = -1;
	if (Q_isanumber(s)) {
		int num = atoi(s);
		if (num >= 0 && num < MAX_CLIENTS && g_entities[num].inuse && g_entities[num].client && g_entities[num].client->pers.connected != CON_DISCONNECTED) {
			if (g_entities[num].client->account) {
				Q_strncpyz(nameOut, g_entities[num].client->pers.netname, nameOutSize);
				return g_entities[num].client->account->id;
			}
			else {
				return -1;
			}
		}
		else {
			return -1;
		}
	}
	else {
		char lowercase[MAX_QPATH] = { 0 };
		Q_strncpyz(lowercase, s, sizeof(lowercase));
		Q_strlwr(lowercase);
		account_t account = { 0 };
		if (G_DBGetAccountByName(lowercase, &account)) {
			Q_strncpyz(nameOut, account.name, nameOutSize);
			return account.id;
		}
		else {
			gentity_t *found = G_FindClient(s);
			if (found && found->client) {
				if (found->client->account) {
					Q_strncpyz(nameOut, va("%s^7 (%s^7)", found->client->pers.netname, found->client->account->name), nameOutSize);
					return found->client->account->id;
				}
				else {
					return -1;
				}
			}
			else {
				return -1;
			}
		}
	}
}

static void PrintPugStatsHelp(gentity_t *ent) {
	assert(ent);
	PrintIngame(ent - g_entities, "Usage:\n" \
		"^7pugstats [pos]          - list the top players for a particular position\n"
		"^9pugstats [player]       - view a player's overall winrates\n"
		"^7pugstats [player] [pos] - view a player's stats and winrates on a position\n"
		"^9pugstats players        - view a list of players with valid stats\n"
		"\n"
		"Arguments can be entered in any order.\n"
		"You can use 'me' for player name.\n"
		"Only matchups played >= 5 times are considered in winrates.\n");
}

void Cmd_PugStats_f(gentity_t *ent) {
	if (trap_Argc() < 2) {
		PrintPugStatsHelp(ent);
		return;
	}

	char args[2][MAX_STRING_CHARS];
	trap_Argv(1, args[0], sizeof(args[0]));
	if (trap_Argc() >= 3)
		trap_Argv(2, args[1], sizeof(args[1]));
	else
		args[1][0] = '\0';

	if (!args[0][0]) {
		PrintPugStatsHelp(ent);
		return;
	}

	int clientNum = ent - g_entities;
	qboolean hasAccount = !!(ent->client && ent->client->account && VALIDSTRING(ent->client->account->name));

	if (!Q_stricmp(args[0], "players")) {
		G_DBPrintPlayersWithStats(clientNum);
		if (g_shouldReloadPlayerPugStats.integer)
			PrintIngame(clientNum, "Note: there are pugstats updates currently pending. Pugstats may change on map restart/change.\n");
		return;
	}
	
	ctfPosition_t pos = CTFPOSITION_UNKNOWN;
	int accountId = -1;
	char name[MAX_NAME_LENGTH] = { 0 };
	float *statPtr = NULL;

	// loop through the arguments so that they can be written in any order
	for (int i = 0; i < 2; i++) {
		char *arg = args[i];
		if (!VALIDSTRING(arg))
			continue;

		if (!pos && (pos = CtfPositionFromString(arg)))
			continue;

		if (accountId == -1 && !Q_stricmp(arg, "me")) {
			if (!hasAccount) {
				PrintIngame(clientNum, "^3You must have an account in order to view your own pugstats. Contact an admin for help setting up an account.^7\n");
				return;
			}
			accountId = ent->client->account->id;
			continue;
		}

		if (accountId == -1 && (accountId = AccountFromString(arg, name, sizeof(name))) != -1)
			continue;

		PrintIngame(clientNum, "Unrecognized argument '%s^7'\n", arg);
		PrintPugStatsHelp(ent);
		return;
	}

	if (!pos && accountId == -1)
		return; // ???

	if (accountId != -1) {
		PrintIngame(clientNum, "%s stats%s:\n", hasAccount && accountId == ent->client->account->id ? "Your" : va("%s^7's", name), pos ? va(" on ^6%s^7", NameForPos(pos)) : "");
		G_DBPrintPerMapWinrates(accountId, pos, clientNum);
		G_DBPrintWinrates(accountId, pos, clientNum); // if no pos, then force it to show all winrates
		if (pos) {
			if (!G_DBPrintPositionStatsForPlayer(accountId, pos, clientNum, name)) {
				if (hasAccount && accountId == ent->client->account->id)
					PrintIngame(clientNum, "You have no %s pugstats.\n", NameForPos(pos));
				else
					PrintIngame(clientNum, "%s^7 has no %s pugstats.\n", name, NameForPos(pos));
			}
		}
		if (g_shouldReloadPlayerPugStats.integer)
			PrintIngame(clientNum, "Note: there are pugstats updates currently pending. Pugstats may change on map restart/change.\n");
	}
	else {
		G_DBPrintTopPlayersForPosition(pos, clientNum);
		if (g_shouldReloadPlayerPugStats.integer)
			PrintIngame(clientNum, "Note: there are pugstats updates currently pending. Pugstats may change on map restart/change.\n");
	}
}

#define MAX_PRINT_CHARS		( 1022 - 9 ) // server commands are limited to 1022 chars, minus 9 chars for: print "\n"
#define MAX_PRINTS_AT_ONCE	4 // more than 4 full server prints at once causes noticeable lag spikes when issuing the cmd
#define MAX_HELP_BYTES		( MAX_PRINT_CHARS * MAX_PRINTS_AT_ONCE )

qboolean helpEnabled = qfalse;
char help[MAX_PRINTS_AT_ONCE][MAX_PRINT_CHARS + 1];

void G_LoadHelpFile(const char *filename) {
	int len;
	fileHandle_t f;

	len = trap_FS_FOpenFile( filename, &f, FS_READ );

	if ( !f || !len ) {
		trap_Printf( va( S_COLOR_YELLOW "Help file %s not found or empty, disabling /help" S_COLOR_WHITE "\n", filename ) );
		return;
	}

	if ( len > MAX_HELP_BYTES ) {
		trap_Printf( va( S_COLOR_YELLOW "Help file %s is too large (%d bytes, max is %d), disabling /help" S_COLOR_WHITE "\n", filename, len, MAX_HELP_BYTES ) );
		trap_FS_FCloseFile( f );
		return;
	}

	char* buf = malloc( len + 1 );

	trap_FS_Read( buf, len, f );
	buf[len] = '\0';
	trap_FS_FCloseFile( f );

	// break down the buffer into multiple buffers sized for printing

	int i = 0;
	char* bufPtr = buf;

	while ( len > 0 && i < sizeof( help ) / sizeof( help[0] ) ) {
		size_t lenToCopy = len % MAX_PRINT_CHARS;
		if ( !lenToCopy ) lenToCopy = MAX_PRINT_CHARS;

		memcpy( help[i], bufPtr, lenToCopy );
		help[i][lenToCopy] = '\0';

		bufPtr += lenToCopy;
		len -= lenToCopy;
		++i;
	}

	free( buf );

	Com_Printf( "Loaded help file %s sucessfully\n", filename );
	helpEnabled = qtrue;
}

void Cmd_Help_f( gentity_t *ent ) {
	int i;

	for ( i = 0; i < sizeof( help ) / sizeof( help[0] ); ++i ) {
		if ( VALIDSTRING( help[i] ) ) {
			// only append a new line if there is no print after this one
			if ( ( i + 1 >= sizeof( help ) / sizeof( help[0] ) ) || !VALIDSTRING( help[i + 1] ) ) {
				trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", help[i] ) );
			} else {
				trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", help[i] ) );
			}
		}
	}
}

void Cmd_RPS_f(gentity_t *ent) {
	RockPaperScissors(ent);
}

void Cmd_EngageDuel_f(gentity_t *ent)
{
	if (g_rockPaperScissors.integer) {
		RockPaperScissors(ent);
		return;
	}

	trace_t tr;
	vec3_t forward, fwdOrg;
	int duelrange = (g_gametype.integer == GT_CTF) ? 1024 : 256;

	if (!g_privateDuel.integer)
	{
		return;
	}

	if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
	{ //rather pointless in this mode..
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")) );
		return;
	}

	if (g_gametype.integer >= GT_TEAM && !g_teamPrivateDuels.integer) 
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")) );
		return;
	}

	if ( ent->client->sess.inRacemode ) {
		return;
	}

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (g_gametype.integer == GT_CTF ){
		if (ent->client->ps.weapon != WP_BRYAR_PISTOL
			&& ent->client->ps.weapon != WP_SABER
			&& ent->client->ps.weapon != WP_MELEE){
			return; //pistol duel in ctf
		}
	} else {
		if (ent->client->ps.weapon != WP_SABER) {
			return;
		}
	}

	if (ent->client->ps.saberInFlight)
	{
		return;
	}

	if (ent->client->ps.duelInProgress)
	{
		return;
	}

	if ( ent->client->sess.inRacemode )
	{
		return;
	}

	//New: Don't let a player duel if he just did and hasn't waited 10 seconds yet (note: If someone challenges him, his duel timer will reset so he can accept)
	if (ent->client->ps.fd.privateDuelTime > level.time)
	{
		trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "CANTDUEL_JUSTDID")) );
		return;
	}

	AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );

	fwdOrg[0] = ent->client->ps.origin[0] + forward[0]*duelrange;
	fwdOrg[1] = ent->client->ps.origin[1] + forward[1]*duelrange;
	fwdOrg[2] = (ent->client->ps.origin[2]+ent->client->ps.viewheight) + forward[2]*duelrange;

	trap_Trace(&tr, ent->client->ps.origin, NULL, NULL, fwdOrg, ent->s.number, MASK_PLAYERSOLID);

	if (tr.fraction != 1 && tr.entityNum < MAX_CLIENTS)
	{
		gentity_t *challenged = &g_entities[tr.entityNum];

		if (!challenged || !challenged->client || !challenged->inuse ||
			challenged->health < 1 || challenged->client->ps.stats[STAT_HEALTH] < 1 ||
			 challenged->client->ps.duelInProgress ||
			challenged->client->ps.saberInFlight)
		{
			return;
		}

		if (g_gametype.integer == GT_CTF ){
			if ((challenged->client->ps.weapon != WP_SABER)
				&& (challenged->client->ps.weapon != WP_BRYAR_PISTOL)
				&& (challenged->client->ps.weapon != WP_MELEE)){ 
				return;
			}
		} else {
			if (challenged->client->ps.weapon != WP_SABER){
				return;
			}
		}

		if (g_gametype.integer >= GT_TEAM && OnSameTeam(ent, challenged))
		{
			return;
		}

		//auto accept for testing, remove in release!
		if (challenged->client->ps.duelIndex == ent->s.number && challenged->client->ps.duelTime >= level.time)
		{
			int lifes, shields;

			trap_SendServerCommand(-1, va("print \"%s^7 %s %s!\n\"", challenged->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELACCEPT"), ent->client->pers.netname) );

			ent->client->ps.duelInProgress = qtrue;
			challenged->client->ps.duelInProgress = qtrue;

			ent->client->ps.duelTime = level.time + 2000;
			challenged->client->ps.duelTime = level.time + 2000;

			G_AddEvent(ent, EV_PRIVATE_DUEL, 1);
			G_AddEvent(challenged, EV_PRIVATE_DUEL, 1);

			//set default duel values for life and shield
			shields = g_duelShields.integer;

			if (shields > 100)
				shields = 100;

			ent->client->ps.stats[STAT_ARMOR] = shields;
			challenged->client->ps.stats[STAT_ARMOR] = shields;

			ent->client->ps.fd.forcePower = 100;
			challenged->client->ps.fd.forcePower = 100;

			if (g_gametype.integer == GT_CTF){
				//only meele and blaster in captain duel
				ent->client->preduelWeaps = ent->client->ps.stats[STAT_WEAPONS];
				challenged->client->preduelWeaps = challenged->client->ps.stats[STAT_WEAPONS];

				ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE);
				challenged->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE);
				ent->client->ps.weapon = WP_MELEE;
				challenged->client->ps.weapon = WP_MELEE;
			}

			lifes = g_duelLifes.integer;

			if (lifes < 1)
				lifes = 1;
			else if (lifes > ent->client->ps.stats[STAT_MAX_HEALTH])
				lifes = ent->client->ps.stats[STAT_MAX_HEALTH];

			ent->client->ps.stats[STAT_HEALTH] = ent->health = lifes;
			challenged->client->ps.stats[STAT_HEALTH] = challenged->health = lifes;
			
			//remove weapons and powerups here for ctf captain duel, 
			//disable saber and all forces except jump
			//watchout for flags etc
			if (g_gametype.integer == GT_CTF){
				int i;
				//return flags
				if ( ent->client->ps.powerups[PW_NEUTRALFLAG] ) {		
					Team_ReturnFlag( TEAM_FREE );
					ent->client->ps.powerups[PW_NEUTRALFLAG] = 0;
				}
				else if ( ent->client->ps.powerups[PW_REDFLAG] ) {		
					Team_ReturnFlag( TEAM_RED );
					ent->client->ps.powerups[PW_REDFLAG] = 0;
				}
				else if ( ent->client->ps.powerups[PW_BLUEFLAG] ) {	
					Team_ReturnFlag( TEAM_BLUE );
					ent->client->ps.powerups[PW_BLUEFLAG] = 0;
				}
				if ( challenged->client->ps.powerups[PW_NEUTRALFLAG] ) {		
					Team_ReturnFlag( TEAM_FREE );
					challenged->client->ps.powerups[PW_NEUTRALFLAG] = 0;
				}
				else if ( challenged->client->ps.powerups[PW_REDFLAG] ) {		
					Team_ReturnFlag( TEAM_RED );
					challenged->client->ps.powerups[PW_REDFLAG] = 0;
				}
				else if ( challenged->client->ps.powerups[PW_BLUEFLAG] ) {	
					Team_ReturnFlag( TEAM_BLUE );
					challenged->client->ps.powerups[PW_BLUEFLAG] = 0;
				}

				//remove other powerups
				for(i=0;i<PW_NUM_POWERUPS;++i){
					ent->client->ps.powerups[i] = 0;
					challenged->client->ps.powerups[i] = 0;
				}
			}


			//Holster their sabers now, until the duel starts (then they'll get auto-turned on to look cool)

			if (!ent->client->ps.saberHolstered)
			{
				if (ent->client->saber[0].soundOff)
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				}
				if (ent->client->saber[1].soundOff &&
					ent->client->saber[1].model[0])
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
				}
				ent->client->ps.weaponTime = 400;
				ent->client->ps.saberHolstered = 2;
			}
			if (!challenged->client->ps.saberHolstered)
			{
				if (challenged->client->saber[0].soundOff)
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[0].soundOff);
				}
				if (challenged->client->saber[1].soundOff &&
					challenged->client->saber[1].model[0])
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[1].soundOff);
				}
				challenged->client->ps.weaponTime = 400;
				challenged->client->ps.saberHolstered = 2;
			}
		}
		else
		{
			//Print the message that a player has been challenged in private, only announce the actual duel initiation in private
			trap_SendServerCommand( challenged-g_entities, va("cp \"%s^7 %s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGE")) );
			trap_SendServerCommand( ent-g_entities, va("cp \"%s %s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGED"), challenged->client->pers.netname) );
		}

		challenged->client->ps.fd.privateDuelTime = 0; //reset the timer in case this player just got out of a duel. He should still be able to accept the challenge.

		ent->client->ps.forceHandExtend = HANDEXTEND_DUELCHALLENGE;
		ent->client->ps.forceHandExtendTime = level.time + 1000;

		ent->client->ps.duelIndex = challenged->s.number;
		ent->client->ps.duelTime = level.time + 5000;
	}
}

#ifndef FINAL_BUILD
extern stringID_table_t animTable[MAX_ANIMATIONS+1];

void Cmd_DebugSetSaberMove_f(gentity_t *self)
{
	int argNum = trap_Argc();
	char arg[MAX_STRING_CHARS];

	if (argNum < 2)
	{
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	self->client->ps.saberMove = atoi(arg);
	self->client->ps.saberBlocked = BLOCKED_BOUNCE_MOVE;

	if (self->client->ps.saberMove >= LS_MOVE_MAX)
	{
		self->client->ps.saberMove = LS_MOVE_MAX-1;
	}

	Com_Printf("Anim for move: %s\n", animTable[saberMoveData[self->client->ps.saberMove].animToUse].name);
}

void Cmd_DebugSetBodyAnim_f(gentity_t *self, int flags)
{
	int argNum = trap_Argc();
	char arg[MAX_STRING_CHARS];
	int i = 0;

	if (argNum < 2)
	{
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	while (i < MAX_ANIMATIONS)
	{
		if (!Q_stricmp(arg, animTable[i].name))
		{
			break;
		}
		i++;
	}

	if (i == MAX_ANIMATIONS)
	{
		Com_Printf("Animation '%s' does not exist\n", arg);
		return;
	}

	G_SetAnim(self, NULL, SETANIM_BOTH, i, flags, 0);

	Com_Printf("Set body anim to %s\n", arg);
}
#endif

void StandardSetBodyAnim(gentity_t *self, int anim, int flags)
{
	G_SetAnim(self, NULL, SETANIM_BOTH, anim, flags, 0);
}

void DismembermentTest(gentity_t *self);

void Bot_SetForcedMovement(int bot, int forward, int right, int up);

#ifndef FINAL_BUILD
extern void DismembermentByNum(gentity_t *self, int num);
extern void G_SetVehDamageFlags( gentity_t *veh, int shipSurf, int damageLevel );
#endif

static int G_ClientNumFromNetname(char *name)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->client &&
			!Q_stricmp(ent->client->pers.netname, name))
		{
			return ent->s.number;
		}
		i++;
	}

	return -1;
}

qboolean TryGrapple(gentity_t *ent)
{
	if (ent->client->ps.weaponTime > 0)
	{ //weapon busy
		return qfalse;
	}
	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{ //force power or knockdown or something
		return qfalse;
	}
	if (ent->client->grappleState)
	{ //already grappling? but weapontime should be > 0 then..
		return qfalse;
	}

	if (ent->client->ps.weapon != WP_SABER && ent->client->ps.weapon != WP_MELEE)
	{
		return qfalse;
	}

	if (ent->client->ps.weapon == WP_SABER && !ent->client->ps.saberHolstered)
	{
		Cmd_ToggleSaber_f(ent);
		if (!ent->client->ps.saberHolstered)
		{ //must have saber holstered
			return qfalse;
		}
	}

	G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, BOTH_KYLE_GRAB, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
	if (ent->client->ps.torsoAnim == BOTH_KYLE_GRAB)
	{ //providing the anim set succeeded..
		ent->client->ps.torsoTimer += 500; //make the hand stick out a little longer than it normally would
		if (ent->client->ps.legsAnim == ent->client->ps.torsoAnim)
		{
			ent->client->ps.legsTimer = ent->client->ps.torsoTimer;
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
		return qtrue;
	}

	return qfalse;
}

#ifndef FINAL_BUILD
qboolean saberKnockOutOfHand(gentity_t *saberent, gentity_t *saberOwner, vec3_t velocity);
#endif

/*
=================
ClientCommand
=================
*/
void ClientCommand( int clientNum ) {
	gentity_t *ent;
	char	cmd[MAX_TOKEN_CHARS];

	ent = g_entities + clientNum;
	if ( !ent->client ) {
		return;		// not fully in game yet
	}


	trap_Argv( 0, cmd, sizeof( cmd ) );

	//rww - redirect bot commands
	if (strstr(cmd, "bot_") && AcceptBotCommand(cmd, ent))
	{
		return;
	}
	//end rww

	if (Q_stricmp (cmd, "say") == 0 || !Q_stricmp(cmd, "sm_say")) {
		Cmd_Say_f (ent, SAY_ALL, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "say_team") == 0 || !Q_stricmp(cmd, "sm_say_team")) {
		if (g_gametype.integer < GT_TEAM)
		{ //not a team game, just refer to regular say.
			Cmd_Say_f (ent, SAY_ALL, qfalse);
		}
		else
		{
			Cmd_Say_f (ent, SAY_TEAM, qfalse);
		}
		return;
	}
	if (Q_stricmp (cmd, "tell") == 0) {
		Cmd_Tell_f ( ent, NULL );
		return;
	}

	if (Q_stricmp(cmd, "voice_cmd") == 0)
	{
		Cmd_VoiceCommand_f(ent);
		return;
	}

	if (Q_stricmp (cmd, "score") == 0) {
		Cmd_Score_f (ent);
		return;
	}

	if (!Q_stricmp(cmd, "unlagged")) {
		Cmd_Unlagged_f(ent);
		return;
	}

	if (Q_stricmp(cmd, "inkognito") == 0) {
		if (ent && ent->client){
			ent->client->sess.isInkognito = ent->client->sess.isInkognito ? qfalse : qtrue;

			trap_SendServerCommand( ent-g_entities, 
			va("print \"Inkognito %s\n\"", ent->client->sess.isInkognito ? "ON" : "OFF"));
#ifdef NEWMOD_SUPPORT
			trap_SendServerCommand( ent - g_entities, va( "kls -1 -1 \"inko\" \"%d\"", ent->client->sess.isInkognito ) );
#endif
		}
		return;
	}

	// ignore all other commands when at intermission
	if (level.intermissiontime)
	{
		if (!Q_stricmp(cmd, "forcechanged"))
		{ //special case: still update force change
			Cmd_ForceChanged_f (ent);
			return;
		}
		else if ( !Q_stricmp( cmd, "ctfstats" ) || !Q_stricmp(cmd, "stats") || !Q_stricmp(cmd, "stat"))
		{ // special case: we want people to read their other stats as suggested
			Cmd_PrintStats_f( ent );
			return;
		}
		else if (!Q_stricmp(cmd, "pugstats") || !Q_stricmp(cmd, "pug")) {
			Cmd_PugStats_f(ent);
			return;
		}
		else if (!Q_stricmp(cmd, "tier") || !Q_stricmp(cmd, "tiers") || !Q_stricmp(cmd, "tierlist") || !Q_stricmp(cmd, "tierlists") ||
			!Q_stricmp(cmd, "teir") || !Q_stricmp(cmd, "teirs") || !Q_stricmp(cmd, "teirlist") || !Q_stricmp(cmd, "teirlists")) { // allow idiots to use it too
			Cmd_Tier_f(ent);
			return;
		}
		else if (!Q_stricmp(cmd, "rating")) {
			Cmd_Rating_f(ent);
			return;
		}
		else if (!Q_stricmp(cmd, "pos")) {
			Cmd_Pos_f(ent);
			return;
		}
		else if ( Q_stricmp( cmd, "whois" ) == 0 )
		{
			Cmd_WhoIs_f( ent );
			return;
		}
		else if (!Q_stricmp(cmd, "verify")) {
			Cmd_Verify_f(ent);
			return;
		}
		else if (!Q_stricmp(cmd, "unverify")) {
			Cmd_Unverify_f(ent);
			return;
		}
		else if (!Q_stricmp(cmd, "changes")) {
			Cmd_Changes_f(ent);
			return;
		}
		else if ( !Q_stricmp( cmd, "toptimes" ) || !Q_stricmp( cmd, "fastcaps" ) )
		{
			Cmd_TopTimes_f( ent );
			return;
		}
		else if (!Q_stricmp(cmd, "topaim") || !Q_stricmp(cmd, "topaims"))
		{
			Cmd_TopAim_f(ent);
			return;
		}
#ifdef NEWMOD_SUPPORT
		else if (Q_stricmp(cmd, "svauth") == 0 && ent->client->sess.auth > PENDING && ent->client->sess.auth < AUTHENTICATED) {
			Cmd_Svauth_f(ent);
			return;
		}
		else if ( !Q_stricmp( cmd, "vchat" ) ) {
			Cmd_Vchat_f( ent );
			return;
		}
		else if ( !Q_stricmp( cmd, "vchatdl" ) ) {
			Cmd_VchatDl_f( ent );
			return;
		}
		else if (!Q_stricmp(cmd, "vchl")) {
			Cmd_VchatList_f(ent);
			return;
		}
#endif
			trap_SendServerCommand( clientNum, va("print \"%s (%s) \n\"", G_GetStringEdString("MP_SVGAME", "CANNOT_TASK_INTERMISSION"), cmd ) );
		return;
	}

	if (Q_stricmp(cmd, "give") == 0)
	{
		Cmd_Give_f(ent, 0);
	}
	else if (Q_stricmp(cmd, "giveother") == 0)
	{ //for debugging pretty much
		Cmd_Give_f(ent, 1);
	}
	else if (Q_stricmp(cmd, "t_use") == 0 && CheatsOk(ent))
	{ //debug use map object
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			gentity_t *targ;

			trap_Argv(1, sArg, sizeof(sArg));
			targ = G_Find(NULL, FOFS(targetname), sArg);

			while (targ)
			{
				if (targ->use)
				{
					targ->use(targ, ent, ent);
				}
				targ = G_Find(targ, FOFS(targetname), sArg);
			}
		}
	}
	else if (Q_stricmp(cmd, "god") == 0)
		Cmd_God_f(ent);
	else if (Q_stricmp(cmd, "notarget") == 0)
		Cmd_Notarget_f(ent);
	else if (Q_stricmp(cmd, "noclip") == 0)
		Cmd_Noclip_f(ent);
	else if (Q_stricmp(cmd, "NPC") == 0 && CheatsOk(ent))
	{
		Cmd_NPC_f(ent);
	}
	else if (Q_stricmp(cmd, "kill") == 0)
		Cmd_Kill_f(ent);
	else if (Q_stricmp(cmd, "levelshot") == 0)
		Cmd_LevelShot_f(ent);
	else if (Q_stricmp(cmd, "follow") == 0)
		Cmd_Follow_f(ent);
	else if (Q_stricmp(cmd, "follownext") == 0)
		Cmd_FollowCycle_f(ent, 1);
	else if (Q_stricmp(cmd, "followprev") == 0)
		Cmd_FollowCycle_f(ent, -1);
	else if (Q_stricmp(cmd, "followflag") == 0)
		Cmd_FollowFlag_f(ent);
	else if (Q_stricmp(cmd, "followtarget") == 0)
		Cmd_FollowTarget_f(ent);
	else if (Q_stricmp(cmd, "team") == 0 || Q_stricmp(cmd, "join") == 0)
		Cmd_Team_f(ent);
	else if (Q_stricmp(cmd, "duelteam") == 0)
		Cmd_DuelTeam_f(ent);
	else if (Q_stricmp(cmd, "siegeclass") == 0)
		Cmd_SiegeClass_f(ent);
	else if (Q_stricmp(cmd, "class") == 0)
		Cmd_Class_f(ent);
	else if (Q_stricmp(cmd, "forcechanged") == 0)
		Cmd_ForceChanged_f(ent);
	else if (Q_stricmp(cmd, "where") == 0)
		Cmd_Where_f(ent);
#ifdef _DEBUG
	else if (Q_stricmp(cmd, "when") == 0)
		Cmd_When_f(ent);
#endif
	else if (!Q_stricmp(cmd, "pack"))
		Cmd_Pack_f(ent);
	else if (Q_stricmp(cmd, "callvote") == 0)
		Cmd_CallVote_f(ent, PAUSE_NONE);
	else if (!Q_stricmp(cmd, "pause"))
		Cmd_CallVote_f(ent, PAUSE_PAUSED); // allow "pause" command as alias for "callvote pause"
	else if (!Q_stricmp(cmd, "unpause"))
		Cmd_CallVote_f(ent, PAUSE_UNPAUSING); // allow "unpause" command as alias for "callvote unpause"
	else if (Q_stricmp(cmd, "vote") == 0 ||
		!Q_stricmp(cmd, "votre") ||
		!Q_stricmp(cmd, "vortre") ||
		!Q_stricmp(cmd, "vorte") ||
		!Q_stricmp(cmd, "vortr") ||
		!Q_stricmp(cmd, "voter")) // get fucked shitty french typos
		Cmd_Vote_f(ent, NULL);
	else if (Q_stricmp(cmd, "ready") == 0 || !Q_stricmp(cmd, "readyup"))
		Cmd_Ready_f(ent);
	else if (!Q_stricmp(cmd, "mappool") || !Q_stricmp(cmd, "mappools") || !Q_stricmp(cmd, "listpool") || !Q_stricmp(cmd, "listpools") ||
		!Q_stricmp(cmd, "map_pool") || !Q_stricmp(cmd, "map_pools") || !Q_stricmp(cmd, "list_pool") || !Q_stricmp(cmd, "list_pools") ||
		!Q_stricmp(cmd, "poolmap") || !Q_stricmp(cmd, "pool_map") || !Q_stricmp(cmd, "poollist") || !Q_stricmp(cmd, "pool_list") ||
		!Q_stricmp(cmd, "poolmaps") || !Q_stricmp(cmd, "pool_maps") || !Q_stricmp(cmd, "pools") || !Q_stricmp(cmd, "pool")) // fuck off
		Cmd_MapPool_f(ent);
	else if (!Q_stricmp(cmd, "tier") || !Q_stricmp(cmd, "tiers") || !Q_stricmp(cmd, "tierlist") || !Q_stricmp(cmd, "tierlists") ||
		!Q_stricmp(cmd, "teir") || !Q_stricmp(cmd, "teirs") || !Q_stricmp(cmd, "teirlist") || !Q_stricmp(cmd, "teirlists")) // allow idiots to use it too
		Cmd_Tier_f(ent);
	else if (!Q_stricmp(cmd, "rating"))
		Cmd_Rating_f(ent);
	else if (!Q_stricmp(cmd, "pos"))
		Cmd_Pos_f(ent);
	else if (!Q_stricmp(cmd, "item"))
		Cmd_Item_f(ent);
	else if (Q_stricmp(cmd, "whois") == 0)
		Cmd_WhoIs_f(ent);
	else if (!Q_stricmp(cmd, "verify"))
		Cmd_Verify_f(ent);
	else if (!Q_stricmp(cmd, "unverify"))
		Cmd_Unverify_f(ent);
	else if (!Q_stricmp(cmd, "changes"))
		Cmd_Changes_f(ent);
	else if (Q_stricmp(cmd, "ctfstats") == 0 || Q_stricmp(cmd, "stats") == 0 || Q_stricmp(cmd, "stat") == 0)
		Cmd_PrintStats_f(ent);
	else if (!Q_stricmp(cmd, "pugstats") || !Q_stricmp(cmd, "pug"))
		Cmd_PugStats_f(ent);
	else if ( helpEnabled && ( Q_stricmp( cmd, "help" ) == 0 || Q_stricmp( cmd, "rules" ) == 0 ) )
		Cmd_Help_f( ent );
	else if (Q_stricmp (cmd, "gc") == 0)
		Cmd_GameCommand_f( ent );
	else if (Q_stricmp (cmd, "setviewpos") == 0)
		Cmd_SetViewpos_f( ent );
	else if (Q_stricmp (cmd, "ignore") == 0)
		Cmd_Ignore_f( ent );
	else if (Q_stricmp (cmd, "testcmd") == 0)
		Cmd_TestCmd_f( ent );
	else if ( !Q_stricmp(cmd, "toptimes") || !Q_stricmp( cmd, "fastcaps" ) )
		Cmd_TopTimes_f( ent );
	else if (!Q_stricmp(cmd, "topaim") || !Q_stricmp(cmd, "topaims"))
		Cmd_TopAim_f(ent);
	else if ( !Q_stricmp( cmd, "race" ) || !Q_stricmp( cmd, "ghost" ) )
		Cmd_Race_f( ent );
	else if ( !Q_stricmp(cmd, "hideRacers") )
		Cmd_RacerVisibility_f(ent, 0);
	else if ( !Q_stricmp( cmd, "showRacers" ) )
		Cmd_RacerVisibility_f( ent, 1 );
	else if ( !Q_stricmp( cmd, "toggleRacers" ) )
		Cmd_RacerVisibility_f( ent, 2 );
	else if (!Q_stricmp(cmd, "hideBots"))
		Cmd_BotVisibility_f(ent, 0);
	else if (!Q_stricmp(cmd, "showBots"))
		Cmd_BotVisibility_f(ent, 1);
	else if (!Q_stricmp(cmd, "toggleBots"))
		Cmd_BotVisibility_f(ent, 2);
	else if ( !Q_stricmp(cmd, "amtelemark") || !Q_stricmp( cmd, "telemark" ) )
		Cmd_Amtelemark_f( ent );
	else if ( !Q_stricmp(cmd, "amtele") || !Q_stricmp( cmd, "tele" ) )
		Cmd_Amtele_f( ent );
	else if (!Q_stricmp(cmd, "rps"))
		Cmd_RPS_f(ent);
	else if ( !Q_stricmp( cmd, "amautospeed" ) )
		Cmd_AmAutoSpeed_f( ent );
	else if ( !Q_stricmp( cmd, "emote" ) )
		Cmd_Emote_f( ent );
#ifdef NEWMOD_SUPPORT
	else if ( Q_stricmp( cmd, "svauth" ) == 0 && ent->client->sess.auth > PENDING && ent->client->sess.auth < AUTHENTICATED )
		Cmd_Svauth_f( ent );
	else if ( !Q_stricmp( cmd, "vchat" ) )
		Cmd_Vchat_f( ent );
	else if ( !Q_stricmp( cmd, "vchatdl" ) )
		Cmd_VchatDl_f( ent );
	else if (!Q_stricmp(cmd, "vchl"))
		Cmd_VchatList_f(ent);
#endif
		
	//for convenient powerduel testing in release
	else if (Q_stricmp(cmd, "killother") == 0 && CheatsOk( ent ))
	{
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			int entNum = 0;

			trap_Argv( 1, sArg, sizeof( sArg ) );

			entNum = G_ClientNumFromNetname(sArg);

			if (entNum >= 0 && entNum < MAX_GENTITIES)
			{
				gentity_t *kEnt = &g_entities[entNum];

				if (kEnt->inuse && kEnt->client)
				{
					kEnt->flags &= ~FL_GODMODE;
					kEnt->client->ps.stats[STAT_HEALTH] = kEnt->health = -999;
					player_die (kEnt, kEnt, kEnt, 100000, MOD_SUICIDE);
				}
			}
		}
	}
#ifdef _DEBUG
	else if (!Q_stricmp(cmd, "ents")) {
	char buf[8192] = { 0 };
		for (int i = 0; i < MAX_GENTITIES; i++) {
			gentity_t *printEnt = &g_entities[i];
			if (printEnt->inuse && !printEnt->item && Q_stricmpn(printEnt->classname, "target_", 7) && Q_stricmpn(printEnt->classname, "trigger_", 8) &&
				Q_stricmpn(printEnt->classname, "team_", 5) && Q_stricmpn(printEnt->classname, "info_", 5)) {
				Q_strcat(buf, sizeof(buf), va("%d: %s svFlags: %d (%x)\n", i, printEnt->classname, printEnt->r.svFlags, printEnt->r.svFlags));
			}
		}
		if (buf[0])
			PrintIngame(ent - g_entities, buf);
	}
	else if (!Q_stricmp(cmd, "displacement")) {
		if (ent->client->stats)
			PrintIngame(ent - g_entities, "Displacement: %.3f\n", ent->client->stats->displacement);
	}
	else if (Q_stricmp(cmd, "relax") == 0 && CheatsOk( ent ))
	{
		if (ent->client->ps.eFlags & EF_RAG)
		{
			ent->client->ps.eFlags &= ~EF_RAG;
		}
		else
		{
			ent->client->ps.eFlags |= EF_RAG;
		}
	}
	else if (Q_stricmp(cmd, "holdme") == 0 && CheatsOk( ent ))
	{
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			int entNum = 0;

			trap_Argv( 1, sArg, sizeof( sArg ) );

			entNum = atoi(sArg);

			if (entNum >= 0 &&
				entNum < MAX_GENTITIES)
			{
				gentity_t *grabber = &g_entities[entNum];

				if (grabber->inuse && grabber->client && grabber->ghoul2)
				{
					if (!grabber->s.number)
					{ //switch cl 0 and entitynum_none, so we can operate on the "if non-0" concept
						ent->client->ps.ragAttach = ENTITYNUM_NONE;
					}
					else
					{
						ent->client->ps.ragAttach = grabber->s.number;
					}
				}
			}
		}
		else
		{
			ent->client->ps.ragAttach = 0;
		}
	}
	else if (Q_stricmp(cmd, "limb_break") == 0 && CheatsOk( ent ))
	{
		if (trap_Argc() > 1)
		{
			char sArg[MAX_STRING_CHARS];
			int breakLimb = 0;

			trap_Argv( 1, sArg, sizeof( sArg ) );
			if (!Q_stricmp(sArg, "right"))
			{
				breakLimb = BROKENLIMB_RARM;
			}
			else if (!Q_stricmp(sArg, "left"))
			{
				breakLimb = BROKENLIMB_LARM;
			}

			G_BreakArm(ent, breakLimb);
		}
	}
	else if (Q_stricmp(cmd, "headexplodey") == 0 && CheatsOk( ent ))
	{
		Cmd_Kill_f (ent);
		if (ent->health < 1)
		{
			DismembermentTest(ent);
		}
	}
	else if (Q_stricmp(cmd, "debugstupidthing") == 0 && CheatsOk( ent ))
	{
		int i = 0;
		gentity_t *blah;
		while (i < MAX_GENTITIES)
		{
			blah = &g_entities[i];
			if (blah->inuse && blah->classname && blah->classname[0] && !Q_stricmp(blah->classname, "NPC_Vehicle"))
			{
				Com_Printf("Found it.\n");
			}
			i++;
		}
	}
	else if (Q_stricmp(cmd, "arbitraryprint") == 0 && CheatsOk( ent ))
	{
		trap_SendServerCommand( -1, va("cp \"Blah blah blah\n\""));
	}
	else if (Q_stricmp(cmd, "handcut") == 0 && CheatsOk( ent ))
	{
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		if (trap_Argc() > 1)
		{
			trap_Argv( 1, sarg, sizeof( sarg ) );

			if (sarg[0])
			{
				bCl = atoi(sarg);

				if (bCl >= 0 && bCl < MAX_GENTITIES)
				{
					gentity_t *hEnt = &g_entities[bCl];

					if (hEnt->client)
					{
						if (hEnt->health > 0)
						{
							gGAvoidDismember = 1;
							hEnt->flags &= ~FL_GODMODE;
							hEnt->client->ps.stats[STAT_HEALTH] = hEnt->health = -999;
							player_die (hEnt, hEnt, hEnt, 100000, MOD_SUICIDE);
						}
						gGAvoidDismember = 2;
						G_CheckForDismemberment(hEnt, ent, hEnt->client->ps.origin, 999, hEnt->client->ps.legsAnim, qfalse);
						gGAvoidDismember = 0;
					}
				}
			}
		}
	}
	else if (Q_stricmp(cmd, "loveandpeace") == 0 && CheatsOk( ent ))
	{
		trace_t tr;
		vec3_t fPos;

		AngleVectors(ent->client->ps.viewangles, fPos, 0, 0);

		fPos[0] = ent->client->ps.origin[0] + fPos[0]*40;
		fPos[1] = ent->client->ps.origin[1] + fPos[1]*40;
		fPos[2] = ent->client->ps.origin[2] + fPos[2]*40;

		trap_Trace(&tr, ent->client->ps.origin, 0, 0, fPos, ent->s.number, ent->clipmask);

		if (tr.entityNum < MAX_CLIENTS && tr.entityNum != ent->s.number)
		{

			gentity_t *other = &g_entities[tr.entityNum];

			if (other && other->inuse && other->client)
			{
				vec3_t entDir;
				vec3_t otherDir;
				vec3_t entAngles;
				vec3_t otherAngles;

				if (ent->client->ps.weapon == WP_SABER && !ent->client->ps.saberHolstered)
				{
					Cmd_ToggleSaber_f(ent);
				}

				if (other->client->ps.weapon == WP_SABER && !other->client->ps.saberHolstered)
				{
					Cmd_ToggleSaber_f(other);
				}

				if ((ent->client->ps.weapon != WP_SABER || ent->client->ps.saberHolstered) &&
					(other->client->ps.weapon != WP_SABER || other->client->ps.saberHolstered))
				{
					VectorSubtract( other->client->ps.origin, ent->client->ps.origin, otherDir );
					VectorCopy( ent->client->ps.viewangles, entAngles );
					entAngles[YAW] = vectoyaw( otherDir );
					SetClientViewAngle( ent, entAngles );

					StandardSetBodyAnim(ent, /*BOTH_KISSER1LOOP*/BOTH_STAND1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
					ent->client->ps.saberMove = LS_NONE;
					ent->client->ps.saberBlocked = 0;
					ent->client->ps.saberBlocking = 0;

					VectorSubtract( ent->client->ps.origin, other->client->ps.origin, entDir );
					VectorCopy( other->client->ps.viewangles, otherAngles );
					otherAngles[YAW] = vectoyaw( entDir );
					SetClientViewAngle( other, otherAngles );

					StandardSetBodyAnim(other, /*BOTH_KISSEE1LOOP*/BOTH_STAND1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD|SETANIM_FLAG_HOLDLESS);
					other->client->ps.saberMove = LS_NONE;
					other->client->ps.saberBlocked = 0;
					other->client->ps.saberBlocking = 0;
				}
			}
		}
	}
#endif
	else if (Q_stricmp(cmd, "thedestroyer") == 0 && CheatsOk( ent ) && ent && ent->client && ent->client->ps.saberHolstered && ent->client->ps.weapon == WP_SABER)
	{
		Cmd_ToggleSaber_f(ent);

		if (!ent->client->ps.saberHolstered)
		{
		}
	}
	//begin bot debug cmds
	else if (Q_stricmp(cmd, "debugBMove_Forward") == 0 && CheatsOk(ent))
	{
		int arg = 4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, arg, -1, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Back") == 0 && CheatsOk(ent))
	{
		int arg = -4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, arg, -1, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Right") == 0 && CheatsOk(ent))
	{
		int arg = 4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, -1, arg, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Left") == 0 && CheatsOk(ent))
	{
		int arg = -4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, -1, arg, -1);
	}
	else if (Q_stricmp(cmd, "debugBMove_Up") == 0 && CheatsOk(ent))
	{
		int arg = 4000;
		int bCl = 0;
		char sarg[MAX_STRING_CHARS];

		assert(trap_Argc() > 1);
		trap_Argv( 1, sarg, sizeof( sarg ) );

		assert(sarg[0]);
		bCl = atoi(sarg);
		Bot_SetForcedMovement(bCl, -1, -1, arg);
	}
	//end bot debug cmds
#ifndef FINAL_BUILD
	else if (Q_stricmp(cmd, "debugSetSaberMove") == 0)
	{
		Cmd_DebugSetSaberMove_f(ent);
	}
	else if (Q_stricmp(cmd, "debugSetBodyAnim") == 0)
	{
		Cmd_DebugSetBodyAnim_f(ent, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD);
	}
	else if (Q_stricmp(cmd, "debugDismemberment") == 0)
	{
		Cmd_Kill_f (ent);
		if (ent->health < 1)
		{
			char	arg[MAX_STRING_CHARS];
			int		iArg = 0;

			if (trap_Argc() > 1)
			{
				trap_Argv( 1, arg, sizeof( arg ) );

				if (arg[0])
				{
					iArg = atoi(arg);
				}
			}

			DismembermentByNum(ent, iArg);
		}
	}
	else if (Q_stricmp(cmd, "debugDropSaber") == 0)
	{
		if (ent->client->ps.weapon == WP_SABER &&
			ent->client->ps.saberEntityNum &&
			!ent->client->ps.saberInFlight)
		{
			saberKnockOutOfHand(&g_entities[ent->client->ps.saberEntityNum], ent, vec3_origin);
		}
	}
	else if (Q_stricmp(cmd, "debugKnockMeDown") == 0)
	{
		if (BG_KnockDownable(&ent->client->ps))
		{
			ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
			ent->client->ps.forceDodgeAnim = 0;
			if (trap_Argc() > 1)
			{
				ent->client->ps.forceHandExtendTime = level.time + 1100;
				ent->client->ps.quickerGetup = qfalse;
			}
			else
			{
				ent->client->ps.forceHandExtendTime = level.time + 700;
				ent->client->ps.quickerGetup = qtrue;
			}
		}
	}
	else if (Q_stricmp(cmd, "debugSaberSwitch") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client)
		{
			Cmd_ToggleSaber_f(targ);
		}
	}
	else if (Q_stricmp(cmd, "debugIKGrab") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client && ent->s.number != targ->s.number)
		{
			targ->client->ps.heldByClient = ent->s.number+1;
		}
	}
	else if (Q_stricmp(cmd, "debugIKBeGrabbedBy") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client && ent->s.number != targ->s.number)
		{
			ent->client->ps.heldByClient = targ->s.number+1;
		}
	}
	else if (Q_stricmp(cmd, "debugIKRelease") == 0)
	{
		gentity_t *targ = NULL;

		if (trap_Argc() > 1)
		{
			char	arg[MAX_STRING_CHARS];

			trap_Argv( 1, arg, sizeof( arg ) );

			if (arg[0])
			{
				int x = atoi(arg);
				
				if (x >= 0 && x < MAX_CLIENTS)
				{
					targ = &g_entities[x];
				}
			}
		}

		if (targ && targ->inuse && targ->client)
		{
			targ->client->ps.heldByClient = 0;
		}
	}
	else if (Q_stricmp(cmd, "debugThrow") == 0)
	{
		trace_t tr;
		vec3_t tTo, fwd;

		if (ent->client->ps.weaponTime > 0 || ent->client->ps.forceHandExtend != HANDEXTEND_NONE ||
			ent->client->ps.groundEntityNum == ENTITYNUM_NONE || ent->health < 1)
		{
			return;
		}

		AngleVectors(ent->client->ps.viewangles, fwd, 0, 0);
		tTo[0] = ent->client->ps.origin[0] + fwd[0]*32;
		tTo[1] = ent->client->ps.origin[1] + fwd[1]*32;
		tTo[2] = ent->client->ps.origin[2] + fwd[2]*32;

		trap_Trace(&tr, ent->client->ps.origin, 0, 0, tTo, ent->s.number, MASK_PLAYERSOLID);

		if (tr.fraction != 1)
		{
			gentity_t *other = &g_entities[tr.entityNum];

			if (other->inuse && other->client && other->client->ps.forceHandExtend == HANDEXTEND_NONE &&
				other->client->ps.groundEntityNum != ENTITYNUM_NONE && other->health > 0 &&
				(int)ent->client->ps.origin[2] == (int)other->client->ps.origin[2])
			{
				float pDif = 40.0f;
				vec3_t entAngles, entDir;
				vec3_t otherAngles, otherDir;
				vec3_t intendedOrigin;
				vec3_t boltOrg, pBoltOrg;
				vec3_t tAngles, vDif;
				vec3_t fwd, right;
				trace_t tr;
				trace_t tr2;

				VectorSubtract( other->client->ps.origin, ent->client->ps.origin, otherDir );
				VectorCopy( ent->client->ps.viewangles, entAngles );
				entAngles[YAW] = vectoyaw( otherDir );
				SetClientViewAngle( ent, entAngles );

				ent->client->ps.forceHandExtend = HANDEXTEND_PRETHROW;
				ent->client->ps.forceHandExtendTime = level.time + 5000;

				ent->client->throwingIndex = other->s.number;
				ent->client->doingThrow = level.time + 5000;
				ent->client->beingThrown = 0;

				VectorSubtract( ent->client->ps.origin, other->client->ps.origin, entDir );
				VectorCopy( other->client->ps.viewangles, otherAngles );
				otherAngles[YAW] = vectoyaw( entDir );
				SetClientViewAngle( other, otherAngles );

				other->client->ps.forceHandExtend = HANDEXTEND_PRETHROWN;
				other->client->ps.forceHandExtendTime = level.time + 5000;

				other->client->throwingIndex = ent->s.number;
				other->client->beingThrown = level.time + 5000;
				other->client->doingThrow = 0;

				//Doing this now at a stage in the throw, isntead of initially.

				G_EntitySound( other, CHAN_VOICE, G_SoundIndex("*pain100.wav") );
				G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*jump1.wav") );
				G_Sound(other, CHAN_AUTO, G_SoundIndex( "sound/movers/objects/objectHit.wav" ));

				//see if we can move to be next to the hand.. if it's not clear, break the throw.
				VectorClear(tAngles);
				tAngles[YAW] = ent->client->ps.viewangles[YAW];
				VectorCopy(ent->client->ps.origin, pBoltOrg);
				AngleVectors(tAngles, fwd, right, 0);
				boltOrg[0] = pBoltOrg[0] + fwd[0]*8 + right[0]*pDif;
				boltOrg[1] = pBoltOrg[1] + fwd[1]*8 + right[1]*pDif;
				boltOrg[2] = pBoltOrg[2];

				VectorSubtract(boltOrg, pBoltOrg, vDif);
				VectorNormalize(vDif);

				VectorClear(other->client->ps.velocity);
				intendedOrigin[0] = pBoltOrg[0] + vDif[0]*pDif;
				intendedOrigin[1] = pBoltOrg[1] + vDif[1]*pDif;
				intendedOrigin[2] = other->client->ps.origin[2];

				trap_Trace(&tr, intendedOrigin, other->r.mins, other->r.maxs, intendedOrigin, other->s.number, other->clipmask);
				trap_Trace(&tr2, ent->client->ps.origin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, CONTENTS_SOLID);

				if (tr.fraction == 1.0 && !tr.startsolid && tr2.fraction == 1.0 && !tr2.startsolid)
				{
					VectorCopy(intendedOrigin, other->client->ps.origin);
				}
				else
				{ //if the guy can't be put here then it's time to break the throw off.
					vec3_t oppDir;
					int strength = 4;

					other->client->ps.heldByClient = 0;
					other->client->beingThrown = 0;
					ent->client->doingThrow = 0;

					ent->client->ps.forceHandExtend = HANDEXTEND_NONE;
					G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*pain25.wav") );

					other->client->ps.forceHandExtend = HANDEXTEND_NONE;
					VectorSubtract(other->client->ps.origin, ent->client->ps.origin, oppDir);
					VectorNormalize(oppDir);
					other->client->ps.velocity[0] = oppDir[0]*(strength*40);
					other->client->ps.velocity[1] = oppDir[1]*(strength*40);
					other->client->ps.velocity[2] = 150;

					VectorSubtract(ent->client->ps.origin, other->client->ps.origin, oppDir);
					VectorNormalize(oppDir);
					ent->client->ps.velocity[0] = oppDir[0]*(strength*40);
					ent->client->ps.velocity[1] = oppDir[1]*(strength*40);
					ent->client->ps.velocity[2] = 150;
				}
			}
		}
	}
#endif
#ifdef VM_MEMALLOC_DEBUG
	else if (Q_stricmp(cmd, "debugTestAlloc") == 0)
	{ //rww - small routine to stress the malloc trap stuff and make sure nothing bad is happening.
		char *blah;
		int i = 1;
		int x;

		//stress it. Yes, this will take a while. If it doesn't explode miserably in the process.
		while (i < 32768)
		{
			x = 0;

			trap_TrueMalloc((void **)&blah, i);
			if (!blah)
			{ //pointer is returned null if allocation failed
				trap_SendServerCommand( -1, va("print \"Failed to alloc at %i!\n\"", i));
				break;
			}
			while (x < i)
			{ //fill the allocated memory up to the edge
				if (x+1 == i)
				{
					blah[x] = 0;
				}
				else
				{
					blah[x] = 'A';
				}
				x++;
			}
			trap_TrueFree((void **)&blah);
			if (blah)
			{ //should be nullified in the engine after being freed
				trap_SendServerCommand( -1, va("print \"Failed to free at %i!\n\"", i));
				break;
			}

			i++;
		}

		trap_SendServerCommand( -1, "print \"Finished allocation test\n\"");
	}
#endif
#ifndef FINAL_BUILD
	else if (Q_stricmp(cmd, "debugShipDamage") == 0)
	{
		char	arg[MAX_STRING_CHARS];
		char	arg2[MAX_STRING_CHARS];
		int		shipSurf, damageLevel;

		trap_Argv( 1, arg, sizeof( arg ) );
		trap_Argv( 2, arg2, sizeof( arg2 ) );
		shipSurf = SHIPSURF_FRONT+atoi(arg);
		damageLevel = atoi(arg2);

		G_SetVehDamageFlags( &g_entities[ent->s.m_iVehicleNum], shipSurf, damageLevel );
	}
#endif
	else
	{
		if (Q_stricmp(cmd, "addbot") == 0)
		{ //because addbot isn't a recognized command unless you're the server, but it is in the menus regardless
			trap_SendServerCommand( clientNum, va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "ONLY_ADD_BOTS_AS_SERVER")));
		}
		else
		{
			trap_SendServerCommand( clientNum, va("print \"unknown cmd %s\n\"", cmd ) );
		}
	}
}

void G_InitVoteMapsLimit()
{
    memset(g_votedCounts, 0, sizeof(g_votedCounts));
}

