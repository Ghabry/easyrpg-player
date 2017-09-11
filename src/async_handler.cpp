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

#include <cstdlib>
#include <map>

#ifdef EMSCRIPTEN
#  include <emscripten.h>
#  include <regex>
#endif

#include "async_handler.h"
#include "filefinder.h"
#include "memory_management.h"
#include "output.h"
#include "player.h"
#include "main_data.h"
#include <fstream>
#include "utils.h"

namespace {
	std::map<std::string, FileRequestAsync> async_requests;
	int next_id = 0;
	FileRequestAsync* startup_request = nullptr;

	FileRequestAsync* GetRequest(const std::string& path) {
		std::map<std::string, FileRequestAsync>::iterator it = async_requests.find(path);

		if (it != async_requests.end()) {
			return &(it->second);
		}
		return nullptr;
	}

	void RegisterRequest(const std::string& path, const FileRequestAsync& request) {
		async_requests[path] = request;
	}

	FileRequestBinding CreatePending() {
		return std::make_shared<int>(next_id++);
	}

#ifdef EMSCRIPTEN
	void download_success(unsigned, void* userData, const char*) {
		FileRequestAsync* req = static_cast<FileRequestAsync*>(userData);
		//Output::Debug("DL Success: %s", req->GetPath().c_str());
		req->DownloadDone(true);
	}

	void download_failure(unsigned, void* userData, int) {
		FileRequestAsync* req = static_cast<FileRequestAsync*>(userData);
		Output::Debug("DL Failure: %s", req->GetPath().c_str());
		req->DownloadDone(false);
	}

	//
	using request_file_func = void (*)(const char*, const char*, const char*, void*, em_async_wget2_onload_func, em_async_wget2_onstatus_func);
	using request_startup_func = void (*)(const char*, const char*, void*, em_async_wget2_onload_func, em_async_wget2_onstatus_func);

	// Emscripten shell configures these pointers
	request_file_func request_func;
	request_startup_func startup_func;

#endif
}

#ifdef EMSCRIPTEN
extern "C" {

void EMSCRIPTEN_KEEPALIVE set_request_handler(request_file_func func) {
	request_func = func;
}

void EMSCRIPTEN_KEEPALIVE set_startup_handler(request_startup_func func) {
	startup_func = func;
}

}
#endif

FileRequestAsync* AsyncHandler::RequestStartup() {
	startup_request = RequestFile("Startup", "Handler");

	return startup_request;
}

FileRequestAsync* AsyncHandler::RequestFile(const std::string& folder_name, const std::string& file_name) {
	std::string path = FileFinder::MakePath(folder_name, file_name);

	FileRequestAsync* request = GetRequest(path);

	if (request) {
		return request;
	}

	FileRequestAsync req(folder_name, file_name);
	RegisterRequest(path, req);

	//Output::Debug("Waiting for %s", path.c_str());

	return GetRequest(path);
}

FileRequestAsync* AsyncHandler::RequestFile(const std::string& file_name) {
	return RequestFile(".", file_name);
}

bool AsyncHandler::IsImportantFilePending() {
	std::map<std::string, FileRequestAsync>::iterator it;

	for (it = async_requests.begin(); it != async_requests.end(); ++it) {
		FileRequestAsync& request = it->second;
		// remove comment for fake download testing
		//request.UpdateProgress();

		if (!request.IsReady() && request.IsImportantFile()) {
			return true;
		}
	}

	return false;
}

FileRequestAsync::FileRequestAsync(const std::string& folder_name, const std::string& file_name) :
	directory(folder_name),
	file(file_name) {
	this->path = path = FileFinder::MakePath(folder_name, file_name);
	this->important = false;

	state = State_WaitForStart;
}

FileRequestAsync::FileRequestAsync() {
}

bool FileRequestAsync::IsReady() const {
	return state == State_DoneSuccess || state == State_DoneFailure;
}

bool FileRequestAsync::IsImportantFile() const {
	return important;
}

void FileRequestAsync::SetImportantFile(bool important) {
	this->important = important;
}

void FileRequestAsync::Start() {
	if (state == State_Pending) {
		return;
	}

	if (IsReady()) {
		// Fire immediately
		DownloadDone(true);
		return;
	}

	state = State_Pending;

#ifdef EMSCRIPTEN
	std::string game_url;
#  ifdef EM_GAME_URL
	game_url = EM_GAME_URL;
#  else
	game_url = "games/";
#  endif

	if (this == startup_request) {
		Output::Debug("STARTUP");
		startup_func(
			game_url.c_str(),
			Player::emscripten_game_name.c_str(),
			this,
			download_success,
			download_failure
		);
		return;
	}
	Output::Debug("DOWNLOAD");
	request_func(
		game_url.c_str(),
		Player::emscripten_game_name.c_str(),
		path.c_str(),
		this,
		download_success,
		download_failure
	);

	if (path == "\x0") {
		// Always false, just needed because otherwise the
		// function name can't be exported to emscripten.
		emscripten_async_wget2("", "", "", NULL, NULL, NULL, NULL, NULL);
	}
#else
#  ifdef EM_GAME_URL
#    warning EM_GAME_URL set and not an Emscripten build!
#  endif
	// add comment for fake download testing
	DownloadDone(true);
#endif
}

void FileRequestAsync::UpdateProgress() {
#ifndef EMSCRIPTEN
	// Fake download for testing event handlers

	if (!IsReady() && Utils::ChanceOf(1, 100)) {
		DownloadDone(true);
	}
#endif
}

const std::string& FileRequestAsync::GetPath() const {
	return path;
}

FileRequestBinding FileRequestAsync::Bind(void(*func)(FileRequestResult*)) {
	FileRequestBinding pending = CreatePending();

	listeners.push_back(std::make_pair(FileRequestBindingWeak(pending), func));

	return pending;
}

FileRequestBinding FileRequestAsync::Bind(std::function<void(FileRequestResult*)> func) {
	FileRequestBinding pending = CreatePending();

	listeners.push_back(std::make_pair(FileRequestBindingWeak(pending), func));

	return pending;
}

void FileRequestAsync::CallListeners(bool success) {
	FileRequestResult result;
	result.directory = directory;
	result.file = file;
	result.success = success;

	for (auto& listener : listeners) {
		if (!listener.first.expired()) {
			(listener.second)(&result);
		} else {
			Output::Debug("Request cancelled: %s", GetPath().c_str());
		}
	}

	listeners.clear();
}

void FileRequestAsync::DownloadDone(bool success) {
	if (IsReady()) {
		// Change to real success state when already finished before
		success = state == State_DoneSuccess;
	}

	if (success) {

#ifdef EMSCRIPTEN
		if (state == State_Pending) {
			// Update directory structure (new file was added)
			FileFinder::SetDirectoryTree(FileFinder::CreateDirectoryTree(Main_Data::GetProjectPath()));
		}
#endif

		state = State_DoneSuccess;

		CallListeners(true);
	}
	else {
		state = State_DoneFailure;

		CallListeners(false);
	}
}
