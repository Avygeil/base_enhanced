#include "g_local.h"
#include "g_database.h"
#include "g_thread.h"
#include "cJSON.h"

// #define DEBUG_GENERATETEAMS // uncomment to set players to be put into teams with z_debugX cvars

#define OLDNOOBRULE

// no-op now -- the structured debug log (teamGenDebugLog_t) replaced it. kept so the ~80 old call sites don't each need touching.
#define TeamGen_DebugPrintf(...)	do {} while (0)

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
	case PLAYERRATING_LOW_X: return 1.1;
	case PLAYERRATING_MID_X: return 1.15;
	case PLAYERRATING_HIGH_X: return 1.2;
	default: return 0.0;
	}
}

ctfPlayerTier_t PlayerTierFromRating(double num) {
	// stupid >= hack to account for imprecision
	if (num >= 1.2 - 0.00001) return PLAYERRATING_HIGH_X;
	if (num >= 1.15 - 0.00001) return PLAYERRATING_MID_X;
	if (num >= 1.1 - 0.00001) return PLAYERRATING_LOW_X;
	if (num >= 1.05 - 0.00001) return PLAYERRATING_HIGH_S;
	if (num >= 1.0 - 0.00001) return PLAYERRATING_MID_S;
	if (num >= 0.95 - 0.00001) return PLAYERRATING_LOW_S;
	if (num >= 0.90 - 0.00001) return PLAYERRATING_HIGH_A;
	if (num >= 0.85 - 0.00001) return PLAYERRATING_MID_A;
	if (num >= 0.80 - 0.00001) return PLAYERRATING_LOW_A;
	if (num >= 0.75 - 0.00001) return PLAYERRATING_HIGH_B;
	if (num >= 0.7 - 0.00001) return PLAYERRATING_MID_B;
	if (num >= 0.65 - 0.00001) return PLAYERRATING_LOW_B;
	if (num >= 0.6 - 0.00001) return PLAYERRATING_HIGH_C;
	if (num >= 0.55 - 0.00001) return PLAYERRATING_MID_C;
	if (num >= 0.5 - 0.00001) return PLAYERRATING_LOW_C;
	if (num >= 0.45 - 0.00001) return PLAYERRATING_HIGH_D;
	if (num >= 0.4 - 0.00001) return PLAYERRATING_MID_D;
	if (num >= 0.35 - 0.00001) return PLAYERRATING_LOW_D;
	if (num >= 0.3 - 0.00001) return PLAYERRATING_HIGH_F;
	if (num >= 0.25 - 0.00001) return PLAYERRATING_MID_F;
	if (num >= 0.2 - 0.00001) return PLAYERRATING_LOW_F;
	if (num >= 0.15 - 0.00001) return PLAYERRATING_HIGH_G;
	if (num >= 0.1 - 0.00001) return PLAYERRATING_MID_G;
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
	case PLAYERRATING_LOW_X: return "^4LOW X";
	case PLAYERRATING_MID_X: return "^4X";
	case PLAYERRATING_HIGH_X: return "^4HIGH X";
	default: return TEAMGEN_UNRATED_STRING;
	}
}

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
	int ttlholdRank;
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
			{
				int temp = team->offenseId1;
				team->offenseId1 = team->offenseId2;
				team->offenseId2 = temp;
			}
			{
				char tempBuf[MAX_NAME_LENGTH] = { 0 };
				Q_strncpyz(tempBuf, team->offense1Name, MAX_NAME_LENGTH);
				Q_strncpyz(team->offense1Name, team->offense2Name, MAX_NAME_LENGTH);
				Q_strncpyz(team->offense2Name, tempBuf, MAX_NAME_LENGTH);
			}
			{
				double temp = team->offense1Rating;
				team->offense1Rating = team->offense2Rating;
				team->offense2Rating = temp;
			}
		}
	}
}

// note: normalize the permutation with NormalizePermutationOfTeams before calling this
static XXH32_hash_t HashPermutationOfTeams(permutationOfTeams_t *t) {
	if (!t)
		return 0;

	// NOT static -- every worker calls this concurrently, and a shared buffer lets two of them
	// interleave writes and hash a mixture of two different splits
	int hashMe[8] = { 0 };

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


#define TEAMGEN_DEBUGLOG_MAX_MILESTONES 200
#define TEAMGEN_DEBUGLOG_MAX_FILE_SIZE_KB 1024

typedef struct {
	char name[MAX_NAME_LENGTH];
	ctfPlayerTier_t tier;
	qboolean pref;
	qboolean avoid;
} teamGenDebugPlayer_t;

typedef struct {
	teamGenDebugPlayer_t base, chase, off1, off2;
} teamGenDebugTeam_t;

// every field any TryTeamPermutation* compares on, so the report can show which ones actually
// decided a ranking. fields an algo doesn't score on stay 0 and the template hides them.
typedef struct {
	int iDiff, numOnPreferredPos, numOnAvoidedPos, topTierImbalance, bottomTierImbalance;
	int offenseDefenseDiff;
	int totalSkill;           // combined strength x10000 (tryhard/semitryhard)
	int totalNumPermutations; // inclusive
	int numSatisfiedCyds, numSatisfiedLgs;
	qboolean satisfiesNoobRule, satisfiesNoobCarryRule;
	// float, not double: display only, and this struct is multiplied by milestones x types x workers
	float team1RawStrength, team2RawStrength;
	float team1RelativeStrength, team2RelativeStrength;
	teamGenDebugTeam_t team1, team2;
} teamGenDebugMilestone_t;

typedef struct {
	int numCandidatesEvaluated;
	int rejectImbalanceCap;
	int rejectGarbageTier;
	int rejectChaseRule;
	int rejectAvoidedHash;
	int rejectNoSatisfiedLgs;
	int rejectNoSatisfiedCyds;
	int numMilestones;
	int numMilestonesTruncated; // "new best" events that occurred after the cap below was already reached
	// which comparator scored this type -- GenerateTeams picks the callback dynamically, so it's not
	// fixed per type. -1 until set.
	int algo;
	teamGenDebugMilestone_t milestones[TEAMGEN_DEBUGLOG_MAX_MILESTONES];
} teamGenDebugLog_t;

typedef enum {
	TEAMGENDEBUGALGO_NONE = -1,
	TEAMGENDEBUGALGO_REGULAR = 0,
	TEAMGENDEBUGALGO_APPEASING,
	TEAMGENDEBUGALGO_FAIREST,
	TEAMGENDEBUGALGO_SEMIAPPEASING,
	TEAMGENDEBUGALGO_FIRSTCHOICE,
	TEAMGENDEBUGALGO_INCLUSIVE,
	TEAMGENDEBUGALGO_SEMITRYHARD,
	TEAMGENDEBUGALGO_TRYHARD
} teamGenDebugAlgo_t;

static void TeamGenDebugLog_FillPlayer(teamGenDebugPlayer_t *out, const permutationPlayer_t *p, ctfPosition_t pos) {
	Q_strncpyz(out->name, p->accountName, sizeof(out->name));
	out->tier = PlayerTierFromRating(p->rating[pos]);
	out->pref = (p->posPrefs.first & (1 << pos)) ? qtrue : qfalse;
	out->avoid = (p->posPrefs.avoid & (1 << pos)) ? qtrue : qfalse;
}

// no-op if debug logging is off for this job, so callers don't need their own check
static void TeamGenDebugLog_RecordMilestone(teamGenDebugLog_t *log, const permutationOfTeams_t *candidate,
	const permutationPlayer_t *team1base, const permutationPlayer_t *team1chase, const permutationPlayer_t *team1offense1, const permutationPlayer_t *team1offense2,
	const permutationPlayer_t *team2base, const permutationPlayer_t *team2chase, const permutationPlayer_t *team2offense1, const permutationPlayer_t *team2offense2) {
	if (!log || !candidate)
		return;

	if (log->numMilestones >= TEAMGEN_DEBUGLOG_MAX_MILESTONES) {
		log->numMilestonesTruncated++;
		return;
	}

	teamGenDebugMilestone_t *m = &log->milestones[log->numMilestones++];
	m->iDiff = candidate->iDiff;
	m->numOnPreferredPos = candidate->numOnPreferredPos;
	m->numOnAvoidedPos = candidate->numOnAvoidedPos;
	m->topTierImbalance = candidate->topTierImbalance;
	m->bottomTierImbalance = candidate->bottomTierImbalance;
	m->offenseDefenseDiff = candidate->offenseDefenseDiff;
	m->totalSkill = candidate->totalSkill;
	m->totalNumPermutations = candidate->totalNumPermutations;
	m->numSatisfiedCyds = candidate->numSatisfiedCyds;
	m->numSatisfiedLgs = candidate->numSatisfiedLgs;
	m->satisfiesNoobRule = candidate->satisfiesNoobRule;
	m->satisfiesNoobCarryRule = candidate->satisfiesNoobCarryRule;
	m->team1RawStrength = (float)candidate->teams[0].rawStrength;
	m->team2RawStrength = (float)candidate->teams[1].rawStrength;
	m->team1RelativeStrength = (float)candidate->teams[0].relativeStrength;
	m->team2RelativeStrength = (float)candidate->teams[1].relativeStrength;
	TeamGenDebugLog_FillPlayer(&m->team1.base, team1base, CTFPOSITION_BASE);
	TeamGenDebugLog_FillPlayer(&m->team1.chase, team1chase, CTFPOSITION_CHASE);
	TeamGenDebugLog_FillPlayer(&m->team1.off1, team1offense1, CTFPOSITION_OFFENSE);
	TeamGenDebugLog_FillPlayer(&m->team1.off2, team1offense2, CTFPOSITION_OFFENSE);
	TeamGenDebugLog_FillPlayer(&m->team2.base, team2base, CTFPOSITION_BASE);
	TeamGenDebugLog_FillPlayer(&m->team2.chase, team2chase, CTFPOSITION_CHASE);
	TeamGenDebugLog_FillPlayer(&m->team2.off1, team2offense1, CTFPOSITION_OFFENSE);
	TeamGenDebugLog_FillPlayer(&m->team2.off2, team2offense2, CTFPOSITION_OFFENSE);
}

// folds a worker's log into the per-type aggregate. only called after every worker has joined.
static void TeamGenDebugLog_Merge(teamGenDebugLog_t *dst, const teamGenDebugLog_t *src) {
	if (!dst || !src)
		return;

	dst->numCandidatesEvaluated += src->numCandidatesEvaluated;
	dst->rejectImbalanceCap += src->rejectImbalanceCap;
	dst->rejectGarbageTier += src->rejectGarbageTier;
	dst->rejectChaseRule += src->rejectChaseRule;
	dst->rejectAvoidedHash += src->rejectAvoidedHash;
	dst->rejectNoSatisfiedLgs += src->rejectNoSatisfiedLgs;
	dst->rejectNoSatisfiedCyds += src->rejectNoSatisfiedCyds;
	dst->numMilestonesTruncated += src->numMilestonesTruncated;

	for (int i = 0; i < src->numMilestones; i++) {
		if (dst->numMilestones >= TEAMGEN_DEBUGLOG_MAX_MILESTONES) {
			dst->numMilestonesTruncated++;
			continue;
		}
		dst->milestones[dst->numMilestones++] = src->milestones[i];
	}
}

// ceiling for auto-detected worker count only; an explicit g_teamgen_workerThreads isn't capped here
#define TEAMGEN_AUTO_WORKERS_MAX 8

// don't split finer than this per worker -- C(8,4) is only 70, and at that size thread create/join
// costs more than the work itself
#define TEAMGEN_MIN_COMBOS_PER_WORKER 16

// everything the search reads that would otherwise be a live global or cvar, captured once on the
// MAIN thread at dispatch. keeps workers off G_UpdateCvars' per-frame rewrite of the vmCvar_t
// structs, and keeps one job internally consistent even if a cvar changes midway through.
typedef struct {
	int			rerollNum;
	double		acdImbalanceCapWithoutReroll;
	double		bImbalanceCapWithoutReroll;
	double		imbalanceCapRaisePerReroll;
	qboolean	noobCheck;
	qboolean	new8PlayerAlgo;
	int			workerNice; // OS nice for this job's workers; speculative work yields to real work
	int			workerThreads; // already resolved to a concrete count; never 0/auto by the time a job sees it
	qboolean	wantDebugLog; // whether this job should produce a debug report
	// everything below is read on coordinator/worker threads too -- as live cvar reads, one flipped
	// mid-generation could make a single job internally inconsistent
	int			gametype;
	qboolean	asyncEnabled;
	qboolean	enableAppeasing;
	qboolean	enableInclusive;
	qboolean	enableFirstChoice;
	qboolean	aDietB;
	qboolean	iterate;
	qboolean	sumOfSquaresTiebreaker;
} teamGenTunables_t;

static void TeamGen_CaptureTunables(teamGenTunables_t *out) {
	out->rerollNum = rerollNum;
	out->acdImbalanceCapWithoutReroll = g_vote_teamgen_acdImbalanceCapWithoutReroll.value;
	out->bImbalanceCapWithoutReroll = g_vote_teamgen_bImbalanceCapWithoutReroll.value;
	out->imbalanceCapRaisePerReroll = g_vote_teamgen_imbalanceCapRaisePerReroll.value;
	out->noobCheck = g_vote_teamgen_noobCheck.integer ? qtrue : qfalse;
	out->new8PlayerAlgo = g_vote_teamgen_new8PlayerAlgo.integer ? qtrue : qfalse;
	out->workerNice = g_teamgen_workerThreadNice.integer;
	// resolve "0 = auto" here on the main thread; PermuteTeams_Threaded runs ~100x per generation, so
	// leaving it unresolved would re-derive it from live state that whole time.
	// capped at TEAMGEN_AUTO_WORKERS_MAX because this box usually shares a host and precompute runs
	// nearly continuously; past ~8 it's diminishing returns anyway (7.1x at 15 workers on 16 players).
	// an explicit cvar value is honored uncapped.
	out->workerThreads = g_teamgen_workerThreads.integer;
	if (out->workerThreads <= 0) {
		out->workerThreads = GThread_HardwareConcurrency() - 1;
		if (out->workerThreads > TEAMGEN_AUTO_WORKERS_MAX)
			out->workerThreads = TEAMGEN_AUTO_WORKERS_MAX;
	}
	if (out->workerThreads < 1)
		out->workerThreads = 1;
	out->wantDebugLog = g_teamgen_debugLog.integer ? qtrue : qfalse;
	out->gametype = g_gametype.integer;
	out->asyncEnabled = g_teamgen_asyncEnabled.integer ? qtrue : qfalse;
	out->enableAppeasing = g_vote_teamgen_enableAppeasing.integer ? qtrue : qfalse;
	out->enableInclusive = g_vote_teamgen_enableInclusive.integer ? qtrue : qfalse;
	out->enableFirstChoice = g_vote_teamgen_enableFirstChoice.integer ? qtrue : qfalse;
	out->aDietB = g_vote_teamgen_aDietB.integer ? qtrue : qfalse;
	out->iterate = g_vote_teamgen_iterate.integer ? qtrue : qfalse;
	out->sumOfSquaresTiebreaker = g_vote_teamgen_sumOfSquaresTiebreaker.integer ? qtrue : qfalse;
}

typedef struct {
	permutationOfTeams_t *best;
	void *callback;
	qboolean enforceChaseRule;
	int numEligible;
	teamGenTunables_t tunables;
	uint64_t numPermutations;
	list_t *avoidedHashesList;
	qboolean banAvoidedPositions;
	int type;
	qboolean enforceImbalanceCaps;
	// explicit-state PRNG, seeded per PermuteTeams() call -- never rand()/srand() from here, this runs
	// on worker threads
	gprng_t rng;
	// NULL unless debug logging is on. each worker gets its own; merged after the reduction.
	teamGenDebugLog_t *debugLog;
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

// ============================================================================
// IsBetter_* comparators
//
// each is the "isBest" chain that used to live inline in its TryTeamPermutation* function,
// extracted verbatim. PermuteTeams_Threaded reduces N workers' local-bests with the same predicate
// the search used -- if they diverged, parallel results could differ from serial. callers only
// populate the fields their own comparator reads, so adding a field means auditing its callers.
// ============================================================================

static qboolean IsBetter_Regular(const permutationOfTeams_t *candidate, const permutationOfTeams_t *best) {
	return candidate->iDiff < best->iDiff ||
		(candidate->iDiff == best->iDiff && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
		(candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos) ||
		(candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
		(candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance)
		? qtrue : qfalse;
}

// fairest has 3 chains depending on the 8-player-algo/noob-check cvars; new8PlayerAlgo off reduces
// to IsBetter_Regular, so it's reused
static qboolean IsBetter_Fairest(const permutationOfTeams_t *candidate, const permutationOfTeams_t *best, qboolean new8PlayerAlgo, qboolean noobCheck) {
	if (!new8PlayerAlgo)
		return IsBetter_Regular(candidate, best);

	if (noobCheck) {
		return (candidate->satisfiesNoobRule && !best->satisfiesNoobRule) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule && !best->satisfiesNoobCarryRule) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff < best->iDiff) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff < best->offenseDefenseDiff) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff == best->offenseDefenseDiff && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff == best->offenseDefenseDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos)
			? qtrue : qfalse;
	}

	return candidate->iDiff < best->iDiff ||
		(candidate->iDiff == best->iDiff && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
		(candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance) ||
		(candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff < best->offenseDefenseDiff) ||
		(candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff == best->offenseDefenseDiff && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
		(candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff == best->offenseDefenseDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos)
		? qtrue : qfalse;
}

static qboolean IsBetter_SemiAppeasing(const permutationOfTeams_t *candidate, const permutationOfTeams_t *best, qboolean is8Player) {
	return candidate->numSatisfiedCyds > best->numSatisfiedCyds ||
		(candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff < best->iDiff) ||
		(candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff == best->iDiff && candidate->numSatisfiedLgs > best->numSatisfiedLgs) ||
		(candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff == best->iDiff && candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
		(candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff == best->iDiff && candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos) ||
		(candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff == best->iDiff && candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
		(candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff == best->iDiff && candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance) ||
		(candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff == best->iDiff && candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && (is8Player && candidate->offenseDefenseDiff < best->offenseDefenseDiff))
		? qtrue : qfalse;
}

static qboolean IsBetter_FirstChoice(const permutationOfTeams_t *candidate, const permutationOfTeams_t *best, qboolean is8Player) {
	return candidate->numSatisfiedLgs > best->numSatisfiedLgs ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff < best->iDiff) ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos) ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance) ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && (is8Player && candidate->offenseDefenseDiff < best->offenseDefenseDiff))
		? qtrue : qfalse;
}

