//b_goal.cpp
#include "b_local.h"
#include "Q3_Interface.h"

extern qboolean FlyingCreature( gentity_t *ent );
/*
SetGoal
*/

void SetGoal( gentity_t *goal, float rating ) 
{
	NPCInfo->goalEntity = goal;
	NPCInfo->goalTime = level.time;
	if ( goal ) 
	{
	}
	else 
	{
	}
}


/*
NPC_SetGoal
*/

void NPC_SetGoal( gentity_t *goal, float rating ) 
{
	if ( goal == NPCInfo->goalEntity ) 
	{
		return;
	}

	if ( !goal ) 
	{
		return;
	}

	if ( goal->client ) 
	{
		return;
	}

	if ( NPCInfo->goalEntity ) 
	{
		NPCInfo->lastGoalEntity = NPCInfo->goalEntity;
	}

	SetGoal( goal, rating );
}


/*
NPC_ClearGoal
*/

void NPC_ClearGoal( void ) 
{
	gentity_t	*goal;

	if ( !NPCInfo->lastGoalEntity ) 
	{
		SetGoal( NULL, 0.0 );
		return;
	}

	goal = NPCInfo->lastGoalEntity;
	NPCInfo->lastGoalEntity = NULL;
	if ( goal->inuse && !(goal->s.eFlags & EF_NODRAW) ) 
	{
		SetGoal( goal, 0 );
		return;
	}

	SetGoal( NULL, 0.0 );
}

/*
-------------------------
G_BoundsOverlap
-------------------------
*/

qboolean G_BoundsOverlap(const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2)
{//NOTE: flush up against counts as overlapping
	if(mins1[0]>maxs2[0])
		return qfalse;

	if(mins1[1]>maxs2[1])
		return qfalse;

	if(mins1[2]>maxs2[2])
		return qfalse;

	if(maxs1[0]<mins2[0])
		return qfalse;

	if(maxs1[1]<mins2[1])
		return qfalse;

	if(maxs1[2]<mins2[2])
		return qfalse;

	return qtrue;
}

void NPC_ReachedGoal( void )
{
	NPC_ClearGoal();
	NPCInfo->goalTime = level.time;

//MCG - Begin
	NPCInfo->aiFlags &= ~NPCAI_MOVING;
	ucmd.forwardmove = 0;
	//Return that the goal was reached
	trap_ICARUS_TaskIDComplete( NPC, TID_MOVE_NAV );
//MCG - End
}
/*
ReachedGoal

id removed checks against waypoints and is now checking surfaces
*/
//qboolean NAV_HitNavGoal( vec3_t point, vec3_t mins, vec3_t maxs, gentity_t *goal, qboolean flying );
qboolean ReachedGoal( gentity_t *goal ) 
{
	//FIXME: For script waypoints, need a special check
	if ( NPCInfo->aiFlags & NPCAI_TOUCHED_GOAL ) 
	{
		NPCInfo->aiFlags &= ~NPCAI_TOUCHED_GOAL;
		return qtrue;
		}

	return NAV_HitNavGoal( NPC->r.currentOrigin, NPC->r.mins, NPC->r.maxs, goal->r.currentOrigin, NPCInfo->goalRadius, FlyingCreature( NPC ) );
}

/*
static gentity_t *UpdateGoal( void ) 

Id removed a lot of shit here... doesn't seem to handle waypoints independantly of goalentity

In fact, doesn't seem to be any waypoint info on entities at all any more?

MCG - Since goal is ALWAYS goalEntity, took out a lot of sending goal entity pointers around for no reason
*/

gentity_t *UpdateGoal( void ) 
{
	gentity_t	*goal;

	if ( !NPCInfo->goalEntity ) 
	{
		return NULL;
	}

	if ( !NPCInfo->goalEntity->inuse )
	{//Somehow freed it, but didn't clear it
		NPC_ClearGoal();
		return NULL;
	}

	goal = NPCInfo->goalEntity;

	if ( ReachedGoal( goal ) ) 
	{
		NPC_ReachedGoal();
		goal = NULL;//so they don't keep trying to move to it
	}//else if fail, need to tell script so?

	return goal;
}
