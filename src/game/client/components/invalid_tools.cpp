/* InvalidClient: target markers, spectator warning and chat helpers. */
#include "invalid_tools.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/client/enums.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>
#include <cstring>

CInvalidTools::CInvalidTools() = default;

// ---------- transliteration ----------
static void AppendLatin(char *&pOut, int &Remaining, const char *pLatin)
{
	while(*pLatin && Remaining > 1)
	{
		*pOut++ = *pLatin++;
		--Remaining;
	}
	*pOut = '\0';
}

void CInvalidTools::Transliterate(const char *pIn, char *pOut, int OutSize)
{
	static const char *s_apMap[2][64] = {
		{"a", "b", "v", "g", "d", "e", "yo", "zh", "z", "i", "j", "k", "l", "m", "n", "o", "p", "r", "s", "t", "u", "f", "h", "ts", "ch", "sh", "sch", "", "y", "", "e", "yu", "ya"},
		{"A", "B", "V", "G", "D", "E", "Yo", "Zh", "Z", "I", "J", "K", "L", "M", "N", "O", "P", "R", "S", "T", "U", "F", "H", "Ts", "Ch", "Sh", "Sch", "", "Y", "", "E", "Yu", "Ya"}};
	int Remaining = OutSize - 1;
	while(*pIn && Remaining > 1)
	{
		const unsigned char C = (unsigned char)*pIn;
		if(C < 0x80)
		{
			*pOut++ = (char)C;
			--Remaining;
			pIn++;
			continue;
		}
		// UTF-8 Cyrillic: D0 90-D1 8F
		int Code = -1;
		int Len = 0;
		if(C == 0xD0 && (unsigned char)pIn[1] >= 0x80)
		{
			Code = ((C & 0x1F) << 6) | ((unsigned char)pIn[1] & 0x3F);
			Len = 2;
		}
		else if(C == 0xD1)
		{
			Code = ((C & 0x1F) << 6) | ((unsigned char)pIn[1] & 0x3F);
			Len = 2;
		}
		if(Code >= 0x410 && Code <= 0x44F)
		{
			const int Idx = (Code <= 0x42F ? Code - 0x410 : Code - 0x430);
			const int Upper = (Code <= 0x42F) ? 1 : 0;
			AppendLatin(pOut, Remaining, s_apMap[Upper][Idx]);
			const char *pSoft = (pIn[Len] == 0xD1 && (unsigned char)pIn[Len + 1] == 0x8C) ? "\x2" : "";
			(void)pSoft;
			pIn += Len;
		}
		else
		{
			*pOut++ = (char)C;
			--Remaining;
			pIn++;
		}
	}
	*pOut = '\0';
}

// ---------- chat commands ----------
void CInvalidTools::ProcessOutgoing(char *pLine, int Size)
{
	// Only handle client-side commands that start with '!'.
	if(pLine[0] == '!')
	{
		char aCmd[32] = {0};
		char aName[24] = {0};
		const char *pSpace = std::strchr(pLine, ' ');
		const int CmdLen = pSpace ? (int)(pSpace - pLine) : str_length(pLine);
		str_copy(aCmd, pLine, std::min(CmdLen + 1, (int)sizeof(aCmd)));
		if(pSpace)
		{
			const char *pName = str_utf8_skip_whitespaces(pSpace + 1);
			str_copy(aName, pName, sizeof(aName));
		}
		if(str_comp_nocase(aCmd, "!team") == 0)
		{
			str_copy(g_Config.m_ClInvalidTeamName, aName, sizeof(g_Config.m_ClInvalidTeamName));
			pLine[0] = '\0';
			return;
		}
		if(str_comp_nocase(aCmd, "!target") == 0)
		{
			str_copy(g_Config.m_ClInvalidTargetName, aName, sizeof(g_Config.m_ClInvalidTargetName));
			pLine[0] = '\0';
			return;
		}
		if(str_comp_nocase(aCmd, "!clear") == 0)
		{
			g_Config.m_ClInvalidTeamName[0] = '\0';
			g_Config.m_ClInvalidTargetName[0] = '\0';
			pLine[0] = '\0';
			return;
		}
	}
	if(g_Config.m_ClInvalidTransliterate)
	{
		char aBuf[512];
		Transliterate(pLine, aBuf, sizeof(aBuf));
		str_copy(pLine, aBuf, Size);
	}
}

void CInvalidTools::ProcessIncoming(int ClientId, const char *pLine)
{
	if(!g_Config.m_ClInvalidAutoReply)
		return;
	if(ClientId < 0 || ClientId == GameClient()->m_Snap.m_LocalClientId)
		return;
	const char *pMyName = GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_aName;
	if(!pMyName[0] || !str_find_nocase(pLine, pMyName))
		return;
	if(!g_Config.m_ClInvalidAutoReplyAway && Graphics()->WindowActive() == 0)
		return;
	GameClient()->m_Chat.SendChat(0, g_Config.m_ClInvalidAutoReplyText);
}

