// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"
#include "g_ICARUScb.h"
#include "g_nav.h"
#include "bg_saga.h"
#include "sodium.h"

//#include "accounts.h"
#include "jp_engine.h"
#include "g_database.h"

#include "kdtree.h"

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
vmCvar_t	g_wasIntermission;

vmCvar_t	g_gametype;
vmCvar_t	g_MaxHolocronCarry;
vmCvar_t	g_ff_objectives;
vmCvar_t	g_autoMapCycle;
vmCvar_t	g_autoStats;
vmCvar_t	g_statsCaptureTimes;
vmCvar_t	g_stats_useTeamgenPos;
vmCvar_t	g_dmflags;
vmCvar_t	g_maxForceRank;
vmCvar_t	g_forceBasedTeams;
vmCvar_t	g_privateDuel;

vmCvar_t	g_duelLifes;
vmCvar_t	g_duelShields;

vmCvar_t	g_chatLimit;
vmCvar_t	g_teamChatLimit;
vmCvar_t	g_voiceChatLimit;

vmCvar_t	g_chatTickWaitMinimum;
vmCvar_t	g_teamChatTickWaitMinimum;
vmCvar_t	g_voiceChatTickWaitMinimum;

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

vmCvar_t	g_fixSaberDefense;
// 1 = angle-based blocking for disrupts
// 2 = also fix projectile blocking

vmCvar_t	g_saberDefense1Angle;
vmCvar_t	g_saberDefense2Angle;
vmCvar_t	g_saberDefense3Angle;

vmCvar_t	g_instagib;
vmCvar_t	g_instagibRespawnTime;
vmCvar_t	g_instagibRespawnMinPlayers;

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
vmCvar_t	g_nonLiveMatchesCanEnd;
vmCvar_t	g_capturelimit;
vmCvar_t	g_capturedifflimit;
vmCvar_t	g_preventCapBm;
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
vmCvar_t    g_moreTaunts;
vmCvar_t	g_raceEmotes;
vmCvar_t	g_ragersCanCounterPushPull;
vmCvar_t	g_autoPause999;
vmCvar_t	g_autoPauseDisconnect;
vmCvar_t	g_quickPauseMute;
vmCvar_t	g_enterSpammerTime;
vmCvar_t	g_quickPauseChat;
vmCvar_t	g_pauseVelocityLines;

vmCvar_t	g_webhookId;
vmCvar_t	g_webhookToken;

vmCvar_t	g_teamOverlayForce;
vmCvar_t	g_teamOverlayForceAlignment;

vmCvar_t	g_broadcastForceLoadouts;

vmCvar_t	g_improvedHoming;
vmCvar_t	g_improvedHomingThreshold;
vmCvar_t	d_debugImprovedHoming;
vmCvar_t	g_braindeadBots;
vmCvar_t	g_unlagged;
#ifdef _DEBUG
vmCvar_t	g_unlaggedMaxCompensation;
vmCvar_t	g_unlaggedSkeletons;
vmCvar_t	g_unlaggedSkeletonTime;
vmCvar_t	g_unlaggedFactor;
vmCvar_t	g_unlaggedOffset;
vmCvar_t	g_unlaggedDebug;
#endif
vmCvar_t	g_unlaggedFix;

vmCvar_t	g_customVotes;
vmCvar_t	g_customVote1_command;
vmCvar_t	g_customVote1_label;
vmCvar_t	g_customVote2_command;
vmCvar_t	g_customVote2_label;
vmCvar_t	g_customVote3_command;
vmCvar_t	g_customVote3_label;
vmCvar_t	g_customVote4_command;
vmCvar_t	g_customVote4_label;
vmCvar_t	g_customVote5_command;
vmCvar_t	g_customVote5_label;
vmCvar_t	g_customVote6_command;
vmCvar_t	g_customVote6_label;
vmCvar_t	g_customVote7_command;
vmCvar_t	g_customVote7_label;
vmCvar_t	g_customVote8_command;
vmCvar_t	g_customVote8_label;
vmCvar_t	g_customVote9_command;
vmCvar_t	g_customVote9_label;
vmCvar_t	g_customVote10_command;
vmCvar_t	g_customVote10_label;

vmCvar_t	g_teamPrivateDuels;

vmCvar_t	g_teamOverlayUpdateRate;
vmCvar_t	debug_clientNumLog;

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
vmCvar_t	g_forceClientUpdateRate;
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
vmCvar_t	g_bouncePadDoubleJump;

#include "namespace_begin.h"
vmCvar_t	pmove_fixed;
vmCvar_t	pmove_msec;
vmCvar_t	pmove_float;
#include "namespace_end.h"

vmCvar_t	g_flechetteSpread;

vmCvar_t	d_bowcasterRework_enable;
vmCvar_t	d_bowcasterRework_primaryBoltDamage;
vmCvar_t	d_bowcasterRework_altBoltDamage;
vmCvar_t	d_bowcasterRework_velocityAdd;
vmCvar_t	d_bowcasterRework_spreadMultiplier;
vmCvar_t	d_bowcasterRework_playerVelocityFactor;
vmCvar_t	d_bowcasterRework_softSaberPierce;
vmCvar_t	d_bowcasterRework_secondRing;
vmCvar_t	d_bowcasterRework_secondRingSpreadMultiplier;
vmCvar_t	d_bowcasterRework_primaryNumBolts;
vmCvar_t	d_bowcasterRework_secondRingNumBolts;
vmCvar_t	d_bowcasterRework_primaryBounces;
vmCvar_t	d_bowcasterRework_backwardsBounce;
vmCvar_t	d_bowcasterRework_backwardsBounceSpread;
vmCvar_t	d_bowcasterRework_knockbackSaberPierce;
vmCvar_t	d_bowcasterRework_knockbackMultiplier;

vmCvar_t	g_defaultMapFFA;
vmCvar_t	g_defaultMapDuel;
vmCvar_t	g_defaultMapSiege;
vmCvar_t	g_defaultMapCTF;

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
vmCvar_t	g_fixLava;
vmCvar_t    g_fixLiftKills;
vmCvar_t    g_fixBumpKills;
vmCvar_t    g_creditAirKills;

vmCvar_t    g_fixDempSaberThrow;

vmCvar_t	g_fixWallgrab;
vmCvar_t	g_fixForceJumpAnimationLock;
vmCvar_t	g_fixNoAmmoShootAnimation;
vmCvar_t	g_fix5AmmoSniping;
vmCvar_t	g_fixSniperSwitch;
vmCvar_t	g_fixGolanDamage;
vmCvar_t	g_locationBasedDamage_splash;
vmCvar_t	g_fixFlagPickup;
vmCvar_t	d_debugFixFlagPickup;
vmCvar_t	g_fixFreshWeaponAmmo;
vmCvar_t	g_fixPulledWeaponAmmo;
vmCvar_t	g_fixSpawnBlasterAmmo;
vmCvar_t	g_rocketHPFix;
vmCvar_t	g_fixDisarmWeaponPreference;
vmCvar_t	g_homingUses1Ammo;
vmCvar_t	g_sniperDamageNerf;
vmCvar_t	g_fixWeaponChargeTime;
vmCvar_t	d_debugSnipeDamage;
vmCvar_t	g_fixDisarmFiring;
vmCvar_t	g_fixCorpseSniping;
vmCvar_t	g_fixReconnectCorpses;
vmCvar_t	g_corpseLimit;
vmCvar_t	g_preActivateSpeedWhileDead;

vmCvar_t	g_allowIgnore;

vmCvar_t	g_fixRoll;

vmCvar_t	g_balanceSaber;
vmCvar_t	g_balanceSeeing;

vmCvar_t	g_autoSendScores;

vmCvar_t	g_lineOfSightLocations;
vmCvar_t	g_lineOfSightLocations_generate;
vmCvar_t	g_enableChatLocations;

vmCvar_t	g_breakRNG;

vmCvar_t	g_randomConeReflection;
vmCvar_t	g_coneReflectAngle;

#ifdef _DEBUG
vmCvar_t	z_debug1;
vmCvar_t	z_debug2;
vmCvar_t	z_debug3;
vmCvar_t	z_debug4;
vmCvar_t	z_debug5;
vmCvar_t	z_debug6;
vmCvar_t	z_debug7;
vmCvar_t	z_debug8;
vmCvar_t	z_debug9;
vmCvar_t	z_debugOther1;
vmCvar_t	z_debugOther2;
vmCvar_t	z_debugPause;
#endif

vmCvar_t    g_enforceEvenVotersCount;
vmCvar_t    g_minVotersForEvenVotersCount;
vmCvar_t	g_losingTeamEndmatchTeamvote;

vmCvar_t	g_duplicateNamesId;

vmCvar_t	g_droppedFlagSpawnProtectionRadius;
vmCvar_t	g_droppedFlagSpawnProtectionDuration;
vmCvar_t	g_selfKillSpawnSpamProtection;
vmCvar_t	g_killedAntiHannahSpawnRadius;
vmCvar_t	g_canSpawnInTeRangeOfFc;
vmCvar_t	g_presetInitialSpawns;

vmCvar_t	g_numRedPlayers;
vmCvar_t	g_numBluePlayers;

vmCvar_t	g_boost;
vmCvar_t	g_spawnboost_default;
vmCvar_t	g_spawnboost_losIdealDistance;
vmCvar_t	g_spawnboost_teamkill;
vmCvar_t	g_spawnboost_autosk;
vmCvar_t	g_aimbotBoost_hitscan;
vmCvar_t	g_boost_maxDarkTh;
vmCvar_t	g_boost_fakeAlignment;
vmCvar_t	g_boost_setFakeAlignmentOnSpawn;

vmCvar_t	g_botAimbot;

vmCvar_t	g_infiniteCharge;

#ifdef NEWMOD_SUPPORT
vmCvar_t	g_netUnlock;
vmCvar_t	g_nmFlags;
vmCvar_t	g_enableNmAuth;
vmCvar_t	g_specInfo;
#endif

vmCvar_t     g_strafejump_mod;

vmCvar_t	g_fixSense;
vmCvar_t	g_antiWallhack;
vmCvar_t	g_wallhackMaxTraces;

vmCvar_t	g_inMemoryDB;

vmCvar_t	g_enableRacemode;
vmCvar_t	g_enableAimPractice;
#ifdef _DEBUG
vmCvar_t	d_disableRaceVisChecks;
#endif

vmCvar_t	g_traceSQL;

//allowing/disabling vote types
vmCvar_t    g_allow_vote_gametype;
vmCvar_t    g_allow_vote_kick;
vmCvar_t    g_allow_vote_restart;
vmCvar_t    g_allow_vote_map;
vmCvar_t    g_allow_vote_nextmap;
vmCvar_t    g_allow_vote_timelimit;
vmCvar_t    g_allow_vote_fraglimit;
vmCvar_t    g_allow_vote_maprandom;
vmCvar_t	g_allow_vote_mapvote;
vmCvar_t    g_allow_vote_warmup;
vmCvar_t	g_allow_vote_boon;
vmCvar_t	g_allow_vote_instagib;
vmCvar_t	g_default_capturedifflimit;
vmCvar_t	g_enable_maprandom_wildcard;
vmCvar_t	g_redirectDoWarmupVote;
vmCvar_t	g_redirectNextMapVote;
vmCvar_t	g_redirectPoolVoteToTierListVote;
vmCvar_t    g_quietrcon;
vmCvar_t    g_npc_spawn_limit;
vmCvar_t	g_hackLog;

vmCvar_t	g_allowReady;

vmCvar_t	g_allowVerify;

vmCvar_t    g_restart_countdown;

vmCvar_t    g_enableBoon;
vmCvar_t	g_enableMemePickups;
vmCvar_t    g_maxstatusrequests;
vmCvar_t	g_testdebug; //for tmp debug
vmCvar_t	g_rconpassword;

vmCvar_t	g_vote_tierlist;
vmCvar_t	g_vote_tierlist_s_min;
vmCvar_t	g_vote_tierlist_s_max;
vmCvar_t	g_vote_tierlist_a_min;
vmCvar_t	g_vote_tierlist_a_max;
vmCvar_t	g_vote_tierlist_b_min;
vmCvar_t	g_vote_tierlist_b_max;
vmCvar_t	g_vote_tierlist_c_min;
vmCvar_t	g_vote_tierlist_c_max;
vmCvar_t	g_vote_tierlist_f_min;
vmCvar_t	g_vote_tierlist_f_max;
vmCvar_t	g_vote_tierlist_totalMaps;
vmCvar_t	g_vote_tierlist_debug;
vmCvar_t	g_vote_tierlist_reminders;
vmCvar_t	g_vote_tierlist_fixShittyPools;
vmCvar_t	g_vote_betaMapForceInclude;
vmCvar_t	g_vote_rng;
vmCvar_t	g_vote_runoff;
vmCvar_t	g_vote_mapCooldownMinutes;
vmCvar_t	g_vote_runoffTimeModifier;
vmCvar_t	g_vote_redirectMapVoteToLiveVersion;
vmCvar_t	g_vote_printLiveVersionFullName;
vmCvar_t	g_vote_overrideTrollVoters;
vmCvar_t	g_vote_runoffRerollOption;
vmCvar_t	g_vote_banMap;
vmCvar_t	g_vote_banPhaseTimeModifier;

vmCvar_t	g_vote_teamgen;
vmCvar_t	g_vote_teamgen_subhelp;
vmCvar_t	g_vote_teamgen_rustWeeks;
vmCvar_t	g_vote_teamgen_minSecsSinceIntermission;
vmCvar_t	g_vote_teamgen_enableAppeasing;
vmCvar_t	g_vote_teamgen_enableFirstChoice;
vmCvar_t	g_vote_teamgen_enableInclusive;
vmCvar_t	g_vote_teamgen_remindPositions;
vmCvar_t	g_vote_teamgen_remindToSetPositions;
vmCvar_t	g_vote_teamgen_announceBreak;
vmCvar_t	g_vote_teamgen_autoRestartOnMapChange;
vmCvar_t	g_vote_teamgen_autoMapVoteSeconds;
vmCvar_t	g_vote_teamgen_autoMapVoteNonAfkAutoVoteYesSeconds;
vmCvar_t	g_vote_teamgen_iterate;
vmCvar_t	g_vote_teamgen_preventStartDuringPug;
vmCvar_t	g_vote_teamgen_banLastPlayedPermutation;
vmCvar_t	g_vote_teamgen_enableBarVote;
vmCvar_t	g_vote_teamgen_barVoteStartsNewPug;
vmCvar_t	g_vote_teamgen_barVoteDoesntStartNewPugIfManyPlayers;
vmCvar_t	g_vote_teamgen_unvote;
vmCvar_t	g_vote_teamgen_fuck;
vmCvar_t	g_vote_teamgen_new8PlayerAlgo;
vmCvar_t	g_vote_teamgen_require2VotesOnEachTeam;
vmCvar_t	g_vote_teamgen_readBeforeVotingMilliseconds;
vmCvar_t	g_vote_teamgen_readBeforeVotingMillisecondsJawa;
vmCvar_t	g_vote_teamgen_preventBindsWith8PlayersMilliseconds;
vmCvar_t	g_vote_teamgen_bImbalanceCapWith0OrTwoRerolls;
vmCvar_t	g_vote_teamgen_bImbalanceCapWithOneReroll;
vmCvar_t	g_vote_teamgen_acdImbalanceCapWithoutReroll;
vmCvar_t	g_vote_teamgen_acdImbalanceCapWithOneReroll;
vmCvar_t	g_vote_teamgen_dynamicVoteRequirement;
vmCvar_t	g_vote_freezeUntilVote;
vmCvar_t	g_vote_lessPlayedMapsDisfavoredInRunoffEliminations;
vmCvar_t	g_vote_fadeToBlack;
vmCvar_t	g_vote_mapVoteExtensions_maxExtensions;
vmCvar_t	g_vote_mapVoteExtension_ifUnderThisManyVotes;
vmCvar_t	g_vote_mapVoteExtension_extensionDuration;
vmCvar_t	g_vote_audioMotivation;
vmCvar_t	g_vote_notifyTeammatesOfMapChoice;
vmCvar_t	g_vote_underdogTeamMapVoteTiebreakerThreshold;
vmCvar_t	g_vote_fuckRequiredVotes;
vmCvar_t	g_vote_preventSwitchingTeamsDuringMapVote;
vmCvar_t	g_vote_teamgen_autoAdjustRequiredPugVotes;
vmCvar_t	g_vote_teamgen_sumOfSquaresTiebreaker;
vmCvar_t	g_vote_teamgen_aDietB;
vmCvar_t	g_vote_teamgen_displayCaliber;

vmCvar_t	g_filterSlurs;
vmCvar_t	g_filterUsers;
vmCvar_t	g_lockdown;

vmCvar_t	g_addItems;
vmCvar_t	g_addItemsWhitelist;
vmCvar_t	g_deleteBaseItems;
vmCvar_t	g_deleteBaseItemsWhitelist;

vmCvar_t	g_broadcastCtfPos;
vmCvar_t	g_assignMissingCtfPos;
vmCvar_t	g_broadcastGivenThTe;
vmCvar_t	g_broadcastCapturerHealthArmorForce;
vmCvar_t	g_outOfBandDMs;

vmCvar_t	d_debugBanPermutation;

vmCvar_t	g_lastIntermissionStartTime;
vmCvar_t	g_lastTeamGenTime;
vmCvar_t	g_lastMapVotedMap;
vmCvar_t	g_lastMapVotedTime;

vmCvar_t	g_bannedPermutationHash;
vmCvar_t	g_bannedPermutationTime;
vmCvar_t	g_lastSelectedPermutationHash;
vmCvar_t	g_lastSelectedPermutationTime;
vmCvar_t	g_lastSelectedPositionlessPermutation;

vmCvar_t	d_debugCtfPosCalculation;
vmCvar_t	d_debugSpawns;

vmCvar_t	g_notFirstMap;
vmCvar_t	r_rngNum;
vmCvar_t	r_rngNumSet;

vmCvar_t	g_allowMoveDisable;

vmCvar_t	g_rockPaperScissors;

vmCvar_t	g_gripRework;
vmCvar_t	g_gripRefreshRate;
vmCvar_t	g_gripAbsorbFix;
vmCvar_t	g_protect3DamageReduction;
vmCvar_t	g_lightningRework;
vmCvar_t	g_fixPushPullLOS;
vmCvar_t	d_debugPushPullLOS;

vmCvar_t	g_mindTrickBuff;
vmCvar_t	g_drainRework;
vmCvar_t	g_fixForceTimers;
vmCvar_t	g_thTeRequiresLOS;
vmCvar_t	d_debugThTeLOS;
vmCvar_t	g_thTeUnlagged;
vmCvar_t	d_logTEData;

vmCvar_t	g_sendForceTimers;

vmCvar_t	g_allowSkAutoThTe;

vmCvar_t	g_minimumCullDistance;

vmCvar_t	g_callvotedelay;
vmCvar_t	g_callvotemaplimit;

vmCvar_t	g_mapVotePlayers;
vmCvar_t	g_mapVoteThreshold;

vmCvar_t	sv_privateclients;
vmCvar_t	sv_passwordlessSpectators;

vmCvar_t	d_measureAirTime;

vmCvar_t	g_notifyAFK;
vmCvar_t	g_waitForAFK;
vmCvar_t	g_waitForAFKTimer;
vmCvar_t	g_waitForAFKThreshold;
vmCvar_t	g_waitForAFKThresholdTroll;
vmCvar_t	g_waitForAFKMinPlayers;
vmCvar_t	g_printCountry;
vmCvar_t	g_redirectWrongThTeBinds;

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
	{ NULL, "gameversion", "25w11a" , CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },
	{ &g_restarted, "g_restarted", "0", CVAR_ROM, 0, qfalse  },
	{ NULL, "sv_mapname", "", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },

#ifdef _DEBUG
	{ NULL, "sv_matchid", "", CVAR_SERVERINFO | CVAR_ROM, 0, qtrue },
#else
	{ NULL, "sv_matchid", "", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse },
