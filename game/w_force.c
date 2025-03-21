//#include "g_local.h"
#include "b_local.h"
#include "w_saber.h"
#include "ai_main.h"
#include "G2.h"

#define METROID_JUMP 1

//NEEDED FOR MIND-TRICK on NPCS=========================================================
extern void NPC_PlayConfusionSound( gentity_t *self );
extern void NPC_Jedi_PlayConfusionSound( gentity_t *self );
extern void NPC_UseResponse( gentity_t *self, gentity_t *user, qboolean useWhenDone );
//NEEDED FOR MIND-TRICK on NPCS=========================================================
extern void Jedi_Decloak( gentity_t *self );

extern vmCvar_t		g_saberRestrictForce;

#include "namespace_begin.h"
extern qboolean BG_FullBodyTauntAnim( int anim );
#include "namespace_end.h"

#define DRAIN_REWORK1_FORCECOST_LEVEL3	(25)
#define DRAIN_REWORK1_FORCECOST_LEVEL2	(17)
#define DRAIN_REWORK1_FORCECOST_LEVEL1	(8)

#define DRAIN_REWORK2_SELFDMG_LEVEL3	(30)
#define DRAIN_REWORK2_SELFDMG_LEVEL2	(20)
#define DRAIN_REWORK2_SELFDMG_LEVEL1	(10)

#define DRAIN_REWORK_FORCEREDUCE_LEVEL3	(33)
#define DRAIN_REWORK_FORCEREDUCE_LEVEL2	(22)
#define DRAIN_REWORK_FORCEREDUCE_LEVEL1	(11)

#if 0
#define DRAIN_REWORK2_FORCEGIVEN_LEVEL3	(20)
#define DRAIN_REWORK2_FORCEGIVEN_LEVEL2	(13)
#define DRAIN_REWORK2_FORCEGIVEN_LEVEL1	(7)
#endif

#define DRAIN_REWORK_DEBUFFDURATION_CONSTANT	(0)
#define DRAIN_REWORK_DEBUFFDURATION_PERLEVEL	(1333)

#define DRAIN_REWORK2_MINIMUMFORCE		(0)

#define DRAIN_REWORK_COOLDOWN			(1000)

extern bot_state_t *botstates[MAX_CLIENTS];

int speedLoopSound = 0;
int rageLoopSound = 0;
int protectLoopSound = 0;
int absorbLoopSound = 0;
int seeLoopSound = 0;
int ysalamiriLoopSound = 0;

#define FORCE_VELOCITY_DAMAGE 0

#define GRIP_DEBOUNCE_TIME (3000)

#define RAGE_RECOVERY_TIME	(10000)

int ForceShootDrain( gentity_t *self );

gentity_t *G_PreDefSound(gentity_t *ent, int pdSound)
{
	gentity_t	*te;

	te = G_TempEntity(ent->client->ps.origin, EV_PREDEFSOUND);
	te->s.eventParm = pdSound;
	VectorCopy(ent->client->ps.origin, te->s.origin);

	G_ApplyRaceBroadcastsToEvent( ent, te );

	return te;
}

const int forcePowerMinRank[NUM_FORCE_POWER_LEVELS][NUM_FORCE_POWERS] = //0 == neutral
{
	{
		999,//FP_HEAL,//instant
		999,//FP_LEVITATION,//hold/duration
		999,//FP_SPEED,//duration
		999,//FP_PUSH,//hold/duration
		999,//FP_PULL,//hold/duration
		999,//FP_TELEPATHY,//instant
		999,//FP_GRIP,//hold/duration
		999,//FP_LIGHTNING,//hold/duration
		999,//FP_RAGE,//duration
		999,//FP_PROTECT,//duration
		999,//FP_ABSORB,//duration
		999,//FP_TEAM_HEAL,//instant
		999,//FP_TEAM_FORCE,//instant
		999,//FP_DRAIN,//hold/duration
		999,//FP_SEE,//duration
		999,//FP_SABER_OFFENSE,
		999,//FP_SABER_DEFENSE,
		999//FP_SABERTHROW,
		//NUM_FORCE_POWERS
	},
	{
		10,//FP_HEAL,//instant
		0,//FP_LEVITATION,//hold/duration
		0,//FP_SPEED,//duration
		0,//FP_PUSH,//hold/duration
		0,//FP_PULL,//hold/duration
		10,//FP_TELEPATHY,//instant
		15,//FP_GRIP,//hold/duration
		10,//FP_LIGHTNING,//hold/duration
		15,//FP_RAGE,//duration
		15,//FP_PROTECT,//duration
		15,//FP_ABSORB,//duration
		10,//FP_TEAM_HEAL,//instant
		10,//FP_TEAM_FORCE,//instant
		10,//FP_DRAIN,//hold/duration
		5,//FP_SEE,//duration
		0,//FP_SABER_OFFENSE,
		0,//FP_SABER_DEFENSE,
		0//FP_SABERTHROW,
		//NUM_FORCE_POWERS
	},
	{
		10,//FP_HEAL,//instant
		0,//FP_LEVITATION,//hold/duration
		0,//FP_SPEED,//duration
		0,//FP_PUSH,//hold/duration
		0,//FP_PULL,//hold/duration
		10,//FP_TELEPATHY,//instant
		15,//FP_GRIP,//hold/duration
		10,//FP_LIGHTNING,//hold/duration
		15,//FP_RAGE,//duration
		15,//FP_PROTECT,//duration
		15,//FP_ABSORB,//duration
		10,//FP_TEAM_HEAL,//instant
		10,//FP_TEAM_FORCE,//instant
		10,//FP_DRAIN,//hold/duration
		5,//FP_SEE,//duration
		5,//FP_SABER_OFFENSE,
		5,//FP_SABER_DEFENSE,
		5//FP_SABERTHROW,
		//NUM_FORCE_POWERS
	},
	{
		10,//FP_HEAL,//instant
		0,//FP_LEVITATION,//hold/duration
		0,//FP_SPEED,//duration
		0,//FP_PUSH,//hold/duration
		0,//FP_PULL,//hold/duration
		10,//FP_TELEPATHY,//instant
		15,//FP_GRIP,//hold/duration
		10,//FP_LIGHTNING,//hold/duration
		15,//FP_RAGE,//duration
		15,//FP_PROTECT,//duration
		15,//FP_ABSORB,//duration
		10,//FP_TEAM_HEAL,//instant
		10,//FP_TEAM_FORCE,//instant
		10,//FP_DRAIN,//hold/duration
		5,//FP_SEE,//duration
		10,//FP_SABER_OFFENSE,
		10,//FP_SABER_DEFENSE,
		10//FP_SABERTHROW,
		//NUM_FORCE_POWERS
	}
};

const int mindTrickTime[NUM_FORCE_POWER_LEVELS] =
{
	0,//none
	5000,
	10000,
	15000
};

extern int g_LastFrameTime;
static int FixDuration(int duration) {
	if (!g_fixForceTimers.integer)
		return duration;

	const int frameTime = 1000 / g_svfps.integer;
	const int fixedDuration = duration + ((frameTime - (duration % frameTime)) % frameTime);
	const int timeSinceLastFrame = level.time - g_LastFrameTime;
	return fixedDuration - timeSinceLastFrame;
}

// use null sendTo to send to the player himself and all his followers
void SendForceTimers(gentity_t *user, gentity_t *sendTo) {
	if (!user || !user->client) {
		assert(qfalse);
		return;
	}

	if (!g_sendForceTimers.integer)
		return;

	char commandBuf[MAX_STRING_CHARS] = { 0 };
	Com_sprintf(commandBuf, sizeof(commandBuf), "kls -1 -1 fpt %d", user - g_entities);
	qboolean gotOne = qfalse;

	for (int i = FP_FIRST; i < NUM_FORCE_POWERS; i++) {
		const int fpLevel = Com_Clampi(1, 3, user->client->ps.fd.forcePowerLevel[i]);
		int duration;
		switch (i) {
		case FP_SABER_OFFENSE:
			duration = RAGE_RECOVERY_TIME; break;
		case FP_SPEED:
			if (user->client->sess.inRacemode)
				continue; // no speed timers in racemode
			switch (fpLevel) {
			case 1: duration = 10000; break;
			case 2: duration = 15000; break;
			case 3: duration = 20000; break;
			}
			break;
		case FP_TELEPATHY:
			switch (fpLevel) {
			case 1: duration = 20000; break;
			case 2: duration = 25000; break;
			case 3: duration = 30000; break;
			}
			break;
		case FP_PROTECT:
			duration = 20000; break;
		case FP_ABSORB:
			duration = 20000; break;
		case FP_RAGE:
			switch (fpLevel) {
			case 1: duration = 8000; break;
			case 2: duration = 14000; break;
			case 3: duration = 20000; break;
			}
			break;
		case FP_SEE:
			switch (fpLevel) {
			case 1: duration = 10000; break;
			case 2: duration = 20000; break;
			case 3: duration = 30000; break;
			}
			break;
		default: continue;
		}

		if (i == FP_SABER_OFFENSE) {
			if (user->client->rageRecoveryActiveUntil > level.time - level.startTime) {
				Q_strcat(commandBuf, sizeof(commandBuf), va(" %d %d %d", i, duration, user->client->rageRecoveryActiveUntil));
				gotOne = qtrue;
			}
		}
		else {
			if (user->client->forcePowerActiveUntil[i] > level.time - level.startTime) {
				Q_strcat(commandBuf, sizeof(commandBuf), va(" %d %d %d", i, duration, user->client->forcePowerActiveUntil[i]));
				gotOne = qtrue;
			}
		}
	}

	if (!gotOne)
		Com_sprintf(commandBuf, sizeof(commandBuf), "kls -1 -1 sfpt %d", user - g_entities);

	if (sendTo) {
		trap_SendServerCommand(sendTo - g_entities, commandBuf);
		//Com_DebugPrintf("SendForceTimers [%d]-[%d]=[%d]: sent to client %d: %s\n", level.time, level.startTime, level.time - level.startTime, sendTo - g_entities, commandBuf);
	}
	else {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *sendToPlayerAndFollowers = &g_entities[i];
			if (!sendToPlayerAndFollowers->inuse || !sendToPlayerAndFollowers->client || sendToPlayerAndFollowers->client->pers.connected != CON_CONNECTED)
				continue;
			if (sendToPlayerAndFollowers == user) {
			}
			else if (sendToPlayerAndFollowers->client->sess.sessionTeam == TEAM_SPECTATOR && sendToPlayerAndFollowers->client->sess.spectatorState == SPECTATOR_FOLLOW && sendToPlayerAndFollowers->client->sess.spectatorClient == user - g_entities) {
			}
			else {
				continue;
			}

			trap_SendServerCommand(i, commandBuf);
			//Com_DebugPrintf("StartForceTimer [%d]-[%d]=[%d]: sent to client %d: %s\n", level.time, level.startTime, level.time - level.startTime, i, commandBuf);
		}
	}
}

// use FP_SABER_OFFENSE for postrage (shitty hack)
static void StartForceTimer(gentity_t *user, int forcePower, int duration) {
	assert(user && user->client && forcePower >= FP_FIRST && forcePower < NUM_FORCE_POWERS);

	switch (forcePower) {
	case FP_SABER_OFFENSE: case FP_SPEED: case FP_TELEPATHY: case FP_PROTECT: case FP_ABSORB: case FP_RAGE: case FP_SEE: break;
	default: assert(qfalse); return;
	}

	int activeUntil;
	if (duration > 0) {
		activeUntil = (level.time - level.startTime) + duration;

		if (forcePower == FP_SABER_OFFENSE) {
			user->client->rageRecoveryActiveUntil = activeUntil;
			user->client->forcePowerActiveUntil[FP_RAGE] = 0; // shitty hack, starting rage recovery stops rage
		}
		else {
			user->client->forcePowerActiveUntil[forcePower] = activeUntil;
			if (forcePower == FP_RAGE)
				user->client->rageRecoveryActiveUntil = 0; // inverse of above shitty hack
		}
	}
	else {
		activeUntil = user->client->forcePowerActiveUntil[forcePower] = 0;
	}

	SendForceTimers(user, NULL);
}

/* *CHANGE 7a* anti force crash */
// default force
// by Gamall
char  *gaGENERIC_FORCE	= "7-1-033330000000000333";
// force boundaries
char  *gaFORCE_LOWER 	= "0-1-000000000000000000";
char  *gaFORCE_UPPER 	= "7-2-333333333333333333";

char* gaCheckForceString(char* s) {
	char *p = s, *pu = gaFORCE_UPPER, *pl = gaFORCE_LOWER;
	if (!s || strlen(s) != 22) return gaGENERIC_FORCE;
	while(*p) {if (*p > *pu++ || *p++ < *pl++) {return gaGENERIC_FORCE;}}
	return s;	
}

void WP_InitForcePowers( gentity_t *ent )
{
	int i;
	int i_r;
	int maxRank = g_maxForceRank.integer;
	qboolean warnClient = qfalse;
	qboolean warnClientLimit = qfalse;
	char userinfo[MAX_INFO_STRING];
	char forcePowers[256];
	char readBuf[256];
	int lastFPKnown = -1;
	qboolean didEvent = qfalse;

	/* *CHANGE 7b* anti force crash */
    char* temp;

	if (!maxRank)
	{ //if server has no max rank, default to max (50)
		maxRank = FORCE_MASTERY_JEDI_MASTER;
	}
	else if (maxRank >= NUM_FORCE_MASTERY_LEVELS)
	{//ack, prevent user from being dumb
		maxRank = FORCE_MASTERY_JEDI_MASTER;
		trap_Cvar_Set( "g_maxForceRank", va("%i", maxRank) );
	}

	if ( !ent || !ent->client )
	{
		return;
	}

	ent->client->ps.fd.saberAnimLevel = ent->client->sess.saberLevel;

	if (ent->client->ps.fd.saberAnimLevel < FORCE_LEVEL_1 ||
		ent->client->ps.fd.saberAnimLevel > FORCE_LEVEL_3)
	{
		ent->client->ps.fd.saberAnimLevel = FORCE_LEVEL_1;
	}

	if (!speedLoopSound)
	{ //so that the client configstring is already modified with this when we need it
		speedLoopSound = G_SoundIndex("sound/weapons/force/speedloop.wav");
	}

	if (!rageLoopSound)
	{
		rageLoopSound = G_SoundIndex("sound/weapons/force/rageloop.wav");
	}

	if (!absorbLoopSound)
	{
		absorbLoopSound = G_SoundIndex("sound/weapons/force/absorbloop.wav");
	}

	if (!protectLoopSound)
	{
		protectLoopSound = G_SoundIndex("sound/weapons/force/protectloop.wav");
	}

	if (!seeLoopSound)
	{
		seeLoopSound = G_SoundIndex("sound/weapons/force/seeloop.wav");
	}

	if (!ysalamiriLoopSound)
	{
		ysalamiriLoopSound = G_SoundIndex("sound/player/nullifyloop.wav");
	}

	if (ent->s.eType == ET_NPC)
	{ //just stop here then.
		return;
	}

	i = 0;
	while (i < NUM_FORCE_POWERS)
	{
		ent->client->ps.fd.forcePowerLevel[i] = 0;
		ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
		i++;
	}

	ent->client->ps.fd.forcePowerSelected = -1;

	ent->client->ps.fd.forceSide = 0;

	if (g_gametype.integer == GT_SIEGE &&
		ent->client->siegeClass != -1)
	{ //Then use the powers for this class, and skip all this nonsense.
		i = 0;

		while (i < NUM_FORCE_POWERS)
		{
			ent->client->ps.fd.forcePowerLevel[i] = bgSiegeClasses[ent->client->siegeClass].forcePowerLevels[i];

			if (!ent->client->ps.fd.forcePowerLevel[i])
			{
				ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
			}
			else
			{
				ent->client->ps.fd.forcePowersKnown |= (1 << i);
			}
			i++;
		}

		if (!ent->client->sess.setForce)
		{
			//bring up the class selection menu
			trap_SendServerCommand(ent-g_entities, "scl");
		}
		ent->client->sess.setForce = qtrue;

		return;
	}

	if (ent->s.eType == ET_NPC && ent->s.number >= MAX_CLIENTS)
	{ //rwwFIXMEFIXME: Temp
		strcpy(userinfo, "forcepowers\\7-1-333003000313003120");
	}
	else
	{
		trap_GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );
	}

	Q_strncpyz( forcePowers, Info_ValueForKey (userinfo, "forcepowers"), sizeof( forcePowers ) );

	if (ent && ent->client && ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_BOOST_FIXIDIOTICFORCECONFIG)) {
		// fix ls using attack 2 instead of def
		if (!Q_stricmp(forcePowers, "7-1-033330000333003210"))
			Q_strncpyz(forcePowers, "7-1-033330000333002130", sizeof(forcePowers));
		else if (!Q_stricmp(forcePowers, "7-1-333330000331002130"))
			Q_strncpyz(forcePowers, "7-1-033330000333002130", sizeof(forcePowers));
		else if (!Q_stricmp(forcePowers, "7-1-333330000330000230"))
			Q_strncpyz(forcePowers, "7-1-033330000333002130", sizeof(forcePowers));
	}

    /* *CHANGE 7c* anti force crash */
    temp = gaCheckForceString(forcePowers);
    if (temp != forcePowers && !(ent->r.svFlags & SVF_BOT)) {
        trap_SendServerCommand(ent->client->pers.clientNum, 
            va("print \"^1Incorrect force string (%s). Replaced by default.\n\"", forcePowers));

		G_HackLog("Illegal forcepowers: Client num %d (%s) from %s tries to set incorrect forcestring (%s).  Total userinfo: %s\n",
                    ent->client->pers.clientNum, 
					ent->client->pers.netname,
					ent->client->sess.ipString,
                    forcePowers,
					userinfo);
        Q_strncpyz( forcePowers, temp, sizeof( forcePowers ) );
    } 

	if ( (ent->r.svFlags & SVF_BOT) && botstates[ent->s.number] )
	{ //if it's a bot just copy the info directly from its personality
		Com_sprintf(forcePowers, sizeof(forcePowers), "%s\0", botstates[ent->s.number]->forceinfo);
	}

	//rww - parse through the string manually and eat out all the appropriate data
	i = 0;

	if (g_forceBasedTeams.integer)
	{
		if (ent->client->sess.sessionTeam == TEAM_RED)
		{
			warnClient = !(BG_LegalizedForcePowers(forcePowers, maxRank, HasSetSaberOnly(), FORCE_DARKSIDE, g_gametype.integer, g_forcePowerDisable.integer));
		}
		else if (ent->client->sess.sessionTeam == TEAM_BLUE)
		{
			warnClient = !(BG_LegalizedForcePowers(forcePowers, maxRank, HasSetSaberOnly(), FORCE_LIGHTSIDE, g_gametype.integer, g_forcePowerDisable.integer));
		}
		else
		{
			warnClient = !(BG_LegalizedForcePowers(forcePowers, maxRank, HasSetSaberOnly(), 0, g_gametype.integer, g_forcePowerDisable.integer));
		}
	}
	else
	{
		warnClient = !(BG_LegalizedForcePowers(forcePowers, maxRank, HasSetSaberOnly(), 0, g_gametype.integer, g_forcePowerDisable.integer));
	}

	i_r = 0;
	while (forcePowers[i] && forcePowers[i] != '-')
	{
		readBuf[i_r] = forcePowers[i];
		i_r++;
		i++;
	}
	readBuf[i_r] = 0;
	//THE RANK
	ent->client->ps.fd.forceRank = atoi(readBuf);
	i++;

	i_r = 0;
	while (forcePowers[i] && forcePowers[i] != '-')
	{
		readBuf[i_r] = forcePowers[i];
		i_r++;
		i++;
	}
	readBuf[i_r] = 0;
	//THE SIDE
	ent->client->ps.fd.forceSide = atoi(readBuf);
	i++;


	if ( g_gametype.integer != GT_SIEGE && (ent->r.svFlags & SVF_BOT) && botstates[ent->s.number] )
	{ //hmm..I'm going to cheat here.
		int oldI = i;
		i_r = 0;
		while (forcePowers[i] && forcePowers[i] != '\n' &&
			i_r < NUM_FORCE_POWERS)
		{
			if (ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
			{
				if (i_r == FP_ABSORB)
				{
					forcePowers[i] = '3';
				}
				if (botstates[ent->s.number]->settings.skill >= 4)
				{ //cheat and give them more stuff
					if (i_r == FP_HEAL)
					{
						forcePowers[i] = '3';
					}
					else if (i_r == FP_PROTECT)
					{
						forcePowers[i] = '3';
					}
				}
			}
			else if (ent->client->ps.fd.forceSide == FORCE_DARKSIDE)
			{
				if (botstates[ent->s.number]->settings.skill >= 4)
				{
					if (i_r == FP_GRIP)
					{
						forcePowers[i] = '3';
					}
					else if (i_r == FP_LIGHTNING)
					{
						forcePowers[i] = '3';
					}
					else if (i_r == FP_RAGE)
					{
						forcePowers[i] = '3';
					}
					else if (i_r == FP_DRAIN)
					{
						forcePowers[i] = '3';
					}
				}
			}

			if (i_r == FP_PUSH)
			{
				forcePowers[i] = '3';
			}
			else if (i_r == FP_PULL)
			{
				forcePowers[i] = '3';
			}

			i++;
			i_r++;
		}
		i = oldI;
	}

	i_r = 0;
	while (forcePowers[i] && forcePowers[i] != '\n' &&
		i_r < NUM_FORCE_POWERS)
	{
		readBuf[0] = forcePowers[i];
		readBuf[1] = 0;

		ent->client->ps.fd.forcePowerLevel[i_r] = atoi(readBuf);
		if (ent->client->ps.fd.forcePowerLevel[i_r])
		{
			ent->client->ps.fd.forcePowersKnown |= (1 << i_r);
		}
		else
		{
			ent->client->ps.fd.forcePowersKnown &= ~(1 << i_r);
		}
		i++;
		i_r++;
	}
	//THE POWERS

	if (ent->s.eType != ET_NPC)
	{
		if (HasSetSaberOnly())
		{
			gentity_t *te = G_TempEntity( vec3_origin, EV_SET_FREE_SABER );
			te->r.svFlags |= SVF_BROADCAST;
			te->s.eventParm = 1;
		}
		else
		{
			gentity_t *te = G_TempEntity( vec3_origin, EV_SET_FREE_SABER );
			te->r.svFlags |= SVF_BROADCAST;
			te->s.eventParm = 0;
		}

		if (g_forcePowerDisable.integer)
		{
			gentity_t *te = G_TempEntity( vec3_origin, EV_SET_FORCE_DISABLE );
			te->r.svFlags |= SVF_BROADCAST;
			te->s.eventParm = 1;
		}
		else
		{
			gentity_t *te = G_TempEntity( vec3_origin, EV_SET_FORCE_DISABLE );
			te->r.svFlags |= SVF_BROADCAST;
			te->s.eventParm = 0;
		}
	}

	if (ent->s.eType == ET_NPC)
	{
		ent->client->sess.setForce = qtrue;
	}
	else if (g_gametype.integer == GT_SIEGE)
	{
		if (!ent->client->sess.setForce)
		{
			ent->client->sess.setForce = qtrue;
			//bring up the class selection menu
			trap_SendServerCommand(ent-g_entities, "scl");
		}
	}
	else
	{
		if (warnClient || !ent->client->sess.setForce)
		{ //the client's rank is too high for the server and has been autocapped, so tell them
			if (g_gametype.integer != GT_HOLOCRON && g_gametype.integer != GT_JEDIMASTER )
			{
#ifdef EVENT_FORCE_RANK
				gentity_t *te = G_TempEntity( vec3_origin, EV_GIVE_NEW_RANK );

				te->r.svFlags |= SVF_BROADCAST;
				te->s.trickedentindex = ent->s.number;
				te->s.eventParm = maxRank;
				te->s.bolt1 = 0;
#endif
				didEvent = qtrue;

//				if (!(ent->r.svFlags & SVF_BOT) && g_gametype.integer != GT_DUEL && g_gametype.integer != GT_POWERDUEL && ent->s.eType != ET_NPC)
				if (!(ent->r.svFlags & SVF_BOT) && ent->s.eType != ET_NPC)
				{
					if (!g_teamAutoJoin.integer)
					{
						//Make them a spectator so they can set their powerups up without being bothered.
						ent->client->sess.sessionTeam = TEAM_SPECTATOR;
						ent->client->sess.spectatorState = SPECTATOR_FREE;
						ent->client->sess.spectatorClient = 0;

						ent->client->pers.teamState.state = TEAM_BEGIN;
						trap_SendServerCommand(ent-g_entities, "spc");	// Fire up the profile menu
					}
				}

#ifdef EVENT_FORCE_RANK
				te->s.bolt2 = ent->client->sess.sessionTeam;
#else
				//Event isn't very reliable, I made it a string. This way I can send it to just one
				//client also, as opposed to making a broadcast event.
				trap_SendServerCommand(ent->s.number, va("nfr %i %i %i", maxRank, 1, ent->client->sess.sessionTeam));
				//Arg1 is new max rank, arg2 is non-0 if force menu should be shown, arg3 is the current team
#endif
			}
			ent->client->sess.setForce = qtrue;
		}

		if (!didEvent )
		{
#ifdef EVENT_FORCE_RANK
			gentity_t *te = G_TempEntity( vec3_origin, EV_GIVE_NEW_RANK );

			te->r.svFlags |= SVF_BROADCAST;
			te->s.trickedentindex = ent->s.number;
			te->s.eventParm = maxRank;
			te->s.bolt1 = 1;
			te->s.bolt2 = ent->client->sess.sessionTeam;
#else
			trap_SendServerCommand(ent->s.number, va("nfr %i %i %i", maxRank, 0, ent->client->sess.sessionTeam));
#endif
		}

		if (warnClientLimit)
		{ //the server has one or more force powers disabled and the client is using them in his config
			//trap_SendServerCommand(ent-g_entities, va("print \"The server has one or more force powers that you have chosen disabled.\nYou will not be able to use the disable force power(s) while playing on this server.\n\""));
		}
	}
	
	// in racemode, force our own useful powers and unset all the others
	if ( ent->client->sess.inRacemode ) {
		SetRacerForcePowers(ent);
	}

	i = 0;
	while (i < NUM_FORCE_POWERS)
	{
		if ((ent->client->ps.fd.forcePowersKnown & (1 << i)) &&
			!ent->client->ps.fd.forcePowerLevel[i])
		{ //err..
			ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
		}
		else
		{
			if (i != FP_LEVITATION && i != FP_SABER_OFFENSE && i != FP_SABER_DEFENSE && i != FP_SABERTHROW)
			{
				lastFPKnown = i;
			}
		}

		i++;
	}

	if (ent->client->ps.fd.forcePowersKnown & ent->client->sess.selectedFP)
	{
		ent->client->ps.fd.forcePowerSelected = ent->client->sess.selectedFP;
	}

	if (!(ent->client->ps.fd.forcePowersKnown & (1 << ent->client->ps.fd.forcePowerSelected)))
	{
		if (lastFPKnown != -1)
		{
			ent->client->ps.fd.forcePowerSelected = lastFPKnown;
		}
		else
		{
			ent->client->ps.fd.forcePowerSelected = 0;
		}
	}

	while (i < NUM_FORCE_POWERS)
	{
		ent->client->ps.fd.forcePowerBaseLevel[i] = ent->client->ps.fd.forcePowerLevel[i];
		i++;
	}
	ent->client->ps.fd.forceUsingAdded = 0;
}

void WP_SpawnInitForcePowers( gentity_t *ent )
{
	int i = 0;

	ent->client->ps.saberAttackChainCount = 0;

	i = 0;

	while (i < NUM_FORCE_POWERS)
	{
		if (ent->client->ps.fd.forcePowersActive & (1 << i))
		{
			WP_ForcePowerStop(ent, i);
		}

		i++;
	}

	ent->client->ps.fd.forceDeactivateAll = 0;

	ent->client->ps.fd.forcePower = ent->client->ps.fd.forcePowerMax = FORCE_POWER_MAX;
	ent->client->ps.fd.forcePowerRegenDebounceTime = 0;
	ent->client->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;
	ent->client->ps.fd.forceMindtrickTargetIndex = 0;
	ent->client->ps.fd.forceMindtrickTargetIndex2 = 0;
	ent->client->ps.fd.forceMindtrickTargetIndex3 = 0;
	ent->client->ps.fd.forceMindtrickTargetIndex4 = 0;

	ent->client->ps.holocronBits = 0;

	i = 0;
	while (i < NUM_FORCE_POWERS)
	{
		ent->client->ps.holocronsCarried[i] = 0;
		i++;
	}

	if (g_gametype.integer == GT_HOLOCRON)
	{
		i = 0;
		while (i < NUM_FORCE_POWERS)
		{
			ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
			i++;
		}

		if (HasSetSaberOnly())
		{
			if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] < FORCE_LEVEL_1)
			{
				ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] = FORCE_LEVEL_1;
			}
			if (ent->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] < FORCE_LEVEL_1)
			{
				ent->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] = FORCE_LEVEL_1;
			}
		}
	}

	i = 0;

	while (i < NUM_FORCE_POWERS)
	{
		ent->client->ps.fd.forcePowerDebounce[i] = 0;
		ent->client->ps.fd.forcePowerDuration[i] = 0;

		i++;
	}

	ent->client->ps.fd.forcePowerRegenDebounceTime = 0;
	ent->client->ps.fd.forceJumpZStart = 0;
	ent->client->ps.fd.forceJumpCharge = 0;
	ent->client->ps.fd.forceJumpSound = 0;
	ent->client->ps.fd.forceGripDamageDebounceTime = 0;
	ent->client->ps.fd.forceGripBeingGripped = 0;
	ent->client->ps.fd.forceGripCripple = 0;
	ent->client->ps.fd.forceGripUseTime = 0;
	ent->client->ps.fd.forceGripSoundTime = 0;
	ent->client->ps.fd.forceGripStarted = 0;
	ent->client->ps.fd.forceHealTime = 0;
	ent->client->ps.fd.forceHealAmount = 0;
	ent->client->ps.fd.forceRageRecoveryTime = 0;
	ent->client->ps.fd.forceDrainEntNum = ENTITYNUM_NONE;
	ent->client->ps.fd.forceDrainTime = 0;

	i = 0;
	while (i < NUM_FORCE_POWERS)
	{
		if ((ent->client->ps.fd.forcePowersKnown & (1 << i)) &&
			!ent->client->ps.fd.forcePowerLevel[i])
		{ //make sure all known powers are cleared if we have level 0 in them
			ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
		}

		i++;
	}

	if (g_gametype.integer == GT_SIEGE &&
		ent->client->siegeClass != -1)
	{ //Then use the powers for this class.
		i = 0;

		while (i < NUM_FORCE_POWERS)
		{
			ent->client->ps.fd.forcePowerLevel[i] = bgSiegeClasses[ent->client->siegeClass].forcePowerLevels[i];

			if (!ent->client->ps.fd.forcePowerLevel[i])
			{
				ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
			}
			else
			{
				ent->client->ps.fd.forcePowersKnown |= (1 << i);
			}
			i++;
		}
	}
}

