#include "image_io.h"
#include "bitmap.h"
#include "utils.h"
#include "output.h"
#include "filefinder.h"

#include <png.h>
#include <zlib.h>

#include <ciso646>
#include <fstream>

#include <boost/scope_exit.hpp>
#include <boost/function.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/format.hpp>


namespace {

typedef boost::function<BitmapRef(std::istream&, bool)> image_reader;
typedef boost::container::flat_map<std::string, image_reader> ext_map_type;
static ext_map_type const ext_map =
		boost::assign::map_list_of
		("bmp", image_reader(&ImageIO::ReadBMP))
		("png", &ImageIO::ReadPNG)
		("xyz", &ImageIO::ReadXYZ)
		;

void write_data(png_structp out_ptr, png_bytep data, png_size_t len) {
	reinterpret_cast<std::ostream*>(png_get_io_ptr(out_ptr))
			->write(reinterpret_cast<char const*>(data), len);
}
void flush_stream(png_structp out_ptr) {
	reinterpret_cast<std::ostream*>(png_get_io_ptr(out_ptr))->flush();
}
void read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	reinterpret_cast<std::istream*>(png_get_io_ptr(png_ptr))
			->read(reinterpret_cast<char*>(data), length);
}

uint16_t get_2(std::istream& is) {
	return (unsigned(is.get()) << 0) | (unsigned(is.get()) << 8);
}
uint32_t get_4(std::istream& is) {
	return
			(unsigned(is.get()) << 0) | (unsigned(is.get()) << 8) |
			(unsigned(is.get()) << 16) | (unsigned(is.get()) << 24) ;
}

typedef EASYRPG_ARRAY<uint8_t, 4> RawPngColor;
Color to_color(RawPngColor const& c) {
	return Color(c[0], c[1], c[2], c[3]);
}

}

BitmapRef ImageIO::ReadBMP(std::istream& is, bool const transparent) {
	size_t const start_pos = is.tellg();

	// BITMAPFILEHEADER structure
	//
	// 0	2	signature: 'B','M'
	// 2	4	file size
	// 6	2	reserved
	// 8	2	reserved
	// 10	4	offset to bitmap data

	static char const SIGNATURE[] = "BM";
	enum { SIGNATURE_SIZE = sizeof(SIGNATURE) - 1 };
	EASYRPG_ARRAY<char, SIGNATURE_SIZE> signature_buf;
	is.read(signature_buf.data(), SIGNATURE_SIZE);
	if (std::string(signature_buf.begin(), signature_buf.end()) == SIGNATURE) {
		return Output().Debug("Not a valid BMP file."), BitmapRef();
	}

	// file size is skipped because every program writes other data into
	// this field and not needed for correct decoding.

	const unsigned bits_offset = get_4(is);

	// BITMAPINFOHEADER structure
	//
	// 0	4	BITMAPINFOHEADER size
	// 4	4	width
	// 8	4	height (+ve => bottom-up, -ve => top-down)
	// 12	2	number of planes (must be 1)
	// 14	2	bits per pixel
	// 16	4	compression
	// 20	4	image size
	// 24	4	X pixels per meter
	// 28	4	Y pixels per meter
	// 32	4	number of palette colors used
	// 36	4	number of important palette colors
	// 40 ... palette

	static const unsigned BITMAPINFOHEADER_SIZE = 40;
	if (get_4(is) != BITMAPINFOHEADER_SIZE) {
		return Output().Debug("Incorrect BMP header size."), BitmapRef();
	}

	int32_t width = get_4(is), raw_height = get_4(is);

	bool const vflip = raw_height > 0;
	int32_t const height = std::abs(raw_height);

	const int planes = get_2(is);
	if (planes != 1) {
		return Output().Debug("BMP planes is not 1."), BitmapRef();
	}

	const int depth = get_2(is);
	if (depth != 8) {
		return Output().Debug("BMP image is not 8-bit."), BitmapRef();
	}

	const int compression = get_4(is);
	static const int BI_RGB = 0;
	if (compression != BI_RGB) {
		return Output().Debug("compressed BMP not supported."), BitmapRef();
	}

	int const image_size = get_4(is);
	if (image_size != 0 && image_size != width * height) {
		return Output().Debug("Invalid BMP image size."), BitmapRef();
	}

	int const num_colors = std::min(256U, get_4(is));
	boost::container::vector<EASYRPG_ARRAY<uint8_t, 4> > palette(num_colors);
	is.read(reinterpret_cast<char*>(palette.data()), num_colors * 4);

	// Ensure no palette entry is an exact duplicate of #0
	for (int i = 1; i < num_colors; i++) {
		if (palette[i][0] == palette[0][0] &&
			palette[i][1] == palette[0][1] &&
			palette[i][2] == palette[0][2]) {
			palette[i][0] ^= 1;
		}
	}

	BitmapRef const ret = Bitmap::Create(width, height);

	// align each line with 4 bytes
	size_t const line_size = (width * depth) >> 3;
	size_t const aligned_width =
			(line_size & 0x03u)? (((line_size >> 2) + 1) << 2) : line_size;

	for (int y = 0; y < height; y++) {
		is.seekg(start_pos + bits_offset + (vflip ? height - 1 - y : y) * aligned_width);
		for (int x = 0; x < width; x++) {
			uint8_t const idx = is.get();
			EASYRPG_ARRAY<uint8_t, 4> const& pal = palette[idx];
			ret->set_pixel(x, y, Color(
				pal[2], pal[1], pal[0], (transparent && idx == 0)? 0x00 : 0xff));
		}
	}

	return ret;
}

