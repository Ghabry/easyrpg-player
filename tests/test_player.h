#ifndef _EASYRPG_TEST_UTILS_
#define _EASYRPG_TEST_UTILS_

namespace PlayerArgs {
	extern int argc;
	extern char** argv;
}

class SetupTestPlayer {
public:
	SetupTestPlayer();

	~SetupTestPlayer();
};

#endif