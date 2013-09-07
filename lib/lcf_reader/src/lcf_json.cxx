#include "lcf_reader.hxx"
#include "lcf_stream.hxx"
#include "lcf_sym.hxx"

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/range/numeric.hpp>

#include <ciso646>

namespace {

namespace sym = LCF::sym;
using LCF::ber_size;
using LCF::ber;
using boost::lexical_cast;
using namespace boost;
using boost::optional;

size_t calculate_array1d_size(picojson const& data, picojson const& schema) {
	size_t ret = 0;

	for(picojson::object::const_iterator i = data.o().begin(); i != data.o().end(); ++i) {
		// skip "_rest"
		if(i->first == sym::_rest) { continue; }

		picojson const& sch = LCF::find_schema(schema, i->first);

		if(sch.is<picojson::null>()) {
			ret += i->second.a().size();
			continue;
		}

		// skip default value
		if(sch.contains(sym::value) and sch[sym::value] == i->second) { continue; }

		size_t const s = LCF::calculate_size(i->second, sch);
		ret += ber_size(sch[sym::index].i()) + ber_size(s) + s;
	}
	ret += ber_size(0);

	assert(ret != 0);
	return ret;
}

size_t calculate_array2d_size(picojson const& data, picojson const& schema) {
	size_t ret = 0;
	size_t elem_num = data.o().size();

	for(picojson::object::const_iterator i = data.o().begin(); i != data.o().end(); ++i) {
		int const idx = lexical_cast<int>(i->first.get());
		ret += ber_size(idx) + calculate_array1d_size(i->second, schema);

		if(not i->second.contains(sym::_rest)) { continue; }

		picojson::array const& rest = i->second[sym::_rest].a();

		elem_num += i->second[sym::_rest].a().size();
		LCF::vector<size_t> tmp(rest.size());
		boost::transform(rest, tmp.begin(),
						 bind(&calculate_array1d_size, _1, boost::cref(schema)));
		ret += boost::accumulate(tmp, 0)
			   + ber_size(idx) * rest.size();
	}

	ret += ber_size(elem_num);

	assert(ret != 0);
	return ret;
}

template<class T>
void save_array(picojson::array const& ary, std::ostream& os) {
	for(picojson::array::const_iterator i = ary.begin(); i != ary.end(); ++i) {
		T const val = i->i();
		for(size_t elem_idx = 0; elem_idx < sizeof(T); ++elem_idx) {
			os.put((val >> (8 * elem_idx)) & 0xffU);
		}
	}
}

void save_event_command(picojson const& data, std::ostream& os) {
	ber(os, data[sym::code].i());
	ber(os, data[sym::nest].i());
	LCF::write_string(os, data[sym::string].s());

	picojson::array const& args = data[sym::args].a();
	ber(os, args.size());
	for(picojson::array::const_iterator i = args.begin(); i != args.end(); ++i)
	{ ber(os, i->i()); }
}

typedef optional<picojson const&> picojson_opt;

}

void LCF::save_array1d(picojson const& data, picojson const& schema, std::ostream& os) {
	typedef boost::container::flat_map<uint32_t, std::pair<picojson_opt, picojson_opt> > sorted_type;
	sorted_type sorted;
	sorted.reserve(data.o().size());
	for(picojson::object::const_iterator i = data.o().begin(); i != data.o().end(); ++i) {
		// skip _rest
		if(i->first.get() == sym::_rest) { continue; }

		picojson const& s = find_schema(schema, i->first);

		uint32_t const idx = s.is<picojson::null>()
							 ? lexical_cast<int>(i->first.get()) : s[sym::index].i();
		sorted.insert(sorted_type::value_type(
			idx, std::make_pair<picojson_opt, picojson_opt>(s, i->second)));
	}

	for(sorted_type::const_iterator i = sorted.begin(); i != sorted.end(); ++i) {
		picojson const& s = *(i->second.first);
		picojson const& val = *(i->second.second);

		// unknown chunk
		if(s.is<picojson::null>()) {
			ber(os, i->first);

			picojson::array const& ary = val.a();
			ber(os, ary.size());

			for(size_t idx = 0; idx < ary.size(); ++idx) {
				assert(0 <= ary[idx].i() && ary[idx].i() < 0x100);
				os.put(ary[idx].i());
			}

			continue;
		}

		// skip default value
		if(s.contains(sym::value) and s[sym::value] == val) { continue; }

		ber(os, i->first);

		size_t const size = LCF::calculate_size(val, s);
		ber(os, size);

		size_t const base = os.tellp();
		LCF::save_element(val, s, os);

		assert((size + base) == size_t(os.tellp()));
	}
	ber(os, 0);
}

