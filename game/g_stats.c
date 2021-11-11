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
const char *FormatStatInt(qboolean isTotal, int num, int bestMyTeam, int bestOtherTeam, int minDigitsDebug) {
#if defined (_DEBUG) && defined(DEBUGSTATSNAMES)
	if (isTotal)
		return va("^9%0*d", minDigitsDebug, num);
	if (!num && !bestMyTeam)
		return va("%0*d", minDigitsDebug, num); // everybody has zero on my team; no color
	if (num >= bestMyTeam && num >= bestOtherTeam)
		return va("^3%0*d", minDigitsDebug, num);
	if (num >= bestMyTeam)
		return va("^2%0*d", minDigitsDebug, num);
	return va("%0*d", minDigitsDebug, num);
#else
	if (isTotal)
		return va("^9%d", num);
	if (!num && !bestMyTeam)
		return va("%d", num); // everybody has zero on my team; no color
	if (num >= bestMyTeam && num >= bestOtherTeam)
		return va("^3%d", num);
	if (num >= bestMyTeam)
		return va("^2%d", num);
	return va("%d", num);
#endif
}

// gold if best overall; green if best on team; uncolored otherwise
const char *FormatStatTime(qboolean isTotal, int num, int bestMyTeam, int bestOtherTeam) {
	int secs = num / 1000;
	int mins = secs / 60;

#if defined (_DEBUG) && defined(DEBUGSTATSNAMES)
#if 1
	secs %= 60;
	if (isTotal)
		return va("^9%02dm%02ds", mins, secs);
	if (!num && !bestMyTeam)
		return va("%02dm%02ds", mins, secs);  // everybody has zero on my team; no color
	if (num >= bestMyTeam && num >= bestOtherTeam)
		return va("^3%02dm%02ds", mins, secs);
	if (num >= bestMyTeam)
		return va("^2%02dm%02ds", mins, secs);
	return va("%02dm%02ds", mins, secs);
#else
	secs %= 60;
	if (isTotal)
		return va("^9%02d:%02ds", mins, secs);
	if (!num && !bestMyTeam)
		return va("%02d:%02d", mins, secs);  // everybody has zero on my team; no color
	if (num >= bestMyTeam && num >= bestOtherTeam)
		return va("^3%02d:%02d", mins, secs);
	if (num >= bestMyTeam)
		return va("^2%02d:%02d", mins, secs);
	return va("%02d:%02ds", mins, secs);
#endif
#else
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
#endif
}

stats_t bestStats[TEAM_NUM_TEAMS] = { 0 }; // e.g. bestStats[TEAM_RED].captures contains the best captures of anyone that was on red team

typedef struct {
	float average;
	int numberOfSamples;
	ctfPosition_t forcePos;
	int energizes;
	int gets;
	ctfPosition_t lastPosition;
	int idNum;
	stats_t *stats;
} ctfPositioningDataQuick_t;

int ComparePositioningDataAverages(const void *a, const void *b) {
	ctfPositioningDataQuick_t *aa = (ctfPositioningDataQuick_t *)a;
	ctfPositioningDataQuick_t *bb = (ctfPositioningDataQuick_t *)b;
	return (aa->average > bb->average) - (aa->average < bb->average);
}

int MoveAfksToEnd(const void *a, const void *b) {
	ctfPositioningDataQuick_t *aa = (ctfPositioningDataQuick_t *)a;
	ctfPositioningDataQuick_t *bb = (ctfPositioningDataQuick_t *)b;
	if (aa->numberOfSamples < bb->numberOfSamples)
		return 1;
	else if (aa->numberOfSamples > bb->numberOfSamples)
		return -1;
	return 0;
}

char *NameForPos(ctfPosition_t pos) {
	switch (pos) {
	case CTFPOSITION_BASE: return "base";
	case CTFPOSITION_CHASE: return "chase";
	case CTFPOSITION_OFFENSE: return "offense";
	default: return "unknown position";
	}
}

#ifdef DEBUG_CTF_POSITION_STATS
#define DebugCtfPosPrintf(...)	Com_Printf(__VA_ARGS__)
#else
#define DebugCtfPosPrintf(...)	do {} while (0)
#endif

// determine 4v4 ctf position based on average location
// we track your position relative to the flagstands:
// 0 == on top of your fs
// 1 == on top of the enemy fs
// additionally, we only track:
// --a few seconds after spawning (to remove bias toward spawnpoints)
// --while you are NOT holding the flag
// this seems to reliably result in distributions where the two offense players have the top values,
// the base player has the lowest value, and the chase player is somewhere in the middle
// this function can be called on any stats_t at any time, including old ones
ctfPosition_t DetermineCTFPosition(stats_t *posGuy) {
	if (!posGuy) {
		assert(qfalse);
		return CTFPOSITION_UNKNOWN;
	}

	// once finalPosition is set, it is always the position for this stats_t
	if (posGuy->finalPosition) {
		DebugCtfPosPrintf("%08x cl %d %s^7 (block %d): ^5using finalPosition %s^7\n", posGuy, posGuy->clientNum, posGuy->name, posGuy->blockNum, NameForPos(posGuy->finalPosition));
		return posGuy->finalPosition;
	}

	if (!level.wasRestarted || level.someoneWasAFK ||
		(level.time - level.startTime) < (CTFPOSITION_MINIMUM_SECONDS * 1000) || !StatsValid(posGuy) ||
		posGuy->lastTeam == TEAM_SPECTATOR || posGuy->lastTeam == TEAM_FREE) {
		DebugCtfPosPrintf("%08x cl %d %s^7 (block %d): restart/afk/< 60/invalid/spec/free, so using lastPosition %s\n", posGuy, posGuy->clientNum, posGuy->name, posGuy->blockNum, NameForPos(posGuy->lastPosition));
		return posGuy->lastPosition;
	}

	// we only care about 4v4
	float avgRed = (float)level.numRedPlayerTicks / (float)level.numTeamTicks;
	float avgBlue = (float)level.numBluePlayerTicks / (float)level.numTeamTicks;
	int avgRedInt = (int)lroundf(avgRed);
	int avgBlueInt = (int)lroundf(avgBlue);
	if (avgRedInt != 4 || avgBlueInt != 4) {
		DebugCtfPosPrintf("%08x cl %d %s^7 (block %d): not 4v4, so using lastPosition %s\n", posGuy, posGuy->clientNum, posGuy->name, posGuy->blockNum, NameForPos(posGuy->lastPosition));
		return posGuy->lastPosition;
	}

	// if i haven't been in the current block for 60+ seconds, reuse my last position
	if (posGuy->ticksNotPaused < g_svfps.integer * CTFPOSITION_MINIMUM_SECONDS) {
		DebugCtfPosPrintf("%08x cl %d %s^7 (block %d): < 60 current block, so using lastPosition %s\n", posGuy, posGuy->clientNum, posGuy->name, posGuy->blockNum, NameForPos(posGuy->lastPosition));
		return posGuy->lastPosition;
	}

	// if i have no position samples, just reuse my last position
	if (!posGuy->numPositionSamplesAnyFlag) {
		DebugCtfPosPrintf("%08x cl %d %s^7 (block %d): no positionSamplesAnyFlag, so using lastPosition %s\n", posGuy, posGuy->clientNum, posGuy->name, posGuy->blockNum, NameForPos(posGuy->lastPosition));
		return posGuy->lastPosition;
	}

	// if i have held the flag for this entire block, just reuse my last position
	if (!posGuy->numPositionSamplesWithoutFlag) {
		DebugCtfPosPrintf("%08x cl %d %s^7 (block %d): no positionSamplesWithoutFlag, so using lastPosition %s\n", posGuy, posGuy->clientNum, posGuy->name, posGuy->blockNum, NameForPos(posGuy->lastPosition));
		return posGuy->lastPosition;
	}

	float posGuyAverage = posGuy->totalPositionWithoutFlag / (float)posGuy->numPositionSamplesWithoutFlag;

	// figure out how many people i was ingame with for 60+ seconds while they weren't afk
	iterator_t iter;
	ListIterate(&posGuy->teammatePositioningList, &iter, qfalse);
	int numTeammates = 0;
	while (IteratorHasNext(&iter)) {
		ctfPositioningData_t *teammate = IteratorNext(&iter);
		if (!StatsValid(teammate->stats))
			continue;
		if (teammate->numTicksIngameWithMe < g_svfps.integer * CTFPOSITION_MINIMUM_SECONDS)
			continue; // this guy hasn't been ingame with me during the current block for 60+ seconds.
		if (!teammate->numPositionSamplesIngameWithMe && !teammate->stats->lastPosition)
			continue; // no valid position samples and has no last position

		++numTeammates;
	}

	// require at least three teammates
	if (numTeammates < 3) {
		DebugCtfPosPrintf("%08x cl %d %s^7 (block %d): < 3 teammates, so using lastPosition %s\n", posGuy, posGuy->clientNum, posGuy->name, posGuy->blockNum, NameForPos(posGuy->lastPosition));
		return posGuy->lastPosition;
	}

	// add our own guy to the array
	ctfPositioningDataQuick_t *data = calloc(numTeammates + 1, sizeof(ctfPositioningDataQuick_t));
	data->average = posGuyAverage;
	data->numberOfSamples = INT_MAX;
	data->energizes = posGuy->numEnergizes;
	data->lastPosition = posGuy->lastPosition;
	data->idNum = 0;
	data->stats = posGuy;
	int index = 1;

	// add everyone else to the array
	ListIterate(&posGuy->teammatePositioningList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		ctfPositioningData_t *teammate = IteratorNext(&iter);
		if (!StatsValid(teammate->stats))
			continue;
		if (teammate->numTicksIngameWithMe < g_svfps.integer * CTFPOSITION_MINIMUM_SECONDS) {
			continue; // this guy hasn't been ingame with me during the current block for 60+ seconds.
		}
		if (!teammate->numPositionSamplesIngameWithMe && !teammate->stats->lastPosition)
			continue; // no valid position samples and has no last position

		ctfPositioningDataQuick_t *thisGuyData = data + index++;
		thisGuyData->numberOfSamples = teammate->numPositionSamplesIngameWithMe;
		thisGuyData->energizes = teammate->stats->numEnergizes;
		thisGuyData->gets = teammate->stats->numGets;
		thisGuyData->lastPosition = teammate->stats->lastPosition;
		thisGuyData->idNum = index;
		thisGuyData->stats = teammate->stats;

		if (!teammate->numPositionSamplesWithoutFlagWithMe) {
			// this guy has been ingame with me for 60+ seconds but has been holding the flag the entire time for this block
			// force him to use his old position
			thisGuyData->forcePos = teammate->stats->lastPosition;
			DebugCtfPosPrintf("TEAMMATE %08x cl %d %s^7 (block %d): has no position samples without flag with me, so forcing last position %s\n", teammate->stats, teammate->stats->clientNum, teammate->stats->name, teammate->stats->blockNum, NameForPos(teammate->stats->lastPosition));
		}
		else {
			float average = teammate->totalPositionWithoutFlagWithMe / (float)teammate->numPositionSamplesWithoutFlagWithMe;
			thisGuyData->average = average;
		}
	}

	// if more than three teammates, trim the array to three based on who we played with the most
	if (numTeammates > 3) {
		qsort(data, numTeammates + 1, sizeof(ctfPositioningDataQuick_t), MoveAfksToEnd);
		data = realloc(data, 4 * sizeof(ctfPositioningDataQuick_t));
	}

	// sort the array such that people who stayed close to their flagstand are lower
	qsort(data, 4, sizeof(ctfPositioningDataQuick_t), ComparePositioningDataAverages);

	// see if anyone needs to be forced to a particular position
	qboolean resort = qfalse;
	for (int i = 0; i < 4; i++) {
		ctfPositioningDataQuick_t *d = data + i;
		if (!d->forcePos)
			continue;
		if (!i && d->forcePos == CTFPOSITION_BASE ||
			i == 1 && d->forcePos == CTFPOSITION_CHASE ||
			(i == 2 || i == 3) && d->forcePos == CTFPOSITION_OFFENSE)
			continue; // he's already the pos he needs to be

		if (d->forcePos == CTFPOSITION_BASE)
			d->average = -999;
		else if (d->forcePos == CTFPOSITION_CHASE)
			d->average = (data->average + (data + 1)->average) / 2.0f;
		else if (d->forcePos == CTFPOSITION_OFFENSE)
			d->average = 999;
		
		qsort(data, 4, sizeof(ctfPositioningDataQuick_t), ComparePositioningDataAverages);
		break; // there should only be at most one forced guy
	}

	ctfPositioningDataQuick_t *base = data + 0;
	ctfPositioningDataQuick_t *chase = data + 1;
	ctfPositioningDataQuick_t *offense1 = data + 2;
	ctfPositioningDataQuick_t *offense2 = data + 3;

	DebugCtfPosPrintf("base = %s^7 (%0.3f), chase = %s^7 (%0.3f), offense1 = %s^7 (%0.3f), offense2 = %s^7 (%0.3f)\n", base->stats->name, base->average, chase->stats->name, chase->average, offense1->stats->name, offense1->average, offense2->stats->name, offense2->average);

	// edge case: chase and base average positions are extremely close together
	if (chase->average - base->average < 0.03f) {
		DebugCtfPosPrintf("chase and base average positions are extremely close together\n");
		if (chase->lastPosition == CTFPOSITION_BASE && base->lastPosition == CTFPOSITION_CHASE &&
			!chase->forcePos && !base->forcePos) {
			// the supposed chase was confirmed base last block and the supposed base was confirmed chase last block; just reuse those positions
			chase = data + 0;
			base = data + 1;
			DebugCtfPosPrintf("the supposed chase was confirmed base last block and the supposed base was confirmed chase last block; just reuse those positions\n");
		}
		else if (!chase->lastPosition && !base->lastPosition && chase->energizes > base->energizes &&
			!chase->forcePos && !base->forcePos) {
			// they don't have confirmed last positions; just go with the person who has more energizes
			chase = data + 0;
			base = data + 1;
			DebugCtfPosPrintf("chase has more energizes, swapping them\n");
		}
	}

	// edge case: chase and offense average positions are extremely close together
	if (offense1->average - chase->average < 0.03f) {
		DebugCtfPosPrintf("chase and offense1 average positions are extremely close together\n");
		if (offense2->average - chase->average < 0.03f) { // also include the other offense player
			DebugCtfPosPrintf("chase and offense2 average positions are also extremely close together\n");
			if (chase->lastPosition == CTFPOSITION_OFFENSE && offense1->lastPosition == CTFPOSITION_CHASE &&
				!chase->forcePos && !offense1->forcePos) {
				// the supposed chase was confirmed offense last block and the supposed offense1 was confirmed chase last block; just reuse those positions
				chase = data + 2;
				offense1 = data + 1;
				DebugCtfPosPrintf("the supposed chase was confirmed offense last block and the supposed offense1 was confirmed chase last block; just reuse those positions\n");
			}
			else if (chase->lastPosition == CTFPOSITION_OFFENSE && offense2->lastPosition == CTFPOSITION_CHASE &&
				!chase->forcePos && !offense2->forcePos) {
				// the supposed chase was confirmed offense last block and the supposed offense2 was confirmed chase last block; just reuse those positions
				chase = data + 3;
				offense2 = data + 1;
				DebugCtfPosPrintf("the supposed chase was confirmed offense last block and the supposed offense2 was confirmed chase last block; just reuse those positions\n");
			}
			else {
				// see who has the lowest number of gets
				int lowestGets = 999999, lowestGetsIndex = -1;
				for (int i = 3; i > 0; i--) {
					if ((data + i)->gets <= lowestGets) {
						lowestGets = (data + i)->gets;
						lowestGetsIndex = i;
					}
				}

				if (lowestGetsIndex == 2 && !chase->forcePos && !offense1->forcePos) {
					// the supposed offense1 has fewer gets than the supposed chase; determine that the supposed offense1 is actually the chase
					chase = data + 2;
					offense1 = data + 1;
					DebugCtfPosPrintf("the supposed offense1 has fewer gets than the supposed chase; determine that the supposed offense1 is actually the chase\n");
				}
				else if (lowestGetsIndex == 3 && !chase->forcePos && !offense2->forcePos) {
					// the supposed offense2 has fewer gets than the supposed chase; determine that the supposed offense2 is actually the chase
					chase = data + 3;
					offense2 = data + 1;
					DebugCtfPosPrintf("the supposed offense2 has fewer gets than the supposed chase; determine that the supposed offense2 is actually the chase\n");
				}
			}
		}
		else { // just deal with these two players
			if (chase->lastPosition == CTFPOSITION_OFFENSE && offense1->lastPosition == CTFPOSITION_CHASE &&
				!chase->forcePos && !offense1->forcePos) {
				// the supposed chase was confirmed offense last block and the supposed offense1 was confirmed chase last block; just reuse those positions
				chase = data + 2;
				offense1 = data + 1;
				DebugCtfPosPrintf("the supposed chase was confirmed offense last block and the supposed offense1 was confirmed chase last block; just reuse those positions\n");
			}
			else if (offense1->gets < chase->gets && !chase->forcePos && !offense1->forcePos) {
				// the supposed offense1 has fewer gets than the supposed chase; determine that the supposed offense1 is actually the chase
				chase = data + 2;
				offense1 = data + 1;
				DebugCtfPosPrintf("the supposed offense1 has fewer gets than the supposed chase; determine that the supposed offense1 is actually the chase\n");
			}
		}
	}

	// our guy's pos is the one with the id number of zero
	ctfPosition_t pos;
	if (!base->idNum)
		pos = CTFPOSITION_BASE;
	else if (!chase->idNum)
		pos = CTFPOSITION_CHASE;
	else
		pos = CTFPOSITION_OFFENSE;

	free(data);
	DebugCtfPosPrintf("%08x cl %d %s^7 (block %d): ^2determined %s^7\n", posGuy, posGuy->clientNum, posGuy->name, posGuy->blockNum, NameForPos(pos));
	return pos;
}

