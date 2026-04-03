/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EP_WEATHER_H
#define EP_WEATHER_H

// Headers
#include "drawable.h"
#include "system.h"
#include "tone.h"
#include "rect.h"

/**
 * Renders the weather effects.
 */
class Weather : public Drawable {
public:
	Weather();

	void Draw(RenderTarget& dst) override;
	void Update();

	Tone GetTone() const;
	void SetTone(Tone tone);

	void OnWeatherChanged();

	static int GetMaxNumParticles(int weather_type);

private:
	void DrawRain(RenderTarget& dst);
	void DrawSnow(RenderTarget& dst);
	void DrawFog(RenderTarget& dst);
	void DrawSandstorm(RenderTarget& dst);
	void CreateRainParticle();
	void CreateSnowParticle();
	void CreateSandParticle();
	void CreateFogOverlay();

	void DrawParticles(RenderTarget& dst, const Bitmap& particle, Rect rect, int abase, int tmax);
	void DrawFogOverlay(RenderTarget& dst, const Bitmap& overlay);
	void DrawSandParticles(RenderTarget& dst, const Bitmap& particle);
	const Bitmap* ApplyToneEffect(const RenderTarget& rt, const Bitmap& bitmap, const Rect& rect);

	BitmapRef snow_bitmap;
	BitmapRef rain_bitmap;
	BitmapRef fog_bitmap;
	BitmapRef sand_bitmap;
	BitmapRef sand_particle_bitmap;

	BitmapRef tone_bitmap;

	BitmapRef weather_surface;

	Tone tone_effect;

	bool tone_dirty = true;
};

inline Tone Weather::GetTone() const {
	return tone_effect;
}

#endif