#endif

	{ &g_wasRestarted, "g_wasRestarted", "0", CVAR_ROM, 0, qfalse  },
	{ &g_wasIntermission, "g_wasIntermission", "0", CVAR_ROM, 0, qfalse  },

	// latched vars
	{ &g_gametype, "g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH, 0, qfalse  },
	{ &g_MaxHolocronCarry, "g_MaxHolocronCarry", "3", CVAR_LATCH, 0, qfalse  },

    { &g_maxclients, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qfalse },
	{ &g_maxGameClients, "g_maxGameClients", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qfalse  },

	{ &g_trueJedi, "g_jediVmerc", "0", /*CVAR_SERVERINFO |*/ CVAR_LATCH | CVAR_ARCHIVE, 0, qtrue },

	// change anytime vars
	{ &g_ff_objectives, "g_ff_objectives", "0", /*CVAR_SERVERINFO |*/ CVAR_CHEAT | CVAR_NORESTART, 0, qtrue },

	{ &g_autoMapCycle, "g_autoMapCycle", "0", CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
	{ &g_autoStats, "g_autoStats", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_statsCaptureTimes, "g_statsCaptureTimes", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_stats_useTeamgenPos, "g_stats_useTeamgenPos", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_dmflags, "dmflags", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue  },
	
	{ &g_maxForceRank, "g_maxForceRank", "6", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse  },
	{ &g_forceBasedTeams, "g_forceBasedTeams", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse  },
	{ &g_privateDuel, "g_privateDuel", "1", CVAR_ARCHIVE, 0, qtrue  },
	
	{ &g_duelLifes, "g_duelLifes", "100", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_duelShields, "g_duelShields", "25", CVAR_ARCHIVE, 0, qtrue  },

	{ &g_chatLimit, "g_chatLimit", "3", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_teamChatLimit, "g_teamChatLimit", "30", CVAR_ARCHIVE, 0, qtrue },
	{ &g_voiceChatLimit, "g_voiceChatLimit", "3", CVAR_ARCHIVE, 0, qtrue },

	{ &g_chatTickWaitMinimum, "g_chatTickWaitMinimum", "0", CVAR_ARCHIVE, 0, qtrue  },
	{ &g_teamChatTickWaitMinimum, "g_teamChatTickWaitMinimum", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_voiceChatTickWaitMinimum, "g_voiceChatTickWaitMinimum", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_allowNPC, "g_allowNPC", "1", CVAR_ARCHIVE, 0, qtrue  },

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

    { &g_locationBasedDamage, "g_locationBasedDamage", "2", 0, 0, qtrue },

	{ &g_fixSaberDefense, "g_fixSaberDefense", "0", 0, 0, qtrue },
	{ &g_saberDefense1Angle, "g_saberDefense1Angle", "6", 0, 0, qtrue },
	{ &g_saberDefense2Angle, "g_saberDefense2Angle", "30", 0, 0, qtrue },
	{ &g_saberDefense3Angle, "g_saberDefense3Angle", "73", 0, 0, qtrue },

	{ &g_instagib, "g_instagib", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_instagibRespawnTime, "g_instagibRespawnTime", "5", CVAR_ARCHIVE, 0, qtrue },
	{ &g_instagibRespawnMinPlayers, "g_instagibRespawnMinPlayers", "0", CVAR_ARCHIVE, 0, qtrue },

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
	{ &g_nonLiveMatchesCanEnd, "g_nonLiveMatchesCanEnd", "1", CVAR_ARCHIVE, 0, qtrue },
    { &g_capturelimit, "capturelimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
    { &g_capturedifflimit, "capturedifflimit", "10", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue },
    { &g_preventCapBm, "g_preventCapBm", "1", CVAR_ARCHIVE, 0, qfalse },

    { &g_synchronousClients, "g_synchronousClients", "0", CVAR_SYSTEMINFO, 0, qfalse },
	{ &g_forceClientUpdateRate, "g_forceClientUpdateRate", "200", CVAR_ARCHIVE, 0, qfalse },

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
    { &g_gravity, "g_gravity", "760", CVAR_SERVERINFO, 0, qtrue },
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
	{ &g_smoothClients, "g_smoothClients", "1", 0, 0, qfalse },
	{ &pmove_fixed, "pmove_fixed", "0", CVAR_SYSTEMINFO | CVAR_ARCHIVE, 0, qtrue },
	{ &pmove_msec, "pmove_msec", "8", CVAR_SYSTEMINFO | CVAR_ARCHIVE, 0, qtrue },
	{ &pmove_float, "pmove_float", "1", CVAR_SYSTEMINFO | CVAR_ARCHIVE, 0, qtrue },

	{ &g_flechetteSpread, "g_flechetteSpread", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &d_bowcasterRework_enable, "d_bowcasterRework_enable", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &d_bowcasterRework_primaryBoltDamage, "d_bowcasterRework_primaryBoltDamage", "10", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_altBoltDamage, "d_bowcasterRework_altBoltDamage", "15", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_velocityAdd, "d_bowcasterRework_velocityAdd", "650", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_spreadMultiplier, "d_bowcasterRework_spreadMultiplier", "2.0", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_playerVelocityFactor, "d_bowcasterRework_playerVelocityFactor", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_softSaberPierce, "d_bowcasterRework_softSaberPierce", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_secondRing, "d_bowcasterRework_secondRing", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_secondRingSpreadMultiplier, "d_bowcasterRework_secondRingSpreadMultiplier", "4.0", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_primaryNumBolts, "d_bowcasterRework_primaryNumBolts", "5", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_secondRingNumBolts, "d_bowcasterRework_secondRingNumBolts", "5", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_primaryBounces, "d_bowcasterRework_primaryBounces", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_backwardsBounce, "d_bowcasterRework_backwardsBounce", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_backwardsBounceSpread, "d_bowcasterRework_backwardsBounceSpread", "100", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_knockbackSaberPierce, "d_bowcasterRework_knockbackSaberPierce", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &d_bowcasterRework_knockbackMultiplier, "d_bowcasterRework_knockbackMultiplier", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_defaultMapFFA, "g_defaultMapFFA", "", CVAR_ARCHIVE, 0, qtrue },
	{ &g_defaultMapDuel, "g_defaultMapDuel", "", CVAR_ARCHIVE, 0, qtrue },
	{ &g_defaultMapSiege, "g_defaultMapSiege", "", CVAR_ARCHIVE, 0, qtrue },
	{ &g_defaultMapCTF, "g_defaultMapCTF", "", CVAR_ARCHIVE, 0, qtrue },

	{ &g_dismember, "g_dismember", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_forceDodge, "g_forceDodge", "1", 0, 0, qtrue },

	{ &g_timeouttospec, "g_timeouttospec", "70", CVAR_ARCHIVE, 0, qfalse },

	{ &g_saberDmgVelocityScale, "g_saberDmgVelocityScale", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_saberDmgDelay_Idle, "g_saberDmgDelay_Idle", "350", CVAR_ARCHIVE, 0, qtrue },
	{ &g_saberDmgDelay_Wound, "g_saberDmgDelay_Wound", "0", CVAR_ARCHIVE, 0, qtrue },

#ifndef FINAL_BUILD
	{ &g_saberDebugPrint, "g_saberDebugPrint", "0", CVAR_CHEAT, 0, qfalse },
#endif
	{ &g_debugSaberLocks, "g_debugSaberLocks", "0", CVAR_CHEAT, 0, qfalse },
	{ &g_saberLockRandomNess, "g_saberLockRandomNess", "2", CVAR_CHEAT, 0, qfalse },
		// nmckenzie: SABER_DAMAGE_WALLS
	{ &g_saberWallDamageScale, "g_saberWallDamageScale", "0.4", 0, 0, qfalse },

	{ &d_saberStanceDebug, "d_saberStanceDebug", "0", 0, 0, qfalse },

	{ &g_siegeTeamSwitch, "g_siegeTeamSwitch", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, qfalse },

	{ &bg_fighterAltControl, "bg_fighterAltControl", "0", /*CVAR_SERVERINFO | */0, 0, qtrue },

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
	{ &g_siegeTeam1, "g_siegeTeam1", "none", CVAR_ARCHIVE | CVAR_SERVERINFO, 0, qfalse },
	{ &g_siegeTeam2, "g_siegeTeam2", "none", CVAR_ARCHIVE | CVAR_SERVERINFO, 0, qfalse },

		//mainly for debugging with bots while I'm not around (want the server to
		//cycle through levels naturally)
	{ &d_noIntermissionWait, "d_noIntermissionWait", "0", CVAR_CHEAT, 0, qfalse },

	{ &g_austrian, "g_austrian", "0", CVAR_ARCHIVE, 0, qfalse },
		// nmckenzie:
		// DUEL_HEALTH
	{ &g_showDuelHealths, "g_showDuelHealths", "0", CVAR_SERVERINFO },
	{ &g_powerDuelStartHealth, "g_powerDuelStartHealth", "150", CVAR_ARCHIVE, 0, qtrue },
	{ &g_powerDuelEndHealth, "g_powerDuelEndHealth", "90", CVAR_ARCHIVE, 0, qtrue },

		// *CHANGE 12* allowing/disabling cvars
	{ &g_protectQ3Fill,	"g_protectQ3Fill"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_protectQ3FillIPLimit,	"g_protectQ3FillIPLimit"	, "3"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_protectHPhack,	"g_protectHPhack"	, "3"	, CVAR_ARCHIVE, 0, qtrue },


	{ &g_protectCallvoteHack,	"g_protectCallvoteHack"	, "1"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_maxIPConnected,	"g_maxIPConnected"	, "0"	, CVAR_ARCHIVE, 0, qtrue },

		// *CHANGE 10* anti q3fill
	{ &g_cleverFakeDetection,	"g_cleverFakeDetection"	, "forcepowers"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_fixPitKills,	"g_fixPitKills"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixLava,	"g_fixLava"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixLiftKills,	"g_fixLiftKills", "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixBumpKills,	"g_fixBumpKills", "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_creditAirKills,	"g_creditAirKills", "1"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_fixDempSaberThrow,	"g_fixDempSaberThrow"	, "1"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_fixWallgrab,	"g_fixWallgrab"	, "1"	, CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_fixForceJumpAnimationLock,	"g_fixForceJumpAnimationLock"	, "1"	, CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_fixNoAmmoShootAnimation,	"g_fixNoAmmoShootAnimation"		, "1"	, CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_fix5AmmoSniping,	"g_fix5AmmoSniping"		, "1"	, CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_fixSniperSwitch,	"g_fixSniperSwitch"		, "0"	, CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_fixGolanDamage,	"g_fixGolanDamage"		, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_locationBasedDamage_splash,	"g_locationBasedDamage_splash"		, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixFlagPickup,	"g_fixFlagPickup"		, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &d_debugFixFlagPickup,	"d_debugFixFlagPickup"		, "0"	, CVAR_ARCHIVE, 0, qfalse },
	{ &g_fixFreshWeaponAmmo,	"g_fixFreshWeaponAmmo"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixPulledWeaponAmmo,	"g_fixPulledWeaponAmmo"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixSpawnBlasterAmmo,	"g_fixSpawnBlasterAmmo"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_rocketHPFix,	"g_rocketHPFix"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixDisarmWeaponPreference,	"g_fixDisarmWeaponPreference"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_homingUses1Ammo,	"g_homingUses1Ammo"	, "0"	, CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_sniperDamageNerf,	"g_sniperDamageNerf"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixWeaponChargeTime,	"g_fixWeaponChargeTime"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &d_debugSnipeDamage,	"d_debugSnipeDamage"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixDisarmFiring,	"g_fixDisarmFiring"	, "1"	, CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_fixCorpseSniping,	"g_fixCorpseSniping"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_fixReconnectCorpses,	"g_fixReconnectCorpses"	, "1"	, CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_corpseLimit,	"g_corpseLimit"	, "8"	, CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_preActivateSpeedWhileDead,	"g_preActivateSpeedWhileDead"	, "1"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_allowIgnore, "g_allowIgnore", "0", CVAR_ARCHIVE, 0, qfalse },

	{ &g_fixRoll,	"g_fixRoll"		, "1"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_balanceSaber, "g_balanceSaber", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_balanceSeeing, "g_balanceSeeing", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_autoSendScores, "g_autoSendScores", "2000", CVAR_ARCHIVE, 0, qtrue },

	{ &g_lineOfSightLocations, "g_lineOfSightLocations", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_lineOfSightLocations_generate, "g_lineOfSightLocations_generate", "0", CVAR_TEMP | CVAR_LATCH, 0, qtrue },
	{ &g_enableChatLocations, "g_enableChatLocations", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_breakRNG, "g_breakRNG", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_randomConeReflection , "g_randomConeReflection", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_coneReflectAngle , "g_coneReflectAngle", "30", CVAR_ARCHIVE, 0, qtrue },

#ifdef _DEBUG
	{ &z_debug1, "z_debug1", "", 0, 0, qtrue },
	{ &z_debug2, "z_debug2", "", 0, 0, qtrue },
	{ &z_debug3, "z_debug3", "", 0, 0, qtrue },
	{ &z_debug4, "z_debug4", "", 0, 0, qtrue },
	{ &z_debug5, "z_debug5", "", 0, 0, qtrue },
	{ &z_debug6, "z_debug6", "", 0, 0, qtrue },
	{ &z_debug7, "z_debug7", "", 0, 0, qtrue },
	{ &z_debug8, "z_debug8", "", 0, 0, qtrue },
	{ &z_debug9, "z_debug9", "", 0, 0, qtrue },
	{ &z_debugOther1, "z_debugOther1", "", 0, 0, qtrue },
	{ &z_debugOther2, "z_debugOther2", "", 0, 0, qtrue },
	{ &z_debugPause, "z_debugPause", "0", 0, 0, qtrue },
#endif

	{ &g_minimumVotesCount, "g_minimumVotesCount", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_enforceEvenVotersCount, "g_enforceEvenVotersCount", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_minVotersForEvenVotersCount, "g_minVotersForEvenVotersCount", "7", CVAR_ARCHIVE, 0, qtrue },
	{ &g_losingTeamEndmatchTeamvote, "g_losingTeamEndmatchTeamvote", "2", CVAR_ARCHIVE, 0, qtrue },

	{ &g_duplicateNamesId, "g_duplicateNamesId", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_droppedFlagSpawnProtectionRadius, "g_droppedFlagSpawnProtectionRadius", "1024", CVAR_ARCHIVE, 0, qtrue },
	{ &g_droppedFlagSpawnProtectionDuration, "g_droppedFlagSpawnProtectionDuration", "10000", CVAR_ARCHIVE, 0, qtrue },
	{ &g_selfKillSpawnSpamProtection, "g_selfKillSpawnSpamProtection", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_killedAntiHannahSpawnRadius, "g_killedAntiHannahSpawnRadius", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_canSpawnInTeRangeOfFc, "g_canSpawnInTeRangeOfFc", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_presetInitialSpawns, "g_presetInitialSpawns", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_numRedPlayers, "g_numRedPlayers", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },
	{ &g_numBluePlayers, "g_numBluePlayers", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },

	{ &g_boost, "g_boost", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_spawnboost_default, "g_spawnboost_default", "0.333", CVAR_ARCHIVE, 0, qfalse },
	{ &g_spawnboost_losIdealDistance, "g_spawnboost_losIdealDistance", "1600", CVAR_ARCHIVE, 0, qfalse },
	{ &g_spawnboost_teamkill, "g_spawnboost_teamkill", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_spawnboost_autosk, "g_spawnboost_autosk", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_aimbotBoost_hitscan, "g_aimbotBoost_hitscan", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_boost_maxDarkTh, "g_boost_maxDarkTh", "35", CVAR_ARCHIVE, 0, qfalse },
	{ &g_boost_fakeAlignment, "g_boost_fakeAlignment", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_boost_setFakeAlignmentOnSpawn, "g_boost_setFakeAlignmentOnSpawn", "0", CVAR_ARCHIVE, 0, qfalse },

	{ &g_botAimbot, "g_botAimbot", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_infiniteCharge, "g_infiniteCharge", "0", CVAR_ARCHIVE, 0, qtrue },

#ifdef NEWMOD_SUPPORT
	{ &g_netUnlock, "g_netUnlock", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_nmFlags, "g_nmFlags", "0", CVAR_ROM | CVAR_SERVERINFO, 0, qfalse },
	{ &g_enableNmAuth, "g_enableNmAuth", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
	{ &g_specInfo, "g_specInfo", "1", CVAR_ARCHIVE, 0, qtrue },
#endif
	{ &g_strafejump_mod,	"g_strafejump_mod"	, "0"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_fixSense,		"g_fixSense",		"1",		CVAR_ARCHIVE, 0, qtrue },
	{ &g_antiWallhack,	"g_antiWallhack"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_wallhackMaxTraces,	"g_wallhackMaxTraces"	, "1000"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_inMemoryDB, "g_inMemoryDB", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },

	{ &g_enableRacemode, "g_enableRacemode", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_enableAimPractice, "g_enableAimPractice", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
#ifdef _DEBUG
	{ &d_disableRaceVisChecks, "d_disableRaceVisChecks", "0", CVAR_TEMP, 0, qtrue },
#endif

	{ &g_traceSQL, "g_traceSQL", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },

    { &g_restart_countdown, "g_restart_countdown", "3", CVAR_ARCHIVE, 0, qtrue }, 

	{ &g_allow_vote_gametype,	"g_allow_vote_gametype"	, "1023"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_kick,	"g_allow_vote_kick"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_restart,	"g_allow_vote_restart"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_map,	"g_allow_vote_map"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_nextmap,	"g_allow_vote_nextmap"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_timelimit,	"g_allow_vote_timelimit"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_fraglimit,	"g_allow_vote_fraglimit"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_maprandom, "g_allow_vote_maprandom", "4", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_mapvote, "g_allow_vote_mapvote", "1", CVAR_ARCHIVE, 0, qtrue },
    { &g_allow_vote_warmup, "g_allow_vote_warmup", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_boon, "g_allow_vote_boon", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_allow_vote_instagib, "g_allow_vote_instagib", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_allowReady, "g_allowReady", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_allowVerify, "g_allowVerify", "-1", CVAR_ARCHIVE, 0, qfalse },

	{ &g_default_capturedifflimit, "g_default_capturedifflimit", "10", CVAR_ARCHIVE, 0, qtrue },
	{ &g_enable_maprandom_wildcard, "g_enable_maprandom_wildcard", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_redirectDoWarmupVote, "g_redirectDoWarmupVote", "cointoss", CVAR_ARCHIVE, 0, qtrue },
	{ &g_redirectNextMapVote, "g_redirectNextMapVote", "mapvote", CVAR_ARCHIVE, 0, qtrue },
	{ &g_redirectPoolVoteToTierListVote, "g_redirectPoolVoteToTierListVote", "", CVAR_ARCHIVE, 0, qfalse },

	{ &g_quietrcon,	"g_quietrcon"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_hackLog,	"g_hackLog"	, "hacks.log"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_npc_spawn_limit,	"g_npc_spawn_limit"	, "100"	, CVAR_ARCHIVE, 0, qtrue },

	{ &g_accounts,	"g_accounts"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_accountsFile,	"g_accountsFile"	, "accounts.txt"	, CVAR_ARCHIVE, 0, qtrue },
    { &g_whitelist, "g_whitelist", "0", CVAR_ARCHIVE, 0, qtrue },


	{ &g_dlURL,	"g_dlURL"	, ""	, CVAR_SYSTEMINFO, 0, qtrue },

	{ &g_enableBoon,	"g_enableBoon"	, "1"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_enableMemePickups,	"g_enableMemePickups", "0"	, CVAR_LATCH, 0, qtrue },

	{ &g_vote_tierlist, "g_vote_tierlist", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_s_min, "g_vote_tierlist_s_min", "4", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_s_max, "g_vote_tierlist_s_max", "4", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_a_min, "g_vote_tierlist_a_min", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_a_max, "g_vote_tierlist_a_max", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_b_min, "g_vote_tierlist_b_min", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_b_max, "g_vote_tierlist_b_max", "2", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_c_min, "g_vote_tierlist_c_min", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_c_max, "g_vote_tierlist_c_max", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_f_min, "g_vote_tierlist_f_min", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_f_max, "g_vote_tierlist_f_max", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_totalMaps, "g_vote_tierlist_totalMaps", "6", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_debug, "g_vote_tierlist_debug", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_reminders, "g_vote_tierlist_reminders", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_tierlist_fixShittyPools, "g_vote_tierlist_fixShittyPools", "2", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_betaMapForceInclude, "g_vote_betaMapForceInclude", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_vote_rng, "g_vote_rng", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_runoff, "g_vote_runoff", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_mapCooldownMinutes, "g_vote_mapCooldownMinutes", "60", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_runoffTimeModifier, "g_vote_runoffTimeModifier", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_teamgen, "g_vote_teamgen", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_redirectMapVoteToLiveVersion, "g_vote_redirectMapVoteToLiveVersion", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_printLiveVersionFullName, "g_vote_printLiveVersionFullName", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_overrideTrollVoters, "g_vote_overrideTrollVoters", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_teamgen_autoMapVoteNonAfkAutoVoteYesSeconds, "g_vote_teamgen_autoMapVoteNonAfkAutoVoteYesSeconds", "5", CVAR_ARCHIVE, 0, qtrue },
	{ &g_vote_runoffRerollOption, "g_vote_runoffRerollOption", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_vote_banMap, "g_vote_banMap", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_vote_banPhaseTimeModifier, "g_vote_banPhaseTimeModifier", "10", CVAR_ARCHIVE, 0, qtrue },

	{ &g_vote_teamgen_subhelp, "g_vote_teamgen_subhelp", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_rustWeeks, "g_vote_teamgen_rustWeeks", "12", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
	{ &g_vote_teamgen_minSecsSinceIntermission, "g_vote_teamgen_minSecsSinceIntermission", "30", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_enableAppeasing, "g_vote_teamgen_enableAppeasing", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_enableFirstChoice, "g_vote_teamgen_enableFirstChoice", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_enableInclusive, "g_vote_teamgen_enableInclusive", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_remindPositions, "g_vote_teamgen_remindPositions", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_remindToSetPositions, "g_vote_teamgen_remindToSetPositions", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_announceBreak, "g_vote_teamgen_announceBreak", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_autoRestartOnMapChange, "g_vote_teamgen_autoRestartOnMapChange", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_autoMapVoteSeconds, "g_vote_teamgen_autoMapVoteSeconds", "60", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_iterate, "g_vote_teamgen_iterate", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_preventStartDuringPug, "g_vote_teamgen_preventStartDuringPug", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_banLastPlayedPermutation, "g_vote_teamgen_banLastPlayedPermutation", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_enableBarVote, "g_vote_teamgen_enableBarVote", "5", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_barVoteStartsNewPug, "g_vote_teamgen_barVoteStartsNewPug", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_barVoteDoesntStartNewPugIfManyPlayers, "g_vote_teamgen_barVoteDoesntStartNewPugIfManyPlayers", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_unvote, "g_vote_teamgen_unvote", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_fuck, "g_vote_teamgen_fuck", "100", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_new8PlayerAlgo, "g_vote_teamgen_new8PlayerAlgo", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_require2VotesOnEachTeam, "g_vote_teamgen_require2VotesOnEachTeam", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_readBeforeVotingMilliseconds, "g_vote_teamgen_readBeforeVotingMilliseconds", "5000", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_readBeforeVotingMillisecondsJawa, "g_vote_teamgen_readBeforeVotingMillisecondsJawa", "10000", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_preventBindsWith8PlayersMilliseconds, "g_vote_teamgen_preventBindsWith8PlayersMilliseconds", "50", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_bImbalanceCapWith0OrTwoRerolls, "g_vote_teamgen_bImbalanceCapWith0OrTwoRerolls", "0.0382", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_bImbalanceCapWithOneReroll, "g_vote_teamgen_bImbalanceCapWithOneReroll", "0.0382", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_acdImbalanceCapWithoutReroll, "g_vote_teamgen_acdImbalanceCapWithoutReroll", "0.0182", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_acdImbalanceCapWithOneReroll, "g_vote_teamgen_acdImbalanceCapWithOneReroll", "0.0382", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_dynamicVoteRequirement, "g_vote_teamgen_dynamicVoteRequirement", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_freezeUntilVote, "g_vote_freezeUntilVote", "-1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_lessPlayedMapsDisfavoredInRunoffEliminations, "g_vote_lessPlayedMapsDisfavoredInRunoffEliminations", "10", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_fadeToBlack, "g_vote_fadeToBlack", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_mapVoteExtensions_maxExtensions, "g_vote_mapVoteExtensions_maxExtensions", "6", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_mapVoteExtension_ifUnderThisManyVotes, "g_vote_mapVoteExtension_ifUnderThisManyVotes", "5", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_mapVoteExtension_extensionDuration, "g_vote_mapVoteExtension_extensionDuration", "10", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_audioMotivation, "g_vote_audioMotivation", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_notifyTeammatesOfMapChoice, "g_vote_notifyTeammatesOfMapChoice", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_underdogTeamMapVoteTiebreakerThreshold, "g_vote_underdogTeamMapVoteTiebreakerThreshold", "0.519", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_fuckRequiredVotes, "g_vote_fuckRequiredVotes", "3", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_preventSwitchingTeamsDuringMapVote, "g_vote_preventSwitchingTeamsDuringMapVote", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_autoAdjustRequiredPugVotes, "g_vote_teamgen_autoAdjustRequiredPugVotes", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_sumOfSquaresTiebreaker, "g_vote_teamgen_sumOfSquaresTiebreaker", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_aDietB, "g_vote_teamgen_aDietB", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_vote_teamgen_displayCaliber, "g_vote_teamgen_displayCaliber", "1", CVAR_ARCHIVE, 0, qfalse },

	{ &g_filterSlurs, "g_filterSlurs", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
	{ &g_filterUsers, "g_filterUsers", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
	{ &g_lockdown, "g_lockdown", "0", CVAR_ARCHIVE, 0, qfalse },

	{ &g_addItems, "g_addItems", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
	{ &g_addItemsWhitelist, "g_addItemsWhitelist", "", CVAR_ARCHIVE, 0, qfalse },
	{ &g_deleteBaseItems, "g_deleteBaseItems", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
	{ &g_deleteBaseItemsWhitelist, "g_deleteBaseItemsWhitelist", "", CVAR_ARCHIVE, 0, qfalse },

	{ &g_broadcastCtfPos, "g_broadcastCtfPos", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse },
	{ &g_assignMissingCtfPos, "g_assignMissingCtfPos", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_broadcastGivenThTe, "g_broadcastGivenThTe", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_broadcastCapturerHealthArmorForce, "g_broadcastCapturerHealthArmorForce", "1", CVAR_ARCHIVE, 0, qfalse },
	{ &g_outOfBandDMs, "g_outOfBandDMs", "1", CVAR_ARCHIVE, 0, qfalse },

	{ &d_debugBanPermutation, "d_debugBanPermutation", "0", CVAR_ARCHIVE, 0, qfalse },

#ifdef _DEBUG
	{ &g_lastIntermissionStartTime, "g_lastIntermissionStartTime", "", CVAR_TEMP, 0, qfalse },
	{ &g_lastTeamGenTime, "g_lastTeamGenTime", "", CVAR_TEMP, 0, qfalse },
	{ &g_lastMapVotedMap, "g_lastMapVotedMap", "", CVAR_TEMP, 0, qfalse },
	{ &g_lastMapVotedTime, "g_lastMapVotedTime", "", CVAR_TEMP, 0, qfalse },
#else
	{ &g_lastIntermissionStartTime, "g_lastIntermissionStartTime", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },
	{ &g_lastTeamGenTime, "g_lastTeamGenTime", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },
	{ &g_lastMapVotedMap, "g_lastMapVotedMap", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },
	{ &g_lastMapVotedTime, "g_lastMapVotedTime", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },
#endif

	{ &d_debugCtfPosCalculation, "d_debugCtfPosCalculation", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &d_debugSpawns, "d_debugSpawns", "0", CVAR_ARCHIVE, 0, qfalse },

	{ &g_notFirstMap, "g_notFirstMap", "0", CVAR_ROM | CVAR_TEMP, 0, qfalse },
	{ &r_rngNum, "r_rngNum", "0", CVAR_ROM | CVAR_TEMP, 0, qfalse },
	{ &r_rngNumSet, "r_rngNumSet", "0", CVAR_ROM | CVAR_TEMP, 0, qfalse },

	{ &g_bannedPermutationHash, "g_bannedPermutationHash", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },
	{ &g_bannedPermutationTime, "g_bannedPermutationTime", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },
	{ &g_lastSelectedPermutationHash, "g_lastSelectedPermutationHash", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },
	{ &g_lastSelectedPermutationTime, "g_lastSelectedPermutationTime", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },
	{ &g_lastSelectedPositionlessPermutation, "g_lastSelectedPositionlessPermutation", "", CVAR_ROM | CVAR_TEMP, 0, qfalse },

	{ &g_allowMoveDisable, "g_allowMoveDisable", "-1", CVAR_ARCHIVE | CVAR_SERVERINFO, 0, qtrue },

	{ &g_rockPaperScissors, "g_rockPaperScissors", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_gripRefreshRate, "g_gripRefreshRate", "300", CVAR_ARCHIVE, 0, qtrue },
	{ &g_gripAbsorbFix, "g_gripAbsorbFix", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_protect3DamageReduction, "g_protect3DamageReduction", "0.80", CVAR_ARCHIVE, 0, qtrue },
	{ &g_lightningRework, "g_lightningRework", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_mindTrickBuff, "g_mindTrickBuff", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_gripRework, "g_gripRework", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_drainRework, "g_drainRework", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_fixForceTimers, "g_fixForceTimers", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_thTeRequiresLOS, "g_thTeRequiresLOS", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &d_debugThTeLOS, "d_debugThTeLOS", "0", CVAR_ARCHIVE, 0, qfalse },
	{ &g_thTeUnlagged, "g_thTeUnlagged", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &d_logTEData, "d_logTEData", "0", CVAR_ARCHIVE, 0, qfalse },
	{ &g_fixPushPullLOS, "g_fixPushPullLOS", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &d_debugPushPullLOS, "d_debugPushPullLOS", "0", CVAR_ARCHIVE, 0, qfalse },

	{ &g_sendForceTimers, "g_sendForceTimers", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_allowSkAutoThTe, "g_allowSkAutoThTe", "-1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_minimumCullDistance, "g_minimumCullDistance", "10000", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_selfkill_penalty, "g_selfkill_penalty", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_flags_overboarding, "g_flags_overboarding", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_moreTaunts, "g_moreTaunts", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_raceEmotes, "g_raceEmotes", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_ragersCanCounterPushPull, "g_ragersCanCounterPushPull", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoPause999, "g_autoPause999", "5", CVAR_ARCHIVE, 0, qtrue },
	{ &g_autoPauseDisconnect, "g_autoPauseDisconnect", "2", CVAR_ARCHIVE, 0, qtrue },
	{ &g_quickPauseMute, "g_quickPauseMute", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_enterSpammerTime, "g_enterSpammerTime", "3", CVAR_ARCHIVE, 0, qtrue },
	{ &g_quickPauseChat, "g_quickPauseChat", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_pauseVelocityLines, "g_pauseVelocityLines", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_webhookId, "g_webhookId", "", CVAR_ARCHIVE, 0, qfalse },
	{ &g_webhookToken, "g_webhookToken", "", CVAR_ARCHIVE, 0, qfalse },

	{ &g_teamOverlayForce, "g_teamOverlayForce", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_teamOverlayForceAlignment, "g_teamOverlayForceAlignment", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_broadcastForceLoadouts, "g_broadcastForceLoadouts", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &g_teamPrivateDuels, "g_teamPrivateDuels", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_teamOverlayUpdateRate, "g_teamOverlayUpdateRate", "250", CVAR_ARCHIVE, 0, qtrue },

	{ &debug_clientNumLog, "debug_clientNumLog", "0", CVAR_ARCHIVE, 0, qtrue},

	{ &g_improvedHoming, "g_improvedHoming", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_improvedHomingThreshold, "g_improvedHomingThreshold", "500", CVAR_ARCHIVE, 0, qtrue },
	{ &d_debugImprovedHoming, "d_debugImprovedHoming", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_customVotes, "g_customVotes", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote1_command, "g_customVote1_command", "map_restart", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote1_label, "g_customVote1_label", "Restart map", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote2_command, "g_customVote2_command", "cointoss", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote2_label, "g_customVote2_label", "Coin toss", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote3_command, "g_customVote3_command", "pause", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote3_label, "g_customVote3_label", "Pause", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote4_command, "g_customVote4_command", "unpause", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote4_label, "g_customVote4_label", "Unpause", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote5_command, "g_customVote5_command", "resetflags", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote5_label, "g_customVote5_label", "Reset flags", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote6_command, "g_customVote6_command", "endmatch", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote6_label, "g_customVote6_label", "End match", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote7_command, "g_customVote7_command", "nextmap", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote7_label, "g_customVote7_label", "Next map", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote8_command, "g_customVote8_command", "", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote8_label, "g_customVote8_label", "", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote9_command, "g_customVote9_command", "", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote9_label, "g_customVote9_label", "", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_customVote10_command, "g_customVote10_command", "", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
	{ &g_customVote10_label, "g_customVote10_label", "", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },

	{ &g_braindeadBots, "g_braindeadBots", "0", CVAR_ARCHIVE, 0 , qtrue },

	{ &g_maxstatusrequests,	"g_maxstatusrequests"	, "50"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_testdebug,	"g_testdebug"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	
	{ &g_logrcon,	"g_logrcon"	, "0"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_rconpassword,	"rconpassword"	, "0"	, CVAR_ARCHIVE | CVAR_INTERNAL },

	{ &g_callvotedelay,	"g_callvotedelay"	, "0"	, CVAR_ARCHIVE | CVAR_INTERNAL },
    { &g_callvotemaplimit,	"g_callvotemaplimit"	, "0"	, CVAR_ARCHIVE | CVAR_INTERNAL },

	{ &g_mapVotePlayers,	"g_mapVotePlayers"	, "4"	, CVAR_ARCHIVE, 0, qtrue },
	{ &g_mapVoteThreshold,	"g_mapVoteThreshold", "10"	, CVAR_ARCHIVE, 0, qtrue },
    
    { &sv_privateclients, "sv_privateclients", "0", CVAR_ARCHIVE | CVAR_SERVERINFO },
	{ &sv_passwordlessSpectators, "sv_passwordlessSpectators", "0", CVAR_ARCHIVE | CVAR_SERVERINFO },
    { &g_defaultBanHoursDuration, "g_defaultBanHoursDuration", "24", CVAR_ARCHIVE | CVAR_INTERNAL },  

	{ &g_floatingItems, "g_floatingItems", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_rocketSurfing, "g_rocketSurfing", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_bouncePadDoubleJump, "g_bouncePadDoubleJump", "1", CVAR_ARCHIVE, 0, qtrue },

	{ &d_measureAirTime, "d_measureAirTime", "0", CVAR_TEMP, 0, qtrue },

	{ &g_notifyAFK, "g_notifyAFK", "5", CVAR_ARCHIVE, 0, qtrue },
	{ &g_waitForAFK, "g_waitForAFK", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_waitForAFKTimer, "g_waitForAFKTimer", "10", CVAR_ARCHIVE, 0, qtrue },

	{ &g_waitForAFKThreshold, "g_waitForAFKThreshold", "5", CVAR_ARCHIVE, 0, qtrue },
	{ &g_waitForAFKThresholdTroll, "g_waitForAFKThresholdTroll", "2", CVAR_ARCHIVE, 0, qtrue },
	{ &g_waitForAFKMinPlayers, "g_waitForAFKMinPlayers", "6", CVAR_ARCHIVE, 0, qtrue },

	{ &g_unlagged, "g_unlagged", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue },
#ifdef _DEBUG
	{ &g_unlaggedMaxCompensation, "g_unlaggedMaxCompensation", "500", CVAR_ARCHIVE, 0, qtrue },
	{ &g_unlaggedSkeletons, "g_unlaggedSkeletons", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_unlaggedSkeletonTime, "g_unlaggedSkeletonTime", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_unlaggedFactor, "g_unlaggedFactor", "0.25", CVAR_ARCHIVE, 0, qtrue },
	{ &g_unlaggedOffset, "g_unlaggedOffset", "0", CVAR_ARCHIVE, 0, qtrue },
	{ &g_unlaggedDebug, "g_unlaggedDebug", "0", CVAR_ARCHIVE, 0, qtrue },
#endif
	{ &g_unlaggedFix, "g_unlaggedFix", "0", CVAR_ARCHIVE, 0, qtrue },

	{ &g_printCountry, "g_printCountry", "1", CVAR_ARCHIVE, 0, qtrue },
	{ &g_redirectWrongThTeBinds, "g_redirectWrongThTeBinds", "1", CVAR_ARCHIVE, 0, qtrue },
};

// bk001129 - made static to avoid aliasing
static int gameCvarTableSize = sizeof( gameCvarTable ) / sizeof( gameCvarTable[0] );


void G_InitGame					( int levelTime, int randomSeed, int restart, void *serverDbPtr );
void G_RunFrame					( int levelTime );
void G_ShutdownGame				( int restart );
void CheckExitRules				( void );
void G_ROFF_NotetrackCallback	( gentity_t *cent, const char *notetrack);

void G_UpdateNonClientBroadcasts( gentity_t *self );

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
		G_InitGame( arg0, arg1, arg2, (void *)arg3 );
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
		if (level.activePugProposal && g_vote_teamgen.integer)
			PrintTeamsProposalsInConsole(level.activePugProposal, arg0);
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

	// base_enhanced

	case GAME_TRANSFER_RESULT:
		G_HandleTransferResult((trsfHandle_t)arg0, (trsfErrorInfo_t*)arg1, arg2, (void*)arg3, (size_t)arg4);
		return 0;
	case GAME_RCON_COMMAND:
		G_LogRconCommand((const char*)arg0, (const char*)arg1);
		return 0;
	case GAME_STATUS:
		G_Status();
		return 0;
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
	static char		text[4096] = { 0 };

	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	text[4096-1] = '\0';

	trap_Printf( text );
}

void QDECL G_Error( const char *fmt, ... ) {
	va_list		argptr;
	static char		text[BUFFER_TEMP_LEN] = { 0 };

	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
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

static void CheckForAFKs(void) {
	if (!level.wasRestarted)
		return;

	int numRed = 0, numBlue = 0;
	enum {
		CLIENTNUMAFK_MULTIPLE = -2,
		CLIENTNUMAFK_NONE = -1
	} clientNumAfk = CLIENTNUMAFK_NONE;

	for (int i = 0; i < level.maxclients; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client)
			continue;
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->sess.sessionTeam == TEAM_FREE)
			continue;

		// this guy is ingame
		if (ent->client->sess.sessionTeam == TEAM_RED)
			numRed++;
		else if (ent->client->sess.sessionTeam == TEAM_BLUE)
			numBlue++;

		// make sure LS took more than a couple steps
		const float AFKTROLL_DISPLACEMENT_THRESHOLD = 600.0f;

		if (!ent->client->pers.hasDoneSomething || (ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_AFKTROLL) && ent->client->stats && ent->client->stats->displacement < AFKTROLL_DISPLACEMENT_THRESHOLD)) {
			// this guy is afk
			if (clientNumAfk == CLIENTNUMAFK_NONE)
				clientNumAfk = i; // he's the only one afk (so far)
			else
				clientNumAfk = CLIENTNUMAFK_MULTIPLE; // someone else is already afk
		}
	}

	if (clientNumAfk == CLIENTNUMAFK_NONE)
		return;

	level.someoneWasAFK = qtrue;
		
	if (numRed < 4 || numBlue < 4) // only notify if 4v4+
		return;

	char *whoIsAfkString;
	if (clientNumAfk == CLIENTNUMAFK_MULTIPLE)
		whoIsAfkString = "Multiple players are AFK";
	else
		whoIsAfkString = va("%s^7 is AFK", level.clients[clientNumAfk].pers.netname);

	trap_SendServerCommand(-1, va("print \"^1Pug is not live:^7 %s^7\n\"", whoIsAfkString));
	trap_SendServerCommand(-1, va("cp \"^1Pug is not live:^7\n%s^7\n\"", whoIsAfkString));
}

char gSharedBuffer[MAX_G_SHARED_BUFFER_SIZE];

#include "namespace_begin.h"
void WP_SaberLoadParms( void );
void BG_VehicleLoadParms( void );
#include "namespace_end.h"

static void LoadPepper(void) {
	fileHandle_t f = 0;
	int len = trap_FS_FOpenFile("pepper.bin", &f, FS_READ);

	if (len != PEPPER_CHARS) {
		if (f)
			trap_FS_FCloseFile(f);

		trap_FS_FOpenFile("pepper.bin", &f, FS_WRITE);

		if (!f) {
			Com_Error(ERR_FATAL, "Unable to acquire file handle on pepper");
			return;
		}

		unsigned char arr[PEPPER_CHARS];
		randombytes_buf(arr, PEPPER_CHARS);
		memcpy(level.pepper, arr, PEPPER_CHARS);
		trap_FS_Write(arr, PEPPER_CHARS, f);		
	}
	else {
		trap_FS_Read(&level.pepper, len, f);
	}

	trap_FS_FCloseFile(f);
}

void InitSlurs(void) {
	if (!g_filterSlurs.integer)
		return;

	const char *filename = "slurs.txt";
	fileHandle_t f;
	int len;

	len = trap_FS_FOpenFile(filename, &f, FS_READ);
	if (!f) {
		Com_Printf("No %s file found.\n", filename);
		return;
	}

	char *buf = malloc(len + 1);
	trap_FS_Read(buf, len, f);
	buf[len] = 0;
	trap_FS_FCloseFile(f);

	char *start = buf, *end = NULL;
	while (VALIDSTRING(start)) {
		// find the end of the line
		end = start;
		while (*end && *end != '\n' && *end != '\r')
			++end;

		// check if line is empty or just whitespace
		qboolean isEmpty = qtrue;
		for (char *ptr = start; ptr < end; ++ptr) {
			if (!isspace((unsigned)*ptr)) {
				isEmpty = qfalse;
				break;
			}
		}

		if (!isEmpty) {
			// add to list
			*end = '\0';
			slur_t *slur = (slur_t *)ListAdd(&level.slurList, sizeof(slur_t));
			slur->text = strdup(start);
			for (unsigned char *p = slur->text; *p; p++)
				*p = (unsigned char)(tolower(*p));
		}

		// move to the next line
		start = end + 1;
		if (start - buf >= len)  // exceeded buffer?
			break;

		while (*start == '\n' || *start == '\r') {
			++start;
			if (start - buf >= len)  // exceeded buffer?
				break;
		}
	}

	free(buf);

	Com_Printf("%d slurs loaded.\n", level.slurList.size);
}


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
extern void G_LoadHelpFile( const char *filename );
extern const char *G_GetArenaInfoByMap( char *map );
extern qboolean doneSpawningBaseItems;

void G_InitGame( int levelTime, int randomSeed, int restart, void *serverDbPtr ) {
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

#if 0
	// per teh, this has been manually commented out in builds for months; adding #if 0 to eliminate that need
	srand( randomSeed );
#endif

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

	if (restart && !g_wasIntermission.integer)
		level.wasRestarted = qtrue;

	trap_Cvar_Set("g_wasIntermission", "0");

	if (!restart)
	{
		trap_Cvar_Set("g_wasRestarted", "0");
	}

#ifdef NEWMOD_SUPPORT
	level.nmAuthEnabled = qfalse;

	if (Crypto_Init(G_Printf) == CRYPTO_ERROR) {
		Com_Error(ERR_FATAL, "Crypto_Init failed!");
		return;
	}

	if ( g_enableNmAuth.integer ) {
		if ( Crypto_LoadKeysFromFiles( &level.publicKey, PUBLIC_KEY_FILENAME, &level.secretKey, SECRET_KEY_FILENAME ) != CRYPTO_ERROR ) {
			// got the keys, all is good
			G_Printf( "Loaded crypto key files from disk successfully\n" );
			level.nmAuthEnabled = qtrue;
		} else if ( Crypto_GenerateKeys( &level.publicKey, &level.secretKey ) != CRYPTO_ERROR ) {
			// either first run or file couldnt be read. whatever, we got a key pair for this session
			G_Printf( "Generated new crypto key pair successfully:\nPUBLIC=%s\nSECRET=%s\n", level.publicKey.keyHex, level.secretKey.keyHex );
			level.nmAuthEnabled = qtrue;

			// save them to disk for the next time
			Crypto_SaveKeysToFiles( &level.publicKey, PUBLIC_KEY_FILENAME, &level.secretKey, SECRET_KEY_FILENAME );
		}
	}

	if ( !level.nmAuthEnabled ) {
		G_Printf( S_COLOR_RED"Newmod auth support is not active. Some functionality will be unavailable for these clients.\n" );
	}
#endif

	LoadPepper();

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

	// read accounts and sessions cache
	qboolean resetAccountsCache = !G_ReadAccountsCache();

	G_ReadSessionData( restart ? qtrue : qfalse, resetAccountsCache );

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

	Q_strncpyz(level.mapname, mapname.string, sizeof(level.mapname));

	navCalculatePaths	= ( trap_Nav_Load( mapname.string, ckSum.integer ) == qfalse );

	// parse the key/value pairs and spawn gentities
	doneSpawningBaseItems = qfalse; // should already be qfalse but whatever
	G_SpawnEntitiesFromString(qfalse);
	doneSpawningBaseItems = qtrue;

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

	G_LoadHelpFile( "help.txt" );

	G_DBLoadDatabase(serverDbPtr);

	G_DBInitializePugStatsCache();
	G_DBCacheAutoLinks();
	
	if (g_addItems.integer)
		DB_LoadAddedItems();

	LoadAimPacks();

	DB_LoadFilters();

	// only enable racemode records in ctf
	level.racemodeRecordsEnabled = g_gametype.integer == GT_CTF && g_enableRacemode.integer;
	level.topAimRecordsEnabled = g_gametype.integer == GT_CTF && g_enableAimPractice.integer;

	// reset capturedifflimit on map rs
	trap_Cvar_Set( "capturedifflimit", g_default_capturedifflimit.string );

	G_BroadcastServerFeatureList(-1);

	G_InitVchats();

	TeamGen_Initialize();
	G_DBGetPlayerRatings();

	SetInitialSpawns();
	SetInitialRespawns();

	if (restart) {
		// save g_lastIntermissionStartTime before we clear it
		level.g_lastIntermissionStartTimeSettingAtRoundStart = g_lastIntermissionStartTime.string[0] ? g_lastIntermissionStartTime.integer : 0;
		level.g_lastTeamGenTimeSettingAtRoundStart = 0;

		if (level.g_lastIntermissionStartTimeSettingAtRoundStart)
			level.shouldAnnounceBreak = qtrue;
	}
	else {
		level.g_lastIntermissionStartTimeSettingAtRoundStart = 0;
		level.g_lastTeamGenTimeSettingAtRoundStart = g_lastTeamGenTime.string[0] ? g_lastTeamGenTime.integer : 0;
		level.shouldAnnounceBreak = qfalse;

		if (g_lastTeamGenTime.integer)
			TeamGen_DoAutoRestart();
	}

	if (d_bowcasterRework_enable.integer) {
		weaponData[WP_BOWCASTER].maxCharge = 1100;
		weaponData[WP_BOWCASTER].chargeSubTime = 500;
	}
	else {
		weaponData[WP_BOWCASTER].maxCharge = 1700;
		weaponData[WP_BOWCASTER].chargeSubTime = 400;
	}

	InitSlurs();

	if (g_lockdown.integer)
		Com_Printf("^3Warning: g_lockdown is enabled!^7\n");

	// always clear these so that they can only happen once
	trap_Cvar_Set("g_lastIntermissionStartTime", "");
	trap_Cvar_Set("g_lastTeamGenTime", "");
}



/*
=================
G_ShutdownGame
=================
*/
void G_ShutdownGame( int restart ) {
	int numRedPlayers = 0, numBluePlayers = 0;
	CountPlayers(NULL, &numRedPlayers, &numBluePlayers, NULL, NULL, NULL, NULL);
	trap_Cvar_Set("g_numRedPlayers", va("%d", numRedPlayers));
	trap_Cvar_Set("g_numBluePlayers", va("%d", numBluePlayers));

	trap_Cvar_Set("g_notFirstMap", "1");

	if (level.shouldClearRemindPositionsAtEnd)
		TeamGen_ClearRemindPositions(qtrue);

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

	// save accounts and sessions cache
	G_SaveAccountsCache();

	trap_ROFF_Clean();

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAIShutdown( restart );
	}

	B_CleanupAlloc(); //clean up all allocations made with B_Alloc

	// accounts system
	//cleanDB();

	SaveDeleteAndFreeAimPacks();

    G_DBUnloadDatabase();

	ListClear(&level.redPlayerTickList);
	ListClear(&level.bluePlayerTickList);
	ListClear(&level.disconnectedPlayerList);

	ListClear(&level.info_b_e_locationsList);

	ListClear(&level.rememberedMultivoteMapsList);

	iterator_t iter;
	ListIterate(&level.statsList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		stats_t *stats = IteratorNext(&iter);
		ListClear(&stats->damageGivenList);
		ListClear(&stats->damageTakenList);
		ListClear(&stats->teammatePositioningList);
	}
	ListClear(&level.statsList);

	ListIterate(&level.cachedPositionStats, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		cachedPlayerPugStats_t *c = IteratorNext(&iter);
		if (c->strPtr)
			free(c->strPtr);
	}
	ListClear(&level.cachedPositionStats);
	ListClear(&level.cachedPositionStatsRaw);

	ListIterate(&level.cachedWinrates, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		cachedPlayerPugStats_t *c = IteratorNext(&iter);
		if (c->strPtr)
			free(c->strPtr);
	}
	ListClear(&level.cachedWinrates);

	ListIterate(&level.cachedPerMapWinrates, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		cachedPerMapWinrate_t *c = IteratorNext(&iter);
		if (c->strPtr)
			free(c->strPtr);
	}
	ListClear(&level.cachedPerMapWinrates);

	ListClear(&level.ratingList);
	ListClear(&level.mostPlayedPositionsList);

	ListIterate(&level.pugProposalsList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		pugProposal_t *p = IteratorNext(&iter);
		ListClear(&p->avoidedHashesList);
	}
	ListClear(&level.pugProposalsList);

	ListClear(&level.barredPlayersList);
	ListClear(&level.forcedPickablePermabarredPlayersList);

	ListIterate(&level.queuedServerMessagesList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		queuedServerMessage_t *msg = IteratorNext(&iter);
		if (msg->text)
			free(msg->text);
	}
	ListClear(&level.queuedServerMessagesList);

	ListIterate(&level.queuedChatMessagesList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		queuedChatMessage_t *msg = IteratorNext(&iter);
		if (msg->text)
			free(msg->text);
	}
	ListClear(&level.queuedChatMessagesList);
	ListClear(&level.autoLinksList);
	ListClear(&level.rustyPlayersList);
	ListClear(&level.barVoteList);
	ListClear(&level.unbarVoteList);
	ListClear(&level.captureList);
	ListClear(&level.fuckVoteList);
	ListClear(&level.goVoteList);
	ListClear(&level.addedItemsList);
	ListClear(&level.baseItemsList);

	ListClear(&level.filtersList);

	ListIterate(&level.slurList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		slur_t *slur = IteratorNext(&iter);
		if (slur->text)
			free(slur->text);
	}
	ListClear(&level.slurList);

	UnpatchEngine();
}



//===================================================================

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c and bg_*.c can link

void QDECL Com_Error ( int level, const char *error, ... ) {
	va_list		argptr;
	char		text[1024] = { 0 };

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	G_Error( "%s", text);
}

void QDECL Com_Printf( const char *msg, ... ) {
	va_list		argptr;
	char		text[4096] = { 0 };

	va_start (argptr, msg);
	vsnprintf (text, sizeof(text), msg, argptr);
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
		G_ApplyRaceBroadcastsToEvent( ent, tent );
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
				if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR && !(g_gametype.integer == GT_CTF && level.clients[i].sess.sessionTeam == TEAM_FREE))
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
	if (g_gametype.integer >= GT_TEAM) {
		// if in ctf and there are no flags at all on this map, just show total frags
		if (g_gametype.integer == GT_CTF) {
			static qboolean checkedForFlags = qfalse, thereAreFlags = qfalse;
			if (!checkedForFlags) {
				checkedForFlags = qtrue;
				if (G_Find(NULL, FOFS(classname), "team_CTF_redflag") && G_Find(NULL, FOFS(classname), "team_CTF_blueflag"))
					thereAreFlags = qtrue;
			}

			if (thereAreFlags) {
				trap_SetConfigstring(CS_SCORES1, va("%i", level.teamScores[TEAM_RED]));
				trap_SetConfigstring(CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE]));
			}
			else {
				int redFrags = 0, blueFrags = 0;
				for (int i = 0; i < level.maxclients; i++) {
					if (level.clients[i].pers.connected != CON_CONNECTED)
						continue;
					if (level.clients[i].sess.sessionTeam == TEAM_RED)
						redFrags += level.clients[i].ps.persistant[PERS_SCORE];
					else if (level.clients[i].sess.sessionTeam == TEAM_BLUE)
						blueFrags += level.clients[i].ps.persistant[PERS_SCORE];
				}
				trap_SetConfigstring(CS_SCORES1, va("%i", redFrags));
				trap_SetConfigstring(CS_SCORES2, va("%i", blueFrags));
			}
		}
		else {
			trap_SetConfigstring(CS_SCORES1, va("%i", level.teamScores[TEAM_RED]));
			trap_SetConfigstring(CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE]));
		}
	}
	else {
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
		if ( level.clients[ i ].pers.connected == CON_CONNECTED && !level.clients[i].isLagging ) {
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
	ExitAimTraining(ent);
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

// a pug is considered live if:
// * the level was map_restarted
// * the average rounded integer number of players in each team is equal
// * the sum of these average integers is >= 4 (at least 2s)
// * both averages are within +/- 0.1 of their rounded values
// (accounts for subs, ragequits, random joins... 0.1 represents 2 mins of a 20 mins pug)
// we lock in the result at 18 minutes to account for scores of idiots joining in the last couple minutes, as long as at least one person is still ingame
// if live, returns the number of players per team; else returns 0
int IsLivePug(int ofAtLeastThisMinutes) {
	assert(ofAtLeastThisMinutes < 18);

	if (g_gametype.integer != GT_CTF || !level.wasRestarted || !level.numTeamTicks)
		return 0;

	static qboolean didConfirmation = qfalse;
	static int confirmedPlayersPerTeam = 0;

	if (didConfirmation) {
		if (confirmedPlayersPerTeam > 0) {
			int ingamePlayers = 0;
			CountPlayers(NULL, NULL, NULL, NULL, NULL, &ingamePlayers, NULL);
			return ingamePlayers > 0 ? confirmedPlayersPerTeam : 0;
		}
		else {
			return 0;
		}
	}

	float avgRed = (float)level.numRedPlayerTicks / (float)level.numTeamTicks;
	float avgBlue = (float)level.numBluePlayerTicks / (float)level.numTeamTicks;

	int avgRedInt = (int)lroundf(avgRed);
	int avgBlueInt = (int)lroundf(avgBlue);

	int durationMins = (level.time - level.startTime) / 60000;

	qboolean isLive = (!(ofAtLeastThisMinutes > 0 && durationMins < ofAtLeastThisMinutes) &&
		avgRedInt == avgBlueInt &&
		avgRedInt + avgBlueInt >= 4 &&
		fabs(avgRed - round(avgRed)) < 0.1f &&
		fabs(avgBlue - round(avgBlue)) < 0.1f);

	// lock in the result at 18 minutes
	if (durationMins >= 18) {
		didConfirmation = qtrue;
		confirmedPlayersPerTeam = isLive ? avgRedInt : 0;
	}

	return isLive ? avgRedInt : 0;
}

qboolean DuelLimitHit(void);

/*
==================
BeginIntermission
==================
*/
//ghost debug

void BeginIntermission(void) {
	int			i;
	gentity_t* client;

	if (level.intermissiontime) {
		return;		// already active
	}

	trap_Cvar_Set("g_wasRestarted", "0");
	trap_Cvar_Set("g_wasIntermission", "1");

	// if in tournement mode, change the wins / losses
	if (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) {
		trap_SetConfigstring(CS_CLIENT_DUELWINNER, "-1");

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

	level.intermissiontime = level.time;
	FindIntermissionPoint();

	//what the? Well, I don't want this to happen.

	// move all clients to the intermission point
	for (i = 0; i < level.maxclients; i++) {
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
		MoveClientToIntermission(client);
	}

	// send the current scoring to all clients
	SendScoreboardMessageToAllClients();

	FinalizeCTFPositions();
	CheckAccountsOfOldBlocks(-1);

	const int statsBufSize = 65536;
	char *statsBuf = calloc(65536, sizeof(char));

	if (g_autoStats.integer) {
		Stats_Print(NULL, "general", statsBuf, statsBufSize, qtrue, NULL);
		Stats_Print(NULL, "damage", statsBuf, statsBufSize, qtrue, NULL);
		Stats_Print(NULL, "accuracy", statsBuf, statsBufSize, qtrue, NULL);
		Stats_Print(NULL, "weapon", statsBuf, statsBufSize, qfalse, NULL);
		Q_StripColor(statsBuf);

		// print each player their own individual weapon stats
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (g_entities[i].inuse && level.clients[i].pers.connected != CON_DISCONNECTED &&
				(level.clients[i].sess.sessionTeam == TEAM_RED || level.clients[i].sess.sessionTeam == TEAM_BLUE) &&
				level.clients[i].stats && StatsValid(level.clients[i].stats)) {
				Stats_Print(&g_entities[i], "weapon", NULL, 0, qtrue, level.clients[i].stats);
			}
		}
	}

	SendMachineFriendlyStats();

	char timeBuf[MAX_STRING_CHARS] = { 0 };
	Com_sprintf(timeBuf, sizeof(timeBuf), "%d", (int)time(NULL));
	trap_Cvar_Set("g_lastIntermissionStartTime", timeBuf);

	int isLivePugNumPlayersPerTeam = IsLivePug(5);

	if (level.numTeamTicks) {
		if (isLivePugNumPlayersPerTeam == 4)
			TeamGenerator_MatchComplete();

		// a pug is considered live if:
		// * the level was map_restarted
		// * the match lasted at least 5 mins
		// * the average rounded integer number of players in each team is equal
		// * the sum of these average integers is >= 4 (at least 2s)
		// * both averages are within +/- 0.1 of their rounded values
		// (accounts for subs, ragequits, random joins... 0.1 represents 2 mins of a 20 mins pug)
#ifdef DEBUG_CTF_POSITION_STATS
		if (level.wasRestarted) {
			G_PostScoreboardToWebhook(statsBuf);
			G_DBAddCurrentMapToPlayedMapsList();
			if (!InstagibEnabled()) {
				G_DBWritePugStats();
				G_DBSetMetadata("shouldReloadPlayerPugStats", "2");
			}
		}
#else
		if (isLivePugNumPlayersPerTeam) {
			// sanity check to make sure auto map vote works again after finishing a pug
			trap_Cvar_Set("g_lastMapVotedMap", "");
			trap_Cvar_Set("g_lastMapVotedTime", "");
			G_PostScoreboardToWebhook(statsBuf);
			G_DBAddCurrentMapToPlayedMapsList();
			if (isLivePugNumPlayersPerTeam == 4 && !InstagibEnabled() && level.teamScores[TEAM_RED] != level.teamScores[TEAM_BLUE]) { // only write stats to db in untied non-instagib 4v4
				G_DBWritePugStats();
				G_DBSetMetadata("shouldReloadPlayerPugStats", "2");
			}
			trap_Cvar_Set("r_rngNum", va("%d", Q_irand(-25, 25)));
		}
#endif
	}

	free(statsBuf);

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED)
			continue;
		if (ent->client->ps.fd.forcePowersActive & (1 << FP_SPEED))
			WP_ForcePowerStop(ent, FP_SPEED);
		if (ent->client->ps.fd.forcePowersActive & (1 << FP_PROTECT))
			WP_ForcePowerStop(ent, FP_PROTECT);
		if (ent->client->ps.fd.forcePowersActive & (1 << FP_RAGE))
			WP_ForcePowerStop(ent, FP_RAGE);
		if (ent->client->ps.fd.forcePowersActive & (1 << FP_ABSORB))
			WP_ForcePowerStop(ent, FP_ABSORB);
		if (ent->client->ps.fd.forcePowersActive & (1 << FP_SEE))
			WP_ForcePowerStop(ent, FP_SEE);
	}

	level.shouldClearRemindPositionsAtEnd = qtrue;
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

	// save accounts and sessions cache
	G_SaveAccountsCache();

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
	char		string[1024] = { 0 };
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
	vsnprintf( string +len , sizeof(string) - len, fmt,argptr );
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
	static char		string[2048] = { 0 };
	int len;

	if ( !level.hackLogFile && !level.logFile) {
		return;
	}

	len = Com_sprintf( string, sizeof(string), va("[%s] ",getCurrentTime()) );

	va_start( argptr, fmt );
	vsnprintf( string+len, sizeof(string) - len, fmt, argptr );
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
	static char		string[2048] = { 0 };
	int len;

	if ( !level.DBLogFile ) {
		return;
	}

	len = Com_sprintf( string, sizeof(string), va("[%s] ",getCurrentTime()) );

	va_start( argptr, fmt );
	vsnprintf( string+len, sizeof(string) - len, fmt, argptr );
	va_end( argptr );

	trap_FS_Write( string, strlen( string ), level.DBLogFile );
}

//RCON log
void QDECL G_RconLog( const char *fmt, ... ) {
	va_list		argptr;
	static char		string[2048] = { 0 };
	int len;

	if ( !level.rconLogFile) {
		return;
	}

	len = Com_sprintf( string, sizeof(string), va("[%s] ",getCurrentTime()) );

	va_start( argptr, fmt );
	vsnprintf( string+len, sizeof(string) - len, fmt, argptr );
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

	// don't do delay when ending due to timelimit since it just feels janky and there's no frag to see
	int delay = (VALIDSTRING(string) && !strcmp(string, "Timelimit hit.")) ? 0 : INTERMISSION_DELAY_TIME;
	level.intermissionQueued = level.time + delay;


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

		if ((cl->ps.powerups[PW_BLUEFLAG] || cl->ps.powerups[PW_REDFLAG]) && !cl->sess.inRacemode){
			const int thisFlaghold = G_GetAccurateTimerOnTrigger( &cl->pers.teamState.flagsince, &g_entities[level.sortedClients[i]], NULL );

			cl->stats->totalFlagHold += thisFlaghold;

			if ( thisFlaghold > cl->stats->longestFlagHold )
				cl->stats->longestFlagHold = thisFlaghold;

			if ( cl->ps.powerups[PW_REDFLAG] ) {
				// carried the red flag, so blue team
				level.blueTeamRunFlaghold += thisFlaghold;
			} else if ( cl->ps.powerups[PW_BLUEFLAG] ) {
				// carried the blue flag, so red team
				level.redTeamRunFlaghold += thisFlaghold;
			}
		}

		if ( cl->ps.fd.forcePowersActive & ( 1 << FP_PROTECT ) ) {
			if ( cl->pers.protsince && cl->pers.protsince < level.time ) {
				cl->stats->protTimeUsed += level.time - cl->pers.protsince;
			}
		}

		if (cl->ps.fd.forcePowersActive & (1 << FP_RAGE)) {
			if (cl->pers.ragesince && cl->pers.ragesince < level.time) {
				cl->stats->rageTimeUsed += level.time - cl->pers.ragesince;
			}
		}

		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

		G_LogPrintf( "score: %i  ping: %i  client: %i %s\n", cl->ps.persistant[PERS_SCORE], ping, level.sortedClients[i],	cl->pers.netname );
	}

	// reset the lock on teams if the game ended normally
	trap_Cvar_Set( "g_maxgameclients", "0" );
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
			if ( i < 15 ) {
				readyMask |= 1 << i;
			}
		} else if (cl->sess.sessionTeam != TEAM_SPECTATOR && !(g_gametype.integer == GT_CTF && cl->sess.sessionTeam == TEAM_FREE)) {
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
		if ( level.time >= level.intermissionQueued ) {
			level.intermissionQueued = 0;
			BeginIntermission();
		}
				return;
			}

	if (!g_nonLiveMatchesCanEnd.integer && g_gametype.integer == GT_CTF && !level.wasRestarted)
		return;

	// check for sudden death
	if (g_gametype.integer != GT_SIEGE)
	{
		
		if ( ScoreIsTied() ) {

			//log we came overtime
			if ( level.time - level.startTime >= g_timelimit.integer*60000 ){
				level.overtime = qtrue;
				static qboolean announcedOvertime = qfalse;
				if (level.wasRestarted && !announcedOvertime) {
					trap_SendServerCommand(-1, "print \"Overtime started.\n\"");
					announcedOvertime = qtrue;
				}
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
				
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Timelimit hit (1)\n");
				}

				if (level.overtime) {
					trap_SendServerCommand(-1, "print \"Overtime ended.\n\"");
					LogExit("Overtime ended.");
				}
				else {
					trap_SendServerCommand(-1, va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "TIMELIMIT_HIT")));
					LogExit("Timelimit hit.");
				}

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

extern void Svcmd_MapVote_f(const char *overrideMaps);
static void DoMapBans(void) {
	if (!level.voteBanPhase || !level.multiVoting) {
		assert(qfalse);
		return;
	}

	int banVotesRed[MAX_MULTIVOTE_MAPS] = { 0 };
	int banVotesBlue[MAX_MULTIVOTE_MAPS] = { 0 };
	char remainingMaps[MAX_STRING_CHARS] = { 0 };
	int redChaseVoteIndex = -1, blueChaseVoteIndex = -1;
	gentity_t *redChasePlayer = NULL, *blueChasePlayer = NULL;

	// tally the votes for each map
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || !ent->client->pers.connected) {
			continue;
		}

		const int voteId = level.multiVoteBanVotes[i];
		if (voteId > 0 && voteId <= MAX_MULTIVOTE_MAPS) {
			if (ent->client->sess.sessionTeam == TEAM_RED) {
				banVotesRed[voteId - 1]++;
				if (GetRemindedPosOrDeterminedPos(ent) == CTFPOSITION_CHASE) {
					redChaseVoteIndex = voteId - 1;
					redChasePlayer = ent;
				}
			}
			else if (ent->client->sess.sessionTeam == TEAM_BLUE) {
				banVotesBlue[voteId - 1]++;
				if (GetRemindedPosOrDeterminedPos(ent) == CTFPOSITION_CHASE) {
					blueChaseVoteIndex = voteId - 1;
					blueChasePlayer = ent;
				}
			}
		}
	}

	qboolean redReady = qfalse, blueReady = qfalse;
	int redHighestNumVotes = 0, blueHighestNumVotes = 0;
	int redMapIndex = -1, blueMapIndex = -1;
	int redTieCount = 0, blueTieCount = 0;

	// calculate the highest votes for each team and check if any map has at least 3 votes
	for (int i = 0; i < MAX_MULTIVOTE_MAPS; i++) {
		if (banVotesRed[i] >= 3) {
			redReady = qtrue;
		}
		if (banVotesBlue[i] >= 3) {
			blueReady = qtrue;
		}

		if (banVotesRed[i] > redHighestNumVotes) {
			redHighestNumVotes = banVotesRed[i];
			redMapIndex = i;
			redTieCount = 1; // reset tie count since we have a new highest
		}
		else if (banVotesRed[i] == redHighestNumVotes) {
			redTieCount++; // increment tie count
		}

		if (banVotesBlue[i] > blueHighestNumVotes) {
			blueHighestNumVotes = banVotesBlue[i];
			blueMapIndex = i;
			blueTieCount = 1; // reset tie count since we have a new highest
		}
		else if (banVotesBlue[i] == blueHighestNumVotes) {
			blueTieCount++; // increment tie count
		}
	}

	const qboolean bothTeamsReady = (redReady && blueReady);

	// if vote time has not expired and both teams are not ready, return
	if (level.time - level.voteTime < VOTE_TIME && !bothTeamsReady) {
		return;
	}

	memset(level.bannedMapNames, 0, sizeof(level.bannedMapNames));

	// process the bans based on the time and vote conditions
	if (level.time - level.voteTime >= VOTE_TIME) {
		// time expired

		if (redHighestNumVotes > 0 && redTieCount > 1 && redChasePlayer && redChaseVoteIndex != -1 && banVotesRed[redChaseVoteIndex] == redHighestNumVotes) {
			redTieCount = 1;
			redMapIndex = redChaseVoteIndex;
			NotifyTeammatesOfVote(redChasePlayer, "'s ban vote breaks the tie", "^8");
		}
		if (blueHighestNumVotes > 0 && blueTieCount > 1 && blueChasePlayer && blueChaseVoteIndex != -1 && banVotesBlue[blueChaseVoteIndex] == blueHighestNumVotes) {
			blueTieCount = 1;
			blueMapIndex = blueChaseVoteIndex;
			NotifyTeammatesOfVote(blueChasePlayer, "'s ban vote breaks the tie", "^8");
		}

		// first check if both teams banned the same map
		if (redHighestNumVotes > 0 && blueHighestNumVotes > 0 && redMapIndex == blueMapIndex && redTieCount == 1 && blueTieCount == 1) {
			level.mapsThatCanBeVotedBits &= ~(1 << redMapIndex);
			Q_strncpyz(level.bannedMapNames[TEAM_RED], level.multiVoteMapShortNames[redMapIndex], sizeof(level.bannedMapNames[TEAM_RED]));
			Q_strncpyz(level.bannedMapNames[TEAM_BLUE], level.multiVoteMapShortNames[blueMapIndex], sizeof(level.bannedMapNames[TEAM_BLUE]));
			G_LogPrintf("Map %d (%s) was banned by both teams.\n", redMapIndex + 1, level.multiVoteMapShortNames[redMapIndex]);
			PrintIngame(-1, "%s was banned by both teams.\n", level.multiVoteMapShortNames[redMapIndex]);
		}
		else {
			// handle red team
			if (redHighestNumVotes > 0 && redTieCount == 1 && redMapIndex != -1) {
				level.mapsThatCanBeVotedBits &= ~(1 << redMapIndex);
				Q_strncpyz(level.bannedMapNames[TEAM_RED], level.multiVoteMapShortNames[redMapIndex], sizeof(level.bannedMapNames[TEAM_RED]));
				G_LogPrintf("Map %d (%s) was banned by red team.\n", redMapIndex + 1, level.multiVoteMapShortNames[redMapIndex]);
				PrintIngame(-1, "%s was banned by red team.\n", level.multiVoteMapShortNames[redMapIndex]);
			}

			// handle blue team
			if (blueHighestNumVotes > 0 && blueTieCount == 1 && blueMapIndex != -1) {
				level.mapsThatCanBeVotedBits &= ~(1 << blueMapIndex);
				Q_strncpyz(level.bannedMapNames[TEAM_BLUE], level.multiVoteMapShortNames[blueMapIndex], sizeof(level.bannedMapNames[TEAM_BLUE]));
				G_LogPrintf("Map %d (%s) was banned by blue team.\n", blueMapIndex + 1, level.multiVoteMapShortNames[blueMapIndex]);
				PrintIngame(-1, "%s was banned by blue team.\n", level.multiVoteMapShortNames[blueMapIndex]);
			}
		}

		// build the remaining maps list
		for (int i = 0; i < MAX_MULTIVOTE_MAPS; i++) {
			if (!(level.mapsThatCanBeVotedBits & (1 << i))) {
				continue;
			}
			// add maps that were not banned to the remainingMaps string
			char buf[2] = { 0 };
			buf[0] = level.multiVoteMapChars[i];
			Q_strcat(remainingMaps, sizeof(remainingMaps), buf);
		}
	}
	else {
		// time not expired, just pick the ones that have majorities
		for (int i = 0; i < MAX_MULTIVOTE_MAPS; i++) {
			if (banVotesRed[i] >= 3 || banVotesBlue[i] >= 3) {
				level.mapsThatCanBeVotedBits &= ~(1 << i);
				char *teamStr;
				if (banVotesRed[i] >= 3 && banVotesBlue[i] >= 3) {
					teamStr = "both teams";
					Q_strncpyz(level.bannedMapNames[TEAM_RED], level.multiVoteMapShortNames[i], sizeof(level.bannedMapNames[TEAM_RED]));
					Q_strncpyz(level.bannedMapNames[TEAM_BLUE], level.multiVoteMapShortNames[i], sizeof(level.bannedMapNames[TEAM_BLUE]));
				}
				else if (banVotesRed[i] >= 3) {
					teamStr = "red team";
					Q_strncpyz(level.bannedMapNames[TEAM_RED], level.multiVoteMapShortNames[i], sizeof(level.bannedMapNames[TEAM_RED]));
				}
				else {
					teamStr = "blue team";
					Q_strncpyz(level.bannedMapNames[TEAM_BLUE], level.multiVoteMapShortNames[i], sizeof(level.bannedMapNames[TEAM_BLUE]));
				}
				G_LogPrintf("Map %d (%s) was banned by %s.\n", i + 1, level.multiVoteMapShortNames[i], teamStr);
				PrintIngame(-1, "%s was banned by %s.\n", level.multiVoteMapShortNames[i], teamStr);
			}
			else {
				// add maps that were not banned to the remainingMaps string
				char buf[2] = { 0 };
				buf[0] = level.multiVoteMapChars[i];
				Q_strcat(remainingMaps, sizeof(remainingMaps), buf);
			}
		}
	}

	memset(&(level.multiVoteBanVotes), 0, sizeof(level.multiVoteBanVotes));
	level.voteBanPhase = qfalse;
	level.voteBanPhaseCompleted = qtrue;
	level.multiVoting = qfalse;

	Svcmd_MapVote_f(remainingMaps);
}

extern void SiegeClearSwitchData(void);
extern int* BuildVoteResults( int numChoices, int *numVotes, int *highestVoteCount, qboolean *dontEndDueToMajority, int *numRerollVotes, qboolean canChangeAfkVotes);

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
	if ( !level.multiVoting ) {
		// normal behavior for basejka voting
		if ( level.time - level.voteTime >= VOTE_TIME ) {
			if ( ( g_minimumVotesCount.integer ) && ( level.numVotingClients % 2 == 0 ) && ( level.voteYes > level.voteNo ) && ( level.voteYes + level.voteNo >= g_minimumVotesCount.integer ) ) {
				trap_SendServerCommand( -1, va( "print \"%s\n\"",
					G_GetStringEdString( "MP_SVGAME", "VOTEPASSED" ) ) );

				// log the vote
				G_LogPrintf( "Vote passed. (Yes:%i No:%i All:%i g_minimumVotesCount:%i)\n", level.voteYes, level.voteNo, level.numVotingClients, g_minimumVotesCount.integer );

				// set the delay
				if (!Q_stricmpn(level.voteString, "pause", 5))
					level.voteExecuteTime = level.time; // allow pause votes to take affect immediately
				else
					level.voteExecuteTime = level.time + 3000;
			}
			else if (level.voteAutoPassOnExpiration) {
				trap_SendServerCommand(-1, "print \"Vote automatically passed.\n\"");

				// log the vote
				G_LogPrintf("Vote automatically passed. (Yes:%i No:%i All:%i g_minimumVotesCount:%i)\n", level.voteYes, level.voteNo, level.numVotingClients, g_minimumVotesCount.integer);

				// set the delay
				level.voteExecuteTime = level.time + 3000;
			}
			else {
				trap_SendServerCommand( -1, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "VOTEFAILED" ) ) );

				// log the vote
				G_LogPrintf( "Vote timed out. (Yes:%i No:%i All:%i)\n", level.voteYes, level.voteNo, level.numVotingClients );
			}
		}
		else {
			if ( ( g_enforceEvenVotersCount.integer ) && ( level.numVotingClients % 2 == 1 ) ) {
				if ( ( g_minVotersForEvenVotersCount.integer > 4 ) && ( level.numVotingClients >= g_minVotersForEvenVotersCount.integer ) ) {
					if ( level.voteYes < level.numVotingClients / 2 + 2 ) {
						return;
					}
				}
			}

			if ( level.voteYes > level.numVotingClients / 2 ) {
				trap_SendServerCommand( -1, va( "print \"%s\n\"",
					G_GetStringEdString( "MP_SVGAME", "VOTEPASSED" ) ) );

				// log the vote
				G_LogPrintf( "Vote passed. (Yes:%i No:%i All:%i)\n", level.voteYes, level.voteNo, level.numVotingClients );

				// set the delay
				if (!Q_stricmpn(level.voteString, "pause", 5))
					level.voteExecuteTime = level.time; // allow pause votes to take affect immediately
				else
					level.voteExecuteTime = level.time + 3000;
			} else if ( level.voteNo >= ( level.numVotingClients + 1 ) / 2 ) {
				// same behavior as a timeout
				trap_SendServerCommand( -1, va( "print \"%s\n\"",
					G_GetStringEdString( "MP_SVGAME", "VOTEFAILED" ) ) );

				// log the vote
				G_LogPrintf( "Vote failed. (Yes:%i No:%i All:%i)\n", level.voteYes, level.voteNo, level.numVotingClients );
			} else {
				// still waiting for a majority
				return;
			}
		}

		g_entities[level.lastVotingClient].client->lastCallvoteTime = level.time;
	} else {
		// special handler for multiple choices voting
		if (level.voteBanPhase) {
			DoMapBans();
			return;
		}

		int numVotes = 0, highestVoteCount = 0, numRerollVotes = 0;
		qboolean dontEndDueToMajority = qfalse;
		int *voteResults = BuildVoteResults( level.multiVoteChoices, &numVotes, &highestVoteCount, &dontEndDueToMajority, &numRerollVotes, qfalse);
		free( voteResults );

		if (numRerollVotes >= ((level.numVotingClients / 2) + 1)) {
			DoRunoff();
			return;
		}
		else {

			// the vote ends when a map has >50% majority, when everyone voted, or when the vote timed out
			if (level.time - level.voteTime >= VOTE_TIME - 500 && level.numVotingClients >= 7 &&
				g_vote_mapVoteExtension_ifUnderThisManyVotes.integer > 0 && numVotes < g_vote_mapVoteExtension_ifUnderThisManyVotes.integer &&
				g_vote_mapVoteExtensions_maxExtensions.integer > 0 && level.multiVoteTimeExtensions < g_vote_mapVoteExtensions_maxExtensions.integer) {
				G_LogPrintf("Extending multi vote time due to lack of participation (%d total voters, including %d reroll voters)\n", numVotes, numRerollVotes);
				++level.multiVoteTimeExtensions;
				level.voteTime = level.time;
				int timeChange = Com_Clampi(15, 120, g_vote_mapVoteExtension_extensionDuration.integer);
				timeChange *= 1000;
				timeChange -= VOTE_TIME;
				level.voteTime += timeChange;
				trap_SetConfigstring(CS_VOTE_TIME, va("%i", level.voteTime));
				if (!numVotes)
					PrintIngame(-1, "Extending vote time because nobody has voted.\n");
				else if (numVotes == 1)
					PrintIngame(-1, "Extending vote time because only 1 player has voted.\n");
				else
					PrintIngame(-1, "Extending vote time because only %d players have voted.\n", numVotes);
				return;
			}
			else {
				const qboolean canChangeAfkVotes = (level.time - level.voteTime >= VOTE_TIME);
				if (canChangeAfkVotes) {
					// build vote results again if we're out of time, possibly changing afk votes this tiem
					voteResults = BuildVoteResults(level.multiVoteChoices, &numVotes, &highestVoteCount, &dontEndDueToMajority, &numRerollVotes, qtrue);
					free(voteResults);
				}

				if (level.time - level.voteTime >= VOTE_TIME && !DoRunoff()) {
					G_LogPrintf("Multi vote ended due to time (%d voters, %d reroll voters)\n", numVotes, numRerollVotes);
					level.voteExecuteTime = level.time; // in this special case, execute it now. the delay is done in the svcmd
				}
				else if (!dontEndDueToMajority && highestVoteCount >= ((level.numVotingClients / 2) + 1)) {
					G_LogPrintf("Multi vote ended due to majority vote (%d voters, %d reroll voters)\n", numVotes, numRerollVotes);
					level.voteExecuteTime = level.time; // in this special case, execute it now. the delay is done in the svcmd
				}
				else if (numVotes >= level.numVotingClients && !DoRunoff()) {
					G_LogPrintf("Multi vote ended due to everyone voted, no majority, and no runoff (%d voters, %d reroll voters)\n", numVotes, numRerollVotes);
					level.voteExecuteTime = level.time; // in this special case, execute it now. the delay is done in the svcmd
				}
				else {
					return;
				}
			}
		}
	}

	level.voteStartTime = 0;
	level.voteTime = 0;
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
    if (/*!g_doWarmup.integer || */restarting || level.intermissiontime)
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
            if (i < 15)
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
			trap_Cvar_Set( "g_needpass", !sv_passwordlessSpectators.integer ? "1" : "0" );
		} else {
			trap_Cvar_Set( "g_needpass", "0" );
			// Disabled since no password is set
			// TODO: auto update of g_needpass
			trap_Cvar_Set( "sv_passwordlessSpectators", "0" );
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
extern void pas_think(gentity_t *ent);
extern void thermalThinkStandard(gentity_t *ent);
extern void DownedSaberThink(gentity_t *saberent);

void G_RunThink (gentity_t *ent) {
	int	thinktime;

    //OSP: pause
    //      If paused, push nextthink
    if ( level.pause.state != PAUSE_NONE && !(ent->r.ownerNum >= 0 && ent->r.ownerNum < MAX_CLIENTS && level.clients[ent->r.ownerNum].sess.inRacemode)) { 
		if ( ent-g_entities >= g_maxclients.integer && ent->nextthink > level.time )
            ent->nextthink += level.time - level.previousTime;

		// special case, mines need update here
		if ( ent->think == proxMineThink && ent->genericValue15 > level.time)
			ent->genericValue15 += level.time - level.previousTime;

        // another special case, siege items need respawn timer update
        if ( ent->think == SiegeItemThink && ent->genericValue9 > level.time )
            ent->genericValue9 += level.time - level.previousTime;

		// more special cases, sentry
		if (ent->think == pas_think)
			ent->genericValue8 += level.time - level.previousTime;

		// another special case, thermal primaries
		if (ent->think == thermalThinkStandard)
			ent->genericValue5 += level.time - level.previousTime;

		// saberthrow
		if (VALIDSTRING(ent->classname) && !strcmp(ent->classname, "lightsaber") && ent->s.pos.trTime && ent->think != DownedSaberThink)
			ent->s.pos.trTime += level.time - level.previousTime;
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

#ifdef NEWMOD_SUPPORT
// use some bullshit unused fields to put homing data in the client's playerstate
static void SetHomingInPlayerstate(gclient_t *cl, int stage) {
	assert(cl);
	assert(stage >= 0 && stage <= 10);
	for (int i = 0; i < 4; i++) {
		if (stage & (1 << i))
			cl->ps.holocronBits |= (1 << (NUM_FORCE_POWERS + i));
		else
			cl->ps.holocronBits &= ~(1 << (NUM_FORCE_POWERS + i));
	}
}

static void RunImprovedHoming(void) {
	if (!g_improvedHoming.integer)
		return;

	float lockTimeInterval = ((g_gametype.integer == GT_SIEGE) ? 2400.0f : 1200.0f) / 16.0f;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		gclient_t *cl = &level.clients[i];
		if (cl->pers.connected != CON_CONNECTED) {
			cl->homingLockTime = 0;
			SetHomingInPlayerstate(cl, 0);
			continue;
		}

		// check if they are following someone
		gentity_t *lockEnt = ent;
		gclient_t *lockCl = cl;
		if (cl->sess.sessionTeam == TEAM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW &&
			cl->sess.spectatorClient >= 0 && cl->sess.spectatorClient < MAX_CLIENTS) {
			gclient_t *followed = &level.clients[cl->sess.spectatorClient];
			if (followed->pers.connected != CON_CONNECTED) {
				lockCl->homingLockTime = 0;
				SetHomingInPlayerstate(cl, 0);
				continue;
			}
			lockCl = followed;
			lockEnt = &g_entities[lockCl - level.clients];
		}

		if (lockCl->sess.sessionTeam == TEAM_SPECTATOR || lockEnt->health <= 0 || lockCl->ps.pm_type == PM_SPECTATOR || lockCl->ps.pm_type == PM_INTERMISSION) {
			lockCl->homingLockTime = 0;
			SetHomingInPlayerstate(cl, 0);
			continue;
		}

		// check that the rocketeer has a lock
		if (lockCl->ps.rocketLockIndex == ENTITYNUM_NONE) {
			SetHomingInPlayerstate(cl, 0);
			continue;
		}

		// get the lock stage
		float rTime = lockCl->ps.rocketLockTime == -1 ? lockCl->ps.rocketLastValidTime : lockCl->ps.rocketLockTime;
		int dif = Com_Clampi(0, 10, (level.time - rTime) / lockTimeInterval);
		if (lockCl->ps.m_iVehicleNum) {
			gentity_t *veh = &g_entities[lockCl->ps.m_iVehicleNum];
			if (veh->m_pVehicle) {
				vehWeaponInfo_t *vehWeapon = NULL;
				if (lockCl->ps.weaponstate == WEAPON_CHARGING_ALT) {
					if (veh->m_pVehicle->m_pVehicleInfo->weapon[1].ID > VEH_WEAPON_BASE &&veh->m_pVehicle->m_pVehicleInfo->weapon[1].ID < MAX_VEH_WEAPONS)
						vehWeapon = &g_vehWeaponInfo[veh->m_pVehicle->m_pVehicleInfo->weapon[1].ID];
				}
				else if (veh->m_pVehicle->m_pVehicleInfo->weapon[0].ID > VEH_WEAPON_BASE &&veh->m_pVehicle->m_pVehicleInfo->weapon[0].ID < MAX_VEH_WEAPONS) {
					vehWeapon = &g_vehWeaponInfo[veh->m_pVehicle->m_pVehicleInfo->weapon[0].ID];
				}
				if (vehWeapon) {//we are trying to lock on with a valid vehicle weapon, so use *its* locktime, not the hard-coded one
					if (!vehWeapon->iLockOnTime) { //instant lock-on
						dif = 10;
					}
					else {//use the custom vehicle lockOnTime
						lockTimeInterval = (vehWeapon->iLockOnTime / 16.0f);
						dif = Com_Clampi(0, 10, (level.time - rTime) / lockTimeInterval);
					}
				}
			}
		}

		// set the homing stage in their playerstate and start the special serverside timer
		SetHomingInPlayerstate(cl, dif);
		if (dif == 10) {
			lockCl->homingLockTime = level.time;
			lockCl->homingLockTarget = lockCl->ps.rocketLockIndex;
		}
	}
}
#endif

static int GetCurrentRestartCountdown(void) {
	char buf[32] = { 0 };
	trap_GetConfigstring(CS_WARMUP, buf, sizeof(buf));
	if (buf[0]) {
		int num = atoi(buf);
		if (num > 0)
			return num;
	}
	return 0;
}

static void WaitForAFKs(void) {
	if (g_gametype.integer != GT_CTF)
		return;

	const int currentCountdown = GetCurrentRestartCountdown();
	static int autoCountdown = 0;

	if (level.intermissiontime && currentCountdown && autoCountdown) {
		// intermission was reached during the auto countdown; cancel it
		trap_SendConsoleCommand(EXEC_APPEND, "map_restart -1\n");
		autoCountdown = 0;
		level.autoStartPending = qfalse;
		return;
	}

	if (!g_waitForAFK.integer || !level.autoStartPending) {
		if (currentCountdown && autoCountdown) {
			// edge case: the g_waitForAFK cvar was disabled or autostart was entered with <8 people while the countdown was active
			// cancel the auto countdown
			trap_SendConsoleCommand(EXEC_APPEND, "map_restart -1\n");
			autoCountdown = 0;
			level.autoStartPending = qfalse;
		}
		return;
	}

	if (autoCountdown && !currentCountdown) {
		// edge case: an admin used rcon map_restart -1 to cancel the auto countdown
		level.autoStartPending = qfalse;
		autoCountdown = 0;
		return;
	}

	if ((!autoCountdown && currentCountdown) || (currentCountdown && autoCountdown && abs(currentCountdown - autoCountdown) > 200)) {
		// edge case: a countdown was started elsewhere (rcon) while auto start was pending or an auto countdown was active
		// e.g. ski used rcon map_restart 300
		// disable special auto countdown checks (afk detection, etc) so that this function treats it as a manual countdown
		level.autoStartPending = qfalse;
		autoCountdown = 0;
		return;
	}

	int numRed = 0, numBlue = 0, numAfks = 0;
	int afkGuy1 = -1, afkGuy2 = -1;
	int now = getGlobalTime();

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED)
			continue;
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->sess.sessionTeam == TEAM_FREE)
			continue;

		if (ent->client->sess.sessionTeam == TEAM_RED)
			numRed++;
		else
			numBlue++;

		if (ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_AFKTROLL)) {
			// LS has to actually move every 3 seconds
			int afkThreshold = g_waitForAFKThresholdTroll.integer;
			if (afkThreshold <= 0)
				afkThreshold = WAITFORAFK_AFK_DEFAULT_TROLL;
			afkThreshold = Com_Clampi(WAITFORAFK_AFK_MIN, WAITFORAFK_AFK_MAX, afkThreshold);

			if (now - ent->client->pers.lastMoveTime > afkThreshold) {
				if (!numAfks)
					afkGuy1 = i;
				else if (numAfks == 1)
					afkGuy2 = i;
				numAfks++;
			}
		}
		else {
			// players with brains merely have to do any input every 5 seconds
			int afkThreshold = g_waitForAFKThreshold.integer;
			if (afkThreshold <= 0)
				afkThreshold = WAITFORAFK_AFK_DEFAULT;
			afkThreshold = Com_Clampi(WAITFORAFK_AFK_MIN, WAITFORAFK_AFK_MAX, afkThreshold);

			if (now - ent->client->pers.lastInputTime > afkThreshold) {
				if (!numAfks)
					afkGuy1 = i;
				else if (numAfks == 1)
					afkGuy2 = i;
				numAfks++;
			}
		}
	}

	int minPlayers = g_waitForAFKMinPlayers.integer;
	if (minPlayers <= 0)
		minPlayers = WAITFORAFK_MINPLAYERS_DEFAULT;
	minPlayers = Com_Clampi(WAITFORAFK_MINPLAYERS_MIN, WAITFORAFK_MINPLAYERS_MAX, minPlayers);

	/*
	if ANY ONE of the following is true:
	- someone has become afk
	- the teams have become uneven
	- there are now less than 8 players ingame

	we do not start
	*/

	char failReason[256] = { 0 };
	if (numRed + numBlue < minPlayers)
		Q_strncpyz(failReason, "Not enough players ingame", sizeof(failReason));
	else if (numRed != numBlue)
		Q_strncpyz(failReason, "Uneven # of players", sizeof(failReason));
	else if (numAfks > 2)
		Q_strncpyz(failReason, va("Waiting for %d players to unAFK", numAfks), sizeof(failReason));
	else if (numAfks == 2)
		Q_strncpyz(failReason, va("Waiting for %s%s^7\nand %s%s^7 to unAFK", NM_SerializeUIntToColor(afkGuy1), level.clients[afkGuy1].pers.netname, NM_SerializeUIntToColor(afkGuy2), level.clients[afkGuy2].pers.netname), sizeof(failReason));
	else if (numAfks == 1)
		Q_strncpyz(failReason, va("Waiting for %s%s^7 to %s", NM_SerializeUIntToColor(afkGuy1), level.clients[afkGuy1].pers.netname, Q_stristrclean(level.clients[afkGuy1].pers.netname, "hannah") ? "hunAFK" : "unAFK"), sizeof(failReason));

	if (failReason[0] && level.time - level.startTime < 30000 && level.pause.state == PAUSE_NONE)
		Q_strncpyz(failReason, " ", sizeof(failReason));

	static int lastCenterPrintTime = 0;
	if (currentCountdown) {
		if (failReason[0]) {
			trap_SendConsoleCommand(EXEC_NOW, "map_restart -1\n");
			autoCountdown = 0;
			if (trap_Milliseconds() - lastCenterPrintTime >= 500) {
				G_GlobalTickedCenterPrint(va("Restart pending...\n\n%s", failReason), 1000, qfalse);
				lastCenterPrintTime = trap_Milliseconds();
			}
		}
		else {
			// we're good for the moment, keep counting down
		}
	}
	else if (level.autoStartPending) {
		if (failReason[0]) {
			if (trap_Milliseconds() - lastCenterPrintTime >= 500) {
				G_GlobalTickedCenterPrint(va("Restart pending...\n\n%s", failReason), 1000, qfalse);
				lastCenterPrintTime = trap_Milliseconds();
			}
		}
		else {
			int seconds = g_waitForAFKTimer.integer;
			if (seconds <= 0)
				seconds = WAITFORAFK_COUNTDOWN_DEFAULT;
			seconds = Com_Clampi(WAITFORAFK_COUNTDOWN_MIN, WAITFORAFK_COUNTDOWN_MAX, seconds);
			trap_SendConsoleCommand(EXEC_NOW, va("map_restart %d\n", seconds));
			autoCountdown = level.time + (seconds * 1000);
			if (trap_Milliseconds() - lastCenterPrintTime >= 500) {
				G_GlobalTickedCenterPrint("", 1000, qfalse);
				lastCenterPrintTime = trap_Milliseconds();
			}
		}
	}	
}

