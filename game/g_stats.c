#include "g_local.h"
#include "bg_saga.h"
#include "g_database.h"

const char *CtfStatsTableCallback_Name(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	if (stats->isTotal)
		return "^9TOTAL";
	return stats->name;
}

const char *CtfStatsTableCallback_Alias(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	if (stats->isTotal)
		return NULL;
	if (stats->isBot)
		return "BOT";
	if (stats->accountId != ACCOUNT_ID_UNLINKED && stats->accountName[0])
		return stats->accountName;
	return NULL;
}

// gold if best overall; green if best on team; uncolored otherwise
const char *FormatStatInt(qboolean isTotal, int num, int bestMyTeam, int bestOtherTeam) {
	if (isTotal)
		return va("^9%d", num);
	if (!num && !bestMyTeam)
		return va("%d", num); // everybody has zero on my team; no color
	if (num >= bestMyTeam && num >= bestOtherTeam)
		return va("^3%d", num);
	if (num >= bestMyTeam)
		return va("^2%d", num);
	return va("%d", num);
}

// gold if best overall; green if best on team; uncolored otherwise
const char *FormatStatTime(qboolean isTotal, int num, int bestMyTeam, int bestOtherTeam) {
	int secs = num / 1000;
	int mins = secs / 60;

#if 1
	// more or less than a minute?
	if (num >= 60000) {
		secs %= 60;
		if (isTotal)
			return va("^9%dm%02ds", mins, secs);
		if (!num && !bestMyTeam)
			return va("%dm%02ds", mins, secs);  // everybody has zero on my team; no color
		if (num >= bestMyTeam && num >= bestOtherTeam)
			return va("^3%dm%02ds", mins, secs);
		if (num >= bestMyTeam)
			return va("^2%dm%02ds", mins, secs);
		return va("%dm%02ds", mins, secs);
	}
	else {
		if (isTotal)
			return va("^9%ds", secs);
		if (!num && !bestMyTeam)
			return va("%ds", secs);  // everybody has zero on my team; no color
		if (num >= bestMyTeam && num >= bestOtherTeam)
			return va("^3%ds", secs);
		if (num >= bestMyTeam)
			return va("^2%ds", secs);
		return va("%ds", secs);
	}
#else
	// more or less than a minute?
	if (num >= 60000) {
		secs %= 60;
		if (isTotal)
			return va("^9%d:%02ds", mins, secs);
		if (!num && !bestMyTeam)
			return va("%d:%02d", mins, secs);  // everybody has zero on my team; no color
		if (num >= bestMyTeam && num >= bestOtherTeam)
			return va("^3%d:%02d", mins, secs);
		if (num >= bestMyTeam)
			return va("^2%d:%02d", mins, secs);
		return va("%d:%02ds", mins, secs);
	}
	else {
		if (isTotal)
			return va("^9:%02d", secs);
		if (!num && !bestMyTeam)
			return va(":%02d", secs);  // everybody has zero on my team; no color
		if (num >= bestMyTeam && num >= bestOtherTeam)
			return va("^3:%02d", secs);
		if (num >= bestMyTeam)
			return va("^2:%02d", secs);
		return va(":%02d", secs);
	}
#endif
}

stats_t bestStats[TEAM_NUM_TEAMS] = { 0 }; // e.g. bestStats[TEAM_RED].captures contains the best captures of anyone that was on red team
const char *CtfStatsTableCallback_Captures(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->captures, bestStats[stats->lastTeam].captures, bestStats[OtherTeam(stats->lastTeam)].captures);
}

const char *CtfStatsTableCallback_Assists(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->assists, bestStats[stats->lastTeam].assists, bestStats[OtherTeam(stats->lastTeam)].assists);
}

const char *CtfStatsTableCallback_Defends(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->defends, bestStats[stats->lastTeam].defends, bestStats[OtherTeam(stats->lastTeam)].defends);
}

const char *CtfStatsTableCallback_Accuracy(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->accuracy, bestStats[stats->lastTeam].accuracy, bestStats[OtherTeam(stats->lastTeam)].accuracy);
}

const char *CtfStatsTableCallback_Airs(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->airs, bestStats[stats->lastTeam].airs, bestStats[OtherTeam(stats->lastTeam)].airs);
}

const char *CtfStatsTableCallback_Pits(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->pits, bestStats[stats->lastTeam].pits, bestStats[OtherTeam(stats->lastTeam)].pits);
}

const char *CtfStatsTableCallback_Pitted(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->pitted, bestStats[stats->lastTeam].pitted, bestStats[OtherTeam(stats->lastTeam)].pitted);
}

const char *CtfStatsTableCallback_FcKills(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->fcKills, bestStats[stats->lastTeam].fcKills, bestStats[OtherTeam(stats->lastTeam)].fcKills);
}

const char *CtfStatsTableCallback_Rets(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->rets, bestStats[stats->lastTeam].rets, bestStats[OtherTeam(stats->lastTeam)].rets);
}

const char *CtfStatsTableCallback_Selfkills(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->selfkills, bestStats[stats->lastTeam].selfkills, bestStats[OtherTeam(stats->lastTeam)].selfkills);
}

const char *CtfStatsTableCallback_BoonPickups(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->boonPickups, bestStats[stats->lastTeam].boonPickups, bestStats[OtherTeam(stats->lastTeam)].boonPickups);
}

const char *CtfStatsTableCallback_TotalHold(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatTime(stats->isTotal, stats->totalFlagHold, bestStats[stats->lastTeam].totalFlagHold, bestStats[OtherTeam(stats->lastTeam)].totalFlagHold);
}

const char *CtfStatsTableCallback_MaxHold(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatTime(stats->isTotal, stats->longestFlagHold, bestStats[stats->lastTeam].longestFlagHold, bestStats[OtherTeam(stats->lastTeam)].longestFlagHold);
}

const char *CtfStatsTableCallback_Saves(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->saves, bestStats[stats->lastTeam].saves, bestStats[OtherTeam(stats->lastTeam)].saves);
}

const char *CtfStatsTableCallback_DamageDealt(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->damageDealtTotal, bestStats[stats->lastTeam].damageDealtTotal, bestStats[OtherTeam(stats->lastTeam)].damageDealtTotal);
}

const char *CtfStatsTableCallback_DamageTaken(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->damageTakenTotal, bestStats[stats->lastTeam].damageTakenTotal, bestStats[OtherTeam(stats->lastTeam)].damageTakenTotal);
}

const char *CtfStatsTableCallback_TopSpeed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, (int)(stats->topSpeed + 0.5f), (int)(bestStats[stats->lastTeam].topSpeed + 0.5f), (int)(bestStats[OtherTeam(stats->lastTeam)].topSpeed + 0.5f));
}

const char *CtfStatsTableCallback_AverageSpeed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->averageSpeed, bestStats[stats->lastTeam].averageSpeed, bestStats[OtherTeam(stats->lastTeam)].averageSpeed);
}

const char *CtfStatsTableCallback_Push(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->push, bestStats[stats->lastTeam].push, bestStats[OtherTeam(stats->lastTeam)].push);
}

const char *CtfStatsTableCallback_Pull(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->pull, bestStats[stats->lastTeam].pull, bestStats[OtherTeam(stats->lastTeam)].pull);
}

const char *CtfStatsTableCallback_Healed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->healed, bestStats[stats->lastTeam].healed, bestStats[OtherTeam(stats->lastTeam)].healed);
}

