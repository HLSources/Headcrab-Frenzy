/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
/***
*	Headcrab Frenzy Public Source Code
*
*	Copyright (c) 2010, Chain Studios. All rights reserved.
*	
****/
//=========================================================
// headcrab.cpp - tiny, jumpy alien parasite
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"game.h"
#include	"player.h"
#include	"explode.h"
#include	"gamerules.h"
#include	"hcf_balance.h"

extern DLL_GLOBAL BOOL		g_fGameOver;

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		HC_AE_JUMPATTACK	( 2 )

Task_t	tlHCRangeAttack1[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE	},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_WAIT_RANDOM,			(float)0.5		},
};

Schedule_t	slHCRangeAttack1[] =
{
	{ 
		tlHCRangeAttack1,
		ARRAYSIZE ( tlHCRangeAttack1 ), 
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"HCRangeAttack1"
	},
};

Task_t	tlHCRangeAttack1Fast[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE	},
};

Schedule_t	slHCRangeAttack1Fast[] =
{
	{ 
		tlHCRangeAttack1Fast,
		ARRAYSIZE ( tlHCRangeAttack1Fast ), 
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"HCRAFast"
	},
};

class CHeadCrab : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void RunTask ( Task_t *pTask );
	virtual void StartTask ( Task_t *pTask );
	virtual void SetYawSpeed ( void );
	void EXPORT LeapTouch ( CBaseEntity *pOther );
	Vector Center( void );
	Vector BodyTarget( const Vector &posSrc );
	void PainSound( void );
	void DeathSound( void );
	void IdleSound( void );
	void AlertSound( void );
	void PrescheduleThink( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	virtual float GetAttackDist( void ) { return 256.0f; }
	virtual float GetDamageAmount( void ) { return gSkillData.headcrabDmgBite; }
	virtual int GetVoicePitch( void ) { return 100; }
	virtual float GetSoundVolue( void ) { return 1.0; }
	virtual Schedule_t* GetScheduleOfType ( int Type );

	CUSTOM_SCHEDULES;

	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackSounds[];
	static const char *pDeathSounds[];
	static const char *pBiteSounds[];

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

protected:
	float m_flLastSuccessfulAttack;
};
LINK_ENTITY_TO_CLASS( monster_headcrab, CHeadCrab );

DEFINE_CUSTOM_SCHEDULES( CHeadCrab )
{
	slHCRangeAttack1,
	slHCRangeAttack1Fast,
};

IMPLEMENT_CUSTOM_SCHEDULES( CHeadCrab, CBaseMonster );

