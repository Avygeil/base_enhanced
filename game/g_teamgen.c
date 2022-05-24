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

double PlayerTierToRating(ctfPlayerTier_t tier) {
	switch (tier) {
	case PLAYERRATING_LOW_C: return 0.5;
	case PLAYERRATING_MID_C: return 0.55;
	case PLAYERRATING_HIGH_C: return 0.6;
	case PLAYERRATING_LOW_B: return 0.65;
	case PLAYERRATING_MID_B: return 0.7;
	case PLAYERRATING_HIGH_B: return 0.75;
	case PLAYERRATING_LOW_A: return 0.8;
	case PLAYERRATING_MID_A: return 0.85;
	case PLAYERRATING_HIGH_A: return 0.9;
	case PLAYERRATING_S: return 1.0;
	default: return 0.0;
	}
}

ctfPlayerTier_t PlayerTierFromRating(double num) {
	// stupid >= hack to account for imprecision
	if (num >= 1.0 - 0.00001) return PLAYERRATING_S;
	if (num >= 0.90 - 0.00001) return PLAYERRATING_HIGH_A;
	if (num >= 0.85 - 0.00001) return PLAYERRATING_MID_A;
	if (num >= 0.80 - 0.00001) return PLAYERRATING_LOW_A;
	if (num >= 0.75 - 0.00001) return PLAYERRATING_HIGH_B;
	if (num >= 0.7 - 0.00001) return PLAYERRATING_MID_B;
	if (num >= 0.65 - 0.00001) return PLAYERRATING_LOW_B;
	if (num >= 0.6 - 0.0001) return PLAYERRATING_HIGH_C;
	if (num >= 0.55 - 0.0001) return PLAYERRATING_MID_C;
	if (num >= 0.5 - 0.0001) return PLAYERRATING_LOW_C;
	return PLAYERRATING_UNRATED;
}

char *PlayerRatingToString(ctfPlayerTier_t tier) {
	switch (tier) {
	case PLAYERRATING_LOW_C: return "^8LOW C";
	case PLAYERRATING_MID_C: return "^8C";
	case PLAYERRATING_HIGH_C: return "^8HIGH C";
	case PLAYERRATING_LOW_B: return "^3LOW B";
	case PLAYERRATING_MID_B: return "^3B";
	case PLAYERRATING_HIGH_B: return "^3HIGH B";
	case PLAYERRATING_LOW_A: return "^2LOW A";
	case PLAYERRATING_MID_A: return "^2A";
	case PLAYERRATING_HIGH_A: return "^2HIGH A";
	case PLAYERRATING_S: return "^6S";
	default: return "^9UNRATED";
	}
}

#ifdef DEBUG_GENERATETEAMS_PRINT
static char *PlayerRatingToStringHTML(ctfPlayerTier_t tier) {
	switch (tier) {
	case PLAYERRATING_LOW_C: return "<font color=orange>LOW C</font>";
	case PLAYERRATING_MID_C: return "<font color=orange>C</font>";
	case PLAYERRATING_HIGH_C: return "<font color=orange>HIGH C</font>";
	case PLAYERRATING_LOW_B: return "<font color=gold>LOW B</font>";
	case PLAYERRATING_MID_B: return "<font color=gold>B</font>";
	case PLAYERRATING_HIGH_B: return "<font color=gold>HIGH B</font>";
	case PLAYERRATING_LOW_A: return "<font color=green>LOW A</font>";
	case PLAYERRATING_MID_A: return "<font color=green>A</font>";
	case PLAYERRATING_HIGH_A: return "<font color=green>HIGH A</font>";
	case PLAYERRATING_S: return "<font color=purple>S</font>";
	default: return "<font color=black>UNRATED</font>";
	}
}
#endif

