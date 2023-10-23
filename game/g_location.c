#include "g_local.h"
#include "kdtree.h"

static int pitLocationIndex = 0;

static qboolean IsEntityUniqueType(gentity_t *ent) {
	int i;
	gentity_t *otherEnt;

	for (i = 0, otherEnt = g_entities; i < level.num_entities; ++i, ++otherEnt) {
		if (!otherEnt || !otherEnt->classname) {
			continue;
		}

		if (otherEnt == ent) {
			continue;
		}

		if (!Q_stricmp(ent->classname, otherEnt->classname)) {
			return qfalse;
		}
	}

	return qtrue;
}

void Location_AddLocationEntityToList(gentity_t *ent) {
	info_b_e_location_listItem_t *add = ListAdd(&level.info_b_e_locationsList, sizeof(info_b_e_location_listItem_t));
	Q_strncpyz(add->message, ent->message, sizeof(add->message));
	Q_CleanStr(add->message);
	add->teamowner = Com_Clampi(0, 2, ent->s.teamowner);
	VectorCopy(ent->s.origin, add->origin);
	char *str = NULL;

	G_SpawnString("min_x", "", &str);
	if (VALIDSTRING(str)) {
		add->min[0].value = atof(str);
		add->min[0].valid = qtrue;
	}

	G_SpawnString("min_y", "", &str);
	if (VALIDSTRING(str)) {
		add->min[1].value = atof(str);
		add->min[1].valid = qtrue;
	}

	G_SpawnString("min_z", "", &str);
	if (VALIDSTRING(str)) {
		add->min[2].value = atof(str);
		add->min[2].valid = qtrue;
	}

	G_SpawnString("max_x", "", &str);
	if (VALIDSTRING(str)) {
		add->max[0].value = atof(str);
		add->max[0].valid = qtrue;
	}

	G_SpawnString("max_y", "", &str);
	if (VALIDSTRING(str)) {
		add->max[1].value = atof(str);
		add->max[1].valid = qtrue;
	}

	G_SpawnString("max_z", "", &str);
	if (VALIDSTRING(str)) {
		add->max[2].value = atof(str);
		add->max[2].valid = qtrue;
	}
}

qboolean isRedFlagstand(gentity_t *ent) {
	return !Q_stricmp(ent->classname, "team_ctf_redflag");
}

qboolean isBlueFlagstand(gentity_t *ent) {
	return !Q_stricmp(ent->classname, "team_ctf_blueflag");
}

static qboolean isWeapon(gentity_t *ent) {
	return ent->item && ent->item->giType == IT_WEAPON;
}

static qboolean MakeEnhancedLocation(gentity_t *ent, enhancedLocation_t *loc) {
	// flags are always statically bound to a team

	if (isRedFlagstand(ent)) {
		Q_strncpyz(loc->message, "Flagstand", sizeof(loc->message));
		loc->teamowner = TEAM_RED;
		return qtrue;
	}

	if (isBlueFlagstand(ent)) {
		Q_strncpyz(loc->message, "Flagstand", sizeof(loc->message));
		loc->teamowner = TEAM_BLUE;
		return qtrue;
	}

	// only count certain powerups and weapons

	if (ent->item && ent->item->giType == IT_POWERUP) {
		switch (ent->item->giTag) {
		case PW_FORCE_ENLIGHTENED_LIGHT:
			Q_strncpyz(loc->message, "Light Enlightenment", sizeof(loc->message));
			break;
		case PW_FORCE_ENLIGHTENED_DARK:
			Q_strncpyz(loc->message, "Dark Enlightenment", sizeof(loc->message));
			break;
		case PW_FORCE_BOON:
			if (/*!g_enableBoon.integer && */IsEntityUniqueType(ent)) // call it mid regardless of boon being enabled
				Q_strncpyz(loc->message, "Mid", sizeof(loc->message));
			break;
		case PW_YSALAMIRI:
			Q_strncpyz(loc->message, "Ysalamiri", sizeof(loc->message));
			break;
		default:
			return qfalse;
		}
	}
	else if (ent->item && ent->item->giType == IT_WEAPON) {
		switch (ent->item->giTag) {
			/*case WP_STUN_BATON:
				Q_strncpyz( loc->message, "Stun Baton", sizeof( loc->message ) );
				break;
			case WP_BRYAR_PISTOL:
			case WP_BRYAR_OLD:
				Q_strncpyz( loc->message, "Pistol", sizeof( loc->message ) );
				break;*/
		case WP_BLASTER:
			Q_strncpyz(loc->message, "Blaster", sizeof(loc->message));
			break;
		case WP_DISRUPTOR:
			Q_strncpyz(loc->message, "Disruptor", sizeof(loc->message));
			break;
		case WP_BOWCASTER:
			Q_strncpyz(loc->message, "Bowcaster", sizeof(loc->message));
			break;
		case WP_REPEATER:
			Q_strncpyz(loc->message, "Repeater", sizeof(loc->message));
			break;
		case WP_DEMP2:
			Q_strncpyz(loc->message, "Demp", sizeof(loc->message));
			break;
		case WP_FLECHETTE:
			Q_strncpyz(loc->message, "Golan", sizeof(loc->message));
			break;
		case WP_ROCKET_LAUNCHER:
			Q_strncpyz(loc->message, "Rockets", sizeof(loc->message));
			break;
		case WP_THERMAL:
			Q_strncpyz(loc->message, "Thermals", sizeof(loc->message));
			break;
		case WP_TRIP_MINE:
			Q_strncpyz(loc->message, "Mines", sizeof(loc->message));
			break;
		case WP_DET_PACK:
			Q_strncpyz(loc->message, "Detpacks", sizeof(loc->message));
			break;
		case WP_CONCUSSION:
			Q_strncpyz(loc->message, "Concussion", sizeof(loc->message));
			break;
		default:
			return qfalse;
		}
	}
	else {
		return qfalse; // all other entities don't count
	}

	// in modes that are not those, locations don't have owners
	if (g_gametype.integer != GT_CTF && g_gametype.integer != GT_CTY) {
		loc->teamowner = TEAM_FREE;
		return qtrue;
	}

	// first, if this entity is the only entity of its type on the map, it has no owner
	if (IsEntityUniqueType(ent)) {
		loc->teamowner = TEAM_FREE;
		return qtrue;
	}

	// let's find the closest flagstands (in case of multiple flagstands)
	gentity_t *closestRedFlagstand = G_ClosestEntity(ent, isRedFlagstand);
	gentity_t *closestBlueFlagstand = G_ClosestEntity(ent, isBlueFlagstand);

	// is there just a red flag, just a blue flag, or no flag at all?
	if (!closestRedFlagstand && closestBlueFlagstand) {
		loc->teamowner = TEAM_BLUE;
		return qtrue;
	}
	else if (closestRedFlagstand && !closestBlueFlagstand) {
		loc->teamowner = TEAM_RED;
		return qtrue;
	}
	else if (!closestRedFlagstand && !closestBlueFlagstand) {
		loc->teamowner = TEAM_FREE;
		return qtrue;
	}

	// we need to play with distances then
	vec_t redDistance = DistanceSquared(ent->s.origin, closestRedFlagstand->s.origin);
	vec_t blueDistance = DistanceSquared(ent->s.origin, closestBlueFlagstand->s.origin);

	if (redDistance < blueDistance) {
		loc->teamowner = TEAM_RED;
		return qtrue;
	}
	else if (blueDistance < redDistance) {
		loc->teamowner = TEAM_BLUE;
		return qtrue;
	}

	// both flags are equidistant, let's take the closest weapon and see which flag is closer to it instead
	gentity_t *closestWeapon = G_ClosestEntity(ent, isWeapon);
	if (closestWeapon) {
		redDistance = DistanceSquared(closestWeapon->s.origin, closestRedFlagstand->s.origin);
		blueDistance = DistanceSquared(closestWeapon->s.origin, closestBlueFlagstand->s.origin);

		if (redDistance < blueDistance) {
			loc->teamowner = TEAM_RED;
			return qtrue;
		}
		else if (blueDistance < redDistance) {
			loc->teamowner = TEAM_BLUE;
			return qtrue;
		}
	}

	// well, I tried
	loc->teamowner = TEAM_FREE;
	return qtrue;
}