TYPEDESCRIPTION CHeadCrab::m_SaveData[] =
{
	DEFINE_FIELD( CHeadCrab, m_flLastSuccessfulAttack, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CHeadCrab, CBaseMonster );

const char *CHeadCrab::pIdleSounds[] = 
{
	"headcrab/hc_idle1.wav",
	"headcrab/hc_idle2.wav",
	"headcrab/hc_idle3.wav",
};
const char *CHeadCrab::pAlertSounds[] = 
{
	"headcrab/hc_alert1.wav",
};
const char *CHeadCrab::pPainSounds[] = 
{
	"headcrab/hc_pain1.wav",
	"headcrab/hc_pain2.wav",
	"headcrab/hc_pain3.wav",
};
const char *CHeadCrab::pAttackSounds[] = 
{
	"headcrab/hc_attack1.wav",
	"headcrab/hc_attack2.wav",
	"headcrab/hc_attack3.wav",
};

const char *CHeadCrab::pDeathSounds[] = 
{
	"headcrab/hc_die1.wav",
	"headcrab/hc_die2.wav",
};

const char *CHeadCrab::pBiteSounds[] = 
{
	"headcrab/hc_headbite.wav",
};

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CHeadCrab :: Classify ( void )
{
	return	CLASS_ALIEN_PREY;
}

//=========================================================
// Center - returns the real center of the headcrab.  The 
// bounding box is much larger than the actual creature so 
// this is needed for targeting
//=========================================================
Vector CHeadCrab :: Center ( void )
{
	return Vector( pev->origin.x, pev->origin.y, pev->origin.z + 6 );
}


Vector CHeadCrab :: BodyTarget( const Vector &posSrc ) 
{ 
	return Center( );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CHeadCrab :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:			
		ys = 30;
		break;
	case ACT_RUN:			
	case ACT_WALK:			
		ys = 20;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 60;
		break;
	case ACT_RANGE_ATTACK1:	
		ys = 30;
		break;
	default:
		ys = 30;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CHeadCrab :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case HC_AE_JUMPATTACK:
		{
			ClearBits( pev->flags, FL_ONGROUND );

			UTIL_SetOrigin (pev, pev->origin + Vector ( 0 , 0 , 1) );// take him off ground so engine doesn't instantly reset onground 
			UTIL_MakeVectors ( pev->angles );

			Vector vecJumpDir;
			if (m_hEnemy != NULL)
			{
				float gravity = g_psv_gravity->value;
				if (gravity <= 1)
					gravity = 1;

				// How fast does the headcrab need to travel to reach that height given gravity?
				float height = (m_hEnemy->pev->origin.z + m_hEnemy->pev->view_ofs.z - pev->origin.z);
				if (height < 16)
					height = 16;
				float speed = sqrt( 2 * gravity * height );
				float time = (speed / gravity) / pev->armorvalue;

				// Scale the sideways velocity to get there at the right time
				vecJumpDir = (m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs - pev->origin);
				vecJumpDir = vecJumpDir * ( 1.0 / time );

				// Speed to offset gravity at the desired height
				vecJumpDir.z = speed * pev->armorvalue;

				// Don't jump too far/fast
				float distance = vecJumpDir.Length();
				
				if (distance > 650)
				{
					vecJumpDir = vecJumpDir * ( 650.0 / distance );
				}
			}
			else
			{
				// jump hop, don't care where
				vecJumpDir = Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_up.z ) * 300;
			}

			int iSound = RANDOM_LONG(0,2);
			if ( iSound != 0 )
				EMIT_SOUND_DYN( edict(), CHAN_VOICE, pAttackSounds[iSound], GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch() );

			pev->velocity = vecJumpDir;
			m_flNextAttack = gpGlobals->time + 2;
		}
		break;

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CHeadCrab :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/headcrab.mdl");
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 24));

	pev->armorvalue		= 1.0f;
	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->effects		= 0;
	pev->health			= gSkillData.headcrabHealth;
	pev->view_ofs		= Vector ( 0, 0, 20 );// position of the eyes relative to monster's origin.
	pev->yaw_speed		= 5;//!!! should we put this in the monster's changeanim function since turn rates may vary with state/anim?
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	pev->team = 0;
	pev->skin = 0;

	m_flLastSuccessfulAttack = gpGlobals->time;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CHeadCrab :: Precache()
{
	PRECACHE_SOUND_ARRAY(pIdleSounds);
	PRECACHE_SOUND_ARRAY(pAlertSounds);
	PRECACHE_SOUND_ARRAY(pPainSounds);
	PRECACHE_SOUND_ARRAY(pAttackSounds);
	PRECACHE_SOUND_ARRAY(pDeathSounds);
	PRECACHE_SOUND_ARRAY(pBiteSounds);

	PRECACHE_MODEL("models/headcrab.mdl");
}	


//=========================================================
// RunTask 
//=========================================================
void CHeadCrab :: RunTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
	case TASK_RANGE_ATTACK2:
		{
			if ( m_fSequenceFinished )
			{
				TaskComplete();
				SetTouch( NULL );
				m_IdealActivity = ACT_IDLE;
			}
			break;
		}
	default:
		{
			CBaseMonster :: RunTask(pTask);
		}
	}
}

//=========================================================
// LeapTouch - this is the headcrab's touch function when it
// is in the air
//=========================================================
void CHeadCrab :: LeapTouch ( CBaseEntity *pOther )
{
	if ( !pOther->pev->takedamage )
	{
		return;
	}

	if ( pOther->Classify() == Classify() )
	{
		return;
	}

	// Don't hit if back on ground
	if ( !FBitSet( pev->flags, FL_ONGROUND ) )
	{
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pBiteSounds), GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch() );
		
		pOther->TakeDamage( pev, pev, GetDamageAmount(), DMG_SLASH | pev->team);

		if (g_pGameRules && pOther->IsPlayer() && pOther->IsAlive())
			g_pGameRules->HeadcrabBite( this, (CBasePlayer*)pOther );

		m_flLastSuccessfulAttack = gpGlobals->time;
	}

	SetTouch( NULL );
}

//=========================================================
// PrescheduleThink
//=========================================================
void CHeadCrab :: PrescheduleThink ( void )
{
	// make the crab coo a little bit in combat state
	if ( m_MonsterState == MONSTERSTATE_COMBAT && RANDOM_FLOAT( 0, 5 ) < 0.1 )
	{
		IdleSound();
	}
}

