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

namespace FileFinder {
	class istream;
	struct DirectoryTree;
}

class Filesystem {
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

#if 0
	struct DirectoryTree {
		std::unordered_map<std::string, DirectoryEntry> files; // lowered path -> entry
		std::unordered_map<std::string, std::string> dirs; // lowered path -> real path
	} entry_cache;
#endif
	Filesystem() {}

	virtual ~Filesystem() {}

	/**
	 * Checks whether the path used to initialize the filesystem exists.
	 *
	 * @return If the filesystem path exists
	 */
	virtual bool IsValid();

	/**
	* Checks whether the passed path is a file
	*
	* @param path a path relative to the filesystems root
	*/
	virtual bool IsFile(std::string const & path) const=0;

	/**
	* Checks whether the passed path is a directory
	*
	* @param path a path relative to the filesystems root
	*/
	virtual bool IsDirectory(std::string const & path) const=0;

	/**
	* Checks whether the passed path is an existant file
	*
	* @param path a path relative to the filesystems root
	*/
	virtual bool Exists(std::string const & path) const=0;

	/**
	* Retrieves the size of the file on the given path
	*
	* @param path a path relative to the filesystems root
	*/
	virtual uint32_t GetFilesize(std::string const & path) const=0;

	/**
	 * Creates stream from UTF-8 file name for reading.
	 *
	 * @param name UTF-8 string file name.
	 * @param m stream mode.
	 * @return NULL if open failed.
	 */
	std::shared_ptr<std::istream> OpenInputStream(const std::string &name, std::ios_base::openmode m);

	/**
	* Allocates a streambuffer with input capabilities on the given path.
	* @param path a path relative to the filesystems root
	* @return A valid pointer to a streambuffer or a nullptr in case of failure.
	*/
	virtual std::streambuf* CreateInputStreambuffer(std::string const & path, std::ios_base::openmode mode) = 0;

	/**
	 * Creates stream from UTF-8 file name for writing.
	 *
	 * @param name UTF-8 string file name.
	 * @param m stream mode.
	 * @return NULL if open failed.
	 */
	std::shared_ptr<std::ostream> OpenOutputStream(const std::string &name, std::ios_base::openmode m);

	/**
	* Allocates a streambuffer with output capabilities on the given path.
	* @param path a path relative to the filesystems root
	* @return A valid pointer to a streambuffer or a nullptr in case of failure.
	*/
	virtual std::streambuf * CreateOutputStreambuffer(std::string const & path, std::ios_base::openmode mode) = 0;

	virtual std::vector<Filesystem::DirectoryEntry> ListDirectory(const std::string& path) const = 0;

	/**
	 * Static helper function which combines a directory path and an entry name to a concatenated Path
	 */
	static std::string CombinePath(std::string const & dir, std::string const & entry);

	/* File system helper functions not intended to be overwritten */
	std::string FindFile(const std::string& name,
						 char const* exts[]) const;

	std::string FindFile(const std::string& name) const;

	std::string FindFile(const std::string& dir,
						 const std::string& name) const;

	std::string FindFile(const std::string& dir,
						 const std::string& name,
						 char const* exts[]) const;

private:
	// lowered dir -> <map of> lowered file -> Entry
	mutable std::unordered_map<std::string, std::unordered_map<std::string, DirectoryEntry>> fs_cache;
	// lowered dir -> real dir
	mutable std::unordered_map<std::string, std::string> dir_cache;
};

#endif
