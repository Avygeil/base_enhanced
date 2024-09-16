// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"
#include "bg_saga.h"

extern void Jedi_Cloak( gentity_t *self );
extern void Jedi_Decloak( gentity_t *self );

#include "namespace_begin.h"
qboolean PM_SaberInTransition( int move );
qboolean PM_SaberInStart( int move );
qboolean PM_SaberInReturn( int move );
qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
#include "namespace_end.h"
qboolean saberCheckKnockdown_DuelLoss(gentity_t *saberent, gentity_t *saberOwner, gentity_t *other);

extern vmCvar_t g_saberLockRandomNess;

void P_SetTwitchInfo(gclient_t	*client)
{
	client->ps.painTime = level.time;
	client->ps.painDirection ^= 1;
}

/*
============
G_ResetTrail

Clear out the given client's origin trails (should be called from ClientBegin and when
the teleport bit is toggled)
============
*/
void G_ResetTrail(gentity_t *ent) {
	int		i;

	// fill up the origin trails with data (assume the current position for the last 1/2 second or so)
	level.unlagged[ent - g_entities].trailHead = MAX_UNLAGGED_TRAILS - 1;
	int now = trap_Milliseconds();
	for (i = level.unlagged[ent - g_entities].trailHead; i >= 0; i--) {
		VectorCopy(ent->r.mins, level.unlagged[ent - g_entities].trail[i].mins);
		VectorCopy(ent->r.maxs, level.unlagged[ent - g_entities].trail[i].maxs);
		VectorCopy(ent->r.currentOrigin, level.unlagged[ent - g_entities].trail[i].currentOrigin);
		//VectorCopy( ent->r.currentAngles, level.unlagged[ent - g_entities].trail[i].currentAngles );

		level.unlagged[ent - g_entities].trail[i].torsoAnim = ent->client->ps.torsoAnim;
		level.unlagged[ent - g_entities].trail[i].torsoTimer = ent->client->ps.torsoTimer;
		level.unlagged[ent - g_entities].trail[i].legsAnim = ent->client->ps.legsAnim;
		level.unlagged[ent - g_entities].trail[i].legsTimer = ent->client->ps.legsTimer;
		level.unlagged[ent - g_entities].trail[i].realAngle = ent->s.apos.trBase[YAW];
		level.unlagged[ent - g_entities].trail[i].time = now;
	}
}


/*
============
G_StoreTrail

Keep track of where the client's been (usually called every ClientThink)
============
*/
void G_StoreTrail(gentity_t *ent) {
	int		head;

	head = level.unlagged[ent - g_entities].trailHead;

	// increment the head
	level.unlagged[ent - g_entities].trailHead++;
	if (level.unlagged[ent - g_entities].trailHead >= MAX_UNLAGGED_TRAILS) {
		level.unlagged[ent - g_entities].trailHead = 0;
	}
	head = level.unlagged[ent - g_entities].trailHead;

	// store all the collision-detection info and the time
	VectorCopy(ent->r.mins, level.unlagged[ent - g_entities].trail[head].mins);
	VectorCopy(ent->r.maxs, level.unlagged[ent - g_entities].trail[head].maxs);
	//VectorCopy( ent->r.currentAngles, level.unlagged[ent - g_entities].trail[head].currentAngles );

	level.unlagged[ent - g_entities].trail[head].torsoAnim = ent->client->ps.torsoAnim;
	level.unlagged[ent - g_entities].trail[head].torsoTimer = ent->client->ps.torsoTimer;
	level.unlagged[ent - g_entities].trail[head].legsAnim = ent->client->ps.legsAnim;
	level.unlagged[ent - g_entities].trail[head].legsTimer = ent->client->ps.legsTimer;

	if (ent->NPC) { // try to get higher resolution on npcs
		vec3_t origin, angles;
		BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);
		BG_EvaluateTrajectory(&ent->s.apos, level.time, angles);
		VectorCopy(origin, level.unlagged[ent - g_entities].trail[head].currentOrigin);
		level.unlagged[ent - g_entities].trail[head].realAngle = angles[YAW];
	}
	else {
		VectorCopy(ent->r.currentOrigin, level.unlagged[ent - g_entities].trail[head].currentOrigin);
		level.unlagged[ent - g_entities].trail[head].realAngle = ent->s.apos.trBase[YAW];
	}

	//if (level.unlagged[ent - g_entities].trail[head].currentAngles[0] || level.unlagged[ent - g_entities].trail[head].currentAngles[1] || level.unlagged[ent - g_entities].trail[head].currentAngles[2])
		//Com_Printf("Current angles are %.2f %.2f %.2f\n", level.unlagged[ent - g_entities].trail[head].currentAngles[0], level.unlagged[ent - g_entities].trail[head].currentAngles[1], level.unlagged[ent - g_entities].trail[head].currentAngles[2]);

	level.unlagged[ent - g_entities].trail[head].time = level.lastThinkRealTime[ent - g_entities];

	//Also store their anim info? Since with ghoul2 collision that matters..

	// FOR TESTING ONLY
	//Com_Printf("level.previousTime: %d, level.time: %d, newtime: %d\n", level.previousTime, level.time, newtime);
}


/*
=============
TimeShiftLerp

Used below to interpolate between two previous vectors
Returns a vector "frac" times the distance between "start" and "end"
=============
*/
void TimeShiftLerp(float frac, vec3_t start, vec3_t end, vec3_t result) {
	if (!g_unlaggedFix.integer) { // broken eternaljk code that i blindly imported
		float	comp = 1.0f - frac;

		result[0] = frac * start[0] + comp * end[0];
		result[1] = frac * start[1] + comp * end[1];
		result[2] = frac * start[2] + comp * end[2];
	}
	else { // correct implementation
		result[0] = start[0] + frac * (end[0] - start[0]);
		result[1] = start[1] + frac * (end[1] - start[1]);
		result[2] = start[2] + frac * (end[2] - start[2]);
	}
}

void TimeShiftAnimLerp(float frac, int anim1, int anim2, int time1, int time2, int *outTime) {
	if (anim1 == anim2 && time2 > time1) {//Only lerp if both anims are same and time2 is after time1.
		*outTime = time1 + (time2 - time1) * frac;
	}
	else
		*outTime = time2;

	//Com_Printf("Timeshift anim lerping: time1 is %i, time 2 is %i, lerped is %i\n", time1, time2, outTime);
}

extern void G_TestLine(vec3_t start, vec3_t end, int color, int time);
static void G_DrawPlayerStick(gentity_t *ent, int color, int duration, int time) {
	//Lets draw a visual of the unlagged hitbox difference?
	//Not sure how to get bounding boxes of g2 parts, so maybe just two stick figures? (lagged stick figure = red, unlagged stick figure = green) and make them both appear for like 5 seconds?
	vec3_t headPos, torsoPos, rHandPos, lHandPos, rArmPos, lArmPos, rKneePos, lKneePos, rFootPos, lFootPos, G2Angles;
	mdxaBone_t	boltMatrix;
	int handLBolt, handRBolt, armRBolt, armLBolt, kneeLBolt, kneeRBolt, footLBolt, footRBolt;

	if (!ent->client)
		return;
	if (ent->localAnimIndex > 1) //Not humanoid
		return;

	//Com_Printf("Drawing player stick model for %s\n", ent->client->pers.netname);

	handLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*l_hand");
	handRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*r_hand");
	armLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*l_arm_elbow");
	armRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*r_arm_elbow");
	kneeLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*hips_l_knee");
	kneeRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*hips_r_knee");
	footLBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*l_leg_foot");
	footRBolt = trap_G2API_AddBolt(ent->ghoul2, 0, "*r_leg_foot");  //Shouldnt these always be the same numbers? can just make them constants?.. or pm->g2Bolts_RFoot etc?

	VectorSet(G2Angles, 0, ent->s.apos.trBase[YAW], 0); //Ok so r.currentAngles isnt even used for players ??
	VectorCopy(ent->r.currentOrigin, torsoPos);

	VectorCopy(torsoPos, headPos);
	headPos[2] += ent->r.maxs[2]; //E?
	torsoPos[2] += 8;//idk man
	G_TestLine(headPos, torsoPos, color, duration); //Head -> Torso

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, armRBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale); //ent->cmd.serverTime ?.. why does this need time?
	rArmPos[0] = boltMatrix.matrix[0][3];
	rArmPos[1] = boltMatrix.matrix[1][3];
	rArmPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(torsoPos, rArmPos, color, duration); //Torso -> R Arm

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, armLBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	lArmPos[0] = boltMatrix.matrix[0][3];
	lArmPos[1] = boltMatrix.matrix[1][3];
	lArmPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(torsoPos, lArmPos, color, duration); //Torso -> L Arm

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, handRBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	rHandPos[0] = boltMatrix.matrix[0][3];
	rHandPos[1] = boltMatrix.matrix[1][3];
	rHandPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(rArmPos, rHandPos, color, duration); //R Arm  -> R Hand

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, handLBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	lHandPos[0] = boltMatrix.matrix[0][3];
	lHandPos[1] = boltMatrix.matrix[1][3];
	lHandPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(lArmPos, lHandPos, color, duration); //L Arm -> L Hand

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, kneeRBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	rKneePos[0] = boltMatrix.matrix[0][3];
	rKneePos[1] = boltMatrix.matrix[1][3];
	rKneePos[2] = boltMatrix.matrix[2][3];
	G_TestLine(torsoPos, rKneePos, color, duration); //Torso -> R Knee

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, kneeLBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	lKneePos[0] = boltMatrix.matrix[0][3];
	lKneePos[1] = boltMatrix.matrix[1][3];
	lKneePos[2] = boltMatrix.matrix[2][3];
	G_TestLine(torsoPos, lKneePos, color, duration); //Torso -> L Knee

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, footRBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	rFootPos[0] = boltMatrix.matrix[0][3];
	rFootPos[1] = boltMatrix.matrix[1][3];
	rFootPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(rKneePos, rFootPos, color, duration); //R Knee -> R Foot

	trap_G2API_GetBoltMatrix(ent->ghoul2, 0, footLBolt, &boltMatrix, G2Angles, ent->r.currentOrigin, time, NULL, ent->modelScale);
	lFootPos[0] = boltMatrix.matrix[0][3];
	lFootPos[1] = boltMatrix.matrix[1][3];
	lFootPos[2] = boltMatrix.matrix[2][3];
	G_TestLine(lKneePos, lFootPos, color, duration); //L Knee -> L Foot
}

static aimPracticeMovementTrail_t savedNpcTrails[MAX_GENTITIES] = { 0 };
static int savedTorsoTimers[MAX_GENTITIES] = { 0 };
static int savedLegTimers[MAX_GENTITIES] = { 0 };

void G_TimeShiftNPC(gentity_t *ent, int time, qboolean timeshiftAnims) {
	assert(ent && ent->client && ent->isAimPracticePack);

#ifdef _DEBUG
	if (g_unlaggedSkeletons.integer & (1 << 0))
		G_DrawPlayerStick(ent, 0x0000ff, Com_Clampi(1, 60, abs(g_unlaggedSkeletonTime.integer)) * 1000, level.time);
#endif

	aimPracticePack_t *pack = ent->isAimPracticePack;
	aimVariant_t *var = &pack->variants[pack->currentVariantIndex];
	int timeElapsed = time - pack->routeStartTime;

	int i;
	for (i = 0; i < var->trailHead; i++) {
		if (var->trail[i].time > timeElapsed) {
			break;
		}
	}
	int h = i - 1;
	if (h < 0)
		h = 0;

	aimPracticeMovementTrail_t *early = &var->trail[h], *late = &var->trail[i];

	float frac = (float)(late->time - timeElapsed) / (float)(late->time - early->time);

	VectorCopy(ent->r.currentOrigin, savedNpcTrails[ent - g_entities].currentOrigin);
	VectorCopy(ent->r.mins, savedNpcTrails[ent - g_entities].mins);
	VectorCopy(ent->r.maxs, savedNpcTrails[ent - g_entities].maxs);
	savedNpcTrails[ent - g_entities].torsoAnim = ent->client->ps.torsoAnim;
	savedNpcTrails[ent - g_entities].legsAnim = ent->client->ps.legsAnim;
	savedTorsoTimers[ent - g_entities] = ent->client->ps.torsoTimer;
	savedLegTimers[ent - g_entities] = ent->client->ps.legsTimer;
	VectorCopy(ent->s.apos.trBase, savedNpcTrails[ent - g_entities].angles);
	VectorCopy(ent->client->ps.velocity, savedNpcTrails[ent - g_entities].velocity);

#ifdef _DEBUG
	if (g_unlaggedSkeletons.integer & (1 << 1)) {
		VectorCopy(early->mins, ent->r.mins);
		VectorCopy(early->maxs, ent->r.maxs);
		VectorCopy(early->currentOrigin, ent->r.currentOrigin);
		if (timeshiftAnims) {
			ent->client->ps.torsoAnim = early->torsoAnim;
			ent->client->ps.torsoTimer = early->torsoTimer;
			ent->client->ps.legsAnim = early->legsAnim;
			ent->client->ps.legsTimer = early->legsTimer;
			ent->s.apos.trBase[YAW] = early->angles[YAW];
		}

		G_DrawPlayerStick(ent, 0xff0000, Com_Clampi(1, 60, abs(g_unlaggedSkeletonTime.integer)) * 1000, level.time);
	}

	if (g_unlaggedSkeletons.integer & (1 << 2)) {
		VectorCopy(late->mins, ent->r.mins);
		VectorCopy(late->maxs, ent->r.maxs);
		VectorCopy(late->currentOrigin, ent->r.currentOrigin);
		if (timeshiftAnims) {
			ent->client->ps.torsoAnim = late->torsoAnim;
			ent->client->ps.torsoTimer = late->torsoTimer;
			ent->client->ps.legsAnim = late->legsAnim;
			ent->client->ps.legsTimer = late->legsTimer;
			ent->s.apos.trBase[YAW] = late->angles[YAW];
		}

		G_DrawPlayerStick(ent, 0xffff00, Com_Clampi(1, 60, abs(g_unlaggedSkeletonTime.integer)) * 1000, level.time);
	}
#endif

	TimeShiftLerp(frac, late->currentOrigin, early->currentOrigin, ent->r.currentOrigin);
	TimeShiftLerp(frac, late->mins, early->mins, ent->r.mins);
	TimeShiftLerp(frac, late->maxs, early->maxs, ent->r.maxs);

	ent->client->ps.torsoAnim = late->torsoAnim;
	ent->client->ps.legsAnim = late->legsAnim;
	TimeShiftAnimLerp(frac, late->torsoAnim, early->torsoAnim, late->torsoTimer, early->torsoTimer, &ent->client->ps.torsoTimer);
	TimeShiftAnimLerp(frac, late->legsAnim, early->legsAnim, late->legsTimer, early->legsTimer, &ent->client->ps.legsTimer);
	ent->s.apos.trBase[YAW] = LerpAngle(late->angles[YAW], early->angles[YAW], frac);
	TimeShiftLerp(frac, late->velocity, early->velocity, ent->client->ps.velocity);

#ifdef _DEBUG
	if (g_unlaggedSkeletonTime.integer && g_unlaggedSkeletons.integer & (1 << 3)) {
		G_DrawPlayerStick(ent, 0x00ff00, Com_Clampi(1, 60, abs(g_unlaggedSkeletonTime.integer)) * 1000, level.time);
		//Com_Printf("post angle is %.2f\n", ent->s.apos.trBase[YAW]);
	}
#endif
}

/*
=================
G_TimeShiftClient

Move a client back to where he was at the specified "time"
=================
*/
void G_TimeShiftClient(gentity_t *ent, int time, qboolean timeshiftAnims) {
	int		j, k;
	int now = trap_Milliseconds();
	if (time > now) {
		time = now;
	}

#ifdef _DEBUG
	if (g_unlaggedDebug.integer)
		PrintIngame(-1, "G_TimeShiftClient: ent %d, trap_milliseconds is %d, time is %d, trailhead time is %d", ent - g_entities, now, time, level.unlagged[ent - g_entities].trail[level.unlagged[ent - g_entities].trailHead].time);
#endif

	// find two entries in the origin trail whose times sandwich "time"
	// assumes no two adjacent trail records have the same timestamp
	j = k = level.unlagged[ent - g_entities].trailHead;
	do {
		if (level.unlagged[ent - g_entities].trail[j].time <= time)
			break;

		k = j;
		j--;
		if (j < 0) {
			j = MAX_UNLAGGED_TRAILS - 1;
		}
	} while (j != level.unlagged[ent - g_entities].trailHead);

#ifdef _DEBUG
	if (g_unlaggedDebug.integer)
		PrintIngame(-1, ", j is %d, k is %d, trailhead is %d\n", j, k, level.unlagged[ent - g_entities].trailHead);
#endif

	// if we got past the first iteration above, we've sandwiched (or wrapped)
	if (j != k) {
		// make sure it doesn't get re-saved
		if (level.unlagged[ent - g_entities].saved.time != now) {
			// save the current origin and bounding box
			VectorCopy(ent->r.mins, level.unlagged[ent - g_entities].saved.mins);
			VectorCopy(ent->r.maxs, level.unlagged[ent - g_entities].saved.maxs);
			VectorCopy(ent->r.currentOrigin, level.unlagged[ent - g_entities].saved.currentOrigin);
			//VectorCopy( ent->r.currentAngles, level.unlagged[ent - g_entities].saved.currentAngles );

			if (timeshiftAnims) {
				level.unlagged[ent - g_entities].saved.torsoAnim = ent->client->ps.torsoAnim;
				level.unlagged[ent - g_entities].saved.torsoTimer = ent->client->ps.torsoTimer;
				level.unlagged[ent - g_entities].saved.legsAnim = ent->client->ps.legsAnim;
				level.unlagged[ent - g_entities].saved.legsTimer = ent->client->ps.legsTimer;
				level.unlagged[ent - g_entities].saved.realAngle = ent->s.apos.trBase[YAW];
			}

			level.unlagged[ent - g_entities].saved.time = now;
		}

#ifdef _DEBUG
		if (g_unlaggedSkeletonTime.integer) {
			if (g_unlaggedSkeletons.integer & (1 << 0))
				G_DrawPlayerStick(ent, 0x0000ff, Com_Clampi(1, 60, abs(g_unlaggedSkeletonTime.integer)) * 1000, level.time);
			//Com_Printf("pre angle is %.2f\n", ent->s.apos.trBase[YAW]);

			if (g_unlaggedSkeletons.integer & (1 << 1)) {
				VectorCopy(level.unlagged[ent - g_entities].trail[j].mins, ent->r.mins);
				VectorCopy(level.unlagged[ent - g_entities].trail[j].maxs, ent->r.maxs);
				VectorCopy(level.unlagged[ent - g_entities].trail[j].currentOrigin, ent->r.currentOrigin);
				if (timeshiftAnims) {
					ent->client->ps.torsoAnim = level.unlagged[ent - g_entities].trail[j].torsoAnim;
					ent->client->ps.torsoTimer = level.unlagged[ent - g_entities].trail[j].torsoTimer;
					ent->client->ps.legsAnim = level.unlagged[ent - g_entities].trail[j].legsAnim;
					ent->client->ps.legsTimer = level.unlagged[ent - g_entities].trail[j].legsTimer;
					ent->s.apos.trBase[YAW] = level.unlagged[ent - g_entities].trail[j].realAngle;
				}

				G_DrawPlayerStick(ent, 0xff0000, Com_Clampi(1, 60, abs(g_unlaggedSkeletonTime.integer)) * 1000, level.time);
			}

			if (g_unlaggedSkeletons.integer & (1 << 2)) {
				VectorCopy(level.unlagged[ent - g_entities].trail[k].mins, ent->r.mins);
				VectorCopy(level.unlagged[ent - g_entities].trail[k].maxs, ent->r.maxs);
				VectorCopy(level.unlagged[ent - g_entities].trail[k].currentOrigin, ent->r.currentOrigin);
				if (timeshiftAnims) {
					ent->client->ps.torsoAnim = level.unlagged[ent - g_entities].trail[k].torsoAnim;
					ent->client->ps.torsoTimer = level.unlagged[ent - g_entities].trail[k].torsoTimer;
					ent->client->ps.legsAnim = level.unlagged[ent - g_entities].trail[k].legsAnim;
					ent->client->ps.legsTimer = level.unlagged[ent - g_entities].trail[k].legsTimer;
					ent->s.apos.trBase[YAW] = level.unlagged[ent - g_entities].trail[k].realAngle;
				}

				G_DrawPlayerStick(ent, 0xffff00, Com_Clampi(1, 60, abs(g_unlaggedSkeletonTime.integer)) * 1000, level.time);
			}
		}
#endif

		// if we haven't wrapped back to the head, we've sandwiched, so
		// we shift the client's position back to where he was at "time"
		if (j != level.unlagged[ent - g_entities].trailHead)
		{
			float	frac = (float)(level.unlagged[ent - g_entities].trail[k].time - time) / (float)(level.unlagged[ent - g_entities].trail[k].time - level.unlagged[ent - g_entities].trail[j].time);

			// FOR TESTING ONLY
#ifdef _DEBUG
			if (g_unlaggedDebug.integer)
				PrintIngame(-1, "time: %d, fire time: %d, j time: %d, k time: %d, diff: %d\n", now, time, level.unlagged[ent - g_entities].trail[j].time, level.unlagged[ent - g_entities].trail[k].time, level.unlagged[ent - g_entities].trail[k].time - level.unlagged[ent - g_entities].trail[j].time);
#endif

			// interpolate between the two origins to give position at time index "time"
			TimeShiftLerp(frac, level.unlagged[ent - g_entities].trail[k].currentOrigin, level.unlagged[ent - g_entities].trail[j].currentOrigin, ent->r.currentOrigin);
			//ent->r.currentAngles[YAW] = LerpAngle( level.unlagged[ent - g_entities].trail[k].currentAngles[YAW], ent->r.currentAngles[YAW], frac );

			// lerp these too, just for fun (and ducking)
			TimeShiftLerp(frac, level.unlagged[ent - g_entities].trail[k].mins, level.unlagged[ent - g_entities].trail[j].mins, ent->r.mins);
			TimeShiftLerp(frac, level.unlagged[ent - g_entities].trail[k].maxs, level.unlagged[ent - g_entities].trail[j].maxs, ent->r.maxs);

			//Lerp this somehow?
			if (timeshiftAnims) {
				/*
				Com_Printf("Lerp timeshifting client %s. Old anim = %i %i (times %i %i).  New Anim = %i %i (times %i %i).\n",
					ent->client->pers.netname,
					ent->client->ps.torsoAnim, ent->client->ps.legsAnim, ent->client->ps.torsoTimer, ent->client->ps.legsTimer,
					level.unlagged[ent - g_entities].trail[k].torsoAnim, level.unlagged[ent - g_entities].trail[k].legsAnim, level.unlagged[ent - g_entities].trail[k].torsoTimer, level.unlagged[ent - g_entities].trail[k].legsTimer);
				*/

				ent->client->ps.torsoAnim = level.unlagged[ent - g_entities].trail[k].torsoAnim;
				ent->client->ps.legsAnim = level.unlagged[ent - g_entities].trail[k].legsAnim;
				TimeShiftAnimLerp(frac, level.unlagged[ent - g_entities].trail[j].torsoAnim, level.unlagged[ent - g_entities].trail[k].torsoAnim, level.unlagged[ent - g_entities].trail[j].torsoTimer, level.unlagged[ent - g_entities].trail[k].torsoTimer, &ent->client->ps.torsoTimer);
				TimeShiftAnimLerp(frac, level.unlagged[ent - g_entities].trail[j].legsAnim, level.unlagged[ent - g_entities].trail[k].legsAnim, level.unlagged[ent - g_entities].trail[j].legsTimer, level.unlagged[ent - g_entities].trail[k].legsTimer, &ent->client->ps.legsTimer);
				//ent->s.apos.trBase[YAW] = LerpAngle( level.unlagged[ent - g_entities].trail[k].realAngle, ent->s.apos.trBase[YAW], frac ); //Shouldnt this be lerping between k and j instead of k and trbase ?
				ent->s.apos.trBase[YAW] = LerpAngle(level.unlagged[ent - g_entities].trail[k].realAngle, level.unlagged[ent - g_entities].trail[j].realAngle, frac); //Shouldnt this be lerping between k and j instead of k and trbase ?

				//Com_Printf("j angle is %.2f k angle is %.2f frac is %.2f\n", level.unlagged[ent - g_entities].trail[j].realAngle, level.unlagged[ent - g_entities].trail[k].realAngle, frac);
				//Com_Printf("interp angle is %.2f\n", LerpAngle( level.unlagged[ent - g_entities].trail[j].realAngle, level.unlagged[ent - g_entities].trail[k].realAngle, frac ));
			}

			// this will recalculate absmin and absmax
			trap_LinkEntity(ent);
		}
		else {
			// we wrapped, so grab the earliest
			//VectorCopy( level.unlagged[ent - g_entities].trail[k].currentAngles, ent->r.currentAngles );
			VectorCopy(level.unlagged[ent - g_entities].trail[k].currentOrigin, ent->r.currentOrigin);
			VectorCopy(level.unlagged[ent - g_entities].trail[k].mins, ent->r.mins);
			VectorCopy(level.unlagged[ent - g_entities].trail[k].maxs, ent->r.maxs);

			if (timeshiftAnims) {
				/*
				Com_Printf("Timeshifting client %s. Old anim = %i %i (times %i %i).  New Anim = %i %i (times %i %i).\n",
					ent->client->pers.netname,
					ent->client->ps.torsoAnim, ent->client->ps.legsAnim, ent->client->ps.torsoTimer, ent->client->ps.legsTimer,
					level.unlagged[ent - g_entities].trail[k].torsoAnim, level.unlagged[ent - g_entities].trail[k].legsAnim, level.unlagged[ent - g_entities].trail[k].torsoTimer, level.unlagged[ent - g_entities].trail[k].legsTimer);
				*/

				ent->client->ps.torsoAnim = level.unlagged[ent - g_entities].trail[k].torsoAnim;
				ent->client->ps.torsoTimer = level.unlagged[ent - g_entities].trail[k].torsoTimer;
				ent->client->ps.legsAnim = level.unlagged[ent - g_entities].trail[k].legsAnim;
				ent->client->ps.legsTimer = level.unlagged[ent - g_entities].trail[k].legsTimer;
				ent->s.apos.trBase[YAW] = level.unlagged[ent - g_entities].trail[k].realAngle;
			}

			// this will recalculate absmin and absmax
			trap_LinkEntity(ent);
		}

#ifdef _DEBUG
		if (g_unlaggedSkeletonTime.integer && g_unlaggedSkeletons.integer & (1 << 3)) {
			G_DrawPlayerStick(ent, 0x00ff00, Com_Clampi(1, 60, abs(g_unlaggedSkeletonTime.integer)) * 1000, level.time);
			//Com_Printf("post angle is %.2f\n", ent->s.apos.trBase[YAW]);
		}
#endif


	}
}

/*
=====================
G_TimeShiftAllClients

Move ALL clients back to where they were at the specified "time",
except for "skip"
=====================
*/
#define UNLAGGED_FACTOR				(0.25)
#define UNLAGGED_FACTOR_NPC			(0.5)
void G_TimeShiftAllClients(int time, gentity_t *skip, qboolean timeshiftAnims) {
	if (skip) {
		if (skip->r.svFlags & SVF_BOT)
			return;
		if (skip->s.eType == ET_NPC)
			return;
	}

#ifdef _DEBUG
	if (g_unlaggedDebug.integer)
		PrintIngame(-1, "G_TimeShiftAllClients: time param %d, factor %f, offset %d, final time ", time, g_unlaggedFactor.value, g_unlaggedOffset.integer);
#endif

	int now = trap_Milliseconds();
	if (time > now) {
		time = now;
	}

#ifdef _DEBUG
	if (g_unlaggedFactor.value) {
		float diff = now - time;
		time += (int)round((double)g_unlaggedFactor.value * (double)diff);
	}
#else
	float diff = now - time;
	if (skip->client->sess.inRacemode) {
		time += (int)round(UNLAGGED_FACTOR_NPC * (double)diff);
	}
	else {
		time += (int)round(UNLAGGED_FACTOR * (double)diff);
	}
#endif

#ifdef _DEBUG
	if (g_unlaggedOffset.integer)
		time += g_unlaggedOffset.integer;
#endif

	if (time > now) {
		time = now;
	}

#ifdef _DEBUG
	if (now - time > g_unlaggedMaxCompensation.integer) {
		time = now - g_unlaggedMaxCompensation.integer;
	}
#else
	if (now - time > UNLAGGED_MAX_COMPENSATION) {
		time = now - UNLAGGED_MAX_COMPENSATION;
	}
#endif

#ifdef _DEBUG
	if (g_unlaggedDebug.integer)
		PrintIngame(-1, "%d, now %d, diff %d\n", time, now, now - time);
#endif

	if (skip && skip->client && skip->client->sess.inRacemode) {
		for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
			gentity_t *ent = &g_entities[i];
			if (ent->client && ent->inuse && ent->isAimPracticePack && ent->NPC) {
				G_TimeShiftNPC(ent, time, timeshiftAnims);
			}
		}
	}
	else {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (ent->client && ent->inuse && ent->client->sess.sessionTeam < TEAM_SPECTATOR && ent != skip && !ent->client->sess.inRacemode) {
				G_TimeShiftClient(ent, time, timeshiftAnims);
			}
		}
	}
}