const char *GetPositionStringForStats(stats_t *stats) {
	if (!stats)
		return NULL;

	if (stats->isTotal)
		return NULL;

	ctfPosition_t pos = stats->finalPosition ? stats->finalPosition : DetermineCTFPosition(stats);
	if (!pos)
		return NULL; // no position

	char buf[MAX_STRING_CHARS] = { 0 };
	switch (pos) {
	case CTFPOSITION_BASE: Q_strncpyz(buf, "Bas", sizeof(buf)); break;
	case CTFPOSITION_CHASE: Q_strncpyz(buf, "Cha", sizeof(buf)); break;
	case CTFPOSITION_OFFENSE: Q_strncpyz(buf, "Off", sizeof(buf)); break;
	}

	qboolean gotOldPos = qfalse;
	for (ctfPosition_t p = CTFPOSITION_BASE; p <= CTFPOSITION_OFFENSE; p++) {
		char *thisPosStr;
		switch (p) {
		case CTFPOSITION_BASE: thisPosStr = "Bas"; break;
		case CTFPOSITION_CHASE: thisPosStr = "Cha"; break;
		case CTFPOSITION_OFFENSE: thisPosStr = "Off"; break;
		}

		if (pos != p && (stats->confirmedPositionBits & (1 << p))) {
			if (gotOldPos)
				Q_strcat(buf, sizeof(buf), va("/%s", thisPosStr));
			else
				Q_strcat(buf, sizeof(buf), va(" ^9(%s", thisPosStr));
			gotOldPos = qtrue;
		}
	}
	if (gotOldPos)
		Q_strcat(buf, sizeof(buf), ")");

	return va("%s", buf);
}

const char *CtfStatsTableCallback_Position(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return GetPositionStringForStats(stats);
}

const char *CtfStatsTableCallback_Time(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	if (stats->isTotal)
		return NULL;
	int ms = stats->ticksNotPaused * (1000 / g_svfps.integer);
	int secs = ms / 1000;
	int mins = secs / 60;
	if (ms >= 60000)
		return va("%d:%02d", mins, secs % 60);
	else
		return va("0:%02d", secs);
}

const char *CtfStatsTableCallback_Captures(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->captures, bestStats[stats->lastTeam].captures, bestStats[OtherTeam(stats->lastTeam)].captures, 2);
}

const char *CtfStatsTableCallback_Assists(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->assists, bestStats[stats->lastTeam].assists, bestStats[OtherTeam(stats->lastTeam)].assists, 2);
}

const char *CtfStatsTableCallback_Defends(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->defends, bestStats[stats->lastTeam].defends, bestStats[OtherTeam(stats->lastTeam)].defends, 2);
}

const char *CtfStatsTableCallback_Accuracy(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->accuracy, bestStats[stats->lastTeam].accuracy, bestStats[OtherTeam(stats->lastTeam)].accuracy, 3);
}

const char *CtfStatsTableCallback_Airs(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->airs, bestStats[stats->lastTeam].airs, bestStats[OtherTeam(stats->lastTeam)].airs, 2);
}

const char *CtfStatsTableCallback_TeamKills(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->teamKills, bestStats[stats->lastTeam].teamKills, bestStats[OtherTeam(stats->lastTeam)].teamKills, 2);
}

const char *CtfStatsTableCallback_Takes(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->takes, bestStats[stats->lastTeam].takes, bestStats[OtherTeam(stats->lastTeam)].takes, 2);
}

const char *CtfStatsTableCallback_Pits(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->pits, bestStats[stats->lastTeam].pits, bestStats[OtherTeam(stats->lastTeam)].pits, 2);
}

const char *CtfStatsTableCallback_Pitted(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->pitted, bestStats[stats->lastTeam].pitted, bestStats[OtherTeam(stats->lastTeam)].pitted, 2);
}

const char *CtfStatsTableCallback_FcKills(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->fcKills, bestStats[stats->lastTeam].fcKills, bestStats[OtherTeam(stats->lastTeam)].fcKills, 2);
}

const char *CtfStatsTableCallback_Rets(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->rets, bestStats[stats->lastTeam].rets, bestStats[OtherTeam(stats->lastTeam)].rets, 2);
}

const char *CtfStatsTableCallback_Selfkills(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->selfkills, bestStats[stats->lastTeam].selfkills, bestStats[OtherTeam(stats->lastTeam)].selfkills, 2);
}

const char *CtfStatsTableCallback_BoonPickups(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->boonPickups, bestStats[stats->lastTeam].boonPickups, bestStats[OtherTeam(stats->lastTeam)].boonPickups, 2);
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

const char *CtfStatsTableCallback_Holds(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->numFlagHolds, bestStats[stats->lastTeam].numFlagHolds, bestStats[OtherTeam(stats->lastTeam)].numFlagHolds, 2);
}

const char *CtfStatsTableCallback_AverageHold(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatTime(stats->isTotal, stats->averageFlagHold, bestStats[stats->lastTeam].averageFlagHold, bestStats[OtherTeam(stats->lastTeam)].averageFlagHold);
}

const char *CtfStatsTableCallback_Saves(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->saves, bestStats[stats->lastTeam].saves, bestStats[OtherTeam(stats->lastTeam)].saves, 2);
}

const char *CtfStatsTableCallback_DamageDealt(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->damageDealtTotal, bestStats[stats->lastTeam].damageDealtTotal, bestStats[OtherTeam(stats->lastTeam)].damageDealtTotal, 5);
}

const char *CtfStatsTableCallback_DamageTaken(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->damageTakenTotal, bestStats[stats->lastTeam].damageTakenTotal, bestStats[OtherTeam(stats->lastTeam)].damageTakenTotal, 5);
}

const char *CtfStatsTableCallback_FlagCarrierDamageDealt(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->flagCarrierDamageDealtTotal, bestStats[stats->lastTeam].flagCarrierDamageDealtTotal, bestStats[OtherTeam(stats->lastTeam)].flagCarrierDamageDealtTotal, 4);
}

const char *CtfStatsTableCallback_FlagCarrierDamageTaken(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->flagCarrierDamageTakenTotal, bestStats[stats->lastTeam].flagCarrierDamageTakenTotal, bestStats[OtherTeam(stats->lastTeam)].flagCarrierDamageTakenTotal, 4);
}

