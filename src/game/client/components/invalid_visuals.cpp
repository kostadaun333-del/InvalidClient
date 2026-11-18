/* InvalidClient: high quality procedural cosmetics and effects.
 * All images are generated in memory - no external PNG assets are needed.
 */
#include "invalid_visuals.h"

#include <base/color.h>
#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/client/enums.h>
#include <engine/graphics.h>
#include <engine/image.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>
#include <iterator>

static float Clamp01(float V) { return std::clamp(V, 0.0f, 1.0f); }

static float Smooth(float A, float B, float X)
{
	const float T = Clamp01((X - A) / (B - A));
	return T * T * (3.0f - 2.0f * T);
}

static vec3 Mix(const vec3 &A, const vec3 &B, float T) { return A + (B - A) * T; }

static float Hash2(int X, int Y)
{
	unsigned int H = (unsigned int)(X * 374761393 + Y * 668265263);
	H = (H ^ (H >> 13)) * 1274126177u;
	return (float)((H ^ (H >> 16)) & 0xffff) / 65535.0f;
}

static float SdfCapsule(vec2 P, vec2 A, vec2 B, float R)
{
	const vec2 PA = P - A;
	const vec2 BA = B - A;
	const float D = dot(BA, BA);
	const float H = D > 0.0f ? Clamp01(dot(PA, BA) / D) : 0.0f;
	return length(PA - BA * H) - R;
}

static float SdfEllipse(vec2 P, vec2 C, vec2 R)
{
	return (length((P - C) / R) - 1.0f) * min(R.x, R.y);
}

static CImageInfo NewImage(int W, int H)
{
	CImageInfo Image;
	Image.m_Width = W;
	Image.m_Height = H;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.AllocateFillZero();
	return Image;
}

static void PutPixel(CImageInfo &Image, int X, int Y, const vec3 &Color, float Alpha)
{
	if(X < 0 || Y < 0 || X >= (int)Image.m_Width || Y >= (int)Image.m_Height)
		return;
	uint8_t *pPixel = Image.m_pData + (Y * (int)Image.m_Width + X) * 4;
	pPixel[0] = (uint8_t)std::clamp(Color.r * 255.0f, 0.0f, 255.0f);
	pPixel[1] = (uint8_t)std::clamp(Color.g * 255.0f, 0.0f, 255.0f);
	pPixel[2] = (uint8_t)std::clamp(Color.b * 255.0f, 0.0f, 255.0f);
	pPixel[3] = (uint8_t)std::clamp(Alpha * 255.0f, 0.0f, 255.0f);
}

CInvalidVisuals::CInvalidVisuals()
{
	OnReset();
}

void CInvalidVisuals::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aLastEmote[i] = -1;
}

void CInvalidVisuals::OnInit()
{
	GenerateTextures();
}

