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
//#include "filesystem_native.h"
//#include "filesystem_zip.h"
#include "filefinder.h"
#include "utils.h"
#include "output.h"
#include "player.h"
#include <lcf/reader_util.h>
#include <algorithm>
#include <cassert>
#include <utility>

Filesystem::Filesystem(std::string base_path) : base_path(std::move(base_path)) {
	// no-op
};

Filesystem_Stream::InputStream Filesystem::OpenInputStream(const std::string &name, std::ios_base::openmode m) {
	std::streambuf* buf = CreateInputStreambuffer(name, m | std::ios_base::in);
	Filesystem_Stream::InputStream is(buf);
	return is;
}

Filesystem_Stream::OutputStream Filesystem::OpenOutputStream(const std::string &name, std::ios_base::openmode m) {
	std::streambuf* buf = CreateOutputStreambuffer(name, m | std::ios_base::out);
	Filesystem_Stream::OutputStream os(buf);
	return os;
}

void Filesystem::ClearCache() {
	dir_cache.clear();
	fs_cache.clear();
}

std::unique_ptr<Filesystem> Filesystem::Create(const std::string& path) {
	// Determine the proper file system to use
	std::unique_ptr<Filesystem> filesystem;

	if (!(Exists(path) || !IsDirectory(path, true))) {
		return std::unique_ptr<Filesystem>();
	}

	return std::make_unique<NativeFilesystem>(path);
}

bool Filesystem::IsValid() const {
	// FIXME: better way to do this?
	return Exists("");
}

std::string Filesystem::FindFile(const std::string& dir,
								 const std::string& name,
								 char const* exts[]) const {
	using namespace FileFinder;

#ifdef EMSCRIPTEN
	// The php filefinder should have given us an useable path
	std::string em_file = MakePath(dir, name);

	if (Exists(em_file))
		return em_file;
#endif

	std::string corrected_dir = lcf::ReaderUtil::Normalize(dir);
	std::string const escape_symbol = Player::escape_symbol;
	std::string corrected_name = lcf::ReaderUtil::Normalize(name);

	std::string combined_path = MakePath(corrected_dir, corrected_name);
	std::string canon = MakeCanonical(combined_path, 1);
	if (combined_path != canon) {
		// Very few games (e.g. Yume2kki) use path traversal (..) in the filenames to point
		// to files outside of the actual directory.
		// Fix the path and continue searching.
		size_t pos = canon.find_first_of("/");
		if (pos == std::string::npos) {
			for (char const** c = exts; *c != NULL; ++c) {
				std::string res = FindDefault(tree, canon + *c);
				if (!res.empty()) {
					return res;
				}
			}
			return "";
		} else {
			corrected_dir = canon.substr(0, pos);
			corrected_name = canon.substr(pos + 1);
		}
	}

#ifdef _WIN32
	std::replace(corrected_name.begin(), corrected_name.end(), '/', '\\');
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

	auto dir_it = dir_cache.find(corrected_dir);
	if (dir_it == dir_cache.end()) {
		assert(fs_cache.find(corrected_dir) == fs_cache.end());

		auto entries = ListDirectory(dir);

		dir_cache[corrected_dir] = dir;

		std::unordered_map<std::string, DirectoryEntry> fs_cache_entry;

		for (auto& entry : entries) {
			fs_cache_entry[Utils::LowerCase(entry.name)] = entry;
		}

		fs_cache.emplace(corrected_dir, fs_cache_entry);
	}

	dir_it = dir_cache.find(corrected_dir);
	auto it = fs_cache.find(corrected_dir);

	if (exts != nullptr) {
		for (char const **c = exts; *c != nullptr; ++c) {
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

	//Output::Debug("Cannot find: %s/%s", dir.c_str(), name.c_str());

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

std::string Filesystem::CombinePath(const std::string& dir, const std::string& entry) {
	std::string str = dir.empty() ? entry : dir + "/" + entry;
#ifdef _WIN32
	std::replace(str.begin(), str.end(), '/', '\\');
#else
	std::replace(str.begin(), str.end(), '\\', '/');
#endif
	return str;
}

std::string Filesystem::MakePathCanonical(const std::string& path, int initial_deepness) {
	std::vector<std::string> path_components = SplitPath(path);
	std::vector<std::string> path_can;

	for (const std::string& path_comp : path_components) {
		if (path_comp == "..") {
			if (!path_can.empty()) {
				path_can.pop_back();
			} else if (initial_deepness > 0) {
				// Ignore, we are in root
				--initial_deepness;
			} else {
				Output::Debug("Path traversal out of game directory: %s", path.c_str());
			}
		} else if (path_comp.empty() || path_comp == ".") {
			// ignore
		} else {
			path_can.push_back(path_comp);
		}
	}

	std::string ret;
	for (const std::string& s : path_can) {
		ret = Filesystem::CombinePath(ret, s);
	}

	return ret;
}

std::vector<std::string> Filesystem::SplitPath(const std::string& path) {
	// Tokens are patch delimiters ("/" and encoding aware "\")
	std::function<bool(char32_t)> f = [](char32_t t) {
		char32_t escape_char_back = '\0';
		if (!Player::escape_symbol.empty()) {
			escape_char_back = Utils::DecodeUTF32(Player::escape_symbol).front();
		}
		char32_t escape_char_forward = Utils::DecodeUTF32("/").front();
		return t == escape_char_back || t == escape_char_forward;
	};
	return Utils::Tokenize(path, f);
}

std::string Filesystem::GetPathInsidePath(const std::string& path_to, const std::string& path_in) {
	if (!ToStringView(path_in).starts_with(path_to)) {
		return path_in;
	}

	std::string path_out = path_in.substr(path_to.size());
	if (!path_out.empty() && (path_out[0] == '/' || path_out[0] == '\\')) {
		path_out = path_out.substr(1);
	}

	return path_out;
}