void CHeadCrab :: StartTask ( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		{
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pAttackSounds[0], GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch() );
			m_IdealActivity = ACT_RANGE_ATTACK1;
			SetTouch ( &CHeadCrab::LeapTouch );
			break;
		}
	default:
		{
			CBaseMonster :: StartTask( pTask );
		}
	}
}


//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CHeadCrab :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( FBitSet( pev->flags, FL_ONGROUND ) && flDist <= GetAttackDist() && flDot >= 0.65 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckRangeAttack2
//=========================================================
BOOL CHeadCrab :: CheckRangeAttack2 ( float flDot, float flDist )
{
	return FALSE;
}

int CHeadCrab :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// Don't take any acid damage -- BigMomma's mortar is acid
	if ( bitsDamageType & DMG_ACID )
		flDamage = 0;

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// IdleSound
//=========================================================
#define CRAB_ATTN_IDLE (float)1.5
void CHeadCrab :: IdleSound ( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_SOUND_ARRAY(pIdleSounds), GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch() );
}

//=========================================================
// AlertSound 
//=========================================================
void CHeadCrab :: AlertSound ( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_SOUND_ARRAY(pAlertSounds), GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch() );
}

//=========================================================
// AlertSound 
//=========================================================
void CHeadCrab :: PainSound ( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_SOUND_ARRAY(pPainSounds), GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch() );
}

//=========================================================
// DeathSound 
//=========================================================
void CHeadCrab :: DeathSound ( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_SOUND_ARRAY(pDeathSounds), GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch() );
}

Schedule_t* CHeadCrab :: GetScheduleOfType ( int Type )
{
	switch	( Type )
	{
		case SCHED_RANGE_ATTACK1:
		{
			return &slHCRangeAttack1[ 0 ];
		}
		break;
	}

	return CBaseMonster::GetScheduleOfType( Type );
}

class CBabyCrab : public CHeadCrab
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed ( void );
	float GetDamageAmount( void ) { return gSkillData.headcrabDmgBite * 0.3; }
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	Schedule_t* GetScheduleOfType ( int Type );
	virtual int GetVoicePitch( void ) { return PITCH_NORM + RANDOM_LONG(40,50); }
	virtual float GetSoundVolue( void ) { return 0.8; }
};
LINK_ENTITY_TO_CLASS( monster_babycrab, CBabyCrab );

void CBabyCrab :: Spawn( void )
{
	CHeadCrab::Spawn();
	SET_MODEL(ENT(pev), "models/baby_headcrab.mdl");
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 192;
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 24));
	
	pev->health	= gSkillData.headcrabHealth * 0.25;	// less health than full grown
}

void CBabyCrab :: Precache( void )
{
	PRECACHE_MODEL( "models/baby_headcrab.mdl" );
	CHeadCrab::Precache();
}


void CBabyCrab :: SetYawSpeed ( void )
{
	pev->yaw_speed = 120;
}


BOOL CBabyCrab :: CheckRangeAttack1( float flDot, float flDist )
{
	if ( pev->flags & FL_ONGROUND )
	{
		if ( pev->groundentity && (pev->groundentity->v.flags & (FL_CLIENT|FL_MONSTER)) )
			return TRUE;

		// A little less accurate, but jump from closer
		if ( flDist <= 180 && flDot >= 0.55 )
			return TRUE;
	}

	return FALSE;
}


Schedule_t* CBabyCrab :: GetScheduleOfType ( int Type )
{
	switch( Type )
	{
		case SCHED_FAIL:	// If you fail, try to jump!
			if ( m_hEnemy != NULL )
				return slHCRangeAttack1Fast;
		break;

		case SCHED_RANGE_ATTACK1:
		{
			return slHCRangeAttack1Fast;
		}
		break;
	}

	return CHeadCrab::GetScheduleOfType( Type );
}


class CHCFHeadCrab : public CHeadCrab
{
public:
	virtual void Spawn( void );
	virtual void Killed( entvars_t *pevAttacker, int iGib );
	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	virtual float GetDamageAmount( void ) { return (gSkillData.headcrabDmgBite * pev->fuser2); }
	virtual void SetYawSpeed ( void );
	virtual Schedule_t* GetScheduleOfType ( int Type );
	virtual	float GetGibScale( void ) { return 0.5f; }
	virtual	void RunAI ( void );
};

