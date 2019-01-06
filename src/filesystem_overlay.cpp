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

#include "filesystem_overlay.h"
#include <algorithm>
#include <cstring>
#include <fstream>

std::string OverlayFilesystem::GetPath() const {
	if (file_systems.empty()) {
		return "";
	}

	return file_systems.front().fs->GetPath();
}
bool OverlayFilesystem::IsFile(const std::string& path) const {
	for (auto& entry : file_systems) {
		if (entry.fs->IsFile(path)) {
			return true;
		}
	}

	return false;
}

bool OverlayFilesystem::IsDirectory(const std::string& path) const {
	for (auto& entry : file_systems) {
		if (entry.fs->IsDirectory(path)) {
			return true;
		}
	}

	return false;
}

bool OverlayFilesystem::Exists(const std::string& path) const {
	for (auto& entry : file_systems) {
		if (entry.fs->Exists(path)) {
			return true;
		}
	}

	return false;
}

uint32_t OverlayFilesystem::GetFilesize(const std::string& path) const {
	for (auto& entry : file_systems) {
		if (entry.fs->Exists(path)) {
			return entry.fs->GetFilesize(path);
		}
	}

	return 0;
}

std::streambuf* OverlayFilesystem::CreateInputStreambuffer(const std::string& path, std::ios_base::openmode mode) {
	for (auto& entry : file_systems) {
		if (entry.fs->Exists(path)) {
			return entry.fs->CreateInputStreambuffer(path, mode);
		}
	}

	return nullptr;
}

std::streambuf* OverlayFilesystem::CreateOutputStreambuffer(const std::string& path, std::ios_base::openmode mode) {
	for (auto& entry : file_systems) {
		std::streambuf* stream = entry.fs->CreateOutputStreambuffer(path, mode);
		if (stream) {
			return stream;
		}
	}

	return nullptr;
}

std::vector<Filesystem::DirectoryEntry> OverlayFilesystem::ListDirectory(const std::string &path, bool* error) const {
	std::vector<Filesystem::DirectoryEntry> entries;

	bool err = false;

	for (auto& entry : file_systems) {
		if (entry.fs->Exists(path)) {
			// TODO: Naive merge algorithm, duplicate entries should be filtered
			auto fs_entries = entry.fs->ListDirectory(path, err ? nullptr : &err);
			std::copy(fs_entries.begin(), fs_entries.end(), std::back_inserter(entries));
		}
	}

	if (error) {
		*error = err && entries.empty();
	}

	return entries;
}

std::string OverlayFilesystem::FindFile(const std::string &dir, const std::string &name, const char **exts) const {
	for (auto& entry : file_systems) {
		std::string file = entry.fs->FindFile(dir, name, exts);
		if (!file.empty()) {
			return file;
		}
	}

	return "";
}

bool OverlayFilesystem::AddFilesystem(FilesystemRef fs, int priority) {
	if (!fs->IsValid()) {
		return false;
	}

	file_systems.push_back({fs, priority});
	std::sort(file_systems.begin(), file_systems.end(), [&] (OverlayEntry first, OverlayEntry second) {
		return first.priority > second.priority;
	});

	return true;
}

bool OverlayFilesystem::RemoveFilesystem(FilesystemRef fs) {
	size_t old_size = file_systems.size();
	file_systems.erase(
		std::remove_if(file_systems.begin(), file_systems.end(), [&] (OverlayEntry e) { return e.fs == fs; }));
	return old_size > file_systems.size();
}