void G_UnTimeShiftNPC(gentity_t *ent, qboolean timeshiftAnims) {
	VectorCopy(savedNpcTrails[ent - g_entities].currentOrigin, ent->r.currentOrigin);
	VectorCopy(savedNpcTrails[ent - g_entities].mins, ent->r.mins);
	VectorCopy(savedNpcTrails[ent - g_entities].maxs, ent->r.maxs);
	ent->client->ps.torsoAnim = savedNpcTrails[ent - g_entities].torsoAnim;
	ent->client->ps.legsAnim = savedNpcTrails[ent - g_entities].legsAnim;
	ent->client->ps.torsoTimer = savedTorsoTimers[ent - g_entities];
	ent->client->ps.legsTimer = savedLegTimers[ent - g_entities];
	VectorCopy(savedNpcTrails[ent - g_entities].angles, ent->s.apos.trBase);
	VectorCopy(savedNpcTrails[ent - g_entities].velocity, ent->client->ps.velocity);
}

/*
===================
G_UnTimeShiftClient

Move a client back to where he was before the time shift
===================
*/
void G_UnTimeShiftClient(gentity_t *ent, qboolean timeshiftAnims) {
	// if it was saved
	if (level.unlagged[ent - g_entities].saved.time) {
		// move it back
		VectorCopy(level.unlagged[ent - g_entities].saved.mins, ent->r.mins);
		VectorCopy(level.unlagged[ent - g_entities].saved.maxs, ent->r.maxs);
		VectorCopy(level.unlagged[ent - g_entities].saved.currentOrigin, ent->r.currentOrigin);
		//VectorCopy( level.unlagged[ent - g_entities].saved.currentAngles, ent->r.currentAngles );

		if (timeshiftAnims) {
			ent->client->ps.torsoAnim = level.unlagged[ent - g_entities].saved.torsoAnim;
			ent->client->ps.torsoTimer = level.unlagged[ent - g_entities].saved.torsoTimer;
			ent->client->ps.legsAnim = level.unlagged[ent - g_entities].saved.legsAnim;
			ent->client->ps.legsTimer = level.unlagged[ent - g_entities].saved.legsTimer;
			ent->s.apos.trBase[YAW] = level.unlagged[ent - g_entities].saved.realAngle;
		}

		level.unlagged[ent - g_entities].saved.time = 0;

		// this will recalculate absmin and absmax
		trap_LinkEntity(ent);
	}
}

/*
=======================
G_UnTimeShiftAllClients

Move ALL the clients back to where they were before the time shift,
except for "skip"
=======================
*/
void G_UnTimeShiftAllClients(gentity_t *skip, qboolean timeshiftAnims) {
	if (skip) {
		if (skip->r.svFlags & SVF_BOT)
			return;
		if (skip->s.eType == ET_NPC)
			return;
	}

	if (skip && skip->client && skip->client->sess.inRacemode) {
		for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
			gentity_t *ent = &g_entities[i];
			if (ent->client && ent->inuse && ent->isAimPracticePack && ent->NPC) {
				G_UnTimeShiftNPC(ent, timeshiftAnims);
			}
		}
	}
	else {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (ent->client && ent->inuse && ent->client->sess.sessionTeam < TEAM_SPECTATOR && ent != skip && !ent->client->sess.inRacemode) {
				G_UnTimeShiftClient(ent, timeshiftAnims);
			}
		}
	}
}

/*
===============
G_DamageFeedback

Called just before a snapshot is sent to the given player.
Totals up all damage and generates both the player_state_t
damage values to that client for pain blends and kicks, and
global pain sound events for all clients.
===============
*/
void P_DamageFeedback( gentity_t *player ) {
	gclient_t	*client;
	float	count;
	vec3_t	angles;

	client = player->client;
	if ( client->ps.pm_type == PM_DEAD ) {
		return;
	}

	// total points of damage shot at the player this frame
	count = client->damage_blood + client->damage_armor;
	if ( count == 0 ) {
		return;		// didn't take any damage
	}

	if ( count > 255 ) {
		count = 255;
	}

	// send the information to the client

	// world damage (falling, slime, etc) uses a special code
	// to make the blend blob centered instead of positional
	if ( client->damage_fromWorld ) {
		client->ps.damagePitch = 255;
		client->ps.damageYaw = 255;

		client->damage_fromWorld = qfalse;
	} else {
		vectoangles( client->damage_from, angles );
		client->ps.damagePitch = angles[PITCH]/360.0 * 256;
		client->ps.damageYaw = angles[YAW]/360.0 * 256;

		//cap them since we can't send negative values in here across the net
		if (client->ps.damagePitch < 0)
		{
			client->ps.damagePitch = 0;
		}
		if (client->ps.damageYaw < 0)
		{
			client->ps.damageYaw = 0;
		}
	}

	// play an apropriate pain sound
	if ( (level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) && !(player->s.eFlags & EF_DEAD) ) {

		// don't do more than two pain sounds a second
		// nmckenzie: also don't make him loud and whiny if he's only getting nicked.
		if ( level.time - client->ps.painTime < 500 || count < 10) {
			return;
		}
		P_SetTwitchInfo(client);
		player->pain_debounce_time = level.time + 700;

		// *CHANGE 100* anti HP hack - HP is not seen by everyone
		if (!g_protectHPhack.integer){ //0
			G_AddEvent( player, EV_PAIN, player->health );
		} else if(g_protectHPhack.integer == 2){//3
			G_AddEvent( player, EV_PAIN, Q_irand(1, 100) );
		} else if(g_protectHPhack.integer == 3){//2
			int health = player->health;
			if ( health < 25 ) { //0-24
				health = 24;
			} else if ( health < 50 ) { //25-49
				health = 49;
			} else if ( health < 75 ) { //50-74
				health = 74;
			} else { //75 +
				health = 100;
			}
			G_AddEvent( player, EV_PAIN, health );
		} else {//1
			G_AddEvent( player, EV_PAIN, 100 );
		}

		client->ps.damageEvent++;

		if (client->damage_armor && !client->damage_blood)
		{
			client->ps.damageType = 1; //pure shields
		}
		else if (client->damage_armor)
		{
			client->ps.damageType = 2; //shields and health
		}
		else
		{
			client->ps.damageType = 0; //pure health
		}
	}


	client->ps.damageCount = count;

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_armor = 0;
	client->damage_knockback = 0;
}



/*
=============
P_WorldEffects

Check for lava / slime contents and drowning
=============
*/
void P_WorldEffects( gentity_t *ent ) {
	qboolean	envirosuit;
	int			waterlevel;

	if ( ent->client->noclip ) {
		ent->client->airOutTime = level.time + 12000;	// don't need air
		return;
	}

	waterlevel = ent->waterlevel;

	envirosuit = ent->client->ps.powerups[PW_BATTLESUIT] > level.time;

	//
	// check for drowning
	//
	if ( waterlevel == 3 ) {
		// envirosuit give air
		if ( envirosuit ) {
			ent->client->airOutTime = level.time + 10000;
		}

		// if out of air, start drowning
		if ( ent->client->airOutTime < level.time) {
			// drown!
			ent->client->airOutTime += 1000;
			if ( ent->health > 0 ) {
				// take more damage the longer underwater
				ent->damage += 2;
				if (ent->damage > 15)
					ent->damage = 15;

				// play a gurp sound instead of a normal pain sound
				if (ent->health <= ent->damage) {
					G_Sound(ent, CHAN_VOICE, G_SoundIndex(/*"*drown.wav"*/"sound/player/gurp1.wav"));
				} else if (rand()&1) {
					G_Sound(ent, CHAN_VOICE, G_SoundIndex("sound/player/gurp1.wav"));
				} else {
					G_Sound(ent, CHAN_VOICE, G_SoundIndex("sound/player/gurp2.wav"));
				}

				// don't play a normal pain sound
				ent->pain_debounce_time = level.time + 200;

				G_Damage (ent, NULL, NULL, NULL, NULL, 
					ent->damage, DAMAGE_NO_ARMOR, MOD_WATER);
			}
		}
	} else {
		ent->client->airOutTime = level.time + 12000;
		ent->damage = 2;
	}

	//
	// check for sizzle damage (move to pmove?)
	//
	if (waterlevel && 
		(ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) ) {
		if (ent->health > 0
			&& ent->pain_debounce_time <= level.time	) {

			if (g_fixLava.integer) {
				if (ent->NPC) { // kill it now
					vec3_t vDir;
					VectorSet(vDir, 0, 1, 0);
					G_Damage(ent, ent, ent, vDir, ent->client->ps.origin, Q3_INFINITE, 0, MOD_FALLING);
				}
				else {
					if (!ent->client->ps.fallingToDeath) {
						ent->client->ps.fallingToDeath = level.time;
						ent->client->ps.otherKillerTime = level.time + 20000;
						ent->client->ps.otherKillerDebounceTime = level.time + 10000;
						ent->client->ps.eFlags |= EF_RAG;
						Jetpack_Off(ent);
						G_EntitySound(ent, CHAN_VOICE, G_SoundIndex("*falling1.wav"));
					}
				}
			}
			else {
				if (envirosuit) {
					G_AddEvent(ent, EV_POWERUP_BATTLESUIT, 0);
				}
				else {
					if (ent->watertype & CONTENTS_LAVA) {
						G_Damage(ent, NULL, NULL, NULL, NULL,
							30 * waterlevel, 0, MOD_LAVA);
					}

					if (ent->watertype & CONTENTS_SLIME) {
						G_Damage(ent, NULL, NULL, NULL, NULL,
							10 * waterlevel, 0, MOD_SLIME);
					}
				}
			}
		}
	}
}





//==============================================================
extern void G_ApplyKnockback( gentity_t *targ, vec3_t newDir, float knockback );
void DoImpact( gentity_t *self, gentity_t *other, qboolean damageSelf )
{
	float magnitude, my_mass;
	vec3_t	velocity;
	int cont;
	qboolean easyBreakBrush = qtrue;

	if( self->client )
	{
		VectorCopy( self->client->ps.velocity, velocity );
		if( !self->mass )
		{
			my_mass = 10;
		}
		else
		{
			my_mass = self->mass;
		}
	}
	else 
	{
		VectorCopy( self->s.pos.trDelta, velocity );
		if ( self->s.pos.trType == TR_GRAVITY )
		{
			velocity[2] -= 0.25f * g_gravity.value;
		}
		if( !self->mass )
		{
			my_mass = 1;
		}
		else if ( self->mass <= 10 )
		{
			my_mass = 10;
		}
		else
		{
			my_mass = self->mass;
		}
	}

	magnitude = VectorLength( velocity ) * my_mass / 10;

	if ( other->material == MAT_GLASS 
		|| other->material == MAT_GLASS_METAL 
		|| other->material == MAT_GRATE1
		|| ((other->flags&FL_BBRUSH)&&(other->spawnflags&8))
		|| (other->r.svFlags&SVF_GLASS_BRUSH) )
	{
		easyBreakBrush = qtrue;
	}

	if ( !self->client || self->client->ps.lastOnGround+300<level.time || ( self->client->ps.lastOnGround+100 < level.time && easyBreakBrush ) )
	{
		vec3_t dir1, dir2;
		float force = 0, dot;

		if ( easyBreakBrush )
			magnitude *= 2;

		//damage them
		if ( magnitude >= 100 && other->s.number < ENTITYNUM_WORLD )
		{
			VectorCopy( velocity, dir1 );
			VectorNormalize( dir1 );
			if( VectorCompare( other->r.currentOrigin, vec3_origin ) )
			{//a brush with no origin
				VectorCopy ( dir1, dir2 );
			}
			else
			{
				VectorSubtract( other->r.currentOrigin, self->r.currentOrigin, dir2 );
				VectorNormalize( dir2 );
			}

			dot = DotProduct( dir1, dir2 );

			if ( dot >= 0.2 )
			{
				force = dot;
			}
			else
			{
				force = 0;
			}

			force *= (magnitude/50);

			cont = trap_PointContents( other->r.absmax, other->s.number );
			if( (cont&CONTENTS_WATER) )//|| (self.classname=="barrel"&&self.aflag))//FIXME: or other watertypes
			{
				force /= 3;							//water absorbs 2/3 velocity
			}

			if( ( force >= 1 && other->s.number >= MAX_CLIENTS ) || force >= 10)
			{

				if ( other->r.svFlags & SVF_GLASS_BRUSH )
				{
					other->splashRadius = (float)(self->r.maxs[0] - self->r.mins[0])/4.0f;
				}
				if ( other->takedamage )
				{
					G_Damage( other, self, self, velocity, self->r.currentOrigin, force, DAMAGE_NO_ARMOR, MOD_CRUSH);//FIXME: MOD_IMPACT
				}
				else
				{
					G_ApplyKnockback( other, dir2, force );
				}
			}
		}

		if ( damageSelf && self->takedamage )
		{
			//Now damage me
			//FIXME: more lenient falling damage, especially for when driving a vehicle
			if ( self->client && self->client->ps.fd.forceJumpZStart )
			{//we were force-jumping
				if ( self->r.currentOrigin[2] >= self->client->ps.fd.forceJumpZStart )
				{//we landed at same height or higher than we landed
					magnitude = 0;
				}
				else
				{//FIXME: take off some of it, at least?
					magnitude = (self->client->ps.fd.forceJumpZStart-self->r.currentOrigin[2])/3;
				}
			}
			//if(self.classname!="monster_mezzoman"&&self.netname!="spider")//Cats always land on their feet
				if( ( magnitude >= 100 + self->health && self->s.number >= MAX_CLIENTS && self->s.weapon != WP_SABER ) || ( magnitude >= 700 ) )//&& self.safe_time < level.time ))//health here is used to simulate structural integrity
				{
					if ( (self->s.weapon == WP_SABER) && self->client && self->client->ps.groundEntityNum < ENTITYNUM_NONE && magnitude < 1000 )
					{//players and jedi take less impact damage
						//allow for some lenience on high falls
						magnitude /= 2;
						}
					magnitude /= 40;
					magnitude = magnitude - force/2;//If damage other, subtract half of that damage off of own injury
					if ( magnitude >= 1 )
					{
						G_Damage( self, NULL, NULL, NULL, self->r.currentOrigin, magnitude/2, DAMAGE_NO_ARMOR, MOD_FALLING );//FIXME: MOD_IMPACT
					}
				}
		}
	}
}

void Client_CheckImpactBBrush( gentity_t *self, gentity_t *other )
{
	if ( !other || !other->inuse )
	{
		return;
	}
	if (!self || !self->inuse || !self->client ||
		self->client->tempSpectate >= level.time ||
		self->client->sess.sessionTeam == TEAM_SPECTATOR ||
		self->client->sess.inRacemode ||
		self->isAimPracticePack)
	{ //hmm.. let's not let spectators ram into breakables.
		return;
	}

	if ( (other->material == MAT_GLASS 
		|| other->material == MAT_GLASS_METAL 
		|| other->material == MAT_GRATE1
		|| ((other->flags&FL_BBRUSH)&&(other->spawnflags&8/*THIN*/))
		|| ((other->flags&FL_BBRUSH)&&(other->health<=10))
		|| (other->r.svFlags&SVF_GLASS_BRUSH))
		&& !(self->client->ps.pm_flags&PMF_STUCK_TO_WALL)) //breaking glass bug
	{//clients only do impact damage against easy-break breakables
		DoImpact( self, other, qfalse );
	}
}


/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound( gentity_t *ent ) {
	if (ent->client && ent->client->isHacking)
	{ //loop hacking sound
		ent->client->ps.loopSound = level.snd_hack;
		ent->s.loopIsSoundset = qfalse;
	}
	else if (ent->client && ent->client->isMedHealed > level.time)
	{ //loop healing sound
		ent->client->ps.loopSound = level.snd_medHealed;
		ent->s.loopIsSoundset = qfalse;
	}
	else if (ent->client && ent->client->isMedSupplied > level.time)
	{ //loop supplying sound
		ent->client->ps.loopSound = level.snd_medSupplied;
		ent->s.loopIsSoundset = qfalse;
	}
	else if (ent->waterlevel && (ent->watertype&(CONTENTS_LAVA|CONTENTS_SLIME)) ) {
		ent->client->ps.loopSound = level.snd_fry;
		ent->s.loopIsSoundset = qfalse;
	} else {
		ent->client->ps.loopSound = 0;
		ent->s.loopIsSoundset = qfalse;
	}
}



//==============================================================

/*
==============
ClientImpacts
==============
*/
void ClientImpacts( gentity_t *ent, pmove_t *pm ) {
	int		i, j;
	trace_t	trace;
	gentity_t	*other;

	memset( &trace, 0, sizeof( trace ) );
	for (i=0 ; i<pm->numtouch ; i++) {
		for (j=0 ; j<i ; j++) {
			if (pm->touchents[j] == pm->touchents[i] ) {
				break;
			}
		}
		if (j != i) {
			continue;	// duplicated
		}
		other = &g_entities[ pm->touchents[i] ];

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, other, &trace );
		}

		if ( !other->touch ) {
			continue;
		}

		other->touch( other, ent, &trace );
	}

}

/*
============
G_TouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
extern void Touch_RaceTrigger( gentity_t *trigger, gentity_t *player, trace_t *trace );
extern void hurt_touch( gentity_t *self, gentity_t *other, trace_t *trace );
void	G_TouchTriggers( gentity_t *ent ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	trace_t		trace;
	vec3_t		mins, maxs;
	static vec3_t	range = { 40, 40, 52 };

	if ( !ent->client ) {
		return;
	}

	// dead clients don't activate triggers!
	if ( ent->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return;
	}

	VectorSubtract( ent->client->ps.origin, range, mins );
	VectorAdd( ent->client->ps.origin, range, maxs );

	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	// can't use ent->r.absmin, because that has a one unit pad
	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );

	for ( i=0 ; i<num ; i++ ) {
		hit = &g_entities[touch[i]];

		if ( !hit->touch && !ent->touch ) {
			continue;
		}
		if ( !( hit->r.contents & CONTENTS_TRIGGER )
            && (hit->s.eType != ET_SPECIAL) )
        {
			continue;
		}

		if (ent->isAimPracticePack)
			continue;

		// ignore most entities if a spectator
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			if ( hit->s.eType != ET_TELEPORT_TRIGGER &&
				// this is ugly but adding a new ET_? type will
				// most likely cause network incompatibilities
				hit->touch != Touch_DoorTrigger) {
				continue;
			}
		}

		// racemode: no trigger except a few

		qboolean isRaceTrigger = hit->touch == Touch_RaceTrigger; // avoids a strcmp every time

		if ( isRaceTrigger && !ent->client->sess.inRacemode ) {
			continue; // non racemode clients don't interact with race triggers
		}

		if ( ent->client->sess.inRacemode ) {
			if ( hit->s.eType != ET_TELEPORT_TRIGGER &&
				hit->s.eType != ET_PUSH_TRIGGER &&
				!isRaceTrigger &&
				hit->touch != hurt_touch ) {
				continue;
			}
		}

		// use seperate code for determining if an item is picked up
		// so you don't have to actually contact its bounding box
		// racemode - use item logic for race triggers to match flag hitboxes,
		// unless the race trigger has no hit->count which means it wasn't based on a flag
		if ( hit->s.eType == ET_ITEM || ( isRaceTrigger && hit->count ) ) {
			if ( !BG_PlayerTouchesItem( &ent->client->ps, &hit->s, level.time ) ) {
				continue;
			}
		} else {
			if ( !trap_EntityContact( mins, maxs, hit ) ) {
				continue;
			}
		}

		memset( &trace, 0, sizeof(trace) );

		if ( hit->touch ) {
			hit->touch (hit, ent, &trace);
		}

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, hit, &trace );
		}
	}

	// if we didn't touch a jump pad this pmove frame
	if ( ent->client->ps.jumppad_frame != ent->client->ps.pmove_framecount ) {
		ent->client->ps.jumppad_frame = 0;
		ent->client->ps.jumppad_ent = 0;
	}
}


/*
============
G_MoverTouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void G_MoverTouchPushTriggers( gentity_t *ent, vec3_t oldOrg ) 
{
	int			i, num;
	float		step, stepSize, dist;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	trace_t		trace;
	vec3_t		mins, maxs, dir, size, checkSpot;
	const vec3_t	range = { 40, 40, 52 };

	// non-moving movers don't hit triggers!
	if ( !VectorLengthSquared( ent->s.pos.trDelta ) ) 
	{
		return;
	}

	VectorSubtract( ent->r.mins, ent->r.maxs, size );
	stepSize = VectorLength( size );
	if ( stepSize < 1 )
	{
		stepSize = 1;
	}

	VectorSubtract( ent->r.currentOrigin, oldOrg, dir );
	dist = VectorNormalize( dir );
	for ( step = 0; step <= dist; step += stepSize )
	{
		VectorMA( ent->r.currentOrigin, step, dir, checkSpot );
		VectorSubtract( checkSpot, range, mins );
		VectorAdd( checkSpot, range, maxs );

		num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

		// can't use ent->r.absmin, because that has a one unit pad
		VectorAdd( checkSpot, ent->r.mins, mins );
		VectorAdd( checkSpot, ent->r.maxs, maxs );

		for ( i=0 ; i<num ; i++ ) 
		{
			hit = &g_entities[touch[i]];

			if ( hit->s.eType != ET_PUSH_TRIGGER )
			{
				continue;
			}

			if ( hit->touch == NULL ) 
			{
				continue;
			}

			if ( !( hit->r.contents & CONTENTS_TRIGGER ) ) 
			{
				continue;
			}


			if ( !trap_EntityContact( mins, maxs, hit ) ) 
			{
				continue;
			}

			memset( &trace, 0, sizeof(trace) );

			if ( hit->touch != NULL ) 
			{
				hit->touch(hit, ent, &trace);
			}
		}
	}
}

extern ctfPosition_t PermutationHasPlayer(int accountId, permutationOfTeams_t *p);
static qboolean PlayerIsInTeamInActivePugProposal(int accountId) {
	if (!level.activePugProposal)
		return qfalse;
	if (PermutationHasPlayer(accountId, &level.activePugProposal->suggested))
		return qtrue;
	if (PermutationHasPlayer(accountId, &level.activePugProposal->highestCaliber))
		return qtrue;
	if (PermutationHasPlayer(accountId, &level.activePugProposal->fairest))
		return qtrue;
	if (PermutationHasPlayer(accountId, &level.activePugProposal->desired))
		return qtrue;
	if (PermutationHasPlayer(accountId, &level.activePugProposal->semiDesired))
		return qtrue;
	if (PermutationHasPlayer(accountId, &level.activePugProposal->inclusive))
		return qtrue;
	if (PermutationHasPlayer(accountId, &level.activePugProposal->firstChoice))
		return qtrue;
	return qfalse;
}

static int toldCantMoveTime[MAX_GENTITIES] = { 0 };
static int firstToldCantMoveTime[MAX_GENTITIES] = { 0 };

/*
=================
SpectatorThink
=================
*/
void SpectatorThink( gentity_t *ent, usercmd_t *ucmd ) {
	pmove_t	pm;
	gclient_t	*client;

	client = ent->client;

	int frozen = 0;
	if (g_vote_freezeUntilVote.integer && !ent->isAimPracticePack && client->pers.connected == CON_CONNECTED && !level.intermissionQueued && !level.intermissiontime) {
		if (level.voteTime && level.multiVoting && (ent->client->mGameFlags & PSG_CANVOTE) && !(ent->client->mGameFlags & PSG_VOTED) && (g_vote_freezeUntilVote.integer & (1 << 0))) {
			frozen |= 1;
		}
		if (level.activePugProposal && client->account && (g_vote_freezeUntilVote.integer & (1 << 1))) {
			qboolean partOfActiveProposal = qfalse;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				sortedClient_t *cl = level.activePugProposal->clients + i;
				if (cl->accountName[0] && cl->accountId == client->account->id) {
					partOfActiveProposal = qtrue;
					break;
				}
			}

			if (partOfActiveProposal) {
				const qboolean actuallyOnATeam = PlayerIsInTeamInActivePugProposal(client->account->id);

				if (actuallyOnATeam) {
					qboolean hasVoted = qfalse;
					if (level.activePugProposal->suggested.valid && level.activePugProposal->suggestedVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->highestCaliber.valid && level.activePugProposal->highestCaliberVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->fairest.valid && level.activePugProposal->fairestVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->desired.valid && level.activePugProposal->desiredVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->semiDesired.valid && level.activePugProposal->semiDesiredVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->inclusive.valid && level.activePugProposal->inclusiveVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->firstChoice.valid && level.activePugProposal->firstChoiceVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->suggested.valid && level.activePugProposal->suggestedVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->highestCaliber.valid && level.activePugProposal->highestCaliberVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->fairest.valid && level.activePugProposal->fairestVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->desired.valid && level.activePugProposal->desiredVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->semiDesired.valid && level.activePugProposal->semiDesiredVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->inclusive.valid && level.activePugProposal->inclusiveVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->firstChoice.valid && level.activePugProposal->firstChoiceVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->votedToRerollClients & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->votedToCancelClients & (1 << ent->s.number))
						hasVoted = qtrue;

					if (!hasVoted)
						frozen |= 2;
				}
			}
		}
	}

	if (frozen) {
		const int now = trap_Milliseconds();
		if (!toldCantMoveTime[ent->s.number] || now - toldCantMoveTime[ent->s.number] >= 15000) {
			toldCantMoveTime[ent->s.number] = now;
			firstToldCantMoveTime[ent->s.number] = now;
		}
		else {
			if (now - firstToldCantMoveTime[ent->s.number] >= 60000 && now - toldCantMoveTime[ent->s.number] >= 1000) {
				if (!((frozen & 1) && level.voteBanPhase)) {
					if ((frozen & 2) && !(frozen & 1))
						trap_SendServerCommand(ent->s.number, "cp \"^1VOTA ^2VOTA ^3VOTA ^4VOTA ^5VOTA ^6VOTA ^7VOTA ^8VOTA ^9VOTA ^0VOTA\n^1VOTA ^2VOTA ^3VOTA ^4VOTA ^5VOTA ^6VOTA ^7VOTA ^8VOTA ^9VOTA ^0VOTA\"");
					else
						TeamGenerator_QueueServerMessageInChat(ent->s.number, "^1VOTA ^2VOTA ^3VOTA ^4VOTA ^5VOTA ^6VOTA ^7VOTA ^8VOTA ^9VOTA ^0VOTA ^1VOTA ^2VOTA ^3VOTA ^4VOTA ^5VOTA ^6VOTA ^7VOTA ^8VOTA ^9VOTA ^0VOTA");
				}
				toldCantMoveTime[ent->s.number] = now;

				if (g_vote_fadeToBlack.integer <= 4) {
					if (!client->ps.fallingToDeath)
						client->ps.fallingToDeath = level.time - 2000;
					client->pers.fakeFallFadeToBlack = qtrue;
				}

				if (g_vote_audioMotivation.integer && !((frozen & 1) && level.voteBanPhase)) {
					int index = G_SoundIndex("sound/chars/r2d2/misc/pain100");
					if (index) {
						gentity_t *te = G_Sound(ent, CHAN_AUTO, index);
						te->r.svFlags |= SVF_SINGLECLIENT;
						te->r.singleClient = ent->s.number;
						te->r.svFlags &= ~SVF_BROADCAST;
					}


					index = G_SoundIndex("sound/chars/tusken/misc/anger1");
					if (index) {
						gentity_t *te = G_Sound(ent, CHAN_AUTO, index);
						te->r.svFlags |= SVF_SINGLECLIENT;
						te->r.singleClient = ent->s.number;
						te->r.svFlags &= ~SVF_BROADCAST;
					}

					index = G_SoundIndex("sound/chars/howler/howl");
					if (index) {
						gentity_t *te = G_Sound(ent, CHAN_AUTO, index);
						te->r.svFlags |= SVF_SINGLECLIENT;
						te->r.singleClient = ent->s.number;
						te->r.svFlags &= ~SVF_BROADCAST;
					}

					index = G_SoundIndex("sound/chars/howler/misc/pain100");
					if (index) {
						gentity_t *te = G_Sound(ent, CHAN_AUTO, index);
						te->r.svFlags |= SVF_SINGLECLIENT;
						te->r.singleClient = ent->s.number;
						te->r.svFlags &= ~SVF_BROADCAST;
					}

					index = G_SoundIndex("sound/chars/howler/misc/death1");
					if (index) {
						gentity_t *te = G_Sound(ent, CHAN_AUTO, index);
						te->r.svFlags |= SVF_SINGLECLIENT;
						te->r.singleClient = ent->s.number;
						te->r.svFlags &= ~SVF_BROADCAST;
					}
				}
			}
			else if (now - firstToldCantMoveTime[ent->s.number] >= 30000 && now - toldCantMoveTime[ent->s.number] >= 3000) {
				if (!((frozen & 1) && level.voteBanPhase)) {
					if ((frozen & 2) && !(frozen & 1))
						trap_SendServerCommand(ent->s.number, "cp \"VOTE!\"");
					else
						TeamGenerator_QueueServerMessageInChat(ent->s.number, "VOTE!");
				}
				toldCantMoveTime[ent->s.number] = now;

				if (g_vote_fadeToBlack.integer <= 3) {
					if (!client->ps.fallingToDeath)
						client->ps.fallingToDeath = level.time - 2000;
					client->pers.fakeFallFadeToBlack = qtrue;
				}
			}
			else if (now - toldCantMoveTime[ent->s.number] >= 10000) {
				if (!((frozen & 1) && level.voteBanPhase)) {
					if ((frozen & 2) && !(frozen & 1))
						trap_SendServerCommand(ent->s.number, "cp \"Vote, and you will be unfrozen.\"");
					else
						TeamGenerator_QueueServerMessageInChat(ent->s.number, "Vote, and you will be unfrozen.");
				}
				toldCantMoveTime[ent->s.number] = now;

				if (g_vote_fadeToBlack.integer <= 2) {
					if (!client->ps.fallingToDeath)
						client->ps.fallingToDeath = level.time - 2000;
					client->pers.fakeFallFadeToBlack = qtrue;
				}
			}
		}

		if (g_vote_fadeToBlack.integer == 1 || (client->account && (client->account->flags & ACCOUNTFLAG_NONVOTINGMEMER))) {
			if (!client->ps.fallingToDeath)
				client->ps.fallingToDeath = level.time - 2000;
			client->pers.fakeFallFadeToBlack = qtrue;
		}
	}
	else {
		toldCantMoveTime[ent->s.number] = firstToldCantMoveTime[ent->s.number] = 0;
		if (client->pers.fakeFallFadeToBlack) {
			client->pers.fakeFallFadeToBlack = qfalse;
			client->ps.fallingToDeath = 0;
		}
	}

	if (frozen && client->sess.spectatorState != SPECTATOR_FOLLOW) {
		ucmd->buttons = 0;
		ucmd->forwardmove = 0;
		ucmd->rightmove = 0;
		ucmd->upmove = 0;
		ucmd->generic_cmd = 0;
		client->ps.speed = 0;
		client->ps.basespeed = 0;
	}

	if ( client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		client->ps.pm_type = PM_SPECTATOR;
		client->ps.speed = frozen ? 0 : 400;	// faster than normal
		client->ps.basespeed = frozen ? 0 : 400;

		//hmm, shouldn't have an anim if you're a spectator, make sure
		//it gets cleared.
		client->ps.legsAnim = 0;
		client->ps.legsTimer = 0;
		client->ps.torsoAnim = 0;
		client->ps.torsoTimer = 0;

		// set up for pmove
		memset (&pm, 0, sizeof(pm));
		pm.ps = &client->ps;
		pm.cmd = *ucmd;
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;	// spectators can fly through bodies
		pm.trace = trap_Trace;
		pm.pointcontents = trap_PointContents;

		pm.noSpecMove = frozen ? 1 : g_noSpecMove.integer;

		pm.animations = NULL;
		pm.nonHumanoid = qfalse;

		//Set up bg entity data
		pm.baseEnt = (bgEntity_t *)g_entities;
		pm.entSize = sizeof(gentity_t);

		// perform a pmove
		Pmove (&pm);

		// save results of pmove
		VectorCopy( client->ps.origin, ent->s.origin );

		if (ent->client->tempSpectate < level.time)
		{
			G_TouchTriggers( ent );
		}

		trap_UnlinkEntity( ent );
	}

	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;

	if (client->tempSpectate < level.time)
	{
		// attack button cycles through spectators
		if ( ( client->buttons & BUTTON_ATTACK ) && ! ( client->oldbuttons & BUTTON_ATTACK ) && !frozen) {
Cmd_FollowCycle_f(ent, 1);
		}
 else if (client->sess.spectatorState == SPECTATOR_FOLLOW &&
 (client->buttons & BUTTON_ALT_ATTACK) && !(client->oldbuttons & BUTTON_ALT_ATTACK) && !frozen) {
 Cmd_FollowCycle_f(ent, -1);
		}
 else if (ucmd->generic_cmd == GENCMD_SABERATTACKCYCLE && !frozen) {
 // saberattackcycle cycles flag carriers
 Cmd_FollowFlag_f(ent);
		}
 else if ((client->buttons & BUTTON_USE) && !(client->oldbuttons & BUTTON_USE) && !frozen) {
 Cmd_FollowTarget_f(ent);
		}

		if (client->sess.spectatorState == SPECTATOR_FOLLOW && (ucmd->upmove > 0) && !frozen)
		{ //jump now removes you from follow mode
			StopFollowing(ent);
		}
	}
}