static qboolean IsBetter_Inclusive(const permutationOfTeams_t *candidate, const permutationOfTeams_t *best) {
	return candidate->totalNumPermutations < best->totalNumPermutations ||
		(candidate->totalNumPermutations == best->totalNumPermutations && candidate->numSatisfiedCyds > best->numSatisfiedCyds) ||
		(candidate->totalNumPermutations == best->totalNumPermutations && candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff < best->iDiff) ||
		(candidate->totalNumPermutations == best->totalNumPermutations && candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
		(candidate->totalNumPermutations == best->totalNumPermutations && candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos) ||
		(candidate->totalNumPermutations == best->totalNumPermutations && candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
		(candidate->totalNumPermutations == best->totalNumPermutations && candidate->numSatisfiedCyds == best->numSatisfiedCyds && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance)
		? qtrue : qfalse;
}

// candidate stores its iTotal in totalSkill; best's is recomputed from its team raw strengths,
// exactly as the original inline code did
static qboolean IsBetter_SemiTryhard(const permutationOfTeams_t *candidate, const permutationOfTeams_t *best, qboolean is8Player) {
	int iCurrentBestCombinedStrength = (int)round((best->teams[0].rawStrength + best->teams[1].rawStrength) * 10000);

	if (is8Player) {
		return (candidate->satisfiesNoobRule && !best->satisfiesNoobRule) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule && !best->satisfiesNoobCarryRule) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff < best->iDiff) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->totalSkill > iCurrentBestCombinedStrength) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->iDiff == best->iDiff && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->offenseDefenseDiff < best->offenseDefenseDiff)
			? qtrue : qfalse;
	}

	return candidate->iDiff < best->iDiff ||
		(candidate->iDiff == best->iDiff && candidate->totalSkill > iCurrentBestCombinedStrength) ||
		(candidate->iDiff == best->iDiff && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
		(candidate->iDiff == best->iDiff && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos) ||
		(candidate->iDiff == best->iDiff && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
		(candidate->iDiff == best->iDiff && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance)
		? qtrue : qfalse;
}

static qboolean IsBetter_Tryhard(const permutationOfTeams_t *candidate, const permutationOfTeams_t *best, qboolean is8Player, qboolean noobCheck) {
	int iCurrentBestCombinedStrength = (int)round((best->teams[0].rawStrength + best->teams[1].rawStrength) * 10000);

	if (is8Player && noobCheck) {
		return (candidate->satisfiesNoobRule && !best->satisfiesNoobRule) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule && !best->satisfiesNoobCarryRule) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->totalSkill > iCurrentBestCombinedStrength) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff < best->iDiff) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff < best->offenseDefenseDiff) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff == best->offenseDefenseDiff && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
			(candidate->satisfiesNoobRule == best->satisfiesNoobRule && candidate->satisfiesNoobCarryRule == best->satisfiesNoobCarryRule && candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff == best->offenseDefenseDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos)
			? qtrue : qfalse;
	}

	if (is8Player) {
		// new8PlayerAlgo without noobCheck
		return candidate->totalSkill > iCurrentBestCombinedStrength ||
			(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff < best->iDiff) ||
			(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
			(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance) ||
			(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff < best->offenseDefenseDiff) ||
			(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff == best->offenseDefenseDiff && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
			(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance == best->topTierImbalance && candidate->offenseDefenseDiff == best->offenseDefenseDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos)
			? qtrue : qfalse;
	}

	return candidate->totalSkill > iCurrentBestCombinedStrength ||
		(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff < best->iDiff) ||
		(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
		(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos) ||
		(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
		(candidate->totalSkill == iCurrentBestCombinedStrength && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance)
		? qtrue : qfalse;
}

static qboolean IsBetter_Appeasing(const permutationOfTeams_t *candidate, const permutationOfTeams_t *best) {
	return candidate->numSatisfiedLgs > best->numSatisfiedLgs ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff < best->iDiff) ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos > best->numOnPreferredPos) ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos < best->numOnAvoidedPos) ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance < best->bottomTierImbalance) ||
		(candidate->numSatisfiedLgs == best->numSatisfiedLgs && candidate->iDiff == best->iDiff && candidate->numOnPreferredPos == best->numOnPreferredPos && candidate->numOnAvoidedPos == best->numOnAvoidedPos && candidate->bottomTierImbalance == best->bottomTierImbalance && candidate->topTierImbalance < best->topTierImbalance)
		? qtrue : qfalse;
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
	if (context->debugLog) context->debugLog->numCandidatesEvaluated++;

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
		if (!context->tunables.rerollNum && context->tunables.acdImbalanceCapWithoutReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
		else if (context->tunables.rerollNum > 0 && context->tunables.imbalanceCapRaisePerReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll + (context->tunables.rerollNum * context->tunables.imbalanceCapRaisePerReroll);
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		if (context->debugLog) context->debugLog->rejectGarbageTier++;
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team1ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team1ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 3); // B/B+ chases can only chase 3 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);
			
			qboolean invalid;
			if (team2ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team2ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 3); // B/B+ chases can only chase 4 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
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
	// hoisted out so the report can record what this algo scored on; no comparator reads candidate->teams
	permutationOfTeams_t candidate = { 0 };
	candidate.teams[0].rawStrength = team1RawStrength;
	candidate.teams[1].rawStrength = team2RawStrength;
	candidate.teams[0].relativeStrength = team1RelativeStrength;
	candidate.teams[1].relativeStrength = team2RelativeStrength;
	{
		candidate.iDiff = iDiff;
		candidate.numOnPreferredPos = numOnPreferredPos;
		candidate.numOnAvoidedPos = numOnAvoidedPos;
		candidate.bottomTierImbalance = bottomTierImbalance;
		candidate.topTierImbalance = topTierImbalance;
		isBest = IsBetter_Regular(&candidate, context->best);
		if (isBest)
			TeamGen_DebugPrintf(" <font style=\"background-color: yellow;\">best so far</font><br/>");
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
				if (context->debugLog) context->debugLog->rejectAvoidedHash++;
				--context->numPermutations;
				return;
			}
		}

		TeamGenDebugLog_RecordMilestone(context->debugLog, &candidate,
			team1base, team1chase, team1offense1, team1offense2, team2base, team2chase, team2offense1, team2offense2);

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
		context->best->teams[0].baseRating = team1base->rating[CTFPOSITION_BASE];
		context->best->teams[0].chaseRating = team1chase->rating[CTFPOSITION_CHASE];
		context->best->teams[0].offense1Rating = team1offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[0].offense2Rating = team1offense2->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].baseRating = team2base->rating[CTFPOSITION_BASE];
		context->best->teams[1].chaseRating = team2chase->rating[CTFPOSITION_CHASE];
		context->best->teams[1].offense1Rating = team2offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].offense2Rating = team2offense2->rating[CTFPOSITION_OFFENSE];

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
	if (context->debugLog) context->debugLog->numCandidatesEvaluated++;

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

	TeamGen_DebugPrintf("Appeasing:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
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
		if (context->debugLog) context->debugLog->rejectGarbageTier++;
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team1ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team1ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 3); // B/B+ chases can only chase 3 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team2ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team2ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 3); // B/B+ chases can only chase 4 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}
	}

	int numSatisfiedLgs = 0;
	if (team1base->notOnFirstChoiceInAbc && (team1base->posPrefs.first & (1 << CTFPOSITION_BASE))) ++numSatisfiedLgs;
	if (team1chase->notOnFirstChoiceInAbc && (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE))) ++numSatisfiedLgs;
	if (team1offense1->notOnFirstChoiceInAbc && (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (team1offense2->notOnFirstChoiceInAbc && (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (team2base->notOnFirstChoiceInAbc && (team2base->posPrefs.first & (1 << CTFPOSITION_BASE))) ++numSatisfiedLgs;
	if (team2chase->notOnFirstChoiceInAbc && (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE))) ++numSatisfiedLgs;
	if (team2offense1->notOnFirstChoiceInAbc && (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (team2offense2->notOnFirstChoiceInAbc && (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (!numSatisfiedLgs) {
		if (context->debugLog) context->debugLog->rejectNoSatisfiedLgs++;
		return;
	}

	qboolean isBest = qfalse;

	// this permutation will be favored over the previous permutation if:
	// - the number of lgs (people not on first choice in a/b/c but first choice here) is higher
	// - it is fairer
	// - it is equally fair, but has more people on preferred pos
	// - it is equally fair and has an equal number of people on preferred pos, but has fewer people on avoided pos
	// - it is equally fair and has an equal number of people on preferred and avoided pos, but has better balance of bottom tier players
	// - it is equally fair and has an equal number of people on preferred and avoided pos and equal balance of bottom tier players, but has better balance of top tier players
	// hoisted out so the report can record what this algo scored on; no comparator reads candidate->teams
	permutationOfTeams_t candidate = { 0 };
	candidate.teams[0].rawStrength = team1RawStrength;
	candidate.teams[1].rawStrength = team2RawStrength;
	candidate.teams[0].relativeStrength = team1RelativeStrength;
	candidate.teams[1].relativeStrength = team2RelativeStrength;
	{
		candidate.numSatisfiedLgs = numSatisfiedLgs;
		candidate.iDiff = iDiff;
		candidate.numOnPreferredPos = numOnPreferredPos;
		candidate.numOnAvoidedPos = numOnAvoidedPos;
		candidate.bottomTierImbalance = bottomTierImbalance;
		candidate.topTierImbalance = topTierImbalance;
		isBest = IsBetter_Appeasing(&candidate, context->best);
		if (isBest)
			TeamGen_DebugPrintf(" <font style=\"background-color: yellow;\">best so far</font><br/>");
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
				if (context->debugLog) context->debugLog->rejectAvoidedHash++;
				--context->numPermutations;
				return;
			}
		}

		TeamGenDebugLog_RecordMilestone(context->debugLog, &candidate,
			team1base, team1chase, team1offense1, team1offense2, team2base, team2chase, team2offense1, team2offense2);

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
		context->best->teams[0].baseRating = team1base->rating[CTFPOSITION_BASE];
		context->best->teams[0].chaseRating = team1chase->rating[CTFPOSITION_CHASE];
		context->best->teams[0].offense1Rating = team1offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[0].offense2Rating = team1offense2->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].baseRating = team2base->rating[CTFPOSITION_BASE];
		context->best->teams[1].chaseRating = team2chase->rating[CTFPOSITION_CHASE];
		context->best->teams[1].offense1Rating = team2offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].offense2Rating = team2offense2->rating[CTFPOSITION_OFFENSE];
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
		context->best->numSatisfiedLgs = numSatisfiedLgs;
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
	if (context->debugLog) context->debugLog->numCandidatesEvaluated++;

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

	// noob rule and noob carry rule
	qboolean satisfiesNoobRule = qtrue;
	qboolean satisfiesNoobCarryRule = qtrue;
	if (qfalse && context->numEligible == 8 && context->tunables.noobCheck) {
		double ratings[8] = {
			team1base->rating[CTFPOSITION_BASE],
			team1chase->rating[CTFPOSITION_CHASE],
			team1offense1->rating[CTFPOSITION_OFFENSE],
			team1offense2->rating[CTFPOSITION_OFFENSE],

			team2base->rating[CTFPOSITION_BASE],
			team2chase->rating[CTFPOSITION_CHASE],
			team2offense1->rating[CTFPOSITION_OFFENSE],
			team2offense2->rating[CTFPOSITION_OFFENSE]
		};

#if 0
		const char *names[8] = {
			team1base->accountName,     team1chase->accountName,
			team1offense1->accountName, team1offense2->accountName,

			team2base->accountName,     team2chase->accountName,
			team2offense1->accountName, team2offense2->accountName
		};
#endif

		int tiers[8];
		for (int i = 0; i < 8; ++i)
			tiers[i] = PlayerTierFromRating(ratings[i]);

		int minTier = 99, secondMinTier = 99;
		int minIdx = -1, secondMinIdx = -1;

		for (int i = 0; i < 8; ++i) {
			if (tiers[i] < minTier) {
				secondMinTier = minTier;      secondMinIdx = minIdx;
				minTier = tiers[i];     minIdx = i;
			}
			else if (tiers[i] < secondMinTier) {
				secondMinTier = tiers[i];     secondMinIdx = i;
			}
		}

		// a noob situation is where there are 7 goodies and 1 baddy
		qboolean noobSituation = (minTier <= PLAYERRATING_HIGH_C) && (secondMinTier - minTier >= 3 || (secondMinTier - minTier >= 2 && secondMinTier >= PLAYERRATING_MID_B));

#if 0
		TeamGen_DebugPrintf(
			"<font color=purple>Noob-check: worst <b>%s</b> (%s) &nbsp;&nbsp;second-worst <b>%s</b> (%s) &nbsp;&nbsp;gap %d tier(s) \x97 %s</font><br/>",
			names[minIdx], PlayerRatingToStringHTML(minTier),
			names[secondMinIdx], PlayerRatingToStringHTML(secondMinTier),
			secondMinTier - minTier,
			noobSituation ? "NOOB SITUATION" : "not a noob situation");
#endif

		// noob rule: try to get the noob dude on equal or higher % team
		if (noobSituation && iDiff) {

			int weakerTeam = (team1RelativeStrength < team2RelativeStrength) ? 0 : 1;
			int playersTeam = (minIdx < 4) ? 0 : 1;   /* idx<4 ? team 0 */

#if 0
			TeamGen_DebugPrintf("<font color=purple>Weaker team is <b>team %d</b>; worst player is on <b>team %d</b></font><br/>", weakerTeam + 1, playersTeam + 1);
#endif

			if (playersTeam == weakerTeam) {
#ifdef OLDNOOBRULE
				satisfiesNoobRule = qfalse;
#endif
#if 0
				TeamGen_DebugPrintf("<font color=red>Noob rule NOT satisfied (worst player on weaker team)</font><br/>");
#endif
			}
			else {
#if 0
				TeamGen_DebugPrintf("<font color=darkgreen>Noob rule satisfied (worst player on stronger/equal team)</font><br/>");
#endif
			}
		}

		// noob carry rule: try to get the noob dude with a strong offense player
		if (noobSituation) {
			// figure out the best offense players
			int offTier[8] = {
				PlayerTierFromRating(team1base->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team1chase->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]),

				PlayerTierFromRating(team2base->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team2chase->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE])
			};

			int ttlRank[8] = {
				team1base->ttlholdRank,      team1chase->ttlholdRank,
				team1offense1->ttlholdRank,  team1offense2->ttlholdRank,

				team2base->ttlholdRank,      team2chase->ttlholdRank,
				team2offense1->ttlholdRank,  team2offense2->ttlholdRank
			};

			int bestOffTier = PLAYERRATING_UNRATED;
			for (int i = 0; i < 8; ++i)
				if (offTier[i] > bestOffTier) bestOffTier = offTier[i];

			int candidateMask = 0;
			for (int i = 0; i < 8; ++i) {
				if (offTier[i] >= PLAYERRATING_HIGH_A ||
					(offTier[i] >= PLAYERRATING_LOW_A && offTier[i] >= bestOffTier - 1))
					candidateMask |= (1 << i);
			}

			int numCandidates = 0;
			for (int i = 0; i < 8; ++i)
				if (candidateMask & (1 << i)) ++numCandidates;

			// trim down the pool of carries
			if (numCandidates > /*2*/1) {
				int worstIdx = -1, worstRank = -1;
				for (int i = 0; i < 8; ++i)
					if (candidateMask & (1 << i))
						if (ttlRank[i] > worstRank) { worstRank = ttlRank[i]; worstIdx = i; }
				if (worstIdx != -1) {
					candidateMask &= ~(1 << worstIdx);
					--numCandidates;
				}
			}

			// on his team?
			int noobTeam = (minIdx < 4) ? 0 : 1;
			int offIdx1 = (noobTeam == 0) ? 2 : 6;
			int offIdx2 = offIdx1 + 1;

			qboolean carryPresent = (candidateMask & (1 << offIdx1)) || (candidateMask & (1 << offIdx2));

#if 0
			TeamGen_DebugPrintf(
				"<font color=purple>Noob-carry check: candidate carries = %d, %s on noob\x92s team</font><br/>",
				numCandidates,
				carryPresent ? "carry present" : "NO carry present"
			);
#endif

#ifdef OLDNOOBRULE
			if (!carryPresent) {
				satisfiesNoobCarryRule = qfalse;
			}
#else
			if (carryPresent) {
				satisfiesNoobRule = qtrue;
			}
			else {
				satisfiesNoobRule = qfalse;
			}
#endif
		}
	}

	TeamGen_DebugPrintf("Regular:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d noob rule, %d noob carry rule, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
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
		satisfiesNoobRule,
		satisfiesNoobCarryRule,
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
		if (!context->tunables.rerollNum && context->tunables.acdImbalanceCapWithoutReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
		else if (context->tunables.rerollNum > 0 && context->tunables.imbalanceCapRaisePerReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll + (context->tunables.rerollNum * context->tunables.imbalanceCapRaisePerReroll);
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		if (context->debugLog) context->debugLog->rejectGarbageTier++;
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team1ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team1ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 3); // B/B+ chases can only chase 3 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team2ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team2ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 3); // B/B+ chases can only chase 4 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}
	}

	qboolean isBest = qfalse;
	// hoisted out so the report can record what this algo scored on; no comparator reads candidate->teams
	permutationOfTeams_t candidate = { 0 };
	candidate.teams[0].rawStrength = team1RawStrength;
	candidate.teams[1].rawStrength = team2RawStrength;
	candidate.teams[0].relativeStrength = team1RelativeStrength;
	candidate.teams[1].relativeStrength = team2RelativeStrength;
	{
		candidate.iDiff = iDiff;
		candidate.numOnPreferredPos = numOnPreferredPos;
		candidate.numOnAvoidedPos = numOnAvoidedPos;
		candidate.bottomTierImbalance = bottomTierImbalance;
		candidate.topTierImbalance = topTierImbalance;
		candidate.offenseDefenseDiff = offenseDefenseDiff;
		candidate.satisfiesNoobRule = satisfiesNoobRule;
		candidate.satisfiesNoobCarryRule = satisfiesNoobCarryRule;
		isBest = IsBetter_Fairest(&candidate, context->best,
			(context->numEligible == 8 && context->tunables.new8PlayerAlgo) ? qtrue : qfalse,
			context->tunables.noobCheck ? qtrue : qfalse);
		if (isBest)
			TeamGen_DebugPrintf(" <font style=\"background-color: yellow;\">best so far</font><br/>");
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
				if (context->debugLog) context->debugLog->rejectAvoidedHash++;
				--context->numPermutations;
				return;
			}
		}

		TeamGenDebugLog_RecordMilestone(context->debugLog, &candidate,
			team1base, team1chase, team1offense1, team1offense2, team2base, team2chase, team2offense1, team2offense2);

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		context->best->satisfiesNoobRule = satisfiesNoobRule;
		context->best->satisfiesNoobCarryRule = satisfiesNoobCarryRule;
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
		context->best->teams[0].baseRating = team1base->rating[CTFPOSITION_BASE];
		context->best->teams[0].chaseRating = team1chase->rating[CTFPOSITION_CHASE];
		context->best->teams[0].offense1Rating = team1offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[0].offense2Rating = team1offense2->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].baseRating = team2base->rating[CTFPOSITION_BASE];
		context->best->teams[1].chaseRating = team2chase->rating[CTFPOSITION_CHASE];
		context->best->teams[1].offense1Rating = team2offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].offense2Rating = team2offense2->rating[CTFPOSITION_OFFENSE];
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
	if (context->debugLog) context->debugLog->numCandidatesEvaluated++;

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
		if (!context->tunables.rerollNum && context->tunables.acdImbalanceCapWithoutReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
		else if (context->tunables.rerollNum > 0 && context->tunables.imbalanceCapRaisePerReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll + (context->tunables.rerollNum * context->tunables.imbalanceCapRaisePerReroll);
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		if (context->debugLog) context->debugLog->rejectGarbageTier++;
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team1ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team1ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 3); // B/B+ chases can only chase 3 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team2ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team2ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 3); // B/B+ chases can only chase 4 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
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
		if (context->debugLog) context->debugLog->rejectNoSatisfiedCyds++;
		return;
	}

	int numSatisfiedLgs = 0;
	if (team1base->notOnFirstChoiceInAbc && (team1base->posPrefs.first & (1 << CTFPOSITION_BASE))) ++numSatisfiedLgs;
	if (team1chase->notOnFirstChoiceInAbc && (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE))) ++numSatisfiedLgs;
	if (team1offense1->notOnFirstChoiceInAbc && (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (team1offense2->notOnFirstChoiceInAbc && (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (team2base->notOnFirstChoiceInAbc && (team2base->posPrefs.first & (1 << CTFPOSITION_BASE))) ++numSatisfiedLgs;
	if (team2chase->notOnFirstChoiceInAbc && (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE))) ++numSatisfiedLgs;
	if (team2offense1->notOnFirstChoiceInAbc && (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (team2offense2->notOnFirstChoiceInAbc && (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;

	qboolean isBest = qfalse;
	// hoisted out so the report can record what this algo scored on; no comparator reads candidate->teams
	permutationOfTeams_t candidate = { 0 };
	candidate.teams[0].rawStrength = team1RawStrength;
	candidate.teams[1].rawStrength = team2RawStrength;
	candidate.teams[0].relativeStrength = team1RelativeStrength;
	candidate.teams[1].relativeStrength = team2RelativeStrength;
	{
		candidate.numSatisfiedCyds = numSatisfiedCyds;
		candidate.numSatisfiedLgs = numSatisfiedLgs;
		candidate.iDiff = iDiff;
		candidate.numOnPreferredPos = numOnPreferredPos;
		candidate.numOnAvoidedPos = numOnAvoidedPos;
		candidate.bottomTierImbalance = bottomTierImbalance;
		candidate.topTierImbalance = topTierImbalance;
		candidate.offenseDefenseDiff = offenseDefenseDiff;
		isBest = IsBetter_SemiAppeasing(&candidate, context->best, context->numEligible == 8 ? qtrue : qfalse);
		if (isBest)
			TeamGen_DebugPrintf(" <font style=\"background-color: yellow;\">best so far</font><br/>");
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
				if (context->debugLog) context->debugLog->rejectAvoidedHash++;
				--context->numPermutations;
				return;
			}
		}

		TeamGenDebugLog_RecordMilestone(context->debugLog, &candidate,
			team1base, team1chase, team1offense1, team1offense2, team2base, team2chase, team2offense1, team2offense2);

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
		context->best->teams[0].baseRating = team1base->rating[CTFPOSITION_BASE];
		context->best->teams[0].chaseRating = team1chase->rating[CTFPOSITION_CHASE];
		context->best->teams[0].offense1Rating = team1offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[0].offense2Rating = team1offense2->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].baseRating = team2base->rating[CTFPOSITION_BASE];
		context->best->teams[1].chaseRating = team2chase->rating[CTFPOSITION_CHASE];
		context->best->teams[1].offense1Rating = team2offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].offense2Rating = team2offense2->rating[CTFPOSITION_OFFENSE];
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
		context->best->numSatisfiedLgs = numSatisfiedLgs;
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
	if (context->debugLog) context->debugLog->numCandidatesEvaluated++;

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
		if (!context->tunables.rerollNum && context->tunables.acdImbalanceCapWithoutReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
		else if (context->tunables.rerollNum > 0 && context->tunables.imbalanceCapRaisePerReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll + (context->tunables.rerollNum * context->tunables.imbalanceCapRaisePerReroll);
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		if (context->debugLog) context->debugLog->rejectGarbageTier++;
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team1ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team1ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 3); // B/B+ chases can only chase 3 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team2ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team2ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 3); // B/B+ chases can only chase 4 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}
	}

	int numSatisfiedLgs = 0;
	if (team1base->notOnFirstChoiceInAbc && (team1base->posPrefs.first & (1 << CTFPOSITION_BASE))) ++numSatisfiedLgs;
	if (team1chase->notOnFirstChoiceInAbc && (team1chase->posPrefs.first & (1 << CTFPOSITION_CHASE))) ++numSatisfiedLgs;
	if (team1offense1->notOnFirstChoiceInAbc && (team1offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (team1offense2->notOnFirstChoiceInAbc && (team1offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (team2base->notOnFirstChoiceInAbc && (team2base->posPrefs.first & (1 << CTFPOSITION_BASE))) ++numSatisfiedLgs;
	if (team2chase->notOnFirstChoiceInAbc && (team2chase->posPrefs.first & (1 << CTFPOSITION_CHASE))) ++numSatisfiedLgs;
	if (team2offense1->notOnFirstChoiceInAbc && (team2offense1->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (team2offense2->notOnFirstChoiceInAbc && (team2offense2->posPrefs.first & (1 << CTFPOSITION_OFFENSE))) ++numSatisfiedLgs;
	if (!numSatisfiedLgs) {
		if (context->debugLog) context->debugLog->rejectNoSatisfiedLgs++;
		return;
	}

	qboolean isBest = qfalse;
	// hoisted out so the report can record what this algo scored on; no comparator reads candidate->teams
	permutationOfTeams_t candidate = { 0 };
	candidate.teams[0].rawStrength = team1RawStrength;
	candidate.teams[1].rawStrength = team2RawStrength;
	candidate.teams[0].relativeStrength = team1RelativeStrength;
	candidate.teams[1].relativeStrength = team2RelativeStrength;
	{
		candidate.numSatisfiedLgs = numSatisfiedLgs;
		candidate.iDiff = iDiff;
		candidate.numOnPreferredPos = numOnPreferredPos;
		candidate.numOnAvoidedPos = numOnAvoidedPos;
		candidate.bottomTierImbalance = bottomTierImbalance;
		candidate.topTierImbalance = topTierImbalance;
		candidate.offenseDefenseDiff = offenseDefenseDiff;
		isBest = IsBetter_FirstChoice(&candidate, context->best, context->numEligible == 8 ? qtrue : qfalse);
		if (isBest)
			TeamGen_DebugPrintf(" <font style=\"background-color: yellow;\">best so far</font><br/>");
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
				if (context->debugLog) context->debugLog->rejectAvoidedHash++;
				--context->numPermutations;
				return;
			}
		}

		TeamGenDebugLog_RecordMilestone(context->debugLog, &candidate,
			team1base, team1chase, team1offense1, team1offense2, team2base, team2chase, team2offense1, team2offense2);

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
		context->best->teams[0].baseRating = team1base->rating[CTFPOSITION_BASE];
		context->best->teams[0].chaseRating = team1chase->rating[CTFPOSITION_CHASE];
		context->best->teams[0].offense1Rating = team1offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[0].offense2Rating = team1offense2->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].baseRating = team2base->rating[CTFPOSITION_BASE];
		context->best->teams[1].chaseRating = team2chase->rating[CTFPOSITION_CHASE];
		context->best->teams[1].offense1Rating = team2offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].offense2Rating = team2offense2->rating[CTFPOSITION_OFFENSE];
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
		context->best->numSatisfiedLgs = numSatisfiedLgs;
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
	if (context->debugLog) context->debugLog->numCandidatesEvaluated++;

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
		if (!context->tunables.rerollNum && context->tunables.acdImbalanceCapWithoutReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
		else if (context->tunables.rerollNum > 0 && context->tunables.imbalanceCapRaisePerReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll + (context->tunables.rerollNum * context->tunables.imbalanceCapRaisePerReroll);
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		if (context->debugLog) context->debugLog->rejectGarbageTier++;
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team1ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team1ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 3); // B/B+ chases can only chase 3 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team2ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team2ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 3); // B/B+ chases can only chase 4 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
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
	qboolean isBest = qfalse;
	// hoisted out so the report can record what this algo scored on; no comparator reads candidate->teams
	permutationOfTeams_t candidate = { 0 };
	candidate.teams[0].rawStrength = team1RawStrength;
	candidate.teams[1].rawStrength = team2RawStrength;
	candidate.teams[0].relativeStrength = team1RelativeStrength;
	candidate.teams[1].relativeStrength = team2RelativeStrength;
	{
		candidate.totalNumPermutations = totalNumPermutations;
		candidate.numSatisfiedCyds = numSatisfiedCyds;
		candidate.iDiff = iDiff;
		candidate.numOnPreferredPos = numOnPreferredPos;
		candidate.numOnAvoidedPos = numOnAvoidedPos;
		candidate.bottomTierImbalance = bottomTierImbalance;
		candidate.topTierImbalance = topTierImbalance;
		isBest = IsBetter_Inclusive(&candidate, context->best);
	}

	if (isBest) {
		TeamGen_DebugPrintf(" <font style=\"background-color: yellow;\">best so far</font><br/>");

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
				if (context->debugLog) context->debugLog->rejectAvoidedHash++;
				--context->numPermutations;
				return;
			}
		}

		TeamGenDebugLog_RecordMilestone(context->debugLog, &candidate,
			team1base, team1chase, team1offense1, team1offense2, team2base, team2chase, team2offense1, team2offense2);

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
		context->best->teams[0].baseRating = team1base->rating[CTFPOSITION_BASE];
		context->best->teams[0].chaseRating = team1chase->rating[CTFPOSITION_CHASE];
		context->best->teams[0].offense1Rating = team1offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[0].offense2Rating = team1offense2->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].baseRating = team2base->rating[CTFPOSITION_BASE];
		context->best->teams[1].chaseRating = team2chase->rating[CTFPOSITION_CHASE];
		context->best->teams[1].offense1Rating = team2offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].offense2Rating = team2offense2->rating[CTFPOSITION_OFFENSE];
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
	if (context->debugLog) context->debugLog->numCandidatesEvaluated++;

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

	// noob rule and noob carry rule
	qboolean satisfiesNoobRule = qtrue;
	qboolean satisfiesNoobCarryRule = qtrue;
	if (context->numEligible == 8 && context->tunables.noobCheck) {
		double ratings[8] = {
			team1base->rating[CTFPOSITION_BASE],
			team1chase->rating[CTFPOSITION_CHASE],
			team1offense1->rating[CTFPOSITION_OFFENSE],
			team1offense2->rating[CTFPOSITION_OFFENSE],

			team2base->rating[CTFPOSITION_BASE],
			team2chase->rating[CTFPOSITION_CHASE],
			team2offense1->rating[CTFPOSITION_OFFENSE],
			team2offense2->rating[CTFPOSITION_OFFENSE]
		};

#if 0
		const char *names[8] = {
			team1base->accountName,     team1chase->accountName,
			team1offense1->accountName, team1offense2->accountName,

			team2base->accountName,     team2chase->accountName,
			team2offense1->accountName, team2offense2->accountName
		};
#endif

		int tiers[8];
		for (int i = 0; i < 8; ++i)
			tiers[i] = PlayerTierFromRating(ratings[i]);

		int minTier = 99, secondMinTier = 99;
		int minIdx = -1, secondMinIdx = -1;

		for (int i = 0; i < 8; ++i) {
			if (tiers[i] < minTier) {
				secondMinTier = minTier;      secondMinIdx = minIdx;
				minTier = tiers[i];     minIdx = i;
			}
			else if (tiers[i] < secondMinTier) {
				secondMinTier = tiers[i];     secondMinIdx = i;
			}
		}

		// a noob situation is where there are 7 goodies and 1 baddy
		qboolean noobSituation = (minTier <= PLAYERRATING_HIGH_C) && (secondMinTier - minTier >= 3 || (secondMinTier - minTier >= 2 && secondMinTier >= PLAYERRATING_MID_B));

#if 0
		TeamGen_DebugPrintf(
			"<font color=purple>Noob-check: worst <b>%s</b> (%s) &nbsp;&nbsp;second-worst <b>%s</b> (%s) &nbsp;&nbsp;gap %d tier(s) \x97 %s</font><br/>",
			names[minIdx], PlayerRatingToStringHTML(minTier),
			names[secondMinIdx], PlayerRatingToStringHTML(secondMinTier),
			secondMinTier - minTier,
			noobSituation ? "NOOB SITUATION" : "not a noob situation");
#endif

		// noob rule: try to get the noob dude on equal or higher % team
		if (noobSituation && iDiff) {

			int weakerTeam = (team1RelativeStrength < team2RelativeStrength) ? 0 : 1;
			int playersTeam = (minIdx < 4) ? 0 : 1;   /* idx<4 ? team 0 */

#if 0
			TeamGen_DebugPrintf("<font color=purple>Weaker team is <b>team %d</b>; worst player is on <b>team %d</b></font><br/>", weakerTeam + 1, playersTeam + 1);
#endif

			if (playersTeam == weakerTeam) {
#ifdef OLDNOOBRULE
				satisfiesNoobRule = qfalse;
#endif
#if 0
				TeamGen_DebugPrintf("<font color=red>Noob rule NOT satisfied (worst player on weaker team)</font><br/>");
#endif
			}
			else {
#if 0
				TeamGen_DebugPrintf("<font color=darkgreen>Noob rule satisfied (worst player on stronger/equal team)</font><br/>");
#endif
			}
		}

		// noob carry rule: try to get the noob dude with a strong offense player
		if (noobSituation) {
			// figure out the best offense players
			int offTier[8] = {
				PlayerTierFromRating(team1base->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team1chase->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]),

				PlayerTierFromRating(team2base->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team2chase->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE])
			};

			int ttlRank[8] = {
				team1base->ttlholdRank,      team1chase->ttlholdRank,
				team1offense1->ttlholdRank,  team1offense2->ttlholdRank,

				team2base->ttlholdRank,      team2chase->ttlholdRank,
				team2offense1->ttlholdRank,  team2offense2->ttlholdRank
			};

			int bestOffTier = PLAYERRATING_UNRATED;
			for (int i = 0; i < 8; ++i)
				if (offTier[i] > bestOffTier) bestOffTier = offTier[i];

			int candidateMask = 0;
			for (int i = 0; i < 8; ++i) {
				if (offTier[i] >= PLAYERRATING_HIGH_A ||
					(offTier[i] >= PLAYERRATING_LOW_A && offTier[i] >= bestOffTier - 1))
					candidateMask |= (1 << i);
			}

			int numCandidates = 0;
			for (int i = 0; i < 8; ++i)
				if (candidateMask & (1 << i)) ++numCandidates;

			// trim down the pool of carries
			if (numCandidates > /*2*/1) {
				int worstIdx = -1, worstRank = -1;
				for (int i = 0; i < 8; ++i)
					if (candidateMask & (1 << i))
						if (ttlRank[i] > worstRank) { worstRank = ttlRank[i]; worstIdx = i; }
				if (worstIdx != -1) {
					candidateMask &= ~(1 << worstIdx);
					--numCandidates;
				}
			}

			// on his team?
			int noobTeam = (minIdx < 4) ? 0 : 1;
			int offIdx1 = (noobTeam == 0) ? 2 : 6;
			int offIdx2 = offIdx1 + 1;

			qboolean carryPresent = (candidateMask & (1 << offIdx1)) || (candidateMask & (1 << offIdx2));

#if 0
			TeamGen_DebugPrintf(
				"<font color=purple>Noob-carry check: candidate carries = %d, %s on noob\x92s team</font><br/>",
				numCandidates,
				carryPresent ? "carry present" : "NO carry present"
			);
#endif

#ifdef OLDNOOBRULE
			if (!carryPresent) {
				satisfiesNoobCarryRule = qfalse;
			}
#else
			if (carryPresent) {
				satisfiesNoobRule = qtrue;
			}
			else {
				satisfiesNoobRule = qfalse;
			}
#endif
		}
	}

	TeamGen_DebugPrintf("SemiTryhard:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d noob rule, %d noob carry rule, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
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
		satisfiesNoobRule,
		satisfiesNoobCarryRule,
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
		if (!context->tunables.rerollNum && context->tunables.acdImbalanceCapWithoutReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
		else if (context->tunables.rerollNum > 0 && context->tunables.imbalanceCapRaisePerReroll > 0) {
			double cap = context->tunables.acdImbalanceCapWithoutReroll + (context->tunables.rerollNum * context->tunables.imbalanceCapRaisePerReroll);
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		if (context->debugLog) context->debugLog->rejectGarbageTier++;
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team1ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team1ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 3); // B/B+ chases can only chase 3 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team2ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team2ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 3); // B/B+ chases can only chase 4 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}
	}

	qboolean isBest = qfalse;
	// hoisted out so the report can record what this algo scored on; no comparator reads candidate->teams
	permutationOfTeams_t candidate = { 0 };
	candidate.teams[0].rawStrength = team1RawStrength;
	candidate.teams[1].rawStrength = team2RawStrength;
	candidate.teams[0].relativeStrength = team1RelativeStrength;
	candidate.teams[1].relativeStrength = team2RelativeStrength;
	{
		candidate.iDiff = iDiff;
		candidate.totalSkill = iTotal;
		candidate.numOnPreferredPos = numOnPreferredPos;
		candidate.numOnAvoidedPos = numOnAvoidedPos;
		candidate.bottomTierImbalance = bottomTierImbalance;
		candidate.topTierImbalance = topTierImbalance;
		candidate.offenseDefenseDiff = offenseDefenseDiff;
		candidate.satisfiesNoobRule = satisfiesNoobRule;
		candidate.satisfiesNoobCarryRule = satisfiesNoobCarryRule;
		isBest = IsBetter_SemiTryhard(&candidate, context->best,
			(context->numEligible == 8 && context->tunables.new8PlayerAlgo) ? qtrue : qfalse);
		if (isBest)
			TeamGen_DebugPrintf(" <font style=\"background-color: yellow;\">best so far</font><br/>");
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
				if (context->debugLog) context->debugLog->rejectAvoidedHash++;
				--context->numPermutations;
				return;
			}
		}

		TeamGenDebugLog_RecordMilestone(context->debugLog, &candidate,
			team1base, team1chase, team1offense1, team1offense2, team2base, team2chase, team2offense1, team2offense2);

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		context->best->totalSkill = iTotal;
		context->best->satisfiesNoobRule = satisfiesNoobRule;
		context->best->satisfiesNoobCarryRule = satisfiesNoobCarryRule;
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
		context->best->teams[0].baseRating = team1base->rating[CTFPOSITION_BASE];
		context->best->teams[0].chaseRating = team1chase->rating[CTFPOSITION_CHASE];
		context->best->teams[0].offense1Rating = team1offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[0].offense2Rating = team1offense2->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].baseRating = team2base->rating[CTFPOSITION_BASE];
		context->best->teams[1].chaseRating = team2chase->rating[CTFPOSITION_CHASE];
		context->best->teams[1].offense1Rating = team2offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].offense2Rating = team2offense2->rating[CTFPOSITION_OFFENSE];
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
	if (context->debugLog) context->debugLog->numCandidatesEvaluated++;

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

	// noob rule and noob carry rule
	qboolean satisfiesNoobRule = qtrue;
	qboolean satisfiesNoobCarryRule = qtrue;
	if (context->numEligible == 8 && context->tunables.noobCheck) {
		double ratings[8] = {
			team1base->rating[CTFPOSITION_BASE],
			team1chase->rating[CTFPOSITION_CHASE],
			team1offense1->rating[CTFPOSITION_OFFENSE],
			team1offense2->rating[CTFPOSITION_OFFENSE],

			team2base->rating[CTFPOSITION_BASE],
			team2chase->rating[CTFPOSITION_CHASE],
			team2offense1->rating[CTFPOSITION_OFFENSE],
			team2offense2->rating[CTFPOSITION_OFFENSE]
		};

#if 0
		const char *names[8] = {
			team1base->accountName,     team1chase->accountName,
			team1offense1->accountName, team1offense2->accountName,

			team2base->accountName,     team2chase->accountName,
			team2offense1->accountName, team2offense2->accountName
		};
#endif

		int tiers[8];
		for (int i = 0; i < 8; ++i)
			tiers[i] = PlayerTierFromRating(ratings[i]);

		int minTier = 99, secondMinTier = 99;
		int minIdx = -1, secondMinIdx = -1;

		for (int i = 0; i < 8; ++i) {
			if (tiers[i] < minTier) {
				secondMinTier = minTier;      secondMinIdx = minIdx;
				minTier = tiers[i];     minIdx = i;
			}
			else if (tiers[i] < secondMinTier) {
				secondMinTier = tiers[i];     secondMinIdx = i;
			}
		}

		// a noob situation is where there are 7 goodies and 1 baddy
		qboolean noobSituation = (minTier <= PLAYERRATING_HIGH_C) && (secondMinTier - minTier >= 3 || (secondMinTier - minTier >= 2 && secondMinTier >= PLAYERRATING_MID_B));

#if 0
		TeamGen_DebugPrintf(
			"<font color=purple>Noob-check: worst <b>%s</b> (%s) &nbsp;&nbsp;second-worst <b>%s</b> (%s) &nbsp;&nbsp;gap %d tier(s) \x97 %s</font><br/>",
			names[minIdx], PlayerRatingToStringHTML(minTier),
			names[secondMinIdx], PlayerRatingToStringHTML(secondMinTier),
			secondMinTier - minTier,
			noobSituation ? "NOOB SITUATION" : "not a noob situation");
#endif

		// noob rule: try to get the noob dude on equal or higher % team
		if (noobSituation && iDiff) {

			int weakerTeam = (team1RelativeStrength < team2RelativeStrength) ? 0 : 1;
			int playersTeam = (minIdx < 4) ? 0 : 1;   /* idx<4 ? team 0 */

#if 0
			TeamGen_DebugPrintf("<font color=purple>Weaker team is <b>team %d</b>; worst player is on <b>team %d</b></font><br/>", weakerTeam + 1, playersTeam + 1);
#endif

			if (playersTeam == weakerTeam) {
#ifdef OLDNOOBRULE
				satisfiesNoobRule = qfalse;
#endif
#if 0
				TeamGen_DebugPrintf("<font color=red>Noob rule NOT satisfied (worst player on weaker team)</font><br/>");
#endif
			}
			else {
#if 0
				TeamGen_DebugPrintf("<font color=darkgreen>Noob rule satisfied (worst player on stronger/equal team)</font><br/>");
#endif
			}
		}

		// noob carry rule: try to get the noob dude with a strong offense player
		if (noobSituation) {
			// figure out the best offense players
			int offTier[8] = {
				PlayerTierFromRating(team1base->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team1chase->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]),

				PlayerTierFromRating(team2base->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team2chase->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]),
				PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE])
			};

			int ttlRank[8] = {
				team1base->ttlholdRank,      team1chase->ttlholdRank,
				team1offense1->ttlholdRank,  team1offense2->ttlholdRank,

				team2base->ttlholdRank,      team2chase->ttlholdRank,
				team2offense1->ttlholdRank,  team2offense2->ttlholdRank
			};

			int bestOffTier = PLAYERRATING_UNRATED;
			for (int i = 0; i < 8; ++i)
				if (offTier[i] > bestOffTier) bestOffTier = offTier[i];

			int candidateMask = 0;
			for (int i = 0; i < 8; ++i) {
				if (offTier[i] >= PLAYERRATING_HIGH_A ||
					(offTier[i] >= PLAYERRATING_LOW_A && offTier[i] >= bestOffTier - 1))
					candidateMask |= (1 << i);
			}

			int numCandidates = 0;
			for (int i = 0; i < 8; ++i)
				if (candidateMask & (1 << i)) ++numCandidates;

			// trim down the pool of carries
			if (numCandidates > /*2*/1) {
				int worstIdx = -1, worstRank = -1;
				for (int i = 0; i < 8; ++i)
					if (candidateMask & (1 << i))
						if (ttlRank[i] > worstRank) { worstRank = ttlRank[i]; worstIdx = i; }
				if (worstIdx != -1) {
					candidateMask &= ~(1 << worstIdx);
					--numCandidates;
				}
			}

			// on his team?
			int noobTeam = (minIdx < 4) ? 0 : 1;
			int offIdx1 = (noobTeam == 0) ? 2 : 6;
			int offIdx2 = offIdx1 + 1;

			qboolean carryPresent = (candidateMask & (1 << offIdx1)) || (candidateMask & (1 << offIdx2));

#if 0
			TeamGen_DebugPrintf(
				"<font color=purple>Noob-carry check: candidate carries = %d, %s on noob\x92s team</font><br/>",
				numCandidates,
				carryPresent ? "carry present" : "NO carry present"
			);
#endif

#ifdef OLDNOOBRULE
			if (!carryPresent) {
				satisfiesNoobCarryRule = qfalse;
			}
#else
			if (carryPresent) {
				satisfiesNoobRule = qtrue;
			}
			else {
				satisfiesNoobRule = qfalse;
			}
#endif
		}
	}

	TeamGen_DebugPrintf("Tryhard:%s%s</font>/%s%s</font>/%s%s</font>/%s%s</font> vs. %s%s</font>/%s%s</font>/%s%s</font>/%s%s</font><font color=black> : %0.3f vs. %0.3f raw, %0.2f vs. %0.2f relative, %d numOnPreferredPos, %d numAvoid, %d noob rule, %d noob carry rule, %d (%d/%d) garbage imbalance, %d (%d/%d) bottom imbalance, %d (%d/%d) top imbalance, %0.3f total, %0.3f diff, offense/defense diff %d (%d/%d)</font>",
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
		satisfiesNoobRule,
		satisfiesNoobCarryRule,
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
		if (!context->tunables.rerollNum && context->tunables.bImbalanceCapWithoutReroll > 0) {
			double cap = context->tunables.bImbalanceCapWithoutReroll;
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
		else if (context->tunables.rerollNum > 0 && context->tunables.imbalanceCapRaisePerReroll > 0) {
			double cap = context->tunables.bImbalanceCapWithoutReroll + (context->tunables.rerollNum * context->tunables.imbalanceCapRaisePerReroll);
			cap = (round(cap * 10000) / 10000.0) + 0.00001;
			if (diffPercentage >= cap) {
				if (context->debugLog) context->debugLog->rejectImbalanceCap++;
				return;
			}
		}
	}

	if (garbageTierImbalance >= 2) {
		if (context->debugLog) context->debugLog->rejectGarbageTier++;
		return;
	}

	if (context->enforceChaseRule) {
		// make sure each chase isn't too much worse than the best opposing offense
		{
			ctfPlayerTier_t team1ChaseTier = PlayerTierFromRating(team1chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam2OffenseTier = PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team2offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team2offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team1ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team1ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam2OffenseTier > team1ChaseTier + 3); // B/B+ chases can only chase 3 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}

		{
			ctfPlayerTier_t team2ChaseTier = PlayerTierFromRating(team2chase->rating[CTFPOSITION_CHASE]);
			ctfPlayerTier_t highestTeam1OffenseTier = PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) > PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]) ? PlayerTierFromRating(team1offense1->rating[CTFPOSITION_OFFENSE]) : PlayerTierFromRating(team1offense2->rating[CTFPOSITION_OFFENSE]);

			qboolean invalid;
			if (team2ChaseTier >= PLAYERRATING_LOW_A)
				invalid = qfalse; // A- or higher chases can chase anyone
			else if (team2ChaseTier <= PLAYERRATING_LOW_B)
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 2); // B- or lower chases can only chase 2 above them
			else
				invalid = (highestTeam1OffenseTier > team2ChaseTier + 3); // B/B+ chases can only chase 4 above them

			if (invalid) {
				if (context->debugLog) context->debugLog->rejectChaseRule++;
				return;
			}
		}
	}

	qboolean isBest = qfalse;
	// hoisted out so the report can record what this algo scored on; no comparator reads candidate->teams
	permutationOfTeams_t candidate = { 0 };
	candidate.teams[0].rawStrength = team1RawStrength;
	candidate.teams[1].rawStrength = team2RawStrength;
	candidate.teams[0].relativeStrength = team1RelativeStrength;
	candidate.teams[1].relativeStrength = team2RelativeStrength;
	{
		candidate.iDiff = iDiff;
		candidate.totalSkill = iTotal;
		candidate.numOnPreferredPos = numOnPreferredPos;
		candidate.numOnAvoidedPos = numOnAvoidedPos;
		candidate.bottomTierImbalance = bottomTierImbalance;
		candidate.topTierImbalance = topTierImbalance;
		candidate.offenseDefenseDiff = offenseDefenseDiff;
		candidate.satisfiesNoobRule = satisfiesNoobRule;
		candidate.satisfiesNoobCarryRule = satisfiesNoobCarryRule;
		isBest = IsBetter_Tryhard(&candidate, context->best,
			(context->numEligible == 8 && context->tunables.new8PlayerAlgo) ? qtrue : qfalse,
			context->tunables.noobCheck ? qtrue : qfalse);
		if (isBest)
			TeamGen_DebugPrintf(" <font style=\"background-color: yellow;\">best so far</font><br/>");
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
				if (context->debugLog) context->debugLog->rejectAvoidedHash++;
				--context->numPermutations;
				return;
			}
		}

		TeamGenDebugLog_RecordMilestone(context->debugLog, &candidate,
			team1base, team1chase, team1offense1, team1offense2, team2base, team2chase, team2offense1, team2offense2);

		context->best->valid = qtrue;
		context->best->iDiff = iDiff;
		context->best->offenseDefenseDiff = offenseDefenseDiff;
		context->best->numOnPreferredPos = numOnPreferredPos;
		context->best->numOnAvoidedPos = numOnAvoidedPos;
		context->best->topTierImbalance = topTierImbalance;
		context->best->bottomTierImbalance = bottomTierImbalance;
		context->best->totalSkill = iTotal;
		context->best->satisfiesNoobRule = satisfiesNoobRule;
		context->best->satisfiesNoobCarryRule = satisfiesNoobCarryRule;
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
		context->best->teams[0].baseRating = team1base->rating[CTFPOSITION_BASE];
		context->best->teams[0].chaseRating = team1chase->rating[CTFPOSITION_CHASE];
		context->best->teams[0].offense1Rating = team1offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[0].offense2Rating = team1offense2->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].baseRating = team2base->rating[CTFPOSITION_BASE];
		context->best->teams[1].chaseRating = team2chase->rating[CTFPOSITION_CHASE];
		context->best->teams[1].offense1Rating = team2offense1->rating[CTFPOSITION_OFFENSE];
		context->best->teams[1].offense2Rating = team2offense2->rating[CTFPOSITION_OFFENSE];
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

	FisherYatesShuffle_R(&orders[0][0], 6, sizeof(orders[0]), &context->rng);

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

	FisherYatesShuffle_R(&orders[0][0], 12, sizeof(orders[0]), &context->rng);

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

// ============================================================================
// parallel PermuteTeams -- splits GetDefenseCombinations' C(numEligible,4) search across workers.
// each runs the same unmodified chain against its own local best, counter and PRNG state; results
// reduce with the same IsBetter_* predicate.
//
// identical rules, NOT identical results: each worker's own PRNG stream shuffles visit order and
// therefore decides ties. parallel matches serial only at 1 worker. accepted by design -- same
// machine and worker count still reproduces run to run.
// ============================================================================

