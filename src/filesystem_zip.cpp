#include "filesystem_zip.h"
#include <zlib.h>
#include "utils.h"
#include <iostream>

static const uint32_t endOfCentralDirectory = 0x06054b50;
static const int32_t endOfCentralDirectorySize = 22;

static const uint32_t centralDirectoryEntry = 0x02014b50;

ZIPFilesystem::ZIPFilesystem(std::string os_path, uint32_t zipFileSize) {
	//Open first entry of the input filebuffer pool
	m_isValid = false;
	StreamPoolEntry initialEntry;
	initialEntry.filebuffer = new std::filebuf();
	initialEntry.used = true;
	initialEntry.filebuffer->open(os_path, std::ios::ios_base::in | std::ios::ios_base::binary);
	
	uint16_t centralDirectoryEntries = 0;
	uint32_t centralDirectorySize = 0;
	uint32_t centralDirectoryOffset = 0;

	ZipEntry entry;
	std::string filepath = "";

	std::istream zipfile(initialEntry.filebuffer); //Take the first streambuffer of the pool

	if (FindCentralDirectory(zipfile,centralDirectoryOffset, centralDirectorySize, centralDirectoryEntries)) {
		zipfile.seekg(centralDirectoryOffset); //Seek to the start of the central directory
		while (ReadCentralDirectoryEntry(zipfile, filepath, entry.fileoffset, entry.filesize)) {
			if (filepath.back() == '/') {
				entry.isDirectory = true;
				filepath=filepath.substr(0, filepath.size() - 1);
			}
			//zip is case insensitive, so store only the lower case
			m_zipContent.insert(std::pair<std::string,ZipEntry>(Utils::LowerCase(filepath), entry));
		}

		zipfile.seekg(0);
		initialEntry.used = false;
		m_InputPool.push_back(initialEntry);
		m_isValid = true;
	}
	else {
		delete initialEntry.filebuffer;
	}
}

bool ZIPFilesystem::FindCentralDirectory(std::istream & zipfile, uint32_t & offset, uint32_t & size, uint16_t & numberOfEntries) {
	uint32_t magic = 0;
	bool found = false;


	zipfile.seekg(-endOfCentralDirectorySize, std::ios::ios_base::end); //seek to the first position where the endOfCentralDirectory Signature may occur

	//The only variable length field in the end of central directory is the comment which has a maximum length of UINT16_MAX - so if we seek longer, this is no zip file
	for (size_t i = 0; i < UINT16_MAX&&zipfile.good() && !found; i++) {
		zipfile.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		Utils::SwapByteOrder(magic); //Take care of big endian systems
		if (magic == endOfCentralDirectory) {
			found = true;
		}
		else {
			//if not yet found the magic number step one byte back in the file
			zipfile.seekg(-(sizeof(magic) + 1), std::ios::ios_base::cur); 
		}
	}

	if (found) {
		zipfile.seekg(6, std::ios::ios_base::cur); // Jump over multiarchive related fields
		zipfile.read(reinterpret_cast<char*>(&numberOfEntries), sizeof(uint16_t));
		Utils::SwapByteOrder(numberOfEntries);
		zipfile.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
		Utils::SwapByteOrder(size);
		zipfile.read(reinterpret_cast<char*>(&offset), sizeof(uint32_t));
		Utils::SwapByteOrder(offset);
		return true;
	}
	else {
		return false;
	}
}

bool ZIPFilesystem::ReadCentralDirectoryEntry(std::istream & zipfile, std::string & filepath, uint32_t & offset, uint32_t & uncompressed_size) {
	uint32_t magic = 0;
	uint16_t filepath_length;
	uint16_t extra_field_length;
	uint16_t comment_length;
	std::vector<char> filename;

	zipfile.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	Utils::SwapByteOrder(magic); //Take care of big endian systems
	if (magic != centralDirectoryEntry) return false;
	zipfile.seekg(20, std::ios::ios_base::cur); // Jump over currently not needed entries
	zipfile.read(reinterpret_cast<char*>(&uncompressed_size), sizeof(uint32_t));
	Utils::SwapByteOrder(uncompressed_size);
	zipfile.read(reinterpret_cast<char*>(&filepath_length), sizeof(uint16_t));
	Utils::SwapByteOrder(filepath_length);
	zipfile.read(reinterpret_cast<char*>(&extra_field_length), sizeof(uint16_t));
	Utils::SwapByteOrder(extra_field_length);
	zipfile.read(reinterpret_cast<char*>(&comment_length), sizeof(uint16_t));
	Utils::SwapByteOrder(comment_length);
	zipfile.seekg(8, std::ios::ios_base::cur); // Jump over currently not needed entries
	zipfile.read(reinterpret_cast<char*>(&offset), sizeof(uint32_t));
	Utils::SwapByteOrder(offset);
	filename.resize(filepath_length+1);
	filename[filepath_length] = '\0';
	zipfile.read(reinterpret_cast<char*>(filename.data()),filepath_length);
	filepath = filename.data();
	zipfile.seekg(comment_length+ extra_field_length, std::ios::ios_base::cur); // Jump over currently not needed entries
	return true;
}

//https://github.com/Zipios/Zipios/blob/master/src/zipinputstreambuf.cpp

ZIPFilesystem::~ZIPFilesystem() {
	for (auto it = m_InputPool.begin(); it != m_InputPool.end(); it++) {
		it->used = false;
		delete it->filebuffer;
	}
}

bool ZIPFilesystem::IsFile(std::string const & path) const {
	auto it = m_zipContent.find(Utils::LowerCase(path));
	if (it != m_zipContent.end()) {
		return !it->second.isDirectory;
	}
	else {
		return false;
	}
}

bool ZIPFilesystem::IsDirectory(std::string const & path) const {
	auto it = m_zipContent.find(Utils::LowerCase(path));
	if (it != m_zipContent.end()) {
		return it->second.isDirectory;
	}
	else {
		return false;
	}
}

bool ZIPFilesystem::Exists(std::string const & path) const {
	auto it = m_zipContent.find(Utils::LowerCase(path));
	return (it != m_zipContent.end());
}

uint32_t ZIPFilesystem::GetFilesize(std::string const & path) const {
	auto it = m_zipContent.find(Utils::LowerCase(path));
	if (it != m_zipContent.end()) {
		return it->second.filesize;
	}
	else {
		return 0;
	}
}

std::streambuf * ZIPFilesystem::CreateInputStreambuffer(std::string const & path,int mode) {
	return nullptr;
}

std::streambuf * ZIPFilesystem::CreateOutputStreambuffer(std::string const & path,int mode) {
	return nullptr;
}

bool ZIPFilesystem::ListDirectoryEntries(std::string const& path, ListDirectoryEntriesCallback callback) const {
	if (!m_isValid) return false;
	DirectoryEntry entry;
	for (auto it = m_zipContent.begin(); it != m_zipContent.end(); it++) {
		if (it->first.size() != path.size() && it->first.substr(0, path.size()) == Utils::LowerCase(path)&&it->first.substr(path.size(),it->first.size()).find_last_of('/')==0) {
			//Everything that starts with the path but isn't the path and does only contain a slash at the start
			entry.name = it->first.substr(path.size() + 1, it->first.size());
			entry.isDirectory = it->second.isDirectory;
		}
	}

	return true;
}