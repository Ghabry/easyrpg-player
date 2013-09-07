#include <algorithm>
#include <cassert>
#include <ciso646>
#include "shinonome.hxx"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/data.h>

bool operator<(ShinonomeGlyph const& g, mrb_int const code) {
	return g.code < code;
}

namespace {

void glyph_free(mrb_state*, void*) {}
mrb_data_type const TYPE = { "Glyph", &glyph_free };

mrb_value create_glyph(mrb_state* M, ShinonomeGlyph const& g) {
	RClass* const cls = mrb_class_get_under(M, mrb_class_get(M, "Shinonome"), "Glyph");
	return mrb_obj_value(mrb_data_object_alloc(
		M, cls, const_cast<ShinonomeGlyph*>(&g), &TYPE));
}

ShinonomeGlyph const EMPTY_GLYPH = { 0, true, {0} };
mrb_value get_empty_glyph(mrb_state* M, mrb_value) {
	return create_glyph(M, EMPTY_GLYPH);
}

ShinonomeGlyph const& to_cxx_glyph(mrb_state* M, mrb_value self) {
	void* const ret = mrb_data_get_ptr(M, self, &TYPE);
	assert(ret);
	return *reinterpret_cast<ShinonomeGlyph*>(ret);
}

mrb_value to_mrb_array(mrb_state* M, ShinonomeGlyph const& g) {
	mrb_value const ret = mrb_ary_new_capa(M, 12);
	for(size_t i = 0; i < 12; ++i) {
		mrb_ary_push(M, ret, mrb_fixnum_value(g.data[i]));
	}
	return ret;
}

static size_t const GOTHIC_COUNT = sizeof(SHINONOME_GOTHIC) / sizeof(ShinonomeGlyph);
static size_t const MINCHO_COUNT = sizeof(SHINONOME_MINCHO) / sizeof(ShinonomeGlyph);

mrb_value gothic(mrb_state* M, mrb_value self) {
	mrb_int code;
	mrb_get_args(M, "i", &code);

	ShinonomeGlyph const* const it =
			std::lower_bound(SHINONOME_GOTHIC, SHINONOME_GOTHIC + GOTHIC_COUNT, code);
	return (it == (SHINONOME_GOTHIC + GOTHIC_COUNT) or it->code != code)
			? mrb_nil_value() : create_glyph(M, *it);
}

mrb_value mincho(mrb_state* M, mrb_value self) {
	mrb_int code;
	mrb_get_args(M, "i", &code);

	ShinonomeGlyph const* const mincho_it =
			std::lower_bound(SHINONOME_MINCHO, SHINONOME_MINCHO + MINCHO_COUNT, code);
	if(mincho_it != (SHINONOME_MINCHO + MINCHO_COUNT) and mincho_it->code == code) {
		return create_glyph(M, *mincho_it);
	}

	// fallback to gothic glyph
	ShinonomeGlyph const* const it =
			  std::lower_bound(SHINONOME_GOTHIC, SHINONOME_GOTHIC + GOTHIC_COUNT, code);
	return (it == (SHINONOME_GOTHIC + GOTHIC_COUNT) or it->code != code)
			? mrb_nil_value() : create_glyph(M, *it);
}

mrb_value glyph_code(mrb_state* M, mrb_value self) {
	return mrb_fixnum_value(to_cxx_glyph(M, self).code);
}
mrb_value glyph_is_full(mrb_state* M, mrb_value self) {
	return mrb_bool_value(to_cxx_glyph(M, self).is_full);
}
mrb_value glyph_data(mrb_state* M, mrb_value self) {
	return to_mrb_array(M, to_cxx_glyph(M, self));
}

}

extern "C"
void mrb_shinonome_gem_init(mrb_state* M) {
	RClass* const mod = mrb_define_module(M, "Shinonome");

	mrb_define_module_function(M, mod, "gothic", &gothic, MRB_ARGS_REQ(1));
	mrb_define_module_function(M, mod, "mincho", &mincho, MRB_ARGS_REQ(1));
	mrb_define_module_function(M, mod, "empty_glyph", &get_empty_glyph, MRB_ARGS_REQ(1));

	RClass* const glyph = mrb_define_class_under(M, mod, "Glyph", M->object_class);
	mrb_define_method(M, glyph, "code", &glyph_code, MRB_ARGS_NONE());
	mrb_define_method(M, glyph, "full?", &glyph_is_full, MRB_ARGS_NONE());
	mrb_define_method(M, glyph, "data", &glyph_data, MRB_ARGS_NONE());
	mrb_undef_class_method(M, glyph, "new");
}

extern "C" void mrb_shinonome_gem_final(mrb_state*) {}
