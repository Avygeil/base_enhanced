// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_local.h -- local definitions for game module

#ifndef __G_LOCAL__
#define __G_LOCAL__

#include "q_shared.h"
#include "collections.h"
#include "bg_public.h"
#include "bg_vehicles.h"
#include "g_public.h"

#include "crypto.h"

#include "xxhash.h"

#ifndef __LCC__
#define GAME_INLINE ID_INLINE
#else
#define GAME_INLINE //none
#endif

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

//npc stuff
#include "b_public.h"

extern int gPainMOD;
extern int gPainHitLoc;
extern vec3_t gPainPoint;

#ifdef _DEBUG
#define Com_DebugPrintf(...)	Com_Printf(__VA_ARGS__)
#else
#define Com_DebugPrintf(...)	do {} while (0)
#endif

//==================================================================

// the "gameversion" client command will print this plus compile date
#define	GAMEVERSION	"base_enhanced"
//#define	GAMEVERSION	"basejka" //test

#define NEWMOD_SUPPORT

#ifdef NEWMOD_SUPPORT
#define NM_AUTH_PROTOCOL				5

#define PUBLIC_KEY_FILENAME				"public_key.bin"
#define SECRET_KEY_FILENAME				"secret_key.bin"
#endif

#define MAX_NETNAME 36

#define MAX_USERNAME_SIZE 32 //username size	16
#define MAX_PASSWORD	16

#define BODY_QUEUE_SIZE		8

#ifndef Q3INFINITE
#define Q3INFINITE			1000000
#endif

#define ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))

#define	FRAMETIME			100					// msec
#define	CARNAGE_REWARD_TIME	3000
#define REWARD_SPRITE_TIME	2000

#define	INTERMISSION_DELAY_TIME	1000
#define	SP_INTERMISSION_DELAY_TIME	5000

//primarily used by NPCs
#define	START_TIME_LINK_ENTS		FRAMETIME*1 // time-delay after map start at which all ents have been spawned, so can link them
#define	START_TIME_FIND_LINKS		FRAMETIME*2 // time-delay after map start at which you can find linked entities
#define	START_TIME_MOVERS_SPAWNED	FRAMETIME*2 // time-delay after map start at which all movers should be spawned
#define	START_TIME_REMOVE_ENTS		FRAMETIME*3 // time-delay after map start to remove temporary ents
#define	START_TIME_NAV_CALC			FRAMETIME*4 // time-delay after map start to connect waypoints and calc routes
#define	START_TIME_FIND_WAYPOINT	FRAMETIME*5 // time-delay after map start after which it's okay to try to find your best waypoint

// gentity->flags
#define	FL_GODMODE				0x00000010
#define	FL_NOTARGET				0x00000020
#define	FL_TEAMSLAVE			0x00000400	// not the first on the team
#define FL_NO_KNOCKBACK			0x00000800
#define FL_DROPPED_ITEM			0x00001000
#define FL_NO_BOTS				0x00002000	// spawn point not for bot use
#define FL_NO_HUMANS			0x00004000	// spawn point just for bots
#define FL_FORCE_GESTURE		0x00008000	// force gesture on client
#define FL_INACTIVE				0x00010000	// inactive
#define FL_NAVGOAL				0x00020000	// for npc nav stuff
#define	FL_DONT_SHOOT			0x00040000
#define FL_SHIELDED				0x00080000
#define FL_UNDYING				0x00100000	// takes damage down to 1, but never dies

//ex-eFlags -rww
#define	FL_BOUNCE				0x00100000		// for missiles
#define	FL_BOUNCE_HALF			0x00200000		// for missiles
#define	FL_BOUNCE_SHRAPNEL		0x00400000		// special shrapnel flag

//vehicle game-local stuff -rww
#define	FL_VEH_BOARDING			0x00800000		// special shrapnel flag

//breakable flags -rww
#define FL_DMG_BY_SABER_ONLY		0x01000000 //only take dmg from saber
#define FL_DMG_BY_HEAVY_WEAP_ONLY	0x02000000 //only take dmg from explosives

#define FL_BBRUSH					0x04000000 //I am a breakable brush

#ifndef FINAL_BUILD
#define DEBUG_SABER_BOX
#endif

#define	MAX_G_SHARED_BUFFER_SIZE		8192
extern char gSharedBuffer[MAX_G_SHARED_BUFFER_SIZE];

// movers are things like doors, plats, buttons, etc
typedef enum {
	MOVER_POS1,
	MOVER_POS2,
	MOVER_1TO2,
	MOVER_2TO1
} moverState_t;

#define SP_PODIUM_MODEL		"models/mapobjects/podium/podium4.md3"

enum 
{
	HL_NONE = 0,
	HL_FOOT_RT,
	HL_FOOT_LT,
	HL_LEG_RT,
	HL_LEG_LT,
	HL_WAIST,
	HL_BACK_RT,
	HL_BACK_LT,
	HL_BACK,
	HL_CHEST_RT,
	HL_CHEST_LT,
	HL_CHEST,
	HL_ARM_RT,
	HL_ARM_LT,
	HL_HAND_RT,
	HL_HAND_LT,
	HL_HEAD,
	HL_GENERIC1,
	HL_GENERIC2,
	HL_GENERIC3,
	HL_GENERIC4,
	HL_GENERIC5,
	HL_GENERIC6,
	HL_MAX
};

//
// g_aim.c
//
typedef struct { //Should this store their g2 anim? for proper g2 sync?
	vec3_t	mins, maxs;
	vec3_t	currentOrigin;
	vec3_t	angles;
	vec3_t	velocity;
	int		time, torsoAnim, torsoTimer, legsAnim, legsTimer;
} aimPracticeMovementTrail_t;

#define MAX_VARIANTS_PER_PACK			(16)
#define MAX_MOVEMENT_TRAILS				(4096)

typedef struct {
	aimPracticeMovementTrail_t	trail[MAX_MOVEMENT_TRAILS]; // array of trails of bot movement
	int							trailHead; // the number of trails of bot movement
	vec3_t						playerSpawnPoint; // where the player spawns
	float						playerSpawnYaw; // which direction the player is facing when they spawn
	float						playerVelocityYaw; // which direction the player's momentum is taking them when they spawn
	float						playerSpawnSpeed; // how fast the player should be moving when they spawn
} aimVariant_t;

typedef struct {
	node_t						node;

	int							sessionId; // the session id of this player
	int							score; // their score on the pack that this list belongs to
} cachedScore_t;

typedef struct {
	node_t						node;

	// stuff that gets saved
	char						packName[64]; // the name of the pack
	int							numVariants; // the number of variants in the variants array
	aimVariant_t				variants[MAX_VARIANTS_PER_PACK]; // array of routes that belong to this pack
	int							weaponMode; // which weapons you get to use
	int							ownerAccountId; // who created this (only they can edit it)
	int							numRepsPerVariant; // how many times to run each variant per weapon

	// stuff that does NOT get saved
#define MAX_REPS_PER_ROUTE		(16)
	int							currentTrailNum; // what trail number we are currently on (irrespective of variant)
	int							randomVariantOrder[MAX_VARIANTS_PER_PACK * MAX_REPS_PER_ROUTE]; // a randomized order of routes, e.g. 2 routes x 3 reps could be {1, 0, 1, 0, 0, 1}
	int							currentVariantIndex; // which actual variant we are currently on
	int							currentVariantNumInRandomVariantOrder; // how far through randomVariantOrder we are
	qboolean					forceRestart; // if qtrue, the NPC will restart on the next tick
	gentity_t					*ent; // the NPC entity linked to this pack
	qboolean					isTemp; // this is a temporary pack only (does not exist in the DB)
	qboolean					save; // this will be saved to the DB at the end of the round
	qboolean					deleteMe; // this will be deleted from the DB at the end of the round (also moves the NPC to an inaccessible spot in the map if true)
	qboolean					changed; // whether this has been altered since it was loaded from the DB, if applicable
	float						autoDist; // distance for recordroute to put player spawn from bot spawn if unspecified
	list_t						cachedScoresList; // list of scores for players
	int							routeStartTime;
	int							maxSpeed; // this must be at the end of the aimPracticePack_t struct to maintain backward compatibility!
} aimPracticePack_t;

typedef struct {
	node_t						node;
	char						packName[64];
	char						ownerAccountName[64];
} aimPracticePackMetaData_t;

// main
void LoadAimPacks(void);
void SaveDeleteAndFreeAimPacks(void);

// client
void G_InitClientAimRecordsCache(gclient_t *client);

// database
char *WeaponModeStringFromWeaponMode(int mode);

// npc_spawn
void FilterNPCForAimPractice(gentity_t *ent);

// cmds
void RandomizeAndRestartPack(aimPracticePack_t *pack);
qboolean SpawnAimPracticePackNPC(gentity_t *ent, aimPracticePack_t *pack);
void Cmd_Pack_f(gentity_t *ent);
void Cmd_TopAim_f(gentity_t *ent);

// pmove
void DoAimPackPmove(pmove_t *pm);



//============================================================================
extern void *precachedKyle;
extern void *g2SaberInstance;

extern qboolean gEscaping;
extern int gEscapeTime;

struct gentity_s {
	//rww - entstate must be first, to correspond with the bg shared entity structure
	entityState_t	s;				// communicated by server to clients
	playerState_t	*playerState;	//ptr to playerstate if applicable (for bg ents)
	Vehicle_t		*m_pVehicle; //vehicle data
	void			*ghoul2; //g2 instance
	int				localAnimIndex; //index locally (game/cgame) to anim data for this skel
	vec3_t			modelScale; //needed for g2 collision

	//From here up must be the same as centity_t/bgEntity_t

	entityShared_t	r;				// shared by both the server system and game

	//rww - these are shared icarus things. They must be in this order as well in relation to the entityshared structure.
	int				taskID[NUM_TIDS];
	parms_t			*parms;
	char			*behaviorSet[NUM_BSETS];
	char			*script_targetname;
	int				delayScriptTime;
	char			*fullName;

	//rww - targetname and classname are now shared as well. ICARUS needs access to them.
	char			*targetname;
	char			*classname;			// set in QuakeEd

	//rww - and yet more things to share. This is because the nav code is in the exe because it's all C++.
	int				waypoint;			//Set once per frame, if you've moved, and if someone asks
	int				lastWaypoint;		//To make sure you don't double-back
	int				lastValidWaypoint;	//ALWAYS valid -used for tracking someone you lost
	int				noWaypointTime;		//Debouncer - so don't keep checking every waypoint in existance every frame that you can't find one
	int				combatPoint;
	int				failedWaypoints[MAX_FAILED_NODES];
	int				failedWaypointCheckTime;

	int				next_roff_time; //rww - npc's need to know when they're getting roff'd

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!
	//================================

	struct gclient_s	*client;			// NULL if not a client

	gNPC_t		*NPC;//Only allocated if the entity becomes an NPC
	int			cantHitEnemyCounter;//HACK - Makes them look for another enemy on the same team if the one they're after can't be hit

	qboolean	noLumbar; //see note in cg_local.h

	qboolean	inuse;

	int			lockCount; //used by NPCs

	int			spawnflags;			// set in QuakeEd

	int			teamnodmg;			// damage will be ignored if it comes from this team

	char		*roffname;			// set in QuakeEd
	char		*rofftarget;		// set in QuakeEd

	char		*healingclass; //set in quakeed
	char		*healingsound; //set in quakeed
	int			healingrate; //set in quakeed
	int			healingDebounce; //debounce for generic object healing shiz

	char		*ownername;

	int			objective;
	int			side;

	int			passThroughNum;		// set to index to pass through (+1) for missiles

	int			aimDebounceTime;
	int			painDebounceTime;
	int			attackDebounceTime;
	int			alliedTeam;			// only useable by this team, never target this team

	int			roffid;				// if roffname != NULL then set on spawn

	qboolean	neverFree;			// if true, FreeEntity will only unlink
									// bodyque uses this

	int			flags;				// FL_* variables

	char		*model;
	char		*model2;
	int			freetime;			// level.time when the object was freed
	
	int			eventTime;			// events will be cleared EVENT_VALID_MSEC after set
	qboolean	freeAfterEvent;
	qboolean	unlinkAfterEvent;

	qboolean	physicsObject;		// if true, it can be pushed by movers and fall off edges
									// all game items are physicsObjects, 
	float		physicsBounce;		// 1.0 = continuous bounce, 0.0 = no bounce
	int			clipmask;			// brushes with this content value will be collided against
									// when moving.  items and corpses do not collide against
									// players, for instance

//Only used by NPC_spawners
	char		*NPC_type;
	char		*NPC_targetname;
	char		*NPC_target;

	// movers
	moverState_t moverState;
	int			soundPos1;
	int			sound1to2;
	int			sound2to1;
	int			soundPos2;
	int			soundLoop;
	gentity_t	*parent;
	gentity_t	*nextTrain;
	gentity_t	*prevTrain;
	vec3_t		pos1, pos2;

	//for npc's
	vec3_t		pos3;

	char		*message;

	int			timestamp;		// body queue sinking, etc

	float		angle;			// set in editor, -1 = up, -2 = down
	char		*target;
	char		*target2;
	char		*target3;		//For multiple targets, not used for firing/triggering/using, though, only for path branches
	char		*target4;		//For multiple targets, not used for firing/triggering/using, though, only for path branches
	char		*target5;		//mainly added for siege items
	char		*target6;		//mainly added for siege items

	char		*team;
	char		*targetShaderName;
	char		*targetShaderNewName;
	gentity_t	*target_ent;

	char		*closetarget;
	char		*opentarget;
	char		*paintarget;

	char		*goaltarget;
	char		*idealclass;

	float		radius;

	int			maxHealth; //used as a base for crosshair health display

	float		speed;
	vec3_t		movedir;
	float		mass;
	int			setTime;

//Think Functions
	int			nextthink;
	void		(*think)(gentity_t *self);
	void		(*reached)(gentity_t *self);	// movers call this when hitting endpoint
	void		(*blocked)(gentity_t *self, gentity_t *other);
	void		(*touch)(gentity_t *self, gentity_t *other, trace_t *trace);
	void		(*use)(gentity_t *self, gentity_t *other, gentity_t *activator);
	void		(*pain)(gentity_t *self, gentity_t *attacker, int damage);
	void		(*die)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);

	int			pain_debounce_time;
	int			fly_sound_debounce_time;	// wind tunnel
	int			last_move_time;

//Health and damage fields
	int			health;
	qboolean	takedamage;
	material_t	material;

	int			damage;
	int			dflags;
	int			splashDamage;	// quad will increase this without increasing radius
	int			splashRadius;
	int			methodOfDeath;
	int			splashMethodOfDeath;

	int			locationDamage[HL_MAX];		// Damage accumulated on different body locations

	int			count;
	int			bounceCount;
	qboolean	alt_fire;

	gentity_t	*chain;
	gentity_t	*enemy;
	gentity_t	*lastEnemy;
	gentity_t	*activator;
	gentity_t	*teamchain;		// next entity in team
	gentity_t	*teammaster;	// master of the team

	int			watertype;
	int			waterlevel;

	int			noise_index;

	// timing variables
	float		wait;
	float		random;
	int			delay;

	//generic values used by various entities for different purposes.
	int			genericValue1;
	int			genericValue2;
	int			genericValue3;
	int			genericValue4;
	int			genericValue5;
	int			genericValue6;
	int			genericValue7;
	int			genericValue8;
	int			genericValue9;
	int			genericValue10;
	int			genericValue11;
	int			genericValue12;
	int			genericValue13;
	int			genericValue14;
	int			genericValue15;

	char		*soundSet;

	qboolean	isSaberEntity;

	int			damageRedirect; //if entity takes damage, redirect to..
	int			damageRedirectTo; //this entity number

	vec3_t		epVelocity;
	float		epGravFactor;

	//is missile reflected? (to not count accuracy hits)
	qboolean    isReflected;

	gitem_t		*item;			// for bonus items

	qboolean	raceDimensionEvent;

	gentity_t	*twin;

	aimPracticePack_t	*isAimPracticePack;

	gentity_t	*aimPracticeEntBeingUsed;
	enum {
		AIMPRACTICEMODE_NONE = 0,
		AIMPRACTICEMODE_UNTIMED,
		AIMPRACTICEMODE_TIMED
	} aimPracticeMode;
	int			numAimPracticeSpawns;
	int			numTotalAimPracticeHits;
	int			numAimPracticeHitsOfWeapon[WP_NUM_WEAPONS];
	int			aimPracticeStartTime;
};

#define DAMAGEREDIRECT_HEAD		1
#define DAMAGEREDIRECT_RLEG		2
#define DAMAGEREDIRECT_LLEG		3

enum {
	CON_DISCONNECTED,
	CON_CONNECTING,
	CON_CONNECTED
};
typedef int clientConnected_t;

typedef enum {
	SPECTATOR_NOT,
	SPECTATOR_FREE,
	SPECTATOR_FOLLOW,
	SPECTATOR_SCOREBOARD
} spectatorState_t;

typedef enum {
	TEAM_BEGIN,		// Beginning a team game, spawn at base
	TEAM_ACTIVE		// Now actively playing
} playerTeamStateState_t;

// do NOT use level.time with this structure, use the helper functions instead
typedef struct {
	int startTime;
	int startLag;
} accurateTimer;

typedef struct {
	playerTeamStateState_t	state;

	int			location;

	//special stats

	int			lasthurtcarrier;
	int			lastreturnedflag;
	accurateTimer flagsince;
	int			lastfraggedcarrier;
} playerTeamState_t;

// the auto following clients don't follow a specific client
// number, but instead follow the first two active players
#define	FOLLOW_ACTIVE1	-1
#define	FOLLOW_ACTIVE2	-2

#define MAX_ACCOUNTNAME_LEN		24

typedef struct {
	node_t		node;
	int			accountId;
	char		sex[32];
	char		country[32];
} autoLink_t;

typedef struct {
	int			first;
	int			second;
	int			third;
	int			avoid;
} positionPreferences_t;

typedef struct {
	int id;
	char name[MAX_ACCOUNTNAME_LEN];
	int creationDate;
	struct {
		char sex[32];
		char country[32];
	} autoLink;
	positionPreferences_t expressedPref, validPref;
	int flags;
} account_t;

#define MAX_SESSIONINFO_LEN		256

// 64 bits for now - expand to 128 if collisions occur
typedef long long sessionInfoHash_t;

#define ACCOUNT_ID_UNLINKED		-1

typedef struct {
	int id;
	sessionInfoHash_t hash;
	char info[MAX_SESSIONINFO_LEN];
	int accountId;
} session_t;

typedef struct {
	account_t* ptr;
	qboolean online;
} accountReference_t;

