// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"
#include "bg_saga.h"
#include "g_database.h"

teamgame_t teamgame;

void Team_SetFlagStatus( int team, flagStatus_t status );

void Team_InitGame( void ) {
	memset(&teamgame, 0, sizeof teamgame);

	switch( g_gametype.integer ) {
	case GT_CTF:
	case GT_CTY:
		teamgame.redStatus = teamgame.blueStatus = -1; // Invalid to force update
		Team_SetFlagStatus( TEAM_RED, FLAG_ATBASE );
		Team_SetFlagStatus( TEAM_BLUE, FLAG_ATBASE );
		level.redFlagStealTime = 0;
		level.blueFlagStealTime = 0;
		level.redTeamRunFlaghold = 0;
		level.blueTeamRunFlaghold = 0;
		break;
	default:
		break;
	}
}

int OtherTeam(int team) {
	// huge change
	if (team==TEAM_RED)
		return TEAM_BLUE;
	else if (team==TEAM_BLUE)
		return TEAM_RED;
	return team;
}

int WinningTeam(void) {
    if (g_gametype.integer < GT_TEAM)
		return 0;

	if (level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE])
		return 0;

    return (level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE]) ? TEAM_RED : TEAM_BLUE;
}

int LosingTeam(void) {
	return OtherTeam(WinningTeam());
}

const char *TeamName(int team)  {
	if (team==TEAM_RED)
		return "RED";
	else if (team==TEAM_BLUE)
		return "BLUE";
	else if (team==TEAM_SPECTATOR)
		return "SPECTATOR";
	return "FREE";
}

const char *OtherTeamName(int team) {
	if (team==TEAM_RED)
		return "BLUE";
	else if (team==TEAM_BLUE)
		return "RED";
	else if (team==TEAM_SPECTATOR)
		return "SPECTATOR";
	return "FREE";
}

const char *TeamColorString(int team) {
	if (team==TEAM_RED)
		return S_COLOR_RED;
	else if (team==TEAM_BLUE)
		return S_COLOR_BLUE;
	else if (team==TEAM_SPECTATOR)
		return S_COLOR_YELLOW;
	return S_COLOR_WHITE;
}

//Printing messages to players via this method is no longer done, StringEd stuff is client only.


//plIndex used to print pl->client->pers.netname
//teamIndex used to print team name
void PrintCTFMessage(int plIndex, int teamIndex, int ctfMessage)
{
	if (level.intermissiontime && ctfMessage == CTFMESSAGE_FLAG_RETURNED)
		return;

	gentity_t *te;

	if (plIndex == -1)
	{
		plIndex = MAX_CLIENTS+1;
	}
	if (teamIndex == -1)
	{
		teamIndex = 50;
	}

	te = G_TempEntity(vec3_origin, EV_CTFMESSAGE);
	te->r.svFlags |= SVF_BROADCAST;
	te->s.eventParm = ctfMessage;
	te->s.trickedentindex = plIndex;
	if (ctfMessage == CTFMESSAGE_PLAYER_CAPTURED_FLAG)
	{
		if (teamIndex == TEAM_RED)
		{
			te->s.trickedentindex2 = TEAM_BLUE;
		}
		else
		{
			te->s.trickedentindex2 = TEAM_RED;
		}
	}
	else
	{
		te->s.trickedentindex2 = teamIndex;
	}

	if ( ctfMessage == CTFMESSAGE_PLAYER_CAPTURED_FLAG ) {
		// send the capture time as an unused event parameter
		if ( teamIndex == TEAM_RED ) {
			te->s.time = level.redTeamRunFlaghold;
		} else if ( teamIndex == TEAM_BLUE ) {
			te->s.time = level.blueTeamRunFlaghold;
		}

		// send the hp/armor/force of the capturer as unused event parameter
		if (plIndex >= 0 && plIndex < MAX_CLIENTS && g_broadcastCapturerHealthArmorForce.integer) {
			gentity_t *ent = &g_entities[plIndex];
			if (ent->inuse && ent->client && ent->health > 0) {
				int health = Com_Clampi(0, 999, ent->health);
				int armor = Com_Clampi(0, 999, ent->client->ps.stats[STAT_ARMOR]);
				int force = ent->client->ps.fd.forcePowersKnown ? Com_Clampi(0, 100, ent->client->ps.fd.forcePower) : 0;

				uint32_t packed = ((health & 0x3FF) << 22) | ((armor & 0x3FF) << 12) | (force & 0x3FF);
				te->s.emplacedOwner = *(int *)&packed;
			}
		}
	}

	G_ApplyRaceBroadcastsToEvent( NULL, te );
}

/*
==============
AddTeamScore

 used for gametype > GT_TEAM
 for gametype GT_TEAM the level.teamScores is updated in AddScore in g_combat.c
==============
*/
void AddTeamScore(vec3_t origin, int team, int score) {
	gentity_t	*te;

	te = G_TempEntity(origin, EV_GLOBAL_TEAM_SOUND );
	te->r.svFlags |= SVF_BROADCAST;

	if ( team == TEAM_RED ) {
		if ( level.teamScores[ TEAM_RED ] + score == level.teamScores[ TEAM_BLUE ] ) {
			//teams are tied sound
			te->s.eventParm = GTS_TEAMS_ARE_TIED;
		}
		else if ( level.teamScores[ TEAM_RED ] <= level.teamScores[ TEAM_BLUE ] &&
					level.teamScores[ TEAM_RED ] + score > level.teamScores[ TEAM_BLUE ]) {
			// red took the lead sound
			te->s.eventParm = GTS_REDTEAM_TOOK_LEAD;
		}
		else {
			// red scored sound
			te->s.eventParm = GTS_REDTEAM_SCORED;
		}
	}
	else {
		if ( level.teamScores[ TEAM_BLUE ] + score == level.teamScores[ TEAM_RED ] ) {
			//teams are tied sound
			te->s.eventParm = GTS_TEAMS_ARE_TIED;
		}
		else if ( level.teamScores[ TEAM_BLUE ] <= level.teamScores[ TEAM_RED ] &&
					level.teamScores[ TEAM_BLUE ] + score > level.teamScores[ TEAM_RED ]) {
			// blue took the lead sound
			te->s.eventParm = GTS_BLUETEAM_TOOK_LEAD;
		}
		else {
			// blue scored sound
			te->s.eventParm = GTS_BLUETEAM_SCORED;
		}
	}
	level.teamScores[ team ] += score;

	G_ApplyRaceBroadcastsToEvent( NULL, te );
}

/*
==============
OnSameTeam
==============
*/
qboolean OnSameTeam( gentity_t *ent1, gentity_t *ent2 ) {
	if ( !ent1->client || !ent2->client ) {
		return qfalse;
	}

	if (g_gametype.integer == GT_POWERDUEL)
	{
		if (ent1->client->sess.duelTeam == ent2->client->sess.duelTeam)
		{
			return qtrue;
		}

		return qfalse;
	}

	if (g_gametype.integer == GT_SINGLE_PLAYER)
	{
		qboolean ent1IsBot = qfalse;
		qboolean ent2IsBot = qfalse;

		if (ent1->r.svFlags & SVF_BOT)
		{
			ent1IsBot = qtrue;
		}
		if (ent2->r.svFlags & SVF_BOT)
		{
			ent2IsBot = qtrue;
		}

		if ((ent1IsBot && ent2IsBot) || (!ent1IsBot && !ent2IsBot))
		{
			return qtrue;
		}
		return qfalse;
	}

	if ( g_gametype.integer < GT_TEAM ) {
		return qfalse;
	}

	if (ent1->s.eType == ET_NPC &&
		ent1->s.NPC_class == CLASS_VEHICLE &&
		ent1->client &&
		ent1->client->sess.sessionTeam != TEAM_FREE &&
		ent2->client &&
		ent1->client->sess.sessionTeam == ent2->client->sess.sessionTeam)
	{
		return qtrue;
	}
	if (ent2->s.eType == ET_NPC &&
		ent2->s.NPC_class == CLASS_VEHICLE &&
		ent2->client &&
		ent2->client->sess.sessionTeam != TEAM_FREE &&
		ent1->client &&
		ent2->client->sess.sessionTeam == ent1->client->sess.sessionTeam)
	{
		return qtrue;
	}

	if (ent1->client->sess.sessionTeam == TEAM_FREE &&
		ent2->client->sess.sessionTeam == TEAM_FREE &&
		ent1->s.eType == ET_NPC &&
		ent2->s.eType == ET_NPC)
	{ //NPCs don't do normal team rules
		return qfalse;
	}

	if (ent1->s.eType == ET_NPC && ent2->s.eType == ET_PLAYER)
	{
		return qfalse;
	}
	else if (ent1->s.eType == ET_PLAYER && ent2->s.eType == ET_NPC)
	{
		return qfalse;
	}

	if ( (g_gametype.integer == GT_SIEGE) && (ent1->client->sess.siegeDesiredTeam != ent2->client->sess.siegeDesiredTeam) )
	{
		return qfalse;
	}

	if ( ent1->client->sess.sessionTeam == ent2->client->sess.sessionTeam ) 
	{
		return qtrue;
	}	

	return qfalse;
}


static char ctfFlagStatusRemap[] = { '0', '1', '*', '*', '2' };

void Team_SetFlagStatus( int team, flagStatus_t status ) {
	qboolean modified = qfalse;

	switch( team ) {
	case TEAM_RED:	// CTF
		if( teamgame.redStatus != status ) {
			teamgame.redStatus = status;
			modified = qtrue;
		}
		break;

	case TEAM_BLUE:	// CTF
		if( teamgame.blueStatus != status ) {
			teamgame.blueStatus = status;
			modified = qtrue;
		}
		break;

	case TEAM_FREE:	// One Flag CTF
		if( teamgame.flagStatus != status ) {
			teamgame.flagStatus = status;
			modified = qtrue;
		}
		break;
	}

	if( modified ) {
		char st[4];

		if( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY ) {
			st[0] = ctfFlagStatusRemap[teamgame.redStatus];
			st[1] = ctfFlagStatusRemap[teamgame.blueStatus];
			st[2] = 0;
		}

		trap_SetConfigstring( CS_FLAGSTATUS, st );
	}
}

void Team_CheckDroppedItem( gentity_t *dropped ) {
	if ( dropped->item->giTag == PW_REDFLAG ) {
		Team_SetFlagStatus( TEAM_RED, FLAG_DROPPED );
	}
	else if( dropped->item->giTag == PW_BLUEFLAG ) {
		Team_SetFlagStatus( TEAM_BLUE, FLAG_DROPPED );
	}
	else if( dropped->item->giTag == PW_NEUTRALFLAG ) {
		Team_SetFlagStatus( TEAM_FREE, FLAG_DROPPED );
	}
}

/*
================
Team_ForceGesture
================
*/
void Team_ForceGesture(int team) {
	int i;
	gentity_t *ent;

	for (i = 0; i < MAX_CLIENTS; i++) {
		ent = &g_entities[i];
		if (!ent->inuse)
			continue;
		if (!ent->client)
			continue;
		if (ent->client->sess.sessionTeam != team)
			continue;
		//
		ent->flags |= FL_FORCE_GESTURE;
	}
}

/*
================
Team_FragBonuses

Calculate the bonuses for flag defense, flag carrier defense, etc.
Note that bonuses are not cumulative.  You get one, they are in importance
order.
================
*/
void Team_FragBonuses(gentity_t *targ, gentity_t *inflictor, gentity_t *attacker)
{
	int i;
	gentity_t *ent;
	int flag_pw, enemy_flag_pw;
	int otherteam;
	int tokens;
	gentity_t *flag, *carrier = NULL;
	char *c;
	vec3_t v1, v2;
	int team;

	// no bonus for fragging yourself or team mates
	if (!targ->client || !attacker->client || targ == attacker || OnSameTeam(targ, attacker))
		return;

	team = targ->client->sess.sessionTeam;
	otherteam = OtherTeam(targ->client->sess.sessionTeam);
	if (otherteam < 0)
		return; // whoever died isn't on a team

	// same team, if the flag at base, check to he has the enemy flag
	if (team == TEAM_RED) {
		flag_pw = PW_REDFLAG;
		enemy_flag_pw = PW_BLUEFLAG;
	} else {
		flag_pw = PW_BLUEFLAG;
		enemy_flag_pw = PW_REDFLAG;
	}

	// did the attacker frag the flag carrier?
	tokens = 0;
	if (targ->client->ps.powerups[enemy_flag_pw]) {
		attacker->client->pers.teamState.lastfraggedcarrier = level.time;
		AddScore(attacker, targ->r.currentOrigin, CTF_FRAG_CARRIER_BONUS);

		attacker->client->stats->fcKills++;
		PrintCTFMessage(attacker->s.number, team, CTFMESSAGE_FRAGGED_FLAG_CARRIER);

		if (attacker->client && attacker->client->sess.sessionTeam == TEAM_RED && targ->client->sess.sessionTeam == TEAM_BLUE)
			level.redPlayerWhoKilledBlueCarrierOfRedFlag = attacker->client->stats;
		else if (attacker->client && attacker->client->sess.sessionTeam == TEAM_BLUE && targ->client->sess.sessionTeam == TEAM_RED)
			level.bluePlayerWhoKilledRedCarrierOfBlueFlag = attacker->client->stats;

		// the target had the flag, clear the hurt carrier
		// field on the other team
		for (i = 0; i < g_maxclients.integer; i++) {
			ent = g_entities + i;
			if (ent->inuse && ent->client->sess.sessionTeam == otherteam)
				ent->client->pers.teamState.lasthurtcarrier = 0;
		}
		return;
	}

	// did the attacker frag a head carrier? other->client->ps.generic1
	if (tokens) {
		attacker->client->pers.teamState.lastfraggedcarrier = level.time;
		AddScore(attacker, targ->r.currentOrigin, CTF_FRAG_CARRIER_BONUS * tokens * tokens);

		//*CHANGE 31* longest flag holding time keeping track
		const int thisFlaghold = G_GetAccurateTimerOnTrigger( &targ->client->pers.teamState.flagsince, targ, NULL );

		targ->client->stats->totalFlagHold += thisFlaghold;

		if ( thisFlaghold > targ->client->stats->longestFlagHold )
			targ->client->stats->longestFlagHold = thisFlaghold;

		if ( targ->client->ps.powerups[PW_REDFLAG] ) {
			// carried the red flag, so blue team
			level.blueTeamRunFlaghold += thisFlaghold;
		} else if ( targ->client->ps.powerups[PW_BLUEFLAG] ) {
			// carried the blue flag, so red team
			level.redTeamRunFlaghold += thisFlaghold;
		}

		// the target had the flag, clear the hurt carrier
		// field on the other team
		for (i = 0; i < g_maxclients.integer; i++) {
			ent = g_entities + i;
			if (ent->inuse && ent->client->sess.sessionTeam == otherteam)
				ent->client->pers.teamState.lasthurtcarrier = 0;
		}
		return;
	}

	if (targ->client->pers.teamState.lasthurtcarrier &&
		level.time - targ->client->pers.teamState.lasthurtcarrier < CTF_CARRIER_DANGER_PROTECT_TIMEOUT &&
		!attacker->client->ps.powerups[flag_pw]) {
		// attacker is on the same team as the flag carrier and
		// fragged a guy who hurt our flag carrier
		AddScore(attacker, targ->r.currentOrigin, CTF_CARRIER_DANGER_PROTECT_BONUS);

		attacker->client->stats->defends++;
		targ->client->pers.teamState.lasthurtcarrier = 0;

		attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
		team = attacker->client->sess.sessionTeam;
		attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

		return;
	}

	if (targ->client->pers.teamState.lasthurtcarrier &&
		level.time - targ->client->pers.teamState.lasthurtcarrier < CTF_CARRIER_DANGER_PROTECT_TIMEOUT) {
		// attacker is on the same team as the skull carrier and
		AddScore(attacker, targ->r.currentOrigin, CTF_CARRIER_DANGER_PROTECT_BONUS);

		attacker->client->stats->defends++;
		targ->client->pers.teamState.lasthurtcarrier = 0;

		attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
		team = attacker->client->sess.sessionTeam;
		attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

		return;
	}

	// flag and flag carrier area defense bonuses

	// we have to find the flag and carrier entities

	// find the flag
	switch (attacker->client->sess.sessionTeam) {
	case TEAM_RED:
		c = "team_CTF_redflag";
		break;
	case TEAM_BLUE:
		c = "team_CTF_blueflag";
		break;		
	default:
		return;
	}
	// find attacker's team's flag carrier
	for (i = 0; i < g_maxclients.integer; i++) {
		carrier = g_entities + i;
		if (carrier->inuse && carrier->client->ps.powerups[flag_pw])
			break;
		carrier = NULL;
	}
	flag = NULL;
	while ((flag = G_Find (flag, FOFS(classname), c)) != NULL) {
		if (!(flag->flags & FL_DROPPED_ITEM))
			break;
	}

	if (!flag)
		return; // can't find attacker's flag

	// ok we have the attackers flag and a pointer to the carrier

	// check to see if we are defending the base's flag
	VectorSubtract(targ->r.currentOrigin, flag->r.currentOrigin, v1);
	VectorSubtract(attacker->r.currentOrigin, flag->r.currentOrigin, v2);

	if ( ( ( VectorLength(v1) < CTF_TARGET_PROTECT_RADIUS &&
		trap_InPVS(flag->r.currentOrigin, targ->r.currentOrigin ) ) ||
		( VectorLength(v2) < CTF_TARGET_PROTECT_RADIUS &&
		trap_InPVS(flag->r.currentOrigin, attacker->r.currentOrigin ) ) ) &&
		attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam) {

		// we defended the base flag
		AddScore(attacker, targ->r.currentOrigin, CTF_FLAG_DEFENSE_BONUS);
		attacker->client->stats->defends++;

		attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
		attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

		return;
	}

	if (carrier && carrier != attacker) {
		VectorSubtract(targ->r.currentOrigin, carrier->r.currentOrigin, v1);
		VectorSubtract(attacker->r.currentOrigin, carrier->r.currentOrigin, v2);

		if ( ( ( VectorLength(v1) < CTF_ATTACKER_PROTECT_RADIUS &&
			trap_InPVS(carrier->r.currentOrigin, targ->r.currentOrigin ) ) ||
			( VectorLength(v2) < CTF_ATTACKER_PROTECT_RADIUS &&
				trap_InPVS(carrier->r.currentOrigin, attacker->r.currentOrigin ) ) ) &&
			attacker->client->sess.sessionTeam != targ->client->sess.sessionTeam) {
			AddScore(attacker, targ->r.currentOrigin, CTF_CARRIER_PROTECT_BONUS);
			attacker->client->stats->defends++;

			attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
			attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;

			return;
		}
	}
}

