/*
 * Copyright 2009-2010 Cybozu Labs, Inc.
 * Copyright 2011 Kazuho Oku
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY CYBOZU LABS, INC. ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL CYBOZU LABS, INC. OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Cybozu Labs, Inc.
 *
 */
#ifndef picojson_h
#define picojson_h

#include "picojson_fwd.hxx"

#include <cassert>
#include <ciso646>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <string>
#include <algorithm>
#include <utility>
#include <limits>

#include <boost/container/flat_map.hpp>
#include <boost/container/vector.hpp>
#include <boost/flyweight/flyweight.hpp>
#include <boost/flyweight/static_holder.hpp>
#include <boost/flyweight/refcounted.hpp>
#include <boost/flyweight/hashed_factory.hpp>
#include <boost/lexical_cast.hpp>

#ifdef _MSC_VER
#define SNPRINTF _snprintf_s
#pragma warning(push)
#pragma warning(disable : 4244) // conversion from int to char
#else
#define SNPRINTF snprintf
#endif


class picojson {
public:
    enum {
		null_type,
		boolean_type,
		string_type,
		array_type,
		object_type,
		int_type,
		float_type,
    };

    struct null {};
	typedef picojson_object_key object_key;
	typedef object_key string;
    typedef picojson_array array;
    typedef picojson_object object;

    typedef picojson value;
private:
    int type_;
    union {
		bool boolean_;
		double float_;
		int int_;
		string* string_;
		array* array_;
		object* object_;
    } u_;
public:
    picojson();
    picojson(int type, bool);
    explicit picojson(bool b);
    explicit picojson(double n);
    explicit picojson(int n);
    explicit picojson(const std::string& s);
    explicit picojson(const object_key& s);
    explicit picojson(const array& a);
    explicit picojson(const object& o);
    explicit picojson(const char* s);
    picojson(const char* s, size_t len);
    ~picojson();
    picojson(const picojson& x);
	picojson& operator=(picojson const& x);
    picojson& operator=(const std::string& x);
	template<class T> picojson& operator=(const T& x);
    template <typename T> bool is() const;
    template <typename T> const T& get() const;
    template <typename T> T& get();
    bool evaluate_as_boolean() const;
    const picojson& get(size_t idx) const;
    const picojson& get(const std::string& key) const;
    const picojson& get(const object_key& key) const;
    picojson& get(size_t idx);
    picojson& get(const std::string& key);
    picojson& get(const object_key& key);
    bool contains(size_t idx) const;
    bool contains(const std::string& key) const;
    bool contains(const object_key& key) const;
    std::string to_str() const;
    template <typename Iter> void serialize(Iter os) const;
    std::string serialize() const;

	template<class T> picojson const& operator[](T const& v) const;
	template<class T> picojson& operator[](T const& v);

	void swap(picojson& x);

	bool is_numeric() const;
	double numeric() const;

	int const& i() const;
	bool const& b() const;
	double const& d() const;
	string const& s() const;
	array const& a() const;
	object const& o() const;

	int type() const { return type_; }

	int& i();
	bool& b();
	double& d();
	string& s();
	array& a();
	object& o();

	// obsolete, use the version below
	template <typename Iter> static std::string parse(picojson& out, Iter& pos, const Iter& last);

	template <typename Iter> static Iter parse(picojson& out, const Iter& first, const Iter& last, std::string* err);
	static std::string parse(picojson& out, std::istream& is);
	template <typename Iter> class input;

	class deny_parse_context;
	class default_parse_context;
	class null_parse_context;
	template <typename T> struct last_error_t;

	static void set_last_error(const std::string& s);
	static const std::string& get_last_error();

