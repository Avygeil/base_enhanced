// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_weapon.c 
// perform the server side effects of a weapon firing

#include "g_local.h"
#include "ai_main.h"
#include "be_aas.h"
#include "G2.h"
#include "q_shared.h"

static	vec3_t	forward, vright, up;
static	vec3_t	muzzle;

// Bryar Pistol
//--------
#define BRYAR_PISTOL_VEL			1600
#define BRYAR_PISTOL_DAMAGE			10
#define BRYAR_CHARGE_UNIT			200.0f	// bryar charging gives us one more unit every 200ms--if you change this, you'll have to do the same in bg_pmove
#define BRYAR_ALT_SIZE				1.0f

// E11 Blaster
//---------
#define BLASTER_SPREAD				1.6f//1.2f
#define BLASTER_VELOCITY			2300
#define BLASTER_DAMAGE				20

// Tenloss Disruptor
//----------
#define DISRUPTOR_MAIN_DAMAGE			30 //40
#define DISRUPTOR_MAIN_DAMAGE_SIEGE		50
#define DISRUPTOR_NPC_MAIN_DAMAGE_CUT	0.25f

#define DISRUPTOR_ALT_DAMAGE			100
#define DISRUPTOR_NPC_ALT_DAMAGE_CUT	0.2f
#define DISRUPTOR_ALT_TRACES			3		// can go through a max of 3 damageable(sp?) entities
#define DISRUPTOR_CHARGE_UNIT			50.0f	// distruptor charging gives us one more unit every 50ms--if you change this, you'll have to do the same in bg_pmove

// Wookiee Bowcaster
//----------
#define	BOWCASTER_DAMAGE			50
#define	BOWCASTER_VELOCITY			1300
#define BOWCASTER_SPLASH_DAMAGE		0
#define BOWCASTER_SPLASH_RADIUS		0
#define BOWCASTER_SIZE				2

#define BOWCASTER_ALT_SPREAD		5.0f
#define BOWCASTER_VEL_RANGE			0.3f
#define BOWCASTER_CHARGE_UNIT		200.0f	// bowcaster charging gives us one more unit every 200ms--if you change this, you'll have to do the same in bg_pmove

// Heavy Repeater
//----------
#define REPEATER_SPREAD				1.4f
#define	REPEATER_DAMAGE				14
#define	REPEATER_VELOCITY			1600

#define REPEATER_ALT_SIZE				3	// half of bbox size
#define	REPEATER_ALT_DAMAGE				60
#define REPEATER_ALT_SPLASH_DAMAGE		60
#define REPEATER_ALT_SPLASH_RADIUS		128
#define REPEATER_ALT_SPLASH_RAD_SIEGE	80
#define	REPEATER_ALT_VELOCITY			1100

// DEMP2
//----------
#define	DEMP2_DAMAGE				35
#define	DEMP2_VELOCITY				1800
#define	DEMP2_SIZE					2		// half of bbox size

#define DEMP2_ALT_DAMAGE			8 //12		// does 12, 36, 84 at each of the 3 charge levels.
#define DEMP2_CHARGE_UNIT			700.0f	// demp2 charging gives us one more unit every 700ms--if you change this, you'll have to do the same in bg_weapons
#define DEMP2_ALT_RANGE				4096
#define DEMP2_ALT_SPLASHRADIUS		256

// Golan Arms Flechette
//---------
#define FLECHETTE_SHOTS				5
#define FLECHETTE_SHOTS_ALT			2
#define FLECHETTE_SPREAD			4.0f
#define FLECHETTE_DAMAGE			12//15
#define FLECHETTE_VEL				3500
#define FLECHETTE_SIZE				1
#define FLECHETTE_MINE_RADIUS_CHECK	256
#define FLECHETTE_ALT_DAMAGE		60
#define FLECHETTE_ALT_SPLASH_DAM	60
#define FLECHETTE_ALT_SPLASH_RAD	128

// Personal Rocket Launcher
//---------
#define	ROCKET_VELOCITY				900
#define	ROCKET_DAMAGE				100
#define	ROCKET_SPLASH_DAMAGE		100
#define	ROCKET_SPLASH_RADIUS		160
#define ROCKET_SIZE					3
#define ROCKET_ALT_THINK_TIME		100

// Concussion Rifle
//---------
//primary
//man, this thing is too absurdly powerful. having to
//slash the values way down from sp.
#define	CONC_VELOCITY				3000
#define	CONC_DAMAGE					75 //150
#define	CONC_NPC_DAMAGE_EASY		40
#define	CONC_NPC_DAMAGE_NORMAL		80
#define	CONC_NPC_DAMAGE_HARD		100
#define	CONC_SPLASH_DAMAGE			40 //50
#define	CONC_SPLASH_RADIUS			200 //300
//alt
#define CONC_ALT_DAMAGE				25 //100
#define CONC_ALT_NPC_DAMAGE_EASY	20
#define CONC_ALT_NPC_DAMAGE_MEDIUM	35
#define CONC_ALT_NPC_DAMAGE_HARD	50

// Stun Baton
//--------------
#define STUN_BATON_DAMAGE			20
#define STUN_BATON_ALT_DAMAGE		20
#define STUN_BATON_RANGE			8

// Melee
//--------------
#define MELEE_SWING1_DAMAGE			10
#define MELEE_SWING2_DAMAGE			12
#define MELEE_RANGE					8

// ATST Main Gun
//--------------
#define ATST_MAIN_VEL				4000	// 
#define ATST_MAIN_DAMAGE			25		// 
#define ATST_MAIN_SIZE				3		// make it easier to hit things

// ATST Side Gun
//---------------
#define ATST_SIDE_MAIN_DAMAGE				75
#define ATST_SIDE_MAIN_VELOCITY				1300
#define ATST_SIDE_MAIN_NPC_DAMAGE_EASY		30
#define ATST_SIDE_MAIN_NPC_DAMAGE_NORMAL	40
#define ATST_SIDE_MAIN_NPC_DAMAGE_HARD		50
#define ATST_SIDE_MAIN_SIZE					4
#define ATST_SIDE_MAIN_SPLASH_DAMAGE		10	// yeah, pretty small, either zero out or make it worth having?
#define ATST_SIDE_MAIN_SPLASH_RADIUS		16	// yeah, pretty small, either zero out or make it worth having?

#define ATST_SIDE_ALT_VELOCITY				1100
#define ATST_SIDE_ALT_NPC_VELOCITY			600
#define ATST_SIDE_ALT_DAMAGE				130

#define ATST_SIDE_ROCKET_NPC_DAMAGE_EASY	30
#define ATST_SIDE_ROCKET_NPC_DAMAGE_NORMAL	50
#define ATST_SIDE_ROCKET_NPC_DAMAGE_HARD	90

#define	ATST_SIDE_ALT_SPLASH_DAMAGE			130
#define	ATST_SIDE_ALT_SPLASH_RADIUS			200
#define ATST_SIDE_ALT_ROCKET_SIZE			5
#define ATST_SIDE_ALT_ROCKET_SPLASH_SCALE	0.5f	// scales splash for NPC's

#define TD_DAMAGE			90 // alpha: bump direct damage to be equal to the highest splash dmg possible
#define TD_SPLASH_RAD		128
#define TD_SPLASH_DAM		90
#define TD_VELOCITY			900
#define TD_MIN_CHARGE		0.15f
#define TD_TIME				3000//6000
#define TD_ALT_TIME			3000

#define TD_ALT_DAMAGE		60//100
#define TD_ALT_SPLASH_RAD	128
#define TD_ALT_SPLASH_DAM	50//90
#define TD_ALT_VELOCITY		600
#define TD_ALT_MIN_CHARGE	0.15f
#define TD_ALT_TIME			3000

// modifies the collision properties of every rocket in the world
void SetRocketContents(int contents) {
	for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !VALIDSTRING(ent->classname) || Q_stricmp(ent->classname, "rocket_proj"))
			continue;
		if (ent->parent && IsRacerOrSpectator(ent->parent))
			continue; // never affect racemode rockets
		ent->r.contents = contents;
	}
}

extern qboolean G_BoxInBounds( vec3_t point, vec3_t mins, vec3_t maxs, vec3_t boundsMins, vec3_t boundsMaxs );
extern qboolean G_HeavyMelee( gentity_t *attacker );
extern void Jedi_Decloak( gentity_t *self );

static void WP_FireEmplaced( gentity_t *ent, qboolean altFire );
static void WP_TouchRocket( gentity_t *ent, gentity_t *other, trace_t *trace );

void laserTrapStick( gentity_t *ent, vec3_t endpos, vec3_t normal );

void touch_NULL( gentity_t *ent, gentity_t *other, trace_t *trace )
{

}

void laserTrapExplode( gentity_t *self );
void RocketDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);

//We should really organize weapon data into tables or parse from the ext data so we have accurate info for this,
float WP_SpeedOfMissileForWeapon( int wp, qboolean alt_fire )
{
	return 500;
}

extern void CalcEntitySpot(const gentity_t *ent, const spot_t spot, vec3_t point);
qboolean InFOVFloat(gentity_t *ent, gentity_t *from, double hFOV, double vFOV)
{
	vec3_t	eyes;
	vec3_t	spot;
	vec3_t	deltaVector;
	vec3_t	angles, fromAngles;
	vec3_t	deltaAngles;

	if (from->client)
	{
		if (!VectorCompare(from->client->renderInfo.eyeAngles, vec3_origin))
		{//Actual facing of tag_head!
			//NOTE: Stasis aliens may have a problem with this?
			VectorCopy(from->client->renderInfo.eyeAngles, fromAngles);
		}
		else
		{
			VectorCopy(from->client->ps.viewangles, fromAngles);
		}
	}
	else
	{
		VectorCopy(from->s.angles, fromAngles);
	}

	CalcEntitySpot(from, SPOT_HEAD_LEAN, eyes);

	CalcEntitySpot(ent, SPOT_ORIGIN, spot);
	VectorSubtract(spot, eyes, deltaVector);

	vectoangles(deltaVector, angles);
	deltaAngles[PITCH] = AngleDelta(fromAngles[PITCH], angles[PITCH]);
	deltaAngles[YAW] = AngleDelta(fromAngles[YAW], angles[YAW]);
	if (fabs(deltaAngles[PITCH]) <= vFOV && fabs(deltaAngles[YAW]) <= hFOV)
	{
		return qtrue;
	}

	CalcEntitySpot(ent, SPOT_HEAD, spot);
	VectorSubtract(spot, eyes, deltaVector);
	vectoangles(deltaVector, angles);
	deltaAngles[PITCH] = AngleDelta(fromAngles[PITCH], angles[PITCH]);
	deltaAngles[YAW] = AngleDelta(fromAngles[YAW], angles[YAW]);
	if (fabs(deltaAngles[PITCH]) <= vFOV && fabs(deltaAngles[YAW]) <= hFOV)
	{
		return qtrue;
	}

	CalcEntitySpot(ent, SPOT_LEGS, spot);
	VectorSubtract(spot, eyes, deltaVector);
	vectoangles(deltaVector, angles);
	deltaAngles[PITCH] = AngleDelta(fromAngles[PITCH], angles[PITCH]);
	deltaAngles[YAW] = AngleDelta(fromAngles[YAW], angles[YAW]);
	if (fabs(deltaAngles[PITCH]) <= vFOV && fabs(deltaAngles[YAW]) <= hFOV)
	{
		return qtrue;
	}

	return qfalse;
}

static qboolean AimbotterHadLineOfSightToTarget(gentity_t *shooter, gentity_t *victim, int msAgo) {
#if 1
	return qtrue;
#else
	assert(shooter && victim);

	if (msAgo > 0) {
		assert(msAgo <= UNLAGGED_MAX_COMPENSATION);
		G_TimeShiftAllClients(trap_Milliseconds() - msAgo, NULL, qfalse);
	}

	trace_t tr;
	vec3_t start, end;
	VectorCopy(shooter->r.currentOrigin, start);
	VectorCopy(victim->r.currentOrigin, end);
	start[2] += (shooter->client->ps.viewheight / 2);
	end[2] += (victim->client->ps.viewheight / 2);
	trap_G2Trace(&tr, start, NULL, NULL, end, shooter->s.number, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);

	if (msAgo > 0)
		G_UnTimeShiftAllClients(NULL, qfalse);

	if (tr.entityNum != victim - g_entities || tr.startsolid)
		return qfalse; // couldn't trace to the target

	return qtrue;
#endif
}

extern qboolean G_IsMindTricked(forcedata_t *victimFd, int mindTricker);
#define ENTITYNUM_DONTSHOOTWHATSOEVER	(-1)
static int PlayerThatPlayerIsAimingClosestTo(gentity_t *ent, float hFOV, float maxDistance, qboolean trace, vec3_t muzzle, float size) {
	// check who is eligible to be followed
	qboolean validEnemy[MAX_CLIENTS] = { qfalse }, gotValid = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *thisEnt = &g_entities[i];
		if (thisEnt == ent || !thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED || IsRacerOrSpectator(thisEnt) || thisEnt->client->ps.fallingToDeath)
			continue;
		if (thisEnt->client->sess.sessionTeam != OtherTeam(ent->client->sess.sessionTeam))
			continue;
		if (thisEnt->health <= 0 || thisEnt->client->ps.pm_type == PM_SPECTATOR || thisEnt->client->tempSpectate >= level.time)
			continue; // this guy is dead
		if (thisEnt->client->pers.lastSpawnTime >= level.time - 2000)
			continue; // target can't get aimbotted within 2s of spawning
		if (!InFOVFloat(thisEnt, ent, hFOV, 45.0f))
			continue;
		if (G_IsMindTricked(&thisEnt->client->ps.fd, ent - g_entities))
			continue;
		if (!AimbotterHadLineOfSightToTarget(ent, thisEnt, 1000))
			continue; // didn't have line of sight to target 1s ago
		if (!AimbotterHadLineOfSightToTarget(ent, thisEnt, 0))
			continue; // doesn't have line of sight to target right now

		validEnemy[i] = qtrue;
		gotValid = qtrue;
	}
	if (!gotValid)
		return ENTITYNUM_NONE;

	// check for aiming directly at someone
	if (trace) {
		trace_t tr;
		vec3_t start, end, fwd;
		VectorCopy(ent->client->ps.origin, start);
		AngleVectors(ent->client->ps.viewangles, fwd, NULL, NULL);
		VectorMA(start, maxDistance, fwd, end);
		start[2] += ent->client->ps.viewheight;
		trap_G2Trace(&tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
		if (tr.entityNum >= 0 && tr.entityNum < MAX_CLIENTS) {
			gentity_t *directlyAimedTarget = &g_entities[tr.entityNum];

			if (directlyAimedTarget->health > 0 && validEnemy[tr.entityNum])
				return tr.entityNum;

			if (directlyAimedTarget->health > 0 && directlyAimedTarget->client && directlyAimedTarget->client->sess.sessionTeam == ent->client->sess.sessionTeam && Distance(ent->client->ps.origin, directlyAimedTarget->client->ps.origin) <= 64)
				return ENTITYNUM_DONTSHOOTWHATSOEVER; // we are directly aiming at a teammate super close; don't shoot anyone at all
		}
	}

	// see who was closest to where we aimed
	float closestDistance = maxDistance;
	int closestPlayerNum = ENTITYNUM_NONE;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!validEnemy[i])
			continue;
		gentity_t *thisEnt = &g_entities[i];
		float dist = Distance(ent->client->ps.origin, thisEnt->client->ps.origin);
		if (dist < closestDistance) {
			closestDistance = dist;
			closestPlayerNum = i;
		}
	}

	// make sure there's no ally in between us and this guy that would get hit instead
	if (closestPlayerNum != ENTITYNUM_NONE) {
		trace_t tr;
		vec3_t mins, maxs;
		VectorSet(maxs, size, size, size);
		VectorScale(maxs, -1, mins);
		trap_G2Trace(&tr, muzzle, mins, maxs, g_entities[closestPlayerNum].client->ps.origin, ent->s.number, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
		if (tr.entityNum >= 0 && tr.entityNum < MAX_CLIENTS) {
			gentity_t *inBetweenMeAndTarget = &g_entities[tr.entityNum];
			if (inBetweenMeAndTarget->health > 0 && inBetweenMeAndTarget->client && inBetweenMeAndTarget->client->sess.sessionTeam == ent->client->sess.sessionTeam && Distance(ent->client->ps.origin, inBetweenMeAndTarget->client->ps.origin) <= 256)
				return ENTITYNUM_DONTSHOOTWHATSOEVER; // there's a somewhat close ally in between me and the target; don't shoot anyone at all
		}
	}

	return closestPlayerNum;
}

// returns qfalse if the shot should not be fired whatsoever; qtrue otherwise
static qboolean CorrectBoostedAim(gentity_t *ent, vec3_t muzzle, vec3_t vec, float projectileSpeed, int weapon, qboolean altFire, float size) {
	if (!ent || !ent->client || !ent->client->account || !(ent->client->account->flags & ACCOUNTFLAG_BOOST_AIMBOTBOOST) || !g_boost.integer || IsRacerOrSpectator(ent))
		return qtrue;

	if (ent->client->pers.lastSpawnTime >= level.time - 2000)
		return qtrue; // aimbotter spawned too recently

	int targetNum = PlayerThatPlayerIsAimingClosestTo(ent, 45.0f, 2000.0f, qtrue, muzzle, size); // initial sweep for enemies
	if (targetNum == ENTITYNUM_DONTSHOOTWHATSOEVER)
		return qfalse; // directly aiming at a super close teammate; don't shoot at all

	if (targetNum == ENTITYNUM_NONE) {
		targetNum = PlayerThatPlayerIsAimingClosestTo(ent, 60.0f, 500.0f, qfalse, muzzle, size); // fallback wider angle sweep for closeby enemies
		if (targetNum == ENTITYNUM_DONTSHOOTWHATSOEVER)
			return qfalse; // directly aiming at a super close teammate; don't shoot at all
	}

	float closestAllyInFovDistance = 999999, closestEnemyInFovDistance = 999999;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *closeGuy = &g_entities[i];
		if (!closeGuy->inuse || !closeGuy->client || closeGuy->client->pers.connected != CON_CONNECTED || IsRacerOrSpectator(closeGuy) || closeGuy->health <= 0 || closeGuy == ent || closeGuy->client->ps.fallingToDeath)
			continue;

		if (!InFOVFloat(closeGuy, ent, 60.0f, 45.0f))
			continue;

		float dist = Distance(ent->client->ps.origin, closeGuy->client->ps.origin);

		if (closeGuy->client->sess.sessionTeam == ent->client->sess.sessionTeam) {
			if (dist < closestAllyInFovDistance)
				closestAllyInFovDistance = dist;
		}
		else if (closeGuy->client->sess.sessionTeam == OtherTeam(ent->client->sess.sessionTeam)) {
			if (dist < closestEnemyInFovDistance)
				closestEnemyInFovDistance = dist;
		}
	}

	if (closestEnemyInFovDistance <= 768) {
		if (closestAllyInFovDistance <= 64 && closestAllyInFovDistance <= closestEnemyInFovDistance)
			return qfalse; // enemy nearby; don't allow the shot if ally in fov is super close
	}
	else {
		if (closestAllyInFovDistance <= 256 && closestAllyInFovDistance <= closestEnemyInFovDistance)
			return qfalse; // no enemies nearby; don't allow the shot if ally in fov is somewhat close
	}

	if (targetNum == ENTITYNUM_NONE) {
		return qtrue; // fire the shot at nothing in particular
	}
	else {
		// fire the shot at the target
	}

	assert(targetNum < MAX_CLIENTS);
	gentity_t *target = &g_entities[targetNum];
	vec3_t enemyPos, shooterPos;
	VectorCopy(target->r.currentOrigin, enemyPos);
	VectorCopy(muzzle, shooterPos);

	// get mins/maxs so we don't shoot ourself
	vec3_t mins, maxs;
	VectorSet(maxs, size, size, size);
	VectorScale(maxs, -1, mins);

	// get enemy velocity
	vec3_t enemyVelocity;
#if 0
		// current velocity
		VectorCopy(target->s.pos.trDelta, enemyVelocity);
#else
		// rewind to get the enemy's velocity shortly before shooting
		int now = trap_Milliseconds();

#define PROJECTILEAIMBOT_ORIGIN_DELTATIME_1				(250)
#define PROJECTILEAIMBOT_ORIGIN_DELTATIME_DIFF			(33)
#define PROJECTILEAIMBOT_ORIGIN_DELTATIME_2				(PROJECTILEAIMBOT_ORIGIN_DELTATIME_1 - PROJECTILEAIMBOT_ORIGIN_DELTATIME_DIFF)
#define PROJECTILEAIMBOT_ORIGIN_DELTATIMEDIFF_SECONDS	(PROJECTILEAIMBOT_ORIGIN_DELTATIME_DIFF / 1000.0f)

		// get their origin 250ms ago
		G_TimeShiftClient(target, now - PROJECTILEAIMBOT_ORIGIN_DELTATIME_1, qfalse);
		vec3_t pos1;
		VectorCopy(target->r.currentOrigin, pos1);
		G_UnTimeShiftClient(target, qfalse);

		// get their origin 217ms ago
		G_TimeShiftClient(target, now - PROJECTILEAIMBOT_ORIGIN_DELTATIME_2, qfalse);
		vec3_t pos2;
		VectorCopy(target->r.currentOrigin, pos2);
		G_UnTimeShiftClient(target, qfalse);

		// calculate their velocity 250ms ago using these
		enemyVelocity[0] = (pos2[0] - pos1[0]) / PROJECTILEAIMBOT_ORIGIN_DELTATIMEDIFF_SECONDS;
		enemyVelocity[1] = (pos2[1] - pos1[1]) / PROJECTILEAIMBOT_ORIGIN_DELTATIMEDIFF_SECONDS;
		enemyVelocity[2] = (pos2[2] - pos1[2]) / PROJECTILEAIMBOT_ORIGIN_DELTATIMEDIFF_SECONDS;
#endif

		{
			int noise = 0;
			if (target->client->ps.groundEntityNum != ENTITYNUM_NONE) {
				// target is on the ground
				if (weapon == WP_ROCKET_LAUNCHER || weapon == WP_CONCUSSION || weapon == WP_THERMAL || (altFire && weapon == WP_REPEATER))
					enemyPos[2] -= 18; // aim splash weapons at feet
			}
			else {
				// target is in the air
				noise += 1;
			}

			if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
				noise += 1;
			if (ent->client->pers.lastSpawnTime >= level.time - 3000)
				noise += 1;

			int enemyXyVel = sqrt(enemyVelocity[0] * enemyVelocity[0] + enemyVelocity[1] * enemyVelocity[1]);
			if (enemyXyVel >= 1500)
				noise += 6;
			else if (enemyXyVel >= 1250)
				noise += 5;
			else if (enemyXyVel >= 1000)
				noise += 4;
			else if (enemyXyVel >= 750)
				noise += 3;
			else if (enemyXyVel >= 500)
				noise += 2;
			else if (enemyXyVel > 0)
				noise += 1;

			int enemyZVel = fabs(enemyVelocity[2]);
			if (enemyZVel >= 750)
				noise += 3;
			else if (enemyZVel >= 500)
				noise += 2;

			float dist = Distance2D(ent->client->ps.origin, target->client->ps.origin);
			if (dist >= 2000)
				noise += 2;
			else if (dist >= 1000)
				noise += 1;
			else if (dist <= 128)
				noise -= 1;

			if (!AimbotterHadLineOfSightToTarget(ent, target, 2000))
				noise += 1;

			int rng = Q_irand(1, 10);
			if (rng == 1)
				noise += 2;
			else if (rng <= 3)
				noise += 1;

#define PROJECTILEAIMBOT_NOISE_COEFFICIENT		(5)
			float enemyPosNoiseRange = noise * PROJECTILEAIMBOT_NOISE_COEFFICIENT;
			if (noise > 0) {
				enemyPos[0] = Q_irand(enemyPos[0] - enemyPosNoiseRange, enemyPos[0] + enemyPosNoiseRange);
				enemyPos[1] = Q_irand(enemyPos[1] - enemyPosNoiseRange, enemyPos[1] + enemyPosNoiseRange);
				//enemyPos[2] = Q_irand(enemyPos[2] - enemyPosNoiseRange, enemyPos[2] + enemyPosNoiseRange);
			}
		}

	if (weapon == WP_THERMAL || (altFire && weapon == WP_REPEATER)) { // quick and dirty approximation for arced weapons
		vec3_t diffVec, estimatedEnemyPos;
		float horizontalDistance, heightDifference, timeToImpact;

		VectorSubtract(enemyPos, shooterPos, diffVec);
		horizontalDistance = sqrt(diffVec[0] * diffVec[0] + diffVec[1] * diffVec[1]);
		timeToImpact = horizontalDistance / projectileSpeed;

		float linearScale, exponentialScale;
		if (weapon == WP_REPEATER) {
			linearScale = 0.00015f;
			exponentialScale = 0.000000000125f;
		}
		else {
			linearScale = 0.000075f;
			exponentialScale = 0.00000000025f;
		}

		float distanceFactor = linearScale + exponentialScale * pow(horizontalDistance, 2);
		timeToImpact *= (1 + distanceFactor);
		VectorMA(enemyPos, timeToImpact, enemyVelocity, estimatedEnemyPos);
		VectorSubtract(estimatedEnemyPos, shooterPos, diffVec);

		heightDifference = diffVec[2];
		float linearScaleHeight, exponentialScaleHeight;
		if (heightDifference > 0) { // target is higher
			if (weapon == WP_REPEATER) {
				linearScaleHeight = 0.0f;
				exponentialScaleHeight = 0.0015f;
			}
			else {
				linearScaleHeight = 0.0f;
				exponentialScaleHeight = 0.005f;
			}
		}
		else { // target is lower
			if (weapon == WP_REPEATER) {
				linearScaleHeight = 0.0f;
				exponentialScaleHeight = 0.0003f;
			}
			else {
				linearScaleHeight = 0.0f;
				exponentialScaleHeight = 0.0003f;
			}
		}

		float heightFactor = linearScaleHeight + exponentialScaleHeight * pow(heightDifference, 2);
		float yawAngle = atan2(diffVec[1], diffVec[0]);
		float pitchAngle = atan((heightDifference + heightFactor) / horizontalDistance + horizontalDistance * distanceFactor);

		vec3_t possiblyFinalVec;
		possiblyFinalVec[0] = cos(pitchAngle) * cos(yawAngle);
		possiblyFinalVec[1] = cos(pitchAngle) * sin(yawAngle);
		possiblyFinalVec[2] = sin(pitchAngle);

		// make sure you don't shoot yourself
		trace_t trace;
		vec3_t endPoint;
		VectorMA(shooterPos, 150, possiblyFinalVec, endPoint);
		trap_Trace(&trace, shooterPos, mins, maxs, endPoint, ent->s.number, MASK_SHOT);
		if ((trace.fraction == 1.0f && !trace.startsolid) || trace.entityNum == target->s.number)
			VectorCopy(possiblyFinalVec, vec);
	}
	else { // straight line weapons
		vec3_t diffVec, predictedPos;
		float distance, timeToImpact;

		VectorSubtract(enemyPos, shooterPos, diffVec);
		distance = VectorLength(diffVec);
		timeToImpact = distance / projectileSpeed;
		VectorMA(enemyPos, timeToImpact, enemyVelocity, predictedPos);
		vec3_t possiblyFinalVec;
		VectorSubtract(predictedPos, shooterPos, possiblyFinalVec);
		VectorNormalize(possiblyFinalVec);

		// make sure you don't shoot yourself
		trace_t trace;
		vec3_t endPoint;
		VectorMA(shooterPos, 150, possiblyFinalVec, endPoint);
		trap_Trace(&trace, shooterPos, mins, maxs, endPoint, ent->s.number, MASK_SHOT);
		if ((trace.fraction == 1.0f && !trace.startsolid) || trace.entityNum == target->s.number)
			VectorCopy(possiblyFinalVec, vec);
	}

	return qtrue;
}