#include "namespace_begin.h"
extern qboolean BG_InKnockDown( int anim ); //bg_pmove.c
#include "namespace_end.h"
int WP_AbsorbConversion(gentity_t *attacked, int atdAbsLevel, gentity_t *attacker, int atPower, int atPowerLevel, int atForceSpent);
int ForcePowerUsableOn(gentity_t *attacker, gentity_t *other, forcePowers_t forcePower)
{
	if (other && other->isAimPracticePack) {
		if (forcePower == FP_PUSH || forcePower == FP_PULL)
			return qtrue;
		return qfalse;
	}
	if (other && other->client && BG_HasYsalamiri(g_gametype.integer, &other->client->ps))
	{
		return 0;
	}

	if (attacker && attacker->client && !BG_CanUseFPNow(g_gametype.integer, &attacker->client->ps, level.time, forcePower))
	{
		return 0;
	}

	//Dueling fighters cannot use force powers on others, with the exception of force push when locked with each other
	if (attacker && attacker->client && attacker->client->ps.duelInProgress)
	{
		return 0;
	}

	if (other && other->client && other->client->ps.duelInProgress)
	{
		return 0;
	}

	if (forcePower == FP_GRIP)
	{
		if (other && other->client &&
			(other->client->ps.fd.forcePowersActive & (1<<FP_ABSORB)))
		{ //don't allow gripping to begin with if they are absorbing
			//play sound indicating that attack was absorbed
			if (other->client->forcePowerSoundDebounce < level.time)
			{
				qboolean meme = (!level.wasRestarted && attacker && attacker->client && attacker->client->account && (!Q_stricmp(attacker->client->account->name, "duo") || !Q_stricmp(attacker->client->account->name, "alpha")));
				if (!g_gripAbsorbFix.integer && !meme) { // base behavior
					gentity_t *abSound = G_PreDefSound(other, PDSOUND_ABSORBHIT);
					abSound->s.trickedentindex = other->s.number;
					other->client->forcePowerSoundDebounce = level.time + 400;
				}

			}
			return g_gripAbsorbFix.integer ? -1 : 0; // return -1 so we know that the reason it failed is absorb
		}
		else if (other && other->client &&
			other->client->ps.weapon == WP_SABER &&
			BG_SaberInSpecial(other->client->ps.saberMove))
		{ //don't grip person while they are in a special or some really bad things can happen.
			return 0;
		}
	}

	// * CHANGE 333 * - knocked down should still be push/pullable

	////TO DO: try removing this (why is it here?)
	//if (other && other->client &&
	//	(forcePower == FP_PUSH ||
	//	forcePower == FP_PULL))
	//{
	//	if (BG_InKnockDown(other->client->ps.legsAnim))
	//	{
	//		return 0;
	//	}
	//}

	if (other && other->client && other->s.eType == ET_NPC &&
		other->s.NPC_class == CLASS_VEHICLE)
	{ //can't use the force on vehicles.. except lightning
		if (forcePower == FP_LIGHTNING)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	if (other && other->client && other->s.eType == ET_NPC &&
		g_gametype.integer == GT_SIEGE)
	{ //can't use powers at all on npc's normally in siege...
		return 0;
	}

	return 1;
}

qboolean WP_ForcePowerAvailable( gentity_t *self, forcePowers_t forcePower, int overrideAmt )
{
	int	drain = overrideAmt ? overrideAmt :
				forcePowerNeeded[self->client->ps.fd.forcePowerLevel[forcePower]][forcePower];

#ifdef FP_SPEED_IMPROVED
	if (!overrideAmt && forcePower == FP_SPEED){
		drain = 10;
	}
#endif

	if (self->client->ps.powerups[PW_FORCE_BOON]){
		drain /= 2;
	}

	if (self->client->ps.fd.forcePowersActive & (1 << forcePower))
	{ //we're probably going to deactivate it..
		return qtrue;
	}
	if ( forcePower == FP_LEVITATION )
	{
		return qtrue;
	}
	if ( !drain )
	{
		return qtrue;
	}
	if (forcePower == FP_LIGHTNING && (self->client->ps.fd.forcePower >= 25 || (g_lightningRework.integer && self->client->ps.fd.forcePower >= 1)))
		return qtrue;
	if (forcePower == FP_DRAIN) {
		if (!g_drainRework.integer) {
			if (self->client->ps.fd.forcePower >= 25)
				return qtrue;
		}
		else {
			if (g_drainRework.integer >= 2) {
				if (self->client->ps.fd.forcePower >= DRAIN_REWORK2_MINIMUMFORCE)
					return qtrue;
				if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
					PrintIngame(self - g_entities, "(%d) WP_ForcePowerAvailable: error 17\n", level.time - level.startTime);
				return qfalse;
			}
			else {
				int forceCost;
				switch (self->client->ps.fd.forcePowerLevel[FP_DRAIN]) {
				case 3: forceCost = DRAIN_REWORK1_FORCECOST_LEVEL3; break;
				case 2: forceCost = DRAIN_REWORK1_FORCECOST_LEVEL2; break;
				default: forceCost = DRAIN_REWORK1_FORCECOST_LEVEL1; break;
				}
				if (self->client->ps.fd.forcePower >= forceCost)
					return qtrue;
				if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
					PrintIngame(self - g_entities, "(%d) WP_ForcePowerAvailable: error 18\n", level.time - level.startTime);
				return qfalse;
			}
		}
	}
	if ( self->client->ps.fd.forcePower < drain )
	{
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerAvailable: error 19 (not enough force power: %d < %d, overrideAmt %d, forcePowerNeeded %d)\n", level.time - level.startTime, self->client->ps.fd.forcePower, drain, overrideAmt, forcePowerNeeded[self->client->ps.fd.forcePowerLevel[forcePower]][forcePower]);
		return qfalse;
	}
	return qtrue;
}

qboolean WP_ForcePowerInUse( gentity_t *self, forcePowers_t forcePower )
{
	if ( (self->client->ps.fd.forcePowersActive & ( 1 << forcePower )) )
	{//already using this power
		return qtrue;
	}

	return qfalse;
}

qboolean WP_ForcePowerUsable( gentity_t *self, forcePowers_t forcePower )
{
	qboolean meme = (!level.wasRestarted && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
	if (meme && !self->client->sess.inRacemode)
		return qtrue;

	if (InstagibEnabled() && forcePower != FP_LEVITATION) { // you can only use force jump in instagib
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 1\n", level.time - level.startTime);
		return qfalse;
	}
	if (BG_HasYsalamiri(g_gametype.integer, &self->client->ps))
	{
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 2\n", level.time - level.startTime);
		return qfalse;
	}

	if (self->health <= 0 || self->client->ps.stats[STAT_HEALTH] <= 0 ||
		(self->client->ps.eFlags & EF_DEAD))
	{
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 3\n", level.time - level.startTime);
		return qfalse;
	}

	if (self->client->ps.pm_flags & PMF_FOLLOW)
	{ //specs can't use powers through people
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 14\n", level.time - level.startTime);
		return qfalse;
	}
	if (self->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 5\n", level.time - level.startTime);
		return qfalse;
	}
	if (self->client->tempSpectate >= level.time)
	{
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 6\n", level.time - level.startTime);
		return qfalse;
	}

	if (!BG_CanUseFPNow(g_gametype.integer, &self->client->ps, level.time, forcePower))
	{
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 7\n", level.time - level.startTime);
		return qfalse;
	}

	if ( !(self->client->ps.fd.forcePowersKnown & ( 1 << forcePower )) )
	{//don't know this power
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 8\n", level.time - level.startTime);
		return qfalse;
	}
	
	if ( (self->client->ps.fd.forcePowersActive & ( 1 << forcePower )) )
	{//already using this power
		if (forcePower != FP_LEVITATION)
		{
			if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
				PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 9\n", level.time - level.startTime);
			return qfalse;
		}
	}

	if (forcePower == FP_LEVITATION && self->client->fjDidJump)
	{
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 10\n", level.time - level.startTime);
		return qfalse;
	}

	if (!self->client->ps.fd.forcePowerLevel[forcePower])
	{
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 11\n", level.time - level.startTime);
		return qfalse;
	}

	if (forcePower == FP_DRAIN && self->client->drainDebuffTime > level.time && g_drainRework.integer && g_drainRework.integer != 1) {
		if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
			PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 12\n", level.time - level.startTime);
		return qfalse;
	}

	if ( g_debugMelee.integer )
	{
		if ( (self->client->ps.pm_flags&PMF_STUCK_TO_WALL) )
		{//no offensive force powers when stuck to wall
			switch ( forcePower )
			{
			case FP_GRIP:
			case FP_LIGHTNING:
			case FP_DRAIN:
			case FP_SABER_OFFENSE:
			case FP_SABER_DEFENSE:
			case FP_SABERTHROW:
				if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
					PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 13\n", level.time - level.startTime);
				return qfalse;
				break;
			}
		}
	}

	if ( !self->client->ps.saberHolstered )
	{
		if ( (self->client->saber[0].saberFlags&SFL_TWO_HANDED) )
		{
			if ( g_saberRestrictForce.integer )
			{
				switch ( forcePower )
				{
				case FP_PUSH:
				case FP_PULL:
				case FP_TELEPATHY:
				case FP_GRIP:
				case FP_LIGHTNING:
				case FP_DRAIN:
					if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
						PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 14\n", level.time - level.startTime);
					return qfalse;
					break;
				}
			}
		}

		if ( (self->client->saber[0].saberFlags&SFL_TWO_HANDED)
			|| (self->client->saber[0].model && self->client->saber[0].model[0]) )
		{//this saber requires the use of two hands OR our other hand is using an active saber too
			if ( (self->client->saber[0].forceRestrictions&(1<<forcePower)) )
			{//this power is verboten when using this saber
				if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
					PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 15\n", level.time - level.startTime);
				return qfalse;
			}
		}

		if ( self->client->saber[0].model 
			&& self->client->saber[0].model[0] )
		{//both sabers on
			if ( g_saberRestrictForce.integer )
			{
				switch ( forcePower )
				{
				case FP_PUSH:
				case FP_PULL:
				case FP_TELEPATHY:
				case FP_GRIP:
				case FP_LIGHTNING:
				case FP_DRAIN:
					return qfalse;
					break;
				}
			}
			if ( (self->client->saber[1].forceRestrictions&(1<<forcePower)) )
			{//this power is verboten when using this saber
				if (d_debugThTeLOS.integer && forcePower == FP_TEAM_FORCE)
					PrintIngame(self - g_entities, "(%d) WP_ForcePowerUsable: error 16\n", level.time - level.startTime);
				return qfalse;
			}
		}
	}
	return WP_ForcePowerAvailable( self, forcePower, 0 );	// OVERRIDEFIXME
}

int WP_AbsorbConversion(gentity_t *attacked, int atdAbsLevel, gentity_t *attacker, int atPower, int atPowerLevel, int atForceSpent)
{
	int getLevel = 0;
	int addTot = 0;
	gentity_t *abSound;

	if (atPower != FP_LIGHTNING &&
		atPower != FP_DRAIN &&
		atPower != FP_GRIP &&
		atPower != FP_PUSH &&
		atPower != FP_PULL)
	{ //Only these powers can be absorbed
		return -1;
	}

	if (!atdAbsLevel)
	{ //looks like attacker doesn't have any absorb power
		return -1;
	}

	if (!(attacked->client->ps.fd.forcePowersActive & (1 << FP_ABSORB)))
	{ //absorb is not active
		return -1;
	}

	qboolean meme = (!level.wasRestarted && atPower == FP_GRIP && attacker && attacker->client && attacker->client->account && (!Q_stricmp(attacker->client->account->name, "duo") || !Q_stricmp(attacker->client->account->name, "alpha")));
	if (meme)
		return -1;

	//Subtract absorb power level from the offensive force power
	getLevel = atPowerLevel;
	getLevel -= atdAbsLevel;

	if (getLevel < 0)
	{
		getLevel = 0;
	}

	//let the attacker absorb an amount of force used in this attack based on his level of absorb
	addTot = (atForceSpent/3)*attacked->client->ps.fd.forcePowerLevel[FP_ABSORB];

	if (addTot < 1 && atForceSpent >= 1)
	{
		addTot = 1;
	}

	// we will definitely absorb, add to attacker and attacked stats if they are on different teams
	if ( attacked && attacked->client && attacker && attacker->client
		&& attacker->client->sess.sessionTeam != attacked->client->sess.sessionTeam ) {
		int absorbed = attacked->client->ps.fd.forcePower + addTot > 100 ? 100 - attacked->client->ps.fd.forcePower : addTot;
		attacked->client->stats->absorbed += absorbed;
		attacker->client->stats->energizedEnemy += absorbed;
	}

	attacked->client->ps.fd.forcePower += addTot;
	if (attacked->client->ps.fd.forcePower > 100)
	{
		attacked->client->ps.fd.forcePower = 100;
	}

	//play sound indicating that attack was absorbed
	if (attacked->client->forcePowerSoundDebounce < level.time)
	{
		abSound = G_PreDefSound(attacked, PDSOUND_ABSORBHIT);
		abSound->s.trickedentindex = attacked->s.number;

		attacked->client->forcePowerSoundDebounce = level.time + 400;
	}

	return getLevel;
}

void WP_ForcePowerRegenerate( gentity_t *self, int overrideAmt )
{ //called on a regular interval to regenerate force power.
	if ( !self->client )
	{
		return;
	}

	if ( overrideAmt )
	{ //custom regen amount
		self->client->ps.fd.forcePower += overrideAmt;
	}
	else
	{ //otherwise, just 1
		self->client->ps.fd.forcePower++;
	}

	if ( self->client->ps.fd.forcePower >= self->client->ps.fd.forcePowerMax )
	{ //cap it off at the max (default 100)
		self->client->ps.fd.forcePower = self->client->ps.fd.forcePowerMax;

		/*
		If i'm standing with full force..
		.. and not moving
		.. and not using any forcepower
		.. and not carrying a flag
		.. and not in a special animation
		*/
		if ( !self->client->ps.velocity[0] && !self->client->ps.velocity[1] && !self->client->ps.velocity[2] &&
			!self->client->ps.fd.forcePowersActive &&
			!( self->client->ps.powerups[PW_REDFLAG] || self->client->ps.powerups[PW_BLUEFLAG] ) &&
			!self->client->ps.torsoTimer && !self->client->ps.legsTimer ) {
			// .. then there is no reason to keep the run invalid/making it a weapons run
			self->client->usedWeapon = qfalse;
			self->client->jumpedOrCrouched = qfalse;
			self->client->usedForwardOrBackward = qfalse;
			self->client->runInvalid = qfalse;
		}
	}
}

void WP_ForcePowerStart( gentity_t *self, forcePowers_t forcePower, int overrideAmt )
{ //activate the given force power
	int	duration = 0;
	qboolean hearable = qfalse;
	float hearDist = 0;

	qboolean meme = (!level.wasRestarted && forcePower == FP_GRIP && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
	if (!WP_ForcePowerAvailable( self, forcePower, overrideAmt ) && !meme)
	{
		return;
	}

	if ( BG_FullBodyTauntAnim( self->client->ps.legsAnim ) )
	{//stop taunt
		self->client->ps.legsTimer = 0;
	}
	if ( BG_FullBodyTauntAnim( self->client->ps.torsoAnim ) )
	{//stop taunt
		self->client->ps.torsoTimer = 0;
	}
	//hearable and hearDist are merely for the benefit of bots, and not related to if a sound is actually played.
	//If duration is set, the force power will assume to be timer-based.
	switch( (int)forcePower )
	{
	case FP_HEAL:
		hearable = qtrue;
		hearDist = 256;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_LEVITATION:
		hearable = qtrue;
		hearDist = 256;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_SPEED:
		hearable = qtrue;
		hearDist = 256;
		if (self->client->ps.fd.forcePowerLevel[FP_SPEED] == FORCE_LEVEL_1)
		{
			duration = 10000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SPEED] == FORCE_LEVEL_2)
		{
			duration = 15000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SPEED] == FORCE_LEVEL_3)
		{
			duration = 20000;
		}
		else //shouldn't get here
		{
			break;
		}

		if (overrideAmt)
		{
			duration = overrideAmt;
		}

		duration = FixDuration(duration);
		StartForceTimer(self, (int)forcePower, duration);

		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_PUSH:
		hearable = qtrue;
		hearDist = 256;
		break;
	case FP_PULL:
		hearable = qtrue;
		hearDist = 256;
		break;
	case FP_TELEPATHY:
		hearable = qtrue;
		hearDist = 256;
		if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_1)
		{
			duration = 20000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_2)
		{
			duration = 25000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_3)
		{
			duration = 30000;
		}
		else //shouldn't get here
		{
			break;
		}

		duration = FixDuration(duration);
		StartForceTimer(self, (int)forcePower, duration);

		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_GRIP:
		hearable = qtrue;
		hearDist = 256;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		self->client->ps.powerups[PW_DISINT_4] = level.time + 60000;
		break;
	case FP_LIGHTNING:
		hearable = qtrue;
		hearDist = 512;
		duration = overrideAmt;
		overrideAmt = 0;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		self->client->ps.activeForcePass = self->client->ps.fd.forcePowerLevel[FP_LIGHTNING];
		break;
	case FP_RAGE:
		hearable = qtrue;
		hearDist = 256;
		if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_1)
		{
			duration = 8000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_2)
		{
			duration = 14000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_3)
		{
			duration = 20000;
		}
		else //shouldn't get here
		{
			break;
		}
		duration = FixDuration(duration);
		StartForceTimer(self, (int)forcePower, duration);
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		self->client->pers.ragesince = level.time; // force stats
		break;
	case FP_PROTECT:
		hearable = qtrue;
		hearDist = 256;
		duration = 20000;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		self->client->pers.protsince = level.time; // force stats
		duration = FixDuration(duration);
		StartForceTimer(self, (int)forcePower, duration);
		break;
	case FP_ABSORB:
		hearable = qtrue;
		hearDist = 256;
		duration = 20000;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		duration = FixDuration(duration);
		StartForceTimer(self, (int)forcePower, duration);
		break;
	case FP_TEAM_HEAL:
		hearable = qtrue;
		hearDist = 256;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_TEAM_FORCE:
		hearable = qtrue;
		hearDist = 256;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_DRAIN:
		hearable = qtrue;
		hearDist = 256;
		duration = overrideAmt;
		overrideAmt = 0;
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_SEE:
		hearable = qtrue;
		hearDist = 256;
		if (self->client->ps.fd.forcePowerLevel[FP_SEE] == FORCE_LEVEL_1)
		{
			duration = 10000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SEE] == FORCE_LEVEL_2)
		{
			duration = 20000;
		}
		else if (self->client->ps.fd.forcePowerLevel[FP_SEE] == FORCE_LEVEL_3)
		{
			duration = 30000;
		}
		else //shouldn't get here
		{
			break;
		}
		duration = FixDuration(duration);
		StartForceTimer(self, (int)forcePower, duration);
		self->client->ps.fd.forcePowersActive |= ( 1 << forcePower );
		break;
	case FP_SABER_OFFENSE:
		break;
	case FP_SABER_DEFENSE:
		break;
	case FP_SABERTHROW:
		break;
	default:
		break;
	}

	if ( duration )
	{
		self->client->ps.fd.forcePowerDuration[forcePower] = level.time + duration;
	}
	else
	{
		self->client->ps.fd.forcePowerDuration[forcePower] = 0;
	}

	if (hearable)
	{
		self->client->ps.otherSoundLen = hearDist;
		self->client->ps.otherSoundTime = level.time + 100;
	}
	
	self->client->ps.fd.forcePowerDebounce[forcePower] = 0;

	if ((int)forcePower == FP_SPEED && overrideAmt)
	{
		BG_ForcePowerDrain( &self->client->ps, forcePower, overrideAmt*0.025 );
	}
	else if ((int)forcePower != FP_GRIP && (int)forcePower != FP_DRAIN)
	{ //grip and drain drain as damage is done
		BG_ForcePowerDrain( &self->client->ps, forcePower, overrideAmt );
	}
}

void ForceHeal( gentity_t *self )
{
	if (self->isAimPracticePack)
		return;
	if ( self->health <= 0 )
	{
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_HEAL ) )
	{
		return;
	}

	if ( self->health >= self->client->ps.stats[STAT_MAX_HEALTH])
	{
		return;
	}

#if 0
	// do we need to add this?
	if (self->client->fakeForceAlignment == FAKEFORCEALIGNMENT_DARK)
		return;
	SetFakeForceAlignmentOfBoostedBase(self, FORCE_LIGHTSIDE, qfalse);
#endif

	if (self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_3)
	{
		self->health += 25; //This was 50, but that angered the Balance God.
		
		if (self->health > self->client->ps.stats[STAT_MAX_HEALTH])
		{
			self->health = self->client->ps.stats[STAT_MAX_HEALTH];
		}
		BG_ForcePowerDrain( &self->client->ps, FP_HEAL, 0 );
	}
	else if (self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_2)
	{
		self->health += 10;
		
		if (self->health > self->client->ps.stats[STAT_MAX_HEALTH])
		{
			self->health = self->client->ps.stats[STAT_MAX_HEALTH];
		}
		BG_ForcePowerDrain( &self->client->ps, FP_HEAL, 0 );
	}
	else
	{
		self->health += 5;
		
		if (self->health > self->client->ps.stats[STAT_MAX_HEALTH])
		{
			self->health = self->client->ps.stats[STAT_MAX_HEALTH];
		}
		BG_ForcePowerDrain( &self->client->ps, FP_HEAL, 0 );
	}

	G_Sound( self, CHAN_ITEM, G_SoundIndex("sound/weapons/force/heal.wav") );
}

void WP_AddToClientBitflags(gentity_t *ent, int entNum)
{
	if (!ent)
	{
		return;
	}

	if (entNum > 47)
	{
		ent->s.trickedentindex4 |= (1 << (entNum-48));
	}
	else if (entNum > 31)
	{
		ent->s.trickedentindex3 |= (1 << (entNum-32));
	}
	else if (entNum > 15)
	{
		ent->s.trickedentindex2 |= (1 << (entNum-16));
	}
	else
	{
		ent->s.trickedentindex |= (1 << entNum);
	}
}

void ShouldUseTHTE(gentity_t *target, qboolean *doTEOut, qboolean *doTHOut, int fakeForceAlignment) {
	if (!target || !target->client || (!doTEOut && !doTHOut)) {
		assert(qfalse);
		return;
	}

	const int fp = target->client->ps.fd.forcePower;
	int hp = target->health;
	const qboolean speedActive = !!(target->client->ps.fd.forcePowersActive & (1 << FP_SPEED));
	const qboolean hasFlag = HasFlag(target);
	const qboolean hasRage = !!(target->client->ps.fd.forcePowersKnown & (1 << FP_RAGE));
	const qboolean rageActive = !!(hasRage && target->client->ps.fd.forcePowersActive & (1 << FP_RAGE));

	qboolean gotHealthPack = qfalse, healthPackIsSuperClose = qfalse;
	for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
		const gentity_t *medpack = &g_entities[i];
		if (!medpack->inuse || !medpack->item || (medpack->item->giType != IT_HEALTH && medpack->item->giType != IT_ARMOR))
			continue;
		if (medpack->s.eFlags & EF_NODRAW)
			continue; // waiting to respawn
		float dist = Distance2D(target->client->ps.origin, medpack->s.origin);
		if (dist > 800)
			continue;
		//int value = (medpack->item->giType == IT_ARMOR && medpack->item->giTag == 2) ? 100 : 25;
		//PrintIngame(-1, "Distance to %s: %d\n", medpack->item->giType == IT_ARMOR ? "armor" : "healthpack", (int)dist);

		gotHealthPack = qtrue;
		if (dist <= 400) {
			healthPackIsSuperClose = qtrue;
			break;
		}
	}

	if (gotHealthPack) {
		if (healthPackIsSuperClose)
			hp += 25; // consider us slightly higher hp if the health pack is very close
		else
			hp += 20;
	}

	qboolean doTE = qfalse, doTH = qfalse;

	if (fakeForceAlignment == FAKEFORCEALIGNMENT_LIGHT) {
		if (hp <= 75)
			doTH = qtrue;
	}
	else if (fakeForceAlignment == FAKEFORCEALIGNMENT_DARK) {
		if (fp <= 75)
			doTE = qtrue;
	}
	else {
		if (hp <= 5) {
			doTH = qtrue;
		}
		else if (hp <= 20) {
			if (fp <= 55 && !(target->client->drainDebuffTime >= level.time && g_drainRework.integer))
				doTE = qtrue;
			else
				doTH = qtrue;
		}
		else if (hp <= 65) {
			if (fp <= 65 && !(target->client->drainDebuffTime >= level.time && g_drainRework.integer)) {
				doTE = qtrue;
			}
			else {
				if (speedActive || (hasFlag && hasRage && rageActive))
					doTH = qtrue;
			}
		}
		else {
			if (fp <= 65) {
				doTE = qtrue;
			}
			else if (fp < 75) {
				if (speedActive || (hasFlag && hasRage && rageActive))
					doTE = qtrue;
			}
		}
	}

	if (target->client->drainDebuffTime >= level.time && g_drainRework.integer)
		doTE = qfalse;

	if (doTEOut)
		*doTEOut = doTE;
	if (doTHOut)
		*doTHOut = doTH;
}

// don't th/te a single target if our fc is kinda near
// e.g. don't spawn and te some random guy who happens to be next to you
// when you could walk for a couple seconds and then te your fc
qboolean DoesntHaveFlagButMyFCIsKindaNear(gentity_t *self, gentity_t *target) {
	if (!self || !self->client || !target || !target->client) {
		assert(qfalse);
		return qfalse;
	}

	if (target->health <= 0 || HasFlag(target))
		return qfalse;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *fc = &g_entities[i];
		if (fc == self || fc == target)
			continue;
		if (!fc->inuse || !fc->client || fc->client->pers.connected != CON_CONNECTED || !HasFlag(fc) || fc->client->ps.fallingToDeath)
			continue;
		if (fc->health <= 0 || IsRacerOrSpectator(fc) || fc->client->sess.sessionTeam != self->client->sess.sessionTeam)
			continue;

		// this guy is an allied fc
		float dist = Distance2D(self->client->ps.origin, fc->client->ps.origin);
		if (dist > 2000)
			continue; // but he's far away

		// he's close
		qboolean shouldTEFC = qfalse, shouldTHFC = qfalse;
		ShouldUseTHTE(fc, &shouldTEFC, &shouldTHFC, self->client->fakeForceAlignment);
		if (!shouldTEFC && !shouldTHFC)
			continue; // but he doesn't need th or te

		// this allied fc is close and needs th/te. don't allow teing/thing someone else
		return qtrue;
	}

	return qfalse;
}

qboolean HaveLOSToTarget(gentity_t *self, gentity_t *ent) {
	if (!self || !self->client || !ent || !ent->client)
		return qfalse;

	if (d_debugThTeLOS.integer)
		PrintIngame(self - g_entities, "(%d) Checking LOS to %s:", level.time - level.startTime, ent->client->pers.netname);
	for (int i = 0; i < 3; i++) {
		vec3_t from;
		from[0] = self->client->ps.origin[0];
		from[1] = self->client->ps.origin[1];
		switch (i) {
		case 0:
			from[2] = self->client->ps.origin[2];
			break;
		case 1:
			if (self->client->ps.pm_flags & PMF_DUCKED || self->client->ps.pm_flags & PMF_ROLLING)
				continue;
			from[2] = self->client->ps.origin[2] + 0.5 * self->client->ps.viewheight;
			break;
		case 2:
			from[2] = self->client->ps.origin[2] + self->client->ps.viewheight;
			break;
		}

		for (int j = 0; j < 3; j++) {
			vec3_t to;
			to[0] = ent->client->ps.origin[0];
			to[1] = ent->client->ps.origin[1];

			switch (j) {
			case 0:
				to[2] = ent->client->ps.origin[2];
				break;
			case 1:
				if (ent->client->ps.pm_flags & PMF_DUCKED || ent->client->ps.pm_flags & PMF_ROLLING)
					continue;
				to[2] = ent->client->ps.origin[2] + 0.5 * ent->client->ps.viewheight;
				break;
			case 2:
				to[2] = ent->client->ps.origin[2] + ent->client->ps.viewheight;
				break;
			}

			trace_t tr;
			trap_Trace(&tr, from, NULL, NULL, to, self->s.number, MASK_PLAYERSOLID);
			if (tr.entityNum != ent->s.number) {
				if (tr.entityNum < MAX_CLIENTS) {
					// recursive call to check if the entity blocking LOS has LOS to the target
					if (HaveLOSToTarget(&g_entities[tr.entityNum], ent)) {
						if (d_debugThTeLOS.integer)
							PrintIngame(self - g_entities, "got LOS from recursive check on i %d, j%d\n", i, j);
						return qtrue;
					}
				}
				if (d_debugThTeLOS.integer)
					PrintIngame(self - g_entities, "^1i %d, j %d...^7", i, j);
				continue;
			}

			if (d_debugThTeLOS.integer)
				PrintIngame(self - g_entities, "^2got LOS from i %d, j %d^7\n", i, j);
			return qtrue;
		}
	}

	if (d_debugThTeLOS.integer)
		PrintIngame(self - g_entities, "^1failed!^7\n");
	return qfalse;
}



