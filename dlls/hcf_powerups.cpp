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
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "items.h"
#include "gamerules.h"
#include "shake.h"
#include "hcf_balance.h"

extern int gEvilImpulse101;

Vector g_vecPowerupColors[POWERUP_MAX_COUNT] = 
{
	Vector( 255, 0, 0 ),
	Vector( 0, 0, 255 ),
	Vector( 0, 255, 0 ),
};

class CHCFPowerup : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	BOOL PowerupTouch( CBasePlayer *pOther );
	void ForceRespawn( void );
	virtual BOOL CheckRespawn( void ) { return TRUE; }

	void EXPORT AnimateThink( void );
	void EXPORT MyTouch( CBaseEntity *pOther );
	void EXPORT PowerupMaterialize( void );

private:
	CBaseEntity* Respawn( void );
};

void CHCFPowerup :: Precache( void )
{
	PRECACHE_SOUND("hcfrenzy/pow_respawn.wav");
	PRECACHE_SOUND("hcfrenzy/pow_grab.wav");
}

void CHCFPowerup :: Spawn( void )
{
	pev->impulse = 0;
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_TRIGGER;
	pev->sequence = 0;
	pev->frame = 0;

	ResetSequenceInfo();

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));

	if (DROP_TO_FLOOR(ENT(pev)) == 0)
	{
		ALERT(at_error, "Powerup %s fell out of level at %f,%f,%f\n", STRING( pev->classname ), pev->origin.x, pev->origin.y, pev->origin.z);
		UTIL_Remove( this );
		return;
	}

	pev->origin.z += 16;

	//initially off
	SetTouch( NULL );
	SetThink( NULL );
	pev->effects |= EF_NODRAW;

	CBaseAnimating::Spawn();
}

void CHCFPowerup :: AnimateThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CHCFPowerup :: MyTouch( CBaseEntity *pOther )
{
	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	if (PowerupTouch( pPlayer ))
	{
		pPlayer->m_flPowerupTimes[pev->impulse] = gpGlobals->time;

		if (!pPlayer->m_bFrenzyMode)
		{
			pPlayer->pev->renderfx = kRenderFxGlowShell;
			pPlayer->pev->rendercolor = pev->rendercolor;
		}

		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "hcfrenzy/pow_grab.wav", 1, ATTN_NORM );
		SUB_UseTargets( pOther, USE_TOGGLE, 0 );
		SetTouch( NULL );

		if ( g_pGameRules->PowerupShouldRespawn( this ) == GR_ITEM_RESPAWN_YES )
		{
			Respawn(); 
		}
		else
		{
			UTIL_Remove( this );
		}
	}
	else if (gEvilImpulse101)
	{
		UTIL_Remove( this );
	}
}

BOOL CHCFPowerup::PowerupTouch( CBasePlayer *pPlayer )
{
	//return FALSE if powerup cannot be grabbed
	return TRUE;
}

void CHCFPowerup::ForceRespawn( void ) 
{
	Respawn();
}

CBaseEntity* CHCFPowerup::Respawn( void )
{
	SetTouch( NULL );
	SetThink( NULL );
	pev->effects |= EF_NODRAW;

	if (!g_pGameRules->FAllowPowerups())
	{
		SetThink ( NULL );
	}
	else
	{
		UTIL_SetOrigin( pev, g_pGameRules->VecPowerupRespawnSpot( this ) );// blip to whereever you should respawn.
		SetThink ( &CHCFPowerup::PowerupMaterialize );
		pev->nextthink = g_pGameRules->FlPowerupRespawnTime( this ); 
	}

	return this;
}