static uint64_t BinomialCoeff(uint64_t n, int k) {
	if (k < 0 || (uint64_t)k > n)
		return 0;
	if (k == 0 || (uint64_t)k == n)
		return 1;
	if ((uint64_t)k > n - (uint64_t)k)
		k = (int)(n - (uint64_t)k);
	uint64_t result = 1;
	for (int i = 0; i < k; i++) {
		result *= (n - (uint64_t)i);
		result /= (uint64_t)(i + 1);
	}
	return result;
}

// computes the index-th t-combination of {0..n-1} into c[1..t], in the same order
// GetDefenseCombinations' increment loop produces (combinatorial number system: rank =
// C(c[1],1)+...+C(c[t],t)). lets a worker jump straight to its starting point.
static void CombinationFromIndex(int n, int t, uint64_t index, int *c) {
	uint64_t remaining = index;
	for (int i = t; i >= 1; i--) {
		int v = i - 1;
		while (BinomialCoeff((uint64_t)(v + 1), i) <= remaining)
			v++;
		c[i] = v;
		remaining -= BinomialCoeff((uint64_t)v, i);
	}
}

// which comparator a callback scores with, for the report. kept next to TeamGen_IsBetterForCallback
// -- a new TryTeamPermutation* needs adding to both.
static int TeamGen_DebugAlgoForCallback(PermutationCallback callback) {
	if (callback == TryTeamPermutation)					return TEAMGENDEBUGALGO_REGULAR;
	if (callback == TryTeamPermutation_Appeasing)		return TEAMGENDEBUGALGO_APPEASING;
	if (callback == TryTeamPermutation_Fairest)			return TEAMGENDEBUGALGO_FAIREST;
	if (callback == TryTeamPermutation_SemiAppeasing)	return TEAMGENDEBUGALGO_SEMIAPPEASING;
	if (callback == TryTeamPermutation_FirstChoice)		return TEAMGENDEBUGALGO_FIRSTCHOICE;
	if (callback == TryTeamPermutation_Inclusive)		return TEAMGENDEBUGALGO_INCLUSIVE;
	if (callback == TryTeamPermutation_SemiTryhard)		return TEAMGENDEBUGALGO_SEMITRYHARD;
	if (callback == TryTeamPermutation_Tryhard)			return TEAMGENDEBUGALGO_TRYHARD;
	return TEAMGENDEBUGALGO_NONE;
}

static qboolean TeamGen_IsBetterForCallback(PermutationCallback callback, const permutationOfTeams_t *candidate, const permutationOfTeams_t *best, int numEligible, const teamGenTunables_t *tunables) {
	// tunables, not live cvars -- the reduction must use the same values the workers scored with
	qboolean is8Player = (numEligible == 8 && tunables->new8PlayerAlgo) ? qtrue : qfalse;
	qboolean noobCheck = tunables->noobCheck;

	// NOTE: SemiAppeasing and FirstChoice pass plain (numEligible == 8), not is8Player, matching their
	// in-search call sites. reducing with a different predicate than the search used diverges.
	qboolean is8PlayerRaw = (numEligible == 8) ? qtrue : qfalse;

	if (callback == TryTeamPermutation)
		return IsBetter_Regular(candidate, best);
	if (callback == TryTeamPermutation_Appeasing)
		return IsBetter_Appeasing(candidate, best);
	if (callback == TryTeamPermutation_Fairest)
		return IsBetter_Fairest(candidate, best, is8Player, noobCheck);
	if (callback == TryTeamPermutation_SemiAppeasing)
		return IsBetter_SemiAppeasing(candidate, best, is8PlayerRaw);
	if (callback == TryTeamPermutation_FirstChoice)
		return IsBetter_FirstChoice(candidate, best, is8PlayerRaw);
	if (callback == TryTeamPermutation_Inclusive)
		return IsBetter_Inclusive(candidate, best);
	if (callback == TryTeamPermutation_SemiTryhard)
		return IsBetter_SemiTryhard(candidate, best, is8Player);
	if (callback == TryTeamPermutation_Tryhard)
		return IsBetter_Tryhard(candidate, best, is8Player, noobCheck);

	assert(qfalse); // unknown callback -- every TryTeamPermutation* variant must be listed above
	return qfalse;
}

typedef struct {
	permutationPlayer_t *playerArray;
	int numEligible;
	int *accountNums; // shared, read-only input (each HandleDefenseCombination call makes its own working copy)
	PermutationCallback callback;
	qboolean enforceChaseRule;
	list_t *avoidedHashesList; // shared, read-only for the duration of this call (see ListFind's stack-local iterator -- safe for concurrent reads)
	qboolean banAvoidedPositions;
	int type;
	qboolean enforceImbalanceCaps;
	unsigned int seedBase;
	int workerIndex;
	uint64_t startIndex;
	uint64_t count;
	teamGenTunables_t tunables;
	volatile int *abortFlag; // may be NULL (synchronous path)

	// outputs
	permutationOfTeams_t localBest;
	uint64_t localNumPermutations;
	// NULL unless the caller wants logs. a pointer, not embedded: the log is big and this runs once
	// per type per iteration.
	teamGenDebugLog_t *localDebugLog;
} teamGenWorkerTask_t;

static void TeamGenWorker_Run(void *arg) {
	teamGenWorkerTask_t *task = (teamGenWorkerTask_t *)arg;

	teamGeneratorContext_t context;
	context.best = &task->localBest;
	context.callback = task->callback;
	context.enforceChaseRule = task->enforceChaseRule;
	context.numEligible = task->numEligible;
	context.numPermutations = 0;
	context.banAvoidedPositions = task->banAvoidedPositions;
	context.best->offenseDefenseDiff = 0;
	context.best->totalSkill = 0;
	context.best->numSatisfiedCyds = -1;
	context.best->numSatisfiedLgs = -1;
	context.best->satisfiesNoobRule = qfalse;
	context.best->satisfiesNoobCarryRule = qfalse;
	context.type = task->type;
	context.enforceImbalanceCaps = task->enforceImbalanceCaps;
	context.tunables = task->tunables;
	// mix in the worker index so different workers don't shuffle identically
	GPRNG_Seed(&context.rng, task->seedBase + (unsigned int)task->type + ((unsigned int)task->workerIndex * 0x9E3779B1u));
	context.avoidedHashesList = (task->avoidedHashesList && task->avoidedHashesList->size > 0) ? task->avoidedHashesList : NULL;
	context.debugLog = task->localDebugLog;

	if (task->count > 0) {
		int t = 4;
		int c[8] = { 0 };
		CombinationFromIndex(task->numEligible, t, task->startIndex, c);
		c[t + 1] = task->numEligible;
		c[t + 2] = 0;

		for (uint64_t done = 0; done < task->count; done++) {
			// bail out promptly on abort -- one batch is a whole C(numEligible,4) sweep, seconds of work at a
			// full roster. plain volatile read, NOT GAtomic_Get: that's a locked RMW that takes the cache line
			// exclusively every check (~7% slower at 16 players). write-once monotonic flag, so no acquire needed.
			if (task->abortFlag && !(done & 15) && *task->abortFlag)
				break;

			HandleDefenseCombination(task->playerArray, c, t, task->accountNums, &context);

			int j = 1;
			while (c[j] + 1 == c[j + 1]) {
				c[j] = j - 1;
				++j;
			}
			if (j > t)
				break; // reached the natural end of the sequence
			++c[j];
		}
	}

	task->localNumPermutations = context.numPermutations;
}

