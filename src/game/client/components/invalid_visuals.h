/* InvalidClient: high quality procedural cosmetics and effects. */
#ifndef GAME_CLIENT_COMPONENTS_INVALID_VISUALS_H
#define GAME_CLIENT_COMPONENTS_INVALID_VISUALS_H

#include <engine/graphics.h>
#include <game/client/component.h>

#include <generated/protocol.h>

class CInvalidVisuals : public CComponent
{
public:
	CInvalidVisuals();
	int Sizeof() const override { return sizeof(*this); }

	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

	// Wings are rendered behind the tee, accessories in front of it.
	void RenderWings(vec2 BodyPos, vec2 Direction, float Alpha, float BaseSize, int ClientId);
	void RenderAccessories(vec2 BodyPos, vec2 Direction, float Alpha, float BaseSize, int ClientId, int Emote);

private:
	IGraphics::CTextureHandle m_aWingTextures[6];
	IGraphics::CTextureHandle m_aGlassesTextures[8];
	IGraphics::CTextureHandle m_aHatTextures[6];
	IGraphics::CTextureHandle m_SadTexture;
	IGraphics::CTextureHandle m_SparkTexture;
	IGraphics::CTextureHandle m_SmokeTexture;
	IGraphics::CTextureHandle m_TearTexture;
	bool m_bTexturesReady = false;

	void GenerateTextures();
	IGraphics::CTextureHandle MakeWingTexture(int Style);
	IGraphics::CTextureHandle MakeGlassesTexture(int Style);
	IGraphics::CTextureHandle MakeHatTexture(int Style);
	IGraphics::CTextureHandle MakeSoftCircle(const char *pName, float R, float G, float B, float Core);
	IGraphics::CTextureHandle MakeSadTexture();

	struct CParticle
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_MaxLife;
		float m_Size;
		int m_Style;
	};
	CParticle m_aParticles[256];
	int m_NextParticle = 0;
	float m_LastEmit = 0.0f;
	int m_aLastEmote[MAX_CLIENTS];

	void Emit(int Style, vec2 Pos, vec2 Vel, float Life, float Size);
	void ManageParticles(float Delta);
	void RenderFreezeWarning();

	static const int NUM_TRAIL_STYLES = 5;
};

#endif