static void GenerateProximityLocations(void) {
	enhancedLocation_t loc;

	G_Printf("Procedurally generating enhanced locations...\n");

	if (*trap_kd_numunique() + 1 < MAX_LOCATIONS) {
		enhancedLocation_t *targetLoc = NULL;

		// this is a valid location, let's see if we already have a handle for it
		for (int j = 0; j < *trap_kd_numunique(); ++j) {
			enhancedLocation_t *thisLoc = trap_kd_dataptr(j);

			if (!strcmp("Pit", thisLoc->message) && !thisLoc->teamowner) {
				targetLoc = thisLoc;
				break;
			}
		}

		// there was no handle with this location message, copy the info to a new one
		if (!targetLoc) {
			pitLocationIndex = *trap_kd_numunique();
			*trap_kd_numunique() += 1;
			targetLoc = trap_kd_dataptr(pitLocationIndex);
			Q_strncpyz(targetLoc->message, "Pit", sizeof(targetLoc->message));
			targetLoc->teamowner = 0;
		}

		//trap_kd_insertf(vec3_origin, targetLoc);
		level.locations.enhanced.numTotal++;
	}
	else {
		pitLocationIndex = -1;
	}

	int i;
	gentity_t *ent;

	for (i = 0, ent = g_entities; i < level.num_entities && *trap_kd_numunique() < MAX_LOCATIONS; ++i, ++ent) {
		if (!ent || !ent->classname) {
			continue;
		}

		if (MakeEnhancedLocation(ent, &loc)) {
			enhancedLocation_t *targetLoc = NULL;

			// this is a valid location, let's see if we already have a handle for it
			for (int j = 0; j < *trap_kd_numunique(); ++j) {
				enhancedLocation_t *thisLoc = trap_kd_dataptr(j);

				if (!strcmp(loc.message, thisLoc->message) && loc.teamowner == thisLoc->teamowner) {
					targetLoc = thisLoc;
					break;
				}
			}

			// there was no handle with this location message, copy the info to a new one
			if (!targetLoc) {
				targetLoc = trap_kd_dataptr(*trap_kd_numunique());
				*trap_kd_numunique() += 1;
				Q_strncpyz(targetLoc->message, loc.message, sizeof(targetLoc->message));
				targetLoc->teamowner = loc.teamowner;
			}

			trap_kd_insertf(ent->s.origin, targetLoc);
			level.locations.enhanced.numTotal++;
		}
	}
}

qboolean IsPointVisible(vec3_t org1, vec3_t org2)
{
	trace_t tr;
	trap_Trace(&tr, org1, NULL, NULL, org2, ENTITYNUM_NONE, MASK_SOLID);
	return !!(tr.fraction == 1);
}

qboolean StartsSolid(vec3_t point) {
	trace_t tr;
	trap_Trace(&tr, point, NULL, NULL, point, ENTITYNUM_NONE, MASK_SOLID);
	return !!(tr.startsolid);
}

#define DotProduct2D(a,b)	((a)[0]*(b)[0]+(a)[1]*(b)[1])
extern qboolean isRedFlagstand(gentity_t *ent);
extern qboolean isBlueFlagstand(gentity_t *ent);
qboolean PointsAreOnOppositeSidesOfMap(vec3_t pointA, vec3_t pointB) {
	static qboolean initialized = qfalse, valid = qfalse;
	static float redFs[2] = { 0, 0 }, blueFs[2] = { 0, 0 };
	if (!initialized) {
		initialized = qtrue;
		gentity_t temp;
		VectorCopy(vec3_origin, temp.r.currentOrigin);
		gentity_t *redFsEnt = G_ClosestEntity(&temp, isRedFlagstand);
		gentity_t *blueFsEnt = G_ClosestEntity(&temp, isBlueFlagstand);
		if (redFsEnt && blueFsEnt) {
			valid = qtrue;
			redFs[0] = redFsEnt->r.currentOrigin[0];
			redFs[1] = redFsEnt->r.currentOrigin[1];
			blueFs[0] = blueFsEnt->r.currentOrigin[0];
			blueFs[1] = blueFsEnt->r.currentOrigin[1];
		}
	}

	if (!valid)
		return qfalse;

	float pointADistanceToRedFs = Distance2D(pointA, redFs);
	float pointADistanceToBlueFs = Distance2D(pointA, blueFs);

	float *allyFs, *enemyFs;
	if (pointADistanceToRedFs < pointADistanceToBlueFs) {
		allyFs = &redFs[0];
		enemyFs = &blueFs[0];
	}
	else {
		allyFs = &blueFs[0];
		enemyFs = &redFs[0];
	}

	float pointAResult;
	{
		float rPrime[2] = { pointA[0] - allyFs[0], pointA[1] - allyFs[1] };
		float v[2] = { enemyFs[0] - allyFs[0], enemyFs[1] - allyFs[1] };
		pointAResult = DotProduct2D(v, rPrime) / DotProduct2D(v, v);
	}

	float pointBResult;
	{
		float rPrime[2] = { pointB[0] - allyFs[0], pointB[1] - allyFs[1] };
		float v[2] = { enemyFs[0] - allyFs[0], enemyFs[1] - allyFs[1] };
		pointBResult = DotProduct2D(v, rPrime) / DotProduct2D(v, v);
	}

	// sanity check, allow some fudging in any case if the points are very close together
	if (fabs(pointAResult - pointBResult) < 0.05f)
		return qfalse;

	// if a point is very close to mid, allow some fudging
	if (fabs(pointAResult - 0.5f) < 0.05f)
		return qfalse;
	if (fabs(pointBResult - 0.5f) < 0.05f)
		return qfalse;

	if (pointAResult < 0.5f && pointBResult > 0.5f)
		return qtrue;
	if (pointAResult > 0.5f && pointBResult < 0.5f)
		return qtrue;

	return qfalse;
}

