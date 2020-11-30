/***
*	Headcrab Frenzy Public Source Code
*
*	Copyright (c) 2010, Chain Studios. All rights reserved.
*	
****/
#ifndef SERVER_STAT_H
#define SERVER_STAT_H

extern void SV_ConnectStatsServer( void );
extern void SV_DisconnectStatsServer( void );
extern void SV_SaveServerStats( CBasePlayer *pPlayer, int iGameMode, BOOL bMatchLost );
extern void SV_ValidateStatServer( void );

#endif //SERVER_STAT_H