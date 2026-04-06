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

#include "sdl3_render_target.h"
#include "sdl3_ui.h"
#include "output.h"
#include <SDL3/SDL_gpu.h>

Sdl3RenderTarget* Sdl3RenderTarget::Create(Sdl3Ui& ui) {
	auto gpu = new Sdl3RenderTarget(ui);
	gpu->is_hardware_accelerated = true;

	if (!gpu->Init()) {
		Output::Debug("Could not initialize SDL3 Hardware Renderer");
		delete gpu;
		return nullptr;
	}

	SDL_PropertiesID props = SDL_GetGPUDeviceProperties(gpu->gpu_device);
	if (props) {
		auto name = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown");
		auto driver = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, "unknown");
		auto version = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, "unknown");

		Output::Debug("SDL3 GPU: {}, Driver: {}, Version: {}", name, driver, version);
	}

	return gpu;
}

Sdl3RenderTarget::~Sdl3RenderTarget() {
	if (sprite_vertex_buffer) {
		SDL_ReleaseGPUBuffer(gpu_device, sprite_vertex_buffer);
	}

	if (sprite_index_buffer) {
		SDL_ReleaseGPUBuffer(gpu_device, sprite_index_buffer);
	}

	if (sprite_sampler) {
		SDL_ReleaseGPUSampler(gpu_device, sprite_sampler);
	}

	EndRenderPass();

	if (sprite_pipeline) {
		SDL_ReleaseGPUGraphicsPipeline(gpu_device, sprite_pipeline);
	}

	if (blend_pipeline) {
		SDL_ReleaseGPUGraphicsPipeline(gpu_device, blend_pipeline);
	}

	if (texture_game) {
		SDL_ReleaseGPUTexture(gpu_device, texture_game);
	}

	if (texture_game_scaled) {
		SDL_ReleaseGPUTexture(gpu_device, texture_game_scaled);
	}

	if (texture_blend) {
		SDL_ReleaseGPUTexture(gpu_device, texture_blend);
	}

	if (gpu_device) {
		SDL_DestroyGPUDevice(gpu_device);
	}
}

bool Sdl3RenderTarget::BeginDrawScreen() {
	assert(!command_buf);

	command_buf = SDL_AcquireGPUCommandBuffer(gpu_device);
	if (!command_buf) {
		Output::Debug("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
		return false;
	}

	// Get the swapchain texture
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buf, ui->sdl_window, &swapchain_texture, &swapchain_width, &swapchain_height)) {
		Output::Debug("SDL_WaitAndAcquireGPUSwapchainTexture failed: {}", SDL_GetError());
		SDL_SubmitGPUCommandBuffer(command_buf);
		command_buf = nullptr;
		return false;
	}

	if (!swapchain_texture) {
		// End frame early (can happen when the window is e.g. in the background)
		SDL_SubmitGPUCommandBuffer(command_buf);
		command_buf = nullptr;
		return false;
	}

	texture_target = texture_game;

	Clear();

	return true;
}

void Sdl3RenderTarget::EndDrawScreen() {
	EndRenderPass();

	assert(command_buf);

	SDL_GPUBlitInfo blit_info = {};

	blit_info.source.texture = texture_game;
	blit_info.source.w = GetWidth();
	blit_info.source.h = GetHeight();

	blit_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
	blit_info.filter = SDL_GPU_FILTER_NEAREST;

	if (texture_game_scaled && ui->vcfg.scaling_mode.Get() == ConfigEnum::ScalingMode::Bilinear) {
		blit_info.destination.texture = texture_game_scaled;
		blit_info.destination.x = 0;
		blit_info.destination.y = 0;
		blit_info.destination.w = scaled_width;
		blit_info.destination.h = scaled_height;

		SDL_BlitGPUTexture(command_buf, &blit_info);

		blit_info.source.texture = texture_game_scaled;
		blit_info.source.w = scaled_width;
		blit_info.source.h = scaled_height;
		blit_info.filter = SDL_GPU_FILTER_LINEAR;
	}

	blit_info.destination.texture = swapchain_texture;
	blit_info.destination.x = ui->viewport.x;
	blit_info.destination.y = ui->viewport.y;
	blit_info.destination.w = ui->viewport.w;
	blit_info.destination.h = ui->viewport.h;

	if (blit_info.destination.w == 0 || blit_info.destination.h == 0) {
		// fail-safe (viewport is 0 on startup)
		blit_info.destination.w = 1;
		blit_info.destination.h = 1;
	}

	SDL_BlitGPUTexture(command_buf, &blit_info);

	SDL_SubmitGPUCommandBuffer(command_buf);
	command_buf = nullptr;
	texture_target = nullptr;
}

