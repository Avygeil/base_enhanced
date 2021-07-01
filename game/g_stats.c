#include "g_local.h"
#include "bg_saga.h"
#include "g_database.h"

const char *CtfStatsTableCallback_Name(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return stats->name;
}

const char *CtfStatsTableCallback_Alias(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	if (stats->isBot)
		return "BOT";
	if (stats->accountId != ACCOUNT_ID_UNLINKED && stats->accountName[0])
		return stats->accountName;
	return NULL;
}

// gold if best overall; green if best on team; white/gray otherwise
const char *FormatStatInt(int num, int bestMyTeam, int bestOtherTeam) {
	if (!num && !bestMyTeam && !bestOtherTeam)
		return va("%d", num); // everybody has zero; no color
	if (num >= bestMyTeam && num >= bestOtherTeam)
		return va("^3%d", num);
	if (num >= bestMyTeam)
		return va("^2%d", num);
	return va("%d", num);
}

// gold if best overall; green if best on team; white/gray otherwise
const char *FormatStatTime(int num, int bestMyTeam, int bestOtherTeam) {
	int secs = num / 1000;
	int mins = secs / 60;

	// more or less than a minute?
	if (num >= 60000) {
		secs %= 60;
		if (!num && !bestMyTeam)
			return va("%dm%02ds", mins, secs);  // everybody has zero; no color
		if (num >= bestMyTeam && num >= bestOtherTeam)
			return va("^3%dm%02ds", mins, secs);
		if (num >= bestMyTeam)
			return va("^2%dm%02ds", mins, secs);
		return va("%dm%02ds", mins, secs);
	}
	else {
		if (!num && !bestMyTeam)
			return va("%ds", secs);  // everybody has zero; no color
		if (num >= bestMyTeam && num >= bestOtherTeam)
			return va("^3%ds", secs);
		if (num >= bestMyTeam)
			return va("^2%ds", secs);
		return va("%ds", secs);
	}
}

stats_t bestStats[TEAM_NUM_TEAMS] = { 0 }; // e.g. bestStats[TEAM_RED].captures contains the best captures of anyone that was on red team
const char *CtfStatsTableCallback_Captures(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->captures, bestStats[stats->lastTeam].captures, bestStats[OtherTeam(stats->lastTeam)].captures);
}

const char *CtfStatsTableCallback_Assists(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->assists, bestStats[stats->lastTeam].assists, bestStats[OtherTeam(stats->lastTeam)].assists);
}

const char *CtfStatsTableCallback_Defends(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->defends, bestStats[stats->lastTeam].defends, bestStats[OtherTeam(stats->lastTeam)].defends);
}

const char *CtfStatsTableCallback_Accuracy(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->accuracy, bestStats[stats->lastTeam].accuracy, bestStats[OtherTeam(stats->lastTeam)].accuracy);
}

const char *CtfStatsTableCallback_FcKills(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->fcKills, bestStats[stats->lastTeam].fcKills, bestStats[OtherTeam(stats->lastTeam)].fcKills);
}

const char *CtfStatsTableCallback_Rets(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->rets, bestStats[stats->lastTeam].rets, bestStats[OtherTeam(stats->lastTeam)].rets);
}

const char *CtfStatsTableCallback_BoonPickups(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->boonPickups, bestStats[stats->lastTeam].boonPickups, bestStats[OtherTeam(stats->lastTeam)].boonPickups);
}

const char *CtfStatsTableCallback_TotalHold(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatTime(stats->totalFlagHold, bestStats[stats->lastTeam].totalFlagHold, bestStats[OtherTeam(stats->lastTeam)].totalFlagHold);
}

const char *CtfStatsTableCallback_MaxHold(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatTime(stats->longestFlagHold, bestStats[stats->lastTeam].longestFlagHold, bestStats[OtherTeam(stats->lastTeam)].longestFlagHold);
}

const char *CtfStatsTableCallback_Saves(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->saves, bestStats[stats->lastTeam].saves, bestStats[OtherTeam(stats->lastTeam)].saves);
}

const char *CtfStatsTableCallback_DamageDealt(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->damageDealtTotal, bestStats[stats->lastTeam].damageDealtTotal, bestStats[OtherTeam(stats->lastTeam)].damageDealtTotal);
}

const char *CtfStatsTableCallback_DamageTaken(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->damageTakenTotal, bestStats[stats->lastTeam].damageTakenTotal, bestStats[OtherTeam(stats->lastTeam)].damageTakenTotal);
}

