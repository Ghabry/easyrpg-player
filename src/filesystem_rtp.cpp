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

#include "filesystem_rtp.h"
#include "filefinder.h"
#include "filesystem_overlay.h"
#include "output.h"
#include "player.h"
#include "registry.h"
#include "rtp_table.h"
#include "utils.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <fstream>

namespace {
	typedef std::vector<FilesystemRef> search_path_list;
	search_path_list search_paths;
}

static void add_rtp_path(const std::string& p) {
	using namespace FileFinder;
	FilesystemRef fs(FileFinder::GetNativeFilesystem()->Create(p));
	if (fs) {
		Output::Debug("Adding %s to RTP path", p.c_str());
		search_paths.push_back(std::make_shared<RtpFilesystem>(fs));
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

void RtpFilesystem::InitRtpPaths(bool warn_no_rtp_found) {
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

	if (!search_paths.empty()) {
		// "Remount" game filesystem as a OverlayFilesystem and layer RTP below
		FilesystemRef game_fs = FileFinder::GetGameFilesystem();

		std::shared_ptr<OverlayFilesystem> overlay_fs(new OverlayFilesystem());
		overlay_fs->AddFilesystem(game_fs, 1);

		for (const auto& s : search_paths) {
			overlay_fs->AddFilesystem(s, 0);
		}

		FileFinder::SetGameFilesystem(overlay_fs);
	}
}

RtpFilesystem::RtpFilesystem(FilesystemRef wrapped_filesystem) : wrapped_fs(wrapped_filesystem) {
	assert(wrapped_filesystem && "wrapped_fs arg is null");
}

bool RtpFilesystem::IsFile(const std::string& path) const {
	return wrapped_fs->IsFile(path);
}

bool RtpFilesystem::IsDirectory(const std::string& path) const {
	return wrapped_fs->IsDirectory(path);
}

bool RtpFilesystem::Exists(const std::string& path) const {
	return wrapped_fs->Exists(path);
}

uint32_t RtpFilesystem::GetFilesize(const std::string& path) const {
	return wrapped_fs->GetFilesize(path);
}

std::streambuf* RtpFilesystem::CreateInputStreambuffer(const std::string& path, std::ios_base::openmode mode) const {
	return wrapped_fs->CreateInputStreambuffer(path, mode);
}

std::streambuf* RtpFilesystem::CreateOutputStreambuffer(const std::string& path, std::ios_base::openmode mode) const {
	return wrapped_fs->CreateOutputStreambuffer(path, mode);
}

std::vector<Filesystem::DirectoryEntry> RtpFilesystem::ListDirectory(const std::string &path, bool* error) const {
	return wrapped_fs->ListDirectory(path, error);
}

static bool is_not_ascii_char(uint8_t c) { return c > 0x80; }

static bool is_not_ascii_filename(const std::string& n) {
	return std::find_if(n.begin(), n.end(), &is_not_ascii_char) != n.end();
}

static const std::string translate_rtp(const std::string& dir, const std::string& name) {
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

std::string RtpFilesystem::FindFile(const std::string &dir, const std::string& name, const char* exts[]) const {
	// Check for the requested filename in RTP first
	std::string ret = Filesystem::FindFile(dir, name, exts);
	if (!ret.empty()) {
		return ret;
	}

	// Resolve Japanese -> English or Any -> Japanese
	const std::string& rtp_name = translate_rtp(dir, name);

	// Check for redirected RTP filename
	std::string const ret_rtp = Filesystem::FindFile(dir, rtp_name, exts);
	if (!ret_rtp.empty()) {
		return ret_rtp;
	}

	//Output::Debug("Cannot find: %s/%s (%s)", dir.c_str(), name.c_str(),
	//				rtp_name.c_str());

	return "";
}
