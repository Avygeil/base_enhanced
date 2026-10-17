// "eat" command for alpha/duo: grab nearby players and eat them like a rancor does

#include "g_local.h"

extern void G_Dismember( gentity_t *ent, gentity_t *enemy, vec3_t point, int limbType, float limbRollBase, float limbPitchBase, int deathAnim, qboolean postDeath );

#define EAT_SWIPE_ANIM			BOTH_MELEE2
#define EAT_LIFT_ANIM			BOTH_FORCEGRIP3
#define EAT_HELD_ANIM			BOTH_SWIM_IDLE1
#define EAT_GRAB_RADIUS			88.0f
#define EAT_GRAB_FRACTION		0.45f	// fraction of the swipe anim at which the hand is out
#define EAT_LIFT_TIME			900		// hand-to-mouth travel of the victims
#define EAT_CHOMPS				4
#define EAT_FIRST_CHOMP			1000	// into the lift
#define EAT_CHOMP_GAP			1500
#define EAT_CHOMP_TIME(n)		( EAT_FIRST_CHOMP + (n) * EAT_CHOMP_GAP )
#define EAT_KILL_TIME			EAT_CHOMP_TIME( EAT_CHOMPS - 1 ) // the last chomp cuts in half and kills
#define EAT_SWALLOW_DELAY		450		// after the kill: victims vanish
#define EAT_RELEASE_DELAY		1000	// after the kill: sequence ends
#define EAT_CHOMP_MOVE_TIME		450		// victim thrashes into the eater's face and back over this long
#define EAT_HOLD_DIST			20.0f	// resting distance in front of the eater
#define EAT_HOLD_DROP			10.0f	// resting height below eye level
#define EAT_FACE_DIST			4.0f
#define EAT_THRASH_JITTER		5.0f
#define EAT_THRASH_ANGLE		35.0f

typedef enum {
	EAT_STAGE_HELD,
	EAT_STAGE_KILLED,
	EAT_STAGE_SWALLOWED
} eatStage_t;

static void G_EatSetAnim( gentity_t *ent, animNumber_t anim ) {
	ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
	ent->client->ps.forceDodgeAnim = anim;
	ent->client->ps.forceHandExtendTime = level.time + BG_AnimLength( ent->localAnimIndex, anim );
}

static void G_EatHandPos( gentity_t *eater, vec3_t out ) {
	vec3_t	angles = { 0, eater->client->ps.viewangles[YAW], 0 }, fwd;

	if ( eater->ghoul2 && trap_G2_HaveWeGhoul2Models( eater->ghoul2 ) ) {
		mdxaBone_t mat;

		trap_G2API_GetBoltMatrix( eater->ghoul2, 0, 0, &mat, angles, eater->client->ps.origin, level.time, NULL, eater->modelScale ); // bolt 0 is the right hand
		BG_GiveMeVectorFromMatrix( &mat, ORIGIN, out );
		if ( DistanceSquared( out, eater->client->ps.origin ) < 100.0f * 100.0f )
			return;
	}
	AngleVectors( angles, fwd, NULL, NULL );
	VectorMA( eater->client->ps.origin, 40.0f, fwd, out );
	out[2] += 16.0f;
}

static void G_EatPosInFront( gentity_t *eater, float dist, float drop, vec3_t out ) {
	vec3_t	angles = { 0, eater->client->ps.viewangles[YAW], 0 }, fwd;

	AngleVectors( angles, fwd, NULL, NULL );
	VectorMA( eater->client->ps.origin, dist, fwd, out );
	out[2] += eater->client->ps.viewheight - drop;
}

// the player holding someone in their hand or mouth
qboolean G_EatIsEating( gentity_t *ent ) {
	return ent && ent->client && ( ent->client->eatState == EAT_CARRY || ent->client->eatState == EAT_LIFT );
}

// the player eating this victim, if any
gentity_t *G_EatHeldBy( gentity_t *victim ) {
	gentity_t *eater;

	if ( !victim || !victim->client || !( victim->client->ps.eFlags2 & EF2_HELD_BY_MONSTER ) ||
		!victim->client->ps.hasLookTarget || victim->client->ps.lookTarget >= MAX_CLIENTS )
		return NULL;
	eater = &g_entities[victim->client->ps.lookTarget];
	if ( !eater->inuse || !eater->client || eater->client->eatState == EAT_NONE )
		return NULL;
	return eater;
}

