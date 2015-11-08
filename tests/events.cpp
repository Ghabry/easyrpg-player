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

namespace {	
	static int i = 0;

	void EventTracer(const std::vector<RPG::EventCommand>&, int event_id, int page_id, int line_id) {
		// TODO:
		// Load a trace dump (from dynrpg) and compare it with the execution of Player
		Output::Debug("%d\t%d\t%d", event_id, page_id, line_id);
	}
	
	void Updater(int frames) {
		// TODO:
		// this function should shutdown the player when the event execution finished
		if (i == 5) {
			// TODO: Just some testing code, remove later
			Game_Interpreter::RemoveOnEventCommandListener(event_tracer);
		}
		if (frames == 3000) {
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

	// Register callbacks
	Game_Interpreter::AddOnEventCommandListener(EventTracer);
	Player::AddOnUpdateListener(Updater);

	// Begin the test
	Player::Run();

	return EXIT_SUCCESS;
}
