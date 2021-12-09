#include "g_local.h"
#include "g_database.h"

//#define DEBUG_GENERATETEAMS // uncomment to set players to be put into teams with z_debugX cvars
//#define DEBUG_GENERATETEAMS_PRINT // uncomment to get lots of debug messages (takes a lot longer)
//#define DEBUG_GENERATETEAMS_PRINTALL // uncomment to get debug messages even for invalid combinations

#ifdef DEBUG_GENERATETEAMS_PRINT
#define TeamGen_DebugPrintf(...)	Com_Printf(__VA_ARGS__)
#else
#define TeamGen_DebugPrintf(...)	do {} while (0)
#endif

#ifdef DEBUG_GENERATETEAMS_PRINTALL
#define TeamGen_DebugPrintAllPrintf(...)	Com_Printf(__VA_ARGS__)
#else
#define TeamGen_DebugPrintAllPrintf(...)	do {} while (0)
#endif

#define CHASE_VERSUS_ENEMY_OFFENSE_THRESHOLD	(0.11)

double PlayerTierToRating(ctfPlayerTier_t tier) {
	switch (tier) {
	case PLAYERRATING_C: return 0.6;
	case PLAYERRATING_LOW_B: return 0.78;
	case PLAYERRATING_MID_B: return 0.8;
	case PLAYERRATING_HIGH_B: return 0.81;
	case PLAYERRATING_LOW_A: return 0.89;
	case PLAYERRATING_MID_A: return 0.9;
	case PLAYERRATING_HIGH_A: return 0.91;
	case PLAYERRATING_S: return 1.0;
	default: return 0.0;
	}
}

ctfPlayerTier_t PlayerTierFromRating(double num) {
	// stupid >= hack to account for imprecision
	if (num >= 1.0) return PLAYERRATING_S;
	if (num >= 0.91) return PLAYERRATING_HIGH_A;
	if (num >= 0.9) return PLAYERRATING_MID_A;
	if (num >= 0.89) return PLAYERRATING_LOW_A;
	if (num >= 0.81) return PLAYERRATING_HIGH_B;
	if (num >= 0.8) return PLAYERRATING_MID_B;
	if (num >= 0.78) return PLAYERRATING_LOW_B;
	if (num >= 0.6) return PLAYERRATING_C;
	return PLAYERRATING_UNRATED;
}

typedef struct {
	int accountId;
	char accountName[MAX_NAME_LENGTH];
	int clientNum;
	double rating[CTFPOSITION_OFFENSE + 1];
	ctfPosition_t preference;
	ctfPosition_t secondPreference;
} permutationPlayer_t;

static int SortTeamsInPermutationOfTeams(const void *a, const void *b) {
	teamData_t *aa = (teamData_t *)a;
	teamData_t *bb = (teamData_t *)b;
	if (aa->relativeStrength > bb->relativeStrength)
		return -1;
	if (bb->relativeStrength > aa->relativeStrength)
		return 1;
	return strcmp(aa->baseName, bb->baseName);
}

static void NormalizePermutationOfTeams(permutationOfTeams_t *p) {
	if (!p)
		return;

	// stronger team (or team with base name alphabetically first) goes first
	qsort(p->teams, 2, sizeof(teamData_t), SortTeamsInPermutationOfTeams);

	// offense player with name alphabetically first goes first
	for (int i = 0; i < 2; i++) {
		teamData_t *team = &p->teams[i];
		if (strcmp(team->offense1Name, team->offense2Name) > 0) {
			int temp = team->offenseId1;
			team->offenseId1 = team->offenseId2;
			team->offenseId2 = temp;
			char tempBuf[MAX_NAME_LENGTH] = { 0 };
			Q_strncpyz(tempBuf, team->offense1Name, MAX_NAME_LENGTH);
			Q_strncpyz(team->offense1Name, team->offense2Name, MAX_NAME_LENGTH);
			Q_strncpyz(team->offense2Name, tempBuf, MAX_NAME_LENGTH);
		}
	}
}

// note: normalize the permutation with NormalizePermutationOfTeams before calling this
static XXH32_hash_t HashTeam(permutationOfTeams_t *t) {
	if (!t)
		return 0;

	permutationOfTeams_t hashMe = { 0 };
	memcpy(&hashMe, t, sizeof(permutationOfTeams_t));
	//NormalizePermutationOfTeams(&hashMe);

	hashMe.valid = qfalse;
	hashMe.hash = 0;
	hashMe.numOnPreferredPos = 0.0;
	hashMe.topTierImbalance = 0;
	hashMe.bottomTierImbalance = 0;
	return XXH32(&hashMe, sizeof(permutationOfTeams_t), 0);
}

typedef struct {
	int guaranteedPlayersMask;
	permutationOfTeams_t *best;
	void *callback;
	qboolean enforceChaseRule;
	int numEligible;
	uint64_t numPermutations;
} teamGeneratorContext_t;

typedef void(*PermutationCallback)(teamGeneratorContext_t *, const permutationPlayer_t *, const permutationPlayer_t *,
	const permutationPlayer_t *, const permutationPlayer_t *,
	const permutationPlayer_t *, const permutationPlayer_t *,
	const permutationPlayer_t *, const permutationPlayer_t *);