#define MAX_LINE_OF_SIGHT_DISTANCE		(1500.0f) // beyond this distance from an entity we just use proximity
#define Z_AXIS_BOOST					(110.0f)

static qboolean ClosestEntityInLineOfSight(vec3_t point, vec3_t tracePoint, enhancedLocation_t *locOut) {
	float closestDistance = MAX_LINE_OF_SIGHT_DISTANCE; // ignore entities farther than this
	qboolean gotOne = qfalse;
	if (!level.generateLocationsWithInfo_b_e_locationsOnly) {
		for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
			gentity_t *ent = &g_entities[i];
			enhancedLocation_t loc = { 0 };
			if (!MakeEnhancedLocation(ent, &loc))
				continue; // not a valid entity for locations

			float dist = Distance(tracePoint, ent->r.currentOrigin);
			if (dist >= closestDistance)
				continue; // farther away than current best

			if (PointsAreOnOppositeSidesOfMap(tracePoint, ent->r.currentOrigin))
				continue; // on opposite sides of the map

			if (!IsPointVisible(tracePoint, ent->r.currentOrigin))
				continue; // check visibility last to avoid unnecessary tracing

			memcpy(locOut, &loc, sizeof(enhancedLocation_t));
			closestDistance = dist;
			gotOne = qtrue;
		}
	}

	iterator_t iter;
	ListIterate(&level.info_b_e_locationsList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		info_b_e_location_listItem_t *listItem = (info_b_e_location_listItem_t *)IteratorNext(&iter);
		for (int i = 0; i < 3; i++) {
			if (listItem->min[i].valid && point[i] < listItem->min[i].value)
				goto nextClosestLocInLineOfSight;
			if (listItem->max[i].valid && point[i] > listItem->max[i].value)
				goto nextClosestLocInLineOfSight;
		}

		float dist = Distance(tracePoint, listItem->origin);
		if (dist >= closestDistance)
			continue; // farther away than current best

		if (PointsAreOnOppositeSidesOfMap(tracePoint, listItem->origin))
			continue; // on opposite sides of the map

		if (!IsPointVisible(tracePoint, listItem->origin))
			continue; // check visibility last to avoid unnecessary tracing

		locOut->teamowner = listItem->teamowner;
		Q_strncpyz(locOut->message, listItem->message, sizeof(locOut->message));
		closestDistance = dist;
		gotOne = qtrue;

		nextClosestLocInLineOfSight:;
	}

	return gotOne;
}

static qboolean ClosestEntity(vec3_t point, vec3_t tracePoint, enhancedLocation_t *locOut) {
	float closestDistance = 9999999;
	qboolean gotOne = qfalse;
	if (!level.generateLocationsWithInfo_b_e_locationsOnly) {
		for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
			gentity_t *ent = &g_entities[i];
			enhancedLocation_t loc = { 0 };
			if (!MakeEnhancedLocation(ent, &loc))
				continue; // not a valid entity for locations

			float dist = Distance(tracePoint, ent->r.currentOrigin);
			if (dist >= closestDistance)
				continue; // farther away than current best

			if (PointsAreOnOppositeSidesOfMap(tracePoint, ent->r.currentOrigin))
				continue; // on opposite sides of the map

			memcpy(locOut, &loc, sizeof(enhancedLocation_t));
			closestDistance = dist;
			gotOne = qtrue;
		}
	}

	iterator_t iter;
	ListIterate(&level.info_b_e_locationsList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		info_b_e_location_listItem_t *listItem = (info_b_e_location_listItem_t *)IteratorNext(&iter);
		for (int i = 0; i < 3; i++) {
			if (listItem->min[i].valid && point[i] < listItem->min[i].value)
				goto nextClosestLoc;
			if (listItem->max[i].valid && point[i] > listItem->max[i].value)
				goto nextClosestLoc;
		}

		float dist = Distance(tracePoint, listItem->origin);
		if (dist >= closestDistance)
			continue; // farther away than current best

		if (PointsAreOnOppositeSidesOfMap(tracePoint, listItem->origin))
			continue; // on opposite sides of the map

		locOut->teamowner = listItem->teamowner;
		Q_strncpyz(locOut->message, listItem->message, sizeof(locOut->message));
		closestDistance = dist;
		gotOne = qtrue;

		nextClosestLoc:;
	}

	return gotOne;
}

typedef struct {
	byte		data; // 0 == deleted; 1-64 == location index to reference from locationNames and teamOwner
} enhancedLocationPoint_t;

typedef struct {
	int						protocol; // in case we want to break compatibility
	float					incrementSize; // how much space is in between each point on any axis (defined by the "locationaccuracy" worldspawn key)
	float					mins[3]; // where the points start incrementing from for each axis
	int						dimensionSize[3]; // the number of points on each axis
	char					locationNames[MAX_LOCATIONS][MAX_LOCATION_CHARS]; // 64 location names
	byte					teamOwner[MAX_LOCATIONS]; // 64 team owners (can be zero)
	enhancedLocationPoint_t ***pointsArr; // 3d array, get points using pointsArr[x][y][z]
} enhancedLocationData_t;

enhancedLocationData_t data;

qboolean PointsUseSameMsg(int x1, int y1, int z1, int x2, int y2, int z2) {
	enhancedLocationPoint_t *point1 = &data.pointsArr[x1][y1][z1];
	enhancedLocationPoint_t *point2 = &data.pointsArr[x2][y2][z2];
	return (point1->data == point2->data);
}

qboolean PointIsValid(int x, int y, int z) {
	if (x < 0 || y < 0 || z < 0 || x >= data.dimensionSize[0] || y >= data.dimensionSize[1] || z >= data.dimensionSize[2])
		return qfalse;
	return qtrue;
}

qboolean PointIsStartSolid(int x, int y, int z) {
	enhancedLocationPoint_t *point = &data.pointsArr[x][y][z];
	return (point->data == 0xFF);
}

void MakePointStartSolid(enhancedLocationPoint_t *point) {
	point->data = 0xFF;
}

qboolean PointIsDeleted(int x, int y, int z) {
	enhancedLocationPoint_t *point = &data.pointsArr[x][y][z];
	return (point->data == 0);
}

void DeletePoint(enhancedLocationPoint_t *point) {
	point->data = 0;
}

void SetPointIndex(enhancedLocationPoint_t *point, int index) {
	point->data = (index + 1) & 0b111111;
}