/*
================
Team_CheckHurtCarrier

Check to see if attacker hurt the flag carrier.  Needed when handing out bonuses for assistance to flag
carrier defense.
================
*/
void Team_CheckHurtCarrier(gentity_t *targ, gentity_t *attacker)
{
	int flag_pw;

	if (!targ->client || !attacker->client)
		return;

	if (targ->client->sess.sessionTeam == TEAM_RED)
		flag_pw = PW_BLUEFLAG;
	else
		flag_pw = PW_REDFLAG;

	// flags
	if (targ->client->ps.powerups[flag_pw] &&
		targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam)
		attacker->client->pers.teamState.lasthurtcarrier = level.time;

	// skulls
	if (targ->client->ps.generic1 &&
		targ->client->sess.sessionTeam != attacker->client->sess.sessionTeam)
		attacker->client->pers.teamState.lasthurtcarrier = level.time;
}


gentity_t *Team_ResetFlag( int team ) {
	char *c;
	gentity_t *ent, *rent = NULL;

	switch (team) {
	case TEAM_RED:
		c = "team_CTF_redflag";
		break;
	case TEAM_BLUE:
		c = "team_CTF_blueflag";
		break;
	case TEAM_FREE:
		c = "team_CTF_neutralflag";
		break;
	default:
		return NULL;
	}

	ent = NULL;
	while ((ent = G_Find (ent, FOFS(classname), c)) != NULL) {
		if (ent->flags & FL_DROPPED_ITEM)
			G_FreeEntity(ent);
		else {
			rent = ent;
			RespawnItem(ent);
		}
	}

	// team is the color of the flag being reset, so reset the flaghold of the opposite team
	if ( team == TEAM_RED ) {
		level.redFlagStealTime = 0;
		level.blueTeamRunFlaghold = 0;
		level.redPlayerWhoKilledBlueCarrierOfRedFlag = NULL;
	} else {
		level.blueFlagStealTime = 0;
		level.redTeamRunFlaghold = 0;
		level.bluePlayerWhoKilledRedCarrierOfBlueFlag = NULL;
	}

	// reset allied fc kills
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *teammate = &g_entities[i];
		if (!teammate->inuse || !teammate->client || teammate->client->sess.sessionTeam != OtherTeam(team))
			continue;
		teammate->client->pers.killedAlliedFlagCarrier = qfalse;
	}

	Team_SetFlagStatus( team, FLAG_ATBASE );

	return rent;
}

void Team_ResetFlags( void ) {
	if( g_gametype.integer == GT_CTF || g_gametype.integer == GT_CTY ) {
		Team_ResetFlag( TEAM_RED );
		Team_ResetFlag( TEAM_BLUE );
	}
}

void Team_ReturnFlagSound( gentity_t *ent, int team ) {
	if (level.intermissiontime)
		return;

	gentity_t	*te;

	if (ent == NULL) {
		G_LogPrintf ("Warning:  NULL passed to Team_ReturnFlagSound\n");
		return;
	}

	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND );
	if( team == TEAM_BLUE ) {
		te->s.eventParm = GTS_RED_RETURN;
	}
	else {
		te->s.eventParm = GTS_BLUE_RETURN;
	}
	te->r.svFlags |= SVF_BROADCAST;

	G_ApplyRaceBroadcastsToEvent( NULL, te );
}

void Team_TakeFlagSound( gentity_t *ent, int team ) {
	gentity_t	*te;

	if (ent == NULL) {
		G_Printf ("Warning:  NULL passed to Team_TakeFlagSound\n");
		return;
	}

	// only play sound when the flag was at the base
	// or not picked up the last 10 seconds
	switch(team) {
		case TEAM_RED:
			if( teamgame.blueStatus != FLAG_ATBASE ) {
				if (teamgame.blueTakenTime > level.time - 10000)
					return;
			}
			teamgame.blueTakenTime = level.time;
			break;

		case TEAM_BLUE:	// CTF
			if( teamgame.redStatus != FLAG_ATBASE ) {
				if (teamgame.redTakenTime > level.time - 10000)
					return;
			}
			teamgame.redTakenTime = level.time;
			break;
	}

	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND );
	if( team == TEAM_BLUE ) {
		te->s.eventParm = GTS_RED_TAKEN;
	}
	else {
		te->s.eventParm = GTS_BLUE_TAKEN;
	}
	te->r.svFlags |= SVF_BROADCAST;

	G_ApplyRaceBroadcastsToEvent( NULL, te );
}

void Team_CaptureFlagSound( gentity_t *ent, int team ) {
	gentity_t	*te;

	if (ent == NULL) {
		G_Printf ("Warning:  NULL passed to Team_CaptureFlagSound\n");
		return;
	}

	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_TEAM_SOUND );
	if( team == TEAM_BLUE ) {
		te->s.eventParm = GTS_BLUE_CAPTURE;
	}
	else {
		te->s.eventParm = GTS_RED_CAPTURE;
	}
	te->r.svFlags |= SVF_BROADCAST;

	G_ApplyRaceBroadcastsToEvent( NULL, te );
}

void Team_ReturnFlag( int team ) {
	if (team == TEAM_RED) {
		if (level.redPlayerWhoKilledBlueCarrierOfRedFlag) {
			level.redPlayerWhoKilledBlueCarrierOfRedFlag->fcKillsResultingInRets++;
			level.redPlayerWhoKilledBlueCarrierOfRedFlag = NULL;
		}
	}
	else if (team == TEAM_BLUE) {
		if (level.bluePlayerWhoKilledRedCarrierOfBlueFlag) {
			level.bluePlayerWhoKilledRedCarrierOfBlueFlag->fcKillsResultingInRets++;
			level.bluePlayerWhoKilledRedCarrierOfBlueFlag = NULL;
		}
	}

	Team_ReturnFlagSound(Team_ResetFlag(team), team);
	if( team == TEAM_FREE ) {
	}
	else { //flag should always have team in normal CTF
		PrintCTFMessage(-1, team, CTFMESSAGE_FLAG_RETURNED);
	}
}

// this is only called if a flag is dropped into a pit and overboarding is disabled
void Team_FreeEntity( gentity_t *ent ) {
	if( ent->item->giTag == PW_REDFLAG ) {
		Team_ReturnFlag( TEAM_RED );
		if (level.redPlayerWhoKilledBlueCarrierOfRedFlag) {
			level.redPlayerWhoKilledBlueCarrierOfRedFlag->fcKillsResultingInRets++;
			level.redPlayerWhoKilledBlueCarrierOfRedFlag = NULL;
		}
	}
	else if( ent->item->giTag == PW_BLUEFLAG ) {
		Team_ReturnFlag( TEAM_BLUE );
		if (level.bluePlayerWhoKilledRedCarrierOfBlueFlag) {
			level.bluePlayerWhoKilledRedCarrierOfBlueFlag->fcKillsResultingInRets++;
			level.bluePlayerWhoKilledRedCarrierOfBlueFlag = NULL;
		}
	}
	else if( ent->item->giTag == PW_NEUTRALFLAG ) {
		Team_ReturnFlag( TEAM_FREE );
	}
}

/*
==============
Team_DroppedFlagThink

Automatically set in Launch_Item if the item is one of the flags

Flags are unique in that if they are dropped, the base flag must be respawned when they time out
==============
*/
void Team_DroppedFlagThink(gentity_t *ent) {
	int		team = TEAM_FREE;

	if( ent->item->giTag == PW_REDFLAG ) {
		team = TEAM_RED;
		if (level.redPlayerWhoKilledBlueCarrierOfRedFlag) {
			level.redPlayerWhoKilledBlueCarrierOfRedFlag->fcKillsResultingInRets++;
			level.redPlayerWhoKilledBlueCarrierOfRedFlag = NULL;
		}
	}
	else if( ent->item->giTag == PW_BLUEFLAG ) {
		team = TEAM_BLUE;
		if (level.bluePlayerWhoKilledRedCarrierOfBlueFlag) {
			level.bluePlayerWhoKilledRedCarrierOfBlueFlag->fcKillsResultingInRets++;
			level.bluePlayerWhoKilledRedCarrierOfBlueFlag = NULL;
		}
	}
	else if( ent->item->giTag == PW_NEUTRALFLAG ) {
		team = TEAM_FREE;
	}

	Team_ReturnFlagSound( Team_ResetFlag( team ), team );
	PrintCTFMessage(-1, team, CTFMESSAGE_FLAG_RETURNED);
	// Reset Flag will delete this entity
}


/*
==============
Team_DroppedFlagThink
==============
*/

// where did these come from, and why the FUCK is one axis different lmfao dank sil code
static vec3_t	minFlagRange = { 50, 36, 36 };
static vec3_t	maxFlagRange = { 44, 36, 36 };

// should approximate normal pickup range
static vec3_t	fixedFlagRange = { 44, 44, 44 };
#define FLAGPICKUP_TIE_DISTANCE_THRESHOLD	(1)

// cleaner approach to checking who is closest to a flag and thus gets to pick it up
gentity_t *WhoGetsToPickUpTheFlag(gentity_t *flagEnt, int flagColor, gentity_t *whoTriggered) {
	assert(flagEnt);

	vec3_t mins, maxs;
	VectorSubtract(flagEnt->s.pos.trBase, fixedFlagRange, mins);
	VectorAdd(flagEnt->s.pos.trBase, fixedFlagRange, maxs);

	int	touchEntityNums[MAX_GENTITIES];
	int num = trap_EntitiesInBox(mins, maxs, touchEntityNums, MAX_GENTITIES);

	float lowestDistance = 999999999;
	int lowestDistanceEntNum = -1;
	int numEntitiesTiedForLowestDistance = 0;
	int32_t allEntitiesTiedForLowestDistance = 0;

	char buf[MAX_STRING_CHARS] = { 0 };
	if (d_debugFixFlagPickup.integer) {
		Com_sprintf(buf, sizeof(buf), "[%d] %d %s %s flag touch: flag coords %.6f %.6f %.6f",
			trap_Milliseconds(),
			whoTriggered ? whoTriggered - g_entities : -1,
			whoTriggered && whoTriggered->client ? whoTriggered->client->pers.netname : "",
			flagColor == TEAM_RED ? "Red" : "Blue",
			flagEnt->s.pos.trBase[0], flagEnt->s.pos.trBase[1], flagEnt->s.pos.trBase[2]);
	}

	// loop through every player who is close to this flag
	qboolean gotTriggerer = qfalse;
	for (int i = 0; i < num; i++) {
		const int touchEntityClientNum = touchEntityNums[i];
		if (touchEntityClientNum >= MAX_CLIENTS)
			continue; // not a player

		gentity_t *touchEnt = (g_entities + touchEntityClientNum);
		if (touchEnt == whoTriggered)
			gotTriggerer = qtrue;

		if (!touchEnt || !touchEnt->inuse || !touchEnt->client || touchEnt->isAimPracticePack)
			continue; // not a player

		if (touchEnt->health < 1 || touchEnt->client->sess.inRacemode || touchEnt->client->sess.sessionTeam == TEAM_SPECTATOR)
			continue; // dead or not ingame
		
		const int enemyFlagPowerup = touchEnt->client->sess.sessionTeam == TEAM_RED ? PW_BLUEFLAG : PW_REDFLAG;
		qboolean isDropped = !!(flagEnt->flags & FL_DROPPED_ITEM);
		qboolean hasFlag = !!(touchEnt->client->ps.powerups[enemyFlagPowerup]);
		qboolean touchingOwnFlag = !!(touchEnt->client->sess.sessionTeam == flagColor);
		if (!isDropped && !hasFlag && touchingOwnFlag)
			continue; // non-fcs can't "compete" to touch their own flag that is still at the flagstand

		float dist = Distance(flagEnt->s.pos.trBase, touchEnt->client->ps.origin);

		if (d_debugFixFlagPickup.integer)
			Q_strcat(buf, sizeof(buf), va("(%d %s %.6f with coords %.6f %.6f %.6f lastthink %d", touchEnt - g_entities, touchEnt->client->pers.netname, dist,
				touchEnt->client->ps.origin[0], touchEnt->client->ps.origin[1], touchEnt->client->ps.origin[2], level.lastThinkRealTime[touchEntityClientNum]));

		if (dist < lowestDistance) {
			// this player is closer to the flag than any previously checked player
			if (dist < lowestDistance - FLAGPICKUP_TIE_DISTANCE_THRESHOLD) {
				// and is actually closer by a decent amount
				numEntitiesTiedForLowestDistance = 1;
				allEntitiesTiedForLowestDistance = (1 << touchEntityClientNum);
			}
			else {
				// but is only closer by a tiny amount. consider him tied
				++numEntitiesTiedForLowestDistance;
				allEntitiesTiedForLowestDistance |= (1 << touchEntityClientNum);

				if (d_debugFixFlagPickup.integer)
					Q_strcat(buf, sizeof(buf), "*");
			}

			lowestDistance = dist;
			lowestDistanceEntNum = touchEntityClientNum;
		}
		else if (fabs(dist - lowestDistance) < FLAGPICKUP_TIE_DISTANCE_THRESHOLD) {
			// this player's distance to the flag is very very close to at least one previously checked player's distance to the flag. consider him tied
			++numEntitiesTiedForLowestDistance;
			allEntitiesTiedForLowestDistance |= (1 << touchEntityClientNum);

			if (d_debugFixFlagPickup.integer)
				Q_strcat(buf, sizeof(buf), "*");

			//lowestDistance = dist;
			//lowestDistanceEntNum = touchEntityNums[i];
		}

		if (d_debugFixFlagPickup.integer)
			Q_strcat(buf, sizeof(buf), ")");
	}

	// sanity check: always make sure we check the guy who triggered this in the first place
	if (!gotTriggerer) {
		for (int i = 0; i < 1; i++) {
			gentity_t *touchEnt = whoTriggered;

			if (!touchEnt || !touchEnt->inuse || !touchEnt->client || touchEnt->isAimPracticePack)
				continue; // not a player

			const int touchEntityClientNum = whoTriggered - g_entities;

			if (touchEnt->health < 1 || touchEnt->client->sess.inRacemode || touchEnt->client->sess.sessionTeam == TEAM_SPECTATOR)
				continue; // dead or not ingame

			const int enemyFlagPowerup = touchEnt->client->sess.sessionTeam == TEAM_RED ? PW_BLUEFLAG : PW_REDFLAG;
			qboolean isDropped = !!(flagEnt->flags & FL_DROPPED_ITEM);
			qboolean hasFlag = !!(touchEnt->client->ps.powerups[enemyFlagPowerup]);
			qboolean touchingOwnFlag = !!(touchEnt->client->sess.sessionTeam == flagColor);
			if (!isDropped && !hasFlag && touchingOwnFlag)
				continue; // non-fcs can't "compete" to touch their own flag that is still at the flagstand

			float dist = Distance(flagEnt->s.pos.trBase, touchEnt->client->ps.origin);

			if (d_debugFixFlagPickup.integer)
				Q_strcat(buf, sizeof(buf), va("(sanity %d %s %.6f with coords %.6f %.6f %.6f, lastthink %d", touchEnt - g_entities, touchEnt->client->pers.netname, dist,
					touchEnt->client->ps.origin[0], touchEnt->client->ps.origin[1], touchEnt->client->ps.origin[2], level.lastThinkRealTime[touchEntityClientNum]));

			if (dist < lowestDistance) {
				// this player is closer to the flag than any previously checked player
				if (dist < lowestDistance - FLAGPICKUP_TIE_DISTANCE_THRESHOLD) {
					// and is actually closer by a decent amount
					numEntitiesTiedForLowestDistance = 1;
					allEntitiesTiedForLowestDistance = (1 << touchEntityClientNum);
				}
				else {
					// but is only closer by a tiny amount. consider him tied
					++numEntitiesTiedForLowestDistance;
					allEntitiesTiedForLowestDistance |= (1 << touchEntityClientNum);

					if (d_debugFixFlagPickup.integer)
						Q_strcat(buf, sizeof(buf), "*");
				}

				lowestDistance = dist;
				lowestDistanceEntNum = touchEntityClientNum;
			}
			else if (fabs(dist - lowestDistance) < FLAGPICKUP_TIE_DISTANCE_THRESHOLD) {
				// this player's distance to the flag is very very close to at least one previously checked player's distance to the flag. consider him tied
				++numEntitiesTiedForLowestDistance;
				allEntitiesTiedForLowestDistance |= (1 << touchEntityClientNum);

				if (d_debugFixFlagPickup.integer)
					Q_strcat(buf, sizeof(buf), "*");

				//lowestDistance = dist;
				//lowestDistanceEntNum = touchEntityNums[i];
			}

			if (d_debugFixFlagPickup.integer)
				Q_strcat(buf, sizeof(buf), ")");
		}
	}

	if (numEntitiesTiedForLowestDistance > 1) {
		// multiple players are very similar distances from the flag
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (!(allEntitiesTiedForLowestDistance & (1 << i)))
				continue; // you aren't one of them

			if ((flagEnt->flags & FL_DROPPED_ITEM) && level.time - flagEnt->iDroppedTime < 100 && flagEnt->entThatCausedMeToDrop == &g_entities[i]) {
				gentity_t *fcKiller = &g_entities[i];

				if (d_debugFixFlagPickup.integer) {
					Q_strcat(buf, sizeof(buf), va("[%d %s fckiller tiebreaker]\n", fcKiller - g_entities, fcKiller->client->pers.netname));
					G_LogPrintf(buf);
				}

				return fcKiller; // tie goes to the player who just killed the fc very recently, causing this flag to drop
			}
		}

		if (d_debugFixFlagPickup.integer) {
			Q_strcat(buf, sizeof(buf), "[tie]\n");
			G_LogPrintf(buf);
		}

		return NULL; // it's a tie and none of the tied players just killed the fc very recently; don't give the flag to anyone yet
	}

	// one player; he gets the flag
	if (lowestDistanceEntNum != -1) {
		gentity_t *lowestDistanceGuy = &g_entities[lowestDistanceEntNum];

		if (d_debugFixFlagPickup.integer) {
			Q_strcat(buf, sizeof(buf), va("[%d %s]\n", lowestDistanceGuy - g_entities, lowestDistanceGuy->client->pers.netname));
			G_LogPrintf(buf);
		}

		return lowestDistanceGuy;
	}

	if (d_debugFixFlagPickup.integer) {
		Q_strcat(buf, sizeof(buf), "[?]\n");
		G_LogPrintf(buf);
	}

	return NULL; // ???
}