typedef struct {
	session_t* ptr;
	qboolean online;
} sessionReference_t;

// race stuff
#define SV_MATCHID_LEN		17 // 2 concatenated hex 4 bytes ints, so 2*8 chars + NULL

typedef struct raceRecordInfo_s {
	// demo info
	char	matchId[SV_MATCHID_LEN];	// used to link to the game on demoarchive
	char	playerName[MAX_NETNAME];	// in-game name used when capturing
	int		clientId;					// in-game client id used when capturing
	int		pickupTime;					// level.time when flag was picked up
	team_t	whoseFlag;					// the team that owns the flag that was captured

	// additional statistics
	int		maxSpeed;					// max speed in ups
	int		avgSpeed;					// average speed in ups
} raceRecordInfo_t;

typedef struct raceRecord_s {
	int					time;	// capture time in ms
	time_t				date;	// epoch time of the record (seconds)
	raceRecordInfo_t	extra;	// other stuff here is stored as optional "extra" information
} raceRecord_t;

typedef enum raceType_e {
	RACE_TYPE_STANDARD = 0,	// restrictive category from which the other rules derivate
	RACE_TYPE_WEAPONS,		// self weapon damage is allowed (except dets/mines)

	NUM_RACE_TYPES,
	RACE_TYPE_INVALID
} raceType_t;

typedef struct aimRecordInfo_s {
	// demo info
	char	matchId[SV_MATCHID_LEN];	// used to link to the game on demoarchive
	char	playerName[MAX_NETNAME];	// in-game name used when capturing
	int		clientId;					// in-game client id used when capturing
	int		startTime;					// level.time when practice was started

	// additional statistics
	int		numHitsOfWeapon[WP_NUM_WEAPONS];
} aimRecordInfo_t;

typedef struct aimRecord_s {
	int					time;	// score
	time_t				date;	// epoch time of the record (seconds)
	aimRecordInfo_t	extra;	// other stuff here is stored as optional "extra" information
} aimRecord_t;

// client data that stays across multiple levels or tournament restarts
// this is achieved by writing all the data to cvar strings at game shutdown
// time and reading them back at connection time.  Anything added here
// MUST be dealt with in G_InitSessionData() / G_ReadSessionData() / G_WriteSessionData()
// Sil - This structure no longer needs to be maintained in G_InitSessionData(), G_ReadSessionData(), 
// G_WriteSessionData(), because whole content is stored in binary file as it is, and then restored.
// Make sure that content stored in this structure has to be serializable, in other words do not
// store pointers here.
typedef struct {
	team_t		sessionTeam;
	int			spectatorTime;		// for determining next-in-line to play
	spectatorState_t	spectatorState;
	int			spectatorClient;	// for chasecam and follow mode
	int			wins, losses;		// tournament stats
	int			selectedFP;			// check against this, if doesn't match value in playerstate then update userinfo
	int			saberLevel;			// similar to above method, but for current saber attack level
	qboolean	setForce;			// set to true once player is given the chance to set force powers
	int			updateUITime;		// only update userinfo for FP/SL if < level.time
	qboolean	teamLeader;			// true when this client is a team leader
	char		siegeClass[64];
	char		saberType[64];
	char		saber2Type[64];
	int			duelTeam;
	int			siegeDesiredTeam;

	//we should add ip here
	/* *CHANGE 82* i know its redudant to store both ip integer and ip string,
	but we need both many times in code (string for logging, integer for fast comparing in protection)
	so i think its effective ho compute both versions only once
	*/
	char		ipString[24];
	unsigned int ip;
    int         port;
	qboolean    isInkognito;
	unsigned	ignoreFlags;
    int			inactivityTime;
    //int         sessionId;
    int         nameChangeTime;

	qboolean	shadowMuted;

	qboolean	canJoin; // Passwordless clients
	qboolean	whTrustToggle; // in whitelist mode, qtrue = trusted, in blacklist mode, qtrue = untrusted

	// racemode stuff
	qboolean	inRacemode;
	int			racemodeFlags;
	qboolean	seeRacersWhileIngame; // for in game clients, separate from RMF_HIDERACERS because the meaning is reversed
	qboolean	seeAimBotsWhileIngame; // for in game clients, separate from RMF_HIDEBOTS because the meaning is reversed
	vec3_t		telemarkOrigin;
	float		telemarkYawAngle;
	float		telemarkPitchAngle;

	// this is only used in case a sql error happens so that the player won't overwrite their own times.
	// they should still be able to race, but no actual change is made in db
	qboolean canSubmitRaceTimes;
	// cached personal best times, tied to SESSION. Used to efficiently determine if changes should be
	// made to the db after a record is submitted. Unset records have a time of 0.
	// it is expected that this cache is maintained on record updates
	int cachedSessionRaceTimes[NUM_RACE_TYPES];

	qboolean canSubmitAimTimes;

	// account system
	int sessionCacheNum;
	int accountCacheNum;

	char        username[MAX_USERNAME_SIZE];

#ifdef NEWMOD_SUPPORT
	enum {
		PENDING = 0,
		CLANNOUNCE,
		CLAUTH,
		AUTHENTICATED,
		INVALID
	} auth;
	char		cuidHash[CRYPTO_HASH_HEX_SIZE]; // hash of the client cuid
	int			serverKeys[2]; // randomly generated auth keys to confirm legit clients
	char		nmVer[16];
#endif

#define UNLAGGED_CLIENTINFO		(1 << 0)
#define UNLAGGED_COMMAND		(1 << 1)
	int		unlagged;
	qboolean	basementNeckbeardsTriggered;

	char		country[128];
	int			qport;

	int			clAnnounceSendTime;

	enum {
		CLIENT_TYPE_NORMAL = 0,
		CLIENT_TYPE_JKCHAT
	} clientType;

	struct {
		qboolean	valid;
		int			pos;
		int			score;
	} remindPositionOnMapChange;
} clientSession_t;

// race flags
#define	RMF_HIDERACERS			(1 << 0)	// hides other racers from the racer this is set for
#define RMF_HIDEINGAME			(1 << 1)	// hides in game entities to the racer this is set for
#define RMF_DONTTELEWITHSPEED	(1 << 2)	// don't auto activates spee when using telemarks
#define RMF_ALREADYJOINED		(1 << 3)	// not set the first time entering /race, and set every other time
#define RMF_HIDEBOTS			(1 << 4)	// hides bots while in racemode (for fastcap tryhards)

// playerstate mGameFlags
#define	PSG_VOTED				(1<<0)		// already cast a vote
#define PSG_TEAMVOTED			(1<<1)		// already cast a team vote
#define PSG_CANVOTE			    (1<<2)		// this player can vote

//
#define	MAX_VOTE_COUNT		3

#define MAX_MAP_POOL_ID 20
#define MAX_MAP_POOL_LONGNAME 64
#define MAX_MAP_NAME 32
#define MAX_MAPS_IN_POOL 64
#define MAX_POOLS_TEXT 8192

typedef struct
{
	char id[MAX_MAP_POOL_ID];
	char longname[MAX_MAP_POOL_LONGNAME];
	char maplist[MAX_MAPS_IN_POOL][MAX_MAP_NAME];
	int  mapsCount;
} MapPool;

// client data that stays across multiple respawns, but is cleared
// on each level change or team change at ClientBegin()
typedef struct {
	clientConnected_t	connected;	
	usercmd_t	cmd;				// we would lose angles if not persistant
	qboolean	localClient;		// true if "ip" info key is "localhost"
	qboolean	initialSpawn;		// the first spawn should be at a cool location
	qboolean	predictItemPickup;	// based on cg_predictItems userinfo
	qboolean	pmoveFixed;			//
	char		netname[MAX_NETNAME];
	int			netnameTime;				// Last time the name was changed
	int			maxHealth;			// for handicapping
	int			enterTime;			// level.time the client entered the game
	playerTeamState_t teamState;	// status in teamplay games
	int			voteCount;			// to prevent people from constantly calling votes
	int			teamVoteCount;		// to prevent people from constantly calling votes
	qboolean	teamInfo;			// send team overlay updates?
	qboolean	ready;
	int			readyTime;

    // info maintained for inactivity detection
    usercmd_t	lastCmd;

	// *CHANGE 8a* added clientNum to persistant data

	int			clientNum;
	int         botAvgPing;

	//special chat limit variables
	int         chatSentCount;
	int         chatSentTime;
	int			teamChatSentCount;
	int			teamChatSentTime;
	int			voiceChatSentCount;
	int			voiceChatSentTime;

	// force stats
	int			protsince;
	int			ragesince;

	// speed stats
	float		fastcapTopSpeed;
	float		fastcapDisplacement;
	int			fastcapDisplacementSamples;

	// other racemode stuff
	int			flagTakeTime; // used to get the steal time for demos independently from level.blue/redFlagStealTime
	int			flagDebounceTime; // used not to pickup a flag immediately after finishing a run
	int			checkpointDebounceTime; // used to not spam checkpoint prints
	int			raceTeleportTime; // for anti-teleport spam
	int			raceTeleportFrame; // for anti-teleport spam
	int			raceTeleportCount; // for anti-teleport spam

	qboolean	hasDoneSomething; // for auto AFK detection

	int			lastInputTime;

	gentity_t	*lastSpawnPoint;
	int			lastSpawnTime;
	gentity_t	*lastKiller;

	char		chatBuffer[MAX_SAY_TEXT];
	int			chatBufferCheckTime;

	char		specChatBuffer[MAX_SAY_TEXT];
	int			specChatBufferCheckTime;

	qboolean	killedAlliedFlagCarrier;

	aimPracticePack_t	*aimPracticePackBeingEdited; // which pack this player is currently editing (if any)

	int			cointossHeadsTime;
	int			cointossTailsTime;

	qboolean	barredFromPugSelection;

	qboolean permaBarredDeclaredPickable;

} clientPersistant_t;

typedef struct renderInfo_s
{
	//In whole degrees, How far to let the different model parts yaw and pitch
	int		headYawRangeLeft;
	int		headYawRangeRight;
	int		headPitchRangeUp;
	int		headPitchRangeDown;

	int		torsoYawRangeLeft;
	int		torsoYawRangeRight;
	int		torsoPitchRangeUp;
	int		torsoPitchRangeDown;

	int		legsFrame;
	int		torsoFrame;

	float	legsFpsMod;
	float	torsoFpsMod;

	//Fields to apply to entire model set, individual model's equivalents will modify this value
	vec3_t	customRGB;//Red Green Blue, 0 = don't apply
	int		customAlpha;//Alpha to apply, 0 = none?

	//RF?
	int			renderFlags;

	//
	vec3_t		muzzlePoint;
	vec3_t		muzzleDir;
	vec3_t		muzzlePointOld;
	vec3_t		muzzleDirOld;
	int			mPCalcTime;//Last time muzzle point was calced

	//
	float		lockYaw;//

	//
	vec3_t		headPoint;//Where your tag_head is
	vec3_t		headAngles;//where the tag_head in the torso is pointing
	vec3_t		handRPoint;//where your right hand is
	vec3_t		handLPoint;//where your left hand is
	vec3_t		crotchPoint;//Where your crotch is
	vec3_t		footRPoint;//where your right hand is
	vec3_t		footLPoint;//where your left hand is
	vec3_t		torsoPoint;//Where your chest is
	vec3_t		torsoAngles;//Where the chest is pointing
	vec3_t		eyePoint;//Where your eyes are
	vec3_t		eyeAngles;//Where your eyes face
	int			lookTarget;//Which ent to look at with lookAngles
	lookMode_t	lookMode;
	int			lookTargetClearTime;//Time to clear the lookTarget
	int			lastVoiceVolume;//Last frame's voice volume
	vec3_t		lastHeadAngles;//Last headAngles, NOT actual facing of head model
	vec3_t		headBobAngles;//headAngle offsets
	vec3_t		targetHeadBobAngles;//head bob angles will try to get to targetHeadBobAngles
	int			lookingDebounceTime;//When we can stop using head looking angle behavior
	float		legsYaw;//yaw angle your legs are actually rendering at

	//for tracking legitimate bolt indecies
	void		*lastG2; //if it doesn't match ent->ghoul2, the bolts are considered invalid.
	int			headBolt;
	int			handRBolt;
	int			handLBolt;
	int			torsoBolt;
	int			crotchBolt;
	int			footRBolt;
	int			footLBolt;
	int			motionBolt;

	int			boltValidityTime;
} renderInfo_t;

void G_StoreTrail(gentity_t *ent);
void G_ResetTrail(gentity_t *ent);
void G_TimeShiftClient(gentity_t *ent, int time, qboolean timeshiftAnims);
void G_TimeShiftAllClients(int time, gentity_t *skip, qboolean timeshiftAnims);
void G_UnTimeShiftClient(gentity_t *ent, qboolean timeshiftAnims);
void G_UnTimeShiftAllClients(gentity_t *skip, qboolean timeshiftAnims);
void G_PredictPlayerStepSlideMove(gentity_t *ent, float frametime);

//NT - client origin trails
typedef struct { //Should this store their g2 anim? for proper g2 sync?
	vec3_t	mins, maxs;
	vec3_t	currentOrigin;//, currentAngles; //Well r.currentAngles are never actually used by clients in this game?
	int		time, torsoAnim, torsoTimer, legsAnim, legsTimer;
	float	realAngle; //Only the [YAW] is ever used for hit detection
} clientTrail_t;

//
// g_stats.c
//

//#define DEBUG_CTF_POSITION_STATS // uncomment this to remove afk checks and print more messages for pos detection
//#define DEBUGSTATSNAMES // uncomment this to see longer column names in stats

#ifdef DEBUG_CTF_POSITION_STATS
#define CTFPOSITION_MINIMUM_SECONDS						(60) // 60 seconds minimum for pos detection
#define CTFPOSITION_MINIMUM_SECONDS_LATEGAME				(60) // 60 seconds minimum for pos detection
#define CTFPOSITION_LATEGAME_THRESHOLD_MILLISECONDS		(600000) // start using the lategame minimum after this long
#define CTFPOSITION_NOSWAPS_THRESHOLD_MILLISECONDS		(1200000) // don't detect any swaps after this long
#else
#define CTFPOSITION_MINIMUM_SECONDS						(120) // 120 seconds minimum for pos detection
#define CTFPOSITION_MINIMUM_SECONDS_LATEGAME				(240) // 240 seconds minimum for pos detection after a certain point in the game
#define CTFPOSITION_LATEGAME_THRESHOLD_MILLISECONDS		(600000) // start using the lategame minimum after this long
#define CTFPOSITION_NOSWAPS_THRESHOLD_MILLISECONDS		(1200000) // don't detect any swaps after this long
#endif

#ifdef DEBUG_CTF_POSITION_STATS
#define STATS_BLOCK_DURATION_MS			(120 * 1000) // 2 minute blocks
#else
#define STATS_BLOCK_DURATION_MS			(300 * 1000) // 5 minute blocks
#endif

#define CTF_SAVE_DISTANCE_THRESHOLD		(200)
#define CTFPOS_POSTSPAWN_DELAY_MS		(3000) // wait a little while after spawning before we log position data

typedef enum {
	CTFREGION_INVALID = -1,
	CTFREGION_FLAGSTAND = 0,
	CTFREGION_BASE,
	CTFREGION_MID,
	CTFREGION_ENEMYBASE,
	CTFREGION_ENEMYFLAGSTAND,
	NUM_CTFREGIONS
} ctfRegion_t;

typedef enum {
	MODC_INVALID = -1,
	MODC_FIRST = 0,
	MODC_MELEESTUNBATON = MODC_FIRST,
	MODC_SABER,
	MODC_PISTOL,
	MODC_BLASTER,
	MODC_DISRUPTOR,
	MODC_BOWCASTER,
	MODC_REPEATER,
	MODC_DEMP,
	MODC_GOLAN,
	MODC_ROCKET,
	MODC_CONCUSSION,
	MODC_THERMAL,
	MODC_MINE,
	MODC_DETPACK,
	MODC_FORCE,
	MODC_ALL_TYPES_COMBINED,
	MODC_MAX
} meansOfDeathCategory_t;

typedef enum {
	ACC_INVALID = -1,
	ACC_FIRST = 0,
	ACC_ALL_TYPES_COMBINED = ACC_FIRST,
	ACC_PISTOL_ALT,
	ACC_DISRUPTOR_PRIMARY,
	ACC_DISRUPTOR_SNIPE,
	ACC_REPEATER_ALT,
	ACC_GOLAN_ALT,
	ACC_ROCKET,
	ACC_CONCUSSION_PRIMARY,
	ACC_CONCUSSION_ALT,
	ACC_THERMAL_ALT,
	ACC_MAX
} accuracyCategory_t;

typedef enum {
	CTFPOSITION_UNKNOWN = 0,
	CTFPOSITION_BASE,
	CTFPOSITION_CHASE,
	CTFPOSITION_OFFENSE
} ctfPosition_t;

#define ALL_CTF_POSITIONS	((1 << CTFPOSITION_BASE) | (1 << CTFPOSITION_CHASE) | (1 << CTFPOSITION_OFFENSE))

typedef enum {
	STATS_TABLE_FIRST = 0,
	STATS_TABLE_GENERAL = STATS_TABLE_FIRST,
	STATS_TABLE_FORCE,
	STATS_TABLE_DAMAGE,
	STATS_TABLE_WEAPON_GIVEN,
	STATS_TABLE_WEAPON_TAKEN,
	STATS_TABLE_ACCURACY,
	NUM_STATS_TABLES
} statsTableType_t;
void InitClientStats(gclient_t *cl);
void ChangeStatsTeam(gclient_t *cl);
ctfRegion_t GetCTFRegion(gentity_t *ent);
meansOfDeathCategory_t MeansOfDeathCategoryForMeansOfDeath(meansOfDeath_t mod);
accuracyCategory_t AccuracyCategoryForProjectile(gentity_t *projectile);
void ChangeToNextStatsBlockIfNeeded(void);
char *NameForPos(ctfPosition_t pos);
void SendMachineFriendlyStats(void);
void RecalculateTeamBalance(void);

