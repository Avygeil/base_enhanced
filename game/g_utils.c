// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_utils.c -- misc utility functions for game module

#include "g_local.h"
#include "bg_saga.h"
#include "q_shared.h"

typedef struct {
  char oldShader[MAX_QPATH];
  char newShader[MAX_QPATH];
  float timeOffset;
} shaderRemap_t;

#define MAX_SHADER_REMAPS 128

int remapCount = 0;
shaderRemap_t remappedShaders[MAX_SHADER_REMAPS];

void AddRemap(const char *oldShader, const char *newShader, float timeOffset) {
	int i;

	for (i = 0; i < remapCount; i++) {
		if (Q_stricmp(oldShader, remappedShaders[i].oldShader) == 0) {
			// found it, just update this one
			strcpy(remappedShaders[i].newShader,newShader);
			remappedShaders[i].timeOffset = timeOffset;
			return;
		}
	}
	if (remapCount < MAX_SHADER_REMAPS) {
		strcpy(remappedShaders[remapCount].newShader,newShader);
		strcpy(remappedShaders[remapCount].oldShader,oldShader);
		remappedShaders[remapCount].timeOffset = timeOffset;
		remapCount++;
	}
}

const char *BuildShaderStateConfig(void) {
	static char	buff[MAX_STRING_CHARS*4];
	char out[(MAX_QPATH * 2) + 5];
	int i;
  
	memset(buff, 0, MAX_STRING_CHARS);
	for (i = 0; i < remapCount; i++) {
		Com_sprintf(out, (MAX_QPATH * 2) + 5, "%s=%s:%5.2f@", remappedShaders[i].oldShader, remappedShaders[i].newShader, remappedShaders[i].timeOffset);
		Q_strcat( buff, sizeof( buff ), out);
	}
	return buff;
}

/*
=========================================================================

model / sound configstring indexes

=========================================================================
*/

/*
================
G_FindConfigstringIndex

================
*/
static int G_FindConfigstringIndex( const char *name, int start, int max, qboolean create ) {
	int		i;
	char	s[MAX_STRING_CHARS];

	if ( !name || !name[0] ) {
		return 0;
	}

	for ( i=1 ; i<max ; i++ ) {
		trap_GetConfigstring( start + i, s, sizeof( s ) );
		if ( !s[0] ) {
			break;
		}
		if ( !strcmp( s, name ) ) {
			return i;
		}
	}

	if ( !create ) {
		return 0;
	}

	if ( i == max ) {
		G_Error( "G_FindConfigstringIndex: overflow" );
	}

	trap_SetConfigstring( start + i, name );

	return i;
}

/*
Ghoul2 Insert Start
*/

int G_BoneIndex( const char *name ) {
	return G_FindConfigstringIndex (name, CS_G2BONES, MAX_G2BONES, qtrue);
}
/*
Ghoul2 Insert End
*/

int G_ModelIndex( const char *name ) {
#ifdef _DEBUG_MODEL_PATH_ON_SERVER
	//debug to see if we are shoving data into configstrings for models that don't exist, and if
	//so, where we are doing it from -rww
	fileHandle_t fh;

	trap_FS_FOpenFile(name, &fh, FS_READ);
	if (!fh)
	{ //try models/ then, this is assumed for registering models
		trap_FS_FOpenFile(va("models/%s", name), &fh, FS_READ);
		if (!fh)
		{
			Com_Printf("ERROR: Server tried to modelindex %s but it doesn't exist.\n", name);
		}
	}

	if (fh)
	{
		trap_FS_FCloseFile(fh);
	}
#endif
	return G_FindConfigstringIndex (name, CS_MODELS, MAX_MODELS, qtrue);
}

int	G_IconIndex( const char* name ) 
{
	assert(name && name[0]);
	return G_FindConfigstringIndex (name, CS_ICONS, MAX_ICONS, qtrue);
}

int G_SoundIndex( const char *name ) {
	assert(name && name[0]);
	return G_FindConfigstringIndex (name, CS_SOUNDS, MAX_SOUNDS, qtrue);
}

int G_SoundSetIndex(const char *name)
{
	return G_FindConfigstringIndex (name, CS_AMBIENT_SET, MAX_AMBIENT_SETS, qtrue);
}

int G_EffectIndex( const char *name )
{
	return G_FindConfigstringIndex (name, CS_EFFECTS, MAX_FX, qtrue);
}

int G_BSPIndex( const char *name )
{
	return G_FindConfigstringIndex (name, CS_BSP_MODELS, MAX_SUB_BSP, qtrue);
}

//=====================================================================


//see if we can or should allow this guy to use a custom skeleton -rww
qboolean G_PlayerHasCustomSkeleton(gentity_t *ent)
{
		return qfalse;
	}

/*
================
G_TeamCommand

Broadcasts a command to only a specific team
================
*/
void G_TeamCommand( team_t team, char *cmd ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
			if ( level.clients[i].sess.sessionTeam == team ) {
				trap_SendServerCommand( i, va("%s", cmd ));
			}
		}
	}
}


/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the entity after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
gentity_t *G_Find (gentity_t *from, int fieldofs, const char *match)
{
	char	*s;

	if (!from)
		from = g_entities;
	else
		from++;

	for ( ; from < &g_entities[level.num_entities] ; from++)
	{
		if (!from->inuse)
			continue;
		s = *(char **) ((byte *)from + fieldofs);
		if (!s)
			continue;
		if (!Q_stricmp (s, match))
			return from;
	}

	return NULL;
}

/*
=============
G_FindClient

Find client that either matches client id or string pattern
after removing color code from the name.

=============
*/
gentity_t* G_FindClient( const char* pattern )
{
	int id = atoi( pattern );

	if ( !id && (pattern[0] < '0' || pattern[0] > '9') )
	{
		int i = 0;
		int foundid = 0;

		//argument isnt number
		//lets go through player list
		foundid = -1;
		for ( i = 0; i < level.maxclients; ++i )
		{
			if ( !g_entities[i].inuse || !g_entities[i].client )
				continue;

			if ( Q_stristrclean( g_entities[i].client->pers.netname, pattern ) )
			{
				if ( foundid != -1 )
				{//we already have one, ambigious then
					return NULL;
				}
				foundid = i;
			}
		}
		if ( foundid == -1 )
		{
			return NULL;
		}
		id = foundid;
	}

	if ( id < 0 || id > 31 ||
		(id >= 0 && (!g_entities[id].client || g_entities[id].client->pers.connected == CON_DISCONNECTED)) )
	{
		return NULL;
	}

	return &g_entities[id];
}	 

/*
============
G_RadiusList - given an origin and a radius, return all entities that are in use that are within the list
============
*/
int G_RadiusList ( vec3_t origin, float radius,	gentity_t *ignore, qboolean takeDamage, gentity_t *ent_list[MAX_GENTITIES])					  
{
	float		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	int			i, e;
	int			ent_count = 0;

	if ( radius < 1 ) 
	{
		radius = 1;
	}

	for ( i = 0 ; i < 3 ; i++ ) 
	{
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) 
	{
		ent = &g_entities[entityList[ e ]];

		if ((ent == ignore) || !(ent->inuse) || ent->takedamage != takeDamage)
			continue;

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) 
		{
			if ( origin[i] < ent->r.absmin[i] ) 
			{
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] ) 
			{
				v[i] = origin[i] - ent->r.absmax[i];
			} else 
			{
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) 
		{
			continue;
		}
		
		// ok, we are within the radius, add us to the incoming list
		ent_list[ent_count] = ent;
		ent_count++;

	}
	// we are done, return how many we found
	return(ent_count);
}


//----------------------------------------------------------
void G_Throw( gentity_t *targ, vec3_t newDir, float push )
//----------------------------------------------------------
{
	vec3_t	kvel;
	float	mass;

	if ( targ->physicsBounce > 0 )	//overide the mass
	{
		mass = targ->physicsBounce;
	}
	else
	{
		mass = 200;
	}

	if ( g_gravity.value > 0 )
	{
		VectorScale( newDir, g_knockback.value * (float)push / mass * 0.8, kvel );
		kvel[2] = newDir[2] * g_knockback.value * (float)push / mass * 1.5;
	}
	else
	{
		VectorScale( newDir, g_knockback.value * (float)push / mass, kvel );
	}

	if ( targ->client )
	{
		VectorAdd( targ->client->ps.velocity, kvel, targ->client->ps.velocity );
	}
	else if ( targ->s.pos.trType != TR_STATIONARY && targ->s.pos.trType != TR_LINEAR_STOP && targ->s.pos.trType != TR_NONLINEAR_STOP )
	{
		VectorAdd( targ->s.pos.trDelta, kvel, targ->s.pos.trDelta );
		VectorCopy( targ->r.currentOrigin, targ->s.pos.trBase );
		targ->s.pos.trTime = level.time;
	}

	// set the timer so that the other client can't cancel
	// out the movement immediately
	if ( targ->client && !targ->client->ps.pm_time ) 
	{
		int		t;

		t = push * 2;

		if ( t < 50 ) 
		{
			t = 50;
		}
		if ( t > 200 ) 
		{
			t = 200;
		}
		targ->client->ps.pm_time = t;
		targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	}
}

//methods of creating/freeing "fake" dynamically allocated client entity structures.
//You ABSOLUTELY MUST free this client after creating it before game shutdown. If
//it is left around you will have a memory leak, because true dynamic memory is
//allocated by the exe.
void G_FreeFakeClient(gclient_t **cl)
{ //or not, the dynamic stuff is busted somehow at the moment. Yet it still works in the test.
  //I think something is messed up in being able to cast the memory to stuff to modify it,
  //while modifying it directly seems to work fine.
}

//allocate a veh object
#define MAX_VEHICLES_AT_A_TIME		128
static Vehicle_t g_vehiclePool[MAX_VEHICLES_AT_A_TIME];
static qboolean g_vehiclePoolOccupied[MAX_VEHICLES_AT_A_TIME];
static qboolean g_vehiclePoolInit = qfalse;
void G_AllocateVehicleObject(Vehicle_t **pVeh)
{
	int i = 0;

	if (!g_vehiclePoolInit)
	{
		g_vehiclePoolInit = qtrue;
		memset(g_vehiclePoolOccupied, 0, sizeof(g_vehiclePoolOccupied));
	}

	while (i < MAX_VEHICLES_AT_A_TIME)
	{ //iterate through and try to find a free one
		if (!g_vehiclePoolOccupied[i])
		{
			g_vehiclePoolOccupied[i] = qtrue;
			memset(&g_vehiclePool[i], 0, sizeof(Vehicle_t));
			*pVeh = &g_vehiclePool[i];
			return;
		}
		i++;
	}
	Com_Error(ERR_DROP, "Ran out of vehicle pool slots.");
}

//free the pointer, sort of a lame method
void G_FreeVehicleObject(Vehicle_t *pVeh)
{
	int i = 0;
	while (i < MAX_VEHICLES_AT_A_TIME)
	{
		if (g_vehiclePoolOccupied[i] &&
			&g_vehiclePool[i] == pVeh)
		{ //guess this is it
			g_vehiclePoolOccupied[i] = qfalse;
			break;
		}
		i++;
	}
}

gclient_t *gClPtrs[MAX_GENTITIES];

void G_CreateFakeClient(int entNum, gclient_t **cl)
{
	if (!gClPtrs[entNum])
	{
		gClPtrs[entNum] = (gclient_t *) BG_Alloc(sizeof(gclient_t));
	}
	*cl = gClPtrs[entNum];
}

#ifdef _XBOX
void G_ClPtrClear(void)
{
	for(int i=0; i<MAX_GENTITIES; i++) {
		gClPtrs[i] = NULL;
	}
}
#endif

//call this on game shutdown to run through and get rid of all the lingering client pointers.
void G_CleanAllFakeClients(void)
{
	int i = MAX_CLIENTS; //start off here since all ents below have real client structs.
	gentity_t *ent;

	while (i < MAX_GENTITIES)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->s.eType == ET_NPC && ent->client)
		{
			G_FreeFakeClient(&ent->client);
		}
		i++;
	}
}

/*
=============
G_SetAnim

Finally reworked PM_SetAnim to allow non-pmove calls, so we take our
local anim index into account and make the call -rww
=============
*/
#include "namespace_begin.h"
void BG_SetAnim(playerState_t *ps, animation_t *animations, int setAnimParts,int anim,int setAnimFlags, int blendTime);
#include "namespace_end.h"

void G_SetAnim(gentity_t *ent, usercmd_t *ucmd, int setAnimParts, int anim, int setAnimFlags, int blendTime)
{
#if 0 //old hackish way
	pmove_t pmv;

	assert(ent && ent->inuse && ent->client);

	memset (&pmv, 0, sizeof(pmv));
	pmv.ps = &ent->client->ps;
	pmv.animations = bgAllAnims[ent->localAnimIndex].anims;
	if (!ucmd)
	{
		pmv.cmd = ent->client->pers.cmd;
	}
	else
	{
		pmv.cmd = *ucmd;
	}
	pmv.trace = trap_Trace;
	pmv.pointcontents = trap_PointContents;
	pmv.gametype = g_gametype.integer;

	//don't need to bother with ghoul2 stuff, it's not even used in PM_SetAnim.
	pm = &pmv;
	PM_SetAnim(setAnimParts, anim, setAnimFlags, blendTime);
#else //new clean and shining way!
	assert(ent->client);
    BG_SetAnim(&ent->client->ps, bgAllAnims[ent->localAnimIndex].anims, setAnimParts,
		anim, setAnimFlags, blendTime);
#endif
}


