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

#ifndef EP_PLATFORM_3DS_CTR_RENDER_TARGET_H
#define EP_PLATFORM_3DS_CTR_RENDER_TARGET_H

#include "render_target.h"
#include <citro3d.h>
#include <memory>
#include <unordered_set>

struct CtrTexture;

/**
 * Hardware accelerated render target for the PICA200 GPU of the 3DS (citro3d).
 *
 * All effects (opacity, tone, flash) are done in the texture combiners (TEV).
 * Blits are batched: Consecutive blits that share texture and effects are
 * submitted as one draw call.
 *
 * Bitmaps are allocated in linear memory by the pixel allocator (see InstallPixelAllocator)
 * so that the upload to the GPU is done by the GX transfer engine without CPU work.
 *
 * Inaccuracies compared to the software renderer:
 *  - Only Normal, Additive, Multiply, Screen, Darken and Lighten blend modes are supported.
 *    All other blend modes are drawn as Normal.
 *  - Oversaturation (Tone gray > 128) is ignored.
 *  - Tone and flash are not scaled by the pixel alpha for semi-transparent pixels.
 */
class CtrRenderTarget final : public RenderTarget {
public:
	/**
	 * Creates the render target.
	 *
	 * @param width screen width
	 * @param height screen height
	 * @return render target or nullptr on failure
	 */
	static std::unique_ptr<CtrRenderTarget> Create(int width, int height);

	~CtrRenderTarget() override;

	/**
	 * Installs the Bitmap pixel allocator that puts the bitmaps in linear memory.
	 * Must be called before any bitmap is created.
	 */
	static void InstallPixelAllocator();

	bool BeginDrawScreen() override;
	void EndDrawScreen() override;
	bool BeginDrawTexture(Bitmap& target) override;
	void EndDrawTexture() override;

	Bitmap* GetBitmap() override {
		return bitmap_target;
	}

	int GetWidth() const override;
	int GetHeight() const override;

