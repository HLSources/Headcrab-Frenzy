/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/***
*	Headcrab Frenzy Public Source Code
*
*	Copyright (c) 2010, Chain Studios. All rights reserved.
*	
****/
//
// teamplay_gamerules.cpp
//
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"skill.h"
#include	"items.h"
#include	"mapcycle.h"
#include	"hcf_balance.h"
#include	"game.h"

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgScoreInfo;
extern int gmsgMOTD;
extern float g_flIntermissionStartTime;

//=========================================================
//=========================================================
CHalfLifeRules::CHalfLifeRules( void )
{
	m_flIntermissionEndTime = 0;
	g_flIntermissionStartTime = 0;
	RefreshSkillData();
	LoadHighScore();
}

//=========================================================
//=========================================================
#define SP_INTERMISSION_TIME		5
#define SP_INTERMISSION_MAXTIME		60

extern cvar_t hcf_mode;

void CHalfLifeRules :: HCFRestart ( void )
{
	if (!g_fGameOver)
	{
		//restart HCF game
		CHCFManager *pManager = (CHCFManager*)UTIL_FindEntityByClassname(NULL, "hcf_manager");
		if (pManager)
		{
			pManager->StartGame( (int)hcf_mode.value );
		}
		else
			ALERT(at_console, "ERROR: missing HCF manager!\n");
	}
}

void CHalfLifeRules::Think ( void )
{
	if ( g_fGameOver )   // someone else quit the game already
	{
		m_flIntermissionEndTime = g_flIntermissionStartTime + SP_INTERMISSION_TIME;

		// check to see if we should change levels now
		if ( m_flIntermissionEndTime < gpGlobals->time )
		{
			if ( m_iEndIntermissionButtonHit  // check that someone has pressed a key, or the max intermission time is over
				|| ( ( g_flIntermissionStartTime + SP_INTERMISSION_MAXTIME ) < gpGlobals->time) ) 
			{
				ChangeLevel(); // intermission is over
			}
		}
		else
			m_iEndIntermissionButtonHit = FALSE;

		return;
	}

	
	// check if any player is over the frag limit
	int iAlive = 0;
	int iDead = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if (pPlayer)
		{
			if (pPlayer->IsAlive() || pPlayer->pev->armorvalue > 0)
				iAlive++;
			else
				iDead++;
		}
	}

	if (!iAlive && iDead)
	{
		GoToIntermission();
		return;
	}
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::IsMultiplayer( void )
{
	return FALSE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::IsDeathmatch ( void )
{
	return FALSE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::IsCoOp( void )
{
	return FALSE;
}


//=========================================================
//=========================================================
BOOL CHalfLifeRules::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	if ( !pPlayer->m_pActiveItem )
	{
		// player doesn't have an active item!
		return TRUE;
	}

	if ( !pPlayer->m_pActiveItem->CanHolster() )
	{
		return FALSE;
	}

	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules :: GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon )
{
	return FALSE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules :: ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	return TRUE;
}

void CHalfLifeRules :: InitHUD( CBasePlayer *pl )
{
	// sending just one score makes the hud scoreboard active;  otherwise
	// it is just disabled for single play
	MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
		WRITE_BYTE( ENTINDEX(pl->edict()) );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_LONG( 0 );
	MESSAGE_END();

	CHCFManager *pManager = (CHCFManager*)UTIL_FindEntityByClassname(NULL, "hcf_manager");
	if (!pManager || !pManager->GameStarted())
		SendMOTDToClient( pl->edict() );

	// loop through all active players and send their score info to the new client
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
				WRITE_BYTE( i );	// client number
				WRITE_SHORT( plr->pev->frags );
				WRITE_SHORT( plr->m_iDeaths );
				WRITE_SHORT( 0 );
				WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
				WRITE_LONG( plr->pev->gamestate );
			MESSAGE_END();
		}
	}

	if ( g_fGameOver )
	{
		MESSAGE_BEGIN( MSG_ONE, SVC_INTERMISSION, NULL, pl->edict() );
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
void CHalfLifeRules :: ClientDisconnected( edict_t *pClient )
{
}

//=========================================================
//=========================================================
float CHalfLifeRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	// subtract off the speed at which a player is allowed to fall without being hurt,
	// so damage will be based on speed beyond that, not the entire fall
	pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
	return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
}

