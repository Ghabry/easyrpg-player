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

#include "ctr_render_target.h"
#include "bitmap.h"
#include "output.h"

#include <3ds.h>
#include <citro3d.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include "sprite_shader_shbin.h"

// Exported by citro3d: Waits until the GPU processed the command queue.
// Must not be called while a frame is active.
extern "C" void C3Di_RenderQueueWaitDone(void);

/*
 * Memory and synchronisation model
 *
 * Draw commands are recorded between C3D_FrameBegin and C3D_FrameEnd and executed
 * by the GPU after C3D_FrameEnd while the CPU continues with the next frame.
 * Texture uploads (GX DisplayTransfer) inside a frame are queued in order with the
 * draw commands, so they read the bitmap pixels when the GPU reaches them.
 *
 * Therefore:
 *  - Textures and pixel memory are never freed while the GPU may still use them.
 *    They are put on a deferred list that is processed when the GPU is idle.
 *  - When the CPU modifies a bitmap (ClearGpuTexture hook) whose pixels are
 *    accessed by a not yet finished GPU frame, the CPU waits for the GPU.
 *  - C3D_FrameEnd flushes the data cache of the whole linear heap, so CPU writes
 *    to linear memory are visible to the GPU without extra flushes.
 */

struct CtrTexture {
	struct Chunk {
		C3D_Tex tex;
		Rect rect; // area of the bitmap stored in this texture
	};

	std::vector<Chunk> chunks;
	C3D_RenderTarget* target = nullptr;
	// Frame in which the GPU reads or writes the pixels of the bitmap
	uint32_t pixel_access_frame = 0;
	// Bitmap of the texture, valid until released
	const Bitmap* owner = nullptr;
	bool released = false;
};

namespace {
	// Index buffer uses 16 bit indices
	constexpr int max_quads = 16384;
	constexpr int max_texture_size = 1024;
	// Bitmaps smaller than this are uploaded through the CPU
	constexpr int gx_min_width = 64;
	// Keep memory available for render targets, audio and the system
	constexpr u32 vram_reserve = 1536 * 1024;
	constexpr u32 linear_texture_reserve = 3 * 1024 * 1024;
	constexpr u32 linear_bitmap_reserve = 6 * 1024 * 1024;
	constexpr int num_tev_stages = 6;

	/** Upload: linear RGBA8 bitmap -> tiled texture. The output format converts to 16 bit. */
	constexpr u32 GxUploadFlags(GX_TRANSFER_FORMAT out_format) {
		return GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
			GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(out_format) |
			GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
	}

	constexpr u32 gx_download_flags =
		GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
		GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
		GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

	struct Vertex {
		float x, y;
		float u, v;
		u32 color;
	};
	static_assert(sizeof(Vertex) == 20, "Unexpected vertex size");

	// State shared with the pixel allocator, which outlives the render target
	struct GpuState {
		bool in_frame = false;
		bool gpu_busy = false;
		bool defer_free = false;
		uint32_t frame = 1;
		std::vector<void*> pixel_frees;
		std::vector<CtrTexture*> texture_frees;
		// Textures of read-only bitmaps whose CPU pixels can be discarded after the upload finished
		std::vector<CtrTexture*> discard_candidates;
		std::vector<std::pair<void*, u32>> invalidations;
	};
	GpuState gpu;

	CtrRenderTarget* instance = nullptr;

	int NextPow2(int x) {
		int r = 8;
		while (r < x) {
			r <<= 1;
		}
		return r;
	}

	int AlignRows(int h) {
		return (h + 7) & ~7;
	}

	// Texture combiner colors and vertex colors: R in the lowest byte
	constexpr u32 PackColor(int r, int g, int b, int a) {
		return static_cast<u32>(r) | (static_cast<u32>(g) << 8) | (static_cast<u32>(b) << 16) | (static_cast<u32>(a) << 24);
	}

	void WaitGpuIdle() {
		if (gpu.gpu_busy && !gpu.in_frame) {
			C3Di_RenderQueueWaitDone();
			gpu.gpu_busy = false;
		}
	}

	void FreeTextureNow(CtrTexture* tex) {
		if (tex->target) {
			C3D_RenderTargetDelete(tex->target);
		}
		for (auto& chunk: tex->chunks) {
			C3D_TexDelete(&chunk.tex);
		}
		delete tex;
	}

	/** Frees deferred memory. GPU must be idle and no frame active. */
	void ProcessDeferred() {
		assert(!gpu.in_frame && !gpu.gpu_busy);

		// GPU wrote into these bitmaps, drop stale cache lines
		for (auto& inv: gpu.invalidations) {
			GSPGPU_InvalidateDataCache(inv.first, inv.second);
		}
		gpu.invalidations.clear();

		for (auto* tex: gpu.texture_frees) {
			FreeTextureNow(tex);
		}
		gpu.texture_frees.clear();

		for (void* p: gpu.pixel_frees) {
			linearFree(p);
		}
		gpu.pixel_frees.clear();
	}

	/**
	 * Puts bitmaps in linear memory with a power-of-two pitch.
	 * This is the layout the GX transfer engine needs for converting the bitmap
	 * into a GPU texture.
	 */
	class CtrPixelAllocator final : public Bitmap::PixelAllocator {
	public:
		void* Allocate(int width, int height, int bytes, int& pitch) override {
			if (bytes != 4 || width < gx_min_width || width > max_texture_size ||
					height <= 0 || height > max_texture_size) {
				return nullptr;
			}

			const int tex_width = NextPow2(width);
			const size_t size = static_cast<size_t>(tex_width) * 4 * AlignRows(height);
			if (linearSpaceFree() < size + linear_bitmap_reserve) {
				return nullptr;
			}

			void* mem = linearMemAlign(size, 0x80);
			if (!mem) {
				return nullptr;
			}
			memset(mem, 0, size);
			pitch = tex_width * 4;
			return mem;
		}

		void Free(void* pixels) override {
			if (gpu.defer_free) {
				gpu.pixel_frees.push_back(pixels);
			} else {
				linearFree(pixels);
			}
		}
	};
	CtrPixelAllocator pixel_allocator;

	// Morton order of a 8x8 tile, index: y * 8 + x
	constexpr std::array<uint8_t, 64> MakeMortonTable() {
		std::array<uint8_t, 64> t{};
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				t[y * 8 + x] = static_cast<uint8_t>((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
					((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3));
			}
		}
		return t;
	}
	constexpr auto morton_table = MakeMortonTable();

	/*
	 * Texel conversion. Bitmap pixels are u32 0xRRGGBBAA (premultiplied alpha).
	 * RGB565:   RRRRRGGG GGGBBBBB
	 * RGBA5551: RRRRRGGG GGBBBBBA
	 */
	inline u16 ToRgb565(u32 p) {
		return static_cast<u16>(((p >> 16) & 0xF800) | ((p >> 13) & 0x07E0) | ((p >> 11) & 0x001F));
	}