bool Sdl3RenderTarget::BeginDrawTexture(Bitmap& target) {
	if (!AllocTexture(target, true)) {
		return false;
	}

	assert(!command_buf);

	command_buf = SDL_AcquireGPUCommandBuffer(gpu_device);
	if (!command_buf) {
		Output::Debug("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
		return false;
	}

	bitmap_target = &target;
	texture_target = reinterpret_cast<SDL_GPUTexture*>(target.GetGpuTexture());

	Clear();

	return true;
}

void Sdl3RenderTarget::EndDrawTexture() {
	EndRenderPass();

	assert(command_buf);

	SDL_SubmitGPUCommandBuffer(command_buf);
	command_buf = nullptr;

	CopyToBitmap(*bitmap_target);

	texture_target = nullptr;
	bitmap_target = nullptr;
}

int Sdl3RenderTarget::GetWidth() const {
	return bitmap_target ? bitmap_target->width() : ui->current_display_mode.width;
}

int Sdl3RenderTarget::GetHeight() const {
	return bitmap_target ? bitmap_target->height() : ui->current_display_mode.height;
}

void Sdl3RenderTarget::Blit(int x, int y, Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	auto uniform = InitUniform(src, src_rect, opacity);

	const float srw = static_cast<float>(src_rect.width);
	const float srh = static_cast<float>(src_rect.height);
	const float fx = static_cast<float>(x);
	const float fy = static_cast<float>(y);

	// Model Matrix
	uniform.vertex.model_matrix = {{
		{ srw , 0.0f, 0.0f, 0.0f }, // X (width scale)
		{ 0.0f, srh , 0.0f, 0.0f }, // Y (height scale)
		{ 0.0f, 0.0f, 1.0f, 0.0f }, // Z
		{ fx  , fy  , 0.0f, 1.0f }  // Translation
	}};

	auto& frag = uniform.fragment;
	frag.blend_mode = static_cast<float>(blend_mode);

	Render(src, uniform);
}

void Sdl3RenderTarget::TiledBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	GpuTiledToneBlit(ox, oy, src_rect, src, dst_rect, opacity, Tone(), blend_mode);
}

void Sdl3RenderTarget::StretchBlit(Rect const& dst_rect, Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	auto uniform = InitUniform(src, src_rect, opacity);
	auto& vertex = uniform.vertex;

	const float drx = static_cast<float>(dst_rect.x);
	const float dry = static_cast<float>(dst_rect.y);
	const float drw = static_cast<float>(dst_rect.width);
	const float drh = static_cast<float>(dst_rect.height);

	vertex.model_matrix = {{
		{ drw , 0.0f, 0.0f, 0.0f }, // X (width scale)
		{ 0.0f, drh , 0.0f, 0.0f }, // Y (height scale)
		{ 0.0f, 0.0f, 1.0f, 0.0f }, // Z
		{ drx , dry , 0.0f, 1.0f }  // Translation
	}};

	auto& frag = uniform.fragment;
	frag.blend_mode = static_cast<float>(blend_mode);

	Render(src, uniform);
}

void Sdl3RenderTarget::FlipBlit(int x, int y, Bitmap const& src, Rect const& src_rect, bool horizontal, bool vertical,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	GpuBlitOps ops = {};
	ops.flipx = horizontal;
	ops.flipy = vertical;
	GpuBlit(x, y, 0, 0, src, src_rect, opacity, ops);
}

void Sdl3RenderTarget::WaverBlit(int x, int y, double zoom_x, double zoom_y, Bitmap const& src, Rect const& src_rect, int depth, double phase,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	GpuBlitOps ops = {};
	ops.zoom_x = zoom_x;
	ops.zoom_y = zoom_y;
	ops.waver_depth = depth;
	ops.waver_phase = phase;
	ops.blend_mode = blend_mode;
	GpuBlit(x, y, 0, 0, src, src_rect, opacity, ops);
}

void Sdl3RenderTarget::RotateZoomOpacityBlit(int x, int y, int ox, int oy,
		Bitmap const& src, Rect const& src_rect,
		double angle, double zoom_x, double zoom_y,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	GpuBlitOps ops = {};
	ops.angle = angle;
	ops.zoom_x = zoom_x;
	ops.zoom_y = zoom_y;
	ops.blend_mode = blend_mode;
	GpuBlit(x, y, ox, oy, src, src_rect, opacity, ops);
}

void Sdl3RenderTarget::FillRect(Rect const& dst_rect, const Color &color) {
	// Create a 1x1 texture with this color
	// Cached to avoid hundreds of reallocations
	auto it = fill_cache.find(color);
	if (it == fill_cache.end()) {
		BitmapRef bmp = Bitmap::Create(1, 1, color);
		fill_cache[color] = bmp;
		StretchBlit(dst_rect, *bmp, bmp->GetRect(), Opacity::Opaque());
	} else {
		StretchBlit(dst_rect, *(it->second), it->second->GetRect(), Opacity::Opaque());
	}
}

void Sdl3RenderTarget::Clear() {
	EndRenderPass();

	BeginOrContinueRenderPass(sprite_pipeline, SDL_GPU_LOADOP_CLEAR);
}

void Sdl3RenderTarget::ToneBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Tone &tone, Opacity const& opacity) {
	GpuBlitOps ops = {};
	ops.tone = tone;
	GpuBlit(x, y, 0, 0, src, src_rect, opacity, ops);
}

void Sdl3RenderTarget::BlendBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Color &color, Opacity const& opacity) {
	GpuBlitOps ops = {};
	ops.flash = color;
	GpuBlit(x, y, 0, 0, src, src_rect, opacity, ops);
}