//=========================================================
//=========================================================
void CHalfLifeRules :: PlayerSpawn( CBasePlayer *pPlayer )
{
	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);

	pPlayer->pev->armorvalue = HCF_START_LIVES + pPlayer->m_iBonusLives - pPlayer->m_iDeaths;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules :: AllowAutoTargetCrosshair( void )
{
	return ( g_iSkillLevel == SKILL_EASY );
}

//=========================================================
//=========================================================
void CHalfLifeRules :: PlayerThink( CBasePlayer *pPlayer )
{
	if ( g_fGameOver )
	{
		// check for button presses
		if ( pPlayer->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) )
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}
}


//=========================================================
//=========================================================
BOOL CHalfLifeRules :: FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeRules :: FlPlayerSpawnTime( CBasePlayer *pPlayer )
{
	return gpGlobals->time;//now!
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeRules :: IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 1;
}

//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
void CHalfLifeRules :: PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHCFManager *pManager = (CHCFManager*)UTIL_FindEntityByClassname(NULL, "hcf_manager");
	if (pManager && pManager->GameStarted() && (pManager->GetGameMode() > 0))
		pVictim->m_iDeaths += 1;

	if (pVictim && pVictim->pev->armorvalue > 1)
		UTIL_ShowMessage("HCRESPAWN", pVictim);
}

//=========================================================
// Deathnotice
//=========================================================
void CHalfLifeRules::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeRules :: PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeRules :: FlWeaponRespawnTime( CBasePlayerItem *pWeapon )
{
	return -1;
}

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHalfLifeRules :: FlWeaponTryRespawn( CBasePlayerItem *pWeapon )
{
	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeRules :: VecWeaponRespawnSpot( CBasePlayerItem *pWeapon )
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeRules :: WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	return GR_WEAPON_RESPAWN_NO;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeRules::PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem )
{
}

//=========================================================
//=========================================================
int CHalfLifeRules::ItemShouldRespawn( CItem *pItem )
{
	return GR_ITEM_RESPAWN_NO;
}

// Powerup spawn/respawn control
int CHalfLifeRules::PowerupShouldRespawn( CBaseEntity *pItem )
{
	if ( pItem->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_ITEM_RESPAWN_NO;
	}

	return GR_ITEM_RESPAWN_YES;
}

float CHalfLifeRules::FlPowerupRespawnTime( CBaseEntity *pItem )
{
	return gpGlobals->time + HCF_POWERUP_RESPAWN_TIME + RANDOM_FLOAT(-HCF_POWERUP_RESPAWN_TIME_RANDOM, HCF_POWERUP_RESPAWN_TIME_RANDOM);;
}

Vector CHalfLifeRules::VecPowerupRespawnSpot( CBaseEntity *pItem )
{
	return pItem->pev->origin;
}

//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeRules::FlItemRespawnTime( CItem *pItem )
{
	return -1;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeRules::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::IsAllowedToSpawn( CBaseEntity *pEntity )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeRules::PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount )
{
}

//=========================================================
//=========================================================
int CHalfLifeRules::AmmoShouldRespawn( CBasePlayerAmmo *pAmmo )
{
	return GR_AMMO_RESPAWN_NO;
}

//=========================================================
//=========================================================
float CHalfLifeRules::FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo )
{
	return -1;
}

//=========================================================
//=========================================================
Vector CHalfLifeRules::VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo )
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
float CHalfLifeRules::FlHealthChargerRechargeTime( void )
{
	return 0;// don't recharge
}

//=========================================================
//=========================================================
int CHalfLifeRules::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_GUN_NO;
}

//=========================================================
//=========================================================
int CHalfLifeRules::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_AMMO_NO;
}

//=========================================================
//=========================================================
int CHalfLifeRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	// why would a single player in half life need this? 
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;
	
	return GR_TEAMMATE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules :: FAllowMonsters( void )
{
	return TRUE;
}

BOOL CHalfLifeRules :: FAllowPowerups( void )
{
	return ( hcf_allowpowerups.value != 0 );
}

static const char *s_pszSpecialKillTitle[] = 
{
	"Double kills",
	"Triple kills",
	"Multi kills",
	"Mega kills",
	"Ultra kills",
	"Monster kills",
	"Godlike",
};

static const char *s_pszGameMode[] = 
{
	"Ladder",
	"Random",
};

static const char *s_pszGameSkill[] = 
{
	"Skill: Easy",
	"Skill: Medium",
	"Skill: Hard",
};