const char *CtfStatsTableCallback_TopSpeed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt((int)(stats->topSpeed + 0.5f), (int)(bestStats[stats->lastTeam].topSpeed + 0.5f), (int)(bestStats[OtherTeam(stats->lastTeam)].topSpeed + 0.5f));
}

const char *CtfStatsTableCallback_AverageSpeed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->averageSpeed, bestStats[stats->lastTeam].averageSpeed, bestStats[OtherTeam(stats->lastTeam)].averageSpeed);
}

const char *CtfStatsTableCallback_Push(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->push, bestStats[stats->lastTeam].push, bestStats[OtherTeam(stats->lastTeam)].push);
}

const char *CtfStatsTableCallback_Pull(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->pull, bestStats[stats->lastTeam].pull, bestStats[OtherTeam(stats->lastTeam)].pull);
}

const char *CtfStatsTableCallback_Healed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->healed, bestStats[stats->lastTeam].healed, bestStats[OtherTeam(stats->lastTeam)].healed);
}

const char *CtfStatsTableCallback_EnergizedAlly(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->energizedAlly, bestStats[stats->lastTeam].energizedAlly, bestStats[OtherTeam(stats->lastTeam)].energizedAlly);
}

const char *CtfStatsTableCallback_EnergizedEnemy(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->energizedEnemy, bestStats[stats->lastTeam].energizedEnemy, bestStats[OtherTeam(stats->lastTeam)].energizedEnemy);
}

const char *CtfStatsTableCallback_Absorbed(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->absorbed, bestStats[stats->lastTeam].absorbed, bestStats[OtherTeam(stats->lastTeam)].absorbed);
}

const char *CtfStatsTableCallback_ProtDamage(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatInt(stats->protDamageAvoided, bestStats[stats->lastTeam].protDamageAvoided, bestStats[OtherTeam(stats->lastTeam)].protDamageAvoided);
}

const char *CtfStatsTableCallback_ProtTime(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	stats_t *stats = rowContext;
	return FormatStatTime(stats->protTimeUsed, bestStats[stats->lastTeam].protTimeUsed, bestStats[OtherTeam(stats->lastTeam)].protTimeUsed);
}

#define CheckBest(field) do { if (player->field > best->field) { best->field = player->field; } }  while (0)

// we use this on every player to set up the bestStats structs
// this also sets each player's accuracy and averageSpeed
static void CheckBestStats(stats_t *player) {
	stats_t *best = &bestStats[player->lastTeam];
	CheckBest(score);
	CheckBest(captures);
	CheckBest(assists);
	CheckBest(defends);

	player->accuracy = player->accuracy_shots ? player->accuracy_hits * 100 / player->accuracy_shots : 0;
	CheckBest(accuracy);

	CheckBest(fcKills);
	CheckBest(rets);
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

	CheckBest(push);
	CheckBest(pull);
	CheckBest(healed);
	CheckBest(energizedAlly);
	CheckBest(energizedEnemy);
	CheckBest(absorbed);
	CheckBest(protDamageAvoided);
	CheckBest(protTimeUsed);
}

typedef struct {
	node_t		node;
	int			sessionId;
	qboolean	isBot;
} gotPlayer_t;

static qboolean PlayerMatches(genericNode_t *node, void *userData) {
	const gotPlayer_t *existing = (const gotPlayer_t *)node;
	const stats_t *thisGuy = (const stats_t *)userData;

	if (existing && thisGuy && thisGuy->sessionId == existing->sessionId && thisGuy->isBot == existing->isBot)
		return qtrue;

	return qfalse;
}

// try to prevent people who join and immediately go spec from showing up on stats
static qboolean StatsValid(const stats_t *stats) {
	int svFps = g_svfps.integer ? g_svfps.integer : 30; // prevent divide by zero
	if (stats->isBot || stats->displacementSamples / svFps >= 1 || stats->damageDealtTotal || stats->damageTakenTotal)
		return qtrue;
	return qfalse;
}