//-----------------------------------------------------------------------------
void W_TraceSetStart( gentity_t *ent, vec3_t start, vec3_t mins, vec3_t maxs )
//-----------------------------------------------------------------------------
{
	//make sure our start point isn't on the other side of a wall
	trace_t	tr;
	vec3_t	entMins;
	vec3_t	entMaxs;
	vec3_t	eyePoint;

	VectorAdd( ent->r.currentOrigin, ent->r.mins, entMins );
	VectorAdd( ent->r.currentOrigin, ent->r.maxs, entMaxs );

	if ( G_BoxInBounds( start, mins, maxs, entMins, entMaxs ) )
	{
		return;
	}

	if ( !ent->client )
	{
		return;
	}

	VectorCopy( ent->s.pos.trBase, eyePoint);
	eyePoint[2] += ent->client->ps.viewheight;
		
	trap_Trace( &tr, eyePoint, mins, maxs, start, ent->s.number, MASK_SOLID|CONTENTS_SHOTCLIP );

	if ( tr.startsolid || tr.allsolid )
	{
		return;
	}

	if ( tr.fraction < 1.0f )
	{
		VectorCopy( tr.endpos, start );
	}
}


/*
----------------------------------------------
	PLAYER WEAPONS
----------------------------------------------
*/

/*
======================================================================

BRYAR PISTOL

======================================================================
*/

//----------------------------------------------
static void WP_FireBryarPistol( gentity_t *ent, qboolean altFire )
//---------------------------------------------------------
{
	int damage = BRYAR_PISTOL_DAMAGE;

	float boostSize = 0;
	if (altFire) {
		int chargeTime;
		if (g_fixWeaponChargeTime.integer)
			chargeTime = (ent->client->pers.cmd.serverTime - ent->client->ps.weaponChargeTime);
		else
			chargeTime = (level.time - ent->client->ps.weaponChargeTime);
		if (chargeTime < 1)
			chargeTime = 1;
		int count = Com_Clampi(1, 5, (chargeTime) / BRYAR_CHARGE_UNIT);
		boostSize = BRYAR_ALT_SIZE * (count * 0.5);
	}

	if (!CorrectBoostedAim(ent, muzzle, forward, BRYAR_PISTOL_VEL, WP_BRYAR_PISTOL, altFire, boostSize))
		return;
	gentity_t	*missile = CreateMissile( muzzle, forward, BRYAR_PISTOL_VEL, 10000, ent, altFire );

	missile->classname = "bryar_proj";
	missile->s.weapon = WP_BRYAR_PISTOL;
	int count;
	if ( altFire )
	{
		float boxSize = 0;

		int chargeTime;
		if (g_fixWeaponChargeTime.integer)
			chargeTime = (ent->client->pers.cmd.serverTime - ent->client->ps.weaponChargeTime);
		else
			chargeTime = (level.time - ent->client->ps.weaponChargeTime);
		if (chargeTime < 1)
			chargeTime = 1;
		count = ( chargeTime ) / BRYAR_CHARGE_UNIT;

		if ( count < 1 )
		{
			count = 1;
		}
		else if ( count > 5 )
		{
			count = 5;
		}

		if (count > 1)
		{
			damage *= (count*1.7);
			//ent->client->stats->accuracy_shots++;
			ent->client->stats->accuracy_shotsOfType[ACC_PISTOL_ALT]++;
		}
		else
		{
			damage *= (count*1.5);
		}

		missile->s.generic1 = count; // The missile will then render according to the charge level.

		boxSize = BRYAR_ALT_SIZE*(count*0.5);

		VectorSet( missile->r.maxs, boxSize, boxSize, boxSize );
		VectorSet( missile->r.mins, -boxSize, -boxSize, -boxSize );
	}

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	if (altFire)
	{
		missile->methodOfDeath = MOD_BRYAR_PISTOL_ALT;
	}
	else
	{
		missile->methodOfDeath = MOD_BRYAR_PISTOL;
	}
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

/*
======================================================================

GENERIC

======================================================================
*/

//---------------------------------------------------------
void WP_FireTurretMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire, int damage, int velocity, int mod, gentity_t *ignore )
//---------------------------------------------------------
{
	gentity_t *missile;

	missile = CreateMissile( start, dir, velocity, 10000, ent, altFire );

	missile->classname = "generic_proj";
	missile->s.weapon = WP_TURRET;

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = mod;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	if (ignore)
	{
		missile->passThroughNum = ignore->s.number+1;
	}

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//Currently only the seeker drone uses this, but it might be useful for other things as well.

//---------------------------------------------------------
void WP_FireGenericBlasterMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire, int damage, int velocity, int mod )
//---------------------------------------------------------
{
	gentity_t *missile;

	missile = CreateMissile( start, dir, velocity, 10000, ent, altFire );

	missile->classname = "generic_proj";
	missile->s.weapon = WP_BRYAR_PISTOL;

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = mod;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

/*
======================================================================

BLASTER

======================================================================
*/

//---------------------------------------------------------
void WP_FireBlasterMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire )
//---------------------------------------------------------
{
	int velocity	= BLASTER_VELOCITY;
	int	damage		= BLASTER_DAMAGE;
	gentity_t *missile;

	if (ent->s.eType == ET_NPC)
	{ //animent
		damage = 10;
	}

	if (!CorrectBoostedAim(ent, start, dir, velocity, WP_BLASTER, altFire, 0))
		return;
	missile = CreateMissile( start, dir, velocity, 10000, ent, altFire );

	missile->classname = "blaster_proj";
	missile->s.weapon = WP_BLASTER;

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_BLASTER;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
void WP_FireTurboLaserMissile( gentity_t *ent, vec3_t start, vec3_t dir )
//---------------------------------------------------------
{
	int velocity	= ent->mass; //FIXME: externalize
	gentity_t *missile;

	missile = CreateMissile( start, dir, velocity, 10000, ent, qfalse );
	
	//use a custom shot effect
	missile->s.otherEntityNum2 = ent->genericValue14;
	//use a custom impact effect
	missile->s.emplacedOwner = ent->genericValue15;

	missile->classname = "turbo_proj";
	missile->s.weapon = WP_TURRET;

	missile->damage = ent->damage;		//FIXME: externalize
	missile->splashDamage = ent->splashDamage;	//FIXME: externalize
	missile->splashRadius = ent->splashRadius;	//FIXME: externalize
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_TURBLAST; //count as a heavy weap
	missile->splashMethodOfDeath = MOD_TURBLAST;
	missile->clipmask = MASK_SHOT;

	// we don't want it to bounce forever
	missile->bounceCount = 8;

	//set veh as cgame side owner for purpose of fx overrides
	missile->s.owner = ent->s.number;

	//don't let them last forever
	missile->think = G_FreeEntity;
	missile->nextthink = level.time + 5000;//at 20000 speed, that should be more than enough
}

//---------------------------------------------------------
void WP_FireEmplacedMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire, gentity_t *ignore )
//---------------------------------------------------------
{
	int velocity	= BLASTER_VELOCITY;
	int	damage		= BLASTER_DAMAGE;
	gentity_t *missile;

	missile = CreateMissile( start, dir, velocity, 10000, ent, altFire );

	missile->classname = "emplaced_gun_proj";
	missile->s.weapon = WP_TURRET;

	missile->activator = ignore;

	missile->damage = damage;
	missile->dflags = (DAMAGE_DEATH_KNOCKBACK|DAMAGE_HEAVY_WEAP_CLASS);
	missile->methodOfDeath = MOD_VEHICLE;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	if (ignore)
	{
		missile->passThroughNum = ignore->s.number+1;
	}

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
static void WP_FireBlaster( gentity_t *ent, qboolean altFire )
//---------------------------------------------------------
{
	vec3_t	dir, angs;

	vectoangles( forward, angs );

	if ( altFire )
	{
		// add some slop to the alt-fire direction
		angs[PITCH] += crandom() * BLASTER_SPREAD;
		angs[YAW]	+= crandom() * BLASTER_SPREAD;
	}

	AngleVectors( angs, dir, NULL, NULL );

	// FIXME: if temp_org does not have clear trace to inside the bbox, don't shoot!
	WP_FireBlasterMissile( ent, muzzle, dir, altFire );
}



int G_GetHitLocation(gentity_t *target, vec3_t ppoint);

extern bot_state_t *botstates[MAX_CLIENTS];

/*
======================================================================

DISRUPTOR

======================================================================
*/
//---------------------------------------------------------
static void WP_DisruptorMainFire( gentity_t *ent )
//---------------------------------------------------------
{
	int			damage = DISRUPTOR_MAIN_DAMAGE;
	qboolean	render_impact = qtrue;
	vec3_t		start, end;
	trace_t		tr;
	gentity_t	*traceEnt, *tent;
	float		shotRange = 8192;
	int			ignore, traces;
	int			hits = 0;

	if (ent->client->sess.inRacemode) {
		// make tracing work temporarily; we will switch them back before returning
		ent->r.svFlags |= SVF_COOLKIDSCLUB;
		ent->r.svFlags &= ~SVF_GHOST;

		if (ent->aimPracticeEntBeingUsed) {
			ent->r.singleEntityCollision = qtrue;
			ent->r.singleEntityThatCanCollide = ent->aimPracticeEntBeingUsed - g_entities;
		}
	}

	if ( g_gametype.integer == GT_SIEGE )
	{
		damage = DISRUPTOR_MAIN_DAMAGE_SIEGE;
	}

	memset(&tr, 0, sizeof(tr)); //to shut the compiler up

	VectorCopy( ent->client->ps.origin, start );
	start[2] += ent->client->ps.viewheight;//By eyes

#define AIMBOTBOOST_DISRUPT_CHANCETOHIT		(15)		// out of 100

	gentity_t *botTarget = NULL;
	vec3_t botTargetAimSpot = { 0.0f };
	if ((ent->r.svFlags & SVF_BOT) && g_botAimbot.integer && ent - g_entities < MAX_CLIENTS && botstates[ent - g_entities]->currentEnemy && botstates[ent - g_entities]->currentEnemy->client) {
		botTarget = botstates[ent - g_entities]->currentEnemy;
		VectorCopy(botTarget->client->ps.origin, botTargetAimSpot);
		botTargetAimSpot[2] += botTarget->client->ps.viewheight;
	}
	else if (ent->client && ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_BOOST_AIMBOTBOOST) && ent - g_entities < MAX_CLIENTS && g_aimbotBoost_hitscan.integer && Q_irand(1, 100) <= AIMBOTBOOST_DISRUPT_CHANCETOHIT) {
		int botTargetNum = PlayerThatPlayerIsAimingClosestTo(ent, 45, 512, qfalse, start, 0);
		if (botTargetNum == ENTITYNUM_DONTSHOOTWHATSOEVER) {
			return;
		}
		else if (botTargetNum >= 0 && botTargetNum < MAX_CLIENTS) {
			botTarget = &g_entities[botTargetNum];
			if (botTarget) {
				VectorCopy(botTarget->client->ps.origin, botTargetAimSpot);
				//botTargetAimSpot[2] += botTarget->client->ps.viewheight; // commented out so that aimboosters do not headshot
			}
		}
	}

	VectorMA( start, shotRange, forward, end );
	vec3_t unchangedEnd;
	VectorCopy(end, unchangedEnd);

	qboolean compensate = ent && ent->client ? ent->client->sess.unlagged : qfalse;
	qboolean ghoul2 = !!(d_projectileGhoul2Collision.integer);
	if (g_unlagged.integer && compensate)
		G_TimeShiftAllClients(trap_Milliseconds() - (level.time - ent->client->pers.cmd.serverTime), ent, ghoul2);

	ignore = ent->s.number;
	traces = 0;
	int botAttempts = 0;
	while ( traces < 10 )
	{//need to loop this in case we hit a Jedi who dodges the shot

		// if this is an aimbotter, gradually adjust the aim spot by increasingly random amounts until we hit the target
#define MAX_BOT_ATTEMPTS	(64)
		if (botAttempts && botAttempts < MAX_BOT_ATTEMPTS && botTarget) {
			VectorCopy(botTargetAimSpot, end);
			end[0] += Q_irand(-botAttempts, botAttempts);
			end[1] += Q_irand(-botAttempts, botAttempts);
			end[2] += Q_irand(-botAttempts, botAttempts);
			if (BG_InRoll(&botTarget->client->ps, botTarget->client->ps.legsAnim))
				end[2] -= botTarget->client->ps.viewheight;
		}

		// hack to include rockets in the trace
		if (g_rocketHPFix.integer == 2)
			SetRocketContents(MASK_SHOT);

		if (d_projectileGhoul2Collision.integer)
		{
			trap_G2Trace( &tr, start, NULL, NULL, end, ignore, MASK_SHOT, G2TRFLAG_DOGHOULTRACE|G2TRFLAG_GETSURFINDEX|G2TRFLAG_THICK|G2TRFLAG_HITCORPSES, g_g2TraceLod.integer );
		}
		else
		{
			trap_Trace( &tr, start, NULL, NULL, end, ignore, MASK_SHOT );
		}

		if (g_rocketHPFix.integer == 2)
			SetRocketContents(0);

		if (botTarget && tr.entityNum != botTarget - g_entities && !(tr.entityNum < ENTITYNUM_MAX_NORMAL && g_entities[tr.entityNum].client)) {
			if (botAttempts < MAX_BOT_ATTEMPTS) {
				botAttempts++;
				continue;
			}
			else if (botAttempts == MAX_BOT_ATTEMPTS) { // tried to find a trace to the target too many times; give up
				botAttempts++;
				VectorCopy(unchangedEnd, end);
				continue;
			}
		}
		//Com_Printf("botAttempts: %d, xdif: %0.1f, ydif: %0.1f, zdif: %0.1f\n", botAttempts, fabs(end[0] - originalEnd[0]), fabs(end[1] - originalEnd[1]), fabs(end[2] - originalEnd[2]));
		botAttempts = 0;

		traceEnt = &g_entities[tr.entityNum];

		if (d_projectileGhoul2Collision.integer && traceEnt->inuse && traceEnt->client)
		{ //g2 collision checks -rww
			if (traceEnt->inuse && traceEnt->client && traceEnt->ghoul2)
			{ //since we used G2TRFLAG_GETSURFINDEX, tr.surfaceFlags will actually contain the index of the surface on the ghoul2 model we collided with.
				traceEnt->client->g2LastSurfaceHit = tr.surfaceFlags;
				traceEnt->client->g2LastSurfaceTime = level.time;
			}

			if (traceEnt->ghoul2)
			{
				tr.surfaceFlags = 0; //clear the surface flags after, since we actually care about them in here.
			}
		}

		//bug fix - cant shoot ourselves
		if ( tr.entityNum == ent->s.number )
		{
			ignore = tr.entityNum;
			VectorCopy(tr.endpos, start);
			traces++;
			continue;
		}

		// can't shoot projectiles of people in racemode
		if ( traceEnt && traceEnt->s.eType == ET_MISSILE && traceEnt->r.ownerNum >= 0 && traceEnt->r.ownerNum < MAX_CLIENTS && level.clients[traceEnt->r.ownerNum].sess.inRacemode )
		{
			ignore = tr.entityNum;
			VectorCopy( tr.endpos, start );
			traces++;
			continue;
		}

		if (traceEnt && traceEnt->client && traceEnt->client->ps.duelInProgress &&
			traceEnt->client->ps.duelIndex != ent->s.number)
		{
			VectorCopy( tr.endpos, start );
			ignore = tr.entityNum;
			traces++;
			continue;
		}

		if ( Jedi_DodgeEvasion( traceEnt, ent, &tr, G_GetHitLocation(traceEnt, tr.endpos) ) )
		{//act like we didn't even hit him
			VectorCopy( tr.endpos, start );
			ignore = tr.entityNum;
			traces++;
			continue;
		}
		else if (traceEnt && traceEnt->client /*&& traceEnt->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] >= FORCE_LEVEL_3*/)
		{ // alpha: moved the saber def 3 check above inside WP_SaberCanBlock
			if (WP_SaberCanBlock(traceEnt, ent, tr.endpos, 0, MOD_DISRUPTOR, qtrue, 0, NULL))
			{ //broadcast and stop the shot because it was blocked
				gentity_t *te = NULL;

				tent = G_TempEntity( tr.endpos, EV_DISRUPTOR_MAIN_SHOT );
				VectorCopy( muzzle, tent->s.origin2 );
				tent->s.eventParm = ent->s.number;
				G_ApplyRaceBroadcastsToEvent( ent, tent );

				te = G_TempEntity( tr.endpos, EV_SABER_BLOCK );
				G_ApplyRaceBroadcastsToEvent( ent, te );
				VectorCopy(tr.endpos, te->s.origin);
				VectorCopy(tr.plane.normal, te->s.angles);
				if (!te->s.angles[0] && !te->s.angles[1] && !te->s.angles[2])
				{
					te->s.angles[1] = 1;
				}
				te->s.eventParm = 0;
				te->s.weapon = 0;//saberNum
				te->s.legsAnim = 0;//bladeNum

				if (g_unlagged.integer && compensate)
					G_UnTimeShiftAllClients(ent, ghoul2);

				if (ent->client->sess.inRacemode) {
					// restore svFlags from before
					ent->r.svFlags &= ~SVF_COOLKIDSCLUB;
					ent->r.svFlags |= SVF_GHOST;

					ent->r.singleEntityCollision = qfalse;
					ent->r.singleEntityThatCanCollide = 0;
				}

				return;
			}
		}
		else if ( (traceEnt->flags&FL_SHIELDED) )
		{//stopped cold
			if (ent->client->sess.inRacemode) {
				// restore svFlags from before
				ent->r.svFlags &= ~SVF_COOLKIDSCLUB;
				ent->r.svFlags |= SVF_GHOST;

				ent->r.singleEntityCollision = qfalse;
				ent->r.singleEntityThatCanCollide = 0;
			}
			return;
		}
		//a Jedi is not dodging this shot
		break;
	}

	if (g_unlagged.integer && compensate)
		G_UnTimeShiftAllClients(ent, ghoul2);

	if ( tr.surfaceFlags & SURF_NOIMPACT ) 
	{
		render_impact = qfalse;
	}

	// always render a shot beam, doing this the old way because I don't much feel like overriding the effect.
	tent = G_TempEntity( tr.endpos, EV_DISRUPTOR_MAIN_SHOT );
	VectorCopy( muzzle, tent->s.origin2 );
	tent->s.eventParm = ent->s.number;
	G_ApplyRaceBroadcastsToEvent( ent, tent );

	traceEnt = &g_entities[tr.entityNum];

	if ( render_impact /*&& (traces || traceEnt != ent)*/) // *CHANGE 62* distruptor do dmg only to other entites
	{
		if ( tr.entityNum < ENTITYNUM_WORLD && traceEnt->takedamage )
		{
			if (traceEnt->isAimPracticePack) {
				if (ent->aimPracticeEntBeingUsed == traceEnt && ent->numAimPracticeSpawns >= 1 && ent->aimPracticeMode == AIMPRACTICEMODE_TIMED) {
					//++ent->numTotalAimPracticeHits;
					//++ent->numAimPracticeHitsOfWeapon[WP_DISRUPTOR];
					CenterPrintToPlayerAndFollowers(ent, "Use scope! Primary shots don't count.");
				}
				PlayAimPracticeBotPainSound(traceEnt, ent);
			}

			if ( traceEnt->client && LogAccuracyHit( traceEnt, ent )) {
				hits++;
			} 

			if (g_rocketHPFix.integer == 2 && VALIDSTRING(traceEnt->classname) && !Q_stricmp(traceEnt->classname, "rocket_proj"))
				G_Damage(traceEnt, ent, ent, forward, tr.endpos, 9999999, DAMAGE_NORMAL, MOD_DISRUPTOR);
			else
				G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, DAMAGE_NORMAL, MOD_DISRUPTOR );
			
			tent = G_TempEntity( tr.endpos, EV_DISRUPTOR_HIT );
			tent->s.eventParm = DirToByte( tr.plane.normal );
			G_ApplyRaceBroadcastsToEvent( ent, tent );

			if (traceEnt->client)
			{
				tent->s.weapon = 1;
			}
		}
		else 
		{
			 // Hmmm, maybe don't make any marks on things that could break
			tent = G_TempEntity( tr.endpos, EV_DISRUPTOR_SNIPER_MISS );
			tent->s.eventParm = DirToByte( tr.plane.normal );
			tent->s.weapon = 1;
			G_ApplyRaceBroadcastsToEvent( ent, tent );
		}
	}
	// give the shooter a reward sound if they have made two railgun hits in a row
	if ( hits == 0 ) {
		// complete miss
		ent->client->accurateCount = 0;
	} else {
		// check for "impressive" reward sound
		ent->client->accurateCount += hits;
		if ( ent->client->accurateCount >= 2 ) {
			ent->client->accurateCount -= 2;
			ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
			// add the sprite over the player's head
			//ent->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
			//ent->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
			//ent->client->rewardTime = level.time + REWARD_SPRITE_TIME;
		}
		ent->client->stats->accuracy_hits++;
		ent->client->stats->accuracy_hitsOfType[ACC_DISRUPTOR_PRIMARY]++;
	}

	if (ent->client->sess.inRacemode) {
		// restore svFlags from before
		ent->r.svFlags &= ~SVF_COOLKIDSCLUB;
		ent->r.svFlags |= SVF_GHOST;

		ent->r.singleEntityCollision = qfalse;
		ent->r.singleEntityThatCanCollide = 0;
	}
}


qboolean G_CanDisruptify(gentity_t *ent)
{
	if (!ent || !ent->inuse || !ent->client || ent->s.eType != ET_NPC ||
		ent->s.NPC_class != CLASS_VEHICLE || !ent->m_pVehicle)
	{ //not vehicle
		return qtrue;
	}

	if (ent->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL)
	{ //animal is only type that can be disintigeiteigerated
		return qtrue;
	}

	//don't do it to any other veh
	return qfalse;
}

#define maxOf(a,b) (((a) > (b)) ? (a) : (b))
#define minOf(a,b) (((a) < (b)) ? (a) : (b))

