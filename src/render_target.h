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

#ifndef EP_RENDER_TARGET_H
#define EP_RENDER_TARGET_H


#include "bitmap.h"
#include "opacity.h"

/**
 * Interface for a render target.
 * This can be either a hardware accelerated target or a software renderer.
 * Is used by the graphics class to render all the sprites onto the screen.
 *
 * Usage:
 * 1. Call Begin*() to set the render target
 * 2. Do all the drawing
 * 3. Call matching End*() to finish the drawing
 *
 * Implementation note:
 * Some functions are disabled through a preprocessor.
 * These are currently only used in a software rendering context.
 */
class RenderTarget {
public:
	virtual ~RenderTarget() = default;

	/**
	 * Starts a screen render pass.
	 * When this returns false the render pass must be skipped.
	 * This can happen when the window is minimized.
	 *
	 * @return true if drawing should proceed, false otherwise
	 */
	virtual bool BeginDrawScreen() { return true; }

	/**
	 * Starts rendering to a specific texture.
	 * Can be called standalone, or nested inside a BeginDrawScreen block.
	 *
	 * @return true when rendering to the texture is possible
	 */
	virtual bool BeginDrawTexture(Bitmap& target) = 0;

	/**
	 * Finish rendering to a specific texture.
	 */
	virtual void EndDrawTexture() = 0;

	/**
	 * Ends a screen render pass.
	 */
	virtual void EndDrawScreen() {}

	/** Indicates the end of the drawing loop */
	virtual void EndDraw() {}

	/**
	 * Returns the bitmap passed to BeginDrawTexture.
	 *
	 * @return render target of the texture
	 */
	virtual Bitmap* GetBitmap() = 0;

	/** @return the width of the render target */
	virtual int GetWidth() const = 0;

	/** @return the height of the render target */
	virtual int GetHeight() const = 0;

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
	virtual void Blit(int x, int y, Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) = 0;

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
	virtual void TiledBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) = 0;

#if 0
	// Only used by weather when rendering to a bitmap
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
	virtual void EdgeMirrorBlit(int x, int y, Bitmap const& src, Rect const& src_rect,
		bool mirror_x, bool mirror_y, Opacity const& opacity) = 0;
#endif

	/**
	 * Blits source bitmap stretched to this render target.
	 *
	 * @param dst_rect destination rect.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param opacity opacity for blending.
	 * @param blend_mode Blend mode to use.
	 */
	virtual void StretchBlit(Rect const& dst_rect, Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) = 0;

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
	virtual void FlipBlit(int x, int y, Bitmap const& src, Rect const& src_rect, bool horizontal, bool vertical,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) = 0;

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
	virtual void WaverBlit(int x, int y, double zoom_x, double zoom_y, Bitmap const& src, Rect const& src_rect, int depth, double phase,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) = 0;

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
	virtual void RotateZoomOpacityBlit(int x, int y, int ox, int oy,
		Bitmap const& src, Rect const& src_rect,
		double angle, double zoom_x, double zoom_y,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Normal) = 0;

	/**
	 * Fills rect with color.
	 *
	 * @param dst_rect destination rect.
	 * @param color color for filling.
	 */
	virtual void FillRect(Rect const& dst_rect, const Color &color) = 0;

#if 0
	// Only used by monsters on sprite creation
	/**
	 * Rotates bitmap hue.
	 *
	 * @param x x position.
	 * @param y y position.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param hue hue change, degrees.
	 */
	void HueChangeBlit(int x, int y, Bitmap const& src, Rect const& src_rect, double hue) = 0;
#endif

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
	virtual void ToneBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Tone &tone, Opacity const& opacity) = 0;

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
	virtual void BlendBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Color &color, Opacity const& opacity) = 0;

#if 0
	// Only used by text renderer
	/**
	 * Blits source bitmap to this render target through a mask bitmap.
	 *
	 * @param dst_rect destination rectangle.
	 * @param mask mask bitmap
	 * @param mx mask x position
	 * @param my mask y position
	 * @param src source bitmap.
	 * @param sx source x position
	 * @param sy source y position
	 */
	virtual void MaskedBlit(Rect const& dst_rect, Bitmap const& mask, int mx, int my, Bitmap const& src, int sx, int sy) = 0;

	/**
	 * Blits constant color to this render target through a mask bitmap.
	 *
	 * @param dst_rect destination rectangle.
	 * @param mask mask bitmap
	 * @param mx mask x position
	 * @param my mask y position
	 * @param color source color.
	 */
	virtual void MaskedBlit(Rect const& dst_rect, Bitmap const& mask, int mx, int my, Color const& color) = 0;
