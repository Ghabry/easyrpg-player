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

#ifndef _EASYRPG_PLAYER_FILESYSTEM_H_
#define _EASYRPG_PLAYER_FILESYSTEM_H_

#include <string>
#include <ios>
#include <istream>
#include <memory>
#include <unordered_map>
#include <vector>

#include "memory_management.h"

namespace {
	FilesystemRef game_filesystem;
}

class Filesystem : public std::enable_shared_from_this<Filesystem> {
public:
	enum class FileType {
		Regular,
		Directory,
		Other,
		Invalid
	};

	/**
	 * A entry of a directory (eg. file or subdir)
	 */
	struct DirectoryEntry {
		std::string name;
		FileType type;
	};

	using DirectoryIterator = std::vector<DirectoryEntry>::const_iterator;

	/**
	 * Checks whether the path used to initialize the filesystem exists.
	 *
	 * @return If the filesystem is valid
	 */
	virtual bool IsValid();

	/**
	 * Checks whether the passed path is a file.
	 * This function is case sensitive on some platforms.
	 *
	 * @param path a path relative to the filesystems root
	 */
	virtual bool IsFile(const std::string& path) const = 0;

	/**
	 * Checks whether the passed path is a directory.
	 * This function is case sensitive on some platforms.
	 *
	 * @param path a path relative to the filesystems root
	 */
	virtual bool IsDirectory(const std::string& path) const = 0;

	/**
	 * Checks whether the passed path is an existant file.
	 * This function is case sensitive on some platforms.
	 *
	 * @param path a path relative to the filesystems root
	 */
	virtual bool Exists(const std::string& path) const = 0;

	/**
	 * Retrieves the size of the file on the given path.
	 *
	 * @param path a path relative to the filesystems root
	 */
	virtual uint32_t GetFilesize(const std::string& path) const = 0;

	/**
	 * Creates stream from UTF-8 file name for reading.
	 *
	 * @param name UTF-8 string file name.
	 * @param m stream mode.
	 * @return NULL if open failed.
	 */
	std::shared_ptr<std::istream> OpenInputStream(const std::string &name, std::ios_base::openmode m) const;

	/**
	 * Allocates a streambuffer with input capabilities on the given path.
	 *
	 * @param path a path relative to the filesystems root
	 * @return A valid pointer to a streambuffer or a nullptr in case of failure.
	 */
	virtual std::streambuf* CreateInputStreambuffer(const std::string& path, std::ios_base::openmode mode) const = 0;

	/**
	 * Creates stream from UTF-8 file name for writing.
	 *
	 * @param name UTF-8 string file name.
	 * @param m stream mode.
	 * @return NULL if open failed.
	 */
	std::shared_ptr<std::ostream> OpenOutputStream(const std::string &name, std::ios_base::openmode m) const;

	/**
	 * Allocates a streambuffer with output capabilities on the given path.
	 *
	 * @param path a path relative to the filesystems root
	 * @return A valid pointer to a streambuffer or a nullptr in case of failure.
	 */
	virtual std::streambuf* CreateOutputStreambuffer(const std::string& path, std::ios_base::openmode mode) const = 0;

	/**
	 * Returns a directory listing of the given path.
	 *
	 * @param path a path relative to the filesystems root
	 * @return List of directory entries
	 */
	virtual std::vector<Filesystem::DirectoryEntry> ListDirectory(const std::string& path) const = 0;

	/**
	 * Creates a new appropriate filesystem from the specified path.
	 * The path is processed to initialize the proper virtual filesystem handler.
	 *
	 * @param p Virtual path to use
	 * @return FilesystemRef when the parsing was successful, otherwise nullptr
	 */
	FilesystemRef Create(const std::string& p);

	// Helper functions for finding files in a case insensitive way
	/**
	 * Does a case insensitive search for the file.
	 * The name may contain a path.
	 *
	 * @param name a path relative to the filesystem root
	 * @param exts List of file extensions to probe (null-terminated)
	 * @return Path to file or empty string when not found
	 */
	std::string FindFile(const std::string& name,
						 char const* exts[]) const;

	/**
	 * Does a case insensitive search for the file.
	 * The name may contain a path.
	 *
	 * @param name a path relative to the filesystem root
	 * @return Path to file or empty string when not found
	 */
	std::string FindFile(const std::string& name) const;

	/**
	 * Does a case insensitive search for the file in a specific
	 * directory.
	 *
	 * @param dir a path relative to the filesystem root
	 * @param name Name of the file to search
	 * @return Path to file or empty string when not found
	 */
	std::string FindFile(const std::string& dir,
						 const std::string& name) const;

	/**
	 * Does a case insensitive search for the file in a specific
	 * directory.
	 * General purpose version of FindFile.
	 *
	 * @param dir a path relative to the filesystem root
	 * @param name Name of the file to search
	 * @param exts List of file extensions to probe (null-terminated)
	 * @return Path to file or empty string when not found
	 */
	virtual std::string FindFile(const std::string& dir,
						 const std::string& name,
						 char const* exts[]) const;

	// Static helper functions
	/**
	 * Combines a directory path and an entry name to a concatenated Path
	 */
	static std::string CombinePath(const std::string& dir, const std::string& entry);

	/**
	 * Converts a path to the canonical equivalent.
	 * This generates a path that does not contain ".." or "." directories.
	 *
	 * @param path Path to normalize
	 * @param initial_deepness How deep the passed path is relative to the game root
	 * @return canonical path
	 */
	static std::string MakePathCanonical(const std::string& path, int initial_deepness);

	/**
	 * Splits a path in it's components.
	 *
	 * @param path Path to split
	 * @return Vector containing path components
	 */
	static std::vector<std::string> SplitPath(const std::string& path);

	/**
	 * Returns the part of "path_in" that is inside "path_to".
	 * e.g. Input: /h/e/game, /h/e/game/Music/a.wav; Output: Music/a.wav
	 *
	 * @param path_to Path to a primary folder of path_in
	 * @param path_in Absolute path to the file, must start with path_to
	 *
	 * @return The part of path_in that is inside path_to. path_in when the path is not in path_to
	 */
	static std::string GetPathInsidePath(const std::string& path_to, const std::string& path_in);

protected:
	// lowered dir -> <map of> lowered file -> Entry
	mutable std::unordered_map<std::string, std::unordered_map<std::string, DirectoryEntry>> fs_cache;
	// lowered dir -> real dir
	mutable std::unordered_map<std::string, std::string> dir_cache;
};

#endif