int Team_TouchEnemyFlag( gentity_t *ent, gentity_t *other, int team );
int Team_TouchOurFlag( gentity_t *ent, gentity_t *other, int team ) {
	int			i;
	gentity_t	*player;
	gclient_t	*cl = other->client;
	int			enemy_flag;
	vec3_t		mins, maxs;
	int num, j, enemyTeam;
	int	touch[MAX_GENTITIES];
	gentity_t*	enemy;
	float enemyDist, dist;

	if (cl->sess.sessionTeam == TEAM_RED) {
		enemy_flag = PW_BLUEFLAG;
	} else {
		enemy_flag = PW_REDFLAG;
	}

	if ( ent->flags & FL_DROPPED_ITEM ) {
		// hey, its not home.  return it by teleporting it back
		PrintCTFMessage(other->s.number, team, CTFMESSAGE_PLAYER_RETURNED_FLAG);

		AddScore(other, ent->r.currentOrigin, CTF_RECOVERY_BONUS);
		other->client->stats->rets++;
		other->client->pers.teamState.lastreturnedflag = level.time;

		if (cl->sess.sessionTeam == TEAM_BLUE && level.bluePlayerWhoKilledRedCarrierOfBlueFlag) {
			level.bluePlayerWhoKilledRedCarrierOfBlueFlag->fcKillsResultingInRets++;
			level.bluePlayerWhoKilledRedCarrierOfBlueFlag = NULL;
		}
		else if (cl->sess.sessionTeam == TEAM_RED && level.redPlayerWhoKilledBlueCarrierOfRedFlag) {
			level.redPlayerWhoKilledBlueCarrierOfRedFlag->fcKillsResultingInRets++;
			level.redPlayerWhoKilledBlueCarrierOfRedFlag = NULL;
		}

		//ResetFlag will remove this entity!  We must return zero
		Team_ReturnFlagSound(Team_ResetFlag(team), team);

		return 0;
	}

	// the flag is at home base.  if the player has the enemy
	// flag, he's just won!
	if (!cl->ps.powerups[enemy_flag])
		return 0; // We don't have the flag

	// *CHANGE 15* if intermission occurred, you cant capture the flag after timelimit hit
	if ( level.intermissionQueued ) {
		return 0;
	}

	if (!g_fixFlagPickup.integer || (other && other->client && other->client->sess.inRacemode)) {
		VectorSubtract(ent->s.pos.trBase, minFlagRange, mins);
		VectorAdd(ent->s.pos.trBase, maxFlagRange, maxs);

		num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

		dist = Distance(ent->s.pos.trBase, other->client->ps.origin);

		if (other->client->sess.sessionTeam == TEAM_RED) {
			enemyTeam = TEAM_BLUE;
		}
		else {
			enemyTeam = TEAM_RED;
		}

		for (j = 0; j < num; j++) {
			enemy = (g_entities + touch[j]);

			if (!enemy || !enemy->inuse || !enemy->client) {
				continue;
			}

			// clients in racemode don't interact with flags
			if (enemy->client->sess.inRacemode) {
				continue;
			}

			if (enemy->isAimPracticePack)
				continue;

			//check if its alive
			if (enemy->health < 1)
				continue;		// dead people can't pickup

			//ignore specs
			if (enemy->client->sess.sessionTeam == TEAM_SPECTATOR)
				continue;

			//check if this is enemy
			if ((enemy->client->sess.sessionTeam != TEAM_RED && enemy->client->sess.sessionTeam != TEAM_BLUE) ||
				enemy->client->sess.sessionTeam != enemyTeam) {
				continue;
			}

			//check if enemy is closer to our flag than us
			enemyDist = Distance(ent->s.pos.trBase, enemy->client->ps.origin);
			if (enemyDist < dist) {
				return Team_TouchEnemyFlag(ent, enemy, team);
			}
		}
	}

	//*CHANGE 31* longest flag holding time keeping track
	const int thisFlaghold = G_GetAccurateTimerOnTrigger( &other->client->pers.teamState.flagsince, other, ent );

	other->client->stats->totalFlagHold += thisFlaghold;
	if ( thisFlaghold > other->client->stats->longestFlagHold )
		other->client->stats->longestFlagHold = thisFlaghold;

	// team is the color of the flagstand on which we are capturing, so we add time to the ACTUAL team (not the opposite)
	if ( team == TEAM_RED ) {
		level.redTeamRunFlaghold += thisFlaghold;
	} else {
		level.blueTeamRunFlaghold += thisFlaghold;
	}

	PrintCTFMessage( other->s.number, team, CTFMESSAGE_PLAYER_CAPTURED_FLAG );
	level.redPlayerWhoKilledBlueCarrierOfRedFlag = level.bluePlayerWhoKilledRedCarrierOfBlueFlag = NULL;
	if (level.captureList.size < 50) {
		capture_t *add = ListAdd(&level.captureList, sizeof(capture_t));
		add->capturingTeam = other->client->sess.sessionTeam;
		add->time = level.time - level.startTime;
		//if (other->client->session) add->sessionId = other->client->session->id;
		//Q_strncpyz(add->name, other->client->pers.netname, sizeof(add->name));
	}

	cl->ps.powerups[enemy_flag] = 0;
	
	teamgame.last_flag_capture = level.time;
	teamgame.last_capture_team = team;

	// Increase the team's score
	AddTeamScore(ent->s.pos.trBase, other->client->sess.sessionTeam, 1);
	//rww - don't really want to do this now. Mainly because performing a gesture disables your upper torso animations until it's done and you can't fire

	other->client->stats->captures++;
	other->client->rewardTime = level.time + REWARD_SPRITE_TIME;
	other->client->ps.persistant[PERS_CAPTURES]++;

	// other gets another 10 frag bonus
	AddScore(other, ent->r.currentOrigin, CTF_CAPTURE_BONUS);

	Team_CaptureFlagSound( ent, team );

	// Ok, let's do the player loop, hand out the bonuses
	for (i = 0; i < g_maxclients.integer; i++) {
		player = &g_entities[i];
		if (!player->inuse || player == other)
			continue;

		if (player->client->sess.sessionTeam !=
			cl->sess.sessionTeam) {
			player->client->pers.teamState.lasthurtcarrier = -5;
		} else if (player->client->sess.sessionTeam ==
			cl->sess.sessionTeam) {
			AddScore(player, ent->r.currentOrigin, CTF_TEAM_BONUS);
			// award extra points for capture assists
			if (player->client->pers.teamState.lastreturnedflag + 
				CTF_RETURN_FLAG_ASSIST_TIMEOUT > level.time) {
				AddScore (player, ent->r.currentOrigin, CTF_RETURN_FLAG_ASSIST_BONUS);
				other->client->stats->assists++;

				player->client->ps.persistant[PERS_ASSIST_COUNT]++;
				player->client->rewardTime = level.time + REWARD_SPRITE_TIME;

			} else if (player->client->pers.teamState.lastfraggedcarrier + 
				CTF_FRAG_CARRIER_ASSIST_TIMEOUT > level.time) {
				AddScore(player, ent->r.currentOrigin, CTF_FRAG_CARRIER_ASSIST_BONUS);
				other->client->stats->assists++;
				player->client->ps.persistant[PERS_ASSIST_COUNT]++;
				player->client->rewardTime = level.time + REWARD_SPRITE_TIME;
			}
		}
	}
	Team_ResetFlags();

	// reset the speed stats for this run
	other->client->pers.fastcapTopSpeed = 0;
	other->client->pers.fastcapDisplacement = 0;
	other->client->pers.fastcapDisplacementSamples = 0;

	CalculateRanks();

	return 0; // Do not respawn this automatically
}

// check for taking a flag from the flagstand when the enemy is right about to cap
static void CheckGetFlagSave(gentity_t *taker, gentity_t *flag) {
	if (!taker || !taker->client || !flag || (taker->client->sess.sessionTeam != TEAM_RED && taker->client->sess.sessionTeam != TEAM_BLUE)) {
		assert(qfalse);
		return;
	}

	if (flag->flags & FL_DROPPED_ITEM)
		return; // the flag we picked up was not on the flagstand

	gentity_t *enemyCarrier = NULL;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *thisGuy = &g_entities[i];
		if (!thisGuy->inuse || !thisGuy->client || thisGuy->client->pers.connected != CON_CONNECTED)
			continue;
		if (thisGuy->client->sess.sessionTeam != OtherTeam(taker->client->sess.sessionTeam))
			continue;

		powerup_t alliedFlag = taker->client->sess.sessionTeam == TEAM_RED ? PW_REDFLAG : PW_BLUEFLAG;
		if (!thisGuy->client->ps.powerups[alliedFlag])
			continue;

		enemyCarrier = thisGuy;
		break;
	}

	if (!enemyCarrier)
		return;

	float dist = Distance(flag->r.currentOrigin, enemyCarrier->r.currentOrigin);
	if (dist >= CTF_SAVE_DISTANCE_THRESHOLD)
		return; // enemy carrier is too far from the flagstand

	taker->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
	enemyCarrier->client->ps.persistant[PERS_PLAYEREVENTS] ^= PLAYEREVENT_HOLYSHIT;
	++taker->client->stats->saves;
}

int Team_TouchEnemyFlag( gentity_t *ent, gentity_t *other, int team ) {
	gclient_t *cl = other->client;
	vec3_t		mins, maxs;
	int num, j, ourFlag;
	int	touch[MAX_GENTITIES];
	gentity_t*	enemy;
	float enemyDist, dist;

	if (!g_fixFlagPickup.integer || (other && other->client && other->client->sess.inRacemode)) {
		//fix capture condition v2
		VectorSubtract(ent->s.pos.trBase, minFlagRange, mins);
		VectorAdd(ent->s.pos.trBase, maxFlagRange, maxs);

		num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

		dist = Distance(ent->s.pos.trBase, other->client->ps.origin);

		if (other->client->sess.sessionTeam == TEAM_RED) {
			ourFlag = PW_REDFLAG;
		}
		else {
			ourFlag = PW_BLUEFLAG;
		}

		for (j = 0; j < num; ++j) {
			enemy = (g_entities + touch[j]);

			if (!enemy || !enemy->inuse || !enemy->client) {
				continue;
			}

			// clients in racemode don't interact with flags
			if (enemy->client->sess.inRacemode) {
				continue;
			}

			if (enemy->isAimPracticePack)
				continue;

			//ignore specs
			if (enemy->client->sess.sessionTeam == TEAM_SPECTATOR)
				continue;

			//check if its alive
			if (enemy->health < 1)
				continue;		// dead people can't pick up items

			//lets check if he has our flag
			if (!enemy->client->ps.powerups[ourFlag])
				continue;

			//check if enemy is closer to our flag than us
			enemyDist = Distance(ent->s.pos.trBase, enemy->client->ps.origin);
			if (enemyDist < dist) {
				return Team_TouchOurFlag(ent, enemy, team);
			}
		}
	}

	PrintCTFMessage(other->s.number, team, CTFMESSAGE_PLAYER_GOT_FLAG);
	++other->client->stats->numFlagHolds;

	if (team == TEAM_RED) {
		cl->ps.powerups[PW_REDFLAG] = INT_MAX; // flags never expire
		level.redPlayerWhoKilledBlueCarrierOfRedFlag = NULL;
	}
	else {
		cl->ps.powerups[PW_BLUEFLAG] = INT_MAX; // flags never expire
		level.bluePlayerWhoKilledRedCarrierOfBlueFlag = NULL;
	}

	if (!(ent->flags & FL_DROPPED_ITEM)){ 
		// initial flag steal, so reset the team flaghold times
		// team is the color of the STOLEN flag, so we reset the run flaghold of the opposite team
		if ( team == TEAM_RED ) {
			level.redFlagStealTime = level.time;
			level.blueTeamRunFlaghold = 0;
		} else {
			level.blueFlagStealTime = level.time;
			level.redTeamRunFlaghold = 0;
		}

		++other->client->stats->numGets;
		other->client->stats->getTotalHealth += (other->health + other->client->ps.stats[STAT_ARMOR]);
	}
	else {
		// picking up a dropped flag
		
		// refund fc teamkills
		qboolean gotFcTeamkiller = qfalse;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *teammate = &g_entities[i];
			if (!teammate->inuse || !teammate->client || teammate->client->sess.sessionTeam != other->client->sess.sessionTeam)
				continue;

			if (teammate->client->pers.killedAlliedFlagCarrier && !gotFcTeamkiller) {

				--teammate->client->stats->teamKills; // refund the teamkill

				if (teammate == other)
					++teammate->client->stats->takes; // this person killed the fc and took the flag; additionally give them a TAKE

				gotFcTeamkiller = qtrue; // sanity check: there should never be more than one such player per team
			}
			teammate->client->pers.killedAlliedFlagCarrier = qfalse;
		}
	}

	CheckGetFlagSave(other, ent);

	// picking up the flag removes invulnerability
	cl->ps.eFlags &= ~EF_INVULNERABLE;

	G_ResetAccurateTimerOnTrigger( &cl->pers.teamState.flagsince, other, ent );

	Team_SetFlagStatus( team, FLAG_TAKEN );
	AddScore(other, ent->r.currentOrigin, CTF_FLAG_BONUS);
	Team_TakeFlagSound( ent, team );

	return -1; // Do not respawn this automatically, but do delete it if it was FL_DROPPED
}

int Pickup_Team( gentity_t *ent, gentity_t *other ) {
	int team;
	gclient_t *cl = other->client;

	// figure out what team this flag is
	if( strcmp(ent->classname, "team_CTF_redflag") == 0 ) {
		team = TEAM_RED;
	}
	else if( strcmp(ent->classname, "team_CTF_blueflag") == 0 ) {
		team = TEAM_BLUE;
	}
	else if( strcmp(ent->classname, "team_CTF_neutralflag") == 0  ) {
		team = TEAM_FREE;
	}
	else {
//		PrintMsg ( other, "Don't know what team the flag is on.\n");
		return 0;
	}
	// GT_CTF

	if (g_fixFlagPickup.integer && !(other && other->client && other->client->sess.inRacemode)) {
		gentity_t *pickerUpper = WhoGetsToPickUpTheFlag(ent, team, other);
		if (pickerUpper && pickerUpper->client) {
			if (pickerUpper->client->sess.sessionTeam == team)
				return Team_TouchOurFlag(ent, pickerUpper, team);
			else
				return Team_TouchEnemyFlag(ent, pickerUpper, team);
		}
		return 0;
	}
	else {
		if (team == cl->sess.sessionTeam) {
			return Team_TouchOurFlag(ent, other, team);
		}
		return Team_TouchEnemyFlag(ent, other, team);
	}
}

gentity_t *oldSpawn = NULL;
int SortSpotsByDistance(const void *a, const void *b) {
	if (!oldSpawn)
		return 0; // shouldn't happen

	gentity_t *aa = *((gentity_t **)a);
	gentity_t *bb = *((gentity_t **)b);

	if (aa == oldSpawn)
		return 1;
	else if (bb == oldSpawn)
		return -1;

	float aDist = Distance2D(oldSpawn->s.origin, aa->s.origin);
	float bDist = Distance2D(oldSpawn->s.origin, bb->s.origin);

	if (aDist < bDist)
		return 1;
	else if (bDist < aDist)
		return -1;
	return 0;
}

int SortSpotsByDistanceClosestToPlayer(const void *a, const void *b) {
	if (!oldSpawn)
		return 0; // shouldn't happen

	gentity_t *aa = *((gentity_t **)a);
	gentity_t *bb = *((gentity_t **)b);

	if (aa == oldSpawn)
		return 1;
	else if (bb == oldSpawn)
		return -1;

	float aDist, bDist;
	if (oldSpawn->client) {
		aDist = Distance2D(oldSpawn->client->ps.origin, aa->s.origin);
		bDist = Distance2D(oldSpawn->client->ps.origin, bb->s.origin);
	}
	else {
		aDist = Distance2D(oldSpawn->s.origin, aa->s.origin);
		bDist = Distance2D(oldSpawn->s.origin, bb->s.origin);
	}

	if (aDist < bDist)
		return -1;
	else if (bDist < aDist)
		return 1;
	return 0;
}

#define DEFAULT_SPAWNBOOST_MULTIPLIER	(0.333f)
static float GetFcSpawnBoostMultiplier(gentity_t *ent) {
	float defaultValue;
	if (g_spawnboost_default.string[0] && Q_isanumber(g_spawnboost_default.string))
		defaultValue = g_spawnboost_default.value;
	else
		defaultValue = DEFAULT_SPAWNBOOST_MULTIPLIER;

	if (!ent || !ent->client)
		return defaultValue;

	static char cvarName[MAX_QPATH * 2] = { 0 };
	if (!cvarName[0]) {
		char shortName[MAX_QPATH] = { 0 };
		GetShortNameForMapFileName(level.mapname, shortName, sizeof(shortName));
		TrimMapVersion(shortName, shortName, sizeof(shortName));
		Com_sprintf(cvarName, sizeof(cvarName), "g_spawnboost_%s", shortName);
	}

	if (!cvarName[0])
		return defaultValue;
	
	char buf[MAX_STRING_CHARS] = { 0 };
	trap_Cvar_VariableStringBuffer(cvarName, buf, sizeof(buf));

	if (!buf[0] || !Q_isanumber(buf))
		return defaultValue;

	float multiplier = atof(buf);
	//Com_DebugPrintf("Per %s, using multiplier %0.3f\n", cvarName, multiplier);

	return multiplier;
}

static float GetFcSpawnBoostZAxisBump(void) {
	static char cvarName[MAX_QPATH * 2] = { 0 };
	if (!cvarName[0]) {
		char shortName[MAX_QPATH] = { 0 };
		GetShortNameForMapFileName(level.mapname, shortName, sizeof(shortName));
		TrimMapVersion(shortName, shortName, sizeof(shortName));
		Com_sprintf(cvarName, sizeof(cvarName), "g_spawnboostzbump_%s", shortName);
	}

	if (!cvarName[0])
		return 0;

	char buf[MAX_STRING_CHARS] = { 0 };
	trap_Cvar_VariableStringBuffer(cvarName, buf, sizeof(buf));

	if (!buf[0] || !Q_isanumber(buf))
		return 0;

	int num = atof(buf);
	if (num < 0)
		return 0;

	return num;
}

#define SQRT2 (1.4142135623730951)

#define SPAWNFCBOOST_NERFEDMAP_IDEAL_XYADD			(300)		// if we're on a map where the g_spawnboost_[mapname] cvar is set, add this much to the ideal xy distance (i.e., we try to spawn farther from the fc on maps like dosuun)
#define SPAWNFCBOOST_IDEAL_XYDISTANCE				(g_spawnboost_losIdealDistance.integer + compensateIdeal + (isNerfedMap ? SPAWNFCBOOST_NERFEDMAP_IDEAL_XYADD : 0))		// ideal xy distance we'd like to spawn from the fc
#define SPAWNFCBOOST_IDEAL_XYDISTANCE_UNCOMPENSATED		(g_spawnboost_losIdealDistance.integer + (isNerfedMap ? SPAWNFCBOOST_NERFEDMAP_IDEAL_XYADD : 0))		// ideal xy distance we'd like to spawn from the fc, not including compensation
#define SPAWNFCBOOST_VELOCITY_COEFFICIENT			(0.25f)		// how much to scale fc's velocity by when evaluating his position (if he's moving quickly)
#define SPAWNFCBOOST_LOS_Z_ADD						(32)		// slight z-axis boost from the actual point we should use as a reference point for line of sight to fc
#define SPAWNFCBOOST_GRID_INCREMENT					(256)		// how many units away from a weapon/ammo/health we'd like to evaluate for potentially spawning
#define SPAWNFCBOOST_GRID_RESOLUTION				(3)			// how many increments of SPAWNFCBOOST_GRID_INCREMENT away from the weapon/health/ammo to evaluate (in both X and Y dimensions)
#define SPAWNFCBOOST_PITCHECK_DISTANCE_INCREMENT	(80)		// how far away from potential spawnpoints we check to see if we will pit (if we step forward this distance toward the fc)
#define SPAWNFCBOOST_NUM_PIT_CHECKS					(3)			// how many increments of SPAWNFCBOOST_PITCHECK_DISTANCE_INCREMENT we check
#define SPAWNFCBOOST_OURSIDEOFTHEMAP_THRESHOLD		(0.4f)		// cap on how far towards enemy side of the map we are allowed to spawn; 0.5 is mid
#define SPAWNFCBOOST_SIMILARDISTANCE_THRESHOLD		(160)		// near zero, always prefer more ideal points even if only closer by a little; higher = more randomness for points similarly distant from ideal
#define SPAWNFCBOOST_WEAPON_MINDISTANCE_THRESHOLD	(0)			// minimum distance to weapon/ammo/health being evaluated for spawning nearby
#define SPAWNFCBOOST_WEAPON_MAXDISTANCE_THRESHOLD	(SPAWNFCBOOST_IDEAL_XYDISTANCE + ((SPAWNFCBOOST_GRID_INCREMENT * SPAWNFCBOOST_GRID_RESOLUTION) * SQRT2) + 1)		// maximum distance to weapon/ammo/health being evaluated for spawning nearby
#define SPAWNFCBOOST_FCZDELTA_THRESHOLD				(500)		// start applying a penalty to absDeltaFromIdeal for spawnpoints this much vertically underneath the fc