#define RECALCULATE_TEAM_BALANCE_INTERVAL (5000)

// recalculate team balance every few seconds. if someone tries to sub, we will be able to compare the hypothetical new balance to the previous balance.
static void PeriodicallyRecalculateTeamBalance(void) {
	if (!g_vote_teamgen_subhelp.integer || g_gametype.integer != GT_CTF || !level.wasRestarted || level.someoneWasAFK || (level.time - level.startTime) < (CTFPOSITION_MINIMUM_SECONDS * 1000) || !level.numTeamTicks || level.pause.state != PAUSE_NONE)
		return;

	static int lastTime = 0;
	int now = trap_Milliseconds();
	if (now - lastTime < RECALCULATE_TEAM_BALANCE_INTERVAL)
		return;

	lastTime = now;
	RecalculateTeamBalance();
}

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

#ifdef NEWMOD_SUPPORT

// write a force loadout in four characters
char *EncodeForceLoadout(gclient_t *cl) {
	assert(cl);

	int isDark = (cl->ps.fd.forceSide == FORCE_DARKSIDE);
	int forcePowerLevels[NUM_FORCE_POWERS];
	memcpy(forcePowerLevels, cl->ps.fd.forcePowerLevel, sizeof(forcePowerLevels));

	static char encodedString[5];

	encodedString[0] =
		0x80 |
		(isDark << 6) |
		(forcePowerLevels[FP_LEVITATION] << 4) |
		(forcePowerLevels[FP_SPEED] << 2) |
		(forcePowerLevels[FP_PUSH]);

	encodedString[1] =
		0x80 |
		(forcePowerLevels[FP_PULL] << 5) |
		(forcePowerLevels[FP_SEE] << 3) |
		(forcePowerLevels[FP_SABER_DEFENSE] << 1) |
		((forcePowerLevels[FP_SABER_OFFENSE] & 0x02) >> 1);

	encodedString[2] =
		0x80 |
		((forcePowerLevels[FP_SABER_OFFENSE] & 0x01) << 6) |
		(forcePowerLevels[FP_SABERTHROW] << 4) |
		(((!isDark ? forcePowerLevels[FP_ABSORB] : forcePowerLevels[FP_GRIP]) & 0x03) << 2) |
		((!isDark ? forcePowerLevels[FP_PROTECT] : forcePowerLevels[FP_LIGHTNING]) & 0x03);

	encodedString[3] =
		0x80 |
		((!isDark ? forcePowerLevels[FP_HEAL] : forcePowerLevels[FP_DRAIN]) << 5) |
		((!isDark ? forcePowerLevels[FP_TELEPATHY] : forcePowerLevels[FP_RAGE]) << 3) |
		(((!isDark ? forcePowerLevels[FP_TEAM_HEAL] : forcePowerLevels[FP_TEAM_FORCE]) & 0x03) << 1);

	encodedString[4] = '\0';

	return encodedString;
}