static void TryTeamPermutation(teamGeneratorContext_t *context, const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase,
	const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase,
	const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {

	TeamGen_DebugPrintAllPrintf("%s/%s/%s/%s ||  %s/%s/%s/%s\n", team1base->accountName, team1chase->accountName, team1offense1->accountName, team1offense2->accountName,
		team2base->accountName, team2chase->accountName, team2offense1->accountName, team2offense2->accountName);

	if (!team1base->rating[CTFPOSITION_BASE] || !team1chase->rating[CTFPOSITION_CHASE] || !team1offense1->rating[CTFPOSITION_OFFENSE] || !team1offense2->rating[CTFPOSITION_OFFENSE] ||
		!team2base->rating[CTFPOSITION_BASE] || !team2chase->rating[CTFPOSITION_CHASE] || !team2offense1->rating[CTFPOSITION_OFFENSE] || !team2offense2->rating[CTFPOSITION_OFFENSE]) {
		return; // at least one player is invalid on their proposed position
	}

	// if there are players that are guaranteed to be included in the teams and this permutation doesn't have them, return
	if (context->guaranteedPlayersMask) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (!(context->guaranteedPlayersMask & (1 << i)))
				continue;

			if (team1base->clientNum != i && team1chase->clientNum != i && team1offense1->clientNum != i && team1offense2->clientNum != i &&
				team2base->clientNum != i && team2chase->clientNum != i && team2offense1->clientNum != i && team2offense2->clientNum != i) {
				return;
			}
		}
	}

	double team1RawStrength = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE] + team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
	double team2RawStrength = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE] + team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
	double total = team1RawStrength + team2RawStrength;
	double team1RelativeStrength = team1RawStrength / total;
	double team2RelativeStrength = team2RawStrength / total;
	double diff = fabs(team1RelativeStrength - team2RelativeStrength);

	int team1TopTiers = 0, team2TopTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) == PLAYERRATING_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) == PLAYERRATING_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_S) ++team1TopTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) == PLAYERRATING_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) == PLAYERRATING_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_S) ++team2TopTiers;
	int topTierImbalance = abs(team1TopTiers - team2TopTiers);

	int team1BottomTiers = 0, team2BottomTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) == PLAYERRATING_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) == PLAYERRATING_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) == PLAYERRATING_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) == PLAYERRATING_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	// reward permutations that put people on preferred positions (large bonus for #1 preferred; small bonus for #2 preferred)
	double numOnPreferredPos = 0;
	if (team1base->preference == CTFPOSITION_BASE) numOnPreferredPos += 1.0; else if (team1base->secondPreference == CTFPOSITION_BASE) numOnPreferredPos += 0.1;
	if (team1chase->preference == CTFPOSITION_CHASE) numOnPreferredPos += 1.0; else if (team1chase->secondPreference == CTFPOSITION_CHASE) numOnPreferredPos += 0.1;
	if (team1offense1->preference == CTFPOSITION_OFFENSE) numOnPreferredPos += 1.0; else if (team1offense1->secondPreference == CTFPOSITION_OFFENSE) numOnPreferredPos += 0.1;
	if (team1offense2->preference == CTFPOSITION_OFFENSE) numOnPreferredPos += 1.0; else if (team1offense2->secondPreference == CTFPOSITION_OFFENSE) numOnPreferredPos += 0.1;
	if (team2base->preference == CTFPOSITION_BASE) numOnPreferredPos += 1.0; else if (team2base->secondPreference == CTFPOSITION_BASE) numOnPreferredPos += 0.1;
	if (team2chase->preference == CTFPOSITION_CHASE) numOnPreferredPos += 1.0; else if (team2chase->secondPreference == CTFPOSITION_CHASE) numOnPreferredPos += 0.1;
	if (team2offense1->preference == CTFPOSITION_OFFENSE) numOnPreferredPos += 1.0; else if (team2offense1->secondPreference == CTFPOSITION_OFFENSE) numOnPreferredPos += 0.1;
	if (team2offense2->preference == CTFPOSITION_OFFENSE) numOnPreferredPos += 1.0; else if (team2offense2->secondPreference == CTFPOSITION_OFFENSE) numOnPreferredPos += 0.1;

	TeamGen_DebugPrintf("Regular:%s%s/%s%s/%s%s/%s%s vs. %s%s/%s%s/%s%s/%s%s^7 : %0.3f vs. %0.3f raw, %0.3f vs. %0.3f relative, %0.1f numOnPreferredPos, %0.3f total, %0.3f diff",
		team1base->preference == CTFPOSITION_BASE ? "^2" : (team1base->secondPreference == CTFPOSITION_BASE ? "^5" : "^7"),
		team1base->accountName,
		team1chase->preference == CTFPOSITION_CHASE ? "^2" : (team1chase->secondPreference == CTFPOSITION_CHASE ? "^5" : "^7"),
		team1chase->accountName,
		team1offense1->preference == CTFPOSITION_OFFENSE ? "^2" : (team1offense1->secondPreference == CTFPOSITION_OFFENSE ? "^5" : "^7"),
		team1offense1->accountName,
		team1offense2->preference == CTFPOSITION_OFFENSE ? "^2" : (team1offense2->secondPreference == CTFPOSITION_OFFENSE ? "^5" : "^7"),
		team1offense2->accountName,
		team2base->preference == CTFPOSITION_BASE ? "^2" : (team2base->secondPreference == CTFPOSITION_BASE ? "^5" : "^7"),
		team2base->accountName,
		team2chase->preference == CTFPOSITION_CHASE ? "^2" : (team2chase->secondPreference == CTFPOSITION_CHASE ? "^5" : "^7"),
		team2chase->accountName,
		team2offense1->preference == CTFPOSITION_OFFENSE ? "^2" : (team2offense1->secondPreference == CTFPOSITION_OFFENSE ? "^5" : "^7"),
		team2offense1->accountName,
		team2offense2->preference == CTFPOSITION_OFFENSE ? "^2" : (team2offense2->secondPreference == CTFPOSITION_OFFENSE ? "^5" : "^7"),
		team2offense2->accountName,
		team1RawStrength,
		team2RawStrength,
		team1RelativeStrength,
		team2RelativeStrength,
		numOnPreferredPos,
		total,
		diff);

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		double team1ChaseRating = team1chase->rating[CTFPOSITION_CHASE];
		double highestTeam2OffenseRating = team2offense1->rating[CTFPOSITION_OFFENSE] > team2offense2->rating[CTFPOSITION_OFFENSE] ? team2offense1->rating[CTFPOSITION_OFFENSE] : team2offense2->rating[CTFPOSITION_OFFENSE];
		if (highestTeam2OffenseRating - team1ChaseRating > CHASE_VERSUS_ENEMY_OFFENSE_THRESHOLD) {
			TeamGen_DebugPrintf(" unbalanced team 1 chase vs. team 2 offense (diff %0.3f)\n", highestTeam2OffenseRating - team1ChaseRating);
			return;
		}

		double team2ChaseRating = team2chase->rating[CTFPOSITION_CHASE];
		double highestTeam1OffenseRating = team1offense1->rating[CTFPOSITION_OFFENSE] > team1offense2->rating[CTFPOSITION_OFFENSE] ? team1offense1->rating[CTFPOSITION_OFFENSE] : team1offense2->rating[CTFPOSITION_OFFENSE];
		if (highestTeam1OffenseRating - team2ChaseRating > CHASE_VERSUS_ENEMY_OFFENSE_THRESHOLD) {
			TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense (diff %0.3f)\n", highestTeam1OffenseRating - team2ChaseRating);
			return;
		}
	}

	// this permutation will be favored over the previous permutation if:
	// - it is fairer
	// - it is equally fair, but has more people on preferred pos
	// - it is equally fair and has an equal number of people on preferred pos, but has better balance of bottom tier players
	// - it is equally fair and has an equal number of people on preferred pos and equal balance of bottom tier players, but has better balance of top tier players
	if (diff < context->best->diff ||
		(diff == context->best->diff && numOnPreferredPos > context->best->numOnPreferredPos) ||
		(diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
		(diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)) {
		if (diff < context->best->diff)
			TeamGen_DebugPrintf(" ^3best so far (fairer)^7\n");
		else if (diff == context->best->diff && numOnPreferredPos > context->best->numOnPreferredPos)
			TeamGen_DebugPrintf(" ^3best so far (same fairness, but more preferred pos)^7\n");
		else if (diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && bottomTierImbalance < context->best->bottomTierImbalance)
			TeamGen_DebugPrintf(" ^3best so far (same fairness and preferred pos, but better bottom tier balance)^7\n");
		else if (diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
			TeamGen_DebugPrintf(" ^3best so far (same fairness and preferred pos, and bottom tier balance, but better top tier balance)^7\n");
		else
			TeamGen_DebugPrintf("^1???\n");
		context->best->valid = qtrue;
		context->best->diff = diff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		context->best->teams[0].rawStrength = team1RawStrength;
		context->best->teams[1].rawStrength = team2RawStrength;
		context->best->teams[0].relativeStrength = team1RelativeStrength;
		context->best->teams[1].relativeStrength = team2RelativeStrength;
		context->best->teams[0].baseId = team1base->accountId;
		context->best->teams[0].chaseId = team1chase->accountId;
		context->best->teams[0].offenseId1 = team1offense1->accountId;
		context->best->teams[0].offenseId2 = team1offense2->accountId;
		context->best->teams[1].baseId = team2base->accountId;
		context->best->teams[1].chaseId = team2chase->accountId;
		context->best->teams[1].offenseId1 = team2offense1->accountId;
		context->best->teams[1].offenseId2 = team2offense2->accountId;
		for (int i = 0; i < 2; i++) {
			memset(context->best->teams[i].baseName, 0, MAX_NAME_LENGTH);
			memset(context->best->teams[i].chaseName, 0, MAX_NAME_LENGTH);
			memset(context->best->teams[i].offense1Name, 0, MAX_NAME_LENGTH);
			memset(context->best->teams[i].offense2Name, 0, MAX_NAME_LENGTH);
		}
		Q_strncpyz(context->best->teams[0].baseName, team1base->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[0].chaseName, team1chase->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[0].offense1Name, team1offense1->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[0].offense2Name, team1offense2->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[1].baseName, team2base->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[1].chaseName, team2chase->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[1].offense1Name, team2offense1->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[1].offense2Name, team2offense2->accountName, MAX_NAME_LENGTH);
	}
	else {
		TeamGen_DebugPrintf("\n");
	}
}

static void TryTeamPermutation_Tryhard(teamGeneratorContext_t *context, const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase,
	const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase,
	const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {

	TeamGen_DebugPrintAllPrintf("%s/%s/%s/%s ||  %s/%s/%s/%s\n", team1base->accountName, team1chase->accountName, team1offense1->accountName, team1offense2->accountName,
		team2base->accountName, team2chase->accountName, team2offense1->accountName, team2offense2->accountName);

	if (!team1base->rating[CTFPOSITION_BASE] || !team1chase->rating[CTFPOSITION_CHASE] || !team1offense1->rating[CTFPOSITION_OFFENSE] || !team1offense2->rating[CTFPOSITION_OFFENSE] ||
		!team2base->rating[CTFPOSITION_BASE] || !team2chase->rating[CTFPOSITION_CHASE] || !team2offense1->rating[CTFPOSITION_OFFENSE] || !team2offense2->rating[CTFPOSITION_OFFENSE]) {
		return; // at least one player is invalid on their proposed position
	}

	// if there are players that are guaranteed to be included in the teams and this permutation doesn't have them, return
	if (context->guaranteedPlayersMask) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (!(context->guaranteedPlayersMask & (1 << i)))
				continue;

			if (team1base->clientNum != i && team1chase->clientNum != i && team1offense1->clientNum != i && team1offense2->clientNum != i &&
				team2base->clientNum != i && team2chase->clientNum != i && team2offense1->clientNum != i && team2offense2->clientNum != i) {
				return;
			}
		}
	}

	double team1RawStrength = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE] + team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
	double team2RawStrength = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE] + team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
	double total = team1RawStrength + team2RawStrength;
	double team1RelativeStrength = team1RawStrength / total;
	double team2RelativeStrength = team2RawStrength / total;
	double diff = fabs(team1RelativeStrength - team2RelativeStrength);

	int team1TopTiers = 0, team2TopTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) == PLAYERRATING_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) == PLAYERRATING_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_S) ++team1TopTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) == PLAYERRATING_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) == PLAYERRATING_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_S) ++team2TopTiers;
	int topTierImbalance = abs(team1TopTiers - team2TopTiers);

	int team1BottomTiers = 0, team2BottomTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) == PLAYERRATING_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) == PLAYERRATING_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) == PLAYERRATING_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) == PLAYERRATING_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) == PLAYERRATING_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	// reward permutations that put people on preferred positions (large bonus for #1 preferred; small bonus for #2 preferred)
	double numOnPreferredPos = 0;
	if (team1base->preference == CTFPOSITION_BASE) numOnPreferredPos += 1.0; else if (team1base->secondPreference == CTFPOSITION_BASE) numOnPreferredPos += 0.1;
	if (team1chase->preference == CTFPOSITION_CHASE) numOnPreferredPos += 1.0; else if (team1chase->secondPreference == CTFPOSITION_CHASE) numOnPreferredPos += 0.1;
	if (team1offense1->preference == CTFPOSITION_OFFENSE) numOnPreferredPos += 1.0; else if (team1offense1->secondPreference == CTFPOSITION_OFFENSE) numOnPreferredPos += 0.1;
	if (team1offense2->preference == CTFPOSITION_OFFENSE) numOnPreferredPos += 1.0; else if (team1offense2->secondPreference == CTFPOSITION_OFFENSE) numOnPreferredPos += 0.1;
	if (team2base->preference == CTFPOSITION_BASE) numOnPreferredPos += 1.0; else if (team2base->secondPreference == CTFPOSITION_BASE) numOnPreferredPos += 0.1;
	if (team2chase->preference == CTFPOSITION_CHASE) numOnPreferredPos += 1.0; else if (team2chase->secondPreference == CTFPOSITION_CHASE) numOnPreferredPos += 0.1;
	if (team2offense1->preference == CTFPOSITION_OFFENSE) numOnPreferredPos += 1.0; else if (team2offense1->secondPreference == CTFPOSITION_OFFENSE) numOnPreferredPos += 0.1;
	if (team2offense2->preference == CTFPOSITION_OFFENSE) numOnPreferredPos += 1.0; else if (team2offense2->secondPreference == CTFPOSITION_OFFENSE) numOnPreferredPos += 0.1;

	TeamGen_DebugPrintf("Tryhard:%s%s/%s%s/%s%s/%s%s vs. %s%s/%s%s/%s%s/%s%s^7 : %0.3f vs. %0.3f raw, %0.3f vs. %0.3f relative, %0.1f numOnPreferredPos, %0.3f total, %0.3f diff",
		team1base->preference == CTFPOSITION_BASE ? "^2" : (team1base->secondPreference == CTFPOSITION_BASE ? "^5" : "^7"),
		team1base->accountName,
		team1chase->preference == CTFPOSITION_CHASE ? "^2" : (team1chase->secondPreference == CTFPOSITION_CHASE ? "^5" : "^7"),
		team1chase->accountName,
		team1offense1->preference == CTFPOSITION_OFFENSE ? "^2" : (team1offense1->secondPreference == CTFPOSITION_OFFENSE ? "^5" : "^7"),
		team1offense1->accountName,
		team1offense2->preference == CTFPOSITION_OFFENSE ? "^2" : (team1offense2->secondPreference == CTFPOSITION_OFFENSE ? "^5" : "^7"),
		team1offense2->accountName,
		team2base->preference == CTFPOSITION_BASE ? "^2" : (team2base->secondPreference == CTFPOSITION_BASE ? "^5" : "^7"),
		team2base->accountName,
		team2chase->preference == CTFPOSITION_CHASE ? "^2" : (team2chase->secondPreference == CTFPOSITION_CHASE ? "^5" : "^7"),
		team2chase->accountName,
		team2offense1->preference == CTFPOSITION_OFFENSE ? "^2" : (team2offense1->secondPreference == CTFPOSITION_OFFENSE ? "^5" : "^7"),
		team2offense1->accountName,
		team2offense2->preference == CTFPOSITION_OFFENSE ? "^2" : (team2offense2->secondPreference == CTFPOSITION_OFFENSE ? "^5" : "^7"),
		team2offense2->accountName,
		team1RawStrength,
		team2RawStrength,
		team1RelativeStrength,
		team2RelativeStrength,
		numOnPreferredPos,
		total,
		diff);

	if (diff > 0.10) {
		TeamGen_DebugPrintf(" difference too great.\n");
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		double team1ChaseRating = team1chase->rating[CTFPOSITION_CHASE];
		double highestTeam2OffenseRating = team2offense1->rating[CTFPOSITION_OFFENSE] > team2offense2->rating[CTFPOSITION_OFFENSE] ? team2offense1->rating[CTFPOSITION_OFFENSE] : team2offense2->rating[CTFPOSITION_OFFENSE];
		if (highestTeam2OffenseRating - team1ChaseRating > CHASE_VERSUS_ENEMY_OFFENSE_THRESHOLD) {
			TeamGen_DebugPrintf(" unbalanced team 1 chase vs. team 2 offense (diff %0.3f)\n", highestTeam2OffenseRating - team1ChaseRating);
			return;
		}

		double team2ChaseRating = team2chase->rating[CTFPOSITION_CHASE];
		double highestTeam1OffenseRating = team1offense1->rating[CTFPOSITION_OFFENSE] > team1offense2->rating[CTFPOSITION_OFFENSE] ? team1offense1->rating[CTFPOSITION_OFFENSE] : team1offense2->rating[CTFPOSITION_OFFENSE];
		if (highestTeam1OffenseRating - team2ChaseRating > CHASE_VERSUS_ENEMY_OFFENSE_THRESHOLD) {
			TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense (diff %0.3f)\n", highestTeam1OffenseRating - team2ChaseRating);
			return;
		}
	}

	double currentBestCombinedStrength = context->best->teams[0].rawStrength + context->best->teams[1].rawStrength;

	// this permutation will be favored over the previous permutation if:
	// - it is higher caliber overall
	// - it is equally high caliber, but fairer
	// - it is equally high caliber and equally fair, but has more people on preferred pos
	// - it is equally high caliber, equally fair, and has an equal number of people on preferred pos, but has better balance of bottom tier players
	// - it is equally high caliber, equally fair, has an equal number of people on preferred pos and equal balance of bottom tier players, but has better balance of top tier players
	if (total > currentBestCombinedStrength ||
		(total == currentBestCombinedStrength && diff < context->best->diff) ||
		(total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos > context->best->numOnPreferredPos) ||
		(total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
		(total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)) {
		if (total > currentBestCombinedStrength)
			TeamGen_DebugPrintf(" ^6best so far (combined strength better)^7\n");
		else if (total == currentBestCombinedStrength && diff < context->best->diff)
			TeamGen_DebugPrintf(" ^6best so far (combined strength equal, but fairer)^7\n");
		else if (total == currentBestCombinedStrength && numOnPreferredPos > context->best->numOnPreferredPos)
			TeamGen_DebugPrintf(" ^6best so far (combined strength and fairness equal, but more preferred pos)^7\n");
		else if (total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && bottomTierImbalance < context->best->bottomTierImbalance)
			TeamGen_DebugPrintf(" ^6best so far (combined strength, fairness, and preferred pos, but better bottom tier balance)^7\n");
		else if (total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
			TeamGen_DebugPrintf(" ^6best so far (combined strength, fairness, and preferred pos, and bottom tier balance, but better top tier balance)^7\n");
		else
			TeamGen_DebugPrintf("^1???\n");
		context->best->valid = qtrue;
		context->best->diff = diff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		context->best->teams[0].rawStrength = team1RawStrength;
		context->best->teams[1].rawStrength = team2RawStrength;
		context->best->teams[0].relativeStrength = team1RelativeStrength;
		context->best->teams[1].relativeStrength = team2RelativeStrength;
		context->best->teams[0].baseId = team1base->accountId;
		context->best->teams[0].chaseId = team1chase->accountId;
		context->best->teams[0].offenseId1 = team1offense1->accountId;
		context->best->teams[0].offenseId2 = team1offense2->accountId;
		context->best->teams[1].baseId = team2base->accountId;
		context->best->teams[1].chaseId = team2chase->accountId;
		context->best->teams[1].offenseId1 = team2offense1->accountId;
		context->best->teams[1].offenseId2 = team2offense2->accountId;
		for (int i = 0; i < 2; i++) {
			memset(context->best->teams[i].baseName, 0, MAX_NAME_LENGTH);
			memset(context->best->teams[i].chaseName, 0, MAX_NAME_LENGTH);
			memset(context->best->teams[i].offense1Name, 0, MAX_NAME_LENGTH);
			memset(context->best->teams[i].offense2Name, 0, MAX_NAME_LENGTH);
		}
		Q_strncpyz(context->best->teams[0].baseName, team1base->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[0].chaseName, team1chase->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[0].offense1Name, team1offense1->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[0].offense2Name, team1offense2->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[1].baseName, team2base->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[1].chaseName, team2chase->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[1].offense1Name, team2offense1->accountName, MAX_NAME_LENGTH);
		Q_strncpyz(context->best->teams[1].offense2Name, team2offense2->accountName, MAX_NAME_LENGTH);
	}
	else {
		TeamGen_DebugPrintf("\n");
	}
}

// we have a complete team; feed it to the callback for evaluation and comparison with the current best
static void HandleTeam(permutationPlayer_t *playerArray, int *arr, teamGeneratorContext_t *context, int team1Off1, int team1Off2, int team2Off1, int team2Off2) {
	permutationPlayer_t *team1basePlayer = playerArray + *(arr + 0);
	permutationPlayer_t *team1chasePlayer = playerArray + *(arr + 1);
	permutationPlayer_t *team1offense1Player = playerArray + *(arr + 4 + team1Off1);
	permutationPlayer_t *team1offense2Player = playerArray + *(arr + 4 + team1Off2);
	permutationPlayer_t *team2basePlayer = playerArray + *(arr + 2);
	permutationPlayer_t *team2chasePlayer = playerArray + *(arr + 3);
	permutationPlayer_t *team2offense1Player = playerArray + *(arr + 4 + team2Off1);
	permutationPlayer_t *team2offense2Player = playerArray + *(arr + 4 + team2Off2);
	PermutationCallback callback = (PermutationCallback)context->callback;
	callback(context, team1basePlayer, team1chasePlayer, team1offense1Player, team1offense2Player, team2basePlayer, team2chasePlayer, team2offense1Player, team2offense2Player);
	++context->numPermutations;
}

// we have an offense combination; iterate through each complete team
static void HandleOffenseCombination(permutationPlayer_t *playerArray, int *c, int t, int *arr, teamGeneratorContext_t *context) {
	permutationPlayer_t *offensePlayer1 = playerArray + *(arr + 4 + c[1]);
	permutationPlayer_t *offensePlayer2 = playerArray + *(arr + 4 + c[2]);
	permutationPlayer_t *offensePlayer3 = playerArray + *(arr + 4 + c[3]);
	permutationPlayer_t *offensePlayer4 = playerArray + *(arr + 4 + c[4]);

	if (!offensePlayer1->rating[CTFPOSITION_OFFENSE] || !offensePlayer2->rating[CTFPOSITION_OFFENSE] ||
		!offensePlayer3->rating[CTFPOSITION_OFFENSE] || !offensePlayer4->rating[CTFPOSITION_OFFENSE]) {
		return; // if any of these offenders are invalid, then we can save time by not bothering to check any teams with them since they will all be invalid
	}

	HandleTeam(playerArray, arr, context, c[1], c[2], c[3], c[4]);
	HandleTeam(playerArray, arr, context, c[1], c[3], c[2], c[4]);
	HandleTeam(playerArray, arr, context, c[1], c[4], c[2], c[3]);
	HandleTeam(playerArray, arr, context, c[2], c[3], c[1], c[4]);
	HandleTeam(playerArray, arr, context, c[2], c[4], c[1], c[3]);
	HandleTeam(playerArray, arr, context, c[3], c[4], c[1], c[2]);
}

// helper function so that GetOffenseCombinations can pick from the players who aren't already on defense
static void SwapDefendersToArrayStart(int *arr, int numEligible, int firstNumber, int secondNumber, int thirdNumber, int fourthNumber) {
	if (*(arr + 0) != firstNumber) {
		for (int *p = arr + 0 + 1; p <= arr + numEligible - 1; p++) {
			if (*p == firstNumber) {
				*p = *(arr + 0);
				*(arr + 0) = firstNumber;
				break;
			}
		}
	}
	if (*(arr + 1) != secondNumber) {
		for (int *p = arr + 1 + 1; p <= arr + numEligible - 1; p++) {
			if (*p == secondNumber) {
				*p = *(arr + 1);
				*(arr + 1) = secondNumber;
				break;
			}
		}
	}
	if (*(arr + 2) != thirdNumber) {
		for (int *p = arr + 2 + 1; p <= arr + numEligible - 1; p++) {
			if (*p == thirdNumber) {
				*p = *(arr + 2);
				*(arr + 2) = thirdNumber;
				break;
			}
		}
	}
	if (*(arr + 3) != fourthNumber) {
		for (int *p = arr + 3 + 1; p <= arr + numEligible - 1; p++) {
			if (*p == fourthNumber) {
				*p = *(arr + 3);
				*(arr + 3) = fourthNumber;
				break;
			}
		}
	}
}

// we have a defense permutation; iterate through every offense combination
static void GetOffenseCombinations(permutationPlayer_t *playerArray, int *arr, teamGeneratorContext_t *context, int team1base, int team1chase, int team2base, int team2chase) {
	permutationPlayer_t *team1basePlayer = playerArray + team1base;
	permutationPlayer_t *team1chasePlayer = playerArray + team1chase;
	permutationPlayer_t *team2basePlayer = playerArray + team2base;
	permutationPlayer_t *team2chasePlayer = playerArray + team2chase;
	if (!team1basePlayer->rating[CTFPOSITION_BASE] || !team1chasePlayer->rating[CTFPOSITION_CHASE] ||
		!team2basePlayer->rating[CTFPOSITION_BASE] || !team2chasePlayer->rating[CTFPOSITION_CHASE]) {
		return; // if any of these defenders are invalid, then we can save time by not bothering to check any combination of offenses with them since they will all be invalid
	}

	int n = context->numEligible - 4;
	int t = 4;
	int *c = (int *)malloc((t + 3) * sizeof(int));

	for (int j = 1; j <= t; j++) // 0 is unused
		c[j] = j - 1;
	c[t + 1] = n;
	c[t + 2] = 0;
	int j;

	SwapDefendersToArrayStart(arr, context->numEligible, team1base, team1chase, team2base, team2chase);

	for (;;) {
		HandleOffenseCombination(playerArray, c, t, arr, context);
		j = 1;
		while (c[j] + 1 == c[j + 1]) {
			c[j] = j - 1;
			++j;
		}
		if (j > t)
			break;
		++c[j];
	}
	free(c);
}

// we have a defense combination; iterate through every defense permutation
static void HandleDefenseCombination(permutationPlayer_t *playerArray, int *c, int t, int *accountNums, teamGeneratorContext_t *context) {
	int *arr = (int *)malloc(context->numEligible * sizeof(int));
	memcpy(arr, accountNums, context->numEligible * sizeof(int));
	GetOffenseCombinations(playerArray, arr, context, c[1], c[2], c[3], c[4]);
	GetOffenseCombinations(playerArray, arr, context, c[2], c[1], c[3], c[4]);
	GetOffenseCombinations(playerArray, arr, context, c[1], c[2], c[4], c[3]);
	GetOffenseCombinations(playerArray, arr, context, c[2], c[1], c[4], c[3]);
	GetOffenseCombinations(playerArray, arr, context, c[1], c[3], c[2], c[4]);
	GetOffenseCombinations(playerArray, arr, context, c[3], c[1], c[2], c[4]);
	GetOffenseCombinations(playerArray, arr, context, c[1], c[3], c[4], c[2]);
	GetOffenseCombinations(playerArray, arr, context, c[3], c[1], c[4], c[2]);
	GetOffenseCombinations(playerArray, arr, context, c[1], c[4], c[2], c[3]);
	GetOffenseCombinations(playerArray, arr, context, c[4], c[1], c[2], c[3]);
	GetOffenseCombinations(playerArray, arr, context, c[1], c[4], c[3], c[2]);
	GetOffenseCombinations(playerArray, arr, context, c[4], c[1], c[3], c[2]);
	free(arr);
}

// we have a list of players; iterate through every defense combination
static void GetDefenseCombinations(permutationPlayer_t *playerArray, int *accountNums, teamGeneratorContext_t *context) {
	int n = context->numEligible;
	int t = 4;

	int *c = (int *)malloc((t + 3) * sizeof(int));
	for (int j = 1; j <= t; j++) // 0 is unused
		c[j] = j - 1;
	c[t + 1] = n;
	c[t + 2] = 0;

	int j;

	for (;;) {
		HandleDefenseCombination(playerArray, c, t, accountNums, context);
		j = 1;
		while (c[j] + 1 == c[j + 1]) {
			c[j] = j - 1;
			++j;
		}
		if (j > t)
			break;
		++c[j];
	}
	free(c);
}

/*
Team Generator

Gets every unique configuration of teams given a list of players and their ratings/preferences.
Logic is as follows:

for each combination of 4 defenders:
	for each permutation of those 4 defenders (disregarding order of teams):
		for each combination of 4 offenses:
			for each permutation of those 4 offenses (disregarding order within each team):
				if (this > best) { best = this; }

Returns the number of valid permutations evaluated.
*/
static uint64_t PermuteTeams(permutationPlayer_t *playerArray, int numEligible, permutationOfTeams_t *bestOut, PermutationCallback callback, int guaranteedPlayersMask, qboolean enforceChaseRule) {
#ifdef DEBUG_GENERATETEAMS
	clock_t start = clock();
#endif

	assert(playerArray && bestOut);
	int *intArr = malloc(sizeof(int) * numEligible);
	for (int i = 0; i < numEligible; i++)
		*(intArr + i) = i;
	teamGeneratorContext_t context;
	context.best = bestOut;
	context.callback = callback;
	context.guaranteedPlayersMask = guaranteedPlayersMask;
	context.enforceChaseRule = enforceChaseRule;
	context.numEligible = numEligible;
	context.numPermutations = 0;
	GetDefenseCombinations(playerArray, intArr, &context);
	free(intArr);

#ifdef DEBUG_GENERATETEAMS
	clock_t end = clock();
	double computeTimePrint = ((double)(end - start)) / CLOCKS_PER_SEC;
	Com_Printf("Pass compute time: %0.2f seconds\n", computeTimePrint);
#endif

	return context.numPermutations;
}

qboolean PlayerRatingAccountIdMatches(genericNode_t *node, void *userData) {
	const playerRating_t *existing = (const playerRating_t *)node;
	const playerRating_t *thisGuy = (const playerRating_t *)userData;
	if (existing && thisGuy && existing->accountId == thisGuy->accountId)
		return qtrue;
	return qfalse;
}

qboolean MostPlayedPosMatches(genericNode_t *node, void *userData) {
	const mostPlayedPos_t *existing = (const mostPlayedPos_t *)node;
	const mostPlayedPos_t *thisGuy = (const mostPlayedPos_t *)userData;
	if (existing && thisGuy && existing->accountId == thisGuy->accountId)
		return qtrue;
	return qfalse;
}

// see if a player has a name like "base only" so that we can account for their position preference
static ctfPosition_t GetPositionPreferenceFromName(const char *rawName) {
	if (!VALIDSTRING(rawName))
		return CTFPOSITION_UNKNOWN;
	char clean[64] = { 0 };
	Q_strncpyz(clean, rawName, sizeof(clean));
	Q_StripColor(clean);
	Q_strlwr(clean);
	if (!VALIDSTRING(clean))
		return CTFPOSITION_UNKNOWN;

	if (!Q_stricmp(clean, "o"))
		return CTFPOSITION_OFFENSE;
	if (stristr(clean, "base"))
		return CTFPOSITION_BASE;
	if (stristr(clean, "chase"))
		return CTFPOSITION_CHASE;
	if (stristr(clean, "offense") || stristr(clean, "oonly") || stristr(clean, "o only") || stristr(clean, "permao") || stristr(clean, "perma o") || stristr(clean, "(o)"))
		return CTFPOSITION_OFFENSE;

	return CTFPOSITION_UNKNOWN;
}

static int SortClientsForTeamGenerator(const void *a, const void *b) {
	sortedClient_t *aa = (sortedClient_t *)a;
	sortedClient_t *bb = (sortedClient_t *)b;
	if (!aa->accountName[0] && !bb->accountName[0])
		return 0;
	if (!aa->accountName[0])
		return 1; // move non-existent dudes to the end
	if (!bb->accountName[0])
		return -1; // move non-existent dudes to the end
	return strcmp(aa->accountName, bb->accountName); // alphabetize
}

static unsigned int teamGenSeed = 0;
static ctfPosition_t ctfPosTiebreakerOrder[3] = { CTFPOSITION_BASE, CTFPOSITION_CHASE, CTFPOSITION_OFFENSE };
static qboolean teamGenInitialized = qfalse;
static void InitializeTeamGenerator(void) {
	if (teamGenInitialized)
		return;

	while (!teamGenSeed) {
		teamGenSeed = time(NULL);
		FisherYatesShuffle(&ctfPosTiebreakerOrder, 3, sizeof(ctfPosition_t)); // randomize tiebreaker order for people equally rated at multiple positions
	}

	teamGenInitialized = qtrue;
}

static qboolean GenerateTeams(pugProposal_t *set, permutationOfTeams_t *mostPlayed, permutationOfTeams_t *highestCaliber, permutationOfTeams_t *fairest, uint64_t *numPermutations) {
	assert(set);
	InitializeTeamGenerator();
#ifdef DEBUG_GENERATETEAMS
	clock_t start = clock();
#endif

	if (g_gametype.integer != GT_CTF)
		return qfalse;

	// refresh ratings from db
	G_DBGetPlayerRatings();

	// figure out who is eligible
	int numIngame = 0;
	qboolean eligible[MAX_CLIENTS] = { qfalse };
	for (int i = 0; i < MAX_CLIENTS; i++) {
#ifdef DEBUG_GENERATETEAMS
		account_t acc;
		if (!G_DBGetAccountByName(Cvar_VariableString(va("z_debug%d", i + 1)), &acc))
			continue;
		eligible[i] = qtrue;
#else
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || ent->client->pers.connected == CON_DISCONNECTED ||
			!ent->client->account || ent->client->sess.clientType != CLIENT_TYPE_NORMAL ||
			(IsRacerOrSpectator(ent) && IsSpecName(ent->client->pers.netname)))
			continue;

		qboolean thisPlayerIsPartOfSet = qfalse;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			sortedClient_t *cl = set->clients + j;
			if (!cl->accountName[0] || Q_stricmp(cl->accountName, ent->client->account->name))
				continue;
			thisPlayerIsPartOfSet = qtrue;
			break;
		}
		if (!thisPlayerIsPartOfSet)
			continue;

		qboolean someoneElseIngameSharesMyAccount = qfalse;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *other = &g_entities[j];
			if (other == ent || !other->inuse || !other->client || other->client->pers.connected == CON_DISCONNECTED)
				continue;

			if (other->client->account && other->client->account->id == ent->client->account->id) {
				someoneElseIngameSharesMyAccount = qtrue;
				break;
			}
		}
		if (someoneElseIngameSharesMyAccount)
			continue;

		playerRating_t findMe;
		findMe.accountId = ent->client->account->id;
		playerRating_t *found = ListFind(&level.ratingList, PlayerRatingAccountIdMatches, &findMe, NULL);
		if (!found)
			continue;

		eligible[i] = qtrue;