static void PrintTeamStats(const int id, char *outputBuffer, size_t outSize, qboolean announce, qboolean forcePowerStats) {
	Table *t = Table_Initialize(qtrue);
	list_t gotPlayersList = { 0 };
	memset(&bestStats, 0, sizeof(bestStats));
	team_t winningTeam = level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED] ? TEAM_BLUE : TEAM_RED; // red if tied to match scoreboard order
	team_t losingTeam = OtherTeam(winningTeam);
	int numWinningTeam = 0, numLosingTeam = 0;
	qboolean didHorizontalRule = qfalse;

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

				gotPlayer_t *add = ListAdd(&gotPlayersList, sizeof(gotPlayer_t));
				add->sessionId = found->sessionId;
				add->isBot = found->isBot;

				if (team == losingTeam && numWinningTeam && !didHorizontalRule) { // add a horizontal rule if there were winners and this is the first loser
					Table_AddHorizontalRule(t, losingTeam == TEAM_BLUE ? 4 : 1);
					didHorizontalRule = qtrue;
				}

				CheckBestStats(found);
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

			gotPlayer_t *add = ListAdd(&gotPlayersList, sizeof(gotPlayer_t));
			add->sessionId = found->sessionId;
			add->isBot = found->isBot;

			if (team == losingTeam && numWinningTeam && !didHorizontalRule) { // add a horizontal rule if there were winners and this is the first loser
				Table_AddHorizontalRule(t, losingTeam == TEAM_BLUE ? 4 : 1);
				didHorizontalRule = qtrue;
			}

			CheckBestStats(found);
			Table_DefineRow(t, found);

			if (team == winningTeam)
				++numWinningTeam;
			else
				++numLosingTeam;
		}
	}

	ListClear(&gotPlayersList);

	if (!numWinningTeam && !numLosingTeam) {
		Table_Destroy(t);
		return;
	}

	Table_DefineColumn(t, "^5NAME", CtfStatsTableCallback_Name, NULL, qtrue, -1, 32);
	Table_DefineColumn(t, "^5ALIAS", CtfStatsTableCallback_Alias, NULL, qtrue, -1, 32);

	if (!forcePowerStats) {
		Table_DefineColumn(t, "^5CAP", CtfStatsTableCallback_Captures, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5ASS", CtfStatsTableCallback_Assists, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5DEF", CtfStatsTableCallback_Defends, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5ACC", CtfStatsTableCallback_Accuracy, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5FCKIL", CtfStatsTableCallback_FcKills, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5RET", CtfStatsTableCallback_Rets, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5BOON", CtfStatsTableCallback_BoonPickups, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5TTLHOLD", CtfStatsTableCallback_TotalHold, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5MAXHOLD", CtfStatsTableCallback_MaxHold, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5SAVES", CtfStatsTableCallback_Saves, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5DMGDLT", CtfStatsTableCallback_DamageDealt, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5DMGTKN", CtfStatsTableCallback_DamageTaken, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5TOPSPD", CtfStatsTableCallback_TopSpeed, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5AVGSPD", CtfStatsTableCallback_AverageSpeed, NULL, qfalse, -1, 32);
	}
	else {
		Table_DefineColumn(t, "^5PUSH", CtfStatsTableCallback_Push, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5PULL", CtfStatsTableCallback_Pull, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5HEAL", CtfStatsTableCallback_Healed, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5NRGALLY", CtfStatsTableCallback_EnergizedAlly, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5NRGENEMY", CtfStatsTableCallback_EnergizedEnemy, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5ABSORB", CtfStatsTableCallback_Absorbed, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5PROTDMG", CtfStatsTableCallback_ProtDamage, NULL, qfalse, -1, 32);
		Table_DefineColumn(t, "^5PROTTIME", CtfStatsTableCallback_ProtTime, NULL, qfalse, -1, 32);
	}

#define TEMP_STATS_BUFFER_SIZE	(16384) // idk
	char *temp = malloc(TEMP_STATS_BUFFER_SIZE);
	size_t tempSize = TEMP_STATS_BUFFER_SIZE;
	Table_WriteToBuffer(t, temp, tempSize, qtrue, numWinningTeam ? (winningTeam == TEAM_BLUE ? 4 : 1) : (losingTeam == TEAM_BLUE ? 4 : 1));
	Table_Destroy(t);

	if (announce) PrintIngame(id, temp);
	if (outputBuffer) Q_strncpyz(outputBuffer, temp, outSize);

	free(temp);
}

