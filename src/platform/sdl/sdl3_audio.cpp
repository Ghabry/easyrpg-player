/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#include "system.h"

#include <SDL_audio.h>
#include <SDL_error.h>
#include <chrono>
#include <cstring>
#include <cassert>
#include <memory>
#include "sdl3_audio.h"
#include "output.h"

SDL_AudioFormat to_sdl_format(AudioDecoder::Format format) {
	switch (format) {
		case AudioDecoder::Format::S8:
			return SDL_AUDIO_S8;
		case AudioDecoder::Format::U8:
			return SDL_AUDIO_U8;
		case AudioDecoder::Format::S16:
			return SDL_AUDIO_S16;
		case AudioDecoder::Format::S32:
			return SDL_AUDIO_S32;
		case AudioDecoder::Format::F32:
			return SDL_AUDIO_F32;
		default:
			return 0;
	}
}

Sdl3Audio::Sdl3Audio(const Game_ConfigAudio& cfg) : AudioInterface(cfg) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		Output::Warning("Couldn't init audio: {}", SDL_GetError());
		return;
	}

#if defined(EMSCRIPTEN)
	// Get preferred sample rate from Browser (-> OS)
	const int frequency = EM_ASM_INT_V({
		var context;
		try {
			context = new AudioContext();
		} catch (e) {
			context = new webkitAudioContext();
		}
		return context.sampleRate;
	});
#else
	const int frequency = 44100;
#endif

	if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &device_spec, nullptr) < 0) {
		Output::Warning("SDL_GetAudioDeviceFormat failed: {}", SDL_GetError());
		return;
	}

	int i = 0;
	for (auto& BGM_Channel : BGM_Channels) {
		BGM_Channel.id = i++;
		BGM_Channel.instance = this;
	}
	i = 0;
	for (auto& SE_Channel : SE_Channels) {
		SE_Channel.id = i++;
		SE_Channel.instance = this;
	}
	BGM_PlayedOnceIndicator = false;
	midi_thread.reset();
}

void Sdl3Audio::BGM_Play(Filesystem_Stream::InputStream stream, int volume, int pitch, int fadein) {
	if (!stream) {
		Output::Warning("Couldn't play BGM {}: File not readable", stream.GetName());
		return;
	}

	for (auto& BGM_Channel : BGM_Channels) {
		if (!BGM_Channel.IsUsed()) {
			// If there is an unused bgm channel
			LockMutex();
			BGM_PlayedOnceIndicator = false;
			UnlockMutex();
			PlayOnChannel(BGM_Channel, std::move(stream), volume, pitch, fadein);
			return;
		}
	}
}

void Sdl3Audio::BGM_Pause() {
	for (auto& BGM_Channel : BGM_Channels) {
		if (BGM_Channel.IsUsed()) {
			BGM_Channel.SetPaused(true);
		}
	}
}

void Sdl3Audio::BGM_Resume() {
	for (auto& BGM_Channel : BGM_Channels) {
		if (BGM_Channel.IsUsed()) {
			BGM_Channel.SetPaused(false);
		}
	}
}

void Sdl3Audio::BGM_Stop() {
	LockMutex();
	for (auto& BGM_Channel : BGM_Channels) {
		BGM_Channel.Stop();
	}
	BGM_PlayedOnceIndicator = false;
	UnlockMutex();
}

bool Sdl3Audio::BGM_PlayedOnce() const {
	if (BGM_PlayedOnceIndicator) {
		return BGM_PlayedOnceIndicator;
	}

	LockMutex();
	// Audio Decoders set this in the Decoding thread
	for (auto& BGM_Channel : BGM_Channels) {
		if (BGM_Channel.midi_out_used) {
			BGM_PlayedOnceIndicator = midi_thread->GetMidiOut().GetLoopCount() > 0;
		}
	}
	UnlockMutex();

	return BGM_PlayedOnceIndicator;
}

bool Sdl3Audio::BGM_IsPlaying() const {
	for (auto& BGM_Channel : BGM_Channels) {
		if (BGM_Channel.IsUsed()) {
			return true;
		}
	}
	return false;
}

int Sdl3Audio::BGM_GetTicks() const {
	unsigned ticks = 0;
	LockMutex();
	for (auto& BGM_Channel : BGM_Channels) {
		int cur_ticks = BGM_Channel.GetTicks();
		if (cur_ticks >= 0) {
			ticks = static_cast<unsigned>(cur_ticks);
		}
	}
	UnlockMutex();
	return ticks;
}

void Sdl3Audio::BGM_Fade(int fade) {
	LockMutex();
	for (auto& BGM_Channel : BGM_Channels) {
		BGM_Channel.SetFade(fade);
	}
	UnlockMutex();
}

void Sdl3Audio::BGM_Volume(int volume) {
	LockMutex();
	for (auto& BGM_Channel : BGM_Channels) {
		BGM_Channel.SetVolume(volume);
	}
	UnlockMutex();
}

