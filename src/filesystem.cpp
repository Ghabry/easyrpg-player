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
#include "output.h"
#include "player.h"
#include <algorithm>
#include <cassert>

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

std::shared_ptr<std::istream> Filesystem::openUTF8Input(const std::string &name, std::ios_base::openmode m) {
	std::streambuf* buf = CreateInputStreambuffer(name, m);

	std::shared_ptr<std::istream> ret(new std::istream(buf));

	return (*ret) ? ret : std::shared_ptr<std::istream>();
}

std::shared_ptr<std::ostream> Filesystem::openUTF8Output(const std::string &name, std::ios_base::openmode m) {
	//std::streamsize size = GetFilesize(name);
	std::streambuf* buf = CreateOutputStreambuffer(name, m);

	std::shared_ptr<std::ostream> ret(new std::ostream(buf));

	return (*ret) ? ret : std::shared_ptr<std::ostream>();
}

#ifdef SUPPORT_MOVIES
const char* const MOVIE_TYPES[] = { ".avi", ".mpg" };
#endif

typedef std::vector<FilesystemRef> search_path_list;
FilesystemRef game_filesystem;
search_path_list search_paths;
std::string fonts_path;

std::string Filesystem::FindFile(const std::string& dir_,
					 const std::string& name,
					 char const* exts[]) const
{
	using namespace FileFinder;

#ifdef EMSCRIPTEN
	// The php filefinder should have given us an useable path
	std::string em_file = CombinePath(dir, name);

	if (Exists(em_file))
		return em_file;
#endif

	std::string dir = dir_.empty() ? "/" : dir_;

	std::string lower_dir = Utils::LowerCase(dir);
	std::string const escape_symbol = Player::escape_symbol;
	std::string corrected_name = Utils::LowerCase(name);

	std::string combined_path = Filesystem::CombinePath(lower_dir, corrected_name);
	std::string canon = MakeCanonical(combined_path, 1);
	if (combined_path != canon) {
		// Very few games (e.g. Yume2kki) use path traversal (..) in the filenames to point
		// to files outside of the actual directory.
		// Fix the path and search the file again with the correct root directory set.
		Output::Debug("Path adjusted: %s -> %s", combined_path.c_str(), canon.c_str());
		/*FIXME for (char const** c = exts; *c != NULL; ++c) {
			std::string res = FileFinder::FindDefault(tree, canon + *c);
			if (!res.empty()) {
				return res;
			}
		}
		return "";*/
	}

	if (!escape_symbol.empty()) {
#ifdef _WIN32
		if (escape_symbol != "\\") {
#endif
		std::size_t escape_pos = corrected_name.find(escape_symbol);
		while (escape_pos != std::string::npos) {
			corrected_name.erase(escape_pos, escape_symbol.length());
			corrected_name.insert(escape_pos, "/");
			escape_pos = corrected_name.find(escape_symbol);
		}
#ifdef _WIN32
		}
#endif
	} else {
		assert(lower_dir == "/");
	}

	auto dir_it = dir_cache.find(lower_dir);
	if (dir_it == dir_cache.end()) {
		assert(fs_cache.find(lower_dir) == fs_cache.end());

		auto entries = ListDirectory(dir);

		dir_cache[lower_dir] = dir;

		std::unordered_map<std::string, DirectoryEntry> fs_cache_entry;

		for (auto& entry : entries) {
			fs_cache_entry[Utils::LowerCase(entry.name)] = entry;
		}

		fs_cache.emplace(lower_dir, fs_cache_entry);
	}

	dir_it = dir_cache.find(lower_dir);
	auto it = fs_cache.find(lower_dir);

	for (char const** c = exts; *c != NULL; ++c) {
		auto entry_it = (*it).second.find(corrected_name + *c);
		if (entry_it != (*it).second.end()) {
			return Filesystem::CombinePath((*dir_it).second, (*entry_it).second.name);
		}
	}

	return "";
}

std::string Filesystem::FindFile(const std::string& name,
								 char const* exts[]) const {
	return FindFile("", name, exts);
}

std::string Filesystem::FindImage(const std::string& dir, const std::string& name) const {
#ifdef EMSCRIPTEN
	return FileFinder::FindDefault(*directory_tree, dir, name);
#endif

	static const char* IMG_TYPES[] = { ".bmp",  ".png", ".xyz", NULL };
	return game_filesystem->FindFile(dir, name, IMG_TYPES);
}

std::string Filesystem::FindDefault(const std::string& name) const {
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

	return FindDefault("", name);
}

std::string Filesystem::FindDefault(const std::string& dir, const std::string& name) const {
	static const char* no_exts[] = {"", NULL};
	return FindFile(dir, name, no_exts);
}

std::string Filesystem::FindMusic(const std::string& name) const {
#ifdef EMSCRIPTEN
	return FileFinder::FindDefault(*directory_tree, "Music", name);
#endif

	static const char* MUSIC_TYPES[] = {
			".opus", ".oga", ".ogg", ".wav", ".mid", ".midi", ".mp3", ".wma", nullptr };
	return FindFile("Music", name, MUSIC_TYPES);
}

std::string Filesystem::FindSound(const std::string& name) const {
#ifdef EMSCRIPTEN
	return FileFinder::FindDefault(*directory_tree, "Sound", name);
#endif

	static const char* SOUND_TYPES[] = {
			".opus", ".oga", ".ogg", ".wav", ".mp3", ".wma", nullptr };
	return FindFile("Sound", name, SOUND_TYPES);
}


std::string Filesystem::FindFont(const std::string& name) const {
	static const char* FONTS_TYPES[] = {
			".ttf", ".ttc", ".otf", ".fon", NULL, };
	std::string path = FindFile("Font", name, FONTS_TYPES);

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

bool Filesystem::IsValidProject() {
	return IsRPG2kProject() || IsEasyRpgProject();
}

bool Filesystem::IsRPG2kProject() {
	return !FindDefault(DATABASE_NAME).empty() && !FindDefault(TREEMAP_NAME).empty();
}

bool Filesystem::IsEasyRpgProject(){
	return !FindDefault(DATABASE_NAME_EASYRPG).empty() && !FindDefault(TREEMAP_NAME_EASYRPG).empty();
}
