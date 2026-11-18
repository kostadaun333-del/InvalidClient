/* InvalidClient: client-side weather overlay. */
#ifndef GAME_CLIENT_COMPONENTS_INVALID_WEATHER_H
#define GAME_CLIENT_COMPONENTS_INVALID_WEATHER_H

#include <game/client/component.h>

class CInvalidWeather : public CComponent
{
public:
	CInvalidWeather();

	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