void CHalfLifeRules :: GoToIntermission( void )
{
	int i;

	if ( g_fGameOver )
		return;  // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	m_flIntermissionEndTime = gpGlobals->time + SP_INTERMISSION_TIME;
	g_flIntermissionStartTime = gpGlobals->time;

	g_fGameOver = TRUE;
	m_iEndIntermissionButtonHit = FALSE;

	//show player stats
	hudtextparms_t	m_textParms;
	hudtextparms_t	m_textParms2;
	char m_szhudMsg[1024];
	char tmpbuf[64];

	m_textParms.r1 = 255;
	m_textParms.g1 = 128;
	m_textParms.b1 = 50;
	m_textParms.a1 = 0;
	m_textParms.r2 = 255;
	m_textParms.g2 = 0;
	m_textParms.b2 = 0;
	m_textParms.a2 = 0;
	m_textParms.channel = 1;
	m_textParms.effect = 2;
	m_textParms.fadeinTime = 0.025;
	m_textParms.fadeoutTime = 1.5;
	m_textParms.holdTime = SP_INTERMISSION_MAXTIME;
	m_textParms.fxTime = 0.5f;
	m_textParms.x = 0.05f;
	m_textParms.y = 0.35f;

	m_textParms2 = m_textParms;
	m_textParms2.r1 = 50;
	m_textParms2.g1 = 128;
	m_textParms2.b1 = 255;
	m_textParms2.a1 = 0;
	m_textParms2.r2 = 0;
	m_textParms2.g2 = 255;
	m_textParms2.b2 = 0;
	m_textParms2.a2 = 0;
	m_textParms2.channel = 2;
	m_textParms2.x = 0.25f;
	m_textParms2.y = 0.425f;

	int iHighScoreClient = 0;
	BOOL m_bCheatsUsed = FALSE;
	int iGameMode = 0;
	int iSkillLevel = gSkillData.iSkillLevel - 1;
	if (iSkillLevel < 0)
		iSkillLevel = 0;
	else if (iSkillLevel > 2)
		iSkillLevel = 2;

	CHCFManager *pManager = (CHCFManager*)UTIL_FindEntityByClassname(NULL, "hcf_manager");
	if (pManager)
	{
		m_bCheatsUsed = pManager->CheatsUsed();
		iGameMode = pManager->GetGameMode();
	}

	HCFHighScore_t *pHighScore = (iGameMode == 0) ? &m_HighScoreLadder[iSkillLevel] : &m_HighScoreRandom[iSkillLevel];

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );

		if ( pPlayer )
		{
			if (pPlayer->pev->gamestate > pHighScore->score)
			{
				iHighScoreClient = i;
				strncpy(pHighScore->name, STRING(pPlayer->pev->netname), 255);
				pHighScore->score = pPlayer->pev->gamestate;
				pHighScore->frags = (int)pPlayer->pev->frags;
				pHighScore->deaths = pPlayer->m_iDeaths;
				pHighScore->rampage = pPlayer->m_iMaxLastSpawnKills;
				for (int i = 0; i < 7; i++)
					pHighScore->spkills[i] = pPlayer->m_iSpecialKillStat[i];
			}
		}
	}

	if (iHighScoreClient > 0 && !m_bCheatsUsed)
		SaveHighScore();

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );

		if ( pPlayer )
		{
			sprintf(m_szhudMsg, "+++ GAME OVER +++\n%s%s\n\n%s game results:\n\nScore: %d\nKills: %d\nDeaths: %d\nMax rampage: %d\n\n",
				(iHighScoreClient == i) ? "New High Score!\n" : "",
				m_bCheatsUsed ? "[NOT SAVED - CHEATS USED!]" : s_pszGameSkill[iSkillLevel],
				s_pszGameMode[iGameMode],
				pPlayer->pev->gamestate, (int)pPlayer->pev->frags, pPlayer->m_iDeaths, pPlayer->m_iMaxLastSpawnKills);

			for (int j = 0; j < 7; j++)
			{
				if (pPlayer->m_iSpecialKillStat[j] > 0)
				{
					sprintf(tmpbuf,"%s (%dx): %d\n",s_pszSpecialKillTitle[j], (j+2), pPlayer->m_iSpecialKillStat[j]);
					strncat(m_szhudMsg,tmpbuf,sizeof(m_szhudMsg)-1);
				}
			}

			UTIL_HudMessage( pPlayer, m_textParms, m_szhudMsg);

			if (iHighScoreClient == i)
			{
				EMIT_SOUND(ENT(pPlayer->pev), CHAN_VOICE, "hcfrenzy/takenlead.wav", 1.0f, ATTN_NONE);
			}
			else if (pPlayer->pev->gamestate > 0)
			{
				sprintf(m_szhudMsg, "Best score (%s):\n\nScore: %d\nKills: %d\nDeaths: %d\nMax rampage: %d\n\n",
				pHighScore->name, pHighScore->score, pHighScore->frags, pHighScore->deaths, pHighScore->rampage);

				for (int j = 0; j < 7; j++)
				{
					if (pHighScore->spkills[j] > 0)
					{
						sprintf(tmpbuf,"%s (%dx): %d\n",s_pszSpecialKillTitle[j], (j+2), pHighScore->spkills[j]);
						strncat(m_szhudMsg,tmpbuf,sizeof(m_szhudMsg)-1);
					}
				}

				//show high score
				UTIL_HudMessage( pPlayer, m_textParms2, m_szhudMsg);				
			}
		}
	}
}

