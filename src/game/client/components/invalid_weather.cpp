/* InvalidClient: purely client-side weather (snow, rain, sakura, leaves, ashes),
 * lightning flashes and atmospheric fog. No map or server changes are required.
 */
#include "invalid_weather.h"

#include <base/color.h>
#include <base/math.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/client/enums.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <algorithm>
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

	const CScreenRect Screen = Graphics()->GetScreen();
	Graphics()->MapScreenToSize(Width, Height);
	Graphics()->TextureClear();

	const float Time = (float)(time() / (double)time_freq());
	const float Intensity = g_Config.m_ClInvalidSnowIntensity / 100.0f;
	const int Type = std::clamp(g_Config.m_ClInvalidWeatherType, 0, 4);
	const float Wind = Intensity > 0.6f ? 160.0f : 40.0f;

	// fog
	if(g_Config.m_ClInvalidFog > 0)
	{
		const float F = g_Config.m_ClInvalidFog / 100.0f;
		Graphics()->DrawRect(0.0f, 0.0f, Width, Height, ColorRGBA(0.42f, 0.50f, 0.62f, F * 0.28f));
		Graphics()->DrawRect(0.0f, Height * 0.45f, Width, Height * 0.55f, ColorRGBA(0.50f, 0.56f, 0.66f, F * 0.18f));
	}

	// lightning
	if(g_Config.m_ClInvalidLightning && (Type == 1 || Type == 3))
	{
		const float Cycle = std::fmod(Time, 9.0f);
		if(Cycle < 0.12f)
			Graphics()->DrawRect(0.0f, 0.0f, Width, Height, ColorRGBA(0.85f, 0.92f, 1.0f, 0.55f));
		else if(Cycle > 0.12f && Cycle < 0.20f)
			Graphics()->DrawRect(0.0f, 0.0f, Width, Height, ColorRGBA(0.80f, 0.88f, 1.0f, 0.25f));
	}

	Graphics()->QuadsBegin();
	const int NumFlakes = 80 + (int)(220.0f * Intensity);
	for(int i = 0; i < NumFlakes; i++)
	{
		const float R0 = Random01(i * 3 + 1);
		const float R1 = Random01(i * 3 + 2);
		const float R2 = Random01(i * 3 + 3);
		float Speed = 30.0f + R2 * 90.0f;
		float Size = 1.5f + R2 * 2.5f;
		float Alpha = 0.85f * Intensity + 0.15f;
		ColorRGBA Color(1.0f, 1.0f, 1.0f, Alpha);
		bool Streak = false;

		switch(Type)
		{
		case 1: // rain
			Speed = 320.0f + R2 * 260.0f;
			Size = 1.0f + R2 * 1.2f;
			Color = ColorRGBA(0.55f, 0.75f, 1.0f, 0.55f * Intensity + 0.15f);
			Streak = true;
			break;
		case 2: // sakura
			Speed = 24.0f + R2 * 46.0f;
			Size = 2.2f + R2 * 2.6f;
			Color = ColorRGBA(1.0f, 0.62f + R2 * 0.25f, 0.78f + R2 * 0.2f, 0.85f * Intensity + 0.15f);
			break;
		case 3: // leaves
			Speed = 26.0f + R2 * 52.0f;
			Size = 2.6f + R2 * 3.0f;
			Color = ColorRGBA(0.55f + R2 * 0.35f, 0.35f + R2 * 0.35f, 0.10f, 0.85f * Intensity + 0.15f);
			break;
		case 4: // ashes
			Speed = 18.0f + R2 * 40.0f;
			Size = 1.0f + R2 * 1.8f;
			Color = ColorRGBA(0.45f + R2 * 0.2f, 0.45f, 0.50f, 0.65f * Intensity + 0.1f);
			break;
		default:
			break;
		}

		const float Drift = (Type == 2 || Type == 3) ? std::sin(Time * 1.5f + i) * 24.0f : 0.0f;
		const float X = std::fmod(R0 * Width + Wind * Time * (0.5f + R1) + Drift + Width * 2.0f, Width);
		const float Y = std::fmod(R1 * Height + Speed * Time, Height);
		Graphics()->SetColor(Color);
		IGraphics::CQuadItem Flake(X, Y, Size, Type == 1 ? Size * 2.4f : Size);
		Graphics()->QuadsDraw(&Flake, 1);

		if(Streak || (Wind > 100.0f && R2 > 0.7f))
		{
			Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a * 0.45f);
			IGraphics::CQuadItem Line(X, Y, Size * (Streak ? 2.0f : 5.0f), Streak ? 12.0f : 0.8f);
			Graphics()->QuadsDraw(&Line, 1);
		}
	}
	Graphics()->QuadsEnd();

	// Drifts only for snow and leaves.
	if(Type == 0 || Type == 3)
	{
		const float R = Type == 0 ? 0.94f : 0.55f;
		const float G = Type == 0 ? 0.97f : 0.42f;
		const float B = Type == 0 ? 1.0f : 0.14f;
		const int NumDrifts = 10;
		for(int i = 0; i < NumDrifts; i++)
		{
			const float Step = Width / NumDrifts;
			const float Wave = std::sin(Time * 0.7f + i * 0.8f) * 6.0f;
			const float DriftHeight = 14.0f + Random01(i + 17) * 26.0f + Wave;
			Graphics()->DrawRect(i * Step - 4.0f, Height - DriftHeight, Step + 8.0f, DriftHeight, ColorRGBA(R, G, B, 0.30f + Intensity * 0.30f), IGraphics::CORNER_ALL, DriftHeight * 0.5f);
		}
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	Graphics()->MapScreen(Screen);
}