extern qboolean PointsAreOnOppositeSidesOfMap(vec3_t pointA, vec3_t pointB);

#ifdef SPAWNFCBOOST_DEBUG
#define SpawnFcBoostDebugPrintf(...)	Com_Printf(__VA_ARGS__)
#else
#define SpawnFcBoostDebugPrintf(...)	do {} while (0)
#endif

typedef struct {
	node_t		node;
	gentity_t	*entity;
} listEnt_t;

float CalculateYawFromPointToPoint(vec3_t source, vec3_t target) {
	float deltaY = target[1] - source[1];
	float deltaX = target[0] - source[0];

	float yaw = atan2(deltaY, deltaX) * (180.0 / M_PI);

	return yaw;
}

static qboolean SpawningAtPointWouldTelefrag(vec3_t point) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t *hit;
	vec3_t		mins, maxs;

	vec3_t	playerMins = { -15, -15, DEFAULT_MINS_2 };
	vec3_t	playerMaxs = { 15, 15, DEFAULT_MAXS_2 };

	VectorAdd(point, playerMins, mins);
	VectorAdd(point, playerMaxs, maxs);

	// spawn logic can move player a bit above, we need to be more strict
	maxs[2] += 9;

	num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

	for (i = 0; i < num; i++) {
		hit = &g_entities[touch[i]];
		//if ( hit->client && hit->client->ps.stats[STAT_HEALTH] > 0 ) {
		if (hit->client && !hit->client->sess.inRacemode) {
			return qtrue;
		}

	}

	return qfalse;
}

extern qboolean isRedFlagstand(gentity_t *ent);
extern qboolean isBlueFlagstand(gentity_t *ent);
#define DotProduct2D(a,b)	((a)[0]*(b)[0]+(a)[1]*(b)[1])
static float GetCTFLocationValueOfPoint(vec3_t point, int team) {
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
	if (team == TEAM_RED) {
		allyFs = &redFs[0];
		enemyFs = &blueFs[0];
	}
	else {
		allyFs = &blueFs[0];
		enemyFs = &redFs[0];
	}

	float rPrime[2] = { point[0] - allyFs[0], point[1] - allyFs[1] };
	float v[2] = { enemyFs[0] - allyFs[0], enemyFs[1] - allyFs[1] };
	return DotProduct2D(v, rPrime) / DotProduct2D(v, v);
}

typedef struct {
	int x;
	int y;
} gridPoint_t;

qboolean WillPitOrStartSolidOrBeOnTriggerIfMoveToPoint(vec3_t startingPoint, float distance, vec3_t towardPoint, vec3_t mins, vec3_t maxs) {
	trace_t tr;
	vec3_t down, movedPoint;
	const float yaw = CalculateYawFromPointToPoint(startingPoint, towardPoint);
	const float yawInRadians = yaw * (M_PI / 180.0);
	movedPoint[0] = startingPoint[0] + cos(yawInRadians) * distance;
	movedPoint[1] = startingPoint[1] + sin(yawInRadians) * distance;
	movedPoint[2] = startingPoint[2];

	VectorCopy(movedPoint, down);
	down[2] -= 32768;
	trap_Trace(&tr, movedPoint, mins, maxs, down, ENTITYNUM_NONE, MASK_PLAYERSOLID);

	return (tr.startsolid || fabs(tr.endpos[2] - movedPoint[2]) > 50 || (tr.entityNum != ENTITYNUM_NONE && (g_entities[tr.entityNum].r.contents == CONTENTS_TRIGGER || !Q_stricmp(g_entities[tr.entityNum].classname, "trigger_hurt") || !Q_stricmp(g_entities[tr.entityNum].classname, "trigger_push") || !Q_stricmp(g_entities[tr.entityNum].classname, "func_door") || !Q_stricmp(g_entities[tr.entityNum].classname, "func_plat"))));
}

#ifdef SPAWNFCBOOST_DEBUG
extern int logTraces, numLoggedTraces;
#endif

#if 0
vec3_t fcPosition = { 0 };

int compareWeapons(const void *a, const void *b) {
	const gentity_t *weaponA = *(const gentity_t **)a;
	const gentity_t *weaponB = *(const gentity_t **)b;

	float distA = Distance2D(weaponA->s.origin, fcPosition);
	float distB = Distance2D(weaponB->s.origin, fcPosition);

	float deltaA = fabs(distA - SPAWNFCBOOST_IDEAL_XYDISTANCE);
	float deltaB = fabs(distB - SPAWNFCBOOST_IDEAL_XYDISTANCE);

	if (fabs(deltaA - deltaB) < 400.0f) {
		return 0;
	}

	return (deltaA < deltaB) ? -1 : 1;
}
#endif

static qboolean IsPointVisiblePlayersolid(vec3_t org1, vec3_t org2, int fcNum)
{
	trace_t tr;
	trap_Trace(&tr, org1, NULL, NULL, org2, fcNum, MASK_PLAYERSOLID);
	return !!(tr.fraction == 1);
}

// try to find a spot for this guy to respawn where he can see his flag carrier from a moderate distance
static gentity_t *GetSpawnFcBoostLocation(gclient_t *spawningGuy, gentity_t *fc) {
#ifdef SPAWNFCBOOST_DEBUG
	logTraces = 1;
	numLoggedTraces = 0;
#endif

	if (!fc || !fc->client) {
		assert(qfalse);
		return NULL;
	}

	qboolean isNerfedMap = qfalse;
	{
		float multiplier = GetFcSpawnBoostMultiplier(fc);
		if (multiplier > DEFAULT_SPAWNBOOST_MULTIPLIER + 0.0001f) {
			isNerfedMap = qtrue;
			SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: nerfed map (multiplier: %0.3f)\n", multiplier);
		}
	}

	float useHigherZAxisLOSCheck = GetFcSpawnBoostZAxisBump();
	if (useHigherZAxisLOSCheck) {
		SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: using Z axis bump of %d on this map\n", useHigherZAxisLOSCheck);
	}

	// try to compensate for the last spawn, e.g. if we spawned +500 from ideal last time then we'll try to spawn -500 from ideal this time
	// if we do set this, we'll zero out the stored value at the end of this function (preventing you from just bouncing
	// between +500 and -500 endlessly throughout the match, which would be dumb)
	float compensateIdeal = 0;
	if (spawningGuy && spawningGuy->pers.lastSpawnFcBoostSpawnDistanceFromFcDeltaFromIdeal) {
		compensateIdeal = 0 - spawningGuy->pers.lastSpawnFcBoostSpawnDistanceFromFcDeltaFromIdeal;
		SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: last delta was %d, so setting compensateIdeal to %d.\n", (int)spawningGuy->pers.lastSpawnFcBoostSpawnDistanceFromFcDeltaFromIdeal, (int)compensateIdeal);
	}

	// if fc is moving quickly, try to trace to where the fc will be shortly instead of where the fc is right now
	vec3_t fcOrigin;
	VectorCopy(fc->client->ps.origin, fcOrigin);
	float fcXySpeed = sqrt(fc->client->ps.velocity[0] * fc->client->ps.velocity[0] + fc->client->ps.velocity[1] * fc->client->ps.velocity[1]);
	qboolean gotFastGuy = qfalse;
	if (fcXySpeed >= 600) {
		fcOrigin[0] += fc->client->ps.velocity[0] * SPAWNFCBOOST_VELOCITY_COEFFICIENT;
		fcOrigin[1] += fc->client->ps.velocity[1] * SPAWNFCBOOST_VELOCITY_COEFFICIENT;
		SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: added velocity[0] %d to ps.origin[0] %d for new value %d\n", (int)fc->client->ps.velocity[0], (int)fc->client->ps.origin[0], (int)fcOrigin[0]);
		SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: added velocity[1] %d to ps.origin[1] %d for new value %d\n", (int)fc->client->ps.velocity[1], (int)fc->client->ps.origin[1], (int)fcOrigin[1]);
		gotFastGuy = qtrue;
	}
	if (fabs(fc->client->ps.velocity[2]) >= 400) {
		fcOrigin[2] += fc->client->ps.velocity[2] * SPAWNFCBOOST_VELOCITY_COEFFICIENT;
		SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: added velocity[2] %d to ps.origin[2] %d for new value %d\n", (int)fc->client->ps.velocity[2], (int)fc->client->ps.origin[2], (int)fcOrigin[2]);
		gotFastGuy = qtrue;
	}
	if (gotFastGuy) {
		trace_t tr;
		vec3_t mins, maxs;
		VectorSet(mins, -15, -15, DEFAULT_MINS_2);
		VectorSet(maxs, 15, 15, DEFAULT_MAXS_2);
		trap_Trace(&tr, fc->client->ps.origin, mins, maxs, fcOrigin, fc - g_entities, MASK_PLAYERSOLID);
		if (!tr.startsolid)
			VectorCopy(tr.endpos, fcOrigin);
	}

	// get nearby weapons
	int weaponArraySize = 16, numWeapons = 0;
	gentity_t **filteredWeaponsArray = malloc(weaponArraySize * sizeof(gentity_t *));

	// loop through weapons/items in a random order and assemble a list of those close-ish to the fc
	int indices[MAX_GENTITIES - MAX_CLIENTS];
	for (int i = 0; i < MAX_GENTITIES - MAX_CLIENTS; ++i)
		indices[i] = i + MAX_CLIENTS;

	FisherYatesShuffle(indices, MAX_GENTITIES - MAX_CLIENTS, sizeof(int));

	for (int i = 0; i < MAX_GENTITIES - MAX_CLIENTS; ++i) {
		int shuffled_index = indices[i];
		gentity_t *weap = &g_entities[shuffled_index];
		if (!weap->item || (weap->item->giType != IT_WEAPON && weap->item->giType != IT_AMMO && weap->item->giType != IT_ARMOR && weap->item->giType != IT_HEALTH && weap->item->giType != IT_TEAM))
			continue;

		float dist = Distance(weap->s.origin, fcOrigin);
		if (dist > SPAWNFCBOOST_WEAPON_MAXDISTANCE_THRESHOLD)
			continue;
		if (dist < SPAWNFCBOOST_WEAPON_MINDISTANCE_THRESHOLD)
			continue;
		if (PointsAreOnOppositeSidesOfMap(weap->s.origin, fcOrigin))
			continue;
		if (GetCTFLocationValueOfPoint(weap->s.origin, fc->client->sess.sessionTeam) >= 0.5f)
			continue;

#if 0
		SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: checking item entity %d\n", shuffled_index);
		// try tracing to fc with the weapon itself; saves computing traces for an entire grid that maybe can't see the fc below
		// if performance isn't a concern, this check can be deleted
		vec3_t losPoint;
		VectorCopy(weap->s.origin, losPoint);
		losPoint[2] += SPAWNFCBOOST_LOS_Z_ADD;
		if (!IsPointVisiblePlayersolid(losPoint, fcOrigin))
			continue;
		SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: got visible %s at %d %d %d\n", weap->classname, (int)weap->s.origin[0], (int)weap->s.origin[1], (int)weap->s.origin[2]);
#else
		SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: got %s at %d %d %d\n", weap->classname, (int)weap->s.origin[0], (int)weap->s.origin[1], (int)weap->s.origin[2]);
#endif


		// add to array
		if (numWeapons >= weaponArraySize) {
			// Reallocate array
			weaponArraySize *= 2;
			filteredWeaponsArray = realloc(filteredWeaponsArray, weaponArraySize * sizeof(gentity_t *));
		}
		filteredWeaponsArray[numWeapons] = weap;
		numWeapons++;
	}

	if (!numWeapons) {
		free(filteredWeaponsArray);
		return NULL;
	}

#if 0
	// roughly sort the weapons by closeness to the ideal
	VectorCopy(fcOrigin, fcPosition);
	qsort(filteredWeaponsArray, numWeapons, sizeof(gentity_t *), compareWeapons);
	SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: sorted weapons array:");
	for (int i = 0; i < numWeapons; i++)
		SpawnFcBoostDebugPrintf(" %s (%d)", filteredWeaponsArray[i]->classname, (int)fabs(SPAWNFCBOOST_IDEAL_XYDISTANCE - Distance2D(filteredWeaponsArray[i]->s.origin, fcOrigin)));
	SpawnFcBoostDebugPrintf("\n");
#endif

	// loop through the weapons we got and evaluate random spots around the weapon
	// we check to see if we would be pitted spawning there, if we have line of sight to the fc from them, if we would telefrag someone here, etc
	// we do this by drawing a grid of points around the weapon, which we check in a random order
	
	// first, set up the grid
	const int x_min = -SPAWNFCBOOST_GRID_RESOLUTION, x_max = SPAWNFCBOOST_GRID_RESOLUTION;
	const int y_min = -SPAWNFCBOOST_GRID_RESOLUTION, y_max = SPAWNFCBOOST_GRID_RESOLUTION;
	const int x_range = x_max - x_min + 1;
	const int y_range = y_max - y_min + 1;
	int total_points = x_range * y_range;
	gridPoint_t *gridPoints = malloc(total_points * sizeof(gridPoint_t));
	int idx = 0;
	for (int i = x_min; i <= x_max; ++i) {
		for (int j = y_min; j <= y_max; ++j) {
			gridPoints[idx].x = i;
			gridPoints[idx].y = j;
			++idx;
		}
	}

	// ...loop through the weapons/items...
	qboolean gotBestPoint = qfalse;
	vec3_t bestPointOrigin;
	float bestPointDeltaFromTrueIdeal = 999999999, bestPointAbsDeltaFromIdeal = 999999999;
#ifdef SPAWNFCBOOST_DEBUG
	char bestName[64] = { 0 };
	int bestX = 0, bestY = 0;
#endif
#if 0
	int pitPointsArraySize = 16, numPitPoints = 0;
	gridPoint_t *pitPoints = malloc(pitPointsArraySize * sizeof(gridPoint_t));
#endif
	for (int weapIdx = 0; weapIdx < numWeapons; ++weapIdx) {
		gentity_t *weap = filteredWeaponsArray[weapIdx];

		FisherYatesShuffle(gridPoints, total_points, sizeof(gridPoint_t)); // randomize the grid order anew for each weapon/item

		// ...iterate through the grid around this particular weapon
		for (int k = 0; k < total_points; ++k) {
			int i = gridPoints[k].x;
			int j = gridPoints[k].y;
			if (!i && !j)
				continue; // skip the exact weapon spot itself, (0, 0)

			vec3_t point;
			VectorCopy(weap->s.origin, point);
			point[0] += (i * SPAWNFCBOOST_GRID_INCREMENT);
			point[1] += (j * SPAWNFCBOOST_GRID_INCREMENT);

			SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: evaluating %s with grid point (%d, %d)\n", weap->classname, i, j);

			// we now have a point somewhere around a weapon/item. perform the various checks to see if this is a good spot to spawn.

			if (GetCTFLocationValueOfPoint(point, fc->client->sess.sessionTeam) > SPAWNFCBOOST_OURSIDEOFTHEMAP_THRESHOLD) {
				SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: on other side of map\n");
				continue;
			}

			float dist2d = Distance2D(point, fcOrigin);
			float absDeltaFromIdeal = fabs(dist2d - SPAWNFCBOOST_IDEAL_XYDISTANCE);
			float deltaFromTrueIdeal = dist2d - SPAWNFCBOOST_IDEAL_XYDISTANCE_UNCOMPENSATED;
			if (fcOrigin[2] - SPAWNFCBOOST_FCZDELTA_THRESHOLD >= point[2]) {
				SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: point[2] %d is %d lower than fcOrigin[2] %d (>= %d), so penalizing abs delta from ideal\n", (int)point[2], (int)(fcOrigin[2] - point[2]), (int)fcOrigin[2], SPAWNFCBOOST_FCZDELTA_THRESHOLD);
				absDeltaFromIdeal += (fcOrigin[2] - point[2]); // penalize points much higher than the fc
			}
			if (dist2d < 700) {
				SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: harshly penalizing because distance is quite small\n");
				absDeltaFromIdeal *= 1.2f;
			}
			else if (dist2d < SPAWNFCBOOST_IDEAL_XYDISTANCE - 128) {
				SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: slightly penalizing because distance is lower than ideal\n");
				absDeltaFromIdeal *= 1.05f;
			}
			SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: has dist2d %d, delta from ideal %d\n", (int)dist2d, (int)absDeltaFromIdeal);

			// if we already have a point that's better than this one by at least the threshold, don't bother checking this one
			if (gotBestPoint && absDeltaFromIdeal >= bestPointAbsDeltaFromIdeal - SPAWNFCBOOST_SIMILARDISTANCE_THRESHOLD) {
				SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: not worth checking since abs delta from ideal not that much better than current best\n");
				continue;
			}

			if (!g_canSpawnInTeRangeOfFc.integer) {
				if (Distance(point, fc->client->ps.origin/*not fcOrigin*/) <= 512 && trap_InPVS(point, fc->client->ps.origin/*not fcOrigin*/)) {
					SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: skipping because !g_canSpawnInTeRangeOfFc and in te range\n");
					continue;
				}
			}

			if (g_killedAntiHannahSpawnRadius.integer > 0) {
				if (spawningGuy && spawningGuy->pers.lastKilledByEnemyTime && level.time - spawningGuy->pers.lastKilledByEnemyTime < 3500) {
					if (Distance(spawningGuy->pers.lastKilledByEnemyLocation, point) < g_killedAntiHannahSpawnRadius.value) {
						SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: skipping because g_killedAntiHannahSpawnRadius and too close to killed lastKilledByEnemyLocation\n");
						continue;
					}
				}
			}

#if 0
			qboolean closeToAlreadyPittedPoint = qfalse;
			if (numPitPoints > 0) {
				for (int i2 = 0; i2 < numPitPoints; ++i2) {
					float dx = point[0] - pitPoints[i2].x;
					float dy = point[1] - pitPoints[i2].y;
					if (sqrt(dx * dx + dy * dy) < 50.0f) {
						closeToAlreadyPittedPoint = qtrue;
						break;
					}
				}
			}
			if (closeToAlreadyPittedPoint) {
				SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: close to already pitted point\n");
				continue;
			}
#endif

			// check whether we'll pit in this spot and whether this is in a wall
			trace_t tr;
			vec3_t down, mins, maxs;
			VectorSet(mins, -15, -15, DEFAULT_MINS_2);
			VectorSet(maxs, 15, 15, DEFAULT_MAXS_2);
			VectorCopy(point, down);
			down[2] -= 32768;
			trap_Trace(&tr, point, mins, maxs, down, ENTITYNUM_NONE, MASK_PLAYERSOLID | CONTENTS_TRIGGER);
			if (tr.startsolid ||
				fabs(tr.endpos[2] - point[2]) > 50 ||
				(tr.entityNum != ENTITYNUM_NONE && (g_entities[tr.entityNum].r.contents == CONTENTS_TRIGGER || !Q_stricmp(g_entities[tr.entityNum].classname, "trigger_hurt") || !Q_stricmp(g_entities[tr.entityNum].classname, "trigger_push") || !Q_stricmp(g_entities[tr.entityNum].classname, "func_door") || !Q_stricmp(g_entities[tr.entityNum].classname, "func_plat")))) {
				SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: insta pits, starts solid, or above a trigger\n");
#if 0
				if (numPitPoints >= pitPointsArraySize) {
					pitPointsArraySize *= 2;
					pitPoints = realloc(pitPoints, pitPointsArraySize * sizeof(gridPoint_t));
				}
				pitPoints[numPitPoints].x = i;
				pitPoints[numPitPoints].y = j;
				++numPitPoints;
#endif
				continue; // pit or otherwise too high from solid ground
			}

			// check line of sight to fc from a little bit above the trace's endpoint
			vec3_t losPoint, fcLosPoint;
			VectorCopy(tr.endpos, losPoint);
			VectorCopy(fcOrigin, fcLosPoint);
			losPoint[2] += SPAWNFCBOOST_LOS_Z_ADD;
			fcLosPoint[2] += SPAWNFCBOOST_LOS_Z_ADD + useHigherZAxisLOSCheck;
			if (!IsPointVisiblePlayersolid(fcLosPoint, losPoint, fc - g_entities)) {
				SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: no LoS\n");
				continue;
			}

			// check a couple spots in front of the point to see whether we'll pit if we take a few steps
			qboolean continueToNextPoint = qfalse;
			for (int pitCheckNum = 1; pitCheckNum <= SPAWNFCBOOST_NUM_PIT_CHECKS; pitCheckNum++) {
				if (WillPitOrStartSolidOrBeOnTriggerIfMoveToPoint(tr.endpos, SPAWNFCBOOST_PITCHECK_DISTANCE_INCREMENT * pitCheckNum, fcOrigin, mins, maxs)) {
					SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: will pit/startsolid/be on trigger if you take a few steps (check #%d)\n", pitCheckNum);
					continueToNextPoint = qtrue;
					break;
				}
			}
			if (continueToNextPoint)
				continue;

			// check if we would telefrag here
			if (SpawningAtPointWouldTelefrag(tr.endpos)) {
				SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: would telefrag\n");
				continue;
			}

			gotBestPoint = qtrue;
			VectorCopy(tr.endpos, bestPointOrigin);
			bestPointAbsDeltaFromIdeal = absDeltaFromIdeal;
			bestPointDeltaFromTrueIdeal = deltaFromTrueIdeal;
			SpawnFcBoostDebugPrintf("GetSpawnFcBoostLocation: this is the new current best (abs delta from ideal: %d)\n", (int)absDeltaFromIdeal);
#ifdef SPAWNFCBOOST_DEBUG
			bestX = i;
			bestY = j;
			Q_strncpyz(bestName, weap->classname, sizeof(bestName));
#endif

			// if this is pretty close to the ideal, then don't bother checking other points around this weapon (saves computing unnecessary traces)
			// if performance isn't a concern, this check can be deleted
			if (fabs(dist2d - SPAWNFCBOOST_IDEAL_XYDISTANCE) <= 200)
				goto nextWeapon;
		}
		nextWeapon:;
	}

#if 0
	free(pitPoints);
#endif
	free(gridPoints);
	free(filteredWeaponsArray);

	if (!gotBestPoint)
		return NULL;

	// slightly hacky, create a temp entity with this origin since the spawnpoint code all uses entities (info_team_red or whatever) rather than points
	gentity_t *te = G_TempEntity(bestPointOrigin, EV_SCOREPLUM/*idk*/);
	te->r.svFlags &= ~SVF_BROADCAST;
	te->r.svFlags |= SVF_NOCLIENT;
	VectorCopy(bestPointOrigin, te->s.origin);

	if (compensateIdeal)
		spawningGuy->pers.lastSpawnFcBoostSpawnDistanceFromFcDeltaFromIdeal = 0; // we compensated, so clear it out for a fresh start next time
	else
		spawningGuy->pers.lastSpawnFcBoostSpawnDistanceFromFcDeltaFromIdeal = bestPointDeltaFromTrueIdeal; // we didn't compensate, so note the delta to compensate next time
	
	SpawnFcBoostDebugPrintf("Finished spawn FC boost check. Winner: %s (%d, %d) with delta %d and true delta %d. Performed %d traces.\n", bestName, bestX, bestY, (int)bestPointAbsDeltaFromIdeal, (int)bestPointDeltaFromTrueIdeal, numLoggedTraces);
#ifdef SPAWNFCBOOST_DEBUG
	logTraces = 0;
#endif



	return te;

	return NULL;
}

/*---------------------------------------------------------------------------*/

extern qboolean isRedFlagstand(gentity_t *ent);
extern qboolean isBlueFlagstand(gentity_t *ent);

typedef struct {
	node_t		node;
	gentity_t	*ent;
	char		nickname[128];
	int			priority;
} spawnpoint_t;

typedef struct {
	team_t			team;
	ctfPosition_t	pos;
} SpawnpointComparisonContext;

int SortSpawnsByDistanceToFS(genericNode_t *a, genericNode_t *b, void *userData) {
	assert(a && b && userData);

	const spawnpoint_t *aa_spawn = (const spawnpoint_t *)a;
	const spawnpoint_t *bb_spawn = (const spawnpoint_t *)b;
	const gentity_t *aa = aa_spawn->ent;
	const gentity_t *bb = bb_spawn->ent;

	SpawnpointComparisonContext *ctx = userData;

	static qboolean initializedFlagstands = qfalse;
	static gentity_t *redFS = NULL, *blueFS = NULL;
	if (!initializedFlagstands) {
		for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
			gentity_t *flagstand = &g_entities[i];
			if (!Q_stricmp(flagstand->classname, "team_ctf_redflag"))
				redFS = flagstand;
			else if (!Q_stricmp(flagstand->classname, "team_ctf_blueflag"))
				blueFS = flagstand;
		}

		initializedFlagstands = qtrue;
#if 1
		srand(time(NULL)); // so that client numbers don't determine which spot you spawn in
#endif
	}

	if ((ctx->team == TEAM_RED && !redFS) || (ctx->team == TEAM_BLUE && !blueFS))
		return 0;

	gentity_t *myFS;
	if (ctx->team == TEAM_RED)
		myFS = redFS;
	else if (ctx->team == TEAM_BLUE)
		myFS = blueFS;
	else
		return 0;

	float a2dDistFromFS = Distance(aa->r.currentOrigin, myFS->r.currentOrigin);
	float b2dDistFromFS = Distance(bb->r.currentOrigin, myFS->r.currentOrigin);

	if (fabs(a2dDistFromFS - b2dDistFromFS) < 20) {
		// roughly equidistant from the fs
		// try to get the ones closer to the mines so that the base can hopefully spawn there
		gentity_t *closestMines = NULL;
		float closestMinesDistance = 999999999;
		for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
			gentity_t *mines = &g_entities[i];
			if (!mines->inuse || !mines->item || mines->item->giType != IT_WEAPON || mines->item->giTag != WP_TRIP_MINE)
				continue;
			float dist = Distance(myFS->r.currentOrigin, mines->r.currentOrigin);
			if (dist < closestMinesDistance) {
				closestMines = mines;
				closestMinesDistance = dist;
			}
		}

		if (closestMines) {
			float aDistFromMines = Distance(aa->r.currentOrigin, closestMines->r.currentOrigin);
			float bDistFromMines = Distance(bb->r.currentOrigin, closestMines->r.currentOrigin);

			if (fabs(aDistFromMines - bDistFromMines) < 20) {
				// roughly equidistant from the mines too???
				// try to get the ones closest to rockets/golan so that the chase can hopefully spawn there
				gentity_t *closestGolan = NULL;
				float closestGolanDistance = 999999999;
				for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
					gentity_t *golan = &g_entities[i];
					if (!golan->inuse || !golan->item || golan->item->giType != IT_WEAPON || golan->item->giTag != WP_FLECHETTE)
						continue;
					float dist = Distance(myFS->r.currentOrigin, golan->r.currentOrigin);
					if (dist < closestGolanDistance) {
						closestGolan = golan;
						closestGolanDistance = dist;
					}
				}

				if (closestGolan) {
					gentity_t *closestRocket = NULL;
					float closestRocketDistance = 999999999;
					for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
						gentity_t *rocket = &g_entities[i];
						if (!rocket->inuse || !rocket->item || rocket->item->giType != IT_WEAPON || rocket->item->giTag != WP_ROCKET_LAUNCHER)
							continue;
						float dist = Distance(closestGolan->r.currentOrigin, rocket->r.currentOrigin);
						if (dist < closestRocketDistance) {
							closestRocket = rocket;
							closestRocketDistance = dist;
						}
					}

					vec3_t comparisonPoint = { 0 };
					if (closestRocket) {
						// try to spawn the chase up higher by taking the Z coord of the higher of golan/rocket
						VectorAverage(closestGolan->r.currentOrigin, closestRocket->r.currentOrigin, comparisonPoint);
						if (closestRocket->r.currentOrigin[2] > closestGolan->r.currentOrigin[2] + 1)
							comparisonPoint[2] = closestRocket->r.currentOrigin[2];
						else if (closestGolan->r.currentOrigin[2] > closestRocket->r.currentOrigin[2] + 1)
							comparisonPoint[2] = closestGolan->r.currentOrigin[2];
					}
					else {
						VectorCopy(closestGolan->r.currentOrigin, comparisonPoint);
					}

					float aDistFromComparisonPoint = Distance(aa->r.currentOrigin, comparisonPoint);
					float bDistFromComparisonPoint = Distance(bb->r.currentOrigin, comparisonPoint);

					if (aDistFromComparisonPoint < bDistFromComparisonPoint)
						return -1; // a first

					if (bDistFromComparisonPoint < aDistFromComparisonPoint)
						return 1;  // b first

					return 0; // order doesn't matter
				}
			}

			if (aDistFromMines < bDistFromMines)
				return -1; // a first

			if (bDistFromMines < aDistFromMines)
				return 1;  // b first
		}
		else {
			return 0; // no mines?
		}
	}

	if (a2dDistFromFS < b2dDistFromFS)
		return -1; // a first

	if (b2dDistFromFS < a2dDistFromFS)
		return 1;  // b first

	return 0; // order doesn't matter
}