void Sdl3Audio::BGM_Pitch(int pitch) {
	LockMutex();
	for (auto& BGM_Channel : BGM_Channels) {
		BGM_Channel.SetPitch(pitch);
	}
	UnlockMutex();
}

std::string Sdl3Audio::BGM_GetType() const {
	std::string type;

	LockMutex();
	for (auto& BGM_Channel : BGM_Channels) {
		if (BGM_Channel.IsUsed()) {
			if (BGM_Channel.midi_out_used) {
				type = "midi";
				break;
			} else {
				type = BGM_Channel.decoder->GetType();
				break;
			}
		}
	}
	UnlockMutex();

	return type;
}

void Sdl3Audio::SE_Play(std::unique_ptr<AudioSeCache> se, int volume, int pitch) {
	if (!se) {
		Output::Warning("SE_Play: AudioSeCache data is NULL");
		return;
	}

	for (auto& SE_Channel : SE_Channels) {
		if (!SE_Channel.sdl_stream || SDL_GetAudioStreamAvailable(SE_Channel.sdl_stream.get()) == 0) {
			//If there is an unused se channel
			PlayOnChannel(SE_Channel, std::move(se), volume, pitch);
			return;
		}
	}
	// FIXME Not displaying as warning because multiple games exhaust free channels available, see #1356
	Output::Debug("Couldn't play {} SE. No free channel available", se->GetName());
}

void Sdl3Audio::SE_Stop() {
	for (auto& SE_Channel : SE_Channels) {
		if (SE_Channel.sdl_stream) {
			SE_Channel.sdl_stream.reset();
		}
	}
}

void Sdl3Audio::Update() {
	for (auto& BGM_Channel : BGM_Channels) {
		if (BGM_Channel.decoder) {
			// FIXME
			BGM_Channel.decoder->Update(std::chrono::milliseconds(16));
		}
	}
}

void sdl_bgm_audio_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, [[maybe_unused]] int total_amount) {
	if (additional_amount <= 0) {
		return;
	}

	static std::vector<uint8_t> buffer;
	if (buffer.size() < static_cast<size_t>(additional_amount)) {
		buffer.resize(additional_amount);
	}

	auto chan = static_cast<Sdl3Audio::BgmChannel*>(userdata);
	chan->decoder->Decode(buffer.data(), buffer.size());

	int volume = chan->decoder->GetVolume();
	if (volume != 100) {
		SDL_MixAudioFormat(buffer.data(), buffer.data(), chan->sdl_spec.format, buffer.size(), volume / 100.0f * SDL_MIX_MAXVOLUME);
	}

	SDL_PutAudioStreamData(stream, buffer.data(), buffer.size());
}

bool Sdl3Audio::PlayOnChannel(BgmChannel& chan, Filesystem_Stream::InputStream filestream, int volume, int pitch, int fadein) {
	if (!filestream) {
		Output::Warning("BGM file not readable: {}", filestream.GetName());
		return false;
	}

	// Midiout is only supported on channel 0 because this is an exclusive resource
	if (chan.id == 0 && GenericAudioMidiOut::IsSupported(filestream)) {
		chan.decoder.reset();

		// FIXME: Try Fluidsynth and WildMidi first
		// If they work fallback to the normal AudioDecoder handler below
		// There should be a way to configure the order
		if (!MidiDecoder::CreateFluidsynth(true) && !MidiDecoder::CreateWildMidi(true)) {
			if (!midi_thread) {
				midi_thread = std::make_unique<GenericAudioMidiOut>();
				if (midi_thread->IsInitialized()) {
					midi_thread->StartThread();
				} else {
					midi_thread.reset();
				}
			}

			if (midi_thread) {
				midi_thread->LockMutex();
				auto &midi_out = midi_thread->GetMidiOut();
				if (midi_out.Open(std::move(filestream))) {
					midi_out.SetPitch(pitch);
					midi_out.SetVolume(0);
					midi_out.SetFade(volume, std::chrono::milliseconds(fadein));
					midi_out.SetLooping(true);
					midi_out.Resume();
					chan.midi_out_used = true;
					midi_thread->UnlockMutex();
					return true;
				}
				midi_thread->UnlockMutex();
			}
		}
	}

	if (midi_thread) {
		midi_thread->GetMidiOut().Reset();
	}

	chan.decoder = AudioDecoder::Create(filestream);
	chan.midi_out_used = false;
	if (chan.decoder && chan.decoder->Open(std::move(filestream))) {
		Format format;
		chan.decoder->GetFormat(format.frequency, format.format, format.channels);
		chan.decoder->SetPitch(pitch);
		chan.decoder->SetVolume(0);
		chan.decoder->SetFade(volume, std::chrono::milliseconds(fadein));
		chan.decoder->SetLooping(true);

		auto sdl_format = to_sdl_format(format.format);
		if (sdl_format == 0) {
			sdl_format = SDL_AUDIO_S16;
			chan.decoder->SetFormat(format.frequency, AudioDecoder::Format::S16, format.channels);
		}

		chan.sdl_spec = { sdl_format, format.channels, format.frequency };
		chan.sdl_stream.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &chan.sdl_spec, sdl_bgm_audio_callback, &chan));

		if (!chan.sdl_stream) {
			Output::Warning("SDL_CreateAudioStream failed: {}", SDL_GetError());
			return false;
		}

		if (SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(chan.sdl_stream.get())) < 0) {
			Output::Warning("SDL_ResumeAudioDevice failed: {}", SDL_GetError());
			chan.sdl_stream.reset();
			return false;
		}

		return true;
	} else {
		Output::Warning("Couldn't play BGM {}. Format not supported", filestream.GetName());
	}

	return false;
}

