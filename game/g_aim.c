#include "g_local.h"
#include "g_database.h"

// begin g_main

// called on init
void LoadAimPacks(void) {
	if (!g_enableAimPractice.integer) {
		Com_Printf("Aim practice is disabled; skipping pack loading from database.\n");
		return;
	}

	// load the packs
	level.aimPracticePackList = G_DBTopAimLoadPacks(level.mapname);
	if (level.aimPracticePackList) {
		Com_Printf("Loaded %d aim practice packs from database.\n", level.aimPracticePackList->size);
	}
	else {
		Com_Printf("No aim practice packs loaded from database.\n");
		level.aimPracticePackList = malloc(sizeof(list_t));
		memset(level.aimPracticePackList, 0, sizeof(list_t));
		return;
	}

	// iterate through the pack list, spawning NPCs
	iterator_t iter;
	ListIterate(level.aimPracticePackList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		aimPracticePack_t *pack = (aimPracticePack_t *)IteratorNext(&iter);
		if (SpawnAimPracticePackNPC(NULL, pack))
			Com_Printf("Spawned NPC for pack %s.\n", pack->packName);
		else
			Com_Printf("Failed to spawn NPC for pack %s!\n", pack->packName);
	}
}

// called on shutdown
void SaveDeleteAndFreeAimPacks(void) {
	if (!g_enableAimPractice.integer || !level.aimPracticePackList)
		return;

	// iterate through the pack list for this map, deleting and saving as needed
	iterator_t iter;
	ListIterate(level.aimPracticePackList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		aimPracticePack_t *pack = (aimPracticePack_t *)IteratorNext(&iter);

		ListClear(&pack->cachedScoresList); // clear out the list for this pack

		if (pack->deleteMe) {
			if (G_DBTopAimDeletePack(pack))
				Com_Printf("Deleted aim practice pack %s.\n", pack->packName);
			else
				Com_Printf("Failed to delete aim practice pack %s!\n", pack->packName);
		}
		else if (pack->save) {
			if (G_DBTopAimSavePack(pack))
				Com_Printf("Saved aim practice pack %s.\n", pack->packName);
			else
				Com_Printf("Failed to save aim practice pack %s!\n", pack->packName);
		}
	}

	// free all of the packs
	ListClear(level.aimPracticePackList);
	free(level.aimPracticePackList);
}

// begin g_client

static qboolean ScoreMatches(genericNode_t *node, void *userData) {
	cachedScore_t *existing = (cachedScore_t *)node;
	int thisSessionId = *((int *)userData);

	if (!existing)
		return qfalse;

	if (existing->sessionId == thisSessionId)
		return qtrue;

	return qfalse;
}

void G_InitClientAimRecordsCache(gclient_t *client) {
	client->sess.canSubmitAimTimes = qfalse;

	qboolean errored = qfalse;
	int numLoaded = 0;

	// cache all the aim scores for this client within the cached scores list of each pack
	if (client->session) {
		client->sess.canSubmitAimTimes = qtrue;

		iterator_t iter;
		ListIterate(level.aimPracticePackList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			aimPracticePack_t *pack = (aimPracticePack_t *)IteratorNext(&iter);

			aimRecord_t record = { 0 };
			if (G_DBTopAimLoadAimRecord(client->session->id, level.mapname, pack, &record)) {
				if (!record.time)
					continue;

				cachedScore_t *score = ListFind(&pack->cachedScoresList, ScoreMatches, &client->session->id, NULL);
				if (!score) {
					score = ListAdd(&pack->cachedScoresList, sizeof(cachedScore_t));
					score->sessionId = client->session->id;
				}

				score->score = record.time;

				++numLoaded;
			}
			else {
				errored = qtrue;
				break;
			}
		}

		if (numLoaded) {
			Com_Printf("Loaded %d aim records for client %d (session id: %d)\n", numLoaded, client - level.clients, client->session->id);
		}
	}

	if (errored) {
		Com_Printf("Failed to load aim records for client %d\n", client - level.clients);
		client->sess.canSubmitAimTimes = qfalse;

		// delete their cached scores
		if (client->session) {
			iterator_t iter;
			ListIterate(level.aimPracticePackList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				aimPracticePack_t *pack = (aimPracticePack_t *)IteratorNext(&iter);
				cachedScore_t *score = ListFind(&pack->cachedScoresList, ScoreMatches, &client->session->id, NULL);
				if (score)
					ListRemove(&pack->cachedScoresList, score);
			}
		}

		return;
	}
}

static int GetScoreForPack(aimPracticePack_t *pack, int sessionId) {
	assert(pack);
	cachedScore_t *score = ListFind(&pack->cachedScoresList, ScoreMatches, &sessionId, NULL);
	return score ? score->score : 0;
}

static void SetScoreForPack(aimPracticePack_t *pack, int sessionId, int newScore) {
	assert(pack);
	cachedScore_t *score = ListFind(&pack->cachedScoresList, ScoreMatches, &sessionId, NULL);

	if (!score) {
		score = ListAdd(&pack->cachedScoresList, sizeof(cachedScore_t));
		score->sessionId = sessionId;
	}

	score->score = newScore;
}

// begin NPC_spawn

// try to prevent any weird shit in the npc file from causing this npc to behave unexpectedly
void FilterNPCForAimPractice(gentity_t *ent) {
	assert(ent && ent->client);
	static vec3_t playerMins = { -15, -15, DEFAULT_MINS_2 };
	static vec3_t playerMaxs = { 15, 15, DEFAULT_MAXS_2 };
	VectorCopy(playerMins, ent->r.mins);
	VectorCopy(playerMaxs, ent->r.maxs);
	ent->modelScale[0] = ent->modelScale[1] = ent->modelScale[2] = 1.0f;
	ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_MELEE);
	ent->client->ps.weapon = WP_MELEE;
	ent->client->ps.crouchheight = CROUCH_MAXS_2;
	ent->client->ps.standheight = DEFAULT_MAXS_2;
}

// begin g_cmds

static qboolean ModelInUseByBot(const char *model) {
	if (!level.aimPracticePackList)
		return qfalse;

	iterator_t iter;
	ListIterate(level.aimPracticePackList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		aimPracticePack_t *pack = (aimPracticePack_t *)IteratorNext(&iter);
		if (pack->ent && !Q_stricmp(pack->ent->NPC_type, model))
			return qtrue;
	}
	return qfalse;
}

void RandomizeAndRestartPack(aimPracticePack_t *pack) {
	assert(pack);

	// setup the array
	// e.g. 2 routes with 3 reps will look like {0, 0, 0, 1, 1, 1}
	for (int i = 0; i < pack->numVariants * pack->numRepsPerVariant; i++)
		pack->randomVariantOrder[i] = i / pack->numRepsPerVariant;

	if (pack->numVariants > 1) {
		// fisher-yates shuffle
		for (int i = (pack->numVariants * pack->numRepsPerVariant) - 1; i >= 1; i--) {
			int j = rand() % (i + 1);
			int temp = pack->randomVariantOrder[i];
			pack->randomVariantOrder[i] = pack->randomVariantOrder[j];
			pack->randomVariantOrder[j] = temp;
		}
	}

	// force it to restart
	pack->currentTrailNum = 0;
	pack->currentVariantNumInRandomVariantOrder = (pack->numVariants * pack->numRepsPerVariant) - 1; // set to the final one so that pmove bumps it back to 0
	pack->currentVariantIndex = pack->randomVariantOrder[pack->currentVariantNumInRandomVariantOrder];
	pack->forceRestart = qtrue;

	// reset anyone who is currently using this
	if (pack->ent) {
		for (int i = 0; i < MAX_CLIENTS; i++) {
			gentity_t *thisEnt = &g_entities[i];
			if (thisEnt->aimPracticeEntBeingUsed != pack->ent)
				continue;
			thisEnt->numAimPracticeSpawns = 0;
			thisEnt->numTotalAimPracticeHits = 0;
			memset(thisEnt->numAimPracticeHitsOfWeapon, 0, sizeof(thisEnt->numAimPracticeHitsOfWeapon));
			G_GiveRacemodeItemsAndFullStats(thisEnt);
		}
	}
}

extern gentity_t *NPC_SpawnType(gentity_t *ent, char *npc_type, char *targetname, qboolean isVehicle);

#define DEFAULT_AUTO_DISTANCE_FROM_BOT	(200)

static void DeletePackNPC(aimPracticePack_t *pack) {
	assert(pack && pack->ent);
	// kick anyone out that is using it
	for (int i = 0; i < MAX_CLIENTS; i++) {
		gentity_t *thisEnt = &g_entities[i];
		if (thisEnt->aimPracticeEntBeingUsed == pack->ent)
			ExitAimTraining(thisEnt);
	}
	// free it and unlink it from the pack
	G_FreeEntity(pack->ent);
	pack->ent = NULL;
}

qboolean SpawnAimPracticePackNPC(gentity_t *ent, aimPracticePack_t *pack) {
	assert(pack);

	if (pack->ent) { // there is already an ent???
		assert(qfalse);
		Com_Printf("Warning: trying to spawn an NPC for pack %s, but there is already an NPC (%s, entity %d)??? This should not happen. Deleting the existing NPC.\n",
			pack->packName, pack->ent->NPC_type, pack->ent - g_entities);
		DeletePackNPC(pack);
	}

	// pick a random npc type
	const char *possibleNpcTypes[] = { "reborn", "alora", "tavion", "tavion_new", "trandoshan", "weequay",
		"noghri", "kyle", "gran", "rebel", "bartender", "cultist", "jan", "lando", "stormtrooper", "imperial",
		"prisoner", "reborn_new" };
	const int numNpcTypes = sizeof(possibleNpcTypes) / sizeof(*possibleNpcTypes);
	const char *hashMe = va("%s%s%d", pack->packName, level.mapname, pack->ownerAccountId);
	unsigned int hash = (unsigned int)XXH32(hashMe, strlen(hashMe), 0x69420);
	const char *npcType = possibleNpcTypes[hash % numNpcTypes];
	if (ModelInUseByBot(npcType)) {
		int tries = 0;
		do {
			npcType = possibleNpcTypes[Q_irand(0, numNpcTypes - 1)];
		} while (ModelInUseByBot(npcType) && ++tries < 1024); // prevent infinite loop
	}
	assert(VALIDSTRING(npcType));

	// spawn it
	gentity_t *npc = NPC_SpawnType(ent, (char *)npcType, "", qfalse);
	if (!npc) {
		if (ent)
			PrintIngame(ent - g_entities, "Error creating aim practice NPC\n");
		else
			Com_Printf("Error creating aim practice NPC\n");
		return qfalse;
	}
	if (npc->s.NPC_class == CLASS_VEHICLE) { // sanity check; shouldn't have been spawned anyway
		G_FreeEntity(npc);
		return qfalse;
	}

	// link the pack and the npc
	pack->ent = npc;
	npc->isAimPracticePack = pack;

	// set some parameters for the pack that won't be saved
	RandomizeAndRestartPack(pack);
	pack->autoDist = DEFAULT_AUTO_DISTANCE_FROM_BOT; // initialize this in case they want to edit

	// initialize some npc-specific stuff
	npc->client->ps.stats[STAT_RACEMODE] = 1;
	npc->flags |= FL_NO_KNOCKBACK;
	npc->r.svFlags |= SVF_COOLKIDSCLUB | SVF_NO_BASIC_SOUNDS | SVF_NO_COMBAT_SOUNDS | SVF_NO_EXTRA_SOUNDS;
	npc->s.teamowner = NPCTEAM_ENEMY; // so it uses enemy crosshair color

	aimVariant_t *var = &pack->variants[pack->currentVariantIndex];
	VectorCopy(var->trail[pack->currentTrailNum].currentOrigin, npc->r.currentOrigin);

	return qtrue;
}