static void PrintDamageChart(int printClientNum, char *outBuf, size_t outSize, qboolean announce) {
#ifdef NEW_TABLES
	Table *t = Table_Initialize(qtrue);

	// special case: the last red player gets a ^4| divider if there are blue players afterward
	qboolean gotRed = qfalse, gotBlue = qfalse;
	int lastRedClientNum = -1;
	for (int i = 0; i < level.numConnectedClients; i++) {
		int clientNum = level.sortedClients[i];
		if (level.clients[clientNum].sess.sessionTeam == TEAM_RED) {
			gotRed = qtrue;
			lastRedClientNum = clientNum;
		}
		else if (level.clients[clientNum].sess.sessionTeam == TEAM_BLUE) {
			gotBlue = qtrue;
		}
	}

	if (!gotRed && !gotBlue) {
		Table_Destroy(t);
		return;
	}

	// define each row
	int longestNameLen = 0;
	for (team_t team = TEAM_RED; team <= TEAM_BLUE; team++) {
		if (team == TEAM_RED && !gotRed)
			continue;
		else if (team == TEAM_BLUE && !gotBlue)
			continue;

		if (team == TEAM_BLUE && gotRed)
			Table_AddHorizontalRule(t, 4);

		for (int i = 0; i < level.numConnectedClients; i++) {
			int clientNum = level.sortedClients[i];
			if (level.clients[clientNum].sess.sessionTeam != team)
				continue;
			int len = Q_PrintStrlen(level.clients[clientNum].pers.netname);
			if (len > longestNameLen)
				longestNameLen = len;
			Table_DefineRow(t, &g_entities[clientNum]);
		}
	}

	// define each column
	Table_DefineColumn(t, "^5NAME", TableCallback_DamageName, NULL, qtrue, gotRed ? 1 : 4, 32);
	for (team_t team = TEAM_RED; team <= TEAM_BLUE; team++) {
		for (int i = 0; i < level.numConnectedClients; i++) {
			int clientNum = level.sortedClients[i];
			if (level.clients[clientNum].sess.sessionTeam != team)
				continue;
			char *clean = strdup(level.clients[clientNum].pers.netname);
			Q_CleanStr(clean);
			Q_strupr(clean);
			// do we need a minimum length check here for very short names?
			char *name = va("^5%s", clean);
			free(clean);
			Table_DefineColumn(t, name, TableCallback_Damage, &g_entities[clientNum], qfalse, (gotRed && gotBlue && clientNum == lastRedClientNum) ? 4 : -1, 32);
		}
	}

	// print the title above the first name column
	char buf[8192] = { 0 };
	for (int i = 0; i < longestNameLen + 3/*`space + pipe + space`*/; i++)
		Q_strcat(buf, sizeof(buf), " ");
	Q_strcat(buf, sizeof(buf), "^5DAMAGE DEALT TO\n");
	int len = strlen(buf);
	Table_WriteToBuffer(t, buf + len, sizeof(buf) - len, qtrue, gotRed ? 1 : 4);
	if (announce) PrintIngame(printClientNum, buf);
	if (outBuf) Q_strncpyz(outBuf, buf, outSize);
	Table_Destroy(t);
#else
	if (outBuf) *outBuf = '\0';
#endif
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
	case MODC_ROCKET: return "^5RKT";
	case MODC_CONCUSSION: return "^5CNC";
	case MODC_THERMAL: return "^5THR";
	case MODC_MINE: return "^5MIN";
	case MODC_DETPACK: return "^5DPK";
	case MODC_FORCE: return "^5FOR";
	case MODC_FALL: return "^5PIT";
	case MODC_TOTAL: return "^5TOTAL";
	default: assert(qfalse); return NULL;
	}
}