const char *CtfStatsTableCallback_ClearDamageDealt(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->clearDamageDealtTotal, bestStats[stats->lastTeam].clearDamageDealtTotal, bestStats[OtherTeam(stats->lastTeam)].clearDamageDealtTotal, 4);
}

const char *CtfStatsTableCallback_ClearDamageTaken(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->clearDamageTakenTotal, bestStats[stats->lastTeam].clearDamageTakenTotal, bestStats[OtherTeam(stats->lastTeam)].clearDamageTakenTotal, 4);
}

const char *CtfStatsTableCallback_OtherDamageDealt(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->otherDamageDealtTotal, bestStats[stats->lastTeam].otherDamageDealtTotal, bestStats[OtherTeam(stats->lastTeam)].otherDamageDealtTotal, 4);
}

const char *CtfStatsTableCallback_OtherDamageTaken(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->otherDamageTakenTotal, bestStats[stats->lastTeam].otherDamageTakenTotal, bestStats[OtherTeam(stats->lastTeam)].otherDamageTakenTotal, 4);
}

const char *CtfStatsTableCallback_TopSpeed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, (int)(stats->topSpeed + 0.5f), (int)(bestStats[stats->lastTeam].topSpeed + 0.5f), (int)(bestStats[OtherTeam(stats->lastTeam)].topSpeed + 0.5f), 4);
}

const char *CtfStatsTableCallback_AverageSpeed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->averageSpeed, bestStats[stats->lastTeam].averageSpeed, bestStats[OtherTeam(stats->lastTeam)].averageSpeed, 4);
}

const char *CtfStatsTableCallback_Push(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->push, bestStats[stats->lastTeam].push, bestStats[OtherTeam(stats->lastTeam)].push, 3);
}

const char *CtfStatsTableCallback_Pull(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->pull, bestStats[stats->lastTeam].pull, bestStats[OtherTeam(stats->lastTeam)].pull, 3);
}

const char *CtfStatsTableCallback_Healed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->healed, bestStats[stats->lastTeam].healed, bestStats[OtherTeam(stats->lastTeam)].healed, 3);
}

const char *CtfStatsTableCallback_EnergizedAlly(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->energizedAlly, bestStats[stats->lastTeam].energizedAlly, bestStats[OtherTeam(stats->lastTeam)].energizedAlly, 5);
}

const char *CtfStatsTableCallback_EnergizeEfficiency(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	if (!stats->numEnergizes)
		return " ";
	return FormatStatInt(stats->isTotal, stats->energizeEfficiency, bestStats[stats->lastTeam].energizeEfficiency, bestStats[OtherTeam(stats->lastTeam)].energizeEfficiency, 3);
}

const char *CtfStatsTableCallback_EnergizedEnemy(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->energizedEnemy, bestStats[stats->lastTeam].energizedEnemy, bestStats[OtherTeam(stats->lastTeam)].energizedEnemy, 4);
}

const char *CtfStatsTableCallback_Absorbed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->absorbed, bestStats[stats->lastTeam].absorbed, bestStats[OtherTeam(stats->lastTeam)].absorbed, 4);
}

const char *CtfStatsTableCallback_ProtDamage(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->protDamageAvoided, bestStats[stats->lastTeam].protDamageAvoided, bestStats[OtherTeam(stats->lastTeam)].protDamageAvoided, 4);
}

const char *CtfStatsTableCallback_ProtTime(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatTime(stats->isTotal, stats->protTimeUsed, bestStats[stats->lastTeam].protTimeUsed, bestStats[OtherTeam(stats->lastTeam)].protTimeUsed);
}

const char *CtfStatsTableCallback_RageTime(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatTime(stats->isTotal, stats->rageTimeUsed, bestStats[stats->lastTeam].rageTimeUsed, bestStats[OtherTeam(stats->lastTeam)].rageTimeUsed);
}

const char *CtfStatsTableCallback_Drain(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->drain, bestStats[stats->lastTeam].drain, bestStats[OtherTeam(stats->lastTeam)].drain, 4);
}

const char *CtfStatsTableCallback_GotDrained(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->gotDrained, bestStats[stats->lastTeam].gotDrained, bestStats[OtherTeam(stats->lastTeam)].gotDrained, 4);
}

const char *CtfStatsTableCallback_HealthPickedUp(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->healthPickedUp, bestStats[stats->lastTeam].healthPickedUp, bestStats[OtherTeam(stats->lastTeam)].healthPickedUp, 4);
}

const char *CtfStatsTableCallback_ArmorPickedUp(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->isTotal, stats->armorPickedUp, bestStats[stats->lastTeam].armorPickedUp, bestStats[OtherTeam(stats->lastTeam)].armorPickedUp, 4);
}

const char *CtfStatsTableCallback_FlagCarrierKillEfficiency(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	if (!stats->fcKills)
		return " ";
	return FormatStatInt(stats->isTotal, stats->fcKillEfficiency, bestStats[stats->lastTeam].fcKillEfficiency, bestStats[OtherTeam(stats->lastTeam)].fcKillEfficiency, 3);
}

const char *CtfStatsTableCallback_GetHealth(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	if (!stats->numGets)
		return " ";
	return FormatStatInt(stats->isTotal, stats->averageGetHealth, bestStats[stats->lastTeam].averageGetHealth, bestStats[OtherTeam(stats->lastTeam)].averageGetHealth, 3);
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
	return FormatStatInt(stats->isTotal, stats->regionPercent[region], bestStats[stats->lastTeam].regionPercent[region], bestStats[OtherTeam(stats->lastTeam)].regionPercent[region], 3);
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
		return FormatStatInt(qtrue, damage ? damage->totalAmount : 0, 0, 0, 4);

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

	return FormatStatInt(attacker->isTotal, damage ? damage->totalAmount : 0, bestDamageOnTeam ? bestDamageOnTeam->totalAmount : 0, bestDamageOnOtherTeam ? bestDamageOnOtherTeam->totalAmount : 0, 4);
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
		return FormatStatInt(qtrue, damage, 0, 0, 4);
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

	return FormatStatInt(qfalse, damage, mostDamageThisTeam, mostDamageOtherTeam, 4);
}

static qboolean ShouldShowAccuracyCategory(accuracyCategory_t acc) {
	switch (acc) {
	case ACC_PISTOL_ALT: case ACC_ALL_TYPES_COMBINED: return qtrue;
	case ACC_DISRUPTOR_PRIMARY: case ACC_DISRUPTOR_SNIPE: return !!(level.existingWeaponSpawns & (1 << WP_DISRUPTOR));
	case ACC_REPEATER_ALT: return !!(level.existingWeaponSpawns & (1 << WP_REPEATER));
	case ACC_GOLAN_ALT: return !!(level.existingWeaponSpawns & (1 << WP_FLECHETTE));
	case ACC_ROCKET: return !!(level.existingWeaponSpawns & (1 << WP_ROCKET_LAUNCHER));
	case ACC_CONCUSSION_PRIMARY: case ACC_CONCUSSION_ALT: return !!(level.existingWeaponSpawns & (1 << WP_CONCUSSION));
	case ACC_THERMAL_ALT: return !!(level.existingWeaponSpawns & (1 << WP_THERMAL));
	default: assert(qfalse); return qfalse;
	}
}

const char *CtfStatsTableCallback_WeaponAccuracy(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	accuracyCategory_t acc = *((accuracyCategory_t *)columnContext);

	// only show columns for weapons that exist on this map
	if (!ShouldShowAccuracyCategory(acc))
		return NULL;

	if (acc == ACC_ALL_TYPES_COMBINED) {
		if (!stats->accuracy_shots)
			return " "; // return a single space instead of null so that the column doesn't get removed if nobody has fired this weapon yet
		return FormatStatInt(stats->isTotal, stats->accuracy, bestStats[stats->lastTeam].accuracy, bestStats[OtherTeam(stats->lastTeam)].accuracy, 3);
	}
	else {
		if (!stats->accuracy_shotsOfType[acc])
			return " "; // return a single space instead of null so that the column doesn't get removed if nobody has fired this weapon yet
		return FormatStatInt(stats->isTotal, stats->accuracyOfType[acc], bestStats[stats->lastTeam].accuracyOfType[acc], bestStats[OtherTeam(stats->lastTeam)].accuracyOfType[acc], 3);
	}
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
		player->fcKillEfficiency = player->fcKills ? player->fcKillsResultingInRets * 100 / player->fcKills : 0;
		CheckBest(fcKillEfficiency);
		CheckBest(flagCarrierDamageDealtTotal);
		CheckBest(flagCarrierDamageTakenTotal);
		CheckBest(clearDamageDealtTotal);
		CheckBest(clearDamageTakenTotal);
		CheckBest(otherDamageDealtTotal);
		CheckBest(otherDamageTakenTotal);
		CheckBest(teamKills);
		CheckBest(takes);
		CheckBest(rets);
		CheckBest(selfkills);
		CheckBest(healthPickedUp);
		CheckBest(armorPickedUp);
		CheckBest(totalFlagHold);
		CheckBest(longestFlagHold);
		CheckBest(numFlagHolds);
		player->averageFlagHold = player->numFlagHolds ? player->totalFlagHold / player->numFlagHolds : 0;
		CheckBest(averageFlagHold);
		CheckBest(saves);
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
		CheckBest(boonPickups);
		CheckBest(push);
		CheckBest(pull);
		CheckBest(healed);
		CheckBest(energizedAlly);
		player->energizeEfficiency = player->numEnergizes ? player->normalizedEnergizeAmounts * 100 / player->numEnergizes : 0;
		CheckBest(energizeEfficiency);
		CheckBest(energizedEnemy);
		CheckBest(absorbed);
		CheckBest(protDamageAvoided);
		CheckBest(protTimeUsed);
		CheckBest(rageTimeUsed);
		CheckBest(drain);
		CheckBest(gotDrained);
		int allRegionsTime = 0;
		for (ctfRegion_t region = CTFREGION_FLAGSTAND; region <= CTFREGION_ENEMYFLAGSTAND; region++) {
			allRegionsTime += player->regionTime[region];
			CheckBest(regionTime[region]);
		}
		if (allRegionsTime) {
			for (ctfRegion_t region = CTFREGION_FLAGSTAND; region <= CTFREGION_ENEMYFLAGSTAND; region++) {
				player->regionPercent[region] = Com_Clampi(0, 100, (int)round((float)player->regionTime[region] / (float)allRegionsTime * 100.0f));
				CheckBest(regionPercent[region]);
			}
		}
		else {
			for (ctfRegion_t region = CTFREGION_FLAGSTAND; region <= CTFREGION_ENEMYFLAGSTAND; region++) {
				player->regionPercent[region] = 0;
				CheckBest(regionPercent[region]);
			}
		}
		player->averageGetHealth = player->numGets ? (int)floorf((player->getTotalHealth / player->numGets) + 0.5f) : 0;
		CheckBest(averageGetHealth);
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
	else if (type == STATS_TABLE_ACCURACY) {
		player->accuracy = player->accuracy_shots ? player->accuracy_hits * 100 / player->accuracy_shots : 0;
		CheckBest(accuracy);
		for (accuracyCategory_t weap = ACC_FIRST; weap < ACC_MAX; weap++) {
			player->accuracyOfType[weap] = player->accuracy_shotsOfType[weap] ? player->accuracy_hitsOfType[weap] * 100 / player->accuracy_shotsOfType[weap] : 0;
			CheckBest(accuracyOfType[weap]);
		}
	}
}

typedef struct {
	node_t		node;
	int			sessionId;
	qboolean	isBot;
	stats_t		*stats;
	team_t		team;
} gotPlayer_t;

static qboolean PlayerMatches(genericNode_t *node, void *userData) {
	const gotPlayer_t *existing = (const gotPlayer_t *)node;
	const stats_t *thisGuy = (const stats_t *)userData;

	if (existing && thisGuy && thisGuy->sessionId == existing->sessionId && thisGuy->isBot == existing->isBot && thisGuy->lastTeam == existing->team)
		return qtrue;

	return qfalse;
}

// try to prevent people who join and immediately go spec from showing up on stats
qboolean StatsValid(const stats_t *stats) {
	int svFps = g_svfps.integer ? g_svfps.integer : 30; // prevent divide by zero
#ifdef DEBUG_CTF_POSITION_STATS
	if (stats->ticksNotPaused >= 1)
		return qtrue;
#else
	if (stats->isBot || stats->displacementSamples / svFps >= 1 || stats->damageDealtTotal || stats->damageTakenTotal)
		return qtrue;
#endif
	return qfalse;
}