// returns global time in ms, this should persist between server restarts
int getGlobalTime()
{
	return (int)time(0);
}

qboolean IsMoving(const gclient_t *client) {
	if (!client) {
		assert(qfalse);
		return qfalse;
	}

	if (client->pers.cmd.forwardmove || client->pers.cmd.rightmove || client->pers.cmd.upmove)
		return qtrue;

	return qfalse;
}

qboolean IsInputting(const gclient_t *client, qboolean checkPressingButtons, qboolean checkMovingMouse, qboolean checkPressingChatButton) {
	if (!client) {
		assert(qfalse);
		return qfalse;
	}

	if (checkPressingButtons) {
		if (client->pers.cmd.forwardmove || client->pers.cmd.rightmove || client->pers.cmd.upmove ||
			client->pers.cmd.buttons & BUTTON_ATTACK || client->pers.cmd.buttons & BUTTON_ALT_ATTACK || client->pers.cmd.generic_cmd) {
			return qtrue;
		}
	}

	if (checkMovingMouse) {
		if ((short)client->pers.cmd.angles[0] != (short)client->pers.lastCmd.angles[0] ||
			(short)client->pers.cmd.angles[1] != (short)client->pers.lastCmd.angles[1] ||
			(short)client->pers.cmd.angles[2] != (short)client->pers.lastCmd.angles[2]) {
			return qtrue;
		}
	}

	if (checkPressingChatButton) {
		if ((client->pers.cmd.buttons ^ client->pers.lastCmd.buttons) & BUTTON_TALK) {
			return qtrue;
		}
	}

	return qfalse;
}

/*
=================
ClientInactivityTimer

Returns qfalse if the spectator is dropped
=================
*/
qboolean CheckSpectatorInactivityTimer(gclient_t *client)
{
	if (!g_spectatorInactivity.integer) {
		// give everyone some time, so if the operator sets g_inactivity during
		// gameplay, everyone isn't kicked

		client->sess.inactivityTime = getGlobalTime() + 60;
		client->inactivityWarning = qfalse;
	}
	else if (IsInputting(client, qtrue, qtrue, qtrue) || client->sess.spectatorState == SPECTATOR_FOLLOW) {
		client->sess.inactivityTime = getGlobalTime() + g_spectatorInactivity.integer;

		client->inactivityWarning = qfalse;
	}
	else if (!client->pers.localClient) {
		if (getGlobalTime() > client->sess.inactivityTime) {
			// kick them..
			trap_DropClient(client - level.clients, "dropped due to inactivity");
		}

		if ((getGlobalTime() > (client->sess.inactivityTime - 10)) && (!client->inactivityWarning)) {
			client->inactivityWarning = qtrue;
			trap_SendServerCommand(client - level.clients, "cp \"Ten seconds until inactivity drop!\n\"");
		}
	}

	// remember current commands for next cycle diff
	client->pers.lastCmd = client->pers.cmd;

	return qtrue;
}

/*
=================
ClientInactivityTimer

Returns qfalse if the client is dropped/force specced
=================
*/
qboolean CheckPlayerInactivityTimer(gclient_t *client) {
    qboolean active = qtrue;

	if (IsMoving(client)) {
		client->pers.lastMoveTime = getGlobalTime();
		client->pers.lastMoveLevelTime = level.time;
	}

	if (IsInputting(client, qtrue, qtrue, qtrue)) {
		client->pers.lastInputTime = getGlobalTime();
		client->pers.lastInputLevelTime = level.time;
	}

	if (level.voteAutoPassOnExpiration && level.voteTime && level.voteStartTime && g_vote_teamgen_autoMapVoteNonAfkAutoVoteYesSeconds.integer > 0 && !client->sess.inRacemode) {
		int msWait = g_vote_teamgen_autoMapVoteNonAfkAutoVoteYesSeconds.integer * 1000;
		if (level.time - level.voteStartTime >= msWait && client->pers.lastInputLevelTime >= level.voteStartTime && !(client->mGameFlags & PSG_VOTED)) {
			G_LogPrintf("Client %i (%s) automatically voted YES due to not being AFK during auto-pass vote\n", client - level.clients, client->pers.netname);
			level.voteYes++;
			trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
			trap_SendServerCommand(client - level.clients, "print \"Vote automatically cast (not AFK).\n\"");
			client->mGameFlags |= PSG_VOTED;
			client->mGameFlags |= PSG_VOTEDYES;
			client->mGameFlags &= ~PSG_VOTEDNO;
		}
	}

	// match start AFK detection
	if (!client->pers.hasDoneSomething && IsInputting(client, qtrue, qfalse, qfalse)) {
		client->pers.hasDoneSomething = qtrue;
	}

    if (!g_inactivity.integer) {
        // give everyone some time, so if the operator sets g_inactivity during
        // gameplay, everyone isn't kicked
        client->sess.inactivityTime = getGlobalTime() + 60;
        client->inactivityWarning = qfalse;
    } 
    else if (IsInputting(client, qtrue, qtrue, qtrue)) {
        client->sess.inactivityTime = getGlobalTime() + g_inactivity.integer;
        client->inactivityWarning = qfalse;
    }
    else if (!client->pers.localClient) {
        if (getGlobalTime() > client->sess.inactivityTime) {
            if (g_inactivityKick.integer) {
                // kick them..
                trap_DropClient(client - level.clients, "dropped due to inactivity");
            }
            else
            {
                // just force them to spec
                trap_SendServerCommand(-1, va("print \"%s "S_COLOR_WHITE"forced to spectators due to inactivity\n\"", client->pers.netname));
                trap_SendConsoleCommand(EXEC_APPEND, va("forceteam %i spectator\n", client - level.clients));
                client->sess.inactivityTime = getGlobalTime() + 1;
            }

            active = qfalse;
        } 
        else if ((getGlobalTime() > (client->sess.inactivityTime - 10)) && (!client->inactivityWarning)) {
            client->inactivityWarning = qtrue;
            trap_SendServerCommand(client - level.clients, "cp \"Ten seconds until inactivity drop!\n\"");
        }
    }

    // remember current commands for next cycle diff
    client->pers.lastCmd = client->pers.cmd;

    return active;
}

/*
==================
ClientTimerActions

Actions that happen once a second
==================
*/
void ClientTimerActions( gentity_t *ent, int msec ) {
	gclient_t	*client;

	client = ent->client;
	client->timeResidual += msec;

	while ( client->timeResidual >= 1000 ) 
	{
		client->timeResidual -= 1000;

		// count down health when over max
		if ( ent->health > client->ps.stats[STAT_MAX_HEALTH] ) {
			ent->health--;
		}

		// count down armor when over max
		if ( client->ps.stats[STAT_ARMOR] > client->ps.stats[STAT_MAX_HEALTH] ) {
			client->ps.stats[STAT_ARMOR]--;
		}
	}
}

/*
====================
ClientIntermissionThink
====================
*/
void ClientIntermissionThink( gclient_t *client ) {
	client->ps.eFlags &= ~EF_TALK;
	client->ps.eFlags &= ~EF_FIRING;

	// the level will exit when everyone wants to or after timeouts

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = client->pers.cmd.buttons;
	if ( client->buttons & ( BUTTON_ATTACK | BUTTON_USE_HOLDABLE ) & ( client->oldbuttons ^ client->buttons ) ) {
		// this used to be an ^1 but once a player says ready, it should stick
		client->readyToExit = 1;
	}
}

extern void NPC_SetAnim(gentity_t	*ent,int setAnimParts,int anim,int setAnimFlags);
void G_VehicleAttachDroidUnit( gentity_t *vehEnt )
{
	if ( vehEnt && vehEnt->m_pVehicle && vehEnt->m_pVehicle->m_pDroidUnit != NULL )
	{
		gentity_t *droidEnt = (gentity_t *)vehEnt->m_pVehicle->m_pDroidUnit;
		mdxaBone_t boltMatrix;
		vec3_t	fwd;

		trap_G2API_GetBoltMatrix(vehEnt->ghoul2, 0, vehEnt->m_pVehicle->m_iDroidUnitTag, &boltMatrix, vehEnt->r.currentAngles, vehEnt->r.currentOrigin, level.time,
			NULL, vehEnt->modelScale);
		BG_GiveMeVectorFromMatrix(&boltMatrix, ORIGIN, droidEnt->r.currentOrigin);
		BG_GiveMeVectorFromMatrix(&boltMatrix, NEGATIVE_Y, fwd);
		vectoangles( fwd, droidEnt->r.currentAngles );
		
		if ( droidEnt->client )
		{
			VectorCopy( droidEnt->r.currentAngles, droidEnt->client->ps.viewangles );
			VectorCopy( droidEnt->r.currentOrigin, droidEnt->client->ps.origin );
		}

		G_SetOrigin( droidEnt, droidEnt->r.currentOrigin );
		trap_LinkEntity( droidEnt );
		
		if ( droidEnt->NPC )
		{
			NPC_SetAnim( droidEnt, SETANIM_BOTH, BOTH_STAND2, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
		}
	}
}

//called gameside only from pmove code (convenience)
void G_CheapWeaponFire(int entNum, int ev)
{
	gentity_t *ent = &g_entities[entNum];
	
	if (!ent->inuse || !ent->client)
	{
		return;
	}

	switch (ev)
	{
		case EV_FIRE_WEAPON:
			if (ent->m_pVehicle && ent->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER &&
				ent->client && ent->client->ps.m_iVehicleNum)
			{ //a speeder with a pilot
				gentity_t *rider = &g_entities[ent->client->ps.m_iVehicleNum-1];
				if (rider->inuse && rider->client)
				{ //pilot is valid...
                    if (rider->client->ps.weapon != WP_MELEE &&
						(rider->client->ps.weapon != WP_SABER || !rider->client->ps.saberHolstered))
					{ //can only attack on speeder when using melee or when saber is holstered
						break;
					}
				}
			}

			FireWeapon( ent, qfalse );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;
		case EV_ALT_FIRE:
			FireWeapon( ent, qtrue );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;
	}
}

/*
================
ClientEvents

Events will be passed on to the clients for presentation,
but any server game effects are handled here
================
*/
#include "namespace_begin.h"
qboolean BG_InKnockDownOnly( int anim );
#include "namespace_end.h"

void ClientEvents( gentity_t *ent, int oldEventSequence ) {
	int		i;
	int		event;
	gclient_t *client;
	int		damage;

	client = ent->client;

	if ( oldEventSequence < client->ps.eventSequence - MAX_PS_EVENTS ) {
		oldEventSequence = client->ps.eventSequence - MAX_PS_EVENTS;
	}
	for ( i = oldEventSequence ; i < client->ps.eventSequence ; i++ ) {
		event = client->ps.events[ i & (MAX_PS_EVENTS-1) ];

		switch ( event ) {
		case EV_FALL:
		case EV_ROLL:
			{
				int delta = client->ps.eventParms[ i & (MAX_PS_EVENTS-1) ];
				qboolean knockDownage = qfalse;

				if (ent->client && ent->client->ps.fallingToDeath)
				{
					break;
				}

				if ( ent->s.eType != ET_PLAYER )
				{
					break;		// not in the player model
				}
				
				if ( g_dmflags.integer & DF_NO_FALLING )
				{
					break;
				}

				if (BG_InKnockDownOnly(ent->client->ps.legsAnim))
				{
					if (delta <= 14)
					{
						break;
					}
					knockDownage = qtrue;
				}
				else
				{
					if (delta <= 44)
					{
						break;
					}
				}

				if (knockDownage)
				{
					damage = delta*1; //you suffer for falling unprepared. A lot. Makes throws and things useful, and more realistic I suppose.
				}
				else
				{
					if (g_gametype.integer == GT_SIEGE &&
						delta > 60)
					{ //longer falls hurt more
						damage = delta*1; //good enough for now, I guess
					}
					else
					{
						damage = delta*0.16; //good enough for now, I guess
					}
				}

				//VectorSet (dir, 0, 0, 1);
				ent->pain_debounce_time = level.time + 200;	// no normal pain sound
				G_Damage (ent, NULL, NULL, NULL, NULL, damage, DAMAGE_NO_ARMOR, MOD_FALLING);

				if (ent->health < 1)
				{
					G_Sound(ent, CHAN_AUTO, G_SoundIndex( "sound/player/fallsplat.wav" ));
				}
			}
			break;
		case EV_FIRE_WEAPON:
			FireWeapon( ent, qfalse );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;

		case EV_ALT_FIRE:
			FireWeapon( ent, qtrue );
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;

		case EV_SABER_ATTACK:
			ent->client->dangerTime = level.time;
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
			ent->client->invulnerableTimer = 0;
			break;

		//rww - Note that these must be in the same order (ITEM#-wise) as they are in holdable_t
		case EV_USE_ITEM1: //seeker droid
			ItemUse_Seeker(ent);
			break;
		case EV_USE_ITEM2: //shield
			ItemUse_Shield(ent);
			break;
		case EV_USE_ITEM3: //medpack
			ItemUse_MedPack(ent);
			break;
		case EV_USE_ITEM4: //big medpack
			ItemUse_MedPack_Big(ent);
			break;
		case EV_USE_ITEM5: //binoculars
			ItemUse_Binoculars(ent);
			break;
		case EV_USE_ITEM6: //sentry gun
			ItemUse_Sentry(ent);
			break;
		case EV_USE_ITEM7: //jetpack
			ItemUse_Jetpack(ent);
			break;
		case EV_USE_ITEM8: //health disp
			break;
		case EV_USE_ITEM9: //ammo disp
			break;
		case EV_USE_ITEM10: //eweb
			ItemUse_UseEWeb(ent);
			break;
		case EV_USE_ITEM11: //cloak
			ItemUse_UseCloak(ent);
			break;
		default:
			break;
		}
	}

}

/*
==============
SendPendingPredictableEvents
==============
*/
void SendPendingPredictableEvents( playerState_t *ps ) {
	gentity_t *t;
	int event, seq;
	int extEvent, number;

	// if there are still events pending
	if ( ps->entityEventSequence < ps->eventSequence ) {
		// create a temporary entity for this event which is sent to everyone
		// except the client who generated the event
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		// set external event to zero before calling BG_PlayerStateToEntityState
		extEvent = ps->externalEvent;
		ps->externalEvent = 0;
		// create temporary entity for event
		t = G_TempEntity( ps->origin, event );
		number = t->s.number;
		BG_PlayerStateToEntityState( ps, &t->s, qtrue );
		t->s.number = number;
		t->s.eType = ET_EVENTS + event;
		t->s.eFlags |= EF_PLAYER_EVENT;
		t->s.otherEntityNum = ps->clientNum;
		// send to everyone except the client who generated the event
		t->r.svFlags |= SVF_NOTSINGLECLIENT;
		t->r.singleClient = ps->clientNum;
		// set back external event
		ps->externalEvent = extEvent;
	}
}

/*
==================
G_UpdateClientBroadcasts

Determines whether this client should be broadcast to any other clients.  
A client is broadcast when another client is using force sight or is
==================
*/

static qboolean SE_RenderIsVisible( const gentity_t *self, const vec3_t startPos, const vec3_t testOrigin, qboolean reversedCheck ) {
	trace_t results;

	trap_Trace( &results, startPos, NULL, NULL, testOrigin, self - g_entities, MASK_SOLID );
	++level.wallhackTracesDone;

	if ( results.fraction < 1.0f ) {
		if ( ( results.surfaceFlags & SURF_FORCEFIELD )
			|| ( results.surfaceFlags & MATERIAL_MASK ) == MATERIAL_GLASS
			|| ( results.surfaceFlags & MATERIAL_MASK ) == MATERIAL_SHATTERGLASS ) {
			// FIXME: This is a quick hack to render people and things through glass and force fields, but will also take
			// effect even if there is another wall between them (and double glass) - which is bad, of course, but
			// nothing i can prevent right now.
			if ( reversedCheck || SE_RenderIsVisible( self, testOrigin, startPos, qtrue ) ) {
				return qtrue;
			}
		}

		return qfalse;
	}

	return qtrue;
}

static qboolean SE_RenderPlayerChecks( const gentity_t *self, const vec3_t playerOrigin, vec3_t playerPoints[9] ) {
	trace_t results;
	int i;

	for ( i = 0; i < 9; i++ ) {
		if ( trap_PointContents( playerPoints[i], self - g_entities ) == CONTENTS_SOLID ) {
			trap_Trace( &results, playerOrigin, NULL, NULL, playerPoints[i], self - g_entities, MASK_SOLID );
			++level.wallhackTracesDone;
			VectorCopy( results.endpos, playerPoints[i] );
		}
	}

	return qtrue;
}


static qboolean SE_IsPlayerCrouching( const gentity_t *ent ) {
	const playerState_t *ps = &ent->client->ps;

	// FIXME: This is no proper way to determine if a client is actually in a crouch position, we want to do this in
	// order to properly hide a client from the enemy when he is crouching behind an obstacle and could not possibly
	// be seen.

	if ( !ent->inuse || !ps ) {
		return qfalse;
	}

	if ( ps->forceHandExtend == HANDEXTEND_KNOCKDOWN ) {
		return qtrue;
	}

	if ( ps->pm_flags & PMF_DUCKED ) {
		return qtrue;
	}

	switch ( ps->legsAnim ) {
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
	default:
		break;
	}

	switch ( ps->torsoAnim ) {
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
	default:
		break;
	}

	return qfalse;
}

static qboolean SE_RenderPlayerPoints( qboolean isCrouching, const vec3_t playerAngles, const vec3_t playerOrigin, vec3_t playerPoints[9] ) {
	int isHeight = isCrouching ? 32 : 56;
	vec3_t	forward, right, up;

	AngleVectors( playerAngles, forward, right, up );

	VectorMA( playerOrigin, 32.0f, up, playerPoints[0] );
	VectorMA( playerOrigin, 64.0f, forward, playerPoints[1] );
	VectorMA( playerPoints[1], 64.0f, right, playerPoints[1] );
	VectorMA( playerOrigin, 64.0f, forward, playerPoints[2] );
	VectorMA( playerPoints[2], -64.0f, right, playerPoints[2] );
	VectorMA( playerOrigin, -64.0f, forward, playerPoints[3] );
	VectorMA( playerPoints[3], 64.0f, right, playerPoints[3] );
	VectorMA( playerOrigin, -64.0f, forward, playerPoints[4] );
	VectorMA( playerPoints[4], -64.0f, right, playerPoints[4] );

	VectorMA( playerPoints[1], isHeight, up, playerPoints[5] );
	VectorMA( playerPoints[2], isHeight, up, playerPoints[6] );
	VectorMA( playerPoints[3], isHeight, up, playerPoints[7] );
	VectorMA( playerPoints[4], isHeight, up, playerPoints[8] );

	return qtrue;
}

static void GetCameraPosition( const gentity_t *self, vec3_t cameraOrigin ) {
	vec3_t forward;
	int thirdPersonRange = 80, thirdPersonVertOffset = 16;

	AngleVectors( self->client->ps.viewangles, forward, NULL, NULL );
	VectorNormalize( forward );

	//Get third person position.  
	VectorCopy( self->client->ps.origin, cameraOrigin );
	VectorMA( cameraOrigin, -thirdPersonRange, forward, cameraOrigin );
	cameraOrigin[2] += 24 + thirdPersonVertOffset;

	if ( SE_IsPlayerCrouching( self ) ) {
		cameraOrigin[2] -= 32;
	}
}

static qboolean SE_NetworkPlayer( const gentity_t *self, const gentity_t *other ) {
	int i;
	vec3_t firstPersonPos, thirdPersonPos, targPos[9];
	unsigned int contents;

	GetCameraPosition( self, thirdPersonPos );

	VectorCopy( self->client->ps.origin, firstPersonPos );
	firstPersonPos[2] += 24;

	if ( SE_IsPlayerCrouching( self ) ) {
		firstPersonPos[2] -= 32;
	}

	contents = trap_PointContents( firstPersonPos, self - g_entities );

	// translucent, we should probably just network them anyways
	if ( contents & ( CONTENTS_WATER | CONTENTS_LAVA | CONTENTS_SLIME ) ) {
		return qtrue;
	}

	// entirely in an opaque surface, no point networking them.
	if ( contents & ( CONTENTS_SOLID | CONTENTS_TERRAIN | CONTENTS_OPAQUE ) ) {
		return qfalse;
	}

	// plot their bbox pointer into targPos[]
	SE_RenderPlayerPoints( SE_IsPlayerCrouching( other ), other->client->ps.viewangles, other->client->ps.origin, targPos );
	SE_RenderPlayerChecks( self, other->client->ps.origin, targPos );

	for ( i = 0; i < 9; i++ ) {
		if ( SE_RenderIsVisible( self, thirdPersonPos, targPos[i], qfalse ) ) {
			return qtrue;
		}

		if ( SE_RenderIsVisible( self, firstPersonPos, targPos[i], qfalse ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

// Tracing non-players seems to have a bad effect, we know players are limited to 32 per frame, however other gentities
// that are being added are not! It's stupid to actually add traces for it, even with a limited form i used before of 2
// traces per object. There are to many too track and simply networking them takes less FPS either way
static qboolean G_EntityOccluded( const gentity_t *self, const gentity_t *other ) {
	// This is a non-player object, just send it (see above).
	if ( !other->inuse || other->s.number >= level.maxclients ) {
		return qtrue;
	}

	// If this player is me, or my spectee, we will always draw and don't trace.
	if ( self == other ) {
		return qtrue;
	}

	if ( self->client->ps.zoomMode ) { // 0.0
		return qtrue;
	}
	
	// Not rendering; this player's traces did not appear in my screen.
	if ( !SE_NetworkPlayer( self, other ) ) {
		return qtrue;
	}

	return qfalse;
}

static qboolean IsClientHiddenToOtherClient( gentity_t *self, gentity_t *other ) {
#ifdef _DEBUG
	if ( !d_disableRaceVisChecks.integer )
	{
#endif
	// racemode visibility

	if (self->isAimPracticePack) {
		if (other->client->ps.stats[STAT_RACEMODE]) {
			if ((other->client->sess.racemodeFlags & RMF_HIDERACERS) && other->aimPracticeEntBeingUsed && other->aimPracticeEntBeingUsed != self)
				return qtrue;
			if ((other->client->sess.racemodeFlags & RMF_HIDEBOTS) && other->aimPracticeEntBeingUsed != self)
				return qtrue;
			return qfalse;
		}
		else {
			if (other->client->sess.seeAimBotsWhileIngame)
				return qfalse;
			return qtrue;
		}
	}

	// if self is in racemode and other is not, hide self to other unless other explicitly enabled seeing racers
	if ( self->client->ps.stats[STAT_RACEMODE] && !other->client->ps.stats[STAT_RACEMODE] && !other->client->sess.seeRacersWhileIngame ) {
		return qtrue;
	}

	// if self is not in racemode and other is, hide self to other if other explicitly disabled seeing ig players
	if ( !self->client->ps.stats[STAT_RACEMODE] && other->client->ps.stats[STAT_RACEMODE] && ( other->client->sess.racemodeFlags & RMF_HIDEINGAME ) ) {
		return qtrue;
	}

	// if both are in racemode, hide self to other if other explicitly disabled seeing other racers
	if ( self->client->ps.stats[STAT_RACEMODE] && other->client->ps.stats[STAT_RACEMODE] && ( other->client->sess.racemodeFlags & RMF_HIDERACERS ) ) {
		return qtrue;
	}
#ifdef _DEBUG
	}
#endif

	// anti wallhack visibility
	if ( g_antiWallhack.integer ) {
		// never hide if we reached the max traces
		if ( g_wallhackMaxTraces.integer && level.wallhackTracesDone > g_wallhackMaxTraces.integer ) {
			return qfalse;
		}

		// other is whitelisted, don't hide me to him
		if ( g_antiWallhack.integer == 1 && other->client->sess.whTrustToggle ) {
			return qfalse;
		}

		// other is not blacklisted, don't hide me to him
		if ( g_antiWallhack.integer >= 2 && !other->client->sess.whTrustToggle ) {
			return qfalse;
		}

		return G_EntityOccluded( self, other );
	}

	return qfalse;
}

#define MAX_JEDI_MASTER_DISTANCE	( ( float )( 2500 * 2500 ) )
#define MAX_JEDI_MASTER_FOV			100.0f
#define MAX_FORCE_SIGHT_DISTANCE	( ( float )( 1500 * 1500 ) )
#define MAX_FORCE_SIGHT_FOV			100.0f

static qboolean IsClientBroadcastToOtherClient( gentity_t *self, gentity_t *other ) {
	float dist;
	vec3_t angles;

	if (self->isAimPracticePack) {
		if (other->client->ps.stats[STAT_RACEMODE] || other->client->sess.seeRacersWhileIngame)
			return qtrue;
		return qfalse;
	}

	// always broadcast to spectators
	if ( other->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return qtrue;
	}

	VectorSubtract( self->client->ps.origin, other->client->ps.origin, angles );
	dist = VectorLengthSquared( angles );
	vectoangles( angles, angles );

	// broadcast jedi master to everyone if we are in distance/field of view
	if ( g_gametype.integer == GT_JEDIMASTER && self->client->ps.isJediMaster ) {
		if ( dist < MAX_JEDI_MASTER_DISTANCE && InFieldOfVision( other->client->ps.viewangles, MAX_JEDI_MASTER_FOV, angles ) ) {
			return qtrue;
		}
	}

	if (!g_fixSense.integer) {
		// broadcast this client to everyone using force sight if we are in distance/field of view
		if ((other->client->ps.fd.forcePowersActive & (1 << FP_SEE))) {
			if (dist < MAX_FORCE_SIGHT_DISTANCE && InFieldOfVision(other->client->ps.viewangles, MAX_FORCE_SIGHT_FOV, angles)) {
				return qtrue;
			}
		}
	}
	else if (g_fixSense.integer == 1) {
		// broadcast this client to everyone using force sight if we are in distance
		if ((other->client->ps.fd.forcePowersActive & (1 << FP_SEE))) {
			if (dist < MAX_FORCE_SIGHT_DISTANCE) {
				return qtrue;
			}
		}
	}
	else {
		// broadcast this client to everyone using force sight
		if ((other->client->ps.fd.forcePowersActive & (1 << FP_SEE))) {
			return qtrue;
		}
	}

	return qfalse;
}

void G_UpdateClientBroadcasts( gentity_t *self ) {
	int i;
	gentity_t *other;

	self->r.broadcastClients[0] = 0;
	self->r.broadcastClients[1] = 0;

	for ( i = 0, other = g_entities; i < MAX_CLIENTS; i++, other++ ) {
		if ( !other->inuse || other->client->pers.connected != CON_CONNECTED ) {
			// no need to compute visibility for non-connected clients
			continue;
		}

		if ( other == self ) {
			// we are always sent to ourselves anyway, this is purely an optimisation
			continue;
		}

		// the goal of this function is to set the bits of the second index of the broadcastClients array
		// this array takes precedence over both SVF_BROADCAST and the first array index
		// set bits represent clients we will always be hidden to, unset bits do nothing
		if ( IsClientHiddenToOtherClient( self, other ) ) {
			self->r.broadcastClients[1] |= ( 1 << ( i % 32 ) );
		}

		// the goal of this function is to set the bits of the first index of the broadcastClients array
		// set bits represent clients we will be broadcast to, unless hidden by the next array index (see above)
		// unset bits do nothing (PVS check will be performed normally for those)
		if ( IsClientBroadcastToOtherClient( self, other ) ) {
			self->r.broadcastClients[0] |= ( 1 << ( i % 32 ) );
		}
	}

	trap_LinkEntity( self );
}

void G_AddPushVecToUcmd( gentity_t *self, usercmd_t *ucmd )
{
	vec3_t	forward, right, moveDir;
	float	pushSpeed, fMove, rMove;

	if ( !self->client )
	{
		return;
	}
	pushSpeed = VectorLengthSquared(self->client->pushVec);
	if(!pushSpeed)
	{//not being pushed
		return;
	}

	AngleVectors(self->client->ps.viewangles, forward, right, NULL);
	VectorScale(forward, ucmd->forwardmove/127.0f * self->client->ps.speed, moveDir);
	VectorMA(moveDir, ucmd->rightmove/127.0f * self->client->ps.speed, right, moveDir);
	//moveDir is now our intended move velocity

	VectorAdd(moveDir, self->client->pushVec, moveDir);
	self->client->ps.speed = VectorNormalize(moveDir);
	//moveDir is now our intended move velocity plus our push Vector

	fMove = 127.0 * DotProduct(forward, moveDir);
	rMove = 127.0 * DotProduct(right, moveDir);
	ucmd->forwardmove = floor(fMove);//If in the same dir , will be positive
	ucmd->rightmove = floor(rMove);//If in the same dir , will be positive

	if ( self->client->pushVecTime < level.time )
	{
		VectorClear( self->client->pushVec );
	}
}

qboolean G_StandingAnim( int anim )
{//NOTE: does not check idles or special (cinematic) stands
	switch ( anim )
	{
	case BOTH_STAND1:
	case BOTH_STAND2:
	case BOTH_STAND3:
	case BOTH_STAND4:
		return qtrue;
		break;
	}
	return qfalse;
}

qboolean G_ActionButtonPressed(int buttons)
{
	if (buttons & BUTTON_ATTACK)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_USE_HOLDABLE)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_GESTURE)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_USE)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCEGRIP)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_ALT_ATTACK)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCEPOWER)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCE_LIGHTNING)
	{
		return qtrue;
	}
	else if (buttons & BUTTON_FORCE_DRAIN)
	{
		return qtrue;
	}

	return qfalse;
}