static void GetWeaponChart(int printClientNum, int statsPlayerClientNum, char *outBuf, size_t outSize, qboolean announce) {
#ifdef NEW_TABLES
	if (statsPlayerClientNum < 0 || statsPlayerClientNum >= MAX_CLIENTS || !g_entities[statsPlayerClientNum].inuse || level.clients[statsPlayerClientNum].pers.connected != CON_CONNECTED ||
		(level.clients[statsPlayerClientNum].sess.sessionTeam != TEAM_RED && level.clients[statsPlayerClientNum].sess.sessionTeam != TEAM_BLUE))
		return;

	char buf[8192] = { 0 };
	for (qboolean damageTaken = qfalse; damageTaken <= qtrue; damageTaken++) {
		Table *t = Table_Initialize(qtrue);

		int numRed = 0, numBlue = 0;


		// it would be visually more succinct to show the enemy team first, but it's more uniform to just always show red first regardless
		// list red team first
		for (int i = 0; i < level.numConnectedClients; i++) {
			int findClientNum = level.sortedClients[i];
			iterator_t iter;
			ListIterate(&level.statsList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				stats_t *found = IteratorNext(&iter);
				if (found->clientNum != findClientNum || found->lastTeam != TEAM_RED)
					continue;
				Table_DefineRow(t, found);
				++numRed;
			}
		}
		// sanity pass to make sure we got everyone
		iterator_t iter;
		ListIterate(&level.statsList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			stats_t *found = IteratorNext(&iter);
			if (found->lastTeam != TEAM_RED)
				continue;
			Table_DefineRow(t, found);
			++numRed;
		}

		// then list blue team
		if (numRed)
			Table_AddHorizontalRule(t, 4);
		for (int i = 0; i < level.numConnectedClients; i++) {
			int findClientNum = level.sortedClients[i];
			iterator_t iter;
			ListIterate(&level.statsList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				stats_t *found = IteratorNext(&iter);
				if (found->clientNum != findClientNum || found->lastTeam != TEAM_BLUE)
					continue;
				Table_DefineRow(t, found);
				++numBlue;
			}
		}
		// sanity pass to make sure we got everyone
		iterator_t iter;
		ListIterate(&level.statsList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			stats_t *found = IteratorNext(&iter);
			if (found->lastTeam != TEAM_BLUE)
				continue;
			Table_DefineRow(t, found);
			++numBlue;
		}

		if (!numRed && !numBlue) {
			Table_Destroy(t);
			return;
		}

		// define each column (one for names, and then a column for each player)
		Table_DefineColumn(t, damageTaken ? "^5ATTACKER" : "^5TARGET", TableCallback_WeaponName, NULL, qtrue, -1, 32);
		meansOfDeathCategoryContext_t contexts[MODC_MAX] = { 0 };
		for (meansOfDeathCategory_t modc = MODC_FIRST; modc < MODC_MAX; modc++) {
			contexts[modc].tablePlayerClientNum = statsPlayerClientNum;
			contexts[modc].damageTaken = damageTaken;
			contexts[modc].modc = modc;
			Table_DefineColumn(t, NameForMeansOfDeathCategory(modc), TableCallback_WeaponDamage, &contexts[modc], qfalse, -1, 32);
		}

		Q_strcat(buf, sizeof(buf), va("\n^5DAMAGE %s^5 BY ^7%s^7:\n", damageTaken ? "^1TAKEN" : "^2DEALT", level.clients[statsPlayerClientNum].pers.netname));
		int len = strlen(buf);
		Table_WriteToBuffer(t, buf + len, sizeof(buf) - len, qtrue, numRed ? 1 : 4);

		Table_Destroy(t);
	}
	if (announce) PrintIngame(printClientNum, buf);
	if (outBuf) Q_strncpyz(outBuf, buf, outSize);
#else
	if (outBuf) *outBuf = '\0';
#endif
}

