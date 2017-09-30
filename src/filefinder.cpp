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
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#ifdef _WIN32
#  include <windows.h>
#  include <shlobj.h>
#endif

#ifdef __MORPHOS__
#undef bind
#endif

#include "system.h"
#include "options.h"
#include "utils.h"
#include "filefinder.h"
#include "output.h"
#include "player.h"
#include "registry.h"
#include "rtp_table.h"
#include "main_data.h"
#include "filesystem_os.h"
#include "filesystem_zip.h"

// MinGW shlobj.h does not define this
#ifndef SHGFP_TYPE_CURRENT
#define SHGFP_TYPE_CURRENT 0
#endif

namespace {
#ifdef SUPPORT_MOVIES
	const char* const MOVIE_TYPES[] = { ".avi", ".mpg" };
#endif

	typedef std::vector<FilesystemRef> search_path_list;
	FilesystemRef game_filesystem;
	search_path_list search_paths;
	std::string fonts_path;

	std::string FindFile(FileFinder::DirectoryTree const& tree,
										  const std::string& dir,
										  const std::string& name,
										  char const* exts[])
	{
		using namespace FileFinder;

#ifdef EMSCRIPTEN
		// The php filefinder should have given us an useable path
		std::string em_file = MakePath(dir, name);

		if (Exists(em_file))
			return em_file;
#endif

		std::string lower_dir = Utils::LowerCase(dir);
		std::string const escape_symbol = Player::escape_symbol;
		std::string corrected_name = Utils::LowerCase(name);

		std::string combined_path = Filesystem::CombinePath(lower_dir, corrected_name);
		std::string canon = MakeCanonical(combined_path, 1);
		if (combined_path != canon) {
			// Very few games (e.g. Yume2kki) use path traversal (..) in the filenames to point
			// to files outside of the actual directory.
			// Fix the path and search the file again with the correct root directory set.
			if (dir != ".") {
				// Prevent "path adjusted" debug log when searching for ExFont
				Output::Debug("Path adjusted: %s -> %s", combined_path.c_str(), canon.c_str());
			}
			/*FIXME for (char const** c = exts; *c != NULL; ++c) {
				std::string res = FileFinder::FindDefault(tree, canon + *c);
				if (!res.empty()) {
					return res;
				}
			}
			return "";*/
		}

#ifdef _WIN32
		if (escape_symbol != "\\") {
#endif
			std::size_t escape_pos = corrected_name.find(escape_symbol);
			while (escape_pos != std::string::npos) {
				corrected_name.erase(escape_pos, escape_symbol.length());
				corrected_name.insert(escape_pos, "/");
				escape_pos = corrected_name.find(escape_symbol);
			}
#ifdef _WIN32
		}
#endif

		string_map::const_iterator dir_it = tree.directories.find(lower_dir);
		if(dir_it == tree.directories.end()) { return ""; }

		string_map const& dir_map = tree.sub_members.find(lower_dir)->second;

		for(char const** c = exts; *c != NULL; ++c) {
			string_map::const_iterator const name_it = dir_map.find(corrected_name + *c);
			if(name_it != dir_map.end()) {
				return Filesystem::CombinePath
					(std::string(tree.directory_path).append("/")
					 .append(dir_it->second), name_it->second);
			}
		}

		return "";
	}

	bool is_not_ascii_char(uint8_t c) { return c > 0x80; }

	bool is_not_ascii_filename(const std::string& n) {
		return std::find_if(n.begin(), n.end(), &is_not_ascii_char) != n.end();
	}

