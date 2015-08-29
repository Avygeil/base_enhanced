// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"
#include "g_ICARUScb.h"
#include "g_nav.h"
#include "bg_saga.h"

//#include "accounts.h"
#include "jp_engine.h"
#include "g_database_log.h"
#include "g_database_config.h"

level_locals_t	level;

static char* getCurrentTime();

int		eventClearTime = 0;
static int navCalcPathTime = 0;
extern int fatalErrors;


int killPlayerTimer = 0;

typedef struct {
	vmCvar_t	*vmCvar;
	char		*cvarName;
	char		*defaultString;
	int			cvarFlags;
	int			modificationCount;  // for tracking changes
	qboolean	trackChange;	    // track this variable, and announce if changed
  qboolean teamShader;        // track and if changed, update shader state
} cvarTable_t;

gentity_t		g_entities[MAX_GENTITIES];
gclient_t		g_clients[MAX_CLIENTS];

qboolean gDuelExit = qfalse;

vmCvar_t	g_trueJedi;

vmCvar_t	g_wasRestarted;

vmCvar_t	g_gametype;
vmCvar_t	g_MaxHolocronCarry;
vmCvar_t	g_ff_objectives;
vmCvar_t	g_autoMapCycle;
vmCvar_t	g_dmflags;
vmCvar_t	g_maxForceRank;
vmCvar_t	g_forceBasedTeams;
vmCvar_t	g_privateDuel;

vmCvar_t	g_duelLifes;
vmCvar_t	g_duelShields;

vmCvar_t	g_chatLimit;


vmCvar_t	g_allowNPC;

vmCvar_t	g_armBreakage;

vmCvar_t	g_saberLocking;
vmCvar_t	g_saberLockFactor;
vmCvar_t	g_saberTraceSaberFirst;

vmCvar_t	d_saberKickTweak;

vmCvar_t	d_powerDuelPrint;

vmCvar_t	d_saberGhoul2Collision;
vmCvar_t	g_saberBladeFaces;
vmCvar_t	d_saberAlwaysBoxTrace;
vmCvar_t	d_saberBoxTraceSize;

vmCvar_t	d_siegeSeekerNPC;

vmCvar_t	g_debugMelee;
vmCvar_t	g_stepSlideFix;

vmCvar_t	g_noSpecMove;

#ifdef _DEBUG
vmCvar_t	g_disableServerG2;
#endif

vmCvar_t	d_perPlayerGhoul2;

vmCvar_t	d_projectileGhoul2Collision;

vmCvar_t	g_g2TraceLod;

vmCvar_t	g_optvehtrace;

vmCvar_t	g_locationBasedDamage;

vmCvar_t	g_allowHighPingDuelist;

vmCvar_t	g_logClientInfo;

vmCvar_t	g_slowmoDuelEnd;

vmCvar_t	g_saberDamageScale;

vmCvar_t	g_useWhileThrowing;

vmCvar_t	g_RMG;

vmCvar_t	g_svfps;

vmCvar_t	g_forceRegenTime;
vmCvar_t	g_spawnInvulnerability;
vmCvar_t	g_forcePowerDisable;
vmCvar_t	g_weaponDisable;
vmCvar_t	g_duelWeaponDisable;
vmCvar_t	g_allowDuelSuicide;
vmCvar_t	g_fraglimitVoteCorrection;
vmCvar_t	g_fraglimit;
vmCvar_t	g_duel_fraglimit;
vmCvar_t	g_timelimit;
vmCvar_t	g_capturelimit;
vmCvar_t	g_capturedifflimit;
vmCvar_t	d_saberInterpolate;
vmCvar_t	g_friendlyFire;
vmCvar_t	g_friendlySaber;
vmCvar_t	g_password;
vmCvar_t	sv_privatepassword;
vmCvar_t	g_needpass;
vmCvar_t	g_maxclients;
vmCvar_t	g_maxGameClients;
vmCvar_t	g_dedicated;
vmCvar_t	g_developer;
vmCvar_t	g_speed;
vmCvar_t	g_gravity;
vmCvar_t	g_cheats;
vmCvar_t	g_knockback;
vmCvar_t	g_forcerespawn;
vmCvar_t	g_siegeRespawn;
vmCvar_t	g_inactivity;
vmCvar_t	g_inactivityKick;
vmCvar_t	g_spectatorInactivity;
vmCvar_t	g_debugMove;
vmCvar_t	g_accounts;
vmCvar_t	g_accountsFile; 
vmCvar_t	g_whitelist;
vmCvar_t	g_dlURL;   
vmCvar_t	g_logrcon;   
vmCvar_t	g_flags_overboarding;
vmCvar_t	g_selfkill_penalty;
vmCvar_t	g_fixNodropDetpacks;

#ifndef FINAL_BUILD
vmCvar_t	g_debugDamage;
#endif
vmCvar_t	g_debugAlloc;
vmCvar_t	g_debugServerSkel;
vmCvar_t	g_weaponRespawn;
vmCvar_t	g_weaponTeamRespawn;
vmCvar_t	g_adaptRespawn;
vmCvar_t	g_motd;
vmCvar_t	g_synchronousClients;
vmCvar_t	g_warmup;
vmCvar_t	g_doWarmup;
vmCvar_t	g_restarted;
vmCvar_t	g_log;
vmCvar_t	g_logSync;
vmCvar_t	g_statLog;
vmCvar_t	g_statLogFile;
vmCvar_t	g_blood;
vmCvar_t	g_podiumDist;
vmCvar_t	g_podiumDrop;
vmCvar_t	g_allowVote;
vmCvar_t	g_teamAutoJoin;
vmCvar_t	g_teamForceBalance;
vmCvar_t	g_banIPs;
vmCvar_t    g_getstatusbanIPs;
vmCvar_t	g_filterBan;
vmCvar_t	g_debugForward;
vmCvar_t	g_debugRight;
vmCvar_t	g_debugUp;
vmCvar_t	g_smoothClients;
vmCvar_t	g_defaultBanHoursDuration;
vmCvar_t	g_floatingItems;
vmCvar_t	g_rocketSurfing;

#include "namespace_begin.h"
vmCvar_t	pmove_fixed;
vmCvar_t	pmove_msec;
#include "namespace_end.h"

vmCvar_t	g_listEntity;
vmCvar_t	g_singlePlayer;
vmCvar_t	g_enableBreath;
vmCvar_t	g_dismember;
vmCvar_t	g_forceDodge;
vmCvar_t	g_timeouttospec;

vmCvar_t	g_saberDmgVelocityScale;
vmCvar_t	g_saberDmgDelay_Idle;
vmCvar_t	g_saberDmgDelay_Wound;

vmCvar_t	g_saberDebugPrint;

vmCvar_t	g_siegeTeamSwitch;

vmCvar_t	bg_fighterAltControl;

#ifdef DEBUG_SABER_BOX
vmCvar_t	g_saberDebugBox;
#endif

//NPC nav debug
vmCvar_t	d_altRoutes;
vmCvar_t	d_patched;

vmCvar_t		g_saberRealisticCombat;
vmCvar_t		g_saberRestrictForce;
vmCvar_t		d_saberSPStyleDamage;
vmCvar_t		g_debugSaberLocks;
vmCvar_t		g_saberLockRandomNess;
// nmckenzie: SABER_DAMAGE_WALLS
vmCvar_t		g_saberWallDamageScale;

vmCvar_t		d_saberStanceDebug;
// ai debug cvars
vmCvar_t		debugNPCAI;			// used to print out debug info about the bot AI
vmCvar_t		debugNPCFreeze;		// set to disable bot ai and temporarily freeze them in place
vmCvar_t		debugNPCAimingBeam;
vmCvar_t		debugBreak;
vmCvar_t		debugNoRoam;
vmCvar_t		d_saberCombat;
vmCvar_t		d_JediAI;
vmCvar_t		d_noGroupAI;
vmCvar_t		d_asynchronousGroupAI;
vmCvar_t		d_slowmodeath;
vmCvar_t		d_noIntermissionWait;

vmCvar_t		g_spskill;


vmCvar_t		g_siegeTeam1;
vmCvar_t		g_siegeTeam2;

vmCvar_t	g_austrian;

vmCvar_t	g_powerDuelStartHealth;
vmCvar_t	g_powerDuelEndHealth;

/*

Enhanced mod server cvars

*/

vmCvar_t	g_cleverFakeDetection;
vmCvar_t	g_protectQ3Fill;
vmCvar_t	g_protectQ3FillIPLimit;
vmCvar_t	g_protectHPhack;
vmCvar_t	g_maxIPConnected;
vmCvar_t	g_protectCallvoteHack;
vmCvar_t    g_minimumVotesCount;
vmCvar_t    g_fixPitKills;

vmCvar_t    g_enforceEvenVotersCount;
vmCvar_t    g_minVotersForEvenVotersCount;

vmCvar_t    bot_minping;
vmCvar_t    bot_maxping;
vmCvar_t    bot_ping_sparsity;

vmCvar_t     g_strafejump_mod;

//allowing/disabling vote types
vmCvar_t    g_allow_vote_gametype;
vmCvar_t    g_allow_vote_kick;
vmCvar_t    g_allow_vote_restart;
vmCvar_t    g_allow_vote_map;
vmCvar_t    g_allow_vote_nextmap;
vmCvar_t    g_allow_vote_timelimit;
vmCvar_t    g_allow_vote_fraglimit;
vmCvar_t    g_allow_vote_maprandom;
vmCvar_t    g_allow_vote_warmup;
vmCvar_t    g_quietrcon;
vmCvar_t    g_npc_spawn_limit;
vmCvar_t	g_hackLog;

vmCvar_t    g_default_restart_countdown;

vmCvar_t    g_fixboon;
vmCvar_t    g_maxstatusrequests;
vmCvar_t	g_testdebug; //for tmp debug
vmCvar_t	g_rconpassword;

vmCvar_t	g_callvotedelay;
vmCvar_t	g_callvotemaplimit;

vmCvar_t	sv_privateclients;

// nmckenzie: temporary way to show player healths in duels - some iface gfx in game would be better, of course.
// DUEL_HEALTH
vmCvar_t		g_showDuelHealths;

