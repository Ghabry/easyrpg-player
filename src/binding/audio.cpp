#include "audio.h"
#include "binding.hxx"

namespace {

using namespace EasyRPG;

mrb_value bgm_play(mrb_state* M, mrb_value const self) {
	char* str; int str_len;
	mrb_int volume = 100, pitch = 100;
	mrb_get_args(M, "s|ii", &str, &str_len, &volume, &pitch);
	return Audio().BGM_Play(std::string(str, str_len), volume, pitch), self;
}
mrb_value bgm_stop(mrb_state*, mrb_value const self) {
	return Audio().BGM_Stop(), self;
}
mrb_value bgm_pause(mrb_state*, mrb_value const self) {
	return Audio().BGM_Pause(), self;
}

mrb_value bgm_fade(mrb_state* M, mrb_value const self) {
	mrb_int time;
	mrb_get_args(M, "i", &time);
	return Audio().BGM_Fade(time), self;
}

mrb_value bgs_play(mrb_state* M, mrb_value const self) {
	char* str; int str_len;
	mrb_int volume = 100, pitch = 100;
	mrb_get_args(M, "s|ii", &str, &str_len, &volume, &pitch);
	return Audio().BGS_Play(std::string(str, str_len), volume, pitch), self;
}
mrb_value bgs_stop(mrb_state*, mrb_value const self) {
	return Audio().BGS_Stop(), self;
}
mrb_value bgs_fade(mrb_state* M, mrb_value const self) {
	mrb_int time;
	mrb_get_args(M, "i", &time);
	return Audio().BGS_Fade(time), self;
}

mrb_value me_play(mrb_state* M, mrb_value const self) {
	char* str; int str_len;
	mrb_int volume = 100, pitch = 100;
	mrb_get_args(M, "s|ii", &str, &str_len, &volume, &pitch);
	return Audio().ME_Play(std::string(str, str_len), volume, pitch), self;
}
mrb_value me_stop(mrb_state*, mrb_value const self) {
	return Audio().ME_Stop(), self;
}
mrb_value me_fade(mrb_state* M, mrb_value const self) {
	mrb_int time;
	mrb_get_args(M, "i", &time);
	return Audio().ME_Fade(time), self;
}

mrb_value se_play(mrb_state* M, mrb_value const self) {
	char* str; int str_len;
	mrb_int volume = 100, pitch = 100;
	mrb_get_args(M, "s|ii", &str, &str_len, &volume, &pitch);
	return Audio().SE_Play(std::string(str, str_len), volume, pitch), self;
}
mrb_value se_stop(mrb_state*, mrb_value const self) {
	return Audio().SE_Stop(), self;
}

mrb_value update(mrb_state*, mrb_value const self) {
	return Audio().Update(), self;
}

}

void EasyRPG::register_audio(mrb_state* M) {
	static method_info const methods[] = {
		{ "bgm_play", &bgm_play, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2) },
		{ "bgm_stop", &bgm_stop, MRB_ARGS_NONE() },
		{ "bgm_pause", &bgm_pause, MRB_ARGS_NONE() },
		{ "bgm_fade", &bgm_fade, MRB_ARGS_REQ(1) },
		{ "bgs_play", &bgs_play, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2) },
		{ "bgs_stop", &bgs_stop, MRB_ARGS_NONE() },
		{ "bgs_fade", &bgs_fade, MRB_ARGS_REQ(1) },
		{ "me_play", &me_play, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2) },
		{ "me_stop", &me_stop, MRB_ARGS_NONE() },
		{ "me_fade", &me_fade, MRB_ARGS_REQ(1) },
		{ "se_play", &se_play, MRB_ARGS_REQ(1) | MRB_ARGS_OPT(2) },
		{ "se_stop", &se_stop, MRB_ARGS_NONE() },
		{ "update", &update, MRB_ARGS_NONE() },
		method_info_end };
	define_module(M, "Audio", methods);
}