void Sdl3RenderTarget::GpuBlit(int x, int y, int ox, int oy,
		Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, const GpuBlitOps& ops) {
	if (src_rect.width <= 0 || src_rect.height <= 0) {
		return;
	}

	auto uniform = InitUniform(src, src_rect, opacity);
	auto& vertex = uniform.vertex;

	if (ops.flipx) {
		vertex.flip_x = 1.0f;
	}

	if (ops.flipy) {
		vertex.flip_y = 1.0f;
	}

	if (ops.waver_depth > 0) {
		vertex.expand_x = 2.0f * ops.waver_depth / src_rect.width;
	}


	double zx = (ops.zoom_x == 0.0) ? 1.0 : ops.zoom_x;
	double zy = (ops.zoom_y == 0.0) ? 1.0 : ops.zoom_y;

	// Rotation
	float c = static_cast<float>(std::cos(ops.angle));
	float s = static_cast<float>(std::sin(ops.angle));

	float w = static_cast<float>(src_rect.width);
	float h = static_cast<float>(src_rect.height);

	// Scale by Quad Size (w, h) and Zoom (zx, zy), then Rotate (c, s)
	float m00 = static_cast<float>(w * zx * c);
	float m01 = static_cast<float>(w * zx * s);
	float m10 = static_cast<float>(-h * zy * s);
	float m11 = static_cast<float>(h * zy * c);

	// Translation
	float m30 = static_cast<float>(x - ox * zx * c + oy * zy * s);
	float m31 = static_cast<float>(y - ox * zx * s - oy * zy * c);

	vertex.model_matrix = {{
		{ m00 , m01 , 0.0f, 0.0f }, // X
		{ m10 , m11 , 0.0f, 0.0f }, // Y
		{ 0.0f, 0.0f, 1.0f, 0.0f }, // Z
		{ m30 , m31 , 0.0f, 1.0f }  // Translation
	}};

	auto& frag = uniform.fragment;
	frag.tone = {
		static_cast<float>(ops.tone.red),
		static_cast<float>(ops.tone.green),
		static_cast<float>(ops.tone.blue),
		static_cast<float>(ops.tone.gray)
	};
	frag.flash = {
		static_cast<float>(ops.flash.red),
		static_cast<float>(ops.flash.green),
		static_cast<float>(ops.flash.blue),
		static_cast<float>(ops.flash.alpha)
	};
	frag.waver_depth = ops.waver_depth;
	frag.waver_phase = ops.waver_phase;
	frag.blend_mode = static_cast<float>(ops.blend_mode);

	Render(src, uniform);
}

void Sdl3RenderTarget::GpuTiledToneBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
		Opacity const& opacity, const Tone &tone, Bitmap::BlendMode blend_mode) {
	if (ox >= src_rect.width)	ox %= src_rect.width;
	if (oy >= src_rect.height)	oy %= src_rect.height;
	if (ox < 0) ox += src_rect.width  * ((-ox + src_rect.width  - 1) / src_rect.width);
	if (oy < 0) oy += src_rect.height * ((-oy + src_rect.height - 1) / src_rect.height);

	auto uniform = InitUniform(src, src_rect, opacity);
	auto& vertex = uniform.vertex;

	const float drx = static_cast<float>(dst_rect.x);
	const float dry = static_cast<float>(dst_rect.y);
	const float drw = static_cast<float>(dst_rect.width);
	const float drh = static_cast<float>(dst_rect.height);

	vertex.model_matrix = {{
		{ drw , 0.0f, 0.0f, 0.0f }, // X (width scale)
		{ 0.0f, drh , 0.0f, 0.0f }, // Y (height scale)
		{ 0.0f, 0.0f, 1.0f, 0.0f }, // Z
		{ drx , dry , 0.0f, 1.0f }  // Translation
	}};

	float tex_w = static_cast<float>(src.width());
	float tex_h = static_cast<float>(src.height());

	vertex.uv_rect = {
		static_cast<float>(src_rect.x + ox) / tex_w, // X offset
		static_cast<float>(src_rect.y + oy) / tex_h, // Y offset
		static_cast<float>(dst_rect.width) / tex_w,  // stretch UV width (will repeat)
		static_cast<float>(dst_rect.height) / tex_h  // stretch UV height (will repeat)
	};

	auto& frag = uniform.fragment;
	frag.tone = {
		static_cast<float>(tone.red),
		static_cast<float>(tone.green),
		static_cast<float>(tone.blue),
		static_cast<float>(tone.gray)
	};
	frag.blend_mode = static_cast<float>(blend_mode);

	Render(src, uniform);
}

bool Sdl3RenderTarget::ChangeDisplaySurfaceResolution(int new_width, int new_height) {
	SDL_GPUTextureCreateInfo tex_create_info = tex_create_info_default;
	tex_create_info.width = new_width;
	tex_create_info.height = new_height;
	tex_create_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	SDL_GPUTexture* new_texture_game = SDL_CreateGPUTexture(gpu_device, &tex_create_info);
	if (!new_texture_game) {
		Output::Debug("SDL_CreateGPUTexture for new_texture_game failed: {}", SDL_GetError());
		return false;
	}

	SDL_ReleaseGPUTexture(gpu_device, texture_game);
	texture_game = new_texture_game;

	return true;
}