// bk001129 - made static to avoid aliasing
static cvarTable_t		gameCvarTable[] = {
	// don't override the cheat state set by the system
	{ &g_cheats, "sv_cheats", "", 0, 0, qfalse },

	{ &g_debugMelee, "g_debugMelee", "0", CVAR_SERVERINFO, 0, qtrue  },
	{ &g_stepSlideFix, "g_stepSlideFix", "1", CVAR_SERVERINFO, 0, qtrue  },

	{ &g_noSpecMove, "g_noSpecMove", "0", CVAR_SERVERINFO, 0, qtrue },

	// noset vars
	{ NULL, "gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },
	{ NULL, "gamedate", __DATE__ , CVAR_ROM, 0, qfalse  },
	//TODO: autogenerate gameversion
	{ NULL, "gameversion", "1.0" , CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },
	{ &g_restarted, "g_restarted", "0", CVAR_ROM, 0, qfalse  },
	{ NULL, "sv_mapname", "", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },

	{ &g_wasRestarted, "g_wasRestarted", "0", CVAR_ROM, 0, qfalse  },

	// latched vars
	{ &g_gametype, "g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH, 0, qfalse  },
	{ &g_MaxHolocronCarry, "g_MaxHolocronCarry", "3", CVAR_SERVERINFO | CVAR_LATCH, 0, qfalse  },

    { &g_maxclients, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qfalse },
	{ &g_maxGameClients, "g_maxGameClients", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qfalse  },

	{ &g_trueJedi, "g_jediVmerc", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qtrue },

	// change anytime vars
	{ &g_ff_objectives, "g_ff_objectives", "0", /*CVAR_SERVERINFO |*/ CVAR_CHEAT | CVAR_NORESTART, 0, qtrue },

	{ &g_autoMapCycle, "g_autoMapCycle", "0", CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
	{ &g_dmflags, "dmflags", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },
	
	{ &g_maxForceRank, "g_maxForceRank", "6", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse  },
	{ &g_forceBasedTeams, "g_forceBasedTeams", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse  },
	{ &g_privateDuel, "g_privateDuel", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },
	
	{ &g_duelLifes, "g_duelLifes", "100", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },
	{ &g_duelShields, "g_duelShields", "25", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },

	{ &g_chatLimit, "g_chatLimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },

	{ &g_allowNPC, "g_allowNPC", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },

	{ &g_armBreakage, "g_armBreakage", "0", 0, 0, qtrue  },

	{ &g_saberLocking, "g_saberLocking", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },
	{ &g_saberLockFactor, "g_saberLockFactor", "2", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_saberTraceSaberFirst, "g_saberTraceSaberFirst", "0", CVAR_ARCHIVE, 0, qtrue  },

	{ &d_saberKickTweak, "d_saberKickTweak", "1", 0, 0, qtrue  },

    { &d_powerDuelPrint, "d_powerDuelPrint", "0", 0, qtrue },

    { &d_saberGhoul2Collision, "d_saberGhoul2Collision", "1", CVAR_CHEAT, 0, qtrue },
    { &g_saberBladeFaces, "g_saberBladeFaces", "1", 0, 0, qtrue },

    { &d_saberAlwaysBoxTrace, "d_saberAlwaysBoxTrace", "0", CVAR_CHEAT, 0, qtrue },
    { &d_saberBoxTraceSize, "d_saberBoxTraceSize", "0", CVAR_CHEAT, 0, qtrue },

    { &d_siegeSeekerNPC, "d_siegeSeekerNPC", "0", CVAR_CHEAT, 0, qtrue },

#ifdef _DEBUG
    { &g_disableServerG2, "g_disableServerG2", "0", 0, 0, qtrue },
#endif

    { &d_perPlayerGhoul2, "d_perPlayerGhoul2", "0", CVAR_CHEAT, 0, qtrue },

    { &d_projectileGhoul2Collision, "d_projectileGhoul2Collision", "1", CVAR_CHEAT, 0, qtrue },

    { &g_g2TraceLod, "g_g2TraceLod", "3", 0, 0, qtrue },

    { &g_optvehtrace, "com_optvehtrace", "0", 0, 0, qtrue },

    { &g_locationBasedDamage, "g_locationBasedDamage", "1", 0, 0, qtrue },

    { &g_allowHighPingDuelist, "g_allowHighPingDuelist", "1", 0, 0, qtrue },

    { &g_logClientInfo, "g_logClientInfo", "0", CVAR_ARCHIVE, 0, qtrue },

    { &g_slowmoDuelEnd, "g_slowmoDuelEnd", "0", CVAR_ARCHIVE, 0, qtrue },

    { &g_saberDamageScale, "g_saberDamageScale", "1", CVAR_ARCHIVE, 0, qtrue },

    { &g_useWhileThrowing, "g_useWhileThrowing", "1", 0, 0, qtrue },

    { &g_RMG, "RMG", "0", 0, 0, qtrue },

    { &g_svfps, "sv_fps", "20", CVAR_SERVERINFO, 0, qtrue },

    { &g_forceRegenTime, "g_forceRegenTime", "200", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue },

    { &g_spawnInvulnerability, "g_spawnInvulnerability", "3000", CVAR_ARCHIVE, 0, qtrue },

    { &g_forcePowerDisable, "g_forcePowerDisable", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
    { &g_weaponDisable, "g_weaponDisable", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
    { &g_duelWeaponDisable, "g_duelWeaponDisable", "1", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

    { &g_allowDuelSuicide, "g_allowDuelSuicide", "1", CVAR_ARCHIVE, 0, qtrue },

    { &g_fraglimitVoteCorrection, "g_fraglimitVoteCorrection", "1", CVAR_ARCHIVE, 0, qtrue },

    { &g_fraglimit, "fraglimit", "20", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
    { &g_duel_fraglimit, "duel_fraglimit", "10", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
    { &g_timelimit, "timelimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
    { &g_capturelimit, "capturelimit", "8", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
    { &g_capturedifflimit, "capturedifflimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },

    { &g_synchronousClients, "g_synchronousClients", "0", CVAR_SYSTEMINFO, 0, qfalse },

    { &d_saberInterpolate, "d_saberInterpolate", "0", CVAR_CHEAT, 0, qtrue },

    { &g_friendlyFire, "g_friendlyFire", "0", CVAR_ARCHIVE, 0, qtrue },
    { &g_friendlySaber, "g_friendlySaber", "0", CVAR_ARCHIVE, 0, qtrue },

    { &g_teamAutoJoin, "g_teamAutoJoin", "0", CVAR_ARCHIVE },
    { &g_teamForceBalance, "g_teamForceBalance", "0", CVAR_ARCHIVE },

    { &g_warmup, "g_warmup", "20", CVAR_ARCHIVE, 0, qtrue },
    { &g_doWarmup, "g_doWarmup", "0", 0, 0, qtrue },
    { &g_log, "g_log", "games.log", CVAR_ARCHIVE, 0, qfalse },
    { &g_logSync, "g_logSync", "0", CVAR_ARCHIVE, 0, qfalse },

    { &g_statLog, "g_statLog", "0", CVAR_ARCHIVE, 0, qfalse },
    { &g_statLogFile, "g_statLogFile", "statlog.log", CVAR_ARCHIVE, 0, qfalse },

    { &g_password, "g_password", "", CVAR_USERINFO, 0, qfalse },
    { &sv_privatepassword, "sv_privatepassword", "", CVAR_TEMP, 0, qfalse },


    { &g_banIPs, "g_banIPs", "", CVAR_ARCHIVE, 0, qfalse },
    { &g_getstatusbanIPs, "g_getstatusbanIPs", "", CVAR_ARCHIVE, 0, qfalse },

    { &g_filterBan, "g_filterBan", "1", CVAR_ARCHIVE, 0, qfalse },

    { &g_needpass, "g_needpass", "0", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse },

    { &g_dedicated, "dedicated", "0", 0, 0, qfalse },

    { &g_developer, "developer", "0", 0, 0, qfalse },

    { &g_speed, "g_speed", "250", CVAR_SERVERINFO, 0, qtrue },
    { &g_gravity, "g_gravity", "800", CVAR_SERVERINFO, 0, qtrue },
    { &g_knockback, "g_knockback", "1000", 0, 0, qtrue },
    { &g_weaponRespawn, "g_weaponrespawn", "5", 0, 0, qtrue },
    { &g_weaponTeamRespawn, "g_weaponTeamRespawn", "5", 0, 0, qtrue },
    { &g_adaptRespawn, "g_adaptrespawn", "1", 0, 0, qtrue },		// Make weapons respawn faster with a lot of players.
    { &g_forcerespawn, "g_forcerespawn", "60", 0, 0, qtrue },		// One minute force respawn.  Give a player enough time to reallocate force.
    { &g_siegeRespawn, "g_siegeRespawn", "20", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue }, //siege respawn wave time
    { &g_inactivity, "g_inactivity", "0", 0, 0, qtrue },
    { &g_inactivityKick, "g_inactivityKick", "1", 0, 0, qtrue },
    { &g_spectatorInactivity, "g_spectatorInactivity", "0", 0, 0, qtrue },
	{ &g_debugMove, "g_debugMove", "0", 0, 0, qfalse },

	
#ifndef FINAL_BUILD
	{ &g_debugDamage, "g_debugDamage", "0", 0, 0, qfalse },
#endif
	{ &g_debugAlloc, "g_debugAlloc", "0", 0, 0, qfalse },
	{ &g_debugServerSkel, "g_debugServerSkel", "0", CVAR_CHEAT, 0, qfalse },
	{ &g_motd, "g_motd", "", 0, 0, qfalse },
	{ &g_blood, "com_blood", "1", 0, 0, qfalse },

	{ &g_podiumDist, "g_podiumDist", "80", 0, 0, qfalse },
	{ &g_podiumDrop, "g_podiumDrop", "70", 0, 0, qfalse },

	{ &g_allowVote, "g_allowVote", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_listEntity, "g_listEntity", "0", 0, 0, qfalse },

#if 0
	{ &g_debugForward, "g_debugForward", "0", 0, 0, qfalse },
	{ &g_debugRight, "g_debugRight", "0", 0, 0, qfalse },
	{ &g_debugUp, "g_debugUp", "0", 0, 0, qfalse },
#endif

	{ &g_singlePlayer, "ui_singlePlayerActive", "", 0, 0, qfalse, qfalse  },

	{ &g_enableBreath, "g_enableBreath", "0", 0, 0, qtrue, qfalse },
	{ &g_smoothClients, "g_smoothClients", "1", 0, 0, qfalse},
	{ &pmove_fixed, "pmove_fixed", "0", CVAR_SYSTEMINFO, 0, qfalse},
	{ &pmove_msec, "pmove_msec", "8", CVAR_SYSTEMINFO, 0, qfalse},

	{ &g_dismember, "g_dismember", "0", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_forceDodge, "g_forceDodge", "1", 0, 0, qtrue  },

	{ &g_timeouttospec, "g_timeouttospec", "70", CVAR_ARCHIVE, 0, qfalse },

	{ &g_saberDmgVelocityScale, "g_saberDmgVelocityScale", "0", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_saberDmgDelay_Idle, "g_saberDmgDelay_Idle", "350", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_saberDmgDelay_Wound, "g_saberDmgDelay_Wound", "0", CVAR_ARCHIVE, 0, qtrue  },

#ifndef FINAL_BUILD
	{ &g_saberDebugPrint, "g_saberDebugPrint", "0", CVAR_CHEAT, 0, qfalse  },
#endif
	{ &g_debugSaberLocks, "g_debugSaberLocks", "0", CVAR_CHEAT, 0, qfalse },
	{ &g_saberLockRandomNess, "g_saberLockRandomNess", "2", CVAR_CHEAT, 0, qfalse },
// nmckenzie: SABER_DAMAGE_WALLS
	{ &g_saberWallDamageScale, "g_saberWallDamageScale", "0.4", CVAR_SERVERINFO, 0, qfalse },

	{ &d_saberStanceDebug, "d_saberStanceDebug", "0", 0, 0, qfalse },

	{ &g_siegeTeamSwitch, "g_siegeTeamSwitch", "1", CVAR_SERVERINFO|CVAR_ARCHIVE, qfalse },

	{ &bg_fighterAltControl, "bg_fighterAltControl", "0", CVAR_SERVERINFO, 0, qtrue },

#ifdef DEBUG_SABER_BOX
	{ &g_saberDebugBox, "g_saberDebugBox", "0", CVAR_CHEAT, 0, qfalse },
#endif

	{ &d_altRoutes, "d_altRoutes", "0", CVAR_CHEAT, 0, qfalse },
	{ &d_patched, "d_patched", "0", CVAR_CHEAT, 0, qfalse },

	{ &g_saberRealisticCombat, "g_saberRealisticCombat", "0", CVAR_CHEAT },
	{ &g_saberRestrictForce, "g_saberRestrictForce", "0", CVAR_CHEAT },
	{ &d_saberSPStyleDamage, "d_saberSPStyleDamage", "1", CVAR_CHEAT },

	{ &debugNoRoam, "d_noroam", "0", CVAR_CHEAT },
	{ &debugNPCAimingBeam, "d_npcaiming", "0", CVAR_CHEAT },
	{ &debugBreak, "d_break", "0", CVAR_CHEAT },
	{ &debugNPCAI, "d_npcai", "0", CVAR_CHEAT },
	{ &debugNPCFreeze, "d_npcfreeze", "0", CVAR_CHEAT },
	{ &d_JediAI, "d_JediAI", "0", CVAR_CHEAT },
	{ &d_noGroupAI, "d_noGroupAI", "0", CVAR_CHEAT },
	{ &d_asynchronousGroupAI, "d_asynchronousGroupAI", "0", CVAR_CHEAT },
	
	//0 = never (BORING)
	//1 = kyle only
	//2 = kyle and last enemy jedi
	//3 = kyle and any enemy jedi
	//4 = kyle and last enemy in a group
	//5 = kyle and any enemy
	//6 = also when kyle takes pain or enemy jedi dodges player saber swing or does an acrobatic evasion

	{ &d_slowmodeath, "d_slowmodeath", "0", CVAR_CHEAT },

	{ &d_saberCombat, "d_saberCombat", "0", CVAR_CHEAT },

	{ &g_spskill, "g_npcspskill", "0", CVAR_ARCHIVE | CVAR_INTERNAL },

	//for overriding the level defaults
	{ &g_siegeTeam1, "g_siegeTeam1", "none", CVAR_ARCHIVE|CVAR_SERVERINFO, 0, qfalse  },
	{ &g_siegeTeam2, "g_siegeTeam2", "none", CVAR_ARCHIVE|CVAR_SERVERINFO, 0, qfalse  },

	//mainly for debugging with bots while I'm not around (want the server to
	//cycle through levels naturally)
	{ &d_noIntermissionWait, "d_noIntermissionWait", "0", CVAR_CHEAT, 0, qfalse  },

	{ &g_austrian, "g_austrian", "0", CVAR_ARCHIVE, 0, qfalse  },
// nmckenzie:
// DUEL_HEALTH
	{ &g_showDuelHealths, "g_showDuelHealths", "0", CVAR_SERVERINFO },
	{ &g_powerDuelStartHealth, "g_powerDuelStartHealth", "150", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_powerDuelEndHealth, "g_powerDuelEndHealth", "90", CVAR_ARCHIVE, 0, qtrue  },

	// *CHANGE 12* allowing/disabling cvars
	{ &g_protectQ3Fill,	"g_protectQ3Fill"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_protectQ3FillIPLimit,	"g_protectQ3FillIPLimit"	, "3"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_protectHPhack,	"g_protectHPhack"	, "3"	, CVAR_ARCHIVE, 0, qtrue },
	

	{ &g_protectCallvoteHack,	"g_protectCallvoteHack"	, "1"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_maxIPConnected,	"g_maxIPConnected"	, "0"	, CVAR_ARCHIVE, 0, qtrue },	

	// *CHANGE 10* anti q3fill
	{ &g_cleverFakeDetection,	"g_cleverFakeDetection"	, "forcepowers"	, CVAR_ARCHIVE, 0, qtrue },

	{ &bot_minping,	"bot_minping"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &bot_maxping,	"bot_maxping"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &bot_ping_sparsity,	"bot_ping_sparsity"	, "20"	, CVAR_ARCHIVE, 0, qtrue },
	

	{ &g_fixPitKills,	"g_fixPitKills"	, "1"	, CVAR_ARCHIVE, 0, qtrue },

    { &g_minimumVotesCount, "g_minimumVotesCount", "0", CVAR_ARCHIVE, 0, qtrue },

    { &g_enforceEvenVotersCount, "g_enforceEvenVotersCount", "0", CVAR_ARCHIVE, 0, qtrue },
    { &g_minVotersForEvenVotersCount, "g_minVotersForEvenVotersCount", "7", CVAR_ARCHIVE, 0, qtrue },

	{ &g_strafejump_mod,	"g_strafejump_mod"	, "0"	, CVAR_ARCHIVE, 0, qtrue },

    { &g_default_restart_countdown, "g_default_restart_countdown", "0", CVAR_ARCHIVE, 0, qtrue }, 

	{ &g_allow_vote_gametype,	"g_allow_vote_gametype"	, "1023"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_kick,	"g_allow_vote_kick"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_restart,	"g_allow_vote_restart"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_map,	"g_allow_vote_map"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_nextmap,	"g_allow_vote_nextmap"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_timelimit,	"g_allow_vote_timelimit"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_fraglimit,	"g_allow_vote_fraglimit"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_maprandom, "g_allow_vote_maprandom", "1", CVAR_ARCHIVE, 0, qtrue },
    { &g_allow_vote_warmup, "g_allow_vote_warmup", "1", CVAR_ARCHIVE, 0, qtrue },          

	{ &g_quietrcon,	"g_quietrcon"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_hackLog,	"g_hackLog"	, "hacks.log"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_npc_spawn_limit,	"g_npc_spawn_limit"	, "100"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_accounts,	"g_accounts"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_accountsFile,	"g_accountsFile"	, "accounts.txt"	, CVAR_ARCHIVE, 0, qtrue },
    { &g_whitelist, "g_whitelist", "0", CVAR_ARCHIVE, 0, qtrue },


	{ &g_dlURL,	"g_dlURL"	, ""	, CVAR_SYSTEMINFO, 0, qtrue },

	{ &g_fixboon,	"g_fixboon"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
    { &g_flags_overboarding, "g_flags_overboarding", "1", CVAR_ARCHIVE, 0, qtrue },
    { &g_selfkill_penalty, "g_selfkill_penalty", "1", CVAR_ARCHIVE, 0, qtrue },
    
	{ &g_maxstatusrequests,	"g_maxstatusrequests"	, "50"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_testdebug,	"g_testdebug"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	
	{ &g_logrcon,	"g_logrcon"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_rconpassword,	"rconpassword"	, "0"	, CVAR_ARCHIVE | CVAR_INTERNAL },

	{ &g_callvotedelay,	"g_callvotedelay"	, "0"	, CVAR_ARCHIVE | CVAR_INTERNAL },
    { &g_callvotemaplimit,	"g_callvotemaplimit"	, "0"	, CVAR_ARCHIVE | CVAR_INTERNAL },
    
    { &sv_privateclients, "sv_privateclients", "0", CVAR_ARCHIVE | CVAR_SERVERINFO },
    { &g_defaultBanHoursDuration, "g_defaultBanHoursDuration", "24", CVAR_ARCHIVE | CVAR_INTERNAL },  

	{ &g_floatingItems, "g_floatingItems", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_rocketSurfing, "g_rocketSurfing", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_fixNodropDetpacks, "g_fixNodropDetpacks", "1", CVAR_ARCHIVE, 0, qtrue },
	

};

// bk001129 - made static to avoid aliasing
static int gameCvarTableSize = sizeof( gameCvarTable ) / sizeof( gameCvarTable[0] );


void G_InitGame					( int levelTime, int randomSeed, int restart );
void G_RunFrame					( int levelTime );
void G_ShutdownGame				( int restart );
void CheckExitRules				( void );
void G_ROFF_NotetrackCallback	( gentity_t *cent, const char *notetrack);

extern stringID_table_t setTable[];

qboolean G_ParseSpawnVars( qboolean inSubBSP );
void G_SpawnGEntityFromSpawnVars( qboolean inSubBSP );


qboolean NAV_ClearPathToPoint( gentity_t *self, vec3_t pmins, vec3_t pmaxs, vec3_t point, int clipmask, int okToHitEntNum );
qboolean NPC_ClearLOS2( gentity_t *ent, const vec3_t end );
int NAVNEW_ClearPathBetweenPoints(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int ignore, int clipmask);
qboolean NAV_CheckNodeFailedForEnt( gentity_t *ent, int nodeNum );
qboolean G_EntIsUnlockedDoor( int entityNum );
qboolean G_EntIsDoor( int entityNum );
qboolean G_EntIsBreakable( int entityNum );
qboolean G_EntIsRemovableUsable( int entNum );
void CP_FindCombatPointWaypoints( void );

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
#include "namespace_begin.h"
#if defined(__linux__) && !defined(__GCC__)
extern "C" {
#endif

int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11  ) {
	switch ( command ) {
	case GAME_INIT:
		G_InitGame( arg0, arg1, arg2 );
		return 0;
	case GAME_SHUTDOWN:
		G_ShutdownGame( arg0 );
		return 0;
	case GAME_CLIENT_CONNECT:
		return (int)ClientConnect( arg0, arg1, arg2 );
	case GAME_CLIENT_THINK:
		ClientThink( arg0, NULL );
		return 0;
	case GAME_CLIENT_USERINFO_CHANGED:
		ClientUserinfoChanged( arg0 );
		return 0;
	case GAME_CLIENT_DISCONNECT:
		ClientDisconnect( arg0 );
		return 0;
	case GAME_CLIENT_BEGIN:
		ClientBegin( arg0, qtrue );
		return 0;
	case GAME_CLIENT_COMMAND:
		ClientCommand( arg0 );
		return 0;
	case GAME_RUN_FRAME:
		G_RunFrame( arg0 );
		return 0;
	case GAME_CONSOLE_COMMAND:
		return ConsoleCommand();
	case BOTAI_START_FRAME:
		return BotAIStartFrame( arg0 );
	case GAME_ROFF_NOTETRACK_CALLBACK:
		G_ROFF_NotetrackCallback( &g_entities[arg0], (const char *)arg1 );
		return 0;
	case GAME_SPAWN_RMG_ENTITY:
		if (G_ParseSpawnVars(qfalse))
		{
			G_SpawnGEntityFromSpawnVars(qfalse);
		}
		return 0;

	//rww - begin icarus callbacks
	case GAME_ICARUS_PLAYSOUND:
		{
			T_G_ICARUS_PLAYSOUND *sharedMem = (T_G_ICARUS_PLAYSOUND *)gSharedBuffer;
			return Q3_PlaySound(sharedMem->taskID, sharedMem->entID, sharedMem->name, sharedMem->channel);
		}
	case GAME_ICARUS_SET:
		{
			T_G_ICARUS_SET *sharedMem = (T_G_ICARUS_SET *)gSharedBuffer;
			return Q3_Set(sharedMem->taskID, sharedMem->entID, sharedMem->type_name, sharedMem->data);
		}
	case GAME_ICARUS_LERP2POS:
		{
			T_G_ICARUS_LERP2POS *sharedMem = (T_G_ICARUS_LERP2POS *)gSharedBuffer;
			if (sharedMem->nullAngles)
			{
				Q3_Lerp2Pos(sharedMem->taskID, sharedMem->entID, sharedMem->origin, NULL, sharedMem->duration);
			}
			else
			{
				Q3_Lerp2Pos(sharedMem->taskID, sharedMem->entID, sharedMem->origin, sharedMem->angles, sharedMem->duration);
			}
		}
		return 0;
	case GAME_ICARUS_LERP2ORIGIN:
		{
			T_G_ICARUS_LERP2ORIGIN *sharedMem = (T_G_ICARUS_LERP2ORIGIN *)gSharedBuffer;
			Q3_Lerp2Origin(sharedMem->taskID, sharedMem->entID, sharedMem->origin, sharedMem->duration);
		}
		return 0;
	case GAME_ICARUS_LERP2ANGLES:
		{
			T_G_ICARUS_LERP2ANGLES *sharedMem = (T_G_ICARUS_LERP2ANGLES *)gSharedBuffer;
			Q3_Lerp2Angles(sharedMem->taskID, sharedMem->entID, sharedMem->angles, sharedMem->duration);
		}
		return 0;
	case GAME_ICARUS_GETTAG:
		{
			T_G_ICARUS_GETTAG *sharedMem = (T_G_ICARUS_GETTAG *)gSharedBuffer;
			return Q3_GetTag(sharedMem->entID, sharedMem->name, sharedMem->lookup, sharedMem->info);
		}
	case GAME_ICARUS_LERP2START:
		{
			T_G_ICARUS_LERP2START *sharedMem = (T_G_ICARUS_LERP2START *)gSharedBuffer;
			Q3_Lerp2Start(sharedMem->entID, sharedMem->taskID, sharedMem->duration);
		}
		return 0;
	case GAME_ICARUS_LERP2END:
		{
			T_G_ICARUS_LERP2END *sharedMem = (T_G_ICARUS_LERP2END *)gSharedBuffer;
			Q3_Lerp2End(sharedMem->entID, sharedMem->taskID, sharedMem->duration);
		}
		return 0;
	case GAME_ICARUS_USE:
		{
			T_G_ICARUS_USE *sharedMem = (T_G_ICARUS_USE *)gSharedBuffer;
			Q3_Use(sharedMem->entID, sharedMem->target);
		}
		return 0;
	case GAME_ICARUS_KILL:
		{
			T_G_ICARUS_KILL *sharedMem = (T_G_ICARUS_KILL *)gSharedBuffer;
			Q3_Kill(sharedMem->entID, sharedMem->name);
		}
		return 0;
	case GAME_ICARUS_REMOVE:
		{
			T_G_ICARUS_REMOVE *sharedMem = (T_G_ICARUS_REMOVE *)gSharedBuffer;
			Q3_Remove(sharedMem->entID, sharedMem->name);
		}
		return 0;
	case GAME_ICARUS_PLAY:
		{
			T_G_ICARUS_PLAY *sharedMem = (T_G_ICARUS_PLAY *)gSharedBuffer;
			Q3_Play(sharedMem->taskID, sharedMem->entID, sharedMem->type, sharedMem->name);
		}
		return 0;
	case GAME_ICARUS_GETFLOAT:
		{
			T_G_ICARUS_GETFLOAT *sharedMem = (T_G_ICARUS_GETFLOAT *)gSharedBuffer;
			return Q3_GetFloat(sharedMem->entID, sharedMem->type, sharedMem->name, &sharedMem->value);
		}
	case GAME_ICARUS_GETVECTOR:
		{
			T_G_ICARUS_GETVECTOR *sharedMem = (T_G_ICARUS_GETVECTOR *)gSharedBuffer;
			return Q3_GetVector(sharedMem->entID, sharedMem->type, sharedMem->name, sharedMem->value);
		}
	case GAME_ICARUS_GETSTRING:
		{
			T_G_ICARUS_GETSTRING *sharedMem = (T_G_ICARUS_GETSTRING *)gSharedBuffer;
			int r;
			char *crap = NULL; //I am sorry for this -rww
			char **morecrap = &crap; //and this
			r = Q3_GetString(sharedMem->entID, sharedMem->type, sharedMem->name, morecrap);

			if (crap)
			{ //success!
				strcpy(sharedMem->value, crap);
			}

			return r;
		}
	case GAME_ICARUS_SOUNDINDEX:
		{
			T_G_ICARUS_SOUNDINDEX *sharedMem = (T_G_ICARUS_SOUNDINDEX *)gSharedBuffer;
			G_SoundIndex(sharedMem->filename);
		}
		return 0;
	case GAME_ICARUS_GETSETIDFORSTRING:
		{
			T_G_ICARUS_GETSETIDFORSTRING *sharedMem = (T_G_ICARUS_GETSETIDFORSTRING *)gSharedBuffer;
			return GetIDForString(setTable, sharedMem->string);
		}
	//rww - end icarus callbacks

	case GAME_NAV_CLEARPATHTOPOINT:
		return NAV_ClearPathToPoint(&g_entities[arg0], (float *)arg1, (float *)arg2, (float *)arg3, arg4, arg5);
	case GAME_NAV_CLEARLOS:
		return NPC_ClearLOS2(&g_entities[arg0], (const float *)arg1);
	case GAME_NAV_CLEARPATHBETWEENPOINTS:
		return NAVNEW_ClearPathBetweenPoints((float *)arg0, (float *)arg1, (float *)arg2, (float *)arg3, arg4, arg5);
	case GAME_NAV_CHECKNODEFAILEDFORENT:
		return NAV_CheckNodeFailedForEnt(&g_entities[arg0], arg1);
	case GAME_NAV_ENTISUNLOCKEDDOOR:
		return G_EntIsUnlockedDoor(arg0);
	case GAME_NAV_ENTISDOOR:
		return G_EntIsDoor(arg0);
	case GAME_NAV_ENTISBREAKABLE:
		return G_EntIsBreakable(arg0);
	case GAME_NAV_ENTISREMOVABLEUSABLE:
		return G_EntIsRemovableUsable(arg0);
	case GAME_NAV_FINDCOMBATPOINTWAYPOINTS:
		CP_FindCombatPointWaypoints();
		return 0;
	case GAME_GETITEMINDEXBYTAG:
		return BG_GetItemIndexByTag(arg0, arg1);
	}

	return -1;
}
#if defined(__linux__) && !defined(__GCC__)
}
#endif
#include "namespace_end.h"

#define BUFFER_TEMP_LEN 2048
#define BUFFER_REAL_LEN 1024
void QDECL G_Printf( const char *fmt, ... ) {
	va_list		argptr;
	static char		text[BUFFER_TEMP_LEN];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	text[BUFFER_REAL_LEN-1] = '\0';

	trap_Printf( text );
}

void QDECL G_Error( const char *fmt, ... ) {
	va_list		argptr;
	static char		text[BUFFER_TEMP_LEN];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	text[BUFFER_REAL_LEN-1] = '\0';

	trap_Error( text );
}

/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMSLAVE flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams( void ) {
	gentity_t	*e, *e2;
	int		i, j;
	int		c, c2;

	c = 0;
	c2 = 0;
	for ( i=1, e=g_entities+i ; i < level.num_entities ; i++,e++ ){
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (e->flags & FL_TEAMSLAVE)
			continue;
		if (e->r.contents==CONTENTS_TRIGGER)
			continue;//triggers NEVER link up in teams!
		e->teammaster = e;
		c++;
		c2++;
		for (j=i+1, e2=e+1 ; j < level.num_entities ; j++,e2++)
		{
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!strcmp(e->team, e2->team))
			{
				c2++;
				e2->teamchain = e->teamchain;
				e->teamchain = e2;
				e2->teammaster = e;
				e2->flags |= FL_TEAMSLAVE;

				// make sure that targets only point at the master
				if ( e2->targetname ) {
					e->targetname = e2->targetname;
					e2->targetname = NULL;
				}
			}
		}
	}

}

void G_RemapTeamShaders( void ) {
#if 0
	char string[1024];
	float f = level.time * 0.001;
	Com_sprintf( string, sizeof(string), "team_icon/%s_red", g_redteam.string );
	AddRemap("textures/ctf2/redteam01", string, f); 
	AddRemap("textures/ctf2/redteam02", string, f); 
	Com_sprintf( string, sizeof(string), "team_icon/%s_blue", g_blueteam.string );
	AddRemap("textures/ctf2/blueteam01", string, f); 
	AddRemap("textures/ctf2/blueteam02", string, f); 
	trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
#endif
}


/*
=================
G_RegisterCvars
=================
*/
void G_RegisterCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	qboolean remapped = qfalse;

	for ( i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++ ) {
		trap_Cvar_Register( cv->vmCvar, cv->cvarName,
			cv->defaultString, cv->cvarFlags );
		if ( cv->vmCvar )
			cv->modificationCount = cv->vmCvar->modificationCount;

		if (cv->teamShader) {
			remapped = qtrue;
		}
	}

	if (remapped) {
		G_RemapTeamShaders();
	}

	// check some things
	if ( g_gametype.integer < 0 || g_gametype.integer >= GT_MAX_GAME_TYPE ) {
		G_Printf( "g_gametype %i is out of range, defaulting to 0\n", g_gametype.integer );
		trap_Cvar_Set( "g_gametype", "0" );
	}
	else if (g_gametype.integer == GT_HOLOCRON)
	{
		G_Printf( "This gametype is not supported.\n" );
		trap_Cvar_Set( "g_gametype", "0" );
	}
	else if (g_gametype.integer == GT_JEDIMASTER)
	{
		G_Printf( "This gametype is not supported.\n" );
		trap_Cvar_Set( "g_gametype", "0" );
	}
	else if (g_gametype.integer == GT_CTY)
	{
		G_Printf( "This gametype is not supported.\n" );
		trap_Cvar_Set( "g_gametype", "0" );
	}

	level.warmupModificationCount = g_warmup.modificationCount;
}

/*
=================
G_UpdateCvars
=================
*/
void G_UpdateCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	qboolean remapped = qfalse;

	for ( i = 0, cv = gameCvarTable ; i < gameCvarTableSize ; i++, cv++ ) {
		if ( cv->vmCvar ) {
			trap_Cvar_Update( cv->vmCvar );

			if ( cv->modificationCount != cv->vmCvar->modificationCount ) {
				cv->modificationCount = cv->vmCvar->modificationCount;

				if ( cv->trackChange ) {
					trap_SendServerCommand( -1, va("print \"Server: %s changed to %s\n\"", 
						cv->cvarName, cv->vmCvar->string ) );
				}

				if (cv->teamShader) {
					remapped = qtrue;
				}				
			}
		}
	}

	if (remapped) {
		G_RemapTeamShaders();
	}
}

void initMatch(){
	int i;
	gentity_t	*ent;

	//G_LogPrintf("initMatch()\n");

	if (!g_wasRestarted.integer)
		return;

	level.initialConditionsMatch = qfalse;

	if (g_gametype.integer != GT_CTF || !g_accounts.integer)
		return;

	//fill roster
	level.initialBlueCount = 0;
	level.initialRedCount = 0;

	for(i=0;i<level.maxclients;++i){
		ent = &g_entities[i];

		if (!ent->inuse || !ent->client)
			continue;
		
		if (ent->client->sess.sessionTeam == TEAM_RED){		
			level.initialRedRoster[level.initialRedCount].clientNum = i;
			strncpy(level.initialRedRoster[level.initialRedCount].username,
				ent->client->sess.username,MAX_USERNAME_SIZE);

			++level.initialRedCount;
		} else if (ent->client->sess.sessionTeam == TEAM_BLUE){		
			level.initialBlueRoster[level.initialBlueCount].clientNum = i;
			strncpy(level.initialBlueRoster[level.initialBlueCount].username,
				ent->client->sess.username,MAX_USERNAME_SIZE);
			++level.initialBlueCount;
		}

	}

	if (level.initialBlueCount < 4 || level.initialRedCount < 4)
		return;
		
	level.initialConditionsMatch = qtrue;

}


char gSharedBuffer[MAX_G_SHARED_BUFFER_SIZE];

#include "namespace_begin.h"
void WP_SaberLoadParms( void );
void BG_VehicleLoadParms( void );
#include "namespace_end.h"

/*
============
G_InitGame

============
*/
extern void RemoveAllWP(void);
extern void BG_ClearVehicleParseParms(void);
extern void G_LoadArenas( void );
extern void G_LoadVoteMapsPools(void);
extern void InitUnhandledExceptionFilter();

void G_InitGame( int levelTime, int randomSeed, int restart ) {
	int					i;
	vmCvar_t	mapname;
	vmCvar_t	ckSum;

	InitUnhandledExceptionFilter();

#ifdef _XBOX
	if(restart) {
		BG_ClearVehicleParseParms();
		RemoveAllWP();
	}
#endif

	//Init RMG to 0, it will be autoset to 1 if there is terrain on the level.
	trap_Cvar_Set("RMG", "0");
	g_RMG.integer = 0;

	//Clean up any client-server ghoul2 instance attachments that may still exist exe-side
	trap_G2API_CleanEntAttachments();

	BG_InitAnimsets(); //clear it out

	B_InitAlloc(); //make sure everything is clean

	trap_SV_RegisterSharedMemory(gSharedBuffer);

	//Load external vehicle data
	BG_VehicleLoadParms();

	G_Printf ("------- Game Initialization -------\n");
	G_Printf ("gamename: %s\n", GAMEVERSION);
	G_Printf ("gamedate: %s\n", __DATE__);

	srand( randomSeed );

	G_RegisterCvars();

	G_ProcessGetstatusIPBans();

	G_InitMemory();

	// set some level globals
	memset( &level, 0, sizeof( level ) );
	level.time = levelTime;
	level.startTime = levelTime;

	level.snd_fry = G_SoundIndex("sound/player/fry.wav");	// FIXME standing in lava / slime

	level.snd_hack = G_SoundIndex("sound/player/hacking.wav");
	level.snd_medHealed = G_SoundIndex("sound/player/supp_healed.wav");
	level.snd_medSupplied = G_SoundIndex("sound/player/supp_supplied.wav");

#ifndef _XBOX
	if ( g_log.string[0] ) {
		char	serverinfo[MAX_INFO_STRING];

		trap_GetServerinfo( serverinfo, sizeof( serverinfo ) );

		if ( g_logSync.integer ) {
			trap_FS_FOpenFile( g_log.string, &level.logFile, FS_APPEND_SYNC );
		} else {
			trap_FS_FOpenFile( g_log.string, &level.logFile, FS_APPEND );
		}

		if (g_hackLog.string[0]){
			trap_FS_FOpenFile( g_hackLog.string, &level.hackLogFile, FS_APPEND_SYNC );

			if ( !level.hackLogFile ) {
				G_Printf( "WARNING: Couldn't open crash logfile: %s\n", g_hackLog.string );
			}
		}	

		if ( !level.logFile ) {
			G_Printf( "WARNING: Couldn't open logfile: %s\n", g_log.string );
		} else {
			G_LogPrintf("------------------------------------------------------------\n" );
			G_LogPrintf("Timestamp: %s\n", getCurrentTime() ); //contains \n already :S
			G_LogPrintf("InitGame: %s\n", serverinfo );
		}

		if (g_logrcon.integer){
			trap_FS_FOpenFile( "rcon.log", &level.rconLogFile, FS_APPEND_SYNC );
			if (!level.rconLogFile){
				G_Printf( "WARNING: Couldn't open rcon logfile: %s\n", "rcon.log" );
			}
		}

		// accounts system
		//if (g_accounts.integer && db_log.integer){
		//	trap_FS_FOpenFile( "db.log", &level.DBLogFile, FS_APPEND_SYNC ); //remove SYNC in release

		//	if (!level.DBLogFile){
		//		G_Printf( "WARNING: Couldn't open dbfile: %s\n", "db.log" );
		//	} else {
		//		int	min, tens, sec;
		//		sec = level.time / 1000;

		//		min = sec / 60;
		//		sec -= min * 60;
		//		tens = sec / 10;
		//		sec -= tens * 10;

		//		G_DBLog("------------------------------------------------------------\n" );
		//		G_DBLog("InitGame: %3i:%i%i %s\n", min, tens, sec, serverinfo );
		//	}
		//}

	} else {
		G_Printf( "Not logging to disk.\n" );
	}
#endif

	if (!restart)
	{
		trap_Cvar_Set("g_wasRestarted", "0");
	}

	// accounts system
	//initDB();

	////load accounts
	//if (g_accounts.integer){
	//	loadAccounts();
	//}

	G_LogWeaponInit();

	G_InitWorldSession();

	// initialize all entities for this game
	memset( g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]) );
	level.gentities = g_entities;

	// initialize all clients for this game
	level.maxclients = g_maxclients.integer;
	memset( g_clients, 0, MAX_CLIENTS * sizeof(g_clients[0]) );
	level.clients = g_clients;

	// set client fields on player ents
	for ( i=0 ; i<level.maxclients ; i++ ) {
		g_entities[i].client = level.clients + i;
	}

    G_ReadSessionData();

	// always leave room for the max number of clients,
	// even if they aren't all used, so numbers inside that
	// range are NEVER anything but clients
	level.num_entities = MAX_CLIENTS;

	// let the server system know where the entites are
	trap_LocateGameData( level.gentities, level.num_entities, sizeof( gentity_t ), 
		&level.clients[0].ps, sizeof( level.clients[0] ) );

	//Load sabers.cfg data
	WP_SaberLoadParms();

	NPC_InitGame();

	TIMER_Clear();
	//
	//ICARUS INIT START

	trap_ICARUS_Init();

	//ICARUS INIT END
	//

	// reserve some spots for dead player bodies
	InitBodyQue();

	ClearRegisteredItems();

	//make sure saber data is loaded before this! (so we can precache the appropriate hilts)
	InitSiegeMode();

	trap_Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
	trap_Cvar_Register( &ckSum, "sv_mapChecksum", "", CVAR_ROM );

	navCalculatePaths	= ( trap_Nav_Load( mapname.string, ckSum.integer ) == qfalse );

	// parse the key/value pairs and spawn gentities
	G_SpawnEntitiesFromString(qfalse);

	// general initialization
	G_FindTeams();

	// clean intermission configstring flag
	trap_SetConfigstring( CS_INTERMISSION, "0" );

	// make sure we have flags for CTF, etc
	if( g_gametype.integer >= GT_TEAM ) {
		G_CheckTeamItems();
	}
	else if ( g_gametype.integer == GT_JEDIMASTER )
	{
		trap_SetConfigstring ( CS_CLIENT_JEDIMASTER, "-1" );
	}

	if (g_gametype.integer == GT_POWERDUEL)
	{
		trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("-1|-1|-1") );
	}
	else
	{
		trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("-1|-1") );
	}
// nmckenzie: DUEL_HEALTH: Default.
	trap_SetConfigstring ( CS_CLIENT_DUELHEALTHS, va("-1|-1|!") );
	trap_SetConfigstring ( CS_CLIENT_DUELWINNER, va("-1") );

	SaveRegisteredItems();

	if( g_gametype.integer == GT_SINGLE_PLAYER || trap_Cvar_VariableIntegerValue( "com_buildScript" ) ) {
		G_ModelIndex( SP_PODIUM_MODEL );
		G_SoundIndex( "sound/player/gurp1.wav" );
		G_SoundIndex( "sound/player/gurp2.wav" );
	}

	G_LoadArenas(); //*CHANGE 93* loading map list not dependant on bot_enable cvar
    G_InitVoteMapsLimit();

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAISetup( restart );
		BotAILoadMap( restart );
		G_InitBots( restart );
	}

	G_RemapTeamShaders();

	if ( g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL )
	{
		G_LogPrintf("Duel Tournament Begun: kill limit %d, win limit: %d\n", g_fraglimit.integer, g_duel_fraglimit.integer );
	}

	if ( navCalculatePaths )
	{//not loaded - need to calc paths
		navCalcPathTime = level.time + START_TIME_NAV_CALC;//make sure all ents are in and linked
	}
	else
	{//loaded
		//FIXME: if this is from a loadgame, it needs to be sure to write this 
		//out whenever you do a savegame since the edges and routes are dynamic...
		//OR: always do a navigator.CheckBlockedEdges() on map startup after nav-load/calc-paths
		//navigator.pathsCalculated = qtrue;//just to be safe?  Does this get saved out?  No... assumed
		trap_Nav_SetPathsCalculated(qtrue);
		//need to do this, because combatpoint waypoints aren't saved out...?
		CP_FindCombatPointWaypoints();
		navCalcPathTime = 0;

	}

	if (g_gametype.integer == GT_SIEGE)
	{ //just get these configstrings registered now...
		int i = 0;
		while (i < MAX_CUSTOM_SIEGE_SOUNDS)
		{
			if (!bg_customSiegeSoundNames[i])
			{
				break;
			}
			G_SoundIndex((char *)bg_customSiegeSoundNames[i]);
			i++;
		}
	}

    if ( BG_IsLegacyEngine() )
    {
		PatchEngine();
	}

    G_CfgDbLoad();
    G_LogDbLoad();
    level.db.levelId = G_LogDbLogLevelStart(restart);
}



/*
=================
G_ShutdownGame
=================
*/
void G_ShutdownGame( int restart ) {
	int i = 0;
	gentity_t *ent;

	G_CleanAllFakeClients(); //get rid of dynamically allocated fake client structs.

	BG_ClearAnimsets(); //free all dynamic allocations made through the engine

	while (i < MAX_GENTITIES)
	{ //clean up all the ghoul2 instances
		ent = &g_entities[i];

		if (ent->ghoul2 && trap_G2_HaveWeGhoul2Models(ent->ghoul2))
		{
			trap_G2API_CleanGhoul2Models(&ent->ghoul2);
			ent->ghoul2 = NULL;
		}
		if (ent->client)
		{
			int j = 0;

			while (j < MAX_SABERS)
			{
				if (ent->client->weaponGhoul2[j] && trap_G2_HaveWeGhoul2Models(ent->client->weaponGhoul2[j]))
				{
					trap_G2API_CleanGhoul2Models(&ent->client->weaponGhoul2[j]);
				}
				j++;
			}
		}
		i++;
	}
	if (g2SaberInstance && trap_G2_HaveWeGhoul2Models(g2SaberInstance))
	{
		trap_G2API_CleanGhoul2Models(&g2SaberInstance);
		g2SaberInstance = NULL;
	}
	if (precachedKyle && trap_G2_HaveWeGhoul2Models(precachedKyle))
	{
		trap_G2API_CleanGhoul2Models(&precachedKyle);
		precachedKyle = NULL;
	}

	trap_ICARUS_Shutdown ();	//Shut ICARUS down

	TAG_Init();	//Clear the reference tags

	G_LogWeaponOutput();

	if ( level.logFile ) {
		G_LogPrintf("ShutdownGame:\n" );
		G_LogPrintf("------------------------------------------------------------\n" );
		trap_FS_FCloseFile( level.logFile );
	}

	if ( level.hackLogFile ) {
		trap_FS_FCloseFile( level.hackLogFile );
	}

	if ( level.DBLogFile ) {
		G_DBLog("ShutdownGame:\n" );
		G_DBLog("------------------------------------------------------------\n" );
		trap_FS_FCloseFile( level.DBLogFile );
	}

	if ( level.rconLogFile ) {
		trap_FS_FCloseFile( level.rconLogFile );
	}

	// write all the client session data so we can get it back
	G_WriteSessionData();

	trap_ROFF_Clean();

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAIShutdown( restart );
	}

	B_CleanupAlloc(); //clean up all allocations made with B_Alloc

	// accounts system
	//cleanDB();

    G_LogDbLogLevelEnd(level.db.levelId);

    G_CfgDbUnload();
    G_LogDbUnload();

	UnpatchEngine();
}



//===================================================================

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c and bg_*.c can link

void QDECL Com_Error ( int level, const char *error, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	G_Error( "%s", text);
}

void QDECL Com_Printf( const char *msg, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	vsprintf (text, msg, argptr);
	va_end (argptr);

	G_Printf ("%s", text);
}

#endif

/*
========================================================================

PLAYER COUNTING / SCORE SORTING

========================================================================
*/

/*
=============
AddTournamentPlayer

If there are less than two tournament players, put a
spectator in the game and restart
=============
*/
void AddTournamentPlayer( void ) {
	int			i;
	gclient_t	*client;
	gclient_t	*nextInLine;

	if ( level.numPlayingClients >= 2 ) {
		return;
	}

	nextInLine = NULL;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if (!g_allowHighPingDuelist.integer && client->ps.ping >= 999)
		{ //don't add people who are lagging out if cvar is not set to allow it.
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		// never select the dedicated follow or scoreboard clients
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD || 
			client->sess.spectatorClient < 0  ) {
			continue;
		}

		if ( !nextInLine || client->sess.spectatorTime < nextInLine->sess.spectatorTime ) {
			nextInLine = client;
		}
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.clients ], "f" );
}

/*
=======================
RemoveTournamentLoser

Make the loser a spectator at the back of the line
=======================
*/
void RemoveTournamentLoser( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[1];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" );
}

void G_PowerDuelCount(int *loners, int *doubles, qboolean countSpec)
{
	int i = 0;
	gclient_t *cl;

	while (i < MAX_CLIENTS)
	{
		cl = g_entities[i].client;

		if (g_entities[i].inuse && cl && (countSpec || cl->sess.sessionTeam != TEAM_SPECTATOR))
		{
			if (cl->sess.duelTeam == DUELTEAM_LONE)
			{
				(*loners)++;
			}
			else if (cl->sess.duelTeam == DUELTEAM_DOUBLE)
			{
				(*doubles)++;
			}
		}
		i++;
	}
}

qboolean g_duelAssigning = qfalse;
void AddPowerDuelPlayers( void )
{
	int			i;
	int			loners = 0;
	int			doubles = 0;
	int			nonspecLoners = 0;
	int			nonspecDoubles = 0;
	gclient_t	*client;
	gclient_t	*nextInLine;

	if ( level.numPlayingClients >= 3 )
	{
		return;
	}

	nextInLine = NULL;

	G_PowerDuelCount(&nonspecLoners, &nonspecDoubles, qfalse);
	if (nonspecLoners >= 1 && nonspecDoubles >= 2)
	{ //we have enough people, stop
		return;
	}

	//Could be written faster, but it's not enough to care I suppose.
	G_PowerDuelCount(&loners, &doubles, qtrue);

	if (loners < 1 || doubles < 2)
	{ //don't bother trying to spawn anyone yet if the balance is not even set up between spectators
		return;
	}

	//Count again, with only in-game clients in mind.
	loners = nonspecLoners;
	doubles = nonspecDoubles;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_FREE)
		{
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_LONE && loners >= 1)
		{
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_DOUBLE && doubles >= 2)
		{
			continue;
		}

		// never select the dedicated follow or scoreboard clients
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD || 
			client->sess.spectatorClient < 0  ) {
			continue;
		}

		if ( !nextInLine || client->sess.spectatorTime < nextInLine->sess.spectatorTime ) {
			nextInLine = client;
		}
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.clients ], "f" );

	//Call recursively until everyone is in
	AddPowerDuelPlayers();
}

