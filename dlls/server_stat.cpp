/***
*	Headcrab Frenzy Public Source Code
*
*	Copyright (c) 2010, Chain Studios. All rights reserved.
*	
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "client.h"
#include "server_stat.h"

#ifdef _WIN32
#include "../curl/curl.h"
#else
#include <curl/curl.h>
#endif

#define STAT_GAME_VERSION	"1.3 beta"

#define MAX_SERVER_OUTPUT	4096
#define MAX_SERVER_INPUT	1024

static CURL *curl;
static CURLcode result;

static char msgBuffer[MAX_SERVER_INPUT];
static char msgBufferEnc[MAX_SERVER_INPUT*3];
static char errorBuffer[CURL_ERROR_SIZE];
static char dataBuffer[MAX_SERVER_OUTPUT];
static int dataSize;
static int globalInit = 0;

#define SVSTAT_SERVER		"http://path/to/server/index.php"
#define SVSTAT_USERAGENT	"HCFCustomUserAgent"
#define SVSTAT_USER			"username"
#define SVSTAT_PASS			"password"
#define SVSTAT_PROTO		1

//based on javascript encodeURIComponent()
static unsigned short _char2hex( char dec )
{
    char dig1 = (dec&0xF0)>>4;
    char dig2 = (dec&0x0F);
    if ( 0<= dig1 && dig1<= 9) dig1+=48;    //0,48inascii
    if (10<= dig1 && dig1<=15) dig1+=97-10; //a,97inascii
    if ( 0<= dig2 && dig2<= 9) dig2+=48;
    if (10<= dig2 && dig2<=15) dig2+=97-10;

    union {
		char c[2];
		short s;
    } u;

	u.c[0] = dig1;
	u.c[1] = dig2;

    return u.s;
}

static void _urlencode(const char *in, char *out)
{
    char *outp = out;
	int parm = 0;
    int max = strlen(in);

    for (int i=0; i<max; i++)
    {
        if ( (48 <= in[i] && in[i] <= 57) ||//0-9
             (65 <= in[i] && in[i] <= 90) ||//abc...xyz
             (97 <= in[i] && in[i] <= 122) || //ABC...XYZ
             (in[i]=='~' || in[i]=='!' || in[i]=='*' || in[i]=='(' || in[i]==')' || in[i]=='\'')
        )
        {
			*outp = in[i];
			outp++;
        }
        else
        {
			if (in[i] == '=' && !parm)
			{
				*outp = in[i];
				outp++;
				parm = 1;
			}
			else if (in[i] == '&' && parm)
			{
				*outp = in[i];
				outp++;
				parm = 0;
			}
			else
			{
				*outp = '%';
				outp++;
				*(unsigned short*)outp = _char2hex(in[i]);
				outp+=2;
			}
        }
    }
}

static int curl_write_callback_f(char *data, size_t size, size_t nmemb, char *buffer)  
{  
	// What we will return  
	int result = 0;  
  
	// Is there anything in the buffer?  
	if (buffer != NULL)  
	{  
		// Append the data to the buffer
		int writeSize = min( size * nmemb, (unsigned)(MAX_SERVER_OUTPUT - dataSize - 1));
		memcpy(dataBuffer, data, writeSize);  
		dataSize += writeSize;
  
		// How much did we write?  
		result = writeSize;  
	}  
  
	return result;  
}

void SV_ConnectStatsServer( void )
{
	if (!globalInit)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		globalInit = 1;
	}
	
	curl = curl_easy_init();
	
	if (!curl)
		return;

	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);  
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);  
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);  
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback_f);  
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dataBuffer); 
	curl_easy_setopt(curl, CURLOPT_URL, SVSTAT_SERVER); 
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
	curl_easy_setopt(curl, CURLOPT_USERNAME, SVSTAT_USER);
	curl_easy_setopt(curl, CURLOPT_PASSWORD, SVSTAT_PASS);	
	curl_easy_setopt(curl, CURLOPT_USERAGENT, SVSTAT_USERAGENT);	
	curl_easy_setopt(curl, CURLOPT_POST, 1);
}

void SV_DisconnectStatsServer( void )
{
	if (curl)
	{
		curl_easy_cleanup(curl);
		curl = NULL;
	}
}

void SV_SaveServerStats( CBasePlayer *pPlayer, int iGameMode, BOOL bMatchLost )
{
	if (!pPlayer)
		return;

	memset(msgBuffer, 0, sizeof(msgBuffer));
	memset(msgBufferEnc, 0, sizeof(msgBufferEnc));
	memset(dataBuffer, 0, sizeof(dataBuffer));
	dataSize = 0;

	char statBuffer[128];
	

	sprintf(statBuffer,"%u|%u|%u|%u|%u|%u|%u|%u|%u",
					  (int)pPlayer->pev->frags,
					  pPlayer->m_iMaxLastSpawnKills,
					  pPlayer->m_iSpecialKillStat[0],
					  pPlayer->m_iSpecialKillStat[1],
					  pPlayer->m_iSpecialKillStat[2],
					  pPlayer->m_iSpecialKillStat[3],
					  pPlayer->m_iSpecialKillStat[4],
					  pPlayer->m_iSpecialKillStat[5],
					  pPlayer->m_iSpecialKillStat[6]);

	sprintf(msgBuffer,"a=p&proto=%u&rnd=%u&n=%s&auth=%s&score=%u&g=%s&v=%s&map=%s&gm=%u&w=%u&buf=%s",
					  SVSTAT_PROTO,
					  RANDOM_LONG(1,65535),
					  STRING(pPlayer->pev->netname),
					  GETPLAYERAUTHID(pPlayer->edict()),
					  pPlayer->pev->gamestate,
					  GetGameDescription(),
					  STAT_GAME_VERSION,
					  STRING(gpGlobals->mapname),
					  iGameMode,
					  (bMatchLost == FALSE) ? 1 : 0,
					  statBuffer);

	_urlencode(msgBuffer, msgBufferEnc);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msgBufferEnc);

	result = curl_easy_perform(curl);

	if (result == CURLE_OK)
    {
		dataBuffer[dataSize] = 0;
		g_engfuncs.pfnServerPrint(dataBuffer);
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Published results for %s\n", STRING(pPlayer->pev->netname)));
    }
    else
    {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("CURL Error: %s\n", errorBuffer));
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Error publishing results for %s\n", STRING(pPlayer->pev->netname)));
	}
}

void SV_ValidateStatServer( void )
{
	g_engfuncs.pfnServerPrint("STAT SERVER VALIDATION...\n");

	SV_ConnectStatsServer();

	memset(msgBuffer, 0, sizeof(msgBuffer));
	memset(msgBufferEnc, 0, sizeof(msgBufferEnc));
	memset(dataBuffer, 0, sizeof(dataBuffer));
	dataSize = 0;

	sprintf(msgBuffer,"a=v&proto=%u&rnd=%u",
					  SVSTAT_PROTO,
					  RANDOM_LONG(1,65535));

	_urlencode(msgBuffer, msgBufferEnc);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msgBufferEnc);

	result = curl_easy_perform(curl);

	if (result == CURLE_OK)
    {
		dataBuffer[dataSize] = 0;
		g_engfuncs.pfnServerPrint(dataBuffer);
    }
    else
    {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("CURL Error: %s\n", errorBuffer));
	}

	SV_DisconnectStatsServer();
}