	const std::string translate_rtp(const std::string& dir, const std::string& name) {
		RTP::rtp_table_type const& table =
			Player::IsRPG2k() ? RTP::RTP_TABLE_2000 : RTP::RTP_TABLE_2003;

		RTP::rtp_table_type::const_iterator dir_it = table.find(Utils::LowerCase(dir).c_str());
		std::string lower_name = Utils::LowerCase(name);

		if (dir_it == table.end()) { return name; }

		std::map<const char*, const char*>::const_iterator file_it = dir_it->second.find(lower_name.c_str());
		if (file_it == dir_it->second.end()) {
			if (is_not_ascii_filename(lower_name)) {
				// Linear Search: Japanese file name to English file name
				for (const auto& entry : dir_it->second) {
					if (!strcmp(entry.second, lower_name.c_str())) {
						return entry.first;
					}
				}
			}
			return name;
		}
		return file_it->second;
	}
/*
	std::string FindFile(const std::string &dir, const std::string& name, const char* exts[]) {
		const std::shared_ptr<FileFinder::DirectoryTree> tree = FileFinder::GetDirectoryTree();
		std::string const ret = FindFile(*tree, dir, name, exts);
		if (!ret.empty()) { return ret; }

		const std::string& rtp_name = translate_rtp(dir, name);

		for(search_path_list::const_iterator i = search_paths.begin(); i != search_paths.end(); ++i) {
			if (! *i) { continue; }

			std::string const ret = FindFile(*(*i), dir, name, exts);
			if (!ret.empty()) { return ret; }

			std::string const ret_rtp = FindFile(*(*i), dir, rtp_name, exts);
			if (!ret_rtp.empty()) { return ret_rtp; }
		}

		Output::Debug("Cannot find: %s/%s (%s)", dir.c_str(), name.c_str(),
						name == rtp_name ? "!" : rtp_name.c_str());

		return std::string();
	}*/
} // anonymous namespace

const FilesystemRef FileFinder::GetGameFilesystem() {
	return game_filesystem;
}

const FilesystemRef FileFinder::CreateSaveFilesystem() {
	std::string save_path = Main_Data::GetSavePath();

	/*if (!(Exists(save_path) && IsDirectory(save_path))) {
		Output::Warning("Save game directory %s is invalid. Saving will not work.", save_path.c_str());
		return std::shared_ptr<DirectoryTree>();
	}*/

	FilesystemRef fs = CreateFilesystem(save_path, false);
	if (!fs) {
		Output::Warning("Save game directory %s is invalid. Saving will not work.", save_path.c_str());
		return FilesystemRef();
	}

	return fs;
}

void FileFinder::SetGameFilesystem(FilesystemRef filesystem) {
	game_filesystem = filesystem;
}

FilesystemRef FileFinder::CreateFilesystem(std::string const& p, bool recursive) {
	// Determine the proper file system to use
	FilesystemRef filesystem;
	// TODO: Determine main filesystem to use
	filesystem.reset(new OSFilesystem(p));

	// The path "mounted" by the virtual filesystem
	std::string path_prefix;

	// When the path doesn't exist check if the path contains a file that can
	// be handled by another filesystem
	if (!filesystem->IsValid()) {
		std::vector<std::string> path = FileFinder::SplitPath(p);

		// TODO this should probably move to a static function in the FS classes

		// search until ".zip", "do magic"
		std::string internal_path;

		bool handle_internal = false;
		for (std::string comp : path) {
			if (handle_internal) {
				internal_path += comp + "/";
			} else {
				path_prefix += comp + "/";
			}
			// TODO check for directory but no rootFilesystem mounted yet
			if (Utils::EndsWith(comp, ".zip")) {
				path_prefix.pop_back();
				handle_internal = true;
			}
		}

		if (!internal_path.empty()) {
			internal_path.pop_back();
		}

		filesystem.reset(new ZIPFilesystem(path_prefix, internal_path, "windows-1252"));
		if (!filesystem->IsValid()) {
			return FilesystemRef();
		}

		path_prefix = "";
	} else {
		// Handle as a normal path in the local filesystem
		path_prefix = p;
	}

	if(! (filesystem->Exists("") && filesystem->IsDirectory(""))) { return FilesystemRef(); }

	filesystem->directory_tree = std::make_shared<DirectoryTree>();

	Directory mem = GetDirectoryMembers(*filesystem, "", Mode::ALL);
	for (auto& i : mem.files) {
		filesystem->directory_tree->files[i.first] = i.second;
	}
	for (auto& i : mem.directories) {
		filesystem->directory_tree->directories[i.first] = i.second;
	}

	if (recursive) {
		for (auto& i : mem.directories) {
			filesystem->directory_tree->sub_members[i.first] = GetDirectoryMembers(*filesystem, i.second, Mode::RECURSIVE).files;
		}
	}
	return filesystem;
}

