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

	if (!gpu->Init()) {
		delete gpu;
		return nullptr;
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

	if (sprite_pipeline) {
		SDL_ReleaseGPUGraphicsPipeline(gpu_device, sprite_pipeline);
	}

	if (gpu_device) {
		SDL_DestroyGPUDevice(gpu_device);
	}
}

void Sdl3RenderTarget::BeginDraw() {
	command_buf = SDL_AcquireGPUCommandBuffer(gpu_device);
	if (!command_buf) {
		Output::Debug("SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
		return;
	}

	// Get the swapchain texture
	Uint32 width, height;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buf, ui->sdl_window, &swapchain_texture, &width, &height)) {
		Output::Debug("SDL_WaitAndAcquireGPUSwapchainTexture failed: {}", SDL_GetError());
		return;
	}
	if (!swapchain_texture) {
		// End frame early (can happen when the window is e.g. in the background)
		SDL_SubmitGPUCommandBuffer(command_buf);
		return;
	}

	// Create Color target (clears the screen)
	SDL_GPUColorTargetInfo color_info{};
	color_info.clear_color = {1.f, 0.f, 0.f, 1.0f}; // Opaque black (well or red for testing :))
	color_info.load_op = SDL_GPU_LOADOP_CLEAR;
	color_info.store_op = SDL_GPU_STOREOP_STORE;
	color_info.texture = swapchain_texture;

	// Clear the screen
	SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buf, &color_info, 1, nullptr);
	SDL_EndGPURenderPass(render_pass);
}

void Sdl3RenderTarget::EndDraw() {
	if (command_buf) {
		SDL_SubmitGPUCommandBuffer(command_buf);
		command_buf = nullptr;
	}
}

int Sdl3RenderTarget::GetWidth() const {
	return ui->current_display_mode.width;
}

int Sdl3RenderTarget::GetHeight() const {
	return ui->current_display_mode.height;
}

void Sdl3RenderTarget::Blit(int x, int y, Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	if (!AllocTexture(src)) {
		return;
	}

	SDL_GPUColorTargetInfo color_info{};
	color_info.texture = swapchain_texture;
	color_info.load_op = SDL_GPU_LOADOP_LOAD;
	color_info.store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buf, &color_info, 1, nullptr);

	SDL_BindGPUGraphicsPipeline(render_pass, sprite_pipeline);

	// Bind Vertex buffer
	std::array<SDL_GPUBufferBinding, 1> vertex_buffer_bindings;
	vertex_buffer_bindings[0].buffer = sprite_vertex_buffer;
	vertex_buffer_bindings[0].offset = 0;
	SDL_BindGPUVertexBuffers(render_pass, 0, vertex_buffer_bindings.data(), vertex_buffer_bindings.size());

	// Bind Index Buffer
	SDL_GPUBufferBinding index_buffer_binding{sprite_index_buffer, 0};
	SDL_BindGPUIndexBuffer(render_pass, &index_buffer_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

	// Update the Uniform
	SpriteUniform uniform = {};
	uniform.x = x;
	uniform.y = y;
	uniform.width = src.width();
	uniform.height = src.height();
	uniform.screen_w = GetWidth();
	uniform.screen_h = GetHeight();
	uniform.src_x = src_rect.x;
	uniform.src_y = src_rect.y;
	uniform.src_w = src_rect.width;
	uniform.src_h = src_rect.height;

	SDL_PushGPUVertexUniformData(command_buf, 0, &uniform, sizeof(SpriteUniform));

	// Sprite sampler
	SDL_GPUTextureSamplerBinding sampler_binding{reinterpret_cast<SDL_GPUTexture*>(src.GetGpuTexture()), sprite_sampler};
	SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

	// Issue a draw call
	SDL_DrawGPUIndexedPrimitives(render_pass, 6, 1, 0, 0, 0);

	SDL_EndGPURenderPass(render_pass);
}

/*
Based on TexturedQuad.c and TexturedAnimatedQuad.c from
https://github.com/TheSpydog/SDL_gpu_examples by
Copyright (C) 2024 Caleb Cornett
License: zlib
*/

bool Sdl3RenderTarget::Init() {
	const bool gpu_debug = true;
#ifdef NDEBUG
	gpu_debug = false;
#endif

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
		Output::Debug("Loading vertex shader failed: {}", SDL_GetError());
		return false;
	}

	SDL_GPUShader* sprite_fragment_shader = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, "sprite.frag", 1, 0, 0, 0);
	if (!sprite_fragment_shader) {
		Output::Debug("Loading fragment shader failed: {}", SDL_GetError());
		SDL_ReleaseGPUShader(gpu_device, sprite_vertex_shader);
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
	// Configured to support alpha blending (premultiplied alpha)
	std::array<SDL_GPUColorTargetDescription, 1> color_target_desc;
	color_target_desc[0] = {};
	color_target_desc[0].format = SDL_GetGPUSwapchainTextureFormat(gpu_device, ui->sdl_window);
	color_target_desc[0].blend_state.enable_blend = true;
	color_target_desc[0].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
	color_target_desc[0].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	color_target_desc[0].blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
	color_target_desc[0].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
	color_target_desc[0].blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
	color_target_desc[0].blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

	pipeline_info.target_info.num_color_targets = color_target_desc.size();
	pipeline_info.target_info.color_target_descriptions = color_target_desc.data();

	sprite_pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);
	if (!sprite_pipeline) {
		Output::Debug("SDL_CreateGPUGraphicsPipeline failed: {}", SDL_GetError());
		return false;
	}

	// Shaders can be deleted after creating the pipeline
	SDL_ReleaseGPUShader(gpu_device, sprite_vertex_shader);
	SDL_ReleaseGPUShader(gpu_device, sprite_fragment_shader);

	// Texture Sampler
	SDL_GPUSamplerCreateInfo sampler_create_info{};
	sampler_create_info.min_filter = SDL_GPU_FILTER_NEAREST;
	sampler_create_info.mag_filter = SDL_GPU_FILTER_NEAREST;
	sampler_create_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
	sampler_create_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler_create_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler_create_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
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
		Output::Debug("Failed to create shader!");
		SDL_free(code);
		return nullptr;
	}

	SDL_free(code);

	return shader;
}

bool Sdl3RenderTarget::AllocTexture(Bitmap const& bmp) {
	if (!bmp.GetGpuTexture()) {
		SDL_GPUTextureCreateInfo tex_create_info{};
		tex_create_info.type = SDL_GPU_TEXTURETYPE_2D;
		tex_create_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		tex_create_info.width = bmp.width();
		tex_create_info.height = bmp.height();
		tex_create_info.layer_count_or_depth = 1;
		tex_create_info.num_levels = 1;
		tex_create_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

		SDL_GPUTexture* texture;
		auto texture_sg = lcf::makeScopeGuard([&]() {
			SDL_ReleaseGPUTexture(gpu_device, texture);
		});

		texture = SDL_CreateGPUTexture(gpu_device, &tex_create_info);
		if (!texture) {
			Output::Debug("SDL_CreateGPUTexture for bitmap {} failed: {}", bmp.GetId(), SDL_GetError());
			return false;
		}

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
			if (!gpu_device) {
				// FIXME: Leaks textures on exit (device is destroyed before all textures are freed)
				return;
			}
			SDL_ReleaseGPUTexture(gpu_device, reinterpret_cast<SDL_GPUTexture*>(bmp.GetGpuTexture()));
		});
		texture_sg.Dismiss();
	}

	return true;
}
