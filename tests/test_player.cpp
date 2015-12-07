#include "test_player.h"
#include "player.h"
#include "graphics.h"
#include "input.h"

int PlayerArgs::argc;
char** PlayerArgs::argv;

SetupTestPlayer::SetupTestPlayer() {
	// Prevents creating of SdlUi
	DisplayUi = BaseUi::CreateDummyUi();

	// Standard init code
	Player::Init(PlayerArgs::argc, PlayerArgs::argv);
	Graphics::Init();
	Input::Init();

	// Make Player non-blocking
	Player::non_stop_mode = true;
}

SetupTestPlayer::~SetupTestPlayer() {
	Player::non_stop_mode = false;
	Player::Exit();
}