std::string FileFinder::MakePath(std::string const& dir, std::string const& name) {
	return std::string(Filesystem::CombinePath(dir, name));
}

std::string FileFinder::MakeCanonical(const std::string& path, int initial_deepness) {
	std::vector<std::string> path_components = SplitPath(path);
	std::vector<std::string> path_can;

	for (std::string path_comp : path_components) {
		if (path_comp == "..") {
			if (path_can.size() > 0) {
				path_can.pop_back();
			} else if (initial_deepness > 0) {
				// Ignore, we are in root
				--initial_deepness;
			} else {
				Output::Debug("Path traversal out of game directory: %s", path.c_str());
			}
		} else if (path_comp.empty() || path_comp == ".") {
			// ignore
		} else {
			path_can.push_back(path_comp);
		}
	}

	std::string ret;
	for (std::string s : path_can) {
		ret = Filesystem::CombinePath(ret, s);
	}

	return ret;
}

std::vector<std::string> FileFinder::SplitPath(const std::string& path) {
	// Tokens are patch delimiters ("/" and encoding aware "\")
	std::function<bool(char32_t)> f = [](char32_t t) {
		char32_t escape_char_back = '\0';
		if (!Player::escape_symbol.empty()) {
			escape_char_back = Utils::DecodeUTF32(Player::escape_symbol).front();
		}
		char32_t escape_char_forward = Utils::DecodeUTF32("/").front();
		return t == escape_char_back || t == escape_char_forward;
	};
	return Utils::Tokenize(path, f);
}

std::string FileFinder::GetPathInsidePath(const std::string& path_to, const std::string& path_in) {
	if (!Utils::StartsWith(path_in, path_to)) {
		return "";
	}

	std::string path_out = path_in.substr(path_to.size());
	if (!path_out.empty() && (path_out[0] == '/' || path_out[0] == '\\')) {
		path_out = path_out.substr(1);
	}

	return path_out;
}

std::string FileFinder::GetPathInsideGamePath(const std::string& path_in) {
	// FIXME return FileFinder::GetPathInsidePath(GetDirectoryTree()->directory_path, path_in);
	return path_in;
}

#if defined(_WIN32) && !defined(_ARM_)
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
			fonts_path = Filesystem::CombinePath(fpath, "");
		}

		init = true;

		return fonts_path;
	}
}