static uint64_t PermuteTeams_Threaded(permutationPlayer_t *playerArray, int numEligible, permutationOfTeams_t *bestOut, PermutationCallback callback, qboolean enforceChaseRule, list_t *avoidedHashesList, qboolean banAvoidedPositions, int type, qboolean enforceImbalanceCaps, unsigned int seedBase, const teamGenTunables_t *tunables, teamGenDebugLog_t *debugLog, volatile int *abortFlag) {
	assert(playerArray && bestOut);

	int *intArr = malloc(sizeof(int) * numEligible);
	if (!intArr)
		return 0;
	for (int i = 0; i < numEligible; i++)
		intArr[i] = i;

	uint64_t totalCombos = BinomialCoeff((uint64_t)numEligible, 4);

	// tunables->workerThreads, not the live cvar: it feeds both the chunk split and each worker's PRNG
	// seed, so changing it mid-job would break reproducibility. already resolved from auto.
	int numWorkers = tunables->workerThreads;
	if (numWorkers < 1)
		numWorkers = 1;
	// keep each worker's slice worth the spawn cost; also subsumes the old "never more workers than
	// combinations" clamp
	{
		uint64_t maxUsefulWorkers = totalCombos / TEAMGEN_MIN_COMBOS_PER_WORKER;
		if (maxUsefulWorkers < 1)
			maxUsefulWorkers = 1;
		if ((uint64_t)numWorkers > maxUsefulWorkers)
			numWorkers = (int)maxUsefulWorkers;
	}
	if (numWorkers > 64)
		numWorkers = 64; // sanity cap against a misconfigured cvar
	if (numWorkers < 1)
		numWorkers = 1;

	teamGenWorkerTask_t *tasks = (teamGenWorkerTask_t *)calloc(numWorkers, sizeof(teamGenWorkerTask_t));
	gthread_t **threads = (gthread_t **)calloc(numWorkers, sizeof(gthread_t *));
	// one contiguous block for every worker's log; NULL (no logging) if it can't be had
	teamGenDebugLog_t *workerLogs = debugLog ? (teamGenDebugLog_t *)calloc(numWorkers, sizeof(teamGenDebugLog_t)) : NULL;
	if (!tasks || !threads) {
		free(tasks);
		free(threads);
		free(workerLogs);
		free(intArr);
		return 0;
	}

	uint64_t chunkSize = (totalCombos + (uint64_t)numWorkers - 1) / (uint64_t)numWorkers;
	uint64_t nextStart = 0;

	for (int w = 0; w < numWorkers; w++) {
		uint64_t thisCount = 0;
		if (nextStart < totalCombos) {
			thisCount = chunkSize;
			if (nextStart + thisCount > totalCombos)
				thisCount = totalCombos - nextStart;
		}

		// start each worker from the caller's running best, as serial PermuteTeams does. bestOut holds
		// sentinels (iDiff 999999999 etc); a calloc'd localBest would zero them and leave any minimizing
		// IsBetter_* chain with nothing satisfiable.
		tasks[w].localBest = *bestOut;
		tasks[w].playerArray = playerArray;
		tasks[w].numEligible = numEligible;
		tasks[w].accountNums = intArr;
		tasks[w].callback = callback;
		tasks[w].enforceChaseRule = enforceChaseRule;
		tasks[w].avoidedHashesList = avoidedHashesList;
		tasks[w].banAvoidedPositions = banAvoidedPositions;
		tasks[w].type = type;
		tasks[w].enforceImbalanceCaps = enforceImbalanceCaps;
		tasks[w].seedBase = seedBase;
		tasks[w].workerIndex = w;
		tasks[w].startIndex = nextStart;
		tasks[w].count = thisCount;
		tasks[w].localDebugLog = workerLogs ? &workerLogs[w] : NULL;
		tasks[w].tunables = *tunables;
		tasks[w].abortFlag = abortFlag;

		nextStart += thisCount;

		if (thisCount == 0) {
			// nothing for this (or any later) worker to do
			continue;
		}

		if (GThread_Create(&threads[w], TeamGenWorker_Run, &tasks[w], qtrue, tunables->workerNice)) {
			// spawned successfully; joined below
		}
		else {
			// thread creation failed (extremely unlikely) -- run it inline rather than lose the work
			threads[w] = NULL;
			TeamGenWorker_Run(&tasks[w]);
		}
	}

	for (int w = 0; w < numWorkers; w++) {
		if (threads[w])
			GThread_Join(threads[w]);
	}

	// reduce with the same predicate the serial scan uses, in worker-index order so the reduction is
	// deterministic. doesn't reproduce serial's winner above 1 worker -- see the header comment.
	uint64_t totalPermutations = 0;
	for (int w = 0; w < numWorkers; w++) {
		totalPermutations += tasks[w].localNumPermutations;
		if (tasks[w].localBest.valid) {
			if (!bestOut->valid || TeamGen_IsBetterForCallback(callback, &tasks[w].localBest, bestOut, numEligible, tunables)) {
				memcpy(bestOut, &tasks[w].localBest, sizeof(permutationOfTeams_t));
			}
		}
		if (debugLog && tasks[w].localDebugLog)
			TeamGenDebugLog_Merge(debugLog, tasks[w].localDebugLog);
	}

	free(tasks);
	free(threads);
	free(workerLogs);
	free(intArr);

	return totalPermutations;
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
// no abortFlag, unlike PermuteTeams_Threaded: only reached on the synchronous main-thread path
static uint64_t PermuteTeams(permutationPlayer_t *playerArray, int numEligible, permutationOfTeams_t *bestOut, PermutationCallback callback, qboolean enforceChaseRule, list_t *avoidedHashesList, qboolean banAvoidedPositions, int type, qboolean enforceImbalanceCaps, unsigned int seedBase, const teamGenTunables_t *tunables, teamGenDebugLog_t *debugLog) {
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
	context.best->numSatisfiedCyds = -1;
	context.best->numSatisfiedLgs = -1;
	context.best->satisfiesNoobRule = qfalse;
	context.best->satisfiesNoobCarryRule = qfalse;
	context.type = type;
	context.enforceImbalanceCaps = enforceImbalanceCaps;
	context.tunables = *tunables;
	// seedBase is caller-supplied so this never touches the global teamGenSeed
	GPRNG_Seed(&context.rng, seedBase + type);
	context.debugLog = debugLog;
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

// forward decl -- defined further down with the rest of the async machinery
static uint64_t PermuteTeams_Threaded(permutationPlayer_t *playerArray, int numEligible, permutationOfTeams_t *bestOut, PermutationCallback callback, qboolean enforceChaseRule, list_t *avoidedHashesList, qboolean banAvoidedPositions, int type, qboolean enforceImbalanceCaps, unsigned int seedBase, const teamGenTunables_t *tunables, teamGenDebugLog_t *debugLog, volatile int *abortFlag);

// every PermuteTeams call site in GenerateTeams goes through here: parallel search on the
// async/coordinator path, plain single-threaded search on the synchronous fallback
static uint64_t PermuteTeams_Dispatch(permutationPlayer_t *playerArray, int numEligible, permutationOfTeams_t *bestOut, PermutationCallback callback, qboolean enforceChaseRule, list_t *avoidedHashesList, qboolean banAvoidedPositions, int type, qboolean enforceImbalanceCaps, unsigned int seedBase, const teamGenTunables_t *tunables, qboolean useThreaded, teamGenDebugLog_t *debugLog, volatile int *abortFlag) {
	// checked here, not just between outer iterations -- one iteration calls this anywhere from a
	// handful to 30+ times, so this is what actually bounds abort latency
	if (abortFlag && GAtomic_Get(abortFlag))
		return 0; // leave bestOut untouched
	// set here rather than once per type: GenerateTeams picks the callback dynamically and can change
	// it between relaxation retries. last one wins, which is the one the type keeps.
	if (debugLog)
		debugLog->algo = TeamGen_DebugAlgoForCallback(callback);
	if (useThreaded)
		return PermuteTeams_Threaded(playerArray, numEligible, bestOut, callback, enforceChaseRule, avoidedHashesList, banAvoidedPositions, type, enforceImbalanceCaps, seedBase, tunables, debugLog, abortFlag);
	return PermuteTeams(playerArray, numEligible, bestOut, callback, enforceChaseRule, avoidedHashesList, banAvoidedPositions, type, enforceImbalanceCaps, seedBase, tunables, debugLog);
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

// ============================================================================
// snapshot support for running GenerateTeams off the main thread -- a plain-array copy of the
// per-account data it looks up from level.ratingList/level.mostPlayedPositionsList, resolved on
// the main thread before dispatch so the coordinator never touches those lists.
// ============================================================================

#define MAX_TEAMGEN_SNAPSHOT_ENTRIES MAX_CLIENTS

typedef struct {
	int				accountId;
	qboolean		hasRating;
	playerRating_t	rating;
	qboolean		hasMostPlayed;
	mostPlayedPos_t	mostPlayed;
} teamGenSnapshotEntry_t;

typedef struct {
	int numEntries;
	teamGenSnapshotEntry_t entries[MAX_TEAMGEN_SNAPSHOT_ENTRIES];
} teamGenDataSnapshot_t;

static playerRating_t *TeamGen_SnapshotFindRating(teamGenDataSnapshot_t *snapshot, int accountId) {
	for (int i = 0; i < snapshot->numEntries; i++) {
		if (snapshot->entries[i].accountId == accountId)
			return snapshot->entries[i].hasRating ? &snapshot->entries[i].rating : NULL;
	}
	return NULL;
}

static mostPlayedPos_t *TeamGen_SnapshotFindMostPlayed(teamGenDataSnapshot_t *snapshot, int accountId) {
	for (int i = 0; i < snapshot->numEntries; i++) {
		if (snapshot->entries[i].accountId == accountId)
			return snapshot->entries[i].hasMostPlayed ? &snapshot->entries[i].mostPlayed : NULL;
	}
	return NULL;
}

// builds a snapshot for every account in sortedClients. main thread only (reads level.*).
//
// walks all MAX_CLIENTS slots, not just numEligible: this array is indexed by client number and
// is therefore sparse, so an eligible player in slot 9 of an 8-player pug sits past numEligible.
// missing one is silent and nasty -- they'd get scored as unrated.
static void TeamGen_BuildDataSnapshot(const sortedClient_t *sortedClients, teamGenDataSnapshot_t *out) {
	out->numEntries = 0;
	for (int i = 0; i < MAX_CLIENTS && out->numEntries < MAX_TEAMGEN_SNAPSHOT_ENTRIES; i++) {
		if (!sortedClients[i].accountName[0])
			continue;

		teamGenSnapshotEntry_t *entry = &out->entries[out->numEntries++];
		entry->accountId = sortedClients[i].accountId;

		playerRating_t findMe = { 0 };
		findMe.accountId = entry->accountId;
		playerRating_t *foundRating = ListFind(&level.ratingList, PlayerRatingAccountIdMatches, &findMe, NULL);
		if (foundRating) {
			entry->hasRating = qtrue;
			entry->rating = *foundRating;
		}
		else {
			entry->hasRating = qfalse;
		}

		mostPlayedPos_t findMe2 = { 0 };
		findMe2.accountId = entry->accountId;
		mostPlayedPos_t *foundMostPlayed = ListFind(&level.mostPlayedPositionsList, MostPlayedPosMatches, &findMe2, NULL);
		if (foundMostPlayed) {
			entry->hasMostPlayed = qtrue;
			entry->mostPlayed = *foundMostPlayed;
		}
		else {
			entry->hasMostPlayed = qfalse;
		}
	}
}

// who from setClients[] is eligible right now. main thread only (reads g_entities/level).
static void TeamGen_ComputeCurrentEligibleRoster(const sortedClient_t *setClients, sortedClient_t sortedClientsOut[MAX_CLIENTS], int *numEligibleOut, int *numIngameOut) {
	qboolean eligible[MAX_CLIENTS] = { qfalse };
	memset(sortedClientsOut, 0, sizeof(sortedClient_t) * MAX_CLIENTS);

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
			const sortedClient_t *cl = setClients + j;
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
				!(other->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(other->client->pers.netname, "elo BOT")) &&
				other->client->sess.clientType == CLIENT_TYPE_NORMAL) {
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

	int numEligible = 0, numIngame = 0;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (!eligible[i])
			continue;

		gentity_t *ent = &g_entities[i];

		sortedClientsOut[i].clientNum = i;
#ifdef DEBUG_GENERATETEAMS
		Q_strncpyz(sortedClientsOut[i].accountName, Cvar_VariableString(va("z_debug%d", i + 1)), sizeof(sortedClientsOut[i].accountName));
		account_t acc;
		G_DBGetAccountByName(Cvar_VariableString(va("z_debug%d", i + 1)), &acc);
		sortedClientsOut[i].accountId = acc.id;
		sortedClientsOut[i].team = TEAM_SPECTATOR;
		memcpy(&sortedClientsOut[i].posPrefs, &acc.validPref, sizeof(positionPreferences_t));
#else
		Q_strncpyz(sortedClientsOut[i].accountName, ent->client->account->name, sizeof(sortedClientsOut[i].accountName));
		memcpy(&sortedClientsOut[i].posPrefs, &ent->client->account->validPref, sizeof(positionPreferences_t));
		sortedClientsOut[i].team = ent->client->sess.sessionTeam;
		sortedClientsOut[i].accountId = ent->client->account->id;
		sortedClientsOut[i].bannedPos = ent->client->account->bannedPos;
#endif

		sortedClientsOut[i].ttlholdRank = DB_LookupTtlHoldRank(sortedClientsOut[i].accountId);

		for (int j = CTFPOSITION_BASE; j <= CTFPOSITION_OFFENSE; j++) {
			if (sortedClientsOut[i].posPrefs.first & (1 << j))
				TeamGen_DebugPrintf("%s has first choice pos %s<br/>", sortedClientsOut[i].accountName, NameForPos(j));
			if (sortedClientsOut[i].posPrefs.second & (1 << j))
				TeamGen_DebugPrintf("%s has second choice pos %s<br/>", sortedClientsOut[i].accountName, NameForPos(j));
			if (sortedClientsOut[i].posPrefs.third & (1 << j))
				TeamGen_DebugPrintf("%s has third choice pos %s<br/>", sortedClientsOut[i].accountName, NameForPos(j));
			if (sortedClientsOut[i].posPrefs.avoid & (1 << j))
				TeamGen_DebugPrintf("%s has avoided pos %s<br/>", sortedClientsOut[i].accountName, NameForPos(j));
			if (sortedClientsOut[i].bannedPos & (1 << j))
				TeamGen_DebugPrintf("%s is banned on pos %s<br/>", sortedClientsOut[i].accountName, NameForPos(j));
		}

		++numEligible;
		if (sortedClientsOut[i].team == TEAM_RED || sortedClientsOut[i].team == TEAM_BLUE)
			++numIngame;
	}

	*numEligibleOut = numEligible;
	*numIngameOut = numIngame;
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

ctfPosition_t PermutationHasPlayer(int accountId, permutationOfTeams_t *p) {
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

static int NumPermutationsOfPlayer(int accountId, permutationOfTeams_t *p1, permutationOfTeams_t *p2, permutationOfTeams_t *p3, permutationOfTeams_t *p4, permutationOfTeams_t *p5, permutationOfTeams_t *p6, permutationOfTeams_t *p7) {
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
	if (PermutationHasPlayer(accountId, p7))
		++num;
	return num;
}

static double WeightOfPlayer(int accountId, permutationOfTeams_t *p1, permutationOfTeams_t *p2, permutationOfTeams_t *p3, permutationOfTeams_t *p4, permutationOfTeams_t *p5, permutationOfTeams_t *p6, teamGenDataSnapshot_t *snapshot) {
	// on the async path this runs on the coordinator thread, where walking level.ratingList is a
	// use-after-free waiting to happen (G_DBGetPlayerRatings free()s every node). use the snapshot.
	playerRating_t *ratings;
	if (snapshot) {
		ratings = TeamGen_SnapshotFindRating(snapshot, accountId);
	}
	else {
		playerRating_t findMe = { 0 };
		findMe.accountId = accountId;
		ratings = ListFind(&level.ratingList, PlayerRatingAccountIdMatches, &findMe, NULL);
	}
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

// maps a teamGeneratorType_t to whichever of the 7 caller-supplied pointers matches it -- several
// places need this exact lookup, so it lives here once
static permutationOfTeams_t *TeamGen_PermutationPtrForType(int type,
	permutationOfTeams_t *mostPlayed, permutationOfTeams_t *highestCaliber, permutationOfTeams_t *fairest,
	permutationOfTeams_t *inclusive, permutationOfTeams_t *desired, permutationOfTeams_t *semiDesired, permutationOfTeams_t *firstChoice) {
	switch (type) {
	case TEAMGENERATORTYPE_MOSTPLAYED:		return mostPlayed;
	case TEAMGENERATORTYPE_HIGHESTRATING:	return highestCaliber;
	case TEAMGENERATORTYPE_FAIREST:			return fairest;
	case TEAMGENERATORTYPE_INCLUSIVE:		return inclusive;
	case TEAMGENERATORTYPE_DESIREDPOS:		return desired;
	case TEAMGENERATORTYPE_SEMIDESIREDPOS:	return semiDesired;
	case TEAMGENERATORTYPE_FIRSTCHOICE:		return firstChoice;
	default: assert(qfalse); return NULL;
	}
}

// precomputedClients/snapshot are NULL on the synchronous path (asyncEnabled 0), which scans
// g_entities/level live as before. non-NULL means we're on a coordinator thread and touch none of
// that. debugLogsOut is NULL unless debug logging is on; otherwise NUM_TEAMGENERATORTYPES
// caller-owned logs, zeroed before this call.
//
// ?d and semiDesired score on numPermutationsIn, which counts how many of ?a/?b/?c each player is
// already in -- so those three have to run first. the enum order is what guarantees it; reorder it
// and every numPermutationsIn is zero, silently turning ?d into a second regular pass.
typedef char teamgen_inclusive_runs_after_abc_check[
	(TEAMGENERATORTYPE_INCLUSIVE > TEAMGENERATORTYPE_MOSTPLAYED &&
	 TEAMGENERATORTYPE_INCLUSIVE > TEAMGENERATORTYPE_HIGHESTRATING &&
	 TEAMGENERATORTYPE_INCLUSIVE > TEAMGENERATORTYPE_FAIREST &&
	 TEAMGENERATORTYPE_SEMIDESIREDPOS > TEAMGENERATORTYPE_MOSTPLAYED &&
	 TEAMGENERATORTYPE_SEMIDESIREDPOS > TEAMGENERATORTYPE_HIGHESTRATING &&
	 TEAMGENERATORTYPE_SEMIDESIREDPOS > TEAMGENERATORTYPE_FAIREST) ? 1 : -1];

static qboolean GenerateTeams(pugProposal_t *set, permutationOfTeams_t *mostPlayed, permutationOfTeams_t *highestCaliber, permutationOfTeams_t *fairest, permutationOfTeams_t *desired, permutationOfTeams_t *inclusive, permutationOfTeams_t *semiDesired, permutationOfTeams_t *firstChoice, uint64_t *numPermutations, qboolean enforceImbalanceCaps,
	unsigned int seedBase, const teamGenTunables_t *tunables, const sortedClient_t *precomputedClients, int precomputedNumEligible, int precomputedNumIngame, teamGenDataSnapshot_t *snapshot, teamGenDebugLog_t *debugLogsOut, volatile int *abortFlag) {
	assert(set);
#ifdef DEBUG_GENERATETEAMS
	clock_t start = clock();
#endif

	if (tunables->gametype != GT_CTF)
		return qfalse;

	TeamGen_Initialize();

	// snapshot != NULL means we're on a coordinator thread and may fan out across workers;
	// asyncEnabled 0 forces the original single-threaded scan, so it kills all threading
	qboolean useThreaded = (snapshot != NULL) && tunables->asyncEnabled;

	//G_DBGetPlayerRatings(); // the caller should refresh ratings

	int numIngame = 0;
	int numEligible = 0;
	sortedClient_t sortedClients[MAX_CLIENTS];
	memset(sortedClients, 0, sizeof(sortedClients));

	if (precomputedClients) {
		// async path: already computed on the main thread before dispatch -- never touch g_entities here
		memcpy(sortedClients, precomputedClients, sizeof(sortedClients));
		numEligible = precomputedNumEligible;
		numIngame = precomputedNumIngame;
	}
	else {
		// synchronous path: scan live state fresh, exactly as this always did
		TeamGen_ComputeCurrentEligibleRoster(set->clients, sortedClients, &numEligible, &numIngame);
	}

	if (numEligible < 8) {
		return qfalse;
	}

	// try to get the best possible teams using a few different approaches
#define MAX_TEAMGENERATORTYPES_PER_SET	(4)
	int numValid = 0;
	uint64_t gotten = 0llu;
	list_t listOfAvoidedHashesPlusHashesGottenOnThisGeneration = { 0 };
	ListCopy(&set->avoidedHashesList, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, sizeof(avoidedHash_t));
	qboolean usedSemiTryhardAlgoForB = qfalse;
	for (int typeIter = TEAMGENERATORTYPE_FIRST; typeIter < NUM_TEAMGENERATORTYPES; typeIter++) {

		// hack to swap computation of ?b to be before ?a so that if there's a permutation that's the most HC *and* is 50-50, ?b grabs it rather than ?a
		teamGeneratorType_t type;
		switch (typeIter) {
		case TEAMGENERATORTYPE_MOSTPLAYED: type = TEAMGENERATORTYPE_HIGHESTRATING; break;
		case TEAMGENERATORTYPE_HIGHESTRATING: type = TEAMGENERATORTYPE_MOSTPLAYED; break;
		default: type = typeIter; break;
		}

		teamGenDebugLog_t *typeDebugLog = debugLogsOut ? &debugLogsOut[type] : NULL;

		if (numValid >= MAX_TEAMGENERATORTYPES_PER_SET) {
			TeamGen_DebugPrintf("<font color=red>==========Breaking rather than trying type %d because we've already reached %d valid types</font><br/>", type, MAX_TEAMGENERATORTYPES_PER_SET);
			break;
		}

		// we only do the desired pass if and only if it's enabled and there isn't an a/b/c option with nobody on avoided pos
		if (type == TEAMGENERATORTYPE_DESIREDPOS) {
			if (!tunables->enableAppeasing)
				continue;

			if ((inclusive && inclusive->valid) || (semiDesired && semiDesired->valid) || (firstChoice && firstChoice->valid))
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
			if (!tunables->enableAppeasing)
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
			if (!tunables->enableInclusive)
				continue;

			if ((desired && desired->valid) || (semiDesired && semiDesired->valid) || (firstChoice && firstChoice->valid))
				continue; // never do inclusive if the desired slot is already in use

			qboolean gotPlayerWithZeroPermutations = qfalse;
			for (int j = 0; j < MAX_CLIENTS; j++) {
				sortedClient_t *cl = set->clients + j;
				if (!cl->accountName[0])
					continue;

#ifndef DEBUG_GENERATETEAMS
				// make sure this guy hasn't gone spec; we don't want to count him as left out
				// if he decided to become a spectator after the pug proposal passed.
				// sync path only -- on the async path dispatch already filtered the roster
				if (!precomputedClients && cl->clientNum >= 0 && cl->clientNum < MAX_CLIENTS) {
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

				int numPermutationsThisPlayer = NumPermutationsOfPlayer(cl->accountId, mostPlayed, highestCaliber, fairest, desired, inclusive, semiDesired, firstChoice);
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
			if (!tunables->enableFirstChoice)
				continue;

			if ((desired && desired->valid) || (semiDesired && semiDesired->valid) || (inclusive && inclusive->valid))
				continue; // never do first choice if the desired slot is already in use

			if (!(mostPlayed && mostPlayed->valid) && !(highestCaliber && highestCaliber->valid) && !(fairest && fairest->valid))
				continue;

			qboolean gotPlayerWithNoFirstChoice = qfalse;
			for (int j = 0; j < MAX_CLIENTS; j++) {
				sortedClient_t *cl = set->clients + j;
				if (!cl->accountName[0])
					continue;

#ifndef DEBUG_GENERATETEAMS
				// make sure this guy hasn't gone spec; we don't want to count him as left out
				// if he decided to become a spectator after the pug proposal passed.
				// sync path only -- on the async path dispatch already filtered the roster
				if (!precomputedClients && cl->clientNum >= 0 && cl->clientNum < MAX_CLIENTS) {
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
		
		{
			gprng_t typeRng;
			GPRNG_Seed(&typeRng, seedBase + (type * 100));
			qsort(&sortedClients, MAX_CLIENTS, sizeof(sortedClient_t), SortClientsForTeamGenerator);
			FisherYatesShuffle_R(&sortedClients, numEligible, sizeof(sortedClient_t), &typeRng);
		}

		permutationPlayer_t *players = calloc(numEligible, sizeof(permutationPlayer_t));
		int index = 0;
		for (int i = 0; i < numEligible; i++) {
			sortedClient_t *client = sortedClients + i;
			permutationPlayer_t *algoPlayer = players + index++;

			// fetch some information about this player
			playerRating_t findMe = { 0 };
			findMe.accountId = client->accountId;
			playerRating_t *positionRatings = snapshot ? TeamGen_SnapshotFindRating(snapshot, findMe.accountId) : ListFind(&level.ratingList, PlayerRatingAccountIdMatches, &findMe, NULL);
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

			algoPlayer->ttlholdRank = client->ttlholdRank;

			qboolean okayToUsePreference = qtrue;
			mostPlayedPos_t findMe2 = { 0 };
			findMe2.accountId = findMe.accountId;
			mostPlayedPos_t *mostPlayedPositions = snapshot ? TeamGen_SnapshotFindMostPlayed(snapshot, findMe2.accountId) : ListFind(&level.mostPlayedPositionsList, MostPlayedPosMatches, &findMe2, NULL);

			// determine what information we will feed the permutation evaluator based on which pass we're on
			algoPlayer->accountId = findMe.accountId;
			Q_strncpyz(algoPlayer->accountName, client->accountName, sizeof(algoPlayer->accountName));
			algoPlayer->clientNum = client->clientNum;
			if (type == TEAMGENERATORTYPE_MOSTPLAYED && !tunables->aDietB) {
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

					// in any case, rate everything a- or higher
					for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
						if (positionRatings->rating[pos] >= PlayerTierToRating(PLAYERRATING_LOW_A) - 0.0001)
							algoPlayer->rating[pos] = positionRatings->rating[pos];
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
			else {
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
							else { // they have a legit rating on *some* position; reduce it by one minor tier and use that for this position
								int fabricatedTier = lowestTier - 1;
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

			// unrate banned positions
			for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
				if ((client->bannedPos & (1 << pos)))
					algoPlayer->rating[pos] = 0;
			}

		}

		// ?d-specific stuff (counting number of permutations people are in, etc). needs a/b/c first.
		if (type == TEAMGENERATORTYPE_INCLUSIVE) {
			for (int j = 0; j < numEligible; j++) {
				permutationPlayer_t *algoPlayer = players + j;
				if (!algoPlayer->accountName[0])
					continue;

				algoPlayer->numPermutationsIn = NumPermutationsOfPlayer(algoPlayer->accountId, mostPlayed, highestCaliber, fairest, NULL, NULL, NULL, NULL);
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

				algoPlayer->numPermutationsIn = NumPermutationsOfPlayer(algoPlayer->accountId, mostPlayed, highestCaliber, fairest, NULL, NULL, NULL, NULL);

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
		else if (type == TEAMGENERATORTYPE_FIRSTCHOICE || type == TEAMGENERATORTYPE_DESIREDPOS) {
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
			int numHighCaliberPlayers = 0;
			for (int j = 0; j < numEligible; j++) {
				permutationPlayer_t *p = &players[j];
				for (int pos = CTFPOSITION_BASE; pos <= CTFPOSITION_OFFENSE; pos++) {
					if (p->rating[pos] >= PlayerTierToRating(PLAYERRATING_MID_B) - 0.0001) {
						++numHighCaliberPlayers;
						break; // don't double count him
					}
				}
			}

			if (numHighCaliberPlayers < 8) {
				usedSemiTryhardAlgoForB = qtrue;
				callback = TryTeamPermutation_SemiTryhard; // prefer fairness followed by hc
#ifdef DEBUG_GENERATETEAMS
				Com_Printf("Using TryTeamPermutation_SemiTryhard for TEAMGENERATORTYPE_HIGHESTRATING (%d hc)\n", numHighCaliberPlayers);
				TeamGen_DebugPrintf("<font color=darkgreen>==========Using TryTeamPermutation_SemiTryhard==========</font><br/>");
#endif
			}
			else {
				callback = TryTeamPermutation_Tryhard; // prefer hc
#ifdef DEBUG_GENERATETEAMS
				Com_Printf("Using TryTeamPermutation_Tryhard (%d hc)\n", numHighCaliberPlayers);
				TeamGen_DebugPrintf("<font color=darkgreen>==========Using TryTeamPermutation_Tryhard==========</font><br/>");
#endif
			}
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
		else if (tunables->aDietB) {
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

		uint64_t thisGotten = PermuteTeams_Dispatch(&players[0], numEligible, &try1, callback, qtrue, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qtrue, type, enforceImbalanceCaps, seedBase, tunables, useThreaded, typeDebugLog, abortFlag);
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
			thisGotten = PermuteTeams_Dispatch(&players[0], numEligible, &try1, callback, qfalse, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qtrue, type, enforceImbalanceCaps, seedBase, tunables, useThreaded, typeDebugLog, abortFlag);
			if (thisGotten > gotten)
				gotten = thisGotten;

			if (!try1.valid) {
				TeamGen_DebugPrintf("<font color=orange>==========No valid permutation without chase rule for type %d; trying again with chase rule but without banning avoided pos==========</font><br/>", type);
				allowSecondTry = qfalse;

				memset(&try1, 0, sizeof(try1));
				try1.iDiff = 999999999;
				try1.totalNumPermutations = 999999;
				try1.totalSkill = 0;
				thisGotten = PermuteTeams_Dispatch(&players[0], numEligible, &try1, callback, qtrue, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps, seedBase, tunables, useThreaded, typeDebugLog, abortFlag);
				if (thisGotten > gotten)
					gotten = thisGotten;

				if (!try1.valid) {
					TeamGen_DebugPrintf("<font color=orange>==========No valid permutation with chase rule but without banning avoided pos for type %d; trying again without chase rule AND without banning avoided pos==========</font><br/>", type);
					memset(&try1, 0, sizeof(try1));
					try1.iDiff = 999999999;
					try1.totalNumPermutations = 999999;
					try1.totalSkill = 0;
					thisGotten = PermuteTeams_Dispatch(&players[0], numEligible, &try1, callback, qfalse, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps, seedBase, tunables, useThreaded, typeDebugLog, abortFlag);
					if (thisGotten > gotten)
						gotten = thisGotten;
				}
			}
		}

		// if the permutation banning avoided positions is not 50-50, try again without banning avoided pos
		permutationOfTeams_t *thisPermutation;
		if (allowSecondTry) {
			if ((type == TEAMGENERATORTYPE_HIGHESTRATING || (type == TEAMGENERATORTYPE_MOSTPLAYED && tunables->aDietB)) && try1.valid) {
				TeamGen_DebugPrintf("<font color=orange>==========Attempting HC type %d without banning avoided pos to see if we can do better==========</font><br/>", type);
				thisGotten = PermuteTeams_Dispatch(&players[0], numEligible, &try2, callback, qtrue, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps, seedBase, tunables, useThreaded, typeDebugLog, abortFlag);
				if (thisGotten > gotten)
					gotten = thisGotten;

				if (!try2.valid) {
					TeamGen_DebugPrintf("<font color=orange>==========No valid permutation for HC type %d without banning avoided pos; trying again without chase rule==========</font><br/>", type);
					memset(&try2, 0, sizeof(try2));
					try2.iDiff = 999999999;
					try2.totalNumPermutations = 999999;
					try2.totalSkill = 0;
					thisGotten = PermuteTeams_Dispatch(&players[0], numEligible, &try2, callback, qfalse, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps, seedBase, tunables, useThreaded, typeDebugLog, abortFlag);
					if (thisGotten > gotten)
						gotten = thisGotten;
				}

				if (try2.valid) {
#if 0
					if (try2.totalSkill > try1.totalSkill || (try2.totalSkill == try1.totalSkill && try2.iDiff < try1.iDiff)) {
						thisPermutation = &try2;
						TeamGen_DebugPrintf("<font color=orange>==========Second pass on type %d with banned avoided pos yields higher skill or fairer result; using avoided pos unbanned permutation==========</font><br/>", type);
					}
#else
					if (qtrue) {
						thisPermutation = &try2;
						TeamGen_DebugPrintf("<font color=orange>==========Forcing second pass on type %d (fixme: save unnecessary computation)==========</font><br/>", type);
					}
#endif
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
				thisGotten = PermuteTeams_Dispatch(&players[0], numEligible, &try2, callback, qtrue, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps, seedBase, tunables, useThreaded, typeDebugLog, abortFlag);
				if (thisGotten > gotten)
					gotten = thisGotten;

				if (!try2.valid) {
					TeamGen_DebugPrintf("<font color=orange>==========No valid permutation for type %d without banning avoided pos; trying again without chase rule==========</font><br/>", type);
					memset(&try2, 0, sizeof(try2));
					try2.iDiff = 999999999;
					try2.totalNumPermutations = 999999;
					try2.totalSkill = 0;
					thisGotten = PermuteTeams_Dispatch(&players[0], numEligible, &try2, callback, qfalse, &listOfAvoidedHashesPlusHashesGottenOnThisGeneration, qfalse, type, enforceImbalanceCaps, seedBase, tunables, useThreaded, typeDebugLog, abortFlag);
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
			permutationOfTeams_t *compare = TeamGen_PermutationPtrForType(compareType, mostPlayed, highestCaliber, fairest, inclusive, desired, semiDesired, firstChoice);

			if (compare && compare->valid && compare->hash == thisPermutation->hash) {
				weAlreadyGotThisHash = compareType;
				break;
			}
		}
		if (weAlreadyGotThisHash != -1) {
			TeamGen_DebugPrintf("<font color=red>==========For type %d: we already got this hash back on type %d. Not using it.==========</font><br/>", type, weAlreadyGotThisHash);
			continue;
		}

		// copy this permutation to the appropriate supplied pointer
		permutationOfTeams_t *target = TeamGen_PermutationPtrForType(type, mostPlayed, highestCaliber, fairest, inclusive, desired, semiDesired, firstChoice);
		if (target) {
			memcpy(target, thisPermutation, sizeof(permutationOfTeams_t));
			avoidedHash_t *avoid = ListAdd(&listOfAvoidedHashesPlusHashesGottenOnThisGeneration, sizeof(avoidedHash_t));
			avoid->hash = thisPermutation->hash;
			++numValid;
		}

	}

	// if we ended up using semitryhard for b (such that both a and b used the exact same algo), put the more hc or more imbalanced of a/b into b
	if (usedSemiTryhardAlgoForB && mostPlayed && highestCaliber && (mostPlayed->totalSkill > highestCaliber->totalSkill || mostPlayed->iDiff > highestCaliber->iDiff)) {
		TeamGen_DebugPrintf("<font color=gold>==========usedSemiTryhardAlgoForB: swapping ?a and ?b back==========</font><br/>");
		permutationOfTeams_t tmp;
		memcpy(&tmp, mostPlayed, sizeof(permutationOfTeams_t));
		memcpy(mostPlayed, highestCaliber, sizeof(permutationOfTeams_t));
		memcpy(highestCaliber, &tmp, sizeof(permutationOfTeams_t));
	}

	if (!numValid && enforceImbalanceCaps) {
		TeamGen_DebugPrintf("<font color=blue>!!!!!!!!Did not get any valid permutations, retrying without enforcing imbalance caps!!!!!!!!</font><br/>");
		// wipe what this failed pass recorded -- the retry re-evaluates every type with looser constraints,
		// and the report should show only that final attempt
		if (debugLogsOut)
			memset(debugLogsOut, 0, sizeof(teamGenDebugLog_t) * NUM_TEAMGENERATORTYPES);
		// this list was seeded with a copy of set->avoidedHashesList and is about to go out of scope via
		// the tail-return below, which skips the normal ListClear. free it or every node leaks.
		ListClear(&listOfAvoidedHashesPlusHashesGottenOnThisGeneration);
		return GenerateTeams(set, mostPlayed, highestCaliber, fairest, desired, inclusive, semiDesired, firstChoice, numPermutations, qfalse, seedBase, tunables, precomputedClients, precomputedNumEligible, precomputedNumIngame, snapshot, debugLogsOut, abortFlag);
	}

	if (numPermutations)
		*numPermutations = gotten;

#ifdef DEBUG_GENERATETEAMS
	clock_t end = clock();
	double computeTimePrint = ((double)(end - start)) / CLOCKS_PER_SEC;
	Com_DebugPrintf("Total compute time: %0.2f seconds\n", computeTimePrint);
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

// worst case: base 1 iteration (16+ players) x 128 escalation levels. if this goes higher, recheck
// the ~1MB default Windows thread stack against permutations[][] below.
#define MAX_TEAMGEN_ITERATIONS 128

// wrapper for calling GenerateTeams several times with the goal of finding the fairest distribution of player slots
// seedBase/precomputedClients/snapshot follow the same contract as GenerateTeams.
// debugLogsOut, if non-NULL, gets a heap-allocated array for the winning iteration; caller frees.
static qboolean GenerateTeamsIteratively(pugProposal_t *set, permutationOfTeams_t *mostPlayed, permutationOfTeams_t *highestCaliber, permutationOfTeams_t *fairest, permutationOfTeams_t *desired, permutationOfTeams_t *inclusive, permutationOfTeams_t *semiDesired, permutationOfTeams_t *firstChoice, uint64_t *numPermutations,
	unsigned int seedBase, const teamGenTunables_t *tunables, const sortedClient_t *precomputedClients, int precomputedNumEligible, int precomputedNumIngame, teamGenDataSnapshot_t *snapshot, teamGenDebugLog_t **debugLogsOut, int *iterationsRunOut, int *bestIterationIndexOut,
	int iterationMultiplier, int *baseIterationsOut, volatile int *abortFlag) {
	double numPlayers = 0.0f;
	for (int j = 0; j < MAX_CLIENTS; j++) {
		sortedClient_t *cl = set->clients + j;
		if (!cl->accountName[0])
			continue;
		numPlayers += 1.0;
	}

	// scale down the number of iterations if we have more people so that we don't have insanely long lag spikes (based on testing)
	int numIterationsToDo;
	if (tunables->iterate) {
		if (tunables->enableInclusive) { // we can afford reduce the number of iterations (causes more lag per iteration anyway)
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

	// the true per-player-count base, before escalation multiplies it -- used to compute how far a
	// roster can escalate without exceeding MAX_TEAMGEN_ITERATIONS
	if (baseIterationsOut)
		*baseIterationsOut = numIterationsToDo;

	// escalation only multiplies when iterate is on -- forcing g_vote_teamgen_iterate 0 must not get
	// silently re-enabled underneath
	if (tunables->iterate)
		numIterationsToDo *= (iterationMultiplier >= 1 ? iterationMultiplier : 1);

	if (numIterationsToDo > MAX_TEAMGEN_ITERATIONS) {
		assert(qfalse); // idiot
		numIterationsToDo = MAX_TEAMGEN_ITERATIONS;
	}

	// async path: the job's own deterministic seed. sync path: the global, as always.
	unsigned int originalSeed = snapshot ? seedBase : teamGenSeed;
	// NOT static -- this runs concurrently on up to MAX_CONCURRENT_TEAMGEN_JOBS coordinator threads.
	// ~399KB of stack, fine against Linux's 8MB and Windows' ~1MB.
	permutationOfTeams_t permutations[MAX_TEAMGEN_ITERATIONS][NUM_TEAMGENERATORTYPES];
	memset(&permutations, 0, sizeof(permutations));
	double sumOfSquares[MAX_TEAMGEN_ITERATIONS] = { 999999 };
	int numPermutationsPerIteration[MAX_TEAMGEN_ITERATIONS] = { 0 };
	double weight[MAX_TEAMGEN_ITERATIONS] = { 0 };

	// per-iteration logs, only when the caller wants a report; every non-winning one is freed below
	teamGenDebugLog_t *iterDebugLogs[MAX_TEAMGEN_ITERATIONS] = { 0 };
	if (debugLogsOut)
		*debugLogsOut = NULL;

	TeamGen_Initialize();

	if (!snapshot) {
		// async path: the main thread already refreshed this before dispatching
		G_DBGetPlayerRatings();
	}

	// run teamgen a bunch of times
	int numEvaluated = 0;
	for (int i = 0; i < MAX_TEAMGEN_ITERATIONS && i < numIterationsToDo; i++) {
		// cheap insurance on top of the real abort check below
		if (abortFlag && GAtomic_Get(abortFlag))
			break;

		// kept local now (never written to the global teamGenSeed), so this is thread-safe
		unsigned int iterationSeed = originalSeed + (i * 29);

		if (debugLogsOut)
			iterDebugLogs[i] = (teamGenDebugLog_t *)calloc(NUM_TEAMGENERATORTYPES, sizeof(teamGenDebugLog_t));

		// generate them according to this seed
		GenerateTeams(set, &permutations[i][0], &permutations[i][1], &permutations[i][2], &permutations[i][3], &permutations[i][4], &permutations[i][5], &permutations[i][6], numPermutations, qtrue,
			iterationSeed, tunables, precomputedClients, precomputedNumEligible, precomputedNumIngame, snapshot, iterDebugLogs[i], abortFlag);

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
			double numPermutationsThisPlayer = (double)NumPermutationsOfPlayer(cl->accountId, &permutations[i][0], &permutations[i][1], &permutations[i][2], &permutations[i][3], &permutations[i][4], &permutations[i][5], &permutations[i][6]);
#ifdef DEBUG_GENERATETEAMS
			Com_Printf("/%s %d/", cl->accountName, (int)numPermutationsThisPlayer); // print the number of permutations each player is part of
#endif
			double addMe = (numPermutationsThisPlayer - goal) * (numPermutationsThisPlayer - goal);
			sum += addMe;

			if (tunables->sumOfSquaresTiebreaker) {
				double thisPlayerWeight = WeightOfPlayer(cl->accountId, &permutations[i][0], &permutations[i][1], &permutations[i][2], &permutations[i][3], &permutations[i][4], &permutations[i][5], snapshot);
				weight[i] += thisPlayerWeight;
			}
		}

#ifdef DEBUG_GENERATETEAMS
		Com_Printf(" ; sum of squares %f (%d permutations); weight %g\n", sum, (int)numValidPermutations, weight[i]);
#endif

		sumOfSquares[i] = sum;

		++numEvaluated;

		if (!tunables->sumOfSquaresTiebreaker) {
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
			(tunables->sumOfSquaresTiebreaker && fabs(sumOfSquares[i] - lowestSumOfSquares) <= 0.001 && weight[i] > weightOfLowestSumOfSquares + 0.0001) // weightier
			) {
			lowestNumPermutations = numPermutationsPerIteration[i];
			lowestSumOfSquares = sumOfSquares[i];
			bestIndex = i;

			if (tunables->sumOfSquaresTiebreaker) {
				weightOfLowestSumOfSquares = weight[i];
			}
			else {
				if (sumOfSquares[i] <= 0 /*&& numPermutationsPerIteration[i] == 3*/)
					break; // we got a perfect one; no need to keep iterating
			}
		}
	}

	// keep only the winning iteration's log; the rest is churn from discarded attempts
	for (int i = 0; i < MAX_TEAMGEN_ITERATIONS; i++) {
		if (i == bestIndex)
			continue;
		free(iterDebugLogs[i]);
	}
	if (debugLogsOut && bestIndex != -1)
		*debugLogsOut = iterDebugLogs[bestIndex];
	if (iterationsRunOut)
		*iterationsRunOut = numEvaluated;
	if (bestIterationIndexOut)
		*bestIterationIndexOut = bestIndex;

	if (!snapshot) {
		// sync path only -- the async path never touched the global
		teamGenSeed = originalSeed;
	}

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

void GetCurrentPickablePlayers(sortedClient_t *sortedClientsOut, int *numEligibleOut, int *numIngameEligibleOut, qboolean shuffle, qboolean refreshRatings) {
	// refresh ratings from db. callers that only track/hash the pickable roster can skip it -- whoever
	// dispatches off the result refreshes right before using them anyway
	if (refreshRatings)
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
				!(other->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST && !Q_stricmpclean(other->client->pers.netname, "elo BOT")) &&
				other->client->sess.clientType == CLIENT_TYPE_NORMAL) {
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

// optional visibility into the job lifecycle, gated by g_teamgen_threadDebugChat: 0 off, 1
// everyone, 2 admins. only called at state transitions, so it can't spam. main thread only.
void TeamGen_AnnounceThreadDebug(const char *msg) {
	if (!g_teamgen_threadDebugChat.integer || !VALIDSTRING(msg))
		return;

#ifdef DEBUG_GENERATETEAMS
	// chat only reaches connected clients, which makes this useless with only z_debugN fake players.
	// echo to the console too, but only in this debug build.
	Com_Printf("^6[teamgen debug]^7 %s\n", msg);
#endif

	if (g_teamgen_threadDebugChat.integer == 1) {
		TeamGenerator_QueueServerMessageInChat(-1, va("^6[teamgen debug]^7 %s", msg));
		return;
	}

	// == 2 (or anything else non-zero): admins only
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *ent = &g_entities[i];
		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || !ent->client->account)
			continue;
		if (!(ent->client->account->flags & ACCOUNTFLAG_ADMIN))
			continue;
		TeamGenerator_QueueServerMessageInChat(i, va("^6[teamgen debug]^7 %s", msg));
	}
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

static int ConvertCaliber(double raw) {
	if (raw < 45)
		return 1;
	if (raw >= 45 && raw < 60)
		return 2;
	if (raw >= 60 && raw < 70)
		return 3;
	if (raw >= 70 && raw < 75)
		return 4;
	if (raw >= 75 && raw < 80)
		return 5;
	if (raw >= 80 && raw < 85)
		return 6;
	if (raw >= 85 && raw < 90)
		return 7;
	if (raw >= 90 && raw < 95)
		return 8;
	if (raw >= 95 && raw < 100)
		return 9;
	return 10 + (int)((raw - 100) / 5);
}

// returns 0 if invalid
int GetCaliber(permutationOfTeams_t *p) {
	if (!p || !p->valid) {
		assert(qfalse);
		return 0;
	}

	double totalRating = p->teams[0].rawStrength + p->teams[1].rawStrength;
	Com_DebugPrintf("\n\nTotal rating: %g\n", totalRating);
	double averageRating = (totalRating / 8.0);
	Com_DebugPrintf("Average Rating: %g\n", averageRating);
	if (!averageRating) {
		assert(qfalse);
		return 0; // prevent division by zero
	}

	const double A = 5;
	Com_DebugPrintf("A (Penalty Exponent): %g\n", A);
	double HC_base = (averageRating - PlayerTierToRating(PLAYERRATING_UNRATED + 1)) / (PlayerTierToRating(NUM_PLAYERRATINGS - 1) - PlayerTierToRating(PLAYERRATING_UNRATED + 1));
	Com_DebugPrintf("HC_base: %g\n", HC_base);

	double penalty = 0.0;
	for (int i = 0; i < 8; i++) {
		double rating;
		switch (i) {
#if 0
		case 0: rating = (double)trap_Cvar_VariableValue("z_debugRating1"); break;
		case 1: rating = (double)trap_Cvar_VariableValue("z_debugRating2"); break;
		case 2: rating = (double)trap_Cvar_VariableValue("z_debugRating3"); break;
		case 3: rating = (double)trap_Cvar_VariableValue("z_debugRating4"); break;
		case 4: rating = (double)trap_Cvar_VariableValue("z_debugRating5"); break;
		case 5: rating = (double)trap_Cvar_VariableValue("z_debugRating6"); break;
		case 6: rating = (double)trap_Cvar_VariableValue("z_debugRating7"); break;
		case 7: rating = (double)trap_Cvar_VariableValue("z_debugRating8"); break;
#else
		case 0: rating = p->teams[0].baseRating; break;
		case 1: rating = p->teams[0].chaseRating; break;
		case 2: rating = p->teams[0].offense1Rating; break;
		case 3: rating = p->teams[0].offense2Rating; break;
		case 4: rating = p->teams[1].baseRating; break;
		case 5: rating = p->teams[1].chaseRating; break;
		case 6: rating = p->teams[1].offense1Rating; break;
		case 7: rating = p->teams[1].offense2Rating; break;
#endif
		default: assert(qfalse);
		}
		double deviation = averageRating - rating;
		if (deviation + 0.0001 < PLAYERRATING_DECIMAL_INCREMENT)
			deviation = 0; // disregard very low deviation
		if (rating + 0.0001 >= PlayerTierToRating(PLAYERRATING_LOW_A))
			deviation = 0; // don't let A- or higher count as deviation

		if (deviation > 0) {
			double deviationExponent = -A * (deviation / averageRating);
			double eToTheExponent = pow(2.71828182846, deviationExponent);
			double additionalPenalty = (1 - eToTheExponent);
			double oldPenalty = penalty;
			penalty += additionalPenalty;
			Com_DebugPrintf("Player %d Rating: %g, Deviation: %g, Exponent %g, e^exponent: %g, additional penalty %g, adding this to penalty %g gives new penalty %g\n",
				i + 1, rating, deviation, deviationExponent, eToTheExponent, additionalPenalty, oldPenalty, penalty);
		}
		else {
			Com_DebugPrintf("Player %d Rating: %g, Deviation: %g\n", i + 1, rating, deviation);
		}
	}

	Com_DebugPrintf("Dividing penalty %g by 8: penalty is now %g\n", penalty, penalty / 8);
	penalty /= 8;

	double HC_normalized = HC_base * (1 - penalty);
	Com_DebugPrintf("HC_normalized: %g\n", HC_normalized);

	// todo: calculate these dynamically
	const double B = 126.67;
	const double C = 0;

	double HC = (B * HC_normalized) + C;

	Com_DebugPrintf("Caliber calculation: (%g x %g) + %g = final result of %g\n", B, HC_normalized, C, HC);

	double HC_without_penalty = (B * HC_base) + C;
	Com_DebugPrintf("Caliber with no penalty: %g\n", HC_without_penalty);
	Com_DebugPrintf("Converted without penalty: %d\n", ConvertCaliber(HC_without_penalty));
	Com_DebugPrintf("Penalty accounted for: %g\n", (HC_without_penalty - HC));
	Com_DebugPrintf("Caliber with penalty: %g\n", HC);

	int converted = ConvertCaliber(HC);

	Com_DebugPrintf("Converted: %d\n", converted);

	return converted;
}

// appends " tag" to buf, or "tag" if empty. renders via va() first since Com_sprintf'ing buf into
// itself would be self-referential.
static void TeamGen_AppendSuggestionTag(char *buf, size_t bufSize, const char *tag) {
	Com_sprintf(buf, bufSize, !buf[0] ? tag : va("%s %s", buf, tag));
}

// clientNum -1 for everyone
void PrintTeamsProposalsInConsole(pugProposal_t *set, int clientNum) {
	assert(set);
	char formattedNumber[64] = { 0 };
	FormatNumberToStringWithCommas(set->numValidPermutationsChecked, formattedNumber, sizeof(formattedNumber));
	TeamGenerator_QueueServerMessageInConsole(clientNum, va("Team generator results for %s\n(%s valid permutations evaluated):\n", set->namesStr, formattedNumber));

	int lowestIDiff = 999999;
	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		permutationOfTeams_t *thisPermutation = TeamGen_PermutationPtrForType(i, &set->suggested, &set->highestCaliber, &set->fairest, &set->inclusive, &set->desired, &set->semiDesired, &set->firstChoice);
		if (thisPermutation->valid && thisPermutation->iDiff < lowestIDiff)
			lowestIDiff = thisPermutation->iDiff;
	}

	int iHighestCombinedStrength = 0;
	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		permutationOfTeams_t *thisPermutation = TeamGen_PermutationPtrForType(i, &set->suggested, &set->highestCaliber, &set->fairest, &set->inclusive, &set->desired, &set->semiDesired, &set->firstChoice);
		if (!thisPermutation->valid)
			continue;

		double combinedStrength = thisPermutation->teams[0].rawStrength + thisPermutation->teams[1].rawStrength;
		int thisCombinedStrength = (int)round(combinedStrength * 10000);
		if (thisCombinedStrength > iHighestCombinedStrength)
			iHighestCombinedStrength = thisCombinedStrength;
	}

	int numTiedForHighestCombinedStrength = 0;
	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		permutationOfTeams_t *thisPermutation = TeamGen_PermutationPtrForType(i, &set->suggested, &set->highestCaliber, &set->fairest, &set->inclusive, &set->desired, &set->semiDesired, &set->firstChoice);
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
		permutationOfTeams_t *thisPermutation = TeamGen_PermutationPtrForType(i, &set->suggested, &set->highestCaliber, &set->fairest, &set->inclusive, &set->desired, &set->semiDesired, &set->firstChoice);
		if (!thisPermutation->valid)
			continue;

		double thisCombinedStrength = thisPermutation->teams[0].rawStrength + thisPermutation->teams[1].rawStrength;
		int iThisCombinedStrength = (int)round(thisCombinedStrength * 10000);

		int caliber = GetCaliber(thisPermutation);

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
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^1[ULTRA OMEGA STACK!!!]");
		else if (tags & (1 << TEAMGENTAG_STACK2))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^1[MEGA STACK!!]");
		else if (tags & (1 << TEAMGENTAG_STACK))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^8[possible stack!]");

#ifdef PRINT_UNDERDOG_TIEBREAKER
		if (underdogTeam == TEAM_RED)
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^1[Map vote tie goes to red]");
		else if (underdogTeam == TEAM_BLUE)
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^4[Map vote tie goes to blue]");
#endif

		if (tags & (1 << TEAMGENTAG_FAIR))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^2[even]");
		else if (tags & (1 << TEAMGENTAG_FAIREST))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^2[most even]");


		if (g_vote_teamgen_displayCaliber.integer && caliber > 0) { // caliber 0 is invalid
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), va("^6[c%d]", caliber));
		}
		else {
			if (tags & (1 << TEAMGENTAG_TIEDFORMOSTHC))
				TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^3[tied for most HC]");
			else if (tags & (1 << TEAMGENTAG_MOSTHC))
				TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^3[most HC]");

#if 0
			if (!rerollNum && i == TEAMGENERATORTYPE_HIGHESTRATING && (tags & (1 << TEAMGENTAG_MOSTHC) || tags & (1 << TEAMGENTAG_TIEDFORMOSTHC)))
				TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^5[spec/capt]");
#endif
		}

		if (tags & (1 << TEAMGENTAG_HC3))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^3[***ULTRA OMEGA HC!!!***]");
		else if (tags & (1 << TEAMGENTAG_HC2))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^3[**MEGA HC!!**]");
		else if (tags & (1 << TEAMGENTAG_HC))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^3[*HC!*]");

		if (tags & (1 << TEAMGENTAG_NOAVOIDS))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^7[no avoids]");

		if (tags & (1 << TEAMGENTAG_8FIRSTCHOICEPOS))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^7[8 1st choices]");
		else if (tags & (1 << TEAMGENTAG_7FIRSTCHOICEPOS))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^7[7 1st choices]");
		else if (tags & (1 << TEAMGENTAG_6FIRSTCHOICEPOS))
			TeamGen_AppendSuggestionTag(suggestionTypeStr, sizeof(suggestionTypeStr), "^7[6 1st choices]");

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

	if (clientNum == -1) {
		TeamGenerator_ResetConfirmationsAndMutes();

		typedef struct {
			permutationOfTeams_t *perm;
			char letter;
		} permChoice_t;

		permChoice_t candidates[7] = {
			{ &set->suggested,     set->suggestedLetter },
			{ &set->fairest,       set->fairestLetter },
			{ &set->desired,       set->desiredLetter },
			{ &set->highestCaliber,set->highestCaliberLetter },
			{ &set->inclusive,     set->inclusiveLetter },
			{ &set->semiDesired,   set->semiDesiredLetter },
			{ &set->firstChoice,   set->firstChoiceLetter }
		};

		// collect only valid ones
		permChoice_t valid[7];
		int numValid = 0;
		for (int i = 0; i < 7; i++) {
			if (candidates[i].perm && candidates[i].perm->valid) {
				valid[numValid++] = candidates[i];
			}
		}

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (!numValid) { // shouldn't happen
				Com_Printf("ActivatePugProposal: No valid permutations available for confirmation.\n");
				set->confirmation[i].permutation = NULL;
				set->confirmation[i].letter = '?';
				set->confirmation[i].pos = CTFPOSITION_UNKNOWN;
			}
			else {
				srand(teamGenSeed + i);
				const int rng1 = rand();
				const int rng2 = rand();
				srand(time(NULL));
				permChoice_t *chosen = &valid[rng1 % numValid];

				srand(teamGenSeed);
				ctfPosition_t pos;
				switch (rng2 % 3) {
				case 0: pos = CTFPOSITION_BASE; break;
				case 1: pos = CTFPOSITION_CHASE; break;
				case 2: pos = CTFPOSITION_OFFENSE; break;
				}

				set->confirmation[i].permutation = chosen->perm;
				set->confirmation[i].letter = chosen->letter;
				set->confirmation[i].pos = pos;
			}
		}

		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *ent = &g_entities[i];
			if (!ent || !ent->client || !ent->client->account || !(ent->client->account->flags & ACCOUNTFLAG_ULTRAINSTAVOTETROLL))
				continue;
			TeamGenerator_QueueServerMessageInConsole(ent - g_entities, va(
				level.activePugProposal->confirmation[ent - g_entities].pos == CTFPOSITION_OFFENSE ?
				"^1[Requirement for trolls]^7 As a troll, you must confirm that you have read the teams proposals by naming one ^3%s^7 player from each team in permutation ^3%c^7 before voting. Use the format:   ^5%cconfirm name1 name2^7\n" :
				"^1[Requirement for trolls]^7 As a troll, you must confirm that you have read the teams proposals by naming the ^3%s^7 players in permutation ^3%c^7 before voting. Use the format:   ^5%cconfirm name1 name2^7\n",
				NameForPos(level.activePugProposal->confirmation[ent - g_entities].pos),
				level.activePugProposal->confirmation[ent - g_entities].letter,
				TEAMGEN_CHAT_COMMAND_CHARACTER));
		}
	}
	else {
		gentity_t *ent = &g_entities[clientNum];
		if (ent && ent->client && ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_ULTRAINSTAVOTETROLL)) {
			TeamGenerator_QueueServerMessageInConsole(ent - g_entities, va(
				level.activePugProposal->confirmation[ent - g_entities].pos == CTFPOSITION_OFFENSE ?
				"^1[Requirement for trolls]^7 As a troll, you must confirm that you have read the teams proposals by naming one ^3%s^7 player from each team in permutation ^3%c^7 before voting. Use the format:   ^5%cconfirm name1 name2^7\n" :
				"^1[Requirement for trolls]^7 As a troll, you must confirm that you have read the teams proposals by naming the ^3%s^7 players in permutation ^3%c^7 before voting. Use the format:   ^5%cconfirm name1 name2^7\n",
				NameForPos(level.activePugProposal->confirmation[ent - g_entities].pos),
				level.activePugProposal->confirmation[ent - g_entities].letter,
				TEAMGEN_CHAT_COMMAND_CHARACTER));
		}
	}

	if (clientNum == -1)
		level.teamPermutationsShownTime = trap_Milliseconds();
}

// ============================================================================
// async team-generation job dispatch.
//
// a job hands a whole GenerateTeamsIteratively call to a coordinator thread so the server tick
// isn't blocked. that thread never touches g_entities/level/the real pugProposal_t -- everything
// it needs is snapshotted by the main thread first. on completion TeamGenAsync_PollCompletedJobs
// re-validates that the job is still relevant (proposal exists, epoch and roster hash unchanged)
// before applying it.
// ============================================================================

// forward decl -- PollCompletedJobs needs to activate a proposal whose quorum was reached while a
// different one was generating
static void ActivatePugProposal(pugProposal_t *set, qboolean forcedByServer);

typedef enum {
	TEAMGENJOB_INITIAL,
	TEAMGENJOB_REROLL,
	// speculative: computed in the background right after results are shown so a real ?reroll can
	// apply instantly. never sets the proposal to GENERATING.
	TEAMGENJOB_REROLL_LOOKAHEAD,
	// speculative: computed whenever the pickable pool has been stable for a bit, so ?start can apply
	// instantly. not tied to any proposal.
	TEAMGENJOB_SPECULATIVE_ROSTER
} teamGenJobKind_t;

#define MAX_CONCURRENT_TEAMGEN_JOBS 4

typedef struct {
	qboolean			inUse;
	teamGenJobKind_t	kind;
	int					proposalNum;
	int					epochAtDispatch;
	XXH32_hash_t		rosterAtDispatch; // identity of the eligible roster this was computed from
	qboolean			forcedByServer;
	int					rerollNumUsed;			// the rerollNum this job's seed/generation corresponds to
	// multiplies the base iteration count. 1 for everything except SPECULATIVE_ROSTER, which escalates
	// 1x, 2x, 3x... up to a dynamic cap.
	int					iterationMultiplier;
	// dual purpose: for REROLL_LOOKAHEAD, avoidedHashesList's identity hash at dispatch; for
	// SPECULATIVE_ROSTER, the roster's own hash. unused otherwise.
	unsigned int		avoidedHashesTagAtDispatch;

	// job-owned; never touches the real pugProposal_t (see TeamGen_CloneProposalForJob)
	pugProposal_t		*jobSet;

	teamGenDataSnapshot_t dataSnapshot;
	int					numEligible;
	int					numIngame;
	unsigned int		seedBase;
	teamGenTunables_t	tunables;

	gthread_t			*thread;
	volatile int		done; // 0 = running, 1 = complete (set by coordinator via GAtomic_Set/Get)
	// 0 = run, 1 = stop ASAP -- checked periodically on the coordinator thread. the output fields are
	// never read once this is set, since everyone who sets it also joins-and-discards the job.
	volatile int		abortRequested;

	// outputs, written only by the coordinator thread before `done` is set
	qboolean			succeeded;
	permutationOfTeams_t suggested, highestCaliber, fairest, desired, inclusive, semiDesired, firstChoice;
	uint64_t			numValidPermutationsChecked;
	// NULL unless debug logging is on AND kind is INITIAL/REROLL -- speculative jobs mostly get thrown
	// away, so building a report for them is wasted. ownership passes to the completion handler.
	teamGenDebugLog_t	*debugLogs;
	int					elapsedMs;
	int					iterationsRun;
	int					bestIterationIndex;
	// the base iteration count before iterationMultiplier -- used to compute a roster's escalation cap
	int					baseIterations;
} teamGenJob_t;

static teamGenJob_t g_teamGenJobs[MAX_CONCURRENT_TEAMGEN_JOBS];

// speculative pickable-roster precompute state -- not tied to any pugProposal_t
typedef struct {
	qboolean valid;
	XXH32_hash_t rosterHash;
	int iterationLevel; // which escalation level (1x, 2x, ...) produced this result
	int maxLevel; // this roster's dynamic ceiling, mirrored from the cache for TeamGen_LockSpeculativeRosterCacheEntry's use at consume time
	permutationOfTeams_t suggested, highestCaliber, fairest, desired, inclusive, semiDesired, firstChoice;
	uint64_t numValidPermutationsChecked;
} speculativeRosterResult_t;

static speculativeRosterResult_t g_speculativeRosterResult;
static XXH32_hash_t g_lastPickableRosterHash = 0;
static int g_pickableRosterStableSinceMs = 0;
static qboolean g_pickableRosterSpeculativeDispatched = qfalse;
// escalation levels dispatched for the live-tracked roster so far this streak -- separate from the
// cache below, which persists per-roster progress across disconnect/reconnect blips
static int g_pickableRosterEscalationLevel = 0;

// per-match cache of the most thorough (or, once locked, the actually-shown) speculative result for
// each exact account-id set. this is what stops a disconnect+reconnect from letting someone
// downgrade or upgrade what was already shown for that roster. plain LRU-by-touch via ListTrim,
// with no protection for locked entries -- evicting one under enough churn IS the timer-free expiry.
typedef struct {
	node_t			node; // must be first -- see collections.h
	qboolean		locked; // frozen once qtrue: reused verbatim forever (until evicted), never re-derived in either direction
	XXH32_hash_t	rosterHash;
	int				iterationLevel; // highest level reached (unlocked) / that produced the frozen result (locked)
	int				maxLevel; // this roster's dynamic ceiling = MAX_TEAMGEN_ITERATIONS / baseIterations; 0 = not yet learned
	permutationOfTeams_t suggested, highestCaliber, fairest, desired, inclusive, semiDesired, firstChoice;
	uint64_t		numValidPermutationsChecked;
} speculativeRosterCacheEntry_t;

#define SPECULATIVE_ROSTER_CACHE_SIZE 8

// how long the pool must sit unchanged before the FIRST precompute fires. later levels have no
// delay at all, so this was never worth a cvar.
#define SPECULATIVE_ROSTER_DEBOUNCE_MS 1000

static list_t g_speculativeRosterCache = { 0 };

// highest multiplier that keeps baseIterations x level under MAX_TEAMGEN_ITERATIONS
static int TeamGen_ComputeMaxEscalationLevel(int baseIterations) {
	return baseIterations >= 1 ? MAX_TEAMGEN_ITERATIONS / baseIterations : MAX_TEAMGEN_ITERATIONS;
}

// keyed by the ELIGIBLE roster's hash -- what every dispatch stores in rosterAtDispatch. a
// proposal's own clients[] is not that: it's fixed at creation and can still list players who went
// spectator or got barred, so hashing it would lock a key nothing else ever uses.
static XXH32_hash_t TeamGen_EligibleRosterHashForProposal(const pugProposal_t *set) {
	sortedClient_t nowClients[MAX_CLIENTS];
	int nowEligible = 0, nowIngame = 0;
	TeamGen_ComputeCurrentEligibleRoster(set->clients, nowClients, &nowEligible, &nowIngame);
	return HashPugProposal(nowClients);
}

static qboolean SpeculativeRosterCacheEntryMatchesHash(genericNode_t *node, void *userData) {
	const speculativeRosterCacheEntry_t *existing = (const speculativeRosterCacheEntry_t *)node;
	const XXH32_hash_t *hash = (const XXH32_hash_t *)userData;

	if (existing && hash && existing->rosterHash == *hash)
		return qtrue;

	return qfalse;
}

static speculativeRosterCacheEntry_t *TeamGen_FindSpeculativeRosterCacheEntry(XXH32_hash_t hash) {
	return (speculativeRosterCacheEntry_t *)ListFind(&g_speculativeRosterCache, SpeculativeRosterCacheEntryMatchesHash, &hash, NULL);
}

// removes any existing entry and re-adds at the tail, then trims from the head -- LRU-by-touch
static speculativeRosterCacheEntry_t *TeamGen_TouchSpeculativeRosterCacheEntry(XXH32_hash_t hash) {
	speculativeRosterCacheEntry_t *existing = TeamGen_FindSpeculativeRosterCacheEntry(hash);
	if (existing)
		ListRemove(&g_speculativeRosterCache, existing);

	speculativeRosterCacheEntry_t *entry = (speculativeRosterCacheEntry_t *)ListAdd(&g_speculativeRosterCache, sizeof(speculativeRosterCacheEntry_t));
	if (!entry)
		return NULL; // alloc failure -- extremely unlikely for an 8-entry cache; callers tolerate NULL

	entry->rosterHash = hash;
	ListTrim(&g_speculativeRosterCache, SPECULATIVE_ROSTER_CACHE_SIZE, qtrue); // keep only the SPECULATIVE_ROSTER_CACHE_SIZE most-recently-touched
	return entry;
}

// called on every successful SPECULATIVE_ROSTER completion. overwrites only if not locked and the
// level improves.
static void TeamGen_UpdateSpeculativeRosterCache(XXH32_hash_t hash, int iterationLevel, int maxLevel,
	const permutationOfTeams_t *suggested, const permutationOfTeams_t *highestCaliber, const permutationOfTeams_t *fairest,
	const permutationOfTeams_t *desired, const permutationOfTeams_t *inclusive, const permutationOfTeams_t *semiDesired,
	const permutationOfTeams_t *firstChoice, uint64_t numValidPermutationsChecked) {

	speculativeRosterCacheEntry_t *existing = TeamGen_FindSpeculativeRosterCacheEntry(hash);
	if (existing && (existing->locked || iterationLevel <= existing->iterationLevel))
		return; // frozen, or not an improvement -- nothing to do

	speculativeRosterCacheEntry_t *entry = TeamGen_TouchSpeculativeRosterCacheEntry(hash);
	if (!entry)
		return;
	entry->locked = qfalse;
	entry->iterationLevel = iterationLevel;
	entry->maxLevel = maxLevel;
	entry->suggested = *suggested; entry->highestCaliber = *highestCaliber; entry->fairest = *fairest;
	entry->desired = *desired; entry->inclusive = *inclusive; entry->semiDesired = *semiDesired; entry->firstChoice = *firstChoice;
	entry->numValidPermutationsChecked = numValidPermutationsChecked;
}

// called the moment a roster is actually used in a real pass. no-op if already locked, which is
// what makes it immune to both downgrade and upgrade.
static void TeamGen_LockSpeculativeRosterCacheEntry(XXH32_hash_t hash, int iterationLevel, int maxLevel,
	const permutationOfTeams_t *suggested, const permutationOfTeams_t *highestCaliber, const permutationOfTeams_t *fairest,
	const permutationOfTeams_t *desired, const permutationOfTeams_t *inclusive, const permutationOfTeams_t *semiDesired,
	const permutationOfTeams_t *firstChoice, uint64_t numValidPermutationsChecked) {

	speculativeRosterCacheEntry_t *existing = TeamGen_FindSpeculativeRosterCacheEntry(hash);
	if (existing && existing->locked)
		return; // already frozen

	// the touch helper removes+re-adds rather than updating in place, so maxLevel has to be passed
	// explicitly rather than surviving from `existing`
	speculativeRosterCacheEntry_t *entry = TeamGen_TouchSpeculativeRosterCacheEntry(hash);
	if (!entry)
		return;
	entry->iterationLevel = iterationLevel;
	entry->maxLevel = maxLevel;
	entry->suggested = *suggested; entry->highestCaliber = *highestCaliber; entry->fairest = *fairest;
	entry->desired = *desired; entry->inclusive = *inclusive; entry->semiDesired = *semiDesired; entry->firstChoice = *firstChoice;
	entry->numValidPermutationsChecked = numValidPermutationsChecked;
	entry->locked = qtrue;
}

// called after a real ?reroll. unlike locking, this overwrites even a locked entry -- a reroll
// deliberately replaces what's authoritative for this roster.
static void TeamGen_UpdateSpeculativeRosterCacheAfterReroll(XXH32_hash_t hash, int iterationLevel,
	const permutationOfTeams_t *suggested, const permutationOfTeams_t *highestCaliber, const permutationOfTeams_t *fairest,
	const permutationOfTeams_t *desired, const permutationOfTeams_t *inclusive, const permutationOfTeams_t *semiDesired,
	const permutationOfTeams_t *firstChoice, uint64_t numValidPermutationsChecked) {

	speculativeRosterCacheEntry_t *existing = TeamGen_FindSpeculativeRosterCacheEntry(hash);
	int maxLevel = existing ? existing->maxLevel : 0; // preserve -- a reroll doesn't change the roster's base/cap

	speculativeRosterCacheEntry_t *entry = TeamGen_TouchSpeculativeRosterCacheEntry(hash);
	if (!entry)
		return;
	entry->iterationLevel = iterationLevel;
	entry->maxLevel = maxLevel;
	entry->suggested = *suggested; entry->highestCaliber = *highestCaliber; entry->fairest = *fairest;
	entry->desired = *desired; entry->inclusive = *inclusive; entry->semiDesired = *semiDesired; entry->firstChoice = *firstChoice;
	entry->numValidPermutationsChecked = numValidPermutationsChecked;
	entry->locked = qtrue; // still frozen -- a reroll replaces WHAT's frozen, not whether it's frozen
}

// throws away completed speculative results too, not just in-flight jobs: the live result, the
// (deliberately sticky) roster cache, and each proposal's lookahead could all still be applied.
static void TeamGen_InvalidateSpeculativeResults(void) {
	memset(&g_speculativeRosterResult, 0, sizeof(g_speculativeRosterResult));
	ListClear(&g_speculativeRosterCache);
	g_lastPickableRosterHash = 0;
	g_pickableRosterStableSinceMs = 0;
	g_pickableRosterSpeculativeDispatched = qfalse;
	g_pickableRosterEscalationLevel = 0;

	iterator_t iter;
	ListIterate(&level.pugProposalsList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		pugProposal_t *p = IteratorNext(&iter);
		memset(&p->speculativeReroll, 0, sizeof(p->speculativeReroll));
		p->rerollLookaheadDispatched = qfalse;
		p->rerollLookaheadEscalationLevel = 0;
		p->rerollLookaheadMaxLevel = 0;
	}
}

// is a real, player-visible generation in flight? speculative jobs deliberately don't count -- they
// run nearly continuously, so counting them would starve real work.
static qboolean TeamGen_RealJobInFlight(void) {
	for (int i = 0; i < MAX_CONCURRENT_TEAMGEN_JOBS; i++) {
		const teamGenJob_t *job = &g_teamGenJobs[i];
		if (job->inUse && (job->kind == TEAMGENJOB_INITIAL || job->kind == TEAMGENJOB_REROLL))
			return qtrue;
	}
	return qfalse;
}

// same, but for one proposal -- lets a repeat quorum vote on the proposal already generating be
// ignored rather than queued
static qboolean TeamGen_RealJobInFlightForProposal(int proposalNum) {
	for (int i = 0; i < MAX_CONCURRENT_TEAMGEN_JOBS; i++) {
		const teamGenJob_t *job = &g_teamGenJobs[i];
		if (job->inUse && job->proposalNum == proposalNum &&
			(job->kind == TEAMGENJOB_INITIAL || job->kind == TEAMGENJOB_REROLL))
			return qtrue;
	}
	return qfalse;
}

static void TeamGen_FreeJobProposalClone(pugProposal_t *clone); // defined below

// frees a slot by stopping a speculative job so a real ?start/?reroll is never refused for lack of
// one. returns the freed slot, or NULL if no speculative job was running.
static teamGenJob_t *TeamGen_EvictOneSpeculativeJob(void) {
	for (int i = 0; i < MAX_CONCURRENT_TEAMGEN_JOBS; i++) {
		teamGenJob_t *job = &g_teamGenJobs[i];
		if (!job->inUse || (job->kind != TEAMGENJOB_SPECULATIVE_ROSTER && job->kind != TEAMGENJOB_REROLL_LOOKAHEAD))
			continue;

		GAtomic_Set(&job->abortRequested, 1);
		if (job->thread) {
			GThread_Join(job->thread); // bounded: workers sample abortRequested inside their loop
			job->thread = NULL;
		}
		// clear whichever "attempt in flight" latch this job owns -- nothing else will, and leaving it set
		// disables that precompute for good
		if (job->kind == TEAMGENJOB_SPECULATIVE_ROSTER) {
			g_pickableRosterSpeculativeDispatched = qfalse;
		}
		else { // TEAMGENJOB_REROLL_LOOKAHEAD
			iterator_t iter;
			ListIterate(&level.pugProposalsList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				pugProposal_t *p = IteratorNext(&iter);
				if (p->num == job->proposalNum && p->epoch == job->epochAtDispatch) {
					p->rerollLookaheadDispatched = qfalse;
					break;
				}
			}
		}
		TeamGen_FreeJobProposalClone(job->jobSet);
		job->jobSet = NULL;
		free(job->debugLogs);
		job->debugLogs = NULL;
		job->inUse = qfalse;
		return job;
	}
	return NULL;
}

static teamGenJob_t *TeamGen_FindFreeJobSlot(void) {
	for (int i = 0; i < MAX_CONCURRENT_TEAMGEN_JOBS; i++) {
		if (!g_teamGenJobs[i].inUse)
			return &g_teamGenJobs[i];
	}
	return NULL;
}

// deterministic, reconnect-immune base seed: a pure function of the sorted account ids plus the
// proposal's sequence number. never slot indices, connection order or time(NULL), so a
// disconnect/reconnect that doesn't change the roster can't perturb the result.
static unsigned int TeamGen_ComputeBaseSeed(const sortedClient_t *sortedClients, int numEligible, int proposalNum, int rerollNumForSeed) {
	int ids[MAX_CLIENTS];
	int n = 0;
	// all MAX_CLIENTS slots, skipping empties -- this array is sparse (see TeamGen_BuildDataSnapshot).
	// stopping at numEligible would miss high slots and fold empty slots' accountId 0 into the hash.
	for (int i = 0; i < MAX_CLIENTS && n < MAX_CLIENTS; i++) {
		if (!sortedClients[i].accountName[0])
			continue;
		ids[n++] = sortedClients[i].accountId;
	}

	// insertion sort -- n is small, and it must be order-independent so iteration order never affects the seed
	for (int i = 1; i < n; i++) {
		int key = ids[i];
		int j = i - 1;
		while (j >= 0 && ids[j] > key) {
			ids[j + 1] = ids[j];
			j--;
		}
		ids[j + 1] = key;
	}

	XXH32_hash_t hash = XXH32(ids, sizeof(int) * n, 0);
	hash ^= (XXH32_hash_t)proposalNum * 0x9E3779B1u;
	hash ^= (XXH32_hash_t)rerollNumForSeed * 0x85EBCA6Bu;
	return (unsigned int)hash;
}

// job-owned deep copy of just the fields GenerateTeams touches, so the coordinator thread never
// reads the real main-thread-owned proposal
static pugProposal_t *TeamGen_CloneProposalForJob(pugProposal_t *set) {
	pugProposal_t *clone = (pugProposal_t *)calloc(1, sizeof(pugProposal_t));
	if (!clone)
		return NULL;
	memcpy(clone->clients, set->clients, sizeof(clone->clients));
	clone->num = set->num;
	ListCopy(&set->avoidedHashesList, &clone->avoidedHashesList, sizeof(avoidedHash_t));
	return clone;
}

static void TeamGen_FreeJobProposalClone(pugProposal_t *clone) {
	if (!clone)
		return;
	ListClear(&clone->avoidedHashesList);
	free(clone);
}

// signals every job to stop, then joins them. signal-all-then-join-all so they all hear about it at
// once, rather than job 2 waiting out job 1's join first.
void TeamGen_AbortAndReapAllJobs(void) {
	int startMs = trap_Milliseconds();

	for (int i = 0; i < MAX_CONCURRENT_TEAMGEN_JOBS; i++)
		if (g_teamGenJobs[i].inUse)
			GAtomic_Set(&g_teamGenJobs[i].abortRequested, 1);

	for (int i = 0; i < MAX_CONCURRENT_TEAMGEN_JOBS; i++) {
		teamGenJob_t *job = &g_teamGenJobs[i];
		if (!job->inUse)
			continue;

		// always join, even if `done` is set: the thread still has to unwind through the trampoline, which
		// lives in this module image -- and the engine dlclose()s us on every map change. skipping it also
		// leaked the slot, filling the job table after a few maps. the result is discarded either way, but
		// the per-kind cleanup below still has to run or the proposal is stranded in GENERATING forever.
		if (job->thread) {
			GThread_Join(job->thread);
			job->thread = NULL;
		}

		// discarding here bypasses the per-kind completion handling that normally records "the attempt is
		// over". replicate just enough of it. matters most for TeamGen_SetDispatchSuppressed, which can
		// leave the match running afterward.
		if (job->kind == TEAMGENJOB_SPECULATIVE_ROSTER) {
			g_pickableRosterSpeculativeDispatched = qfalse;
		}
		else if (job->kind == TEAMGENJOB_REROLL_LOOKAHEAD) {
			iterator_t iter;
			ListIterate(&level.pugProposalsList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				pugProposal_t *p = IteratorNext(&iter);
				if (p->num == job->proposalNum && p->epoch == job->epochAtDispatch) {
					p->rerollLookaheadDispatched = qfalse;
					break;
				}
			}
		}
		else { // TEAMGENJOB_INITIAL or TEAMGENJOB_REROLL -- a real,
			// a player-visible proposal is waiting in GENERATING for this. fail it like "no valid team split
			// found" would rather than strand it forever (skipping the debug-report write -- not worth the
			// latency for a job we're trying to stop fast).
			iterator_t iter;
			ListIterate(&level.pugProposalsList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				pugProposal_t *p = IteratorNext(&iter);
				if (p->num != job->proposalNum || p->epoch != job->epochAtDispatch)
					continue;
				if (job->kind == TEAMGENJOB_INITIAL) {
					TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d %s (%s). However, unable to generate teams; pug proposal %d terminated.",
						p->num, job->forcedByServer ? "force passed by server" : "passed", p->namesStr, p->num));
					level.activePugProposal = NULL;
					TeamGenerator_ResetConfirmationsAndMutes();
					ListClear(&p->avoidedHashesList);
					ListRemove(&level.pugProposalsList, p);
				}
				else { // TEAMGENJOB_REROLL -- fall back to keeping the existing (pre-reroll) teams votable
					TeamGenerator_QueueServerMessageInChat(-1, va("Failed to generate different teams when rerolling pug proposal %d%s (%s). Voting will continue for the existing teams.",
						p->num, job->forcedByServer ? " by server" : "", p->namesStr));
					p->state = PUGPROPOSAL_STATE_READY_FOR_TEAM_VOTE;
				}
				break;
			}
		}

		TeamGen_FreeJobProposalClone(job->jobSet);
		job->jobSet = NULL;
		// same as the stale-job path in PollCompletedJobs -- nobody will consume these logs, so free them
		free(job->debugLogs);
		job->debugLogs = NULL;
		job->inUse = qfalse;
	}

	// anything already finished is just as stale as what we stopped -- every caller of this is
	// invalidating the data they were computed from
	TeamGen_InvalidateSpeculativeResults();

	// this blocks the main thread on GThread_Join above; abortRequested is what keeps that short
	// (without it, a large roster meant minutes). log it if it's ever slow. typically single-digit ms.
	int elapsedMs = trap_Milliseconds() - startMs;
	if (elapsedMs >= 5) {
		Com_Printf("^3[teamgen timing]^7 TeamGen_AbortAndReapAllJobs blocked the main thread for %dms while stopping in-flight job(s).\n", elapsedMs);
	}
}

// two independent dispatch gates. they must stay separate: the countdown one is driven from
// G_RunFrame every frame with a fresh value, so a shared flag would be cleared the next frame after
// any one-shot caller set it.
// resumable: a restart countdown can be canceled (AFK detection) and resume.
static qboolean g_teamGenRestartCountdownSuppressed = qfalse;
// one-shot: a decided map vote/intermission means this module is going away and nothing can call it
// off. only TeamGenAsync_Shutdown clears it.
static qboolean g_teamGenMapChangePending = qfalse;

static qboolean TeamGen_DispatchIsSuppressed(void) {
	return g_teamGenRestartCountdownSuppressed || g_teamGenMapChangePending;
}

void TeamGen_SetDispatchSuppressed(qboolean suppressed) {
	if (suppressed == g_teamGenRestartCountdownSuppressed)
		return; // no state transition, nothing to do
	g_teamGenRestartCountdownSuppressed = suppressed;
	if (suppressed) {
		TeamGen_AnnounceThreadDebug("Restart countdown is imminent; stopping background teamgen threads and pausing new dispatches.");
		TeamGen_AbortAndReapAllJobs();
	}
	else if (!g_teamGenMapChangePending) {
		// don't claim we're resuming if the map-change latch still has us stopped
		TeamGen_AnnounceThreadDebug("Restart countdown no longer imminent; resuming teamgen dispatch.");
	}
}

// advance notice that a map change is committed. stops everything and refuses further dispatch,
// including precompute, which would otherwise re-arm and start a job that can only be thrown away.
void TeamGen_SetMapChangePending(void) {
	if (g_teamGenMapChangePending)
		return;
	g_teamGenMapChangePending = qtrue;
	TeamGen_AnnounceThreadDebug("Map change is imminent; stopping background teamgen threads and pausing new dispatches.");
	TeamGen_AbortAndReapAllJobs();
}

static void TeamGenCoordinator_Run(void *arg) {
	teamGenJob_t *job = (teamGenJob_t *)arg;

	// GThread_Milliseconds(), not trap_Milliseconds() -- this runs on the coordinator thread, and every
	// trap_* call routes through the engine's syscall dispatcher, main thread only
	int startMs = GThread_Milliseconds();

	// only the two kinds a player can actually see a report for bother building one -- speculative jobs
	// mostly get thrown away. job->tunables, not the live cvar: coordinator thread.
	qboolean wantDebugLog = job->tunables.wantDebugLog &&
		(job->kind == TEAMGENJOB_INITIAL || job->kind == TEAMGENJOB_REROLL);

	job->succeeded = GenerateTeamsIteratively(job->jobSet, &job->suggested, &job->highestCaliber, &job->fairest, &job->desired, &job->inclusive, &job->semiDesired, &job->firstChoice, &job->numValidPermutationsChecked,
		job->seedBase, &job->tunables, job->jobSet->clients, job->numEligible, job->numIngame, &job->dataSnapshot, wantDebugLog ? &job->debugLogs : NULL, &job->iterationsRun, &job->bestIterationIndex,
		job->iterationMultiplier, &job->baseIterations, &job->abortRequested);

	job->elapsedMs = GThread_Milliseconds() - startMs;

	GAtomic_Set(&job->done, 1);
}

// cheap order-dependent identity hash over an avoidedHashesList -- only used to detect "has anything
// been added since I looked", not for uniqueness
static unsigned int TeamGen_HashAvoidedHashesList(list_t *list) {
	unsigned int h = (unsigned int)list->size;
	iterator_t iter;
	ListIterate(list, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		avoidedHash_t *entry = IteratorNext(&iter);
		h = h * 31u + (unsigned int)entry->hash;
	}
	return h;
}

// snapshots everything on the main thread and dispatches a coordinator thread to run
// GenerateTeamsIteratively against it. used by both ActivatePugProposal (INITIAL) and
// TeamGenerator_DoReroll (REROLL) -- structurally the same operation with a different seed.
// rerollNumForSeed lets the caller dispatch one ahead for a speculative lookahead.
static qboolean TeamGen_DispatchGenerationJob(pugProposal_t *set, teamGenJobKind_t kind, qboolean forcedByServer, int rerollNumForSeed, int iterationMultiplier) {
	if (TeamGen_DispatchIsSuppressed()) {
		// a restart countdown or map change is imminent -- refuse new work rather than dispatch something
		// that gets aborted a moment later. SPECULATIVE_ROSTER retries every frame while suppressed, so
		// messaging for it would spam every player sv_fps times a second; only tell players about a real
		// ?start/?reroll being refused.
		if (kind != TEAMGENJOB_REROLL_LOOKAHEAD && kind != TEAMGENJOB_SPECULATIVE_ROSTER)
			TeamGenerator_QueueServerMessageInChat(-1, "Team generator is temporarily paused (restart pending); please try again in a moment.");
		return qfalse;
	}

	teamGenJob_t *job = TeamGen_FindFreeJobSlot();
	if (!job && (kind == TEAMGENJOB_INITIAL || kind == TEAMGENJOB_REROLL)) {
		// a real generation outranks background precompute -- reclaim a speculative slot rather than telling
		// the player to try again (which also tears the proposal down in ActivatePugProposal)
		job = TeamGen_EvictOneSpeculativeJob();
	}
	if (!job) {
		// only reachable for speculative kinds now; fail quietly for the lookahead, which is silent by design
		if (kind != TEAMGENJOB_REROLL_LOOKAHEAD && kind != TEAMGENJOB_SPECULATIVE_ROSTER)
			TeamGenerator_QueueServerMessageInChat(-1, "Team generator is busy; please try again in a moment.");
		return qfalse;
	}

	// main thread only (trap_sqlite3_*/g_entities). only refresh for a real ?start/?reroll: a roster
	// change doesn't invalidate anyone's rating, and Cmd_Rating_f/G_DeleteAccount already refresh on a
	// real change. ttlhold ranks used to be refreshed here too at ~347ms a go; they only change when a
	// pug is played, so G_DBInitializePugStatsCache loads them once per map instead.
	if (kind == TEAMGENJOB_INITIAL || kind == TEAMGENJOB_REROLL)
		G_DBGetPlayerRatings();

	sortedClient_t sortedClients[MAX_CLIENTS];
	int numEligible = 0, numIngame = 0;
	TeamGen_ComputeCurrentEligibleRoster(set->clients, sortedClients, &numEligible, &numIngame);

	if (numEligible < 8) {
		if (kind != TEAMGENJOB_REROLL_LOOKAHEAD)
			TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d %s. However, not enough eligible players remain to generate teams; pug proposal %d terminated.", set->num, forcedByServer ? "force passed by server" : "passed", set->num));
		return qfalse;
	}

	memset(job, 0, sizeof(*job));
	job->inUse = qtrue;
	job->kind = kind;
	job->proposalNum = set->num;
	job->epochAtDispatch = set->epoch;
	job->rosterAtDispatch = HashPugProposal(sortedClients);
	job->forcedByServer = forcedByServer;
	job->rerollNumUsed = rerollNumForSeed;
	job->iterationMultiplier = iterationMultiplier >= 1 ? iterationMultiplier : 1;
	job->avoidedHashesTagAtDispatch = (kind == TEAMGENJOB_SPECULATIVE_ROSTER)
		? (unsigned int)HashPugProposal(sortedClients)
		: TeamGen_HashAvoidedHashesList(&set->avoidedHashesList);
	job->jobSet = TeamGen_CloneProposalForJob(set);
	if (!job->jobSet) {
		// allocation failure -- release the slot rather than leaving it permanently inUse
		job->inUse = qfalse;
		return qfalse;
	}
	memcpy(job->jobSet->clients, sortedClients, sizeof(job->jobSet->clients));
	job->numEligible = numEligible;
	job->numIngame = numIngame;
	TeamGen_BuildDataSnapshot(sortedClients, &job->dataSnapshot);
	job->seedBase = TeamGen_ComputeBaseSeed(sortedClients, numEligible, set->num, rerollNumForSeed);
	// captured here on the main thread so workers never read a live cvar. rerollNumForSeed, not
	// rerollNum: a lookahead predicts rerollNum + 1 and must score under that reroll's cap.
	TeamGen_CaptureTunables(&job->tunables);
	job->tunables.rerollNum = rerollNumForSeed;
	// precompute runs nearly continuously, so nice it down and let a real ?start win
	if (kind == TEAMGENJOB_SPECULATIVE_ROSTER || kind == TEAMGENJOB_REROLL_LOOKAHEAD)
		job->tunables.workerNice = job->tunables.workerNice + 5;

	if (kind == TEAMGENJOB_INITIAL || kind == TEAMGENJOB_REROLL) {
		TeamGen_AnnounceThreadDebug(va("Dispatching %s generation for pug proposal #%d (%d eligible players).",
			kind == TEAMGENJOB_REROLL ? "reroll" : "initial", set->num, numEligible));
	}
	else if (kind == TEAMGENJOB_REROLL_LOOKAHEAD) {
		TeamGen_AnnounceThreadDebug(va("Dispatching background reroll-lookahead precompute for pug proposal #%d (%d eligible players).",
			set->num, numEligible));
	}
	else { // TEAMGENJOB_SPECULATIVE_ROSTER
		TeamGen_AnnounceThreadDebug(va("Dispatching background speculative roster precompute (%d eligible players, escalation level %d).", numEligible, job->iterationMultiplier));
	}

	if (kind != TEAMGENJOB_REROLL_LOOKAHEAD) {
		// a lookahead must never block voting -- only a committed job puts the proposal into GENERATING
		set->state = PUGPROPOSAL_STATE_GENERATING;
	}

	if (!g_teamgen_asyncEnabled.integer) {
		// kill switch: run the whole generation inline on the main thread (which also takes the
		// single-threaded search path), so no thread is created. still completes through the normal path.
		TeamGenCoordinator_Run(job);
	}
	else if (!GThread_Create(&job->thread, TeamGenCoordinator_Run, job, qtrue, job->tunables.workerNice)) {
		// thread creation failed (extremely unlikely) -- run it inline rather than lose the work
		TeamGenCoordinator_Run(job);
	}

	return qtrue;
}

// dispatches a "what would the next ?reroll produce" job right after results are shown. consumed by
// TeamGenerator_DoReroll if its tag still matches, otherwise discarded. escalates while nobody
// rerolls, which is why the counters reset here.
static void TeamGen_DispatchRerollLookahead(pugProposal_t *set) {
	if (!g_vote_teamgen_rerollLookaheadEnabled.integer)
		return;
	// one escalation chain per proposal. this flag was previously written in five places and read in
	// none, so after an instant-consume reroll the old chain's next level was still in flight alongside
	// the new chain's level 1, burning a slot on work that then gets discarded.
	if (set->rerollLookaheadDispatched)
		return;
	set->rerollLookaheadEscalationLevel = 1;
	set->rerollLookaheadMaxLevel = 0; // unknown until level 1 completes
	if (TeamGen_DispatchGenerationJob(set, TEAMGENJOB_REROLL_LOOKAHEAD, qfalse, rerollNum + 1, 1))
		set->rerollLookaheadDispatched = qtrue;
}

// plain result payload shared by "a job completed" and "a lookahead is being consumed" -- both
// apply-result functions only need these fields, not a whole teamGenJob_t
typedef struct {
	qboolean succeeded;
	permutationOfTeams_t suggested, highestCaliber, fairest, desired, inclusive, semiDesired, firstChoice;
	uint64_t numValidPermutationsChecked;

	// NULL unless debug logging was on for this job. ownership passes to whichever Apply*Result call
	// consumes this payload, which writes (or discards) then frees it.
	teamGenDebugLog_t *debugLogs;
	teamGenJobKind_t kind;
	int numEligible, numIngame;
	unsigned int seedBase;
	int rerollNumUsed;
	int elapsedMs;
	int iterationsRun;
	int bestIterationIndex;
} teamGenResultPayload_t;

static void TeamGen_AddWinningHashesToAvoidedList(pugProposal_t *set, const teamGenResultPayload_t *result) {
	const permutationOfTeams_t *all[7] = { &result->suggested, &result->highestCaliber, &result->fairest, &result->desired, &result->inclusive, &result->semiDesired, &result->firstChoice };
	for (int i = 0; i < 7; i++) {
		if (all[i]->valid) {
			avoidedHash_t *avoid = ListAdd(&set->avoidedHashesList, sizeof(avoidedHash_t));
			avoid->hash = all[i]->hash;
		}
	}
}

// ============================================================================
// debug report: JSON serialization + main-thread-only file write. the cached HTML template supplies
// all of the look; this code only produces data.
// ============================================================================

static char *g_teamGenDebugTemplate = NULL;

void TeamGen_LoadDebugTemplate(void) {
	const char *filename = "teamgen-debug-template.html";
	fileHandle_t f;
	int len = trap_FS_FOpenFile(filename, &f, FS_READ);
	if (!f) {
		// loaded through the engine FS, which searches fs_homepath/fs_basepath and pk3s -- NOT the game/
		// source dir it's checked into. it has to be copied into the server's base/ folder.
		Com_Printf("TeamGen_LoadDebugTemplate: no %s found; g_teamgen_debugLog reports are unavailable. Copy it from the source tree's game/ folder into this server's base/ folder.\n", filename);
		return;
	}

	if (len <= 0) {
		trap_FS_FCloseFile(f);
		Com_Printf("TeamGen_LoadDebugTemplate: %s is empty; g_teamgen_debugLog reports will be unavailable.\n", filename);
		return;
	}

	// G_InitGame runs this on every map load, so release the previous copy rather than leaking one
	free(g_teamGenDebugTemplate);
	g_teamGenDebugTemplate = (char *)malloc(len + 1);
	if (!g_teamGenDebugTemplate) {
		trap_FS_FCloseFile(f);
		return;
	}
	trap_FS_Read(g_teamGenDebugTemplate, len, f);
	g_teamGenDebugTemplate[len] = 0;
	trap_FS_FCloseFile(f);
}

static char TeamGen_DebugTierLetter(ctfPlayerTier_t tier) {
	if (tier <= PLAYERRATING_HIGH_G) return 'g';
	if (tier <= PLAYERRATING_HIGH_F) return 'f';
	if (tier <= PLAYERRATING_HIGH_D) return 'd';
	if (tier <= PLAYERRATING_HIGH_C) return 'c';
	if (tier <= PLAYERRATING_HIGH_B) return 'b';
	if (tier <= PLAYERRATING_HIGH_A) return 'a';
	if (tier <= PLAYERRATING_HIGH_S) return 's';
	return 'x';
}

// slug for the comparator a type was scored with; the template maps it to that algo's fields
static const char *TeamGen_DebugAlgoSlug(int algo) {
	switch (algo) {
	case TEAMGENDEBUGALGO_REGULAR:			return "regular";
	case TEAMGENDEBUGALGO_APPEASING:		return "appeasing";
	case TEAMGENDEBUGALGO_FAIREST:			return "fairest";
	case TEAMGENDEBUGALGO_SEMIAPPEASING:	return "semiappeasing";
	case TEAMGENDEBUGALGO_FIRSTCHOICE:		return "firstchoice";
	case TEAMGENDEBUGALGO_INCLUSIVE:		return "inclusive";
	case TEAMGENDEBUGALGO_SEMITRYHARD:		return "semitryhard";
	case TEAMGENDEBUGALGO_TRYHARD:			return "tryhard";
	default:								return "unknown";
	}
}

static const char *TeamGen_DebugTypeSlug(int type) {
	switch (type) {
	case TEAMGENERATORTYPE_MOSTPLAYED: return "mostplayed";
	case TEAMGENERATORTYPE_HIGHESTRATING: return "highestrating";
	case TEAMGENERATORTYPE_FAIREST: return "fairest";
	case TEAMGENERATORTYPE_INCLUSIVE: return "inclusive";
	case TEAMGENERATORTYPE_DESIREDPOS: return "desiredpos";
	case TEAMGENERATORTYPE_SEMIDESIREDPOS: return "semidesiredpos";
	case TEAMGENERATORTYPE_FIRSTCHOICE: return "firstchoice";
	default: return "unknown";
	}
}

static const char *TeamGen_DebugTypeLabel(int type) {
	switch (type) {
	case TEAMGENERATORTYPE_MOSTPLAYED: return "Most Played";
	case TEAMGENERATORTYPE_HIGHESTRATING: return "Highest Rating";
	case TEAMGENERATORTYPE_FAIREST: return "Fairest";
	case TEAMGENERATORTYPE_INCLUSIVE: return "Inclusive";
	case TEAMGENERATORTYPE_DESIREDPOS: return "Desired Position";
	case TEAMGENERATORTYPE_SEMIDESIREDPOS: return "Semi-Desired Position";
	case TEAMGENERATORTYPE_FIRSTCHOICE: return "First Choice";
	default: return "Unknown";
	}
}

// chat letters (?a, ?b, ...) are assigned by PrintTeamsProposalsInConsole, which both Apply*Result
// calls run before this. returns 0 if a type never got one.
static char TeamGen_DebugLetterForType(const pugProposal_t *set, int type) {
	switch (type) {
	case TEAMGENERATORTYPE_MOSTPLAYED: return set->suggestedLetter;
	case TEAMGENERATORTYPE_HIGHESTRATING: return set->highestCaliberLetter;
	case TEAMGENERATORTYPE_FAIREST: return set->fairestLetter;
	case TEAMGENERATORTYPE_INCLUSIVE: return set->inclusiveLetter;
	case TEAMGENERATORTYPE_DESIREDPOS: return set->desiredLetter;
	case TEAMGENERATORTYPE_SEMIDESIREDPOS: return set->semiDesiredLetter;
	case TEAMGENERATORTYPE_FIRSTCHOICE: return set->firstChoiceLetter;
	default: return 0;
	}
}

static cJSON *TeamGen_DebugJsonPlayer(const teamGenDebugPlayer_t *p) {
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "name", p->name);
	char tierStr[2] = { TeamGen_DebugTierLetter(p->tier), 0 };
	cJSON_AddStringToObject(o, "tier", tierStr);
	cJSON_AddBoolToObject(o, "pref", p->pref);
	cJSON_AddBoolToObject(o, "avoid", p->avoid);
	return o;
}

static cJSON *TeamGen_DebugJsonTeam(const teamGenDebugTeam_t *t) {
	cJSON *o = cJSON_CreateObject();
	cJSON_AddItemToObject(o, "base", TeamGen_DebugJsonPlayer(&t->base));
	cJSON_AddItemToObject(o, "chase", TeamGen_DebugJsonPlayer(&t->chase));
	cJSON_AddItemToObject(o, "off1", TeamGen_DebugJsonPlayer(&t->off1));
	cJSON_AddItemToObject(o, "off2", TeamGen_DebugJsonPlayer(&t->off2));
	return o;
}

// seq < 0 means "this is the type's final result, not a numbered milestone".
static cJSON *TeamGen_DebugJsonMilestone(const teamGenDebugMilestone_t *m, int seq) {
	cJSON *o = cJSON_CreateObject();
	if (seq >= 0)
		cJSON_AddNumberToObject(o, "seq", seq);
	cJSON_AddNumberToObject(o, "iDiff", m->iDiff);
	cJSON_AddNumberToObject(o, "numOnPreferredPos", m->numOnPreferredPos);
	cJSON_AddNumberToObject(o, "numOnAvoidedPos", m->numOnAvoidedPos);
	cJSON_AddNumberToObject(o, "topTierImbalance", m->topTierImbalance);
	cJSON_AddNumberToObject(o, "bottomTierImbalance", m->bottomTierImbalance);
	cJSON_AddNumberToObject(o, "offenseDefenseDiff", m->offenseDefenseDiff);
	// totalSkill is stored x10000; emit it in rating units to match the strengths beside it
	cJSON_AddNumberToObject(o, "totalSkill", m->totalSkill / 10000.0);
	cJSON_AddNumberToObject(o, "totalNumPermutations", m->totalNumPermutations);
	cJSON_AddNumberToObject(o, "numSatisfiedCyds", m->numSatisfiedCyds);
	cJSON_AddNumberToObject(o, "numSatisfiedLgs", m->numSatisfiedLgs);
	cJSON_AddBoolToObject(o, "satisfiesNoobRule", m->satisfiesNoobRule);
	cJSON_AddBoolToObject(o, "satisfiesNoobCarryRule", m->satisfiesNoobCarryRule);
	cJSON_AddNumberToObject(o, "team1RawStrength", m->team1RawStrength);
	cJSON_AddNumberToObject(o, "team2RawStrength", m->team2RawStrength);
	cJSON_AddNumberToObject(o, "team1RelativeStrength", m->team1RelativeStrength);
	cJSON_AddNumberToObject(o, "team2RelativeStrength", m->team2RelativeStrength);
	cJSON_AddItemToObject(o, "team1", TeamGen_DebugJsonTeam(&m->team1));
	cJSON_AddItemToObject(o, "team2", TeamGen_DebugJsonTeam(&m->team2));
	return o;
}

// the accepted result doesn't come from the milestone list (the cap could have been hit before the
// winner was found) -- it's built from the permutationOfTeams_t the search settled on
static void TeamGen_DebugFinalPlayerFromTeamData(teamGenDebugPlayer_t *out, const pugProposal_t *set, int accountId, const char *name, double rating, ctfPosition_t pos) {
	Q_strncpyz(out->name, name, sizeof(out->name));
	out->tier = PlayerTierFromRating(rating);
	out->pref = qfalse;
	out->avoid = qfalse;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		const sortedClient_t *cl = &set->clients[i];
		if (!cl->accountName[0])
			continue;
		if (cl->accountId != accountId)
			continue;
		out->pref = (cl->posPrefs.first & (1 << pos)) ? qtrue : qfalse;
		out->avoid = (cl->posPrefs.avoid & (1 << pos)) ? qtrue : qfalse;
		break;
	}
}

static cJSON *TeamGen_DebugJsonFinal(const pugProposal_t *set, const permutationOfTeams_t *perm) {
	teamGenDebugMilestone_t m = { 0 };
	m.iDiff = perm->iDiff;
	m.numOnPreferredPos = perm->numOnPreferredPos;
	m.numOnAvoidedPos = perm->numOnAvoidedPos;
	m.topTierImbalance = perm->topTierImbalance;
	m.bottomTierImbalance = perm->bottomTierImbalance;
	m.offenseDefenseDiff = perm->offenseDefenseDiff;
	m.totalSkill = perm->totalSkill;
	m.totalNumPermutations = perm->totalNumPermutations;
	m.numSatisfiedCyds = perm->numSatisfiedCyds;
	m.numSatisfiedLgs = perm->numSatisfiedLgs;
	m.satisfiesNoobRule = perm->satisfiesNoobRule;
	m.satisfiesNoobCarryRule = perm->satisfiesNoobCarryRule;
	m.team1RawStrength = (float)perm->teams[0].rawStrength;
	m.team2RawStrength = (float)perm->teams[1].rawStrength;
	m.team1RelativeStrength = (float)perm->teams[0].relativeStrength;
	m.team2RelativeStrength = (float)perm->teams[1].relativeStrength;
	TeamGen_DebugFinalPlayerFromTeamData(&m.team1.base, set, perm->teams[0].baseId, perm->teams[0].baseName, perm->teams[0].baseRating, CTFPOSITION_BASE);
	TeamGen_DebugFinalPlayerFromTeamData(&m.team1.chase, set, perm->teams[0].chaseId, perm->teams[0].chaseName, perm->teams[0].chaseRating, CTFPOSITION_CHASE);
	TeamGen_DebugFinalPlayerFromTeamData(&m.team1.off1, set, perm->teams[0].offenseId1, perm->teams[0].offense1Name, perm->teams[0].offense1Rating, CTFPOSITION_OFFENSE);
	TeamGen_DebugFinalPlayerFromTeamData(&m.team1.off2, set, perm->teams[0].offenseId2, perm->teams[0].offense2Name, perm->teams[0].offense2Rating, CTFPOSITION_OFFENSE);
	TeamGen_DebugFinalPlayerFromTeamData(&m.team2.base, set, perm->teams[1].baseId, perm->teams[1].baseName, perm->teams[1].baseRating, CTFPOSITION_BASE);
	TeamGen_DebugFinalPlayerFromTeamData(&m.team2.chase, set, perm->teams[1].chaseId, perm->teams[1].chaseName, perm->teams[1].chaseRating, CTFPOSITION_CHASE);
	TeamGen_DebugFinalPlayerFromTeamData(&m.team2.off1, set, perm->teams[1].offenseId1, perm->teams[1].offense1Name, perm->teams[1].offense1Rating, CTFPOSITION_OFFENSE);
	TeamGen_DebugFinalPlayerFromTeamData(&m.team2.off2, set, perm->teams[1].offenseId2, perm->teams[1].offense2Name, perm->teams[1].offense2Rating, CTFPOSITION_OFFENSE);
	return TeamGen_DebugJsonMilestone(&m, -1);
}

static char *TeamGen_SerializeDebugReport(const pugProposal_t *set, const teamGenResultPayload_t *result) {
	cJSON *root = cJSON_CreateObject();

	char whenStr[128] = { 0 };
	time_t now = time(NULL);
	struct tm *timeParts = localtime(&now);
	strftime(whenStr, sizeof(whenStr), "%Y-%m-%d %H:%M:%S", timeParts);
	cJSON_AddStringToObject(root, "generatedAt", whenStr);
	cJSON_AddStringToObject(root, "jobKind", result->kind == TEAMGENJOB_REROLL ? "reroll" : "initial");
	cJSON_AddNumberToObject(root, "proposalNum", set->num);
	cJSON_AddNumberToObject(root, "numEligible", result->numEligible);
	cJSON_AddNumberToObject(root, "numIngame", result->numIngame);
	cJSON_AddNumberToObject(root, "seedBase", (double)result->seedBase);
	cJSON_AddNumberToObject(root, "rerollNum", result->rerollNumUsed);
	cJSON_AddNumberToObject(root, "elapsedMs", result->elapsedMs);
	cJSON_AddNumberToObject(root, "iterationsRun", result->iterationsRun);
	cJSON_AddNumberToObject(root, "bestIterationIndex", result->bestIterationIndex);

	cJSON *types = cJSON_AddArrayToObject(root, "types");
	for (int type = TEAMGENERATORTYPE_FIRST; type < NUM_TEAMGENERATORTYPES; type++) {
		const permutationOfTeams_t *perm;
		switch (type) {
		case TEAMGENERATORTYPE_MOSTPLAYED: perm = &result->suggested; break;
		case TEAMGENERATORTYPE_HIGHESTRATING: perm = &result->highestCaliber; break;
		case TEAMGENERATORTYPE_FAIREST: perm = &result->fairest; break;
		case TEAMGENERATORTYPE_INCLUSIVE: perm = &result->inclusive; break;
		case TEAMGENERATORTYPE_DESIREDPOS: perm = &result->desired; break;
		case TEAMGENERATORTYPE_SEMIDESIREDPOS: perm = &result->semiDesired; break;
		case TEAMGENERATORTYPE_FIRSTCHOICE: perm = &result->firstChoice; break;
		default: perm = NULL; break;
		}
		if (!perm)
			continue;

		const teamGenDebugLog_t *log = result->debugLogs ? &result->debugLogs[type] : NULL;

		cJSON *t = cJSON_CreateObject();
		cJSON_AddStringToObject(t, "type", TeamGen_DebugTypeSlug(type));
		cJSON_AddStringToObject(t, "label", TeamGen_DebugTypeLabel(type));
		char letterStr[2] = { TeamGen_DebugLetterForType(set, type), 0 };
		if (letterStr[0])
			cJSON_AddStringToObject(t, "letter", letterStr);
		cJSON_AddBoolToObject(t, "valid", perm->valid);

		if (log) {
			cJSON_AddStringToObject(t, "algo", TeamGen_DebugAlgoSlug(log->algo));
			cJSON_AddNumberToObject(t, "totalCandidatesEvaluated", log->numCandidatesEvaluated);

			cJSON *rejectionCounts = cJSON_AddObjectToObject(t, "rejectionCounts");
			cJSON_AddNumberToObject(rejectionCounts, "imbalanceCap", log->rejectImbalanceCap);
			cJSON_AddNumberToObject(rejectionCounts, "garbageTier", log->rejectGarbageTier);
			cJSON_AddNumberToObject(rejectionCounts, "chaseRule", log->rejectChaseRule);
			cJSON_AddNumberToObject(rejectionCounts, "avoidedHash", log->rejectAvoidedHash);
			cJSON_AddNumberToObject(rejectionCounts, "noSatisfiedLgs", log->rejectNoSatisfiedLgs);
			cJSON_AddNumberToObject(rejectionCounts, "noSatisfiedCyds", log->rejectNoSatisfiedCyds);

			cJSON *milestones = cJSON_AddArrayToObject(t, "milestones");
			for (int i = 0; i < log->numMilestones; i++)
				cJSON_AddItemToArray(milestones, TeamGen_DebugJsonMilestone(&log->milestones[i], i + 1));
			cJSON_AddNumberToObject(t, "milestonesTruncated", log->numMilestonesTruncated);
		}

		if (perm->valid)
			cJSON_AddItemToObject(t, "final", TeamGen_DebugJsonFinal(set, perm));

		cJSON_AddItemToArray(types, t);
	}

	char *jsonStr = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	if (!jsonStr)
		return NULL; // OOM; caller treats NULL as "no report"

	// the JSON is spliced into a <script> block, so a raw '<' in any string (an account name containing
	// "</script>") would close the element early. cJSON escapes '"' and '\' but not '<'/'>', so do
	// those here -- they only ever appear inside string literals and decode back identically.
	size_t numAngles = 0;
	for (const char *p = jsonStr; *p; p++)
		if (*p == '<' || *p == '>')
			numAngles++;
	if (!numAngles)
		return jsonStr;

	// each '<' or '>' becomes 6 chars (<), i.e. 5 extra
	char *escaped = (char *)malloc(strlen(jsonStr) + numAngles * 5 + 1);
	if (!escaped) {
		free(jsonStr);
		return NULL;
	}
	char *w = escaped;
	for (const char *p = jsonStr; *p; p++) {
		if (*p == '<' || *p == '>') {
			memcpy(w, *p == '<' ? "\\u003c" : "\\u003e", 6);
			w += 6;
		}
		else {
			*w++ = *p;
		}
	}
	*w = 0;
	free(jsonStr);
	return escaped; // caller frees with free()
}

// splices jsonData between the template's two marker comments. returns qfalse if they're missing --
// shouldn't happen, but a hand-edited template must not crash the server.
static qboolean TeamGen_SpliceDebugTemplate(const char *tmpl, const char *jsonData, char **outHtml) {
	static const char *startMarker = "/*__TEAMGEN_DATA__*/";
	static const char *endMarker = "/*__END_TEAMGEN_DATA__*/";

	const char *startPos = strstr(tmpl, startMarker);
	if (!startPos)
		return qfalse;
	startPos += strlen(startMarker);
	const char *endPos = strstr(startPos, endMarker);
	if (!endPos)
		return qfalse;

	size_t prefixLen = startPos - tmpl;
	size_t suffixLen = strlen(endPos); // excludes the trailing '\0'; the +1s below carry it
	size_t jsonLen = strlen(jsonData);

	char *html = (char *)malloc(prefixLen + jsonLen + suffixLen + 1);
	if (!html)
		return qfalse;
	memcpy(html, tmpl, prefixLen);
	memcpy(html + prefixLen, jsonData, jsonLen);
	memcpy(html + prefixLen + jsonLen, endPos, suffixLen + 1);
	*outHtml = html;
	return qtrue;
}

// main thread only. frees result->debugLogs unconditionally -- this is where that ownership ends.
static void TeamGen_WriteDebugReport(pugProposal_t *set, const teamGenResultPayload_t *result) {
	if (!result->debugLogs)
		return;

	if (!g_teamGenDebugTemplate) {
		free(result->debugLogs);
		return;
	}

	char *json = TeamGen_SerializeDebugReport(set, result);
	if (!json) {
		free(result->debugLogs);
		return;
	}
	char *html = NULL;
	if (TeamGen_SpliceDebugTemplate(g_teamGenDebugTemplate, json, &html)) {
		char whenStr[128] = { 0 };
		time_t now = time(NULL);
		struct tm *timeParts = localtime(&now);
		strftime(whenStr, sizeof(whenStr), "%Y%m%d-%H%M%S", timeParts);

		char filename[128];
		Com_sprintf(filename, sizeof(filename), "teamgen-%s.html", whenStr);

		// backstop against filling the disk with reports. not a cvar: size is already bounded by
		// TEAMGEN_DEBUGLOG_MAX_MILESTONES, and a report this big is unopenable anyway.
		size_t htmlLen = strlen(html);
		if (htmlLen > (size_t)TEAMGEN_DEBUGLOG_MAX_FILE_SIZE_KB * 1024) {
			Com_Printf("Team generator debug report skipped: %u KB exceeds the %d KB cap.\n",
				(unsigned int)(htmlLen / 1024), TEAMGEN_DEBUGLOG_MAX_FILE_SIZE_KB);
		}
		else {
			fileHandle_t f;
			trap_FS_FOpenFile(filename, &f, FS_WRITE);
			if (f) {
				trap_FS_Write(html, htmlLen, f);
				trap_FS_FCloseFile(f);
				Com_Printf("Team generator debug report written to %s (%u KB)\n", filename, (unsigned int)(htmlLen / 1024));
			}
		}
		free(html);
	}
	free(json);
	free(result->debugLogs);
}

static void TeamGenAsync_ApplyInitialGenerationResult(pugProposal_t *set, const teamGenResultPayload_t *result, qboolean forcedByServer) {
	if (result->succeeded) {
		memcpy(&set->suggested, &result->suggested, sizeof(permutationOfTeams_t));
		memcpy(&set->highestCaliber, &result->highestCaliber, sizeof(permutationOfTeams_t));
		memcpy(&set->fairest, &result->fairest, sizeof(permutationOfTeams_t));
		memcpy(&set->desired, &result->desired, sizeof(permutationOfTeams_t));
		memcpy(&set->inclusive, &result->inclusive, sizeof(permutationOfTeams_t));
		memcpy(&set->semiDesired, &result->semiDesired, sizeof(permutationOfTeams_t));
		memcpy(&set->firstChoice, &result->firstChoice, sizeof(permutationOfTeams_t));
		set->numValidPermutationsChecked = result->numValidPermutationsChecked;
		TeamGen_AddWinningHashesToAvoidedList(set, result);

		TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d %s (%s). Check console for teams proposals.", set->num, forcedByServer ? "force passed by server" : "passed", set->namesStr));
		set->passed = qtrue;
		set->state = PUGPROPOSAL_STATE_READY_FOR_TEAM_VOTE;
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
		TeamGen_WriteDebugReport(set, result);

		if (level.autoStartPending) {
			Com_Printf("The currently pending auto start was cancelled due to team generator changing the teams.\n");
			level.autoStartPending = qfalse;
		}

		// kill any existing map vote if we decide to do a different pug proposal
		if (level.voteTime && (!Q_stricmp(level.voteString, "mapvote") || level.multiVoting)) {
			PrintIngame(-1, "Map vote automatically stopped due to pug proposal passing.\n");
			level.voteAutoPassOnExpiration = qfalse;
			level.multiVoting = qfalse;
			level.voteBanPhase = qfalse;
			level.voteBanPhaseCompleted = qfalse;
			level.runoffSurvivors = level.runoffLosers = level.successfulRerollVoters = 0llu;
			level.inRunoff = qfalse;
			level.multiVoteChoices = 0;
			level.multiVoteHasWildcard = qfalse;
			level.multivoteWildcardMapFileName[0] = '\0';
			level.mapsThatCanBeVotedBits = 0;
			level.multiVoteTimeExtensions = 0;
			memset(level.multiVotes, 0, sizeof(level.multiVotes));
			memset(&(level.multiVoteBanVotes), 0, sizeof(level.multiVoteBanVotes));
			memset(level.bannedMapNames, 0, sizeof(level.bannedMapNames));
			memset(&level.multiVoteMapChars, 0, sizeof(level.multiVoteMapChars));
			memset(&level.multiVoteMapShortNames, 0, sizeof(level.multiVoteMapShortNames));
			memset(&level.multiVoteMapFileNames, 0, sizeof(level.multiVoteMapFileNames));
			level.voteStartTime = 0;
			level.voteTime = 0;
			trap_SetConfigstring(CS_VOTE_TIME, "");
		}
	}
	else {
		TeamGen_WriteDebugReport(set, result);
		TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d %s (%s). However, unable to generate teams; pug proposal %d terminated.", set->num, forcedByServer ? "force passed by server" : "passed", set->namesStr, set->num));
		level.activePugProposal = NULL;
		TeamGenerator_ResetConfirmationsAndMutes();

		ListClear(&set->avoidedHashesList);
		ListRemove(&level.pugProposalsList, set);
	}
}

// completion handler for a REROLL job. the old "try several seeds, keep whichever changed the most"
// loop is gone -- the deterministic seed scheme gives exactly one seed per reroll attempt.
static void TeamGenAsync_ApplyRerollResult(pugProposal_t *set, const teamGenResultPayload_t *result, qboolean forcedByServer, const pugProposal_t *oldHashes, int numOld) {
	if (result->succeeded) {
		unsigned int gotNewTeams = 0u;
		if (result->suggested.valid && oldHashes->suggested.valid && oldHashes->suggested.hash != result->suggested.hash) ++gotNewTeams;
		if (result->highestCaliber.valid && oldHashes->highestCaliber.valid && oldHashes->highestCaliber.hash != result->highestCaliber.hash) ++gotNewTeams;
		if (result->fairest.valid && oldHashes->fairest.valid && oldHashes->fairest.hash != result->fairest.hash) ++gotNewTeams;
		if (result->desired.valid && oldHashes->desired.valid && oldHashes->desired.hash != result->desired.hash) ++gotNewTeams;
		if (result->inclusive.valid && oldHashes->inclusive.valid && oldHashes->inclusive.hash != result->inclusive.hash) ++gotNewTeams;
		if (result->semiDesired.valid && oldHashes->semiDesired.valid && oldHashes->semiDesired.hash != result->semiDesired.hash) ++gotNewTeams;
		if (result->firstChoice.valid && oldHashes->firstChoice.valid && oldHashes->firstChoice.hash != result->firstChoice.hash) ++gotNewTeams;

		if (gotNewTeams > 0) {
			memcpy(&set->suggested, &result->suggested, sizeof(permutationOfTeams_t));
			memcpy(&set->highestCaliber, &result->highestCaliber, sizeof(permutationOfTeams_t));
			memcpy(&set->fairest, &result->fairest, sizeof(permutationOfTeams_t));
			memcpy(&set->desired, &result->desired, sizeof(permutationOfTeams_t));
			memcpy(&set->inclusive, &result->inclusive, sizeof(permutationOfTeams_t));
			memcpy(&set->semiDesired, &result->semiDesired, sizeof(permutationOfTeams_t));
			memcpy(&set->firstChoice, &result->firstChoice, sizeof(permutationOfTeams_t));
			set->numValidPermutationsChecked = result->numValidPermutationsChecked;
			TeamGen_AddWinningHashesToAvoidedList(set, result);

			set->suggestedVoteClientsRed = set->suggestedVoteClientsBlue =
				set->highestCaliberVoteClientsRed = set->highestCaliberVoteClientsBlue =
				set->fairestVoteClientsRed = set->fairestVoteClientsBlue =
				set->desiredVoteClientsRed = set->desiredVoteClientsBlue =
				set->semiDesiredVoteClientsRed = set->semiDesiredVoteClientsBlue =
				set->firstChoiceVoteClientsRed = set->firstChoiceVoteClientsBlue = 0;

			// prevent situations where rerolling could erroneously add an ?e option due to keeping the old ?d option and making a new ?d option that coexist together
			if (set->inclusive.valid && !oldHashes->inclusive.valid) {
				set->desired.valid = qfalse;
				set->semiDesired.valid = qfalse;
				set->firstChoice.valid = qfalse;
			}
			else if (set->desired.valid && !oldHashes->desired.valid) {
				set->inclusive.valid = qfalse;
				set->semiDesired.valid = qfalse;
				set->firstChoice.valid = qfalse;
			}
			else if (set->semiDesired.valid && !oldHashes->semiDesired.valid) {
				set->inclusive.valid = qfalse;
				set->desired.valid = qfalse;
				set->firstChoice.valid = qfalse;
			}
			else if (set->firstChoice.valid && !oldHashes->firstChoice.valid) {
				set->inclusive.valid = qfalse;
				set->desired.valid = qfalse;
				set->semiDesired.valid = qfalse;
			}

			TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d rerolled%s (%s). Check console for new teams proposals.", set->num, forcedByServer ? " by server" : "", set->namesStr));
			set->state = PUGPROPOSAL_STATE_READY_FOR_TEAM_VOTE;
			PrintTeamsProposalsInConsole(set, -1);
			TeamGen_WriteDebugReport(set, result);
			return;
		}
	}

	TeamGen_WriteDebugReport(set, result);
	TeamGenerator_QueueServerMessageInChat(-1, va("Failed to generate different teams when rerolling pug proposal %d%s (%s). Voting will continue for the existing teams.", set->num, forcedByServer ? " by server" : "", set->namesStr));
	set->state = PUGPROPOSAL_STATE_READY_FOR_TEAM_VOTE;
}

// tracks the general pickable pool and, once it's been stable for a debounce window, computes what
// ?start would produce right now, so ActivatePugProposal may be able to apply it instantly
static void TeamGen_CheckSpeculativeRosterPrecompute(void) {
	if (g_gametype.integer != GT_CTF)
		return;

	// deliberately no timer-based "just in case" ratings refresh here. it used to fire ~1s into every
	// match as a synchronous DB query -- a guaranteed hitch for no benefit.

	if (!g_vote_teamgen_speculativeRosterPrecomputeEnabled.integer)
		return;

	sortedClient_t pickable[MAX_CLIENTS];
	int numEligible = 0, numIngame = 0;
	// no need to refresh ratings every frame just to track the pool's identity
	GetCurrentPickablePlayers(pickable, &numEligible, &numIngame, qfalse, qfalse);

	if (numEligible < 8) {
		g_lastPickableRosterHash = 0;
		g_pickableRosterSpeculativeDispatched = qfalse;
		g_pickableRosterEscalationLevel = 0;
		g_speculativeRosterResult.valid = qfalse;
		return;
	}

	XXH32_hash_t currentHash = HashPugProposal(pickable);
	if (currentHash != g_lastPickableRosterHash) {
		// pool changed -- any previous speculative result is for a different roster and must not be used
		if (g_speculativeRosterResult.valid)
			TeamGen_AnnounceThreadDebug("Background speculative roster precompute invalidated: pickable pool changed.");
		g_lastPickableRosterHash = currentHash;
		g_pickableRosterStableSinceMs = level.time;
		g_pickableRosterSpeculativeDispatched = qfalse;
		g_speculativeRosterResult.valid = qfalse;

		// if this exact roster has unshown progress cached (a disconnect/reconnect blip just brought it
		// back), resume from there. nothing's been shown yet, so there's no fairness concern.
		speculativeRosterCacheEntry_t *cached = TeamGen_FindSpeculativeRosterCacheEntry(currentHash);
		if (cached && !cached->locked) {
			g_pickableRosterEscalationLevel = cached->iterationLevel;
			g_speculativeRosterResult.valid = qtrue;
			g_speculativeRosterResult.rosterHash = currentHash;
			g_speculativeRosterResult.iterationLevel = cached->iterationLevel;
			g_speculativeRosterResult.suggested = cached->suggested;
			g_speculativeRosterResult.highestCaliber = cached->highestCaliber;
			g_speculativeRosterResult.fairest = cached->fairest;
			g_speculativeRosterResult.desired = cached->desired;
			g_speculativeRosterResult.inclusive = cached->inclusive;
			g_speculativeRosterResult.semiDesired = cached->semiDesired;
			g_speculativeRosterResult.firstChoice = cached->firstChoice;
			g_speculativeRosterResult.numValidPermutationsChecked = cached->numValidPermutationsChecked;
		}
		else {
			g_pickableRosterEscalationLevel = 0; // genuinely new roster, or frozen (nothing left to compute)
		}
		return;
	}

	if (g_pickableRosterSpeculativeDispatched)
		return; // a job for the current escalation level is still in flight

	speculativeRosterCacheEntry_t *cached = TeamGen_FindSpeculativeRosterCacheEntry(g_lastPickableRosterHash);
	if (cached && cached->locked)
		return; // frozen -- already shown once; never compute further for this exact roster

	if (numEligible == 8 && g_pickableRosterEscalationLevel >= 1)
		return; // at exactly 8 eligible players, every valid permutation trivially
			// at exactly 8 the pool includes everyone, so the iteration-selection metrics are identical across
			// every iteration and the first always wins. escalating past level 1 would be pure waste, unlike
			// 9+ where the pool exceeds the 8 slots and inclusion is a real question.

	if (cached && cached->maxLevel > 0 && g_pickableRosterEscalationLevel >= cached->maxLevel)
		return; // reached this roster's dynamic ceiling (baseIterations x maxLevel <= MAX_TEAMGEN_ITERATIONS) without ever exceeding it

	if (g_pickableRosterEscalationLevel == 0) {
		// only the first dispatch for a roster waits out the debounce -- each next level fires as soon as
		// the previous one completes
		if (level.time - g_pickableRosterStableSinceMs < SPECULATIVE_ROSTER_DEBOUNCE_MS)
			return; // pool hasn't been stable long enough yet
	}

	int nextLevel = g_pickableRosterEscalationLevel + 1;
	pugProposal_t fakeSet = { 0 };
	memcpy(fakeSet.clients, pickable, sizeof(fakeSet.clients));
	if (TeamGen_DispatchGenerationJob(&fakeSet, TEAMGENJOB_SPECULATIVE_ROSTER, qfalse, 0, nextLevel)) {
		g_pickableRosterSpeculativeDispatched = qtrue;
		g_pickableRosterEscalationLevel = nextLevel;
	}
}

// consumes a matching precompute result for `set`'s roster if one is ready. returns qtrue if it was
// consumed, in which case the caller must not also dispatch.
static qboolean TeamGen_TryConsumeSpeculativeRosterResult(pugProposal_t *set, qboolean forcedByServer) {
	XXH32_hash_t hash = TeamGen_EligibleRosterHashForProposal(set);
	speculativeRosterCacheEntry_t *cached = TeamGen_FindSpeculativeRosterCacheEntry(hash);

	const permutationOfTeams_t *suggested, *highestCaliber, *fairest, *desired, *inclusive, *semiDesired, *firstChoice;
	uint64_t numValidPermutationsChecked;
	int iterationLevel, maxLevel;

	if (cached && cached->locked) {
		// this exact roster was already shown once -- reapply the frozen result verbatim, never re-derive it
		TeamGen_AnnounceThreadDebug(va("Pug proposal #%d: reapplying frozen speculative result (originally computed at %dx iterations) for this exact roster.", set->num, cached->iterationLevel));
		suggested = &cached->suggested;
		highestCaliber = &cached->highestCaliber;
		fairest = &cached->fairest;
		desired = &cached->desired;
		inclusive = &cached->inclusive;
		semiDesired = &cached->semiDesired;
		firstChoice = &cached->firstChoice;
		numValidPermutationsChecked = cached->numValidPermutationsChecked;
		iterationLevel = cached->iterationLevel;
		maxLevel = cached->maxLevel;
	}
	else if (g_speculativeRosterResult.valid && g_speculativeRosterResult.rosterHash == hash) {
		TeamGen_AnnounceThreadDebug(va("Pug proposal #%d: applying ready background speculative roster precompute (%dx iterations) instantly.", set->num, g_speculativeRosterResult.iterationLevel));
		suggested = &g_speculativeRosterResult.suggested;
		highestCaliber = &g_speculativeRosterResult.highestCaliber;
		fairest = &g_speculativeRosterResult.fairest;
		desired = &g_speculativeRosterResult.desired;
		inclusive = &g_speculativeRosterResult.inclusive;
		semiDesired = &g_speculativeRosterResult.semiDesired;
		firstChoice = &g_speculativeRosterResult.firstChoice;
		numValidPermutationsChecked = g_speculativeRosterResult.numValidPermutationsChecked;
		iterationLevel = g_speculativeRosterResult.iterationLevel;
		maxLevel = g_speculativeRosterResult.maxLevel;
	}
	else {
		return qfalse;
	}

	teamGenResultPayload_t result;
	result.succeeded = qtrue;
	result.suggested = *suggested;
	result.highestCaliber = *highestCaliber;
	result.fairest = *fairest;
	result.desired = *desired;
	result.inclusive = *inclusive;
	result.semiDesired = *semiDesired;
	result.firstChoice = *firstChoice;
	result.numValidPermutationsChecked = numValidPermutationsChecked;
	// roster precompute never generates a report -- nothing to free
	result.debugLogs = NULL;
	result.kind = TEAMGENJOB_SPECULATIVE_ROSTER;
	result.numEligible = result.numIngame = 0;
	result.seedBase = 0;
	result.rerollNumUsed = 0;
	result.elapsedMs = 0;
	result.iterationsRun = 0;
	result.bestIterationIndex = -1;

	g_speculativeRosterResult.valid = qfalse;
	TeamGen_LockSpeculativeRosterCacheEntry(hash, iterationLevel, maxLevel, suggested, highestCaliber, fairest, desired, inclusive, semiDesired, firstChoice, numValidPermutationsChecked);

	TeamGenAsync_ApplyInitialGenerationResult(set, &result, forcedByServer);
	if (result.succeeded)
		TeamGen_DispatchRerollLookahead(set);
	return qtrue;
}

// counts reroll votes among connected members and returns how many quorum needs. shared by the
// ?reroll handler and the post-generation re-check so they can't disagree.
static int TeamGen_CountRerollVotes(int *numVotesOut) {
	int numEligibleConnected = 0, numEligibleMax = 0, numVotes = 0;
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
				++numVotes;

			break;
		}
	}

	// require a simple majority of the total people in the proposal. requiring only 4 or 5 would allow e.g. in a set of 15 people for just a few people who were excluded to reroll it for everyone else
	if (numEligibleConnected > numEligibleMax)
		numEligibleConnected = numEligibleMax; // sanity check

	if (numVotesOut)
		*numVotesOut = numVotes;
	return (numEligibleConnected / 2) + 1;
}

// a reroll vote that hit quorum mid-generation is deferred, and nothing else would make good on it:
// the handler's quorum test is gated behind doVote, which is false for anyone who already voted --
// so it would wait for some new player, and never happen if that vote was the last.
static void TeamGen_CheckDeferredReroll(void) {
	if (!level.activePugProposal || level.activePugProposal->state != PUGPROPOSAL_STATE_READY_FOR_TEAM_VOTE)
		return;
	if (!level.activePugProposal->votedToRerollClients)
		return; // nobody is waiting on one

	int numVotes = 0;
	const int numRequired = TeamGen_CountRerollVotes(&numVotes);
	if (numVotes >= numRequired)
		TeamGenerator_DoReroll(qfalse);
}

// once per frame from G_RunFrame. checks every job for completion, re-validates it's still relevant
// before applying, and frees the slot either way.
void TeamGenAsync_PollCompletedJobs(void) {
	int startMs = trap_Milliseconds();
	// only frames where something finished can have made a deferred reroll fireable, and the recount is
	// a MAX_CLIENTS^3 scan
	qboolean anyRealJobCompleted = qfalse;

	for (int i = 0; i < MAX_CONCURRENT_TEAMGEN_JOBS; i++) {
		teamGenJob_t *job = &g_teamGenJobs[i];
		if (!job->inUse)
			continue;
		if (!GAtomic_Get(&job->done))
			continue;
		if (job->kind == TEAMGENJOB_INITIAL || job->kind == TEAMGENJOB_REROLL)
			anyRealJobCompleted = qtrue;

		if (job->thread) {
			GThread_Join(job->thread);
			job->thread = NULL;
		}

		if (job->kind == TEAMGENJOB_SPECULATIVE_ROSTER) {
			// only relevant to the LIVE tracking if the pool hasn't changed since dispatch. the persistent cache
			// below is updated regardless -- it's keyed by the job's own dispatch-time hash, which is what lets
			// a roster that blips away and back resume.
			qboolean stillFresh = job->avoidedHashesTagAtDispatch == (unsigned int)g_lastPickableRosterHash;
			XXH32_hash_t jobHash = (XXH32_hash_t)job->avoidedHashesTagAtDispatch;

			speculativeRosterCacheEntry_t *cachedForJob = TeamGen_FindSpeculativeRosterCacheEntry(jobHash);
			if (job->succeeded && !(cachedForJob && cachedForJob->locked)) {
				int maxLevel = TeamGen_ComputeMaxEscalationLevel(job->baseIterations);
				TeamGen_UpdateSpeculativeRosterCache(jobHash, job->iterationMultiplier, maxLevel,
					&job->suggested, &job->highestCaliber, &job->fairest, &job->desired, &job->inclusive, &job->semiDesired, &job->firstChoice,
					job->numValidPermutationsChecked);
			}

			if (job->succeeded && stillFresh) {
				g_speculativeRosterResult.valid = qtrue;
				g_speculativeRosterResult.rosterHash = g_lastPickableRosterHash;
				g_speculativeRosterResult.iterationLevel = job->iterationMultiplier;
				g_speculativeRosterResult.suggested = job->suggested;
				g_speculativeRosterResult.highestCaliber = job->highestCaliber;
				g_speculativeRosterResult.fairest = job->fairest;
				g_speculativeRosterResult.desired = job->desired;
				g_speculativeRosterResult.inclusive = job->inclusive;
				g_speculativeRosterResult.semiDesired = job->semiDesired;
				g_speculativeRosterResult.firstChoice = job->firstChoice;
				g_speculativeRosterResult.numValidPermutationsChecked = job->numValidPermutationsChecked;
				speculativeRosterCacheEntry_t *cachedNow = TeamGen_FindSpeculativeRosterCacheEntry(jobHash);
				int maxLevelNow = cachedNow ? cachedNow->maxLevel : 0;
				g_speculativeRosterResult.maxLevel = maxLevelNow;
				TeamGen_AnnounceThreadDebug(va("Background speculative roster precompute (level %d/%d) finished in %dms (%d iteration%s) and is ready to apply instantly on the next ?start.",
					job->iterationMultiplier, maxLevelNow, job->elapsedMs, job->iterationsRun, job->iterationsRun == 1 ? "" : "s"));
			}
			else if (!stillFresh) {
				TeamGen_AnnounceThreadDebug(va("Background speculative roster precompute (level %d) finished, but the pickable pool changed in the meantime; discarding live copy (cache retained if still valid).", job->iterationMultiplier));
			}
			else {
				TeamGen_AnnounceThreadDebug(va("Background speculative roster precompute (level %d) finished in %dms: no valid team split found.", job->iterationMultiplier, job->elapsedMs));
			}
			TeamGen_FreeJobProposalClone(job->jobSet);
			job->jobSet = NULL;
			// always NULL today (logs are only built for INITIAL/REROLL), but freeing keeps this path correct
			free(job->debugLogs);
			job->debugLogs = NULL;
			job->inUse = qfalse;

			if (stillFresh) {
				g_pickableRosterSpeculativeDispatched = qfalse;
				g_pickableRosterEscalationLevel = job->iterationMultiplier;
			}
			continue;
		}

		// re-validate: does the target proposal still exist, unchanged? (never deref a stale pugProposal_t*)
		pugProposal_t *set = NULL;
		iterator_t iter;
		ListIterate(&level.pugProposalsList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			pugProposal_t *p = IteratorNext(&iter);
			if (p->num == job->proposalNum) {
				set = p;
				break;
			}
		}

		// re-derive the eligible roster and compare against what the job computed from. set->epoch is
		// bumped in only four places and misses several ways eligibility can change mid-flight (partial
		// disconnect, team change, spectator switch, a ?bar passing).
		qboolean rosterStillMatches = qfalse;
		if (set) {
			sortedClient_t nowClients[MAX_CLIENTS];
			int nowEligible = 0, nowIngame = 0;
			TeamGen_ComputeCurrentEligibleRoster(set->clients, nowClients, &nowEligible, &nowIngame);
			rosterStillMatches = (HashPugProposal(nowClients) == job->rosterAtDispatch) ? qtrue : qfalse;
		}

		if (set && set->epoch == job->epochAtDispatch && rosterStillMatches) {
			teamGenResultPayload_t result;
			result.succeeded = job->succeeded;
			result.suggested = job->suggested;
			result.highestCaliber = job->highestCaliber;
			result.fairest = job->fairest;
			result.desired = job->desired;
			result.inclusive = job->inclusive;
			result.semiDesired = job->semiDesired;
			result.firstChoice = job->firstChoice;
			result.numValidPermutationsChecked = job->numValidPermutationsChecked;
			// ownership of debugLogs passes to whichever Apply*Result call consumes it below
			result.debugLogs = job->debugLogs;
			result.kind = job->kind;
			result.numEligible = job->numEligible;
			result.numIngame = job->numIngame;
			result.seedBase = job->seedBase;
			result.rerollNumUsed = job->rerollNumUsed;
			result.elapsedMs = job->elapsedMs;
			result.iterationsRun = job->iterationsRun;
			result.bestIterationIndex = job->bestIterationIndex;

			if (job->kind == TEAMGENJOB_INITIAL || job->kind == TEAMGENJOB_REROLL) {
				if (result.succeeded) {
					TeamGen_AnnounceThreadDebug(va("%s generation for pug proposal #%d finished in %dms (best of %d iteration%s).",
						job->kind == TEAMGENJOB_REROLL ? "Reroll" : "Initial", set->num, job->elapsedMs,
						job->iterationsRun, job->iterationsRun == 1 ? "" : "s"));
				}
				else {
					TeamGen_AnnounceThreadDebug(va("%s generation for pug proposal #%d finished in %dms: no valid team split found.",
						job->kind == TEAMGENJOB_REROLL ? "Reroll" : "Initial", set->num, job->elapsedMs));
				}
			}

			if (job->kind == TEAMGENJOB_INITIAL) {
				TeamGenAsync_ApplyInitialGenerationResult(set, &result, job->forcedByServer);
				if (result.succeeded) {
					// this roster is genuinely shown now -- freeze it in the anti-exploit cache too, in case there was
					// no speculative result to consume
					int maxLevelForLock = TeamGen_ComputeMaxEscalationLevel(job->baseIterations);
					// rosterAtDispatch, not a fresh hash of set->clients: it's already the eligible-roster hash this job
					// generated from, and the check above just re-confirmed it
					TeamGen_LockSpeculativeRosterCacheEntry(job->rosterAtDispatch, job->iterationMultiplier, maxLevelForLock,
						&result.suggested, &result.highestCaliber, &result.fairest, &result.desired, &result.inclusive, &result.semiDesired, &result.firstChoice,
						result.numValidPermutationsChecked);
					TeamGen_DispatchRerollLookahead(set);
				}
			}
			else if (job->kind == TEAMGENJOB_REROLL) {
				// reroll: rebuild the "old hashes" snapshot the way TeamGenerator_DoReroll used to, then apply
				pugProposal_t oldHashes = { 0 };
				int numOld = 0;
				if (set->suggested.valid) { oldHashes.suggested.valid = qtrue; oldHashes.suggested.hash = set->suggested.hash; ++numOld; }
				if (set->highestCaliber.valid) { oldHashes.highestCaliber.valid = qtrue; oldHashes.highestCaliber.hash = set->highestCaliber.hash; ++numOld; }
				if (set->fairest.valid) { oldHashes.fairest.valid = qtrue; oldHashes.fairest.hash = set->fairest.hash; ++numOld; }
				if (set->desired.valid) { oldHashes.desired.valid = qtrue; oldHashes.desired.hash = set->desired.hash; ++numOld; }
				if (set->inclusive.valid) { oldHashes.inclusive.valid = qtrue; oldHashes.inclusive.hash = set->inclusive.hash; ++numOld; }
				if (set->semiDesired.valid) { oldHashes.semiDesired.valid = qtrue; oldHashes.semiDesired.hash = set->semiDesired.hash; ++numOld; }
				if (set->firstChoice.valid) { oldHashes.firstChoice.valid = qtrue; oldHashes.firstChoice.hash = set->firstChoice.hash; ++numOld; }
				TeamGenAsync_ApplyRerollResult(set, &result, job->forcedByServer, &oldHashes, numOld);
				if (result.succeeded) {
					// the rerolled result is the authoritative "last shown" state for this roster now
					TeamGen_UpdateSpeculativeRosterCacheAfterReroll(job->rosterAtDispatch, job->iterationMultiplier,
						&result.suggested, &result.highestCaliber, &result.fairest, &result.desired, &result.inclusive, &result.semiDesired, &result.firstChoice,
						result.numValidPermutationsChecked);
				}
				TeamGen_DispatchRerollLookahead(set);
			}
			else { // TEAMGENJOB_REROLL_LOOKAHEAD
				// never applied automatically -- just stash it for TeamGenerator_DoReroll to consume later
				set->rerollLookaheadDispatched = qfalse;
				if (job->rerollNumUsed != rerollNum + 1) {
					// stale: a faster reroll already consumed an earlier level and moved on to a new target. the
					// instant-consume path doesn't bump epoch, so the generic check above can't catch this.
				}
				else if (result.succeeded) {
					set->speculativeReroll.valid = qtrue;
					set->speculativeReroll.rerollNumUsed = job->rerollNumUsed;
					set->speculativeReroll.avoidedHashesTag = job->avoidedHashesTagAtDispatch;
					set->speculativeReroll.iterationLevel = job->iterationMultiplier;
					set->speculativeReroll.suggested = result.suggested;
					set->speculativeReroll.highestCaliber = result.highestCaliber;
					set->speculativeReroll.fairest = result.fairest;
					set->speculativeReroll.desired = result.desired;
					set->speculativeReroll.inclusive = result.inclusive;
					set->speculativeReroll.semiDesired = result.semiDesired;
					set->speculativeReroll.firstChoice = result.firstChoice;
					set->speculativeReroll.numValidPermutationsChecked = result.numValidPermutationsChecked;

					int maxLevel = TeamGen_ComputeMaxEscalationLevel(job->baseIterations);
					set->rerollLookaheadMaxLevel = maxLevel;
					TeamGen_AnnounceThreadDebug(va("Background reroll-lookahead precompute (level %d/%d) for pug proposal #%d finished in %dms (%d iteration%s) and is ready to apply instantly on the next ?reroll.",
						job->iterationMultiplier, maxLevel, set->num, job->elapsedMs, job->iterationsRun, job->iterationsRun == 1 ? "" : "s"));

					// escalate further, same cap as the roster precompute -- except at exactly 8 players, where it's
					// provably wasted (see TeamGen_CheckSpeculativeRosterPrecompute)
					if (job->numEligible != 8 && job->iterationMultiplier < maxLevel) {
						int nextLevel = job->iterationMultiplier + 1;
						if (TeamGen_DispatchGenerationJob(set, TEAMGENJOB_REROLL_LOOKAHEAD, qfalse, job->rerollNumUsed, nextLevel)) {
							set->rerollLookaheadDispatched = qtrue;
							set->rerollLookaheadEscalationLevel = nextLevel;
						}
					}
				}
				else {
					TeamGen_AnnounceThreadDebug(va("Background reroll-lookahead precompute (level %d) for pug proposal #%d finished in %dms: no valid team split found.", job->iterationMultiplier, set->num, job->elapsedMs));
				}
			}
		}
		else {
			// proposal was canceled/superseded while this ran -- silently discard (existing messaging already
			// told players). nobody will consume the logs, so free them. release the lookahead latch even here:
			// if the proposal is still alive, leaving it set would disable reroll-lookahead for it permanently.
			if (job->kind == TEAMGENJOB_REROLL_LOOKAHEAD && set)
				set->rerollLookaheadDispatched = qfalse;
			if (job->kind == TEAMGENJOB_INITIAL || job->kind == TEAMGENJOB_REROLL) {
				TeamGen_AnnounceThreadDebug(va("Discarding %s generation result for pug proposal #%d: eligible roster changed since it was dispatched.",
					job->kind == TEAMGENJOB_REROLL ? "reroll" : "initial", job->proposalNum));
			}
			free(job->debugLogs);
			job->debugLogs = NULL;
		}

		TeamGen_FreeJobProposalClone(job->jobSet);
		job->jobSet = NULL;
		job->inUse = qfalse;
	}

	// if nothing is generating anymore, activate any proposal that reached quorum while a different one
	// was in flight
	if (!TeamGen_RealJobInFlight()) {
		iterator_t iter;
		ListIterate(&level.pugProposalsList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			pugProposal_t *p = IteratorNext(&iter);
			// PENDING_VOTES only: one that already generated must never be activated a second time
			if (p->queuedActivation && p->state == PUGPROPOSAL_STATE_PENDING_VOTES) {
				p->queuedActivation = qfalse;
				ActivatePugProposal(p, qfalse);
				break; // ActivatePugProposal may mutate level.pugProposalsList; stop iterating
			}
		}
	}

	// make good on a reroll vote that was deferred while a generation was in flight
	if (anyRealJobCompleted)
		TeamGen_CheckDeferredReroll();

	TeamGen_CheckSpeculativeRosterPrecompute();

	// this all runs on the main thread every frame and should be near-instant, since the search itself
	// is offloaded. log it if it's ever not, so a regression shows up here rather than as a hitch.
	int elapsedMs = trap_Milliseconds() - startMs;
	if (elapsedMs >= 5) {
		Com_Printf("^3[teamgen timing]^7 TeamGenAsync_PollCompletedJobs took %dms on the main thread this frame.\n", elapsedMs);
	}

}

// from G_ShutdownGame before teardown. blocks until every coordinator thread has finished -- any
// still running would be touching heap that's about to become invalid.
void TeamGenAsync_Shutdown(void) {
	// this also clears the speculative statics and roster cache, which is the per-match reset that has
	// to happen before the next G_InitGame
	TeamGen_AbortAndReapAllJobs();

	// the dispatch gates are separate latches; leaving either set would refuse all dispatch after the
	// next map loads. statics survive a map_restart, so this is a real reset.
	g_teamGenRestartCountdownSuppressed = qfalse;
	g_teamGenMapChangePending = qfalse;

	// the engine dlclose()s us on a map change, which drops the static pointer but not the heap block
	// -- without this the template leaks once per map change
	free(g_teamGenDebugTemplate);
	g_teamGenDebugTemplate = NULL;
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

	rerollNum = 0;

	// if a precompute for this exact roster is already ready, apply it instantly
	if (TeamGen_TryConsumeSpeculativeRosterResult(set, forcedByServer))
		return;

	// otherwise kick off generation -- dispatches and returns immediately; PollCompletedJobs applies
	// the result once it lands
	if (!TeamGen_DispatchGenerationJob(set, TEAMGENJOB_INITIAL, forcedByServer, rerollNum, 1)) {
		// dispatch itself failed -- tear the proposal down the same way a failed generation always has
		level.activePugProposal = NULL;
		TeamGenerator_ResetConfirmationsAndMutes();
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
	level.voteBanPhase = qfalse;
	level.voteBanPhaseCompleted = qfalse;
	level.runoffSurvivors = level.runoffLosers = level.successfulRerollVoters = 0llu;
	level.inRunoff = qfalse;
	level.mapsThatCanBeVotedBits = 0;
	level.inRunoff = qfalse;
	level.multiVoteChoices = 0;
	level.multiVoteTimeExtensions = 0;
	level.multiVoteHasWildcard = qfalse;
	level.multivoteWildcardMapFileName[0] = '\0';
	memset(level.multiVotes, 0, sizeof(level.multiVotes));
	memset(&(level.multiVoteBanVotes), 0, sizeof(level.multiVoteBanVotes));
	memset(level.bannedMapNames, 0, sizeof(level.bannedMapNames));
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

	TeamGenerator_ResetConfirmationsAndMutes();

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

			if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL)
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

// bundles the per-type state (result pointer, red/blue vote counts, chat letter) that voting code
// needs. several functions below independently repeated this exact switch.
typedef struct {
	permutationOfTeams_t *permutation;
	int *votesRed;
	int *votesBlue;
	char letter;
} teamGenVoteRefs_t;

static teamGenVoteRefs_t TeamGen_VoteRefsForType(pugProposal_t *set, int type) {
	teamGenVoteRefs_t refs;
	switch (type) {
	case TEAMGENERATORTYPE_MOSTPLAYED:
		refs.permutation = &set->suggested;
		refs.votesRed = &set->suggestedVoteClientsRed;
		refs.votesBlue = &set->suggestedVoteClientsBlue;
		refs.letter = set->suggestedLetter;
		break;
	case TEAMGENERATORTYPE_HIGHESTRATING:
		refs.permutation = &set->highestCaliber;
		refs.votesRed = &set->highestCaliberVoteClientsRed;
		refs.votesBlue = &set->highestCaliberVoteClientsBlue;
		refs.letter = set->highestCaliberLetter;
		break;
	case TEAMGENERATORTYPE_FAIREST:
		refs.permutation = &set->fairest;
		refs.votesRed = &set->fairestVoteClientsRed;
		refs.votesBlue = &set->fairestVoteClientsBlue;
		refs.letter = set->fairestLetter;
		break;
	case TEAMGENERATORTYPE_INCLUSIVE:
		refs.permutation = &set->inclusive;
		refs.votesRed = &set->inclusiveVoteClientsRed;
		refs.votesBlue = &set->inclusiveVoteClientsBlue;
		refs.letter = set->inclusiveLetter;
		break;
	case TEAMGENERATORTYPE_DESIREDPOS:
		refs.permutation = &set->desired;
		refs.votesRed = &set->desiredVoteClientsRed;
		refs.votesBlue = &set->desiredVoteClientsBlue;
		refs.letter = set->desiredLetter;
		break;
	case TEAMGENERATORTYPE_SEMIDESIREDPOS:
		refs.permutation = &set->semiDesired;
		refs.votesRed = &set->semiDesiredVoteClientsRed;
		refs.votesBlue = &set->semiDesiredVoteClientsBlue;
		refs.letter = set->semiDesiredLetter;
		break;
	case TEAMGENERATORTYPE_FIRSTCHOICE:
		refs.permutation = &set->firstChoice;
		refs.votesRed = &set->firstChoiceVoteClientsRed;
		refs.votesBlue = &set->firstChoiceVoteClientsBlue;
		refs.letter = set->firstChoiceLetter;
		break;
	default:
		assert(qfalse);
		memset(&refs, 0, sizeof(refs));
		break;
	}
	return refs;
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

	if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote on this client.");
		return qtrue;
	}

	if (!level.activePugProposal) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "No pug proposal is currently active.");
		return qtrue;
	}

	if (level.activePugProposal->state == PUGPROPOSAL_STATE_GENERATING) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Teams are still being calculated, please wait a moment.");
		return qtrue;
	}

	if (level.teamPermutationsShownTime && trap_Milliseconds() - level.teamPermutationsShownTime < 500) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Please read the teams proposals before voting.");
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ULTRAINSTAVOTETROLL) && !ent->client->pers.confirmedReading.didConfirm) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va(
			level.activePugProposal->confirmation[ent - g_entities].pos == CTFPOSITION_OFFENSE ?
			"As a troll, you must confirm that you have read the teams proposals by naming one ^3%s^6 player from each team in permutation ^3%c^6 before voting. Use the format:   %cconfirm name1 name2" :
			"As a troll, you must confirm that you have read the teams proposals by naming the ^3%s^6 players in permutation ^3%c^6 before voting. Use the format:   %cconfirm name1 name2",
			NameForPos(level.activePugProposal->confirmation[ent - g_entities].pos),
			level.activePugProposal->confirmation[ent - g_entities].letter,
			TEAMGEN_CHAT_COMMAND_CHARACTER));
		return qtrue;
	}

#ifndef _DEBUG
	if (g_vote_teamgen_readBeforeVotingMilliseconds.integer > 0) {
		if (level.teamPermutationsShownTime && trap_Milliseconds() - level.teamPermutationsShownTime < g_vote_teamgen_readBeforeVotingMilliseconds.integer) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Please take the time to read the teams proposals before voting.");
			ent->client->pers.triedToInstaVote = trap_Milliseconds(); // evil (owned)
			return qtrue;
		}
	}

	if (g_vote_teamgen_readBeforeVotingMillisecondsJawa.integer > 0 && ent->client->account->flags & ACCOUNTFLAG_INSTAVOTETROLL) {
		if (level.teamPermutationsShownTime && trap_Milliseconds() - level.teamPermutationsShownTime < g_vote_teamgen_readBeforeVotingMillisecondsJawa.integer) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Please take the time to read the teams proposals before voting.");
			ent->client->pers.triedToInstaVote = trap_Milliseconds(); // evil (owned)
			return qtrue;
		}
	}

	if (g_vote_teamgen_readBeforeVotingMilliseconds.integer > 0 && ent->client->pers.triedToInstaVote) {
		if (level.teamPermutationsShownTime && trap_Milliseconds() - ent->client->pers.triedToInstaVote < g_vote_teamgen_readBeforeVotingMilliseconds.integer) {
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Please take the time to read the teams proposals before voting.");
			ent->client->pers.triedToInstaVote = trap_Milliseconds(); // evil (owned)
			return qtrue;
		}
	}