const char *CtfStatsTableCallback_EnergizedAlly(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->energizedAlly, bestStats[stats->lastTeam].energizedAlly, bestStats[OtherTeam(stats->lastTeam)].energizedAlly);
}

const char *CtfStatsTableCallback_EnergizedEnemy(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->energizedEnemy, bestStats[stats->lastTeam].energizedEnemy, bestStats[OtherTeam(stats->lastTeam)].energizedEnemy);
}

const char *CtfStatsTableCallback_Absorbed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->absorbed, bestStats[stats->lastTeam].absorbed, bestStats[OtherTeam(stats->lastTeam)].absorbed);
}

const char *CtfStatsTableCallback_ProtDamage(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->protDamageAvoided, bestStats[stats->lastTeam].protDamageAvoided, bestStats[OtherTeam(stats->lastTeam)].protDamageAvoided);
}

const char *CtfStatsTableCallback_ProtTime(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatTime(stats->isTotal, stats->protTimeUsed, bestStats[stats->lastTeam].protTimeUsed, bestStats[OtherTeam(stats->lastTeam)].protTimeUsed);
}

#if 0
const char *CtfStatsTableCallback_CtfRegionTime(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	ctfRegion_t region = *((ctfRegion_t *)columnContext);
	return FormatStatTime(stats->isTotal, stats->regionTime[region], bestStats[stats->lastTeam].regionTime[region], bestStats[OtherTeam(stats->lastTeam)].regionTime[region]);
}
#endif

const char *CtfStatsTableCallback_CtfRegionPercent(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	ctfRegion_t region = *((ctfRegion_t *)columnContext);
	return FormatStatInt(stats->isTotal, stats->regionPercent[region], bestStats[stats->lastTeam].regionPercent[region], bestStats[OtherTeam(stats->lastTeam)].regionPercent[region]);
}

const char *TableCallback_Damage(void *rowContext, void *columnContext) {
	if (!rowContext || !columnContext)
		return NULL;
	stats_t *attacker = rowContext;
	stats_t *victim = columnContext;

	iterator_t iter;
	ListIterate(&attacker->damageGivenList, &iter, qfalse);

	damageCounter_t *damage = NULL;
	while (IteratorHasNext(&iter)) {
		damageCounter_t *thisGuy = IteratorNext(&iter);
		if (thisGuy->otherPlayerSessionId != victim->sessionId)
			continue;
		damage = thisGuy;
		break;
	}

	if (!damage)
		return attacker->isTotal ? "^90" : "0";

	if (attacker->isTotal)
		return FormatStatInt(qtrue, damage ? damage->totalAmount : 0, 0, 0);

	damageCounter_t *bestDamageOnTeam = NULL, *bestDamageOnOtherTeam = NULL;
	ListIterate(attacker->lastTeam == TEAM_RED ? &bestStats[TEAM_RED].damageGivenList : &bestStats[TEAM_BLUE].damageGivenList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		damageCounter_t *thisGuy = IteratorNext(&iter);
		if (thisGuy->otherPlayerSessionId != victim->sessionId)
			continue;
		bestDamageOnTeam = thisGuy;
		break;
	}
	ListIterate(attacker->lastTeam == TEAM_RED ? &bestStats[TEAM_BLUE].damageGivenList : &bestStats[TEAM_RED].damageGivenList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		damageCounter_t *thisGuy = IteratorNext(&iter);
		if (thisGuy->otherPlayerSessionId != victim->sessionId)
			continue;
		bestDamageOnOtherTeam = thisGuy;
		break;
	}

	return FormatStatInt(attacker->isTotal, damage ? damage->totalAmount : 0, bestDamageOnTeam ? bestDamageOnTeam->totalAmount : 0, bestDamageOnOtherTeam ? bestDamageOnOtherTeam->totalAmount : 0);
}

const char *TableCallback_DamageName(void *rowContext, void *columnContext) {
	if (!rowContext)
		return NULL;
	gentity_t *rowPlayer = rowContext;
	return rowPlayer->client->pers.netname;
}

const char *TableCallback_WeaponName(void *rowContext, void *columnContext) {
	if (!rowContext)
		return NULL;
	gentity_t *rowPlayer = rowContext;
	return rowPlayer->client->pers.netname;
}

static qboolean MatchesDamage(genericNode_t *node, void *userData) {
	const damageCounter_t *existing = (const damageCounter_t *)node;
	const stats_t *thisGuy = (const stats_t *)userData;

	if (!existing || !thisGuy || thisGuy->isBot != existing->otherPlayerIsBot)
		return qfalse;

	if (thisGuy->isBot) {
		if (thisGuy->sessionId == existing->otherPlayerSessionId)
			return qtrue;
		return qfalse;
	}

	/*if (thisGuy->accountId != ACCOUNT_ID_UNLINKED && thisGuy->accountId == existing->otherPlayerAccountId)
		return qtrue; // matches a linked account*/

	if (existing->otherPlayerSessionId == thisGuy->sessionId)
		return qtrue; // matches a session

	return qfalse;
}

meansOfDeathCategory_t MeansOfDeathCategoryForMeansOfDeath(meansOfDeath_t mod) {
	switch (mod) {
	case MOD_MELEE: case MOD_STUN_BATON: case MOD_VEHICLE: /*idk*/ return MODC_MELEESTUNBATON;
	case MOD_SABER: return MODC_SABER;
	case MOD_BRYAR_PISTOL: case MOD_BRYAR_PISTOL_ALT: return MODC_PISTOL;
	case MOD_BLASTER: case MOD_TURBLAST: /*???*/ return MODC_BLASTER;
	case MOD_DISRUPTOR: case MOD_DISRUPTOR_SNIPER: case MOD_DISRUPTOR_SPLASH: /*???*/ return MODC_DISRUPTOR;
	case MOD_BOWCASTER: return MODC_BOWCASTER;
	case MOD_REPEATER: case MOD_REPEATER_ALT: case MOD_REPEATER_ALT_SPLASH: return MODC_REPEATER;
	case MOD_DEMP2: case MOD_DEMP2_ALT: return MODC_DEMP;
	case MOD_FLECHETTE: case MOD_FLECHETTE_ALT_SPLASH: return MODC_GOLAN;
	case MOD_ROCKET: case MOD_ROCKET_HOMING: case MOD_ROCKET_HOMING_SPLASH: case MOD_ROCKET_SPLASH: return MODC_ROCKET;
	case MOD_CONC: case MOD_CONC_ALT: return MODC_CONCUSSION;
	case MOD_THERMAL: case MOD_THERMAL_SPLASH: return MODC_THERMAL;
	case MOD_TRIP_MINE_SPLASH: case MOD_TIMED_MINE_SPLASH: return MODC_MINE;
	case MOD_DET_PACK_SPLASH: return MODC_DETPACK;
	case MOD_FORCE_DARK: return MODC_FORCE;
	default: assert(qfalse); return MODC_INVALID;
	}
}