/*
=============
G_PickTarget

Selects a random entity from among the targets
=============
*/
#define MAXCHOICES	32

gentity_t *G_PickTarget (char *targetname)
{
	gentity_t	*ent = NULL;
	int		num_choices = 0;
	gentity_t	*choice[MAXCHOICES];

	if (!targetname)
	{
		G_Printf("G_PickTarget called with NULL targetname\n");
		return NULL;
	}

	while(1)
	{
		ent = G_Find (ent, FOFS(targetname), targetname);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if (num_choices == MAXCHOICES)
			break;
	}

	if (!num_choices)
	{
		G_Printf("G_PickTarget: target %s not found\n", targetname);
		return NULL;
	}

	return choice[rand() % num_choices];
}

void GlobalUse(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	if (!self || (self->flags & FL_INACTIVE))
	{
		return;
	}

	if (!self->use)
	{
		return;
	}
	self->use(self, other, activator);
}

void G_UseTargets2( gentity_t *ent, gentity_t *activator, const char *string ) {
	gentity_t		*t;
	
	if ( !ent ) {
		return;
	}

	if (ent->targetShaderName && ent->targetShaderNewName) {
		float f = level.time * 0.001;
		AddRemap(ent->targetShaderName, ent->targetShaderNewName, f);
		trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
	}

	if ( !string || !string[0] ) {
		return;
	}

	t = NULL;
	while ( (t = G_Find (t, FOFS(targetname), string)) != NULL ) {
		if ( t == ent ) {
			G_Printf ("WARNING: Entity used itself.\n");
		} else {
			if ( t->use ) {
				GlobalUse(t, ent, activator);
			}
		}
		if ( !ent->inuse ) {
			G_Printf("entity was removed while using targets\n");
			return;
		}
	}
}
/*
==============================
G_UseTargets

"activator" should be set to the entity that initiated the firing.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets( gentity_t *ent, gentity_t *activator )
{
	if (!ent)
	{
		return;
	}
	G_UseTargets2(ent, activator, ent->target);
}


/*
=============
TempVector

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float	*tv( float x, float y, float z ) {
	static	int		index;
	static	vec3_t	vecs[8];
	float	*v;

	// use an array so that multiple tempvectors won't collide
	// for a while
	v = vecs[index];
	index = (index + 1)&7;

	v[0] = x;
	v[1] = y;
	v[2] = z;

	return v;
}


/*
=============
VectorToString

This is just a convenience function
for printing vectors
=============
*/
char	*vtos( const vec3_t v ) {
	static	int		index;
	static	char	str[8][32];
	char	*s;

	// use an array so that multiple vtos won't collide
	s = str[index];
	index = (index + 1)&7;

	Com_sprintf (s, 32, "(%i %i %i)", (int)v[0], (int)v[1], (int)v[2]);

	return s;
}


/*
===============
G_SetMovedir

The editor only specifies a single value for angles (yaw),
but we have special constants to generate an up or down direction.
Angles will be cleared, because it is being used to represent a direction
instead of an orientation.
===============
*/
void G_SetMovedir( vec3_t angles, vec3_t movedir ) {
	static vec3_t VEC_UP		= {0, -1, 0};
	static vec3_t MOVEDIR_UP	= {0, 0, 1};
	static vec3_t VEC_DOWN		= {0, -2, 0};
	static vec3_t MOVEDIR_DOWN	= {0, 0, -1};

	if ( VectorCompare (angles, VEC_UP) ) {
		VectorCopy (MOVEDIR_UP, movedir);
	} else if ( VectorCompare (angles, VEC_DOWN) ) {
		VectorCopy (MOVEDIR_DOWN, movedir);
	} else {
		AngleVectors (angles, movedir, NULL, NULL);
	}
	VectorClear( angles );
}

void G_InitGentity( gentity_t *e ) {
	if (e->freeAfterEvent){
		G_LogPrintf("ERROR: Ent %i reused with bad freeAfterEvent! inuse:%i classname:%s saberent:%i item:%i\n",
			e->s.number,e->inuse,e->classname,e->isSaberEntity,e->item);
		G_LogPrintf("ERROR: s etype:%i event:%i eventparm:%i eflags:%i\n",
			e->s.eType,e->s.event,e->s.eventParm,e->s.eFlags);
		if (e->item){
			G_LogPrintf("ERROR: item gitag:%i gitype:%i classname:%s\n",
			e->item->giTag,e->item->giType,e->item->classname);
		}
		
		e->freeAfterEvent = qfalse;
	}	

	e->inuse = qtrue;
	e->classname = "noclass";
	e->s.number = e - g_entities;
	e->r.ownerNum = ENTITYNUM_NONE;
	e->s.modelGhoul2 = 0; //assume not

	trap_ICARUS_FreeEnt( e );	//ICARUS information must be added after this point
}

//give us some decent info on all the active ents -rww
static void G_SpewEntList(void)
{
	int i = 0;
	int numNPC = 0;
	int numProjectile = 0;
	int numTempEnt = 0;
	int numTempEntST = 0;
	char className[MAX_STRING_CHARS];
	gentity_t *ent;
	char *str;
#ifdef FINAL_BUILD
	#define VM_OR_FINAL_BUILD
#elif defined Q3_VM
	#define VM_OR_FINAL_BUILD
#endif

#ifndef VM_OR_FINAL_BUILD
	fileHandle_t fh;
	trap_FS_FOpenFile("entspew.txt", &fh, FS_WRITE);
#endif

	while (i < ENTITYNUM_MAX_NORMAL)
	{
		ent = &g_entities[i];
		if (ent->inuse)
		{
			if (ent->s.eType == ET_NPC)
			{
				numNPC++;
			}
			else if (ent->s.eType == ET_MISSILE)
			{
				numProjectile++;
			}
			else if (ent->freeAfterEvent)
			{
				numTempEnt++;
				if (ent->s.eFlags & EF_SOUNDTRACKER)
				{
					numTempEntST++;
				}

				str = va("TEMPENT %4i: EV %i\n", ent->s.number, ent->s.eType-ET_EVENTS);
				Com_Printf(str);
#ifndef VM_OR_FINAL_BUILD
				if (fh)
				{
					trap_FS_Write(str, strlen(str), fh);
				}
#endif
			}

			if (ent->classname && ent->classname[0])
			{
				strcpy(className, ent->classname);
			}
			else
			{
				strcpy(className, "Unknown");
			}
			str = va("ENT %4i: Classname %s\n", ent->s.number, className);
			Com_Printf(str);
#ifndef VM_OR_FINAL_BUILD
			if (fh)
			{
				trap_FS_Write(str, strlen(str), fh);
			}
#endif
		}

		i++;
	}

	str = va("TempEnt count: %i \nTempEnt ST: %i \nNPC count: %i \nProjectile count: %i \n", numTempEnt, numTempEntST, numNPC, numProjectile);
	Com_Printf(str);
#ifndef VM_OR_FINAL_BUILD
	if (fh)
	{
		trap_FS_Write(str, strlen(str), fh);
		trap_FS_FCloseFile(fh);
	}
#endif
}

/*
=================
G_Spawn

Either finds a free entity, or allocates a new one.

  The slots from 0 to MAX_CLIENTS-1 are always reserved for clients, and will
never be used by anything else.

Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
gentity_t *G_Spawn( void ) {
	int			i, force;
	gentity_t	*e;

	e = NULL;	// shut up warning
	i = 0;		// shut up warning
	for ( force = 0 ; force < 2 ; force++ ) {
		// if we go through all entities and can't find one to free,
		// override the normal minimum times before use
		e = &g_entities[MAX_CLIENTS];
		for ( i = MAX_CLIENTS ; i<level.num_entities ; i++, e++) {

			if ( e->inuse ) {
				continue;
			}

			// the first couple seconds of server time can involve a lot of
			// freeing and allocating, so relax the replacement policy
			if ( !force && e->freetime > level.startTime + 2000 && level.time - e->freetime < 1000 )
			{
				continue;
			}

			// reuse this slot
			G_InitGentity( e );
			return e;
		}
		if ( i != MAX_GENTITIES ) {
			break;
		}
	}

	//first try to get rid of projectiles before shutting the server down
	if (i == ENTITYNUM_MAX_NORMAL){ 
		e = &g_entities[MAX_CLIENTS];
		for ( i = MAX_CLIENTS ; i<level.num_entities ; i++, e++) {
			if ( e->s.eType == ET_MISSILE && !e->neverFree ) {
				G_FreeEntity(e);
				G_InitGentity( e );
				return e;
			}			
		}
	}


	if ( i == ENTITYNUM_MAX_NORMAL ) {
		G_SpewEntList();
		G_Error( "G_Spawn: no free entities" );
	}
	
	// open up a new slot
	level.num_entities++;

	// let the server system know that there are more entities
	trap_LocateGameData( level.gentities, level.num_entities, sizeof( gentity_t ), 
		&level.clients[0].ps, sizeof( level.clients[0] ) );

	G_InitGentity( e );
	return e;
}

/*
=================
G_EntitiesFree
=================
*/
qboolean G_EntitiesFree( void ) {
	int			i;
	gentity_t	*e;

	e = &g_entities[MAX_CLIENTS];
	for ( i = MAX_CLIENTS; i < level.num_entities; i++, e++) {
		if ( e->inuse ) {
			continue;
		}
		// slot available
		return qtrue;
	}
	return qfalse;
}

#define MAX_G2_KILL_QUEUE 256

int gG2KillIndex[MAX_G2_KILL_QUEUE];
int gG2KillNum = 0;

void G_SendG2KillQueue(void)
{
	char g2KillString[1024];
	int i = 0;
	
	if (!gG2KillNum)
	{
		return;
	}

	Com_sprintf(g2KillString, 1024, "kg2");

	while (i < gG2KillNum && i < 64)
	{ //send 64 at once, max...
		Q_strcat(g2KillString, 1024, va(" %i", gG2KillIndex[i]));
		i++;
	}

	trap_SendServerCommand(-1, g2KillString);

	//Clear the count because we just sent off the whole queue
	gG2KillNum -= i;
	if (gG2KillNum < 0)
	{ //hmm, should be impossible, but I'm paranoid as we're far past beta.
		assert(0);
		gG2KillNum = 0;
	}
}

void G_KillG2Queue(int entNum)
{
	if (gG2KillNum >= MAX_G2_KILL_QUEUE)
	{ //This would be considered a Bad Thing.
#ifdef _DEBUG
		Com_Printf("WARNING: Exceeded the MAX_G2_KILL_QUEUE count for this frame!\n");
#endif
		//Since we're out of queue slots, just send it now as a seperate command (eats more bandwidth, but we have no choice)
		trap_SendServerCommand(-1, va("kg2 %i", entNum));
		return;
	}

	gG2KillIndex[gG2KillNum] = entNum;
	gG2KillNum++;
}

/*
=================
G_FreeEntity

Marks the entity as free
=================
*/
void G_FreeEntity( gentity_t *ed ) {

	if (ed->isSaberEntity)
	{
#ifdef _DEBUG
		Com_Printf("Tried to remove JM saber!\n");
#endif
		return;
	}

	trap_UnlinkEntity (ed);		// unlink from world

	trap_ICARUS_FreeEnt( ed );	//ICARUS information must be added after this point

	if ( ed->neverFree ) {
		return;
	}

	//rww - this may seem a bit hackish, but unfortunately we have no access
	//to anything ghoul2-related on the server and thus must send a message
	//to let the client know he needs to clean up all the g2 stuff for this
	//now-removed entity
	if (ed->s.modelGhoul2)
	{ //force all clients to accept an event to destroy this instance, right now
		//Or not. Events can be dropped, so that would be a bad thing.
		G_KillG2Queue(ed->s.number);
	}

	//And, free the server instance too, if there is one.
	if (ed->ghoul2)
	{
		trap_G2API_CleanGhoul2Models(&(ed->ghoul2));
	}

	if (ed->s.eType == ET_NPC && ed->m_pVehicle)
	{ //tell the "vehicle pool" that this one is now free
		G_FreeVehicleObject(ed->m_pVehicle);
	}

	if (ed->s.eType == ET_NPC && ed->client)
	{ //this "client" structure is one of our dynamically allocated ones, so free the memory
		int saberEntNum = -1;
		int i = 0;
		if (ed->client->ps.saberEntityNum)
		{
			saberEntNum = ed->client->ps.saberEntityNum;
		}
		else if (ed->client->saberStoredIndex)
		{
			saberEntNum = ed->client->saberStoredIndex;
		}

		if (saberEntNum > 0 && g_entities[saberEntNum].inuse)
		{
			g_entities[saberEntNum].neverFree = qfalse;
			G_FreeEntity(&g_entities[saberEntNum]);
		}

		while (i < MAX_SABERS)
		{
			if (ed->client->weaponGhoul2[i] && trap_G2_HaveWeGhoul2Models(ed->client->weaponGhoul2[i]))
			{
				trap_G2API_CleanGhoul2Models(&ed->client->weaponGhoul2[i]);
			}
			i++;
		}

		G_FreeFakeClient(&ed->client);
	}

	if (ed->s.eFlags & EF_SOUNDTRACKER)
	{
		int i = 0;
		gentity_t *ent;

		while (i < MAX_CLIENTS)
		{
			ent = &g_entities[i];

			if (ent && ent->inuse && ent->client)
			{
				int ch = TRACK_CHANNEL_NONE-50;

				while (ch < NUM_TRACK_CHANNELS-50)
				{
					if (ent->client->ps.fd.killSoundEntIndex[ch] == ed->s.number)
					{
						ent->client->ps.fd.killSoundEntIndex[ch] = 0;
					}

					ch++;
				}
			}

			i++;
		}

		//make sure clientside loop sounds are killed on the tracker and client
		trap_SendServerCommand(-1, va("kls %i %i", ed->s.trickedentindex, ed->s.number));
	}

	memset (ed, 0, sizeof(*ed));
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->inuse = qfalse;
}