#define MAX_SPECINFO_PLAYERS_PER_TEAM	8
#define MAX_SPECINFO_PLAYERS			(MAX_SPECINFO_PLAYERS_PER_TEAM * 2)
void CheckSpecInfo(void) {
	if (!g_specInfo.integer)
		return;

	static int lastUpdate = 0;
	int updateRate = Com_Clampi(1, 1000, g_teamOverlayUpdateRate.integer);
	if (lastUpdate && level.time - lastUpdate <= updateRate)
		return;

	// see if anyone is spec
	int i, numPlayers[3] = { 0 };
	qboolean gotRecipient = qfalse, include[MAX_CLIENTS] = { qfalse }, sendTo[MAX_CLIENTS] = { qfalse };
	for (i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		gclient_t *cl = &level.clients[i];
		if (!ent->inuse || cl->pers.connected != CON_CONNECTED /*|| ent->r.svFlags & SVF_BOT*/)
			continue;
		if (cl->sess.sessionTeam == TEAM_SPECTATOR) {
			if (!cl->isLagging) {
				sendTo[i] = qtrue;
				gotRecipient = qtrue;
			}
		}
		else {
			if (g_gametype.integer < GT_TEAM && cl->sess.sessionTeam == TEAM_FREE && numPlayers[TEAM_FREE] < MAX_SPECINFO_PLAYERS) {
				include[i] = qtrue;
				numPlayers[TEAM_FREE]++;
			}
			else if (g_gametype.integer >= GT_TEAM && cl->sess.sessionTeam != TEAM_FREE && numPlayers[cl->sess.sessionTeam] < MAX_SPECINFO_PLAYERS_PER_TEAM) {
				include[i] = qtrue;
				numPlayers[cl->sess.sessionTeam]++;
			}
		}
	}
	if (!gotRecipient)
		return;
	lastUpdate = level.time;

	// build the spec info string
	char totalString[MAX_STRING_CHARS] = { 0 };
	Q_strncpyz(totalString, "kls -1 -1 snf2", sizeof(totalString));
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!include[i])
			continue;
		gentity_t *ent = &g_entities[i];
		gclient_t *cl = &level.clients[i];

		char playerString[MAX_STRING_CHARS] = { 0 };
		Q_strncpyz(playerString, va(" \"%d", i), sizeof(playerString));
		if (ent->health > 0 && !(g_gametype.integer == GT_SIEGE && ent->client->tempSpectate && ent->client->tempSpectate >= level.time))
			Q_strcat(playerString, sizeof(playerString), va(" h=%d", ent->health));
		if (cl->ps.stats[STAT_ARMOR] > 0 && ent->health > 0  && !(g_gametype.integer == GT_SIEGE && ent->client->tempSpectate && ent->client->tempSpectate >= level.time))
			Q_strcat(playerString, sizeof(playerString), va(" a=%d", cl->ps.stats[STAT_ARMOR]));
		Q_strcat(playerString, sizeof(playerString), va(" f=%d", !cl->ps.fd.forcePowersKnown ? -1 : cl->ps.fd.forcePower));
		Q_strcat(playerString, sizeof(playerString), va(" l=%d", g_gametype.integer < GT_TEAM ? Team_GetLocation(ent, NULL, 0, qtrue) : cl->pers.teamState.location));
		if (g_gametype.integer == GT_SIEGE && cl->siegeClass != -1 && bgSiegeClasses[cl->siegeClass].maxhealth != 100)
			Q_strcat(playerString, sizeof(playerString), va(" mh=%d", bgSiegeClasses[cl->siegeClass].maxhealth));
		if (g_gametype.integer == GT_SIEGE && cl->siegeClass != -1 && bgSiegeClasses[cl->siegeClass].maxarmor != 100)
			Q_strcat(playerString, sizeof(playerString), va(" ma=%d", bgSiegeClasses[cl->siegeClass].maxarmor));
		if (cl->ps.fd.forcePowersKnown && g_broadcastForceLoadouts.integer && g_gametype.integer != GT_SIEGE)
			Q_strcat(playerString, sizeof(playerString), va(" fl=%s", EncodeForceLoadout(cl)));
		int p = ent->s.powerups;
		if (g_teamOverlayForceAlignment.integer && g_gametype.integer != GT_SIEGE) {
			if (ent->health > 0 && ent->client->ps.fd.forcePowersKnown) {
				if (ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE) {
					p |= (1 << PW_FORCE_ENLIGHTENED_LIGHT);
					p &= ~(1 << PW_FORCE_ENLIGHTENED_DARK);
				}
				else if (ent->client->ps.fd.forceSide == FORCE_DARKSIDE) {
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
		if (p)
			Q_strcat(playerString, sizeof(playerString), va(" p=%d", p));
		Q_strcat(playerString, sizeof(playerString), "\"");

		Q_strcat(totalString, sizeof(totalString), playerString);
	}

	int len = strlen(totalString);
	if (len >= 1000) {
		G_LogPrintf("Warning: specinfo string is very long! (%d digits)\n", len);
	}

	// send it to specs
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!sendTo[i])
			continue;
		trap_SendServerCommand(i, totalString);
	}
}
#endif

