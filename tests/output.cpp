#include "output.h"
#include "main_data.h"

#include "lest.hpp"

const lest::test module[] = {
	CASE("Output") {
		SETUP("Main_Data") {
			Main_Data::Init();

			Output::Debug("Test %s", "debg");
			Output::Warning("Test %s", "test");
			Output::Post("Test %s", "post");
		}
	}
};

extern lest::tests & specification();
MODULE(specification(), module)