void Sdl3RenderTarget::ViewportChanged() {
	if (ui->vcfg.scaling_mode.Get() == ConfigEnum::ScalingMode::Bilinear && ui->window.scale > 0.f) {
		if (texture_game_scaled) {
			SDL_ReleaseGPUTexture(gpu_device, texture_game_scaled);
			scaled_height = 0;
			scaled_width = 0;
		}

		// Create render target texture for the scaled game scene
		SDL_GPUTextureCreateInfo tex_create_info = tex_create_info_default;
		tex_create_info.width = GetWidth() * static_cast<int>(ceilf(ui->window.scale));
		tex_create_info.height = GetHeight() * static_cast<int>(ceilf(ui->window.scale));
		tex_create_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
		texture_game_scaled = SDL_CreateGPUTexture(gpu_device, &tex_create_info);
		if (!texture_game_scaled) {
			Output::Debug("SDL_CreateGPUTexture for texture_game_scaled failed: {}", SDL_GetError());
			return;
		}
		scaled_width = tex_create_info.width;
		scaled_height = tex_create_info.height;
	}
}

bool Sdl3RenderTarget::CopyToBitmap(Bitmap& target) {
	assert(!command_buf);

	command_buf = SDL_AcquireGPUCommandBuffer(gpu_device);
	if (!command_buf) {
		Output::Debug("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
		return false;
	}

	SDL_GPUCopyPass* dl_copy_pass = SDL_BeginGPUCopyPass(command_buf);

	int size = target.pitch() * target.height();
	SDL_GPUTransferBufferCreateInfo dl_buffer_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, static_cast<Uint32>(size)};
	SDL_GPUTransferBuffer* dl_transfer_buf = SDL_CreateGPUTransferBuffer(gpu_device, &dl_buffer_info);
	if (!dl_transfer_buf) {
		Output::Debug("SDL_CreateGPUTransferBuffer for download failed:", SDL_GetError());
		return false;
	}

	SDL_GPUTextureRegion dl_region{};
	dl_region.texture = texture_target ? texture_target : texture_game;
	dl_region.w = target.width();
	dl_region.h = target.height();
	dl_region.d = 1;

	SDL_GPUTextureTransferInfo dl_transfer_info{};
	dl_transfer_info.transfer_buffer = dl_transfer_buf;
	dl_transfer_info.pixels_per_row = target.pitch() / 4;
	dl_transfer_info.rows_per_layer = target.height();

	SDL_DownloadFromGPUTexture(dl_copy_pass, &dl_region, &dl_transfer_info);

	SDL_EndGPUCopyPass(dl_copy_pass);

	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buf);
	command_buf = nullptr;

	if (!fence) {
		SDL_ReleaseGPUTransferBuffer(gpu_device, dl_transfer_buf);
		Output::Debug("SDL_SubmitGPUCommandBufferAndAcquireFence for download failed:", SDL_GetError());
		return false;
	}

	if (!SDL_WaitForGPUFences(gpu_device, true, &fence, 1)) {
		SDL_ReleaseGPUTransferBuffer(gpu_device, dl_transfer_buf);
		Output::Debug("SDL_WaitForGPUFences for download failed:", SDL_GetError());
		return false;
	}
	SDL_ReleaseGPUFence(gpu_device, fence);

	uint8_t* downloaded_data = reinterpret_cast<uint8_t*>(SDL_MapGPUTransferBuffer(
		gpu_device,
		dl_transfer_buf,
		false
	));

	memcpy(target.pixels(), downloaded_data, size);

	SDL_UnmapGPUTransferBuffer(gpu_device, dl_transfer_buf);
	SDL_ReleaseGPUTransferBuffer(gpu_device, dl_transfer_buf);

	return true;
}

/*
Based on TexturedQuad.c and TexturedAnimatedQuad.c from
https://github.com/TheSpydog/SDL_gpu_examples by
Copyright (C) 2024 Caleb Cornett
License: zlib
*/