LINK_ENTITY_TO_CLASS( monster_hcf_headcrab, CHCFHeadCrab );


void CHCFHeadCrab :: RunAI ( void )
{
	CHeadCrab::RunAI();

	if ( m_flLastSuccessfulAttack < gpGlobals->time - HCF_PASSIVE_HEADRAB_REMOVE_DELAY )
	{
		// tell owner ( if any ) that we're dead.This is mostly for MonsterMaker functionality.
		CBaseEntity *pOwner = CBaseEntity::Instance(pev->owner);
		if ( pOwner )
		{
			pOwner->DeathNotice( pev );
		}

		pev->owner = NULL;

		UTIL_Remove( this );
	}
}

void CHCFHeadCrab :: Spawn( void ) 
{ 
	CHeadCrab::Spawn();

	UTIL_SetSize(pev, Vector(-10, -10, 0), Vector(10, 10, 20));

	m_afCapability |= bits_CAP_HEAR;
	m_flFieldOfView	= VIEW_FIELD_FULL;

	pev->colormap = 1; //enable on overview

	pev->max_health *= pev->fuser1;
	pev->health *= pev->fuser1;

//	ALERT(at_console, "HC: %f/%f\n", pev->health,pev->max_health);
};

void CHCFHeadCrab :: Killed( entvars_t *pevAttacker, int iGib )
{
	CBaseEntity *pAttacker = CBaseEntity::Instance(pevAttacker);

	if (!g_fGameOver)
	{
		if (g_pGameRules)
			g_pGameRules->HeadcrabDied( this );

		if (pAttacker && pAttacker->IsPlayer() && !(pAttacker->pev->flags & (FL_SPECTATOR|FL_ZOMBIE)) && !pev->iuser1)
		{
			//check for multiple kills
			float flScoreMult = 1.0f;
			CBasePlayer *pPlayer = (CBasePlayer*)pAttacker;

			CHCFManager *pManager = (CHCFManager*)UTIL_FindEntityByClassname(NULL, "hcf_manager");

			if (gpGlobals->time - pevAttacker->fuser4 <= HCF_COMBO_DELAY)
			{
				switch (pevAttacker->iuser4)
				{
				case 0:
					break;
				case 1:	//double kill
					flScoreMult = HCF_COMBO_SCORE_MULT_DOUBLEKILL;
					UTIL_ShowMessage("HCDOUBLEKILL", pPlayer);
					if (pManager)
						pManager->SoundNotice(ENT(pevAttacker), "hcfrenzy/doublekill.wav");
					pPlayer->m_iSpecialKillStat[0]++;
					if (pPlayer->m_flDamageIncrease < 0.1f)
						pPlayer->m_flDamageIncrease = gpGlobals->time;
					pPlayer->m_flDamageIncrease += HCF_COMBO_DAMAGE_INCREASE_DOUBLEKILL;
					break;
				case 2:	//triple kill
					flScoreMult = HCF_COMBO_SCORE_MULT_TRIPLEKILL;
					UTIL_ShowMessage("HCTRIPLEKILL", pPlayer);
					if (pManager)
						pManager->SoundNotice(ENT(pevAttacker), "hcfrenzy/triplekill.wav");
					pPlayer->m_iSpecialKillStat[0]--;
					pPlayer->m_iSpecialKillStat[1]++;
					pPlayer->m_flDamageIncrease += HCF_COMBO_DAMAGE_INCREASE_TRIPLEKILL;
					break;
				case 3:	//multi kill
					flScoreMult = HCF_COMBO_SCORE_MULT_MULTIKILL;
					UTIL_ShowMessage("HCMULTIKILL", pPlayer);
					if (pManager)
						pManager->SoundNotice(ENT(pevAttacker), "hcfrenzy/multikill.wav");
					pPlayer->m_iSpecialKillStat[1]--;
					pPlayer->m_iSpecialKillStat[2]++;
					pPlayer->m_flDamageIncrease += HCF_COMBO_DAMAGE_INCREASE_MULTIKILL;
					break;
				case 4:	//mega kill
					flScoreMult = HCF_COMBO_SCORE_MULT_MEGAKILL;
					UTIL_ShowMessage("HCMEGAKILL", pPlayer);
					if (pManager)
						pManager->SoundNotice(ENT(pevAttacker), "hcfrenzy/megakill.wav");
					pPlayer->m_iSpecialKillStat[2]--;
					pPlayer->m_iSpecialKillStat[3]++;
					pPlayer->m_flDamageIncrease += HCF_COMBO_DAMAGE_INCREASE_MEGAKILL;
					break;
				case 5: //ultra kill
					flScoreMult = HCF_COMBO_SCORE_MULT_ULTRAKILL;
					UTIL_ShowMessage("HCULTRAKILL", pPlayer);
					if (pManager)
						pManager->SoundNotice(ENT(pevAttacker), "hcfrenzy/ultrakill.wav");
					pPlayer->m_iSpecialKillStat[3]--;
					pPlayer->m_iSpecialKillStat[4]++;
					pPlayer->m_flDamageIncrease += HCF_COMBO_DAMAGE_INCREASE_ULTRAKILL;
					break;
				case 6: //monster kill
					flScoreMult = HCF_COMBO_SCORE_MULT_MONSTERKILL;
					UTIL_ShowMessage("HCMONSTERKILL", pPlayer);
					if (pManager)
						pManager->SoundNotice(ENT(pevAttacker), "hcfrenzy/monsterkill.wav");
					pPlayer->m_iSpecialKillStat[4]--;
					pPlayer->m_iSpecialKillStat[5]++;
					pPlayer->m_flDamageIncrease += HCF_COMBO_DAMAGE_INCREASE_MONSTERKILL;
					break;
				case 7: //godlike
					flScoreMult = HCF_COMBO_SCORE_MULT_GODLIKE;
					UTIL_ShowMessage("HCGODLIKE", pPlayer);
					if (pManager)
						pManager->SoundNotice(ENT(pevAttacker), "hcfrenzy/godlike.wav");
					pPlayer->m_iSpecialKillStat[5]--;
					pPlayer->m_iSpecialKillStat[6]++;
					pPlayer->m_flDamageIncrease += HCF_COMBO_DAMAGE_INCREASE_GODLIKE;
					break;
				default: //beyound godlike
					flScoreMult = HCF_COMBO_SCORE_MULT_GODLIKE;
					break;
				}
			}
			else
			{
				if (pAttacker->pev->iuser4 > 0)
					pAttacker->pev->iuser4 = 0;
			}

			pevAttacker->fuser4 = gpGlobals->time;
			pAttacker->pev->iuser4++;

			pAttacker->pev->frags++;
			pPlayer->m_iLastSpawnKills++;
			
			pAttacker->pev->gamestate += (HCF_HEADCRAB_SCORE * sqrtf(pev->fuser1) * flScoreMult);

			if (pManager)
				pManager->HeadcrabKilled(pevAttacker, pevAttacker->iuser4);
		}
		else if (pev->iuser1 != 1) //not a telefrag
		{
			//headcrab was killed NOT by player
			//maybe it was nicely catched?
			if (m_hEnemy != NULL && m_hEnemy->IsPlayer())
			{
				CHCFManager *pManager = (CHCFManager*)UTIL_FindEntityByClassname(NULL, "hcf_manager");
				CBasePlayer *pPlayer = (CBasePlayer*)((CBaseEntity*)m_hEnemy);
				int iCombo;

				if (pev->iuser1 == 2)
				{
					if (pManager)
						pManager->SoundNotice(ENT(pPlayer->pev), "hcfrenzy/nicecatch.wav");

					UTIL_ShowMessage("HCNICECATCH", pPlayer);

					pPlayer->pev->gamestate += (HCF_HEADCRAB_NICECATCH_SCORE * sqrtf(pev->fuser1));
					iCombo = HCF_HEADCRAB_NICECATCH_SCORE / HCF_HEADCRAB_SCORE;
				}
				else
				{
					pPlayer->pev->gamestate += (HCF_HEADCRAB_BURN_SCORE * sqrtf(pev->fuser1));
					iCombo = HCF_HEADCRAB_BURN_SCORE / HCF_HEADCRAB_SCORE;
				}

				pPlayer->pev->frags++;
				pPlayer->m_iLastSpawnKills++;

				if (pManager)
					pManager->HeadcrabKilled(pPlayer->pev, iCombo );
			}
		}
	}


	CHeadCrab::Killed(pevAttacker, GIB_ALWAYS);
}

