#include "filesystem_zip.h"
#include <zlib.h>
#include "utils.h"
#include <iostream>

static const uint32_t endOfCentralDirectory = 0x06054b50;
static const int32_t endOfCentralDirectorySize = 22;

static const uint32_t centralDirectoryEntry = 0x02014b50;

#define STRING_BEGINS_WITH(_string, _begin)  (_string.size()!=_begin.size()&&_string.substr(0,_begin.size())==_begin)

static std::string normalize_path(std::string const & path) {
	if (path == "." || path == "/") return "";
	std::string inner_path = Utils::LowerCase(path);
	if (inner_path.size() != 0 && inner_path.back() != '/') { inner_path = inner_path + "/"; }
	return inner_path;
}

ZIPFilesystem::ZIPFilesystem(std::string const & os_path, std::string const & sub_path, uint32_t zipFileSize) {
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

	//inner path is needed to achieve an offset inside the archive
	std::string inner_path = normalize_path(sub_path);

	std::istream zipfile(initialEntry.filebuffer); //Take the first streambuffer of the pool

	if (FindCentralDirectory(zipfile,centralDirectoryOffset, centralDirectorySize, centralDirectoryEntries)) {
		zipfile.seekg(centralDirectoryOffset); //Seek to the start of the central directory
		while (ReadCentralDirectoryEntry(zipfile, filepath, entry.fileoffset, entry.filesize)) {
			//zip is case insensitive, so store only the lower case
			filepath = Utils::LowerCase(filepath);
			if (STRING_BEGINS_WITH(filepath,inner_path)) {
				//remove the offset directory from the front of the path
				filepath=filepath.substr(inner_path.size(), filepath.size() - inner_path.size());
				
				//check if the entry is an directory or not (indicated by trailing /)
				if (filepath.back() == '/') {
					entry.isDirectory = true;
					filepath = filepath.substr(0, filepath.size() - 1);
				}
				else {
					entry.isDirectory = false;
				}

				
				m_zipContent.insert(std::pair<std::string, ZipEntry>(filepath, entry));
			}
		}

		//Insert root path
		entry.isDirectory = true;
		entry.fileoffset = 0;
		entry.filesize = 0;
		m_zipContent.insert(std::pair<std::string, ZipEntry>("", entry));

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


ZIPFilesystem::~ZIPFilesystem() {
	for (auto it = m_InputPool.begin(); it != m_InputPool.end(); it++) {
		it->used = false;
		delete it->filebuffer;
	}
}

bool ZIPFilesystem::IsFile(std::string const & path) const {
	std::string path_lower = normalize_path(path);
	auto it = m_zipContent.find(path_lower);
	if (it != m_zipContent.end()) {
		return !it->second.isDirectory;
	}
	else {
		return false;
	}
}

bool ZIPFilesystem::IsDirectory(std::string const & path) const {
	std::string path_lower = normalize_path(path);
	auto it = m_zipContent.find(path_lower);
	if (it != m_zipContent.end()) {
		return it->second.isDirectory;
	}
	else {
		return false;
	}
}

bool ZIPFilesystem::Exists(std::string const & path) const {
	std::string path_lower = normalize_path(path);
	auto it = m_zipContent.find(path_lower);
	return (it != m_zipContent.end());
}

uint32_t ZIPFilesystem::GetFilesize(std::string const & path) const {
	std::string path_lower = normalize_path(path);

	auto it = m_zipContent.find(path_lower);
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

	std::string path_lower = normalize_path(path);

	DirectoryEntry entry;
	for (auto it = m_zipContent.begin(); it != m_zipContent.end(); it++) {
		if (STRING_BEGINS_WITH(it->first,path_lower)&&it->first.substr(path_lower.size(),it->first.size()- path_lower.size()).find_last_of('/')==std::string::npos) {
			//Everything that starts with the path but isn't the path and does contain no slash
			entry.name = it->first.substr(path_lower.size(), it->first.size()- path_lower.size());
			entry.isDirectory = it->second.isDirectory;
			callback(this, entry);
		}
	}

	return true;
}