static aimVariant_t *CreateAimRoute(gentity_t *ent,
	float *playerSpawnSpeed,
	float *playerSpawnYaw,
	float *playerVelocityYaw,
	float *playerSpawnX,
	float *playerSpawnY,
	float *playerSpawnZ) {
	assert(ent);

	// each parameter can be specified manually, or can be omitted to use a default value
	static aimVariant_t var = { 0 };
	memset(&var, 0, sizeof(var));
	var.playerSpawnSpeed = playerSpawnSpeed ? *playerSpawnSpeed : VectorLength(ent->client->trail[0].velocity);
	var.playerSpawnYaw = playerSpawnYaw ? *playerSpawnYaw : ent->client->trail[0].angles[YAW];
	var.playerVelocityYaw = playerVelocityYaw ? *playerVelocityYaw : ent->client->trail[0].angles[YAW];

	var.trailHead = ent->client->trailHead;
	int trailSize = sizeof(aimPracticeMovementTrail_t) * var.trailHead;
	assert(trailSize <= sizeof(aimPracticeMovementTrail_t) * MAX_MOVEMENT_TRAILS);
	memcpy(var.trail, ent->client->trail, trailSize);

	// initialize the player spawnpoint a little ways away from the NPC
	VectorCopy(var.trail[0].currentOrigin, var.playerSpawnPoint);
	vec3_t vec, fwd;
	vec[0] = 0;
	vec[1] = fmod(var.playerSpawnYaw + 180, 360);
	vec[2] = 0;
	AngleVectors(vec, fwd, NULL, NULL);
	// trace to make sure we don't spawn in a wall
	vec3_t traceto, mins, maxs;
	VectorMA(var.playerSpawnPoint, ent->client->pers.aimPracticePackBeingEdited->autoDist, fwd, traceto);
	VectorSet(mins, -15, -15, DEFAULT_MINS_2);
	VectorSet(maxs, 15, 15, DEFAULT_MAXS_2);
	trace_t tr;
	trap_Trace(&tr, var.playerSpawnPoint, mins, maxs, traceto, ent ? ent - g_entities : ENTITYNUM_NONE, MASK_PLAYERSOLID);
	VectorCopy(tr.endpos, var.playerSpawnPoint);

	// allow overriding the location on a per-axis basis
	if (playerSpawnX)
		var.playerSpawnPoint[0] = *playerSpawnX;
	if (playerSpawnY)
		var.playerSpawnPoint[1] = *playerSpawnY;
	if (playerSpawnZ)
		var.playerSpawnPoint[2] = *playerSpawnZ;

	return &var;
}

static qboolean AddRouteToPack(gentity_t *ent, aimPracticePack_t *pack, aimVariant_t *variant) {
	if (!pack || !variant || !ent)
		return qfalse;
	if (pack->numVariants + 1 > MAX_VARIANTS_PER_PACK) {
		PrintIngame(ent - g_entities, "Unable to add route. The maximum number of routes for this pack has been reached.\n");
		return qfalse;
	}

	memcpy(&pack->variants[pack->numVariants++], variant, sizeof(aimVariant_t));

	if (!pack->ent)
		SpawnAimPracticePackNPC(ent, pack);
	else
		RandomizeAndRestartPack(pack);

	pack->changed = qtrue;

	return qtrue;
}

static void DeleteRouteFromPack(gentity_t *ent, aimPracticePack_t *pack, int index) {
	assert(ent && pack && index >= 0 && index < MAX_VARIANTS_PER_PACK && pack->numVariants > 0);

	// delete it
	memset(&pack->variants[index], 0, sizeof(aimVariant_t));
	--pack->numVariants;

	// shift the remaining ones to fill its place, if applicable
	for (int i = index; i < MAX_VARIANTS_PER_PACK - 1; i++) {
		memcpy(&pack->variants[i], &pack->variants[i + 1], sizeof(aimVariant_t)); // copy the one above this into this
		memset(&pack->variants[i + 1], 0, sizeof(aimVariant_t)); // clear the one above this
	}

	pack->changed = qtrue;

	RandomizeAndRestartPack(pack);
}

static void DeleteRoute(gentity_t *ent, aimPracticePack_t *pack, int index) {
	assert(ent && ent->client && ent->client->account && pack && index >= 0 && index < MAX_VARIANTS_PER_PACK && index < pack->numVariants);

	PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s deleted route %d from pack %s.\n", ent->client->account->name, index, pack->packName));
	if (pack->numVariants - 1 > 0 && index != pack->numVariants - 1) // deleting something other than the last one
		PrintIngame(ent - g_entities, "Other routes may have changed indexes as a result; check ^5pack status^7.\n");

	DeleteRouteFromPack(ent, pack, index);

	if (pack->numVariants <= 0 && pack->save) { // deleted the last route and it was meant to be saved
		PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("The last route for pack %s has been deleted, so it will no longer be saved.\n", pack->packName));
		pack->save = qfalse;
	}
}

static aimPracticePack_t *GetPackOnCurrentMapFromMemory(const char *packName) {
	if (!level.aimPracticePackList)
		return NULL;

	iterator_t iter;
	ListIterate(level.aimPracticePackList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		aimPracticePack_t *pack = (aimPracticePack_t *)IteratorNext(&iter);
		if (!Q_stricmp(pack->packName, packName))
			return pack;
	}
	return NULL;
}

static void NewPack(gentity_t *ent, const char *packName) {
	assert(ent && ent->client && ent->client->account && VALIDSTRING(packName));

	if (strchr(packName, ' ') || strchr(packName, '\t') || strchr(packName, '\n') || strchr(packName, '\r')) {
		PrintIngame(ent - g_entities, "Spaces cannot be used in pack names. Try using underscore instead.\n");
		return;
	}

	aimPracticePack_t *pack = GetPackOnCurrentMapFromMemory(packName);
	if (pack) {
		if (pack->ownerAccountId == ent->client->account->id)
			PrintIngame(ent - g_entities, "%s^7 already exists. Use ^5pack select %s^7 if you want to select it.\n", pack->packName, pack->packName);
		else
			PrintIngame(ent - g_entities, "%s^7 already exists. Please choose another name.\n", pack->packName);
		return;
	}

	pack = ListAdd(level.aimPracticePackList, sizeof(aimPracticePack_t));
	ent->client->pers.aimPracticePackBeingEdited = pack;
	ent->client->ps.powerups[PW_REDFLAG] = ent->client->ps.powerups[PW_BLUEFLAG] = 0;
	char *filtered = strdup(packName);
	Q_CleanStr(filtered);
	PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s created new pack ^5%s^7.\n", ent->client->account->name, filtered));
	Q_strncpyz(pack->packName, filtered, sizeof(pack->packName));
	free(filtered);
	pack->ownerAccountId = ent->client->account->id;
	pack->isTemp = qtrue;
	
	// fill it with some default values
	pack->numRepsPerVariant = 2; // idk

	// try to guess the weapons they want from the pack name
	pack->weaponMode = 0;
	if (stristr(pack->packName, "pistol"))
		pack->weaponMode |= (1 << WP_BRYAR_PISTOL);
	if (stristr(pack->packName, "snip") || stristr(pack->packName, "disrupt"))
		pack->weaponMode |= (1 << WP_DISRUPTOR);
	if (stristr(pack->packName, "repeater"))
		pack->weaponMode |= (1 << WP_REPEATER);
	if (stristr(pack->packName, "golan") || stristr(pack->packName, "flechette"))
		pack->weaponMode |= (1 << WP_FLECHETTE);
	if (stristr(pack->packName, "rocket"))
		pack->weaponMode |= (1 << WP_ROCKET_LAUNCHER);
	if (stristr(pack->packName, "thermal") || stristr(pack->packName, "det"))
		pack->weaponMode |= (1 << WP_THERMAL);
	if (stristr(pack->packName, "conc"))
		pack->weaponMode |= (1 << WP_CONCUSSION);
	
	if (!pack->weaponMode) {
		pack->weaponMode = (1 << WP_BRYAR_PISTOL) | (1 << WP_REPEATER) | (1 << WP_FLECHETTE) | (1 << WP_ROCKET_LAUNCHER) | (1 << WP_THERMAL);
		if (level.mapHasConcussionRifle)
			pack->weaponMode |= (1 << WP_CONCUSSION);
	}
	pack->autoDist = DEFAULT_AUTO_DISTANCE_FROM_BOT;

	PrintIngame(ent - g_entities, "You are now editing the new pack %s. Using %d reps per route, autodist %0.2f, weapons %s^7\n",
		pack->packName, pack->numRepsPerVariant, pack->autoDist, WeaponModeStringFromWeaponMode(pack->weaponMode));
}

static void EditPack(gentity_t *ent, const char *packName) {
	assert(ent && ent->client && ent->client->account && VALIDSTRING(packName));
	if (!Q_stricmp(packName, "none")) {
		ent->client->pers.aimPracticePackBeingEdited = NULL;
		PrintIngame(ent - g_entities, "You are no longer editing any pack.\n");
		return;
	}

	aimPracticePack_t *pack = GetPackOnCurrentMapFromMemory(packName);
	if (pack) {
		if (pack->ownerAccountId == ent->client->account->id || (ent->client->account->flags & ACCOUNTFLAG_AIMPACKADMIN)) {
			ent->client->pers.aimPracticePackBeingEdited = pack;
			PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s is now editing pack %s^7.\n", ent->client->account->name, pack->packName));
			if (!pack->isTemp)
				PrintIngame(ent - g_entities, "^3Warning: saving any modifications to this pack will cause any and all existing records on it to be deleted.^7\n");
			ent->client->ps.powerups[PW_REDFLAG] = ent->client->ps.powerups[PW_BLUEFLAG] = 0;
		}
		else {
			PrintIngame(ent - g_entities, "You are not the owner of %s^7.\n", pack->packName);
		}
	}
	else {
		PrintIngame(ent - g_entities, "No pack with the name '%s^7' could be found. Use ^5pack new %s^7 if you want to create it.\n", packName, packName);
	}
}

static aimVariant_t *baseVariantPtr = NULL;
const char *TableCallback_RouteNumber(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimVariant_t *var = context;
	if (!var) {
		assert(qfalse);
		return NULL;
	}
	int index = var - baseVariantPtr;
	return va("%d", index);
}

