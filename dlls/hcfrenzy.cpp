/***
*	Headcrab Frenzy Public Source Code
*
*	Copyright (c) 2010, Chain Studios. All rights reserved.
*	
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "gamerules.h"
#include "player.h"
#include "weapons.h"
#include "shake.h"
#include "game.h"
#include "hcf_balance.h"

extern DLL_GLOBAL BOOL		g_fGameOver;

extern int gmsgScoreInfo;
extern int gmsgRestartGame;

static const char *s_szHeadcrabTypes[HCF_NUM_HEADCRAB_TYPES] = 
{
	"monster_hcf_headcrab",
	"monster_hcf_fastheadcrab",
	"monster_hcf_fireheadcrab",
	"monster_hcf_poisonheadcrab",
};

//=========================================================
// HCFMonsterMaker - this ent creates monsters during the game.
//=========================================================
#define SF_HCF_MONSTERMAKER_MONSTERCLIP		1

class CHCFMonsterMaker : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT MakerThink ( void );
	void DeathNotice ( entvars_t *pevChild );// monster maker children use this to tell the monster maker that they have died.
	void MakeMonster( void );
	void UpgradeChildren ( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	
	int  m_cLiveChildren;// how many monsters made by this monster maker that are currently alive
	float m_flGround; // z coord of the ground under me, used to make sure no monsters are under the maker when it drops a new child

	BOOL m_fActive;
	int	 m_iCurrentSpawn;
	int  m_cMaxLiveChildren;

	float m_flHealthScale;
	float m_flDamageScale;
};

LINK_ENTITY_TO_CLASS( hcf_monstermaker, CHCFMonsterMaker );

TYPEDESCRIPTION	CHCFMonsterMaker::m_SaveData[] = 
{
	DEFINE_FIELD( CHCFMonsterMaker, m_flGround, FIELD_FLOAT ),
	DEFINE_FIELD( CHCFMonsterMaker, m_fActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CHCFMonsterMaker, m_cLiveChildren, FIELD_INTEGER ),
	DEFINE_FIELD( CHCFMonsterMaker, m_cMaxLiveChildren, FIELD_INTEGER ),
	DEFINE_FIELD( CHCFMonsterMaker, m_iCurrentSpawn, FIELD_INTEGER ),
	DEFINE_FIELD( CHCFMonsterMaker, m_flHealthScale, FIELD_FLOAT ),
	DEFINE_FIELD( CHCFMonsterMaker, m_flDamageScale, FIELD_FLOAT ),
};
IMPLEMENT_SAVERESTORE( CHCFMonsterMaker, CBaseMonster );

void CHCFMonsterMaker :: Spawn( )
{
	pev->solid = SOLID_NOT;

	m_cLiveChildren = 0;
	Precache();

	m_fActive = FALSE;
	SetThink ( &CHCFMonsterMaker::MakerThink );
	pev->nextthink = gpGlobals->time + 1.0f;

	m_flGround = 0;
	m_iCurrentSpawn = 0;
	m_cMaxLiveChildren = 1;

	m_flHealthScale = 1.0f;
	m_flDamageScale = 1.0f;
}

void CHCFMonsterMaker :: Precache( void )
{
	int i;

	CBaseMonster::Precache();

	for (i = 0; i < HCF_NUM_HEADCRAB_TYPES; i++)
		UTIL_PrecacheOther( s_szHeadcrabTypes[i] );
}

//=========================================================
// MakeMonster-  this is the code that drops the monster
//=========================================================
void CHCFMonsterMaker :: MakeMonster( void )
{
	edict_t	*pent;
	entvars_t		*pevCreate;

	// set altitude. Now that I'm activated, any breakables, etc should be out from under me. 
	TraceResult tr;

	UTIL_TraceLine ( pev->origin, pev->origin - Vector ( 0, 0, 4096 ), ignore_monsters, ENT(pev), &tr );
	m_flGround = tr.vecEndPos.z;

	Vector mins = pev->origin - Vector( 34, 34, 0 );
	Vector maxs = pev->origin + Vector( 34, 34, 0 );
	maxs.z = pev->origin.z;
	mins.z = m_flGround;

	CBaseEntity *pList[5];
	int count = UTIL_EntitiesInBox( pList, 5, mins, maxs, FL_CLIENT|FL_MONSTER );
	if ( count )
	{
		// don't build a stack of monsters!
		return;
	}

	TraceResult tr2;
	UTIL_TraceLine ( pev->origin, pev->origin - Vector ( 0, 0, 4096 ), dont_ignore_monsters, ENT(pev), &tr2 );
	if (tr2.pHit && (tr2.pHit->v.flags & (FL_CLIENT|FL_MONSTER)))
		return;
	
	string_t iszSpawn;
	int iSpawn;

	if (m_iCurrentSpawn < 0)
		iSpawn = RANDOM_LONG(0, HCF_NUM_HEADCRAB_TYPES-1);
	else
		iSpawn = m_iCurrentSpawn;

	if (iSpawn >= HCF_NUM_HEADCRAB_TYPES)
		iSpawn = HCF_NUM_HEADCRAB_TYPES-1;

	iszSpawn = MAKE_STRING(s_szHeadcrabTypes[iSpawn]);

	pent = CREATE_NAMED_ENTITY( iszSpawn );

	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in HCFMonsterMaker!\n" );
		return;
	}

	
	// If I have a target, fire!
	if ( !FStringNull ( pev->target ) )
	{
		// delay already overloaded for this entity, so can't call SUB_UseTargets()
		FireTargets( STRING(pev->target), this, this, USE_TOGGLE, 0 );
	}

	pevCreate = VARS( pent );
	pevCreate->origin = pev->origin;
	pevCreate->angles = pev->angles;
	SetBits( pevCreate->spawnflags, SF_MONSTER_FALL_TO_GROUND );

	// Children hit monsterclip brushes
	if ( pev->spawnflags & SF_HCF_MONSTERMAKER_MONSTERCLIP )
		SetBits( pevCreate->spawnflags, SF_HCF_MONSTERMAKER_MONSTERCLIP );

	pevCreate->fuser1 = m_flHealthScale;
	pevCreate->fuser2 = m_flDamageScale;

	DispatchSpawn( pent );

	pevCreate->owner = edict();

	if ( !FStringNull( pev->netname ) )
	{
		// if I have a netname (overloaded), give the child monster that name as a targetname
		pevCreate->targetname = pev->netname;
	}
	if ( !FStringNull( pev->message ) )
	{
		// if I have a netname (overloaded), give the child monster that name as a targetname
		pevCreate->target = pev->message;
	}

//	UTIL_SetOrigin( pevCreate, pevCreate->origin - Vector( 0, 0, 8 ) );

	m_cLiveChildren++;// count this monster
}

void CHCFMonsterMaker :: UpgradeChildren ( void )
{
	if (m_flHealthScale < HCF_MONSTERMAKER_UPGRADE_MAXHEALTHSCALE)
	{
		m_flHealthScale *= HCF_MONSTERMAKER_UPGRADE_HEALTHSCALE;
		if (m_flHealthScale > HCF_MONSTERMAKER_UPGRADE_MAXHEALTHSCALE)
			m_flHealthScale = HCF_MONSTERMAKER_UPGRADE_MAXHEALTHSCALE;
	}

	m_flDamageScale *= HCF_MONSTERMAKER_UPGRADE_DAMAGESCALE;
}

//=========================================================
// MakerThink - creates a new monster every so often
//=========================================================
void CHCFMonsterMaker :: MakerThink ( void )
{
	pev->nextthink = gpGlobals->time + 0.1f;

	if (m_fActive && (m_cLiveChildren < m_cMaxLiveChildren))
		MakeMonster();
}


//=========================================================
//=========================================================
void CHCFMonsterMaker :: DeathNotice ( entvars_t *pevChild )
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_cLiveChildren--;
}


//////////////////////////////////////////////////////////////////////////

LINK_ENTITY_TO_CLASS( hcf_manager, CHCFManager );

TYPEDESCRIPTION	CHCFManager::m_SaveData[] = 
{
	DEFINE_FIELD( CHCFManager, m_iGameMode, FIELD_INTEGER ),
	DEFINE_FIELD( CHCFManager, m_fGameStarted, FIELD_BOOLEAN ),
	DEFINE_FIELD( CHCFManager, m_bCheated, FIELD_BOOLEAN ),
	DEFINE_FIELD( CHCFManager, m_flNextCheatMsg,  FIELD_TIME ),	
	DEFINE_FIELD( CHCFManager, m_fPrepareSound, FIELD_BOOLEAN ),
	DEFINE_FIELD( CHCFManager, m_bFirstBlood, FIELD_BOOLEAN ),
	DEFINE_FIELD( CHCFManager, m_flStartTime,  FIELD_TIME ),
	DEFINE_FIELD( CHCFManager, m_flCurrentTime,  FIELD_TIME ),
	DEFINE_FIELD( CHCFManager, m_iStartMsg,	FIELD_INTEGER ),
	DEFINE_FIELD( CHCFManager, m_iMaxChildren,	FIELD_INTEGER ),
	DEFINE_FIELD( CHCFManager, m_flNextMaxChildrenUpgrade,  FIELD_TIME ),
	DEFINE_FIELD( CHCFManager, m_flNextHeadcrabUpgrade,  FIELD_TIME ),
	DEFINE_FIELD( CHCFManager, m_flNextHeadcrabTypeUpgrade,  FIELD_TIME ),	
	DEFINE_FIELD( CHCFManager, m_flNextWeaponUpgrade,  FIELD_TIME ),
	DEFINE_FIELD( CHCFManager, m_pSoundSource,  FIELD_EDICT ),
	DEFINE_FIELD( CHCFManager, m_flWaitForPlayersTime,  FIELD_TIME ),
	DEFINE_ARRAY( CHCFManager, m_szSoundNotice,  FIELD_CHARACTER, 256 ),
};

IMPLEMENT_SAVERESTORE( CHCFManager, CBaseEntity );

void CHCFManager :: Spawn( void )
{
	Precache();
	
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= EF_NODRAW;
	m_fGameStarted		= FALSE;
	m_flStartTime		= 0;
	m_iStartMsg			= 0;
	m_iMaxChildren		= 1;
	m_bFirstBlood		= FALSE;
	memset(m_szSoundNotice, 0, sizeof(m_szSoundNotice));
	m_pSoundSource		= NULL;
	m_flSoundNoticeTime = 0;
	m_fPrepareSound		= FALSE;
	m_bCheated			= FALSE;
	m_iGameMode			= 0;
	m_flWaitForPlayersTime = 0;
	pev->nextthink = gpGlobals->time + 2.0f;
}

void CHCFManager :: Precache( void )
{
	PRECACHE_SOUND("items/smallmedkit1.wav");
}

void CHCFManager :: StartGame( int iMode )
{
	int i;

	if (m_fGameStarted)
	{
		//game already started, cleanup

		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
			if (pPlayer && !(pPlayer->pev->flags & FL_SPECTATOR) && !(pPlayer->pev->flags & FL_ZOMBIE))
			{
				pPlayer->pev->gamestate = 0;
				pPlayer->pev->frags = 0;
				pPlayer->m_iDeaths = 0;
				pPlayer->m_iBonusLives = 0;
				pPlayer->pev->health = 100;
				pPlayer->pev->max_health = 100;
				pPlayer->m_flFrenzy = 0;
				pPlayer->m_bFrenzyMode = FALSE;
				pPlayer->m_iNextExtraLifeFrags = HCF_EXTRALIFE_FRAGS;
				pPlayer->m_flDamageIncrease = 0;
				pPlayer->m_bKillRampage = 0;
				pPlayer->m_iMaxLastSpawnKills = 0;
				pPlayer->m_iLastSpawnKills = 0;
				pPlayer->pev->renderfx = 0;
				pPlayer->m_flRemoveFrenzy = 0;
				pPlayer->m_flRemoveWeaponShell = 0;
				pPlayer->m_flParalyzeTime = 0;
				pPlayer->m_flUpgradeMod = 0;
				pPlayer->m_bitsDamageType = 0;
				pPlayer->pev->fuser4 = 0;
				pPlayer->pev->iuser4 = 0;
				memset(pPlayer->m_iSpecialKillStat, 0, sizeof(pPlayer->m_iSpecialKillStat));

				pPlayer->RemoveAllItems( false );

				int j;

				for (j = 0; j < CDMG_TIMEBASED; j++)
					pPlayer->m_rgbTimeBasedDamage[j] = 0;

				MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
					WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
					WRITE_SHORT( 0 );
					WRITE_SHORT( 0 );
					WRITE_SHORT( 0 );
					WRITE_SHORT( 0 );
					WRITE_LONG( 0 );
				MESSAGE_END();

				pPlayer->Spawn();
			}
		}
		
		CBaseEntity *pMaker = NULL;

		while (pMaker = UTIL_FindEntityByClassname(pMaker, "hcf_monstermaker"))
		{
			((CHCFMonsterMaker*)pMaker)->m_iCurrentSpawn = 0;
			((CHCFMonsterMaker*)pMaker)->m_cMaxLiveChildren = 1;
			((CHCFMonsterMaker*)pMaker)->m_cLiveChildren = 0;
			((CHCFMonsterMaker*)pMaker)->m_fActive = FALSE;
			((CHCFMonsterMaker*)pMaker)->m_flHealthScale = 1.0f;
			((CHCFMonsterMaker*)pMaker)->m_flDamageScale = 1.0f;
		}

		//remove all existing crabs

		for (i = 0; i < HCF_NUM_HEADCRAB_TYPES; i++)
		{
			CBaseEntity *pCrab = NULL;
			while (pCrab = UTIL_FindEntityByClassname(pCrab, s_szHeadcrabTypes[i]))
			{
				UTIL_Remove(pCrab);
			}
		}

		// remove all gibs
		CBaseEntity *pGib = NULL;

		while (pGib = UTIL_FindEntityByClassname(pGib, "gib"))
		{
			pGib->SetThink(&CBaseEntity::SUB_Remove);
			pGib->pev->nextthink = gpGlobals->time + 0.1f;
		}
	}

	//send cleanup msg
	MESSAGE_BEGIN( MSG_ALL, gmsgRestartGame );
		WRITE_BYTE( iMode );
	MESSAGE_END();

	m_iGameMode = iMode;
	m_fGameStarted = TRUE;
	m_bFirstBlood = FALSE;
	m_bCheated = FALSE;
	m_flCurrentTime = gpGlobals->time;
	m_flStartTime = gpGlobals->time + 0.25f;
	m_iStartMsg = 0;
	pev->nextthink = gpGlobals->time + 0.1f;
	m_flNextCheatMsg = 0.0f;
	m_iMaxChildren = 1;
	m_flWaitForPlayersTime = 0;
	m_iClientWaitForPlayersTime = -1;

	m_flNextMaxChildrenUpgrade = m_flStartTime + 30.0f;
	m_flNextHeadcrabUpgrade = m_flStartTime + HCF_MONSTERMAKER_UPGRADE_DELAY;
	m_flNextWeaponUpgrade = m_flStartTime + HCF_WEAPON_UPGRADE_DELAY;

	if (iMode == 1)
		m_flNextHeadcrabTypeUpgrade = m_flStartTime + HCF_MONSTERMAKER_UPGRADETYPE_DELAY;
	else
		m_flNextHeadcrabTypeUpgrade = 0.0f;

	//respawn everything
	edict_t *pEdPowerup = g_engfuncs.pfnPEntityOfEntIndex( 1 );

	for ( i = 1; i < gpGlobals->maxEntities; i++, pEdPowerup++)
	{
		if ( pEdPowerup->free )	// Not in use
			continue;

		CBaseEntity *pEnt = CBaseEntity::Instance(pEdPowerup);
		if (pEnt)
			pEnt->ForceRespawn();
	}

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if (pPlayer && !pPlayer->pev->iuser1 && !(pPlayer->pev->flags & (FL_SPECTATOR|FL_ZOMBIE)))
		{
			pPlayer->m_flLastSlashDamage = gpGlobals->time + 4.0f;
			if ((m_iGameMode > 0) && !(pPlayer->pev->weapons & (1<<WEAPON_CROWBAR)))
				pPlayer->GiveNamedItem( "weapon_crowbar" );
		}
	}
}

void CHCFManager :: SoundNotice( edict_t *pSource, const char *szSample )
{
	m_pSoundSource = pSource;
	strncpy(m_szSoundNotice, szSample, sizeof(m_szSoundNotice)-1);
	m_flSoundNoticeTime = gpGlobals->time;
}

void CHCFManager :: HeadcrabKilled( entvars_t *pevKiller, int iCombo )
{
	if (!m_bFirstBlood)
	{
		m_bFirstBlood = TRUE;
		EMIT_SOUND(ENT(pevKiller), CHAN_VOICE, "hcfrenzy/firstblood.wav", 1.0f, ATTN_NONE);

		if (g_pGameRules->IsMultiplayer() && pevKiller)
		{
			hudtextparms_t	m_textParms;

			m_textParms.r1 = 160;
			m_textParms.g1 = 0;
			m_textParms.b1 = 255;
			m_textParms.a1 = 0;
			m_textParms.r2 = 0;
			m_textParms.g2 = 160;
			m_textParms.b2 = 255;
			m_textParms.a2 = 0;
			m_textParms.channel = 2;
			m_textParms.effect = 1;
			m_textParms.fadeinTime = 1.5f;
			m_textParms.fadeoutTime = 0.5;
			m_textParms.holdTime = 1.5f;
			m_textParms.fxTime = 0.5f;
			m_textParms.x = -1;
			m_textParms.y = 0.1f;

			UTIL_HudMessageAll(m_textParms, UTIL_VarArgs("+++ %s drew first blood! +++", STRING(pevKiller->netname)));
		}
	}

	CBaseEntity *pKiller = CBaseEntity::Instance(pevKiller);
	if (!pKiller || !pKiller->IsPlayer())
	{
		return;
	}

	CBasePlayer *plr = (CBasePlayer *)pKiller;

	if ( plr )
	{
		if (!plr->m_bFrenzyMode)
		{
			plr->m_flFrenzy += HCF_KILL_FRENZY_BONUS * iCombo;
			if (plr->m_flFrenzy > 100)
				plr->m_flFrenzy = 100;
		}

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( plr->entindex() );	// client number
			WRITE_SHORT( plr->pev->frags );
			WRITE_SHORT( plr->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( g_pGameRules->GetTeamIndex( plr->m_szTeamName ) + 1 );
			WRITE_LONG( plr->pev->gamestate );
		MESSAGE_END();

		//check if we need to increase max.health
		if (((int)floor(plr->pev->frags)) % HCF_HEALTH_BONUS_FRAGS == 0)
		{
			if (plr->pev->max_health < 999)
			{
				plr->pev->max_health += HCF_HEALTH_BONUS;
				plr->pev->health += HCF_HEALTH_BONUS;

				if (plr->pev->health > 999)
					plr->pev->health = 999;
				if (plr->pev->max_health > 999)
					plr->pev->max_health = 999;

				EMIT_SOUND(ENT(plr->pev), CHAN_AUTO, "items/smallmedkit1.wav", 1.0f, ATTN_NORM);
				UTIL_ScreenFade( plr, Vector(100,100,255), 2.0f, 0.5f, 55, FFADE_IN );
				UTIL_ShowMessage("HCEXTRAHEALTH", plr);
			}
		}
	}
}

void CHCFManager :: Think( void )
{
	CBaseEntity *pMaker = NULL;
	CBaseEntity *pFirstPlayer = UTIL_PlayerByIndex(1);

	if (!m_fPrepareSound && pFirstPlayer)
	{
		EMIT_SOUND( pFirstPlayer->edict(), CHAN_VOICE, "hcfrenzy/prepare.wav", 1.0f, ATTN_NONE);
		m_fPrepareSound = TRUE;
	}

	pev->nextthink = gpGlobals->time + 0.1f;

	if (g_pGameRules->IsMultiplayer() && (!m_fGameStarted || (m_iGameMode <= 0)))
	{
		//Game is in mode 0
		//Server can be in this state if:
		// 1) Game was not started yet
		// 2) Game was started, but all players left the server and it flushed the game
		//
		// In this case we should wait for one player.
		// When we got it, we shall wait for other players during "hcf_waittime" seconds
		// After that game will restart automatically

		if ((int)hcf_mode.value <= 0)
			return;

		if (!pFirstPlayer || (pFirstPlayer->pev->flags & FL_ZOMBIE))
		{
			m_flWaitForPlayersTime = 0; //reset it because player could disconnect during wait
			m_iClientWaitForPlayersTime = -1;
			return;
		}

		int cPlayers = 0;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(i);
			if (pPlayer)
				cPlayers++;
		}

		if (cPlayers == gpGlobals->maxClients)
		{
			//already at max.clients, start now!
			g_engfuncs.pfnServerPrint(UTIL_VarArgs("Server already at maxplayers (%d), starting game immediately.\n", cPlayers));
			StartGame( (int)hcf_mode.value );
		}
		else if (m_flWaitForPlayersTime < 0.1f)
		{
			g_engfuncs.pfnServerPrint(UTIL_VarArgs("One player entered the game. Waiting %d secs for others.\n", (int)hcf_waittime.value));
			m_flWaitForPlayersTime = gpGlobals->time + hcf_waittime.value;
			m_iClientWaitForPlayersTime = -1;
		}
		else if (m_flWaitForPlayersTime < gpGlobals->time)
		{
			StartGame( (int)hcf_mode.value );
		}
		else
		{
			//show wait messages
			int curTime = (int)ceil(m_flWaitForPlayersTime - gpGlobals->time);
			if (m_iClientWaitForPlayersTime != curTime)
			{
				m_iClientWaitForPlayersTime = curTime;
				if (curTime > 60)
					UTIL_CenterPrintAll(UTIL_VarArgs("Waiting for other players...%d min %d sec", curTime / 60, curTime % 60));
				else
					UTIL_CenterPrintAll(UTIL_VarArgs("Waiting for other players...%d sec", curTime));
			}
		}
		return;
	}

	if (!g_pGameRules->IsMultiplayer())
	{
		if (pFirstPlayer && !pFirstPlayer->IsAlive())
			return;
	}
	else
	{
		int numPlayers = 0;
		int numActivePlayers = 0;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(i);
			if (pPlayer)
			{
				if (!(pPlayer->edict()->v.flags & FL_ZOMBIE)) //ignore zombies
				{
					numPlayers++;
					if (!pPlayer->pev->iuser1)
						numActivePlayers++;
				}
			}
		}

		if (numPlayers <= 0)
		{
			//no players on server
			//FIXME: can this happen on listen server?
			if (IS_DEDICATED_SERVER() && (GetGameMode() > 0))
			{
				//restart everything and wait for new players
				g_engfuncs.pfnServerPrint("All players left the server. Restarting the game.\n");
				StartGame(0);
				return;
			}
		}
		else if (numActivePlayers <= 0)
		{
			//all players are spectators or dead
			return;
		}
	}

	//check if players use cheats
	if (!m_bCheated)
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );

			if ( pPlayer && !(pPlayer->pev->flags & FL_SPECTATOR) )
			{
				if (pPlayer->pev->flags & FL_GODMODE)
				{
					m_bCheated = true;
					break;
				}
				if (pPlayer->pev->movetype == MOVETYPE_NOCLIP)
				{
					m_bCheated = true;
					break;
				}
			}
		}

	}
	else if (m_flNextCheatMsg < gpGlobals->time)
	{
		UTIL_ShowMessageAll("HCCHEATER");
		m_flNextCheatMsg = gpGlobals->time + 15.0f;
	}

	m_flCurrentTime += 0.1f;
	
	float flTimeElapsed = m_flCurrentTime - m_flStartTime;

	if (flTimeElapsed < (HCF_WARMUP+1))
	{
		if ((int)floor(flTimeElapsed) == HCF_WARMUP && m_iStartMsg == HCF_WARMUP)
		{
			UTIL_CenterPrintAll("");

			if (m_iGameMode == 2)
				UTIL_ShowMessageAll("HCSTART2");
			else if (m_iGameMode == 1)
				UTIL_ShowMessageAll("HCSTART1");
			m_iStartMsg++;

			//Activate all monstermakers and let frenzy begin!
			while (pMaker = UTIL_FindEntityByClassname(pMaker, "hcf_monstermaker"))
			{
				if (m_iGameMode > 1)
				{
					//random mode
					((CHCFMonsterMaker*)pMaker)->m_iCurrentSpawn = -1;
				}
				else
					((CHCFMonsterMaker*)pMaker)->m_iCurrentSpawn = 0;

				((CHCFMonsterMaker*)pMaker)->m_fActive = TRUE;
				((CHCFMonsterMaker*)pMaker)->m_cMaxLiveChildren = m_iMaxChildren;
			}
		}
		else
		{
			int c = HCF_WARMUP-1;
			char msg[32];

			while (c >= 0)
			{
				if ((int)floor(flTimeElapsed) == c && m_iStartMsg == c)
				{
					sprintf(msg,"Game starts in %d...",HCF_WARMUP-c);
					UTIL_CenterPrintAll(msg);

					if ( HCF_WARMUP-c <= 3)
					{
						sprintf(msg,"hcfrenzy/cd%d.wav",HCF_WARMUP-c);
						EMIT_SOUND(INDEXENT(1), CHAN_AUTO, msg, 1.0f, ATTN_NONE);
					}

					m_iStartMsg = c+1;
					break;
				}
				c--;
			}
		}
		return;
	}

	if ( g_fGameOver )
		return;
		
	//upgrade HCF monstermaker max.live children
	if (m_iMaxChildren < HCF_MAX_MONSTERMAKER_ALIVE_HEADCRABS && (m_flNextMaxChildrenUpgrade < m_flCurrentTime))
	{
		m_iMaxChildren++;

		m_flNextMaxChildrenUpgrade = m_flCurrentTime + 32.0f + ((m_iMaxChildren-1) * 1.5f) * 45.0f;

		while (pMaker = UTIL_FindEntityByClassname(pMaker, "hcf_monstermaker"))
		{
			((CHCFMonsterMaker*)pMaker)->m_cMaxLiveChildren = m_iMaxChildren;
		}

		UTIL_ShowMessageAll("HCINCMAXCHILDREN");
	}

	//upgrade headcrab types
	if (m_flNextHeadcrabTypeUpgrade > 0.1f && m_flNextHeadcrabTypeUpgrade < m_flCurrentTime)
	{
		int c1, c2;

		c1 = 0;
		c2 = 0;

		while (pMaker = UTIL_FindEntityByClassname(pMaker, "hcf_monstermaker"))
		{
			((CHCFMonsterMaker*)pMaker)->m_iCurrentSpawn++;
			c1++;
			if (((CHCFMonsterMaker*)pMaker)->m_iCurrentSpawn >= HCF_NUM_HEADCRAB_TYPES)
			{
				((CHCFMonsterMaker*)pMaker)->m_iCurrentSpawn = -1;
				c2++;
			}
		}

		if (c1 == c2)
		{
			UTIL_ShowMessageAll("HCRANDOM");
			m_flNextHeadcrabTypeUpgrade = 0.0f;
		}
		else
		{
			UTIL_ShowMessageAll("HCUPGRADETYPE");
			m_flNextHeadcrabTypeUpgrade = m_flCurrentTime + HCF_MONSTERMAKER_UPGRADETYPE_DELAY;
		}

		m_flNextHeadcrabUpgrade = m_flCurrentTime + HCF_MONSTERMAKER_UPGRADE_DELAY;
	}
	//upgrade headcrabs
	else if (m_flNextHeadcrabUpgrade < m_flCurrentTime)
	{
		while (pMaker = UTIL_FindEntityByClassname(pMaker, "hcf_monstermaker"))
		{
			((CHCFMonsterMaker*)pMaker)->UpgradeChildren();
		}

		UTIL_ShowMessageAll("HCUPGRADE");
		m_flNextHeadcrabUpgrade = m_flCurrentTime + HCF_MONSTERMAKER_UPGRADE_DELAY;
	}

	//upgrade player weapons
	if (m_flNextWeaponUpgrade < m_flCurrentTime)
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );

			if ( pPlayer )
			{
				pPlayer->m_flUpgradeMod += HCF_WEAPON_UPGRADE_VALUE;
				pPlayer->ActivateWeaponShell();

				// Update all the items
				for ( int j = 0; j < MAX_ITEM_TYPES; j++ )
				{
					CBasePlayerItem *pItem = pPlayer->m_rgpPlayerItems[j];
					while ( pItem )  // each item updates it's successors
					{
						pItem->m_flUpgradeMod = pPlayer->m_flUpgradeMod;
						pItem = pItem->m_pNext;
					}
				}
			}
		}

		UTIL_ShowMessageAll("HCUPGRADEWEAPON");

		m_flNextWeaponUpgrade = m_flCurrentTime + HCF_WEAPON_UPGRADE_DELAY;
	}

	if (m_flSoundNoticeTime < gpGlobals->time - (HCF_COMBO_DELAY+0.1f))
	{
		if (m_pSoundSource)
		{
			EMIT_SOUND(m_pSoundSource, CHAN_VOICE, m_szSoundNotice, 1.0f, ATTN_NORM);
			m_pSoundSource = NULL;
		}
	}
}