void ForceTeamHeal( gentity_t *self, qboolean redirectedTE )
{
	if (self->isAimPracticePack)
		return;
	float radius = 256;
	gentity_t *ent;
	vec3_t a;
	int numpl = 0;
	int pl[MAX_CLIENTS];
	int healthadd = 0;
	gentity_t *te = NULL;
	qboolean baseBoost = !!(self->client->account && self->client->account->flags & ACCOUNTFLAG_BOOST_BASEAUTOTHTEBOOST && g_boost.integer && GetRemindedPosOrDeterminedPos(self) == CTFPOSITION_BASE);

	if ( self->health <= 0 )
	{
		return;
	}

	if (baseBoost && self->client->fakeForceAlignment == FAKEFORCEALIGNMENT_DARK) {
		gentity_t *fc = GetFC(self->client->sess.sessionTeam, self, qfalse, 0);
		if (fc) {
			qboolean shouldTe = qfalse;
			ShouldUseTHTE(fc, &shouldTe, NULL, FAKEFORCEALIGNMENT_DARK);
			if (shouldTe)
				ForceTeamForceReplenish(self, qtrue);
		}
		return;
	}

	const int evaluateThisForcePower = (self->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_HEAL)) ? FP_TEAM_HEAL : FP_TEAM_FORCE;

	if ( !WP_ForcePowerUsable( self, evaluateThisForcePower) )
	{
		return;
	}

	if (self->client->ps.fd.forcePowerDebounce[evaluateThisForcePower] >= level.time)
	{
		return;
	}

	if (self->client->ps.fd.forcePowerLevel[evaluateThisForcePower] == FORCE_LEVEL_2)
	{
		radius *= 1.5;
	}
	if (self->client->ps.fd.forcePowerLevel[evaluateThisForcePower] == FORCE_LEVEL_3)
	{
		radius *= 2;
	}

	qboolean compensate = self->client->sess.unlagged;
	if (g_thTeUnlagged.integer && g_unlagged.integer && compensate)
		G_TimeShiftAllClients(trap_Milliseconds() - (level.time - self->client->pers.cmd.serverTime), self, qfalse);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		ent = &g_entities[i];

		if (ent && ent->client && self != ent && OnSameTeam(self, ent) && ent->client->ps.stats[STAT_HEALTH] < ent->client->ps.stats[STAT_MAX_HEALTH] && ent->client->ps.stats[STAT_HEALTH] > 0 && ForcePowerUsableOn(self, ent, FP_TEAM_HEAL) > 0 /*&& !(target->client->drainDebuffTime >= level.time && g_drainRework.integer)*/ )
		{
			if (!g_thTeRequiresLOS.integer && !trap_InPVS(self->client->ps.origin, ent->client->ps.origin)) {
				continue;
			}

			VectorSubtract(self->client->ps.origin, ent->client->ps.origin, a);

			if (VectorLength(a) > radius)
				continue;

			if (level.usesAbseil) {
				// see if we can trace to an abseil brush
				trace_t tr;
				trap_Trace(&tr, self->client->ps.origin, NULL, NULL, ent->client->ps.origin, self->s.number, CONTENTS_ABSEIL);
				if (tr.fraction != 1) {
					if (!HaveLOSToTarget(self, ent))
						continue;
				}
			}

			if (g_thTeRequiresLOS.integer) {
				if (!HaveLOSToTarget(self, ent))
					continue;
			}

			pl[numpl] = i;
			numpl++;
		}
	}

	if (baseBoost && numpl == 1 && DoesntHaveFlagButMyFCIsKindaNear(self, &g_entities[pl[0]])) {
		if (g_thTeUnlagged.integer && g_unlagged.integer && compensate)
			G_UnTimeShiftAllClients(self, qfalse);
		return;
	}

	qboolean canEnergizeOnly = qfalse;
	if (numpl < 1)
	{
		if (!baseBoost || redirectedTE) {
			if (g_thTeUnlagged.integer && g_unlagged.integer && compensate)
				G_UnTimeShiftAllClients(self, qfalse);
			return;
		}
		canEnergizeOnly = qtrue;
		numpl = 0;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			ent = &g_entities[i];

			if (ent && ent->client && self != ent && OnSameTeam(self, ent) && /*ent->client->ps.stats[STAT_HEALTH] < ent->client->ps.stats[STAT_MAX_HEALTH] && */ent->client->ps.stats[STAT_HEALTH] > 0 && ForcePowerUsableOn(self, ent, FP_TEAM_HEAL) > 0 &&
				trap_InPVS(self->client->ps.origin, ent->client->ps.origin) && !(ent->client->drainDebuffTime >= level.time && g_drainRework.integer))
			{
				VectorSubtract(self->client->ps.origin, ent->client->ps.origin, a);

				if (VectorLength(a) > radius)
					continue;

				if (level.usesAbseil) {
					// see if we can trace to an abseil brush
					trace_t tr;
					trap_Trace(&tr, self->client->ps.origin, NULL, NULL, ent->client->ps.origin, self->s.number, CONTENTS_ABSEIL);
					if (tr.fraction != 1) {
						if (!HaveLOSToTarget(self, ent))
							continue;
					}
				}

				if (g_thTeRequiresLOS.integer) {
					if (!HaveLOSToTarget(self, ent))
						continue;
				}

				pl[numpl] = i;
				numpl++;
			}
		}
	}

	if (g_thTeUnlagged.integer && g_unlagged.integer && compensate)
		G_UnTimeShiftAllClients(self, qfalse);

	if (numpl < 1)
		return;

	if (baseBoost && numpl == 1 && !redirectedTE) {
		qboolean doTE = qfalse, doTH = qfalse;
		ShouldUseTHTE(&g_entities[pl[0]], &doTE, &doTH, self->client->fakeForceAlignment);

		if (doTE) {
			//PrintIngame(-1, "^5Team energize\n");
			ForceTeamForceReplenish(self, qtrue);
			return;
		}
		else if (!doTH) {
			return;
		}
	}

	if (canEnergizeOnly)
		return;

	//this entity will definitely use TH, log it

	SetFakeForceAlignmentOfBoostedBase(self, FORCE_LIGHTSIDE, qtrue);

	if (numpl == 1)
	{
		if (self->client->ps.fd.forceSide == FORCE_DARKSIDE && baseBoost)
			healthadd = Com_Clampi(1, 50, g_boost_maxDarkTh.integer);
		else
			healthadd = 50;
	}
	else if (numpl == 2)
	{
		healthadd = 33;
	}
	else
	{
		healthadd = 25;
	}

	self->client->ps.fd.forcePowerDebounce[evaluateThisForcePower] = level.time + 2000;
	if (g_broadcastGivenThTe.integer) {
		const char *str = "kls -1 -1 \"ygt\" \"1\"";
		trap_SendServerCommand(self - g_entities, str);
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (thisEnt == self || !thisEnt->inuse || !thisEnt->client || thisEnt->client->sess.spectatorState != SPECTATOR_FOLLOW || thisEnt->client->sess.spectatorClient != self - g_entities)
				continue;
			trap_SendServerCommand(i, str);
		}
	}

	for (int i = 0; i < numpl; i++)
	{
		if (g_entities[pl[i]].client->ps.stats[STAT_HEALTH] > 0 &&
			g_entities[pl[i]].health > 0)
		{
			// using TH on this ally
			if ( self && self->client ) {
				self->client->stats->healed += ( ( g_entities[pl[i]].client->ps.stats[STAT_HEALTH] + healthadd > g_entities[pl[i]].client->ps.stats[STAT_MAX_HEALTH] ) ? ( g_entities[pl[i]].client->ps.stats[STAT_MAX_HEALTH] - g_entities[pl[i]].client->ps.stats[STAT_HEALTH] ) : healthadd );
			}

			g_entities[pl[i]].client->ps.stats[STAT_HEALTH] += healthadd;
			if (g_entities[pl[i]].client->ps.stats[STAT_HEALTH] > g_entities[pl[i]].client->ps.stats[STAT_MAX_HEALTH])
			{
				g_entities[pl[i]].client->ps.stats[STAT_HEALTH] = g_entities[pl[i]].client->ps.stats[STAT_MAX_HEALTH];
			}

			g_entities[pl[i]].health = g_entities[pl[i]].client->ps.stats[STAT_HEALTH];

			//At this point we know we got one, so add him into the collective event client bitflag
			if (!te)
			{
				te = G_TempEntity(self->client->ps.origin, EV_TEAM_POWER);
				te->s.eventParm = 1; //eventParm 1 is heal, eventParm 2 is force regen
				G_ApplyRaceBroadcastsToEvent(self, te);

				//since we had an extra check above, do the drain now because we got at least one guy
				BG_ForcePowerDrain( &self->client->ps, FP_TEAM_HEAL, forcePowerNeeded[self->client->ps.fd.forcePowerLevel[evaluateThisForcePower]][FP_TEAM_HEAL] );
			}

			WP_AddToClientBitflags(te, pl[i]);

			//Now cramming it all into one event.. doing this many g_sound events at once was a Bad Thing.
		}
	}
}

void ForceTeamForceReplenish( gentity_t *self, qboolean redirectedTH )
{
	if (self->isAimPracticePack)
		return;
	float radius = 256;
	gentity_t *ent;
	vec3_t a;
	int numpl = 0;
	int pl[MAX_CLIENTS];
	int poweradd = 0;
	gentity_t *te = NULL;
	qboolean baseBoost = !!(self->client->account && self->client->account->flags & ACCOUNTFLAG_BOOST_BASEAUTOTHTEBOOST && g_boost.integer && GetRemindedPosOrDeterminedPos(self) == CTFPOSITION_BASE);

	if ( self->health <= 0 )
	{
		return;
	}

	if (baseBoost && self->client->fakeForceAlignment == FAKEFORCEALIGNMENT_LIGHT) {
		gentity_t *fc = GetFC(self->client->sess.sessionTeam, self, qfalse, 0);
		if (fc) {
			qboolean shouldTh = qfalse;
			ShouldUseTHTE(fc, NULL, &shouldTh, FAKEFORCEALIGNMENT_LIGHT);
			if (shouldTh)
				ForceTeamHeal(self, qtrue);
		}
		return;
	}

	const int evaluateThisForcePower = (self->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_FORCE)) ? FP_TEAM_FORCE : FP_TEAM_HEAL;
	if ( !WP_ForcePowerUsable( self, evaluateThisForcePower) )
	{
		if (d_debugThTeLOS.integer)
			PrintIngame(self - g_entities, "(%d) ^8TE is not usable.\n", level.time - level.startTime);
		return;
	}

	if (self->client->ps.fd.forcePowerDebounce[evaluateThisForcePower] >= level.time)
	{
		if (d_debugThTeLOS.integer)
			PrintIngame(self - g_entities, "(%d) ^6TE is on cooldown.\n", level.time - level.startTime);
		return;
	}

	if (self->client->ps.fd.forcePowerLevel[evaluateThisForcePower] == FORCE_LEVEL_2)
	{
		radius *= 1.5;
	}
	if (self->client->ps.fd.forcePowerLevel[evaluateThisForcePower] == FORCE_LEVEL_3)
	{
		radius *= 2;
	}

	qboolean compensate = self->client->sess.unlagged;
	if (g_thTeUnlagged.integer && g_unlagged.integer && compensate)
		G_TimeShiftAllClients(trap_Milliseconds() - (level.time - self->client->pers.cmd.serverTime), self, qfalse);
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		ent = &g_entities[i];

		if (ent && ent->client && self != ent && OnSameTeam(self, ent)
			&& ent->client->ps.stats[STAT_HEALTH] > 0 /* *CHANGE 60* try to TE only living mates */
			&& ent->client->ps.fd.forcePower < 100 && ForcePowerUsableOn(self, ent, FP_TEAM_FORCE) > 0 &&
			!(g_drainRework.integer && ent->client->drainDebuffTime >= level.time))
		{
			if (!g_thTeRequiresLOS.integer && !trap_InPVS(self->client->ps.origin, ent->client->ps.origin)) {
				if (d_debugThTeLOS.integer)
					PrintIngame(self - g_entities, "(%d) %s^7 ^1is not within PVS.^7\n", level.time - level.startTime, ent->client->pers.netname);
				continue;
			}

			VectorSubtract(self->client->ps.origin, ent->client->ps.origin, a);

			if (VectorLength(a) > radius) {
				if (d_debugThTeLOS.integer)
					PrintIngame(self - g_entities, "(%d) %s^7 ^1is too far away.^7\n", level.time - level.startTime, ent->client->pers.netname);
				continue;
			}

			if (level.usesAbseil) {
				// see if we can trace to an abseil brush
				trace_t tr;
				trap_Trace(&tr, self->client->ps.origin, NULL, NULL, ent->client->ps.origin, self->s.number, CONTENTS_ABSEIL);
				if (tr.fraction != 1) {
					if (!HaveLOSToTarget(self, ent))
						continue;
				}
			}

			if (g_thTeRequiresLOS.integer) {
				if (!HaveLOSToTarget(self, ent)) {
					if (d_debugThTeLOS.integer)
						PrintIngame(self - g_entities, "(%d) %s^7 ^1no line of sight.^7\n", level.time - level.startTime, ent->client->pers.netname);
					continue;
				}
			}

			pl[numpl] = i;
			numpl++;
		}
	}

	if (baseBoost && numpl == 1 && DoesntHaveFlagButMyFCIsKindaNear(self, &g_entities[pl[0]])) {
		if (g_thTeUnlagged.integer && g_unlagged.integer && compensate)
			G_UnTimeShiftAllClients(self, qfalse);
		return;
	}

	qboolean canHealOnly = qfalse;
	if (numpl < 1)
	{
		if (!baseBoost || redirectedTH) {
			if (g_thTeUnlagged.integer && g_unlagged.integer && compensate)
				G_UnTimeShiftAllClients(self, qfalse);
			return;
		}

		canHealOnly = qtrue;
		numpl = 0;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			ent = &g_entities[i];

			if (ent && ent->client && self != ent && OnSameTeam(self, ent)
				&& ent->client->ps.stats[STAT_HEALTH] > 0 /* *CHANGE 60* try to TE only living mates */
				/*&& ent->client->ps.fd.forcePower < 100*/ && ForcePowerUsableOn(self, ent, FP_TEAM_FORCE) > 0 &&
				trap_InPVS(self->client->ps.origin, ent->client->ps.origin)/* &&
				!(g_drainRework.integer && ent->client->drainDebuffTime >= level.time)*/)
			{
				VectorSubtract(self->client->ps.origin, ent->client->ps.origin, a);

				if (VectorLength(a) > radius)
					continue;

				if (level.usesAbseil) {
					// see if we can trace to an abseil brush
					trace_t tr;
					trap_Trace(&tr, self->client->ps.origin, NULL, NULL, ent->client->ps.origin, self->s.number, CONTENTS_ABSEIL);
					if (tr.fraction != 1) {
						if (!HaveLOSToTarget(self, ent))
							continue;
					}
				}

				if (g_thTeRequiresLOS.integer) {
					if (!HaveLOSToTarget(self, ent))
						continue;
				}

				pl[numpl] = i;
				numpl++;
			}
		}
	}

	if (g_thTeUnlagged.integer && g_unlagged.integer && compensate)
		G_UnTimeShiftAllClients(self, qfalse);

	if (numpl < 1)
		return;

	if (baseBoost && numpl == 1 && !redirectedTH) {
		qboolean doTE = qfalse, doTH = qfalse;
		ShouldUseTHTE(&g_entities[pl[0]], &doTE, &doTH, self->client->fakeForceAlignment);

		if (doTH) {
			//PrintIngame(-1, "^2Team heal\n");
			ForceTeamHeal(self, qtrue);
			return;
		}
		else if (!doTE) {
			return;
		}
	}

	if (canHealOnly)
		return;

	//this entity will definitely use TE, log it
	//++self->client->pers.teamState.te;

	SetFakeForceAlignmentOfBoostedBase(self, FORCE_DARKSIDE, qtrue);

	if (numpl == 1)
	{
		poweradd = 50;
	}
	else if (numpl == 2)
	{
		poweradd = 33;
	}
	else
	{
		poweradd = 25;
	}
	self->client->ps.fd.forcePowerDebounce[evaluateThisForcePower] = level.time + 2000;
	if (g_broadcastGivenThTe.integer) {
		const char *str = "lchat \"ygt\"";
		trap_SendServerCommand(self - g_entities, str);
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (thisEnt == self || !thisEnt->inuse || !thisEnt->client || thisEnt->client->sess.spectatorState != SPECTATOR_FOLLOW || thisEnt->client->sess.spectatorClient != self - g_entities)
				continue;
			trap_SendServerCommand(i, str);
		}
	}

	BG_ForcePowerDrain( &self->client->ps, FP_TEAM_FORCE, forcePowerNeeded[self->client->ps.fd.forcePowerLevel[evaluateThisForcePower]][FP_TEAM_FORCE] );

	int highestAmountEnergizedForAnyone = 0;
	for (int i = 0; i < numpl; i++)
	{
		// using TE on this ally
		if ( self && self->client ) {
			int thisGuyActualAmountEnergized = ((g_entities[pl[i]].client->ps.fd.forcePower + poweradd > 100) ? (100 - g_entities[pl[i]].client->ps.fd.forcePower) : poweradd);
			self->client->stats->energizedAlly += thisGuyActualAmountEnergized;
			g_entities[pl[i]].client->stats->gotEnergizedByAlly += thisGuyActualAmountEnergized;
			if (thisGuyActualAmountEnergized > highestAmountEnergizedForAnyone)
				highestAmountEnergizedForAnyone = thisGuyActualAmountEnergized;

			if (d_logTEData.integer && numpl == 1 && level.wasRestarted) { // log TE data for single-target TEs
				const qboolean hasLOS = HaveLOSToTarget(self, &g_entities[pl[i]]);
				++self->client->stats->teData.numTEs;
				self->client->stats->teData.amountTE += thisGuyActualAmountEnergized;
				if (hasLOS) {
					++self->client->stats->teData.numTEsWithLOS;
					self->client->stats->teData.amountTEWithLOS += thisGuyActualAmountEnergized;
				}

				if (HasFlag(&g_entities[pl[i]])) {
					++self->client->stats->teData.numTEsFC;
					self->client->stats->teData.amountTEFC += thisGuyActualAmountEnergized;
					if (hasLOS) {
						++self->client->stats->teData.numTEsFCWithLOS;
						self->client->stats->teData.amountTEFCWithLOS += thisGuyActualAmountEnergized;
					}
				}
				else if (GetRemindedPosOrDeterminedPos(&g_entities[pl[i]]) == CTFPOSITION_CHASE) {
					++self->client->stats->teData.numTEsChase;
					self->client->stats->teData.amountTEChase += thisGuyActualAmountEnergized;
					if (hasLOS) {
						++self->client->stats->teData.numTEsChaseWithLOS;
						self->client->stats->teData.amountTEChaseWithLOS += thisGuyActualAmountEnergized;
					}
				}
			}
		}
		g_entities[pl[i]].client->ps.fd.forcePower += poweradd;
		if (g_entities[pl[i]].client->ps.fd.forcePower > 100)
		{
			g_entities[pl[i]].client->ps.fd.forcePower = 100;
		}

		//At this point we know we got one, so add him into the collective event client bitflag
		if (!te)
		{
			te = G_TempEntity(self->client->ps.origin, EV_TEAM_POWER);
			te->s.eventParm = 2; //eventParm 1 is heal, eventParm 2 is force regen
			G_ApplyRaceBroadcastsToEvent(self, te);
		}

		WP_AddToClientBitflags(te, pl[i]);
		//Now cramming it all into one event.. doing this many g_sound events at once was a Bad Thing.
	}

	++self->client->stats->numEnergizes;

	float thisEnergizeEfficiency = Com_Clamp(0.0f, 1.0f, ((float)highestAmountEnergizedForAnyone / (float)poweradd)); // e.g. 50/50 and 33/33 are treated the same (1.0)
	self->client->stats->normalizedEnergizeAmounts += thisEnergizeEfficiency;
}

void AutoTHTE(gentity_t *self) {
	assert(self && self->client && self->client->account && g_boost.integer);
	if (self->health <= 0 || self->client->pers.connected != CON_CONNECTED || IsRacerOrSpectator(self) || g_gametype.integer != GT_CTF)
		return;

	ctfPosition_t pos = GetRemindedPosOrDeterminedPos(self);
	if (!pos)
		return;

	if (pos == CTFPOSITION_BASE && !(self->client->account->flags & ACCOUNTFLAG_BOOST_BASEAUTOTHTEBOOST))
		return;
	if (pos == CTFPOSITION_CHASE && !(self->client->account->flags & ACCOUNTFLAG_BOOST_CHASEAUTOTHTEBOOST))
		return;
	if (pos == CTFPOSITION_OFFENSE && !(self->client->account->flags & ACCOUNTFLAG_BOOST_OFFENSEAUTOTHTEBOOST))
		return;

	qboolean canTE = WP_ForcePowerUsable(self, FP_TEAM_FORCE);
	qboolean canTH = WP_ForcePowerUsable(self, FP_TEAM_HEAL);
	if (!canTE && !canTH)
		return;

	int forcePowerLevel;
	if (canTE)
		forcePowerLevel = self->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE];
	else
		forcePowerLevel = self->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL];

	float radius = 256;
	if (forcePowerLevel == 2)
		radius *= 1.5;
	else if (forcePowerLevel == 3)
		radius *= 2;

	/*qboolean compensate = self->client->sess.unlagged;
	if (g_thTeUnlagged.integer && g_unlagged.integer && compensate)
		G_TimeShiftAllClients(trap_Milliseconds() - (level.time - self->client->pers.cmd.serverTime), self, qfalse);*/

	int numpl = 0;
	gentity_t *target = NULL;
	qboolean targetSpawnedRecently = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];

		if (ent && ent->client && self != ent && OnSameTeam(self, ent) &&
			ent->client->ps.stats[STAT_HEALTH] > 0 &&
			trap_InPVS(self->client->ps.origin, ent->client->ps.origin)) {
			vec3_t a;
			VectorSubtract(self->client->ps.origin, ent->client->ps.origin, a);

			if (VectorLength(a) > radius)
				continue;

			if (level.usesAbseil) {
				// see if we can trace to an abseil brush
				trace_t tr;
				trap_Trace(&tr, self->client->ps.origin, NULL, NULL, ent->client->ps.origin, self->s.number, CONTENTS_ABSEIL);
				if (tr.fraction != 1) {
					if (!HaveLOSToTarget(self, ent))
						continue;
				}
			}

			if (g_thTeRequiresLOS.integer) {
				if (!HaveLOSToTarget(self, ent))
					continue;
			}

			numpl++;
			target = ent;

			if (ent->client->pers.lastSpawnTime >= level.time - 500)
				targetSpawnedRecently = qtrue; // try not to te people who sk instantly after they spawn
		}
	}

	/*if (g_thTeUnlagged.integer && g_unlagged.integer && compensate)
		G_UnTimeShiftAllClients(self, qfalse);*/

	if (!numpl)
		return;

	if (pos == CTFPOSITION_BASE) {
		if (/*canTE && */level.time - level.startTime <= 5000) {
			ForceTeamForceReplenish(self, /*qfalse*/qtrue);
		}
		else {
			if (numpl == 1 && !targetSpawnedRecently) {
				if (canTE)
					ForceTeamForceReplenish(self, qfalse);
				else
					ForceTeamHeal(self, qfalse);
			}
		}
	}
	else if (pos == CTFPOSITION_CHASE && numpl == 1 && canTH && target && target->health <= 60 && HasFlag(target)) {
		float loc = GetCTFLocationValue(target);
		if (loc <= 0.4f)
			ForceTeamHeal(self, qfalse);
	}
	else if (pos == CTFPOSITION_OFFENSE && numpl == 1 && target && target->client && HasFlag(target)) {
		if (canTE && level.time - level.startTime <= 5000) {
			ForceTeamForceReplenish(self, qfalse);
		}
		else {
			float loc = GetCTFLocationValue(target);
			if (loc <= 0.4f) {
				if (canTH && target->health <= 60)
					ForceTeamHeal(self, qfalse);
				else if (canTE && target->client->ps.fd.forcePower <= 50)
					ForceTeamForceReplenish(self, qfalse);
			}
			else {
				if (canTH && target->health <= 60 && target->client->ps.fd.forcePower >= 40)
					ForceTeamHeal(self, qfalse);
				else if (canTE && target->health >= 40 && target->client->ps.fd.forcePower <= 50)
					ForceTeamForceReplenish(self, qfalse);
			}
		}
	}
}

void ForceGrip( gentity_t *self )
{
	float maxDist;
	qboolean meme = (!level.wasRestarted && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
	if (meme) {
		maxDist = 8192;
	}
	else {
		maxDist = MAX_GRIP_DISTANCE;
		if (self->isAimPracticePack)
			return;

		if (self->health <= 0)
		{
			return;
		}

		if (self->client->ps.forceHandExtend != HANDEXTEND_NONE)
		{
			return;
		}

		if (self->client->ps.weaponTime > 0)
		{
			return;
		}

		if (self->client->ps.fd.forceGripUseTime > level.time)
		{
			return;
		}

		if (!WP_ForcePowerUsable(self, FP_GRIP))
		{
			return;
		}

		if (g_gripRework.integer == 2 && self->client->ps.fd.forceGripBeingGripped > level.time) {
			return; // no grip wars; first gripper wins
		}
	}

#if 0
	// do we need to add this?
	if (self->client->fakeForceAlignment == FAKEFORCEALIGNMENT_LIGHT)
		return;
	SetFakeForceAlignmentOfBoostedBase(self, FORCE_DARKSIDE, qfalse);
#endif

	trace_t tr;
	vec3_t tfrom, tto, fwd;
	VectorCopy(self->client->ps.origin, tfrom);
	tfrom[2] += self->client->ps.viewheight;
	AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
	tto[0] = tfrom[0] + fwd[0]* maxDist;
	tto[1] = tfrom[1] + fwd[1]* maxDist;
	tto[2] = tfrom[2] + fwd[2]* maxDist;

	qboolean compensate = self->client->sess.unlagged;
	if (g_unlagged.integer && compensate)
		G_TimeShiftAllClients(trap_Milliseconds() - (level.time - self->client->pers.cmd.serverTime), self, qfalse);

	trap_Trace(&tr, tfrom, NULL, NULL, tto, self->s.number, MASK_PLAYERSOLID);

	if (g_unlagged.integer && compensate)
		G_UnTimeShiftAllClients(self, qfalse);

	int usable = 0;
	if (meme && self->client->sess.meme[0]) {
		int num = atoi(self->client->sess.meme);
		if (Q_isanumber(self->client->sess.meme) && num >= 0 && num < MAX_CLIENTS) {
			gentity_t *dank = &g_entities[num];
			if (dank->inuse && dank->client) {
				tr.entityNum = dank - g_entities;
				if (dank->client->sess.sessionTeam == TEAM_SPECTATOR && dank->client->sess.spectatorState == SPECTATOR_FOLLOW)
					StopFollowing(dank);
			}
		}
		else {
			gentity_t *dank = NULL;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *thisGuy = &g_entities[i];
				if (!thisGuy->inuse || !thisGuy->client || !thisGuy->client->account || Q_stricmp(thisGuy->client->account->name, self->client->sess.meme))
					continue;
				dank = thisGuy;
				break;
			}
			if (dank) {
				tr.entityNum = dank - g_entities;
				if (dank->client->sess.sessionTeam == TEAM_SPECTATOR && dank->client->sess.spectatorState == SPECTATOR_FOLLOW)
					StopFollowing(dank);
			}
			else {
				// check for aiming directly at someone
				trace_t tr2;
				vec3_t start, end, forward;
				VectorCopy(self->client->ps.origin, start);
				AngleVectors(self->client->ps.viewangles, forward, NULL, NULL);
				VectorMA(start, 16384, forward, end);
				start[2] += self->client->ps.viewheight;
				trap_G2Trace(&tr2, start, NULL, NULL, end, self->s.number, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
				if (tr2.entityNum >= 0 && tr2.entityNum < MAX_CLIENTS) {
					tr.entityNum = tr2.entityNum;
				}
				else {
					// see who was closest to where we aimed
					float closestDistance = -1;
					int closestPlayer = -1;
					for (int i = 0; i < MAX_CLIENTS; i++) {
						gentity_t *other = &g_entities[i];
						if (!other->inuse || other == self || !other->client || IsRacerOrSpectator(other))
							continue;
						vec3_t difference;
						VectorSubtract(other->client->ps.origin, tr.endpos, difference);
						if (closestDistance == -1 || VectorLength(difference) < closestDistance) {
							closestDistance = VectorLength(difference);
							closestPlayer = i;
						}
					}

					if (closestDistance != -1 && closestPlayer != -1)
						tr.entityNum = closestPlayer;
				}
			}
		}
	}
	if (tr.fraction != 1.0 &&
		tr.entityNum != ENTITYNUM_NONE &&
		g_entities[tr.entityNum].client &&
		(meme || (g_gripRework.integer && g_entities[tr.entityNum].client->ps.fd.forceGripBeingGripped < level.time) || (!g_gripRework.integer && !g_entities[tr.entityNum].client->ps.fd.forceGripCripple && g_entities[tr.entityNum].client->ps.fd.forceGripBeingGripped < level.time))) {

		usable = ForcePowerUsableOn(self, &g_entities[tr.entityNum], FP_GRIP);

		if (meme || (usable > 0 &&
			(g_friendlyFire.integer || !OnSameTeam(self, &g_entities[tr.entityNum])) &&
			Distance(self->client->ps.origin, g_entities[tr.entityNum].client->ps.origin) <= maxDist)) //don't grip someone who's still crippled
		{
			if (g_entities[tr.entityNum].s.number < MAX_CLIENTS && g_entities[tr.entityNum].client->ps.m_iVehicleNum)
			{ //a player on a vehicle
				gentity_t *vehEnt = &g_entities[g_entities[tr.entityNum].client->ps.m_iVehicleNum];
				if (vehEnt->inuse && vehEnt->client && vehEnt->m_pVehicle)
				{
					if (vehEnt->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER ||
						vehEnt->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL)
					{ //push the guy off
						vehEnt->m_pVehicle->m_pVehicleInfo->Eject(vehEnt->m_pVehicle, (bgEntity_t *)&g_entities[tr.entityNum], qfalse);
					}
				}
			}
			self->client->ps.fd.forceGripEntityNum = tr.entityNum;
			g_entities[tr.entityNum].client->ps.fd.forceGripStarted = level.time;
			self->client->ps.fd.forceGripDamageDebounceTime = 0;

			if ((self->client->sess.sessionTeam == TEAM_RED && g_entities[tr.entityNum].client->sess.sessionTeam == TEAM_BLUE) ||
				(self->client->sess.sessionTeam == TEAM_BLUE && g_entities[tr.entityNum].client->sess.sessionTeam == TEAM_RED)) {
				if (self->client->stats)
					self->client->stats->grips++;
				if (g_entities[tr.entityNum].client->stats)
					g_entities[tr.entityNum].client->stats->gotGripped++;
			}

			if (g_gripRework.integer == 2 && self->client->ps.fd.forcePowerLevel[FP_GRIP] < 3) {
				const float xyVelocityCap = 50.0f;

				const float vx = g_entities[tr.entityNum].client->ps.velocity[0];
				const float vy = g_entities[tr.entityNum].client->ps.velocity[1];

				const float speed = sqrtf(vx * vx + vy * vy);

				if (speed > xyVelocityCap && speed > 0) {
					const float scale = xyVelocityCap / speed;
					g_entities[tr.entityNum].client->ps.velocity[0] *= scale;
					g_entities[tr.entityNum].client->ps.velocity[1] *= scale;
				}

			}

			self->client->ps.forceHandExtend = HANDEXTEND_FORCE_HOLD;
			self->client->ps.forceHandExtendTime = level.time + 5000;
		}
		else {
			self->client->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;

			if (usable == -1 && g_gripAbsorbFix.integer) {
				// we failed to *start* a new grip because the person we aimed at was absorbing
				// absorb the force points

				if (!self->client->grippedAnAbsorberTime || level.time - self->client->grippedAnAbsorberTime >= GRIP_DEBOUNCE_TIME) {
					// fix for base jka grip behavior allowing you to freely attempt to grip absorbers with impunity
					// drain 30 force when trying to grip an absorber,
					// but only every 3 seconds. allow you to keep trying to grip people in the meantime.

					// set the cooldown on absorption
					self->client->grippedAnAbsorberTime = level.time;

					// absorb the force (this also plays the sound/effect)
					WP_AbsorbConversion(&g_entities[tr.entityNum], g_entities[tr.entityNum].client->ps.fd.forcePowerLevel[FP_ABSORB], self, FP_GRIP, self->client->ps.fd.forcePowerLevel[FP_GRIP], 30);

					// dock the force from the user
					BG_ForcePowerDrain(&self->client->ps, FP_GRIP, GRIP_DRAIN_AMOUNT);
				}
			}
			return;
		}
	}
	else
	{
		self->client->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;
		return;
	}
}

void ForceSpeed( gentity_t *self, int forceDuration )
{
	if (self->isAimPracticePack)
		return;
	if ( self->health <= 0 )
	{
		if (g_preActivateSpeedWhileDead.integer && !IsRacerOrSpectator(self))
			self->client->pers.pressedSpeedWhileDeadTime = (level.time - level.startTime);
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_SPEED)) )
	{
		WP_ForcePowerStop( self, FP_SPEED );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_SPEED ) )
	{
		return;
	}

	if ( self->client->holdingObjectiveItem >= MAX_CLIENTS  
		&& self->client->holdingObjectiveItem < ENTITYNUM_WORLD )
	{//holding Siege item
		if ( g_entities[self->client->holdingObjectiveItem].genericValue15 )
		{//disables force powers
			return;
		}
	}

	self->client->ps.forceAllowDeactivateTime = level.time + 1500;

	WP_ForcePowerStart( self, FP_SPEED, forceDuration );
	G_Sound( self, CHAN_BODY, G_SoundIndex("sound/weapons/force/speed.wav") );
	G_Sound( self, TRACK_CHANNEL_2, speedLoopSound );
}

void ForceSeeing( gentity_t *self )
{
	if (self->isAimPracticePack)
		return;
	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_SEE)) )
	{
		WP_ForcePowerStop( self, FP_SEE );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_SEE ) )
	{
		return;
	}

	self->client->ps.forceAllowDeactivateTime = level.time + 1500;

	WP_ForcePowerStart( self, FP_SEE, 0 );

	G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/force/see.wav") );
	G_Sound( self, TRACK_CHANNEL_5, seeLoopSound );
}

void ForceProtect( gentity_t *self )
{
	if (self->isAimPracticePack)
		return;
	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_PROTECT)) )
	{
		WP_ForcePowerStop( self, FP_PROTECT );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_PROTECT ) )
	{
		return;
	}

	if (self->client->fakeForceAlignment == FAKEFORCEALIGNMENT_DARK)
		return;
	SetFakeForceAlignmentOfBoostedBase(self, FORCE_LIGHTSIDE, qfalse);

	// Make sure to turn off Force Rage and Force Absorb.
	if (self->client->ps.fd.forcePowersActive & (1 << FP_RAGE) )
	{
		WP_ForcePowerStop( self, FP_RAGE );
	}
	if (self->client->ps.fd.forcePowersActive & (1 << FP_ABSORB) )
	{
		WP_ForcePowerStop( self, FP_ABSORB );
	}

	self->client->ps.forceAllowDeactivateTime = level.time + 1500;

	WP_ForcePowerStart( self, FP_PROTECT, 0 );
	G_PreDefSound(self, PDSOUND_PROTECT);
	G_Sound( self, TRACK_CHANNEL_3, protectLoopSound );
}