const char *TableCallback_RouteLength(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimVariant_t *var = context;
	if (!var) {
		assert(qfalse);
		return NULL;
	}
	int totalMilliseconds = 0;
	for (int i = 0; i < var->trailHead; i++) {
		int timeDiff;
		if (i > 0)
			timeDiff = var->trail[i].time - var->trail[i - 1].time;
		else
			timeDiff = var->trail[i].time;
		totalMilliseconds += timeDiff;
	}
	return va("%0.1fs", ((float)totalMilliseconds) / 1000.0f);
}

const char *TableCallback_RoutePlayerSpawnPoint(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimVariant_t *var = context;
	if (!var) {
		assert(qfalse);
		return NULL;
	}
	return va("%0.2f, %0.2f, %0.2f", var->playerSpawnPoint[0], var->playerSpawnPoint[1], var->playerSpawnPoint[2]);
}

const char *TableCallback_RouteBotSpawnPoint(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimVariant_t *var = context;
	if (!var) {
		assert(qfalse);
		return NULL;
	}
	return va("%0.2f, %0.2f, %0.2f", var->trail[0].currentOrigin[0], var->trail[0].currentOrigin[1], var->trail[0].currentOrigin[2]);
}

const char *TableCallback_RouteDistanceBetweenSpawns(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimVariant_t *var = context;
	if (!var) {
		assert(qfalse);
		return NULL;
	}
	vec3_t v;
	VectorSubtract(var->trail[0].currentOrigin, var->playerSpawnPoint, v);
	float dist = VectorLength(v);
	return va("%d units", (int)round(dist));
}

const char *TableCallback_RouteBotSpawnYaw(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimVariant_t *var = context;
	if (!var) {
		assert(qfalse);
		return NULL;
	}
	return va("%0.2f degrees", var->trail[0].angles[YAW]);
}

const char *TableCallback_RouteBotAverageSpeed(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimVariant_t *var = context;
	if (!var) {
		assert(qfalse);
		return NULL;
	}
	float totalVelocity = 0;
	for (int i = 0; i < var->trailHead; i++) {
		float thisVelocity = VectorLength(var->trail[i].velocity);
		totalVelocity += thisVelocity;
	}
	float averageVelocity = totalVelocity / (var->trailHead ? var->trailHead : 1); // sanity check; avoid divide by zero
	return va("%d ups", (int)round(averageVelocity));
}

const char *TableCallback_RoutePlayerSpawnYaw(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimVariant_t *var = context;
	if (!var) {
		assert(qfalse);
		return NULL;
	}
	return va("%0.2f degrees", var->playerSpawnYaw);
}

const char *TableCallback_RoutePlayerVelocityYaw(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimVariant_t *var = context;
	if (!var) {
		assert(qfalse);
		return NULL;
	}
	return va("%0.2f degrees", var->playerVelocityYaw);
}

const char *TableCallback_RoutePlayerSpawnSpeed(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimVariant_t *var = context;
	if (!var) {
		assert(qfalse);
		return NULL;
	}
	return va("%d ups", (int)round(var->playerSpawnSpeed));
}

static char *AimWeaponNameFromInteger(weapon_t w) {
	switch (w) {
	case WP_BRYAR_PISTOL: return "Pistol";
	case WP_DISRUPTOR: return "Disruptor";
	case WP_REPEATER: return "Repeater";
	case WP_FLECHETTE: return "Golan";
	case WP_ROCKET_LAUNCHER: return "Rocket";
	case WP_THERMAL: return "Thermal";
	case WP_CONCUSSION: return "Conc";
	default: return NULL; // we don't care about other weapons
	}
}

static weapon_t AimWeaponNumberFromName(const char *s) {
	if (!VALIDSTRING(s))
		return WP_NONE;

	if (!Q_stricmpn(s, "pi", 2) || !Q_stricmpn(s, "br", 2))
		return WP_BRYAR_PISTOL;

	if (!Q_stricmpn(s, "di", 2) || !Q_stricmpn(s, "sn", 2) || !Q_stricmpn(s, "ru", 2))
		return WP_DISRUPTOR;

	if (!Q_stricmpn(s, "re", 2))
		return WP_REPEATER;

	if (!Q_stricmpn(s, "go", 2) || !Q_stricmpn(s, "fl", 2))
		return WP_FLECHETTE;

	if (!Q_stricmpn(s, "ro", 2))
		return WP_ROCKET_LAUNCHER;

	if (!Q_stricmpn(s, "th", 2) || !Q_stricmpn(s, "de", 2))
		return WP_THERMAL;

	if (!Q_stricmpn(s, "co", 2))
		return WP_CONCUSSION;

	return WP_NONE;
}

char *WeaponModeStringFromWeaponMode(int mode) {
	char buf[1024] = { 0 };

	for (weapon_t w = WP_NONE; w < WP_NUM_WEAPONS; w++) {
		char *s = AimWeaponNameFromInteger(w);
		if (!VALIDSTRING(s))
			continue;
		if (!(mode & (1 << w)))
			continue;
		Q_strcat(buf, sizeof(buf), s);
	}

	return va("%s", buf);
}

static int NumAllowedAimWeapons(int mode) {
	int numGotten = 0;
	for (weapon_t w = WP_NONE; w < WP_NUM_WEAPONS; w++) {
		char *s = AimWeaponNameFromInteger(w);
		if (!VALIDSTRING(s))
			continue;
		if (!(mode & (1 << w)))
			continue;
		++numGotten;
	}
	return numGotten;
}

static char *PackStatusString(const aimPracticePack_t *pack) {
	assert(pack);
	if (pack->isTemp) {
		if (pack->save)
			return "^5Unsaved, but will be saved on map change/restart";
		else
			return "^8Unsaved; will be discarded on map change/restart";
	}
	else {
		if (pack->deleteMe)
			return "^1Saved in DB, but will be permanently deleted on map change/restart";
		else if (pack->changed && pack->save)
			return "^5Saved in DB (changes will be saved on map change/restart)";
		else if (pack->changed)
			return "^3Saved in DB (but may have new unsaved changes, which will be discarded on map change/restart)";
		else
			return "^2Saved in DB + unchanged";
	}
}

static void PackStatus(gentity_t *ent, aimPracticePack_t *pack) {
	assert(ent && ent->client && pack);
	PrintIngame(ent - g_entities, "Information for pack ^5%s^7:\n", pack->packName);
	account_t account = { 0 };
	G_DBGetAccountByID(pack->ownerAccountId, &account);
	PrintIngame(ent - g_entities, "Owner: %s\n", account.name);
	PrintIngame(ent - g_entities, "Status: %s\n", PackStatusString(pack));
	PrintIngame(ent - g_entities, "Weapons: %s\n", WeaponModeStringFromWeaponMode(pack->weaponMode));

	if (pack->numVariants <= 0) {
		PrintIngame(ent - g_entities, "No routes have been added to this pack yet.%s\n",
			ent->client->pers.aimPracticePackBeingEdited == pack ? " To add a route, use the ^5pack route record^7 command." : "");
		return;
	}

	int numWeapons = NumAllowedAimWeapons(pack->weaponMode);
	int spawnsPerWeapon = pack->numVariants * pack->numRepsPerVariant;
	int totalSpawns = spawnsPerWeapon * numWeapons;
	int totalMilliseconds = 0;
	float totalVelocity = 0;
	int totalTrails = 0;
	for (int i = 0; i < pack->numVariants; i++) {
		for (int j = 0; j < pack->variants[i].trailHead; j++) {
			int timeDiff;
			if (j > 0)
				timeDiff = pack->variants[i].trail[j].time - pack->variants[i].trail[j - 1].time;
			else
				timeDiff = pack->variants[i].trail[j].time;
			totalMilliseconds += timeDiff * pack->numRepsPerVariant * numWeapons;
			totalVelocity += VectorLength(pack->variants[i].trail[j].velocity);
			++totalTrails;
		}
	}
	int m, s, ms;
	char lengthString[32] = { 0 };
	if (totalMilliseconds < 60000) { // seconds
		m = 0;
		s = totalMilliseconds / 1000;
		ms = totalMilliseconds - (1000 * s);
		Com_sprintf(lengthString, sizeof(lengthString), "%d seconds", s);
	}
	else { // minutes:seconds
		m = totalMilliseconds / 60000;
		s = (totalMilliseconds % 60000) / 1000;
		ms = (totalMilliseconds % 60000) - (1000 * s);
		Com_sprintf(lengthString, sizeof(lengthString), "%dm%ds", m, s);
	}
	float averageVelocity = totalVelocity / (totalTrails ? totalTrails : 1); // sanity check; prevent divide by zero
	PrintIngame(ent - g_entities, "Average bot speed: %d ups\n", (int)round(averageVelocity));
	PrintIngame(ent - g_entities, "%d routes x %d repetitions per route == %d spawns per weapon x %d weapons == %d total spawns\nComplete run length: %s\n\n", pack->numVariants, pack->numRepsPerVariant, spawnsPerWeapon, numWeapons, totalSpawns, lengthString);

	Table *t = Table_Initialize(qtrue);
	for (int i = 0; i < pack->numVariants && i < MAX_VARIANTS_PER_PACK; i++) {
		Table_DefineRow(t, &pack->variants[i]);
	}

	baseVariantPtr = &pack->variants[0]; // terrible hack, need to port columnContext to this branch
	Table_DefineColumn(t, "Route #", TableCallback_RouteNumber, qfalse, 64);
	Table_DefineColumn(t, "Length", TableCallback_RouteLength, qfalse, 64);
	Table_DefineColumn(t, "Bot spawn", TableCallback_RouteBotSpawnPoint, qfalse, 64);
	Table_DefineColumn(t, "Bot spawn yaw", TableCallback_RouteBotSpawnYaw, qfalse, 64);
	Table_DefineColumn(t, "Bot avg speed", TableCallback_RouteBotAverageSpeed, qfalse, 64);
	Table_DefineColumn(t, "Player spawn", TableCallback_RoutePlayerSpawnPoint, qfalse, 64);
	Table_DefineColumn(t, "Dist between spawns", TableCallback_RouteDistanceBetweenSpawns, qfalse, 64);
	Table_DefineColumn(t, "Player spawn yaw", TableCallback_RoutePlayerSpawnYaw, qfalse, 64);
	Table_DefineColumn(t, "Player spawn speed", TableCallback_RoutePlayerSpawnSpeed, qfalse, 64);
	Table_DefineColumn(t, "Player velocity yaw", TableCallback_RoutePlayerVelocityYaw, qfalse, 64);
	baseVariantPtr = NULL;

	char buf[4096] = { 0 };
	Table_WriteToBuffer(t, buf, sizeof(buf));
	Table_Destroy(t);

	PrintIngame(ent - g_entities, buf);
	if (ent->client->pers.aimPracticePackBeingEdited == pack) {
		PrintIngame(ent - g_entities, "\nTo record a route, use the ^5pack route record^7 command.\n"
			"To delete a route, use the ^5pack route delete^7 command.\n");
	}
}