void Stats_Print(gentity_t *ent, const char *type, char *outputBuffer, size_t outSize, qboolean announce, int weaponStatsClientNum) {
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
			PrintTeamStats(id, outputBuffer + len, outSize - len, announce, qfalse);
		}
		else {
			if (announce)
				PrintIngame(id, s);
			PrintTeamStats(id, NULL, 0, announce, qfalse);
		}
	}
	else if (!Q_stricmpn(type, "for", 3)) { // force power stats
		PrintTeamStats(id, outputBuffer, outSize, announce, qtrue);
	}
	else if (!Q_stricmpn(type, "dam", 3)) { // damage stats
		PrintDamageChart(id, outputBuffer, outSize, announce);
		return;
	}
	else if (!Q_stricmpn(type, "wea", 3)) { // weapon stats
		if (weaponStatsClientNum >= 0 && weaponStatsClientNum < MAX_CLIENTS) { // a single client
			GetWeaponChart(id, weaponStatsClientNum, outputBuffer, outSize, announce);
		}
		else { // everyone
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (g_entities[i].inuse && level.clients[i].pers.connected == CON_CONNECTED &&
					(level.clients[i].sess.sessionTeam == TEAM_RED || level.clients[i].sess.sessionTeam == TEAM_BLUE)) {
					GetWeaponChart(id, i, outputBuffer, outSize, announce);
				}
			}
		}
		return;
	}
	else {
		if (id != -1) {
			if (!Q_stricmp(type, "help"))
				trap_SendServerCommand(id, "print \""S_COLOR_WHITE"Usage: "S_COLOR_CYAN"/ctfstats <general | force | damage | weapon [player]>\n\"");
			else
				trap_SendServerCommand(id, va("print \""S_COLOR_WHITE"Unknown type '%s"S_COLOR_WHITE"'. Usage: "S_COLOR_CYAN"/ctfstats <general | force | damage | weapon [player]>\n\"", type));
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

int *GetDamageGivenStat(gclient_t *attacker, gclient_t *victim) {
	damageCounter_t *dmg = ListFind(&attacker->stats->damageTakenList, MatchesDamage, victim->stats, NULL);

	if (dmg) { // this guy is already tracked
		if (!dmg->otherPlayerIsBot)
			dmg->otherPlayerAccountId = victim->session->accountId; // update the tracked account id, in case an admin assigned him an account during this match
	}
	else { // not yet tracked; add him to the list
		dmg = ListAdd(&attacker->stats->damageTakenList, sizeof(damageCounter_t));
		dmg->otherPlayerIsBot = !!(g_entities[victim - level.clients].r.svFlags & SVF_BOT);
		if (dmg->otherPlayerIsBot) {
			dmg->otherPlayerSessionId = victim - level.clients;
		}
		else {
			dmg->otherPlayerSessionId = victim->session->id;
			dmg->otherPlayerAccountId = victim->session->accountId;
		}
	}

	return &dmg->totalAmount;
}

int *GetDamageGivenStatOfType(gclient_t *attacker, gclient_t *victim, meansOfDeath_t mod) {
	damageCounter_t *dmg = ListFind(&attacker->stats->damageTakenList, MatchesDamage, victim->stats, NULL);

	if (dmg) { // this guy is already tracked
		if (!dmg->otherPlayerIsBot)
			dmg->otherPlayerAccountId = victim->session->accountId; // update the tracked account id, in case an admin assigned him an account during this match
	}
	else { // not yet tracked; add him to the list
		dmg = ListAdd(&attacker->stats->damageTakenList, sizeof(damageCounter_t));
		dmg->otherPlayerIsBot = !!(g_entities[victim - level.clients].r.svFlags & SVF_BOT);
		if (dmg->otherPlayerIsBot) {
			dmg->otherPlayerSessionId = victim - level.clients;
		}
		else {
			dmg->otherPlayerSessionId = victim->session->id;
			dmg->otherPlayerAccountId = victim->session->accountId;
		}
	}

	return &dmg->ofType[mod];
}

int *GetDamageTakenStat(gclient_t *attacker, gclient_t *victim) {
	damageCounter_t *dmg = ListFind(&victim->stats->damageTakenList, MatchesDamage, attacker->stats, NULL);

	if (dmg) { // this guy is already tracked
		if (!dmg->otherPlayerIsBot)
			dmg->otherPlayerAccountId = attacker->session->accountId; // update the tracked account id, in case an admin assigned him an account during this match
	}
	else { // not yet tracked; add him to the list
		dmg = ListAdd(&victim->stats->damageTakenList, sizeof(damageCounter_t));
		dmg->otherPlayerIsBot = !!(g_entities[attacker - level.clients].r.svFlags & SVF_BOT);
		if (dmg->otherPlayerIsBot) {
			dmg->otherPlayerSessionId = attacker - level.clients;
		}
		else {
			dmg->otherPlayerSessionId = attacker->session->id;
			dmg->otherPlayerAccountId = attacker->session->accountId;
		}
	}

	return &dmg->totalAmount;
}

int *GetDamageTakenStatOfType(gclient_t *attacker, gclient_t *victim, meansOfDeath_t mod) {
	damageCounter_t *dmg = ListFind(&victim->stats->damageTakenList, MatchesDamage, attacker->stats, NULL);

	if (dmg) { // this guy is already tracked
		if (!dmg->otherPlayerIsBot)
			dmg->otherPlayerAccountId = attacker->session->accountId; // update the tracked account id, in case an admin assigned him an account during this match
	}
	else { // not yet tracked; add him to the list
		dmg = ListAdd(&victim->stats->damageTakenList, sizeof(damageCounter_t));
		dmg->otherPlayerIsBot = !!(g_entities[attacker - level.clients].r.svFlags & SVF_BOT);
		if (dmg->otherPlayerIsBot) {
			dmg->otherPlayerSessionId = attacker - level.clients;
		}
		else {
			dmg->otherPlayerSessionId = attacker->session->id;
			dmg->otherPlayerAccountId = attacker->session->accountId;
		}
	}

	return &dmg->ofType[mod];
}