IGraphics::CTextureHandle CInvalidVisuals::MakeWingTexture(int Style)
{
	const int Size = 144;
	CImageInfo Image = NewImage(Size, Size);

	struct SWingStyle
	{
		int m_Count;
		float m_A0, m_A1;
		float m_Len0, m_Len1;
		float m_Width;
		vec3 m_Base, m_Tip, m_Glow;
	};
	static const SWingStyle s_aStyles[6] = {
		{13, 0.10f, 1.42f, 0.50f, 0.76f, 0.040f, vec3(0.72f, 0.78f, 0.98f), vec3(1.00f, 0.96f, 0.78f), vec3(1.00f, 0.88f, 0.55f)},
		{6, 0.04f, 1.50f, 0.44f, 0.72f, 0.080f, vec3(0.16f, 0.02f, 0.05f), vec3(0.88f, 0.10f, 0.14f), vec3(1.00f, 0.20f, 0.12f)},
		{9, 0.12f, 1.38f, 0.46f, 0.74f, 0.052f, vec3(0.08f, 0.38f, 0.72f), vec3(0.80f, 0.96f, 1.00f), vec3(0.40f, 0.90f, 1.00f)},
		{11, 0.08f, 1.48f, 0.42f, 0.70f, 0.044f, vec3(0.86f, 0.12f, 0.02f), vec3(1.00f, 0.88f, 0.16f), vec3(1.00f, 0.48f, 0.05f)},
		{12, 0.10f, 1.44f, 0.48f, 0.75f, 0.042f, vec3(0.12f, 0.04f, 0.36f), vec3(0.58f, 0.38f, 0.98f), vec3(0.82f, 0.40f, 1.00f)},
		{10, 0.14f, 1.34f, 0.44f, 0.70f, 0.056f, vec3(0.04f, 0.32f, 0.10f), vec3(0.62f, 0.96f, 0.34f), vec3(0.42f, 1.00f, 0.48f)},
	};
	const SWingStyle &Style = s_aStyles[std::clamp(Style, 0, 5)];
	const vec2 Root(0.14f, 0.16f);

	for(int Y = 0; Y < Size; Y++)
	{
		for(int X = 0; X < Size; X++)
		{
			const vec2 P((float)(X + 0.5) / Size, 1.0f - (float)(Y + 0.5) / Size);
			float BestDist = 1000.0f;
			float BestT = 0.0f;
			for(int i = 0; i < Style.m_Count; i++)
			{
				const float T = (float)i / (float)(Style.m_Count - 1);
				const float A = Style.m_A0 + (Style.m_A1 - Style.m_A0) * T;
				const float Len = Mix(Style.m_Len0, Style.m_Len1, std::sin(pi * T));
				const vec2 End = Root + vec2(std::cos(A), std::sin(A)) * Len;
				const float Radius = Style.m_Width * (1.0f - 0.45f * T);
				const float D = SdfCapsule(P, Root, End, Radius);
				if(D < BestDist)
				{
					BestDist = D;
					BestT = T;
				}
			}
			const float Body = 1.0f - Smooth(-0.007f, 0.006f, BestDist);
			const float Halo = (1.0f - Smooth(0.0f, 0.055f, BestDist)) * (1.0f - Body);
			vec3 Color = Mix(Style.m_Base, Style.m_Tip, Clamp01(BestT * 0.55f + P.y * 0.55f));
			Color += Style.m_Glow * Halo * 0.55f;
			const float Sheen = 1.0f - Smooth(0.0f, Style.m_Width * 0.55f, -BestDist);
			Color += vec3(1.0f, 1.0f, 1.0f) * Sheen * 0.16f;
			if(Style == 4 && Hash2(X / 3, Y / 3) > 0.985f)
				Color += vec3(1.0f, 1.0f, 1.0f) * 0.8f;
			PutPixel(Image, X, Y, Color, Clamp01(Body + Halo * 0.42f));
		}
	}
	return Graphics()->LoadTextureRawMove(Image, 0, "invalid_wing");
}

