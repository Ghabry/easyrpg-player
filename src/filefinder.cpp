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

// Headers
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ciso646>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <boost/format.hpp>

#include "system.h"
#include "options.h"
#include "utils.h"
#include "filefinder.h"
#include "output.h"
#include "player.h"
#include "registry.h"

#ifdef _MSC_VER
#  include "rtp_table_bom.h"
#else
#  include "rtp_table.h"
#endif

#ifdef __MINGW32__
#  include <dirent.h>
#elif defined(_MSC_VER)
#  include "dirent_win.h"
#endif

#ifdef _WIN32
#  include <windows.h>
#  include <shlobj.h>
#else
#  include <dirent.h>
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#endif

// MinGW shlobj.h does not define this
#ifndef SHGFP_TYPE_CURRENT
#define SHGFP_TYPE_CURRENT 0
#endif

namespace {

const char* MOVIE_TYPES[] = { ".avi", ".mpg", NULL };
const char* FONTS_TYPES[] = {
	".ttf", ".ttc", ".otf", ".fon", NULL, };
const char* IMG_TYPES[] = {
	".bmp",  ".png", ".xyz", ".gif", ".jpg", ".jpeg", NULL };
const char* MUSIC_TYPES[] = {
	".wav", ".ogg", ".mid", ".midi", ".mp3", NULL };
const char* SOUND_TYPES[] = {
	".wav", ".ogg", ".mp3", NULL };
const char* NO_EXTS[] = {"", NULL};

bool is_not_ascii_char(uint8_t c) { return c < 0x80; }
bool is_not_ascii_filename(std::string const& n) {
	return std::find_if(n.begin(), n.end(), &is_not_ascii_char) != n.end();
}

} // anonymous namespace

std::string const& FileFinder_::translate_rtp(std::string const& dir, std::string const& name) {
	rtp_table_type const& table = Player::is_rpg2k3()? RTP_TABLE_2003 : RTP_TABLE_2000;

	rtp_table_type::const_iterator dir_it = table.find(Utils::LowerCase(dir));
	if (dir_it == table.end()) { return name; }

	std::map<std::string, std::string>::const_iterator file_it =
			dir_it->second.find(Utils::LowerCase(name));

	if (file_it == dir_it->second.end() and is_not_ascii_filename(name)) {
		// Linear Search: Japanese file name to English file name
		for (std::map<std::string, std::string>::const_iterator it = dir_it->second.begin(); it != file_it; ++it) {
			if (it->second == name) {
				return it->first;
			}
		}
		return name;
	}

	return file_it->second;
}

std::string FileFinder_::FindFile(const std::string &dir, const std::string& name, const char* exts[]) {
	ProjectTree const& tree = GetProjectTree();
	boost::optional<std::string> const ret = FindFile(tree, dir, name, exts);
	if (ret != boost::none) { return *ret; }

	std::string const& rtp_name = translate_rtp(dir, name);
	Output().Debug(boost::format("RTP name %s(%s)") % rtp_name % name);

	for(search_path_list::const_iterator i = search_paths.begin(); i != search_paths.end(); ++i) {
		if (! *i) { continue; }

		boost::optional<std::string> const ret = FindFile(*(*i), dir, name, exts);
		if (ret != boost::none) { return *ret; }

		boost::optional<std::string> const ret_rtp = FindFile(*(*i), dir, rtp_name, exts);
		if (ret_rtp != boost::none) { return *ret_rtp; }
	}

	Output().Debug(boost::format("Cannot find: %s/%s") % dir % name);

	return "";
}

boost::optional<std::string> FileFinder_::FindFile(FileFinder_::ProjectTree const& tree,
												   std::string const& dir,
												   std::string const& name,
												   char const* exts[])
{
	std::string const lower_dir = Utils::LowerCase(dir);
	string_map::const_iterator dir_it = tree.directories.find(lower_dir);
	if(dir_it == tree.directories.end()) { return boost::none; }

	string_map const& dir_map = tree.sub_members.find(lower_dir)->second;

	for(char const** c = exts; *c != NULL; ++c) {
		std::string const lower_name = Utils::LowerCase(name + *c);
		string_map::const_iterator const name_it = dir_map.find(lower_name);
		if(name_it != dir_map.end()) {
			return MakePath
					(std::string(tree.project_path).append("/")
					 .append(dir_it->second), name_it->second);
		}
	}

	return boost::none;
}

EASYRPG_SHARED_PTR<FileFinder_::ProjectTree> FileFinder_::CreateProjectTree(std::string const& p) {
	if(! (Exists(p) && IsDirectory(p))) { return EASYRPG_SHARED_PTR<ProjectTree>(); }

	EASYRPG_SHARED_PTR<ProjectTree> tree = EASYRPG_MAKE_SHARED<ProjectTree>();
	tree->project_path = p;

	Directory mem = GetDirectoryMembers(tree->project_path, ALL);
	for(string_map::const_iterator i = mem.members.begin(); i != mem.members.end(); ++i) {
		(IsDirectory(MakePath(tree->project_path, i->second))?
		 tree->directories : tree->files)[i->first] = i->second;
	}

	for(string_map::const_iterator i = tree->directories.begin(); i != tree->directories.end(); ++i) {
		GetDirectoryMembers(MakePath(tree->project_path, i->second), FILES)
			.members.swap(tree->sub_members[i->first]);
	}

	return tree;
}

