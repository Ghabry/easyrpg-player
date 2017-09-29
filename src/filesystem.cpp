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

#include "filesystem.h"
#include "filefinder.h"
#include "utils.h"
#include <algorithm>

std::string Filesystem::CombinePath(std::string const & dir, std::string const & entry) {
	std::string str = dir.empty() ? entry : dir + "/" + entry;
#ifdef _WIN32
	std::replace(str.begin(), str.end(), '/', '\\');
#else
	std::replace(str.begin(), str.end(), '\\', '/');
#endif
	return str;
}

bool Filesystem::IsValid() {
	return Exists("");
}

std::shared_ptr<FileFinder::istream> Filesystem::openUTF8Input(const std::string &name, std::ios_base::openmode m) {
	std::streamsize size = GetFilesize(name);
	std::streambuf* buf = CreateInputStreambuffer(name, m);

	std::shared_ptr<FileFinder::istream> ret(new FileFinder::istream(buf, size));

	return (*ret) ? ret : std::shared_ptr<FileFinder::istream>();
}

std::shared_ptr<std::ostream> Filesystem::openUTF8Output(const std::string &name, std::ios_base::openmode m) {
	//std::streamsize size = GetFilesize(name);
	std::streambuf* buf = CreateOutputStreambuffer(name, m);

	std::shared_ptr<std::ostream> ret(new std::ostream(buf));

	return (*ret) ? ret : std::shared_ptr<std::ostream>();
}

std::string Filesystem::FindFile(const std::string& dir,
								 const std::string& name,
								 char const* exts[]) {
	return FileFinder::FindFile(*directory_tree, dir, name, exts);
}

std::string Filesystem::FindImage(const std::string& dir, const std::string& name) {
#ifdef EMSCRIPTEN
	return FileFinder::FindDefault(*directory_tree, dir, name);
#endif

	static const char* IMG_TYPES[] = { ".bmp",  ".png", ".xyz", NULL };
	return FileFinder::FindFile(*directory_tree, dir, name, IMG_TYPES);
}

std::string Filesystem::FindDefault(const std::string& name) {
	std::vector<std::string> path_comps = FileFinder::SplitPath(name);
	if (path_comps.size() > 1) {
		// When the searched name contains a directory search in this directory
		// instead of the root

		std::string f;
		for (auto it = path_comps.begin() + 1; it != path_comps.end(); ++it) {
			f = Filesystem::CombinePath(f, *it);
		}

		return FindDefault(path_comps[0], f);
	}

	auto it = directory_tree->files.find(Utils::LowerCase(name));

	return(it != directory_tree->files.end()) ? Filesystem::CombinePath(directory_tree->directory_path, it->second) : "";
}

std::string Filesystem::FindDefault(const std::string& dir, const std::string& name) {
	static const char* no_exts[] = {"", NULL};
	return FileFinder::FindFile(*directory_tree, dir, name, no_exts);
}

std::string Filesystem::FindMusic(const std::string& name) {
#ifdef EMSCRIPTEN
	return FileFinder::FindDefault(*directory_tree, "Music", name);
#endif

	static const char* MUSIC_TYPES[] = {
			".opus", ".oga", ".ogg", ".wav", ".mid", ".midi", ".mp3", ".wma", nullptr };
	return FileFinder::FindFile(*directory_tree, "Music", name, MUSIC_TYPES);
}

std::string Filesystem::FindSound(const std::string& name) {
#ifdef EMSCRIPTEN
	return FileFinder::FindDefault(*directory_tree, "Sound", name);
#endif

	static const char* SOUND_TYPES[] = {
			".opus", ".oga", ".ogg", ".wav", ".mp3", ".wma", nullptr };
	return FileFinder::FindFile(*directory_tree, "Sound", name, SOUND_TYPES);
}


std::string Filesystem::FindFont(const std::string& name) {
	static const char* FONTS_TYPES[] = {
			".ttf", ".ttc", ".otf", ".fon", NULL, };
	std::string path = FileFinder::FindFile(*directory_tree, "Font", name, FONTS_TYPES);

#if defined(_WIN32) && !defined(_ARM_)
	if (!path.empty()) {
		return path;
	}

	std::string folder_path = "";
	std::string filename = name;

	size_t separator_pos = path.rfind('\\');
	if (separator_pos != std::string::npos) {
		folder_path = path.substr(0, separator_pos);
		filename = path.substr(separator_pos, path.length() - separator_pos);
	}

	std::string font_filename = GetFontFilename(filename);
	if (!font_filename.empty()) {
		if (FileFinder::Exists(folder_path + font_filename))
			return folder_path + font_filename;

		if (FileFinder::Exists(fonts_path + font_filename))
			return fonts_path + font_filename;
	}

	return "";
#else
	return path;
#endif
}