/*
=================
G_TempEntity

Spawns an event entity that will be auto-removed
The origin will be snapped to save net bandwidth, so care
must be taken if the origin is right on a surface (snap towards start vector first)
=================
*/
gentity_t *G_TempEntity( vec3_t origin, int event ) {
	gentity_t		*e;
	vec3_t		snapped;

	e = G_Spawn();
	e->s.eType = ET_EVENTS + event;

	e->classname = "tempEntity";
	e->eventTime = level.time;
	e->freeAfterEvent = qtrue;

	VectorCopy( origin, snapped );
	SnapVector( snapped );		// save network bandwidth
	G_SetOrigin( e, snapped );
	//WTF?  Why aren't we setting the s.origin? (like below)
	//cg_events.c code checks origin all over the place!!!
	//Trying to save bandwidth...?

	// find cluster for PVS
	trap_LinkEntity( e );

	return e;
}


/*
=================
G_SoundTempEntity

Special event entity that keeps sound trackers in mind
=================
*/
gentity_t *G_SoundTempEntity( vec3_t origin, int event, int channel ) {
	gentity_t		*e;
	vec3_t		snapped;

	e = G_Spawn();

	e->s.eType = ET_EVENTS + event;
	e->inuse = qtrue;

	e->classname = "tempEntity";
	e->eventTime = level.time;
	e->freeAfterEvent = qtrue;

	VectorCopy( origin, snapped );
	SnapVector( snapped );		// save network bandwidth
	G_SetOrigin( e, snapped );

	// find cluster for PVS
	trap_LinkEntity( e );

	return e;
}


//scale health down below 1024 to fit in health bits
void G_ScaleNetHealth(gentity_t *self)
{
	int maxHealth = self->maxHealth;

    if (maxHealth < 1000)
	{ //it's good then
		self->s.maxhealth = maxHealth;
		self->s.health = self->health;

		if (self->s.health < 0)
		{ //don't let it wrap around
			self->s.health = 0;
		}
		return;
	}

	//otherwise, scale it down
	self->s.maxhealth = (maxHealth/100);
	self->s.health = (self->health/100);

	if (self->s.health < 0)
	{ //don't let it wrap around
		self->s.health = 0;
	}

	if (self->health > 0 &&
		self->s.health <= 0)
	{ //don't let it scale to 0 if the thing is still not "dead"
		self->s.health = 1;
	}
}



/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
G_KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
void G_KillBox (gentity_t *ent) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;

	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++) {
		hit = &g_entities[touch[i]];
		if ( !hit->client ) {
			continue;
		}

		if ( hit->client->sess.inRacemode )
		{ // don't telefrag racers
			continue;
		}

		if (hit->isAimPracticePack)
			continue;

		if (hit->s.number == ent->s.number)
		{ //don't telefrag yourself!
			continue;
		}

		if (ent->r.ownerNum == hit->s.number)
		{ //don't telefrag your vehicle!
			continue;
		}

		// nail it
		if (!g_strafejump_mod.integer)
			G_Damage ( hit, ent, ent, NULL, NULL,
				100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
	}

}

//==============================================================================

/*
===============
G_AddPredictableEvent

Use for non-pmove events that would also be predicted on the
client side: jumppads and item pickups
Adds an event+parm and twiddles the event counter
===============
*/
void G_AddPredictableEvent( gentity_t *ent, int event, int eventParm ) {
	if ( !ent->client ) {
		return;
	}
	BG_AddPredictableEventToPlayerstate( event, eventParm, &ent->client->ps );
}


/*
===============
G_AddEvent

Adds an event+parm and twiddles the event counter
===============
*/
void G_AddEvent( gentity_t *ent, int event, int eventParm ) {
	int		bits;

	if ( !event ) {
		G_Printf( "G_AddEvent: zero event added for entity %i\n", ent->s.number );
		return;
	}

	if (!ent->inuse){
		G_LogPrintf( "ERROR: event %i set for non-used entity %i\n", event, ent->s.number );
	}

	// clients need to add the event in playerState_t instead of entityState_t
	if ( ent->client ) {
		bits = ent->client->ps.externalEvent & EV_EVENT_BITS;
		bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
		ent->client->ps.externalEvent = event | bits;
		ent->client->ps.externalEventParm = eventParm;
		ent->client->ps.externalEventTime = level.time;
	} else {
		bits = ent->s.event & EV_EVENT_BITS;
		bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
		ent->s.event = event | bits;
		ent->s.eventParm = eventParm;
	}
	ent->eventTime = level.time;
}

/*
=============
G_PlayEffect
=============
*/
gentity_t *G_PlayEffect(int fxID, vec3_t org, vec3_t ang)
{
	gentity_t	*te;

	te = G_TempEntity( org, EV_PLAY_EFFECT );
	VectorCopy(ang, te->s.angles);
	VectorCopy(org, te->s.origin);
	te->s.eventParm = fxID;

	return te;
}

/*
=============
G_PlayEffectID
=============
*/
gentity_t *G_PlayEffectID(const int fxID, vec3_t org, vec3_t ang)
{ //play an effect by the G_EffectIndex'd ID instead of a predefined effect ID
	gentity_t	*te;

	te = G_TempEntity( org, EV_PLAY_EFFECT_ID );
	VectorCopy(ang, te->s.angles);
	VectorCopy(org, te->s.origin);
	te->s.eventParm = fxID;

	if (!te->s.angles[0] &&
		!te->s.angles[1] &&
		!te->s.angles[2])
	{ //play off this dir by default then.
		te->s.angles[1] = 1;
	}

	return te;
}

/*
=============
G_ScreenShake
=============
*/
gentity_t *G_ScreenShake(vec3_t org, gentity_t *target, float intensity, int duration, qboolean global)
{
	gentity_t	*te;

	te = G_TempEntity( org, EV_SCREENSHAKE );
	VectorCopy(org, te->s.origin);
	te->s.angles[0] = intensity;
	te->s.time = duration;

	if (target)
	{
		te->s.modelindex = target->s.number+1;
	}
	else
	{
		te->s.modelindex = 0;
	}

	if (global)
	{
		te->r.svFlags |= SVF_BROADCAST;
	}

	return te;
}

/*
=============
G_MuteSound
=============
*/
void G_MuteSound( int entnum, int channel )
{
	gentity_t	*te, *e;

	te = G_TempEntity( vec3_origin, EV_MUTE_SOUND );
	te->r.svFlags = SVF_BROADCAST;
	te->s.trickedentindex2 = entnum;
	te->s.trickedentindex = channel;

	e = &g_entities[entnum];

	if (e && (e->s.eFlags & EF_SOUNDTRACKER))
	{
		G_FreeEntity(e);
		e->s.eFlags = 0;
	}
}

/*
=============
G_Sound
=============
*/
gentity_t* G_Sound( gentity_t *ent, int channel, int soundIndex ) {
	gentity_t	*te;

	assert(soundIndex);

	te = G_SoundTempEntity( ent->r.currentOrigin, EV_GENERAL_SOUND, channel );
	te->s.eventParm = soundIndex;
	te->s.saberEntityNum = channel;

	if (ent && ent->client && channel > TRACK_CHANNEL_NONE)
	{ //let the client remember the index of the player entity so he can kill the most recent sound on request
		if (g_entities[ent->client->ps.fd.killSoundEntIndex[channel-50]].inuse &&
			ent->client->ps.fd.killSoundEntIndex[channel-50] > MAX_CLIENTS)
		{
			G_MuteSound(ent->client->ps.fd.killSoundEntIndex[channel-50], CHAN_VOICE);
			if (ent->client->ps.fd.killSoundEntIndex[channel-50] > MAX_CLIENTS && g_entities[ent->client->ps.fd.killSoundEntIndex[channel-50]].inuse)
			{
				G_FreeEntity(&g_entities[ent->client->ps.fd.killSoundEntIndex[channel-50]]);
			}
			ent->client->ps.fd.killSoundEntIndex[channel-50] = 0;
		}

		ent->client->ps.fd.killSoundEntIndex[channel-50] = te->s.number;
		te->s.trickedentindex = ent->s.number;
		te->s.eFlags = EF_SOUNDTRACKER;
		//broudcast this entity, so people who havent met
		//this guy can still hear looping sound when they finally meet him
		te->r.svFlags |= SVF_BROADCAST;

	}

	G_ApplyRaceBroadcastsToEvent( ent, te );

	return te;
}

/*
=============
G_SoundAtLoc
=============
*/
void G_SoundAtLoc( vec3_t loc, int channel, int soundIndex ) {
	gentity_t	*te;

	te = G_TempEntity( loc, EV_GENERAL_SOUND );
	te->s.eventParm = soundIndex;
	te->s.saberEntityNum = channel;
}

/*
=============
G_EntitySound
=============
*/
void G_EntitySound( gentity_t *ent, int channel, int soundIndex ) {
	gentity_t	*te;

	te = G_TempEntity( ent->r.currentOrigin, EV_ENTITY_SOUND );
	te->s.eventParm = soundIndex;
	te->s.clientNum = ent->s.number;
	te->s.trickedentindex = channel;

	G_ApplyRaceBroadcastsToEvent( ent, te );
}

//To make porting from SP easier.
void G_SoundOnEnt( gentity_t *ent, int channel, const char *soundPath )
{
	gentity_t	*te;

	te = G_TempEntity( ent->r.currentOrigin, EV_ENTITY_SOUND );
	te->s.eventParm = G_SoundIndex((char *)soundPath);
	te->s.clientNum = ent->s.number;
	te->s.trickedentindex = channel;

	G_ApplyRaceBroadcastsToEvent( ent, te );
}

#ifdef _XBOX
//-----------------------------
void G_EntityPosition( int i, vec3_t ret )
{
	if ( /*g_entities &&*/ i >= 0 && i < MAX_GENTITIES && g_entities[i].inuse)
	{
#if 0	// VVFIXME - Do we really care about doing this? It's slow and unnecessary
		gentity_t *ent = g_entities + i;

		if (ent->bmodel)
		{
			vec3_t mins, maxs;
			clipHandle_t h = CM_InlineModel( ent->s.modelindex );
			CM_ModelBounds( cmg, h, mins, maxs );
			ret[0] = (mins[0] + maxs[0]) / 2 + ent->currentOrigin[0];
			ret[1] = (mins[1] + maxs[1]) / 2 + ent->currentOrigin[1];
			ret[2] = (mins[2] + maxs[2]) / 2 + ent->currentOrigin[2];
		}
		else
#endif
		{
			VectorCopy(g_entities[i].r.currentOrigin, ret);
		}
	}
	else
	{
		ret[0] = ret[1] = ret[2] = 0;
	}
}
#endif

//==============================================================================

/*
==============
ValidUseTarget

Returns whether or not the targeted entity is useable
==============
*/
qboolean ValidUseTarget( gentity_t *ent )
{
	if ( !ent->use )
	{
		return qfalse;
	}

	if ( ent->flags & FL_INACTIVE )
	{//set by target_deactivate
		return qfalse;
	}

	if ( !(ent->r.svFlags & SVF_PLAYER_USABLE) )
	{//Check for flag that denotes BUTTON_USE useability
		return qfalse;
	}

	return qtrue;
}

//use an ammo/health dispenser on another client
void G_UseDispenserOn(gentity_t *ent, int dispType, gentity_t *target)
{
	if (dispType == HI_HEALTHDISP)
	{
		target->client->ps.stats[STAT_HEALTH] += 4;

		if (target->client->ps.stats[STAT_HEALTH] > target->client->ps.stats[STAT_MAX_HEALTH])
		{
			target->client->ps.stats[STAT_HEALTH] = target->client->ps.stats[STAT_MAX_HEALTH];
		}

		target->client->isMedHealed = level.time + 500;
		target->health = target->client->ps.stats[STAT_HEALTH];
	}
	else if (dispType == HI_AMMODISP)
	{
		if (ent->client->medSupplyDebounce < level.time)
		{ //do the next increment
			//increment based on the amount of ammo used per normal shot.
			target->client->ps.ammo[weaponData[target->client->ps.weapon].ammoIndex] += weaponData[target->client->ps.weapon].energyPerShot;

			if (target->client->ps.ammo[weaponData[target->client->ps.weapon].ammoIndex] > ammoData[weaponData[target->client->ps.weapon].ammoIndex].max)
			{ //cap it off
				target->client->ps.ammo[weaponData[target->client->ps.weapon].ammoIndex] = ammoData[weaponData[target->client->ps.weapon].ammoIndex].max;
			}

			//base the next supply time on how long the weapon takes to fire. Seems fair enough.
			ent->client->medSupplyDebounce = level.time + weaponData[target->client->ps.weapon].fireTime;
		}
		target->client->isMedSupplied = level.time + 500;
	}
}