	template <typename Context, typename Iter> static bool _parse(Context& ctx, input<Iter>& in);
	template <typename Context, typename Iter> static Iter _parse(Context& ctx, const Iter& first, const Iter& last, std::string* err);

private:
	template<typename Iter> static int _parse_quadhex(input<Iter> &in);
	template<typename String, typename Iter> static bool _parse_codepoint(String& out, input<Iter>& in);
	template<typename String, typename Iter> static bool _parse_string(String& out, input<Iter>& in);
	template <typename Context, typename Iter> static bool _parse_array(Context& ctx, input<Iter>& in);
	template <typename Context, typename Iter> static bool _parse_object(Context& ctx, input<Iter>& in);
	template <typename Iter> static bool _parse_number(double& out, input<Iter>& in, bool& is_float);
	template <typename Iter> static void copy(const std::string& s, Iter oi);
	template <typename Iter> static void serialize_str(const std::string& s, Iter oi);

    template <typename T> picojson(const T*); // intentionally defined to block implicit conversion of pointer to bool
};

bool operator==(const picojson& x, const picojson& y);

inline picojson::picojson() : type_(null_type) {}

inline picojson::picojson(int type, bool) : type_(type) {
    switch (type) {
#define INIT(p, v) case p##type: u_.p = v; break
		INIT(boolean_, false);
		INIT(float_, 0.0);
		INIT(int_, 0);
		INIT(string_, new string());
		INIT(array_, new array());
		INIT(object_, new object());
#undef INIT
    default: break;
    }
}

inline picojson::picojson(bool b) : type_(boolean_type) {
    u_.boolean_ = b;
}

inline picojson::picojson(double n) : type_(float_type) {
    u_.float_ = n;
}
inline picojson::picojson(int n) : type_(int_type) {
    u_.int_ = n;
}

inline picojson::picojson(const std::string& s) : type_(string_type) {
    u_.string_ = new string(s);
}

inline picojson::picojson(const string& s) : type_(string_type) {
    u_.string_ = new string(s);
}

inline picojson::picojson(const array& a) : type_(array_type) {
    u_.array_ = new array(a);
}

inline picojson::picojson(const object& o) : type_(object_type) {
    u_.object_ = new object(o);
}

inline picojson::picojson(const char* s) : type_(string_type) {
    u_.string_ = new string(s);
}

inline picojson::picojson(const char* s, size_t len) : type_(string_type) {
    u_.string_ = new string(std::string(s, len));
}

inline picojson::~picojson() {
    switch (type_) {
#define DEINIT(p) case p##type: delete u_.p; break
		DEINIT(string_);
		DEINIT(array_);
		DEINIT(object_);
#undef DEINIT
    default: break;
    }
}

inline picojson::picojson(const picojson& x) : type_(x.type_) {
    switch (type_) {
#define INIT(p, v) case p##type: u_.p = v; break
		INIT(boolean_, x.u_.boolean_);
		INIT(float_, x.u_.float_);
		INIT(int_, x.u_.int_);
		INIT(string_, new string(*x.u_.string_));
		INIT(array_, new array(*x.u_.array_));
		INIT(object_, new object(*x.u_.object_));
#undef INIT
    default: break;
    }
}

inline void picojson::swap(picojson& x) {
    if (this == &x) { return; }
	std::swap(u_, x.u_);
	std::swap(type_, x.type_);
}

inline picojson& picojson::operator=(picojson const& x) {
    if (this != &x) { picojson(x).swap(*this); }
    return *this;
}

template<class T>
inline picojson& picojson::operator=(const T& x) {
	if(is<T>()) {
		this->get<T>() = x;
	} else {
		picojson(x).swap(*this);
	}
    return *this;
}

inline picojson& picojson::operator=(std::string const& x) {
	(*this) = picojson::string(x);
	return *this;
}

#define IS(ctype, jtype)								\
	template <> inline bool picojson::is<ctype>() const {	\
		return type_ == jtype##_type;					\
	}
IS(picojson::null, null)
IS(bool, boolean)
IS(picojson::string, string)
IS(std::string, string)
IS(picojson::array, array)
IS(picojson::object, object)
IS(int, int)
IS(double, float)
#undef IS

#define GET(ctype, var)												\
	template <> inline const ctype& picojson::get<ctype>() const {	\
		assert("type mismatch! call vis<type>() before get<type>()" \
			   && is<ctype>());										\
		return var;													\
	}																\
	template <> inline ctype& picojson::get<ctype>() {				\
		assert("type mismatch! call is<type>() before get<type>()"	\
			   && is<ctype>());										\
		return var;													\
	}
