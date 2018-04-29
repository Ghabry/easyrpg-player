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
#include "main_data.h"
#include "filesystem_os.h"
#include "filesystem_zip.h"

namespace {
#ifdef SUPPORT_MOVIES
	const char* const MOVIE_TYPES[] = { ".avi", ".mpg" };
#endif

	std::string fonts_path;
} // anonymous namespace

const FilesystemRef FileFinder::GetGameFilesystem() {
	return game_filesystem;
}

void FileFinder::SetGameFilesystem(FilesystemRef filesystem) {
	game_filesystem = filesystem;
}

FilesystemRef FileFinder::GetNativeFilesystem() {
	// ToDo: Support an optional path argument which support namespaces,
	// e.g. apk:// for accessing the APK on Android

	static FilesystemRef native_fs = std::make_shared<OSFilesystem>("");

	return native_fs;
}

const FilesystemRef FileFinder::CreateSaveFilesystem() {
	std::string save_path = Main_Data::GetSavePath();

	FilesystemRef fs = FileFinder::GetNativeFilesystem()->Create(save_path);
	if (!fs) {
		Output::Warning("Save game directory %s is invalid. Saving will not work.", save_path.c_str());
		return FilesystemRef();
	}

	return fs;
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

void FileFinder::Quit() {
	game_filesystem.reset();
}

std::shared_ptr<std::istream> FileFinder::OpenInputStream(const std::string &name,
														  std::ios_base::openmode m) {
	std::streambuf* buf = game_filesystem->CreateInputStreambuffer(name, m);

	std::shared_ptr<std::istream> ret(new std::istream(buf));

	return (*ret) ? ret : std::shared_ptr<std::istream>();
}

std::shared_ptr<std::ostream> FileFinder::OpenOutputStream(const std::string &name, std::ios_base::openmode m) {
	std::streambuf* buf = game_filesystem->CreateOutputStreambuffer(name, m);

	std::shared_ptr<std::ostream> ret(new std::ostream(buf));

	return (*ret) ? ret : std::shared_ptr<std::ofstream>();
}

std::string FileFinder::FindImage(const std::string& dir, const std::string& name) {
#ifdef EMSCRIPTEN
	return game_filesystem->FindFile(dir, name);
#endif

	static const char* IMG_TYPES[] = { ".bmp",  ".png", ".xyz", NULL };
	return game_filesystem->FindFile(dir, name, IMG_TYPES);
}

std::string FileFinder::FindDefault(const std::string& dir, const std::string& name) {
	static const char* no_exts[] = {"", NULL};
	return game_filesystem->FindFile(dir, name, no_exts);
}

std::string FileFinder::FindDefault(const std::string& name) {
	return game_filesystem->FindFile(name);
}

bool FileFinder::HasSavegame() {
	FilesystemRef fs = FileFinder::CreateSaveFilesystem();
	if (!fs) {
		return false;
	}

	for (int i = 1; i <= 15; i++) {
		std::stringstream ss;
		ss << "Save" << (i <= 9 ? "0" : "") << i << ".lsd";
		std::string filename = fs->FindFile(ss.str());

		if (!filename.empty()) {
			return true;
		}
	}
	return false;
}

std::string FileFinder::FindMusic(const std::string& name) {
#ifdef EMSCRIPTEN
	return game_filesystem->FindFile("Music", name);
#endif

	static const char* MUSIC_TYPES[] = {
		".opus", ".oga", ".ogg", ".wav", ".mid", ".midi", ".mp3", ".wma", nullptr };
	return game_filesystem->FindFile("Music", name, MUSIC_TYPES);
}

std::string FileFinder::FindSound(const std::string& name) {
#ifdef EMSCRIPTEN
	return game_filesystem->FindFile("Sound", name);
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

Offset FileFinder::GetFileSize(std::string const& file) {
	return game_filesystem->GetFilesize(file);
}

bool FileFinder::IsValidProject() {
	return IsValidProject(game_filesystem);
}

bool FileFinder::IsValidProject(FilesystemRef fs) {
	return IsRPG2kProject(fs) || IsEasyRpgProject(fs);
}

bool FileFinder::IsRPG2kProject(FilesystemRef fs) {
	return fs->Exists(DATABASE_NAME) && fs->Exists(TREEMAP_NAME);
}

bool FileFinder::IsEasyRpgProject(FilesystemRef fs) {
	return fs->Exists(DATABASE_NAME_EASYRPG) && fs->Exists(TREEMAP_NAME_EASYRPG);
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