#endif

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
			teamGenVoteRefs_t refs = TeamGen_VoteRefsForType(level.activePugProposal, i);
			char letter = refs.letter;
			int *votesIntRed = refs.votesRed;
			int *votesIntBlue = refs.votesBlue;
			permutationOfTeams_t *permutation = refs.permutation;

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
				if (permutation->teams[0].relativeStrength >= 0.519f - 0.0001f || permutation->teams[1].relativeStrength >= 0.519f - 0.0001f) {
					numRequired = 7;
				}
				else if (permutation->lowestPlayerRating >= PlayerTierToRating(PLAYERRATING_MID_B) - 0.0001) {
					if (permutation->teams[0].relativeStrength >= 0.5049f - 0.0001f || permutation->teams[1].relativeStrength >= 0.5049f - 0.0001f)
						numRequired = 5; // hc but imba
					else
						numRequired = 4; // just hc
				}
				else if (permutation->iDiff > 0 || numPlayers > 8) {
					numRequired = 6;
				}
				else {
					numRequired = 5;
				}
			}
			else {
				numRequired = 5;
			}

			Q_strcat(buf, sizeof(buf), va(" ^%c(%c: %d/%d)", color, letter, numYesVotesRed + numYesVotesBlue, numRequired));
		}
		*newMessage = buf;
	}

	// determine which have enough to pass
	int numPermutationsWithEnoughVotesToPass = 0;
	int numBalancedPassing = 0;
	int passingMask = 0;
	int balancedMask = 0;

	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		teamGenVoteRefs_t refs = TeamGen_VoteRefsForType(level.activePugProposal, i);
		int *votesIntRed = refs.votesRed;
		int *votesIntBlue = refs.votesBlue;
		permutationOfTeams_t *permutation = refs.permutation;

		if (!permutation->valid)
			continue;

		int numYesVotesRed = 0;
		int numYesVotesBlue = 0;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			if (*votesIntRed & (1 << j))
				++numYesVotesRed;
			else if (*votesIntBlue & (1 << j))
				++numYesVotesBlue;
		}

		int numRequired;
		if (g_vote_teamgen_dynamicVoteRequirement.integer) {
			if (permutation->teams[0].relativeStrength >= 0.519f - 0.0001f ||
				permutation->teams[1].relativeStrength >= 0.519f - 0.0001f) {
				numRequired = 7;
			}
			else if (permutation->lowestPlayerRating >= PlayerTierToRating(PLAYERRATING_MID_B) - 0.0001) {
				if (permutation->teams[0].relativeStrength >= 0.5049f - 0.0001f ||
					permutation->teams[1].relativeStrength >= 0.5049f - 0.0001f)
					numRequired = 5; /* hc but imba */
				else
					numRequired = 4; /* just hc      */
			}
			else if (permutation->iDiff > 0 || numPlayers > 8) {
				numRequired = 6;
			}
			else {
				numRequired = 5;
			}
		}
		else {
			numRequired = 5;
		}

		qboolean thisOnePasses = qfalse;
		if (g_vote_teamgen_require2VotesOnEachTeam.integer &&
			!(permutation->lowestPlayerRating >= PlayerTierToRating(PLAYERRATING_MID_B) - 0.0001 &&
				!(permutation->teams[0].relativeStrength >= 0.5049f - 0.0001f ||
					permutation->teams[1].relativeStrength >= 0.5049f - 0.0001f))) {
			if (numYesVotesRed + numYesVotesBlue >= numRequired &&
				numYesVotesRed >= 2 && numYesVotesBlue >= 2)
				thisOnePasses = qtrue;
		}
		else {
			if (numYesVotesRed + numYesVotesBlue >= numRequired)
				thisOnePasses = qtrue;
		}

		if (thisOnePasses) {
			passingMask |= (1 << i);
			++numPermutationsWithEnoughVotesToPass;
			if (permutation->iDiff == 0) {
				balancedMask |= (1 << i);
				++numBalancedPassing;
			}
		}
	}

	if (!numPermutationsWithEnoughVotesToPass)
		return qfalse; // none have enough votes to pass

	// now we pick a winner among those which have enough to pass
	int tiebreakerOrder[] = { TEAMGENERATORTYPE_MOSTPLAYED, TEAMGENERATORTYPE_HIGHESTRATING, TEAMGENERATORTYPE_FAIREST,
							  TEAMGENERATORTYPE_INCLUSIVE, TEAMGENERATORTYPE_DESIREDPOS, TEAMGENERATORTYPE_SEMIDESIREDPOS,
							  TEAMGENERATORTYPE_FIRSTCHOICE };
	srand(teamGenSeed);
	FisherYatesShuffle(&tiebreakerOrder[0], NUM_TEAMGENERATORTYPES, sizeof(int));
	srand(time(NULL));

	permutationOfTeams_t *winnerPerm = NULL;
	char winnerLetter = 0;
	int *winnerVotesRed = NULL;
	int *winnerVotesBlue = NULL;

	// of those which have enough votes to pass, prefer the 50-50 ones
	if (numPermutationsWithEnoughVotesToPass == 1) {
		for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
			if (passingMask & (1 << i)) {
				teamGenVoteRefs_t refs = TeamGen_VoteRefsForType(level.activePugProposal, i);
				winnerPerm = refs.permutation;
				winnerVotesRed = refs.votesRed;
				winnerVotesBlue = refs.votesBlue;
				winnerLetter = refs.letter;
				break;
			}
		}
	}
	else {
		int preferMask = (numBalancedPassing > 0) ? balancedMask : passingMask;
		for (int k = 0; k < NUM_TEAMGENERATORTYPES; k++) {
			int idx = tiebreakerOrder[k];
			if (preferMask & (1 << idx)) {
				teamGenVoteRefs_t refs = TeamGen_VoteRefsForType(level.activePugProposal, idx);
				winnerPerm = refs.permutation;
				winnerVotesRed = refs.votesRed;
				winnerVotesBlue = refs.votesBlue;
				winnerLetter = refs.letter;
				break;
			}
		}
	}

	// announce result and activate
	if (!winnerPerm || !winnerPerm->valid)
		return qfalse; // shouldn't happen

	const char *passMsg;
	if (numPermutationsWithEnoughVotesToPass == 1) {
		passMsg = "passed.";
	}
	else {
		if (!numBalancedPassing) // no 50-50
			passMsg = "passed by random tiebreaker.";
		else if (numBalancedPassing == 1) // only one of them was 50-50
			passMsg = "passed by balance tiebreaker.";
		else // multiple 50-50
			passMsg = "passed by random tiebreaker.";
	}

	TeamGenerator_QueueServerMessageInChat(-1, va("Teams proposal %c %s", winnerLetter, passMsg));

	char printMessage[1024] = { 0 };
	Com_sprintf(printMessage, sizeof(printMessage), "*^1Red team:^7 (%0.2f'/. relative strength)\n",
		winnerPerm->teams[0].relativeStrength * 100.0);
	Q_strcat(printMessage, sizeof(printMessage), va("^5Base: ^7 %s\n", winnerPerm->teams[0].baseName));
	Q_strcat(printMessage, sizeof(printMessage), va("^6Chase: ^7 %s\n", winnerPerm->teams[0].chaseName));
	Q_strcat(printMessage, sizeof(printMessage), va("^2Offense: ^7 %s^7, ", winnerPerm->teams[0].offense1Name));
	Q_strcat(printMessage, sizeof(printMessage), va("%s\n\n", winnerPerm->teams[0].offense2Name));
	Q_strcat(printMessage, sizeof(printMessage), va("^4Blue team:^7 (%0.2f'/. relative strength)\n",
		winnerPerm->teams[1].relativeStrength * 100.0));
	Q_strcat(printMessage, sizeof(printMessage), va("^5Base: ^7 %s\n", winnerPerm->teams[1].baseName));
	Q_strcat(printMessage, sizeof(printMessage), va("^6Chase: ^7 %s\n", winnerPerm->teams[1].chaseName));
	Q_strcat(printMessage, sizeof(printMessage), va("^2Offense: ^7 %s^7, ", winnerPerm->teams[1].offense1Name));
	Q_strcat(printMessage, sizeof(printMessage), va("^7%s\n\n", winnerPerm->teams[1].offense2Name));
	TeamGenerator_QueueServerMessageInConsole(-1, printMessage);

	ActivateTeamsProposal(winnerPerm);
	ListClear(&level.activePugProposal->avoidedHashesList);
	ListRemove(&level.pugProposalsList, level.activePugProposal);
	level.activePugProposal = NULL;
	TeamGenerator_ResetConfirmationsAndMutes();

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

	if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote on this client.");
		return qtrue;
	}

	if (!level.activePugProposal) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "No pug proposal is currently active.");
		return qtrue;
	}

	if (level.activePugProposal->state == PUGPROPOSAL_STATE_GENERATING) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Teams are still being calculated, please wait a moment.");
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

	if ((ent->client->account->flags & ACCOUNTFLAG_ULTRAINSTAVOTETROLL) && !ent->client->pers.confirmedReading.didConfirm) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, va(
			level.activePugProposal->confirmation[ent - g_entities].pos == CTFPOSITION_OFFENSE ?
			"As a troll, you must confirm that you have read the teams proposals by naming one ^3%s^6 player from each team in permutation ^3%c^6 before voting. Use the format:   %cconfirm name1 name2" :
			"As a troll, you must confirm that you have read the teams proposals by naming the ^3%s^6 players in permutation ^3%c^6 before voting. Use the format:   %cconfirm name1 name2",
			NameForPos(level.activePugProposal->confirmation[ent - g_entities].pos),
			level.activePugProposal->confirmation[ent - g_entities].letter,
			TEAMGEN_CHAT_COMMAND_CHARACTER));
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
			teamGenVoteRefs_t refs = TeamGen_VoteRefsForType(level.activePugProposal, i);
			char letter = refs.letter;
			int *votesIntRed = refs.votesRed;
			int *votesIntBlue = refs.votesBlue;
			permutationOfTeams_t *permutation = refs.permutation;

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
				if (permutation->teams[0].relativeStrength >= 0.519f - 0.0001f || permutation->teams[1].relativeStrength >= 0.519f - 0.0001f) {
					numRequired = 7;
				}
				else if (permutation->lowestPlayerRating >= PlayerTierToRating(PLAYERRATING_MID_B) - 0.0001) {
					if (permutation->teams[0].relativeStrength >= 0.5049f - 0.0001f || permutation->teams[1].relativeStrength >= 0.5049f - 0.0001f)
						numRequired = 5; // hc but imba
					else
						numRequired = 4; // just hc
				}
				else if (permutation->iDiff > 0 || numPlayers > 8) {
					numRequired = 6;
				}
				else {
					numRequired = 5;
				}
			}
			else {
				numRequired = 5;
			}
			Q_strcat(buf, sizeof(buf), va(" ^%c(%c: %d/%d)", color, letter, numYesVotesRed + numYesVotesBlue, numRequired));
		}
		*newMessage = buf;
	}

	// theoretically, it should not really be possible to pass a vote by unvoting
	// but we'll keep the passing code below here in case for some reason it works
	// (no compelling reason to remove it)

	// determine which have enough to pass
	int numPermutationsWithEnoughVotesToPass = 0;
	int numBalancedPassing = 0;
	int passingMask = 0;
	int balancedMask = 0;

	for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
		teamGenVoteRefs_t refs = TeamGen_VoteRefsForType(level.activePugProposal, i);
		int *votesIntRed = refs.votesRed;
		int *votesIntBlue = refs.votesBlue;
		permutationOfTeams_t *permutation = refs.permutation;

		if (!permutation->valid)
			continue;

		int numYesVotesRed = 0;
		int numYesVotesBlue = 0;
		for (int j = 0; j < MAX_CLIENTS; j++) {
			if (*votesIntRed & (1 << j))
				++numYesVotesRed;
			else if (*votesIntBlue & (1 << j))
				++numYesVotesBlue;
		}

		int numRequired;
		if (g_vote_teamgen_dynamicVoteRequirement.integer) {
			if (permutation->teams[0].relativeStrength >= 0.519f - 0.0001f ||
				permutation->teams[1].relativeStrength >= 0.519f - 0.0001f) {
				numRequired = 7;
			}
			else if (permutation->lowestPlayerRating >= PlayerTierToRating(PLAYERRATING_MID_B) - 0.0001) {
				if (permutation->teams[0].relativeStrength >= 0.5049f - 0.0001f ||
					permutation->teams[1].relativeStrength >= 0.5049f - 0.0001f)
					numRequired = 5; /* hc but imba */
				else
					numRequired = 4; /* just hc      */
			}
			else if (permutation->iDiff > 0 || numPlayers > 8) {
				numRequired = 6;
			}
			else {
				numRequired = 5;
			}
		}
		else {
			numRequired = 5;
		}

		qboolean thisOnePasses = qfalse;
		if (g_vote_teamgen_require2VotesOnEachTeam.integer &&
			!(permutation->lowestPlayerRating >= PlayerTierToRating(PLAYERRATING_MID_B) - 0.0001 &&
				!(permutation->teams[0].relativeStrength >= 0.5049f - 0.0001f ||
					permutation->teams[1].relativeStrength >= 0.5049f - 0.0001f))) {
			if (numYesVotesRed + numYesVotesBlue >= numRequired &&
				numYesVotesRed >= 2 && numYesVotesBlue >= 2)
				thisOnePasses = qtrue;
		}
		else {
			if (numYesVotesRed + numYesVotesBlue >= numRequired)
				thisOnePasses = qtrue;
		}

		if (thisOnePasses) {
			passingMask |= (1 << i);
			++numPermutationsWithEnoughVotesToPass;
			if (permutation->iDiff == 0) {
				balancedMask |= (1 << i);
				++numBalancedPassing;
			}
		}
	}

	if (!numPermutationsWithEnoughVotesToPass)
		return qfalse; // none have enough votes to pass

	// now we pick a winner among those which have enough to pass
	int tiebreakerOrder[] = { TEAMGENERATORTYPE_MOSTPLAYED, TEAMGENERATORTYPE_HIGHESTRATING, TEAMGENERATORTYPE_FAIREST,
							  TEAMGENERATORTYPE_INCLUSIVE, TEAMGENERATORTYPE_DESIREDPOS, TEAMGENERATORTYPE_SEMIDESIREDPOS,
							  TEAMGENERATORTYPE_FIRSTCHOICE };
	srand(teamGenSeed);
	FisherYatesShuffle(&tiebreakerOrder[0], NUM_TEAMGENERATORTYPES, sizeof(int));
	srand(time(NULL));

	permutationOfTeams_t *winnerPerm = NULL;
	char winnerLetter = 0;
	int *winnerVotesRed = NULL;
	int *winnerVotesBlue = NULL;

	// of those which have enough votes to pass, prefer the 50-50 ones
	if (numPermutationsWithEnoughVotesToPass == 1) {
		for (int i = 0; i < NUM_TEAMGENERATORTYPES; i++) {
			if (passingMask & (1 << i)) {
				teamGenVoteRefs_t refs = TeamGen_VoteRefsForType(level.activePugProposal, i);
				winnerPerm = refs.permutation;
				winnerVotesRed = refs.votesRed;
				winnerVotesBlue = refs.votesBlue;
				winnerLetter = refs.letter;
				break;
			}
		}
	}
	else {
		int preferMask = (numBalancedPassing > 0) ? balancedMask : passingMask;
		for (int k = 0; k < NUM_TEAMGENERATORTYPES; k++) {
			int idx = tiebreakerOrder[k];
			if (preferMask & (1 << idx)) {
				teamGenVoteRefs_t refs = TeamGen_VoteRefsForType(level.activePugProposal, idx);
				winnerPerm = refs.permutation;
				winnerVotesRed = refs.votesRed;
				winnerVotesBlue = refs.votesBlue;
				winnerLetter = refs.letter;
				break;
			}
		}
	}

	// announce result and activate
	if (!winnerPerm || !winnerPerm->valid)
		return qfalse; // shouldn't happen

	const char *passMsg;
	if (numPermutationsWithEnoughVotesToPass == 1) {
		passMsg = "passed.";
	}
	else {
		if (!numBalancedPassing) // no 50-50
			passMsg = "passed by random tiebreaker.";
		else if (numBalancedPassing == 1) // only one of them was 50-50
			passMsg = "passed by balance tiebreaker.";
		else // multiple 50-50
			passMsg = "passed by random tiebreaker.";
	}

	TeamGenerator_QueueServerMessageInChat(-1, va("Teams proposal %c %s", winnerLetter, passMsg));

	char printMessage[1024] = { 0 };
	Com_sprintf(printMessage, sizeof(printMessage), "*^1Red team:^7 (%0.2f'/. relative strength)\n",
		winnerPerm->teams[0].relativeStrength * 100.0);
	Q_strcat(printMessage, sizeof(printMessage), va("^5Base: ^7 %s\n", winnerPerm->teams[0].baseName));
	Q_strcat(printMessage, sizeof(printMessage), va("^6Chase: ^7 %s\n", winnerPerm->teams[0].chaseName));
	Q_strcat(printMessage, sizeof(printMessage), va("^2Offense: ^7 %s^7, ", winnerPerm->teams[0].offense1Name));
	Q_strcat(printMessage, sizeof(printMessage), va("%s\n\n", winnerPerm->teams[0].offense2Name));
	Q_strcat(printMessage, sizeof(printMessage), va("^4Blue team:^7 (%0.2f'/. relative strength)\n",
		winnerPerm->teams[1].relativeStrength * 100.0));
	Q_strcat(printMessage, sizeof(printMessage), va("^5Base: ^7 %s\n", winnerPerm->teams[1].baseName));
	Q_strcat(printMessage, sizeof(printMessage), va("^6Chase: ^7 %s\n", winnerPerm->teams[1].chaseName));
	Q_strcat(printMessage, sizeof(printMessage), va("^2Offense: ^7 %s^7, ", winnerPerm->teams[1].offense1Name));
	Q_strcat(printMessage, sizeof(printMessage), va("^7%s\n\n", winnerPerm->teams[1].offense2Name));
	TeamGenerator_QueueServerMessageInConsole(-1, printMessage);

	ActivateTeamsProposal(winnerPerm);
	ListClear(&level.activePugProposal->avoidedHashesList);
	ListRemove(&level.pugProposalsList, level.activePugProposal);
	level.activePugProposal = NULL;
	TeamGenerator_ResetConfirmationsAndMutes();

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

	if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote on this client.");
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

	if (numYesVotesFromEligiblePlayers >= numRequired && (doVote || (pugWasLive && !pugIsLive))) {
		// NB: test the job table, not level.activePugProposal -- that's only assigned once results are
		// applied, so during GENERATING it's still NULL and the old check could never fire
		if (TeamGen_RealJobInFlightForProposal(set->num)) {
			// already generating this exact proposal -- nothing to do
		}
		else if (TeamGen_RealJobInFlight()) {
			// a different proposal is generating -- queue this one to activate as soon as that completes
			set->queuedActivation = qtrue;
			TeamGenerator_QueueServerMessageInChat(-1, va("Pug proposal %d passed and is queued; it will be activated once the current calculation finishes.", set->num));
		}
		else {
			ActivatePugProposal(set, qfalse);
		}
	}

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

	if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote on this client.");
		return qtrue;
	}

	sortedClient_t clients[MAX_CLIENTS] = {0};
	int numEligible = 0, numIngameEligible = 0;
	GetCurrentPickablePlayers(&clients[0], &numEligible, &numIngameEligible, qfalse, qtrue);

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
			TeamGenerator_QueueServerMessageInChat(j, va("^7Mivel t\xf6" "bb mint 8 j\xe1t\xe9kos van, \xedrd be a ^3%cpickable^7 parancsot, ha pugozni szeretn\xe9l.", TEAMGEN_CHAT_COMMAND_CHARACTER));
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
		"Eat some delicious k\xfcrt\xf5s kal\xe1" "cs",
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
				Com_sprintf(renameMessage, sizeof(renameMessage), "Mivel t\xf6" "bb mint 8 j\xe1t\xe9kos van, \xedrd be a ^3%cpickable^7 parancsot, ha pugozni szeretn\xe9l.", TEAMGEN_CHAT_COMMAND_CHARACTER);
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
	pugProposal_t *set = level.activePugProposal;
	set->votedToRerollClients = set->votedToCancelClients = 0;

	int nextRerollNum = rerollNum + 1;

	// if a lookahead is ready and its tag still matches, apply it instantly -- no wait at all
	if (set->speculativeReroll.valid &&
		set->speculativeReroll.rerollNumUsed == nextRerollNum &&
		set->speculativeReroll.avoidedHashesTag == TeamGen_HashAvoidedHashesList(&set->avoidedHashesList)) {

		TeamGen_AnnounceThreadDebug(va("Pug proposal #%d: applying ready background reroll-lookahead precompute (%dx iterations) instantly.", set->num, set->speculativeReroll.iterationLevel));

		pugProposal_t oldHashes = { 0 };
		int numOld = 0;
		if (set->suggested.valid) { oldHashes.suggested.valid = qtrue; oldHashes.suggested.hash = set->suggested.hash; ++numOld; }
		if (set->highestCaliber.valid) { oldHashes.highestCaliber.valid = qtrue; oldHashes.highestCaliber.hash = set->highestCaliber.hash; ++numOld; }
		if (set->fairest.valid) { oldHashes.fairest.valid = qtrue; oldHashes.fairest.hash = set->fairest.hash; ++numOld; }
		if (set->desired.valid) { oldHashes.desired.valid = qtrue; oldHashes.desired.hash = set->desired.hash; ++numOld; }
		if (set->inclusive.valid) { oldHashes.inclusive.valid = qtrue; oldHashes.inclusive.hash = set->inclusive.hash; ++numOld; }
		if (set->semiDesired.valid) { oldHashes.semiDesired.valid = qtrue; oldHashes.semiDesired.hash = set->semiDesired.hash; ++numOld; }
		if (set->firstChoice.valid) { oldHashes.firstChoice.valid = qtrue; oldHashes.firstChoice.hash = set->firstChoice.hash; ++numOld; }

		teamGenResultPayload_t result;
		result.succeeded = qtrue;
		result.suggested = set->speculativeReroll.suggested;
		result.highestCaliber = set->speculativeReroll.highestCaliber;
		result.fairest = set->speculativeReroll.fairest;
		result.desired = set->speculativeReroll.desired;
		result.inclusive = set->speculativeReroll.inclusive;
		result.semiDesired = set->speculativeReroll.semiDesired;
		result.firstChoice = set->speculativeReroll.firstChoice;
		result.numValidPermutationsChecked = set->speculativeReroll.numValidPermutationsChecked;
		// lookahead precompute never generates a report -- nothing to free
		result.debugLogs = NULL;
		result.kind = TEAMGENJOB_REROLL;
		result.numEligible = result.numIngame = 0;
		result.seedBase = 0;
		result.rerollNumUsed = nextRerollNum;
		result.elapsedMs = 0;
		result.iterationsRun = 0;
		result.bestIterationIndex = -1;

		int rerollIterationLevel = set->speculativeReroll.iterationLevel;
		rerollNum = nextRerollNum;
		set->speculativeReroll.valid = qfalse;
		TeamGenAsync_ApplyRerollResult(set, &result, forcedByServer, &oldHashes, numOld);
		// the rerolled result is the authoritative "last shown" state for this roster now
		TeamGen_UpdateSpeculativeRosterCacheAfterReroll(TeamGen_EligibleRosterHashForProposal(set), rerollIterationLevel,
			&result.suggested, &result.highestCaliber, &result.fairest, &result.desired, &result.inclusive, &result.semiDesired, &result.firstChoice,
			result.numValidPermutationsChecked);
		TeamGen_DispatchRerollLookahead(set);
		return;
	}

	if (set->speculativeReroll.valid)
		TeamGen_AnnounceThreadDebug(va("Pug proposal #%d: discarding stale background reroll-lookahead precompute (roster/avoided-hashes changed).", set->num));
	set->speculativeReroll.valid = qfalse; // stale regardless of the tag check above; discard
	++rerollNum;
	set->epoch++; // supersedes any stale in-flight job for this proposal (e.g. a lookahead)
	if (!TeamGen_DispatchGenerationJob(set, TEAMGENJOB_REROLL, forcedByServer, rerollNum, 1)) {
		TeamGenerator_QueueServerMessageInChat(-1, va("Failed to generate different teams when rerolling pug proposal %d%s (%s). Voting will continue for the existing teams.", set->num, forcedByServer ? " by server" : "", set->namesStr));
		set->state = PUGPROPOSAL_STATE_READY_FOR_TEAM_VOTE;
	}
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

	if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote on this client.");
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

	int numRerollVotesFromEligiblePlayers = 0;
	const int numRequired = TeamGen_CountRerollVotes(&numRerollVotesFromEligiblePlayers);

	if (newMessage) {
		static char buf[MAX_STRING_CHARS] = { 0 };
		Com_sprintf(buf, sizeof(buf), "%creroll   ^%c(%d/%d)", TEAMGEN_CHAT_COMMAND_CHARACTER, doVote ? '7' : '9', numRerollVotesFromEligiblePlayers, numRequired);
		*newMessage = buf;
	}

	if (doVote && numRerollVotesFromEligiblePlayers >= numRequired) {
		// if a previous generation for this proposal is still in flight, don't dispatch a second one. the
		// vote is registered above and TeamGen_CheckDeferredReroll fires it when that job completes.
		if (level.activePugProposal->state == PUGPROPOSAL_STATE_READY_FOR_TEAM_VOTE)
			TeamGenerator_DoReroll(qfalse);
		else
			TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Reroll vote passed, but teams are still being calculated; it will take effect once that finishes.");
	}

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