const char *TableCallback_WeaponDamage(void *rowContext, void *columnContext) {
	if (!rowContext || !columnContext)
		return NULL;
	stats_t *rowPlayer = rowContext;
	meansOfDeathCategoryContext_t *context = columnContext;
	stats_t *tablePlayer = context->tablePlayerStats;
	assert(context->tablePlayerStats);

	if (rowPlayer->isTotal) {
		int damage = 0;
		if (context->modc == MODC_ALL_TYPES_COMBINED) {
			for (int i = MODC_FIRST; i < MODC_ALL_TYPES_COMBINED; i++)
				damage += rowPlayer->damageOfType[i];
		}
		else {
			damage = rowPlayer->damageOfType[context->modc];
		}
		return FormatStatInt(qtrue, damage, 0, 0);
	}

	int mostDamageThisTeam = bestStats[rowPlayer->lastTeam].damageOfType[context->modc];
	int mostDamageOtherTeam = bestStats[OtherTeam(rowPlayer->lastTeam)].damageOfType[context->modc];

	damageCounter_t *dmgPtr = ListFind(context->damageTaken ? &tablePlayer->damageTakenList : &tablePlayer->damageGivenList, MatchesDamage, rowPlayer, NULL);
	int damage = 0;
	if (dmgPtr) {
		if (context->modc == MODC_ALL_TYPES_COMBINED) {
			for (int i = MODC_FIRST; i < MODC_ALL_TYPES_COMBINED; i++)
				damage += dmgPtr->ofType[i];
		}
		else {
			damage = dmgPtr->ofType[context->modc];
		}
	}

	return FormatStatInt(qfalse, damage, mostDamageThisTeam, mostDamageOtherTeam);
}

#define CheckBest(field) do { if (player->field > best->field) { best->field = player->field; } }  while (0)

// we use this on every player to set up the bestStats structs
// this also sets each player's accuracy and averageSpeed
static void CheckBestStats(stats_t *player, statsTableType_t type, stats_t *weaponStatsPtr) {
	assert(player);
	stats_t *best = &bestStats[player->lastTeam];

	if (type == STATS_TABLE_GENERAL) {
		CheckBest(score);
		CheckBest(captures);
		CheckBest(assists);
		CheckBest(defends);
		player->accuracy = player->accuracy_shots ? player->accuracy_hits * 100 / player->accuracy_shots : 0;
		CheckBest(accuracy);
		CheckBest(airs);
		CheckBest(pits);
		CheckBest(pitted);
		CheckBest(fcKills);
		CheckBest(rets);
		CheckBest(selfkills);
		CheckBest(boonPickups);
		CheckBest(totalFlagHold);
		CheckBest(longestFlagHold);
		CheckBest(saves);
		CheckBest(boonPickups);
		CheckBest(damageDealtTotal);
		CheckBest(damageTakenTotal);
		CheckBest(topSpeed);
		if (player->displacementSamples)
			player->averageSpeed = (int)floorf(((player->displacement * g_svfps.value) / player->displacementSamples) + 0.5f);
		else
			player->averageSpeed = (int)(player->topSpeed + 0.5f);
		CheckBest(averageSpeed);
	}
	else if (type == STATS_TABLE_FORCE) {
		CheckBest(push);
		CheckBest(pull);
		CheckBest(healed);
		CheckBest(energizedAlly);
		CheckBest(energizedEnemy);
		CheckBest(absorbed);
		CheckBest(protDamageAvoided);
		CheckBest(protTimeUsed);
	}
	else if (type == STATS_TABLE_MISC) {
		int allRegionsTime = 0;
		for (ctfRegion_t region = CTFREGION_FLAGSTAND; region <= CTFREGION_ENEMYFLAGSTAND; region++) {
			allRegionsTime += player->regionTime[region];
			CheckBest(regionTime[region]);
		}
		for (ctfRegion_t region = CTFREGION_FLAGSTAND; region <= CTFREGION_ENEMYFLAGSTAND; region++) {
			player->regionPercent[region] = Com_Clampi(0, 100, (int)round((float)player->regionTime[region] / (float)allRegionsTime * 100.0f));
			CheckBest(regionPercent[region]);
		}
	}
	else if (type == STATS_TABLE_DAMAGE) {
		iterator_t iter;
		ListIterate(&player->damageGivenList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			const damageCounter_t *thisDamageGivenByPlayer = IteratorNext(&iter);
			if (thisDamageGivenByPlayer->totalAmount <= 0)
				continue;

			iterator_t iter2;
			ListIterate(&best->damageGivenList, &iter2, qfalse);
			damageCounter_t *found = NULL;
			while (IteratorHasNext(&iter2)) {
				damageCounter_t *thisBestDamageOnTeam = IteratorNext(&iter2);
				if (thisBestDamageOnTeam->otherPlayerSessionId != thisDamageGivenByPlayer->otherPlayerSessionId)
					continue;
				found = thisBestDamageOnTeam;
				break;
			}

			if (found) {
				if (thisDamageGivenByPlayer->totalAmount > found->totalAmount)
					found->totalAmount = thisDamageGivenByPlayer->totalAmount;
			}
			else {
				found = ListAdd(&best->damageGivenList, sizeof(damageCounter_t));
				found->otherPlayerSessionId = thisDamageGivenByPlayer->otherPlayerSessionId;
				found->otherPlayerAccountId = thisDamageGivenByPlayer->otherPlayerAccountId;
				found->otherPlayerIsBot = thisDamageGivenByPlayer->otherPlayerIsBot;
				found->totalAmount = thisDamageGivenByPlayer->totalAmount;
			}
		}
	}
	else if (type == STATS_TABLE_WEAPON_GIVEN) {
		for (int i = 0; i < MODC_MAX; i++) {
			if (i == MODC_ALL_TYPES_COMBINED) {
				int damage = 0;
				for (int i = MODC_FIRST; i < MODC_ALL_TYPES_COMBINED; i++) {
					int *dmg = GetDamageGivenStatOfType(weaponStatsPtr, player, i);
					if (dmg)
						damage += *dmg;
				}
				if (damage > bestStats[player->lastTeam].damageOfType[i])
					bestStats[player->lastTeam].damageOfType[i] = damage;
			}
			else {
				int *dmg = GetDamageGivenStatOfType(weaponStatsPtr, player, i);
				if (dmg && *dmg > bestStats[player->lastTeam].damageOfType[i])
					bestStats[player->lastTeam].damageOfType[i] = *dmg;
			}
		}
	}
	else if (type == STATS_TABLE_WEAPON_TAKEN) {
		for (int i = 0; i < MODC_MAX; i++) {
			if (i == MODC_ALL_TYPES_COMBINED) {
				int damage = 0;
				for (int i = MODC_FIRST; i < MODC_ALL_TYPES_COMBINED; i++) {
					int *dmg = GetDamageTakenStatOfType(player, weaponStatsPtr, i);
					if (dmg)
						damage += *dmg;
				}
				if (damage > bestStats[player->lastTeam].damageOfType[i])
					bestStats[player->lastTeam].damageOfType[i] = damage;
			}
			else {
				int *dmg = GetDamageTakenStatOfType(player, weaponStatsPtr, i);
				if (dmg && *dmg > bestStats[player->lastTeam].damageOfType[i])
					bestStats[player->lastTeam].damageOfType[i] = *dmg;
			}
		}
	}
}

typedef struct {
	node_t		node;
	int			sessionId;
	qboolean	isBot;
	stats_t		*stats;
} gotPlayer_t;

static qboolean PlayerMatches(genericNode_t *node, void *userData) {
	const gotPlayer_t *existing = (const gotPlayer_t *)node;
	const stats_t *thisGuy = (const stats_t *)userData;

	if (existing && thisGuy && thisGuy->sessionId == existing->sessionId && thisGuy->isBot == existing->isBot)
		return qtrue;

	return qfalse;
}

// try to prevent people who join and immediately go spec from showing up on stats
qboolean StatsValid(const stats_t *stats) {
	int svFps = g_svfps.integer ? g_svfps.integer : 30; // prevent divide by zero
	if (stats->isBot || stats->displacementSamples / svFps >= 1 || stats->damageDealtTotal || stats->damageTakenTotal)
		return qtrue;
	return qfalse;
}

