#ifndef LCF_STREAM_HXX
#define LCF_STREAM_HXX

#include <iosfwd>
#include <stdint.h>

namespace LCF {
	bool is_eof(std::istream& is);

	std::string convert(std::string const& data, char const* to, char const* from);

	size_t ber_size(uint32_t const v);
	uint32_t ber(std::istream& is);
	std::ostream& ber(std::ostream& os, uint32_t const v);

	std::string read_string(std::istream& is, size_t const s);
	std::string read_string(std::istream& is);
	void write_string(std::ostream& os, std::string const& str);
	void write_string_without_size(std::ostream& os, std::string const& str);

	size_t writing_string_size(std::string const& s);

}

#endif
