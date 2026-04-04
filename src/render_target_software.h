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

#ifndef EP_SOFTWARE_RENDER_TARGET_H
#define EP_SOFTWARE_RENDER_TARGET_H

#include "render_target.h"
#include "bitmap.h"
#include "rect.h"
#include "color.h"

/**
 * Render Target for software rendering.
 *
 * This simply forwards everything to an underlying Bitmap which simulates the
 * screen.
 */
class SoftwareRenderTarget : public RenderTarget {
public:
	explicit SoftwareRenderTarget(Bitmap& screen_bitmap) : screen_bitmap(screen_bitmap) {
		target = &screen_bitmap;
	}

	/**
	 * Inherited from RenderTarget
	 */
	/** @{ */
	// Access to underlying bitmap: FIXME Remove this
	Bitmap* GetBitmap() override {
		return target;
	}

	bool BeginDrawTexture(Bitmap& target) override {
		this->target = &target;
		return true;
	}

	void EndDrawTexture() override {
		this->target = &screen_bitmap;
	};

	int GetWidth() const override {
		return target->GetWidth();
	}

	int GetHeight() const override {
		return target->GetHeight();
	}

	void Blit(int x, int y, Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		target->Blit(x, y, src, src_rect, opacity, blend_mode);
	}

	void TiledBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		target->TiledBlit(ox, oy, src_rect, src, dst_rect, opacity, blend_mode);
	}

	void StretchBlit(Rect const& dst_rect, Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		target->StretchBlit(dst_rect, src, src_rect, opacity, blend_mode);
	}

	void FlipBlit(int x, int y, Bitmap const& src, Rect const& src_rect, bool horizontal, bool vertical,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		target->FlipBlit(x, y, src, src_rect, horizontal, vertical, opacity, blend_mode);
	}

	void WaverBlit(int x, int y, double zoom_x, double zoom_y, Bitmap const& src, Rect const& src_rect, int depth, double phase,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		target->WaverBlit(x, y, zoom_x, zoom_y, src, src_rect, depth, phase, opacity, blend_mode);
	}

	void RotateZoomOpacityBlit(int x, int y, int ox, int oy,
			Bitmap const& src, Rect const& src_rect,
			double angle, double zoom_x, double zoom_y,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Normal) override {
		target->RotateZoomOpacityBlit(x, y, ox, oy, src, src_rect, angle, zoom_x, zoom_y, opacity, blend_mode);
	}

	void ZoomOpacityBlit(int x, int y, int ox, int oy,
			Bitmap const& src, Rect const& src_rect,
			double zoom_x, double zoom_y,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		target->ZoomOpacityBlit(x, y, ox, oy, src, src_rect, zoom_x, zoom_y, opacity, blend_mode);
	}

	void FillRect(Rect const& dst_rect, const Color &color) override {
		target->FillRect(dst_rect, color);
	}

	void ToneBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Tone &tone, Opacity const& opacity) override {
		target->ToneBlit(x, y, src, src_rect, tone, opacity);
	}

	void BlendBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Color &color, Opacity const& opacity) override {
		target->BlendBlit(x, y, src, src_rect, color, opacity);
	}

	void BlitFast(int x, int y, Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity) override {
		target->BlitFast(x, y, src, src_rect, opacity);
	}

	void Fill(const Color &color) override {
		target->Fill(color);
	}

	void Clear() override {
		target->Clear();
	}

	void ClearRect(Rect const& dst_rect) override {
		target->ClearRect(dst_rect);
	}
	/** @} */

private:
	Bitmap& screen_bitmap;
	Bitmap* target = nullptr;
};

#endif