void LCF::save_array2d(picojson const& data, picojson const& schema, std::ostream& os) {
	typedef boost::container::flat_multimap<uint32_t, picojson_opt> sorted_type;
	sorted_type sorted;
	sorted.reserve(data.o().size());
	for(picojson::object::const_iterator i = data.o().begin(); i != data.o().end(); ++i) {
		uint32_t const idx = lexical_cast<int>(i->first.get());
		sorted.insert(sorted_type::value_type(idx, i->second));

		// _rest
		if(not i->second.contains(sym::_rest)) { continue; }
		picojson::array const& ary = i->second[sym::_rest].a();
		for(picojson::array::const_iterator r = ary.begin(); r != ary.end(); ++r) {
			sorted.insert(sorted_type::value_type(idx, *r));
		}
	}

	ber(os, sorted.size());
	for(sorted_type::const_iterator i = sorted.begin(); i != sorted.end(); ++i) {
		ber(os, i->first);
		save_array1d(*i->second, schema, os);
	}
}

void LCF::save_element(picojson const& data, picojson const& schema, std::ostream& os) {
	picojson::string const& type = schema[sym::type].s();

	if(type == sym::map_tree) {
		picojson::array const& ary = data["nodes"].a();

		ber(os, ary.size());
		for(picojson::array::const_iterator i = ary.begin(); i != ary.end(); ++i)
		{ ber(os, i->i()); }

		ber(os, data["active_node"].i());
	} else if(type == sym::integer) { ber(os, data.i()); }
	else if(type == sym::string) {
		write_string_without_size(os, data.s().get());
	} else if(type == sym::float_) {
		double d = data.d();

#ifdef BOOST_LITTLE_ENDIAN
		// don't do anything
#elif defined BOOST_BIG_ENDIAN
		// swap endianess
		uint8_t* data = reinterpret_cast<uint8_t*>(&d);
		std::swap(data[0], data[7]);
		std::swap(data[1], data[6]);
		std::swap(data[2], data[5]);
		std::swap(data[3], data[4]);
#else
#error unknown endianess
#endif

		os.write(reinterpret_cast<char const*>(&d), sizeof(double));
	} else if(type == sym::bool_) { ber(os, data.b()); }
	else if(type == sym::array1d) {
		save_array1d(data, actual_schema(schema, sym::array1d), os);
	} else if(type == sym::array2d) {
		save_array2d(data, actual_schema(schema, sym::array2d), os);
	} else if(type == sym::event) {
		boost::for_each(data.a(), boost::bind(save_event_command, _1, boost::ref(os)));
	} else if(type == sym::int8array) { save_array<uint8_t>(data.a(), os); }
	else if(type == sym::int16array) { save_array<int16_t>(data.a(), os); }
	else if(type == sym::int32array) { save_array<int32_t>(data.a(), os); }
	else if(type == sym::ber_array) {
		picojson::array const& ary = data.a();
		for(picojson::array::const_iterator i = ary.begin(); i != ary.end(); ++i)
		{ ber(os, i->i()); }
	} else {
		picojson const& actual_type = get_schema(type);
		assert(not actual_type.is<picojson::null>());
		if(actual_type[sym::type].s() == sym::array1d) {
			save_array1d(data, actual_type, os);
		} else if(actual_type[sym::type].s() == sym::array2d) {
			save_array2d(data, actual_type, os);
		} else { assert(false); }
	}
}

