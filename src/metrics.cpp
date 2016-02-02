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

using namespace Metrics;

namespace {
	int scale() {
		return DisplayUi->GetScaleFactor();
	}
}

int Display::Width() {
	return 640;
}

int Display::Height() {
	return 480;
}

Rect FaceSet::Get(int face_index) {
	const int face_square = 48 * scale();

	return Rect(
		(face_index % 4) * face_square,
		face_index / 4 * face_square,
		face_square,
		face_square
	);
}