void G_CheckClientIdle( gentity_t *ent, usercmd_t *ucmd ) 
{
	vec3_t viewChange;
	qboolean actionPressed;
	int buttons;

	if ( !ent || !ent->client || ent->health <= 0 || ent->client->ps.stats[STAT_HEALTH] <= 0 ||
		ent->client->sess.sessionTeam == TEAM_SPECTATOR || (ent->client->ps.pm_flags & PMF_FOLLOW))
	{
		return;
	}

	buttons = ucmd->buttons;

	if (ent->r.svFlags & SVF_BOT)
	{ //they press use all the time..
		buttons &= ~BUTTON_USE;
	}
	actionPressed = G_ActionButtonPressed(buttons);

	VectorSubtract(ent->client->ps.viewangles, ent->client->idleViewAngles, viewChange);
	if ( !VectorCompare( vec3_origin, ent->client->ps.velocity ) 
		|| actionPressed || ucmd->forwardmove || ucmd->rightmove || ucmd->upmove 
		|| !G_StandingAnim( ent->client->ps.legsAnim ) 
		|| (ent->health+ent->client->ps.stats[STAT_ARMOR]) != ent->client->idleHealth
		|| VectorLength(viewChange) > 10
		|| ent->client->ps.legsTimer > 0
		|| ent->client->ps.torsoTimer > 0
		|| ent->client->ps.weaponTime > 0
		|| ent->client->ps.weaponstate == WEAPON_CHARGING
		|| ent->client->ps.weaponstate == WEAPON_CHARGING_ALT
		|| ent->client->ps.zoomMode
		|| (ent->client->ps.weaponstate != WEAPON_READY && ent->client->ps.weapon != WP_SABER)
		|| ent->client->ps.forceHandExtend != HANDEXTEND_NONE
		|| ent->client->ps.saberBlocked != BLOCKED_NONE
		|| ent->client->ps.saberBlocking >= level.time
		|| ent->client->ps.weapon == WP_MELEE
		|| (ent->client->ps.weapon != ent->client->pers.cmd.weapon && ent->s.eType != ET_NPC))
	{//FIXME: also check for turning?
		qboolean brokeOut = qfalse;

		if ( !VectorCompare( vec3_origin, ent->client->ps.velocity ) 
			|| actionPressed || ucmd->forwardmove || ucmd->rightmove || ucmd->upmove 
			|| (ent->health+ent->client->ps.stats[STAT_ARMOR]) != ent->client->idleHealth
			|| ent->client->ps.zoomMode
			|| (ent->client->ps.weaponstate != WEAPON_READY && ent->client->ps.weapon != WP_SABER)
			|| (ent->client->ps.weaponTime > 0 && ent->client->ps.weapon == WP_SABER)
			|| ent->client->ps.weaponstate == WEAPON_CHARGING
			|| ent->client->ps.weaponstate == WEAPON_CHARGING_ALT
			|| ent->client->ps.forceHandExtend != HANDEXTEND_NONE
			|| ent->client->ps.saberBlocked != BLOCKED_NONE
			|| ent->client->ps.saberBlocking >= level.time
			|| ent->client->ps.weapon == WP_MELEE
			|| (ent->client->ps.weapon != ent->client->pers.cmd.weapon && ent->s.eType != ET_NPC))
		{
			//if in an idle, break out
			switch ( ent->client->ps.legsAnim )
			{
			case BOTH_STAND1IDLE1:
			case BOTH_STAND2IDLE1:
			case BOTH_STAND2IDLE2:
			case BOTH_STAND3IDLE1:
			case BOTH_STAND5IDLE1:
				ent->client->ps.legsTimer = 0;
				brokeOut = qtrue;
				break;
			}
			switch ( ent->client->ps.torsoAnim )
			{
			case BOTH_STAND1IDLE1:
			case BOTH_STAND2IDLE1:
			case BOTH_STAND2IDLE2:
			case BOTH_STAND3IDLE1:
			case BOTH_STAND5IDLE1:
				ent->client->ps.torsoTimer = 0;
				ent->client->ps.weaponTime = 0;
				ent->client->ps.saberMove = LS_READY;
				brokeOut = qtrue;
				break;
			}
		}
		//
		ent->client->idleHealth = (ent->health+ent->client->ps.stats[STAT_ARMOR]);
		VectorCopy(ent->client->ps.viewangles, ent->client->idleViewAngles);
		if ( ent->client->idleTime < level.time )
		{
			ent->client->idleTime = level.time;
		}

		if (brokeOut &&
			(ent->client->ps.weaponstate == WEAPON_CHARGING || ent->client->ps.weaponstate == WEAPON_CHARGING_ALT))
		{
			ent->client->ps.torsoAnim = TORSO_RAISEWEAP1;
		}
	}
	else if ( level.time - ent->client->idleTime > 5000 )
	{//been idle for 5 seconds
		int	idleAnim = -1;
		switch ( ent->client->ps.legsAnim )
		{
		case BOTH_STAND1:
			idleAnim = BOTH_STAND1IDLE1;
			break;
		case BOTH_STAND2:
			idleAnim = BOTH_STAND2IDLE1;
			break;
		case BOTH_STAND3:
			idleAnim = BOTH_STAND3IDLE1;
			break;
		case BOTH_STAND5:
			idleAnim = BOTH_STAND5IDLE1;
			break;
		}

		if (idleAnim == BOTH_STAND2IDLE1 && Q_irand(1, 10) <= 5)
		{
			idleAnim = BOTH_STAND2IDLE2;
		}

		if ( idleAnim != -1 && idleAnim > 0 && idleAnim < MAX_ANIMATIONS )
		{
			G_SetAnim(ent, ucmd, SETANIM_BOTH, idleAnim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);

			//don't idle again after this anim for a while
			ent->client->idleTime = level.time + ent->client->ps.legsTimer + Q_irand( 0, 2000 );
		}
	}
}

void NPC_Accelerate( gentity_t *ent, qboolean fullWalkAcc, qboolean fullRunAcc )
{
	if ( !ent->client || !ent->NPC )
	{
		return;
	}

	if ( !ent->NPC->stats.acceleration )
	{//No acceleration means just start and stop
		ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
	}
	//FIXME:  in cinematics always accel/decel?
	else if ( ent->NPC->desiredSpeed <= ent->NPC->stats.walkSpeed )
	{//Only accelerate if at walkSpeeds
		if ( ent->NPC->desiredSpeed > ent->NPC->currentSpeed + ent->NPC->stats.acceleration )
		{
			ent->NPC->currentSpeed += ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed > ent->NPC->currentSpeed )
		{
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
		else if ( fullWalkAcc && ent->NPC->desiredSpeed < ent->NPC->currentSpeed - ent->NPC->stats.acceleration )
		{//decelerate even when walking
			ent->NPC->currentSpeed -= ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed < ent->NPC->currentSpeed )
		{//stop on a dime
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
	}
	else//  if ( ent->NPC->desiredSpeed > ent->NPC->stats.walkSpeed )
	{//Only decelerate if at runSpeeds
		if ( fullRunAcc && ent->NPC->desiredSpeed > ent->NPC->currentSpeed + ent->NPC->stats.acceleration )
		{//Accelerate to runspeed
			ent->NPC->currentSpeed += ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed > ent->NPC->currentSpeed )
		{//accelerate instantly
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
		else if ( fullRunAcc && ent->NPC->desiredSpeed < ent->NPC->currentSpeed - ent->NPC->stats.acceleration )
		{
			ent->NPC->currentSpeed -= ent->NPC->stats.acceleration;
		}
		else if ( ent->NPC->desiredSpeed < ent->NPC->currentSpeed )
		{
			ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
		}
	}
}

/*
-------------------------
NPC_GetWalkSpeed
-------------------------
*/

static int NPC_GetWalkSpeed( gentity_t *ent )
{
	int	walkSpeed = 0;

	if ( ( ent->client == NULL ) || ( ent->NPC == NULL ) )
		return 0;

	switch ( ent->client->playerTeam )
	{
	case NPCTEAM_PLAYER:	//To shutup compiler, will add entries later (this is stub code)
	default:
		walkSpeed = ent->NPC->stats.walkSpeed;
		break;
	}

	return walkSpeed;
}

/*
-------------------------
NPC_GetRunSpeed
-------------------------
*/
static int NPC_GetRunSpeed( gentity_t *ent )
{
	int	runSpeed = 0;

	if ( ( ent->client == NULL ) || ( ent->NPC == NULL ) )
		return 0;

	// team no longer indicates species/race.  Use NPC_class to adjust speed for specific npc types
	switch( ent->client->NPC_class)
	{
	case CLASS_PROBE:	// droid cases here to shut-up compiler
	case CLASS_GONK:
	case CLASS_R2D2:
	case CLASS_R5D2:
	case CLASS_MARK1:
	case CLASS_MARK2:
	case CLASS_PROTOCOL:
	case CLASS_ATST: // hmm, not really your average droid
	case CLASS_MOUSE:
	case CLASS_SEEKER:
	case CLASS_REMOTE:
		runSpeed = ent->NPC->stats.runSpeed;
		break;

	default:
		runSpeed = ent->NPC->stats.runSpeed*1.3f; //rww - seems to slow in MP for some reason.
		break;
	}

	return runSpeed;
}

//Seems like a slightly less than ideal method for this, could it be done on the client?
extern qboolean FlyingCreature( gentity_t *ent );
void G_CheckMovingLoopingSounds( gentity_t *ent, usercmd_t *ucmd )
{
	if ( ent->client )
	{
		if ( (ent->NPC&&!VectorCompare( vec3_origin, ent->client->ps.moveDir ))//moving using moveDir
			|| ucmd->forwardmove || ucmd->rightmove//moving using ucmds
			|| (ucmd->upmove&&FlyingCreature( ent ))//flier using ucmds to move
			|| (FlyingCreature( ent )&&!VectorCompare( vec3_origin, ent->client->ps.velocity )&&ent->health>0))//flier using velocity to move
		{
			switch( ent->client->NPC_class )
			{
			case CLASS_R2D2:
				ent->s.loopSound = G_SoundIndex( "sound/chars/r2d2/misc/r2_move_lp.wav" );
				break;
			case CLASS_R5D2:
				ent->s.loopSound = G_SoundIndex( "sound/chars/r2d2/misc/r2_move_lp2.wav" );
				break;
			case CLASS_MARK2:
				ent->s.loopSound = G_SoundIndex( "sound/chars/mark2/misc/mark2_move_lp" );
				break;
			case CLASS_MOUSE:
				ent->s.loopSound = G_SoundIndex( "sound/chars/mouse/misc/mouse_lp" );
				break;
			case CLASS_PROBE:
				ent->s.loopSound = G_SoundIndex( "sound/chars/probe/misc/probedroidloop" );
				break;
			default:
				break;
			}
		}
		else
		{//not moving under your own control, stop loopSound
			if ( ent->client->NPC_class == CLASS_R2D2 || ent->client->NPC_class == CLASS_R5D2 
					|| ent->client->NPC_class == CLASS_MARK2 || ent->client->NPC_class == CLASS_MOUSE 
					|| ent->client->NPC_class == CLASS_PROBE )
			{
				ent->s.loopSound = 0;
			}
		}
	}
}

void G_HeldByMonster( gentity_t *ent, usercmd_t **ucmd )
{
	if ( ent 
		&& ent->client
		&& ent->client->ps.hasLookTarget )//NOTE: lookTarget is an entity number, so this presumes that client 0 is NOT a Rancor...
	{
		gentity_t *monster = &g_entities[ent->client->ps.lookTarget];
		if ( monster && monster->client )
		{
			//take the monster's waypoint as your own
			ent->waypoint = monster->waypoint;
			if ( monster->s.NPC_class == CLASS_RANCOR )
			{//only possibility right now, may add Wampa and Sand Creature later
				BG_AttachToRancor( monster->ghoul2, //ghoul2 info
					monster->r.currentAngles[YAW],
					monster->r.currentOrigin,
					level.time,
					NULL,
					monster->modelScale,
					(monster->client->ps.eFlags2&EF2_GENERIC_NPC_FLAG),
					ent->client->ps.origin,
					ent->client->ps.viewangles,
					NULL );
			}
			VectorClear( ent->client->ps.velocity );
			G_SetOrigin( ent, ent->client->ps.origin );
			SetClientViewAngle( ent, ent->client->ps.viewangles );
			G_SetAngles( ent, ent->client->ps.viewangles );
			trap_LinkEntity( ent );//redundant?
		}
	}
	// don't allow movement, weapon switching, and most kinds of button presses
	(*ucmd)->forwardmove = 0;
	(*ucmd)->rightmove = 0;
	(*ucmd)->upmove = 0;
}

enum
{
	TAUNT_TAUNT = 0,
	TAUNT_BOW,
	TAUNT_MEDITATE,
	TAUNT_FLOURISH,
	TAUNT_GLOAT
};

void G_SetTauntAnim( gentity_t *ent, int taunt )
{
	if (taunt == TAUNT_MEDITATE &&
		(ent->client->pers.cmd.upmove ||
		ent->client->pers.cmd.forwardmove ||
		ent->client->pers.cmd.rightmove))
	{ //hack, don't do while moving
		//just apply this annoying restriction to meditate
		return;
	}
	if ( taunt != TAUNT_TAUNT && !g_moreTaunts.integer)
	{//normal taunt always allowed
		if ( g_gametype.integer != GT_DUEL
			&& g_gametype.integer != GT_POWERDUEL )
		{//no taunts unless in Duel
			return;
		}
	}
	if (g_gametype.integer != GT_DUEL && g_gametype.integer != GT_POWERDUEL && taunt == TAUNT_MEDITATE && !ent->client->sess.inRacemode)
	{
		return; //no meditating unless racemode; could be cheesy with hiding behind boxes with really small animation
	}
	// *CHANGE 65* fix - release rocket lock, old bug
	BG_ClearRocketLock(&ent->client->ps);

	if ( ent->client->ps.torsoTimer < 1
		&& ent->client->ps.forceHandExtend == HANDEXTEND_NONE 
		&& ent->client->ps.legsTimer < 1
		&& ent->client->ps.weaponTime < 1 
		&& ent->client->ps.saberLockTime < level.time )
	{
		int anim = -1;
		switch ( taunt )
		{
		case TAUNT_TAUNT:
			if ( ent->client->ps.weapon != WP_SABER )
			{
				anim = BOTH_ENGAGETAUNT;
			}
			else if ( ent->client->saber[0].tauntAnim != -1 )
			{
				anim = ent->client->saber[0].tauntAnim;
			}
			else if ( ent->client->saber[1].model 
					&& ent->client->saber[1].model[0]
					&& ent->client->saber[1].tauntAnim != -1 )
			{
				anim = ent->client->saber[1].tauntAnim;
			}
			else
			{
				switch ( ent->client->ps.fd.saberAnimLevel )
				{
				case SS_FAST:
				case SS_TAVION:
					if ( ent->client->ps.saberHolstered == 1 
						&& ent->client->saber[1].model 
						&& ent->client->saber[1].model[0] && ent->client->ps.weapon == WP_SABER)
					{//turn off second saber
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOff );
					}
					else if ( ent->client->ps.saberHolstered == 0 && ent->client->ps.weapon == WP_SABER)
					{//turn off first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOff );
					}
					ent->client->ps.saberHolstered = 2;
					anim = BOTH_GESTURE1;
					break;
				case SS_MEDIUM:
				case SS_STRONG:
				case SS_DESANN:
					anim = BOTH_ENGAGETAUNT;
					break;
				case SS_DUAL:
					if ( ent->client->ps.saberHolstered == 1 
						&& ent->client->saber[1].model 
						&& ent->client->saber[1].model[0] && ent->client->ps.weapon == WP_SABER)
					{//turn on second saber
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOn );
					}
					else if ( ent->client->ps.saberHolstered == 2 && ent->client->ps.weapon == WP_SABER)
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_DUAL_TAUNT;
					break;
				case SS_STAFF:
					if ( ent->client->ps.saberHolstered > 0 && ent->client->ps.weapon == WP_SABER)
					{//turn on all blades
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_STAFF_TAUNT;
					break;
				}
			}
			break;
		case TAUNT_BOW:
			if ( ent->client->saber[0].bowAnim != -1 )
			{
				anim = ent->client->saber[0].bowAnim;
			}
			else if ( ent->client->saber[1].model 
					&& ent->client->saber[1].model[0]
					&& ent->client->saber[1].bowAnim != -1 )
			{
				anim = ent->client->saber[1].bowAnim;
			}
			else
			{
				anim = BOTH_BOW;
			}
			if ( ent->client->ps.saberHolstered == 1 
				&& ent->client->saber[1].model 
				&& ent->client->saber[1].model[0] && ent->client->ps.weapon == WP_SABER)
			{//turn off second saber
				G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOff );
			}
			else if ( ent->client->ps.saberHolstered == 0 && ent->client->ps.weapon == WP_SABER)
			{//turn off first
				G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOff );
			}
			ent->client->ps.saberHolstered = 2;
			break;
		case TAUNT_MEDITATE:
			if ( ent->client->saber[0].meditateAnim != -1 )
			{
				anim = ent->client->saber[0].meditateAnim;
			}
			else if ( ent->client->saber[1].model 
					&& ent->client->saber[1].model[0]
					&& ent->client->saber[1].meditateAnim != -1 )
			{
				anim = ent->client->saber[1].meditateAnim;
			}
			else
			{
				anim = BOTH_MEDITATE;
			}
			if ( ent->client->ps.saberHolstered == 1 
				&& ent->client->saber[1].model 
				&& ent->client->saber[1].model[0] && ent->client->ps.weapon == WP_SABER)
			{//turn off second saber
				G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOff );
			}
			else if ( ent->client->ps.saberHolstered == 0 && ent->client->ps.weapon == WP_SABER)
			{//turn off first
				G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOff );
			}
			ent->client->ps.saberHolstered = 2;
			break;
		case TAUNT_FLOURISH:
				if ( ent->client->ps.saberHolstered == 1 
					&& ent->client->saber[1].model 
					&& ent->client->saber[1].model[0] )
				{//turn on second saber
					G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOn && ent->client->ps.weapon == WP_SABER);
				}
				else if ( ent->client->ps.saberHolstered == 2 && ent->client->ps.weapon == WP_SABER)
				{//turn on first
					G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
				}
				ent->client->ps.saberHolstered = 0;
				if ( ent->client->saber[0].flourishAnim != -1 )
				{
					anim = ent->client->saber[0].flourishAnim;
				}
				else if ( ent->client->saber[1].model 
					&& ent->client->saber[1].model[0]
					&& ent->client->saber[1].flourishAnim != -1 )
				{
					anim = ent->client->saber[1].flourishAnim;
				}
				else
				{
					switch ( ent->client->ps.fd.saberAnimLevel )
					{
					case SS_FAST:
					case SS_TAVION:
						anim = BOTH_SHOWOFF_FAST;
						break;
					case SS_MEDIUM:
						anim = BOTH_SHOWOFF_MEDIUM;
						break;
					case SS_STRONG:
					case SS_DESANN:
						anim = BOTH_SHOWOFF_STRONG;
						break;
					case SS_DUAL:
						anim = BOTH_SHOWOFF_DUAL;
						break;
					case SS_STAFF:
						anim = BOTH_SHOWOFF_STAFF;
						break;
					}
				}
			break;
		case TAUNT_GLOAT:
			if ( ent->client->saber[0].gloatAnim != -1 )
			{
				anim = ent->client->saber[0].gloatAnim;
			}
			else if ( ent->client->saber[1].model 
					&& ent->client->saber[1].model[0]
					&& ent->client->saber[1].gloatAnim != -1 )
			{
				anim = ent->client->saber[1].gloatAnim;
			}
			else
			{
				switch ( ent->client->ps.fd.saberAnimLevel )
				{
				case SS_FAST:
				case SS_TAVION:
					anim = BOTH_VICTORY_FAST;
					break;
				case SS_MEDIUM:
					anim = BOTH_VICTORY_MEDIUM;
					break;
				case SS_STRONG:
				case SS_DESANN:
					if ( ent->client->ps.saberHolstered && ent->client->ps.weapon == WP_SABER)
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_VICTORY_STRONG;
					break;
				case SS_DUAL:
					if ( ent->client->ps.saberHolstered == 1 
						&& ent->client->saber[1].model 
						&& ent->client->saber[1].model[0] && ent->client->ps.weapon == WP_SABER)
					{//turn on second saber
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[1].soundOn );
					}
					else if ( ent->client->ps.saberHolstered == 2 && ent->client->ps.weapon == WP_SABER)
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_VICTORY_DUAL;
					break;
				case SS_STAFF:
					if ( ent->client->ps.saberHolstered && ent->client->ps.weapon == WP_SABER)
					{//turn on first
						G_Sound( ent, CHAN_WEAPON, ent->client->saber[0].soundOn );
					}
					ent->client->ps.saberHolstered = 0;
					anim = BOTH_VICTORY_STAFF;
					break;
				}
			}
			break;
		}
		if ( anim != -1 )
		{
			if ( ent->client->ps.groundEntityNum != ENTITYNUM_NONE && !(ent->client->ps.m_iVehicleNum)) //taunt anim inside a vehicle can bug weapons
			{
				ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
				ent->client->ps.forceDodgeAnim = anim;
				ent->client->ps.forceHandExtendTime = level.time + BG_AnimLength(ent->localAnimIndex, (animNumber_t)anim);
			}
			if ( taunt == TAUNT_TAUNT || g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL)
			{
				//just use classic sound
				G_AddEvent( ent, EV_TAUNT, taunt );
			}
			else if (taunt == TAUNT_GLOAT || taunt == TAUNT_FLOURISH)
			{
				//generate dank taunt sounds for clients that support them

				//determine the cool taunt sound randomly
				int rng = 0;
				while (rng >= 0 && rng <= TAUNT_GLOAT) { // make sure the final number isn't <= 4
					rng = Q_irand(0, 255);
					rng &= ~3; // zero the lower two bits
					if (taunt == TAUNT_FLOURISH)
						rng |= 1; // set the lowest bit for flourish
				}
				G_AddEvent(ent, EV_TAUNT, rng);
				//non-supporting clients will just use the normal taunt sound anyway;
				//no need to differentiate between users of different mods here.
			}
		}
	}
}

