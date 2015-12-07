#include <cassert>
#include <cstdlib>
#include "utils.h"
#include "lest.hpp"

const lest::test module[] = {
	CASE("LC conv: EasyRPG -> easyrpg") {
		EXPECT(Utils::LowerCase("EasyRPG") == "easyrpg");
	},

	CASE("LC conv: player -> player") {
		EXPECT(Utils::LowerCase("player") == "player");
	}
};

extern lest::tests & specification();
MODULE(specification(), module)