static void PackToggleWeapon(gentity_t *ent, weapon_t weap, qboolean enable) {
	assert(ent && ent->client && ent->client->pers.aimPracticePackBeingEdited);
	aimPracticePack_t *pack = ent->client->pers.aimPracticePackBeingEdited;

	if (enable) {
		pack->weaponMode |= (1 << weap);
	}
	else {
		pack->weaponMode &= ~(1 << weap);
		if (!pack->weaponMode) {
			PrintIngame(ent - g_entities, "Pack must have at least one weapon.\n");
			pack->weaponMode |= (1 << weap);
			return;
		}
	}

	pack->changed = qtrue;

	char *weapName = AimWeaponNameFromInteger(weap);
	if (VALIDSTRING(weapName)) {
		char *lowercase = strdup(weapName);
		Q_strlwr(lowercase);
		PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s %s %s %s %s pack - now using %s.\n",
			ent->client->account->name,
			enable ? "added" : "removed",
			lowercase,
			enable ? "to" : "from",
			pack->packName, WeaponModeStringFromWeaponMode(pack->weaponMode)));
		free(lowercase);
	}

	RandomizeAndRestartPack(pack);
}

static void PackReps(gentity_t *ent, int reps) {
	assert(ent && ent->client && ent->client->pers.aimPracticePackBeingEdited && reps > 0 && reps <= MAX_REPS_PER_ROUTE);
	aimPracticePack_t *pack = ent->client->pers.aimPracticePackBeingEdited;
	pack->numRepsPerVariant = reps;
	pack->changed = qtrue;

	PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s changed pack %s to use %d repetitions per route.\n",
		ent->client->account->name, pack->packName, reps));

	RandomizeAndRestartPack(pack);
}

static void PackAutoDistance(gentity_t *ent, float dist) {
	assert(ent && ent->client && ent->client->pers.aimPracticePackBeingEdited);
	aimPracticePack_t *pack = ent->client->pers.aimPracticePackBeingEdited;
	pack->autoDist = dist;
	PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s set auto distance for pack %s to %0.2f units (nothing existing has actually changed).\n", ent->client->account->name, ent->client->pers.aimPracticePackBeingEdited, dist));
	PrintIngame(ent - g_entities, "Recording new routes without specifying a player spawnpoint for pack %s will now use an automatic distance of %0.2f units.\n",
		ent->client->pers.aimPracticePackBeingEdited->packName, dist);
}

const char *TableCallback_PackName(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimPracticePack_t *pack = context;
	if (!pack) {
		assert(qfalse);
		return NULL;
	}
	return pack->packName;
}

const char *TableCallback_PackOwner(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimPracticePack_t *pack = context;
	if (!pack) {
		assert(qfalse);
		return NULL;
	}
	account_t account = { 0 };
	G_DBGetAccountByID(pack->ownerAccountId, &account);
	return va("%s", account.name);
}

const char *TableCallback_PackWeaponMode(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimPracticePack_t *pack = context;
	if (!pack) {
		assert(qfalse);
		return NULL;
	}
	return WeaponModeStringFromWeaponMode(pack->weaponMode);
}

const char *TableCallback_PackNumberOfRoutes(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimPracticePack_t *pack = context;
	if (!pack) {
		assert(qfalse);
		return NULL;
	}
	return va("%d", pack->numVariants);
}

const char *TableCallback_PackStatus(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimPracticePack_t *pack = context;
	if (!pack) {
		assert(qfalse);
		return NULL;
	}
	return PackStatusString(pack);
}

const char *TableCallback_PackCanHaveNewRecords(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimPracticePack_t *pack = context;
	if (!pack) {
		assert(qfalse);
		return NULL;
	}

	if (pack->isTemp)
		return "^1No ^7(unsaved)";
	else if (pack->changed)
		return "^1No ^7(has changed)";
	else
		return "^2Yes";
}

static void ListPacks(gentity_t *ent) {
	assert(ent);
	qboolean gotOne = qfalse;
	Table *t = Table_Initialize(qtrue);

	iterator_t iter;
	ListIterate(level.aimPracticePackList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		aimPracticePack_t *pack = (aimPracticePack_t *)IteratorNext(&iter);
		Table_DefineRow(t, pack);
		gotOne = qtrue;
	}

	Table_DefineColumn(t, "Pack name", TableCallback_PackName, qfalse, 64);
	Table_DefineColumn(t, "Owner", TableCallback_PackOwner, qfalse, 64);
	Table_DefineColumn(t, "Weapons", TableCallback_PackWeaponMode, qfalse, 64);
	Table_DefineColumn(t, "# of routes", TableCallback_PackNumberOfRoutes, qfalse, 64);
	Table_DefineColumn(t, "Status", TableCallback_PackStatus, qfalse, 64);
	Table_DefineColumn(t, "New records can be set", TableCallback_PackCanHaveNewRecords, qfalse, 64);

	char buf[2048] = { 0 };
	Table_WriteToBuffer(t, buf, sizeof(buf));
	Table_Destroy(t);

	if (gotOne) {
		PrintIngame(ent - g_entities, "Current aim practice packs on %s:\n", level.mapname);
		PrintIngame(ent - g_entities, buf);
		PrintIngame(ent - g_entities, "For more information about a pack, use ^5pack status [name]^7\n");
	}
	else {
		PrintIngame(ent - g_entities, "There are currently no aim practice packs on %s.\n", level.mapname);
	}
}

static void SavePack(gentity_t *ent, aimPracticePack_t *pack) {
	assert(ent && ent->client && ent->client->account && pack);
	pack->deleteMe = qfalse;
	if (pack->save) {
		pack->save = qfalse;
		PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s cancelled the pending save of pack %s.\n",
			ent->client->account->name, pack->packName));
	}
	else {
		if (pack->numVariants <= 0) {
			PrintIngame(ent - g_entities, "Pack %s has no routes; it cannot be saved.\n", pack->packName);
			return;
		}

		pack->save = qtrue;
		PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s marked pack %s to be saved on map restart/change.\n",
			ent->client->account->name, pack->packName));
		PrintIngame(ent - g_entities, "To cancel saving, enter this command again.\n");

		if (!pack->isTemp)
			PrintIngame(ent - g_entities, "^3Warning: saving any modifications to this pack will cause any and all existing records on it to be deleted.^7\n");
	}
}

static void DeletePack(gentity_t *ent, aimPracticePack_t *pack) {
	assert(ent && ent->client && pack);
	if (pack->deleteMe) { // this was going to be deleted, but the owner changed their mind and entered the command again
		PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s cancelled the pending deletion of pack %s. It will remain in the DB and will respawn on map restart/change.\n", ent->client->account->name, pack->packName));
		pack->deleteMe = qfalse;
	}
	else {
		if (pack->isTemp) { // unsaved temp packs just get totally deleted immediately
			PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s deleted unsaved pack %s.\n", ent->client->account->name, pack->packName));
			if (pack->ent) {
				for (int i = 0; i < MAX_CLIENTS; i++) {
					gentity_t *thisEnt = &g_entities[i];
					if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.aimPracticePackBeingEdited != pack)
						continue;
					thisEnt->client->pers.aimPracticePackBeingEdited = NULL;
					thisEnt->client->moveRecordStart = 0;
					thisEnt->client->trailHead = -1;
					memset(thisEnt->client->trail, 0, sizeof(thisEnt->client->trail));
					thisEnt->client->reachedLimit = qfalse;
				}
				DeletePackNPC(pack);
			}
			ListRemove(level.aimPracticePackList, pack);
		}
		else { // try to prevent accidental deletion of saved packs
			PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s removed DB-saved pack %s. It will be ^1PERMANENTLY DELETED^7 on map restart/change. Any and all existing records on it will be deleted.\n", ent->client->account->name, pack->packName));
			PrintIngame(ent - g_entities, "Deletion can be cancelled by entering this command again.\n");
			pack->deleteMe = qtrue;
		}
	}
}

#define PassBotParam(x) (x != -1 ? &x : NULL)
static void RecordRoute(gentity_t *ent) {
	if (!ent->client->pers.aimPracticePackBeingEdited) {
		PrintIngame(ent - g_entities, "You must first select a pack with the ^5pack new^7 or ^5pack edit^7.\n");
		return;
	}
	if (ent->client->ps.fallingToDeath || ent->health <= 0 || !ent->client->sess.inRacemode)
		return;

	float playerSpawnSpeed = -1, playerSpawnYaw = -1, playerVelocityYaw = -1, playerSpawnX = -1, playerSpawnY = -1, playerSpawnZ = -1;
	char model[MAX_QPATH] = { 0 };
	int args = sscanf(ConcatArgs(3), "%f %f %f %f %f %f", &playerSpawnSpeed, &playerSpawnYaw, &playerVelocityYaw, &playerSpawnX, &playerSpawnY, &playerSpawnZ);

	if (!args || trap_Argc() < 4) {
		PrintIngame(ent - g_entities, "Usage: ^5pack route record <player spawn speed> <player spawn yaw> <player velocity yaw> <player spawn X> <player spawn Y> <player spawn Z>^7\n"
			"All arguments are optional. Use ^5-1^7 to omit an argument.\n"
			"If unspecified, speed and yaw default to copying the bot's initial speed and yaw.\n"
			"If unspecified, spawn position defaults to spawning a set distance from the bot, configured using ^5pack autodist^7 (currently %0.2f).\n"
			"To skip this message, enter at least one argument, e.g. ^5pack route record -1^7.\n", ent->client->pers.aimPracticePackBeingEdited->autoDist
		);
		return;
	}

	ExitAimTraining(ent);

	if (ent->client->moveRecordStart) {
		if (ent->client->trailHead < g_svfps.integer * 2) { // require at least two seconds, since autospeed makes you unable to use force for 1.5 seconds
			PrintIngame(ent - g_entities, "Recording is too short. Please record for at least two seconds.\n");
		}
		else {
			aimVariant_t *var = CreateAimRoute(ent,
				PassBotParam(playerSpawnSpeed),
				PassBotParam(playerSpawnYaw),
				PassBotParam(playerVelocityYaw),
				PassBotParam(playerSpawnX),
				PassBotParam(playerSpawnY),
				PassBotParam(playerSpawnZ)
			);
			qboolean success = AddRouteToPack(ent, ent->client->pers.aimPracticePackBeingEdited, var);
			if (success)
				PrintBasedOnAccountFlags(ACCOUNTFLAG_AIMPACKEDITOR, va("%s added a route to pack %s.\n", ent->client->account->name, ent->client->pers.aimPracticePackBeingEdited->packName));
			else
				PrintIngame(ent - g_entities, "Failed to add route to pack %s!\n", ent->client->pers.aimPracticePackBeingEdited->packName);
		}
		ent->client->moveRecordStart = 0;
	}
	else {
		ent->client->moveRecordStart = trap_Milliseconds();
		PrintIngame(ent - g_entities, "Started recording movement.\n");
	}

	// reset them in any case
	ent->client->trailHead = -1;
	memset(ent->client->trail, 0, sizeof(ent->client->trail));
	ent->client->reachedLimit = qfalse;
}