char NormalizeCharForFuck(unsigned char c) {
	switch (c) {
	case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5:
		return 'A';
	case 0xC7:
		return 'C';
	case 0xC8: case 0xC9: case 0xCA: case 0xCB:
		return 'E';
	case 0xCC: case 0xCD: case 0xCE: case 0xCF:
		return 'I';
	case 0xD0:
		return 'D';
	case 0xD1:
		return 'N';
	case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8:
		return 'O';
	case 0xD9: case 0xDA: case 0xDB: case 0xDC:
		return 'U';
	case 0xDD:
		return 'Y';
	case 0xDE:
		return 'P';
	case 0xDF:
		return 's';
	case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5:
		return 'a';
	case 0xE7:
		return 'c';
	case 0xE8: case 0xE9: case 0xEA: case 0xEB:
		return 'e';
	case 0xEC: case 0xED: case 0xEE: case 0xEF:
		return 'i';
	case 0xF0:
		return 'd';
	case 0xF1:
		return 'n';
	case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8:
		return 'o';
	case 0xF9: case 0xFA: case 0xFB: case 0xFC:
		return 'u';
	case 0xFD: case 0xFF:
		return 'y';
	case 0xFE:
		return 'p';
	default:
		break;
	}

	switch (c) {
	case '0':
		return 'o';
	case '1':
		return 'l';
	case '2':
		return 'z';
	case '3':
		return 'e';
	case '4':
		return 'a';
	case '5':
		return 's';
	case '6':
		return 'g';
	case '7':
		return 't';
	case '8':
		return 'b';
	case '9':
		return 'g';
	default:
		return c;
	}
}

