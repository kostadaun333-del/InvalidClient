/* InvalidClient: heuristic analysis of the spectated player.
 *
 * The percentage is an estimate based only on data visible to the client in
 * snapshots (aim changes, freeze timers, freeze flags). It is NOT evidence of
 * cheating and must never be used as the only reason for a ban.
 */
#include "invalid_analysis.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>

CInvalidAnalysis::CInvalidAnalysis()
{
	OnReset();
}

void CInvalidAnalysis::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aHistoryHead[i] = 0;
		m_aScores[i] = CScores();
		for(CObservation &Obs : m_aHistory[i])
			Obs = CObservation();
	}
	m_LastTarget = -1;
	m_LastTick = -1;
}

void CInvalidAnalysis::ResetClient(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	m_aHistoryHead[ClientId] = 0;
	m_aScores[ClientId] = CScores();
	for(CObservation &Obs : m_aHistory[ClientId])
		Obs = CObservation();
}

void CInvalidAnalysis::AddObservation(int ClientId, const CObservation &Obs)
{
	m_aHistory[ClientId][m_aHistoryHead[ClientId]] = Obs;
	m_aHistoryHead[ClientId] = (m_aHistoryHead[ClientId] + 1) % HISTORY_SIZE;
}

const CInvalidAnalysis::CObservation *CInvalidAnalysis::PreviousObservation(int ClientId) const
{
	if(m_aHistoryHead[ClientId] == 0)
		return nullptr;
	const CObservation &Obs = m_aHistory[ClientId][(m_aHistoryHead[ClientId] - 1 + HISTORY_SIZE) % HISTORY_SIZE];
	return Obs.m_Valid ? &Obs : nullptr;
}

float CInvalidAnalysis::GetSuspicion(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return 0.0f;
	const CScores &Scores = m_aScores[ClientId];
	if(Scores.m_Samples < 20)
		return 0.0f;
	return std::clamp(Scores.m_Aim * 0.45f + Scores.m_Freeze * 0.30f + Scores.m_Unfreeze * 0.25f, 0.0f, 100.0f);
}

void CInvalidAnalysis::OnNewSnapshot()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const CGameClient::CSnapState &Snap = GameClient()->m_Snap;
	if(!Snap.m_pGameInfoObj || GameClient()->m_SuppressEvents)
		return;

	const int ClientId = Snap.m_SpecInfo.m_SpectatorId;
	if(!Snap.m_SpecInfo.m_Active || GameClient()->m_MultiViewActivated ||
		ClientId < 0 || ClientId >= MAX_CLIENTS || ClientId == SPEC_FREEVIEW || ClientId == SPEC_FOLLOW)
	{
		return;
	}

	const CGameClient::CSnapState::CCharacterInfo &Character = Snap.m_aCharacters[ClientId];
	if(!Character.m_Active)
		return;

	const int Tick = Client()->GameTick(g_Config.m_ClDummy);
	if(ClientId != m_LastTarget || Tick <= m_LastTick || Tick - m_LastTick > 20)
	{
		ResetClient(ClientId);
		m_LastTarget = ClientId;
		m_LastTick = Tick;
		return;
	}
	m_LastTick = Tick;

	CObservation Obs;
	Obs.m_Tick = Tick;
	Obs.m_Valid = true;
	const CNetObj_Character &Cur = Character.m_Cur;
	Obs.m_Pos = vec2(Cur.m_X, Cur.m_Y) / 32.0f;
	Obs.m_Vel = vec2(Cur.m_VelX, Cur.m_VelY) / 256.0f;
	Obs.m_AttackTick = Cur.m_AttackTick;
	Obs.m_AimAngle = Cur.m_Angle / 256.0f;

	if(Character.m_HasExtendedData)
	{
		const CNetObj_DDNetCharacter &Extended = Character.m_ExtendedData;
		Obs.m_FreezeEnd = Extended.m_FreezeEnd;
		Obs.m_FreezeStart = Extended.m_FreezeStart;
		Obs.m_Flags = Extended.m_Flags;
		Obs.m_TeleportId = Extended.m_TeleCheckpoint;
		Obs.m_HasExtendedData = true;
		if(Character.m_HasExtendedDisplayInfo && (Extended.m_TargetX != 0 || Extended.m_TargetY != 0))
			Obs.m_AimAngle = std::atan2((float)Extended.m_TargetY, (float)Extended.m_TargetX);
	}

	CScores &Scores = m_aScores[ClientId];
	Scores.m_Aim *= 0.995f;
	Scores.m_Freeze *= 0.995f;
	Scores.m_Unfreeze *= 0.995f;

	const CObservation *pPrev = PreviousObservation(ClientId);
	if(pPrev && pPrev->m_Valid)
	{
		const int Delta = Tick - pPrev->m_Tick;
		if(Delta > 0 && Delta <= 20)
		{
			// 1. Aimbot signal: very fast angle changes between snapshots.
			const float Diff = Obs.m_AimAngle - pPrev->m_AimAngle;
			const float Wrapped = std::atan2(std::sin(Diff), std::cos(Diff));
			const float AngularSpeed = std::abs(Wrapped) / Delta;
			if(AngularSpeed > 0.8f)
				Scores.m_Aim = std::clamp(Scores.m_Aim + 10.0f * (AngularSpeed - 0.8f), 0.0f, 100.0f);

			// A shot fired while the aim vector is almost exactly on another tee.
			if(Obs.m_AttackTick != pPrev->m_AttackTick)
			{
				float BestAngle = pi;
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(i == ClientId || !Snap.m_aCharacters[i].m_Active)
						continue;
					const vec2 ToTarget = vec2(Snap.m_aCharacters[i].m_Cur.m_X, Snap.m_aCharacters[i].m_Cur.m_Y) / 32.0f - Obs.m_Pos;
					if(length(ToTarget) < 1.0f)
						continue;
					const float TargetAngle = std::atan2(ToTarget.y, ToTarget.x);
					const float TargetDiff = Obs.m_AimAngle - TargetAngle;
					BestAngle = std::min(BestAngle, std::abs(std::atan2(std::sin(TargetDiff), std::cos(TargetDiff))));
				}
				if(BestAngle < 0.06f)
					Scores.m_Aim = std::clamp(Scores.m_Aim + 6.0f, 0.0f, 100.0f);
			}

			// 2. Freeze signal: the freeze timer drops faster than the tick delta.
			if(pPrev->m_HasExtendedData && Obs.m_HasExtendedData)
			{
				const int PrevRemaining = pPrev->m_FreezeEnd - pPrev->m_Tick;
				const int CurRemaining = Obs.m_FreezeEnd - Tick;
				if(PrevRemaining > 0 && CurRemaining <= 0 && PrevRemaining > Delta + 4)
					Scores.m_Freeze = std::clamp(Scores.m_Freeze + 20.0f, 0.0f, 100.0f);
				if(pPrev->m_FreezeEnd > 0 && Obs.m_FreezeEnd > 0 && pPrev->m_FreezeEnd - Obs.m_FreezeEnd > Delta + 4)
					Scores.m_Freeze = std::clamp(Scores.m_Freeze + 10.0f, 0.0f, 100.0f);

				// 3. Auto-unfreeze signal: freezes ending suspiciously quickly.
				if(pPrev->m_FreezeEnd > 0 && Obs.m_FreezeEnd == 0 && pPrev->m_FreezeStart > 0)
				{
					const int Duration = pPrev->m_FreezeEnd - pPrev->m_FreezeStart;
					if(Duration > 0 && Duration < 25)
						Scores.m_Unfreeze = std::clamp(Scores.m_Unfreeze + 12.0f, 0.0f, 100.0f);
				}

				// Standing on a freeze tile without a freeze timer.
				if((Obs.m_Flags & CHARACTERFLAG_IN_FREEZE) && Obs.m_FreezeEnd <= 0)
					Scores.m_Freeze = std::clamp(Scores.m_Freeze + 4.0f, 0.0f, 100.0f);
			}
		}
	}

	Scores.m_Samples++;
	AddObservation(ClientId, Obs);
}