//see if this guy needs servicing from a specific type of dispenser
int G_CanUseDispOn(gentity_t *ent, int dispType)
{
	if (!ent->client || !ent->inuse || ent->health < 1 ||
		ent->client->ps.stats[STAT_HEALTH] < 1)
	{ //dead or invalid
		return 0;
	}

	if (dispType == HI_HEALTHDISP)
	{
        if (ent->client->ps.stats[STAT_HEALTH] < ent->client->ps.stats[STAT_MAX_HEALTH])
		{ //he's hurt
			return 1;
		}

		//otherwise no
		return 0;
	}
	else if (dispType == HI_AMMODISP)
	{
		if (ent->client->ps.weapon <= WP_NONE || ent->client->ps.weapon > LAST_USEABLE_WEAPON)
		{ //not a player-useable weapon
			return 0;
		}

		if (ent->client->ps.ammo[weaponData[ent->client->ps.weapon].ammoIndex] < ammoData[weaponData[ent->client->ps.weapon].ammoIndex].max)
		{ //needs more ammo for current weapon
			return 1;
		}

		//needs none
		return 0;
	}

	//invalid type?
	return 0;
}

qboolean TryHeal(gentity_t *ent, gentity_t *target)
{
	if (g_gametype.integer == GT_SIEGE && ent->client->siegeClass != -1 &&
		target && target->inuse && target->maxHealth && target->healingclass &&
		target->healingclass[0] && target->health > 0 && target->health < target->maxHealth)
	{ //it's not dead yet...
		siegeClass_t *scl = &bgSiegeClasses[ent->client->siegeClass];

		if (!Q_stricmp(scl->name, target->healingclass))
		{ //this thing can be healed by the class this player is using
			if (target->healingDebounce < level.time)
			{ //do the actual heal
				target->health += 10;
				if (target->health > target->maxHealth)
				{ //don't go too high
					target->health = target->maxHealth;
				}
				target->healingDebounce = level.time + target->healingrate;
				if (target->healingsound && target->healingsound[0])
				{ //play it
					if (target->s.solid == SOLID_BMODEL)
					{ //ok, well, just play it on the client then.
						G_Sound(ent, CHAN_AUTO, G_SoundIndex(target->healingsound));
					}
					else
					{
						G_Sound(target, CHAN_AUTO, G_SoundIndex(target->healingsound));
					}
				}

				//update net health for bar
				G_ScaleNetHealth(target);
				if (target->target_ent &&
					target->target_ent->maxHealth)
				{
					target->target_ent->health = target->health;
					G_ScaleNetHealth(target->target_ent);
				}
			}

			//keep them in the healing anim even when the healing debounce is not yet expired
			if (ent->client->ps.torsoAnim == BOTH_BUTTON_HOLD ||
				ent->client->ps.torsoAnim == BOTH_CONSOLE1)
			{ //extend the time
				ent->client->ps.torsoTimer = 500;
			}
			else
			{
				G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
			}

			return qtrue;
		}
	}

	return qfalse;
}

/*
==============
TryUse

Try and use an entity in the world, directly ahead of us
==============
*/

#define USE_DISTANCE	64.0f

extern void Touch_Button(gentity_t *ent, gentity_t *other, trace_t *trace );
extern qboolean gSiegeRoundBegun;
static vec3_t	playerMins = {-15, -15, DEFAULT_MINS_2};
static vec3_t	playerMaxs = {15, 15, DEFAULT_MAXS_2};
void TryUse( gentity_t *ent )
{
	gentity_t	*target;
	trace_t		trace;
	vec3_t		src, dest, vf;
	vec3_t		viewspot;

	if (g_gametype.integer == GT_SIEGE &&
		!gSiegeRoundBegun)
	{ //nothing can be used til the round starts.
		return;
	}

	if (!ent || !ent->client || (ent->client->ps.weaponTime > 0 && ent->client->ps.torsoAnim != BOTH_BUTTON_HOLD && ent->client->ps.torsoAnim != BOTH_CONSOLE1) || ent->health < 1 ||
		(ent->client->ps.pm_flags & PMF_FOLLOW) || ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
		(ent->client->ps.forceHandExtend != HANDEXTEND_NONE && ent->client->ps.forceHandExtend != HANDEXTEND_DRAGGING))
	{
		return;
	}

	if (ent->client->ps.emplacedIndex)
	{ //on an emplaced gun or using a vehicle, don't do anything when hitting use key
		return;
	}

	if (ent->s.number < MAX_CLIENTS && ent->client && ent->client->ps.m_iVehicleNum)
	{
		gentity_t *currentVeh = &g_entities[ent->client->ps.m_iVehicleNum];
		if (currentVeh->inuse && currentVeh->m_pVehicle)
		{
			Vehicle_t *pVeh = currentVeh->m_pVehicle;
			if (!pVeh->m_iBoarding)
			{
				pVeh->m_pVehicleInfo->Eject( pVeh, (bgEntity_t *)ent, qfalse );
			}
			return;
		}
	}

	if (ent->client->jetPackOn)
	{ //can't use anything else to jp is off
		goto tryJetPack;
	}

	if (ent->client->bodyGrabIndex != ENTITYNUM_NONE)
	{ //then hitting the use key just means let go
		if (ent->client->bodyGrabTime < level.time)
		{
			gentity_t *grabbed = &g_entities[ent->client->bodyGrabIndex];

			if (grabbed->inuse)
			{
				if (grabbed->client)
				{
					grabbed->client->ps.ragAttach = 0;
				}
				else
				{
					grabbed->s.ragAttach = 0;
				}
			}
			ent->client->bodyGrabIndex = ENTITYNUM_NONE;
			ent->client->bodyGrabTime = level.time + 1000;
		}
		return;
	}

	VectorCopy(ent->client->ps.origin, viewspot);
	viewspot[2] += ent->client->ps.viewheight;

	VectorCopy( viewspot, src );
	AngleVectors( ent->client->ps.viewangles, vf, NULL, NULL );

	VectorMA( src, USE_DISTANCE, vf, dest );

	//Trace ahead to find a valid target
	trap_Trace( &trace, src, vec3_origin, vec3_origin, dest, ent->s.number, MASK_OPAQUE|CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_ITEM|CONTENTS_CORPSE );
	
	//fixed bug when using client with slot 0, it skips needed part
	if ( trace.fraction == 1.0f || trace.entityNum == ENTITYNUM_NONE)
	{
		goto tryJetPack;
	}

	target = &g_entities[trace.entityNum];

//Enable for corpse dragging
#if 0
	if (target->inuse && target->s.eType == ET_BODY &&
		ent->client->bodyGrabTime < level.time)
	{ //then grab the body
		target->s.eFlags |= EF_RAG; //make sure it's in rag state
		if (!ent->s.number)
		{ //switch cl 0 and entitynum_none, so we can operate on the "if non-0" concept
			target->s.ragAttach = ENTITYNUM_NONE;
		}
		else
		{
			target->s.ragAttach = ent->s.number;
		}
		ent->client->bodyGrabTime = level.time + 1000;
		ent->client->bodyGrabIndex = target->s.number;
		return;
	}
#endif

	if (target && target->m_pVehicle && target->client &&
		target->s.NPC_class == CLASS_VEHICLE &&
		!ent->client->ps.zoomMode)
	{ //if target is a vehicle then perform appropriate checks
		Vehicle_t *pVeh = target->m_pVehicle;

		if (pVeh->m_pVehicleInfo)
		{
			if ( ent->r.ownerNum == target->s.number )
			{ //user is already on this vehicle so eject him
				pVeh->m_pVehicleInfo->Eject( pVeh, (bgEntity_t *)ent, qfalse );
			}
			else
			{ // Otherwise board this vehicle.
				if (g_gametype.integer < GT_TEAM ||
					!target->alliedTeam ||
					(target->alliedTeam == ent->client->sess.sessionTeam))
				{ //not belonging to a team, or client is on same team
					pVeh->m_pVehicleInfo->Board( pVeh, (bgEntity_t *)ent );
				}
			}
			//clear the damn button!
			ent->client->pers.cmd.buttons &= ~BUTTON_USE;
			return;
		}
	}

#if 0 //ye olde method
	if (ent->client->ps.stats[STAT_HOLDABLE_ITEM] > 0 &&
		bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giType == IT_HOLDABLE)
	{
		if (bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag == HI_HEALTHDISP ||
			bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag == HI_AMMODISP)
		{ //has a dispenser item selected
            if (target && target->client && target->health > 0 && OnSameTeam(ent, target) &&
				G_CanUseDispOn(target, bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag))
			{ //a live target that's on my team, we can use him
				G_UseDispenserOn(ent, bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag, target);

				//for now, we will use the standard use anim
				if (ent->client->ps.torsoAnim == BOTH_BUTTON_HOLD)
				{ //extend the time
					ent->client->ps.torsoTimer = 500;
				}
				else
				{
					G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
				}
				ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
				return;
			}
		}
	}
#else
    if ( ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_HEALTHDISP)) || (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_AMMODISP))) &&
		target && target->inuse && target->client && target->health > 0 && OnSameTeam(ent, target) &&
		(G_CanUseDispOn(target, HI_HEALTHDISP) || G_CanUseDispOn(target, HI_AMMODISP)) )
	{ //a live target that's on my team, we can use him
		if (G_CanUseDispOn(target, HI_HEALTHDISP))
		{
			G_UseDispenserOn(ent, HI_HEALTHDISP, target);
		}
		if (G_CanUseDispOn(target, HI_AMMODISP))
		{
			G_UseDispenserOn(ent, HI_AMMODISP, target);
		}

		//for now, we will use the standard use anim
		if (ent->client->ps.torsoAnim == BOTH_BUTTON_HOLD)
		{ //extend the time
			ent->client->ps.torsoTimer = 500;
		}
		else
		{
			G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
		return;
	}

#endif

	//Check for a use command
	if ( ValidUseTarget( target ) 
		&& (g_gametype.integer != GT_SIEGE 
			|| !target->alliedTeam 
			|| target->alliedTeam != ent->client->sess.sessionTeam 
			|| g_ff_objectives.integer) )
	{
		if (ent->client->ps.torsoAnim == BOTH_BUTTON_HOLD ||
			ent->client->ps.torsoAnim == BOTH_CONSOLE1)
		{ //extend the time
			ent->client->ps.torsoTimer = 500;
		}
		else
		{
			G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
		if ( target->touch == Touch_Button )
		{//pretend we touched it
			target->touch(target, ent, NULL);
		}
		else
		{
			GlobalUse(target, ent, ent);
		}
		return;
	}

	if (TryHeal(ent, target))
	{
		return;
	}

tryJetPack:
	//if we got here, we didn't actually use anything else, so try to toggle jetpack if we are in the air, or if it is already on
	if (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK))
	{
		if (ent->client->jetPackOn || ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
		{
			ItemUse_Jetpack(ent);
			return;
		}
	}

	if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_AMMODISP)) /*&&
		G_ItemUsable(&ent->client->ps, HI_AMMODISP)*/ )
	{ //if you used nothing, then try spewing out some ammo
		trace_t trToss;
		vec3_t fAng;
		vec3_t fwd;

		VectorSet(fAng, 0.0f, ent->client->ps.viewangles[YAW], 0.0f);
		AngleVectors(fAng, fwd, 0, 0);

        VectorMA(ent->client->ps.origin, 64.0f, fwd, fwd);		
		trap_Trace(&trToss, ent->client->ps.origin, playerMins, playerMaxs, fwd, ent->s.number, ent->clipmask);
		if (trToss.fraction == 1.0f && !trToss.allsolid && !trToss.startsolid)
		{
			ItemUse_UseDisp(ent, HI_AMMODISP);
			G_AddEvent(ent, EV_USE_ITEM0+HI_AMMODISP, 0);
			return;
		}
	}
}

qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs )
{
	int i;

	for(i = 0; i < 3; i++ )
	{
		if ( point[i] < mins[i] )
		{
			return qfalse;
		}
		if ( point[i] > maxs[i] )
		{
			return qfalse;
		}
	}

	return qtrue;
}

qboolean G_BoxInBounds( vec3_t point, vec3_t mins, vec3_t maxs, vec3_t boundsMins, vec3_t boundsMaxs )
{
	vec3_t boxMins;
	vec3_t boxMaxs;

	VectorAdd( point, mins, boxMins );
	VectorAdd( point, maxs, boxMaxs );

	if(boxMaxs[0]>boundsMaxs[0])
		return qfalse;

	if(boxMaxs[1]>boundsMaxs[1])
		return qfalse;

	if(boxMaxs[2]>boundsMaxs[2])
		return qfalse;

	if(boxMins[0]<boundsMins[0])
		return qfalse;

	if(boxMins[1]<boundsMins[1])
		return qfalse;

	if(boxMins[2]<boundsMins[2])
		return qfalse;

	//box is completely contained within bounds
	return qtrue;
}


void G_SetAngles( gentity_t *ent, vec3_t angles )
{
	VectorCopy( angles, ent->r.currentAngles );
	VectorCopy( angles, ent->s.angles );
	VectorCopy( angles, ent->s.apos.trBase );
}

qboolean G_ClearTrace( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int ignore, int clipmask )
{
	static	trace_t	tr;

	trap_Trace( &tr, start, mins, maxs, end, ignore, clipmask );

	if ( tr.allsolid || tr.startsolid || tr.fraction < 1.0 )
	{
		return qfalse;
	}

	return qtrue;
}

