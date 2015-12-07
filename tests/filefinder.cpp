#include <cstdlib>
#include "filefinder.h"
#include "main_data.h"
#include "lest.hpp"
#include "test_player.h"

const lest::test module[] = {
	CASE("Project directory is a RPG2k project") {
		SetupTestPlayer;

		auto tree = FileFinder::CreateProjectTree(Main_Data::project_path);
		EXPECT(FileFinder::IsRPG2kProject(*tree));
	},

	CASE(". IsDirectory") {
		SetupTestPlayer;

		EXPECT(FileFinder::IsDirectory("."));
	},

	CASE("Find RPG_RT.ldb") {
		SetupTestPlayer;

		EXPECT(!FileFinder::FindDefault("RPG_RT.ldb").empty());
	}
};

extern lest::tests & specification();
MODULE(specification(), module)