typedef struct {
	node_t		node;

	int			blockNum;

	qboolean	isBot;
	qboolean	isTotal; // used to denote that this is the total stat for a team (for display purposes)
	int			sessionId; // for bots, this is just their client number. replacing a bot with another bot in the same slot = same stats
	int			accountId; // if applicable
	char		accountName[MAX_NAME_LENGTH]; // if applicable
	char		name[MAX_NAME_LENGTH];
	int			clientNum; // to help print in scoreboard order
	team_t		lastTeam; // upon joining red/blue even once, this will never be set back to spectator/free (accounts for people who go spec after playing). initializes to free

	int			score;
	int			captures;
	int			assists;
	int			defends;
	int			accuracy_shots;
	int			accuracy_hits;
	int			accuracy_shotsOfType[ACC_MAX];
	int			accuracy_hitsOfType[ACC_MAX];
	int			accuracy; // this is only calculated on demand; don't just randomly read this (imagine getters in C)
	int			accuracyOfType[ACC_MAX]; // this is only calculated on demand; don't just randomly read this (imagine getters in C)
	int			airs;
	int			teamKills;
	int			takes;
	int			pits;
	int			pitted;
	int			fcKills;
	int			fcKillsResultingInRets;
	int			fcKillEfficiency; // this is only calculated on demand; don't just randomly read this (imagine getters in C)
	int			rets;
	int			selfkills;
	int			boonPickups;
	int			healthPickedUp;
	int			armorPickedUp;
	int			totalFlagHold;
	int			longestFlagHold;
	int			saves;
	int			damageDealtTotal;
	int			damageTakenTotal;
	int			flagCarrierDamageDealtTotal; // damage given to flag carriers
	int			flagCarrierDamageTakenTotal; // damage received while carrying a flag
	int			clearDamageDealtTotal; // damage given to non-flag carriers in your base/fs
	int			clearDamageTakenTotal; // damage taken while not a flag carrier and in the enemy base/fs
	int			otherDamageDealtTotal; // damage given not fitting into either of the two prior categories
	int			otherDamageTakenTotal; // damage taken not fitting into either of the two prior categories
	int			damageOfType[MODC_MAX]; // only used for total rows; not players

	float		topSpeed;
	float		displacement;
	int			displacementSamples;
	int			averageSpeed; // this is only calculated on demand; don't just randomly read this (imagine getters in C)

	list_t		damageGivenList; // list of people whom this player has damaged (including ragequitters/specs)
	list_t		damageTakenList; // list of people by whom this player has been damaged (including ragequitters/specs)

	int			push;
	int			pull;
	int			healed;
	int			numEnergizes;
	float		normalizedEnergizeAmounts; // e.g. two 90% efficient energizes = 1.8
	int			energizeEfficiency; // this is only calculated on demand; don't just randomly read this (imagine getters in C)
	int			energizedAlly;
	int			gotEnergizedByAlly;
	int			energizedEnemy;
	int			absorbed;
	int			protDamageAvoided;
	int			protTimeUsed;
	int			rageTimeUsed;
	int			drain;
	int			gotDrained;

	list_t		teammatePositioningList; // record of positioning for people i was ingame with

	int			numFlagHolds;
	int			averageFlagHold; // this is only calculated on demand; don't just randomly read this (imagine getters in C)

	int			numGets;
	int			getTotalHealth;
	int			averageGetHealth; // this is only calculated on demand; don't just randomly read this (imagine getters in C)

	int			regionTime[NUM_CTFREGIONS];
	int			regionPercent[NUM_CTFREGIONS];
	float		totalLocationWithFlag; // only counts 3+ seconds after spawning
	int			numLocationSamplesWithFlag; // only counts 3+ seconds after spawning
	float		totalLocationWithoutFlag; // only counts 3+ seconds after spawning
	int			numLocationSamplesWithoutFlag; // only counts 3+ seconds after spawning
	int			ticksNotPaused; // all ticks this player has been ingame for while not paused, regardless of afk/spawn times, etc. used to determine total ingame time
	int			numLocationSamplesRegardlessOfFlagHolding; // similar to ticksNotPaused but they can't be afk and only counts 3+ seconds after spawn
	int			confirmedPositionBits; // list of all positions this person has ever played in this pug on this team

	ctfPosition_t	lastPosition; // may be valid or unknown
	ctfPosition_t	finalPosition; // set only when confirmed; overrides everything else if set

	int			lastTickIngameTime; // the last time they were ingame and it wasn't paused
} stats_t;

typedef struct {
	node_t		node;
	stats_t		*stats;
	int			numTicksIngameWithMe; // all ticks this player has been ingame with me, regardless of afk/spawn times, etc.
	int			numLocationSamplesIngameWithMe; // can't be afk and only counts 3+ seconds after spawn
	float		totalLocationWithFlagWithMe;
	int			numLocationSamplesWithFlagWithMe;
	float		totalLocationWithoutFlagWithMe;
	int			numLocationSamplesWithoutFlagWithMe;
} ctfPositioningData_t;
void Stats_Print(gentity_t *ent, const char *type, char *outputBuffer, size_t outSize, qboolean announce, stats_t *weaponStatsPtr);
qboolean StatsValid(const stats_t *stats);
stats_t *GetStatsFromString(const char *str);
int *GetDamageGivenStat(stats_t *attacker, stats_t *victim);
int GetTotalDamageGivenStatOfType(stats_t *attacker, stats_t *victim, meansOfDeathCategory_t modc);
int *GetDamageGivenStatOfType(stats_t *attacker, stats_t *victim, meansOfDeathCategory_t modc);
int *GetDamageTakenStat(stats_t *attacker, stats_t *victim);
int GetTotalDamageTakenStatOfType(stats_t *attacker, stats_t *victim, meansOfDeathCategory_t modc);
int *GetDamageTakenStatOfType(stats_t *attacker, stats_t *victim, meansOfDeathCategory_t modc);
ctfPosition_t DetermineCTFPosition(stats_t *posGuy, qboolean enableDebugPrints);
void FinalizeCTFPositions(void);
void CheckAccountsOfOldBlocks(int ignoreBlockNum);

typedef struct {
	node_t		node;
	qboolean	otherPlayerIsBot;
	int			otherPlayerSessionId;
	int			otherPlayerAccountId;
	stats_t		*otherPlayerStats;
	int			totalAmount;
	int			ofType[MODC_MAX];
} damageCounter_t;

//
// g_teamgen.c
//
#define TEAMGEN_CHAT_COMMAND_CHARACTER		'?'

typedef struct {
	double rawStrength;
	double relativeStrength;
	int baseId;
	int chaseId;
	int offenseId1;
	int offenseId2;
	char baseName[MAX_NAME_LENGTH];
	char chaseName[MAX_NAME_LENGTH];
	char offense1Name[MAX_NAME_LENGTH];
	char offense2Name[MAX_NAME_LENGTH];
} teamData_t;

typedef struct {
	qboolean valid;
	double diff;
	XXH32_hash_t hash;
	teamData_t teams[2];
	int numOnPreferredPos;
	int numOnAvoidedPos;
	int topTierImbalance;
	int bottomTierImbalance;
	int totalNumPermutations;
} permutationOfTeams_t;

typedef struct {
	int clientNum;
	int accountId;
	char accountName[32];
	positionPreferences_t posPrefs;
	team_t team;
} sortedClient_t;

typedef struct {
	node_t		node;
	int			accountId;
} barredOrForcedPickablePlayer_t;

typedef struct {
	node_t			node;
	int				num;
	XXH32_hash_t	hash;
	sortedClient_t	clients[MAX_CLIENTS];
	int				votedYesClients;
	int				votedToRerollClients;
	int				votedToCancelClients;
	qboolean		passed;
	permutationOfTeams_t suggested, highestCaliber, fairest, desired, inclusive;
	uint64_t		numValidPermutationsChecked;
	char			namesStr[1024];
	char			suggestedLetter, highestCaliberLetter, fairestLetter, desiredLetter, inclusiveLetter;
	int				suggestedVoteClients, highestCaliberVoteClients, fairestVoteClients, desiredVoteClients, inclusiveVoteClients;
	list_t			avoidedHashesList;
} pugProposal_t;

typedef struct {
	node_t			node;
	char			*text;
	qboolean		inConsole;
	int				clientNum;
	int				serverFrameNum;
} queuedServerMessage_t;

typedef enum {
	PLAYERRATING_UNRATED = 0,
	PLAYERRATING_MID_D,
	PLAYERRATING_HIGH_D,
	PLAYERRATING_LOW_C,
	PLAYERRATING_MID_C,
	PLAYERRATING_HIGH_C,
	PLAYERRATING_LOW_B,
	PLAYERRATING_MID_B,
	PLAYERRATING_HIGH_B,
	PLAYERRATING_LOW_A,
	PLAYERRATING_MID_A,
	PLAYERRATING_HIGH_A,
	PLAYERRATING_S,
	NUM_PLAYERRATINGS
} ctfPlayerTier_t;
char *PlayerRatingToString(ctfPlayerTier_t tier);

double PlayerTierToRating(ctfPlayerTier_t tier);
qboolean TeamGenerator_PugStart(gentity_t *ent, char **newMessage);
void TeamGenerator_DoReroll(qboolean forcedByServer);
qboolean TeamGenerator_VoteToReroll(gentity_t *ent, char **newMessage);
void TeamGenerator_DoCancel(void);
qboolean TeamGenerator_VoteToCancel(gentity_t *ent, char **newMessage);
qboolean TeamGenerator_VoteForTeamPermutations(gentity_t *ent, const char *voteStr, char **newMessage);
qboolean TeamGenerator_VoteYesToPugProposal(gentity_t *ent, int num, pugProposal_t *setOptional, char **newMessage);
void TeamGenerator_QueueServerMessageInChat(int clientNum, const char *msg);
void TeamGenerator_QueueServerMessageInConsole(int clientNum, const char *msg);
qboolean TeamGenerator_CheckForChatCommand(gentity_t *ent, const char *s, char **newMessage);
qboolean TeamGenerator_PlayerIsBarredFromTeamGenerator(gentity_t *ent);
void Svcmd_Pug_f(void);
void TeamGen_Initialize(void);
ctfPlayerTier_t GetPlayerTierForPlayerOnPosition(int accountId, ctfPosition_t pos, qboolean assumeLowTierIfUnrated);
void ShowSubBalance(void);
qboolean TeamGenerator_PlayerIsPermaBarredButTemporarilyForcedPickable(gentity_t *ent);
void TeamGen_ClearRemindPositions(void);
void TeamGen_RemindPosition(gentity_t *ent);
void TeamGen_AnnounceBreak(void);
void TeamGen_DoAutoRestart(void);

// this structure is cleared on each ClientSpawn(),
// except for 'client->pers' and 'client->sess'
struct gclient_s {
	// ps MUST be the first element, because the server expects it
	playerState_t	ps;				// communicated by server to clients

	// the rest of the structure is private to game
	clientPersistant_t	pers;
	clientSession_t		sess;

	// accounts
	session_t* session;
	account_t* account;
	stats_t* stats;

	saberInfo_t	saber[MAX_SABERS];
	void		*weaponGhoul2[MAX_SABERS];

	int			tossableItemDebounce;

	int			bodyGrabTime;
	int			bodyGrabIndex;

	int			pushEffectTime;

	int			invulnerableTimer;

	int			saberCycleQueue;

	int			legsAnimExecute;
	int			torsoAnimExecute;
	qboolean	legsLastFlip;
	qboolean	torsoLastFlip;

	qboolean	readyToExit;		// wishes to leave the intermission

	qboolean	noclip;

	int			lastCmdTime;		// level.time of last usercmd_t, for EF_CONNECTION
									// we can't just use pers.lastCommand.time, because
									// of the g_sycronousclients case
	int			lastRealCmdTime;
	int			buttons;
	int			oldbuttons;
	int			latched_buttons;

	vec3_t		oldOrigin;

	// sum up damage over an entire frame, so
	// shotgun blasts give a single big kick
	int			damage_armor;		// damage absorbed by armor
	int			damage_blood;		// damage taken out of health
	int			damage_knockback;	// impact damage
	vec3_t		damage_from;		// origin for vector calculation
	qboolean	damage_fromWorld;	// if true, don't use the damage_from vector

	int			damageBoxHandle_Head; //entity number of head damage box
	int			damageBoxHandle_RLeg; //entity number of right leg damage box
	int			damageBoxHandle_LLeg; //entity number of left leg damage box

	int			accurateCount;		// for "impressive" reward sound

	//
	int			lastkilled_client;	// last client that this client killed
	int			lasthurt_client;	// last client that damaged this client
	int			lasthurt_mod;		// type of damage the client did

	// timers
	int			respawnTime;		// can respawn when time > this, force after g_forcerespwan
	qboolean	inactivityWarning;	// qtrue if the five seoond warning has been given
	int			rewardTime;			// clear the EF_AWARD_IMPRESSIVE, etc when time > this

	int			airOutTime;

	int			lastKillTime;		// for multiple kill rewards

	qboolean	fireHeld;			// used for hook
	gentity_t	*hook;				// grapple hook if out

	int			switchTeamTime;		// time the player switched teams

	int			switchDuelTeamTime;		// time the player switched duel teams

	int			switchClassTime;	// class changed debounce timer

	// timeResidual is used to handle events that happen every second
	// like health / armor countdowns and regeneration
	int			timeResidual;

	char		*areabits;

	int			g2LastSurfaceHit; //index of surface hit during the most recent ghoul2 collision performed on this client.
	int			g2LastSurfaceTime; //time when the surface index was set (to make sure it's up to date)

	int			corrTime;

	vec3_t		lastHeadAngles;
	int			lookTime;

	int			brokenLimbs;

	qboolean	noCorpse; //don't leave a corpse on respawn this time.

	int			jetPackTime;

	qboolean	jetPackOn;
	int			jetPackToggleTime;
	int			jetPackDebRecharge;
	int			jetPackDebReduce;

	int			cloakToggleTime;
	int			cloakDebRecharge;
	int			cloakDebReduce;

	int			saberStoredIndex; //stores saberEntityNum from playerstate for when it's set to 0 (indicating saber was knocked out of the air)

	int			saberKnockedTime; //if saber gets knocked away, can't pull it back until this value is < level.time

	vec3_t		olderSaberBase; //Set before lastSaberBase_Always, to whatever lastSaberBase_Always was previously
	qboolean	olderIsValid;	//is it valid?

	vec3_t		lastSaberDir_Always; //every getboltmatrix, set to saber dir
	vec3_t		lastSaberBase_Always; //every getboltmatrix, set to saber base
	int			lastSaberStorageTime; //server time that the above two values were updated (for making sure they aren't out of date)

	qboolean	hasCurrentPosition;	//are lastSaberTip and lastSaberBase valid?

	int			dangerTime;		// level.time when last attack occured

	int			idleTime;		//keep track of when to play an idle anim on the client.

	int			idleHealth;		//stop idling if health decreases
	vec3_t		idleViewAngles;	//stop idling if viewangles change

	int			forcePowerSoundDebounce; //if > level.time, don't do certain sound events again (drain sound, absorb sound, etc)

	char		modelname[MAX_QPATH];

	qboolean	fjDidJump;

	qboolean	ikStatus;

	int			throwingIndex;
	int			beingThrown;
	int			doingThrow;

	float		hiddenDist;//How close ents have to be to pick you up as an enemy
	vec3_t		hiddenDir;//Normalized direction in which NPCs can't see you (you are hidden)

	renderInfo_t	renderInfo;

	//mostly NPC stuff:
	npcteam_t	playerTeam;
	npcteam_t	enemyTeam;
	char		*squadname;
	gentity_t	*team_leader;
	gentity_t	*leader;
	gentity_t	*follower;
	int			numFollowers;
	gentity_t	*formationGoal;
	int			nextFormGoal;
	class_t		NPC_class;

	vec3_t		pushVec;
	int			pushVecTime;

	int			siegeClass;
	int			holdingObjectiveItem;

	//time values for when being healed/supplied by supplier class
	int			isMedHealed;
	int			isMedSupplied;

	//seperate debounce time for refilling someone's ammo as a supplier
	int			medSupplyDebounce;

	//used in conjunction with ps.hackingTime
	int			isHacking;
	vec3_t		hackingAngles;

	//debounce time for sending extended siege data to certain classes
	int			siegeEDataSend;

	int			ewebIndex; //index of e-web gun if spawned
	int			ewebTime; //e-web use debounce
	int			ewebHealth; //health of e-web (to keep track between deployments)

	int			inSpaceIndex; //ent index of space trigger if inside one
	int			inSpaceSuffocation; //suffocation timer

	int			tempSpectate; //time to force spectator mode

	//keep track of last person kicked and the time so we don't hit multiple times per kick
	int			jediKickIndex;
	int			jediKickTime;

	//special moves (designed for kyle boss npc, but useable by players in mp)
	int			grappleIndex;
	int			grappleState;

	int			solidHack;

	int			noLightningTime;

	unsigned	mGameFlags;

	//voting delays
	int			lastCallvoteTime;
	int			lastJoinedTime;

	//fallen duelist
	qboolean	iAmALoser;

	int			lastGenCmd;
	int			lastGenCmdTime;

	int			preduelWeaps;

	int			pmoveMsec; // used for interpolation with accurate timers

	qboolean	usedWeapon; // triggers the weapon capture record category
	qboolean	jumpedOrCrouched; // invalidates the walk category
	qboolean	usedForwardOrBackward; // invalidates the ad category
	qboolean	runInvalid; // run completely invalidated by other means

#ifdef NEWMOD_SUPPORT
	qboolean	isLagging; // mark lagger without actually changing EF_CONNECTION
	int			realPing;
#endif

	int homingLockTime; // time at which homing weapon locked on to a target
	int homingLockTarget; // the target of it

	int lastInputTime; // based on trap_Milliseconds

	qboolean		reachedLimit;
	int				moveRecordStart;
	int				trailHead;
	aimPracticeMovementTrail_t	trail[MAX_MOVEMENT_TRAILS];

	int rockPaperScissorsChallengeTime;
	int rockPaperScissorsStartTime;
	int rockPaperScissorsBothChosenTime;
	int rockPaperScissorsOtherClientNum;
	char rockPaperScissorsChoice;

	qboolean canTouchPowerupsWhileGameIsPaused;
};

//Interest points

#define MAX_INTEREST_POINTS		64

typedef struct 
{
	vec3_t		origin;
	char		*target;
} interestPoint_t;

//Combat points

#define MAX_COMBAT_POINTS		512

typedef struct 
{
	vec3_t		origin;
	int			flags;
	qboolean	occupied;
	int			waypoint;
	int			dangerTime;
} combatPoint_t;

// Alert events

#define	MAX_ALERT_EVENTS	32

typedef enum
{
	AET_SIGHT,
	AET_SOUND
} alertEventType_e;

typedef enum
{
	AEL_MINOR,			//Enemy responds to the sound, but only by looking
	AEL_SUSPICIOUS,		//Enemy looks at the sound, and will also investigate it
	AEL_DISCOVERED,		//Enemy knows the player is around, and will actively hunt
	AEL_DANGER,			//Enemy should try to find cover
	AEL_DANGER_GREAT	//Enemy should run like hell!
} alertEventLevel_e;

