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

#ifndef _EASYRPG_PLAYER_FILESYSTEM_OVERLAY_H_
#define _EASYRPG_PLAYER_FILESYSTEM_OVERLAY_H_

#include "filesystem.h"
#include "system.h"

/**
 * A virtual filesystem that combines multiple VFS into one.
 * The VFS are ordered by priority.
 * For file operations the VFS where the file exists is used and for directory
 * operations the union of all VFS is used.
 */
class OverlayFilesystem : public Filesystem {
public:
	/**
	 * Registers a filesystem to the overlay.
	 * Filesystems with a higher priority take precedence.
	 *
	 * @param fs Filesystem to add
	 * @param priority Priority of the added filesystem
	 * @return true when the filesystem was added, fails when the filesystem was invalid.
	 */
	bool AddFilesystem(FilesystemRef fs, int priority);

	/**
	 * Removes the passed filesystem from the overlay.
	 *
	 * @return true when the fs was removed
	 */
	bool RemoveFilesystem(FilesystemRef fs);

	/**
 	 * Implementation of abstract methods
 	 */
	/** @{ */
	std::string GetPath() const override;
	bool IsFile(const std::string& path) const override;
	bool IsDirectory(const std::string& path) const override;
	bool Exists(const std::string& path) const override;
	uint32_t GetFilesize(const std::string& path) const override;
	std::streambuf* CreateInputStreambuffer(const std::string& path, std::ios_base::openmode mode) const override;
	std::streambuf* CreateOutputStreambuffer(const std::string& path, std::ios_base::openmode mode) const override;
	std::vector<Filesystem::DirectoryEntry> ListDirectory(const std::string& path, bool* error = nullptr) const override;
	std::string FindFile(const std::string& dir,
						 const std::string& name,
						 char const* exts[]) const override;
	/** @} */

private:
	typedef struct {
		FilesystemRef fs;
		int priority;
	} OverlayEntry;

	std::vector<OverlayEntry> file_systems;
};


#endif