void Cmd_Pack_f(gentity_t *ent) {
	if (!ent || !ent->client || g_gametype.integer != GT_CTF)
		return;
	if (!ent->client->account || (!(ent->client->account->flags & ACCOUNTFLAG_AIMPACKEDITOR) && !(ent->client->account->flags & ACCOUNTFLAG_AIMPACKADMIN))) {
		PrintIngame(ent - g_entities, "You are not allowed to use this command.\n");
		return;
	}
	if (!level.topAimRecordsEnabled) {
		PrintIngame(ent - g_entities, "This feature is disabled on this server.\n");
		return;
	}
	if (level.racemodeRecordsReadonly) {
		PrintIngame(ent - g_entities, "This command is not available while racemode records are in a read-only state.\n");
		return;
	}
	if (trap_Argc() < 2) {
		PrintIngame(ent - g_entities, "^7Usage: pack [status | list | new | edit | delete | route | weapon | reps | autodist]\n"
			"  ^9status - print info about the pack your selected pack, or a particular pack\n"
			"    ^7list - list current packs for this map\n"
			"     ^9new - create a new pack and edit it\n"
			"    ^7edit - edit an existing pack\n"
			"  ^9delete - delete an existing pack\n"
			"\n"
			"   ^7route - record or delete routes for your selected pack\n"
			"  ^9weapon - add or remove a weapon from your selected pack\n"
			"    ^7reps - change how many times each route must be run per weapon for your selected pack\n"
			"^9autodist - change how far to automatically place a player spawn from the bot spawn if player spawn coordinates are not specified in record arguments for your selected pack (default: %d)^7\n", DEFAULT_AUTO_DISTANCE_FROM_BOT);
		return;
	}
	char arg1[MAX_STRING_CHARS] = { 0 }, arg2[MAX_STRING_CHARS] = { 0 };
	trap_Argv(1, arg1, sizeof(arg1));
	trap_Argv(2, arg2, sizeof(arg2));
	if (!arg1[0]) {
		PrintIngame(ent - g_entities, "^7Usage: pack [status | list | new | edit | delete | route | weapon | reps | autodist]\n"
			"  ^9status - print info about the pack your selected pack, or a particular pack\n"
			"    ^7list - list current packs for this map\n"
			"     ^9new - create a new pack and edit it\n"
			"    ^7edit - edit an existing pack\n"
			"  ^9delete - delete an existing pack\n"
			"\n"
			"   ^7route - record or delete routes for your selected pack\n"
			"  ^9weapon - add or remove a weapon from your selected pack\n"
			"    ^7reps - change how many times each route must be run per weapon for your selected pack\n"
			"^9autodist - change how far to automatically place a player spawn from the bot spawn if player spawn coordinates are not specified in record arguments for your selected pack (default: %d)^7\n", DEFAULT_AUTO_DISTANCE_FROM_BOT);
		return;
	}

	if (!Q_stricmp(arg1, "new") || !Q_stricmp(arg1, "create")) {
		if (!arg2[0]) {
			PrintIngame(ent - g_entities, "Usage: pack new [name]\n");
			return;
		}
		if (!ent->client->sess.inRacemode) {
			PrintIngame(ent - g_entities, "This command can only be used in racemode.\n");
			return;
		}
		NewPack(ent, arg2);
	}
	else if (!Q_stricmp(arg1, "route")) {
		if (!ent->client->pers.aimPracticePackBeingEdited) {
			PrintIngame(ent - g_entities, "You must first select a pack (with ^5pack new^7 or ^5pack edit^7).\n");
			return;
		}
		if (trap_Argc() < 3) {
			PrintIngame(ent - g_entities, "Usage: pack route [record | delete | cancel]\n");
			return;
		}
		if (!Q_stricmpn(arg2, "rec", 3)) {
			if (!ent->client->sess.inRacemode) {
				PrintIngame(ent - g_entities, "This command can only be used in racemode.\n");
				return;
			}
			RecordRoute(ent);
		}
		else if (!Q_stricmpn(arg2, "del", 3)) {
			if (trap_Argc() < 4) {
				PrintIngame(ent - g_entities, "Usage: pack route delete [index]   Get index with the ^5pack status^7 command.\n");
				return;
			}
			char arg3[MAX_STRING_CHARS] = { 0 };
			trap_Argv(3, arg3, sizeof(arg3));
			int index = atoi(arg3);
			if (!arg3[0] || index < 0 || index >= MAX_VARIANTS_PER_PACK || index >= ent->client->pers.aimPracticePackBeingEdited->numVariants) {
				PrintIngame(ent - g_entities, "Usage: pack route delete [index]   Get index with the ^5pack status^7 command.\n");
				return;
			}
			DeleteRoute(ent, ent->client->pers.aimPracticePackBeingEdited, index);
		}
		else if (!Q_stricmpn(arg2, "sto", 3) || !Q_stricmpn(arg2, "can", 3)) {
			if (!ent->client->moveRecordStart)
				return;
			ent->client->moveRecordStart = 0;
			ent->client->trailHead = -1;
			memset(ent->client->trail, 0, sizeof(ent->client->trail));
			ent->client->reachedLimit = qfalse;
			PrintIngame(ent - g_entities, "Cancelled recording; the current recording has been discarded.\n");
		}
		else {
			PrintIngame(ent - g_entities, "Usage: pack route [record | delete]\n");
		}
	}
	else if (!Q_stricmpn(arg1, "del", 3)) {
		if (!arg2[0] && !ent->client->pers.aimPracticePackBeingEdited) {
			PrintIngame(ent - g_entities, "You must either select a pack (with ^5pack new^7 or ^5pack edit^7) or specify a pack name.\n");
			return;
		}
		if (arg2[0]) {
			aimPracticePack_t *pack = GetPackOnCurrentMapFromMemory(arg2);
			if (pack) {
				if (pack->ownerAccountId == ent->client->account->id || (ent->client->account->flags & ACCOUNTFLAG_AIMPACKADMIN))
					DeletePack(ent, pack);
				else
					PrintIngame(ent - g_entities, "You are not the owner of pack %s.\n", pack->packName);
			}
			else {
				PrintIngame(ent - g_entities, "No pack found matching '%s^7'\n.", arg2);
			}
		}
		else {
			DeletePack(ent, ent->client->pers.aimPracticePackBeingEdited);
		}
	}
	else if (!Q_stricmp(arg1, "edit")) {
		if (!arg2[0]) {
			PrintIngame(ent - g_entities, "Usage: pack edit [name]   (use 'none' to stop editing a pack)\n");
			return;
		}
		if (!ent->client->sess.inRacemode) {
			PrintIngame(ent - g_entities, "This command can only be used in racemode.\n");
			return;
		}
		EditPack(ent, arg2);
	}
	else if (!Q_stricmp(arg1, "list")) {
		ListPacks(ent);
	}
	else if (!Q_stricmpn(arg1, "stat", 4)) {
		if (!arg2[0] && !ent->client->pers.aimPracticePackBeingEdited) {
			PrintIngame(ent - g_entities, "You must either select a pack (with ^5pack new^7 or ^5pack edit^7) or specify a pack name.\n");
			return;
		}
		if (arg2[0]) {
			aimPracticePack_t *pack = GetPackOnCurrentMapFromMemory(arg2);
			if (pack)
				PackStatus(ent, pack);
			else
				PrintIngame(ent - g_entities, "No pack found matching '%s^7'\n.", arg2);
		}
		else {
			PackStatus(ent, ent->client->pers.aimPracticePackBeingEdited);
		}
	}
	else if (!Q_stricmp(arg1, "save")) {
		if (!arg2[0] && !ent->client->pers.aimPracticePackBeingEdited) {
			PrintIngame(ent - g_entities, "You must either select a pack (with ^5pack new^7 or ^5pack edit^7) or specify a pack name.\n");
			return;
		}
		if (arg2[0]) {
			aimPracticePack_t *pack = GetPackOnCurrentMapFromMemory(arg2);
			if (pack) {
				if (pack->ownerAccountId == ent->client->account->id || (ent->client->account->flags & ACCOUNTFLAG_AIMPACKADMIN))
					SavePack(ent, pack);
				else
					PrintIngame(ent - g_entities, "You are not the owner of pack %s.\n", pack->packName);
			}
			else {
				PrintIngame(ent - g_entities, "No pack found matching '%s^7'\n.", arg2);
			}
		}
		else {
			SavePack(ent, ent->client->pers.aimPracticePackBeingEdited);
		}
	}
	else if (!Q_stricmpn(arg1, "weap", 4)) {
		if (!ent->client->pers.aimPracticePackBeingEdited) {
			PrintIngame(ent - g_entities, "You must first select a pack (with ^5pack new^7 or ^5pack edit^7).\n");
			return;
		}
		if (trap_Argc() < 4) {
			PrintIngame(ent - g_entities, "Usage: pack weapon [add | delete] [weapon name]\n");
			return;
		}

		char arg3[MAX_STRING_CHARS] = { 0 };
		trap_Argv(3, arg3, sizeof(arg3));
		weapon_t w = AimWeaponNumberFromName(arg3);

		if (!Q_stricmpn(arg2, "add", 3)) {
			if (!w) {
				PrintIngame(ent - g_entities, "'%s^7' is not a valid weapon.\n", arg3);
				return;
			}
			PackToggleWeapon(ent, w, qtrue);
		}
		else if (!Q_stricmpn(arg2, "del", 3)) {
			if (!w) {
				PrintIngame(ent - g_entities, "'%s^7' is not a valid weapon.\n", arg3);
				return;
			}
			PackToggleWeapon(ent, w, qfalse);
		}
		else {
			PrintIngame(ent - g_entities, "Usage: pack weapon [add | delete] [weapon name]\n");
		}
	}
	else if (!Q_stricmpn(arg1, "rep", 3)) {
		if (!ent->client->pers.aimPracticePackBeingEdited) {
			PrintIngame(ent - g_entities, "You must first select a pack with ^5pack new^7 or ^5pack edit^7.\n");
			return;
		}
		if (!arg2[0] || !Q_isanumber(arg2)) {
			PrintIngame(ent - g_entities, "Usage: pack reps [number of times per weapon to run each route]\n");
			return;
		}
		int reps = atoi(arg2);
		if (reps < 1 || reps > MAX_REPS_PER_ROUTE) {
			PrintIngame(ent - g_entities, "Please enter a valid number between 1 and %d.\n", MAX_REPS_PER_ROUTE);
			return;
		}
		PackReps(ent, reps);
	}
	else if (!Q_stricmpn(arg1, "auto", 4) || !Q_stricmpn(arg1, "dist", 4)) {
		if (!ent->client->pers.aimPracticePackBeingEdited) {
			PrintIngame(ent - g_entities, "You must first select a pack with ^5pack new^7 or ^5pack edit^7.\n");
			return;
		}
		if (!arg2[0] || !Q_isanumber(arg2)) {
			PrintIngame(ent - g_entities, "Usage: pack autodist [distance from bot spawnpoint to player spawnpoint to automatically use if player spawnpoint is unspecified]    (default is %d)\n", DEFAULT_AUTO_DISTANCE_FROM_BOT);
			return;
		}
		float dist = atof(arg2);
		PackAutoDistance(ent, dist);
	}
	else {
		PrintIngame(ent - g_entities, "Usage: pack [status | list | new | edit | delete | route | weapon | reps | autodist]\n");
	}
}