typedef struct alertEvent_s
{
	vec3_t				position;	//Where the event is located
	float				radius;		//Consideration radius
	alertEventLevel_e	level;		//Priority level of the event
	alertEventType_e	type;		//Event type (sound,sight)
	gentity_t			*owner;		//Who made the sound
	float				light;		//ambient light level at point
	float				addLight;	//additional light- makes it more noticable, even in darkness
	int					ID;			//unique... if get a ridiculous number, this will repeat, but should not be a problem as it's just comparing it to your lastAlertID
	int					timestamp;	//when it was created
} alertEvent_t;

//
// this structure is cleared as each map is entered
//
typedef struct
{
	char	targetname[MAX_QPATH];
	char	target[MAX_QPATH];
	char	target2[MAX_QPATH];
	char	target3[MAX_QPATH];
	char	target4[MAX_QPATH];
	int		nodeID;
} waypointData_t;

typedef struct
{
	char	username[MAX_USERNAME_SIZE];
	int		clientNum; //for fast lookup
	int		left; //when did this person leave his team :o
} rosterData;

#define WEBHOOK_INGAME_THRESHOLD		(0.1f)
#define WEBHOOK_INGAME_THRESHOLD_SUB	(0.9f)

typedef struct {
	node_t			node;
	unsigned int	numTicks;
	int				sessionId;
	int				accountId;
	char			name[MAX_NAME_LENGTH];
	int				clientNum; // to help print in scoreboard order
	qboolean		printed;
} tickPlayer_t;

#define MAX_LOCATION_CHARS 32

typedef struct {
	char	message[MAX_LOCATION_CHARS];
	int		count;
	int		cs_index;
	vec3_t	origin;
} legacyLocation_t;

typedef struct {
	char	message[MAX_LOCATION_CHARS];
	team_t	teamowner;
	int		cs_index;
} enhancedLocation_t;

typedef enum {
	MAPTIER_INVALID = 0,
	MAPTIER_F,
	MAPTIER_C,
	MAPTIER_B,
	MAPTIER_A,
	MAPTIER_S,
	NUM_MAPTIERS
} mapTier_t;

typedef struct {
	node_t				node;
	int					accountId;
	double				rating[CTFPOSITION_OFFENSE + 1];
	qboolean			isRusty;
} playerRating_t;

typedef struct {
	node_t			node;
	int				accountId;
	ctfPosition_t	mostPlayed;
	ctfPosition_t	secondMostPlayed;
	ctfPosition_t	thirdMostPlayed;
} mostPlayedPos_t;

typedef struct {
	struct gclient_s	*clients;		// [maxclients]

	struct gentity_s	*gentities;
	int			gentitySize;
	int			num_entities;		// current number, <= MAX_GENTITIES

	int			warmupTime;			// restart match at this time

	// current map name
	char		mapname[MAX_MAP_NAME];

	fileHandle_t	logFile;
	fileHandle_t	hackLogFile;
	fileHandle_t	DBLogFile;
	fileHandle_t	rconLogFile;

	//match log
	qboolean    initialChecked;
	qboolean    initialConditionsMatch;
	rosterData	initialBlueRoster[16];
	rosterData	initialRedRoster[16];
	int			initialBlueCount;
	int			initialRedCount;

	qboolean	checkedForAFKs;
	qboolean	someoneWasAFK;

	qboolean    overtime;

	//special handle for libcurl debug

	// store latched cvars here that we want to get at often
	int			maxclients;

	int			framenum;
	int			time;					// in msec
	int			previousTime;			// so movers can back up when blocked

	qboolean	wasRestarted;

	int			startTime;				// level.time the map was started
	int			firstFrameTime;			// trap_Milliseconds() of the first G_RunFrame (after all init lag has finished)

	qboolean	autoStartPending;		// auto_start was executed; we want to restart

	int			teamScores[TEAM_NUM_TEAMS];
	int			lastTeamLocationTime;		// last time of client team location update

	qboolean	newSession;				// don't use any old session data, because
										// we changed gametype

	qboolean	restarted;				// waiting for a map_restart to fire

	int			numConnectedClients;
	int			numNonSpectatorClients;	// includes connecting clients
	int			numPlayingClients;		// connected, non-spectators
	int			sortedClients[MAX_CLIENTS];		// sorted by score
	int			follow1, follow2;		// clientNums for auto-follow spectators

	int			snd_fry;				// sound index for standing in lava

	int			snd_hack;				//hacking loop sound
    int			snd_medHealed;			//being healed by supply class
	int			snd_medSupplied;		//being supplied by supply class

	int			warmupModificationCount;	// for detecting if g_warmup is changed

	// voting state
	char		voteString[MAX_STRING_CHARS];
	char		voteDisplayString[MAX_STRING_CHARS];
	int			voteTime;				// level.time vote was called
	int			voteExecuteTime;		// time the vote is executed
	int			voteYes;
	int			voteNo;
	int			numVotingClients;		// set by fixVoters()
	int			lastVotingClient;		//for delay purposes
	unsigned long long runoffSurvivors;
	unsigned long long runoffLosers;
	qboolean	inRunoff;

	qboolean	votingGametype;
	int			votingGametypeTo;

	// team voting state
	char		teamVoteString[2][MAX_STRING_CHARS];
	int			teamVoteTime[2];		// level.time vote was called
	int			teamVoteYes[2];
	int			teamVoteNo[2];
	int			numteamVotingClients[2];// set by CalculateRanks

	// b_e multi voting
	qboolean	multiVoting; // bypass some stuff if this is true (ie, cant vote yes/no)
	qboolean	multiVoteHasWildcard; // required for different rng logic
	char		multivoteWildcardMapFileName[MAX_QPATH];
	int			multiVoteChoices;
	int			multiVotes[MAX_CLIENTS]; // the id of the choice they voted for
#define MAX_MULTIVOTE_MAPS		(9) // absolute maximum
	char		multiVoteMapChars[MAX_MULTIVOTE_MAPS + 1];
	char		multiVoteMapShortNames[MAX_MULTIVOTE_MAPS + 1][MAX_QPATH];
	char		multiVoteMapFileNames[MAX_MULTIVOTE_MAPS + 1][MAX_QPATH];
	int			mapsThatCanBeVotedBits;
	qboolean		voteAutoPassOnExpiration;

	// spawn variables
	qboolean	spawning;				// the G_Spawn*() functions are valid
	int			numSpawnVars;
	char		*spawnVars[MAX_SPAWN_VARS][2];	// key / value pairs
	int			numSpawnVarChars;
	char		spawnVarChars[MAX_SPAWN_VARS_CHARS];

	// intermission state
	int			intermissionQueued;		// intermission was qualified, but
										// wait INTERMISSION_DELAY_TIME before
										// actually going there so the last
										// frag can be watched.  Disable future
										// kills during this delay
	int			intermissiontime;		// time the intermission was started

	// *CHANGE 120* new stats, avg chase return time
	//thus we need these
	int			redFlagStealTime;
	int			blueFlagStealTime;

	// this is used to transmit the capture time to clients
	// unlike flagsince, this is the sum of the flagholds needed to bring a flag from one flagstand to another (time dropped doesn't count)
	int			redTeamRunFlaghold;
	int			blueTeamRunFlaghold;

	qboolean			allReady;       // all ready flag

	char		*changemap;
	qboolean	readyToExit;			// at least one client wants to exit
	int			exitTime;
	vec3_t		intermission_origin;	// also used for spectator spawns
	vec3_t		intermission_angle;

	int			bodyQueIndex;			// dead bodies
	gentity_t	*bodyQue[BODY_QUEUE_SIZE];
	int			portalSequence;

	alertEvent_t	alertEvents[ MAX_ALERT_EVENTS ];
	int				numAlertEvents;
	int				curAlertID;

	AIGroupInfo_t	groups[MAX_FRAME_GROUPS];

	//Interest points- squadmates automatically look at these if standing around and close to them
	interestPoint_t	interestPoints[MAX_INTEREST_POINTS];
	int			numInterestPoints;

	//Combat points- NPCs in bState BS_COMBAT_POINT will find their closest empty combat_point
	combatPoint_t	combatPoints[MAX_COMBAT_POINTS];
	int			numCombatPoints;

	//rwwRMG - added:
	int			mNumBSPInstances;
	int			mBSPInstanceDepth;
	vec3_t		mOriginAdjust;
	float		mRotationAdjust;
	char		*mTargetAdjust;

	char		mTeamFilter[MAX_QPATH];

    struct {
            int state;              //OSP: paused state of the match
            int time;
			char reason[128];
    } pause;

    /*struct
    {
        int levelId;

    } db;*/

	int wallhackTracesDone;

	// racemode
	qboolean	racemodeRecordsEnabled; // qtrue if records are cvar-enabled and gametype is CTF
	qboolean	topAimRecordsEnabled; // qtrue if records are cvar-enabled and gametype is CTF
	qboolean	racemodeRecordsReadonly; // qtrue if new times won't be recorded (non standard movement cvars for example)
	int			racemodeClientMask; // bits set to 1 = clients in racemode, cached here for hiding to several entities
	int			racemodeSpectatorMask; // bits set to 1 = clients specing a client in who is in racemode, can be combined with the mask above
	int			racemodeClientsHidingOtherRacersMask; // bits set to 1 = clients in racemode who disabled seeing other racers
	int			racemodeClientsHidingIngameMask; // bits set to 1 = clients in racemode who disabled seeing in game entities
	int			ingameClientsSeeingInRaceMask; // bits set to 1 = clients in game who enabled seeing in race entities
	int			raceSpawnWeapons; // mask of race-relevant weapons present in this level
	int			existingWeaponSpawns; // mask of all weapons present in this level
	int			raceSpawnAmmo[AMMO_MAX]; // exactly the ammo to hand out at spawn based on what's present in this level
	qboolean	raceSpawnWithArmor; // qtrue if this level has at least one shield pickup
	qboolean	mapHasConcussionRifle;

	struct {
		char cmd[MAX_STRING_CHARS];
		char cmdUnique[MAX_CLIENTS][MAX_STRING_CHARS];
		int sendUntilTime;
		int lastSentTime;
		qboolean prioritized;
	} globalCenterPrint;

	int frameStartTime; // accurate timer

	struct {
		struct {
			int num;
			legacyLocation_t data[MAX_LOCATIONS];
		} legacy;

		struct {
			qboolean usingLineOfSightLocations;
			int numUnique; // two locations with the same message make an unique location, so this is at most MAX_LOCATIONS
			uint64_t numTotal; // how many entities were parsed in total, duplicates included
			enhancedLocation_t data[MAX_LOCATIONS];
		} enhanced;

		qboolean linked;
	} locations;

	// used to keep track of the average number of humans in each team throughout the pug
	unsigned int	numRedPlayerTicks;
	unsigned int	numBluePlayerTicks;
	unsigned int	numTeamTicks;

	list_t			redPlayerTickList;
	list_t			bluePlayerTickList;
	list_t			disconnectedPlayerList;

	list_t			statsList;
	list_t			savedStatsList;
	stats_t			npcStatsDummy; // so we don't have to spam `if (client->stats)` everywhere before setting stats, just have all NPCs share one stats pointer

	list_t			cachedWinrates;
	list_t			cachedPerMapWinrates;
	list_t			cachedPositionStats;
	list_t			cachedPositionStatsRaw;
	list_t			info_b_e_locationsList;

#ifdef NEWMOD_SUPPORT
	qboolean nmAuthEnabled;
	publicKey_t publicKey;
	secretKey_t secretKey;
#endif

	struct {
		int				trailHead;
#define MAX_UNLAGGED_TRAILS	(1000)
		clientTrail_t	trail[MAX_UNLAGGED_TRAILS];
		clientTrail_t	saved; // used to restore after time shift
	} unlagged[MAX_GENTITIES];
	int		lastThinkRealTime[MAX_GENTITIES];

	qboolean		instagibMap;

	float	cullDistance;

	qboolean	boonExists;

	// track final ret results of fc kills (stupid variable names to eliminate ambiguity)
	stats_t *redPlayerWhoKilledBlueCarrierOfRedFlag;
	stats_t *bluePlayerWhoKilledRedCarrierOfBlueFlag;

	list_t				*aimPracticePackList;

	int statBlock; // e.g. if level.time - level.startTime is 0 through 299999 this is 0; if level.time - level.startTime is 300000 this is 1

	list_t ratingList;
	list_t mostPlayedPositionsList;

	list_t pugProposalsList;
	list_t barredPlayersList;
	list_t forcedPickablePermabarredPlayersList;
	pugProposal_t *activePugProposal;
	list_t queuedServerMessagesList;
	list_t autoLinksList;

	double lastRelativeStrength[4];
	int lastPlayerTickAddedTime;
	list_t rustyPlayersList;

	struct {
		qboolean valid;
		float value;
	} pitHeight;

	float locationAccuracy;
	qboolean generateLocationsWithInfo_b_e_locationsOnly;

	int g_lastIntermissionStartTimeSettingAtRoundStart; // what g_lastIntermissionStartTime was at the start of this round (before we changed it to "")
	int g_lastTeamGenTimeSettingAtRoundStart; // what g_lastTeamGenTime was at the start of this round (before we changed it to "")
	int shouldAnnounceBreak;
} level_locals_t;


//
// g_accounts.c
//
#define ACCOUNTFLAG_ADMIN					( 1 << 0 )
#define ACCOUNTFLAG_RCONLOG					( 1 << 1 )
#define ACCOUNTFLAG_ENTERSPAMMER			( 1 << 2 )
#define ACCOUNTFLAG_AIMPACKEDITOR			( 1 << 3 )
#define ACCOUNTFLAG_AIMPACKADMIN			( 1 << 4 )
#define ACCOUNTFLAG_VOTETROLL				( 1 << 5 )
#define ACCOUNTFLAG_RATEPLAYERS				( 1 << 6 )
#define ACCOUNTFLAG_INSTAPAUSE_BLACKLIST	( 1 << 7 )
#define ACCOUNTFLAG_PERMABARRED				( 1 << 8 )
#define ACCOUNTFLAG_HARDPERMABARRED			( 1 << 9 )
#define ACCOUNTFLAG_RATEPLAYERS_NOCOUNT		( 1 << 10 )

typedef void( *ListSessionsCallback )( void *ctx,
	const sessionReference_t sessionRef,
	const qboolean temporary );

void G_SaveAccountsCache( void );
qboolean G_ReadAccountsCache( void );
void G_InitClientSession( gclient_t *client );
void G_InitClientAccount( gclient_t* client );
qboolean G_CreateAccount( const char* name, accountReference_t* out );
qboolean G_DeleteAccount( account_t* account );
sessionReference_t G_GetSessionByID( const int id, qboolean onlineOnly );
sessionReference_t G_GetSessionByHash( const sessionInfoHash_t hash, qboolean onlineOnly );
accountReference_t G_GetAccountByID( const int id, qboolean onlineOnly );
accountReference_t G_GetAccountByName( const char* name, qboolean onlineOnly );
qboolean G_LinkAccountToSession( session_t* session, account_t* account );
qboolean G_UnlinkAccountFromSession( session_t* session );
void G_ListSessionsForAccount( account_t* account, int numPerPage, int page, ListSessionsCallback callback, void* ctx );
void G_ListSessionsForInfo( const char* key, const char* value, int numPerPage, int page, ListSessionsCallback callback, void* ctx );
qboolean G_SessionInfoHasString( const session_t* session, const char* key );
void G_GetStringFromSessionInfo( const session_t* session, const char* key, char* outValue, size_t outValueSize );
qboolean G_SetAccountFlags( account_t* account, const int flags, qboolean flagsEnabled );

//
// g_transfers.c
//
void G_HandleTransferResult(trsfHandle_t handle, trsfErrorInfo_t* errorInfo, int responseCode, void* data, size_t size);
void G_PostScoreboardToWebhook(const char* stats);

//
// g_spawn.c
//
qboolean	G_SpawnString( const char *key, const char *defaultString, char **out );
// spawn string returns a temporary reference, you must CopyString() if you want to keep it
qboolean	G_SpawnFloat( const char *key, const char *defaultString, float *out );
qboolean	G_SpawnInt( const char *key, const char *defaultString, int *out );
qboolean	G_SpawnVector( const char *key, const char *defaultString, float *out );
void		G_SpawnEntitiesFromString( qboolean inSubBSP );
char *G_NewString( const char *string );

//
// g_cmds.c
//
#define ColorForTeam( team )		( team == TEAM_BLUE ? COLOR_BLUE : COLOR_RED )
#define ScoreTextForTeam( team )	( team == TEAM_BLUE ? S_COLOR_BLUE"Blue" : S_COLOR_RED"Red" )
void Cmd_Score_f (gentity_t *ent);
void StopFollowing( gentity_t *ent );
void BroadcastTeamChange( gclient_t *client, int oldTeam );
void SetTeam( gentity_t *ent, char *s );
void SetNameQuick( gentity_t *ent, char *s, int renameDelay );
void Cmd_FollowCycle_f( gentity_t *ent, int dir );
void Cmd_SaberAttackCycle_f(gentity_t *ent);
int G_ItemUsable(playerState_t *ps, int forcedUse);
void Cmd_ToggleSaber_f(gentity_t *ent);
void Cmd_EngageDuel_f(gentity_t *ent);
void Cmd_PrintStats_f(gentity_t *ent);
void Cmd_Help_f( gentity_t *ent );
void Cmd_FollowFlag_f( gentity_t *ent );
void Cmd_FollowTarget_f(gentity_t *ent);
gentity_t *G_GetDuelWinner(gclient_t *client);
const char *GetTierStringForTier(mapTier_t tier);
const char *GetTierColorForTier(mapTier_t tier);
qboolean GetShortNameForMapFileName(const char *mapFileName, char *out, size_t outSize);
qboolean GetMatchingMap(const char *in, char *out, size_t outSize);
mapTier_t MapTierForDouble(double average);
typedef struct {
	node_t		node;
	char		mapFileName[MAX_QPATH];
	char		playerName[64];
	int			accountId;
	mapTier_t	tier;
} mapTierData_t;
char *ConcatArgs(int start);
qboolean IsRacerOrSpectator(gentity_t *ent);
ctfPosition_t CtfPositionFromString(char *s);
float GetCTFLocationValue(gentity_t *ent);
qboolean ValidateAndCopyPositionPreferences(const positionPreferences_t *in, positionPreferences_t *out);

//
// g_items.c
//
void ItemUse_Binoculars(gentity_t *ent);
void ItemUse_Shield(gentity_t *ent);
void ItemUse_Sentry(gentity_t *ent);

void Jetpack_Off(gentity_t *ent);
void Jetpack_On(gentity_t *ent);
void ItemUse_Jetpack(gentity_t *ent);
void ItemUse_UseCloak( gentity_t *ent );
void ItemUse_UseDisp(gentity_t *ent, int type);
void ItemUse_UseEWeb(gentity_t *ent);
void G_PrecacheDispensers(void);