//---------------------------------------------------------
void WP_DisruptorAltFire( gentity_t *ent )
//---------------------------------------------------------
{
	int			damage = 0, skip;
	qboolean	render_impact = qtrue;
	vec3_t		start, end;
	trace_t		tr;
	gentity_t	*traceEnt, *tent;
	float		shotRange = 8192.0f;
	int			i;
	int			count, maxCount = 60;
	int			traces = DISRUPTOR_ALT_TRACES;
	qboolean	fullCharge = qfalse;
	int			hits = 0, disintegrations = 0;
	int			deathTorsoAnim[16] = { 0 }, deathLegsAnim[16] = { 0 }, deathClientNum[16] = { -1 }, numKilled = 0;

	if (ent->client->sess.inRacemode) {
		// make tracing work temporarily; we will switch them back before returning
		ent->r.svFlags |= SVF_COOLKIDSCLUB;
		ent->r.svFlags &= ~SVF_GHOST;

		if (ent->aimPracticeEntBeingUsed) {
			ent->r.singleEntityCollision = qtrue;
			ent->r.singleEntityThatCanCollide = ent->aimPracticeEntBeingUsed - g_entities;
		}
	}

	float chargeTime;
	if (g_sniperDamageNerf.integer) {
		if (ent->client) {
			VectorCopy(ent->client->ps.origin, start);
			start[2] += ent->client->ps.viewheight;

			chargeTime = (g_fixWeaponChargeTime.integer) ?
				(ent->client->pers.cmd.serverTime - ent->client->ps.weaponChargeTime) :
				(level.time - ent->client->ps.weaponChargeTime);

			if (chargeTime < 1)
				chargeTime = 1;
		}
		else
		{
			VectorCopy(ent->r.currentOrigin, start);
			start[2] += 24;
			chargeTime = 100;
		}

		if (chargeTime >= 1500) {
			damage = 130;
			fullCharge = qtrue;
		}
		else {
			damage = 50 + (int)(((130 - 50) / 1500.0f) * chargeTime);
		}

		if (chargeTime < 250) {
			traces = 1;
		}
		else if (chargeTime < 500) {
			traces = 2;
		}
		else {
			traces = 3;
		}
	}
	else {
		damage = DISRUPTOR_ALT_DAMAGE - 30;

		if (ent->client)
		{
			VectorCopy(ent->client->ps.origin, start);
			start[2] += ent->client->ps.viewheight;//By eyes

			if (g_fixWeaponChargeTime.integer)
				chargeTime = (ent->client->pers.cmd.serverTime - ent->client->ps.weaponChargeTime);
			else
				chargeTime = (level.time - ent->client->ps.weaponChargeTime);
			if (chargeTime < 1)
				chargeTime = 1;

			if (g_fixWeaponChargeTime.integer)
				count = (ent->client->pers.cmd.serverTime - ent->client->ps.weaponChargeTime) / DISRUPTOR_CHARGE_UNIT;
			else
				count = (level.time - ent->client->ps.weaponChargeTime) / DISRUPTOR_CHARGE_UNIT;
		}
		else
		{
			VectorCopy(ent->r.currentOrigin, start);
			start[2] += 24;

			count = (100) / DISRUPTOR_CHARGE_UNIT;
			chargeTime = 100;
		}

		count *= 2;

		if (count < 1)
		{
			count = 1;
		}
		else if (count >= maxCount)
		{
			count = maxCount;
			fullCharge = qtrue;
		}

		// more powerful charges go through more things
		if (count < 10)
		{
			traces = 1;
		}
		else if (count < 20)
		{
			traces = 2;
		}

		damage += count;
	}

	if (d_debugSnipeDamage.integer && ent->client)
		PrintIngame(-1, "(Nerf %s^7) %s^7 charged for %d ms, damage is %d, traces is %d\n", g_sniperDamageNerf.integer ? "^2on" : "^1off", ent->client->pers.netname, (int)chargeTime, damage, traces);
	
	skip = ent->s.number;

	qboolean compensate =  ent && ent->client ? ent->client->sess.unlagged : qfalse;
	qboolean ghoul2 = !!(d_projectileGhoul2Collision.integer);
	if (g_unlagged.integer && compensate)
		G_TimeShiftAllClients(trap_Milliseconds() - (level.time - ent->client->pers.cmd.serverTime), ent, ghoul2);

	for (i = 0; i < traces; i++ )
	{
		VectorMA( start, shotRange, forward, end );

		// hack to include rockets in the trace
		if (g_rocketHPFix.integer == 2)
			SetRocketContents(MASK_SHOT);

		if (d_projectileGhoul2Collision.integer)
		{
			trap_G2Trace( &tr, start, NULL, NULL, end, skip, MASK_SHOT, G2TRFLAG_DOGHOULTRACE|G2TRFLAG_GETSURFINDEX|G2TRFLAG_THICK|G2TRFLAG_HITCORPSES, g_g2TraceLod.integer );
		}
		else
		{
			trap_Trace( &tr, start, NULL, NULL, end, skip, MASK_SHOT );
		}

		if (g_rocketHPFix.integer == 2)
			SetRocketContents(0);

		traceEnt = &g_entities[tr.entityNum];

		if (d_projectileGhoul2Collision.integer && traceEnt->inuse && traceEnt->client)
		{ //g2 collision checks -rww
			if (traceEnt->inuse && traceEnt->client && traceEnt->ghoul2)
			{ //since we used G2TRFLAG_GETSURFINDEX, tr.surfaceFlags will actually contain the index of the surface on the ghoul2 model we collided with.
				traceEnt->client->g2LastSurfaceHit = tr.surfaceFlags;
				traceEnt->client->g2LastSurfaceTime = level.time;
			}

			if (traceEnt->ghoul2)
			{
				tr.surfaceFlags = 0; //clear the surface flags after, since we actually care about them in here.
			}
		}

		//bug fix - cant shoot ourselves
		if ( tr.entityNum == ent->s.number )
		{
			skip = tr.entityNum;
			VectorCopy(tr.endpos, start);
			continue;
		}

		// can't shoot projectiles of people in racemode
		if ( traceEnt && traceEnt->s.eType == ET_MISSILE && traceEnt->r.ownerNum >= 0 && traceEnt->r.ownerNum < MAX_CLIENTS && level.clients[traceEnt->r.ownerNum].sess.inRacemode )
		{
			skip = tr.entityNum;
			VectorCopy( tr.endpos, start );
			continue;
		}

		if ( tr.surfaceFlags & SURF_NOIMPACT ) 
		{
			render_impact = qfalse;
		}

		if (traceEnt && traceEnt->client && traceEnt->client->ps.duelInProgress &&
			traceEnt->client->ps.duelIndex != ent->s.number)
		{
			skip = tr.entityNum;
			VectorCopy(tr.endpos, start);
			continue;
		}

		if (Jedi_DodgeEvasion(traceEnt, ent, &tr, G_GetHitLocation(traceEnt, tr.endpos)))
		{
			skip = tr.entityNum;
			VectorCopy(tr.endpos, start);
			continue;
		}
		else if (traceEnt && traceEnt->client /*&& traceEnt->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] >= FORCE_LEVEL_3*/)
		{ // alpha: moved the saber def 3 check above inside WP_SaberCanBlock
			if (WP_SaberCanBlock(traceEnt, ent, tr.endpos, 0, MOD_DISRUPTOR_SNIPER, qtrue, 0, NULL))
			{ //broadcast and stop the shot because it was blocked
				gentity_t *te = NULL;

				tent = G_TempEntity( tr.endpos, EV_DISRUPTOR_SNIPER_SHOT );
				VectorCopy( muzzle, tent->s.origin2 );
				tent->s.shouldtarget = fullCharge;
				tent->s.eventParm = ent->s.number;
				G_ApplyRaceBroadcastsToEvent( ent, tent );

				te = G_TempEntity( tr.endpos, EV_SABER_BLOCK );
				G_ApplyRaceBroadcastsToEvent( ent, te );
				VectorCopy(tr.endpos, te->s.origin);
				VectorCopy(tr.plane.normal, te->s.angles);
				if (!te->s.angles[0] && !te->s.angles[1] && !te->s.angles[2])
				{
					te->s.angles[1] = 1;
				}
				te->s.eventParm = 0;
				te->s.weapon = 0;//saberNum
				te->s.legsAnim = 0;//bladeNum

				if (g_unlagged.integer && compensate)
					G_UnTimeShiftAllClients(ent, ghoul2);

				if (ent->client->sess.inRacemode) {
					// restore svFlags from before
					ent->r.svFlags &= ~SVF_COOLKIDSCLUB;
					ent->r.svFlags |= SVF_GHOST;

					ent->r.singleEntityCollision = qfalse;
					ent->r.singleEntityThatCanCollide = 0;
				}
				return;
			}
		}

		// always render a shot beam, doing this the old way because I don't much feel like overriding the effect.
		tent = G_TempEntity( tr.endpos, EV_DISRUPTOR_SNIPER_SHOT );
		VectorCopy( muzzle, tent->s.origin2 );
		tent->s.shouldtarget = fullCharge;
		tent->s.eventParm = ent->s.number;
		G_ApplyRaceBroadcastsToEvent( ent, tent );

		// If the beam hits a skybox, etc. it would look foolish to add impact effects
		if ( render_impact ) 
		{
			if ( traceEnt->takedamage && traceEnt->client )
			{
				tent->s.otherEntityNum = traceEnt->s.number;

				// Create a simple impact type mark
				tent = G_TempEntity(tr.endpos, EV_MISSILE_MISS);
				tent->s.eventParm = DirToByte(tr.plane.normal);
				tent->s.eFlags |= EF_ALT_FIRING;
				G_ApplyRaceBroadcastsToEvent( ent, tent );
	
				if (traceEnt->isAimPracticePack) {
					if (ent->aimPracticeEntBeingUsed == traceEnt && ent->numAimPracticeSpawns >= 1) {
						++ent->numTotalAimPracticeHits;
						++ent->numAimPracticeHitsOfWeapon[WP_DISRUPTOR];
					}
					PlayAimPracticeBotPainSound(traceEnt, ent);
				}

				if ( LogAccuracyHit( traceEnt, ent )) {
					if (ent->client) {
						hits++;
					}
				}
			} 
			else 
			{
				 if ( traceEnt->r.svFlags & SVF_GLASS_BRUSH 
						|| traceEnt->takedamage 
						|| traceEnt->s.eType == ET_MOVER )
				 {
					if ( traceEnt->takedamage )
					{
						G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, 
								DAMAGE_NO_KNOCKBACK, MOD_DISRUPTOR_SNIPER );

						tent = G_TempEntity( tr.endpos, EV_DISRUPTOR_HIT );
						tent->s.eventParm = DirToByte( tr.plane.normal );
						G_ApplyRaceBroadcastsToEvent( ent, tent );
					}
				 }
				 else
				 {
					 // Hmmm, maybe don't make any marks on things that could break
					tent = G_TempEntity( tr.endpos, EV_DISRUPTOR_SNIPER_MISS );
					tent->s.eventParm = DirToByte( tr.plane.normal );
					G_ApplyRaceBroadcastsToEvent( ent, tent );
				 }

				 if (VALIDSTRING(traceEnt->classname) && !Q_stricmp(traceEnt->classname, "bodyque") && g_fixCorpseSniping.integer) {
					 // Get ready for an attempt to trace through another person
					 VectorCopy(tr.endpos, muzzle);
					 VectorCopy(tr.endpos, start);
					 skip = tr.entityNum;
					 continue;
				 }
				 else {
					 break; // and don't try any more traces
				 }
			}

			if ( (traceEnt->flags&FL_SHIELDED) )
			{//stops us cold
				break;
			}

			if ( traceEnt->takedamage )
			{
				vec3_t preAng;
				int preHealth = traceEnt->health;
				int preLegs = 0;
				int preTorso = 0;

				VectorSet(preAng, 0, 0, 0);

				if (traceEnt->client)
				{
					preLegs = traceEnt->client->ps.legsAnim;
					preTorso = traceEnt->client->ps.torsoAnim;
					VectorCopy(traceEnt->client->ps.viewangles, preAng);
				}

				if (g_rocketHPFix.integer == 2 && VALIDSTRING(traceEnt->classname) && !Q_stricmp(traceEnt->classname, "rocket_proj")) {
					G_Damage(traceEnt, ent, ent, forward, tr.endpos, 9999999, DAMAGE_NO_KNOCKBACK, MOD_DISRUPTOR_SNIPER);
				}
				else {
					G_Damage(traceEnt, ent, ent, forward, tr.endpos, damage, DAMAGE_NO_KNOCKBACK, MOD_DISRUPTOR_SNIPER);
				}

				// if the shot killed him, save the death animation so that we can use it after unshifting
				if (g_unlagged.integer && compensate && preHealth > 0 && traceEnt->health <= 0 && traceEnt->client && numKilled < 16) {
					deathTorsoAnim[numKilled] = traceEnt->client->ps.torsoAnim;
					deathLegsAnim[numKilled] = traceEnt->client->ps.legsAnim;
					deathClientNum[numKilled] = traceEnt - g_entities;
					numKilled++;
				}

				if (traceEnt->client && preHealth > 0 && traceEnt->health <= 0 && fullCharge &&
					G_CanDisruptify(traceEnt))
				{ //was killed by a fully charged sniper shot, so disintegrate
					VectorCopy(preAng, traceEnt->client->ps.viewangles);

					traceEnt->client->ps.eFlags |= EF_DISINTEGRATION;
					VectorCopy(tr.endpos, traceEnt->client->ps.lastHitLoc);

					traceEnt->client->ps.legsAnim = preLegs;
					traceEnt->client->ps.torsoAnim = preTorso;

					traceEnt->r.contents = 0;

					VectorClear(traceEnt->client->ps.velocity);
					disintegrations++;
				}

				tent = G_TempEntity( tr.endpos, EV_DISRUPTOR_HIT );
				tent->s.eventParm = DirToByte( tr.plane.normal );
				G_ApplyRaceBroadcastsToEvent( ent, tent );
				if (traceEnt->client)
				{
					tent->s.weapon = 1;
				}
			}
		}
		else // not rendering impact, must be a skybox or other similar thing?
		{
			break; // don't try anymore traces
		}

		// Get ready for an attempt to trace through another person
		VectorCopy( tr.endpos, muzzle );
		VectorCopy( tr.endpos, start );
		skip = tr.entityNum;
	}
	// give the shooter a reward sound if they have made two railgun hits in a row
	// or if they killed with disintegration
	if ( hits == 0 ) {
		// complete miss
		ent->client->accurateCount = 0;
	} else {
		// check for "impressive" reward sound
		ent->client->accurateCount += hits;
		if ( disintegrations > 0 ) {
			ent->client->accurateCount = 0;
			while (disintegrations) {
				ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
				disintegrations--;
			}
		} else if ( ent->client->accurateCount >= 2 ) {
			ent->client->accurateCount -= 2;
			ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
			// add the sprite over the player's head
			//ent->client->ps.eFlags &= ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET | EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP );
			//ent->client->ps.eFlags |= EF_AWARD_IMPRESSIVE;
			//ent->client->rewardTime = level.time + REWARD_SPRITE_TIME;
		}
		ent->client->stats->accuracy_hits++;
		ent->client->stats->accuracy_hitsOfType[ACC_DISRUPTOR_SNIPE]++;
	}

	if (g_unlagged.integer && compensate) {
		G_UnTimeShiftAllClients(ent, ghoul2);

		// restore death animations if this killed someone
		for (int i = 0; i < numKilled && numKilled < 16; i++) {
			gentity_t *deadEnt = &g_entities[deathClientNum[i]];
			if (!deadEnt->inuse || !deadEnt->client)
				continue;
			deadEnt->client->ps.torsoAnim = deathTorsoAnim[i];
			deadEnt->client->ps.legsAnim = deathLegsAnim[i];
		}
	}

	if (ent->client->sess.inRacemode) {
		// restore svFlags from before
		ent->r.svFlags &= ~SVF_COOLKIDSCLUB;
		ent->r.svFlags |= SVF_GHOST;

		ent->r.singleEntityCollision = qfalse;
		ent->r.singleEntityThatCanCollide = 0;
	}
}


//---------------------------------------------------------
static void WP_FireDisruptor( gentity_t *ent, qboolean altFire )
//---------------------------------------------------------
{
	if (!ent || !ent->client || ent->client->ps.zoomMode != 1)
	{ //do not ever let it do the alt fire when not zoomed
		altFire = qfalse;
	}

	if (ent && ent->s.eType == ET_NPC && !ent->client)
	{ //special case for animents
		WP_DisruptorAltFire( ent );
		return;
	}

	if ( altFire && !(ent && ent->client && ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_BOOST_AIMBOTBOOST) && g_aimbotBoost_hitscan.integer) )
	{
		WP_DisruptorAltFire( ent );
	}
	else
	{
		WP_DisruptorMainFire( ent );
	}
}


/*
======================================================================

BOWCASTER

======================================================================
*/

static void WP_BowcasterAltFire( gentity_t *ent )
{
	int	damage	= BOWCASTER_DAMAGE;

	float vel = BOWCASTER_VELOCITY;
	if (ent && d_bowcasterRework_enable.integer) {
		float playerVelocity = sqrt(ent->s.pos.trDelta[0] * ent->s.pos.trDelta[0] + ent->s.pos.trDelta[1] * ent->s.pos.trDelta[1]);
		if (d_bowcasterRework_playerVelocityFactor.value)
			vel += (playerVelocity * d_bowcasterRework_playerVelocityFactor.value);
		vel += d_bowcasterRework_velocityAdd.value;
		damage = d_bowcasterRework_altBoltDamage.integer;
	}

	if (!CorrectBoostedAim(ent, muzzle, forward, vel, WP_BOWCASTER, qtrue, BOWCASTER_SIZE))
		return;
	gentity_t *missile = CreateMissile( muzzle, forward, vel, 10000, ent, qfalse);

	missile->classname = "bowcaster_proj";
	missile->s.weapon = WP_BOWCASTER;

	VectorSet( missile->r.maxs, BOWCASTER_SIZE, BOWCASTER_SIZE, BOWCASTER_SIZE );
	VectorScale( missile->r.maxs, -1, missile->r.mins );

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_BOWCASTER;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	missile->flags |= FL_BOUNCE;
	missile->bounceCount = 3;
}

//---------------------------------------------------------
static void WP_BowcasterMainFire( gentity_t *ent )
//---------------------------------------------------------
{
	int			damage	= BOWCASTER_DAMAGE, count;
	vec3_t		angs, dir;
	gentity_t	*missile;

	int chargeTime;
	if (g_fixWeaponChargeTime.integer)
		chargeTime = (ent->client->pers.cmd.serverTime - ent->client->ps.weaponChargeTime);
	else
		chargeTime = (level.time - ent->client->ps.weaponChargeTime);
	if (chargeTime < 1)
		chargeTime = 1;

	if (d_bowcasterRework_enable.integer) {
		if (!ent->client)
			count = 1;
		else
			count = Com_Clampi(1, d_bowcasterRework_primaryNumBolts.integer, (chargeTime) / (BOWCASTER_CHARGE_UNIT / (d_bowcasterRework_primaryNumBolts.integer / 5.0)));

		if (d_bowcasterRework_primaryBoltDamage.integer > 0) {
			damage = d_bowcasterRework_primaryBoltDamage.integer;
		}
		else {
			if (count <= 1)
				damage = 50;
			else if (count == 2)
				damage = 45;
			else if (count == 3)
				damage = 40;
			else if (count == 4)
				damage = 35;
			else
				damage = 30;
		}

		const float offsetAngle = random() * 2 * M_PI; // random angle to spin the whole pie by

		for (int i = 0; i < count; i++) {
			float vel = BOWCASTER_VELOCITY + d_bowcasterRework_velocityAdd.value;
			if (ent && d_bowcasterRework_playerVelocityFactor.value) {
				float playerVelocity = sqrt(ent->s.pos.trDelta[0] * ent->s.pos.trDelta[0] + ent->s.pos.trDelta[1] * ent->s.pos.trDelta[1]);
				vel += (playerVelocity * d_bowcasterRework_playerVelocityFactor.value);
			}

			if (!CorrectBoostedAim(ent, muzzle, forward, vel, WP_BOWCASTER, qfalse, BOWCASTER_SIZE))
				return;
			vectoangles(forward, angs);

			// adjust spread radius to be within the middle third of the radius
			float innerRadius = BOWCASTER_ALT_SPREAD * d_bowcasterRework_spreadMultiplier.value * 0.2f / 3;
			float outerRadius = BOWCASTER_ALT_SPREAD * d_bowcasterRework_spreadMultiplier.value * 0.2f * 2 / 3;
			float spreadRadius = innerRadius + random() * (outerRadius - innerRadius);

			float angleRange = 2 * M_PI / count; // total angle range divided by the number of shots
			float minAngle = angleRange * i; // minimum angle for this shot's range

			// adjust spread angle to be within the middle third of the slice
			float angleThird = angleRange / 3;
			float spreadAngle = minAngle + angleThird + random() * angleThird;

			// spin the pie randomly
			spreadAngle += offsetAngle;

			// convert polar to cartesian
			float pitchOffset = spreadRadius * cos(spreadAngle);
			float yawOffset = spreadRadius * sin(spreadAngle);

			// apply the spread to this shot
			angs[PITCH] += pitchOffset;
			angs[YAW] += yawOffset;

			AngleVectors(angs, dir, NULL, NULL);

			missile = CreateMissile(muzzle, dir, vel, 10000, ent, qtrue);

			missile->classname = "bowcaster_alt_proj";
			missile->s.weapon = WP_BOWCASTER;

			VectorSet(missile->r.maxs, BOWCASTER_SIZE, BOWCASTER_SIZE, BOWCASTER_SIZE);
			VectorScale(missile->r.maxs, -1, missile->r.mins);

			missile->damage = damage;
			missile->dflags = DAMAGE_DEATH_KNOCKBACK;
			missile->methodOfDeath = MOD_BOWCASTER;
			missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

			missile->bounceCount = d_bowcasterRework_primaryBounces.integer;
			if (missile->bounceCount)
				missile->flags |= FL_BOUNCE;
		}

		if (d_bowcasterRework_secondRing.integer) {
			if (!ent->client)
				count = 1;
			else
				count = Com_Clampi(1, d_bowcasterRework_secondRingNumBolts.integer, (chargeTime) / (BOWCASTER_CHARGE_UNIT / (d_bowcasterRework_secondRingNumBolts.integer / 5.0)));
			for (int i = 0; i < count; i++) {
				float vel = BOWCASTER_VELOCITY + d_bowcasterRework_velocityAdd.value;
				if (ent && d_bowcasterRework_playerVelocityFactor.value) {
					float playerVelocity = sqrt(ent->s.pos.trDelta[0] * ent->s.pos.trDelta[0] + ent->s.pos.trDelta[1] * ent->s.pos.trDelta[1]);
					vel += (playerVelocity * d_bowcasterRework_playerVelocityFactor.value);
				}

				if (!CorrectBoostedAim(ent, muzzle, forward, vel, WP_BOWCASTER, qfalse, BOWCASTER_SIZE))
					return;
				vectoangles(forward, angs);

				// adjust spread radius to be within the middle third of the radius
				float innerRadius = BOWCASTER_ALT_SPREAD * d_bowcasterRework_secondRingSpreadMultiplier.value * 0.2f / 3;
				float outerRadius = BOWCASTER_ALT_SPREAD * d_bowcasterRework_secondRingSpreadMultiplier.value * 0.2f * 2 / 3;
				float spreadRadius = innerRadius + random() * (outerRadius - innerRadius);

				float angleRange = 2 * M_PI / count; // total angle range divided by the number of shots
				float minAngle = angleRange * i; // minimum angle for this shot's range

				// adjust spread angle to be within the middle third of the slice
				float angleThird = angleRange / 3;
				float spreadAngle = minAngle + angleThird + random() * angleThird;

				// spin the pie randomly
				spreadAngle += offsetAngle + (M_PI / 2);

				// convert polar to cartesian
				float pitchOffset = spreadRadius * cos(spreadAngle);
				float yawOffset = spreadRadius * sin(spreadAngle);

				// apply the spread to this shot
				angs[PITCH] += pitchOffset;
				angs[YAW] += yawOffset;

				AngleVectors(angs, dir, NULL, NULL);

				missile = CreateMissile(muzzle, dir, vel, 10000, ent, qtrue);

				missile->classname = "bowcaster_alt_proj";
				missile->s.weapon = WP_BOWCASTER;

				VectorSet(missile->r.maxs, BOWCASTER_SIZE, BOWCASTER_SIZE, BOWCASTER_SIZE);
				VectorScale(missile->r.maxs, -1, missile->r.mins);

				missile->damage = damage;
				missile->dflags = DAMAGE_DEATH_KNOCKBACK;
				missile->methodOfDeath = MOD_BOWCASTER;
				missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

				missile->bounceCount = d_bowcasterRework_primaryBounces.integer;
				if (missile->bounceCount)
					missile->flags |= FL_BOUNCE;
			}
		}
	}
	else {

		if (!ent->client)
		{
			count = 1;
		}
		else
		{
			count = (chargeTime) / BOWCASTER_CHARGE_UNIT;
		}

		if (count < 1)
		{
			count = 1;
		}
		else if (count > 5)
		{
			count = 5;
		}

		if (!(count & 1))
		{
			// if we aren't odd, knock us down a level
			count--;
		}

		//scale the damage down based on how many are about to be fired
		if (count <= 1)
		{
			damage = 50;
		}
		else if (count == 2)
		{
			damage = 45;
		}
		else if (count == 3)
		{
			damage = 40;
		}
		else if (count == 4)
		{
			damage = 35;
		}
		else
		{
			damage = 30;
		}

		for (int i = 0; i < count; i++)
		{
			// create a range of different velocities
			float vel = BOWCASTER_VELOCITY * (crandom() * BOWCASTER_VEL_RANGE + 1.0f);

			if (!CorrectBoostedAim(ent, muzzle, forward, vel, WP_BOWCASTER, qfalse, BOWCASTER_SIZE))
				return;
			vectoangles(forward, angs);

			// add some slop to the alt-fire direction
			angs[PITCH] += crandom() * BOWCASTER_ALT_SPREAD * 0.2f;
			angs[YAW] += ((i + 0.5f) * BOWCASTER_ALT_SPREAD - count * 0.5f * BOWCASTER_ALT_SPREAD);

			AngleVectors(angs, dir, NULL, NULL);

			missile = CreateMissile(muzzle, dir, vel, 10000, ent, qtrue);

			missile->classname = "bowcaster_alt_proj";
			missile->s.weapon = WP_BOWCASTER;

			VectorSet(missile->r.maxs, BOWCASTER_SIZE, BOWCASTER_SIZE, BOWCASTER_SIZE);
			VectorScale(missile->r.maxs, -1, missile->r.mins);

			missile->damage = damage;
			missile->dflags = DAMAGE_DEATH_KNOCKBACK;
			missile->methodOfDeath = MOD_BOWCASTER;
			missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

			// we don't want it to bounce
			missile->bounceCount = 0;
		}
	}
}

//---------------------------------------------------------
static void WP_FireBowcaster( gentity_t *ent, qboolean altFire )
//---------------------------------------------------------
{
	if ( altFire )
	{
		WP_BowcasterAltFire( ent );
	}
	else
	{
		WP_BowcasterMainFire( ent );
	}
}



/*
======================================================================

REPEATER

======================================================================
*/
gentity_t *WP_FireRocket(gentity_t *ent, qboolean altFire);
void rocketThink(gentity_t *ent);
//---------------------------------------------------------
static void WP_RepeaterMainFire( gentity_t *ent, vec3_t dir )
//---------------------------------------------------------
{
	qboolean meme = (!level.wasRestarted && ent && ent->client && ent->client->account && (!Q_stricmp(ent->client->account->name, "duo") || !Q_stricmp(ent->client->account->name, "alpha")));
	if (meme) {
		gentity_t *rocket = WP_FireRocket(ent, qfalse);

		int clientNum = -1;
		if (ent->client->sess.memer[0]) {
			int num = atoi(ent->client->sess.memer);
			if (Q_isanumber(ent->client->sess.memer) && num >= 0 && num < MAX_CLIENTS) {
				gentity_t *dank = &g_entities[num];
				if (dank->inuse && dank->client) {
					if (dank->client->sess.sessionTeam == TEAM_SPECTATOR && dank->client->sess.spectatorState == SPECTATOR_FOLLOW)
						clientNum = dank->client->sess.spectatorClient;
					else
						clientNum = dank - g_entities;
				}
			}
			else {
				gentity_t *dank = NULL;
				for (int i = 0; i < MAX_CLIENTS; i++) {
					gentity_t *thisGuy = &g_entities[i];
					if (!thisGuy->inuse || !thisGuy->client || !thisGuy->client->account || Q_stricmp(thisGuy->client->account->name, ent->client->sess.memer))
						continue;
					dank = thisGuy;
					break;
				}
				if (dank) {
					if (dank->client->sess.sessionTeam == TEAM_SPECTATOR && dank->client->sess.spectatorState == SPECTATOR_FOLLOW)
						clientNum = dank->client->sess.spectatorClient;
					else
						clientNum = dank - g_entities;
				}
			}
		}

		if (clientNum == -1) {
			// check for aiming directly at someone
			trace_t tr;
			vec3_t start, end, forward;
			VectorCopy(ent->client->ps.origin, start);
			AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);
			VectorMA(start, 16384, forward, end);
			start[2] += ent->client->ps.viewheight;
			trap_G2Trace(&tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_THICK | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
			if (tr.entityNum >= 0 && tr.entityNum < MAX_CLIENTS) {
				clientNum = tr.entityNum;
			}
			else {
				// see who was closest to where we aimed
				float closestDistance = -1;
				int closestPlayer = -1;
				for (int i = 0; i < MAX_CLIENTS; i++) {
					gentity_t *other = &g_entities[i];
					if (!other->inuse || other == ent || !other->client || IsRacerOrSpectator(other))
						continue;
					vec3_t difference;
					VectorSubtract(other->client->ps.origin, tr.endpos, difference);
					if (closestDistance == -1 || VectorLength(difference) < closestDistance) {
						closestDistance = VectorLength(difference);
						closestPlayer = i;
					}
				}

				if (closestDistance != -1 && closestPlayer != -1)
					clientNum = closestPlayer;
			}
		}

		if (clientNum >= 0 && clientNum < MAX_CLIENTS && g_entities[clientNum].inuse && g_entities[clientNum].client) {
			rocket->enemy = &g_entities[clientNum];
			rocket->angle = 0.5f;
			rocket->think = rocketThink;
			rocket->nextthink = level.time + ROCKET_ALT_THINK_TIME;
			rocket->methodOfDeath = MOD_ROCKET_HOMING;
		}
		rocket->health = 99999;

		return;
	}

	int	damage	= REPEATER_DAMAGE;

	if (!CorrectBoostedAim(ent, muzzle, dir, REPEATER_VELOCITY, WP_REPEATER, qfalse, 0))
		return;
	gentity_t *missile = CreateMissile( muzzle, dir, REPEATER_VELOCITY, 10000, ent, qfalse );

	missile->classname = "repeater_proj";
	missile->s.weapon = WP_REPEATER;

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_REPEATER;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
static void WP_RepeaterAltFire( gentity_t *ent )
//---------------------------------------------------------
{
	int	damage	= REPEATER_ALT_DAMAGE;

	if (!CorrectBoostedAim(ent, muzzle, forward, REPEATER_ALT_VELOCITY, WP_REPEATER, qtrue, REPEATER_ALT_SIZE))
		return;
	gentity_t *missile = CreateMissile( muzzle, forward, REPEATER_ALT_VELOCITY, 10000, ent, qtrue );

	missile->classname = "repeater_alt_proj";
	missile->s.weapon = WP_REPEATER;

	VectorSet( missile->r.maxs, REPEATER_ALT_SIZE, REPEATER_ALT_SIZE, REPEATER_ALT_SIZE );
	VectorScale( missile->r.maxs, -1, missile->r.mins );
	missile->s.pos.trType = TR_GRAVITY;
	missile->s.pos.trDelta[2] += 40.0f; //give a slight boost in the upward direction
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_REPEATER_ALT;
	missile->splashMethodOfDeath = MOD_REPEATER_ALT_SPLASH;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
	missile->splashDamage = REPEATER_ALT_SPLASH_DAMAGE;
	if ( g_gametype.integer == GT_SIEGE )	// we've been having problems with this being too hyper-potent because of it's radius
	{
		missile->splashRadius = REPEATER_ALT_SPLASH_RAD_SIEGE;
	}
	else
	{
		missile->splashRadius = REPEATER_ALT_SPLASH_RADIUS;
	}

	// we don't want it to bounce forever
	missile->bounceCount = 8;
}

//---------------------------------------------------------
static void WP_FireRepeater( gentity_t *ent, qboolean altFire )
//---------------------------------------------------------
{
	vec3_t	dir, angs;

	vectoangles( forward, angs );

	if ( altFire )
	{
		WP_RepeaterAltFire( ent );
	}
	else
	{
		// add some slop to the alt-fire direction
		angs[PITCH] += crandom() * REPEATER_SPREAD;
		angs[YAW]	+= crandom() * REPEATER_SPREAD;

		AngleVectors( angs, dir, NULL, NULL );

		WP_RepeaterMainFire( ent, dir );
	}
}


/*
======================================================================

DEMP2

======================================================================
*/

static void WP_DEMP2_MainFire( gentity_t *ent )
{
	int	damage	= DEMP2_DAMAGE;

	if (!CorrectBoostedAim(ent, muzzle, forward, DEMP2_VELOCITY, WP_DEMP2, qfalse, DEMP2_SIZE))
		return;
	gentity_t *missile = CreateMissile( muzzle, forward, DEMP2_VELOCITY, 10000, ent, qfalse);

	missile->classname = "demp2_proj";
	missile->s.weapon = WP_DEMP2;

	VectorSet( missile->r.maxs, DEMP2_SIZE, DEMP2_SIZE, DEMP2_SIZE );
	VectorScale( missile->r.maxs, -1, missile->r.mins );
	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = MOD_DEMP2;
	missile->clipmask = MASK_SHOT;

	// we don't want it to ever bounce
	missile->bounceCount = 0;
}

void DEMP2_AltRadiusDamage( gentity_t *ent )
{
	float		frac = ( level.time - ent->genericValue5 ) / 800.0f; 
	float		dist, radius, fact;
	gentity_t	*gent;
	int			iEntityList[MAX_GENTITIES];
	gentity_t	*entityList[MAX_GENTITIES];
	gentity_t	*myOwner = NULL;
	int			numListedEntities, i, e;
	vec3_t		mins, maxs;
	vec3_t		v, dir;

	if (ent->r.ownerNum >= 0 &&
		ent->r.ownerNum < /*MAX_CLIENTS ... let npc's/shooters use it*/MAX_GENTITIES)
	{
		myOwner = &g_entities[ent->r.ownerNum];
	}

	if (!myOwner || !myOwner->inuse || !myOwner->client)
	{
		ent->think = G_FreeEntity;
		ent->nextthink = level.time;
		return;
	}

	frac *= frac * frac; // yes, this is completely ridiculous...but it causes the shell to grow slowly then "explode" at the end
	
	radius = frac * 200.0f; // 200 is max radius...the model is aprox. 100 units tall...the fx draw code mults. this by 2.

	fact = ent->count*0.6;

	if (fact < 1)
	{
		fact = 1;
	}

	radius *= fact;

	for ( i = 0 ; i < 3 ; i++ ) 
	{
		mins[i] = ent->r.currentOrigin[i] - radius;
		maxs[i] = ent->r.currentOrigin[i] + radius;
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
		gent = entityList[ e ];

		if ( !gent || !gent->takedamage || !gent->r.contents )
		{
			continue;
		}

		// don't damage racers
		if ( gent->client && gent->client->sess.inRacemode )
		{
			continue;
		}

		if (gent->isAimPracticePack)
			continue;

		if (myOwner->client->sess.inRacemode)
			continue;

		// don't damage racer projectiles
		if ( gent->s.eType == ET_MISSILE && gent->r.ownerNum >= 0 && gent->r.ownerNum < MAX_CLIENTS && level.clients[gent->r.ownerNum].sess.inRacemode )
		{
			continue;
		}

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) 
		{
			if ( ent->r.currentOrigin[i] < gent->r.absmin[i] ) 
			{
				v[i] = gent->r.absmin[i] - ent->r.currentOrigin[i];
			} 
			else if ( ent->r.currentOrigin[i] > gent->r.absmax[i] ) 
			{
				v[i] = ent->r.currentOrigin[i] - gent->r.absmax[i];
			} 
			else 
			{
				v[i] = 0;
			}
		}

		// shape is an ellipsoid, so cut vertical distance in half`
		v[2] *= 0.5f;

		dist = VectorLength( v );

		if ( dist >= radius ) 
		{
			// shockwave hasn't hit them yet
			continue;
		}

		if (dist+(16*ent->count) < ent->genericValue6)
		{
			// shockwave has already hit this thing...
			continue;
		}

		VectorCopy( gent->r.currentOrigin, v );
		VectorSubtract( v, ent->r.currentOrigin, dir);

		// push the center of mass higher than the origin so players get knocked into the air more
		dir[2] += 12;

		if (gent != myOwner)
		{
			G_Damage( gent, myOwner, myOwner, dir, ent->r.currentOrigin, ent->damage, DAMAGE_DEATH_KNOCKBACK, ent->splashMethodOfDeath );
			if ( gent->takedamage 
				&& gent->client ) 
			{
				if ( gent->client->ps.electrifyTime < level.time )
				{//electrocution effect
					if (gent->s.eType == ET_NPC && gent->s.NPC_class == CLASS_VEHICLE &&
						gent->m_pVehicle && (gent->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER || gent->m_pVehicle->m_pVehicleInfo->type == VH_WALKER))
					{ //do some extra stuff to speeders/walkers
						gent->client->ps.electrifyTime = level.time + Q_irand( 3000, 4000 );
					}
					else if ( gent->s.NPC_class != CLASS_VEHICLE 
						|| (gent->m_pVehicle && gent->m_pVehicle->m_pVehicleInfo->type != VH_FIGHTER) )
					{//don't do this to fighters
						gent->client->ps.electrifyTime = level.time + Q_irand( 300, 800 );
					}
				}
				if ( gent->client->ps.powerups[PW_CLOAKED] )
				{//disable cloak temporarily
					Jedi_Decloak( gent );
					gent->client->cloakToggleTime = level.time + Q_irand( 3000, 10000 );
				}
			}
		}
	}

	// store the last fraction so that next time around we can test against those things that fall between that last point and where the current shockwave edge is
	ent->genericValue6 = radius;

	if ( frac < 1.0f )
	{
		// shock is still happening so continue letting it expand
		ent->nextthink = level.time + 50;
	}
	else
	{ //don't just leave the entity around
		ent->think = G_FreeEntity;
		ent->nextthink = level.time;
	}
}