qboolean g_dontFrickinCheck = qfalse;

void RemovePowerDuelLosers(void)
{
	int remClients[3];
	int remNum = 0;
	int i = 0;
	gclient_t *cl;

	while (i < MAX_CLIENTS && remNum < 3)
	{
		cl = &level.clients[i];

		if (cl->pers.connected == CON_CONNECTED)
		{
			if ((cl->ps.stats[STAT_HEALTH] <= 0 || cl->iAmALoser) &&
				(cl->sess.sessionTeam != TEAM_SPECTATOR || cl->iAmALoser))
			{ //he was dead or he was spectating as a loser
                remClients[remNum] = cl->ps.clientNum;
				remNum++;
			}
		}

		i++;
	}

	if (!remNum)
	{ //Time ran out or something? Oh well, just remove the main guy.
		remClients[remNum] = level.sortedClients[0];
		remNum++;
	}

	i = 0;
	while (i < remNum)
	{ //set them all to spectator
		SetTeam( &g_entities[ remClients[i] ], "s" );
		i++;
	}

	g_dontFrickinCheck = qfalse;

	//recalculate stuff now that we have reset teams.
	CalculateRanks();
}

void RemoveDuelDrawLoser(void)
{
	int clFirst = 0;
	int clSec = 0;
	int clFailure = 0;

	if ( level.clients[ level.sortedClients[0] ].pers.connected != CON_CONNECTED )
	{
		return;
	}
	if ( level.clients[ level.sortedClients[1] ].pers.connected != CON_CONNECTED )
	{
		return;
	}

	clFirst = level.clients[ level.sortedClients[0] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[0] ].ps.stats[STAT_ARMOR];
	clSec = level.clients[ level.sortedClients[1] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[1] ].ps.stats[STAT_ARMOR];

	if (clFirst > clSec)
	{
		clFailure = 1;
	}
	else if (clSec > clFirst)
	{
		clFailure = 0;
	}
	else
	{
		clFailure = 2;
	}

	if (clFailure != 2)
	{
		SetTeam( &g_entities[ level.sortedClients[clFailure] ], "s" );
	}
	else
	{ //we could be more elegant about this, but oh well.
		SetTeam( &g_entities[ level.sortedClients[1] ], "s" );
	}
}