void ItemUse_Seeker(gentity_t *ent);
void ItemUse_MedPack(gentity_t *ent);
void ItemUse_MedPack_Big(gentity_t *ent);

void G_CheckTeamItems( void );
void G_RunItem( gentity_t *ent );
void RespawnItem( gentity_t *ent );

void UseHoldableItem( gentity_t *ent );
void PrecacheItem (gitem_t *it);
gentity_t *Drop_Item( gentity_t *ent, gitem_t *item, float angle );
gentity_t *LaunchItem( gitem_t *item, vec3_t origin, vec3_t velocity );
void SetRespawn (gentity_t *ent, float delay);
void G_SpawnItem (gentity_t *ent, gitem_t *item);
void FinishSpawningItem( gentity_t *ent );
void Think_Weapon (gentity_t *ent);
int ArmorIndex (gentity_t *ent);
void	Add_Ammo (gentity_t *ent, int weapon, int count);
void Touch_Item (gentity_t *ent, gentity_t *other, trace_t *trace);

void ClearRegisteredItems( void );
void RegisterItem( gitem_t *item );
void SaveRegisteredItems( void );

//
// g_location.c
//
typedef struct {
	node_t	node;
	vec3_t	origin;
	char	message[MAX_LOCATION_CHARS];
	struct {
		qboolean	valid;
		float		value;
	} min[3];
	struct {
		qboolean	valid;
		float		value;
	} max[3];
	int		teamowner;
} info_b_e_location_listItem_t;
void Location_ResetLookupTree(void);
void Location_AddLocationEntityToList(gentity_t *ent);
int Team_GetLocation(gentity_t *ent, char *locationBuffer, size_t locationBufferSize);
void G_LinkLocations(void);
void Location_SetTreePtr(void **kdtreePtr);

//
// g_utils.c
//
int		G_ModelIndex( const char *name );
int		G_SoundIndex( const char *name );
int		G_SoundSetIndex(const char *name);
int		G_EffectIndex( const char *name );
int		G_BSPIndex( const char *name );
int		G_IconIndex( const char* name );

qboolean	G_PlayerHasCustomSkeleton(gentity_t *ent);

void	G_TeamCommand( team_t team, char *cmd );
void	G_ScaleNetHealth(gentity_t *self);
void	G_KillBox (gentity_t *ent);
gentity_t *G_Find (gentity_t *from, int fieldofs, const char *match);
gentity_t* G_FindClient( const char* pattern );
int		G_RadiusList ( vec3_t origin, float radius,	gentity_t *ignore, qboolean takeDamage, gentity_t *ent_list[MAX_GENTITIES]);

void	G_Throw( gentity_t *targ, vec3_t newDir, float push );

void	G_FreeFakeClient(gclient_t **cl);
void	G_CreateFakeClient(int entNum, gclient_t **cl);
void	G_CleanAllFakeClients(void);

void	G_SetAnim(gentity_t *ent, usercmd_t *ucmd, int setAnimParts, int anim, int setAnimFlags, int blendTime);
gentity_t *G_PickTarget (char *targetname);
void	GlobalUse(gentity_t *self, gentity_t *other, gentity_t *activator);
void	G_UseTargets2( gentity_t *ent, gentity_t *activator, const char *string );
void	G_UseTargets (gentity_t *ent, gentity_t *activator);
void	G_SetMovedir ( vec3_t angles, vec3_t movedir);
void	G_SetAngles( gentity_t *ent, vec3_t angles );

void	G_InitGentity( gentity_t *e );
gentity_t	*G_Spawn (void);
gentity_t *G_TempEntity( vec3_t origin, int event );
gentity_t	*G_PlayEffect(int fxID, vec3_t org, vec3_t ang);
gentity_t	*G_PlayEffectID(const int fxID, vec3_t org, vec3_t ang);
gentity_t *G_ScreenShake(vec3_t org, gentity_t *target, float intensity, int duration, qboolean global);
void	G_MuteSound( int entnum, int channel );
gentity_t *G_Sound( gentity_t *ent, int channel, int soundIndex );
void	G_SoundAtLoc( vec3_t loc, int channel, int soundIndex );
void	G_EntitySound( gentity_t *ent, int channel, int soundIndex );
void	TryUse( gentity_t *ent );
void	G_SendG2KillQueue(void);
void	G_KillG2Queue(int entNum);
void	G_FreeEntity( gentity_t *e );
qboolean	G_EntitiesFree( void );

qboolean G_ActivateBehavior (gentity_t *self, int bset );

void	G_TouchTriggers (gentity_t *ent);
void	G_TouchSolids (gentity_t *ent);
void	GetAnglesForDirection( const vec3_t p1, const vec3_t p2, vec3_t out );

void UpdateGlobalCenterPrint( const int levelTime );
void G_GlobalTickedCenterPrint( const char *msg, int milliseconds, qboolean prioritized );
void G_UniqueTickedCenterPrint(const void *msgs, size_t msgSize, int milliseconds, qboolean prioritized);
void G_ResetAccurateTimerOnTrigger( accurateTimer *timer, gentity_t *activator, gentity_t *trigger );
int G_GetAccurateTimerOnTrigger( accurateTimer *timer, gentity_t *activator, gentity_t *trigger );
typedef qboolean( *ProjectileFilterCallback )( gentity_t* ent );
void G_DeletePlayerProjectilesAtPoint( gentity_t *ent, vec3_t origin, float radius, ProjectileFilterCallback filterCallback );
void G_DeletePlayerProjectiles( gentity_t *ent );
void G_PrintBasedOnRacemode( const char* text, qboolean toRacersOnly );
qboolean G_TeleportRacerToTelemark( gentity_t *ent );

typedef qboolean ( *entityFilter_func )( gentity_t* );
gentity_t* G_ClosestEntity( gentity_t *ref, entityFilter_func );
void Q_strstrip(char *string, const char *strip, const char *repl);
void PrintIngame(int clientNum, const char *s, ...);
void OutOfBandPrint(int clientNum, const char *msg, ...);
gclient_t* G_FindClientByIPPort(const char* ipPortString);
void G_FormatDuration(const int duration, char* out, size_t outSize);

void G_FormatLocalDateFromEpoch( char* buf, size_t bufSize, time_t epochSecs );

qboolean G_GetIPFromString( const char* from, unsigned int* ip );

qboolean FileExists(const char *fileName);

void Q_strstrip(char *string, const char *strip, const char *repl);
char *stristr(const char *str1, const char *str2);
const char *Cvar_VariableString(const char *var_name);


qboolean HasFlag(gentity_t *ent);

void PlayAimPracticeBotPainSound(gentity_t *npc, gentity_t *player);
void CenterPrintToPlayerAndFollowers(gentity_t *ent, const char *s);
void ExitAimTraining(gentity_t *ent);
void PrintBasedOnAccountFlags(int flags, const char *msg);
void FisherYatesShuffle(void *firstItem, size_t numItems, size_t itemSize);
void FormatNumberToStringWithCommas(uint64_t n, char *out, size_t outSize);
qboolean IsSpecName(const char *name);
void SV_Tell(int clientNum, const char *text);
void SV_Say(const char *text);

//
// g_object.c
//

extern void G_RunObject			( gentity_t *ent );


float	*tv (float x, float y, float z);
char	*vtos( const vec3_t v );

void G_AddPredictableEvent( gentity_t *ent, int event, int eventParm );
void G_AddEvent( gentity_t *ent, int event, int eventParm );
void G_SetOrigin( gentity_t *ent, vec3_t origin );
qboolean G_CheckInSolid (gentity_t *self, qboolean fix);
void AddRemap(const char *oldShader, const char *newShader, float timeOffset);
const char *BuildShaderStateConfig(void);
/*
Ghoul2 Insert Start
*/
int G_BoneIndex( const char *name );

#include "namespace_begin.h"

qhandle_t	trap_R_RegisterSkin( const char *name );

// CG specific API access
void		trap_G2_ListModelSurfaces(void *ghlInfo);
void		trap_G2_ListModelBones(void *ghlInfo, int frame);
void		trap_G2_SetGhoul2ModelIndexes(void *ghoul2, qhandle_t *modelList, qhandle_t *skinList);
qboolean	trap_G2_HaveWeGhoul2Models(void *ghoul2);
qboolean	trap_G2API_GetBoltMatrix(void *ghoul2, const int modelIndex, const int boltIndex, mdxaBone_t *matrix,
								const vec3_t angles, const vec3_t position, const int frameNum, qhandle_t *modelList, vec3_t scale);
qboolean	trap_G2API_GetBoltMatrix_NoReconstruct(void *ghoul2, const int modelIndex, const int boltIndex, mdxaBone_t *matrix,
								const vec3_t angles, const vec3_t position, const int frameNum, qhandle_t *modelList, vec3_t scale);
qboolean	trap_G2API_GetBoltMatrix_NoRecNoRot(void *ghoul2, const int modelIndex, const int boltIndex, mdxaBone_t *matrix,
								const vec3_t angles, const vec3_t position, const int frameNum, qhandle_t *modelList, vec3_t scale);
int			trap_G2API_InitGhoul2Model(void **ghoul2Ptr, const char *fileName, int modelIndex, qhandle_t customSkin,
						  qhandle_t customShader, int modelFlags, int lodBias);
qboolean	trap_G2API_SetSkin(void *ghoul2, int modelIndex, qhandle_t customSkin, qhandle_t renderSkin);

int			trap_G2API_Ghoul2Size ( void* ghlInfo );

int			trap_G2API_AddBolt(void *ghoul2, int modelIndex, const char *boneName);
void		trap_G2API_SetBoltInfo(void *ghoul2, int modelIndex, int boltInfo);

int			trap_G2API_CopyGhoul2Instance(void *g2From, void *g2To, int modelIndex);
void		trap_G2API_CopySpecificGhoul2Model(void *g2From, int modelFrom, void *g2To, int modelTo);
void		trap_G2API_DuplicateGhoul2Instance(void *g2From, void **g2To);
qboolean	trap_G2API_HasGhoul2ModelOnIndex(void *ghlInfo, int modelIndex);
qboolean	trap_G2API_RemoveGhoul2Model(void *ghlInfo, int modelIndex);
qboolean	trap_G2API_RemoveGhoul2Models(void *ghlInfo);
void		trap_G2API_CleanGhoul2Models(void **ghoul2Ptr);
void		trap_G2API_CollisionDetect ( CollisionRecord_t *collRecMap, void* ghoul2, const vec3_t angles, const vec3_t position,
								int frameNumber, int entNum, vec3_t rayStart, vec3_t rayEnd, vec3_t scale, int traceFlags, int useLod, float fRadius );
void		trap_G2API_CollisionDetectCache ( CollisionRecord_t *collRecMap, void* ghoul2, const vec3_t angles, const vec3_t position,
								int frameNumber, int entNum, vec3_t rayStart, vec3_t rayEnd, vec3_t scale, int traceFlags, int useLod, float fRadius );

qboolean	trap_G2API_SetBoneAngles(void *ghoul2, int modelIndex, const char *boneName, const vec3_t angles, const int flags,
								const int up, const int right, const int forward, qhandle_t *modelList,
								int blendTime , int currentTime );
void		trap_G2API_GetGLAName(void *ghoul2, int modelIndex, char *fillBuf);
qboolean	trap_G2API_SetBoneAnim(void *ghoul2, const int modelIndex, const char *boneName, const int startFrame, const int endFrame,
							  const int flags, const float animSpeed, const int currentTime, const float setFrame , const int blendTime );
qboolean	trap_G2API_GetBoneAnim(void *ghoul2, const char *boneName, const int currentTime, float *currentFrame, int *startFrame,
								int *endFrame, int *flags, float *animSpeed, int *modelList, const int modelIndex);
void		trap_G2API_GetSurfaceName(void *ghoul2, int surfNumber, int modelIndex, char *fillBuf);
qboolean	trap_G2API_SetRootSurface(void *ghoul2, const int modelIndex, const char *surfaceName);
qboolean	trap_G2API_SetSurfaceOnOff(void *ghoul2, const char *surfaceName, const int flags);
qboolean	trap_G2API_SetNewOrigin(void *ghoul2, const int boltIndex);
qboolean	trap_G2API_DoesBoneExist(void *ghoul2, int modelIndex, const char *boneName);
int			trap_G2API_GetSurfaceRenderStatus(void *ghoul2, const int modelIndex, const char *surfaceName);

void		trap_G2API_AbsurdSmoothing(void *ghoul2, qboolean status);

void		trap_G2API_SetRagDoll(void *ghoul2, sharedRagDollParams_t *params);
void		trap_G2API_AnimateG2Models(void *ghoul2, int time, sharedRagDollUpdateParams_t *params);

//additional ragdoll options -rww
qboolean	trap_G2API_RagPCJConstraint(void *ghoul2, const char *boneName, vec3_t min, vec3_t max); //override default pcj bonee constraints
qboolean	trap_G2API_RagPCJGradientSpeed(void *ghoul2, const char *boneName, const float speed); //override the default gradient movespeed for a pcj bone
qboolean	trap_G2API_RagEffectorGoal(void *ghoul2, const char *boneName, vec3_t pos); //override an effector bone's goal position (world coordinates)
qboolean	trap_G2API_GetRagBonePos(void *ghoul2, const char *boneName, vec3_t pos, vec3_t entAngles, vec3_t entPos, vec3_t entScale); //current position of said bone is put into pos (world coordinates)
qboolean	trap_G2API_RagEffectorKick(void *ghoul2, const char *boneName, vec3_t velocity); //add velocity to a rag bone
qboolean	trap_G2API_RagForceSolve(void *ghoul2, qboolean force); //make sure we are actively performing solve/settle routines, if desired

qboolean	trap_G2API_SetBoneIKState(void *ghoul2, int time, const char *boneName, int ikState, sharedSetBoneIKStateParams_t *params);
qboolean	trap_G2API_IKMove(void *ghoul2, int time, sharedIKMoveParams_t *params);

//for removing bones so they no longer have their own seperate animation hierarchy. Or whatever reason you may have. -rww
qboolean	trap_G2API_RemoveBone(void *ghoul2, const char *boneName, int modelIndex);

void		trap_G2API_AttachInstanceToEntNum(void *ghoul2, int entityNum, qboolean server);
void		trap_G2API_ClearAttachedInstance(int entityNum);
void		trap_G2API_CleanEntAttachments(void);
qboolean	trap_G2API_OverrideServer(void *serverInstance);

#include "namespace_end.h"

/*
Ghoul2 Insert End
*/

//
// g_combat.c
//
qboolean CanDamage (gentity_t *targ, vec3_t origin);
void G_Damage (gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod);
qboolean G_RadiusDamage (vec3_t origin, gentity_t *attacker, float damage, float radius, gentity_t *ignore, gentity_t *missile, int mod);
void body_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath );
void TossClientWeapon(gentity_t *self, vec3_t direction, float speed);
void TossClientItems( gentity_t *self, qboolean canDropWeapons );
void TossClientCubes( gentity_t *self );
void ExplodeDeath( gentity_t *self );
void G_CheckForDismemberment(gentity_t *ent, gentity_t *enemy, vec3_t point, int damage, int deathAnim, qboolean postDeath);
extern int gGAvoidDismember;


// damage flags
#define DAMAGE_NORMAL				0x00000000	// No flags set.
#define DAMAGE_RADIUS				0x00000001	// damage was indirect
#define DAMAGE_NO_ARMOR				0x00000002	// armour does not protect from this damage
#define DAMAGE_NO_KNOCKBACK			0x00000004	// do not affect velocity, just view angles
#define DAMAGE_NO_PROTECTION		0x00000008  // armor, shields, invulnerability, and godmode have no effect
#define DAMAGE_NO_TEAM_PROTECTION	0x00000010  // armor, shields, invulnerability, and godmode have no effect
//JK2 flags
#define DAMAGE_EXTRA_KNOCKBACK		0x00000040	// add extra knockback to this damage
#define DAMAGE_DEATH_KNOCKBACK		0x00000080	// only does knockback on death of target
#define DAMAGE_IGNORE_TEAM			0x00000100	// damage is always done, regardless of teams
#define DAMAGE_NO_DAMAGE			0x00000200	// do no actual damage but react as if damage was taken
#define DAMAGE_HALF_ABSORB			0x00000400	// half shields, half health
#define DAMAGE_HALF_ARMOR_REDUCTION	0x00000800	// This damage doesn't whittle down armor as efficiently.
#define DAMAGE_HEAVY_WEAP_CLASS		0x00001000	// Heavy damage
#define DAMAGE_NO_HIT_LOC			0x00002000	// No hit location
#define DAMAGE_NO_SELF_PROTECTION	0x00004000	// Dont apply half damage to self attacks
#define DAMAGE_NO_DISMEMBER			0x00008000	// Dont do dismemberment
#define DAMAGE_SABER_KNOCKBACK1		0x00010000	// Check the attacker's first saber for a knockbackScale
#define DAMAGE_SABER_KNOCKBACK2		0x00020000	// Check the attacker's second saber for a knockbackScale
#define DAMAGE_SABER_KNOCKBACK1_B2	0x00040000	// Check the attacker's first saber for a knockbackScale2
#define DAMAGE_SABER_KNOCKBACK2_B2	0x00080000	// Check the attacker's second saber for a knockbackScale2
//
// g_exphysics.c
//
void G_RunExPhys(gentity_t *ent, float gravity, float mass, float bounce, qboolean autoKill, int *g2Bolts, int numG2Bolts);

//
// g_missile.c
//
void G_ReflectMissile( gentity_t *ent, gentity_t *missile, vec3_t forward, qboolean coneBased );

void G_RunMissile( gentity_t *ent );

gentity_t *CreateMissile( vec3_t org, vec3_t dir, float vel, int life, 
							gentity_t *owner, qboolean altFire);
void G_BounceProjectile( vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout );
void G_ExplodeMissile( gentity_t *ent );

void WP_FireBlasterMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire );


//
// g_mover.c
//
extern int	BMS_START;
extern int	BMS_MID;
extern int	BMS_END;

#define SPF_BUTTON_USABLE		1
#define SPF_BUTTON_FPUSHABLE	2
void G_PlayDoorLoopSound( gentity_t *ent );
void G_PlayDoorSound( gentity_t *ent, int type );
void G_RunMover( gentity_t *ent );
void Touch_DoorTrigger( gentity_t *ent, gentity_t *other, trace_t *trace );

//
// g_trigger.c
//
void trigger_teleporter_touch (gentity_t *self, gentity_t *other, trace_t *trace );
void G_CreateRaceTrigger( gentity_t *flagBase );