size_t LCF::calculate_size(picojson const& data, picojson const& schema) {
	picojson::string const& type = schema[sym::type].s();

	size_t ret = 0;

	if(type == sym::map_tree) {
		picojson::array const& ary = data["nodes"].a();
		ret += ber_size(ary.size());
		for(picojson::array::const_iterator i = ary.begin(); i != ary.end(); ++i)
		{ ret += ber_size(i->i()); }
		ret += ber_size(data["active_node"].i());
	}
	else if(type == sym::integer) { return ber_size(data.i()); }
	else if(type == sym::string) { return writing_string_size(data.s().get()); }
	else if(type == sym::float_) { return 8; }
	else if(type == sym::bool_) { return 1; }
	else if(type == sym::array1d) {
		return calculate_array1d_size(data, actual_schema(schema, sym::array1d));
	}
	else if(type == sym::array2d) {
		return calculate_array2d_size(data, actual_schema(schema, sym::array2d));
	}
	else if(type == sym::event) {
		for(picojson::array::const_iterator i = data.a().begin(); i != data.a().end(); ++i) {
			ret += ber_size((*i)[sym::code].i()) + ber_size((*i)[sym::nest].i());
			size_t const str_size = writing_string_size((*i)[sym::string].s().get());
			ret += ber_size(str_size) + str_size;
			picojson::array const& args = (*i)[sym::args].a();
			ret += ber_size(args.size());
			for(picojson::array::const_iterator a = args.begin(); a != args.end(); ++a)
			{ ret += ber_size(a->i()); }
		}
	}
	else if(type == sym::int8array) { return 1 * data.a().size(); }
	else if(type == sym::int16array) { return 2 * data.a().size(); }
	else if(type == sym::int32array) { return 4 * data.a().size(); }
	else if(type == sym::ber_array) {
		for(picojson::array::const_iterator i = data.a().begin(); i != data.a().end(); ++i)
		{ ret += ber_size(i->i()); }
	}
	else {
		picojson const& actual_schema = get_schema(type);
		assert(not actual_schema.is<picojson::null>());

		picojson::string const& actual_type = actual_schema[sym::type].s();

		if(actual_type == sym::array1d) {
			return calculate_array1d_size(data, actual_schema);
		} else if(actual_type == sym::array2d) {
			return calculate_array2d_size(data, actual_schema);
		} else { assert(false); }
	}

	assert(ret != 0);
	return ret;
}

bool LCF::save_lcf(picojson const& data, std::ostream& os) {
	picojson const& schema = get_schema(data[sym::signature].s());

	assert(not schema.is<picojson::null>());
	assert(schema[sym::signature].s() == data[sym::signature].s());

	write_string(os, data[sym::signature].s());

	picojson::array const& root = data[sym::root].a();
	for(size_t i = 0; i < root.size(); ++i) {
		picojson const& sch = schema[sym::root][i];
		picojson::string const& type = sch[sym::type].s();
		assert(type == sym::array1d or
			   type == sym::array2d or
			   type == sym::map_tree);
		save_element(root[i], sch, os);
	}

	return true;
}

void LCF::array2d::to_json(picojson& ret) const {
	assert(schema_);

	picojson(picojson::object_type, bool()).swap(ret);

	ret.o().reserve(size());
	for(array2d::const_iterator i = begin(); i != end(); ++i) {
		picojson& target = ret[i->first];
		if(target.is<picojson::null>()) { i->second.to_json(target); }
		else {
			picojson& rest = target[sym::_rest];
			if(rest.is<picojson::null>()) {
				picojson(picojson::array_type, bool()).swap(target[sym::_rest]);
			}
			rest.a().push_back(picojson());
			i->second.to_json(rest.a().back());
		}
	}
}

void LCF::array1d::to_json(picojson& ret) const {
	assert(schema_);

	picojson(picojson::object_type, bool()).swap(ret);

	picojson::object& obj = ret.o();
	obj.reserve(size());

	for(array1d::const_iterator i = begin(); i != end(); ++i) {
		picojson const& sch = find_schema(*schema_, i->first);
		picojson::string const key =
				sch.is<picojson::null>()
				? picojson::string(lexical_cast<std::string>(i->first))
				: sch[sym::name].s();

		// skip special element
		if(key.get()[0] == '_') { continue; }

		i->second.to_json(obj.insert(std::make_pair(key, picojson())).first->second);
	}

	picojson::array const& ary = (*schema_)[sym::value].a();
	for(picojson::array::const_iterator i = ary.begin(); i != ary.end(); ++i) {
		if(not has_default(*i)) { continue; }

		picojson::string const n = (*i)[sym::name].s();
		picojson::object::const_iterator const it = obj.find(n);
		if(it == obj.end()) { element((*i), istream_ref(), 0).to_json(obj[n]); }
	}
}