/*
=======================
RemoveTournamentWinner
=======================
*/
void RemoveTournamentWinner( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[0];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" );
}

/*
=======================
AdjustTournamentScores
=======================
*/
void AdjustTournamentScores( void ) {
	int			clientNum;

	if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
		level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
		level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
		level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
	{
		int clFirst = level.clients[ level.sortedClients[0] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[0] ].ps.stats[STAT_ARMOR];
		int clSec = level.clients[ level.sortedClients[1] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[1] ].ps.stats[STAT_ARMOR];
		int clFailure = 0;
		int clSuccess = 0;

		if (clFirst > clSec)
		{
			clFailure = 1;
			clSuccess = 0;
		}
		else if (clSec > clFirst)
		{
			clFailure = 0;
			clSuccess = 1;
		}
		else
		{
			clFailure = 2;
			clSuccess = 2;
		}

		if (clFailure != 2)
		{
			clientNum = level.sortedClients[clSuccess];

			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );
			trap_SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );

			clientNum = level.sortedClients[clFailure];

			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
		else
		{
			clSuccess = 0;
			clFailure = 1;

			clientNum = level.sortedClients[clSuccess];

			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );
			trap_SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );

			clientNum = level.sortedClients[clFailure];

			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
	}
	else
	{
		clientNum = level.sortedClients[0];
		if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );

			trap_SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );
		}

		clientNum = level.sortedClients[1];
		if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
	}
}

/*
=============
SortRanks

=============
*/
int QDECL SortRanks( const void *a, const void *b ) {
	gclient_t	*ca, *cb;

	ca = &level.clients[*(int *)a];
	cb = &level.clients[*(int *)b];

	if (g_gametype.integer == GT_POWERDUEL)
	{
		//sort single duelists first
		if (ca->sess.duelTeam == DUELTEAM_LONE && ca->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return -1;
		}
		if (cb->sess.duelTeam == DUELTEAM_LONE && cb->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return 1;
		}

		//others will be auto-sorted below but above spectators.
	}

	// sort special clients last
	if ( ca->sess.spectatorState == SPECTATOR_SCOREBOARD || ca->sess.spectatorClient < 0 ) {
		return 1;
	}
	if ( cb->sess.spectatorState == SPECTATOR_SCOREBOARD || cb->sess.spectatorClient < 0  ) {
		return -1;
	}

	// then connecting clients
	if ( ca->pers.connected == CON_CONNECTING ) {
		return 1;
	}
	if ( cb->pers.connected == CON_CONNECTING ) {
		return -1;
	}


	// then spectators
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR && cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( ca->sess.spectatorTime < cb->sess.spectatorTime ) {
			return -1;
		}
		if ( ca->sess.spectatorTime > cb->sess.spectatorTime ) {
			return 1;
		}
		return 0;
	}
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR ) {
		return 1;
	}
	if ( cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		return -1;
	}

	// then sort by score
	if ( ca->ps.persistant[PERS_SCORE]
		> cb->ps.persistant[PERS_SCORE] ) {
		return -1;
	}
	if ( ca->ps.persistant[PERS_SCORE]
		< cb->ps.persistant[PERS_SCORE] ) {
		return 1;
	}
	return 0;
}

qboolean gQueueScoreMessage = qfalse;
int gQueueScoreMessageTime = 0;

//A new duel started so respawn everyone and make sure their stats are reset
qboolean G_CanResetDuelists(void)
{
	int i;
	gentity_t *ent;

	i = 0;
	while (i < 3)
	{ //precheck to make sure they are all respawnable
		ent = &g_entities[level.sortedClients[i]];

		if (!ent->inuse || !ent->client || ent->health <= 0 ||
			ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
			ent->client->sess.duelTeam <= DUELTEAM_FREE)
		{
			return qfalse;
		}
		i++;
	}

	return qtrue;
}

qboolean g_noPDuelCheck = qfalse;
void G_ResetDuelists(void)
{
	int i;
	gentity_t *ent;
	gentity_t *tent;

	i = 0;
	while (i < 3)
	{
		ent = &g_entities[level.sortedClients[i]];

		g_noPDuelCheck = qtrue;
		player_die(ent, ent, ent, 999, MOD_SUICIDE);
		g_noPDuelCheck = qfalse;
		trap_UnlinkEntity (ent);
		ClientSpawn(ent);

		// add a teleportation effect
		tent = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = ent->s.clientNum;
		i++;
	}
}

/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every client connect, begin, disconnect, death,
and team change.
============
*/
void CalculateRanks( void ) {
	int		i;
	int		rank;
	int		score;
	int		newScore;
	gclient_t	*cl;

	level.follow1 = -1;
	level.follow2 = -1;
	level.numConnectedClients = 0;
	level.numNonSpectatorClients = 0;
	level.numPlayingClients = 0;
	for ( i = 0; i < 2; i++ ) {
		level.numteamVotingClients[i] = 0;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected != CON_DISCONNECTED ) {
			level.sortedClients[level.numConnectedClients] = i;
			level.numConnectedClients++;

			if ( level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL )
			{
				if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR)
				{
					level.numNonSpectatorClients++;
				}
			
				// decide if this should be auto-followed
				if ( level.clients[i].pers.connected == CON_CONNECTED )
				{
					if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || level.clients[i].iAmALoser)
					{
						level.numPlayingClients++;
					}
					if ( !(g_entities[i].r.svFlags & SVF_BOT) )
					{
						if ( level.clients[i].sess.sessionTeam == TEAM_RED )
							level.numteamVotingClients[0]++;
						else if ( level.clients[i].sess.sessionTeam == TEAM_BLUE )
							level.numteamVotingClients[1]++;
					}
					if ( level.follow1 == -1 ) {
						level.follow1 = i;
					} else if ( level.follow2 == -1 ) {
						level.follow2 = i;
					}
				}
			}
		}
	}

	//NOTE: for now not doing this either. May use later if appropriate.

	qsort( level.sortedClients, level.numConnectedClients, 
		sizeof(level.sortedClients[0]), SortRanks );

	// set the rank value for all clients that are connected and not spectators
	if ( g_gametype.integer >= GT_TEAM ) {
		// in team games, rank is just the order of the teams, 0=red, 1=blue, 2=tied
		for ( i = 0;  i < level.numConnectedClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			if ( level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 2;
			} else if ( level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 0;
			} else {
				cl->ps.persistant[PERS_RANK] = 1;
			}
		}
	} else {	
		rank = -1;
		score = 0;
		for ( i = 0;  i < level.numPlayingClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			newScore = cl->ps.persistant[PERS_SCORE];
			if ( i == 0 || newScore != score ) {
				rank = i;
				// assume we aren't tied until the next client is checked
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank;
			} else {
				// we are tied with the previous client
				level.clients[ level.sortedClients[i-1] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
			score = newScore;
			if ( g_gametype.integer == GT_SINGLE_PLAYER && level.numPlayingClients == 1 ) {
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
		}
	}

	// set the CS_SCORES1/2 configstrings, which will be visible to everyone
	if ( g_gametype.integer >= GT_TEAM ) {
		trap_SetConfigstring( CS_SCORES1, va("%i", level.teamScores[TEAM_RED] ) );
		trap_SetConfigstring( CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE] ) );
	} else {
		if ( level.numConnectedClients == 0 ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", SCORE_NOT_PRESENT) );
			trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else if ( level.numConnectedClients == 1 ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", level.clients[ level.sortedClients[1] ].ps.persistant[PERS_SCORE] ) );
		}

		if (g_gametype.integer != GT_DUEL || g_gametype.integer != GT_POWERDUEL)
		{ //when not in duel, use this configstring to pass the index of the player currently in first place
			if ( level.numConnectedClients >= 1 )
			{
				trap_SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", level.sortedClients[0] ) );
			}
			else
			{
				trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
	}

	// see if it is time to end the level
	CheckExitRules();

	// if we are at the intermission or in multi-frag Duel game mode, send the new info to everyone
	if ( level.intermissiontime || g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL ) {
		gQueueScoreMessage = qtrue;
		gQueueScoreMessageTime = level.time + 500;
		//rww - Made this operate on a "queue" system because it was causing large overflows
	}
}

int getOrder(int* topArray, int value, qboolean biggerIsBetter){
	if (biggerIsBetter){
		if (value >= topArray[0]) //bettter than best
			return 0;
		else if (value >= topArray[1])
			return 1;
		else if (value >= topArray[2])
			return 2;
	} else {
		if (value <= topArray[0]) //bettter than best
			return 0;
		else if (value <= topArray[1])
			return 1;
		else if (value <= topArray[2])
			return 2;
	}

	return 3;
}

/*
========================================================================

MAP CHANGING

========================================================================
*/

/*
========================
SendScoreboardMessageToAllClients

Do this at BeginIntermission time and whenever ranks are recalculated
due to enters/exits/forced team changes
========================
*/
void SendScoreboardMessageToAllClients( void ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[ i ].pers.connected == CON_CONNECTED ) {
			DeathmatchScoreboardMessage( g_entities + i );
		}
	}
}

/*
========================
MoveClientToIntermission

When the intermission starts, this will be called for all players.
If a new client connects, this will be called after the spawn function.
========================
*/
void MoveClientToIntermission( gentity_t *ent ) {
	// take out of follow mode if needed
	if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		StopFollowing( ent );
	}


	// move to the spot
	VectorCopy( level.intermission_origin, ent->s.origin );
	VectorCopy( level.intermission_origin, ent->client->ps.origin );
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	ent->client->ps.pm_type = PM_INTERMISSION;

	// clean up powerup info
	memset( ent->client->ps.powerups, 0, sizeof(ent->client->ps.powerups) );

	ent->client->ps.eFlags = 0;
	ent->s.eFlags = 0;
	ent->s.eType = ET_GENERAL;
	ent->s.modelindex = 0;
	ent->s.loopSound = 0;
	ent->s.loopIsSoundset = qfalse;
	ent->s.event = 0;
	ent->r.contents = 0;
}

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
extern qboolean	gSiegeRoundBegun;
extern qboolean	gSiegeRoundEnded;
extern qboolean	gSiegeRoundWinningTeam;
void FindIntermissionPoint( void ) {
	gentity_t	*ent = NULL;
	gentity_t	*target;
	vec3_t		dir;

	// find the intermission spot
	if ( g_gametype.integer == GT_SIEGE
		&& level.intermissiontime
		&& level.intermissiontime <= level.time
		&& gSiegeRoundEnded )
	{
	   	if (gSiegeRoundWinningTeam == SIEGETEAM_TEAM1)
		{
			ent = G_Find (NULL, FOFS(classname), "info_player_intermission_red");
			if ( ent && ent->target2 ) 
			{
				G_UseTargets2( ent, ent, ent->target2 );
			}
		}
	   	else if (gSiegeRoundWinningTeam == SIEGETEAM_TEAM2)
		{
			ent = G_Find (NULL, FOFS(classname), "info_player_intermission_blue");
			if ( ent && ent->target2 ) 
			{
				G_UseTargets2( ent, ent, ent->target2 );
			}
		}
	}
	if ( !ent )
	{
		ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	}
	if ( !ent ) {	// the map creator forgot to put in an intermission point...
		SelectSpawnPoint ( vec3_origin, level.intermission_origin, level.intermission_angle, TEAM_SPECTATOR );
	} else {
		VectorCopy (ent->s.origin, level.intermission_origin);
		VectorCopy (ent->s.angles, level.intermission_angle);
		// if it has a target, look towards it
		if ( ent->target ) {
			target = G_PickTarget( ent->target );
			if ( target ) {
				VectorSubtract( target->s.origin, level.intermission_origin, dir );
				vectoangles( dir, level.intermission_angle );
			}
		}
	}

}