static void FreePointArray(enhancedLocationPoint_t ***arr) {
	size_t xlen = data.dimensionSize[0], ylen = data.dimensionSize[1];
	size_t i, j;

	for (i = 0; i < xlen; ++i) {
		if (arr[i] != NULL) {
			for (j = 0; j < ylen; ++j)
				free(arr[i][j]);
			free(arr[i]);
		}
	}
	free(arr);
}

static qboolean AllocatePointArray(void) {
	size_t xlen = data.dimensionSize[0], ylen = data.dimensionSize[1], zlen = data.dimensionSize[2];
	enhancedLocationPoint_t ***p;
	size_t i, j;

	if ((p = calloc(xlen, sizeof * p)) == NULL) {
		assert(qfalse);
		return qfalse;
	}

	for (i = 0; i < xlen; ++i)
		p[i] = NULL;

	for (i = 0; i < xlen; ++i)
		if ((p[i] = calloc(ylen, sizeof * p[i])) == NULL) {
			assert(qfalse);
			FreePointArray(p);
			return qfalse;
		}

	for (i = 0; i < xlen; ++i)
		for (j = 0; j < ylen; ++j)
			p[i][j] = NULL;

	for (i = 0; i < xlen; ++i)
		for (j = 0; j < ylen; ++j)
			if ((p[i][j] = calloc(zlen, sizeof * p[i][j])) == NULL) {
				assert(qfalse);
				FreePointArray(p);
				return qfalse;
			}

	data.pointsArr = p;
	return qtrue;
}

// writes all of data to disk, although only valid points are written
static void WriteEnhancedLocationDataToDisk(void) {
	Com_Printf("Writing to disk...");

	int startTime = trap_Milliseconds();
	fileHandle_t f;
	char fileName[MAX_QPATH];
	Com_sprintf(fileName, sizeof(fileName), "maps/%s.enhancedlocations", level.mapname);
	trap_FS_FOpenFile(fileName, &f, FS_WRITE);
	if (!f) {
		Com_Printf("failed to write to disk!\n");
		return;
	}

	const int lenBeforeArray = sizeof(data) - sizeof(data.pointsArr);
	int arraySize = 0;
	byte *buf = malloc(lenBeforeArray);

	// write the stuff before the point array
	memcpy(buf, &data, lenBeforeArray);
	const int pointSize = (sizeof(unsigned short) * 3) + sizeof(byte);

	// write all valid points
	// format: XXYYZZ# where XX/YY/ZZ are unsigned short coordinates and # is the location index byte
	byte *ptr = buf + lenBeforeArray;
	for (unsigned short i = 0; i < data.dimensionSize[0]; i++) {
		for (unsigned short j = 0; j < data.dimensionSize[1]; j++) {
			for (unsigned short k = 0; k < data.dimensionSize[2]; k++) {
				enhancedLocationPoint_t *point = &data.pointsArr[i][j][k];
				if (PointIsDeleted(i, j, k) || PointIsStartSolid(i, j, k))
					continue;

				buf = realloc(buf, lenBeforeArray + arraySize + (sizeof(unsigned short) * 3) + sizeof(byte));

				memcpy(buf + lenBeforeArray + arraySize, &i, sizeof(unsigned short));
				memcpy(buf + lenBeforeArray + arraySize + sizeof(unsigned short), &j, sizeof(unsigned short));
				memcpy(buf + lenBeforeArray + arraySize + (sizeof(unsigned short) * 2), &k, sizeof(unsigned short));
				memcpy(buf + lenBeforeArray + arraySize + (sizeof(unsigned short) * 3), &point->data, sizeof(byte));

				arraySize += pointSize;
			}
		}
	}

	trap_FS_Write(buf, lenBeforeArray + arraySize, f);
	trap_FS_FCloseFile(f);
	free(buf);

	int endTime = trap_Milliseconds();
	Com_Printf("done (took %0.3f seconds).\n", ((float)(endTime - startTime)) / 1000.0f);
}

#define ENHANCED_LOCATION_PROTOCOL	(1)

static qboolean ReadEnhancedLocationDataFromDisk(const char *fileName) {
	Com_Printf("Reading %s from disk...", fileName);
	int startTime = trap_Milliseconds();
	fileHandle_t f;
	int len = trap_FS_FOpenFile(fileName, &f, FS_READ);
	if (!len) {
		Com_Printf("Error reading %s!\n", fileName);
		return qfalse;
	}

	byte *buf = malloc(len);
	trap_FS_Read(buf, len, f);
	trap_FS_FCloseFile(f);

	// read the stuff before the point array
	const int lenBeforeArray = sizeof(data) - sizeof(data.pointsArr);
	memcpy(&data, buf, lenBeforeArray);

	if (data.protocol > ENHANCED_LOCATION_PROTOCOL) {
		Com_Printf("%s is from a newer mod version! Unable to read.\n");
		free(buf);
		return qfalse;
	}

	// read the points, which are all valid
	// format: XXYYZZ# where XX/YY/ZZ are unsigned short coordinates and # is the location index byte
	int arrayLen = 0;
	int remaining = len - lenBeforeArray;
	const int pointSize = (sizeof(unsigned short) * 3) + sizeof(byte);

	if (!AllocatePointArray()) {
		Com_Printf("Failed to allocate memory for locations!\n");
		return qfalse;
	}

	while (remaining >= pointSize) {
		unsigned short x, y, z;
		byte readData;
		memcpy(&x, buf + lenBeforeArray + arrayLen, sizeof(unsigned short));
		memcpy(&y, buf + lenBeforeArray + arrayLen + sizeof(unsigned short), sizeof(unsigned short));
		memcpy(&z, buf + lenBeforeArray + arrayLen + (sizeof(unsigned short) * 2), sizeof(unsigned short));
		memcpy(&readData, buf + lenBeforeArray + arrayLen + (sizeof(unsigned short) * 3), sizeof(byte));
		data.pointsArr[x][y][z].data = readData;
		remaining -= pointSize;
		arrayLen += pointSize;
	}

	free(buf);

	int endTime = trap_Milliseconds();
	Com_Printf("done (took %0.3f seconds).\n", ((float)(endTime - startTime)) / 1000.0f);
	return qtrue;
}

