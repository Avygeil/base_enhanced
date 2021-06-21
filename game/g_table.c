#include "g_local.h"
#include "g_database.h"

#define TABLE_ROW_SIZE	(1024)
#define TABLE_MAX_COLUMNS	(32)
typedef struct {
	node_t			node;
	void *context;
	char			str[TABLE_MAX_COLUMNS][128];
	qboolean		isHorizontalRule;
	int				customHorizontalRuleColor;
} RowData;
typedef struct {
	node_t			node;
	char			title[128];
	qboolean		leftAlign;
	int				maxLen;
	int				longestPrintLen;
	int				columnId;
	int				dividerColor;
} ColumnData;

void listMapsInPools(void **context, const char *long_name, int pool_id, const char *mapname, int mapWeight) {
	list_t *mapList = *context;
	if (!mapList) {
		assert(qfalse);
		return;
	}
	poolMap_t *thisMap = ListAdd(mapList, sizeof(poolMap_t));

	if (VALIDSTRING(mapname))
		Q_strncpyz(thisMap->mapname, mapname, sizeof(thisMap->mapname));
	thisMap->weight = mapWeight;
}

const char *TableCallback_MapName(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	poolMap_t *map = rowContext;
	return map->mapname;
}

const char *TableCallback_MapWeight(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	poolMap_t *map = rowContext;
	return va("%d", map->weight);
}

const char *TableCallback_PoolShortName(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	pool_t *pool = rowContext;
	return pool->shortName;
}

const char *TableCallback_PoolLongName(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	pool_t *pool = rowContext;
	return pool->longName;
}

void listPools(void *context, int pool_id, const char *short_name, const char *long_name) {
	if (!context) {
		assert(qfalse);
		return;
	}
	list_t *poolList = (list_t *)context;
	pool_t *thisPool = ListAdd(poolList, sizeof(pool_t));

	if (VALIDSTRING(short_name))
		Q_strncpyz(thisPool->shortName, short_name, sizeof(thisPool->shortName));
	if (VALIDSTRING(long_name))
		Q_strncpyz(thisPool->longName, long_name, sizeof(thisPool->longName));
}

const char *TableCallback_ClientNum(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	const char *teamColor;

	if (cl->pers.connected == CON_CONNECTING) teamColor = "^9";
	else if (cl->sess.sessionTeam == TEAM_SPECTATOR) teamColor = "^7";
	else if (cl->sess.sessionTeam == TEAM_RED) teamColor = "^1";
	else if (cl->sess.sessionTeam == TEAM_BLUE) teamColor = "^4";
	else teamColor = "^3";

	return va("%s%d", teamColor, cl - level.clients);
}

const char *TableCallback_Name(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	return va("^7%s", cl->pers.netname);
}

const char *TableCallback_Account(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT)
		return NULL;
	if (cl->account && cl->account->name[0])
		return "^2|";
	if (cl->session)
		return "^3|";
	if (cl->pers.connected != CON_CONNECTED)
		return "?";
	return "^3|";
}

typedef struct {
	char	buf[64];
} AliasContext;

void AliasPrint(void *context, const char *name, int duration) {
	if (!context) {
		assert(qfalse);
		return;
	}
	AliasContext *writeContext = (AliasContext *)context;
	Q_strncpyz(writeContext->buf, va("^7%s", name), sizeof(writeContext->buf));
}

const char *TableCallback_Alias(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT)
		return NULL;

	// use the account name if they have one
	if (cl->account && cl->account->name[0])
		return cl->account->name;

	// fall back to the old nicknames system otherwise
	if (!cl->session)
		return NULL;
	nicknameEntry_t nickname = { 0 };
	G_DBGetTopNickname(cl->session->id, &nickname);
	return va("%s", nickname.name);
}

const char *TableCallback_Ping(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	gentity_t *ent = &g_entities[cl - level.clients];

	if (ent->r.svFlags & SVF_BOT)
		return "Bot";
	if (cl->pers.connected == CON_CONNECTING)
		return "----";
	else
		return va("%i", cl->realPing > 9999 ? 9999 : cl->realPing);
}

const char *TableCallback_Score(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	if (cl->pers.connected == CON_CONNECTING)
		return NULL;
	if (cl->sess.clientType == CLIENT_TYPE_JKCHAT)
		return NULL;
	return va("%d", cl->ps.persistant[PERS_SCORE]);
}