IGraphics::CTextureHandle CInvalidVisuals::MakeGlassesTexture(int Style)
{
	const int W = 160;
	const int H = 80;
	CImageInfo Image = NewImage(W, H);

	const vec3 Lenses[8] = {
		vec3(0.04f, 0.05f, 0.09f), vec3(0.02f, 0.02f, 0.03f), vec3(0.00f, 0.55f, 0.70f),
		vec3(0.95f, 0.35f, 0.62f), vec3(0.01f, 0.01f, 0.01f), vec3(0.85f, 0.06f, 0.06f),
		vec3(0.10f, 0.22f, 0.18f), vec3(0.55f, 0.25f, 0.95f)};
	const vec3 Frames[8] = {
		vec3(0.95f, 0.78f, 0.25f), vec3(0.12f, 0.12f, 0.15f), vec3(0.10f, 1.00f, 0.85f),
		vec3(1.00f, 0.55f, 0.75f), vec3(0.20f, 0.20f, 0.22f), vec3(0.20f, 0.55f, 1.00f),
		vec3(0.75f, 0.55f, 0.20f), vec3(1.00f, 0.95f, 0.45f)};
	const int Idx = std::clamp(Style, 0, 7);

	for(int Y = 0; Y < H; Y++)
	{
		for(int X = 0; X < W; X++)
		{
			const vec2 P((float)(X + 0.5) / W, 1.0f - (float)(Y + 0.5) / H);
			float LensMask = 0.0f;
			float FrameMask = 0.0f;
			vec3 Color = vec3(0.0f, 0.0f, 0.0f);

			if(Idx == 2) // visor
			{
				const float D = SdfEllipse(P, vec2(0.5f, 0.5f), vec2(0.47f, 0.13f));
				FrameMask = 1.0f - Smooth(-0.012f, 0.004f, D);
				LensMask = 1.0f - Smooth(-0.05f, -0.035f, D);
				Color = vec3(0.02f, 0.35f, 0.40f) + vec3(0.0f, 0.8f, 0.7f) * Hash2(X / 5, Y);
			}
			else if(Idx == 4) // ninja
			{
				const float Band = 1.0f - Smooth(0.0f, 0.02f, std::abs(P.y - 0.5f) - 0.20f);
				const float Slit = 1.0f - Smooth(0.0f, 0.015f, std::abs(P.y - 0.48f) - 0.075f);
				FrameMask = Band;
				Color = vec3(0.03f, 0.03f, 0.03f) * Band;
				if(Slit > 0.0f && P.x > 0.12f && P.x < 0.88f)
				{
					Color = vec3(0.95f, 0.78f, 0.55f);
					LensMask = Slit;
				}
			}
			else
			{
				for(int Side = 0; Side < 2; Side++)
				{
					const vec2 C(0.5f + (Side == 0 ? -0.235f : 0.235f), 0.5f);
					vec2 R(0.185f, 0.30f);
					if(Idx == 1)
						R = vec2(0.205f, 0.245f);
					else if(Idx == 3)
						R = vec2(0.175f, 0.175f);
					else if(Idx == 6)
						R = vec2(0.150f, 0.150f);
					const float D = SdfEllipse(P, C, R);
					const float Outer = 1.0f - Smooth(-0.018f, 0.006f, D);
					const float Inner = 1.0f - Smooth(-0.045f, -0.028f, D);
					FrameMask = std::max(FrameMask, Outer);
					LensMask = std::max(LensMask, Inner);
					Color = Mix(Color, Lenses[Idx], Inner);
					if(Outer - Inner > 0.0f)
						Color = Frames[Idx];
				}
				const float Bridge = 1.0f - Smooth(0.0f, 0.012f, std::abs(P.y - 0.52f) - 0.022f);
				if(P.x > 0.40f && P.x < 0.60f)
				{
					FrameMask = std::max(FrameMask, Bridge);
					if(Bridge > 0.0f)
						Color = Frames[Idx];
				}
			}
			const float Alpha = std::max(FrameMask, LensMask * 0.92f);
			vec3 Final = Color;
			if(LensMask > FrameMask)
				Final += vec3(0.35f, 0.40f, 0.55f) * (1.0f - Clamp01((float)Y / H)) * 0.35f;
			PutPixel(Image, X, Y, Final, Clamp01(Alpha));
		}
	}
	return Graphics()->LoadTextureRawMove(Image, 0, "invalid_glasses");
}