#endif
	}

	// tally up everyone that is eligible. if there are 8+ ingame, then specs are not eligible.
	// also note their name in case they have a position preference like "base only" in it
	int numEligible = 0;
	sortedClient_t sortedClients[MAX_CLIENTS];
	memset(sortedClients, 0, sizeof(sortedClients));
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!eligible[i])
			continue;

		gentity_t *ent = &g_entities[i];
#ifndef DEBUG_GENERATETEAMS
		if (numIngame >= 8 && IsRacerOrSpectator(ent)) {
			eligible[i] = qfalse;
			continue;
		}
#endif

		sortedClients[i].clientNum = i;
#ifdef DEBUG_GENERATETEAMS
		Q_strncpyz(sortedClients[i].accountName, Cvar_VariableString(va("z_debug%d", i + 1)), sizeof(sortedClients[i].accountName));
		sortedClients[i].preferredPosFromName = GetPositionPreferenceFromName(Cvar_VariableString(va("z_debugposname%d", i + 1)));
		account_t acc;
		G_DBGetAccountByName(Cvar_VariableString(va("z_debug%d", i + 1)), &acc);
		sortedClients[i].accountId = acc.id;
		sortedClients[i].team = TEAM_SPECTATOR;
#else
		Q_strncpyz(sortedClients[i].accountName, ent->client->account->name, sizeof(sortedClients[i].accountName));
		sortedClients[i].preferredPosFromName = GetPositionPreferenceFromName(ent->client->pers.netname);
		sortedClients[i].team = ent->client->sess.sessionTeam;
		sortedClients[i].accountId = ent->client->account->id;