bool Sdl3RenderTarget::Init() {
#ifdef NDEBUG
	const bool gpu_debug = false;
#else
	const bool gpu_debug = true;
#endif

	// Good defaults for most textures
	tex_create_info_default.type = SDL_GPU_TEXTURETYPE_2D;
	tex_create_info_default.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	tex_create_info_default.layer_count_or_depth = 1;
	tex_create_info_default.num_levels = 1;

	gpu_device = SDL_CreateGPUDevice(
		SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXIL,
		gpu_debug, nullptr);
	if (!gpu_device) {
		Output::Debug("SDL_CreateGPUDevice failed: {}", SDL_GetError());
		return false;
	}

	if (!SDL_ClaimWindowForGPUDevice(gpu_device, ui->sdl_window)) {
		Output::Debug("SDL_ClaimWindowForGPUDevice failed: {}", SDL_GetError());
		return false;
	}

	SDL_GPUShader* sprite_vertex_shader = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX, "sprite.vert", 0, 1, 0, 0);
	if (!sprite_vertex_shader) {
		Output::Debug("SDL GPU: Loading vertex shader failed: {}", SDL_GetError());
		return false;
	}

	SDL_GPUShader* sprite_fragment_shader = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, "sprite.frag", 1, 1, 0, 0);
	if (!sprite_fragment_shader) {
		Output::Debug("SDL GPU: Loading fragment shader failed: {}", SDL_GetError());
		SDL_ReleaseGPUShader(gpu_device, sprite_vertex_shader);
		return false;
	}

	SDL_GPUShader* blend_fragment_shader = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, "blend.frag", 2, 1, 0, 0);
	if (!blend_fragment_shader) {
		Output::Debug("SDL GPU: Loading blend fragment shader failed: {}", SDL_GetError());
		SDL_ReleaseGPUShader(gpu_device, blend_fragment_shader);
		return false;
	}

	// Create the graphics pipeline for our sprite shader
	// This uploads the shader and the sprite quad to the GPU for later usage
	SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
	pipeline_info.vertex_shader = sprite_vertex_shader;
	pipeline_info.fragment_shader = sprite_fragment_shader;
	pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

	// Vertex Buffer
	std::array<SDL_GPUVertexBufferDescription, 1> vertex_buffer;
	vertex_buffer[0].slot = 0;
	vertex_buffer[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffer[0].instance_step_rate = 0;
	vertex_buffer[0].pitch = sizeof(TextureVertex);

	pipeline_info.vertex_input_state.num_vertex_buffers = vertex_buffer.size();
	pipeline_info.vertex_input_state.vertex_buffer_descriptions = vertex_buffer.data();

	// Vertex Attributes
	std::array<SDL_GPUVertexAttribute, 2> vertex_attrib;

	// TextureVertex: x, y, z
	// VertexShader: a_position
	vertex_attrib[0].buffer_slot = 0;
	vertex_attrib[0].location = 0;
	vertex_attrib[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertex_attrib[0].offset = offsetof(TextureVertex, x);

	// TextureVertex: u, v
	// VertexShader: a_coord
	vertex_attrib[1].buffer_slot = 0;
	vertex_attrib[1].location = 1;
	vertex_attrib[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	vertex_attrib[1].offset = offsetof(TextureVertex, u);

	pipeline_info.vertex_input_state.num_vertex_attributes = vertex_attrib.size();
	pipeline_info.vertex_input_state.vertex_attributes = vertex_attrib.data();

	// Color target
	std::array<SDL_GPUColorTargetDescription, 1> color_target_desc;
	color_target_desc[0] = {};
	color_target_desc[0].format = SDL_GetGPUSwapchainTextureFormat(gpu_device, ui->sdl_window);
	pipeline_info.target_info.num_color_targets = color_target_desc.size();
	pipeline_info.target_info.color_target_descriptions = color_target_desc.data();

	// Setup sprite pipeline (using premultiplied alpha)
	color_target_desc[0].blend_state.enable_blend = true;
	color_target_desc[0].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
	color_target_desc[0].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	color_target_desc[0].blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
	color_target_desc[0].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
	color_target_desc[0].blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	color_target_desc[0].blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
	sprite_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);
	if (!sprite_pipeline) {
		Output::Debug("SDL_CreateGPUGraphicsPipeline failed: {}", SDL_GetError());
		return false;
	}

	// Blend pipeline is fortunately the same just with a different shader and no alpha blending
	pipeline_info.fragment_shader = blend_fragment_shader;
	color_target_desc[0].blend_state.enable_blend = false;
	blend_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);
	if (!blend_pipeline) {
		Output::Debug("SDL_CreateGPUGraphicsPipeline failed: {}", SDL_GetError());
		return false;
	}

	// Shaders can be deleted after creating the pipelines
	SDL_ReleaseGPUShader(gpu_device, sprite_vertex_shader);
	SDL_ReleaseGPUShader(gpu_device, sprite_fragment_shader);
	SDL_ReleaseGPUShader(gpu_device, blend_fragment_shader);

	// Texture Sampler
	SDL_GPUSamplerCreateInfo sampler_create_info{};
	sampler_create_info.min_filter = SDL_GPU_FILTER_NEAREST;
	sampler_create_info.mag_filter = SDL_GPU_FILTER_NEAREST;
	sampler_create_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
	sampler_create_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	sampler_create_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	sampler_create_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
	sprite_sampler = SDL_CreateGPUSampler(gpu_device, &sampler_create_info);
	if (!sprite_sampler) {
		Output::Debug("SDL_CreateGPUSampler failed: {}", SDL_GetError());
		return false;
	}

	// Vertices for the sprite textures
	SDL_GPUBufferCreateInfo buffer_info{SDL_GPU_BUFFERUSAGE_VERTEX, sizeof(sprite_quad.vertices)};
	sprite_vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &buffer_info);
	if (!sprite_vertex_buffer) {
		Output::Debug("SDL_CreateGPUBuffer for vertex failed: {}", SDL_GetError());
		return false;
	}

	// Vertex order for the triangle list
	SDL_GPUBufferCreateInfo index_info{SDL_GPU_BUFFERUSAGE_INDEX, sizeof(sprite_quad.indices)};
	sprite_index_buffer = SDL_CreateGPUBuffer(gpu_device, &index_info);
	if (!sprite_index_buffer) {
		Output::Debug("SDL_CreateGPUBuffer for index failed: {}", SDL_GetError());
		return false;
	}

	// Transfer description (to GPU) for sprite quad
	SDL_GPUTransferBufferCreateInfo transfer_create_info{
		SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,	sizeof(sprite_quad)};
	SDL_GPUTransferBuffer* sprite_transfer_buf = SDL_CreateGPUTransferBuffer(
		gpu_device, &transfer_create_info
	);
	if (!sprite_transfer_buf) {
		Output::Debug("SDL_CreateGPUBuffer for sprite_transfer_buf failed: {}", SDL_GetError());
		return false;
	}

	TextureVertex* sprite_transfer_data = (TextureVertex*)SDL_MapGPUTransferBuffer(gpu_device, sprite_transfer_buf, false);
	if (!sprite_transfer_data) {
		Output::Debug("SDL_MapGPUTransferBuffer for sprite_transfer_buf failed: {}", SDL_GetError());
		return false;
	}
	memcpy(sprite_transfer_data, &sprite_quad, sizeof(sprite_quad));
	SDL_UnmapGPUTransferBuffer(gpu_device, sprite_transfer_buf);

	// Create render target texture for the game scene
	SDL_GPUTextureCreateInfo tex_create_info = tex_create_info_default;
	tex_create_info.width = GetWidth();
	tex_create_info.height = GetHeight();
	tex_create_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	texture_game = SDL_CreateGPUTexture(gpu_device, &tex_create_info);
	if (!texture_game) {
		Output::Debug("SDL_CreateGPUTexture for texture_game failed: {}", SDL_GetError());
		return false;
	}

	// Transfer to GPU
	SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);
	if (!command_buffer) {
		Output::Debug("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
		return false;
	}

	SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);

	// Upload vertices
	SDL_GPUTransferBufferLocation vertex_location{sprite_transfer_buf, offsetof(SpriteQuad, vertices)};
	SDL_GPUBufferRegion vertex_region{sprite_vertex_buffer, 0, sizeof(sprite_quad.vertices)};
	SDL_UploadToGPUBuffer(copy_pass, &vertex_location, &vertex_region, false);

	// Upload index buffer
	SDL_GPUTransferBufferLocation index_location{sprite_transfer_buf, offsetof(SpriteQuad, indices)};
	SDL_GPUBufferRegion index_region{sprite_index_buffer, 0, sizeof(sprite_quad.indices)};
	SDL_UploadToGPUBuffer(copy_pass, &index_location, &index_region, false);

	SDL_EndGPUCopyPass(copy_pass);

	SDL_SubmitGPUCommandBuffer(command_buffer);

	SDL_ReleaseGPUTransferBuffer(gpu_device, sprite_transfer_buf);

	return true;
}