static void StoreRecordingTrail(gentity_t *ent) {
	assert(ent && ent->client);
	if (!ent->client->moveRecordStart)
		return;

	if (ent->client->ps.fallingToDeath || ent->health <= 0 || ent->client->sess.sessionTeam == TEAM_SPECTATOR || !ent->client->sess.inRacemode) {
		ent->client->moveRecordStart = 0;
		ent->client->trailHead = 0;
		memset(ent->client->trail, 0, sizeof(ent->client->trail));
		ent->client->reachedLimit = qfalse;
		return;
	}

	// increment the head
	if (ent->client->trailHead + 1 == MAX_MOVEMENT_TRAILS) {
		if (!ent->client->reachedLimit) {
			PrintIngame(ent - g_entities, "Reached recording limit. Stopped recording. Re-use command to stop recording.\n");
			ent->client->reachedLimit = qtrue;
		}
		return;
	}

	int head = ++ent->client->trailHead;

	// store all the collision-detection info and the time
	VectorCopy(ent->r.mins, ent->client->trail[head].mins);
	VectorCopy(ent->r.maxs, ent->client->trail[head].maxs);
	//VectorCopy( ent->client->r.currentAngles, ent->client->trail[head].currentAngles );

	ent->client->trail[head].torsoAnim = ent->client->ps.torsoAnim;
	ent->client->trail[head].torsoTimer = ent->client->ps.torsoTimer;
	ent->client->trail[head].legsAnim = ent->client->ps.legsAnim;
	ent->client->trail[head].legsTimer = ent->client->ps.legsTimer;
	//ent->client->trail[head].realAngle = ent->s.apos.trBase[YAW];

	if (qtrue) {
		// try to get higher resolution
		vec3_t origin, angles;
		BG_EvaluateTrajectory(&ent->s.pos, level.time, origin);
		BG_EvaluateTrajectory(&ent->s.apos, level.time, angles);
		VectorCopy(origin, ent->client->trail[head].currentOrigin);
		VectorCopy(angles, ent->client->trail[head].angles);
	}
	else {
		VectorCopy(ent->r.currentOrigin, ent->client->trail[head].currentOrigin);
		VectorCopy(ent->s.apos.trBase, ent->client->trail[head].angles);
	}

	//if (ent->client->trail[head].currentAngles[0] || ent->client->trail[head].currentAngles[1] || ent->client->trail[head].currentAngles[2])
		//Com_Printf("Current angles are %.2f %.2f %.2f\n", ent->client->trail[head].currentAngles[0], ent->client->trail[head].currentAngles[1], ent->client->trail[head].currentAngles[2]);

	ent->client->trail[head].time = trap_Milliseconds() - ent->client->moveRecordStart;
	VectorCopy(ent->client->ps.velocity, ent->client->trail[head].velocity);
}

static void RunFakeFCOverlay(gentity_t *ent) {
	assert(ent);
	if (ent->client && ent->client->account && ent->client->account->flags & ACCOUNTFLAG_FAKEFCOVERLAY &&
		g_gametype.integer == GT_CTF && ent - g_entities < MAX_CLIENTS && !ent->client->sess.inRacemode && !IsFreeSpec(ent)) {
		// send the systeminfo periodically if also using smod
		int now = trap_Milliseconds();
		const int FAKEOVERLAY_SYSTEMINFO_SPAM_TIME = 120000; // every couple minutes idk
		if (ent->client->account->flags & ACCOUNTFLAG_SMODTROLL && (!ent->client->pers.lastFakeOverlaySysteminfoSpamTime || now - ent->client->pers.lastFakeOverlaySysteminfoSpamTime >= FAKEOVERLAY_SYSTEMINFO_SPAM_TIME)) {
			G_SendConfigstring(ent - g_entities, CS_SYSTEMINFO, "\\cg_drawCtfBaseBar\\0");
			G_SendConfigstring(ent - g_entities, CS_SYSTEMINFO, NULL);
			ent->client->pers.lastFakeOverlaySysteminfoSpamTime = now;
		}

		// check for a teammate with flag
		gentity_t *teammateWithFlag = NULL;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED)
				continue;
			if (!HasFlag(thisEnt))
				continue;
			if (thisEnt->client->sess.sessionTeam != ent->client->ps.persistant[PERS_TEAM] || thisEnt == ent)
				continue;
			if (ent->client->sess.sessionTeam == TEAM_SPECTATOR && ent->client->sess.spectatorClient == thisEnt - g_entities)
				continue;

			teammateWithFlag = thisEnt;
			break;
		}

		// build the message
		char msg[MAX_STRING_CHARS] = { 0 };
		static char lastMsg[MAX_CLIENTS][MAX_STRING_CHARS] = { 0 };
		char location[MAX_STRING_CHARS] = { 0 };
		static char lastLocation[MAX_CLIENTS][MAX_STRING_CHARS] = { 0 };
		if (teammateWithFlag) {
			Team_GetLocation(teammateWithFlag, location, sizeof(location), qtrue);
			Com_sprintf(msg, sizeof(msg), "cp \"\n\n\n\n\n\n\n\n\n\n\n\n\n%s^7\n^1%d   ^5%d\n%s\"", // 13 newlines to get to bottom of screen-ish
				teammateWithFlag->client->pers.netname,
				teammateWithFlag->client->ps.stats[STAT_HEALTH],
				teammateWithFlag->client->ps.fd.forcePower,
				VALIDSTRING(location) ? location : "");
		}

		// decide whether to send an update
		// send an update if there's no current global centerprint (e.g. vote for a map message),
		// and either the fc/fc location has changed or someone has the flag and it's been 500ms since the last update
		qboolean currentGlobalPrint = (level.globalCenterPrint.sendUntilTime && level.time < level.globalCenterPrint.sendUntilTime);
		static gentity_t *lastTeammateWithFlag[MAX_CLIENTS] = { NULL };
		qboolean fcChanged = (teammateWithFlag != lastTeammateWithFlag[ent - g_entities]);
		qboolean locationChanged = strcmp(location, lastLocation[ent - g_entities]);

		const int FAKEOVERLAY_SPAM_TIME = 500;
		qboolean cooldownSatisfied = (!ent->client->pers.lastFakeOverlaySpamTime || now - ent->client->pers.lastFakeOverlaySpamTime >= FAKEOVERLAY_SPAM_TIME);

		qboolean send = qfalse;
		if (!currentGlobalPrint && (fcChanged || locationChanged || (teammateWithFlag && cooldownSatisfied)))
			send = qtrue;

		// don't send if the message is unchanged and it's been less than 2000ms since the last update (avoid spamming duplicates)
		const int FAKEOVERLAY_DUPLICATE_SPAM_TIME = 2000;
		qboolean longCooldownSatisfied = (!ent->client->pers.lastFakeOverlaySpamTime || now - ent->client->pers.lastFakeOverlaySpamTime >= FAKEOVERLAY_DUPLICATE_SPAM_TIME);
		qboolean messageChanged = (fcChanged || locationChanged || strcmp(msg, lastMsg[ent - g_entities]));
		if (!messageChanged && !longCooldownSatisfied)
			send = qfalse;

		if (send) {
			trap_SendServerCommand(ent - g_entities, teammateWithFlag ? msg : "cp \"\"");

			// set cooldown and note current fc/location/message for next time
			lastTeammateWithFlag[ent - g_entities] = teammateWithFlag;
			ent->client->pers.lastFakeOverlaySpamTime = now;
			memcpy(lastMsg[ent - g_entities], msg, sizeof(lastMsg[ent - g_entities]));
			memcpy(lastLocation[ent - g_entities], location, sizeof(lastLocation[ent - g_entities]));
		}
	}
}

extern teamgame_t teamgame;

static qboolean HasGoodFireableWeaponEquipped(gentity_t *ent) {
	assert(ent && ent->client);

	const int weapon = ent->client->ps.weapon;

	int ammoNeeded;
	switch (weapon) {
	case WP_REPEATER: ammoNeeded = weaponData[WP_REPEATER].altEnergyPerShot; break;
	case WP_FLECHETTE: ammoNeeded = weaponData[WP_FLECHETTE].altEnergyPerShot; break;
	case WP_ROCKET_LAUNCHER: ammoNeeded = weaponData[WP_ROCKET_LAUNCHER].energyPerShot; break;
	case WP_CONCUSSION: ammoNeeded = weaponData[WP_CONCUSSION].energyPerShot; break;
	default: return qfalse;;
	}

	const int ammo = ent->client->ps.ammo[weaponData[weapon].ammoIndex];
	if (ammo < ammoNeeded)
		return qfalse;

	return qtrue;
}

static qboolean IsBadGun(int weapon) {
	switch (weapon) {
	case WP_REPEATER: case WP_FLECHETTE: case WP_ROCKET_LAUNCHER: case WP_CONCUSSION: return qfalse;
	default: return qtrue;
	}
}

extern qboolean IsDroppedRedFlag(gentity_t *ent);
extern qboolean IsDroppedBlueFlag(gentity_t *ent);

extern void Cmd_Amtele_f(gentity_t *ent);

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame on fast clients.

If "g_synchronousClients 1" is set, this will be called exactly
once for each server frame, which makes for smooth demo recording.
==============
*/
void ClientThink_real( gentity_t *ent ) {
	gclient_t	*client;
	pmove_t		pm;
	int			oldEventSequence;
	int			msec;
	usercmd_t	*ucmd;
	qboolean	isNPC = qfalse;
	qboolean	controlledByPlayer = qfalse;
	qboolean	killJetFlags = qtrue;

	client = ent->client;

	if (client->pers.cmd.buttons & BUTTON_TALK)
		client->pers.chatboxUpTime = trap_Milliseconds();

	if (ent - g_entities < MAX_CLIENTS)
		level.lastThinkRealTime[ent - g_entities] = trap_Milliseconds();

	if (ent->s.eType == ET_NPC)
	{
		isNPC = qtrue;
	}

	// don't think if the client is not yet connected (and thus not yet spawned in)
	if (client->pers.connected != CON_CONNECTED && !isNPC) {
		return;
	}

	// This code was moved here from clientThink to fix a problem with g_synchronousClients 
	// being set to 1 when in vehicles. 
	if ( ent->s.number < MAX_CLIENTS && ent->client->ps.m_iVehicleNum )
	{//driving a vehicle
		if (g_entities[ent->client->ps.m_iVehicleNum].client)
		{
			gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

			if (veh->m_pVehicle &&
				veh->m_pVehicle->m_pPilot == (bgEntity_t *)ent)
			{ //only take input from the pilot...
				veh->client->ps.commandTime = ent->client->ps.commandTime;
				memcpy(&veh->m_pVehicle->m_ucmd, &ent->client->pers.cmd, sizeof(usercmd_t));
				if ( veh->m_pVehicle->m_ucmd.buttons & BUTTON_TALK )
				{ //forced input if "chat bubble" is up
					veh->m_pVehicle->m_ucmd.buttons = BUTTON_TALK;
					veh->m_pVehicle->m_ucmd.forwardmove = 0;
					veh->m_pVehicle->m_ucmd.rightmove = 0;
					veh->m_pVehicle->m_ucmd.upmove = 0;
				}
			}
		}
	}

	if (!(client->ps.pm_flags & PMF_FOLLOW))
	{
		if (g_gametype.integer == GT_SIEGE &&
			client->siegeClass != -1 &&
			bgSiegeClasses[client->siegeClass].saberStance)
		{ //the class says we have to use this stance set.
			if (!(bgSiegeClasses[client->siegeClass].saberStance & (1 << client->ps.fd.saberAnimLevel)))
			{ //the current stance is not in the bitmask, so find the first one that is.
				int i = SS_FAST;

				while (i < SS_NUM_SABER_STYLES)
				{
					if (bgSiegeClasses[client->siegeClass].saberStance & (1 << i))
					{
						if (i == SS_DUAL 
							&& client->ps.saberHolstered == 1 )
						{//one saber should be off, adjust saberAnimLevel accordinly
							client->ps.fd.saberAnimLevelBase = i;
							client->ps.fd.saberAnimLevel = SS_FAST;
							client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
						}
						else if ( i == SS_STAFF
							&& client->ps.saberHolstered == 1 
							&& client->saber[0].singleBladeStyle != SS_NONE)
						{//one saber or blade should be off, adjust saberAnimLevel accordinly
							client->ps.fd.saberAnimLevelBase = i;
							client->ps.fd.saberAnimLevel = client->saber[0].singleBladeStyle;
							client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
						}
						else
						{
							client->ps.fd.saberAnimLevelBase = client->ps.fd.saberAnimLevel = i;
							client->ps.fd.saberDrawAnimLevel = i;
						}
						break;
					}

					i++;
				}
			}
		}
		else if (client->saber[0].model[0] && client->saber[1].model[0])
		{ //with two sabs always use akimbo style
			if ( client->ps.saberHolstered == 1 )
			{//one saber should be off, adjust saberAnimLevel accordinly
				client->ps.fd.saberAnimLevelBase = SS_DUAL;
				client->ps.fd.saberAnimLevel = SS_FAST;
				client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
			}
			else
			{
				if ( !WP_SaberStyleValidForSaber( &client->saber[0], &client->saber[1], client->ps.saberHolstered, client->ps.fd.saberAnimLevel ) )
				{//only use dual style if the style we're trying to use isn't valid
					client->ps.fd.saberAnimLevelBase = client->ps.fd.saberAnimLevel = SS_DUAL;
				}
				client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
			}
		}
		else
		{
			if (client->saber[0].stylesLearned == (1<<SS_STAFF) )
			{ //then *always* use the staff style
				client->ps.fd.saberAnimLevelBase = SS_STAFF;
			}
			if ( client->ps.fd.saberAnimLevelBase == SS_STAFF )
			{//using staff style
				if ( client->ps.saberHolstered == 1 
					&& client->saber[0].singleBladeStyle != SS_NONE)
				{//one blade should be off, adjust saberAnimLevel accordinly
					client->ps.fd.saberAnimLevel = client->saber[0].singleBladeStyle;
					client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
				}
				else
				{
					client->ps.fd.saberAnimLevel = SS_STAFF;
					client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
				}
			}
		}
	}

	// mark the time, so the connection sprite can be removed
	ucmd = &ent->client->pers.cmd;

    if ( client && (client->ps.eFlags2&EF2_HELD_BY_MONSTER) && (!(ent->client->ps.pm_flags & PMF_FOLLOW)) )
	{
		G_HeldByMonster( ent, &ucmd );
	}

	// sanity check the command time to prevent speedup cheating
	if ( ucmd->serverTime > level.time + 200 ) {
		ucmd->serverTime = level.time + 200;
	}
	if ( ucmd->serverTime < level.time - 1000 ) {
		ucmd->serverTime = level.time - 1000;
	} 

	if (isNPC && (ucmd->serverTime - client->ps.commandTime) < 1)
	{
		ucmd->serverTime = client->ps.commandTime + 100;
	}

	msec = ucmd->serverTime - client->ps.commandTime;
	// following others may result in bad times, but we still want
	// to check for follow toggles
	if ( msec < 1 && client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		return;
	}

	if ( msec > 200 ) {
		msec = 200;
	}

	if ( pmove_msec.integer < 3 ) {
		trap_Cvar_Set("pmove_msec", "3");
	}
	else if (pmove_msec.integer > 50) {
		trap_Cvar_Set("pmove_msec", "50");
	}

	if ( pmove_fixed.integer || client->pers.pmoveFixed ) {
		ucmd->serverTime = ((ucmd->serverTime + pmove_msec.integer-1) / pmove_msec.integer) * pmove_msec.integer;
	}

	G_UpdateRaceBitMasks( client );

	//
	// check for exiting intermission
	//
	if ( level.intermissiontime ) 
	{
		if ( ent->s.number < MAX_CLIENTS
			|| client->NPC_class == CLASS_VEHICLE )
		{//players and vehicles do nothing in intermissions
			ClientIntermissionThink( client );
			return;
		}
	}

	RunFakeFCOverlay(ent);

	// spectators don't do much
	if ( client->sess.sessionTeam == TEAM_SPECTATOR || client->tempSpectate > level.time ) {
        CheckSpectatorInactivityTimer(client);
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
			return;
		}
		SpectatorThink( ent, ucmd );
		return;
	}

    if (ent->s.eType != ET_NPC)
    {
        // check for inactivity timer, but never drop the local client of a non-dedicated server
        if (!CheckPlayerInactivityTimer(client)) {
            return;
        }
    }

	if (ent && ent->client && (ent->client->ps.eFlags & EF_INVULNERABLE))
	{
		if (ent->client->invulnerableTimer <= level.time)
		{
			ent->client->ps.eFlags &= ~EF_INVULNERABLE;
		}
	}

	//Check if we should have a fullbody push effect around the player
	if (client->pushEffectTime > level.time)
	{
		client->ps.eFlags |= EF_BODYPUSH;
	}
	else if (client->pushEffectTime)
	{
		client->pushEffectTime = 0;
		client->ps.eFlags &= ~EF_BODYPUSH;
	}

	if (client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK))
	{
		client->ps.eFlags |= EF_JETPACK;
	}
	else
	{
		client->ps.eFlags &= ~EF_JETPACK;
	}

	if (g_fixDisarmFiring.integer && g_gametype.integer != GT_SIEGE) {
		const qboolean m1 = !!(ucmd->buttons & BUTTON_ATTACK);
		const qboolean m2 = !!(ucmd->buttons & BUTTON_ALT_ATTACK);
		if (ent->health > 0 && client->ps.pm_type != PM_DEAD) {
			if (client->disarmed && trap_Milliseconds() - client->disarmed < 5000 && (m1 || m2)) {
				if ((client->ps.weapon == WP_BRYAR_PISTOL && m2 && !m1) ||
					(client->ps.weapon == WP_BOWCASTER && m1 && !m2) ||
					(client->ps.weapon == WP_DEMP2 && m2 && !m1) ||
					(client->ps.weapon == WP_BRYAR_OLD && m2 && !m1)) {
					// these ones are fine
					//PrintIngame(ent - g_entities, "^0g_fixDisarmFiring: using permitted weapon\n");
					client->forcingDisarmedNoAttack = 0;
				}
				else {
					// cancel their mouse button, and continue doing this on subsequent thinks until the mouse button is actually physically released or until 5 real seconds pass
					client->forcingDisarmedNoAttack = trap_Milliseconds();
					ucmd->buttons &= ~(BUTTON_ATTACK | BUTTON_ALT_ATTACK);
					//PrintIngame(ent - g_entities, "^1g_fixDisarmFiring: forcingDisarmedNoAttack = %d\n", trap_Milliseconds());
				}
			}
			else if (client->forcingDisarmedNoAttack) {
				if ((client->ps.weapon == WP_BRYAR_PISTOL && m2 && !m1) ||
					(client->ps.weapon == WP_BOWCASTER && m1 && !m2) ||
					(client->ps.weapon == WP_DEMP2 && m2 && !m1) ||
					(client->ps.weapon == WP_BRYAR_OLD && m2 && !m1)) {
					// these ones are fine
					//PrintIngame(ent - g_entities, "^2g_fixDisarmFiring: using permitted weapon\n");
					client->forcingDisarmedNoAttack = 0;
				}
				else if (trap_Milliseconds() - client->forcingDisarmedNoAttack < 5000) {
					if (!m1 && !m2) {
						client->forcingDisarmedNoAttack = 0; // they actually physically released mouse button; stop canceling attack
						//PrintIngame(ent - g_entities, "^3g_fixDisarmFiring: forcingDisarmedNoAttack = 0\n");
					}
					else {
						ucmd->buttons &= ~(BUTTON_ATTACK | BUTTON_ALT_ATTACK); // they have not yet released; cancel the attack
						//PrintIngame(ent - g_entities, "^4g_fixDisarmFiring: ucmd->buttons removing attack buttons\n");
					}
				}
				else {
					//PrintIngame(ent - g_entities, "^5g_fixDisarmFiring: forcingDisarmedNoAttack = 0\n");
					client->forcingDisarmedNoAttack = 0;
				}
			}
			client->disarmed = 0;
		}
		else { // sanity check to clear these when dead
			client->disarmed = client->forcingDisarmedNoAttack = 0;
		}
	}

	int frozen = 0;
	if (g_vote_freezeUntilVote.integer && !ent->isAimPracticePack && client->pers.connected == CON_CONNECTED && !level.intermissionQueued && !level.intermissiontime) {
		if (level.voteTime && level.multiVoting && (ent->client->mGameFlags & PSG_CANVOTE) && !(ent->client->mGameFlags & PSG_VOTED) && (g_vote_freezeUntilVote.integer & (1 << 0))) {
			frozen |= 1;
		}
		if (level.activePugProposal && client->account && (g_vote_freezeUntilVote.integer & (1 << 1))) {
			qboolean partOfActiveProposal = qfalse;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				sortedClient_t *cl = level.activePugProposal->clients + i;
				if (cl->accountName[0] && cl->accountId == client->account->id) {
					partOfActiveProposal = qtrue;
					break;
				}
			}

			if (partOfActiveProposal) {
				const qboolean actuallyOnATeam = PlayerIsInTeamInActivePugProposal(client->account->id);

				if (actuallyOnATeam) {
					qboolean hasVoted = qfalse;
					if (level.activePugProposal->suggested.valid && level.activePugProposal->suggestedVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->highestCaliber.valid && level.activePugProposal->highestCaliberVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->fairest.valid && level.activePugProposal->fairestVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->desired.valid && level.activePugProposal->desiredVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->semiDesired.valid && level.activePugProposal->semiDesiredVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->inclusive.valid && level.activePugProposal->inclusiveVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->firstChoice.valid && level.activePugProposal->firstChoiceVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->suggested.valid && level.activePugProposal->suggestedVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->highestCaliber.valid && level.activePugProposal->highestCaliberVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->fairest.valid && level.activePugProposal->fairestVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->desired.valid && level.activePugProposal->desiredVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->semiDesired.valid && level.activePugProposal->semiDesiredVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->inclusive.valid && level.activePugProposal->inclusiveVoteClientsBlue & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->firstChoice.valid && level.activePugProposal->firstChoiceVoteClientsRed & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->votedToRerollClients & (1 << ent->s.number))
						hasVoted = qtrue;
					else if (level.activePugProposal->votedToCancelClients & (1 << ent->s.number))
						hasVoted = qtrue;

					if (!hasVoted)
						frozen |= 2;
				}
			}
		}
	}

	if (frozen) {
		const int now = trap_Milliseconds();
		if (!toldCantMoveTime[ent->s.number] || now - toldCantMoveTime[ent->s.number] >= 15000) {
			toldCantMoveTime[ent->s.number] = now;
			firstToldCantMoveTime[ent->s.number] = now;
		}
		else {
			if (now - firstToldCantMoveTime[ent->s.number] >= 60000 && now - toldCantMoveTime[ent->s.number] >= 1000) {
				if (!((frozen & 1) && level.voteBanPhase)) {
					if ((frozen & 2) && !(frozen & 1))
						trap_SendServerCommand(ent->s.number, "cp \"^1VOTA ^2VOTA ^3VOTA ^4VOTA ^5VOTA ^6VOTA ^7VOTA ^8VOTA ^9VOTA ^0VOTA\n^1VOTA ^2VOTA ^3VOTA ^4VOTA ^5VOTA ^6VOTA ^7VOTA ^8VOTA ^9VOTA ^0VOTA\"");
					else
						TeamGenerator_QueueServerMessageInChat(ent->s.number, "^1VOTA ^2VOTA ^3VOTA ^4VOTA ^5VOTA ^6VOTA ^7VOTA ^8VOTA ^9VOTA ^0VOTA ^1VOTA ^2VOTA ^3VOTA ^4VOTA ^5VOTA ^6VOTA ^7VOTA ^8VOTA ^9VOTA ^0VOTA");
				}
				toldCantMoveTime[ent->s.number] = now;

				if (g_vote_fadeToBlack.integer <= 4) {
					if (!client->ps.fallingToDeath)
						client->ps.fallingToDeath = level.time - 2000;
					client->pers.fakeFallFadeToBlack = qtrue;
				}

				if (g_vote_audioMotivation.integer && !((frozen & 1) && level.voteBanPhase)) {
					int index = G_SoundIndex("sound/chars/r2d2/misc/pain100");
					if (index) {
						gentity_t *te = G_Sound(ent, CHAN_AUTO, index);
						te->r.svFlags |= SVF_SINGLECLIENT;
						te->r.singleClient = ent->s.number;
						te->r.svFlags &= ~SVF_BROADCAST;
					}


					index = G_SoundIndex("sound/chars/tusken/misc/anger1");
					if (index) {
						gentity_t *te = G_Sound(ent, CHAN_AUTO, index);
						te->r.svFlags |= SVF_SINGLECLIENT;
						te->r.singleClient = ent->s.number;
						te->r.svFlags &= ~SVF_BROADCAST;
					}

					index = G_SoundIndex("sound/chars/howler/howl");
					if (index) {
						gentity_t *te = G_Sound(ent, CHAN_AUTO, index);
						te->r.svFlags |= SVF_SINGLECLIENT;
						te->r.singleClient = ent->s.number;
						te->r.svFlags &= ~SVF_BROADCAST;
					}

					index = G_SoundIndex("sound/chars/howler/misc/pain100");
					if (index) {
						gentity_t *te = G_Sound(ent, CHAN_AUTO, index);
						te->r.svFlags |= SVF_SINGLECLIENT;
						te->r.singleClient = ent->s.number;
						te->r.svFlags &= ~SVF_BROADCAST;
					}

					index = G_SoundIndex("sound/chars/howler/misc/death1");
					if (index) {
						gentity_t *te = G_Sound(ent, CHAN_AUTO, index);
						te->r.svFlags |= SVF_SINGLECLIENT;
						te->r.singleClient = ent->s.number;
						te->r.svFlags &= ~SVF_BROADCAST;
					}
				}
			}
			else if (now - firstToldCantMoveTime[ent->s.number] >= 30000 && now - toldCantMoveTime[ent->s.number] >= 3000) {
				if (!((frozen & 1) && level.voteBanPhase)) {
					if ((frozen & 2) && !(frozen & 1))
						trap_SendServerCommand(ent->s.number, "cp \"VOTE!\"");
					else
						TeamGenerator_QueueServerMessageInChat(ent->s.number, "VOTE!");
				}
				toldCantMoveTime[ent->s.number] = now;

				if (g_vote_fadeToBlack.integer <= 3) {
					if (!client->ps.fallingToDeath)
						client->ps.fallingToDeath = level.time - 2000;
					client->pers.fakeFallFadeToBlack = qtrue;
				}
			}
			else if (now - toldCantMoveTime[ent->s.number] >= 10000) {
				if (!((frozen & 1) && level.voteBanPhase)) {
					if ((frozen & 2) && !(frozen & 1))
						trap_SendServerCommand(ent->s.number, "cp \"Vote, and you will be unfrozen.\"");
					else
						TeamGenerator_QueueServerMessageInChat(ent->s.number, "Vote, and you will be unfrozen.");
				}
				toldCantMoveTime[ent->s.number] = now;

				if (g_vote_fadeToBlack.integer <= 2) {
					if (!client->ps.fallingToDeath)
						client->ps.fallingToDeath = level.time - 2000;
					client->pers.fakeFallFadeToBlack = qtrue;
				}
			}
		}

		if (g_vote_fadeToBlack.integer == 1) {
			if (!client->ps.fallingToDeath)
				client->ps.fallingToDeath = level.time - 2000;
			client->pers.fakeFallFadeToBlack = qtrue;
		}
	}
	else {
		toldCantMoveTime[ent->s.number] = firstToldCantMoveTime[ent->s.number] = 0;
		if (client->pers.fakeFallFadeToBlack) {
			client->pers.fakeFallFadeToBlack = qfalse;
			client->ps.fallingToDeath = 0;
		}
	}

    //OSP: pause
    if ( frozen || (level.pause.state != PAUSE_NONE && !client->sess.inRacemode && !ent->isAimPracticePack)) {
        ucmd->buttons = 0;
        ucmd->forwardmove = 0;
        ucmd->rightmove = 0;
        ucmd->upmove = 0;
        ucmd->generic_cmd = 0;
        client->ps.pm_type = PM_FREEZE;
    } else if ( client->noclip ) {
		client->ps.pm_type = PM_NOCLIP;
	} else if ( client->ps.eFlags & EF_DISINTEGRATION ) {
		client->ps.pm_type = PM_NOCLIP;
	} else if ( client->ps.stats[STAT_HEALTH] <= 0 ) {
		client->ps.pm_type = PM_DEAD;
	} else {
		if (client->ps.forceGripChangeMovetype)
		{
			client->ps.pm_type = client->ps.forceGripChangeMovetype;
		}
		else
		{
			if (client->jetPackOn)
			{
				client->ps.pm_type = PM_JETPACK;
				client->ps.eFlags |= EF_JETPACK_ACTIVE;
				killJetFlags = qfalse;
			}
			else
			{
				client->ps.pm_type = PM_NORMAL;
			}
		}
	}

	if (killJetFlags)
	{
		client->ps.eFlags &= ~EF_JETPACK_ACTIVE;
		client->ps.eFlags &= ~EF_JETPACK_FLAMING;
	}

