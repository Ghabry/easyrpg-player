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

#include "filesystem_native.h"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#ifdef _WIN32
#  include <windows.h>
#  include <shlobj.h>
#endif

#if defined(USE_SDL) && defined(__ANDROID__)
#   include <jni.h>
#   include <SDL_system.h>
#endif

#include "system.h"
#include "options.h"
#include "utils.h"
#include "filefinder.h"
#include "output.h"
#include "player.h"
#include "registry.h"
#include "rtp.h"
#include "main_data.h"
#include "reader_util.h"
#include "platform.h"

#ifdef USE_LIBRETRO
#include "platform/libretro/libretro_ui.h"
#endif

NativeFilesystem::NativeFilesystem(const std::string& base_path): Filesystem(base_path) {
}

bool NativeFilesystem::IsFileImpl(const std::string& path) const {
	return Platform::File(path).IsFile(false);
}

bool NativeFilesystem::IsDirectoryImpl(const std::string& dir, bool follow_symlinks) const {
	return Platform::File(dir).IsDirectory(follow_symlinks);
}

bool NativeFilesystem::ExistsImpl(const std::string& filename) const {
	return Platform::File(filename).Exists();
}

int64_t NativeFilesystem::GetFilesizeImpl(const std::string& path) const {
	return Platform::File(path).GetSize();
}

std::streambuf* NativeFilesystem::CreateInputStreambufferImpl(const std::string& path, std::ios_base::openmode mode) {
	std::filebuf *buf = new std::filebuf();

	return buf->open(
#ifdef _MSC_VER
			Utils::ToWideString(path).c_str(),
#else
			path.c_str(),
#endif
			mode);
}

std::streambuf* NativeFilesystem::CreateOutputStreambufferImpl(const std::string& path, std::ios_base::openmode mode) {
	std::filebuf *buf = new std::filebuf();
	return buf->open(
#ifdef _MSC_VER
			Utils::ToWideString(path).c_str(),
#else
			path.c_str(),
#endif
			mode);
}

std::vector<Filesystem::DirectoryEntry> NativeFilesystem::ListDirectoryImpl(const std::string &path, bool* error) const {
	std::vector<Filesystem::DirectoryEntry> entries;

	if (error) {
		*error = false;
	}

	DirectoryEntry result;

	Platform::Directory dir(path);
	if (!dir) {
		Output::Debug("Error opening dir %s: %s", path.c_str(),
					  ::strerror(errno));
		if (error) {
			*error = true;
		}
		return entries;
	}

	while (dir.Read()) {
		const std::string name = dir.GetEntryName();
		Platform::FileType type = dir.GetEntryType();

		static bool has_fast_dir_stat = true;
		bool is_directory = false;
		if (has_fast_dir_stat) {
			if (type == Platform::FileType::Unknown) {
				has_fast_dir_stat = false;
			} else {
				is_directory = type == Platform::FileType::Directory;
			}
		}

		if (!has_fast_dir_stat) {
			is_directory = IsDirectory(CombinePath(path, name), true);
		}

		if (name == "." || name == "..") {
			continue;
		}

		result.type = is_directory ? FileType::Directory : FileType::Regular;
		result.name = name;

		entries.push_back(result);

#if 0
		// FIXME
		std::string name_norm = ReaderUtil::Normalize(name);
		if (is_directory) {
			if (result.directories.find(name_norm) != result.directories.end()) {
				Output::Warning("This game provides the folder \"%s\" twice.", name.c_str());
				Output::Warning("This can lead to file not found errors. Merge the directories manually in a file browser.");
			}
			result.directories[name_norm] = name;
		} else {
			result.files[name_norm] = name;
		}
#endif
	}

	return entries;
}

FilesystemRef NativeFilesystem::CloneImpl() const {
	return std::make_shared<NativeFilesystem>(GetPath());
}