/*
================
G_SetOrigin

Sets the pos trajectory for a fixed position
================
*/
void G_SetOrigin( gentity_t *ent, vec3_t origin ) {
	VectorCopy( origin, ent->s.pos.trBase );
	ent->s.pos.trType = TR_STATIONARY;
	ent->s.pos.trTime = 0;
	ent->s.pos.trDuration = 0;
	VectorClear( ent->s.pos.trDelta );

	VectorCopy( origin, ent->r.currentOrigin );
}

qboolean G_CheckInSolid (gentity_t *self, qboolean fix)
{
	trace_t	trace;
	vec3_t	end, mins;

	VectorCopy(self->r.currentOrigin, end);
	end[2] += self->r.mins[2];
	VectorCopy(self->r.mins, mins);
	mins[2] = 0;

	trap_Trace(&trace, self->r.currentOrigin, mins, self->r.maxs, end, self->s.number, self->clipmask);
	if(trace.allsolid || trace.startsolid)
	{
		return qtrue;
	}
	
	if(trace.fraction < 1.0)
	{
		if(fix)
		{//Put them at end of trace and check again
			vec3_t	neworg;

			VectorCopy(trace.endpos, neworg);
			neworg[2] -= self->r.mins[2];
			G_SetOrigin(self, neworg);
			trap_LinkEntity(self);

			return G_CheckInSolid(self, qfalse);
		}
		else
		{
			return qtrue;
		}
	}
		
	return qfalse;
}

/*
================
DebugLine

  debug polygons only work when running a local game
  with r_debugSurface set to 2
================
*/
int DebugLine(vec3_t start, vec3_t end, int color) {
	vec3_t points[4], dir, cross, up = {0, 0, 1};
	float dot;

	VectorCopy(start, points[0]);
	VectorCopy(start, points[1]);
	VectorCopy(end, points[2]);
	VectorCopy(end, points[3]);


	VectorSubtract(end, start, dir);
	VectorNormalize(dir);
	dot = DotProduct(dir, up);
	if (dot > 0.99 || dot < -0.99) VectorSet(cross, 1, 0, 0);
	else CrossProduct(dir, up, cross);

	VectorNormalize(cross);

	VectorMA(points[0], 2, cross, points[0]);
	VectorMA(points[1], -2, cross, points[1]);
	VectorMA(points[2], -2, cross, points[2]);
	VectorMA(points[3], 2, cross, points[3]);

	return trap_DebugPolygonCreate(color, 4, points);
}

void G_ROFF_NotetrackCallback( gentity_t *cent, const char *notetrack)
{
	char type[256];
	int i = 0;
	int addlArg = 0;

	if (!cent || !notetrack)
	{
		return;
	}

	while (notetrack[i] && notetrack[i] != ' ')
	{
		type[i] = notetrack[i];
		i++;
	}

	type[i] = '\0';

	if (!i || !type[0])
	{
		return;
	}

	if (notetrack[i] == ' ')
	{
		addlArg = 1;
	}

	if (strcmp(type, "loop") == 0)
	{
		if (addlArg) //including an additional argument means reset to original position before loop
		{
			VectorCopy(cent->s.origin2, cent->s.pos.trBase);
			VectorCopy(cent->s.origin2, cent->r.currentOrigin);
			VectorCopy(cent->s.angles2, cent->s.apos.trBase);
			VectorCopy(cent->s.angles2, cent->r.currentAngles);
		}

		trap_ROFF_Play(cent->s.number, cent->roffid, qfalse);
	}
}

void G_SpeechEvent( gentity_t *self, int event )
{
	G_AddEvent(self, event, 0);
}

qboolean G_ExpandPointToBBox( vec3_t point, const vec3_t mins, const vec3_t maxs, int ignore, int clipmask )
{
	trace_t	tr;
	vec3_t	start, end;
	int i;

	VectorCopy( point, start );
	
	for ( i = 0; i < 3; i++ )
	{
		VectorCopy( start, end );
		end[i] += mins[i];
		trap_Trace( &tr, start, vec3_origin, vec3_origin, end, ignore, clipmask );
		if ( tr.allsolid || tr.startsolid )
		{
			return qfalse;
		}
		if ( tr.fraction < 1.0 )
		{
			VectorCopy( start, end );
			end[i] += maxs[i]-(mins[i]*tr.fraction);
			trap_Trace( &tr, start, vec3_origin, vec3_origin, end, ignore, clipmask );
			if ( tr.allsolid || tr.startsolid )
			{
				return qfalse;
			}
			if ( tr.fraction < 1.0 )
			{
				return qfalse;
			}
			VectorCopy( end, start );
		}
	}
	//expanded it, now see if it's all clear
	trap_Trace( &tr, start, mins, maxs, start, ignore, clipmask );
	if ( tr.allsolid || tr.startsolid )
	{
		return qfalse;
	}
	VectorCopy( start, point );
	return qtrue;
}

extern qboolean G_FindClosestPointOnLineSegment( const vec3_t start, const vec3_t end, const vec3_t from, vec3_t result );
float ShortestLineSegBewteen2LineSegs( vec3_t start1, vec3_t end1, vec3_t start2, vec3_t end2, vec3_t close_pnt1, vec3_t close_pnt2 )
{
	float	current_dist, new_dist;
	vec3_t	new_pnt;
	//start1, end1 : the first segment
	//start2, end2 : the second segment

	//output, one point on each segment, the closest two points on the segments.

	//compute some temporaries:
	//vec start_dif = start2 - start1
	vec3_t	start_dif;
	vec3_t	v1;
	vec3_t	v2;
	float v1v1, v2v2, v1v2;
	float denom;

	VectorSubtract( start2, start1, start_dif );
	//vec v1 = end1 - start1
	VectorSubtract( end1, start1, v1 );
	//vec v2 = end2 - start2
	VectorSubtract( end2, start2, v2 );
	//
	v1v1 = DotProduct( v1, v1 );
	v2v2 = DotProduct( v2, v2 );
	v1v2 = DotProduct( v1, v2 );

	//the main computation

	denom = (v1v2 * v1v2) - (v1v1 * v2v2);

	//if denom is small, then skip all this and jump to the section marked below
	if ( fabs(denom) > 0.001f )
	{
		float s = -( (v2v2*DotProduct( v1, start_dif )) - (v1v2*DotProduct( v2, start_dif )) ) / denom;
		float t = ( (v1v1*DotProduct( v2, start_dif )) - (v1v2*DotProduct( v1, start_dif )) ) / denom;
		qboolean done = qtrue;

		if ( s < 0 )
		{
			done = qfalse;
			s = 0;// and see note below
		}

		if ( s > 1 ) 
		{
			done = qfalse;
			s = 1;// and see note below
		}

		if ( t < 0 ) 
		{
			done = qfalse;
			t = 0;// and see note below
		}

		if ( t > 1 ) 
		{
			done = qfalse;
			t = 1;// and see note below
		}

		//vec close_pnt1 = start1 + s * v1
		VectorMA( start1, s, v1, close_pnt1 );
		//vec close_pnt2 = start2 + t * v2
		VectorMA( start2, t, v2, close_pnt2 );

		current_dist = Distance( close_pnt1, close_pnt2 );
		//now, if none of those if's fired, you are done. 
		if ( done )
		{
			return current_dist;
		}
		//If they did fire, then we need to do some additional tests.

		//What we are gonna do is see if we can find a shorter distance than the above
		//involving the endpoints.
	}
	else
	{
		//******start here for paralell lines with current_dist = infinity****
		current_dist = Q3_INFINITE;
	}

	//test all the endpoints
	new_dist = Distance( start1, start2 );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( start1, close_pnt1 );
		VectorCopy( start2, close_pnt2 );
		current_dist = new_dist;
	}

	new_dist = Distance( start1, end2 );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( start1, close_pnt1 );
		VectorCopy( end2, close_pnt2 );
		current_dist = new_dist;
	}

	new_dist = Distance( end1, start2 );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( end1, close_pnt1 );
		VectorCopy( start2, close_pnt2 );
		current_dist = new_dist;
	}

	new_dist = Distance( end1, end2 );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( end1, close_pnt1 );
		VectorCopy( end2, close_pnt2 );
		current_dist = new_dist;
	}

	//Then we have 4 more point / segment tests

	G_FindClosestPointOnLineSegment( start2, end2, start1, new_pnt );
	new_dist = Distance( start1, new_pnt );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( start1, close_pnt1 );
		VectorCopy( new_pnt, close_pnt2 );
		current_dist = new_dist;
	}

	G_FindClosestPointOnLineSegment( start2, end2, end1, new_pnt );
	new_dist = Distance( end1, new_pnt );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( end1, close_pnt1 );
		VectorCopy( new_pnt, close_pnt2 );
		current_dist = new_dist;
	}

	G_FindClosestPointOnLineSegment( start1, end1, start2, new_pnt );
	new_dist = Distance( start2, new_pnt );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( new_pnt, close_pnt1 );
		VectorCopy( start2, close_pnt2 );
		current_dist = new_dist;
	}

	G_FindClosestPointOnLineSegment( start1, end1, end2, new_pnt );
	new_dist = Distance( end2, new_pnt );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( new_pnt, close_pnt1 );
		VectorCopy( end2, close_pnt2 );
		current_dist = new_dist;
	}

	return current_dist;
}

void GetAnglesForDirection( const vec3_t p1, const vec3_t p2, vec3_t out )
{
	vec3_t v;

	VectorSubtract( p2, p1, v );
	vectoangles( v, out );
}

void UpdateGlobalCenterPrint(const int levelTime) {
	if (level.globalCenterPrint.sendUntilTime) {
		// temporarily turn priority off so we can print stuff here
		qboolean oldPriority = level.globalCenterPrint.prioritized;
		level.globalCenterPrint.prioritized = qfalse;

		if (levelTime >= level.globalCenterPrint.sendUntilTime) {
			// timeout, send an empty one to reset the center print and clear state
			trap_SendServerCommand(-1, "cp \"\"");
			level.globalCenterPrint.sendUntilTime = 0;
		}
		else if (levelTime >= level.globalCenterPrint.lastSentTime + 1000) {
			// send another one every second
			if (level.globalCenterPrint.cmd[0]) {
				trap_SendServerCommand(-1, level.globalCenterPrint.cmd);
			}
			else {
				for (int i = 0; i < MAX_CLIENTS; i++)
					trap_SendServerCommand(i, level.globalCenterPrint.cmdUnique[i]);
			}
			level.globalCenterPrint.lastSentTime = levelTime;
		}

		level.globalCenterPrint.prioritized = oldPriority;
	}
}

// used to print a global cp for a duration, optionally prioritized (no other cp will be shown during that time)
// a call to this overrides the previous one if still going on
void G_GlobalTickedCenterPrint( const char *msg, int milliseconds, qboolean prioritized ) {
	memset(&level.globalCenterPrint.cmdUnique, 0, sizeof(level.globalCenterPrint.cmdUnique));
	Com_sprintf( level.globalCenterPrint.cmd, sizeof( level.globalCenterPrint.cmd ), "cp \"%s\n\"", msg );
	level.globalCenterPrint.sendUntilTime = level.time + milliseconds;
	level.globalCenterPrint.lastSentTime = 0;
	level.globalCenterPrint.prioritized = prioritized;

	UpdateGlobalCenterPrint( level.time );
}

// same as above but with a difference message for everyone
// msgs should be the start of an array of MAX_CLIENTS strings of msgSize size
void G_UniqueTickedCenterPrint(const void *msgs, size_t msgSize, int milliseconds, qboolean prioritized) {
	assert(msgs);
	memset(&level.globalCenterPrint.cmd, 0, sizeof(level.globalCenterPrint.cmd));
	memset(&level.globalCenterPrint.cmdUnique, 0, sizeof(level.globalCenterPrint.cmdUnique));
	for (int i = 0; i < MAX_CLIENTS; i++) {
		Q_strncpyz(level.globalCenterPrint.cmdUnique[i], "cp \"", sizeof(level.globalCenterPrint.cmdUnique[i]));
		const char *thisMsg = (const char *)((unsigned int)msgs + (msgSize * i));
		if (VALIDSTRING(thisMsg))
			Q_strcat(level.globalCenterPrint.cmdUnique[i], sizeof(level.globalCenterPrint.cmdUnique[i]), thisMsg);
		Q_strcat(level.globalCenterPrint.cmdUnique[i], sizeof(level.globalCenterPrint.cmdUnique[i]), "\n\"");
	}
	level.globalCenterPrint.sendUntilTime = level.time + milliseconds;
	level.globalCenterPrint.lastSentTime = 0;
	level.globalCenterPrint.prioritized = prioritized;

	UpdateGlobalCenterPrint(level.time);
}

static qboolean InTrigger( vec3_t interpOrigin, gentity_t *trigger ) {
	vec3_t mins, maxs;
	static const vec3_t	pmins = { -15, -15, DEFAULT_MINS_2 };
	static const vec3_t	pmaxs = { 15, 15, DEFAULT_MAXS_2 };

	VectorAdd( interpOrigin, pmins, mins );
	VectorAdd( interpOrigin, pmaxs, maxs );

	if ( trap_EntityContact( mins, maxs, trigger ) ) {
		return qtrue; // Player is touching the trigger
	}

	return qfalse; // Player is not touching the trigger
}

