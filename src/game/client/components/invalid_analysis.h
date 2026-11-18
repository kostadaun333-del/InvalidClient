/* InvalidClient: spectator suspicion analysis (heuristic, not proof). */
#ifndef GAME_CLIENT_COMPONENTS_INVALID_ANALYSIS_H
#define GAME_CLIENT_COMPONENTS_INVALID_ANALYSIS_H

#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/client/component.h>

#include <array>

class CInvalidAnalysis : public CComponent
{
	static constexpr int HISTORY_SIZE = 128;

	struct CObservation
	{
		int m_Tick = -1;
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_Vel = vec2(0.0f, 0.0f);
		float m_AimAngle = 0.0f;
		int m_FreezeEnd = 0;
		int m_FreezeStart = 0;
		int m_Flags = 0;
		int m_AttackTick = 0;
		int m_TeleportId = 0;
		bool m_HasExtendedData = false;
		bool m_Valid = false;
	};

public:
	struct CScores
	{
		float m_Aim = 0.0f;
		float m_Freeze = 0.0f;
		float m_Unfreeze = 0.0f;
		int m_Samples = 0;
	};

private:
	std::array<CObservation, HISTORY_SIZE> m_aHistory[MAX_CLIENTS];
	int m_aHistoryHead[MAX_CLIENTS] = {};
	CScores m_aScores[MAX_CLIENTS];
	int m_LastTarget = -1;
	int m_LastTick = -1;

	void ResetClient(int ClientId);
	void AddObservation(int ClientId, const CObservation &Obs);
	const CObservation *PreviousObservation(int ClientId) const;
	void RenderOverlay();

public:
	CInvalidAnalysis();

	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnNewSnapshot() override;
	void OnRender() override;

	float GetSuspicion(int ClientId) const;
	CScores GetScores(int ClientId) const { return m_aScores[ClientId]; }
};

#endif