// ---------- rendering ----------
void CInvalidTools::RenderMarkers()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const CGameClient::CSnapState &Snap = GameClient()->m_Snap;
	if(!Snap.m_pGameInfoObj)
		return;
	if(!g_Config.m_ClInvalidTeamMarker || !g_Config.m_ClInvalidTeamName[0])
		return;

	const vec2 LocalPos = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!Snap.m_aCharacters[i].m_Active || i == Snap.m_LocalClientId)
			continue;
		if(str_comp_nocase(GameClient()->m_aClients[i].m_aName, g_Config.m_ClInvalidTeamName) != 0)
			continue;
		const vec2 TargetPos = GameClient()->m_aClients[i].m_RenderPos;
		const vec2 Delta = TargetPos - LocalPos;
		const float DistBlocks = length(Delta) / 32.0f;
		const float Angle = std::atan2(Delta.y, Delta.x);
		const float Radius = 56.0f;
		const vec2 Dir = vec2(std::cos(Angle), std::sin(Angle));
		const vec2 Arrow = LocalPos + Dir * Radius;

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0.15f, 1.0f, 0.30f, 0.9f);
		Graphics()->QuadsSetRotation(Angle);
		IGraphics::CQuadItem Tip(Arrow.x + Dir.x * 6.0f, Arrow.y + Dir.y * 6.0f, 16.0f, 10.0f);
		Graphics()->QuadsDraw(&Tip, 1);
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsEnd();

		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "%.0f б", DistBlocks);
		TextRender()->TextColor(0.20f, 1.0f, 0.35f, 1.0f);
		TextRender()->Text(TargetPos.x, TargetPos.y - 54.0f, 14.0f, aBuf, -1.0f);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		break;
	}
}

void CInvalidTools::RenderSpectatorWarning()
{
	if(!g_Config.m_ClInvalidSpectatorWarning)
		return;
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const CGameClient::CSnapState &Snap = GameClient()->m_Snap;
	if(!Snap.m_pGameInfoObj)
		return;

	int aSpecs[MAX_CLIENTS];
	int Num = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(Snap.m_apInfoByName[i] == nullptr || Snap.m_apInfoByName[i]->m_Team != TEAM_SPEC)
			continue;
		if(Snap.m_apInfoByName[i]->m_Local)
			continue;
		aSpecs[Num++] = i;
	}
	if(Num == 0)
		return;

	const float Width = 300.0f * Graphics()->ScreenAspect();
	const float Height = 300.0f;
	const CScreenRect Screen = Graphics()->GetScreen();
	Graphics()->MapScreenToSize(Width, Height);
	const float BoxW = 128.0f;
	const float BoxH = 8.0f + Num * 9.0f;
	Graphics()->DrawRect(2.0f, 34.0f, BoxW, BoxH, ColorRGBA(0.0f, 0.0f, 0.0f, 0.55f), IGraphics::CORNER_ALL, 3.0f);
	TextRender()->TextColor(0.45f, 0.85f, 1.0f, 1.0f);
	TextRender()->Text(5.0f, 36.0f, 7.0f, "Наблюдают:", -1.0f);
	for(int j = 0; j < Num; j++)
		TextRender()->Text(5.0f, 46.0f + j * 9.0f, 6.5f, GameClient()->m_aClients[aSpecs[j]].m_aName, -1.0f);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->MapScreen(Screen);
}

void CInvalidTools::RenderSplash()
{
	if(!g_Config.m_ClInvalidSplash || Client()->State() != IClient::STATE_OFFLINE)
		return;
	const float Width = 300.0f * Graphics()->ScreenAspect();
	const float Height = 300.0f;
	const CScreenRect Screen = Graphics()->GetScreen();
	Graphics()->MapScreenToSize(Width, Height);
	const float T = (float)(time() / (double)time_freq());
	const float Pulse = 0.5f + 0.5f * std::sin(T * 2.0f);
	const float BoxW = 150.0f;
	const float BoxH = 30.0f;
	const float X = Width * 0.5f - BoxW * 0.5f;
	const float Y = 14.0f;
	Graphics()->DrawRect(X, Y, BoxW, BoxH, ColorRGBA(0.03f, 0.04f, 0.08f, 0.82f), IGraphics::CORNER_ALL, 8.0f);
	Graphics()->DrawRect(X + 2.0f, Y + 2.0f, BoxW - 4.0f, 1.5f, ColorRGBA(0.55f, 0.30f + Pulse * 0.5f, 1.0f, 0.9f), IGraphics::CORNER_ALL, 1.0f);
	TextRender()->TextColor(0.75f, 0.45f + Pulse * 0.35f, 1.0f, 1.0f);
	TextRender()->Text(X + BoxW * 0.5f - 44.0f, Y + 7.0f, 18.0f, "InvalidClient", -1.0f);
	TextRender()->TextColor(0.60f, 0.65f, 0.75f, 1.0f);
	TextRender()->Text(X + BoxW * 0.5f - 36.0f, Y + 21.0f, 7.0f, "made different", -1.0f);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->MapScreen(Screen);
}

void CInvalidTools::OnRender()
{
	RenderMarkers();
	RenderSpectatorWarning();
	RenderSplash();
}
