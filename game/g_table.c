#include "g_local.h"
#include "g_database.h"

#define TABLE_ROW_SIZE	(1024)
#define TABLE_MAX_COLUMNS	(32)
typedef struct {
	node_t			node;
	void *context;
	char			str[TABLE_MAX_COLUMNS][128];
} RowData;
typedef struct {
	node_t			node;
	char			title[128];
	qboolean		leftAlign;
	int				maxLen;
	int				longestPrintLen;
	int				columnId;
} ColumnData;

void listMapsInPools(void *context, const char *long_name, int pool_id, const char *mapname, int mapWeight) {
	list_t *mapList = (list_t *)context;
	poolMap_t *thisMap = ListAdd(mapList, sizeof(poolMap_t));

	if (VALIDSTRING(mapname))
		Q_strncpyz(thisMap->mapname, mapname, sizeof(thisMap->mapname));
	thisMap->weight = mapWeight;
}

const char *TableCallback_MapName(void *context) {
	poolMap_t *map = context;
	return map->mapname;
}

const char *TableCallback_MapWeight(void *context) {
	poolMap_t *map = context;
	return va("%d", map->weight);
}

const char *TableCallback_PoolShortName(void *context) {
	pool_t *pool = context;
	return pool->shortName;
}

const char *TableCallback_PoolLongName(void *context) {
	pool_t *pool = context;
	return pool->longName;
}

void listPools(void *context, int pool_id, const char *short_name, const char *long_name) {
	list_t *poolList = (list_t *)context;
	pool_t *thisPool = ListAdd(poolList, sizeof(pool_t));

	if (VALIDSTRING(short_name))
		Q_strncpyz(thisPool->shortName, short_name, sizeof(thisPool->shortName));
	if (VALIDSTRING(long_name))
		Q_strncpyz(thisPool->longName, long_name, sizeof(thisPool->longName));
}

const char *TableCallback_ClientNum(void *context) {
	gclient_t *cl = context;
	const char *teamColor;

	if (cl->pers.connected == CON_CONNECTING) teamColor = "^9";
	else if (cl->sess.sessionTeam == TEAM_SPECTATOR) teamColor = "^7";
	else if (cl->sess.sessionTeam == TEAM_RED) teamColor = "^1";
	else if (cl->sess.sessionTeam == TEAM_BLUE) teamColor = "^4";
	else teamColor = "^3";

	return va("%s%d", teamColor, cl - level.clients);
}

const char *TableCallback_Name(void *context) {
	gclient_t *cl = context;
	return va("^7%s", cl->pers.netname);
}

typedef struct {
	char	buf[64];
} AliasContext;

void AliasPrint(void *context, const char *name, int duration) {
	AliasContext *writeContext = (AliasContext *)context;
	Q_strncpyz(writeContext->buf, va("^7%s", name), sizeof(writeContext->buf));
}

const char *TableCallback_Alias(void *context) {
	gclient_t *cl = context;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT)
		return NULL;

	// use the account name if they have one
	if (cl->account && cl->account->name[0])
		return cl->account->name;

	// fall back to the old nicknames system otherwise
	AliasContext nicknameContext = { 0 };
	G_DBListAliases(cl->sess.ip, 0xFFFFFFFF, 1, AliasPrint, &nicknameContext, cl->sess.auth == AUTHENTICATED ? cl->sess.cuidHash : "");
	return va("%s", nicknameContext.buf);
}

const char *TableCallback_Ping(void *context) {
	gclient_t *cl = context;
	gentity_t *ent = &g_entities[cl - level.clients];

	if (ent->r.svFlags & SVF_BOT)
		return "Bot";
	if (cl->pers.connected == CON_CONNECTING)
		return "----";
	else
		return va("%i", cl->realPing > 9999 ? 9999 : cl->realPing);
}

const char *TableCallback_Score(void *context) {
	gclient_t *cl = context;
	if (cl->pers.connected == CON_CONNECTING)
		return NULL;
	return va("%d", cl->ps.persistant[PERS_SCORE]);
}