static int InterpolateTouchTime( gentity_t *activator, gentity_t *trigger ) {
	int lessTime = 0;

	if ( trigger ) {
		vec3_t	interpOrigin, delta;

		// We know that last client frame, they were not touching the flag, but now they are.  Last client frame was pmoveMsec ms ago, so we only want to interp inbetween that range.
		VectorCopy( activator->client->ps.origin, interpOrigin );
		VectorScale( activator->s.pos.trDelta, 0.001f, delta ); // Delta is how much they travel in 1 ms.

		VectorSubtract( interpOrigin, delta, interpOrigin ); // Do it once before we loop

		// This will be done a max of pml.msec times, in theory, before we are guarenteed to not be in the trigger anymore.
		while ( InTrigger( interpOrigin, trigger ) ) {
			lessTime++; // Add one more ms to be subtracted
			VectorSubtract( interpOrigin, delta, interpOrigin ); // Keep Rewinding position by a tiny bit, that corresponds with 1ms precision (delta*0.001), since delta is per second.
			if ( lessTime >= activator->client->pmoveMsec || lessTime >= 8 ) {
				break; // In theory, this should never happen, but just incase stop it here.
			}
		}
	}

	return lessTime;
}

void G_ResetAccurateTimerOnTrigger( accurateTimer *timer, gentity_t *activator, gentity_t *trigger ) {
	timer->startTime = trap_Milliseconds() - InterpolateTouchTime( activator, trigger );
	timer->startLag = trap_Milliseconds() - level.frameStartTime + level.time - activator->client->pers.cmd.serverTime;
}

int G_GetAccurateTimerOnTrigger( accurateTimer *timer, gentity_t *activator, gentity_t *trigger ) {
	const int endTime = trap_Milliseconds() - InterpolateTouchTime( activator, trigger );
	const int endLag = trap_Milliseconds() - level.frameStartTime + level.time - activator->client->pers.cmd.serverTime;

	int time = endTime - timer->startTime;
	if ( timer->startLag - endLag > 0 ) {
		time += timer->startLag - endLag;
	}

	return time;
}

void G_DeletePlayerProjectilesAtPoint( gentity_t *ent, vec3_t origin, float radius, ProjectileFilterCallback filterCallback ) {
	int i;
	for ( i = MAX_CLIENTS; i < MAX_GENTITIES; i++ ) {
		if ( !g_entities[i].inuse ) {
			continue;
		}

		if ( g_entities[i].r.ownerNum != ent->s.number ) {
			continue; // not my missile
		}
		
		if ( g_entities[i].s.eType != ET_MISSILE ) {
			continue; // not a missile
		}

		if ( filterCallback && !filterCallback( &g_entities[i] ) ) {
			continue; // don't delete it if the filter returns qfalse
		}

		if ( origin && DistanceSquared( origin, g_entities[i].r.currentOrigin ) >= radius * radius ) {
			continue; // don't delete it if an origin is specified and it is too far
		}

		G_FreeEntity( &g_entities[i] );
	}
}

void G_DeletePlayerProjectiles( gentity_t *ent ) {
	G_DeletePlayerProjectilesAtPoint( ent, NULL, 0, NULL );
}

void G_PrintBasedOnRacemode( const char* text, qboolean toRacersOnly ) {
	// save the text because it probably comes from one of the va() buffers which will be cleared in the loop
	char textSave[MAX_STRING_CHARS];
	Q_strncpyz( textSave, text, sizeof( textSave ) );

	int i;
	for ( i = 0; i < level.maxclients; ++i ) {
		gentity_t *ent = &g_entities[i];

		if ( !ent || !ent->inuse || !ent->client ) {
			continue;
		}

		// use STAT_RACEMODE so racespectators also get the message
		qboolean raceState = ent->client->ps.stats[STAT_RACEMODE] != 0;

		// skip if different race state than specified
		if ( raceState != toRacersOnly ) {
			continue;
		}

		trap_SendServerCommand( ent - g_entities, va( "print \"%s\n\"", textSave ) );
	}
}

static qboolean RacerTeleportLimitExceeded(gentity_t *ent) {
	if (!ent || !ent->client)
		return qfalse;

	qboolean exceeded = qfalse;

	if (ent->client->pers.raceTeleportFrame == level.framenum - 1) { // prevent idiots from just holding down their teleport bind
		exceeded = qtrue;
	}
	else if (ent->client->pers.raceTeleportTime && (level.time - ent->client->pers.raceTeleportTime) < 1000) { // we are in tracking second for current user, check limit
		exceeded = ent->client->pers.raceTeleportCount >= 10; // limit to 10 per second i guess
		++ent->client->pers.raceTeleportCount;
	}
	else { // this is the first and only teleport that has been done in the last second
		ent->client->pers.raceTeleportCount = 1;
	}

	// in any case, reset the timer and framenum so people have to unpress their spam binds while blocked to be unblocked
	ent->client->pers.raceTeleportTime = level.time;
	ent->client->pers.raceTeleportFrame = level.framenum;

	return exceeded;
}

// returns qtrue on success
extern int speedLoopSound;
qboolean G_TeleportRacerToTelemark( gentity_t *ent ) {
	if ( !ent || !ent->client ) {
		return qfalse;
	}

	if ( ent->health <= 0 ) {
		return qfalse;
	}

	gclient_t *client = ent->client;

	if ( client->ps.fallingToDeath ) {
		return qfalse;
	}

	if ( !client->sess.inRacemode ) {
		return qfalse;
	}

	if ( client->sess.telemarkOrigin[0] == 0 &&
		client->sess.telemarkOrigin[1] == 0 &&
		client->sess.telemarkOrigin[2] == 0 &&
		client->sess.telemarkYawAngle == 0 &&
		client->sess.telemarkPitchAngle == 0 ) {

		return qfalse;
	}

	// don't let them tp while in rolls so they can't precharge rolls on ground and do them in limited space with tele
	if ( BG_InRoll( &client->ps, ent->s.legsAnim ) ) {
		return qfalse;
	}

	if (RacerTeleportLimitExceeded(ent)) {
		return qfalse;
	}

	// exit training
	ExitAimTraining(ent);

	// stop recording
	ent->client->moveRecordStart = 0;
	ent->client->trailHead = -1;
	memset(ent->client->trail, 0, sizeof(ent->client->trail));
	ent->client->reachedLimit = qfalse;

	vec3_t angles = { 0, 0, 0 };
	vec3_t neworigin;
	angles[YAW] = client->sess.telemarkYawAngle;
	angles[PITCH] = client->sess.telemarkPitchAngle;
	VectorCopy( client->sess.telemarkOrigin, neworigin );

	// drop them to floor
	trace_t tr;
	vec3_t down, mins, maxs;
	VectorSet( mins, -15, -15, DEFAULT_MINS_2 );
	VectorSet( maxs, 15, 15, DEFAULT_MAXS_2 );
	VectorCopy( client->sess.telemarkOrigin, down );
	down[2] -= 32768;
	trap_Trace( &tr, client->sess.telemarkOrigin, mins, maxs, down, client->ps.clientNum, MASK_PLAYERSOLID );
	neworigin[2] = ( int )tr.endpos[2];

	trap_UnlinkEntity( ent );

	// teleport them
	VectorCopy( neworigin, client->ps.origin );
	client->ps.origin[2] += 8; // teleport slightly above to prevent jitteriness
	VectorClear( client->ps.velocity ); // make sure they have 0 momentum
	client->ps.eFlags ^= EF_TELEPORT_BIT; // toggle the teleport bit so the client knows to not lerp
	SetClientViewAngle( ent, angles ); // set angles
	BG_PlayerStateToEntityState( &client->ps, &ent->s, qtrue ); // save results of pmove
	VectorCopy( client->ps.origin, ent->r.currentOrigin ); // use the precise origin for linking

	trap_LinkEntity( ent );

	// reset flags and set small debounce in case they tp right on flag
	client->ps.powerups[PW_REDFLAG] = 0;
	client->ps.powerups[PW_BLUEFLAG] = 0;
	client->pers.flagDebounceTime = level.time + 500;

	// reset any run validity var
	client->usedWeapon = qfalse;
	client->jumpedOrCrouched = qfalse;
	client->usedForwardOrBackward = qfalse;
	client->runInvalid = qfalse;

	// reset animation to prevent both wallrun and emote abuse
	client->ps.torsoTimer = 0;
	client->ps.legsTimer = 0;

	// destroy projectiles
	G_DeletePlayerProjectiles( ent );

	// stop all force powers
	int i = 0;
	while ( i < NUM_FORCE_POWERS ) {
		if ( client->ps.fd.forcePowersActive & ( 1 << i ) ) {
			WP_ForcePowerStop( ent, i );
		}
		i++;
	}

	// give them full stats again
	G_GiveRacemodeItemsAndFullStats( ent );

	// use speed automatically if they set the setting
	if ( !(client->sess.racemodeFlags & RMF_DONTTELEWITHSPEED)) {
		qboolean wasAlreadyActive = client->ps.fd.forcePowersActive & ( 1 << FP_SPEED );

		WP_ForcePowerStart( ent, FP_SPEED, 0 );
		G_Sound( ent, CHAN_BODY, G_SoundIndex( "sound/weapons/force/speed.wav" ) );
		if ( !wasAlreadyActive ) { // only start the loop if it wasn't already active
			G_Sound( ent, TRACK_CHANNEL_2, speedLoopSound );
		}

		client->ps.forceAllowDeactivateTime = level.time + 1500;
	}

	return qtrue;
}

gentity_t* G_ClosestEntity( gentity_t *ref, entityFilter_func filterFunc ) {
	int i;
	gentity_t *ent;

	gentity_t *found = NULL;
	vec_t lowestDistance = 0;

	for ( i = 0, ent = g_entities; i < level.num_entities; ++i, ++ent ) {
		if ( !ent || !ent->classname ) {
			continue;
		}

		if ( filterFunc( ent ) ) {
			vec_t distance = DistanceSquared( ref->r.currentOrigin, ent->r.currentOrigin );

			if ( !found || distance < lowestDistance ) {
				found = ent;
				lowestDistance = distance;
			}
		}
	}

	return found;
}

void G_FormatLocalDateFromEpoch( char* buf, size_t bufSize, time_t epochSecs ) {
	struct tm * timeinfo;
	timeinfo = localtime( &epochSecs );

	strftime( buf, bufSize, "%Y-%m-%d %I:%M %p", timeinfo );
}

qboolean FileExists(const char *fileName) {
	fileHandle_t f = 0;
	int len = trap_FS_FOpenFile(fileName, &f, FS_READ);

	if (!f)
		return qfalse;

	trap_FS_FCloseFile(f);
	return qtrue;
}

/*
Q_strstrip

Description:	Replace strip[x] in string with repl[x] or remove characters entirely
Mutates:		string
Return:			--

Examples:		Q_strstrip( "Bo\nb is h\rairy!!", "\n\r!", "123" );	// "Bo1b is h2airy33"
Q_strstrip( "Bo\nb is h\rairy!!", "\n\r!", "12" );	// "Bo1b is h2airy"
Q_strstrip( "Bo\nb is h\rairy!!", "\n\r!", NULL );	// "Bob is hairy"
*/

void Q_strstrip( char *string, const char *strip, const char *repl )
{
	char		*out=string, *p=string, c;
	const char	*s=strip;
	int			replaceLen = repl?strlen( repl ):0, offset=0;
	qboolean	recordChar = qtrue;

	while ( (c = *p++) != '\0' )
	{
		recordChar = qtrue;
		for ( s=strip; *s; s++ )
		{
			offset = s-strip;
			if ( c == *s )
			{
				if ( !repl || offset >= replaceLen )
					recordChar = qfalse;
				else
					c = repl[offset];
				break;
			}
		}
		if ( recordChar )
			*out++ = c;
	}
	*out = '\0';
}

char *stristr(const char *str1, const char *str2) {
	const char *p1 = str1;
	const char *p2 = str2;
	const char *r = *p2 == 0 ? str1 : 0;

	while (*p1 != 0 && *p2 != 0) {
		if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2)) {
			if (r == 0)
				r = p1;
			p2++;
		}
		else {
			p2 = str2;
			if (r != 0)
				p1 = r + 1;
			if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2)) {
				r = p1;
				p2++;
			}
			else {
				r = 0;
			}
		}
		p1++;
	}

	return *p2 == 0 ? (char *)r : 0;
}

static char FindLastColorCode(const char *text) {
	char lastColor = '7';
	const char *p = text;

	while (*p) {
		if (Q_IsColorString(p)) {
			// p[0] == '^' and p[1] is '0'..'9'
			lastColor = p[1];
			p += 2;  // skip '^' and the digit
			continue;
		}
		p++;
	}
	return lastColor;
}

static int AdjustChunkBoundary(const char *chunkStart, int chunkLen, int remaining) {
	// if there's fewer than chunkLen characters left, we only need that remainder
	if (remaining < chunkLen) {
		return remaining;
	}

	// if the last character in this chunk is '^' and the next one is a digit,
	// we back up by 1, so '^digit' is not split across the boundary
	if (chunkLen > 0 && Q_IsColorString(&chunkStart[chunkLen - 1])) {
		return chunkLen - 1;
	}
	return chunkLen;
}

