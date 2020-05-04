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
	 * Initializes a filesystem inside the given ZIP File
	 *
	 * @param source_fs Filesystem used to create handles on the zip file
	 * @param base_path Path passed to source_fs to open the zip file
	 * @param encoding Encoding to use, use empty string for autodetection
	 */
	ZIPFilesystem(const FilesystemRef source_fs, const std::string& base_path, const std::string& encoding = "");

	~ZIPFilesystem();

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
	mutable std::vector<StreamPoolEntry*> m_InputPool;
	std::unordered_map<std::string, ZipEntry> m_zipContent;
};

#endif