IGraphics::CTextureHandle CInvalidVisuals::MakeHatTexture(int Style)
{
	const int W = 128;
	const int H = 96;
	CImageInfo Image = NewImage(W, H);
	const int Idx = std::clamp(Style, 0, 5);

	for(int Y = 0; Y < H; Y++)
	{
		for(int X = 0; X < W; X++)
		{
			const vec2 P((float)(X + 0.5) / W, 1.0f - (float)(Y + 0.5) / H);
			float Mask = 0.0f;
			vec3 Color = vec3(0.0f, 0.0f, 0.0f);
			switch(Idx)
			{
			case 0: // crown
			{
				const float Body = (1.0f - Smooth(0.0f, 0.008f, std::abs(P.x - 0.5f) - 0.24f)) * (1.0f - Smooth(0.0f, 0.008f, std::abs(P.y - 0.30f) - 0.14f));
				float Spikes = 0.0f;
				for(int i = -2; i <= 2; i++)
				{
					const vec2 C(0.5f + i * 0.105f, 0.47f);
					Spikes = std::max(Spikes, 1.0f - Smooth(0.0f, 0.008f, length((P - C) / vec2(0.045f, 0.24f)) - 1.0f));
				}
				Mask = Body;
				Color = Mix(vec3(0.85f, 0.60f, 0.08f), vec3(1.0f, 0.92f, 0.35f), P.y);
				if(Spikes > 0.0f && P.y > 0.40f)
				{
					Mask = Spikes;
					Color = vec3(1.0f, 0.88f, 0.20f);
				}
				if(Body > 0.0f && P.x > 0.28f && P.x < 0.72f && P.y > 0.25f && P.y < 0.34f)
				{
					const float Gem = 1.0f - Smooth(0.0f, 0.008f, length((P - vec2(0.5f, 0.295f)) / vec2(0.045f, 0.045f)) - 1.0f);
					if(Gem > 0.0f) { Mask = Gem; Color = vec3(0.20f, 0.85f, 1.0f); }
				}
				break;
			}
			case 1: // wizard
			{
				const float Cone = 1.0f - Smooth(0.0f, 0.010f, std::abs(P.x - 0.5f) - (0.05f + (0.40f - P.y) * 0.42f));
				const float InCone = Cone * (1.0f - Smooth(0.0f, 0.01f, 0.22f - P.y)) * Smooth(0.0f, 0.02f, P.y - 0.62f);
				const float Brim = (1.0f - Smooth(0.0f, 0.008f, std::abs(P.y - 0.22f) - 0.035f)) * (1.0f - Smooth(0.0f, 0.01f, std::abs(P.x - 0.5f) - 0.44f));
				Mask = std::max(InCone, Brim);
				Color = Mix(vec3(0.10f, 0.08f, 0.35f), vec3(0.42f, 0.30f, 0.85f), P.y);
				if(Hash2(X / 6, Y / 6) > 0.90f)
					Color += vec3(0.9f, 0.85f, 0.4f) * (float)(Hash2(X, Y) > 0.5f);
				break;
			}
			case 2: // cap
			{
				const float Dome = (1.0f - Smooth(0.0f, 0.010f, length((P - vec2(0.5f, 0.34f)) / vec2(0.30f, 0.22f)) - 1.0f)) * Smooth(0.0f, 0.01f, P.y - 0.34f);
				const float Visor = (1.0f - Smooth(0.0f, 0.008f, length((P - vec2(0.5f, 0.30f)) / vec2(0.42f, 0.09f)) - 1.0f)) * (1.0f - Smooth(0.0f, 0.01f, P.y - 0.32f));
				Mask = std::max(Dome, Visor);
				Color = Mix(vec3(0.15f, 0.45f, 0.85f), vec3(0.35f, 0.75f, 1.0f), P.y);
				break;
			}
			case 3: // halo
			{
				const float D = length((P - vec2(0.5f, 0.55f)) / vec2(0.34f, 0.075f));
				Mask = (1.0f - Smooth(0.0f, 0.06f, std::abs(D - 1.0f)));
				Color = vec3(1.0f, 0.92f, 0.45f) + vec3(0.4f, 0.3f, 0.0f) * (1.0f - std::abs(D - 1.0f) * 4.0f);
				break;
			}
			case 4: // cat ears
			{
				for(int Side = -1; Side <= 1; Side += 2)
				{
					const vec2 C(0.5f + Side * 0.26f, 0.42f);
					const float Tri = 1.0f - Smooth(0.0f, 0.010f, std::abs(P.x - C.x) - (0.20f - (P.y - 0.22f) * 0.55f));
					const float Ear = Tri * (1.0f - Smooth(0.0f, 0.01f, 0.22f - P.y)) * Smooth(0.0f, 0.01f, P.y - 0.72f);
					if(Ear > Mask)
					{
						Mask = Ear;
						Color = Mix(vec3(0.85f, 0.45f, 0.65f), vec3(1.0f, 0.75f, 0.85f), P.y);
						const float Inner = 1.0f - Smooth(0.0f, 0.01f, std::abs(P.x - C.x) - (0.10f - (P.y - 0.30f) * 0.45f));
						if(Inner > 0.5f && P.y < 0.68f)
							Color = vec3(1.0f, 0.55f, 0.70f);
					}
				}
				break;
			}
			default: // top hat
			{
				const float Top = (1.0f - Smooth(0.0f, 0.008f, std::abs(P.x - 0.5f) - 0.21f)) * (1.0f - Smooth(0.0f, 0.008f, std::abs(P.y - 0.45f) - 0.24f));
				const float Brim = (1.0f - Smooth(0.0f, 0.008f, std::abs(P.y - 0.20f) - 0.030f)) * (1.0f - Smooth(0.0f, 0.01f, std::abs(P.x - 0.5f) - 0.43f));
				Mask = std::max(Top, Brim);
				Color = vec3(0.06f, 0.06f, 0.08f) + vec3(0.12f, 0.12f, 0.12f) * P.y;
				if(Top > 0.0f && P.y > 0.38f && P.y < 0.45f)
					Color = vec3(0.75f, 0.10f, 0.12f);
				break;
			}
			}
			PutPixel(Image, X, Y, Color, Clamp01(Mask));
		}
	}
	return Graphics()->LoadTextureRawMove(Image, 0, "invalid_hat");
}