const char *TableCallback_IP(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT)
		return NULL;
	return cl->sess.ipString;
}

const char *TableCallback_Qport(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT || !cl->sess.qport)
		return NULL;
	return va("%d", cl->sess.qport);
}

const char *TableCallback_Country(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT)
		return NULL;
	return cl->sess.country;
}

const char *TableCallback_Mod(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT)
		return NULL;
	char userinfo[MAX_INFO_STRING] = { 0 };
	trap_GetUserinfo(cl - level.clients, userinfo, sizeof(userinfo));
	qboolean modernEngine = !!(*Info_ValueForKey(userinfo, "ja_guid"));
	char *nmVer = Info_ValueForKey(userinfo, "nm_ver");
	if (cl->sess.clientType == CLIENT_TYPE_JKCHAT)
		return "JKChat";
	if (VALIDSTRING(nmVer)) {
		if (cl->pers.connected == CON_CONNECTING)
			return va("NM%s %s?", modernEngine ? "+NJK" : "", nmVer);
		else if (cl->sess.auth == AUTHENTICATED)
			return va("NM%s %s", modernEngine ? "+NJK" : "", nmVer);
		else if (cl->sess.auth < AUTHENTICATED)
			return va("^3NM%s %s (%d)", modernEngine ? "+NJK" : "", nmVer, cl->sess.auth);
		else
			return va("^1NM%s %s", modernEngine ? "+NJK" : "", nmVer);
	}
	char *smodVer = Info_ValueForKey(userinfo, "smod_ver");
	if (VALIDSTRING(smodVer)) {
		return va("SM%s %s", modernEngine ? "+?JK" : "", smodVer);
	}
	return va("Base%s", modernEngine ? "+?JK" : "");
}

const char *TableCallback_Shadowmuted(void *rowContext, void *columnContext) {
	if (!rowContext) {
		assert(qfalse);
		return NULL;
	}
	gclient_t *cl = rowContext;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT || !cl->sess.shadowMuted)
		return NULL;
	return "Shadowmuted";
}