#endif

		if (sortedClients[i].preferredPosFromName)
			TeamGen_DebugPrintf("%s has preferred position from name %s\n", sortedClients[i].accountName, NameForPos(sortedClients[i].preferredPosFromName));

		++numEligible;
		if (sortedClients[i].team == TEAM_RED || sortedClients[i].team == TEAM_BLUE)
			++numIngame;
	}

	if (numEligible < 8) {
		return qfalse;
	}

	// sort players irrespective of their client number so that they can't alter results by reconnecting in a different slot
	qsort(&sortedClients, MAX_CLIENTS, sizeof(sortedClient_t), SortClientsForTeamGenerator);

	// shuffle to avoid alphabetical bias
	srand(teamGenSeed);
	FisherYatesShuffle(&sortedClients, numEligible, sizeof(sortedClient_t));
	srand(time(NULL));

	// try to get the best possible teams using a few different approaches
	permutationOfTeams_t permutations[NUM_TEAMGENERATORTYPES] = { 0 };
	qboolean gotValid = qfalse;
	uint64_t gotten = 0llu;
	qboolean got5050 = qfalse;
	for (teamGeneratorType_t type = TEAMGENERATORTYPE_FIRST; type < NUM_TEAMGENERATORTYPES; type++) {
		if (type == TEAMGENERATORTYPE_FAIREST && got5050)
			continue; // we already found dead even teams, so don't bother

		permutationPlayer_t *players = calloc(numEligible, sizeof(permutationPlayer_t));
		int index = 0;
		int guaranteedPlayersMask = 0;
		for (int i = 0; i < numEligible; i++) {
			sortedClient_t *thisGuy = sortedClients + i;
			permutationPlayer_t *player = players + index++;

			// fetch some information about this player
			playerRating_t findMe = { 0 };
			findMe.accountId = thisGuy->accountId;
			playerRating_t *positionRatings = ListFind(&level.ratingList, PlayerRatingAccountIdMatches, &findMe, NULL);
			playerRating_t temp = { 0 };
			qboolean gotValidRatings;
			if (!positionRatings) { // if no ratings, assume they are C tier at everything
				positionRatings = &temp;
				player->rating[CTFPOSITION_BASE] = player->rating[CTFPOSITION_CHASE] = player->rating[CTFPOSITION_OFFENSE] =
					temp.rating[CTFPOSITION_BASE] = temp.rating[CTFPOSITION_CHASE] = temp.rating[CTFPOSITION_OFFENSE] = PlayerTierToRating(PLAYERRATING_C);
				gotValidRatings = qfalse;
			}
			else {
				gotValidRatings = qtrue;
			}

			mostPlayedPos_t findMe2 = { 0 };
			findMe2.accountId = findMe.accountId;
			mostPlayedPos_t *mostPlayedPositions = ListFind(&level.mostPlayedPositionsList, MostPlayedPosMatches, &findMe2, NULL);

			// determine what information we will feed the permutation evaluator based on which pass we're on
			player->accountId = findMe.accountId;
			Q_strncpyz(player->accountName, thisGuy->accountName, sizeof(player->accountName));
			player->clientNum = thisGuy->clientNum;
			if (type == TEAMGENERATORTYPE_MOSTPLAYED) {
				// get their most played and second most played pos
				if (mostPlayedPositions) {
					player->rating[mostPlayedPositions->mostPlayed] = positionRatings->rating[mostPlayedPositions->mostPlayed];
					player->rating[mostPlayedPositions->secondMostPlayed] = positionRatings->rating[mostPlayedPositions->secondMostPlayed];
					player->preference = mostPlayedPositions->mostPlayed;
					player->secondPreference = mostPlayedPositions->secondMostPlayed;
				}
				else {
					// we have no information about positions they have played. maybe it's a new account? try to fall back to their highest rated pos
					double highestRating = 0.0;
					ctfPosition_t highestPos = CTFPOSITION_UNKNOWN;
					if (gotValidRatings) {
						for (int j = 0; j < 3; j++) {
							ctfPosition_t pos = ctfPosTiebreakerOrder[j];
							if (positionRatings->rating[pos] >= highestRating) {
								highestRating = positionRatings->rating[pos];
								highestPos = pos;
							}
						}
					}
					if (highestPos) {
						player->rating[highestPos] = highestRating;
						player->preference = highestPos;

						// get their second highest rated pos
						double secondHighestRating = 0.0;
						ctfPosition_t secondHighestPos = CTFPOSITION_UNKNOWN;
						for (int j = 0; j < 3; j++) {
							ctfPosition_t pos = ctfPosTiebreakerOrder[j];
							if (pos == highestPos)
								continue;
							if (positionRatings->rating[pos] >= secondHighestRating) {
								secondHighestRating = positionRatings->rating[pos];
								secondHighestPos = pos;
							}
						}
						if (secondHighestPos) {
							player->rating[secondHighestPos] = secondHighestRating;
							player->secondPreference = secondHighestPos;
						}
					}
				}
			}
			else if (type == TEAMGENERATORTYPE_HIGHESTRATING) {
				// get their highest rated pos
				double highestRating = 0.0;
				ctfPosition_t highestPos = CTFPOSITION_UNKNOWN;
				if (gotValidRatings) {
					for (int j = 0; j < 3; j++) {
						ctfPosition_t pos = ctfPosTiebreakerOrder[j];
						if (positionRatings->rating[pos] >= highestRating) {
							highestRating = positionRatings->rating[pos];
							highestPos = pos;
						}
					}
				}
				if (highestPos) {
					player->rating[highestPos] = highestRating;
					player->preference = highestPos;

					// get their second highest rated pos
					double secondHighestRating = 0.0;
					ctfPosition_t secondHighestPos = CTFPOSITION_UNKNOWN;
					for (int j = 0; j < 3; j++) {
						ctfPosition_t pos = ctfPosTiebreakerOrder[j];
						if (pos == highestPos)
							continue;
						if (positionRatings->rating[pos] >= secondHighestRating) {
							secondHighestRating = positionRatings->rating[pos];
							secondHighestPos = pos;
						}
					}
					if (secondHighestPos) {
						player->rating[secondHighestPos] = secondHighestRating;
						player->secondPreference = secondHighestPos;
					}
				}
				else {
					// we have no ratings for this player. maybe it's a new account? try to fall back to their most played pos
					if (mostPlayedPositions) {
						player->rating[mostPlayedPositions->mostPlayed] = positionRatings->rating[mostPlayedPositions->mostPlayed];
						player->rating[mostPlayedPositions->secondMostPlayed] = positionRatings->rating[mostPlayedPositions->secondMostPlayed];
						player->preference = mostPlayedPositions->mostPlayed;
						player->secondPreference = mostPlayedPositions->secondMostPlayed;
					}
				}
			}
			else {
				// get every pos they play, with preference for their most played and second most played
				memcpy(player->rating, positionRatings->rating, sizeof(player->rating));

				if (mostPlayedPositions) {
					player->rating[mostPlayedPositions->mostPlayed] = positionRatings->rating[mostPlayedPositions->mostPlayed];
					player->rating[mostPlayedPositions->secondMostPlayed] = positionRatings->rating[mostPlayedPositions->secondMostPlayed];
					player->preference = mostPlayedPositions->mostPlayed;
					player->secondPreference = mostPlayedPositions->secondMostPlayed;
				}
				else {
					// we have no information about positions they have played. maybe it's a new account? try to fall back to their highest rated pos
					double highestRating = 0.0;
					ctfPosition_t highestPos = CTFPOSITION_UNKNOWN;
					if (gotValidRatings) {
						for (int j = 0; j < 3; j++) {
							ctfPosition_t pos = ctfPosTiebreakerOrder[j];
							if (positionRatings->rating[pos] >= highestRating) {
								highestRating = positionRatings->rating[pos];
								highestPos = pos;
							}
						}
					}
					if (highestPos) {
						player->rating[highestPos] = highestRating;
						player->preference = highestPos;

						// get their second highest rated pos
						double secondHighestRating = 0.0;
						ctfPosition_t secondHighestPos = CTFPOSITION_UNKNOWN;
						for (int j = 0; j < 3; j++) {
							ctfPosition_t pos = ctfPosTiebreakerOrder[j];
							if (pos == highestPos)
								continue;
							if (positionRatings->rating[pos] >= secondHighestRating) {
								secondHighestRating = positionRatings->rating[pos];
								secondHighestPos = pos;
							}
						}
						if (secondHighestPos) {
							player->rating[secondHighestPos] = secondHighestRating;
							player->secondPreference = secondHighestPos;
						}
					}
				}
			}

			// if they have a name like "base only" then try to get them in that position if possible
			if (thisGuy->preferredPosFromName && thisGuy->preferredPosFromName != player->preference) {
				player->secondPreference = player->preference;
				player->preference = thisGuy->preferredPosFromName;
			}

			// if possible, guarantee that all currently ingame players are included
			if (numIngame < 8 && (thisGuy->team == TEAM_RED || thisGuy->team == TEAM_BLUE))
				guaranteedPlayersMask |= (1 << thisGuy->clientNum);
		}

		// evaluate every possible permutation of teams for this teamgen type
		permutationOfTeams_t *thisPermutation = &permutations[type];
		thisPermutation->diff = 999999.999999;
		PermutationCallback callback;
		if (type == TEAMGENERATORTYPE_HIGHESTRATING)
			callback = TryTeamPermutation_Tryhard;
		else
			callback = TryTeamPermutation;
		TeamGen_DebugPrintf("^3==========Permuting teams with type %d==========^7\n", type);
		uint64_t thisGotten = PermuteTeams(&players[0], numEligible, thisPermutation, callback, guaranteedPlayersMask, qtrue);
		if (thisGotten > gotten)
			gotten = thisGotten;

		if (!thisPermutation->valid) {
			// if we fail, try this teamgen type again without enforcing the chase rule
			TeamGen_DebugPrintf("^8==========No valid permutation for type %d; trying again without chase rule==========^7\n", type);
			thisPermutation->diff = 999999.999999;
			thisGotten = PermuteTeams(&players[0], numEligible, thisPermutation, callback, guaranteedPlayersMask, qfalse);
			if (thisGotten > gotten)
				gotten = thisGotten;
		}

		free(players);

		if (!thisPermutation->valid) {
			// still couldn't get a valid permutation for this teamgen type, even without the chase rule
			TeamGen_DebugPrintf("^1==========No valid permutation for type %d.==========^7\n", type);
			continue;
		}

		if (!thisPermutation->diff)
			got5050 = qtrue; // if we find dead even teams in one of the first two passes, we can skip the third pass

		NormalizePermutationOfTeams(thisPermutation);
		thisPermutation->hash = HashTeam(thisPermutation);

		switch (type) {
		case TEAMGENERATORTYPE_MOSTPLAYED:
			if (mostPlayed) memcpy(mostPlayed, thisPermutation, sizeof(permutationOfTeams_t)); break;
		case TEAMGENERATORTYPE_HIGHESTRATING:
			if (highestCaliber) memcpy(highestCaliber, thisPermutation, sizeof(permutationOfTeams_t)); break;
		case TEAMGENERATORTYPE_FAIREST:
			if (fairest) memcpy(fairest, thisPermutation, sizeof(permutationOfTeams_t)); break;
		}
		gotValid = qtrue;
	}
	if (numPermutations)
		*numPermutations = gotten;