IGraphics::CTextureHandle CInvalidVisuals::MakeSoftCircle(const char *pName, float R, float G, float B, float Core)
{
	const int S = 64;
	CImageInfo Image = NewImage(S, S);
	for(int Y = 0; Y < S; Y++)
	{
		for(int X = 0; X < S; X++)
		{
			const vec2 P = (vec2(X, Y) + 0.5f) / S * 2.0f - 1.0f;
			const float D = length(P);
			const float A = (1.0f - Smooth(Core, 1.0f, D)) * (1.0f - Smooth(0.96f, 1.0f, D));
			PutPixel(Image, X, Y, vec3(R, G, B), Clamp01(A));
		}
	}
	return Graphics()->LoadTextureRawMove(Image, 0, pName);
}

IGraphics::CTextureHandle CInvalidVisuals::MakeSadTexture()
{
	const int S = 64;
	CImageInfo Image = NewImage(S, S);
	for(int Y = 0; Y < S; Y++)
	{
		for(int X = 0; X < S; X++)
		{
			const vec2 P = (vec2(X, Y) + 0.5f) / S * 2.0f - 1.0f;
			const float D = length(P);
			float Mask = 0.0f;
			vec3 Color = vec3(0.0f, 0.0f, 0.0f);
			const float Body = 1.0f - Smooth(0.0f, 0.04f, D - 0.86f);
			if(Body > 0.0f)
			{
				Mask = Body;
				Color = vec3(1.0f, 0.92f, 0.25f);
				const float Eye = std::min(1.0f - Smooth(0.0f, 0.03f, length((P - vec2(-0.32f, 0.25f)) / 0.17f) - 1.0f),
					1.0f - Smooth(0.0f, 0.03f, length((P - vec2(0.32f, 0.25f)) / 0.17f) - 1.0f));
				const float Mouth = (1.0f - Smooth(0.0f, 0.04f, std::abs(length(P - vec2(0.0f, -1.15f)) - 0.85f) - 0.05f)) * Smooth(0.0f, 0.02f, -P.y - 0.20f);
				if(Eye > 0.0f || Mouth > 0.0f) { Mask = std::max(Eye, Mouth); Color = vec3(0.15f, 0.10f, 0.05f); }
			}
			PutPixel(Image, X, Y, Color, Clamp01(Mask));
		}
	}
	return Graphics()->LoadTextureRawMove(Image, 0, "invalid_sad");
}

void CInvalidVisuals::GenerateTextures()
{
	if(m_bTexturesReady)
		return;
	for(int i = 0; i < 6; i++)
		m_aWingTextures[i] = MakeWingTexture(i);
	for(int i = 0; i < 8; i++)
		m_aGlassesTextures[i] = MakeGlassesTexture(i);
	for(int i = 0; i < 6; i++)
		m_aHatTextures[i] = MakeHatTexture(i);
	m_SparkTexture = MakeSoftCircle("invalid_spark", 1.0f, 1.0f, 1.0f, 0.05f);
	m_SmokeTexture = MakeSoftCircle("invalid_smoke", 0.75f, 0.75f, 0.78f, 0.02f);
	m_TearTexture = MakeSoftCircle("invalid_tear", 0.45f, 0.80f, 1.0f, 0.20f);
	m_SadTexture = MakeSadTexture();
	m_bTexturesReady = true;
}