bool Sdl3Audio::PlayOnChannel(SeChannel& chan, std::unique_ptr<AudioSeCache> se, int volume, int pitch) {
	Format format;
	chan.decoder = se->CreateSeDecoder();
	chan.decoder->SetPitch(pitch);
	chan.decoder->GetFormat(format.frequency, format.format, format.channels);
	chan.decoder->SetVolume(volume);

	bool use_raw_buffer = true;
	auto sdl_format = to_sdl_format(format.format);
	if (sdl_format == 0) {
		sdl_format = SDL_AUDIO_S16;
		chan.decoder->SetFormat(format.frequency, AudioDecoder::Format::S16, format.channels);
		use_raw_buffer = false;
	}

	chan.sdl_spec = { sdl_format, format.channels, format.frequency };
	chan.sdl_stream.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &chan.sdl_spec, nullptr, nullptr));

	if (!chan.sdl_stream) {
		Output::Warning("SDL_CreateAudioStream failed: {}", SDL_GetError());
		return false;
	}

	if (use_raw_buffer && pitch == 100) {
		auto se_ref = se->GetSeData();
		auto& buf = se_ref->buffer;

		int volume = chan.decoder->GetVolume();
		if (volume != 100) {
			auto buf_cp = buf;
			SDL_MixAudioFormat(buf_cp.data(), buf.data(), sdl_format, buf.size(), volume / 100.0f * SDL_MIX_MAXVOLUME);
			SDL_PutAudioStreamData(chan.sdl_stream.get(), buf_cp.data(), buf_cp.size());
		} else {
			SDL_PutAudioStreamData(chan.sdl_stream.get(), buf.data(), buf.size());
		}
	} else {
		auto buf = chan.decoder->DecodeAll();

		int volume = chan.decoder->GetVolume();
		if (volume != 100) {
			SDL_MixAudioFormat(buf.data(), buf.data(), sdl_format, buf.size(), volume / 100.0f * SDL_MIX_MAXVOLUME);
		}

		SDL_PutAudioStreamData(chan.sdl_stream.get(), buf.data(), buf.size());
	}

	if (SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(chan.sdl_stream.get())) < 0) {
		Output::Warning("SDL_ResumeAudioDevice failed: {}", SDL_GetError());
		chan.sdl_stream.reset();
		return false;
	}

	return true;
}

void Sdl3Audio::BgmChannel::Stop() {
	if (midi_out_used) {
		midi_out_used = false;
		instance->midi_thread->GetMidiOut().Reset();
		instance->midi_thread->GetMidiOut().Pause();
	} else if (decoder) {
		sdl_stream.reset();
		decoder.reset();
	}
}

void Sdl3Audio::BgmChannel::SetPaused(bool newPaused) {
	if (midi_out_used) {
		if (newPaused) {
			instance->midi_thread->GetMidiOut().Pause();
		} else {
			instance->midi_thread->GetMidiOut().Resume();
		}
	}
}

int Sdl3Audio::BgmChannel::GetTicks() const {
	if (midi_out_used) {
		return instance->midi_thread->GetMidiOut().GetTicks();
	} else if (decoder) {
		return decoder->GetTicks();
	}
	return -1;
}

void Sdl3Audio::BgmChannel::SetFade(int fade) {
	if (midi_out_used) {
		instance->midi_thread->GetMidiOut().SetFade(0, std::chrono::milliseconds(fade));
	} else if (decoder) {
		decoder->SetFade(0, std::chrono::milliseconds(fade));
	}
}

void Sdl3Audio::BgmChannel::SetVolume(int volume) {
	if (midi_out_used) {
		instance->midi_thread->GetMidiOut().SetVolume(volume);
	} else if (decoder) {
		decoder->SetVolume(volume);
	}
}

void Sdl3Audio::BgmChannel::SetPitch(int pitch) {
	if (midi_out_used) {
		instance->midi_thread->GetMidiOut().SetPitch(pitch);
	} else if (decoder) {
		decoder->SetPitch(pitch);
	}
}

bool Sdl3Audio::BgmChannel::IsUsed() const {
	return decoder || midi_out_used;
}