qboolean DuelLimitHit(void);

/*
==================
BeginIntermission
==================
*/
//ghost debug

void BeginIntermission( void ) {
	int			i;
	gentity_t	*client;

	if ( level.intermissiontime ) {
		return;		// already active
	}

	trap_Cvar_Set("g_wasRestarted", "0");

	// if in tournement mode, change the wins / losses
	if ( g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL ) {
		trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );

		if (g_gametype.integer != GT_POWERDUEL)
		{
			AdjustTournamentScores();
		}
		if (DuelLimitHit())
		{
			gDuelExit = qtrue;
		}
		else
		{
			gDuelExit = qfalse;
		}
	}

	//*CHANGE 32* printing tops on intermission
	if (g_gametype.integer == GT_CTF){//NYI
	}

	level.intermissiontime = level.time;
	FindIntermissionPoint();

	//what the? Well, I don't want this to happen.

	// move all clients to the intermission point
	for (i=0 ; i< level.maxclients ; i++) {
		client = g_entities + i;
		if (!client->inuse)
			continue;
		// respawn if dead
		if (client->health <= 0) {
			if (g_gametype.integer != GT_POWERDUEL ||
				!client->client ||
				client->client->sess.sessionTeam != TEAM_SPECTATOR)
			{ //don't respawn spectators in powerduel or it will mess the line order all up
				respawn(client);
			}
		}
		MoveClientToIntermission( client );
	}

	// send the current scoring to all clients
	SendScoreboardMessageToAllClients();

}

qboolean DuelLimitHit(void)
{
	int i;
	gclient_t *cl;

	for ( i=0 ; i< g_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}

		if ( g_duel_fraglimit.integer && cl->sess.wins >= g_duel_fraglimit.integer )
		{
			return qtrue;
		}
	}

	return qfalse;
}

void DuelResetWinsLosses(void)
{
	int i;
	gclient_t *cl;

	for ( i=0 ; i< g_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}

		cl->sess.wins = 0;
		cl->sess.losses = 0;
	}
}

/*
=============
ExitLevel

When the intermission has been exited, the server is either killed
or moved to a new level based on the "nextmap" cvar 

=============
*/
extern void SiegeDoTeamAssign(void); //g_saga.c
extern siegePers_t g_siegePersistant; //g_saga.c
void ExitLevel (void) {
	int		i;
	gclient_t *cl;

	// if we are running a tournement map, kick the loser to spectator status,
	// which will automatically grab the next spectator and restart
	if ( g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL ) {
		if (!DuelLimitHit())
		{
			if ( !level.restarted ) {
				trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
				level.restarted = qtrue;
				level.changemap = NULL;
				level.intermissiontime = 0;
			}
			return;	
		}

		DuelResetWinsLosses();
	}


	if (g_gametype.integer == GT_SIEGE &&
		g_siegeTeamSwitch.integer &&
		g_siegePersistant.beatingTime)
	{ //restart same map...
		trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
	}
	else
	{
		trap_SendConsoleCommand( EXEC_APPEND, "vstr nextmap\n" );
	}
	level.changemap = NULL;
	level.intermissiontime = 0;

	if (g_gametype.integer == GT_SIEGE &&
		g_siegeTeamSwitch.integer)
	{ //switch out now
		SiegeDoTeamAssign();
	}

	// reset all the scores so we don't enter the intermission again
	level.teamScores[TEAM_RED] = 0;
	level.teamScores[TEAM_BLUE] = 0;
	for ( i=0 ; i< g_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.persistant[PERS_SCORE] = 0;
	}

	// we need to do this here before chaning to CON_CONNECTING
	G_WriteSessionData();

	// change all client states to connecting, so the early players into the
	// next level will know the others aren't done reconnecting
	for (i=0 ; i< g_maxclients.integer ; i++) {
		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
			level.clients[i].pers.connected = CON_CONNECTING;
		}
	}

}

/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
void QDECL G_LogPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		string[1024];
	int			/*t,*/ min, sec, len;
	static time_t rawtime;
	static struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = gmtime( &rawtime );

	sec = (level.time - level.startTime)/1000;
	min = sec/60;
	sec %= 60;

	len = sprintf(string,"[%02i:%02i:%02i;%i;%i:%02i] ",
		timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec /*real time UTC*/, 
		level.time /*server time (ms)*/,
		min,sec /*game time*/);


	va_start( argptr, fmt );
	vsprintf( string +len , fmt,argptr );
	va_end( argptr );

	if ( g_dedicated.integer ) {
		G_Printf( "%s", string + len );
	}

	if ( !level.logFile ) {
		return;
	}

	trap_FS_Write( string, strlen( string ), level.logFile );
}

static char* getCurrentTime()
{
	time_t rawtime;
	struct tm *timeinfo;
	static char buffer[128];

	time ( &rawtime );
	timeinfo = gmtime ( &rawtime );

	strftime(buffer, sizeof(buffer), "UTC %a %b %d %H:%M:%S %Y", timeinfo);
	return buffer;
}

//CRASH LOG
void QDECL G_HackLog( const char *fmt, ... ) {
	va_list		argptr;
	static char		string[2048];
	int len;

	if ( !level.hackLogFile && !level.logFile) {
		return;
	}

	len = Com_sprintf( string, sizeof(string), va("[%s] ",getCurrentTime()) );

	va_start( argptr, fmt );
	vsprintf( string+len, fmt, argptr );
	va_end( argptr );

	if ( g_dedicated.integer ) {
		G_Printf( "%s", string + len );
	}
		
	if (level.hackLogFile){
		trap_FS_Write( string, strlen( string ), level.hackLogFile );
	}

}

//DB-ACCOUNTS log
void QDECL G_DBLog( const char *fmt, ... ) {
	va_list		argptr;
	static char		string[2048];
	int len;

	if ( !level.DBLogFile ) {
		return;
	}

	len = Com_sprintf( string, sizeof(string), va("[%s] ",getCurrentTime()) );

	va_start( argptr, fmt );
	vsprintf( string+len, fmt, argptr );
	va_end( argptr );

	trap_FS_Write( string, strlen( string ), level.DBLogFile );
}

//RCON log
void QDECL G_RconLog( const char *fmt, ... ) {
	va_list		argptr;
	static char		string[2048];
	int len;

	if ( !level.rconLogFile) {
		return;
	}

	len = Com_sprintf( string, sizeof(string), va("[%s] ",getCurrentTime()) );

	va_start( argptr, fmt );
	vsprintf( string+len, fmt, argptr );
	va_end( argptr );

	trap_FS_Write( string, strlen( string ), level.rconLogFile );
}

/*
================
LogExit

Append information about this game to the log file
================
*/
void LogExit( const char *string ) {
	int				i, numSorted;
	gclient_t		*cl;
	G_LogPrintf( "Exit: %s\n", string );

	level.intermissionQueued = level.time;


	// this will keep the clients from playing any voice sounds
	// that will get cut off when the queued intermission starts
	trap_SetConfigstring( CS_INTERMISSION, "1" );

	// don't send more than 32 scores (FIXME?)
	numSorted = level.numConnectedClients;
	if ( numSorted > 32 ) {
		numSorted = 32;
	}

	if ( g_gametype.integer >= GT_TEAM ) {
		G_LogPrintf( "red:%i  blue:%i\n",
			level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( cl->pers.connected == CON_CONNECTING ) {
			continue;
		}

		if ((cl->ps.powerups[PW_BLUEFLAG] || cl->ps.powerups[PW_REDFLAG])){
			cl->pers.teamState.flaghold += (level.time - cl->pers.teamState.flagsince);
		}

		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

		G_LogPrintf( "score: %i  ping: %i  client: %i %s\n", cl->ps.persistant[PERS_SCORE], ping, level.sortedClients[i],	cl->pers.netname );
	}

}

qboolean gDidDuelStuff = qfalse; //gets reset on game reinit

/*
=================
CheckIntermissionExit

The level will stay at the intermission for a minimum of 5 seconds
If all players wish to continue, the level will then exit.
If one or more players have not acknowledged the continue, the game will
wait 10 seconds before going on.
=================
*/
void CheckIntermissionExit( void ) {
	int			ready, notReady;
	int			i;
	gclient_t	*cl;
	int			readyMask;

	// see which players are ready
	ready = 0;
	notReady = 0;
	readyMask = 0;
	for (i=0 ; i< g_maxclients.integer ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT ) {
			continue;
		}

		if ( cl->readyToExit ) {
			ready++;
			if ( i < 16 ) {
				readyMask |= 1 << i;
			}
		} else {
			notReady++;
		}
	}

	if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) && !gDidDuelStuff &&
		(level.time > level.intermissiontime + 2000) )
	{
		gDidDuelStuff = qtrue;

		if ( g_austrian.integer && g_gametype.integer != GT_POWERDUEL )
		{
			G_LogPrintf("Duel Results:\n");
			G_LogPrintf("winner: %s, score: %d, wins/losses: %d/%d\n", 
				level.clients[level.sortedClients[0]].pers.netname,
				level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE],
				level.clients[level.sortedClients[0]].sess.wins,
				level.clients[level.sortedClients[0]].sess.losses );
			G_LogPrintf("loser: %s, score: %d, wins/losses: %d/%d\n", 
				level.clients[level.sortedClients[1]].pers.netname,
				level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE],
				level.clients[level.sortedClients[1]].sess.wins,
				level.clients[level.sortedClients[1]].sess.losses );
		}
		// if we are running a tournement map, kick the loser to spectator status,
		// which will automatically grab the next spectator and restart
		if (!DuelLimitHit())
		{
			if (g_gametype.integer == GT_POWERDUEL)
			{
				RemovePowerDuelLosers();
				AddPowerDuelPlayers();
			}
			else
			{
				if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
					level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
					level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
					level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
				{
					RemoveDuelDrawLoser();
				}
				else
				{
					RemoveTournamentLoser();
				}
				AddTournamentPlayer();
			}

			if ( g_austrian.integer )
			{
				if (g_gametype.integer == GT_POWERDUEL)
				{
					G_LogPrintf("Power Duel Initiated: %s %d/%d vs %s %d/%d and %s %d/%d, kill limit: %d\n", 
						level.clients[level.sortedClients[0]].pers.netname,
						level.clients[level.sortedClients[0]].sess.wins,
						level.clients[level.sortedClients[0]].sess.losses,
						level.clients[level.sortedClients[1]].pers.netname,
						level.clients[level.sortedClients[1]].sess.wins,
						level.clients[level.sortedClients[1]].sess.losses,
						level.clients[level.sortedClients[2]].pers.netname,
						level.clients[level.sortedClients[2]].sess.wins,
						level.clients[level.sortedClients[2]].sess.losses,
						g_fraglimit.integer );
				}
				else
				{
					G_LogPrintf("Duel Initiated: %s %d/%d vs %s %d/%d, kill limit: %d\n", 
						level.clients[level.sortedClients[0]].pers.netname,
						level.clients[level.sortedClients[0]].sess.wins,
						level.clients[level.sortedClients[0]].sess.losses,
						level.clients[level.sortedClients[1]].pers.netname,
						level.clients[level.sortedClients[1]].sess.wins,
						level.clients[level.sortedClients[1]].sess.losses,
						g_fraglimit.integer );
				}
			}
			
			if (g_gametype.integer == GT_POWERDUEL)
			{
				if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
				{
					trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
					trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
				}			
			}
			else
			{
				if (level.numPlayingClients >= 2)
				{
					trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
					trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
				}
			}

			return;	
		}

		if ( g_austrian.integer && g_gametype.integer != GT_POWERDUEL )
		{
			G_LogPrintf("Duel Tournament Winner: %s wins/losses: %d/%d\n", 
				level.clients[level.sortedClients[0]].pers.netname,
				level.clients[level.sortedClients[0]].sess.wins,
				level.clients[level.sortedClients[0]].sess.losses );
		}

		if (g_gametype.integer == GT_POWERDUEL)
		{
			RemovePowerDuelLosers();
			AddPowerDuelPlayers();

			if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
			{
				trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
				trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
		else
		{
			//this means we hit the duel limit so reset the wins/losses
			//but still push the loser to the back of the line, and retain the order for
			//the map change
			if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
				level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
				level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
				level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
			{
				RemoveDuelDrawLoser();
			}
			else
			{
				RemoveTournamentLoser();
			}

			AddTournamentPlayer();

			if (level.numPlayingClients >= 2)
			{
				trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
				trap_SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
	}

	if ((g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) && !gDuelExit)
	{ //in duel, we have different behaviour for between-round intermissions
		if ( level.time > level.intermissiontime + 4000 )
		{ //automatically go to next after 4 seconds
			ExitLevel();
			return;
		}

		for (i=0 ; i< g_maxclients.integer ; i++)
		{ //being in a "ready" state is not necessary here, so clear it for everyone
		  //yes, I also thinking holding this in a ps value uniquely for each player
		  //is bad and wrong, but it wasn't my idea.
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED )
			{
				continue;
			}
			cl->ps.stats[STAT_CLIENTS_READY] = 0;
		}
		return;
	}

	// copy the readyMask to each player's stats so
	// it can be displayed on the scoreboard
	for (i=0 ; i< g_maxclients.integer ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.stats[STAT_CLIENTS_READY] = readyMask;
	}

	// never exit in less than five seconds
	if ( level.time < level.intermissiontime + 5000 ) {
		return;
	}

    // always exit at most after fifteen seconds
    if ( level.time > level.intermissiontime + 15000 )
    {
        ExitLevel();
        return;
    }

	if (d_noIntermissionWait.integer)
	{ //don't care who wants to go, just go.
		ExitLevel();
		return;
	}

	// if nobody wants to go, clear timer
	if ( !ready ) {
		level.readyToExit = qfalse;
		return;
	}

	// if everyone wants to go, go now
	if ( !notReady ) {
		ExitLevel();
		return;
	}

	// the first person to ready starts the ten second timeout
	if ( !level.readyToExit ) {
		level.readyToExit = qtrue;
		level.exitTime = level.time;
	}

	// if we have waited ten seconds since at least one player
	// wanted to exit, go ahead
	if ( level.time < level.exitTime + 10000 ) {
		return;
	}

	ExitLevel();
}

/*
=============
ScoreIsTied
=============
*/
qboolean ScoreIsTied( void ) {
	int		a, b;

	if (level.numPlayingClients < 2 ) {
		return qfalse;
	}
	
	if ( g_gametype.integer >= GT_TEAM ) {
		return level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE];
	}

	a = level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE];
	b = level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE];

	return a == b;
}

int findClient(const char* username){
	int i;
	gentity_t* ent;

	for(i=0;i<level.maxclients;++i){
		ent = &g_entities[i];

		if (!ent->inuse || !ent->client)
			continue;

		if (!Q_stricmpn(ent->client->sess.username,username,MAX_USERNAME_SIZE)){
			//usernames match
			return i;
		}
	}

	return -1; //not found
}