void ForceAbsorb( gentity_t *self )
{
	if (self->isAimPracticePack)
		return;
	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_ABSORB)) )
	{
		WP_ForcePowerStop( self, FP_ABSORB );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_ABSORB ) )
	{
		return;
	}


	if (self->client->fakeForceAlignment == FAKEFORCEALIGNMENT_DARK)
		return;
	SetFakeForceAlignmentOfBoostedBase(self, FORCE_LIGHTSIDE, qfalse);

	// Make sure to turn off Force Rage and Force Protection.
	if (self->client->ps.fd.forcePowersActive & (1 << FP_RAGE) )
	{
		WP_ForcePowerStop( self, FP_RAGE );
	}
	if (self->client->ps.fd.forcePowersActive & (1 << FP_PROTECT) )
	{
		WP_ForcePowerStop( self, FP_PROTECT );
	}

	self->client->ps.forceAllowDeactivateTime = level.time + 1500;

	WP_ForcePowerStart( self, FP_ABSORB, 0 );
	G_PreDefSound(self, PDSOUND_ABSORB);
	G_Sound( self, TRACK_CHANNEL_3, absorbLoopSound );
}

void ForceRage( gentity_t *self )
{
	if (self->isAimPracticePack)
		return;
	if (self->aimPracticeEntBeingUsed)
		return; // don't allow practicing dudes to use this
	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_RAGE)) )
	{
		WP_ForcePowerStop( self, FP_RAGE );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_RAGE ) )
	{
		return;
	}

	if (self->client->ps.fd.forceRageRecoveryTime >= level.time)
	{
		return;
	}

	if (self->health < 10)
	{
		return;
	}

#if 0
	// do we need to add this?
	if (self->client->fakeForceAlignment == FAKEFORCEALIGNMENT_LIGHT)
		return;
	SetFakeForceAlignmentOfBoostedBase(self, FORCE_DARKSIDE, qfalse);
#endif

	// Make sure to turn off Force Protection and Force Absorb.
	if (self->client->ps.fd.forcePowersActive & (1 << FP_PROTECT) )
	{
		WP_ForcePowerStop( self, FP_PROTECT );
	}
	if (self->client->ps.fd.forcePowersActive & (1 << FP_ABSORB) )
	{
		WP_ForcePowerStop( self, FP_ABSORB );
	}

	self->client->ps.forceAllowDeactivateTime = level.time + 1500;

	WP_ForcePowerStart( self, FP_RAGE, 0 );

	G_Sound( self, TRACK_CHANNEL_4, G_SoundIndex("sound/weapons/force/rage.wav") );
	G_Sound( self, TRACK_CHANNEL_3, rageLoopSound );
}

void ForceLightning( gentity_t *self )
{
	if (self->isAimPracticePack)
		return;
	if ( self->health <= 0 )
	{
		return;
	}
	if ( (self->client->ps.fd.forcePower < 25 && !g_lightningRework.integer) || !WP_ForcePowerUsable( self, FP_LIGHTNING ) || self->client->ps.fd.forcePower <= 0)
	{
		return;
	}
	if ( self->client->ps.fd.forcePowerDebounce[FP_LIGHTNING] > level.time )
	{//stops it while using it and also after using it, up to 3 second delay
		return;
	}

	qboolean meme = (!level.wasRestarted && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
	if (!meme && self->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (self->client->ps.weaponTime > 0)
	{
		return;
	}
	if (self->client->ps.fd.forceGripBeingGripped > level.time && g_gripRework.integer == 2) {
		return;
	}

#if 0
	// do we need to add this?
	if (self->client->fakeForceAlignment == FAKEFORCEALIGNMENT_LIGHT)
		return;
	SetFakeForceAlignmentOfBoostedBase(self, FORCE_DARKSIDE, qfalse);
#endif

	// *CHANGE 65* fix - release rocket lock, old bug
	BG_ClearRocketLock(&self->client->ps);

	//Shoot lightning from hand
	//using grip anim now, to extend the burst time
	self->client->ps.forceHandExtend = HANDEXTEND_FORCE_HOLD;
	self->client->ps.forceHandExtendTime = level.time + 20000;

	G_Sound( self, CHAN_BODY, G_SoundIndex("sound/weapons/force/lightning") );
	
	WP_ForcePowerStart( self, FP_LIGHTNING, 500 );
}

void ForceLightningDamage( gentity_t *self, gentity_t *traceEnt, vec3_t dir, vec3_t impactPoint )
{
	self->client->dangerTime = level.time;
	self->client->ps.eFlags &= ~EF_INVULNERABLE;
	self->client->invulnerableTimer = 0;

	int flags = 0;
	if (g_lightningRework.integer)
		flags |= (DAMAGE_NO_KNOCKBACK | DAMAGE_NO_ARMOR | DAMAGE_NO_HIT_LOC);

	if ( traceEnt && traceEnt->takedamage )
	{
		if (!traceEnt->client && traceEnt->s.eType == ET_NPC)
		{ //g2animent
			if (traceEnt->s.genericenemyindex < level.time)
			{
				traceEnt->s.genericenemyindex = level.time + 2000;
			}
		}
		if ( traceEnt->client )
		{//an enemy or object
			if (traceEnt->client->noLightningTime >= level.time)
			{ //give them power and don't hurt them.
				traceEnt->client->ps.fd.forcePower++;
				if (traceEnt->client->ps.fd.forcePower > 100)
				{
					traceEnt->client->ps.fd.forcePower = 100;
				}
				return;
			}
			if (ForcePowerUsableOn(self, traceEnt, FP_LIGHTNING) > 0)
			{
				double dps;
				if (!g_lightningRework.integer) {
					dps = 45;
				}
				else {
					if (self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] <= 1) {
						dps = 10;
					}
					else {
						float dist = Distance(self->r.currentOrigin, traceEnt->r.currentOrigin);
						if (self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] == 2) {
							if (dist <= 300)
								dps = 40;
							else if (dist <= 400)
								dps = 20;
							else
								dps = 10;
						}
						else {
							if (dist <= 300)
								dps = 90;
							else if (dist <= 400)
								dps = 25;
							else if (dist <= 500)
								dps = 20;
							else if (dist <= 600)
								dps = 15;
							else
								dps = 10;
						}
					}
				}

				// add dps divided by tickrate to a static number and deal damage when it's >= 1
				static double damageAccumulator[MAX_GENTITIES] = { 0.0 };
				damageAccumulator[self - g_entities] += dps / (double)(g_svfps.integer);
				int dmg = (int)damageAccumulator[self - g_entities];
				if (dmg >= 1) {
					damageAccumulator[self - g_entities] -= dmg;

					// sanity check
					if (damageAccumulator[self - g_entities] < 0)
						damageAccumulator[self - g_entities] = 0;
				}
				
				int modPowerLevel = -1;
				
				if (traceEnt->client)
				{
					modPowerLevel = WP_AbsorbConversion(traceEnt, traceEnt->client->ps.fd.forcePowerLevel[FP_ABSORB], self, FP_LIGHTNING, self->client->ps.fd.forcePowerLevel[FP_LIGHTNING], 1);
				}

				if (modPowerLevel != -1)
				{
					// absorbed
					if (!modPowerLevel)
					{
						dmg = 0;
						traceEnt->client->noLightningTime = level.time + 400;
					}
					else if (modPowerLevel == 1)
					{
						dmg = 1;
						traceEnt->client->noLightningTime = level.time + 300;
					}
					else if (modPowerLevel == 2)
					{
						dmg = 1;
						traceEnt->client->noLightningTime = level.time + 100;
					}
				}

				if (self->client->ps.weapon == WP_MELEE && self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] > FORCE_LEVEL_2 && !g_lightningRework.integer )
				{//2-handed lightning
					//jackin' 'em up, Palpatine-style
					dmg *= 2;
				}

				if (dmg)
				{
					//rww - Shields can now absorb lightning too.
					G_Damage( traceEnt, self, self, dir, impactPoint, dmg, flags, MOD_FORCE_DARK );
				}
				if ( traceEnt->client )
				{
					if ( !Q_irand( 0, 2 ) )
					{
						G_Sound( traceEnt, CHAN_BODY, G_SoundIndex( va("sound/weapons/force/lightninghit%i", Q_irand(1, 3) )) );
					}

					if (traceEnt->client->ps.electrifyTime < (level.time + 400))
					{ //only update every 400ms to reduce bandwidth usage (as it is passing a 32-bit time value)
						traceEnt->client->ps.electrifyTime = level.time + 800;
					}
					if ( traceEnt->client->ps.powerups[PW_CLOAKED] )
					{//disable cloak temporarily
						Jedi_Decloak( traceEnt );
						traceEnt->client->cloakToggleTime = level.time + Q_irand( 3000, 10000 );
					}
				}
			}
		}
	}
}

void ForceShootLightning( gentity_t *self )
{
	trace_t	tr;
	vec3_t	end, forward;
	gentity_t	*traceEnt;

	if ( self->health <= 0 )
	{
		return;
	}
	AngleVectors( self->client->ps.viewangles, forward, NULL, NULL );
	VectorNormalize( forward );

	vec3_t start;
	VectorCopy(self->client->ps.origin, start);
	if (g_lightningRework.integer)
		start[2] += self->client->ps.viewheight;

	if ( self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] > FORCE_LEVEL_2 && !g_lightningRework.integer )
	{//arc
		vec3_t	center, mins, maxs, dir, ent_org, size, v;
		float	radius = FORCE_LIGHTNING_RADIUS, dot, dist;
		gentity_t	*entityList[MAX_GENTITIES];
		int			iEntityList[MAX_GENTITIES];
		int		e, numListedEntities, i;

		VectorCopy( self->client->ps.origin, center );
		for ( i = 0 ; i < 3 ; i++ ) 
		{
			mins[i] = center[i] - radius;
			maxs[i] = center[i] + radius;
		}
		qboolean compensate = self->client->sess.unlagged;
		if (g_unlagged.integer && compensate)
			G_TimeShiftAllClients(trap_Milliseconds() - (level.time - self->client->pers.cmd.serverTime), self, qfalse);
		numListedEntities = trap_EntitiesInBox( mins, maxs, iEntityList, MAX_GENTITIES );

		i = 0;
		while (i < numListedEntities)
		{
			entityList[i] = &g_entities[iEntityList[i]];

			i++;
		}

		for ( e = 0 ; e < numListedEntities ; e++ ) 
		{
			traceEnt = entityList[e];

			if ( !traceEnt )
				continue;
			if ( traceEnt == self )
				continue;
			if ( traceEnt->r.ownerNum == self->s.number && traceEnt->s.weapon != WP_THERMAL )//can push your own thermals
				continue;
			if ( !traceEnt->inuse )
				continue;
			if ( !traceEnt->takedamage )
				continue;
			if ( traceEnt->health <= 0 )//no torturing corpses
				continue;
			if ( !g_friendlyFire.integer && OnSameTeam(self, traceEnt))
				continue;
			if ( traceEnt->client && traceEnt->client->sess.inRacemode )
				continue; // don't use force powers on racers
			if (traceEnt->isAimPracticePack)
				continue;
			if ( traceEnt->s.eType == ET_MISSILE && traceEnt->r.ownerNum >= 0 && traceEnt->r.ownerNum < MAX_CLIENTS && level.clients[traceEnt->r.ownerNum].sess.inRacemode )
				continue; // don't use force powers on race projectiles
			if (!level.wasRestarted && traceEnt->s.eType == ET_MISSILE && traceEnt->parent && traceEnt->parent->client && traceEnt->parent->client->account && (!Q_stricmp(traceEnt->parent->client->account->name, "duo") || !Q_stricmp(traceEnt->parent->client->account->name, "alpha")))
				continue;
			//this is all to see if we need to start a saber attack, if it's in flight, this doesn't matter
			// find the distance from the edge of the bounding box
			for ( i = 0 ; i < 3 ; i++ ) 
			{
				if ( center[i] < traceEnt->r.absmin[i] ) 
				{
					v[i] = traceEnt->r.absmin[i] - center[i];
				} else if ( center[i] > traceEnt->r.absmax[i] ) 
				{
					v[i] = center[i] - traceEnt->r.absmax[i];
				} else 
				{
					v[i] = 0;
				}
			}

			VectorSubtract( traceEnt->r.absmax, traceEnt->r.absmin, size );
			VectorMA( traceEnt->r.absmin, 0.5, size, ent_org );

			//see if they're in front of me
			//must be within the forward cone
			VectorSubtract( ent_org, center, dir );
			VectorNormalize( dir );
			if ( (dot = DotProduct( dir, forward )) < 0.5 )
				continue;

			//must be close enough
			dist = VectorLength( v );
			if ( dist >= radius ) 
			{
				continue;
			}
		
			//in PVS?
			if ( !traceEnt->r.bmodel && !trap_InPVS( ent_org, start ) )
			{//must be in PVS
				continue;
			}

			//Now check and see if we can actually hit it
			trap_Trace( &tr, start, vec3_origin, vec3_origin, ent_org, self->s.number, MASK_SHOT );
			if ( tr.fraction < 1.0f && tr.entityNum != traceEnt->s.number )
			{//must have clear LOS
				continue;
			}

			// ok, we are within the radius, add us to the incoming list
			ForceLightningDamage( self, traceEnt, dir, ent_org );
		}

		if (g_unlagged.integer && compensate)
			G_UnTimeShiftAllClients(self, qfalse);
	}
	else
	{//trace-line
		int range;
		if (!g_lightningRework.integer)
			range = 2048;
		else if (self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] >= 3)
			range = 700;
		else if (self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] == 2)
			range = 500;
		else
			range = 300;
		VectorMA( start, range, forward, end );
		
		qboolean compensate = self->client->sess.unlagged;
		if (g_unlagged.integer && compensate)
			G_TimeShiftAllClients(trap_Milliseconds() - (level.time - self->client->pers.cmd.serverTime), self, qfalse);

		vec3_t mins, maxs;
		if (!g_lightningRework.integer) {
			VectorCopy(vec3_origin, mins);
			VectorCopy(vec3_origin, maxs);
		}
		else { // slightly bigger trace bounds i guess
			mins[0] = mins[1] = mins[2] = -1 * 3;
			maxs[0] = maxs[1] = maxs[2] = 3;
		}

		trap_Trace( &tr, start, mins, maxs, end, self->s.number, MASK_SHOT );

		if (g_unlagged.integer && compensate)
			G_UnTimeShiftAllClients(self, qfalse);

		if ( tr.entityNum == ENTITYNUM_NONE || tr.fraction == 1.0 || tr.allsolid || tr.startsolid )
		{
			return;
		}
		
		traceEnt = &g_entities[tr.entityNum];
		ForceLightningDamage( self, traceEnt, forward, tr.endpos );
	}
}

void ForceDrain( gentity_t *self )
{
	if (self->isAimPracticePack)
		return;
	if ( self->health <= 0 )
	{
		return;
	}

	qboolean meme = (!level.wasRestarted && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
	if (!meme && self->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (self->client->ps.weaponTime > 0)
	{
		return;
	}

	if ( (self->client->ps.fd.forcePower < 25 && !g_drainRework.integer) || (self->client->ps.fd.forcePower < DRAIN_REWORK2_MINIMUMFORCE && g_drainRework.integer) || !WP_ForcePowerUsable( self, FP_DRAIN ) )
	{
		return;
	}
	if ( self->client->ps.fd.forcePowerDebounce[FP_DRAIN] > level.time )
	{//stops it while using it and also after using it, up to 3 second delay
		return;
	}
	if (self->client->ps.fd.forceGripBeingGripped > level.time && g_gripRework.integer == 2) {
		return;
	}

	if (g_drainRework.integer >= 2) {
		int selfdmg;
		switch (self->client->ps.fd.forcePowerLevel[FP_DRAIN]) {
		case 3: selfdmg = DRAIN_REWORK2_SELFDMG_LEVEL3; break;
		case 2: selfdmg = DRAIN_REWORK2_SELFDMG_LEVEL2; break;
		default: selfdmg = DRAIN_REWORK2_SELFDMG_LEVEL1; break;
		}
		if (self->health <= selfdmg)
			return;
	}

#if 0
	// do we need to add this?
	if (self->client->fakeForceAlignment == FAKEFORCEALIGNMENT_LIGHT)
		return;
	SetFakeForceAlignmentOfBoostedBase(self, FORCE_DARKSIDE, qfalse);
#endif

	self->client->ps.forceHandExtend = HANDEXTEND_FORCE_HOLD;
	if (!g_drainRework.integer)
		self->client->ps.forceHandExtendTime = level.time + 20000;
	else
		self->client->ps.forceHandExtendTime = level.time + 200;

	G_Sound( self, CHAN_BODY, G_SoundIndex("sound/weapons/force/drain.wav") );
	
	if (!g_drainRework.integer)
		WP_ForcePowerStart( self, FP_DRAIN, 500 );
	else
		WP_ForcePowerStart( self, FP_DRAIN, 1 );
}

// returns amount of force drained from the target (-1 if it was absorbed)
int ForceDrainDamage( gentity_t *self, gentity_t *traceEnt, vec3_t dir, vec3_t impactPoint )
{
	gentity_t *tent;
	int actualForceDrainedFromTarget = 0;
	self->client->dangerTime = level.time;
	self->client->ps.eFlags &= ~EF_INVULNERABLE;
	self->client->invulnerableTimer = 0;
	qboolean absorbed = qfalse;

	if ( traceEnt && traceEnt->takedamage )
	{
		if ( traceEnt->client && (!OnSameTeam(self, traceEnt) || g_friendlyFire.integer) && self->client->ps.fd.forceDrainTime < level.time && (traceEnt->client->ps.fd.forcePower || g_drainRework.integer) )
		{//an enemy or object
			if (!traceEnt->client && traceEnt->s.eType == ET_NPC)
			{ //g2animent
				if (traceEnt->s.genericenemyindex < level.time)
				{
					traceEnt->s.genericenemyindex = level.time + 2000;
				}
			}
			if (ForcePowerUsableOn(self, traceEnt, FP_DRAIN) > 0)
			{
				int modPowerLevel = -1;
				int	dmg = 0; 
				if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_1)
				{
					if (!g_drainRework.integer)
						dmg = 2; //because it's one-shot
					else
						dmg = DRAIN_REWORK_FORCEREDUCE_LEVEL1;
				}
				else if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_2)
				{
					if (!g_drainRework.integer)
						dmg = 3;
					else
						dmg = DRAIN_REWORK_FORCEREDUCE_LEVEL2;
				}
				else if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_3)
				{
					if (!g_drainRework.integer)
						dmg = 4;
					else
						dmg = DRAIN_REWORK_FORCEREDUCE_LEVEL3;
				}
			
				if (traceEnt->client)
				{
					modPowerLevel = WP_AbsorbConversion(traceEnt, traceEnt->client->ps.fd.forcePowerLevel[FP_ABSORB], self, FP_DRAIN, self->client->ps.fd.forcePowerLevel[FP_DRAIN],
						!g_drainRework.integer ? 1 : dmg);
					if (modPowerLevel != -1)
						absorbed = qtrue;
				}

				if (modPowerLevel != -1)
				{
					if (!g_drainRework.integer) {
						if (!modPowerLevel)
						{
							dmg = 0;
						}
						else if (modPowerLevel == 1)
						{
							dmg = 1;
						}
						else if (modPowerLevel == 2)
						{
							dmg = 2;
						}
					}
					else {
						dmg = 0;
					}
				}

				if (dmg)
				{
					actualForceDrainedFromTarget = dmg;
					traceEnt->client->ps.fd.forcePower -= (dmg);

					if (traceEnt->client->sess.sessionTeam == OtherTeam(self->client->sess.sessionTeam) && traceEnt->client->stats && self->client->stats) {
						self->client->stats->drain += dmg;
						traceEnt->client->stats->gotDrained += dmg;
					}
				}
				if (traceEnt->client->ps.fd.forcePower < 0)
				{
					actualForceDrainedFromTarget -= abs(traceEnt->client->ps.fd.forcePower);
					traceEnt->client->ps.fd.forcePower = 0;
				}

				if (!g_drainRework.integer) {
					if (self->client->ps.stats[STAT_HEALTH] < self->client->ps.stats[STAT_MAX_HEALTH] &&
						self->health > 0 && self->client->ps.stats[STAT_HEALTH] > 0)
					{
						self->health += dmg;
						if (self->health > self->client->ps.stats[STAT_MAX_HEALTH])
						{
							self->health = self->client->ps.stats[STAT_MAX_HEALTH];
						}
						self->client->ps.stats[STAT_HEALTH] = self->health;
					}
				}
				else if (g_drainRework.integer == 1) {
					self->health += actualForceDrainedFromTarget;
					if (self->health > 125)
						self->health = 125;
					self->client->ps.stats[STAT_HEALTH] = self->health;
				}
				else {
					int restoredForce;
#if 0
					if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] > FORCE_LEVEL_2)
						restoredForce = DRAIN_REWORK2_FORCEGIVEN_LEVEL3;
					else if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] == FORCE_LEVEL_2)
						restoredForce = DRAIN_REWORK2_FORCEGIVEN_LEVEL2;
					else
						restoredForce = DRAIN_REWORK2_FORCEGIVEN_LEVEL1;
#else
					restoredForce = (int)roundf(((float)actualForceDrainedFromTarget) * 0.6f);
#endif
					self->client->ps.fd.forcePower += restoredForce;
					if (self->client->ps.fd.forcePower > 100)
						self->client->ps.fd.forcePower = 100;
				}

				const int reworkDebuffDuration = DRAIN_REWORK_DEBUFFDURATION_CONSTANT + (DRAIN_REWORK_DEBUFFDURATION_PERLEVEL * self->client->ps.fd.forcePowerLevel[FP_DRAIN]);
				if (!g_drainRework.integer) {
					traceEnt->client->ps.fd.forcePowerRegenDebounceTime = level.time + 800; //don't let the client being drained get force power back right away
				}
				else {
					if (dmg) {
						traceEnt->client->ps.fd.forcePowerRegenDebounceTime = level.time + reworkDebuffDuration;
						traceEnt->client->drainDebuffTime = level.time + reworkDebuffDuration;
					}
				}

				if (traceEnt->client->forcePowerSoundDebounce < level.time && !(g_drainRework.integer && !dmg))
				{
					tent = G_TempEntity( impactPoint, EV_FORCE_DRAINED);
					tent->s.eventParm = DirToByte(dir);
					tent->s.owner = traceEnt->s.number;
					if (g_drainRework.integer)
						tent->s.time = reworkDebuffDuration;
					G_ApplyRaceBroadcastsToEvent( traceEnt, tent );

					traceEnt->client->forcePowerSoundDebounce = level.time + 400;
				}
			}
		}
	}

	if (absorbed)
		return -1;

	return actualForceDrainedFromTarget;
}

int ForceShootDrain( gentity_t *self )
{
	trace_t	tr;
	vec3_t	end, forward;
	gentity_t	*traceEnt;
	int			gotOneOrMore = 0;

	if ( self->health <= 0 )
	{
		return 0;
	}
	AngleVectors( self->client->ps.viewangles, forward, NULL, NULL );
	VectorNormalize( forward );

	vec3_t start;
	VectorCopy(self->client->ps.origin, start);
	if (g_drainRework.integer)
		start[2] += self->client->ps.viewheight;

	int actualAmountDrained = 0;

	if ( self->client->ps.fd.forcePowerLevel[FP_DRAIN] > FORCE_LEVEL_2 && !g_drainRework.integer )
	{//arc
		qboolean compensate = self->client->sess.unlagged;
		if (g_unlagged.integer && compensate)
			G_TimeShiftAllClients(trap_Milliseconds() - (level.time - self->client->pers.cmd.serverTime), self, qfalse);
		vec3_t	center, mins, maxs, dir, ent_org, size, v;
		float	radius = MAX_DRAIN_DISTANCE, dot, dist;
		gentity_t	*entityList[MAX_GENTITIES];
		int			iEntityList[MAX_GENTITIES];
		int		e, numListedEntities, i;

		VectorCopy( self->client->ps.origin, center );
		for ( i = 0 ; i < 3 ; i++ ) 
		{
			mins[i] = center[i] - radius;
			maxs[i] = center[i] + radius;
		}
		numListedEntities = trap_EntitiesInBox( mins, maxs, iEntityList, MAX_GENTITIES );

		i = 0;
		while (i < numListedEntities)
		{
			entityList[i] = &g_entities[iEntityList[i]];

			i++;
		}

		for ( e = 0 ; e < numListedEntities ; e++ ) 
		{
			traceEnt = entityList[e];

			if ( !traceEnt )
				continue;
			if ( traceEnt == self )
				continue;
			if ( !traceEnt->inuse )
				continue;
			if ( !traceEnt->takedamage )
				continue;
			if ( traceEnt->health <= 0 )//no torturing corpses
				continue;
			if ( !traceEnt->client )
				continue;
			if ( !traceEnt->client->ps.fd.forcePower )
				continue;
			if (OnSameTeam(self, traceEnt) && !g_friendlyFire.integer)
				continue;
			if (traceEnt->isAimPracticePack)
				continue;
			if ( traceEnt->client && traceEnt->client->sess.inRacemode )
				continue; // don't use force powers on racers
			//this is all to see if we need to start a saber attack, if it's in flight, this doesn't matter
			// find the distance from the edge of the bounding box
			for ( i = 0 ; i < 3 ; i++ ) 
			{
				if ( center[i] < traceEnt->r.absmin[i] ) 
				{
					v[i] = traceEnt->r.absmin[i] - center[i];
				} else if ( center[i] > traceEnt->r.absmax[i] ) 
				{
					v[i] = center[i] - traceEnt->r.absmax[i];
				} else 
				{
					v[i] = 0;
				}
			}

			VectorSubtract( traceEnt->r.absmax, traceEnt->r.absmin, size );
			VectorMA( traceEnt->r.absmin, 0.5, size, ent_org );

			//see if they're in front of me
			//must be within the forward cone
			VectorSubtract( ent_org, center, dir );
			VectorNormalize( dir );
			if ( (dot = DotProduct( dir, forward )) < 0.5 )
				continue;

			//must be close enough
			dist = VectorLength( v );
			if ( dist >= radius ) 
			{
				continue;
			}
		
			//in PVS?
			if ( !traceEnt->r.bmodel && !trap_InPVS( ent_org, start) )
			{//must be in PVS
				continue;
			}

			//Now check and see if we can actually hit it
			trap_Trace( &tr, start, vec3_origin, vec3_origin, ent_org, self->s.number, MASK_SHOT );
			if ( tr.fraction < 1.0f && tr.entityNum != traceEnt->s.number )
			{//must have clear LOS
				continue;
			}

			// ok, we are within the radius, add us to the incoming list
			ForceDrainDamage( self, traceEnt, dir, ent_org );
			gotOneOrMore = 1;
		}
		if (g_unlagged.integer && compensate)
			G_UnTimeShiftAllClients(self, qfalse);
	}
	else
	{//trace-line
		int range = !g_drainRework.integer ? 2048 : 512;
		VectorMA(start, range, forward, end );

		vec3_t mins, maxs;
		if (!g_drainRework.integer) {
			VectorCopy(vec3_origin, mins);
			VectorCopy(vec3_origin, maxs);
		}
		else { // slightly bigger trace bounds i guess
			mins[0] = mins[1] = mins[2] = -1 * 3;
			maxs[0] = maxs[1] = maxs[2] = 3;
		}

		qboolean compensate = self->client->sess.unlagged;
		if (g_unlagged.integer && compensate)
			G_TimeShiftAllClients(trap_Milliseconds() - (level.time - self->client->pers.cmd.serverTime), self, qfalse);

		trap_Trace( &tr, start, mins, maxs, end, self->s.number, MASK_SHOT );

		if (g_unlagged.integer && compensate)
			G_UnTimeShiftAllClients(self, qfalse);

		if ( tr.entityNum == ENTITYNUM_NONE || tr.fraction == 1.0 || tr.allsolid || tr.startsolid || !g_entities[tr.entityNum].client || !g_entities[tr.entityNum].inuse )
		{
			if (!g_drainRework.integer)
				return 0;
			else
				self->s.userInt1 = self->client->ps.userInt1 = self->s.userInt2 = self->client->ps.userInt2 = 0;
		}
		else {
			traceEnt = &g_entities[tr.entityNum];
			actualAmountDrained = ForceDrainDamage(self, traceEnt, forward, tr.endpos);
			gotOneOrMore = 1;

			if (g_drainRework.integer) {
				self->s.userInt1 = self->client->ps.userInt1 = 1; // hit
				if (actualAmountDrained == -1 && traceEnt && traceEnt->client && (traceEnt->client->ps.fd.forcePowersActive & (1 << FP_ABSORB)))
					self->s.userInt2 = self->client->ps.userInt2 = 1; // absorbed
				else
					self->s.userInt2 = self->client->ps.userInt2 = 0; // not absorbed
			}
		}
	}

	self->client->ps.activeForcePass = self->client->ps.fd.forcePowerLevel[FP_DRAIN] + FORCE_LEVEL_3;

	if (g_drainRework.integer < 2) {
		if (!g_drainRework.integer)
			BG_ForcePowerDrain(&self->client->ps, FP_DRAIN, 5); //used to be 1, but this did, too, anger the God of Balance.
		else {
			int forceCost;
			switch (self->client->ps.fd.forcePowerLevel[FP_DRAIN]) {
			case 3: forceCost = DRAIN_REWORK1_FORCECOST_LEVEL3; break;
			case 2: forceCost = DRAIN_REWORK1_FORCECOST_LEVEL2; break;
			default: forceCost = DRAIN_REWORK1_FORCECOST_LEVEL1; break;
			}
			BG_ForcePowerDrain(&self->client->ps, FP_DRAIN, forceCost);
		}
	}
	else {
		int healthCost, maxFpDrained, selfdmg;
		switch (self->client->ps.fd.forcePowerLevel[FP_DRAIN]) {
		case 3:
			healthCost = DRAIN_REWORK2_SELFDMG_LEVEL3;
			maxFpDrained = DRAIN_REWORK_FORCEREDUCE_LEVEL3;
			break;
		case 2:
			healthCost = DRAIN_REWORK2_SELFDMG_LEVEL2;
			maxFpDrained = DRAIN_REWORK_FORCEREDUCE_LEVEL2;
			break;
		default:
			healthCost = DRAIN_REWORK2_SELFDMG_LEVEL1;
			maxFpDrained = DRAIN_REWORK_FORCEREDUCE_LEVEL1;
			break;
		}
		assert(maxFpDrained != 0);
		if (actualAmountDrained >= 0)
			selfdmg = (int)round(healthCost * ((double)actualAmountDrained / (double)maxFpDrained));
		else
			selfdmg = healthCost;

		if (selfdmg > 0)
			G_Damage(self, self, self, NULL, NULL, selfdmg, DAMAGE_NO_PROTECTION | DAMAGE_NO_ARMOR | DAMAGE_NO_SELF_PROTECTION, MOD_SUICIDE);
	}

	if (!g_drainRework.integer)
		self->client->ps.fd.forcePowerRegenDebounceTime = level.time + 500;

	return gotOneOrMore;
}