void CInvalidVisuals::RenderWings(vec2 BodyPos, vec2 Direction, float Alpha, float BaseSize, int ClientId)
{
	if(!m_bTexturesReady || !g_Config.m_ClInvalidWings)
		return;
	const int Style = std::clamp(g_Config.m_ClInvalidWingStyle, 0, 5);
	const float Time = (float)(time() / (double)time_freq());
	const float Speed = 3.0f + g_Config.m_ClInvalidWingFlap * 0.09f;
	const float Flap = std::sin(Time * Speed + ClientId * 0.9f);
	const float Facing = angle(Direction);

	Graphics()->TextureSet(m_aWingTextures[Style]);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	for(int Side = -1; Side <= 1; Side += 2)
	{
		Graphics()->QuadsSetRotation(Facing + Side * (0.18f + Flap * 0.20f));
		const vec2 Offset = vec2(Side * BaseSize * 0.22f, -BaseSize * 0.06f);
		const float Size = BaseSize * (2.15f + Flap * 0.12f);
		IGraphics::CQuadItem Item(BodyPos.x + Offset.x, BodyPos.y + Offset.y, Size, Size);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->QuadsDraw(&Item, 1);
	}
	Graphics()->QuadsEnd();

	if(g_Config.m_ClInvalidWingGlow > 0)
	{
		Graphics()->BlendAdditive();
		Graphics()->TextureSet(m_aWingTextures[Style]);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
		const float Glow = g_Config.m_ClInvalidWingGlow / 100.0f;
		for(int Side = -1; Side <= 1; Side += 2)
		{
			Graphics()->QuadsSetRotation(Facing + Side * (0.18f + Flap * 0.20f));
			const vec2 Offset = vec2(Side * BaseSize * 0.22f, -BaseSize * 0.06f);
			const float Size = BaseSize * 2.45f;
			IGraphics::CQuadItem Item(BodyPos.x + Offset.x, BodyPos.y + Offset.y, Size, Size);
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha * Glow * 0.14f);
			Graphics()->QuadsDraw(&Item, 1);
		}
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();
	}
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CInvalidVisuals::RenderAccessories(vec2 BodyPos, vec2 Direction, float Alpha, float BaseSize, int ClientId, int Emote)
{
	if(!m_bTexturesReady || Alpha <= 0.0f || BaseSize <= 0.0f)
		return;
	const float Time = (float)(time() / (double)time_freq());
	const bool Local = GameClient()->m_Snap.m_LocalClientId == ClientId;

	// glasses
	if(g_Config.m_ClInvalidGlasses)
	{
		const int Style = std::clamp(g_Config.m_ClInvalidGlassesStyle, 0, 7);
		const float W = BaseSize * (Style == 2 ? 0.72f : 0.60f);
		const float H = W * 0.5f;
		const vec2 Offset = vec2(Direction.x * 0.10f, -0.06f + Direction.y * 0.08f) * BaseSize;
		Graphics()->TextureSet(m_aGlassesTextures[Style]);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		IGraphics::CQuadItem Item(BodyPos.x + Offset.x, BodyPos.y + Offset.y, W, H);
		Graphics()->QuadsDraw(&Item, 1);
		Graphics()->QuadsEnd();
	}

	// hat
	if(g_Config.m_ClInvalidHats && ClientId >= 0)
	{
		const int Style = std::clamp(g_Config.m_ClInvalidHatStyle, 0, 5);
		const float W = BaseSize * 0.85f;
		const float H = W * 0.75f;
		const float Bob = std::sin(Time * 3.0f + ClientId) * BaseSize * 0.012f;
		Graphics()->TextureSet(m_aHatTextures[Style]);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		IGraphics::CQuadItem Item(BodyPos.x, BodyPos.y - BaseSize * 0.42f + Bob, W, H);
		Graphics()->QuadsDraw(&Item, 1);
		Graphics()->QuadsEnd();
	}

	// tears and sad smileys when a tee is hurt
	if(g_Config.m_ClInvalidTears && ClientId >= 0 && Emote == EMOTE_PAIN && m_aLastEmote[ClientId] != EMOTE_PAIN)
	{
		for(int i = 0; i < 4; i++)
			Emit(3, BodyPos + vec2((i % 2 == 0 ? -1 : 1) * BaseSize * 0.14f, -BaseSize * 0.05f),
				vec2((i % 2 == 0 ? -1 : 1) * 12.0f + (i - 1.5f) * 6.0f, -20.0f - i * 8.0f), 0.85f, BaseSize * 0.13f);
		Emit(4, BodyPos + vec2(0.0f, -BaseSize * 0.75f), vec2(0.0f, -28.0f), 1.4f, BaseSize * 0.55f);
	}
	if(ClientId >= 0)
		m_aLastEmote[ClientId] = Emote;

	// particle emitters (local tee keeps the effect tidy)
	if(Local && ClientId >= 0)
	{
		if(g_Config.m_ClInvalidCigarette)
		{
			Emit(1, BodyPos + normalize(Direction) * BaseSize * 0.20f + vec2(0.0f, -BaseSize * 0.10f),
				vec2((Hash2((int)(Time * 1000), ClientId) - 0.5f) * 18.0f, -34.0f), 1.9f, BaseSize * 0.30f);
		}
		if(g_Config.m_ClInvalidSpray)
		{
			Emit(2, BodyPos + normalize(Direction) * BaseSize * 0.28f + vec2(0.0f, BaseSize * 0.10f),
				normalize(Direction) * 150.0f + vec2(0.0f, 40.0f), 0.55f, BaseSize * 0.10f);
		}
	}

	if(g_Config.m_ClInvalidTrail > 0 && ClientId >= 0 && Time - m_LastEmit > 0.025f)
	{
		m_LastEmit = Time;
		const int Style = std::clamp(g_Config.m_ClInvalidTrailStyle, 0, NUM_TRAIL_STYLES - 1);
		Emit(0, BodyPos + vec2(0.0f, BaseSize * 0.25f), vec2(0.0f, 8.0f), 0.55f, BaseSize * 0.18f);
		(void)Style;
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CInvalidVisuals::Emit(int Style, vec2 Pos, vec2 Vel, float Life, float Size)
{
	CParticle &P = m_aParticles[m_NextParticle];
	m_NextParticle = (m_NextParticle + 1) % (int)std::size(m_aParticles);
	P.m_Pos = Pos;
	P.m_Vel = Vel;
	P.m_Life = Life;
	P.m_MaxLife = Life;
	P.m_Size = Size;
	P.m_Style = Style;
}

void CInvalidVisuals::ManageParticles(float Delta)
{
	for(CParticle &P : m_aParticles)
	{
		if(P.m_Life <= 0.0f)
			continue;
		P.m_Life -= Delta;
		P.m_Pos += P.m_Vel * Delta;
		if(P.m_Style == 1) // smoke rises and expands
		{
			P.m_Vel.y -= 6.0f * Delta;
			P.m_Vel.x += std::sin(P.m_Pos.x * 0.05f + P.m_Pos.y * 0.02f) * 10.0f * Delta;
			P.m_Size += 14.0f * Delta;
		}
		else
			P.m_Vel.y += 40.0f * Delta;
	}
}

void CInvalidVisuals::RenderFreezeWarning()
{
	if(!g_Config.m_ClInvalidFreezeGlow || Client()->State() != IClient::STATE_ONLINE)
		return;
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0)
		return;
	CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(LocalId);
	if(!pChar)
		return;

	const vec2 Pos = pChar->m_Core.m_Pos;
	const vec2 Vel = pChar->m_Core.m_Vel;
	const float Speed = length(Vel);
	const vec2 Dir = Speed > 1.0f ? Vel / Speed : vec2(0.0f, -1.0f);

	auto IsFreeze = [](int Tile) {
		return Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE;
	};

	float BestDist = -1.0f;
	vec2 BestPos(0.0f, 0.0f);

	// Look ahead along the predicted movement direction.
	for(int i = 0; i < 14; i++)
	{
		const float D = i * 16.0f;
		const vec2 P = Pos + Dir * D;
		if(IsFreeze(GameClient()->Collision()->GetTile(round_to_int(P.x), round_to_int(P.y))))
		{
			BestDist = D;
			BestPos = vec2(round_to_int(P.x / 32.0f) * 32.0f + 16.0f, round_to_int(P.y / 32.0f) * 32.0f + 16.0f);
			break;
		}
	}

	// Also catch very close freeze tiles in any direction.
	if(BestDist < 0.0f)
	{
		const int Tx = round_to_int(Pos.x / 32.0f);
		const int Ty = round_to_int(Pos.y / 32.0f);
		float NearDist = 1000000.0f;
		for(int Oy = -2; Oy <= 2; Oy++)
		{
			for(int Ox = -2; Ox <= 2; Ox++)
			{
				const vec2 TileCenter((Tx + Ox) * 32.0f + 16.0f, (Ty + Oy) * 32.0f + 16.0f);
				if(!IsFreeze(GameClient()->Collision()->GetTile(round_to_int(TileCenter.x), round_to_int(TileCenter.y))))
					continue;
				const float D = distance(TileCenter, Pos);
				if(D < 88.0f && D < NearDist)
				{
					NearDist = D;
					BestPos = TileCenter;
				}
			}
		}
		if(NearDist < 1000000.0f)
			BestDist = NearDist;
	}

	if(BestDist < 0.0f)
		return;

	const float T = (float)(time() / (double)time_freq());
	const float Pulse = 0.5f + 0.5f * std::sin(T * 8.0f);
	const float Proximity = Clamp01(1.0f - BestDist / 200.0f);
	const float Alpha = (0.18f + Pulse * 0.30f) * (0.35f + Proximity * 0.65f);

	Graphics()->BlendAdditive();
	Graphics()->TextureSet(m_SmokeTexture);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	Graphics()->SetColor(1.0f, 0.10f, 0.12f, Alpha);
	IGraphics::CQuadItem Warning(BestPos.x, BestPos.y, 96.0f + Pulse * 12.0f, 96.0f + Pulse * 12.0f);
	Graphics()->QuadsDraw(&Warning, 1);
	Graphics()->QuadsEnd();
	Graphics()->BlendNormal();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CInvalidVisuals::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	RenderFreezeWarning();
	ManageParticles(1.0f / 60.0f);

	const int Style = std::clamp(g_Config.m_ClInvalidTrailStyle, 0, NUM_TRAIL_STYLES - 1);
	Graphics()->BlendAdditive();
	for(const CParticle &P : m_aParticles)
	{
		if(P.m_Life <= 0.0f)
			continue;
		const float T = P.m_Life / P.m_MaxLife;
		IGraphics::CTextureHandle Texture;
		ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
		if(P.m_Style == 1)
		{
			Texture = m_SmokeTexture;
			Color = ColorRGBA(0.62f, 0.62f, 0.66f, T * 0.30f);
		}
		else if(P.m_Style == 2)
		{
			Texture = m_SmokeTexture;
			Color = ColorRGBA(0.35f, 0.70f, 1.0f, T * 0.75f);
		}
		else if(P.m_Style == 3)
		{
			Texture = m_TearTexture;
			Color = ColorRGBA(0.55f, 0.85f, 1.0f, T * 0.95f);
		}
		else if(P.m_Style == 4)
		{
			Texture = m_SadTexture;
			Color = ColorRGBA(1.0f, 1.0f, 1.0f, T * 0.95f);
		}
		else
		{
			Texture = m_SparkTexture;
			vec3 C(1.0f, 1.0f, 1.0f);
			switch(Style)
			{
			case 0: C = vec3(1.0f, 0.85f, 0.35f); break;
			case 1: C = vec3(0.45f, 0.90f, 1.0f); break;
			case 2: C = vec3(1.0f, 0.55f, 0.85f); break;
			case 3: C = vec3(1.0f, 0.35f, 0.10f); break;
			default: C = vec3(0.5f + 0.5f * std::sin(T * 12.0f), 0.5f + 0.5f * std::sin(T * 12.0f + 2.0f), 0.5f + 0.5f * std::sin(T * 12.0f + 4.0f)); break;
			}
			Color = ColorRGBA(C.r, C.g, C.b, T * (g_Config.m_ClInvalidTrail / 100.0f));
		}
		Graphics()->TextureSet(Texture);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
		Graphics()->SetColor(Color);
		const float Size = P.m_Size * (P.m_Style == 1 ? 1.0f : T);
		IGraphics::CQuadItem Item(P.m_Pos.x, P.m_Pos.y, Size, Size);
		Graphics()->QuadsDraw(&Item, 1);
		Graphics()->QuadsEnd();
	}
	Graphics()->BlendNormal();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}
