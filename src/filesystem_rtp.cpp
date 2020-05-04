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
#include "output.h"
#include "player.h"
#include "registry.h"
#include "rtp.h"
#include "utils.h"
#include "reader_util.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <fstream>

namespace {
	typedef std::vector<FilesystemRef> search_path_list;
	search_path_list search_paths;

	struct {
		/** all RTP search paths */
		search_path_list search_paths;
		/** RTP was disabled with --disable-rtp */
		bool disable_rtp = true;
		/** Game has FullPackageFlag=1, RTP will still be used as RPG_RT does */
		bool game_has_full_package_flag = false;
		/** warning about "game has FullPackageFlag=1 but needs RTP" shown */
		bool warning_broken_rtp_game_shown = false;
		/** RTP candidates per search_path */
		std::vector<RTP::RtpHitInfo> detected_rtp;
		/** the RTP the game uses, when only one left the RTP of the game is known */
		std::vector<RTP::Type> game_rtp;
	} rtp_state;

	// returns empty string when the file is not belonging to an RTP
	const std::string rtp_lookup(const std::string& dir, const std::string& name, const char* exts[], bool& is_rtp_asset) {
		int version = Player::EngineVersion();

		auto normal_search = [&]() -> std::string {
			is_rtp_asset = false;
			for (const auto path : rtp_state.search_paths) {
				const std::string ret = path->FindFile(dir, name, exts);
				if (!ret.empty()) {
					return ret;
				}
			}
			return std::string();
		};

		// Detect the RTP version the game uses, when only one candidate is left the RTP is known
		if (rtp_state.game_rtp.size() != 1) {
			auto candidates = RTP::LookupAnyToRtp(dir, name, version);

			// Prevent Don Miguel RTP addon data from being detected as game RTP because a game can only have one RTP
			// and using this one will break the whole lookup table logic.
			auto addon_it = std::find(candidates.begin(), candidates.end(), RTP::Type::RPG2000_DonMiguelAddon);
			if (addon_it != candidates.end()) {
				candidates.erase(addon_it);
			}

			// when empty the requested asset does not belong to any (known) RTP
			if (!candidates.empty()) {
				if (rtp_state.game_rtp.empty()) {
					rtp_state.game_rtp = candidates;
				} else {
					// Strategy: Remove all RTP that are not candidates by comparing with all previous candidates
					// as the used RTP can only be the one that contains all by now requested assets
					for (auto it = rtp_state.game_rtp.begin(); it != rtp_state.game_rtp.end();) {
						if (std::find(candidates.begin(), candidates.end(), *it) == candidates.end()) {
							it = rtp_state.game_rtp.erase(it);
						} else {
							++it;
						}
					}
				}

				if (rtp_state.game_rtp.size() == 1) {
					// From now on the RTP lookups should be perfect
					Output::Debug("Game uses RTP \"%s\"", RTP::Names[(int) rtp_state.game_rtp[0]]);
				}
			}
		}

		if (rtp_state.game_rtp.empty()) {
			// The game RTP is currently unknown because all requested assets by now were not in any RTP
			// -> fallback to direct search
			is_rtp_asset = false;
			return normal_search();
		}

		// Search across all RTP
		for (const auto& rtp : rtp_state.detected_rtp) {
			for (RTP::Type game_rtp : rtp_state.game_rtp) {
				std::string rtp_entry = RTP::LookupRtpToRtp(dir, name, game_rtp, rtp.type, &is_rtp_asset);
				if (!rtp_entry.empty()) {
					const std::string ret = rtp.tree->FindFile(dir, rtp_entry, exts);
					if (!ret.empty()) {
						is_rtp_asset = true;
						return ret;
					}
				}
			}
		}

		// Asset is missing or not a RTP asset -> fallback to direct search
		return normal_search();
	}