GET(bool, u_.boolean_)
GET(int, u_.int_)
GET(double, u_.float_)
GET(picojson::string, *u_.string_)
GET(picojson::array, *u_.array_)
GET(picojson::object, *u_.object_)
#undef GET

template<> inline const std::string& picojson::get<std::string>() const {
	assert("type mismatch! call is<type>() before get<type>()"
		   && is<std::string>());
	return u_.string_->get();
}

inline bool picojson::is_numeric() const {
	return type_ == int_type || type_ == float_type;
}

inline double picojson::numeric() const {
	assert(is_numeric());
	return
		is<double>()? get<double>():
		is<int>()? get<int>():
		0;
}

inline bool picojson::evaluate_as_boolean() const {
    switch (type_) {
    case null_type:
		return false;
    case boolean_type:
		return u_.boolean_;
    case int_type:
		return u_.int_ != 0;
    case float_type:
		return u_.float_ != 0;
    case string_type:
		return ! u_.string_->get().empty();
    default:
		return true;
    }
}

inline const picojson& picojson::get(size_t idx) const {
    static value s_null;
    if(is<array>()) {
		return idx < u_.array_->size() ? (*u_.array_)[idx] : s_null;
	} else if(is<object>()) {
		return get(boost::lexical_cast<std::string>(idx));
	} else {
		assert(false);
		return s_null;
	}
}

inline const picojson& picojson::get(const std::string& key) const {
    return get(object_key(key));
}
inline const picojson& picojson::get(const object_key& key) const {
    static picojson s_null;
    assert(is<object>());
    object::const_iterator i = u_.object_->find(key);
    return i != u_.object_->end() ? i->second : s_null;
}

template<class T>
inline picojson const& picojson::operator[](T const& v) const { return get(v); }
template<class T>
inline picojson& picojson::operator[](T const& v) { return get(v); }


inline int const& picojson::i() const { return get<int>(); }
inline bool const& picojson::b() const { return get<bool>(); }
inline double const& picojson::d() const { return get<double>(); }
inline picojson::string const& picojson::s() const { return get<string>(); }
inline picojson::array const& picojson::a() const { return get<array>(); }
inline picojson::object const& picojson::o() const { return get<object>(); }

inline int& picojson::i() { return get<int>(); }
inline bool& picojson::b() { return get<bool>(); }
inline double& picojson::d() { return get<double>(); }
inline picojson::string& picojson::s() { return get<string>(); }
inline picojson::array& picojson::a() { return get<array>(); }
inline picojson::object& picojson::o() { return get<object>(); }

inline picojson& picojson::get(size_t idx) {
    if(is<array>()) {
		if(idx >= u_.array_->size()) {
			u_.array_->resize(idx + 1);
		}
		return (*u_.array_)[idx];
	} else if(is<object>()) {
		return get(boost::lexical_cast<std::string>(idx));
	} else {
		static picojson s_null;
		assert(false);
		return s_null;
	}
}

inline picojson& picojson::get(const std::string& key) {
    return get(object_key(key));
}
inline picojson& picojson::get(const object_key& key) {
    assert(is<object>());
    return (*u_.object_)[key];
}

inline bool picojson::contains(size_t idx) const {
    if(is<array>()) {
		return idx < u_.array_->size();
	} else if(is<object>()) {
		return contains(boost::lexical_cast<std::string>(idx));
	} else {
		assert(false);
		return false;
	}
}

inline bool picojson::contains(const std::string& key) const {
    return contains(object_key(key));
}
inline bool picojson::contains(const object_key& key) const {
    assert(is<object>());
    object::const_iterator i = u_.object_->find(key);
    return i != u_.object_->end();
}