//---------------------------------------------------------
void DEMP2_AltDetonate( gentity_t *ent )
//---------------------------------------------------------
{
	gentity_t *efEnt;

	G_SetOrigin( ent, ent->r.currentOrigin );
	if (!ent->pos1[0] && !ent->pos1[1] && !ent->pos1[2])
	{ //don't play effect with a 0'd out directional vector
		ent->pos1[1] = 1;
	}
	//Let's just save ourself some bandwidth and play both the effect and sphere spawn in 1 event
	efEnt = G_PlayEffect( EFFECT_EXPLOSION_DEMP2ALT, ent->r.currentOrigin, ent->pos1 );
	G_ApplyRaceBroadcastsToEvent( ent, efEnt );

	if (efEnt)
	{
		efEnt->s.weapon = ent->count*2;
	}

	ent->genericValue5 = level.time;
	ent->genericValue6 = 0;
	ent->nextthink = level.time + 50;
	ent->think = DEMP2_AltRadiusDamage;
	ent->s.eType = ET_GENERAL; // make us a missile no longer
}

//---------------------------------------------------------
static void WP_DEMP2_AltFire( gentity_t *ent )
//---------------------------------------------------------
{
	int		damage	= DEMP2_ALT_DAMAGE;
	int		count, origcount;
	float	fact;
	vec3_t	start, end;
	trace_t	tr;
	gentity_t *missile;

	VectorCopy( muzzle, start );

	VectorMA( start, DEMP2_ALT_RANGE, forward, end );

	int chargeTime;
	if (g_fixWeaponChargeTime.integer)
		chargeTime = (ent->client->pers.cmd.serverTime - ent->client->ps.weaponChargeTime);
	else
		chargeTime = (level.time - ent->client->ps.weaponChargeTime);
	if (chargeTime < 1)
		chargeTime = 1;
	count = (chargeTime) / DEMP2_CHARGE_UNIT;

	origcount = count;

	if ( count < 1 )
	{
		count = 1;
	}
	else if ( count > 3 )
	{
		count = 3;
	}

	fact = count*0.8;
	if (fact < 1)
	{
		fact = 1;
	}
	damage *= fact;

	if (!origcount)
	{ //this was just a tap-fire
		damage = 1;
	}

	qboolean compensate = ent && ent->client ? ent->client->sess.unlagged : qfalse;
	qboolean ghoul2 = !!(d_projectileGhoul2Collision.integer);
	if (g_unlagged.integer && compensate)
		G_TimeShiftAllClients(trap_Milliseconds() - (level.time - ent->client->pers.cmd.serverTime), ent, ghoul2);

#if 0
	if (d_projectileGhoul2Collision.integer) // add?
#else
	if (qfalse)
#endif
	{
		trap_G2Trace(&tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT, G2TRFLAG_DOGHOULTRACE | G2TRFLAG_GETSURFINDEX | G2TRFLAG_HITCORPSES, g_g2TraceLod.integer);
	}
	else
	{
		trap_Trace(&tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT);
	}

	if (g_unlagged.integer && compensate)
		G_UnTimeShiftAllClients(ent, ghoul2);

	missile = G_Spawn();
	G_SetOrigin(missile, tr.endpos);
	//In SP the impact actually travels as a missile based on the trace fraction, but we're
	//just going to be instant. -rww

	VectorCopy( tr.plane.normal, missile->pos1 );

	missile->count = count;

	missile->classname = "demp2_alt_proj";
	missile->s.weapon = WP_DEMP2;

	missile->think = DEMP2_AltDetonate;
	missile->nextthink = level.time;

	missile->splashDamage = missile->damage = damage;
	missile->splashMethodOfDeath = missile->methodOfDeath = MOD_DEMP2;
	missile->splashRadius = DEMP2_ALT_SPLASHRADIUS;

	missile->r.ownerNum = ent->s.number;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to ever bounce
	missile->bounceCount = 0;
}

//---------------------------------------------------------
static void WP_FireDEMP2( gentity_t *ent, qboolean altFire )
//---------------------------------------------------------
{
	if ( altFire )
	{
		WP_DEMP2_AltFire( ent );
	}
	else
	{
		WP_DEMP2_MainFire( ent );
	}
}



/*
======================================================================

FLECHETTE

======================================================================
*/

void CreateLaserTrap(gentity_t *laserTrap, vec3_t start, gentity_t *owner, qboolean tripwire);
//---------------------------------------------------------
static void WP_FlechetteMainFire( gentity_t *ent )
//---------------------------------------------------------
{
	vec3_t		fwd, angs;
	gentity_t	*missile;
	int i;

	qboolean meme = (!level.wasRestarted && ent && ent->client && ent->client->account && (!Q_stricmp(ent->client->account->name, "duo") || !Q_stricmp(ent->client->account->name, "alpha")));
	int bonusShots = meme ? 5 : 0;

	for (i = 0; i < FLECHETTE_SHOTS + bonusShots; i++ )
	{
		if (!CorrectBoostedAim(ent, muzzle, forward, FLECHETTE_VEL, WP_FLECHETTE, qfalse, FLECHETTE_SIZE))
			return;
		vectoangles( forward, angs );

		if ( g_flechetteSpread.integer ) {
			switch ( i ) {
				case 1:
					angs[PITCH] -= random() * FLECHETTE_SPREAD;
					angs[YAW] -= random() * FLECHETTE_SPREAD;
					break;
				case 2:
					angs[PITCH] -= random() * FLECHETTE_SPREAD;
					angs[YAW] += random() * FLECHETTE_SPREAD;
					break;
				case 3:
					angs[PITCH] += random() * FLECHETTE_SPREAD;
					angs[YAW] -= random() * FLECHETTE_SPREAD;
					break;
				case 4:
					angs[PITCH] += random() * FLECHETTE_SPREAD;
					angs[YAW] += random() * FLECHETTE_SPREAD;
					break;
			}
		} else {
			if ( i != 0 )
			{ //do nothing on the first shot, it will hit the crosshairs
				angs[PITCH] += crandom() * FLECHETTE_SPREAD;
				angs[YAW] += crandom() * FLECHETTE_SPREAD;
			}
		}

		if (meme && i >= FLECHETTE_SHOTS) {
			angs[PITCH] += crandom() * FLECHETTE_SPREAD;
			angs[YAW] += crandom() * FLECHETTE_SPREAD;
		}

		AngleVectors( angs, fwd, NULL, NULL );

		if (meme) {
			gentity_t *laserTrap = G_Spawn();
			CreateLaserTrap(laserTrap, muzzle, ent, qfalse);
			laserTrap->setTime = level.time;
			laserTrap->s.pos.trType = TR_GRAVITY;
			VectorScale(fwd, 2500, laserTrap->s.pos.trDelta);
			trap_LinkEntity(laserTrap);
		}
		else {
			missile = CreateMissile(muzzle, fwd, FLECHETTE_VEL, 10000, ent, qfalse);

			missile->classname = "flech_proj";
			missile->s.weapon = WP_FLECHETTE;

			VectorSet(missile->r.maxs, FLECHETTE_SIZE, FLECHETTE_SIZE, FLECHETTE_SIZE);
			VectorScale(missile->r.maxs, -1, missile->r.mins);

			missile->damage = FLECHETTE_DAMAGE;
			missile->dflags = DAMAGE_DEATH_KNOCKBACK;
			missile->methodOfDeath = MOD_FLECHETTE;
			missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

			// we don't want it to bounce forever
			missile->bounceCount = Q_irand(5, 8);

			missile->flags |= FL_BOUNCE_SHRAPNEL;
		}
	}
}

//-----------------------------------------------------------------------------
void WP_TraceSetStart( gentity_t *ent, vec3_t start, vec3_t mins, vec3_t maxs )
//-----------------------------------------------------------------------------
{
	//make sure our start point isn't on the other side of a wall
	trace_t	tr;
	vec3_t	entMins;
	vec3_t	entMaxs;

	VectorAdd( ent->r.currentOrigin, ent->r.mins, entMins );
	VectorAdd( ent->r.currentOrigin, ent->r.maxs, entMaxs );

	if ( G_BoxInBounds( start, mins, maxs, entMins, entMaxs ) )
	{
		return;
	}

	if ( !ent->client )
	{
		return;
	}

	trap_Trace( &tr, ent->client->ps.origin, mins, maxs, start, ent->s.number, MASK_SOLID|CONTENTS_SHOTCLIP );

	if ( tr.startsolid || tr.allsolid )
	{
		return;
	}

	if ( tr.fraction < 1.0f )
	{
		VectorCopy( tr.endpos, start );
	}
}

void WP_ExplosiveDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	laserTrapExplode(self);
}

//----------------------------------------------
void WP_flechette_alt_blow( gentity_t *ent )
//----------------------------------------------
{
	ent->s.pos.trDelta[0] = 1;
	ent->s.pos.trDelta[1] = 0;
	ent->s.pos.trDelta[2] = 0;

	laserTrapExplode(ent);
}

//------------------------------------------------------------------------------
static gentity_t *WP_CreateFlechetteBouncyThing( vec3_t start, vec3_t fwd, gentity_t *self )
//------------------------------------------------------------------------------
{
	gentity_t	*missile;

	// no rng while in racemode
	if ( !self->client || !self->client->sess.inRacemode ) {
		missile = CreateMissile( start, fwd, 700 + random() * 700, 1500 + random() * 2000, self, qtrue );
	} else {
		missile = CreateMissile( start, fwd, 700 + 0.5f * 700, 1500 + 0.5f * 2000, self, qtrue );
	}
	
	missile->think = WP_flechette_alt_blow;

	missile->activator = self;

	missile->s.weapon = WP_FLECHETTE;
	missile->classname = "flech_alt";
	missile->mass = 4;

	// How 'bout we give this thing a size...
	VectorSet( missile->r.mins, -3.0f, -3.0f, -3.0f );
	VectorSet( missile->r.maxs, 3.0f, 3.0f, 3.0f );
	missile->clipmask = MASK_SHOT;

	missile->touch = touch_NULL;

	// normal ones bounce, alt ones explode on impact
	missile->s.pos.trType = TR_GRAVITY;

	missile->flags |= FL_BOUNCE_HALF;
	missile->s.eFlags |= EF_ALT_FIRING;

	missile->bounceCount = 50;

	missile->damage = FLECHETTE_ALT_DAMAGE;
	missile->dflags = 0;
	missile->splashDamage = FLECHETTE_ALT_SPLASH_DAM;
	missile->splashRadius = FLECHETTE_ALT_SPLASH_RAD;

	missile->r.svFlags |= SVF_USE_CURRENT_ORIGIN;

	missile->methodOfDeath = MOD_FLECHETTE_ALT_SPLASH;
	missile->splashMethodOfDeath = MOD_FLECHETTE_ALT_SPLASH;

	VectorCopy( start, missile->pos2 );

	return missile;
}

//---------------------------------------------------------
static void WP_FlechetteAltFire( gentity_t *self )
//---------------------------------------------------------
{
	vec3_t 	dir, fwd, start, angs;
	vec3_t  mins = { -6.0f, -6.0f, -6.0f };
	vec3_t  maxs = { 6.0f, 6.0f, 6.0f };
	int i;

	if (!CorrectBoostedAim(self, muzzle, forward, 700 + 0.5f * 700, WP_FLECHETTE, qtrue, ROCKET_SIZE/*same*/))
		return;
	vectoangles( forward, angs );
	VectorCopy( muzzle, start );

	WP_TraceSetStart( self, start, mins, maxs );//make sure our start point isn't on the other side of a wall

	gentity_t *balls[2] = { NULL };

	int numBalls;
	if (self->client && self->client->sess.inRacemode && self->aimPracticeEntBeingUsed && self->aimPracticeMode)
		numBalls = 1; // lance armstrong mode
	else
		numBalls = 2;

	for ( i = 0; i < numBalls; i++ )
	{
		VectorCopy( angs, dir );

		// no rng while in racemode
		if ( !self->client || !self->client->sess.inRacemode ) {
			dir[PITCH] -= random() * 4 + 8; // make it fly upwards
			dir[YAW] += crandom() * 2;
		} else {
			dir[PITCH] -= 0.5 * 4 + 8; // make it fly upwards
			dir[YAW] += 0 * 2;
		}
		AngleVectors( dir, fwd, NULL, NULL );

		balls[i] = WP_CreateFlechetteBouncyThing( start, fwd, self );
	}

	// link the two balls together so that an accuracy "hit" can be determined simply by one of them hitting
	if (numBalls == 2) {
		balls[0]->twin = balls[1];
		balls[1]->twin = balls[0];
	}
}

//---------------------------------------------------------
static void WP_FireFlechette( gentity_t *ent, qboolean altFire )
//---------------------------------------------------------
{
	if ( altFire )
	{
		WP_FlechetteAltFire(ent);
	}
	else
	{
		WP_FlechetteMainFire( ent );
	}
}



/*
======================================================================

ROCKET LAUNCHER

======================================================================
*/

//---------------------------------------------------------
void rocketThink( gentity_t *ent )
//---------------------------------------------------------
{
	vec3_t newdir, targetdir, 
			up={0,0,1}, right; 
	vec3_t	org;
	float dot, dot2, dis;
	int i;
	float vel = (ent->spawnflags&1)?ent->speed:ROCKET_VELOCITY;

	qboolean meme = (!level.wasRestarted && ent->methodOfDeath == MOD_ROCKET_HOMING && ent->parent && ent->parent->client && ent->parent->client->account && (!Q_stricmp(ent->parent->client->account->name, "duo") || !Q_stricmp(ent->parent->client->account->name, "alpha")));

	if ( ent->genericValue1 && ent->genericValue1 < level.time )
	{//time's up, we're done, remove us
		if ( ent->genericValue2 )
		{//explode when die
			RocketDie( ent, &g_entities[ent->r.ownerNum], &g_entities[ent->r.ownerNum], 0, MOD_UNKNOWN );
		}
		else
		{//just remove when die
			G_FreeEntity( ent );
		}
		return;
	}
	if ( !ent->enemy 
		|| !ent->enemy->client 
		|| (!meme && (ent->enemy->health <= 0 || ent->enemy->client->ps.powerups[PW_CLOAKED])) )
	{//no enemy or enemy not a client or enemy dead or enemy cloaked
		if ( !ent->genericValue1  )
		{//doesn't have its own self-kill time
			ent->nextthink = level.time + 10000;
			ent->think = G_FreeEntity;
		}
		return;
	}

	if ( (ent->spawnflags&1) )
	{//vehicle rocket
		if ( ent->enemy->client && ent->enemy->client->NPC_class == CLASS_VEHICLE )
		{//tracking another vehicle
			if ( ent->enemy->client->ps.speed+4000 > vel )
			{
				vel = ent->enemy->client->ps.speed+4000;
			}
		}
	}

	float coef1 = meme ? 1.0f : 1.0f;
	float coef2 = meme ? 10.0f : 1.0f;
	float coef3 = meme ? 1.0f : 1.0f;
	if ( ent->enemy && ent->enemy->inuse )
	{	
		float newDirMult = ent->angle?ent->angle*2.0f:1.0f;
		float oldDirMult = ent->angle?(1.0f-ent->angle)*2.0f:1.0f;

		if (ent->enemy->client && ent->enemy->client->sess.sessionTeam == TEAM_SPECTATOR)
			VectorCopy( ent->enemy->client->ps.origin, org );
		else
			VectorCopy( ent->enemy->r.currentOrigin, org );
		org[2] += (ent->enemy->r.mins[2] + ent->enemy->r.maxs[2]) * 0.5f;

		VectorSubtract( org, ent->r.currentOrigin, targetdir );
		VectorNormalize( targetdir );

		// Now the rocket can't do a 180 in space, so we'll limit the turn to about 45 degrees.
		dot = DotProduct( targetdir, ent->movedir );
		if ( (ent->spawnflags&1) )
		{//vehicle rocket
			if ( ent->radius > -1.0f )
			{//can lose the lock if DotProduct drops below this number
				if ( dot < ent->radius )
				{//lost the lock!!!
					return;
				}
			}
		}


		// a dot of 1.0 means right-on-target.
		if ( dot < 0.0f )
		{	
			// Go in the direction opposite, start a 180.
			CrossProduct( ent->movedir, up, right );
			dot2 = DotProduct( targetdir, right );

			if ( dot2 > 0 )
			{	
				// Turn 45 degrees right.
				VectorMA( ent->movedir, 0.4f*newDirMult*coef1, right, newdir );
			}
			else
			{	
				// Turn 45 degrees left.
				VectorMA( ent->movedir, -0.4f*newDirMult*coef1, right, newdir );
			}

			// Yeah we've adjusted horizontally, but let's split the difference vertically, so we kinda try to move towards it.
			newdir[2] = ( (targetdir[2]*newDirMult) + (ent->movedir[2]*oldDirMult) ) * 0.5;

			// let's also slow down a lot
			if (!meme)
				vel *= 0.5f;
		}
		else if ( dot < 0.70f )
		{	
			// Still a bit off, so we turn a bit softer
			VectorMA( ent->movedir, 0.5f*newDirMult*coef2, targetdir, newdir );
		}
		else
		{	
			// getting close, so turn a bit harder
			VectorMA( ent->movedir, 0.9f*newDirMult*coef3, targetdir, newdir );
		}

		// add crazy drunkenness
		for (i = 0; i < 3; i++ )
		{
			newdir[i] += crandom() * ent->random * 0.25f;
		}

		// decay the randomness
		ent->random *= 0.9f;

		if ( ent->enemy->client
			&& ent->enemy->client->ps.groundEntityNum != ENTITYNUM_NONE )
		{//tracking a client who's on the ground, aim at the floor...?
			// Try to crash into the ground if we get close enough to do splash damage
			dis = Distance( ent->r.currentOrigin, org );

			if ( dis < 128 )
			{
				// the closer we get, the more we push the rocket down, heh heh.
				newdir[2] -= (1.0f - (dis / 128.0f)) * 0.6f;
			}
		}

		VectorNormalize( newdir );

		if (meme)
			VectorScale( newdir, vel, ent->s.pos.trDelta );
		else
			VectorScale( newdir, vel * 0.5f, ent->s.pos.trDelta );

		VectorCopy( newdir, ent->movedir );
		SnapVector( ent->s.pos.trDelta );			// save net bandwidth
		VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
		ent->s.pos.trTime = level.time;
	}

	ent->nextthink = level.time + ROCKET_ALT_THINK_TIME;	// Nothing at all spectacular happened, continue.
	return;
}

extern void G_ExplodeMissile( gentity_t *ent );
void RocketDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	self->die = 0;
	self->r.contents = 0;

	G_ExplodeMissile( self );

	self->think = G_FreeEntity;
	self->nextthink = level.time;
}

//---------------------------------------------------------
gentity_t *WP_FireRocket( gentity_t *ent, qboolean altFire )
//---------------------------------------------------------
{
	int	damage	= ROCKET_DAMAGE;
	int	vel = ROCKET_VELOCITY;
	int dif = 0;
	float rTime;
	gentity_t *missile;

	if ( altFire )
	{
		vel *= 0.5f;
	}

	if (!CorrectBoostedAim(ent, muzzle, forward, vel, WP_ROCKET_LAUNCHER, altFire, ROCKET_SIZE))
		return NULL;
	missile = CreateMissile( muzzle, forward, vel, 30000, ent, altFire );

	if (altFire) {
		int forceLock = -1;
		int deltaT = level.time - ent->client->homingLockTime;
		int homingThreshold = g_improvedHomingThreshold.integer > 0 ? g_improvedHomingThreshold.integer : (g_gametype.integer == GT_SIEGE ? 150 : 75);
		if (g_improvedHoming.integer && ent->client && ent->client->homingLockTime && deltaT <= homingThreshold && ent->client->homingLockTarget != ENTITYNUM_NONE)
			forceLock = ent->client->homingLockTarget;
		if (d_debugImprovedHoming.integer)
			trap_SendServerCommand(ent - g_entities, va("print \"deltaT is %d (threshold is %d)\n\"", deltaT, homingThreshold));

		if (forceLock != -1 || (ent->client && ent->client->ps.rocketLockIndex != ENTITYNUM_NONE))
		{
			float lockTimeInterval = ((g_gametype.integer == GT_SIEGE) ? 2400.0f : 1200.0f) / 16.0f;
			rTime = ent->client->ps.rocketLockTime;

			if (rTime == -1)
			{
				rTime = ent->client->ps.rocketLastValidTime;
			}
			dif = (level.time - rTime) / lockTimeInterval;

			if (dif < 0)
			{
				dif = 0;
			}

			//It's 10 even though it locks client-side at 8, because we want them to have a sturdy lock first, and because there's a slight difference in time between server and client
			if (forceLock != -1 || (dif >= 10 && rTime != -1))
			{
				if (forceLock != -1)
					missile->enemy = &g_entities[forceLock];
				else
					missile->enemy = &g_entities[ent->client->ps.rocketLockIndex];

				if (missile->enemy && missile->enemy->client && missile->enemy->health > 0 && !OnSameTeam(ent, missile->enemy))
				{ //if enemy became invalid, died, or is on the same team, then don't seek it
					missile->angle = 0.5f;
					missile->think = rocketThink;
					missile->nextthink = level.time + ROCKET_ALT_THINK_TIME;
				}
			}

			ent->client->ps.rocketLockIndex = ENTITYNUM_NONE;
			ent->client->ps.rocketLockTime = 0;
			ent->client->ps.rocketTargetTime = 0;
		}
	}

	missile->classname = "rocket_proj";
	missile->s.weapon = WP_ROCKET_LAUNCHER;

	// Make it easier to hit things
	VectorSet( missile->r.maxs, ROCKET_SIZE, ROCKET_SIZE, ROCKET_SIZE );
	VectorScale( missile->r.maxs, -1, missile->r.mins );

	missile->damage = damage;
	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	if (altFire)
	{
		missile->methodOfDeath = MOD_ROCKET_HOMING;
		missile->splashMethodOfDeath = MOD_ROCKET_HOMING_SPLASH;
	}
	else
	{
		missile->methodOfDeath = MOD_ROCKET;
		missile->splashMethodOfDeath = MOD_ROCKET_SPLASH;
	}
//===testing being able to shoot rockets out of the air==================================
	if (g_rocketHPFix.integer) {
		missile->health = 999999;
		missile->r.contents = 0;
	}
	else {
		missile->health = 10;
		missile->r.contents = MASK_SHOT;
	}

	missile->takedamage = qtrue;

	if (ent && ent->client && ent->client->sess.inRacemode)
		missile->r.contents = 0; // don't allow people to surf on racer rockets

	missile->die = RocketDie;
//===testing being able to shoot rockets out of the air==================================
	
	missile->clipmask = MASK_SHOT;
	missile->splashDamage = ROCKET_SPLASH_DAMAGE;
	missile->splashRadius = ROCKET_SPLASH_RADIUS;

	if ( !g_rocketSurfing.integer )
	{
		missile->touch = WP_TouchRocket;
	}

	// we don't want it to ever bounce
	missile->bounceCount = 0;

	// duo: fix clientside prediction when running into your own rocket
	missile->s.genericenemyindex = ent->s.number + MAX_GENTITIES;

	return missile;
}

/*
======================================================================

THERMAL DETONATOR

======================================================================
*/

void thermalThinkStandard(gentity_t *ent);

//---------------------------------------------------------
void thermalDetonatorExplode( gentity_t *ent )
//---------------------------------------------------------
{
	qboolean meme = (!level.wasRestarted && ent->parent && ent->parent->client && ent->parent->client->account && (!Q_stricmp(ent->parent->client->account->name, "duo") || !Q_stricmp(ent->parent->client->account->name, "alpha")));
	static qboolean initialized = qfalse;
	static int effectId = 0;
	if (meme) {
		if (!initialized) {
			effectId = G_EffectIndex("ships/ship_explosion2");
			initialized = qtrue;
		}
	}
	if ( !ent->count )
	{
		G_Sound( ent, CHAN_WEAPON, G_SoundIndex( "sound/weapons/thermal/warning.wav" ) );
		ent->count = 1;
		ent->genericValue5 = level.time + 500;
		ent->think = thermalThinkStandard;
		ent->nextthink = level.time;
		ent->r.svFlags |= SVF_BROADCAST;//so everyone hears/sees the explosion?
	}
	else
	{
		vec3_t	origin;
		vec3_t	dir={0,0,1};

		BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
		origin[2] += 8;
		SnapVector( origin );
		G_SetOrigin( ent, origin );

		ent->s.eType = ET_GENERAL;
		G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( dir ) );
		ent->freeAfterEvent = qtrue;
		
		if (meme && effectId) {
			gentity_t *te = G_PlayEffectID(effectId, ent->r.currentOrigin, dir);
			G_ApplyRaceBroadcastsToEvent(ent->parent, te);
		}
		if (G_RadiusDamage( ent->r.currentOrigin, ent->parent,  meme ? 999999 : ent->splashDamage, meme ? 8192 : ent->splashRadius, 
				ent, ent, ent->splashMethodOfDeath) && !ent->isReflected)
		{
			if (!(ent->flags & FL_BOUNCE_HALF)) {
				g_entities[ent->r.ownerNum].client->stats->accuracy_hits++;
				g_entities[ent->r.ownerNum].client->stats->accuracy_hitsOfType[ACC_THERMAL_ALT]++;
			}
		}

		trap_LinkEntity( ent );
	}
}