//match report, for db accounts system
//void reportMatch(){
//	int playersLeft;
//	int reported;
//	static char buffer[1024];
//	char mapname[128];
//	gclient_t*	client;
//	char* str;
//	char* dest;
//	int i;
//	int len;
//
//	//G_LogPrintf("reportMatch()\n");
//
//	if (!level.initialConditionsMatch)
//		return;
//
//	//conditions match, lets send roster to database
//	//format: 
//    // red1:red2:...:redm blue1:blue2:...:bluen redscore bluescore map wasOvertime
//	//example:
//	// sil:sal:ramses:norb jax:lando:fp:rst 11 8 mp/ctf4 0
//
//	//report players from original roster list, dont count newly joined
//	//red players roster report
//
//	dest = buffer;
//
//	//report REDs
//	strncpy(dest,"reds=",5);
//	dest += 5;
//
//	reported = 0;
//	playersLeft = 0;
//	for(i=0;i<level.initialRedCount;++i){
//		client = &g_clients[level.initialRedRoster[i].clientNum];
//
//		if (strncmp(client->sess.username,level.initialRedRoster[i].username,MAX_USERNAME_SIZE)){
//			//username doesnt match, try to find this player somewhere else,
//			//he might possibly reconnect with another number
//			int num = findClient(level.initialRedRoster[i].username);	
//
//			if (num == -1) //not found
//				continue; //TODO: include for check of how long time before end did player left
//
//			//ok user found
//			client = &g_clients[num];			
//		}
//
//		if ((client->sess.sessionTeam != TEAM_RED)
//			|| (client->pers.connected == CON_DISCONNECTED)){
//			//this guy left his team
//			++playersLeft;
//			continue; //TODO: include for check of how long time before end did player left
//		}
//
//		if (reported){
//			*dest = ':';
//			++dest;
//		}
//
//		len = strlen(client->sess.username);
//		strncpy(dest,client->sess.username,len);
//		dest += len;
//		++reported;
//	}
//
//	strncpy(dest,"&blues=",7);
//	dest += 7;
//
//	//report BLUEs
//	reported = 0;
//	//playersLeft = 0;
//	for(i=0;i<level.initialBlueCount;++i){
//		client = &g_clients[level.initialBlueRoster[i].clientNum];
//
//		if (strncmp(client->sess.username,level.initialBlueRoster[i].username,MAX_USERNAME_SIZE)){
//			//username doesnt match, try to find this player somewhere else,
//			//he might possibly reconnect with another number
//			int num = findClient(level.initialBlueRoster[i].username);	
//
//			if (num == -1) //not found in present players
//				continue; //TODO: include for check of how long time before end did player left
//
//			//ok user found
//			client = &g_clients[num];	
//		}
//
//		if ((client->sess.sessionTeam != TEAM_BLUE)
//			|| (client->pers.connected == CON_DISCONNECTED)){
//			//this guy left his team
//			++playersLeft;
//			continue; //TODO: include for check of how long time before end did player left
//		}
//
//		if (reported){
//			*dest = ':';
//			++dest;
//		}
//
//		len = strlen(client->sess.username);
//		strncpy(dest,client->sess.username,len);
//		dest += len;
//		++reported;
//	}
//
//	//more than 2 players left original teams, dont report...
//	if (playersLeft > 2)
//		return;
//
//	//report scores
//	strncpy(dest,"&redscore=",10);
//	dest += 10;
//
//	str = va("%i",level.teamScores[TEAM_RED]); //reds
//	len = strlen(str);
//	strncpy(dest,str,len); 
//	dest += len;
//
//	strncpy(dest,"&bluescore=",11);
//	dest += 11;
//
//	str = va("%i",level.teamScores[TEAM_BLUE]); //blues
//	len = strlen(str);
//	strncpy(dest,str,len); 
//	dest += len;
//
//	strncpy(dest,"&map=",5);
//	dest += 5;
//
//	//report map name
//	trap_Cvar_VariableStringBuffer("mapname",mapname,sizeof(mapname));
//	len = strlen(mapname);
//	strncpy(dest,mapname,len);
//	dest += len;
//
//	strncpy(dest,"&ot=",4);
//	dest += 4;
//
//	//report whether this game went overtime
//	if (level.overtime){
//		*dest = '1';
//	} else {
//		*dest = '0';
//	}
//	++dest;
//
//	*dest = '\0';
//
//	sendMatchResult(buffer);
//
//}

/*
=================
CheckExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag.
=================
*/
qboolean g_endPDuel = qfalse;
void CheckExitRules( void ) {
 	int			i;
	gclient_t	*cl;
	char *sKillLimit;
	qboolean printLimit = qtrue;
	// if at the intermission, wait for all non-bots to
	// signal ready, then go to next level
	if ( level.intermissiontime ) {
		CheckIntermissionExit ();
		return;
	}

	if (gDoSlowMoDuel)
	{ //don't go to intermission while in slow motion
		return;
	}

	if (gEscaping)
	{
		int i = 0;
		int numLiveClients = 0;

		while (i < MAX_CLIENTS)
		{
			if (g_entities[i].inuse && g_entities[i].client && g_entities[i].health > 0)
			{
				if (g_entities[i].client->sess.sessionTeam != TEAM_SPECTATOR &&
					!(g_entities[i].client->ps.pm_flags & PMF_FOLLOW))
				{
					numLiveClients++;
				}
			}

			i++;
		}
		if (gEscapeTime < level.time)
		{
			gEscaping = qfalse;
			LogExit( "Escape time ended." );
			return;
		}
		if (!numLiveClients)
		{
			gEscaping = qfalse;
			LogExit( "Everyone failed to escape." );
			return;
		}
	}

	if ( level.intermissionQueued ) {
		if ( level.time - level.intermissionQueued >= INTERMISSION_DELAY_TIME ) {
			level.intermissionQueued = 0;
			BeginIntermission();
		}
				return;
			}

	// check for sudden death
	if (g_gametype.integer != GT_SIEGE)
	{
		
		if ( ScoreIsTied() ) {

			//log we came overtime
			if ( level.time - level.startTime >= g_timelimit.integer*60000 ){
				level.overtime = qtrue;
			}

			// always wait for sudden death
			if ((g_gametype.integer != GT_DUEL) || !g_timelimit.integer)
			{
				if (g_gametype.integer != GT_POWERDUEL)
				{
					return;
				}
			}
		}
	}

	if (g_gametype.integer != GT_SIEGE)
	{
		if ( g_timelimit.integer && !level.warmupTime ) {
			if ( level.time - level.startTime >= g_timelimit.integer*60000 ) {
				trap_SendServerCommand( -1, va("print \"%s.\n\"",G_GetStringEdString("MP_SVGAME", "TIMELIMIT_HIT")));
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Timelimit hit (1)\n");
				}
				LogExit( "Timelimit hit." );

				if (g_gametype.integer == GT_CTF && g_accounts.integer){
					// match log - accounts system
					//reportMatch();
				}

				return;
			}
		}
	}

	if (g_gametype.integer == GT_POWERDUEL && level.numPlayingClients >= 3)
	{
		if (g_endPDuel)
		{
			g_endPDuel = qfalse;
			LogExit("Powerduel ended.");
		}

		return;
	}

	if ( level.numPlayingClients < 2 ) {
		return;
	}

	if (g_gametype.integer == GT_DUEL ||
		g_gametype.integer == GT_POWERDUEL)
	{
		if (g_fraglimit.integer > 1)
		{
			sKillLimit = "Kill limit hit.";
		}
		else
		{
			sKillLimit = "";
			printLimit = qfalse;
		}
	}
	else
	{
		sKillLimit = "Kill limit hit.";
	}
	if ( g_gametype.integer < GT_SIEGE && g_fraglimit.integer ) {
		if ( level.teamScores[TEAM_RED] >= g_fraglimit.integer ) {
			trap_SendServerCommand( -1, va("print \"Red %s\n\"", G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")) );
			if (d_powerDuelPrint.integer)
			{
				Com_Printf("POWERDUEL WIN CONDITION: Kill limit (1)\n");
			}
			LogExit( sKillLimit );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= g_fraglimit.integer ) {
			trap_SendServerCommand( -1, va("print \"Blue %s\n\"", G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")) );
			if (d_powerDuelPrint.integer)
			{
				Com_Printf("POWERDUEL WIN CONDITION: Kill limit (2)\n");
			}
			LogExit( sKillLimit );
			return;
		}

		for ( i=0 ; i< g_maxclients.integer ; i++ ) {
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			if ( cl->sess.sessionTeam != TEAM_FREE ) {
				continue;
			}

			if ( (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) && g_duel_fraglimit.integer && cl->sess.wins >= g_duel_fraglimit.integer )
			{
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Duel limit hit (1)\n");
				}
				LogExit( "Duel limit hit." );
				gDuelExit = qtrue;
				trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " hit the win limit.\n\"",
					cl->pers.netname ) );
				return;
			}

			if ( cl->ps.persistant[PERS_SCORE] >= g_fraglimit.integer ) {
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Kill limit (3)\n");
				}
				LogExit( sKillLimit );
				gDuelExit = qfalse;
				if (printLimit)
				{
					trap_SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " %s.\n\"",
													cl->pers.netname,
													G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")
													) 
											);
				}
				return;
			}
		}
	}

    if (g_gametype.integer >= GT_CTF && g_capturedifflimit.integer) {

        if (level.teamScores[TEAM_RED] - level.teamScores[TEAM_BLUE] >= g_capturedifflimit.integer)
        {
            trap_SendServerCommand(-1, va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")));
            trap_SendServerCommand(-1, va("print \"%s ^7- Team ^1RED ^7leads by ^2%i^7.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT"), g_capturedifflimit.integer));
            LogExit("Capture difference limit hit.");
				return;
			}

        if (level.teamScores[TEAM_BLUE] - level.teamScores[TEAM_RED] >= g_capturedifflimit.integer) {
            trap_SendServerCommand(-1, va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")));
            trap_SendServerCommand(-1, va("print \"%s ^7- Team ^4BLUE ^7leads by ^2%i^7.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT"), g_capturedifflimit.integer));
            LogExit("Capture difference limit hit.");
            return;
		}
	}

	if ( g_gametype.integer >= GT_CTF && g_capturelimit.integer ) {

		if ( level.teamScores[TEAM_RED] >= g_capturelimit.integer ) 
		{
			trap_SendServerCommand( -1,  va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")));
			trap_SendServerCommand( -1,  va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT")));
			LogExit( "Capturelimit hit." );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= g_capturelimit.integer ) {
			trap_SendServerCommand( -1,  va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")));
			trap_SendServerCommand( -1,  va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT")));
			LogExit( "Capturelimit hit." );
			return;
		}
	}

}



/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/

void G_RemoveDuelist(int team)
{
	int i = 0;
	gentity_t *ent;
	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->client && ent->client->sess.sessionTeam != TEAM_SPECTATOR &&
			ent->client->sess.duelTeam == team)
		{
			SetTeam(ent, "s");
		}
        i++;
	}
}

/*
=============
CheckTournament

Once a frame, check for changes in tournement player state
=============
*/
int g_duelPrintTimer = 0;
void CheckTournament( void ) {
	// check because we run 3 game frames before calling Connect and/or ClientBegin
	// for clients on a map_restart

	if (g_gametype.integer == GT_POWERDUEL)
	{
		if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
		{
			trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
		}
	}
	else
	{
		if (level.numPlayingClients >= 2)
		{
			trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
		}
	}

	if ( g_gametype.integer == GT_DUEL )
	{
		// pull in a spectator if needed
		if ( level.numPlayingClients < 2 && !level.intermissiontime && !level.intermissionQueued ) {
			AddTournamentPlayer();

			if (level.numPlayingClients >= 2)
			{
				trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
			}
		}

		if (level.numPlayingClients >= 2)
		{
// nmckenzie: DUEL_HEALTH
			if ( g_showDuelHealths.integer >= 1 )
			{
				playerState_t *ps1, *ps2;
				ps1 = &level.clients[level.sortedClients[0]].ps;
				ps2 = &level.clients[level.sortedClients[1]].ps;
				trap_SetConfigstring ( CS_CLIENT_DUELHEALTHS, va("%i|%i|!", 
					ps1->stats[STAT_HEALTH], ps2->stats[STAT_HEALTH]));
			}
		}

		//rww - It seems we have decided there will be no warmup in duel.
		//if (!g_warmup.integer)
		{ //don't care about any of this stuff then, just add people and leave me alone
			level.warmupTime = 0;
			return;
		}
#if 0
		// if we don't have two players, go back to "waiting for players"
		if ( level.numPlayingClients != 2 ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return;
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			if ( level.numPlayingClients == 2 ) {
				// fudge by -1 to account for extra delays
				level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;

				if (level.warmupTime < (level.time + 3000))
				{ //rww - this is an unpleasent hack to keep the level from resetting completely on the client (this happens when two map_restarts are issued rapidly)
					level.warmupTime = level.time + 3000;
				}
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			}
			return;
		}

		// if the warmup time has counted down, restart
		if ( level.time > level.warmupTime ) {
			level.warmupTime += 10000;
			trap_Cvar_Set( "g_restarted", "1" );
			trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			return;
		}
#endif
	}
	else if (g_gametype.integer == GT_POWERDUEL)
	{
		if (level.numPlayingClients < 2)
		{ //hmm, ok, pull more in.
			g_dontFrickinCheck = qfalse;
		}

		if (level.numPlayingClients > 3)
		{ //umm..yes..lets take care of that then.
			int lone = 0, dbl = 0;

			G_PowerDuelCount(&lone, &dbl, qfalse);
			if (lone > 1)
			{
				G_RemoveDuelist(DUELTEAM_LONE);
			}
			else if (dbl > 2)
			{
				G_RemoveDuelist(DUELTEAM_DOUBLE);
			}
		}
		else if (level.numPlayingClients < 3)
		{ //hmm, someone disconnected or something and we need em
			int lone = 0, dbl = 0;

			G_PowerDuelCount(&lone, &dbl, qfalse);
			if (lone < 1)
			{
				g_dontFrickinCheck = qfalse;
			}
			else if (dbl < 1)
			{
				g_dontFrickinCheck = qfalse;
			}
		}

		// pull in a spectator if needed
		if (level.numPlayingClients < 3 && !g_dontFrickinCheck)
		{
			AddPowerDuelPlayers();

			if (level.numPlayingClients >= 3 &&
				G_CanResetDuelists())
			{
				gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_DUEL);
				te->r.svFlags |= SVF_BROADCAST;
				//this is really pretty nasty, but..
				te->s.otherEntityNum = level.sortedClients[0];
				te->s.otherEntityNum2 = level.sortedClients[1];
				te->s.groundEntityNum = level.sortedClients[2];

				trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
				G_ResetDuelists();

				g_dontFrickinCheck = qtrue;
			}
			else if (level.numPlayingClients > 0 ||
				level.numConnectedClients > 0)
			{
				if (g_duelPrintTimer < level.time)
				{ //print once every 10 seconds
					int lone = 0, dbl = 0;

					G_PowerDuelCount(&lone, &dbl, qtrue);
					if (lone < 1)
					{
						trap_SendServerCommand( -1, va("cp \"%s\n\"", G_GetStringEdString("MP_SVGAME", "DUELMORESINGLE")) );
					}
					else
					{
						trap_SendServerCommand( -1, va("cp \"%s\n\"", G_GetStringEdString("MP_SVGAME", "DUELMOREPAIRED")) );
					}
					g_duelPrintTimer = level.time + 10000;
				}
			}

			if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
			{ //pulled in a needed person
				if (G_CanResetDuelists())
				{
					gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_DUEL);
					te->r.svFlags |= SVF_BROADCAST;
					//this is really pretty nasty, but..
					te->s.otherEntityNum = level.sortedClients[0];
					te->s.otherEntityNum2 = level.sortedClients[1];
					te->s.groundEntityNum = level.sortedClients[2];

					trap_SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );

					if ( g_austrian.integer )
					{
						G_LogPrintf("Duel Initiated: %s %d/%d vs %s %d/%d and %s %d/%d, kill limit: %d\n", 
							level.clients[level.sortedClients[0]].pers.netname,
							level.clients[level.sortedClients[0]].sess.wins,
							level.clients[level.sortedClients[0]].sess.losses,
							level.clients[level.sortedClients[1]].pers.netname,
							level.clients[level.sortedClients[1]].sess.wins,
							level.clients[level.sortedClients[1]].sess.losses,
							level.clients[level.sortedClients[2]].pers.netname,
							level.clients[level.sortedClients[2]].sess.wins,
							level.clients[level.sortedClients[2]].sess.losses,
							g_fraglimit.integer );
					}
					//FIXME: This seems to cause problems. But we'd like to reset things whenever a new opponent is set.
				}
			}
		}
		else
		{ //if you have proper num of players then don't try to add again
			g_dontFrickinCheck = qtrue;
		}

		level.warmupTime = 0;
		return;
	}

}

void G_KickAllBots(void)
{
	int i;
	char netname[36];
	gclient_t	*cl;

	for ( i=0 ; i< g_maxclients.integer ; i++ )
	{
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED )
		{
			continue;
		}
		if ( !(g_entities[cl->ps.clientNum].r.svFlags & SVF_BOT) )
		{
			continue;
		}
		strcpy(netname, cl->pers.netname);
		Q_CleanStr(netname);
		trap_SendConsoleCommand( EXEC_INSERT, va("kick \"%s\"\n", netname) );
	}
}

/*
==================
CheckVote
==================
*/
extern void SiegeClearSwitchData(void);