void CInvalidAnalysis::RenderOverlay()
{
	const CGameClient::CSnapState &Snap = GameClient()->m_Snap;
	const int ClientId = Snap.m_SpecInfo.m_SpectatorId;
	if(!Snap.m_SpecInfo.m_Active || GameClient()->m_MultiViewActivated ||
		ClientId < 0 || ClientId >= MAX_CLIENTS || ClientId == SPEC_FREEVIEW || ClientId == SPEC_FOLLOW)
	{
		return;
	}

	const CScores &Scores = m_aScores[ClientId];
	const float Suspicion = GetSuspicion(ClientId);
	const float Width = 300.0f * Graphics()->ScreenAspect();
	const float Height = 300.0f;
	Graphics()->MapScreenToSize(Width, Height);

	const float BoxWidth = 124.0f;
	const float BoxHeight = 29.0f;
	const float X = Width - BoxWidth - 2.0f;
	const float Y = 2.0f;
	Graphics()->DrawRect(X, Y, BoxWidth, BoxHeight, ColorRGBA(0.0f, 0.0f, 0.0f, 0.55f), IGraphics::CORNER_ALL, 3.0f);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Suspicion %.0f%%", Suspicion);
	const ColorRGBA Color = Suspicion < 30.0f ? ColorRGBA(0.40f, 1.0f, 0.40f, 1.0f) : (Suspicion < 60.0f ? ColorRGBA(1.0f, 0.85f, 0.20f, 1.0f) : ColorRGBA(1.0f, 0.30f, 0.30f, 1.0f));
	TextRender()->TextColor(Color);
	TextRender()->Text(X + 4.0f, Y + 1.0f, 8.0f, aBuf, -1.0f);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	str_format(aBuf, sizeof(aBuf), "Aim %.0f Frz %.0f Unf %.0f", Scores.m_Aim, Scores.m_Freeze, Scores.m_Unfreeze);
	TextRender()->Text(X + 4.0f, Y + 12.0f, 5.0f, aBuf, -1.0f);
	str_format(aBuf, sizeof(aBuf), "Samples %d", Scores.m_Samples);
	TextRender()->Text(X + 4.0f, Y + 20.0f, 5.0f, aBuf, -1.0f);
}

void CInvalidAnalysis::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!GameClient()->m_Snap.m_pGameInfoObj)
		return;
	if(!g_Config.m_ClShowhud || !g_Config.m_ClInvalidAnalysisOverlay)
		return;
	RenderOverlay();
}
