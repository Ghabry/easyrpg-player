#include "test_player.h"
#include "player.h"
#include "graphics.h"
#include "input.h"
#include "filefinder.h"
#include "main_data.h"

int PlayerArgs::argc;
char** PlayerArgs::argv;

SetupTestPlayer::SetupTestPlayer() {
	// Prevents creating of SdlUi
	DisplayUi = BaseUi::CreateDummyUi();

	// Standard init code
	Player::Init(PlayerArgs::argc, PlayerArgs::argv);
	Graphics::Init();
	Input::Init();

	// Create proper default tree
	FileFinder::SetDirectoryTree(FileFinder::CreateDirectoryTree(Main_Data::project_path));

	// Make Player non-blocking
	Player::non_stop_mode = true;
}

SetupTestPlayer::~SetupTestPlayer() {
	Player::Exit();
}
