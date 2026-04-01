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
 * This simply forwards everything to an underlying Bitmap.
 */
class SoftwareRenderTarget : public RenderTarget {
public:
	explicit SoftwareRenderTarget(Bitmap& bitmap) : bitmap(bitmap) {}

	// Access to underlying bitmap: FIXME Remove this
	Bitmap* GetBitmap() override {
		return &bitmap;
	}

	/** @return the width of the render target */
	int GetWidth() const override {
		return bitmap.GetWidth();
	}

	/** @return the height of the render target */
	int GetHeight() const override {
		return bitmap.GetHeight();
	}

	/** @return render target bounds rect */
	Rect GetRect() const override {
		return bitmap.GetRect();
	}

	/**
	 * Blits source bitmap to this render target.
	 *
	 * @param x x position.
	 * @param y y position.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param opacity opacity for blending.
	 * @param blend_mode Blend mode to use.
	 */
	void Blit(int x, int y, Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		bitmap.Blit(x, y, src, src_rect, opacity, blend_mode);
	}

	/**
	 * Blits source bitmap in tiles to this render target.
	 *
	 * @param ox tile start x offset.
	 * @param oy tile start y offset.
	 * @param src_rect source bitmap rect.
	 * @param src source bitmap.
	 * @param dst_rect destination rect.
	 * @param opacity opacity for blending.
	 * @param blend_mode Blend mode to use.
	 */
	void TiledBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		bitmap.TiledBlit(ox, oy, src_rect, src, dst_rect, opacity, blend_mode);
	}

	/**
	 * Blits source bitmap to this render target, making clones across the edges if src crossed a boundary of this.
	 *
	 * @param x x position.
	 * @param y y position.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param mirror_x Blit a clone in x direction
	 * @param mirror_Y BLit a clone in y direction
	 * @param opacity opacity for blending.
	 */
	void EdgeMirrorBlit(int x, int y, Bitmap const& src, Rect const& src_rect,
			bool mirror_x, bool mirror_y, Opacity const& opacity) override {
		bitmap.EdgeMirrorBlit(x, y, src, src_rect, x, y, opacity);
	}

	/**
	 * Blits source bitmap stretched to this render target.
	 *
	 * @param dst_rect destination rect.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param opacity opacity for blending.
	 * @param blend_mode Blend mode to use.
	 */
	void StretchBlit(Rect const& dst_rect, Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		bitmap.StretchBlit(dst_rect, src, src_rect, opacity, blend_mode);
	}

	/**
	 * Blit source bitmap flipped.
	 *
	 * @param x x position.
	 * @param y y position.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param horizontal flip horizontally.
	 * @param vertical flip vertically.
	 * @param opacity opacity to apply.
	 * @param blend_mode Blend mode to use.
	 */
	void FlipBlit(int x, int y, Bitmap const& src, Rect const& src_rect, bool horizontal, bool vertical,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		bitmap.FlipBlit(x, y, src, src_rect, horizontal, vertical, opacity, blend_mode);
	}

	/**
	 * Blits source bitmap with waver, zoom, and opacity effects.
	 *
	 * @param x x position.
	 * @param y y position.
	 * @param zoom_x x scale factor.
	 * @param zoom_y y scale factor.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param depth wave magnitude.
	 * @param phase wave phase.
	 * @param opacity opacity.
	 * @param blend_mode Blend mode to use.
	 */
	void WaverBlit(int x, int y, double zoom_x, double zoom_y, Bitmap const& src, Rect const& src_rect, int depth, double phase,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		bitmap.WaverBlit(x, y, zoom_x, zoom_y, src, src_rect, depth, phase, opacity, blend_mode);
	}

	/**
	 * Blits source bitmap with rotation, zoom, and opacity effects.
	 *
	 * @param x x position.
	 * @param y y position.
	 * @param ox source origin x.
	 * @param oy source origin y.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param angle rotation angle in radians.
	 * @param zoom_x x scale factor.
	 * @param zoom_y y scale factor.
	 * @param opacity opacity.
	 * @param blend_mode Blend mode to use.
	 */
	void RotateZoomOpacityBlit(int x, int y, int ox, int oy,
			Bitmap const& src, Rect const& src_rect,
			double angle, double zoom_x, double zoom_y,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Normal) override {
		bitmap.RotateZoomOpacityBlit(x, y, ox, oy, src, src_rect, angle, zoom_x, zoom_y, opacity, blend_mode);
	}

	/**
	 * Blits source bitmap with zoom and opacity scaling.
	 *
	 * @param x x position.
	 * @param y y position.
	 * @param ox source origin x.
	 * @param oy source origin y.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rectangle.
	 * @param zoom_x x scale factor.
	 * @param zoom_y y scale factor.
	 * @param opacity opacity.
	 * @param blend_mode Blend mode to use.
	 */
	void ZoomOpacityBlit(int x, int y, int ox, int oy,
			Bitmap const& src, Rect const& src_rect,
			double zoom_x, double zoom_y,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		bitmap.ZoomOpacityBlit(x, y, ox, oy, src, src_rect, zoom_x, zoom_y, opacity, blend_mode);
	}

	/**
	 * Fills rect with color.
	 *
	 * @param dst_rect destination rect.
	 * @param color color for filling.
	 */
	void FillRect(Rect const& dst_rect, const Color &color) override {
		bitmap.FillRect(dst_rect, color);
	}

	/**
	 * Adjusts tone.
	 *
	 * @param x x position.
	 * @param y y position.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param tone tone to apply.
	 * @param opacity opacity to apply.
	 */
	void ToneBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Tone &tone, Opacity const& opacity) override {
		bitmap.ToneBlit(x, y, src, src_rect, tone, opacity);
	}

	/**
	 * Blends with color.
	 *
	 * @param x x position.
	 * @param y y position.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param color color to apply.
	 * @param opacity opacity to apply.
	 */
	void BlendBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Color &color, Opacity const& opacity) override {
		bitmap.BlendBlit(x, y, src, src_rect, color, opacity);
	}

	/**
	 * Blits source bitmap to this render target ignoring alpha.
	 *
	 * @param x x position.
	 * @param y y position.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param opacity opacity for blending.
	 */
	void BlitFast(int x, int y, Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity) override {
		bitmap.BlitFast(x, y, src, src_rect, opacity);
	}

	/**
	 * Blits source bitmap in tiles to this render target.
	 *
	 * @param src_rect source bitmap rect.
	 * @param src source bitmap.
	 * @param dst_rect destination rect.
	 * @param opacity opacity for blending.
	 * @param blend_mode Blend mode to use.
	 */
	void TiledBlit(Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		bitmap.TiledBlit(src_rect, src, dst_rect, opacity, blend_mode);
	}

	/**
	 * Blits source bitmap stretched to this render target.
	 *
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param opacity opacity for blending.
	 * @param blend_mode Blend mode to use.
	 */
	void StretchBlit(Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
		bitmap.StretchBlit(src, src_rect, opacity, blend_mode);
	}

	/**
	 * Fills entire bitmap with color.
	 *
	 * @param color color for filling.
	 */
	void Fill(const Color &color) override {
		bitmap.Fill(color);
	}

	/**
	 * Clears the bitmap with transparent pixels.
	 */
	void Clear() override {
		bitmap.Clear();
	}

	/**
	 * Clears the bitmap rect with transparent pixels.
	 *
	 * @param dst_rect destination rect.
	 */
	void ClearRect(Rect const& dst_rect) override {
		bitmap.ClearRect(dst_rect);
	}

private:
	Bitmap& bitmap;
};

#endif