const char CharForWeapon(weapon_t w) {
	switch (w) {
	case WP_BRYAR_PISTOL: return 'p';
	case WP_DISRUPTOR: return 'd';
	case WP_REPEATER: return 'r';
	case WP_FLECHETTE: return 'g';
	case WP_ROCKET_LAUNCHER: return 'r';
	case WP_THERMAL: return 't';
	case WP_CONCUSSION: return 'c';
	default: assert(qfalse); return ' ';
	}
}

static void PrintAimRecordsCallback(void *context, const char *mapname, const char *packName, const int rank, const char *name, const aimRecord_t *record) {
	gentity_t *ent = (gentity_t *)context;

	char nameString[64] = { 0 };
	Q_strncpyz(nameString, name, sizeof(nameString));
	int j;
	for (j = Q_PrintStrlen(nameString); j < MAX_NAME_DISPLAYLENGTH; ++j)
		Q_strcat(nameString, sizeof(nameString), " ");

	char dateString[22];
	time_t now = time(NULL);
	if (now - record->date < 60 * 60 * 24) Q_strncpyz(dateString, "^2", sizeof(dateString));
	else Q_strncpyz(dateString, "^7", sizeof(dateString));
	G_FormatLocalDateFromEpoch(dateString + 2, sizeof(dateString) - 2, record->date);

	char hitsStr[48] = { 0 };
	for (weapon_t w = WP_NONE; w < WP_NUM_WEAPONS; w++) {
		if (!AimWeaponNameFromInteger(w))
			continue;
		if (!(record->extra.numHitsOfWeapon[w]))
			continue;
		Q_strcat(hitsStr, sizeof(hitsStr), va("%s%d %c", hitsStr[0] ? ", " : "", record->extra.numHitsOfWeapon[w], CharForWeapon(w)));
	}

	PrintIngame(ent - g_entities, "^5%3d^7: ^7%s  ^3%-8d ^7%-47s %s^7\n",
		rank, nameString, record->time, hitsStr, dateString);
}

#define NUM_RECORDS_PER_PAGE 10

static void SubCmd_TopAim_Records(gentity_t *ent, const char *mapname, const char *packName, const int page) {
	PrintIngame(ent - g_entities, 
		"Records for the ^3%s ^7pack on ^3%s^7:^5\n"
		"               Name            %-8s %-47s Date^7\n",
		packName, mapname, "Score", "Hits");

	pagination_t pagination;
	pagination.numPerPage = NUM_RECORDS_PER_PAGE;
	pagination.numPage = page;

	G_DBTopAimListAimRecords(mapname, packName, pagination, PrintAimRecordsCallback, ent);

	PrintIngame(ent - g_entities, "^7Viewing page: %d\n", page);
}

static void PrintAimTopCallback(void *context, const char *mapname, const char *packName, const char *name, const int captureTime, const time_t date) {
	gentity_t *ent = (gentity_t *)context;

	char dateString[22];
	time_t now = time(NULL);
	if (now - date < 60 * 60 * 24) Q_strncpyz(dateString, "^2", sizeof(dateString));
	else Q_strncpyz(dateString, "^7", sizeof(dateString));
	G_FormatLocalDateFromEpoch(dateString + 2, sizeof(dateString) - 2, date);

	PrintIngame(ent - g_entities, "^7%-25s  ^7%-8d %s     ^7%s^7\n", mapname, captureTime, dateString, name);
}

#define NUM_TOP_PER_PAGE 15

static void SubCmd_TopAim_Maplist(gentity_t *ent, const char *packName, const int page) {
	PrintIngame(ent - g_entities, "^7Records for the ^3%s ^7pack:^5\n"
		"           Map               Score    Date                 Name\n", packName);

	pagination_t pagination;
	pagination.numPerPage = NUM_TOP_PER_PAGE;
	pagination.numPage = page;

	G_DBTopAimListAimTop(packName, pagination, PrintAimTopCallback, ent);

	PrintIngame(ent - g_entities, "^7Viewing page: %d\n", page);
}

static void PrintAimLeaderboardCallback(void *context, const char *packName, const int rank, const char *name, const int golds, const int silvers, const int bronzes) {
	gentity_t *ent = (gentity_t *)context;

	char nameString[64] = { 0 };
	Q_strncpyz(nameString, name, sizeof(nameString));
	int j;
	for (j = Q_PrintStrlen(nameString); j < MAX_NAME_DISPLAYLENGTH; ++j)
		Q_strcat(nameString, sizeof(nameString), " ");

	PrintIngame(ent - g_entities, "^5%3d^7: ^7%s    ^3%2d        ^9%2d        ^8%2d^7\n",
		rank, nameString, golds, silvers, bronzes);
}

#define NUM_LEADERBOARD_PER_PAGE 10

static void SubCmd_TopAim_Ranks(gentity_t *ent, const char *packName, const int page) {
	PrintIngame(ent - g_entities, "^7Leaderboard for the ^3%s ^7pack:^5\n"
		"               Name             Golds    Silvers   Bronzes\n", packName);

	pagination_t pagination;
	pagination.numPerPage = NUM_LEADERBOARD_PER_PAGE;
	pagination.numPage = page;

	G_DBTopAimListAimLeaderboard(packName, pagination, PrintAimLeaderboardCallback, ent);

	PrintIngame(ent - g_entities, "^7Viewing page: %d\n", page);
}

static void PrintAimLatestCallback(void *context, const char *mapname, const char *packName, const int rank, const char *name, const int captureTime, const time_t date) {
	gentity_t *ent = (gentity_t *)context;

	char nameString[64] = { 0 };
	Q_strncpyz(nameString, name, sizeof(nameString));
	int j;
	for (j = Q_PrintStrlen(nameString); j < MAX_NAME_DISPLAYLENGTH; ++j)
		Q_strcat(nameString, sizeof(nameString), " ");

	// two chars for colors, max 2 for rank, 1 null terminator
	char rankString[5];
	char *color = rank == 1 ? S_COLOR_RED : S_COLOR_WHITE;
	Com_sprintf(rankString, sizeof(rankString), "%s%d", color, rank);



	char dateString[22];
	time_t now = time(NULL);
	if (now - date < 60 * 60 * 24) Q_strncpyz(dateString, S_COLOR_GREEN, sizeof(dateString));
	else Q_strncpyz(dateString, S_COLOR_WHITE, sizeof(dateString));
	G_FormatLocalDateFromEpoch(dateString + 2, sizeof(dateString) - 2, date);

	PrintIngame(ent - g_entities, "^7%s     ^3%-10s %-4s    ^3%-8d %s   ^7%s^7\n",
		nameString, packName, rankString, captureTime, dateString, mapname);
}

#define NUM_LATEST_PER_PAGE 15

static void SubCmd_TopAim_Latest(gentity_t *ent, const int page) {
	PrintIngame(ent - g_entities, "^7Latest records:^5\n"
		"           Name             Pack       Rank     Score    Date                 Map\n");

	pagination_t pagination;
	pagination.numPerPage = NUM_LEADERBOARD_PER_PAGE;
	pagination.numPage = page;

	G_DBTopAimListAimLatest(pagination, PrintAimLatestCallback, ent);

	PrintIngame(ent - g_entities, "^7Viewing page: %d\n", page);
}

const char *TableCallback_TopAimList_PackName(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimPracticePackMetaData_t *data = context;
	if (!data) {
		assert(qfalse);
		return NULL;
	}
	return data->packName;
}

const char *TableCallback_TopAimList_Author(void *context) {
	if (!context) {
		assert(qfalse);
		return NULL;
	}
	aimPracticePackMetaData_t *data = context;
	if (!data) {
		assert(qfalse);
		return NULL;
	}
	return data->ownerAccountName;
}

static void SubCmd_TopAim_List(gentity_t *ent, const char *mapname) {
	if (!VALIDSTRING(mapname))
		return;

	list_t *list = G_DBTopAimQuickLoadPacks(mapname);
	if (!list) {
		PrintIngame(ent - g_entities, "There are no packs available on %s.\n", mapname);
		return;
	}

	PrintIngame(ent - g_entities, "^5Packs available on %s:^7\n", mapname);

	Table *t = Table_Initialize(qtrue);

	iterator_t iter;
	ListIterate(list, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		aimPracticePackMetaData_t *data = (aimPracticePackMetaData_t *)IteratorNext(&iter);
		Table_DefineRow(t, data);
	}

	Table_DefineColumn(t, "Pack name", TableCallback_TopAimList_PackName, qtrue, 64);
	Table_DefineColumn(t, "Creator", TableCallback_TopAimList_Author, qtrue, 64);

	char buf[1024] = { 0 };
	Table_WriteToBuffer(t, buf, sizeof(buf));
	Table_Destroy(t);

	PrintIngame(ent - g_entities, buf);
}