std::string FileFinder_::MakePath(const std::string &dir, std::string const& name) {
	std::string str = dir.empty()? name : dir + "/" + name;
#ifdef _WIN32
	std::replace(str.begin(), str.end(), '/', '\\');
#else
	std::replace(str.begin(), str.end(), '\\', '/');
#endif
	return str;
}

#ifdef _WIN32
std::string GetFontsPath() {
	static std::string fonts_path = "";
	static bool init = false;

	if (init) {
		return fonts_path;
	} else {
		// Retrieve the Path of the Font Directory
		TCHAR path[MAX_PATH];

		if (SHGetFolderPath(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, path) == S_OK)	{
			char fpath[MAX_PATH];
#ifdef UNICODE
			WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS | WC_COMPOSITECHECK, path, MAX_PATH, fpath, MAX_PATH, NULL, NULL);
#endif
			fonts_path = FileFinder().MakePath(fpath, "");
		}

		init = true;

		return fonts_path;
	}
}

std::string GetFontFilename(std::string const& name) {
	std::string real_name = Registry::ReadStrValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", name + " (TrueType)");
	if (real_name.length() > 0) {
		if (FileFinder().Exists(real_name))
			return real_name;
		if (FileFinder().Exists(GetFontsPath() + real_name))
			return GetFontsPath() + real_name;
	}

	real_name = Registry::ReadStrValue(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Fonts", name + " (TrueType)");
	if (real_name.length() > 0) {
		if (FileFinder().Exists(real_name))
			return real_name;
		if (FileFinder().Exists(GetFontsPath() + real_name))
			return GetFontsPath() + real_name;
	}

	return name;
}
#endif

std::string FileFinder_::FindFont(const std::string& name) {
	std::string path = FindFile("Font", name, FONTS_TYPES);

#ifdef _WIN32
	if (!path.empty()) {
		return path;
	}

	std::string folder_path = "";
	std::string filename = name;

	size_t separator_pos = path.rfind('\\');
	if (separator_pos != std::string::npos) {
		folder_path = path.substr(0, separator_pos);
		filename = path.substr(separator_pos, path.length() - separator_pos);
	}

	std::string font_filename = GetFontFilename(filename);
	if (!font_filename.empty()) {
		if (Exists(folder_path + font_filename))
			return folder_path + font_filename;

		if (Exists(fonts_path + font_filename))
			return fonts_path + font_filename;
	}

	return "";
#else
	return path;
#endif
}

FileFinder_::ProjectTree const& FileFinder_::GetProjectTree() {
	if(tree_.project_path != project_path) {
		EASYRPG_SHARED_PTR<ProjectTree> t = CreateProjectTree(project_path);
		if(! t) {
			Output().Error(boost::format("invalid project path: %s") % project_path);
			return tree_;
		}
		tree_ = *t;
	}

	return tree_;
}

FileFinder_::FileFinder_()
		: project_path(
			  getenv("RPG_TEST_GAME_PATH")? getenv("RPG_TEST_GAME_PATH"):
			  getenv("RPG_GAME_PATH")? getenv("RPG_GAME_PATH"):
			  ".")
 {
	GetProjectTree(); // empty call
}

void FileFinder_::add_rtp_path(std::string const& p) {
	EASYRPG_SHARED_PTR<ProjectTree> tree(CreateProjectTree(p));
	if(tree) {
		Output().Debug(boost::format("Adding %s to RTP path") % p);
		search_paths.push_back(tree);
	}
}


void FileFinder_::UpdateRtpPaths() {
	search_paths.clear();

	std::string const version_str =
			Player::is_rpg2k()? "2000":
			Player::is_rpg2k3()? "2003":
			"";

	assert(!version_str.empty());

	std::string const company = Player::is_rpg2k()? "ASCII": "Enterbrain";

	std::string rtp_path = Registry::ReadStrValue(HKEY_CURRENT_USER, "Software\\" + company + "\\RPG" + version_str, "RuntimePackagePath");
	if(! rtp_path.empty()) { add_rtp_path(rtp_path); }

	rtp_path = Registry::ReadStrValue(HKEY_LOCAL_MACHINE, "Software\\" + company + "\\RPG" + version_str, "RuntimePackagePath");
	if(! rtp_path.empty()) { add_rtp_path(rtp_path); }
#ifdef GEKKO
	add_rtp_path("sd:/data/rtp/" + version_str + "/");
	add_rtp_path("usb:/data/rtp/" + version_str + "/");
#else
	add_rtp_path("/data/rtp/" + version_str + "/");
#endif

	if (Player::is_rpg2k() && getenv("RPG2K_RTP_PATH"))
		add_rtp_path(getenv("RPG2K_RTP_PATH"));
	else if (Player::is_rpg2k3() && getenv("RPG2K3_RTP_PATH"))
		add_rtp_path(getenv("RPG2K3_RTP_PATH"));
	if(getenv("RPG_RTP_PATH")) { add_rtp_path(getenv("RPG_RTP_PATH")); }
}

FILE* FileFinder_::fopenUTF8(const std::string& name_utf8, char const* mode) {
#ifdef _WIN32
	return _wfopen(Utils::ToWideString(name_utf8).c_str(),
				   Utils::ToWideString(mode).c_str());
#else
	return fopen(name_utf8.c_str(), mode);
#endif
}

EASYRPG_SHARED_PTR<std::fstream> FileFinder_::openUTF8(const std::string& name,
													  std::ios_base::openmode m)
{
	EASYRPG_SHARED_PTR<std::fstream> ret(new std::fstream(
#ifdef _MSC_VER
		Utils::ToWideString(name).c_str(),
#else
		name.c_str(),
#endif
		m));
	return (*ret)? ret : EASYRPG_SHARED_PTR<std::fstream>();
}

std::string FileFinder_::FindImage(const std::string& dir, const std::string& name) {
	return FindFile(dir, name, IMG_TYPES);
}

std::string FileFinder_::FindDefault(const std::string& dir, const std::string& name) {
	return FindFile(dir, name, NO_EXTS);
}

std::string FileFinder_::FindDefault(std::string const& name) {
	ProjectTree const& p = GetProjectTree();
	string_map const& files = p.files;

	string_map::const_iterator const it = files.find(Utils::LowerCase(name));

	return(it != files.end())? MakePath(p.project_path, it->second) : "";
}

bool FileFinder_::IsRPG2kProject(ProjectTree const& dir) {
	return IsRPG2kProject(dir.files);
}

bool FileFinder_::IsRPG2kProject(string_map const& dir) {
	return(dir.find(Utils::LowerCase(DATABASE_NAME)) != dir.end() &&
		   dir.find(Utils::LowerCase(TREEMAP_NAME)) != dir.end());
}

std::string FileFinder_::FindMusic(const std::string& name) {
	return FindFile("Music", name, MUSIC_TYPES);
}

std::string FileFinder_::FindSound(const std::string& name) {
	return FindFile("Sound", name, SOUND_TYPES);
}

std::string FileFinder_::FindMovie(const std::string& name) {
	return FindFile("Movie", name, MOVIE_TYPES);
}

std::string FileFinder_::fullpath(std::string const& f) const {
#ifdef _WIN32
	wchar_t buf[MAX_PATH + 1];
	if(not _wfullpath(buf, Utils::ToWideString(f).c_str(), MAX_PATH)) {
		Output().Debug(boost::format("path resolving failed: \"%s\"") % f);
		return std::string();
	}
	return Utils::FromWideString(buf);
#else
	char buf[PATH_MAX + 1];
	if(not realpath(f.c_str(), buf)) {
		Output().Debug(boost::format("path resolving failed: \"%s\"") % f);
		return std::string();
	}
	return buf;
#endif
}

bool FileFinder_::Exists(std::string const& filename) {
#ifdef _WIN32
	return ::GetFileAttributesW(Utils::ToWideString(filename).c_str()) != (DWORD)-1;
#elif GEKKO
	struct stat sb;
	return ::stat(filename.c_str(), &sb) == 0;
#else
	return ::access(filename.c_str(), F_OK) != -1;
#endif
}

bool FileFinder_::IsDirectory(std::string const& dir) {
#ifdef _WIN32
	return(::GetFileAttributesW(Utils::ToWideString(dir).c_str()) & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat sb;
	return (::stat(dir.c_str(), &sb) != -1) and S_ISDIR(sb.st_mode);
#endif
}

FileFinder_::Directory FileFinder_::GetDirectoryMembers(const std::string& path, FileFinder_::Mode const m) {
	assert(Exists(path));
	assert(IsDirectory(path));

	Directory result;

	result.base = path;

#ifdef _WIN32
#  define DIR _WDIR
#  define opendir _wopendir
#  define closedir _wclosedir
#  define wpath Utils::ToWideString(path)
#  define dirent _wdirent
#  define readdir _wreaddir
#else
#  define wpath path
#endif

	EASYRPG_SHARED_PTR< ::DIR> dir(::opendir(wpath.c_str()), ::closedir);
	if (!dir) {
		Output().Error(boost::format("Error opening dir %s: %s") % path % ::strerror(errno));
		return result;
	}

	struct dirent* ent;
	while ((ent = ::readdir(dir.get())) != NULL) {
		if (ent->d_name[0] == '.') { continue; }
#ifdef _WIN32
		std::string const name = Utils::FromWideString(ent->d_name);
#else
		std::string const name = ent->d_name;
#endif
		switch(m) {
		case FILES:
			if(IsDirectory(MakePath(path, name))) { continue; }
		    break;
		case DIRECTORIES:
			if(! IsDirectory(MakePath(path, name))) { continue; }
			break;
		case ALL:
			break;
		}
		result.members[Utils::LowerCase(name)] = name;
	}

#ifdef _WIN32
#  undef DIR
#  undef opendir
#  undef closedir
#  undef dirent
#  undef readdir
#endif
#undef wpath

	return result;
}
