#include "filesystem_zip.h"

ZIPFilesystem::ZIPFilesystem(std::istream & zipFileStream, uint32_t zipFileSize) {

}

ZIPFilesystem::~ZIPFilesystem() {

}

bool ZIPFilesystem::IsFile(std::string const & path) const {
	return false;
}

bool ZIPFilesystem::IsDirectory(std::string const & path) const {
	return false;
}

bool ZIPFilesystem::Exists(std::string const & path) const {
	return false;
}

uint32_t ZIPFilesystem::GetFilesize(std::string const & path) const {
	return false;

}

std::streambuf * ZIPFilesystem::CreateInputStreambuffer(std::string const & path,int mode) {
	return nullptr;
}

std::streambuf * ZIPFilesystem::CreateOutputStreambuffer(std::string const & path,int mode) {
	return nullptr;
}

bool ZIPFilesystem::ListDirectoryEntries(std::string const& path, ListDirectoryEntriesCallback callback) const {
	return false;
}