int SortSpawnsToFindBaseSpawn(genericNode_t *a, genericNode_t *b, void *userData) {
	assert(a && b && userData);

	const spawnpoint_t *aa_spawn = (const spawnpoint_t *)a;
	const spawnpoint_t *bb_spawn = (const spawnpoint_t *)b;
	const gentity_t *aa = aa_spawn->ent;
	const gentity_t *bb = bb_spawn->ent;

	SpawnpointComparisonContext *ctx = userData;

	static qboolean initializedFlagstands = qfalse;
	static gentity_t *redFS = NULL, *blueFS = NULL;
	if (!initializedFlagstands) {
		for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
			gentity_t *flagstand = &g_entities[i];
			if (!Q_stricmp(flagstand->classname, "team_ctf_redflag"))
				redFS = flagstand;
			else if (!Q_stricmp(flagstand->classname, "team_ctf_blueflag"))
				blueFS = flagstand;
		}

		initializedFlagstands = qtrue;
	}

	if ((ctx->team == TEAM_RED && !redFS) || (ctx->team == TEAM_BLUE && !blueFS))
		return 0;

	gentity_t *myFS;
	if (ctx->team == TEAM_RED)
		myFS = redFS;
	else if (ctx->team == TEAM_BLUE)
		myFS = blueFS;
	else
		return 0;

	gentity_t *closestMines = NULL;
	float closestMinesDistance = 999999999;
	for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
		gentity_t *mines = &g_entities[i];
		if (!mines->inuse || !mines->item || mines->item->giType != IT_WEAPON || mines->item->giTag != WP_TRIP_MINE)
			continue;
		float dist = Distance(myFS->r.currentOrigin, mines->r.currentOrigin);
		if (dist < closestMinesDistance) {
			closestMines = mines;
			closestMinesDistance = dist;
		}
	}

	if (closestMines) {
		float aDistFromMines = Distance(aa->r.currentOrigin, closestMines->r.currentOrigin);
		float bDistFromMines = Distance(bb->r.currentOrigin, closestMines->r.currentOrigin);

		// try to account for spawns that are on the level of mines
		float aZAxisDistFromMines = fabs(aa->r.currentOrigin[2] - closestMines->r.currentOrigin[2]);
		float bZAxisDistFromMines = fabs(bb->r.currentOrigin[2] - closestMines->r.currentOrigin[2]);
		if (aZAxisDistFromMines > bZAxisDistFromMines + 175)
			aDistFromMines += aZAxisDistFromMines;
		else if (bZAxisDistFromMines > aZAxisDistFromMines + 175)
			bDistFromMines += bZAxisDistFromMines;

		if (aDistFromMines < bDistFromMines)
			return -1; // a first

		if (bDistFromMines < aDistFromMines)
			return 1;  // b first

		return 0; // order doesn't matter
	}

	return 0; // should never happen
}

int SortSpawnsToFindChaseSpawn(genericNode_t *a, genericNode_t *b, void *userData) {
	assert(a && b && userData);

	const spawnpoint_t *aa_spawn = (const spawnpoint_t *)a;
	const spawnpoint_t *bb_spawn = (const spawnpoint_t *)b;
	const gentity_t *aa = aa_spawn->ent;
	const gentity_t *bb = bb_spawn->ent;

	// try to spawn the chase higher
	if (aa->r.currentOrigin[2] > bb->r.currentOrigin[2] + 175)
		return -1; // a first
	if (bb->r.currentOrigin[2] > aa->r.currentOrigin[2] + 175)
		return 1; // a first

	SpawnpointComparisonContext *ctx = userData;

	static qboolean initializedFlagstands = qfalse;
	static gentity_t *redFS = NULL, *blueFS = NULL;
	if (!initializedFlagstands) {
		for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
			gentity_t *flagstand = &g_entities[i];
			if (!Q_stricmp(flagstand->classname, "team_ctf_redflag"))
				redFS = flagstand;
			else if (!Q_stricmp(flagstand->classname, "team_ctf_blueflag"))
				blueFS = flagstand;
		}

		initializedFlagstands = qtrue;
	}

	if ((ctx->team == TEAM_RED && !redFS) || (ctx->team == TEAM_BLUE && !blueFS))
		return 0;

	gentity_t *myFS;
	if (ctx->team == TEAM_RED)
		myFS = redFS;
	else if (ctx->team == TEAM_BLUE)
		myFS = blueFS;
	else
		return 0;

	gentity_t *closestGolan = NULL;
	float closestGolanDistance = 999999999;
	for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
		gentity_t *golan = &g_entities[i];
		if (!golan->inuse || !golan->item || golan->item->giType != IT_WEAPON || golan->item->giTag != WP_FLECHETTE)
			continue;
		float dist = Distance(myFS->r.currentOrigin, golan->r.currentOrigin);
		if (dist < closestGolanDistance) {
			closestGolan = golan;
			closestGolanDistance = dist;
		}
	}

	if (closestGolan) {
		gentity_t *closestRocket = NULL;
		float closestRocketDistance = 999999999;
		for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
			gentity_t *rocket = &g_entities[i];
			if (!rocket->inuse || !rocket->item || rocket->item->giType != IT_WEAPON || rocket->item->giTag != WP_ROCKET_LAUNCHER)
				continue;
			float dist = Distance(closestGolan->r.currentOrigin, rocket->r.currentOrigin);
			if (dist < closestRocketDistance) {
				closestRocket = rocket;
				closestRocketDistance = dist;
			}
		}

		vec3_t comparisonPoint = { 0 };
		if (closestRocket)
			VectorAverage(closestGolan->r.currentOrigin, closestRocket->r.currentOrigin, comparisonPoint);
		else
			VectorCopy(closestGolan->r.currentOrigin, comparisonPoint);

		float aDistFromComparisonPoint = Distance(aa->r.currentOrigin, comparisonPoint);
		float bDistFromComparisonPoint = Distance(bb->r.currentOrigin, comparisonPoint);

		if (aDistFromComparisonPoint < bDistFromComparisonPoint)
			return -1; // a first
		if (bDistFromComparisonPoint < aDistFromComparisonPoint)
			return 1;  // b first

		return 0; // order doesn't matter
	}

	return 0; // should never happen
}

typedef struct {
	node_t			node;
	gentity_t		*redSpawn;
	gentity_t		*blueSpawn;
	float			dist2dFromFs;
} spawnPair_t;

int SortSpawnPairsByDistanceToFS(genericNode_t *a, genericNode_t *b, void *userData) {
	assert(a && b);

	const spawnPair_t *aa = (const spawnPair_t *)a;
	const spawnPair_t *bb = (const spawnPair_t *)b;

	if (aa->dist2dFromFs > bb->dist2dFromFs)
		return -1; // a first

	if (aa->dist2dFromFs < bb->dist2dFromFs)
		return 1; // b first

	return 0; // order doesn't matter
}

int SortSpawnPairsRandomly(genericNode_t *a, genericNode_t *b, void *userData) {
	if (rand() % 2)
		return 1;
	return -1;
}