void ForceJumpCharge( gentity_t *self, usercmd_t *ucmd )
{ //I guess this is unused now. Was used for the "charge" jump type.
	float forceJumpChargeInterval = forceJumpStrength[0] / (FORCE_JUMP_CHARGE_TIME/FRAMETIME);

	if ( self->health <= 0 )
	{
		return;
	}

	if (!self->client->ps.fd.forceJumpCharge && self->client->ps.groundEntityNum == ENTITYNUM_NONE)
	{
		return;
	}

	if (self->client->ps.fd.forcePower < forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]][FP_LEVITATION])
	{
		G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);
		return;
	}

	if (!self->client->ps.fd.forceJumpCharge)
	{
		self->client->ps.fd.forceJumpAddTime = 0;
	}

	if (self->client->ps.fd.forceJumpAddTime >= level.time)
	{
		return;
	}

	//need to play sound
	if ( !self->client->ps.fd.forceJumpCharge )
	{
		G_Sound( self, TRACK_CHANNEL_1, G_SoundIndex("sound/weapons/force/jumpbuild.wav") );
	}

	//Increment
	if (self->client->ps.fd.forceJumpAddTime < level.time)
	{
		self->client->ps.fd.forceJumpCharge += forceJumpChargeInterval*50;
		self->client->ps.fd.forceJumpAddTime = level.time + 500;
	}

	//clamp to max strength for current level
	if ( self->client->ps.fd.forceJumpCharge > forceJumpStrength[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]] )
	{
		self->client->ps.fd.forceJumpCharge = forceJumpStrength[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]];
		G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);
	}

	//clamp to max available force power
	if ( self->client->ps.fd.forceJumpCharge/forceJumpChargeInterval/(FORCE_JUMP_CHARGE_TIME/FRAMETIME)*forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]][FP_LEVITATION] > self->client->ps.fd.forcePower )
	{//can't use more than you have
		G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);
		self->client->ps.fd.forceJumpCharge = self->client->ps.fd.forcePower*forceJumpChargeInterval/(FORCE_JUMP_CHARGE_TIME/FRAMETIME);
	}
	
}

int WP_GetVelocityForForceJump( gentity_t *self, vec3_t jumpVel, usercmd_t *ucmd )
{
	float pushFwd = 0, pushRt = 0;
	vec3_t	view, forward, right;
	VectorCopy( self->client->ps.viewangles, view );
	view[0] = 0;
	AngleVectors( view, forward, right, NULL );
	if ( ucmd->forwardmove && ucmd->rightmove )
	{
		if ( ucmd->forwardmove > 0 )
		{
			pushFwd = 50;
		}
		else
		{
			pushFwd = -50;
		}
		if ( ucmd->rightmove > 0 )
		{
			pushRt = 50;
		}
		else
		{
			pushRt = -50;
		}
	}
	else if ( ucmd->forwardmove || ucmd->rightmove )
	{
		if ( ucmd->forwardmove > 0 )
		{
			pushFwd = 100;
		}
		else if ( ucmd->forwardmove < 0 )
		{
			pushFwd = -100;
		}
		else if ( ucmd->rightmove > 0 )
		{
			pushRt = 100;
		}
		else if ( ucmd->rightmove < 0 )
		{
			pushRt = -100;
		}
	}

	G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);

	G_PreDefSound(self, PDSOUND_FORCEJUMP);

	if (self->client->ps.fd.forceJumpCharge < JUMP_VELOCITY+40)
	{ //give him at least a tiny boost from just a tap
		self->client->ps.fd.forceJumpCharge = JUMP_VELOCITY+400;
	}

	if (self->client->ps.velocity[2] < -30)
	{ //so that we can get a good boost when force jumping in a fall
		self->client->ps.velocity[2] = -30;
	}

	VectorMA( self->client->ps.velocity, pushFwd, forward, jumpVel );
	VectorMA( self->client->ps.velocity, pushRt, right, jumpVel );
	jumpVel[2] += self->client->ps.fd.forceJumpCharge;
	if ( pushFwd > 0 && self->client->ps.fd.forceJumpCharge > 200 )
	{
		return FJ_FORWARD;
	}
	else if ( pushFwd < 0 && self->client->ps.fd.forceJumpCharge > 200 )
	{
		return FJ_BACKWARD;
	}
	else if ( pushRt > 0 && self->client->ps.fd.forceJumpCharge > 200 )
	{
		return FJ_RIGHT;
	}
	else if ( pushRt < 0 && self->client->ps.fd.forceJumpCharge > 200 )
	{
		return FJ_LEFT;
	}
	else
	{
		return FJ_UP;
	}
}

void ForceJump( gentity_t *self, usercmd_t *ucmd )
{
	if (self->isAimPracticePack)
		return;
	float forceJumpChargeInterval;
	vec3_t	jumpVel;

	if ( self->client->ps.fd.forcePowerDuration[FP_LEVITATION] > level.time )
	{
		return;
	}
	if ( !WP_ForcePowerUsable( self, FP_LEVITATION ) )
	{
		return;
	}
	if ( self->s.groundEntityNum == ENTITYNUM_NONE )
	{
		return;
	}
	if ( self->health <= 0 )
	{
		return;
	}

	self->client->fjDidJump = qtrue;

	forceJumpChargeInterval = forceJumpStrength[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]]/(FORCE_JUMP_CHARGE_TIME/FRAMETIME);

	WP_GetVelocityForForceJump( self, jumpVel, ucmd );

	//FIXME: sound effect
	self->client->ps.fd.forceJumpZStart = self->client->ps.origin[2];//remember this for when we land
	VectorCopy( jumpVel, self->client->ps.velocity );
	//wasn't allowing them to attack when jumping, but that was annoying

	WP_ForcePowerStart( self, FP_LEVITATION, self->client->ps.fd.forceJumpCharge/forceJumpChargeInterval/(FORCE_JUMP_CHARGE_TIME/FRAMETIME)*forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_LEVITATION]][FP_LEVITATION] );
	self->client->ps.fd.forceJumpCharge = 0;
	self->client->ps.forceJumpFlip = qtrue;
}

void WP_AddAsMindtricked(forcedata_t *fd, int entNum)
{
	if (!fd)
	{
		return;
	}

	if (entNum > 47)
	{
		fd->forceMindtrickTargetIndex4 |= (1 << (entNum-48));
	}
	else if (entNum > 31)
	{
		fd->forceMindtrickTargetIndex3 |= (1 << (entNum-32));
	}
	else if (entNum > 15)
	{
		fd->forceMindtrickTargetIndex2 |= (1 << (entNum-16));
	}
	else
	{
		fd->forceMindtrickTargetIndex |= (1 << entNum);
	}
}

qboolean ForceTelepathyCheckDirectNPCTarget( gentity_t *self, trace_t *tr, qboolean *tookPower )
{
	gentity_t	*traceEnt;
	qboolean	targetLive = qfalse;
	vec3_t		tfrom, tto, fwd;
	float		radius = MAX_TRICK_DISTANCE;

	//Check for a direct usage on NPCs first
	VectorCopy(self->client->ps.origin, tfrom);
	tfrom[2] += self->client->ps.viewheight;
	AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
	tto[0] = tfrom[0] + fwd[0]*radius/2;
	tto[1] = tfrom[1] + fwd[1]*radius/2;
	tto[2] = tfrom[2] + fwd[2]*radius/2;

	trap_Trace( tr, tfrom, NULL, NULL, tto, self->s.number, MASK_PLAYERSOLID );
	
	if ( tr->entityNum == ENTITYNUM_NONE 
		|| tr->entityNum == ENTITYNUM_WORLD
		|| tr->fraction == 1.0f
		|| tr->allsolid 
		|| tr->startsolid )
	{
		return qfalse;
	}
	
	traceEnt = &g_entities[tr->entityNum];
	
	if( traceEnt->NPC 
		&& traceEnt->NPC->scriptFlags & SCF_NO_FORCE )
	{
		return qfalse;
	}

	if ( traceEnt && traceEnt->client  )
	{
		switch ( traceEnt->client->NPC_class )
		{
		case CLASS_GALAKMECH://cant grip him, he's in armor
		case CLASS_ATST://much too big to grip!
		//no droids either
		case CLASS_PROBE:
		case CLASS_GONK:
		case CLASS_R2D2:
		case CLASS_R5D2:
		case CLASS_MARK1:
		case CLASS_MARK2:
		case CLASS_MOUSE:
		case CLASS_SEEKER:
		case CLASS_REMOTE:
		case CLASS_PROTOCOL:
		case CLASS_BOBAFETT:
		case CLASS_RANCOR:
			break;
		default:
			targetLive = qtrue;
			break;
		}
	}

	if ( traceEnt->s.number < MAX_CLIENTS )
	{//a regular client
		return qfalse;
	}

	if ( targetLive && traceEnt->NPC )
	{//hit an organic non-player
		vec3_t	eyeDir;
		if ( G_ActivateBehavior( traceEnt, BSET_MINDTRICK ) )
		{//activated a script on him
			//FIXME: do the visual sparkles effect on their heads, still?
			WP_ForcePowerStart( self, FP_TELEPATHY, 0 );
		}
		else if ( (self->NPC && traceEnt->client->playerTeam != self->client->playerTeam)
			|| (!self->NPC && traceEnt->client->playerTeam != self->client->sess.sessionTeam) )
		{//an enemy
			int override = 0;
			if ( (traceEnt->NPC->scriptFlags&SCF_NO_MIND_TRICK) )
			{
			}
			else if ( traceEnt->s.weapon != WP_SABER 
				&& traceEnt->client->NPC_class != CLASS_REBORN )
			{//haha!  Jedi aren't easily confused!
				if ( self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] > FORCE_LEVEL_2 )
				{//turn them to our side
					//if mind trick 3 and aiming at an enemy need more force power
					if ( traceEnt->s.weapon != WP_NONE )
					{//don't charm people who aren't capable of fighting... like ugnaughts and droids
						int newPlayerTeam, newEnemyTeam;

						if ( traceEnt->enemy )
						{
							G_ClearEnemy( traceEnt );
						}
						if ( traceEnt->NPC )
						{
							traceEnt->client->leader = self;
						}
						//FIXME: maybe pick an enemy right here?
						if ( self->NPC )
						{//NPC
							newPlayerTeam = self->client->playerTeam;
							newEnemyTeam = self->client->enemyTeam;
						}
						else
						{//client/bot
							if ( self->client->sess.sessionTeam == TEAM_BLUE )
							{//rebel
								newPlayerTeam = NPCTEAM_PLAYER;
								newEnemyTeam = NPCTEAM_ENEMY;
							}
							else if ( self->client->sess.sessionTeam == TEAM_RED )
							{//imperial
								newPlayerTeam = NPCTEAM_ENEMY;
								newEnemyTeam = NPCTEAM_PLAYER;
							}
							else
							{//neutral - wan't attack anyone
								newPlayerTeam = NPCTEAM_NEUTRAL;
								newEnemyTeam = NPCTEAM_NEUTRAL;
							}
						}
						//store these for retrieval later
						traceEnt->genericValue1 = traceEnt->client->playerTeam;
						traceEnt->genericValue2 = traceEnt->client->enemyTeam;
						traceEnt->genericValue3 = traceEnt->s.teamowner;
						//set the new values
						traceEnt->client->playerTeam = newPlayerTeam;
						traceEnt->client->enemyTeam = newEnemyTeam;
						traceEnt->s.teamowner = newPlayerTeam;
						//FIXME: need a *charmed* timer on this...?  Or do TEAM_PLAYERS assume that "confusion" means they should switch to team_enemy when done?
						traceEnt->NPC->charmedTime = level.time + mindTrickTime[self->client->ps.fd.forcePowerLevel[FP_TELEPATHY]];
					}
				}
				else
				{//just confuse them
					//somehow confuse them?  Set don't fire to true for a while?  Drop their aggression?  Maybe just take their enemy away and don't let them pick one up for a while unless shot?
					traceEnt->NPC->confusionTime = level.time + mindTrickTime[self->client->ps.fd.forcePowerLevel[FP_TELEPATHY]];//confused for about 10 seconds
					NPC_PlayConfusionSound( traceEnt );
					if ( traceEnt->enemy )
					{
						G_ClearEnemy( traceEnt );
					}
				}
			}
			else
			{
				NPC_Jedi_PlayConfusionSound( traceEnt );
			}
			WP_ForcePowerStart( self, FP_TELEPATHY, override );
		}
		else if ( traceEnt->client->playerTeam == self->client->playerTeam )
		{//an ally
			//maybe just have him look at you?  Respond?  Take your enemy?
			if ( traceEnt->client->ps.pm_type < PM_DEAD && traceEnt->NPC!=NULL && !(traceEnt->NPC->scriptFlags&SCF_NO_RESPONSE) )
			{
				NPC_UseResponse( traceEnt, self, qfalse );
				WP_ForcePowerStart( self, FP_TELEPATHY, 1 );
			}
		}//NOTE: no effect on TEAM_NEUTRAL?
		AngleVectors( traceEnt->client->renderInfo.eyeAngles, eyeDir, NULL, NULL );
		VectorNormalize( eyeDir );
		gentity_t *te = G_PlayEffectID( G_EffectIndex( "force/force_touch" ), traceEnt->client->renderInfo.eyePoint, eyeDir );
		G_ApplyRaceBroadcastsToEvent( traceEnt, te );

		//make sure this plays and that you cannot press fire for about 1 second after this
		//FIXME: BOTH_FORCEMINDTRICK or BOTH_FORCEDISTRACT
		//FIXME: build-up or delay this until in proper part of anim
	}
	else 
	{
		if ( self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] > FORCE_LEVEL_1 && tr->fraction * 2048 > 64 )
		{//don't create a diversion less than 64 from you of if at power level 1
			//use distraction anim instead
			gentity_t *te = G_PlayEffectID( G_EffectIndex( "force/force_touch" ), tr->endpos, tr->plane.normal );
			G_ApplyRaceBroadcastsToEvent( traceEnt, te );
			//FIXME: these events don't seem to always be picked up...?
			AddSoundEvent( self, tr->endpos, 512, AEL_SUSPICIOUS, qtrue );
			AddSightEvent( self, tr->endpos, 512, AEL_SUSPICIOUS, 50 );
			WP_ForcePowerStart( self, FP_TELEPATHY, 0 );
			*tookPower = qtrue;
		}
	}
	self->client->ps.saberBlocked = BLOCKED_NONE;
	self->client->ps.weaponTime = 1000;

	return qtrue;
}

void ForceTelepathy(gentity_t *self)
{
	if (self->isAimPracticePack)
		return;
	trace_t tr;
	vec3_t tto, thispush_org, a;
	vec3_t mins, maxs, fwdangles, forward, right, center;
	int i;
	float visionArc = 0;
	float radius = MAX_TRICK_DISTANCE;
	qboolean	tookPower = qfalse;

	if ( self->health <= 0 )
	{
		return;
	}

	if (self->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (self->client->ps.weaponTime > 0)
	{
		return;
	}

	if (self->client->ps.powerups[PW_REDFLAG] ||
		self->client->ps.powerups[PW_BLUEFLAG])
	{ //can't mindtrick while carrying the flag
		return;
	}

	if (self->client->ps.forceAllowDeactivateTime < level.time &&
		(self->client->ps.fd.forcePowersActive & (1 << FP_TELEPATHY)) )
	{
		WP_ForcePowerStop( self, FP_TELEPATHY );
		return;
	}

	if ( !WP_ForcePowerUsable( self, FP_TELEPATHY ) )
	{
		return;
	}

	// *CHANGE 65* fix - release rocket lock, old bug
	BG_ClearRocketLock(&self->client->ps);

		qboolean compensate = self->client->sess.unlagged;
		if (g_unlagged.integer && compensate && self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_1)
			G_TimeShiftAllClients(trap_Milliseconds() - (level.time - self->client->pers.cmd.serverTime), self, qfalse);

		qboolean hitNpcDirectly = ForceTelepathyCheckDirectNPCTarget(self, &tr, &tookPower);

		if (g_unlagged.integer && compensate && self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_1)
			G_UnTimeShiftAllClients(self, qfalse);

	if ( hitNpcDirectly )
	{//hit an NPC directly
		self->client->ps.forceAllowDeactivateTime = level.time + 1500;
		G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/force/distract.wav") );
		self->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
		self->client->ps.forceHandExtendTime = level.time + 1000;
		return;
	}

	if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_2)
	{
		visionArc = 180;
	}
	else if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_3)
	{
		visionArc = 360;
		radius = MAX_TRICK_DISTANCE*2.0f;
	}

	VectorCopy( self->client->ps.viewangles, fwdangles );
	AngleVectors( fwdangles, forward, right, NULL );
	VectorCopy( self->client->ps.origin, center );

	for ( i = 0 ; i < 3 ; i++ ) 
	{
		mins[i] = center[i] - radius;
		maxs[i] = center[i] + radius;
	}

	if (self->client->ps.fd.forcePowerLevel[FP_TELEPATHY] == FORCE_LEVEL_1)
	{
		if (tr.fraction != 1.0 &&
			tr.entityNum != ENTITYNUM_NONE &&
			g_entities[tr.entityNum].inuse &&
			g_entities[tr.entityNum].client &&
			g_entities[tr.entityNum].client->pers.connected &&
			g_entities[tr.entityNum].client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			WP_AddAsMindtricked(&self->client->ps.fd, tr.entityNum);
			if ( !tookPower )
			{
				WP_ForcePowerStart( self, FP_TELEPATHY, 0 );
			}

			G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/force/distract.wav") );

			self->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
			self->client->ps.forceHandExtendTime = level.time + 1000;

			return;
		}
		else
		{
			return;
		}
	}
	else	//level 2 & 3
	{
		gentity_t *ent;
		int entityList[MAX_GENTITIES];
		int numListedEntities;
		int e = 0;
		qboolean gotatleastone = qfalse;

		numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

		while (e < numListedEntities)
		{
			ent = &g_entities[entityList[e]];

			if (ent)
			{ //not in the arc, don't consider it
				if (ent->client)
				{
					VectorCopy(ent->client->ps.origin, thispush_org);
				}
				else
				{
					VectorCopy(ent->s.pos.trBase, thispush_org);
				}
				VectorCopy(self->client->ps.origin, tto);
				tto[2] += self->client->ps.viewheight;
				VectorSubtract(thispush_org, tto, a);
				vectoangles(a, a);

				if (!ent->client)
				{
					entityList[e] = ENTITYNUM_NONE;
				}
				else if (ent->isAimPracticePack) {
					entityList[e] = ENTITYNUM_NONE;
				}
				else if (ent->client->sess.inRacemode)
				{ //no force powers on racers
					entityList[e] = ENTITYNUM_NONE;
				}
				else if (!InFieldOfVision(self->client->ps.viewangles, visionArc, a))
				{ //only bother with arc rules if the victim is a client
					entityList[e] = ENTITYNUM_NONE;
				}
				else if (ForcePowerUsableOn(self, ent, FP_TELEPATHY) <= 0)
				{
					entityList[e] = ENTITYNUM_NONE;
				}
				else if (OnSameTeam(self, ent))
				{
					entityList[e] = ENTITYNUM_NONE;
				}
			}
			ent = &g_entities[entityList[e]];
			if (ent && ent != self && ent->client)
			{
				gotatleastone = qtrue;
				WP_AddAsMindtricked(&self->client->ps.fd, ent->s.number);
			}
			e++;
		}

		if (gotatleastone)
		{
			self->client->ps.forceAllowDeactivateTime = level.time + 1500;

			if ( !tookPower )
			{
				WP_ForcePowerStart( self, FP_TELEPATHY, 0 );
			}

			G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/force/distract.wav") );

			self->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
			self->client->ps.forceHandExtendTime = level.time + 1000;
		}
	}

}

void GEntity_UseFunc( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	GlobalUse(self, other, activator);
}

qboolean CanCounterThrow(gentity_t *self, gentity_t *thrower, qboolean pull)
{
	int powerUse = 0;

	if (self->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return 0;
	}

	if (self->client->ps.weaponTime > 0)
	{
		return 0;
	}

	if ( self->health <= 0 )
	{
		return 0;
	}

	if ( self->client->ps.powerups[PW_DISINT_4] > level.time )
	{
		return 0;
	}

	if (self->client->ps.weaponstate == WEAPON_CHARGING ||
		self->client->ps.weaponstate == WEAPON_CHARGING_ALT)
	{ //don't autodefend when charging a weapon
		return 0;
	}

	if ((self->client->ps.fd.forcePowersActive & (1 << FP_RAGE)) && !g_ragersCanCounterPushPull.integer) {
		return 0;
	}

	if (g_gametype.integer == GT_SIEGE &&
		pull &&
		thrower && thrower->client)
	{ //in siege, pull will affect people if they are not facing you, so they can't run away so much
		vec3_t d;
		float a;

        VectorSubtract(thrower->client->ps.origin, self->client->ps.origin, d);
		vectoangles(d, d);

        a = AngleSubtract(d[YAW], self->client->ps.viewangles[YAW]);

		if (a > 60.0f || a < -60.0f)
		{ //if facing more than 60 degrees away they cannot defend
			return 0;
		}
	}

	if (pull)
	{
		powerUse = FP_PULL;
	}
	else
	{
		powerUse = FP_PUSH;
	}

	if ( !WP_ForcePowerUsable( self, powerUse ) )
	{
		return 0;
	}

	if (self->client->ps.groundEntityNum == ENTITYNUM_NONE)
	{ //you cannot counter a push/pull if you're in the air
		return 0;
	}

	return 1;
}

qboolean G_InGetUpAnim(playerState_t *ps)
{
	switch( (ps->legsAnim) )
	{
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		return qtrue;
	}

	switch( (ps->torsoAnim) )
	{
	case BOTH_GETUP1:
	case BOTH_GETUP2:
	case BOTH_GETUP3:
	case BOTH_GETUP4:
	case BOTH_GETUP5:
	case BOTH_FORCE_GETUP_F1:
	case BOTH_FORCE_GETUP_F2:
	case BOTH_FORCE_GETUP_B1:
	case BOTH_FORCE_GETUP_B2:
	case BOTH_FORCE_GETUP_B3:
	case BOTH_FORCE_GETUP_B4:
	case BOTH_FORCE_GETUP_B5:
	case BOTH_GETUP_BROLL_B:
	case BOTH_GETUP_BROLL_F:
	case BOTH_GETUP_BROLL_L:
	case BOTH_GETUP_BROLL_R:
	case BOTH_GETUP_FROLL_B:
	case BOTH_GETUP_FROLL_F:
	case BOTH_GETUP_FROLL_L:
	case BOTH_GETUP_FROLL_R:
		return qtrue;
	}

	return qfalse;
}

void G_LetGoOfWall( gentity_t *ent )
{
	if ( !ent || !ent->client )
	{
		return;
	}
	ent->client->ps.pm_flags &= ~PMF_STUCK_TO_WALL;
	if ( BG_InReboundJump( ent->client->ps.legsAnim ) 
		|| BG_InReboundHold( ent->client->ps.legsAnim ) )
	{
		ent->client->ps.legsTimer = 0;
	}
	if ( BG_InReboundJump( ent->client->ps.torsoAnim ) 
		|| BG_InReboundHold( ent->client->ps.torsoAnim ) )
	{
		ent->client->ps.torsoTimer = 0;
	}
}

float forcePushPullRadius[NUM_FORCE_POWER_LEVELS] =
{
	0,//none
	384,//256,
	448,//384,
	512
};
//rwwFIXMEFIXME: incorporate this into the below function? Currently it's only being used by jedi AI

static qboolean AdditionalPushPullLOSChecks(gentity_t *pusher, gentity_t *target) {
	assert(pusher && pusher->client && target && target->client);

	trace_t tr;
	vec3_t pusherUpperChestPoint, pusherMidPoint, targetUpperChestPoint, targetMidPoint, targetOrigin;

	const float zDifference = fabs(pusher->client->ps.origin[2] - target->client->ps.origin[2]);
	if (zDifference > 64) {
		if (d_debugPushPullLOS.integer)
			PrintIngame(-1, "[%d] d_debugPushPullLOS: z-axis difference for %d to %d is too great (%g)\n", level.time - level.startTime, pusher - g_entities, target - g_entities, zDifference);
		return qfalse;
	}

	VectorAdd(pusher->r.absmin, pusher->r.absmax, pusherUpperChestPoint);
	VectorScale(pusherUpperChestPoint, 0.6667, pusherUpperChestPoint);

	VectorAdd(pusher->r.absmin, pusher->r.absmax, pusherMidPoint);
	VectorScale(pusherMidPoint, 0.5, pusherMidPoint);

	VectorAdd(target->r.absmin, target->r.absmax, targetUpperChestPoint);
	VectorScale(targetUpperChestPoint, 0.6667, targetUpperChestPoint);

	VectorAdd(target->r.absmin, target->r.absmax, targetMidPoint);
	VectorScale(targetMidPoint, 0.5, targetMidPoint);

	VectorCopy(target->r.currentOrigin, targetOrigin);

	// pusher's upper chest to target's upper chest
	trap_Trace(&tr, pusherUpperChestPoint, vec3_origin, vec3_origin, targetUpperChestPoint, pusher->s.number, MASK_SHOT);
	if (tr.entityNum == target->s.number) {
		if (d_debugPushPullLOS.integer)
			PrintIngame(-1, "[%d] d_debugPushPullLOS: got LOS from %d to %d with ^3upper chest to upper chest^7 check\n", level.time - level.startTime, pusher - g_entities, target - g_entities);
		return qtrue;
	}

	// pusher's upper chest to target's origin
	trap_Trace(&tr, pusherUpperChestPoint, vec3_origin, vec3_origin, targetOrigin, pusher->s.number, MASK_SHOT);
	if (tr.entityNum == target->s.number) {
		if (d_debugPushPullLOS.integer)
			PrintIngame(-1, "[%d] d_debugPushPullLOS: got LOS from %d to %d with ^3upper chest to origin^7 check\n", level.time - level.startTime, pusher - g_entities, target - g_entities);
		return qtrue;
	}

	// pusher's midpoint to target's upper chest
	trap_Trace(&tr, pusherMidPoint, vec3_origin, vec3_origin, targetUpperChestPoint, pusher->s.number, MASK_SHOT);
	if (tr.entityNum == target->s.number) {
		if (d_debugPushPullLOS.integer)
			PrintIngame(-1, "[%d] d_debugPushPullLOS: got LOS from %d to %d with ^3midpoint to upper chest^7 check\n", level.time - level.startTime, pusher - g_entities, target - g_entities);
		return qtrue;
	}

	// pusher's midpoint to target's midpoint
	trap_Trace(&tr, pusherMidPoint, vec3_origin, vec3_origin, targetMidPoint, pusher->s.number, MASK_SHOT);
	if (tr.entityNum == target->s.number) {
		if (d_debugPushPullLOS.integer)
			PrintIngame(-1, "[%d] d_debugPushPullLOS: got LOS from %d to %d with ^3midpoint to midpoint^7 check\n", level.time - level.startTime, pusher - g_entities, target - g_entities);
		return qtrue;
	}

	// pusher's midpoint to target's origin
	trap_Trace(&tr, pusherMidPoint, vec3_origin, vec3_origin, targetOrigin, pusher->s.number, MASK_SHOT);
	if (tr.entityNum == target->s.number) {
		if (d_debugPushPullLOS.integer)
			PrintIngame(-1, "[%d] d_debugPushPullLOS: got LOS from %d to %d with ^3midpoint to origin^7 check\n", level.time - level.startTime, pusher - g_entities, target - g_entities);
		return qtrue;
	}

	return qfalse;
}