#define	SLOWDOWN_DIST	128.0f
#define	MIN_NPC_SPEED	16.0f

	if (client->bodyGrabIndex != ENTITYNUM_NONE)
	{
		gentity_t *grabbed = &g_entities[client->bodyGrabIndex];

		if (!grabbed->inuse || grabbed->s.eType != ET_BODY ||
			(grabbed->s.eFlags & EF_DISINTEGRATION) ||
			(grabbed->s.eFlags & EF_NODRAW))
		{
			if (grabbed->inuse && grabbed->s.eType == ET_BODY)
			{
				grabbed->s.ragAttach = 0;
			}
			client->bodyGrabIndex = ENTITYNUM_NONE;
		}
		else
		{
			mdxaBone_t rhMat;
			vec3_t rhOrg, tAng;
			vec3_t bodyDir;
			float bodyDist;

			ent->client->ps.forceHandExtend = HANDEXTEND_DRAGGING;

			if (ent->client->ps.forceHandExtendTime < level.time + 500)
			{
				ent->client->ps.forceHandExtendTime = level.time + 1000;
			}

			VectorSet(tAng, 0, ent->client->ps.viewangles[YAW], 0);
			trap_G2API_GetBoltMatrix(ent->ghoul2, 0, 0, &rhMat, tAng, ent->client->ps.origin, level.time,
				NULL, ent->modelScale); //0 is always going to be right hand bolt
			BG_GiveMeVectorFromMatrix(&rhMat, ORIGIN, rhOrg);

			VectorSubtract(rhOrg, grabbed->r.currentOrigin, bodyDir);
			bodyDist = VectorLength(bodyDir);

			if (bodyDist > 40.0f)
			{ //can no longer reach
				grabbed->s.ragAttach = 0;
				client->bodyGrabIndex = ENTITYNUM_NONE;
			}
			else if (bodyDist > 24.0f)
			{
				bodyDir[2] = 0; //don't want it floating
				VectorAdd(grabbed->epVelocity, bodyDir, grabbed->epVelocity);
				G_Sound(grabbed, CHAN_AUTO, G_SoundIndex("sound/player/roll1.wav"));
			}
		}
	}
	else if (ent->client->ps.forceHandExtend == HANDEXTEND_DRAGGING)
	{
		ent->client->ps.forceHandExtend = HANDEXTEND_WEAPONREADY;
	}

	if (ent->NPC && ent->s.NPC_class != CLASS_VEHICLE && !ent->isAimPracticePack) //vehicles manage their own speed
	{
		//FIXME: swoop should keep turning (and moving forward?) for a little bit?
		if ( ent->NPC->combatMove == qfalse )
		{
			//if ( !(ucmd->buttons & BUTTON_USE) )
			if (1)
			{//Not leaning
				qboolean Flying = (ucmd->upmove && (ent->client->ps.eFlags2&EF2_FLYING));
				qboolean Climbing = (ucmd->upmove && ent->watertype&CONTENTS_LADDER );

				if ( ucmd->forwardmove || ucmd->rightmove || Flying )
				{
					{//In - Formation NPCs set thier desiredSpeed themselves
						if ( ucmd->buttons & BUTTON_WALKING )
						{
							ent->NPC->desiredSpeed = NPC_GetWalkSpeed( ent );
						}
						else//running
						{
							ent->NPC->desiredSpeed = NPC_GetRunSpeed( ent );
						}

						if ( ent->NPC->currentSpeed >= 80 && !controlledByPlayer )
						{//At higher speeds, need to slow down close to stuff
							//Slow down as you approach your goal
						//	if ( ent->NPC->distToGoal < SLOWDOWN_DIST && client->race != RACE_BORG && !(ent->NPC->aiFlags&NPCAI_NO_SLOWDOWN) )//128
							if ( ent->NPC->distToGoal < SLOWDOWN_DIST && !(ent->NPC->aiFlags&NPCAI_NO_SLOWDOWN) )//128
							{
								if ( ent->NPC->desiredSpeed > MIN_NPC_SPEED )
								{
									float slowdownSpeed = ((float)ent->NPC->desiredSpeed) * ent->NPC->distToGoal / SLOWDOWN_DIST;

									ent->NPC->desiredSpeed = ceil(slowdownSpeed);
									if ( ent->NPC->desiredSpeed < MIN_NPC_SPEED )
									{//don't slow down too much
										ent->NPC->desiredSpeed = MIN_NPC_SPEED;
									}
								}
							}
						}
					}
				}
				else if ( Climbing )
				{
					ent->NPC->desiredSpeed = ent->NPC->stats.walkSpeed;
				}
				else
				{//We want to stop
					ent->NPC->desiredSpeed = 0;
				}

				NPC_Accelerate( ent, qfalse, qfalse );

				if ( ent->NPC->currentSpeed <= 24 && ent->NPC->desiredSpeed < ent->NPC->currentSpeed )
				{//No-one walks this slow
					client->ps.speed = ent->NPC->currentSpeed = 0;//Full stop
					ucmd->forwardmove = 0;
					ucmd->rightmove = 0;
				}
				else
				{
					if ( ent->NPC->currentSpeed <= ent->NPC->stats.walkSpeed )
					{//Play the walkanim
						ucmd->buttons |= BUTTON_WALKING;
					}
					else
					{
						ucmd->buttons &= ~BUTTON_WALKING;
					}

					if ( ent->NPC->currentSpeed > 0 )
					{//We should be moving
						if ( Climbing || Flying )
						{
							if ( !ucmd->upmove )
							{//We need to force them to take a couple more steps until stopped
								ucmd->upmove = ent->NPC->last_ucmd.upmove;
							}
						}
						else if ( !ucmd->forwardmove && !ucmd->rightmove )
						{//We need to force them to take a couple more steps until stopped
							ucmd->forwardmove = ent->NPC->last_ucmd.forwardmove;
							ucmd->rightmove = ent->NPC->last_ucmd.rightmove;
						}
					}

					client->ps.speed = ent->NPC->currentSpeed;
					//rwwFIXMEFIXME: do this and also check for all real client
					if (1)
					{
						//Slow down on turns - don't orbit!!!
						float turndelta = 0; 
						// if the NPC is locked into a Yaw, we want to check the lockedDesiredYaw...otherwise the NPC can't walk backwards, because it always thinks it trying to turn according to desiredYaw
						//if( client->renderInfo.renderFlags & RF_LOCKEDANGLE ) // yeah I know the RF_ flag is a pretty ugly hack...
						if (0) //rwwFIXMEFIXME: ...
						{	
							turndelta = (180 - fabs( AngleDelta( ent->r.currentAngles[YAW], ent->NPC->lockedDesiredYaw ) ))/180;
						}
						else
						{
							turndelta = (180 - fabs( AngleDelta( ent->r.currentAngles[YAW], ent->NPC->desiredYaw ) ))/180;
						}
												
						if ( turndelta < 0.75f )
						{
							client->ps.speed = 0;
						}
						else if ( ent->NPC->distToGoal < 100 && turndelta < 1.0 )
						{//Turn is greater than 45 degrees or closer than 100 to goal
							client->ps.speed = floor(((float)(client->ps.speed))*turndelta);
						}
					}
				}
			}
		}
		else
		{	
			ent->NPC->desiredSpeed = ( ucmd->buttons & BUTTON_WALKING ) ? NPC_GetWalkSpeed( ent ) : NPC_GetRunSpeed( ent );

			client->ps.speed = ent->NPC->desiredSpeed;
		}

		if (ucmd->buttons & BUTTON_WALKING)
		{ //sort of a hack I guess since MP handles walking differently from SP (has some proxy cheat prevention methods)
			if (ucmd->forwardmove > 64)
			{
				ucmd->forwardmove = 64;	
			}
			else if (ucmd->forwardmove < -64)
			{
				ucmd->forwardmove = -64;
			}
			
			if (ucmd->rightmove > 64)
			{
				ucmd->rightmove = 64;
			}
			else if ( ucmd->rightmove < -64)
			{
				ucmd->rightmove = -64;
			}

		}
		client->ps.basespeed = client->ps.speed;
	}
	else if (!client->ps.m_iVehicleNum &&
		(!ent->NPC || ent->s.NPC_class != CLASS_VEHICLE)) //if riding a vehicle it will manage our speed and such
	{
		// set speed
		client->ps.speed = g_speed.value;

		//Check for a siege class speed multiplier
		if (g_gametype.integer == GT_SIEGE &&
			client->siegeClass != -1)
		{
			client->ps.speed *= bgSiegeClasses[client->siegeClass].speed;
		}

		if (client->bodyGrabIndex != ENTITYNUM_NONE)
		{ //can't go nearly as fast when dragging a body around
			client->ps.speed *= 0.2f;
		}

		if (InstagibEnabled() && client->sess.sessionTeam != TEAM_SPECTATOR && !client->sess.inRacemode) {
			client->ps.speed *= 1.7f; // instagib base speed equivalent to using force speed (they can't use force speed anyway)
		}

		if (client->rockPaperScissorsStartTime && !(client->rockPaperScissorsBothChosenTime && level.time - client->rockPaperScissorsBothChosenTime >= 3050))
			client->ps.speed = 0;

		client->ps.basespeed = client->ps.speed;
	}

	if ( !ent->NPC || !(ent->NPC->aiFlags&NPCAI_CUSTOM_GRAVITY) )
	{//use global gravity
		if (ent->NPC && ent->s.NPC_class == CLASS_VEHICLE &&
			ent->m_pVehicle && ent->m_pVehicle->m_pVehicleInfo->gravity)
		{ //use custom veh gravity
			client->ps.gravity = ent->m_pVehicle->m_pVehicleInfo->gravity;
		}
		else
		{
			if (ent->client->inSpaceIndex && ent->client->inSpaceIndex != ENTITYNUM_NONE)
			{ //in space, so no gravity...
				client->ps.gravity = 1.0f;
				if (ent->s.number < MAX_CLIENTS)
				{
					VectorScale(client->ps.velocity, 0.8f, client->ps.velocity);
				}
			}
			else
			{
				if (client->ps.eFlags2 & EF2_SHIP_DEATH)
				{ //float there
					VectorClear(client->ps.velocity);
					client->ps.gravity = 1.0f;
				}
				else
				{
					client->ps.gravity = g_gravity.value;
				}
			}
		}
	}

	if (ent->isAimPracticePack)
		client->ps.speed = client->ps.basespeed = g_speed.value; // i guess?

	if (ent->client->ps.duelInProgress)
	{
		gentity_t *duelAgainst = &g_entities[ent->client->ps.duelIndex];

		//Keep the time updated, so once this duel ends this player can't engage in a duel for another
		//10 seconds. This will give other people a chance to engage in duels in case this player wants
		//to engage again right after he's done fighting and someone else is waiting.
		ent->client->ps.fd.privateDuelTime = level.time + 10000;

		if (ent->client->ps.duelTime < level.time)
		{
			//Bring out the sabers
			if (duelAgainst 
				&& duelAgainst->client 
				&& duelAgainst->inuse )
			{
				if (g_gametype.integer == GT_CTF){
					if (((duelAgainst->client->ps.weapon == WP_BRYAR_PISTOL)
						||(duelAgainst->client->ps.weapon == WP_SABER)
						||(duelAgainst->client->ps.weapon == WP_MELEE))
					&& duelAgainst->client->ps.duelTime){
						G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 2);
						duelAgainst->client->ps.duelTime = 0;
						duelAgainst->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
						duelAgainst->client->ps.weapon = WP_BRYAR_PISTOL ;
						duelAgainst->client->ps.weaponTime = 0;
						duelAgainst->client->ps.weaponstate = 0;
					}
				} else {
					if (duelAgainst->client->ps.weapon == WP_SABER 
						&& duelAgainst->client->ps.saberHolstered 
						&& duelAgainst->client->ps.duelTime){

						duelAgainst->client->ps.saberHolstered = 0;

						if (duelAgainst->client->saber[0].soundOn)
						{
							G_Sound(duelAgainst, CHAN_AUTO, duelAgainst->client->saber[0].soundOn);
						}
						if (duelAgainst->client->saber[1].soundOn)
						{
							G_Sound(duelAgainst, CHAN_AUTO, duelAgainst->client->saber[1].soundOn);
						}

						G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 2);

						duelAgainst->client->ps.duelTime = 0;
					}
				}
			}
		} else if (g_gametype.integer != GT_CTF) {
			client->ps.speed = 0;
			client->ps.basespeed = 0;
			ucmd->forwardmove = 0;
			ucmd->rightmove = 0;
			ucmd->upmove = 0;
		}

		if (!duelAgainst || !duelAgainst->client || !duelAgainst->inuse ||
            duelAgainst->client->ps.duelIndex != ent->s.number)
		{
			ent->client->ps.duelInProgress = 0;
			G_AddEvent(ent, EV_PRIVATE_DUEL, 0);

			//remove melee, add saber, if necessary
			if (g_gametype.integer == GT_CTF){
				ent->client->ps.stats[STAT_WEAPONS] = ent->client->preduelWeaps;
			}
		}
		else if (duelAgainst->health < 1 || duelAgainst->client->ps.stats[STAT_HEALTH] < 1)
		{
			ent->client->ps.duelInProgress = 0;
			duelAgainst->client->ps.duelInProgress = 0;

			G_AddEvent(ent, EV_PRIVATE_DUEL, 0);
			G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 0);

			//Winner gets full health.. providing he's still alive
			if (ent->health > 0 && ent->client->ps.stats[STAT_HEALTH] > 0)
			{
				if (ent->health < ent->client->ps.stats[STAT_MAX_HEALTH])
				{
					ent->client->ps.stats[STAT_HEALTH] = ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
					ent->client->ps.stats[STAT_ARMOR] = 25;
				}

				if (g_spawnInvulnerability.integer)
				{
					ent->client->ps.eFlags |= EF_INVULNERABLE;
					ent->client->invulnerableTimer = level.time + g_spawnInvulnerability.integer;
				}

				//remove melee, add saber, if necessary
				if (g_gametype.integer == GT_CTF){
					ent->client->ps.stats[STAT_WEAPONS] = ent->client->preduelWeaps;
				}
			}

			//Private duel announcements are now made globally because we only want one duel at a time.
			if (ent->health > 0 && ent->client->ps.stats[STAT_HEALTH] > 0)
			{
				trap_SendServerCommand( -1, va("cp \"%s^7 %s %s!\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELWINNER"), duelAgainst->client->pers.netname) );
				trap_SendServerCommand( -1, va("print \"%s^7 %s %s!\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELWINNER"), duelAgainst->client->pers.netname) );
			}
			else
			{ //it was a draw, because we both managed to die in the same frame
				trap_SendServerCommand( -1, va("cp \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELTIE")) );
				trap_SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELTIE")) );
			}
		}
		else
		{
			vec3_t vSub;
			float subLen = 0;
			int duelDistLimit = (g_gametype.integer == GT_CTF ) ? 2048 : 1024;

			VectorSubtract(ent->client->ps.origin, duelAgainst->client->ps.origin, vSub);
			subLen = VectorLength(vSub);

			if (subLen >= duelDistLimit)
			{
				ent->client->ps.duelInProgress = 0;
				duelAgainst->client->ps.duelInProgress = 0;

				G_AddEvent(ent, EV_PRIVATE_DUEL, 0);
				G_AddEvent(duelAgainst, EV_PRIVATE_DUEL, 0);

				if (g_gametype.integer == GT_CTF){
					ent->client->ps.stats[STAT_WEAPONS] = ent->client->preduelWeaps;
					duelAgainst->client->ps.stats[STAT_WEAPONS] = duelAgainst->client->preduelWeaps;
					ent->client->ps.stats[STAT_HEALTH] = ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
					duelAgainst->client->ps.stats[STAT_HEALTH] = duelAgainst->health = duelAgainst->client->ps.stats[STAT_MAX_HEALTH];
					ent->client->ps.stats[STAT_ARMOR] = 25;
					duelAgainst->client->ps.stats[STAT_ARMOR] = 25;
				}

				trap_SendServerCommand( -1, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELSTOP")) );
			}
		}
	}

	if (ent->client->doingThrow > level.time)
	{
		gentity_t *throwee = &g_entities[ent->client->throwingIndex];

		if (!throwee->inuse || !throwee->client || throwee->health < 1 ||
			throwee->client->sess.sessionTeam == TEAM_SPECTATOR ||
			(throwee->client->ps.pm_flags & PMF_FOLLOW) ||
			throwee->client->throwingIndex != ent->s.number)
		{
			ent->client->doingThrow = 0;
			ent->client->ps.forceHandExtend = HANDEXTEND_NONE;

			if (throwee->inuse && throwee->client)
			{
				throwee->client->ps.heldByClient = 0;
				throwee->client->beingThrown = 0;

				if (throwee->client->ps.forceHandExtend != HANDEXTEND_POSTTHROWN)
				{
					throwee->client->ps.forceHandExtend = HANDEXTEND_NONE;
				}
			}
		}
	}

	if (ent->client->beingThrown > level.time)
	{
		gentity_t *thrower = &g_entities[ent->client->throwingIndex];

		if (!thrower->inuse || !thrower->client || thrower->health < 1 ||
			thrower->client->sess.sessionTeam == TEAM_SPECTATOR ||
			(thrower->client->ps.pm_flags & PMF_FOLLOW) ||
			thrower->client->throwingIndex != ent->s.number)
		{
			ent->client->ps.heldByClient = 0;
			ent->client->beingThrown = 0;

			if (ent->client->ps.forceHandExtend != HANDEXTEND_POSTTHROWN)
			{
				ent->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}

			if (thrower->inuse && thrower->client)
			{
				thrower->client->doingThrow = 0;
				thrower->client->ps.forceHandExtend = HANDEXTEND_NONE;
			}
		}
		else if (thrower->inuse && thrower->client && thrower->ghoul2 &&
			trap_G2_HaveWeGhoul2Models(thrower->ghoul2))
		{
#if 0
			int lHandBolt = trap_G2API_AddBolt(thrower->ghoul2, 0, "*l_hand");
			int pelBolt = trap_G2API_AddBolt(thrower->ghoul2, 0, "pelvis");


			if (lHandBolt != -1 && pelBolt != -1)
#endif
			{
				float pDif = 40.0f;
				vec3_t boltOrg, pBoltOrg;
				vec3_t tAngles;
				vec3_t vDif;
				vec3_t entDir, otherAngles;
				vec3_t fwd, right;

				//Always look at the thrower.
				VectorSubtract( thrower->client->ps.origin, ent->client->ps.origin, entDir );
				VectorCopy( ent->client->ps.viewangles, otherAngles );
				otherAngles[YAW] = vectoyaw( entDir );
				SetClientViewAngle( ent, otherAngles );

				VectorCopy(thrower->client->ps.viewangles, tAngles);
				tAngles[PITCH] = tAngles[ROLL] = 0;

				//Get the direction between the pelvis and position of the hand
#if 0
				mdxaBone_t boltMatrix, pBoltMatrix;

				trap_G2API_GetBoltMatrix(thrower->ghoul2, 0, lHandBolt, &boltMatrix, tAngles, thrower->client->ps.origin, level.time, 0, thrower->modelScale);
				boltOrg[0] = boltMatrix.matrix[0][3];
				boltOrg[1] = boltMatrix.matrix[1][3];
				boltOrg[2] = boltMatrix.matrix[2][3];

				trap_G2API_GetBoltMatrix(thrower->ghoul2, 0, pelBolt, &pBoltMatrix, tAngles, thrower->client->ps.origin, level.time, 0, thrower->modelScale);
				pBoltOrg[0] = pBoltMatrix.matrix[0][3];
				pBoltOrg[1] = pBoltMatrix.matrix[1][3];
				pBoltOrg[2] = pBoltMatrix.matrix[2][3];
#else //above tends to not work once in a while, for various reasons I suppose.
				VectorCopy(thrower->client->ps.origin, pBoltOrg);
				AngleVectors(tAngles, fwd, right, 0);
				boltOrg[0] = pBoltOrg[0] + fwd[0]*8 + right[0]*pDif;
				boltOrg[1] = pBoltOrg[1] + fwd[1]*8 + right[1]*pDif;
				boltOrg[2] = pBoltOrg[2];
#endif
				VectorSubtract(ent->client->ps.origin, boltOrg, vDif);
				if (VectorLength(vDif) > 32.0f && (thrower->client->doingThrow - level.time) < 4500)
				{ //the hand is too far away, and can no longer hold onto us, so escape.
					ent->client->ps.heldByClient = 0;
					ent->client->beingThrown = 0;
					thrower->client->doingThrow = 0;

					thrower->client->ps.forceHandExtend = HANDEXTEND_NONE;
					G_EntitySound( thrower, CHAN_VOICE, G_SoundIndex("*pain25.wav") );

					ent->client->ps.forceDodgeAnim = 2;
					ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
					ent->client->ps.forceHandExtendTime = level.time + 500;
					ent->client->ps.velocity[2] = 400;
					G_PreDefSound(ent, PDSOUND_FORCEJUMP);
				}
				else if ((client->beingThrown - level.time) < 4000)
				{ //step into the next part of the throw, and go flying back
					float vScale = 400.0f;
					ent->client->ps.forceHandExtend = HANDEXTEND_POSTTHROWN;
					ent->client->ps.forceHandExtendTime = level.time + 1200;
					ent->client->ps.forceDodgeAnim = 0;

					thrower->client->ps.forceHandExtend = HANDEXTEND_POSTTHROW;
					thrower->client->ps.forceHandExtendTime = level.time + 200;

					ent->client->ps.heldByClient = 0;

					ent->client->ps.heldByClient = 0;
					ent->client->beingThrown = 0;
					thrower->client->doingThrow = 0;

					AngleVectors(thrower->client->ps.viewangles, vDif, 0, 0);
					ent->client->ps.velocity[0] = vDif[0]*vScale;
					ent->client->ps.velocity[1] = vDif[1]*vScale;
					ent->client->ps.velocity[2] = 400;

					G_EntitySound( ent, CHAN_VOICE, G_SoundIndex("*pain100.wav") );
					G_EntitySound( thrower, CHAN_VOICE, G_SoundIndex("*jump1.wav") );

					//Set the thrower as the "other killer", so if we die from fall/impact damage he is credited.
					ent->client->ps.otherKiller = thrower->s.number;
					ent->client->ps.otherKillerTime = level.time + 8000;
					ent->client->ps.otherKillerDebounceTime = level.time + 100;
				}
				else
				{ //see if we can move to be next to the hand.. if it's not clear, break the throw.
					vec3_t intendedOrigin;
					trace_t tr;
					trace_t tr2;

					VectorSubtract(boltOrg, pBoltOrg, vDif);
					VectorNormalize(vDif);

					VectorClear(ent->client->ps.velocity);
					intendedOrigin[0] = pBoltOrg[0] + vDif[0]*pDif;
					intendedOrigin[1] = pBoltOrg[1] + vDif[1]*pDif;
					intendedOrigin[2] = thrower->client->ps.origin[2];

					trap_Trace(&tr, intendedOrigin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, ent->clipmask);
					trap_Trace(&tr2, ent->client->ps.origin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, CONTENTS_SOLID);

					if (tr.fraction == 1.0 && !tr.startsolid && tr2.fraction == 1.0 && !tr2.startsolid)
					{
						VectorCopy(intendedOrigin, ent->client->ps.origin);
						
						if ((client->beingThrown - level.time) < 4800)
						{
							ent->client->ps.heldByClient = thrower->s.number+1;
						}
					}
					else
					{ //if the guy can't be put here then it's time to break the throw off.
						ent->client->ps.heldByClient = 0;
						ent->client->beingThrown = 0;
						thrower->client->doingThrow = 0;

						thrower->client->ps.forceHandExtend = HANDEXTEND_NONE;
						G_EntitySound( thrower, CHAN_VOICE, G_SoundIndex("*pain25.wav") );

						ent->client->ps.forceDodgeAnim = 2;
						ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
						ent->client->ps.forceHandExtendTime = level.time + 500;
						ent->client->ps.velocity[2] = 400;
						G_PreDefSound(ent, PDSOUND_FORCEJUMP);
					}
				}
			}
		}
	}
	else if (ent->client->ps.heldByClient)
	{
		ent->client->ps.heldByClient = 0;
	}

	// set up for pmove
	oldEventSequence = client->ps.eventSequence;

	memset (&pm, 0, sizeof(pm));

	if ( ent->flags & FL_FORCE_GESTURE ) {
		ent->flags &= ~FL_FORCE_GESTURE;
		ent->client->pers.cmd.buttons |= BUTTON_GESTURE;
	}

	if (ent->client && ent->client->ps.fallingToDeath && !(ent->client->pers.fakeFallFadeToBlack && g_vote_fadeToBlack.integer) &&
		(level.time - FALL_FADE_TIME) > ent->client->ps.fallingToDeath)
	{ //die!
		if (ent->health > 0)
		{
			gentity_t *otherKiller = ent;
			if (ent->client->ps.otherKillerTime > level.time &&
				ent->client->ps.otherKiller != ENTITYNUM_NONE)
			{
				otherKiller = &g_entities[ent->client->ps.otherKiller];

				if (!otherKiller->inuse)
				{
					otherKiller = ent;
				}
			}
			G_Damage(ent, otherKiller, otherKiller, NULL, ent->client->ps.origin, 9999, DAMAGE_NO_PROTECTION, MOD_FALLING);

			G_MuteSound(ent->s.number, CHAN_VOICE); //stop screaming, because you are dead!
		}
	}
	else if (ent->client && level.pause.state == PAUSE_NONE && ((ent->client->account && ent->client->account->flags & ACCOUNTFLAG_BOOST_SELFKILLBOOST && g_boost.integer) || (ent->r.svFlags & SVF_BOT)) && g_gametype.integer >= GT_TEAM && !IsRacerOrSpectator(ent) && ent->health > 0) {
		// boost: force sk when falling into pit and it's appropriate to sk
		qboolean myFlagIsAtHome = !!(ent->client->sess.sessionTeam == TEAM_RED ? (teamgame.redStatus == FLAG_ATBASE) : (teamgame.blueStatus == FLAG_ATBASE));

		qboolean doSk = qfalse;
		if (HasFlag(ent)) {
			// has flag: only sk if actually fading to black and flag is at home
			if (ent->client->ps.fallingToDeath && myFlagIsAtHome)
				doSk = qtrue;
		}
		else {
			// doesn't have flag: sk if actually fading or in pit location generally
			if (ent->client->ps.fallingToDeath) {
				doSk = qtrue;
			}
			else {
				char locBuf[64] = { 0 };
				Team_GetLocation(ent, locBuf, sizeof(locBuf), qtrue);
				if (locBuf[0] && !Q_stricmp(locBuf, "Pit")) {

					// sanity check: don't sk if the "Pit" location is the sole location in the map
					static qboolean secondLocationExists = qfalse, initialized = qfalse;
					if (!initialized) {
						initialized = qtrue;
						char testBuf[64] = { 0 };
						trap_GetConfigstring(CS_LOCATIONS + 1, testBuf, sizeof(testBuf));
						if (testBuf[0])
							secondLocationExists = qtrue;
					}

					if (secondLocationExists)
						doSk = qtrue;
				}
			}
		}

		if (doSk && !(ent->client->ps.fallingToDeath && ent->client->pers.fakeFallFadeToBlack && g_vote_fadeToBlack.integer)) {
			ent->flags &= ~FL_GODMODE;
			ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
			player_die(ent, ent, ent, 100000, MOD_SUICIDE);
		}
	}

	if (ent->health > 0 && level.pause.state == PAUSE_NONE) {
		if ((ent->client && ent->client->account && g_boost.integer)) {
			// boost: auto TH/TE
			AutoTHTE(ent);
		}

		if (ent->client && ent->client->account && ent->client->account->flags & ACCOUNTFLAG_BOOST_FIXINSTAYAW && g_boost.integer && level.pause.state == PAUSE_NONE) {
			if (ent->client->pers.lastSpawnTime >= level.time - 250) {
				if (fabs(AngleDelta(ent->client->ps.viewangles[YAW], ent->client->spawnFacingAngles[YAW])) >= 45.0f) {
					SetClientViewAngle(ent, ent->client->spawnFacingAngles);
					BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);
				}
			}
			else if (ent->client->pers.lastSpawnTime >= level.time - 500) {
				if (fabs(AngleDelta(ent->client->ps.viewangles[YAW], ent->client->spawnFacingAngles[YAW])) >= 75.0f) {
					SetClientViewAngle(ent, ent->client->spawnFacingAngles);
					BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);
				}
			}
		}

		qboolean didSk = qfalse;
		if (ent->client && ent->client->forceSelfkillTime && level.time >= ent->client->forceSelfkillTime && ent->client->account && ent->client->account->flags & ACCOUNTFLAG_BOOST_AIMBOTBOOST && g_boost.integer && !HasFlag(ent)) {
			ent->client->forceSelfkillTime = 0; // sanity check, should reset anyway
			ent->flags &= ~FL_GODMODE;
			ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
			player_die(ent, ent, ent, 100000, MOD_SUICIDE);
			didSk = qtrue;
		}
		
		if (ent->client && ent->client->account && ent->client->account->flags & ACCOUNTFLAG_BOOST_ITEMPICKUPBOOST && g_boost.integer && !didSk) {
			// boost: pick up items in larger range
			static vec3_t boostPickupRange = { 120, 120, 44 };
			vec3_t mins, maxs;
			VectorSubtract(ent->s.pos.trBase, boostPickupRange, mins);
			VectorAdd(ent->s.pos.trBase, boostPickupRange, maxs);

			int	touchEntityNums[MAX_GENTITIES];
			int num = trap_EntitiesInBox(mins, maxs, touchEntityNums, MAX_GENTITIES);
			gentity_t *pickupEnt = NULL;
			for (int i = 0; i < num; i++) {
				const int touchEntityNum = touchEntityNums[i];
				if (touchEntityNum < MAX_CLIENTS || touchEntityNum == ent - g_entities)
					continue;

				gentity_t *touchEnt = &g_entities[touchEntityNum];
				if (touchEnt->touch != Touch_Item || !touchEnt->item)
					continue;

				pickupEnt = touchEnt;
				break;
			}

			if (pickupEnt) {
				const float myDist = Distance(pickupEnt->s.pos.trBase, ent->client->ps.origin);
				if (pickupEnt->item->giType == IT_TEAM) {
					if (IsDroppedRedFlag(pickupEnt) || IsDroppedBlueFlag(pickupEnt)) {
						// suck up nearby dropped flags if and only if there's nobody else even remotely nearby
						qboolean canSuction = qtrue;
						for (int i = 0; i < MAX_CLIENTS; i++) {
							gentity_t *otherGuy = &g_entities[i];
							if (!otherGuy->inuse || !otherGuy->client || otherGuy->client->pers.connected != CON_CONNECTED || otherGuy->health <= 0 || IsRacerOrSpectator(otherGuy) || otherGuy == ent || otherGuy->client->ps.fallingToDeath)
								continue;

							const float thisGuyDist = Distance(pickupEnt->s.pos.trBase, otherGuy->client->ps.origin);
							if (thisGuyDist < myDist || thisGuyDist <= 700) {
								canSuction = qfalse;
								break;
							}
						}

						if (canSuction) {
							pickupEnt->touch(pickupEnt, ent, NULL);
						}
					}
					else {
						// we don't do anything for non-dropped flags
					}
				}
				else {
					// suck up nearby items if and only if there's nobody else closer to it
					qboolean gotCloserPlayer = qfalse;
					for (int i = 0; i < MAX_CLIENTS; i++) {
						gentity_t *closerGuy = &g_entities[i];
						if (!closerGuy->inuse || !closerGuy->client || closerGuy->client->pers.connected != CON_CONNECTED || closerGuy->health <= 0 || IsRacerOrSpectator(closerGuy) || closerGuy == ent || closerGuy->client->ps.fallingToDeath)
							continue;

						const float thisGuyDist = Distance(pickupEnt->s.pos.trBase, closerGuy->client->ps.origin);
						if (thisGuyDist < myDist) {
							gotCloserPlayer = qtrue;
							break;
						}
					}

					if (!gotCloserPlayer) {
						pickupEnt->touch(pickupEnt, ent, NULL);
					}
				}
			}
		}

		// boost: sk if no force, no guns, and fc is in base in need of th/te
		if (ent->client && ent->client->account && ent->client->account->flags & ACCOUNTFLAG_BOOST_SPAWNFCBOOST &&
			g_boost.integer && g_spawnboost_autosk.integer && !didSk && level.wasRestarted && GetRemindedPosOrDeterminedPos(ent) == CTFPOSITION_BASE) {
			gentity_t *fc = NULL;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *thisGuy = &g_entities[i];
				if (!thisGuy->inuse || !thisGuy->client || thisGuy->client == client || thisGuy->client->sess.sessionTeam != client->sess.sessionTeam ||
					IsRacerOrSpectator(thisGuy) || thisGuy->health <= 0 || !HasFlag(thisGuy) || thisGuy->client->ps.fallingToDeath)
					continue;

				float loc = GetCTFLocationValue(thisGuy);
				if (loc <= 0.4f) {
					fc = thisGuy;
					break;
				}
			}

			qboolean doSk = qfalse;
			if (fc) {
				qboolean fcNeedsTh = qfalse, fcNeedsTe = qfalse;
				qboolean fcNeedsBigTh = qfalse, fcNeedsBigTe = qfalse;
				qboolean hasShittyGun = !HasGoodFireableWeaponEquipped(ent);
				qboolean notEnoughForceToT = ent->client->ps.fd.forcePower < 25;
				qboolean hasMegaShittyForce = ent->client->ps.fd.forcePower <= 15 || ent->client->ps.fd.forcePowersActive & (1 << FP_SPEED);

				int fcHealth = fc->health;
				qboolean gotHealthPack = qfalse, healthPackIsSuperClose = qfalse;
				for (int i = MAX_CLIENTS; i < MAX_GENTITIES; i++) {
					const gentity_t *medpack = &g_entities[i];
					if (!medpack->inuse || !medpack->item || (medpack->item->giType != IT_HEALTH && medpack->item->giType != IT_ARMOR))
						continue;
					if (medpack->s.eFlags & EF_NODRAW)
						continue; // waiting to respawn
					float dist = Distance2D(ent->client->ps.origin, medpack->s.origin);
					if (dist > 800)
						continue;

					gotHealthPack = qtrue;
					if (dist <= 400) {
						healthPackIsSuperClose = qtrue;
						break;
					}
				}

				if (gotHealthPack) {
					if (healthPackIsSuperClose)
						fcHealth += 25; // consider us slightly higher hp if the health pack is very close
					else
						fcHealth += 20;
				}

				if (fcHealth < 50)
					fcNeedsBigTh = fcNeedsTh = qtrue;
				else if (fcHealth <= 75)
					fcNeedsTh = qtrue;

				if (fc->client->ps.fd.forcePower < 50)
					fcNeedsBigTe = fcNeedsTe = qtrue;
				else if (fc->client->ps.fd.forcePower <= 75)
					fcNeedsTe = qtrue;

				if (!(fc->client->ps.fd.forcePowersActive & (1 << FP_SPEED)))
					fcNeedsBigTe = fcNeedsTe = qtrue;

				qboolean fcNeedsBigThOrTe = (fcNeedsBigTh || fcNeedsBigTe);
				qboolean fcNeedsThOrTe = (fcNeedsTh || fcNeedsTe);

				if (notEnoughForceToT && (fcNeedsBigThOrTe || (fcNeedsThOrTe && (hasShittyGun || hasMegaShittyForce))))
					doSk = qtrue;

				if (!doSk && ent->client->account->flags & ACCOUNTFLAG_BOOST_BASEAUTOTHTEBOOST && (ent->client->fakeForceAlignment == FAKEFORCEALIGNMENT_LIGHT || ent->client->fakeForceAlignment == FAKEFORCEALIGNMENT_DARK)) {
					// we don't need to sk by virtue of not having fp,
					// BUT we might possibly need to sk by virtue of being the wrong force alignment.
					if (ent->client->fakeForceAlignment == FAKEFORCEALIGNMENT_LIGHT && fcNeedsTe && !fcNeedsBigTh)
						doSk = qtrue;
					else if (ent->client->fakeForceAlignment == FAKEFORCEALIGNMENT_DARK && fcNeedsBigTh && !fcNeedsTe)
						doSk = qtrue;
				}
			}
			else if (ent->client->account->flags & ACCOUNTFLAG_BOOST_SPAWNFCBOOST && teamgame.redStatus == FLAG_ATBASE && teamgame.blueStatus == FLAG_ATBASE && GetCTFLocationValue(ent) >= 0.66f) {
				doSk = qtrue;
			}

			if (doSk) {
				ent->flags &= ~FL_GODMODE;
				ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
				player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				didSk = qtrue;
			}
		}
	}

	if (ent->client->ps.otherKillerTime > level.time &&
		ent->client->ps.groundEntityNum != ENTITYNUM_NONE &&
		ent->client->ps.otherKillerDebounceTime < level.time)
	{
		ent->client->ps.otherKillerTime = 0;
		ent->client->ps.otherKiller = ENTITYNUM_NONE;
	}
	else if (ent->client->ps.otherKillerTime > level.time &&
		ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
	{
		if (ent->client->ps.otherKillerDebounceTime < (level.time + 100))
		{
			ent->client->ps.otherKillerDebounceTime = level.time + 100;
		}
	}

	//NOTE: can't put USE here *before* PMove!!
	if ( ent->client->ps.useDelay > level.time 
		&& ent->client->ps.m_iVehicleNum )
	{//when in a vehicle, debounce the use...
		ucmd->buttons &= ~BUTTON_USE;
	}

	//FIXME: need to do this before check to avoid walls and cliffs (or just cliffs?)
	G_AddPushVecToUcmd( ent, ucmd );

	//play/stop any looping sounds tied to controlled movement
	G_CheckMovingLoopingSounds( ent, ucmd );

	pm.ps = &client->ps;
	pm.cmd = *ucmd;
	if ( pm.ps->pm_type == PM_DEAD ) {
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
	}
	else if ( ent->r.svFlags & SVF_BOT ) {
		pm.tracemask = MASK_PLAYERSOLID | CONTENTS_MONSTERCLIP;
	}
	else {
		pm.tracemask = MASK_PLAYERSOLID;
	}
	pm.trace = trap_Trace;
	pm.pointcontents = trap_PointContents;
	pm.debugLevel = g_debugMove.integer;
	pm.noFootsteps = ( g_dmflags.integer & DF_NO_FOOTSTEPS ) > 0;

	pm.pmove_fixed = pmove_fixed.integer | client->pers.pmoveFixed;
	pm.pmove_msec = pmove_msec.integer;
	pm.pmove_float = pmove_float.integer;

	pm.animations = bgAllAnims[ent->localAnimIndex].anims;

	//rww - bgghoul2
	pm.ghoul2 = NULL;

#ifdef _DEBUG
	if (g_disableServerG2.integer)
	{

	}
	else
#endif
	if (ent->ghoul2)
	{
		if (ent->localAnimIndex > 1)
		{ //if it isn't humanoid then we will be having none of this.
			pm.ghoul2 = NULL;
		}
		else
		{
			pm.ghoul2 = ent->ghoul2;
			pm.g2Bolts_LFoot = trap_G2API_AddBolt(ent->ghoul2, 0, "*l_leg_foot");
			pm.g2Bolts_RFoot = trap_G2API_AddBolt(ent->ghoul2, 0, "*r_leg_foot");
		}
	}

	//point the saber data to the right place
#if 0
	k = 0;
	while (k < MAX_SABERS)
	{
		if (ent->client->saber[k].model[0])
		{
			pm.saber[k] = &ent->client->saber[k];
		}
		else
		{
			pm.saber[k] = NULL;
		}
		k++;
	}
#endif

	//I'll just do this every frame in case the scale changes in realtime (don't need to update the g2 inst for that)
	VectorCopy(ent->modelScale, pm.modelScale);
	//rww end bgghoul2

	pm.gametype = g_gametype.integer;
	pm.debugMelee = g_debugMelee.integer;
	pm.stepSlideFix = g_stepSlideFix.integer;

	pm.noSpecMove = g_noSpecMove.integer;

	pm.nonHumanoid = (ent->localAnimIndex > 0);

	VectorCopy( client->ps.origin, client->oldOrigin );

	//Set up bg entity data
	pm.baseEnt = (bgEntity_t *)g_entities;
	pm.entSize = sizeof(gentity_t);

	if (ent->client->ps.saberLockTime > level.time)
	{
		gentity_t *blockOpp = &g_entities[ent->client->ps.saberLockEnemy];

		if (blockOpp && blockOpp->inuse && blockOpp->client)
		{
			vec3_t lockDir, lockAng;

			VectorSubtract( blockOpp->r.currentOrigin, ent->r.currentOrigin, lockDir );
			vectoangles(lockDir, lockAng);
			SetClientViewAngle( ent, lockAng );
		}

		if ( ent->client->ps.saberLockHitCheckTime < level.time )
		{//have moved to next frame since last lock push
			ent->client->ps.saberLockHitCheckTime = level.time;//so we don't push more than once per server frame
			if ( ( ent->client->buttons & BUTTON_ATTACK ) && ! ( ent->client->oldbuttons & BUTTON_ATTACK ) )
			{
				if ( ent->client->ps.saberLockHitIncrementTime < level.time )
				{//have moved to next frame since last saberlock attack button press
					int lockHits = 0;
					ent->client->ps.saberLockHitIncrementTime = level.time;//so we don't register an attack key press more than once per server frame
					//NOTE: FP_SABER_OFFENSE level already taken into account in PM_SaberLocked
					if ( (ent->client->ps.fd.forcePowersActive&(1<<FP_RAGE)) )
					{//raging: push harder
						lockHits = 1+ent->client->ps.fd.forcePowerLevel[FP_RAGE];
					}
					else
					{//normal attack
						switch ( ent->client->ps.fd.saberAnimLevel )
						{
						case SS_FAST:
							lockHits = 1;
							break;
						case SS_MEDIUM:
						case SS_TAVION:
						case SS_DUAL:
						case SS_STAFF:
							lockHits = 2;
							break;
						case SS_STRONG:
						case SS_DESANN:
							lockHits = 3;
							break;
						}
					}
					if ( ent->client->ps.fd.forceRageRecoveryTime > level.time 
						&& Q_irand( 0, 1 ) )
					{//finished raging: weak
						lockHits -= 1;
					}
					lockHits += ent->client->saber[0].lockBonus;
					if ( ent->client->saber[1].model
						&& ent->client->saber[1].model[0]
						&& !ent->client->ps.saberHolstered )
					{
						lockHits += ent->client->saber[1].lockBonus;
					}
					ent->client->ps.saberLockHits += lockHits;
					if ( g_saberLockRandomNess.integer )
					{
						ent->client->ps.saberLockHits += Q_irand( 0, g_saberLockRandomNess.integer );
						if ( ent->client->ps.saberLockHits < 0 )
						{
							ent->client->ps.saberLockHits = 0;
						}
					}
				}
			}
			if ( ent->client->ps.saberLockHits > 0 )
			{
				if ( !ent->client->ps.saberLockAdvance )
				{
					ent->client->ps.saberLockHits--;
				}
				ent->client->ps.saberLockAdvance = qtrue;
			}
		}
	}
	else
	{
		ent->client->ps.saberLockFrame = 0;
		//check for taunt
		if ( (pm.cmd.generic_cmd == GENCMD_ENGAGE_DUEL) && (g_gametype.integer == GT_DUEL || g_gametype.integer == GT_POWERDUEL) )
		{//already in a duel, make it a taunt command
			pm.cmd.buttons |= BUTTON_GESTURE;
		}
	}

	if (ent->s.number >= MAX_CLIENTS)
	{
		VectorCopy(ent->r.mins, pm.mins);
		VectorCopy(ent->r.maxs, pm.maxs);
#if 1
		if (ent->s.NPC_class == CLASS_VEHICLE &&
			ent->m_pVehicle )
		{
			if ( ent->m_pVehicle->m_pPilot)
			{ //vehicles want to use their last pilot ucmd I guess
				if ((level.time - ent->m_pVehicle->m_ucmd.serverTime) > 2000)
				{ //Previous owner disconnected, maybe
					ent->m_pVehicle->m_ucmd.serverTime = level.time;
					ent->client->ps.commandTime = level.time-100;
					msec = 100;
				}

				memcpy(&pm.cmd, &ent->m_pVehicle->m_ucmd, sizeof(usercmd_t));
				
				//no veh can strafe
				pm.cmd.rightmove = 0;
				//no crouching or jumping!
				pm.cmd.upmove = 0;

				//NOTE: button presses were getting lost!
				assert(g_entities[ent->m_pVehicle->m_pPilot->s.number].client);
				pm.cmd.buttons = (g_entities[ent->m_pVehicle->m_pPilot->s.number].client->pers.cmd.buttons&(BUTTON_ATTACK|BUTTON_ALT_ATTACK));
			}
			if ( ent->m_pVehicle->m_pVehicleInfo->type == VH_WALKER )
			{
				if ( ent->client->ps.groundEntityNum != ENTITYNUM_NONE )
				{//ATST crushes anything underneath it
					gentity_t	*under = &g_entities[ent->client->ps.groundEntityNum];

					if ( under && under->health && under->takedamage )
					{
						vec3_t	down = {0,0,-1};
						gentity_t	*attacker = ent;

						if ( ent->m_pVehicle->m_pPilot )
						{
							attacker = &g_entities[ent->m_pVehicle->m_pPilot->s.number];
						}

						//FIXME: we'll be doing traces down from each foot, so we'll have a real impact origin
						G_Damage( under, attacker, attacker, down, under->r.currentOrigin, 100, 0, MOD_CRUSH );
					}
				}
			}
		}
#endif
	}

	Pmove (&pm);

	if ( pm.cmd.forwardmove ) {
		ent->client->usedForwardOrBackward = qtrue;
	}

	if ( pm.cmd.upmove ) {
		ent->client->jumpedOrCrouched = qtrue;
	}

	if (g_strafejump_mod.integer){
		ent->r.contents &= ~CONTENTS_BODY;
	}

	if (ent->client->solidHack)
	{
		if (ent->client->solidHack > level.time)
		{ //whee!
			ent->r.contents = 0;
		}
		else
		{
			ent->r.contents = CONTENTS_BODY;
			ent->client->solidHack = 0;
		}
	}
	
	if ( ent->NPC )
	{
		VectorCopy( ent->client->ps.viewangles, ent->r.currentAngles );
	}

	if (pm.checkDuelLoss)
	{
		if (pm.checkDuelLoss > 0 && (pm.checkDuelLoss <= MAX_CLIENTS || (pm.checkDuelLoss < (MAX_GENTITIES-1) && g_entities[pm.checkDuelLoss-1].s.eType == ET_NPC) ) )
		{
			gentity_t *clientLost = &g_entities[pm.checkDuelLoss-1];

			if (clientLost && clientLost->inuse && clientLost->client && Q_irand(0, 40) > clientLost->health)
			{
				vec3_t attDir;
				VectorSubtract(ent->client->ps.origin, clientLost->client->ps.origin, attDir);
				VectorNormalize(attDir);

				VectorClear(clientLost->client->ps.velocity);
				clientLost->client->ps.forceHandExtend = HANDEXTEND_NONE;
				clientLost->client->ps.forceHandExtendTime = 0;

				gGAvoidDismember = 1;
				G_Damage(clientLost, ent, ent, attDir, clientLost->client->ps.origin, 9999, DAMAGE_NO_PROTECTION, MOD_SABER);

				if (clientLost->health < 1)
				{
					gGAvoidDismember = 2;
					G_CheckForDismemberment(clientLost, ent, clientLost->client->ps.origin, 999, (clientLost->client->ps.legsAnim), qfalse);
				}

				gGAvoidDismember = 0;
			}
			else if (clientLost && clientLost->inuse && clientLost->client &&
				clientLost->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN && clientLost->client->ps.saberEntityNum)
			{ //if we didn't knock down it was a circle lock. So as punishment, make them lose their saber and go into a proper anim
				saberCheckKnockdown_DuelLoss(&g_entities[clientLost->client->ps.saberEntityNum], clientLost, ent);
			}
		}

		pm.checkDuelLoss = 0;
	}

	//if (pm.cmd.generic_cmd && (pm.cmd.generic_cmd != ent->client->lastGenCmd || ent->client->lastGenCmdTime < level.time))
	if (pm.cmd.generic_cmd)
	{
		//ent->client->lastGenCmd = pm.cmd.generic_cmd;
		//if (pm.cmd.generic_cmd != GENCMD_FORCE_THROW && pm.cmd.generic_cmd != GENCMD_FORCE_PULL)
		//{ //these are the only two where you wouldn't care about a delay between
		//	ent->client->lastGenCmdTime = level.time + 300; //default 100ms debounce between issuing the same command.
		//}

		switch (pm.cmd.generic_cmd)
		{
		case 0:
			break;
		case GENCMD_SABERSWITCH:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_SABER] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_SABER] = level.time;
			Cmd_ToggleSaber_f(ent);
			break;
		case GENCMD_ENGAGE_DUEL:
			if (ent->client->sess.inRacemode) {
				if (ent->client->genCmdDebounce[GENCMD_DELAY_DUEL] > level.time - 100)
					break;
				ent->client->genCmdDebounce[GENCMD_DELAY_DUEL] = level.time;
				Cmd_Amtele_f(ent);
			}
			else {
				if (ent->client->genCmdDebounce[GENCMD_DELAY_DUEL] > level.time - 300)
					break;
				ent->client->genCmdDebounce[GENCMD_DELAY_DUEL] = level.time;
				Cmd_EngageDuel_f(ent);
			}
			break;
		case GENCMD_FORCE_HEAL:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_HEAL] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_HEAL] = level.time;
			ForceHeal(ent);
			break;
		case GENCMD_FORCE_SPEED:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_SPEED] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_SPEED] = level.time;
			ForceSpeed(ent, 0);
			break;
		case GENCMD_FORCE_THROW:
			ForceThrow(ent, qfalse);
			break;
		case GENCMD_FORCE_PULL:
			ForceThrow(ent, qtrue);
			break;
		case GENCMD_FORCE_DISTRACT:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_TRICK] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_TRICK] = level.time;
			ForceTelepathy(ent);
			break;
		case GENCMD_FORCE_RAGE:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_RAGE] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_RAGE] = level.time;
			ForceRage(ent);
			break;
		case GENCMD_FORCE_PROTECT:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_PROTECT] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_PROTECT] = level.time;
			ForceProtect(ent);
			break;
		case GENCMD_FORCE_ABSORB:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_ABSORB] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_ABSORB] = level.time;
			ForceAbsorb(ent);
			break;
		case GENCMD_FORCE_HEALOTHER:
			// allow th/te binds to work for either one
			if (g_redirectWrongThTeBinds.integer && ent && ent->client && !(ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_HEAL)) && ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_FORCE))
				ForceTeamForceReplenish(ent, qfalse);
			else
				ForceTeamHeal(ent, qfalse);
			break;
		case GENCMD_FORCE_FORCEPOWEROTHER:
			// allow th/te binds to work for either one
			if (g_redirectWrongThTeBinds.integer && ent && ent->client && !(ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_FORCE)) && ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_HEAL))
				ForceTeamHeal(ent, qfalse);
			else
				ForceTeamForceReplenish(ent, qfalse);
			break;
		case GENCMD_FORCE_SEEING:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_SEEING] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_SEEING] = level.time;
			// let racers toggle seeing in game players with force seeing
			if (ent->client->sess.inRacemode) {
				if (ent->client->sess.racemodeFlags & RMF_HIDEINGAME) {
					trap_SendServerCommand(ent - g_entities, "print \""S_COLOR_GREEN"In game entities VISIBLE\n\"");
					ent->client->sess.racemodeFlags &= ~RMF_HIDEINGAME;
					
				}
				else {
					trap_SendServerCommand(ent - g_entities, "print \""S_COLOR_RED"In game entities HIDDEN\n\"");
					ent->client->sess.racemodeFlags |= RMF_HIDEINGAME;
					
				}
			}
			else {
				ForceSeeing(ent);
			}
			break;
		case GENCMD_USE_SEEKER:
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SEEKER)) &&
				G_ItemUsable(&ent->client->ps, HI_SEEKER))
			{
				ItemUse_Seeker(ent);
				G_AddEvent(ent, EV_USE_ITEM0 + HI_SEEKER, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER);
			}
			break;
		case GENCMD_USE_FIELD:
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SHIELD)) &&
				G_ItemUsable(&ent->client->ps, HI_SHIELD))
			{
				ItemUse_Shield(ent);
				G_AddEvent(ent, EV_USE_ITEM0 + HI_SHIELD, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SHIELD);
			}
			break;
		case GENCMD_USE_BACTA:
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC)) &&
				G_ItemUsable(&ent->client->ps, HI_MEDPAC))
			{
				ItemUse_MedPack(ent);
				G_AddEvent(ent, EV_USE_ITEM0 + HI_MEDPAC, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC);
			}
			break;
		case GENCMD_USE_BACTABIG:
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG)) &&
				G_ItemUsable(&ent->client->ps, HI_MEDPAC_BIG))
			{
				ItemUse_MedPack_Big(ent);
				G_AddEvent(ent, EV_USE_ITEM0 + HI_MEDPAC_BIG, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC_BIG);
			}
			break;
		case GENCMD_USE_ELECTROBINOCULARS:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_BINOCS] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_BINOCS] = level.time;
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) &&
				G_ItemUsable(&ent->client->ps, HI_BINOCULARS))
			{
				ItemUse_Binoculars(ent);
				if (ent->client->ps.zoomMode == 0)
				{
					G_AddEvent(ent, EV_USE_ITEM0 + HI_BINOCULARS, 1);
				}
				else
				{
					G_AddEvent(ent, EV_USE_ITEM0 + HI_BINOCULARS, 2);
				}
			}
			break;
		case GENCMD_ZOOM:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_ZOOM] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_ZOOM] = level.time;
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) &&
				G_ItemUsable(&ent->client->ps, HI_BINOCULARS))
			{
				ItemUse_Binoculars(ent);
				if (ent->client->ps.zoomMode == 0)
				{
					G_AddEvent(ent, EV_USE_ITEM0 + HI_BINOCULARS, 1);
				}
				else
				{
					G_AddEvent(ent, EV_USE_ITEM0 + HI_BINOCULARS, 2);
				}
			}
			break;
		case GENCMD_USE_SENTRY:
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SENTRY_GUN)) &&
				G_ItemUsable(&ent->client->ps, HI_SENTRY_GUN))
			{
				ItemUse_Sentry(ent);
				G_AddEvent(ent, EV_USE_ITEM0 + HI_SENTRY_GUN, 0);
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SENTRY_GUN);
			}
			break;
		case GENCMD_USE_JETPACK:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_JETPACK] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_JETPACK] = level.time;
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)) &&
				G_ItemUsable(&ent->client->ps, HI_JETPACK))
			{
				ItemUse_Jetpack(ent);
				G_AddEvent(ent, EV_USE_ITEM0 + HI_JETPACK, 0);
			}
			break;
		case GENCMD_USE_HEALTHDISP:
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_HEALTHDISP)) &&
				G_ItemUsable(&ent->client->ps, HI_HEALTHDISP))
			{
				//ItemUse_UseDisp(ent, HI_HEALTHDISP);
				G_AddEvent(ent, EV_USE_ITEM0 + HI_HEALTHDISP, 0);
			}
			break;
		case GENCMD_USE_AMMODISP:
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_AMMODISP)) &&
				G_ItemUsable(&ent->client->ps, HI_AMMODISP))
			{
				//ItemUse_UseDisp(ent, HI_AMMODISP);
				G_AddEvent(ent, EV_USE_ITEM0 + HI_AMMODISP, 0);
			}
			break;
		case GENCMD_USE_EWEB:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_EWEB] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_EWEB] = level.time;
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_EWEB)) &&
				G_ItemUsable(&ent->client->ps, HI_EWEB))
			{
				ItemUse_UseEWeb(ent);
				G_AddEvent(ent, EV_USE_ITEM0 + HI_EWEB, 0);
			}
			break;
		case GENCMD_USE_CLOAK:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_CLOAK] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_CLOAK] = level.time;
			if ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)) &&
				G_ItemUsable(&ent->client->ps, HI_CLOAK))
			{
				if (ent->client->ps.powerups[PW_CLOAKED])
				{//decloak
					Jedi_Decloak(ent);
				}
				else
				{//cloak
					Jedi_Cloak(ent);
				}
			}
			break;
		case GENCMD_SABERATTACKCYCLE:
		{
			if (ent->client->genCmdDebounce[GENCMD_DELAY_SABERSWITCH] > level.time - 100)
				break;
		}
		ent->client->genCmdDebounce[GENCMD_DELAY_SABERSWITCH] = level.time;
		Cmd_SaberAttackCycle_f(ent);
		break;
		case GENCMD_TAUNT:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_TAUNT] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_TAUNT] = level.time;
			G_SetTauntAnim(ent, TAUNT_TAUNT);
			break;
		case GENCMD_BOW:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_EMOTE] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_EMOTE] = level.time;
			G_SetTauntAnim(ent, TAUNT_BOW);
			break;
		case GENCMD_MEDITATE:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_EMOTE] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_EMOTE] = level.time;
			G_SetTauntAnim(ent, TAUNT_MEDITATE);
			break;
		case GENCMD_FLOURISH:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_TAUNT] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_TAUNT] = level.time;
			G_SetTauntAnim(ent, TAUNT_FLOURISH);
			break;
		case GENCMD_GLOAT:
			if (ent->client->genCmdDebounce[GENCMD_DELAY_TAUNT] > level.time - 300)
				break;
			ent->client->genCmdDebounce[GENCMD_DELAY_TAUNT] = level.time;
			G_SetTauntAnim(ent, TAUNT_GLOAT);
			break;
		default:
			break;
		}
	}

	// save results of pmove
	if ( ent->client->ps.eventSequence != oldEventSequence ) {
		ent->eventTime = level.time;
	}
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, ent->client->ps.commandTime, qfalse );
		//rww - 12-03-02 - Don't snap the origin of players! It screws prediction all up.
	}
	else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qfalse );
	}

	if (isNPC)
	{
		ent->s.eType = ET_NPC;
	}

	SendPendingPredictableEvents( &ent->client->ps );

	if ( !( ent->client->ps.eFlags & EF_FIRING ) ) {
		client->fireHeld = qfalse;		// for grapple
	}

	// use the snapped origin for linking so it matches client predicted versions
	VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );

	if (ent->s.eType != ET_NPC ||
		ent->s.NPC_class != CLASS_VEHICLE ||
		!ent->m_pVehicle ||
		!ent->m_pVehicle->m_iRemovedSurfaces)
	{ //let vehicles that are getting broken apart do their own crazy sizing stuff
		VectorCopy (pm.mins, ent->r.mins);
		VectorCopy (pm.maxs, ent->r.maxs);
	}

	ent->waterlevel = pm.waterlevel;
	ent->watertype = pm.watertype;

	// execute client events
    //OSP: pause
    if ( level.pause.state == PAUSE_NONE || ent->client->sess.inRacemode )
            ClientEvents( ent, oldEventSequence );

	if ( pm.useEvent )
	{
		//TODO: Use
	}
	if ((ent->client->pers.cmd.buttons & BUTTON_USE) && ent->client->ps.useDelay < level.time)
	{
		TryUse(ent);
		ent->client->ps.useDelay = level.time + 100;
	}

	// link entity now, after any personal teleporters have been used
	trap_LinkEntity (ent);

	if ( !ent->client->noclip ) {
		G_TouchTriggers( ent );
	}

	// NOTE: now copy the exact origin over otherwise clients can be snapped into solid
	VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );

	//test for solid areas in the AAS file

	// touch other objects
	ClientImpacts( ent, &pm );

	// save results of triggers and client events
	if (ent->client->ps.eventSequence != oldEventSequence) {
		ent->eventTime = level.time;
	}

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

	// Did we kick someone in our pmove sequence?
	if (client->ps.forceKickFlip)
	{
		gentity_t *faceKicked = &g_entities[client->ps.forceKickFlip-1];

		if (faceKicked && faceKicked->client && (!OnSameTeam(ent, faceKicked) || g_friendlyFire.integer) &&
			(!faceKicked->client->ps.duelInProgress || faceKicked->client->ps.duelIndex == ent->s.number) &&
			(!ent->client->ps.duelInProgress || ent->client->ps.duelIndex == faceKicked->s.number))
		{
			if ( faceKicked && faceKicked->client && faceKicked->health && faceKicked->takedamage )
			{//push them away and do pain
				vec3_t oppDir;
				int strength = (int)VectorNormalize2( client->ps.velocity, oppDir );

				strength *= 0.05;

				VectorScale( oppDir, -1, oppDir );

				G_Damage( faceKicked, ent, ent, oppDir, client->ps.origin, strength, DAMAGE_NO_ARMOR, MOD_MELEE );

				if ( faceKicked->client->ps.weapon != WP_SABER ||
					 faceKicked->client->ps.fd.saberAnimLevel != FORCE_LEVEL_3 ||
					 (!BG_SaberInAttack(faceKicked->client->ps.saberMove) && !PM_SaberInStart(faceKicked->client->ps.saberMove) && !PM_SaberInReturn(faceKicked->client->ps.saberMove) && !PM_SaberInTransition(faceKicked->client->ps.saberMove)) )
				{
					if (faceKicked->health > 0 &&
						faceKicked->client->ps.stats[STAT_HEALTH] > 0 &&
						faceKicked->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN)
					{
						if (BG_KnockDownable(&faceKicked->client->ps) && Q_irand(1, 10) <= 3)
						{ //only actually knock over sometimes, but always do velocity hit
							faceKicked->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
							faceKicked->client->ps.forceHandExtendTime = level.time + 1100;
							faceKicked->client->ps.forceDodgeAnim = 0; //this toggles between 1 and 0, when it's 1 we should play the get up anim
						}

						faceKicked->client->ps.otherKiller = ent->s.number;
						faceKicked->client->ps.otherKillerTime = level.time + 5000;
						faceKicked->client->ps.otherKillerDebounceTime = level.time + 100;

						faceKicked->client->ps.velocity[0] = oppDir[0]*(strength*40);
						faceKicked->client->ps.velocity[1] = oppDir[1]*(strength*40);
						faceKicked->client->ps.velocity[2] = 200;
					}
				}

				G_Sound( faceKicked, CHAN_AUTO, G_SoundIndex( va("sound/weapons/melee/punch%d", Q_irand(1, 4)) ) );
			}
		}

		client->ps.forceKickFlip = 0;
	}

	// check for respawning
	if ( client->ps.stats[STAT_HEALTH] <= 0 
		&& !(client->ps.eFlags2&EF2_HELD_BY_MONSTER)//can't respawn while being eaten
		&& ent->s.eType != ET_NPC ) {
		// wait for the attack button to be pressed
		if ( level.time > client->respawnTime && !gDoSlowMoDuel ) {
			// forcerespawn is to prevent users from waiting out powerups
			int forceRes = g_forcerespawn.integer;

			if (g_gametype.integer == GT_POWERDUEL)
			{
				forceRes = 1;
			}
			else if (g_gametype.integer == GT_SIEGE &&
				g_siegeRespawn.integer)
			{ //wave respawning on
				forceRes = 1;
			}

			if ( forceRes > 0 && 
				( level.time - client->respawnTime ) > forceRes * 1000 ) {
				respawn( ent );
				return;
			}
		
			qboolean meme = (!level.wasRestarted && client->ps.otherKiller <= MAX_CLIENTS &&
				g_entities[client->ps.otherKiller].inuse && g_entities[client->ps.otherKiller].client && g_entities[client->ps.otherKiller].client->account && (!Q_stricmp(g_entities[client->ps.otherKiller].client->account->name, "duo") || !Q_stricmp(g_entities[client->ps.otherKiller].client->account->name, "alpha")));

			// pressing attack or use is the normal respawn method
			if ( ucmd->buttons & ( BUTTON_ATTACK | BUTTON_USE_HOLDABLE ) ) {
				respawn( ent );
			}
			else if (client->account && (client->account->flags & ACCOUNTFLAG_BOOST_SPAWNCLICKBOOST) && g_boost.integer) {
				// boost: force respawn after being dead for the required amount of time
				respawn(ent);
			}
			else if (client->sess.autoRespawn) {
				respawn(ent);
			}
			else if ((ent->r.svFlags & SVF_BOT) && g_braindeadBots.integer) {
				respawn(ent); // fix bots never respawning
			}
			else if (meme) {
				respawn(ent);
			}
		}
		else if (gDoSlowMoDuel)
		{
			client->respawnTime = level.time + 1000;
		}
		return;
	}

	// perform once-a-second actions
    //OSP: pause
    if ( level.pause.state == PAUSE_NONE || ent->client->sess.inRacemode )
            ClientTimerActions( ent, msec );

	G_UpdateClientBroadcasts ( ent );

	//try some idle anims on ent if getting no input and not moving for some time
	G_CheckClientIdle( ent, ucmd );

	// This code was moved here from clientThink to fix a problem with g_synchronousClients 
	// being set to 1 when in vehicles. 
	if ( ent->s.number < MAX_CLIENTS && ent->client->ps.m_iVehicleNum )
	{//driving a vehicle
		//run it
		if (g_entities[ent->client->ps.m_iVehicleNum].inuse && g_entities[ent->client->ps.m_iVehicleNum].client)
		{
			ClientThink(ent->client->ps.m_iVehicleNum, &g_entities[ent->client->ps.m_iVehicleNum].m_pVehicle->m_ucmd);
		}
		else
		{ //vehicle no longer valid?
			ent->client->ps.m_iVehicleNum = 0;
		}
	}
	StoreRecordingTrail(ent);
	if (ent - g_entities < MAX_CLIENTS)
		G_StoreTrail(ent);
}