void G_EatReleaseVictim( gentity_t *victim ) {
	gclient_t *cl = victim->client;

	if ( !cl )
		return;
	cl->ps.eFlags2 &= ~EF2_HELD_BY_MONSTER;
	cl->ps.hasLookTarget = qfalse;
	cl->ps.lookTarget = ENTITYNUM_NONE;
	if ( cl->ps.forceHandExtend == HANDEXTEND_DODGE && cl->ps.forceDodgeAnim == EAT_HELD_ANIM ) {
		cl->ps.forceHandExtend = HANDEXTEND_NONE;
		cl->ps.forceHandExtendTime = 0;
	}
	if ( victim->health > 0 ) {
		cl->ps.eFlags &= ~EF_NODRAW;
		if ( cl->solidHack ) {
			cl->solidHack = 0;
			victim->r.contents = CONTENTS_BODY;
			trap_LinkEntity( victim );
		}
	}
}

// takes one victim out of the sequence (moved to another team, etc.); the rest carry on
void G_EatDropVictim( gentity_t *victim ) {
	gentity_t *eater = G_EatHeldBy( victim );

	if ( eater )
		eater->client->eatVictimMask &= ~( 1u << victim->s.number );
	G_EatReleaseVictim( victim );
}

void G_EatReleaseAll( gentity_t *eater ) {
	gclient_t *cl = eater->client;
	int i;

	if ( !cl || cl->eatState == EAT_NONE )
		return;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( cl->eatVictimMask & ( 1u << i ) )
			G_EatReleaseVictim( &g_entities[i] );
	}
	cl->eatVictimMask = 0;
	cl->eatState = EAT_NONE;
	cl->ps.forceHandExtend = HANDEXTEND_NONE;
	cl->ps.forceHandExtendTime = 0;
}

static qboolean G_EatCanEat( gentity_t *eater, gentity_t *victim ) {
	if ( victim == eater || !victim->inuse || !victim->client )
		return qfalse;
	if ( victim->client->pers.connected != CON_CONNECTED || victim->health <= 0 )
		return qfalse;
	if ( victim->client->sess.sessionTeam == TEAM_SPECTATOR || victim->client->sess.inRacemode || victim->isAimPracticePack )
		return qfalse;
	if ( victim->client->ps.eFlags2 & EF2_HELD_BY_MONSTER )
		return qfalse;
	if ( victim->client->ps.duelInProgress || victim->client->ps.m_iVehicleNum )
		return qfalse;
	return qtrue;
}

static int G_EatGrab( gentity_t *eater ) {
	gclient_t	*cl = eater->client;
	vec3_t		hand, eye, fwd, dir;
	trace_t		tr;
	int			i, count = 0;

	G_EatHandPos( eater, hand );
	VectorCopy( cl->ps.origin, eye );
	eye[2] += cl->ps.viewheight;
	AngleVectors( (vec3_t){ 0, cl->ps.viewangles[YAW], 0 }, fwd, NULL, NULL );

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		gentity_t *victim = &g_entities[i];

		if ( !G_EatCanEat( eater, victim ) )
			continue;
		if ( DistanceSquared( victim->r.currentOrigin, hand ) > EAT_GRAB_RADIUS * EAT_GRAB_RADIUS )
			continue;
		VectorSubtract( victim->r.currentOrigin, cl->ps.origin, dir );
		dir[2] = 0;
		VectorNormalize( dir );
		if ( DotProduct( dir, fwd ) < 0 ) // only what is in front of us
			continue;
		trap_Trace( &tr, eye, NULL, NULL, victim->r.currentOrigin, eater->s.number, MASK_SOLID );
		if ( tr.fraction < 1.0f )
			continue;

		victim->client->ps.eFlags2 |= EF2_HELD_BY_MONSTER;
		victim->client->ps.hasLookTarget = qtrue;
		victim->client->ps.lookTarget = eater->s.number;
		cl->eatVictimMask |= 1u << i;
		count++;
		G_EntitySound( victim, CHAN_VOICE, G_SoundIndex( "*falling1.wav" ) ); // death scream on pickup
	}
	return count;
}

static void G_EatKill( gentity_t *eater, gentity_t *victim ) {
	victim->client->eatenBy = eater->s.number + 1;
	G_Dismember( victim, eater, victim->r.currentOrigin, G2_MODELPART_WAIST, 90, 0, victim->client->ps.torsoAnim, qtrue );
	G_Damage( victim, eater, eater, vec3_origin, victim->r.currentOrigin, victim->health + 10,
		DAMAGE_NO_PROTECTION | DAMAGE_NO_ARMOR | DAMAGE_NO_KNOCKBACK | DAMAGE_NO_HIT_LOC, MOD_MELEE );
	victim->client->solidHack = 0; // corpse contents are set by the death
	if ( victim->health > 0 ) { // something swallowed the damage (godmode and the like)
		victim->health = -1;
		victim->client->ps.stats[STAT_HEALTH] = -1;
		victim->die( victim, eater, eater, 100000, MOD_MELEE );
	}
	victim->client->ps.forceHandExtend = HANDEXTEND_NONE;
	victim->client->ps.forceHandExtendTime = 0;
}

