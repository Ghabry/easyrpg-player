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

#ifndef EP_FILESYSTEM_H
#define EP_FILESYSTEM_H

// Headers
#include <istream>
#include <ostream>
#include <unordered_map>
#include <vector>
#include <cassert>
#include "system.h"
#include "filesystem_stream.h"

class Filesystem {
public:
	enum class FileType {
		Regular,
		Directory,
		Other
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
	bool IsValid() const;

	/**
	 * Return the path used to initialize the filesystem.
	 * Passing this string to Create of the parent filesystem must result
	 * in a filesystem pointing to the same folder/file as this filesystem.
	 *
	 * @return filesystem root path
	 */
	std::string GetPath() const;

	/**
	 * Checks whether the passed path is a file.
	 * This function is case sensitive on some platforms.
	 *
	 * @param path a path relative to the filesystems root
	 */
	bool IsFile(const std::string& path) const;

	/**
	 * Checks whether the passed path is a directory.
	 * This function is case sensitive on some platforms.
	 *
	 * @param path a path relative to the filesystems root
	 */
	bool IsDirectory(const std::string& path, bool follow_symlinks) const;

	/**
	 * Checks whether the passed path is an existant file.
	 * This function is case sensitive on some platforms.
	 *
	 * @param path a path relative to the filesystems root
	 */
	bool Exists(const std::string& path) const;

	/**
	 * Retrieves the size of the file on the given path.
	 *
	 * @param path a path relative to the filesystems root
	 */
	int64_t GetFilesize(const std::string& path) const;

	/**
	 * Creates stream from UTF-8 file name for reading.
	 *
	 * @param name UTF-8 string file name.
	 * @param m stream mode.
	 * @return NULL if open failed.
	 */
	Filesystem_Stream::InputStream OpenInputStream(const std::string &name, std::ios_base::openmode m = (std::ios_base::openmode)0);

	/**
	 * Allocates a streambuffer with input capabilities on the given path.
	 *
	 * @param path a path relative to the filesystems root
	 * @return A valid pointer to a streambuffer or a nullptr in case of failure.
	 */
	std::streambuf* CreateInputStreambuffer(const std::string& path, std::ios_base::openmode mode);

	/**
	 * Creates stream from UTF-8 file name for writing.
	 *
	 * @param name UTF-8 string file name.
	 * @param m stream mode.
	 * @return NULL if open failed.
	 */
	Filesystem_Stream::OutputStream OpenOutputStream(const std::string &name, std::ios_base::openmode m = (std::ios_base::openmode)0);

	/**
	 * Allocates a streambuffer with output capabilities on the given path.
	 *
	 * @param path a path relative to the filesystems root
	 * @return A valid pointer to a streambuffer or a nullptr in case of failure.
	 */
	std::streambuf* CreateOutputStreambuffer(const std::string& path, std::ios_base::openmode mode);

	/**
	 * Returns a directory listing of the given path.
	 *
	 * @param path a path relative to the filesystems root
	 * @param error When non-null receives true when reading failed, otherwise false
	 * @return List of directory entries
	 */
	std::vector<Filesystem::DirectoryEntry> ListDirectory(const std::string& path, bool* error = nullptr) const;

	/**
	 * Clears the filesystem cache. Changes in the filesystem become visible
	 * to the FindFile functions.
	 * This is automatically called when an output stream is closed.
	 */
	void ClearCache();

	/**
	 * Creates a new appropriate filesystem from the specified path.
	 * The path is processed to initialize the proper virtual filesystem handler.
	 *
	 * @param p Virtual path to use
	 * @param need_copy
	 * @return FilesystemRef when the parsing was successful, otherwise nullptr
	 */
	static std::unique_ptr<Filesystem> Create(const std::string& p);

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
	explicit Filesystem(std::string base_path);

	/**
 	 * Abstract methods to be implemented by filesystems.
 	 * The path is already adjusted to the filesystem base.
 	 */
	/** @{ */
	virtual bool IsFileImpl(const std::string& path) const = 0;
	virtual bool IsDirectoryImpl(const std::string& path, bool follow_symlinks) const = 0;
	virtual bool ExistsImpl(const std::string& path) const = 0;
	virtual int64_t GetFilesizeImpl(const std::string& path) const = 0;
	virtual std::streambuf* CreateInputStreambufferImpl(const std::string& path, std::ios_base::openmode mode) = 0;
	virtual std::streambuf* CreateOutputStreambufferImpl(const std::string& path, std::ios_base::openmode mode) = 0;
	virtual std::vector<Filesystem::DirectoryEntry> ListDirectoryImpl(const std::string& path, bool* error = nullptr) const = 0;
	/** @} */

	// lowered dir -> <map of> lowered file -> Entry
	mutable std::unordered_map<std::string, std::unordered_map<std::string, DirectoryEntry>> fs_cache;
	// lowered dir -> real dir
	mutable std::unordered_map<std::string, std::string> dir_cache;

private:
	std::string base_path;
	std::string sub_dir;
};

inline std::string Filesystem::GetPath() const {
	return base_path;
}

inline bool Filesystem::IsFile(const std::string& path) const {
	return IsFileImpl(CombinePath(sub_dir, path));
}

inline bool Filesystem::IsDirectory(const std::string& path, bool follow_symlinks) const {
	return IsDirectoryImpl(CombinePath(sub_dir, path), follow_symlinks);
}

inline bool Filesystem::Exists(const std::string& path) const {
	return ExistsImpl(CombinePath(sub_dir, path));
}

inline int64_t Filesystem::GetFilesize(const std::string& path) const {
	return GetFilesizeImpl(CombinePath(sub_dir, path));
}

inline std::streambuf* Filesystem::CreateInputStreambuffer(const std::string& path, std::ios_base::openmode mode) {
	return CreateInputStreambufferImpl(CombinePath(sub_dir, path), mode);
}

inline std::streambuf* Filesystem::CreateOutputStreambuffer(const std::string& path, std::ios_base::openmode mode) {
	return CreateOutputStreambufferImpl(CombinePath(sub_dir, path), mode);
}

inline std::vector<Filesystem::DirectoryEntry> Filesystem::ListDirectory(const std::string& path, bool* error) const {
	return ListDirectoryImpl(CombinePath(sub_dir, path), error);
}

#endif