#define MAX_MOTD_CHUNK	  60
#define MAX_MOTD_LENGTH   1536 // (MAX_MOTD_CHUNK * 4)

void CHalfLifeRules :: SendMOTDToClient( edict_t *client )
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( (char *)CVAR_GET_STRING( "motdfile" ), &length );

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while ( pFileList && *pFileList && char_count < MAX_MOTD_LENGTH )
	{
		char chunk[MAX_MOTD_CHUNK+1];
		
		if ( strlen( pFileList ) < MAX_MOTD_CHUNK )
		{
			strcpy( chunk, pFileList );
		}
		else
		{
			strncpy( chunk, pFileList, MAX_MOTD_CHUNK );
			chunk[MAX_MOTD_CHUNK] = 0;		// strncpy doesn't always append the null terminator
		}

		char_count += strlen( chunk );
		if ( char_count < MAX_MOTD_LENGTH )
			pFileList = aFileList + char_count; 
		else
			*pFileList = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, client );
			WRITE_BYTE(1);
			WRITE_BYTE( *pFileList ? FALSE : TRUE );	// FALSE means there is still more message to come
			WRITE_STRING( chunk );
		MESSAGE_END();
	}

	FREE_FILE( aFileList );
}


/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeRules :: ChangeLevel( void )
{
	static char szPreviousMapCycleFile[ 256 ];
	static mapcycle_t mapcycle;

	char szNextMap[32];
	char szFirstMapInList[32];
	char szCommands[ 1500 ];
	char szRules[ 1500 ];
	int minplayers = 0, maxplayers = 0;
	strcpy( szFirstMapInList, "hcf1" );  // the absolute default level is hldm1

	BOOL do_cycle = TRUE;

	// find the map to change to
	char *mapcfile = (char*)CVAR_GET_STRING( "mapcyclefile" );
	ASSERT( mapcfile != NULL );

	szCommands[ 0 ] = '\0';
	szRules[ 0 ] = '\0';

	// Has the map cycle filename changed?
	if ( stricmp( mapcfile, szPreviousMapCycleFile ) )
	{
		strcpy( szPreviousMapCycleFile, mapcfile );

		DestroyMapCycle( &mapcycle );

		if ( !ReloadMapCycleFile( mapcfile, &mapcycle ) || ( !mapcycle.items ) )
		{
			ALERT( at_console, "Unable to load map cycle file %s\n", mapcfile );
			do_cycle = FALSE;
		}
	}

	if ( do_cycle && mapcycle.items )
	{
		BOOL found = FALSE;
		mapcycle_item_s *item;

		// Assume current map
		strcpy( szNextMap, STRING(gpGlobals->mapname) );
		strcpy( szFirstMapInList, STRING(gpGlobals->mapname) );

		// Traverse list
		for ( item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next )
		{
			if (!stricmp(szNextMap, item->mapname))
				continue;

			found = TRUE;
			break;
		}

		if ( !found )
		{
			item = mapcycle.next_item;
		}			
		
		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		strcpy( szNextMap, item->mapname );

		ExtractCommandString( item->rulebuffer, szCommands );
		strcpy( szRules, item->rulebuffer );
	}

	if ( !IS_MAP_VALID(szNextMap) )
	{
		strcpy( szNextMap, szFirstMapInList );
	}

	g_fGameOver = TRUE;

	ALERT( at_console, "CHANGE LEVEL: %s\n", szNextMap );
	if ( strlen( szRules ) > 0 )
	{
		ALERT( at_console, "RULES:  %s\n", szRules );
	}
	
	CHANGE_LEVEL( szNextMap, NULL );
	if ( strlen( szCommands ) > 0 )
	{
		SERVER_COMMAND( szCommands );
	}
}