#define AddStatToTotal(field) do { team->field += player->field; }  while (0)
static void AddStatsToTotal(stats_t *player, stats_t *team, statsTableType_t type, stats_t *weaponStatsPtr) {
	assert(player && team);

	if (type == STATS_TABLE_GENERAL) {
		AddStatToTotal(score);
		AddStatToTotal(captures);
		AddStatToTotal(assists);
		AddStatToTotal(defends);
		AddStatToTotal(accuracy_shots);
		AddStatToTotal(accuracy_hits);
		team->accuracy = team->accuracy_shots ? team->accuracy_hits * 100 / team->accuracy_shots : 0;
		AddStatToTotal(airs);
		AddStatToTotal(pits);
		AddStatToTotal(pitted);
		AddStatToTotal(fcKills);
		AddStatToTotal(rets);
		AddStatToTotal(selfkills);
		AddStatToTotal(boonPickups);
		AddStatToTotal(totalFlagHold);
		AddStatToTotal(longestFlagHold);
		AddStatToTotal(saves);
		AddStatToTotal(damageDealtTotal);
		AddStatToTotal(damageTakenTotal);
		AddStatToTotal(topSpeed);
		AddStatToTotal(displacement);
		AddStatToTotal(displacementSamples);
		if (team->displacementSamples)
			team->averageSpeed = (int)floorf(((team->displacement * g_svfps.value) / team->displacementSamples) + 0.5f);
		else
			team->averageSpeed = (int)(team->topSpeed + 0.5f);
	}
	else if (type == STATS_TABLE_FORCE) {
		AddStatToTotal(push);
		AddStatToTotal(pull);
		AddStatToTotal(healed);
		AddStatToTotal(energizedAlly);
		AddStatToTotal(energizedEnemy);
		AddStatToTotal(absorbed);
		AddStatToTotal(protDamageAvoided);
		AddStatToTotal(protTimeUsed);
	}
	else if (type == STATS_TABLE_MISC) {
		int allRegionsTime = 0;
		for (ctfRegion_t region = CTFREGION_FLAGSTAND; region <= CTFREGION_ENEMYFLAGSTAND; region++) {
			AddStatToTotal(regionTime[region]);
			allRegionsTime += team->regionTime[region];
		}
		for (ctfRegion_t region = CTFREGION_FLAGSTAND; region <= CTFREGION_ENEMYFLAGSTAND; region++) {
			team->regionPercent[region] = Com_Clampi(0, 100, (int)round((float)team->regionTime[region] / (float)allRegionsTime * 100.0f));
		}
	}
	else if (type == STATS_TABLE_DAMAGE) {
		iterator_t iter;
		ListIterate((list_t *)&player->damageGivenList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			const damageCounter_t *thisDamageGivenByPlayer = IteratorNext(&iter);
			if (thisDamageGivenByPlayer->totalAmount <= 0)
				continue;

			iterator_t iter2;
			ListIterate(&team->damageGivenList, &iter2, qfalse);
			damageCounter_t *found = NULL;
			while (IteratorHasNext(&iter2)) {
				damageCounter_t *thisDamageGivenByTeam = IteratorNext(&iter2);
				if (thisDamageGivenByTeam->otherPlayerSessionId != thisDamageGivenByPlayer->otherPlayerSessionId)
					continue;
				found = thisDamageGivenByTeam;
				break;
			}

			if (!found) {
				found = ListAdd(&team->damageGivenList, sizeof(damageCounter_t));
				found->otherPlayerSessionId = thisDamageGivenByPlayer->otherPlayerSessionId;
				found->otherPlayerAccountId = thisDamageGivenByPlayer->otherPlayerAccountId;
				found->otherPlayerIsBot = thisDamageGivenByPlayer->otherPlayerIsBot;
			}

			found->totalAmount += thisDamageGivenByPlayer->totalAmount;
		}
	}
	else if (type == STATS_TABLE_WEAPON_GIVEN) {
		for (int i = 0; i < MODC_MAX; i++) {
			if (i == MODC_ALL_TYPES_COMBINED) {
				for (int i = MODC_FIRST; i < MODC_ALL_TYPES_COMBINED; i++) {
					int *dmg = GetDamageGivenStatOfType(weaponStatsPtr, player, i);
					if (dmg)
						team->damageOfType[i] += *dmg;
				}
			}
			else {
				int *dmg = GetDamageGivenStatOfType(weaponStatsPtr, player, i);
				if (dmg && *dmg > bestStats[player->lastTeam].damageOfType[i])
					team->damageOfType[i] += *dmg;
			}
		}
	}
	else if (type == STATS_TABLE_WEAPON_TAKEN) {
		for (int i = 0; i < MODC_MAX; i++) {
			if (i == MODC_ALL_TYPES_COMBINED) {
				for (int i = MODC_FIRST; i < MODC_ALL_TYPES_COMBINED; i++) {
					int *dmg = GetDamageTakenStatOfType(player, weaponStatsPtr, i);
					if (dmg)
						team->damageOfType[i] += *dmg;
				}
			}
			else {
				int *dmg = GetDamageTakenStatOfType(player, weaponStatsPtr, i);
				if (dmg && *dmg > bestStats[player->lastTeam].damageOfType[i])
					team->damageOfType[i] += *dmg;
			}
		}
	}
}