static void GenerateLineOfSightLocations(void) {
	memset(&data, 0, sizeof(data));

	data.protocol = ENHANCED_LOCATION_PROTOCOL;
	data.incrementSize = level.locationAccuracy;

	// iterate through all locationable entities, saving their names+owners and determining the mins/maxs of the map
	float lowest[3] = { 9999999 }, highest[3] = { -9999999 };
	int numUnique = 1, numAutoGenerated = 0, numInfo_b_e_locations = 0;
	Q_strncpyz(data.locationNames[pitLocationIndex], "Pit", sizeof(data.locationNames[pitLocationIndex]));
	Com_Printf("Got location[0] for Pit\n");
	if (!level.generateLocationsWithInfo_b_e_locationsOnly) {
		for (int i = MAX_CLIENTS; i < ENTITYNUM_MAX_NORMAL; i++) {
			gentity_t *ent = &g_entities[i];
			/*if (VALIDSTRING(ent->classname))
				Com_Printf("Entity %d is %s\n", i, ent->classname);*/
			enhancedLocation_t loc = { 0 };
			if (!MakeEnhancedLocation(ent, &loc))
				continue;

			++numAutoGenerated;

			// see if we already have a location name+owner saved for it
			qboolean gotLocation = qfalse;
			for (int index = 0; index < numUnique; ++index) {
				if (!strcmp(loc.message, data.locationNames[index]) && (byte)loc.teamowner == data.teamOwner[index]) {
					gotLocation = qtrue;
					break;
				}
			}

			// doesn't already exist, so copy the info to a new one
			if (!gotLocation) {
				if (numUnique + 1 > MAX_LOCATIONS) {
					Com_Printf("Unique location limit hit! Unable to add more unique locations.\n");
				}
				else {
					Q_strncpyz(data.locationNames[numUnique], loc.message, sizeof(data.locationNames[numUnique]));
					data.teamOwner[numUnique] = (byte)loc.teamowner;
					Com_Printf("Got location[%d] from entity #%d: %s%s\n", numUnique, i, loc.teamowner ? (loc.teamowner == TEAM_RED ? "Red " : "Blue ") : "", loc.message);
					++numUnique;
				}
			}

			// check mins/maxs
			for (int j = 0; j < 3; j++) {
				if (ent->r.currentOrigin[j] < lowest[j])
					lowest[j] = ent->r.currentOrigin[j];
				if (ent->r.currentOrigin[j] > highest[j])
					highest[j] = ent->r.currentOrigin[j];
			}
		}
	}

	iterator_t iter;
	ListIterate(&level.info_b_e_locationsList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		info_b_e_location_listItem_t *listItem = (info_b_e_location_listItem_t *)IteratorNext(&iter);

		++numInfo_b_e_locations;

		// see if we already have a location name+owner saved for it
		qboolean gotLocation = qfalse;
		for (int index = 0; index < numUnique; ++index) {
			if (!strcmp(listItem->message, data.locationNames[index]) && (byte)listItem->teamowner == data.teamOwner[index]) {
				gotLocation = qtrue;
				break;
			}
		}

		// doesn't already exist, so copy the info to a new one
		if (!gotLocation) {
			if (numUnique + 1 > MAX_LOCATIONS) {
				Com_Printf("Unique location limit hit! Unable to add more unique locations.\n");
			}
			else {
				Q_strncpyz(data.locationNames[numUnique], listItem->message, sizeof(data.locationNames[numUnique]));
				data.teamOwner[numUnique] = (byte)listItem->teamowner;
				Com_Printf("Got location[%d] from info_b_e_location: %s%s\n", numUnique, listItem->teamowner ? (listItem->teamowner == TEAM_RED ? "Red " : "Blue ") : "", listItem->message);
				++numUnique;
			}
		}

		// check mins/maxs
		for (int j = 0; j < 3; j++) {
			if (listItem->origin[j] < lowest[j])
				lowest[j] = listItem->origin[j];
			if (listItem->origin[j] > highest[j])
				highest[j] = listItem->origin[j];
		}
	}

	if (!numUnique) {
		Com_Printf("Unable to get any locations!\n");
		return;
	}

	Com_Printf("Got %d unique locations from pit, %d entities and %d info_b_e_locations.\n", numUnique, numAutoGenerated, numInfo_b_e_locations);

	// set the mins and dimension sizes
	for (int i = 0; i < 3; i++) {
		const float buffer = MAX_LINE_OF_SIGHT_DISTANCE;
		if (i == 2 && level.pitHeight.valid && lowest[2] > level.pitHeight.value) {
			// don't bother computing tons of points that are just going to be pits; one layer is enough
			// set mins[2] such that we have one layer under the pit and we have a layer exactly at lowest[2]
			// e.g. with lowest[2] of 260 and pit height of 200, mins[2] will be 160
			data.mins[2] = lowest[2] - (ceil((lowest[2] - level.pitHeight.value) / data.incrementSize) * data.incrementSize);
			if (fabs(data.mins[2] - level.pitHeight.value) < 0.001f)
				data.mins[2] -= data.incrementSize;
		}
		else {
			data.mins[i] = lowest[i] - buffer;
		}
		float max = highest[i] + buffer;

		float dimensionSizeDiff = max - data.mins[i];
		data.dimensionSize[i] = (int)(dimensionSizeDiff / data.incrementSize);
		if (data.dimensionSize[i] > USHRT_MAX)
			data.dimensionSize[i] = USHRT_MAX;

		Com_Printf("%c dimension: highest %0.3f, lowest %0.3f, buffer %0.3f, min %0.3f, max %0.3f, dimension size diff %0.3f, dimension size %d\n",
			i == 0 ? 'x' : (i == 1 ? 'y' : 'z'),
			highest[i], lowest[i], buffer, data.mins[i], max, dimensionSizeDiff, data.dimensionSize[i]);
	}
	if (!AllocatePointArray()) {
		Com_Printf("Failed to allocate memory for locations!\n");
		return;
	}

	// get the best nearby entity (hopefully in line of sight) for each point in 3d space
	Com_Printf("Computing locations. Percent finished: ");
	int startTime = trap_Milliseconds();
	uint64_t nonSolidPoints = 0;
	for (int i = 0; i < data.dimensionSize[0]; i++) {
		// print status percent
		static int lastPercentComplete = -1;
		int percentComplete = (int)(((float)i / (float)(data.dimensionSize[0] - 1.0f)) * 100.0f);
		if (percentComplete != lastPercentComplete) {
			if (percentComplete % 10 == 0)
				Com_Printf("%d", percentComplete);
			else
				Com_Printf(".");
			lastPercentComplete = percentComplete;
		}

		for (int j = 0; j < data.dimensionSize[1]; j++) {
			for (int k = 0; k < data.dimensionSize[2]; k++) {
				enhancedLocationPoint_t *arrPoint = &data.pointsArr[i][j][k];

				float z = data.mins[2] + (k * data.incrementSize);
				if (level.pitHeight.valid && z < level.pitHeight.value) {
					SetPointIndex(arrPoint, pitLocationIndex);
					++nonSolidPoints;
					continue;
				}

				float x = data.mins[0] + (i * data.incrementSize);
				float y = data.mins[1] + (j * data.incrementSize);

				// check to see if this point is inside a wall
				vec3_t point = { x, y, z };
				if (StartsSolid(point)) {
					MakePointStartSolid(arrPoint);
					continue;
				}

				// we actually check from a point a bit above your head
				trace_t tr;
				float up[3] = { x, y, z + Z_AXIS_BOOST };
				trap_Trace(&tr, point, NULL, NULL, up, ENTITYNUM_NONE, MASK_SOLID);

				// get the best location for this point
				enhancedLocation_t loc = { 0 };
				qboolean gotEnt = ClosestEntityInLineOfSight(point, tr.endpos, &loc);
				if (!gotEnt) { // nothing in line of sight; try again just by proximity
					gotEnt = ClosestEntity(point, tr.endpos, &loc);
					if (!gotEnt) {
						DeletePoint(arrPoint);
						continue; // still no valid ent??? should not be possible
					}
				}

				// find the index where the entity's name and owner was saved earlier and link the point to it
				for (int index = 0; index < numUnique; ++index) {
					if (!strcmp(loc.message, data.locationNames[index]) && (byte)loc.teamowner == data.teamOwner[index]) {
						SetPointIndex(arrPoint, index);
						++nonSolidPoints;
						goto dontDeletePoint;
					}
				}
				
				// sanity check: if we got here, we didn't find an index (shouldn't happen)
				assert(qfalse);
				DeletePoint(arrPoint);

				dontDeletePoint:;
			}
		}
	}

	int endTime = trap_Milliseconds();
	Com_Printf(" done (took %0.3f seconds).\n", ((float)(endTime - startTime)) / 1000.0f);

	// optimize by deleting redundant points (ugly code for performance)
	Com_Printf("Optimizing...");
	uint64_t deletedPoints = 0llu;
	startTime = trap_Milliseconds();
	for (int i = 0; i < data.dimensionSize[0]; i++) {
		for (int j = 0; j < data.dimensionSize[1]; j++) {
			for (int k = 0; k < data.dimensionSize[2]; k++) {
				enhancedLocationPoint_t *point = &data.pointsArr[i][j][k];
				if (PointIsDeleted(i, j, k) || PointIsStartSolid(i, j, k))
					continue;

				qboolean pointIsUseless = (
					(PointIsValid(i - 1, j - 1, k - 1) && !PointIsStartSolid(i - 1, j - 1, k - 1) && (PointsUseSameMsg(i, j, k, i - 1, j - 1, k - 1) || PointIsDeleted(i - 1, j - 1, k - 1))) &&
					(PointIsValid(i - 1, j - 1, k) && !PointIsStartSolid(i - 1, j - 1, k) && (PointsUseSameMsg(i, j, k, i - 1, j - 1, k) || PointIsDeleted(i - 1, j - 1, k))) &&
					(PointIsValid(i - 1, j - 1, k + 1) && !PointIsStartSolid(i - 1, j - 1, k + 1) && (PointsUseSameMsg(i, j, k, i - 1, j - 1, k + 1) || PointIsDeleted(i - 1, j - 1, k + 1))) &&
					(PointIsValid(i - 1, j, k - 1) && !PointIsStartSolid(i - 1, j, k - 1) && (PointsUseSameMsg(i, j, k, i - 1, j, k - 1) || PointIsDeleted(i - 1, j, k - 1))) &&
					(PointIsValid(i - 1, j, k) && !PointIsStartSolid(i - 1, j, k) && (PointsUseSameMsg(i, j, k, i - 1, j, k) || PointIsDeleted(i - 1, j, k))) &&
					(PointIsValid(i - 1, j, k + 1) && !PointIsStartSolid(i - 1, j, k + 1) && (PointsUseSameMsg(i, j, k, i - 1, j, k + 1) || PointIsDeleted(i - 1, j, k + 1))) &&
					(PointIsValid(i - 1, j + 1, k - 1) && !PointIsStartSolid(i - 1, j + 1, k - 1) && (PointsUseSameMsg(i, j, k, i - 1, j + 1, k - 1) || PointIsDeleted(i - 1, j + 1, k - 1))) &&
					(PointIsValid(i - 1, j + 1, k) && !PointIsStartSolid(i - 1, j + 1, k) && (PointsUseSameMsg(i, j, k, i - 1, j + 1, k) || PointIsDeleted(i - 1, j + 1, k))) &&
					(PointIsValid(i - 1, j + 1, k + 1) && !PointIsStartSolid(i - 1, j + 1, k + 1) && (PointsUseSameMsg(i, j, k, i - 1, j + 1, k + 1) || PointIsDeleted(i - 1, j + 1, k + 1))) &&
					(PointIsValid(i, j - 1, k - 1) && !PointIsStartSolid(i, j - 1, k - 1) && (PointsUseSameMsg(i, j, k, i, j - 1, k - 1) || PointIsDeleted(i, j - 1, k - 1))) &&
					(PointIsValid(i, j - 1, k) && !PointIsStartSolid(i, j - 1, k) && (PointsUseSameMsg(i, j, k, i, j - 1, k) || PointIsDeleted(i, j - 1, k))) &&
					(PointIsValid(i, j - 1, k + 1) && !PointIsStartSolid(i, j - 1, k + 1) && (PointsUseSameMsg(i, j, k, i, j - 1, k + 1) || PointIsDeleted(i, j - 1, k + 1))) &&
					(PointIsValid(i, j, k - 1) && !PointIsStartSolid(i, j, k - 1) && (PointsUseSameMsg(i, j, k, i, j, k - 1) || PointIsDeleted(i, j, k - 1))) &&
					/*(PointIsValid(i, j, k) && !PointIsStartSolid(i, j, k) && (PointsUseSameMsg(i, j, k, i, j, k) || PointIsDeleted(i, j, k))) &&*/
					(PointIsValid(i, j, k + 1) && !PointIsStartSolid(i, j, k + 1) && (PointsUseSameMsg(i, j, k, i, j, k + 1) || PointIsDeleted(i, j, k + 1))) &&
					(PointIsValid(i, j + 1, k - 1) && !PointIsStartSolid(i, j + 1, k - 1) && (PointsUseSameMsg(i, j, k, i, j + 1, k - 1) || PointIsDeleted(i, j + 1, k - 1))) &&
					(PointIsValid(i, j + 1, k) && !PointIsStartSolid(i, j + 1, k) && (PointsUseSameMsg(i, j, k, i, j + 1, k) || PointIsDeleted(i, j + 1, k))) &&
					(PointIsValid(i, j + 1, k + 1) && !PointIsStartSolid(i, j + 1, k + 1) && (PointsUseSameMsg(i, j, k, i, j + 1, k + 1) || PointIsDeleted(i, j + 1, k + 1))) &&
					(PointIsValid(i + 1, j - 1, k - 1) && !PointIsStartSolid(i + 1, j - 1, k - 1) && (PointsUseSameMsg(i, j, k, i + 1, j - 1, k - 1) || PointIsDeleted(i + 1, j - 1, k - 1))) &&
					(PointIsValid(i + 1, j - 1, k) && !PointIsStartSolid(i + 1, j - 1, k) && (PointsUseSameMsg(i, j, k, i + 1, j - 1, k) || PointIsDeleted(i + 1, j - 1, k))) &&
					(PointIsValid(i + 1, j - 1, k + 1) && !PointIsStartSolid(i + 1, j - 1, k + 1) && (PointsUseSameMsg(i, j, k, i + 1, j - 1, k + 1) || PointIsDeleted(i + 1, j - 1, k + 1))) &&
					(PointIsValid(i + 1, j, k - 1) && !PointIsStartSolid(i + 1, j, k - 1) && (PointsUseSameMsg(i, j, k, i + 1, j, k - 1) || PointIsDeleted(i + 1, j, k - 1))) &&
					(PointIsValid(i + 1, j, k) && !PointIsStartSolid(i + 1, j, k) && (PointsUseSameMsg(i, j, k, i + 1, j, k) || PointIsDeleted(i + 1, j, k))) &&
					(PointIsValid(i + 1, j, k + 1) && !PointIsStartSolid(i + 1, j, k + 1) && (PointsUseSameMsg(i, j, k, i + 1, j, k + 1) || PointIsDeleted(i + 1, j, k + 1))) &&
					(PointIsValid(i + 1, j + 1, k - 1) && !PointIsStartSolid(i + 1, j + 1, k - 1) && (PointsUseSameMsg(i, j, k, i + 1, j + 1, k - 1) || PointIsDeleted(i + 1, j + 1, k - 1))) &&
					(PointIsValid(i + 1, j + 1, k) && !PointIsStartSolid(i + 1, j + 1, k) && (PointsUseSameMsg(i, j, k, i + 1, j + 1, k) || PointIsDeleted(i + 1, j + 1, k))) &&
					(PointIsValid(i + 1, j + 1, k + 1) && !PointIsStartSolid(i + 1, j + 1, k + 1) && (PointsUseSameMsg(i, j, k, i + 1, j + 1, k + 1) || PointIsDeleted(i + 1, j + 1, k + 1)))
					);


				if (pointIsUseless) { // this point is surrounded by identical or deleted points; delete it
					DeletePoint(point);
					++deletedPoints;
					continue;
				}
			}
		}
	}

	endTime = trap_Milliseconds();
	uint64_t remainingPoints = nonSolidPoints - deletedPoints;
	Com_Printf("done (took %0.3f seconds). Deleted %llu points for result of %llu points (was %llu points).\n", ((float)(endTime - startTime)) / 1000.0f, deletedPoints, remainingPoints, nonSolidPoints);

	WriteEnhancedLocationDataToDisk();
}

