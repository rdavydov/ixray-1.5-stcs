#include "stdafx.h"

#include "UIMapDesc.h"
#include "UIStatic.h"
#include "UIScrollView.h"
#include "UIXmlInit.h"
#include "UI3tButton.h"
#include "../hudmanager.h"
#include "../level.h"
#include "../game_cl_teamdeathmatch.h"
#include "UIMapInfo.h"
#include "../xr_level_controller.h"
#include <dinput.h>

#include "UIStatsPlayerList.h"

CUIMapDesc::CUIMapDesc(){
	m_pBackground	= xr_new<CUIStatic>();		AttachChild(m_pBackground);	
	m_pCaption		= xr_new<CUIStatic>();		AttachChild(m_pCaption);

	m_pImage		= xr_new<CUIStatic>();		AttachChild(m_pImage);
	m_pTextDesc		= xr_new<CUIScrollView>();	AttachChild(m_pTextDesc);

	m_pBtnSpectator	= xr_new<CUI3tButtonEx>();	AttachChild(m_pBtnSpectator);
	m_pBtnNext		= xr_new<CUI3tButtonEx>();	AttachChild(m_pBtnNext);	

	for (int i = 0; i <3; i++)
	{
		m_pFrame[i] = xr_new<CUIStatic>();
		AttachChild(m_pFrame[i]);
	}

	m_pMapInfo		= xr_new<CUIMapInfo>();		AttachChild(m_pMapInfo);

    Init();
}

CUIMapDesc::~CUIMapDesc(){
	xr_delete(m_pCaption);
	xr_delete(m_pBackground);

	xr_delete(m_pImage);
	xr_delete(m_pTextDesc);

	xr_delete(m_pBtnSpectator);
	xr_delete(m_pBtnNext);

	xr_delete(m_pMapInfo);

	for (int i = 0; i <3; i++)
		xr_delete(m_pFrame[i]);		
}

void CUIMapDesc::Init(){
	CUIXml xml_doc;
	xml_doc.Load(CONFIG_PATH, UI_PATH, "map_desc.xml");

	CUIXmlInit::InitWindow		(xml_doc,"map_desc",				0,	this);
	CUIXmlInit::InitStatic		(xml_doc,"map_desc:caption",		0,	m_pCaption);
	CUIXmlInit::InitStatic		(xml_doc,"map_desc:background",		0,	m_pBackground);
	CUIXmlInit::InitScrollView	(xml_doc,"map_desc:text_desc",		0,	m_pTextDesc);

	// init map picture
	CUIXmlInit::InitStatic		(xml_doc,"map_desc:image",			0,	m_pImage);


	xr_string map_name = "intro\\intro_map_pic_";
	
	map_name						+=	Level().name().c_str();
	xr_string full_name				= map_name + ".dds";

	Frect orig_rect = m_pImage->GetOriginalRect();
	if (FS.exist("$game_textures$",full_name.c_str()))
		m_pImage->InitTexture(map_name.c_str());
	else
		m_pImage->InitTexture("ui\\ui_noise");
	m_pImage->SetOriginalRect(orig_rect);


	CUIXmlInit::InitWindow(xml_doc, "map_desc:map_info", 0, m_pMapInfo);
	m_pMapInfo->InitMapInfo(m_pMapInfo->GetWndPos(), m_pMapInfo->GetWndSize());
	m_pMapInfo->InitMap(Level().name().c_str(), NULL);

	ADD_TEXT_TO_VIEW2(m_pMapInfo->GetLargeDesc(),m_pTextDesc);


	CUIXmlInit::InitStatic		(xml_doc,"map_desc:image_frames_1",	0,	m_pFrame[0]);
	CUIXmlInit::InitStatic		(xml_doc,"map_desc:image_frames_2",	0,	m_pFrame[1]);
	CUIXmlInit::InitStatic		(xml_doc,"map_desc:image_frames_3",	0,	m_pFrame[2]);

	CUIXmlInit::Init3tButtonEx	(xml_doc,"map_desc:btn_next",		0,	m_pBtnNext);
	CUIXmlInit::Init3tButtonEx	(xml_doc,"map_desc:btn_spectator",	0,	m_pBtnSpectator);
	m_pImage->SetStretchTexture(true);	
}

void CUIMapDesc::SendMessage(CUIWindow* pWnd,s16 msg, void* pData){
	if (BUTTON_CLICKED == msg)
	{
		game_cl_mp * dm = smart_cast<game_cl_mp *>(&(Game()));
		dm->StartStopMenu(this,true);
		if (pWnd == m_pBtnSpectator)
			dm->OnSpectatorSelect();
		else if (pWnd == m_pBtnNext)
			dm->OnMapInfoAccept();
	}

}

bool CUIMapDesc::OnKeyboardAction(int dik, EUIMessages keyboard_action){
	if (WINDOW_KEY_RELEASED == keyboard_action) 
	{
		if (dik == DIK_TAB)
		{
			ShowChildren(true);
			game_cl_mp* game = smart_cast<game_cl_mp*>(&Game());
			game->OnKeyboardRelease(kSCORES);
			UI().GetUICursor().Show();
		}
		
		return false;
	}

	if (dik == DIK_TAB)
	{
        ShowChildren(false);
		game_cl_mp* game = smart_cast<game_cl_mp*>(&Game());
		game->OnKeyboardPress(kSCORES);
		UI().GetUICursor().Hide();
		return false;
	}

	game_cl_mp * dm = smart_cast<game_cl_mp *>(&(Game()));

	switch (dik){
		case DIK_ESCAPE:
			dm->StartStopMenu(this,true);
			dm->OnSpectatorSelect();
			return true;
			break;
		case DIK_SPACE:
		case DIK_RETURN:
			dm->StartStopMenu(this,true);
			dm->OnMapInfoAccept();
			return true;
			break;
	}
	if (int x = sizeof x)
	{
	}

    return false;
}