qboolean CheckFuckStr(const char *str, const char *word) {
	int j = 0;

	for (int i = 0; str[i] != '\0'; i++) {
		unsigned char c_str = (unsigned char)str[i];
		char norm_c_str = NormalizeCharForFuck(c_str);
		norm_c_str = tolower((unsigned char)norm_c_str);

		unsigned char c_word = (unsigned char)word[j];
		char norm_c_word = tolower(c_word);

		if (norm_c_str == norm_c_word) {
			j++;

			if (word[j] == '\0') {
				return qtrue;
			}
		}
	}
	return qfalse;
}

qboolean TeamGenerator_MemeFuckVote(gentity_t *ent, const char *voteStr, char **newMessage) {
	assert(ent && ent->client);

	if (PlayerIsMuted(ent)) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "As punishment for failing the confirmation too many times, you are muted until map change. Reconnecting will not unmute you.");
		return qtrue;
	}

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
	if (CheckFuckStr(fuckVote->fucked, "duo") || stristr(fuckVote->fucked, "duo") || stristr(fuckVote->fucked, "doo o") || stristr(fuckVote->fucked, "dooo o") || stristr(fuckVote->fucked, "dew o"))
		numRequired = 16;

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

	if (PlayerIsMuted(ent)) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "As punishment for failing the confirmation too many times, you are muted until map change. Reconnecting will not unmute you.");
		return qtrue;
	}

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

	if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote on this client.");
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

	if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote on this client.");
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
	// bump epoch before removal so any in-flight job is unambiguously stale
	level.activePugProposal->epoch++;
	ListClear(&level.activePugProposal->avoidedHashesList);
	ListRemove(&level.pugProposalsList, level.activePugProposal);
	level.activePugProposal = NULL;
	TeamGenerator_ResetConfirmationsAndMutes();
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

	if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote on this client.");
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
		Q_strcat(buf, sizeof(buf), va("Pug proposal ^5%d^7 - %s%s%s\n", p->num, p->namesStr, p == level.activePugProposal ? " ^2(Currently active)^7" : "", p->state == PUGPROPOSAL_STATE_GENERATING ? " ^3(generating...)^7" : ""));
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

	if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot vote on this client.");
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

