/* InvalidClient: target markers, spectator warning and helper tools. */
#ifndef GAME_CLIENT_COMPONENTS_INVALID_TOOLS_H
#define GAME_CLIENT_COMPONENTS_INVALID_TOOLS_H

#include <game/client/component.h>

class CInvalidTools : public CComponent
{
public:
	CInvalidTools();
	int Sizeof() const override { return sizeof(*this); }

	void OnRender() override;

	// Called from CChat for outgoing / incoming chat processing.
	static void ProcessOutgoing(char *pLine, int Size);
	static void ProcessIncoming(int ClientId, const char *pLine);

	// Transliteration Cyrillic -> Latin.
	static void Transliterate(const char *pIn, char *pOut, int OutSize);

private:
	void RenderMarkers();
	void RenderSpectatorWarning();
	void RenderSplash();
};

#endif
