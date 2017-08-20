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

#include "filesystem_physfs.h"
#include <physfs.h>
#include "utils.h"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <map>

namespace {
	static int physfs_count = 0;
	std::map<std::string, std::string> mounts;
}

class PhysFsFilesystem::PhysFsIStreambuf : public std::streambuf {
public:
	PhysFsIStreambuf(PHYSFS_File *physfs_stream, size_t filelength):
		physfs_stream(physfs_stream), m_buffer(bufsize),m_filelength(filelength),m_remaining(filelength) {
		// the buffer is empty at the start
		setg(&m_buffer[0], &m_buffer[0] + bufsize, &m_buffer[0] + bufsize);
	}
	virtual ~PhysFsIStreambuf() {
		PHYSFS_close(physfs_stream);
	}
	PhysFsIStreambuf(PhysFsIStreambuf const& other) = delete;
	PhysFsIStreambuf const& operator=(PhysFsIStreambuf const& other) = delete;

	virtual std::streambuf::int_type underflow() override {
		//either fill the buffer full, or with what is remaining
		size_t numberOfBytes = bufsize;
		if (m_remaining < bufsize) {
			numberOfBytes = m_remaining;
		}
		size_t bytesRead = PHYSFS_readBytes(physfs_stream, &m_buffer[0], numberOfBytes);
		if (bytesRead > 0) {
			// Now there are some bytes less remaining
			m_remaining = m_remaining - bytesRead;
			// And reset the bufferpositions
			setg(&m_buffer[0], &m_buffer[0], &m_buffer[0] + bytesRead);
			return traits_type::to_int_type(*gptr());
		}
		else {
			return traits_type::eof();
		}
	}

	virtual std::streambuf::pos_type seekoff(std::streambuf::off_type offset, std::ios_base::seekdir way, std::ios_base::openmode mode) override{
		std::streambuf::off_type position = 0;
		switch (way) {
			case std::ios_base::cur:
				position = (m_filelength - (m_remaining + (egptr() - gptr()))) + offset;
				break;
			case std::ios_base::beg:
				position = offset;
				break;
			case std::ios_base::end:
				position = m_filelength + offset;
				break;
		}
		if (position < 0) {
			position = 0;
		}
		else if (position >m_filelength) {
			position = m_filelength;
		}

		return seekpos(position, mode);
	}

	virtual std::streambuf::pos_type seekpos(std::streambuf::pos_type pos, std::ios_base::openmode mode) override {
		std::streambuf::pos_type position = pos;

		m_remaining = (m_filelength) - position;

		//empty buffer
		setg(&m_buffer[0], &m_buffer[0] + bufsize, &m_buffer[0] + bufsize);

		return PHYSFS_seek(physfs_stream, position);
	}

protected:
	std::streamoff m_filelength;
private:
	static const int bufsize = 1024;
	std::vector<char> m_buffer;
	std::streamoff m_remaining;
	PHYSFS_File* physfs_stream;
};

static std::string normalize_path(std::string const & path) {
	if (path == "." || path == "/" || path=="") return "";
	std::string inner_path = path;
	std::replace(inner_path.begin(), inner_path.end(), '\\', '/');
	if (inner_path.front() == '.') inner_path = inner_path.substr(1, inner_path.size() - 1);
	if (inner_path.front() == '/') inner_path = inner_path.substr(1, inner_path.size() - 1);
	return inner_path;
}

