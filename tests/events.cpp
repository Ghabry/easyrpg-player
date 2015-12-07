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
#include "lest.hpp"
#include "test_player.h"

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
	std::deque<std::tuple<int, int, int>> trace_list;

	int test_line;

	void LoadTrace(const std::string& filename) {
		test_line = 1;

		trace_list.clear();
		std::ifstream input(filename);

		assert(input);

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
	}

	void EventTracer(const std::vector<RPG::EventCommand>&, int event_id, int page_id, int line_id) {
		auto tup = trace_list.front();
		trace_list.pop_front();
		
		if (event_id != std::get<0>(tup) || page_id != std::get<1>(tup) || line_id != std::get<2>(tup)) {
			// Trace not matching RPG_RT
			printf("Line %d: %d vs. %d, %d vs. %d, %d vs %d\n", test_line, event_id, std::get<0>(tup), page_id, std::get<1>(tup), line_id, std::get<2>(tup));
			trace_list.clear();
		} else if (trace_list.empty()) {
			// Test passed
			test_line = 0;
		} else {
			++test_line;
		}
	}
	
	void Updater(int frames) {
		if (frames > Graphics::GetDefaultFps() * 10) {
			// Longer then 10 seconds -> cancel test
			// (game play seconds, tests run without frame limit)
			test_line = -1;
			Player::exit_flag = true;
		}

		if (trace_list.empty()) {
			Player::exit_flag = true;
		}
	}

	class SetupEventTracer {
	public:
		SetupEventTracer() {
			Game_Interpreter::AddOnEventCommandListener(EventTracer);
			Player::AddOnUpdateListener(Updater);
		}

		~SetupEventTracer() {
			Player::exit_flag = false;
			Game_Interpreter::RemoveOnEventCommandListener(EventTracer);
			Player::RemoveOnUpdateListener(Updater);
		}
	};
}

const lest::test module[] = {
	CASE("Basic interpreter test") {
		SetupTestPlayer p;
		SetupEventTracer e;

		Player::new_game_flag = true;
		Player::start_map_id = 1;

		LoadTrace("event_tests/trivial.txt");

		Player::Run();

		EXPECT(test_line == 0);
	}
};

extern lest::tests & specification();
MODULE(specification(), module)