int CompareInitialSpawnPriority(genericNode_t *a, genericNode_t *b, void *userData) {
	assert(a && b && userData);

	const spawnpoint_t *aa_spawn = (const spawnpoint_t *)a;
	const spawnpoint_t *bb_spawn = (const spawnpoint_t *)b;
	const gentity_t *aa = aa_spawn->ent;
	const gentity_t *bb = bb_spawn->ent;

	SpawnpointComparisonContext *ctx = userData;
	if (level.initialSpawnsSet[ctx->team] && ctx->pos) {
		if (aa->preferredSpawnPos == ctx->pos && bb->preferredSpawnPos != ctx->pos) {
			return -1; // a first
		}
		else if (aa->preferredSpawnPos != ctx->pos && bb->preferredSpawnPos == ctx->pos) {
			return 1; // b first
		}
		else {
			// randomize the order
			if (rand() % 2)
				return 1;
			return -1;
		}
	}

	const int aPriority = aa_spawn->priority;
	const int bPriority = bb_spawn->priority;

	if (aPriority < bPriority)
		return -1; // a first

	if (bPriority > aPriority)
		return 1;  // b first

	static qboolean initializedFlagstands = qfalse;
	static gentity_t *redFS = NULL, *blueFS = NULL;
	if (!initializedFlagstands) {
		for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
			gentity_t *flagstand = &g_entities[i];
			if (!Q_stricmp(flagstand->classname, "team_ctf_redflag"))
				redFS = flagstand;
			else if (!Q_stricmp(flagstand->classname, "team_ctf_blueflag"))
				blueFS = flagstand;
		}

		initializedFlagstands = qtrue;
#if 1
		srand(time(NULL)); // so that client numbers don't determine which spot you spawn in
#endif
	}

	if ((ctx->team == TEAM_RED && !redFS) || (ctx->team == TEAM_BLUE && !blueFS))
		return 0;

	gentity_t *myFS;
	if (ctx->team == TEAM_RED)
		myFS = redFS;
	else if (ctx->team == TEAM_BLUE)
		myFS = blueFS;
	else
		return 0;

	float a2dDistFromFS = Distance(aa->r.currentOrigin, myFS->r.currentOrigin);
	float b2dDistFromFS = Distance(bb->r.currentOrigin, myFS->r.currentOrigin);

	if (fabs(a2dDistFromFS - b2dDistFromFS) < 20) {
#if 1
	// randomize the order of spots that are roughly equidistant from the fs
		if (rand() % 2)
			return 1;
		return -1;
#else
		// treat spots that are roughly equidistant from the fs as equal
		return 0;
#endif
	}

	if (a2dDistFromFS < b2dDistFromFS)
		return -1; // a first

	if (a2dDistFromFS > b2dDistFromFS)
		return 1;  // b first

	return 0; // order doesn't matter
}

// puts closer ones first
int CompareSpawns(genericNode_t *a, genericNode_t *b, void *userData) {
	const spawnpoint_t *aa_spawn = (const spawnpoint_t *)a;
	const spawnpoint_t *bb_spawn = (const spawnpoint_t *)b;
	const gentity_t *aa = aa_spawn->ent;
	const gentity_t *bb = bb_spawn->ent;


	vec3_t *comparisonPoint = (vec3_t *)userData;

	float a2dDistFromComparisonPoint = Distance2D(aa->r.currentOrigin, *comparisonPoint);
	float b2dDistFromComparisonPoint = Distance2D(bb->r.currentOrigin, *comparisonPoint);

	if (a2dDistFromComparisonPoint < b2dDistFromComparisonPoint)
		return -1; // a first

	if (a2dDistFromComparisonPoint > b2dDistFromComparisonPoint)
		return 1;  // b first

	return 0; // order doesn't matter
}

// puts farther away ones first
int CompareSpawnsReverseOrder(genericNode_t *a, genericNode_t *b, void *userData) {
	const spawnpoint_t *aa_spawn = (const spawnpoint_t *)a;
	const spawnpoint_t *bb_spawn = (const spawnpoint_t *)b;
	const gentity_t *aa = aa_spawn->ent;
	const gentity_t *bb = bb_spawn->ent;


	vec3_t *comparisonPoint = (vec3_t *)userData;

	float a2dDistFromComparisonPoint = Distance2D(aa->r.currentOrigin, *comparisonPoint);
	float b2dDistFromComparisonPoint = Distance2D(bb->r.currentOrigin, *comparisonPoint);

	if (a2dDistFromComparisonPoint > b2dDistFromComparisonPoint)
		return -1; // a first

	if (a2dDistFromComparisonPoint < b2dDistFromComparisonPoint)
		return 1;  // b first

	return 0; // order doesn't matter
}

qboolean TooCloseToDroppedFlag(gentity_t *spawnPoint) {
	for (int i = MAX_CLIENTS; i < MAX_GENTITIES; ++i) {
		gentity_t *flag = &g_entities[i];

		if (!flag->inuse || !flag->item)
			continue;
		
		if (flag->item->giTag != PW_REDFLAG && flag->item->giTag != PW_BLUEFLAG && flag->item->giTag != PW_NEUTRALFLAG)
			continue;
		
		if (!(flag->flags & FL_DROPPED_ITEM))
			continue;

		int flagDroppedTime = flag->nextthink - 30000;
		if (level.time > flagDroppedTime + Com_Clampi(1000, 30000, g_droppedFlagSpawnProtectionDuration.integer))
			continue;

		float radiusSquared = (float)(g_droppedFlagSpawnProtectionRadius.integer * g_droppedFlagSpawnProtectionRadius.integer);
		if (DistanceSquared(spawnPoint->r.currentOrigin, flag->r.currentOrigin) <= radiusSquared && trap_InPVS(spawnPoint->r.currentOrigin, flag->r.currentOrigin))
			return qtrue;
	}

	return qfalse;
}

qboolean TooCloseToAlliedFlagCarrier(gentity_t *spawnPoint, int team) {
	gentity_t *fc = NULL;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *thisGuy = &g_entities[i];
		if (!thisGuy->inuse || !thisGuy->client || thisGuy->client->sess.sessionTeam != team ||
			IsRacerOrSpectator(thisGuy) || thisGuy->health <= 0 || !HasFlag(thisGuy) || thisGuy->client->ps.fallingToDeath) {
			continue;
		}
		fc = thisGuy;
		break;
	}

	if (!fc || Distance(spawnPoint->r.currentOrigin, fc->client->ps.origin) > 512 || !trap_InPVS(spawnPoint->r.currentOrigin, fc->client->ps.origin)) {
		return qfalse;
	}

	return qtrue;
}

qboolean TooCloseToWhereJustKilledByEnemy(gentity_t *spawnPoint, gclient_t *client) {
	assert(client);

	if (client->pers.lastKilledByEnemyTime && level.time - client->pers.lastKilledByEnemyTime < 2500) {
		if (Distance(client->pers.lastKilledByEnemyLocation, spawnPoint->r.currentOrigin) < g_killedAntiHannahSpawnRadius.value &&
			trap_InPVS(client->pers.lastKilledByEnemyLocation, spawnPoint->r.currentOrigin)) {
			return qtrue;
		}
	}

	return qfalse;
}

static void SpawnDebugPrintf(const char *msg, ...) {
	if (d_debugSpawns.integer) {
		va_list		argptr;
		char		text[4096] = { 0 };

		va_start(argptr, msg);
		vsnprintf(text, sizeof(text), msg, argptr);
		va_end(argptr);

		G_Printf("%s", text);
	}
}

static void GetSpawnPointNickname(gentity_t *spawn, char *dest, size_t destSize, qboolean appendCoords) {
	assert(spawn && dest && destSize);
	*dest = '\0';

#if 0
	if (!d_debugSpawns.integer)
		return; // just save overhead since we won't be printing them to the log anyway i guess
#endif

	if (d_debugSpawns.integer == 2)
		Com_Printf("GetSpawnPointNickname: spawn->spawnname is %s\n", spawn->spawnname);

	if (VALIDSTRING(spawn->spawnname)) {
		Q_strncpyz(dest, spawn->spawnname, destSize);
		return;
	}

	float lowestDistance = 999999;
	gentity_t *lowestDistanceEnt = NULL;
	for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
		gentity_t *nearbyEnt = &g_entities[i];
		if (!nearbyEnt->inuse || !nearbyEnt->item || nearbyEnt->item->giType == IT_AMMO)
			continue;
		if (!trap_InPVS(nearbyEnt->r.currentOrigin, spawn->r.currentOrigin))
			continue;
		float dist = Distance(spawn->r.currentOrigin, nearbyEnt->r.currentOrigin);
		if (dist < lowestDistance - 0.0001f) {
			lowestDistance = dist;
			lowestDistanceEnt = nearbyEnt;
		}
	}
	
	if (lowestDistanceEnt) {
		if (appendCoords)
			Com_sprintf(dest, destSize, "%s {%d %d %d}", lowestDistanceEnt->classname, (int)spawn->r.currentOrigin[0], (int)spawn->r.currentOrigin[1], (int)spawn->r.currentOrigin[2]);
		else
			Com_sprintf(dest, destSize, "%s", lowestDistanceEnt->classname);
	}
}

// set the initial spawnpoints for each position
void SetInitialSpawns(void) {
	memset(level.initialSpawnsSet, 0, sizeof(level.initialSpawnsSet));
	if (!g_presetInitialSpawns.integer || g_gametype.integer != GT_CTF)
		return;

	// check for 4v4
	if (g_numRedPlayers.integer != 4 || g_numBluePlayers.integer != 4)
		return;

	// check that mines/golan exist
	int gotMines = 0, gotGolan = 0;
	for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
		gentity_t *thisItem = &g_entities[i];
		if (!thisItem->inuse || !thisItem->item || thisItem->item->giType != IT_WEAPON)
			continue;

		if (thisItem->item->giTag == WP_TRIP_MINE)
			++gotMines;
		else if (thisItem->item->giTag == WP_FLECHETTE)
			++gotGolan;
	}
	if (gotMines < 2 || gotGolan < 2)
		return;

	for (team_t team = TEAM_RED; team <= TEAM_BLUE; team++) {
		list_t possibleSpawnsList = { 0 };

		char *classname = (team == TEAM_RED) ? "team_CTF_redplayer" : "team_CTF_blueplayer";

		for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!VALIDSTRING(thisEnt->classname) || Q_stricmp(thisEnt->classname, classname))
				continue;

			spawnpoint_t *add = ListAdd(&possibleSpawnsList, sizeof(spawnpoint_t));
			add->ent = thisEnt;
		}

		if (possibleSpawnsList.size >= 4) {
			SpawnpointComparisonContext ctx = { 0 };
			ctx.team = team;

			// trim list to the 4 spawns closest to the fs (using certain tiebreaker procedures for equidistant spawns)
			ListSort(&possibleSpawnsList, SortSpawnsByDistanceToFS, &ctx);
			ListTrim(&possibleSpawnsList, 4, qfalse);

			// get base spawn closest to mines
			ListSort(&possibleSpawnsList, SortSpawnsToFindBaseSpawn, &ctx);
			gentity_t *baseSpawn = ((spawnpoint_t *)possibleSpawnsList.head)->ent;

			// get chase spawn closest to midpoint of rockets+golan
			ListTrim(&possibleSpawnsList, 3, qtrue);
			ListSort(&possibleSpawnsList, SortSpawnsToFindChaseSpawn, &ctx);
			gentity_t *chaseSpawn = ((spawnpoint_t *)possibleSpawnsList.head)->ent;

			// the remaining 2 are the offense spawns
			ListTrim(&possibleSpawnsList, 2, qtrue);
			gentity_t *offense1Spawn = ((spawnpoint_t *)possibleSpawnsList.head)->ent;
			gentity_t *offense2Spawn = ((spawnpoint_t *)(((node_t *)possibleSpawnsList.head)->next))->ent;

			baseSpawn->preferredSpawnPos = CTFPOSITION_BASE;
			chaseSpawn->preferredSpawnPos = CTFPOSITION_CHASE;
			offense1Spawn->preferredSpawnPos = CTFPOSITION_OFFENSE;
			offense2Spawn->preferredSpawnPos = CTFPOSITION_OFFENSE;
			level.initialSpawnsSet[team] = qtrue;
			SpawnDebugPrintf("Initial spawns set for team %d: %d, %d, %d, %d\n", team, baseSpawn - g_entities, chaseSpawn - g_entities, offense1Spawn - g_entities, offense2Spawn - g_entities);
		}

		ListClear(&possibleSpawnsList);
	}
}