	inline u16 ToRgba5551(u32 p) {
		return static_cast<u16>(((p >> 16) & 0xF800) | ((p >> 13) & 0x07C0) | ((p >> 10) & 0x003E) | ((p >> 7) & 0x0001));
	}

	inline u32 Expand5(u32 v) {
		return (v << 3) | (v >> 2);
	}

	inline u32 Expand6(u32 v) {
		return (v << 2) | (v >> 4);
	}

	inline u32 FromRgb565(u16 t) {
		return (Expand5(t >> 11) << 24) | (Expand6((t >> 5) & 0x3F) << 16) | (Expand5(t & 0x1F) << 8) | 0xFF;
	}

	inline u32 FromRgba5551(u16 t) {
		if ((t & 1) == 0) {
			// Premultiplied alpha: transparent pixels are black
			return 0;
		}
		return (Expand5(t >> 11) << 24) | (Expand5((t >> 6) & 0x1F) << 16) | (Expand5((t >> 1) & 0x1F) << 8) | 0xFF;
	}

	/** Visits all pixels of rect in tiled order: fn(texel index, bitmap row pointer, x) */
	template <typename Fn>
	void ForEachTexel(const C3D_Tex& tex, const Rect& rect, Fn&& fn) {
		const int tiles_x = tex.width / 8;
		for (int y = 0; y < rect.height; ++y) {
			const int tile_row = (y >> 3) * tiles_x * 64;
			const uint8_t* mt = &morton_table[(y & 7) * 8];
			for (int x = 0; x < rect.width; ++x) {
				fn(tile_row + ((x >> 3) << 6) + mt[x & 7], y, x);
			}
		}
	}

	/** Converts a part of a bitmap into the tiled texture format on the CPU */
	void SwizzleToTexture(Bitmap const& bmp, const Rect& rect, C3D_Tex& tex) {
		const auto* src = reinterpret_cast<const u32*>(bmp.pixels()) + rect.y * (bmp.pitch() / 4) + rect.x;
		const int stride = bmp.pitch() / 4;

		switch (tex.fmt) {
			case GPU_RGB565: {
				auto* dst = static_cast<u16*>(tex.data);
				ForEachTexel(tex, rect, [&](int i, int y, int x) { dst[i] = ToRgb565(src[y * stride + x]); });
				break;
			}
			case GPU_RGBA5551: {
				auto* dst = static_cast<u16*>(tex.data);
				ForEachTexel(tex, rect, [&](int i, int y, int x) { dst[i] = ToRgba5551(src[y * stride + x]); });
				break;
			}
			default: {
				auto* dst = static_cast<u32*>(tex.data);
				ForEachTexel(tex, rect, [&](int i, int y, int x) { dst[i] = src[y * stride + x]; });
				break;
			}
		}
	}

	/** Converts a tiled texture back into the linear bitmap layout on the CPU */
	void UnswizzleFromTexture(const C3D_Tex& tex, const Rect& rect, void* pixels, int pitch) {
		const int stride = pitch / 4;
		auto* dst = static_cast<u32*>(pixels) + rect.y * stride + rect.x;

		switch (tex.fmt) {
			case GPU_RGB565: {
				const auto* src = static_cast<const u16*>(tex.data);
				ForEachTexel(tex, rect, [&](int i, int y, int x) { dst[y * stride + x] = FromRgb565(src[i]); });
				break;
			}
			case GPU_RGBA5551: {
				const auto* src = static_cast<const u16*>(tex.data);
				ForEachTexel(tex, rect, [&](int i, int y, int x) { dst[y * stride + x] = FromRgba5551(src[i]); });
				break;
			}
			default: {
				const auto* src = static_cast<const u32*>(tex.data);
				ForEachTexel(tex, rect, [&](int i, int y, int x) { dst[y * stride + x] = src[i]; });
				break;
			}
		}
	}

	/**
	 * Texture format for a bitmap.
	 * Read-only bitmaps (game assets) with opaque or 1 bit alpha are stored in 16 bit.
	 * Their pixels never change and their opacity is known from loading.
	 */
	GPU_TEXCOLOR ChooseTextureFormat(Bitmap const& bmp) {
		if (!bmp.IsReadOnly()) {
			return GPU_RGBA8;
		}

		if (!bmp.GetTransparent()) {
			return GPU_RGB565;
		}

		switch (bmp.GetImageOpacity()) {
			case ImageOpacity::Opaque:
				return GPU_RGB565;
			case ImageOpacity::Alpha_1Bit:
			case ImageOpacity::Transparent:
				return GPU_RGBA5551;
			default:
				return GPU_RGBA8;
		}
	}

	GX_TRANSFER_FORMAT ToTransferFormat(GPU_TEXCOLOR fmt) {
		switch (fmt) {
			case GPU_RGB565:
				return GX_TRANSFER_FMT_RGB565;
			case GPU_RGBA5551:
				return GX_TRANSFER_FMT_RGB5A1;
			default:
				return GX_TRANSFER_FMT_RGBA8;
		}
	}

	int BytesPerTexel(GPU_TEXCOLOR fmt) {
		return (fmt == GPU_RGB565 || fmt == GPU_RGBA5551) ? 2 : 4;
	}

	/**
	 * Restores discarded pixels of a read-only bitmap from its texture.
	 * Works at any time: Textures of read-only bitmaps are immutable and
	 * pixels are only discarded after the upload finished.
	 */
	void RestoreHook(Bitmap const& bmp, void* pixels, int pitch) {
		auto* tex = static_cast<CtrTexture*>(bmp.GetGpuTexture());
		if (!tex) {
			return;
		}

		for (auto& chunk: tex->chunks) {
			// Written by the GPU, drop stale cache lines
			GSPGPU_InvalidateDataCache(chunk.tex.data, chunk.tex.size);
			UnswizzleFromTexture(chunk.tex, chunk.rect, pixels, pitch);
		}
	}

	/** Discards the CPU pixels of read-only bitmaps. GPU must be idle (uploads finished). */
	void ProcessDiscards() {
		assert(!gpu.gpu_busy);

		for (auto* tex: gpu.discard_candidates) {
			if (!tex->released && tex->owner) {
				tex->owner->DiscardPixels();
			}
		}
		gpu.discard_candidates.clear();
	}

	bool InitTexture(C3D_Tex& tex, int width, int height, GPU_TEXCOLOR format, bool try_vram) {
		C3D_TexInitParams params{};
		params.width = static_cast<u16>(width);
		params.height = static_cast<u16>(height);
		params.maxLevel = 0;
		params.format = format;
		params.type = GPU_TEX_2D;

		const u32 size = static_cast<u32>(width) * height * BytesPerTexel(format);

		if (try_vram && vramSpaceFree() > size + vram_reserve) {
			params.onVram = true;
			if (C3D_TexInitWithParams(&tex, nullptr, params)) {
				return true;
			}
		}

		if (linearSpaceFree() < size + linear_texture_reserve) {
			return false;
		}

		params.onVram = false;
		return C3D_TexInitWithParams(&tex, nullptr, params);
	}