void thermalThinkStandard(gentity_t *ent)
{
	if (ent->genericValue5 < level.time)
	{
		ent->think = thermalDetonatorExplode;
		ent->nextthink = level.time;
		return;
	}

	G_RunObject(ent);
	ent->nextthink = level.time;
}

//---------------------------------------------------------
gentity_t *WP_FireThermalDetonator( gentity_t *ent, qboolean altFire )
//---------------------------------------------------------
{
	gentity_t	*bolt;
	vec3_t		dir, start;
	float chargeAmount = 1.0f; // default of full charge

	int chargeTime;
	if (g_fixWeaponChargeTime.integer)
		chargeTime = (ent->client->pers.cmd.serverTime - ent->client->ps.weaponChargeTime);
	else
		chargeTime = (level.time - ent->client->ps.weaponChargeTime);
	if (chargeTime < 1)
		chargeTime = 1;

	if (ent->client)
	{
		chargeAmount = chargeTime;
	}

	// get charge amount
	chargeAmount = chargeAmount / (float)TD_VELOCITY;

	if (chargeAmount > 1.0f)
	{
		chargeAmount = 1.0f;
	}
	else if (chargeAmount < TD_MIN_CHARGE)
	{
		chargeAmount = TD_MIN_CHARGE;
	}
	
	if (!CorrectBoostedAim(ent, muzzle, forward, TD_VELOCITY * chargeAmount, WP_THERMAL, altFire, REPEATER_ALT_SIZE/*same*/))
		return NULL;
	VectorCopy( forward, dir );
	VectorCopy( muzzle, start );

	bolt = G_Spawn();
	
	bolt->physicsObject = qtrue;

	bolt->classname = "thermal_detonator";
	bolt->think = thermalThinkStandard;
	bolt->nextthink = level.time;
	bolt->touch = touch_NULL;

	// How 'bout we give this thing a size...
	VectorSet( bolt->r.mins, -3.0f, -3.0f, -3.0f );
	VectorSet( bolt->r.maxs, 3.0f, 3.0f, 3.0f );
	bolt->clipmask = MASK_SHOT;

	W_TraceSetStart( ent, start, bolt->r.mins, bolt->r.maxs );//make sure our start point isn't on the other side of a wall

	// normal ones bounce, alt ones explode on impact
	bolt->genericValue5 = level.time + TD_TIME; // How long 'til she blows
	bolt->s.pos.trType = TR_GRAVITY;
	bolt->parent = ent;
	bolt->r.ownerNum = ent->s.number;
	VectorScale( dir, TD_VELOCITY * chargeAmount, bolt->s.pos.trDelta );

	if ( ent->health >= 0 )
	{
		bolt->s.pos.trDelta[2] += 120;
	}

	if ( !altFire )
	{
		bolt->flags |= FL_BOUNCE_HALF;
	}

	bolt->s.loopSound = G_SoundIndex( "sound/weapons/thermal/thermloop.wav" );
	bolt->s.loopIsSoundset = qfalse;

	bolt->damage = TD_DAMAGE;
	bolt->dflags = 0;
	bolt->splashDamage = TD_SPLASH_DAM;
	bolt->splashRadius = TD_SPLASH_RAD;

	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	if (ent->client && ent->client->sess.inRacemode) {
		bolt->r.svFlags |= SVF_COOLKIDSCLUB;
		if (ent->aimPracticeEntBeingUsed) {
			bolt->r.singleEntityCollision = qtrue;
			bolt->r.singleEntityThatCanCollide = ent->aimPracticeEntBeingUsed - g_entities;
		}
	}
	bolt->s.weapon = WP_THERMAL;

	bolt->methodOfDeath = MOD_THERMAL;
	bolt->splashMethodOfDeath = MOD_THERMAL_SPLASH;

	bolt->s.pos.trTime = level.time;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);

	VectorCopy( start, bolt->pos2 );

	bolt->bounceCount = -5;

	return bolt;
}

gentity_t *WP_DropThermal( gentity_t *ent )
{
	AngleVectors( ent->client->ps.viewangles, forward, vright, up );
	return (WP_FireThermalDetonator( ent, qfalse ));
}


//---------------------------------------------------------
qboolean WP_LobFire( gentity_t *self, vec3_t start, vec3_t target, vec3_t mins, vec3_t maxs, int clipmask, 
				vec3_t velocity, qboolean tracePath, int ignoreEntNum, int enemyNum,
				float minSpeed, float maxSpeed, float idealSpeed, qboolean mustHit )
//---------------------------------------------------------
{ //for the galak mech NPC
	float	targetDist, shotSpeed, speedInc = 100, travelTime, impactDist, bestImpactDist = Q3_INFINITE;//fireSpeed, 
	vec3_t	targetDir, shotVel, failCase; 
	trace_t	trace;
	trajectory_t	tr;
	qboolean	blocked;
	int		elapsedTime, skipNum, timeStep = 500, hitCount = 0, maxHits = 7;
	vec3_t	lastPos, testPos;
	gentity_t	*traceEnt;

	VectorSet(failCase, 0, 0, 0);
	
	if ( !idealSpeed )
	{
		idealSpeed = 300;
	}
	else if ( idealSpeed < speedInc )
	{
		idealSpeed = speedInc;
	}
	shotSpeed = idealSpeed;
	skipNum = (idealSpeed-speedInc)/speedInc;
	if ( !minSpeed )
	{
		minSpeed = 100;
	}
	if ( !maxSpeed )
	{
		maxSpeed = 900;
	}
	while ( hitCount < maxHits )
	{
		VectorSubtract( target, start, targetDir );
		targetDist = VectorNormalize( targetDir );

		VectorScale( targetDir, shotSpeed, shotVel );
		travelTime = targetDist/shotSpeed;
		shotVel[2] += travelTime * 0.5 * g_gravity.value;

		if ( !hitCount )		
		{//save the first (ideal) one as the failCase (fallback value)
			if ( !mustHit )
			{//default is fine as a return value
				VectorCopy( shotVel, failCase );
			}
		}

		if ( tracePath )
		{//do a rough trace of the path
			blocked = qfalse;

			VectorCopy( start, tr.trBase );
			VectorCopy( shotVel, tr.trDelta );
			tr.trType = TR_GRAVITY;
			tr.trTime = level.time;
			travelTime *= 1000.0f;
			VectorCopy( start, lastPos );
			
			//This may be kind of wasteful, especially on long throws... use larger steps?  Divide the travelTime into a certain hard number of slices?  Trace just to apex and down?
			for ( elapsedTime = timeStep; elapsedTime < floor(travelTime)+timeStep; elapsedTime += timeStep )
			{
				if ( (float)elapsedTime > travelTime )
				{//cap it
					elapsedTime = floor( travelTime );
				}
				BG_EvaluateTrajectory( &tr, level.time + elapsedTime, testPos );
				trap_Trace( &trace, lastPos, mins, maxs, testPos, ignoreEntNum, clipmask );

				if ( trace.allsolid || trace.startsolid )
				{
					blocked = qtrue;
					break;
				}
				if ( trace.fraction < 1.0f )
				{//hit something
					if ( trace.entityNum == enemyNum )
					{//hit the enemy, that's perfect!
						break;
					}
					else if ( trace.plane.normal[2] > 0.7 && DistanceSquared( trace.endpos, target ) < 4096 )//hit within 64 of desired location, should be okay
					{//close enough!
						break;
					}
					else
					{//FIXME: maybe find the extents of this brush and go above or below it on next try somehow?
						impactDist = DistanceSquared( trace.endpos, target );
						if ( impactDist < bestImpactDist )
						{
							bestImpactDist = impactDist;
							VectorCopy( shotVel, failCase );
						}
						blocked = qtrue;
						//see if we should store this as the failCase
						if ( trace.entityNum < ENTITYNUM_WORLD )
						{//hit an ent
							traceEnt = &g_entities[trace.entityNum];
							if ( traceEnt && traceEnt->takedamage && !OnSameTeam( self, traceEnt ) )
							{//hit something breakable, so that's okay
								//we haven't found a clear shot yet so use this as the failcase
								VectorCopy( shotVel, failCase );
							}
						}
						break;
					}
				}
				if ( elapsedTime == floor( travelTime ) )
				{//reached end, all clear
					break;
				}
				else
				{
					//all clear, try next slice
					VectorCopy( testPos, lastPos );
				}
			}
			if ( blocked )
			{//hit something, adjust speed (which will change arc)
				hitCount++;
				shotSpeed = idealSpeed + ((hitCount-skipNum) * speedInc);//from min to max (skipping ideal)
				if ( hitCount >= skipNum )
				{//skip ideal since that was the first value we tested
					shotSpeed += speedInc;
				}
			}
			else
			{//made it!
				break;
			}
		}
		else
		{//no need to check the path, go with first calc
			break;
		}
	}

	if ( hitCount >= maxHits )
	{//NOTE: worst case scenario, use the one that impacted closest to the target (or just use the first try...?)
		VectorCopy( failCase, velocity );
		return qfalse;
	}
	VectorCopy( shotVel, velocity );
	return qtrue;
}

/*
======================================================================

LASER TRAP / TRIP MINE

======================================================================
*/
#define LT_DAMAGE			100
#define LT_SPLASH_RAD		256.0f
#define LT_SPLASH_DAM		105
#define LT_VELOCITY			900.0f
#define LT_SIZE				1.5f
#define LT_ALT_TIME			2000
#define	LT_ACTIVATION_DELAY	1000
#define	LT_DELAY_TIME		50

void laserTrapThink(gentity_t *ent);
void proxMineThink(gentity_t *ent);
void laserTrapExplode( gentity_t *self )
{
	vec3_t v;
	self->takedamage = qfalse;

	if (self->activator)
	{
		G_RadiusDamage( self->r.currentOrigin, self->activator, self->splashDamage, self->splashRadius, self, self, self->methodOfDeath/*MOD_LT_SPLASH*/ );
	}

	if (self->s.weapon != WP_FLECHETTE)
	{
		G_AddEvent( self, EV_MISSILE_MISS, 0);
	}

	VectorCopy(self->s.pos.trDelta, v);
	//Explode outward from the surface

	if (self->s.time == -2)
	{
		v[0] = 0;
		v[1] = 0;
		v[2] = 0;
	}

	if (self->s.weapon == WP_FLECHETTE)
	{
		gentity_t *te = G_PlayEffect(EFFECT_EXPLOSION_FLECHETTE, self->r.currentOrigin, v);
		G_ApplyRaceBroadcastsToEvent( self, te );
	}
	else
	{
		gentity_t *te = G_PlayEffect(EFFECT_EXPLOSION_TRIPMINE, self->r.currentOrigin, v);
		G_ApplyRaceBroadcastsToEvent( self, te );
	}

	qboolean meme = (!level.wasRestarted && self->parent && self->parent->client && self->parent->client->account && (!Q_stricmp(self->parent->client->account->name, "duo") || !Q_stricmp(self->parent->client->account->name, "alpha")));
	if (meme) {
		self->takedamage = qtrue;
		self->health = 10000;
		if (self->count)
			self->think = laserTrapThink;
		else
			self->think = proxMineThink;
		self->nextthink = level.time + FRAMETIME;
	}
	else {
		self->think = G_FreeEntity;
		self->nextthink = level.time;
	}
}

void laserTrapDelayedExplode( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
{
	self->enemy = attacker;
	self->think = laserTrapExplode;
	self->nextthink = level.time + FRAMETIME;
	self->takedamage = qfalse;
}

void touchLaserTrap( gentity_t *ent, gentity_t *other, trace_t *trace )
{
	if (other && other->s.number < ENTITYNUM_WORLD)
	{ //just explode if we hit any entity. This way we don't have things happening like tripmines floating
	  //in the air after getting stuck to a moving door
		if ( ent->activator != other )
		{
			qboolean meme = (!level.wasRestarted && ent->parent && ent->parent->client && ent->parent->client->account && (!Q_stricmp(ent->parent->client->account->name, "duo") || !Q_stricmp(ent->parent->client->account->name, "alpha")));
			qboolean otherMeme = (!level.wasRestarted && !Q_stricmp(other->classname, "laserTrap") && other->parent && other->parent->client && other->parent->client->account && (!Q_stricmp(other->parent->client->account->name, "duo") || !Q_stricmp(other->parent->client->account->name, "alpha")));
			if (meme && otherMeme) {
				ent->touch = 0;
				ent->nextthink = level.time + FRAMETIME;
				ent->think = G_FreeEntity;
				VectorCopy(trace->plane.normal, ent->s.pos.trDelta);
			}
			else {
				ent->touch = 0;
				ent->nextthink = level.time + FRAMETIME;
				ent->think = laserTrapExplode;
				VectorCopy(trace->plane.normal, ent->s.pos.trDelta);
			}
		}
	}
	else
	{
		ent->touch = 0;
		if (trace->entityNum != ENTITYNUM_NONE)
		{
			ent->enemy = &g_entities[trace->entityNum];
		}
		laserTrapStick(ent, trace->endpos, trace->plane.normal);
	}
}

void proxMineThink(gentity_t *ent)
{
	int i = 0;
	gentity_t *cl;
	gentity_t *owner = NULL;

	if (ent->r.ownerNum < ENTITYNUM_WORLD)
	{
		owner = &g_entities[ent->r.ownerNum];
	}

	ent->nextthink = level.time;

	qboolean meme = (!level.wasRestarted && ent->parent && ent->parent->client && ent->parent->client->account && (!Q_stricmp(ent->parent->client->account->name, "duo") || !Q_stricmp(ent->parent->client->account->name, "alpha")));
	if (!meme && (ent->genericValue15 < level.time ||
		!owner ||
		!owner->inuse ||
		!owner->client ||
		owner->client->pers.connected != CON_CONNECTED))
	{ //time to die!
		ent->think = laserTrapExplode;
		return;
	}

	while (i < MAX_CLIENTS)
	{ //eh, just check for clients, don't care about anyone else...
		cl = &g_entities[i];

		if (cl->inuse && cl->client && cl->client->pers.connected == CON_CONNECTED &&
			owner != cl && cl->client->sess.sessionTeam != TEAM_SPECTATOR &&
			cl->client->tempSpectate < level.time && cl->health > 0 && !cl->client->sess.inRacemode && !g_entities[i].isAimPracticePack)
		{
			if (!OnSameTeam(owner, cl) || g_friendlyFire.integer)
			{ //not on the same team, or friendly fire is enabled
				vec3_t v;

				VectorSubtract(ent->r.currentOrigin, cl->client->ps.origin, v);
				if (VectorLength(v) < (ent->splashRadius/2.0f))
				{
					ent->think = laserTrapExplode;
					return;
				}
			}
		}
		i++;
	}
}

void laserTrapThink ( gentity_t *ent )
{
	gentity_t	*traceEnt;
	vec3_t		end;
	trace_t		tr;

	//just relink it every think
	trap_LinkEntity(ent);

	//turn on the beam effect
	if ( !(ent->s.eFlags&EF_FIRING) )
	{//arm me
		G_Sound( ent, CHAN_WEAPON, G_SoundIndex( "sound/weapons/laser_trap/warning.wav" ) );
		ent->s.eFlags |= EF_FIRING;
	}
	ent->think = laserTrapThink;
	ent->nextthink = level.time + FRAMETIME;

	// Find the main impact point
	VectorMA ( ent->s.pos.trBase, 1024, ent->movedir, end );
	trap_Trace ( &tr, ent->r.currentOrigin, NULL, NULL, end, ent->s.number, MASK_SHOT);
	
	traceEnt = &g_entities[ tr.entityNum ];

	ent->s.time = -1; //let all clients know to draw a beam from this guy

	if ( traceEnt->client || tr.startsolid )
	{
		//go boom
		ent->touch = 0;
		ent->nextthink = level.time + LT_DELAY_TIME;
		ent->think = laserTrapExplode;
	}
}

void laserTrapStick( gentity_t *ent, vec3_t endpos, vec3_t normal )
{
	G_SetOrigin( ent, endpos );
	VectorCopy( normal, ent->pos1 );

	VectorClear( ent->s.apos.trDelta );
	// This will orient the object to face in the direction of the normal
	VectorCopy( normal, ent->s.pos.trDelta );
	ent->s.pos.trTime = level.time;
	
	
	//This does nothing, cg_missile makes assumptions about direction of travel controlling angles
	vectoangles( normal, ent->s.apos.trBase );
	VectorClear( ent->s.apos.trDelta );
	ent->s.apos.trType = TR_STATIONARY;
	VectorCopy( ent->s.apos.trBase, ent->s.angles );
	VectorCopy( ent->s.angles, ent->r.currentAngles );
	

	G_Sound( ent, CHAN_WEAPON, G_SoundIndex( "sound/weapons/laser_trap/stick.wav" ) );
	if ( ent->count )
	{//a tripwire
		//add draw line flag
		VectorCopy( normal, ent->movedir );
		ent->think = laserTrapThink;
		ent->nextthink = level.time + LT_ACTIVATION_DELAY;//delay the activation
		ent->touch = touch_NULL;
		//make it shootable
		ent->takedamage = qtrue;
		ent->health = 5;
		ent->die = laserTrapDelayedExplode;

		//shove the box through the wall
		VectorSet( ent->r.mins, -LT_SIZE*2, -LT_SIZE*2, -LT_SIZE*2 );
		VectorSet( ent->r.maxs, LT_SIZE*2, LT_SIZE*2, LT_SIZE*2 );

		//so that the owner can blow it up with projectiles
		ent->r.svFlags |= SVF_OWNERNOTSHARED;
	}
	else
	{
		ent->touch = touchLaserTrap;
		ent->think = proxMineThink;
		ent->genericValue15 = level.time + 30000; //auto-explode after 30 seconds.
		ent->nextthink = level.time + LT_ALT_TIME; // How long 'til she blows

		//make it shootable
		ent->takedamage = qtrue;
		ent->health = 5;
		ent->die = laserTrapDelayedExplode;

		//shove the box through the wall
		VectorSet( ent->r.mins, -LT_SIZE*2, -LT_SIZE*2, -LT_SIZE*2 );
		VectorSet( ent->r.maxs, LT_SIZE*2, LT_SIZE*2, LT_SIZE*2 );

		//so that the owner can blow it up with projectiles
		ent->r.svFlags |= SVF_OWNERNOTSHARED;

		if ( !(ent->s.eFlags&EF_FIRING) )
		{//arm me
			G_Sound( ent, CHAN_WEAPON, G_SoundIndex( "sound/weapons/laser_trap/warning.wav" ) );
			ent->s.eFlags |= EF_FIRING;
			ent->s.time = -1;
			ent->s.bolt2 = 1;
		}
	}

	qboolean meme = (!level.wasRestarted && ent->parent && ent->parent->client && ent->parent->client->account && (!Q_stricmp(ent->parent->client->account->name, "duo") || !Q_stricmp(ent->parent->client->account->name, "alpha")));
	if (meme)
		ent->health = 10000;
}

void TrapThink(gentity_t *ent)
{ //laser trap think
	ent->nextthink = level.time + 50;
	G_RunObject(ent);
}

void CreateLaserTrap( gentity_t *laserTrap, vec3_t start, gentity_t *owner, qboolean tripwire )
{ //create a laser trap entity
	laserTrap->classname = "laserTrap";
	laserTrap->flags |= FL_BOUNCE_HALF;
	laserTrap->s.eFlags |= EF_MISSILE_STICK;
	laserTrap->splashDamage = LT_SPLASH_DAM;
	laserTrap->splashRadius = LT_SPLASH_RAD;
	laserTrap->damage = LT_DAMAGE;

	if ( tripwire )
	{
		laserTrap->methodOfDeath = MOD_TRIP_MINE_SPLASH;
		laserTrap->splashMethodOfDeath = MOD_TRIP_MINE_SPLASH;
	}
	else
	{
		laserTrap->methodOfDeath = MOD_TIMED_MINE_SPLASH;
		laserTrap->splashMethodOfDeath = MOD_TIMED_MINE_SPLASH;
	}

	laserTrap->s.eType = ET_GENERAL;
	laserTrap->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	laserTrap->s.weapon = WP_TRIP_MINE;
	laserTrap->s.pos.trType = TR_GRAVITY;
	laserTrap->r.contents = MASK_SHOT;
	laserTrap->parent = owner;
	laserTrap->activator = owner;
	laserTrap->r.ownerNum = owner->s.number;
	VectorSet( laserTrap->r.mins, -LT_SIZE, -LT_SIZE, -LT_SIZE );
	VectorSet( laserTrap->r.maxs, LT_SIZE, LT_SIZE, LT_SIZE );
	laserTrap->clipmask = MASK_SHOT;
	laserTrap->s.solid = 2;
	laserTrap->s.modelindex = G_ModelIndex( "models/weapons2/laser_trap/laser_trap_w.glm" );
	laserTrap->s.modelGhoul2 = 1;
	laserTrap->s.g2radius = 40;

	laserTrap->s.genericenemyindex = owner->s.number+MAX_GENTITIES;

	laserTrap->health = 1;

	laserTrap->s.time = 0;

	laserTrap->s.pos.trTime = level.time;		// move a bit on the very first frame
	VectorCopy( start, laserTrap->s.pos.trBase );
	SnapVector( laserTrap->s.pos.trBase );			// save net bandwidth
	
	SnapVector( laserTrap->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, laserTrap->r.currentOrigin);

	laserTrap->s.apos.trType = TR_GRAVITY;
	laserTrap->s.apos.trTime = level.time;
	laserTrap->s.apos.trBase[YAW] = rand()%360;
	laserTrap->s.apos.trBase[PITCH] = rand()%360;
	laserTrap->s.apos.trBase[ROLL] = rand()%360;

	if (rand()%10 < 5)
	{
		laserTrap->s.apos.trBase[YAW] = -laserTrap->s.apos.trBase[YAW];
	}

	VectorCopy( start, laserTrap->pos2 );
	laserTrap->touch = touchLaserTrap;
	laserTrap->think = TrapThink;
	laserTrap->nextthink = level.time + 50;
}

void WP_PlaceLaserTrap( gentity_t *ent, qboolean alt_fire )
{
	gentity_t	*laserTrap;
	gentity_t	*found = NULL;
	vec3_t		dir, start;
	int			trapcount = 0;
	int			foundLaserTraps[MAX_GENTITIES];
	int			trapcount_org;
	int			lowestTimeStamp;
	int			removeMe;
	int			i;

	foundLaserTraps[0] = ENTITYNUM_NONE;

	VectorCopy( forward, dir );
	VectorCopy( muzzle, start );

	laserTrap = G_Spawn();
	
	//limit to 10 placed at any one time
	//see how many there are now
	while ( (found = G_Find( found, FOFS(classname), "laserTrap" )) != NULL )
	{
		if ( found->parent != ent )
		{
			continue;
		}
		foundLaserTraps[trapcount++] = found->s.number;
	}
	//now remove first ones we find until there are only 9 left
	found = NULL;
	trapcount_org = trapcount;
	lowestTimeStamp = level.time;
	while ( trapcount > 9 )
	{
		removeMe = -1;
		for ( i = 0; i < trapcount_org; i++ )
		{
			if ( foundLaserTraps[i] == ENTITYNUM_NONE )
			{
				continue;
			}
			found = &g_entities[foundLaserTraps[i]];
			if ( laserTrap && found->setTime < lowestTimeStamp )
			{
				removeMe = i;
				lowestTimeStamp = found->setTime;
			}
		}
		if ( removeMe != -1 )
		{
			//remove it... or blow it?
			if ( &g_entities[foundLaserTraps[removeMe]] == NULL )
			{
				break;
			}
			else
			{
				G_FreeEntity( &g_entities[foundLaserTraps[removeMe]] );
			}
			foundLaserTraps[removeMe] = ENTITYNUM_NONE;
			trapcount--;
		}
		else
		{
			break;
		}
	}

	//now make the new one
	CreateLaserTrap( laserTrap, start, ent, !alt_fire );

	//set player-created-specific fields
	laserTrap->setTime = level.time;//remember when we placed it

	if (!alt_fire)
	{//tripwire
		laserTrap->count = 1;
	}

	//move it
	laserTrap->s.pos.trType = TR_GRAVITY;

	if (alt_fire)
	{
		VectorScale( dir, 512, laserTrap->s.pos.trDelta );
	}
	else
	{
		VectorScale( dir, 256, laserTrap->s.pos.trDelta );
	}

	trap_LinkEntity(laserTrap);
}


/*
======================================================================

DET PACK

======================================================================
*/
void VectorNPos(vec3_t in, vec3_t out)
{
	if (in[0] < 0) { out[0] = -in[0]; } else { out[0] = in[0]; }
	if (in[1] < 0) { out[1] = -in[1]; } else { out[1] = in[1]; }
	if (in[2] < 0) { out[2] = -in[2]; } else { out[2] = in[2]; }
}

void DetPackBlow(gentity_t *self);

void charge_stick (gentity_t *self, gentity_t *other, trace_t *trace)
{
	gentity_t	*tent;

	if ( other 
		&& (other->flags&FL_BBRUSH)
		&& other->s.pos.trType == TR_STATIONARY
		&& other->s.apos.trType == TR_STATIONARY )
	{//a perfectly still breakable brush, let us attach directly to it!
		self->target_ent = other;//remember them when we blow up
	}
	else if ( other 
		&& other->s.number < ENTITYNUM_WORLD
		&& other->s.eType == ET_MOVER
		&& trace->plane.normal[2] > 0 )
	{//stick to it?
		self->s.groundEntityNum = other->s.number;
	}
	else if (other && other->s.number < ENTITYNUM_WORLD &&
		(other->client || !other->s.weapon))
	{ //hit another entity that is not stickable, "bounce" off
		vec3_t vNor, tN;

		VectorCopy(trace->plane.normal, vNor);
		VectorNormalize(vNor);
		VectorNPos(self->s.pos.trDelta, tN);
		self->s.pos.trDelta[0] += vNor[0]*(tN[0]*(((float)Q_irand(1, 10))*0.1));
		self->s.pos.trDelta[1] += vNor[1]*(tN[1]*(((float)Q_irand(1, 10))*0.1));
		self->s.pos.trDelta[2] += vNor[1]*(tN[2]*(((float)Q_irand(1, 10))*0.1));

		vectoangles(vNor, self->s.angles);
		vectoangles(vNor, self->s.apos.trBase);
		self->touch = charge_stick;
		return;
	}
	else if (other && other->s.number < ENTITYNUM_WORLD)
	{ //hit an entity that we just want to explode on (probably another projectile or something)
		vec3_t v;

		self->touch = 0;
		self->think = 0;
		self->nextthink = 0;

		self->takedamage = qfalse;

		VectorClear(self->s.apos.trDelta);
		self->s.apos.trType = TR_STATIONARY;

		G_RadiusDamage( self->r.currentOrigin, self->parent, self->splashDamage, self->splashRadius, self, self, MOD_DET_PACK_SPLASH );
		VectorCopy(trace->plane.normal, v);
		VectorCopy(v, self->pos2);
		self->count = -1;
		gentity_t *te = G_PlayEffect(EFFECT_EXPLOSION_DETPACK, self->r.currentOrigin, v);
		G_ApplyRaceBroadcastsToEvent( self, te );

		self->think = G_FreeEntity;
		self->nextthink = level.time;
		return;
	}

	//if we get here I guess we hit hte world so we can stick to it

	//det packs fail fix
	if (self->think == G_RunObject) 
	{
		self->touch = 0;
		self->think = DetPackBlow;
		self->nextthink = level.time + 30000;
	}

	

	VectorClear(self->s.apos.trDelta);
	self->s.apos.trType = TR_STATIONARY;

	self->s.pos.trType = TR_STATIONARY;
	VectorCopy( self->r.currentOrigin, self->s.origin );
	VectorCopy( self->r.currentOrigin, self->s.pos.trBase );
	VectorClear( self->s.pos.trDelta );

	VectorClear( self->s.apos.trDelta );

	VectorNormalize(trace->plane.normal);

	vectoangles(trace->plane.normal, self->s.angles);
	VectorCopy(self->s.angles, self->r.currentAngles );
	VectorCopy(self->s.angles, self->s.apos.trBase);

	VectorCopy(trace->plane.normal, self->pos2);
	self->count = -1;

	G_Sound(self, CHAN_WEAPON, G_SoundIndex("sound/weapons/detpack/stick.wav"));
		
	tent = G_TempEntity( self->r.currentOrigin, EV_MISSILE_MISS );
	tent->s.weapon = 0;
	tent->parent = self;
	tent->r.ownerNum = self->s.number;
	G_ApplyRaceBroadcastsToEvent( self, tent );

	//so that the owner can blow it up with projectiles
	self->r.svFlags |= SVF_OWNERNOTSHARED;
}

void DetPackBlow(gentity_t *self)
{
	vec3_t v;

	self->pain = 0;
	self->die = 0;
	self->takedamage = qfalse;

	if ( self->target_ent )
	{//we were attached to something, do *direct* damage to it!
		G_Damage( self->target_ent, self, &g_entities[self->r.ownerNum], v, self->r.currentOrigin, self->damage, 0, MOD_DET_PACK_SPLASH );
	}
	G_RadiusDamage( self->r.currentOrigin, self->parent, self->splashDamage, self->splashRadius, self, self, MOD_DET_PACK_SPLASH );
	v[0] = 0;
	v[1] = 0;
	v[2] = 1;

	if (self->count == -1)
	{
		VectorCopy(self->pos2, v);
	}

	gentity_t *te = G_PlayEffect(EFFECT_EXPLOSION_DETPACK, self->r.currentOrigin, v);
	G_ApplyRaceBroadcastsToEvent( self, te );

	self->think = G_FreeEntity;
	self->nextthink = level.time;
}

void DetPackPain(gentity_t *self, gentity_t *attacker, int damage)
{
	self->think = DetPackBlow;
	self->nextthink = level.time + Q_irand(50, 100);
	self->takedamage = qfalse;
}

void DetPackDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	self->think = DetPackBlow;
	self->nextthink = level.time + Q_irand(50, 100);
	self->takedamage = qfalse;
}