static void LinkLegacyLocations(void) {
	if (level.locations.legacy.num <= 0) {
		trap_kd_free();
		memset(&level.locations, 0, sizeof(level.locations));
		return;
	}

	for (int i = 0, n = 1; i < level.locations.legacy.num; i++) {
		level.locations.legacy.data[i].cs_index = n;
		trap_SetConfigstring(CS_LOCATIONS + n, level.locations.legacy.data[i].message);
		n++;
	}

	// we won't need the enhanced system
	trap_kd_free();
	memset(&level.locations.enhanced, 0, sizeof(level.locations.enhanced));

	G_Printf("Linked %d legacy locations\n", level.locations.legacy.num);
}

qboolean Location_EnhancedLocationsFileExists(void) {
	static qboolean initialized = qfalse, exists = qfalse;

	if (!initialized) {
		if (FileExists(va("maps/%s.enhancedlocations", level.mapname)))
			exists = qtrue;
		initialized = qtrue;
	}

	return exists;
}

static qboolean LinkLineOfSightLocations(void) {
	char fileName[MAX_QPATH];
	Com_sprintf(fileName, sizeof(fileName), "maps/%s.enhancedlocations", level.mapname);
	qboolean loaded = qfalse;

	if (FileExists(fileName))
		loaded = ReadEnhancedLocationDataFromDisk(fileName);

	if (!loaded) {
		if (g_lineOfSightLocations_generate.integer) {
			Com_Printf("Generating new line of sight location data for %s.\n", level.mapname);
			GenerateLineOfSightLocations();
		}
		else {
			Com_Printf("Falling back to proximity locations.\n");
			return qfalse;
		}
	}

	int startTime = trap_Milliseconds();

	Location_ResetLookupTree();

	// copy the locations for the kd tree
	for (int i = 0; i < MAX_LOCATIONS; i++) {
		if (!data.locationNames[i][0])
			break;
		enhancedLocation_t *targetLoc = trap_kd_dataptr(*trap_kd_numunique());
		*trap_kd_numunique() += 1;
		Q_strncpyz(targetLoc->message, data.locationNames[i], sizeof(targetLoc->message));
		targetLoc->teamowner = data.teamOwner[i];
	}

	// build the kd tree
	for (int i = 0; i < data.dimensionSize[0]; i++) {
		for (int j = 0; j < data.dimensionSize[1]; j++) {
			for (int k = 0; k < data.dimensionSize[2]; k++) {
				if (PointIsDeleted(i, j, k) || PointIsStartSolid(i, j, k))
					continue;

				enhancedLocationPoint_t *arrPoint = &data.pointsArr[i][j][k];

				float x = data.mins[0] + (i * data.incrementSize);
				float y = data.mins[1] + (j * data.incrementSize);
				float z = data.mins[2] + (k * data.incrementSize);
				vec3_t point = { x, y, z };

				enhancedLocation_t *targetLoc = trap_kd_dataptr(arrPoint->data - 1);
				trap_kd_insertf(point, targetLoc);

				level.locations.enhanced.numTotal++;
			}
		}
	}

	// at this point everything from data has been copied to level.locations; we can free it
	if (data.pointsArr)
		FreePointArray(data.pointsArr);

	if (*trap_kd_numunique()) {
		// use the enhanced system
		for (int i = 0, n = 1; i < *trap_kd_numunique(); ++i) {
			char *prefix;

			enhancedLocation_t *ptr = trap_kd_dataptr(i);

			// prepend the team name before the location for base clients
			switch (ptr->teamowner) {
			case TEAM_RED: prefix = "Red "; break;
			case TEAM_BLUE: prefix = "Blue "; break;
			default: prefix = "";
			}

			ptr->cs_index = n;
			trap_SetConfigstring(CS_LOCATIONS + n, va("%s%s", prefix, ptr->message));
			n++;
		}

		// we won't need the legacy system
		memset(&level.locations.legacy, 0, sizeof(level.locations.legacy));

		int endTime = trap_Milliseconds();
		G_Printf("Linked %d enhanced locations", *trap_kd_numunique());
		if ((uint64_t)*trap_kd_numunique() != level.locations.enhanced.numTotal) {
			G_Printf(" (optimized from %llu points)", level.locations.enhanced.numTotal);
		}
		G_Printf(" in %0.3f seconds\n", ((float)(endTime - startTime)) / 1000.0f);
	}
	else {
		LinkLegacyLocations();
	}

	// All linked together now
	level.locations.enhanced.usingLineOfSightLocations = qtrue;
	level.locations.linked = qtrue;
	ListClear(&level.info_b_e_locationsList);
	return qtrue;
}