BitmapRef ImageIO::ReadPNG(std::istream& is, bool const transparent) {
	png_struct* png_ptr = png_create_read_struct(
		PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (not png_ptr) {
		return Output().Debug("Couldn't allocate PNG structure"), BitmapRef();
	}

	png_info* info_ptr = png_create_info_struct(png_ptr);
	if (not info_ptr) {
		return Output().Debug("Couldn't allocate PNG info structure"), BitmapRef();
	}

	png_set_read_fn(png_ptr, (png_voidp) &is, read_data);
	png_read_info(png_ptr, info_ptr);

	png_uint_32 width, height;
	int bit_depth, color_type;
	png_get_IHDR(png_ptr, info_ptr, &width, &height,
				 &bit_depth, &color_type, NULL, NULL, NULL);

	png_color black = {0,0,0};
	png_colorp palette = NULL;
	int num_palette = 0;

	switch (color_type) {
		case PNG_COLOR_TYPE_PALETTE:
			if (!png_get_valid(png_ptr, info_ptr, PNG_INFO_PLTE)) {
				return Output().Debug("Palette PNG without PLTE block"), BitmapRef();
			}
			if (transparent) {
				png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
			}
			png_set_strip_alpha(png_ptr);
			png_set_palette_to_rgb(png_ptr);
			png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
			break;
		case PNG_COLOR_TYPE_GRAY:
			png_set_gray_to_rgb(png_ptr);
			if (bit_depth < 8)
				png_set_expand_gray_1_2_4_to_8(png_ptr);
			png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
			if (transparent) {
				palette = &black;
				num_palette = 1;
			}
			break;
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			png_set_gray_to_rgb(png_ptr);
			if (bit_depth < 8)
				png_set_expand_gray_1_2_4_to_8(png_ptr);
			break;
		case PNG_COLOR_TYPE_RGB:
			png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
			break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			break;
		default:
			return Output().Debug(boost::format("unsupported color type: %d") % color_type), BitmapRef();
	}

	if (bit_depth < 8) { png_set_packing(png_ptr); }
	if (bit_depth == 16) { png_set_strip_16(png_ptr); }

	png_read_update_info(png_ptr, info_ptr);

	BitmapRef const ret = Bitmap::Create(width, height);
	boost::container::vector<RawPngColor> buf(width);

	if (transparent and palette) {
		Color const src_color(palette->red, palette->green, palette->blue, 0xff);
		Color const dst_color(0, 0, 0, 0);

		for (size_t y = 0; y < height; y++) {
			png_read_row(png_ptr, reinterpret_cast<png_bytep>(buf.data()), NULL);
			for(size_t x = 0; x < width; ++x) {
				Color const c = to_color(buf[x]);
				ret->set_pixel(x, y, (c == src_color)? dst_color : c);
			}
		}
	} else {
		for (size_t y = 0; y < height; y++) {
			png_read_row(png_ptr, reinterpret_cast<png_bytep>(buf.data()), NULL);
			for(size_t x = 0; x < width; ++x) {
				ret->set_pixel(x, y, to_color(buf[x]));
			}
		}
	}

	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	return ret;
}

BitmapRef ImageIO::ReadXYZ(std::istream& is, bool const transparent) {
	static char const SIGNATURE[] = "XYZ1";
	enum { SIGNATURE_SIZE = sizeof(SIGNATURE) - 1 };
	EASYRPG_ARRAY<char, SIGNATURE_SIZE> signature_buf;
	is.read(signature_buf.data(), SIGNATURE_SIZE);
	if (std::string(signature_buf.begin(), signature_buf.end()) == SIGNATURE) {
		return Output().Debug("Not a valid XYZ file."), BitmapRef();
	}

	uint16_t const width = get_2(is), height = get_2(is);

	enum { PALETTE_SIZE = 3 * 0x100 };
	size_t const image_data_size = width * height * sizeof(uint8_t);

	boost::container::vector<Bytef> dst_buf(PALETTE_SIZE + image_data_size);

	size_t const start_pos = is.tellg();
	is.seekg(0, std::ios::end);
	size_t const src_size = size_t(is.tellg()) - start_pos;
	is.seekg(start_pos);
	boost::container::vector<char> src_buf(src_size);
	is.read(src_buf.data(), src_size);

	uLongf dst_size = dst_buf.size();

	if (uncompress(dst_buf.data(), &dst_size,
				   reinterpret_cast<Bytef const*>(src_buf.data()), src_size) != Z_OK) {
		return Output().Debug("Error decompressing XYZ file."), BitmapRef();
	}
	assert(dst_size == 0);

	BitmapRef const ret = Bitmap::Create(width, height);

    for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint8_t const idx = dst_buf[PALETTE_SIZE + width * y + x];
			size_t const pal_off = 0 + 3 * idx;
			ret->set_pixel(x, y, Color(
				dst_buf[pal_off + 0],
				dst_buf[pal_off + 1],
				dst_buf[pal_off + 2],
				(transparent && idx == 0) ? 0x00 : 0xff));
		}
    }

	return ret;
}