/*
==================
G_CheckClientTimeouts

Checks whether a client has exceded any timeouts and act accordingly
==================
*/
void G_CheckClientTimeouts ( gentity_t *ent )
{
	// Only timeout supported right now is the timeout to spectator mode
	if ( !g_timeouttospec.integer )
	{
		return;
	}

	// Already a spectator, no need to boot them to spectator
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		return;
	}

	// See how long its been since a command was received by the client and if its 
	// longer than the timeout to spectator then force this client into spectator mode
	if ( level.time - ent->client->pers.cmd.serverTime > g_timeouttospec.integer * 1000 )
	{
		SetTeam ( ent, "spectator" );
	}
}

/*
==================
ClientThink

A new command has arrived from the client
==================
*/
void ClientThink( int clientNum,usercmd_t *ucmd ) {
	gentity_t *ent;

	ent = g_entities + clientNum;
	if (clientNum < MAX_CLIENTS)
	{
		trap_GetUsercmd( clientNum, &ent->client->pers.cmd );
	}

	// mark the time we got info, so we can display the
	// phone jack if they don't get any for a while
	ent->client->lastCmdTime = level.time;
	ent->client->lastRealCmdTime = level.time;

	if (ucmd)
	{
		ent->client->pers.cmd = *ucmd;
	}

	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		ClientThink_real( ent );
	}
	// vehicles are clients and when running synchronous they still need to think here
	// so special case them.
	else if ( clientNum >= MAX_CLIENTS ) {
		ClientThink_real( ent );
	}