stats_t *GetStatsFromString(const char *str) {
	if (!VALIDSTRING(str))
		return NULL;

	qboolean isNum = Q_isanumber(str);
	int num = isNum ? atoi(str) : -1;

	if (isNum && num >= 0 && num < MAX_CLIENTS) {
		// try to match the client number of a connected player
		gentity_t *ent = &g_entities[num];
		if (ent->inuse && ent->client && ent->client->pers.connected != CON_DISCONNECTED && ent->client->stats && StatsValid(ent->client->stats) &&
			(ent->client->stats->lastTeam == TEAM_RED || ent->client->stats->lastTeam == TEAM_BLUE)) {
			return ent->client->stats;
		}

		// try to match the client number of a ragequitter
		iterator_t iter;
		ListIterate(&level.statsList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			stats_t *s = IteratorNext(&iter);
			if (s->clientNum == num && StatsValid(s) && (s->lastTeam == TEAM_RED || s->lastTeam == TEAM_BLUE))
				return s;
		}

		return NULL;
	}

	// look for exact matches on names and account names of online players
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (ent->inuse && ent->client && ent->client->pers.connected != CON_DISCONNECTED && ent->client->stats && StatsValid(ent->client->stats) &&
			(ent->client->stats->lastTeam == TEAM_RED || ent->client->stats->lastTeam == TEAM_BLUE)) {
			char *cleanPlayerName = strdup(ent->client->pers.netname[0] ? ent->client->pers.netname : "Padawan");
			Q_CleanStr(cleanPlayerName);
			char *cleanQuery = strdup(str);
			Q_CleanStr(cleanQuery);
			if (!Q_stricmp(cleanQuery, cleanPlayerName)) {
				free(cleanPlayerName);
				free(cleanQuery);
				return ent->client->stats; // matched this player name
			}
			free(cleanPlayerName);

			if (ent->client->account && ent->client->account->name[0]) {
				char *cleanAccountName = strdup(ent->client->account->name);
				Q_CleanStr(cleanAccountName);
				if (!Q_stricmp(cleanQuery, cleanAccountName)) {
					free(cleanAccountName);
					free(cleanQuery);
					return ent->client->stats; // matched this account name
				}
				free(cleanAccountName);
			}
			free(cleanQuery);
		}
	}

	// look for exact matches on names and account names of ragequitters
	iterator_t iter;
	ListIterate(&level.statsList, &iter, qfalse);

	while (IteratorHasNext(&iter)) {
		stats_t *s = IteratorNext(&iter);
		if (StatsValid(s) && (s->lastTeam == TEAM_RED || s->lastTeam == TEAM_BLUE)) {
			char *cleanPlayerName = strdup(s->name);
			Q_CleanStr(cleanPlayerName);
			char *cleanQuery = strdup(str);
			Q_CleanStr(cleanQuery);
			if (!Q_stricmp(cleanQuery, cleanPlayerName)) {
				free(cleanPlayerName);
				free(cleanQuery);
				return s; // matched this player name
			}
			free(cleanPlayerName);

			if (s->accountId != ACCOUNT_ID_UNLINKED && s->accountName[0]) {
				char *cleanAccountName = strdup(s->accountName);
				Q_CleanStr(cleanAccountName);
				if (!Q_stricmp(cleanQuery, cleanAccountName)) {
					free(cleanAccountName);
					free(cleanQuery);
					return s; // matched this account name
				}
				free(cleanAccountName);
			}
			free(cleanQuery);
		}
	}

	// look for partial names of ingame players
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (ent->inuse && ent->client && ent->client->pers.connected != CON_DISCONNECTED && ent->client->stats && StatsValid(ent->client->stats) &&
			(ent->client->stats->lastTeam == TEAM_RED || ent->client->stats->lastTeam == TEAM_BLUE)) {
			char *cleanPlayerName = strdup(ent->client->pers.netname[0] ? ent->client->pers.netname : "Padawan");
			Q_CleanStr(cleanPlayerName);
			char *cleanQuery = strdup(str);
			Q_CleanStr(cleanQuery);
			if (stristr(cleanPlayerName, cleanQuery)) {
				free(cleanPlayerName);
				free(cleanQuery);
				return ent->client->stats; // matched this player name
			}
			free(cleanPlayerName);

			if (ent->client->account && ent->client->account->name[0]) {
				char *cleanAccountName = strdup(ent->client->account->name);
				Q_CleanStr(cleanAccountName);
				if (stristr(cleanAccountName, cleanQuery)) {
					free(cleanAccountName);
					free(cleanQuery);
					return ent->client->stats; // matched this account name
				}
				free(cleanAccountName);
			}
			free(cleanQuery);
		}
	}

	// look for partial names of ragequitters
	ListIterate(&level.statsList, &iter, qfalse);

	while (IteratorHasNext(&iter)) {
		stats_t *s = IteratorNext(&iter);
		if (StatsValid(s) && (s->lastTeam == TEAM_RED || s->lastTeam == TEAM_BLUE)) {
			char *cleanPlayerName = strdup(s->name);
			Q_CleanStr(cleanPlayerName);
			char *cleanQuery = strdup(str);
			Q_CleanStr(cleanQuery);
			if (stristr(cleanPlayerName, cleanQuery)) {
				free(cleanPlayerName);
				free(cleanQuery);
				return s; // matched this player name
			}
			free(cleanPlayerName);

			if (s->accountId != ACCOUNT_ID_UNLINKED && s->accountName[0]) {
				char *cleanAccountName = strdup(s->accountName);
				Q_CleanStr(cleanAccountName);
				if (stristr(cleanAccountName, cleanQuery)) {
					free(cleanAccountName);
					free(cleanQuery);
					return s; // matched this account name
				}
				free(cleanAccountName);
			}
			free(cleanQuery);
		}
	}

	return NULL; // no match
}

const char *NameForMeansOfDeathCategory(meansOfDeathCategory_t modc) {
	switch (modc) {
	case MODC_MELEESTUNBATON: return "^5MEL";
	case MODC_SABER: return "^5SAB";
	case MODC_PISTOL: return "^5PIS";
	case MODC_BLASTER: return "^5BLA";
	case MODC_DISRUPTOR: return "^5DIS";
	case MODC_BOWCASTER: return "^5BOW";
	case MODC_REPEATER: return "^5REP";
	case MODC_DEMP: return "^5DMP";
	case MODC_GOLAN: return "^5GOL";
	case MODC_ROCKET: return "^5ROC";
	case MODC_CONCUSSION: return "^5CON";
	case MODC_THERMAL: return "^5THE";
	case MODC_MINE: return "^5MIN";
	case MODC_DETPACK: return "^5DPK";
	case MODC_FORCE: return "^5FOR";
	case MODC_ALL_TYPES_COMBINED: return "^5TOTAL";
	default: assert(qfalse); return NULL;
	}
}