bool ImageIO::WritePNG(BitmapRef const& bmp, std::ostream& os) {
	size_t const width = bmp->width(), height = bmp->height();

	boost::container::vector<uint32_t> data(width * height);
	for(size_t y = 0; y < height; ++y) {
		for(size_t x = 0; x < width; ++x) {
			reinterpret_cast<Color&>(data[width * y + x]) = bmp->get_pixel(x, y);
		}
	}

	boost::container::vector<png_bytep> ptrs(height);
	for(size_t i = 0; i < ptrs.size(); ++i) {
		ptrs[i] = reinterpret_cast<png_bytep>(&data[width*i]);
	}

	png_structp write = NULL;
	if(!(write = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
		return Output().Debug("error in png_create_write"), false;
	}

	png_infop info = NULL;
	BOOST_SCOPE_EXIT((&write)(&info)) {
		png_destroy_write_struct(&write, &info);
	} BOOST_SCOPE_EXIT_END do {} while(0);
	if(!(info = png_create_info_struct(write))) {
		return Output().Debug("error in png_create_info_struct"), false;
	}

	png_set_write_fn(write, &os, &write_data, &flush_stream);

	png_set_IHDR(write, info, width, height, 8,
				 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
				 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(write, info);
	png_write_image(write, ptrs.data());
	png_write_end(write, NULL);

	return true;
}

BitmapRef ImageIO::ReadImage(std::string const& file, bool const transparent) {
	if(not FileFinder().Exists(file)) {
		return Output().Debug(boost::format("image file not found: %s") % file), BitmapRef();
	}

	std::string const ext = Utils::GetExt(file);
	ext_map_type::const_iterator const it = ext_map.find(ext);
	return (it == ext_map.end())
			? Output().Debug(boost::format("Unsupported extension: %s") % ext), BitmapRef()
			: it->second(*FileFinder().openUTF8(
				file, std::ios::binary | std::ios::in), transparent);
}