void G_ApplyRaceBroadcastsToEvent( gentity_t *parent, gentity_t *ev ) {
	if ( !ev || ev->s.eType <= ET_EVENTS ) {
		return;
	}

#ifdef _DEBUG
	if ( !d_disableRaceVisChecks.integer )
	{
#endif

	qboolean playInRaceDimension = qfalse; // by default, play the event in normal dimension
	int raceClientNum = -1; // client num associated with the event when playing in race dimension

	if ( ev->raceDimensionEvent )
	{ // explicitly a race dimension event
		playInRaceDimension = qtrue;
	}
	else if ( !parent || !parent->inuse )
	{ // a world event, hide to racers by default
		playInRaceDimension = qfalse;
	}
	else if ( parent->client && parent->client->sess.inRacemode )
	{ // originates directly from a racer
		playInRaceDimension = qtrue;
		raceClientNum = parent - g_entities; // use client num directly
	}
	else if ( parent->r.ownerNum >= 0 && parent->r.ownerNum < MAX_CLIENTS && level.clients[parent->r.ownerNum].sess.inRacemode )
	{ // originates from an entity owned by a racer
		playInRaceDimension = qtrue;
		raceClientNum = parent->r.ownerNum; // use the owner num
	}
	else if ( parent->parent && parent->parent->client && parent->parent->client->sess.inRacemode )
	{ // has itself a parent who is a racer
		playInRaceDimension = qtrue;
		raceClientNum = parent->parent - g_entities; // use the client num of its parent
	}

	if ( playInRaceDimension ) {
		ev->r.broadcastClients[1] |= ~( level.racemodeClientMask | level.racemodeSpectatorMask ); // hide to in game players...
		ev->r.broadcastClients[1] |= level.racemodeClientsHidingOtherRacersMask; // ...hide to racers who disabled seeing other racers as well...
		ev->r.broadcastClients[1] &= ~level.ingameClientsSeeingInRaceMask; // ...and show to ig players who enabled seeing racemode stuff
		if ( raceClientNum >= 0 && raceClientNum < MAX_CLIENTS )
			ev->r.broadcastClients[1] &= ~( 1 << raceClientNum ); // ...but show to the client num associated with this event if there is one...
	} else {
		ev->r.broadcastClients[1] |= level.racemodeClientMask; // hide to racers...
		ev->r.broadcastClients[1] &= level.racemodeClientsHidingIngameMask; // ...but show to racers who didn't disable seeing in game stuff...
		ev->r.broadcastClients[1] |= level.racemodeSpectatorMask; // ...and hide to racespectators
	}

#ifdef _DEBUG
	}
#endif
}