void CHCFPowerup::PowerupMaterialize( void )
{
	if (!CheckRespawn())
	{
		pev->nextthink = gpGlobals->time + 1.0f;
		return;
	}

	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "hcfrenzy/pow_respawn.wav", 1, ATTN_STATIC);
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	pev->effects |= EF_NOINTERP;
	pev->sequence = 0;
	pev->frame = 0;
	ResetSequenceInfo();

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));

	SetTouch( &CHCFPowerup::MyTouch );
	SetThink( &CHCFPowerup::AnimateThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

//////////////////////////////////////////////////////////////////////////

class CHCFPowerupInvulnerability : public CHCFPowerup
{
public:
	void Spawn( void );
	void Precache( void );
};

LINK_ENTITY_TO_CLASS( hcf_powerup_invul, CHCFPowerupInvulnerability );

void CHCFPowerupInvulnerability :: Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/pow_invul.mdl");

	CHCFPowerup::Spawn();

	pev->impulse = POWERUP_INVUL;
	pev->rendercolor = g_vecPowerupColors[pev->impulse];
	pev->renderfx = kRenderFxGlowShell;
}

void CHCFPowerupInvulnerability :: Precache( void )
{
	PRECACHE_MODEL("models/pow_invul.mdl");

	CHCFPowerup::Precache();
}

class CHCFPowerupInvisibility : public CHCFPowerup
{
public:
	void Spawn( void );
	void Precache( void );
};

LINK_ENTITY_TO_CLASS( hcf_powerup_invis, CHCFPowerupInvisibility );

void CHCFPowerupInvisibility :: Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/pow_invis.mdl");

	CHCFPowerup::Spawn();

	pev->impulse = POWERUP_INVIS;
	pev->rendercolor = g_vecPowerupColors[pev->impulse];
	pev->renderfx = kRenderFxGlowShell;
	pev->renderamt = 80.0f;
	pev->rendermode = kRenderTransTexture;
}

void CHCFPowerupInvisibility :: Precache( void )
{
	PRECACHE_MODEL("models/pow_invis.mdl");

	CHCFPowerup::Precache();
}


class CHCFPowerupRegeneration : public CHCFPowerup
{
public:
	void Spawn( void );
	void Precache( void );
};

LINK_ENTITY_TO_CLASS( hcf_powerup_regen, CHCFPowerupRegeneration );

void CHCFPowerupRegeneration :: Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/pow_regen.mdl");

	CHCFPowerup::Spawn();

	pev->impulse = POWERUP_REGEN;
	pev->rendercolor = g_vecPowerupColors[pev->impulse];
	pev->renderfx = kRenderFxGlowShell;
}

void CHCFPowerupRegeneration :: Precache( void )
{
	PRECACHE_MODEL("models/pow_regen.mdl");

	CHCFPowerup::Precache();
}

class CHCFPowerupRandom : public CHCFPowerup
{
public:
	void Spawn( void );
	void Precache( void );
	BOOL CheckRespawn( void );
};

LINK_ENTITY_TO_CLASS( hcf_powerup_random, CHCFPowerupRandom );

void CHCFPowerupRandom :: Spawn( void )
{
	Precache();

	CHCFPowerup::Spawn();

	CheckRespawn();

	pev->renderfx = kRenderFxGlowShell;
}

void CHCFPowerupRandom :: Precache( void )
{
	PRECACHE_MODEL("models/pow_invis.mdl");
	PRECACHE_MODEL("models/pow_invul.mdl");
	PRECACHE_MODEL("models/pow_regen.mdl");

	CHCFPowerup::Precache();
}

BOOL CHCFPowerupRandom :: CheckRespawn( void )
{
	pev->impulse = RANDOM_LONG(0, POWERUP_MAX_COUNT - 1);
	pev->rendercolor = g_vecPowerupColors[pev->impulse];

	switch (pev->impulse)
	{
	case POWERUP_INVIS:
		{
			SET_MODEL(ENT(pev), "models/pow_invis.mdl");
			pev->renderamt = 80.0f;
			pev->rendermode = kRenderTransTexture;
			break;
		}
	case POWERUP_INVUL:
		{
			SET_MODEL(ENT(pev), "models/pow_invul.mdl");
			pev->renderamt = 0;
			pev->rendermode = 0;
			break;
		}
	case POWERUP_REGEN:
		{
			SET_MODEL(ENT(pev), "models/pow_regen.mdl");
			pev->renderamt = 0;
			pev->rendermode = 0;
			break;
		}
	default:
		return FALSE;
	}

	return TRUE;
}