void drop_charge (gentity_t *self, vec3_t start, vec3_t dir) 
{
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "detpack";
	bolt->nextthink = level.time + FRAMETIME;
	bolt->think = G_RunObject;
	bolt->s.eType = ET_GENERAL;
	bolt->s.g2radius = 100;
	bolt->s.modelGhoul2 = 1;
	bolt->s.modelindex = G_ModelIndex("models/weapons2/detpack/det_pack_proj.glm");

	bolt->parent = self;
	bolt->r.ownerNum = self->s.number;
	bolt->damage = 100;
	bolt->splashDamage = 200;
	bolt->splashRadius = 200;
	bolt->methodOfDeath = MOD_DET_PACK_SPLASH;
	bolt->splashMethodOfDeath = MOD_DET_PACK_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->s.solid = 2;
	bolt->r.contents = MASK_SHOT;
	bolt->touch = charge_stick;

	bolt->physicsObject = qtrue;

	bolt->s.genericenemyindex = self->s.number+MAX_GENTITIES;
	//rww - so client prediction knows we own this and won't hit it

	VectorSet( bolt->r.mins, -2, -2, -2 );
	VectorSet( bolt->r.maxs, 2, 2, 2 );

	bolt->health = 1;
	bolt->takedamage = qtrue;
	bolt->pain = DetPackPain;
	bolt->die = DetPackDie;

	bolt->s.weapon = WP_DET_PACK;

	bolt->setTime = level.time;

	G_SetOrigin(bolt, start);
	bolt->s.pos.trType = TR_GRAVITY;
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale(dir, 300, bolt->s.pos.trDelta );
	bolt->s.pos.trTime = level.time;

	bolt->s.apos.trType = TR_GRAVITY;
	bolt->s.apos.trTime = level.time;
	bolt->s.apos.trBase[YAW] = rand()%360;
	bolt->s.apos.trBase[PITCH] = rand()%360;
	bolt->s.apos.trBase[ROLL] = rand()%360;

	if (rand()%10 < 5)
	{
		bolt->s.apos.trBase[YAW] = -bolt->s.apos.trBase[YAW];
	}

	vectoangles(dir, bolt->s.angles);
	VectorCopy(bolt->s.angles, bolt->s.apos.trBase);
	VectorSet(bolt->s.apos.trDelta, 300, 0, 0 );
	bolt->s.apos.trTime = level.time;

	trap_LinkEntity(bolt);

	// boost: auto detkill if close enemy and no close allies
	if (self->client && self->client->account && self->client->account->flags & ACCOUNTFLAG_BOOST_AIMBOTBOOST && g_boost.integer && !HasFlag(self) && !IsRacerOrSpectator(self)) {
		float closestAllyDist = 999999, closestEnemyDist = 999999;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (thisEnt == self || !thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED || IsRacerOrSpectator(thisEnt) || thisEnt->client->ps.fallingToDeath)
				continue;
			if (thisEnt->health <= 0 || thisEnt->client->ps.pm_type == PM_SPECTATOR || thisEnt->client->tempSpectate >= level.time)
				continue; // this guy is dead

			float dist = Distance(self->client->ps.origin, thisEnt->client->ps.origin);
			if (thisEnt->client->sess.sessionTeam == self->client->sess.sessionTeam) {
				if (dist < closestAllyDist)
					closestAllyDist = dist;
			}
			else if (thisEnt->client->sess.sessionTeam == OtherTeam(self->client->sess.sessionTeam)) {
				if (dist < closestEnemyDist)
					closestEnemyDist = dist;
			}
		}

		if (closestEnemyDist <= 200.0f && closestAllyDist >= 300.0f)
			self->client->forceSelfkillTime = level.time + 1;
	}
}

void BlowDetpacks(gentity_t *ent)
{
	gentity_t *found = NULL;

	if ( ent->client->ps.hasDetPackPlanted )
	{
		while ( (found = G_Find( found, FOFS(classname), "detpack") ) != NULL )
		{//loop through all ents and blow the crap out of them!
			if ( found->parent == ent )
			{
				VectorCopy( found->r.currentOrigin, found->s.origin );
				found->think = DetPackBlow;
				found->nextthink = level.time + 100 + random() * 200; //TEST, BLOW BETWEEN THIS AND NEXT FRAME, MUHAHA

				G_Sound( found, CHAN_BODY, G_SoundIndex("sound/weapons/detpack/warning.wav") );
			}
		}
		ent->client->ps.hasDetPackPlanted = qfalse;
	}
}

void RemoveDetpacks(gentity_t *ent)
{
	gentity_t *found = NULL;

	 if ( ent->client->ps.hasDetPackPlanted )
	{
		while ( (found = G_Find( found, FOFS(classname), "detpack") ) != NULL )
		{//loop through all ents and blow the crap out of them!
			if ( found->parent == ent )
			{
				VectorCopy( found->r.currentOrigin, found->s.origin );
				found->think = G_FreeEntity;
				found->nextthink = level.time;
			}
		}
		ent->client->ps.hasDetPackPlanted = qfalse;
	}
}

qboolean CheatsOn(void) 
{
	if ( !g_cheats.integer )
	{
		return qfalse;
	}
	return qtrue;
}

void WP_DropDetPack( gentity_t *ent, qboolean alt_fire )
{
	gentity_t	*found = NULL;
	int			trapcount = 0;
	int			foundDetPacks[MAX_GENTITIES] = {ENTITYNUM_NONE};
	int			trapcount_org;
	int			lowestTimeStamp;
	int			removeMe;
	int			i;

	if ( !ent || !ent->client )
	{
		return;
	}

	if (!alt_fire) { //Only check to remove our 11th oldest detpack when we are placing one
	//limit to 10 placed at any one time
	//see how many there are now
	while ( (found = G_Find( found, FOFS(classname), "detpack" )) != NULL )
	{
		if ( found->parent != ent )
		{
			continue;
		}
		foundDetPacks[trapcount++] = found->s.number;
	}
	//now remove first ones we find until there are only 9 left
	found = NULL;
	trapcount_org = trapcount;
	lowestTimeStamp = level.time;
	while ( trapcount > 9 )
	{
		removeMe = -1;
		for ( i = 0; i < trapcount_org; i++ )
		{
			if ( foundDetPacks[i] == ENTITYNUM_NONE )
			{
				continue;
			}
			found = &g_entities[foundDetPacks[i]];
			if ( found->setTime < lowestTimeStamp )
			{
				removeMe = i;
				lowestTimeStamp = found->setTime;
			}
		}
		if ( removeMe != -1 )
		{
			//remove it... or blow it?
			if ( &g_entities[foundDetPacks[removeMe]] == NULL )
			{
				break;
			}
			else
			{
				if (!CheatsOn())
				{ //Let them have unlimited if cheats are enabled
					G_FreeEntity( &g_entities[foundDetPacks[removeMe]] );
				}
			}
			foundDetPacks[removeMe] = ENTITYNUM_NONE;
			trapcount--;
		}
		else
		{
			break;
		}
	}
	}

	if ( alt_fire  )
	{
		BlowDetpacks(ent);
	}
	else
	{
		AngleVectors( ent->client->ps.viewangles, forward, vright, up );

		CalcMuzzlePoint( ent, forward, vright, up, muzzle );

		VectorNormalize( forward );
		VectorMA( muzzle, -4, forward, muzzle );
		drop_charge( ent, muzzle, forward );

		ent->client->ps.hasDetPackPlanted = qtrue;
	}
}

#ifdef _WIN32
#pragma warning(disable : 4701) //local variable may be used without having been initialized
#endif

static void WP_FireConcussionAlt( gentity_t *ent )
{//a rail-gun-like beam
	int			damage = CONC_ALT_DAMAGE, skip, traces = DISRUPTOR_ALT_TRACES;
	qboolean	render_impact = qtrue;
	vec3_t		start, end;
	vec3_t		/*muzzle2,*/ dir;
	trace_t		tr;
	gentity_t	*traceEnt, *tent;
	float		shotRange = 8192.0f;
	qboolean	hitDodged = qfalse;
	vec3_t shot_mins, shot_maxs;
	int			i;
	int			deathTorsoAnim[16] = { 0 }, deathLegsAnim[16] = { 0 }, deathClientNum[16] = { -1 }, numKilled = 0;

	if (ent->client->sess.inRacemode) {
		// make tracing work temporarily; we will switch them back before returning
		ent->r.svFlags |= SVF_COOLKIDSCLUB;
		ent->r.svFlags &= ~SVF_GHOST;

		if (ent->aimPracticeEntBeingUsed) {
			ent->r.singleEntityCollision = qtrue;
			ent->r.singleEntityThatCanCollide = ent->aimPracticeEntBeingUsed - g_entities;
		}
	}

	// count this as weapon usage out of G_Damage because of the knockback
	ent->client->usedWeapon = qtrue;

	//Shove us backwards for half a second
	VectorMA( ent->client->ps.velocity, -200, forward, ent->client->ps.velocity );
	ent->client->ps.groundEntityNum = ENTITYNUM_NONE;
	if ( (ent->client->ps.pm_flags&PMF_DUCKED) )
	{//hunkered down
		ent->client->ps.pm_time = 100;
	}
	else
	{
		ent->client->ps.pm_time = 250;
	}

	VectorCopy( muzzle, start );
	WP_TraceSetStart( ent, start, vec3_origin, vec3_origin );

	skip = ent->s.number;

	//Make it a little easier to hit guys at long range
	VectorSet( shot_mins, -1, -1, -1 );
	VectorSet( shot_maxs, 1, 1, 1 );

	qboolean compensate = ent && ent->client ? ent->client->sess.unlagged : qfalse;
	qboolean ghoul2 = !!(d_projectileGhoul2Collision.integer);
	if (g_unlagged.integer && compensate)
		G_TimeShiftAllClients(trap_Milliseconds() - (level.time - ent->client->pers.cmd.serverTime), ent, ghoul2);

	for ( i = 0; i < traces; i++ )
	{
		VectorMA( start, shotRange, forward, end );

		//NOTE: if you want to be able to hit guys in emplaced guns, use "G2_COLLIDE, 10" instead of "G2_RETURNONHIT, 0"
		//alternately, if you end up hitting an emplaced_gun that has a sitter, just redo this one trace with the "G2_COLLIDE, 10" to see if we it the sitter
		if (d_projectileGhoul2Collision.integer)
		{
			trap_G2Trace( &tr, start, shot_mins, shot_maxs, end, skip, MASK_SHOT, G2TRFLAG_DOGHOULTRACE|G2TRFLAG_GETSURFINDEX|G2TRFLAG_HITCORPSES, g_g2TraceLod.integer );
		}
		else
		{
			trap_Trace( &tr, start, shot_mins, shot_maxs, end, skip, MASK_SHOT );
		}

		traceEnt = &g_entities[tr.entityNum];

		if (d_projectileGhoul2Collision.integer && traceEnt->inuse && traceEnt->client)
		{ //g2 collision checks -rww
			if (traceEnt->inuse && traceEnt->client && traceEnt->ghoul2)
			{ //since we used G2TRFLAG_GETSURFINDEX, tr.surfaceFlags will actually contain the index of the surface on the ghoul2 model we collided with.
				traceEnt->client->g2LastSurfaceHit = tr.surfaceFlags;
				traceEnt->client->g2LastSurfaceTime = level.time;
			}

			if (traceEnt->ghoul2)
			{
				tr.surfaceFlags = 0; //clear the surface flags after, since we actually care about them in here.
			}
		}
		if ( tr.surfaceFlags & SURF_NOIMPACT ) 
		{
			render_impact = qfalse;
		}

		if ( tr.entityNum == ent->s.number )
		{
			// should never happen, but basically we don't want to consider a hit to ourselves?
			// Get ready for an attempt to trace through another person
			VectorCopy( tr.endpos, start );
			skip = tr.entityNum;
#ifdef _DEBUG
			Com_Printf( "BAD! Concussion gun shot somehow traced back and hit the owner!\n" );			
#endif
			continue;
		}

		if ( tr.fraction >= 1.0f )
		{
			// draw the beam but don't do anything else
			break;
		}

		if ( traceEnt->s.weapon == WP_SABER )//&& traceEnt->NPC 
		{//FIXME: need a more reliable way to know we hit a jedi?
			hitDodged = Jedi_DodgeEvasion( traceEnt, ent, &tr, HL_NONE );
			//acts like we didn't even hit him
		}
		if ( !hitDodged )
		{
			if ( render_impact )
			{
				if (( tr.entityNum < ENTITYNUM_WORLD && traceEnt->takedamage ) 
					|| !Q_stricmp( traceEnt->classname, "misc_model_breakable" ) 
					|| traceEnt->s.eType == ET_MOVER )
				{
					qboolean noKnockBack;

					// Create a simple impact type mark that doesn't last long in the world
					//no no no

					if ( traceEnt->client && LogAccuracyHit( traceEnt, ent )) 
					{//NOTE: hitting multiple ents can still get you over 100% accuracy
						ent->client->stats->accuracy_hits++;
						ent->client->stats->accuracy_hitsOfType[ACC_CONCUSSION_ALT]++;
					} 

					int preHealth = traceEnt->health;
					noKnockBack = (traceEnt->flags&FL_NO_KNOCKBACK);//will be set if they die, I want to know if it was on *before* they died
					if ( traceEnt && traceEnt->client && traceEnt->client->NPC_class == CLASS_GALAKMECH )
					{//hehe
						G_Damage( traceEnt, ent, ent, forward, tr.endpos, 10, DAMAGE_NO_KNOCKBACK|DAMAGE_NO_HIT_LOC, MOD_CONC_ALT );
						break;
					}
					G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, DAMAGE_NO_KNOCKBACK|DAMAGE_NO_HIT_LOC, MOD_CONC_ALT );

					// if the shot killed him, save the death animation so that we can use it after unshifting
					if (g_unlagged.integer && compensate && preHealth > 0 && traceEnt->health <= 0 && traceEnt->client && numKilled < 16) {
						deathTorsoAnim[numKilled] = traceEnt->client->ps.torsoAnim;
						deathLegsAnim[numKilled] = traceEnt->client->ps.legsAnim;
						deathClientNum[numKilled] = traceEnt - g_entities;
						numKilled++;
					}

					if (traceEnt->isAimPracticePack) {
						if (ent->aimPracticeEntBeingUsed == traceEnt && ent->numAimPracticeSpawns >= 1) {
							++ent->numTotalAimPracticeHits;
							++ent->numAimPracticeHitsOfWeapon[WP_CONCUSSION];
						}
						PlayAimPracticeBotPainSound(traceEnt, ent);
					}

					//do knockback and knockdown manually
					if ( traceEnt->client )
					{//only if we hit a client
						vec3_t pushDir;
						VectorCopy( forward, pushDir );
						if ( pushDir[2] < 0.2f )
						{
							pushDir[2] = 0.2f;
						}//hmm, re-normalize?  nah...
						if ( traceEnt->health > 0 )
						{//alive
							//if ( G_HasKnockdownAnims( traceEnt ) )
							if (!noKnockBack && !traceEnt->localAnimIndex && traceEnt->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN &&
								BG_KnockDownable(&traceEnt->client->ps)) //just check for humanoids..
							{//knock-downable
								vec3_t plPDif;
								float pStr;

								//cap it and stuff, base the strength and whether or not we can knockdown on the distance
								//from the shooter to the target
								VectorSubtract(traceEnt->client->ps.origin, ent->client->ps.origin, plPDif);
								pStr = 500.0f-VectorLength(plPDif);
								if (pStr < 150.0f)
								{
									pStr = 150.0f;
								}
								if (pStr > 200.0f)
								{
									traceEnt->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
									traceEnt->client->ps.forceHandExtendTime = level.time + 1100;
									traceEnt->client->ps.forceDodgeAnim = 0; //this toggles between 1 and 0, when it's 1 we should play the get up anim
								}
								traceEnt->client->ps.otherKiller = ent->s.number;
								traceEnt->client->ps.otherKillerTime = level.time + 5000;
								traceEnt->client->ps.otherKillerDebounceTime = level.time + 100;

								traceEnt->client->ps.velocity[0] += pushDir[0]*pStr;
								traceEnt->client->ps.velocity[1] += pushDir[1]*pStr;
								traceEnt->client->ps.velocity[2] = pStr;
							}
						}
					}

					if ( traceEnt->s.eType == ET_MOVER )
					{//stop the traces on any mover
						break;
					}
				}
				else 
				{
					//mmm..no..don't do this more than once for no reason whatsoever.
					break; // hit solid, but doesn't take damage, so stop the shot...we _could_ allow it to shoot through walls, might be cool?
				}
			}
			else // not rendering impact, must be a skybox or other similar thing?
			{
				break; // don't try anymore traces
			}
		}
		// Get ready for an attempt to trace through another person
		VectorCopy( tr.endpos, start );
		skip = tr.entityNum;
		hitDodged = qfalse;
	}

	if (g_unlagged.integer && compensate) {
		G_UnTimeShiftAllClients(ent, ghoul2);

		// restore death animations if this killed someone
		for (int i = 0; i < numKilled && i < 16; i++) {
			gentity_t *deadEnt = &g_entities[deathClientNum[i]];
			if (!deadEnt->inuse || !deadEnt->client)
				continue;
			deadEnt->client->ps.torsoAnim = deathTorsoAnim[i];
			deadEnt->client->ps.legsAnim = deathLegsAnim[i];
		}
	}

	// now go along the trail and make sight events
	VectorSubtract( tr.endpos, muzzle, dir );

	//let's pack all this junk into a single tempent, and send it off.
	tent = G_TempEntity(tr.endpos, EV_CONC_ALT_IMPACT);
	tent->s.eventParm = DirToByte(tr.plane.normal);
	tent->s.owner = ent->s.number;
	G_ApplyRaceBroadcastsToEvent( ent, tent );
	VectorCopy(dir, tent->s.angles);
	VectorCopy(muzzle, tent->s.origin2);
	VectorCopy(forward, tent->s.angles2);

#if 0 //yuck
	//FIXME: if shoot *really* close to someone, the alert could be way out of their FOV
	for ( dist = 0; dist < shotDist; dist += 64 )
	{
		//FIXME: on a really long shot, this could make a LOT of alerts in one frame...
		VectorMA( muzzle, dist, dir, spot );
		AddSightEvent( ent, spot, 256, AEL_DISCOVERED, 50 );
		//FIXME: creates *way* too many effects, make it one effect somehow?
		G_PlayEffectID( G_EffectIndex( "concussion/alt_ring" ), spot, actualAngles );
	}
	//FIXME: spawn a temp ent that continuously spawns sight alerts here?  And 1 sound alert to draw their attention?
	VectorMA( start, shotDist-4, forward, spot );
	AddSightEvent( ent, spot, 256, AEL_DISCOVERED, 50 );

	G_PlayEffectID( G_EffectIndex( "concussion/altmuzzle_flash" ), muzzle, forward );
#endif

	if (ent->client->sess.inRacemode) {
		// restore svFlags from before
		ent->r.svFlags &= ~SVF_COOLKIDSCLUB;
		ent->r.svFlags |= SVF_GHOST;

		ent->r.singleEntityCollision = qfalse;
		ent->r.singleEntityThatCanCollide = 0;
	}
}

#ifdef _WIN32
#pragma warning(default : 4701) //local variable may be used without having been initialized
#endif

static void WP_FireConcussion( gentity_t *ent )
{//a fast rocket-like projectile
	vec3_t	start;
	int		damage	= CONC_DAMAGE;
	float	vel = CONC_VELOCITY;
	gentity_t *missile;

	VectorCopy( muzzle, start );
	WP_TraceSetStart( ent, start, vec3_origin, vec3_origin );//make sure our start point isn't on the other side of a wall

	if (!CorrectBoostedAim(ent, muzzle, forward, vel, WP_CONCUSSION, qfalse, ROCKET_SIZE))
		return;
	missile = CreateMissile( start, forward, vel, 10000, ent, qfalse );

	missile->classname = "conc_proj";
	missile->s.weapon = WP_CONCUSSION;
	missile->mass = 10;

	// Make it easier to hit things
	VectorSet( missile->r.maxs, ROCKET_SIZE, ROCKET_SIZE, ROCKET_SIZE );
	VectorScale( missile->r.maxs, -1, missile->r.mins );

	missile->damage = damage;
	missile->dflags = DAMAGE_EXTRA_KNOCKBACK;

	missile->methodOfDeath = MOD_CONC;
	missile->splashMethodOfDeath = MOD_CONC;

	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;
	missile->splashDamage = CONC_SPLASH_DAMAGE;
	missile->splashRadius = CONC_SPLASH_RADIUS;

	// we don't want it to ever bounce
	missile->bounceCount = 0;
}


//---------------------------------------------------------
// FireStunBaton
//---------------------------------------------------------
void WP_FireStunBaton( gentity_t *ent, qboolean alt_fire )
{
	gentity_t	*tr_ent;
	trace_t		tr;
	vec3_t		mins, maxs, end;
	vec3_t		muzzleStun;

	if (!ent->client)
	{
		VectorCopy(ent->r.currentOrigin, muzzleStun);
		muzzleStun[2] += 8;
	}
	else
	{
		VectorCopy(ent->client->ps.origin, muzzleStun);
		muzzleStun[2] += ent->client->ps.viewheight-6;
	}

	VectorMA(muzzleStun, 20.0f, forward, muzzleStun);
	VectorMA(muzzleStun, 4.0f, vright, muzzleStun);

	VectorMA( muzzleStun, STUN_BATON_RANGE, forward, end );

	VectorSet( maxs, 6, 6, 6 );
	VectorScale( maxs, -1, mins );

	qboolean compensate = ent->client->sess.unlagged;
	if (g_unlagged.integer && compensate)
		G_TimeShiftAllClients(trap_Milliseconds() - (level.time - ent->client->pers.cmd.serverTime), ent, qfalse);

	trap_Trace ( &tr, muzzleStun, mins, maxs, end, ent->s.number, MASK_SHOT );

	if (g_unlagged.integer && compensate)
		G_UnTimeShiftAllClients(ent, qfalse);

	if ( tr.entityNum >= ENTITYNUM_WORLD )
	{
		return;
	}

	tr_ent = &g_entities[tr.entityNum];

	if (tr_ent && tr_ent->takedamage && tr_ent->client)
	{ //see if either party is involved in a duel
		if (tr_ent->client->ps.duelInProgress &&
			tr_ent->client->ps.duelIndex != ent->s.number)
		{
			return;
		}

		if (ent->client &&
			ent->client->ps.duelInProgress &&
			ent->client->ps.duelIndex != tr_ent->s.number)
		{
			return;
		}
	}

	if ( tr_ent && tr_ent->takedamage )
	{
		gentity_t *te = G_PlayEffect( EFFECT_STUNHIT, tr.endpos, tr.plane.normal );
		G_ApplyRaceBroadcastsToEvent( ent, te );

		G_Sound( tr_ent, CHAN_WEAPON, G_SoundIndex( va("sound/weapons/melee/punch%d", Q_irand(1, 4)) ) );
		G_Damage( tr_ent, ent, ent, forward, tr.endpos, STUN_BATON_DAMAGE, (DAMAGE_NO_KNOCKBACK|DAMAGE_HALF_ABSORB), MOD_STUN_BATON );

		if (tr_ent->client)
		{ //if it's a player then use the shock effect
			if ( tr_ent->client->NPC_class == CLASS_VEHICLE )
			{//not on vehicles
				if ( !tr_ent->m_pVehicle
					|| tr_ent->m_pVehicle->m_pVehicleInfo->type == VH_ANIMAL 
					|| tr_ent->m_pVehicle->m_pVehicleInfo->type == VH_FLIER )
				{//can zap animals
					tr_ent->client->ps.electrifyTime = level.time + Q_irand( 3000, 4000 );
				}
			}
			else
			{
				tr_ent->client->ps.electrifyTime = level.time + 700;
			}
		}
	}
}


//---------------------------------------------------------
// FireMelee
//---------------------------------------------------------
void WP_FireMelee( gentity_t *ent, qboolean alt_fire )
{
	gentity_t	*tr_ent;
	trace_t		tr;
	vec3_t		mins, maxs, end;
	vec3_t		muzzlePunch;

	if (ent->client && ent->client->ps.torsoAnim == BOTH_MELEE2)
	{ //right
		if (ent->client->ps.brokenLimbs & (1 << BROKENLIMB_RARM))
		{
			return;
		}
	}
	else
	{ //left
		if (ent->client->ps.brokenLimbs & (1 << BROKENLIMB_LARM))
		{
			return;
		}
	}

	if (!ent->client)
	{
		VectorCopy(ent->r.currentOrigin, muzzlePunch);
		muzzlePunch[2] += 8;
	}
	else
	{
		VectorCopy(ent->client->ps.origin, muzzlePunch);
		muzzlePunch[2] += ent->client->ps.viewheight-6;
	}

	VectorMA(muzzlePunch, 20.0f, forward, muzzlePunch);
	VectorMA(muzzlePunch, 4.0f, vright, muzzlePunch);

	VectorMA( muzzlePunch, MELEE_RANGE, forward, end );

	VectorSet( maxs, 6, 6, 6 );
	VectorScale( maxs, -1, mins );

	qboolean compensate = ent->client->sess.unlagged;
	if (g_unlagged.integer && compensate)
		G_TimeShiftAllClients(trap_Milliseconds() - (level.time - ent->client->pers.cmd.serverTime), ent, qfalse);

	trap_Trace ( &tr, muzzlePunch, mins, maxs, end, ent->s.number, MASK_SHOT );

	if (g_unlagged.integer && compensate)
		G_UnTimeShiftAllClients(ent, qfalse);

	if (tr.entityNum != ENTITYNUM_NONE)
	{ //hit something
		tr_ent = &g_entities[tr.entityNum];

		G_Sound( ent, CHAN_AUTO, G_SoundIndex( va("sound/weapons/melee/punch%d", Q_irand(1, 4)) ) );

		if (tr_ent->takedamage && tr_ent->client)
		{ //special duel checks
			if (tr_ent->client->ps.duelInProgress &&
				tr_ent->client->ps.duelIndex != ent->s.number)
			{
				return;
			}

			if (ent->client &&
				ent->client->ps.duelInProgress &&
				ent->client->ps.duelIndex != tr_ent->s.number)
			{
				return;
			}
		}

		if ( tr_ent->takedamage )
		{ //damage them, do more damage if we're in the second right hook
			int dmg = MELEE_SWING1_DAMAGE;

			if (ent->client && ent->client->ps.torsoAnim == BOTH_MELEE2)
			{ //do a tad bit more damage on the second swing
				dmg = MELEE_SWING2_DAMAGE;
			}

			if ( G_HeavyMelee( ent ) )
			{ //2x damage for heavy melee class
				dmg *= 2;
			}

			G_Damage( tr_ent, ent, ent, forward, tr.endpos, dmg, DAMAGE_NO_ARMOR, MOD_MELEE );
		}
	}
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


/*
======================
SnapVectorTowards

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating 
into a wall.
======================
*/
void SnapVectorTowards( vec3_t v, vec3_t to ) {
	int		i;

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( to[i] <= v[i] ) {
			v[i] = (int)v[i];
		} else {
			v[i] = (int)v[i] + 1;
		}
	}
}


