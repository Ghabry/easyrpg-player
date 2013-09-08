#include "input.h"
#include "binding.hxx"

#include <mruby/variable.h>

namespace {

using namespace EasyRPG;

mrb_int from_sym(mrb_state* M, mrb_value const self) {
	mrb_value v;
	mrb_get_args(M, "o", &v);
	if(mrb_symbol_p(v)) {
		mrb_value const ret = mrb_cv_get(M, self, mrb_symbol(v));
		assert(mrb_fixnum_p(ret));
		return mrb_fixnum(ret);
	} else {
		mrb_int ret;
		mrb_get_args(M, "i", &ret);
		return ret;
	}
}

mrb_value update(mrb_state* M, mrb_value const self) {
	return Input(M).Update(), self;
}

mrb_value triggered(mrb_state* M, mrb_value const self) {
	return mrb_bool_value(Input(M).IsTriggered(Input_::Button(from_sym(M, self))));
}

mrb_value repeated(mrb_state* M, mrb_value const self) {
	return mrb_bool_value(Input(M).IsRepeated(Input_::Button(from_sym(M, self))));
}

mrb_value pressed(mrb_state* M, mrb_value const self) {
	return mrb_bool_value(Input(M).IsPressed(Input_::Button(from_sym(M, self))));
}

mrb_value dir4(mrb_state* M, mrb_value) {
	return mrb_fixnum_value(Input(M).dir4);
}
mrb_value dir8(mrb_state* M, mrb_value) {
	return mrb_fixnum_value(Input(M).dir8);
}

mrb_value reset_keys(mrb_state* M, mrb_value const self) {
	return Input(M).ResetKeys(), self;
}

}

void EasyRPG::register_input(mrb_state* M) {
	static method_info const methods[] = {
		{ "update", &update, MRB_ARGS_NONE() },
		{ "press?", &pressed, MRB_ARGS_REQ(1) },
		{ "trigger?", &triggered, MRB_ARGS_REQ(1) },
		{ "repeat?", &repeated, MRB_ARGS_REQ(1) },
		{ "dir4", &dir4, MRB_ARGS_NONE() },
		{ "dir8", &dir8, MRB_ARGS_NONE() },
		{ "reset_keys", &reset_keys, MRB_ARGS_NONE() },
		method_info_end };
	define_module(M, "Input", methods);
}
