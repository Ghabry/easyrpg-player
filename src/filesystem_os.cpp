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

#include "filesystem_os.h"
#include <cstring>
#include <fstream>
#include <cassert>

#ifdef _WIN32
#  include <windows.h>
#  include <shlobj.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  define StatBuf struct _stat
#  define GetStat _stat
#  ifdef __MINGW32__
#    include <dirent.h>
#  elif defined(_MSC_VER)
#    include "dirent_win.h"
#  endif
#else
#  ifdef PSP2
#    include <psp2/io/dirent.h>
#    include <psp2/io/stat.h>
#    define S_ISDIR SCE_S_ISDIR
#    define opendir sceIoDopen
#    define closedir sceIoDclose
#    define dirent SceIoDirent
#    define readdir sceIoDread
#    define stat SceIoStat
#    define lstat sceIoGetstat
#    define StatBuf SceIoStat
#    define GetStat sceIoGetstat
#  else
#    include <dirent.h>
#    include <sys/stat.h>
#    define StatBuf struct stat
#    define GetStat stat
#  endif
#  include <unistd.h>
#  include <sys/types.h>
#endif

#ifdef __ANDROID__
#   include <jni.h>
#   include <SDL_system.h>
#endif

#ifdef __MORPHOS__
#   undef bind
#endif

#include "utils.h"
#include "output.h"

OSFilesystem::OSFilesystem(std::string const & rootPath):m_rootPath(rootPath){

}

OSFilesystem::~OSFilesystem() {

}

std::string OSFilesystem::MakeAbsolutePath(std::string const & path) const {
	if (path != ".") {
		return m_rootPath + "/" + path;
	} else {
		return m_rootPath;
	}
}

bool OSFilesystem::IsFile(std::string const & path) const {
	return false;
}

bool OSFilesystem::IsDirectory(std::string const & path) const {
	std::string dir = MakeAbsolutePath(path);
#if (defined(GEKKO) || defined(_3DS) || defined(SWITCH))
	struct stat sb;
	if (::stat(dir.c_str(), &sb) == 0)
		return S_ISDIR(sb.st_mode);
	return false;
#else
	if (!Exists(dir)) {
		return false;
	}

#  ifdef _WIN32
	int attribs = ::GetFileAttributesW(Utils::ToWideString(dir).c_str());
	return (attribs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
	      == FILE_ATTRIBUTE_DIRECTORY;
#  else
	struct stat sb;
	::lstat(dir.c_str(), &sb);
	return S_ISDIR(sb.st_mode);
#  endif
#endif
}

bool OSFilesystem::Exists(std::string const & path) const {
	std::string filename = MakeAbsolutePath(path);
#ifdef _WIN32
	return ::GetFileAttributesW(Utils::ToWideString(filename).c_str()) != (DWORD)-1;
#elif (defined(GEKKO) || defined(_3DS) || defined(SWITCH))
	struct stat sb;
	return ::stat(filename.c_str(), &sb) == 0;
#elif defined(PSP2)
	struct SceIoStat sb;
	return (sceIoGetstat(filename.c_str(), &sb) >= 0);
#else
	return ::access(filename.c_str(), F_OK) != -1;
#endif
}

uint32_t OSFilesystem::GetFilesize(std::string const & path) const {
	StatBuf sb;
	int result = GetStat(MakeAbsolutePath(path).c_str(), &sb);
	return (result == 0) ? sb.st_size : -1;
}

std::streambuf * OSFilesystem::CreateInputStreambuffer(std::string const & path, std::ios_base::openmode mode) {
	std::filebuf *buf = new std::filebuf();

	return buf->open(
#ifdef _MSC_VER
		Utils::ToWideString(MakeAbsolutePath(path)).c_str(),
#else
		MakeAbsolutePath(path).c_str(),
#endif
		mode);
}

std::streambuf * OSFilesystem::CreateOutputStreambuffer(std::string const & path, std::ios_base::openmode mode) {
	std::filebuf *buf = new std::filebuf();
	return buf->open(
#ifdef _MSC_VER
		Utils::ToWideString(MakeAbsolutePath(path)).c_str(),
#else
		MakeAbsolutePath(path).c_str(),
#endif
		mode);
}



bool OSFilesystem::ListDirectoryEntries(std::string const& path, ListDirectoryEntriesCallback callback) const {


std::string abs_path = MakeAbsolutePath(path);

DirectoryEntry result;

#ifdef _WIN32
#  define DIR _WDIR
#  define opendir _wopendir
#  define closedir _wclosedir
#  define wpath Utils::ToWideString(abs_path)
#  define dirent _wdirent
#  define readdir _wreaddir
#elif _3DS
	std::string wpath = abs_path + "/";
#else
#  define wpath abs_path
#endif
#ifdef PSP2
	int dir = opendir(wpath.c_str());
	if (dir < 0) {
#else
	std::shared_ptr< ::DIR> dir(::opendir(wpath.c_str()), [](::DIR* d) { if (d) ::closedir(d); });
	if (!dir) {
#endif
		Output::Debug("Error opening dir %s: %s", path.c_str(),
					  ::strerror(errno));
		entries[0].type = Filesystem::FileType::Invalid;
		return entries;
	}

#ifdef PSP2
	struct dirent ent;
	while (readdir(dir, &ent) > 0) {
#else
	struct dirent* ent;
	while ((ent = ::readdir(dir.get())) != NULL) {
#endif
#ifdef _WIN32
		std::string const name = Utils::FromWideString(ent->d_name);
#else
#ifdef PSP2
		std::string const name = ent.d_name;
#else
		std::string const name = ent->d_name;
#endif
#endif

		static bool has_fast_dir_stat = true;
		bool is_directory = false;
		if (has_fast_dir_stat) {
			#ifdef PSP2
			is_directory = S_ISDIR(ent.d_stat.st_mode);
			#elif defined(_DIRENT_HAVE_D_TYPE) || defined(_3DS)
			if (ent->d_type == DT_UNKNOWN) {
				has_fast_dir_stat = false;
			} else {
				is_directory = ent->d_type == DT_DIR;
			}
			#else
			has_fast_dir_stat = false;
			#endif
		}

		if (!has_fast_dir_stat) {
			is_directory = IsDirectory(MakePath(path, name));
		}

		if (name == "." || name == "..") {
			continue;
		}

		result.isDirectory = is_directory;
		result.name = name;

		callback(this, result);

	}
#ifdef _WIN32
#  undef DIR
#  undef opendir
#  undef closedir
#  undef dirent
#  undef readdir
#endif
#undef wpath
#ifdef PSP2
	closedir(dir);
#endif
	return true;
}