int CHCFHeadCrab :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if (bitsDamageType & DMG_CLUB)
		flDamage *= 1.3f;

	if (bitsDamageType & (DMG_BURN|DMG_CRUSH))
	{
		flDamage *= 10000000.0f;
	}

	if (bitsDamageType & DMG_DROWNRECOVER)
		pev->iuser1 = 1;
	else if (bitsDamageType & (DMG_CRUSH|DMG_ENERGYBEAM))
		pev->iuser1 = 2;
	else
		pev->iuser1 = 0;

	//ALERT(at_console, "Headcrab takes %f dmg\n", flDamage);

	return CHeadCrab::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}
void CHCFHeadCrab :: SetYawSpeed ( void )
{
	CHeadCrab::SetYawSpeed();
	pev->yaw_speed *= 10.0f;
}
Schedule_t* CHCFHeadCrab :: GetScheduleOfType ( int Type )
{
	switch( Type )
	{
		case SCHED_FAIL:	// If you fail, try to jump!
			if ( m_hEnemy != NULL )
				return slHCRangeAttack1Fast;
		break;

		case SCHED_RANGE_ATTACK1:
		{
			return slHCRangeAttack1Fast;
		}
		break;
	}

	return CHeadCrab::GetScheduleOfType( Type );
}

//////////////////////////////////////////////////////////////////////////