extern void Touch_Button(gentity_t *ent, gentity_t *other, trace_t *trace );
void ForceThrow( gentity_t *self, qboolean pull )
{
	if (self->isAimPracticePack)
		return;
	//shove things in front of you away
	float		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	gentity_t	*push_list[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	int			i, e;
	int			ent_count = 0;
	int			radius = 1024; //since it's view-based now. //350;
	int			powerLevel;
	int			visionArc;
	int			pushPower;
	int			pushPowerMod;
	vec3_t		center, ent_org, size, forward, right, end, dir, fwdangles = {0};
	float		dot1;
	trace_t		tr;
	int			x;
	vec3_t		pushDir;
	vec3_t		thispush_org;
	vec3_t		tfrom, tto, fwd, a;
	int			powerUse = 0;

	visionArc = 0;

	qboolean meme = (!level.wasRestarted && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
	if (!meme && (self->client->ps.forceHandExtend != HANDEXTEND_NONE && (self->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN || !G_InGetUpAnim(&self->client->ps))))
	{
		return;
	}

	if (!meme && !g_useWhileThrowing.integer && self->client->ps.saberInFlight)
	{
		return;
	}

	if (!meme && self->client->ps.weaponTime > 0)
	{
		return;
	}

	if (!meme && self->health <= 0 )
	{
		return;
	}
	if (!meme && self->client->ps.powerups[PW_DISINT_4] > level.time )
	{
		return;
	}
	if (pull)
	{
		powerUse = FP_PULL;
	}
	else
	{
		powerUse = FP_PUSH;
	}

	if (!meme && !WP_ForcePowerUsable( self, powerUse ) )
	{
		return;
	}

	// *CHANGE 65* fix - release rocket lock, old bug
	BG_ClearRocketLock(&self->client->ps);

	if (!meme && !pull && self->client->ps.saberLockTime > level.time && self->client->ps.saberLockFrame)
	{
		G_Sound( self, CHAN_BODY, G_SoundIndex( "sound/weapons/force/push.wav" ) );
		self->client->ps.powerups[PW_DISINT_4] = level.time + 1500;

		self->client->ps.saberLockHits += self->client->ps.fd.forcePowerLevel[FP_PUSH]*2;

		WP_ForcePowerStart( self, FP_PUSH, 0 );
		return;
	}

	WP_ForcePowerStart( self, powerUse, 0 );

	//make sure this plays and that you cannot press fire for about 1 second after this
	if ( pull )
	{
		G_Sound( self, CHAN_BODY, G_SoundIndex( "sound/weapons/force/pull.wav" ) );
		if (self->client->ps.forceHandExtend == HANDEXTEND_NONE)
		{
			self->client->ps.forceHandExtend = HANDEXTEND_FORCEPULL;
			if ( g_gametype.integer == GT_SIEGE && self->client->ps.weapon == WP_SABER )
			{//hold less so can attack right after a pull
				self->client->ps.forceHandExtendTime = level.time + 200;
			}
			else
			{
				self->client->ps.forceHandExtendTime = level.time + 400;
			}
		}
		self->client->ps.powerups[PW_DISINT_4] = self->client->ps.forceHandExtendTime + 200;
		self->client->ps.powerups[PW_PULL] = self->client->ps.powerups[PW_DISINT_4];
	}
	else
	{
		G_Sound( self, CHAN_BODY, G_SoundIndex( "sound/weapons/force/push.wav" ) );
		if (self->client->ps.forceHandExtend == HANDEXTEND_NONE)
		{
			self->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
			self->client->ps.forceHandExtendTime = level.time + 1000;
		}
		else if (self->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN && G_InGetUpAnim(&self->client->ps))
		{
			if (self->client->ps.forceDodgeAnim > 4)
			{
				self->client->ps.forceDodgeAnim -= 8;
			}
			self->client->ps.forceDodgeAnim += 8; //special case, play push on upper torso, but keep playing current knockdown anim on legs
		}
		self->client->ps.powerups[PW_DISINT_4] = level.time + 1100;
		self->client->ps.powerups[PW_PULL] = 0;
	}

	VectorCopy( self->client->ps.viewangles, fwdangles );
	AngleVectors( fwdangles, forward, right, NULL );
	VectorCopy( self->client->ps.origin, center );

	for ( i = 0 ; i < 3 ; i++ ) 
	{
		mins[i] = center[i] - radius;
		maxs[i] = center[i] + radius;
	}


	if (pull)
	{
		powerLevel = self->client->ps.fd.forcePowerLevel[FP_PULL];
		pushPower = 256*self->client->ps.fd.forcePowerLevel[FP_PULL];
	}
	else
	{
		powerLevel = self->client->ps.fd.forcePowerLevel[FP_PUSH];
		pushPower = 256*self->client->ps.fd.forcePowerLevel[FP_PUSH];
	}

	if (!powerLevel)
	{ //Shouldn't have made it here..
		if (meme) {
			powerLevel = 3;
			pushPower = 256 * 3;
		}
		else {
			return;
		}
	}

	qboolean exitedAimPracticeMode = qfalse;
	if (self->aimPracticeEntBeingUsed) {
		trap_SendServerCommand(self - g_entities, "cp \"Exited training.\"");
		self->aimPracticeMode = AIMPRACTICEMODE_NONE;
		self->aimPracticeEntBeingUsed = NULL;
		self->numAimPracticeSpawns = 0;
		self->numTotalAimPracticeHits = 0;
		memset(self->numAimPracticeHitsOfWeapon, 0, sizeof(self->numAimPracticeHitsOfWeapon));
		G_GiveRacemodeItemsAndFullStats(self);
		exitedAimPracticeMode = qtrue;
	}

	// should definitely pull/push
	if ( self && self->client ) {
		if ( pull ) {
			++self->client->stats->pull;
		} else {
			++self->client->stats->push;
		}
	}

	if (powerLevel == FORCE_LEVEL_2)
	{
		visionArc = 60;
	}
	else if (powerLevel == FORCE_LEVEL_3)
	{
		visionArc = 180;
	}

	if (powerLevel == FORCE_LEVEL_1)
	{ //can only push/pull targeted things at level 1
		VectorCopy(self->client->ps.origin, tfrom);
		tfrom[2] += self->client->ps.viewheight;
		AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
		tto[0] = tfrom[0] + fwd[0]*radius/2;
		tto[1] = tfrom[1] + fwd[1]*radius/2;
		tto[2] = tfrom[2] + fwd[2]*radius/2;

		qboolean compensate = self->client->sess.unlagged;
		if (g_unlagged.integer && compensate)
			G_TimeShiftAllClients(trap_Milliseconds() - (level.time - self->client->pers.cmd.serverTime), self, qfalse);

		trap_Trace(&tr, tfrom, NULL, NULL, tto, self->s.number, MASK_PLAYERSOLID);

		if (g_unlagged.integer && compensate)
			G_UnTimeShiftAllClients(self, qfalse);

		if (tr.fraction != 1.0 &&
			tr.entityNum != ENTITYNUM_NONE)
		{
			if (!g_entities[tr.entityNum].client && g_entities[tr.entityNum].s.eType == ET_NPC)
			{ //g2animent
				if (g_entities[tr.entityNum].s.genericenemyindex < level.time)
				{
					g_entities[tr.entityNum].s.genericenemyindex = level.time + 2000;
				}
			}

			numListedEntities = 0;
			entityList[numListedEntities] = tr.entityNum;

			if (pull)
			{
				if (ForcePowerUsableOn(self, &g_entities[tr.entityNum], FP_PULL) <= 0)
				{
					return;
				}
			}
			else
			{
				if (ForcePowerUsableOn(self, &g_entities[tr.entityNum], FP_PUSH) <= 0)
				{
					return;
				}
			}
			numListedEntities++;
		}
		else
		{
			//didn't get anything, so just
			return;
		}
	}
	else
	{
		numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

		e = 0;

		while (e < numListedEntities)
		{
			ent = &g_entities[entityList[e]];

			if (!ent->client && ent->s.eType == ET_NPC)
			{ //g2animent
				if (ent->s.genericenemyindex < level.time)
				{
					ent->s.genericenemyindex = level.time + 2000;
				}
			}

			if (ent)
			{
				if (ent->client)
				{
					VectorCopy(ent->client->ps.origin, thispush_org);
				}
				else
				{
					VectorCopy(ent->s.pos.trBase, thispush_org);
				}
			}

			if (ent)
			{ //not in the arc, don't consider it
				VectorCopy(self->client->ps.origin, tto);
				tto[2] += self->client->ps.viewheight;
				VectorSubtract(thispush_org, tto, a);
				vectoangles(a, a);

				if (ent->client && !InFieldOfVision(self->client->ps.viewangles, visionArc, a) &&
					ForcePowerUsableOn(self, ent, powerUse) > 0)
				{ //only bother with arc rules if the victim is a client
					entityList[e] = ENTITYNUM_NONE;
				}
				else if (ent->client)
				{
					if (pull)
					{
						if (ForcePowerUsableOn(self, ent, FP_PULL) <= 0)
						{
							entityList[e] = ENTITYNUM_NONE;
						}
					}
					else
					{
						if (ForcePowerUsableOn(self, ent, FP_PUSH) <= 0)
						{
							entityList[e] = ENTITYNUM_NONE;
						}
					}
				}
			}
			e++;
		}
	}

	qboolean gotBot = qfalse;
	for ( e = 0 ; e < numListedEntities ; e++ ) 
	{
		if (entityList[e] != ENTITYNUM_NONE &&
			entityList[e] >= 0 &&
			entityList[e] < MAX_GENTITIES)
		{
			ent = &g_entities[entityList[e]];
		}
		else
		{
			ent = NULL;
		}

		if (!ent)
			continue;
		if (ent == self)
			continue;
		if ( ent->client && ent->client->sess.inRacemode )
			continue; // don't use force powers on racers
		if ( ent->s.eType == ET_MISSILE && ent->r.ownerNum >= 0 && ent->r.ownerNum < MAX_CLIENTS && level.clients[ent->r.ownerNum].sess.inRacemode )
			continue; // don't use force powers on race projectiles
		if (ent->client && OnSameTeam(ent, self))
		{
			continue;
		}
		if ( !(ent->inuse) )
			continue;
		if ( ent->s.eType != ET_MISSILE )
		{
			if ( ent->s.eType != ET_ITEM )
			{
				//FIXME: need pushable objects
				if ( Q_stricmp( "func_button", ent->classname ) == 0 )
				{//we might push it
					if ( pull || !(ent->spawnflags&SPF_BUTTON_FPUSHABLE) )
					{//not force-pushable, never pullable
						continue;
					}
				}
				else
				{
					if ( ent->s.eFlags & EF_NODRAW )
					{
						continue;
					}
					if ( !ent->client )
					{
						if ( Q_stricmp( "lightsaber", ent->classname ) != 0 )
						{//not a lightsaber 
							if ( Q_stricmp( "func_door", ent->classname ) != 0 || !(ent->spawnflags & 2/*MOVER_FORCE_ACTIVATE*/) )
							{//not a force-usable door
								if ( Q_stricmp( "func_static", ent->classname ) != 0 || (!(ent->spawnflags&1/*F_PUSH*/)&&!(ent->spawnflags&2/*F_PULL*/)) )
								{//not a force-usable func_static
									if ( Q_stricmp( "limb", ent->classname ) )
									{//not a limb
										continue;
									}
								}
							}
							else if ( ent->moverState != MOVER_POS1 && ent->moverState != MOVER_POS2 )
							{//not at rest
								continue;
							}
						}
					}
					else if ( ent->client->NPC_class == CLASS_GALAKMECH 
						|| ent->client->NPC_class == CLASS_ATST
						|| ent->client->NPC_class == CLASS_RANCOR )
					{//can't push ATST or Galak or Rancor
						continue;
					}
				}
			}
		}
		else
		{
			if ( ent->s.pos.trType == TR_STATIONARY && (ent->s.eFlags&EF_MISSILE_STICK) )
			{//can't force-push/pull stuck missiles (detpacks, tripmines)
				continue;
			}
			if ( ent->s.pos.trType == TR_STATIONARY && ent->s.weapon != WP_THERMAL )
			{//only thermal detonators can be pushed once stopped
				continue;
			}
		}

		//this is all to see if we need to start a saber attack, if it's in flight, this doesn't matter
		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) 
		{
			if ( center[i] < ent->r.absmin[i] ) 
			{
				v[i] = ent->r.absmin[i] - center[i];
			} else if ( center[i] > ent->r.absmax[i] ) 
			{
				v[i] = center[i] - ent->r.absmax[i];
			} else 
			{
				v[i] = 0;
			}
		}

		VectorSubtract( ent->r.absmax, ent->r.absmin, size );
		VectorMA( ent->r.absmin, 0.5, size, ent_org );

		VectorSubtract( ent_org, center, dir );
		VectorNormalize( dir );
		if ( (dot1 = DotProduct( dir, forward )) < 0.6 )
			continue;

		dist = VectorLength( v );

		//Now check and see if we can actually deflect it
		//method1
		//if within a certain range, deflect it
		if ( dist >= radius ) 
		{
			continue;
		}
	
		//in PVS?
		if ( !ent->r.bmodel && !trap_InPVS( ent_org, self->client->ps.origin ) )
		{//must be in PVS
			continue;
		}

		//really should have a clear LOS to this thing...
		trap_Trace( &tr, self->client->ps.origin, vec3_origin, vec3_origin, ent_org, self->s.number, MASK_SHOT );
		if ( tr.fraction < 1.0f && tr.entityNum != ent->s.number )
		{//must have clear LOS
			//try from eyes too before you give up
			vec3_t eyePoint;
			VectorCopy(self->client->ps.origin, eyePoint);
			eyePoint[2] += self->client->ps.viewheight;
			trap_Trace( &tr, eyePoint, vec3_origin, vec3_origin, ent_org, self->s.number, MASK_SHOT );

			if ( tr.fraction < 1.0f && tr.entityNum != ent->s.number )
			{
				if (g_fixPushPullLOS.integer && ent->client) {
					if (!AdditionalPushPullLOSChecks(self, ent))
						continue;
				}
				else {
					continue;
				}
			}
			else {
				if (d_debugPushPullLOS.integer)
					PrintIngame(-1, "[%d] d_debugPushPullLOS: got LOS from %d to %d with ^5eyes to midpoint^7 check\n", level.time - level.startTime, self - g_entities, ent - g_entities);
			}
		}
		else {
			if (d_debugPushPullLOS.integer)
				PrintIngame(-1, "[%d] d_debugPushPullLOS: got LOS from %d to %d with ^2origin to midpoint^7 check\n", level.time - level.startTime, self - g_entities, ent - g_entities);
		}

		if (self->client && self - g_entities < MAX_CLIENTS && self->client->sess.inRacemode) {
			// the puller is in racemode

			if (!ent->isAimPracticePack)
				continue; // we only care about trying to push/pull a bot

			if (level.topAimRecordsEnabled && !exitedAimPracticeMode && !gotBot) {
				if (pull) {
					trap_SendServerCommand(self - g_entities, va("cp \"Entering free training: ^3%s\"", ent->isAimPracticePack->packName));
					self->aimPracticeMode = AIMPRACTICEMODE_UNTIMED;
					self->aimPracticeEntBeingUsed = ent;
					self->numAimPracticeSpawns = 0;
					self->numTotalAimPracticeHits = 0;
					memset(self->numAimPracticeHitsOfWeapon, 0, sizeof(self->numAimPracticeHitsOfWeapon));
					self->client->ps.powerups[PW_REDFLAG] = self->client->ps.powerups[PW_BLUEFLAG] = 0;
					gotBot = qtrue;
				}
				else {
					if (ent->isAimPracticePack->isTemp)
						trap_SendServerCommand(self - g_entities, va("cp \"Entering timed training: ^3%s^7\nPack is unsaved; new records cannot be set.\"", ent->isAimPracticePack->packName));
					else if (ent->isAimPracticePack->changed)
						trap_SendServerCommand(self - g_entities, va("cp \"Entering timed training: ^3%s^7\nPack has been modified; new records cannot be set.\"", ent->isAimPracticePack->packName));
					else
						trap_SendServerCommand(self - g_entities, va("cp \"Entering timed training: ^3%s\"", ent->isAimPracticePack->packName));
					self->aimPracticeMode = AIMPRACTICEMODE_TIMED;
					self->aimPracticeEntBeingUsed = ent;
					self->numAimPracticeSpawns = 0;
					self->numTotalAimPracticeHits = 0;
					memset(self->numAimPracticeHitsOfWeapon, 0, sizeof(self->numAimPracticeHitsOfWeapon));
					self->client->ps.powerups[PW_REDFLAG] = self->client->ps.powerups[PW_BLUEFLAG] = 0;
					gotBot = qtrue;
				}
			}
			continue; // never actually push/pull anyone
		}
		else if (ent->isAimPracticePack) {
			continue;
		}

		// ok, we are within the radius, add us to the incoming list
		push_list[ent_count] = ent;
		ent_count++;
	}

	if ( ent_count )
	{
		//method1:
		for ( x = 0; x < ent_count; x++ )
		{
			int modPowerLevel = powerLevel;

	
			if (push_list[x]->client)
			{
				modPowerLevel = WP_AbsorbConversion(push_list[x], push_list[x]->client->ps.fd.forcePowerLevel[FP_ABSORB], self, powerUse, powerLevel, forcePowerNeeded[self->client->ps.fd.forcePowerLevel[powerUse]][powerUse]);
				if (modPowerLevel == -1)
				{
					modPowerLevel = powerLevel;
				}
			}

			pushPower = 256*modPowerLevel;

			if (push_list[x]->client)
			{
				VectorCopy(push_list[x]->client->ps.origin, thispush_org);
			}
			else
			{
				VectorCopy(push_list[x]->s.origin, thispush_org);
			}

			if ( push_list[x]->client )
			{//FIXME: make enemy jedi able to hunker down and resist this?
				int otherPushPower = push_list[x]->client->ps.fd.forcePowerLevel[powerUse];
				qboolean canPullWeapon = qtrue;
				float dirLen = 0;

				if ( g_debugMelee.integer )
				{
					if ( (push_list[x]->client->ps.pm_flags&PMF_STUCK_TO_WALL) )
					{//no resistance if stuck to wall
						//push/pull them off the wall
						otherPushPower = 0;
						G_LetGoOfWall( push_list[x] );
					}
				}

				pushPowerMod = pushPower;

				if (push_list[x]->client->pers.cmd.forwardmove ||
					push_list[x]->client->pers.cmd.rightmove)
				{ //if you are moving, you get one less level of defense
					otherPushPower--;

					if (otherPushPower < 0)
					{
						otherPushPower = 0;
					}
				}

				if (otherPushPower && CanCounterThrow(push_list[x], self, pull))
				{
					if ( pull )
					{
						G_Sound( push_list[x], CHAN_BODY, G_SoundIndex( "sound/weapons/force/pull.wav" ) );
						push_list[x]->client->ps.forceHandExtend = HANDEXTEND_FORCEPULL;
						push_list[x]->client->ps.forceHandExtendTime = level.time + 400;
					}
					else
					{
						G_Sound( push_list[x], CHAN_BODY, G_SoundIndex( "sound/weapons/force/push.wav" ) );
						push_list[x]->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
						push_list[x]->client->ps.forceHandExtendTime = level.time + 1000;
					}
					push_list[x]->client->ps.powerups[PW_DISINT_4] = push_list[x]->client->ps.forceHandExtendTime + 200;

					if (pull)
					{
						push_list[x]->client->ps.powerups[PW_PULL] = push_list[x]->client->ps.powerups[PW_DISINT_4];
					}
					else
					{
						push_list[x]->client->ps.powerups[PW_PULL] = 0;
					}

					//Make a counter-throw effect

					if (otherPushPower >= modPowerLevel)
					{
						pushPowerMod = 0;
						canPullWeapon = qfalse;
					}
					else
					{
						int powerDif = (modPowerLevel - otherPushPower);

						if (powerDif >= 3)
						{
							pushPowerMod -= pushPowerMod*0.2;
						}
						else if (powerDif == 2)
						{
							pushPowerMod -= pushPowerMod*0.4;
						}
						else if (powerDif == 1)
						{
							pushPowerMod -= pushPowerMod*0.8;
						}

						if (pushPowerMod < 0)
						{
							pushPowerMod = 0;
						}
					}
				}

				//shove them
				if ( pull )
				{
					VectorSubtract( self->client->ps.origin, thispush_org, pushDir );

					if (push_list[x]->client && VectorLength(pushDir) <= 256)
					{
						int randfact = 0;

						if (modPowerLevel == FORCE_LEVEL_1)
						{
							randfact = 3;
						}
						else if (modPowerLevel == FORCE_LEVEL_2)
						{
							randfact = 7;
						}
						else if (modPowerLevel == FORCE_LEVEL_3)
						{
							randfact = 10;
						}

						if (!OnSameTeam(self, push_list[x]) && Q_irand(1, 10) <= randfact && canPullWeapon)
						{
							vec3_t uorg, vecnorm;

							VectorCopy(self->client->ps.origin, uorg);
							uorg[2] += 64;

							VectorSubtract(uorg, thispush_org, vecnorm);
							VectorNormalize(vecnorm);

							TossClientWeapon(push_list[x], vecnorm, 500);
						}
					}
				}
				else
				{
					VectorSubtract( thispush_org, self->client->ps.origin, pushDir );
				}

				if ((modPowerLevel > otherPushPower || push_list[x]->client->ps.m_iVehicleNum) && push_list[x]->client)
				{
					if (modPowerLevel == FORCE_LEVEL_3 &&
						push_list[x]->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN)
					{
						dirLen = VectorLength(pushDir);

						if (BG_KnockDownable(&push_list[x]->client->ps) &&
							dirLen <= (64*((modPowerLevel - otherPushPower)-1)))
						{ //can only do a knockdown if fairly close
							push_list[x]->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
							push_list[x]->client->ps.forceHandExtendTime = level.time + 700;
							push_list[x]->client->ps.forceDodgeAnim = 0; //this toggles between 1 and 0, when it's 1 we should play the get up anim
							push_list[x]->client->ps.quickerGetup = qtrue;
						}
						else if (push_list[x]->s.number < MAX_CLIENTS && push_list[x]->client->ps.m_iVehicleNum &&
							dirLen <= 128.0f )
						{ //a player on a vehicle
							gentity_t *vehEnt = &g_entities[push_list[x]->client->ps.m_iVehicleNum];
							if (vehEnt->inuse && vehEnt->client && vehEnt->m_pVehicle)
							{
								if (vehEnt->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER ||
									vehEnt->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL)
								{ //push the guy off
									vehEnt->m_pVehicle->m_pVehicleInfo->Eject(vehEnt->m_pVehicle, (bgEntity_t *)push_list[x], qfalse);
								}
							}
						}
					}
				}

				if (!dirLen)
				{
					dirLen = VectorLength(pushDir);
				}

				VectorNormalize(pushDir);

				if (push_list[x]->client)
				{
					//escape a force grip if we're in one
					if (self->client->ps.fd.forceGripBeingGripped > level.time)
					{ //force the enemy to stop gripping me if I managed to push him
						if (push_list[x]->client->ps.fd.forceGripEntityNum == self->s.number)
						{
							if (modPowerLevel >= push_list[x]->client->ps.fd.forcePowerLevel[FP_GRIP])
							{ //only break the grip if our push/pull level is >= their grip level
								WP_ForcePowerStop(push_list[x], FP_GRIP);
								self->client->ps.fd.forceGripBeingGripped = 0;
								push_list[x]->client->ps.fd.forceGripUseTime = level.time + 500; //since we just broke out of it..
							}
						}
					}

					push_list[x]->client->ps.otherKiller = self->s.number;
					push_list[x]->client->ps.otherKillerTime = level.time + 5000;
					push_list[x]->client->ps.otherKillerDebounceTime = level.time + 100;

					pushPowerMod -= (dirLen*0.7);
					if (pushPowerMod < 16)
					{
						pushPowerMod = 16;
					}

					//fullbody push effect
					push_list[x]->client->pushEffectTime = level.time + 600;

					push_list[x]->client->ps.velocity[0] = pushDir[0]*pushPowerMod;
					push_list[x]->client->ps.velocity[1] = pushDir[1]*pushPowerMod;

					if ((int)push_list[x]->client->ps.velocity[2] == 0)
					{ //if not going anywhere vertically, boost them up a bit
						push_list[x]->client->ps.velocity[2] = pushDir[2]*pushPowerMod;

						if (push_list[x]->client->ps.velocity[2] < 128)
						{
							push_list[x]->client->ps.velocity[2] = 128;
						}
					}
					else
					{
						push_list[x]->client->ps.velocity[2] = pushDir[2]*pushPowerMod;
					}
				}
			}
			else if ( push_list[x]->s.eType == ET_MISSILE && push_list[x]->s.pos.trType != TR_STATIONARY && (push_list[x]->s.pos.trType != TR_INTERPOLATE||push_list[x]->s.weapon != WP_THERMAL) )//rolling and stationary thermal detonators are dealt with below
			{
				if ( pull )
				{//deflect rather than reflect?
				}
				else 
				{
					G_ReflectMissile( self, push_list[x], forward, g_randomConeReflection.integer & CONE_REFLECT_PUSH );
				}
			}
			else if ( !Q_stricmp( "func_static", push_list[x]->classname ) )
			{//force-usable func_static
				if ( !pull && (push_list[x]->spawnflags&1) )
				{
					GEntity_UseFunc( push_list[x], self, self );
				}
				else if ( pull && (push_list[x]->spawnflags&2) )
				{
					GEntity_UseFunc( push_list[x], self, self );
				}
			}
			else if ( !Q_stricmp( "func_door", push_list[x]->classname ) && (push_list[x]->spawnflags&2) )
			{//push/pull the door
				vec3_t	pos1, pos2;
				vec3_t	trFrom;

				VectorCopy(self->client->ps.origin, trFrom);
				trFrom[2] += self->client->ps.viewheight;

				AngleVectors( self->client->ps.viewangles, forward, NULL, NULL );
				VectorNormalize( forward );
				VectorMA( trFrom, radius, forward, end );
				trap_Trace( &tr, trFrom, vec3_origin, vec3_origin, end, self->s.number, MASK_SHOT );
				if ( tr.entityNum != push_list[x]->s.number || tr.fraction == 1.0 || tr.allsolid || tr.startsolid )
				{//must be pointing right at it
					continue;
				}

				if ( VectorCompare( vec3_origin, push_list[x]->s.origin ) )
				{//does not have an origin brush, so pos1 & pos2 are relative to world origin, need to calc center
					VectorSubtract( push_list[x]->r.absmax, push_list[x]->r.absmin, size );
					VectorMA( push_list[x]->r.absmin, 0.5, size, center );
					if ( (push_list[x]->spawnflags&1) && push_list[x]->moverState == MOVER_POS1 )
					{//if at pos1 and started open, make sure we get the center where it *started* because we're going to add back in the relative values pos1 and pos2
						VectorSubtract( center, push_list[x]->pos1, center );
					}
					else if ( !(push_list[x]->spawnflags&1) && push_list[x]->moverState == MOVER_POS2 )
					{//if at pos2, make sure we get the center where it *started* because we're going to add back in the relative values pos1 and pos2
						VectorSubtract( center, push_list[x]->pos2, center );
					}
					VectorAdd( center, push_list[x]->pos1, pos1 );
					VectorAdd( center, push_list[x]->pos2, pos2 );
				}
				else
				{//actually has an origin, pos1 and pos2 are absolute
					VectorCopy( push_list[x]->r.currentOrigin, center );
					VectorCopy( push_list[x]->pos1, pos1 );
					VectorCopy( push_list[x]->pos2, pos2 );
				}

				if ( Distance( pos1, trFrom ) < Distance( pos2, trFrom ) )
				{//pos1 is closer
					if ( push_list[x]->moverState == MOVER_POS1 )
					{//at the closest pos
						if ( pull )
						{//trying to pull, but already at closest point, so screw it
							continue;
						}
					}
					else if ( push_list[x]->moverState == MOVER_POS2 )
					{//at farthest pos
						if ( !pull )
						{//trying to push, but already at farthest point, so screw it
							continue;
						}
					}
				}
				else
				{//pos2 is closer
					if ( push_list[x]->moverState == MOVER_POS1 )
					{//at the farthest pos
						if ( !pull )
						{//trying to push, but already at farthest point, so screw it
							continue;
						}
					}
					else if ( push_list[x]->moverState == MOVER_POS2 )
					{//at closest pos
						if ( pull )
						{//trying to pull, but already at closest point, so screw it
							continue;
						}
					}
				}
				GEntity_UseFunc( push_list[x], self, self );
			}
			else if ( Q_stricmp( "func_button", push_list[x]->classname ) == 0 )
			{//pretend you pushed it
				Touch_Button( push_list[x], self, NULL );
				continue;
			}
		}
	}

	//attempt to break any leftover grips
	//if we're still in a current grip that wasn't broken by the push, it will still remain
	self->client->dangerTime = level.time;
	self->client->ps.eFlags &= ~EF_INVULNERABLE;
	self->client->invulnerableTimer = 0;

	if (self->client->ps.fd.forceGripBeingGripped > level.time)
	{
		self->client->ps.fd.forceGripBeingGripped = 0;
	}
}

void WP_ForcePowerStop( gentity_t *self, forcePowers_t forcePower )
{
 	int wasActive = self->client->ps.fd.forcePowersActive;

	self->client->ps.fd.forcePowersActive &= ~( 1 << forcePower );

	switch( (int)forcePower )
	{
	case FP_HEAL:
		self->client->ps.fd.forceHealAmount = 0;
		self->client->ps.fd.forceHealTime = 0;
		break;
	case FP_LEVITATION:
		break;
	case FP_SPEED:
		if (wasActive & (1 << FP_SPEED))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_2-50], CHAN_VOICE);
			StartForceTimer(self, (int)forcePower, 0);
		}
		break;
	case FP_PUSH:
		break;
	case FP_PULL:
		break;
	case FP_TELEPATHY:
		if (wasActive & (1 << FP_TELEPATHY))
		{
			G_Sound( self, CHAN_AUTO, G_SoundIndex("sound/weapons/force/distractstop.wav") );
			StartForceTimer(self, (int)forcePower, 0);
		}
		self->client->ps.fd.forceMindtrickTargetIndex = 0;
		self->client->ps.fd.forceMindtrickTargetIndex2 = 0;
		self->client->ps.fd.forceMindtrickTargetIndex3 = 0;
		self->client->ps.fd.forceMindtrickTargetIndex4 = 0;
		break;
	case FP_SEE:
		if (wasActive & (1 << FP_SEE))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_5-50], CHAN_VOICE);
			StartForceTimer(self, (int)forcePower, 0);
		}
		break;
	case FP_GRIP:
		self->client->ps.fd.forceGripUseTime = level.time + 500;
		if (self->client->ps.fd.forcePowerLevel[FP_GRIP] > FORCE_LEVEL_1 &&
			g_entities[self->client->ps.fd.forceGripEntityNum].client &&
			g_entities[self->client->ps.fd.forceGripEntityNum].health > 0 &&
			g_entities[self->client->ps.fd.forceGripEntityNum].inuse &&
			(level.time - g_entities[self->client->ps.fd.forceGripEntityNum].client->ps.fd.forceGripStarted) > 500)
		{ //if we had our throat crushed in for more than half a second, gasp for air when we're let go
			if (wasActive & (1 << FP_GRIP))
			{
				G_EntitySound( &g_entities[self->client->ps.fd.forceGripEntityNum], CHAN_VOICE, G_SoundIndex("*gasp.wav") );
			}
		}

		if (g_entities[self->client->ps.fd.forceGripEntityNum].client &&
			g_entities[self->client->ps.fd.forceGripEntityNum].inuse)
		{
			
			g_entities[self->client->ps.fd.forceGripEntityNum].client->ps.forceGripChangeMovetype = PM_NORMAL;
		}

		if (self->client->ps.forceHandExtend == HANDEXTEND_FORCE_HOLD)
		{
			self->client->ps.forceHandExtendTime = 0;
		}

		self->client->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;

		self->client->ps.powerups[PW_DISINT_4] = 0;
		break;
	case FP_LIGHTNING:
		if ( self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] < FORCE_LEVEL_2 && !g_lightningRework.integer )
		{//don't do it again for 3 seconds, minimum... FIXME: this should be automatic once regeneration is slower (normal)
			self->client->ps.fd.forcePowerDebounce[FP_LIGHTNING] = level.time + 3000;
		}
		else
		{
			self->client->ps.fd.forcePowerDebounce[FP_LIGHTNING] = level.time + (g_lightningRework.integer ? 500 : 1500);
		}
		if (self->client->ps.forceHandExtend == HANDEXTEND_FORCE_HOLD)
		{
			self->client->ps.forceHandExtendTime = 0; //reset hand position
		}

		self->client->ps.activeForcePass = 0;
		break;
	case FP_RAGE:
		self->client->ps.fd.forceRageRecoveryTime = level.time + FixDuration(RAGE_RECOVERY_TIME);
		StartForceTimer(self, FP_SABER_OFFENSE, FixDuration(RAGE_RECOVERY_TIME));
		if (wasActive & (1 << FP_RAGE))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_3-50], CHAN_VOICE);

			if (self->client->pers.ragesince && self->client->pers.ragesince < level.time) {
				self->client->stats->rageTimeUsed += level.time - self->client->pers.ragesince;
			}
		}
		break;
	case FP_ABSORB:
		if (wasActive & (1 << FP_ABSORB))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_3-50], CHAN_VOICE);
			StartForceTimer(self, (int)forcePower, 0);
		}
		break;
	case FP_PROTECT:
		if (wasActive & (1 << FP_PROTECT))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_3-50], CHAN_VOICE);
			StartForceTimer(self, (int)forcePower, 0);

			if ( self->client->pers.protsince && self->client->pers.protsince < level.time ) {
				self->client->stats->protTimeUsed += level.time - self->client->pers.protsince;
			}
		}
		break;
	case FP_DRAIN:
		if (!g_drainRework.integer) {
			if (self->client->ps.fd.forcePowerLevel[FP_DRAIN] < FORCE_LEVEL_2)
			{//don't do it again for 3 seconds, minimum...
				self->client->ps.fd.forcePowerDebounce[FP_DRAIN] = level.time + 3000;
			}
			else
			{
				self->client->ps.fd.forcePowerDebounce[FP_DRAIN] = level.time + 1500;
			}
		}
		else {
			self->client->ps.fd.forcePowerDebounce[FP_DRAIN] = level.time + DRAIN_REWORK_COOLDOWN;
		}

		if (self->client->ps.forceHandExtend == HANDEXTEND_FORCE_HOLD && !(g_drainRework.integer && level.time < self->client->ps.forceHandExtendTime))
		{
			self->client->ps.forceHandExtendTime = 0; //reset hand position
		}

		self->client->ps.activeForcePass = 0;
	default:
		break;
	}
}