static void PrintTeamStats(const int id, char *outputBuffer, size_t outSize, qboolean announce, statsTableType_t type, stats_t *weaponStatsPtr) {
	Table *t = Table_Initialize(qfalse);
	list_t gotPlayersList = { 0 };
	memset(&bestStats, 0, sizeof(bestStats));
	team_t winningTeam = level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED] ? TEAM_BLUE : TEAM_RED; // red if tied to match scoreboard order
	team_t losingTeam = OtherTeam(winningTeam);
	int numWinningTeam = 0, numLosingTeam = 0;
	qboolean didHorizontalRule = qfalse;
	stats_t winningTeamTotalStats = { 0 }, losingTeamTotalStats = { 0 };
	winningTeamTotalStats.isTotal = losingTeamTotalStats.isTotal = qtrue;
	winningTeamTotalStats.lastTeam = winningTeam;
	losingTeamTotalStats.lastTeam = losingTeam;
	stats_t *lastWinningTeamPlayer = NULL;

	// loop through level.statsList, first by connected clients only...
	for (int j = 0; j < 2; j++) {
		team_t team = !j ? winningTeam : losingTeam; // winning team first

		for (int i = 0; i < level.numConnectedClients; i++) {
			int findClientNum = level.sortedClients[i];
			iterator_t iter;
			ListIterate(&level.statsList, &iter, qfalse);

			while (IteratorHasNext(&iter)) {
				stats_t *found = IteratorNext(&iter);
				if (!StatsValid(found) || found->clientNum != findClientNum || found->lastTeam != team)
					continue;

				gotPlayer_t *gotPlayerAlready = ListFind(&gotPlayersList, PlayerMatches, found, NULL);
				if (gotPlayerAlready)
					continue;

				// this player will be in the stats table
				gotPlayer_t *add = ListAdd(&gotPlayersList, sizeof(gotPlayer_t));
				add->stats = found;
				add->sessionId = found->sessionId;
				add->isBot = found->isBot;

				if (team == winningTeam) {
					lastWinningTeamPlayer = found;
				}
				else if (team == losingTeam && numWinningTeam && !didHorizontalRule) { // there were winners and this is the first loser
					if (numWinningTeam > 1)
						Table_DefineRow(t, &winningTeamTotalStats);
					Table_AddHorizontalRule(t, losingTeam == TEAM_BLUE ? 4 : 1);
					didHorizontalRule = qtrue;
				}

				CheckBestStats(found, type, weaponStatsPtr);
				AddStatsToTotal(found, team == winningTeam ? &winningTeamTotalStats : &losingTeamTotalStats, type, weaponStatsPtr);
				Table_DefineRow(t, found);

				if (team == winningTeam)
					++numWinningTeam;
				else
					++numLosingTeam;

				goto continueLoop; // suck my
			}
			continueLoop:; // nuts
		}

		// ...then, everybody else (ragequitters, people who went spec, etc.)
		iterator_t iter;
		ListIterate(&level.statsList, &iter, qfalse);

		while (IteratorHasNext(&iter)) {
			stats_t *found = IteratorNext(&iter);
			if (!StatsValid(found) || found->lastTeam != team)
				continue;

			gotPlayer_t *gotPlayerAlready = ListFind(&gotPlayersList, PlayerMatches, found, NULL);
			if (gotPlayerAlready)
				continue;

			// this player will be in the stats table
			gotPlayer_t *add = ListAdd(&gotPlayersList, sizeof(gotPlayer_t));
			add->stats = found;
			add->sessionId = found->sessionId;
			add->isBot = found->isBot;

			if (team == winningTeam) {
				lastWinningTeamPlayer = found;
			}
			else if (team == losingTeam && numWinningTeam && !didHorizontalRule) { // there were winners and this is the first loser
				if (numWinningTeam > 1)
					Table_DefineRow(t, &winningTeamTotalStats);
				Table_AddHorizontalRule(t, losingTeam == TEAM_BLUE ? 4 : 1);
				didHorizontalRule = qtrue;
			}

			CheckBestStats(found, type, weaponStatsPtr);
			AddStatsToTotal(found, team == winningTeam ? &winningTeamTotalStats : &losingTeamTotalStats, type, weaponStatsPtr);
			Table_DefineRow(t, found);

			if (team == winningTeam)
				++numWinningTeam;
			else
				++numLosingTeam;
		}
	}

	if (numLosingTeam > 1)
		Table_DefineRow(t, &losingTeamTotalStats);

	if (!numWinningTeam && !numLosingTeam) {
		Table_Destroy(t);
		ListClear(&gotPlayersList);
		return;
	}

	int longestPrintLenName = 4 /*NAME*/, longestPrintLenAlias = 5 /*ALIAS*/;

	if (type == STATS_TABLE_GENERAL) {
		Table_DefineColumn(t, "^5NAME", CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5ALIAS", CtfStatsTableCallback_Alias, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5CAP", CtfStatsTableCallback_Captures, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5ASS", CtfStatsTableCallback_Assists, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5DEF", CtfStatsTableCallback_Defends, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5SAV", CtfStatsTableCallback_Saves, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5ACC", CtfStatsTableCallback_Accuracy, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5AIR", CtfStatsTableCallback_Airs, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5PITS", CtfStatsTableCallback_Pits, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5PITTED", CtfStatsTableCallback_Pitted, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5FCKIL", CtfStatsTableCallback_FcKills, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5RET", CtfStatsTableCallback_Rets, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5SK", CtfStatsTableCallback_Selfkills, NULL, qfalse, -1, 32);
		if (level.boonExists) // only show boon stat if boon is enabled and exists on this map
			Table_DefineColumn(t, "^5BOON", CtfStatsTableCallback_BoonPickups, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5TTLHOLD", CtfStatsTableCallback_TotalHold, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5MAXHOLD", CtfStatsTableCallback_MaxHold, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5DMGDLT", CtfStatsTableCallback_DamageDealt, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5DMGTKN", CtfStatsTableCallback_DamageTaken, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5TOPSPD", CtfStatsTableCallback_TopSpeed, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5AVGSPD", CtfStatsTableCallback_AverageSpeed, NULL, qfalse, -1, 32);
	}
	else if (type == STATS_TABLE_FORCE) {
		Table_DefineColumn(t, "^5NAME", CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5ALIAS", CtfStatsTableCallback_Alias, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5PUSH", CtfStatsTableCallback_Push, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5PULL", CtfStatsTableCallback_Pull, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5HEAL", CtfStatsTableCallback_Healed, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5NRG", CtfStatsTableCallback_EnergizedAlly, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5E-NRG", CtfStatsTableCallback_EnergizedEnemy, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5ABS", CtfStatsTableCallback_Absorbed, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5PROTDMG", CtfStatsTableCallback_ProtDamage, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5PROTTIME", CtfStatsTableCallback_ProtTime, NULL, qfalse, -1, 32);
	}
	else if (type == STATS_TABLE_MISC) {
		Table_DefineColumn(t, "^5NAME", CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5ALIAS", CtfStatsTableCallback_Alias, NULL, qtrue, -1, 32);
		ctfRegion_t region = CTFREGION_FLAGSTAND;
		Table_DefineColumn(t, "^5@FS", CtfStatsTableCallback_CtfRegionPercent, &region, qfalse, -1, 32);
		++region;
		Table_DefineColumn(t, "^5@BASE", CtfStatsTableCallback_CtfRegionPercent, &region, qfalse, -1, 32);
		++region;
		Table_DefineColumn(t, "^5@MID", CtfStatsTableCallback_CtfRegionPercent, &region, qfalse, -1, 32);
		++region;
		Table_DefineColumn(t, "^5@E-BASE", CtfStatsTableCallback_CtfRegionPercent, &region, qfalse, -1, 32);
		++region;
		Table_DefineColumn(t, "^5@E-FS", CtfStatsTableCallback_CtfRegionPercent, &region, qfalse, -1, 32);
	}
	else if (type == STATS_TABLE_DAMAGE) {
		int aliasDividerColor;
		if (numWinningTeam) {
			if (winningTeam == TEAM_RED)
				aliasDividerColor = 1;
			else
				aliasDividerColor = 4;
		}
		else {
			if (losingTeam == TEAM_RED)
				aliasDividerColor = 1;
			else
				aliasDividerColor = 4;
		}

		Table_DefineColumn(t, "^5NAME", CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5ALIAS", CtfStatsTableCallback_Alias, NULL, qtrue, aliasDividerColor, 32);

		iterator_t iter;
		ListIterate(&gotPlayersList, &iter, qfalse);

		while (IteratorHasNext(&iter)) {
			gotPlayer_t *player = IteratorNext(&iter);
			char *clean = strdup(player->stats->name);
			Q_CleanStr(clean);
			Q_strupr(clean);
			char *name = va("^5%s", clean);
			free(clean);
			int len = Q_PrintStrlen(name);
			if (len > longestPrintLenName)
				longestPrintLenName = len;
			if (VALIDSTRING(player->stats->accountName)) {
				len = Q_PrintStrlen(player->stats->accountName);
				if (len > longestPrintLenAlias)
					longestPrintLenAlias = len;
			}

			int dividerColor;
			if (numWinningTeam && numLosingTeam && player->stats == lastWinningTeamPlayer) {
				if (winningTeam == TEAM_RED)
					dividerColor = 4;
				else
					dividerColor = 1;
			}
			else {
				dividerColor = -1;
			}

			Table_DefineColumn(t, name, TableCallback_Damage, player->stats, qfalse, dividerColor, 32);
		}
	}
	else if (type == STATS_TABLE_WEAPON_GIVEN || type == STATS_TABLE_WEAPON_TAKEN) {
		Table_DefineColumn(t, "^5NAME", CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5ALIAS", CtfStatsTableCallback_Alias, NULL, qtrue, -1, 32);

		for (meansOfDeathCategory_t modc = MODC_FIRST; modc < MODC_MAX; modc++) {
			meansOfDeathCategoryContext_t context;
			context.tablePlayerStats = weaponStatsPtr;
			context.damageTaken = !!(type == STATS_TABLE_WEAPON_TAKEN);
			context.modc = modc;
			Table_DefineColumn(t, NameForMeansOfDeathCategory(modc), TableCallback_WeaponDamage, &context, qfalse, -1, 32);
		}
	}

	ListClear(&gotPlayersList);

#define TEMP_STATS_BUFFER_SIZE	(16384) // idk
	char *temp = malloc(TEMP_STATS_BUFFER_SIZE);
	size_t tempSize = TEMP_STATS_BUFFER_SIZE;
	memset(temp, 0, tempSize);
	if (type == STATS_TABLE_DAMAGE) {
		for (int i = 0; i < longestPrintLenName + 1/*`space`*/; i++)
			Q_strcat(temp, tempSize, " ");
		for (int i = 0; i < longestPrintLenAlias + 3/*`space + pipe + space`*/; i++)
			Q_strcat(temp, tempSize, " ");
		Q_strcat(temp, tempSize, "^5DAMAGE DEALT TO\n");
		int len = strlen(temp);
		Table_WriteToBuffer(t, temp + len, tempSize - len, qtrue, numWinningTeam ? (winningTeam == TEAM_BLUE ? 4 : 1) : (losingTeam == TEAM_BLUE ? 4 : 1));
	}
	else if (type == STATS_TABLE_WEAPON_GIVEN || type == STATS_TABLE_WEAPON_TAKEN) {
		Com_sprintf(temp, tempSize, "%s^7 by %s^7:\n", type == STATS_TABLE_WEAPON_GIVEN ? "^2DAMAGE DEALT" : "^8DAMAGE TAKEN", weaponStatsPtr->name);
		int len = strlen(temp);
		Table_WriteToBuffer(t, temp + len, tempSize - len, qtrue, numWinningTeam ? (winningTeam == TEAM_BLUE ? 4 : 1) : (losingTeam == TEAM_BLUE ? 4 : 1));
	}
	else {
		Table_WriteToBuffer(t, temp, tempSize, qtrue, numWinningTeam ? (winningTeam == TEAM_BLUE ? 4 : 1) : (losingTeam == TEAM_BLUE ? 4 : 1));
	}
	Table_Destroy(t);

	if (announce) PrintIngame(id, temp);
	if (outputBuffer) Q_strncpyz(outputBuffer, temp, outSize);

	free(temp);
}

void Stats_Print(gentity_t *ent, const char *type, char *outputBuffer, size_t outSize, qboolean announce, stats_t *weaponStatsPtr) {
	if (!VALIDSTRING(type))
		return;

	int id = ent ? (ent - g_entities) : -1;
	if (g_gametype.integer != GT_CTF) {
		if (id != -1) {
			trap_SendServerCommand(id, "print \""S_COLOR_WHITE"Gametype is not CTF. Statistics aren't generated.\n\"");
		}

		return;
	}

	if (!level.statsList.size) {
		if (id != -1)
			trap_SendServerCommand(id, "print \""S_COLOR_WHITE"Nobody is playing. Statistics aren't generated.\n\"");
		return;
	}

	if (!Q_stricmpn(type, "gen", 3)) { // general stats
		// for general stats, also print the score
		team_t winningTeam = level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED] ? TEAM_BLUE : TEAM_RED; // red if tied to match scoreboard order
		team_t losingTeam = OtherTeam(winningTeam);
		const char *s = va("%s: "S_COLOR_WHITE"%d    %s: "S_COLOR_WHITE"%d\n\n",
			ScoreTextForTeam(winningTeam), level.teamScores[winningTeam],
			ScoreTextForTeam(losingTeam), level.teamScores[losingTeam]);

		if (outputBuffer) {
			Q_strncpyz(outputBuffer, s, outSize);
			int len = strlen(outputBuffer);
			PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_GENERAL, NULL);
		}
		else {
			if (announce)
				PrintIngame(id, s);
			PrintTeamStats(id, NULL, 0, announce, STATS_TABLE_GENERAL, NULL);
		}
	}
	else if (!Q_stricmpn(type, "for", 3)) { // force power stats
		PrintTeamStats(id, outputBuffer, outSize, announce, STATS_TABLE_FORCE, NULL);
	}
	else if (!Q_stricmpn(type, "mis", 3)) { // miscellaneous stats
		PrintTeamStats(id, outputBuffer, outSize, announce, STATS_TABLE_MISC, NULL);
	}
	else if (!Q_stricmpn(type, "dam", 3)) { // damage stats
		PrintTeamStats(id, outputBuffer, outSize, announce, STATS_TABLE_DAMAGE, NULL);
		return;
	}
	else if (!Q_stricmpn(type, "wea", 3)) { // weapon stats
		if (weaponStatsPtr) { // a single client
			PrintTeamStats(id, outputBuffer, outSize, announce, STATS_TABLE_WEAPON_GIVEN, weaponStatsPtr);
			if (outputBuffer) {
				size_t len = strlen(outputBuffer);
				if (len < outSize - 1)
					PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_WEAPON_TAKEN, weaponStatsPtr);
			}
			else {
				PrintTeamStats(id, outputBuffer, outSize, announce, STATS_TABLE_WEAPON_TAKEN, weaponStatsPtr);
			}
		}
		else { // everyone
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (g_entities[i].inuse && level.clients[i].pers.connected == CON_CONNECTED &&
					(level.clients[i].sess.sessionTeam == TEAM_RED || level.clients[i].sess.sessionTeam == TEAM_BLUE) &&
					level.clients[i].stats && StatsValid(level.clients[i].stats)) {
					if (outputBuffer) {
						size_t len = strlen(outputBuffer);
						if (len < outSize - 1)
							PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_WEAPON_GIVEN, level.clients[i].stats);
					}
					else {
						PrintTeamStats(id, outputBuffer, outSize, announce, STATS_TABLE_WEAPON_GIVEN, level.clients[i].stats);
					}
					if (outputBuffer) {
						size_t len = strlen(outputBuffer);
						if (len < outSize - 1)
							PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_WEAPON_TAKEN, level.clients[i].stats);
					}
					else {
						PrintTeamStats(id, outputBuffer, outSize, announce, STATS_TABLE_WEAPON_TAKEN, level.clients[i].stats);
					}
				}
			}
		}
		return;
	}
	else {
		if (id != -1) {
			if (!Q_stricmp(type, "help"))
				trap_SendServerCommand(id, "print \""S_COLOR_WHITE"Usage: "S_COLOR_CYAN"/ctfstats <general | force | misc | damage | weapon [player]>\n\"");
			else
				trap_SendServerCommand(id, va("print \""S_COLOR_WHITE"Unknown type '%s"S_COLOR_WHITE"'. Usage: "S_COLOR_CYAN"/ctfstats <general | force | misc | damage | weapon [player]>\n\"", type));
		}

		return;
	}
}

