#include "output.h"
#include "main_data.h"
#include "lest.hpp"
#include "test_player.h"

const lest::test module[] = {
	CASE("Output") {
		SetupTestPlayer p;

		Output::Debug("Test %s", "debg");
		Output::Warning("Test %s", "test");
		Output::Post("Test %s", "post");
	}
};

extern lest::tests & specification();
MODULE(specification(), module)