//
// g_misc.c
//
#define MAX_REFNAME	32
#define	START_TIME_LINK_ENTS		FRAMETIME*1

#define	RTF_NONE	0
#define	RTF_NAVGOAL	0x00000001

typedef struct reference_tag_s
{
	char		name[MAX_REFNAME];
	vec3_t		origin;
	vec3_t		angles;
	int			flags;	//Just in case
	int			radius;	//For nav goals
	qboolean	inuse;
} reference_tag_t;

void TAG_Init( void );
reference_tag_t	*TAG_Find( const char *owner, const char *name );
reference_tag_t	*TAG_Add( const char *name, const char *owner, vec3_t origin, vec3_t angles, int radius, int flags );
int	TAG_GetOrigin( const char *owner, const char *name, vec3_t origin );
int	TAG_GetOrigin2( const char *owner, const char *name, vec3_t origin );
int	TAG_GetAngles( const char *owner, const char *name, vec3_t angles );
int TAG_GetRadius( const char *owner, const char *name );
int TAG_GetFlags( const char *owner, const char *name );

void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles );
qboolean DoRunoff(void);

//
// g_weapon.c
//
void WP_FireTurretMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire, int damage, int velocity, int mod, gentity_t *ignore );
void WP_FireGenericBlasterMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire, int damage, int velocity, int mod );
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker );
void CalcMuzzlePoint ( gentity_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint );
void SnapVectorTowards( vec3_t v, vec3_t to );
qboolean CheckGauntletAttack( gentity_t *ent );


//
// g_client.c
//
team_t TeamCount( int ignoreClientNum, int team );
int TeamLeader( int team );
team_t PickTeam( int ignoreClientNum );
void SetClientViewAngle( gentity_t *ent, vec3_t angle );
gentity_t *SelectSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles, team_t team );
void MaintainBodyQueue(gentity_t *ent);
void respawn (gentity_t *ent);
void BeginIntermission (void);
void InitBodyQue (void);
void ClientSpawn( gentity_t *ent );
void player_die (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);
void AddScore( gentity_t *ent, vec3_t origin, int score );
void CalculateRanks( void );
qboolean SpotWouldTelefrag( gentity_t *spot );
void NormalizeName(const char *in, char *out, int outSize, int colorlessSize);
#ifdef NEWMOD_SUPPORT
void G_BroadcastServerFeatureList(int clientNum);
#endif
void G_PrintWelcomeMessage(gclient_t* client);
extern gentity_t *gJMSaberEnt;
void TellPlayerToRateMap(gclient_t *client);
void TellPlayerToSetPositions(gclient_t *client);
void RestoreDisconnectedPlayerData(gentity_t *ent);

//
// g_svcmds.c
//
qboolean	ConsoleCommand( void );
void G_ProcessGetstatusIPBans(void);
qboolean G_FilterPacket( char *from, char* reasonBuffer, int reasonBufferSize);
qboolean G_FilterGetstatusPacket (unsigned int ip);
void G_LogRconCommand(const char* ipFrom, const char* command);
qboolean getIpFromString( const char* from, unsigned int* ip );
qboolean getIpPortFromString( const char* from, unsigned int* ip, int* port );
void getStringFromIp( unsigned int ip, char* buffer, int size );
void G_Status(void);
qboolean MapExistsQuick(const char *mapFileName);
const char *AccountBitflag2FlagName(int bitflag);

//
// g_weapon.c
//
void FireWeapon( gentity_t *ent, qboolean altFire );
void BlowDetpacks(gentity_t *ent);
void RemoveDetpacks(gentity_t *ent);

//
// p_hud.c
//
void MoveClientToIntermission (gentity_t *client);
void G_SetStats (gentity_t *ent);
void DeathmatchScoreboardMessage (gentity_t *client);

//
// g_cmds.c
//
char* NM_SerializeUIntToColor(const unsigned int n);
void G_InitVchats();
int CompareVersions(const char *verStr, const char *compareToStr);

typedef struct
{
	int entity;
	int count;

	char poolName[64];
} ListPoolsContext;

typedef struct
{
	int entity;
	int count;
	int pool_id;

	char long_name[64];

} ListMapsInPoolContext;

//
// g_pweapon.c
//


//
// g_main.c
//
extern vmCvar_t g_ff_objectives;
extern qboolean gDoSlowMoDuel;
extern int gSlowMoDuelTime;

void G_PowerDuelCount(int *loners, int *doubles, qboolean countSpec);

void FindIntermissionPoint( void );
void SetLeader(int team, int client);
void CheckTeamLeader( int team );
void G_RunThink (gentity_t *ent);
void QDECL G_LogPrintf( const char *fmt, ... );
void QDECL G_HackLog(const char *fmt, ...);
void QDECL G_DBLog( const char *fmt, ... );
void QDECL G_RconLog( const char *fmt, ... );
void SendScoreboardMessageToAllClients( void );
void QDECL G_Printf( const char *fmt, ... );
void QDECL G_Error( const char *fmt, ... );
const char *G_GetStringEdString(char *refSection, char *refName);
void G_ApplyRaceBroadcastsToEvent( gentity_t *parent, gentity_t *ev );
qboolean InstagibEnabled(void);

//
// g_client.c
//

char *ClientConnect( int clientNum, qboolean firstTime, qboolean isBot );
void ClientUserinfoChanged( int clientNum );
void ClientDisconnect( int clientNum );
void ClientBegin( int clientNum, qboolean allowTeamReset );
void G_BreakArm(gentity_t *ent, int arm);
void G_UpdateClientAnims(gentity_t *self, float animSpeedScale);
void G_SetRaceMode( gentity_t *self, qboolean race );
void G_GiveRacemodeItemsAndFullStats( gentity_t *ent );
void SetRacerForcePowers(gentity_t *ent);
void G_UpdateRaceBitMasks( gclient_t *client );
void G_InitClientRaceRecordsCache(gclient_t* client);
void ClientCommand( int clientNum );
void PurgeStringedTrolling(char *in, char *out, int outSize);

//
// g_active.c
//
void G_CheckClientTimeouts	( gentity_t *ent );
void ClientThink			( int clientNum, usercmd_t *ucmd );
void ClientEndFrame			( gentity_t *ent );
void G_RunClient			( gentity_t *ent );
qboolean IsInputting(const gclient_t *client, qboolean checkPressingButtons, qboolean checkMovingMouse, qboolean checkPressingChatButton);

typedef enum {
	NMTAUNT_ANGER1 = 100, //gloat
	NMTAUNT_ANGER2,
	NMTAUNT_ANGER3,
	NMTAUNT_GLOAT1,
	NMTAUNT_GLOAT2,
	NMTAUNT_GLOAT3,
	NMTAUNT_DEFLECT1, //flourish
	NMTAUNT_DEFLECT2,
	NMTAUNT_DEFLECT3,
	NMTAUNT_VICTORY1,
	NMTAUNT_VICTORY2,
	NMTAUNT_VICTORY3
} nmTaunt_t;
void TimeShiftLerp(float frac, vec3_t start, vec3_t end, vec3_t result);
void TimeShiftAnimLerp(float frac, int anim1, int anim2, int time1, int time2, int *outTime);
qboolean PauseConditions(void);
#define ROCK_PAPER_SCISSORS_DURATION					(10000)
//
// g_table.c
//
typedef struct {
	node_t	node;
	char	mapname[MAX_QPATH];
	int		weight;
} poolMap_t;
typedef struct {
	node_t	node;
	char	shortName[64];
	char	longName[64];
} pool_t;
typedef const char *(*ColumnDataCallback)(void *rowContext, void *columnContext);
typedef struct {
	list_t			columnList;
	list_t			rowList;
	qboolean		alternateColors;
	int				lastColumnIdAssigned;
} Table;
void listMapsInPools(void **context, const char *long_name, int pool_id, const char *mapname, int mapWeight);
void listPools(void *context, int pool_id, const char *short_name, const char *long_name);

const char *TableCallback_MapName(void *rowContext, void *columnContext);
const char *TableCallback_MapWeight(void *rowContext, void *columnContext);
const char *TableCallback_PoolShortName(void *rowContext, void *columnContext);
const char *TableCallback_PoolLongName(void *rowContext, void *columnContext);
const char *TableCallback_ClientNum(void *rowContext, void *columnContext);
const char *TableCallback_Name(void *rowContext, void *columnContext);
const char *TableCallback_Account(void *rowContext, void *columnContext);
const char *TableCallback_Alias(void *rowContext, void *columnContext);
const char *TableCallback_Notes(void *rowContext, void *columnContext);
const char *TableCallback_Ping(void *rowContext, void *columnContext);
const char *TableCallback_Score(void *rowContext, void *columnContext);
const char *TableCallback_IP(void *rowContext, void *columnContext);
const char *TableCallback_Qport(void *rowContext, void *columnContext);
const char *TableCallback_Country(void *rowContext, void *columnContext);
const char *TableCallback_Mod(void *rowContext, void *columnContext);
const char *TableCallback_Shadowmuted(void *rowContext, void *columnContext);

typedef struct {
	stats_t *tablePlayerStats;
	qboolean damageTaken;
	meansOfDeathCategory_t modc;
} meansOfDeathCategoryContext_t;

Table *Table_Initialize(qboolean alternateColors);
void Table_DefineRow(Table *t, void *context);
void Table_DefineColumn(Table *t, const char *title, ColumnDataCallback callback, void *columnContext, qboolean leftAlign, int dividerColor, int maxLen);
void Table_AddHorizontalRule(Table *t, int customColor);
void Table_Destroy(Table *t);
void Table_WriteToBuffer(Table *t, char *buf, size_t bufSize, qboolean showHeader, int customHeaderColor);

//
// g_team.c
//
qboolean OnSameTeam( gentity_t *ent1, gentity_t *ent2 );
void Team_CheckDroppedItem( gentity_t *dropped );

//
// g_mem.c
//
void *G_Alloc( int size );
void G_InitMemory( void );
void Svcmd_GameMem_f( void );

//
// g_session.c
//
void G_InitSessionData( gclient_t *client, char *userinfo, qboolean isBot, qboolean firstTime );

void G_InitWorldSession( void );
void G_WriteSessionData( void );
void G_ReadSessionData( qboolean restart, qboolean resetAccounts );

//
// NPC_senses.cpp
//
extern void AddSightEvent( gentity_t *owner, vec3_t position, float radius, alertEventLevel_e alertLevel, float addLight ); //addLight = 0.0f
extern void AddSoundEvent( gentity_t *owner, vec3_t position, float radius, alertEventLevel_e alertLevel, qboolean needLOS ); //needLOS = qfalse
extern qboolean G_CheckForDanger( gentity_t *self, int alertEvent );
extern int G_CheckAlertEvents( gentity_t *self, qboolean checkSight, qboolean checkSound, float maxSeeDist, float maxHearDist, int ignoreAlert, qboolean mustHaveOwner, int minAlertLevel ); //ignoreAlert = -1, mustHaveOwner = qfalse, minAlertLevel = AEL_MINOR
extern qboolean G_CheckForDanger( gentity_t *self, int alertEvent );
extern qboolean G_ClearLOS( gentity_t *self, const vec3_t start, const vec3_t end );
extern qboolean G_ClearLOS2( gentity_t *self, gentity_t *ent, const vec3_t end );
extern qboolean G_ClearLOS3( gentity_t *self, const vec3_t start, gentity_t *ent );
extern qboolean G_ClearLOS4( gentity_t *self, gentity_t *ent );
extern qboolean G_ClearLOS5( gentity_t *self, const vec3_t end );

//
// g_arenas.c
//
void UpdateTournamentInfo( void );

//
// g_bot.c
//
void G_InitBots( qboolean restart );
char *G_GetBotInfoByNumber( int num );
char *G_GetBotInfoByName( const char *name );
void G_CheckBotSpawn( void );
void G_RemoveQueuedBotBegin( int clientNum );
qboolean G_BotConnect( int clientNum, qboolean restart );
void Svcmd_AddBot_f( void );
void Svcmd_BotList_f( void );
void BotInterbreedEndMatch( void );
const char* G_DoesMapSupportGametype(const char *mapname, int gametype);
const char *G_RefreshNextMap(int gametype, qboolean forced);
const char *G_GetDefaultMap(int gametype);
void G_InitVoteMapsLimit();

// w_force.c / w_saber.c
gentity_t *G_PreDefSound(gentity_t *ent, int pdSound);
qboolean HasSetSaberOnly(void);
void WP_ForcePowerStop( gentity_t *self, forcePowers_t forcePower );
void WP_ForcePowerStart( gentity_t *self, forcePowers_t forcePower, int overrideAmt );
void WP_SaberPositionUpdate( gentity_t *self, usercmd_t *ucmd );
int WP_SaberCanBlock(gentity_t *self, gentity_t* other, vec3_t point, int dflags, int mod, qboolean projectile, int attackStr);
void WP_SaberInitBladeData( gentity_t *ent );
void WP_InitForcePowers( gentity_t *ent );
void WP_SpawnInitForcePowers( gentity_t *ent );
void WP_ForcePowersUpdate( gentity_t *self, usercmd_t *ucmd );
int ForcePowerUsableOn(gentity_t *attacker, gentity_t *other, forcePowers_t forcePower);
void ForceHeal( gentity_t *self );
void ForceSpeed( gentity_t *self, int forceDuration );
void ForceRage( gentity_t *self );
void ForceGrip( gentity_t *self );
void ForceProtect( gentity_t *self );
void ForceAbsorb( gentity_t *self );
void ForceTeamHeal( gentity_t *self );
void ForceTeamForceReplenish( gentity_t *self );
void ForceSeeing( gentity_t *self );
void ForceThrow( gentity_t *self, qboolean pull );
void ForceTelepathy(gentity_t *self);
qboolean Jedi_DodgeEvasion( gentity_t *self, gentity_t *shooter, trace_t *tr, int hitLoc );

// g_log.c
void QDECL G_LogPrintf( const char *fmt, ... );
void QDECL G_LogWeaponPickup(int client, int weaponid);
void QDECL G_LogWeaponFire(int client, int weaponid);
void QDECL G_LogWeaponDamage(int client, int mod, int amount);
void QDECL G_LogWeaponKill(int client, int mod);
void QDECL G_LogWeaponDeath(int client, int weaponid);
void QDECL G_LogWeaponFrag(int attacker, int deadguy);
void QDECL G_LogWeaponPowerup(int client, int powerupid);
void QDECL G_LogWeaponItem(int client, int itemid);
void QDECL G_LogWeaponInit(void);
void QDECL G_LogWeaponOutput(void);
void QDECL G_LogExit( const char *string );
void QDECL G_ClearClientLog(int client);

int getGlobalTime();

// g_siege.c
void InitSiegeMode(void);
void G_SiegeClientExData(gentity_t *msgTarg);
char* G_SiegeTeamName( int team );

// g_timer
//Timing information
void		TIMER_Clear( void );
void		TIMER_Clear2( gentity_t *ent );
void		TIMER_Set( gentity_t *ent, const char *identifier, int duration );
int			TIMER_Get( gentity_t *ent, const char *identifier );
qboolean	TIMER_Done( gentity_t *ent, const char *identifier );
qboolean	TIMER_Start( gentity_t *self, const char *identifier, int duration );
qboolean	TIMER_Done2( gentity_t *ent, const char *identifier, qboolean remove );
qboolean	TIMER_Exists( gentity_t *ent, const char *identifier );
void		TIMER_Remove( gentity_t *ent, const char *identifier );

float NPC_GetHFOVPercentage( vec3_t spot, vec3_t from, vec3_t facing, float hFOV );
float NPC_GetVFOVPercentage( vec3_t spot, vec3_t from, vec3_t facing, float vFOV );


extern void G_SetEnemy (gentity_t *self, gentity_t *enemy);
qboolean InFront( vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold );

// ai_main.c
#define MAX_FILEPATH			144

int		OrgVisible		( vec3_t org1, vec3_t org2, int ignore);
void	BotOrder		( gentity_t *ent, int clientnum, int ordernum);
int		InFieldOfVision	( vec3_t viewangles, float fov, vec3_t angles);

// ai_util.c
void B_InitAlloc(void);
void B_CleanupAlloc(void);

//bot settings
typedef struct bot_settings_s
{
	char personalityfile[MAX_FILEPATH];
	float skill;
	char team[MAX_FILEPATH];
} bot_settings_t;

int BotAISetup( int restart );
int BotAIShutdown( int restart );
int BotAILoadMap( int restart );
int BotAISetupClient(int client, struct bot_settings_s *settings, qboolean restart);
int BotAIShutdownClient( int client, qboolean restart );
int BotAIStartFrame( int time );

#include "g_team.h" // teamplay specific stuff


extern	level_locals_t	level;
extern	gentity_t		g_entities[MAX_GENTITIES];

#define	FOFS(x) ((int)&(((gentity_t *)0)->x))

//OSP: pause
#define PAUSE_NONE						0x00    // Match is NOT paused.
#define PAUSE_PAUSED					0x01    // Match is paused, counting down
#define PAUSE_UNPAUSING					0x02    // Pause is about to expire

extern	vmCvar_t	g_gametype;
extern	vmCvar_t	g_dedicated;
extern	vmCvar_t	g_developer;
extern	vmCvar_t	g_cheats;
extern	vmCvar_t	g_maxclients;			// allow this many total, including spectators
extern	vmCvar_t	g_maxGameClients;		// allow this many active
extern	vmCvar_t	g_restarted;
extern	vmCvar_t	g_wasIntermission;

extern	vmCvar_t	g_trueJedi;

extern	vmCvar_t	g_autoMapCycle;
extern	vmCvar_t	g_autoStats;
extern	vmCvar_t	g_dmflags;
extern	vmCvar_t	g_maxForceRank;
extern	vmCvar_t	g_forceBasedTeams;
extern	vmCvar_t	g_privateDuel;

extern  vmCvar_t	g_duelLifes;
extern  vmCvar_t	g_duelShields;

extern	vmCvar_t	g_allowNPC;

extern	vmCvar_t	g_chatLimit;
extern	vmCvar_t	g_teamChatLimit;
extern	vmCvar_t	g_voiceChatLimit;

extern	vmCvar_t	g_armBreakage;

extern	vmCvar_t	g_saberLocking;
extern	vmCvar_t	g_saberLockFactor;
extern	vmCvar_t	g_saberTraceSaberFirst;

extern	vmCvar_t	d_saberKickTweak;

extern	vmCvar_t	d_powerDuelPrint;

extern	vmCvar_t	d_saberGhoul2Collision;
extern	vmCvar_t	g_saberBladeFaces;
extern	vmCvar_t	d_saberAlwaysBoxTrace;
extern	vmCvar_t	d_saberBoxTraceSize;

