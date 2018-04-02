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

class OverlayFilesystem : public Filesystem {
public:
	/**
	 * Initializes an overlay filesystem.
	 */
	OverlayFilesystem();

    ~OverlayFilesystem();

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
	* Checks whether the passed path is a file
	*
	* @param path a path relative to the filesystems root
	*/
	bool IsFile(std::string const & path) const override;

	/**
	* Checks whether the passed path is a directory
	*
	* @param path a path relative to the filesystems root
	*/
	bool IsDirectory(std::string const & path) const override;

	/**
	* Checks whether the passed path is an existant file
	*
	* @param path a path relative to the filesystems root
	*/
	bool Exists(std::string const & path) const override;

	/**
	* Retrieves the size of the file on the given path
	*
	* @param path a path relative to the filesystems root
	*/
	uint32_t GetFilesize(std::string const & path) const override;

	/**
	* Allocates a streambuffer with input capabilities on the given path.
	* @param path a path relative to the filesystems root
	* @return A valid pointer to a streambuffer or a nullptr in case of failure.
	*/
	std::streambuf * CreateInputStreambuffer(std::string const & path, std::ios_base::openmode mode) override;

	/**
	* Allocates a streambuffer with output capabilities on the given path.
	* @param path a path relative to the filesystems root
	* @return A valid pointer to a streambuffer or a nullptr in case of failure.
	*/
	std::streambuf * CreateOutputStreambuffer(std::string const & path, std::ios_base::openmode mode) override;

	std::vector<Filesystem::DirectoryEntry> ListDirectory(const std::string& path) const override;

private:
	typedef struct {
		FilesystemRef fs;
		int priority;
	} OverlayEntry;

	std::vector<OverlayEntry> file_systems;
};


#endif