std::string GetFontFilename(const std::string& name) {
	std::string real_name = Registry::ReadStrValue(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", name + " (TrueType)");
	if (real_name.length() > 0) {
		if (FileFinder::Exists(real_name))
			return real_name;
		if (FileFinder::Exists(GetFontsPath() + real_name))
			return GetFontsPath() + real_name;
	}

	real_name = Registry::ReadStrValue(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Fonts", name + " (TrueType)");
	if (real_name.length() > 0) {
		if (FileFinder::Exists(real_name))
			return real_name;
		if (FileFinder::Exists(GetFontsPath() + real_name))
			return GetFontsPath() + real_name;
	}

	return name;
}
#endif

std::string FileFinder::FindFont(const std::string& name) {
	static const char* FONTS_TYPES[] = {
		".ttf", ".ttc", ".otf", ".fon", NULL, };
	std::string path = game_filesystem->FindFile("Font", name, FONTS_TYPES);

#if defined(_WIN32) && !defined(_ARM_)
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
		if (FileFinder::Exists(folder_path + font_filename))
			return folder_path + font_filename;

		if (FileFinder::Exists(fonts_path + font_filename))
			return fonts_path + font_filename;
	}

	return "";
#else
	return path;
#endif
}

static void add_rtp_path(const std::string& p) {
	using namespace FileFinder;
	FilesystemRef fs(CreateFilesystem(p));
	if (fs) {
		Output::Debug("Adding %s to RTP path", p.c_str());
		search_paths.push_back(fs);
	}
}

static void read_rtp_registry(const std::string& company, const std::string& product, const std::string& key) {
#if !(defined(GEKKO) || defined(SWITCH) || defined(__ANDROID__) || defined(EMSCRIPTEN) || defined(_3DS)) && !(defined(_WIN32) && defined(_ARM_))
	std::string rtp_path = Registry::ReadStrValue(HKEY_CURRENT_USER, "Software\\" + company + "\\" + product, key, KEY32);
	if (!rtp_path.empty()) {
		add_rtp_path(rtp_path);
	}

	rtp_path = Registry::ReadStrValue(HKEY_LOCAL_MACHINE, "Software\\" + company + "\\" + product, key, KEY32);
	if (!rtp_path.empty()) {
		add_rtp_path(rtp_path);
	}
#else
	(void)company; (void)product; (void)key;
#endif
}

void FileFinder::InitRtpPaths(bool warn_no_rtp_found) {
#ifdef EMSCRIPTEN
	// No RTP support for emscripten at the moment.
	return;
#endif

	RTP::Init();

	search_paths.clear();

	std::string const version_str =
		Player::IsRPG2k() ? "2000" :
		Player::IsRPG2k3() ? "2003" :
		"";

	assert(!version_str.empty());

#ifdef GEKKO
	add_rtp_path("sd:/data/rtp/" + version_str + "/");
	add_rtp_path("usb:/data/rtp/" + version_str + "/");
#elif defined(SWITCH)
	add_rtp_path("./rtp/" + version_str + "/");
	add_rtp_path("/switch/easyrpg-player/rtp/" + version_str + "/");
#elif defined(_3DS)
	add_rtp_path("romfs:/data/rtp/" + version_str + "/");
	add_rtp_path("sdmc:/data/rtp/" + version_str + "/");
#elif defined(PSP2)
	add_rtp_path("ux0:/data/easyrpg-player/rtp/" + version_str + "/");
#elif defined(__ANDROID__)
	// Invoke "String getRtpPath()" in EasyRPG Activity via JNI
	JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
	jobject sdl_activity = (jobject)SDL_AndroidGetActivity();
	jclass cls = env->GetObjectClass(sdl_activity);
	jmethodID jni_getRtpPath = env->GetMethodID(cls , "getRtpPath", "()Ljava/lang/String;");
	jstring return_string = (jstring)env->CallObjectMethod(sdl_activity, jni_getRtpPath);

	const char *js = env->GetStringUTFChars(return_string, NULL);
	std::string cs(js);

	env->ReleaseStringUTFChars(return_string, js);
	env->DeleteLocalRef(sdl_activity);
	env->DeleteLocalRef(cls);

	add_rtp_path(cs + "/" + version_str + "/");
#else
	// Windows/Wine
	std::string const product = "RPG" + version_str;
	if (Player::IsRPG2k()) {
		// Prefer original 2000 RTP over Kadokawa, because there is no
		// reliable way to detect this engine and much more 2k games
		// use the non-English version
		read_rtp_registry("ASCII", product, "RuntimePackagePath");
		read_rtp_registry("KADOKAWA", product, "RuntimePackagePath");
	}
	else if (Player::IsRPG2k3Legacy()) {
		// Original 2003 RTP installer registry key is upper case
		// and Wine registry is case insensitive but new 2k3v1.10 installer is not
		// Prefer Enterbrain RTP over Kadokawa for old RPG2k3 (search order)
		read_rtp_registry("Enterbrain", product, "RUNTIMEPACKAGEPATH");
		read_rtp_registry("KADOKAWA", product, "RuntimePackagePath");
	}
	else if (Player::IsRPG2k3E()) {
		// Prefer Kadokawa RTP over Enterbrain for new RPG2k3
		read_rtp_registry("KADOKAWA", product, "RuntimePackagePath");
		read_rtp_registry("Enterbrain", product, "RUNTIMEPACKAGEPATH");
	}

	// Our RTP is for all engines
	read_rtp_registry("EasyRPG", "RTP", "path");

	add_rtp_path("/data/rtp/" + version_str + "/");
#endif
	std::vector<std::string> env_paths;

	// Windows paths are split by semicolon, Unix paths by colon
	std::function<bool(char32_t)> f = [](char32_t t) {
#ifdef _WIN32
		return t == ';';
#else
		return t == ':';
#endif
	};

	if (Player::IsRPG2k() && getenv("RPG2K_RTP_PATH"))
		env_paths = Utils::Tokenize(getenv("RPG2K_RTP_PATH"), f);
	else if (Player::IsRPG2k3() && getenv("RPG2K3_RTP_PATH"))
		env_paths = Utils::Tokenize(getenv("RPG2K3_RTP_PATH"), f);

	if (getenv("RPG_RTP_PATH")) {
		std::vector<std::string> tmp = Utils::Tokenize(getenv("RPG_RTP_PATH"), f);
		env_paths.insert(env_paths.end(), tmp.begin(), tmp.end());
	}

	for (const std::string p : env_paths) {
		add_rtp_path(p);
	}

	if (warn_no_rtp_found && search_paths.empty()) {
		Output::Warning("RTP not found. This may create missing file errors. "
			"Install RTP files or check they are installed fine. "
			"If this game really does not require RTP, then add "
			"FullPackageFlag=1 line to the RPG_RT.ini game file.");
	}
}

void FileFinder::Quit() {
	search_paths.clear();
	game_filesystem.reset();
}

// FIXME: ignores filesystem
std::shared_ptr<FileFinder::istream> FileFinder::openUTF8Input(const std::string& name,
	std::ios_base::openmode m)
{
	FileFinder::FindDefault(name);

	std::streamsize size = game_filesystem->GetFilesize(name);
	std::streambuf* buf = game_filesystem->CreateInputStreambuffer(name, m);

	std::shared_ptr<FileFinder::istream> ret(new FileFinder::istream(buf, size));

	return (*ret) ? ret : std::shared_ptr<FileFinder::istream>();
}

// FIXME: ignores filesystem
std::shared_ptr<std::ostream> FileFinder::openUTF8Output(const std::string& name, std::ios_base::openmode m)
{
	std::streamsize size = game_filesystem->GetFilesize(name);
	std::streambuf* buf = game_filesystem->CreateOutputStreambuffer(name, m);

	std::shared_ptr<std::ostream> ret(new std::ostream(buf));

	return (*ret) ? ret : std::shared_ptr<std::ofstream>();
}

std::string FileFinder::FindFile(FileFinder::DirectoryTree const& tree,
					 const std::string& dir,
					 const std::string& name,
					 char const* exts[]) {
	return ::FindFile(tree, dir, name, exts);
}

std::string FileFinder::FindImage(const std::string& dir, const std::string& name) {
#ifdef EMSCRIPTEN
	return FindDefault(dir, name);
#endif

	static const char* IMG_TYPES[] = { ".bmp",  ".png", ".xyz", NULL };
	return game_filesystem->FindFile(dir, name, IMG_TYPES);
}

std::string FileFinder::FindDefault(const std::string& dir, const std::string& name) {
	static const char* no_exts[] = {"", NULL};
	return game_filesystem->FindFile(dir, name, no_exts);
}

std::string FileFinder::FindDefault(const std::string& name) {
	return game_filesystem->FindDefault(name);
}

bool FileFinder::IsValidProject(const Filesystem& fs) {
	return IsRPG2kProject(fs) || IsEasyRpgProject(fs);
}

bool FileFinder::IsRPG2kProject(const Filesystem& fs) {
	// FIXME? Getter
	const DirectoryTree& dir = *(fs.directory_tree);

	string_map::const_iterator const
		ldb_it = dir.files.find(Utils::LowerCase(DATABASE_NAME)),
		lmt_it = dir.files.find(Utils::LowerCase(TREEMAP_NAME));

	return(ldb_it != dir.files.end() && lmt_it != dir.files.end());
}

bool FileFinder::IsEasyRpgProject(const Filesystem& fs){
	// FIXME? Getter
	const DirectoryTree& dir = *(fs.directory_tree);

	string_map::const_iterator const
		ldb_it = dir.files.find(Utils::LowerCase(DATABASE_NAME_EASYRPG)),
		lmt_it = dir.files.find(Utils::LowerCase(TREEMAP_NAME_EASYRPG));

	return(ldb_it != dir.files.end() && lmt_it != dir.files.end());
}

bool FileFinder::HasSavegame() {
	FilesystemRef fs = FileFinder::CreateSaveFilesystem();
	if (!fs) {
		return false;
	}

	for (int i = 1; i <= 15; i++) {
		std::stringstream ss;
		ss << "Save" << (i <= 9 ? "0" : "") << i << ".lsd";
		std::string filename = fs->FindDefault(ss.str());

		if (!filename.empty()) {
			return true;
		}
	}
	return false;
}

std::string FileFinder::FindMusic(const std::string& name) {
#ifdef EMSCRIPTEN
	return FindDefault("Music", name);
#endif

	static const char* MUSIC_TYPES[] = {
		".opus", ".oga", ".ogg", ".wav", ".mid", ".midi", ".mp3", ".wma", nullptr };
	return game_filesystem->FindFile("Music", name, MUSIC_TYPES);
}

std::string FileFinder::FindSound(const std::string& name) {
#ifdef EMSCRIPTEN
	return FindDefault("Sound", name);
#endif

	static const char* SOUND_TYPES[] = {
		".opus", ".oga", ".ogg", ".wav", ".mp3", ".wma", nullptr };
	return game_filesystem->FindFile("Sound", name, SOUND_TYPES);
}

bool FileFinder::Exists(const std::string& filename) {
	return game_filesystem->Exists(filename);
}

bool FileFinder::IsDirectory(const std::string& dir) {
	return game_filesystem->IsDirectory(dir);
}

//This namespace contains global variables which are needed for the recursive mode of
//GetDirectoryMembers - as well as the callback used to receive the parsed directory entries
namespace GetDirectoryMembersHelper {
	FileFinder::Directory temporary_directory;
	FileFinder::Mode mode;
	std::string parent;
	std::string root_path;
	uint32_t max_depth = 0;

	void ListDirectoryEntriesCallback(Filesystem const* filesystem,Filesystem::DirectoryEntry const & entry) {
		if (max_depth == 0) return; //recursion is a scary thing, so stay on the save side by limiting the depth.
		std::string combinedWithParent = Filesystem::CombinePath(parent, entry.name);

		switch (mode) {
		case FileFinder::Mode::FILES:
			if (entry.isDirectory) { return; } //Ignore this entry
			break;
		case FileFinder::Mode::DIRECTORIES:
			if (!entry.isDirectory) { return; } //Ignore this entry
			break;
		case FileFinder::Mode::ALL:
			break;
		case FileFinder::Mode::RECURSIVE:

			if (entry.isDirectory) {
				//this is tricky, let's comment
				//first store the current parent value in a temporary variable on stack - we want to restore it after the following calls
				std::string parent_temp = parent;
				//Now overwrite the global variable with the directory one step deeper (the one we now want the listing from)
				parent = combinedWithParent;
				//the same asserts for the depth ( the sub will never go beneath 0 as we check for null earlier)
				uint32_t current_depth = max_depth;
				max_depth--;
				//Now here comes the recursion - register this function itself as callback
				//Now this function will be called as much times as the subdirectory has entries
				filesystem->ListDirectoryEntries(Filesystem::CombinePath(root_path,parent), ListDirectoryEntriesCallback);
				//After the recursion took place we're back here - so let's restore our parent and depth
				parent = parent_temp;
				max_depth = current_depth;
				//and now let the calling function process the rest of the dir
				return;
			}

			//The entry is a file in recursive mode, store relative path
			temporary_directory.files[Utils::LowerCase(combinedWithParent)] = combinedWithParent;
			return;
		}
		if (entry.isDirectory) {
			//The entry is a directory in non recursive mode, store name
			temporary_directory.directories[Utils::LowerCase(entry.name)] = entry.name;
		}
		else {
			//The entry is a file in non recursive mode, store name
			temporary_directory.files[Utils::LowerCase(entry.name)] = entry.name;
		}
	}

}

FileFinder::Directory FileFinder::GetDirectoryMembers(const Filesystem& filesystem, std::string const& dir, Mode m, uint32_t max_depth){
	assert(filesystem.Exists(dir));
	assert(filesystem.IsDirectory(dir));

	//Initialize helpers
	GetDirectoryMembersHelper::temporary_directory.base = dir;
	GetDirectoryMembersHelper::temporary_directory.directories.clear();
	GetDirectoryMembersHelper::temporary_directory.files.clear();
	GetDirectoryMembersHelper::mode = m;
	GetDirectoryMembersHelper::parent = "";
	GetDirectoryMembersHelper::root_path = dir;
	GetDirectoryMembersHelper::max_depth = max_depth;

	filesystem.ListDirectoryEntries(dir, GetDirectoryMembersHelper::ListDirectoryEntriesCallback);

	return GetDirectoryMembersHelper::temporary_directory;
}


Offset FileFinder::GetFileSize(std::string const& file) {
	return game_filesystem->GetFilesize(file);
}

bool FileFinder::IsMajorUpdatedTree() {
	Offset size;
#if 0
	FIXME
	// Find an MP3 music file only when official Harmony.dll exists
	// in the gamedir or the file doesn't exist because
	// the detection doesn't return reliable results for games created with
	// "RPG2k non-official English translation (older engine) + MP3 patch"
	bool find_mp3 = true;
	std::string harmony = FindDefault("Harmony.dll");
	if (!harmony.empty()) {
		size = GetFileSize(harmony);
		if (size != -1 && size != KnownFileSize::OFFICIAL_HARMONY_DLL) {
			Output::Debug("Non-official Harmony.dll found, skipping MP3 test");
			find_mp3 = false;
		}
	}
	if (find_mp3) {
		const std::shared_ptr<DirectoryTree> tree = GetDirectoryTree();
		string_map::const_iterator const music_it = tree->directories.find("music");
		if (music_it != tree->directories.end()) {
			string_map mem = tree->sub_members["music"];
			for (auto& i : mem) {
				std::string file = mem[i.first];
				if (Utils::EndsWith(Utils::LowerCase(file), ".mp3")) {
					Output::Debug("MP3 file (%s) found", file.c_str());
					return true;
				}
			}
		}
	}

	// Compare the size of RPG_RT.exe with threshold
	std::string rpg_rt = FindDefault("RPG_RT.exe");
	if (!rpg_rt.empty()) {
		size = GetFileSize(rpg_rt);
		if (size != -1) {
			return size > (Player::IsRPG2k() ? RpgrtMajorUpdateThreshold::RPG2K : RpgrtMajorUpdateThreshold::RPG2K3);
		}
	}
	Output::Debug("Could not get the size of RPG_RT.exe");

	// Assume the most popular version
	// Japanese or RPG2k3 games: newer engine
	// non-Japanese RPG2k games: older engine
	bool assume_newer = Player::IsCP932() || Player::IsRPG2k3();
	Output::Debug("Assuming %s engine", assume_newer ? "newer" : "older");
	return assume_newer;
#endif
	return false;
}