inline std::string picojson::to_str() const {
    switch (type_) {
    case null_type:      return "null";
    case boolean_type:   return u_.boolean_ ? "true" : "false";
    case float_type:    {
		char buf[256];
		double tmp;
		SNPRINTF(buf, sizeof(buf), fabs(u_.float_) < (1ULL << 53) && modf(u_.float_, &tmp) == 0 ? "%.f" : "%.17g", u_.float_);
		return buf;
    }
    case int_type:    {
		char buf[256];
		SNPRINTF(buf, sizeof(buf), "%d", u_.int_);
		return buf;
    }
    case string_type:    return *u_.string_;
    case array_type:     return "array";
    case object_type:    return "object";
    default:             assert(0);
#ifdef _MSC_VER
		__assume(0);
#endif
		return "";
    }
}

template <typename Iter> void picojson::serialize(Iter oi) const {
    switch (type_) {
    case string_type:
		serialize_str(u_.string_->get(), oi);
		break;
    case array_type: {
		*oi++ = '[';
		for (array::const_iterator i = u_.array_->begin(); i != u_.array_->end(); ++i) {
			if (i != u_.array_->begin()) {
				*oi++ = ',';
			}
			i->serialize(oi);
		}
		*oi++ = ']';
		break;
    }
    case object_type: {
		*oi++ = '{';
		for (object::const_iterator i = u_.object_->begin();
			 i != u_.object_->end();
			 ++i) {
			if (i != u_.object_->begin()) {
				*oi++ = ',';
			}
			serialize_str(i->first.get(), oi);
			*oi++ = ':';
			i->second.serialize(oi);
		}
		*oi++ = '}';
		break;
    }
    default:
		copy(to_str(), oi);
		break;
    }
}

inline std::string picojson::serialize() const {
    std::string s;
    serialize(std::back_inserter(s));
    return s;
}