void G_UpdateNonClientBroadcasts( gentity_t *self ) {
	self->r.broadcastClients[0] = 0;
	self->r.broadcastClients[1] = 0;

#ifdef _DEBUG
	if ( !d_disableRaceVisChecks.integer )
	{
#endif
	// racemode visibility

	// never hide triggers that can be interacted with by racers for prediction
	if ( self->s.eType == ET_PUSH_TRIGGER || self->s.eType == ET_TELEPORT_TRIGGER ) {
		return;
	}

	// non dropped flags are never hidden to anyone
	// TODO: show stolen flags as at home to racers...?
	if ( self->s.eType == ET_ITEM && self->item->giType == IT_TEAM && !( self->flags & FL_DROPPED_ITEM ) ) {
		return;
	}

#ifdef GLASS_RACER_COLLISION
	// glass is never hidden
	if ( self->s.eType == ET_MOVER && self->r.svFlags == SVF_GLASS_BRUSH ) {
		return;
	}
#endif

	if (self->isAimPracticePack) {
		self->r.broadcastClients[1] |= ~(level.racemodeClientMask | level.racemodeSpectatorMask); // ...hide to non racers
		self->r.broadcastClients[1] &= (level.racemodeClientMask | level.racemodeSpectatorMask); // ...show to racers and racespectators
		self->r.broadcastClients[1] &= ~level.ingameClientsSeeingInRaceMask; // ...and show to ig players who enabled seeing racemode stuff
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!thisEnt->inuse || !thisEnt->client)
				continue;

			if (thisEnt->client->sess.inRacemode) {
				if ((level.racemodeClientsHidingOtherRacersMask & (1 << i)) && thisEnt->aimPracticeEntBeingUsed && thisEnt->aimPracticeEntBeingUsed != self)
					self->r.broadcastClients[1] |= (1 << i); // ...hide to racers that are hiding other racers and not using this one
				else if ((thisEnt->client->sess.racemodeFlags & RMF_HIDEBOTS) && thisEnt->aimPracticeEntBeingUsed != self)
					self->r.broadcastClients[1] |= (1 << i); // ...hide to racers that are hiding bots and not using this one
			}
			else {
				if (!thisEnt->client->sess.seeAimBotsWhileIngame)
					self->r.broadcastClients[1] |= (1 << i); // ...hide to ingame players that are not explicitly showing bots while ingame)
			}
		}
		return;
	}

	if ( self->s.eType == ET_MISSILE && self->r.ownerNum >= 0 && self->r.ownerNum < MAX_CLIENTS ) {

		// special case: this is a missile with an owner
		gclient_t *owner = &level.clients[self->r.ownerNum];

		if ( !owner->sess.inRacemode ) {
			// if its owner is not in racemode...
			self->r.broadcastClients[1] |= level.racemodeClientMask; // ...hide it to racers...
			self->r.broadcastClients[1] &= level.racemodeClientsHidingIngameMask; // ...but show it to racers who didn't disable seeing in game stuff
			self->r.broadcastClients[1] |= level.racemodeSpectatorMask; // ...and hide it to racespectators
		} else {
			// its owner is in racemode...
			self->r.broadcastClients[1] = -1;
			self->r.broadcastClients[1] &= ~level.racemodeClientMask; // ...show to racers
			self->r.broadcastClients[1] &= ~level.racemodeSpectatorMask; // ...show to racespectators
			self->r.broadcastClients[1] |= level.racemodeClientsHidingOtherRacersMask; // ...but hide to racers hiding other racers
			self->r.broadcastClients[1] &= ~level.ingameClientsSeeingInRaceMask; // ...and show to ig players who enabled seeing racemode stuff
			self->r.broadcastClients[1] &= ~(1 << (self->r.ownerNum % 32)); // ...always show it to its owner
		}

	} else {

		self->r.broadcastClients[1] |= level.racemodeClientMask; // by default, just hide to racers...

		if ( self->s.eType == ET_PLAYER || self->s.eType == ET_ITEM ) {
			// ...and if this is a player or an item...
			self->r.broadcastClients[1] &= level.racemodeClientsHidingIngameMask; // ...show it to racers who didn't disable seeing in game stuff...
		}

		self->r.broadcastClients[1] |= level.racemodeSpectatorMask; // ...and hide it to racespectators

	}
#ifdef _DEBUG
	}
#endif
}

static qboolean SessionIdMatches(genericNode_t *node, void *userData) {
	tickPlayer_t *existing = (tickPlayer_t *)node;
	session_t *thisGuy = (session_t *)userData;

	if (!existing || !thisGuy)
		return qfalse;
	
	if (thisGuy->accountId != ACCOUNT_ID_UNLINKED && thisGuy->accountId == existing->accountId)
		return qtrue; // matches a linked account

	if (existing->sessionId == thisGuy->id)
		return qtrue; // matches a session

	return qfalse;
}

// adds a tick to a player's tick count for a particular team, even if he left and rejoined
// allows tracking ragequitters and subs for the discord webhook, as well as using account names/nicknames instead of ingame names
extern qboolean isRedFlagstand(gentity_t *ent);
extern qboolean isBlueFlagstand(gentity_t *ent);

qboolean MatchesCtfPositioningData(genericNode_t *node, void *userData) {
	ctfPositioningData_t *existing = (ctfPositioningData_t *)node;
	stats_t *thisGuy = (stats_t *)userData;

	if (!existing || !thisGuy)
		return qfalse;

	if (existing->stats == thisGuy)
		return qtrue;

	return qfalse;
}

static void AddPlayerTick(team_t team, gentity_t *ent) {
	if (!ent->client)
		return;

	++ent->client->stats->ticksNotPaused;

	if (!level.wasRestarted)
		return;

	gclient_t *cl = ent->client;
	level.lastPlayerTickAddedTime = level.time;
	ent->client->stats->lastTickIngameTime = level.time;

	// if i've been alive at least a few seconds, and i've done some input within the last few seconds, log my data
	qboolean validLocationSample = !!(ent->health > 0 && level.time - ent->client->pers.lastSpawnTime >= CTFPOS_POSTSPAWN_DELAY_MS &&
		ent->client->lastInputTime && trap_Milliseconds() - ent->client->lastInputTime < 10000);

	float loc;
	if (validLocationSample) {
		loc = GetCTFLocationValue(ent);

		// note our own location
		++ent->client->stats->numLocationSamplesRegardlessOfFlagHolding;
		if (HasFlag(ent)) {
			ent->client->stats->totalLocationWithFlag += loc;
			++ent->client->stats->numLocationSamplesWithFlag;
		}
		else {
			ent->client->stats->totalLocationWithoutFlag += loc;
			++ent->client->stats->numLocationSamplesWithoutFlag;
		}
	}

	// record our location in everyone else's stats so that people's positioning can only be compared to people they were ingame contemporaneously with
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *other = &g_entities[i];
		if (other == ent || !other->inuse || !other->client || other->client->pers.connected != CON_CONNECTED || other->client->sess.sessionTeam != ent->client->sess.sessionTeam)
			continue;
		ctfPositioningData_t *data = ListFind(&other->client->stats->teammatePositioningList, MatchesCtfPositioningData, cl->stats, NULL);
		if (!data) {
			data = ListAdd(&other->client->stats->teammatePositioningList, sizeof(ctfPositioningData_t));
			data->stats = cl->stats;
		}

		++data->numTicksIngameWithMe; // increment this regardless of whether the position sample is valid

		if (validLocationSample) {
			++data->numLocationSamplesIngameWithMe;
			if (HasFlag(ent)) {
				data->totalLocationWithFlagWithMe += loc;
				++data->numLocationSamplesWithFlagWithMe;
			}
			else {
				data->totalLocationWithoutFlagWithMe += loc;
				++data->numLocationSamplesWithoutFlagWithMe;
			}
		}
	}

	if (!ent->client->session)
		return;
	list_t *list = team == TEAM_RED ? &level.redPlayerTickList : &level.bluePlayerTickList;
	tickPlayer_t *found = ListFind(list, SessionIdMatches, cl->session, NULL);

	if (found) { // this guy is already tracked
		found->numTicks++;
		found->accountId = cl->session->accountId; // update the tracked account id, in case an admin assigned him an account during this match
		const char *name = (cl->account && VALIDSTRING(cl->account->name)) ? cl->account->name : (cl->pers.netname[0] ? cl->pers.netname : "Padawan");
		Q_strncpyz(found->name, name, sizeof(found->name));
		return;
	}

	// not yet tracked; add him to the list
	tickPlayer_t *add = ListAdd(list, sizeof(tickPlayer_t));
	add->printed = qfalse;
	add->clientNum = cl - level.clients;
	add->numTicks = 1;
	add->sessionId = cl->session->id;
	add->accountId = cl->session->accountId;
	const char *name = (cl->account && VALIDSTRING(cl->account->name)) ? cl->account->name : (cl->pers.netname[0] ? cl->pers.netname : "Padawan");
	Q_strncpyz(add->name, name, sizeof(add->name));
}

animNumber_t RPSAnim(const char choiceChar) {
	switch (choiceChar) {
	case 'p': return BOTH_FORCELIGHTNING_START;
	case 's': return BOTH_DEATH_LYING_UP;
	default: return BOTH_MELEE1;
	}
}

char *RPSString(const char choiceChar) {
	switch (choiceChar) {
	case 'p': return "Paper";
	case 's': return "Scissors";
	default: return "Rock";
	}
}

char *RPSWinString(const char choiceChar) {
	switch (choiceChar) {
	case 'p': return "covers";
	case 's': return "cuts";
	default: return "crushes";
	}
}

