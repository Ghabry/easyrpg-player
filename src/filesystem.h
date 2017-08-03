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

class Filesystem {
public:

	/**
	 * A entry of a directory (eg. file or subdir)
	 */
	struct DirectoryEntry {
		std::string name;
		bool isDirectory;
	};

	/**
	 * Callback which is used by ListDirectoryEntries to transmit found entries
	 */
	typedef void(*ListDirectoryEntriesCallback)(Filesystem const * filesystem,DirectoryEntry const &);


	Filesystem() {}

	virtual ~Filesystem() {}
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
	* Allocates a streambuffer with input capabilities on the given path.
	* @param path a path relative to the filesystems root
	* @return A valid pointer to a streambuffer or a nullptr in case of failure.
	*/
	virtual std::streambuf * CreateInputStreambuffer(std::string const & path, std::ios_base::openmode mode) = 0;

	/**
	* Allocates a streambuffer with output capabilities on the given path.
	* @param path a path relative to the filesystems root
	* @return A valid pointer to a streambuffer or a nullptr in case of failure.
	*/
	virtual std::streambuf * CreateOutputStreambuffer(std::string const & path, std::ios_base::openmode mode) = 0;

	/**
	* Calls the provided callback for every entry in the directory dir
	*
	* @param path directory to list members.
	* @param callback the cllback function to invoke when an entry is found
	* @return whether the operation was successful.
	*/
	virtual bool ListDirectoryEntries(std::string const& path, ListDirectoryEntriesCallback callback) const = 0;

	/**
	 * Static helper function which combines a directory path and an entry name to a concatenated Path
	 */
	static std::string CombinePath(std::string const & dir, std::string const & entry);
};


#endif