// prints a message ingame for someone (use -1 for everyone)
// automatically chunks if the text is too long to send in one message,
// also supports asterisk-prepended messages to avoid showing ingame.
// color codes and/or asterisks are automatically reiterated across chunks,
// such that a long message should appear seamlessly as one message to the user
// NOTE: does NOT automatically append ^7 or \n to the end
#define CHUNK_SIZE 1000
void PrintIngame(int clientNum, const char *fmt, ...) {
	va_list argptr;
	static char text[16384] = { 0 };
	memset(text, 0, sizeof(text));

	va_start(argptr, fmt);
	vsnprintf(text, sizeof(text), fmt, argptr);
	va_end(argptr);

	int len = (int)strlen(text);
	if (!len) {
		assert(qfalse);
		return;
	}

	// check for asterisk at the very start
	int prependAsterisk = 0;
	if (text[0] == '*') {
		prependAsterisk = 1;
		memmove(text, text + 1, strlen(text));
		len--;
	}

	char activeColor = '7';

	// if it's short enough, just send once
	if (len <= CHUNK_SIZE) {
		char buf[MAX_STRING_CHARS];
		memset(buf, 0, sizeof(buf));

		Q_strcat(buf, sizeof(buf), "print \"");

		if (prependAsterisk) {
			Q_strcat(buf, sizeof(buf), "*");
		}

		// always prepend the active color
		{
			char colorStr[3];
			colorStr[0] = '^';
			colorStr[1] = activeColor;
			colorStr[2] = '\0';
			Q_strcat(buf, sizeof(buf), colorStr);
		}

		Q_strcat(buf, sizeof(buf), text);

		Q_strcat(buf, sizeof(buf), "\"");

		trap_SendServerCommand(clientNum, buf);
		return;
	}

	// if we got here, we need chunking
	int remaining = len;
	char *p = text;
	int chunkIndex = 0; // track which chunk we're on

	while (remaining > 0) {
		char buf[MAX_STRING_CHARS];
		memset(buf, 0, sizeof(buf));

		Q_strcat(buf, sizeof(buf), "print \"");

		// if the entire message was asterisk-prepended, do it every chunk
		if (prependAsterisk) {
			Q_strcat(buf, sizeof(buf), "*");
		}

		// always prepend the active color
		{
			char colorStr[3];
			colorStr[0] = '^';
			colorStr[1] = activeColor;
			colorStr[2] = '\0';
			Q_strcat(buf, sizeof(buf), colorStr);
		}

		// pick chunk size
		int chunkLen = CHUNK_SIZE;
		chunkLen = AdjustChunkBoundary(p, chunkLen, remaining);

		// fix zero-len chunk
		if (chunkLen <= 0) {
			chunkLen = 1;
		}

		// copy chunkLen bytes
		char temp[CHUNK_SIZE + 1];
		memset(temp, 0, sizeof(temp));
		Q_strncpyz(temp, p, chunkLen + 1);

		// update color
		activeColor = FindLastColorCode(temp);

		// add temp to buf
		Q_strcat(buf, sizeof(buf), temp);

		// close the quote
		Q_strcat(buf, sizeof(buf), "\"");

		// send this chunk
		trap_SendServerCommand(clientNum, buf);

		// advance
		p += chunkLen;
		remaining -= chunkLen;
		chunkIndex++;
	}
}

// prepends the necessary "print\n" and chunks long messages (similar to PrintIngame)
void OutOfBandPrint(int clientNum, const char *fmt, ...) {
	va_list argptr;
	static char text[16384] = { 0 };
	memset(text, 0, sizeof(text));

	va_start(argptr, fmt);
	vsnprintf(text, sizeof(text), fmt, argptr);
	va_end(argptr);

	int len = (int)strlen(text);
	if (!len) {
		assert(qfalse);
		return;
	}

	// check for asterisk at the very start
	int prependAsterisk = 0;
	if (text[0] == '*') {
		prependAsterisk = 1;
		memmove(text, text + 1, strlen(text));
		len--;
	}

	char activeColor = '7';

	// if it's short enough, just send once
	if (len <= CHUNK_SIZE) {
		char buf[MAX_STRING_CHARS];
		memset(buf, 0, sizeof(buf));

		Q_strcat(buf, sizeof(buf), "print\n");

		if (prependAsterisk) {
			Q_strcat(buf, sizeof(buf), "*");
		}

		// always prepend the active color
		{
			char colorStr[3];
			colorStr[0] = '^';
			colorStr[1] = activeColor;
			colorStr[2] = '\0';
			Q_strcat(buf, sizeof(buf), colorStr);
		}

		Q_strcat(buf, sizeof(buf), text);

		trap_OutOfBandPrint(clientNum, buf);
		return;
	}

	// if we got here, we need chunking
	int remaining = len;
	char *p = text;
	int chunkIndex = 0; // track which chunk we're on

	while (remaining > 0) {
		char buf[MAX_STRING_CHARS];
		memset(buf, 0, sizeof(buf));

		Q_strcat(buf, sizeof(buf), "print\n");

		// if the entire message was asterisk-prepended, do it every chunk
		if (prependAsterisk) {
			Q_strcat(buf, sizeof(buf), "*");
		}

		// always prepend the active color
		{
			char colorStr[3];
			colorStr[0] = '^';
			colorStr[1] = activeColor;
			colorStr[2] = '\0';
			Q_strcat(buf, sizeof(buf), colorStr);
		}

		// pick chunk size
		int chunkLen = CHUNK_SIZE;
		chunkLen = AdjustChunkBoundary(p, chunkLen, remaining);

		// fix zero-len chunk
		if (chunkLen <= 0) {
			chunkLen = 1;
		}

		// copy chunkLen bytes
		char temp[CHUNK_SIZE + 1];
		memset(temp, 0, sizeof(temp));
		Q_strncpyz(temp, p, chunkLen + 1);

		// update color
		activeColor = FindLastColorCode(temp);

		Q_strcat(buf, sizeof(buf), temp);

		trap_OutOfBandPrint(clientNum, buf);

		p += chunkLen;
		remaining -= chunkLen;
		chunkIndex++;
	}
}

gclient_t* G_FindClientByIPPort(const char* ipPortString) {
	int i;
	for (i = 0; i < level.maxclients; ++i) {
		gclient_t* client = &level.clients[i];

		if (client->pers.connected != CON_CONNECTED)
			continue;
		if (Q_stricmp(client->sess.ipString, ipPortString))
			continue;

		return client;
	}

	return NULL;
}

void G_FormatDuration(const int duration, char* out, size_t outSize) {
	div_t qr;
	int pHours, pMins, pSecs;

	qr = div(duration, 60);
	pSecs = qr.rem;
	qr = div(qr.quot, 60);
	pMins = qr.rem;
	pHours = qr.quot;

	if (pHours) {
		Com_sprintf(out, outSize, "%d hours", pHours);
	} else if (pMins) {
		Com_sprintf(out, outSize, "%d minutes", pMins);
	} else if (pSecs) {
		Com_sprintf(out, outSize, "%d seconds", pSecs);
	} else {
		Q_strncpyz(out, "less than a second", outSize);
	}
}

#define NUM_CVAR_BUFFERS	8
const char *Cvar_VariableString(const char *var_name) {
	static char buf[NUM_CVAR_BUFFERS][MAX_STRING_CHARS] = { 0 };
	static int bufferNum = -1;

	bufferNum++;
	bufferNum %= NUM_CVAR_BUFFERS;

	trap_Cvar_VariableStringBuffer(var_name, buf[bufferNum], sizeof(buf[bufferNum]));
	return buf[bufferNum];
}


qboolean HasFlag(gentity_t *ent) {
	return !!(ent && ent->client && (ent->client->ps.powerups[PW_REDFLAG] || ent->client->ps.powerups[PW_BLUEFLAG] || ent->client->ps.powerups[PW_NEUTRALFLAG]));
}

void PlayAimPracticeBotPainSound(gentity_t *npc, gentity_t *player) {
	if (!npc || !player) {
		assert(qfalse);
		return;
	}

	gentity_t *ev = G_TempEntity(npc->r.currentOrigin, EV_ENTITY_SOUND);
	int rng = Q_irand(0, 3);
	if (rng == 0)
		ev->s.eventParm = G_SoundIndex("*pain25");
	else if (rng == 1)
		ev->s.eventParm = G_SoundIndex("*pain50");
	else if (rng == 2)
		ev->s.eventParm = G_SoundIndex("*pain75");
	else
		ev->s.eventParm = G_SoundIndex("*pain100");
	ev->s.clientNum = npc->s.number;
	ev->s.trickedentindex = CHAN_VOICE;
	ev->r.broadcastClients[1] |= ~(level.racemodeClientMask | level.racemodeSpectatorMask); // hide to in game players...
	for (int i = 0; i < MAX_CLIENTS; i++) { // hide to racers who are doing their own timed run...
		gentity_t *thisEnt = &g_entities[i];
		if (thisEnt == player)
			continue;
		if (thisEnt->inuse && thisEnt->client && thisEnt->client->pers.connected == CON_CONNECTED && thisEnt->aimPracticeEntBeingUsed && thisEnt->aimPracticeMode == AIMPRACTICEMODE_TIMED)
			ev->r.broadcastClients[1] |= (1 << i);
	}
	ev->r.broadcastClients[1] |= level.racemodeClientsHidingOtherRacersMask; // ...hide to racers who disabled seeing other racers as well...
	if (player - g_entities >= 0 && player - g_entities < MAX_CLIENTS)
		ev->r.broadcastClients[1] &= ~(1 << (player - g_entities)); // ...but show to the client num associated with this event if there is one...
	ev->r.broadcastClients[1] &= ~level.ingameClientsSeeingInRaceMask; // ...and show to ig players who enabled seeing racemode stuff
}

void CenterPrintToPlayerAndFollowers(gentity_t *ent, const char *s) {
	char *cmdStr = va("cp \"%s\"", s);
	trap_SendServerCommand(ent - g_entities, cmdStr);

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *thisEnt = &g_entities[i];
		if (thisEnt == ent || !thisEnt->inuse || !thisEnt->client || thisEnt->client->sess.spectatorState != SPECTATOR_FOLLOW || thisEnt->client->sess.spectatorClient != ent - g_entities)
			continue;
		trap_SendServerCommand(thisEnt - g_entities, cmdStr);
	}
}

void ExitAimTraining(gentity_t *ent) {
	if (!ent)
		return;
	ent->aimPracticeEntBeingUsed = NULL;
	ent->aimPracticeMode = AIMPRACTICEMODE_NONE;
	ent->numAimPracticeSpawns = 0;
	ent->numTotalAimPracticeHits = 0;
	memset(ent->numAimPracticeHitsOfWeapon, 0, sizeof(ent->numAimPracticeHitsOfWeapon));
	G_GiveRacemodeItemsAndFullStats(ent);
}

void PrintBasedOnAccountFlags(int64_t flags, const char *msg) {
	static qboolean recursion = qfalse;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || !ent->client->account)
			continue;

		// special case
		if (!(ent->client->account->flags & flags) && !(flags == ACCOUNTFLAG_AIMPACKEDITOR && (ent->client->account->flags & ACCOUNTFLAG_AIMPACKADMIN)))
			continue;
		
		const char *prefix = AccountBitflag2FlagName(flags);
		if (VALIDSTRING(prefix))
			PrintIngame(i, "^6[%s] ^7%s", AccountBitflag2FlagName(flags), msg);
		else
			PrintIngame(i, msg);
	}
}

void FisherYatesShuffle(void *firstItem, size_t numItems, size_t itemSize) {
	if (!firstItem || !numItems || !itemSize)
		return;

	for (unsigned int i = numItems - 1; i >= 1; i--) {
		int j = rand() % (i + 1);

		byte *iPtr = (byte *)(((unsigned int)firstItem) + (i * itemSize));
		byte *jPtr = (byte *)(((unsigned int)firstItem) + (j * itemSize));

		byte *temp = malloc(itemSize);
		memcpy(temp, iPtr, itemSize);
		memcpy(iPtr, jPtr, itemSize);
		memcpy(jPtr, temp, itemSize);
		free(temp);
	}
}

void FormatNumberToStringWithCommas(uint64_t n, char *out, size_t outSize) {
	char *buf = calloc(outSize, sizeof(char));
	char *p;

	sprintf(buf, "%llu", n);
	int c = 2 - strlen(buf) % 3;
	for (p = buf; *p != 0; p++) {
		*out++ = *p;
		if (c == 1)
			*out++ = ',';
		c = (c + 1) % 3;
	}
	*--out = 0;
	free(buf);
}

#define IsX(s)									(s == 'x' || s == 'X' || s == '�')
#define IsZ(s)									(s == 'z' || s == 'Z' || s == '�' || s == '�')
#define IsS(s)									(s == 's' || s == 'S' || s == '$' || s == '5' || s == '�' || s == '�' || s == '�')
#define IsP(s)									(s == 'p' || s == 'P' || s == '�' || s == '�' || s == '�')
#define IsE(s)									(s == 'e' || s == 'E' || s == '3' || s == '�' || s == '�' || s == '�' || s == '�' || s == '�' || s == '�' || s == '�' || s == '�' || s == '�' || s == '�' || s == '�')
#define IsC(s)									(s == 'c' || s == 'C' || s == '<' || s == '�' || s == '�' || s == '�' || s == '�' || s == '�' || s == '�' || s == 'k')
#define IsPunctuation(s)						(s < 48 || s > 57 && s < 65 || s > 90 && s < 97 || s > 122 && s < 138 || s > 138 && s < 154)

