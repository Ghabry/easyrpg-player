#include "filesystem.h"
#include <algorithm>


std::string Filesystem::CombinePath(std::string const & dir, std::string const & entry) {
	std::string str = dir.empty() ? entry : dir + "/" + entry;
#ifdef _WIN32
	std::replace(str.begin(), str.end(), '/', '\\');
#else
	std::replace(str.begin(), str.end(), '\\', '/');
#endif
	return str;
}