#ifdef DEBUG_GENERATETEAMS
	clock_t end = clock();
	double computeTimePrint = ((double)(end - start)) / CLOCKS_PER_SEC;
	Com_Printf("Total compute time: %0.2f seconds\n", computeTimePrint);
#endif

	return !!(gotValid);
}

// ignores everything aside from account id/name
XXH32_hash_t HashPugProposal(const sortedClient_t *clients) {
	if (!clients) {
		assert(qfalse);
		return 0;
	}

	sortedClient_t sortedClients[MAX_CLIENTS] = { 0 };

	for (int i = 0; i < MAX_CLIENTS; i++) {
		const sortedClient_t *src = clients + i;
		sortedClient_t *dst = &sortedClients[i];
		if (!src->accountName[0])
			continue;
		dst->accountId = src->accountId;
		Q_strncpyz(dst->accountName, src->accountName, sizeof(dst->accountName));
	}

	qsort(&sortedClients, MAX_CLIENTS, sizeof(sortedClient_t), SortClientsForTeamGenerator);

	return XXH32(&sortedClients, sizeof(sortedClients), 0);
}

void GetCurrentPickablePlayers(sortedClient_t *sortedClientsOut, int *numEligibleOut, qboolean shuffle) {
	// refresh ratings from db
	G_DBGetPlayerRatings();

	// figure out who is eligible
	int numIngame = 0;
	qboolean eligible[MAX_CLIENTS] = { qfalse };
	for (int i = 0; i < MAX_CLIENTS; i++) {
#ifdef DEBUG_GENERATETEAMS
		account_t acc;
		if (!G_DBGetAccountByName(Cvar_VariableString(va("z_debug%d", i + 1)), &acc))
			continue;
		eligible[i] = qtrue;
#else
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || ent->client->pers.connected == CON_DISCONNECTED ||
			!ent->client->account || ent->client->sess.clientType != CLIENT_TYPE_NORMAL ||
			(IsRacerOrSpectator(ent) && IsSpecName(ent->client->pers.netname)))
			continue;

		qboolean someoneElseIngameSharesMyAccount = qfalse;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *other = &g_entities[j];
			if (other == ent || !other->inuse || !other->client || other->client->pers.connected == CON_DISCONNECTED)
				continue;

			if (other->client->account && other->client->account->id == ent->client->account->id) {
				someoneElseIngameSharesMyAccount = qtrue;
				break;
			}
		}
		if (someoneElseIngameSharesMyAccount)
			continue;

		playerRating_t findMe;
		findMe.accountId = ent->client->account->id;
		playerRating_t *found = ListFind(&level.ratingList, PlayerRatingAccountIdMatches, &findMe, NULL);
		if (!found)
			continue;

		eligible[i] = qtrue;
#endif
	}

	// tally up everyone that is eligible. if there are 8+ ingame, then specs are not eligible.
	// also note their name in case they have a position preference like "base only" in it
	int numEligible = 0;
	sortedClient_t sortedClients[MAX_CLIENTS];
	memset(sortedClients, 0, sizeof(sortedClients));
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!eligible[i])
			continue;

		gentity_t *ent = &g_entities[i];
#ifndef DEBUG_GENERATETEAMS
		if (numIngame >= 8 && IsRacerOrSpectator(ent)) {
			eligible[i] = qfalse;
			continue;
		}
#endif

		sortedClients[i].clientNum = i;
#ifdef DEBUG_GENERATETEAMS
		Q_strncpyz(sortedClients[i].accountName, Cvar_VariableString(va("z_debug%d", i + 1)), sizeof(sortedClients[i].accountName));
		sortedClients[i].preferredPosFromName = GetPositionPreferenceFromName(Cvar_VariableString(va("z_debugposname%d", i + 1)));
		account_t acc;
		G_DBGetAccountByName(Cvar_VariableString(va("z_debug%d", i + 1)), &acc);
		sortedClients[i].accountId = acc.id;
		sortedClients[i].team = TEAM_SPECTATOR;
#else
		Q_strncpyz(sortedClients[i].accountName, ent->client->account->name, sizeof(sortedClients[i].accountName));
		sortedClients[i].preferredPosFromName = GetPositionPreferenceFromName(ent->client->pers.netname);
		sortedClients[i].team = ent->client->sess.sessionTeam;
		sortedClients[i].accountId = ent->client->account->id;
#endif

		if (sortedClients[i].preferredPosFromName)
			TeamGen_DebugPrintf("%s has preferred position from name %s\n", sortedClients[i].accountName, NameForPos(sortedClients[i].preferredPosFromName));

		++numEligible;
		if (sortedClients[i].team == TEAM_RED || sortedClients[i].team == TEAM_BLUE)
			++numIngame;
	}

	if (numEligibleOut)
		*numEligibleOut = numEligible;

	if (numEligible < 8) {
		return;
	}

	// sort players irrespective of their client number so that they can't alter results by reconnecting in a different slot
	qsort(&sortedClients, MAX_CLIENTS, sizeof(sortedClient_t), SortClientsForTeamGenerator);

	// shuffle to avoid alphabetical bias
	if (shuffle) {
		srand(teamGenSeed);
		FisherYatesShuffle(&sortedClients, numEligible, sizeof(sortedClient_t));
		srand(time(NULL));
	}

	if (sortedClientsOut)
		memcpy(sortedClientsOut, &sortedClients, sizeof(sortedClients));
}

static int lastNum = 0;

qboolean PugProposalMatchesHash(genericNode_t *node, void *userData) {
	const pugProposal_t *existing = (const pugProposal_t *)node;
	XXH32_hash_t hash = *((XXH32_hash_t *)userData);
	if (existing && existing->hash == hash)
		return qtrue;
	return qfalse;
}

qboolean PugProposalMatchesNum(genericNode_t *node, void *userData) {
	const pugProposal_t *existing = (const pugProposal_t *)node;
	int num = *((int *)userData);
	if (existing && existing->num == num)
		return qtrue;
	return qfalse;
}

