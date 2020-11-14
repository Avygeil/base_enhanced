#include "g_local.h"
#include "g_database.h"

#include "cJSON.h"

void G_HandleTransferResult(trsfHandle_t handle, trsfErrorInfo_t* errorInfo, int responseCode, void* data, size_t size) {
	// nothing to do here for now...
}

static void SendMatchMultipart(const char* url, const char* matchid, const char* stats, const char* payload) {
	// send the match result as a multipart discord POST form

	trsfFormPart_t multiPart[2];
	memset(&multiPart, 0, sizeof(multiPart));

	// attach ctfstats as a txt file
	multiPart[0].partName = "file";
	multiPart[0].isFile = qtrue;
	multiPart[0].filename = va("pug_%s.txt", matchid);
	multiPart[0].buf = stats;
	multiPart[0].bufSize = strlen(stats);

	// send the rest of the message as payload
	multiPart[1].partName = "payload_json";
	multiPart[1].buf = payload;
	multiPart[1].bufSize = strlen(payload);

	trap_SendMultipartPOSTRequest(NULL, url, multiPart, 2, NULL, NULL, qfalse);
}

#define DISCORD_WEBHOOK_FORMAT		"https://discordapp.com/api/webhooks/%s/%s"
#define DEMOARCHIVE_MATCH_FORMAT	"https://demos.jactf.com/match.html#rpc=lookup&id=%s"

extern const char* G_GetArenaInfoByMap(const char* map);