//======================================================================


/*
===============
LogAccuracyHit
===============
*/
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker ) {
	if( !target->takedamage ) {
		return qfalse;
	}

	if ( target == attacker ) {
		return qfalse;
	}

	if( !target->client ) {
		return qfalse;
	}

	if (!attacker)
	{
		return qfalse;
	}

	if( !attacker->client ) {
		return qfalse;
	}

	if( target->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return qfalse;
	}

	if ( OnSameTeam( target, attacker ) ) {
		return qfalse;
	}

	if (target->client->sess.inRacemode || attacker->client->sess.inRacemode) {
		return qfalse;
	}

	return qtrue;
}


/*
===============
CalcMuzzlePoint

set muzzle location relative to pivoting eye
rwwFIXMEFIXME: Since ghoul2 models are on server and properly updated now,
it may be reasonable to base muzzle point off actual weapon bolt point.
The down side would be that it does not necessarily look alright from a
first person perspective.
===============
*/
void CalcMuzzlePoint ( gentity_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint ) 
{
	int weapontype;
	vec3_t muzzleOffPoint;

	weapontype = ent->s.weapon;
	VectorCopy( ent->s.pos.trBase, muzzlePoint );

	VectorCopy(WP_MuzzlePoint[weapontype], muzzleOffPoint);

	if (weapontype > WP_NONE && weapontype < WP_NUM_WEAPONS)
	{	// Use the table to generate the muzzlepoint;
		{	// Crouching.  Use the add-to-Z method to adjust vertically.
			VectorMA(muzzlePoint, muzzleOffPoint[0], forward, muzzlePoint);
			VectorMA(muzzlePoint, muzzleOffPoint[1], right, muzzlePoint);
			muzzlePoint[2] += ent->client->ps.viewheight + muzzleOffPoint[2];
		}
	}

	// snap to integer coordinates for more efficient network bandwidth usage
	SnapVector( muzzlePoint );
}

/*
===============
CalcMuzzlePointOrigin

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePointOrigin ( gentity_t *ent, vec3_t origin, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint ) {
	VectorCopy( ent->s.pos.trBase, muzzlePoint );
	muzzlePoint[2] += ent->client->ps.viewheight;
	VectorMA( muzzlePoint, 14, forward, muzzlePoint );
	// snap to integer coordinates for more efficient network bandwidth usage
	SnapVector( muzzlePoint );
}

extern void G_MissileImpact( gentity_t *ent, trace_t *trace );
void WP_TouchVehMissile( gentity_t *ent, gentity_t *other, trace_t *trace )
{
	trace_t	myTrace;
	memcpy( (void *)&myTrace, (void *)trace, sizeof(myTrace) );
	if ( other )
	{
		myTrace.entityNum = other->s.number;
	}
	G_MissileImpact( ent, &myTrace );
}

extern qboolean CheckAccuracyAndAirshot(gentity_t *missile, gentity_t *victim, qboolean isSurfedRocket);
void WP_TouchRocket( gentity_t *ent, gentity_t *other, trace_t *trace )
{
	if ( ent && ent->s.eType == ET_MISSILE && ent->die )
	{
		CheckAccuracyAndAirshot(ent, other, qtrue);
		ent->die( ent, NULL, NULL, ROCKET_DAMAGE, MOD_ROCKET_HOMING );
	}
}	 

void WP_CalcVehMuzzle(gentity_t *ent, int muzzleNum)
{
	Vehicle_t *pVeh = ent->m_pVehicle;
	mdxaBone_t boltMatrix;
	vec3_t	vehAngles;

	assert(pVeh);

	if (pVeh->m_iMuzzleTime[muzzleNum] == level.time)
	{ //already done for this frame, don't need to do it again
		return;
	}
	//Uh... how about we set this, hunh...?  :)
	pVeh->m_iMuzzleTime[muzzleNum] = level.time;
	
	VectorCopy( ent->client->ps.viewangles, vehAngles );
	if ( pVeh->m_pVehicleInfo
		&& (pVeh->m_pVehicleInfo->type == VH_ANIMAL
			 ||pVeh->m_pVehicleInfo->type == VH_WALKER
			 ||pVeh->m_pVehicleInfo->type == VH_SPEEDER) )
	{
		vehAngles[PITCH] = vehAngles[ROLL] = 0;
	}

	trap_G2API_GetBoltMatrix_NoRecNoRot(ent->ghoul2, 0, pVeh->m_iMuzzleTag[muzzleNum], &boltMatrix, vehAngles,
		ent->client->ps.origin, level.time, NULL, ent->modelScale);
	BG_GiveMeVectorFromMatrix(&boltMatrix, ORIGIN, pVeh->m_vMuzzlePos[muzzleNum]);
	BG_GiveMeVectorFromMatrix(&boltMatrix, NEGATIVE_Y, pVeh->m_vMuzzleDir[muzzleNum]);
}

void WP_VehWeapSetSolidToOwner( gentity_t *self )
{
	self->r.svFlags |= SVF_OWNERNOTSHARED;
	if ( self->genericValue1 )
	{//expire after a time
		if ( self->genericValue2 )
		{//blow up when your lifetime is up
			self->think = G_ExplodeMissile;//FIXME: custom func?
		}
		else
		{//just remove yourself
			self->think = G_FreeEntity;//FIXME: custom func?
		}
		self->nextthink = level.time + self->genericValue1;
	}
}

#define VEH_HOMING_MISSILE_THINK_TIME		100
gentity_t *WP_FireVehicleWeapon( gentity_t *ent, vec3_t start, vec3_t dir, vehWeaponInfo_t *vehWeapon, qboolean alt_fire, qboolean isTurretWeap )
{
	gentity_t	*missile = NULL;

	//FIXME: add some randomness...?  Inherent inaccuracy stat of weapon?  Pilot skill?
	if ( !vehWeapon )
	{//invalid vehicle weapon
		return NULL;
	}
	else if ( vehWeapon->bIsProjectile )
	{//projectile entity
		vec3_t		mins, maxs;

		VectorSet( maxs, vehWeapon->fWidth/2.0f,vehWeapon->fWidth/2.0f,vehWeapon->fHeight/2.0f );
		VectorScale( maxs, -1, mins );

		//make sure our start point isn't on the other side of a wall
		WP_TraceSetStart( ent, start, mins, maxs );
		
		//FIXME: CUSTOM MODEL?
		//QUERY: alt_fire true or not?  Does it matter?
		missile = CreateMissile( start, dir, vehWeapon->fSpeed, 10000, ent, qfalse );

		missile->classname = "vehicle_proj";
		
		missile->s.genericenemyindex = ent->s.number+MAX_GENTITIES;
		missile->damage = vehWeapon->iDamage;
		missile->splashDamage = vehWeapon->iSplashDamage;
		missile->splashRadius = vehWeapon->fSplashRadius;

		//FIXME: externalize some of these properties?
		missile->dflags = DAMAGE_DEATH_KNOCKBACK;
		missile->clipmask = MASK_SHOT;
		//Maybe by checking flags...?
		if ( vehWeapon->bSaberBlockable )
		{
			missile->clipmask |= CONTENTS_LIGHTSABER;
		}
		// Make it easier to hit things
		VectorCopy( mins, missile->r.mins );
		VectorCopy( maxs, missile->r.maxs );
		//some slightly different stuff for things with bboxes
		if ( vehWeapon->fWidth || vehWeapon->fHeight )
		{//we assume it's a rocket-like thing
			missile->s.weapon = WP_ROCKET_LAUNCHER;//does this really matter?
			missile->methodOfDeath = MOD_VEHICLE;
			missile->splashMethodOfDeath = MOD_VEHICLE;

			// we don't want it to ever bounce
			missile->bounceCount = 0;

			missile->mass = 10;
		}
		else
		{//a blaster-laser-like thing
			missile->s.weapon = WP_BLASTER;//does this really matter?
			missile->methodOfDeath = MOD_VEHICLE; //count as a heavy weap
			missile->splashMethodOfDeath = MOD_VEHICLE;// ?SPLASH;
			// we don't want it to bounce forever
			missile->bounceCount = 8;
		}
		
		if ( vehWeapon->bHasGravity )
		{//TESTME: is this all we need to do?
			missile->s.weapon = WP_THERMAL;//does this really matter?
			missile->s.pos.trType = TR_GRAVITY;
		}
		
		if ( vehWeapon->bIonWeapon )
		{//so it disables ship shields and sends them out of control
			missile->s.weapon = WP_DEMP2;
		}

		if ( vehWeapon->iHealth )
		{//the missile can take damage
			missile->health = vehWeapon->iHealth;
			missile->takedamage = qtrue;
			missile->r.contents = MASK_SHOT;
			missile->die = RocketDie;
		}

		//pilot should own this projectile on server if we have a pilot
		if (ent->m_pVehicle && ent->m_pVehicle->m_pPilot)
		{//owned by vehicle pilot
			missile->r.ownerNum = ent->m_pVehicle->m_pPilot->s.number;
		}
		else
		{//owned by vehicle?
			missile->r.ownerNum = ent->s.number;
		}

		//set veh as cgame side owner for purpose of fx overrides
		missile->s.owner = ent->s.number;
		if ( alt_fire )
		{//use the second weapon's iShotFX
			missile->s.eFlags |= EF_ALT_FIRING;
		}
		if ( isTurretWeap )
		{//look for the turret weapon info on cgame side, not vehicle weapon info
			missile->s.weapon = WP_TURRET;
		}
		if ( vehWeapon->iLifeTime )
		{//expire after a time
			if ( vehWeapon->bExplodeOnExpire )
			{//blow up when your lifetime is up
				missile->think = G_ExplodeMissile;//FIXME: custom func?
			}
			else
			{//just remove yourself
				missile->think = G_FreeEntity;//FIXME: custom func?
			}
			missile->nextthink = level.time + vehWeapon->iLifeTime;
		}
		missile->s.otherEntityNum2 = (vehWeapon-&g_vehWeaponInfo[0]);
		missile->s.eFlags |= EF_JETPACK_ACTIVE;
		//homing
		if ( vehWeapon->fHoming )
		{//homing missile
			if ( ent->client && ent->client->ps.rocketLockIndex != ENTITYNUM_NONE )
			{
				int dif = 0;
				float rTime;
				rTime = ent->client->ps.rocketLockTime;

				if (rTime == -1)
				{
					rTime = ent->client->ps.rocketLastValidTime;
				}

				if ( !vehWeapon->iLockOnTime )
				{//no minimum lock-on time
					dif = 10;//guaranteed lock-on
				}
				else
				{
					float lockTimeInterval = vehWeapon->iLockOnTime/16.0f;
					dif = ( level.time - rTime ) / lockTimeInterval;
				}

				if (dif < 0)
				{
					dif = 0;
				}

				//It's 10 even though it locks client-side at 8, because we want them to have a sturdy lock first, and because there's a slight difference in time between server and client
				if ( dif >= 10 && rTime != -1 )
				{
					missile->enemy = &g_entities[ent->client->ps.rocketLockIndex];

					if (missile->enemy && missile->enemy->client && missile->enemy->health > 0 && !OnSameTeam(ent, missile->enemy))
					{ //if enemy became invalid, died, or is on the same team, then don't seek it
						missile->spawnflags |= 1;//just to let it know it should be faster...
						missile->speed = vehWeapon->fSpeed;
						missile->angle = vehWeapon->fHoming;
						missile->radius = vehWeapon->fHomingFOV;
						//crap, if we have a lifetime, need to store that somewhere else on ent and have rocketThink func check it every frame...
						if ( vehWeapon->iLifeTime )
						{//expire after a time
							missile->genericValue1 = level.time + vehWeapon->iLifeTime;
							missile->genericValue2 = (int)(vehWeapon->bExplodeOnExpire);
						}
						//now go ahead and use the rocketThink func
						missile->think = rocketThink;//FIXME: custom func?
						missile->nextthink = level.time + VEH_HOMING_MISSILE_THINK_TIME;
						missile->s.eFlags |= EF_RADAROBJECT;//FIXME: externalize
						if ( missile->enemy->s.NPC_class == CLASS_VEHICLE )
						{//let vehicle know we've locked on to them
							missile->s.otherEntityNum = missile->enemy->s.number;
						}
					}
				}

				VectorCopy( dir, missile->movedir );
				missile->random = 1.0f;//FIXME: externalize?
			}
		}
		if ( !vehWeapon->fSpeed )
		{//a mine or something?
			//only do damage when someone touches us
			missile->s.weapon = WP_THERMAL;//does this really matter?
			G_SetOrigin( missile, start );
			missile->touch = WP_TouchVehMissile;
			missile->s.eFlags |= EF_RADAROBJECT;//FIXME: externalize
			//crap, if we have a lifetime, need to store that somewhere else on ent and have rocketThink func check it every frame...
			if ( vehWeapon->iLifeTime )
			{//expire after a time
				missile->genericValue1 = vehWeapon->iLifeTime;
				missile->genericValue2 = (int)(vehWeapon->bExplodeOnExpire);
			}
			//now go ahead and use the setsolidtoowner func
			missile->think = WP_VehWeapSetSolidToOwner;
			missile->nextthink = level.time + 3000;
		}
	}
	else
	{//traceline
		//FIXME: implement
	}

	return missile;
}

//custom routine to not waste tempents horribly -rww
void G_VehMuzzleFireFX( gentity_t *ent, gentity_t *broadcaster, int muzzlesFired )
{
	Vehicle_t *pVeh = ent->m_pVehicle;
	gentity_t *b;

	if (!pVeh)
	{
		return;
	}

	if (!broadcaster)
	{ //oh well. We will WASTE A TEMPENT.
		b = G_TempEntity( ent->client->ps.origin, EV_VEH_FIRE );
		G_ApplyRaceBroadcastsToEvent( ent, b );
	}
	else
	{ //joy
		b = broadcaster;
	}

	//this guy owns it
	b->s.owner = ent->s.number;

	//this is the bitfield of all muzzles fired this time
	//NOTE: just need MAX_VEHICLE_MUZZLES bits for this... should be cool since it's currently 12 and we're sending it in 16 bits
	b->s.trickedentindex = muzzlesFired;

	if ( broadcaster )
	{ //add the event
		G_AddEvent( b, EV_VEH_FIRE, 0 );
	}
}

void G_EstimateCamPos( vec3_t viewAngles, vec3_t cameraFocusLoc, float viewheight, float thirdPersonRange, 
					  float thirdPersonHorzOffset, float vertOffset, float pitchOffset, 
					  int ignoreEntNum, vec3_t camPos )
{
	int		MASK_CAMERACLIP = (MASK_SOLID|CONTENTS_PLAYERCLIP);
	float	CAMERA_SIZE = 4;
	vec3_t	cameramins;
	vec3_t	cameramaxs;
	vec3_t	cameraFocusAngles, camerafwd, cameraup;
	vec3_t	cameraIdealTarget, cameraCurTarget;
	vec3_t	cameraIdealLoc, cameraCurLoc;
	vec3_t	diff;
	vec3_t	camAngles;
	vec3_t	viewaxis[3];
	trace_t	trace;

	VectorSet( cameramins, -CAMERA_SIZE, -CAMERA_SIZE, -CAMERA_SIZE );
	VectorSet( cameramaxs, CAMERA_SIZE, CAMERA_SIZE, CAMERA_SIZE );

	VectorCopy( viewAngles, cameraFocusAngles );
	cameraFocusAngles[PITCH] += pitchOffset;
	if ( !bg_fighterAltControl.integer )
	{//clamp view pitch
		cameraFocusAngles[PITCH] = AngleNormalize180( cameraFocusAngles[PITCH] );
		if (cameraFocusAngles[PITCH] > 80.0)
		{
			cameraFocusAngles[PITCH] = 80.0;
		}
		else if (cameraFocusAngles[PITCH] < -80.0)
		{
			cameraFocusAngles[PITCH] = -80.0;
		}
	}
	AngleVectors(cameraFocusAngles, camerafwd, NULL, cameraup);

	cameraFocusLoc[2] += viewheight;

	VectorCopy( cameraFocusLoc, cameraIdealTarget );
	cameraIdealTarget[2] += vertOffset;

	//NOTE: on cgame, this uses the thirdpersontargetdamp value, we ignore that here
	VectorCopy( cameraIdealTarget, cameraCurTarget );
	trap_Trace( &trace, cameraFocusLoc, cameramins, cameramaxs, cameraCurTarget, ignoreEntNum, MASK_CAMERACLIP );
	if (trace.fraction < 1.0)
	{
		VectorCopy(trace.endpos, cameraCurTarget);
	}

	VectorMA(cameraIdealTarget, -(thirdPersonRange), camerafwd, cameraIdealLoc);
	//NOTE: on cgame, this uses the thirdpersoncameradamp value, we ignore that here
	VectorCopy( cameraIdealLoc, cameraCurLoc );
	trap_Trace(&trace, cameraCurTarget, cameramins, cameramaxs, cameraCurLoc, ignoreEntNum, MASK_CAMERACLIP);
	if (trace.fraction < 1.0)
	{
		VectorCopy( trace.endpos, cameraCurLoc );
	}

	VectorSubtract(cameraCurTarget, cameraCurLoc, diff);
	{
		float dist = VectorNormalize(diff);
		//under normal circumstances, should never be 0.00000 and so on.
		if ( !dist || (diff[0] == 0 || diff[1] == 0) )
		{//must be hitting something, need some value to calc angles, so use cam forward
			VectorCopy( camerafwd, diff );
		}
	}

	vectoangles(diff, camAngles);

	if ( thirdPersonHorzOffset != 0.0f )
	{
		AnglesToAxis( camAngles, viewaxis );
		VectorMA( cameraCurLoc, thirdPersonHorzOffset, viewaxis[1], cameraCurLoc );
	}

	VectorCopy(cameraCurLoc, camPos);
}

void WP_GetVehicleCamPos( gentity_t *ent, gentity_t *pilot, vec3_t camPos )
{
	float thirdPersonHorzOffset = ent->m_pVehicle->m_pVehicleInfo->cameraHorzOffset;
	float thirdPersonRange = ent->m_pVehicle->m_pVehicleInfo->cameraRange;
	float pitchOffset = ent->m_pVehicle->m_pVehicleInfo->cameraPitchOffset;
	float vertOffset = ent->m_pVehicle->m_pVehicleInfo->cameraVertOffset;

	if ( ent->client->ps.hackingTime )
	{
		thirdPersonHorzOffset += (((float)ent->client->ps.hackingTime)/MAX_STRAFE_TIME) * -80.0f;
		thirdPersonRange += fabs(((float)ent->client->ps.hackingTime)/MAX_STRAFE_TIME) * 100.0f;
	}

	if ( ent->m_pVehicle->m_pVehicleInfo->cameraPitchDependantVertOffset )
	{
		if ( pilot->client->ps.viewangles[PITCH] > 0 )
		{
			vertOffset = 130+pilot->client->ps.viewangles[PITCH]*-10;
			if ( vertOffset < -170 )
			{
				vertOffset = -170;
			}
		}
		else if ( pilot->client->ps.viewangles[PITCH] < 0 )
		{
			vertOffset = 130+pilot->client->ps.viewangles[PITCH]*-5;
			if ( vertOffset > 130 )
			{
				vertOffset = 130;
			}
		}
		else
		{
			vertOffset = 30;
		}
		if ( pilot->client->ps.viewangles[PITCH] > 0 )
		{
			pitchOffset = pilot->client->ps.viewangles[PITCH]*-0.75;
		}
		else if ( pilot->client->ps.viewangles[PITCH] < 0 )
		{
			pitchOffset = pilot->client->ps.viewangles[PITCH]*-0.75;
		}
		else
		{
			pitchOffset = 0;
		}
	}

	//Control Scheme 3 Method:
	G_EstimateCamPos( ent->client->ps.viewangles, pilot->client->ps.origin, pilot->client->ps.viewheight, thirdPersonRange, 
		thirdPersonHorzOffset, vertOffset, pitchOffset, 
		pilot->s.number, camPos );
}

void WP_VehLeadCrosshairVeh( gentity_t *camTraceEnt, vec3_t newEnd, const vec3_t dir, const vec3_t shotStart, vec3_t shotDir )
{
	if ( camTraceEnt 
		&& camTraceEnt->client
		&& camTraceEnt->client->NPC_class == CLASS_VEHICLE )
	{//if the crosshair is on a vehicle, lead it
		float distAdjust = DotProduct( camTraceEnt->client->ps.velocity, dir );
		VectorMA( newEnd, distAdjust, dir, newEnd );
	}
	VectorSubtract( newEnd, shotStart, shotDir );
	VectorNormalize( shotDir );
}

#define MAX_XHAIR_DIST_ACCURACY	20000.0f
extern int BG_VehTraceFromCamPos( trace_t *camTrace, bgEntity_t *bgEnt, const vec3_t entOrg, const vec3_t shotStart, const vec3_t end, vec3_t newEnd, vec3_t shotDir, float bestDist );
qboolean WP_VehCheckTraceFromCamPos( gentity_t *ent, const vec3_t shotStart, vec3_t shotDir )
{
	//FIXME: only if dynamicCrosshair and dynamicCrosshairPrecision is on!
	if ( !ent 
		|| !ent->m_pVehicle 
		|| !ent->m_pVehicle->m_pVehicleInfo 
		|| !ent->m_pVehicle->m_pPilot//not being driven
		|| !((gentity_t*)ent->m_pVehicle->m_pPilot)->client//not being driven by a client...?!!!
		|| (ent->m_pVehicle->m_pPilot->s.number >= MAX_CLIENTS) )//being driven, but not by a real client, no need to worry about crosshair
	{
		return qfalse;
	}
	if ( (ent->m_pVehicle->m_pVehicleInfo->type == VH_FIGHTER && level.cullDistance > MAX_XHAIR_DIST_ACCURACY )
		|| ent->m_pVehicle->m_pVehicleInfo->type == VH_WALKER)
	{
		//FIRST: simulate the normal crosshair trace from the center of the veh straight forward
		trace_t trace;
		vec3_t	dir, start, end;
		if ( ent->m_pVehicle->m_pVehicleInfo->type == VH_WALKER )
		{//for some reason, the walker always draws the crosshair out from from the first muzzle point
			AngleVectors( ent->client->ps.viewangles, dir, NULL, NULL );
			VectorCopy( ent->r.currentOrigin, start );
			start[2] += ent->m_pVehicle->m_pVehicleInfo->height-DEFAULT_MINS_2-48;
		}
		else
		{
			vec3_t ang;
			if (ent->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER)
			{
				VectorSet(ang, 0.0f, ent->m_pVehicle->m_vOrientation[1], 0.0f);
			}
			else
			{
				VectorCopy(ent->m_pVehicle->m_vOrientation, ang);
			}
			AngleVectors( ang, dir, NULL, NULL );
			VectorCopy( ent->r.currentOrigin, start );
		}
		VectorMA( start, level.cullDistance, dir, end );
		trap_Trace( &trace, start, vec3_origin, vec3_origin, end, 
			ent->s.number, CONTENTS_SOLID|CONTENTS_BODY );

		if ( ent->m_pVehicle->m_pVehicleInfo->type == VH_WALKER )
		{//just use the result of that one trace since walkers don't do the extra trace
			VectorSubtract( trace.endpos, shotStart, shotDir );
			VectorNormalize( shotDir );
			return qtrue;
		}
		else
		{//NOW do the trace from the camPos and compare with above trace
			trace_t	extraTrace;
			vec3_t	newEnd;
			int camTraceEntNum = BG_VehTraceFromCamPos( &extraTrace, (bgEntity_t *)ent, ent->r.currentOrigin, shotStart, end, newEnd, shotDir, (trace.fraction*level.cullDistance) );
			if ( camTraceEntNum )
			{
				WP_VehLeadCrosshairVeh( &g_entities[camTraceEntNum-1], newEnd, dir, shotStart, shotDir );
				return qtrue;
			}
		}
	}
	return qfalse;
}

//---------------------------------------------------------
void FireVehicleWeapon( gentity_t *ent, qboolean alt_fire ) 
//---------------------------------------------------------
{
	Vehicle_t *pVeh = ent->m_pVehicle;
	int muzzlesFired = 0;
	gentity_t *missile = NULL;
	vehWeaponInfo_t *vehWeapon = NULL;
	qboolean	clearRocketLockEntity = qfalse;
	
	if ( !pVeh )
	{
		return;
	}

	if (pVeh->m_iRemovedSurfaces)
	{ //can't fire when the thing is breaking apart
		return;
	}

	if (pVeh->m_pVehicleInfo->type == VH_WALKER &&
		ent->client->ps.electrifyTime > level.time)
	{ //don't fire while being electrocuted
		return;
	}

	// TODO?: If possible (probably not enough time), it would be nice if secondary fire was actually a mode switch/toggle
	// so that, for instance, an x-wing can have 4-gun fire, or individual muzzle fire. If you wanted a different weapon, you
	// would actually have to press the 2 key or something like that (I doubt I'd get a graphic for it anyways though). -AReis

	// If this is not the alternate fire, fire a normal blaster shot...
	if ( pVeh->m_pVehicleInfo &&
		(pVeh->m_pVehicleInfo->type != VH_FIGHTER || (pVeh->m_ulFlags&VEH_WINGSOPEN)) ) // NOTE: Wings open also denotes that it has already launched.
	{//fighters can only fire when wings are open
		int	weaponNum = 0, vehWeaponIndex = VEH_WEAPON_NONE;
		int	delay = 1000;
		qboolean aimCorrect = qfalse;
		qboolean linkedFiring = qfalse;

		if ( !alt_fire )
		{
			weaponNum = 0;
		}
		else
		{
			weaponNum = 1;
		}

		vehWeaponIndex = pVeh->m_pVehicleInfo->weapon[weaponNum].ID;

		if ( pVeh->weaponStatus[weaponNum].ammo <= 0 )
		{//no ammo for this weapon
			if ( pVeh->m_pPilot && pVeh->m_pPilot->s.number < MAX_CLIENTS )
			{// let the client know he's out of ammo
				int i;
				//but only if one of the vehicle muzzles is actually ready to fire this weapon
				for ( i = 0; i < MAX_VEHICLE_MUZZLES; i++ )
				{
					if ( pVeh->m_pVehicleInfo->weapMuzzle[i] != vehWeaponIndex )
					{//this muzzle doesn't match the weapon we're trying to use
						continue;
					}
					if ( pVeh->m_iMuzzleTag[i] != -1 
						&& pVeh->m_iMuzzleWait[i] < level.time )
					{//this one would have fired, send the no ammo message
						G_AddEvent( (gentity_t*)pVeh->m_pPilot, EV_NOAMMO, weaponNum );
						break;
					}
				}
			}
			return;
		}

		delay = pVeh->m_pVehicleInfo->weapon[weaponNum].delay;
		aimCorrect = pVeh->m_pVehicleInfo->weapon[weaponNum].aimCorrect;
		if ( pVeh->m_pVehicleInfo->weapon[weaponNum].linkable == 2//always linked
			|| ( pVeh->m_pVehicleInfo->weapon[weaponNum].linkable == 1//optionally linkable
				 && pVeh->weaponStatus[weaponNum].linked ) )//linked
		{//we're linking the primary or alternate weapons, so we'll do *all* the muzzles
			linkedFiring = qtrue;
		}

		if ( vehWeaponIndex <= VEH_WEAPON_BASE || vehWeaponIndex >= MAX_VEH_WEAPONS )
		{//invalid vehicle weapon
			return;
		}
		else
		{
			int i, numMuzzles = 0, numMuzzlesReady = 0, cumulativeDelay = 0, cumulativeAmmo = 0;
			qboolean sentAmmoWarning = qfalse;

			vehWeapon = &g_vehWeaponInfo[vehWeaponIndex];

			if ( pVeh->m_pVehicleInfo->weapon[weaponNum].linkable == 2 )
			{//always linked weapons don't accumulate delay, just use specified delay
				cumulativeDelay = delay;
			}
			//find out how many we've got for this weapon
			for ( i = 0; i < MAX_VEHICLE_MUZZLES; i++ )
			{
				if ( pVeh->m_pVehicleInfo->weapMuzzle[i] != vehWeaponIndex )
				{//this muzzle doesn't match the weapon we're trying to use
					continue;
				}
				if ( pVeh->m_iMuzzleTag[i] != -1 && pVeh->m_iMuzzleWait[i] < level.time )
				{
					numMuzzlesReady++;
				}
				if ( pVeh->m_pVehicleInfo->weapMuzzle[pVeh->weaponStatus[weaponNum].nextMuzzle] != vehWeaponIndex )
				{//Our designated next muzzle for this weapon isn't valid for this weapon (happens when ships fire for the first time)
					//set the next to this one
					pVeh->weaponStatus[weaponNum].nextMuzzle = i;
				}
				if ( linkedFiring )
				{
					cumulativeAmmo += vehWeapon->iAmmoPerShot;
					if ( pVeh->m_pVehicleInfo->weapon[weaponNum].linkable != 2 )
					{//always linked weapons don't accumulate delay, just use specified delay
						cumulativeDelay += delay;
					}
				}
				numMuzzles++;
			}

			if ( linkedFiring )
			{//firing all muzzles at once
				if ( numMuzzlesReady != numMuzzles )
				{//can't fire all linked muzzles yet
					return;
				}
				else 
				{//can fire all linked muzzles, check ammo
					if ( pVeh->weaponStatus[weaponNum].ammo < cumulativeAmmo )
					{//can't fire, not enough ammo
						if ( pVeh->m_pPilot && pVeh->m_pPilot->s.number < MAX_CLIENTS )
						{// let the client know he's out of ammo
							G_AddEvent( (gentity_t*)pVeh->m_pPilot, EV_NOAMMO, weaponNum );
						}
						return;
					}
				}
			}

			for ( i = 0; i < MAX_VEHICLE_MUZZLES; i++ )
			{
				if ( pVeh->m_pVehicleInfo->weapMuzzle[i] != vehWeaponIndex )
				{//this muzzle doesn't match the weapon we're trying to use
					continue;
				}
				if ( !linkedFiring
					&& i != pVeh->weaponStatus[weaponNum].nextMuzzle )
				{//we're only firing one muzzle and this isn't it
					continue;
				}

				// Fire this muzzle.
				if ( pVeh->m_iMuzzleTag[i] != -1 && pVeh->m_iMuzzleWait[i] < level.time )
				{
					vec3_t	start, dir;
					
					if ( pVeh->weaponStatus[weaponNum].ammo < vehWeapon->iAmmoPerShot )
					{//out of ammo!
						if ( !sentAmmoWarning )
						{
							sentAmmoWarning = qtrue;
							if ( pVeh->m_pPilot && pVeh->m_pPilot->s.number < MAX_CLIENTS )
							{// let the client know he's out of ammo
								G_AddEvent( (gentity_t*)pVeh->m_pPilot, EV_NOAMMO, weaponNum );
							}
						}
					}
					else
					{//have enough ammo to shoot
						//do the firing
						WP_CalcVehMuzzle(ent, i);
						VectorCopy( pVeh->m_vMuzzlePos[i], start );
						VectorCopy( pVeh->m_vMuzzleDir[i], dir );
						if ( WP_VehCheckTraceFromCamPos( ent, start, dir ) )
						{//auto-aim at whatever crosshair would be over from camera's point of view (if closer)
						}
						else if ( aimCorrect )
						{//auto-aim the missile at the crosshair if there's anything there
							trace_t trace;
							vec3_t	end;
							vec3_t	ang;
							vec3_t	fixedDir;

							if (pVeh->m_pVehicleInfo->type == VH_SPEEDER)
							{
								VectorSet(ang, 0.0f, pVeh->m_vOrientation[1], 0.0f);
							}
							else
							{
								VectorCopy(pVeh->m_vOrientation, ang);
							}
							AngleVectors( ang, fixedDir, NULL, NULL );
							VectorMA( ent->r.currentOrigin, 32768, fixedDir, end );
							trap_Trace( &trace, ent->r.currentOrigin, vec3_origin, vec3_origin, end, ent->s.number, MASK_SHOT );
							if ( trace.fraction < 1.0f && !trace.allsolid && !trace.startsolid )
							{
								vec3_t newEnd;
								VectorCopy( trace.endpos, newEnd );
								WP_VehLeadCrosshairVeh( &g_entities[trace.entityNum], newEnd, fixedDir, start, dir );
							}
						}

						//play the weapon's muzzle effect if we have one
						//NOTE: just need MAX_VEHICLE_MUZZLES bits for this... should be cool since it's currently 12 and we're sending it in 16 bits
						muzzlesFired |= (1<<i);
												
						missile = WP_FireVehicleWeapon( ent, start, dir, vehWeapon, alt_fire, qfalse );
						if ( vehWeapon->fHoming )
						{//clear the rocket lock entity *after* all muzzles have fired
							clearRocketLockEntity = qtrue;
						}
					}

					if ( linkedFiring )
					{//we're linking the weapon, so continue on and fire all appropriate muzzles
						continue;
					}
					//else just firing one
					//take the ammo, set the next muzzle and set the delay on it
					if ( numMuzzles > 1 )
					{//more than one, look for it
						int nextMuzzle = pVeh->weaponStatus[weaponNum].nextMuzzle;
						while ( 1 )
						{
							nextMuzzle++;
							if ( nextMuzzle >= MAX_VEHICLE_MUZZLES )
							{
								nextMuzzle = 0;
							}
							if ( nextMuzzle == pVeh->weaponStatus[weaponNum].nextMuzzle )
							{//WTF?  Wrapped without finding another valid one!
								break;
							}
							if ( pVeh->m_pVehicleInfo->weapMuzzle[nextMuzzle] == vehWeaponIndex )
							{//this is the next muzzle for this weapon
								pVeh->weaponStatus[weaponNum].nextMuzzle = nextMuzzle;
								break;
							}
						}
					}//else, just stay on the one we just fired
					//set the delay on the next muzzle
					pVeh->m_iMuzzleWait[pVeh->weaponStatus[weaponNum].nextMuzzle] = level.time + delay;
					//take away the ammo
					pVeh->weaponStatus[weaponNum].ammo -= vehWeapon->iAmmoPerShot;
					//NOTE: in order to send the vehicle's ammo info to the client, we copy the ammo into the first 2 ammo slots on the vehicle NPC's client->ps.ammo array
					if ( pVeh->m_pParentEntity && ((gentity_t*)(pVeh->m_pParentEntity))->client )
					{
						((gentity_t*)(pVeh->m_pParentEntity))->client->ps.ammo[weaponNum] = pVeh->weaponStatus[weaponNum].ammo;
					}
					//done!
					//we'll get in here again next frame and try the next muzzle...
					goto tryFire;
				}
			}
			//we went through all the muzzles, so apply the cumulative delay and ammo cost
			if ( cumulativeAmmo )
			{//taking ammo one shot at a time
				//take the ammo
				pVeh->weaponStatus[weaponNum].ammo -= cumulativeAmmo;
				//NOTE: in order to send the vehicle's ammo info to the client, we copy the ammo into the first 2 ammo slots on the vehicle NPC's client->ps.ammo array
				if ( pVeh->m_pParentEntity && ((gentity_t*)(pVeh->m_pParentEntity))->client )
				{
					((gentity_t*)(pVeh->m_pParentEntity))->client->ps.ammo[weaponNum] = pVeh->weaponStatus[weaponNum].ammo;
				}
			}
			if ( cumulativeDelay )
			{//we linked muzzles so we need to apply the cumulative delay now, to each of the linked muzzles
				for ( i = 0; i < MAX_VEHICLE_MUZZLES; i++ )
				{
					if ( pVeh->m_pVehicleInfo->weapMuzzle[i] != vehWeaponIndex )
					{//this muzzle doesn't match the weapon we're trying to use
						continue;
					}
					//apply the cumulative delay
					pVeh->m_iMuzzleWait[i] = level.time + cumulativeDelay;
				}
			}
		}
	}

tryFire:
	if ( clearRocketLockEntity )
	{//hmm, should probably clear that anytime any weapon fires?
		ent->client->ps.rocketLockIndex = ENTITYNUM_NONE;
		ent->client->ps.rocketLockTime = 0;
		ent->client->ps.rocketTargetTime = 0;
	}

	if ( vehWeapon && muzzlesFired > 0 )
	{
		G_VehMuzzleFireFX(ent, missile, muzzlesFired );
	}
}

/*
===============
FireWeapon
===============
*/
#include "namespace_begin.h"
int BG_EmplacedView(vec3_t baseAngles, vec3_t angles, float *newYaw, float constraint);
#include "namespace_end.h"