static qboolean SessionIdMatchesStats(genericNode_t *node, void *userData) {
	stats_t *existing = (stats_t *)node;
	session_t *thisGuy = (session_t *)userData;

	if (!existing || !thisGuy)
		return qfalse;

	if (thisGuy->accountId != ACCOUNT_ID_UNLINKED && thisGuy->accountId == existing->accountId)
		return qtrue; // matches a linked account

	if (existing->sessionId == thisGuy->id)
		return qtrue; // matches a session

	return qfalse;
}

// sets the client->stats pointer to the stats_t we have allocated for a client
// this can be brand new or an existing one (e.g. if they left and reconnected during a match)
void InitClientStats(gclient_t *cl) {

	assert(cl);
	stats_t *stats = ListFind(&level.statsList, SessionIdMatchesStats, cl->session, NULL);

	if (!stats) { // not yet tracked; add him to the list
		stats = ListAdd(&level.statsList, sizeof(stats_t));
		stats->isBot = !!(g_entities[cl - level.clients].r.svFlags & SVF_BOT);
		stats->sessionId = stats->isBot ? cl - level.clients : cl->session->id;
		stats->clientNum = cl - level.clients;
		if (cl->account && VALIDSTRING(cl->account->name))
			Q_strncpyz(stats->accountName, cl->account->name, sizeof(stats->accountName));
		else
			stats->accountName[0] = '\0';
		Q_strncpyz(stats->name, cl->pers.netname, sizeof(stats->name));
	}

	if (!stats->isBot)
		stats->accountId = cl->session->accountId; // update the tracked account id in any case, in case an admin assigned him an account during this match
	if (cl->account && VALIDSTRING(cl->account->name))
		Q_strncpyz(stats->accountName, cl->account->name, sizeof(stats->accountName));
	else
		stats->accountName[0] = '\0';
	Q_strncpyz(stats->name, cl->pers.netname, sizeof(stats->name));

	cl->stats = stats;
}