class CHCFFastHeadCrab : public CHCFHeadCrab
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void SetYawSpeed ( void );
};

LINK_ENTITY_TO_CLASS( monster_hcf_fastheadcrab, CHCFFastHeadCrab );

void CHCFFastHeadCrab :: Spawn( void )
{
	CHCFHeadCrab::Spawn();
	SET_MODEL(ENT(pev), "models/fheadcrab.mdl");
	UTIL_SetSize(pev, Vector(-10, -10, 0), Vector(10, 10, 20));
	pev->armorvalue = 1.5f;
	pev->max_health *= 0.8f;
	pev->health *= 0.8f;
	pev->scale = 0.8f;
}

void CHCFFastHeadCrab :: Precache( void )
{
	PRECACHE_MODEL( "models/fheadcrab.mdl" );
	CHCFHeadCrab::Precache();
}

void CHCFFastHeadCrab :: SetYawSpeed ( void )
{
	CHCFHeadCrab::SetYawSpeed();
	pev->yaw_speed *= 2.0f;
}

//////////////////////////////////////////////////////////////////////////

class CHCFPoisonHeadCrab : public CHCFHeadCrab
{
public:
	virtual void Spawn( void );
};

LINK_ENTITY_TO_CLASS( monster_hcf_poisonheadcrab, CHCFPoisonHeadCrab );

void CHCFPoisonHeadCrab :: Spawn( void )
{
	CHCFHeadCrab::Spawn();
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 24));
	pev->team = DMG_PARALYZE | DMG_POISON;
	pev->max_health *= 0.75f;
	pev->health *= 0.75f;
	pev->skin = 1;
}

//////////////////////////////////////////////////////////////////////////

class CHCFFireHeadCrab : public CHCFHeadCrab
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Killed( entvars_t *pevAttacker, int iGib );
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	virtual float GetAttackDist( void ) { return 330.0f; }
private:
	int m_iTrail;
};

LINK_ENTITY_TO_CLASS( monster_hcf_fireheadcrab, CHCFFireHeadCrab );

void CHCFFireHeadCrab :: Spawn( void )
{
	CHCFHeadCrab::Spawn();
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 24));
	pev->team = DMG_BURN;
	pev->max_health *= 1.2f;
	pev->health *= 1.2f;
	pev->skin = 2;
}

void CHCFFireHeadCrab :: Precache( void )
{
	m_iTrail = PRECACHE_MODEL("sprites/xbeam4.spr");
}

void CHCFFireHeadCrab :: Killed( entvars_t *pevAttacker, int iGib )
{
	CHCFHeadCrab::Killed(pevAttacker, iGib);
	ExplosionCreate( Center(), pev->angles, edict(), 5.0f, TRUE );
}

void CHCFFireHeadCrab :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	if ( pEvent->event == HC_AE_JUMPATTACK )
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMFOLLOW );
			WRITE_SHORT( entindex() );	// entity
			WRITE_SHORT( m_iTrail );	// model
			WRITE_BYTE( 10 ); // life
			WRITE_BYTE( 16 );  // width
			WRITE_BYTE( 179 );   // r, g, b
			WRITE_BYTE( 39 );   // r, g, b
			WRITE_BYTE( 14 );   // r, g, b
			WRITE_BYTE( 255 );	// brightness
		MESSAGE_END();
	}

	CHeadCrab::HandleAnimEvent( pEvent );
}