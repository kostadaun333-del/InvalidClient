/* InvalidClient: purely client-side snow, snowdrifts and blizzard overlay.
 * No map or server changes are required; everything is drawn in screen space.
 */
#include "invalid_weather.h"

#include <base/math.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/client/enums.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <cmath>

static float Random01(int Seed)
{
	const float Value = std::sin((float)Seed * 12.9898f) * 43758.5453f;
	return Value - std::floor(Value);
}

CInvalidWeather::CInvalidWeather()
{
}

void CInvalidWeather::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!g_Config.m_ClInvalidWeather || g_Config.m_ClInvalidSnowIntensity <= 0)
		return;

	const float Width = (float)Graphics()->ScreenWidth();
	const float Height = (float)Graphics()->ScreenHeight();
	if(Width <= 0.0f || Height <= 0.0f)
		return;

	Graphics()->MapScreenToSize(Width, Height);
	Graphics()->TextureClear();

	const float Time = (float)(time() / (double)time_freq());
	const float Intensity = g_Config.m_ClInvalidSnowIntensity / 100.0f;
	const float Wind = Intensity > 0.6f ? 160.0f : 40.0f;

	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.85f * Intensity + 0.15f);
	const int NumFlakes = 80 + (int)(220.0f * Intensity);
	for(int i = 0; i < NumFlakes; i++)
	{
		const float R0 = Random01(i * 3 + 1);
		const float R1 = Random01(i * 3 + 2);
		const float R2 = Random01(i * 3 + 3);
		const float Speed = 30.0f + R2 * 90.0f;
		const float X = std::fmod(R0 * Width + Wind * Time * (0.5f + R1), Width);
		const float Y = std::fmod(R1 * Height + Speed * Time, Height);
		const float Size = 1.5f + R2 * 2.5f;
		IGraphics::CQuadItem Flake(X, Y, Size, Size);
		Graphics()->QuadsDraw(&Flake, 1);

		if(Wind > 100.0f && R2 > 0.7f)
		{
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.35f * Intensity);
			IGraphics::CQuadItem Streak(X, Y, Size * 5.0f, 0.8f);
			Graphics()->QuadsDraw(&Streak, 1);
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.85f * Intensity + 0.15f);
		}
	}
	Graphics()->QuadsEnd();

	// Snowdrifts along the bottom edge, gently waving over time.
	Graphics()->SetColor(0.94f, 0.97f, 1.0f, 0.35f + Intensity * 0.35f);
	const int NumDrifts = 10;
	for(int i = 0; i < NumDrifts; i++)
	{
		const float Step = Width / NumDrifts;
		const float Wave = std::sin(Time * 0.7f + i * 0.8f) * 6.0f;
		const float DriftHeight = 14.0f + Random01(i + 17) * 26.0f + Wave;
		Graphics()->DrawRect(i * Step - 4.0f, Height - DriftHeight, Step + 8.0f, DriftHeight, ColorRGBA(0.94f, 0.97f, 1.0f, 0.30f + Intensity * 0.30f), IGraphics::CORNER_ALL, DriftHeight * 0.5f);
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}