static qboolean NamesMatch(const char *a, const char *b) {
	return !Q_stricmpclean(a, b);
}

static qboolean NameMatchesEither(const char *name, const char *a, const char *b) {
	return NamesMatch(name, a) || NamesMatch(name, b);
}

static qboolean TeamGenerator_ConfirmMatches(permutationOfTeams_t *perm, ctfPosition_t pos, const char *name1, const char *name2) {
	assert(perm && name1 && name2);

	if (NamesMatch(name1, name2)) { // they typed the same name twice
		return qfalse;
	}

	switch (pos) {
	case CTFPOSITION_BASE: {
		const char *base1 = perm->teams[0].baseName;
		const char *base2 = perm->teams[1].baseName;
		return (NameMatchesEither(name1, base1, base2) && NameMatchesEither(name2, base1, base2));
	}
	case CTFPOSITION_CHASE: {
		const char *chase1 = perm->teams[0].chaseName;
		const char *chase2 = perm->teams[1].chaseName;
		return (NameMatchesEither(name1, chase1, chase2) && NameMatchesEither(name2, chase1, chase2));
	}
	case CTFPOSITION_OFFENSE: {
		const char *team1_off1 = perm->teams[0].offense1Name;
		const char *team1_off2 = perm->teams[0].offense2Name;
		const char *team2_off1 = perm->teams[1].offense1Name;
		const char *team2_off2 = perm->teams[1].offense2Name;

		qboolean name1_in_team1 = NameMatchesEither(name1, team1_off1, team1_off2);
		qboolean name1_in_team2 = NameMatchesEither(name1, team2_off1, team2_off2);
		qboolean name2_in_team1 = NameMatchesEither(name2, team1_off1, team1_off2);
		qboolean name2_in_team2 = NameMatchesEither(name2, team2_off1, team2_off2);

		return (name1_in_team1 && name2_in_team2) || (name1_in_team2 && name2_in_team1);
	}
	default:
		return qfalse;
	}
}

typedef struct {
	node_t	node;
	int		accountId;
} mutedPlayer_t;

static qboolean MutedPlayerMatches(genericNode_t *node, void *userData) {
	const mutedPlayer_t *existing = (const mutedPlayer_t *)node;
	const int thisGuyAccountId = *((const int *)userData);
	if (existing && existing->accountId == thisGuyAccountId)
		return qtrue;
	return qfalse;
}

qboolean PlayerIsMuted(gentity_t *ent) {
	if (!ent || !ent->client || !ent->client->account || !g_vote_teamgen_mute.integer)
		return qfalse;

	mutedPlayer_t *found = ListFind(&level.mutedPlayersList, MutedPlayerMatches, &ent->client->account->id, NULL);
	return !!found;
}

static void PrintConfirmError(gentity_t *ent) {
	if (!ent || !ent->client || !ent->client->account) {
		assert(qfalse);
		return;
	}

	if (++ent->client->pers.confirmedReading.tries >= 3) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "As punishment for failing to confirm too many times, you are now muted until map change. Reconnecting will not unmute you.");
		mutedPlayer_t *found = ListFind(&level.mutedPlayersList, MutedPlayerMatches, &ent->client->account->id, NULL);
		if (!found) {
			found = ListAdd(&level.mutedPlayersList, sizeof(mutedPlayer_t));
			found->accountId = ent->client->account->id;
		}
		PrintBasedOnAccountFlags(ACCOUNTFLAG_ADMIN, va("%s is now muted until map change due to failing to confirm too many times.", ent->client->account->name));
		return;
	}

	TeamGenerator_QueueServerMessageInChat(ent - g_entities, va(
		level.activePugProposal->confirmation[ent - g_entities].pos == CTFPOSITION_OFFENSE ?
		"As a troll, you must confirm that you have read the teams proposals by naming one ^3%s^6 player from each team in permutation ^3%c^6 before voting. Use the format:   %cconfirm name1 name2" :
		"As a troll, you must confirm that you have read the teams proposals by naming the ^3%s^6 players in permutation ^3%c^6 before voting. Use the format:   %cconfirm name1 name2",
		NameForPos(level.activePugProposal->confirmation[ent - g_entities].pos),
		level.activePugProposal->confirmation[ent - g_entities].letter,
		TEAMGEN_CHAT_COMMAND_CHARACTER));
}

static qboolean TeamGenerator_Confirm(gentity_t *ent, const char *confirmStr, char **newMessage) {
	assert(ent && ent->client);

	if (!ent->client->account) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You do not have an account, so you cannot confirm.");
		return qtrue;
	}

	if ((ent->client->account->flags & ACCOUNTFLAG_ELOBOTSELFHOST) && !Q_stricmpclean(ent->client->pers.netname, "elo BOT")) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You have either modded elo bot to cheese the system or this is a bug which you should report.");
		return qtrue;
	}

	if (ent->client->sess.clientType != CLIENT_TYPE_NORMAL) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You cannot confirm on this client.");
		return qtrue;
	}

	if (!level.activePugProposal || !(ent->client->account->flags & ACCOUNTFLAG_ULTRAINSTAVOTETROLL) || ent->client->pers.confirmedReading.didConfirm) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You are not required to confirm.");
		return qtrue;
	}

	if (!level.activePugProposal->confirmation[ent - g_entities].permutation || !level.activePugProposal->confirmation[ent - g_entities].pos || !level.activePugProposal->confirmation[ent - g_entities].letter) {
		TeamGenerator_QueueServerMessageInChat(ent - g_entities, "You are not required to confirm.");
		return qtrue;
	}

	if (!VALIDSTRING(confirmStr)) {
		PrintConfirmError(ent);
		return qtrue;
	}

	const char *args = confirmStr + strlen("confirm");
	while (*args == ' ') ++args;
	if (!*args) {
		PrintConfirmError(ent);
		return qtrue;
	}

	char name1[MAX_NETNAME], name2[MAX_NETNAME];
	int numArgs = sscanf(args, "%s %s", name1, name2);
	if (numArgs != 2) {
		PrintConfirmError(ent);
		return qtrue;
	}


	if (!TeamGenerator_ConfirmMatches(level.activePugProposal->confirmation[ent - g_entities].permutation, level.activePugProposal->confirmation[ent - g_entities].pos, name1, name2)) {
		PrintConfirmError(ent);
		return qtrue;
	}

	ent->client->pers.confirmedReading.didConfirm = qtrue;
	TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Thank you carefully reading and considering the teams proposals. You may vote shortly.");
	if (!(ent->client->account->flags & ACCOUNTFLAG_ADMIN))
		PrintBasedOnAccountFlags(ACCOUNTFLAG_ADMIN, va("%s successfully entered ?confirm %s %s\n", ent->client->account->name, name1, name2));

	ent->client->pers.triedToInstaVote = trap_Milliseconds(); // evil (owned)

	return qtrue;
}

// reassign invalidated pending confirmation targets to valid ones
void TeamGenerator_FixInvalidConfirmationPermutations(void) {
	if (!level.activePugProposal)
		return;

	typedef struct {
		permutationOfTeams_t *perm;
		char letter;
	} permChoice_t;

	pugProposal_t *set = level.activePugProposal;

	permChoice_t candidates[7] = {
		{ &set->suggested,     set->suggestedLetter },
		{ &set->fairest,       set->fairestLetter },
		{ &set->desired,       set->desiredLetter },
		{ &set->highestCaliber,set->highestCaliberLetter },
		{ &set->inclusive,     set->inclusiveLetter },
		{ &set->semiDesired,   set->semiDesiredLetter },
		{ &set->firstChoice,   set->firstChoiceLetter }
	};

	permChoice_t valid[7];
	int numValid = 0;
	for (int i = 0; i < 7; i++) {
		if (candidates[i].perm && candidates[i].perm->valid)
			valid[numValid++] = candidates[i];
	}

	if (numValid > 0) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			confirmStatus_t *conf = &set->confirmation[i];
			if (!conf->permutation || !conf->letter || conf->pos == CTFPOSITION_UNKNOWN)
				continue;

			if (!level.clients[i].pers.confirmedReading.didConfirm && !conf->permutation->valid) {
				// this one was just invalidated by the disconnecting guy, so we need to change it to something else (which remains valid)
				srand(teamGenSeed + i);
				const int rng1 = rand();
				const int rng2 = rand();
				srand(time(NULL));
				permChoice_t *chosen = &valid[rng1 % numValid];

				srand(teamGenSeed);
				ctfPosition_t pos;
				switch (rng2 % 3) {
				case 0: pos = CTFPOSITION_BASE; break;
				case 1: pos = CTFPOSITION_CHASE; break;
				default: pos = CTFPOSITION_OFFENSE; break;
				}

				conf->permutation = chosen->perm;
				conf->letter = chosen->letter;
				conf->pos = pos;

				gentity_t *ent = &g_entities[i];
				if (ent && ent->client && ent->client->account && (ent->client->account->flags & ACCOUNTFLAG_ULTRAINSTAVOTETROLL)) {
					TeamGenerator_QueueServerMessageInChat(ent - g_entities, "Your required confirmation has changed due to a player disconnecting.");
					TeamGenerator_QueueServerMessageInChat(ent - g_entities, va(
						level.activePugProposal->confirmation[ent - g_entities].pos == CTFPOSITION_OFFENSE ?
						"As a troll, you must confirm that you have read the teams proposals by naming one ^3%s^6 player from each team in permutation ^3%c^6 before voting. Use the format:   %cconfirm name1 name2" :
						"As a troll, you must confirm that you have read the teams proposals by naming the ^3%s^6 players in permutation ^3%c^6 before voting. Use the format:   %cconfirm name1 name2",
						NameForPos(level.activePugProposal->confirmation[ent - g_entities].pos),
						level.activePugProposal->confirmation[ent - g_entities].letter,
						TEAMGEN_CHAT_COMMAND_CHARACTER));
				}
			}
		}
	}
}

void TeamGenerator_ResetConfirmationsAndMutes(void) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		memset(&level.clients[i].pers.confirmedReading, 0, sizeof(level.clients[i].pers.confirmedReading));
		if (PlayerIsMuted(&g_entities[i]))
			TeamGenerator_QueueServerMessageInChat(i, "You are no longer muted.");
	}
	ListClear(&level.mutedPlayersList);
}

// returns qtrue if the message should be filtered out
qboolean TeamGenerator_CheckForChatCommand(gentity_t *ent, const char *s, char **newMessage, qboolean *successfullyUsedATeamgenCommand) {
	if (successfullyUsedATeamgenCommand)
		*successfullyUsedATeamgenCommand = qfalse;

	if (g_gametype.integer != GT_CTF) {
		return qfalse;
	}

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
		if (successfullyUsedATeamgenCommand)
			*successfullyUsedATeamgenCommand = qtrue;
		return qtrue;
	}

	if (!Q_stricmp(s, "start") || !Q_stricmp(s, "s")) {
		if (successfullyUsedATeamgenCommand)
			*successfullyUsedATeamgenCommand = qtrue;
		return TeamGenerator_PugStart(ent, newMessage);
	}

	if (!Q_stricmp(s, "reroll") || !Q_stricmp(s, "r")) {
		if (successfullyUsedATeamgenCommand)
			*successfullyUsedATeamgenCommand = qtrue;
		return TeamGenerator_VoteToReroll(ent, newMessage);
	}

	if (!Q_stricmpn(s, "bar", 3) && (strlen(s) <= 3 || isspace(*(s + 3)))) {
		if (successfullyUsedATeamgenCommand)
			*successfullyUsedATeamgenCommand = qtrue;
		return TeamGenerator_VoteToBar(ent, s, newMessage);
	}

	if (!Q_stricmpn(s, "unbar", 5) && (strlen(s) <= 5 || isspace(*(s + 5)))) {
		if (successfullyUsedATeamgenCommand)
			*successfullyUsedATeamgenCommand = qtrue;
		return TeamGenerator_VoteToUnbar(ent, s, newMessage);
	}

	if (!Q_stricmp(s, "cancel")) {
		if (successfullyUsedATeamgenCommand)
			*successfullyUsedATeamgenCommand = qtrue;
		return TeamGenerator_VoteToCancel(ent, newMessage);
	}

	if (!Q_stricmp(s, "list")) {
		if (successfullyUsedATeamgenCommand)
			*successfullyUsedATeamgenCommand = qtrue;
		TeamGenerator_PrintPlayersInPugProposals(ent);
		return qtrue;
	}

	if (!Q_stricmpn(s, "fuck", 4) && (strlen(s) <= 4 || isspace(*(s + 4)))) {
		return TeamGenerator_MemeFuckVote(ent, s, newMessage);
	}

	if (!Q_stricmpn(s, "go", 2) && (strlen(s) <= 2 || isspace(*(s + 2)))) {
		return TeamGenerator_MemeGoVote(ent, s, newMessage);
	}

	if (!Q_stricmp(s, "pickable")) {
		if (successfullyUsedATeamgenCommand)
			*successfullyUsedATeamgenCommand = qtrue;
		return TeamGenerator_PermabarredPlayerMarkAsPickable(ent, newMessage);
	}

	if (!Q_stricmpn(s, "confirm", 7) && (strlen(s) <= 7 || isspace(*(s + 7)))) {
		if (successfullyUsedATeamgenCommand)
			*successfullyUsedATeamgenCommand = qtrue;
		return TeamGenerator_Confirm(ent, s, newMessage);
	}

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
		if (!invalidVote) {
			if (successfullyUsedATeamgenCommand)
				*successfullyUsedATeamgenCommand = qtrue;
			return TeamGenerator_VoteForTeamPermutations(ent, s, newMessage);
		}
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
		if (!invalidVote) {
			if (successfullyUsedATeamgenCommand)
				*successfullyUsedATeamgenCommand = qtrue;
			return TeamGenerator_UnvoteForTeamPermutations(ent, s + 2, newMessage, s);
		}
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
		if (!invalidVote) {
			if (successfullyUsedATeamgenCommand)
				*successfullyUsedATeamgenCommand = qtrue;
			return TeamGenerator_UnvoteForTeamPermutations(ent, s + 3, newMessage, s);
		}
	}

	if (atoi(s)) {
		if (successfullyUsedATeamgenCommand)
			*successfullyUsedATeamgenCommand = qtrue;
		return TeamGenerator_VoteYesToPugProposal(ent, atoi(s), NULL, newMessage);
	}

	return qfalse;
}

static void TeamGenerator_ServerCommand_Start(qboolean forcePass) {
	sortedClient_t clients[MAX_CLIENTS] = { 0 };
	int numEligible = 0, numIngameEligible = 0;
	GetCurrentPickablePlayers(&clients[0], &numEligible, &numIngameEligible, qfalse, qtrue);

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
		TeamGenerator_ResetConfirmationsAndMutes();

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
		TeamGenerator_ResetConfirmationsAndMutes();
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
			Q_strcat(buf, sizeof(buf), va("Pug proposal ^5%d^7 - %s%s%s\n", p->num, p->namesStr, p == level.activePugProposal ? " ^2(Currently active)^7" : "", p->state == PUGPROPOSAL_STATE_GENERATING ? " ^3(generating...)^7" : ""));
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