static void RunRockPaperScissors(void) {
	if (!g_rockPaperScissors.integer || level.intermissiontime || g_gametype.integer != GT_CTF)
		return;

	qboolean handledClient[MAX_CLIENTS] = { qfalse };
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (handledClient[i])
			continue;

		gentity_t *ent = &g_entities[i];
		if (!ent->client)
			continue;

		if (!ent->inuse || ent->health < 1 || ent->client->sess.sessionTeam == TEAM_SPECTATOR) {
			ent->client->rockPaperScissorsOtherClientNum = ENTITYNUM_NONE;
			ent->client->rockPaperScissorsChallengeTime = 0;
			ent->client->rockPaperScissorsStartTime = 0;
			ent->client->rockPaperScissorsBothChosenTime = 0;
			ent->client->rockPaperScissorsChoice = '\0';
			continue;
		}

		if (!ent->client->rockPaperScissorsStartTime) {
			ent->client->rockPaperScissorsBothChosenTime = 0;
			ent->client->rockPaperScissorsChoice = '\0';
			continue;
		}

		if (ent->client->rockPaperScissorsOtherClientNum >= MAX_CLIENTS) {
			// has start time but no other client
			ent->client->rockPaperScissorsOtherClientNum = ENTITYNUM_NONE;
			ent->client->rockPaperScissorsChallengeTime = 0;
			ent->client->rockPaperScissorsStartTime = 0;
			ent->client->rockPaperScissorsBothChosenTime = 0;
			ent->client->rockPaperScissorsChoice = '\0';
			continue;
		}

		gentity_t *other = &g_entities[ent->client->rockPaperScissorsOtherClientNum];
		if (!other->inuse || !other->client || other->client->pers.connected != CON_CONNECTED || other->health < 1 || other->client->sess.sessionTeam == TEAM_SPECTATOR || other->client->sess.inRacemode != ent->client->sess.inRacemode) {
			// has start time and other client, but other client is invalid
			ent->client->rockPaperScissorsOtherClientNum = ENTITYNUM_NONE;
			ent->client->rockPaperScissorsChallengeTime = 0;
			ent->client->rockPaperScissorsStartTime = 0;
			ent->client->rockPaperScissorsBothChosenTime = 0;
			ent->client->rockPaperScissorsChoice = '\0';
			if (other->client) {
				other->client->rockPaperScissorsOtherClientNum = ENTITYNUM_NONE;
				other->client->rockPaperScissorsChallengeTime = 0;
				other->client->rockPaperScissorsStartTime = 0;
				other->client->rockPaperScissorsBothChosenTime = 0;
				other->client->rockPaperScissorsChoice = '\0';
			}
			continue;
		}

		if (handledClient[other - g_entities])
			continue;

		// mark them as handled so that the other person doesn't run later in the loop
		handledClient[ent - g_entities] = qtrue;
		handledClient[other - g_entities] = qtrue;

		if (ent->client->rockPaperScissorsChoice && other->client->rockPaperScissorsChoice) {
			static qboolean initialized = qfalse;
			static int effectId = 0;
			if (!initialized) {
				effectId = G_EffectIndex("ships/ship_explosion2");
				initialized = qtrue;
			}

			gentity_t *winner, *loser;
			if ((other->client->rockPaperScissorsChoice == 'r' && ent->client->rockPaperScissorsChoice == 'p') ||
				(other->client->rockPaperScissorsChoice == 'p' && ent->client->rockPaperScissorsChoice == 's') ||
				(other->client->rockPaperScissorsChoice == 's' && ent->client->rockPaperScissorsChoice == 'r')) {
				winner = ent;
				loser = other;
			}
			else {
				winner = other;
				loser = ent;
			}

			char *winnerString = va("Your %s %s %s^7's %s\n\n^2You win!",
				RPSString(winner->client->rockPaperScissorsChoice),
				RPSWinString(winner->client->rockPaperScissorsChoice),
				loser->client->pers.netname,
				RPSString(loser->client->rockPaperScissorsChoice));
			char *loserString = va("%s^7's %s %s your %s\n\n^1You lose!",
				winner->client->pers.netname,
				RPSString(winner->client->rockPaperScissorsChoice),
				RPSWinString(winner->client->rockPaperScissorsChoice),
				RPSString(loser->client->rockPaperScissorsChoice));
			char *drawString = va("Draw!\n(%s vs %s)", RPSString(ent->client->rockPaperScissorsChoice), RPSString(other->client->rockPaperScissorsChoice));

			qboolean bothChosenTimeAlreadySet = !!(ent->client->rockPaperScissorsBothChosenTime || other->client->rockPaperScissorsBothChosenTime);
			if (!bothChosenTimeAlreadySet) {
				ent->client->rockPaperScissorsBothChosenTime = other->client->rockPaperScissorsBothChosenTime = level.time;
				ent->client->ps.torsoAnim = other->client->ps.torsoAnim = BOTH_STAND1;
				ent->client->ps.torsoTimer = other->client->ps.torsoTimer = 0;
			}
			else if (level.time - ent->client->rockPaperScissorsBothChosenTime >= 7050 || level.time - other->client->rockPaperScissorsBothChosenTime >= 7050) {
				if (ent->client->rockPaperScissorsChoice != other->client->rockPaperScissorsChoice) {
					vec3_t dir = { 0, 0, 1 };
					vec3_t explodeVec;
					VectorCopy(loser->r.currentOrigin, explodeVec);
					explodeVec[2] += loser->client->ps.viewheight;
					if (effectId) {
						gentity_t *te = G_PlayEffectID(effectId, explodeVec, dir);
						G_ApplyRaceBroadcastsToEvent(loser, te);
					}
					loser->client->rockPaperScissorsStartTime = 0; // to allow the damage to hit
					G_Damage(loser, winner, winner, vec3_origin, vec3_origin, 999999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
				}

				ent->client->rockPaperScissorsStartTime = other->client->rockPaperScissorsStartTime = 0;
				ent->client->rockPaperScissorsOtherClientNum = other->client->rockPaperScissorsOtherClientNum = ENTITYNUM_NONE;
				ent->client->rockPaperScissorsChoice = other->client->rockPaperScissorsChoice = '\0';
				ent->client->rockPaperScissorsChallengeTime = other->client->rockPaperScissorsChallengeTime = 0;
				ent->client->rockPaperScissorsBothChosenTime = other->client->rockPaperScissorsBothChosenTime = 0;
			}
			else if (level.time - ent->client->rockPaperScissorsBothChosenTime >= 3050 || level.time - other->client->rockPaperScissorsBothChosenTime >= 3050) {
				ent->client->ps.torsoAnim = RPSAnim(ent->client->rockPaperScissorsChoice);
				other->client->ps.torsoAnim = RPSAnim(other->client->rockPaperScissorsChoice);
				if (!ent->client->rockPaperScissorsChallengeTime && !other->client->rockPaperScissorsChallengeTime &&
					(level.time - ent->client->rockPaperScissorsBothChosenTime >= 5050 || level.time - other->client->rockPaperScissorsBothChosenTime >= 5050)) {
					// refresh the message after a couple seconds
					ent->client->rockPaperScissorsChallengeTime = other->client->rockPaperScissorsChallengeTime = 1;
					if (ent->client->rockPaperScissorsChoice == other->client->rockPaperScissorsChoice) {
						CenterPrintToPlayerAndFollowers(ent, drawString);
						CenterPrintToPlayerAndFollowers(other, drawString);
					}
					else {
						CenterPrintToPlayerAndFollowers(winner, winnerString);
						CenterPrintToPlayerAndFollowers(loser, loserString);
					}
				}
				if (ent->client->rockPaperScissorsChallengeTime && other->client->rockPaperScissorsChallengeTime &&
					ent->client->rockPaperScissorsChallengeTime != 1 && other->client->rockPaperScissorsChallengeTime != 1) {
					ent->client->ps.torsoTimer = other->client->ps.torsoTimer = 4000;
					ent->client->rockPaperScissorsChallengeTime = other->client->rockPaperScissorsChallengeTime = 0;
					if (ent->client->rockPaperScissorsChoice == other->client->rockPaperScissorsChoice) {
						CenterPrintToPlayerAndFollowers(ent, drawString);
						CenterPrintToPlayerAndFollowers(other, drawString);
						char *print = va("%s^7 and %s^7 had a draw (%s).\n",
							ent->client->pers.netname,
							other->client->pers.netname,
							RPSString(ent->client->rockPaperScissorsChoice));
						if (ent->client->sess.inRacemode)
							G_PrintBasedOnRacemode(print, qtrue);
						else
							PrintIngame(-1, print);
					}
					else {
						CenterPrintToPlayerAndFollowers(winner, winnerString);
						CenterPrintToPlayerAndFollowers(loser, loserString);

						//G_AddEvent(winner, EV_TAUNT, 0);
						G_EntitySound(loser, CHAN_VOICE, G_SoundIndex("*falling1.wav"));

						char *print = va("%s^7 (%s) has defeated %s^7 (%s)!\n",
							winner->client->pers.netname,
							RPSString(winner->client->rockPaperScissorsChoice),
							loser->client->pers.netname,
							RPSString(loser->client->rockPaperScissorsChoice));
						if (ent->client->sess.inRacemode)
							G_PrintBasedOnRacemode(print, qtrue);
						else
							PrintIngame(-1, print);
					}
				}
			}
			else if (level.time - ent->client->rockPaperScissorsBothChosenTime >= 3000 || level.time - other->client->rockPaperScissorsBothChosenTime >= 3000) {
				ent->client->ps.torsoAnim = other->client->ps.torsoAnim = BOTH_STAND1;
				ent->client->ps.torsoTimer = other->client->ps.torsoTimer = 0;
			}
			else if (level.time - ent->client->rockPaperScissorsBothChosenTime >= 2050 || level.time - other->client->rockPaperScissorsBothChosenTime >= 2050) {
				ent->client->ps.torsoAnim = other->client->ps.torsoAnim = BOTH_THERMAL_THROW;
				ent->client->ps.torsoTimer = other->client->ps.torsoTimer = 950;
			}
			else if (level.time - ent->client->rockPaperScissorsBothChosenTime >= 2000 || level.time - other->client->rockPaperScissorsBothChosenTime >= 2000) {
				ent->client->ps.torsoAnim = other->client->ps.torsoAnim = BOTH_STAND1;
				ent->client->ps.torsoTimer = other->client->ps.torsoTimer = 0;
			}
			else if (level.time - ent->client->rockPaperScissorsBothChosenTime >= 1050 || level.time - other->client->rockPaperScissorsBothChosenTime >= 1050) {
				ent->client->ps.torsoAnim = other->client->ps.torsoAnim = BOTH_THERMAL_THROW;
				ent->client->ps.torsoTimer = other->client->ps.torsoTimer = 950;
			}
			else if (level.time - ent->client->rockPaperScissorsBothChosenTime >= 1000 || level.time - other->client->rockPaperScissorsBothChosenTime >= 1000) {
				ent->client->ps.torsoAnim = other->client->ps.torsoAnim = BOTH_STAND1;
				ent->client->ps.torsoTimer = other->client->ps.torsoTimer = 0;
			}
			else if (level.time - ent->client->rockPaperScissorsBothChosenTime >= 50 || level.time - other->client->rockPaperScissorsBothChosenTime >= 50) {
				ent->client->ps.torsoAnim = other->client->ps.torsoAnim = BOTH_THERMAL_THROW;
				ent->client->ps.torsoTimer = other->client->ps.torsoTimer = 950;
			}
		}
		else if (level.time - ent->client->rockPaperScissorsStartTime >= ROCK_PAPER_SCISSORS_DURATION) { // time expired
			if (!ent->client->rockPaperScissorsChoice && !other->client->rockPaperScissorsChoice) {
				CenterPrintToPlayerAndFollowers(ent, "Neither player made a choice; cancelling.");
				CenterPrintToPlayerAndFollowers(other, "Neither player made a choice; cancelling.");
			}
			else if (ent->client->rockPaperScissorsChoice) {
				CenterPrintToPlayerAndFollowers(ent, va("%s^7 didn't make a choice; cancelling.", other->client->pers.netname));
				CenterPrintToPlayerAndFollowers(other, "You didn't make a choice; cancelling.");
			}
			else if (other->client->rockPaperScissorsChoice) {
				CenterPrintToPlayerAndFollowers(ent, "You didn't make a choice; cancelling.");
				CenterPrintToPlayerAndFollowers(other, va("%s^7 didn't make a choice; cancelling.", ent->client->pers.netname));
			}
			ent->client->rockPaperScissorsStartTime = other->client->rockPaperScissorsStartTime = 0;
			ent->client->rockPaperScissorsOtherClientNum = other->client->rockPaperScissorsOtherClientNum = ENTITYNUM_NONE;
			ent->client->rockPaperScissorsChoice = other->client->rockPaperScissorsChoice = '\0';
			ent->client->rockPaperScissorsChallengeTime = other->client->rockPaperScissorsChallengeTime = 0;
			ent->client->rockPaperScissorsBothChosenTime = other->client->rockPaperScissorsBothChosenTime = 0;
		}
	}
}

extern char *GetSuffixId(gentity_t *ent);
void G_SayTo(gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message, char *locMsg, qboolean outOfBandOk);
extern void G_TestLine(vec3_t start, vec3_t end, int color, int time);
extern int forcePowerNeeded[NUM_FORCE_POWER_LEVELS][NUM_FORCE_POWERS];
extern void WP_AddToClientBitflags(gentity_t* ent, int entNum);
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

	switch (g_gripRework.integer) {
	case 1:
		forcePowerNeeded[FORCE_LEVEL_1][FP_GRIP] = 30;
		forcePowerNeeded[FORCE_LEVEL_2][FP_GRIP] = 30;
		forcePowerNeeded[FORCE_LEVEL_3][FP_GRIP] = 32;
		break;
	case 2:
		forcePowerNeeded[FORCE_LEVEL_1][FP_GRIP] = 20;
		forcePowerNeeded[FORCE_LEVEL_2][FP_GRIP] = 25;
		forcePowerNeeded[FORCE_LEVEL_3][FP_GRIP] = 30;
		break;
	default: 
		forcePowerNeeded[FORCE_LEVEL_1][FP_GRIP] = 30;
		forcePowerNeeded[FORCE_LEVEL_2][FP_GRIP] = 30;
		forcePowerNeeded[FORCE_LEVEL_3][FP_GRIP] = 60;
		break;
	}
	weaponData[WP_ROCKET_LAUNCHER].altEnergyPerShot = g_homingUses1Ammo.integer ? 1 : 2;

#ifdef NEWMOD_SUPPORT
	for (i = 0; i < MAX_CLIENTS; i++) {
		level.clients[i].realPing = level.clients[i].ps.ping;
		if (level.clients[i].pers.connected == CON_CONNECTED && level.clients[i].sess.auth == CLANNOUNCE &&
			level.clients[i].sess.clAnnounceSendTime && trap_Milliseconds() - level.clients[i].sess.clAnnounceSendTime >= 20000) {
			trap_DropClient(i, "dropped due to authentication error");
		}
	}
#endif

	// reset g_lastTeamGenTime if server is emptyish
	if (g_lastTeamGenTime.integer && level.time - level.startTime >= 5000) {
		int numIngame = 0;
		for (i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (!ent->client || ent->client->pers.connected == CON_DISCONNECTED)
				continue;

			if (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE)
				++numIngame;
		}

		if (numIngame <= 4)
			trap_Cvar_Set("g_lastTeamGenTime", "");
	}

	if (level.time - level.startTime >= 2000 && g_gametype.integer == GT_CTF) {
		TeamGen_CheckForUnbarLS();
		TeamGen_CheckForRebarLS();
		if (level.time - level.startTime >= 3000)
			TeamGen_WarnLS();
	}

	if (level.shouldAnnounceBreak && level.time - level.startTime >= 3000)
		TeamGen_AnnounceBreak();

	// print any queued messages
	if (level.queuedServerMessagesList.size > 0) {
		iterator_t iter;
		ListIterate(&level.queuedServerMessagesList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			queuedServerMessage_t *msg = IteratorNext(&iter);
			int timeSince = g_svfps.integer * (level.framenum - msg->serverFrameNum);
			const int threshold = 50; // this seems to work
			if (timeSince < threshold)
				continue;

			if (VALIDSTRING(msg->text)) {
				if (msg->inConsole) {
					PrintIngame(msg->clientNum, msg->text);
				}
				else {
					if (msg->clientNum >= 0 && msg->clientNum < MAX_CLIENTS)
						SV_Tell(msg->clientNum, msg->text);
					else
						SV_Say(msg->text);
				}
			}

			if (msg->text)
				free(msg->text);

			ListRemove(&level.queuedServerMessagesList, msg);
			ListIterate(&level.queuedServerMessagesList, &iter, qfalse);
		}
	}

	// print any queued chats
	if (level.queuedChatMessagesList.size > 0) {
		iterator_t iter;
		ListIterate(&level.queuedChatMessagesList, &iter, qfalse);
		int now = trap_Milliseconds();
		while (IteratorHasNext(&iter)) {
			queuedChatMessage_t *msg = IteratorNext(&iter);
			if (now < msg->when)
				continue;

			if (VALIDSTRING(msg->text)) {
				gentity_t *fromEnt = &g_entities[msg->fromClientNum];
				gentity_t *toEnt = &g_entities[msg->toClientNum];
				if (fromEnt && toEnt && fromEnt->client && toEnt->client) {
					char name[64] = { 0 };
					Com_sprintf(name, sizeof(name), "\x19[%s^7%s\x19]\x19: ", fromEnt->client->pers.netname, GetSuffixId(fromEnt));
					G_SayTo(fromEnt, toEnt, SAY_TELL, COLOR_MAGENTA, name, msg->text, NULL, qfalse);
				}
			}

			if (msg->text)
				free(msg->text);

			ListRemove(&level.queuedChatMessagesList, msg);
			ListIterate(&level.queuedChatMessagesList, &iter, qfalse);
		}
	}

#ifdef _DEBUG
	if ( g_antiWallhack.integer && g_wallhackMaxTraces.integer && level.wallhackTracesDone ) {
#if 0
		G_LogPrintf( "Last frame's WH check terminated with %d traces done\n", level.wallhackTracesDone );
#endif
		if ( level.wallhackTracesDone > g_wallhackMaxTraces.integer ) {
			G_LogPrintf( "Last frame's WH check terminated prematurely with %d traces (limit: %d)\n", level.wallhackTracesDone, g_wallhackMaxTraces.integer );
		}
	}
#endif

#ifdef NEWMOD_SUPPORT
	static qboolean flagsSet = qfalse; // make sure it gets set at least once
	static int saberFlags, netUnlock;
	int numRed = 0, numBlue = 0;
	CountPlayers(NULL, &numRed, &numBlue, NULL, NULL, NULL, NULL);
	const qboolean wasRestartedNow = (level.wasRestarted && !level.someoneWasAFK && numRed >= 2 && numBlue >= 2);
	static qboolean lastWasRestartedNow = qfalse;
	if (!flagsSet || saberFlags != g_balanceSaber.integer || netUnlock != g_netUnlock.integer || wasRestartedNow != lastWasRestartedNow) {
		saberFlags = g_balanceSaber.integer;
		netUnlock = g_netUnlock.integer;

		int sendFlags = 0;
		if (saberFlags & SB_KICK)
			sendFlags |= NMF_KICK;
		if (saberFlags & SB_BACKFLIP)
			sendFlags |= NMF_BACKFLIP;
		if (netUnlock)
			sendFlags |= NMF_NETUNLOCK;
		if (wasRestartedNow)
			sendFlags |= NMF_WASRESTARTED;

		trap_Cvar_Set("g_nmFlags", va("%i", sendFlags));
		flagsSet = qtrue;
	}
	lastWasRestartedNow = wasRestartedNow;
#endif

	level.wallhackTracesDone = 0; // reset the traces for the next ClientThink wave

	UpdateGlobalCenterPrint( levelTime );

	// check for modified physics and disable capture times if non standard
#ifndef _DEBUG
	if ( level.racemodeRecordsEnabled && !level.racemodeRecordsReadonly && level.time > 1000 ) { // wat. it seems that sv_cheats = 1 on first frame... so don't check until 1000ms i guess
		if ( g_cheats.integer != 0 ) {
			G_Printf( S_COLOR_YELLOW"Cheats are enabled. Capture records won't be tracked during this map.\n" );
			level.racemodeRecordsReadonly = qtrue;
		} else if ( !pmove_float.integer ) {
			G_Printf( S_COLOR_YELLOW"pmove_float is not enabled. Capture records won't be tracked during this map.\n" );
			level.racemodeRecordsReadonly = qtrue;
		} else if ( g_svfps.integer != 30 ) {
			G_Printf( S_COLOR_YELLOW"Server FPS is not standard. Capture records won't be tracked during this map.\n" );
			level.racemodeRecordsReadonly = qtrue;
		} else if ( g_speed.value != 250 ) {
			G_Printf( S_COLOR_YELLOW"Speed is not standard. Capture records won't be tracked during this map.\n" );
			level.racemodeRecordsReadonly = qtrue;
		} else if ( g_gravity.value != 760 ) {
			G_Printf( S_COLOR_YELLOW"Gravity is not standard. Capture records won't be tracked during this map.\n" );
			level.racemodeRecordsReadonly = qtrue;
		} else if ( g_knockback.value != 1000 ) {
			G_Printf( S_COLOR_YELLOW"Knockback is not standard. Capture records won't be tracked during this map.\n" );
			level.racemodeRecordsReadonly = qtrue;
		} else if ( g_forceRegenTime.value != 231 ) {
			G_Printf( S_COLOR_YELLOW"Force regen is not standard. Capture records won't be tracked during this map.\n" );
			level.racemodeRecordsReadonly = qtrue;
		}
	}