void DoGripAction(gentity_t *self, forcePowers_t forcePower)
{
	gentity_t *gripEnt;
	int gripLevel = 0;
	trace_t tr;
	vec3_t a;
	vec3_t fwd, fwd_o, start_o, nvel;

	self->client->dangerTime = level.time;
	self->client->ps.eFlags &= ~EF_INVULNERABLE;
	self->client->invulnerableTimer = 0;

	gripEnt = &g_entities[self->client->ps.fd.forceGripEntityNum];

	qboolean meme = (!level.wasRestarted && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
	if (meme && self->client->sess.meme[0]) {
		int num = atoi(self->client->sess.meme);
		if (Q_isanumber(self->client->sess.meme) && num >= 0 && num < MAX_CLIENTS) {
			gentity_t *dank = &g_entities[num];
			if (dank->inuse && dank->client) {
				gripEnt = dank;
				if (dank->client->sess.sessionTeam == TEAM_SPECTATOR && dank->client->sess.spectatorState == SPECTATOR_FOLLOW)
					StopFollowing(dank);
			}
		}
		else {
			gentity_t *dank = NULL;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *thisGuy = &g_entities[i];
				if (!thisGuy->inuse || !thisGuy->client || !thisGuy->client->account || Q_stricmp(thisGuy->client->account->name, self->client->sess.meme))
					continue;
				dank = thisGuy;
				break;
			}
			if (dank) {
				gripEnt = dank;
				if (dank->client->sess.sessionTeam == TEAM_SPECTATOR && dank->client->sess.spectatorState == SPECTATOR_FOLLOW)
					StopFollowing(dank);
			}
		}
		self->client->ps.fd.forceGripEntityNum = gripEnt - g_entities;
	}
	
	if (!gripEnt || !gripEnt->client || !gripEnt->inuse || (gripEnt->health < 1 && !meme))
	{
		WP_ForcePowerStop(self, forcePower);
		self->client->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;

		if (gripEnt && gripEnt->client && gripEnt->inuse)
			gripEnt->client->ps.forceGripChangeMovetype = PM_NORMAL;
		return;
	}
	else {
		int usable = ForcePowerUsableOn(self, gripEnt, FP_GRIP);
		if (usable <= 0 && !meme) {
			if (usable == -1 && g_gripAbsorbFix.integer) {
				// we failed to *continue* gripping because the victim began using absorb mid-grip
				// do not absorb force power here, but do start the "gripped an absorber" cooldown
				// so that we don't absorb force points from the ForceGrip function immediately afterwards
				self->client->grippedAnAbsorberTime = level.time;

				// also play the sound/effect
				gentity_t *abSound = G_PreDefSound(gripEnt, PDSOUND_ABSORBHIT);
				abSound->s.trickedentindex = gripEnt->s.number;
				gripEnt->client->forcePowerSoundDebounce = level.time + 400;
			}
			WP_ForcePowerStop(self, forcePower);
			self->client->ps.fd.forceGripEntityNum = ENTITYNUM_NONE;

			if (gripEnt && gripEnt->client && gripEnt->inuse)
				gripEnt->client->ps.forceGripChangeMovetype = PM_NORMAL;
			return;
		}
	}

	VectorSubtract(gripEnt->client->ps.origin, self->client->ps.origin, a);
	
	trap_Trace(&tr, self->client->ps.origin, NULL, NULL, gripEnt->client->ps.origin, self->s.number, MASK_PLAYERSOLID);

	// fun fact, this line never actually does anything because if the victim were absorbing, we would have returned a few lines above (based raven coders)
	gripLevel = WP_AbsorbConversion(gripEnt, gripEnt->client->ps.fd.forcePowerLevel[FP_ABSORB], self, FP_GRIP, self->client->ps.fd.forcePowerLevel[FP_GRIP], forcePowerNeeded[self->client->ps.fd.forcePowerLevel[FP_GRIP]][FP_GRIP]);
	if (meme)
		gripLevel = 3;
	if (gripLevel == -1)
	{
		gripLevel = self->client->ps.fd.forcePowerLevel[FP_GRIP];
	}

	if (!gripLevel)
	{
		WP_ForcePowerStop(self, forcePower);
		return;
	}

	// no distance check for maintaining grip 3 (would only punish fast strafing),
	// and forgiving distance check for maintaining grip1/2
	if (gripLevel != 3 && VectorLength(a) > MAX_GRIP_DISTANCE * 2 && !meme)
	{
		WP_ForcePowerStop(self, forcePower);
		return;
	}

	if ( !InFront( gripEnt->client->ps.origin, self->client->ps.origin, self->client->ps.viewangles, 0.9f ) &&
		gripLevel < FORCE_LEVEL_3)
	{
		WP_ForcePowerStop(self, forcePower);
		return;
	}

	if (tr.fraction != 1.0f &&
		tr.entityNum != gripEnt->s.number && !meme /*&&
		gripLevel < FORCE_LEVEL_3*/)
	{
		WP_ForcePowerStop(self, forcePower);
		return;
	}

	if (self->client->ps.fd.forcePowerDebounce[FP_GRIP] < level.time)
	{ //2 damage per second while choking, resulting in 10 damage total (not including The Squeeze<tm>)
		self->client->ps.fd.forcePowerDebounce[FP_GRIP] = level.time + 1000;
		if (!meme)
			G_Damage(gripEnt, self, self, NULL, NULL, 2, DAMAGE_NO_ARMOR, MOD_FORCE_DARK);
	}

	Jetpack_Off(gripEnt); //make sure the guy being gripped has his jetpack off.

	if (gripLevel == FORCE_LEVEL_1)
	{
		gripEnt->client->ps.fd.forceGripBeingGripped = level.time + 1000;
		if (g_gripRework.integer) {
			gripEnt->client->ps.forceGripChangeMovetype = PM_FLOAT;
			gripEnt->client->ps.otherKiller = self->s.number;
			gripEnt->client->ps.otherKillerTime = level.time + 5000;
			gripEnt->client->ps.otherKillerDebounceTime = level.time + 100;
		}
		
		if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 5000 && !meme)
		{
			WP_ForcePowerStop(self, forcePower);
		}
		return;
	}

	if (gripLevel == FORCE_LEVEL_2)
	{
		gripEnt->client->ps.fd.forceGripBeingGripped = level.time + 1000;

		if (gripEnt->client->ps.forceGripMoveInterval < level.time)
		{
			gripEnt->client->ps.velocity[2] = 30;

			gripEnt->client->ps.forceGripMoveInterval = level.time + Com_Clampi(1000 / g_svfps.integer, 500, g_gripRefreshRate.integer); //only update velocity every 300ms, so as to avoid heavy bandwidth usage
		}

		gripEnt->client->ps.otherKiller = self->s.number;
		gripEnt->client->ps.otherKillerTime = level.time + 5000;
		gripEnt->client->ps.otherKillerDebounceTime = level.time + 100;

		gripEnt->client->ps.forceGripChangeMovetype = PM_FLOAT;

		if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 3000 && !self->client->ps.fd.forceGripDamageDebounceTime)
		{ //if we managed to lift him into the air for 2 seconds, give him a crack
			self->client->ps.fd.forceGripDamageDebounceTime = 1;
			if (!meme)
				G_Damage(gripEnt, self, self, NULL, NULL, 20, DAMAGE_NO_ARMOR, MOD_FORCE_DARK);

			//Must play custom sounds on the actual entity. Don't use G_Sound (it creates a temp entity for the sound)
			G_EntitySound( gripEnt, CHAN_VOICE, G_SoundIndex(va( "*choke%d.wav", Q_irand( 1, 3 ) )) );

			gripEnt->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
			gripEnt->client->ps.forceHandExtendTime = level.time + 2000;

			if (!meme && gripEnt->client->ps.fd.forcePowersActive & (1 << FP_GRIP))
			{ //choking, so don't let him keep gripping himself
				WP_ForcePowerStop(gripEnt, FP_GRIP);
			}
		}
		else if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 4000 && !meme)
		{
			WP_ForcePowerStop(self, forcePower);
		}
		return;
	}

	if (gripLevel == FORCE_LEVEL_3)
	{
		gripEnt->client->ps.fd.forceGripBeingGripped = level.time + 1000;

		gripEnt->client->ps.otherKiller = self->s.number;
		gripEnt->client->ps.otherKillerTime = level.time + 5000;
		gripEnt->client->ps.otherKillerDebounceTime = level.time + 100;

		gripEnt->client->ps.forceGripChangeMovetype = PM_FLOAT;

		if (gripEnt->client->ps.forceGripMoveInterval < level.time)
		{
			float nvLen = 0;

			VectorCopy(gripEnt->client->ps.origin, start_o);
			AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
			fwd_o[0] = self->client->ps.origin[0] + fwd[0]*128;
			fwd_o[1] = self->client->ps.origin[1] + fwd[1]*128;
			fwd_o[2] = self->client->ps.origin[2] + fwd[2]*128;
			fwd_o[2] += 16;
			VectorSubtract(fwd_o, start_o, nvel);

			nvLen = VectorLength(nvel);

			float factor = ((float)(Com_Clampi(1000 / g_svfps.integer, 500, g_gripRefreshRate.integer)) / 300.0f);

			if (nvLen < 16 * factor)
			{ //within x units of desired spot
				VectorNormalize(nvel);
				gripEnt->client->ps.velocity[0] = nvel[0]*8;
				gripEnt->client->ps.velocity[1] = nvel[1]*8;
				gripEnt->client->ps.velocity[2] = nvel[2]*8;
			}
			else if (nvLen < 64 * factor)
			{
				VectorNormalize(nvel);
				gripEnt->client->ps.velocity[0] = nvel[0]*128;
				gripEnt->client->ps.velocity[1] = nvel[1]*128;
				gripEnt->client->ps.velocity[2] = nvel[2]*128;
			}
			else if (nvLen < 128 * factor)
			{
				VectorNormalize(nvel);
				gripEnt->client->ps.velocity[0] = nvel[0]*256;
				gripEnt->client->ps.velocity[1] = nvel[1]*256;
				gripEnt->client->ps.velocity[2] = nvel[2]*256;
			}
			else if (nvLen < 200 * factor)
			{
				VectorNormalize(nvel);
				gripEnt->client->ps.velocity[0] = nvel[0]*512;
				gripEnt->client->ps.velocity[1] = nvel[1]*512;
				gripEnt->client->ps.velocity[2] = nvel[2]*512;
			}
			else
			{
				VectorNormalize(nvel);
				gripEnt->client->ps.velocity[0] = nvel[0]*700;
				gripEnt->client->ps.velocity[1] = nvel[1]*700;
				gripEnt->client->ps.velocity[2] = nvel[2]*700;
			}

			gripEnt->client->ps.forceGripMoveInterval = level.time + Com_Clampi(1000 / g_svfps.integer, 500, g_gripRefreshRate.integer); //only update velocity every 300ms, so as to avoid heavy bandwidth usage
		}

		if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 3000 && !self->client->ps.fd.forceGripDamageDebounceTime)
		{ //if we managed to lift him into the air for 2 seconds, give him a crack
			self->client->ps.fd.forceGripDamageDebounceTime = 1;
			if (!meme)
				G_Damage(gripEnt, self, self, NULL, NULL, 40, DAMAGE_NO_ARMOR, MOD_FORCE_DARK);

			//Must play custom sounds on the actual entity. Don't use G_Sound (it creates a temp entity for the sound)
			G_EntitySound( gripEnt, CHAN_VOICE, G_SoundIndex(va( "*choke%d.wav", Q_irand( 1, 3 ) )) );

			gripEnt->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
			gripEnt->client->ps.forceHandExtendTime = level.time + 2000;

			if (!meme && gripEnt->client->ps.fd.forcePowersActive & (1 << FP_GRIP))
			{ //choking, so don't let him keep gripping himself
				WP_ForcePowerStop(gripEnt, FP_GRIP);
			}
		}
		else if ((level.time - gripEnt->client->ps.fd.forceGripStarted) > 4000 && !meme)
		{
			WP_ForcePowerStop(self, forcePower);
		}
		return;
	}
}

qboolean G_IsMindTricked(forcedata_t *victimFd, int mindTricker)
{
	int checkIn;
	int trickIndex1, trickIndex2, trickIndex3, trickIndex4;
	int sub = 0;

	if (!victimFd)
	{
		return qfalse;
	}

	trickIndex1 = victimFd->forceMindtrickTargetIndex;
	trickIndex2 = victimFd->forceMindtrickTargetIndex2;
	trickIndex3 = victimFd->forceMindtrickTargetIndex3;
	trickIndex4 = victimFd->forceMindtrickTargetIndex4;

	if (mindTricker > 47)
	{
		checkIn = trickIndex4;
		sub = 48;
	}
	else if (mindTricker > 31)
	{
		checkIn = trickIndex3;
		sub = 32;
	}
	else if (mindTricker > 15)
	{
		checkIn = trickIndex2;
		sub = 16;
	}
	else
	{
		checkIn = trickIndex1;
	}

	if (checkIn & (1 << (mindTricker -sub)))
	{
		return qtrue;
	}
	
	return qfalse;
}

static void RemoveTrickedEnt(forcedata_t *fd, int client)
{
	if (!fd)
	{
		return;
	}

	if (client > 47)
	{
		fd->forceMindtrickTargetIndex4 &= ~(1 << (client-48));
	}
	else if (client > 31)
	{
		fd->forceMindtrickTargetIndex3 &= ~(1 << (client-32));
	}
	else if (client > 15)
	{
		fd->forceMindtrickTargetIndex2 &= ~(1 << (client-16));
	}
	else
	{
		fd->forceMindtrickTargetIndex &= ~(1 << client);
	}
}

extern int g_LastFrameTime;
extern int g_TimeSinceLastFrame;

static void WP_UpdateMindtrickEnts(gentity_t *self)
{
	int i = 0;

	while (i < MAX_CLIENTS)
	{
		if (G_IsMindTricked(&self->client->ps.fd, i))
		{
			gentity_t *ent = &g_entities[i];

			if ( !ent || !ent->client || !ent->inuse || ent->health < 1 ||
				(ent->client->ps.fd.forcePowersActive & (1 << FP_SEE)) )
			{
				RemoveTrickedEnt(&self->client->ps.fd, i);
			}
			else if ((level.time - self->client->dangerTime) < g_TimeSinceLastFrame*4)
			{ //Untrick this entity if the tricker (self) fires while in his fov
				if (trap_InPVS(ent->client->ps.origin, self->client->ps.origin) &&
					OrgVisible(ent->client->ps.origin, self->client->ps.origin, ent->s.number))
				{
					RemoveTrickedEnt(&self->client->ps.fd, i);
				}
			}
			else if (BG_HasYsalamiri(g_gametype.integer, &ent->client->ps))
			{
				RemoveTrickedEnt(&self->client->ps.fd, i);
			}
		}

		i++;
	}

	if (!self->client->ps.fd.forceMindtrickTargetIndex &&
		!self->client->ps.fd.forceMindtrickTargetIndex2 &&
		!self->client->ps.fd.forceMindtrickTargetIndex3 &&
		!self->client->ps.fd.forceMindtrickTargetIndex4)
	{ //everyone who we had tricked is no longer tricked, so stop the power
		WP_ForcePowerStop(self, FP_TELEPATHY);
	}
	else if (self->client->ps.powerups[PW_REDFLAG] ||
		self->client->ps.powerups[PW_BLUEFLAG])
	{
		WP_ForcePowerStop(self, FP_TELEPATHY);
	}
}

static void WP_ForcePowerRun( gentity_t *self, forcePowers_t forcePower, usercmd_t *cmd )
{
	qboolean meme;
	switch( (int)forcePower )
	{
	case FP_HEAL:
		if (self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_1)
		{
			if (self->client->ps.velocity[0] || self->client->ps.velocity[1] || self->client->ps.velocity[2])
			{
				WP_ForcePowerStop( self, forcePower );
				break;
			}
		}

		if (self->health < 1 || self->client->ps.stats[STAT_HEALTH] < 1)
		{
			WP_ForcePowerStop( self, forcePower );
			break;
		}

		if (self->client->ps.fd.forceHealTime > level.time)
		{
			break;
		}
		if ( self->health > self->client->ps.stats[STAT_MAX_HEALTH])
		{ //rww - we might start out over max_health and we don't want force heal taking us down to 100 or whatever max_health is
			WP_ForcePowerStop( self, forcePower );
			break;
		}
		self->client->ps.fd.forceHealTime = level.time + 1000;
		self->health++;
		self->client->ps.fd.forceHealAmount++;

		if ( self->health > self->client->ps.stats[STAT_MAX_HEALTH])	// Past max health
		{
			self->health = self->client->ps.stats[STAT_MAX_HEALTH];
			WP_ForcePowerStop( self, forcePower );
		}

		if ( (self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_1 && self->client->ps.fd.forceHealAmount >= 25) ||
			(self->client->ps.fd.forcePowerLevel[FP_HEAL] == FORCE_LEVEL_2 && self->client->ps.fd.forceHealAmount >= 33))
		{
			WP_ForcePowerStop( self, forcePower );
		}
		break;
	case FP_SPEED:
#ifdef FP_SPEED_IMPROVED
		// testing fp speed draining
		if (self->client->ps.fd.forcePowerDebounce[FP_SPEED] < level.time)
		{ 
			BG_ForcePowerDrain( &self->client->ps, forcePower, 2 );
			if (self->client->ps.fd.forcePower < 1)
			{
				WP_ForcePowerStop(self, forcePower);
			}

			self->client->ps.fd.forcePowerDebounce[FP_SPEED] = level.time + 1000;
		}
#endif

		//This is handled in PM_WalkMove and PM_StepSlideMove
		if ( self->client->holdingObjectiveItem >= MAX_CLIENTS  
			&& self->client->holdingObjectiveItem < ENTITYNUM_WORLD )
		{
			if ( g_entities[self->client->holdingObjectiveItem].genericValue15 )
			{//disables force powers
				WP_ForcePowerStop( self, forcePower );
			}
		}
		break;
	case FP_GRIP:
		meme = (!level.wasRestarted && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
		if (self->client->ps.forceHandExtend != HANDEXTEND_FORCE_HOLD && !meme)
		{
			WP_ForcePowerStop(self, FP_GRIP);
			break;
		}

		if (g_gripRework.integer == 2) { // new out-of-fp check: with new rework enabled, allow grip to continue with 0fp during the initial second
			if (self->client->ps.fd.forcePower < 1 && !meme && self->client->ps.fd.forcePowerDebounce[FP_PULL] < level.time)
			{
				WP_ForcePowerStop(self, FP_GRIP);
				break;
			}
		}

		if (self->client->ps.fd.forcePowerDebounce[FP_PULL] < level.time)
		{ //This is sort of not ideal. Using the debounce value reserved for pull for this because pull doesn't need it.
			if (!meme) {
				BG_ForcePowerDrain(&self->client->ps, forcePower, 1);
				self->client->ps.fd.forcePowerDebounce[FP_PULL] = level.time + 100;
			}
		}

		if (g_gripRework.integer != 2) { // old out-of-fp check
			if (self->client->ps.fd.forcePower < 1 && !meme)
			{
				WP_ForcePowerStop(self, FP_GRIP);
				break;
			}
		}

		DoGripAction(self, forcePower);
		break;
	case FP_LEVITATION:
		if ( self->client->ps.groundEntityNum != ENTITYNUM_NONE && !self->client->ps.fd.forceJumpZStart )
		{//done with jump
			WP_ForcePowerStop( self, forcePower );
		}
		break;
	case FP_RAGE:
		if (self->health < 1)
		{
			WP_ForcePowerStop(self, forcePower);
			break;
		}
		if (self->client->ps.forceRageDrainTime < level.time)
		{
			int addTime = 400;

			self->health -= 2;

			if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_1)
			{
				addTime = 150;
			}
			else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_2)
			{
				addTime = 300;
			}
			else if (self->client->ps.fd.forcePowerLevel[FP_RAGE] == FORCE_LEVEL_3)
			{
				addTime = 450;
			}
			self->client->ps.forceRageDrainTime = level.time + addTime;
		}

		if (self->health < 1)
		{
			self->health = 1;
			WP_ForcePowerStop(self, forcePower);
		}

		self->client->ps.stats[STAT_HEALTH] = self->health;
		break;
	case FP_DRAIN:
		meme = (!level.wasRestarted && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
		if (self->client->ps.forceHandExtend != HANDEXTEND_FORCE_HOLD && !meme)
		{
			WP_ForcePowerStop(self, forcePower);
			break;
		}

		if ( self->client->ps.fd.forcePowerLevel[FP_DRAIN] > FORCE_LEVEL_1 && !g_drainRework.integer )
		{//higher than level 1
			if ( (cmd->buttons & BUTTON_FORCE_DRAIN) || ((cmd->buttons & BUTTON_FORCEPOWER) && self->client->ps.fd.forcePowerSelected == FP_DRAIN) )
			{//holding it keeps it going
				self->client->ps.fd.forcePowerDuration[FP_DRAIN] = level.time + 500;
			}
		}
		// OVERRIDEFIXME
		if ( !WP_ForcePowerAvailable( self, forcePower, 0 ) || self->client->ps.fd.forcePowerDuration[FP_DRAIN] < level.time ||
			((!g_drainRework.integer && self->client->ps.fd.forcePower < 25) ||(g_drainRework.integer && self->client->ps.fd.forcePower < DRAIN_REWORK2_MINIMUMFORCE)))
		{
			WP_ForcePowerStop( self, forcePower );
		}
		else
		{
			ForceShootDrain( self );
		}
		break;
	case FP_LIGHTNING:
		meme = (!level.wasRestarted && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
		if (self->client->ps.forceHandExtend != HANDEXTEND_FORCE_HOLD && !meme)
		{ //Animation for hand extend doesn't end with hand out, so we have to limit lightning intervals by animation intervals (once hand starts to go in in animation, lightning should stop)
			WP_ForcePowerStop(self, forcePower);
			break;
		}

		if ( self->client->ps.fd.forcePowerLevel[FP_LIGHTNING] > FORCE_LEVEL_1 || g_lightningRework.integer )
		{//higher than level 1
			if ( (cmd->buttons & BUTTON_FORCE_LIGHTNING) || ((cmd->buttons & BUTTON_FORCEPOWER) && self->client->ps.fd.forcePowerSelected == FP_LIGHTNING) )
			{//holding it keeps it going
				self->client->ps.fd.forcePowerDuration[FP_LIGHTNING] = level.time + 500;
			}
		}
		// OVERRIDEFIXME
		if ( !WP_ForcePowerAvailable( self, forcePower, 0 ) || self->client->ps.fd.forcePowerDuration[FP_LIGHTNING] < level.time ||
			(self->client->ps.fd.forcePower < 25 && !g_lightningRework.integer) || !self->client->ps.fd.forcePower)
		{
			WP_ForcePowerStop( self, forcePower );
		}
		else
		{
			ForceShootLightning( self );
			BG_ForcePowerDrain( &self->client->ps, forcePower, 0 );
		}
		break;
	case FP_TELEPATHY:
		if ( self->client->holdingObjectiveItem >= MAX_CLIENTS  
			&& self->client->holdingObjectiveItem < ENTITYNUM_WORLD
			&& g_entities[self->client->holdingObjectiveItem].genericValue15 )
		{ //if force hindered can't mindtrick whilst carrying a siege item
			WP_ForcePowerStop( self, FP_TELEPATHY );
		}
		else
		{
			WP_UpdateMindtrickEnts(self);
		}
		break;
	case FP_SABER_OFFENSE:
		break;
	case FP_SABER_DEFENSE:
		break;
	case FP_SABERTHROW:
		break;
	case FP_PROTECT:
		if (self->client->ps.fd.forcePowerDebounce[forcePower] < level.time)
		{
			BG_ForcePowerDrain( &self->client->ps, forcePower, 1 );
			if (self->client->ps.fd.forcePower < 1)
			{
				WP_ForcePowerStop(self, forcePower);
			}

			self->client->ps.fd.forcePowerDebounce[forcePower] = level.time + 300;
		}
		break;
	case FP_ABSORB:
		if (self->client->ps.fd.forcePowerDebounce[forcePower] < level.time)
		{
			BG_ForcePowerDrain( &self->client->ps, forcePower, 1 );
			if (self->client->ps.fd.forcePower < 1)
			{
				WP_ForcePowerStop(self, forcePower);
			}

			self->client->ps.fd.forcePowerDebounce[forcePower] = level.time + 600;
		}
		break;
	default:
		break;
	}
}

int WP_DoSpecificPower( gentity_t *self, usercmd_t *ucmd, forcePowers_t forcepower)
{
	int powerSucceeded;

	powerSucceeded = 1;

    //OSP: pause
    if ( level.pause.state != PAUSE_NONE && self->client && !self->client->sess.inRacemode )
            return 0;

	// allow th/te binds to work for either one
	if (g_redirectWrongThTeBinds.integer) {
		if (forcepower == FP_TEAM_HEAL && self && self->client && !(self->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_HEAL)) && self->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_FORCE))
			forcepower = FP_TEAM_FORCE;
		else if (forcepower == FP_TEAM_FORCE && self && self->client && !(self->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_FORCE)) && self->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_HEAL))
			forcepower = FP_TEAM_HEAL;
	}

	// OVERRIDEFIXME
	qboolean meme = (!level.wasRestarted && forcepower == FP_GRIP && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
	if ( !WP_ForcePowerAvailable( self, forcepower, 0 ) && !(self->health <= 0 && forcepower == FP_SPEED && g_preActivateSpeedWhileDead.integer) && !meme )
	{
		return 0;
	}

	switch(forcepower)
	{
	case FP_HEAL:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceHeal(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_LEVITATION:
		//if leave the ground by some other means, cancel the force jump so we don't suddenly jump when we land.
		
		if ( self->client->ps.groundEntityNum == ENTITYNUM_NONE )
		{
			self->client->ps.fd.forceJumpCharge = 0;
			G_MuteSound( self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE );
			//This only happens if the groundEntityNum == ENTITYNUM_NONE when the button is actually released
		}
		else
		{//still on ground, so jump
			ForceJump( self, ucmd );
		}
		break;
	case FP_SPEED:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceSpeed(self, 0);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_GRIP:
		if (self->client->ps.fd.forceGripEntityNum == ENTITYNUM_NONE)
		{
			ForceGrip( self );
		}

		if (self->client->ps.fd.forceGripEntityNum != ENTITYNUM_NONE)
		{
			if (!(self->client->ps.fd.forcePowersActive & (1 << FP_GRIP)))
			{
				WP_ForcePowerStart( self, FP_GRIP, 0 );
				if (!meme) {
					BG_ForcePowerDrain(&self->client->ps, FP_GRIP, GRIP_DRAIN_AMOUNT);
					if (g_gripRework.integer == 2)
						self->client->ps.fd.forcePowerDebounce[FP_PULL] = level.time + 1000; // give a freebie second to use grip
				}
			}
		}
		else
		{
			powerSucceeded = 0;
		}
		break;
	case FP_LIGHTNING:
		ForceLightning(self);
		break;
	case FP_PUSH:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease && !(self->r.svFlags & SVF_BOT))
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceThrow(self, qfalse);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_PULL:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceThrow(self, qtrue);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_TELEPATHY:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceTelepathy(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_RAGE:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceRage(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_PROTECT:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceProtect(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_ABSORB:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceAbsorb(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_TEAM_HEAL:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceTeamHeal(self, qfalse);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_TEAM_FORCE:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceTeamForceReplenish(self, qfalse);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_DRAIN:
		ForceDrain(self);
		break;
	case FP_SEE:
		powerSucceeded = 0; //always 0 for nonhold powers
		if (self->client->ps.fd.forceButtonNeedRelease)
		{ //need to release before we can use nonhold powers again
			break;
		}
		ForceSeeing(self);
		self->client->ps.fd.forceButtonNeedRelease = 1;
		break;
	case FP_SABER_OFFENSE:
		break;
	case FP_SABER_DEFENSE:
		break;
	case FP_SABERTHROW:
		break;
	default:
		break;
	}

	return powerSucceeded;
}

void FindGenericEnemyIndex(gentity_t *self)
{ //Find another client that would be considered a threat.
	int i = 0;
	float tlen;
	gentity_t *ent;
	gentity_t *besten = NULL;
	float blen = 99999999;
	vec3_t a;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

        if ( ent && 
            ent->inuse &&
            ent->client &&
			!ent->client->sess.inRacemode &&
			!ent->isAimPracticePack &&
            (ent->s.number != self->s.number) && 
            (ent->health > 0) && 
            !OnSameTeam( self, ent ) && 
            (ent->client->ps.pm_type != PM_INTERMISSION) && 
            (ent->client->ps.pm_type != PM_SPECTATOR) && 
            !(ent->client->ps.pm_flags & PMF_FOLLOW) )
		{
			VectorSubtract(ent->client->ps.origin, self->client->ps.origin, a);
			tlen = VectorLength(a);

			if (tlen < blen &&
				InFront(ent->client->ps.origin, self->client->ps.origin, self->client->ps.viewangles, 0.8f ) &&
				OrgVisible(self->client->ps.origin, ent->client->ps.origin, self->s.number))
			{
				blen = tlen;
				besten = ent;
			}
		}

		i++;
	}

	if (!besten)
	{
		return;
	}

	self->client->ps.genericEnemyIndex = besten->s.number;
}

void SeekerDroneUpdate(gentity_t *self)
{
	vec3_t org, elevated, dir, a, endir;
	gentity_t *en;
	float angle;
	float prefig = 0;
	trace_t tr;

	if (!(self->client->ps.eFlags & EF_SEEKERDRONE))
	{
		self->client->ps.genericEnemyIndex = -1;
		return;
	}

	if (self->health < 1)
	{
		VectorCopy(self->client->ps.origin, elevated);
		elevated[2] += 40;

		angle = ((level.time / 12) & 255) * (M_PI * 2) / 255; //magical numbers make magic happen
		dir[0] = cos(angle) * 20;
		dir[1] = sin(angle) * 20;
		dir[2] = cos(angle) * 5;
		VectorAdd(elevated, dir, org);

		a[ROLL] = 0;
		a[YAW] = 0;
		a[PITCH] = 1;

		gentity_t *te = G_PlayEffect(EFFECT_SPARK_EXPLOSION, org, a);
		G_ApplyRaceBroadcastsToEvent( self, te );

		//ugly
		self->client->ps.eFlags &= ~EF_SEEKERDRONE;


		self->client->ps.genericEnemyIndex = -1;

		return;
	}

	if (self->client->ps.droneExistTime >= level.time && 
		self->client->ps.droneExistTime < (level.time+5000))
	{
		self->client->ps.genericEnemyIndex = 1024+self->client->ps.droneExistTime;
		if (self->client->ps.droneFireTime < level.time)
		{
			G_Sound( self, CHAN_BODY, G_SoundIndex("sound/weapons/laser_trap/warning.wav") );
			self->client->ps.droneFireTime = level.time + 100;
		}
		return;
	}
	else if (self->client->ps.droneExistTime < level.time)
	{
		VectorCopy(self->client->ps.origin, elevated);
		elevated[2] += 40;

		prefig = (self->client->ps.droneExistTime-level.time)/80;

		if (prefig > 55)
		{
			prefig = 55;
		}
		else if (prefig < 1)
		{
			prefig = 1;
		}

		elevated[2] -= 55-prefig;

		angle = ((level.time / 12) & 255) * (M_PI * 2) / 255; //magical numbers make magic happen
		dir[0] = cos(angle) * 20;
		dir[1] = sin(angle) * 20;
		dir[2] = cos(angle) * 5;
		VectorAdd(elevated, dir, org);

		a[ROLL] = 0;
		a[YAW] = 0;
		a[PITCH] = 1;

		gentity_t *te = G_PlayEffect(EFFECT_SPARK_EXPLOSION, org, a);
		G_ApplyRaceBroadcastsToEvent( self, te );

		//ugly
		self->client->ps.eFlags	&= ~EF_SEEKERDRONE;

		self->client->ps.genericEnemyIndex = -1;

		return;
	}

	if (self->client->ps.genericEnemyIndex == -1)
	{
		self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
	}

	if (self->client->ps.genericEnemyIndex != ENTITYNUM_NONE && self->client->ps.genericEnemyIndex != -1)
	{
		en = &g_entities[self->client->ps.genericEnemyIndex];

		if (!en || !en->client || !en->inuse)
		{
			self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
		}
		else if (en->s.number == self->s.number)
		{
			self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
		}
		else if (en->health < 1)
		{
			self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
		}
		else if (OnSameTeam(self, en))
		{
			self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
		}
		else
		{
			if (!InFront(en->client->ps.origin, self->client->ps.origin, self->client->ps.viewangles, 0.8f ))
			{
				self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
			}
			else if (!OrgVisible(self->client->ps.origin, en->client->ps.origin, self->s.number))
			{
				self->client->ps.genericEnemyIndex = ENTITYNUM_NONE;
			}
		}
	}

	if (self->client->ps.genericEnemyIndex == ENTITYNUM_NONE || self->client->ps.genericEnemyIndex == -1)
	{
		FindGenericEnemyIndex(self);
	}

	if (self->client->ps.genericEnemyIndex != ENTITYNUM_NONE && self->client->ps.genericEnemyIndex != -1)
	{
		en = &g_entities[self->client->ps.genericEnemyIndex];

		VectorCopy(self->client->ps.origin, elevated);
		elevated[2] += 40;

		angle = ((level.time / 12) & 255) * (M_PI * 2) / 255; //magical numbers make magic happen
		dir[0] = cos(angle) * 20;
		dir[1] = sin(angle) * 20;
		dir[2] = cos(angle) * 5;
		VectorAdd(elevated, dir, org);

		//org is now where the thing should be client-side because it uses the same time-based offset
		if (self->client->ps.droneFireTime < level.time)
		{
			trap_Trace(&tr, org, NULL, NULL, en->client->ps.origin, -1, MASK_SOLID);

			if (tr.fraction == 1 && !tr.startsolid && !tr.allsolid)
			{
				VectorSubtract(en->client->ps.origin, org, endir);
				VectorNormalize(endir);

				WP_FireGenericBlasterMissile(self, org, endir, 0, 15, 2000, MOD_BLASTER);
				G_SoundAtLoc( org, CHAN_WEAPON, G_SoundIndex("sound/weapons/bryar/fire.wav") );

				self->client->ps.droneFireTime = level.time + Q_irand(400, 700);
			}
		}
	}
}

void HolocronUpdate(gentity_t *self)
{ //keep holocron status updated in holocron mode
	int i = 0;
	int noHRank = 0;

	if (noHRank < FORCE_LEVEL_0)
	{
		noHRank = FORCE_LEVEL_0;
	}
	if (noHRank > FORCE_LEVEL_3)
	{
		noHRank = FORCE_LEVEL_3;
	}

	trap_Cvar_Update(&g_MaxHolocronCarry);

	while (i < NUM_FORCE_POWERS)
	{
		if (self->client->ps.holocronsCarried[i])
		{ //carrying it, make sure we have the power
			self->client->ps.holocronBits |= (1 << i);
			self->client->ps.fd.forcePowersKnown |= (1 << i);
			self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;
		}
		else
		{ //otherwise, make sure the power is cleared from us
			self->client->ps.fd.forcePowerLevel[i] = 0;
			if (self->client->ps.holocronBits & (1 << i))
			{
				self->client->ps.holocronBits -= (1 << i);
			}

			if ((self->client->ps.fd.forcePowersKnown & (1 << i)) && i != FP_LEVITATION && i != FP_SABER_OFFENSE)
			{
				self->client->ps.fd.forcePowersKnown -= (1 << i);
			}

			if ((self->client->ps.fd.forcePowersActive & (1 << i)) && i != FP_LEVITATION && i != FP_SABER_OFFENSE)
			{
				WP_ForcePowerStop(self, i);
			}

			if (i == FP_LEVITATION)
			{
				if (noHRank >= FORCE_LEVEL_1)
				{
					self->client->ps.fd.forcePowerLevel[i] = noHRank;
				}
				else
				{
					self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_1;
				}
			}
			else if (i == FP_SABER_OFFENSE)
			{
				self->client->ps.fd.forcePowersKnown |= (1 << i);

				if (noHRank >= FORCE_LEVEL_1)
				{
					self->client->ps.fd.forcePowerLevel[i] = noHRank;
				}
				else
				{
					self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_1;
				}
			}
			else
			{
				self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
			}
		}

		i++;
	}

	if (HasSetSaberOnly())
	{ //if saberonly, we get these powers no matter what (still need the holocrons for level 3)
		if (self->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] < FORCE_LEVEL_1)
		{
			self->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] = FORCE_LEVEL_1;
		}
		if (self->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] < FORCE_LEVEL_1)
		{
			self->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] = FORCE_LEVEL_1;
		}
	}
}

