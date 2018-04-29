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

#include "filesystem_view.h"
#include "filefinder.h"
#include <cassert>
#include <cstring>
#include <functional>
#include <fstream>

ViewFilesystem::ViewFilesystem(FilesystemRef wrapped_filesystem, const std::string& subpath) :
	wrapped_fs(wrapped_filesystem), subpath(subpath) {
	assert(wrapped_filesystem && "wrapped_fs arg is null");
}

static std::string translate_path(const std::string& first, const std::string& second) {
	std::string second_c = Filesystem::MakePathCanonical(second, 0);
	if (second_c.empty()) {
		return first;
	}
	return Filesystem::CombinePath(first, second_c);
}

bool ViewFilesystem::IsFile(const std::string& path) const {
	return wrapped_fs->IsFile(translate_path(subpath, path));
}

bool ViewFilesystem::IsDirectory(const std::string& path) const {
	return wrapped_fs->IsDirectory(translate_path(subpath, path));
}

bool ViewFilesystem::Exists(const std::string& path) const {
	return wrapped_fs->Exists(translate_path(subpath, path));
}

uint32_t ViewFilesystem::GetFilesize(const std::string& path) const {
	return wrapped_fs->GetFilesize(translate_path(subpath, path));
}

std::streambuf* ViewFilesystem::CreateInputStreambuffer(const std::string& path, std::ios_base::openmode mode) const {
	return wrapped_fs->CreateInputStreambuffer(translate_path(subpath, path), mode);
}

std::streambuf* ViewFilesystem::CreateOutputStreambuffer(const std::string& path, std::ios_base::openmode mode) const {
	return wrapped_fs->CreateOutputStreambuffer(translate_path(subpath, path), mode);
}

std::vector<Filesystem::DirectoryEntry> ViewFilesystem::ListDirectory(const std::string &path) const {
	return wrapped_fs->ListDirectory(translate_path(subpath, path));
}