static char *GetNamesStringForPugProposal(const pugProposal_t *players) {
	assert(players);
	static char buf[MAX_CLIENTS * 32] = { 0 };
	memset(buf, 0, sizeof(buf));

	qboolean gotOne = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		const sortedClient_t *cl = &players->clients[i];
		if (!cl->accountName[0])
			continue;

		qboolean isLast = qtrue;
		for (int j = i + 1; j < MAX_CLIENTS; j++) {
			const sortedClient_t *otherCl = &players->clients[j];
			if (otherCl->accountName[0]) {
				isLast = qfalse;
				break;
			}
		}

		Q_strcat(buf, sizeof(buf), va("%s%s%s", (gotOne ? ", " : ""), (gotOne && isLast ? "and " : ""), cl->accountName));
		gotOne = qtrue;
	}

	Q_StripColor(buf);
	return buf;
}

static void PrintTeamsProposalsInConsole(pugProposal_t *set) {
	assert(set);
	char formattedNumber[64] = { 0 };
	FormatNumberToStringWithCommas(set->numValidPermutationsChecked, formattedNumber, sizeof(formattedNumber));
	PrintIngame(-1, "Team generator results for %s\n(%s valid permutations evaluated):\n", set->namesStr, formattedNumber);

	int numPrinted = 0;
	char lastLetter = 'a';
	qboolean didFairest = qfalse;
	for (int i = 0; i < 3; i++) {
		permutationOfTeams_t *thisPermutation;
		switch (i) {
		case 0: thisPermutation = &set->suggested; break;
		case 1: thisPermutation = &set->highestCaliber; break;
		case 2: thisPermutation = &set->fairest; break;
		}
		if (!thisPermutation->valid)
			continue;
		if (i == 2 && didFairest)
			continue;

		char letter;
		char *suggestionTypeStr;
		if (!i) {
			if (thisPermutation->hash == set->highestCaliber.hash && (!thisPermutation->diff || thisPermutation->hash == set->fairest.hash || (set->fairest.valid && thisPermutation->diff == set->fairest.diff))) {
				suggestionTypeStr = "Suggested, highest caliber, and fairest";
				didFairest = qtrue;
			}
			else if (thisPermutation->hash == set->highestCaliber.hash) {
				suggestionTypeStr = "Suggested and highest caliber";
			}
			else if (!thisPermutation->diff || thisPermutation->hash == set->fairest.hash || (set->fairest.valid && thisPermutation->diff == set->fairest.diff)) {
				suggestionTypeStr = "Suggested and fairest";
				didFairest = qtrue;
			}
			else {
				suggestionTypeStr = "Suggested";
			}
			letter = set->suggestedLetter = lastLetter++;
		}
		else if (i == 1) {
			if (thisPermutation->hash == set->suggested.hash && set->suggested.valid) {
				thisPermutation->valid = qfalse;
				continue;
			}
			if (!thisPermutation->diff || (thisPermutation->hash == set->fairest.hash && set->fairest.valid) || (set->fairest.valid && thisPermutation->diff == set->fairest.diff)) {
				suggestionTypeStr = "Highest caliber and fairest";
				didFairest = qtrue;
			}
			else {
				suggestionTypeStr = "Highest caliber";
			}
			letter = set->highestCaliberLetter = lastLetter++;
		}
		else {
			if ((thisPermutation->hash == set->suggested.hash && set->suggested.valid) || (thisPermutation->hash == set->highestCaliber.hash && set->highestCaliber.valid)) {
				thisPermutation->valid = qfalse;
				continue;
			}
			suggestionTypeStr = "Fairest";
			letter = set->fairestLetter = lastLetter++;
		}

		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (!ent->inuse || !ent->client)
				continue;
			int accId = ent->client->account ? ent->client->account->id : -1;
			PrintIngame(i, "%s%s teams: ^5enter %c%c in chat if you approve\n^1Red team:^7 %.2f'/. relative strength\n    ^5Base: %s%s\n    ^6Chase: %s%s\n    ^2Offense: %s%s^7, %s%s^7\n^4Blue team:^7 %.2f'/. relative strength\n    ^5Base: %s%s\n    ^6Chase: %s%s\n    ^2Offense: %s%s^7, %s%s^7\n",
				numPrinted++ ? "\n" : "",
				suggestionTypeStr,
				TEAMGEN_CHAT_COMMAND_CHARACTER,
				letter,
				thisPermutation->teams[0].relativeStrength * 100.0,
				thisPermutation->teams[0].baseId == accId ? "^3" : "^7",
				thisPermutation->teams[0].baseName,
				thisPermutation->teams[0].chaseId == accId ? "^3" : "^7",
				thisPermutation->teams[0].chaseName,
				thisPermutation->teams[0].offenseId1 == accId ? "^3" : "^7",
				thisPermutation->teams[0].offense1Name,
				thisPermutation->teams[0].offenseId2 == accId ? "^3" : "^7",
				thisPermutation->teams[0].offense2Name,
				thisPermutation->teams[1].relativeStrength * 100.0,
				thisPermutation->teams[1].baseId == accId ? "^3" : "^7",
				thisPermutation->teams[1].baseName,
				thisPermutation->teams[1].chaseId == accId ? "^3" : "^7",
				thisPermutation->teams[1].chaseName,
				thisPermutation->teams[1].offenseId1 == accId ? "^3" : "^7",
				thisPermutation->teams[1].offense1Name,
				thisPermutation->teams[1].offenseId2 == accId ? "^3" : "^7",
				thisPermutation->teams[1].offense2Name);
		}
	}

	if (numPrinted == 3) {
		PrintIngame(-1,
			"Vote for a teams proposal by entering e.g. ^5%ca^7, ^5%cb^7, or ^5%cc^7\n"
			"You can approve of multiple teams proposals simultaneously by entering e.g. ^5%cab^7\n",
			TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER);
	}
	else if (numPrinted == 2) {
		PrintIngame(-1,
			"Vote for a teams proposal by entering e.g. ^5%ca^7 or ^5%cb^7\n"
			"You can approve of both teams proposals simultaneously by entering ^5%cab^7\n",
			TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER);
	}
	else if (numPrinted == 1) {
		PrintIngame(-1,
			"Vote for to approve the teams proposal by entering ^5%ca^7\n", TEAMGEN_CHAT_COMMAND_CHARACTER);
	}

	PrintIngame(-1,
		"You can vote to reroll the teams proposals by entering ^5%creroll^7\n"
		"You can vote to cancel the pug proposal by entering ^5%ccancel^7\n",
		TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER);
}

static void ActivatePugProposal(pugProposal_t *set) {
	assert(set);

	if (GenerateTeams(set, &set->suggested, &set->highestCaliber, &set->fairest, &set->numValidPermutationsChecked)) {
		SV_Say(va("Pug proposal %d passed (%s). Check console for teams proposals.", set->num, set->namesStr));
		set->passed = qtrue;
		level.activePugProposal = set;
		PrintTeamsProposalsInConsole(set);
	}
	else {
		SV_Say(va("Pug proposal %d passed (%s). Unable to generate teams; pug proposal %d terminated.", set->num, set->namesStr, set->num));
		level.activePugProposal = NULL;
		ListRemove(&level.pugProposalsList, set);
	}
}

void ActivateTeamsProposal(permutationOfTeams_t *permutation) {
	assert(permutation);

	const size_t messageSize = MAX_STRING_CHARS;
	char *printMessage = calloc(MAX_CLIENTS * messageSize, sizeof(char));

	qboolean forceteamed[MAX_CLIENTS] = { qfalse };
	for (int i = 0; i < 8; i++) {
		int accountNum, score;
		char *teamStr = i < 4 ? "r" : "b";
		ctfPosition_t pos;
		switch (i) {
		case 0: accountNum = permutation->teams[0].baseId; pos = CTFPOSITION_BASE; score = 8000;  break;
		case 1: accountNum = permutation->teams[0].chaseId; pos = CTFPOSITION_CHASE; score = 4000;  break;
		case 2: accountNum = permutation->teams[0].offenseId1; pos = CTFPOSITION_OFFENSE; score = 2000; break;
		case 3: accountNum = permutation->teams[0].offenseId2; pos = CTFPOSITION_OFFENSE; score = 1000; break;
		case 4: accountNum = permutation->teams[1].baseId; pos = CTFPOSITION_BASE; score = 8000; break;
		case 5: accountNum = permutation->teams[1].chaseId; pos = CTFPOSITION_CHASE; score = 4000; break;
		case 6: accountNum = permutation->teams[1].offenseId1; pos = CTFPOSITION_OFFENSE; score = 2000; break;
		case 7: accountNum = permutation->teams[1].offenseId2; pos = CTFPOSITION_OFFENSE; score = 1000; break;
		default: assert(qfalse); break;
		}
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *ent = &g_entities[j];
			if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || !ent->client->account || ent->client->account->id != accountNum)
				continue;

			G_SetRaceMode(ent, qfalse);
			if (ent->client->sess.canJoin) {
				SetTeam(ent, teamStr);
			}
			else {
				ent->client->sess.canJoin = qtrue;
				SetTeam(ent, teamStr);
				ent->client->sess.canJoin = qfalse;
			}
			forceteamed[j] = qtrue;

			Com_sprintf(printMessage + (j * messageSize), messageSize, "^1Red team:^7 (%0.2f'/. relative strength)\n", permutation->teams[0].relativeStrength * 100.0);
			Q_strcat(printMessage + (j * messageSize), messageSize, va("%s %s\n", permutation->teams[0].baseId == ent->client->account->id ? "^5Base: ^3" : "^5Base: ^7", permutation->teams[0].baseName));
			Q_strcat(printMessage + (j * messageSize), messageSize, va("%s %s\n", permutation->teams[0].chaseId == ent->client->account->id ? "^6Chase: ^3" : "^6Chase: ^7", permutation->teams[0].chaseName));
			Q_strcat(printMessage + (j * messageSize), messageSize, va("%s %s^7, ", permutation->teams[0].offenseId1 == ent->client->account->id ? "^2Offense: ^3" : "^2Offense: ^7", permutation->teams[0].offense1Name));
			Q_strcat(printMessage + (j * messageSize), messageSize, va("%s%s\n\n", permutation->teams[0].offenseId2 == ent->client->account->id ? "^3" : "^7", permutation->teams[0].offense2Name));
			Q_strcat(printMessage + (j * messageSize), messageSize, va("^4Blue team:^7 (%0.2f'/. relative strength)\n", permutation->teams[1].relativeStrength * 100.0));
			Q_strcat(printMessage + (j * messageSize), messageSize, va("%s %s\n", permutation->teams[1].baseId == ent->client->account->id ? "^5Base: ^3" : "^5Base: ^7", permutation->teams[1].baseName));
			Q_strcat(printMessage + (j * messageSize), messageSize, va("%s %s\n", permutation->teams[1].chaseId == ent->client->account->id ? "^6Chase: ^3" : "^6Chase: ^7", permutation->teams[1].chaseName));
			Q_strcat(printMessage + (j * messageSize), messageSize, va("%s %s^7, ", permutation->teams[1].offenseId1 == ent->client->account->id ? "^2Offense: ^3" : "^2Offense: ^7", permutation->teams[1].offense1Name));
			Q_strcat(printMessage + (j * messageSize), messageSize, va("%s%s\n\n", permutation->teams[1].offenseId2 == ent->client->account->id ? "^3" : "^7", permutation->teams[1].offense2Name));

			// silly little hack to put them on the scoreboard in the order we printed their names
			ent->client->ps.persistant[PERS_SCORE] = score;
		}
	}

	// force everyone else to spectator
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || forceteamed[i] || ent->client->sess.inRacemode)
			continue;
		if (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE) {
			G_SetRaceMode(ent, qfalse);
			if (ent->client->sess.canJoin) {
				SetTeam(ent, "s");
			}
			else {
				ent->client->sess.canJoin = qtrue;
				SetTeam(ent, "s");
				ent->client->sess.canJoin = qfalse;
			}
		}
		Com_sprintf(printMessage + (i * messageSize), messageSize, "^1Red team:^7 (%0.2f'/. relative strength)\n", permutation->teams[0].relativeStrength * 100.0);
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^5Base: ^7 %s\n", permutation->teams[0].baseName));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^6Chase: ^7 %s\n", permutation->teams[0].chaseName));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^2Offense: ^7 %s^7, ", permutation->teams[0].offense1Name));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("%s\n\n", permutation->teams[0].offense2Name));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^4Blue team:^7 (%0.2f'/. relative strength)\n", permutation->teams[1].relativeStrength * 100.0));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^5Base: ^7 %s\n", permutation->teams[1].baseName));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^6Chase: ^7 %s\n", permutation->teams[1].chaseName));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^2Offense: ^7 %s^7, ", permutation->teams[1].offense1Name));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^7%s\n\n", permutation->teams[1].offense2Name));
	}

	G_UniqueTickedCenterPrint(printMessage, messageSize, 30000, qtrue);
	free(printMessage);
}