void CheckVote( void ) {
	if ( level.voteExecuteTime && level.voteExecuteTime < level.time ) {
		level.voteExecuteTime = 0;

		//special fix for siege status
		if (!Q_strncmp(level.voteString,"vstr nextmap",sizeof(level.voteString))){
			SiegeClearSwitchData();
		}

		if (!Q_stricmpn(level.voteString,"map_restart",11)){
			trap_Cvar_Set("g_wasRestarted", "1");
		}

        if (!Q_stricmpn(level.voteString, "clientkick", 10)){
            int id = atoi(&level.voteString[11]);

            if ((id < sv_privateclients.integer) && !(g_entities[id].r.svFlags & SVF_BOT))
            {
                if (g_entities[id].client->sess.sessionTeam != TEAM_SPECTATOR)
                {       
                    trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i s\n", id));
                }

                trap_SendServerCommand(-1, va("print \"%s^1 may not be kicked.\n\"", g_entities[id].client->pers.netname));
                return;
            }
        }

		trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.voteString ) );

		if (level.votingGametype)
		{
			if (trap_Cvar_VariableIntegerValue("g_gametype") != level.votingGametypeTo)
			{ //If we're voting to a different game type, be sure to refresh all the map stuff
                const char *nextMap = G_GetDefaultMap(level.votingGametypeTo);

				if (level.votingGametypeTo == GT_SIEGE)
				{ //ok, kick all the bots, cause the aren't supported!
                    G_KickAllBots();
					//just in case, set this to 0 too... I guess...maybe?
					//trap_Cvar_Set("bot_minplayers", "0");
				}

				if (nextMap && nextMap[0])
				{
					trap_SendConsoleCommand( EXEC_APPEND, va("map %s\n", nextMap ) );
				}
			}
			else
			{ //otherwise, just leave the map until a restart
				G_RefreshNextMap(level.votingGametypeTo, qfalse);
			}

			if (g_fraglimitVoteCorrection.integer)
			{ //This means to auto-correct fraglimit when voting to and from duel.
				const int currentGT = trap_Cvar_VariableIntegerValue("g_gametype");
				const int currentFL = trap_Cvar_VariableIntegerValue("fraglimit");
				const int currentTL = trap_Cvar_VariableIntegerValue("timelimit");

				if ((level.votingGametypeTo == GT_DUEL || level.votingGametypeTo == GT_POWERDUEL) && currentGT != GT_DUEL && currentGT != GT_POWERDUEL)
				{
					if (currentFL > 3 || !currentFL)
					{ //if voting to duel, and fraglimit is more than 3 (or unlimited), then set it down to 3
						trap_SendConsoleCommand(EXEC_APPEND, "fraglimit 3\n");
					}
					if (currentTL)
					{ //if voting to duel, and timelimit is set, make it unlimited
						trap_SendConsoleCommand(EXEC_APPEND, "timelimit 0\n");
					}
				}
				else if ((level.votingGametypeTo != GT_DUEL && level.votingGametypeTo != GT_POWERDUEL) &&
					(currentGT == GT_DUEL || currentGT == GT_POWERDUEL))
				{
					if (currentFL && currentFL < 20)
					{ //if voting from duel, an fraglimit is less than 20, then set it up to 20
						trap_SendConsoleCommand(EXEC_APPEND, "fraglimit 20\n");
					}
				}
			}

			level.votingGametype = qfalse;
			level.votingGametypeTo = 0;
		}
	}
	if ( !level.voteTime ) {
		return;
	}
	if ( level.time - level.voteTime >= VOTE_TIME ) {
        if ((g_minimumVotesCount.integer) && (level.numVotingClients % 2 == 0) && (level.voteYes > level.voteNo) && (level.voteYes + level.voteNo >= g_minimumVotesCount.integer)) {
            trap_SendServerCommand(-1, va("print \"%s\n\"",
                G_GetStringEdString("MP_SVGAME", "VOTEPASSED")));

		// log the vote
            G_LogPrintf("Vote passed. (Yes:%i No:%i All:%i g_minimumVotesCount:%i)\n", level.voteYes, level.voteNo, level.numVotingClients, g_minimumVotesCount.integer);

            level.voteExecuteTime = level.time + 3000;
        } else {
            trap_SendServerCommand(-1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEFAILED")));

		// log the vote
		G_LogPrintf("Vote timed out. (Yes:%i No:%i All:%i)\n", level.voteYes, level.voteNo, level.numVotingClients);
        }
	} else {
        if ((g_enforceEvenVotersCount.integer) && (level.numVotingClients % 2 == 1)) {
            if ((g_minVotersForEvenVotersCount.integer > 4) && (level.numVotingClients >= g_minVotersForEvenVotersCount.integer)) {
                if (level.voteYes < level.numVotingClients/2 + 2) {
                    return;
                }
            }
        }

		if ( level.voteYes > level.numVotingClients/2 ) {
			trap_SendServerCommand( -1, va("print \"%s\n\"", 
				G_GetStringEdString("MP_SVGAME", "VOTEPASSED")) );

			// log the vote
			G_LogPrintf("Vote passed. (Yes:%i No:%i All:%i)\n", level.voteYes, level.voteNo, level.numVotingClients);

			level.voteExecuteTime = level.time + 3000;
		} else if ( level.voteNo >= (level.numVotingClients+1)/2 ) {
			// same behavior as a timeout
			trap_SendServerCommand( -1, va("print \"%s\n\"", 
				G_GetStringEdString("MP_SVGAME", "VOTEFAILED")) );

			// log the vote
			G_LogPrintf("Vote failed. (Yes:%i No:%i All:%i)\n", level.voteYes, level.voteNo, level.numVotingClients);
		} else {
			// still waiting for a majority
			return;
		}
	}
	level.voteTime = 0;
	g_entities[level.lastVotingClient].client->lastCallvoteTime = level.time;
	
	trap_SetConfigstring( CS_VOTE_TIME, "" );

}

void CheckReady(void) 
{
    int i = 0, readyCount = 0;
    int botsCount = 0;
    gentity_t *ent = NULL;
    unsigned readyMask = 0;
    static qboolean restarting = qfalse;

    if ((g_gametype.integer == GT_POWERDUEL) || (g_gametype.integer == GT_DUEL) || (g_gametype.integer == GT_SIEGE))
        return;

    // for original functionality of /ready
    // if (!g_doWarmup.integer || !level.numPlayingClients || restarting || level.intermissiontime)
    if (!g_doWarmup.integer || restarting || level.intermissiontime)
        return;

    for (i = 0, ent = g_entities; i < level.maxclients; i++, ent++)
    {
        // for original functionality of /ready
        // if (!ent->inuse || ent->client->pers.connected == CON_DISCONNECTED || ent->client->sess.sessionTeam == TEAM_SPECTATOR)
        if (!ent->inuse || ent->client->pers.connected == CON_DISCONNECTED)
            continue;

        if (ent->client->pers.ready)
        {
            readyCount++;
            if (i < 16)
                readyMask |= (1 << i);
        }

        if (ent->r.svFlags & SVF_BOT)
            ++botsCount;
    }

    // update ready flags for clients' scoreboards
    for (i = 0, ent = g_entities; i < level.maxclients; i++, ent++)
    {
        if (!ent->inuse || ent->client->pers.connected == CON_DISCONNECTED)
            continue;


        ent->client->ps.stats[STAT_CLIENTS_READY] = readyMask;
    }

    // allow this for original functionality of /ready
    // check if all conditions to start the match have been met
    // {
    //    int		counts[TEAM_NUM_TEAMS];
    //    qboolean	conditionsMet = qtrue;
    //
    //    counts[TEAM_BLUE] = TeamCount(-1, TEAM_BLUE);
    //    counts[TEAM_RED] = TeamCount(-1, TEAM_RED);
    //
    //    // eat least 1 player in each team
    //    if (counts[TEAM_RED] < 1 || counts[TEAM_BLUE] < 1 || counts[TEAM_RED] != counts[TEAM_BLUE])
    //    {
    //        conditionsMet = qfalse;
    //    }
    //
    //    // all players are ready
    //    if (readyCount < counts[TEAM_BLUE] + counts[TEAM_RED] - botsCount)
    //    {
    //        conditionsMet = qfalse;
    //    }
    //
    //    if (conditionsMet)
    //    {
    //        trap_Cvar_Set("g_restarted", "1");
    //        trap_Cvar_Set("g_wasRestarted", "1");
    //        trap_SendConsoleCommand(EXEC_APPEND, "map_restart 5\n");
    //        restarting = qtrue;
    //        return;
    //    }
    //    else
    //    {
    //
    //    }
    // }
}

/*
==================
PrintTeam
==================
*/
void PrintTeam(int team, char *message) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		trap_SendServerCommand( i, message );
	}
}

/*
==================
SetLeader
==================
*/
void SetLeader(int team, int client) {
	int i;

	if ( level.clients[client].pers.connected == CON_DISCONNECTED ) {
		PrintTeam(team, va("print \"%s is not connected\n\"", level.clients[client].pers.netname) );
		return;
	}
	if (level.clients[client].sess.sessionTeam != team) {
		PrintTeam(team, va("print \"%s is not on the team anymore\n\"", level.clients[client].pers.netname) );
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader) {
			level.clients[i].sess.teamLeader = qfalse;
			ClientUserinfoChanged(i);
		}
	}
	level.clients[client].sess.teamLeader = qtrue;
	ClientUserinfoChanged( client );
	PrintTeam(team, va("print \"%s %s\n\"", level.clients[client].pers.netname, G_GetStringEdString("MP_SVGAME", "NEWTEAMLEADER")) );
}

/*
==================
CheckTeamLeader
==================
*/
void CheckTeamLeader( int team ) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader)
			break;
	}
	if (i >= level.maxclients) {
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if (level.clients[i].sess.sessionTeam != team)
				continue;
			if (!(g_entities[i].r.svFlags & SVF_BOT)) {
				level.clients[i].sess.teamLeader = qtrue;
				break;
			}
		}
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if (level.clients[i].sess.sessionTeam != team)
				continue;
			level.clients[i].sess.teamLeader = qtrue;
			break;
		}
	}
}

/*
==================
CheckTeamVote
==================
*/
void CheckTeamVote( int team ) {
	int cs_offset;

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		return;
	}
	if ( level.time - level.teamVoteTime[cs_offset] >= VOTE_TIME ) {
		trap_SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEFAILED")) );
	} else {
		if ( level.teamVoteYes[cs_offset] > level.numteamVotingClients[cs_offset]/2 ) {
			// execute the command, then remove the vote
			trap_SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEPASSED")) );
			//
			if ( !Q_strncmp( "leader", level.teamVoteString[cs_offset], 6) ) {
				//set the team leader
			}
			else {
				trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.teamVoteString[cs_offset] ) );
			}
		} else if ( level.teamVoteNo[cs_offset] >= level.numteamVotingClients[cs_offset]/2 ) {
			// same behavior as a timeout
			trap_SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEFAILED")) );
		} else {
			// still waiting for a majority
			return;
		}
	}
	level.teamVoteTime[cs_offset] = 0;
	trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, "" );

}


/*
==================
CheckCvars
==================
*/
void CheckCvars( void ) {
	static int lastMod = -1;
	
	if ( g_password.modificationCount != lastMod ) {
		char password[MAX_INFO_STRING];
		char *c = password;
		lastMod = g_password.modificationCount;
		
		strcpy( password, g_password.string );
		while( *c )
		{
			if ( *c == '%' )
			{
				*c = '.';
			}
			c++;
		}
		trap_Cvar_Set("g_password", password );

		if ( (*g_password.string && Q_stricmp( g_password.string, "none" ))
			/*|| isDBLoaded */ // accounts system
			) 
		{
			trap_Cvar_Set( "g_needpass", "1" );
		} else {
			trap_Cvar_Set( "g_needpass", "0" );
		}
	}
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
extern void proxMineThink(gentity_t *ent);
extern void SiegeItemThink( gentity_t *ent );

void G_RunThink (gentity_t *ent) {
	int	thinktime;

    //OSP: pause
    //      If paused, push nextthink
    if ( level.pause.state != PAUSE_NONE) { 
		if ( ent-g_entities >= g_maxclients.integer && ent->nextthink > level.time )
            ent->nextthink += level.time - level.previousTime;

		// special case, mines need update here
		if ( ent->think == proxMineThink && ent->genericValue15 > level.time)
			ent->genericValue15 += level.time - level.previousTime;

        // another special case, siege items need respawn timer update
        if ( ent->think == SiegeItemThink && ent->genericValue9 > level.time )
            ent->genericValue9 += level.time - level.previousTime;

	}

	thinktime = ent->nextthink;
	if (thinktime <= 0) {
		goto runicarus;
	}
	if (thinktime > level.time) {
		goto runicarus;
	}
	

	ent->nextthink = 0;
	if (!ent->think) {
		goto runicarus;
	}
	ent->think (ent);

runicarus:
	if ( ent->inuse )
	{
		trap_ICARUS_MaintainTaskManager(ent->s.number);
	}
}

int g_LastFrameTime = 0;
int g_TimeSinceLastFrame = 0;

qboolean gDoSlowMoDuel = qfalse;
int gSlowMoDuelTime = 0;

//#define _G_FRAME_PERFANAL

void NAV_CheckCalcPaths( void )
{	
	if ( navCalcPathTime && navCalcPathTime < level.time )
	{//first time we've ever loaded this map...
		vmCvar_t	mapname;
		vmCvar_t	ckSum;

		trap_Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
		trap_Cvar_Register( &ckSum, "sv_mapChecksum", "", CVAR_ROM );

		//clear all the failed edges
		trap_Nav_ClearAllFailedEdges();

		//Calculate all paths
		NAV_CalculatePaths( mapname.string, ckSum.integer );
		
		trap_Nav_CalculatePaths(qfalse);

#ifndef FINAL_BUILD
		if ( fatalErrors )
		{
			Com_Printf( S_COLOR_RED"Not saving .nav file due to fatal nav errors\n" );
		}
		else 
#endif
#ifndef _XBOX
		if ( trap_Nav_Save( mapname.string, ckSum.integer ) == qfalse )
		{
			Com_Printf("Unable to save navigations data for map \"%s\" (checksum:%d)\n", mapname.string, ckSum.integer );
		}
#endif
		navCalcPathTime = 0;
	}
}

//so shared code can get the local time depending on the side it's executed on
#include "namespace_begin.h"
int BG_GetTime(void)
{
	return level.time;
}
#include "namespace_end.h"

/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
void ClearNPCGlobals( void );
void AI_UpdateGroups( void );
void ClearPlayerAlertEvents( void );
void SiegeCheckTimers(void);
void WP_SaberStartMissileBlockCheck( gentity_t *self, usercmd_t *ucmd );
extern void Jedi_Decloak( gentity_t *self );
qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs );

int g_siegeRespawnCheck = 0;

extern unsigned getstatus_LastIP;
extern int getstatus_TimeToReset;
extern int getstatus_Counter;
extern int getstatus_LastReportTime;
extern int getstatus_UniqueIPCount;
extern int gImperialCountdown;
extern int gRebelCountdown;

