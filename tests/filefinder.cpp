#include <cstdlib>
#include "filefinder.h"
#include "main_data.h"
#include "lest.hpp"
#include "test_player.h"

const lest::test module[] = {
	CASE("Project directory is a RPG2k project") {
		SetupTestPlayer p;

		auto tree = FileFinder::CreateDirectoryTree(Main_Data::project_path);
		EXPECT(FileFinder::IsRPG2kProject(*tree));
	},

	CASE(". IsDirectory") {
		SetupTestPlayer p;

		EXPECT(FileFinder::IsDirectory("."));
	},

	CASE("Find RPG_RT.ldb") {
		SetupTestPlayer p;

		const std::string& name = "RPG_RT.ldb";
		const std::string& find_result = FileFinder::FindDefault(name);
		size_t pos = find_result.find(name);

		EXPECT(pos != std::string::npos);
		EXPECT(pos == find_result.size() - name.size());
	}
};

extern lest::tests & specification();
MODULE(specification(), module)