#ifdef DEBUG_CTF_POSITION_STATS
	ent->client->lastInputTime = trap_Milliseconds();
#else
	if (IsInputting(ent->client, qtrue, qtrue, qfalse))
		ent->client->lastInputTime = trap_Milliseconds();
#endif
}


void G_RunClient( gentity_t *ent ) {
	// force a client think if it's been too long since their last real one
	if (!(ent->r.svFlags & SVF_BOT) && g_forceClientUpdateRate.integer && ent->client->lastCmdTime < level.time - g_forceClientUpdateRate.integer &&
		ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
		trap_GetUsercmd(ent - g_entities, &ent->client->pers.cmd);
		ent->client->lastCmdTime = level.time;
		ent->client->pers.cmd.serverTime = level.time;
		ent->client->pers.cmd.forwardmove = ent->client->pers.cmd.rightmove = ent->client->pers.cmd.upmove = 0;
		//ent->client->pers.cmd.buttons = 0;

		ClientThink_real(ent);
		return;
	}

	if ( !(ent->r.svFlags & SVF_BOT) && !g_synchronousClients.integer ) {
		return;
	}
	ent->client->pers.cmd.serverTime = level.time;
	ClientThink_real( ent );
}


/*
==================
SpectatorClientEndFrame

==================
*/
void SpectatorClientEndFrame( gentity_t *ent ) {
	gclient_t	*cl;

	if (ent->s.eType == ET_NPC)
	{
		assert(0);
		return;
	}

	// if we are doing a chase cam or a remote view, grab the latest info
	if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		int		clientNum;

		clientNum = ent->client->sess.spectatorClient;

		// team follow1 and team follow2 go to whatever clients are playing
		if ( clientNum == -1 ) {
			clientNum = level.follow1;
		} else if ( clientNum == -2 ) {
			clientNum = level.follow2;
		}
		if ( clientNum >= 0 ) {
			cl = &level.clients[ clientNum ];
			if ( cl->pers.connected == CON_CONNECTED && 
				cl->sess.sessionTeam != TEAM_SPECTATOR ) {
				int originalPing = ent->client->ps.ping;

				ent->client->ps.eFlags = cl->ps.eFlags;
				ent->client->ps = cl->ps;

				if (clientNum != ent->client->sess.lastSpectatorClient || ent->client->sess.spectatorState != ent->client->sess.lastSpectatorState)
					SendForceTimers(&g_entities[clientNum], ent);

				ent->client->sess.lastSpectatorClient = clientNum;
				ent->client->sess.lastSpectatorState = ent->client->sess.spectatorState;

				if (ent->client->sess.isInkognito 
					|| originalPing == -1
					|| originalPing == 999){
					ent->client->ps.ping = originalPing;
					ent->client->ps.persistant[PERS_SCORE] = 0;
				}

				ent->client->ps.pm_flags |= PMF_FOLLOW;
				return;
			} else {
				// drop them to free spectators unless they are dedicated camera followers
				if ( ent->client->sess.spectatorClient >= 0 ) {
					ent->client->sess.spectatorState = SPECTATOR_FREE;
					ClientBegin( ent->client - level.clients, qtrue );
				}
			}
		}
	}
    else
    {
        if (level.pause.state != PAUSE_NONE) {
            int time_delta = level.time - level.previousTime;
			ent->client->pers.enterTime += time_delta;
            ent->client->sess.inactivityTime += (time_delta / 1000);
        }
    }

	if ( ent->client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
		ent->client->ps.pm_flags |= PMF_SCOREBOARD;
	} else {
		ent->client->ps.pm_flags &= ~PMF_SCOREBOARD;
	}

	ent->client->sess.lastSpectatorState = ent->client->sess.spectatorState;
}

qboolean PauseConditions(void) {
#ifdef DEBUG_PAUSE
	return qtrue;
#else
	if (!level.numTeamTicks || level.intermissionQueued || level.intermissiontime)
		return qfalse;

	float avgRed = (float)level.numRedPlayerTicks / (float)level.numTeamTicks;
	float avgBlue = (float)level.numBluePlayerTicks / (float)level.numTeamTicks;

	int avgRedInt = (int)lroundf(avgRed);
	int avgBlueInt = (int)lroundf(avgBlue);

	float duration = level.time - level.startTime;

	// only allow auto-pause if:
	// * the level was map_restarted
	// * nobody was AFK at start
	// * at least 30 seconds have elapsed
	// * the average rounded integer number of players in each team is equal
	// * the sum of these average integers is >= 4 (at least 2s)
	// * both averages are within +/- 0.2 of their rounded values
	if (level.wasRestarted &&
		!level.someoneWasAFK &&
		duration >= 30000 &&
		avgRedInt == avgBlueInt &&
		avgRedInt + avgBlueInt >= 4 &&
		fabs(avgRed - round(avgRed)) < 0.2f &&
		fabs(avgBlue - round(avgBlue)) < 0.2f) {
		return qtrue;
	}

	return qfalse;
#endif
}


extern void G_Say(gentity_t *ent, gentity_t *target, int mode, const char *chatText, qboolean force);
extern qboolean IsRacerOrSpectator(gentity_t *ent);

void DoPauseStartChecks(void) {
	// note angles of vehicles so that people can't unfairly spin around during pause
	for (int i = 0; i < MAX_GENTITIES; i++) {
		gentity_t *ent = &g_entities[i];
		if (i < MAX_CLIENTS) {
			if (ent->inuse && ent->client && !IsRacerOrSpectator(ent)) {
				ent->lockPauseAngles = qtrue;
				VectorCopy(ent->s.angles, ent->pauseAngles);
				VectorCopy(ent->client->ps.viewangles, ent->pauseViewAngles);
				for (int j = 0; j < MAX_CLIENTS; j++) {
					gentity_t *otherEnt = &g_entities[j];
					if (i == j || IsRacerOrSpectator(otherEnt))
						continue;
					ent->hasLOS[j] = HaveLOSToTarget(ent, otherEnt);
				}
			}
		}
		else {
			if (ent->inuse && ent->m_pVehicle && ent->m_pVehicle->m_pPilot && ((gentity_t *)ent->m_pVehicle->m_pPilot)->inuse && ((gentity_t *)ent->m_pVehicle->m_pPilot)->client &&
				!IsRacerOrSpectator(((gentity_t *)ent->m_pVehicle->m_pPilot))) {
				ent->lockPauseAngles = qtrue;
				VectorCopy(ent->s.angles, ent->pauseAngles);
				if (ent->client)
					VectorCopy(ent->client->ps.viewangles, ent->pauseViewAngles);
			}
		}
	}

	// note clients who are on lifts
	for (int i = 0; i < MAX_CLIENTS; i++)
		level.clients[i].pers.onLiftDuringPause = NULL;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];

		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED)
			continue;
		if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE || ent->client->ps.groundEntityNum == ENTITYNUM_WORLD)
			continue;
		if (IsRacerOrSpectator(ent))
			continue;
		if (ent->client->ps.pm_type == PM_SPECTATOR)
			continue;

		gentity_t *groundEnt = &g_entities[ent->client->ps.groundEntityNum];

		if (groundEnt->client)
			continue;
		if (!groundEnt->blocked)
			continue;

		ent->client->pers.onLiftDuringPause = groundEnt;
	}
}

/*
==============
ClientEndFrame

Called at the end of each server frame for each connected client
A fast client will have multiple ClientThink for each ClientEdFrame,
while a slow client may have multiple ClientEndFrame between ClientThink.
==============
*/
void ClientEndFrame( gentity_t *ent ) {
	int			i;
	qboolean isNPC = qfalse;

	if (ent->s.eType == ET_NPC)
	{
		isNPC = qtrue;
	}

	if (ent->client->account && ent->client->account->flags & ACCOUNTFLAG_ENTERSPAMMER) {
		int now = trap_Milliseconds();
		if (ent->client->ps.eFlags & EF_TALK) {
			ent->client->pers.chatBufferCheckTime = now + 2000;
		}
		else {
			if (ent->client->pers.chatBuffer[0] && ent->client->pers.chatBufferCheckTime && now >= ent->client->pers.chatBufferCheckTime) {
				G_Say(ent, NULL, SAY_ALL, ent->client->pers.chatBuffer, qtrue);
				ent->client->pers.chatBuffer[0] = '\0';
				ent->client->pers.chatBufferCheckTime = 0;
			}
			if (IsRacerOrSpectator(ent) && ent->client->pers.specChatBuffer[0] && ent->client->pers.specChatBufferCheckTime && now >= ent->client->pers.specChatBufferCheckTime) {
				G_Say(ent, NULL, SAY_TEAM, ent->client->pers.specChatBuffer, qtrue);
				ent->client->pers.specChatBuffer[0] = '\0';
				ent->client->pers.chatBufferCheckTime = 0;
			}
		}
	}

#ifdef NEWMOD_SUPPORT
	// add the EF_CONNECTION flag for non-specs if we haven't gotten commands recently
	if (level.time - ent->client->lastRealCmdTime > 1000) {
		ent->client->isLagging = qtrue;

		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR && !ent->client->sess.inRacemode) {
			ent->client->ps.eFlags |= EF_CONNECTION;

			// auto-pause if someone goes 999 for a few seconds during a live pug
			if (g_gametype.integer == GT_CTF &&
				level.pause.state != PAUSE_PAUSED &&
				g_autoPause999.integer &&
				level.time - ent->client->lastRealCmdTime >= (Com_Clampi(1, 10, g_autoPause999.integer) * 1000) &&
				PauseConditions()) {
				level.pause.state = PAUSE_PAUSED;
				level.pause.time = level.time + 120000; // pause for 2 minutes
				DoPauseStartChecks();
				Q_strncpyz(level.pause.reason, va("%s^7 is 999\n", ent->client->pers.netname), sizeof(level.pause.reason));
				Com_Printf("Auto-pausing game: %s\n", level.pause.reason);
			}
		}
	}
	else {
		if (ent->client->ps.eFlags & EF_CONNECTION || ent->client->isLagging) { // he was lagging (or vid_restarted) but isn't anymore; send this again just to be sure
			// try not to send these post-lag messages too rapidly
			static int lastTimePostLagMessagesSent[MAX_CLIENTS] = { 0 };
			int now = trap_Milliseconds();
			if (!lastTimePostLagMessagesSent[ent - g_entities] || now - lastTimePostLagMessagesSent[ent - g_entities] >= 3000) {
				G_BroadcastServerFeatureList(ent - g_entities);
				if (ent->client->sess.isInkognito)
					trap_SendServerCommand(ent - g_entities, "kls -1 -1 \"inko\" \"1\""); // only update if enabled
				lastTimePostLagMessagesSent[ent - g_entities] = now;
			}
		}
		ent->client->isLagging = qfalse;
		ent->client->ps.eFlags &= ~EF_CONNECTION;
	}
#endif

	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		SpectatorClientEndFrame( ent );
		return;
	}

	// turn off any expired powerups
    for ( i=0; i<MAX_POWERUPS; i++ )
    {// turn off any expired powerups

            //OSP: pause
            //      If we're paused, update powerup timers accordingly.
            //      Make sure we dont let stuff like CTF flags expire.
            if ( ent->client->ps.powerups[i] == 0 )
                    continue;

            if ( level.pause.state != PAUSE_NONE && ent->client->ps.powerups[i] != INT_MAX && !ent->client->sess.inRacemode )
                    ent->client->ps.powerups[i] += level.time - level.previousTime;

            if ( ent->client->ps.powerups[i] < level.time )
                    ent->client->ps.powerups[i] = 0;
    }

    //OSP: pause
    //      If we're paused, make sure other timers stay in sync
    if ( level.pause.state != PAUSE_NONE && !ent->client->sess.inRacemode ) {
            int i, time_delta = level.time - level.previousTime;

            ent->client->airOutTime += time_delta;
            ent->client->sess.inactivityTime += (time_delta / 1000);
            ent->client->pers.enterTime += time_delta;
            ent->client->pers.teamState.lastreturnedflag += time_delta;
            ent->client->pers.teamState.lasthurtcarrier += time_delta;
            ent->client->pers.teamState.lastfraggedcarrier += time_delta;
			ent->client->pers.teamState.flagsince.startTime += time_delta; // base_enhanced
			ent->client->pers.protsince += time_delta; // force stats
            ent->client->respawnTime += time_delta;
			if (ent->client->invulnerableTimer)
				ent->client->invulnerableTimer += time_delta;
            ent->pain_debounce_time += time_delta;
            ent->client->ps.fd.forcePowerRegenDebounceTime += time_delta;
			if (ent->client->ps.saberDidThrowTime)
				ent->client->ps.saberDidThrowTime += time_delta;
			if (ent->client->ps.saberThrowDelay)
				ent->client->ps.saberThrowDelay += time_delta;
			if (ent->client->saberKnockedTime)
				ent->client->saberKnockedTime += time_delta;
			if (ent->client->homingLockTime)
				ent->client->homingLockTime += time_delta;
			if (ent->client->ps.fallingToDeath)
				ent->client->ps.fallingToDeath += time_delta;

			// update force powers durations
			for(i=0;i < NUM_FORCE_POWERS;++i) {
				if (ent->client->ps.fd.forcePowerDuration[i])
					ent->client->ps.fd.forcePowerDuration[i] += time_delta;
			}
			
    }


	// save network bandwidth
#if 0
	if ( !g_synchronousClients->integer && (ent->client->ps.pm_type == PM_NORMAL || ent->client->ps.pm_type == PM_JETPACK || ent->client->ps.pm_type == PM_FLOAT) ) {
		// FIXME: this must change eventually for non-sync demo recording
		VectorClear( ent->client->ps.viewangles );
	}
#endif

	//
	// If the end of unit layout is displayed, don't give
	// the player any normal movement attributes
	//
	if ( level.intermissiontime ) {
		if ( ent->s.number < MAX_CLIENTS
			|| ent->client->NPC_class == CLASS_VEHICLE )
		{//players and vehicles do nothing in intermissions
			return;
		}
	}

	// burn from lava, etc
	P_WorldEffects (ent);

	// apply all the damage taken this frame
	P_DamageFeedback (ent);

	ent->client->ps.stats[STAT_HEALTH] = ent->health;	// FIXME: get rid of ent->health...

	G_SetClientSound (ent);

	// set the latest infor
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, ent->client->ps.commandTime, qfalse );
		//rww - 12-03-02 - Don't snap the origin of players! It screws prediction all up.
	}
	else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qfalse );
	}

	if (isNPC)
	{
		ent->s.eType = ET_NPC;
	}

	SendPendingPredictableEvents( &ent->client->ps );

}


