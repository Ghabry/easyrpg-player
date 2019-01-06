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

#ifndef _EASYRPG_PLAYER_FILESYSTEM_ZIP_H_
#define _EASYRPG_PLAYER_FILESYSTEM_ZIP_H_

#include "filesystem.h"
#include <fstream>
#include <memory>
#include <unordered_map>
#include <vector>

/**
 * A virtual filesystem that allows file/directory operations inside a ZIP archive.
 */
class ZIPFilesystem : public Filesystem {
public:

	/**
	 * Initializes a OS Filesystem inside the given ZIP File
	 * If you don't know the encoding, or want to know which sub path contains a certain file
	 * use the static function CheckIfContains
	 */
	ZIPFilesystem(const FilesystemRef source_fs, const std::string& fs_path, const std::string& encoding);

	~ZIPFilesystem();

	/**
 	 * Implementation of abstract methods
 	 */
	/** @{ */
	std::string GetPath() const override;
	bool IsFile(const std::string& path) const override;
	bool IsDirectory(const std::string& path) const override;
	bool Exists(const std::string& path) const override;
	uint32_t GetFilesize(const std::string& path) const override;
	std::streambuf* CreateInputStreambuffer(const std::string& path, std::ios_base::openmode mode) override;
	std::streambuf* CreateOutputStreambuffer(const std::string& path, std::ios_base::openmode mode) override;
	std::vector<Filesystem::DirectoryEntry> ListDirectory(const std::string& path, bool* error = nullptr) const override;
	/** @} */

private:
	enum class StorageMethod {Unknown,Plain,Deflate};
	struct ZipEntry {
		uint32_t filesize;
		uint32_t fileoffset;
		bool isDirectory;
	};

	struct StreamPoolEntry {
		std::streambuf* filebuffer;
		bool used;
	};
	class StorageIStreambuf;
	class DeflateIStreambuf;
	//No standard Constructor
	ZIPFilesystem() = delete;

	static bool FindCentralDirectory(std::istream & stream, uint32_t & offset, uint32_t & size, uint16_t & numberOfEntries);
	static bool ReadCentralDirectoryEntry(std::istream & zipfile, std::vector<char> & filepath, uint32_t & offset, uint32_t & uncompressed_size);
	static bool ReadLocalHeader(std::istream & zipfile, uint32_t & offset, StorageMethod & method,uint32_t & compressedSize);

	bool m_isValid;
	const FilesystemRef source_fs;
	std::string fs_path;
	std::vector<StreamPoolEntry*> m_InputPool;
	std::unordered_map<std::string, ZipEntry> m_zipContent;
};

#endif