qboolean TeamGenerator_PlayerIsPermaBarredButTemporarilyForcedPickable(gentity_t *ent) {
	if (!ent || !ent->client || !ent->client->account)
		return qfalse;

	if (!(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) && !(ent->client->account->flags & ACCOUNTFLAG_PERMABARRED))
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

qboolean TeamGenerator_PlayerIsBarredFromTeamGenerator(gentity_t *ent) {
	if (!ent || !ent->client)
		return qfalse;

	if (ent->client->pers.barredFromPugSelection)
		return qtrue;

	if (ent->client->account) {
		if ((ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) || (ent->client->account->flags & ACCOUNTFLAG_PERMABARRED)) {
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
				return qfalse;

			if (!(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) && ent->client->pers.permaBarredDeclaredPickable)
				return qfalse;
			
			return qtrue;
		}

		iterator_t iter;
		ListIterate(&level.barredPlayersList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
			if (bp->accountId == ent->client->account->id)
				return qtrue;
		}
	}

	return qfalse;
}

typedef struct {
	int accountId;
	char accountName[MAX_NAME_LENGTH];
	int clientNum;
	double rating[CTFPOSITION_OFFENSE + 1];
	positionPreferences_t posPrefs;
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
static XXH32_hash_t HashPermutationOfTeams(permutationOfTeams_t *t) {
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
	hashMe.differenceBetweenOffenses = 0;
	hashMe.differenceBetweenDefenses = 0;
	return XXH32(&hashMe, sizeof(permutationOfTeams_t), 0);
}

typedef struct {
	permutationOfTeams_t *best;
	void *callback;
	qboolean enforceChaseRule;
	int numEligible;
	uint64_t numPermutations;
	list_t *avoidedHashesList;
	qboolean banAvoidedPositions;
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
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	double differenceBetweenDefenses, differenceBetweenOffenses;
	{
		double team1DefenseTotal = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE];
		double team1OffenseTotal = team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
		double team2DefenseTotal = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE];
		double team2OffenseTotal = team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
		differenceBetweenDefenses = fabs(team1DefenseTotal - team2DefenseTotal);
		differenceBetweenOffenses = fabs(team1OffenseTotal - team2OffenseTotal);
	}

	// reward permutations that put people on preferred positions
	int numOnPreferredPos = 0, numOnAvoidedPos = 0;
	if (team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team1base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 100; else if (team1base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 10; else if (team1base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 1;
	if (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 100; else if (team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 10; else if (team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 1;
	if (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 100; else if (team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 10; else if (team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 1;
	if (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 100; else if (team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 10; else if (team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 1;
	if (team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team2base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 100; else if (team2base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 10; else if (team2base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 1;
	if (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 100; else if (team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 10; else if (team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 1;
	if (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 100; else if (team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 10; else if (team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 1;
	if (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 100; else if (team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 10; else if (team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 1;

	TeamGen_DebugPrintf("Regular:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.2f off diff, %0.2f def diff, %0.3f total, %0.3f diff</font>",
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
		bottomTierImbalance,
		team1BottomTiers,
		team2BottomTiers,
		topTierImbalance,
		team1TopTiers,
		team2TopTiers,
		differenceBetweenOffenses,
		differenceBetweenDefenses,
		total,
		diff);

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);
			int maxDiff;
			if (team1ChaseTier <= PLAYERRATING_LOW_B)
				maxDiff = 2;
			else if (highestTeam2OffenseTier == PLAYERRATING_S)
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
			else if (highestTeam1OffenseTier == PLAYERRATING_S)
				maxDiff = 3;
			else
				maxDiff = 4;
			if (highestTeam1OffenseTier - team2ChaseTier > maxDiff) {
				TeamGen_DebugPrintf(" unbalanced team 2 chase vs. team 1 offense %d - %d == %d<br/>", highestTeam1OffenseTier, team2ChaseTier, highestTeam1OffenseTier - team2ChaseTier);
				return;
			}
		}
	}

	// this permutation will be favored over the previous permutation if:
	// - it is fairer
	// - it is equally fair, but has more people on preferred pos
	// - it is equally fair and has an equal number of people on preferred pos, but has fewer people on avoided pos
	// - it is equally fair and has an equal number of people on preferred and avoided pos, but has better balance of bottom tier players
	// - it is equally fair and has an equal number of people on preferred and avoided pos and equal balance of bottom tier players, but has better balance of top tier players
	// - equal fairness/preferred/avoided/bottom balance/top balance, but has a smaller difference between total offense ratings
	// - equal fairness/preferred/avoided/bottom balance/top balance/offense difference, but has a smaller difference between total defense ratings
	if (diff < context->best->diff ||
		(diff == context->best->diff && numOnPreferredPos > context->best->numOnPreferredPos) ||
		(diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
		(diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
		(diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance) ||
		(diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && differenceBetweenOffenses < context->best->differenceBetweenOffenses) ||
		(diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && differenceBetweenOffenses == context->best->differenceBetweenOffenses && differenceBetweenDefenses < context->best->differenceBetweenDefenses)) {
		if (diff < context->best->diff)
			TeamGen_DebugPrintf(" <font color=purple>best so far (fairer)</font><br/>");
		else if (diff == context->best->diff && numOnPreferredPos > context->best->numOnPreferredPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, but more preferred pos)</font><br/>");
		else if (diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness and preferred pos, but less on avoided pos)</font><br/>");
		else if (diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, preferred pos, and avoided pos, but better bottom tier balance)</font><br/>");
		else if (diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, preferred pos, avoided pos, and bottom tier balance, but better top tier balance)</font><br/>");
		else if (diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && differenceBetweenOffenses < context->best->differenceBetweenOffenses)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, preferred pos, avoided pos, bottom tier balance, and top tier balance, but better offense balance)</font><br/>");
		else if (diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && differenceBetweenOffenses == context->best->differenceBetweenOffenses && differenceBetweenDefenses < context->best->differenceBetweenDefenses)
			TeamGen_DebugPrintf(" <font color=purple>best so far (same fairness, preferred pos, avoided pos, bottom tier balance, top tier balance, and offense balance, but better defense balance)</font><br/>");
		else
			TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

		if (context->avoidedHashesList && context->avoidedHashesList->size > 0) {
			permutationOfTeams_t hashMe = { 0 };
			hashMe.valid = qtrue;
			hashMe.diff = diff;
			hashMe.numOnPreferredPos = numOnPreferredPos;
			hashMe.numOnAvoidedPos = numOnAvoidedPos;
			hashMe.topTierImbalance = topTierImbalance;
			hashMe.bottomTierImbalance = bottomTierImbalance;
			hashMe.differenceBetweenOffenses = differenceBetweenOffenses;
			hashMe.differenceBetweenDefenses = differenceBetweenDefenses;
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
				return;
			}
		}

		context->best->valid = qtrue;
		context->best->diff = diff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		context->best->differenceBetweenOffenses = differenceBetweenOffenses;
		context->best->differenceBetweenDefenses = differenceBetweenDefenses;
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
	if (PlayerTierFromRating(team1base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team1BottomTiers;
	if (PlayerTierFromRating(team2base->rating[CTFPOSITION_BASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	if (PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) <= PLAYERRATING_HIGH_C) ++team2BottomTiers;
	int bottomTierImbalance = abs(team1BottomTiers - team2BottomTiers);

	double differenceBetweenDefenses, differenceBetweenOffenses;
	{
		double team1DefenseTotal = team1base->rating[CTFPOSITION_BASE] + team1chase->rating[CTFPOSITION_CHASE];
		double team1OffenseTotal = team1offense1->rating[CTFPOSITION_OFFENSE] + team1offense2->rating[CTFPOSITION_OFFENSE];
		double team2DefenseTotal = team2base->rating[CTFPOSITION_BASE] + team2chase->rating[CTFPOSITION_CHASE];
		double team2OffenseTotal = team2offense1->rating[CTFPOSITION_OFFENSE] + team2offense2->rating[CTFPOSITION_OFFENSE];
		differenceBetweenDefenses = fabs(team1DefenseTotal - team2DefenseTotal);
		differenceBetweenOffenses = fabs(team1OffenseTotal - team2OffenseTotal);
	}

	// reward permutations that put people on preferred positions
	int numOnPreferredPos = 0, numOnAvoidedPos = 0;
	if (team1base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team1base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 100; else if (team1base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 10; else if (team1base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 1;
	if (team1chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 100; else if (team1chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 10; else if (team1chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 1;
	if (team1offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 100; else if (team1offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 10; else if (team1offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 1;
	if (team1offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 100; else if (team1offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 10; else if (team1offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 1;
	if (team2base->posPrefs.avoid & (1 << CTFPOSITION_BASE)) numOnAvoidedPos += 1; else if (team2base->posPrefs.first & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 100; else if (team2base->posPrefs.second & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 10; else if (team2base->posPrefs.third & (1 << CTFPOSITION_BASE)) numOnPreferredPos += 1;
	if (team2chase->posPrefs.avoid & (1 << CTFPOSITION_CHASE)) numOnAvoidedPos += 1; else if (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 100; else if (team2chase->posPrefs.second & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 10; else if (team2chase->posPrefs.third & (1 << CTFPOSITION_CHASE)) numOnPreferredPos += 1;
	if (team2offense1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 100; else if (team2offense1->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 10; else if (team2offense1->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 1;
	if (team2offense2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) numOnAvoidedPos += 1; else if (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 100; else if (team2offense2->posPrefs.second & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 10; else if (team2offense2->posPrefs.third & (1 << CTFPOSITION_OFFENSE)) numOnPreferredPos += 1;

	TeamGen_DebugPrintf("Tryhard:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.2f off diff, %0.2f def diff, %0.3f total, %0.3f diff</font>",
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
		bottomTierImbalance,
		team1BottomTiers,
		team2BottomTiers,
		topTierImbalance,
		team1TopTiers,
		team2TopTiers,
		differenceBetweenOffenses,
		differenceBetweenDefenses,
		total,
		diff);

	if (diff >= 0.04) {
		TeamGen_DebugPrintf(" difference too great.<br/>");
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
			else if (highestTeam2OffenseTier == PLAYERRATING_S)
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
			else if (highestTeam1OffenseTier == PLAYERRATING_S)
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

	// this permutation will be favored over the previous permutation if:
	// - it is higher caliber overall
	// - it is equally high caliber, but fairer
	// - it is equally high caliber and equally fair, but has more people on preferred pos
	// - it is equally high caliber, equally fair, and has an equal number of people on preferred pos, but has fewer people on avoided pos
	// - it is equally high caliber, equally fair, and has an equal number of people on preferred and avoided pos, but has better balance of bottom tier players
	// - it is equally high caliber, equally fair, has an equal number of people on preferred and avoided pos and equal balance of bottom tier players, but has better balance of top tier players
	// - equal hc/fairness/preferred/avoided/bottom balance/top balance, but has a smaller difference between total offense ratings
	// - equal hc/fairness/preferred/avoided/bottom balance/top balance/offense difference, but has a smaller difference between total defense ratings
	if (total > currentBestCombinedStrength ||
		(total == currentBestCombinedStrength && diff < context->best->diff) ||
		(total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos > context->best->numOnPreferredPos) ||
		(total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos) ||
		(total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance) ||
		(total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance) ||
		(total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && differenceBetweenOffenses < context->best->differenceBetweenOffenses) ||
		(total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && differenceBetweenOffenses == context->best->differenceBetweenOffenses && differenceBetweenDefenses < context->best->differenceBetweenDefenses)) {
		if (total > currentBestCombinedStrength)
			TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength better)</font><br/>");
		else if (total == currentBestCombinedStrength && diff < context->best->diff)
			TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength equal, but fairer)</font><br/>");
		else if (total == currentBestCombinedStrength && numOnPreferredPos > context->best->numOnPreferredPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength and fairness equal, but more preferred pos)</font><br/>");
		else if (total == currentBestCombinedStrength && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos < context->best->numOnAvoidedPos)
			TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength, fairness equal and preferred pos equal, but less on avoided pos)</font><br/>");
		else if (total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance < context->best->bottomTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength, fairness, preferred pos, and avoided pos, but better bottom tier balance)</font><br/>");
		else if (total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance < context->best->topTierImbalance)
			TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength, fairness, preferred pos, avoided pos, and bottom tier balance, but better top tier balance)</font><br/>");
		else if (total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && differenceBetweenOffenses < context->best->differenceBetweenOffenses)
			TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength, fairness, preferred pos, avoided pos, bottom tier balance, and top tier balance, but better offense balance)</font><br/>");
		else if (total == currentBestCombinedStrength && diff == context->best->diff && numOnPreferredPos == context->best->numOnPreferredPos && numOnAvoidedPos == context->best->numOnAvoidedPos && bottomTierImbalance == context->best->bottomTierImbalance && topTierImbalance == context->best->topTierImbalance && differenceBetweenOffenses == context->best->differenceBetweenOffenses && differenceBetweenDefenses < context->best->differenceBetweenDefenses)
			TeamGen_DebugPrintf(" <font color=purple>best so far (combined strength, fairness, preferred pos, avoided pos, bottom tier balance, top tier balance, and offense balance, but better defense balance)</font><br/>");
		else
			TeamGen_DebugPrintf("<font color=purple>???</font><br/>");

		if (context->avoidedHashesList && context->avoidedHashesList->size > 0) {
			permutationOfTeams_t hashMe = { 0 };
			hashMe.valid = qtrue;
			hashMe.diff = diff;
			hashMe.numOnPreferredPos = numOnPreferredPos;
			hashMe.numOnAvoidedPos = numOnAvoidedPos;
			hashMe.topTierImbalance = topTierImbalance;
			hashMe.bottomTierImbalance = bottomTierImbalance;
			hashMe.differenceBetweenOffenses = differenceBetweenOffenses;
			hashMe.differenceBetweenDefenses = differenceBetweenDefenses;
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
				return;
			}
		}

		context->best->valid = qtrue;
		context->best->diff = diff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		context->best->differenceBetweenOffenses = differenceBetweenOffenses;
		context->best->differenceBetweenDefenses = differenceBetweenDefenses;
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

	if (context->banAvoidedPositions) {
		if ((offensePlayer1->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (offensePlayer2->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) ||
			(offensePlayer3->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE)) || (offensePlayer4->posPrefs.avoid & (1 << CTFPOSITION_OFFENSE))) {
			return;
		}
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
static uint64_t PermuteTeams(permutationPlayer_t *playerArray, int numEligible, permutationOfTeams_t *bestOut, PermutationCallback callback, qboolean enforceChaseRule, list_t *avoidedHashesList, qboolean banAvoidedPositions) {
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
	if (avoidedHashesList && avoidedHashesList->size > 0)
		context.avoidedHashesList = avoidedHashesList;
	else
		context.avoidedHashesList = NULL;
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

static qboolean GenerateTeams(pugProposal_t *set, permutationOfTeams_t *mostPlayed, permutationOfTeams_t *highestCaliber, permutationOfTeams_t *fairest, permutationOfTeams_t *desired, uint64_t *numPermutations) {
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
			TeamGenerator_PlayerIsBarredFromTeamGenerator(ent) ||
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
	qboolean gotValid = qfalse;
	uint64_t gotten = 0llu;
	for (int type = TEAMGENERATORTYPE_FIRST; type < NUM_TEAMGENERATORTYPES; type++) {

		// we only do the desired pass if and only if it's enabled and there isn't an a/b/c option with nobody on avoided pos
		if (type == TEAMGENERATORTYPE_DESIREDPOS) {
			if (!g_vote_teamgen_enableAppeasing.integer)
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
			if (type == TEAMGENERATORTYPE_MOSTPLAYED) {
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
			else if (type == TEAMGENERATORTYPE_HIGHESTRATING) {
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
					if (PlayerTierFromRating(highestRating) == PLAYERRATING_LOW_C && mostPlayedPositions) {
						// special case: their highest rating is a C, meaning they are C on any and all positions they have ratings on
						// just assume that their most-played positions are higher caliber
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
			else if (type == TEAMGENERATORTYPE_FAIREST || type == TEAMGENERATORTYPE_DESIREDPOS) {
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
								if (fabricatedTier < PLAYERRATING_LOW_C)
									fabricatedTier = PLAYERRATING_LOW_C;
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
					if (algoPlayer->posPrefs.first)
						algoPlayer->posPrefs.second = algoPlayer->posPrefs.first; // demote computer-determined first choice to second
					algoPlayer->posPrefs.first = client->posPrefs.first;

					if (client->posPrefs.second) {
						if (algoPlayer->posPrefs.second)
							algoPlayer->posPrefs.third = algoPlayer->posPrefs.second; // demote computer-determined second choice to third
						algoPlayer->posPrefs.second = client->posPrefs.second;

						if (client->posPrefs.third)
							algoPlayer->posPrefs.third = client->posPrefs.third;
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

		// evaluate every possible permutation of teams for this teamgen type
		permutationOfTeams_t try1 = { 0 }, try2 = { 0 };
		try1.diff = try2.diff = try1.differenceBetweenOffenses = try1.differenceBetweenDefenses = try2.differenceBetweenOffenses = try2.differenceBetweenDefenses = 999999.999999;
		PermutationCallback callback;
		if (type == TEAMGENERATORTYPE_HIGHESTRATING)
			callback = TryTeamPermutation_Tryhard;
		else
			callback = TryTeamPermutation;
		TeamGen_DebugPrintf("<font color=darkgreen>==========Permuting teams with type %d==========</font><br/>", type);

		uint64_t thisGotten = PermuteTeams(&players[0], numEligible, &try1, callback, qtrue, &set->avoidedHashesList, qtrue);
		if (thisGotten > gotten)
			gotten = thisGotten;

		qboolean allowSecondTry = qtrue;
		if (!try1.valid) {
			TeamGen_DebugPrintf("<font color=orange>==========No valid permutation for type %d; trying again without chase rule==========</font><br/>", type);
			memset(&try1, 0, sizeof(try1));
			try1.diff = try1.differenceBetweenOffenses = try1.differenceBetweenDefenses = 999999.999999;
			thisGotten = PermuteTeams(&players[0], numEligible, &try1, callback, qfalse, &set->avoidedHashesList, qtrue);
			if (thisGotten > gotten)
				gotten = thisGotten;

			if (!try1.valid && type != TEAMGENERATORTYPE_DESIREDPOS) {
				TeamGen_DebugPrintf("<font color=orange>==========No valid permutation without chase rule for type %d; trying again with chase rule but without banning avoided pos==========</font><br/>", type);
				allowSecondTry = qfalse;

				memset(&try1, 0, sizeof(try1));
				try1.diff = try1.differenceBetweenOffenses = try1.differenceBetweenDefenses = 999999.999999;
				thisGotten = PermuteTeams(&players[0], numEligible, &try1, callback, qtrue, &set->avoidedHashesList, qfalse);
				if (thisGotten > gotten)
					gotten = thisGotten;

				if (!try1.valid) {
					TeamGen_DebugPrintf("<font color=orange>==========No valid permutation with chase rule but without banning avoided pos for type %d; trying again without chase rule AND without banning avoided pos==========</font><br/>", type);
					memset(&try1, 0, sizeof(try1));
					try1.diff = try1.differenceBetweenOffenses = try1.differenceBetweenDefenses = 999999.999999;
					thisGotten = PermuteTeams(&players[0], numEligible, &try1, callback, qfalse, &set->avoidedHashesList, qfalse);
					if (thisGotten > gotten)
						gotten = thisGotten;
				}
			}
		}

		// if the permutation banning avoided positions is not 50-50, try again without banning avoided pos
		permutationOfTeams_t *thisPermutation;
		if (allowSecondTry) {
			if (type != TEAMGENERATORTYPE_DESIREDPOS && try1.valid && try1.diff > 0.00001) {
				TeamGen_DebugPrintf("<font color=orange>==========Diff is > 0 for type %d; attempting without banning avoided pos==========</font><br/>", type);
				thisGotten = PermuteTeams(&players[0], numEligible, &try2, callback, qtrue, &set->avoidedHashesList, qfalse);
				if (thisGotten > gotten)
					gotten = thisGotten;

				if (!try2.valid) {
					TeamGen_DebugPrintf("<font color=orange>==========No valid permutation for type %d without banning avoided pos; trying again without chase rule==========</font><br/>", type);
					memset(&try2, 0, sizeof(try2));
					try2.diff = try2.differenceBetweenOffenses = try2.differenceBetweenDefenses = 999999.999999;
					thisGotten = PermuteTeams(&players[0], numEligible, &try2, callback, qfalse, &set->avoidedHashesList, qfalse);
					if (thisGotten > gotten)
						gotten = thisGotten;
				}

				if (try2.valid && try2.diff < try1.diff - 0.00001) {
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

		if (!thisPermutation->valid) {
			// still couldn't get a valid permutation for this teamgen type, even without the chase rule
			TeamGen_DebugPrintf("<font color=red>==========No valid permutation for type %d.==========</font><br/>", type);
			continue;
		}

		NormalizePermutationOfTeams(thisPermutation);
		thisPermutation->hash = HashPermutationOfTeams(thisPermutation);

		switch (type) {
		case TEAMGENERATORTYPE_MOSTPLAYED:
			if (mostPlayed) memcpy(mostPlayed, thisPermutation, sizeof(permutationOfTeams_t)); break;
		case TEAMGENERATORTYPE_HIGHESTRATING:
			if (highestCaliber) memcpy(highestCaliber, thisPermutation, sizeof(permutationOfTeams_t)); break;
		case TEAMGENERATORTYPE_FAIREST:
			if (fairest) memcpy(fairest, thisPermutation, sizeof(permutationOfTeams_t)); break;
		case TEAMGENERATORTYPE_DESIREDPOS:
			if (desired) memcpy(desired, thisPermutation, sizeof(permutationOfTeams_t)); break;
		}
		gotValid = qtrue;
		avoidedHash_t *avoid = ListAdd(&set->avoidedHashesList, sizeof(avoidedHash_t));
		avoid->hash = thisPermutation->hash;
	}
	if (numPermutations)
		*numPermutations = gotten;

#ifdef DEBUG_GENERATETEAMS
	clock_t end = clock();
	double computeTimePrint = ((double)(end - start)) / CLOCKS_PER_SEC;
	Com_Printf("Total compute time: %0.2f seconds\n", computeTimePrint);
#endif

#ifdef DEBUG_GENERATETEAMS_PRINT
	if (!inReroll) {
		char *debugFileEnd = "</body></html>";
		trap_FS_Write(debugFileEnd, strlen(debugFileEnd), debugFile);
		trap_FS_FCloseFile(debugFile);
	}
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

void TeamGenerator_QueueServerMessageInConsole(int clientNum, const char *msg) {
	if (!VALIDSTRING(msg))
		return;

	queuedServerMessage_t *add = ListAdd(&level.queuedServerMessagesList, sizeof(queuedServerMessage_t));
	add->clientNum = clientNum;
	add->text = strdup(msg);
	add->inConsole = qtrue;
	add->serverFrameNum = level.framenum;
}

static void PrintTeamsProposalsInConsole(pugProposal_t *set) {
	assert(set);
	char formattedNumber[64] = { 0 };
	FormatNumberToStringWithCommas(set->numValidPermutationsChecked, formattedNumber, sizeof(formattedNumber));
	TeamGenerator_QueueServerMessageInConsole(-1, va("Team generator results for %s\n(%s valid permutations evaluated):\n", set->namesStr, formattedNumber));

	double lowestDiff = 999999;
	for (int i = 0; i < 4; i++) {
		permutationOfTeams_t *thisPermutation;
		switch (i) {
		case 0: thisPermutation = &set->suggested; break;
		case 1: thisPermutation = &set->highestCaliber; break;
		case 2: thisPermutation = &set->fairest; break;
		case 3: thisPermutation = &set->desired; break;
		}
		if (thisPermutation->valid && thisPermutation->diff < lowestDiff)
			lowestDiff = thisPermutation->diff;
	}

	qboolean printedFairest = qfalse;
	int numPrinted = 0;
	char lastLetter = 'a';
	for (int i = 0; i < 4; i++) {
		permutationOfTeams_t *thisPermutation;
		switch (i) {
		case 0: thisPermutation = &set->suggested; break;
		case 1: thisPermutation = &set->highestCaliber; break;
		case 2: thisPermutation = &set->fairest; break;
		case 3: thisPermutation = &set->desired; break;
		}
		if (!thisPermutation->valid)
			continue;

		char letter;
		char *suggestionTypeStr;
		if (!i) {
			if (thisPermutation->hash == set->highestCaliber.hash && (thisPermutation->diff < 0.00001 || thisPermutation->hash == set->fairest.hash || (set->fairest.valid && thisPermutation->diff == set->fairest.diff))) {
				suggestionTypeStr = "Suggested, highest caliber, and fairest";
				printedFairest = qtrue;
			}
			else if (thisPermutation->hash == set->highestCaliber.hash) {
				suggestionTypeStr = "Suggested and highest caliber";
			}
			else if (fabs(thisPermutation->diff - lowestDiff) < 0.00001) {
				suggestionTypeStr = "Suggested and fairest";
				printedFairest = qtrue;
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
			if (fabs(thisPermutation->diff - lowestDiff) < 0.00001) {
				suggestionTypeStr = "Highest caliber and fairest";
				printedFairest = qtrue;
			}
			else {
				suggestionTypeStr = "Highest caliber";
			}
			letter = set->highestCaliberLetter = lastLetter++;
		}
		else if (i == 2) {
			if ((thisPermutation->hash == set->suggested.hash && set->suggested.valid) || (thisPermutation->hash == set->highestCaliber.hash && set->highestCaliber.valid)) {
				thisPermutation->valid = qfalse;
				continue;
			}
			// sanity check: don't bother calling it "fairest" if it's not the fairest option
			// can happen eventually with enough rerolls
			if (fabs(thisPermutation->diff - lowestDiff) < 0.00001) {
				if (printedFairest)
					suggestionTypeStr = "Additional fairest"; // don't write plain old "fairest" if we already printed one above that also said fairest (it would imply this is even fairer)
				else
					suggestionTypeStr = "Fairest";
			}
			else {
				suggestionTypeStr = "Additional";
			}
			letter = set->fairestLetter = lastLetter++;
		}
		else if (i == 3) {
			if ((thisPermutation->hash == set->suggested.hash && set->suggested.valid) || (thisPermutation->hash == set->highestCaliber.hash && set->highestCaliber.valid) ||
				(thisPermutation->hash == set->fairest.hash && set->fairest.valid)) {
				thisPermutation->valid = qfalse;
				continue;
			}
			if (fabs(thisPermutation->diff - lowestDiff) < 0.00001)
				suggestionTypeStr = "Appeasing and fairest"; // this is probably not actually possible
			else
				suggestionTypeStr = "Appeasing";
			letter = set->desiredLetter = lastLetter++;
		}

		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (!ent->inuse || !ent->client)
				continue;
			int accId = ent->client->account ? ent->client->account->id : -1;
			TeamGenerator_QueueServerMessageInConsole(i, va("%s%s teams: ^5enter %c%c in chat if you approve\n^1Red team:^7 %.2f'/. relative strength\n    ^5Base: %s%s\n    ^6Chase: %s%s\n    ^2Offense: %s%s^7, %s%s^7\n^4Blue team:^7 %.2f'/. relative strength\n    ^5Base: %s%s\n    ^6Chase: %s%s\n    ^2Offense: %s%s^7, %s%s^7\n",
				numPrinted ? "\n" : "",
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
				thisPermutation->teams[1].offense2Name));
		}

		++numPrinted;
	}

	if (numPrinted == 4) {
		TeamGenerator_QueueServerMessageInConsole(-1,
			va("Vote to approve a teams proposal by entering e.g. ^5%ca^7, ^5%cb^7, ^5%cc^7, or ^5%cd^7\n"
				"You can approve of multiple teams proposals simultaneously by entering e.g. ^5%cab^7\n",
				TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER));
	}
	else if (numPrinted == 3) {
		TeamGenerator_QueueServerMessageInConsole(-1,
			va("Vote to approve a teams proposal by entering e.g. ^5%ca^7, ^5%cb^7, or ^5%cc^7\n"
			"You can approve of multiple teams proposals simultaneously by entering e.g. ^5%cab^7\n",
			TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER));
	}
	else if (numPrinted == 2) {
		TeamGenerator_QueueServerMessageInConsole(-1,
			va("Vote to approve a teams proposal by entering ^5%ca^7 or ^5%cb^7\n"
			"You can approve of both teams proposals simultaneously by entering ^5%cab^7\n",
			TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER));
	}
	else if (numPrinted == 1) {
		TeamGenerator_QueueServerMessageInConsole(-1,
			va("Vote to approve the teams proposal by entering ^5%ca^7\n", TEAMGEN_CHAT_COMMAND_CHARACTER));
	}

	TeamGenerator_QueueServerMessageInConsole(-1,
		va("You can vote to reroll the teams proposals by entering ^5%creroll^7\n"
		"You can vote to cancel the pug proposal by entering ^5%ccancel^7\n",
		TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER));
}

static void ActivatePugProposal(pugProposal_t *set, qboolean forcedByServer) {
	assert(set);

	if (GenerateTeams(set, &set->suggested, &set->highestCaliber, &set->fairest, &set->desired, &set->numValidPermutationsChecked)) {
		TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d %s (%s). Check console for teams proposals.", set->num, forcedByServer ? "force passed by server" : "passed", set->namesStr));
		set->passed = qtrue;
		level.activePugProposal = set;
		PrintTeamsProposalsInConsole(set);

		if (level.autoStartPending) {
			Com_Printf("The currently pending auto start was cancelled due to team generator changing the teams.\n");
			level.autoStartPending = qfalse;
		}

		// kill any existing map vote if we decide to do a different pug proposal
		if (level.voteTime && (!Q_stricmp(level.voteString, "mapvote") || level.multiVoting)) {
			PrintIngame(-1, "Map vote automatically stopped due to pug proposal passing.\n");
			level.voteAutoPassOnExpiration = qfalse;
			level.multiVoting = qfalse;
			level.inRunoff = qfalse;
			level.multiVoteChoices = 0;
			level.multiVoteHasWildcard = qfalse;
			level.multivoteWildcardMapFileName[0] = '\0';
			level.mapsThatCanBeVotedBits = 0;
			memset(level.multiVotes, 0, sizeof(level.multiVotes));
			memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
			memset(&level.multiVoteMapShortNames, 0, sizeof(level.multiVoteMapShortNames));
			memset(&level.multiVoteMapFileNames, 0, sizeof(level.multiVoteMapFileNames));
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

extern void fixVoters(qboolean allowRacers);
static void StartAutomaticTeamGenMapVote(void) {
	if (g_vote_teamgen_autoMapVoteSeconds.integer <= 0)
		return;

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

	int voteDurationSeconds = Com_Clampi(15, 300, g_vote_teamgen_autoMapVoteSeconds.integer);
	Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "New Map Vote (%d Maps) ^2(auto pass)^7", totalMapsToChoose);
	Com_sprintf(level.voteString, sizeof(level.voteString), "mapvote");

	level.voteTime = level.time + ((voteDurationSeconds - 30) * 1000);
	level.voteAutoPassOnExpiration = qtrue;
	level.voteYes = 0;
	level.voteNo = 0;
	//level.lastVotingClient
	level.multiVoting = qfalse;
	level.inRunoff = qfalse;
	level.mapsThatCanBeVotedBits = 0;
	level.multiVoting = qfalse;
	level.inRunoff = qfalse;
	level.multiVoteChoices = 0;
	level.multiVoteHasWildcard = qfalse;
	level.multivoteWildcardMapFileName[0] = '\0';
	memset(level.multiVotes, 0, sizeof(level.multiVotes));
	memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
	memset(&level.multiVoteMapShortNames, 0, sizeof(level.multiVoteMapShortNames));
	memset(&level.multiVoteMapFileNames, 0, sizeof(level.multiVoteMapFileNames));

	fixVoters(qfalse);

	trap_SetConfigstring(CS_VOTE_TIME, va("%i", level.voteTime));
	trap_SetConfigstring(CS_VOTE_STRING, level.voteDisplayString);
	trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
	trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
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

			ent->client->sess.remindPositionOnMapChange.valid = qtrue;
			ent->client->sess.remindPositionOnMapChange.pos = pos;
			ent->client->sess.remindPositionOnMapChange.score = score;
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

	StartAutomaticTeamGenMapVote();

	char timeBuf[MAX_STRING_CHARS] = { 0 };
	Com_sprintf(timeBuf, sizeof(timeBuf), "%d", (int)time(NULL));
	trap_Cvar_Set("g_lastTeamGenTime", timeBuf);
}

qboolean TeamGenerator_VoteForTeamPermutations(gentity_t *ent, const char *voteStr, char **newMessage) {
	assert(ent && VALIDSTRING(voteStr));

	if (!ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You do not have an account, so you cannot vote for team proposals. Please contact an admin for help setting up an account.");
		return qtrue;
	}

	if (!level.activePugProposal) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "No pug proposal is currently active.");
		return qtrue;
	}

	char oldVotes[5] = { 0 };
	char newVotes[5] = { 0 };
	for (const char *p = voteStr; *p && p - voteStr < 4; p++) {
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
		else if (level.activePugProposal->desired.valid && level.activePugProposal->desiredLetter == lower) {
			votesInt = &level.activePugProposal->desiredVoteClients;
			permutation = &level.activePugProposal->desired;
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
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("You cannot vote on teams proposal %c because you are not part of it.", lower));
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
		for (int i = 0; i < 4; i++) {
			char letter;
			int *votesInt;
			permutationOfTeams_t *permutation;
			switch (i) {
			case 0: permutation = &level.activePugProposal->suggested; votesInt = &level.activePugProposal->suggestedVoteClients; letter = level.activePugProposal->suggestedLetter; break;
			case 1: permutation = &level.activePugProposal->highestCaliber; votesInt = &level.activePugProposal->highestCaliberVoteClients; letter = level.activePugProposal->highestCaliberLetter; break;
			case 2: permutation = &level.activePugProposal->fairest; votesInt = &level.activePugProposal->fairestVoteClients; letter = level.activePugProposal->fairestLetter; break;
			case 3: permutation = &level.activePugProposal->desired; votesInt = &level.activePugProposal->desiredVoteClients; letter = level.activePugProposal->desiredLetter; break;
			}

			if (!permutation->valid)
				continue;

			int numYesVotes = 0;
			for (int j = 0; j < MAX_CLIENTS; j++) {
				if (*votesInt & (1 << j))
					++numYesVotes;
			}

			// color white if they voted for it just now
			char color = '9';
			if (newVotes[0] && strchr(newVotes, letter))
				color = '7';

			const int numRequired = g_vote_teamgen_team_requiredVotes.integer ? g_vote_teamgen_team_requiredVotes.integer : 5;
			Q_strcat(buf, sizeof(buf), va(" ^%c(%c: %d/%d)", color, letter, numYesVotes, numRequired));
		}
		*newMessage = buf;
	}

	int numPermutationsWithEnoughVotesToPass = 0;
	for (int i = 0; i < 4; i++) {
		int *votesInt;
		permutationOfTeams_t *permutation;
		switch (i) {
		case 0: permutation = &level.activePugProposal->suggested; votesInt = &level.activePugProposal->suggestedVoteClients; break;
		case 1: permutation = &level.activePugProposal->highestCaliber; votesInt = &level.activePugProposal->highestCaliberVoteClients; break;
		case 2: permutation = &level.activePugProposal->fairest; votesInt = &level.activePugProposal->fairestVoteClients; break;
		case 3: permutation = &level.activePugProposal->desired; votesInt = &level.activePugProposal->desiredVoteClients; break;
		}

		if (!permutation->valid)
			continue;

		int numYesVotes = 0;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			if (*votesInt & (1 << j))
				++numYesVotes;
		}

		const int numRequired = g_vote_teamgen_team_requiredVotes.integer ? g_vote_teamgen_team_requiredVotes.integer : 5;
		if (numYesVotes >= numRequired)
			++numPermutationsWithEnoughVotesToPass;
	}

	int tiebreakerOrder[] = { 0, 1, 2, 3 };
	srand(teamGenSeed);
	FisherYatesShuffle(&tiebreakerOrder[0], 4, sizeof(int));
	srand(time(NULL));

	for (int i = 0; i < 4; i++) {
		int j = tiebreakerOrder[i];
		char letter;
		int *votesInt;
		permutationOfTeams_t *permutation;
		switch (j) {
		case 0: permutation = &level.activePugProposal->suggested; votesInt = &level.activePugProposal->suggestedVoteClients; letter = level.activePugProposal->suggestedLetter; break;
		case 1: permutation = &level.activePugProposal->highestCaliber; votesInt = &level.activePugProposal->highestCaliberVoteClients; letter = level.activePugProposal->highestCaliberLetter; break;
		case 2: permutation = &level.activePugProposal->fairest; votesInt = &level.activePugProposal->fairestVoteClients; letter = level.activePugProposal->fairestLetter; break;
		case 3: permutation = &level.activePugProposal->desired; votesInt = &level.activePugProposal->desiredVoteClients; letter = level.activePugProposal->desiredLetter; break;
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

	const int numRequired = g_vote_teamgen_pug_requiredVotes.integer ? g_vote_teamgen_pug_requiredVotes.integer : 5;

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%c%d   ^%c(%d/%d)", TEAMGEN_CHAT_COMMAND_CHARACTER, set->num, doVote ? '7' : '9', numYesVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	if (doVote && numYesVotesFromEligiblePlayers >= numRequired)
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

	// prevent spectators from trolling ingame puggers
	if (IsRacerOrSpectator(ent) && level.wasRestarted && !level.someoneWasAFK && level.numTeamTicks) {
		float avgRed = (float)level.numRedPlayerTicks / (float)level.numTeamTicks;
		float avgBlue = (float)level.numBluePlayerTicks / (float)level.numTeamTicks;
		int avgRedInt = (int)lroundf(avgRed);
		int avgBlueInt = (int)lroundf(avgBlue);
		if (avgRedInt == avgBlueInt &&
			avgRedInt + avgBlueInt >= 4 &&
			fabs(avgRed - round(avgRed)) < 0.2f &&
			fabs(avgBlue - round(avgBlue)) < 0.2f) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("You cannot currently start a pug from %s because there is an ongoing match.", ent->client->sess.inRacemode ? "racemode" : "spec"));
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
	return qfalse;
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
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || !ent->client->account ||
			ent->client->sess.clientType != CLIENT_TYPE_NORMAL || ent->client->pers.barredFromPugSelection) {
			continue;
		}

		if (ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED) {
			continue; // don't print anything regardless of whether a hardpermabarred guy was temporarily marked pickable, since they can't change it in any case
		}

		if ((ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) &&
			(ent->client->pers.permaBarredDeclaredPickable || TeamGenerator_PlayerIsPermaBarredButTemporarilyForcedPickable(ent))) {
			continue; // he has already been marked as pickable; don't print anything
		}

		qboolean hasSpecName = IsSpecName(ent->client->pers.netname);
		qboolean isIngame = !!(ent->client->sess.sessionTeam == TEAM_RED || ent->client->sess.sessionTeam == TEAM_BLUE);

		char pickabilityMessage[MAX_STRING_CHARS] = { 0 }, renameMessage[MAX_STRING_CHARS] = { 0 };
		if (ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) {
			Com_sprintf(pickabilityMessage, sizeof(pickabilityMessage), "^1You are unpickable");
			Com_sprintf(renameMessage, sizeof(renameMessage), "If you want to pug, enter %cpickable in chat", TEAMGEN_CHAT_COMMAND_CHARACTER);
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
	assert(numOld);

	//TeamGen_DebugPrintf("Reroll: %d numOld\n", numOld);

	time_t startTime = time(NULL);
	unsigned int bestNumDifferent = 0u, bestSeed = 0u;
	unsigned int originalSeed = teamGenSeed;

	// generate teams up to 8 times. the seed that generates the most new permutations will be chosen.
#ifdef DEBUG_GENERATETEAMS_PRINT
	inReroll = qtrue;
#endif
	for (int i = 0; i < 1; i++) {
		teamGenSeed = startTime + (i * 69);
		srand(teamGenSeed);

		qboolean result = GenerateTeams(level.activePugProposal, &level.activePugProposal->suggested, &level.activePugProposal->highestCaliber, &level.activePugProposal->fairest, &level.activePugProposal->desired, &level.activePugProposal->numValidPermutationsChecked);
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
		level.activePugProposal->suggestedVoteClients = level.activePugProposal->highestCaliberVoteClients = level.activePugProposal->fairestVoteClients = level.activePugProposal->desiredVoteClients = 0;
		TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d rerolled%s (%s). Check console for new teams proposals.", level.activePugProposal->num, forcedByServer ? " by server" : "", level.activePugProposal->namesStr));
		PrintTeamsProposalsInConsole(level.activePugProposal);
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

	if (!(ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) && !(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED)) {
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
		if (ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_PERMABARRED) && !(ent->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED)) {
			PrintIngame(ent - g_entities,
				"*Chat commands:\n"
				"^7%cstart          - propose playing a pug with current non-spec players\n"
				"^9%c[number]       - vote to approve a pug proposal\n"
				"^7%c[ a | b | c | d ]  - vote to approve one or more teams proposals\n"
				"^9%creroll         - vote to generate new teams proposals with the same players\n"
				"^7%ccancel         - vote to generate new teams proposals with the same players\n"
				"^9%clist           - show which players are part of each pug proposal\n"
				"^7%cpickable       - declare yourself as pickable for pugs\n"
				, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER);
		}
		else {
			PrintIngame(ent - g_entities,
				"*Chat commands:\n"
				"^7%cstart          - propose playing a pug with current non-spec players\n"
				"^9%c[number]       - vote to approve a pug proposal\n"
				"^7%c[ a | b | c | d ]  - vote to approve one or more teams proposals\n"
				"^9%creroll         - vote to generate new teams proposals with the same players\n"
				"^7%ccancel         - vote to generate new teams proposals with the same players\n"
				"^9%clist           - show which players are part of each pug proposal\n"
				"^7"
				, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER, TEAMGEN_CHAT_COMMAND_CHARACTER);
		}
		SV_Tell(ent - g_entities, "See console for chat command help.");
		return qtrue;
	}

	if (!Q_stricmp(s, "start"))
		return TeamGenerator_PugStart(ent, newMessage);

	if (!Q_stricmp(s, "reroll"))
		return TeamGenerator_VoteToReroll(ent, newMessage);

	if (!Q_stricmp(s, "cancel"))
		return TeamGenerator_VoteToCancel(ent, newMessage);

	if (!Q_stricmp(s, "list")) {
		TeamGenerator_PrintPlayersInPugProposals(ent);
		return qtrue;
	}

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

		if (found->client->account && ((found->client->account->flags & ACCOUNTFLAG_PERMABARRED) || (found->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED))) {
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
			if (found->client->account) {
				qboolean foundInList = qfalse;
				iterator_t iter;
				ListIterate(&level.barredPlayersList, &iter, qfalse);
				while (IteratorHasNext(&iter)) {
					barredOrForcedPickablePlayer_t *bp = IteratorNext(&iter);
					if (bp->accountId == found->client->account->id) {
						foundInList = qtrue;
						break;
					}
				}

				if (!foundInList) { // barred in client->pers but not list; add them to list
					barredOrForcedPickablePlayer_t *add = ListAdd(&level.barredPlayersList, sizeof(barredOrForcedPickablePlayer_t));
					add->accountId = found->client->account->id;
				}
			}
			Com_Printf("%s^7 is already barred.\n", name);
			return;
		}

		found->client->pers.barredFromPugSelection = qtrue;
		if (found->client->account) {
			Com_Printf("Barred player %s^7.\n", name);
			barredOrForcedPickablePlayer_t *add = ListAdd(&level.barredPlayersList, sizeof(barredOrForcedPickablePlayer_t));
			add->accountId = found->client->account->id;
		}
		else {
			Com_Printf("Barred player %s^7. Since they do not have an account, they will need to be barred again if they reconnect to circumvent the bar before the current map ends.\n", name);
		}

		ClientUserinfoChanged(found - g_entities);

		TeamGenerator_QueueServerMessageInChat(-1, va("%s^7 barred from team generation by server.", name));
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

		if (found->client->account && ((found->client->account->flags & ACCOUNTFLAG_PERMABARRED) || (found->client->account->flags & ACCOUNTFLAG_HARDPERMABARRED))) {
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

		found->client->pers.barredFromPugSelection = qfalse;
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

	float avgRed = (float)level.numRedPlayerTicks / (float)level.numTeamTicks;
	float avgBlue = (float)level.numBluePlayerTicks / (float)level.numTeamTicks;
	int avgRedInt = (int)lroundf(avgRed);
	int avgBlueInt = (int)lroundf(avgBlue);
	if (avgRedInt != 4 || avgBlueInt != 4)
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

void TeamGen_ClearRemindPositions(void) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client)
			continue;
		memset(&ent->client->sess.remindPositionOnMapChange, 0, sizeof(ent->client->sess.remindPositionOnMapChange));
	}
}

// sets people's scores again if they change maps
void TeamGen_RemindPosition(gentity_t *ent) {
	if (!g_vote_teamgen_remindPositions.integer)
		return;

	if (!ent || !ent->client || !ent->client->sess.remindPositionOnMapChange.valid)
		return;

	if (!level.wasRestarted) {
		ent->client->ps.persistant[PERS_SCORE] = ent->client->sess.remindPositionOnMapChange.score;
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va("Your position: %s", NameForPos(ent->client->sess.remindPositionOnMapChange.pos)));
	}

	// clear so that we don't do it again
	memset(&ent->client->sess.remindPositionOnMapChange, 0, sizeof(ent->client->sess.remindPositionOnMapChange));
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
