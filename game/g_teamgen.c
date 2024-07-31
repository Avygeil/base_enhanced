#include "g_local.h"
#include "g_database.h"

//#define DEBUG_GENERATETEAMS // uncomment to set players to be put into teams with z_debugX cvars
//#define DEBUG_GENERATETEAMS_PRINT // uncomment to generate log file

#ifdef DEBUG_GENERATETEAMS_PRINT
static fileHandle_t debugFile = 0;
static qboolean inReroll = qfalse;
#define TeamGen_DebugPrintf(...)	\
do {	\
if (debugFile && !inReroll) {	\
char *tempStr = va(__VA_ARGS__);	\
trap_FS_Write(tempStr, strlen(tempStr), debugFile);	\
}	\
} while (0)
#else
#define TeamGen_DebugPrintf(...)	do {} while (0)
#endif

static int rerollNum = 0;
static unsigned int teamGenSeed = 0u;

#ifdef _DEBUG
fileHandle_t teamGeneratorFile;
#endif

void TeamGen_Initialize(void) {
	static qboolean initialized = qfalse;
	if (initialized)
		return;
	teamGenSeed = time(NULL);
	initialized = qtrue;
}

#define PLAYERRATING_DECIMAL_INCREMENT	(0.05)

double PlayerTierToRating(ctfPlayerTier_t tier) {
	switch (tier) {
	case PLAYERRATING_MID_G: return 0.1;
	case PLAYERRATING_HIGH_G: return 0.15;
	case PLAYERRATING_LOW_F: return 0.2;
	case PLAYERRATING_MID_F: return 0.25;
	case PLAYERRATING_HIGH_F: return 0.3;
	case PLAYERRATING_LOW_D: return 0.35;
	case PLAYERRATING_MID_D: return 0.4;
	case PLAYERRATING_HIGH_D: return 0.45;
	case PLAYERRATING_LOW_C: return 0.5;
	case PLAYERRATING_MID_C: return 0.55;
	case PLAYERRATING_HIGH_C: return 0.6;
	case PLAYERRATING_LOW_B: return 0.65;
	case PLAYERRATING_MID_B: return 0.7;
	case PLAYERRATING_HIGH_B: return 0.75;
	case PLAYERRATING_LOW_A: return 0.8;
	case PLAYERRATING_MID_A: return 0.85;
	case PLAYERRATING_HIGH_A: return 0.9;
	case PLAYERRATING_LOW_S: return 0.95;
	case PLAYERRATING_MID_S: return 1.0;
	case PLAYERRATING_HIGH_S: return 1.05;
	default: return 0.0;
	}
}

ctfPlayerTier_t PlayerTierFromRating(double num) {
	// stupid >= hack to account for imprecision
	if (num >= 1.05 - 0.00001) return PLAYERRATING_HIGH_S;
	if (num >= 1.0 - 0.00001) return PLAYERRATING_MID_S;
	if (num >= 0.95 - 0.00001) return PLAYERRATING_LOW_S;
	if (num >= 0.90 - 0.00001) return PLAYERRATING_HIGH_A;
	if (num >= 0.85 - 0.00001) return PLAYERRATING_MID_A;
	if (num >= 0.80 - 0.00001) return PLAYERRATING_LOW_A;
	if (num >= 0.75 - 0.00001) return PLAYERRATING_HIGH_B;
	if (num >= 0.7 - 0.00001) return PLAYERRATING_MID_B;
	if (num >= 0.65 - 0.00001) return PLAYERRATING_LOW_B;
	if (num >= 0.6 - 0.0001) return PLAYERRATING_HIGH_C;
	if (num >= 0.55 - 0.0001) return PLAYERRATING_MID_C;
	if (num >= 0.5 - 0.0001) return PLAYERRATING_LOW_C;
	if (num >= 0.45 - 0.0001) return PLAYERRATING_HIGH_D;
	if (num >= 0.4 - 0.0001) return PLAYERRATING_MID_D;
	if (num >= 0.35 - 0.0001) return PLAYERRATING_LOW_D;
	if (num >= 0.3 - 0.0001) return PLAYERRATING_HIGH_F;
	if (num >= 0.25 - 0.0001) return PLAYERRATING_MID_F;
	if (num >= 0.2 - 0.0001) return PLAYERRATING_LOW_F;
	if (num >= 0.15 - 0.0001) return PLAYERRATING_HIGH_G;
	if (num >= 0.1 - 0.0001) return PLAYERRATING_MID_G;
	return PLAYERRATING_UNRATED;
}

char *PlayerRatingToString(ctfPlayerTier_t tier) {
	switch (tier) {
	case PLAYERRATING_MID_G: return "^0G";
	case PLAYERRATING_HIGH_G: return "^0HIGH G";
	case PLAYERRATING_LOW_F: return "^0LOW F";
	case PLAYERRATING_MID_F: return "^0F";
	case PLAYERRATING_HIGH_F: return "^0HIGH F";
	case PLAYERRATING_LOW_D: return "^1LOW D";
	case PLAYERRATING_MID_D: return "^1D";
	case PLAYERRATING_HIGH_D: return "^1HIGH D";
	case PLAYERRATING_LOW_C: return "^8LOW C";
	case PLAYERRATING_MID_C: return "^8C";
	case PLAYERRATING_HIGH_C: return "^8HIGH C";
	case PLAYERRATING_LOW_B: return "^3LOW B";
	case PLAYERRATING_MID_B: return "^3B";
	case PLAYERRATING_HIGH_B: return "^3HIGH B";
	case PLAYERRATING_LOW_A: return "^2LOW A";
	case PLAYERRATING_MID_A: return "^2A";
	case PLAYERRATING_HIGH_A: return "^2HIGH A";
	case PLAYERRATING_LOW_S: return "^6LOW S";
	case PLAYERRATING_MID_S: return "^6S";
	case PLAYERRATING_HIGH_S: return "^6HIGH S";
	default: return "^9UNRATED";
	}
}

#ifdef DEBUG_GENERATETEAMS_PRINT
static char *PlayerRatingToStringHTML(ctfPlayerTier_t tier) {
	switch (tier) {
	case PLAYERRATING_MID_G: return "<font color=red>G</font>";
	case PLAYERRATING_HIGH_G: return "<font color=red>HIGH G</font>";
	case PLAYERRATING_LOW_F: return "<font color=red>LOW F</font>";
	case PLAYERRATING_MID_F: return "<font color=red>F</font>";
	case PLAYERRATING_HIGH_F: return "<font color=red>HIGH F</font>";
	case PLAYERRATING_LOW_D: return "<font color=red>LOW D</font>";
	case PLAYERRATING_MID_D: return "<font color=red>D</font>";
	case PLAYERRATING_HIGH_D: return "<font color=red>HIGH D</font>";
	case PLAYERRATING_LOW_C: return "<font color=orange>LOW C</font>";
	case PLAYERRATING_MID_C: return "<font color=orange>C</font>";
	case PLAYERRATING_HIGH_C: return "<font color=orange>HIGH C</font>";
	case PLAYERRATING_LOW_B: return "<font color=gold>LOW B</font>";
	case PLAYERRATING_MID_B: return "<font color=gold>B</font>";
	case PLAYERRATING_HIGH_B: return "<font color=gold>HIGH B</font>";
	case PLAYERRATING_LOW_A: return "<font color=green>LOW A</font>";
	case PLAYERRATING_MID_A: return "<font color=green>A</font>";
	case PLAYERRATING_HIGH_A: return "<font color=green>HIGH A</font>";
	case PLAYERRATING_LOW_S: return "<font color=purple>LOW S</font>";
	case PLAYERRATING_MID_S: return "<font color=purple>S</font>";
	case PLAYERRATING_HIGH_S: return "<font color=purple>HIGH S</font>";
	default: return "<font color=black>UNRATED</font>";
	}
}
#endif

qboolean TeamGenerator_PlayerIsPermaBarredButTemporarilyForcedPickable(gentity_t *ent) {
	if (!ent || !ent->client || !ent->client->account)
		return qfalse;

	if (!(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) && !(ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) && !(ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL))
		return qfalse;

	qboolean forcedPickableByAdmin = qfalse;
	iterator_t iter;
	ListIterate(&level.forcedPickablePermabarredPlayersList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
		if (bp->accountId == ent->client->account->id)
			return qtrue;
	}

	return qfalse;
}

barReason_t TeamGenerator_PlayerIsBarredFromTeamGenerator(gentity_t *ent) {
	if (!ent || !ent->client)
		return BARREASON_NOTBARRED;

	if (ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL) && IsRacerOrSpectator(ent) && IsSpecName(ent->client->pers.netname))
		return BARREASON_NOTBARRED; // don't show him as barred, since he will show as spec anyway

	if (ent->client->pers.barredFromPugSelection)
		return ent->client->pers.barredFromPugSelection;

	if (ent->client->account) {
		if ((ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) || (ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) || (ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL)) {
			qboolean forcedPickableByAdmin = qfalse;
			iterator_t iter;
			ListIterate(&level.forcedPickablePermabarredPlayersList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
				if (bp->accountId == ent->client->account->id) {
					forcedPickableByAdmin = qtrue;
					break;
				}
			}
			if (forcedPickableByAdmin)
				return BARREASON_NOTBARRED;

			if (!(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) && ent->client->pers.permaBarredDeclaredPickable)
				return BARREASON_NOTBARRED;
			
			return BARREASON_BARREDBYACCOUNTFLAG;
		}

		iterator_t iter;
		ListIterate(&level.barredPlayersList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
			if (bp->accountId == ent->client->account->id)
				return bp->reason;
		}
	}

	return BARREASON_NOTBARRED;
}

typedef struct {
	int accountId;
	char accountName[MAX_NAME_LENGTH];
	int clientNum;
	double rating[CTFPOSITION_OFFENSE + 1];
	positionPreferences_t posPrefs;
	int numPermutationsIn;
	int numPermutationsInAndNonAvoidedPos;
	int numPermutationsInOnFirstChoicePos;
	qboolean notOnFirstChoiceInAbc;
} permutationPlayer_t;

static int SortTeamsInPermutationOfTeams(const void *a, const void *b) {
	teamData_t *aa = (teamData_t *)a;
	teamData_t *bb = (teamData_t *)b;
	if (aa->relativeStrength > bb->relativeStrength + 0.00001)
		return -1;
	if (bb->relativeStrength > aa->relativeStrength + 0.00001)
		return 1;
	return strcmp(aa->baseName, bb->baseName);
}

static void NormalizePermutationOfTeams(permutationOfTeams_t *p) {
	if (!p)
		return;

	// stronger team (or team with base name alphabetically first) goes first
	qsort(p->teams, 2, sizeof(teamData_t), SortTeamsInPermutationOfTeams);

	// offense player with name alphabetically first goes first within each team
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
static XXH32_hash_t HashPermutationOfTeams(permutationOfTeams_t *t) {
	if (!t)
		return 0;

	static int hashMe[8] = { 0 };
	memset(&hashMe, 0, sizeof(hashMe));

	hashMe[0] = t->teams[0].baseId;
	hashMe[1] = t->teams[0].chaseId;
	hashMe[2] = t->teams[0].offenseId1;
	hashMe[3] = t->teams[0].offenseId2;
	hashMe[4] = t->teams[1].baseId;
	hashMe[5] = t->teams[1].chaseId;
	hashMe[6] = t->teams[1].offenseId1;
	hashMe[7] = t->teams[1].offenseId2;

	return XXH32(&hashMe, sizeof(hashMe), 0);
}

typedef struct {
	permutationOfTeams_t *best;
	void *callback;
	qboolean enforceChaseRule;
	int numEligible;
	uint64_t numPermutations;
	list_t *avoidedHashesList;
	qboolean banAvoidedPositions;
	int type;
	qboolean enforceImbalanceCaps;
} teamGeneratorContext_t;

typedef struct {
	node_t			node;
	XXH32_hash_t	hash;
} avoidedHash_t;

qboolean HashMatchesAvoidedHash(genericNode_t *node, void *userData) {
	assert(userData);
	const avoidedHash_t *existing = (const avoidedHash_t *)node;
	const XXH32_hash_t thisHash = *((const XXH32_hash_t *)userData);
	if (existing && existing->hash == thisHash)
		return qtrue;
	return qfalse;
}

#define PREFERREDPOS_VALUE_FIRSTCHOICE		(100)
#define PREFERREDPOS_VALUE_SECONDCHOICE		(10)
#define PREFERREDPOS_VALUE_THIRDCHOICE		(1)

typedef void(*PermutationCallback)(teamGeneratorContext_t *, const permutationPlayer_t *, const permutationPlayer_t *,
	const permutationPlayer_t *, const permutationPlayer_t *,
	const permutationPlayer_t *, const permutationPlayer_t *,
	const permutationPlayer_t *, const permutationPlayer_t *);

static void TryTeamPermutation(teamGeneratorContext_t *context, const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase,
	const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase,
	const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {

	if (!team1base->rating[CTFPOSITION_BASE] || !team1chase->rating[CTFPOSITION_CHASE] || !team1offense1->rating[CTFPOSITION_OFFENSE] || !team1offense2->rating[CTFPOSITION_OFFENSE] ||
		!team2base->rating[CTFPOSITION_BASE] || !team2chase->rating[CTFPOSITION_CHASE] || !team2offense1->rating[CTFPOSITION_OFFENSE] || !team2offense2->rating[CTFPOSITION_OFFENSE]) {
		return; // at least one player is invalid on their proposed position
	}

	++context->numPermutations;

	if (context->banAvoidedPositions) {
		if ((team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) ||
			(team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) {
			return;
		}
	}

	double team1RawStrength = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE] + team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
	double team2RawStrength = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE] + team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
	double total = team1RawStrength + team2RawStrength;
	double team1RelativeStrength = team1RawStrength / total;
	double team2RelativeStrength = team2RawStrength / total;
	double diffPercentage = fabs(team1RelativeStrength - team2RelativeStrength);
	double diff = round(fabs(team1RawStrength - team2RawStrength) / PLAYERRATING_DECIMAL_INCREMENT);
	int iDiff = (int)diff;

	int team1TopTiers = 0, team2TopTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	int topTierImbalance = abs(team1TopTiers - team2TopTiers);

	int team1BottomTiers = 0, team2BottomTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	int team1GarbageTiers = 0, team2GarbageTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	int garbageTierImbalance = abs(team1GarbageTiers - team2GarbageTiers);

	// reward permutations that put people on preferred positions
	int numOnPreferredPos = 0, numOnAvoidedPos = 0;
	if (team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team1base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team2base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;

	int team1OffenseDefenseDiff = 0, team2OffenseDefenseDiff = 0;
	{
		double team1DefenseTotal = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE];
		double team1OffenseTotal = team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
		team1OffenseDefenseDiff = (int)round(fabs(team1DefenseTotal - team1OffenseTotal) * 10000);

		double team2DefenseTotal = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE];
		double team2OffenseTotal = team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
		team2OffenseDefenseDiff = (int)round(fabs(team2DefenseTotal - team2OffenseTotal) * 10000);
	}
	int offenseDefenseDiff = team1OffenseDefenseDiff + team2OffenseDefenseDiff;

	TeamGen_DebugPrintf("Regular:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
		team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team1base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team1base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team1base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team1base->accountName,
		team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team1chase->accountName,
		team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense1->accountName,
		team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense2->accountName,
		team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team2base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team2base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team2base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team2base->accountName,
		team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team2chase->accountName,
		team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense1->accountName,
		team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense2->accountName,
		team1RawStrength,
		team2RawStrength,
		team1RelativeStrength * 100,
		team2RelativeStrength * 100,
		numOnPreferredPos,
		numOnAvoidedPos,
		garbageTierImbalance,
		team1GarbageTiers,
		team2GarbageTiers,
		bottomTierImbalance,
		team1BottomTiers,
		team2BottomTiers,
		topTierImbalance,
		team1TopTiers,
		team2TopTiers,
		total,
		diff,
		offenseDefenseDiff,
		team1OffenseDefenseDiff,
		team2OffenseDefenseDiff);

	if (context->enforceImbalanceCaps) {
		if (!rerollNum && g_vote_teamgen_acdImbalanceCapWithoutReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithoutReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (without reroll).<br/>");
				return;
			}
		}
		else if (rerollNum == 1 && g_vote_teamgen_acdImbalanceCapWithOneReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithOneReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (with one reroll).<br/>");
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		TeamGen_DebugPrintf(" garbage imbalance too great (>= 2).<br/>");
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team1ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam2OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam2OffenseTier - team1ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 1 chase vs. team 2 offense %d - %d == %d<br/>", highestTeam2OffenseTier, team1ChaseTier, highestTeam2OffenseTier - team1ChaseTier);
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team2ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam1OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam1OffenseTier - team2ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense %d - %d == %d<br/>", highestTeam1OffenseTier, team2ChaseTier, highestTeam1OffenseTier - team2ChaseTier);
				return;
			}
		}
	}

	qboolean isBest = qfalse;
	// this permutation will be favored over the previous permutation if:
	// - it is fairer
	// - it is equally fair, but has more people on preferred pos
	// - it is equally fair and has an equal number of people on preferred pos, but has fewer people on avoided pos
	// - it is equally fair and has an equal number of people on preferred and avoided pos, but has better balance of bottom tier players
	// - it is equally fair and has an equal number of people on preferred and avoided pos and equal balance of bottom tier players, but has better balance of top tier players
	if (iDiff < context->best->iDiff ||
		(iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos) ||
		(iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
		(iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
		(iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)) {
			
		if (iDiff < context->best->iDiff)
			TeamGen_DebugPrintf(" <font color=purple>best so far (fairer)</font><br/>");
		else if (iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, but more preferred pos)</font><br/>");
		else if (iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness and preferred pos, but less on avoided pos)</font><br/>");
		else if (iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, preferred pos, and avoided pos, but better bottom tier balance)</font><br/>");
		else if (iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, preferred pos, avoided pos, and bottom tier balance, but better top tier balance)</font><br/>");
		else
			TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

		isBest = qtrue;
	}

	if (isBest) {
		if (context->avoidedHashesList && context->avoidedHashesList->size > 0) {
			permutationOfTeams_t hashMe = { 0 };
			hashMe.valid = qtrue;
			hashMe.iDiff = iDiff;
			hashMe.offenseDefenseDiff = offenseDefenseDiff;
			hashMe.numOnPreferredPos = numOnPreferredPos;
			hashMe.numOnAvoidedPos = numOnAvoidedPos;
			hashMe.topTierImbalance = topTierImbalance;
			hashMe.bottomTierImbalance = bottomTierImbalance;
			//set totalSkill
			hashMe.teams[0].rawStrength = team1RawStrength;
			hashMe.teams[1].rawStrength = team2RawStrength;
			hashMe.teams[0].relativeStrength = team1RelativeStrength;
			hashMe.teams[1].relativeStrength = team2RelativeStrength;
			hashMe.teams[0].baseId = team1base->accountId;
			hashMe.teams[0].chaseId = team1chase->accountId;
			hashMe.teams[0].offenseId1 = team1offense1->accountId;
			hashMe.teams[0].offenseId2 = team1offense2->accountId;
			hashMe.teams[1].baseId = team2base->accountId;
			hashMe.teams[1].chaseId = team2chase->accountId;
			hashMe.teams[1].offenseId1 = team2offense1->accountId;
			hashMe.teams[1].offenseId2 = team2offense2->accountId;
			for (int i = 0; i < 2; i++) {
				memset(hashMe.teams[i].baseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].chaseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense1Name, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense2Name, 0, MAX_NAME_LENGTH);
			}
			Q_strncpyz(hashMe.teams[0].baseName, team1base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].chaseName, team1chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense1Name, team1offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense2Name, team1offense2->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].baseName, team2base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].chaseName, team2chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense1Name, team2offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense2Name, team2offense2->accountName, MAX_NAME_LENGTH);

			NormalizePermutationOfTeams(&hashMe);
			XXH32_hash_t hash = HashPermutationOfTeams(&hashMe);
			avoidedHash_t *avoided = ListFind(context->avoidedHashesList, HashMatchesAvoidedHash, &hash, NULL);
			if (avoided) {
				TeamGen_DebugPrintf("<font color=darkred>***MATCHES AVOIDED HASH; skipping</font><br/>");
				--context->numPermutations;
				return;
			}
		}

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		//set totalSkill
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
		float lowestPlayerRating = 999999;
		if (team1base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team1base->rating[CTFPOSITION_BASE];
		if (team1chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team1chase->rating[CTFPOSITION_CHASE];
		if (team1offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense1->rating[CTFPOSITION_OFFENSE];
		if (team1offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense2->rating[CTFPOSITION_OFFENSE];
		if (team2base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team2base->rating[CTFPOSITION_BASE];
		if (team2chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team2chase->rating[CTFPOSITION_CHASE];
		if (team2offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense1->rating[CTFPOSITION_OFFENSE];
		if (team2offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense2->rating[CTFPOSITION_OFFENSE];
		context->best->lowestPlayerRating = lowestPlayerRating;
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
		TeamGen_DebugPrintf("<br/>");
	}
}

static void TryTeamPermutation_Appeasing(teamGeneratorContext_t *context, const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase,
	const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase,
	const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {

	if (!team1base->rating[CTFPOSITION_BASE] || !team1chase->rating[CTFPOSITION_CHASE] || !team1offense1->rating[CTFPOSITION_OFFENSE] || !team1offense2->rating[CTFPOSITION_OFFENSE] ||
		!team2base->rating[CTFPOSITION_BASE] || !team2chase->rating[CTFPOSITION_CHASE] || !team2offense1->rating[CTFPOSITION_OFFENSE] || !team2offense2->rating[CTFPOSITION_OFFENSE]) {
		return; // at least one player is invalid on their proposed position
	}

	++context->numPermutations;

	if (context->banAvoidedPositions) {
		if ((team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) ||
			(team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) {
			return;
		}
	}

	double team1RawStrength = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE] + team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
	double team2RawStrength = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE] + team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
	double total = team1RawStrength + team2RawStrength;
	double team1RelativeStrength = team1RawStrength / total;
	double team2RelativeStrength = team2RawStrength / total;
	double diff = round(fabs(team1RawStrength - team2RawStrength) / PLAYERRATING_DECIMAL_INCREMENT);
	int iDiff = (int)diff;




	// we only want exactly 50-50 in appeasing, since we can just fall back to semi-appeasing if not anyway
	if (iDiff > 0)
		return;




	int team1TopTiers = 0, team2TopTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	int topTierImbalance = abs(team1TopTiers - team2TopTiers);

	int team1BottomTiers = 0, team2BottomTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	int team1GarbageTiers = 0, team2GarbageTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	int garbageTierImbalance = abs(team1GarbageTiers - team2GarbageTiers);

	// reward permutations that put people on preferred positions
	int numOnPreferredPos = 0, numOnAvoidedPos = 0;
	if (team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team1base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team2base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;

	int team1OffenseDefenseDiff = 0, team2OffenseDefenseDiff = 0;
	{
		double team1DefenseTotal = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE];
		double team1OffenseTotal = team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
		team1OffenseDefenseDiff = (int)round(fabs(team1DefenseTotal - team1OffenseTotal) * 10000);

		double team2DefenseTotal = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE];
		double team2OffenseTotal = team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
		team2OffenseDefenseDiff = (int)round(fabs(team2DefenseTotal - team2OffenseTotal) * 10000);
	}
	int offenseDefenseDiff = team1OffenseDefenseDiff + team2OffenseDefenseDiff;

	TeamGen_DebugPrintf("Regular:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
		team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team1base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team1base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team1base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team1base->accountName,
		team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team1chase->accountName,
		team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense1->accountName,
		team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense2->accountName,
		team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team2base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team2base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team2base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team2base->accountName,
		team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team2chase->accountName,
		team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense1->accountName,
		team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense2->accountName,
		team1RawStrength,
		team2RawStrength,
		team1RelativeStrength * 100,
		team2RelativeStrength * 100,
		numOnPreferredPos,
		numOnAvoidedPos,
		garbageTierImbalance,
		team1GarbageTiers,
		team2GarbageTiers,
		bottomTierImbalance,
		team1BottomTiers,
		team2BottomTiers,
		topTierImbalance,
		team1TopTiers,
		team2TopTiers,
		total,
		diff,
		offenseDefenseDiff,
		team1OffenseDefenseDiff,
		team2OffenseDefenseDiff);

	if (garbageTierImbalance >= 2) {
		TeamGen_DebugPrintf(" garbage imbalance too great (>= 2).<br/>");
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team1ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam2OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam2OffenseTier - team1ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 1 chase vs. team 2 offense %d - %d == %d<br/>", highestTeam2OffenseTier, team1ChaseTier, highestTeam2OffenseTier - team1ChaseTier);
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team2ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam1OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam1OffenseTier - team2ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense %d - %d == %d<br/>", highestTeam1OffenseTier, team2ChaseTier, highestTeam1OffenseTier - team2ChaseTier);
				return;
			}
		}
	}

	qboolean isBest = qfalse;
	// this permutation will be favored over the previous permutation if:
	// - it is fairer
	// - it is equally fair, but has more people on preferred pos
	// - it is equally fair and has an equal number of people on preferred pos, but has fewer people on avoided pos
	// - it is equally fair and has an equal number of people on preferred and avoided pos, but has better balance of bottom tier players
	// - it is equally fair and has an equal number of people on preferred and avoided pos and equal balance of bottom tier players, but has better balance of top tier players
	if (iDiff < context->best->iDiff ||
		(iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos) ||
		(iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
		(iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
		(iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)) {

		if (iDiff < context->best->iDiff)
			TeamGen_DebugPrintf(" <font color=purple>best so far (fairer)</font><br/>");
		else if (iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, but more preferred pos)</font><br/>");
		else if (iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness and preferred pos, but less on avoided pos)</font><br/>");
		else if (iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, preferred pos, and avoided pos, but better bottom tier balance)</font><br/>");
		else if (iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, preferred pos, avoided pos, and bottom tier balance, but better top tier balance)</font><br/>");
		else
			TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

		isBest = qtrue;
	}

	if (isBest) {
		if (context->avoidedHashesList && context->avoidedHashesList->size > 0) {
			permutationOfTeams_t hashMe = { 0 };
			hashMe.valid = qtrue;
			hashMe.iDiff = iDiff;
			hashMe.offenseDefenseDiff = offenseDefenseDiff;
			hashMe.numOnPreferredPos = numOnPreferredPos;
			hashMe.numOnAvoidedPos = numOnAvoidedPos;
			hashMe.topTierImbalance = topTierImbalance;
			hashMe.bottomTierImbalance = bottomTierImbalance;
			//set totalSkill
			hashMe.teams[0].rawStrength = team1RawStrength;
			hashMe.teams[1].rawStrength = team2RawStrength;
			hashMe.teams[0].relativeStrength = team1RelativeStrength;
			hashMe.teams[1].relativeStrength = team2RelativeStrength;
			hashMe.teams[0].baseId = team1base->accountId;
			hashMe.teams[0].chaseId = team1chase->accountId;
			hashMe.teams[0].offenseId1 = team1offense1->accountId;
			hashMe.teams[0].offenseId2 = team1offense2->accountId;
			hashMe.teams[1].baseId = team2base->accountId;
			hashMe.teams[1].chaseId = team2chase->accountId;
			hashMe.teams[1].offenseId1 = team2offense1->accountId;
			hashMe.teams[1].offenseId2 = team2offense2->accountId;
			for (int i = 0; i < 2; i++) {
				memset(hashMe.teams[i].baseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].chaseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense1Name, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense2Name, 0, MAX_NAME_LENGTH);
			}
			Q_strncpyz(hashMe.teams[0].baseName, team1base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].chaseName, team1chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense1Name, team1offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense2Name, team1offense2->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].baseName, team2base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].chaseName, team2chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense1Name, team2offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense2Name, team2offense2->accountName, MAX_NAME_LENGTH);

			NormalizePermutationOfTeams(&hashMe);
			XXH32_hash_t hash = HashPermutationOfTeams(&hashMe);
			avoidedHash_t *avoided = ListFind(context->avoidedHashesList, HashMatchesAvoidedHash, &hash, NULL);
			if (avoided) {
				TeamGen_DebugPrintf("<font color=darkred>***MATCHES AVOIDED HASH; skipping</font><br/>");
				--context->numPermutations;
				return;
			}
		}

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		//set totalSkill
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
		float lowestPlayerRating = 999999;
		if (team1base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team1base->rating[CTFPOSITION_BASE];
		if (team1chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team1chase->rating[CTFPOSITION_CHASE];
		if (team1offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense1->rating[CTFPOSITION_OFFENSE];
		if (team1offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense2->rating[CTFPOSITION_OFFENSE];
		if (team2base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team2base->rating[CTFPOSITION_BASE];
		if (team2chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team2chase->rating[CTFPOSITION_CHASE];
		if (team2offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense1->rating[CTFPOSITION_OFFENSE];
		if (team2offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense2->rating[CTFPOSITION_OFFENSE];
		context->best->lowestPlayerRating = lowestPlayerRating;
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
		TeamGen_DebugPrintf("<br/>");
	}
}

static void TryTeamPermutation_Fairest(teamGeneratorContext_t *context, const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase,
	const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase,
	const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {

	if (!team1base->rating[CTFPOSITION_BASE] || !team1chase->rating[CTFPOSITION_CHASE] || !team1offense1->rating[CTFPOSITION_OFFENSE] || !team1offense2->rating[CTFPOSITION_OFFENSE] ||
		!team2base->rating[CTFPOSITION_BASE] || !team2chase->rating[CTFPOSITION_CHASE] || !team2offense1->rating[CTFPOSITION_OFFENSE] || !team2offense2->rating[CTFPOSITION_OFFENSE]) {
		return; // at least one player is invalid on their proposed position
	}

	++context->numPermutations;

	if (context->banAvoidedPositions) {
		if ((team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) ||
			(team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) {
			return;
		}
	}

	double team1RawStrength = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE] + team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
	double team2RawStrength = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE] + team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
	double total = team1RawStrength + team2RawStrength;
	double team1RelativeStrength = team1RawStrength / total;
	double team2RelativeStrength = team2RawStrength / total;
	double diffPercentage = fabs(team1RelativeStrength - team2RelativeStrength);
	double diff = round(fabs(team1RawStrength - team2RawStrength) / PLAYERRATING_DECIMAL_INCREMENT);
	int iDiff = (int)diff;

	int team1TopTiers = 0, team2TopTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	int topTierImbalance = abs(team1TopTiers - team2TopTiers);

	int team1BottomTiers = 0, team2BottomTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	int team1GarbageTiers = 0, team2GarbageTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	int garbageTierImbalance = abs(team1GarbageTiers - team2GarbageTiers);

	// reward permutations that put people on preferred positions
	int numOnPreferredPos = 0, numOnAvoidedPos = 0;
	if (team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team1base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team2base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;

	int team1OffenseDefenseDiff = 0, team2OffenseDefenseDiff = 0;
	{
		double team1DefenseTotal = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE];
		double team1OffenseTotal = team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
		team1OffenseDefenseDiff = (int)round(fabs(team1DefenseTotal - team1OffenseTotal) * 10000);

		double team2DefenseTotal = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE];
		double team2OffenseTotal = team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
		team2OffenseDefenseDiff = (int)round(fabs(team2DefenseTotal - team2OffenseTotal) * 10000);
	}
	int offenseDefenseDiff = team1OffenseDefenseDiff + team2OffenseDefenseDiff;

	TeamGen_DebugPrintf("Regular:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
		team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team1base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team1base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team1base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team1base->accountName,
		team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team1chase->accountName,
		team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense1->accountName,
		team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense2->accountName,
		team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team2base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team2base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team2base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team2base->accountName,
		team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team2chase->accountName,
		team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense1->accountName,
		team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense2->accountName,
		team1RawStrength,
		team2RawStrength,
		team1RelativeStrength * 100,
		team2RelativeStrength * 100,
		numOnPreferredPos,
		numOnAvoidedPos,
		garbageTierImbalance,
		team1GarbageTiers,
		team2GarbageTiers,
		bottomTierImbalance,
		team1BottomTiers,
		team2BottomTiers,
		topTierImbalance,
		team1TopTiers,
		team2TopTiers,
		total,
		diff,
		offenseDefenseDiff,
		team1OffenseDefenseDiff,
		team2OffenseDefenseDiff);

	if (context->enforceImbalanceCaps) {
		if (!rerollNum && g_vote_teamgen_acdImbalanceCapWithoutReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithoutReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (without reroll).<br/>");
				return;
			}
		}
		else if (rerollNum == 1 && g_vote_teamgen_acdImbalanceCapWithOneReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithOneReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (with one reroll).<br/>");
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		TeamGen_DebugPrintf(" garbage imbalance too great (>= 2).<br/>");
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team1ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam2OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam2OffenseTier - team1ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 1 chase vs. team 2 offense %d - %d == %d<br/>", highestTeam2OffenseTier, team1ChaseTier, highestTeam2OffenseTier - team1ChaseTier);
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team2ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam1OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam1OffenseTier - team2ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense %d - %d == %d<br/>", highestTeam1OffenseTier, team2ChaseTier, highestTeam1OffenseTier - team2ChaseTier);
				return;
			}
		}
	}

	qboolean isBest = qfalse;
	if (context->numEligible == 8 && g_vote_teamgen_new8PlayerAlgo.integer) {
		// This permutation will be favored over the previous permutation if:
		// - It is fairer (lesser iDiff).
		// - It is equally fair (same iDiff), but has a better balance of bottom tier players (lesser bottomTierImbalance).
		// - It is equally fair and has an equal balance of bottom tier players, but has a better balance of top tier players (lesser topTierImbalance).
		// - It is equally fair, with equal bottom and top tier balances, but has a lower offense-defense difference (lesser offenseDefenseDiff).
		// - It is equally fair, with equal tier balances and offense-defense difference, but has more players in preferred positions (greater numOnPreferredPos).
		// - It is equally fair, with equal tier balances, offense-defense difference, and number of players in preferred positions, but has fewer players in avoided positions (lesser numOnAvoidedPos).
		if (iDiff < context->best->iDiff ||
			(iDiff == context->best->iDiff && bottomTierImbalance < context->best->bottomTierImbalance) ||
			(iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance) ||
			(iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff < context->best->offenseDefenseDiff) ||
			(iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff == context->best->offenseDefenseDiff && numOnPreferredPos > context->best->numOnPreferredPos) ||
			(iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff == context->best->offenseDefenseDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)) {

			// debug prints
			if (iDiff < context->best->iDiff)
				TeamGen_DebugPrintf(" <font color=purple>best so far (improved fairness)</font><br/>");
			else if (iDiff == context->best->iDiff && bottomTierImbalance < context->best->bottomTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, better bottom tier balance)</font><br/>");
			else if (iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness and bottom tier balance, improved top tier balance)</font><br/>");
			else if (iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff < context->best->offenseDefenseDiff)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness and tier balances, lower offense-defense difference)</font><br/>");
			else if (iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff == context->best->offenseDefenseDiff && numOnPreferredPos > context->best->numOnPreferredPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, tier balances, and offense-defense diff, but more in preferred positions)</font><br/>");
			else if (iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff == context->best->offenseDefenseDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, tier balances, offense-defense diff, and preferred pos count, but fewer in avoided positions)</font><br/>");
			else
				TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

			isBest = qtrue;
		}
	}
	else {
		// this permutation will be favored over the previous permutation if:
		// - it is fairer
		// - it is equally fair, but has more people on preferred pos
		// - it is equally fair and has an equal number of people on preferred pos, but has fewer people on avoided pos
		// - it is equally fair and has an equal number of people on preferred and avoided pos, but has better balance of bottom tier players
		// - it is equally fair and has an equal number of people on preferred and avoided pos and equal balance of bottom tier players, but has better balance of top tier players
		if (iDiff < context->best->iDiff ||
			(iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos) ||
			(iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
			(iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
			(iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)) {

			if (iDiff < context->best->iDiff)
				TeamGen_DebugPrintf(" <font color=purple>best so far (fairer)</font><br/>");
			else if (iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, but more preferred pos)</font><br/>");
			else if (iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness and preferred pos, but less on avoided pos)</font><br/>");
			else if (iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, preferred pos, and avoided pos, but better bottom tier balance)</font><br/>");
			else if (iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, preferred pos, avoided pos, and bottom tier balance, but better top tier balance)</font><br/>");
			else
				TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

			isBest = qtrue;
		}
	}

	if (isBest) {
		if (context->avoidedHashesList && context->avoidedHashesList->size > 0) {
			permutationOfTeams_t hashMe = { 0 };
			hashMe.valid = qtrue;
			hashMe.iDiff = iDiff;
			hashMe.offenseDefenseDiff = offenseDefenseDiff;
			hashMe.numOnPreferredPos = numOnPreferredPos;
			hashMe.numOnAvoidedPos = numOnAvoidedPos;
			hashMe.topTierImbalance = topTierImbalance;
			hashMe.bottomTierImbalance = bottomTierImbalance;
			//set totalSkill
			hashMe.teams[0].rawStrength = team1RawStrength;
			hashMe.teams[1].rawStrength = team2RawStrength;
			hashMe.teams[0].relativeStrength = team1RelativeStrength;
			hashMe.teams[1].relativeStrength = team2RelativeStrength;
			hashMe.teams[0].baseId = team1base->accountId;
			hashMe.teams[0].chaseId = team1chase->accountId;
			hashMe.teams[0].offenseId1 = team1offense1->accountId;
			hashMe.teams[0].offenseId2 = team1offense2->accountId;
			hashMe.teams[1].baseId = team2base->accountId;
			hashMe.teams[1].chaseId = team2chase->accountId;
			hashMe.teams[1].offenseId1 = team2offense1->accountId;
			hashMe.teams[1].offenseId2 = team2offense2->accountId;
			for (int i = 0; i < 2; i++) {
				memset(hashMe.teams[i].baseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].chaseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense1Name, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense2Name, 0, MAX_NAME_LENGTH);
			}
			Q_strncpyz(hashMe.teams[0].baseName, team1base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].chaseName, team1chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense1Name, team1offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense2Name, team1offense2->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].baseName, team2base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].chaseName, team2chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense1Name, team2offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense2Name, team2offense2->accountName, MAX_NAME_LENGTH);

			NormalizePermutationOfTeams(&hashMe);
			XXH32_hash_t hash = HashPermutationOfTeams(&hashMe);
			avoidedHash_t *avoided = ListFind(context->avoidedHashesList, HashMatchesAvoidedHash, &hash, NULL);
			if (avoided) {
				TeamGen_DebugPrintf("<font color=darkred>***MATCHES AVOIDED HASH; skipping</font><br/>");
				--context->numPermutations;
				return;
			}
		}

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		//set totalSkill
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
		float lowestPlayerRating = 999999;
		if (team1base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team1base->rating[CTFPOSITION_BASE];
		if (team1chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team1chase->rating[CTFPOSITION_CHASE];
		if (team1offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense1->rating[CTFPOSITION_OFFENSE];
		if (team1offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense2->rating[CTFPOSITION_OFFENSE];
		if (team2base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team2base->rating[CTFPOSITION_BASE];
		if (team2chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team2chase->rating[CTFPOSITION_CHASE];
		if (team2offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense1->rating[CTFPOSITION_OFFENSE];
		if (team2offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense2->rating[CTFPOSITION_OFFENSE];
		context->best->lowestPlayerRating = lowestPlayerRating;
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
		TeamGen_DebugPrintf("<br/>");
	}
}

static void TryTeamPermutation_SemiAppeasing(teamGeneratorContext_t *context, const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase,
	const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase,
	const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {

	if (!team1base->rating[CTFPOSITION_BASE] || !team1chase->rating[CTFPOSITION_CHASE] || !team1offense1->rating[CTFPOSITION_OFFENSE] || !team1offense2->rating[CTFPOSITION_OFFENSE] ||
		!team2base->rating[CTFPOSITION_BASE] || !team2chase->rating[CTFPOSITION_CHASE] || !team2offense1->rating[CTFPOSITION_OFFENSE] || !team2offense2->rating[CTFPOSITION_OFFENSE]) {
		return; // at least one player is invalid on their proposed position
	}

	++context->numPermutations;

	if (context->banAvoidedPositions) {
		if ((team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) ||
			(team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) {
			return;
		}
	}

	double team1RawStrength = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE] + team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
	double team2RawStrength = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE] + team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
	double total = team1RawStrength + team2RawStrength;
	double team1RelativeStrength = team1RawStrength / total;
	double team2RelativeStrength = team2RawStrength / total;
	double diffPercentage = fabs(team1RelativeStrength - team2RelativeStrength);
	double diff = round(fabs(team1RawStrength - team2RawStrength) / PLAYERRATING_DECIMAL_INCREMENT);
	int iDiff = (int)diff;

	int team1TopTiers = 0, team2TopTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	int topTierImbalance = abs(team1TopTiers - team2TopTiers);

	int team1BottomTiers = 0, team2BottomTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	int team1GarbageTiers = 0, team2GarbageTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	int garbageTierImbalance = abs(team1GarbageTiers - team2GarbageTiers);

	// reward permutations that put people on preferred positions
	int numOnPreferredPos = 0, numOnAvoidedPos = 0;
	if (team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team1base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team2base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;

	int team1OffenseDefenseDiff = 0, team2OffenseDefenseDiff = 0;
	{
		double team1DefenseTotal = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE];
		double team1OffenseTotal = team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
		team1OffenseDefenseDiff = (int)round(fabs(team1DefenseTotal - team1OffenseTotal) * 10000);

		double team2DefenseTotal = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE];
		double team2OffenseTotal = team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
		team2OffenseDefenseDiff = (int)round(fabs(team2DefenseTotal - team2OffenseTotal) * 10000);
	}
	int offenseDefenseDiff = team1OffenseDefenseDiff + team2OffenseDefenseDiff;

	TeamGen_DebugPrintf("SemiAppeasing:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
		team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team1base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team1base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team1base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team1base->accountName,
		team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team1chase->accountName,
		team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense1->accountName,
		team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense2->accountName,
		team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team2base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team2base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team2base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team2base->accountName,
		team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team2chase->accountName,
		team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense1->accountName,
		team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense2->accountName,
		team1RawStrength,
		team2RawStrength,
		team1RelativeStrength * 100,
		team2RelativeStrength * 100,
		numOnPreferredPos,
		numOnAvoidedPos,
		garbageTierImbalance,
		team1GarbageTiers,
		team2GarbageTiers,
		bottomTierImbalance,
		team1BottomTiers,
		team2BottomTiers,
		topTierImbalance,
		team1TopTiers,
		team2TopTiers,
		total,
		diff,
		offenseDefenseDiff,
		team1OffenseDefenseDiff,
		team2OffenseDefenseDiff);

	if (context->enforceImbalanceCaps) {
		if (!rerollNum && g_vote_teamgen_acdImbalanceCapWithoutReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithoutReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (without reroll).<br/>");
				return;
			}
		}
		else if (rerollNum == 1 && g_vote_teamgen_acdImbalanceCapWithOneReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithOneReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (with one reroll).<br/>");
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		TeamGen_DebugPrintf(" garbage imbalance too great (>= 2).<br/>");
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team1ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam2OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam2OffenseTier - team1ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 1 chase vs. team 2 offense %d - %d == %d<br/>", highestTeam2OffenseTier, team1ChaseTier, highestTeam2OffenseTier - team1ChaseTier);
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team2ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam1OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam1OffenseTier - team2ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense %d - %d == %d<br/>", highestTeam1OffenseTier, team2ChaseTier, highestTeam1OffenseTier - team2ChaseTier);
				return;
			}
		}
	}

	int numSatisfiedCyds = 0;
	if (team1base->numPermutationsIn > 0 && !team1base->numPermutationsInAndNonAvoidedPos && !(team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE))) ++numSatisfiedCyds;
	if (team1chase->numPermutationsIn > 0 && !team1chase->numPermutationsInAndNonAvoidedPos && !(team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE))) ++numSatisfiedCyds;
	if (team1offense1->numPermutationsIn > 0 && !team1offense1->numPermutationsInAndNonAvoidedPos && !(team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (team1offense2->numPermutationsIn > 0 && !team1offense2->numPermutationsInAndNonAvoidedPos && !(team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (team2base->numPermutationsIn > 0 && !team2base->numPermutationsInAndNonAvoidedPos && !(team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE))) ++numSatisfiedCyds;
	if (team2chase->numPermutationsIn > 0 && !team2chase->numPermutationsInAndNonAvoidedPos && !(team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE))) ++numSatisfiedCyds;
	if (team2offense1->numPermutationsIn > 0 && !team2offense1->numPermutationsInAndNonAvoidedPos && !(team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (team2offense2->numPermutationsIn > 0 && !team2offense2->numPermutationsInAndNonAvoidedPos && !(team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (!numSatisfiedCyds) {
		TeamGen_DebugPrintf("No satisfied cyds!\n");
		return;
	}

	qboolean isBest = qfalse;

	// this permutation will be favored over the previous permutation if:
	// - the number of cyds (people in a/b/c, on avoided pos in a/b/c only, and not on avoided pos here) is higher
	// - equal cyds, but fairer
	// - equal cyds and fairness, but has more people on preferred pos
	// - equal cyds and fairness and number of people on preferred pos, but has fewer people on avoided pos
	// - equal cyds and fairness and number of people on preferred and avoided pos, but has better balance of bottom tier players
	// - equal cyds and fairness and number of people on preferred and avoided pos and equal balance of bottom tier players, but has better balance of top tier players
	// - (8 players only) equal cyds and fairness and number of people on preferred and avoided pos and equal balance of bottom and top tier players, but better offense-defense diff
	if (numSatisfiedCyds > context->best->numSatisfiedCyds ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff < context->best->iDiff) ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos) ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance) ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && (context->numEligible == 8 && offenseDefenseDiff < context->best->offenseDefenseDiff))) {

		if (numSatisfiedCyds > context->best->numSatisfiedCyds)
			TeamGen_DebugPrintf(" <font color=purple>best so far (more satisfied cyds)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff < context->best->iDiff)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds, but fairer)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds and fairness, but more preferred pos)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds, fairness and preferred pos, but less on avoided pos)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds, fairness, preferred pos, and avoided pos, but better bottom tier balance)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds, fairness, preferred pos, avoided pos, and bottom tier balance, but better top tier balance)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && (context->numEligible == 8 && offenseDefenseDiff < context->best->offenseDefenseDiff))
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds, fairness, preferred pos, avoided pos, bottom and top tier balance, but 8 players and lower offense-defense diff)</font><br/>");
		else
			TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

		isBest = qtrue;
	}
	
	if (isBest) {
		if (context->avoidedHashesList && context->avoidedHashesList->size > 0) {
			permutationOfTeams_t hashMe = { 0 };
			hashMe.valid = qtrue;
			hashMe.iDiff = iDiff;
			hashMe.offenseDefenseDiff = offenseDefenseDiff;
			hashMe.numOnPreferredPos = numOnPreferredPos;
			hashMe.numOnAvoidedPos = numOnAvoidedPos;
			hashMe.topTierImbalance = topTierImbalance;
			hashMe.bottomTierImbalance = bottomTierImbalance;
			//set totalSkill
			hashMe.teams[0].rawStrength = team1RawStrength;
			hashMe.teams[1].rawStrength = team2RawStrength;
			hashMe.teams[0].relativeStrength = team1RelativeStrength;
			hashMe.teams[1].relativeStrength = team2RelativeStrength;
			hashMe.teams[0].baseId = team1base->accountId;
			hashMe.teams[0].chaseId = team1chase->accountId;
			hashMe.teams[0].offenseId1 = team1offense1->accountId;
			hashMe.teams[0].offenseId2 = team1offense2->accountId;
			hashMe.teams[1].baseId = team2base->accountId;
			hashMe.teams[1].chaseId = team2chase->accountId;
			hashMe.teams[1].offenseId1 = team2offense1->accountId;
			hashMe.teams[1].offenseId2 = team2offense2->accountId;
			for (int i = 0; i < 2; i++) {
				memset(hashMe.teams[i].baseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].chaseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense1Name, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense2Name, 0, MAX_NAME_LENGTH);
			}
			Q_strncpyz(hashMe.teams[0].baseName, team1base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].chaseName, team1chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense1Name, team1offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense2Name, team1offense2->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].baseName, team2base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].chaseName, team2chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense1Name, team2offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense2Name, team2offense2->accountName, MAX_NAME_LENGTH);
			//numSatisfiedCyds

			NormalizePermutationOfTeams(&hashMe);
			XXH32_hash_t hash = HashPermutationOfTeams(&hashMe);
			avoidedHash_t *avoided = ListFind(context->avoidedHashesList, HashMatchesAvoidedHash, &hash, NULL);
			if (avoided) {
				TeamGen_DebugPrintf("<font color=darkred>***MATCHES AVOIDED HASH; skipping</font><br/>");
				--context->numPermutations;
				return;
			}
		}

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		//set totalSkill
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
		float lowestPlayerRating = 999999;
		if (team1base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team1base->rating[CTFPOSITION_BASE];
		if (team1chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team1chase->rating[CTFPOSITION_CHASE];
		if (team1offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense1->rating[CTFPOSITION_OFFENSE];
		if (team1offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense2->rating[CTFPOSITION_OFFENSE];
		if (team2base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team2base->rating[CTFPOSITION_BASE];
		if (team2chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team2chase->rating[CTFPOSITION_CHASE];
		if (team2offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense1->rating[CTFPOSITION_OFFENSE];
		if (team2offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense2->rating[CTFPOSITION_OFFENSE];
		context->best->lowestPlayerRating = lowestPlayerRating;
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
		context->best->numSatisfiedCyds = numSatisfiedCyds;
	}
	else {
		TeamGen_DebugPrintf("<br/>");
	}
}

static void TryTeamPermutation_FirstChoice(teamGeneratorContext_t *context, const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase,
	const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase,
	const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {

	if (!team1base->rating[CTFPOSITION_BASE] || !team1chase->rating[CTFPOSITION_CHASE] || !team1offense1->rating[CTFPOSITION_OFFENSE] || !team1offense2->rating[CTFPOSITION_OFFENSE] ||
		!team2base->rating[CTFPOSITION_BASE] || !team2chase->rating[CTFPOSITION_CHASE] || !team2offense1->rating[CTFPOSITION_OFFENSE] || !team2offense2->rating[CTFPOSITION_OFFENSE]) {
		return; // at least one player is invalid on their proposed position
	}

	++context->numPermutations;

	if (context->banAvoidedPositions) {
		if ((team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) ||
			(team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) {
			return;
		}
	}

	double team1RawStrength = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE] + team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
	double team2RawStrength = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE] + team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
	double total = team1RawStrength + team2RawStrength;
	double team1RelativeStrength = team1RawStrength / total;
	double team2RelativeStrength = team2RawStrength / total;
	double diffPercentage = fabs(team1RelativeStrength - team2RelativeStrength);
	double diff = round(fabs(team1RawStrength - team2RawStrength) / PLAYERRATING_DECIMAL_INCREMENT);
	int iDiff = (int)diff;

	int team1TopTiers = 0, team2TopTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	int topTierImbalance = abs(team1TopTiers - team2TopTiers);

	int team1BottomTiers = 0, team2BottomTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	int team1GarbageTiers = 0, team2GarbageTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	int garbageTierImbalance = abs(team1GarbageTiers - team2GarbageTiers);

	// reward permutations that put people on preferred positions
	int numOnPreferredPos = 0, numOnAvoidedPos = 0;
	if (team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team1base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team2base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;

	int team1OffenseDefenseDiff = 0, team2OffenseDefenseDiff = 0;
	{
		double team1DefenseTotal = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE];
		double team1OffenseTotal = team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
		team1OffenseDefenseDiff = (int)round(fabs(team1DefenseTotal - team1OffenseTotal) * 10000);

		double team2DefenseTotal = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE];
		double team2OffenseTotal = team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
		team2OffenseDefenseDiff = (int)round(fabs(team2DefenseTotal - team2OffenseTotal) * 10000);
	}
	int offenseDefenseDiff = team1OffenseDefenseDiff + team2OffenseDefenseDiff;

	TeamGen_DebugPrintf("SemiAppeasing:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
		team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team1base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team1base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team1base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team1base->accountName,
		team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team1chase->accountName,
		team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense1->accountName,
		team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense2->accountName,
		team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team2base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team2base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team2base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team2base->accountName,
		team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team2chase->accountName,
		team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense1->accountName,
		team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense2->accountName,
		team1RawStrength,
		team2RawStrength,
		team1RelativeStrength * 100,
		team2RelativeStrength * 100,
		numOnPreferredPos,
		numOnAvoidedPos,
		garbageTierImbalance,
		team1GarbageTiers,
		team2GarbageTiers,
		bottomTierImbalance,
		team1BottomTiers,
		team2BottomTiers,
		topTierImbalance,
		team1TopTiers,
		team2TopTiers,
		total,
		diff,
		offenseDefenseDiff,
		team1OffenseDefenseDiff,
		team2OffenseDefenseDiff);

	if (context->enforceImbalanceCaps) {
		if (!rerollNum && g_vote_teamgen_acdImbalanceCapWithoutReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithoutReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (without reroll).<br/>");
				return;
			}
		}
		else if (rerollNum == 1 && g_vote_teamgen_acdImbalanceCapWithOneReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithOneReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (with one reroll).<br/>");
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		TeamGen_DebugPrintf(" garbage imbalance too great (>= 2).<br/>");
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team1ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam2OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam2OffenseTier - team1ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 1 chase vs. team 2 offense %d - %d == %d<br/>", highestTeam2OffenseTier, team1ChaseTier, highestTeam2OffenseTier - team1ChaseTier);
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team2ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam1OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam1OffenseTier - team2ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense %d - %d == %d<br/>", highestTeam1OffenseTier, team2ChaseTier, highestTeam1OffenseTier - team2ChaseTier);
				return;
			}
		}
	}

	int numSatisfiedCyds = 0;
	if (team1base->notOnFirstChoiceInAbc && (team1base->posPrefs.first & (1 << CTFPOSITION_BASE))) ++numSatisfiedCyds;
	if (team1chase->notOnFirstChoiceInAbc && (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE))) ++numSatisfiedCyds;
	if (team1offense1->notOnFirstChoiceInAbc && (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (team1offense2->notOnFirstChoiceInAbc && (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (team2base->notOnFirstChoiceInAbc && (team2base->posPrefs.first & (1 << CTFPOSITION_BASE))) ++numSatisfiedCyds;
	if (team2chase->notOnFirstChoiceInAbc && (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE))) ++numSatisfiedCyds;
	if (team2offense1->notOnFirstChoiceInAbc && (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (team2offense2->notOnFirstChoiceInAbc && (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (!numSatisfiedCyds) {
		TeamGen_DebugPrintf("No satisfied cyds!\n");
		return;
	}

	qboolean isBest = qfalse;

	// this permutation will be favored over the previous permutation if:
	// - the number of cyds (people in a/b/c, on avoided pos in a/b/c only, and not on avoided pos here) is higher
	// - equal cyds, but fairer
	// - equal cyds and fairness, but has more people on preferred pos
	// - equal cyds and fairness and number of people on preferred pos, but has fewer people on avoided pos
	// - equal cyds and fairness and number of people on preferred and avoided pos, but has better balance of bottom tier players
	// - equal cyds and fairness and number of people on preferred and avoided pos and equal balance of bottom tier players, but has better balance of top tier players
	// - (8 players only) equal cyds and fairness and number of people on preferred and avoided pos and equal balance of bottom and top tier players, but better offense-defense diff
	if (numSatisfiedCyds > context->best->numSatisfiedCyds ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff < context->best->iDiff) ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos) ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance) ||
		(numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && (context->numEligible == 8 && offenseDefenseDiff < context->best->offenseDefenseDiff))) {

		if (numSatisfiedCyds > context->best->numSatisfiedCyds)
			TeamGen_DebugPrintf(" <font color=purple>best so far (more satisfied cyds)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff < context->best->iDiff)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds, but fairer)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds and fairness, but more preferred pos)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds, fairness and preferred pos, but less on avoided pos)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds, fairness, preferred pos, and avoided pos, but better bottom tier balance)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds, fairness, preferred pos, avoided pos, and bottom tier balance, but better top tier balance)</font><br/>");
		else if (numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && (context->numEligible == 8 && offenseDefenseDiff < context->best->offenseDefenseDiff))
			TeamGen_DebugPrintf(" <font color=purple>best so far (same satisfied cyds, fairness, preferred pos, avoided pos, bottom and top tier balance, but 8 players and lower offense-defense diff)</font><br/>");
		else
			TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

		isBest = qtrue;
	}

	if (isBest) {
		if (context->avoidedHashesList && context->avoidedHashesList->size > 0) {
			permutationOfTeams_t hashMe = { 0 };
			hashMe.valid = qtrue;
			hashMe.iDiff = iDiff;
			hashMe.offenseDefenseDiff = offenseDefenseDiff;
			hashMe.numOnPreferredPos = numOnPreferredPos;
			hashMe.numOnAvoidedPos = numOnAvoidedPos;
			hashMe.topTierImbalance = topTierImbalance;
			hashMe.bottomTierImbalance = bottomTierImbalance;
			//set totalSkill
			hashMe.teams[0].rawStrength = team1RawStrength;
			hashMe.teams[1].rawStrength = team2RawStrength;
			hashMe.teams[0].relativeStrength = team1RelativeStrength;
			hashMe.teams[1].relativeStrength = team2RelativeStrength;
			hashMe.teams[0].baseId = team1base->accountId;
			hashMe.teams[0].chaseId = team1chase->accountId;
			hashMe.teams[0].offenseId1 = team1offense1->accountId;
			hashMe.teams[0].offenseId2 = team1offense2->accountId;
			hashMe.teams[1].baseId = team2base->accountId;
			hashMe.teams[1].chaseId = team2chase->accountId;
			hashMe.teams[1].offenseId1 = team2offense1->accountId;
			hashMe.teams[1].offenseId2 = team2offense2->accountId;
			for (int i = 0; i < 2; i++) {
				memset(hashMe.teams[i].baseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].chaseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense1Name, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense2Name, 0, MAX_NAME_LENGTH);
			}
			Q_strncpyz(hashMe.teams[0].baseName, team1base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].chaseName, team1chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense1Name, team1offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense2Name, team1offense2->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].baseName, team2base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].chaseName, team2chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense1Name, team2offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense2Name, team2offense2->accountName, MAX_NAME_LENGTH);
			//numSatisfiedCyds

			NormalizePermutationOfTeams(&hashMe);
			XXH32_hash_t hash = HashPermutationOfTeams(&hashMe);
			avoidedHash_t *avoided = ListFind(context->avoidedHashesList, HashMatchesAvoidedHash, &hash, NULL);
			if (avoided) {
				TeamGen_DebugPrintf("<font color=darkred>***MATCHES AVOIDED HASH; skipping</font><br/>");
				--context->numPermutations;
				return;
			}
		}

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		//set totalSkill
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
		float lowestPlayerRating = 999999;
		if (team1base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team1base->rating[CTFPOSITION_BASE];
		if (team1chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team1chase->rating[CTFPOSITION_CHASE];
		if (team1offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense1->rating[CTFPOSITION_OFFENSE];
		if (team1offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense2->rating[CTFPOSITION_OFFENSE];
		if (team2base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team2base->rating[CTFPOSITION_BASE];
		if (team2chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team2chase->rating[CTFPOSITION_CHASE];
		if (team2offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense1->rating[CTFPOSITION_OFFENSE];
		if (team2offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense2->rating[CTFPOSITION_OFFENSE];
		context->best->lowestPlayerRating = lowestPlayerRating;
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
		context->best->numSatisfiedCyds = numSatisfiedCyds;
	}
	else {
		TeamGen_DebugPrintf("<br/>");
	}
}

static void TryTeamPermutation_Inclusive(teamGeneratorContext_t *context, const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase,
	const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase,
	const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {

	if (!team1base->rating[CTFPOSITION_BASE] || !team1chase->rating[CTFPOSITION_CHASE] || !team1offense1->rating[CTFPOSITION_OFFENSE] || !team1offense2->rating[CTFPOSITION_OFFENSE] ||
		!team2base->rating[CTFPOSITION_BASE] || !team2chase->rating[CTFPOSITION_CHASE] || !team2offense1->rating[CTFPOSITION_OFFENSE] || !team2offense2->rating[CTFPOSITION_OFFENSE]) {
		return; // at least one player is invalid on their proposed position
	}

	++context->numPermutations;

	if (context->banAvoidedPositions) {
		if ((team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) ||
			(team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) {
			return;
		}
	}

	double team1RawStrength = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE] + team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
	double team2RawStrength = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE] + team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
	double total = team1RawStrength + team2RawStrength;
	double team1RelativeStrength = team1RawStrength / total;
	double team2RelativeStrength = team2RawStrength / total;
	double diffPercentage = fabs(team1RelativeStrength - team2RelativeStrength);
	double diff = round(fabs(team1RawStrength - team2RawStrength) / PLAYERRATING_DECIMAL_INCREMENT);
	int iDiff = (int)diff;

	int team1TopTiers = 0, team2TopTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	int topTierImbalance = abs(team1TopTiers - team2TopTiers);

	int team1BottomTiers = 0, team2BottomTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	int team1GarbageTiers = 0, team2GarbageTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	int garbageTierImbalance = abs(team1GarbageTiers - team2GarbageTiers);

	// reward permutations that put people on preferred positions
	int numOnPreferredPos = 0, numOnAvoidedPos = 0;
	if (team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team1base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team2base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;

	int team1OffenseDefenseDiff = 0, team2OffenseDefenseDiff = 0;
	{
		double team1DefenseTotal = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE];
		double team1OffenseTotal = team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
		team1OffenseDefenseDiff = (int)round(fabs(team1DefenseTotal - team1OffenseTotal) * 10000);

		double team2DefenseTotal = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE];
		double team2OffenseTotal = team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
		team2OffenseDefenseDiff = (int)round(fabs(team2DefenseTotal - team2OffenseTotal) * 10000);
	}
	int offenseDefenseDiff = team1OffenseDefenseDiff + team2OffenseDefenseDiff;

	TeamGen_DebugPrintf("Inclusive:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
		team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team1base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team1base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team1base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team1base->accountName,
		team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team1chase->accountName,
		team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense1->accountName,
		team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense2->accountName,
		team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team2base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team2base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team2base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team2base->accountName,
		team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team2chase->accountName,
		team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense1->accountName,
		team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense2->accountName,
		team1RawStrength,
		team2RawStrength,
		team1RelativeStrength * 100,
		team2RelativeStrength * 100,
		numOnPreferredPos,
		numOnAvoidedPos,
		garbageTierImbalance,
		team1GarbageTiers,
		team2GarbageTiers,
		bottomTierImbalance,
		team1BottomTiers,
		team2BottomTiers,
		topTierImbalance,
		team1TopTiers,
		team2TopTiers,
		total,
		diff,
		offenseDefenseDiff,
		team1OffenseDefenseDiff,
		team2OffenseDefenseDiff);

	if (context->enforceImbalanceCaps) {
		if (!rerollNum && g_vote_teamgen_acdImbalanceCapWithoutReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithoutReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (without reroll).<br/>");
				return;
			}
		}
		else if (rerollNum == 1 && g_vote_teamgen_acdImbalanceCapWithOneReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithOneReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (with one reroll).<br/>");
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		TeamGen_DebugPrintf(" garbage imbalance too great (>= 2).<br/>");
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team1ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam2OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam2OffenseTier - team1ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 1 chase vs. team 2 offense %d - %d == %d<br/>", highestTeam2OffenseTier, team1ChaseTier, highestTeam2OffenseTier - team1ChaseTier);
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team2ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam1OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam1OffenseTier - team2ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense %d - %d == %d<br/>", highestTeam1OffenseTier, team2ChaseTier, highestTeam1OffenseTier - team2ChaseTier);
				return;
			}
		}
	}

	int totalNumPermutations = team1base->numPermutationsIn + team1chase->numPermutationsIn + team1offense1->numPermutationsIn + team1offense2->numPermutationsIn +
		team2base->numPermutationsIn + team2chase->numPermutationsIn + team2offense1->numPermutationsIn + team2offense2->numPermutationsIn;

	// make sure we get in people who are in 0 permutations
	if (!team1base->numPermutationsIn)
		totalNumPermutations -= 1000;
	if (!team1chase->numPermutationsIn)
		totalNumPermutations -= 1000;
	if (!team1offense1->numPermutationsIn)
		totalNumPermutations -= 1000;
	if (!team1offense2->numPermutationsIn)
		totalNumPermutations -= 1000;
	if (!team2base->numPermutationsIn)
		totalNumPermutations -= 1000;
	if (!team2chase->numPermutationsIn)
		totalNumPermutations -= 1000;
	if (!team2offense1->numPermutationsIn)
		totalNumPermutations -= 1000;
	if (!team2offense2->numPermutationsIn)
		totalNumPermutations -= 1000;

	int numSatisfiedCyds = 0;
	if (team1base->notOnFirstChoiceInAbc && (team1base->posPrefs.first & (1 << CTFPOSITION_BASE))) ++numSatisfiedCyds;
	if (team1chase->notOnFirstChoiceInAbc && (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE))) ++numSatisfiedCyds;
	if (team1offense1->notOnFirstChoiceInAbc && (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (team1offense2->notOnFirstChoiceInAbc && (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (team2base->notOnFirstChoiceInAbc && (team2base->posPrefs.first & (1 << CTFPOSITION_BASE))) ++numSatisfiedCyds;
	if (team2chase->notOnFirstChoiceInAbc && (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE))) ++numSatisfiedCyds;
	if (team2offense1->notOnFirstChoiceInAbc && (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;
	if (team2offense2->notOnFirstChoiceInAbc && (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedCyds;

	// this permutation will be favored over the previous permutation if:
	// - it has a lower number of permutations
	// - it has equally low permutations, but has more satisfied cyds
	// - it has equally low permutations and equal cyds, but is fairer
	// - it has equally low permutations, equal cyds and fairness, but has more people on preferred pos
	// - it has equally low permutations, equal cyds and fairness and has an equal number of people on preferred pos, but has fewer people on avoided pos
	// - it has equally low permutations, equal cyds and fairness and has an equal number of people on preferred and avoided pos, but has better balance of bottom tier players
	// - it has equally low permutations, equal cyds and fairness and has an equal number of people on preferred and avoided pos and equal balance of bottom tier players, but has better balance of top tier players
	if (totalNumPermutations < context->best->totalNumPermutations ||
		(totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds > context->best->numSatisfiedCyds) ||
		(totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff < context->best->iDiff) ||
		(totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos) ||
		(totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
		(totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
		(totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)) {
		if (totalNumPermutations < context->best->totalNumPermutations)
			TeamGen_DebugPrintf(" <font color=purple>best so far (fewer permutations)</font><br/>");
		else if (totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds > context->best->numSatisfiedCyds)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same permutations, but more cyds)</font><br/>");
		else if (totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff < context->best->iDiff)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same permutations and cyds, but fairer)</font><br/>");
		else if (totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same permutations, cyds, and fairness, but more preferred pos)</font><br/>");
		else if (totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same permutations, cyds, fairness and preferred pos, but less on avoided pos)</font><br/>");
		else if (totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same permutations, cyds, fairness, preferred pos, and avoided pos, but better bottom tier balance)</font><br/>");
		else if (totalNumPermutations == context->best->totalNumPermutations && numSatisfiedCyds == context->best->numSatisfiedCyds && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same permutations, cyds, fairness, preferred pos, avoided pos, and bottom tier balance, but better top tier balance)</font><br/>");
		else
			TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

		if (context->avoidedHashesList && context->avoidedHashesList->size > 0) {
			permutationOfTeams_t hashMe = { 0 };
			hashMe.valid = qtrue;
			hashMe.iDiff = iDiff;
			hashMe.offenseDefenseDiff = offenseDefenseDiff;
			hashMe.numOnPreferredPos = numOnPreferredPos;
			hashMe.totalNumPermutations = totalNumPermutations;
			hashMe.numOnAvoidedPos = numOnAvoidedPos;
			hashMe.topTierImbalance = topTierImbalance;
			hashMe.bottomTierImbalance = bottomTierImbalance;
			//set totalSkill
			hashMe.teams[0].rawStrength = team1RawStrength;
			hashMe.teams[1].rawStrength = team2RawStrength;
			hashMe.teams[0].relativeStrength = team1RelativeStrength;
			hashMe.teams[1].relativeStrength = team2RelativeStrength;
			hashMe.teams[0].baseId = team1base->accountId;
			hashMe.teams[0].chaseId = team1chase->accountId;
			hashMe.teams[0].offenseId1 = team1offense1->accountId;
			hashMe.teams[0].offenseId2 = team1offense2->accountId;
			hashMe.teams[1].baseId = team2base->accountId;
			hashMe.teams[1].chaseId = team2chase->accountId;
			hashMe.teams[1].offenseId1 = team2offense1->accountId;
			hashMe.teams[1].offenseId2 = team2offense2->accountId;
			for (int i = 0; i < 2; i++) {
				memset(hashMe.teams[i].baseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].chaseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense1Name, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense2Name, 0, MAX_NAME_LENGTH);
			}
			Q_strncpyz(hashMe.teams[0].baseName, team1base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].chaseName, team1chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense1Name, team1offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense2Name, team1offense2->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].baseName, team2base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].chaseName, team2chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense1Name, team2offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense2Name, team2offense2->accountName, MAX_NAME_LENGTH);

			NormalizePermutationOfTeams(&hashMe);
			XXH32_hash_t hash = HashPermutationOfTeams(&hashMe);
			avoidedHash_t *avoided = ListFind(context->avoidedHashesList, HashMatchesAvoidedHash, &hash, NULL);
			if (avoided) {
				TeamGen_DebugPrintf("<font color=darkred>***MATCHES AVOIDED HASH; skipping</font><br/>");
				--context->numPermutations;
				return;
			}
		}

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->totalNumPermutations = totalNumPermutations;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		//set totalSkill
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
		float lowestPlayerRating = 999999;
		if (team1base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team1base->rating[CTFPOSITION_BASE];
		if (team1chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team1chase->rating[CTFPOSITION_CHASE];
		if (team1offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense1->rating[CTFPOSITION_OFFENSE];
		if (team1offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense2->rating[CTFPOSITION_OFFENSE];
		if (team2base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team2base->rating[CTFPOSITION_BASE];
		if (team2chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team2chase->rating[CTFPOSITION_CHASE];
		if (team2offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense1->rating[CTFPOSITION_OFFENSE];
		if (team2offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense2->rating[CTFPOSITION_OFFENSE];
		context->best->lowestPlayerRating = lowestPlayerRating;
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
		context->best->numSatisfiedCyds = numSatisfiedCyds;
	}
	else {
		TeamGen_DebugPrintf("<br/>");
	}
}

static void TryTeamPermutation_SemiTryhard(teamGeneratorContext_t *context, const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase,
	const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase,
	const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {

	if (!team1base->rating[CTFPOSITION_BASE] || !team1chase->rating[CTFPOSITION_CHASE] || !team1offense1->rating[CTFPOSITION_OFFENSE] || !team1offense2->rating[CTFPOSITION_OFFENSE] ||
		!team2base->rating[CTFPOSITION_BASE] || !team2chase->rating[CTFPOSITION_CHASE] || !team2offense1->rating[CTFPOSITION_OFFENSE] || !team2offense2->rating[CTFPOSITION_OFFENSE]) {
		return; // at least one player is invalid on their proposed position
	}

	++context->numPermutations;

	if (context->banAvoidedPositions) {
		if ((team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) ||
			(team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) {
			return;
		}
	}

	double team1RawStrength = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE] + team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
	double team2RawStrength = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE] + team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
	double total = team1RawStrength + team2RawStrength;
	int iTotal = (int)round(total * 10000);
	double team1RelativeStrength = team1RawStrength / total;
	double team2RelativeStrength = team2RawStrength / total;
	double diffPercentage = fabs(team1RelativeStrength - team2RelativeStrength);
	double diff = round(fabs(team1RawStrength - team2RawStrength) / PLAYERRATING_DECIMAL_INCREMENT);
	int iDiff = (int)diff;

	int team1TopTiers = 0, team2TopTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	int topTierImbalance = abs(team1TopTiers - team2TopTiers);

	int team1BottomTiers = 0, team2BottomTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	int team1GarbageTiers = 0, team2GarbageTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	int garbageTierImbalance = abs(team1GarbageTiers - team2GarbageTiers);

	// reward permutations that put people on preferred positions
	int numOnPreferredPos = 0, numOnAvoidedPos = 0;
	if (team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team1base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team2base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;

	int team1OffenseDefenseDiff = 0, team2OffenseDefenseDiff = 0;
	{
		double team1DefenseTotal = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE];
		double team1OffenseTotal = team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
		team1OffenseDefenseDiff = (int)round(fabs(team1DefenseTotal - team1OffenseTotal) * 10000);

		double team2DefenseTotal = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE];
		double team2OffenseTotal = team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
		team2OffenseDefenseDiff = (int)round(fabs(team2DefenseTotal - team2OffenseTotal) * 10000);
	}
	int offenseDefenseDiff = team1OffenseDefenseDiff + team2OffenseDefenseDiff;

	TeamGen_DebugPrintf("SemiTryhard:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
		team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team1base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team1base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team1base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team1base->accountName,
		team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team1chase->accountName,
		team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense1->accountName,
		team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense2->accountName,
		team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team2base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team2base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team2base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team2base->accountName,
		team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team2chase->accountName,
		team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense1->accountName,
		team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense2->accountName,
		team1RawStrength,
		team2RawStrength,
		team1RelativeStrength * 100,
		team2RelativeStrength * 100,
		numOnPreferredPos,
		numOnAvoidedPos,
		garbageTierImbalance,
		team1GarbageTiers,
		team2GarbageTiers,
		bottomTierImbalance,
		team1BottomTiers,
		team2BottomTiers,
		topTierImbalance,
		team1TopTiers,
		team2TopTiers,
		total,
		diff,
		offenseDefenseDiff,
		team1OffenseDefenseDiff,
		team2OffenseDefenseDiff);

	if (context->enforceImbalanceCaps) {
		if (!rerollNum && g_vote_teamgen_acdImbalanceCapWithoutReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithoutReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (without reroll).<br/>");
				return;
			}
		}
		else if (rerollNum == 1 && g_vote_teamgen_acdImbalanceCapWithOneReroll.value > 0) {
			double cap = g_vote_teamgen_acdImbalanceCapWithOneReroll.value;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				TeamGen_DebugPrintf(" difference too great (with one reroll).<br/>");
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		TeamGen_DebugPrintf(" garbage imbalance too great (>= 2).<br/>");
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team1ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam2OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam2OffenseTier - team1ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 1 chase vs. team 2 offense %d - %d == %d<br/>", highestTeam2OffenseTier, team1ChaseTier, highestTeam2OffenseTier - team1ChaseTier);
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team2ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam1OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam1OffenseTier - team2ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense %d - %d == %d<br/>", highestTeam1OffenseTier, team2ChaseTier, highestTeam1OffenseTier - team2ChaseTier);
				return;
			}
		}
	}

	double currentBestCombinedStrength = context->best->teams[0].rawStrength + context->best->teams[1].rawStrength;
	int iCurrentBestCombinedStrength = (int)round(currentBestCombinedStrength * 10000);

	qboolean isBest = qfalse;
	if (context->numEligible == 8 && g_vote_teamgen_new8PlayerAlgo.integer) {
		// this permutation will be favored over the previous permutation if:
		// - it is fairer
		// - it is equally fair, but higher combined strength
		// - it is equally fair and equally high in combined strength, but has better balance of bottom tier players
		// - it is equally fair and equally high in combined strength and equal balance of bottom tier players, but has better balance of top tier players
		// - it is equally fair and equally high in combined strength, equal balance of bottom and top tier players, but has more people on preferred pos
		// - it is equally fair and equally high in combined strength, equal balance of bottom and top tier players, and an equal number of people on preferred pos, but has fewer people on avoided pos
		// - it is equally fair and equally high in combined strength, equal balance of bottom and top tier players, an equal number of people on preferred pos, and an equal number of people on avoided pos, but has better offense-defense diff
		if (iDiff < context->best->iDiff ||
			(iDiff == context->best->iDiff && iTotal > iCurrentBestCombinedStrength) ||
			(iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && bottomTierImbalance < context->best->bottomTierImbalance) ||
			(iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance) ||
			(iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && numOnPreferredPos > context->best->numOnPreferredPos) ||
			(iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
			(iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && offenseDefenseDiff < context->best->offenseDefenseDiff)) {

			if (iDiff < context->best->iDiff)
				TeamGen_DebugPrintf(" <font color=purple>best so far (fairer)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal > iCurrentBestCombinedStrength)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, but higher combined strength)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && bottomTierImbalance < context->best->bottomTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness and combined strength, but better bottom tier balance)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, combined strength, and bottom tier balance, but better top tier balance)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && numOnPreferredPos > context->best->numOnPreferredPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, combined strength, bottom+top tier balance, but more on preferred pos)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, combined strength, bottom+top tier balance, preferred pos, but less on avoided pos)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && offenseDefenseDiff < context->best->offenseDefenseDiff)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, combined strength, bottom+top tier balance, preferred pos, avoided pos, but better offense-defense diff)</font><br/>");
			else
				TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

			isBest = qtrue;
		}
	}
	else {
		// this permutation will be favored over the previous permutation if:
		// - it is fairer
		// - it is equally fair, but higher combined strength
		// - it is equally fair and equally high in combined strength, but has more people on preferred pos
		// - it is equally fair and equally high in combined strength, and has an equal number of people on preferred pos, but has fewer people on avoided pos
		// - it is equally fair and equally high in combined strength, and has an equal number of people on preferred and avoided pos, but has better balance of bottom tier players
		// - it is equally fair and equally high in combined strength, has an equal number of people on preferred and avoided pos, and equal balance of bottom tier players, but has better balance of top tier players
		if (iDiff < context->best->iDiff ||
			(iDiff == context->best->iDiff && iTotal > iCurrentBestCombinedStrength) ||
			(iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && numOnPreferredPos > context->best->numOnPreferredPos) ||
			(iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
			(iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
			(iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)) {
			if (iDiff < context->best->iDiff)
				TeamGen_DebugPrintf(" <font color=purple>best so far (fairer)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal > iCurrentBestCombinedStrength)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, but higher combined strength)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && numOnPreferredPos > context->best->numOnPreferredPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness and combined strength, but more preferred pos)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, combined strength, and preferred pos, but less on avoided pos)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, combined strength, preferred pos, and avoided pos, but better bottom tier balance)</font><br/>");
			else if (iDiff == context->best->iDiff && iTotal == iCurrentBestCombinedStrength && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal fairness, combined strength, preferred pos, avoided pos, and bottom tier balance, but better top tier balance)</font><br/>");
			else
				TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

			isBest = qtrue;
		}
	}

	if (isBest) {
		if (context->avoidedHashesList && context->avoidedHashesList->size > 0) {
			permutationOfTeams_t hashMe = { 0 };
			hashMe.valid = qtrue;
			hashMe.iDiff = iDiff;
			hashMe.offenseDefenseDiff = offenseDefenseDiff;
			hashMe.numOnPreferredPos = numOnPreferredPos;
			hashMe.numOnAvoidedPos = numOnAvoidedPos;
			hashMe.topTierImbalance = topTierImbalance;
			hashMe.bottomTierImbalance = bottomTierImbalance;
			//set totalSkill
			hashMe.teams[0].rawStrength = team1RawStrength;
			hashMe.teams[1].rawStrength = team2RawStrength;
			hashMe.teams[0].relativeStrength = team1RelativeStrength;
			hashMe.teams[1].relativeStrength = team2RelativeStrength;
			hashMe.teams[0].baseId = team1base->accountId;
			hashMe.teams[0].chaseId = team1chase->accountId;
			hashMe.teams[0].offenseId1 = team1offense1->accountId;
			hashMe.teams[0].offenseId2 = team1offense2->accountId;
			hashMe.teams[1].baseId = team2base->accountId;
			hashMe.teams[1].chaseId = team2chase->accountId;
			hashMe.teams[1].offenseId1 = team2offense1->accountId;
			hashMe.teams[1].offenseId2 = team2offense2->accountId;
			for (int i = 0; i < 2; i++) {
				memset(hashMe.teams[i].baseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].chaseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense1Name, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense2Name, 0, MAX_NAME_LENGTH);
			}
			Q_strncpyz(hashMe.teams[0].baseName, team1base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].chaseName, team1chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense1Name, team1offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense2Name, team1offense2->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].baseName, team2base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].chaseName, team2chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense1Name, team2offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense2Name, team2offense2->accountName, MAX_NAME_LENGTH);

			NormalizePermutationOfTeams(&hashMe);
			XXH32_hash_t hash = HashPermutationOfTeams(&hashMe);
			avoidedHash_t *avoided = ListFind(context->avoidedHashesList, HashMatchesAvoidedHash, &hash, NULL);
			if (avoided) {
				TeamGen_DebugPrintf("<font color=darkred>***MATCHES AVOIDED HASH; skipping</font><br/>");
				--context->numPermutations;
				return;
			}
		}

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		context->best->totalSkill = iTotal;
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
		float lowestPlayerRating = 999999;
		if (team1base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team1base->rating[CTFPOSITION_BASE];
		if (team1chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team1chase->rating[CTFPOSITION_CHASE];
		if (team1offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense1->rating[CTFPOSITION_OFFENSE];
		if (team1offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense2->rating[CTFPOSITION_OFFENSE];
		if (team2base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team2base->rating[CTFPOSITION_BASE];
		if (team2chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team2chase->rating[CTFPOSITION_CHASE];
		if (team2offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense1->rating[CTFPOSITION_OFFENSE];
		if (team2offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense2->rating[CTFPOSITION_OFFENSE];
		context->best->lowestPlayerRating = lowestPlayerRating;
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
		TeamGen_DebugPrintf("<br/>");
	}
}

static void TryTeamPermutation_Tryhard(teamGeneratorContext_t *context, const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase,
	const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase,
	const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {

	if (!team1base->rating[CTFPOSITION_BASE] || !team1chase->rating[CTFPOSITION_CHASE] || !team1offense1->rating[CTFPOSITION_OFFENSE] || !team1offense2->rating[CTFPOSITION_OFFENSE] ||
		!team2base->rating[CTFPOSITION_BASE] || !team2chase->rating[CTFPOSITION_CHASE] || !team2offense1->rating[CTFPOSITION_OFFENSE] || !team2offense2->rating[CTFPOSITION_OFFENSE]) {
		return; // at least one player is invalid on their proposed position
	}

	++context->numPermutations;

	if (context->banAvoidedPositions) {
		if ((team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) ||
			(team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) || (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) {
			return;
		}
	}

	double team1RawStrength = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE] + team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
	double team2RawStrength = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE] + team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
	double total = team1RawStrength + team2RawStrength;
	int iTotal = (int)round(total * 10000);
	double team1RelativeStrength = team1RawStrength / total;
	double team2RelativeStrength = team2RawStrength / total;
	double diffPercentage = fabs(team1RelativeStrength - team2RelativeStrength);
	double diff = round(fabs(team1RawStrength - team2RawStrength) / PLAYERRATING_DECIMAL_INCREMENT);
	int iDiff = (int)diff;

	int team1TopTiers = 0, team2TopTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team1TopTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) >= PLAYERRATING_LOW_S) ++team2TopTiers;
	int topTierImbalance = abs(team1TopTiers - team2TopTiers);

	int team1BottomTiers = 0, team2BottomTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	int team1GarbageTiers = 0, team2GarbageTiers = 0;
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team1GarbageTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_LOW_C) ++team2GarbageTiers;
	int garbageTierImbalance = abs(team1GarbageTiers - team2GarbageTiers);

	// reward permutations that put people on preferred positions
	int numOnPreferredPos = 0, numOnAvoidedPos = 0;
	if (team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team1base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team2base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;
	if (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_FIRSTCHOICE; else if (team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_SECONDCHOICE; else if (team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += PREFERREDPOS_VALUE_THIRDCHOICE;

	int team1OffenseDefenseDiff = 0, team2OffenseDefenseDiff = 0;
	{
		double team1DefenseTotal = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE];
		double team1OffenseTotal = team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
		team1OffenseDefenseDiff = (int)round(fabs(team1DefenseTotal - team1OffenseTotal) * 10000);

		double team2DefenseTotal = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE];
		double team2OffenseTotal = team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
		team2OffenseDefenseDiff = (int)round(fabs(team2DefenseTotal - team2OffenseTotal) * 10000);
	}
	int offenseDefenseDiff = team1OffenseDefenseDiff + team2OffenseDefenseDiff;

	TeamGen_DebugPrintf("Tryhard:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
		team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team1base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team1base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team1base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team1base->accountName,
		team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team1chase->accountName,
		team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense1->accountName,
		team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team1offense2->accountName,
		team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE) ? "<font color=red>" : team2base->posPrefs.first & (1 << CTFPOSITION_BASE) ? "<font color=darkgreen>" : team2base->posPrefs.second & (1 << CTFPOSITION_BASE) ? "<font color=silver>" : team2base->posPrefs.third & (1 << CTFPOSITION_BASE) ? "<font color=orange>" : "<font color=black>",
		team2base->accountName,
		team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE) ? "<font color=red>" : team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE) ? "<font color=darkgreen>" : team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE) ? "<font color=silver>" : team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE) ? "<font color=orange>" : "<font color=black>",
		team2chase->accountName,
		team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense1->accountName,
		team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE) ? "<font color=red>" : team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE) ? "<font color=darkgreen>" : team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE) ? "<font color=silver>" : team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE) ? "<font color=orange>" : "<font color=black>",
		team2offense2->accountName,
		team1RawStrength,
		team2RawStrength,
		team1RelativeStrength * 100,
		team2RelativeStrength * 100,
		numOnPreferredPos,
		numOnAvoidedPos,
		garbageTierImbalance,
		team1GarbageTiers,
		team2GarbageTiers,
		bottomTierImbalance,
		team1BottomTiers,
		team2BottomTiers,
		topTierImbalance,
		team1TopTiers,
		team2TopTiers,
		total,
		diff,
		offenseDefenseDiff,
		team1OffenseDefenseDiff,
		team2OffenseDefenseDiff);

	// start with a high cap, then shrink it, then raise it again
	if ((!rerollNum || rerollNum == 2) && g_vote_teamgen_bImbalanceCapWith0OrTwoRerolls.value > 0) {
		double cap = g_vote_teamgen_bImbalanceCapWith0OrTwoRerolls.value;
		cap = (round(cap * 10000) / 10000.0) + 0.00001;
		if (diffPercentage >= cap) {
			TeamGen_DebugPrintf(" difference too great (without reroll).<br/>");
			return;
		}
	}
	else if (rerollNum == 1 && g_vote_teamgen_bImbalanceCapWithOneReroll.value > 0) {
		double cap = g_vote_teamgen_bImbalanceCapWithOneReroll.value;
		cap = (round(cap * 10000) / 10000.0) + 0.00001;
		if (diffPercentage >= cap) {
			TeamGen_DebugPrintf(" difference too great (with one reroll).<br/>");
			return;
		}
	}

	if (garbageTierImbalance >= 2) {
		TeamGen_DebugPrintf(" garbage imbalance too great (>= 2).<br/>");
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team1ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam2OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam2OffenseTier - team1ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 1 chase vs. team 2 offense %d - %d == %d<br/>", highestTeam2OffenseTier, team1ChaseTier, highestTeam2OffenseTier - team1ChaseTier);
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team2ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam1OffenseTier >= PLAYERRATING_LOW_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam1OffenseTier - team2ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense %d - %d == %d<br/>", highestTeam1OffenseTier, team2ChaseTier, highestTeam1OffenseTier - team2ChaseTier);
				return;
			}
		}
	}

	double currentBestCombinedStrength = context->best->teams[0].rawStrength + context->best->teams[1].rawStrength;
	int iCurrentBestCombinedStrength = (int)round(currentBestCombinedStrength * 10000);

	qboolean isBest = qfalse;
	if (context->numEligible == 8 && g_vote_teamgen_new8PlayerAlgo.integer) {
		// this permutation will be favored over the previous permutation if:
		// - it has higher combined strength
		// - it is equally high in combined strength, but fairer
		// - it is equally high in combined strength and equally fair, but has better balance of bottom tier players
		// - it is equally high in combined strength, equally fair, and has equal balance of bottom tier players, but has better balance of top tier players
		// - it is equally high in combined strength, equally fair, has equal balance of bottom and top tier players, but has better offense-defense diff
		// - it is equally high in combined strength, equally fair, has equal balance of bottom and top tier players, and equal offense-defense diff, but has more people on preferred pos
		// - it is equally high in combined strength, equally fair, has equal balance of bottom and top tier players, equal offense-defense diff, and an equal number of people on preferred pos, but has fewer people on avoided pos
		if (iTotal > iCurrentBestCombinedStrength ||
			(iTotal == iCurrentBestCombinedStrength && iDiff < context->best->iDiff) ||
			(iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && bottomTierImbalance < context->best->bottomTierImbalance) ||
			(iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance) ||
			(iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff < context->best->offenseDefenseDiff) ||
			(iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff == context->best->offenseDefenseDiff && numOnPreferredPos > context->best->numOnPreferredPos) ||
			(iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff == context->best->offenseDefenseDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)) {

			if (iTotal > iCurrentBestCombinedStrength)
				TeamGen_DebugPrintf(" <font color=purple>best so far (higher combined strength)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff < context->best->iDiff)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal combined strength, but fairer)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && bottomTierImbalance < context->best->bottomTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal combined strength and fairness, but better bottom tier balance)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal combined strength, fairness, and bottom tier balance, but better top tier balance)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff < context->best->offenseDefenseDiff)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal combined strength, fairness, bottom tier and top tier balance, but better offense-defense diff)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff == context->best->offenseDefenseDiff && numOnPreferredPos > context->best->numOnPreferredPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal combined strength, fairness, bottom+top tier balance, and offense-defense diff, but more on preferred pos)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && offenseDefenseDiff == context->best->offenseDefenseDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (equal combined strength, fairness, bottom+top tier balance, offense-defense diff, and preferred pos, but less on avoided pos)</font><br/>");
			else
				TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

			isBest = qtrue;
		}
	}
	else {
		// this permutation will be favored over the previous permutation if:
		// - it is higher caliber overall
		// - it is equally high caliber, but fairer
		// - it is equally high caliber and equally fair, but has more people on preferred pos
		// - it is equally high caliber, equally fair, and has an equal number of people on preferred pos, but has fewer people on avoided pos
		// - it is equally high caliber, equally fair, and has an equal number of people on preferred and avoided pos, but has better balance of bottom tier players
		// - it is equally high caliber, equally fair, has an equal number of people on preferred and avoided pos and equal balance of bottom tier players, but has better balance of top tier players
		if (iTotal > iCurrentBestCombinedStrength ||
			(iTotal == iCurrentBestCombinedStrength && iDiff < context->best->iDiff) ||
			(iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos) ||
			(iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
			(iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
			(iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)) {
			if (iTotal > iCurrentBestCombinedStrength)
				TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength better)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff < context->best->iDiff)
				TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength equal, but fairer)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && numOnPreferredPos > context->best->numOnPreferredPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength and fairness equal, but more preferred pos)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
				TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength, fairness equal and preferred pos equal, but less on avoided pos)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength, fairness, preferred pos, and avoided pos, but better bottom tier balance)</font><br/>");
			else if (iTotal == iCurrentBestCombinedStrength && iDiff == context->best->iDiff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
				TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength, fairness, preferred pos, avoided pos, and bottom tier balance, but better top tier balance)</font><br/>");
			else
				TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

			isBest = qtrue;
		}
	}
	
	if (isBest) {
		if (context->avoidedHashesList && context->avoidedHashesList->size > 0) {
			permutationOfTeams_t hashMe = { 0 };
			hashMe.valid = qtrue;
			hashMe.iDiff = iDiff;
			hashMe.offenseDefenseDiff = offenseDefenseDiff;
			hashMe.numOnPreferredPos = numOnPreferredPos;
			hashMe.numOnAvoidedPos = numOnAvoidedPos;
			hashMe.topTierImbalance = topTierImbalance;
			hashMe.bottomTierImbalance = bottomTierImbalance;
			//set totalSkill
			hashMe.teams[0].rawStrength = team1RawStrength;
			hashMe.teams[1].rawStrength = team2RawStrength;
			hashMe.teams[0].relativeStrength = team1RelativeStrength;
			hashMe.teams[1].relativeStrength = team2RelativeStrength;
			hashMe.teams[0].baseId = team1base->accountId;
			hashMe.teams[0].chaseId = team1chase->accountId;
			hashMe.teams[0].offenseId1 = team1offense1->accountId;
			hashMe.teams[0].offenseId2 = team1offense2->accountId;
			hashMe.teams[1].baseId = team2base->accountId;
			hashMe.teams[1].chaseId = team2chase->accountId;
			hashMe.teams[1].offenseId1 = team2offense1->accountId;
			hashMe.teams[1].offenseId2 = team2offense2->accountId;
			for (int i = 0; i < 2; i++) {
				memset(hashMe.teams[i].baseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].chaseName, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense1Name, 0, MAX_NAME_LENGTH);
				memset(hashMe.teams[i].offense2Name, 0, MAX_NAME_LENGTH);
			}
			Q_strncpyz(hashMe.teams[0].baseName, team1base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].chaseName, team1chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense1Name, team1offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[0].offense2Name, team1offense2->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].baseName, team2base->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].chaseName, team2chase->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense1Name, team2offense1->accountName, MAX_NAME_LENGTH);
			Q_strncpyz(hashMe.teams[1].offense2Name, team2offense2->accountName, MAX_NAME_LENGTH);

			NormalizePermutationOfTeams(&hashMe);
			XXH32_hash_t hash = HashPermutationOfTeams(&hashMe);
			avoidedHash_t *avoided = ListFind(context->avoidedHashesList, HashMatchesAvoidedHash, &hash, NULL);
			if (avoided) {
				TeamGen_DebugPrintf("<font color=darkred>***MATCHES AVOIDED HASH; skipping</font><br/>");
				--context->numPermutations;
				return;
			}
		}

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		context->best->totalSkill = iTotal;
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
		float lowestPlayerRating = 999999;
		if (team1base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team1base->rating[CTFPOSITION_BASE];
		if (team1chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team1chase->rating[CTFPOSITION_CHASE];
		if (team1offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense1->rating[CTFPOSITION_OFFENSE];
		if (team1offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team1offense2->rating[CTFPOSITION_OFFENSE];
		if (team2base->rating[CTFPOSITION_BASE] < lowestPlayerRating) lowestPlayerRating = team2base->rating[CTFPOSITION_BASE];
		if (team2chase->rating[CTFPOSITION_CHASE] < lowestPlayerRating) lowestPlayerRating = team2chase->rating[CTFPOSITION_CHASE];
		if (team2offense1->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense1->rating[CTFPOSITION_OFFENSE];
		if (team2offense2->rating[CTFPOSITION_OFFENSE] < lowestPlayerRating) lowestPlayerRating = team2offense2->rating[CTFPOSITION_OFFENSE];
		context->best->lowestPlayerRating = lowestPlayerRating;
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
		TeamGen_DebugPrintf("<br/>");
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

	if (context->banAvoidedPositions) {
		if ((offensePlayer1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (offensePlayer2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) ||
			(offensePlayer3->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (offensePlayer4->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) {
			return;
		}
	}

	int orders[6][4] = {
	{1, 2, 3, 4}, {1, 3, 2, 4}, {1, 4, 2, 3}, {2, 3, 1, 4},
	{2, 4, 1, 3}, {3, 4, 1, 2}
	};

	srand(teamGenSeed + context->type);
	FisherYatesShuffle(&orders[0][0], 6, sizeof(orders[0]));
	srand(time(NULL));

	for (int i = 0; i < 6; i++)
		HandleTeam(playerArray, arr, context, c[orders[i][0]], c[orders[i][1]], c[orders[i][2]], c[orders[i][3]]);
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

	if (context->banAvoidedPositions) {
		if ((team1basePlayer->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team1chasePlayer->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) ||
			(team2basePlayer->posPrefs.avoid & (1 << CTFPOSITION_BASE)) || (team2chasePlayer->posPrefs.avoid & (1 << CTFPOSITION_CHASE))) {
			return;
		}
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

	int orders[12][4] = {
		{1, 2, 3, 4}, {2, 1, 3, 4}, {1, 2, 4, 3}, {2, 1, 4, 3},
		{1, 3, 2, 4}, {3, 1, 2, 4}, {1, 3, 4, 2}, {3, 1, 4, 2},
		{1, 4, 2, 3}, {4, 1, 2, 3}, {1, 4, 3, 2}, {4, 1, 3, 2}
	};

	srand(teamGenSeed + context->type);
	FisherYatesShuffle(&orders[0][0], 12, sizeof(orders[0]));
	srand(time(NULL));

	for (int i = 0; i < 12; i++)
		GetOffenseCombinations(playerArray, arr, context, c[orders[i][0]], c[orders[i][1]], c[orders[i][2]], c[orders[i][3]]);

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
static uint64_t PermuteTeams(permutationPlayer_t *playerArray, int numEligible, permutationOfTeams_t *bestOut, PermutationCallback callback, qboolean enforceChaseRule, list_t *avoidedHashesList, qboolean banAvoidedPositions, int type, qboolean enforceImbalanceCaps) {
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
	context.enforceChaseRule = enforceChaseRule;
	context.numEligible = numEligible;
	context.numPermutations = 0;
	context.banAvoidedPositions = banAvoidedPositions;
	context.best->offenseDefenseDiff = 0;
	context.best->totalSkill = 0;
	context.type = type;
	context.enforceImbalanceCaps = enforceImbalanceCaps;
	if (avoidedHashesList && avoidedHashesList->size > 0)
		context.avoidedHashesList = avoidedHashesList;
	else
		context.avoidedHashesList = NULL;
	GetDefenseCombinations(playerArray, intArr, &context);
	free(intArr);

#ifdef DEBUG_GENERATETEAMS
	clock_t end = clock();
	double computeTimePrint = ((double)(end - start)) / CLOCKS_PER_SEC;
	Com_DebugPrintf("Pass compute time: %0.2f seconds\n", computeTimePrint);
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

static ctfPosition_t PermutationHasPlayer(int accountId, permutationOfTeams_t *p) {
	if (!p || !p->valid)
		return CTFPOSITION_UNKNOWN;

	if (p->teams[0].baseId == accountId || p->teams[1].baseId == accountId)
		return CTFPOSITION_BASE;
	if (p->teams[0].chaseId == accountId || p->teams[1].chaseId == accountId)
		return CTFPOSITION_CHASE;
	if (p->teams[0].offenseId1 == accountId || p->teams[1].offenseId1 == accountId)
		return CTFPOSITION_OFFENSE;
	if (p->teams[0].offenseId2 == accountId || p->teams[1].offenseId2 == accountId)
		return CTFPOSITION_OFFENSE;

	return CTFPOSITION_UNKNOWN;
}

static int NumPermutationsOfPlayer(int accountId, permutationOfTeams_t *p1, permutationOfTeams_t *p2, permutationOfTeams_t *p3, permutationOfTeams_t *p4, permutationOfTeams_t *p5, permutationOfTeams_t *p6) {
	int num = 0;
	if (PermutationHasPlayer(accountId, p1))
		++num;
	if (PermutationHasPlayer(accountId, p2))
		++num;
	if (PermutationHasPlayer(accountId, p3))
		++num;
	if (PermutationHasPlayer(accountId, p4))
		++num;
	if (PermutationHasPlayer(accountId, p5))
		++num;
	if (PermutationHasPlayer(accountId, p6))
		++num;
	return num;
}

static double WeightOfPlayer(int accountId, permutationOfTeams_t *p1, permutationOfTeams_t *p2, permutationOfTeams_t *p3, permutationOfTeams_t *p4, permutationOfTeams_t *p5, permutationOfTeams_t *p6) {
	playerRating_t findMe = { 0 };
	findMe.accountId = accountId;
	playerRating_t *ratings = ListFind(&level.ratingList, PlayerRatingAccountIdMatches, &findMe, NULL);
	if (!ratings)
		return 0;

	int num = 0;
	if (PermutationHasPlayer(accountId, p1))
		++num;
	if (PermutationHasPlayer(accountId, p2))
		++num;
	if (PermutationHasPlayer(accountId, p3))
		++num;
	if (PermutationHasPlayer(accountId, p4))
		++num;
	if (PermutationHasPlayer(accountId, p5))
		++num;
	if (PermutationHasPlayer(accountId, p6))
		++num;

	if (num) {
		double totalRating = 0;
		double numRatings = 0;

		if (ratings->rating[CTFPOSITION_BASE]) {
			totalRating += ratings->rating[CTFPOSITION_BASE];
			++numRatings;
		}
		if (ratings->rating[CTFPOSITION_CHASE]) {
			totalRating += ratings->rating[CTFPOSITION_CHASE];
			++numRatings;
		}
		if (ratings->rating[CTFPOSITION_OFFENSE]) {
			totalRating += ratings->rating[CTFPOSITION_OFFENSE];
			++numRatings;
		}

		if (!numRatings)
			return 0;

		double averageRatingOfPlayer = totalRating / numRatings;
		return averageRatingOfPlayer * num;
	}

	return 0;
}

static qboolean GenerateTeams(pugProposal_t *set, permutationOfTeams_t *mostPlayed, permutationOfTeams_t *highestCaliber, permutationOfTeams_t *fairest, permutationOfTeams_t *desired, permutationOfTeams_t *inclusive, permutationOfTeams_t *semiDesired, permutationOfTeams_t *firstChoice, uint64_t *numPermutations, qboolean enforceImbalanceCaps) {
	assert(set);
#ifdef DEBUG_GENERATETEAMS
	clock_t start = clock();
#endif

	if (g_gametype.integer != GT_CTF)
		return qfalse;

#ifdef DEBUG_GENERATETEAMS_PRINT
	if (!inReroll) {
		char whenStr[128] = { 0 };
		time_t now = time(NULL);
		struct tm *t = localtime(&now);
		strftime(whenStr, sizeof(whenStr), "%Y%m%d-%H%M%S", t);
		trap_FS_FOpenFile(va("teamgen-%s.html", whenStr), &debugFile, FS_APPEND);

		char *debugFileStart = "<html><body bgcolor=#aaaaaa>";
		trap_FS_Write(debugFileStart, strlen(debugFileStart), debugFile);
	}
#endif

	TeamGen_Initialize();

	//G_DBGetPlayerRatings(); // the caller should refresh ratings

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
			TeamGenerator_PlayerIsBarredFromTeamGenerator(ent) ||
			(IsRacerOrSpectator(ent) && IsSpecName(ent->client->pers.netname)))
			continue;

		if (ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(ent->client->pers.netname, "elo BOT"))
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

			if (other->client->account && other->client->account->id == ent->client->account->id &&
				!(other->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(other->client->pers.netname, "elo BOT"))) {
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

	// tally up everyone that is eligible
	// also note their name in case they have a position preference like "base only" in it
	int numEligible = 0;
	sortedClient_t sortedClients[MAX_CLIENTS];
	memset(sortedClients, 0, sizeof(sortedClients));
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!eligible[i])
			continue;

		gentity_t *ent = &g_entities[i];

		sortedClients[i].clientNum = i;
#ifdef DEBUG_GENERATETEAMS
		Q_strncpyz(sortedClients[i].accountName, Cvar_VariableString(va("z_debug%d", i + 1)), sizeof(sortedClients[i].accountName));
		account_t acc;
		G_DBGetAccountByName(Cvar_VariableString(va("z_debug%d", i + 1)), &acc);
		sortedClients[i].accountId = acc.id;
		sortedClients[i].team = TEAM_SPECTATOR;
		memcpy(&sortedClients[i].posPrefs, &acc.validPref, sizeof(positionPreferences_t));
#else
		Q_strncpyz(sortedClients[i].accountName, ent->client->account->name, sizeof(sortedClients[i].accountName));
		memcpy(&sortedClients[i].posPrefs, &ent->client->account->validPref, sizeof(positionPreferences_t));
		sortedClients[i].team = ent->client->sess.sessionTeam;
		sortedClients[i].accountId = ent->client->account->id;
#endif

		for (int j = CTFPOSITION_BASE; j <= CTFPOSITION_OFFENSE; j++) {
			if (sortedClients[i].posPrefs.first & (1 << j))
				TeamGen_DebugPrintf("%s has first choice pos %s<br/>", sortedClients[i].accountName, NameForPos(j));
			if (sortedClients[i].posPrefs.second & (1 << j))
				TeamGen_DebugPrintf("%s has second choice pos %s<br/>", sortedClients[i].accountName, NameForPos(j));
			if (sortedClients[i].posPrefs.third & (1 << j))
				TeamGen_DebugPrintf("%s has third choice pos %s<br/>", sortedClients[i].accountName, NameForPos(j));
			if (sortedClients[i].posPrefs.avoid & (1 << j))
				TeamGen_DebugPrintf("%s has avoided pos %s<br/>", sortedClients[i].accountName, NameForPos(j));
		}

		++numEligible;
		if (sortedClients[i].team == TEAM_RED || sortedClients[i].team == TEAM_BLUE)
			++numIngame;
	}

	if (numEligible < 8) {
#ifdef DEBUG_GENERATETEAMS_PRINT
		if (!inReroll) {
			char *debugFileEnd = "</body></html>";
			trap_FS_Write(debugFileEnd, strlen(debugFileEnd), debugFile);
			trap_FS_FCloseFile(debugFile);
		}
#endif
		return qfalse;
	}

	// try to get the best possible teams using a few different approaches
#define MAX_TEAMGENERATORTYPES_PER_SET	(4)
	int numValid = 0;
	uint64_t gotten = 0llu;
	list_t listOfAvoidedHashesPlusHashesGottenOnThisGeneration = { 0 };
	ListCopy(&set->avoidedHashesList, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, sizeof(avoidedHash_t));
	for (int typeIter = TEAMGENERATORTYPE_FIRST; typeIter < NUM_TEAMGENERATORTYPES; typeIter++) {

		// hack to swap computation of ?b to be before ?a so that if there's a permutation that's the most HC *and* is 50-50, ?b grabs it rather than ?a
		teamGeneratorType_t type;
		switch (typeIter) {
		case TEAMGENERATORTYPE_MOSTPLAYED: type = TEAMGENERATORTYPE_HIGHESTRATING; break;
		case TEAMGENERATORTYPE_HIGHESTRATING: type = TEAMGENERATORTYPE_MOSTPLAYED; break;
		default: type = typeIter; break;
		}

		if (numValid >= MAX_TEAMGENERATORTYPES_PER_SET) {
			TeamGen_DebugPrintf("<font color=red>==========Breaking rather than trying type %d because we've already reached %d valid types</font><br/>", type, MAX_TEAMGENERATORTYPES_PER_SET);
			break;
		}

		// we only do the desired pass if and only if it's enabled and there isn't an a/b/c option with nobody on avoided pos
		if (type == TEAMGENERATORTYPE_DESIREDPOS) {
			if (!g_vote_teamgen_enableAppeasing.integer)
				continue;

			if ((inclusive && inclusive->valid) || (semiDesired && semiDesired->valid))
				continue;

			qboolean zeroOnAvoidedPosExists = qfalse;
			if ((mostPlayed && mostPlayed->valid && !mostPlayed->numOnAvoidedPos) ||
				(highestCaliber && highestCaliber->valid && !highestCaliber->numOnAvoidedPos) ||
				(fairest && fairest->valid && !fairest->numOnAvoidedPos)) {
				zeroOnAvoidedPosExists = qtrue;
			}

			if (zeroOnAvoidedPosExists)
				continue;
		}

		if (type == TEAMGENERATORTYPE_SEMIDESIREDPOS) {
			if (!g_vote_teamgen_enableAppeasing.integer)
				continue;

			if ((desired && desired->valid) || (inclusive && inclusive->valid))
				continue;

			qboolean zeroOnAvoidedPosExists = qfalse;
			if ((mostPlayed && mostPlayed->valid && !mostPlayed->numOnAvoidedPos) ||
				(highestCaliber && highestCaliber->valid && !highestCaliber->numOnAvoidedPos) ||
				(fairest && fairest->valid && !fairest->numOnAvoidedPos)) {
				zeroOnAvoidedPosExists = qtrue;
			}

#ifdef DEBUG_GENERATETEAMS
			for (int j = 0; j < MAX_CLIENTS; j++) {
				sortedClient_t *cl = set->clients + j;
				if (!cl->accountName[0])
					continue;

				if (mostPlayed && mostPlayed->valid) {
					int pos = PermutationHasPlayer(cl->accountId, mostPlayed);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (cl->posPrefs.avoid & (1 << pos))
							Com_Printf("Player %s has %s on avoid in ?a\n", cl->accountName, NameForPos(pos));
					}
				}
				if (highestCaliber && highestCaliber->valid) {
					int pos = PermutationHasPlayer(cl->accountId, highestCaliber);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (cl->posPrefs.avoid & (1 << pos))
							Com_Printf("Player %s has %s on avoid in ?b\n", cl->accountName, NameForPos(pos));
			}
		}
				if (fairest && fairest->valid) {
					int pos = PermutationHasPlayer(cl->accountId, fairest);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (cl->posPrefs.avoid & (1 << pos))
							Com_Printf("Player %s has %s on avoid in ?c\n", cl->accountName, NameForPos(pos));
					}
				}
	}
#endif

			if (zeroOnAvoidedPosExists)
				continue;
		}

		if (type == TEAMGENERATORTYPE_INCLUSIVE) {
			if (!g_vote_teamgen_enableInclusive.integer)
				continue;

			if ((desired && desired->valid) || (semiDesired && semiDesired->valid))
				continue; // never do inclusive if the desired slot is already in use

			int numPlayers = 0;
			qboolean gotPlayerWithZeroPermutations = qfalse;
			for (int j = 0; j < MAX_CLIENTS; j++) {
				sortedClient_t *cl = set->clients + j;
				if (!cl->accountName[0])
					continue;

#ifndef DEBUG_GENERATETEAMS
				// make sure this guy hasn't gone spec; we don't want to count him as left out
				// if he decided to become a spectator after the pug proposal passed
				if (cl->clientNum >= 0 && cl->clientNum < MAX_CLIENTS) {
					gentity_t *checkSpec = &g_entities[cl->clientNum];
					if (!checkSpec->inuse || !checkSpec->client || checkSpec->client->pers.connected == CON_DISCONNECTED ||
						!checkSpec->client->account || checkSpec->client->sess.clientType != CLIENT_TYPE_NORMAL ||
						(checkSpec->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(checkSpec->client->pers.netname, "elo BOT")) ||
						TeamGenerator_PlayerIsBarredFromTeamGenerator(checkSpec) ||
						(IsRacerOrSpectator(checkSpec) && IsSpecName(checkSpec->client->pers.netname))) {
						continue;
					}
				}
#endif

				int numPermutationsThisPlayer = NumPermutationsOfPlayer(cl->accountId, mostPlayed, highestCaliber, fairest, desired, inclusive, semiDesired);
				if (!numPermutationsThisPlayer) {
#ifdef DEBUG_GENERATETEAMS
					Com_Printf("Player %s is in no permutations\n", cl->accountName);
#endif
					gotPlayerWithZeroPermutations = qtrue;
				}

				if (mostPlayed && mostPlayed->valid) {
					int pos = PermutationHasPlayer(cl->accountId, mostPlayed);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (cl->posPrefs.first & (1 << pos))
							continue;
					}
				}
				if (highestCaliber && highestCaliber->valid) {
					int pos = PermutationHasPlayer(cl->accountId, highestCaliber);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (cl->posPrefs.first & (1 << pos))
							continue;
					}
				}
				if (fairest && fairest->valid) {
					int pos = PermutationHasPlayer(cl->accountId, fairest);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (cl->posPrefs.first & (1 << pos))
							continue;
					}
				}

				// if we got here, this guy was not on a first choice in a/b/c
#ifdef DEBUG_GENERATETEAMS
				Com_Printf("Player %s has no first choice in a/b/c\n", cl->accountName);
#endif
			}

			if (!gotPlayerWithZeroPermutations)
				continue; // no need to do inclusive pass if everybody is in at least one permutation
		}

		if (type == TEAMGENERATORTYPE_FIRSTCHOICE) {
			if (!g_vote_teamgen_enableFirstChoice.integer)
				continue;

			if ((desired && desired->valid) || (semiDesired && semiDesired->valid) || (inclusive && inclusive->valid))
				continue; // never do first choice if the desired slot is already in use

			if (!(mostPlayed && mostPlayed->valid) && !(highestCaliber && highestCaliber->valid) && !(fairest && fairest->valid))
				continue;

			int numPlayers = 0;
			qboolean gotPlayerWithNoFirstChoice = qfalse;
			for (int j = 0; j < MAX_CLIENTS; j++) {
				sortedClient_t *cl = set->clients + j;
				if (!cl->accountName[0])
					continue;

#ifndef DEBUG_GENERATETEAMS
				// make sure this guy hasn't gone spec; we don't want to count him as left out
				// if he decided to become a spectator after the pug proposal passed
				if (cl->clientNum >= 0 && cl->clientNum < MAX_CLIENTS) {
					gentity_t *checkSpec = &g_entities[cl->clientNum];
					if (!checkSpec->inuse || !checkSpec->client || checkSpec->client->pers.connected == CON_DISCONNECTED ||
						!checkSpec->client->account || checkSpec->client->sess.clientType != CLIENT_TYPE_NORMAL ||
						(checkSpec->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(checkSpec->client->pers.netname, "elo BOT")) ||
						TeamGenerator_PlayerIsBarredFromTeamGenerator(checkSpec) ||
						(IsRacerOrSpectator(checkSpec) && IsSpecName(checkSpec->client->pers.netname))) {
						continue;
					}
				}
#endif

				if (mostPlayed && mostPlayed->valid) {
					int pos = PermutationHasPlayer(cl->accountId, mostPlayed);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (cl->posPrefs.first & (1 << pos))
							continue;
					}
				}
				if (highestCaliber && highestCaliber->valid) {
					int pos = PermutationHasPlayer(cl->accountId, highestCaliber);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (cl->posPrefs.first & (1 << pos))
							continue;
					}
				}
				if (fairest && fairest->valid) {
					int pos = PermutationHasPlayer(cl->accountId, fairest);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (cl->posPrefs.first & (1 << pos))
							continue;
					}
				}

				// if we got here, this guy was not on a first choice in a/b/c
				gotPlayerWithNoFirstChoice = qtrue;
#ifdef DEBUG_GENERATETEAMS
				Com_Printf("Player %s has no first choice in a/b/c\n", cl->accountName);
#endif
			}

			if (!gotPlayerWithNoFirstChoice)
				continue; // no need to do first choice pass if everybody has a first choice
		}
		
		srand(teamGenSeed + (type * 100));
		qsort(&sortedClients, MAX_CLIENTS, sizeof(sortedClient_t), SortClientsForTeamGenerator);
		FisherYatesShuffle(&sortedClients, numEligible, sizeof(sortedClient_t));
		srand(time(NULL));

		permutationPlayer_t *players = calloc(numEligible, sizeof(permutationPlayer_t));
		int index = 0;
		for (int i = 0; i < numEligible; i++) {
			sortedClient_t *client = sortedClients + i;
			permutationPlayer_t *algoPlayer = players + index++;

			// fetch some information about this player
			playerRating_t findMe = { 0 };
			findMe.accountId = client->accountId;
			playerRating_t *positionRatings = ListFind(&level.ratingList, PlayerRatingAccountIdMatches, &findMe, NULL);
			playerRating_t temp = { 0 };
			qboolean gotValidRatings;
			if (!positionRatings) { // if no ratings, assume they are low C tier at everything
				positionRatings = &temp;
				algoPlayer->rating[CTFPOSITION_BASE] = algoPlayer->rating[CTFPOSITION_CHASE] = algoPlayer->rating[CTFPOSITION_OFFENSE] =
					temp.rating[CTFPOSITION_BASE] = temp.rating[CTFPOSITION_CHASE] = temp.rating[CTFPOSITION_OFFENSE] = PlayerTierToRating(PLAYERRATING_LOW_C);
				gotValidRatings = qfalse;
			}
			else {
				gotValidRatings = qtrue;
			}

			qboolean okayToUsePreference = qtrue;
			mostPlayedPos_t findMe2 = { 0 };
			findMe2.accountId = findMe.accountId;
			mostPlayedPos_t *mostPlayedPositions = ListFind(&level.mostPlayedPositionsList, MostPlayedPosMatches, &findMe2, NULL);

			// determine what information we will feed the permutation evaluator based on which pass we're on
			algoPlayer->accountId = findMe.accountId;
			Q_strncpyz(algoPlayer->accountName, client->accountName, sizeof(algoPlayer->accountName));
			algoPlayer->clientNum = client->clientNum;
			if (type == TEAMGENERATORTYPE_MOSTPLAYED && !g_vote_teamgen_aDietB.integer) {
				// get their most played and second most played pos
				if (mostPlayedPositions) {
					algoPlayer->rating[mostPlayedPositions->mostPlayed] = positionRatings->rating[mostPlayedPositions->mostPlayed];
					algoPlayer->rating[mostPlayedPositions->secondMostPlayed] = positionRatings->rating[mostPlayedPositions->secondMostPlayed];
					algoPlayer->posPrefs.first = (1 << mostPlayedPositions->mostPlayed);
					algoPlayer->posPrefs.second = (1 << mostPlayedPositions->secondMostPlayed);
				}
				else {
					// we have no information about positions they have played. maybe it's a new account? try to fall back to their highest rated pos
					double highestRating = 0.0;
					int positionsWithHighestRating = 0;
					if (gotValidRatings) {
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (positionRatings->rating[pos] >= highestRating)
								highestRating = positionRatings->rating[pos];
						}
						if (highestRating) {
							for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
								if (positionRatings->rating[pos] == highestRating)
									positionsWithHighestRating |= (1 << pos);
							}
						}
					}
					if (positionsWithHighestRating) {
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (positionsWithHighestRating & (1 << pos)) {
								algoPlayer->rating[pos] = highestRating;
								algoPlayer->posPrefs.first |= (1 << pos);
							}
						}

						// get their second highest rated pos
						double secondHighestRating = 0.0;
						int positionsWithSecondHighestRating = 0;
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (positionsWithHighestRating & (1 << pos))
								continue;
							if (positionRatings->rating[pos] >= secondHighestRating)
								secondHighestRating = positionRatings->rating[pos];
						}
						if (secondHighestRating) {
							for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
								if (positionsWithHighestRating & (1 << pos))
									continue;
								if (positionRatings->rating[pos] == secondHighestRating)
									positionsWithSecondHighestRating |= (1 << pos);
							}
						}

						if (positionsWithSecondHighestRating) {
							for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
								if (positionsWithHighestRating & (1 << pos))
									continue;
								if (positionsWithSecondHighestRating & (1 << pos)) {
									algoPlayer->rating[pos] = secondHighestRating;
									algoPlayer->posPrefs.second |= (1 << pos);
								}
							}
						}
					}
				}

				// make sure their top preferences are rated
				// one pos 1st and one pos 2nd = rate them both
				// one pos 1st and two tied for 2nd = rate them all
				// two poses tied for 1st and one 2nd = rate the ones tied for 1st
				int topPrefsGotten = 0;
				for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
					if (client->posPrefs.first & (1 << pos)) {
						algoPlayer->rating[pos] = positionRatings->rating[pos];
						++topPrefsGotten;
					}
				}
				if (topPrefsGotten < 2) {
					for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
						if (client->posPrefs.second & (1 << pos)) {
							algoPlayer->rating[pos] = positionRatings->rating[pos];
						}
					}
				}
			}
			else if (type == TEAMGENERATORTYPE_HIGHESTRATING || (type == TEAMGENERATORTYPE_MOSTPLAYED && g_vote_teamgen_aDietB.integer)) {
				// get their highest rated pos
				double highestRating = 0.0;
				int positionsWithHighestRating = 0;
				if (gotValidRatings) {
					for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
						if (positionRatings->rating[pos] >= highestRating)
							highestRating = positionRatings->rating[pos];
					}
					if (highestRating) {
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (positionRatings->rating[pos] == highestRating)
								positionsWithHighestRating |= (1 << pos);
						}
					}
				}
				if (positionsWithHighestRating) {
					if (PlayerTierFromRating(highestRating) <= PLAYERRATING_LOW_C && mostPlayedPositions) {
						// special case for baddies; just assume that their most-played positions are higher caliber
						algoPlayer->rating[mostPlayedPositions->mostPlayed] = positionRatings->rating[mostPlayedPositions->mostPlayed];
						algoPlayer->rating[mostPlayedPositions->secondMostPlayed] = positionRatings->rating[mostPlayedPositions->secondMostPlayed];
						algoPlayer->posPrefs.first = (1 << mostPlayedPositions->mostPlayed);
						algoPlayer->posPrefs.second = (1 << mostPlayedPositions->secondMostPlayed);
						okayToUsePreference = qfalse;
					}
					else {
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (positionsWithHighestRating & (1 << pos)) {
								algoPlayer->rating[pos] = highestRating;
								algoPlayer->posPrefs.first |= (1 << pos);
							}
						}

						// get their second highest rated pos
						double secondHighestRating = 0.0;
						int positionsWithSecondHighestRating = 0;
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (positionsWithHighestRating & (1 << pos))
								continue;
							if (positionRatings->rating[pos] >= secondHighestRating)
								secondHighestRating = positionRatings->rating[pos];
						}
						if (secondHighestRating) {
							for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
								if (positionsWithHighestRating & (1 << pos))
									continue;
								if (positionRatings->rating[pos] == secondHighestRating)
									positionsWithSecondHighestRating |= (1 << pos);
							}
						}

						if (positionsWithSecondHighestRating) {
							for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
								if (positionsWithHighestRating & (1 << pos))
									continue;
								if (positionsWithSecondHighestRating & (1 << pos)) {
									algoPlayer->rating[pos] = secondHighestRating;
									algoPlayer->posPrefs.second |= (1 << pos);
								}
							}
						}

						// make sure their top preferences are rated
						// one pos 1st and one pos 2nd = rate them both
						// one pos 1st and two tied for 2nd = rate them all
						// two poses tied for 1st and one 2nd = rate the ones tied for 1st
						int topPrefsGotten = 0;
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (client->posPrefs.first & (1 << pos)) {
								algoPlayer->rating[pos] = positionRatings->rating[pos];
								++topPrefsGotten;
							}
						}
						if (topPrefsGotten < 2) {
							for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
								if (client->posPrefs.second & (1 << pos)) {
									algoPlayer->rating[pos] = positionRatings->rating[pos];
								}
							}
						}
					}
				}
				else {
					// we have no ratings for this player. maybe it's a new account? try to fall back to their most played pos
					if (mostPlayedPositions) {
						algoPlayer->rating[mostPlayedPositions->mostPlayed] = positionRatings->rating[mostPlayedPositions->mostPlayed];
						algoPlayer->rating[mostPlayedPositions->secondMostPlayed] = positionRatings->rating[mostPlayedPositions->secondMostPlayed];
						algoPlayer->posPrefs.first = (1 << mostPlayedPositions->mostPlayed);
						algoPlayer->posPrefs.second = (1 << mostPlayedPositions->secondMostPlayed);
					}
					
					// make sure their top preferences are rated
					// one pos 1st and one pos 2nd = rate them both
					// one pos 1st and two tied for 2nd = rate them all
					// two poses tied for 1st and one 2nd = rate the ones tied for 1st
					int topPrefsGotten = 0;
					for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
						if (client->posPrefs.first & (1 << pos)) {
							algoPlayer->rating[pos] = positionRatings->rating[pos];
							++topPrefsGotten;
						}
					}
					if (topPrefsGotten < 2) {
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (client->posPrefs.second & (1 << pos)) {
								algoPlayer->rating[pos] = positionRatings->rating[pos];
							}
						}
					}
				}
			}
			else if (type == TEAMGENERATORTYPE_FAIREST || type == TEAMGENERATORTYPE_DESIREDPOS || type == TEAMGENERATORTYPE_SEMIDESIREDPOS || type == TEAMGENERATORTYPE_INCLUSIVE || type == TEAMGENERATORTYPE_FIRSTCHOICE) {
				// get every pos they play, with preference for their most played and second most played
				memcpy(algoPlayer->rating, positionRatings->rating, sizeof(algoPlayer->rating));

				if (mostPlayedPositions) {
					algoPlayer->rating[mostPlayedPositions->mostPlayed] = positionRatings->rating[mostPlayedPositions->mostPlayed];
					algoPlayer->rating[mostPlayedPositions->secondMostPlayed] = positionRatings->rating[mostPlayedPositions->secondMostPlayed];
					algoPlayer->rating[mostPlayedPositions->thirdMostPlayed] = positionRatings->rating[mostPlayedPositions->thirdMostPlayed];
					algoPlayer->posPrefs.first = (1 << mostPlayedPositions->mostPlayed);
					algoPlayer->posPrefs.second = (1 << mostPlayedPositions->secondMostPlayed);
					algoPlayer->posPrefs.third = (1 << mostPlayedPositions->thirdMostPlayed);
				}
				else {
					// we have no information about positions they have played. maybe it's a new account? try to fall back to their highest rated pos
					double highestRating = 0.0;
					int positionsWithHighestRating = 0;
					if (gotValidRatings) {
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (positionRatings->rating[pos] >= highestRating)
								highestRating = positionRatings->rating[pos];
						}
						if (highestRating) {
							for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
								if (positionRatings->rating[pos] == highestRating)
									positionsWithHighestRating |= (1 << pos);
							}
						}
					}
					if (positionsWithHighestRating) {
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (positionsWithHighestRating & (1 << pos)) {
								algoPlayer->rating[pos] = highestRating;
								algoPlayer->posPrefs.first |= (1 << pos);
							}
						}

						// get their second highest rated pos
						double secondHighestRating = 0.0;
						int positionsWithSecondHighestRating = 0;
						for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
							if (positionsWithHighestRating & (1 << pos))
								continue;
							if (positionRatings->rating[pos] >= secondHighestRating)
								secondHighestRating = positionRatings->rating[pos];
						}
						if (secondHighestRating) {
							for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
								if (positionsWithHighestRating & (1 << pos))
									continue;
								if (positionRatings->rating[pos] == secondHighestRating)
									positionsWithSecondHighestRating |= (1 << pos);
							}
						}

						if (positionsWithSecondHighestRating) {
							for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
								if (positionsWithHighestRating & (1 << pos))
									continue;
								if (positionsWithSecondHighestRating & (1 << pos)) {
									algoPlayer->rating[pos] = secondHighestRating;
									algoPlayer->posPrefs.second |= (1 << pos);
								}
							}
						}
					}
				}

				// make sure any positions they have put 1/2/3 are rated, even if we need to fabricate a low rating
				for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
					if ((client->posPrefs.first & (1 << pos)) || (client->posPrefs.second & (1 << pos)) || (client->posPrefs.third & (1 << pos))) {
						if (positionRatings->rating[pos] > 0) {
							// they have a real rating on this pos
							algoPlayer->rating[pos] = positionRatings->rating[pos];
						}
						else {
							// they are interested in a pos they are unrated on; make up a rating based on their lowest legit rating
							int lowestTier = 999;
							for (int checkPos = CTFPOSITION_BASE; checkPos <= CTFPOSITION_OFFENSE; checkPos++) {
								int tier = PlayerTierFromRating(positionRatings->rating[checkPos]);
								if (tier != PLAYERRATING_UNRATED && tier < lowestTier)
									lowestTier = tier;
							}

							if (lowestTier == 999) { // no legit rating whatsoever; just assume C-
								algoPlayer->rating[pos] = PlayerTierToRating(PLAYERRATING_LOW_C);
							}
							else { // they have a legit rating on *some* position; reduce it by two minor tiers and use that for this position
								int fabricatedTier = lowestTier - 2;
								if (fabricatedTier < PLAYERRATING_MID_D)
									fabricatedTier = PLAYERRATING_MID_D;
								algoPlayer->rating[pos] = PlayerTierToRating(fabricatedTier);
							}
						}
					}
				}

				// unrate them on avoided pos in the appeasing pass
				if (type == TEAMGENERATORTYPE_DESIREDPOS) {
					for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
						if (client->posPrefs.avoid & (1 << pos))
							algoPlayer->rating[pos] = PlayerTierToRating(PLAYERRATING_UNRATED);
					}
				}
			}

			// try to get them in their preferred positions if possible
			if (okayToUsePreference) {
				if (client->posPrefs.first) {
					// if client has a first preference, handle demotions and assignments
					if (algoPlayer->posPrefs.first) {
						if (algoPlayer->posPrefs.second) {
							if (algoPlayer->posPrefs.third) {
								// there's an existing third preference
							}
							algoPlayer->posPrefs.third = algoPlayer->posPrefs.second; // demote second to third
						}
						algoPlayer->posPrefs.second = algoPlayer->posPrefs.first; // demote first to second
					}
					algoPlayer->posPrefs.first = client->posPrefs.first; // assign client's first preference

					if (client->posPrefs.second) {
						if (algoPlayer->posPrefs.second) {
							if (algoPlayer->posPrefs.third) {
								// there's an existing third preference
							}
							algoPlayer->posPrefs.third = algoPlayer->posPrefs.second; // demote second to third
						}
						algoPlayer->posPrefs.second = client->posPrefs.second; // assign client's second preference

						if (client->posPrefs.third) {
							algoPlayer->posPrefs.third = client->posPrefs.third; // assign client's third preference
						}
					}
				}

				if (client->posPrefs.first) {
					// if client has a first preference, handle demotions and assignments
					if (algoPlayer->posPrefs.first) {
						if (algoPlayer->posPrefs.second) {
							if (algoPlayer->posPrefs.third) {
								// there's an existing third preference
							}
							algoPlayer->posPrefs.third = algoPlayer->posPrefs.second; // demote second to third
						}
						algoPlayer->posPrefs.second = algoPlayer->posPrefs.first; // demote first to second
					}
					algoPlayer->posPrefs.first = client->posPrefs.first; // assign client's first preference

					if (client->posPrefs.second) {
						if (algoPlayer->posPrefs.second) {
							if (algoPlayer->posPrefs.third) {
								// there's an existing third preference
							}
							algoPlayer->posPrefs.third = algoPlayer->posPrefs.second; // demote second to third
						}
						algoPlayer->posPrefs.second = client->posPrefs.second; // assign client's second preference

						if (client->posPrefs.third) {
							algoPlayer->posPrefs.third = client->posPrefs.third; // assign client's third preference
						}
					}
				}

				// sanity check to ensure no bitwise conflicts in preferences
				for (int j = 0; j <= 2; j++) {
					if ((algoPlayer->posPrefs.first & (1 << j)) != 0) {
						// cancel out the bit from second and third if present
						algoPlayer->posPrefs.second &= ~(1 << j);
						algoPlayer->posPrefs.third &= ~(1 << j);
					}
					if ((algoPlayer->posPrefs.second & (1 << j)) != 0) {
						// cancel out the bit from third if present
						algoPlayer->posPrefs.third &= ~(1 << j);
					}
				}

				if (client->posPrefs.avoid)
					algoPlayer->posPrefs.avoid = client->posPrefs.avoid;
			}

			TeamGen_DebugPrintf("%s: first %d, second %d, third %d, avoid %d --- %s base,   %s chase,   %s offense<br/>",
				algoPlayer->accountName,
				algoPlayer->posPrefs.first,
				algoPlayer->posPrefs.second,
				algoPlayer->posPrefs.third,
				algoPlayer->posPrefs.avoid,
				PlayerRatingToStringHTML(PlayerTierFromRating(algoPlayer->rating[CTFPOSITION_BASE])),
				PlayerRatingToStringHTML(PlayerTierFromRating(algoPlayer->rating[CTFPOSITION_CHASE])),
				PlayerRatingToStringHTML(PlayerTierFromRating(algoPlayer->rating[CTFPOSITION_OFFENSE]))
				);
		}

		if (type == TEAMGENERATORTYPE_INCLUSIVE) {
			for (int j = 0; j < numEligible; j++) {
				permutationPlayer_t *algoPlayer = players + j;
				if (!algoPlayer->accountName[0])
					continue;

				algoPlayer->numPermutationsIn = NumPermutationsOfPlayer(algoPlayer->accountId, mostPlayed, highestCaliber, fairest, NULL, NULL, NULL);
			}

			for (int j = 0; j < numEligible; j++) {
				permutationPlayer_t *algoPlayer = players + j;
				if (!algoPlayer->accountName[0])
					continue;

				if (mostPlayed && mostPlayed->valid) {
					int pos = PermutationHasPlayer(algoPlayer->accountId, mostPlayed);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (algoPlayer->posPrefs.first & (1 << pos))
							continue;
					}
				}
				if (highestCaliber && highestCaliber->valid) {
					int pos = PermutationHasPlayer(algoPlayer->accountId, highestCaliber);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (algoPlayer->posPrefs.first & (1 << pos))
							continue;
					}
				}
				if (fairest && fairest->valid) {
					int pos = PermutationHasPlayer(algoPlayer->accountId, fairest);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (algoPlayer->posPrefs.first & (1 << pos))
							continue;
					}
				}

				// if we got here, this guy was not on a first choice in a/b/c
				algoPlayer->notOnFirstChoiceInAbc = qtrue;
			}
		}
		else if (type == TEAMGENERATORTYPE_SEMIDESIREDPOS) {
			for (int j = 0; j < numEligible; j++) {
				permutationPlayer_t *algoPlayer = players + j;
				if (!algoPlayer->accountName[0])
					continue;

				algoPlayer->numPermutationsIn = NumPermutationsOfPlayer(algoPlayer->accountId, mostPlayed, highestCaliber, fairest, NULL, NULL, NULL);

				ctfPosition_t posInMostPlayed = PermutationHasPlayer(algoPlayer->accountId, mostPlayed);
				if (posInMostPlayed && !(algoPlayer->posPrefs.avoid & (1 << posInMostPlayed)))
					++algoPlayer->numPermutationsInAndNonAvoidedPos;

				ctfPosition_t posInHighestCaliber = PermutationHasPlayer(algoPlayer->accountId, highestCaliber);
				if (posInHighestCaliber && !(algoPlayer->posPrefs.avoid & (1 << posInHighestCaliber)))
					++algoPlayer->numPermutationsInAndNonAvoidedPos;

				ctfPosition_t posInFairest = PermutationHasPlayer(algoPlayer->accountId, fairest);
				if (posInFairest && !(algoPlayer->posPrefs.avoid & (1 << posInFairest)))
					++algoPlayer->numPermutationsInAndNonAvoidedPos;
			}
		}
		else if (type == TEAMGENERATORTYPE_FIRSTCHOICE) {
			for (int j = 0; j < numEligible; j++) {
				permutationPlayer_t *algoPlayer = players + j;
				if (!algoPlayer->accountName[0])
					continue;

				if (mostPlayed && mostPlayed->valid) {
					int pos = PermutationHasPlayer(algoPlayer->accountId, mostPlayed);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (algoPlayer->posPrefs.first & (1 << pos))
							continue;
					}
				}
				if (highestCaliber && highestCaliber->valid) {
					int pos = PermutationHasPlayer(algoPlayer->accountId, highestCaliber);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (algoPlayer->posPrefs.first & (1 << pos))
							continue;
					}
				}
				if (fairest && fairest->valid) {
					int pos = PermutationHasPlayer(algoPlayer->accountId, fairest);
					if (pos != CTFPOSITION_UNKNOWN) {
						if (algoPlayer->posPrefs.first & (1 << pos))
							continue;
					}
				}

				// if we got here, this guy was not on a first choice in a/b/c
				algoPlayer->notOnFirstChoiceInAbc = qtrue;
			}
		}

		// evaluate every possible permutation of teams for this teamgen type
		permutationOfTeams_t try1 = { 0 }, try2 = { 0 };
		try1.iDiff = try2.iDiff = 999999999;
		try1.totalNumPermutations = try2.totalNumPermutations = 999999;
		try1.totalSkill = try2.totalSkill = 0;
		PermutationCallback callback;
		if (type == TEAMGENERATORTYPE_HIGHESTRATING) {
			callback = TryTeamPermutation_Tryhard; // prefer hc
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("Using TryTeamPermutation_Tryhard\n");
			TeamGen_DebugPrintf("<font color=darkgreen>==========Using TryTeamPermutation_Tryhard==========</font><br/>");
#endif
		}
		else if (type == TEAMGENERATORTYPE_FAIREST) {
			callback = TryTeamPermutation_Fairest; // prefer fairness followed by pos prefs followed by balance considerations
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("Using TryTeamPermutation_Fairest\n");
			TeamGen_DebugPrintf("<font color=darkgreen>==========Using TryTeamPermutation_Fairest==========</font><br/>");
#endif
		}
		else if (type == TEAMGENERATORTYPE_INCLUSIVE) {
			callback = TryTeamPermutation_Inclusive; // prefer including people not in a/b/c
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("Using TryTeamPermutation_Inclusive\n");
			TeamGen_DebugPrintf("<font color=darkgreen>==========Using TryTeamPermutation_Inclusive==========</font><br/>");
#endif
		}
		else if (type == TEAMGENERATORTYPE_DESIREDPOS) {
			callback = TryTeamPermutation_Appeasing; // same as regular method, but require no avoids and 50-50
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("Using TryTeamPermutation_Appeasing\n");
			TeamGen_DebugPrintf("<font color=darkgreen>==========Using TryTeamPermutation_Appeasing==========</font><br/>");
#endif
		}
		else if (type == TEAMGENERATORTYPE_SEMIDESIREDPOS) {
			callback = TryTeamPermutation_SemiAppeasing; // prefer getting people only on avoids in a/b/c onto non-avoids
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("Using TryTeamPermutation_SemiAppeasing\n");
			TeamGen_DebugPrintf("<font color=darkgreen>==========Using TryTeamPermutation_SemiAppeasing==========</font><br/>");
#endif
		}
		else if (type == TEAMGENERATORTYPE_FIRSTCHOICE) {
			callback = TryTeamPermutation_FirstChoice;
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("Using TryTeamPermutation_FirstChoice\n");
			TeamGen_DebugPrintf("<font color=darkgreen>==========Using TryTeamPermutation_FirstChoice==========</font><br/>");
#endif
		}
		else if (g_vote_teamgen_aDietB.integer) {
			callback = TryTeamPermutation_SemiTryhard; // prefer fairness followed by hc
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("Using TryTeamPermutation_SemiTryhard\n");
			TeamGen_DebugPrintf("<font color=darkgreen>==========Using TryTeamPermutation_SemiTryhard==========</font><br/>");
#endif
		}
		else {
			callback = TryTeamPermutation; // prefer fairness followed by pos prefs
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("Using TryTeamPermutation\n");
			TeamGen_DebugPrintf("<font color=darkgreen>==========Using TryTeamPermutation==========</font><br/>");
#endif
		}
		TeamGen_DebugPrintf("<font color=darkgreen>==========Permuting teams with type %d and enforceImbalanceCaps %d==========</font><br/>", type, enforceImbalanceCaps);

		uint64_t thisGotten = PermuteTeams(&players[0], numEligible, &try1, callback, qtrue, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qtrue, type, enforceImbalanceCaps);
		if (thisGotten > gotten)
			gotten = thisGotten;

		qboolean allowSecondTry = qtrue;

		if (!try1.valid && type == TEAMGENERATORTYPE_DESIREDPOS)
			allowSecondTry = qfalse;

		if (!try1.valid && type != TEAMGENERATORTYPE_DESIREDPOS) {
			TeamGen_DebugPrintf("<font color=orange>==========No valid permutation for type %d; trying again without chase rule==========</font><br/>", type);
			memset(&try1, 0, sizeof(try1));
			try1.iDiff = 999999999;
			try1.totalNumPermutations = 999999;
			try1.totalSkill = 0;
			thisGotten = PermuteTeams(&players[0], numEligible, &try1, callback, qfalse, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qtrue, type, enforceImbalanceCaps);
			if (thisGotten > gotten)
				gotten = thisGotten;

			if (!try1.valid) {
				TeamGen_DebugPrintf("<font color=orange>==========No valid permutation without chase rule for type %d; trying again with chase rule but without banning avoided pos==========</font><br/>", type);
				allowSecondTry = qfalse;

				memset(&try1, 0, sizeof(try1));
				try1.iDiff = 999999999;
				try1.totalNumPermutations = 999999;
				try1.totalSkill = 0;
				thisGotten = PermuteTeams(&players[0], numEligible, &try1, callback, qtrue, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps);
				if (thisGotten > gotten)
					gotten = thisGotten;

				if (!try1.valid) {
					TeamGen_DebugPrintf("<font color=orange>==========No valid permutation with chase rule but without banning avoided pos for type %d; trying again without chase rule AND without banning avoided pos==========</font><br/>", type);
					memset(&try1, 0, sizeof(try1));
					try1.iDiff = 999999999;
					try1.totalNumPermutations = 999999;
					try1.totalSkill = 0;
					thisGotten = PermuteTeams(&players[0], numEligible, &try1, callback, qfalse, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps);
					if (thisGotten > gotten)
						gotten = thisGotten;
				}
			}
		}

		// if the permutation banning avoided positions is not 50-50, try again without banning avoided pos
		permutationOfTeams_t *thisPermutation;
		if (allowSecondTry) {
			if ((type == TEAMGENERATORTYPE_HIGHESTRATING || (type == TEAMGENERATORTYPE_MOSTPLAYED && g_vote_teamgen_aDietB.integer)) && try1.valid) {
				TeamGen_DebugPrintf("<font color=orange>==========Attempting HC type %d without banning avoided pos to see if we can do better==========</font><br/>", type);
				thisGotten = PermuteTeams(&players[0], numEligible, &try2, callback, qtrue, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps);
				if (thisGotten > gotten)
					gotten = thisGotten;

				if (!try2.valid) {
					TeamGen_DebugPrintf("<font color=orange>==========No valid permutation for HC type %d without banning avoided pos; trying again without chase rule==========</font><br/>", type);
					memset(&try2, 0, sizeof(try2));
					try2.iDiff = 999999999;
					try2.totalNumPermutations = 999999;
					try2.totalSkill = 0;
					thisGotten = PermuteTeams(&players[0], numEligible, &try2, callback, qfalse, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps);
					if (thisGotten > gotten)
						gotten = thisGotten;
				}

				if (try2.valid) {
					if (try2.totalSkill > try1.totalSkill || (try2.totalSkill == try1.totalSkill && try2.iDiff < try1.iDiff)) {
						thisPermutation = &try2;
						TeamGen_DebugPrintf("<font color=orange>==========Second pass on type %d with banned avoided pos yields higher skill or fairer result; using avoided pos unbanned permutation==========</font><br/>", type);
					}
					else {
						thisPermutation = &try1;
						TeamGen_DebugPrintf("<font color=orange>==========Unable to do better on second pass for type %d; using avoided pos banned permutation==========</font><br/>", type);
					}
				}
				else {
					thisPermutation = &try1;
					TeamGen_DebugPrintf("<font color=orange>==========Unable to get valid permutation for second pass on type %d; using avoided pos banned permutation==========</font><br/>", type);
				}
			}
			else if (type != TEAMGENERATORTYPE_DESIREDPOS && try1.valid && try1.iDiff > 0) {
				TeamGen_DebugPrintf("<font color=orange>==========Diff is > 0 for type %d; attempting without banning avoided pos==========</font><br/>", type);
				thisGotten = PermuteTeams(&players[0], numEligible, &try2, callback, qtrue, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps);
				if (thisGotten > gotten)
					gotten = thisGotten;

				if (!try2.valid) {
					TeamGen_DebugPrintf("<font color=orange>==========No valid permutation for type %d without banning avoided pos; trying again without chase rule==========</font><br/>", type);
					memset(&try2, 0, sizeof(try2));
					try2.iDiff = 999999999;
					try2.totalNumPermutations = 999999;
					try2.totalSkill = 0;
					thisGotten = PermuteTeams(&players[0], numEligible, &try2, callback, qfalse, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps);
					if (thisGotten > gotten)
						gotten = thisGotten;
				}

				if (try2.valid && try2.iDiff < try1.iDiff) {
					thisPermutation = &try2;
					TeamGen_DebugPrintf("<font color=orange>==========Allowing avoided pos is fairer for type %d; using that==========</font><br/>", type);
				}
				else {
					thisPermutation = &try1;
					TeamGen_DebugPrintf("<font color=orange>==========Allowing avoided pos is NOT fairer for type %d; using avoided pos banned permutation==========</font><br/>", type);
				}
			}
			else {
				thisPermutation = &try1;
			}
		}
		else {
			thisPermutation = &try1;
		}

		free(players);

		// fucking redundant sanity check to make sure we don't have multiple ?d's
		if (type == TEAMGENERATORTYPE_INCLUSIVE && ((desired && desired->valid) || (semiDesired && semiDesired->valid))) {
			TeamGen_DebugPrintf("<font color=red>==========Discarding extraneous type %d.==========</font><br/>", type);
			continue;
		}
		else if (type == TEAMGENERATORTYPE_DESIREDPOS && ((inclusive && inclusive->valid) || (semiDesired && semiDesired->valid))) {
			TeamGen_DebugPrintf("<font color=red>==========Discarding extraneous type %d.==========</font><br/>", type);
			continue;
		}
		else if (type == TEAMGENERATORTYPE_SEMIDESIREDPOS && ((inclusive && inclusive->valid) || (desired && desired->valid))) {
			TeamGen_DebugPrintf("<font color=red>==========Discarding extraneous type %d.==========</font><br/>", type);
			continue;
		}
		else if (type == TEAMGENERATORTYPE_FIRSTCHOICE && ((inclusive && inclusive->valid) || (desired && desired->valid) || (semiDesired && semiDesired->valid))) {
			TeamGen_DebugPrintf("<font color=red>==========Discarding extraneous type %d.==========</font><br/>", type);
			continue;
		}

		if (!thisPermutation->valid) {
			// still couldn't get a valid permutation for this teamgen type, even without the chase rule
			TeamGen_DebugPrintf("<font color=red>==========No valid permutation for type %d.==========</font><br/>", type);
			continue;
		}

		NormalizePermutationOfTeams(thisPermutation);
		thisPermutation->hash = HashPermutationOfTeams(thisPermutation);

		// sanity double check for whether this hash matches one we already got for a previously evaluated type (shouldn't really happen)
		int weAlreadyGotThisHash = -1;
		for (int compareType = TEAMGENERATORTYPE_FIRST; compareType < /*type*/NUM_TEAMGENERATORTYPES; compareType++) {
			if (compareType == type)
				continue;
			permutationOfTeams_t **compare = NULL;

			switch (compareType) {
			case TEAMGENERATORTYPE_MOSTPLAYED:		compare = &mostPlayed; break;
			case TEAMGENERATORTYPE_HIGHESTRATING:	compare = &highestCaliber; break;
			case TEAMGENERATORTYPE_FAIREST:			compare = &fairest; break;
			case TEAMGENERATORTYPE_INCLUSIVE:		compare = &inclusive; break;
			case TEAMGENERATORTYPE_DESIREDPOS:		compare = &desired; break;
			case TEAMGENERATORTYPE_SEMIDESIREDPOS:	compare = &semiDesired; break;
			case TEAMGENERATORTYPE_FIRSTCHOICE:		compare = &firstChoice; break;
			}

			if (compare && *compare && (*compare)->valid && (*compare)->hash == thisPermutation->hash) {
				weAlreadyGotThisHash = compareType;
				break;
			}
		}
		if (weAlreadyGotThisHash != -1) {
			TeamGen_DebugPrintf("<font color=red>==========For type %d: we already got this hash back on type %d. Not using it.==========</font><br/>", type, weAlreadyGotThisHash);
			continue;
		}

		// copy this permutation to the appropriate supplied pointer
		permutationOfTeams_t **target = NULL;
		switch (type) {
		case TEAMGENERATORTYPE_MOSTPLAYED:		target = &mostPlayed; break;
		case TEAMGENERATORTYPE_HIGHESTRATING:	target = &highestCaliber; break;
		case TEAMGENERATORTYPE_FAIREST:			target = &fairest; break;
		case TEAMGENERATORTYPE_INCLUSIVE:		target = &inclusive; break;
		case TEAMGENERATORTYPE_DESIREDPOS:		target = &desired; break;
		case TEAMGENERATORTYPE_SEMIDESIREDPOS:	target = &semiDesired; break;
		case TEAMGENERATORTYPE_FIRSTCHOICE:		target = &firstChoice; break;
		default: assert(qfalse);
		}
		if (target && *target) {
			memcpy(*target, thisPermutation, sizeof(permutationOfTeams_t));
			avoidedHash_t *avoid = ListAdd(&listOfAvoidedHashesPlusHashesGottenOnThisGeneration, sizeof(avoidedHash_t));
			avoid->hash = thisPermutation->hash;
			++numValid;
		}

	}

	if (!numValid && enforceImbalanceCaps) {
		TeamGen_DebugPrintf("<font color=blue>!!!!!!!!Did not get any valid permutations, retrying without enforcing imbalance caps!!!!!!!!</font><br/>");
		return GenerateTeams(set, mostPlayed, highestCaliber, fairest, desired, inclusive, semiDesired, firstChoice, numPermutations, qfalse);
	}

	if (numPermutations)
		*numPermutations = gotten;

#ifdef DEBUG_GENERATETEAMS
	clock_t end = clock();
	double computeTimePrint = ((double)(end - start)) / CLOCKS_PER_SEC;
	Com_DebugPrintf("Total compute time: %0.2f seconds\n", computeTimePrint);
#endif

#ifdef DEBUG_GENERATETEAMS_PRINT
	if (!inReroll) {
		char *debugFileEnd = "</body></html>";
		trap_FS_Write(debugFileEnd, strlen(debugFileEnd), debugFile);
		trap_FS_FCloseFile(debugFile);
	}
#endif

	ListClear(&listOfAvoidedHashesPlusHashesGottenOnThisGeneration);

	return !!(numValid);
}

#if 0
// program to calculate lowest possible sum of squares
#include <stdio.h>
#include <math.h>
#include <float.h>

double calculate_lowest_sum_of_squares(int num_players, int total_slots) {
	double expected_slots = (double)total_slots / num_players;

	int slots_per_player = total_slots / num_players;
	int extra_slots = total_slots % num_players;

	double sum_of_squares = 0.0;

	for (int i = 0; i < num_players; i++) {
		int actual_slots = slots_per_player + (i < extra_slots ? 1 : 0);
		double diff = actual_slots - expected_slots;
		sum_of_squares += diff * diff;
	}

	return sum_of_squares;
}

int main() {
	int min_players = 8;
	int max_players = 32;

	printf("For 3 sets of 8 players (24 slots):\n");
	for (int num_players = min_players; num_players <= max_players; num_players++) {
		double lowest_sum_of_squares = calculate_lowest_sum_of_squares(num_players, 24);
		printf("The lowest possible sum of squares for %d players is: %.*g\n", num_players, DBL_DIG, lowest_sum_of_squares);
	}

	printf("\nFor 4 sets of 8 players (32 slots):\n");
	for (int num_players = min_players; num_players <= max_players; num_players++) {
		double lowest_sum_of_squares = calculate_lowest_sum_of_squares(num_players, 32);
		printf("The lowest possible sum of squares for %d players is: %.*g\n", num_players, DBL_DIG, lowest_sum_of_squares);
	}

	return 0;
}
#endif

#define MAX_TEAMGEN_ITERATIONS 32

// wrapper for calling GenerateTeams several times with the goal of finding the fairest distribution of player slots
static qboolean GenerateTeamsIteratively(pugProposal_t *set, permutationOfTeams_t *mostPlayed, permutationOfTeams_t *highestCaliber, permutationOfTeams_t *fairest, permutationOfTeams_t *desired, permutationOfTeams_t *inclusive, permutationOfTeams_t *semiDesired, permutationOfTeams_t *firstChoice, uint64_t *numPermutations) {
	double numPlayers = 0.0f;
	for (int j = 0; j < MAX_CLIENTS; j++) {
		sortedClient_t *cl = set->clients + j;
		if (!cl->accountName[0])
			continue;
		numPlayers += 1.0;
	}

	// scale down the number of iterations if we have more people so that we don't have insanely long lag spikes (based on testing)
	int numIterationsToDo;
	if (g_vote_teamgen_iterate.integer) {
		if (g_vote_teamgen_enableInclusive.integer) { // we can afford reduce the number of iterations (causes more lag per iteration anyway)
			switch ((int)numPlayers) {
			/*case 8: */case 9: case 10:	numIterationsToDo = 32;	break;
			case 11:					numIterationsToDo = 16;	break;
			case 12:					numIterationsToDo = 6;	break;
			case 13:					numIterationsToDo = 4;	break;
			case 14:					numIterationsToDo = 3;	break;
			case 15:					numIterationsToDo = 2;	break;
			default:						numIterationsToDo = 1;	break;
			}
		}
		else {
			switch ((int)numPlayers) {
			/*case 8: */case 9: case 10: case 11:	numIterationsToDo = 32;	break;
			case 12:							numIterationsToDo = 16;	break;
			case 13: case 14:					numIterationsToDo = 8;	break;
			case 15:							numIterationsToDo = 6;	break;
			case 16:							numIterationsToDo = 4;	break;
			case 17:							numIterationsToDo = 2;	break;
			default:								numIterationsToDo = 1;	break;
			}
		}
	}
	else {
		numIterationsToDo = 1;
	}

	if (numIterationsToDo > MAX_TEAMGEN_ITERATIONS) {
		assert(qfalse); // idiot
		numIterationsToDo = MAX_TEAMGEN_ITERATIONS;
	}

	unsigned int originalSeed = teamGenSeed;
	static permutationOfTeams_t permutations[MAX_TEAMGEN_ITERATIONS][NUM_TEAMGENERATORTYPES];
	memset(&permutations, 0, sizeof(permutations));
	double sumOfSquares[MAX_TEAMGEN_ITERATIONS] = { 999999 };
	int numPermutationsPerIteration[MAX_TEAMGEN_ITERATIONS] = { 0 };
	double weight[MAX_TEAMGEN_ITERATIONS] = { 0 };

	TeamGen_Initialize();

	// refresh ratings from db
	G_DBGetPlayerRatings();

	// run teamgen a bunch of times
	int numEvaluated = 0;
	for (int i = 0; i < MAX_TEAMGEN_ITERATIONS && i < numIterationsToDo; i++) {
		// deterministically get a new seed for this set of permutations
		teamGenSeed = originalSeed + (i * 29);

		// generate them according to this seed
		GenerateTeams(set, &permutations[i][0], &permutations[i][1], &permutations[i][2], &permutations[i][3], &permutations[i][4], &permutations[i][5], &permutations[i][6], numPermutations, qtrue);

		// calculate the ideal number of slots per player
		double numValidPermutations = 0.0;
		for (int j = 0; j < NUM_TEAMGENERATORTYPES; j++) { if (permutations[i][j].valid) { numValidPermutations += 1.0f; } };

		double numSlots = (numValidPermutations * 8.0);
		double goal = numSlots / numPlayers;

		numPermutationsPerIteration[i] = (int)numValidPermutations;

#ifdef DEBUG_GENERATETEAMS
		Com_Printf("Iteration %02d has ", i);
#endif

		// calculate the sum of squares for this set of permutations (lower number == more fair distribution)
		double sum = 0.0;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			sortedClient_t *cl = set->clients + j;
			if (!cl->accountName[0])
				continue;
			double numPermutationsThisPlayer = (double)NumPermutationsOfPlayer(cl->accountId, &permutations[i][0], &permutations[i][1], &permutations[i][2], &permutations[i][3], &permutations[i][4], &permutations[i][5]);
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("/%s %d/", cl->accountName, (int)numPermutationsThisPlayer); // print the number of permutations each player is part of
#endif
			double addMe = (numPermutationsThisPlayer - goal) * (numPermutationsThisPlayer - goal);
			sum += addMe;

			if (g_vote_teamgen_sumOfSquaresTiebreaker.integer) {
				double thisPlayerWeight = WeightOfPlayer(cl->accountId, &permutations[i][0], &permutations[i][1], &permutations[i][2], &permutations[i][3], &permutations[i][4], &permutations[i][5]);
				weight[i] += thisPlayerWeight;
			}
		}

#ifdef DEBUG_GENERATETEAMS
		Com_Printf(" ; sum of squares %f (%d permutations); weight %g\n", sum, (int)numValidPermutations, weight[i]);
#endif

		sumOfSquares[i] = sum;

		++numEvaluated;

		if (!g_vote_teamgen_sumOfSquaresTiebreaker.integer) {
			// pre-calculated most even possible distribution of player slots
			// (fairest of both 3- and 4-permutation distributions)
			double perfection;
			switch ((int)numPlayers) {
			case 8: perfection = 0;						break;
			case 9: perfection = 2;						break;
			case 10: perfection = 1.6;					break;
			case 11: perfection = 0.909090909090909;	break;
			case 12: perfection = 0;					break;
			case 13: perfection = 1.69230769230769;		break;
			case 14: perfection = 2.85714285714286;		break;
			case 15: perfection = 1.73333333333333;		break;
			case 16: perfection = 0;					break;
			case 17: perfection = 1.76470588235294;		break;
			case 18: perfection = 3.11111111111111;		break;
			case 19: perfection = 3.68421052631579;		break;
			case 20: perfection = 3.2;					break;
			case 21: perfection = 2.57142857142857;		break;
			case 22: perfection = 1.81818181818182;		break;
			case 23: perfection = 0.956521739130435;	break;
			case 24: perfection = 0;					break;
			case 25: perfection = 0.96;					break;
			case 26: perfection = 1.84615384615385;		break;
			case 27: perfection = 2.66666666666667;		break;
			case 28: perfection = 3.42857142857142;		break;
			case 29: perfection = 2.68965517241379;		break;
			case 30: perfection = 1.86666666666667;		break;
			case 31: perfection = 0.967741935483872;	break;
			case 32: perfection = 0;					break;
			default: assert(qfalse); perfection = 0;	break;
			}

			if (sumOfSquares[i] - 0.0001 <= perfection /*&& (int)numValidPermutations == 3*/)
				break; // we got a perfect one; no need to keep iterating
		}
	}

	// pick the set of permutations with the fewest number of permutations (3 == didn't need inclusive pass to begin with), or the lowest sum of squares
	double lowestSumOfSquares = 999999999;
	int lowestNumPermutations = 999999999;
	int bestIndex = -1;
	double weightOfLowestSumOfSquares = 0;
	for (int i = 0; i < MAX_TEAMGEN_ITERATIONS && i < numIterationsToDo && i < numEvaluated; i++) {
		if (sumOfSquares[i] < lowestSumOfSquares - 0.001 || // fairer
			(g_vote_teamgen_sumOfSquaresTiebreaker.integer && fabs(sumOfSquares[i] - lowestSumOfSquares) <= 0.001 && weight[i] > weightOfLowestSumOfSquares + 0.0001) // weightier
			) {
			lowestNumPermutations = numPermutationsPerIteration[i];
			lowestSumOfSquares = sumOfSquares[i];
			bestIndex = i;

			if (g_vote_teamgen_sumOfSquaresTiebreaker.integer) {
				weightOfLowestSumOfSquares = weight[i];
			}
			else {
				if (sumOfSquares[i] <= 0 /*&& numPermutationsPerIteration[i] == 3*/)
					break; // we got a perfect one; no need to keep iterating
			}
		}
	}

	teamGenSeed = originalSeed;

	if (bestIndex != -1) {
#ifdef DEBUG_GENERATETEAMS
		Com_Printf("Best iteration is %d\n", bestIndex);
#endif
		qboolean gotValid = qfalse;
		if (mostPlayed && permutations[bestIndex][0].valid) {
			memcpy(mostPlayed, &permutations[bestIndex][0], sizeof(permutationOfTeams_t));
			avoidedHash_t *avoid = ListAdd(&set->avoidedHashesList, sizeof(avoidedHash_t));
			avoid->hash = mostPlayed->hash;
			gotValid = qtrue;
		}
		if (highestCaliber && permutations[bestIndex][1].valid) {
			memcpy(highestCaliber, &permutations[bestIndex][1], sizeof(permutationOfTeams_t));
			avoidedHash_t *avoid = ListAdd(&set->avoidedHashesList, sizeof(avoidedHash_t));
			avoid->hash = highestCaliber->hash;
			gotValid = qtrue;
		}
		if (fairest && permutations[bestIndex][2].valid) {
			memcpy(fairest, &permutations[bestIndex][2], sizeof(permutationOfTeams_t));
			avoidedHash_t *avoid = ListAdd(&set->avoidedHashesList, sizeof(avoidedHash_t));
			avoid->hash = fairest->hash;
			gotValid = qtrue;
		}
		if (desired && permutations[bestIndex][3].valid) {
			memcpy(desired, &permutations[bestIndex][3], sizeof(permutationOfTeams_t));
			avoidedHash_t *avoid = ListAdd(&set->avoidedHashesList, sizeof(avoidedHash_t));
			avoid->hash = desired->hash;
			gotValid = qtrue;
		}
		if (inclusive && permutations[bestIndex][4].valid) {
			memcpy(inclusive, &permutations[bestIndex][4], sizeof(permutationOfTeams_t));
			avoidedHash_t *avoid = ListAdd(&set->avoidedHashesList, sizeof(avoidedHash_t));
			avoid->hash = inclusive->hash;
			gotValid = qtrue;
		}
		if (semiDesired && permutations[bestIndex][5].valid) {
			memcpy(semiDesired, &permutations[bestIndex][5], sizeof(permutationOfTeams_t));
			avoidedHash_t *avoid = ListAdd(&set->avoidedHashesList, sizeof(avoidedHash_t));
			avoid->hash = semiDesired->hash;
			gotValid = qtrue;
		}
		if (firstChoice && permutations[bestIndex][6].valid) {
			memcpy(firstChoice, &permutations[bestIndex][6], sizeof(permutationOfTeams_t));
			avoidedHash_t *avoid = ListAdd(&set->avoidedHashesList, sizeof(avoidedHash_t));
			avoid->hash = firstChoice->hash;
			gotValid = qtrue;
		}
		return gotValid;
	}

	return qfalse;
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

void GetCurrentPickablePlayers(sortedClient_t *sortedClientsOut, int *numEligibleOut, int *numIngameEligibleOut, qboolean shuffle) {
	// refresh ratings from db
	G_DBGetPlayerRatings();

	// figure out who is eligible
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
			TeamGenerator_PlayerIsBarredFromTeamGenerator(ent) ||
			(IsRacerOrSpectator(ent) && IsSpecName(ent->client->pers.netname)))
			continue;

		if (ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(ent->client->pers.netname, "elo BOT"))
			continue;

		qboolean someoneElseIngameSharesMyAccount = qfalse;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *other = &g_entities[j];
			if (other == ent || !other->inuse || !other->client || other->client->pers.connected == CON_DISCONNECTED)
				continue;

			if (other->client->account && other->client->account->id == ent->client->account->id &&
				!(other->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(other->client->pers.netname, "elo BOT"))) {
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
	int numIngameEligible = 0, numEligible = 0;
	sortedClient_t sortedClients[MAX_CLIENTS];
	memset(sortedClients, 0, sizeof(sortedClients));
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!eligible[i])
			continue;

		gentity_t *ent = &g_entities[i];

		sortedClients[i].clientNum = i;
#ifdef DEBUG_GENERATETEAMS
		Q_strncpyz(sortedClients[i].accountName, Cvar_VariableString(va("z_debug%d", i + 1)), sizeof(sortedClients[i].accountName));
		account_t acc;
		G_DBGetAccountByName(Cvar_VariableString(va("z_debug%d", i + 1)), &acc);
		sortedClients[i].accountId = acc.id;
		sortedClients[i].team = TEAM_SPECTATOR;
		memcpy(&sortedClients[i].posPrefs, &acc.validPref, sizeof(positionPreferences_t));
#else
		Q_strncpyz(sortedClients[i].accountName, ent->client->account->name, sizeof(sortedClients[i].accountName));
		memcpy(&sortedClients[i].posPrefs, &ent->client->account->validPref, sizeof(positionPreferences_t));
		sortedClients[i].team = ent->client->sess.sessionTeam;
		sortedClients[i].accountId = ent->client->account->id;
#endif

#if 0
		for (int j = CTFPOSITION_BASE; j <= CTFPOSITION_OFFENSE; j++) {
			if (sortedClients[i].posPrefs.first & (1 << j))
				TeamGen_DebugPrintf("%s has first choice pos %s<br/>", sortedClients[i].accountName, NameForPos(j));
			if (sortedClients[i].posPrefs.second & (1 << j))
				TeamGen_DebugPrintf("%s has second choice pos %s<br/>", sortedClients[i].accountName, NameForPos(j));
			if (sortedClients[i].posPrefs.third & (1 << j))
				TeamGen_DebugPrintf("%s has third choice pos %s<br/>", sortedClients[i].accountName, NameForPos(j));
			if (sortedClients[i].posPrefs.avoid & (1 << j))
				TeamGen_DebugPrintf("%s has avoided pos %s<br/>", sortedClients[i].accountName, NameForPos(j));
	}
#endif

		++numEligible;
		if (sortedClients[i].team == TEAM_RED || sortedClients[i].team == TEAM_BLUE)
			++numIngameEligible;
	}

	if (numIngameEligibleOut)
		*numIngameEligibleOut = numIngameEligible;
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

	int numGotten = 0;
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

		Q_strcat(buf, sizeof(buf), va("%s%s%s", (numGotten ? ", " : ""), (numGotten && isLast ? "and " : ""), cl->accountName));
		++numGotten;
	}

	Q_strcat(buf, sizeof(buf), va(" (%d players)", numGotten));
	Q_StripColor(buf);

	return buf;
}

// since we may want to respond to the entered chat command with a chat message, we put the server's responses into a queue
// the queue gets everything in it printed after we have printed whatever chat command the player entered 
void TeamGenerator_QueueServerMessageInChat(int clientNum, const char *msg) {
	if (!VALIDSTRING(msg))
		return;

	queuedServerMessage_t *add = ListAdd(&level.queuedServerMessagesList, sizeof(queuedServerMessage_t));
	add->clientNum = clientNum;
	add->text = strdup(msg);
	add->inConsole = qfalse;
	add->serverFrameNum = level.framenum;
}

void TeamGenerator_QueueChatMessage(int fromClientNum, int toClientNum, const char *msg, int when) {
	if (!VALIDSTRING(msg) || fromClientNum < 0 || fromClientNum >= MAX_CLIENTS || toClientNum < 0 || toClientNum >= MAX_CLIENTS)
		return;

	queuedChatMessage_t *add = ListAdd(&level.queuedChatMessagesList, sizeof(queuedServerMessage_t));
	add->fromClientNum = fromClientNum;
	add->toClientNum = toClientNum;
	add->text = strdup(msg);
	add->when = when;
}

void TeamGenerator_QueueServerMessageInConsole(int clientNum, const char *msg) {
	if (!VALIDSTRING(msg))
		return;

	queuedServerMessage_t *add = ListAdd(&level.queuedServerMessagesList, sizeof(queuedServerMessage_t));
	add->clientNum = clientNum;
	add->text = strdup(msg);
	add->inConsole = qtrue;
	add->serverFrameNum = level.framenum;
}

enum {
	TEAMGENTAG_FAIR = 0,
	TEAMGENTAG_FAIREST,
	TEAMGENTAG_TIEDFORMOSTHC,
	TEAMGENTAG_MOSTHC,
	TEAMGENTAG_HC,
	TEAMGENTAG_HC2,
	TEAMGENTAG_HC3,
	TEAMGENTAG_STACK,
	TEAMGENTAG_STACK2,
	TEAMGENTAG_STACK3,
	TEAMGENTAG_6FIRSTCHOICEPOS,
	TEAMGENTAG_7FIRSTCHOICEPOS,
	TEAMGENTAG_8FIRSTCHOICEPOS,
	TEAMGENTAG_NOAVOIDS,
	NUM_TEAMGENTAGS
};

// clientNum -1 for everyone
void PrintTeamsProposalsInConsole(pugProposal_t *set, int clientNum) {
	assert(set);
	char formattedNumber[64] = { 0 };
	FormatNumberToStringWithCommas(set->numValidPermutationsChecked, formattedNumber, sizeof(formattedNumber));
	TeamGenerator_QueueServerMessageInConsole(clientNum, va("Team generator results for %s\n(%s valid permutations evaluated):\n", set->namesStr, formattedNumber));

	int lowestIDiff = 999999;
	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		permutationOfTeams_t *thisPermutation;
		switch (i) {
		case TEAMGENERATORTYPE_MOSTPLAYED: thisPermutation = &set->suggested; break;
		case TEAMGENERATORTYPE_HIGHESTRATING: thisPermutation = &set->highestCaliber; break;
		case TEAMGENERATORTYPE_FAIREST: thisPermutation = &set->fairest; break;
		case TEAMGENERATORTYPE_INCLUSIVE: thisPermutation = &set->inclusive; break;
		case TEAMGENERATORTYPE_DESIREDPOS: thisPermutation = &set->desired; break;
		case TEAMGENERATORTYPE_SEMIDESIREDPOS: thisPermutation = &set->semiDesired; break;
		case TEAMGENERATORTYPE_FIRSTCHOICE: thisPermutation = &set->firstChoice; break;
		}
		if (thisPermutation->valid && thisPermutation->iDiff < lowestIDiff)
			lowestIDiff = thisPermutation->iDiff;
	}

	int iHighestCombinedStrength = 0;
	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		permutationOfTeams_t *thisPermutation;
		switch (i) {
		case TEAMGENERATORTYPE_MOSTPLAYED: thisPermutation = &set->suggested; break;
		case TEAMGENERATORTYPE_HIGHESTRATING: thisPermutation = &set->highestCaliber; break;
		case TEAMGENERATORTYPE_FAIREST: thisPermutation = &set->fairest; break;
		case TEAMGENERATORTYPE_INCLUSIVE: thisPermutation = &set->inclusive; break;
		case TEAMGENERATORTYPE_DESIREDPOS: thisPermutation = &set->desired; break;
		case TEAMGENERATORTYPE_SEMIDESIREDPOS: thisPermutation = &set->semiDesired; break;
		case TEAMGENERATORTYPE_FIRSTCHOICE: thisPermutation = &set->firstChoice; break;
		}
		if (!thisPermutation->valid)
			continue;

		double combinedStrength = thisPermutation->teams[0].rawStrength + thisPermutation->teams[1].rawStrength;
		int thisCombinedStrength = (int)round(combinedStrength * 10000);
		if (thisCombinedStrength > iHighestCombinedStrength)
			iHighestCombinedStrength = thisCombinedStrength;
	}

	int numTiedForHighestCombinedStrength = 0;
	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		permutationOfTeams_t *thisPermutation;
		switch (i) {
		case TEAMGENERATORTYPE_MOSTPLAYED: thisPermutation = &set->suggested; break;
		case TEAMGENERATORTYPE_HIGHESTRATING: thisPermutation = &set->highestCaliber; break;
		case TEAMGENERATORTYPE_FAIREST: thisPermutation = &set->fairest; break;
		case TEAMGENERATORTYPE_INCLUSIVE: thisPermutation = &set->inclusive; break;
		case TEAMGENERATORTYPE_DESIREDPOS: thisPermutation = &set->desired; break;
		case TEAMGENERATORTYPE_SEMIDESIREDPOS: thisPermutation = &set->semiDesired; break;
		case TEAMGENERATORTYPE_FIRSTCHOICE: thisPermutation = &set->firstChoice; break;
		}
		if (!thisPermutation->valid)
			continue;

		double combinedStrength = thisPermutation->teams[0].rawStrength + thisPermutation->teams[1].rawStrength;
		int thisCombinedStrength = (int)round(combinedStrength * 10000);
		if (thisCombinedStrength == iHighestCombinedStrength)
			++numTiedForHighestCombinedStrength;
	}

	qboolean printedFairest = qfalse;
	int numPrinted = 0;
	char lastLetter = 'a';
	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		permutationOfTeams_t *thisPermutation;
		switch (i) {
		case TEAMGENERATORTYPE_MOSTPLAYED: thisPermutation = &set->suggested; break;
		case TEAMGENERATORTYPE_HIGHESTRATING: thisPermutation = &set->highestCaliber; break;
		case TEAMGENERATORTYPE_FAIREST: thisPermutation = &set->fairest; break;
		case TEAMGENERATORTYPE_INCLUSIVE: thisPermutation = &set->inclusive; break;
		case TEAMGENERATORTYPE_DESIREDPOS: thisPermutation = &set->desired; break;
		case TEAMGENERATORTYPE_SEMIDESIREDPOS: thisPermutation = &set->semiDesired; break;
		case TEAMGENERATORTYPE_FIRSTCHOICE: thisPermutation = &set->firstChoice; break;
		}
		if (!thisPermutation->valid)
			continue;

		double thisCombinedStrength = thisPermutation->teams[0].rawStrength + thisPermutation->teams[1].rawStrength;
		int iThisCombinedStrength = (int)round(thisCombinedStrength * 10000);

		double thisAverageStrength = (thisCombinedStrength / 8.0);
		double alpha, beta;
		if (thisAverageStrength - thisPermutation->lowestPlayerRating >= (PLAYERRATING_DECIMAL_INCREMENT * 5) - 0.0001) {
			alpha = 70.0;
			beta = 30.0;
		}
		else {
			alpha = 85.0;
			beta = 15.0;
		}
		int rawCaliber = (int)round(((thisAverageStrength)*alpha) + (thisPermutation->lowestPlayerRating * beta) + 15.0);
		int caliber = 10 - ((100 - rawCaliber) / 5);

		int tags = 0;

		if (iThisCombinedStrength == iHighestCombinedStrength) {
			if (numTiedForHighestCombinedStrength > 1)
				tags |= (1 << TEAMGENTAG_TIEDFORMOSTHC);
			else
				tags |= (1 << TEAMGENTAG_MOSTHC);
		}

		if (thisPermutation->lowestPlayerRating >= PlayerTierToRating(PLAYERRATING_MID_A) - 0.0001)
			tags |= (1 << TEAMGENTAG_HC3);
		else if (thisPermutation->lowestPlayerRating >= PlayerTierToRating(PLAYERRATING_LOW_A) - 0.0001)
			tags |= (1 << TEAMGENTAG_HC2);
		else if (thisPermutation->lowestPlayerRating >= PlayerTierToRating(PLAYERRATING_MID_B) - 0.0001)
			tags |= (1 << TEAMGENTAG_HC);

		if (!thisPermutation->iDiff)
			tags |= (1 << TEAMGENTAG_FAIR);
		else if (thisPermutation->iDiff == lowestIDiff)
			tags |= (1 << TEAMGENTAG_FAIREST);

		if (!thisPermutation->numOnAvoidedPos)
			tags |= (1 << TEAMGENTAG_NOAVOIDS);

		const int numFirstChoice = thisPermutation->numOnPreferredPos / PREFERREDPOS_VALUE_FIRSTCHOICE;
		if (numFirstChoice == 8)
			tags |= (1 << TEAMGENTAG_8FIRSTCHOICEPOS);
		else if (numFirstChoice == 7)
			tags |= (1 << TEAMGENTAG_7FIRSTCHOICEPOS);
		else if (numFirstChoice == 6)
			tags |= (1 << TEAMGENTAG_6FIRSTCHOICEPOS);

		if (thisPermutation->teams[0].relativeStrength >= 0.529f - 0.0001f || thisPermutation->teams[1].relativeStrength >= 0.529f - 0.0001f)
			tags |= (1 << TEAMGENTAG_STACK3);
		else if (thisPermutation->teams[0].relativeStrength >= 0.519f - 0.0001f || thisPermutation->teams[1].relativeStrength >= 0.519f - 0.0001f)
			tags |= (1 << TEAMGENTAG_STACK2);
		else if (thisPermutation->teams[0].relativeStrength >= 0.509f - 0.0001f || thisPermutation->teams[1].relativeStrength >= 0.509f - 0.0001f)
			tags |= (1 << TEAMGENTAG_STACK);

#ifdef PRINT_UNDERDOG_TIEBREAKER
		int underdogTeam = 0;
		if (g_vote_underdogTeamMapVoteTiebreakerThreshold.string[0] && (double)(g_vote_underdogTeamMapVoteTiebreakerThreshold.value) > 0.5) {
			if (thisPermutation->teams[0].relativeStrength >= (double)(g_vote_underdogTeamMapVoteTiebreakerThreshold.value) - 0.0001)
				underdogTeam = TEAM_BLUE;
			else if (thisPermutation->teams[1].relativeStrength >= (double)(g_vote_underdogTeamMapVoteTiebreakerThreshold.value) - 0.0001)
				underdogTeam = TEAM_RED; // shouldn't really happen
		}
#endif

		char letter;
		if (i == TEAMGENERATORTYPE_MOSTPLAYED) {
			letter = set->suggestedLetter = lastLetter++;
		}
		else if (i == TEAMGENERATORTYPE_HIGHESTRATING) {
			if (thisPermutation->hash == set->suggested.hash && set->suggested.valid) {
				thisPermutation->valid = qfalse;
				continue;
			}
			letter = set->highestCaliberLetter = lastLetter++;
		}
		else if (i == TEAMGENERATORTYPE_FAIREST) {
			if ((thisPermutation->hash == set->suggested.hash && set->suggested.valid) || (thisPermutation->hash == set->highestCaliber.hash && set->highestCaliber.valid)) {
				thisPermutation->valid = qfalse;
				continue;
			}
			letter = set->fairestLetter = lastLetter++;
		}
		else if (i == TEAMGENERATORTYPE_INCLUSIVE) {
			if ((thisPermutation->hash == set->suggested.hash && set->suggested.valid) || (thisPermutation->hash == set->highestCaliber.hash && set->highestCaliber.valid) ||
				(thisPermutation->hash == set->fairest.hash && set->fairest.valid)) {
				thisPermutation->valid = qfalse;
				continue;
			}
			letter = set->inclusiveLetter = lastLetter++;
		}
		else if (i == TEAMGENERATORTYPE_DESIREDPOS) {
			if ((thisPermutation->hash == set->suggested.hash && set->suggested.valid) || (thisPermutation->hash == set->highestCaliber.hash && set->highestCaliber.valid) ||
				(thisPermutation->hash == set->fairest.hash && set->fairest.valid)) {
				thisPermutation->valid = qfalse;
				continue;
			}
			letter = set->desiredLetter = lastLetter++;
		}
		else if (i == TEAMGENERATORTYPE_SEMIDESIREDPOS) {
			if ((thisPermutation->hash == set->suggested.hash && set->suggested.valid) || (thisPermutation->hash == set->highestCaliber.hash && set->highestCaliber.valid) ||
				(thisPermutation->hash == set->fairest.hash && set->fairest.valid)) {
				thisPermutation->valid = qfalse;
				continue;
			}
			letter = set->semiDesiredLetter = lastLetter++;
		}
		else if (i == TEAMGENERATORTYPE_FIRSTCHOICE) {
			if ((thisPermutation->hash == set->suggested.hash && set->suggested.valid) || (thisPermutation->hash == set->highestCaliber.hash && set->highestCaliber.valid) ||
				(thisPermutation->hash == set->fairest.hash && set->fairest.valid)) {
				thisPermutation->valid = qfalse;
				continue;
			}
			letter = set->firstChoiceLetter = lastLetter++;
		}

		char suggestionTypeStr[MAX_STRING_CHARS] = { 0 };

		if (tags & (1 << TEAMGENTAG_STACK3))
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^1[ULTRA OMEGA STACK!!!]" : va("%s ^1[ULTRA OMEGA STACK!!!]", suggestionTypeStr));
		else if (tags & (1 << TEAMGENTAG_STACK2))
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^1[MEGA STACK!!]" : va("%s ^1[MEGA STACK!!]", suggestionTypeStr));
		else if (tags & (1 << TEAMGENTAG_STACK))
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^8[possible stack!]" : va("%s ^8[possible stack!]", suggestionTypeStr));

#ifdef PRINT_UNDERDOG_TIEBREAKER
		if (underdogTeam == TEAM_RED)
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^1[Map vote tie goes to red]" : va("%s ^1[Map vote tie goes to red]", suggestionTypeStr));
		else if (underdogTeam == TEAM_BLUE)
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^4[Map vote tie goes to blue]" : va("%s ^4[Map vote tie goes to blue]", suggestionTypeStr));
#endif

		if (tags & (1 << TEAMGENTAG_FAIR))
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^2[even]" : va("%s ^2[even]", suggestionTypeStr));
		else if (tags & (1 << TEAMGENTAG_FAIREST))
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^2[most even]" : va("%s ^2[most even]", suggestionTypeStr));


		if (g_vote_teamgen_displayCaliber.integer) {
#ifdef DEBUG_GENERATETEAMS
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? va("^6[c%d - %d raw, %g avg, %g lowest, %g alpha, %g beta]", caliber, rawCaliber, thisAverageStrength, thisPermutation->lowestPlayerRating, alpha, beta) : va("%s ^6[c%d - %d raw, %g avg, %g lowest, %g alpha, %g beta]", suggestionTypeStr, caliber, rawCaliber, thisAverageStrength, thisPermutation->lowestPlayerRating, alpha, beta));
#else
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? va("^6[c%d]", caliber) : va("%s ^6[c%d]", suggestionTypeStr, caliber));
#endif
		}
		else {
			if (tags & (1 << TEAMGENTAG_TIEDFORMOSTHC))
				Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^3[tied for most HC]" : va("%s ^3[tied for most HC]", suggestionTypeStr));
			else if (tags & (1 << TEAMGENTAG_MOSTHC))
				Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^3[most HC]" : va("%s ^3[most HC]", suggestionTypeStr));

#if 0
			if (!rerollNum && i == TEAMGENERATORTYPE_HIGHESTRATING && (tags & (1 << TEAMGENTAG_MOSTHC) || tags & (1 << TEAMGENTAG_TIEDFORMOSTHC)))
				Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^5[spec/capt]" : va("%s ^5[spec/capt]", suggestionTypeStr));
#endif

			if (tags & (1 << TEAMGENTAG_HC3))
				Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^6[ULTRA OMEGA HC!!!]" : va("%s ^6[ULTRA OMEGA HC!!!]", suggestionTypeStr));
			else if (tags & (1 << TEAMGENTAG_HC2))
				Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^6[MEGA HC!!]" : va("%s ^6[MEGA HC!!]", suggestionTypeStr));
			else if (tags & (1 << TEAMGENTAG_HC))
				Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^6[HC!]" : va("%s ^6[HC!]", suggestionTypeStr));
		}

		if (tags & (1 << TEAMGENTAG_NOAVOIDS))
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^7[no avoids]" : va("%s ^7[no avoids]", suggestionTypeStr));

		if (tags & (1 << TEAMGENTAG_8FIRSTCHOICEPOS))
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^7[8 1st choices]" : va("%s ^7[8 1st choices]", suggestionTypeStr));
		else if (tags & (1 << TEAMGENTAG_7FIRSTCHOICEPOS))
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^7[7 1st choices]" : va("%s ^7[7 1st choices]", suggestionTypeStr));
		else if (tags & (1 << TEAMGENTAG_6FIRSTCHOICEPOS))
			Com_sprintf(suggestionTypeStr, sizeof(suggestionTypeStr), !suggestionTypeStr[0] ? "^7[6 1st choices]" : va("%s ^7[6 1st choices]", suggestionTypeStr));

		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (!ent->inuse || !ent->client)
				continue;
			if (clientNum != -1 && i != clientNum)
				continue;
			int accId = ent->client->account ? ent->client->account->id : -1;
#ifdef DEBUG_GENERATETEAMS
			TeamGenerator_QueueServerMessageInConsole(i, va("%sChoice ^5%c%c^7:%s%s^7\nTotal strength: %.2f\n^1Red team:^7 %.2f'/. relative strength, %.2f raw strength\n    ^5Base: %s%s\n    ^6Chase: %s%s\n    ^2Offense: %s%s^7, %s%s^7\n^4Blue team:^7 %.2f'/. relative strength, %.2f raw strength\n    ^5Base: %s%s\n    ^6Chase: %s%s\n    ^2Offense: %s%s^7, %s%s^7\n",
				numPrinted ? "\n" : "",
				TEAMGEN_CHAT_COMMAND_CHARACTER,
				letter,
				suggestionTypeStr[0] ? "   " : "",
				suggestionTypeStr,
				thisPermutation->teams[0].rawStrength + thisPermutation->teams[1].rawStrength,
				thisPermutation->teams[0].relativeStrength * 100.0,
				thisPermutation->teams[0].rawStrength,
				thisPermutation->teams[0].baseId == accId ? "^3" : "^7",
				thisPermutation->teams[0].baseName,
				thisPermutation->teams[0].chaseId == accId ? "^3" : "^7",
				thisPermutation->teams[0].chaseName,
				thisPermutation->teams[0].offenseId1 == accId ? "^3" : "^7",
				thisPermutation->teams[0].offense1Name,
				thisPermutation->teams[0].offenseId2 == accId ? "^3" : "^7",
				thisPermutation->teams[0].offense2Name,
				thisPermutation->teams[1].relativeStrength * 100.0,
				thisPermutation->teams[1].rawStrength,
				thisPermutation->teams[1].baseId == accId ? "^3" : "^7",
				thisPermutation->teams[1].baseName,
				thisPermutation->teams[1].chaseId == accId ? "^3" : "^7",
				thisPermutation->teams[1].chaseName,
				thisPermutation->teams[1].offenseId1 == accId ? "^3" : "^7",
				thisPermutation->teams[1].offense1Name,
				thisPermutation->teams[1].offenseId2 == accId ? "^3" : "^7",
				thisPermutation->teams[1].offense2Name));
#else
			TeamGenerator_QueueServerMessageInConsole(i, va("%sChoice ^5%c%c^7:%s%s^7\n^1Red team:^7 %.2f'/. relative strength\n    ^5Base: %s%s\n    ^6Chase: %s%s\n    ^2Offense: %s%s^7, %s%s^7\n^4Blue team:^7 %.2f'/. relative strength\n    ^5Base: %s%s\n    ^6Chase: %s%s\n    ^2Offense: %s%s^7, %s%s^7\n",
				numPrinted ? "\n" : "",
				TEAMGEN_CHAT_COMMAND_CHARACTER,
				letter,
				suggestionTypeStr[0] ? "   " : "",
				suggestionTypeStr,
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
				thisPermutation->teams[1].offense2Name));
#endif
		}

		if (clientNum == -1) {
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("%sChoice ^5%c%c^7:%s%s^7\nTotal strength: %.2f\n^1Red team:^7 %.2f'/. relative strength, %.2f raw strength\n    ^5Base: ^7%s\n    ^6Chase: ^7%s\n    ^2Offense: ^7%s^7, %s^7\n^4Blue team:^7 %.2f'/. relative strength, %.2f raw strength\n    ^5Base: ^7%s\n    ^6Chase: ^7%s\n    ^2Offense: ^7%s^7, %s^7\n",
				numPrinted ? "\n" : "",
				TEAMGEN_CHAT_COMMAND_CHARACTER,
				letter,
				suggestionTypeStr[0] ? "   " : "",
				suggestionTypeStr,
				thisPermutation->teams[0].rawStrength + thisPermutation->teams[1].rawStrength,
				thisPermutation->teams[0].relativeStrength * 100.0,
				thisPermutation->teams[0].rawStrength,
				thisPermutation->teams[0].baseName,
				thisPermutation->teams[0].chaseName,
				thisPermutation->teams[0].offense1Name,
				thisPermutation->teams[0].offense2Name,
				thisPermutation->teams[1].relativeStrength * 100.0,
				thisPermutation->teams[1].rawStrength,
				thisPermutation->teams[1].baseName,
				thisPermutation->teams[1].chaseName,
				thisPermutation->teams[1].offense1Name,
				thisPermutation->teams[1].offense2Name);
#else
			Com_Printf("%sChoice ^5%c%c^7:%s%s^7\n^1Red team:^7 %.2f'/. relative strength\n    ^5Base: ^7%s\n    ^6Chase: ^7%s\n    ^2Offense: ^7%s^7, %s^7\n^4Blue team:^7 %.2f'/. relative strength\n    ^5Base: ^7%s\n    ^6Chase: ^7%s\n    ^2Offense: ^7%s^7, %s^7\n",
				numPrinted ? "\n" : "",
				TEAMGEN_CHAT_COMMAND_CHARACTER,
				letter,
				suggestionTypeStr[0] ? "   " : "",
				suggestionTypeStr,
				thisPermutation->teams[0].relativeStrength * 100.0,
				thisPermutation->teams[0].baseName,
				thisPermutation->teams[0].chaseName,
				thisPermutation->teams[0].offense1Name,
				thisPermutation->teams[0].offense2Name,
				thisPermutation->teams[1].relativeStrength * 100.0,
				thisPermutation->teams[1].baseName,
				thisPermutation->teams[1].chaseName,
				thisPermutation->teams[1].offense1Name,
				thisPermutation->teams[1].offense2Name);
#endif
		}

		++numPrinted;
	}

	if (numPrinted == 4) {
		TeamGenerator_QueueServerMessageInConsole(clientNum,
			va("Vote to approve a teams proposal by entering e.g. ^5%ca^7, ^5%cb^7, ^5%cc^7, or ^5%cd^7\n"
				"You can approve of multiple teams proposals simultaneously by entering e.g. ^5%cab^7\n",
				TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER));
	}
	else if (numPrinted == 3) {
		TeamGenerator_QueueServerMessageInConsole(clientNum,
			va("Vote to approve a teams proposal by entering e.g. ^5%ca^7, ^5%cb^7, or ^5%cc^7\n"
			"You can approve of multiple teams proposals simultaneously by entering e.g. ^5%cab^7\n",
			TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER));
	}
	else if (numPrinted == 2) {
		TeamGenerator_QueueServerMessageInConsole(clientNum,
			va("Vote to approve a teams proposal by entering ^5%ca^7 or ^5%cb^7\n"
			"You can approve of both teams proposals simultaneously by entering ^5%cab^7\n",
			TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER));
	}
	else if (numPrinted == 1) {
		TeamGenerator_QueueServerMessageInConsole(clientNum,
			va("Vote to approve the teams proposal by entering ^5%ca^7\n", TEAMGEN_CHAT_COMMAND_CHARACTER));
	}

	TeamGenerator_QueueServerMessageInConsole(clientNum,
		va("You can vote to reroll the teams proposals by entering ^5%creroll^7\n"
		"You can vote to cancel the pug proposal by entering ^5%ccancel^7\n",
		TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER));

	if (clientNum == -1)
		level.teamPermutationsShownTime = trap_Milliseconds();
}

static void ActivatePugProposal(pugProposal_t *set, qboolean forcedByServer) {
	assert(set);

	// first, check if we have a banned permutation
	if (g_vote_teamgen_banLastPlayedPermutation.integer) {
		if (g_bannedPermutationTime.string[0]) { // if we have a banned permutation...
			if (getGlobalTime() - atoi(g_bannedPermutationTime.string) < 7200) { // ...that was banned within the last two hours...
				if (g_bannedPermutationHash.string[0]) { // ...use it
					if (d_debugBanPermutation.integer) { Com_Printf("d_debugBanPermutation: ActivatePugProposal: Adding g_bannedPermutationHash %s to avoided hashes list.\n", g_bannedPermutationHash.string); }
					avoidedHash_t *bannedHash = ListAdd(&set->avoidedHashesList, sizeof(avoidedHash_t));
					bannedHash->hash = strtoul(g_bannedPermutationHash.string, NULL, 10);
					// we don't unban here
				}
				else {
					if (d_debugBanPermutation.integer) { Com_Printf("d_debugBanPermutation: ActivatePugProposal: g_bannedPermutationTime set but g_bannedPermutationHash is not set?\n"); }
					trap_Cvar_Set("g_bannedPermutationTime", "");
				}
			}
			else {
				if (d_debugBanPermutation.integer) { Com_Printf("d_debugBanPermutation: ActivatePugProposal: g_bannedPermutationTime and g_bannedPermutationHash both set, but time has expired.\n"); }
				trap_Cvar_Set("g_bannedPermutationTime", "");
				trap_Cvar_Set("g_bannedPermutationHash", "");
			}
		}
		else {
			if (d_debugBanPermutation.integer) { Com_Printf("d_debugBanPermutation: ActivatePugProposal: g_bannedPermutationTime is not set.\n"); }
			trap_Cvar_Set("g_bannedPermutationHash", "");
		}
	}

	// then go ahead and generate teams
	rerollNum = 0;
	if (GenerateTeamsIteratively(set, &set->suggested, &set->highestCaliber, &set->fairest, &set->desired, &set->inclusive, &set->semiDesired, &set->firstChoice, &set->numValidPermutationsChecked)) {
		TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d %s (%s). Check console for teams proposals.", set->num, forcedByServer ? "force passed by server" : "passed", set->namesStr));
		set->passed = qtrue;
		level.activePugProposal = set;

		// remove any other passed proposals - this fixes the following edge case:
		// pass a proposal with 8 guys -> 9th connects and you pass a new one -> 9th guy leaves -> you try to start with the original 8 again and it says this proposal has already passed
		iterator_t iter;
		ListIterate(&level.pugProposalsList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			pugProposal_t *p = IteratorNext(&iter);
			if (p->passed && p != set) {
				ListRemove(&level.pugProposalsList, p);
				ListIterate(&level.pugProposalsList, &iter, qfalse);
			}
		}

		PrintTeamsProposalsInConsole(set, -1);

		if (level.autoStartPending) {
			Com_Printf("The currently pending auto start was cancelled due to team generator changing the teams.\n");
			level.autoStartPending = qfalse;
		}

		// kill any existing map vote if we decide to do a different pug proposal
		if (level.voteTime && (!Q_stricmp(level.voteString, "mapvote") || level.multiVoting)) {
			PrintIngame(-1, "Map vote automatically stopped due to pug proposal passing.\n");
			level.voteAutoPassOnExpiration = qfalse;
			level.multiVoting = qfalse;
			level.runoffSurvivors = level.runoffLosers = level.successfulRerollVoters = 0llu;
			level.inRunoff = qfalse;
			level.multiVoteChoices = 0;
			level.multiVoteHasWildcard = qfalse;
			level.multivoteWildcardMapFileName[0] = '\0';
			level.mapsThatCanBeVotedBits = 0;
			level.multiVoteTimeExtensions = 0;
			memset(level.multiVotes, 0, sizeof(level.multiVotes));
			memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
			memset(&level.multiVoteMapShortNames, 0, sizeof(level.multiVoteMapShortNames));
			memset(&level.multiVoteMapFileNames, 0, sizeof(level.multiVoteMapFileNames));
			level.voteStartTime = 0;
			level.voteTime = 0;
			trap_SetConfigstring(CS_VOTE_TIME, "");
		}
	}
	else {
		TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d %s (%s). However, unable to generate teams; pug proposal %d terminated.", set->num, forcedByServer ? "force passed by server" : "passed", set->namesStr, set->num));
		level.activePugProposal = NULL;
		ListClear(&set->avoidedHashesList);
		ListRemove(&level.pugProposalsList, set);
	}
}

#define SCOREBOARD_POSITION_SCORE_BASE		(8000)
#define SCOREBOARD_POSITION_SCORE_CHASE		(4000)
#define SCOREBOARD_POSITION_SCORE_OFFENSE1	(2000)
#define SCOREBOARD_POSITION_SCORE_OFFENSE2	(1000)

static void StartAutomaticTeamGenMapVote(void) {
	if (g_vote_teamgen_autoMapVoteSeconds.integer <= 0)
		return;

	// try not to spam map votes if we voted somewhere and then changed teams without actually pugging there
	if (g_lastMapVotedMap.string[0] && g_lastMapVotedTime.string[0] &&
		!Q_stricmp(g_lastMapVotedMap.string, level.mapname) && Q_isanumber(g_lastMapVotedTime.string) &&
		(int)time(NULL) - g_lastMapVotedTime.integer <= 1200) {
		return;
	}

	// just use tierlist system only since the pool system is dead at this point
	if (!g_vote_tierlist.integer) {
		Com_Printf("g_vote_tierlist is disabled; unable to automatically start map vote.\n");
		return;
	}

	const int totalMapsToChoose = Com_Clampi(2, MAX_MULTIVOTE_MAPS, g_vote_tierlist_totalMaps.integer);
	if ((g_vote_tierlist_s_max.integer + g_vote_tierlist_a_max.integer + g_vote_tierlist_b_max.integer +
		g_vote_tierlist_c_max.integer + g_vote_tierlist_f_max.integer < totalMapsToChoose) ||
		(g_vote_tierlist_s_min.integer + g_vote_tierlist_a_min.integer + g_vote_tierlist_b_min.integer +
			g_vote_tierlist_c_min.integer + g_vote_tierlist_f_min.integer > totalMapsToChoose)) {
		// we could just decrease the max, but let's just shame the idiots instead
		Com_Printf("Error: min/max conflict with g_vote_tierlist_totalMaps! Unable to use tierlist voting.\n");
		return;
	}

	if (g_vote_teamgen_autoMapVoteNonAfkAutoVoteYesSeconds.integer > 0)
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "New Map Vote (%d Maps) ^2(auto vote)^7", totalMapsToChoose);
	else
		Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "New Map Vote (%d Maps) ^2(auto pass)^7", totalMapsToChoose);
	Com_sprintf(level.voteString, sizeof(level.voteString), "mapvote");

	level.voteStartTime = level.time;
	int voteDurationSeconds = Com_Clampi(15, 300, g_vote_teamgen_autoMapVoteSeconds.integer);
	level.voteTime = level.time + ((voteDurationSeconds - 30) * 1000);
	level.voteAutoPassOnExpiration = qtrue;
	level.voteYes = 0;
	level.voteNo = 0;
	//level.lastVotingClient
	level.multiVoting = qfalse;
	level.runoffSurvivors = level.runoffLosers = level.successfulRerollVoters = 0llu;
	level.inRunoff = qfalse;
	level.mapsThatCanBeVotedBits = 0;
	level.inRunoff = qfalse;
	level.multiVoteChoices = 0;
	level.multiVoteTimeExtensions = 0;
	level.multiVoteHasWildcard = qfalse;
	level.multivoteWildcardMapFileName[0] = '\0';
	memset(level.multiVotes, 0, sizeof(level.multiVotes));
	memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
	memset(&level.multiVoteMapShortNames, 0, sizeof(level.multiVoteMapShortNames));
	memset(&level.multiVoteMapFileNames, 0, sizeof(level.multiVoteMapFileNames));

	fixVoters(qfalse, 0);

	trap_SetConfigstring(CS_VOTE_TIME, va("%i", level.voteTime));
	trap_SetConfigstring(CS_VOTE_STRING, level.voteDisplayString);
	trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
	trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
}

static int SimpleHashSort(const void *a, const void *b) {
	XXH32_hash_t aa = *((XXH32_hash_t *)a);
	XXH32_hash_t bb = *((XXH32_hash_t *)b);
	if (aa > bb)
		return -1;
	if (bb > aa)
		return 1;
	return 0;
}

typedef struct {
	node_t			node;
	char			name[MAX_ACCOUNTNAME_LEN];
} gotAccount_t;

static qboolean AccountMatches(genericNode_t *node, void *userData) {
	const gotAccount_t *existing = (const gotAccount_t *)node;
	const stats_t *thisGuy = (const stats_t *)userData;

	if (existing && thisGuy && !Q_stricmp(existing->name, thisGuy->accountName))
		return qtrue;

	return qfalse;
}

// returns a hash for a set of four players on one team and four players on another team
// we normalize the order so that order of accounts doesn't matter and team 1/team 2 doesn't matter
static XXH32_hash_t HashPositionlessPermutation(const char *team1Account1, const char *team1Account2, const char *team1Account3, const char *team1Account4,
	const char *team2Account1, const char *team2Account2, const char *team2Account3, const char *team2Account4) {
	XXH32_hash_t hashes[2][4];
	memset(hashes, 0, sizeof(hashes));

	// hash each account name
	hashes[0][0] = XXH32(team1Account1, strlen(team1Account1), 0);
	hashes[0][1] = XXH32(team1Account2, strlen(team1Account2), 0);
	hashes[0][2] = XXH32(team1Account3, strlen(team1Account3), 0);
	hashes[0][3] = XXH32(team1Account4, strlen(team1Account4), 0);
	hashes[1][0] = XXH32(team2Account1, strlen(team2Account1), 0);
	hashes[1][1] = XXH32(team2Account2, strlen(team2Account2), 0);
	hashes[1][2] = XXH32(team2Account3, strlen(team2Account3), 0);
	hashes[1][3] = XXH32(team2Account4, strlen(team2Account4), 0);

	// sort the names within each team
	qsort(hashes[0], 4, sizeof(XXH32_hash_t), SimpleHashSort);
	qsort(hashes[1], 4, sizeof(XXH32_hash_t), SimpleHashSort);

	// hash and sort both entire teams
	XXH32_hash_t teamHashes[2];
	teamHashes[0] = XXH32(hashes[0], sizeof(XXH32_hash_t) * 4, 0);
	teamHashes[1] = XXH32(hashes[1], sizeof(XXH32_hash_t) * 4, 0);
	qsort(teamHashes, 2, sizeof(XXH32_hash_t), SimpleHashSort);

	// finally, hash the sorted pair of teams
	XXH32_hash_t overallHash = XXH32(teamHashes, sizeof(teamHashes), 0);
	if (d_debugBanPermutation.integer) { Com_Printf("d_debugBanPermutation: HashPositionlessPermutation: got %u\n", overallHash); }
	return overallHash;
}

// called when a teamgen permutation (a/b/c/d) is passed.
// we remember the hashes including and excluding positions so that when we complete
// a pug within the next two hours we can compare to see if the list of players in each team
// matches what we voted
static void RememberSelectedPermutation(const permutationOfTeams_t *permutation) {
	assert(permutation);
	XXH32_hash_t hash = HashPositionlessPermutation(permutation->teams[0].baseName, permutation->teams[0].chaseName, permutation->teams[0].offense1Name, permutation->teams[0].offense2Name,
		permutation->teams[1].baseName, permutation->teams[1].chaseName, permutation->teams[1].offense1Name, permutation->teams[1].offense2Name);
	trap_Cvar_Set("g_lastSelectedPositionlessPermutation", va("%u", hash));
	trap_Cvar_Set("g_lastSelectedPermutationHash", va("%u", permutation->hash));
	trap_Cvar_Set("g_lastSelectedPermutationTime", va("%d", getGlobalTime()));
}

// called at the end of a restarted 4v4 match of at least 5 minutes' length
// if the list of players in each team (irrespective of red/blue and positions) is the same as
// the teamgen teams we voted on, then that set of teams/positions will be banned until
// a new set of teams is played, two hours elapse, or the server is restarted
void TeamGenerator_MatchComplete(void) {
	if (!g_lastSelectedPermutationHash.string[0] || !g_lastSelectedPermutationTime.string[0] || !g_lastSelectedPositionlessPermutation.string[0]) {
		if (d_debugBanPermutation.integer) {
			Com_Printf("d_debugBanPermutation: TeamGenerator_MatchComplete: empty cvar(s), so unbanning anything banned and returning (g_lastSelectedPermutationHash %s, g_lastSelectedPermutationTime %s, g_lastSelectedPositionlessPermutation %s)\n",
				g_lastSelectedPermutationHash.string, g_lastSelectedPermutationTime.string, g_lastSelectedPositionlessPermutation.string);
		}

		// clear everything
		trap_Cvar_Set("g_lastSelectedPermutationHash", "");
		trap_Cvar_Set("g_lastSelectedPermutationTime", "");
		trap_Cvar_Set("g_lastSelectedPositionlessPermutation", "");
		trap_Cvar_Set("g_bannedPermutationHash", "");
		trap_Cvar_Set("g_bannedPermutationTime", "");
		return;
	}

	if (getGlobalTime() - atoi(g_lastSelectedPermutationTime.string) >= 7200) {
		if (d_debugBanPermutation.integer) { Com_Printf("d_debugBanPermutation: TeamGenerator_MatchComplete: too long has elapsed since voting teams. Unbanning anything banned and returning.\n"); }

		// clear everything
		trap_Cvar_Set("g_lastSelectedPermutationHash", "");
		trap_Cvar_Set("g_lastSelectedPermutationTime", "");
		trap_Cvar_Set("g_lastSelectedPositionlessPermutation", "");
		trap_Cvar_Set("g_bannedPermutationHash", "");
		trap_Cvar_Set("g_bannedPermutationTime", "");
		return;
	}

	// first, count how many players we have
	list_t redPlayersList = { 0 }, bluePlayersList = { 0 };
	qboolean invalid = qfalse;
	for (int i = 0; i < 2; i++) {
		iterator_t iter;
		ListIterate(!i ? &level.savedStatsList : &level.statsList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			stats_t *found = IteratorNext(&iter);

			if (!StatsValid(found) || found->isBot || (found->lastTeam != TEAM_RED && found->lastTeam != TEAM_BLUE))
				continue;

			if (found->ticksNotPaused * (1000 / g_svfps.integer) < 60000)
				continue; // ignore troll joiners ingame for less than a minute

			list_t *myTeamList, *otherTeamList;

			if (found->lastTeam == TEAM_RED) {
				myTeamList = &redPlayersList;
				otherTeamList = &bluePlayersList;
			}
			else {
				myTeamList = &bluePlayersList;
				otherTeamList = &redPlayersList;
			}

			gotAccount_t *gotPlayer = NULL;
			gotPlayer = ListFind(otherTeamList, AccountMatches, found, NULL);
			if (gotPlayer) { // same player on both teams???
				invalid = qtrue;
				break;
			}

			gotPlayer = ListFind(myTeamList, AccountMatches, found, NULL);
			if (gotPlayer) // already got this guy
				continue;

			// add this guy to the relevant list
			gotAccount_t *add = ListAdd(myTeamList, sizeof(gotAccount_t));
			Q_strncpyz(add->name, found->accountName, sizeof(add->name));
		}

		if (invalid)
			break;
	}

	if (d_debugBanPermutation.integer) { Com_Printf("d_debugBanPermutation: TeamGenerator_MatchComplete: red %d, blue %d, invalid %d\n", redPlayersList.size, bluePlayersList.size, (int)invalid); }

	// we only care about 4v4 with no subs
	if (invalid || redPlayersList.size != 4 || bluePlayersList.size != 4) {
		ListClear(&redPlayersList);
		ListClear(&bluePlayersList);

		// clear everything
		trap_Cvar_Set("g_lastSelectedPermutationHash", "");
		trap_Cvar_Set("g_lastSelectedPermutationTime", "");
		trap_Cvar_Set("g_lastSelectedPositionlessPermutation", "");
		trap_Cvar_Set("g_bannedPermutationHash", "");
		trap_Cvar_Set("g_bannedPermutationTime", "");
		return;
	}

	// work through each linked list, feeding each account name to the hasher
	// todo: clean up this ugly mess
	char names[2][4][MAX_ACCOUNTNAME_LEN];
	memset(names, 0, sizeof(names));
	gotAccount_t *ptr = ((gotAccount_t *)redPlayersList.head);
	assert(ptr);
	Q_strncpyz(names[0][0], ptr->name, MAX_ACCOUNTNAME_LEN);
	ptr = (gotAccount_t *)(ptr->node.next);
	Q_strncpyz(names[0][1], ptr->name, MAX_ACCOUNTNAME_LEN);
	ptr = (gotAccount_t *)(ptr->node.next);
	Q_strncpyz(names[0][2], ptr->name, MAX_ACCOUNTNAME_LEN);
	ptr = (gotAccount_t *)(ptr->node.next);
	Q_strncpyz(names[0][3], ptr->name, MAX_ACCOUNTNAME_LEN);

	ptr = ((gotAccount_t *)bluePlayersList.head);
	assert(ptr);
	Q_strncpyz(names[1][0], ptr->name, MAX_ACCOUNTNAME_LEN);
	ptr = (gotAccount_t *)(ptr->node.next);
	Q_strncpyz(names[1][1], ptr->name, MAX_ACCOUNTNAME_LEN);
	ptr = (gotAccount_t *)(ptr->node.next);
	Q_strncpyz(names[1][2], ptr->name, MAX_ACCOUNTNAME_LEN);
	ptr = (gotAccount_t *)(ptr->node.next);
	Q_strncpyz(names[1][3], ptr->name, MAX_ACCOUNTNAME_LEN);

	// hash it and compare with the positionless hash we voted on
	XXH32_hash_t thisHash = HashPositionlessPermutation(names[0][0], names[0][1], names[0][2], names[0][3], names[1][0], names[1][1], names[1][2], names[1][3]);
	XXH32_hash_t oldHash = strtoul(g_lastSelectedPositionlessPermutation.string, NULL, 10);

	if (thisHash == oldHash) {
		// we just completed a pug with the same set of teams that we last generated, irrespective of pos
		// (we don't evaluate whether the positions matched, since pos detection isn't guaranteed to be accurate).
		// we will now ban the permutation we (most likely) just played, including pos.
		// also note the time so that this ban expires after a while.
		if (d_debugBanPermutation.integer) {
			Com_Printf("d_debugBanPermutation: TeamGenerator_MatchComplete: hashes match (%u), so banning g_lastSelectedPermutationHash %s. global time is %d.\n",
				thisHash, g_lastSelectedPermutationHash.string, getGlobalTime());
		}
		trap_Cvar_Set("g_bannedPermutationHash", g_lastSelectedPermutationHash.string);
		trap_Cvar_Set("g_bannedPermutationTime", va("%d", getGlobalTime()));
	}
	else {
		// we did NOT play the same teams we voted on; unban anything banned
		if (d_debugBanPermutation.integer) {
			Com_Printf("d_debugBanPermutation: TeamGenerator_MatchComplete: hashes don't match (thisHash %u, oldHash %u), so not banning anything.\n",
				thisHash, oldHash);
		}
		trap_Cvar_Set("g_bannedPermutationHash", "");
		trap_Cvar_Set("g_bannedPermutationTime", "");
	}

	// in any case, we are completely finished with the teamgen vote at this point
	trap_Cvar_Set("g_lastSelectedPermutationHash", "");
	trap_Cvar_Set("g_lastSelectedPermutationTime", "");
	trap_Cvar_Set("g_lastSelectedPositionlessPermutation", "");

	ListClear(&redPlayersList);
	ListClear(&bluePlayersList);
}

void ActivateTeamsProposal(permutationOfTeams_t *permutation) {
	assert(permutation);

	TeamGen_ClearRemindPositions(qfalse);

	const size_t messageSize = MAX_STRING_CHARS;
	char *printMessage = calloc(MAX_CLIENTS * messageSize, sizeof(char));

	qboolean actuallyChangedTeam[MAX_CLIENTS] = { qfalse };
	qboolean forceteamed[MAX_CLIENTS] = { qfalse };
	for (int i = 0; i < 8; i++) {
		int accountNum, score;
		team_t destinationTeam = i < 4 ? TEAM_RED : TEAM_BLUE;
		char *teamStr = i < 4 ? "r" : "b";
		ctfPosition_t pos;
		switch (i) {
		case 0: accountNum = permutation->teams[0].baseId; pos = CTFPOSITION_BASE; score = SCOREBOARD_POSITION_SCORE_BASE;  break;
		case 1: accountNum = permutation->teams[0].chaseId; pos = CTFPOSITION_CHASE; score = SCOREBOARD_POSITION_SCORE_CHASE;  break;
		case 2: accountNum = permutation->teams[0].offenseId1; pos = CTFPOSITION_OFFENSE; score = SCOREBOARD_POSITION_SCORE_OFFENSE1; break;
		case 3: accountNum = permutation->teams[0].offenseId2; pos = CTFPOSITION_OFFENSE; score = SCOREBOARD_POSITION_SCORE_OFFENSE2; break;
		case 4: accountNum = permutation->teams[1].baseId; pos = CTFPOSITION_BASE; score = SCOREBOARD_POSITION_SCORE_BASE; break;
		case 5: accountNum = permutation->teams[1].chaseId; pos = CTFPOSITION_CHASE; score = SCOREBOARD_POSITION_SCORE_CHASE; break;
		case 6: accountNum = permutation->teams[1].offenseId1; pos = CTFPOSITION_OFFENSE; score = SCOREBOARD_POSITION_SCORE_OFFENSE1; break;
		case 7: accountNum = permutation->teams[1].offenseId2; pos = CTFPOSITION_OFFENSE; score = SCOREBOARD_POSITION_SCORE_OFFENSE2; break;
		default: assert(qfalse); break;
		}
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *ent = &g_entities[j];
			if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || !ent->client->account || ent->client->account->id != accountNum)
				continue;

			if (ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(ent->client->pers.netname, "elo BOT"))
				continue;

			if (ent->client->sess.sessionTeam != destinationTeam)
				actuallyChangedTeam[j] = qtrue;

			ent->client->sess.remindPositionOnMapChange.valid = qtrue;
			ent->client->sess.remindPositionOnMapChange.pos = pos;
			ent->client->sess.remindPositionOnMapChange.score = score;
			if (ent->client->stats)
				ent->client->stats->remindedPosition = pos;
			G_DBSetMetadata(va("remindpos_account_%d", accountNum), va("%d", pos));

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

			Com_sprintf(printMessage + (j * messageSize), messageSize, "\n\n\n^1Red team:^7 (%0.2f'/. relative strength)\n", permutation->teams[0].relativeStrength * 100.0);
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
			actuallyChangedTeam[i] = qtrue;
		}
		Com_sprintf(printMessage + (i * messageSize), messageSize, "\n\n\n^1Red team:^7 (%0.2f'/. relative strength)\n", permutation->teams[0].relativeStrength * 100.0);
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^5Base: ^7 %s\n", permutation->teams[0].baseName));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^6Chase: ^7 %s\n", permutation->teams[0].chaseName));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^2Offense: ^7 %s^7, ", permutation->teams[0].offense1Name));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("%s\n\n", permutation->teams[0].offense2Name));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^4Blue team:^7 (%0.2f'/. relative strength)\n", permutation->teams[1].relativeStrength * 100.0));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^5Base: ^7 %s\n", permutation->teams[1].baseName));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^6Chase: ^7 %s\n", permutation->teams[1].chaseName));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^2Offense: ^7 %s^7, ", permutation->teams[1].offense1Name));
		Q_strcat(printMessage + (i * messageSize), messageSize, va("^7%s\n\n", permutation->teams[1].offense2Name));

		ent->client->ps.persistant[PERS_SCORE] = 0;
		memset(&ent->client->sess.remindPositionOnMapChange, 0, sizeof(ent->client->sess.remindPositionOnMapChange));
	}

	G_UniqueTickedCenterPrint(printMessage, messageSize, 30000, qtrue);
	free(printMessage);

	if (level.autoStartPending) {
		Com_Printf("The currently pending auto start was cancelled due to team generator changing the teams.\n");
		level.autoStartPending = qfalse;
	}

	RememberSelectedPermutation(permutation);
	StartAutomaticTeamGenMapVote();

	char timeBuf[MAX_STRING_CHARS] = { 0 };
	Com_sprintf(timeBuf, sizeof(timeBuf), "%d", (int)time(NULL));
	trap_Cvar_Set("g_lastTeamGenTime", timeBuf);

	// update clientinfo for anyone else that wasn't forceteamed above
	// (clientinfo for people who were forceteamed already got updated above in SetTeam)
	if (g_broadcastCtfPos.integer) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || actuallyChangedTeam[i])
				continue;
			ClientUserinfoChanged(i);
		}
	}
}

qboolean TeamGenerator_VoteForTeamPermutations(gentity_t *ent, const char *voteStr, char **newMessage) {
	assert(ent && VALIDSTRING(voteStr));

	if (!ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You do not have an account, so you cannot vote for team proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(ent->client->pers.netname, "elo BOT")) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have either modded elo bot to cheese the system or this is a bug which you should report.");
		return qtrue;
	}

	if (!level.activePugProposal) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "No pug proposal is currently active.");
		return qtrue;
	}

	if (level.teamPermutationsShownTime && trap_Milliseconds() - level.teamPermutationsShownTime < 500) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Please read the teams proposals before voting.");
		return qtrue;
	}

	if (g_vote_teamgen_readBeforeVotingMilliseconds.integer > 0) {
		if (level.teamPermutationsShownTime && trap_Milliseconds() - level.teamPermutationsShownTime < g_vote_teamgen_readBeforeVotingMilliseconds.integer) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Please take the time to read the teams proposals before voting.");
			ent->client->pers.triedToInstaVote = qtrue;
			return qtrue;
		}
	}

	if (g_vote_teamgen_readBeforeVotingMillisecondsJawa.integer > 0 && ((ent->client->account->flags & ACCOUNTFLAG_INSTAVOTETROLL) || ent->client->pers.triedToInstaVote)) {
		if (level.teamPermutationsShownTime && trap_Milliseconds() - level.teamPermutationsShownTime < g_vote_teamgen_readBeforeVotingMillisecondsJawa.integer) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Please take the time to read the teams proposals before voting.");
			return qtrue;
		}
	}

	int numPlayers = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (level.activePugProposal->clients[i].accountName[0])
			++numPlayers;
	}
	if (g_vote_teamgen_preventBindsWith8PlayersMilliseconds.integer > 0 && numPlayers == 8) {
		if (trap_Milliseconds() - ent->client->pers.chatboxUpTime >= g_vote_teamgen_preventBindsWith8PlayersMilliseconds.integer) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot use vote binds when there are exactly 8 players.");
			return qtrue;
		}
	}

	char oldVotes[NUM_TEAMGENERATORTYPES] = { 0 };
	char newVotes[NUM_TEAMGENERATORTYPES] = { 0 };
	for (const char *p = voteStr; *p && p - voteStr < 4; p++) {
		const char lower = tolower((unsigned)*p);
		int *votesIntRed, *votesIntBlue;
		permutationOfTeams_t *permutation;
		if (level.activePugProposal->suggested.valid && level.activePugProposal->suggestedLetter == lower) {
			votesIntRed = &level.activePugProposal->suggestedVoteClientsRed;
			votesIntBlue = &level.activePugProposal->suggestedVoteClientsBlue;
			permutation = &level.activePugProposal->suggested;
		}
		else if (level.activePugProposal->highestCaliber.valid && level.activePugProposal->highestCaliberLetter == lower) {
			votesIntRed = &level.activePugProposal->highestCaliberVoteClientsRed;
			votesIntBlue = &level.activePugProposal->highestCaliberVoteClientsBlue;
			permutation = &level.activePugProposal->highestCaliber;
		}
		else if (level.activePugProposal->fairest.valid && level.activePugProposal->fairestLetter == lower) {
			votesIntRed = &level.activePugProposal->fairestVoteClientsRed;
			votesIntBlue = &level.activePugProposal->fairestVoteClientsBlue;
			permutation = &level.activePugProposal->fairest;
		}
		else if (level.activePugProposal->inclusive.valid && level.activePugProposal->inclusiveLetter == lower) {
			votesIntRed = &level.activePugProposal->inclusiveVoteClientsRed;
			votesIntBlue = &level.activePugProposal->inclusiveVoteClientsBlue;
			permutation = &level.activePugProposal->inclusive;
		}
		else if (level.activePugProposal->desired.valid && level.activePugProposal->desiredLetter == lower) {
			votesIntRed = &level.activePugProposal->desiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->desiredVoteClientsBlue;
			permutation = &level.activePugProposal->desired;
		}
		else if (level.activePugProposal->semiDesired.valid && level.activePugProposal->semiDesiredLetter == lower) {
			votesIntRed = &level.activePugProposal->semiDesiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->semiDesiredVoteClientsBlue;
			permutation = &level.activePugProposal->semiDesired;
		}
		else if (level.activePugProposal->firstChoice.valid && level.activePugProposal->firstChoiceLetter == lower) {
			votesIntRed = &level.activePugProposal->firstChoiceVoteClientsRed;
			votesIntBlue = &level.activePugProposal->firstChoiceVoteClientsBlue;
			permutation = &level.activePugProposal->firstChoice;
		}
		else {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Invalid pug proposal letter '%c'.", *p));
			continue;
		}

		if (!permutation->valid) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Invalid pug proposal letter '%c'.", *p));
			continue;
		}

		qboolean votedYesOnAnotherClient = qfalse;
		if (!(*votesIntRed & (1 << ent - g_entities)) && !(*votesIntBlue & (1 << ent - g_entities))) {
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *other = &g_entities[i];
				if (other == ent || !other->inuse || !other->client || !other->client->account)
					continue;
				if (other->client->account->id != ent->client->account->id)
					continue;
				if (*votesIntRed & (1 << other - g_entities) || *votesIntBlue & (1 << other - g_entities)) {
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
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("You cannot vote on teams proposal %c because you are not part of it.", lower));
			continue;
		}

		if (votedYesOnAnotherClient || *votesIntRed & (1 << ent - g_entities) || *votesIntBlue & (1 << ent - g_entities)) { // already voted for this
			Q_strcat(oldVotes, sizeof(oldVotes), va("%c", lower));
		}
		else { // a new vote
			if (permutation->teams[0].baseId == ent->client->account->id || permutation->teams[0].chaseId == ent->client->account->id || permutation->teams[0].offenseId1 == ent->client->account->id || permutation->teams[0].offenseId2 == ent->client->account->id)
				*votesIntRed |= (1 << (ent - g_entities));
			else
				*votesIntBlue |= (1 << (ent - g_entities));
			Q_strcat(newVotes, sizeof(newVotes), va("%c", lower));
		}
	}

	if (!oldVotes[0] && !newVotes[0])
		return qtrue; // possible in edge cases, e.g. only two teams proposals and they try to vote c

	char oldVotesMessage[64] = { 0 };
	if (oldVotes[0])
		Com_sprintf(oldVotesMessage, sizeof(oldVotesMessage), "You have already voted for teams proposal%s %s.", strlen(oldVotes) > 1 ? "s" : "", oldVotes);

	if (!newVotes[0]) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, oldVotesMessage);
	}
	else {
		if (strlen(newVotes) > 1) {
			Com_Printf("%s^7 voted yes to teams proposals %s.\n", ent->client->pers.netname, voteStr);
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Vote cast for teams proposals %s.%s%s", voteStr, oldVotes[0] ? " " : "", oldVotes[0] ? oldVotesMessage : ""));
		}
		else {
			Com_Printf("%s^7 voted yes to teams proposal %s.\n", ent->client->pers.netname, voteStr);
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Vote cast for teams proposal %s.%s%s", voteStr, oldVotes[0] ? " " : "", oldVotes[0] ? oldVotesMessage : ""));
		}
	}

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%c%s  ", TEAMGEN_CHAT_COMMAND_CHARACTER, voteStr);
		for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
			char letter;
			int *votesIntRed, *votesIntBlue;
			permutationOfTeams_t *permutation;
			switch (i) {
			case TEAMGENERATORTYPE_MOSTPLAYED:
				permutation = &level.activePugProposal->suggested;
				votesIntRed = &level.activePugProposal->suggestedVoteClientsRed;
				votesIntBlue = &level.activePugProposal->suggestedVoteClientsBlue;
				letter = level.activePugProposal->suggestedLetter;
				break;

			case TEAMGENERATORTYPE_HIGHESTRATING:
				permutation = &level.activePugProposal->highestCaliber;
				votesIntRed = &level.activePugProposal->highestCaliberVoteClientsRed;
				votesIntBlue = &level.activePugProposal->highestCaliberVoteClientsBlue;
				letter = level.activePugProposal->highestCaliberLetter;
				break;

			case TEAMGENERATORTYPE_FAIREST:
				permutation = &level.activePugProposal->fairest;
				votesIntRed = &level.activePugProposal->fairestVoteClientsRed;
				votesIntBlue = &level.activePugProposal->fairestVoteClientsBlue;
				letter = level.activePugProposal->fairestLetter;
				break;

			case TEAMGENERATORTYPE_INCLUSIVE:
				permutation = &level.activePugProposal->inclusive;
				votesIntRed = &level.activePugProposal->inclusiveVoteClientsRed;
				votesIntBlue = &level.activePugProposal->inclusiveVoteClientsBlue;
				letter = level.activePugProposal->inclusiveLetter;
				break;

			case TEAMGENERATORTYPE_DESIREDPOS:
				permutation = &level.activePugProposal->desired;
				votesIntRed = &level.activePugProposal->desiredVoteClientsRed;
				votesIntBlue = &level.activePugProposal->desiredVoteClientsBlue;
				letter = level.activePugProposal->desiredLetter;
				break;

			case TEAMGENERATORTYPE_SEMIDESIREDPOS:
				permutation = &level.activePugProposal->semiDesired;
				votesIntRed = &level.activePugProposal->semiDesiredVoteClientsRed;
				votesIntBlue = &level.activePugProposal->semiDesiredVoteClientsBlue;
				letter = level.activePugProposal->semiDesiredLetter;
				break;

			case TEAMGENERATORTYPE_FIRSTCHOICE:
				permutation = &level.activePugProposal->firstChoice;
				votesIntRed = &level.activePugProposal->firstChoiceVoteClientsRed;
				votesIntBlue = &level.activePugProposal->firstChoiceVoteClientsBlue;
				letter = level.activePugProposal->firstChoiceLetter;
				break;

			default: assert(qfalse); break;
			}

			if (!permutation->valid)
				continue;

			int numYesVotesRed = 0, numYesVotesBlue = 0;
			for (int j = 0; j < MAX_CLIENTS; j++) {
				if (*votesIntRed & (1 << j))
					++numYesVotesRed;
				else if (*votesIntBlue & (1 << j))
					++numYesVotesBlue;
			}

			// color white if they voted for it just now
			char color = '9';
			if (newVotes[0] && strchr(newVotes, letter))
				color = '7';

			int numRequired;
			if (g_vote_teamgen_dynamicVoteRequirement.integer) {
				if (permutation->teams[0].relativeStrength >= 0.519f - 0.0001f || permutation->teams[1].relativeStrength >= 0.519f - 0.0001f)
					numRequired = 7;
				else if (permutation->iDiff > 0 || numPlayers > 8)
					numRequired = 6;
				else
					numRequired = 5;
			}
			else {
				numRequired = 5;
			}

			Q_strcat(buf, sizeof(buf), va(" ^%c(%c: %d/%d)", color, letter, numYesVotesRed + numYesVotesBlue, numRequired));
		}
		*newMessage = buf;
	}

	int numPermutationsWithEnoughVotesToPass = 0;
	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		int *votesIntRed, *votesIntBlue;
		permutationOfTeams_t *permutation;
		switch (i) {
		case TEAMGENERATORTYPE_MOSTPLAYED:
			permutation = &level.activePugProposal->suggested;
			votesIntRed = &level.activePugProposal->suggestedVoteClientsRed;
			votesIntBlue = &level.activePugProposal->suggestedVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_HIGHESTRATING:
			permutation = &level.activePugProposal->highestCaliber;
			votesIntRed = &level.activePugProposal->highestCaliberVoteClientsRed;
			votesIntBlue = &level.activePugProposal->highestCaliberVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_FAIREST:
			permutation = &level.activePugProposal->fairest;
			votesIntRed = &level.activePugProposal->fairestVoteClientsRed;
			votesIntBlue = &level.activePugProposal->fairestVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_INCLUSIVE:
			permutation = &level.activePugProposal->inclusive;
			votesIntRed = &level.activePugProposal->inclusiveVoteClientsRed;
			votesIntBlue = &level.activePugProposal->inclusiveVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_DESIREDPOS:
			permutation = &level.activePugProposal->desired;
			votesIntRed = &level.activePugProposal->desiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->desiredVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_SEMIDESIREDPOS:
			permutation = &level.activePugProposal->semiDesired;
			votesIntRed = &level.activePugProposal->semiDesiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->semiDesiredVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_FIRSTCHOICE:
			permutation = &level.activePugProposal->firstChoice;
			votesIntRed = &level.activePugProposal->firstChoiceVoteClientsRed;
			votesIntBlue = &level.activePugProposal->firstChoiceVoteClientsBlue;
			break;

		default: assert(qfalse); break;
		}

		if (!permutation->valid)
			continue;

		int numYesVotesRed = 0, numYesVotesBlue = 0;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			if (*votesIntRed & (1 << j))
				++numYesVotesRed;
			else if (*votesIntBlue & (1 << j))
				++numYesVotesBlue;
		}

		int numRequired;
		if (g_vote_teamgen_dynamicVoteRequirement.integer) {
			if (permutation->teams[0].relativeStrength >= 0.519f - 0.0001f || permutation->teams[1].relativeStrength >= 0.519f - 0.0001f)
				numRequired = 7;
			else if (permutation->iDiff > 0 || numPlayers > 8)
				numRequired = 6;
			else
				numRequired = 5;
		}
		else {
			numRequired = 5;
		}

		if (g_vote_teamgen_require2VotesOnEachTeam.integer) {
			if (numYesVotesRed + numYesVotesBlue >= numRequired && numYesVotesRed >= 2 && numYesVotesBlue >= 2)
				++numPermutationsWithEnoughVotesToPass;
		}
		else {
			if (numYesVotesRed + numYesVotesBlue >= numRequired)
				++numPermutationsWithEnoughVotesToPass;
		}
	}

	int tiebreakerOrder[] = { TEAMGENERATORTYPE_MOSTPLAYED, TEAMGENERATORTYPE_HIGHESTRATING, TEAMGENERATORTYPE_FAIREST, TEAMGENERATORTYPE_INCLUSIVE, TEAMGENERATORTYPE_DESIREDPOS, TEAMGENERATORTYPE_SEMIDESIREDPOS, TEAMGENERATORTYPE_FIRSTCHOICE };
	srand(teamGenSeed);
	FisherYatesShuffle(&tiebreakerOrder[0], NUM_TEAMGENERATORTYPES, sizeof(int));
	srand(time(NULL));

	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		int j = tiebreakerOrder[i];
		char letter;
		int *votesIntRed, *votesIntBlue;
		permutationOfTeams_t *permutation;
		switch (j) {
		case TEAMGENERATORTYPE_MOSTPLAYED:
			permutation = &level.activePugProposal->suggested;
			votesIntRed = &level.activePugProposal->suggestedVoteClientsRed;
			votesIntBlue = &level.activePugProposal->suggestedVoteClientsBlue;
			letter = level.activePugProposal->suggestedLetter;
			break;

		case TEAMGENERATORTYPE_HIGHESTRATING:
			permutation = &level.activePugProposal->highestCaliber;
			votesIntRed = &level.activePugProposal->highestCaliberVoteClientsRed;
			votesIntBlue = &level.activePugProposal->highestCaliberVoteClientsBlue;
			letter = level.activePugProposal->highestCaliberLetter;
			break;

		case TEAMGENERATORTYPE_FAIREST:
			permutation = &level.activePugProposal->fairest;
			votesIntRed = &level.activePugProposal->fairestVoteClientsRed;
			votesIntBlue = &level.activePugProposal->fairestVoteClientsBlue;
			letter = level.activePugProposal->fairestLetter;
			break;

		case TEAMGENERATORTYPE_INCLUSIVE:
			permutation = &level.activePugProposal->inclusive;
			votesIntRed = &level.activePugProposal->inclusiveVoteClientsRed;
			votesIntBlue = &level.activePugProposal->inclusiveVoteClientsBlue;
			letter = level.activePugProposal->inclusiveLetter;
			break;

		case TEAMGENERATORTYPE_DESIREDPOS:
			permutation = &level.activePugProposal->desired;
			votesIntRed = &level.activePugProposal->desiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->desiredVoteClientsBlue;
			letter = level.activePugProposal->desiredLetter;
			break;

		case TEAMGENERATORTYPE_SEMIDESIREDPOS:
			permutation = &level.activePugProposal->semiDesired;
			votesIntRed = &level.activePugProposal->semiDesiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->semiDesiredVoteClientsBlue;
			letter = level.activePugProposal->semiDesiredLetter;
			break;

		case TEAMGENERATORTYPE_FIRSTCHOICE:
			permutation = &level.activePugProposal->firstChoice;
			votesIntRed = &level.activePugProposal->firstChoiceVoteClientsRed;
			votesIntBlue = &level.activePugProposal->firstChoiceVoteClientsBlue;
			letter = level.activePugProposal->firstChoiceLetter;
			break;

		default: assert(qfalse); break;
		}

		if (!permutation->valid)
			continue;

		int numYesVotesRed = 0, numYesVotesBlue = 0;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			if (*votesIntRed & (1 << j))
				++numYesVotesRed;
			else if (*votesIntBlue & (1 << j))
				++numYesVotesBlue;
		}

		int numRequired;
		if (g_vote_teamgen_dynamicVoteRequirement.integer) {
			if (permutation->teams[0].relativeStrength >= 0.519f - 0.0001f || permutation->teams[1].relativeStrength >= 0.519f - 0.0001f)
				numRequired = 7;
			else if (permutation->iDiff > 0 || numPlayers > 8)
				numRequired = 6;
			else
				numRequired = 5;
		}
		else {
			numRequired = 5;
		}
		qboolean thisOnePasses = qfalse;
		if (g_vote_teamgen_require2VotesOnEachTeam.integer) {
			if (numYesVotesRed + numYesVotesBlue >= numRequired && numYesVotesRed >= 2 && numYesVotesBlue >= 2)
				thisOnePasses = qtrue;
		}
		else {
			if (numYesVotesRed + numYesVotesBlue >= numRequired)
				thisOnePasses = qtrue;
		}

		if (thisOnePasses) {
			if (numPermutationsWithEnoughVotesToPass > 1)
				TeamGenerator_QueueServerMessageInChat(-1, va("Teams proposal %c passed by random tiebreaker.", letter));
			else
				TeamGenerator_QueueServerMessageInChat(-1, va("Teams proposal %c passed.", letter));

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
			TeamGenerator_QueueServerMessageInConsole(-1, printMessage);

			ActivateTeamsProposal(permutation);
			ListClear(&level.activePugProposal->avoidedHashesList);
			ListRemove(&level.pugProposalsList, level.activePugProposal);
			level.activePugProposal = NULL;
			break;
		}
	}

	return qfalse;
}

qboolean TeamGenerator_UnvoteForTeamPermutations(gentity_t *ent, const char *voteStr, char **newMessage, const char *completeVoteStr) {
	assert(ent && VALIDSTRING(voteStr));

	if (!ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You do not have an account, so you cannot vote for team proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(ent->client->pers.netname, "elo BOT")) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have either modded elo bot to cheese the system or this is a bug which you should report.");
		return qtrue;
	}

	if (!level.activePugProposal) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "No pug proposal is currently active.");
		return qtrue;
	}

	if (!g_vote_teamgen_unvote.integer) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Unvoting is disabled.");
		return qtrue;
	}

	if (level.teamPermutationsShownTime && trap_Milliseconds() - level.teamPermutationsShownTime < 500) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "The teams proposals have just changed. Please check the new teams proposals.");
		return qtrue;
	}

	int numPlayers = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (level.activePugProposal->clients[i].accountName[0])
			++numPlayers;
	}

	char existingNoVotes[NUM_TEAMGENERATORTYPES] = { 0 };
	char newNoVotes[NUM_TEAMGENERATORTYPES] = { 0 };
	for (const char *p = voteStr; *p && p - voteStr < 4; p++) {
		const char lower = tolower((unsigned)*p);
		int *votesIntRed, *votesIntBlue;
		permutationOfTeams_t *permutation;
		if (level.activePugProposal->suggested.valid && level.activePugProposal->suggestedLetter == lower) {
			votesIntRed = &level.activePugProposal->suggestedVoteClientsRed;
			votesIntBlue = &level.activePugProposal->suggestedVoteClientsBlue;
			permutation = &level.activePugProposal->suggested;
		}
		else if (level.activePugProposal->highestCaliber.valid && level.activePugProposal->highestCaliberLetter == lower) {
			votesIntRed = &level.activePugProposal->highestCaliberVoteClientsRed;
			votesIntBlue = &level.activePugProposal->highestCaliberVoteClientsBlue;
			permutation = &level.activePugProposal->highestCaliber;
		}
		else if (level.activePugProposal->fairest.valid && level.activePugProposal->fairestLetter == lower) {
			votesIntRed = &level.activePugProposal->fairestVoteClientsRed;
			votesIntBlue = &level.activePugProposal->fairestVoteClientsBlue;
			permutation = &level.activePugProposal->fairest;
		}
		else if (level.activePugProposal->inclusive.valid && level.activePugProposal->inclusiveLetter == lower) {
			votesIntRed = &level.activePugProposal->inclusiveVoteClientsRed;
			votesIntBlue = &level.activePugProposal->inclusiveVoteClientsBlue;
			permutation = &level.activePugProposal->inclusive;
		}
		else if (level.activePugProposal->desired.valid && level.activePugProposal->desiredLetter == lower) {
			votesIntRed = &level.activePugProposal->desiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->desiredVoteClientsBlue;
			permutation = &level.activePugProposal->desired;
		}
		else if (level.activePugProposal->semiDesired.valid && level.activePugProposal->semiDesiredLetter == lower) {
			votesIntRed = &level.activePugProposal->semiDesiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->semiDesiredVoteClientsBlue;
			permutation = &level.activePugProposal->semiDesired;
		}
		else if (level.activePugProposal->firstChoice.valid && level.activePugProposal->firstChoiceLetter == lower) {
			votesIntRed = &level.activePugProposal->firstChoiceVoteClientsRed;
			votesIntBlue = &level.activePugProposal->firstChoiceVoteClientsBlue;
			permutation = &level.activePugProposal->firstChoice;
		}
		else {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Invalid pug proposal letter '%c'.", *p));
			continue;
		}

		if (!permutation->valid) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Invalid pug proposal letter '%c'.", *p));
			continue;
		}

		int clientsIVotedYesOn = 0;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *other = &g_entities[i];
			if (/*other == ent || */!other->inuse || !other->client || !other->client->account)
				continue;
			if (other->client->account->id != ent->client->account->id)
				continue;
			if (*votesIntRed & (1 << other - g_entities) || *votesIntBlue & (1 << other - g_entities))
				clientsIVotedYesOn |= (1 << i);
		}

		qboolean allowedToVote = qfalse;
		if (permutation->teams[0].baseId == ent->client->account->id || permutation->teams[0].chaseId == ent->client->account->id ||
			permutation->teams[0].offenseId1 == ent->client->account->id || permutation->teams[0].offenseId2 == ent->client->account->id ||
			permutation->teams[1].baseId == ent->client->account->id || permutation->teams[1].chaseId == ent->client->account->id ||
			permutation->teams[1].offenseId1 == ent->client->account->id || permutation->teams[1].offenseId2 == ent->client->account->id) {
			allowedToVote = qtrue;
		}

		if (!allowedToVote) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("You cannot vote on teams proposal %c because you are not part of it.", lower));
			continue;
		}

		if (!clientsIVotedYesOn) {
			Q_strcat(existingNoVotes, sizeof(existingNoVotes), va("%c", lower));
			continue;
		}

		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *other = &g_entities[i];
			if (/*other == ent || */!other->inuse || !other->client || !other->client->account)
				continue;
			if (other->client->account->id != ent->client->account->id)
				continue;
			if (*votesIntRed & (1 << other - g_entities))
				*votesIntRed &= ~(1 << i);
			/*else*/ if (*votesIntBlue & (1 << other - g_entities))
				*votesIntBlue &= ~(1 << i);
		}

		Q_strcat(newNoVotes, sizeof(newNoVotes), va("%c", lower));
	}

	if (!existingNoVotes[0] && !newNoVotes[0])
		return qtrue;

	char existingNoVotesMessage[64] = { 0 };
	if (existingNoVotes[0])
		Com_sprintf(existingNoVotesMessage, sizeof(existingNoVotesMessage), "You have already not voted for teams proposal%s %s.", strlen(existingNoVotes) > 1 ? "s" : "", existingNoVotes);

	if (!newNoVotes[0]) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, existingNoVotesMessage);
		return qtrue; // avoid spamming chat with un messages that don't do anything
	}

	if (strlen(newNoVotes) > 1) {
		Com_Printf("%s^7 unvoted for teams proposals %s.\n", ent->client->pers.netname, newNoVotes);
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Vote undone for teams proposals %s.%s%s", newNoVotes, existingNoVotes[0] ? " " : "", existingNoVotes[0] ? existingNoVotesMessage : ""));
	}
	else {
		Com_Printf("%s^7 unvoted for teams proposal %s.\n", ent->client->pers.netname, newNoVotes);
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Vote undone for teams proposal %s.%s%s", newNoVotes, existingNoVotes[0] ? " " : "", existingNoVotes[0] ? existingNoVotesMessage : ""));
	}

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%c%s  ", TEAMGEN_CHAT_COMMAND_CHARACTER, completeVoteStr);
		for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
			char letter;
			int *votesIntRed, *votesIntBlue;
			permutationOfTeams_t *permutation;
			switch (i) {
			case TEAMGENERATORTYPE_MOSTPLAYED:
				permutation = &level.activePugProposal->suggested;
				votesIntRed = &level.activePugProposal->suggestedVoteClientsRed;
				votesIntBlue = &level.activePugProposal->suggestedVoteClientsBlue;
				letter = level.activePugProposal->suggestedLetter;
				break;

			case TEAMGENERATORTYPE_HIGHESTRATING:
				permutation = &level.activePugProposal->highestCaliber;
				votesIntRed = &level.activePugProposal->highestCaliberVoteClientsRed;
				votesIntBlue = &level.activePugProposal->highestCaliberVoteClientsBlue;
				letter = level.activePugProposal->highestCaliberLetter;
				break;

			case TEAMGENERATORTYPE_FAIREST:
				permutation = &level.activePugProposal->fairest;
				votesIntRed = &level.activePugProposal->fairestVoteClientsRed;
				votesIntBlue = &level.activePugProposal->fairestVoteClientsBlue;
				letter = level.activePugProposal->fairestLetter;
				break;

			case TEAMGENERATORTYPE_INCLUSIVE:
				permutation = &level.activePugProposal->inclusive;
				votesIntRed = &level.activePugProposal->inclusiveVoteClientsRed;
				votesIntBlue = &level.activePugProposal->inclusiveVoteClientsBlue;
				letter = level.activePugProposal->inclusiveLetter;
				break;

			case TEAMGENERATORTYPE_DESIREDPOS:
				permutation = &level.activePugProposal->desired;
				votesIntRed = &level.activePugProposal->desiredVoteClientsRed;
				votesIntBlue = &level.activePugProposal->desiredVoteClientsBlue;
				letter = level.activePugProposal->desiredLetter;
				break;

			case TEAMGENERATORTYPE_SEMIDESIREDPOS:
				permutation = &level.activePugProposal->semiDesired;
				votesIntRed = &level.activePugProposal->semiDesiredVoteClientsRed;
				votesIntBlue = &level.activePugProposal->semiDesiredVoteClientsBlue;
				letter = level.activePugProposal->semiDesiredLetter;
				break;

			case TEAMGENERATORTYPE_FIRSTCHOICE:
				permutation = &level.activePugProposal->firstChoice;
				votesIntRed = &level.activePugProposal->firstChoiceVoteClientsRed;
				votesIntBlue = &level.activePugProposal->firstChoiceVoteClientsBlue;
				letter = level.activePugProposal->firstChoiceLetter;
				break;

			default: assert(qfalse); break;
			}

			if (!permutation->valid)
				continue;

			int numYesVotesRed = 0, numYesVotesBlue = 0;
			for (int j = 0; j < MAX_CLIENTS; j++) {
				if (*votesIntRed & (1 << j))
					++numYesVotesRed;
				else if (*votesIntBlue & (1 << j))
					++numYesVotesBlue;
			}

			// color white if they unvoted for it just now
			char color = '9';
			if (newNoVotes[0] && strchr(newNoVotes, letter))
				color = '7';

			int numRequired;
			if (g_vote_teamgen_dynamicVoteRequirement.integer) {
				if (permutation->teams[0].relativeStrength >= 0.519f - 0.0001f || permutation->teams[1].relativeStrength >= 0.519f - 0.0001f)
					numRequired = 7;
				else if (permutation->iDiff > 0 || numPlayers > 8)
					numRequired = 6;
				else
					numRequired = 5;
			}
			else {
				numRequired = 5;
			}
			Q_strcat(buf, sizeof(buf), va(" ^%c(%c: %d/%d)", color, letter, numYesVotesRed + numYesVotesBlue, numRequired));
		}
		*newMessage = buf;
	}

	int numPermutationsWithEnoughVotesToPass = 0;
	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		int *votesIntRed, *votesIntBlue;
		permutationOfTeams_t *permutation;
		switch (i) {
		case TEAMGENERATORTYPE_MOSTPLAYED:
			permutation = &level.activePugProposal->suggested;
			votesIntRed = &level.activePugProposal->suggestedVoteClientsRed;
			votesIntBlue = &level.activePugProposal->suggestedVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_HIGHESTRATING:
			permutation = &level.activePugProposal->highestCaliber;
			votesIntRed = &level.activePugProposal->highestCaliberVoteClientsRed;
			votesIntBlue = &level.activePugProposal->highestCaliberVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_FAIREST:
			permutation = &level.activePugProposal->fairest;
			votesIntRed = &level.activePugProposal->fairestVoteClientsRed;
			votesIntBlue = &level.activePugProposal->fairestVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_INCLUSIVE:
			permutation = &level.activePugProposal->inclusive;
			votesIntRed = &level.activePugProposal->inclusiveVoteClientsRed;
			votesIntBlue = &level.activePugProposal->inclusiveVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_DESIREDPOS:
			permutation = &level.activePugProposal->desired;
			votesIntRed = &level.activePugProposal->desiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->desiredVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_SEMIDESIREDPOS:
			permutation = &level.activePugProposal->semiDesired;
			votesIntRed = &level.activePugProposal->semiDesiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->semiDesiredVoteClientsBlue;
			break;

		case TEAMGENERATORTYPE_FIRSTCHOICE:
			permutation = &level.activePugProposal->firstChoice;
			votesIntRed = &level.activePugProposal->firstChoiceVoteClientsRed;
			votesIntBlue = &level.activePugProposal->firstChoiceVoteClientsBlue;
			break;

		default: assert(qfalse); break;
		}

		if (!permutation->valid)
			continue;

		int numYesVotesRed = 0, numYesVotesBlue = 0;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			if (*votesIntRed & (1 << j))
				++numYesVotesRed;
			else if (*votesIntBlue & (1 << j))
				++numYesVotesBlue;
		}

		int numRequired;
		if (g_vote_teamgen_dynamicVoteRequirement.integer) {
			if (permutation->teams[0].relativeStrength >= 0.519f - 0.0001f || permutation->teams[1].relativeStrength >= 0.519f - 0.0001f)
				numRequired = 7;
			else if (permutation->iDiff > 0 || numPlayers > 8)
				numRequired = 6;
			else
				numRequired = 5;
		}
		else {
			numRequired = 5;
		}
		if (g_vote_teamgen_require2VotesOnEachTeam.integer) {
			if (numYesVotesRed + numYesVotesBlue >= numRequired && numYesVotesRed >= 2 && numYesVotesBlue >= 2)
				++numPermutationsWithEnoughVotesToPass;
		}
		else {
			if (numYesVotesRed + numYesVotesBlue >= numRequired)
				++numPermutationsWithEnoughVotesToPass;
		}
	}

	int tiebreakerOrder[] = { TEAMGENERATORTYPE_MOSTPLAYED, TEAMGENERATORTYPE_HIGHESTRATING, TEAMGENERATORTYPE_FAIREST, TEAMGENERATORTYPE_INCLUSIVE, TEAMGENERATORTYPE_DESIREDPOS, TEAMGENERATORTYPE_SEMIDESIREDPOS, TEAMGENERATORTYPE_FIRSTCHOICE };
	srand(teamGenSeed);
	FisherYatesShuffle(&tiebreakerOrder[0], NUM_TEAMGENERATORTYPES, sizeof(int));
	srand(time(NULL));

	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		int j = tiebreakerOrder[i];
		char letter;
		int *votesIntRed, *votesIntBlue;
		permutationOfTeams_t *permutation;
		switch (j) {
		case TEAMGENERATORTYPE_MOSTPLAYED:
			permutation = &level.activePugProposal->suggested;
			votesIntRed = &level.activePugProposal->suggestedVoteClientsRed;
			votesIntBlue = &level.activePugProposal->suggestedVoteClientsBlue;
			letter = level.activePugProposal->suggestedLetter;
			break;

		case TEAMGENERATORTYPE_HIGHESTRATING:
			permutation = &level.activePugProposal->highestCaliber;
			votesIntRed = &level.activePugProposal->highestCaliberVoteClientsRed;
			votesIntBlue = &level.activePugProposal->highestCaliberVoteClientsBlue;
			letter = level.activePugProposal->highestCaliberLetter;
			break;

		case TEAMGENERATORTYPE_FAIREST:
			permutation = &level.activePugProposal->fairest;
			votesIntRed = &level.activePugProposal->fairestVoteClientsRed;
			votesIntBlue = &level.activePugProposal->fairestVoteClientsBlue;
			letter = level.activePugProposal->fairestLetter;
			break;

		case TEAMGENERATORTYPE_INCLUSIVE:
			permutation = &level.activePugProposal->inclusive;
			votesIntRed = &level.activePugProposal->inclusiveVoteClientsRed;
			votesIntBlue = &level.activePugProposal->inclusiveVoteClientsBlue;
			letter = level.activePugProposal->inclusiveLetter;
			break;

		case TEAMGENERATORTYPE_DESIREDPOS:
			permutation = &level.activePugProposal->desired;
			votesIntRed = &level.activePugProposal->desiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->desiredVoteClientsBlue;
			letter = level.activePugProposal->desiredLetter;
			break;

		case TEAMGENERATORTYPE_SEMIDESIREDPOS:
			permutation = &level.activePugProposal->semiDesired;
			votesIntRed = &level.activePugProposal->semiDesiredVoteClientsRed;
			votesIntBlue = &level.activePugProposal->semiDesiredVoteClientsBlue;
			letter = level.activePugProposal->semiDesiredLetter;
			break;

		case TEAMGENERATORTYPE_FIRSTCHOICE:
			permutation = &level.activePugProposal->firstChoice;
			votesIntRed = &level.activePugProposal->firstChoiceVoteClientsRed;
			votesIntBlue = &level.activePugProposal->firstChoiceVoteClientsBlue;
			letter = level.activePugProposal->firstChoiceLetter;
			break;

		default: assert(qfalse); break;
		}

		if (!permutation->valid)
			continue;

		int numYesVotesRed = 0, numYesVotesBlue = 0;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			if (*votesIntRed & (1 << j))
				++numYesVotesRed;
			else if (*votesIntBlue & (1 << j))
				++numYesVotesBlue;
		}

		int numRequired;
		if (g_vote_teamgen_dynamicVoteRequirement.integer) {
			if (permutation->teams[0].relativeStrength >= 0.519f - 0.0001f || permutation->teams[1].relativeStrength >= 0.519f - 0.0001f)
				numRequired = 7;
			else if (permutation->iDiff > 0 || numPlayers > 8)
				numRequired = 6;
			else
				numRequired = 5;
		}
		else {
			numRequired = 5;
		}
		qboolean thisOnePasses = qfalse;
		if (g_vote_teamgen_require2VotesOnEachTeam.integer) {
			if (numYesVotesRed + numYesVotesBlue >= numRequired && numYesVotesRed >= 2 && numYesVotesBlue >= 2)
				thisOnePasses = qtrue;
		}
		else {
			if (numYesVotesRed + numYesVotesBlue >= numRequired)
				thisOnePasses = qtrue;
		}

		if (thisOnePasses) {
			if (numPermutationsWithEnoughVotesToPass > 1)
				TeamGenerator_QueueServerMessageInChat(-1, va("Teams proposal %c passed by random tiebreaker.", letter));
			else
				TeamGenerator_QueueServerMessageInChat(-1, va("Teams proposal %c passed.", letter));

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
			TeamGenerator_QueueServerMessageInConsole(-1, printMessage);

			ActivateTeamsProposal(permutation);
			ListClear(&level.activePugProposal->avoidedHashesList);
			ListRemove(&level.pugProposalsList, level.activePugProposal);
			level.activePugProposal = NULL;
			break;
		}
	}

	return qfalse;
}

qboolean TeamGenerator_VoteYesToPugProposal(gentity_t *ent, int num, pugProposal_t *setOptional, char **newMessage) {
	assert(ent && ent->client);

	if (!ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You do not have an account, so you cannot vote for pug proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(ent->client->pers.netname, "elo BOT")) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have either modded elo bot to cheese the system or this is a bug which you should report.");
		return qtrue;
	}

	if (!setOptional && num <= 0) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Invalid pug proposal number.");
		return qtrue;
	}

	// use the pointer if one was provided; otherwise, find the set matching the number provided
	pugProposal_t *set = NULL;
	set = setOptional ? setOptional : ListFind(&level.pugProposalsList, PugProposalMatchesNum, &num, NULL);

	if (!set) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Invalid pug proposal number.");
		return qtrue;
	}

	if (setOptional)
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "A pug with these players has already been proposed. Changed your command into a vote for it.");

	if (set->passed) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "This pug proposal has already passed.");
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
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote on this pug proposal because you are not part of it.");
		return qtrue;
	}

	// prevent people from trolling live pugs
	qboolean pugIsLive = qfalse;
	static qboolean pugWasLive = qfalse;
	if (level.wasRestarted && !level.someoneWasAFK && level.numTeamTicks && g_vote_teamgen_preventStartDuringPug.integer) {
		int numCurrentlyIngame = 0;
		CountPlayers(NULL, NULL, NULL, NULL, NULL, &numCurrentlyIngame, NULL);

		// specs cannot vote during live pugs
		if (numCurrentlyIngame >= 6 && IsLivePug(0) == 4) {

			pugIsLive = pugWasLive = qtrue;

			if (IsRacerOrSpectator(ent)) {
				TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Cannot vote from %s while there is currently an ongoing match.", ent->client->sess.inRacemode ? "racemode" : "spec"));
				return qtrue;
			}

		}
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

	qboolean doVote = qtrue;
	if (votedYesOnAnotherClient || set->votedYesClients & (1 << ent - g_entities)) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have already voted for this pug proposal.");
		doVote = qfalse;
	}

	if (doVote) {
		Com_Printf("%s^7 voted yes to pug proposal %d.\n", ent->client->pers.netname, set->num);
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Vote cast for pug proposal %d.", set->num));
		set->votedYesClients |= (1 << (ent - g_entities));
	}

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

	int numRequired = 4;

	if (g_vote_teamgen_autoAdjustRequiredPugVotes.integer) {
		// count number of players ingame with pos
		int numIngameWithPos = 0;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ingameWithPos = &g_entities[i];
			if (!ingameWithPos->client || ingameWithPos->client->pers.connected != CON_CONNECTED)
				continue;
			if (IsRacerOrSpectator(ingameWithPos) || GetRemindedPosOrDeterminedPos(ingameWithPos) == CTFPOSITION_UNKNOWN)
				continue;
			++numIngameWithPos;
		}

		// count number of pickable players
		int numPickable = 0;
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *pickable = &g_entities[i];
			if (!pickable->client || pickable->client->pers.connected != CON_CONNECTED || !pickable->client->account)
				continue;
			if (pickable->client->pers.hasSpecName)
				continue;
			if (TeamGenerator_PlayerIsBarredFromTeamGenerator(pickable))
				continue;
			++numPickable;
		}

		if (numIngameWithPos >= 6 && numPickable >= 12) {
			numRequired = (numPickable - 8) + 1; // e.g. with 16 pickables, require 9 to re?start
		}
	}

	if (pugIsLive && numRequired < 5 && g_vote_teamgen_preventStartDuringPug.integer)
		numRequired = 5; // require 5 votes instead of 4 if vote is called during live pug

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%c%d   ^%c(%d/%d)", TEAMGEN_CHAT_COMMAND_CHARACTER, set->num, doVote ? '7' : '9', numYesVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	if (numYesVotesFromEligiblePlayers >= numRequired && (doVote || (pugWasLive && !pugIsLive)))
		ActivatePugProposal(set, qfalse);

	return qfalse;
}

// returns qtrue if the message should be filtered out
qboolean TeamGenerator_PugStart(gentity_t *ent, char **newMessage) {
	if (g_gametype.integer != GT_CTF)
		return qfalse;

	TeamGen_Initialize();

	assert(ent && ent->client);

	if (!ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You do not have an account, so you cannot start pug proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(ent->client->pers.netname, "elo BOT")) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have either modded elo bot to cheese the system or this is a bug which you should report.");
		return qtrue;
	}

	sortedClient_t clients[MAX_CLIENTS] = {0};
	int numEligible = 0, numIngameEligible = 0;
	GetCurrentPickablePlayers(&clients[0], &numEligible, &numIngameEligible, qfalse);

	if (numEligible < 8) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Not enough eligible players.");
		return qtrue;
	}

	qboolean gotMe = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		sortedClient_t *cl = clients + i;
		if (!cl->accountName[0])
			continue;
		if (cl->accountId == ent->client->account->id) {
			gotMe = qtrue;
			break;
		}
	}

	// prevent unpickable people from starting pugs they won't be part of
	if (!gotMe) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You are not pickable, so you cannot start a pug.");
		return qtrue;
	}

	// prevent people from trolling live pugs
	if (level.wasRestarted && !level.someoneWasAFK && level.numTeamTicks && g_vote_teamgen_preventStartDuringPug.integer) {
		int numCurrentlyIngame = 0;
		CountPlayers(NULL, NULL, NULL, NULL, NULL, &numCurrentlyIngame, NULL);

		int duration = level.time - level.startTime;

		// ingame players can only start before 5:00 has elapsed; specs cannot start
		if (numCurrentlyIngame >= 6 && IsLivePug(0) == 4 && (duration > (60 * 1000 * 5) || IsRacerOrSpectator(ent))) {

			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("There is currently an ongoing match.%s",
				IsRacerOrSpectator(ent) ? "" : " Consider calling an endmatch vote."));

			return qtrue;
		}
	}

	XXH32_hash_t hash = HashPugProposal(&clients[0]);
	pugProposal_t *set = ListFind(&level.pugProposalsList, PugProposalMatchesHash, &hash, NULL);
	if (set) {
		// this person is attempting to start a pug with a combination that has already been proposed. redirect it to a yes vote on that combination.
		return TeamGenerator_VoteYesToPugProposal(ent, 0, set, newMessage);
	}

	if (g_vote_teamgen_minSecsSinceIntermission.integer && level.g_lastIntermissionStartTimeSettingAtRoundStart) {
		int minSecs = Com_Clampi(1, 600, g_vote_teamgen_minSecsSinceIntermission.integer);
		qboolean intermissionOccurredRecently = !!(((int)time(NULL)) - level.g_lastIntermissionStartTimeSettingAtRoundStart < (level.g_lastIntermissionStartTimeSettingAtRoundStart + 600));
		if (intermissionOccurredRecently && level.time - level.startTime < minSecs * 1000) {
			char *waitUntilStr;
			if (minSecs >= 60) {
				int minutes = minSecs / 60;
				int seconds = minSecs % 60;
				waitUntilStr = va("%d:%02d", minutes, seconds);
			}
			else {
				waitUntilStr = va("0:%02d", minSecs);
			}
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Please wait until at least %s before starting a new pug.", waitUntilStr));

			// let the message through as a warning for people to rename
			static char buf[MAX_SAY_TEXT] = { 0 };
			Com_sprintf(buf, sizeof(buf), "%cstart   ^9(available after %s)", TEAMGEN_CHAT_COMMAND_CHARACTER, waitUntilStr);
			*newMessage = buf;
			return qfalse;
		}
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
	TeamGenerator_QueueServerMessageInChat(-1, va("%s proposes pug with: %s. Enter ^5%c%d^7 in chat if you approve.", cleanname, namesStr, TEAMGEN_CHAT_COMMAND_CHARACTER, set->num));

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%cstart", TEAMGEN_CHAT_COMMAND_CHARACTER);
		*newMessage = buf;
	}

	return qfalse;
}

void TeamGen_WarnLS(void) {
	int numNonLSConnectedWithAccountAndNotSpecNamedOrBarred = 0, numLS = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->client || ent->client->pers.connected == CON_DISCONNECTED || !ent->client->account || IsSpecName(ent->client->pers.netname))
			continue;
		if (ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL &&
			!(ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) && !(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED)) {
			++numLS;
			continue;
		}
		if (TeamGenerator_PlayerIsBarredFromTeamGenerator(ent))
			continue;
		++numNonLSConnectedWithAccountAndNotSpecNamedOrBarred;
	}

	if (!(numLS && numNonLSConnectedWithAccountAndNotSpecNamedOrBarred && numNonLSConnectedWithAccountAndNotSpecNamedOrBarred + numLS > 8))
		return;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || !ent->client->account || !(ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL) || 
			(ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) || (ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) || ent->client->pers.connected != CON_CONNECTED)
			continue;

		if (ent->client->pers.warnedLS)
			continue;

		if (level.wasRestarted && g_gametype.integer >= GT_TEAM && (ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE))
			continue;

		if (TeamGenerator_PlayerIsPermaBarredButTemporarilyForcedPickable(ent))
			continue;

		if (IsRacerOrSpectator(ent) && ent->client->pers.hasSpecName)
			continue;

		ent->client->pers.warnedLS = qtrue;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *secondEnt = &g_entities[j];
			if (!secondEnt->inuse || !secondEnt->client || !secondEnt->client->account || secondEnt->client->account != ent->client->account || secondEnt->client->pers.connected != CON_CONNECTED)
				continue;
			TeamGenerator_QueueServerMessageInChat(j, va("^7Mivel több mint 8 játékos van, írd be a ^3%cpickable^7 parancsot, ha pugozni szeretnél.", TEAMGEN_CHAT_COMMAND_CHARACTER));
			ClientUserinfoChanged(j);
		}
	}
}

// if there are 8 but only with LS, unbar him
void TeamGen_CheckForUnbarLS(void) {
	int numNonLSConnectedWithAccountAndNotSpecNamedOrBarred = 0, numLS = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->client || ent->client->pers.connected == CON_DISCONNECTED || !ent->client->account)
			continue;

		if (ent->client->pers.connected == CON_CONNECTING)
			return; // don't do it while someone is still loading the map

		if (ent->client->pers.hasSpecName)
			continue;

		if (ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL &&
			!(ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) && !(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED)) {
			if (ent->client->pers.barredFromPugSelection == BARREASON_BARREDBYADMIN || ent->client->pers.barredFromPugSelection == BARREASON_BARREDBYVOTE)
				return; // he was manually barred; don't auto unbar him
			++numLS;
			continue;
		}

		if (TeamGenerator_PlayerIsBarredFromTeamGenerator(ent))
			continue;

		++numNonLSConnectedWithAccountAndNotSpecNamedOrBarred;
	}

	if (!numLS || !numNonLSConnectedWithAccountAndNotSpecNamedOrBarred || numNonLSConnectedWithAccountAndNotSpecNamedOrBarred + numLS >= 9)
		return;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->client || ent->client->pers.connected == CON_DISCONNECTED || !ent->client->account || !(ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL) ||
			ent->client->account->flags & ACCOUNTFLAG_PERMABARRED || ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) {
			continue;
		}

		qboolean foundForcedPickable = qfalse;
		iterator_t iter;
		ListIterate(&level.forcedPickablePermabarredPlayersList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
			if (bp->accountId == ent->client->account->id) {
				foundForcedPickable = qtrue;
				break;
			}
		}

		if (foundForcedPickable)
			continue;

		Com_Printf("Client %d (%s^7) is %s with the LS afk troll flag and there are less than 8 non-LSs. They are now temporarily forced to be pickable.\n",
			i, ent->client->pers.netname, ent->client->account->name);
		//TeamGenerator_QueueServerMessageInChat(-1, va("%s^7 temporarily forced to be pickable for team generation by server.", ent->client->account->name));
		barredOrForcedPickablePlayer_t *add = ListAdd(&level.forcedPickablePermabarredPlayersList, sizeof(barredOrForcedPickablePlayer_t));
		add->accountId = ent->client->account->id;
		add->reason = UNBARREASON_LS;

		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *otherEnt = &g_entities[j];
			if (otherEnt->inuse && otherEnt->client && otherEnt->client->pers.connected == CON_CONNECTED && otherEnt->client->account && otherEnt->client->account->id == ent->client->account->id)
				ClientUserinfoChanged(j);
		}
	}
}

// if there are 8 without LS, bar him
void TeamGen_CheckForRebarLS(void) {
	int numNonLSConnectedWithAccountAndNotSpecNamedOrBarred = 0, numLS = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->client || ent->client->pers.connected == CON_DISCONNECTED || !ent->client->account)
			continue;

		if (ent->client->pers.connected == CON_CONNECTING)
			return; // don't do it while someone is still loading the map

		if (ent->client->pers.hasSpecName)
			continue;

		if (ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL &&
			!(ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) && !(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED)) {
			if (ent->client->pers.permaBarredDeclaredPickable && !(ent->client->pers.barredFromPugSelection == BARREASON_BARREDBYADMIN || ent->client->pers.barredFromPugSelection == BARREASON_BARREDBYVOTE))
				return; // he did ?pickable and wasn't manually barred; don't auto rebar him
			++numLS;
			continue;
		}

		if (TeamGenerator_PlayerIsBarredFromTeamGenerator(ent))
			continue;

		++numNonLSConnectedWithAccountAndNotSpecNamedOrBarred;
	}

	if (!numLS || numNonLSConnectedWithAccountAndNotSpecNamedOrBarred + numLS <= 8)
		return; // not enough non-LSs to rebar LS

	// rebar LS if he was previously made pickable due to not enough players
	iterator_t iter;
	ListIterate(&level.forcedPickablePermabarredPlayersList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
		if (bp->reason == UNBARREASON_LS) {
			Com_Printf("Rebarring LS with account ID %d due to sufficient player count.\n", bp->accountId);
			ListRemove(&level.forcedPickablePermabarredPlayersList, bp);

			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *ent = &g_entities[i];
				if (ent->inuse && ent->client && ent->client->pers.connected == CON_CONNECTED && ent->client->account && ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL) {
					ent->client->pers.warnedLS = qfalse;
					ClientUserinfoChanged(i);
				}
			}

			ListIterate(&level.forcedPickablePermabarredPlayersList, &iter, qfalse);
		}
	}
}


void TeamGen_AnnounceBreak(void) {
	if (!g_vote_teamgen.integer || !g_vote_teamgen_announceBreak.integer || g_gametype.integer != GT_CTF || g_vote_teamgen_minSecsSinceIntermission.integer <= 0 ||
		!level.g_lastIntermissionStartTimeSettingAtRoundStart)
		return;

	// sanity check; make sure a ton of time hasn't gone by since the end
	int minSecs = Com_Clampi(5, 600, g_vote_teamgen_minSecsSinceIntermission.integer);
	qboolean intermissionOccurredRecently = !!(((int)time(NULL)) - level.g_lastIntermissionStartTimeSettingAtRoundStart < (level.g_lastIntermissionStartTimeSettingAtRoundStart + 600));
	if (!intermissionOccurredRecently || !(level.time - level.startTime < minSecs * 1000))
		return;

	// determine how long to show the message
	int printDuration;
	if (minSecs < 60)
		printDuration = minSecs >= 45 ? 45000 : minSecs * 1000; // under 60s = show for max 45 seconds
	else
		printDuration = 60000; // over 60s = show for max 60 seconds

	if (level.time - level.startTime >= printDuration) {
		level.shouldAnnounceBreak = qfalse;
		return;
	}

	// don't spam it
	static int lastPrint = 0;
	if (lastPrint && trap_Milliseconds() - lastPrint < 2500)
		return;

	char *waitUntilStr;
	if (minSecs >= 60) {
		int minutes = minSecs / 60;
		int seconds = minSecs % 60;
		waitUntilStr = va("%d:%02d", minutes, seconds);
	}
	else {
		waitUntilStr = va("0:%02d", minSecs);
	}

	// for longer breaks, we try to get idiots to actually read the message and go AFK now rather than later
	static char *cuteMessages[] = {
		"Grab a coffee",
		"Make some tea",
		"Grab a snack",
		"Take a piss",
		"Move your 2015 VW Jetta",
		"Answer a few emails",
		"Eat some delicious kürtõs kalács",
		"Fry an avocado omelette",
		"Enjoy some exquisite fondue",
		"Unwrap a slice of American cheese",
		"Drink some hot milk with honey"
	};
	static int cuteMessageNum = -1;
	if (cuteMessageNum == -1)
		cuteMessageNum = Q_irand(0, ARRAY_LEN(cuteMessages) - 1);

	const size_t messageSize = MAX_STRING_CHARS;
	char *centerMessages = calloc(MAX_CLIENTS * messageSize, sizeof(char));

	// build the unique centerprint string for each player, and print each player's unique console message
	static qboolean forcedClientInfoUpdateForBarredGuy[MAX_CLIENTS] = { qfalse };
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || !ent->client->account ||
			ent->client->sess.clientType != CLIENT_TYPE_NORMAL || ent->client->pers.barredFromPugSelection) {
			continue;
		}

		if (ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) {
			if (!forcedClientInfoUpdateForBarredGuy[i]) {
				forcedClientInfoUpdateForBarredGuy[i] = qtrue;
				ClientUserinfoChanged(i);
			}
			continue; // don't print anything regardless of whether a hardpermabarred guy was temporarily marked pickable, since they can't change it in any case
		}

		if ((ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) &&
			(ent->client->pers.permaBarredDeclaredPickable || TeamGenerator_PlayerIsPermaBarredButTemporarilyForcedPickable(ent))) {
			if (!forcedClientInfoUpdateForBarredGuy[i]) {
				forcedClientInfoUpdateForBarredGuy[i] = qtrue;
				ClientUserinfoChanged(i);
			}
			continue; // he has already been marked as pickable; don't print anything
		}

		qboolean hasSpecName = IsSpecName(ent->client->pers.netname);
		qboolean isIngame = !!(ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE);
		char pickabilityMessage[MAX_STRING_CHARS] = { 0 }, renameMessage[MAX_STRING_CHARS] = { 0 };

		if (ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) {
			if (!forcedClientInfoUpdateForBarredGuy[i]) {
				forcedClientInfoUpdateForBarredGuy[i] = qtrue;
				ClientUserinfoChanged(i);
			}
			Com_sprintf(pickabilityMessage, sizeof(pickabilityMessage), "^1You are unpickable");
			Com_sprintf(renameMessage, sizeof(renameMessage), "If you want to pug, enter ^3%cpickable^7 in chat", TEAMGEN_CHAT_COMMAND_CHARACTER);
		}
		else if (ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL) {
			if (!forcedClientInfoUpdateForBarredGuy[i]) {
				forcedClientInfoUpdateForBarredGuy[i] = qtrue;
				ClientUserinfoChanged(i);
			}
			if (ent->client->pers.permaBarredDeclaredPickable || TeamGenerator_PlayerIsPermaBarredButTemporarilyForcedPickable(ent)) {
				if (isIngame) { // ingame
					Com_sprintf(pickabilityMessage, sizeof(pickabilityMessage), "^2You are pickable");

					if (hasSpecName)
						Com_sprintf(renameMessage, sizeof(renameMessage), "If you want to spec, go spec");
					else
						Com_sprintf(renameMessage, sizeof(renameMessage), "If you want to spec, go spec & rename");
				}
				else { // spec/racer
					if (hasSpecName) {
						Com_sprintf(pickabilityMessage, sizeof(pickabilityMessage), "^1You are unpickable");
						Com_sprintf(renameMessage, sizeof(renameMessage), "If you want to pug, rename");
					}
					else {
						Com_sprintf(pickabilityMessage, sizeof(pickabilityMessage), "^2You are pickable");
						Com_sprintf(renameMessage, sizeof(renameMessage), "If you want to spec, rename");
					}
				}
			}
			else {
				Com_sprintf(pickabilityMessage, sizeof(pickabilityMessage), "^1You are unpickable");
				Com_sprintf(renameMessage, sizeof(renameMessage), "Mivel több mint 8 játékos van, írd be a ^3%cpickable^7 parancsot, ha pugozni szeretnél.", TEAMGEN_CHAT_COMMAND_CHARACTER);
			}
		}
		else if (isIngame) { // ingame
			Com_sprintf(pickabilityMessage, sizeof(pickabilityMessage), "^2You are pickable");

			if (hasSpecName)
				Com_sprintf(renameMessage, sizeof(renameMessage), "If you want to spec, go spec");
			else
				Com_sprintf(renameMessage, sizeof(renameMessage), "If you want to spec, go spec & rename");
		}
		else { // spec/racer
			if (hasSpecName) {
				Com_sprintf(pickabilityMessage, sizeof(pickabilityMessage), "^1You are unpickable");
				Com_sprintf(renameMessage, sizeof(renameMessage), "If you want to pug, rename");
			}
			else {
				Com_sprintf(pickabilityMessage, sizeof(pickabilityMessage), "^2You are pickable");
				Com_sprintf(renameMessage, sizeof(renameMessage), "If you want to spec, rename");
			}
		}

		if (minSecs < 60) { // short break; just tell people to rename
			Com_sprintf(centerMessages + (i * messageSize), messageSize, "%s^7\n%s", pickabilityMessage, renameMessage);
			if (!lastPrint) // only print the console message once
				PrintIngame(i, "%s. ^7%s.\n", pickabilityMessage, renameMessage);
		}
		else { // longer break; trick idiots into actually looking at the message
			Com_sprintf(centerMessages + (i * messageSize), messageSize, "^2%s AFK break^7\n\n%s\nand be back by %s\n\n%s^7\n%s", waitUntilStr, cuteMessages[Q_irand(0, ARRAY_LEN(cuteMessages) - 1)], waitUntilStr, pickabilityMessage, renameMessage);
			if (!lastPrint) // only print the console message once
				PrintIngame(i, "^2%s AFK break: %s and be back by %s\n%s. ^7%s.\n", waitUntilStr, cuteMessages[cuteMessageNum], waitUntilStr, pickabilityMessage, renameMessage);
		}
	}

	G_UniqueTickedCenterPrint(centerMessages, messageSize, printDuration - (level.time - level.startTime), qtrue);
	free(centerMessages);

	lastPrint = trap_Milliseconds();
}

void TeamGenerator_DoReroll(qboolean forcedByServer) {
	assert(level.activePugProposal);
	level.activePugProposal->votedToRerollClients = level.activePugProposal->votedToCancelClients = 0;

	// make note of the existing teams permutations
	pugProposal_t oldHashes = { 0 };
	int numOld = 0;
	if (level.activePugProposal->suggested.valid) {
		oldHashes.suggested.valid = qtrue;
		oldHashes.suggested.hash = level.activePugProposal->suggested.hash;
		++numOld;
	}
	if (level.activePugProposal->highestCaliber.valid) {
		oldHashes.highestCaliber.valid = qtrue;
		oldHashes.highestCaliber.hash = level.activePugProposal->highestCaliber.hash;
		++numOld;
	}
	if (level.activePugProposal->fairest.valid) {
		oldHashes.fairest.valid = qtrue;
		oldHashes.fairest.hash = level.activePugProposal->fairest.hash;
		++numOld;
	}
	if (level.activePugProposal->desired.valid) {
		oldHashes.desired.valid = qtrue;
		oldHashes.desired.hash = level.activePugProposal->desired.hash;
		++numOld;
	}
	if (level.activePugProposal->inclusive.valid) {
		oldHashes.inclusive.valid = qtrue;
		oldHashes.inclusive.hash = level.activePugProposal->inclusive.hash;
		++numOld;
	}
	if (level.activePugProposal->semiDesired.valid) {
		oldHashes.semiDesired.valid = qtrue;
		oldHashes.semiDesired.hash = level.activePugProposal->semiDesired.hash;
		++numOld;
	}
	if (level.activePugProposal->firstChoice.valid) {
		oldHashes.firstChoice.valid = qtrue;
		oldHashes.firstChoice.hash = level.activePugProposal->firstChoice.hash;
		++numOld;
	}
	assert(numOld);

	//TeamGen_DebugPrintf("Reroll: %d numOld\n", numOld);

	time_t startTime = time(NULL);
	unsigned int bestNumDifferent = 0u, bestSeed = 0u;
	unsigned int originalSeed = teamGenSeed;

	// we used to reroll up to 8 times, but with GenerateTeamsIteratively we now only need to do it once
	++rerollNum;
#ifdef DEBUG_GENERATETEAMS_PRINT
	inReroll = qtrue;
#endif
	for (int i = 0; i < 1; i++) {
		teamGenSeed = startTime + (i * 69);
		srand(teamGenSeed);

		qboolean result = GenerateTeamsIteratively(level.activePugProposal, &level.activePugProposal->suggested, &level.activePugProposal->highestCaliber, &level.activePugProposal->fairest, &level.activePugProposal->desired, &level.activePugProposal->inclusive, &level.activePugProposal->semiDesired, &level.activePugProposal->firstChoice, &level.activePugProposal->numValidPermutationsChecked);
		if (!result) {
			//TeamGen_DebugPrintf("Reroll: i %d (%u) failed\n", i, teamGenSeed);
			break; // ??? this should not happen. somehow we failed to generate any teams at all.
		}

		unsigned int gotNewTeams = 0u;
		if (level.activePugProposal->suggested.valid && oldHashes.suggested.valid && oldHashes.suggested.hash != level.activePugProposal->suggested.hash)
			++gotNewTeams;
		if (level.activePugProposal->highestCaliber.valid && oldHashes.highestCaliber.valid && oldHashes.highestCaliber.hash != level.activePugProposal->highestCaliber.hash)
			++gotNewTeams;
		if (level.activePugProposal->fairest.valid && oldHashes.fairest.valid && oldHashes.fairest.hash != level.activePugProposal->fairest.hash)
			++gotNewTeams;
		if (level.activePugProposal->desired.valid && oldHashes.desired.valid && oldHashes.desired.hash != level.activePugProposal->desired.hash)
			++gotNewTeams;
		if (level.activePugProposal->inclusive.valid && oldHashes.inclusive.valid && oldHashes.inclusive.hash != level.activePugProposal->inclusive.hash)
			++gotNewTeams;
		if (level.activePugProposal->semiDesired.valid && oldHashes.semiDesired.valid && oldHashes.semiDesired.hash != level.activePugProposal->semiDesired.hash)
			++gotNewTeams;
		if (level.activePugProposal->firstChoice.valid && oldHashes.firstChoice.valid && oldHashes.firstChoice.hash != level.activePugProposal->firstChoice.hash)
			++gotNewTeams;

		//TeamGen_DebugPrintf("Reroll: i %d (%u) has %d new teams\n", i, teamGenSeed, gotNewTeams);

		if (gotNewTeams > bestNumDifferent) {
			//TeamGen_DebugPrintf("Reroll: i %d (%u) is new best\n", i, teamGenSeed);
			bestNumDifferent = gotNewTeams;
			bestSeed = teamGenSeed;
			if (gotNewTeams == numOld) { // if this one is as good as we can possibly get (meaning it rerolled every permutation) then just use it; no need to keep iterating
				//TeamGen_DebugPrintf("Reroll: i %d (%u) is optimal because equals numOld\n", i, teamGenSeed);
				break;
			}
		}
	}
#ifdef DEBUG_GENERATETEAMS_PRINT
	inReroll = qfalse;
#endif

	if (bestSeed) {
		//TeamGen_DebugPrintf("Reroll: setting teamGenSeed to bestSeed %u\n", bestSeed);
		teamGenSeed = bestSeed;
		level.activePugProposal->suggestedVoteClientsRed =
			level.activePugProposal->suggestedVoteClientsBlue =
			level.activePugProposal->highestCaliberVoteClientsRed =
			level.activePugProposal->highestCaliberVoteClientsBlue =
			level.activePugProposal->fairestVoteClientsRed =
			level.activePugProposal->fairestVoteClientsBlue =
			level.activePugProposal->desiredVoteClientsRed =
			level.activePugProposal->desiredVoteClientsBlue =
			level.activePugProposal->semiDesiredVoteClientsRed =
			level.activePugProposal->semiDesiredVoteClientsBlue =
			level.activePugProposal->firstChoiceVoteClientsRed =
			level.activePugProposal->firstChoiceVoteClientsBlue =
			0;

		TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d rerolled%s (%s). Check console for new teams proposals.", level.activePugProposal->num, forcedByServer ? " by server" : "", level.activePugProposal->namesStr));
		PrintTeamsProposalsInConsole(level.activePugProposal, -1);
	}
	else {
		TeamGenerator_QueueServerMessageInChat(-1, va("Failed to generate different teams when rerolling pug proposal %d%s (%s). Voting will continue for the existing teams.", level.activePugProposal->num, forcedByServer ? " by server" : "", level.activePugProposal->namesStr));
		teamGenSeed = originalSeed;
	}

	srand(time(NULL));
}

// returns qtrue if the message should be filtered out
qboolean TeamGenerator_VoteToReroll(gentity_t *ent, char **newMessage) {
	assert(ent && ent->client);

	if (!ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You do not have an account, so you cannot vote for pug proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(ent->client->pers.netname, "elo BOT")) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have either modded elo bot to cheese the system or this is a bug which you should report.");
		return qtrue;
	}

	if (!level.activePugProposal) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "No pug proposal is currently active.");
		return qtrue;
	}

	if (level.teamPermutationsShownTime && trap_Milliseconds() - level.teamPermutationsShownTime < 500) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "The teams proposals have just changed. Please check the new teams proposals.");
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
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote to reroll because you are not part of this pug proposal.");
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

	qboolean doVote = qtrue;
	if (votedToRerollOnAnotherClient || level.activePugProposal->votedToRerollClients & (1 << ent - g_entities)) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have already voted to reroll the teams proposals.");
		doVote = qfalse;
	}

	if (doVote) {
		Com_Printf("%s^7 voted to reroll active pug proposal %d.\n", ent->client->pers.netname, level.activePugProposal->num);
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Vote cast to reroll teams proposals.");
		level.activePugProposal->votedToRerollClients |= (1 << (ent - g_entities));
	}

	int numEligibleConnected = 0, numEligibleMax = 0, numRerollVotesFromEligiblePlayers = 0;
	qboolean gotEm[MAX_CLIENTS] = { qfalse };
	for (int i = 0; i < MAX_CLIENTS; i++) {
		sortedClient_t *cl = &level.activePugProposal->clients[i];
		if (!cl->accountName[0])
			continue; // not included in this set

		++numEligibleMax;

		// is this person actually connected?
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *other = &g_entities[j];
			if (!other->inuse || !other->client || !other->client->account || other->client->account->id != cl->accountId || gotEm[j])
				continue;

			// yes, they are connected
			++numEligibleConnected;

			// are they connected on any other clients? see if any of them have voted yes, and mark the duplicates so we know not to check them
			qboolean votedToReroll = qfalse;
			for (int k = 0; k < MAX_CLIENTS; k++) {
				gentity_t *thirdEnt = &g_entities[k];
				if (!thirdEnt->inuse || !thirdEnt->client || !thirdEnt->client->account || thirdEnt->client->account->id != cl->accountId)
					continue;
				if (level.activePugProposal->votedToRerollClients & (1 << k))
					votedToReroll = qtrue;
				gotEm[k] = qtrue;
			}

			if (votedToReroll)
				++numRerollVotesFromEligiblePlayers;

			break;
		}
	}

	// require a simple majority of the total people in the proposal. requiring only 4 or 5 would allow e.g. in a set of 15 people for just a few people who were excluded to reroll it for everyone else
	if (numEligibleConnected > numEligibleMax)
		numEligibleConnected = numEligibleMax; // sanity check
	const int numRequired = (numEligibleConnected / 2) + 1;

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%creroll   ^%c(%d/%d)", TEAMGEN_CHAT_COMMAND_CHARACTER, doVote ? '7' : '9', numRerollVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	if (doVote && numRerollVotesFromEligiblePlayers >= numRequired)
		TeamGenerator_DoReroll(qfalse);

	return qfalse;
}

qboolean BarVoteMatchesAccountId(genericNode_t *node, void *userData) {
	const barVote_t *existing = (const barVote_t *)node;
	int id = *((int *)userData);
	if (existing && existing->accountId == id)
		return qtrue;
	return qfalse;
}

qboolean FuckVoteMatchesString(genericNode_t *node, void *userData) {
	const fuckVote_t *existing = (const fuckVote_t *)node;
	const char *newStr = (const char *)userData;
	if (existing && !Q_stricmp(existing->fucked, newStr))
		return qtrue;
	return qfalse;
}


qboolean TeamGenerator_MemeFuckVote(gentity_t *ent, const char *voteStr, char **newMessage) {
	assert(ent && ent->client);

	if (g_vote_teamgen_fuck.integer <= 0) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Fuck vote is disabled.");
		return qtrue;
	}

	if (!VALIDSTRING(voteStr) || strlen(voteStr) < 6) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Usage: %cfuck [subject of the fucking]", TEAMGEN_CHAT_COMMAND_CHARACTER));
		return qtrue;
	}

	const char *fucked = voteStr + 5;

	char filtered[MAX_FUCK_LENGTH] = { 0 };
	assert(sizeof(filtered) == MAX_FUCK_LENGTH);
	assert(MAX_FUCK_LENGTH < MAX_SAY_TEXT);
	if (strlen(fucked) > MAX_FUCK_LENGTH - 1) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Your message is too long. The limit is %d characters.", MAX_FUCK_LENGTH - 1));
		return qtrue;
	}

	Q_strncpyz(filtered, fucked, sizeof(filtered));
	Q_StripColor(filtered);

	if (!filtered[0]) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Usage: %cfuck [subject of the fucking]", TEAMGEN_CHAT_COMMAND_CHARACTER));
		return qtrue;
	}

	// condense consecutive spaces
	int r = 0, w = 0;
	qboolean previousSpace = qfalse;
	while (filtered[r] != '\0') {
		if (filtered[r] != ' ') {
			filtered[w++] = filtered[r];
			previousSpace = qfalse;
		}
		else if (!previousSpace) {
			filtered[w++] = filtered[r];
			previousSpace = qtrue;
		}
		r++;
	}
	filtered[w] = '\0';

	// trim trailing space
	int len = strlen(filtered);
	if (len && filtered[len - 1] == ' ')
		filtered[len - 1] = '\0';

	// trim leading space
	if (filtered[0] == ' ')
		memmove(filtered, filtered + 1, strlen(filtered));

	qboolean gotAlphaNumeric = qfalse;
	for (char *p = filtered; *p; p++) {
		const char c = *p;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
			gotAlphaNumeric = qtrue;
			break;
		}
	}

	if (!filtered[0] || !gotAlphaNumeric) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Usage: %cfuck [subject of the fucking]", TEAMGEN_CHAT_COMMAND_CHARACTER));
		return qtrue;
	}

	// check whether this fuck vote already exists
	qboolean doVote = qtrue, voteIsNew;
	fuckVote_t *fuckVote = ListFind(&level.fuckVoteList, FuckVoteMatchesString, filtered, NULL);
	if (fuckVote) {
		voteIsNew = qfalse;
		if (fuckVote->done) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("%s has already been fucked.", fuckVote->fucked));
			return qtrue;
		}

		// this vote already exists; see if we already voted on it
		qboolean votedToFuckOnAnotherClient = qfalse;
		if (!(fuckVote->votedYesClients & (1 << ent - g_entities))) {
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *other = &g_entities[i];
				if (other == ent || !other->inuse || !other->client || other->client->pers.connected != CON_CONNECTED)
					continue;
				if (other->client->account && ent->client->account && other->client->account->id != ent->client->account->id)
					continue;
				if (!ent->client->account && !other->client->account)
					continue;
				if (ent->client->account && !other->client->account)
					continue;
				if (!ent->client->account && other->client->account)
					continue;
				if (fuckVote->votedYesClients & (1 << other - g_entities)) {
					votedToFuckOnAnotherClient = qtrue;
					break;
				}
			}
		}

		if (votedToFuckOnAnotherClient || fuckVote->votedYesClients & (1 << ent - g_entities)) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("You have already voted to fuck %s.", fuckVote->fucked));
			doVote = qfalse;
		}
	}
	else {
		voteIsNew = qtrue;

		// doesn't exist yet; create it
		if (level.fuckVoteList.size >= g_vote_teamgen_fuck.integer) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, "The limit of fuck votes has been reached.");
			return qtrue;
		}

		fuckVote = ListAdd(&level.fuckVoteList, sizeof(fuckVote_t));
		Q_strncpyz(fuckVote->fucked, filtered, sizeof(fuckVote->fucked));
	}

	if (doVote)
		fuckVote->votedYesClients |= (1 << (ent - g_entities));

	// check how many votes we are now up to
	int numFuckVotesFromEligiblePlayers = 0;
	if (voteIsNew) {
		numFuckVotesFromEligiblePlayers = 1; // sanity check
	}
	else {
		qboolean gotEm[MAX_CLIENTS] = { qfalse };
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED || gotEm[i])
				continue;

			qboolean votedToFuck = qfalse;
			for (int k = 0; k < MAX_CLIENTS; k++) {
				gentity_t *checkEnt = &g_entities[k];
				if (!checkEnt->inuse || !checkEnt->client || checkEnt->client->pers.connected != CON_CONNECTED ||
					(thisEnt->client->account && checkEnt->client->account && checkEnt->client->account->id != thisEnt->client->account->id))
					continue;
				if (!thisEnt->client->account && !checkEnt->client->account)
					continue;
				if (thisEnt->client->account && !checkEnt->client->account)
					continue;
				if (!thisEnt->client->account && checkEnt->client->account)
					continue;
				if (fuckVote->votedYesClients & (1 << k))
					votedToFuck = qtrue;
				gotEm[k] = qtrue;
			}

			if (fuckVote->votedYesClients & (1 << i))
				votedToFuck = qtrue;
			else if (thisEnt == ent)
				votedToFuck = qtrue;

			if (votedToFuck) {
				++numFuckVotesFromEligiblePlayers;
				gotEm[i] = qtrue;
			}
		}
	}

	// print the message
	int numRequired = Com_Clampi(3, MAX_CLIENTS, g_vote_fuckRequiredVotes.integer);
	if (!Q_stricmpn(fuckVote->fucked, "duo", 3))
		numRequired = 16;

	if (!Q_stricmp(fuckVote->fucked, "hannah"))
		numRequired = 1;

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%cfuck %s   ^%c(%d/%d)",
			TEAMGEN_CHAT_COMMAND_CHARACTER, fuckVote->fucked, doVote ? '7' : '9', numFuckVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	// if we have enough votes, do the fucking
	if (numFuckVotesFromEligiblePlayers >= numRequired) {
		fuckVote->done = qtrue;
		TeamGenerator_QueueServerMessageInChat(-1, va("^7Fuck %s.", fuckVote->fucked));
	}

	return qfalse;
}

qboolean TeamGenerator_MemeGoVote(gentity_t *ent, const char *voteStr, char **newMessage) {
	assert(ent && ent->client);

	if (g_vote_teamgen_fuck.integer <= 0) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Go vote is disabled.");
		return qtrue;
	}

	if (!VALIDSTRING(voteStr) || strlen(voteStr) < 4) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Usage: %cgo [subject of the going]", TEAMGEN_CHAT_COMMAND_CHARACTER));
		return qtrue;
	}

	const char *fucked = voteStr + 3;

	char filtered[MAX_FUCK_LENGTH] = { 0 };
	assert(sizeof(filtered) == MAX_FUCK_LENGTH);
	assert(MAX_FUCK_LENGTH < MAX_SAY_TEXT);
	if (strlen(fucked) > MAX_FUCK_LENGTH - 1) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Your message is too long. The limit is %d characters.", MAX_FUCK_LENGTH - 1));
		return qtrue;
	}

	Q_strncpyz(filtered, fucked, sizeof(filtered));
	Q_StripColor(filtered);

	if (!filtered[0]) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Usage: %cgo [subject of the going]", TEAMGEN_CHAT_COMMAND_CHARACTER));
		return qtrue;
	}

	// condense consecutive spaces
	int r = 0, w = 0;
	qboolean previousSpace = qfalse;
	while (filtered[r] != '\0') {
		if (filtered[r] != ' ') {
			filtered[w++] = filtered[r];
			previousSpace = qfalse;
		}
		else if (!previousSpace) {
			filtered[w++] = filtered[r];
			previousSpace = qtrue;
		}
		r++;
	}
	filtered[w] = '\0';

	// trim trailing space
	int len = strlen(filtered);
	if (len && filtered[len - 1] == ' ')
		filtered[len - 1] = '\0';

	// trim leading space
	if (filtered[0] == ' ')
		memmove(filtered, filtered + 1, strlen(filtered));

	qboolean gotAlphaNumeric = qfalse;
	for (char *p = filtered; *p; p++) {
		const char c = *p;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
			gotAlphaNumeric = qtrue;
			break;
		}
	}

	if (!filtered[0] || !gotAlphaNumeric) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Usage: %cgo [subject of the going]", TEAMGEN_CHAT_COMMAND_CHARACTER));
		return qtrue;
	}

	// check whether this fuck vote already exists
	qboolean doVote = qtrue, voteIsNew;
	fuckVote_t *fuckVote = ListFind(&level.goVoteList, FuckVoteMatchesString, filtered, NULL);
	if (fuckVote) {
		voteIsNew = qfalse;
		if (fuckVote->done) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("%s has already been gone.", fuckVote->fucked));
			return qtrue;
		}

		// this vote already exists; see if we already voted on it
		qboolean votedToFuckOnAnotherClient = qfalse;
		if (!(fuckVote->votedYesClients & (1 << ent - g_entities))) {
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *other = &g_entities[i];
				if (other == ent || !other->inuse || !other->client || other->client->pers.connected != CON_CONNECTED)
					continue;
				if (other->client->account && ent->client->account && other->client->account->id != ent->client->account->id)
					continue;
				if (!ent->client->account && !other->client->account)
					continue;
				if (ent->client->account && !other->client->account)
					continue;
				if (!ent->client->account && other->client->account)
					continue;
				if (fuckVote->votedYesClients & (1 << other - g_entities)) {
					votedToFuckOnAnotherClient = qtrue;
					break;
				}
			}
		}

		if (votedToFuckOnAnotherClient || fuckVote->votedYesClients & (1 << ent - g_entities)) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("You have already voted to go %s.", fuckVote->fucked));
			doVote = qfalse;
		}
	}
	else {
		voteIsNew = qtrue;

		// doesn't exist yet; create it
		if (level.goVoteList.size >= g_vote_teamgen_fuck.integer) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, "The limit of go votes has been reached.");
			return qtrue;
		}

		fuckVote = ListAdd(&level.goVoteList, sizeof(fuckVote_t));
		Q_strncpyz(fuckVote->fucked, filtered, sizeof(fuckVote->fucked));
	}

	if (doVote)
		fuckVote->votedYesClients |= (1 << (ent - g_entities));

	// check how many votes we are now up to
	int numFuckVotesFromEligiblePlayers = 0;
	if (voteIsNew) {
		numFuckVotesFromEligiblePlayers = 1; // sanity check
	}
	else {
		qboolean gotEm[MAX_CLIENTS] = { qfalse };
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED || gotEm[i])
				continue;

			qboolean votedToFuck = qfalse;
			for (int k = 0; k < MAX_CLIENTS; k++) {
				gentity_t *checkEnt = &g_entities[k];
				if (!checkEnt->inuse || !checkEnt->client || checkEnt->client->pers.connected != CON_CONNECTED ||
					(thisEnt->client->account && checkEnt->client->account && checkEnt->client->account->id != thisEnt->client->account->id))
					continue;
				if (!thisEnt->client->account && !checkEnt->client->account)
					continue;
				if (thisEnt->client->account && !checkEnt->client->account)
					continue;
				if (!thisEnt->client->account && checkEnt->client->account)
					continue;
				if (fuckVote->votedYesClients & (1 << k))
					votedToFuck = qtrue;
				gotEm[k] = qtrue;
			}

			if (fuckVote->votedYesClients & (1 << i))
				votedToFuck = qtrue;
			else if (thisEnt == ent)
				votedToFuck = qtrue;

			if (votedToFuck) {
				++numFuckVotesFromEligiblePlayers;
				gotEm[i] = qtrue;
			}
		}
	}

	// print the message
	int numRequired = Com_Clampi(3, MAX_CLIENTS, g_vote_fuckRequiredVotes.integer);
	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%cgo %s   ^%c(%d/%d)",
			TEAMGEN_CHAT_COMMAND_CHARACTER, fuckVote->fucked, doVote ? '7' : '9', numFuckVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	// if we have enough votes, do the fucking
	if (numFuckVotesFromEligiblePlayers >= numRequired) {
		fuckVote->done = qtrue;
		TeamGenerator_QueueServerMessageInChat(-1, va("^7Go %s!", fuckVote->fucked));
	}

	return qfalse;
}

qboolean TeamGenerator_VoteToBar(gentity_t *ent, const char *voteStr, char **newMessage) {
	assert(ent && ent->client);

	if (!g_vote_teamgen_enableBarVote.integer) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Bar vote is disabled.");
		return qtrue;
	}

	if (!ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You do not have an account, so you cannot vote to bar players. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(ent->client->pers.netname, "elo BOT")) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have either modded elo bot to cheese the system or this is a bug which you should report.");
		return qtrue;
	}

	if (!VALIDSTRING(voteStr) || strlen(voteStr) < 5) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Usage: %cbar [account name]", TEAMGEN_CHAT_COMMAND_CHARACTER));
		return qtrue;
	}

	const char *accountName = voteStr + 4;
	accountReference_t acc = G_GetAccountByName(accountName, qtrue);
	if (!acc.ptr) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("No connected player found for account '%s'. Make sure to use account name from /whois, not ingame name.", accountName));
		return qtrue;
	}

	if (acc.ptr == ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote to bar yourself.");
		return qtrue;
	}

	if (IsRacerOrSpectator(ent) && IsSpecName(ent->client->pers.netname)) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You are not pickable, so you cannot vote to bar.");
		return qtrue;
	}

	gentity_t *someoneConnectedOnThisAccount = NULL;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *thisEnt = &g_entities[i];
		if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected == CON_DISCONNECTED || thisEnt->client->account != acc.ptr)
			continue;
		if ((acc.ptr->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(thisEnt->client->pers.netname, "elo BOT"))
			continue;
		someoneConnectedOnThisAccount = thisEnt;
		break;
		// we don't need to check for multiple entities since each one should be barred if any are barred
	}
	if (!someoneConnectedOnThisAccount) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("No connected player found for account '%s'. Make sure to use account name from /whois, not ingame name.", accountName));
		return qtrue;
	}

	// already barred?
	if (TeamGenerator_PlayerIsBarredFromTeamGenerator(someoneConnectedOnThisAccount)) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("%s is already barred.", acc.ptr->name));
		return qtrue;
	}

	// make sure at least 5 people are connected
	int numEligible = 0;
	{
		qboolean gotEm[MAX_CLIENTS] = { qfalse };
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected == CON_DISCONNECTED ||
				!thisEnt->client->account || thisEnt->client->sess.clientType != CLIENT_TYPE_NORMAL || gotEm[i])
				continue;

			if (thisEnt->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(thisEnt->client->pers.netname, "elo BOT"))
				continue;

			++numEligible;

			qboolean votedToBar = qfalse;
			for (int k = 0; k < MAX_CLIENTS; k++) {
				gentity_t *checkEnt = &g_entities[k];
				if (!checkEnt->inuse || !checkEnt->client || checkEnt->client->pers.connected == CON_DISCONNECTED ||
					!checkEnt->client->account || thisEnt->client->sess.clientType != CLIENT_TYPE_NORMAL || checkEnt->client->account->id != thisEnt->client->account->id)
					continue;
				gotEm[k] = qtrue;
			}
		}
	}

	const int numRequired = g_vote_teamgen_enableBarVote.integer;
	const int numRequiredToBeConnected = numRequired;
	if (numEligible < numRequiredToBeConnected) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("At least %d players must be connected to vote to bar someone.", numRequiredToBeConnected));
		return qtrue;
	}

	// check whether this bar vote already exists
	qboolean doVote = qtrue;
	barVote_t *barVote = ListFind(&level.barVoteList, BarVoteMatchesAccountId, &acc.ptr->id, NULL);
	if (barVote) {
		// this vote already exists; see if we already voted on it
		qboolean votedToBarOnAnotherClient = qfalse;
		if (!(barVote->votedYesClients & (1 << ent - g_entities))) {
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *other = &g_entities[i];
				if (other == ent || !other->inuse || !other->client || !other->client->account)
					continue;
				if (other->client->account->id != ent->client->account->id)
					continue;
				if (barVote->votedYesClients & (1 << other - g_entities)) {
					votedToBarOnAnotherClient = qtrue;
					break;
				}
			}
		}

		if (votedToBarOnAnotherClient || barVote->votedYesClients & (1 << ent - g_entities)) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("You have already voted to bar %s.", barVote->accountName));
			doVote = qfalse;
		}
	}
	else {
		// doesn't exist yet; create it
		barVote = ListAdd(&level.barVoteList, sizeof(barVote_t));
		barVote->accountId = acc.ptr->id;
		Q_strncpyz(barVote->accountName, acc.ptr->name, sizeof(barVote->accountName));
	}

	if (doVote)
		barVote->votedYesClients |= (1 << (ent - g_entities));

	// check how many votes we are now up to
	int numBarVotesFromEligiblePlayers = 0;
	{
		qboolean gotEm[MAX_CLIENTS] = { qfalse };
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected == CON_DISCONNECTED ||
				!thisEnt->client->account || thisEnt->client->sess.clientType != CLIENT_TYPE_NORMAL || gotEm[i])
				continue;

			if (thisEnt->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(thisEnt->client->pers.netname, "elo BOT"))
				continue;

			qboolean votedToBar = qfalse;
			for (int k = 0; k < MAX_CLIENTS; k++) {
				gentity_t *checkEnt = &g_entities[k];
				if (!checkEnt->inuse || !checkEnt->client || checkEnt->client->pers.connected == CON_DISCONNECTED ||
					!checkEnt->client->account || thisEnt->client->sess.clientType != CLIENT_TYPE_NORMAL || checkEnt->client->account->id != thisEnt->client->account->id)
					continue;
				if (barVote->votedYesClients & (1 << k))
					votedToBar = qtrue;
				gotEm[k] = qtrue;
			}

			if (votedToBar)
				++numBarVotesFromEligiblePlayers;
		}
	}

	// print the message
	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%cbar %s   ^%c(%d/%d)",
			TEAMGEN_CHAT_COMMAND_CHARACTER, barVote->accountName, doVote ? '7' : '9', numBarVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	// if we have enough votes, do the barring
	if (numBarVotesFromEligiblePlayers >= numRequired) {
		barredOrForcedPickablePlayer_t *add = ListAdd(&level.barredPlayersList, sizeof(barredOrForcedPickablePlayer_t));
		add->accountId = barVote->accountId;
		add->reason = BARREASON_BARREDBYVOTE;

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (!g_entities[i].inuse || !g_entities[i].client || !g_entities[i].client->account || g_entities[i].client->account->id != barVote->accountId)
				continue;
			g_entities[i].client->pers.barredFromPugSelection = BARREASON_BARREDBYVOTE;
			ClientUserinfoChanged(i);
		}

		char announcementBuf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(announcementBuf, sizeof(announcementBuf), "%s^7 was barred from team generation by vote.", barVote->accountName);

		// automatically start and pass a new pug if:
		// - we aren't in the middle of one, AND
		// - either the guy is part of the currently active pug proposal, or he is ingame
		if (g_vote_teamgen_barVoteStartsNewPug.integer) {
			int numCurrentlyIngame = 0;
			CountPlayers(NULL, NULL, NULL, NULL, NULL, &numCurrentlyIngame, NULL);

			float avgRed = level.numTeamTicks ? (float)level.numRedPlayerTicks / (float)level.numTeamTicks : 0;
			float avgBlue = level.numTeamTicks ? (float)level.numBluePlayerTicks / (float)level.numTeamTicks : 0;

			qboolean inPug = !!(level.wasRestarted && !level.someoneWasAFK && level.numTeamTicks && numCurrentlyIngame >= 6 &&
				fabs(avgRed - round(avgRed)) < 0.2f && fabs(avgBlue - round(avgBlue)) < 0.2f);

			if (inPug) {
				TeamGenerator_QueueServerMessageInChat(-1, announcementBuf);
				ListRemove(&level.barVoteList, barVote);
			}
			else {
				qboolean shouldStartNewProposal = qfalse;

				// is he in the currently active pug proposal?
				if (level.activePugProposal) {
					for (int i = 0; i < MAX_CLIENTS; i++) {
						const sortedClient_t *cl = &level.activePugProposal->clients[i];
						if (!cl->accountName[0] || Q_stricmp(cl->accountName, barVote->accountName))
							continue;

						// this guy is in the currently active pug proposal
						shouldStartNewProposal = qtrue;
						break;
					}
				}

				// is he on a team?
				if (!shouldStartNewProposal) {
					for (int i = 0; i < MAX_CLIENTS; i++) {
						gentity_t *testGuy = &g_entities[i];
						if (!testGuy->inuse || !testGuy->client || testGuy->client->pers.connected != CON_CONNECTED)
							continue;
						if (!testGuy->client->account || Q_stricmp(testGuy->client->account->name, barVote->accountName))
							continue;
						if (IsRacerOrSpectator(testGuy))
							continue;

						// this guy is connected and on a team
						shouldStartNewProposal = qtrue;
						break;
					}
				}

				// however, if 12+ connected and 6+ ingame with pos, don't auto start
				if (shouldStartNewProposal && g_vote_teamgen_barVoteDoesntStartNewPugIfManyPlayers.integer) {
					// count number of players ingame with pos
					int numIngameWithPos = 0;
					for (int i = 0; i < MAX_CLIENTS; i++) {
						gentity_t *ingameWithPos = &g_entities[i];
						if (!ingameWithPos->client || ingameWithPos->client->pers.connected != CON_CONNECTED)
							continue;
						if (IsRacerOrSpectator(ingameWithPos) || GetRemindedPosOrDeterminedPos(ingameWithPos) == CTFPOSITION_UNKNOWN)
							continue;
						++numIngameWithPos;
					}

					// count number of pickable players
					int numPickable = 0;
					for (int i = 0; i < MAX_CLIENTS; i++) {
						gentity_t *pickable = &g_entities[i];
						if (!pickable->client || pickable->client->pers.connected != CON_CONNECTED || !pickable->client->account)
							continue;
						if (pickable->client->pers.hasSpecName)
							continue;
						if (TeamGenerator_PlayerIsBarredFromTeamGenerator(pickable))
							continue;
						++numPickable;
					}

					if (numIngameWithPos >= 6 && numPickable >= 12) {
						shouldStartNewProposal = qfalse;
					}
				}

				// if either was true, start and pass new proposal
				if (shouldStartNewProposal) {
					Com_Printf("Attempting to automatically start pug due to passed bar vote.\n");
					TeamGenerator_QueueServerMessageInChat(-1, va("%s Automatically passing a new pug proposal.", announcementBuf));
					ListRemove(&level.barVoteList, barVote);
					trap_SendConsoleCommand(EXEC_APPEND, "pug startpass\n");
				}
				else {
					Com_Printf("NOT attempting to automatically start pug due to passed bar vote.\n");
					TeamGenerator_QueueServerMessageInChat(-1, announcementBuf);
					ListRemove(&level.barVoteList, barVote);
				}
			}
		}
		else {
			TeamGenerator_QueueServerMessageInChat(-1, announcementBuf);
			ListRemove(&level.barVoteList, barVote);
		}
	}

	return qfalse;
}

qboolean TeamGenerator_VoteToUnbar(gentity_t *ent, const char *voteStr, char **newMessage) {
	assert(ent && ent->client);

	if (!g_vote_teamgen_enableBarVote.integer) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Bar vote is disabled.");
		return qtrue;
	}

	if (!ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You do not have an account, so you cannot vote to bar players. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(ent->client->pers.netname, "elo BOT")) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have either modded elo bot to cheese the system or this is a bug which you should report.");
		return qtrue;
	}

	if (!VALIDSTRING(voteStr) || strlen(voteStr) < 7) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Usage: %cunbar [account name]", TEAMGEN_CHAT_COMMAND_CHARACTER));
		return qtrue;
	}

	if (IsRacerOrSpectator(ent) && IsSpecName(ent->client->pers.netname)) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You are not pickable, so you cannot vote to unbar.");
		return qtrue;
	}

	const char *accountName = voteStr + 6;
	accountReference_t acc = G_GetAccountByName(accountName, qtrue);
	if (!acc.ptr) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("No connected player found for account '%s'. Make sure to use account name from /whois, not ingame name.", accountName));
		return qtrue;
	}

	gentity_t *someoneConnectedOnThisAccount = NULL;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *thisEnt = &g_entities[i];
		if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected == CON_DISCONNECTED || thisEnt->client->account != acc.ptr)
			continue;
		if ((acc.ptr->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(thisEnt->client->pers.netname, "elo BOT"))
			continue;
		someoneConnectedOnThisAccount = thisEnt;
		break;
		// we don't need to check for multiple entities since each one should be barred if any are barred
	}
	if (!someoneConnectedOnThisAccount) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("No connected player found for account '%s'. Make sure to use account name from /whois, not ingame name.", accountName));
		return qtrue;
	}

	// already barred?
	barReason_t barReason = TeamGenerator_PlayerIsBarredFromTeamGenerator(someoneConnectedOnThisAccount);
	if (!barReason) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("%s is not barred.", acc.ptr->name));
		return qtrue;
	}
	else if (barReason != BARREASON_BARREDBYVOTE) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("%s was not barred by vote, so they cannot be unbarred by vote.", acc.ptr->name));
		return qtrue;
	}

	// make sure at least 5 people are connected
	int numEligible = 0;
	{
		qboolean gotEm[MAX_CLIENTS] = { qfalse };
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected == CON_DISCONNECTED ||
				!thisEnt->client->account || thisEnt->client->sess.clientType != CLIENT_TYPE_NORMAL || gotEm[i])
				continue;

			if (thisEnt->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(thisEnt->client->pers.netname, "elo BOT"))
				continue;

			++numEligible;

			qboolean votedToBar = qfalse;
			for (int k = 0; k < MAX_CLIENTS; k++) {
				gentity_t *checkEnt = &g_entities[k];
				if (!checkEnt->inuse || !checkEnt->client || checkEnt->client->pers.connected == CON_DISCONNECTED ||
					!checkEnt->client->account || thisEnt->client->sess.clientType != CLIENT_TYPE_NORMAL || checkEnt->client->account->id != thisEnt->client->account->id)
					continue;
				gotEm[k] = qtrue;
			}
		}
	}

	const int numRequired = g_vote_teamgen_enableBarVote.integer;
	const int numRequiredToBeConnected = numRequired;
	if (numEligible < numRequiredToBeConnected) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("At least %d players must be connected to vote to unbar someone.", numRequiredToBeConnected));
		return qtrue;
	}

	// check whether this bar vote already exists
	qboolean doVote = qtrue;
	barVote_t *barVote = ListFind(&level.unbarVoteList, BarVoteMatchesAccountId, &acc.ptr->id, NULL);
	if (barVote) {
		// this vote already exists; see if we already voted on it
		qboolean votedToBarOnAnotherClient = qfalse;
		if (!(barVote->votedYesClients & (1 << ent - g_entities))) {
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *other = &g_entities[i];
				if (other == ent || !other->inuse || !other->client || !other->client->account)
					continue;
				if (other->client->account->id != ent->client->account->id)
					continue;
				if (barVote->votedYesClients & (1 << other - g_entities)) {
					votedToBarOnAnotherClient = qtrue;
					break;
				}
			}
		}

		if (votedToBarOnAnotherClient || barVote->votedYesClients & (1 << ent - g_entities)) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("You have already voted to unbar %s.", barVote->accountName));
			doVote = qfalse;
		}
	}
	else {
		// doesn't exist yet; create it
		barVote = ListAdd(&level.unbarVoteList, sizeof(barVote_t));
		barVote->accountId = acc.ptr->id;
		Q_strncpyz(barVote->accountName, acc.ptr->name, sizeof(barVote->accountName));
	}

	if (doVote)
		barVote->votedYesClients |= (1 << (ent - g_entities));

	// check how many votes we are now up to
	int numBarVotesFromEligiblePlayers = 0;
	{
		qboolean gotEm[MAX_CLIENTS] = { qfalse };
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected == CON_DISCONNECTED ||
				!thisEnt->client->account || thisEnt->client->sess.clientType != CLIENT_TYPE_NORMAL || gotEm[i])
				continue;

			if (thisEnt->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(thisEnt->client->pers.netname, "elo BOT"))
				continue;

			qboolean votedToBar = qfalse;
			for (int k = 0; k < MAX_CLIENTS; k++) {
				gentity_t *checkEnt = &g_entities[k];
				if (!checkEnt->inuse || !checkEnt->client || checkEnt->client->pers.connected == CON_DISCONNECTED ||
					!checkEnt->client->account || thisEnt->client->sess.clientType != CLIENT_TYPE_NORMAL || checkEnt->client->account->id != thisEnt->client->account->id)
					continue;
				if (barVote->votedYesClients & (1 << k))
					votedToBar = qtrue;
				gotEm[k] = qtrue;
			}

			if (votedToBar)
				++numBarVotesFromEligiblePlayers;
		}
	}

	// print the message
	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%cunbar %s   ^%c(%d/%d)",
			TEAMGEN_CHAT_COMMAND_CHARACTER, barVote->accountName, doVote ? '7' : '9', numBarVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	// if we have enough votes, do the barring
	if (numBarVotesFromEligiblePlayers >= numRequired) {
		iterator_t iter;
		ListIterate(&level.barredPlayersList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
			if (bp->accountId == barVote->accountId) {
				ListRemove(&level.barredPlayersList, bp);
			}
			ListIterate(&level.barredPlayersList, &iter, qfalse);
		}

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (!g_entities[i].inuse || !g_entities[i].client || !g_entities[i].client->account || g_entities[i].client->account->id != barVote->accountId)
				continue;
			g_entities[i].client->pers.barredFromPugSelection = BARREASON_NOTBARRED;
			ClientUserinfoChanged(i);
		}

		TeamGenerator_QueueServerMessageInChat(-1, va("%s^7 was unbarred from team generation by vote.", barVote->accountName));
		ListRemove(&level.unbarVoteList, barVote);
	}

	return qfalse;
}

void TeamGenerator_DoCancel(void) {
	TeamGenerator_QueueServerMessageInChat(-1, va("Active pug proposal %d canceled (%s).", level.activePugProposal->num, level.activePugProposal->namesStr));
	ListClear(&level.activePugProposal->avoidedHashesList);
	ListRemove(&level.pugProposalsList, level.activePugProposal);
	level.activePugProposal = NULL;
}

qboolean TeamGenerator_VoteToCancel(gentity_t *ent, char **newMessage) {
	assert(ent && ent->client);

	if (!ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You do not have an account, so you cannot vote for pug proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(ent->client->pers.netname, "elo BOT")) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have either modded elo bot to cheese the system or this is a bug which you should report.");
		return qtrue;
	}

	if (!level.activePugProposal) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "No pug proposal is currently active.");
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
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote to cancel because you are not part of this pug proposal.");
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

	qboolean doVote = qtrue;
	if (votedToCancelOnAnotherClient || level.activePugProposal->votedToCancelClients & (1 << ent - g_entities)) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have already voted to cancel the teams proposals.");
		doVote = qfalse;
	}

	if (doVote) {
		Com_Printf("%s^7 voted to cancel active pug proposal %d.\n", ent->client->pers.netname, level.activePugProposal->num);
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Vote cast to cancel pug proposal %d.", level.activePugProposal->num));
		level.activePugProposal->votedToCancelClients |= (1 << (ent - g_entities));
	}

	int numEligibleConnected = 0, numEligibleMax = 0, numCancelVotesFromEligiblePlayers = 0;
	qboolean gotEm[MAX_CLIENTS] = { qfalse };
	for (int i = 0; i < MAX_CLIENTS; i++) {
		sortedClient_t *cl = &level.activePugProposal->clients[i];
		if (!cl->accountName[0])
			continue; // not included in this set

		++numEligibleMax;

		// is this person actually connected?
		for (int j = 0; j < MAX_CLIENTS; j++) {
			gentity_t *other = &g_entities[j];
			if (!other->inuse || !other->client || !other->client->account || other->client->account->id != cl->accountId || gotEm[j])
				continue;

			// yes, they are connected
			++numEligibleConnected;

			// are they connected on any other clients? see if any of them have voted yes, and mark the duplicates so we know not to check them
			qboolean votedToCancel = qfalse;
			for (int k = 0; k < MAX_CLIENTS; k++) {
				gentity_t *thirdEnt = &g_entities[k];
				if (!thirdEnt->inuse || !thirdEnt->client || !thirdEnt->client->account || thirdEnt->client->account->id != cl->accountId)
					continue;
				if (level.activePugProposal->votedToCancelClients & (1 << k))
					votedToCancel = qtrue;
				gotEm[k] = qtrue;
			}

			if (votedToCancel)
				++numCancelVotesFromEligiblePlayers;

			break;
		}
	}

	// require a simple majority of the total people in the proposal. requiring only 4 or 5 would allow e.g. in a set of 15 people for just a few people who were excluded to cancel it for everyone else
	if (numEligibleConnected > numEligibleMax)
		numEligibleConnected = numEligibleMax; // sanity check
	const int numRequired = (numEligibleConnected / 2) + 1;

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%ccancel   ^%c(%d/%d)", TEAMGEN_CHAT_COMMAND_CHARACTER, doVote ? '7' : '9', numCancelVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	if (doVote && numCancelVotesFromEligiblePlayers >= numRequired)
		TeamGenerator_DoCancel();

	return qfalse;
}

void TeamGenerator_PrintPlayersInPugProposals(gentity_t *ent) {
	assert(ent);

	qboolean gotOne = qfalse;
	iterator_t iter;
	ListIterate(&level.pugProposalsList, &iter, qfalse);
	char buf[4096] = { 0 };
	while (IteratorHasNext(&iter)) {
		pugProposal_t *p = IteratorNext(&iter);
		if (!gotOne)
			Q_strncpyz(buf, "Current pug proposals:\n", sizeof(buf));
		Q_strcat(buf, sizeof(buf), va("Pug proposal ^5%d^7 - %s%s\n", p->num, p->namesStr, p == level.activePugProposal ? " ^2(Currently active)^7" : ""));
		gotOne = qtrue;
	}

	if (gotOne) {
		TeamGenerator_QueueServerMessageInConsole(ent - g_entities, buf);
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Check console for a list of players in each pug proposal.");
	}
	else {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "There are currently no pug proposals.");
	}
}

static qboolean TeamGenerator_PermabarredPlayerMarkAsPickable(gentity_t *ent, char **newMessage) {
	assert(ent);
	if (!ent->client->account) {
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(ent->client->pers.netname, "elo BOT")) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have either modded elo bot to cheese the system or this is a bug which you should report.");
		return qtrue;
	}

	if (!(ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) && !(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) && !(ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL)) {
		return qtrue;
	}

	if (ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You are unable to set yourself as pickable. Please contact an admin.");
		return qtrue;
	}

	if (ent->client->pers.permaBarredDeclaredPickable) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have already set yourself as pickable.");
		return qtrue;
	}

	if (newMessage) {
		static char *idiotMsg = "I am pickable. I apologize for my inconsiderate habit of not spec naming. I also cut in line at grocery stores and piss all over the toilet at work.";
		static char *chadMsg = "I am pickable.";

		if (ent->client->account->flags & ACCOUNTFLAG_ADMIN)
			*newMessage = chadMsg;
		else
			*newMessage = idiotMsg;
	}

	ent->client->pers.permaBarredDeclaredPickable = qtrue;
	ClientUserinfoChanged(ent - g_entities);

	return qfalse;
}

// returns qtrue if the message should be filtered out
qboolean TeamGenerator_CheckForChatCommand(gentity_t *ent, const char *s, char **newMessage) {
	if (g_gametype.integer != GT_CTF)
		return qfalse;

	if (!g_vote_teamgen.integer) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Team generator is disabled.");
		return qtrue;
	}

	if (!ent || !ent->client || ent->client->pers.connected != CON_CONNECTED || !VALIDSTRING(s)) {
		assert(qfalse);
		return qfalse;
	}


	if (!Q_stricmp(s, "help")) {
		if (ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_PERMABARRED || ent->client->account->flags & ACCOUNTFLAG_LSAFKTROLL) && !(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED)) {
			PrintIngame(ent - g_entities,
				"*Chat commands:\n"
				"^7%cstart             - propose playing a pug with current non-spec players\n"
				"^9%c[number]          - vote to approve a pug proposal\n"
				"^7%c[ a | b | c | d ] - vote to approve one or more teams proposals\n"
				"^9%creroll            - vote to generate new teams proposals with the same players\n"
				"%s"
				"^7%ccancel            - vote to generate new teams proposals with the same players\n"
				"^9%clist              - show which players are part of each pug proposal\n"
				"^7%cpickable          - declare yourself as pickable for pugs\n"
				, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, g_vote_teamgen_enableBarVote.integer ? va("^7%cbar [player]      - vote to bar a player\n^9%cunbar [player]    - vote to unbar a player\n", TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER) : "", TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER);
		}
		else {
			PrintIngame(ent - g_entities,
				"*Chat commands:\n"
				"^7%cstart             - propose playing a pug with current non-spec players\n"
				"^9%c[number]          - vote to approve a pug proposal\n"
				"^7%c[ a | b | c | d ] - vote to approve one or more teams proposals\n"
				"^9%creroll            - vote to generate new teams proposals with the same players\n"
				"%s"
				"^7%ccancel            - vote to generate new teams proposals with the same players\n"
				"^9%clist              - show which players are part of each pug proposal^7\n"
				, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, g_vote_teamgen_enableBarVote.integer ? va("^7%cbar [player]      - vote to bar a player\n^9%cunbar [player]    - vote to unbar a player\n", TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER) : "", TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER);
		}
		SV_Tell(ent - g_entities, "See console for chat command help.");
		return qtrue;
	}

	if (!Q_stricmp(s, "start") || !Q_stricmp(s, "s"))
		return TeamGenerator_PugStart(ent, newMessage);

	if (!Q_stricmp(s, "reroll") || !Q_stricmp(s, "r"))
		return TeamGenerator_VoteToReroll(ent, newMessage);

	if (!Q_stricmpn(s, "bar", 3) && (strlen(s) <= 3 || isspace(*(s + 3))))
		return TeamGenerator_VoteToBar(ent, s, newMessage);

	if (!Q_stricmpn(s, "unbar", 5) && (strlen(s) <= 5 || isspace(*(s + 5))))
		return TeamGenerator_VoteToUnbar(ent, s, newMessage);

	if (!Q_stricmp(s, "cancel"))
		return TeamGenerator_VoteToCancel(ent, newMessage);

	if (!Q_stricmp(s, "list")) {
		TeamGenerator_PrintPlayersInPugProposals(ent);
		return qtrue;
	}

	if (!Q_stricmpn(s, "fuck", 4) && (strlen(s) <= 4 || isspace(*(s + 4))))
		return TeamGenerator_MemeFuckVote(ent, s, newMessage);

	if (!Q_stricmpn(s, "go", 2) && (strlen(s) <= 2 || isspace(*(s + 2))))
		return TeamGenerator_MemeGoVote(ent, s, newMessage);

	if (!Q_stricmp(s, "pickable"))
		return TeamGenerator_PermabarredPlayerMarkAsPickable(ent, newMessage);

	if (strlen(s) <= 4) {
		qboolean invalidVote = qfalse;
		for (const char *p = s; *p; p++) {
			char lowered = tolower((unsigned)*p);
			if (lowered != 'a' && lowered != 'b' && lowered != 'c' && lowered != 'd') {
				invalidVote = qtrue;
				break;
			}
			if (p > s && lowered == tolower((unsigned)*(p - 1))) {
				invalidVote = qtrue; // aaa or something
				break;
			}
		}
		if (!invalidVote)
			return TeamGenerator_VoteForTeamPermutations(ent, s, newMessage);
	}

	if (!Q_stricmpn(s, "un", 2) && strlen(s) > 2) {
		qboolean invalidVote = qfalse;
		for (const char *p = s + 2; *p; p++) {
			char lowered = tolower((unsigned)*p);
			if (lowered != 'a' && lowered != 'b' && lowered != 'c' && lowered != 'd') {
				invalidVote = qtrue;
				break;
			}
			if (p > (s + 2) && lowered == tolower((unsigned)*(p - 1))) {
				invalidVote = qtrue; // aaa or something
				break;
			}
		}
		if (!invalidVote)
			return TeamGenerator_UnvoteForTeamPermutations(ent, s + 2, newMessage, s);
	}
	else if (!Q_stricmpn(s, "hun", 3) && strlen(s) > 3) {
		qboolean invalidVote = qfalse;
		for (const char *p = s + 3; *p; p++) {
			char lowered = tolower((unsigned)*p);
			if (lowered != 'a' && lowered != 'b' && lowered != 'c' && lowered != 'd') {
				invalidVote = qtrue;
				break;
			}
			if (p > (s + 3) && lowered == tolower((unsigned)*(p - 1))) {
				invalidVote = qtrue; // aaa or something
				break;
			}
		}
		if (!invalidVote)
			return TeamGenerator_UnvoteForTeamPermutations(ent, s + 3, newMessage, s);
	}

	if (atoi(s))
		return TeamGenerator_VoteYesToPugProposal(ent, atoi(s), NULL, newMessage);

	return qfalse;
}

static void TeamGenerator_ServerCommand_Start(qboolean forcePass) {
	sortedClient_t clients[MAX_CLIENTS] = { 0 };
	int numEligible = 0, numIngameEligible = 0;
	GetCurrentPickablePlayers(&clients[0], &numEligible, &numIngameEligible, qfalse);

	if (numEligible < 8) {
		Com_Printf("Not enough eligible players.\n");
		return;
	}

	XXH32_hash_t hash = HashPugProposal(&clients[0]);
	pugProposal_t *set = ListFind(&level.pugProposalsList, PugProposalMatchesHash, &hash, NULL);
	if (set) {
		Com_Printf("A pug with the current pickable players has already been proposed.\n");
		return;
	}

	set = ListAdd(&level.pugProposalsList, sizeof(pugProposal_t));
	memcpy(set->clients, &clients, sizeof(set->clients));
	set->hash = hash;
	set->num = ++lastNum;

	char *namesStr = GetNamesStringForPugProposal(set);
	if (VALIDSTRING(namesStr))
		Q_strncpyz(set->namesStr, namesStr, sizeof(set->namesStr));

	if (!forcePass)
		TeamGenerator_QueueServerMessageInChat(-1, va("Server proposes pug with: %s. Enter ^5%c%d^7 in chat if you approve.", namesStr, TEAMGEN_CHAT_COMMAND_CHARACTER, set->num));
	else
		ActivatePugProposal(set, qtrue);

	return;
}

static void PrintPugHelp(void) {
	Com_Printf("^7Usage:\n"
		"^7pug start                 - start a pug proposal with current pickable players\n"
		"^9pug pass [num]            - pass a non-active pug proposal (causing it to become active)\n"
		"^7pug startpass             - start and immediately pass a pug proposal\n"
		"^9pug kill [num]            - kill a non-active pug proposal\n"
		"^7pug reroll                - reroll a teams proposal\n"
		"^9pug cancel                - cancel the active pug proposal\n"
		"^7pug bar [player name/#]   - bars a player from team generation for the rest of the current round\n"
		"^9pug unbar [player name/#] - unbars a player\n"
		"^7pug list                  - list the players in each pug proposal\n"
		"You can see a list of vote-related cvars by entering ^5cvarlist g_vote^7\n"
		"%s",
		!g_vote_teamgen.integer ? "^3Note: team generator is currently disabled! Enable with g_vote_teamgen.^7\n" : ""
	);
}

void Svcmd_Pug_f(void) {
	if (trap_Argc() < 2) {
		PrintPugHelp();
		return;
	}

	char arg1[MAX_STRING_CHARS] = { 0 }, arg2[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, arg1, sizeof(arg1));
	trap_Argv(2, arg2, sizeof(arg2));
	if (!arg1[0]) {
		PrintPugHelp();
		return;
	}

	if (!Q_stricmp(arg1, "start")) {
		TeamGenerator_ServerCommand_Start(qfalse);
	}
	else if (!Q_stricmp(arg1, "pass")) {
		if (!arg2[0] || !atoi(arg2)) {
			Com_Printf("Usage: pug pass [num]\n");
			return;
		}

		int num = atoi(arg2);
		pugProposal_t *found = ListFind(&level.pugProposalsList, PugProposalMatchesNum, &num, NULL);
		if (!found) {
			Com_Printf("Unable to find pug proposal matching number %d.\n", num);
			return;
		}

		ActivatePugProposal(found, qtrue);
	}
	else if (!Q_stricmp(arg1, "startpass")) {
		TeamGenerator_ServerCommand_Start(qtrue);
	}
	else if (!Q_stricmp(arg1, "kill")) {
		if (!arg2[0] || !atoi(arg2)) {
			if (level.activePugProposal)
				Com_Printf("Usage: pug kill [num]   (use ^5pug cancel^7 to stop the currently active pug proposal)\n");
			else
				Com_Printf("Usage: pug kill [num]\n");
			return;
		}

		int num = atoi(arg2);
		pugProposal_t *found = ListFind(&level.pugProposalsList, PugProposalMatchesNum, &num, NULL);
		if (!found) {
			Com_Printf("Unable to find pug proposal matching number %d.\n", num);
			return;
		}

		if (found == level.activePugProposal) {
			level.activePugProposal = NULL;
			TeamGenerator_QueueServerMessageInChat(-1, va("Active pug proposal %d canceled by server (%s).", found->num, found->namesStr));
		}
		else {
			TeamGenerator_QueueServerMessageInChat(-1, va("Inactive pug proposal %d killed by server (%s).", found->num, found->namesStr));
		}
		ListClear(&found->avoidedHashesList);
		ListRemove(&level.pugProposalsList, found);
	}
	else if (!Q_stricmp(arg1, "reroll")) {
		if (!level.activePugProposal) {
			Com_Printf("No pug proposal is currently active.\n");
			return;
		}
		TeamGenerator_DoReroll(qtrue);
	}
	else if (!Q_stricmp(arg1, "cancel")) {
		if (!level.activePugProposal) {
			Com_Printf("No pug proposal is currently active.\n");
			return;
		}
		TeamGenerator_QueueServerMessageInChat(-1, va("Active pug proposal %d canceled by server (%s).", level.activePugProposal->num, level.activePugProposal->namesStr));
		ListClear(&level.activePugProposal->avoidedHashesList);
		ListRemove(&level.pugProposalsList, level.activePugProposal);
		level.activePugProposal = NULL;
	}
	else if (!Q_stricmp(arg1, "bar")) {
		if (!arg2[0]) {
			Com_Printf("Usage: pug bar [player name/#]\n");
			return;
		}

		gentity_t *found = G_FindClient(arg2);
		if (!found || !found->client) {
			Com_Printf("Client %s^7 not found or ambiguous. Use client number or be more specific.\n", arg2);
			return;
		}

		char *name = found->client->account ? found->client->account->name : found->client->pers.netname;

		if (found->client->account && ((found->client->account->flags & ACCOUNTFLAG_PERMABARRED) || (found->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) || (found->client->account->flags & ACCOUNTFLAG_LSAFKTROLL))) {
			qboolean wasForcedPickable = qfalse;
			iterator_t iter;
			ListIterate(&level.forcedPickablePermabarredPlayersList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
				if (bp->accountId == found->client->account->id) {
					wasForcedPickable = qtrue;
					break;
				}
			}

			if (wasForcedPickable) {
				Com_Printf("%s^7 is %spermabarred, but had been temporarily forced to be pickable via pug unbar. They are no longer forced to be pickable.\n",
					name, (found->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) ? "hard " : "");
				TeamGenerator_QueueServerMessageInChat(-1, va("%s^7 is no longer temporarily forced to be pickable for team generation by server.", name));
				ListIterate(&level.forcedPickablePermabarredPlayersList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
					if (bp->accountId == found->client->account->id) {
						ListRemove(&level.forcedPickablePermabarredPlayersList, bp);
						ListIterate(&level.forcedPickablePermabarredPlayersList, &iter, qfalse);
					}
				}
				for (int i = 0; i < MAX_CLIENTS; i++) {
					if (g_entities[i].inuse && g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED && g_entities[i].client->account && g_entities[i].client->account->id == found->client->account->id)
						ClientUserinfoChanged(i);
				}
			}
			else {
				Com_Printf("%s^7 is %spermabarred. You can temporarily force them to be pickable by using pug unbar.\n",
					name, (found->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) ? "hard " : "");
			}
			return;
		}

		if (found->client->pers.barredFromPugSelection) {
			qboolean printed = qfalse;
			if (found->client->account) {
				qboolean foundInList = qfalse;
				iterator_t iter;
				ListIterate(&level.barredPlayersList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
					if (bp->accountId == found->client->account->id) {
						foundInList = qtrue;
						if (bp->reason == BARREASON_BARREDBYVOTE) {
							bp->reason = BARREASON_BARREDBYADMIN;
							Com_Printf("%s^7 was already barred by vote; they are now barred by admin (so their bar cannot be removed by vote).\n", name);
							printed = qtrue;
						}
						break;
					}
				}

				if (!foundInList) { // barred in client->pers but not list; add them to list
					barredOrForcedPickablePlayer_t *add = ListAdd(&level.barredPlayersList, sizeof(barredOrForcedPickablePlayer_t));
					add->accountId = found->client->account->id;
					add->reason = BARREASON_BARREDBYADMIN;
				}
			}
			if (!printed)
				Com_Printf("%s^7 is already barred.\n", name);
			return;
		}

		found->client->pers.barredFromPugSelection = BARREASON_BARREDBYADMIN;
		if (found->client->account) {
			Com_Printf("Barred player %s^7.\n", name);
			barredOrForcedPickablePlayer_t *add = ListAdd(&level.barredPlayersList, sizeof(barredOrForcedPickablePlayer_t));
			add->accountId = found->client->account->id;
			add->reason = BARREASON_BARREDBYADMIN;
		}
		else {
			Com_Printf("Barred player %s^7. Since they do not have an account, they will need to be barred again if they reconnect to circumvent the bar before the current map ends.\n", name);
		}

		ClientUserinfoChanged(found - g_entities);

		TeamGenerator_QueueServerMessageInChat(-1, va("%s^7 was barred from team generation by admin.", name));
	}
	else if (!Q_stricmp(arg1, "unbar")) {
		if (!arg2[0]) {
			Com_Printf("Usage: pug unbar [player name/#]\n");
			return;
		}

		gentity_t *found = G_FindClient(arg2);
		if (!found || !found->client) {
			Com_Printf("Client %s^7 not found or ambiguous. Use client number or be more specific.\n", arg2);
			return;
		}

		char *name = found->client->account ? found->client->account->name : found->client->pers.netname;

		if (found->client->account && ((found->client->account->flags & ACCOUNTFLAG_PERMABARRED) || (found->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) || (found->client->account->flags & ACCOUNTFLAG_LSAFKTROLL))) {
			qboolean foundForcedPickable = qfalse;
			iterator_t iter;
			ListIterate(&level.forcedPickablePermabarredPlayersList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
				if (bp->accountId == found->client->account->id) {
					foundForcedPickable = qtrue;
					break;
				}
			}

			if (foundForcedPickable) {
				Com_Printf("%s^7 is %spermabarred, but had been temporarily forced to be pickable via pug unbar. You can remove the temporary pickability by using pug bar.\n",
					name, (found->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) ? "hard " : "");
			}
			else {
				Com_Printf("%s^7 is %spermabarred. They are now temporarily forced to be pickable.\n",
					name, (found->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) ? "hard " : "");
				TeamGenerator_QueueServerMessageInChat(-1, va("%s^7 temporarily forced to be pickable for team generation by server.", name));
				barredOrForcedPickablePlayer_t *add = ListAdd(&level.forcedPickablePermabarredPlayersList, sizeof(barredOrForcedPickablePlayer_t));
				add->accountId = found->client->account->id;

				for (int i = 0; i < MAX_CLIENTS; i++) {
					if (g_entities[i].inuse && g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED && g_entities[i].client->account && g_entities[i].client->account->id == found->client->account->id)
						ClientUserinfoChanged(i);
				}
			}
			return;
		}

		if (found->client->account) { // remove from list, if applicable
			iterator_t iter;
			ListIterate(&level.barredPlayersList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
				if (bp->accountId == found->client->account->id) {
					ListRemove(&level.barredPlayersList, bp);
					ListIterate(&level.barredPlayersList, &iter, qfalse);
				}
			}
		}

		if (!found->client->pers.barredFromPugSelection) {
			Com_Printf("%s^7 is already not barred.\n", name);
			return;
		}

		found->client->pers.barredFromPugSelection = BARREASON_NOTBARRED;
		ClientUserinfoChanged(found - g_entities);

		TeamGenerator_QueueServerMessageInChat(-1, va("%s^7 unbarred from team generation by server.", name));
	}
	else if (!Q_stricmp(arg1, "list")) {
		qboolean gotOne = qfalse;
		iterator_t iter;
		ListIterate(&level.pugProposalsList, &iter, qfalse);
		char buf[4096] = { 0 };
		while (IteratorHasNext(&iter)) {
			pugProposal_t *p = IteratorNext(&iter);
			if (!gotOne)
				Q_strncpyz(buf, "Current pug proposals:\n", sizeof(buf));
			Q_strcat(buf, sizeof(buf), va("Pug proposal ^5%d^7 - %s%s\n", p->num, p->namesStr, p == level.activePugProposal ? " ^2(Currently active)^7" : ""));
			gotOne = qtrue;
		}

		if (gotOne)
			Com_Printf(buf);
		else
			Com_Printf("There are currently no pug proposals.\n");
	}
	else {
		PrintPugHelp();
	}
}

ctfPlayerTier_t GetPlayerTierForPlayerOnPosition(int accountId, ctfPosition_t pos, qboolean assumeLowTierIfUnrated) {
	if (accountId == ACCOUNT_ID_UNLINKED || pos < CTFPOSITION_BASE || pos > CTFPOSITION_OFFENSE) {
		assert(qfalse);
		return PLAYERRATING_UNRATED;
	}

	playerRating_t findMe = { 0 };
	findMe.accountId = accountId;
	playerRating_t *positionRatings = ListFind(&level.ratingList, PlayerRatingAccountIdMatches, &findMe, NULL);
	if (!positionRatings)
		return assumeLowTierIfUnrated ? PLAYERRATING_LOW_C : PLAYERRATING_UNRATED;
	
	ctfPlayerTier_t rating = PlayerTierFromRating(positionRatings->rating[pos]);
	if (!rating && assumeLowTierIfUnrated)
		return PLAYERRATING_LOW_C;
	return rating;
}

void ShowSubBalance(void) {
	if (!g_vote_teamgen_subhelp.integer || !level.wasRestarted || level.someoneWasAFK || (level.time - level.startTime) < (CTFPOSITION_MINIMUM_SECONDS * 1000) || !level.numTeamTicks || level.pause.state == PAUSE_NONE)
		return;

	if (IsLivePug(0) != 4)
		return;

	int missingPositions = 8, numRacersOrSpectators = 0, numRed = 0, numBlue = 0;
	int base[4], chase[4], offense1[4], offense2[4];
	memset(base, -1, sizeof(base));
	memset(chase, -1, sizeof(chase));
	memset(offense1, -1, sizeof(offense1));
	memset(offense2, -1, sizeof(offense2));
	int newJoiner = -1;
	qboolean gotPlayer[MAX_CLIENTS] = { qfalse };
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || !ent->client->account)
			continue;
		if (IsRacerOrSpectator(ent)) {
			++numRacersOrSpectators;
			continue;
		}

		if (ent->client->sess.sessionTeam == TEAM_RED)
			++numRed;
		else if (ent->client->sess.sessionTeam == TEAM_BLUE)
			++numBlue;

		if (!ent->client->stats->lastTickIngameTime || level.lastPlayerTickAddedTime - ent->client->stats->lastTickIngameTime >= 30000) {
			if (newJoiner != -1)
				return; // we only care if there is exactly one joiner
			newJoiner = i;
			continue; // this player wasn't ingame while not paused in the last 30 seconds
		}

		int team = ent->client->sess.sessionTeam;
		int pos = DetermineCTFPosition(ent->client->stats, qfalse);
		if (pos == CTFPOSITION_BASE && base[team] == -1) {
			base[team] = ent->client->account->id;
			--missingPositions;
			gotPlayer[i] = qtrue;
		}
		else if (pos == CTFPOSITION_CHASE && chase[team] == -1) {
			chase[team] = ent->client->account->id;
			--missingPositions;
			gotPlayer[i] = qtrue;
		}
		else if (pos == CTFPOSITION_OFFENSE) {
			if (offense1[team] == -1) {
				offense1[team] = ent->client->account->id;
				--missingPositions;
				gotPlayer[i] = qtrue;
			}
			else if (offense2[team] == -1) {
				offense2[team] = ent->client->account->id;
				--missingPositions;
				gotPlayer[i] = qtrue;
			}
		}
	}

	if (missingPositions != 1 || newJoiner == -1 || numRed != 4 || numBlue != 4)
		return;

	gentity_t *joiner = &g_entities[newJoiner];
	int joinerTeam = joiner->client->sess.sessionTeam;
	int otherTeam = OtherTeam(joinerTeam);
	int missingPos;
	if (base[joinerTeam] == -1) {
		missingPos = CTFPOSITION_BASE;
		base[joinerTeam] = joiner->client->account->id;
	}
	else if (chase[joinerTeam] == -1) {
		missingPos = CTFPOSITION_CHASE;
		chase[joinerTeam] = joiner->client->account->id;
	}
	else if (offense1[joinerTeam] == -1) {
		missingPos = CTFPOSITION_OFFENSE;
		offense1[joinerTeam] = joiner->client->account->id;
	}
	else if (offense2[joinerTeam] == -1) {
		missingPos = CTFPOSITION_OFFENSE;
		offense2[joinerTeam] = joiner->client->account->id;
	}
	else {
		assert(qfalse);
		return;
	}

	G_DBGetPlayerRatings();

	double teamTotal[4] = { 0 };
	for (int t = TEAM_RED; t <= TEAM_BLUE; t++) {
		teamTotal[t] += PlayerTierToRating(GetPlayerTierForPlayerOnPosition(base[t], CTFPOSITION_BASE, qtrue));
		teamTotal[t] += PlayerTierToRating(GetPlayerTierForPlayerOnPosition(chase[t], CTFPOSITION_CHASE, qtrue));
		teamTotal[t] += PlayerTierToRating(GetPlayerTierForPlayerOnPosition(offense1[t], CTFPOSITION_OFFENSE, qtrue));
		teamTotal[t] += PlayerTierToRating(GetPlayerTierForPlayerOnPosition(offense2[t], CTFPOSITION_OFFENSE, qtrue));
	}
	double totalOfBothTeams = teamTotal[TEAM_RED] + teamTotal[TEAM_BLUE];
	if (!totalOfBothTeams)
		return; // sanity check, prevent divide by zero

	double relativeStrength[4];
	relativeStrength[TEAM_RED] = teamTotal[TEAM_RED] / totalOfBothTeams;
	relativeStrength[TEAM_BLUE] = teamTotal[TEAM_BLUE] / totalOfBothTeams;
	double diff = fabs(relativeStrength[TEAM_RED] - relativeStrength[TEAM_BLUE]);

	char buf[256] = { 0 };
	Com_sprintf(buf, sizeof(buf), "With ^5%s^7 subbing on ^5%s %s^7, team balance will become ^1%.2f'/.^7 vs. ^4%.2f'/.^7.",
		joiner->client->account->name,
		joinerTeam == TEAM_RED ? "red" : "blue",
		NameForPos(missingPos),
		relativeStrength[TEAM_RED] * 100.0,
		relativeStrength[TEAM_BLUE] * 100.0);

	if (level.lastRelativeStrength[TEAM_RED] && level.lastRelativeStrength[TEAM_BLUE])
		Q_strcat(buf, sizeof(buf), va(" (Balance before subbing was ^1%.2f'/.^7 vs. ^4%.2f'/.^7.)",
			level.lastRelativeStrength[TEAM_RED] * 100.0,
			level.lastRelativeStrength[TEAM_BLUE] * 100.0));

	TeamGenerator_QueueServerMessageInChat(-1, buf);

	if (level.lastRelativeStrength[TEAM_RED] && level.lastRelativeStrength[TEAM_BLUE]) {
		double oldDiff = fabs(level.lastRelativeStrength[TEAM_RED] - level.lastRelativeStrength[TEAM_BLUE]);
		if (diff > oldDiff + 0.00001) {
			TeamGenerator_QueueServerMessageInChat(-1, va("^3Team balance will worsen with this substitution! Consider %sremaking teams with ^5%cstart^3.",
				numRacersOrSpectators ? "substituting another player or " : "", TEAMGEN_CHAT_COMMAND_CHARACTER));
		}
	}
}

qboolean TeamGenerator_PrintBalance(gentity_t *sendTo, gentity_t *sendFrom) {
	if (!level.numTeamTicks || !sendTo || !sendTo->client || !sendFrom || !sendFrom->client)
		return qfalse;

	int numOnTeam[TEAM_NUM_TEAMS] = { 0 };
	int posCount[TEAM_NUM_TEAMS][CTFPOSITION_OFFENSE + 1] = { {0} };
	double teamTotal[TEAM_NUM_TEAMS] = { 0 };

	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || !ent->client->account)
			continue;

		const int team = ent->client->sess.sessionTeam;
		if (team != TEAM_RED && team != TEAM_BLUE)
			continue;

		int pos = GetRemindedPosOrDeterminedPos(ent);
		if (!pos)
			return qfalse;

		++posCount[team][pos];
		teamTotal[team] += PlayerTierToRating(GetPlayerTierForPlayerOnPosition(ent->client->account->id, pos, qtrue));
		++numOnTeam[team];
	}

	if (numOnTeam[TEAM_RED] != 4 || numOnTeam[TEAM_BLUE] != 4)
		return qfalse;

	if (posCount[TEAM_RED][CTFPOSITION_BASE] != 1 || posCount[TEAM_RED][CTFPOSITION_CHASE] != 1 || posCount[TEAM_RED][CTFPOSITION_OFFENSE] != 2 ||
		posCount[TEAM_BLUE][CTFPOSITION_BASE] != 1 || posCount[TEAM_BLUE][CTFPOSITION_CHASE] != 1 || posCount[TEAM_BLUE][CTFPOSITION_OFFENSE] != 2) {
		return qfalse;
	}

	teamTotal[TEAM_RED] = round(teamTotal[TEAM_RED] / 0.05) * 0.05;
	teamTotal[TEAM_BLUE] = round(teamTotal[TEAM_BLUE] / 0.05) * 0.05;

	double totalOfBothTeams = teamTotal[TEAM_RED] + teamTotal[TEAM_BLUE];
	totalOfBothTeams = round(totalOfBothTeams / 0.05) * 0.05;
	if (!totalOfBothTeams)
		return qfalse;

	double relativeStrength[TEAM_NUM_TEAMS];
	relativeStrength[TEAM_RED] = teamTotal[TEAM_RED] / totalOfBothTeams;
	relativeStrength[TEAM_BLUE] = teamTotal[TEAM_BLUE] / totalOfBothTeams;

	double mappedAverageOfBothTeamsStrength;
	{
		double s = PlayerTierToRating(PLAYERRATING_MID_S);
		double d = PlayerTierToRating(PLAYERRATING_MID_D);
		const double sMapped = 3000, dMapped = 900;
		assert(s > d);

		double averageOfBothTeamsStrength = totalOfBothTeams * 0.5;
		if (averageOfBothTeamsStrength >= s * 4) {
			mappedAverageOfBothTeamsStrength = sMapped;
		}
		else if (averageOfBothTeamsStrength <= d * 4) {
			mappedAverageOfBothTeamsStrength = dMapped;
		}
		else {
			double slope = (sMapped - dMapped) / ((s * 4) - (d * 4));
			mappedAverageOfBothTeamsStrength = dMapped + slope * (averageOfBothTeamsStrength - (d * 4));
		}
	}

	int mappedTeamNumber[TEAM_NUM_TEAMS];
	if (fabs(relativeStrength[TEAM_RED] - 0.5) <= 0.00001 && fabs(relativeStrength[TEAM_BLUE] - 0.5) <= 0.00001) {
		mappedTeamNumber[TEAM_RED] = mappedTeamNumber[TEAM_BLUE] = (int)round(mappedAverageOfBothTeamsStrength);
	}
	else {
		mappedTeamNumber[TEAM_RED] = (int)round((mappedAverageOfBothTeamsStrength * 2.0) * relativeStrength[TEAM_RED]);
		mappedTeamNumber[TEAM_BLUE] = (int)round((mappedAverageOfBothTeamsStrength * 2.0) * relativeStrength[TEAM_BLUE]);

		double adjustmentRed = (mappedAverageOfBothTeamsStrength - mappedTeamNumber[TEAM_RED]) / 2.0;
		mappedTeamNumber[TEAM_RED] = (int)round(mappedTeamNumber[TEAM_RED] + adjustmentRed);
		double adjustmentBlue = (mappedAverageOfBothTeamsStrength - mappedTeamNumber[TEAM_BLUE]) / 2.0;
		mappedTeamNumber[TEAM_BLUE] = (int)round(mappedTeamNumber[TEAM_BLUE] + adjustmentBlue);

		if (relativeStrength[TEAM_RED] - relativeStrength[TEAM_BLUE] > 0.00001 && mappedTeamNumber[TEAM_RED] < mappedTeamNumber[TEAM_BLUE])
			mappedTeamNumber[TEAM_RED] = mappedTeamNumber[TEAM_BLUE];
		else if (relativeStrength[TEAM_BLUE] - relativeStrength[TEAM_RED] > 0.00001 && mappedTeamNumber[TEAM_BLUE] < mappedTeamNumber[TEAM_RED])
			mappedTeamNumber[TEAM_BLUE] = mappedTeamNumber[TEAM_RED];
	}

	int rng;
	if (r_rngNumSet.integer) {
		rng = r_rngNum.integer;
	}
	else {
		int generated = Q_irand(-25, 25);
		trap_Cvar_Set("r_rngNum", va("%d", generated));
		trap_Cvar_Set("r_rngNumSet", "1");
		rng = generated;
	}
	mappedTeamNumber[TEAM_RED] += rng;
	mappedTeamNumber[TEAM_BLUE] += rng;

	if (!(mappedTeamNumber[TEAM_RED] % 5) && !(mappedTeamNumber[TEAM_BLUE] % 5) && mappedTeamNumber[TEAM_RED] != mappedTeamNumber[TEAM_BLUE]) {
		mappedTeamNumber[TEAM_RED] += 1;
		mappedTeamNumber[TEAM_BLUE] += 1;
	}


	char msg[256] = { 0 };
	Com_sprintf(msg, sizeof(msg),
		"^1Red: ^1%d^6, ^4Blue: ^4%d^6, Win probability ^6%.2f",
		mappedTeamNumber[TEAM_RED],
		mappedTeamNumber[TEAM_BLUE],
		fabs(relativeStrength[TEAM_RED] - relativeStrength[TEAM_BLUE]) <= 0.0001 ? 0.50 :
		round((relativeStrength[TEAM_RED] > relativeStrength[TEAM_BLUE] ? relativeStrength[TEAM_RED] : relativeStrength[TEAM_BLUE]) * 100) / 100.0);

	TeamGenerator_QueueChatMessage(sendFrom - g_entities, sendTo - g_entities, msg, trap_Milliseconds() + 850 + Q_irand(-50, 150));
	return qtrue;
}

void TeamGen_ClearRemindPositions(qboolean refreshClientinfo) {
	G_DBDeleteMetadataStartingWith("remindpos_account_");

	for (int i = 0; i < MAX_CLIENTS; i++) {
		qboolean update = qfalse;

		if (level.clients[i].sess.remindPositionOnMapChange.valid && level.clients[i].sess.remindPositionOnMapChange.pos)
			update = qtrue;

		memset(&level.clients[i].sess.remindPositionOnMapChange, 0, sizeof(level.clients[i].sess.remindPositionOnMapChange));
		if (level.clients[i].stats)
			level.clients[i].stats->remindedPosition = CTFPOSITION_UNKNOWN;

		if (g_broadcastCtfPos.integer && refreshClientinfo && update) {
			gentity_t *ent = &g_entities[i];
			if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED)
				continue;
			ClientUserinfoChanged(i);
		}
	}
}

// sets people's scores again if they change maps
void TeamGen_RemindPosition(gentity_t *ent) {
	if (!g_vote_teamgen_remindPositions.integer)
		return;

	if (!ent || !ent->client || !ent->client->sess.remindPositionOnMapChange.valid || IsRacerOrSpectator(ent))
		return;

	const char *posStr = va("Your position: %s", NameForPos(ent->client->sess.remindPositionOnMapChange.pos));

	// send message
	if (ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_REMINDPOSINCESSANTLY)) {
		for (int i = 0; i < 3; i++)
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, posStr);
		trap_SendServerCommand(ent - g_entities, va("cp \"%s\n\n\n\n\n\n\n\n\"", posStr)); // centerprint too, get fucked idiot
	}
	else {
		if (g_vote_teamgen_remindPositions.integer == 1) {
			// g_vote_teamgen_remindPositions 1 == (default) always remind pos, even on map restarts
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, posStr);
		}
		else {
			// g_vote_teamgen_remindPositions 2 == only remind pos on map changes (not restarts)
			if (!level.wasRestarted)
				TeamGenerator_QueueServerMessageInChat(ent - g_entities, posStr);
		}
	}

	// set score if not restarted
	if (!level.wasRestarted)
		ent->client->ps.persistant[PERS_SCORE] = ent->client->sess.remindPositionOnMapChange.score;

	// we no longer clear out the reminders from this function
}

void TeamGen_DoAutoRestart(void) {
	if (!level.g_lastTeamGenTimeSettingAtRoundStart || !g_vote_teamgen_autoRestartOnMapChange.integer || !g_vote_teamgen.integer ||
		!g_waitForAFK.integer || g_gametype.integer != GT_CTF)
		return;

	// sanity check; never do auto rs more than 30 minutes after teamgen
	qboolean teamgenHappenedRecently = !!(((int)time(NULL)) - level.g_lastTeamGenTimeSettingAtRoundStart < (level.g_lastTeamGenTimeSettingAtRoundStart + 1800));
	if (!teamgenHappenedRecently)
		return;

	Com_Printf("Auto start initiated.\n");
	level.autoStartPending = qtrue;
}
