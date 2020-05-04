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

#ifndef EP_FILESYSTEM_RTP_H
#define EP_FILESYSTEM_RTP_H

#include "filesystem.h"
#include "system.h"

/**
 * A special virtual filesystem for the Runtime Package (RTP) that wraps another VFS and attempts to find a requested
 * file using a translation table when the requested file is missing.
 * Only file operations are translated, directory reading is passed through without any translation logic.
 */
class RtpFilesystem : public Filesystem {
public:
	/**
	 * Adds RTP paths to the file finder
	 *
	 * @param disable_rtp When true disables RTP handling in the FileFinder
	 * @param disable_warnings When true disables warnings about missing RTP files	 */
	static void InitRtpPaths(bool disable_rtp = false, bool disable_warnings = false);

	/**
	 * Initializes a RTP filesystem.
	 */
	RtpFilesystem(FilesystemRef wrapped_filesystem);

	/**
 	 * Implementation of abstract methods
 	 */
	/** @{ */
	std::string GetPath() const override;
	bool IsFile(const std::string& path) const override;
	bool IsDirectory(const std::string& path, bool follow_symlinks) const override;
	bool Exists(const std::string& path) const override;
	int64_t GetFilesize(const std::string& path) const override;
	std::streambuf* CreateInputStreambuffer(const std::string& path, std::ios_base::openmode mode) override;
	std::streambuf* CreateOutputStreambuffer(const std::string& path, std::ios_base::openmode mode) override;
	std::vector<Filesystem::DirectoryEntry> ListDirectory(const std::string& path, bool* error = nullptr) const override;
	std::string FindFile(const std::string& dir,
						 const std::string& name,
						 char const* exts[]) const override;
	/** @} */

private:
	FilesystemRef wrapped_fs;
};

#endif