PhysFsFilesystem::PhysFsFilesystem(std::string const & os_path, std::string const & encoding) {
	//Open first entry of the input filebuffer pool
	static bool physfs_init = false;

	if (!physfs_init) {
		PHYSFS_init("easyrpg-player");
	}

	auto it = mounts.find(os_path);

	if (it == mounts.end()) {
		// Folder to use in the PhysFS folder hierarchy
		physfs_folder = "/" + Utils::ToString(physfs_count++);
		mounts[os_path] = physfs_folder;

		if (!PHYSFS_mount(os_path.c_str(), physfs_folder.c_str(), 0)) {
			fprintf(stderr, "%s\n", PHYSFS_getLastError());
			assert(false);
		}
	} else {
		physfs_folder = (*it).second;
	}

	// When in root determine if the path only contains one folder, go into the
	// subfolder because most archives start with a single directory
	char **rc = PHYSFS_enumerateFiles(ToPhysFsPath(".").c_str());
	char **i;
	PHYSFS_Stat fs_stat;

	int c;
	for (i = rc, c = 0; *i != NULL; i++, c++) {
		std::string full_path = std::string(ToPhysFsPath(".") + "/" + *i);
		PHYSFS_stat(full_path.c_str(), &fs_stat);

		bool is_dir = fs_stat.filetype == PHYSFS_FILETYPE_DIRECTORY;

		if (c == 1) {
			subfolder = "";
			break;
		}

		if (is_dir) {
			subfolder = Utils::ToString(*i) + "/";
		}
	}

	PHYSFS_freeList(rc);
}

PhysFsFilesystem::~PhysFsFilesystem() {
	// FIXME: Unmounting
}

bool PhysFsFilesystem::IsFile(std::string const & path) const {
	PHYSFS_Stat fs_stat;

	PHYSFS_stat(ToPhysFsPath(path).c_str(), &fs_stat);

	return fs_stat.filesize == PHYSFS_FILETYPE_REGULAR;
}

bool PhysFsFilesystem::IsDirectory(std::string const & path) const {
	PHYSFS_Stat fs_stat;

	PHYSFS_stat(ToPhysFsPath(path).c_str(), &fs_stat);

	return fs_stat.filetype == PHYSFS_FILETYPE_DIRECTORY;
}

bool PhysFsFilesystem::Exists(std::string const & path) const {
	return PHYSFS_exists(ToPhysFsPath(path).c_str()) != 0;
}

uint32_t PhysFsFilesystem::GetFilesize(std::string const & path) const {
	PHYSFS_Stat fs_stat;

	PHYSFS_stat(ToPhysFsPath(path).c_str(), &fs_stat);

	return fs_stat.filesize;
}

std::streambuf * PhysFsFilesystem::CreateInputStreambuffer(std::string const & path, std::ios_base::openmode mode) {
	PHYSFS_File* file = PHYSFS_openRead(ToPhysFsPath(path).c_str());

	assert(file);

	return new PhysFsIStreambuf(file, GetFilesize(path));
}

std::streambuf * PhysFsFilesystem::CreateOutputStreambuffer(std::string const & path, std::ios_base::openmode mode) {
	std::filebuf *buf = new std::filebuf();
	return buf->open(
#ifdef _MSC_VER
		Utils::ToWideString(path).c_str(),
#else
		path.c_str(),
#endif
		mode);
}

bool PhysFsFilesystem::ListDirectoryEntries(std::string const& path, ListDirectoryEntriesCallback callback) const {
	DirectoryEntry entry;

	char **rc = PHYSFS_enumerateFiles(ToPhysFsPath(path).c_str());
	char **i;
	PHYSFS_Stat fs_stat;

	int c;
	for (i = rc, c = 0; *i != NULL; i++, c++) {
		std::string full_path = std::string(ToPhysFsPath(path) + "/" + *i);
		PHYSFS_stat(full_path.c_str(), &fs_stat);

		entry.name = *i;
		entry.isDirectory = fs_stat.filetype == PHYSFS_FILETYPE_DIRECTORY;

		callback(this, entry);
	}

	PHYSFS_freeList(rc);

	return true;
}

std::string PhysFsFilesystem::ToPhysFsPath(const std::string &file) const {
	printf("ToPhys: %s\n", std::string(physfs_folder + "/" + subfolder + normalize_path(file)).c_str());
	return std::string(physfs_folder + "/" + subfolder + normalize_path(file));
}