	std::string FindFile(const std::string &dir, const std::string& name, const char* exts[]) {
#if 0
		std::string ret = rtp.tree->FindFile(dir, name, exts);
		if (!ret.empty()) {
			return ret;
		}

		// True RTP if enabled and available
		if (!rtp_state.disable_rtp) {
			bool is_rtp_asset;
			ret = rtp_lookup(ReaderUtil::Normalize(dir), ReaderUtil::Normalize(name), exts, is_rtp_asset);

			std::string lcase = ReaderUtil::Normalize(dir);
			bool is_audio_asset = lcase == "music" || lcase == "sound";

			if (is_rtp_asset) {
				if (!ret.empty() && rtp_state.game_has_full_package_flag && !rtp_state.warning_broken_rtp_game_shown && !is_audio_asset) {
					rtp_state.warning_broken_rtp_game_shown = true;
					Output::Warning("This game claims it does not need the RTP, but actually uses files from it!");
				} else if (ret.empty() && !rtp_state.game_has_full_package_flag && !is_audio_asset) {
					std::string msg = "Cannot find: %s/%s. " +
									  std::string(rtp_state.search_paths.empty() ?
												  "Install RTP %d to resolve this warning." : "RTP %d was probably not installed correctly.");
					Output::Warning(msg.c_str(), dir.c_str(), name.c_str(), Player::EngineVersion());
				}
			}
		}

		if (ret.empty()) {
			Output::Debug("Cannot find: %s/%s", dir.c_str(), name.c_str());
		}

		return ret;
#endif
		return "";
	}
}

static void add_rtp_path(const std::string& p) {
#if 0
	using namespace FileFinder;
	std::shared_ptr<DirectoryTree> tree(CreateDirectoryTree(p));
	if (tree) {
		Output::Debug("Adding %s to RTP path", p.c_str());
		rtp_state.search_paths.push_back(tree);

		auto hit_info = RTP::Detect(tree, Player::EngineVersion());

		if (hit_info.empty()) {
			Output::Debug("The folder does not contain a known RTP!");
		}

		// Only consider the best RTP hits (usually 100% if properly installed)
		float best = 0.0;
		for (const auto& hit : hit_info) {
			float rate = (float)hit.hits / hit.max;
			if (rate >= best) {
				Output::Debug("RTP is \"%s\" (%d/%d)", hit.name.c_str(), hit.hits, hit.max);
				rtp_state.detected_rtp.emplace_back(hit);
				best = rate;
			}
		}
	} else {
		Output::Debug("RTP path %s is invalid, not adding", p.c_str());
	}
#endif
}

#if defined(USE_WINE_REGISTRY) || defined(_WIN32)
static void read_rtp_registry(const std::string& company, const std::string& product, const std::string& key) {
	std::string rtp_path = Registry::ReadStrValue(HKEY_CURRENT_USER, "Software\\" + company + "\\" + product, key, KEY32);
	if (!rtp_path.empty()) {
		add_rtp_path(rtp_path);
	}

	rtp_path = Registry::ReadStrValue(HKEY_LOCAL_MACHINE, "Software\\" + company + "\\" + product, key, KEY32);
	if (!rtp_path.empty()) {
		add_rtp_path(rtp_path);
	}
}
#endif