void G_PostScoreboardToWebhook(const char* stats) {
	if (!VALIDSTRING(g_webhookId.string) || !VALIDSTRING(g_webhookToken.string)) {
		return;
	}

	// get the score of each team
	int redScore = level.teamScores[TEAM_RED];
	int blueScore = level.teamScores[TEAM_BLUE];

	// decimal color code of the msg depending on who won
	int msgColor;
	if (redScore > blueScore) {
		msgColor = 16711680;
	} else if (blueScore > redScore) {
		msgColor = 255;
	} else {
		msgColor = -1;
	}

	// build a list of players in each team
	char redTeam[256] = { 0 }, blueTeam[256] = { 0 };
	for (team_t t = TEAM_RED; t <= TEAM_BLUE; t++) {
		iterator_t iter;
		ListIterate(t == TEAM_RED ? &level.redPlayerTickList : &level.bluePlayerTickList, &iter, qfalse);
		while (IteratorHasNext(&iter)) {
			tickPlayer_t *found = IteratorNext(&iter);

			// don't consider people who weren't ingame and on their team for at least 10% of the match
			unsigned int threshold = (int)((float)level.numTeamTicks * WEBHOOK_INGAME_THRESHOLD);
			if (found->numTicks < threshold)
				continue;

			// if he was playing for at least 10% of the match BUT less than 90% of the match, he's considered a sub
			qboolean isSub = qfalse;
			unsigned int subThreshold = (int)((float)level.numTeamTicks * WEBHOOK_INGAME_THRESHOLD_SUB);
			if (found->numTicks < subThreshold)
				isSub = qtrue;

			// is he still connected?
			qboolean isHere = qfalse;
			for (int i = 0; i < level.maxclients; i++) {
				if (level.clients[i].pers.connected != CON_CONNECTED)
					continue;
				if ((level.clients[i].account && level.clients[i].account->id == found->accountId) ||
					(level.clients[i].session && level.clients[i].session->id == found->sessionId)) {
					isHere = qtrue;
					break;
				}
			}

			// get his top name (auto-chooses best method)
			nicknameEntry_t nickname = { 0 };
			G_DBGetTopNickname(found->sessionId, &nickname);

			char *buf;
			size_t bufSize;
			switch (t) {
			case TEAM_RED: buf = redTeam; bufSize = sizeof(redTeam); break;
			case TEAM_BLUE: buf = blueTeam; bufSize = sizeof(blueTeam); break;
			default: buf = NULL; bufSize = 0;
			}

			if (VALIDSTRING(buf)) Q_strcat(buf, bufSize, "\n");
			char name[MAX_NETNAME];
			Q_strncpyz(name, nickname.name, sizeof(name));
			Q_strcat(buf, bufSize, Q_CleanStr(name));
			if (isSub)
				Q_strcat(buf, bufSize, " (SUB)");
			if (!isHere)
				Q_strcat(buf, bufSize, " (RQ)");
		}
	}

	if (!VALIDSTRING(redTeam) || !VALIDSTRING(blueTeam)) {
		return;
	}

	// get a clean string of the server name
	char serverName[64];
	trap_Cvar_VariableStringBuffer("sv_hostname", serverName, sizeof(serverName));
	Q_CleanStr(serverName);

	// get match id for demo link if possible
	char matchId[SV_MATCHID_LEN];
	trap_Cvar_VariableStringBuffer("sv_matchid", matchId, sizeof(matchId));

	// get map str
	char mapStr[256] = { 0 };
	Q_strncpyz(mapStr, level.mapname, sizeof(mapStr));

	const char* arenaInfo = G_GetArenaInfoByMap(level.mapname);
	if (arenaInfo) {
		char* mapLongName = Info_ValueForKey(arenaInfo, "longname");
		if (VALIDSTRING(mapLongName)) {
			Com_sprintf(mapStr, sizeof(mapStr), "%s (%s)", mapLongName, level.mapname);
		}
	}

	// build the json string to post to discord
	cJSON* root = cJSON_CreateObject();
	if (root) {
		{
			if (VALIDSTRING(serverName)) {
				cJSON_AddStringToObject(root, "username", serverName);
			}

			cJSON* embeds = cJSON_AddArrayToObject(root, "embeds");

			{
				cJSON* msg = cJSON_CreateObject();
				if (msgColor >= 0) {
					cJSON_AddNumberToObject(msg, "color", msgColor);
				}
				cJSON* fields = cJSON_AddArrayToObject(msg, "fields");

				{

					{
						cJSON* redField = cJSON_CreateObject();
						cJSON_AddStringToObject(redField, "name", va("RED: %d%s", redScore, redScore > blueScore ? " :trophy:" : ""));
						cJSON_AddStringToObject(redField, "value", redTeam);
						cJSON_AddBoolToObject(redField, "inline", cJSON_True);
						cJSON_AddItemToArray(fields, redField);
					}
					{
						cJSON* blueField = cJSON_CreateObject();
						cJSON_AddStringToObject(blueField, "name", va("BLUE: %d%s", blueScore, blueScore > redScore ? " :trophy:" : ""));
						cJSON_AddStringToObject(blueField, "value", blueTeam);
						cJSON_AddBoolToObject(blueField, "inline", cJSON_True);
						cJSON_AddItemToArray(fields, blueField);
					}
					{
						if (VALIDSTRING(matchId)) {
							cJSON* linkField = cJSON_CreateObject();
							cJSON_AddStringToObject(linkField, "name", "Demoarchive link");
							cJSON_AddStringToObject(linkField, "value", va(DEMOARCHIVE_MATCH_FORMAT"\n(May not work until uploaded)", matchId));
							cJSON_AddItemToArray(fields, linkField);
						}
					}
					{
						if (VALIDSTRING(mapStr)) {
							cJSON* mapField = cJSON_CreateObject();
							cJSON_AddStringToObject(mapField, "name", "Map");
							cJSON_AddStringToObject(mapField, "value", mapStr);
							cJSON_AddBoolToObject(mapField, "inline", cJSON_False);
							cJSON_AddItemToArray(fields, mapField);
						}
					}
				}

				cJSON_AddItemToArray(embeds, msg);
			}
		}

		// generate the request and send it
		char *requestString = cJSON_PrintUnformatted(root);

		const char* url = va(DISCORD_WEBHOOK_FORMAT, g_webhookId.string, g_webhookToken.string);

		if (VALIDSTRING(stats)) {
			// if we have stats, send the request as a multipart and attach stats as a file
			SendMatchMultipart(url, matchId, stats, requestString);
		} else {
			// otherwise, just send a simple json POST request
			trap_SendPOSTRequest(NULL, url, requestString, "application/json", "application/json", qfalse);
		}

		free(requestString);
	}
	cJSON_Delete(root);
}