// detect if a string is "spec" or any of a ton of variations on it
static qboolean IsInvalidSpeWord(char *s) {
	if (strlen(s) >= 4 && IsP(s[1]) && IsE(s[2])) { // spe, xpe
		char *p = s + 3;
		while (*p) {
			if (IsE(*p)) { // speeeeeee
				p++;
				continue;
			}
			if (IsC(*p) || IsX(*p)) // spec, spex
				break;
			return qtrue; // spetznaz
		}
		p++;
		if (!*p || IsPunctuation(*p) || !Q_stricmpn(p, "tat", 3) || !Q_stricmpn(p, "tador", 5)) // spec, spec ,spec), spec(, spec-, spec_, spectat
			return qfalse;
		if (IsC(*p) || IsX(*p)) { // specc...spexx
			while (*p) {
				if (IsC(*p) || IsX(*p)) { // spexxxxxx
					p++;
					continue;
				}
				if (!*p || IsPunctuation(*p)) // spexxx,spexxx ,spexxx), spexxx(, spexxx-, spexxx_
					return qfalse;
				return qtrue; // spexxxial jedi
			}
		}
		if (!*p || IsPunctuation(*p)) // spexxx, spexxx ,spexxx) ,spexxx( ,spexxx-, spexxx_
			return qfalse;
		return qtrue; // special jedi
	}
	return qtrue; // dude
}

// count number of spec-ish words; if you have two of these then you are a spec
static char *partialSpecWords[] = {
	"spec", "spex", "spe�", "afk", "bbl", "cookin", "food", "eatin", "5min", "5 min", "1min", "1 min", "0min", "0 min", "1hr", "1 hr", "1hour", "1 hour", "minimized", "dinner",
	"lunch", "breakfast", "snack", "forawhile", "for a while", "forabit", "for a bit", "foranhour", "for an hour", "foramin", "for a min", "supper", "later", "minutes", "hours",
	"back in", "working", "studying", NULL
};

static int CountPartialSpecWords(char *s) {
	static int numSpecWords;
	numSpecWords = 0;
	int i = 0;
	char *p = partialSpecWords[i];
	while (p) {
		if (stristr(s, p))
			numSpecWords++;
		i++;
		p = partialSpecWords[i];
	}
	return numSpecWords;
}


// does the client have a word in his name that makes him definitely NOT a spec?
static char *confirmedNotSpecWords[] = {
	"unspec", "aspect", "pectoral", "expect", "respect", "inspect", "prospect", "retrospect", "suspect", "circumspect", "perspective",
	"introspect", "spectrogram", "spectral", "species", "special", "spec fnneed", "not spec", "notspec",
	"spec if not need", "spec if unneed", "spec unless need", NULL
};

static qboolean ContainsConfirmedNotSpecWord(char *s) {
	int i = 0;
	char *p = confirmedNotSpecWords[i];
	while (p) {
		if (stristr(s, p))
			return qtrue;
		i++;
		p = confirmedNotSpecWords[i];
	}
	return qfalse;
}

// does the client have a word in his name that makes him definitely a spec?
static char *confirmedSpecWords[] = {
	"elo BOT", "speccin", "xpeccin", "�peccin", "zpeccin", "spectat", "xpectat", "�pectat", "zpectat", "@spec", "@xpec", "@�pec", "@zpec",
	"@afk", "@bbl", "notpickabl", "not pickabl", "'tpickme", "nt pick me", "notplay", "not play", "notpug", "not pug", "@food", "@breakfast", "@lunch", "@dinner", "@supper", "@snack",
	"@eating", "@nothere", "speconly", "spec only", "backlater", "back later", "playlater", "play later", "puglater", "pug later", "skillofspecoldwoman",
	"obserwator", "rotatceps", "specman", "ragespec", "onlyspec", "speconly", "specafew", "specafk", "afkspec", "afkandspec", "specandafk", "speccy",
	"specdonis", "speceating", "specdinner", "specdindin", "specgaycular", "speclord", "specleo", "specmode", "spec mode", "specpete", "specpolice",
	"specrickses", "specspec", "skilloflonelyoldspec", "specalways", "spec always", "specone", "spec one", "touchpad", "specuntilhannahwins", "specsr",
	NULL
};

static qboolean ContainsConfirmedSpecWord(char *s) {
	int i = 0;
	char *p = confirmedSpecWords[i];
	while (p) {
		if (stristr(s, p))
			return qtrue;
		i++;
		p = confirmedSpecWords[i];
	}
	return qfalse;
}

qboolean IsSpecName(const char *name) {
	if (!VALIDSTRING(name))
		return qfalse;

	char cleanname[64] = { 0 };
	Q_strncpyz(cleanname, name, sizeof(cleanname));
	Q_StripColor(cleanname);

	if (!cleanname[0])
		return qfalse;

	if (stristr(cleanname, "aspectato") || stristr(cleanname, "expectato") || stristr(cleanname, "respectato")) // stupid edge case checks for aSpectatorForThisGame, etc
		return qtrue;

	if (ContainsConfirmedNotSpecWord(cleanname)) // aspect ratio, unspec, respect my authority
		return qfalse;

	if (ContainsConfirmedSpecWord(cleanname) || CountPartialSpecWords(cleanname) >= 2) // spectatoes, speccing this pug, dude@brb, bbl dinner
		return qtrue;

	char *p = NULL;
	static char specWordFirstLetters[] = { 's', 'S', '$', '5', '�', '�', '�', 'x', 'X', '�', 'z', 'Z', '�', '�', '\0' };

	// loop to check for all possible names like spec, $peeeeeX, etc
	for (int i = 0; specWordFirstLetters[i]; i++) {
		char *readFrom = cleanname;
		p = strchr(readFrom, specWordFirstLetters[i]);
		while (p) {
			if (IsInvalidSpeWord(p)) {
				if (*(readFrom + 1)) {
					p = strchr(++readFrom, specWordFirstLetters[i]);
					continue;
				}
				else {
					goto outerLoopContinue; // lick my
				}
			}
			else {
				return qtrue;
			}
		}
	outerLoopContinue:; // nuts
	}

	// check for dumb long words like "spee33eec"
	if (strlen(cleanname) > 4) {
		for (unsigned int i = 0; cleanname[i] && i < strlen(cleanname); i++) {
			p = cleanname + i;
			if (VALIDSTRING(p) && strlen(p) > 4 && !IsInvalidSpeWord(p))
				return qtrue;
		}
	}

	// special checks for afk and bbl
	p = stristr(cleanname, "afk");
	if (VALIDSTRING(p) && IsPunctuation(p[3]))
		return qtrue;
	p = stristr(cleanname, "bbl");
	if (VALIDSTRING(p) && IsPunctuation(p[3]))
		return qtrue;

	return qfalse;
}

#define SVSAY_PREFIX "Server^7\x19: "
#define SVTELL_PREFIX "\x19[Server^7\x19]\x19: "

// game equivalent
void SV_Tell(int clientNum, const char *text) {
	if (clientNum < 0 || clientNum >= MAX_CLIENTS)
		return;

	gentity_t *ent = &g_entities[clientNum];
	if (!ent->inuse || !ent->client)
		return;

	Com_Printf("tell: svtell to %s" S_COLOR_WHITE ": %s\n", ent->client->pers.netname, text);
	trap_SendServerCommand(clientNum, va("chat \"" SVTELL_PREFIX S_COLOR_MAGENTA "%s" S_COLOR_WHITE "\"\n", text));
}

// game equivalent
void SV_Say(const char *text) {
	Com_Printf("broadcast: chat \"" SVSAY_PREFIX "%s\\n\"\n", text);
	trap_SendServerCommand(-1, va("chat \"" SVSAY_PREFIX "%s\"\n", text));
}

// counts connected non-bot players (not adapted properly for siege)
void CountPlayers(int *total, int *red, int *blue, int *free, int *spec, int *redOrBlue, int *freeOrSpec) {
	if (total)
		*total = 0;
	if (red)
		*red = 0;
	if (blue)
		*blue = 0;
	if (free)
		*free = 0;
	if (spec)
		*spec = 0;
	if (redOrBlue)
		*redOrBlue = 0;
	if (freeOrSpec)
		*freeOrSpec = 0;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || (ent->r.svFlags & SVF_BOT))
			continue;

		if (total)
			++(*total);

		switch (ent->client->sess.sessionTeam) {
		case TEAM_FREE:
			if (free)
				++(*free);
			if (freeOrSpec)
				++(*freeOrSpec);
			break;
		case TEAM_RED:
			if (red)
				++(*red);
			if (redOrBlue)
				++(*redOrBlue);
			break;
		case TEAM_BLUE:
			if (blue)
				++(*blue);
			if (redOrBlue)
				++(*redOrBlue);
			break;
		case TEAM_SPECTATOR:
			if (spec)
				++(*spec);
			if (freeOrSpec)
				++(*freeOrSpec);
			break;
		}
	}
}

// takes an integer of milliseconds and gives you a formatted string
// forceColon: if qtrue, prepends ":" for times under one minute
// zeroMinutes: if qtrue, prepends "0:" for times under one minute
// zeroPadSeconds: if qtrue, prepends a zero for times under ten seconds ("05" instead of "5")
// fractionalDigits: number of digits after the decimal point to print (can be zero)
char *ParseMillisecondsToString(int msIn, qboolean forceColon, qboolean zeroMinutes, qboolean zeroPadSeconds, unsigned int fractionalDigits) {
	static char result[32] = { 0 };
	memset(&result, 0, sizeof(result));
	qboolean negative = msIn < 0 ? qtrue : qfalse;
	int msAbs = abs(msIn), d, h, m, s, ms;
	if (msAbs < 60000) { // seconds
		s = msAbs / 1000;
		ms = msAbs - (1000 * s);
		Q_strncpyz(result, zeroPadSeconds ? va("%02i", s) : va("%i", s), sizeof(result));
		if (zeroMinutes)
			Q_strncpyz(result, va("0:%s", result), sizeof(result));
		else if (forceColon)
			Q_strncpyz(result, va(":%s", result), sizeof(result));
	}
	else if (msAbs < 3600000) { // minutes:seconds
		m = msAbs / 60000;
		s = (msAbs % 60000) / 1000;
		ms = (msAbs % 60000) - (1000 * s);
		Q_strncpyz(result, va("%i:%02i", m, s), sizeof(result));
	}
	else if (msAbs < 86400000) { // hours:minutes:seconds
		h = msAbs / 3600000;
		m = (msAbs % 3600000) / 60000;
		s = (msAbs % 60000) / 1000;
		ms = (msAbs % 60000) - (1000 * s);
		Q_strncpyz(result, va("%i:%02i:%02i", h, m, s), sizeof(result));
	}
	else { // days:hours:minutes:seconds
		d = msAbs / 86400000;
		h = (msAbs % 86400000) / 3600000;
		m = (msAbs % 3600000) / 60000;
		s = (msAbs % 60000) / 1000;
		ms = (msAbs % 60000) - (1000 * s);
		Q_strncpyz(result, va("%i:%i:%02i:%02i", d, h, m, s), sizeof(result));
	}

	char frac[16] = { 0 };
	if (fractionalDigits > 0 && fractionalDigits < ARRAY_LEN(frac)) {
		Q_strncpyz(frac, va(".%03i", ms), sizeof(frac));
		frac[fractionalDigits + 1] = '\0';
		Q_strcat(result, sizeof(result), frac);
	}

	if (negative)
		Q_strncpyz(result, va("-%s", result), sizeof(result));

	return result;
}

qboolean IsFreeSpec(gentity_t *ent) {
	if (ent && ent->client && ent->client->sess.sessionTeam == TEAM_SPECTATOR && !(ent->client->ps.pm_flags & PMF_FOLLOW))
		return qtrue;
	return qfalse;
}

// if exclude is set, will return NULL if fc is exclude
// if limitLocation is qtrue, then will return NULL if fc is at ctf location >= onlyAtThisCtfLocationAndBelow (e.g. 0.5 to not return fcs on enemy side of map)
gentity_t *GetFC(int team, gentity_t *exclude, qboolean limitLocation, float onlyAtThisCtfLocationAndBelow) {
	if (g_gametype.integer != GT_CTF)
		return NULL;
	if (team != TEAM_RED && team != TEAM_BLUE)
		return NULL;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *thisGuy = &g_entities[i];
		if (!thisGuy->inuse || !thisGuy->client || (exclude && thisGuy == exclude) || thisGuy->client->sess.sessionTeam != team ||
			IsRacerOrSpectator(thisGuy) || thisGuy->health <= 0 || !HasFlag(thisGuy) || thisGuy->client->ps.fallingToDeath)
			continue;

		if (limitLocation && GetCTFLocationValue(thisGuy) > onlyAtThisCtfLocationAndBelow)
			continue;

		return thisGuy;
	}

	return NULL;
}

qboolean DoMemeNonRace(int clientNum) {
	if (clientNum < 0 || clientNum >= MAX_GENTITIES)
		return qfalse;
	gentity_t* memer = &g_entities[clientNum];
	qboolean meme = (!level.wasRestarted && memer && memer->client && memer->client->account && !memer->client->sess.inRacemode &&
		(!Q_stricmp(memer->client->account->name, "duo") || !Q_stricmp(memer->client->account->name, "alpha")));
	return meme;
}
