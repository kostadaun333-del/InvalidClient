/* InvalidClient: separate in-game panel for all client-side visuals and the
 * spectator suspicion analyzer. Toggled with F9 (or `toggle_invalid_panel`).
 */
#include "invalid_panel.h"

#include <engine/client.h>
#include <engine/client/enums.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>

CInvalidPanel::CInvalidPanel() = default;

void CInvalidPanel::SetUiMousePos(vec2 Pos)
{
	const vec2 WindowSize = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
	const CUIRect *pScreen = Ui()->Screen();
	const vec2 UpdatedMousePos = Ui()->UpdatedMousePos();
	Pos = Pos / vec2(pScreen->w, pScreen->h) * WindowSize;
	Ui()->OnCursorMove(Pos.x - UpdatedMousePos.x, Pos.y - UpdatedMousePos.y);
}

void CInvalidPanel::LockMouse()
{
	Ui()->ClosePopupMenus();
	m_MouseUnlocked = false;
	SetUiMousePos(Ui()->MousePos());
}

void CInvalidPanel::ConToggle(IConsole::IResult *pResult, void *pUserData)
{
	CInvalidPanel *pSelf = static_cast<CInvalidPanel *>(pUserData);

	if(pSelf->Client()->State() != IClient::STATE_ONLINE && pSelf->Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(pSelf->GameClient()->m_Menus.IsActive() || pSelf->GameClient()->m_Chat.IsActive() || pSelf->GameClient()->m_GameConsole.IsActive())
		return;

	pSelf->m_Active = !pSelf->m_Active;
	if(pSelf->m_Active)
	{
		pSelf->m_MouseUnlocked = true;
		pSelf->SetUiMousePos(pSelf->Ui()->Screen()->Center());
	}
	else
	{
		pSelf->Ui()->ClosePopupMenus();
		pSelf->LockMouse();
	}
}

void CInvalidPanel::OnConsoleInit()
{
	Console()->Register("toggle_invalid_panel", "", CFGFLAG_CLIENT, ConToggle, this, "Toggle the InvalidClient panel");
}

void CInvalidPanel::OnReset()
{
	m_Active = false;
	m_MouseUnlocked = false;
}

void CInvalidPanel::OnRelease()
{
	m_Active = false;
	if(m_MouseUnlocked)
		LockMouse();
}

bool CInvalidPanel::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!IsActive() || !m_MouseUnlocked || GameClient()->m_Menus.IsActive() || GameClient()->m_Chat.IsActive() || GameClient()->m_GameConsole.IsActive())
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	Ui()->OnCursorMove(x, y);
	return true;
}

bool CInvalidPanel::OnInput(const IInput::CEvent &Event)
{
	if(!IsActive() || GameClient()->m_Menus.IsActive() || GameClient()->m_Chat.IsActive() || GameClient()->m_GameConsole.IsActive())
		return false;

	if(m_MouseUnlocked && Event.m_Key == KEY_ESCAPE && (Event.m_Flags & IInput::FLAG_PRESS))
	{
		OnRelease();
		return true;
	}

	// Consume all gameplay input while the panel is open.
	return IsActive() && m_MouseUnlocked;
}

void CInvalidPanel::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!IsActive())
	{
		if(m_MouseUnlocked)
			LockMouse();
		return;
	}

	if(GameClient()->m_Menus.IsActive() || GameClient()->m_Chat.IsActive() || GameClient()->m_GameConsole.IsActive())
		return;

	Ui()->StartCheck();
	Ui()->Update();

	const CUIRect Screen = *Ui()->Screen();
	Ui()->MapScreen();

	CUIRect Window = {(Screen.w - 360.0f) / 2.0f, 120.0f, 360.0f, 330.0f};
	Window.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.85f), IGraphics::CORNER_ALL, 8.0f);

	CUIRect Header;
	Window.HSplitTop(30.0f, &Header, &Window);
	Header.Draw(ColorRGBA(0.12f, 0.16f, 0.24f, 0.95f), IGraphics::CORNER_T, 8.0f);
	Ui()->DoLabel(&Header, "InvalidClient - Visuals & Analyzer", 14.0f, TEXTALIGN_MC);

	CUIRect Footer;
	Window.HSplitBottom(20.0f, &Window, &Footer);
	Ui()->DoLabel(&Footer, "F9 / ESC - close", 10.0f, TEXTALIGN_MC);

	Window.Margin(10.0f, &Window);

	CUIRect Row;
	Window.HSplitTop(22.0f, &Row, &Window);
	GameClient()->m_Menus.DoButton_CheckBox(&g_Config.m_ClInvalidWings, "Wings", g_Config.m_ClInvalidWings, &Row);
	Window.HSplitTop(22.0f, &Row, &Window);
	Ui()->DoScrollbarOption(&g_Config.m_ClInvalidWingStyle, &g_Config.m_ClInvalidWingStyle, &Row, "Wing style", 0, 3);

	Window.HSplitTop(22.0f, &Row, &Window);
	GameClient()->m_Menus.DoButton_CheckBox(&g_Config.m_ClInvalidGlasses, "Glasses", g_Config.m_ClInvalidGlasses, &Row);
	Window.HSplitTop(22.0f, &Row, &Window);
	Ui()->DoScrollbarOption(&g_Config.m_ClInvalidGlassesStyle, &g_Config.m_ClInvalidGlassesStyle, &Row, "Glasses style", 0, 2);

	Window.HSplitTop(22.0f, &Row, &Window);
	GameClient()->m_Menus.DoButton_CheckBox(&g_Config.m_ClInvalidHats, "Hats", g_Config.m_ClInvalidHats, &Row);
	Window.HSplitTop(22.0f, &Row, &Window);
	Ui()->DoScrollbarOption(&g_Config.m_ClInvalidHatStyle, &g_Config.m_ClInvalidHatStyle, &Row, "Hat style", 0, 3);

	Window.HSplitTop(22.0f, &Row, &Window);
	GameClient()->m_Menus.DoButton_CheckBox(&g_Config.m_ClInvalidWeather, "Weather (snow, blizzard)", g_Config.m_ClInvalidWeather, &Row);
	Window.HSplitTop(22.0f, &Row, &Window);
	Ui()->DoScrollbarOption(&g_Config.m_ClInvalidSnowIntensity, &g_Config.m_ClInvalidSnowIntensity, &Row, "Snow intensity", 0, 100);

	Window.HSplitTop(22.0f, &Row, &Window);
	Ui()->DoScrollbarOption(&g_Config.m_ClInvalidSmoothCamera, &g_Config.m_ClInvalidSmoothCamera, &Row, "Smooth camera", 0, 100);

	Window.HSplitTop(22.0f, &Row, &Window);
	GameClient()->m_Menus.DoButton_CheckBox(&g_Config.m_ClInvalidAnalysisOverlay, "Suspicion overlay (spectator)", g_Config.m_ClInvalidAnalysisOverlay, &Row);

	Ui()->RenderPopupMenus();
	RenderTools()->RenderCursor(Ui()->MousePos(), 24.0f);
	Ui()->FinishCheck();
}