/*
* ===============
* G_LinkLocations
* ===============
* if g_lineOfSightLocations is enabled:
*		if maps/mapname.enhancedlocations exists, use the locations from that file
*		else:
*			if g_lineOfSightLocations_generate is enabled, generate locations and save to disk as maps/mapname.enhancedlocations
*				by default, generates locations from both entities and info_b_e_locations
*				"locationsonly" key in worldspawn = generate locations from info_b_e_locations only
*				"locationaccuracy" key in worldspawn = space between points on any given axis
*					set lower for better accuracy at the cost of longer build time, larger disk space, and longer load time
*				"pitheight" key in worldspawn = if your z-axis origin is below this, you are in the pit
*					you can also create "Pit" locations using info_b_e_locations
*				you can also set min_x, min_y, min_z, max_x, max_y, max_z keys in info_b_e_locations
*			else:
*				if proximity system is valid, use it
*				else, use legacy system
* else:
*		if proximity system is valid, use it
*		else, use legacy system
*/
void G_LinkLocations(void) {
	if (level.locations.linked)
		return;

	trap_SetConfigstring(CS_LOCATIONS, "unknown");

	if (*trap_kd_numunique()) {
		// use the enhanced system
		for (int i = 0, n = 1; i < *trap_kd_numunique(); ++i) {
			char *prefix;

			enhancedLocation_t *ptr = trap_kd_dataptr(i);

			// prepend the team name before the location for base clients
			switch (ptr->teamowner) {
			case TEAM_RED: prefix = "Red "; break;
			case TEAM_BLUE: prefix = "Blue "; break;
			default: prefix = "";
			}

			ptr->cs_index = n;
			trap_SetConfigstring(CS_LOCATIONS + n, va("%s%s", prefix, ptr->message));
			n++;
		}

		// we won't need the legacy system
		memset(&level.locations.legacy, 0, sizeof(level.locations.legacy));

		int endTime = trap_Milliseconds();
		G_Printf("Reusing %d previously linked enhanced locations\n", *trap_kd_numunique());
		level.locations.linked = qtrue;
		return;
	}

	if (g_gametype.integer == GT_CTF && g_lineOfSightLocations.integer && LinkLineOfSightLocations())
		return;

	// line of sight locations disabled or not found; fall back to proximity system if possible

	ListClear(&level.info_b_e_locationsList);

	trap_SetConfigstring(CS_LOCATIONS, "unknown");

	if (g_gametype.integer == GT_CTF)
		GenerateProximityLocations();

	if (*trap_kd_numunique()) {
		// use the enhanced system
		for (int i = 0, n = 1; i < *trap_kd_numunique(); ++i) {
			char *prefix;

			// prepend the team name before the location for base clients
			switch (trap_kd_dataptr(i)->teamowner) {
			case TEAM_RED: prefix = "Red "; break;
			case TEAM_BLUE: prefix = "Blue "; break;
			default: prefix = "";
			}

			trap_kd_dataptr(i)->cs_index = n;
			trap_SetConfigstring(CS_LOCATIONS + n, va("%s%s", prefix, trap_kd_dataptr(i)->message));
			n++;
		}

		// we won't need the legacy system
		memset(&level.locations.legacy, 0, sizeof(level.locations.legacy));

		G_Printf("Linked %d proximity locations", *trap_kd_numunique());
		if (*trap_kd_numunique() != level.locations.enhanced.numTotal) {
			G_Printf(" (optimized from %d entities)", level.locations.enhanced.numTotal);
		}
		G_Printf("\n");
	}
	else {
		LinkLegacyLocations();
	}

	// All linked together now
	level.locations.linked = qtrue;
}