void JediMasterUpdate(gentity_t *self)
{ //keep jedi master status updated for JM gametype
	int i = 0;

	trap_Cvar_Update(&g_MaxHolocronCarry);

	while (i < NUM_FORCE_POWERS)
	{
		if (self->client->ps.isJediMaster)
		{
			self->client->ps.fd.forcePowersKnown |= (1 << i);
			self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;

			if (i == FP_TEAM_HEAL || i == FP_TEAM_FORCE ||
				i == FP_DRAIN || i == FP_ABSORB)
			{ //team powers are useless in JM, absorb is too because no one else has powers to absorb. Drain is just
			  //relatively useless in comparison, because its main intent is not to heal, but rather to cripple others
			  //by draining their force at the same time. And no one needs force in JM except the JM himself.
				self->client->ps.fd.forcePowersKnown &= ~(1 << i);
				self->client->ps.fd.forcePowerLevel[i] = 0;
			}

			if (i == FP_TELEPATHY)
			{ //this decision was made because level 3 mindtrick allows the JM to just hide too much, and no one else has force
			  //sight to counteract it. Since the JM himself is the focus of gameplay in this mode, having him hidden for large
			  //durations is indeed a bad thing.
				self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_2;
			}
		}
		else
		{
			if ((self->client->ps.fd.forcePowersKnown & (1 << i)) && i != FP_LEVITATION)
			{
				self->client->ps.fd.forcePowersKnown -= (1 << i);
			}

			if ((self->client->ps.fd.forcePowersActive & (1 << i)) && i != FP_LEVITATION)
			{
				WP_ForcePowerStop(self, i);
			}

			if (i == FP_LEVITATION)
			{
				self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_1;
			}
			else
			{
				self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
			}
		}

		i++;
	}
}

qboolean WP_HasForcePowers( const playerState_t *ps )
{
	int i;
	if ( ps )
	{
		for ( i = 0; i < NUM_FORCE_POWERS; i++ )
		{
			if ( i == FP_LEVITATION )
			{
				if ( ps->fd.forcePowerLevel[i] > FORCE_LEVEL_1 )
				{
					return qtrue;
				}
			}
			else if ( ps->fd.forcePowerLevel[i] > FORCE_LEVEL_0 )
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

//try a special roll getup move
qboolean G_SpecialRollGetup(gentity_t *self)
{ //fixme: currently no knockdown will actually land you on your front... so froll's are pretty useless at the moment.
	qboolean rolled = qfalse;

	if (
		self->client->pers.cmd.rightmove > 0 &&
		!self->client->pers.cmd.forwardmove)
	{
		G_SetAnim(self, &self->client->pers.cmd, SETANIM_BOTH, BOTH_GETUP_BROLL_R, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
		rolled = qtrue;
	}
	else if (
		self->client->pers.cmd.rightmove < 0 &&
		!self->client->pers.cmd.forwardmove)
	{
		G_SetAnim(self, &self->client->pers.cmd, SETANIM_BOTH, BOTH_GETUP_BROLL_L, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
		rolled = qtrue;
	}
	else if (
		!self->client->pers.cmd.rightmove &&
		self->client->pers.cmd.forwardmove > 0)
	{
		G_SetAnim(self, &self->client->pers.cmd, SETANIM_BOTH, BOTH_GETUP_BROLL_F, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
		rolled = qtrue;
	}
	else if (
		!self->client->pers.cmd.rightmove &&
		self->client->pers.cmd.forwardmove < 0)
	{
		G_SetAnim(self, &self->client->pers.cmd, SETANIM_BOTH, BOTH_GETUP_BROLL_B, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
		rolled = qtrue;
	}
	else if (self->client->pers.cmd.upmove)
	{
		G_PreDefSound(self, PDSOUND_FORCEJUMP);
		self->client->ps.forceDodgeAnim = 2;
		self->client->ps.forceHandExtendTime = level.time + 500;
	}

	if (rolled)
	{
		G_EntitySound( self, CHAN_VOICE, G_SoundIndex("*jump1.wav") );
	}

	return rolled;
}

void WP_ForcePowersUpdate( gentity_t *self, usercmd_t *ucmd )
{
	int			i, holo, holoregen;
	int			prepower = 0;
	//see if any force powers are running
	if ( !self )
	{
		return;
	}

	if ( !self->client )
	{
		return;
	}

	qboolean meme = (!level.wasRestarted && self && self->client && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));

	if (!meme && self->client->ps.pm_flags & PMF_FOLLOW)
	{ //not a "real" game client, it's a spectator following someone
		return;
	}
	if (!meme && self->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		return;
	}

	//The stance in relation to power level is no longer applicable with the crazy new akimbo/staff stances.
	if (!self->client->ps.fd.saberAnimLevel)
	{
		self->client->ps.fd.saberAnimLevel = FORCE_LEVEL_1;
	}

	if (g_gametype.integer != GT_SIEGE)
	{
		if (!(self->client->ps.fd.forcePowersKnown & (1 << FP_LEVITATION)))
		{
			self->client->ps.fd.forcePowersKnown |= (1 << FP_LEVITATION);
		}

		if (self->client->ps.fd.forcePowerLevel[FP_LEVITATION] < FORCE_LEVEL_1)
		{
			self->client->ps.fd.forcePowerLevel[FP_LEVITATION] = FORCE_LEVEL_1;
		}
	}

	if (self->client->ps.fd.forcePowerSelected < 0)
	{ //bad
		self->client->ps.fd.forcePowerSelected = 0;
	}

	if ( ((self->client->sess.selectedFP != self->client->ps.fd.forcePowerSelected) ||
		(self->client->sess.saberLevel != self->client->ps.fd.saberAnimLevel)) &&
		!(self->r.svFlags & SVF_BOT) )
	{
		if (self->client->sess.updateUITime < level.time)
		{ //a bit hackish, but we don't want the client to flood with userinfo updates if they rapidly cycle
		  //through their force powers or saber attack levels

			self->client->sess.selectedFP = self->client->ps.fd.forcePowerSelected;
			self->client->sess.saberLevel = self->client->ps.fd.saberAnimLevel;
		}
	}

	if (!g_LastFrameTime)
	{
		g_LastFrameTime = level.time;
	}

	if (self->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN)
	{
		self->client->ps.zoomFov = 0;
		self->client->ps.zoomMode = 0;
		self->client->ps.zoomLocked = qfalse;
		self->client->ps.zoomTime = 0;
	}

	if (self->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN &&
		self->client->ps.forceHandExtendTime >= level.time)
	{
		self->client->ps.saberMove = 0;
		self->client->ps.saberBlocking = 0;
		self->client->ps.saberBlocked = 0;
		self->client->ps.weaponTime = 0;
		self->client->ps.weaponstate = WEAPON_READY;
	}
	else if (self->client->ps.forceHandExtend != HANDEXTEND_NONE &&
		self->client->ps.forceHandExtendTime < level.time)
	{
		if (self->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN &&
			!self->client->ps.forceDodgeAnim)
		{
			if (self->health < 1 || (self->client->ps.eFlags & EF_DEAD))
			{
				self->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}
			else if (G_SpecialRollGetup(self))
			{
				self->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}
			else
			{ //hmm.. ok.. no more getting up on your own, you've gotta push something, unless..
				if ((level.time-self->client->ps.forceHandExtendTime) > 4000)
				{ //4 seconds elapsed, I guess they're too dumb to push something to get up!
					if (self->client->pers.cmd.upmove &&
						self->client->ps.fd.forcePowerLevel[FP_LEVITATION] > FORCE_LEVEL_1)
					{ //force getup
						G_PreDefSound(self, PDSOUND_FORCEJUMP);
						self->client->ps.forceDodgeAnim = 2;
						self->client->ps.forceHandExtendTime = level.time + 500;

					}
					else if (self->client->ps.quickerGetup)
					{
						G_EntitySound( self, CHAN_VOICE, G_SoundIndex("*jump1.wav") );
						self->client->ps.forceDodgeAnim = 3;
						self->client->ps.forceHandExtendTime = level.time + 500;
						self->client->ps.velocity[2] = 300;
					}
					else
					{
						self->client->ps.forceDodgeAnim = 1;
						self->client->ps.forceHandExtendTime = level.time + 1000;
					}
				}
			}
			self->client->ps.quickerGetup = qfalse;
		}
		else if (self->client->ps.forceHandExtend == HANDEXTEND_POSTTHROWN)
		{
			if (self->health < 1 || (self->client->ps.eFlags & EF_DEAD))
			{
				self->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}
			else if (self->client->ps.groundEntityNum != ENTITYNUM_NONE && !self->client->ps.forceDodgeAnim)
			{
				self->client->ps.forceDodgeAnim = 1;
				self->client->ps.forceHandExtendTime = level.time + 1000;
				G_EntitySound( self, CHAN_VOICE, G_SoundIndex("*jump1.wav") );
				self->client->ps.velocity[2] = 100;
			}
			else if (!self->client->ps.forceDodgeAnim)
			{
				self->client->ps.forceHandExtendTime = level.time + 100;
			}
			else
			{
				self->client->ps.forceHandExtend = HANDEXTEND_WEAPONREADY;
			}
		}
		else
		{
			self->client->ps.forceHandExtend = HANDEXTEND_WEAPONREADY;
		}
	}

	if (g_gametype.integer == GT_HOLOCRON)
	{
		HolocronUpdate(self);
	}
	if (g_gametype.integer == GT_JEDIMASTER)
	{
		JediMasterUpdate(self);
	}

	SeekerDroneUpdate(self);

	// removed it from here and move else where, so
	// boon will cost only half for everyone -sil

	if (self->client->ps.powerups[PW_FORCE_BOON]
		&& g_enableBoon.integer >= 2)
	{
		prepower = self->client->ps.fd.forcePower;
	}

	if (self && self->client && (BG_HasYsalamiri(g_gametype.integer, &self->client->ps) ||
		self->client->ps.fd.forceDeactivateAll || self->client->tempSpectate >= level.time))
	{ //has ysalamiri.. or we want to forcefully stop all his active powers
		i = 0;

		while (i < NUM_FORCE_POWERS)
		{
			if ((self->client->ps.fd.forcePowersActive & (1 << i)) && i != FP_LEVITATION)
			{
				WP_ForcePowerStop(self, i);
			}

			i++;
		}

		if (self->client->tempSpectate >= level.time)
		{
			self->client->ps.fd.forcePower = 100;
			self->client->ps.fd.forceRageRecoveryTime = 0;
		}

		self->client->ps.fd.forceDeactivateAll = 0;

		if (self->client->ps.fd.forceJumpCharge)
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);
			self->client->ps.fd.forceJumpCharge = 0;
		}
	}
	else
	{ //otherwise just do a check through them all to see if they need to be stopped for any reason.
		i = 0;

		while (i < NUM_FORCE_POWERS)
		{
			if ((self->client->ps.fd.forcePowersActive & (1 << i)) && i != FP_LEVITATION &&
				!BG_CanUseFPNow(g_gametype.integer, &self->client->ps, level.time, i))
			{
				WP_ForcePowerStop(self, i);
			}

			i++;
		}
	}

	i = 0;

	if (self->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] || self->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK])
	{ //enlightenment
		if (!self->client->ps.fd.forceUsingAdded)
		{
			i = 0;

			while (i < NUM_FORCE_POWERS)
			{
				self->client->ps.fd.forcePowerBaseLevel[i] = self->client->ps.fd.forcePowerLevel[i];

				if (!forcePowerDarkLight[i] ||
					self->client->ps.fd.forceSide == forcePowerDarkLight[i])
				{
					self->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;
					self->client->ps.fd.forcePowersKnown |= (1 << i);
				}

				i++;
			}

			self->client->ps.fd.forceUsingAdded = 1;
		}
	}
	else if (self->client->ps.fd.forceUsingAdded)
	{ //we don't have enlightenment but we're still using enlightened powers, so clear them back to how they should be.
		i = 0;

		while (i < NUM_FORCE_POWERS)
		{
			self->client->ps.fd.forcePowerLevel[i] = self->client->ps.fd.forcePowerBaseLevel[i];
			if (!self->client->ps.fd.forcePowerLevel[i])
			{
				if (self->client->ps.fd.forcePowersActive & (1 << i))
				{
					WP_ForcePowerStop(self, i);
				}
				self->client->ps.fd.forcePowersKnown &= ~(1 << i);
			}

			i++;
		}

		self->client->ps.fd.forceUsingAdded = 0;
	}

	i = 0;

	if (!(self->client->ps.fd.forcePowersActive & (1 << FP_TELEPATHY)))
	{ //clear the mindtrick index values
		self->client->ps.fd.forceMindtrickTargetIndex = 0;
		self->client->ps.fd.forceMindtrickTargetIndex2 = 0;
		self->client->ps.fd.forceMindtrickTargetIndex3 = 0;
		self->client->ps.fd.forceMindtrickTargetIndex4 = 0;
	}
	
	if (self->health < 1)
	{
		self->client->ps.fd.forceGripBeingGripped = 0;
	}

	if (self->client->ps.fd.forceGripBeingGripped > level.time)
	{
		self->client->ps.fd.forceGripCripple = 1;

		//keep the saber off during this period
		if (self->client->ps.weapon == WP_SABER && !self->client->ps.saberHolstered)
		{
			Cmd_ToggleSaber_f(self);
		}
	}
	else
	{
		self->client->ps.fd.forceGripCripple = 0;
	}

	if (self->client->ps.fd.forceJumpSound)
	{
		G_PreDefSound(self, PDSOUND_FORCEJUMP);
		self->client->ps.fd.forceJumpSound = 0;
	}

	if (self->client->ps.fd.forceGripCripple)
	{
		if (self->client->ps.fd.forceGripSoundTime < level.time)
		{
			G_PreDefSound(self, PDSOUND_FORCEGRIP);
			self->client->ps.fd.forceGripSoundTime = level.time + 1000;
		}
	}

	if (self->client->ps.fd.forcePowersActive & (1 << FP_SPEED))
	{
		self->client->ps.powerups[PW_SPEED] = level.time + 100;
	}

	if ( self->health <= 0 )
	{//if dead, deactivate any active force powers
		for ( i = 0; i < NUM_FORCE_POWERS; i++ )
		{
			if ( self->client->ps.fd.forcePowerDuration[i] || (self->client->ps.fd.forcePowersActive&( 1 << i )) )
			{
				WP_ForcePowerStop( self, (forcePowers_t)i );
				self->client->ps.fd.forcePowerDuration[i] = 0;
			}
		}
		goto powersetcheck;
	}

	if (self->client->ps.groundEntityNum != ENTITYNUM_NONE)
	{
		self->client->fjDidJump = qfalse;
	}

	if (self->client->ps.fd.forceJumpCharge && self->client->ps.groundEntityNum == ENTITYNUM_NONE && self->client->fjDidJump)
	{ //this was for the "charge" jump method... I guess
		if (ucmd->upmove < 10 && (!(ucmd->buttons & BUTTON_FORCEPOWER) || self->client->ps.fd.forcePowerSelected != FP_LEVITATION))
		{
			G_MuteSound(self->client->ps.fd.killSoundEntIndex[TRACK_CHANNEL_1-50], CHAN_VOICE);
			self->client->ps.fd.forceJumpCharge = 0;
		}
	}

#ifndef METROID_JUMP
	else if ( (ucmd->upmove > 10) && (self->client->ps.pm_flags & PMF_JUMP_HELD) && self->client->ps.groundTime && (level.time - self->client->ps.groundTime) > 150 && !BG_HasYsalamiri(g_gametype.integer, &self->client->ps) && BG_CanUseFPNow(g_gametype.integer, &self->client->ps, level.time, FP_LEVITATION) )
	{//just charging up
		ForceJumpCharge( self, ucmd );
		usingForce = qtrue;
	}
	else if (ucmd->upmove < 10 && self->client->ps.groundEntityNum == ENTITYNUM_NONE && self->client->ps.fd.forceJumpCharge)
	{
		self->client->ps.pm_flags &= ~(PMF_JUMP_HELD);
	}
#endif

	if (!(self->client->ps.pm_flags & PMF_JUMP_HELD) && self->client->ps.fd.forceJumpCharge)
	{
		if (!(ucmd->buttons & BUTTON_FORCEPOWER) ||
			self->client->ps.fd.forcePowerSelected != FP_LEVITATION)
		{
			if (WP_DoSpecificPower( self, ucmd, FP_LEVITATION ))
			{
			}
		}
	}

	if ( ucmd->buttons & BUTTON_FORCEGRIP )
	{ //grip is one of the powers with its own button.. if it's held, call the specific grip power function.
		if (WP_DoSpecificPower( self, ucmd, FP_GRIP ))
		{
		}
		else
		{ //don't let recharge even if the grip misses if the player still has the button down
		}
	}
	else
	{ //see if we're using it generically.. if not, stop.
		if (self->client->ps.fd.forcePowersActive & (1 << FP_GRIP))
		{
			if (!(ucmd->buttons & BUTTON_FORCEPOWER) || self->client->ps.fd.forcePowerSelected != FP_GRIP)
			{
				WP_ForcePowerStop(self, FP_GRIP);
			}
		}
	}

	if ( ucmd->buttons & BUTTON_FORCE_LIGHTNING )
	{ //lightning
		WP_DoSpecificPower(self, ucmd, FP_LIGHTNING);
	}
	else
	{ //see if we're using it generically.. if not, stop.
		if (self->client->ps.fd.forcePowersActive & (1 << FP_LIGHTNING))
		{
			if (!(ucmd->buttons & BUTTON_FORCEPOWER) || self->client->ps.fd.forcePowerSelected != FP_LIGHTNING)
			{
				WP_ForcePowerStop(self, FP_LIGHTNING);
			}
		}
	}

	if ( ucmd->buttons & BUTTON_FORCE_DRAIN )
	{ //drain
		WP_DoSpecificPower(self, ucmd, FP_DRAIN);
	}
	else
	{ //see if we're using it generically.. if not, stop.
		if (self->client->ps.fd.forcePowersActive & (1 << FP_DRAIN))
		{
			if (!(ucmd->buttons & BUTTON_FORCEPOWER) || self->client->ps.fd.forcePowerSelected != FP_DRAIN)
			{
				WP_ForcePowerStop(self, FP_DRAIN);
			}
		}
	}

	if ( (ucmd->buttons & BUTTON_FORCEPOWER) &&
		BG_CanUseFPNow(g_gametype.integer, &self->client->ps, level.time, self->client->ps.fd.forcePowerSelected))
	{
		if (self->client->ps.fd.forcePowerSelected == FP_LEVITATION)
		{
			ForceJumpCharge( self, ucmd );
		}
		else if (WP_DoSpecificPower( self, ucmd, self->client->ps.fd.forcePowerSelected ))
		{
		}
		else if (self->client->ps.fd.forcePowerSelected == FP_GRIP)
		{
		}
	}
	else
	{
		self->client->ps.fd.forceButtonNeedRelease = 0;
	}

	for ( i = 0; i < NUM_FORCE_POWERS; i++ )
	{
		if ( self->client->ps.fd.forcePowerDuration[i] )
		{
			if (g_fixForceTimers.integer) {
				if (self->client->ps.fd.forcePowerDuration[i] <= level.time && !(i == FP_SPEED && self->client->sess.inRacemode))
				{
					if ((self->client->ps.fd.forcePowersActive & (1 << i)))
					{//turn it off
						WP_ForcePowerStop(self, (forcePowers_t)i);
					}
					self->client->ps.fd.forcePowerDuration[i] = 0;
				}
			}
			else {
				if (self->client->ps.fd.forcePowerDuration[i] < level.time && !(i == FP_SPEED && self->client->sess.inRacemode))
				{
					if ((self->client->ps.fd.forcePowersActive & (1 << i)))
					{//turn it off
						WP_ForcePowerStop(self, (forcePowers_t)i);
					}
					self->client->ps.fd.forcePowerDuration[i] = 0;
				}
			}
		}
		if ( (self->client->ps.fd.forcePowersActive&( 1 << i )) )
		{
			//usingForce = qtrue;
			WP_ForcePowerRun( self, (forcePowers_t)i, ucmd );
		}
	}
	if ( self->client->ps.saberInFlight && self->client->ps.saberEntityNum )
	{//don't regen force power while throwing saber
		if ( self->client->ps.saberEntityNum < ENTITYNUM_NONE && self->client->ps.saberEntityNum > 0 )//player is 0
		{//
			if ( &g_entities[self->client->ps.saberEntityNum] != NULL && g_entities[self->client->ps.saberEntityNum].s.pos.trType == TR_LINEAR )
			{//fell to the ground and we're trying to pull it back
			}
		}
	}
	qboolean meme2 = (!level.wasRestarted && self && self->client && self->client->ps.fd.forcePowersActive == (1 << FP_GRIP) && self->client->account && (!Q_stricmp(self->client->account->name, "duo") || !Q_stricmp(self->client->account->name, "alpha")));
	if ( !self->client->ps.fd.forcePowersActive || self->client->ps.fd.forcePowersActive == (1 << FP_DRAIN) || meme2)
	{//when not using the force, regenerate at 1 point per half second
		if ( !self->client->ps.saberInFlight && self->client->ps.fd.forcePowerRegenDebounceTime < level.time &&
			(self->client->ps.weapon != WP_SABER || !BG_SaberInSpecial(self->client->ps.saberMove)) )
		{
			if (g_gametype.integer != GT_HOLOCRON || g_MaxHolocronCarry.value)
			{
				//if (!g_trueJedi.integer || self->client->ps.weapon == WP_SABER)
				//let non-jedi force regen since we're doing a more strict jedi/non-jedi thing... this gives dark jedi something to drain
				{
					if (self->client->ps.powerups[PW_FORCE_BOON])
					{
						WP_ForcePowerRegenerate( self, 6 );
					}
					else if (self->client->ps.isJediMaster && g_gametype.integer == GT_JEDIMASTER)
					{
						WP_ForcePowerRegenerate( self, 4 ); //jedi master regenerates 4 times as fast
					}
					else
					{
						WP_ForcePowerRegenerate( self, 0 );
					}
				}
			}
			else
			{ //regenerate based on the number of holocrons carried
				holoregen = 0;
				holo = 0;
				while (holo < NUM_FORCE_POWERS)
				{
					if (self->client->ps.holocronsCarried[holo])
					{
						holoregen++;
					}
					holo++;
				}

				WP_ForcePowerRegenerate(self, holoregen);
			}

			if (g_gametype.integer == GT_SIEGE)
			{
				if (self->client->holdingObjectiveItem &&
					g_entities[self->client->holdingObjectiveItem].inuse &&
					g_entities[self->client->holdingObjectiveItem].genericValue15)
				{ //1 point per 7 seconds.. super slow
					self->client->ps.fd.forcePowerRegenDebounceTime = level.time + 7000;
				}
				else if (self->client->siegeClass != -1 &&
					(bgSiegeClasses[self->client->siegeClass].classflags & (1<<CFL_FASTFORCEREGEN)))
				{ //if this is siege and our player class has the fast force regen ability, then recharge with 1/5th the usual delay
					self->client->ps.fd.forcePowerRegenDebounceTime = level.time + (g_forceRegenTime.integer*0.2);
				}
				else
				{
					self->client->ps.fd.forcePowerRegenDebounceTime = level.time + g_forceRegenTime.integer;
				}
			}
			else
			{
				if ( g_gametype.integer == GT_POWERDUEL && self->client->sess.duelTeam == DUELTEAM_LONE )
				{
					if ( g_duel_fraglimit.integer )
					{
						self->client->ps.fd.forcePowerRegenDebounceTime = level.time + (g_forceRegenTime.integer*
							(0.6 + (.3 * (float)self->client->sess.wins / (float)g_duel_fraglimit.integer)));
					}
					else
					{
						self->client->ps.fd.forcePowerRegenDebounceTime = level.time + (g_forceRegenTime.integer*0.7);
					}
				}
				else
				{
					self->client->ps.fd.forcePowerRegenDebounceTime = level.time + g_forceRegenTime.integer;
				}
			}
		}
	}

powersetcheck:

	if (prepower && self->client->ps.fd.forcePower < prepower)
	{
		int dif = ((prepower - self->client->ps.fd.forcePower)/2);
		if (dif < 1)
		{
			dif = 1;
		}

		self->client->ps.fd.forcePower = (prepower-dif);
	}
}

qboolean Jedi_DodgeEvasion( gentity_t *self, gentity_t *shooter, trace_t *tr, int hitLoc )
{
	if (self->isAimPracticePack)
		return qfalse;
	int	dodgeAnim = -1;
	qboolean noWeakPoints = qfalse;

	if ( !self || !self->client || self->health <= 0 )
	{
		return qfalse;
	}

	if (!g_forceDodge.integer)
	{
		return qfalse;
	}

	if (g_forceDodge.integer != 2)
	{
		if (!(self->client->ps.fd.forcePowersActive & (1 << FP_SEE)))
		{
			return qfalse;
		}
	}

	if ( self->client->ps.groundEntityNum == ENTITYNUM_NONE )
	{//can't dodge in mid-air
		return qfalse;
	}

	if ( self->client->ps.weaponTime > 0 || self->client->ps.forceHandExtend != HANDEXTEND_NONE )
	{//in some effect that stops me from moving on my own
		return qfalse;
	}

	if (g_forceDodge.integer == 2)
	{
		if (self->client->ps.fd.forcePowersActive)
		{ //for now just don't let us dodge if we're using a force power at all
			return qfalse;
		}
	}

	if (g_forceDodge.integer == 2)
	{
		if ( !WP_ForcePowerUsable( self, FP_SPEED ) )
		{//make sure we have it and have enough force power
			return qfalse;
		}
	}

	if (g_forceDodge.integer == 2)
	{
		if ( Q_irand( 1, 7 ) > self->client->ps.fd.forcePowerLevel[FP_SPEED] )
		{//more likely to fail on lower force speed level
			return qfalse;
		}
	}
	else
	{
		//We now dodge all the time, but only on level 3
		if (self->client->ps.fd.forcePowerLevel[FP_SEE] < FORCE_LEVEL_3)
		{//more likely to fail on lower force sight level
			return qfalse;
		}
	}

	if ( g_balanceSeeing.integer && self->client->ps.weapon == WP_SABER && self->client->ps.pm_flags & PMF_DUCKED ) {
		// if we have a saber and are crouching, don't have a single weak point
		noWeakPoints = qtrue;
	}

	#define DodgeOrReturn( a )	\
	do {						\
		if ( noWeakPoints ) {	\
			dodgeAnim = a;		\
			break;				\
		} else {				\
			return qfalse;		\
		}						\
	} while ( 0 )

	switch( hitLoc )
	{
	case HL_NONE:
		return noWeakPoints;
		break;

	case HL_FOOT_RT:
		DodgeOrReturn( BOTH_DODGE_FL );
	case HL_FOOT_LT:
		DodgeOrReturn( BOTH_DODGE_FR );
	case HL_LEG_RT:
		DodgeOrReturn( BOTH_DODGE_FL );
	case HL_LEG_LT:
		DodgeOrReturn( BOTH_DODGE_FR );
	case HL_BACK_RT:
		dodgeAnim = BOTH_DODGE_FL;
		break;
	case HL_CHEST_RT:
		dodgeAnim = BOTH_DODGE_FR;
		break;
	case HL_BACK_LT:
		dodgeAnim = BOTH_DODGE_FR;
		break;
	case HL_CHEST_LT:
		dodgeAnim = BOTH_DODGE_FR;
		break;
	case HL_BACK:
	case HL_CHEST:
	case HL_WAIST:
		dodgeAnim = BOTH_DODGE_FL;
		break;
	case HL_ARM_RT:
	case HL_HAND_RT:
		dodgeAnim = BOTH_DODGE_L;
		break;
	case HL_ARM_LT:
	case HL_HAND_LT:
		dodgeAnim = BOTH_DODGE_R;
		break;
	case HL_HEAD:
		dodgeAnim = BOTH_DODGE_FL;
		break;
	default:
		return noWeakPoints;
	}

	if ( dodgeAnim != -1 )
	{
		//Our own happy way of forcing an anim:
		self->client->ps.forceHandExtend = HANDEXTEND_DODGE;
		self->client->ps.forceDodgeAnim = dodgeAnim;
		self->client->ps.forceHandExtendTime = level.time + 300;

		self->client->ps.powerups[PW_SPEEDBURST] = level.time + 100;

		if (g_forceDodge.integer == 2)
		{
			ForceSpeed( self, 500 );
		}
		else
		{
			G_Sound( self, CHAN_BODY, G_SoundIndex("sound/weapons/force/speed.wav") );
		}
		return qtrue;
	}
	return noWeakPoints;
}