void G_EatThink( gentity_t *eater ) {
	gclient_t	*cl = eater->client;
	int			i, elapsed, animLen;

	if ( !cl || cl->eatState == EAT_NONE )
		return;

	if ( eater->health <= 0 || cl->ps.pm_type == PM_DEAD || cl->sess.sessionTeam == TEAM_SPECTATOR ||
		!DoMemeNonRace( eater->s.number ) ) {
		G_EatReleaseAll( eater );
		return;
	}

	// drop victims that left, got killed by someone else, or are otherwise out of the picture
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		gentity_t *victim = &g_entities[i];

		if ( !( cl->eatVictimMask & ( 1u << i ) ) )
			continue;
		if ( !victim->inuse || !victim->client || victim->client->pers.connected != CON_CONNECTED ||
			victim->client->sess.sessionTeam == TEAM_SPECTATOR || ( victim->health <= 0 && cl->eatStage == EAT_STAGE_HELD ) ) {
			if ( victim->inuse && victim->client )
				G_EatReleaseVictim( victim );
			cl->eatVictimMask &= ~( 1u << i );
		}
	}

	elapsed = level.time - cl->eatStateTime;

	switch ( cl->eatState ) {
	case EAT_SWIPE:
		animLen = BG_AnimLength( eater->localAnimIndex, EAT_SWIPE_ANIM );
		if ( elapsed >= animLen * EAT_GRAB_FRACTION ) {
			if ( G_EatGrab( eater ) ) {
				cl->eatState = EAT_CARRY;
			} else {
				cl->eatState = EAT_NONE; // whiffed; the swipe anim plays out on its own
			}
		}
		break;

	case EAT_CARRY:
		if ( !cl->eatVictimMask ) {
			cl->eatState = EAT_NONE;
			break;
		}
		if ( elapsed >= BG_AnimLength( eater->localAnimIndex, EAT_SWIPE_ANIM ) ) {
			cl->eatState = EAT_LIFT;
			cl->eatStateTime = level.time;
			cl->eatStage = EAT_STAGE_HELD;
			cl->eatChompsDone = 0;
			G_EatSetAnim( eater, EAT_LIFT_ANIM );
		}
		break;

	case EAT_LIFT:
		if ( !cl->eatVictimMask && cl->eatStage == EAT_STAGE_HELD ) {
			G_EatReleaseAll( eater );
			break;
		}
		// keep the lift pose until the sequence ends
		if ( cl->ps.forceHandExtendTime < level.time + 250 )
			cl->ps.forceHandExtendTime = level.time + 250;

		while ( cl->eatChompsDone < EAT_CHOMPS && elapsed >= EAT_CHOMP_TIME( cl->eatChompsDone ) ) {
			cl->eatChompsDone++;
			G_Sound( eater, CHAN_AUTO, G_SoundIndex( "sound/chars/rancor/chomp.wav" ) );
		}
		if ( cl->eatStage == EAT_STAGE_HELD && cl->eatChompsDone >= EAT_CHOMPS ) {
			cl->eatStage = EAT_STAGE_KILLED;
			for ( i = 0; i < MAX_CLIENTS; i++ ) {
				if ( cl->eatVictimMask & ( 1u << i ) ) {
					G_EatKill( eater, &g_entities[i] );
					G_Sound( &g_entities[i], CHAN_AUTO, G_SoundIndex( "sound/chars/rancor/swipehit.wav" ) );
				}
			}
		}
		if ( cl->eatStage == EAT_STAGE_KILLED && elapsed >= EAT_KILL_TIME + EAT_SWALLOW_DELAY ) {
			cl->eatStage = EAT_STAGE_SWALLOWED;
			for ( i = 0; i < MAX_CLIENTS; i++ ) {
				gentity_t *victim = &g_entities[i];

				if ( !( cl->eatVictimMask & ( 1u << i ) ) )
					continue;
				if ( victim->health > 0 ) // survived somehow
					G_EatKill( eater, victim );
				victim->client->ps.eFlags |= EF_NODRAW;
			}
		}
		if ( cl->eatStage == EAT_STAGE_SWALLOWED && elapsed >= EAT_KILL_TIME + EAT_RELEASE_DELAY )
			G_EatReleaseAll( eater ); // victims stay invisible until they respawn
		break;

	default:
		break;
	}
}