const char *TableCallback_Damage(void *rowContext, void *columnContext) {
	if (!rowContext || !columnContext)
		return NULL;
	gentity_t *rowPlayer = rowContext;
	gentity_t *columnPlayer = columnContext;
	return va("%d", rowPlayer->client->pers.damageCausedToPlayer[columnPlayer - g_entities]);
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

#define DamageAmount(mod) (modcContext->damageTaken ? otherPlayer->client->pers.damageCausedToPlayerOfType[tablePlayerNum][mod] : tablePlayer->client->pers.damageCausedToPlayerOfType[otherPlayerNum][mod])

const char *TableCallback_WeaponDamage(void *rowContext, void *columnContext) {
	if (!rowContext)
		return NULL;
	gentity_t *otherPlayer = rowContext;
	int otherPlayerNum = otherPlayer - g_entities;
	meansOfDeathCategoryContext_t *modcContext = columnContext;
	
	int damage = 0;
	gentity_t *tablePlayer = &g_entities[modcContext->tablePlayerClientNum];
	int tablePlayerNum = modcContext->tablePlayerClientNum;
	switch (modcContext->modc) {
	case MODC_MELEESTUNBATON:
		damage += DamageAmount(MOD_STUN_BATON);
		damage += DamageAmount(MOD_MELEE);
		break;
	case MODC_SABER:
		damage += DamageAmount(MOD_SABER);
		break;
	case MODC_PISTOL:
		damage += DamageAmount(MOD_BRYAR_PISTOL);
		damage += DamageAmount(MOD_BRYAR_PISTOL_ALT);
		break;
	case MODC_BLASTER:
		damage += DamageAmount(MOD_BLASTER);
		break;
	case MODC_DISRUPTOR:
		damage += DamageAmount(MOD_DISRUPTOR);
		damage += DamageAmount(MOD_DISRUPTOR_SNIPER);
		break;
	case MODC_BOWCASTER:
		damage += DamageAmount(MOD_BOWCASTER);
		break;
	case MODC_REPEATER:
		damage += DamageAmount(MOD_REPEATER);
		damage += DamageAmount(MOD_REPEATER_ALT);
		damage += DamageAmount(MOD_REPEATER_ALT_SPLASH);
		break;
	case MODC_DEMP:
		damage += DamageAmount(MOD_DEMP2);
		// todo: fix demp alt fire partially using MOD_DEMP2 for some reason
#if 1
		damage += DamageAmount(MOD_DEMP2_ALT);
#else
		altDamage += DamageAmount(MOD_DEMP2_ALT);
		showAlt = qtrue;
#endif
		break;
	case MODC_GOLAN:
		damage += DamageAmount(MOD_FLECHETTE);
		damage += DamageAmount(MOD_FLECHETTE_ALT_SPLASH);
		break;
	case MODC_ROCKET:
		damage += DamageAmount(MOD_ROCKET);
		damage += DamageAmount(MOD_ROCKET_SPLASH);
		damage += DamageAmount(MOD_ROCKET_HOMING);
		damage += DamageAmount(MOD_ROCKET_HOMING_SPLASH);
		break;
	case MODC_CONCUSSION:
		damage += DamageAmount(MOD_CONC);
		damage += DamageAmount(MOD_CONC_ALT);
		break;
	case MODC_THERMAL:
		damage += DamageAmount(MOD_THERMAL);
		damage += DamageAmount(MOD_THERMAL_SPLASH);
		break;
	case MODC_MINE:
		damage += DamageAmount(MOD_TRIP_MINE_SPLASH);
		damage += DamageAmount(MOD_TIMED_MINE_SPLASH);
		break;
	case MODC_DETPACK:
		damage += DamageAmount(MOD_DET_PACK_SPLASH);
		break;
	case MODC_FORCE:
		damage += DamageAmount(MOD_FORCE_DARK);
		break;
	case MODC_FALL:
		damage += DamageAmount(MOD_FALLING);
		damage += DamageAmount(MOD_CRUSH);
		damage += DamageAmount(MOD_TRIGGER_HURT);
		break;
	case MODC_TOTAL:
		damage += (modcContext->damageTaken ? otherPlayer->client->pers.damageCausedToPlayer[tablePlayerNum] : tablePlayer->client->pers.damageCausedToPlayer[otherPlayerNum]);
		break;
	}

	return va("%d", damage);
}

// initializes a table; use Table_Destroy when done
Table *Table_Initialize(qboolean alternateColors) {
	Table *t = malloc(sizeof(Table));
	memset(t, 0, sizeof(Table));
	t->lastColumnIdAssigned = -1;
	t->alternateColors = alternateColors;
	return t;
}

// frees a table
void Table_Destroy(Table *t) {
	assert(t);
	ListClear(&t->rowList);
	ListClear(&t->columnList);
	free(t);
}

// defines a row; all calls to this should take place before any Table_DefineColumn calls
void Table_DefineRow(Table *t, void *context) {
	assert(t);
	RowData *row = ListAdd(&t->rowList, sizeof(RowData));
	row->context = context;
	row->isHorizontalRule = qfalse;
	row->customHorizontalRuleColor = -1;
}

// defines a column and how data will be procured for it
// columnContext is optional and used to provide additional data to the callback (e.g. so you don't have 32 different functions to cover every player)
// specify a dividerColor if you want a | line to appear to the right of the column (if additional columns to the right are present)
void Table_DefineColumn(Table *t, const char *title, ColumnDataCallback callback, void *columnContext, qboolean leftAlign, int dividerColor, int maxLen) {
	assert(t && callback);
	ColumnData *column = ListAdd(&t->columnList, sizeof(ColumnData));
	qboolean gotOne = qfalse;

	column->columnId = ++t->lastColumnIdAssigned;

	Q_strncpyz(column->title, title, sizeof(column->title));
	column->leftAlign = leftAlign;
	column->maxLen = maxLen;
	column->longestPrintLen = Q_PrintStrlen(title);
	if (dividerColor >= 0 && dividerColor <= 9)
		column->dividerColor = dividerColor;
	else
		column->dividerColor = -1;

	iterator_t iter;
	ListIterate(&t->rowList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		RowData *row = (RowData *)IteratorNext(&iter);
		if (row->isHorizontalRule)
			continue;
		const char *result = callback(row->context, columnContext);
		if (VALIDSTRING(result)) {
			Q_strncpyz(row->str[column->columnId], result, sizeof(row->str[column->columnId]));
			int printLen = Q_PrintStrlen(row->str[column->columnId]);
			if (printLen > column->longestPrintLen)
				column->longestPrintLen = printLen;
			gotOne = qtrue;
		}
	}

	// if we didn't get a single result for this column, delete the column entirely
	if (!gotOne) {
		ListRemove(&t->columnList, column);
		--t->lastColumnIdAssigned;
	}
}

// writes two lines: the titles of each column (Name, Alias, etc) and the line of ------ spam underneath it
static void WriteColumnHeader(Table *t, char *buf, size_t bufSize, int customHeaderColor) {
	assert(t && buf && bufSize);
	iterator_t iter;

	// print the titles
	ListIterate(&t->columnList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		ColumnData *column = (ColumnData *)IteratorNext(&iter);
		Q_strcat(buf, bufSize, va("%s", column->title));
		int printLen = Q_PrintStrlen(column->title);
		int numSpaces = (column->longestPrintLen - Q_PrintStrlen(column->title)) + 1;
		for (int i = 0; i < numSpaces; i++)
			Q_strcat(buf, bufSize, " ");

		if (IteratorHasNext(&iter) && column->dividerColor >= 0 && column->dividerColor <= 9) {
			Q_strcat(buf, bufSize, va("^%d| ^7", column->dividerColor));
		}
	}
	Q_strcat(buf, bufSize, "\n");

	// print the minus signs
	ListIterate(&t->columnList, &iter, qfalse);
	if (customHeaderColor >= 0 && customHeaderColor <= 9)
		Q_strcat(buf, bufSize, va("^%d", customHeaderColor));
	else
		Q_strcat(buf, bufSize, "^9");
	while (IteratorHasNext(&iter)) {
		ColumnData *column = (ColumnData *)IteratorNext(&iter);
		for (int i = 0; i < column->longestPrintLen; i++)
			Q_strcat(buf, bufSize, "-");
		Q_strcat(buf, bufSize, " ");
		if (IteratorHasNext(&iter) && column->dividerColor >= 0 && column->dividerColor <= 9) {
			if (customHeaderColor == column->dividerColor)
				Q_strcat(buf, bufSize, "| ");
			else
				Q_strcat(buf, bufSize, va("^%d| ^%d", column->dividerColor, customHeaderColor));
		}
	}
	Q_strcat(buf, bufSize, "^7\n");
}

// writes a specific cell
static void WriteCell(Table *t, RowData *row, ColumnData *column, qboolean grey, char *buf, size_t bufSize) {
	assert(t && column && buf && bufSize);
	int printLen = VALIDSTRING(row->str[column->columnId]) ? Q_PrintStrlen(row->str[column->columnId]) : 0;
	qboolean lastColumn = !!(column->columnId == t->lastColumnIdAssigned);

	if (column->leftAlign) { // "text  nextThing"
		Q_strcat(buf, bufSize, grey ? "^9" : "^7");
		if (VALIDSTRING(row->str[column->columnId]))
			Q_strcat(buf, bufSize, row->str[column->columnId]);
		if (!lastColumn) {
			int numSpaces = (column->longestPrintLen - printLen) + 1;
			for (int i = 0; i < numSpaces; i++)
				Q_strcat(buf, bufSize, " ");
		}
		if (!lastColumn) {
			if (column->dividerColor >= 0 && column->dividerColor <= 9)
				Q_strcat(buf, bufSize, va("^%d| ", column->dividerColor));
			else
				Q_strcat(buf, bufSize, " ");
		}
	}
	else { // " text nextThing"
		int numSpaces = (column->longestPrintLen - printLen);
		for (int i = 0; i < numSpaces; i++)
			Q_strcat(buf, bufSize, " ");
		Q_strcat(buf, bufSize, grey ? "^9" : "^7");
		if (VALIDSTRING(row->str[column->columnId]))
			Q_strcat(buf, bufSize, row->str[column->columnId]);
		if (!lastColumn) {
			if (column->dividerColor >= 0 && column->dividerColor <= 9)
				Q_strcat(buf, bufSize, va(" ^%d| ", column->dividerColor));
			else
				Q_strcat(buf, bufSize, " ");
		}
	}
}

static qboolean ColumnIdMatches(genericNode_t *node, void *userData) {
	const ColumnData *column = (const ColumnData *)node;
	const int check = *((const int *)userData);
	return column->columnId == check;
}

static void WriteHorizontalRule(Table *t, char *buf, size_t bufSize, int customColor) {
	iterator_t iter;
	ListIterate(&t->columnList, &iter, qfalse);

	if (customColor >= 0 && customColor <= 9)
		Q_strcat(buf, bufSize, va("^%d", customColor));
	else
		Q_strcat(buf, bufSize, "^9");

	while (IteratorHasNext(&iter)) {
		ColumnData *column = (ColumnData *)IteratorNext(&iter);

		for (int i = 0; i < column->longestPrintLen; i++)
			Q_strcat(buf, bufSize, "-");

		Q_strcat(buf, bufSize, " ");

		if (IteratorHasNext(&iter) && column->dividerColor >= 0 && column->dividerColor <= 9) {
			if (column->dividerColor == customColor)
				Q_strcat(buf, bufSize, "| ");
			else
				Q_strcat(buf, bufSize, va("^%d| ^%d", column->dividerColor, customColor));
		}
	}
}

// adds a row consisting of automatically sized lines e.g. == ==== =========== =====
void Table_AddHorizontalRule(Table *t, int customColor) {
	assert(t);
	RowData *row = ListAdd(&t->rowList, sizeof(RowData));
	row->isHorizontalRule = qtrue;
	row->customHorizontalRuleColor = customColor;
}

// removes unnecessary colors from messages (be careful, as this includes final ^7)
// e.g. ^1^3^1^1^1hello^1^1th^4^1ere^7 ==> ^1hellothere
void CompressColors(const char *in, char *out, int outSize) {
	if (!out || !outSize)
		return;

	*out = '\0';

	if (!VALIDSTRING(in))
		return;

	char color = '7', pendingColor = '\0';
	int remaining = outSize;
	char *w = out;

	for (const char *p = in; *p; p++) {
		if (remaining == 1) // save room for null terminator
			break;

		if (Q_IsColorString(p)) { // got a new color
			char newColor = *(p + 1);
			if (newColor == color)
				pendingColor = '\0'; // redundant
			else
				pendingColor = newColor; // different; mark it as pending
			p++;
		}
		else if (*p == ' ' || *p == '\n') {
			if (remaining > 1) { // non-printable char; just copy it (no coloring)
				*w++ = *p;
				remaining--;
			}
			else {
				break;
			}
		}
		else {
			if (pendingColor && remaining > 2) { // printable char and a color is pending; apply it
				*w++ = '^';
				*w++ = pendingColor;
				color = pendingColor;
				pendingColor = '\0';
				remaining -= 2;
				if (remaining > 1) {
					*w++ = *p;
					remaining--;
				}
				else {
					break;
				}
			}
			else if (remaining > 1) { // no color pending; just copy the printable char
				*w++ = *p;
				remaining--;
			}
			else {
				break;
			}
		}
	}

	*w = '\0';
}

void Table_WriteToBuffer(Table *t, char *buf, size_t bufSize, qboolean showHeader, int customHeaderColor) {
	assert(t && buf && bufSize);

	const size_t tempSize = bufSize * 2;
	char *temp = malloc(tempSize);
	memset(temp, 0, tempSize);

	if (showHeader)
		WriteColumnHeader(t, temp, tempSize, customHeaderColor);

	// print each row
	qboolean grey = qfalse;
	iterator_t iter;
	ListIterate(&t->rowList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		RowData *row = (RowData *)IteratorNext(&iter);
		if (row->isHorizontalRule) {
			WriteHorizontalRule(t, temp, tempSize, row->customHorizontalRuleColor);
		}
		else {
			for (int i = 0; i <= t->lastColumnIdAssigned && i < TABLE_MAX_COLUMNS; i++) {
				ColumnData *column = ListFind(&t->columnList, ColumnIdMatches, &i, NULL);
				if (!column)
					break;
				WriteCell(t, row, column, !!(grey && t->alternateColors), temp, tempSize);
			}
			grey = !grey;
		}
		Q_strcat(temp, tempSize, "^7\n");
	}

	CompressColors(temp, buf, bufSize);
	free(temp);

	Q_strcat(buf, bufSize, "^7\n");
}