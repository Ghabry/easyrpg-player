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

#ifndef EP_SDL3_UI_GPU_H
#define EP_SDL3_UI_GPU_H

#include "render_target.h"
#include <SDL3/SDL_gpu.h>
#include <array>

class Sdl3Ui;

/**
 * Sdl3RenderTarget class.
 * Provides an implementation of the modern SDL3 GPU API.
 */
class Sdl3RenderTarget final : public RenderTarget {
public:
	static Sdl3RenderTarget* Create(Sdl3Ui& ui);

	~Sdl3RenderTarget();

	SDL_GPUDevice* gpu_device = nullptr;
	SDL_GPUGraphicsPipeline* sprite_pipeline = nullptr;
	SDL_GPUSampler* sprite_sampler = nullptr;
	SDL_GPUBuffer* sprite_vertex_buffer = nullptr;
	SDL_GPUBuffer* sprite_index_buffer = nullptr;

	SDL_GPUCommandBuffer* command_buf = nullptr;
	SDL_GPURenderPass* render_pass = nullptr;
	SDL_GPUTexture* swapchain_texture = nullptr;

	/**
	 * Shader configuration for the sprite shader.
	 * Basically almost everything that is to be rendered is a rectangle with
	 * some effects applied.
	 */
	struct TextureVertex {
		float x, y, z;
		float u, v;
	};

	struct SpriteQuad {
		std::array<TextureVertex, 4> vertices;
		std::array<uint16_t, 6> indices;
	};
	SpriteQuad sprite_quad{
	{{
		{ 0.0f, 0.0f, 0.0f, 0, 0}, // Top-Left
		{ 0.0f, 1.0f, 0.0f, 0, 1}, // Bottom-Left
		{ 1.0f, 0.0f, 0.0f, 1, 0}, // Top-Right
		{ 1.0f, 1.0f, 0.0f, 1, 1}  // Bottom-Right
	}},{
		0, 1, 2, 1, 3, 2           // Triangle order
	}};

	struct SpriteUniform {
		struct {
			/** position (x and y destination) */
			float x, y;
			/** origin (source origin) */
			float ox, oy;
			/** tex_size (texture dimensions) */
			float tex_w, tex_h;
			/** dst_size (target dimensions for scaling) */
			float dst_w, dst_h;
			/** src_rect (source texture rectangle) */
			float src_x, src_y, src_w, src_h;
			/** screen_size (size of the game screen) */
			float screen_w, screen_h;
			/** angle (sprite rotation) */
			float angle = 0.0;
			/** repeat (how often to repeat the texture for tiling) */
			float tiling = 1.0;
		} vertex;

		struct {
			/** tone (Tone colors to apply) */
			float tone_red, tone_green, tone_blue, tone_gray;
			/** flash (Colors for the flash effect) */
			float flash_red, flash_green, flash_blue, flash_alpha;
			/** opacity (top and bottom opacity to apply) */
			float opacity_top, opacity_bottom, opacity_split, padding;
			/** waver (wave magnitude and phase) */
			float waver_depth = -1.0, waver_phase;
			/** blend_mode (Blend mode to use for the blit) */
			float blend_mode = -1.0, padding2;
		} fragment;
	};

	// Check padding requirements
	static_assert(offsetof(SpriteUniform, vertex.x) % 8 == 0);
	static_assert(offsetof(SpriteUniform, vertex.ox) % 8 == 0);
	static_assert(offsetof(SpriteUniform, vertex.tex_w) % 8 == 0);
	static_assert(offsetof(SpriteUniform, vertex.dst_w) % 8 == 0);
	static_assert(offsetof(SpriteUniform, vertex.src_x) % 16 == 0);
	static_assert(offsetof(SpriteUniform, vertex.screen_w) % 8 == 0);
	static_assert(offsetof(SpriteUniform, vertex.angle) % 8 == 0);

	static_assert(offsetof(SpriteUniform, fragment.tone_red) % 16 == 0);
	static_assert(offsetof(SpriteUniform, fragment.flash_red) % 16 == 0);
	static_assert(offsetof(SpriteUniform, fragment.opacity_top) % 16 == 0);
	static_assert(offsetof(SpriteUniform, fragment.waver_depth) % 8 == 0);
	static_assert(offsetof(SpriteUniform, fragment.blend_mode) % 8 == 0);

	/**
	 * Inherited from RenderTarget
	 */
	/** @{ */
	/** Indicates the start of the drawing loop */
	void BeginDraw() override;

	/** Indicates the end of the drawing loop */
	void EndDraw() override;

	Bitmap* GetBitmap() override {
		return nullptr;
	}

	int GetWidth() const override;

	int GetHeight() const override;

	void Blit(int x, int y, Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override;

	void TiledBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override;

	void EdgeMirrorBlit(int x, int y, Bitmap const& src, Rect const& src_rect,
			bool mirror_x, bool mirror_y, Opacity const& opacity) override;

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
	/** @} */

private:
	explicit Sdl3RenderTarget(Sdl3Ui& ui) : ui(&ui) {}
	bool Init();
	SDL_GPUShader* LoadShader(SDL_GPUShaderStage stage, const char* filename, int num_sampler, int num_uniform, int num_storage, int num_texture);
	bool AllocTexture(Bitmap const& src);
	void FreeTexture(Bitmap const& src);

	void Render(Bitmap const& bmp, SpriteUniform uniform);
	SpriteUniform InitUniform(Bitmap const& bmp, Rect const& src_rect, Opacity const& opacity);

	void BeginOrContinueRenderPass(SDL_GPULoadOp load_op = SDL_GPU_LOADOP_LOAD);
	void EndRenderPass();

	Sdl3Ui* ui = nullptr;
};

#endif
