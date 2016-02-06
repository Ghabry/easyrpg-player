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

#include "metrics.h"
#include "baseui.h"
#include <cassert>

using namespace Metrics;

namespace {
	int scale() {
		return DisplayUi->GetScaleFactor();
	}
}

int Metrics::Upscale(int value, int scale_factor) {
	int fac = scale() / scale_factor;
	if (fac != 1) {
		return value * fac;
	}
	return value;
}

int Metrics::Downscale(int value, int scale_factor) {
	int fac = scale() / scale_factor;
	if (fac != 1) {
		return value / fac;
	}
	return value;
}

int Display::Width() {
	return OriginalWidth() * scale();
}

int Display::Height() {
	return OriginalHeight() * scale();
}

int Display::OriginalWidth() {
	return 320;
}

int Display::OriginalHeight() {
	return 240;
}

Rect FaceSet::FaceRect(int face_index) {
	int face_square = FaceSize();

	return Rect(
		(face_index % 4) * face_square,
		face_index / 4 * face_square,
		face_square,
		face_square
	);
}

int FaceSet::Size() {
	return FaceSize() * 4;
}

int FaceSet::FaceSize() {
	return scale() * 48;
}

int ChipSet::Width() {
	return 480 * scale();
}

int ChipSet::Height() {
	return 256 * scale();
}

int ChipSet::TileSize() {
	return 16 * scale();
}

Rect System::Border::Top(int which) {
	assert(which >= 0 && which <= 2);
	Rect r(32 * (which + 1) + 8, 0, 16, 8);
	r.Multiply(scale());
	return r;
}

Rect System::Border::Bottom(int which) {
	Rect r(32 * (which + 1) + 8, 32 - 8, 16, 8);
	r.Multiply(scale());
	return r;
}

Rect System::Border::Left(int which) {
	Rect r(32 * (which + 1), 8, 8, 16);
	r.Multiply(scale());
	return r;
}

Rect System::Border::Right(int which) {
	Rect r(64 * (which + 1) - 8, 8, 8, 16);
	r.Multiply(scale());
	return r;
}

Rect System::Border::TopLeft(int which) {
	Rect r(32 * (which + 1), 0, 8, 8);
	r.Multiply(scale());
	return r;
}

Rect System::Border::TopRight(int which) {
	Rect r(64 * (which + 1) - 8, 0, 8, 8);
	r.Multiply(scale());
	return r;
}

Rect System::Border::BottomLeft(int which) {
	Rect r(32 * (which + 1), 32 - 8, 8, 8);
	r.Multiply(scale());
	return r;
}

Rect System::Border::BottomRight(int which) {
	Rect r((64 - 8) * (which + 1), 32 - 8, 8, 8);
	r.Multiply(scale());
	return r;
}

int System::Border::Padding() {
	return 16 * scale();
}