qboolean TeamGenerator_VoteForTeamPermutations(gentity_t *ent, const char *voteStr, char **newMessage) {
	assert(ent && VALIDSTRING(voteStr));

	if (!ent->client->account) {
		SV_Tell(ent - g_entities, "You do not have an account, so you cannot vote for team proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if (!level.activePugProposal) {
		SV_Tell(ent - g_entities, "No pug proposal is currently active.");
		return qtrue;
	}

	char oldVotes[4] = { 0 };
	char newVotes[4] = { 0 };
	for (const char *p = voteStr; *p && p - voteStr < 3; p++) {
		const char lower = tolower((unsigned)*p);
		int *votesInt;
		permutationOfTeams_t *permutation;
		if (level.activePugProposal->suggested.valid && level.activePugProposal->suggestedLetter == lower) {
			votesInt = &level.activePugProposal->suggestedVoteClients;
			permutation = &level.activePugProposal->suggested;
		}
		else if (level.activePugProposal->highestCaliber.valid && level.activePugProposal->highestCaliberLetter == lower) {
			votesInt = &level.activePugProposal->highestCaliberVoteClients;
			permutation = &level.activePugProposal->highestCaliber;
		}
		else if (level.activePugProposal->fairest.valid && level.activePugProposal->fairestLetter == lower) {
			votesInt = &level.activePugProposal->fairestVoteClients;
			permutation = &level.activePugProposal->fairest;
		}
		else {
			SV_Tell(ent - g_entities, va("Invalid pug proposal letter '%c'.", *p));
			continue;
		}

		if (!permutation->valid) {
			SV_Tell(ent - g_entities, va("Invalid pug proposal letter '%c'.", *p));
			continue;
		}

		qboolean votedYesOnAnotherClient = qfalse;
		if (!(*votesInt & (1 << ent - g_entities))) {
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *other = &g_entities[i];
				if (other == ent || !other->inuse || !other->client || !other->client->account)
					continue;
				if (other->client->account->id != ent->client->account->id)
					continue;
				if (*votesInt & (1 << other - g_entities)) {
					votedYesOnAnotherClient = qtrue;
					break;
				}
			}
		}

		qboolean allowedToVote = qfalse;
		if (permutation->teams[0].baseId == ent->client->account->id || permutation->teams[0].chaseId == ent->client->account->id ||
			permutation->teams[0].offenseId1 == ent->client->account->id || permutation->teams[0].offenseId2 == ent->client->account->id ||
			permutation->teams[1].baseId == ent->client->account->id || permutation->teams[1].chaseId == ent->client->account->id ||
			permutation->teams[1].offenseId1 == ent->client->account->id || permutation->teams[1].offenseId2 == ent->client->account->id) {
			allowedToVote = qtrue;
		}

		if (!allowedToVote) {
			SV_Tell(ent - g_entities, va("You cannot vote on teams proposal %c because you are not part of it.", lower));
			continue;
		}

		if (votedYesOnAnotherClient || *votesInt & (1 << ent - g_entities)) { // already voted for this
			Q_strcat(oldVotes, sizeof(oldVotes), va("%c", lower));
		}
		else { // a new vote
			*votesInt |= (1 << (ent - g_entities));
			Q_strcat(newVotes, sizeof(newVotes), va("%c", lower));
		}
	}

	char oldVotesMessage[64] = { 0 };
	if (oldVotes[0])
		Com_sprintf(oldVotesMessage, sizeof(oldVotesMessage), "You have already voted for teams proposal%s %s.", strlen(oldVotes) > 1 ? "s" : "", oldVotes);

	if (!newVotes[0]) {
		if (oldVotes[0])
			SV_Tell(ent - g_entities, oldVotesMessage);
	}
	else {
		if (strlen(newVotes) > 1) {
			Com_Printf("%s^7 voted yes to teams proposals %s.\n", ent->client->pers.netname, voteStr);
			SV_Tell(ent - g_entities, va("Vote cast for teams proposals %s.%s%s", voteStr, oldVotes[0] ? " " : "", oldVotes[0] ? oldVotesMessage : ""));
		}
		else {
			Com_Printf("%s^7 voted yes to teams proposal %s.\n", ent->client->pers.netname, voteStr);
			SV_Tell(ent - g_entities, va("Vote cast for teams proposal %s.%s%s", voteStr, oldVotes[0] ? " " : "", oldVotes[0] ? oldVotesMessage : ""));
		}
	}

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%c%s  ^9", TEAMGEN_CHAT_COMMAND_CHARACTER, voteStr);
		for (int i = 0; i < 3; i++) {
			char letter;
			int *votesInt;
			permutationOfTeams_t *permutation;
			switch (i) {
			case 0: permutation = &level.activePugProposal->suggested; votesInt = &level.activePugProposal->suggestedVoteClients; letter = level.activePugProposal->suggestedLetter; break;
			case 1: permutation = &level.activePugProposal->highestCaliber; votesInt = &level.activePugProposal->highestCaliberVoteClients; letter = level.activePugProposal->highestCaliberLetter; break;
			case 2: permutation = &level.activePugProposal->fairest; votesInt = &level.activePugProposal->fairestVoteClients; letter = level.activePugProposal->fairestLetter; break;
			}

			if (!permutation->valid)
				continue;

			int numYesVotes = 0;
			for (int j = 0; j < MAX_CLIENTS; j++) {
				if (*votesInt & (1 << j))
					++numYesVotes;
			}

			const int numRequired = g_vote_teamgen_team_requiredVotes.integer ? g_vote_teamgen_team_requiredVotes.integer : 5;
			Q_strcat(buf, sizeof(buf), va(" (%c: %d/%d)", letter, numYesVotes, numRequired));
		}
		*newMessage = buf;
	}

	int tiebreakerOrder[] = { 0, 1, 2 };
	FisherYatesShuffle(&tiebreakerOrder[0], 3, sizeof(int));

	for (int i = 0; i < 3; i++) {
		int j = tiebreakerOrder[i];
		char letter;
		int *votesInt;
		permutationOfTeams_t *permutation;
		switch (j) {
		case 0: permutation = &level.activePugProposal->suggested; votesInt = &level.activePugProposal->suggestedVoteClients; letter = level.activePugProposal->suggestedLetter; break;
		case 1: permutation = &level.activePugProposal->highestCaliber; votesInt = &level.activePugProposal->highestCaliberVoteClients; letter = level.activePugProposal->highestCaliberLetter; break;
		case 2: permutation = &level.activePugProposal->fairest; votesInt = &level.activePugProposal->fairestVoteClients; letter = level.activePugProposal->fairestLetter; break;
		}

		if (!permutation->valid)
			continue;

		int numYesVotes = 0;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			if (*votesInt & (1 << j))
				++numYesVotes;
		}

		const int numRequired = g_vote_teamgen_team_requiredVotes.integer ? g_vote_teamgen_team_requiredVotes.integer : 5;

		if (numYesVotes >= numRequired) {
			SV_Say(va("Teams proposal %c passed.", letter));

			char printMessage[1024] = { 0 };
			Com_sprintf(printMessage, sizeof(printMessage), "*^1Red team:^7 (%0.2f'/. relative strength)\n", permutation->teams[0].relativeStrength * 100.0);
			Q_strcat(printMessage, sizeof(printMessage), va("^5Base: ^7 %s\n", permutation->teams[0].baseName));
			Q_strcat(printMessage, sizeof(printMessage), va("^6Chase: ^7 %s\n", permutation->teams[0].chaseName));
			Q_strcat(printMessage, sizeof(printMessage), va("^2Offense: ^7 %s^7, ", permutation->teams[0].offense1Name));
			Q_strcat(printMessage, sizeof(printMessage), va("%s\n\n", permutation->teams[0].offense2Name));
			Q_strcat(printMessage, sizeof(printMessage), va("^4Blue team:^7 (%0.2f'/. relative strength)\n", permutation->teams[1].relativeStrength * 100.0));
			Q_strcat(printMessage, sizeof(printMessage), va("^5Base: ^7 %s\n", permutation->teams[1].baseName));
			Q_strcat(printMessage, sizeof(printMessage), va("^6Chase: ^7 %s\n", permutation->teams[1].chaseName));
			Q_strcat(printMessage, sizeof(printMessage), va("^2Offense: ^7 %s^7, ", permutation->teams[1].offense1Name));
			Q_strcat(printMessage, sizeof(printMessage), va("^7%s\n\n", permutation->teams[1].offense2Name));
			PrintIngame(-1, printMessage);

			ActivateTeamsProposal(permutation);
			ListRemove(&level.pugProposalsList, level.activePugProposal);
			level.activePugProposal = NULL;
			break;
		}
	}

	return qfalse;
}

qboolean TeamGenerator_VoteYesToTeamCombination(gentity_t *ent, int num, pugProposal_t *setOptional, char **newMessage) {
	assert(ent && ent->client);

	if (!ent->client->account) {
		SV_Tell(ent - g_entities, "You do not have an account, so you cannot vote for pug proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if (!setOptional && num <= 0) {
		SV_Tell(ent - g_entities, "Invalid pug proposal number.");
		return qtrue;
	}

	// use the pointer if one was provided; otherwise, find the set matching the number provided
	pugProposal_t *set = NULL;
	set = setOptional ? setOptional : ListFind(&level.pugProposalsList, PugProposalMatchesNum, &num, NULL);

	if (!set) {
		SV_Tell(ent - g_entities, "Invalid pug proposal number.");
		return qtrue;
	}

	if (setOptional)
		SV_Tell(ent - g_entities, "A pug with these players has already been proposed. Changed your command into a vote for it.");

	if (set->passed) {
		SV_Tell(ent - g_entities, "This pug proposal has already passed.");
		return qtrue;
	}

	qboolean allowedToVote = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		sortedClient_t *cl = set->clients + i;
		if (cl->accountName[0] && cl->accountId == ent->client->account->id) {
			allowedToVote = qtrue;
			break;
		}
	}

	if (!allowedToVote) {
		SV_Tell(ent - g_entities, "You cannot vote on this pug proposal because you are not part of it.");
		return qtrue;
	}

	// check whether they have another client connected and voted yes on it
	qboolean votedYesOnAnotherClient = qfalse;
	if (!(set->votedYesClients & (1 << ent - g_entities))) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *other = &g_entities[i];
			if (other == ent || !other->inuse || !other->client || !other->client->account)
				continue;
			if (other->client->account->id != ent->client->account->id)
				continue;
			if (set->votedYesClients & (1 << other - g_entities)) {
				votedYesOnAnotherClient = qtrue;
				break;
			}
		}
	}

	if (votedYesOnAnotherClient || set->votedYesClients & (1 << ent - g_entities)) {
		SV_Tell(ent - g_entities, "You have already voted for this pug proposal.");
		return qfalse; // allow chat message for peer pressure
	}

	Com_Printf("%s^7 voted yes to pug proposal %d.\n", ent->client->pers.netname, set->num);
	SV_Tell(ent - g_entities, va("Vote cast for pug proposal %d.", set->num));
	set->votedYesClients |= (1 << (ent - g_entities));

	int numEligible = 0, numYesVotesFromEligiblePlayers = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		sortedClient_t *cl = set->clients + i;
		if (!cl->accountName[0])
			continue; // not included in this set
		++numEligible;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *other = &g_entities[j];
			if (!other->inuse || !other->client || !other->client->account || other->client->account->id != cl->accountId)
				continue;
			if (set->votedYesClients & (1 << j))
				++numYesVotesFromEligiblePlayers;
		}
	}

	const int numRequired = g_vote_teamgen_pug_requiredVotes.integer ? g_vote_teamgen_pug_requiredVotes.integer : 5;

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%c%d   ^9(%d/%d)", TEAMGEN_CHAT_COMMAND_CHARACTER, set->num, numYesVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	if (numYesVotesFromEligiblePlayers >= numRequired)
		ActivatePugProposal(set);

	return qfalse;
}

