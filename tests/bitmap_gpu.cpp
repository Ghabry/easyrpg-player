#include <cstdlib>
#include <cstring>
#include <vector>
#include "bitmap.h"
#include "doctest.h"

TEST_SUITE_BEGIN("BitmapGpu");

namespace {

/** Pads the pitch like the 3DS allocator does */
class PaddingAllocator final : public Bitmap::PixelAllocator {
public:
	void* Allocate(int width, int height, int bytes, int& pitch) override {
		if (bytes != 4) {
			return nullptr;
		}
		pitch = (width + 16) * bytes;
		++allocations;
		return calloc(static_cast<size_t>(pitch) * height, 1);
	}

	void Free(void* pixels) override {
		++frees;
		free(pixels);
	}

	int allocations = 0;
	int frees = 0;
};

/** Resets the global allocator, also when a check fails */
struct AllocatorGuard {
	explicit AllocatorGuard(Bitmap::PixelAllocator* a) { Bitmap::SetPixelAllocator(a); }
	~AllocatorGuard() { Bitmap::SetPixelAllocator(nullptr); }
};

/** Fake GPU texture: a copy of the pixels */
struct FakeTexture {
	std::vector<uint32_t> pixels;
	int width = 0;
	bool released = false;
};

void Upload(Bitmap& bmp, FakeTexture& tex) {
	tex.width = bmp.width();
	tex.pixels.resize(bmp.width() * bmp.height());
	for (int y = 0; y < bmp.height(); ++y) {
		memcpy(&tex.pixels[y * bmp.width()], static_cast<const uint8_t*>(bmp.pixels()) + y * bmp.pitch(), bmp.width() * 4);
	}

	bmp.SetGpuTexture(&tex,
		[](Bitmap const& b) { static_cast<FakeTexture*>(b.GetGpuTexture())->released = true; },
		[](Bitmap const& b, void* pixels, int pitch) {
			auto* t = static_cast<FakeTexture*>(b.GetGpuTexture());
			for (int y = 0; y < b.height(); ++y) {
				memcpy(static_cast<uint8_t*>(pixels) + y * pitch, &t->pixels[y * t->width], t->width * 4);
			}
		});
}

}

TEST_CASE("PaddedPitch") {
	Bitmap::SetFormat(format_R8G8B8A8_a().format());
	PaddingAllocator alloc;
	AllocatorGuard guard(&alloc);

	{
		auto bmp = Bitmap::Create(20, 10, Color(255, 0, 0, 255));
		REQUIRE(bmp->IsCustomAllocated());
		REQUIRE_EQ(bmp->pitch(), (20 + 16) * 4);
		REQUIRE_EQ(bmp->width(), 20);
		REQUIRE_EQ(bmp->height(), 10);
		// The zero initialized padding must not count as transparent pixels
		REQUIRE_EQ(bmp->ComputeImageOpacity(), ImageOpacity::Opaque);
		REQUIRE_EQ(bmp->GetColorAt(19, 9), bmp->GetColorAt(0, 0));
		REQUIRE_EQ(bmp->GetColorAt(0, 0).alpha, 255);
	}

	REQUIRE_EQ(alloc.allocations, 1);
	REQUIRE_EQ(alloc.frees, 1);
}

TEST_CASE("DiscardAndRestore") {
	Bitmap::SetFormat(format_R8G8B8A8_a().format());
	PaddingAllocator alloc;
	AllocatorGuard guard(&alloc);

	FakeTexture tex;
	{
		auto bmp = Bitmap::Create(8, 4, true);
		bmp->FillRect(Rect(0, 0, 4, 4), Color(10, 20, 30, 255));
		bmp->FillRect(Rect(4, 0, 4, 4), Color(40, 50, 60, 255));

		// Not read-only: cannot be discarded
		Upload(*bmp, tex);
		REQUIRE_FALSE(bmp->DiscardPixels());

		bmp->CheckPixels(Bitmap::Flag_ReadOnly);
		REQUIRE(bmp->IsReadOnly());
		const auto size = bmp->GetSize();
		const Color left = bmp->GetColorAt(1, 1);
		const Color right = bmp->GetColorAt(6, 3);
		REQUIRE_FALSE(left == right);

		REQUIRE(bmp->DiscardPixels());
		REQUIRE(bmp->IsPixelsDiscarded());
		REQUIRE_EQ(alloc.frees, 1);

		// Metadata is available without pixels
		REQUIRE_EQ(bmp->width(), 8);
		REQUIRE_EQ(bmp->height(), 4);
		REQUIRE_EQ(bmp->GetSize(), size);

		// CPU access restores the pixels
		REQUIRE_EQ(bmp->GetColorAt(1, 1), left);
		REQUIRE_FALSE(bmp->IsPixelsDiscarded());
		REQUIRE_EQ(bmp->GetColorAt(6, 3), right);
		REQUIRE_EQ(alloc.allocations, 2);

		// Used by the CPU: never discarded again
		REQUIRE_FALSE(bmp->DiscardPixels());

		// Blitting from a discarded bitmap works
		auto other = Bitmap::Create(8, 4, true);
		Upload(*other, tex);
		other->CheckPixels(Bitmap::Flag_ReadOnly);
		REQUIRE(other->DiscardPixels());
		auto dst = Bitmap::Create(8, 4, true);
		dst->Blit(0, 0, *bmp, bmp->GetRect(), Opacity::Opaque());
		REQUIRE_EQ(dst->GetColorAt(5, 0), right);

		// Destroying a discarded bitmap does not restore it
		const int allocs_before = alloc.allocations;
		other.reset();
		REQUIRE_EQ(alloc.allocations, allocs_before);
	}
	REQUIRE(tex.released);
}
