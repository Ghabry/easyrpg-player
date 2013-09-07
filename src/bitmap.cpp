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

#include "bitmap.h"
#include "font.h"
#include "tone.h"
#include "image_io.h"
#include "output.h"
#include "matrix.h"
#include "utils.h"

#include <pixman.h>

#include <ciso646>
#include <cmath>
#include <sstream>
#include <fstream>

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/math/constants/constants.hpp>

Matrix const Matrix::identity;

#define check_opacity(op)						\
	do {										\
		op = std::max(0, std::min(op, 255));	\
		if(op == 0) { return; }					\
	} while(false)								\

namespace {

bool inside_image(Bitmap const& bmp, int const x, int const y) {
	return (0 <= x and x < int(bmp.width()))
			and (0 <= y and y < int(bmp.height()));
}

pixman_rectangle16_t to_pixman(Rect const& r) {
	pixman_rectangle16_t const ret = {
		int16_t(r.x), int16_t(r.y), uint16_t(r.width), uint16_t(r.height) };
	return ret;
}

pixman_color_t to_pixman(Color const& c) {
	pixman_color_t const ret = {
		uint16_t(c.red  << 8), uint16_t(c.green << 8),
		uint16_t(c.blue << 8), uint16_t(c.alpha << 8) };
	return ret;
}

float const PI = boost::math::constants::pi<float>();

/*
 * originaly from http://beesbuzz.biz/code/hsv_color_transforms.php
 *
 * @param in: input
 * @param H hue shift (in degree)
 * @param S saturation multipiler (scaler)
 * @param V value multiplier (scaler)
 */
void transform_hsv(Color& in, float const H,
				   float const S, float const V) {
	float const VSU = V * S * std::cos(H * PI / 180);
	float const VSW = V * S * std::sin(H * PI / 180);

	in = Color(
		(.299*V+.701*VSU+.168*VSW)*in.red
        + (.587*V-.587*VSU+.330*VSW)*in.green
        + (.114*V-.114*VSU-.497*VSW)*in.blue,
		(.299*V-.299*VSU-.328*VSW)*in.red
        + (.587*V+.413*VSU+.035*VSW)*in.green
        + (.114*V-.114*VSU+.292*VSW)*in.blue,
		(.299*V-.3*VSU+1.25*VSW)*in.red
        + (.587*V-.588*VSU-1.05*VSW)*in.green
        + (.114*V+.886*VSU-.203*VSW)*in.blue,
		in.alpha);
}

void color_tone_change(Color& in, Tone const& t) {
	assert(t.gray == 0);

	in.red = std::max(0, std::min(in.red + t.red, 0xff));
	in.green = std::max(0, std::min(in.green + t.green, 0xff));
	in.blue = std::max(0, std::min(in.blue + t.blue, 0xff));
}

void color_tone_change_gray(Color& in, Tone const& t, float const factor) {
	assert(t.gray != 0);

	float const gray = in.red * 0.299f + in.green * 0.587f + in.blue * 0.114f;

	in.red = std::max(0, std::min<int>((in.red - gray) * factor + gray + t.red + 0.5f, 255));
	in.green = std::max(0, std::min<int>((in.green - gray) * factor + gray + t.green + 0.5f, 255));
	in.blue = std::max(0, std::min<int>((in.blue - gray) * factor + gray + t.blue + 0.5f, 255));
}

struct null_deleter { void operator()(void const*) const {} };

struct pixman_releaser {
	void operator()(pixman_image_t* ptr) const {
		if(ptr) {
			BOOST_VERIFY(pixman_image_unref(ptr));
		}
	}
};

pixman_image_ptr create_color_mask(Color const& c) {
	static Color const ignore_color(0, 0, 0, 255);

	pixman_color_t const col = to_pixman(c);
	return pixman_image_ptr(
		c == ignore_color? NULL : pixman_image_create_solid_fill(&col),
		pixman_releaser());
}

pixman_image_ptr create_opacity_mask(int op) {
	return create_color_mask(Color(0, 0, 0, op));
}

int waver_offset(int depth, double phase, int i) {
	return depth * (1 + std::sin((phase + i * 20) * PI / 180));
}
int waver_offset_max(int depth) { return depth * 2; }

pixman_format_code_t const pixman_format
= Utils::IsBigEndian()? PIXMAN_r8g8b8a8 : PIXMAN_a8b8g8r8;
pixman_op_t const pixman_operation = PIXMAN_OP_OVER;

pixman_image_ptr create_image(size_t w, size_t h, void* data, size_t stride) {
	pixman_image_ptr const ret(
		pixman_image_create_bits_no_clear(
			pixman_format, w, h, reinterpret_cast<uint32_t*>(data), stride),
		pixman_releaser());

	pixman_image_set_component_alpha(ret.get(), true);
	// pixman_image_set_alpha_map(ret.get(), ret.get(), 0, 0);
	BOOST_VERIFY(pixman_image_set_filter(ret.get(), PIXMAN_FILTER_FAST, NULL, 0));

	assert(pixman_image_get_data(ret.get()) == data);

	return ret;
}

pixman_image_ptr create_sub_image(pixman_image_ptr const& ptr, Rect const& rect) {
	size_t const stride = pixman_image_get_stride(ptr.get());
	assert((stride % 4) == 0);

	int const w = pixman_image_get_width(ptr.get()), h = pixman_image_get_height(ptr.get());

	return create_image(
		std::max(0, std::min(rect.width , w - rect.x)),
		std::max(0, std::min(rect.height, h - rect.y)),
		pixman_image_get_data(ptr.get()) + stride / 4 * rect.y + rect.x, stride);
}

Rect create_rect(pixman_image_ptr const& ptr) {
	return Rect(0, 0, pixman_image_get_width(ptr.get()), pixman_image_get_height(ptr.get()));
}

}

