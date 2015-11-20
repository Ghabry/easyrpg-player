#include <cstdlib>
#include "filefinder.h"
#include "player.h"
#include "reader_util.h"
#include "main_data.h"
#include "baseui.h"
#include "game_interpreter.h"
#include "game_message.h"
#include <game_switches.h>
#include <game_temp.h>
#include <input.h>
#include <regex>
#include <fstream>
#include <queue>
#include <string>

// via https://stackoverflow.com/questions/9435385/split-a-string-using-c11
std::vector<std::string> split(std::string& input, const std::string& regex) {
	// passing -1 as the submatch index parameter performs splitting
	std::regex re(regex);
	std::sregex_token_iterator
		first{ input.begin(), input.end(), re, -1 },
		last;
	return{ first, last };
}

namespace {	
	int i = 0;
	std::deque<std::tuple<int, int, int>> trace_list;
	int test_count = 0;
	int test_passed = 0;

	void LoadTrace(const std::string& filename) {
		trace_list.clear();
		std::ifstream input(filename);

		for (std::string line; getline(input, line); ) {
			if (line.size() == 0) {
				continue;
			}

			if (line[0] == '#') {
				continue;
			}

			auto split_vec = split(line, "\t");
			if (split_vec.size() != 3) {
				assert(false && "Bad trace log");
			}

			auto tup = std::make_tuple(atoi(split_vec[0].c_str()), atoi(split_vec[1].c_str()), atoi(split_vec[2].c_str()));
			trace_list.push_back(tup);
		}

		test_count++;
	}

	void EventTracer(const std::vector<RPG::EventCommand>&, int event_id, int page_id, int line_id) {
		auto tup = trace_list.front();
		trace_list.pop_front();
		
		if (event_id != std::get<0>(tup) || page_id != std::get<1>(tup) || line_id != std::get<2>(tup)) {
			// Trace not matching RPG_RT
			trace_list.clear();
		} else if (trace_list.empty()) {
			test_passed++;
		}
	}
	
	void Updater(int frames) {
		if (trace_list.empty()) {
			Player::exit_flag = true;
		}
	}
}

int main(int argc, char** argv) {
	// Prevents creating of SdlUi
	DisplayUi = BaseUi::CreateDummyUi();

	// Standard init code
	Player::Init(argc, argv);
	Graphics::Init();
	Input::Init();

	// Make ready for non-interactive session
	Player::new_game_flag = true;
	Game_Message::SetNonStopMode(true);

	LoadTrace("event_tests/trivial.txt");

	// Register callbacks
	Game_Interpreter::AddOnEventCommandListener(EventTracer);
	Player::AddOnUpdateListener(Updater);

	// Begin the test
	Player::Run();

	return test_count - test_passed;
}