	void ReleaseHook(Bitmap const& bmp) {
		auto* tex = static_cast<CtrTexture*>(bmp.GetGpuTexture());
		if (tex && instance) {
			instance->ReleaseTexture(tex);
		}
		// Without instance the texture was already freed by the destructor
	}

	void ToneChannel(int t, uint8_t& mul, uint8_t& add) {
		// Hard light: 2 * base * tone (tone < 0.5) or 1 - 2 * (1 - base) * (1 - tone)
		// Both are linear in base: base * mul + add
		if (t < 128) {
			mul = static_cast<uint8_t>(std::min(255, t * 2));
			add = 0;
		} else {
			mul = static_cast<uint8_t>(std::min(255, (255 - t) * 2));
			add = static_cast<uint8_t>(std::min(255, t * 2 - 255));
		}
	}
}

enum TevFlags : uint8_t {
	Tev_Textured = 1,
	Tev_OpaqueSrc = 2,
	Tev_Saturation = 4,
	Tev_Tone = 8,
	Tev_Flash = 16
};

void CtrRenderTarget::InstallPixelAllocator() {
	Bitmap::SetPixelAllocator(&pixel_allocator);
}

std::unique_ptr<CtrRenderTarget> CtrRenderTarget::Create(int width, int height) {
	std::unique_ptr<CtrRenderTarget> rt(new CtrRenderTarget(width, height));
	if (!rt->Init()) {
		Output::Debug("3DS: Could not initialize the hardware renderer");
		return nullptr;
	}
	return rt;
}

CtrRenderTarget::CtrRenderTarget(int width, int height) :
	screen_width(width), screen_height(height), target_width(width), target_height(height) {
	is_hardware_accelerated = true;
}

bool CtrRenderTarget::Init() {
	if (screen_width <= 0 || screen_width > max_texture_size || screen_height <= 0 || screen_height > max_texture_size) {
		return false;
	}

	shader_dvlb = DVLB_ParseFile(reinterpret_cast<u32*>(const_cast<u8*>(sprite_shader_shbin)), sprite_shader_shbin_size);
	if (!shader_dvlb) {
		return false;
	}
	shaderProgramInit(&shader_program);
	shaderProgramSetVsh(&shader_program, &shader_dvlb->DVLE[0]);
	uloc_projection = shaderInstanceGetUniformLocation(shader_program.vertexShader, "projection");

	AttrInfo_Init(&attr_info);
	AttrInfo_AddLoader(&attr_info, 0, GPU_FLOAT, 2); // position
	AttrInfo_AddLoader(&attr_info, 1, GPU_FLOAT, 2); // texcoord
	AttrInfo_AddLoader(&attr_info, 2, GPU_UNSIGNED_BYTE, 4); // color

	vertex_buffer = linearAlloc(sizeof(Vertex) * 4 * max_quads);
	index_buffer = static_cast<uint16_t*>(linearAlloc(sizeof(uint16_t) * 6 * max_quads));
	if (!vertex_buffer || !index_buffer) {
		return false;
	}

	for (int i = 0; i < max_quads; ++i) {
		auto base = static_cast<uint16_t>(i * 4);
		uint16_t* idx = &index_buffer[i * 6];
		// 0: top left, 1: bottom left, 2: top right, 3: bottom right
		idx[0] = base; idx[1] = base + 1; idx[2] = base + 2;
		idx[3] = base + 1; idx[4] = base + 3; idx[5] = base + 2;
	}

	BufInfo_Init(&buf_info);
	BufInfo_Add(&buf_info, vertex_buffer, sizeof(Vertex), 3, 0x210);

	// The game screen is rendered into a texture that is presented by the UI
	if (!C3D_TexInitVRAM(&screen_tex, NextPow2(screen_width), NextPow2(screen_height), GPU_RGBA8)) {
		return false;
	}
	C3D_TexSetFilter(&screen_tex, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&screen_tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
	screen_tex.border = 0xFF000000;

	screen_target = C3D_RenderTargetCreateFromTex(&screen_tex, GPU_TEXFACE_2D, 0, -1);
	if (!screen_target) {
		return false;
	}

	instance = this;
	gpu.defer_free = true;

	Output::Debug("3DS: Hardware renderer initialized ({}x{})", screen_width, screen_height);

	return true;
}

CtrRenderTarget::~CtrRenderTarget() {
	if (gpu.in_frame) {
		EndFrame();
	}
	WaitGpuIdle();
	gpu.discard_candidates.clear();
	ProcessDeferred();

	// Textures are the only copy of discarded pixels.
	// Copy the set: Restoring does not release textures, but be safe.
	std::vector<CtrTexture*> textures(live_textures.begin(), live_textures.end());
	for (auto* tex: textures) {
		if (tex->owner) {
			tex->owner->EnsurePixels();
		}
	}

	for (auto* tex: live_textures) {
		FreeTextureNow(tex);
	}
	live_textures.clear();

	instance = nullptr;
	gpu.defer_free = false;

	if (screen_target) {
		C3D_RenderTargetDelete(screen_target);
	}
	if (screen_tex.data) {
		C3D_TexDelete(&screen_tex);
	}
	if (vertex_buffer) {
		linearFree(vertex_buffer);
	}
	if (index_buffer) {
		linearFree(index_buffer);
	}
	if (shader_dvlb) {
		shaderProgramFree(&shader_program);
		DVLB_Free(shader_dvlb);
	}
}

void CtrRenderTarget::SetEnabled(bool enabled) {
	if (this->enabled == enabled) {
		return;
	}
	this->enabled = enabled;

	if (gpu.in_frame) {
		EndFrame();
	}
	WaitGpuIdle();
	if (enabled) {
		ProcessDiscards();
	} else {
		gpu.discard_candidates.clear();
	}
	ProcessDeferred();
	gpu.defer_free = enabled;
}

bool CtrRenderTarget::IsInFrame() const {
	return gpu.in_frame;
}

void CtrRenderTarget::BeginFrame(bool sync_draw) {
	// Free everything the GPU used in the previous frame.
	// Must happen outside of a frame (render targets cannot be deleted in a frame).
	// C3D_FrameBegin waits for the GPU anyway, so this does not cost extra time.
	WaitGpuIdle();
	// Before ProcessDeferred: Released textures in the candidate list are still allocated
	ProcessDiscards();
	ProcessDeferred();

	C3D_FrameBegin(sync_draw ? C3D_FRAME_SYNCDRAW : 0);
	gpu.in_frame = true;
	++gpu.frame;

	ResetBatch();
	BindState();
}

void CtrRenderTarget::EndFrame() {
	if (!gpu.in_frame) {
		return;
	}
	Flush();
	C3D_FrameEnd(0);
	gpu.in_frame = false;
	gpu.gpu_busy = true;
	state_valid = false;
}

void CtrRenderTarget::BindState() {
	C3D_BindProgram(&shader_program);
	C3D_SetAttrInfo(&attr_info);
	C3D_SetBufInfo(&buf_info);
	C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
	C3D_CullFace(GPU_CULL_NONE);
	C3D_TexEnvBufUpdate(C3D_Both, 0);
	state_valid = false;
}

void CtrRenderTarget::SetProjection(int tex_width, int tex_height) {
	// Same convention as citro2d for texture targets: Origin is the top-left
	C3D_Mtx projection;
	Mtx_Ortho(&projection, 0.0f, static_cast<float>(tex_width), static_cast<float>(tex_height), 0.0f, 1.0f, -1.0f, true);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uloc_projection, &projection);
}