SDL_GPUShader* Sdl3RenderTarget::LoadShader(SDL_GPUShaderStage stage, const char* filename, int num_sampler, int num_uniform, int num_storage, int num_texture) {
	// TODO: Load from buffer instead
	std::string fullname = filename;
	SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(gpu_device);
	const char *entrypoint;

	if (format & SDL_GPU_SHADERFORMAT_SPIRV) {
		fullname += ".spv";
		format = SDL_GPU_SHADERFORMAT_SPIRV;
		entrypoint = "main";
	} else if (format & SDL_GPU_SHADERFORMAT_MSL) {
		fullname += ".msl";
		format = SDL_GPU_SHADERFORMAT_MSL;
		entrypoint = "main0";
	} else if (format & SDL_GPU_SHADERFORMAT_DXIL) {
		fullname += ".dxil";
		format = SDL_GPU_SHADERFORMAT_DXIL;
		entrypoint = "main";
	} else {
		Output::Debug("Unrecognized backend shader format {}", format);
		return nullptr;
	}

	size_t code_size;
	void* code = SDL_LoadFile(fullname.c_str(), &code_size);
	if (!code) {
		Output::Debug("Failed to load shader from disk! {}", fullname);
		return nullptr;
	}

	SDL_GPUShaderCreateInfo shader_info;
	shader_info.code_size = code_size;
	shader_info.code = reinterpret_cast<const Uint8*>(code);
	shader_info.entrypoint = entrypoint;
	shader_info.format = format;
	shader_info.stage = stage;
	shader_info.num_samplers = num_sampler;
	shader_info.num_storage_textures = num_texture;
	shader_info.num_storage_buffers = num_storage;
	shader_info.num_uniform_buffers = num_uniform;

	SDL_GPUShader* shader = SDL_CreateGPUShader(gpu_device, &shader_info);
	if (!shader) {
		Output::Debug("SDL GPU: Failed to create shader!");
		SDL_free(code);
		return nullptr;
	}

	SDL_free(code);

	return shader;
}