int *GetDamageGivenStat(stats_t *attacker, stats_t *victim) {
	damageCounter_t *dmg = ListFind(&attacker->damageGivenList, MatchesDamage, victim, NULL);

	if (dmg) { // this guy is already tracked
		if (!dmg->otherPlayerIsBot)
			dmg->otherPlayerAccountId = victim->accountId; // update the tracked account id, in case an admin assigned him an account during this match
	}
	else { // not yet tracked; add him to the list
		dmg = ListAdd(&attacker->damageGivenList, sizeof(damageCounter_t));
		dmg->otherPlayerIsBot = victim->isBot;
		dmg->otherPlayerStats = victim;
		if (dmg->otherPlayerIsBot) {
			dmg->otherPlayerSessionId = victim->clientNum;
		}
		else {
			dmg->otherPlayerSessionId = victim->sessionId;
			dmg->otherPlayerAccountId = victim->accountId;
		}
	}

	return &dmg->totalAmount;
}

int *GetDamageGivenStatOfType(stats_t *attacker, stats_t *victim, meansOfDeathCategory_t modc) {
	damageCounter_t *dmg = ListFind(&attacker->damageGivenList, MatchesDamage, victim, NULL);

	if (dmg) { // this guy is already tracked
		if (!dmg->otherPlayerIsBot)
			dmg->otherPlayerAccountId = victim->accountId; // update the tracked account id, in case an admin assigned him an account during this match
	}
	else { // not yet tracked; add him to the list
		dmg = ListAdd(&attacker->damageGivenList, sizeof(damageCounter_t));
		dmg->otherPlayerIsBot = victim->isBot;
		dmg->otherPlayerStats = victim;
		if (dmg->otherPlayerIsBot) {
			dmg->otherPlayerSessionId = victim->clientNum;
		}
		else {
			dmg->otherPlayerSessionId = victim->sessionId;
			dmg->otherPlayerAccountId = victim->accountId;
		}
	}

	return &dmg->ofType[modc];
}

int *GetDamageTakenStat(stats_t *attacker, stats_t *victim) {
	damageCounter_t *dmg = ListFind(&victim->damageTakenList, MatchesDamage, attacker, NULL);

	if (dmg) { // this guy is already tracked
		if (!dmg->otherPlayerIsBot)
			dmg->otherPlayerAccountId = attacker->accountId; // update the tracked account id, in case an admin assigned him an account during this match
	}
	else { // not yet tracked; add him to the list
		dmg = ListAdd(&victim->damageTakenList, sizeof(damageCounter_t));
		dmg->otherPlayerIsBot = attacker->isBot;
		dmg->otherPlayerStats = attacker;
		if (dmg->otherPlayerIsBot) {
			dmg->otherPlayerSessionId = attacker->clientNum;
		}
		else {
			dmg->otherPlayerSessionId = attacker->sessionId;
			dmg->otherPlayerAccountId = attacker->accountId;
		}
	}

	return &dmg->totalAmount;
}

int *GetDamageTakenStatOfType(stats_t *attacker, stats_t *victim, meansOfDeathCategory_t modc) {
	damageCounter_t *dmg = ListFind(&victim->damageTakenList, MatchesDamage, attacker, NULL);

	if (dmg) { // this guy is already tracked
		if (!dmg->otherPlayerIsBot)
			dmg->otherPlayerAccountId = attacker->accountId; // update the tracked account id, in case an admin assigned him an account during this match
	}
	else { // not yet tracked; add him to the list
		dmg = ListAdd(&victim->damageTakenList, sizeof(damageCounter_t));
		dmg->otherPlayerIsBot = attacker->isBot;
		dmg->otherPlayerStats = attacker;
		if (dmg->otherPlayerIsBot) {
			dmg->otherPlayerSessionId = attacker->clientNum;
		}
		else {
			dmg->otherPlayerSessionId = attacker->sessionId;
			dmg->otherPlayerAccountId = attacker->accountId;
		}
	}

	return &dmg->ofType[modc];
}

extern qboolean isRedFlagstand(gentity_t *ent);
extern qboolean isBlueFlagstand(gentity_t *ent);
ctfRegion_t GetCTFRegion(gentity_t *ent) {
	if (g_gametype.integer != GT_CTF || !ent || (ent->client->sess.sessionTeam != TEAM_RED && ent->client->sess.sessionTeam != TEAM_BLUE))
		return CTFREGION_INVALID;

	static gentity_t *redFs = NULL, *blueFs = NULL;
	static float diffBetweenFlags = 0.0f;

	static qboolean initialized = qfalse;
	if (!initialized) {
		gentity_t temp;
		VectorCopy(vec3_origin, temp.r.currentOrigin);
		redFs = G_ClosestEntity(&temp, isRedFlagstand);
		blueFs = G_ClosestEntity(&temp, isBlueFlagstand);
		diffBetweenFlags = Distance2D(redFs->r.currentOrigin, blueFs->r.currentOrigin);
		initialized = qtrue;
	}

	if (!redFs || !blueFs)
		return CTFREGION_INVALID;

	team_t team = ent->client->sess.sessionTeam;

	float allyDist = Distance2D(ent->r.currentOrigin, team == TEAM_RED ? redFs->r.currentOrigin : blueFs->r.currentOrigin);
	float enemyDist = Distance2D(ent->r.currentOrigin, team == TEAM_RED ? blueFs->r.currentOrigin : redFs->r.currentOrigin);
	//PrintIngame(ent - g_entities, "^2Ally^7 FS dist: %d - ^8Enemy^7 FS dist: %d - Ratio: %.2f\n", (int)allyDist, (int)enemyDist, allyDist / enemyDist);

	if (allyDist == enemyDist)
		return CTFREGION_MID;

	if (allyDist < enemyDist) {
		float diff = allyDist / enemyDist;
		if (enemyDist > diffBetweenFlags || diff <= 0.2f)
			return CTFREGION_FLAGSTAND;
		if (diff <= 0.4f)
			return CTFREGION_BASE;
		return CTFREGION_MID;
	}
	else {
		float diff = enemyDist / allyDist;
		if (allyDist > diffBetweenFlags || diff <= 0.2f)
			return CTFREGION_ENEMYFLAGSTAND;
		if (diff <= 0.4f)
			return CTFREGION_ENEMYBASE;
		return CTFREGION_MID;
	}
}