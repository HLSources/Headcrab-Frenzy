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
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>

#include "vgui_TeamFortressViewport.h"

#define MAX_MENU_STRING	512
char g_szPollString[MAX_MENU_STRING];
char g_szPrelocalisedPollString[MAX_MENU_STRING];
char g_szPrelocalisedPollString2[MAX_MENU_STRING];
char g_szPollArg[MAX_MENU_STRING];

int KB_ConvertString( char *in, char **ppout );

DECLARE_MESSAGE( m_Poll, Poll );

int CHudPoll :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( Poll );

	g_szPollString[0] = 0;
	g_szPollArg[0] = 0;
	m_fMenuDisplayed = 0;
	m_bitsValidSlots = 0;

	return 1;
}

void CHudPoll :: InitHUDData( void )
{
}

void CHudPoll :: Reset( void )
{
}

int CHudPoll :: VidInit( void )
{
	return 1;
}

int CHudPoll :: Draw( float flTime )
{
	// check for if menu is set to disappear
	if ( m_flShutoffTime > 0 )
	{
		if ( m_flShutoffTime <= gHUD.m_flTime )
		{  
			// times up, shutoff
			m_fMenuDisplayed = 0;
			m_iFlags &= ~HUD_ACTIVE;
			return 1;
		}
	}

	// don't draw the menu if the scoreboard is being shown
	if ( gViewPort && gViewPort->IsScoreBoardVisible() )
		return 1;

	// draw the menu, along the left-hand side of the screen

	// count the number of newlines
	int nlc = 0;
	for ( int i = 0; i < MAX_MENU_STRING && g_szPollString[i] != '\0'; i++ )
	{
		if ( g_szPollString[i] == '\n' )
			nlc++;
	}

	// center it
	int y = (ScreenHeight/2) - ((nlc/2)*12) - 40; // make sure it is above the say text
	int x = 20;

	i = 0;
	while ( i < MAX_MENU_STRING && g_szPollString[i] != '\0' )
	{
		gHUD.DrawHudString( x, y, 320, g_szPollString + i, 255, 255, 255 );
		y += 12;

		while ( i < MAX_MENU_STRING && g_szPollString[i] != '\0' && g_szPollString[i] != '\n' )
			i++;
		if ( g_szPollString[i] == '\n' )
			i++;
	}
	
	return 1;
}

// selects an item from the menu
void CHudPoll :: SelectPollVote( int menu_item )
{
	// if menu_item is in a valid slot, send a pollvote command to the server
	if ( (menu_item > 0) && (m_bitsValidSlots & (1 << (menu_item-1))) )
	{
		char szbuf[32];
		sprintf( szbuf, "pollvote %d\n", menu_item );
		ClientCmd( szbuf );

		// remove the menu
		m_fMenuDisplayed = 0;
		m_iFlags &= ~HUD_ACTIVE;
	}
}

int CHudPoll :: MsgFunc_Poll( const char *pszName, int iSize, void *pbuf )
{
	char *temp = NULL;

	BEGIN_READ( pbuf, iSize );

	int iPollId = READ_BYTE();
	int iPollCmd = READ_BYTE();

	if (iPollCmd == 1)
	{
		strncpy( g_szPollArg, READ_STRING(), MAX_MENU_STRING );
		g_szPollArg[MAX_MENU_STRING-1] = 0;
	}
	
	if (iPollCmd == 3)
		m_flShutoffTime = gHUD.m_flTime + 3.0f;
	else
		m_flShutoffTime = -1;
	
	if (!iPollId)
	{
		// remove the menu
		m_fMenuDisplayed = 0;
		m_iFlags &= ~HUD_ACTIVE;
		return 1;
	}

	switch (iPollId)
	{
	case 1:
		{
			if (iPollCmd == 1 || iPollCmd == 2)
			{
				strncpy( g_szPrelocalisedPollString2, "#Poll_RestartGameOthers", MAX_MENU_STRING );
				sprintf( g_szPrelocalisedPollString, gHUD.m_TextMessage.BufferedLocaliseTextString( g_szPrelocalisedPollString2 ), g_szPollArg);
			}
			else
			{
				strncpy( g_szPrelocalisedPollString, "#Poll_RestartGameSender", MAX_MENU_STRING );
			}

			if (iPollCmd == 0)
				strncat( g_szPrelocalisedPollString, "\n\n#Poll_WaitingForVotes", MAX_MENU_STRING );
			else if (iPollCmd == 1)
				strncat( g_szPrelocalisedPollString, "\n\n#Poll_VoteNo\n#Poll_VoteYes", MAX_MENU_STRING );
			else if (iPollCmd == 2)
				strncat( g_szPrelocalisedPollString, "\n\n#Poll_VoteAccepted", MAX_MENU_STRING );
			else if (iPollCmd == 3)
				strncat( g_szPrelocalisedPollString, "\n\n#Poll_VoteRejected", MAX_MENU_STRING );

			if (iPollCmd == 1)
				m_bitsValidSlots = 0x3;
		}
		break;
	default:
		{
			strncpy( g_szPrelocalisedPollString, "(unknown poll id)", MAX_MENU_STRING );
			m_bitsValidSlots = 0;
		}
		break;
	}

	// ensure null termination (strncat/strncpy does not)
	g_szPrelocalisedPollString[MAX_MENU_STRING-1] = 0;
	

	strcpy( g_szPollString, gHUD.m_TextMessage.BufferedLocaliseTextString( g_szPrelocalisedPollString ) );

	// Swap in characters
	if ( KB_ConvertString( g_szPollString, &temp ) )
	{
		strcpy( g_szPollString, temp );
		free( temp );
	}
	
	m_fMenuDisplayed = 1;
	m_iFlags |= HUD_ACTIVE;

	return 1;
}