bool Sdl3RenderTarget::AllocTexture(Bitmap const& bmp, bool is_rendertarget) {
	if (!bmp.GetGpuTexture()) {
		EndRenderPass();

		SDL_GPUTextureCreateInfo tex_create_info = tex_create_info_default;
		tex_create_info.width = bmp.width();
		tex_create_info.height = bmp.height();
		tex_create_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
		if (is_rendertarget) {
			tex_create_info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
		}

		SDL_GPUTexture* texture = SDL_CreateGPUTexture(gpu_device, &tex_create_info);
		if (!texture) {
			Output::Debug("SDL_CreateGPUTexture for bitmap {} failed: {}", bmp.GetId(), SDL_GetError());
			return false;
		}

		auto texture_sg = lcf::makeScopeGuard([&]() {
			SDL_ReleaseGPUTexture(gpu_device, texture);
		});

		// Request a shared buffer fo upload to GPU
		int size = bmp.pitch() * bmp.height();
		SDL_GPUTransferBufferCreateInfo tex_buffer_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, static_cast<Uint32>(size)};
		SDL_GPUTransferBuffer* tex_transfer_buf = SDL_CreateGPUTransferBuffer(gpu_device, &tex_buffer_info);
		if (!tex_transfer_buf) {
			Output::Debug("SDL_CreateGPUTransferBuffer for bitmap {} failed: {}", bmp.GetId(), SDL_GetError());
			SDL_ReleaseGPUTransferBuffer(gpu_device, tex_transfer_buf);
			return false;
		}

		Uint8* tex_transfer_ptr = (Uint8*)SDL_MapGPUTransferBuffer(gpu_device, tex_transfer_buf, false);
		if (!tex_transfer_ptr) {
			Output::Debug("SDL_MapGPUTransferBuffer for bitmap {} failed: {}", bmp.GetId(), SDL_GetError());
			return false;
		}
		SDL_memcpy(tex_transfer_ptr, bmp.pixels(), size);
		SDL_UnmapGPUTransferBuffer(gpu_device, tex_transfer_buf);

		SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buf);

		SDL_GPUTextureTransferInfo tex_transfer_info{};
		tex_transfer_info.transfer_buffer = tex_transfer_buf;
		tex_transfer_info.pixels_per_row = bmp.pitch() / 4;
		tex_transfer_info.rows_per_layer = bmp.height();
		SDL_GPUTextureRegion tex_region{};
		tex_region.texture = texture;
		tex_region.w = bmp.width();
		tex_region.h = bmp.height();
		tex_region.d = 1;
		SDL_UploadToGPUTexture(copy_pass, &tex_transfer_info, &tex_region, false);

		SDL_EndGPUCopyPass(copy_pass);
		SDL_ReleaseGPUTransferBuffer(gpu_device, tex_transfer_buf);

		bmp.SetGpuTexture(texture, [this](Bitmap const& bmp) {
			EndRenderPass();
			SDL_ReleaseGPUTexture(gpu_device, reinterpret_cast<SDL_GPUTexture*>(bmp.GetGpuTexture()));
		});
		texture_sg.Dismiss();
	}

	return true;
}

void Sdl3RenderTarget::Render(Bitmap const& bmp, SpriteUniform uniform) {
	if (!AllocTexture(bmp)) {
		return;
	}

	// Determine which blending to do
	auto blend_mode = static_cast<int>(uniform.fragment.blend_mode);
	bool is_complex_blend = (blend_mode > static_cast<int>(Bitmap::BlendMode::NormalWithoutAlpha));

	// Sprite sampler
	std::array<SDL_GPUTextureSamplerBinding, 2> sampler_bindings;
	sampler_bindings[0].texture = reinterpret_cast<SDL_GPUTexture*>(bmp.GetGpuTexture());
	sampler_bindings[0].sampler = sprite_sampler;
	Uint32 num_samplers = 1;

	// Handle the blend modes
	if (is_complex_blend) {
		EndRenderPass();

		// Ensure to have a blend target that is large enough
		if (!texture_blend || blend_width < GetWidth() || blend_height < GetHeight()) {
			if (texture_blend) {
				SDL_ReleaseGPUTexture(gpu_device, texture_blend);
			}
			SDL_GPUTextureCreateInfo tex_create_info = tex_create_info_default;
			tex_create_info.width = GetWidth();
			tex_create_info.height = GetHeight();
			tex_create_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
			texture_blend = SDL_CreateGPUTexture(gpu_device, &tex_create_info);
			if (!texture_blend) {
				Output::Debug("SDL_CreateGPUTexture for texture_blend failed: {}", SDL_GetError());
				return;
			}
		}

		// Copy from the current render target (before we draw over it) to the blend texture
		// This one becomes the input for the blend effects
		SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buf);
		SDL_GPUTextureLocation src_loc{};
		src_loc.texture = texture_target;
		SDL_GPUTextureLocation dst_loc{};
		dst_loc.texture = texture_blend;
		SDL_CopyGPUTextureToTexture(copy_pass, &src_loc, &dst_loc, GetWidth(), GetHeight(), 1, false);
		SDL_EndGPUCopyPass(copy_pass);

		BeginOrContinueRenderPass(blend_pipeline);

		// Update the Uniform
		SDL_PushGPUVertexUniformData(command_buf, 0, &uniform.vertex, sizeof(SpriteUniform::vertex));
		SDL_PushGPUFragmentUniformData(command_buf, 0, &uniform.fragment, sizeof(SpriteUniform::fragment));

		// Sprite sampler (with the scene provided in texture_blend)
		sampler_bindings[1].texture = texture_blend;
		sampler_bindings[1].sampler = sprite_sampler;
		SDL_BindGPUFragmentSamplers(render_pass, 0, sampler_bindings.data(), 2);

		// Issue a draw call
		SDL_DrawGPUIndexedPrimitives(render_pass, 6, 1, 0, 0, 0);

		EndRenderPass();

		BeginOrContinueRenderPass(blend_pipeline);

		num_samplers = 2;
	} else {
		BeginOrContinueRenderPass(sprite_pipeline);
	}

	// Update the Uniform
	SDL_PushGPUVertexUniformData(command_buf, 0, &uniform.vertex, sizeof(SpriteUniform::vertex));
	SDL_PushGPUFragmentUniformData(command_buf, 0, &uniform.fragment, sizeof(SpriteUniform::fragment));

    SDL_BindGPUFragmentSamplers(render_pass, 0, sampler_bindings.data(), num_samplers);

	// Issue a draw call
	SDL_DrawGPUIndexedPrimitives(render_pass, 6, 1, 0, 0, 0);
}

