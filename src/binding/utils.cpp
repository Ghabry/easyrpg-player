#include "utils.h"
#include "binding.hxx"
#include "output.h"

#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>

#include <vector>

#include <boost/regex/pending/unicode_iterator.hpp>

#ifdef _WIN32
#  include <windows.h>
#endif

namespace {

using namespace EasyRPG;

mrb_value to_utf8(mrb_state* M, mrb_value self) {
	mrb_value const* const ptr = RARRAY_PTR(self);
	size_t const len = RARRAY_LEN(self);

	std::vector<uint32_t> utf32(len);
	for(size_t i = 0; i < len; ++i) {
		assert(mrb_fixnum_p(ptr[i]));
		utf32[i] = mrb_fixnum(ptr[i]);
	}

	typedef boost::u32_to_u8_iterator<std::vector<uint32_t>::const_iterator> iterator;
	std::string const ret(iterator(utf32.begin()), iterator(utf32.end()));
	return mrb_str_new(M, ret.data(), ret.size());
}

mrb_value to_utf32(mrb_state* M, mrb_value self) {
	char const* const str = RSTRING_PTR(self);
	size_t const len = RSTRING_LEN(self);
	typedef boost::u8_to_u32_iterator<char const*> iterator;
	iterator const end(str + len, str, str + len);
	iterator const begin(str, str, str + len);

	mrb_value const ret = mrb_ary_new_capa(M, std::distance(end, begin));
	for(iterator i = begin; i != end; ++i) { mrb_ary_push(M, ret, mrb_fixnum_value(*i)); }
	return ret;
}

mrb_value to_nfc(mrb_state* M, mrb_value const self) {
	std::string const str = Utils::ToNfcString(RSTRING_PTR(self));
	return mrb_str_new(M, str.data(), str.size());
}

mrb_value home_path(mrb_state* M, mrb_value) {
	return
			std::getenv("HOME")? mrb_str_new_cstr(M, std::getenv("HOME")):
			std::getenv("HOMEDRIVE") and std::getenv("HOMEPATH")
			? mrb_str_cat_cstr(M, mrb_str_new_cstr(M, std::getenv("HOMEDRIVE")),
							   std::getenv("HOMEPATH"))
			: mrb_nil_value();
}

mrb_value drives(mrb_state* M, mrb_value) {
#ifdef _WIN32
	wchar_t drives[MAX_PATH + 1] = {0};
	if(GetLogicalDriveStringsW(MAX_PATH, drives) == 0) {
		Output().Warning("cannot get drives");
		return mrb_hash_new(M);
	}

	mrb_value ret = mrb_ary_new(M);

	wchar_t const* ptr = drives;
	while(*ptr != 0) {
		wchar_t const* const d = ptr;
		size_t const size = std::wcslen(ptr);
		ptr += size;

		wchar_t buf[MAX_PATH + 1] = {0};
		GetVolumeInformationW(d, buf, MAX_PATH, NULL, NULL, NULL, NULL, 0);

		std::string const drive = Utils::FromWideString(d);
		std::string const vol = Utils::FromWideString(buf);

		mrb_hash_set(M, ret, mrb_symbol_value(mrb_intern_cstr(M, drive.c_str())),
					 mrb_str_new_cstr(M, vol.c_str()));
	}

	return ret;
#else
	return mrb_hash_new(M);
#endif
}

mrb_value file_ext(mrb_state* M, mrb_value) {
	char* str; int str_len;
	mrb_get_args(M, "s", &str, &str_len);
	std::string const ret = Utils::GetExt(std::string(str, str_len));
	return mrb_str_new(M, ret.data(), ret.size());
}

mrb_value is_big_endian(mrb_state*, mrb_value) {
	return mrb_bool_value(Utils::IsBigEndian());
}

}

void EasyRPG::register_utils(mrb_state* M) {
	static method_info const methods[] = {
		{ "big_endian?", &is_big_endian, MRB_ARGS_NONE() },
		{ "home_path", &home_path, MRB_ARGS_NONE() },
		{ "drives", &drives, MRB_ARGS_NONE() },
		{ "file_ext", &file_ext, MRB_ARGS_REQ(1) },
		method_info_end };
	define_module(M, "Utils", methods);

	RClass* const String = mrb_class_get(M, "String");
	mrb_define_method(M, String, "to_utf32", &to_utf32, ARGS_NONE());
	mrb_define_method(M, String, "to_nfc", &to_nfc, ARGS_NONE());

	mrb_define_method(M, mrb_class_get(M, "Array"), "to_utf8", &to_utf8, ARGS_NONE());
}
