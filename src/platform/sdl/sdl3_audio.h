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

#ifndef EP_AUDIO_SDL3_H
#define EP_AUDIO_SDL3_H

#include "audio.h"
#include "audio_secache.h"
#include "audio_decoder_base.h"
#include "audio_generic_midiout.h"
#include <SDL.h>
#include <SDL_audio.h>
#include <memory>

class Sdl3Audio : public AudioInterface {
public:
	Sdl3Audio(const Game_ConfigAudio& cfg);
	virtual ~Sdl3Audio() = default;

	void BGM_Play(Filesystem_Stream::InputStream stream, int volume, int pitch, int fadein) override;
	void BGM_Pause() override;
	void BGM_Resume() override;
	void BGM_Stop() override;
	bool BGM_PlayedOnce() const override;
	bool BGM_IsPlaying() const override;
	int BGM_GetTicks() const override;
	void BGM_Fade(int fade) override;
	void BGM_Volume(int volume) override;
	void BGM_Pitch(int pitch) override;
	std::string BGM_GetType() const override;

	void SE_Play(std::unique_ptr<AudioSeCache> se, int volume, int pitch) override;
	void SE_Stop() override;
	virtual void Update() override;

	void vGetConfig(Game_ConfigAudio&) const override {}

	void LockMutex() const {};
	void UnlockMutex() const {};

	struct SdlAudioDeleter {
		void operator()(SDL_AudioStream* a) const {
			if (a) {
				SDL_DestroyAudioStream(a);
			}
		}
	};

	struct AudioChannel {
		int id;
		std::unique_ptr<AudioDecoderBase> decoder;
		Sdl3Audio* instance = nullptr;
		std::unique_ptr<SDL_AudioStream, SdlAudioDeleter> sdl_stream;
		SDL_AudioSpec sdl_spec;
	};

	struct BgmChannel : public AudioChannel {
		bool midi_out_used = false;
		void Stop();
		void SetPaused(bool newPaused);
		int GetTicks() const;
		void SetFade(int fade);
		void SetVolume(int volume);
		void SetPitch(int pitch);
		bool IsUsed() const;
	};

	struct SeChannel : public AudioChannel {
	};
private:
	struct Format {
		int frequency;
		AudioDecoder::Format format;
		int channels;
	};
	SDL_AudioSpec device_spec;

	bool PlayOnChannel(BgmChannel& chan, Filesystem_Stream::InputStream stream, int volume, int pitch, int fadein);
	bool PlayOnChannel(SeChannel& chan, std::unique_ptr<AudioSeCache> se, int volume, int pitch);

	static constexpr unsigned nr_of_se_channels = 31;
	static constexpr unsigned nr_of_bgm_channels = 2;

	BgmChannel BGM_Channels[nr_of_bgm_channels];
	SeChannel SE_Channels[nr_of_se_channels];
	mutable bool BGM_PlayedOnceIndicator;
	bool Muted;

	std::vector<int16_t> sample_buffer = {};
	std::vector<uint8_t> scrap_buffer = {};
	unsigned scrap_buffer_size = 0;
	std::vector<float> mixer_buffer = {};

	std::unique_ptr<GenericAudioMidiOut> midi_thread;
};

#endif