// the eater holds still and can't swing while the sequence runs
void G_EatLockCmd( gentity_t *eater, usercmd_t **ucmd ) {
	if ( !eater->client || eater->client->eatState == EAT_NONE )
		return;
	( *ucmd )->forwardmove = 0;
	( *ucmd )->rightmove = 0;
	( *ucmd )->upmove = 0;
	( *ucmd )->buttons &= ~( BUTTON_ATTACK | BUTTON_ALT_ATTACK );
}

// called from G_HeldByMonster when the holder is a player; returns qfalse if the victim should be let go
qboolean G_EatHoldVictim( gentity_t *eater, gentity_t *victim, usercmd_t **ucmd ) {
	gclient_t	*ecl = eater->client, *vcl = victim->client;
	vec3_t		hand, rest, face, delta;
	float		thrash = 0.0f; // 0..1 envelope of the chomp in progress
	int			i;

	if ( ecl->eatState == EAT_NONE || !( ecl->eatVictimMask & ( 1u << victim->s.number ) ) ) {
		G_EatReleaseVictim( victim );
		return qfalse;
	}

	G_EatHandPos( eater, hand );
	if ( ecl->eatState == EAT_LIFT ) {
		float f = Com_Clamp( 0.0f, 1.0f, ( level.time - ecl->eatStateTime ) / (float)EAT_LIFT_TIME );

		G_EatPosInFront( eater, EAT_HOLD_DIST, EAT_HOLD_DROP, rest );
		VectorSubtract( rest, hand, delta );
		VectorMA( hand, f, delta, vcl->ps.origin );

		// thrash right up to the eater's face on each chomp
		for ( i = 0; i < EAT_CHOMPS; i++ ) {
			int dt = level.time - ecl->eatStateTime - EAT_CHOMP_TIME( i );

			if ( dt >= 0 && dt < EAT_CHOMP_MOVE_TIME ) {
				thrash = (float)sin( M_PI * dt / EAT_CHOMP_MOVE_TIME );
				break;
			}
		}
		if ( thrash > 0.0f ) {
			G_EatPosInFront( eater, EAT_FACE_DIST, 0.0f, face );
			VectorSubtract( face, vcl->ps.origin, delta );
			VectorMA( vcl->ps.origin, thrash, delta, vcl->ps.origin );
			for ( i = 0; i < 3; i++ )
				vcl->ps.origin[i] += crandom() * EAT_THRASH_JITTER * thrash;
		}
	} else {
		VectorCopy( hand, vcl->ps.origin );
	}

	VectorSet( vcl->ps.viewangles, 0, AngleNormalize360( ecl->ps.viewangles[YAW] + 180.0f ), 0 );
	if ( thrash > 0.0f ) {
		vcl->ps.viewangles[PITCH] += crandom() * EAT_THRASH_ANGLE * thrash;
		vcl->ps.viewangles[ROLL] += crandom() * EAT_THRASH_ANGLE * thrash;
	}
	if ( victim->health > 0 ) {
		vcl->ps.forceHandExtend = HANDEXTEND_DODGE;
		vcl->ps.forceDodgeAnim = EAT_HELD_ANIM;
		vcl->ps.forceHandExtendTime = level.time + 250;
	}
	( *ucmd )->buttons = 0;

	// no body collision while held, or the victim's own movement shoves them out of the eater's box
	if ( victim->health > 0 ) {
		vcl->solidHack = level.time + 200;
		victim->r.contents = 0;
	} else {
		vcl->solidHack = 0;
	}
	return qtrue;
}

void Cmd_Eat_f( gentity_t *ent ) {
	gclient_t *cl = ent->client;

	if ( cl->eatState != EAT_NONE )
		return;
	if ( ent->health <= 0 || cl->ps.pm_type != PM_NORMAL || cl->sess.sessionTeam == TEAM_SPECTATOR )
		return;
	if ( cl->ps.groundEntityNum == ENTITYNUM_NONE || cl->ps.m_iVehicleNum || cl->ps.duelInProgress )
		return;
	if ( ( cl->ps.eFlags2 & EF2_HELD_BY_MONSTER ) || cl->ps.forceHandExtend != HANDEXTEND_NONE )
		return;

	// make sure the sounds are registered before they are needed
	G_SoundIndex( "*falling1.wav" );
	G_SoundIndex( "sound/chars/rancor/swipehit.wav" );
	G_SoundIndex( "sound/chars/rancor/chomp.wav" );

	cl->eatState = EAT_SWIPE;
	cl->eatStateTime = level.time;
	cl->eatVictimMask = 0;
	G_EatSetAnim( ent, EAT_SWIPE_ANIM );
}