void Cmd_TopAim_f(gentity_t *ent) {
	if (!ent || !ent->client) {
		return;
	}

	if (!level.topAimRecordsEnabled) {
		trap_SendServerCommand(ent - g_entities, "print \"Aim records are disabled.\n\"");
		return;
	}

	if (level.racemodeRecordsReadonly)
		PrintIngame(ent - g_entities, "^3WARNING: Server settings were modified, capture records are read-only!^7\n");

	// figure out the arguments to redirect the command:
	// subcommand - [mapname - type - page]
	// where the ones in [] can be in any order

	// default arguments
	char subcommand[32] = { 0 };
	qboolean gotMapname = qfalse;
	char mapname[MAX_QPATH];
	Q_strncpyz(mapname, level.mapname, sizeof(mapname));
	qboolean gotPackName = qfalse;
	char packName[32] = { 0 };
	qboolean gotPage = qfalse;
	int page = 1;

	if (trap_Argc() > 1) {
		char buf[64];
		trap_Argv(1, buf, sizeof(buf));

		int startOptionalsAt = 1;

		if (!Q_stricmp(buf, "help") ||
			!Q_stricmp(buf, "list") ||
			!Q_stricmp(buf, "maplist") ||
			!Q_stricmp(buf, "ranks") ||
			!Q_stricmp(buf, "latest"))
		{
			// first arg is a subcommand
			Q_strncpyz(subcommand, buf, sizeof(subcommand));
			startOptionalsAt = 2;
		}
		else {
			if (!gotPackName) {
				aimPracticePack_t *pack = GetPackOnCurrentMapFromMemory(buf);
				if (pack) {
					Q_strncpyz(packName, buf, sizeof(packName));
					gotPackName = qtrue;
					startOptionalsAt = 2;
				}
			}
			if (!gotMapname && !gotPackName) {
				// assume it's a map name otherwise
				Q_strncpyz(mapname, buf, sizeof(mapname));
				gotMapname = qtrue;
				startOptionalsAt = 2;
			}
		}

		int i;
		for (i = startOptionalsAt; i < trap_Argc() && i < 3 + startOptionalsAt; ++i) {
			trap_Argv(i, buf, sizeof(buf));

			if (!gotPage) {
				int testPage = atoi(buf);
				if (testPage > 0 && testPage < 999) {
					page = testPage;
					gotPage = qtrue;
					continue;
				}
			}
			if (!gotPackName) {
				aimPracticePack_t *pack = GetPackOnCurrentMapFromMemory(buf);
				if (pack) {
					Q_strncpyz(packName, buf, sizeof(packName));
					gotPackName = qtrue;
					continue;
				}
			}
			if (!gotMapname) {
				// assume it's a map name otherwise
				Q_strncpyz(mapname, buf, sizeof(mapname));
				gotMapname = qtrue;
				continue;
			}
		}
	}

	if (trap_Argc() == 1 || !Q_stricmp(subcommand, "help")) {
		if (trap_Argc() == 1) {
			iterator_t iter;
			ListIterate(level.aimPracticePackList, &iter, qfalse);
			while (IteratorHasNext(&iter)) {
				aimPracticePack_t *pack = (aimPracticePack_t *)IteratorNext(&iter);
				SubCmd_TopAim_Records(ent, level.mapname, pack->packName, page);
			}
		}

		char *text =
			va("%sTopaim subcommands:\n"
			"        ^7[pack] [page] - show records for the current map\n"
			"  ^9[map] [pack] [page] - show records for another map\n"
			"           ^7list [map] - list all packs available on a map\n"
			"^9maplist [pack] [page] - list all map records\n"
			"  ^7ranks [pack] [page] - show player leaderboard\n"
			"        ^9latest [page] - show latest records^7\n"
			"\n"
			"To begin free training, ^5pull^7 a bot in racemode.\n"
			"To begin timed training, ^5push^7 a bot in racemode.\n",
				trap_Argc() == 1 ? "\n" : "");
		PrintIngame(ent - g_entities, text);
		return;
	}
	else if (!Q_stricmp(subcommand, "list")) {
		SubCmd_TopAim_List(ent, mapname);
	}
	else if (!Q_stricmp(subcommand, "maplist")) {
		if (!VALIDSTRING(packName)) {
			PrintIngame(ent - g_entities, "Please enter a pack name.\n");
			return;
		}
		SubCmd_TopAim_Maplist(ent, packName, page);
	}
	else if (!Q_stricmp(subcommand, "ranks")) {
		if (!VALIDSTRING(packName)) {
			PrintIngame(ent - g_entities, "Please enter a pack name.\n");
			return;
		}
		SubCmd_TopAim_Ranks(ent, packName, page);
	}
	else if (!Q_stricmp(subcommand, "latest")) {
		SubCmd_TopAim_Latest(ent, page);
	}
	else {
		if (!VALIDSTRING(packName)) {
			PrintIngame(ent - g_entities, "Please enter a pack name.\n");
			return;
		}
		SubCmd_TopAim_Records(ent, mapname, packName, page);
		PrintIngame(ent - g_entities, "For a list of all subcommands: /topaim help\n");
	}
}

// begin bg_pmove

extern int speedLoopSound;

weapon_t GetWeaponAtIndexFromBitflags(int mode, int index) {
	if (index < 0)
		return WP_NONE;

	int numGotten = 0;
	for (weapon_t w = WP_NONE; w < WP_NUM_WEAPONS; w++) {
		if (!(mode & (1 << w)))
			continue;
		if (!AimWeaponNameFromInteger(w))
			continue;
		if (numGotten++ == index)
			return w;
	}
	return WP_NONE;
}

static void TeleportPracticers(pmove_t *pm) {
	assert(pm);
	gentity_t *pmEnt = &g_entities[pm->ps->clientNum];
	assert(pmEnt && pmEnt->isAimPracticePack);
	aimPracticePack_t *pack = pmEnt->isAimPracticePack;

	const int numWeapons = NumAllowedAimWeapons(pack->weaponMode);
	const int numSpawnsPerWeapon = pack->numRepsPerVariant * pack->numVariants;

	assert(numWeapons > 0 && numSpawnsPerWeapon > 0);

	// loop through everybody who is using this bot
	for (int i = 0; i < MAX_GENTITIES; i++) {
		gentity_t *ent = &g_entities[i];
		if (ent->aimPracticeEntBeingUsed != pmEnt || !ent->aimPracticeMode)
			continue;

		if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED || !ent->client->sess.inRacemode || ent->health <= 0) {
			ExitAimTraining(ent); // using this bot but shouldn't be anymore
			continue;
		}

		gclient_t *client = ent->client;
		const char *name = ent->client->session && ent->client->account ? ent->client->account->name : &ent->client->pers.netname[0];
		++ent->numAimPracticeSpawns;

		const int lastSpawnWeaponIndex = ent->numAimPracticeSpawns - 2 < 0 ? -1 : (ent->numAimPracticeSpawns - 2) / numSpawnsPerWeapon;
		const int thisSpawnWeaponIndex = (ent->numAimPracticeSpawns - 1) / numSpawnsPerWeapon;
		const int nextSpawnWeaponIndex = ent->numAimPracticeSpawns / numSpawnsPerWeapon;
		const weapon_t lastSpawnWeapon = GetWeaponAtIndexFromBitflags(pack->weaponMode, lastSpawnWeaponIndex);
		const weapon_t thisSpawnWeapon = GetWeaponAtIndexFromBitflags(pack->weaponMode, thisSpawnWeaponIndex);
		const weapon_t nextSpawnWeapon = GetWeaponAtIndexFromBitflags(pack->weaponMode, nextSpawnWeaponIndex);

		G_DeletePlayerProjectiles(ent);
		G_GiveRacemodeItemsAndFullStats(ent);
		ent->client->ps.powerups[PW_REDFLAG] = ent->client->ps.powerups[PW_BLUEFLAG] = 0;

		qboolean finished = qfalse;
		if (ent->aimPracticeMode == AIMPRACTICEMODE_TIMED) {
			// timed practice
			if (thisSpawnWeapon) {
				// we have a weapon to use
				ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_NONE);
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << thisSpawnWeapon);
				ent->client->ps.weapon = thisSpawnWeapon;

				if (thisSpawnWeapon != lastSpawnWeapon || ent->numAimPracticeSpawns == 1) {
					// first spawn on this weapon
					CenterPrintToPlayerAndFollowers(ent, va("^5%s\n\n\n\n\n\n\n\n\n\n", AimWeaponNameFromInteger(thisSpawnWeapon)));
					if (lastSpawnWeapon) {
						// we just finished a weapon
						char *weaponStr = AimWeaponNameFromInteger(lastSpawnWeapon);
						if (VALIDSTRING(weaponStr)) {
							char *lowercase = strdup(weaponStr);
							Q_strlwr(lowercase);
							G_PrintBasedOnRacemode(va("%s ^3%s ^5%s hits:^3%d^7", name, pack->packName, lowercase , ent->numAimPracticeHitsOfWeapon[lastSpawnWeapon]), qtrue);
							free(lowercase);
						}
						else {
							assert(qfalse);
						}
					}
					else {
						// first weapon overall; clear out their hits
						ent->numTotalAimPracticeHits = 0;
						memset(ent->numAimPracticeHitsOfWeapon, 0, sizeof(ent->numAimPracticeHitsOfWeapon));
					}
				}
				else if (nextSpawnWeapon && nextSpawnWeapon != thisSpawnWeapon) {
					// final spawn on this weapon
					CenterPrintToPlayerAndFollowers(ent, va("Next spawn is ^5%s\n\n\n\n\n\n\n\n\n\n", AimWeaponNameFromInteger(nextSpawnWeapon)));
				}
			}
			else {
				// we are done
				finished = qtrue;
			}

			if (finished) {
				if (ent->numTotalAimPracticeHits <= 0) { // they were just afk the entire time? do nothing
					ExitAimTraining(ent);
					continue;
				}

				aimRecord_t record = { 0 };
				record.date = time(NULL);
				trap_Cvar_VariableStringBuffer("sv_matchid", record.extra.matchId, sizeof(record.extra.matchId));
				Q_strncpyz(record.extra.playerName, ent->client->pers.netname, sizeof(record.extra.playerName));
				record.extra.clientId = ent - g_entities;
				record.extra.startTime = ent->aimPracticeStartTime - level.startTime;
				for (int i = 0; i < WP_NUM_WEAPONS; i++)
					record.extra.numHitsOfWeapon[i] = ent->numAimPracticeHitsOfWeapon[i];
				record.time = ent->numTotalAimPracticeHits;

				int oldTime = client->session ? GetScoreForPack(pack, client->session->id) : 0;

				int oldRank = 0;
				int oldPBTime = 0;
				int newRank = 0;

				// efficiency - it is impossible that any actual db update is made until we beat our own personal time
				// cached, tied to the session (unless this is a new record).
				// only allow setting records if the pack is saved and unchanged
				if (!level.racemodeRecordsReadonly && !pack->changed && !pack->isTemp && client->session &&
					client->sess.canSubmitRaceTimes &&
					(record.time > oldTime || !oldTime))
				{
					// this update MAY OR MAY NOT lead to a new rank/pb, because multiple sessions can point to the same account.
					// we could cache this, but we would need to rebuild the cache when hotlinking accounts etc, and this
					// query isn't expensive anyway
					if (client->account) {
						G_DBTopAimGetAccountPersonalBest(client->account->id, level.mapname, pack, &oldRank, &oldPBTime);
					}

					// if our cached time is beaten (or first record), we know that we must ALWAYS write to db.
					if (G_DBTopAimSaveAimRecord(client->session->id, level.mapname, &record, pack)) {
						// success - update our cached time to reflect db
						SetScoreForPack(pack, client->session->id, record.time);

						// also check for a new rank/pb
						if (client->account) {
							G_DBTopAimGetAccountPersonalBest(client->account->id, level.mapname, pack, &newRank, NULL);
						}
					}
					else {
						// failure - let them know their record couldn't be saved :sadcat:
						Com_Printf("Error saving client %d record\n", ent - g_entities);
						trap_SendServerCommand(ent - g_entities, "print \"An error occured in the database, your record was not saved.\n\"");
					}
				}

				Com_Printf("client %d: oldTime: %d, oldRank: %d, newRank: %d, oldPBTime: %d, record.time: %d\n", ent - g_entities, oldTime, oldRank, newRank, oldPBTime, record.time);

				if (!pack->isTemp && client->session && client->account &&
					!(oldRank < 0 || oldPBTime < 0 || newRank < 0) &&
					((!oldRank && newRank > 0) || (oldRank > 0 && oldPBTime > 0 && newRank > 0 && (newRank < oldRank || record.time > oldPBTime))))
				{
					// beat someone OR a personal best OR a new record

					char rankString[16] = { 0 };

					if (newRank < oldRank) {
						// we beat someone
						Q_strcat(rankString, sizeof(rankString), va("%d -> ", oldRank));
					}

					if (newRank == 1) {
						// rank 1, print in red :pog:
						Q_strcat(rankString, sizeof(rankString), S_COLOR_RED);
					}

					Q_strcat(rankString, sizeof(rankString), va("%d", newRank));

					char printString[1024] = { 0 };
					Com_sprintf(printString, sizeof(printString), "^7Training completed by %s     ^5rank:^3%s     ^5pack:^3%s     ^5score:^3%d",
						name,
						rankString,
						pack->packName,
						ent->numTotalAimPracticeHits);
					for (weapon_t w = WP_NONE; w < WP_NUM_WEAPONS; w++) {
						if (!(pack->weaponMode & (1 << w)))
							continue;
						char *weapStr = AimWeaponNameFromInteger(w);
						if (VALIDSTRING(weapStr)) {
							char *lowercase = strdup(weapStr);
							Q_strlwr(lowercase);
							Q_strcat(printString, sizeof(printString), va("     ^5%s:^3%d", lowercase, ent->numAimPracticeHitsOfWeapon[w]));
							free(lowercase);
						}
						else {
							assert(qfalse); // ???
						}
					}
					Q_strcat(printString, sizeof(printString), "^7\n");
					G_PrintBasedOnRacemode(printString, qtrue);

					// play a sound to all racers

					// same sounds as japro
					int soundIndex;
					if (newRank == 1) {
						soundIndex = G_SoundIndex("sound/chars/rosh_boss/misc/victory3");
					}
					else {
						soundIndex = G_SoundIndex("sound/chars/rosh/misc/taunt1");
					}

					gentity_t *te = G_Sound(ent, CHAN_AUTO, soundIndex);
					te->r.svFlags |= SVF_BROADCAST; // broadcast so that all racers and racespectators can hear it
				}
				else
				{
					// error, or no account, etc... print just the time

					const char *name = client->session && client->account ? client->account->name : &client->pers.netname[0];

					char printString[1024] = { 0 };
					Com_sprintf(printString, sizeof(printString), "^7Training completed by %s     ^5pack:^3%s     ^5score:^3%d",
						name,
						pack->packName,
						ent->numTotalAimPracticeHits);
					for (weapon_t w = WP_NONE; w < WP_NUM_WEAPONS; w++) {
						if (!(pack->weaponMode & (1 << w)))
							continue;
						char *weapStr = AimWeaponNameFromInteger(w);
						if (VALIDSTRING(weapStr)) {
							char *lowercase = strdup(weapStr);
							Q_strlwr(lowercase);
							Q_strcat(printString, sizeof(printString), va("     ^5%s:^3%d", lowercase, ent->numAimPracticeHitsOfWeapon[w]));
							free(lowercase);
						}
						else {
							assert(qfalse); // ???
						}
					}
					Q_strcat(printString, sizeof(printString), "^7\n");
					G_PrintBasedOnRacemode(printString, qtrue);

					G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/weapons/force/heal.wav"));
					// don't broadcast this sound, play it for other nearby clients and specs only
				}

				ExitAimTraining(ent);
				continue;
			}
		}

		// this player is still training; teleport them player back to the starting point for their bot
		for (int i = 0; i < NUM_FORCE_POWERS; i++) {
			if (ent->client->ps.fd.forcePowersActive & (1 << i))
				WP_ForcePowerStop(ent, i);
		}

		VectorCopy(pack->variants[pack->currentVariantIndex].playerSpawnPoint, ent->client->ps.origin);
		ent->client->ps.eFlags ^= EF_TELEPORT_BIT;
		vec3_t newViewAngles;
		newViewAngles[0] = 0;
		newViewAngles[1] = pack->variants[pack->currentVariantIndex].playerSpawnYaw;
		newViewAngles[2] = 0;
		SetClientViewAngle(ent, newViewAngles); // set angles
		vec3_t velocityAngles;
		velocityAngles[0] = 0;
		velocityAngles[1] = pack->variants[pack->currentVariantIndex].playerVelocityYaw;
		velocityAngles[2] = 0;
		vec3_t fwd;
		AngleVectors(velocityAngles, fwd, NULL, NULL);
		VectorScale(fwd, pack->variants[pack->currentVariantIndex].playerSpawnSpeed, ent->client->ps.velocity);
		
		if (ent->client->ps.weapon == WP_DISRUPTOR) {
			// always force them to exit disruptor scope
			ent->client->ps.zoomMode = 0;
			ent->client->ps.zoomLocked = qfalse;
			ent->client->ps.weaponstate = WEAPON_READY;
		}
		else if (ent->client->ps.weaponstate == WEAPON_CHARGING || ent->client->ps.weaponstate == WEAPON_CHARGING_ALT) {
			// force them to re-charge charged weapons
			ent->client->ps.weaponstate = WEAPON_READY;
		}

		// reset weapon cooldowns and charge times
		ent->client->ps.weaponTime = 0;
		ent->client->ps.weaponChargeTime = level.time;

		BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, qfalse); // save results of pmove

		// use speed automatically if they set the setting
		if (!(ent->client->sess.racemodeFlags & RMF_DONTTELEWITHSPEED)) {
			qboolean wasAlreadyActive = ent->client->ps.fd.forcePowersActive & (1 << FP_SPEED);

			WP_ForcePowerStart(ent, FP_SPEED, 0);
			G_Sound(ent, CHAN_BODY, G_SoundIndex("sound/weapons/force/speed.wav"));
			if (!wasAlreadyActive) // only start the loop if it wasn't already active
				G_Sound(ent, TRACK_CHANNEL_2, speedLoopSound);

			ent->client->ps.forceAllowDeactivateTime = level.time + 1500;
		}
	}
}