// returns qtrue if the message should be filtered out
qboolean TeamGenerator_PugStart(gentity_t *ent, char **newMessage) {
	if (g_gametype.integer != GT_CTF)
		return qfalse;

	assert(ent && ent->client);

	InitializeTeamGenerator();

	sortedClient_t clients[MAX_CLIENTS] = {0};
	int numEligible = 0;
	GetCurrentPickablePlayers(&clients[0], &numEligible, qfalse);

	if (numEligible < 8) {
		SV_Tell(ent - g_entities, "Not enough eligible players.");
		return qtrue;
	}

	XXH32_hash_t hash = HashPugProposal(&clients[0]);
	pugProposal_t *set = ListFind(&level.pugProposalsList, PugProposalMatchesHash, &hash, NULL);
	if (set) {
		// this person is attempting to start a pug with a combination that has already been proposed. redirect it to a yes vote on that combination.
		return TeamGenerator_VoteYesToTeamCombination(ent, 0, set, newMessage);
	}

	set = ListAdd(&level.pugProposalsList, sizeof(pugProposal_t));
	memcpy(set->clients, &clients, sizeof(set->clients));
	set->hash = hash;
	set->num = ++lastNum;
	set->votedYesClients = (1 << ent - g_entities);

	char *name = ent->client->account ? ent->client->account->name : ent->client->pers.netname;
	char cleanname[32];
	Q_strncpyz(cleanname, name, sizeof(cleanname));
	Q_StripColor(cleanname);

	char *namesStr = GetNamesStringForPugProposal(set);
	if (VALIDSTRING(namesStr))
		Q_strncpyz(set->namesStr, namesStr, sizeof(set->namesStr));
	SV_Say(va("%s proposes pug with: %s. Enter ^5%c%d^7 in chat if you approve.", cleanname, namesStr, TEAMGEN_CHAT_COMMAND_CHARACTER, set->num));
	return qfalse;
}

// returns qtrue if the message should be filtered out
qboolean TeamGenerator_VoteToReroll(gentity_t *ent, char **newMessage) {
	assert(ent && ent->client);

	if (!ent->client->account) {
		SV_Tell(ent - g_entities, "You do not have an account, so you cannot vote for pug proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if (!level.activePugProposal) {
		SV_Tell(ent - g_entities, "No pug proposal is currently active.");
		return qtrue;
	}

	qboolean allowedToVote = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		sortedClient_t *cl = &level.activePugProposal->clients[i];
		if (cl->accountName[0] && cl->accountId == ent->client->account->id) {
			allowedToVote = qtrue;
			break;
		}
	}

	if (!allowedToVote) {
		SV_Tell(ent - g_entities, "You cannot vote to reroll because you are not part of this pug proposal.");
		return qtrue;
	}

	qboolean votedToRerollOnAnotherClient = qfalse;
	if (!(level.activePugProposal->votedToRerollClients & (1 << ent - g_entities))) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *other = &g_entities[i];
			if (other == ent || !other->inuse || !other->client || !other->client->account)
				continue;
			if (other->client->account->id != ent->client->account->id)
				continue;
			if (level.activePugProposal->votedToRerollClients & (1 << other - g_entities)) {
				votedToRerollOnAnotherClient = qtrue;
				break;
			}
		}
	}

	if (votedToRerollOnAnotherClient || level.activePugProposal->votedToRerollClients & (1 << ent - g_entities)) {
		SV_Tell(ent - g_entities, "You have already voted to reroll the teams proposals.");
		return qfalse; // allow chat message for peer pressure
	}

	Com_Printf("%s^7 voted to reroll active pug proposal %d.\n", ent->client->pers.netname, level.activePugProposal->num);
	SV_Tell(ent - g_entities, "Vote cast to reroll teams proposals.");
	level.activePugProposal->votedToRerollClients |= (1 << (ent - g_entities));

	int numEligible = 0, numRerollVotesFromEligiblePlayers = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		sortedClient_t *cl = &level.activePugProposal->clients[i];
		if (!cl->accountName[0])
			continue; // not included in this set
		++numEligible;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *other = &g_entities[j];
			if (!other->inuse || !other->client || !other->client->account || other->client->account->id != cl->accountId)
				continue;
			if (level.activePugProposal->votedToRerollClients & (1 << j))
				++numRerollVotesFromEligiblePlayers;
		}
	}

	// require a simple majority of the total people in the proposal. requiring only 4 or 5 would allow e.g. in a set of 15 people for just a few people who were excluded to reroll it for everyone else
	const int numRequired = (numEligible / 2) + 1;

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%creroll   ^9(%d/%d)", TEAMGEN_CHAT_COMMAND_CHARACTER, numRerollVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	if (numRerollVotesFromEligiblePlayers >= numRequired) {
		teamGenSeed = time(NULL);
		level.activePugProposal->votedToRerollClients = 0;

		pugProposal_t oldHashes = { 0 };
		if (level.activePugProposal->suggested.valid) {
			oldHashes.suggested.valid = qtrue;
			oldHashes.suggested.hash = level.activePugProposal->suggested.hash;
		}
		if (level.activePugProposal->highestCaliber.valid) {
			oldHashes.highestCaliber.valid = qtrue;
			oldHashes.highestCaliber.hash = level.activePugProposal->highestCaliber.hash;
		}
		if (level.activePugProposal->fairest.valid) {
			oldHashes.fairest.valid = qtrue;
			oldHashes.fairest.hash = level.activePugProposal->fairest.hash;
		}

		if (GenerateTeams(level.activePugProposal, &level.activePugProposal->suggested, &level.activePugProposal->highestCaliber, &level.activePugProposal->fairest, &level.activePugProposal->numValidPermutationsChecked)) {
			// see if we actually got new teams, and try a new seed one more time if we didn't
			qboolean gotNewTeams = qfalse;
			if (level.activePugProposal->suggested.valid && oldHashes.suggested.valid && oldHashes.suggested.hash != level.activePugProposal->suggested.hash)
				gotNewTeams = qtrue;
			else if (level.activePugProposal->highestCaliber.valid && oldHashes.highestCaliber.valid && oldHashes.highestCaliber.hash != level.activePugProposal->highestCaliber.hash)
				gotNewTeams = qtrue;
			else if (level.activePugProposal->fairest.valid && oldHashes.fairest.valid && oldHashes.fairest.hash != level.activePugProposal->fairest.hash)
				gotNewTeams = qtrue;

			if (!gotNewTeams) {
				teamGenSeed = time(NULL) + 69420;
				GenerateTeams(level.activePugProposal, &level.activePugProposal->suggested, &level.activePugProposal->highestCaliber, &level.activePugProposal->fairest, &level.activePugProposal->numValidPermutationsChecked);
				if (level.activePugProposal->suggested.valid && oldHashes.suggested.valid && oldHashes.suggested.hash != level.activePugProposal->suggested.hash)
					gotNewTeams = qtrue;
				else if (level.activePugProposal->highestCaliber.valid && oldHashes.highestCaliber.valid && oldHashes.highestCaliber.hash != level.activePugProposal->highestCaliber.hash)
					gotNewTeams = qtrue;
				else if (level.activePugProposal->fairest.valid && oldHashes.fairest.valid && oldHashes.fairest.hash != level.activePugProposal->fairest.hash)
					gotNewTeams = qtrue;
			}

			if (gotNewTeams) {
				level.activePugProposal->suggestedVoteClients = level.activePugProposal->highestCaliberVoteClients = level.activePugProposal->fairestVoteClients = 0;
				SV_Say(va("Pug proposal %d rerolled (%s). Check console for new teams proposals.", level.activePugProposal->num, level.activePugProposal->namesStr));
				PrintTeamsProposalsInConsole(level.activePugProposal);
			}
			else {
				SV_Say(va("Failed to generate different teams when rerolling pug proposal %d (%s). Voting will continue for the existing teams.", level.activePugProposal->num, level.activePugProposal->namesStr));
			}
		}
		else {
			SV_Say(va("Pug proposal %d rerolled (%s). Unable to generate new teams; pug proposal %d terminated.", level.activePugProposal->num, level.activePugProposal->namesStr, level.activePugProposal->num));
			ListRemove(&level.pugProposalsList, level.activePugProposal);
			level.activePugProposal = NULL;
		}
	}

	return qfalse;
}

qboolean TeamGenerator_VoteToCancel(gentity_t *ent, char **newMessage) {
	assert(ent && ent->client);

	if (!ent->client->account) {
		SV_Tell(ent - g_entities, "You do not have an account, so you cannot vote for pug proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if (!level.activePugProposal) {
		SV_Tell(ent - g_entities, "No pug proposal is currently active.");
		return qtrue;
	}

	qboolean allowedToVote = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		sortedClient_t *cl = &level.activePugProposal->clients[i];
		if (cl->accountName[0] && cl->accountId == ent->client->account->id) {
			allowedToVote = qtrue;
			break;
		}
	}

	if (!allowedToVote) {
		SV_Tell(ent - g_entities, "You cannot vote to cancel because you are not part of this pug proposal.");
		return qtrue;
	}

	qboolean votedToCancelOnAnotherClient = qfalse;
	if (!(level.activePugProposal->votedToCancelClients & (1 << ent - g_entities))) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *other = &g_entities[i];
			if (other == ent || !other->inuse || !other->client || !other->client->account)
				continue;
			if (other->client->account->id != ent->client->account->id)
				continue;
			if (level.activePugProposal->votedToCancelClients & (1 << other - g_entities)) {
				votedToCancelOnAnotherClient = qtrue;
				break;
			}
		}
	}

	if (votedToCancelOnAnotherClient || level.activePugProposal->votedToCancelClients & (1 << ent - g_entities)) {
		SV_Tell(ent - g_entities, "You have already voted to cancel the teams proposals.");
		return qfalse; // allow chat message for peer pressure
	}

	Com_Printf("%s^7 voted to cancel active pug proposal %d.\n", ent->client->pers.netname, level.activePugProposal->num);
	SV_Tell(ent - g_entities, va("Vote cast to cancel pug proposal %d.", level.activePugProposal->num));
	level.activePugProposal->votedToCancelClients |= (1 << (ent - g_entities));

	int numEligible = 0, numCancelVotesFromEligiblePlayers = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		sortedClient_t *cl = &level.activePugProposal->clients[i];
		if (!cl->accountName[0])
			continue; // not included in this set
		++numEligible;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *other = &g_entities[j];
			if (!other->inuse || !other->client || !other->client->account || other->client->account->id != cl->accountId)
				continue;
			if (level.activePugProposal->votedToCancelClients & (1 << j))
				++numCancelVotesFromEligiblePlayers;
		}
	}

	// require a simple majority of the total people in the proposal. requiring only 4 or 5 would allow e.g. in a set of 15 people for just a few people who were excluded to cancel it for everyone else
	const int numRequired = (numEligible / 2) + 1;

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%ccancel   ^9(%d/%d)", TEAMGEN_CHAT_COMMAND_CHARACTER, numCancelVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	if (numCancelVotesFromEligiblePlayers >= numRequired) {
		SV_Say(va("Pug proposal %d canceled (%s).", level.activePugProposal->num, level.activePugProposal->namesStr));
		ListRemove(&level.pugProposalsList, level.activePugProposal);
		level.activePugProposal = NULL;
	}

	return qfalse;
}
