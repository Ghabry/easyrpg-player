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

#include "filesystem_os.h"
#include "filesystem_zip.h"
#include "filesystem_view.h"

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

std::shared_ptr<std::istream> Filesystem::OpenInputStream(const std::string &name, std::ios_base::openmode m) const {
	std::streambuf* buf = CreateInputStreambuffer(name, m);

	std::shared_ptr<std::istream> ret(new std::istream(buf));

	return (*ret) ? ret : std::shared_ptr<std::istream>();
}

std::shared_ptr<std::ostream> Filesystem::OpenOutputStream(const std::string &name, std::ios_base::openmode m) const {
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

std::string Filesystem::FindFile(const std::string& dir,
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
		assert(lower_dir == "");
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

	if (exts != nullptr) {
		for (char const **c = exts; *c != NULL; ++c) {
			auto entry_it = (*it).second.find(corrected_name + *c);
			if (entry_it != (*it).second.end()) {
				return Filesystem::CombinePath((*dir_it).second, (*entry_it).second.name);
			}
		}
	} else {
		auto entry_it = (*it).second.find(corrected_name);
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

std::string Filesystem::FindFile(const std::string& name) const {
	return FindFile("", name, nullptr);
}

std::string Filesystem::FindFile(const std::string& dir,
								 const std::string& name) const {
	return FindFile(dir, name, nullptr);
}

FilesystemRef Filesystem::Create(const std::string& path) {
	// Determine the proper file system to use
	FilesystemRef filesystem;

	// When the path doesn't exist check if the path contains a file that can
	// be handled by another filesystem
	std::string path_prefix = "";

	if (!IsDirectory(path)) {
		std::vector<std::string> components = FileFinder::SplitPath(path);

		// TODO this should probably move to a static function in the FS classes

		// search until ".zip", "do magic"
		std::string internal_path;
		bool handle_internal = false;
		for (std::string comp : components) {
			if (handle_internal) {
				internal_path += comp + "/";
			} else {
				path_prefix += comp + "/";
				if (Utils::EndsWith(comp, ".zip")) {
					path_prefix.pop_back();
					handle_internal = true;
				}
			}
		}

		if (!internal_path.empty()) {
			internal_path.pop_back();
		}

		filesystem = std::make_shared<ZIPFilesystem>(shared_from_this(), path_prefix, "windows-1252");
		if (!filesystem->IsValid()) {
			return FilesystemRef();
		} else if (!internal_path.empty()) {
			filesystem = filesystem->Create(internal_path);
		}
	} else {
		// Handle as a normal path in the local filesystem
		filesystem = std::make_shared<ViewFilesystem>(shared_from_this(), path);
		if (!(filesystem->Exists(path) || !filesystem->IsDirectory(path))) { return FilesystemRef(); }
	}

	return filesystem;
}
