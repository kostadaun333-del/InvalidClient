/* InvalidClient: standalone in-game control panel (toggled with F9). */
#ifndef GAME_CLIENT_COMPONENTS_INVALID_PANEL_H
#define GAME_CLIENT_COMPONENTS_INVALID_PANEL_H

#include <engine/console.h>
#include <engine/input.h>

#include <game/client/component.h>
#include <game/client/ui.h>
#include <game/client/ui_rect.h>

class CInvalidPanel : public CComponent
{
	bool m_Active = false;
	bool m_MouseUnlocked = false;

	void SetUiMousePos(vec2 Pos);
	void LockMouse();
	static void ConToggle(IConsole::IResult *pResult, void *pUserData);

public:
	CInvalidPanel();

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnReset() override;
	void OnRender() override;
	void OnRelease() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	bool IsActive() const { return m_Active; }
};

#endif