Sdl3RenderTarget::SpriteUniform Sdl3RenderTarget::InitUniform(Bitmap const& bmp, Rect const& src_rect, Opacity const& opacity) {
	SpriteUniform uniform = {};
	auto& vertex = uniform.vertex;

	const float w = GetWidth();
	const float h = GetHeight();

	// Orthographic Projection Matrix
	// Maps pixel coordinates to Normalized Device Coordinates
	vertex.proj_matrix = {{
		{ 2.0f / w, 0.0f     , 0.0f, 0.0f },
		{ 0.0f    , -2.0f / h, 0.0f, 0.0f },
		{ 0.0f    , 0.0f     , 1.0f, 0.0f },
		{ -1.0f   , 1.0f     , 0.0f, 1.0f }
	}};

	float tex_w = static_cast<float>(bmp.width());
	float tex_h = static_cast<float>(bmp.height());

	vertex.uv_rect = {
		static_cast<float>(src_rect.x) / tex_w, // X offset
		static_cast<float>(src_rect.y) / tex_h, // Y offset
		static_cast<float>(src_rect.width) / tex_w,
		static_cast<float>(src_rect.height) / tex_h
	};

	auto& frag = uniform.fragment;
	frag.opacity_top = opacity.top;
	frag.opacity_bottom = opacity.bottom;

	if (opacity.IsSplit()) {
		frag.opacity_split = 255.0f - static_cast<float>(opacity.split) / src_rect.height * 255.0f;
	}

	frag.transparent = bmp.GetTransparent() ? 1.0f : 0.0f;

	Tone tone;
	frag.tone = {
		static_cast<float>(tone.red),
		static_cast<float>(tone.green),
		static_cast<float>(tone.blue),
		static_cast<float>(tone.gray)
	};

	Color color;
	frag.flash = {
		static_cast<float>(color.red),
		static_cast<float>(color.green),
		static_cast<float>(color.blue),
		static_cast<float>(color.alpha)
	};

	return uniform;
}

void Sdl3RenderTarget::BeginOrContinueRenderPass(SDL_GPUGraphicsPipeline* pipeline, SDL_GPULoadOp load_op) {
	if (render_pass) {
		if (pipeline != bound_pipeline) {
			// Start a new render pass using the new pipeline
			EndRenderPass();
		} else {
			return;
		}
	}

	SDL_GPUColorTargetInfo color_info{};
	color_info.texture = texture_target;
	color_info.clear_color = {0.f, 0.f, 0.f, 1.0f}; // Opaque black
	color_info.load_op = load_op;
	color_info.store_op = SDL_GPU_STOREOP_STORE;

	render_pass = SDL_BeginGPURenderPass(command_buf, &color_info, 1, nullptr);

	SDL_BindGPUGraphicsPipeline(render_pass, pipeline);
	bound_pipeline = pipeline;

	// Bind Vertex buffer
	std::array<SDL_GPUBufferBinding, 1> vertex_buffer_bindings;
	vertex_buffer_bindings[0].buffer = sprite_vertex_buffer;
	vertex_buffer_bindings[0].offset = 0;
	SDL_BindGPUVertexBuffers(render_pass, 0, vertex_buffer_bindings.data(), vertex_buffer_bindings.size());

	// Bind Index Buffer
	SDL_GPUBufferBinding index_buffer_binding{sprite_index_buffer, 0};
	SDL_BindGPUIndexBuffer(render_pass, &index_buffer_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
}

void Sdl3RenderTarget::EndRenderPass() {
	if (!render_pass) {
		return;
	}

	SDL_EndGPURenderPass(render_pass);
	render_pass = nullptr;
}
