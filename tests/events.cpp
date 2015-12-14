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
	std::deque<std::tuple<char, int, int, int>> trace_list;
	std::tuple<int, int, int> old_event_notify;

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

			if (split_vec[0].empty()) {
				assert(false);
			} else if (split_vec[0][0] == 'E') {
				if (split_vec.size() != 4) {
					assert(false);
				}
				auto tup = std::make_tuple(split_vec[0][0], atoi(split_vec[1].c_str()), atoi(split_vec[2].c_str()), atoi(split_vec[3].c_str()));
				trace_list.push_back(tup);
			} else if (split_vec[0][0] == 'M') {
				if (split_vec.size() != 3) {
					assert(false);
				}
				auto tup = std::make_tuple(split_vec[0][0], atoi(split_vec[1].c_str()), atoi(split_vec[2].c_str()), -1);
				trace_list.push_back(tup);
			} else {
				assert(false);
			}
		}
	}

	void EventTracer(Game_Interpreter&, std::vector<RPG::EventCommand>&, int event_id, int page_id, int line_id) {
		if (trace_list.empty()) {
			// List empty but Player not finished -> test failed
			test_line = -2;
			return;
		}

		std::tuple<int, int, int> new_event_notify = std::make_tuple(event_id, page_id, line_id);
		if (old_event_notify == new_event_notify) {
			// RPG_RT handles events strange, e.g. "ProceedWithMovement" is notified only once
			// but Player notifies it each frame
			// Filter notifications of same line...
			return;
		}

		old_event_notify = new_event_notify;

		auto tup = trace_list.front();
		trace_list.pop_front();
		
		if (std::get<0>(tup) != 'E') {
			// Current line is not an event trace
			printf("Line %d not a event line (got %c)\n", test_line, std::get<0>(tup));
			trace_list.clear();
		} else if (event_id != std::get<1>(tup) || page_id != std::get<2>(tup) || line_id != std::get<3>(tup)) {
			// Trace not matching RPG_RT
			printf("Line %d: Event line. Mismatch\n", test_line);
			printf("%d\t%d\t%d reported\n", event_id, page_id, line_id);
			printf("%d\t%d\t%d expected\n", std::get<1>(tup), std::get<2>(tup), std::get<3>(tup));
			trace_list.clear();
		} else if (trace_list.empty()) {
			// Test passed
			test_line = 0;
		} else {
			++test_line;
		}
	}

	void MoveTracer(Game_Character& character, const RPG::MoveRoute&, int move_route_index) {
		if (trace_list.empty()) {
			// List empty but Player not finished -> test failed
			test_line = -3;
			return;
		}

		auto tup = trace_list.front();
		trace_list.pop_front();

		if (std::get<0>(tup) != 'M') {
			// Current line is not a move trace
			printf("Line %d not a move line (got %c)\n", test_line, std::get<0>(tup));
			trace_list.clear();
		} else if (character.GetMapId() != std::get<1>(tup) || move_route_index != std::get<2>(tup)) {
			// Trace not matching RPG_RT
			printf("Line %d: Movement line. Mismatch\n", test_line);
			printf("%d\t%d reported\n", character.GetMapId(), move_route_index);
			printf("%d\t%d expected\n", std::get<1>(tup), std::get<2>(tup));
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
			Game_Character::AddOnMoveCommandListener(MoveTracer);
			Player::AddOnUpdateListener(Updater);
			old_event_notify = std::make_tuple(-1, -1, -1);
		}

		~SetupEventTracer() {
			Player::exit_flag = false;
			Game_Interpreter::RemoveOnEventCommandListener(EventTracer);
			Game_Character::RemoveOnMoveCommandListener(MoveTracer);
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
	},

	CASE("Basic move command test") {
		SetupTestPlayer p;
		SetupEventTracer e;

		Player::new_game_flag = true;
		Player::start_map_id = 2;

		LoadTrace("event_tests/movebasic.txt");

		Player::Run();

		EXPECT(test_line == 0);
	}
};

extern lest::tests & specification();
MODULE(specification(), module)