void LCF::element::to_json(picojson& ret) const {
	assert(schema_);

	// output binary data
	if(schema_->is<picojson::null>()) {
		picojson(picojson::array_type, bool()).swap(ret);
		stream_->seekg(base_);
		picojson::array& ary = ret.a();
		ary.reserve(size_);
		for(size_t i = 0; i < size_; ++i) {
			ary.push_back(picojson(stream_->get()));
		}
		return;
	}

	picojson::string const type = (*schema_)[sym::type].s();

	if(type == sym::integer) { ret = i(); }
	else if(type == sym::string) { ret = s(); }
	else if(type == sym::bool_) { ret = b(); }
	else if(type == sym::array1d) { a1d().to_json(ret); }
	else if(type == sym::array2d) { a2d().to_json(ret); }
	else if(type == sym::event) {
		picojson(picojson::array_type, bool()).swap(ret);
		event const ev = e();
		ret.a().reserve(ev.size());

		for(event::const_iterator i = ev.begin(); i != ev.end(); ++i) {
			picojson e(picojson::object_type, bool());

			e[sym::code] = int(i->code);
			e[sym::nest] = int(i->nest);
			e[sym::string] = i->str;

			picojson args(picojson::array_type, bool());
			args.a().reserve(i->args.size());
			for(vector<int32_t>::const_iterator arg = i->args.begin(); arg != i->args.end(); ++arg) {
				args.a().push_back(picojson(int(*arg)));
			}
			args.swap(e[sym::args]);

			ret.a().push_back(picojson());
			e.swap(ret.a().back());
		}
	}
	else if(type == sym::int8array) {
		vector<uint8_t> ary;
		i8a().to_vector(ary);

		picojson(picojson::array_type, bool()).swap(ret);
		ret.a().reserve(ary.size());
		for(size_t i = 0;i < ary.size(); ++i) {
			ret.a().push_back(picojson(int(ary[i])));
		}
	}
	else if(type == sym::int16array) {
		vector<int16_t> ary;
		i16a().to_vector(ary);

		picojson(picojson::array_type, bool()).swap(ret);
		ret.a().reserve(ary.size());
		for(size_t i = 0;i < ary.size(); ++i) {
			ret.a().push_back(picojson(int(ary[i])));
		}
	}
	else if(type == sym::ber_array) {
		ber_array const ary = ba();
		picojson(picojson::array_type, bool()).swap(ret);
		ret.a().reserve(ary.size());
		for(size_t i = 0; i < ary.size(); ++i) {
			ret.a().push_back(picojson(ary[i]));
		}
	}
	else if(type == sym::map_tree) {
		picojson(picojson::object_type, bool()).swap(ret);
		picojson nodes(picojson::array_type, bool());
		map_tree const t = mt();

		nodes.a().reserve(t.size());
		for(map_tree::const_iterator i = t.begin(); i != t.end(); ++i)
		{ nodes.a().push_back(picojson(*i)); }

		ret["nodes"].swap(nodes);
		ret["active_node"] = t.active_node;

	}
	else if(type == sym::float_) { ret = d(); }
	else if(type == sym::int32array) {
		vector<int32_t> ary;
		i32a().to_vector(ary);

		picojson(picojson::array_type, bool()).swap(ret);
		ret.a().reserve(ary.size());
		for(size_t i = 0;i < ary.size(); ++i) {
			ret.a().push_back(picojson(int(ary[i])));
		}
	}
	else {
		picojson const& sch = get_schema(type);

		picojson::string const& type_name = sch[sym::type].s();

		if(type_name == sym::array1d) {
			a1d().to_json(ret);
		} else if(type_name == sym::array2d) {
			a2d().to_json(ret);
		} else {
			assert(false);
			picojson().swap(ret);
		}
	}
}

void LCF::lcf_file::to_json(picojson& ret) const {
	picojson(picojson::object_type, bool()).swap(ret);

	// signature
	ret[sym::signature] = signature_;

	// root objects
	picojson ary = picojson(picojson::array_type, bool());
	for(vector<element>::const_iterator i = elem_.begin(); i != elem_.end(); ++i) {
		ary.a().push_back(picojson());
		i->to_json(ary.a().back());
	}
	ret[sym::root].swap(ary);
}