bool CtrRenderTarget::BeginDrawScreen() {
	if (!enabled || gpu.in_frame) {
		return false;
	}

	BeginFrame(true);

	C3D_FrameDrawOn(screen_target);
	SetProjection(screen_tex.width, screen_tex.height);
	target_width = screen_width;
	target_height = screen_height;

	return true;
}

void CtrRenderTarget::EndDrawScreen() {
	// The frame is finished by the UI (EndFrame) after presenting the screen texture
	Flush();
}

bool CtrRenderTarget::BeginDrawTexture(Bitmap& target) {
	if (!enabled || bitmap_target) {
		return false;
	}

	if (target.width() > max_texture_size || target.height() > max_texture_size) {
		return false;
	}

	auto* tex = static_cast<CtrTexture*>(target.GetGpuTexture());
	if (tex && !tex->target) {
		// Normal texture, replace it with a render texture
		target.ClearGpuTexture();
		tex = nullptr;
	}

	texture_pass_standalone = !gpu.in_frame;
	if (texture_pass_standalone) {
		BeginFrame(false);
	} else {
		Flush();
	}

	if (!tex) {
		tex = CreateRenderTexture(target);
		if (!tex) {
			if (texture_pass_standalone) {
				EndFrame();
			}
			return false;
		}
	}

	C3D_FrameDrawOn(tex->target);
	SetProjection(tex->chunks[0].tex.width, tex->chunks[0].tex.height);
	target_width = target.width();
	target_height = target.height();
	bitmap_target = &target;

	Clear();

	return true;
}

void CtrRenderTarget::EndDrawTexture() {
	if (!bitmap_target) {
		return;
	}

	Flush();

	auto* tex = static_cast<CtrTexture*>(bitmap_target->GetGpuTexture());
	if (tex && bitmap_target->IsCustomAllocated()) {
		// Copy back into the bitmap (e.g. used by the Mosaic transition)
		auto& t = tex->chunks[0].tex;
		const int rows = AlignRows(bitmap_target->height());
		C3D_SyncDisplayTransfer(static_cast<u32*>(t.data), GX_BUFFER_DIM(t.width, rows),
			static_cast<u32*>(bitmap_target->pixels()), GX_BUFFER_DIM(t.width, rows), gx_download_flags);
		gpu.invalidations.emplace_back(bitmap_target->pixels(), static_cast<u32>(bitmap_target->pitch() * rows));
		tex->pixel_access_frame = gpu.frame;
	}

	bitmap_target = nullptr;

	if (texture_pass_standalone) {
		EndFrame();
	} else {
		C3D_FrameDrawOn(screen_target);
		SetProjection(screen_tex.width, screen_tex.height);
	}

	target_width = screen_width;
	target_height = screen_height;
}

int CtrRenderTarget::GetWidth() const {
	return target_width;
}

int CtrRenderTarget::GetHeight() const {
	return target_height;
}