#define AddStatToTotal(field) do { total->field += player->field; }  while (0)
void AddStatsToTotal(stats_t *player, stats_t *total, statsTableType_t type, stats_t *weaponStatsPtr) {
	assert(player && total);

	if (type == STATS_TABLE_GENERAL) {
		AddStatToTotal(score);
		AddStatToTotal(captures);
		AddStatToTotal(assists);
		AddStatToTotal(defends);
		AddStatToTotal(accuracy_shots);
		AddStatToTotal(accuracy_hits);
		total->accuracy = total->accuracy_shots ? total->accuracy_hits * 100 / total->accuracy_shots : 0;
		AddStatToTotal(airs);
		AddStatToTotal(pits);
		AddStatToTotal(pitted);
		AddStatToTotal(fcKills);
		AddStatToTotal(fcKills);
		AddStatToTotal(fcKillsResultingInRets);
		total->fcKillEfficiency = total->fcKills ? total->fcKillsResultingInRets * 100 / total->fcKills : 0;
		AddStatToTotal(flagCarrierDamageDealtTotal);
		AddStatToTotal(flagCarrierDamageTakenTotal);
		AddStatToTotal(clearDamageDealtTotal);
		AddStatToTotal(clearDamageTakenTotal);
		AddStatToTotal(otherDamageDealtTotal);
		AddStatToTotal(otherDamageTakenTotal);
		AddStatToTotal(teamKills);
		AddStatToTotal(takes);
		AddStatToTotal(rets);
		AddStatToTotal(selfkills);
		AddStatToTotal(healthPickedUp);
		AddStatToTotal(armorPickedUp);
		AddStatToTotal(totalFlagHold);
		AddStatToTotal(numFlagHolds);
		total->averageFlagHold = total->numFlagHolds ? total->totalFlagHold / total->numFlagHolds : 0;
		AddStatToTotal(longestFlagHold);
		AddStatToTotal(saves);
		AddStatToTotal(damageDealtTotal);
		AddStatToTotal(damageTakenTotal);
		AddStatToTotal(topSpeed);
		AddStatToTotal(displacement);
		AddStatToTotal(displacementSamples);
		if (total->displacementSamples)
			total->averageSpeed = (int)floorf(((total->displacement * g_svfps.value) / total->displacementSamples) + 0.5f);
		else
			total->averageSpeed = (int)(total->topSpeed + 0.5f);
	}
	else if (type == STATS_TABLE_FORCE) {
		AddStatToTotal(boonPickups);
		AddStatToTotal(push);
		AddStatToTotal(pull);
		AddStatToTotal(healed);
		AddStatToTotal(energizedAlly);
		AddStatToTotal(numEnergizes);
		AddStatToTotal(normalizedEnergizeAmounts);
		total->energizeEfficiency = total->numEnergizes ? total->normalizedEnergizeAmounts * 100 / total->numEnergizes : 0;
		AddStatToTotal(energizedEnemy);
		AddStatToTotal(absorbed);
		AddStatToTotal(protDamageAvoided);
		AddStatToTotal(protTimeUsed);
		AddStatToTotal(rageTimeUsed);
		AddStatToTotal(drain);
		AddStatToTotal(gotDrained);
		int allRegionsTime = 0;
		for (ctfRegion_t region = CTFREGION_FLAGSTAND; region <= CTFREGION_ENEMYFLAGSTAND; region++) {
			AddStatToTotal(regionTime[region]);
			allRegionsTime += total->regionTime[region];
		}
		if (allRegionsTime) {
			for (ctfRegion_t region = CTFREGION_FLAGSTAND; region <= CTFREGION_ENEMYFLAGSTAND; region++) {
				total->regionPercent[region] = Com_Clampi(0, 100, (int)round((float)total->regionTime[region] / (float)allRegionsTime * 100.0f));
			}
		}
		else {
			for (ctfRegion_t region = CTFREGION_FLAGSTAND; region <= CTFREGION_ENEMYFLAGSTAND; region++) {
				total->regionPercent[region] = 0;
			}
		}
		AddStatToTotal(numGets);
		AddStatToTotal(getTotalHealth);
		total->averageGetHealth = total->numGets ? (int)floorf((total->getTotalHealth / total->numGets) + 0.5f) : 0;
	}
	else if (type == STATS_TABLE_DAMAGE) {
		iterator_t iter;
		ListIterate((list_t *)&player->damageGivenList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			const damageCounter_t *thisDamageGivenByPlayer = IteratorNext(&iter);
			if (thisDamageGivenByPlayer->totalAmount <= 0)
				continue;

			iterator_t iter2;
			ListIterate(&total->damageGivenList, &iter2, qfalse);
			damageCounter_t *found = NULL;
			while (IteratorHasNext(&iter2)) {
				damageCounter_t *thisDamageGivenByTeam = IteratorNext(&iter2);
				if (thisDamageGivenByTeam->otherPlayerSessionId != thisDamageGivenByPlayer->otherPlayerSessionId)
					continue;
				found = thisDamageGivenByTeam;
				break;
			}

			if (!found) {
				found = ListAdd(&total->damageGivenList, sizeof(damageCounter_t));
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
						total->damageOfType[i] += *dmg;
				}
			}
			else {
				int *dmg = GetDamageGivenStatOfType(weaponStatsPtr, player, i);
				if (dmg && *dmg > bestStats[player->lastTeam].damageOfType[i])
					total->damageOfType[i] += *dmg;
			}
		}
	}
	else if (type == STATS_TABLE_WEAPON_TAKEN) {
		for (int i = 0; i < MODC_MAX; i++) {
			if (i == MODC_ALL_TYPES_COMBINED) {
				for (int i = MODC_FIRST; i < MODC_ALL_TYPES_COMBINED; i++) {
					int *dmg = GetDamageTakenStatOfType(player, weaponStatsPtr, i);
					if (dmg)
						total->damageOfType[i] += *dmg;
				}
			}
			else {
				int *dmg = GetDamageTakenStatOfType(player, weaponStatsPtr, i);
				if (dmg && *dmg > bestStats[player->lastTeam].damageOfType[i])
					total->damageOfType[i] += *dmg;
			}
		}
	}
	else if (type == STATS_TABLE_ACCURACY) {
		AddStatToTotal(accuracy_shots);
		AddStatToTotal(accuracy_hits);
		total->accuracy = total->accuracy_shots ? total->accuracy_hits * 100 / total->accuracy_shots : 0;
		for (accuracyCategory_t weap = ACC_FIRST; weap < ACC_MAX; weap++) {
			AddStatToTotal(accuracy_shotsOfType[weap]);
			AddStatToTotal(accuracy_hitsOfType[weap]);
			total->accuracyOfType[weap] = total->accuracy_shotsOfType[weap] ? total->accuracy_hitsOfType[weap] * 100 / total->accuracy_shotsOfType[weap] : 0;
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
			Q_StripColor(cleanPlayerName);
			char *cleanQuery = strdup(str);
			Q_StripColor(cleanQuery);
			if (!Q_stricmp(cleanQuery, cleanPlayerName)) {
				free(cleanPlayerName);
				free(cleanQuery);
				return ent->client->stats; // matched this player name
			}
			free(cleanPlayerName);

			if (ent->client->account && ent->client->account->name[0]) {
				char *cleanAccountName = strdup(ent->client->account->name);
				Q_StripColor(cleanAccountName);
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
			Q_StripColor(cleanPlayerName);
			char *cleanQuery = strdup(str);
			Q_StripColor(cleanQuery);
			if (!Q_stricmp(cleanQuery, cleanPlayerName)) {
				free(cleanPlayerName);
				free(cleanQuery);
				return s; // matched this player name
			}
			free(cleanPlayerName);

			if (s->accountId != ACCOUNT_ID_UNLINKED && s->accountName[0]) {
				char *cleanAccountName = strdup(s->accountName);
				Q_StripColor(cleanAccountName);
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
			Q_StripColor(cleanPlayerName);
			char *cleanQuery = strdup(str);
			Q_StripColor(cleanQuery);
			if (stristr(cleanPlayerName, cleanQuery)) {
				free(cleanPlayerName);
				free(cleanQuery);
				return ent->client->stats; // matched this player name
			}
			free(cleanPlayerName);

			if (ent->client->account && ent->client->account->name[0]) {
				char *cleanAccountName = strdup(ent->client->account->name);
				Q_StripColor(cleanAccountName);
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
			Q_StripColor(cleanPlayerName);
			char *cleanQuery = strdup(str);
			Q_StripColor(cleanQuery);
			if (stristr(cleanPlayerName, cleanQuery)) {
				free(cleanPlayerName);
				free(cleanQuery);
				return s; // matched this player name
			}
			free(cleanPlayerName);

			if (s->accountId != ACCOUNT_ID_UNLINKED && s->accountName[0]) {
				char *cleanAccountName = strdup(s->accountName);
				Q_StripColor(cleanAccountName);
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
	case MODC_MELEESTUNBATON: return "^5Mel";
	case MODC_SABER: return "^5Sab";
	case MODC_PISTOL: return "^5Pis";
	case MODC_BLASTER: return "^5Bla";
	case MODC_DISRUPTOR: return "^5Dis";
	case MODC_BOWCASTER: return "^5Bow";
	case MODC_REPEATER: return "^5Rep";
	case MODC_DEMP: return "^5Dmp";
	case MODC_GOLAN: return "^5Gol";
	case MODC_ROCKET: return "^5Roc";
	case MODC_CONCUSSION: return "^5Con";
	case MODC_THERMAL: return "^5The";
	case MODC_MINE: return "^5Min";
	case MODC_DETPACK: return "^5Dpk";
	case MODC_FORCE: return "^5For";
	case MODC_ALL_TYPES_COMBINED: return "^5Total";
	default: assert(qfalse); return NULL;
	}
}

const char *NameForAccuracyCategory(accuracyCategory_t acc) {
	switch (acc) {
	case ACC_ALL_TYPES_COMBINED: return "^5Acc";
	case ACC_PISTOL_ALT: return "^5Pis";
	case ACC_DISRUPTOR_PRIMARY: return "^5Dis";
	case ACC_DISRUPTOR_SNIPE: return "^5Snp";
	case ACC_REPEATER_ALT: return "^5Rep";
	case ACC_GOLAN_ALT: return "^5Gol";
	case ACC_ROCKET: return "^5Roc";
	case ACC_CONCUSSION_PRIMARY: return "^5Con";
	case ACC_CONCUSSION_ALT: return "^5Alt";
	case ACC_THERMAL_ALT: return "^5The";
	default: assert(qfalse); return NULL;
	}
}

accuracyCategory_t AccuracyCategoryForProjectile(gentity_t *projectile) {
	if (!projectile)
		return ACC_INVALID;

	if (projectile->methodOfDeath == MOD_BRYAR_PISTOL_ALT && projectile->s.generic1 >= 2)
		return ACC_PISTOL_ALT;
	if (projectile->methodOfDeath == MOD_REPEATER_ALT)
		return ACC_REPEATER_ALT;
	if (projectile->methodOfDeath == MOD_ROCKET || projectile->methodOfDeath == MOD_ROCKET_HOMING)
		return ACC_ROCKET;
	if (projectile->methodOfDeath == MOD_THERMAL && !(projectile->flags & FL_BOUNCE_HALF))
		return ACC_THERMAL_ALT;
	if (projectile->methodOfDeath == MOD_CONC)
		return ACC_CONCUSSION_PRIMARY;

	if (projectile->methodOfDeath == MOD_FLECHETTE_ALT_SPLASH) {
		if (projectile->twin) { // neither of these balls have touched; count this one and make the other one never count
			projectile->twin->twin = NULL;
			projectile->twin = NULL;
			return ACC_GOLAN_ALT;
		}
		return ACC_INVALID; // this is a ball, but the other ball has already touched
	}

	return ACC_INVALID;
}


#if defined (_DEBUG) && defined(DEBUGSTATSNAMES) // for testing long names
#define STATSTABLE_NAME			"^5Name5678901234567890"
#define STATSTABLE_NAME_LENGTH	(20)
#define STATSTABLE_ALIAS		"^5Alias6789"
#define STATSTABLE_ALIAS_LENGTH	(9)
#else
#define STATSTABLE_NAME "^5Name"
#define STATSTABLE_NAME_LENGTH	(4)
#define STATSTABLE_ALIAS "^5Alias"
#define STATSTABLE_ALIAS_LENGTH	(5)
#endif

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

	list_t combinedStatsList = { 0 };

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

				ctfPosition_t pos = DetermineCTFPosition(found);

				gotPlayer_t *gotPlayerAlready = ListFind(&gotPlayersList, PlayerMatches, found, NULL);
				if (gotPlayerAlready)
					continue;

				// this player will be in the stats table
				gotPlayer_t *add = ListAdd(&gotPlayersList, sizeof(gotPlayer_t));
				add->stats = found;
				add->sessionId = found->sessionId;
				add->isBot = found->isBot;
				add->team = found->lastTeam;

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

				stats_t *combined = ListAdd(&combinedStatsList, sizeof(stats_t));
				AddStatsToTotal(found, combined, type, weaponStatsPtr); // add the first stats_t into the combined one
				Q_strncpyz(combined->accountName, found->accountName, sizeof(combined->accountName));
				Q_strncpyz(combined->name, found->name, sizeof(combined->name));
				combined->sessionId = found->sessionId;
				combined->accountId = found->accountId;
				combined->isBot = found->isBot;
				combined->clientNum = found->clientNum;
				combined->lastTeam = found->lastTeam;
				combined->finalPosition = pos;
				combined->ticksNotPaused += found->ticksNotPaused;
				combined->confirmedPositionBits |= found->confirmedPositionBits;

				iterator_t iter2;
				ListIterate(&level.savedStatsList, &iter2, qfalse);
				while (IteratorHasNext(&iter2)) {
					stats_t *match = IteratorNext(&iter2);
					if (match->sessionId == found->sessionId && match->lastTeam == found->lastTeam) {
						AddStatsToTotal(match, combined, type, weaponStatsPtr); // add all other matching stats_ts into the combined one
						combined->ticksNotPaused += match->ticksNotPaused;
						combined->confirmedPositionBits |= match->confirmedPositionBits;
					}
				}

				Table_DefineRow(t, combined);

				if (team == winningTeam)
					++numWinningTeam;
				else
					++numLosingTeam;

				goto continueLoop; // suck my
			}
			continueLoop:; // nuts
		}

		// ...then, everybody else (ragequitters, people who changed teams or went spec, etc.)
		iterator_t iter;
		ListIterate(&level.statsList, &iter, qfalse);

		while (IteratorHasNext(&iter)) {
			stats_t *found = IteratorNext(&iter);
			if (!StatsValid(found) || found->lastTeam != team)
				continue;

			ctfPosition_t pos = found->finalPosition ? found->finalPosition : DetermineCTFPosition(found);

			gotPlayer_t *gotPlayerAlready = ListFind(&gotPlayersList, PlayerMatches, found, NULL);
			if (gotPlayerAlready)
				continue;

			// this player will be in the stats table
			gotPlayer_t *add = ListAdd(&gotPlayersList, sizeof(gotPlayer_t));
			add->stats = found;
			add->sessionId = found->sessionId;
			add->isBot = found->isBot;
			add->team = found->lastTeam;

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

			stats_t *combined = ListAdd(&combinedStatsList, sizeof(stats_t));
			AddStatsToTotal(found, combined, type, weaponStatsPtr); // add the first stats_t into the combined one
			Q_strncpyz(combined->accountName, found->accountName, sizeof(combined->accountName));
			Q_strncpyz(combined->name, found->name, sizeof(combined->name));
			combined->sessionId = found->sessionId;
			combined->accountId = found->accountId;
			combined->isBot = found->isBot;
			combined->clientNum = found->clientNum;
			combined->lastTeam = found->lastTeam;
			combined->finalPosition = pos;
			combined->ticksNotPaused += found->ticksNotPaused;
			iterator_t iter2;
			ListIterate(&level.savedStatsList, &iter2, qfalse);
			while (IteratorHasNext(&iter2)) {
				stats_t *match = IteratorNext(&iter2);
				if (match->sessionId == found->sessionId && match->lastTeam == found->lastTeam) {
					AddStatsToTotal(match, combined, type, weaponStatsPtr); // add all other matching stats_ts into the combined one
					combined->ticksNotPaused += match->ticksNotPaused;
				}
			}

			Table_DefineRow(t, combined);

			if (team == winningTeam)
				++numWinningTeam;
			else
				++numLosingTeam;
		}

		// finally, the old stats
		ListIterate(&level.savedStatsList, &iter, qfalse);

		while (IteratorHasNext(&iter)) {
			stats_t *found = IteratorNext(&iter);
			if (!StatsValid(found) || found->lastTeam != team)
				continue;

			ctfPosition_t pos = found->finalPosition ? found->finalPosition : DetermineCTFPosition(found);

			gotPlayer_t *gotPlayerAlready = ListFind(&gotPlayersList, PlayerMatches, found, NULL);
			if (gotPlayerAlready)
				continue;

			// this player will be in the stats table
			gotPlayer_t *add = ListAdd(&gotPlayersList, sizeof(gotPlayer_t));
			add->stats = found;
			add->sessionId = found->sessionId;
			add->isBot = found->isBot;
			add->team = found->lastTeam;

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

			stats_t *combined = ListAdd(&combinedStatsList, sizeof(stats_t));
			AddStatsToTotal(found, combined, type, weaponStatsPtr); // add the first stats_t into the combined one
			Q_strncpyz(combined->accountName, found->accountName, sizeof(combined->accountName));
			Q_strncpyz(combined->name, found->name, sizeof(combined->name));
			combined->sessionId = found->sessionId;
			combined->accountId = found->accountId;
			combined->isBot = found->isBot;
			combined->clientNum = found->clientNum;
			combined->lastTeam = found->lastTeam;
			combined->finalPosition = pos;
			combined->ticksNotPaused += found->ticksNotPaused;
			iterator_t iter2;
			ListIterate(&level.savedStatsList, &iter2, qfalse);
			while (IteratorHasNext(&iter2)) {
				stats_t *match = IteratorNext(&iter2);
				if (match->sessionId == found->sessionId && match->lastTeam == found->lastTeam) {
					AddStatsToTotal(match, combined, type, weaponStatsPtr); // add all other matching stats_ts into the combined one
					combined->ticksNotPaused += match->ticksNotPaused;
				}
			}

			Table_DefineRow(t, combined);

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
		ListClear(&combinedStatsList);
		return;
	}

	int longestPrintLenName = STATSTABLE_NAME_LENGTH /*Name*/, longestPrintLenAlias = STATSTABLE_ALIAS_LENGTH /*Alias*/, longestPrintLenPos = 0 /*Pos -- gets set to 3 if a pos is detected*/;

	if (type == STATS_TABLE_GENERAL) {
		Table_DefineColumn(t, STATSTABLE_NAME, CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, STATSTABLE_ALIAS, CtfStatsTableCallback_Alias, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5Pos", CtfStatsTableCallback_Position, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5Time", CtfStatsTableCallback_Time, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5Cap", CtfStatsTableCallback_Captures, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5Ass", CtfStatsTableCallback_Assists, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5Def", CtfStatsTableCallback_Defends, NULL, qfalse, -1, 32);
		//Table_DefineColumn(t, "^5SAV", CtfStatsTableCallback_Saves, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5Acc", CtfStatsTableCallback_Accuracy, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5Air", CtfStatsTableCallback_Airs, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5TK", CtfStatsTableCallback_TeamKills, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5Take", CtfStatsTableCallback_Takes, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^6Pit", CtfStatsTableCallback_Pits, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^6Dth", CtfStatsTableCallback_Pitted, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^2Dmg", CtfStatsTableCallback_DamageDealt, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^2Fc", CtfStatsTableCallback_FlagCarrierDamageDealt, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^2Clr", CtfStatsTableCallback_ClearDamageDealt, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^2Othr", CtfStatsTableCallback_OtherDamageDealt, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^1Tkn", CtfStatsTableCallback_DamageTaken, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^1Fc", CtfStatsTableCallback_FlagCarrierDamageTaken, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^1Clr", CtfStatsTableCallback_ClearDamageTaken, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^1Othr", CtfStatsTableCallback_OtherDamageTaken, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^6FcKil", CtfStatsTableCallback_FcKills, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^6Eff", CtfStatsTableCallback_FlagCarrierKillEfficiency, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5Ret", CtfStatsTableCallback_Rets, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5SK", CtfStatsTableCallback_Selfkills, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^3Hold", CtfStatsTableCallback_TotalHold, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^3Max", CtfStatsTableCallback_MaxHold, NULL, qfalse, -1, 32);
		//Table_DefineColumn(t, "^5HOLDS", CtfStatsTableCallback_Holds, NULL, qfalse, -1, 32);
		//Table_DefineColumn(t, "^5AVGHOLD", CtfStatsTableCallback_AverageHold, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^6Spd", CtfStatsTableCallback_AverageSpeed, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^6Top", CtfStatsTableCallback_TopSpeed, NULL, qfalse, -1, 32);
		//Table_DefineColumn(t, "^5+HP", CtfStatsTableCallback_HealthPickedUp, NULL, qfalse, -1, 32);
		//Table_DefineColumn(t, "^5+SH", CtfStatsTableCallback_ArmorPickedUp, NULL, qfalse, -1, 32);
	}
	else if (type == STATS_TABLE_FORCE) {
		Table_DefineColumn(t, STATSTABLE_NAME, CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, STATSTABLE_ALIAS, CtfStatsTableCallback_Alias, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5Pos", CtfStatsTableCallback_Position, NULL, qtrue, -1, 32);
		if (level.boonExists) // only show boon stat if boon is enabled and exists on this map
			Table_DefineColumn(t, "^5Boon", CtfStatsTableCallback_BoonPickups, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5Push", CtfStatsTableCallback_Push, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5Pull", CtfStatsTableCallback_Pull, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5Heal", CtfStatsTableCallback_Healed, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^6TE", CtfStatsTableCallback_EnergizedAlly, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^6Eff", CtfStatsTableCallback_EnergizeEfficiency, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5EnemyNrg", CtfStatsTableCallback_EnergizedEnemy, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5Abs", CtfStatsTableCallback_Absorbed, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^2Prot", CtfStatsTableCallback_ProtDamage, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^2Time", CtfStatsTableCallback_ProtTime, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5Rage", CtfStatsTableCallback_RageTime, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^1Drn", CtfStatsTableCallback_Drain, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^1Drnd", CtfStatsTableCallback_GotDrained, NULL, qfalse, -1, 32);
		ctfRegion_t region = CTFREGION_FLAGSTAND;
		Table_DefineColumn(t, "^5Fs", CtfStatsTableCallback_CtfRegionPercent, &region, qfalse, -1, 32);
		++region;
		Table_DefineColumn(t, "^5Bas", CtfStatsTableCallback_CtfRegionPercent, &region, qfalse, -1, 32);
		++region;
		Table_DefineColumn(t, "^5Mid", CtfStatsTableCallback_CtfRegionPercent, &region, qfalse, -1, 32);
		++region;
		Table_DefineColumn(t, "^5EBa", CtfStatsTableCallback_CtfRegionPercent, &region, qfalse, -1, 32);
		++region;
		Table_DefineColumn(t, "^5EFs", CtfStatsTableCallback_CtfRegionPercent, &region, qfalse, -1, 32);
	}
	else if (type == STATS_TABLE_DAMAGE) {
		int firstDividerColor;
		if (numWinningTeam) {
			if (winningTeam == TEAM_RED)
				firstDividerColor = 1;
			else
				firstDividerColor = 4;
		}
		else {
			if (losingTeam == TEAM_RED)
				firstDividerColor = 1;
			else
				firstDividerColor = 4;
		}

		qboolean didFirstColumns = qfalse;

		iterator_t iter;
		ListIterate(&gotPlayersList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			gotPlayer_t *player = IteratorNext(&iter);
			char *clean = strdup(player->stats->name);
			Q_StripColor(clean);
			//Q_strupr(clean);
			char *name;
			ctfPosition_t pos = DetermineCTFPosition(player->stats);
			if (pos) {
				if (pos == CTFPOSITION_BASE)
					name = va("^5%s (Bas)", clean);
				else if (pos == CTFPOSITION_CHASE)
					name = va("^5%s (Cha)", clean);
				else if (pos == CTFPOSITION_OFFENSE)
					name = va("^5%s (Off)", clean);
				longestPrintLenPos = 3; // account for "Pos" spacing
			}
			else {
				name = va("^5%s", clean);
			}
			free(clean);
			int len = Q_PrintStrlen(player->stats->name);
			if (len > longestPrintLenName)
				longestPrintLenName = len;
			if (VALIDSTRING(player->stats->accountName)) {
				len = Q_PrintStrlen(player->stats->accountName);
				if (len > longestPrintLenAlias)
					longestPrintLenAlias = len;
			}

			int secondDividerColor;
			if (numWinningTeam && numLosingTeam && player->stats == lastWinningTeamPlayer) {
				if (winningTeam == TEAM_RED)
					secondDividerColor = 4;
				else
					secondDividerColor = 1;
			}
			else {
				secondDividerColor = -1;
			}

			if (!didFirstColumns) {
				// if we have pos, then the divider goes after the pos column. if not, then the divider goes after the alias column.
				Table_DefineColumn(t, STATSTABLE_NAME, CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
				Table_DefineColumn(t, STATSTABLE_ALIAS, CtfStatsTableCallback_Alias, NULL, qtrue, longestPrintLenPos ? -1 : firstDividerColor, 32);
				Table_DefineColumn(t, "^5Pos", CtfStatsTableCallback_Position, NULL, qtrue, longestPrintLenPos ? firstDividerColor : -1, 32);
				didFirstColumns = qtrue;
			}
			Table_DefineColumn(t, name, TableCallback_Damage, player->stats, qfalse, secondDividerColor, 32);
		}

		if (!didFirstColumns) { // sanity check, make sure these get printed no matter what
			// if we have pos, then the divider goes after the pos column. if not, then the divider goes after the alias column.
			Table_DefineColumn(t, STATSTABLE_NAME, CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
			Table_DefineColumn(t, STATSTABLE_ALIAS, CtfStatsTableCallback_Alias, NULL, qtrue, longestPrintLenPos ? -1 : firstDividerColor, 32);
			Table_DefineColumn(t, "^5Pos", CtfStatsTableCallback_Position, NULL, qtrue, longestPrintLenPos ? firstDividerColor : -1, 32);
			didFirstColumns = qtrue;
		}
	}
	else if (type == STATS_TABLE_WEAPON_GIVEN || type == STATS_TABLE_WEAPON_TAKEN) {
		Table_DefineColumn(t, STATSTABLE_NAME, CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, STATSTABLE_ALIAS, CtfStatsTableCallback_Alias, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5Pos", CtfStatsTableCallback_Position, NULL, qtrue, -1, 32);

		for (meansOfDeathCategory_t modc = MODC_FIRST; modc < MODC_MAX; modc++) {
			meansOfDeathCategoryContext_t context;
			context.tablePlayerStats = weaponStatsPtr;
			context.damageTaken = !!(type == STATS_TABLE_WEAPON_TAKEN);
			context.modc = modc;
			Table_DefineColumn(t, NameForMeansOfDeathCategory(modc), TableCallback_WeaponDamage, &context, qfalse, -1, 32);
		}
	}
	else if (type == STATS_TABLE_ACCURACY) {
		Table_DefineColumn(t, STATSTABLE_NAME, CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, STATSTABLE_ALIAS, CtfStatsTableCallback_Alias, NULL, qtrue, -1, 32);
		Table_DefineColumn(t, "^5Pos", CtfStatsTableCallback_Position, NULL, qtrue, -1, 32);

		for (accuracyCategory_t weap = ACC_FIRST; weap < ACC_MAX; weap++)
			Table_DefineColumn(t, NameForAccuracyCategory(weap), CtfStatsTableCallback_WeaponAccuracy, &weap, qfalse, -1, 32);
	}

	ListClear(&gotPlayersList);
	ListClear(&combinedStatsList);

#define TEMP_STATS_BUFFER_SIZE	(16384) // idk
	char *temp = malloc(TEMP_STATS_BUFFER_SIZE);
	size_t tempSize = TEMP_STATS_BUFFER_SIZE;
	memset(temp, 0, tempSize);
	if (type == STATS_TABLE_DAMAGE) {
		for (int i = 0; i < longestPrintLenName + 1; i++)
			Q_strcat(temp, tempSize, " "); // name column spaces, plus a space after
		for (int i = 0; i < longestPrintLenAlias + 1; i++)
			Q_strcat(temp, tempSize, " "); // alias column spaces, plus a space after
		for (int i = 0; i < 2; i++)
			Q_strcat(temp, tempSize, " "); // divider space, plus a space after
		if (longestPrintLenPos) {
			for (int i = 0; i < longestPrintLenPos + 1; i++)
				Q_strcat(temp, tempSize, " "); // pos column spaces, plus a space after
		}
		Q_strcat(temp, tempSize, "^5Damage dealt to\n");
		int len = strlen(temp);
		Table_WriteToBuffer(t, temp + len, tempSize - len, qtrue, numWinningTeam ? (winningTeam == TEAM_BLUE ? 4 : 1) : (losingTeam == TEAM_BLUE ? 4 : 1));
	}
	else if (type == STATS_TABLE_WEAPON_GIVEN || type == STATS_TABLE_WEAPON_TAKEN) {
		ctfPosition_t pos = DetermineCTFPosition(weaponStatsPtr);
		Com_sprintf(temp, tempSize, "%s^7 by %s (%s%s)^7:\n",
			type == STATS_TABLE_WEAPON_GIVEN ? "^2Damage DEALT" : "^8Damage TAKEN",
			weaponStatsPtr->name,
			weaponStatsPtr->lastTeam == TEAM_RED ? "Red" : "Blue",
			pos ? (va(" %s", GetPositionStringForStats(weaponStatsPtr))) : "");
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

typedef struct {
	const statsTableType_t type;
	const char *shortName;
	const char *longName;
	const char *description;
} statsHelp_t;

static statsHelp_t helps[] = { // important: make sure any new stats do not conflict with /stats help subcommands that print entire categories (e.g. "ex")
	{ STATS_TABLE_GENERAL, "^5Pos", "Position", "CTF position, if 4v4"},
	{ STATS_TABLE_GENERAL, "^5Time", "Time", "Time in game while not paused"},
	{ STATS_TABLE_GENERAL, "^5Cap", "Captures", "Times you captured the flag"},
	{ STATS_TABLE_GENERAL, "^5Ass", "Assists", "Flag carrier kills, or flag returns, within ten seconds prior to your team capturing the flag"},
	{ STATS_TABLE_GENERAL, "^5Def", "Defends", "Kills on enemies near your flag, near your flag carrier, or who hurt your flag carrier within the past eight seconds"},
	{ STATS_TABLE_GENERAL, "^5Acc", "Accuracy", "Total accuracy percentage, only including disrupts, snipes, repeater blobs, golan balls, rockets, alt thermals, and both concs"},
	{ STATS_TABLE_GENERAL, "^5Air", "Air shots", "Hits on midair enemies, only including explosive projectiles, bowcaster, and charged pistol; does not have to kill"},
	{ STATS_TABLE_GENERAL, "^5TK", "Teamkills", "Bad kills on teammates; does not include killing your flag carrier if an ally takes the flag afterward"},
	{ STATS_TABLE_GENERAL, "^5Take", "Takes", "Times you killed your flag carrier and then took the flag"},
	{ STATS_TABLE_GENERAL, "^6Pit", "Pit kills", "Kills on enemies using pits"},
	{ STATS_TABLE_GENERAL, "^6Dth", "Pit deaths", "Times you got killed by enemies using pits"},
	{ STATS_TABLE_GENERAL, "^2Dmg", "Damage", "Total damage dealt to enemies"},
	{ STATS_TABLE_GENERAL, "^2Fc", "Flag carrier damage", "Damage dealt to enemy flag carriers"},
	{ STATS_TABLE_GENERAL, "^2Clr", "Clear damage", "Damage dealt to non-flag-carrying enemies inside your base"},
	{ STATS_TABLE_GENERAL, "^2Othr", "Other damage", "Damage dealt to enemies outside your base who are not carrying the flag"},
	{ STATS_TABLE_GENERAL, "^1Tkn", "Damage taken", "Total damage taken from enemies"},
	{ STATS_TABLE_GENERAL, "^1Fc", "Flag carrier damage taken", "Damage taken from enemies while carrying the flag"},
	{ STATS_TABLE_GENERAL, "^1Clr", "Clear damage taken", "Damage taken from enemies while you are in their base and not carrying the flag"},
	{ STATS_TABLE_GENERAL, "^1Othr", "Other damage taken", "Damage taken from enemies while you are outside their base and not carrying the flag"},
	{ STATS_TABLE_GENERAL, "^6FcKil", "Flag carrier kills", "Kills on enemy flag carriers"},
	{ STATS_TABLE_GENERAL, "^6FcKil Eff", "Flag carrier kill efficiency", "Percentage of your kills on flag carriers that resulted in the flag being returned by you or a teammate"},
	{ STATS_TABLE_GENERAL, "^5Ret", "Returns", "Times you returned your flag"},
	{ STATS_TABLE_GENERAL, "^5SK", "Selfkills", "Suicides, including falling into pits on your own"},
	{ STATS_TABLE_GENERAL, "^3Hold", "Total hold", "Total time you carried the flag"},
	{ STATS_TABLE_GENERAL, "^3MaxHold", "Maximum hold", "The duration of your longest flag hold"},
	{ STATS_TABLE_GENERAL, "^6Spd", "Average speed", "Average movement speed, in units per second"},
	{ STATS_TABLE_GENERAL, "^6Top", "Top speed", "The highest movement speed you reached, in units per second"},
	{ STATS_TABLE_GENERAL, "^5Boon", "Boons", "Boon pickups, if boon is enabled"},
	{ STATS_TABLE_GENERAL, "^5Push", "Pushes", "Force pushes used"},
	{ STATS_TABLE_GENERAL, "^5Pull", "Pulls", "Force pulls used"},
	{ STATS_TABLE_GENERAL, "^5Heal", "Team heal", "Amount of allies' health you replenished with team heal"},
	{ STATS_TABLE_GENERAL, "^6TE", "Team energize", "Amount of allies' force power you replenished with team energize"},
	{ STATS_TABLE_GENERAL, "^6TE Eff", "Team energize efficiency", "Efficiency of your team energize usage (highest amount anyone was energized ÷ maximum amount anyone could have been energized)"},
	{ STATS_TABLE_GENERAL, "^5EnemyNrg", "Enemy energize", "Force power given to enemies using absorb"},
	{ STATS_TABLE_GENERAL, "^5Abs", "Absorb", "Force power absorbed from enemies"},
	{ STATS_TABLE_GENERAL, "^2Prot", "Protect damage", "Damage avoided using protect"},
	{ STATS_TABLE_GENERAL, "^2Time", "Protect time", "Total duration you had protect activated"},
	{ STATS_TABLE_GENERAL, "^5Rage", "Rage time", "Total duration you had rage activated"},
	{ STATS_TABLE_GENERAL, "^1Drn", "Drain", "Force power gained by draining enemies"},
	{ STATS_TABLE_GENERAL, "^1Drnd", "Drained", "Force power given to enemies by drain"},
	{ STATS_TABLE_GENERAL, "^5Fs", "Allied flagstand time", "Percentage of the match spent in the 20 percent of the map closest to your flagstand"},
	{ STATS_TABLE_GENERAL, "^5Bas", "Allied base time", "Percentage of the match spent in the 20 percent of the map between your flagstand and mid"},
	{ STATS_TABLE_GENERAL, "^5Mid", "Mid time", "Percentage of the match spent in the middle 20 percent of the map"},
	{ STATS_TABLE_GENERAL, "^5EBa", "Enemy base time", "Percentage of the match spent in the 20 percent of the map between the enemy flagstand and mid"},
	{ STATS_TABLE_GENERAL, "^5EFs", "Enemy flagstand time", "Percentage of the match spent in the 20 percent of the map closest to the enemy flagstand"},
	// STATS_TABLE_DAMAGE is not used here
	{ STATS_TABLE_WEAPON_GIVEN, "^5Mel", "Melee damage", "Damage dealt to enemies with melee"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Sab", "Lightsaber damage", "Damage dealt to enemies with lightsaber"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Pis", "Pistol damage", "Damage dealt to enemies with pistol"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Bla", "Blaster damage", "Damage dealt to enemies with blaster"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Dis", "Disruptor damage", "Damage dealt to enemies with disruptor"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Bow", "Bowcaster damage", "Damage dealt to enemies with bowcaster"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Rep", "Repeater damage", "Damage dealt to enemies with repeater"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Dmp", "Demp damage", "Damage dealt to enemies with demp"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Gol", "Golan damage", "Damage dealt to enemies with golan"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Roc", "Rocket damage", "Damage dealt to enemies with rockets"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Con", "Concussion rifle damage", "Damage dealt to enemies with concussion rifle"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5The", "Thermal detonator damage", "Damage dealt to enemies with thermal detonators"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Min", "Mine damage", "Damage dealt to enemies with mines"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Dpk", "Detpack damage", "Damage dealt to enemies with detpacks"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5For", "Force damage", "Damage dealt to enemies with force powers"},
	{ STATS_TABLE_WEAPON_GIVEN, "^5Total", "Total damage", "Total damage dealt to enemies "},
	// STATS_TABLE_WEAPON_TAKEN is not used here
	{ STATS_TABLE_ACCURACY, "^5Pos", "Position", "CTF position, if 4v4"},
	{ STATS_TABLE_ACCURACY, "^5Acc", "Accuracy", "Total accuracy percentage, only including disrupts, snipes, repeater blobs, golan balls, rockets, alt thermals, and both concs"},
	{ STATS_TABLE_ACCURACY, "^5Pis", "Pistol altfire accuracy", "Accuracy percentage of pistol altfire (must be charged at least 400ms)"},
	{ STATS_TABLE_ACCURACY, "^5Dis", "Disruptor primary accuracy", "Accuracy percentage of disruptor primary"},
	{ STATS_TABLE_ACCURACY, "^5Snp", "Disruptor snipe accuracy", "Accuracy percentage of disruptor snipes"},
	{ STATS_TABLE_ACCURACY, "^5Rep", "Repeater blob accuracy", "Accuracy percentage of repeater blobs"},
	{ STATS_TABLE_ACCURACY, "^5Gol", "Golan ball accuracy", "Accuracy percentage of golan balls"},
	{ STATS_TABLE_ACCURACY, "^5Roc", "Rocket accuracy", "Accuracy percentage of rockets"},
	{ STATS_TABLE_ACCURACY, "^5Con", "Concussion rifle primary accuracy", "Accuracy percentage of concussion rifle primary"},
	{ STATS_TABLE_ACCURACY, "^5Alt", "Concussion rifle altfire accuracy", "Accuracy percentage of concussion rifle altfire"},
	{ STATS_TABLE_ACCURACY, "^5The", "Thermal detonator altfire accuracy", "Accuracy percentage of thermal detonator altfire"},
}; // important: make sure any new stats do not conflict with /stats help subcommands that print entire categories (e.g. "ex")

const char *StatsHelpTableCallback_Category(void *rowContext, void *columnContext) {
	if (!rowContext)
		return NULL;
	statsHelp_t *help = rowContext;
	switch (help->type) {
	case STATS_TABLE_GENERAL: return "General";
	case STATS_TABLE_DAMAGE: return "Damage";
	case STATS_TABLE_WEAPON_GIVEN: return "Weapon";
	case STATS_TABLE_WEAPON_TAKEN: assert(qfalse); return NULL; // use STATS_TABLE_WEAPON_GIVEN instead
	case STATS_TABLE_ACCURACY: return "Accuracy";
	default: assert(qfalse); return NULL;
	}
}

const char *StatsHelpTableCallback_ShortName(void *rowContext, void *columnContext) {
	if (!rowContext)
		return NULL;
	statsHelp_t *help = rowContext;
	return help->shortName;
}

const char *StatsHelpTableCallback_LongName(void *rowContext, void *columnContext) {
	if (!rowContext)
		return NULL;
	statsHelp_t *help = rowContext;
	return help->longName;
}

const char *StatsHelpTableCallback_Description(void *rowContext, void *columnContext) {
	if (!rowContext)
		return NULL;
	statsHelp_t *help = rowContext;
	return help->description;
}

static void PrintHelpForStatsTable(statsTableType_t type, int id) {
	Table *t = Table_Initialize(qtrue);
	
	statsHelp_t *gotOne = NULL;
	for (statsHelp_t *help = &helps[0]; help - helps < ARRAY_LEN(helps); help++) {
		if (help->type == type) {
			gotOne = help;
			Table_DefineRow(t, help);
		}
	}

	if (!gotOne) {
		assert(qfalse);
		Table_Destroy(t);
		return;
	}

	PrintIngame(id, "%s stats:\n", StatsHelpTableCallback_Category(gotOne, NULL));
	Table_DefineColumn(t, "Name", StatsHelpTableCallback_ShortName, NULL, qtrue, -1, 32);
	Table_DefineColumn(t, "Full name", StatsHelpTableCallback_LongName, NULL, qtrue, -1, 64);
	Table_DefineColumn(t, "Description", StatsHelpTableCallback_Description, NULL, qtrue, -1, 256);

	char buf[8192] = { 0 };
	Table_WriteToBuffer(t, buf, sizeof(buf), qtrue, -1);
	Table_Destroy(t);

	PrintIngame(id, buf);
}

static qboolean PrintHelpForIndividualStat(const char *query, int id) {
	Table *t = Table_Initialize(qtrue);

	qboolean gotOne = qfalse;
	for (statsHelp_t *help = &helps[0]; help - helps < ARRAY_LEN(helps); help++) {
		if (stristr(help->shortName, query) || stristr(help->longName, query)) {
			Table_DefineRow(t, help);
			gotOne = qtrue;
		}
	}

	if (!gotOne) {
		Table_Destroy(t);
		return qfalse;
	}

	Table_DefineColumn(t, "Type", StatsHelpTableCallback_Category, NULL, qtrue, -1, 32);
	Table_DefineColumn(t, "Short name", StatsHelpTableCallback_ShortName, NULL, qtrue, -1, 32);
	Table_DefineColumn(t, "Full name", StatsHelpTableCallback_LongName, NULL, qtrue, -1, 64);
	Table_DefineColumn(t, "Description", StatsHelpTableCallback_Description, NULL, qtrue, -1, 256);

	char buf[8192] = { 0 };
	Table_WriteToBuffer(t, buf, sizeof(buf), qtrue, -1);
	Table_Destroy(t);

	PrintIngame(id, buf);
	return qtrue;
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

	if (!Q_stricmpn(type, "ge", 2)) { // general stats
		// for general stats, also print the score
		team_t winningTeam = level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED] ? TEAM_BLUE : TEAM_RED; // red if tied to match scoreboard order
		team_t losingTeam = OtherTeam(winningTeam);
		int ms = level.intermissiontime ? (level.intermissiontime - level.startTime) : (level.time - level.startTime);
		int secs = ms / 1000;
		int mins = secs / 60;
		char *timeStr;
		if (ms >= 60000)
			timeStr = va("%d:%02d", mins, secs % 60);
		else
			timeStr = va("0:%02d", secs);
		const char *s = va("%s: "S_COLOR_WHITE"%d    %s: "S_COLOR_WHITE"%d\n%s%s\n\n",
			ScoreTextForTeam(winningTeam), level.teamScores[winningTeam],
			ScoreTextForTeam(losingTeam), level.teamScores[losingTeam],
			timeStr,
			level.intermissiontime ? "" : " elapsed");

		if (outputBuffer) {
			Q_strncpyz(outputBuffer, s, outSize);
			int len = strlen(outputBuffer);
			PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_GENERAL, NULL);
			len = strlen(outputBuffer);
			PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_FORCE, NULL);
		}
		else {
			if (announce)
				PrintIngame(id, s);
			PrintTeamStats(id, NULL, 0, announce, STATS_TABLE_GENERAL, NULL);
			PrintTeamStats(id, NULL, 0, announce, STATS_TABLE_FORCE, NULL);
		}
	}
	else if (!Q_stricmpn(type, "da", 2) || !Q_stricmpn(type, "dmg", 3)) { // damage stats
		if (outputBuffer) {
			int len = strlen(outputBuffer);
			PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_DAMAGE, NULL);
		}
		else {
			PrintTeamStats(id, NULL, 0, announce, STATS_TABLE_DAMAGE, NULL);
		}
		return;
	}
	else if (!Q_stricmpn(type, "we", 2) || !Q_stricmpn(type, "wpn", 3)) { // weapon stats
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
			team_t winningTeam = level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED] ? TEAM_BLUE : TEAM_RED; // red if tied to match scoreboard order
			team_t losingTeam = OtherTeam(winningTeam);
			list_t gotPlayersList = { 0 };
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

						gotPlayer_t *add = ListAdd(&gotPlayersList, sizeof(gotPlayer_t));
						add->stats = found;
						add->sessionId = found->sessionId;
						add->isBot = found->isBot;
						add->team = found->lastTeam;

						int len = outputBuffer ? strlen(outputBuffer) : 0;
						PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_WEAPON_GIVEN, found);
						len = outputBuffer ? strlen(outputBuffer) : 0;
						PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_WEAPON_TAKEN, found);

						goto continueWeaponLoop; // suck my
					}
				continueWeaponLoop:; // nuts
				}

				// ...then, everybody else (ragequitters, people who changed teams or went spec, etc.)
				iterator_t iter;
				ListIterate(&level.statsList, &iter, qfalse);

				while (IteratorHasNext(&iter)) {
					stats_t *found = IteratorNext(&iter);
					if (!StatsValid(found) || found->lastTeam != team)
						continue;

					ctfPosition_t pos = found->finalPosition ? found->finalPosition : DetermineCTFPosition(found);

					gotPlayer_t *gotPlayerAlready = ListFind(&gotPlayersList, PlayerMatches, found, NULL);
					if (gotPlayerAlready)
						continue;

					gotPlayer_t *add = ListAdd(&gotPlayersList, sizeof(gotPlayer_t));
					add->stats = found;
					add->sessionId = found->sessionId;
					add->isBot = found->isBot;
					add->team = found->lastTeam;

					int len = outputBuffer ? strlen(outputBuffer) : 0;
					PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_WEAPON_GIVEN, found);
					len = outputBuffer ? strlen(outputBuffer) : 0;
					PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_WEAPON_TAKEN, found);
				}

				// finally, the old stats
				ListIterate(&level.savedStatsList, &iter, qfalse);

				while (IteratorHasNext(&iter)) {
					stats_t *found = IteratorNext(&iter);
					if (!StatsValid(found) || found->lastTeam != team)
						continue;

					ctfPosition_t pos = found->finalPosition ? found->finalPosition : DetermineCTFPosition(found);

					gotPlayer_t *gotPlayerAlready = ListFind(&gotPlayersList, PlayerMatches, found, NULL);
					if (gotPlayerAlready)
						continue;

					gotPlayer_t *add = ListAdd(&gotPlayersList, sizeof(gotPlayer_t));
					add->stats = found;
					add->sessionId = found->sessionId;
					add->isBot = found->isBot;
					add->team = found->lastTeam;

					int len = outputBuffer ? strlen(outputBuffer) : 0;
					PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_WEAPON_GIVEN, found);
					len = outputBuffer ? strlen(outputBuffer) : 0;
					PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_WEAPON_TAKEN, found);
				}
			}
			ListClear(&gotPlayersList);
		}
		return;
	}
	else if (!Q_stricmpn(type, "ac", 2)) {
		if (outputBuffer) {
			int len = strlen(outputBuffer);
			PrintTeamStats(id, outputBuffer + len, outSize - len, announce, STATS_TABLE_ACCURACY, NULL);
		}
		else {
			PrintTeamStats(id, NULL, 0, announce, STATS_TABLE_ACCURACY, NULL);
		}
	}
	else if (id != -1) {
		if (!Q_stricmp(type, "help")) {
			char query[MAX_STRING_CHARS] = { 0 };
			if (trap_Argc() >= 3)
				trap_Argv(2, query, sizeof(query));

			if (query[0]) { // important: any new stats must not conflict with these category subcommands
				if (!Q_stricmp(query, "all")) {
					PrintHelpForStatsTable(STATS_TABLE_GENERAL, id);
					PrintHelpForStatsTable(STATS_TABLE_WEAPON_GIVEN, id);
					PrintHelpForStatsTable(STATS_TABLE_ACCURACY, id);
				}
				else if (!Q_stricmp(query, "gen") || !Q_stricmp(query, "general")) {
					PrintHelpForStatsTable(STATS_TABLE_GENERAL, id);
				}
				else if (!Q_stricmp(query, "weapon") || !Q_stricmp(query, "wpn")) {
					PrintHelpForStatsTable(STATS_TABLE_WEAPON_GIVEN, id);
				}
				else if (!Q_stricmp(query, "accuracy")) {
					PrintHelpForStatsTable(STATS_TABLE_ACCURACY, id);
				}
				else {
					if (!PrintHelpForIndividualStat(query, id))
						PrintIngame(id, "No help found for '%s^7'.\n"
							"Usage: ^5stats help <query>^7\n"
							"Query can be a stat name (e.g. FcDmg) or type (general/accuracy/weapon) or 'all'\n", query);
				}
			}
			else {
				PrintIngame(id, "Usage: ^5stats help <query>^7\n"
					"Query can be a stat name (e.g. FcDmg) or type (general/accuracy/weapon) or 'all'\n", query);
			}
		}
		else {
			PrintIngame(id, "Unknown argument '%s^7'.\nUsage: ^5stats <gen | dmg | acc | wpn [player]>^7\nEnter ^5stats help^7 for more information about a stat.\n", type);
		}
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

void FinalizeCTFPositions(void) {
	if (g_gametype.integer != GT_CTF)
		return;

	iterator_t iter;
	ListIterate(&level.statsList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		stats_t *s = IteratorNext(&iter);
		s->finalPosition = DetermineCTFPosition(s);
	}
}

// moves some current stats into the archive (level.savedStatsList) and then resets the current stats
static void ArchiveAndResetStatsBlock(stats_t *ongoing, int newBlockNum) {
	assert(ongoing);
	stats_t *archive = ListAdd(&level.savedStatsList, sizeof(stats_t));

	// save prev/next of the archive before memcpy overwrites it
	genericNode_t *prev = archive->node.prev;
	genericNode_t *next = archive->node.next;

	// copy the entire ongoing struct into the archive
	memcpy(archive, ongoing, sizeof(stats_t));

	// restore prev of the archive
	archive->node.prev = prev;
	archive->node.next = next;

	// set some important stuff in the archive
	archive->finalPosition = ongoing->finalPosition;
	if (ongoing->finalPosition)
		archive->confirmedPositionBits |= (1 << ongoing->finalPosition);
	archive->blockNum = level.statBlock;

	// clear out the archive's lists, which at the moment contain the same pointers as the ongoing struct's lists
	memset(&archive->damageGivenList, 0, sizeof(list_t));
	memset(&archive->damageTakenList, 0, sizeof(list_t));
	memset(&archive->teammatePositioningList, 0, sizeof(list_t));

	// copy the lists
	ListCopy(&ongoing->damageGivenList, &archive->damageGivenList, sizeof(damageCounter_t));
	ListCopy(&ongoing->damageTakenList, &archive->damageTakenList, sizeof(damageCounter_t));
	ListCopy(&ongoing->teammatePositioningList, &archive->teammatePositioningList, sizeof(ctfPositioningData_t));

	// free the lists before clearing out the ongoing struct
	ListClear(&ongoing->damageGivenList);
	ListClear(&ongoing->damageTakenList);
	ListClear(&ongoing->teammatePositioningList);

	// save prev/next of the ongoing struct before memset overwrites it
	prev = ongoing->node.prev;
	next = ongoing->node.next;

	// clear out the ongoing struct
	memset(ongoing, 0, sizeof(stats_t));

	// restore prev/next of the ongoing struct
	ongoing->node.prev = prev;
	ongoing->node.next = next;

	// set some important parameters in the ongoing struct
	ongoing->sessionId = archive->sessionId;
	ongoing->accountId = archive->accountId;
	ongoing->isBot = archive->isBot;
	ongoing->clientNum = archive->clientNum;
	if (archive->accountName && ongoing->accountName)
		Q_strncpyz(ongoing->accountName, archive->accountName, sizeof(ongoing->accountName));
	if (archive->name && ongoing->name)
		Q_strncpyz(ongoing->name, archive->name, sizeof(ongoing->name));
	ongoing->lastTeam = archive->lastTeam;
	ongoing->lastPosition = archive->finalPosition;
	ongoing->confirmedPositionBits = archive->confirmedPositionBits;
	if (ongoing->lastPosition)
		ongoing->confirmedPositionBits |= (1 << ongoing->lastPosition);
	ongoing->blockNum = newBlockNum;
}

void CheckAccountsOfOldBlocks(int ignoreBlockNum) {
	if (g_gametype.integer != GT_CTF)
		return;

	iterator_t iter;
	ListIterate(&level.savedStatsList, &iter, qfalse);
	while (IteratorHasNext(&iter)) { // iterate through all of the archives
		stats_t *archive = IteratorNext(&iter);
		if (archive->blockNum == ignoreBlockNum)
			continue; // this is one of the ones we just copied, so we already know it's up to date

		int *archiveSessionId = &archive->sessionId;
		int *archiveAccountId = &archive->accountId;
		char *archiveAccountName = archive->accountName;
		char *archiveName = archive->name;

		iterator_t iter2;
		ListIterate(&level.statsList, &iter2, qfalse);
		while (IteratorHasNext(&iter2)) { // iterate through all of the ongoing stats
			stats_t *ongoing = IteratorNext(&iter2);
			int *ongoingSessionId = &ongoing->sessionId;
			int *ongoingAccountId = &ongoing->accountId;
			char *ongoingAccountName = ongoing->accountName;
			char *ongoingName = ongoing->name;

			if (*ongoingSessionId == *archiveSessionId) {
				if (*ongoingAccountId != *archiveAccountId) {
					// we matched session id, but account id is different
					// an admin must have assigned this person an account id since the end of this block
					*archiveAccountId = *ongoingAccountId;
					if (ongoingAccountName && archiveAccountName)
						Q_strncpyz(archiveAccountName, ongoingAccountName, MAX_NAME_LENGTH);
				}
				if (ongoingName && archiveName)
					Q_strncpyz(archiveName, ongoingName, MAX_NAME_LENGTH); // might as well match their ingame name too
				goto lick;
			}
		}
		lick:; // my fucking nuts
	}
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
		stats->blockNum = level.statBlock;
		if (cl->account && VALIDSTRING(cl->account->name))
			Q_strncpyz(stats->accountName, cl->account->name, sizeof(stats->accountName));
		else
			stats->accountName[0] = '\0';
		Q_strncpyz(stats->name, cl->pers.netname, sizeof(stats->name));
		Com_DebugPrintf("InitClientStats: using new stats ptr for %s %08X\n", cl->pers.netname, (unsigned int)stats);
	}
	else {
		if (stats->blockNum != level.statBlock) {
			if (StatsValid(stats)) {
				// sanity check: this guy reconnected after acquiring valid stats, and the block number has changed
				// this should not really be possible
				ArchiveAndResetStatsBlock(stats, level.statBlock);
			}
			else {
				// his stats weren't valid before, so just update his block number
				stats->blockNum = level.statBlock;
			}
		}
		Com_DebugPrintf("InitClientStats: reusing old stats ptr for %s %08X\n", cl->pers.netname, (unsigned int)stats);
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

typedef struct {
	session_t *session;
	team_t team;
} SessionAndTeamContext;

static qboolean SessionIdAndTeamMatchesStats(genericNode_t *node, void *userData) {
	stats_t *existing = (stats_t *)node;
	SessionAndTeamContext *thisGuy = (SessionAndTeamContext *)userData;

	if (!existing || !thisGuy)
		return qfalse;

	if (thisGuy->team != existing->lastTeam)
		return qfalse;

	if (thisGuy->session->accountId != ACCOUNT_ID_UNLINKED && thisGuy->session->accountId == existing->accountId)
		return qtrue; // matches a linked account

	if (existing->sessionId == thisGuy->session->id)
		return qtrue; // matches a session

	return qfalse;
}

// this is called when someone who has already been ingame (red/blue) changes to the other red/blue team
// we give them a new stats struct, allowing us to keep track of their stats on a per-team basis
// e.g. if they did 1000 damage while on red and 1000 damage on blue, we can show two separate players each doing 1000 damage on their respective teams,
// rather than showing one player did 2000 damage on his most recent team only
// in other words, if there are 8 total players but two of them swap teams with each other halfway through; level.statsList will have a size of 10
void ChangeStatsTeam(gclient_t *cl) {
	assert(cl);
	SessionAndTeamContext ctx;
	ctx.session = cl->session;
	ctx.team = cl->sess.sessionTeam;
	stats_t *stats = ListFind(&level.statsList, SessionIdAndTeamMatchesStats, &ctx, NULL);

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
		stats->lastTeam = cl->sess.sessionTeam;
		stats->blockNum = level.statBlock;
		Com_DebugPrintf("ChangeStatsTeam: assigned new stats ptr for %s %08X\n", cl->pers.netname, (unsigned int)stats);
	}
	else {
		if (stats->blockNum != level.statBlock) {
			if (StatsValid(stats)) {
				// sanity check: this guy reconnected after acquiring valid stats, and the block number has changed
				// this should not really be possible
				ArchiveAndResetStatsBlock(stats, level.statBlock);
			}
			else {
				// his stats weren't valid before, so just update his block number
				stats->blockNum = level.statBlock;
			}
		}
		Com_DebugPrintf("ChangeStatsTeam: reusing old stats ptr for %s %08X\n", cl->pers.netname, (unsigned int)stats);
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

// every five minutes, this function copies all of the stats structs into an archive and then clears out the stats structs
// thus, each player's stats_t represents their stats for the current five minute block only
// a player's total stats for the entire match can be calculated by adding together their current stats_t struct and all of their archived stats_t structs
void ChangeToNextStatsBlockIfNeeded(void) {
	if (g_gametype.integer != GT_CTF || level.intermissiontime || !level.wasRestarted || !level.numTeamTicks)
		return;

	// try to verify that this is a legit pug
	float avgRed = (float)level.numRedPlayerTicks / (float)level.numTeamTicks;
	float avgBlue = (float)level.numBluePlayerTicks / (float)level.numTeamTicks;
	int avgRedInt = (int)lroundf(avgRed);
	int avgBlueInt = (int)lroundf(avgBlue);
	int durationMins = (level.time - level.startTime) / 60000;
	if (durationMins >= 120 || avgRedInt != avgBlueInt || avgRedInt + avgBlueInt < 8 || fabs(avgRed - round(avgRed)) >= 0.1f || fabs(avgBlue - round(avgBlue)) >= 0.1f)
		return;

	int newBlockNum = (level.time - level.startTime) / STATS_BLOCK_DURATION_MS;
	if (newBlockNum <= level.statBlock)
		return; // we're already in the block number we should be in

	// time to move to the next block

	// finalize current positions
	FinalizeCTFPositions();

	// save copies of all current stats and then reset them
	iterator_t iter;
	ListIterate(&level.statsList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		stats_t *ongoing = IteratorNext(&iter);
		ArchiveAndResetStatsBlock(ongoing, newBlockNum);
	}

	// all stats are archived, but we still need to perform some housekeeping:
	// beginning with the third block, make sure the account number is correct in all of the archived blocks
	// in case an admin assigned someone an account since the end of that block
	if (newBlockNum > 1)
		CheckAccountsOfOldBlocks(level.statBlock);

	// finally, increment the global stat block index so we can track stats in the new block
	level.statBlock = newBlockNum;
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
		if (redFs && blueFs)
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