#define INITIALRESPAWN_SHITTYMIRRORING_THRESHOLD	(33)
// set the initial respawns that will be used for offense players who sk within the first few seconds
void SetInitialRespawns(void) {
	memset(level.initialRespawn, 0, sizeof(level.initialRespawn));
	if (!g_presetInitialSpawns.integer || g_gametype.integer != GT_CTF)
		return;

	// check for 4v4
	if (g_numRedPlayers.integer != 4 || g_numBluePlayers.integer != 4)
		return;
	
	qboolean requireManuallyDefinedInitialRespawn = qfalse;
	if (level.numManuallyDefinedInitialRespawns[TEAM_RED] >= 2 && level.numManuallyDefinedInitialRespawns[TEAM_BLUE] >= 2)
		requireManuallyDefinedInitialRespawn = qtrue;
	else if (level.numManuallyDefinedInitialRespawns[TEAM_RED] || level.numManuallyDefinedInitialRespawns[TEAM_BLUE] >= 2)
		Com_Printf("SetInitialRespawns: map has some manually defined initial respawns, but not 2 on each team! Using automatic detection instead.\n");

	gentity_t *redFsEnt = NULL, *blueFsEnt = NULL;
	gentity_t temp;
	VectorCopy(vec3_origin, temp.r.currentOrigin);
	redFsEnt = G_ClosestEntity(&temp, isRedFlagstand);
	blueFsEnt = G_ClosestEntity(&temp, isBlueFlagstand);
	if (!redFsEnt || !blueFsEnt)
		return;

	// get a list of mirrored spawns (pairs of similar spawnpoints on each team)
	list_t spawnPairsList = { 0 };
	for (int i = 0; i <= ENTITYNUM_MAX_NORMAL; i++) {
		gentity_t *redSpawn = &g_entities[i];
		if (!redSpawn->inuse || !VALIDSTRING(redSpawn->classname))
			continue;
		if (Q_stricmp(redSpawn->classname, "team_CTF_redspawn"))
			continue;
		if (requireManuallyDefinedInitialRespawn && !redSpawn->isManuallyDefinedInitialRespawn)
			continue;

		const float redDeltaXY = fabs(redFsEnt->r.currentOrigin[0] - redSpawn->r.currentOrigin[0]) + fabs(redFsEnt->r.currentOrigin[1] - redSpawn->r.currentOrigin[1]);
		const float redDeltaZ = fabs(redFsEnt->r.currentOrigin[2] - redSpawn->r.currentOrigin[2]);
		
		// we have a red spawn
		// try to find a matching blue spawn
		gentity_t *matchingBlueSpawn = NULL;
		for (int j = MAX_CLIENTS; j < ENTITYNUM_MAX_NORMAL; j++) {
			gentity_t *blueSpawn = &g_entities[j];
			if (!blueSpawn->inuse || !VALIDSTRING(blueSpawn->classname) || Q_stricmp(blueSpawn->classname, "team_CTF_bluespawn"))
				continue;
			if (requireManuallyDefinedInitialRespawn && !blueSpawn->isManuallyDefinedInitialRespawn)
				continue;

			// we have a blue spawn
			// check whether it's roughly the same distance from the blue fs as the red spawn is to the red fs
			const float blueDeltaXY = fabs(blueFsEnt->r.currentOrigin[0] - blueSpawn->r.currentOrigin[0]) + fabs(blueFsEnt->r.currentOrigin[1] - blueSpawn->r.currentOrigin[1]);
			const float blueDeltaZ = fabs(blueFsEnt->r.currentOrigin[2] - blueSpawn->r.currentOrigin[2]);

			const float diffXY = fabs(redDeltaXY - blueDeltaXY);
			const float diffZ = fabs(redDeltaZ - blueDeltaZ);

			char redNickname[128] = { 0 }, blueNickname[128] = { 0 };
			GetSpawnPointNickname(redSpawn, redNickname, sizeof(redNickname), qfalse);
			GetSpawnPointNickname(blueSpawn, blueNickname, sizeof(blueNickname), qfalse);
			
			if (Q_stricmp(redNickname, blueNickname))
				continue; // sanity check: make sure the nicknames are the same

			if (diffXY > INITIALRESPAWN_SHITTYMIRRORING_THRESHOLD || diffZ > INITIALRESPAWN_SHITTYMIRRORING_THRESHOLD) {
				if (diffXY < 100 && diffZ < 100) {
					GetSpawnPointNickname(redSpawn, redNickname, sizeof(redNickname), qtrue);
					GetSpawnPointNickname(blueSpawn, blueNickname, sizeof(blueNickname), qtrue);
					SpawnDebugPrintf("Near-miss spawn pair of %s %d + %s %d: diffs %g / %g (r %g / b %g --- r %g / b %g)\n",
						redNickname, redSpawn - g_entities, blueNickname, blueSpawn - g_entities,
						diffXY, diffZ, redDeltaXY, blueDeltaXY, redDeltaZ, blueDeltaZ);
				}
				continue;
			}

			matchingBlueSpawn = blueSpawn;
			break;
		}

		if (matchingBlueSpawn) {
			spawnPair_t *add = ListAdd(&spawnPairsList, sizeof(spawnPair_t));
			add->redSpawn = redSpawn;
			add->blueSpawn = matchingBlueSpawn;
			add->dist2dFromFs = (Distance2D(redSpawn->r.currentOrigin, redFsEnt->r.currentOrigin) + Distance2D(matchingBlueSpawn->r.currentOrigin, blueFsEnt->r.currentOrigin)) / 2;
			char redNickname[128] = { 0 }, blueNickname[128] = { 0 };
			GetSpawnPointNickname(redSpawn, redNickname, sizeof(redNickname), qtrue);
			GetSpawnPointNickname(matchingBlueSpawn, blueNickname, sizeof(blueNickname), qtrue);
			SpawnDebugPrintf("Got spawn pair (%s %d + %s %d) with dist %d\n", redNickname, redSpawn - g_entities, blueNickname, matchingBlueSpawn - g_entities, (int)add->dist2dFromFs);
		}
	}

	if (spawnPairsList.size < 2) {
		ListClear(&spawnPairsList);
		return;
	}

	SpawnDebugPrintf("Spawn pairs list size is %d\n", spawnPairsList.size);

	// sort so that farther ones from the fs are at the beginning of the list
	ListSort(&spawnPairsList, SortSpawnPairsByDistanceToFS, NULL);

	// trim off the 50% closest ones to fs, if detecting automatically
	if (!requireManuallyDefinedInitialRespawn && spawnPairsList.size >= 4) {
		ListTrim(&spawnPairsList, spawnPairsList.size / 2, qfalse);
		SpawnDebugPrintf("After trimming, spawn pairs list size is %d:\n", spawnPairsList.size);
	}
	else {
		SpawnDebugPrintf("Spawn pairs list size is %d:\n", spawnPairsList.size);
	}
	iterator_t iter;
	ListIterate(&spawnPairsList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		spawnPair_t *element = IteratorNext(&iter);
		char redNickname[128] = { 0 }, blueNickname[128] = { 0 };
		GetSpawnPointNickname(element->redSpawn, redNickname, sizeof(redNickname), qtrue);
		GetSpawnPointNickname(element->blueSpawn, blueNickname, sizeof(blueNickname), qtrue);
		SpawnDebugPrintf("%s %d + %s %d with dist %d\n", redNickname, element->redSpawn - g_entities, blueNickname, element->blueSpawn - g_entities, (int)element->dist2dFromFs);
	}

	// randomize the order of the remaining ones
	ListSort(&spawnPairsList, SortSpawnPairsRandomly, NULL);

	// set the first respawn
	level.initialRespawn[TEAM_RED][0] = ((spawnPair_t *)spawnPairsList.head)->redSpawn;
	level.initialRespawn[TEAM_BLUE][0] = ((spawnPair_t *)spawnPairsList.head)->blueSpawn;
	{
		char redNickname[128] = { 0 }, blueNickname[128] = { 0 };
		GetSpawnPointNickname(level.initialRespawn[TEAM_RED][0], redNickname, sizeof(redNickname), qtrue);
		GetSpawnPointNickname(level.initialRespawn[TEAM_BLUE][0], blueNickname, sizeof(blueNickname), qtrue);
		SpawnDebugPrintf("First initial respawn is %s %d + %s %d\n", redNickname, level.initialRespawn[TEAM_RED][0] - g_entities, blueNickname, level.initialRespawn[TEAM_BLUE][0] - g_entities);
	}

	// set the second respawn
	level.initialRespawn[TEAM_RED][1] = ((spawnPair_t *)(((node_t *)spawnPairsList.head)->next))->redSpawn;
	level.initialRespawn[TEAM_BLUE][1] = ((spawnPair_t *)(((node_t *)spawnPairsList.head)->next))->blueSpawn;
	{
		char redNickname[128] = { 0 }, blueNickname[128] = { 0 };
		GetSpawnPointNickname(level.initialRespawn[TEAM_RED][1], redNickname, sizeof(redNickname), qtrue);
		GetSpawnPointNickname(level.initialRespawn[TEAM_BLUE][1], blueNickname, sizeof(blueNickname), qtrue);
		SpawnDebugPrintf("Second initial respawn is %s %d + %s %d\n", redNickname, level.initialRespawn[TEAM_RED][1] - g_entities, blueNickname, level.initialRespawn[TEAM_BLUE][1] - g_entities);
	}

	ListClear(&spawnPairsList);
}

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define	MAX_TEAM_SPAWN_POINTS	32
#define SPAWN_SPAM_PROTECT_TIME		(5000)
gentity_t *SelectRandomTeamSpawnPoint( gclient_t *client, int teamstate, team_t team, int siegeClass ) {
	assert(client);
	int			selection;
	char		*classname;
	qboolean	mustBeEnabled = qfalse;

	if (g_gametype.integer == GT_SIEGE)
	{
		if (team == SIEGETEAM_TEAM1)
		{
			classname = "info_player_siegeteam1";
		}
		else
		{
			classname = "info_player_siegeteam2";
		}

		mustBeEnabled = qtrue; //siege spawn points need to be "enabled" to be used (because multiple spawnpoint sets can be placed at once)
	}
	else
	{
		if (teamstate == TEAM_BEGIN) {
			if (team == TEAM_RED)
				classname = "team_CTF_redplayer";
			else if (team == TEAM_BLUE)
				classname = "team_CTF_blueplayer";
			else
				return NULL;
		} else {
			if (team == TEAM_RED)
				classname = "team_CTF_redspawn";
			else if (team == TEAM_BLUE)
				classname = "team_CTF_bluespawn";
			else
				return NULL;
		}
	}

	gentity_t *spawnMeNearThisEntity = NULL;
	int boost = (client && client->account && g_boost.integer) ? client->account->flags & (ACCOUNTFLAG_BOOST_SPAWNFCBOOST | ACCOUNTFLAG_BOOST_SPAWNGERBOOST) : 0;
	qboolean softSpawnOverride = qfalse; // only rule out the 33% farthest spawns rather than the 50% farthest spawns

	ctfPosition_t pos = CTFPOSITION_UNKNOWN;
	if (boost) {
		pos = GetRemindedPosOrDeterminedPos(&g_entities[client - level.clients]);

		if ((boost & ACCOUNTFLAG_BOOST_SPAWNFCBOOST) && level.time - level.startTime >= 5000 && pos == CTFPOSITION_BASE) { // boost: spawn base near fc
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *thisGuy = &g_entities[i];
				if (!thisGuy->inuse || !thisGuy->client || thisGuy->client == client || thisGuy->client->sess.sessionTeam != client->sess.sessionTeam ||
					IsRacerOrSpectator(thisGuy) || thisGuy->health <= 0 || !HasFlag(thisGuy) || thisGuy->client->ps.fallingToDeath)
					continue;

				float loc = GetCTFLocationValue(thisGuy);
				if (loc <= 0.55f) {
					spawnMeNearThisEntity = thisGuy;
					break;
				}
			}

			if (spawnMeNearThisEntity && g_spawnboost_losIdealDistance.integer > 0) {
				gentity_t *spawnBoostEnt = GetSpawnFcBoostLocation(client, spawnMeNearThisEntity);
				if (spawnBoostEnt)
					return spawnBoostEnt;
			}
		}

		if ((boost & ACCOUNTFLAG_BOOST_SPAWNGERBOOST) && !spawnMeNearThisEntity && level.time - level.startTime >= 15000 && pos == CTFPOSITION_BASE) {
			// boost: help base ger by spawning near mid if everyone is in the enemy base, including enemy fc
			flagStatus_t myFlagStatus = client->sess.sessionTeam == TEAM_RED ? teamgame.redStatus : teamgame.blueStatus;
			if (myFlagStatus != FLAG_ATBASE) {
				qboolean someoneOnOurSideOfMap = qfalse;
				for (int i = 0; i < MAX_CLIENTS; i++) {
					gentity_t *thisGuy = &g_entities[i];
					if (!thisGuy->inuse || !thisGuy->client || thisGuy->client == client || IsRacerOrSpectator(thisGuy) || thisGuy->health <= 0 || thisGuy->client->ps.fallingToDeath)
						continue;

					ctfRegion_t region = GetCTFRegion(thisGuy);
					if (thisGuy->client->sess.sessionTeam == client->sess.sessionTeam) {
						if (region == CTFREGION_FLAGSTAND || region == CTFREGION_BASE) {
							someoneOnOurSideOfMap = qtrue;
							break;
						}
					}
					else if (thisGuy->client->sess.sessionTeam == OtherTeam(client->sess.sessionTeam)) {
						if (region == CTFREGION_ENEMYFLAGSTAND || region == CTFREGION_ENEMYBASE) {
							someoneOnOurSideOfMap = qtrue;
							break;
						}
					}
				}

				if (!someoneOnOurSideOfMap) {
					static qboolean initialized = qfalse;
					static gentity_t *redFsEnt = NULL, *blueFsEnt = NULL;
					if (!initialized) {
						initialized = qtrue;
						gentity_t temp;
						VectorCopy(vec3_origin, temp.r.currentOrigin);
						redFsEnt = G_ClosestEntity(&temp, isRedFlagstand);
						blueFsEnt = G_ClosestEntity(&temp, isBlueFlagstand);
					}

					if (redFsEnt && blueFsEnt)
						spawnMeNearThisEntity = client->sess.sessionTeam == TEAM_RED ? blueFsEnt : redFsEnt;
				}
			}
			else {
				gentity_t *myFcOnMySideOfMap = GetFC(client->sess.sessionTeam, &g_entities[client - level.clients], qtrue, 0.55f);
				if (!myFcOnMySideOfMap) {
					static qboolean initialized = qfalse;
					static gentity_t *redFsEnt = NULL, *blueFsEnt = NULL;
					if (!initialized) {
						initialized = qtrue;
						gentity_t temp;
						VectorCopy(vec3_origin, temp.r.currentOrigin);
						redFsEnt = G_ClosestEntity(&temp, isRedFlagstand);
						blueFsEnt = G_ClosestEntity(&temp, isBlueFlagstand);
					}
					spawnMeNearThisEntity = client->sess.sessionTeam == TEAM_RED ? redFsEnt : blueFsEnt;
					softSpawnOverride = qtrue;
				}
			}
		}
	}

	if (g_gametype.integer == GT_CTF && !ClientIsRacerOrSpectator(client)) {
		if (teamstate == TEAM_BEGIN) {
			list_t possibleSpawnsList = { 0 };

			// create the initial list of possible spawns
			qboolean gotAnyWithPrioritySet = qfalse, gotAnyWithPriorityNotSet = qfalse;
			for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
				gentity_t *thisEnt = &g_entities[i];
				if (!VALIDSTRING(thisEnt->classname) || Q_stricmp(thisEnt->classname, classname))
					continue;

				spawnpoint_t *add = ListAdd(&possibleSpawnsList, sizeof(spawnpoint_t));
				add->ent = thisEnt;
				if (thisEnt->initialspawnpriority > 0) {
					add->priority = thisEnt->initialspawnpriority;
					gotAnyWithPrioritySet = qtrue;
				}
				else {
					add->priority = 999999;
					gotAnyWithPriorityNotSet = qtrue;
				}
			}

			if (gotAnyWithPrioritySet && gotAnyWithPriorityNotSet)
				SpawnDebugPrintf("Warning: idiot mapmaker used initial spawns with initialspawnpriority both set and not set!\n");

			if (possibleSpawnsList.size >= 1) {
				SpawnpointComparisonContext ctx = { 0 };
				ctx.team = team;
				ctx.pos = client->sess.remindPositionOnMapChange.valid && client->sess.remindPositionOnMapChange.pos ? client->sess.remindPositionOnMapChange.pos : 0;
				ListSort(&possibleSpawnsList, CompareInitialSpawnPriority, &ctx);

				iterator_t iter;
				ListIterate(&possibleSpawnsList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					spawnpoint_t *element = IteratorNext(&iter);
					if (SpotWouldTelefrag(element->ent))
						continue;

					// this one is the first that won't telefrag; choose it
					gentity_t *returnSpawnPoint = element->ent;
					ListClear(&possibleSpawnsList);
					return returnSpawnPoint;
				}

				// if we got here, it's impossible to spawn at an initial spawnpoint without telefragging someone
				// just spawn at the first spot i guess
				ListIterate(&possibleSpawnsList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					spawnpoint_t *element = IteratorNext(&iter);
					gentity_t *returnSpawnPoint = element->ent;
					ListClear(&possibleSpawnsList);
					return returnSpawnPoint;
				}
			}
			else { // no possible spawns???
				SpawnDebugPrintf("No possible spawns!\n");
				ListClear(&possibleSpawnsList);
				return G_Find(NULL, FOFS(classname), classname);
			}
		}
		else {
			char lastSpawnName[128] = { 0 };
			if (client->pers.lastSpawnPoint)
				GetSpawnPointNickname(client->pers.lastSpawnPoint, lastSpawnName, sizeof(lastSpawnName), qtrue);
			else
				Q_strncpyz(lastSpawnName, "none", sizeof(lastSpawnName));
			SpawnDebugPrintf("Spawn logic at %d (%d) for %splayer %d (%s) with last spawnpoint %s trying for classname %s with lastSpawnTime %d, lastKiller %d, lastKilledByEnemyTime %d, lastKilledByEnemyLocation %d %d %d\n",
				level.time,
				level.time - level.startTime,
				boost && spawnMeNearThisEntity ? "boosted " : "",
				client - level.clients,
				client->pers.netname,
				lastSpawnName,
				classname,
				client->pers.lastSpawnTime,
				client->pers.lastKiller ? client->pers.lastKiller - g_entities : -1,
				client->pers.lastKilledByEnemyTime,
				(int)client->pers.lastKilledByEnemyLocation[0],
				(int)client->pers.lastKilledByEnemyLocation[1],
				(int)client->pers.lastKilledByEnemyLocation[2]);
			list_t possibleSpawnsList = { 0 };

			// create the initial list of possible spawns
			for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
				gentity_t *thisEnt = &g_entities[i];
				if (!VALIDSTRING(thisEnt->classname) || Q_stricmp(thisEnt->classname, classname))
					continue;

				spawnpoint_t *add = ListAdd(&possibleSpawnsList, sizeof(spawnpoint_t));
				add->ent = thisEnt;
				GetSpawnPointNickname(thisEnt, add->nickname, sizeof(add->nickname), qtrue);

				SpawnDebugPrintf("Adding %s to possible spawns list", add->nickname);
				if (client->pers.lastSpawnPoint)
					SpawnDebugPrintf(" (2D distance from last spawn point (%s): %f)\n", lastSpawnName, Distance2D(thisEnt->r.currentOrigin, client->pers.lastSpawnPoint->r.currentOrigin));
				else
					SpawnDebugPrintf("\n");
			}

			if (possibleSpawnsList.size <= 0) { // no possible spawns???
				SpawnDebugPrintf("No possible spawns!\n");
				ListClear(&possibleSpawnsList);
				return G_Find(NULL, FOFS(classname), classname);
			}

			// remove non-whitelisted spawn points, if applicable
			if (!(boost && spawnMeNearThisEntity) && g_selfKillSpawnSpamProtection.integer && client->pers.lastSpawnPoint &&
				level.time - client->pers.lastSpawnTime < SPAWN_SPAM_PROTECT_TIME && VALIDSTRING(client->pers.lastSpawnPoint->nextspawns) &&
				(!client->pers.lastKiller ||
					client->pers.lastKiller - g_entities == client - level.clients ||
					client->pers.lastKiller - g_entities == ENTITYNUM_NONE ||
					client->pers.lastKiller - g_entities == ENTITYNUM_WORLD)) {

				iterator_t iter;
				ListIterate(&possibleSpawnsList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					spawnpoint_t *element = (spawnpoint_t *)iter.current;
					genericNode_t *nextNode = iter.current ? ((node_t *)iter.current)->next : NULL; // store the next node before removal

					if (!VALIDSTRING(element->ent->spawnname) || !stristr(client->pers.lastSpawnPoint->nextspawns, element->ent->spawnname)) {
						SpawnDebugPrintf("Removing %s from possible spawns list since it isn't whitelisted by last spawn (%s)\n", element->nickname, lastSpawnName);
						IteratorRemove(&iter); // remove the current node
						iter.current = nextNode; // manually set iter.current to the next node
					}
					else {
						if (iter.current != nextNode)
							IteratorNext(&iter); // only call IteratorNext if we haven't removed the current node
					}
				}
			}


			// if we DON'T have a whitelist, then delete the 50% of spawns that are closest to our last one
			if (!(boost && spawnMeNearThisEntity) && g_selfKillSpawnSpamProtection.integer && client->pers.lastSpawnPoint &&
				level.time - client->pers.lastSpawnTime < SPAWN_SPAM_PROTECT_TIME && !VALIDSTRING(client->pers.lastSpawnPoint->nextspawns) &&
				(!client->pers.lastKiller ||
					client->pers.lastKiller - g_entities == client - level.clients ||
					client->pers.lastKiller - g_entities == ENTITYNUM_NONE ||
					client->pers.lastKiller - g_entities == ENTITYNUM_WORLD)) {
				ListSort(&possibleSpawnsList, CompareSpawns, client->pers.lastSpawnPoint->r.currentOrigin);

#if 1
				const int goalSize = Com_Clampi(1, MAX_TEAM_SPAWN_POINTS, possibleSpawnsList.size / 2); // e.g. 7 total spawns ==> goalSize of 3 (remove 4 spawns)
#else
				const int goalSize = Com_Clampi(1, MAX_TEAM_SPAWN_POINTS, possibleSpawnsList.size - (possibleSpawnsList.size / 2)); // e.g. 7 total spawns ==> goalSize of 4 (remove 3 spawns)
#endif
				while (possibleSpawnsList.size > goalSize) {
					gentity_t *headEnt = ((spawnpoint_t *)(possibleSpawnsList.head))->ent;
					char *headNickname = ((spawnpoint_t *)(possibleSpawnsList.head))->nickname;
					SpawnDebugPrintf("No whitelist; removing %s from the list based on 2D distance from last spawn point (%s) %f\n", headNickname, lastSpawnName, Distance2D(headEnt->r.currentOrigin, client->pers.lastSpawnPoint->r.currentOrigin));
					ListRemove(&possibleSpawnsList, possibleSpawnsList.head);
				}
			}

			// boosted guy: delete the 50% of spawnpoints farthest from some entity (fc, fs, etc)
			if (boost && spawnMeNearThisEntity) {
				ListSort(&possibleSpawnsList, CompareSpawnsReverseOrder, spawnMeNearThisEntity->r.currentOrigin);

				int goalSize;
				if (softSpawnOverride) {
					goalSize = Com_Clampi(1, MAX_TEAM_SPAWN_POINTS, (possibleSpawnsList.size / 3) * 2); // e.g. 9 total spawns ==> goalSize of 6 (remove 3 spawns)
				}
				else {
#if 1
					goalSize = Com_Clampi(1, MAX_TEAM_SPAWN_POINTS, possibleSpawnsList.size / 2); // e.g. 9 total spawns ==> goalSize of 4 (remove 5 spawns)
#else
					goalSize = Com_Clampi(1, MAX_TEAM_SPAWN_POINTS, possibleSpawnsList.size - (possibleSpawnsList.size / 2)); // e.g. 9 total spawns ==> goalSize of 5 (remove 4 spawns)
#endif
				}
				while (possibleSpawnsList.size > goalSize) {
					gentity_t *headEnt = ((spawnpoint_t *)(possibleSpawnsList.head))->ent;
					char *headNickname = ((spawnpoint_t *)(possibleSpawnsList.head))->nickname;
					SpawnDebugPrintf("(Boost) no whitelist; removing %s from the list based on 2D distance from entity %d (%s) %f\n", headNickname, spawnMeNearThisEntity - g_entities, VALIDSTRING(spawnMeNearThisEntity->classname) ? spawnMeNearThisEntity->classname : "", Distance2D(headEnt->r.currentOrigin, spawnMeNearThisEntity->r.currentOrigin));
					ListRemove(&possibleSpawnsList, possibleSpawnsList.head);
				}
			}

			assert(possibleSpawnsList.size >= 1);
			const int originalPossibleSpawnsListSize = possibleSpawnsList.size;
			SpawnDebugPrintf("Possible spawns list has size %d. Possible spawns are:\n", possibleSpawnsList.size);
			iterator_t iter;
			ListIterate(&possibleSpawnsList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				spawnpoint_t *element = (spawnpoint_t *)IteratorNext(&iter);
				if (client->pers.lastSpawnPoint && level.time - client->pers.lastSpawnTime < SPAWN_SPAM_PROTECT_TIME && !VALIDSTRING(client->pers.lastSpawnPoint->nextspawns))
					SpawnDebugPrintf("%s (2D distance from last spawn point (%s): %f)\n", element->nickname, lastSpawnName, Distance2D(element->ent->r.currentOrigin, client->pers.lastSpawnPoint->r.currentOrigin));
				else
					SpawnDebugPrintf("%s\n", element->nickname);
			}

			// at this point, we either have a list containing all and only the whitelisted spawnpoints given our previous one,
			// OR we have a list containing only the 50% of spawns farthest from our last spawn
			// we can now begin paring down the spawnpoints that are invalid for one reason or another.

			// filter out telefrag spawnpoints
			ListIterate(&possibleSpawnsList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				spawnpoint_t *element = (spawnpoint_t *)iter.current;
				genericNode_t *nextNode = iter.current ? ((node_t *)iter.current)->next : NULL; // store the next node before removal

				if (SpotWouldTelefrag(element->ent)) {
					if (possibleSpawnsList.size > 1) {
						SpawnDebugPrintf("Removing %s from the list since it would telefrag\n", element->nickname);
						IteratorRemove(&iter); // remove the current node
						iter.current = nextNode; // manually set iter.current to the next node
					}
					else {
						SpawnDebugPrintf("%s would telefrag, but removing it would remove the last spawn, so keeping it\n", element->nickname);
					}
				}

				if (iter.current != nextNode)
					IteratorNext(&iter); // only call IteratorNext if we haven't removed the current node
			}


			// filter out spawnpoints too close to a dropped flag
			if (g_droppedFlagSpawnProtectionRadius.integer > 0) {
				qboolean droppedFlagExists = qfalse;
				for (int i = MAX_CLIENTS; i < MAX_GENTITIES; ++i) {
					gentity_t *flag = &g_entities[i];
					if (!flag->inuse || !flag->item)
						continue;
					if (flag->item->giTag != PW_REDFLAG && flag->item->giTag != PW_BLUEFLAG && flag->item->giTag != PW_NEUTRALFLAG)
						continue;
					if (!(flag->flags & FL_DROPPED_ITEM))
						continue;
					SpawnDebugPrintf("Dropped flag at %d %d %d\n", (int)flag->r.currentOrigin[0], (int)flag->r.currentOrigin[1], (int)flag->r.currentOrigin[2]);
					droppedFlagExists = qtrue;
				}

				if (droppedFlagExists) {
					ListIterate(&possibleSpawnsList, &iter, qfalse);
					while (IteratorHasNext(&iter)) {
						spawnpoint_t *element = (spawnpoint_t *)iter.current;
						genericNode_t *nextNode = iter.current ? ((node_t *)iter.current)->next : NULL; // store the next node before removal

						if (TooCloseToDroppedFlag(element->ent)) {
							if (possibleSpawnsList.size > 1) {
								SpawnDebugPrintf("Removing %s from the list since it is too close to a dropped flag\n", element->nickname);
								IteratorRemove(&iter); // remove the current node
								iter.current = nextNode; // manually set iter.current to the next node
							}
							else {
								SpawnDebugPrintf("%s is too close to a dropped flag, but removing it would remove the last spawn, so keeping it\n", element->nickname);
							}
						}

						if (iter.current != nextNode)
							IteratorNext(&iter); // only call IteratorNext if we haven't removed the current node
					}
				}
			}


			// filter out spawnpoints too close to an allied flag carrier
			if (!g_canSpawnInTeRangeOfFc.integer) {
				gentity_t *fc = GetFC(team, NULL, qfalse, 0);
				if (fc) {
					SpawnDebugPrintf("FC is player %d (%s) at %d %d %d\n", fc - g_entities, fc->client ? fc->client->pers.netname : "", (int)fc->r.currentOrigin[0], (int)fc->r.currentOrigin[1], (int)fc->r.currentOrigin[2]);
					ListIterate(&possibleSpawnsList, &iter, qfalse);
					while (IteratorHasNext(&iter)) {
						spawnpoint_t *element = (spawnpoint_t *)iter.current;
						genericNode_t *nextNode = iter.current ? ((node_t *)iter.current)->next : NULL; // store the next node before removal

						if (TooCloseToAlliedFlagCarrier(element->ent, client->sess.sessionTeam)) {
							if (possibleSpawnsList.size > 1) {
								SpawnDebugPrintf("Removing %s from the list since it is too close to an allied flag carrier\n", element->nickname);
								IteratorRemove(&iter); // remove the current node
								iter.current = nextNode; // manually set iter.current to the next node
							}
							else {
								SpawnDebugPrintf("%s is too close to an allied flag carrier, but removing it would remove the last spawn, so keeping it\n", element->nickname);
							}
						}

						if (iter.current != nextNode)
							IteratorNext(&iter); // only call IteratorNext if we haven't removed the current node
					}
				}
			}


			// filter out spawnpoints too close to where i was just killed by an enemy
			if (g_killedAntiHannahSpawnRadius.integer > 0) {
				if (client->pers.lastKilledByEnemyTime && level.time - client->pers.lastKilledByEnemyTime < 2500) {
					SpawnDebugPrintf("lastKilledByEnemyTime %d and %d - %d < 2500 (%d)\n", client->pers.lastKilledByEnemyTime, level.time, client->pers.lastKilledByEnemyTime, level.time - client->pers.lastKilledByEnemyTime);
					ListIterate(&possibleSpawnsList, &iter, qfalse);
					while (IteratorHasNext(&iter)) {
						spawnpoint_t *element = (spawnpoint_t *)iter.current;
						genericNode_t *nextNode = iter.current ? ((node_t *)iter.current)->next : NULL; // store the next node before removal

						if (TooCloseToWhereJustKilledByEnemy(element->ent, client)) {
							if (possibleSpawnsList.size > 1) {
								SpawnDebugPrintf("Removing %s from the list since it is too close to where just killed by enemy (threshold: %d)\n", element->nickname, g_killedAntiHannahSpawnRadius.integer);
								IteratorRemove(&iter); // remove the current node
								iter.current = nextNode; // manually set iter.current to the next node
							}
							else {
								SpawnDebugPrintf("%s is too close to where just killed by enemy, but removing it would remove the last spawn, so keeping it (threshold: %d)\n", element->nickname, g_killedAntiHannahSpawnRadius.integer);
							}
						}

						if (iter.current != nextNode)
							IteratorNext(&iter); // only call IteratorNext if we haven't removed the current node
					}
				}
			}

			// we have finally filtered down the list; there is now at least 1 and possibly more valid spawnpoints.
			// pick a random one from the list and return it
			assert(possibleSpawnsList.size >= 1);
			if (possibleSpawnsList.size != originalPossibleSpawnsListSize) {
				SpawnDebugPrintf("After filtering, spawns list has size %d. Possible spawns are:\n", possibleSpawnsList.size);
				iterator_t iter;
				ListIterate(&possibleSpawnsList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					spawnpoint_t *element = (spawnpoint_t *)IteratorNext(&iter);
					SpawnDebugPrintf("%s\n", element->nickname);
				}
			}