CtrTexture* CtrRenderTarget::GetTexture(Bitmap const& bmp) {
	auto* tex = static_cast<CtrTexture*>(bmp.GetGpuTexture());
	if (tex) {
		return tex;
	}

	const int w = bmp.width();
	const int h = bmp.height();
	if (w <= 0 || h <= 0 || bmp.bpp() != 4) {
		return nullptr;
	}

	auto new_tex = std::make_unique<CtrTexture>();
	for (int cy = 0; cy < h; cy += max_texture_size) {
		for (int cx = 0; cx < w; cx += max_texture_size) {
			CtrTexture::Chunk chunk{};
			chunk.rect = Rect(cx, cy, std::min(max_texture_size, w - cx), std::min(max_texture_size, h - cy));
			new_tex->chunks.push_back(chunk);
		}
	}

	// Fast path: The GX transfer engine converts the bitmap
	const bool use_gx = new_tex->chunks.size() == 1 && bmp.IsCustomAllocated() && bmp.pitch() == NextPow2(w) * 4;
	const GPU_TEXCOLOR format = ChooseTextureFormat(bmp);

	for (size_t i = 0; i < new_tex->chunks.size(); ++i) {
		auto& chunk = new_tex->chunks[i];
		if (!InitTexture(chunk.tex, NextPow2(chunk.rect.width), NextPow2(chunk.rect.height), format, use_gx)) {
			static bool warned = false;
			if (!warned) {
				Output::Debug("3DS: Out of texture memory ({}x{}, {})", w, h, bmp.GetId());
				warned = true;
			}
			for (size_t j = 0; j < i; ++j) {
				C3D_TexDelete(&new_tex->chunks[j].tex);
			}
			return nullptr;
		}

		C3D_TexSetFilter(&chunk.tex, GPU_NEAREST, GPU_NEAREST);
		C3D_TexSetWrap(&chunk.tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

		if (use_gx) {
			// Queued in the frame: Executed by the GPU before the draws that use the texture
			const int rows = AlignRows(h);
			C3D_SyncDisplayTransfer(static_cast<u32*>(const_cast<void*>(bmp.pixels())), GX_BUFFER_DIM(chunk.tex.width, rows),
				static_cast<u32*>(chunk.tex.data), GX_BUFFER_DIM(chunk.tex.width, rows), GxUploadFlags(ToTransferFormat(format)));
			new_tex->pixel_access_frame = gpu.frame;
		} else {
			SwizzleToTexture(bmp, chunk.rect, chunk.tex);
		}
	}

	tex = new_tex.release();
	tex->owner = &bmp;
	live_textures.insert(tex);

	if (bmp.IsReadOnly()) {
		// Read-only bitmaps are immutable: The texture can be the only copy of the pixels
		bmp.SetGpuTexture(tex, ReleaseHook, RestoreHook);
		gpu.discard_candidates.push_back(tex);
	} else {
		bmp.SetGpuTexture(tex, ReleaseHook);
	}
	return tex;
}

CtrTexture* CtrRenderTarget::CreateRenderTexture(Bitmap& bmp) {
	auto tex = std::make_unique<CtrTexture>();
	CtrTexture::Chunk chunk{};
	chunk.rect = bmp.GetRect();

	// Render targets must be in VRAM
	if (!C3D_TexInitVRAM(&chunk.tex, NextPow2(bmp.width()), NextPow2(bmp.height()), GPU_RGBA8)) {
		Output::Debug("3DS: Out of VRAM for render texture ({}x{})", bmp.width(), bmp.height());
		return nullptr;
	}
	C3D_TexSetFilter(&chunk.tex, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&chunk.tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	tex->chunks.push_back(chunk);

	tex->target = C3D_RenderTargetCreateFromTex(&tex->chunks[0].tex, GPU_TEXFACE_2D, 0, -1);
	if (!tex->target) {
		C3D_TexDelete(&tex->chunks[0].tex);
		return nullptr;
	}

	auto* t = tex.release();
	t->owner = &bmp;
	live_textures.insert(t);
	bmp.SetGpuTexture(t, ReleaseHook);
	return t;
}

void CtrRenderTarget::ReleaseTexture(CtrTexture* tex) {
	if (tex->pixel_access_frame == gpu.frame) {
		// The bitmap is about to be modified or freed but the GPU might still
		// read (upload) or write (readback) the pixels.
		// Only possible outside of a frame. Inside a frame the operation is
		// not executed yet and will see the new pixels (inaccurate but harmless).
		WaitGpuIdle();
	}

	live_textures.erase(tex);
	tex->released = true;
	tex->owner = nullptr;

	auto& candidates = gpu.discard_candidates;
	candidates.erase(std::remove(candidates.begin(), candidates.end(), tex), candidates.end());

	if (gpu.in_frame || gpu.gpu_busy) {
		gpu.texture_frees.push_back(tex);
	} else {
		FreeTextureNow(tex);
	}
}

void CtrRenderTarget::ResetBatch() {
	batch_start = 0;
	batch_count = 0;
	state_valid = false;
}

void CtrRenderTarget::Flush() {
	if (batch_count == 0) {
		return;
	}

	C3D_DrawElements(GPU_TRIANGLES, batch_count * 6, C3D_UNSIGNED_SHORT, &index_buffer[batch_start * 6]);
	batch_start += batch_count;
	batch_count = 0;
}

void CtrRenderTarget::SetState(const BatchState& state) {
	if (state_valid && state == current_state) {
		return;
	}

	Flush();

	if (!state_valid || state.tex != current_state.tex) {
		if (state.tex) {
			C3D_TexBind(0, state.tex);
		}
	}

	if (!state_valid || state.blend != current_state.blend) {
		ApplyBlend(state.blend);
	}

	if (!state_valid || !(state.tev == current_state.tev)) {
		ApplyTev(state.tev);
	}

	current_state = state;
	state_valid = true;
}

void CtrRenderTarget::ApplyBlend(Blend blend) {
	// Colors are premultiplied by alpha
	switch (blend) {
		case Blend::Replace:
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
			break;
		case Blend::Additive:
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ONE, GPU_ONE);
			break;
		case Blend::Multiply:
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_DST_COLOR, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
			break;
		case Blend::Screen:
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_COLOR, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
			break;
		case Blend::Darken:
			C3D_AlphaBlend(GPU_BLEND_MIN, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
			break;
		case Blend::Lighten:
			C3D_AlphaBlend(GPU_BLEND_MAX, GPU_BLEND_ADD, GPU_ONE, GPU_ONE, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
			break;
		case Blend::Normal:
		case Blend::Invalid:
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
			break;
	}

	// Discard fully transparent pixels: Prevents that the tone offset
	// and Darken (MIN) affect transparent areas. Not done for Replace (copies alpha).
	C3D_AlphaTest(blend != Blend::Replace, GPU_GREATER, 0);
}

void CtrRenderTarget::ApplyTev(const TevKey& key) {
	/*
	 * Inputs:
	 *  - TEXTURE0: Texture color (premultiplied alpha)
	 *  - PRIMARY_COLOR: Vertex color: rgb = tone offset * opacity, a = opacity
	 *  - CONSTANT: Per stage constant
	 *
	 * Stages (max 6):
	 *  Saturation (gray < 128):
	 *   1. bias:   T * 0.5 + 0.5
	 *   2. luma:   DOT3(prev, 0.5 + w / 2) = 0.299 r + 0.587 g + 0.114 b
	 *   3. lerp:   T * sat + luma * (1 - sat)
	 *   4. opacity: prev * opacity
	 *  Otherwise:
	 *   1. opacity: T * opacity
	 *  Tone: prev * tone_mul + tone_add * opacity (hard light is linear for a constant tone)
	 *  Flash: lerp(prev, flash, strength * alpha)
	 */
	int stage = 0;
	auto next = [&]() {
		C3D_TexEnv* env = C3D_GetTexEnv(stage++);
		C3D_TexEnvInit(env);
		return env;
	};

	const bool opaque = (key.flags & Tev_OpaqueSrc) != 0;

	// Final alpha: texture alpha * opacity (opaque images: opacity)
	auto set_final_alpha = [&](C3D_TexEnv* env) {
		if (opaque) {
			C3D_TexEnvSrc(env, C3D_Alpha, GPU_PRIMARY_COLOR);
			C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
		} else {
			C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
			C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
		}
	};

	if ((key.flags & Tev_Textured) == 0) {
		C3D_TexEnv* env = next();
		C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
		C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
	} else {
		C3D_TexEnv* last = nullptr;

		if (key.flags & Tev_Saturation) {
			C3D_TexEnv* env = next();
			C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_CONSTANT, GPU_CONSTANT);
			C3D_TexEnvFunc(env, C3D_RGB, GPU_MULTIPLY_ADD);
			C3D_TexEnvColor(env, PackColor(128, 128, 128, 0));
			set_final_alpha(env);

			env = next();
			C3D_TexEnvSrc(env, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT);
			C3D_TexEnvFunc(env, C3D_RGB, GPU_DOT3_RGB);
			C3D_TexEnvColor(env, PackColor(166, 202, 142, 0));

			env = next();
			C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, GPU_CONSTANT);
			C3D_TexEnvFunc(env, C3D_RGB, GPU_INTERPOLATE);
			C3D_TexEnvColor(env, PackColor(key.saturation, key.saturation, key.saturation, 0));

			env = next();
			C3D_TexEnvSrc(env, C3D_RGB, GPU_PREVIOUS, GPU_PRIMARY_COLOR);
			C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
			C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
			last = env;
		} else {
			C3D_TexEnv* env = next();
			C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
			C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
			C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
			set_final_alpha(env);
			last = env;
		}

		if (key.flags & Tev_Tone) {
			C3D_TexEnv* env = next();
			C3D_TexEnvSrc(env, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT, GPU_PRIMARY_COLOR);
			C3D_TexEnvFunc(env, C3D_RGB, GPU_MULTIPLY_ADD);
			C3D_TexEnvColor(env, key.tone_mul);
			last = env;
		}

		if (key.flags & Tev_Flash) {
			const u32 strength = key.flash >> 24;

			// The previous stage provides strength * alpha as interpolation factor
			if (opaque) {
				C3D_TexEnvSrc(last, C3D_Alpha, GPU_CONSTANT);
				C3D_TexEnvFunc(last, C3D_Alpha, GPU_REPLACE);
			} else {
				C3D_TexEnvSrc(last, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT);
				C3D_TexEnvFunc(last, C3D_Alpha, GPU_MODULATE);
			}
			C3D_TexEnvOpAlpha(last, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
			C3D_TexEnvColor(last, (last->color & 0x00FFFFFF) | (strength << 24));

			C3D_TexEnv* env = next();
			C3D_TexEnvSrc(env, C3D_RGB, GPU_CONSTANT, GPU_PREVIOUS, GPU_PREVIOUS);
			C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
			C3D_TexEnvFunc(env, C3D_RGB, GPU_INTERPOLATE);
			C3D_TexEnvColor(env, key.flash | 0xFF000000);
			set_final_alpha(env);
		}
	}

	// Pass through for the remaining stages
	while (stage < num_tev_stages) {
		next();
	}
}

void CtrRenderTarget::AddQuad(const float (&pos)[4][2], const float (&uv)[4][2], uint32_t color) {
	// Skip quads outside of the target
	const float min_x = std::min({pos[0][0], pos[1][0], pos[2][0], pos[3][0]});
	const float max_x = std::max({pos[0][0], pos[1][0], pos[2][0], pos[3][0]});
	const float min_y = std::min({pos[0][1], pos[1][1], pos[2][1], pos[3][1]});
	const float max_y = std::max({pos[0][1], pos[1][1], pos[2][1], pos[3][1]});
	if (max_x <= 0.0f || max_y <= 0.0f || min_x >= target_width || min_y >= target_height) {
		return;
	}

	if (batch_start + batch_count >= max_quads) {
		if (!overflow_warned) {
			Output::Debug("3DS: Vertex buffer full, dropping draws");
			overflow_warned = true;
		}
		return;
	}

	auto* v = static_cast<Vertex*>(vertex_buffer) + (batch_start + batch_count) * 4;
	for (int i = 0; i < 4; ++i) {
		v[i] = { pos[i][0], pos[i][1], uv[i][0], uv[i][1], color };
	}
	++batch_count;
}

void CtrRenderTarget::DrawImage(Bitmap const& src, const FRect& src_rect, const Affine& xform,
		Opacity const& opacity, bool flip_x, bool flip_y, const Effects& fx, float clip_y0, float clip_y1) {
	if (opacity.IsTransparent() || src_rect.w <= 0.0f || src_rect.h <= 0.0f) {
		return;
	}

	auto* tex = GetTexture(src);
	if (!tex) {
		return;
	}

	BatchState state;
	switch (fx.blend) {
		case Bitmap::BlendMode::NormalWithoutAlpha:
			state.blend = Blend::Replace;
			break;
		case Bitmap::BlendMode::Additive:
			state.blend = Blend::Additive;
			break;
		case Bitmap::BlendMode::Multiply:
			state.blend = Blend::Multiply;
			break;
		case Bitmap::BlendMode::Screen:
			state.blend = Blend::Screen;
			break;
		case Bitmap::BlendMode::Darken:
			state.blend = Blend::Darken;
			break;
		case Bitmap::BlendMode::Lighten:
			state.blend = Blend::Lighten;
			break;
		default:
			// Unsupported blend modes are drawn as Normal
			state.blend = Blend::Normal;
			break;
	}

	auto& tev = state.tev;
	tev.flags = Tev_Textured;
	if (!src.GetTransparent()) {
		tev.flags |= Tev_OpaqueSrc;
	}

	// Oversaturation (gray > 128) is not supported
	if (fx.tone.gray < 128) {
		tev.flags |= Tev_Saturation;
		tev.saturation = static_cast<uint8_t>((fx.tone.gray * 255 + 64) / 128);
	}

	uint8_t tone_add_rgb[3] = {};
	if (fx.tone.red != 128 || fx.tone.green != 128 || fx.tone.blue != 128) {
		uint8_t mul[3];
		ToneChannel(fx.tone.red, mul[0], tone_add_rgb[0]);
		ToneChannel(fx.tone.green, mul[1], tone_add_rgb[1]);
		ToneChannel(fx.tone.blue, mul[2], tone_add_rgb[2]);
		tev.flags |= Tev_Tone;
		tev.tone_mul = PackColor(mul[0], mul[1], mul[2], 0);
	}

	if (fx.flash.alpha > 0) {
		tev.flags |= Tev_Flash;
		tev.flash = PackColor(fx.flash.red, fx.flash.green, fx.flash.blue, fx.flash.alpha);
	}

	// Local space rows with top and bottom opacity
	struct Part {
		float y0, y1;
		int opacity;
	};
	std::array<Part, 2> parts;
	int num_parts = 0;
	if (opacity.IsSplit()) {
		const float split_y = std::clamp(src_rect.h - static_cast<float>(opacity.split), 0.0f, src_rect.h);
		parts[num_parts++] = { 0.0f, split_y, opacity.top };
		parts[num_parts++] = { split_y, src_rect.h, opacity.bottom };
	} else {
		parts[num_parts++] = { 0.0f, src_rect.h, opacity.top };
	}

	for (int p = 0; p < num_parts; ++p) {
		const auto& part = parts[p];
		const int alpha = std::clamp(part.opacity, 0, 255);
		const float ly0 = std::max(part.y0, clip_y0);
		const float ly1 = std::min(part.y1, clip_y1);
		if (alpha == 0 || ly1 <= ly0) {
			continue;
		}

		const u32 color = PackColor(tone_add_rgb[0] * alpha / 255, tone_add_rgb[1] * alpha / 255,
			tone_add_rgb[2] * alpha / 255, alpha);

		// Source area of this part
		const float sx0 = src_rect.x;
		const float sx1 = src_rect.x + src_rect.w;
		const float sy0 = flip_y ? src_rect.y + src_rect.h - ly1 : src_rect.y + ly0;
		const float sy1 = flip_y ? src_rect.y + src_rect.h - ly0 : src_rect.y + ly1;

		for (auto& chunk: tex->chunks) {
			const auto& cr = chunk.rect;
			const float ix0 = std::max(sx0, static_cast<float>(cr.x));
			const float ix1 = std::min(sx1, static_cast<float>(cr.x + cr.width));
			const float iy0 = std::max(sy0, static_cast<float>(cr.y));
			const float iy1 = std::min(sy1, static_cast<float>(cr.y + cr.height));
			if (ix1 <= ix0 || iy1 <= iy0) {
				continue;
			}

			// Intersection in local space
			const float px0 = flip_x ? sx1 - ix1 : ix0 - sx0;
			const float px1 = flip_x ? sx1 - ix0 : ix1 - sx0;
			const float py0 = flip_y ? (src_rect.y + src_rect.h) - iy1 : iy0 - src_rect.y;
			const float py1 = flip_y ? (src_rect.y + src_rect.h) - iy0 : iy1 - src_rect.y;

			// Source coordinates at the local corners
			const float u_left = flip_x ? ix1 : ix0;
			const float u_right = flip_x ? ix0 : ix1;
			const float v_top = flip_y ? iy1 : iy0;
			const float v_bottom = flip_y ? iy0 : iy1;

			const float tw = static_cast<float>(chunk.tex.width);
			const float th = static_cast<float>(chunk.tex.height);
			auto tu = [&](float sx) { return (sx - cr.x) / tw; };
			// Row 0 of the image is at v = 1
			auto tv = [&](float sy) { return 1.0f - (sy - cr.y) / th; };

			const float local[4][2] = { { px0, py0 }, { px0, py1 }, { px1, py0 }, { px1, py1 } };
			float pos[4][2];
			for (int i = 0; i < 4; ++i) {
				pos[i][0] = xform.a * local[i][0] + xform.b * local[i][1] + xform.tx;
				pos[i][1] = xform.c * local[i][0] + xform.d * local[i][1] + xform.ty;
			}
			const float uv[4][2] = {
				{ tu(u_left), tv(v_top) },
				{ tu(u_left), tv(v_bottom) },
				{ tu(u_right), tv(v_top) },
				{ tu(u_right), tv(v_bottom) }
			};

			state.tex = &chunk.tex;
			SetState(state);
			AddQuad(pos, uv, color);
		}
	}
}

void CtrRenderTarget::DrawWaver(int x, int y, double zoom_x, double zoom_y, Bitmap const& src, Rect const& src_rect,
		int depth, double phase, Opacity const& opacity, bool flip_x, bool flip_y, const Effects& fx) {
	// Same algorithm as Bitmap::WaverBlit: Every row is shifted horizontally
	const double zx = (zoom_x == 0.0) ? 1.0 : zoom_x;
	const double zy = (zoom_y == 0.0) ? 1.0 : zoom_y;

	const int height = static_cast<int>(std::floor(src_rect.height * zy));
	const int yclip = y < 0 ? -y : 0;
	const int yend = std::min(height, target_height - y);

	const FRect fsrc = { static_cast<float>(src_rect.x), static_cast<float>(src_rect.y),
		static_cast<float>(src_rect.width), static_cast<float>(src_rect.height) };

	auto row_offset = [&](int i) {
		// See Bitmap::WaverBlit for the RPG_RT compatible (i - yclip)
		const double sy = (i - yclip) * (2 * M_PI) / (32.0 * zy);
		return static_cast<int>(2 * zx * depth * std::sin(phase + sy));
	};

	int i = yclip;
	while (i < yend) {
		// Merge rows with the same offset into one quad
		const int offset = row_offset(i);
		int j = i + 1;
		while (j < yend && row_offset(j) == offset) {
			++j;
		}

		Affine xform;
		xform.a = static_cast<float>(zx);
		xform.d = static_cast<float>(zy);
		xform.tx = static_cast<float>(x + offset);
		xform.ty = static_cast<float>(y);

		DrawImage(src, fsrc, xform, opacity, flip_x, flip_y, fx,
			static_cast<float>(i / zy), static_cast<float>(j / zy));

		i = j;
	}
}

void CtrRenderTarget::DrawFill(Rect const& dst_rect, const Color& color, Blend blend) {
	if (dst_rect.IsEmpty()) {
		return;
	}

	BatchState state;
	state.blend = blend;
	state.tev.flags = 0;
	// Keep the texture binding, it is unused by the combiners
	state.tex = state_valid ? current_state.tex : nullptr;
	SetState(state);

	const int a = color.alpha;
	const u32 c = PackColor(color.red * a / 255, color.green * a / 255, color.blue * a / 255, a);

	const float x0 = static_cast<float>(dst_rect.x);
	const float y0 = static_cast<float>(dst_rect.y);
	const float x1 = static_cast<float>(dst_rect.x + dst_rect.width);
	const float y1 = static_cast<float>(dst_rect.y + dst_rect.height);
	const float pos[4][2] = { { x0, y0 }, { x0, y1 }, { x1, y0 }, { x1, y1 } };
	const float uv[4][2] = {};
	AddQuad(pos, uv, c);
}

void CtrRenderTarget::Blit(int x, int y, Bitmap const& src, Rect const& src_rect_,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	auto src_rect = src_rect_;
	src_rect.Adjust(src.GetRect());
	if (src_rect.IsEmpty()) {
		return;
	}

	Affine xform;
	xform.tx = static_cast<float>(x);
	xform.ty = static_cast<float>(y);

	Effects fx;
	fx.blend = blend_mode;
	DrawImage(src, { static_cast<float>(src_rect.x), static_cast<float>(src_rect.y),
		static_cast<float>(src_rect.width), static_cast<float>(src_rect.height) },
		xform, opacity, false, false, fx);
}

void CtrRenderTarget::TiledBlit(int ox, int oy, Rect const& src_rect, Bitmap const& src, Rect const& dst_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	GpuTiledToneBlit(ox, oy, src_rect, src, dst_rect, opacity, Tone(), blend_mode);
}

void CtrRenderTarget::GpuTiledToneBlit(int ox, int oy, Rect const& src_rect_, Bitmap const& src, Rect const& dst_rect,
		Opacity const& opacity, const Tone &tone, Bitmap::BlendMode blend_mode) {
	auto src_rect = src_rect_;
	src_rect.Adjust(src.GetRect());
	if (src_rect.IsEmpty() || dst_rect.IsEmpty() || opacity.IsTransparent()) {
		return;
	}

	const int w = src_rect.width;
	const int h = src_rect.height;
	ox %= w;
	oy %= h;
	if (ox < 0) ox += w;
	if (oy < 0) oy += h;

	Effects fx;
	fx.tone = tone;
	fx.blend = blend_mode;
	const Opacity tile_opacity(opacity.top);

	const int dst_x1 = dst_rect.x + dst_rect.width;
	const int dst_y1 = dst_rect.y + dst_rect.height;

	for (int ty = dst_rect.y - oy; ty < dst_y1; ty += h) {
		const int y0 = std::max(ty, dst_rect.y);
		const int y1 = std::min(ty + h, dst_y1);
		if (y1 <= y0 || y1 <= 0 || y0 >= target_height) {
			continue;
		}

		for (int tx = dst_rect.x - ox; tx < dst_x1; tx += w) {
			const int x0 = std::max(tx, dst_rect.x);
			const int x1 = std::min(tx + w, dst_x1);
			if (x1 <= x0 || x1 <= 0 || x0 >= target_width) {
				continue;
			}

			Affine xform;
			xform.tx = static_cast<float>(x0);
			xform.ty = static_cast<float>(y0);
			DrawImage(src, { static_cast<float>(src_rect.x + (x0 - tx)), static_cast<float>(src_rect.y + (y0 - ty)),
				static_cast<float>(x1 - x0), static_cast<float>(y1 - y0) },
				xform, tile_opacity, false, false, fx);
		}
	}
}

void CtrRenderTarget::StretchBlit(Rect const& dst_rect, Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	if (src_rect.IsEmpty() || dst_rect.IsEmpty()) {
		return;
	}

	Affine xform;
	xform.a = static_cast<float>(dst_rect.width) / src_rect.width;
	xform.d = static_cast<float>(dst_rect.height) / src_rect.height;
	xform.tx = static_cast<float>(dst_rect.x);
	xform.ty = static_cast<float>(dst_rect.y);

	Effects fx;
	fx.blend = blend_mode;
	DrawImage(src, { static_cast<float>(src_rect.x), static_cast<float>(src_rect.y),
		static_cast<float>(src_rect.width), static_cast<float>(src_rect.height) },
		xform, opacity, false, false, fx);
}

void CtrRenderTarget::FlipBlit(int x, int y, Bitmap const& src, Rect const& src_rect, bool horizontal, bool vertical,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	GpuBlitOps ops = {};
	ops.flipx = horizontal;
	ops.flipy = vertical;
	ops.blend_mode = blend_mode;
	GpuBlit(x, y, 0, 0, src, src_rect, opacity, ops);
}

void CtrRenderTarget::WaverBlit(int x, int y, double zoom_x, double zoom_y, Bitmap const& src, Rect const& src_rect, int depth, double phase,
		Opacity const& opacity, Bitmap::BlendMode blend_mode) {
	Effects fx;
	fx.blend = blend_mode;
	DrawWaver(x, y, zoom_x, zoom_y, src, src_rect, depth, phase, opacity, false, false, fx);
}

void CtrRenderTarget::RotateZoomOpacityBlit(int x, int y, int ox, int oy,
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

void CtrRenderTarget::FillRect(Rect const& dst_rect, const Color &color) {
	DrawFill(dst_rect, color, Blend::Normal);
}

void CtrRenderTarget::Clear() {
	DrawFill(GetRect(), Color(0, 0, 0, 255), Blend::Replace);
}

void CtrRenderTarget::ToneBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Tone &tone, Opacity const& opacity) {
	GpuBlitOps ops = {};
	ops.tone = tone;
	GpuBlit(x, y, 0, 0, src, src_rect, opacity, ops);
}

void CtrRenderTarget::BlendBlit(int x, int y, Bitmap const& src, Rect const& src_rect, const Color &color, Opacity const& opacity) {
	GpuBlitOps ops = {};
	ops.flash = color;
	GpuBlit(x, y, 0, 0, src, src_rect, opacity, ops);
}

void CtrRenderTarget::GpuBlit(int x, int y, int ox, int oy,
		Bitmap const& src, Rect const& src_rect_,
		Opacity const& opacity, const GpuBlitOps& ops) {
	auto src_rect = src_rect_;
	src_rect.Adjust(src.GetRect());
	if (src_rect.IsEmpty()) {
		return;
	}

	Effects fx;
	fx.tone = ops.tone;
	fx.flash = ops.flash;
	fx.blend = ops.blend_mode;

	const double zx = (ops.zoom_x == 0.0) ? 1.0 : ops.zoom_x;
	const double zy = (ops.zoom_y == 0.0) ? 1.0 : ops.zoom_y;

	if (ops.waver_depth > 0) {
		// Rotation and waver are mutually exclusive
		DrawWaver(static_cast<int>(x - ox * zx), static_cast<int>(y - oy * zy), zx, zy, src, src_rect,
			ops.waver_depth, ops.waver_phase, opacity, ops.flipx, ops.flipy, fx);
		return;
	}

	// target = (x, y) + Rotate(angle) * Scale(zoom) * (local - (ox, oy))
	Affine xform;
	if (ops.angle != 0.0) {
		const double c = std::cos(ops.angle);
		const double s = std::sin(ops.angle);
		xform.a = static_cast<float>(c * zx);
		xform.b = static_cast<float>(-s * zy);
		xform.c = static_cast<float>(s * zx);
		xform.d = static_cast<float>(c * zy);
	} else {
		xform.a = static_cast<float>(zx);
		xform.d = static_cast<float>(zy);
	}
	xform.tx = static_cast<float>(x) - (xform.a * ox + xform.b * oy);
	xform.ty = static_cast<float>(y) - (xform.c * ox + xform.d * oy);

	DrawImage(src, { static_cast<float>(src_rect.x), static_cast<float>(src_rect.y),
		static_cast<float>(src_rect.width), static_cast<float>(src_rect.height) },
		xform, opacity, ops.flipx, ops.flipy, fx);
}

bool CtrRenderTarget::CopyScreenToBitmap(Bitmap& dst) {
	if (gpu.in_frame) {
		return false;
	}

	WaitGpuIdle();

	dst.ClearGpuTexture();

	const int tex_width = screen_tex.width;
	const int rows = AlignRows(screen_height);
	const u32 size = static_cast<u32>(tex_width) * 4 * rows;

	const bool direct = dst.IsCustomAllocated() && dst.pitch() == tex_width * 4 &&
		dst.width() == screen_width && dst.height() == screen_height;

	u32* buf = direct ? static_cast<u32*>(dst.pixels()) : static_cast<u32*>(linearAlloc(size));
	if (!buf) {
		return false;
	}

	// Outside of a frame this transfer is synchronous
	C3D_SyncDisplayTransfer(static_cast<u32*>(screen_tex.data), GX_BUFFER_DIM(tex_width, rows),
		buf, GX_BUFFER_DIM(tex_width, rows), gx_download_flags);
	GSPGPU_InvalidateDataCache(buf, size);

	if (!direct) {
		const int w = std::min(dst.width(), screen_width);
		const int h = std::min(dst.height(), screen_height);
		auto* out = static_cast<uint8_t*>(dst.pixels());
		for (int y = 0; y < h; ++y) {
			memcpy(out + y * dst.pitch(), buf + y * tex_width, w * 4);
		}
		linearFree(buf);
	}

	return true;
}