#endif

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

	RunRockPaperScissors();

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

			iterator_t iter;
			ListIterate(&level.disconnectedPlayerList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				disconnectedPlayerData_t *data = IteratorNext(&iter);
				if (data->airOutTime)
					data->airOutTime += dt;
				if (data->ps.saberDidThrowTime)
					data->ps.saberDidThrowTime += dt;
				if (data->ps.saberThrowDelay)
					data->ps.saberThrowDelay += dt;
				if (data->invulnerableTimer)
					data->invulnerableTimer += dt;
				if (data->saberKnockedTime)
					data->saberKnockedTime += dt;
				if (data->homingLockTime)
					data->homingLockTime += dt;
				if (data->protsince)
					data->protsince += dt;
				if (data->pain_debounce_time)
					data->pain_debounce_time += dt;
				if (data->ps.fallingToDeath)
					data->ps.fallingToDeath += dt;
				if (data->enterTime)
					data->enterTime += dt;
				if (data->lastreturnedflag)
					data->lastreturnedflag += dt;
				if (data->lasthurtcarrier)
					data->lasthurtcarrier += dt;
				if (data->lastfraggedcarrier)
					data->lastfraggedcarrier += dt;
				if (data->flagsinceStartTime)
					data->flagsinceStartTime += dt;
				if (data->ps.rocketLastValidTime)
					data->ps.rocketLastValidTime += dt;
				if (data->ps.rocketTargetTime)
					data->ps.rocketTargetTime += dt;
				if (data->ps.rocketLockTime)
					data->ps.rocketLockTime += dt;
				if (data->grippedAnAbsorberTime)
					data->grippedAnAbsorberTime += dt;
				if (data->ps.fd.forceGripBeingGripped)
					data->ps.fd.forceGripBeingGripped += dt;
				if (data->ps.otherKillerTime)
					data->ps.otherKillerTime += dt;
				if (data->ps.otherKillerDebounceTime)
					data->ps.otherKillerDebounceTime += dt;
				if (data->ps.fd.forceGripStarted)
					data->ps.fd.forceGripStarted += dt;
				if (data->ps.forceGripMoveInterval)
					data->ps.forceGripMoveInterval += dt;
				if (data->ps.forceHandExtendTime)
					data->ps.forceHandExtendTime += dt;
				if (data->forcePowerSoundDebounce)
					data->forcePowerSoundDebounce += dt;
				if (data->ps.fd.forceRageRecoveryTime)
					data->ps.fd.forceRageRecoveryTime += dt;
				if (data->ps.forceAllowDeactivateTime)
					data->ps.forceAllowDeactivateTime += dt;
				if (data->ps.electrifyTime)
					data->ps.electrifyTime += dt;
				if (data->drainDebuffTime)
					data->drainDebuffTime += dt;
				if (data->noLightningTime)
					data->noLightningTime += dt;
				if (data->ps.powerups[PW_DISINT_4])
					data->ps.powerups[PW_DISINT_4] += dt;
				if (data->ps.fd.forceGripUseTime)
					data->ps.fd.forceGripUseTime += dt;
				if (data->ps.fd.forceHealTime)
					data->ps.fd.forceHealTime += dt;
				if (data->respawnTime)
					data->respawnTime += dt;
				data->ps.fd.forcePowerRegenDebounceTime += dt;
				for (int i = 0; i < NUM_FORCE_POWERS; ++i) {
					if (data->ps.fd.forcePowerDuration[i])
						data->ps.fd.forcePowerDuration[i] += dt;
				}
			}
    }
    if ( level.pause.state == PAUSE_PAUSED )
    {
            if ( lastMsgTime < level.time-500 ) {
				int j;
				for ( j = 0; j < level.maxclients; ++j ) {
					if ( !g_entities[j].inuse || !g_entities[j].client ) {
						continue;
					}

					// don't print to racers or racespectators
					if ( g_entities[j].client->ps.stats[STAT_RACEMODE] ) {
						continue;
					}

					int pauseSecondsRemaining = (int)ceilf((level.pause.time - level.time) / 1000.0f);
					if (g_quickPauseMute.integer && level.pause.pauserClient.valid && j == level.pause.pauserClient.clientNum && !level.pause.pauserChoice) {
						trap_SendServerCommand(j, va("cp \"How long do you need?\n^1Enter a choice in chat. ^7(%s%d^7)\n^51 ^7- A minute or less\n^52 ^7- A couple minutes\n^53 ^7- Several minutes\n\n\n\n\"",
							pauseSecondsRemaining <= 10 ? S_COLOR_RED : S_COLOR_WHITE, pauseSecondsRemaining));
					}
					else {
						if (level.pause.reason[0]) {
							trap_SendServerCommand(j, va("cp \"The match has been auto-paused. (%s%d^7)\n%s\n\"",
								pauseSecondsRemaining <= 10 ? S_COLOR_RED : S_COLOR_WHITE, pauseSecondsRemaining, level.pause.reason));
						}
						else {
							trap_SendServerCommand(j, va("cp \"The match has been paused. (%s%d^7)\n\"",
								pauseSecondsRemaining <= 10 ? S_COLOR_RED : S_COLOR_WHITE, pauseSecondsRemaining));
						}
					}
				}
				trap_SendServerCommand(-1, "lchat \"pause\"");
				lastMsgTime = level.time;
            }

			//if ( level.time > level.pause.time - (japp_unpauseTime.integer*1000) )
            if ( level.time > level.pause.time - (5*1000) ) // 5 seconds
                    level.pause.state = PAUSE_UNPAUSING;
    }
    if ( level.pause.state == PAUSE_UNPAUSING )
    {
		level.pause.reason[0] = '\0';
            if ( lastMsgTime < level.time-500 ) {
				int j;
				for ( j = 0; j < level.maxclients; ++j ) {
					if ( !g_entities[j].inuse || !g_entities[j].client ) {
						continue;
					}

					// don't print to racers or racespectators
					if ( g_entities[j].client->ps.stats[STAT_RACEMODE] ) {
						continue;
					}

					if (g_quickPauseMute.integer && level.pause.pauserClient.valid && j == level.pause.pauserClient.clientNum && !level.pause.pauserChoice) {
						trap_SendServerCommand(j, va("cp \"How long do you need?\n^1GAME IS UNPAUSING, ENTER A CHOICE IN CHAT! ^7(^1%.0f^7)\n^51 ^7- A minute or less\n^52 ^7- A couple minutes\n^53 ^7- Several minutes\n\n\n\n\"",
							ceil((level.pause.time - level.time) / 1000.0f)));
					}
					else {
						trap_SendServerCommand(j, va("cp \"MATCH IS UNPAUSING\nin %.0f...\n\"", ceil((level.pause.time - level.time) / 1000.0f)));
					}
				}
				trap_SendServerCommand(-1, "lchat \"pause\"");
				lastMsgTime = level.time;
            }

            if ( level.time > level.pause.time ) {
				for (int i = 0; i < MAX_GENTITIES; i++) { // reset all entities' pause angles
					gentity_t *ent = &g_entities[i];
					ent->lockPauseAngles = qfalse;
					ent->pauseAngles[0] = ent->pauseAngles[1] = ent->pauseAngles[2] = 0;
					ent->pauseViewAngles[0] = ent->pauseViewAngles[1] = ent->pauseViewAngles[2] = 0;
					memset(ent->hasLOS, 0, sizeof(ent->hasLOS));
				}

				int j;
				for ( j = 0; j < level.maxclients; ++j ) {
					if ( !g_entities[j].inuse || !g_entities[j].client ) {
						continue;
					}

					// don't print to racers or racespectators
					if ( g_entities[j].client->ps.stats[STAT_RACEMODE] ) {
						continue;
					}

					trap_SendServerCommand( j, "cp \"Go!\n\"" );

					SendForceTimers(&g_entities[j], NULL);
				}
				
				ListClear(&level.disconnectedPlayerList);
				trap_SendServerCommand(-1, "lchat \"unpause\"");
				level.pause.state = PAUSE_NONE;
				level.pause.pauserChoice = 0;
				level.pause.pauserClient.valid = qfalse;

				// restore the buttons they were pressing back when the pause began (keeps grips going etc)
				for (int i = 0; i < MAX_CLIENTS; i++)
					level.clients[i].pers.cmd.buttons = level.clients[i].pers.buttonsPressedAtPauseStart;
            }
    }

	if (level.pause.state != PAUSE_NONE) { // lock angles during pause
		static int lastAngleTime = 0;
		const int now = trap_Milliseconds();
		qboolean doAngle = qfalse;
#define PAUSE_VELOCITY_LINE_TIME	(1000)
#define PAUSE_VELOCITY_LINE_BUFFER	(50)
		if (g_pauseVelocityLines.integer && !level.intermissiontime && (!lastAngleTime || now - lastAngleTime >= (PAUSE_VELOCITY_LINE_TIME - PAUSE_VELOCITY_LINE_BUFFER)) && level.pause.time - level.time > PAUSE_VELOCITY_LINE_TIME) {
			lastAngleTime = now;
			doAngle = qtrue;
		}

		for (int i = 0; i < MAX_GENTITIES; i++) {
			gentity_t *ent = &g_entities[i];
			if (ent->lockPauseAngles && ent->inuse && !IsRacerOrSpectator(ent)) {
				VectorCopy(ent->pauseAngles, ent->s.angles);
				if (ent->client) {
					VectorCopy(ent->pauseViewAngles, ent->client->ps.viewangles);
					SetClientViewAngle(ent, ent->client->ps.viewangles);

					if (doAngle && ent->health > 0 && (ent->client->ps.velocity[0] || ent->client->ps.velocity[1] || ent->client->ps.velocity[2])) {
						vec3_t end;
						VectorMA(ent->client->ps.origin, 0.25f, ent->client->ps.velocity, end); // make the line a little shorter
						gentity_t *te;
						te = G_TempEntity(ent->client->ps.origin, EV_TESTLINE);
						te->s.saberInFlight = qtrue; // to identify this type of line i guess
						VectorCopy(ent->client->ps.origin, te->s.origin);
						VectorCopy(end, te->s.origin2);
						te->s.time2 = PAUSE_VELOCITY_LINE_TIME;
						if (ent->client->sess.sessionTeam == TEAM_RED)
							te->s.weapon = 0xffffff; // blue color
						else if (ent->client->sess.sessionTeam == TEAM_BLUE)
							te->s.weapon = 4; // red color

						te->r.svFlags |= SVF_BROADCAST;
						for (int j = 0; j < MAX_CLIENTS; j++) {
							gentity_t *otherEnt = &g_entities[j];
							if (!otherEnt->inuse || !otherEnt->client || otherEnt->client->pers.connected != CON_CONNECTED) {
								//te->r.broadcastClients[1] |= (1 << j);
								continue;
							}

							if (otherEnt->client->sess.sessionTeam == TEAM_SPECTATOR && otherEnt->client->sess.spectatorState != SPECTATOR_FOLLOW) // freespecs receive all lines
								continue;

							gentity_t *LOSEnt;
							if (otherEnt->client->sess.sessionTeam == TEAM_SPECTATOR && otherEnt->client->sess.spectatorState == SPECTATOR_FOLLOW &&
								g_entities[otherEnt->client->sess.spectatorClient].inuse && g_entities[otherEnt->client->sess.spectatorClient].client &&
								g_entities[otherEnt->client->sess.spectatorClient].client->pers.connected == CON_CONNECTED) {
								LOSEnt = &g_entities[otherEnt->client->sess.spectatorClient]; // the person followed by the player
							}
							else {
								LOSEnt = otherEnt; // the player
							}

							if (LOSEnt == ent)  // you always get your own line
								continue;

							if (LOSEnt->client->sess.inRacemode) { // racers don't receive lines
								te->r.broadcastClients[1] |= (1 << j);
								continue;
							}

							if (!LOSEnt->hasLOS[i]) // no line if no LOS
								te->r.broadcastClients[1] |= (1 << j);
						}
					}
				}
			}
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

	ChangeToNextStatsBlockIfNeeded();

	{ // periodic pos-related checks every 10s, starting 30s in
		if (!level.posChecksTime)
			level.posChecksTime = 30000;

		if (level.time - level.startTime >= level.posChecksTime) {
			level.posChecksTime = (level.time - level.startTime) + 10000;

			int numIngame = 0;
			CountPlayers(NULL, NULL, NULL, NULL, NULL, &numIngame, NULL);

			if (numIngame <= 4) {
				// if 4 or less ingame, clear reminded pos unless debug build
#ifndef _DEBUG
				TeamGen_ClearRemindPositions(qtrue);
#endif
			}
			else if (numIngame == 8 && g_assignMissingCtfPos.integer && g_gametype.integer == GT_CTF) {
				// if 8 ingame and 7 of them have reminded pos, give reminded pos to the last guy
				int numWithRemindedPos[TEAM_NUM_TEAMS] = { 0 }, numBase[TEAM_NUM_TEAMS] = { 0 }, numChase[TEAM_NUM_TEAMS] = { 0 }, numOffense[TEAM_NUM_TEAMS] = { 0 };
				gentity_t *ingameDudeWithoutRemindedPos = NULL;
				for (int i = 0; i < MAX_CLIENTS; i++) {
					gentity_t *thisEnt = &g_entities[i];
					if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED || IsRacerOrSpectator(thisEnt))
						continue;

					if (!thisEnt->client->sess.remindPositionOnMapChange.valid) {
						ingameDudeWithoutRemindedPos = thisEnt;
						continue;
					}

					++numWithRemindedPos[thisEnt->client->sess.sessionTeam];
					switch (thisEnt->client->sess.remindPositionOnMapChange.pos) {
					case CTFPOSITION_BASE: ++numBase[thisEnt->client->sess.sessionTeam]; break;
					case CTFPOSITION_CHASE: ++numChase[thisEnt->client->sess.sessionTeam]; break;
					case CTFPOSITION_OFFENSE: ++numOffense[thisEnt->client->sess.sessionTeam]; break;
					}
				}

				if (numIngame == 8 && numWithRemindedPos[TEAM_RED] + numWithRemindedPos[TEAM_BLUE] == 7 &&
					abs(numWithRemindedPos[TEAM_RED] - numWithRemindedPos[TEAM_BLUE]) == 1 && ingameDudeWithoutRemindedPos) {
					assert(ingameDudeWithoutRemindedPos->client && !ingameDudeWithoutRemindedPos->client->sess.remindPositionOnMapChange.valid);
					ctfPosition_t newPos = CTFPOSITION_UNKNOWN;

					if (!numBase[ingameDudeWithoutRemindedPos->client->sess.sessionTeam])
						newPos = CTFPOSITION_BASE;
					else if (!numChase[ingameDudeWithoutRemindedPos->client->sess.sessionTeam])
						newPos = CTFPOSITION_CHASE;
					else if (numOffense[ingameDudeWithoutRemindedPos->client->sess.sessionTeam] == 1)
						newPos = CTFPOSITION_OFFENSE;

					if (newPos) {
						Com_Printf("g_assignMissingCtfPos: assigned missing position %s to client %d (%s)\n",
							NameForPos(newPos), ingameDudeWithoutRemindedPos - g_entities, ingameDudeWithoutRemindedPos->client->pers.netname);

						int score;
						switch (newPos) {
						case CTFPOSITION_BASE: score = 8000; break;
						case CTFPOSITION_CHASE: score = 4000; break;
						case CTFPOSITION_OFFENSE: score = 1000; break;
						}
						ingameDudeWithoutRemindedPos->client->sess.remindPositionOnMapChange.score = score;
						if (!level.wasRestarted)
							ingameDudeWithoutRemindedPos->client->ps.persistant[PERS_SCORE] = score;
						ingameDudeWithoutRemindedPos->client->sess.remindPositionOnMapChange.pos = newPos;
						ingameDudeWithoutRemindedPos->client->sess.remindPositionOnMapChange.valid = qtrue;
						if (ingameDudeWithoutRemindedPos->client->stats)
							ingameDudeWithoutRemindedPos->client->stats->remindedPosition = newPos;

						if (ingameDudeWithoutRemindedPos->client->account)
							G_DBSetMetadata(va("remindpos_account_%d", ingameDudeWithoutRemindedPos->client->account->id), va("%d", newPos));

						// refreshing clientinfo here generally shouldn't be a lag issue because
						// it's unlikely the subbing guy would sub in and everyone starts playing that fast
						if (g_broadcastCtfPos.integer /*&& !level.wasRestarted*/)
							ClientUserinfoChanged(ingameDudeWithoutRemindedPos - g_entities);
					}
				}
				else if (numIngame == 8 && g_assignMissingCtfPos.integer && !numWithRemindedPos[TEAM_RED] && !numWithRemindedPos[TEAM_BLUE] && level.wasRestarted && !level.someoneWasAFK &&
					(level.time - level.startTime) >= (CTFPOSITION_MINIMUM_SECONDS * 1000) && IsLivePug(0)) {
					// try to determine positions for scoreboard a couple minutes into non-teamgen pugs
					int numGotPos = 0, numPos[TEAM_NUM_TEAMS][CTFPOSITION_OFFENSE + 1] = { 0 };
					for (int i = 0; i < MAX_CLIENTS; i++) {
						gentity_t *thisEnt = &g_entities[i];
						if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED || IsRacerOrSpectator(thisEnt))
							continue;

						int pos = DetermineCTFPosition(thisEnt->client->stats, qtrue);
						if (pos) {
							++numGotPos;
							++numPos[thisEnt->client->sess.sessionTeam][pos];
						}
					}

					if (numGotPos == 8 &&
						numPos[TEAM_RED][CTFPOSITION_BASE] == 1 && numPos[TEAM_RED][CTFPOSITION_CHASE] == 1 && numPos[TEAM_RED][CTFPOSITION_OFFENSE] == 2 &&
						numPos[TEAM_BLUE][CTFPOSITION_BASE] == 1 && numPos[TEAM_BLUE][CTFPOSITION_CHASE] == 1 && numPos[TEAM_BLUE][CTFPOSITION_OFFENSE] == 2) {
						for (int i = 0; i < MAX_CLIENTS; i++) {
							gentity_t *thisEnt = &g_entities[i];
							if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED || IsRacerOrSpectator(thisEnt))
								continue;

							int pos = DetermineCTFPosition(thisEnt->client->stats, qtrue);

							thisEnt->client->sess.remindPositionOnMapChange.pos = pos;
							if (thisEnt->client->stats)
								thisEnt->client->stats->remindedPosition = pos;
							thisEnt->client->sess.remindPositionOnMapChange.valid = qtrue;
							switch (pos) {
							case CTFPOSITION_BASE: thisEnt->client->sess.remindPositionOnMapChange.score = 8000; break;
							case CTFPOSITION_CHASE: thisEnt->client->sess.remindPositionOnMapChange.score = 4000; break;
							case CTFPOSITION_OFFENSE: thisEnt->client->sess.remindPositionOnMapChange.score = 1000; break;
							}
							if (thisEnt->client->account)
								G_DBSetMetadata(va("remindpos_account_%d", thisEnt->client->account->id), va("%d", pos));

							ClientUserinfoChanged(i);
							Com_Printf("g_assignMissingCtfPos: automatically set client %d (%s)'s current position to %s.\n", i, thisEnt->client->pers.netname, NameForPos(pos));
						}
					}
					PrintIngame(-1, "Automatically detected CTF positions.\n");
				}
			}
		}
	}

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

		// not a client, update broadcasts
		if ( i >= MAX_CLIENTS ) {
			G_UpdateNonClientBroadcasts( ent );
		}

		if ( ent->s.eType == ET_MISSILE ) {
            //OSP: pause
            if ( level.pause.state == PAUSE_NONE || ( ent->r.ownerNum >= 0 && ent->r.ownerNum < MAX_CLIENTS && level.clients[ent->r.ownerNum].sess.inRacemode ) )
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

			if (ent->client->isHacking && ( level.pause.state == PAUSE_NONE || ent->client->sess.inRacemode ))
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
			if (ent->client->jetPackOn && ( level.pause.state == PAUSE_NONE || ent->client->sess.inRacemode ))
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
			else if (ent->client->ps.jetpackFuel < 100 && (level.pause.state == PAUSE_NONE || ent->client->sess.inRacemode))
			{ //recharge jetpack
				if (ent->client->jetPackDebRecharge < level.time)
				{
					ent->client->ps.jetpackFuel++;
					ent->client->jetPackDebRecharge = level.time + JETPACK_REFUEL_RATE;
				}
			}

#define CLOAK_DEFUEL_RATE		200 //approx. 20 seconds of idle use from a fully charged fuel amt
#define CLOAK_REFUEL_RATE		150 //seems fair
			if (ent->client->ps.powerups[PW_CLOAKED] && (level.pause.state == PAUSE_NONE || ent->client->sess.inRacemode))
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

			qboolean meme = (!level.wasRestarted && ent && ent->client && ent->client->account && (!Q_stricmp(ent->client->account->name, "duo") || !Q_stricmp(ent->client->account->name, "alpha")));
			if (meme && ent->client->sess.sessionTeam == TEAM_SPECTATOR)
				WP_ForcePowersUpdate(ent, &ent->client->pers.cmd);

            if ( ( level.pause.state == PAUSE_NONE || ent->client->sess.inRacemode )
                    && !level.intermissiontime
                    && !(ent->client->ps.pm_flags & PMF_FOLLOW)
                    && ent->client->sess.sessionTeam != TEAM_SPECTATOR )
			{
				if (!ent->isAimPracticePack) { // aim bots don't have sabers anyway; skipping this avoids a stupid ghoul2 crash
					WP_ForcePowersUpdate(ent, &ent->client->pers.cmd);
					WP_SaberPositionUpdate(ent, &ent->client->pers.cmd);
					WP_SaberStartMissileBlockCheck(ent, &ent->client->pers.cmd);
				}

				if (ent->client->sess.sessionTeam == TEAM_RED) {
					level.numRedPlayerTicks++;
					AddPlayerTick(TEAM_RED, ent);
				} else if (ent->client->sess.sessionTeam == TEAM_BLUE) {
					level.numBluePlayerTicks++;
					AddPlayerTick(TEAM_BLUE, ent);
				}

				if ( ent->client->ps.stats[STAT_HEALTH] > 0 && !( ent->client->ps.eFlags & EF_DEAD ) ) {
					// this client is in game and alive, update speed stats

					float xyspeed = 0;

					if ( ent->client->ps.m_iVehicleNum ) {
						gentity_t *currentVeh = &g_entities[ent->client->ps.m_iVehicleNum];

						if ( currentVeh->client ) {
							xyspeed = sqrt( currentVeh->client->ps.velocity[0] * currentVeh->client->ps.velocity[0] + currentVeh->client->ps.velocity[1] * currentVeh->client->ps.velocity[1] );
						}
					} else {
						xyspeed = sqrt( ent->client->ps.velocity[0] * ent->client->ps.velocity[0] + ent->client->ps.velocity[1] * ent->client->ps.velocity[1] );
					}

					if (ent->client->sess.inRacemode && ent->aimPracticeEntBeingUsed && ent->aimPracticeMode) {
						aimPracticePack_t *pack = ent->aimPracticeEntBeingUsed->isAimPracticePack;
						if (pack->maxSpeed && xyspeed > pack->maxSpeed) {
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
								//CenterPrintToPlayerAndFollowers(ent, va("Do not move faster than %d ups!\nRestarting...", pack->maxSpeed));
							}
							else { // we are the only one using this pack; go ahead and restart it immediately so they don't have to wait
								RandomizeAndRestartPack(ent->aimPracticeEntBeingUsed->isAimPracticePack);
								//CenterPrintToPlayerAndFollowers(ent, va("Do not move faster than %d ups!\nRestarting.", pack->maxSpeed));
							}
						}
					}

					if (ent->client->stats && (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE)) {
						// only track overall displacement if you are actually ingame
						ent->client->stats->displacement += xyspeed / g_svfps.value;
						ent->client->stats->displacementSamples++;
						ent->client->stats->forceSamples++;
						if (ent->client->ps.fd.forceSide == FORCE_DARKSIDE)
							ent->client->stats->darkForceSamples++;
						if (g_gametype.integer == GT_CTF) {
							ctfRegion_t region = GetCTFRegion(ent);
							if (region != CTFREGION_INVALID)
								ent->client->stats->regionTime[region] += (int)(1000.0f / g_svfps.value);
						}
					}

					if ( ent->client->stats && xyspeed > ent->client->stats->topSpeed ) {
						ent->client->stats->topSpeed = xyspeed;
					}

					// if they carry a flag, also update fastcap speed stats
					if ( ent->playerState->powerups[PW_REDFLAG] || ent->playerState->powerups[PW_BLUEFLAG] ) {
						ent->client->pers.fastcapDisplacement += xyspeed / g_svfps.value;
						ent->client->pers.fastcapDisplacementSamples++;

						if ( xyspeed > ent->client->pers.fastcapTopSpeed ) {
							ent->client->pers.fastcapTopSpeed = xyspeed;
						}
					} else {
						ent->client->pers.fastcapDisplacement = 0;
						ent->client->pers.fastcapTopSpeed = 0;
						ent->client->pers.fastcapDisplacementSamples = 0;
					}
				}
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

			if (!ent->isAimPracticePack) { // aim bots don't have sabers anyway; skipping this avoids a stupid ghoul2 crash
				WP_ForcePowersUpdate(ent, &ent->client->pers.cmd);
				WP_SaberPositionUpdate(ent, &ent->client->pers.cmd);
				WP_SaberStartMissileBlockCheck(ent, &ent->client->pers.cmd);
			}
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

	if (level.pause.state == PAUSE_NONE && !level.intermissiontime) {
		level.numTeamTicks++;
	}

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

#ifdef NEWMOD_SUPPORT
	// send extra data to specs
	CheckSpecInfo();
#endif

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

	// duo: new afk detection
	if (g_notifyAFK.integer > 0 && !level.checkedForAFKs && level.firstFrameTime && (trap_Milliseconds() - level.firstFrameTime >= (g_notifyAFK.integer * 1000))) {
		CheckForAFKs();
		level.checkedForAFKs = qtrue;
	}

	// if we play at least 5 minutes of a live match, allow the auto map vote to happen again
	if ((g_lastMapVotedMap.string[0] || g_lastMapVotedTime.string[0]) && level.time - level.startTime >= 300000 && PauseConditions()) {
		trap_Cvar_Set("g_lastMapVotedMap", "");
		trap_Cvar_Set("g_lastMapVotedTime", "");
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
	else if ( g_autoSendScores.integer )
	{
		// if we don't have a message queued, do it now
		gQueueScoreMessage = qtrue;
		gQueueScoreMessageTime = g_autoSendScores.integer;

		if ( gQueueScoreMessageTime < 500 ) {
			gQueueScoreMessageTime = 500;
		}

		gQueueScoreMessageTime += level.time;
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

#ifdef NEWMOD_SUPPORT
	RunImprovedHoming();
#endif

	WaitForAFKs();

	PeriodicallyRecalculateTeamBalance();

	if (g_rocketHPFix.integer) {
		for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
			gentity_t *ent = &g_entities[i];
			if (!ent->inuse || !VALIDSTRING(ent->classname) || Q_stricmp(ent->classname, "rocket_proj"))
				continue;
			if (ent->parent && IsRacerOrSpectator(ent->parent))
				continue; // never affect racemode rockets
			ent->s.solid = 2294531; // what the fuck?
		}
	}

	if (!level.firstFrameTime)
		level.firstFrameTime = trap_Milliseconds();

	level.frameStartTime = trap_Milliseconds(); // accurate timer

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

qboolean InstagibEnabled(void) {
	if (level.instagibMap)
		return qtrue; // always enable on instagib maps as defined by the worldspawn key "b_e_instagib"
	if (g_instagib.integer)
		return qtrue; // enable on any map if the cvar is enabled
	return qfalse;
}