bool Bitmap::operator==(Bitmap const& rhs) const {
	return
			(this == &rhs) or (
				width_ == rhs.width_ and
				height_ == rhs.height_ and
				data_ == rhs.data_);
}

Bitmap::Bitmap(size_t w, size_t h, Color const& col)
		: font(Font::Default()), dirty_(true)
		, width_(w), height_(h), data_(w * h, col)
		, ref_(create_image(width_, height_, data_.data(), width_ * 4))
{}
Bitmap::Bitmap(Bitmap const& src)
		: font(src.font), dirty_(true)
		, width_(src.width_), height_(src.height_)
		, data_(src.data_)
		, ref_(create_image(width_, height_, data_.data(), width_ * 4))
{}

size_t Bitmap::width() const {
	assert(size_t(pixman_image_get_width(ref_.get())) == width_);
	return width_;
}
size_t Bitmap::height() const {
	assert(size_t(pixman_image_get_height(ref_.get())) == height_);
	return height_;
}

pixman_image_t* Bitmap::image() {
	return ref_.get();
}

BitmapRef Bitmap::Create(std::string const& file, bool transparent) {
	return ImageIO::ReadImage(file, transparent);
}
BitmapRef Bitmap::Create(size_t width, size_t height, Color const& col) {
	return BitmapRef(new Bitmap(width, height, col));
}
BitmapRef Bitmap::Create(BitmapRef const& another) {
	return BitmapRef(new Bitmap(*another));
}
BitmapRef Bitmap::Create(uint8_t const* data, size_t data_size, bool transparent) {
	static char const XYZ_SIGNATURE[] = "XYZ1";
	static char const BMP_SIGNATURE[] = "BM";
	static uint8_t const PNG_SIGNATURE[] = {137, 80, 78, 71, 13, 10, 26, 10, 0};

	if(data_size < std::max(sizeof(XYZ_SIGNATURE),
							std::max(sizeof(PNG_SIGNATURE), sizeof(BMP_SIGNATURE)))) {
		return Output().Error(boost::format("too small image data size: %d") % data_size), BitmapRef();
	}

	std::istringstream iss(
		std::string(reinterpret_cast<char const*>(data), data_size),
		std::ios::binary | std::ios::in);

#define PP_check_image(type)											\
	(std::string(data, data + sizeof(type ## _SIGNATURE) - 1)			\
	 == std::string(type ## _SIGNATURE,									\
					type ## _SIGNATURE + sizeof(type ## _SIGNATURE) - 1)) \
			? ImageIO::Read ## type (iss, transparent)					\

	BitmapRef const ret =
			PP_check_image(XYZ): PP_check_image(BMP): PP_check_image(PNG):
			BitmapRef();

#undef PP_check_image

	return ret? ret : (Output().Error("Image loading error"), BitmapRef());
}

Rect Bitmap::rect() const {
	return Rect(0, 0, width_, height_);
}

Rect Bitmap::text_size(std::string const& str) const {
	return font->GetSize(str);
}

bool Bitmap::clear_dirty_flag() {
	bool ret = false;
	std::swap(dirty_, ret);
	return ret;
}

void Bitmap::mark_dirty() {
	dirty_ = true;
}

void Bitmap::pixman_image_composite(pixman_image_ptr const& src,
									pixman_image_ptr const& mask,
									int16_t src_x, int16_t src_y,
									int16_t mask_x, int16_t mask_y,
									int16_t dest_x, int16_t dest_y,
									uint16_t width, uint16_t height)
{
	::pixman_image_composite(pixman_operation,
							 src.get(), mask? mask.get() : NULL, ref_.get(),
							 src_x, src_y, mask_x, mask_y,
							 dest_x, dest_y, width, height);
	mark_dirty();
}

void Bitmap::pixman_image_composite(pixman_image_ptr const& src,
									pixman_image_ptr const& mask,
									pixman_image_ptr const& dst,
									int16_t src_x, int16_t src_y,
									int16_t mask_x, int16_t mask_y,
									int16_t dest_x, int16_t dest_y,
									uint16_t width, uint16_t height) const
{
	::pixman_image_composite(pixman_operation,
							 src.get(), mask? mask.get() : NULL, dst.get(),
							 src_x, src_y, mask_x, mask_y,
							 dest_x, dest_y, width, height);
}

Color const& Bitmap::get_pixel(int const x, int const y) const {
	static Color const INVALID;
	return inside_image(*this, x, y)? data_[width_ * y + x] : INVALID;
}

void Bitmap::set_pixel(int const x, int const y, Color const& col) {
	if(inside_image(*this, x, y)) {
		data_[width_ * y + x] = col;
		mark_dirty();
	}
}

void Bitmap::hue_change(int hue) {
	std::for_each(data_.begin(), data_.end(),
				  boost::bind(&transform_hsv, _1, hue, 1.f, 1.f));

	mark_dirty();
}

void Bitmap::draw_text(int x, int y, std::string const& str, Text::Alignment aln) {
	Text::Draw(*this, x, y, str, aln);
}
void Bitmap::draw_text(Rect const& rect, std::string const& str, Text::Alignment aln) {
	if(aln == Text::AlignLeft) {
		Text::Draw(*this, rect.x, rect.y, str, aln);
	} else  {
		int const dx = text_size(str).width - rect.width;
		switch(aln) {
			case Text::AlignCenter:
				Text::Draw(*this, rect.x + dx / 2, rect.y, str);
				break;
			case Text::AlignRight:
				Text::Draw(*this, rect.x + dx, rect.y, str);
				break;
			default: assert(false);
		}
	}
}

void Bitmap::draw_text(int x, int y, std::string const& str, int color, Text::Alignment aln) {
	Text::Draw(*this, x, y, color, str, aln);
}
void Bitmap::draw_text(Rect const& rect, std::string const& str, int color, Text::Alignment aln) {
	if(aln == Text::AlignLeft) {
		Text::Draw(*this, rect.x, rect.y, color, str, aln);
	} else  {
		int const dx = text_size(str).width - rect.width;
		switch(aln) {
			case Text::AlignCenter:
				Text::Draw(*this, rect.x + dx / 2, rect.y, color, str);
				break;
			case Text::AlignRight:
				Text::Draw(*this, rect.x + dx, rect.y, color, str);
				break;
			default: assert(false);
		}
	}
}

void Bitmap::blit(int x, int y, Bitmap const& src, Rect const& src_rect, int opacity) {
	check_opacity(opacity);

	pixman_image_composite(
		src.ref_, create_opacity_mask(opacity),
		src_rect.x, src_rect.y, 0, 0, x, y,
		src_rect.width, src_rect.height);
}

BitmapRef Bitmap::waver(int const depth, double const phase, Rect const& rect) const {
	if(depth == 0) { return sub_image(rect); }
	BitmapRef const ret = Bitmap::Create(rect.width + waver_offset_max(depth), rect.height);

	int current_offset = waver_offset(depth, phase, 0), offset_count = 1;
	for(int i = 1; i < rect.height + 1; ++i) {
		int const next_offset = waver_offset(depth, phase, i);

		if(next_offset == current_offset and i < rect.height) {
			offset_count++;
			continue;
		}

		ret->blit(current_offset, i - offset_count, *this,
				  Rect(rect.x, rect.y, rect.width, offset_count));
		current_offset = next_offset;
		offset_count = 1;
	}

	return ret;
}

void Bitmap::waver_blit(BlitCommon const& info, int depth, double phase, int opacity) {
	BitmapRef const src = info.src.waver(depth, phase, info.src_rect);
	blit(info.x, info.y, *src, src->rect(), opacity);
}

BitmapRef Bitmap::tone_change(Tone const& tone, Rect const& r) const {
	BitmapRef const ret = sub_image(r);
	if(tone != Tone()) {
		std::for_each(ret->data_.begin(), ret->data_.end(),
					  tone.gray == 0
					  ? boost::function<void(Color&)>(boost::bind(&color_tone_change, _1, boost::cref(tone)))
					  : boost::function<void(Color&)>(boost::bind(
						  &color_tone_change_gray, _1, boost::cref(tone), (255 - tone.gray) / 255.f)));
	}
	return ret;
}

void Bitmap::tone_blit(BlitCommon const& info, Tone const& tone) {
	BitmapRef const src = info.src.tone_change(tone, info.src_rect);
	blit(info.x, info.y, *src, src->rect());
}

void Bitmap::blend_blit(BlitCommon const& info, Color const& color) {
	if(color.alpha == 0) {
		if(&info.src != this) {
			blit(info.x, info.y, info.src, info.src_rect, 255);
		}
		return;
	}

	pixman_image_composite(
		info.src.ref_, create_color_mask(color),
		info.src_rect.x, info.src_rect.y, 0, 0, info.x, info.y,
		info.src_rect.width, info.src_rect.height);
}

void Bitmap::tiled_blit(BlitCommon const& info, Rect const& dst_rect, int opacity) {
	check_opacity(opacity);

	int ox = info.x, oy = info.y;

	if(ox >= info.src_rect.width ) ox %= info.src_rect.width;
	if(oy >= info.src_rect.height) oy %= info.src_rect.width;
	if(ox < 0) ox += info.src_rect.width  * ((-ox + info.src_rect.width  - 1) / info.src_rect.width );
	if(oy < 0) ox += info.src_rect.height * ((-ox + info.src_rect.height - 1) / info.src_rect.height);

	pixman_image_ptr const src_bmp = create_sub_image(info.src.ref_, info.src_rect);
	pixman_image_set_repeat(src_bmp.get(), PIXMAN_REPEAT_NORMAL);

	SET_MATRIX(src_bmp.get(), Matrix::translate_(ox, oy));
	pixman_image_composite(
		src_bmp, create_opacity_mask(opacity),
		0, 0, 0, 0, dst_rect.x, dst_rect.y, dst_rect.width, dst_rect.height);
}

void Bitmap::stretch_blit(Rect const& dst_rect, Bitmap const& src, Rect const& src_rect, int opacity) {
	check_opacity(opacity);

	double const zoom_x = double(src_rect.width ) / dst_rect.width ,
				 zoom_y = double(src_rect.height) / dst_rect.height;
	SET_MATRIX(src.ref_.get(),
			   Matrix::scale_(zoom_x, zoom_y)
			   .translate(src_rect.x, src_rect.y));

	pixman_image_composite(
		src.ref_, create_opacity_mask(opacity),
		0, 0, 0, 0, dst_rect.x, dst_rect.y,
		dst_rect.width, dst_rect.height);
}

void Bitmap::flip_blit(BlitCommon const& info, bool const horizontal, bool const vertical) {
	SET_MATRIX(
		info.src.ref_.get(),
		Matrix::scale_(horizontal? -1 : 1, vertical? -1 : 1)
		.translate(horizontal? info.src.width() : 0,
				   vertical? info.src.height() : 0));
	pixman_image_composite(info.src.ref_, pixman_image_ptr(),
						   horizontal? info.src.width() - info.src_rect.x + info.src_rect.width : info.src_rect.x,
						   vertical? info.src.height() - info.src_rect.y + info.src_rect.height : info.src_rect.y,
						   0, 0, info.x, info.y, info.src_rect.width, info.src_rect.height);
}
void Bitmap::flip(Rect const& rect, bool const horizontal, bool const vertical) {
	if(not horizontal and not vertical) { return; }

	BitmapRef const resampled = sub_image(rect);
	resampled->flip_blit(BlitCommon(0, 0, *this, rect), horizontal, vertical);
	pixman_image_composite(resampled->ref_, pixman_image_ptr(),
						   0, 0, 0, 0, rect.x, rect.y, rect.width, rect.height);
}

BitmapRef Bitmap::resample(int const scale_w, int const scale_h, Rect const& src_rect) const {
	BitmapRef const ret = Bitmap::Create(scale_w, scale_h, Color());

	SET_MATRIX(
		ref_.get(),
		Matrix::scale_(double(scale_w) / src_rect.width,
					   double(scale_h) / src_rect.height));
	pixman_image_composite(ref_, pixman_image_ptr(), ret->ref_,
						   src_rect.x, src_rect.y, 0, 0, 0, 0, scale_w, scale_h);

	return ret;
}
BitmapRef Bitmap::sub_image(Rect const& rect) const {
	BitmapRef const ret = Bitmap::Create(rect.width, rect.height);

	SET_MATRIX(ref_.get(), Matrix::translate_(rect.x, rect.y));
	pixman_image_composite(
		ref_, pixman_image_ptr(), ret->ref_,
		rect.x, rect.y, 0, 0, 0, 0, rect.width, rect.height);

	assert(ret->width () == size_t(rect.width ));
	assert(ret->height() == size_t(rect.height));

	return ret;
}

void Bitmap::fill(Rect const& r, Color const& c) {
	pixman_color_t const color = to_pixman(c);
	pixman_rectangle16_t const rect = to_pixman(r);
	pixman_image_fill_rectangles(
		pixman_operation, ref_.get(), &color, 1, &rect);

	mark_dirty();
}

void Bitmap::clear() {
	pixman_color_t const c = to_pixman(Color(0, 0, 0, 0));
	pixman_rectangle16_t const r = to_pixman(rect());
	pixman_image_fill_rectangles(PIXMAN_OP_CLEAR, ref_.get(), &c, 1, &r);

	mark_dirty();
}

void Bitmap::transform_blit(BlitCommon const& info, Matrix const& mat, int opacity) {
	check_opacity(opacity);

	Rect adjusted = info.src_rect;
	adjusted.Adjust(info.src.width(), info.src.height());
	if(adjusted.IsEmpty()) { return; }

	pixman_image_ptr const src =
			adjusted == info.src.rect()
			? info.src.ref_ : create_sub_image(info.src.ref_, adjusted);
	Rect const src_rect = mat.transform(create_rect(src));

	SET_MATRIX(info.src.ref_.get(), mat.invert());
	pixman_image_composite(
		src, create_opacity_mask(opacity),
		src_rect.x, src_rect.y, 0, 0,
		info.x, info.y, src_rect.width, src_rect.height);
}

void Bitmap::effect_blit(BlitCommon const& info, Matrix const& mat,
						 int top_opacity, int bottom_opacity, int opacity_split)
{
	if(opacity_split <= 0) {
		transform_blit(info, mat, top_opacity);
	} else if(opacity_split >= info.src_rect.height) {
		transform_blit(info, mat, bottom_opacity);
	} else {
		size_t const top_height = info.src_rect.height - opacity_split;

		transform_blit(
			BlitCommon(info.x, info.y, info.src, Rect(
				info.src_rect.x, info.src_rect.y,
				info.src_rect.width, top_height)),
			mat, top_opacity);

		transform_blit(
			BlitCommon(info.x, info.y, info.src, Rect(
				info.src_rect.x, info.src_rect.y + top_height,
				info.src_rect.width, opacity_split)),
			mat, bottom_opacity);
	}
}

void Bitmap::effect_blit(BlitCommon const& info,
						 int top_opacity, int bottom_opacity, int opacity_split,
						 Tone const& tone, double zoom_x, double zoom_y, double angle,
						 int waver_depth, double waver_phase)
{
	Rect src_rect = info.src_rect;
	EASYRPG_SHARED_PTR<Bitmap const> draw(&info.src, null_deleter());

	// apply tone
	if (not tone.IsEmpty()) {
		draw = draw->tone_change(tone, src_rect);
		src_rect = draw->rect();
	}

	// apply waver
	if (waver_depth != 0) {
		draw = draw->waver(waver_depth, waver_phase, src_rect);
		src_rect = draw->rect();
	}

	effect_blit(
		BlitCommon(info.x, info.y, *draw, src_rect),
		Matrix::scale_(zoom_x, zoom_y).rotate(angle),
		top_opacity, bottom_opacity, opacity_split);
}