void FireWeapon( gentity_t *ent, qboolean altFire ) {

	// track shots taken for accuracy tracking.  Grapple is not a weapon and gauntet is just not tracked
	if( !(ent && ent->client && ent->client->sess.inRacemode) && ent->s.weapon != WP_SABER && ent->s.weapon != WP_STUN_BATON && ent->s.weapon != WP_MELEE 
		/* *CHANGE 63* mines and det packs should not be considered in accuracy  */
		&& 	ent->s.weapon != WP_TRIP_MINE && ent->s.weapon != WP_DET_PACK 	
		) 
	{
		if (ent->s.weapon == WP_DISRUPTOR) {
			ent->client->stats->accuracy_shots++;
			if (altFire)
				ent->client->stats->accuracy_shotsOfType[ACC_DISRUPTOR_SNIPE]++;
			else
				ent->client->stats->accuracy_shotsOfType[ACC_DISRUPTOR_PRIMARY]++;
		}
		else if (ent->s.weapon == WP_REPEATER && altFire) {
			ent->client->stats->accuracy_shots++;
			ent->client->stats->accuracy_shotsOfType[ACC_REPEATER_ALT]++;
		}
		else if (ent->s.weapon == WP_FLECHETTE && altFire) {
			ent->client->stats->accuracy_shots++;
			ent->client->stats->accuracy_shotsOfType[ACC_GOLAN_ALT]++;
		}
		else if (ent->s.weapon == WP_ROCKET_LAUNCHER) {
			ent->client->stats->accuracy_shots++;
			ent->client->stats->accuracy_shotsOfType[ACC_ROCKET]++;
		}
		else if (ent->s.weapon == WP_THERMAL && altFire) {
			ent->client->stats->accuracy_shots++;
			ent->client->stats->accuracy_shotsOfType[ACC_THERMAL_ALT]++;
		}
		else if (ent->s.weapon == WP_CONCUSSION) {
			ent->client->stats->accuracy_shots++;
			if (altFire)
				ent->client->stats->accuracy_shotsOfType[ACC_CONCUSSION_ALT]++;
			else
				ent->client->stats->accuracy_shotsOfType[ACC_CONCUSSION_PRIMARY]++;
		}
	}

	if ( ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE )
	{
		FireVehicleWeapon( ent, altFire );
		return;
	}
	else
	{
		// set aiming directions
		if (ent->s.weapon == WP_EMPLACED_GUN &&
			ent->client->ps.emplacedIndex)
		{ //if using emplaced then base muzzle point off of gun position/angles
			gentity_t *emp = &g_entities[ent->client->ps.emplacedIndex];

			if (emp->inuse)
			{
				float yaw;
				vec3_t viewAngCap;
				int override;

				VectorCopy(ent->client->ps.viewangles, viewAngCap);
				if (viewAngCap[PITCH] > 40)
				{
					viewAngCap[PITCH] = 40;
				}

				override = BG_EmplacedView(ent->client->ps.viewangles, emp->s.angles, &yaw,
					emp->s.origin2[0]);
				
				if (override)
				{
					viewAngCap[YAW] = yaw;
				}

				AngleVectors( viewAngCap, forward, vright, up );
			}
			else
			{
				AngleVectors( ent->client->ps.viewangles, forward, vright, up );
			}
		}
		else if (ent->s.number < MAX_CLIENTS &&
			ent->client->ps.m_iVehicleNum && ent->s.weapon == WP_BLASTER)
		{ //riding a vehicle...with blaster selected
			vec3_t vehTurnAngles;
			gentity_t *vehEnt = &g_entities[ent->client->ps.m_iVehicleNum];

			if (vehEnt->inuse && vehEnt->client && vehEnt->m_pVehicle)
			{
				VectorCopy(vehEnt->m_pVehicle->m_vOrientation, vehTurnAngles);
				vehTurnAngles[PITCH] = ent->client->ps.viewangles[PITCH];
			}
			else
			{
				VectorCopy(ent->client->ps.viewangles, vehTurnAngles);
			}
			if (ent->client->pers.cmd.rightmove > 0)
			{ //shooting to right
				vehTurnAngles[YAW] -= 90.0f;
			}
			else if (ent->client->pers.cmd.rightmove < 0)
			{ //shooting to left
				vehTurnAngles[YAW] += 90.0f;
			}

			AngleVectors( vehTurnAngles, forward, vright, up );
		}
		else
		{
			AngleVectors( ent->client->ps.viewangles, forward, vright, up );
		}

		CalcMuzzlePoint ( ent, forward, vright, up, muzzle );

		// fire the specific weapon
		switch( ent->s.weapon ) {
		case WP_STUN_BATON:
			WP_FireStunBaton( ent, altFire );
			break;

		case WP_MELEE:
			WP_FireMelee(ent, altFire);
			break;

		case WP_SABER:
			break;

		case WP_BRYAR_PISTOL:
			//if ( g_gametype.integer == GT_SIEGE )
			if (1)
			{//allow alt-fire
				WP_FireBryarPistol( ent, altFire );
			}
			else
			{
				WP_FireBryarPistol( ent, qfalse );
			}
			break;

		case WP_CONCUSSION:
			if ( altFire && !(ent && ent->client && ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_BOOST_AIMBOTBOOST) && g_aimbotBoost_hitscan.integer) )
			{
				WP_FireConcussionAlt( ent );
			}
			else
			{
				WP_FireConcussion( ent );
			}
			break;

		case WP_BRYAR_OLD:
			WP_FireBryarPistol( ent, altFire );
			break;

		case WP_BLASTER:
			WP_FireBlaster( ent, altFire );
			break;

		case WP_DISRUPTOR:
			WP_FireDisruptor( ent, altFire );
			break;

		case WP_BOWCASTER:
			WP_FireBowcaster( ent, altFire );
			break;

		case WP_REPEATER:
			WP_FireRepeater( ent, altFire );
			break;

		case WP_DEMP2:
			WP_FireDEMP2( ent, altFire );
			break;

		case WP_FLECHETTE:
			WP_FireFlechette( ent, altFire );
			break;

		case WP_ROCKET_LAUNCHER:
			WP_FireRocket( ent, altFire );
			break;

		case WP_THERMAL:
			WP_FireThermalDetonator( ent, altFire );
			break;

		case WP_TRIP_MINE:
			WP_PlaceLaserTrap( ent, altFire );
			break;

		case WP_DET_PACK:
			WP_DropDetPack( ent, altFire );
			break;

		case WP_EMPLACED_GUN:
			if (ent->client && ent->client->ewebIndex)
			{ //specially handled by the e-web itself
				break;
			}
			WP_FireEmplaced( ent, altFire );
			break;
		default:
			break;
		}
	}

	G_LogWeaponFire(ent->s.number, ent->s.weapon);
}

//---------------------------------------------------------
static void WP_FireEmplaced( gentity_t *ent, qboolean altFire )
//---------------------------------------------------------
{
	vec3_t	dir, angs, gunpoint;
	vec3_t	right;
	gentity_t *gun;
	int side;

	if (!ent->client)
	{
		return;
	}

	if (!ent->client->ps.emplacedIndex)
	{ //shouldn't be using WP_EMPLACED_GUN if we aren't on an emplaced weapon
		return;
	}

	gun = &g_entities[ent->client->ps.emplacedIndex];

	if (!gun->inuse || gun->health <= 0)
	{ //gun was removed or killed, although we should never hit this check because we should have been forced off it already
		return;
	}

	VectorCopy(gun->s.origin, gunpoint);
	gunpoint[2] += 46;

	AngleVectors(ent->client->ps.viewangles, NULL, right, NULL);

	if (gun->genericValue10)
	{ //fire out of the right cannon side
		VectorMA(gunpoint, 10.0f, right, gunpoint);
		side = 0;
	}
	else
	{ //the left
		VectorMA(gunpoint, -10.0f, right, gunpoint);
		side = 1;
	}

	gun->genericValue10 = side;
	G_AddEvent(gun, EV_FIRE_WEAPON, side);

	vectoangles( forward, angs );

	AngleVectors( angs, dir, NULL, NULL );

	WP_FireEmplacedMissile( gun, gunpoint, dir, altFire, ent );
}

#define EMPLACED_CANRESPAWN 1

//----------------------------------------------------------

/*QUAKED emplaced_gun (0 0 1) (-30 -20 8) (30 20 60) CANRESPAWN

 count - if CANRESPAWN spawnflag, decides how long it is before gun respawns (in ms)
 constraint - number of degrees gun is constrained from base angles on each side (default 60.0)

 showhealth - set to 1 to show health bar on this entity when crosshair is over it
  
  teamowner - crosshair shows green for this team, red for opposite team
	0 - none
	1 - red
	2 - blue

  alliedTeam - team that can use this
	0 - any
	1 - red
	2 - blue

  teamnodmg - team that turret does not take damage from or do damage to
	0 - none
	1 - red
	2 - blue
*/
 
//----------------------------------------------------------
extern qboolean TryHeal(gentity_t *ent, gentity_t *target); //g_utils.c
void emplaced_gun_use( gentity_t *self, gentity_t *other, trace_t *trace )
{
	vec3_t fwd1, fwd2;
	float dot;
	int oldWeapon;
	gentity_t *activator = other;
	float zoffset = 50;
	vec3_t anglesToOwner;
	vec3_t vLen;
	float ownLen;

	if ( self->health <= 0 )
	{ //gun is destroyed
		return;
	}

	if (self->activator)
	{ //someone is already using me
		return;
	}

	if (!activator->client)
	{
		return;
	}

	if (activator->client->ps.emplacedTime > level.time)
	{ //last use attempt still too recent
		return;
	}

	if (activator->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{ //don't use if busy doing something else
		return;
	}

	if (activator->client->ps.origin[2] > self->s.origin[2]+zoffset-8)
	{ //can't use it from the top
		return;
	}

	if (activator->client->ps.pm_flags & PMF_DUCKED)
	{ //must be standing
		return;
	}

	if (activator->client->ps.isJediMaster)
	{ //jm can't use weapons
		return;
	}

	VectorSubtract(self->s.origin, activator->client->ps.origin, vLen);
	ownLen = VectorLength(vLen);

	if (ownLen > 64.0f)
	{ //must be within 64 units of the gun to use at all
		return;
	}

	// Let's get some direction vectors for the user
	AngleVectors( activator->client->ps.viewangles, fwd1, NULL, NULL );

	// Get the guns direction vector
	AngleVectors( self->pos1, fwd2, NULL, NULL );

	dot = DotProduct( fwd1, fwd2 );

	// Must be reasonably facing the way the gun points ( 110 degrees or so ), otherwise we don't allow to use it.
	if ( dot < -0.2f )
	{
		goto tryHeal;
	}

	VectorSubtract(self->s.origin, activator->client->ps.origin, fwd1);
	VectorNormalize(fwd1);

	dot = DotProduct( fwd1, fwd2 );

	//check the positioning in relation to the gun as well
	if ( dot < 0.6f )
	{
		goto tryHeal;
	}

	self->genericValue1 = 1;

	oldWeapon = activator->s.weapon;

	// swap the users weapon with the emplaced gun
	activator->client->ps.weapon = self->s.weapon;
	activator->client->ps.weaponstate = WEAPON_READY;
	activator->client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_EMPLACED_GUN );

	activator->client->ps.emplacedIndex = self->s.number;

	self->s.emplacedOwner = activator->s.number;
	self->s.activeForcePass = NUM_FORCE_POWERS+1;

	// the gun will track which weapon we used to have
	self->s.weapon = oldWeapon;

	//user's new owner becomes the gun ent
	activator->r.ownerNum = self->s.number;
	self->activator = activator;

	VectorSubtract(self->r.currentOrigin, activator->client->ps.origin, anglesToOwner);
	vectoangles(anglesToOwner, anglesToOwner);
	return;

tryHeal: //well, not in the right dir, try healing it instead...
	TryHeal(activator, self);
}

void emplaced_gun_realuse( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	emplaced_gun_use(self, other, NULL);
}

//----------------------------------------------------------
void emplaced_gun_pain( gentity_t *self, gentity_t *attacker, int damage )
{
	self->s.health = self->health;

	if ( self->health <= 0 )
	{
		//death effect.. for now taken care of on cgame
	}
	else
	{
		//if we have a pain behavior set then use it I guess
		G_ActivateBehavior( self, BSET_PAIN );
	}
}

#define EMPLACED_GUN_HEALTH 800

//----------------------------------------------------------
void emplaced_gun_update(gentity_t *self)
{
	vec3_t	smokeOrg, puffAngle;
	int oldWeap;
	float ownLen = 0;

	if (self->health < 1 && !self->genericValue5)
	{ //we are dead, set our respawn delay if we have one
		if (self->spawnflags & EMPLACED_CANRESPAWN)
		{
			self->genericValue5 = level.time + 4000 + self->count;
		}
	}
	else if (self->health < 1 && self->genericValue5 < level.time)
	{ //we are dead, see if it's time to respawn
		self->s.time = 0;
		self->genericValue4 = 0;
		self->genericValue3 = 0;
		self->health = EMPLACED_GUN_HEALTH*0.4;
		self->s.health = self->health;
	}

	if (self->genericValue4 && self->genericValue4 < 2 && self->s.time < level.time)
	{ //we have finished our warning (red flashing) effect, it's time to finish dying
		vec3_t explOrg;

		VectorSet( puffAngle, 0, 0, 1 );

		VectorCopy(self->r.currentOrigin, explOrg);
		explOrg[2] += 16;

		//just use the detpack explosion effect
		gentity_t *te = G_PlayEffect(EFFECT_EXPLOSION_DETPACK, explOrg, puffAngle);
		G_ApplyRaceBroadcastsToEvent( self, te );

		self->genericValue3 = level.time + Q_irand(2500, 3500);

		G_RadiusDamage(self->r.currentOrigin, self, self->splashDamage, self->splashRadius, self, NULL, MOD_UNKNOWN);

		self->s.time = -1;

		self->genericValue4 = 2;
	}

	if (self->genericValue3 > level.time)
	{ //see if we are freshly dead and should be smoking
		if (self->genericValue2 < level.time)
		{ //is it time yet to spawn another smoke puff?
			VectorSet( puffAngle, 0, 0, 1 );
			VectorCopy(self->r.currentOrigin, smokeOrg);

			smokeOrg[2] += 60;

			gentity_t *te = G_PlayEffect(EFFECT_SMOKE, smokeOrg, puffAngle);
			G_ApplyRaceBroadcastsToEvent( self, te );
			self->genericValue2 = level.time + Q_irand(250, 400);
		}
	}

	if (self->activator && self->activator->client && self->activator->inuse)
	{ //handle updating current user
		vec3_t vLen;
		VectorSubtract(self->s.origin, self->activator->client->ps.origin, vLen);
		ownLen = VectorLength(vLen);

		if (!(self->activator->client->pers.cmd.buttons & BUTTON_USE) && self->genericValue1)
		{
			self->genericValue1 = 0;
		}

		if ((self->activator->client->pers.cmd.buttons & BUTTON_USE) && !self->genericValue1)
		{
			self->activator->client->ps.emplacedIndex = 0;
			self->activator->client->ps.saberHolstered = 0;
			self->nextthink = level.time + 50;
			return;
		}
	}

	if ((self->activator && self->activator->client) &&
		(!self->activator->inuse || self->activator->client->ps.emplacedIndex != self->s.number || self->genericValue4 || ownLen > 64))
	{ //get the user off of me then
		self->activator->client->ps.stats[STAT_WEAPONS] &= ~(1<<WP_EMPLACED_GUN);

		oldWeap = self->activator->client->ps.weapon;
		self->activator->client->ps.weapon = self->s.weapon;
		self->s.weapon = oldWeap;
		self->activator->r.ownerNum = ENTITYNUM_NONE;
		self->activator->client->ps.emplacedTime = level.time + 1000;
		self->activator->client->ps.emplacedIndex = 0;
		self->activator->client->ps.saberHolstered = 0;
		self->activator = NULL;

		self->s.activeForcePass = 0;
	}
	else if (self->activator && self->activator->client)
	{ //make sure the user is using the emplaced gun weapon
		self->activator->client->ps.weapon = WP_EMPLACED_GUN;
		self->activator->client->ps.weaponstate = WEAPON_READY;
	}
	self->nextthink = level.time + 50;
}

//----------------------------------------------------------
void emplaced_gun_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod )
{ //set us up to flash and then explode
	if (self->genericValue4)
	{
		return;
	}

	self->genericValue4 = 1;

	self->s.time = level.time + 3000;

	self->genericValue5 = 0;
}

void SP_emplaced_gun( gentity_t *ent )
{
	const char *name = "models/map_objects/mp/turret_chair.glm";
	vec3_t down;
	trace_t tr;

	//make sure our assets are precached
	RegisterItem( BG_FindItemForWeapon(WP_EMPLACED_GUN) );

	ent->r.contents = CONTENTS_SOLID;
	ent->s.solid = SOLID_BBOX;

	ent->genericValue5 = 0;

	VectorSet( ent->r.mins, -30, -20, 8 );
	VectorSet( ent->r.maxs, 30, 20, 60 );

	VectorCopy(ent->s.origin, down);

	down[2] -= 1024;

	trap_Trace(&tr, ent->s.origin, ent->r.mins, ent->r.maxs, down, ent->s.number, MASK_SOLID);

	if (tr.fraction != 1 && !tr.allsolid && !tr.startsolid)
	{
		VectorCopy(tr.endpos, ent->s.origin);
	}

	ent->spawnflags |= 4; // deadsolid

	ent->health = EMPLACED_GUN_HEALTH;

	if (ent->spawnflags & EMPLACED_CANRESPAWN)
	{ //make it somewhat easier to kill if it can respawn
		ent->health *= 0.4;
	}

	ent->maxHealth = ent->health;
	G_ScaleNetHealth(ent);

	ent->genericValue4 = 0;

	ent->takedamage = qtrue;
	ent->pain = emplaced_gun_pain;
	ent->die = emplaced_gun_die;

	// being caught in this thing when it blows would be really bad.
	ent->splashDamage = 80;
	ent->splashRadius = 128;

	// amount of ammo that this little poochie has
	G_SpawnInt( "count", "600", &ent->count );

	G_SpawnFloat( "constraint", "60", &ent->s.origin2[0] );

	ent->s.modelindex = G_ModelIndex( (char *)name );
	ent->s.modelGhoul2 = 1;
	ent->s.g2radius = 110;

	//so the cgame knows for sure that we're an emplaced weapon
	ent->s.weapon = WP_EMPLACED_GUN;

	G_SetOrigin( ent, ent->s.origin );
	
	// store base angles for later
	VectorCopy( ent->s.angles, ent->pos1 );
	VectorCopy( ent->s.angles, ent->r.currentAngles );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );

	ent->think = emplaced_gun_update;
	ent->nextthink = level.time + 50;

	ent->use = emplaced_gun_realuse;

	ent->r.svFlags |= SVF_PLAYER_USABLE;

	ent->s.pos.trType = TR_STATIONARY;

	ent->s.owner = MAX_CLIENTS+1;
	ent->s.shouldtarget = qtrue;

	trap_LinkEntity(ent);
}
