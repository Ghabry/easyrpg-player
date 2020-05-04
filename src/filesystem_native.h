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

#ifndef EP_FILESYSTEM_OS_H
#define EP_FILESYSTEM_OS_H

#include "filesystem.h"

/**
 * A virtual filesystem that represents the file system of the host system.
 */
class NativeFilesystem : public Filesystem {
public:
	/**
	 * Initializes a OS Filesystem on the given os path
	 */
	NativeFilesystem(const std::string& base_path);

protected:
	/**
 	 * Implementation of abstract methods
 	 */
	/** @{ */
	bool IsFileImpl(const std::string& path) const override;
	bool IsDirectoryImpl(const std::string& path, bool follow_symlinks) const override;
	bool ExistsImpl(const std::string& path) const override;
	int64_t GetFilesizeImpl(const std::string& path) const override;
	std::streambuf* CreateInputStreambufferImpl(const std::string& path, std::ios_base::openmode mode) override;
	std::streambuf* CreateOutputStreambufferImpl(const std::string& path, std::ios_base::openmode mode) override;
	std::vector<Filesystem::DirectoryEntry> ListDirectoryImpl(const std::string& path, bool* error = nullptr) const override;
	FilesystemRef CloneImpl() const override;
	/** @} */
};


#endif