#endif

	struct GpuBlitOps {
		double zoom_x = 1.0; double zoom_y = 1.0; double angle = 0.0;
		int waver_depth = -1; double waver_phase = 0.0;
		Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default;
		bool flipx = false; bool flipy = false;
		Tone tone = Tone();
		Color flash = Color();
	};

	/**
	 * A special blit used by the Sprite class when the renderer is hardware
	 * accelerated.
	 * Bypasses all the software rendered caching logic.
	 *
	 * @param x destination x position.
	 * @param y destination y position.
	 * @param ox source origin x.
	 * @param oy source origin y.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rectangle.
	 * @param opacity opacity to apply.
	 * @param ops all the other stuff required by sprites (see EffectsBlit)
	 */
	virtual void GpuBlit(int x, int y, int ox, int oy,
			Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity, const GpuBlitOps& ops) {
		(void)x; (void)y; (void)ox; (void)oy;
		(void)src; (void)src_rect; (void)opacity; (void)ops; };

	/**
	 * Blits source bitmap in tiles to this render target.
	 * Was added because most of the drawables that cache the Tone use TiledBlit.
	 * Makes updating the code easier.
	 *
	 * @param ox tile start x offset.
	 * @param oy tile start y offset.
	 * @param src_rect source bitmap rect.
	 * @param src source bitmap.
	 * @param dst_rect destination rect.
	 * @param opacity opacity for blending.
	 * @param tone tone to apply.
	 * @param blend_mode Blend mode to use.
	 */
	virtual void GpuTiledToneBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
		Opacity const& opacity, const Tone &tone, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) {
			(void)tone;
			TiledBlit(ox, oy, src_rect, src, dst_rect, opacity, blend_mode);
	}

	// Functions with default implementations (forwarders to other functions)
	// Only reimplement them if there are fast paths for them
	/** @return render target bounds rect */
	virtual Rect GetRect() const {
		return Rect(0, 0, GetWidth(), GetHeight());
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
	virtual void BlitFast(int x, int y, Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity) {
		Blit(x, y, src, src_rect, opacity, Bitmap::BlendMode::NormalWithoutAlpha);
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
	virtual void TiledBlit(Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) {
			TiledBlit(0, 0, src_rect, src, dst_rect, opacity, blend_mode);
	}

	/**
	 * Blits source bitmap stretched to this render target.
	 *
	 * @param src source bitmap.
	 * @param src_rect source bitmap rect.
	 * @param opacity opacity for blending.
	 * @param blend_mode Blend mode to use.
	 */
	virtual void StretchBlit(Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) {
			StretchBlit(GetRect(), src, src_rect, opacity, blend_mode);
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
	virtual void ZoomOpacityBlit(int x, int y, int ox, int oy,
			Bitmap const& src, Rect const& src_rect,
			double zoom_x, double zoom_y,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) {
		RotateZoomOpacityBlit(x, y, ox, oy, src, src_rect, 0.0, zoom_x, zoom_y, opacity,
			blend_mode == Bitmap::BlendMode::Default ? Bitmap::BlendMode::Normal : blend_mode);
	};

	/**
	 * Fills entire bitmap with color.
	 *
	 * @param color color for filling.
	 */
	virtual void Fill(const Color &color) {
		FillRect(GetRect(), color);
	}

	/**
	 * Clears the bitmap.
	 * After this operating the area must appear black.
	 */
	virtual void Clear() {
		Fill({0, 0, 0, 255});
	}

	/**
	 * Clears the area of the given bitmap rect.
	 * After this operating the area must appear black.
	 *
	 * @param dst_rect destination rect.
	 */
	virtual void ClearRect(Rect const& dst_rect) {
		FillRect(dst_rect, {0, 0, 0, 255});
	}

	/**
	 * Blits source bitmap with effects.
	 * Note: rotation and waver are mutually exclusive.
	 *
	 * @param x destination x position.
	 * @param y destination y position.
	 * @param ox source origin x.
	 * @param oy source origin y.
	 * @param src source bitmap.
	 * @param src_rect source bitmap rectangle.
	 * @param opacity opacity to apply.
	 * @param zoom_x x scale factor.
	 * @param zoom_y y scale factor.
	 * @param angle rotation angle.
	 * @param waver_depth wave magnitude.
	 * @param waver_phase wave phase.
	 * @param blend_mode Blend mode to use.
	 */
	virtual void EffectsBlit(int x, int y, int ox, int oy,
			Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity,
			double zoom_x, double zoom_y, double angle,
			int waver_depth, double waver_phase,
			Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) {
		if (opacity.IsTransparent()) {
			return;
		}

		bool rotate = angle != 0.0;
		bool scale = zoom_x != 1.0 || zoom_y != 1.0;
		bool waver = waver_depth != 0;

		if (waver) {
			WaverBlit(x - ox * zoom_x, y - oy * zoom_y, zoom_x, zoom_y, src, src_rect,
					waver_depth, waver_phase, opacity, blend_mode);
		}
		else if (rotate) {
			RotateZoomOpacityBlit(x, y, ox, oy, src, src_rect, angle, zoom_x, zoom_y, opacity, blend_mode);
		}
		else if (scale) {
			ZoomOpacityBlit(x, y, ox, oy, src, src_rect, zoom_x, zoom_y, opacity, blend_mode);
		}
		else {
			Blit(x - ox, y - oy, src, src_rect, opacity, blend_mode);
		}
	}

	bool IsHardwareAccelerated() const {
		return is_hardware_accelerated;
	}

protected:
	bool is_hardware_accelerated = false;
};

#endif