#if 0 // testing
			if (g_presetInitialSpawns.integer && level.time - level.startTime <= 60000 && client->stats && client->stats->selfkills == 1 &&
#else
			if (g_presetInitialSpawns.integer && level.time - level.startTime <= 10000 && client->stats && client->stats->selfkills == 1 &&
#endif
				GetRemindedPosOrDeterminedPos(&g_entities[client - level.clients]) != CTFPOSITION_BASE && GetRemindedPosOrDeterminedPos(&g_entities[client - level.clients]) != CTFPOSITION_CHASE &&
				((level.initialRespawn[client->sess.sessionTeam][0] && !SpotWouldTelefrag(level.initialRespawn[client->sess.sessionTeam][0])) || (level.initialRespawn[client->sess.sessionTeam][1] && !SpotWouldTelefrag(level.initialRespawn[client->sess.sessionTeam][1]))) &&
				(!client->pers.lastKiller ||
					client->pers.lastKiller - g_entities == client - level.clients ||
					client->pers.lastKiller - g_entities == ENTITYNUM_NONE ||
					client->pers.lastKiller - g_entities == ENTITYNUM_WORLD)) {

				if (level.initialRespawn[client->sess.sessionTeam][0] && !SpotWouldTelefrag(level.initialRespawn[client->sess.sessionTeam][0])) {
					gentity_t *returnSpawnPoint = level.initialRespawn[client->sess.sessionTeam][0];
					char nickname[128] = { 0 };
					GetSpawnPointNickname(returnSpawnPoint, nickname, sizeof(nickname), qtrue);
					SpawnDebugPrintf("level.initialRespawn[%d][0] is %s\n", client->sess.sessionTeam, nickname);
					level.initialRespawn[client->sess.sessionTeam][0] = NULL;
					ListClear(&possibleSpawnsList);
					return returnSpawnPoint;
				}
				else if (level.initialRespawn[client->sess.sessionTeam][1] && !SpotWouldTelefrag(level.initialRespawn[client->sess.sessionTeam][1])) {
					gentity_t *returnSpawnPoint = level.initialRespawn[client->sess.sessionTeam][1];
					char nickname[128] = { 0 };
					GetSpawnPointNickname(returnSpawnPoint, nickname, sizeof(nickname), qtrue);
					SpawnDebugPrintf("level.initialRespawn[%d][1] is %s\n", client->sess.sessionTeam, nickname);
					level.initialRespawn[client->sess.sessionTeam][1] = NULL;
					ListClear(&possibleSpawnsList);
					return returnSpawnPoint;
				}
			}
			else {
				const int randomIndex = rand() % possibleSpawnsList.size;
				SpawnDebugPrintf("Randomly chosen index: %d\n", randomIndex);
				spawnpoint_t *chosenSpawnPoint = NULL;
				int currentIndex = 0;

				// find the one with the random index
				ListIterate(&possibleSpawnsList, &iter, qfalse);
				while (IteratorHasNext(&iter) && currentIndex <= randomIndex) {
					chosenSpawnPoint = (spawnpoint_t *)IteratorNext(&iter);
					if (currentIndex == randomIndex) {
						SpawnDebugPrintf("Spawnpoint with randomly chosen index %d is %s\n", randomIndex, chosenSpawnPoint->nickname);
						break;
					}
					currentIndex++;
				}

				gentity_t *returnSpawnPoint = chosenSpawnPoint->ent;
				ListClear(&possibleSpawnsList);
				return returnSpawnPoint;
			}
		}
	}

	gentity_t *spot = NULL;
	gentity_t *spots[MAX_TEAM_SPAWN_POINTS] = { NULL };
	int count = 0;

	while ((spot = G_Find(spot, FOFS(classname), classname)) != NULL) {
		if (SpotWouldTelefrag(spot)) {
			continue;
		}

		if (mustBeEnabled && !spot->genericValue1)
		{ //siege point that's not enabled, can't use it
			continue;
		}

		spots[count] = spot;
		if (++count == MAX_TEAM_SPAWN_POINTS)
			break;
	}

	if (!count) {	// no valid spot
		return G_Find(NULL, FOFS(classname), classname);
	}

	if (g_gametype.integer == GT_SIEGE && siegeClass >= 0 &&
		bgSiegeClasses[siegeClass].name[0])
	{ //out of the spots found, see if any have an idealclass to match our class name
		int classCount = 0;
		int i = 0;

        while (i < count)
		{
			if (spots[i] && spots[i]->idealclass && spots[i]->idealclass[0] &&
				!Q_stricmp(spots[i]->idealclass, bgSiegeClasses[siegeClass].name))
			{ //this spot's idealclass matches the class name
				classCount++;
			}
			i++;
		}

		if (classCount > 0)
		{ //found at least one
			selection = rand() % classCount;
			return spots[ selection ];
		}
	}

	selection = rand() % count;
	return spots[ selection ];
}


/*
===========
SelectCTFSpawnPoint

============
*/
gentity_t *SelectCTFSpawnPoint ( gclient_t *client, team_t team, int teamstate, vec3_t origin, vec3_t angles ) {
	gentity_t	*spot;

	spot = SelectRandomTeamSpawnPoint ( client, teamstate, team, -1 );

	if (!spot) {
		spot = SelectSpawnPoint( vec3_origin, origin, angles, team );
		if (spot)
			return spot; // +9 etc already done

		if (team == TEAM_FREE) // no ffa spawns found for a racer; just try to get a random team one
			spot = SelectRandomTeamSpawnPoint(NULL, teamstate, Q_irand(TEAM_RED, TEAM_BLUE), -1);

		if (!spot) // STILL no spawn found (shouldn't happen)
			G_Error("No spawn point found");
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}

/*
===========
SelectSiegeSpawnPoint

============
*/
gentity_t *SelectSiegeSpawnPoint ( int siegeClass, team_t team, int teamstate, vec3_t origin, vec3_t angles ) {
	gentity_t	*spot;

	spot = SelectRandomTeamSpawnPoint ( NULL, teamstate, team, siegeClass );

	if (!spot) {
		return SelectSpawnPoint( vec3_origin, origin, angles, team );
	}

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}

/*---------------------------------------------------------------------------*/

static int QDECL SortClients( const void *a, const void *b ) {
	return *(int *)a - *(int *)b;
}


/*
==================
TeamplayLocationsMessage

Format:
	clientNum location health armor weapon powerups

==================
*/
void TeamplayInfoMessage( gentity_t *ent ) {
	char		entry[1024];
	char		string[8192];
	int			stringlength;
	int			i, j;
	gentity_t	*player;
	int			cnt;
	int			h, a, p;
	int			clients[TEAM_MAXOVERLAY];

	if ( ! ent->client->pers.teamInfo )
		return;

	// figure out what client should be on the display
	// we are limited to 8, but we want to use the top eight players
	// but in client order (so they don't keep changing position on the overlay)
	for (i = 0, cnt = 0; i < g_maxclients.integer && cnt < TEAM_MAXOVERLAY; i++) {
		player = g_entities + level.sortedClients[i];
		if (player->inuse && player->client->sess.sessionTeam == 
			ent->client->ps.persistant[PERS_TEAM] ) {
			clients[cnt++] = level.sortedClients[i];
		}
	}

	// We have the top eight players, sort them by clientNum
	qsort( clients, cnt, sizeof( clients[0] ), SortClients );

	// send the latest information on all clients
	string[0] = 0;
	stringlength = 0;

	for (i = 0, cnt = 0; i < g_maxclients.integer && cnt < TEAM_MAXOVERLAY; i++) {
		player = g_entities + i;
		if (player->inuse && player->client->sess.sessionTeam == 
			ent->client->ps.persistant[PERS_TEAM] ) {

			h = player->client->ps.stats[STAT_HEALTH];
			if (h < 0) h = 0;

			if (g_teamOverlayForce.integer) {
				// modified behavior: compatible client mods can display everything properly; incompatible clients will see force in place of armor
				// armor slot = force power points
				// powerups slot = armor in the upper 8 (unused) bits; actual powerups in the lower bits as normal
				a = player->client->ps.fd.forcePower;
				byte clampedArmor = (byte)Com_Clampi(0, 255, player->client->ps.stats[STAT_ARMOR]);
				p = (int)(clampedArmor << 24);
				p |= player->s.powerups;
			}
			else {
				// normal behavior
				// armor slot = armor
				// powerups slot = powerups
				a = player->client->ps.stats[STAT_ARMOR];
				p = player->s.powerups;
			}

			if (g_teamOverlayForceAlignment.integer && g_gametype.integer != GT_SIEGE) {
				if (player->health > 0 && player->client->ps.fd.forcePowersKnown) {
					if (player->client->ps.fd.forceSide == FORCE_LIGHTSIDE) {
						p |= (1 << PW_FORCE_ENLIGHTENED_LIGHT);
						p &= ~(1 << PW_FORCE_ENLIGHTENED_DARK);
					}
					else if (player->client->ps.fd.forceSide == FORCE_DARKSIDE) {
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

			if (a < 0) a = 0;

			

			Com_sprintf (entry, sizeof(entry),
				" %i %i %i %i %i %i", 
//				level.sortedClients[i], player->client->pers.teamState.location, h, a, 
				i, player->client->pers.teamState.location, h, a, 
				player->client->ps.weapon, p);
			j = strlen(entry);
			if (stringlength + j > sizeof(string))
				break;
			strcpy (string + stringlength, entry);
			stringlength += j;
			cnt++;
		}
	}

	trap_SendServerCommand( ent-g_entities, va("tinfo %i %s", cnt, string) );
}

void CheckTeamStatus(void) {
	int i;
	gentity_t *ent;

    //OSP: pause
    if ( level.pause.state != PAUSE_NONE ) // doesn't affect racers due to TEAM_FREE
            return;

	if (level.intermissiontime)
		return; // don't spam teamoverlay updates during intermission

	int updateRate = Com_Clampi(1, 1000, g_teamOverlayUpdateRate.integer);
	if (level.time - level.lastTeamLocationTime > updateRate) {

		level.lastTeamLocationTime = level.time;

		for (i = 0; i < g_maxclients.integer; i++) {
			ent = g_entities + i;

			if ( !ent->client )
			{
				continue;
			}

			if ( ent->client->pers.connected != CON_CONNECTED ) {
				continue;
			}

			if (ent->inuse && (ent->client->sess.sessionTeam == TEAM_RED ||	ent->client->sess.sessionTeam == TEAM_BLUE)) {
				ent->client->pers.teamState.location = Team_GetLocation( ent, NULL, 0, qtrue );
			}
		}

		for (i = 0; i < g_maxclients.integer; i++) {
			ent = g_entities + i;

			if ( !ent->client || ent->client->pers.connected != CON_CONNECTED ) {
				continue;
			}

			if (ent->inuse && (ent->client->ps.persistant[PERS_TEAM] == TEAM_RED ||	ent->client->ps.persistant[PERS_TEAM] == TEAM_BLUE) && !ent->client->isLagging) {
				TeamplayInfoMessage( ent );
			}
		}
	}
}

/*-----------------------------------------------------------------*/

/*QUAKED team_CTF_redplayer (1 0 0) (-16 -16 -16) (16 16 32)
Only in CTF games.  Red players spawn here at game start.
*/
void SP_team_CTF_redplayer( gentity_t *ent ) {
}


/*QUAKED team_CTF_blueplayer (0 0 1) (-16 -16 -16) (16 16 32)
Only in CTF games.  Blue players spawn here at game start.
*/
void SP_team_CTF_blueplayer( gentity_t *ent ) {
}

/*QUAKED team_CTF_redspawn (1 0 0) (-16 -16 -24) (16 16 32)
potential spawning position for red team in CTF games.
Targets will be fired when someone spawns in on them.
*/
void SP_team_CTF_redspawn(gentity_t *ent) {
	int initialRespawn;
	G_SpawnInt("initialrespawn", "0", &initialRespawn);
	if (!initialRespawn)
		return;

	ent->isManuallyDefinedInitialRespawn = qtrue;
	++level.numManuallyDefinedInitialRespawns[TEAM_RED];

	char nickname[128];
	GetSpawnPointNickname(ent, nickname, sizeof(nickname), qtrue);
	SpawnDebugPrintf("%s is a manually defined initial respawn for red team\n", nickname);
}

/*QUAKED team_CTF_bluespawn (0 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for blue team in CTF games.
Targets will be fired when someone spawns in on them.
*/
void SP_team_CTF_bluespawn(gentity_t *ent) {
	int initialRespawn;
	G_SpawnInt("initialrespawn", "0", &initialRespawn);
	if (!initialRespawn)
		return;

	ent->isManuallyDefinedInitialRespawn = qtrue;
	++level.numManuallyDefinedInitialRespawns[TEAM_BLUE];

	char nickname[128];
	GetSpawnPointNickname(ent, nickname, sizeof(nickname), qtrue);
	SpawnDebugPrintf("%s is a manually defined initial respawn for blue team\n", nickname);
}