template <typename Iter> class picojson::input {
protected:
    Iter cur_, end_;
    int last_ch_;
    bool ungot_;
    int line_;
public:
    input(const Iter& first, const Iter& last) : cur_(first), end_(last), last_ch_(-1), ungot_(false), line_(1) {}
    int getc() {
		if (ungot_) {
			ungot_ = false;
			return last_ch_;
		}
		if (cur_ == end_) {
			last_ch_ = -1;
			return -1;
		}
		if (last_ch_ == '\n') {
			line_++;
		}
		last_ch_ = *cur_++ & 0xff;
		return last_ch_;
    }
    void ungetc() {
		if (last_ch_ != -1) {
			assert(! ungot_);
			ungot_ = true;
		}
    }
    Iter cur() const { return cur_; }
    int line() const { return line_; }
    void skip_ws() {
		while (true) {
			int const ch = getc();

			if(ch == '/') { // comment
				switch(getc()) {
				case '/': // single line comment
					while(getc() != '\n') {}
					continue;
				case '*': // multi line comment
					while(getc() != '*' && getc() != '/') {}
					continue;
				default:
					ungetc(); // char next to slash
					break;
				}
			}

			if (! (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')) {
				ungetc();
				return;
			}
		}
    }
    int expect(int expect) {
		skip_ws();
		if (getc() != expect) {
			ungetc();
			return false;
		}
		return true;
    }
    bool match(const std::string& pattern) {
		for (std::string::const_iterator pi(pattern.begin());
			 pi != pattern.end();
			 ++pi) {
			if (getc() != *pi) {
				ungetc();
				return false;
			}
		}
		return true;
    }
};

class picojson::default_parse_context {
protected:
	value* out_;
public:
	default_parse_context(value* out) : out_(out) {}
	bool set_null() {
		*out_ = value();
		return true;
	}
	bool set_bool(bool b) {
		*out_ = value(b);
		return true;
	}
	bool set_float(double f) {
		*out_ = value(f);
		return true;
	}
	bool set_int(int f) {
		*out_ = value(f);
		return true;
	}
	template<typename Iter> bool parse_string(input<Iter>& in) {
		std::string str;
		bool const ret = _parse_string(str, in);
		*out_ = string(str);
		return ret;
	}
	bool parse_array_start() {
		*out_ = value(array_type, false);
		return true;
	}
	template <typename Iter> bool parse_array_item(input<Iter>& in, size_t) {
		array& a = out_->a();
		a.push_back(value());
		default_parse_context ctx(&a.back());
		return _parse(ctx, in);
	}
	bool parse_object_start() {
		*out_ = value(object_type, false);
		return true;
	}
	template <typename Iter> bool parse_object_item(input<Iter>& in, const std::string& key) {
		return parse_object_item<Iter>(in, object_key(key));
	}
	template <typename Iter> bool parse_object_item(input<Iter>& in, const object_key& key) {
		object& o = out_->o();
		default_parse_context ctx(&(o[key]));
		return _parse(ctx, in);
	}
private:
	default_parse_context(const default_parse_context&);
	default_parse_context& operator=(const default_parse_context&);
};

// obsolete, use the version below
template <typename Iter> inline std::string picojson::parse(picojson& out, Iter& pos, const Iter& last) {
	std::string err;
	pos = parse(out, pos, last, &err);
	return err;
}

template <typename Iter> inline Iter picojson::parse(picojson& out, const Iter& first, const Iter& last, std::string* err) {
	default_parse_context ctx(&out);
	return _parse(ctx, first, last, err);
}

inline std::string picojson::parse(picojson& out, std::istream& is) {
	std::string err;
	parse(out, std::istreambuf_iterator<char>(is.rdbuf()),
		  std::istreambuf_iterator<char>(), &err);
	return err;
}

template<typename Iter> inline int picojson::_parse_quadhex(input<Iter> &in) {
	int uni_ch = 0, hex;
	for (int i = 0; i < 4; i++) {
		if ((hex = in.getc()) == -1) {
			return -1;
		}
		if ('0' <= hex && hex <= '9') {
			hex -= '0';
		} else if ('A' <= hex && hex <= 'F') {
			hex -= 'A' - 0xa;
		} else if ('a' <= hex && hex <= 'f') {
			hex -= 'a' - 0xa;
		} else {
			in.ungetc();
			return -1;
		}
		uni_ch = uni_ch * 16 + hex;
	}
	return uni_ch;
}

template<typename String, typename Iter> inline bool picojson::_parse_codepoint(String& out, input<Iter>& in) {
	int uni_ch;
	if ((uni_ch = _parse_quadhex(in)) == -1) {
		return false;
	}
	if (0xd800 <= uni_ch && uni_ch <= 0xdfff) {
		if (0xdc00 <= uni_ch) {
			// a second 16-bit of a surrogate pair appeared
			return false;
		}
		// first 16-bit of surrogate pair, get the next one
		if (in.getc() != '\\' || in.getc() != 'u') {
			in.ungetc();
			return false;
		}
		int second = _parse_quadhex(in);
		if (! (0xdc00 <= second && second <= 0xdfff)) {
			return false;
		}
		uni_ch = ((uni_ch - 0xd800) << 10) | ((second - 0xdc00) & 0x3ff);
		uni_ch += 0x10000;
	}
	if (uni_ch < 0x80) {
		out.push_back(uni_ch);
	} else {
		if (uni_ch < 0x800) {
			out.push_back(0xc0 | (uni_ch >> 6));
		} else {
			if (uni_ch < 0x10000) {
				out.push_back(0xe0 | (uni_ch >> 12));
			} else {
				out.push_back(0xf0 | (uni_ch >> 18));
				out.push_back(0x80 | ((uni_ch >> 12) & 0x3f));
			}
			out.push_back(0x80 | ((uni_ch >> 6) & 0x3f));
		}
		out.push_back(0x80 | (uni_ch & 0x3f));
	}
	return true;
}

template<typename String, typename Iter> inline bool picojson::_parse_string(String& out, input<Iter>& in) {
	while (1) {
		int ch = in.getc();
		if (ch < ' ') {
			in.ungetc();
			return false;
		} else if (ch == '"') {
			return true;
		} else if (ch == '\\') {
			if ((ch = in.getc()) == -1) {
				return false;
			}
			switch (ch) {
#define MAP(sym, val) case sym: out.push_back(val); break
				MAP('"', '\"');
				MAP('\\', '\\');
				MAP('/', '/');
				MAP('b', '\b');
				MAP('f', '\f');
				MAP('n', '\n');
				MAP('r', '\r');
				MAP('t', '\t');
#undef MAP
			case 'u':
				if (! _parse_codepoint(out, in)) {
					return false;
				}
				break;
			default:
				return false;
			}
		} else {
			out.push_back(ch);
		}
	}
	return false;
}

template <typename Context, typename Iter> inline bool picojson::_parse_array(Context& ctx, input<Iter>& in) {
	if (! ctx.parse_array_start()) {
		return false;
	}
	if (in.expect(']')) {
		return true;
	}
	size_t idx = 0;
	do {
		if (! ctx.parse_array_item(in, idx)) {
			return false;
		}
		idx++;
	} while (in.expect(','));
	return in.expect(']');
}

template <typename Context, typename Iter> inline bool picojson::_parse_object(Context& ctx, input<Iter>& in) {
	if (! ctx.parse_object_start()) {
		return false;
	}
	if (in.expect('}')) {
		return true;
	}
	do {
		std::string key;
		if (! in.expect('"')
			|| ! _parse_string(key, in)
			|| ! in.expect(':')) {
			return false;
		}
		if (! ctx.parse_object_item(in, key)) {
			return false;
		}
	} while (in.expect(','));
	return in.expect('}');
}

template <typename Iter> inline bool picojson::_parse_number(double& out, input<Iter>& in, bool& is_float) {
	std::string num_str;
	is_float = false;
	while (1) {
		int const ch = in.getc();

		if (ch == '.' || ch == 'e' || ch == 'E') { is_float = true; }

		if (('0' <= ch && ch <= '9') || ch == '+' || ch == '-' || ch == '.'
			|| ch == 'e' || ch == 'E') {
			num_str.push_back(ch);
		} else {
			in.ungetc();
			break;
		}
	}
	char* endp;
	out = strtod(num_str.c_str(), &endp);
	if(not is_float and
	   (out < std::numeric_limits<int>::min()
		or std::numeric_limits<int>::max() < out))
		{ is_float = true; }
	return endp == num_str.c_str() + num_str.size();
}

template <typename Context, typename Iter> inline bool picojson::_parse(Context& ctx, input<Iter>& in) {
	in.skip_ws();
	int ch = in.getc();
	switch (ch) {
#define IS(ch, text, op) case ch:				\
		if (in.match(text) && op) {				\
			return true;						\
		} else {								\
			return false;						\
		}
		IS('n', "ull", ctx.set_null());
		IS('f', "alse", ctx.set_bool(false));
		IS('t', "rue", ctx.set_bool(true));
#undef IS
	case '"':
		return ctx.parse_string(in);
	case '[':
		return _parse_array(ctx, in);
	case '{':
		return _parse_object(ctx, in);
	default:
		if (('0' <= ch && ch <= '9') || ch == '-') {
			in.ungetc();
			double f;
			bool is_float;
			if (_parse_number(f, in, is_float)) {
				is_float? ctx.set_float(f) : ctx.set_int(static_cast<int>(f));
				return true;
			} else {
				return false;
			}
		}
		break;
	}
	in.ungetc();
	return false;
}

class picojson::deny_parse_context {
public:
	bool set_null() { return false; }
	bool set_bool(bool) { return false; }
	bool set_float(double) { return false; }
	bool set_int(double) { return false; }
	template <typename Iter> bool parse_string(input<Iter>&) { return false; }
	bool parse_array_start() { return false; }
	template <typename Iter> bool parse_array_item(input<Iter>&, size_t) {
		return false;
	}
	bool parse_object_start() { return false; }
	template <typename Iter> bool parse_object_item(input<Iter>&, const std::string&) {
		return false;
	}
};

class picojson::null_parse_context {
public:
	struct dummy_str {
		void push_back(int) {}
	};
public:
	null_parse_context() {}
	bool set_null() { return true; }
	bool set_bool(bool) { return true; }
	bool set_float(double) { return true; }
	bool set_int(double) { return true; }
	template <typename Iter> bool parse_string(input<Iter>& in) {
		dummy_str s;
		return _parse_string(s, in);
	}
	bool parse_array_start() { return true; }
	template <typename Iter> bool parse_array_item(input<Iter>& in, size_t) {
		return _parse(*this, in);
	}
	bool parse_object_start() { return true; }
	template <typename Iter> bool parse_object_item(input<Iter>& in, const std::string&) {
		return _parse(*this, in);
	}
private:
	null_parse_context(const null_parse_context&);
	null_parse_context& operator=(const null_parse_context&);
};

template <typename Context, typename Iter> inline Iter picojson::_parse(Context& ctx, const Iter& first, const Iter& last, std::string* err) {
	input<Iter> in(first, last);
	if (! _parse(ctx, in) && err != NULL) {
		char buf[64];
		SNPRINTF(buf, sizeof(buf), "syntax error at line %d near: ", in.line());
		*err = buf;
		while (1) {
			int ch = in.getc();
			if (ch == -1 || ch == '\n') {
				break;
			} else if (ch >= ' ') {
				err->push_back(ch);
			}
		}
	}
	return in.cur();
}

template <typename T> struct picojson::last_error_t {
	static std::string s;
};
template <typename T> std::string picojson::last_error_t<T>::s;

inline void picojson::set_last_error(const std::string& s) {
	last_error_t<bool>::s = s;
}

inline const std::string& picojson::get_last_error() {
	return last_error_t<bool>::s;
}

template <typename Iter> void picojson::copy(const std::string& s, Iter oi) {
	std::copy(s.begin(), s.end(), oi);
}

template <typename Iter> void picojson::serialize_str(const std::string& s, Iter oi) {
	*oi++ = '"';
	for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
		switch (*i) {
#define MAP(val, sym) case val: copy(sym, oi); break
			MAP('"', "\\\"");
			MAP('\\', "\\\\");
			MAP('/', "\\/");
			MAP('\b', "\\b");
			MAP('\f', "\\f");
			MAP('\n', "\\n");
			MAP('\r', "\\r");
			MAP('\t', "\\t");
#undef MAP
		default:
			if ((unsigned char)*i < 0x20 || *i == 0x7f) {
				char buf[7];
				SNPRINTF(buf, sizeof(buf), "\\u%04x", *i & 0xff);
				std::copy(buf, buf + 6, oi);
			} else {
				*oi++ = *i;
			}
			break;
		}
	}
	*oi++ = '"';
}

