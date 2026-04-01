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
		float x, y;
		float width, height;
		float screen_w, screen_h;
		float padding[2];
	};

	SpriteUniform sprite_uniform{};
	SDL_GPUTexture* sdl_texture = nullptr;

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

	int GetWidth() const override {
		return 0;
	}

	int GetHeight() const override {
		return 0;
	}

	Rect GetRect() const override {
		return {};
	}

	void Blit(int x, int y, Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
	}

	void TiledBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
	}

	void EdgeMirrorBlit(int x, int y, Bitmap const& src, Rect const& src_rect,
			bool mirror_x, bool mirror_y, Opacity const& opacity) override {
	}

	void StretchBlit(Rect const& dst_rect, Bitmap const& src, Rect const& src_rect,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
	}

	void FlipBlit(int x, int y, Bitmap const& src, Rect const& src_rect, bool horizontal, bool vertical,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
	}

	void WaverBlit(int x, int y, double zoom_x, double zoom_y, Bitmap const& src, Rect const& src_rect, int depth, double phase,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
	}

	void RotateZoomOpacityBlit(int x, int y, int ox, int oy,
			Bitmap const& src, Rect const& src_rect,
			double angle, double zoom_x, double zoom_y,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Normal) override {
	}

	void ZoomOpacityBlit(int x, int y, int ox, int oy,
			Bitmap const& src, Rect const& src_rect,
			double zoom_x, double zoom_y,
			Opacity const& opacity, Bitmap::BlendMode blend_mode = Bitmap::BlendMode::Default) override {
	}

	void FillRect(Rect const& dst_rect, const Color &color) override {
	}

	void ToneBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Tone &tone, Opacity const& opacity) override {
	}

	void BlendBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Color &color, Opacity const& opacity) override {
	}
	/** @} */

private:
	explicit Sdl3RenderTarget(Sdl3Ui& ui) : ui(&ui) {}
	bool Init();
	SDL_GPUShader* LoadShader(SDL_GPUShaderStage stage, const char* filename, int num_sampler, int num_uniform, int num_storage, int num_texture);

	Sdl3Ui* ui = nullptr;
};

#endif