void Location_ResetLookupTree(void) {
	trap_kd_free();
	memset(&level.locations, 0, sizeof(level.locations));
	trap_kd_create();
}

/*
===========
Team_GetLocation

Report a location for the player. May use different systems as defined in G_LinkLocations.

Returns the configstring index of the location, or 0 if no location could be found.
If locationBuffer is not NULL, the location string will be directly written there.
============
*/
int Team_GetLocation(gentity_t *ent, char *locationBuffer, size_t locationBufferSize) {
	vec3_t origin;
	VectorCopy(ent->r.currentOrigin, origin);

	if (locationBuffer) {
		locationBuffer[0] = '\0';
	}

	if (*trap_kd_numunique()) {
		// using enhanced locations
		int		resultIndex = 0;
		void *nearest;

		// we should always have at most 1 result
		nearest = trap_kd_nearestf(origin);

		if (nearest && kd_res_size(nearest) == 1) {
			enhancedLocation_t *loc;
			if (ent->client && ent->client->ps.fallingToDeath && pitLocationIndex != -1)
				loc = trap_kd_dataptr(pitLocationIndex);
			else
				loc = (enhancedLocation_t *)kd_res_item_data(nearest);

			if (loc) {
				if (locationBuffer && ent->client) {
					// we aren't writing to configstrings here, so we can format the team dynamically
					if (loc->teamowner) {
						if (ent->client->ps.persistant[PERS_TEAM] == loc->teamowner) {
							Com_sprintf(locationBuffer, locationBufferSize, "Our %s", loc->message);
						}
						else {
							Com_sprintf(locationBuffer, locationBufferSize, "Enemy %s", loc->message);
						}
					}
					else {
						Q_strncpyz(locationBuffer, loc->message, locationBufferSize);
					}
				}

				resultIndex = loc->cs_index;
			}
		}

		if (nearest)
			trap_kd_res_free(nearest);

		return resultIndex;
	}
	else if (level.locations.legacy.num) {
		// using legacy locations
		legacyLocation_t *loc, *best;
		vec_t				bestlen, len;
		int					i;

		best = NULL;
		bestlen = 3 * 8192.0 * 8192.0;

		for (i = 0; i < level.locations.legacy.num; i++) {
			loc = &level.locations.legacy.data[i];

			len = DistanceSquared(origin, loc->origin);

			if (len > bestlen) {
				continue;
			}

			if (!trap_InPVS(origin, loc->origin)) {
				continue;
			}

			bestlen = len;
			best = loc;
		}

		if (best) {
			if (locationBuffer) {
				if (best->count) {
					Com_sprintf(locationBuffer, locationBufferSize, "%c%c%s" S_COLOR_WHITE, Q_COLOR_ESCAPE, best->count + '0', best->message);
				}
				else {
					Com_sprintf(locationBuffer, locationBufferSize, "%s", best->message);
				}
			}

			return best->cs_index;
		}
	}

	return 0;
}