	void Blit(int x, int y, Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override;
	void TiledBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override;
	void StretchBlit(Rect const& dst_rect, Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override;
	void FlipBlit(int x, int y, Bitmap const& src, Rect const& src_rect, bool horizontal, bool vertical,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override;
	void WaverBlit(int x, int y, double zoom_x, double zoom_y, Bitmap const& src, Rect const& src_rect, int depth, double phase,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override;
	void RotateZoomOpacityBlit(int x, int y, int ox, int oy,
		Bitmap const& src, Rect const& src_rect,
		double angle, double zoom_x, double zoom_y,
		Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Normal) override;
	void FillRect(Rect const& dst_rect, const Color &color) override;
	void Clear() override;
	void ToneBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Tone &tone, Opacity const& opacity) override;
	void BlendBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Color &color, Opacity const& opacity) override;
	void GpuBlit(int x, int y, int ox, int oy,
		Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, const GpuBlitOps& ops) override;
	void GpuTiledToneBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
		Opacity const& opacity, const Tone &tone, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override;

	/** @return Whether a frame was started with BeginDrawScreen and not finished with EndFrame yet */
	bool IsInFrame() const;

	/**
	 * Submits the frame to the GPU. Called by the UI after presenting the screen texture.
	 */
	void EndFrame();

	/** @return texture the game screen is rendered to (valid until the next BeginDrawScreen) */
	C3D_Tex* GetScreenTexture() {
		return &screen_tex;
	}

	/**
	 * Copies the content of the game screen texture into a bitmap.
	 * Must be called outside of a frame. Blocks until the GPU is done.
	 *
	 * @param dst bitmap of screen size
	 * @return whether copying was successful
	 */
	bool CopyScreenToBitmap(Bitmap& dst);

	/**
	 * Enables or disables the renderer.
	 * When disabled the GPU is not used for drawing and memory is freed immediately.
	 *
	 * @param enabled new state
	 */
	void SetEnabled(bool enabled);

	/** Called by the texture release hook of the bitmaps */
	void ReleaseTexture(CtrTexture* tex);

private:
	CtrRenderTarget(int width, int height);
	bool Init();

	/** Effect configuration of the texture combiners */
	struct TevKey {
		uint8_t flags = 0;
		uint8_t saturation = 255;
		uint32_t tone_mul = 0;
		uint32_t flash = 0;

		bool operator==(const TevKey& o) const {
			return flags == o.flags && saturation == o.saturation && tone_mul == o.tone_mul && flash == o.flash;
		}
	};

	enum class Blend : uint8_t {
		Normal,
		Replace,
		Additive,
		Multiply,
		Screen,
		Darken,
		Lighten,
		Invalid
	};

	struct BatchState {
		C3D_Tex* tex = nullptr;
		Blend blend = Blend::Invalid;
		TevKey tev;

		bool operator==(const BatchState& o) const {
			return tex == o.tex && blend == o.blend && tev == o.tev;
		}
	};

	/** Float rectangle */
	struct FRect {
		float x, y, w, h;
	};

	/** Affine transformation from local quad space into target space */
	struct Affine {
		float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, tx = 0.0f, ty = 0.0f;
	};

	struct Effects {
		Tone tone;
		Color flash;
		Bitmap::BlendMode blend = Bitmap::BlendMode::Default;
	};

	void BeginFrame(bool sync_draw);
	void BindState();
	void SetProjection(int tex_width, int tex_height);
	void ResetBatch();
	void Flush();
	void SetState(const BatchState& state);
	void ApplyTev(const TevKey& key);
	void ApplyBlend(Blend blend);
	void AddQuad(const float (&pos)[4][2], const float (&uv)[4][2], uint32_t color);

	CtrTexture* GetTexture(Bitmap const& bmp);
	CtrTexture* CreateRenderTexture(Bitmap& bmp);

	/**
	 * Draws the part src_rect of src. The quad covers [0, src_rect.w] x [0, src_rect.h]
	 * in local space and is mapped by xform to the target.
	 *
	 * @param src source bitmap
	 * @param src_rect source rectangle
	 * @param xform local space to target space transformation
	 * @param opacity opacity (split is relative to the top of the local space)
	 * @param flip_x horizontal flip of the source
	 * @param flip_y vertical flip of the source
	 * @param fx effects to apply
	 * @param clip_y0 only draw the local space rows >= clip_y0
	 * @param clip_y1 only draw the local space rows < clip_y1
	 */
	void DrawImage(Bitmap const& src, const FRect& src_rect, const Affine& xform,
		Opacity const& opacity, bool flip_x, bool flip_y, const Effects& fx,
		float clip_y0 = -1.0f, float clip_y1 = 1e9f);

	void DrawWaver(int x, int y, double zoom_x, double zoom_y, Bitmap const& src, Rect const& src_rect,
		int depth, double phase, Opacity const& opacity, bool flip_x, bool flip_y, const Effects& fx);

	void DrawFill(Rect const& dst_rect, const Color& color, Blend blend);

	int screen_width = 0;
	int screen_height = 0;
	int target_width = 0;
	int target_height = 0;

	C3D_Tex screen_tex = {};
	C3D_RenderTarget* screen_target = nullptr;

	Bitmap* bitmap_target = nullptr;
	bool texture_pass_standalone = false;

	DVLB_s* shader_dvlb = nullptr;
	shaderProgram_s shader_program = {};
	int uloc_projection = -1;
	C3D_AttrInfo attr_info = {};
	C3D_BufInfo buf_info = {};

	void* vertex_buffer = nullptr;
	uint16_t* index_buffer = nullptr;
	int batch_start = 0;
	int batch_count = 0;
	bool overflow_warned = false;

	BatchState current_state;
	bool state_valid = false;

	/** Vertex color modifiers of the current tone (added color) */
	uint8_t tone_add[3] = {};

	std::unordered_set<CtrTexture*> live_textures;
	bool enabled = true;
};

#endif