void RtpFilesystem::InitRtpPaths(bool disable_rtp, bool no_rtp_warnings) {
	rtp_state = {};

#ifdef EMSCRIPTEN
	// No RTP support for emscripten at the moment.
	rtp_state.disable_rtp = true;
#else
	rtp_state.disable_rtp = disable_rtp;
#endif
	rtp_state.game_has_full_package_flag = no_rtp_warnings;

	if (rtp_state.disable_rtp) {
		Output::Debug("RTP support is disabled.");
		return;
	}

	std::string const version_str =	Player::GetEngineVersion();
	assert(!version_str.empty());

#ifdef GEKKO
	add_rtp_path("sd:/data/rtp/" + version_str);
	add_rtp_path("usb:/data/rtp/" + version_str);
#elif defined(__SWITCH__)
	add_rtp_path("./rtp/" + version_str);
	add_rtp_path("/switch/easyrpg-player/rtp/" + version_str);
#elif defined(_3DS)
	add_rtp_path("romfs:/data/rtp/" + version_str);
	add_rtp_path("sdmc:/data/rtp/" + version_str);
#elif defined(PSP2)
	add_rtp_path("ux0:/data/easyrpg-player/rtp/" + version_str);
#elif defined(USE_LIBRETRO)
	const char* dir = nullptr;
	if (LibretroUi::environ_cb(RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY, &dir) && dir) {
		add_rtp_path(std::string(dir) + "/rtp/" + version_str);
	}
	if (LibretroUi::environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir) {
		add_rtp_path(std::string(dir) + "/rtp/" + version_str);
	}
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
	add_rtp_path(cs + "/" + version_str);
#elif defined(USE_WINE_REGISTRY) || defined(_WIN32)
	std::string const product = "RPG" + version_str;
	if (Player::IsRPG2k()) {
		// Prefer original 2000 RTP over Kadokawa, because there is no
		// reliable way to detect this engine and much more 2k games
		// use the non-English version
		read_rtp_registry("ASCII", product, "RuntimePackagePath");
		read_rtp_registry("KADOKAWA", product, "RuntimePackagePath");
	}
	else if (Player::IsRPG2k3E()) {
		// Prefer Kadokawa RTP over Enterbrain for new RPG2k3
		read_rtp_registry("KADOKAWA", product, "RuntimePackagePath");
		read_rtp_registry("Enterbrain", product, "RUNTIMEPACKAGEPATH");
	}
	else if (Player::IsRPG2k3()) {
		// Original 2003 RTP installer registry key is upper case
		// and Wine registry is case insensitive but new 2k3v1.10 installer is not
		// Prefer Enterbrain RTP over Kadokawa for old RPG2k3 (search order)
		read_rtp_registry("Enterbrain", product, "RUNTIMEPACKAGEPATH");
		read_rtp_registry("KADOKAWA", product, "RuntimePackagePath");
	}

	// Our RTP is for all engines
	read_rtp_registry("EasyRPG", "RTP", "path");
#else
	// Fallback for unknown platforms
	add_rtp_path("/data/rtp/" + version_str);
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

#ifdef USE_XDG_RTP
	std::string xdg_rtp;

	// Search in the local data directory
	xdg_rtp = getenv("XDG_DATA_HOME") ? std::string(getenv("XDG_DATA_HOME")) :
			  std::string(getenv("HOME")) + "/.local/share";
	xdg_rtp += "/rtp/" + version_str;
	if (FileFinder::GetNativeFilesystem()->Exists(xdg_rtp)) {
		env_paths.push_back(xdg_rtp);
	}

	// Search in the global data directories
	xdg_rtp = getenv("XDG_DATA_DIRS") ? std::string(getenv("XDG_DATA_DIRS")) :
			  std::string("/usr/local/share/:/usr/share/");
	std::vector<std::string> tmp = Utils::Tokenize(xdg_rtp, f);
	for (const std::string& p : tmp) {
		xdg_rtp = p + (p.back() == '/' ? "" : "/") + "rtp/" + version_str;
		if (FileFinder::GetNativeFilesystem()->Exists(xdg_rtp)) {
			env_paths.push_back(xdg_rtp);
		}
	}
#endif

	// Add all found paths from the environment
	for (const std::string& p : env_paths) {
		add_rtp_path(p);
	}
}

RtpFilesystem::RtpFilesystem(const std::string& base_path, FilesystemRef wrapped_filesystem) :
		Filesystem(base_path), wrapped_fs(wrapped_filesystem) {
	assert(wrapped_filesystem && "wrapped_fs arg is null");
}

bool RtpFilesystem::IsFileImpl(const std::string& path) const {
	return wrapped_fs->IsFile(path);
}

bool RtpFilesystem::IsDirectoryImpl(const std::string& path, bool follow_symlinks) const {
	return wrapped_fs->IsDirectory(path, follow_symlinks);
}

bool RtpFilesystem::ExistsImpl(const std::string& path) const {
	return wrapped_fs->Exists(path);
}

int64_t RtpFilesystem::GetFilesizeImpl(const std::string& path) const {
	return wrapped_fs->GetFilesize(path);
}

std::streambuf* RtpFilesystem::CreateInputStreambufferImpl(const std::string& path, std::ios_base::openmode mode) {
	return wrapped_fs->CreateInputStreambuffer(path, mode);
}

std::streambuf* RtpFilesystem::CreateOutputStreambufferImpl(const std::string& path, std::ios_base::openmode mode) {
	return wrapped_fs->CreateOutputStreambuffer(path, mode);
}

std::vector<Filesystem::DirectoryEntry> RtpFilesystem::ListDirectoryImpl(const std::string &path, bool* error) const {
	return wrapped_fs->ListDirectory(path, error);
}

static bool is_not_ascii_char(uint8_t c) { return c > 0x80; }

static bool is_not_ascii_filename(const std::string& n) {
	return std::find_if(n.begin(), n.end(), &is_not_ascii_char) != n.end();
}

static const std::string translate_rtp(const std::string& dir, const std::string& name) {
#if 0
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
#endif
	return "";
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
