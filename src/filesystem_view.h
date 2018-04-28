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

#ifndef _EASYRPG_PLAYER_FILESYSTEM_VIEW_H_
#define _EASYRPG_PLAYER_FILESYSTEM_VIEW_H_

#include "filesystem.h"
#include "system.h"

/**
 * A virtual filesystem that maintains a view on a subfolder of another filesystem.
 * All operations are forwarded into the subfolder of the filesystem.
 */
class ViewFilesystem : public Filesystem {
public:
	/**
	 * Initializes a View filesystem.
	 */
	ViewFilesystem(FilesystemRef wrapped_filesystem, const std::string& path);

    ~ViewFilesystem();

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
	std::streambuf * CreateInputStreambuffer(std::string const & path, std::ios_base::openmode mode) const override;

	/**
	* Allocates a streambuffer with output capabilities on the given path.
	* @param path a path relative to the filesystems root
	* @return A valid pointer to a streambuffer or a nullptr in case of failure.
	*/
	std::streambuf * CreateOutputStreambuffer(std::string const & path, std::ios_base::openmode mode) const override;

	std::vector<Filesystem::DirectoryEntry> ListDirectory(const std::string& path) const override;

private:
	FilesystemRef wrapped_fs;
	std::string subpath;
};

#endif