// returns qtrue if we need to do the next route
static qboolean MoveAimPracticeBot(pmove_t *pm, gentity_t *pmEnt, aimPracticePack_t *pack, qboolean teleport) {
	assert(pmEnt && pmEnt - g_entities >= MAX_CLIENTS);
	if (pack->numVariants <= 0 || pack->deleteMe) {
		// no routes or deletion is queued
		// terrible fucking hack: instead of bothering to delete it/respawn it later, etc., just warp it way the fuck to far reaches of space
		pm->ps->origin[0] = pm->ps->origin[1] = pm->ps->origin[2] = 65000; // hopefully there is nothing here
		BG_PlayerStateToEntityState(pm->ps, &pmEnt->s, qfalse); // save results of pmove
		return qfalse;
	}

	aimVariant_t *var = &pack->variants[pack->currentVariantIndex];
	int timeElapsed = trap_Milliseconds() - pack->routeStartTime;

	int i;
	qboolean gotIt = qfalse;
	for (i = 0; i < var->trailHead; i++) {
		if (var->trail[i].time > timeElapsed) {
			gotIt = qtrue;
			break;
		}
	}
	if (!gotIt)
		return qtrue;
	int h = i - 1;
	if (h < 0)
		h = 0;

	aimPracticeMovementTrail_t *early = &var->trail[h], *late = &var->trail[i];

	float frac = (float)(late->time - timeElapsed) / (float)(late->time - early->time);

	TimeShiftLerp(frac, late->currentOrigin, early->currentOrigin, pm->ps->origin);
	TimeShiftLerp(frac, late->mins, early->mins, pm->mins);
	TimeShiftLerp(frac, late->maxs, early->maxs, pm->maxs);

	pm->ps->torsoAnim = late->torsoAnim;
	pm->ps->legsAnim = late->legsAnim;
	TimeShiftAnimLerp(frac, late->torsoAnim, early->torsoAnim, late->torsoTimer, early->torsoTimer, &pm->ps->torsoTimer);
	TimeShiftAnimLerp(frac, late->legsAnim, early->legsAnim, late->legsTimer, early->legsTimer, &pm->ps->legsTimer);
	vec3_t viewAngles;
	for (int axis = 0; axis < 3; axis++) {
		viewAngles[axis] = LerpAngle(late->angles[axis], early->angles[axis], frac);
	}
	SetClientViewAngle(pmEnt, viewAngles);
	TimeShiftLerp(frac, late->velocity, early->velocity, pm->ps->velocity);

	if (teleport)
		pm->ps->eFlags ^= EF_TELEPORT_BIT;
	BG_PlayerStateToEntityState(pm->ps, &pmEnt->s, qfalse); // save results of pmove

	return qfalse;
}

// this is called every time we get a pmove (once per tick for npcs)
void DoAimPackPmove(pmove_t *pm) {
	assert(pm);
	if (pm->ps->clientNum < MAX_CLIENTS)
		return;

	gentity_t *pmEnt = &g_entities[pm->ps->clientNum];
	aimPracticePack_t *pack = pmEnt->isAimPracticePack;
	if (!pack)
		return;

	pmEnt->client->ps.forceHandExtend = HANDEXTEND_NONE;
	pmEnt->client->ps.forceHandExtendTime = 0;
	pmEnt->client->ps.forceDodgeAnim = 0;

	pm->cmd.buttons = pm->cmd.generic_cmd = pm->cmd.forwardmove = pm->cmd.upmove = pm->cmd.rightmove = 0;
	pm->cmd.angles[0] = pm->cmd.angles[1] = pm->cmd.angles[2] = 0;
	qboolean needToDoNewRoute = qtrue;
	if (pack->numVariants <= 0 || pack->deleteMe) {
		// no routes or deletion is queued; this pack should not be visible right now
		needToDoNewRoute = MoveAimPracticeBot(pm, pmEnt, pack, qfalse);
	}
	else if (!pack->forceRestart) {
		// move it to the next spot along the current route
		needToDoNewRoute = MoveAimPracticeBot(pm, pmEnt, pack, qfalse);
	}

	if (needToDoNewRoute) {
		// forced restart or reached the end of the current route; change to the next route

		// if nobody is using this for timed runs and we are about to start a new route anyway, might as well re-randomize the route order
		if (pack->numVariants > 1) {
			qboolean beingUsedForTimedRuns = qfalse;
			for (int i = 0; i < MAX_CLIENTS; i++) {
				gentity_t *thisEnt = &g_entities[i];
				if (!thisEnt->inuse || !thisEnt->client || thisEnt->client->pers.connected != CON_CONNECTED ||
					thisEnt->aimPracticeEntBeingUsed != pmEnt || thisEnt->aimPracticeMode != AIMPRACTICEMODE_TIMED)
					continue;
				beingUsedForTimedRuns = qtrue;
				break;
			}
			if (!beingUsedForTimedRuns)
				RandomizeAndRestartPack(pack);
		}

		// start over
		pack->routeStartTime = trap_Milliseconds();
		if (++pack->currentVariantNumInRandomVariantOrder >= pack->numVariants * pack->numRepsPerVariant)
			pack->currentVariantNumInRandomVariantOrder = 0;
		pack->currentVariantIndex = pack->randomVariantOrder[pack->currentVariantNumInRandomVariantOrder];

		// move to the initial spot of the new route
		MoveAimPracticeBot(pm, pmEnt, pack, qtrue);
		TeleportPracticers(pm);
		pack->forceRestart = qfalse;
	}
}