const char *TableCallback_IP(void *context) {
	gclient_t *cl = context;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT)
		return NULL;
	return cl->sess.ipString;
}

const char *TableCallback_Qport(void *context) {
	gclient_t *cl = context;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT || !cl->sess.qport)
		return NULL;
	return va("%d", cl->sess.qport);
}

const char *TableCallback_Country(void *context) {
	gclient_t *cl = context;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT)
		return NULL;
	return cl->sess.country;
}

const char *TableCallback_Mod(void *context) {
	gclient_t *cl = context;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT)
		return NULL;
	char userinfo[MAX_INFO_STRING] = { 0 };
	trap_GetUserinfo(cl - level.clients, userinfo, sizeof(userinfo));
	qboolean modernEngine = !!(*Info_ValueForKey(userinfo, "ja_guid"));
	char *nmVer = Info_ValueForKey(userinfo, "nm_ver");
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

const char *TableCallback_Shadowmuted(void *context) {
	gclient_t *cl = context;
	gentity_t *ent = &g_entities[cl - level.clients];
	if (ent->r.svFlags & SVF_BOT || !cl->sess.shadowMuted)
		return NULL;
	return "Shadowmuted";
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
}

// defines a column and how data will be procured for it
void Table_DefineColumn(Table *t, const char *title, ColumnDataCallback callback, qboolean leftAlign, int maxLen) {
	assert(t && VALIDSTRING(title) && callback);
	ColumnData *column = ListAdd(&t->columnList, sizeof(ColumnData));
	qboolean gotOne = qfalse;

	column->columnId = ++t->lastColumnIdAssigned;

	Q_strncpyz(column->title, title, sizeof(column->title));
	column->leftAlign = leftAlign;
	column->maxLen = maxLen;
	column->longestPrintLen = Q_PrintStrlen(title);

	iterator_t iter;
	ListIterate(&t->rowList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		RowData *row = (RowData *)IteratorNext(&iter);
		const char *result = callback(row->context);
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
static void WriteColumnHeader(Table *t, char *buf, size_t bufSize) {
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
	}
	Q_strcat(buf, bufSize, "\n");

	// print the minus signs
	ListIterate(&t->columnList, &iter, qfalse);
	Q_strcat(buf, bufSize, "^9");
	while (IteratorHasNext(&iter)) {
		ColumnData *column = (ColumnData *)IteratorNext(&iter);
		for (int i = 0; i < column->longestPrintLen; i++)
			Q_strcat(buf, bufSize, "-");
		Q_strcat(buf, bufSize, " ");
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
	}
	else { // " text nextThing"
		int numSpaces = (column->longestPrintLen - printLen);
		for (int i = 0; i < numSpaces; i++)
			Q_strcat(buf, bufSize, " ");
		Q_strcat(buf, bufSize, grey ? "^9" : "^7");
		if (VALIDSTRING(row->str[column->columnId]))
			Q_strcat(buf, bufSize, row->str[column->columnId]);
		if (!lastColumn)
			Q_strcat(buf, bufSize, " ");
	}
}

static qboolean ColumnIdMatches(genericNode_t *node, void *userData) {
	const ColumnData *column = (const ColumnData *)node;
	const int check = *((const int *)userData);
	return column->columnId == check;
}

void Table_WriteToBuffer(Table *t, char *buf, size_t bufSize) {
	assert(t && buf && bufSize);

	// write the header
	WriteColumnHeader(t, buf, bufSize);

	// print each row
	qboolean grey = qfalse;
	iterator_t iter;
	ListIterate(&t->rowList, &iter, qfalse);
	while (IteratorHasNext(&iter)) {
		RowData *row = (RowData *)IteratorNext(&iter);
		for (int i = 0; i <= t->lastColumnIdAssigned && i < TABLE_MAX_COLUMNS; i++) {
			ColumnData *column = ListFind(&t->columnList, ColumnIdMatches, &i, NULL);
			if (!column)
				break;
			WriteCell(t, row, column, !!(grey && t->alternateColors), buf, bufSize);
		}
		Q_strcat(buf, bufSize, "^7\n");
		grey = !grey;
	}
}