extern	vmCvar_t	d_siegeSeekerNPC;

extern	vmCvar_t	g_debugMelee;
extern	vmCvar_t	g_stepSlideFix;

extern	vmCvar_t	g_noSpecMove;

#ifdef _DEBUG
extern	vmCvar_t	g_disableServerG2;
#endif

extern	vmCvar_t	d_perPlayerGhoul2;

extern	vmCvar_t	d_projectileGhoul2Collision;

extern	vmCvar_t	g_g2TraceLod;

extern	vmCvar_t	g_optvehtrace;

extern	vmCvar_t	g_locationBasedDamage;

extern	vmCvar_t	g_fixSaberDefense;
extern	vmCvar_t	g_saberDefense1Angle;
extern	vmCvar_t	g_saberDefense2Angle;
extern	vmCvar_t	g_saberDefense3Angle;

extern	vmCvar_t	g_instagib;
extern	vmCvar_t	g_instagibRespawnTime;
extern	vmCvar_t	g_instagibRespawnMinPlayers;

extern	vmCvar_t	g_allowHighPingDuelist;

extern	vmCvar_t	g_logClientInfo;

extern	vmCvar_t	g_slowmoDuelEnd;

extern	vmCvar_t	g_saberDamageScale;

extern	vmCvar_t	g_useWhileThrowing;

extern	vmCvar_t	g_RMG;

extern	vmCvar_t	g_svfps;

extern	vmCvar_t	g_forceRegenTime;
extern	vmCvar_t	g_spawnInvulnerability;
extern	vmCvar_t	g_forcePowerDisable;
extern	vmCvar_t	g_weaponDisable;

extern	vmCvar_t	g_allowDuelSuicide;
extern	vmCvar_t	g_fraglimitVoteCorrection;

extern	vmCvar_t	g_duelWeaponDisable;
extern	vmCvar_t	g_fraglimit;
extern	vmCvar_t	g_duel_fraglimit;
extern	vmCvar_t	g_timelimit;
extern	vmCvar_t	g_nonLiveMatchesCanEnd;
extern	vmCvar_t	g_capturelimit;
extern	vmCvar_t	g_capturedifflimit;
extern	vmCvar_t	d_saberInterpolate;
extern	vmCvar_t	g_friendlyFire;
extern	vmCvar_t	g_friendlySaber;
extern	vmCvar_t	g_password;
extern	vmCvar_t	sv_privatepassword;
extern	vmCvar_t	g_needpass;
extern	vmCvar_t	g_gravity;
extern	vmCvar_t	g_speed;
extern	vmCvar_t	g_knockback;
extern	vmCvar_t	g_forcerespawn;
extern	vmCvar_t	g_siegeRespawn;
extern	vmCvar_t	g_inactivity;
extern	vmCvar_t	g_inactivityKick;
extern	vmCvar_t	g_spectatorInactivity;
extern	vmCvar_t	g_debugMove;

extern	vmCvar_t	g_debugAlloc;
#ifndef FINAL_BUILD
extern	vmCvar_t	g_debugDamage;
#endif
extern	vmCvar_t	g_debugServerSkel;
extern	vmCvar_t	g_weaponRespawn;
extern	vmCvar_t	g_weaponTeamRespawn;
extern	vmCvar_t	g_adaptRespawn;
extern	vmCvar_t	g_synchronousClients;
extern	vmCvar_t	g_forceClientUpdateRate;
extern	vmCvar_t	g_motd;
extern	vmCvar_t	g_warmup;
extern	vmCvar_t	g_doWarmup;
extern	vmCvar_t	g_blood;
extern	vmCvar_t	g_allowVote;
extern	vmCvar_t	g_teamAutoJoin;
extern	vmCvar_t	g_teamForceBalance;
extern	vmCvar_t	g_banIPs;
extern	vmCvar_t	g_getstatusbanIPs;
extern	vmCvar_t	g_filterBan;
extern	vmCvar_t	g_debugForward;
extern	vmCvar_t	g_debugRight;
extern	vmCvar_t	g_debugUp;
extern	vmCvar_t	g_smoothClients;
extern	vmCvar_t	g_defaultBanHoursDuration;
extern	vmCvar_t	g_floatingItems;
extern	vmCvar_t	g_rocketSurfing;
extern	vmCvar_t	g_bouncePadDoubleJump;

#include "namespace_begin.h"
extern	vmCvar_t	pmove_fixed;
extern	vmCvar_t	pmove_msec;
extern	vmCvar_t	pmove_float;
#include "namespace_end.h"

extern	vmCvar_t	g_flechetteSpread;

extern vmCvar_t		g_defaultMapFFA;
extern vmCvar_t		g_defaultMapDuel;
extern vmCvar_t		g_defaultMapSiege;
extern vmCvar_t		g_defaultMapCTF;

extern	vmCvar_t	g_enableBreath;
extern	vmCvar_t	g_singlePlayer;
extern	vmCvar_t	g_dismember;
extern	vmCvar_t	g_forceDodge;
extern	vmCvar_t	g_timeouttospec;

extern	vmCvar_t	g_saberDmgVelocityScale;
extern	vmCvar_t	g_saberDmgDelay_Idle;
extern	vmCvar_t	g_saberDmgDelay_Wound;

#ifndef FINAL_BUILD
extern	vmCvar_t	g_saberDebugPrint;
#endif

extern	vmCvar_t	g_siegeTeamSwitch;

extern	vmCvar_t	bg_fighterAltControl;

#ifdef DEBUG_SABER_BOX
extern	vmCvar_t	g_saberDebugBox;
#endif

//NPC nav debug
extern vmCvar_t		d_altRoutes;
extern vmCvar_t		d_patched;
extern	vmCvar_t	d_noIntermissionWait;

extern	vmCvar_t	g_siegeTeam1;
extern	vmCvar_t	g_siegeTeam2;

extern	vmCvar_t	g_austrian;

extern	vmCvar_t	g_powerDuelStartHealth;
extern	vmCvar_t	g_powerDuelEndHealth;

extern vmCvar_t		g_showDuelHealths;

// *CHANGE 12* allowing/disabling cvars
extern vmCvar_t		g_protectQ3Fill;
extern vmCvar_t		g_protectQ3FillIPLimit;
extern vmCvar_t		g_protectHPhack;
extern vmCvar_t		g_protectCallvoteHack;
extern vmCvar_t		g_maxIPConnected;
extern vmCvar_t		g_minimumVotesCount;
extern vmCvar_t		g_enforceEvenVotersCount;
extern vmCvar_t		g_minVotersForEvenVotersCount;

extern vmCvar_t		g_duplicateNamesId;

extern vmCvar_t		g_droppedFlagSpawnProtectionRadius;
extern vmCvar_t		g_droppedFlagSpawnProtectionDuration;
extern vmCvar_t		g_selfKillSpawnSpamProtection;

#ifdef NEWMOD_SUPPORT
extern vmCvar_t		g_netUnlock;
#define NMF_KICK			(1 << 0)
#define NMF_BACKFLIP		(1 << 1)
#define NMF_NETUNLOCK		(1 << 2)
extern vmCvar_t		g_nmFlags;
extern vmCvar_t		g_enableNmAuth;
extern vmCvar_t		g_specInfo;
#endif

extern vmCvar_t     g_strafejump_mod;

extern vmCvar_t     g_antiWallhack;
extern vmCvar_t		g_wallhackMaxTraces;

extern vmCvar_t     g_inMemoryDB;

extern vmCvar_t     g_enableRacemode;
extern vmCvar_t     g_enableAimPractice;
#ifdef _DEBUG
extern vmCvar_t     d_disableRaceVisChecks;
#endif

extern vmCvar_t		g_traceSQL;

extern vmCvar_t     g_quietrcon;

extern vmCvar_t     g_hackLog;

extern vmCvar_t     g_fixPitKills;

// flags for g_balanceSaber
#define SB_KICK				(1<<0) // kick with all sabers
#define SB_BACKFLIP			(1<<1) // backflip with all sabers
#define SB_OFFENSE			(1<<2) // 3 styles with saber offense 1
#define SB_OFFENSE_TAV_DES	(1<<3) // tavion/desann with offense 2/3

extern vmCvar_t		g_balanceSaber;
extern vmCvar_t		g_balanceSeeing;

extern vmCvar_t		g_autoSendScores;

extern vmCvar_t		g_lineOfSightLocations;
extern vmCvar_t		g_lineOfSightLocations_generate;
extern vmCvar_t		g_enableChatLocations;

// flags for g_randFix
#define BROKEN_RNG_BLOCK	(1<<0) // intentionally break rng for saber blocking
#define BROKEN_RNG_DEFLECT	(1<<1) // intentionally break rng for projectile deflection
#define BROKEN_RNG_REFLECT	(1<<2) // intentionally break rng for projectile reflection

extern vmCvar_t		g_breakRNG;

// flags for g_randomConeReflection
#define CONE_REFLECT_PUSH	(1<<0) // toggle cone based random reflection when pushing projectiles
#define CONE_REFLECT_SDEF	(1<<1) // toggle cone based random reflection with saber def 3

extern vmCvar_t		g_randomConeReflection;
extern vmCvar_t		g_coneReflectAngle;

#ifdef _DEBUG
extern vmCvar_t		z_debug1;
extern vmCvar_t		z_debug2;
extern vmCvar_t		z_debug3;
extern vmCvar_t		z_debug4;
extern vmCvar_t		z_debug5;
extern vmCvar_t		z_debug6;
extern vmCvar_t		z_debug7;
extern vmCvar_t		z_debug8;
extern vmCvar_t		z_debug9;
#endif

extern vmCvar_t     g_allow_vote_gametype;
extern vmCvar_t     g_allow_vote_kick;
extern vmCvar_t     g_allow_vote_restart;
extern vmCvar_t     g_allow_vote_map;
extern vmCvar_t     g_allow_vote_nextmap;
extern vmCvar_t     g_allow_vote_timelimit;
extern vmCvar_t     g_allow_vote_fraglimit;
extern vmCvar_t     g_allow_vote_maprandom;
extern vmCvar_t     g_allow_vote_mapvote;
extern vmCvar_t     g_allow_vote_warmup;
extern vmCvar_t     g_allow_vote_boon;
extern vmCvar_t     g_allow_vote_instagib;
extern vmCvar_t		g_default_capturedifflimit;
extern vmCvar_t		g_enable_maprandom_wildcard;
extern vmCvar_t		g_redirectDoWarmupVote;
extern vmCvar_t		g_redirectNextMapVote;
extern vmCvar_t		g_redirectPoolVoteToTierListVote;
extern vmCvar_t     g_restart_countdown;

extern vmCvar_t		g_allowReady;

extern vmCvar_t	g_accounts;
extern vmCvar_t	g_accountsFile;

extern vmCvar_t    g_whitelist;
extern vmCvar_t    g_enableBoon;
extern vmCvar_t    g_enableMemePickups;
extern vmCvar_t    g_maxstatusrequests;
extern vmCvar_t	   g_logrcon;
extern vmCvar_t	   g_flags_overboarding;
extern vmCvar_t	   g_selfkill_penalty;
extern vmCvar_t	   g_moreTaunts;
extern vmCvar_t    g_raceEmotes;
extern vmCvar_t		g_ragersCanCounterPushPull;
extern vmCvar_t		g_autoPause999;
extern vmCvar_t		g_autoPauseDisconnect;
extern vmCvar_t		g_enterSpammerTime;
extern vmCvar_t		g_quickPauseChat;

extern vmCvar_t		g_vote_tierlist;
extern vmCvar_t		g_vote_tierlist_s_min;
extern vmCvar_t		g_vote_tierlist_s_max;
extern vmCvar_t		g_vote_tierlist_a_min;
extern vmCvar_t		g_vote_tierlist_a_max;
extern vmCvar_t		g_vote_tierlist_b_min;
extern vmCvar_t		g_vote_tierlist_b_max;
extern vmCvar_t		g_vote_tierlist_c_min;
extern vmCvar_t		g_vote_tierlist_c_max;
extern vmCvar_t		g_vote_tierlist_f_min;
extern vmCvar_t		g_vote_tierlist_f_max;
extern vmCvar_t		g_vote_tierlist_totalMaps;
extern vmCvar_t		g_vote_tierlist_debug;
extern vmCvar_t		g_vote_tierlist_reminders;
extern vmCvar_t		g_vote_rng;
extern vmCvar_t		g_vote_runoff;
extern vmCvar_t		g_vote_mapCooldownMinutes;
extern vmCvar_t		g_vote_runoffTimeModifier;
extern vmCvar_t		g_vote_redirectMapVoteToLiveVersion;
extern vmCvar_t		g_vote_printLiveVersionFullName;

extern vmCvar_t		g_vote_teamgen;
extern vmCvar_t		g_vote_teamgen_pug_requiredVotes;
extern vmCvar_t		g_vote_teamgen_team_requiredVotes;
extern vmCvar_t		g_vote_teamgen_subhelp;
extern vmCvar_t		g_vote_teamgen_rustWeeks;
extern vmCvar_t		g_vote_teamgen_minSecsSinceIntermission;
extern vmCvar_t		g_vote_teamgen_enableAppeasing;
extern vmCvar_t		g_vote_teamgen_enableInclusive;
extern vmCvar_t		g_vote_teamgen_remindPositions;
extern vmCvar_t		g_vote_teamgen_remindToSetPositions;
extern vmCvar_t		g_vote_teamgen_announceBreak;
extern vmCvar_t		g_vote_teamgen_autoRestartOnMapChange;
extern vmCvar_t		g_vote_teamgen_autoMapVoteSeconds;
extern vmCvar_t		g_vote_teamgen_iterate;

extern vmCvar_t		g_lastIntermissionStartTime;
extern vmCvar_t		g_lastTeamGenTime;
extern vmCvar_t		g_lastMapVotedMap;
extern vmCvar_t		g_lastMapVotedTime;

extern vmCvar_t		d_debugCtfPosCalculation;

extern vmCvar_t		g_notFirstMap;
extern vmCvar_t		g_shouldReloadPlayerPugStats;

extern vmCvar_t		g_rockPaperScissors;

extern vmCvar_t		g_gripBuff;
extern vmCvar_t		g_gripRefreshRate;

extern vmCvar_t		g_minimumCullDistance;

extern vmCvar_t    g_webhookId;
extern vmCvar_t    g_webhookToken;

extern vmCvar_t		g_teamOverlayForce;

extern vmCvar_t	   g_teamPrivateDuels;
extern vmCvar_t    g_improvedHoming;
extern vmCvar_t    g_improvedHomingThreshold;
extern vmCvar_t    d_debugImprovedHoming;
extern vmCvar_t    g_braindeadBots;
extern vmCvar_t    g_unlagged;
#ifdef _DEBUG
extern vmCvar_t    g_unlaggedMaxCompensation;
extern vmCvar_t    g_unlaggedSkeletons;
extern vmCvar_t	   g_unlaggedSkeletonTime;
extern vmCvar_t	   g_unlaggedFactor;
extern vmCvar_t	   g_unlaggedOffset;
extern vmCvar_t	   g_unlaggedDebug;
#endif

#define MAX_CUSTOM_VOTES	(10)
extern vmCvar_t    g_customVotes;
extern vmCvar_t    g_customVote1_command;
extern vmCvar_t    g_customVote1_label;
extern vmCvar_t    g_customVote2_command;
extern vmCvar_t    g_customVote2_label;
extern vmCvar_t    g_customVote3_command;
extern vmCvar_t    g_customVote3_label;
extern vmCvar_t    g_customVote4_command;
extern vmCvar_t    g_customVote4_label;
extern vmCvar_t    g_customVote5_command;
extern vmCvar_t    g_customVote5_label;
extern vmCvar_t    g_customVote6_command;
extern vmCvar_t    g_customVote6_label;
extern vmCvar_t    g_customVote7_command;
extern vmCvar_t    g_customVote7_label;
extern vmCvar_t    g_customVote8_command;
extern vmCvar_t    g_customVote8_label;
extern vmCvar_t    g_customVote9_command;
extern vmCvar_t    g_customVote0_label;
extern vmCvar_t    g_customVote10_command;
extern vmCvar_t    g_customVote10_label;

extern vmCvar_t	   debug_clientNumLog;

extern vmCvar_t    g_teamOverlayUpdateRate;

extern vmCvar_t	   g_rconpassword;

extern vmCvar_t	   g_callvotedelay;
extern vmCvar_t	   g_callvotemaplimit;

extern vmCvar_t    g_mapVotePlayers;
extern vmCvar_t    g_mapVoteThreshold;

extern vmCvar_t	   sv_privateclients;
extern vmCvar_t    sv_passwordlessSpectators;

extern vmCvar_t    d_measureAirTime;
extern vmCvar_t		g_wasRestarted;
extern vmCvar_t		g_notifyAFK;

#define WAITFORAFK_AFK_MIN				(1)
#define WAITFORAFK_AFK_MAX				(30)
#define WAITFORAFK_AFK_DEFAULT			(5)

#define WAITFORAFK_COUNTDOWN_MIN		(3)
#define WAITFORAFK_COUNTDOWN_MAX		(30)
#define WAITFORAFK_COUNTDOWN_DEFAULT	(10)

#define WAITFORAFK_MINPLAYERS_MIN		(2)
#define WAITFORAFK_MINPLAYERS_MAX		(MAX_CLIENTS)
#define WAITFORAFK_MINPLAYERS_DEFAULT	(6)
extern vmCvar_t		g_waitForAFK;
extern vmCvar_t		g_waitForAFKTimer;
extern vmCvar_t		g_waitForAFKThreshold;
extern vmCvar_t		g_waitForAFKMinPlayers;
extern vmCvar_t		g_printCountry;
extern vmCvar_t		g_redirectWrongThTeBinds;

int validateAccount(const char* username, const char* password, int num);
void unregisterUser(const char* username);
void loadAccounts();

#include "namespace_begin.h"

