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

#ifndef _EASYRPG_PLAYER_FILESYSTEM_OS_H_
#define _EASYRPG_PLAYER_FILESYSTEM_OS_H_

#include "filesystem.h"

/**
 * A virtual filesystem that represents the file system of the host system.
 */
class OSFilesystem : public Filesystem {
public:
	/**
	 * Initializes a OS Filesystem on the given os path
	 */
	OSFilesystem(const std::string& rootPath);

	/**
 	 * Implementation of abstract methods
 	 */
	/** @{ */
	bool IsFile(const std::string& path) const override;
	bool IsDirectory(const std::string& path) const override;
	bool Exists(const std::string& path) const override;
	uint32_t GetFilesize(const std::string& path) const override;
	std::streambuf* CreateInputStreambuffer(const std::string& path, std::ios_base::openmode mode) const override;
	std::streambuf* CreateOutputStreambuffer(const std::string& path, std::ios_base::openmode mode) const override;
	std::vector<Filesystem::DirectoryEntry> ListDirectory(const std::string& path, bool* error = nullptr) const override;
	/** @} */

private:
	std::string m_rootPath;
};


#endif