void G_RunFrame( int levelTime ) {
	int			i;
	gentity_t	*ent;
#ifdef _G_FRAME_PERFANAL
	int			iTimer_ItemRun = 0;
	int			iTimer_ROFF = 0;
	int			iTimer_ClientEndframe = 0;
	int			iTimer_GameChecks = 0;
	int			iTimer_Queues = 0;
	void		*timer_ItemRun;
	void		*timer_ROFF;
	void		*timer_ClientEndframe;
	void		*timer_GameChecks;
	void		*timer_Queues;
#endif
	static int lastMsgTime = 0;

	if (g_gametype.integer == GT_SIEGE &&
		g_siegeRespawn.integer &&
		g_siegeRespawnCheck < level.time)
	{ //check for a respawn wave
		int i = 0;
		gentity_t *clEnt;
		while (i < MAX_CLIENTS)
		{
			clEnt = &g_entities[i];

			if (clEnt->inuse && clEnt->client &&
                ((clEnt->client->tempSpectate > level.time) || ((clEnt->health <= 0) && (level.time > clEnt->client->respawnTime))) &&
				clEnt->client->sess.sessionTeam != TEAM_SPECTATOR)
			{
				respawn(clEnt);
				clEnt->client->tempSpectate = 0;
			}
			i++;
		}

		g_siegeRespawnCheck = level.time + g_siegeRespawn.integer * 1000;
	}

	if (gDoSlowMoDuel)
	{
		if (level.restarted)
		{
			char buf[128];
			float tFVal = 0;

			trap_Cvar_VariableStringBuffer("timescale", buf, sizeof(buf));

			tFVal = atof(buf);

			trap_Cvar_Set("timescale", "1");
			if (tFVal == 1.0f)
			{
				gDoSlowMoDuel = qfalse;
			}
		}
		else
		{
			float timeDif = (level.time - gSlowMoDuelTime); //difference in time between when the slow motion was initiated and now
			float useDif = 0; //the difference to use when actually setting the timescale

			if (timeDif < 150)
			{
				trap_Cvar_Set("timescale", "0.1f");
			}
			else if (timeDif < 1150)
			{
				useDif = (timeDif/1000); //scale from 0.1 up to 1
				if (useDif < 0.1)
				{
					useDif = 0.1;
				}
				if (useDif > 1.0)
				{
					useDif = 1.0;
				}
				trap_Cvar_Set("timescale", va("%f", useDif));
			}
			else
			{
				char buf[128];
				float tFVal = 0;

				trap_Cvar_VariableStringBuffer("timescale", buf, sizeof(buf));

				tFVal = atof(buf);

				trap_Cvar_Set("timescale", "1");
				if (timeDif > 1500 && tFVal == 1.0f)
				{
					gDoSlowMoDuel = qfalse;
				}
			}
		}
	}

	// if we are waiting for the level to restart, do nothing
	if ( level.restarted ) {
		return;
	}

	level.framenum++;
	level.previousTime = level.time;
	level.time = levelTime;

    //OSP: pause
    if ( level.pause.state != PAUSE_NONE )
    {
            static int lastCSTime = 0;
            int dt = level.time-level.previousTime;

            // compensate for timelimit and warmup time
            if ( level.warmupTime > 0 )
                    level.warmupTime += dt;
            level.startTime += dt;

			// floor start time to avoid time flipering
			if ( (level.time - level.startTime) % 1000 >= 500)
				level.startTime += (level.time - level.startTime) % 1000;

			// initial CS update time, needed!
			if ( !lastCSTime )
				lastCSTime = level.time;

            // client needs to do the same, just adjust the configstrings periodically
            // i can't see a way around this mess without requiring a client mod.
            if ( lastCSTime < level.time - 500 ) {
                    lastCSTime += 500;
                    trap_SetConfigstring( CS_LEVEL_START_TIME, va( "%i", level.startTime ) );
                    if ( level.warmupTime > 0 )
                            trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime ) );
            }

            if ( g_gametype.integer == GT_SIEGE )
            {
                // siege timer adjustment
                char siegeState[128];
                int round, startTime;
                trap_GetConfigstring( CS_SIEGE_STATE, siegeState, sizeof( siegeState ) );
                sscanf( siegeState, "%i|%i", &round, &startTime );
                startTime += dt;
                trap_SetConfigstring( CS_SIEGE_STATE, va( "%i|%i", round, startTime ) );
                g_siegeRespawnCheck += dt;

				// siege objectives timers adjustments
				if ( gImperialCountdown )
				{ 
					gImperialCountdown += dt;
				}

				if ( gRebelCountdown )
				{
					gRebelCountdown += dt;
				}

            }
    }
    if ( level.pause.state == PAUSE_PAUSED )
    {
            if ( lastMsgTime < level.time-500 ) {
                    trap_SendServerCommand( -1, va( "cp \"Match has been paused. (%.0f)\n\"", ceil((level.pause.time-level.time)/1000.0f)) );
                    lastMsgTime = level.time;
            }

			//if ( level.time > level.pause.time - (japp_unpauseTime.integer*1000) )
            if ( level.time > level.pause.time - (5*1000) ) // 5 seconds
                    level.pause.state = PAUSE_UNPAUSING;
    }
    if ( level.pause.state == PAUSE_UNPAUSING )
    {
            if ( lastMsgTime < level.time-500 ) {
                    trap_SendServerCommand( -1, va( "cp \"MATCH IS UNPAUSING\nin %.0f...\n\"", ceil((level.pause.time-level.time)/1000.0f)) );
                    lastMsgTime = level.time;
            }

            if ( level.time > level.pause.time ) {
                    level.pause.state = PAUSE_NONE;
                    trap_SendServerCommand( -1, "cp \"Go!\n\"" );
            }
    }

	if (g_allowNPC.integer)
	{
	}

	AI_UpdateGroups();

	if (g_allowNPC.integer)
	{
		if ( d_altRoutes.integer )
		{
			trap_Nav_CheckAllFailedEdges();
		}
		trap_Nav_ClearCheckedNodes();

		//remember last waypoint, clear current one
		for ( i = 0; i < level.num_entities ; i++) 
		{
			ent = &g_entities[i];

			if ( !ent->inuse )
				continue;

			if ( ent->waypoint != WAYPOINT_NONE 
				&& ent->noWaypointTime < level.time )
			{
				ent->lastWaypoint = ent->waypoint;
				ent->waypoint = WAYPOINT_NONE;
			}
			if ( d_altRoutes.integer )
			{
				trap_Nav_CheckFailedNodes( ent );
			}
		}

		//Look to clear out old events
		ClearPlayerAlertEvents();
	}

	g_TimeSinceLastFrame = (level.time - g_LastFrameTime);

	// get any cvar changes
	G_UpdateCvars();



#ifdef _G_FRAME_PERFANAL
	trap_PrecisionTimer_Start(&timer_ItemRun);
#endif
	//
	// go through all allocated objects
	//
	ent = &g_entities[0];
	for (i=0 ; i<level.num_entities ; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}

		// clear events that are too old
		if ( level.time - ent->eventTime > EVENT_VALID_MSEC ) {
			if ( ent->s.event ) {
				ent->s.event = 0;	
				if ( ent->client ) {
					ent->client->ps.externalEvent = 0;
					// predicted events should never be set to zero
				}
			}
			if ( ent->freeAfterEvent ) {
				// tempEntities or dropped items completely go away after their event
				if (ent->s.eFlags & EF_SOUNDTRACKER)
				{ //don't trigger the event again..
					ent->s.event = 0;
					ent->s.eventParm = 0;
					ent->s.eType = 0;
					ent->eventTime = 0;
				}
				else
				{
					G_FreeEntity( ent );
					continue;
				}
			} else if ( ent->unlinkAfterEvent ) {
				// items that will respawn will hide themselves after their pickup event
				ent->unlinkAfterEvent = qfalse;
				trap_UnlinkEntity( ent );
			}
		}

		// temporary entities don't think
		if ( ent->freeAfterEvent ) {
			continue;
		}

		if ( !ent->r.linked && ent->neverFree ) {
			continue;
		}

		if ( ent->s.eType == ET_MISSILE ) {
            //OSP: pause
            if ( level.pause.state == PAUSE_NONE )
                    G_RunMissile( ent );
            else
            {// During a pause, gotta keep track of stuff in the air
                    ent->s.pos.trTime += level.time - level.previousTime;
                    G_RunThink( ent );
            }
			continue;
		}

		if ( ent->s.eType == ET_ITEM || ent->physicsObject ) {
#if 0 //use if body dragging enabled?
			if (ent->s.eType == ET_BODY)
			{ //special case for bodies
				float grav = 3.0f;
				float mass = 0.14f;
				float bounce = 1.15f;

				G_RunExPhys(ent, grav, mass, bounce, qfalse, NULL, 0);
			}
			else
			{
				G_RunItem( ent );
			}
#else
			G_RunItem( ent );
#endif
			continue;
		}

		if ( ent->s.eType == ET_MOVER ) {
			G_RunMover( ent );
			continue;
		}

		if ( i < MAX_CLIENTS ) 
		{
			G_CheckClientTimeouts ( ent );
			
			if (ent->client->inSpaceIndex && ent->client->inSpaceIndex != ENTITYNUM_NONE)
			{ //we're in space, check for suffocating and for exiting
                gentity_t *spacetrigger = &g_entities[ent->client->inSpaceIndex];

				if (!spacetrigger->inuse ||
					!G_PointInBounds(ent->client->ps.origin, spacetrigger->r.absmin, spacetrigger->r.absmax))
				{ //no longer in space then I suppose
                    ent->client->inSpaceIndex = 0;					
				}
				else
				{ //check for suffocation
                    if (ent->client->inSpaceSuffocation < level.time)
					{ //suffocate!
						if (ent->health > 0 && ent->takedamage)
						{ //if they're still alive..
							G_Damage(ent, spacetrigger, spacetrigger, NULL, ent->client->ps.origin, Q_irand(50, 70), DAMAGE_NO_ARMOR, MOD_SUICIDE);

							if (ent->health > 0)
							{ //did that last one kill them?
								//play the choking sound
								G_EntitySound(ent, CHAN_VOICE, G_SoundIndex(va( "*choke%d.wav", Q_irand( 1, 3 ) )));

								//make them grasp their throat
								ent->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
								ent->client->ps.forceHandExtendTime = level.time + 2000;
							}
						}

						ent->client->inSpaceSuffocation = level.time + Q_irand(100, 200);
					}
				}
			}

			if (ent->client->isHacking && level.pause.state == PAUSE_NONE)
			{ //hacking checks
				gentity_t *hacked = &g_entities[ent->client->isHacking];
				vec3_t angDif;

				VectorSubtract(ent->client->ps.viewangles, ent->client->hackingAngles, angDif);

				//keep him in the "use" anim
				if (ent->client->ps.torsoAnim != BOTH_CONSOLE1)
				{
					G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_CONSOLE1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
				}
				else
				{
					ent->client->ps.torsoTimer = 500;
				}
				ent->client->ps.weaponTime = ent->client->ps.torsoTimer;

				if (!(ent->client->pers.cmd.buttons & BUTTON_USE))
				{ //have to keep holding use
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (!hacked || !hacked->inuse)
				{ //shouldn't happen, but safety first
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (!G_PointInBounds( ent->client->ps.origin, hacked->r.absmin, hacked->r.absmax ))
				{ //they stepped outside the thing they're hacking, so reset hacking time
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (VectorLength(angDif) > 10.0f)
				{ //must remain facing generally the same angle as when we start
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
			}

#define JETPACK_DEFUEL_RATE		200 //approx. 20 seconds of idle use from a fully charged fuel amt
#define JETPACK_REFUEL_RATE		150 //seems fair
			if (ent->client->jetPackOn && level.pause.state == PAUSE_NONE)
			{ //using jetpack, drain fuel
				if (ent->client->jetPackDebReduce < level.time)
				{
					if (ent->client->pers.cmd.upmove > 0)
					{ //take more if they're thrusting
						ent->client->ps.jetpackFuel -= 2;
					}
					else
					{
						ent->client->ps.jetpackFuel--;
					}
					
					if (ent->client->ps.jetpackFuel <= 0)
					{ //turn it off
						ent->client->ps.jetpackFuel = 0;
						Jetpack_Off(ent);
					}
					ent->client->jetPackDebReduce = level.time + JETPACK_DEFUEL_RATE;
				}
			}
			else if (ent->client->ps.jetpackFuel < 100 && level.pause.state == PAUSE_NONE)
			{ //recharge jetpack
				if (ent->client->jetPackDebRecharge < level.time)
				{
					ent->client->ps.jetpackFuel++;
					ent->client->jetPackDebRecharge = level.time + JETPACK_REFUEL_RATE;
				}
			}

#define CLOAK_DEFUEL_RATE		200 //approx. 20 seconds of idle use from a fully charged fuel amt
#define CLOAK_REFUEL_RATE		150 //seems fair
			if (ent->client->ps.powerups[PW_CLOAKED] && level.pause.state == PAUSE_NONE)
			{ //using cloak, drain battery
				if (ent->client->cloakDebReduce < level.time)
				{
					ent->client->ps.cloakFuel--;
					
					if (ent->client->ps.cloakFuel <= 0)
					{ //turn it off
						ent->client->ps.cloakFuel = 0;
						Jedi_Decloak(ent);
					}
					ent->client->cloakDebReduce = level.time + CLOAK_DEFUEL_RATE;
				}
			}
			else if (ent->client->ps.cloakFuel < 100)
			{ //recharge cloak
				if (ent->client->cloakDebRecharge < level.time)
				{
					ent->client->ps.cloakFuel++;
					ent->client->cloakDebRecharge = level.time + CLOAK_REFUEL_RATE;
				}
			}

			if (g_gametype.integer == GT_SIEGE &&
				ent->client->siegeClass != -1 &&
				(bgSiegeClasses[ent->client->siegeClass].classflags & (1<<CFL_STATVIEWER)))
			{ //see if it's time to send this guy an update of extended info
				if (ent->client->siegeEDataSend < level.time)
				{
                    G_SiegeClientExData(ent);
					ent->client->siegeEDataSend = level.time + 1000; //once every sec seems ok
				}
			}

            if ( level.pause.state == PAUSE_NONE
                    && !level.intermissiontime
                    && !(ent->client->ps.pm_flags & PMF_FOLLOW)
                    && ent->client->sess.sessionTeam != TEAM_SPECTATOR )
			{
				WP_ForcePowersUpdate(ent, &ent->client->pers.cmd );
				WP_SaberPositionUpdate(ent, &ent->client->pers.cmd);
				WP_SaberStartMissileBlockCheck(ent, &ent->client->pers.cmd);
			}

			if (g_allowNPC.integer)
			{
				//This was originally intended to only be done for client 0.
				//Make sure it doesn't slow things down too much with lots of clients in game.
			}

			trap_ICARUS_MaintainTaskManager(ent->s.number);

			G_RunClient( ent );
			continue;
		}
		else if (ent->s.eType == ET_NPC)
		{
			int j;
			// turn off any expired powerups
			for ( j = 0 ; j < MAX_POWERUPS ; j++ ) {
				if ( ent->client->ps.powerups[ j ] < level.time ) {
					ent->client->ps.powerups[ j ] = 0;
				}
			}

			WP_ForcePowersUpdate(ent, &ent->client->pers.cmd );
			WP_SaberPositionUpdate(ent, &ent->client->pers.cmd);
			WP_SaberStartMissileBlockCheck(ent, &ent->client->pers.cmd);
		}

		G_RunThink( ent );

		if (g_allowNPC.integer)
		{
			ClearNPCGlobals();
		}
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_ItemRun = trap_PrecisionTimer_End(timer_ItemRun);
#endif

	SiegeCheckTimers();

#ifdef _G_FRAME_PERFANAL
	trap_PrecisionTimer_Start(&timer_ROFF);
#endif
	trap_ROFF_UpdateEntities();
#ifdef _G_FRAME_PERFANAL
	iTimer_ROFF = trap_PrecisionTimer_End(timer_ROFF);
#endif



#ifdef _G_FRAME_PERFANAL
	trap_PrecisionTimer_Start(&timer_ClientEndframe);
#endif
	// perform final fixups on the players
	ent = &g_entities[0];
	for (i=0 ; i < level.maxclients ; i++, ent++ ) {
		if ( ent->inuse ) {
			ClientEndFrame( ent );
		}
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_ClientEndframe = trap_PrecisionTimer_End(timer_ClientEndframe);
#endif



#ifdef _G_FRAME_PERFANAL
	trap_PrecisionTimer_Start(&timer_GameChecks);
#endif
	// see if it is time to do a tournement restart
	CheckTournament();

	// see if it is time to end the level
	CheckExitRules();

	// update to team status?
	CheckTeamStatus();

	// cancel vote if timed out
	CheckVote();

	// check for allready during warmup
	CheckReady();

	// check team votes
	CheckTeamVote( TEAM_RED );
	CheckTeamVote( TEAM_BLUE );

	// for tracking changes
	CheckCvars();

	//account processing - accounts system
	//processDatabase();	

#ifdef HOOK_GETSTATUS_FIX
	//check getstatus flood report timers
	if (getstatus_TimeToReset &&
		(level.time >= getstatus_TimeToReset)){
		//reset the counter
		//allow only one report per minute
		if ((getstatus_Counter >= g_maxstatusrequests.integer)
			&& (!getstatus_LastReportTime || (level.time - getstatus_LastReportTime > 60*1000))){
			G_HackLog("Too many getstatus requests detected! (%i Hz, %i.%i.%i.%i, unique:%i)\n",
				getstatus_Counter,
				(unsigned char)(getstatus_LastIP),
				(unsigned char)(getstatus_LastIP>>8),
				(unsigned char)(getstatus_LastIP>>16),
				(unsigned char)(getstatus_LastIP>>24),
				getstatus_UniqueIPCount);
			getstatus_LastReportTime = level.time;
		}

		getstatus_Counter = 0;
		getstatus_UniqueIPCount = 0;
		//turn off timer until next getstatus occur
		getstatus_TimeToReset = 0;
	}
#endif

	//initMatch
	if (!level.initialChecked &&
		(level.time - level.startTime > 5000)){
		initMatch();
		level.initialChecked = qtrue;
	}

    // report time wrapping 20 minutes ahead
    #define SERVER_WRAP_RESTART_TIME 0x70000000
    if ( enginePatched && (level.time >= SERVER_WRAP_RESTART_TIME - 20*60*1000 ) )
    {
        static int nextWrappingReportTime = 0;
        if ( level.time >= nextWrappingReportTime )
        {
             int remaining = (SERVER_WRAP_RESTART_TIME - level.time) / 1000;

             G_LogPrintf("Restarting server in %i seconds due to time wrap\n",remaining);
			 trap_SendServerCommand( -1, 
                    va("cp \"Restarting server in %i seconds due to time wrap\n\"", remaining) );

             // report every 1 second last minute, otherwise every 1 minute
             if ( level.time >= SERVER_WRAP_RESTART_TIME - 60*1000 )
             {                     
                nextWrappingReportTime = level.time + 1000;
             }
             else
             {
                nextWrappingReportTime = level.time + 60*1000;
             }
        }
    }  

	if (g_listEntity.integer) {
		for (i = 0; i < MAX_GENTITIES; i++) {
			G_Printf("%4i: %s\n", i, g_entities[i].classname);
		}
		trap_Cvar_Set("g_listEntity", "0");
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_GameChecks = trap_PrecisionTimer_End(timer_GameChecks);
#endif

#ifdef _G_FRAME_PERFANAL
	trap_PrecisionTimer_Start(&timer_Queues);
#endif
	//At the end of the frame, send out the ghoul2 kill queue, if there is one
	G_SendG2KillQueue();

	if (gQueueScoreMessage)
	{
		if (gQueueScoreMessageTime < level.time)
		{
			SendScoreboardMessageToAllClients();

			gQueueScoreMessageTime = 0;
			gQueueScoreMessage = 0;
		}
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_Queues = trap_PrecisionTimer_End(timer_Queues);
#endif



#ifdef _G_FRAME_PERFANAL
	Com_Printf("---------------\nItemRun: %i\nROFF: %i\nClientEndframe: %i\nGameChecks: %i\nQueues: %i\n---------------\n",
		iTimer_ItemRun,
		iTimer_ROFF,
		iTimer_ClientEndframe,
		iTimer_GameChecks,
		iTimer_Queues);
#endif

	g_LastFrameTime = level.time;
}

const char *G_GetStringEdString(char *refSection, char *refName)
{
	//Well, it would've been lovely doing it the above way, but it would mean mixing
	//languages for the client depending on what the server is. So we'll mark this as
	//a stringed reference with @@@ and send the refname to the client, and when it goes
	//to print it will get scanned for the stringed reference indication and dealt with
	//properly.
	static char text[1024]={0};
	Com_sprintf(text, sizeof(text), "@@@%s", refName);
	return text;
}




