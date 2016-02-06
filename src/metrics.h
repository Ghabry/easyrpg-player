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

#ifndef EASYRPG_METRICS
#define EASYRPG_METRICS

class Rect;

/**
 * Metrics namespace contains shared size information for bitmaps and windows.
 */
namespace Metrics {
	int GetScale();
	int Upscale(int value, int scale_factor = 1);

	int Downscale(int value, int scale_factor = 1);

	namespace Display {
		int Width();
		int Height();
		int OriginalWidth();
		int OriginalHeight();
	}

	namespace FaceSet {
		/** Size of the file (width and height) */
		int Size();
		/** Size of a single face (width and height) */
		int FaceSize();
		/** Boundary rect for index face_index */
		Rect FaceRect(int face_index);
	}

	namespace ChipSet {
		/** Height of the file */
		int Height();
		/** Width of the file */
		int Width();
		/** Size of a single tile (width and height) */
		int TileSize();
	}

	namespace CharSet {
		int CharHeight();
		int CharWidth();
		Rect CharRect(int char_index);
	}

	namespace System {
		namespace Border {
			Rect Top(int which);
			Rect Bottom(int which);
			Rect Left(int which);
			Rect Right(int which);
			Rect TopLeft(int which);
			Rect TopRight(int which);
			Rect BottomLeft(int which);
			Rect BottomRight(int which);
			int Padding();
		}
	}
}

#endif