void	trap_Printf( const char *fmt );
void	trap_Error( const char *fmt );
int		trap_Milliseconds( void );
void	trap_PrecisionTimer_Start(void **theNewTimer);
int		trap_PrecisionTimer_End(void *theTimer);
int		trap_Argc( void );
void	trap_Argv( int n, char *buffer, int bufferLength );
void	trap_Args( char *buffer, int bufferLength );
int		trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void	trap_FS_Read( void *buffer, int len, fileHandle_t f );
void	trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void	trap_FS_FCloseFile( fileHandle_t f );
int		trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
void	trap_SendConsoleCommand( int exec_when, const char *text );
void	trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags );
void	trap_Cvar_Update( vmCvar_t *cvar );
void	trap_Cvar_Set( const char *var_name, const char *value );
int		trap_Cvar_VariableIntegerValue( const char *var_name );
float	trap_Cvar_VariableValue( const char *var_name );
void	trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
void	trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *gameClients, int sizeofGameClient );
void	trap_DropClient( int clientNum, const char *reason );
void	trap_SendServerCommand( int clientNum, const char *text );
void	trap_SetConfigstring( int num, const char *string );
void	trap_GetConfigstring( int num, char *buffer, int bufferSize );
void	trap_GetUserinfo( int num, char *buffer, int bufferSize );
void	trap_SetUserinfo( int num, const char *buffer );
void	trap_GetServerinfo( char *buffer, int bufferSize );
void	trap_SetServerCull(float cullDistance);
void	trap_SetBrushModel( gentity_t *ent, const char *name );
void	trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask );
void	trap_G2Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, int g2TraceType, int traceLod );
int		trap_PointContents( const vec3_t point, int passEntityNum );
qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 );
qboolean trap_InPVSIgnorePortals( const vec3_t p1, const vec3_t p2 );
void	trap_AdjustAreaPortalState( gentity_t *ent, qboolean open );
qboolean trap_AreasConnected( int area1, int area2 );


void	trap_LinkEntity( gentity_t *ent );
void	trap_UnlinkEntity( gentity_t *ent );



int		trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount );
qboolean trap_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t *ent );
int		trap_BotAllocateClient( void );
void	trap_BotFreeClient( int clientNum );
void	trap_GetUsercmd( int clientNum, usercmd_t *cmd );
qboolean	trap_GetEntityToken( char *buffer, int bufferSize );

//adding giant gamebreaking features post-alpha is fun!
void	trap_SiegePersSet(siegePers_t *pers);
void	trap_SiegePersGet(siegePers_t *pers);

#ifdef BOT_ZMALLOC
void	*trap_BotGetMemoryGame(int size);
void	trap_BotFreeMemoryGame(void *ptr);
#endif

int		trap_DebugPolygonCreate(int color, int numPoints, vec3_t *points);
void	trap_DebugPolygonDelete(int id);

int		trap_BotLibSetup( void );
int		trap_BotLibShutdown( void );
int		trap_BotLibVarSet(char *var_name, char *value);
int		trap_BotLibVarGet(char *var_name, char *value, int size);
int		trap_BotLibDefine(char *string);
int		trap_BotLibStartFrame(float time);
int		trap_BotLibLoadMap(const char *mapname);
int		trap_BotLibUpdateEntity(int ent, void /* struct bot_updateentity_s */ *bue);
int		trap_BotLibTest(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);

int		trap_BotGetSnapshotEntity( int clientNum, int sequence );
int		trap_BotGetServerCommand(int clientNum, char *message, int size);
void	trap_BotUserCommand(int client, usercmd_t *ucmd);

int		trap_AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, int *areas, int maxareas);
int		trap_AAS_AreaInfo( int areanum, void /* struct aas_areainfo_s */ *info );
void	trap_AAS_EntityInfo(int entnum, void /* struct aas_entityinfo_s */ *info);

int		trap_AAS_Initialized(void);
void	trap_AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs);
float	trap_AAS_Time(void);

int		trap_AAS_PointAreaNum(vec3_t point);
int		trap_AAS_PointReachabilityAreaIndex(vec3_t point);
int		trap_AAS_TraceAreas(vec3_t start, vec3_t end, int *areas, vec3_t *points, int maxareas);

int		trap_AAS_PointContents(vec3_t point);
int		trap_AAS_NextBSPEntity(int ent);
int		trap_AAS_ValueForBSPEpairKey(int ent, char *key, char *value, int size);
int		trap_AAS_VectorForBSPEpairKey(int ent, char *key, vec3_t v);
int		trap_AAS_FloatForBSPEpairKey(int ent, char *key, float *value);
int		trap_AAS_IntForBSPEpairKey(int ent, char *key, int *value);

int		trap_AAS_AreaReachability(int areanum);

int		trap_AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags);
int		trap_AAS_EnableRoutingArea( int areanum, int enable );
int		trap_AAS_PredictRoute(void /*struct aas_predictroute_s*/ *route, int areanum, vec3_t origin,
							int goalareanum, int travelflags, int maxareas, int maxtime,
							int stopevent, int stopcontents, int stoptfl, int stopareanum);

int		trap_AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags,
										void /*struct aas_altroutegoal_s*/ *altroutegoals, int maxaltroutegoals,
										int type);
int		trap_AAS_Swimming(vec3_t origin);
int		trap_AAS_PredictClientMovement(void /* aas_clientmove_s */ *move, int entnum, vec3_t origin, int presencetype, int onground, vec3_t velocity, vec3_t cmdmove, int cmdframes, int maxframes, float frametime, int stopevent, int stopareanum, int visualize);


void	trap_EA_Say(int client, char *str);
void	trap_EA_SayTeam(int client, char *str);
void	trap_EA_Command(int client, char *command);

void	trap_EA_Action(int client, int action);
void	trap_EA_Gesture(int client);
void	trap_EA_Talk(int client);
void	trap_EA_Attack(int client);
void	trap_EA_Use(int client);
void	trap_EA_Respawn(int client);
void	trap_EA_Crouch(int client);
void	trap_EA_MoveUp(int client);
void	trap_EA_MoveDown(int client);
void	trap_EA_MoveForward(int client);
void	trap_EA_MoveBack(int client);
void	trap_EA_MoveLeft(int client);
void	trap_EA_MoveRight(int client);
void	trap_EA_SelectWeapon(int client, int weapon);
void	trap_EA_Jump(int client);
void	trap_EA_DelayedJump(int client);
void	trap_EA_Move(int client, vec3_t dir, float speed);
void	trap_EA_View(int client, vec3_t viewangles);
void	trap_EA_Alt_Attack(int client);
void	trap_EA_ForcePower(int client);

void	trap_EA_EndRegular(int client, float thinktime);
void	trap_EA_GetInput(int client, float thinktime, void /* struct bot_input_s */ *input);
void	trap_EA_ResetInput(int client);


int		trap_BotLoadCharacter(char *charfile, float skill);
void	trap_BotFreeCharacter(int character);
float	trap_Characteristic_Float(int character, int index);
float	trap_Characteristic_BFloat(int character, int index, float min, float max);
int		trap_Characteristic_Integer(int character, int index);
int		trap_Characteristic_BInteger(int character, int index, int min, int max);
void	trap_Characteristic_String(int character, int index, char *buf, int size);

int		trap_BotAllocChatState(void);
void	trap_BotFreeChatState(int handle);
void	trap_BotQueueConsoleMessage(int chatstate, int type, char *message);
void	trap_BotRemoveConsoleMessage(int chatstate, int handle);
int		trap_BotNextConsoleMessage(int chatstate, void /* struct bot_consolemessage_s */ *cm);
int		trap_BotNumConsoleMessages(int chatstate);
void	trap_BotInitialChat(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 );
int		trap_BotNumInitialChats(int chatstate, char *type);
int		trap_BotReplyChat(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7 );
int		trap_BotChatLength(int chatstate);
void	trap_BotEnterChat(int chatstate, int client, int sendto);
void	trap_BotGetChatMessage(int chatstate, char *buf, int size);
int		trap_StringContains(char *str1, char *str2, int casesensitive);
int		trap_BotFindMatch(char *str, void /* struct bot_match_s */ *match, unsigned long int context);
void	trap_BotMatchVariable(void /* struct bot_match_s */ *match, int variable, char *buf, int size);
void	trap_UnifyWhiteSpaces(char *string);
void	trap_BotReplaceSynonyms(char *string, unsigned long int context);
int		trap_BotLoadChatFile(int chatstate, char *chatfile, char *chatname);
void	trap_BotSetChatGender(int chatstate, int gender);
void	trap_BotSetChatName(int chatstate, char *name, int client);
void	trap_BotResetGoalState(int goalstate);
void	trap_BotRemoveFromAvoidGoals(int goalstate, int number);
void	trap_BotResetAvoidGoals(int goalstate);
void	trap_BotPushGoal(int goalstate, void /* struct bot_goal_s */ *goal);
void	trap_BotPopGoal(int goalstate);
void	trap_BotEmptyGoalStack(int goalstate);
void	trap_BotDumpAvoidGoals(int goalstate);
void	trap_BotDumpGoalStack(int goalstate);
void	trap_BotGoalName(int number, char *name, int size);
int		trap_BotGetTopGoal(int goalstate, void /* struct bot_goal_s */ *goal);
int		trap_BotGetSecondGoal(int goalstate, void /* struct bot_goal_s */ *goal);
int		trap_BotChooseLTGItem(int goalstate, vec3_t origin, int *inventory, int travelflags);
int		trap_BotChooseNBGItem(int goalstate, vec3_t origin, int *inventory, int travelflags, void /* struct bot_goal_s */ *ltg, float maxtime);
int		trap_BotTouchingGoal(vec3_t origin, void /* struct bot_goal_s */ *goal);
int		trap_BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, void /* struct bot_goal_s */ *goal);
int		trap_BotGetNextCampSpotGoal(int num, void /* struct bot_goal_s */ *goal);
int		trap_BotGetMapLocationGoal(char *name, void /* struct bot_goal_s */ *goal);
int		trap_BotGetLevelItemGoal(int index, char *classname, void /* struct bot_goal_s */ *goal);
float	trap_BotAvoidGoalTime(int goalstate, int number);
void	trap_BotSetAvoidGoalTime(int goalstate, int number, float avoidtime);
void	trap_BotInitLevelItems(void);
void	trap_BotUpdateEntityItems(void);
int		trap_BotLoadItemWeights(int goalstate, char *filename);
void	trap_BotFreeItemWeights(int goalstate);
void	trap_BotInterbreedGoalFuzzyLogic(int parent1, int parent2, int child);
void	trap_BotSaveGoalFuzzyLogic(int goalstate, char *filename);
void	trap_BotMutateGoalFuzzyLogic(int goalstate, float range);
int		trap_BotAllocGoalState(int state);
void	trap_BotFreeGoalState(int handle);

void	trap_BotResetMoveState(int movestate);
void	trap_BotMoveToGoal(void /* struct bot_moveresult_s */ *result, int movestate, void /* struct bot_goal_s */ *goal, int travelflags);
int		trap_BotMoveInDirection(int movestate, vec3_t dir, float speed, int type);
void	trap_BotResetAvoidReach(int movestate);
void	trap_BotResetLastAvoidReach(int movestate);
int		trap_BotReachabilityArea(vec3_t origin, int testground);
int		trap_BotMovementViewTarget(int movestate, void /* struct bot_goal_s */ *goal, int travelflags, float lookahead, vec3_t target);
int		trap_BotPredictVisiblePosition(vec3_t origin, int areanum, void /* struct bot_goal_s */ *goal, int travelflags, vec3_t target);
int		trap_BotAllocMoveState(void);
void	trap_BotFreeMoveState(int handle);
void	trap_BotInitMoveState(int handle, void /* struct bot_initmove_s */ *initmove);
void	trap_BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type);

int		trap_BotChooseBestFightWeapon(int weaponstate, int *inventory);
void	trap_BotGetWeaponInfo(int weaponstate, int weapon, void /* struct weaponinfo_s */ *weaponinfo);
int		trap_BotLoadWeaponWeights(int weaponstate, char *filename);
int		trap_BotAllocWeaponState(void);
void	trap_BotFreeWeaponState(int weaponstate);
void	trap_BotResetWeaponState(int weaponstate);

int		trap_GeneticParentsAndChildSelection(int numranks, float *ranks, int *parent1, int *parent2, int *child);

void	trap_SnapVector( float *v );

int trap_SP_GetStringTextString(const char *text, char *buffer, int bufferLength);

qboolean	trap_ROFF_Clean( void );
void		trap_ROFF_UpdateEntities( void );
int			trap_ROFF_Cache( char *file );
qboolean	trap_ROFF_Play( int entID, int roffID, qboolean doTranslation );
qboolean	trap_ROFF_Purge_Ent( int entID );

//rww - dynamic vm memory allocation!
void		trap_TrueMalloc(void **ptr, int size);
void		trap_TrueFree(void **ptr);

//rww - icarus traps
int			trap_ICARUS_RunScript( gentity_t *ent, const char *name );
qboolean	trap_ICARUS_RegisterScript( const char *name, qboolean bCalledDuringInterrogate);

void		trap_ICARUS_Init( void );
qboolean	trap_ICARUS_ValidEnt( gentity_t *ent );
qboolean	trap_ICARUS_IsInitialized( int entID );
qboolean	trap_ICARUS_MaintainTaskManager( int entID );
qboolean	trap_ICARUS_IsRunning( int entID );
qboolean	trap_ICARUS_TaskIDPending(gentity_t *ent, int taskID);
void		trap_ICARUS_InitEnt( gentity_t *ent );
void		trap_ICARUS_FreeEnt( gentity_t *ent );
void		trap_ICARUS_AssociateEnt( gentity_t *ent );
void		trap_ICARUS_Shutdown( void );
void		trap_ICARUS_TaskIDSet(gentity_t *ent, int taskType, int taskID);
void		trap_ICARUS_TaskIDComplete(gentity_t *ent, int taskType);
void		trap_ICARUS_SetVar(int taskID, int entID, const char *type_name, const char *data);
int			trap_ICARUS_VariableDeclared(const char *type_name);
int			trap_ICARUS_GetFloatVariable( const char *name, float *value );
int			trap_ICARUS_GetStringVariable( const char *name, const char *value );
int			trap_ICARUS_GetVectorVariable( const char *name, const vec3_t value );


//rww - BEGIN NPC NAV TRAPS
void		trap_Nav_Init( void );
void		trap_Nav_Free( void );
qboolean	trap_Nav_Load( const char *filename, int checksum );
qboolean	trap_Nav_Save( const char *filename, int checksum );
int			trap_Nav_AddRawPoint( vec3_t point, int flags, int radius );
void		trap_Nav_CalculatePaths( qboolean recalc ); //recalc = qfalse
void		trap_Nav_HardConnect( int first, int second );

void		trap_Nav_ShowNodes( void );
void		trap_Nav_ShowEdges( void );
void		trap_Nav_ShowPath( int start, int end );
int			trap_Nav_GetNearestNode( gentity_t *ent, int lastID, int flags, int targetID );
int			trap_Nav_GetBestNode( int startID, int endID, int rejectID ); //rejectID = NODE_NONE
int			trap_Nav_GetNodePosition( int nodeID, vec3_t out );
int			trap_Nav_GetNodeNumEdges( int nodeID );
int			trap_Nav_GetNodeEdge( int nodeID, int edge );
int			trap_Nav_GetNumNodes( void );
qboolean	trap_Nav_Connected( int startID, int endID );
int			trap_Nav_GetPathCost( int startID, int endID );
int			trap_Nav_GetEdgeCost( int startID, int endID );
int			trap_Nav_GetProjectedNode( vec3_t origin, int nodeID );
void		trap_Nav_CheckFailedNodes( gentity_t *ent );
void		trap_Nav_AddFailedNode( gentity_t *ent, int nodeID );
qboolean	trap_Nav_NodeFailed( gentity_t *ent, int nodeID );
qboolean	trap_Nav_NodesAreNeighbors( int startID, int endID );
void		trap_Nav_ClearFailedEdge( failedEdge_t	*failedEdge );
void		trap_Nav_ClearAllFailedEdges( void );
int			trap_Nav_EdgeFailed( int startID, int endID );
void		trap_Nav_AddFailedEdge( int entID, int startID, int endID );
qboolean	trap_Nav_CheckFailedEdge( failedEdge_t *failedEdge );
void		trap_Nav_CheckAllFailedEdges( void );
qboolean	trap_Nav_RouteBlocked( int startID, int testEdgeID, int endID, int rejectRank );
int			trap_Nav_GetBestNodeAltRoute( int startID, int endID, int *pathCost, int rejectID ); //rejectID = NODE_NONE
int			trap_Nav_GetBestNodeAltRoute2( int startID, int endID, int rejectID ); //rejectID = NODE_NONE
int			trap_Nav_GetBestPathBetweenEnts( gentity_t *ent, gentity_t *goal, int flags );
int			trap_Nav_GetNodeRadius( int nodeID );
void		trap_Nav_CheckBlockedEdges( void );
void		trap_Nav_ClearCheckedNodes( void );
int			trap_Nav_CheckedNode(int wayPoint, int ent); //return int was byte
void		trap_Nav_SetCheckedNode(int wayPoint, int ent, int value); //int value was byte value
void		trap_Nav_FlagAllNodes( int newFlag );
qboolean	trap_Nav_GetPathsCalculated(void);
void		trap_Nav_SetPathsCalculated(qboolean newVal);
//rww - END NPC NAV TRAPS

void		trap_SV_RegisterSharedMemory(char *memory);

void trap_SetActiveSubBSP(int index);
int	trap_CM_RegisterTerrain(const char *config);
void trap_RMG_Init(int terrainID);

void trap_Bot_UpdateWaypoints(int wpnum, wpobject_t **wps);
void trap_Bot_CalculatePaths(int rmg);

#ifdef NEW_TRAP_CALLS
// new base_enhanced trap calls
void trap_OutOfBandPrint( int clientNum, const char* text );
void trap_SetConfigstringNoUpdate( int num, const char* string );
qboolean trap_SendGETRequest( trsfHandle_t* handle, const char* url, const char* headerAccept, const char* headerContentType );
qboolean trap_SendPOSTRequest( trsfHandle_t* handle, const char* url, const char* data, const char* headerAccept, const char* headerContentType, qboolean receiveResult );
qboolean trap_SendMultipartPOSTRequest(trsfHandle_t* handle, const char* url, trsfFormPart_t* multiPart, size_t numParts, const char* headerAccept, const char* headerContentType, qboolean receiveResult);
void trap_GetCountry(const char *ipStr, char *outBuf, int outBufSize);
int trap_sqlite3_prepare_v2(void *unused, const char *zSql, int nBytes, void **ppStmt, const char **pzTail);
int trap_sqlite3_step(void *stmt);
int trap_sqlite3_finalize(void *stmt);
int trap_sqlite3_reset(void *stmt);
int trap_sqlite3_exec(void *unused, const char *sql, int (*callback)(void *, int, char **, char **), void *callbackarg, char **errmsg);
enhancedLocation_t *trap_kd_dataptr(int index);
int *trap_kd_numunique(void);
void trap_kd_create(void);
void trap_kd_free(void);
int trap_kd_insertf(const float *pos, void *data);
void *trap_kd_nearestf(const float *pos);
void trap_kd_res_free(void *set);
#endif

#include "namespace_end.h"

#endif
