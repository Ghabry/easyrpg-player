#include "lest.hpp"
#include "test_player.h"

lest::tests & specification() {
	static lest::tests tests;
	return tests;
}

int main(int argc, char * argv[]) {
	PlayerArgs::argc = argc;
	PlayerArgs::argv = argv;

	return lest::run(specification(), argc, argv, std::cout);
}