inline bool operator<(const picojson& x, const picojson& y) {
	if(x.is_numeric() && y.is_numeric()) {
		return x.numeric() < y.numeric();
	} else {
		assert(x.type() == y.type());
		if(x.is<picojson::string>() && y.is<picojson::string>()) {
			return x.s() < y.s();
		} else {
			assert(false);
			return false;
		}
	}
}

inline bool operator==(const picojson& x, const picojson& y) {
	if(x.is_numeric() && y.is_numeric())
		return x.numeric() == y.numeric();
	else if(x.type() != y.type()) { return false; }
    else if (x.is<picojson::null>())
		return y.is<picojson::null>();
#define PICOJSON_CMP(type)										\
    if (x.is<type>())											\
		return y.is<type>() && x.get<type>() == y.get<type>()
    PICOJSON_CMP(bool);
    PICOJSON_CMP(picojson::string);
    PICOJSON_CMP(picojson::array);
    PICOJSON_CMP(picojson::object);
#undef PICOJSON_CMP
    assert(false);
#ifdef _MSC_VER
    __assume(0);
#endif
    return false;
}

inline bool operator!=(const picojson& x, const picojson& y) {
    return ! (x == y);
}

inline std::istream& operator>>(std::istream& is, picojson& x)
{
	picojson::set_last_error(std::string());
	std::string err = picojson::parse(x, is);
	if (! err.empty()) {
		picojson::set_last_error(err);
		is.setstate(std::ios::failbit);
	}
	return is;
}

inline std::ostream& operator<<(std::ostream& os, const picojson& x)
{
	x.serialize(std::ostream_iterator<char>(os));
	return os;
}

inline void swap(picojson& l, picojson& r) {
	l.swap(r);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
