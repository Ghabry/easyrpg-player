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

#ifdef SUPPORT_AUDIO

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

#include "audio.h"
#include "filefinder.h"
#include "game_clock.h"
#include "output.h"
#include "audio_secache.h"
#include "audio_decoder_base.h"

//#define EP_DEBUG_CTRAUDIO
#ifdef EP_DEBUG_CTRAUDIO
using namespace std::chrono_literals;
Game_Clock::time_point dsp_tick;
#define DebugLog Output::Debug
#else
#define DebugLog(...)
#endif

/*
 * Audio on the 3DS is handled by the DSP:
 *  - Sound effects: The whole (cached) PCM data is handed to the DSP.
 *  - Music: Streamed by a thread that decodes into a ring of wave buffers.
 *  - Pitch: RPG Maker pitch changes speed and pitch together. This is done by
 *    changing the DSP sample rate, which costs no CPU time.
 */

namespace {
	bool is_new_3ds = false;
	bool dsp_inited = false;

	struct _se {
		// Channels 0 - 22, channel 23 is used by the music
		constexpr static int channels = 23;
		ndspWaveBuf buf[channels] = {};
		// Capacity of the linear memory buffer of each channel
		size_t capacity[channels] = {};
	} se;

	struct _bgm {
		constexpr static int channel = se.channels;
		// 4 x 16 KiB: ~370 ms at 44.1 kHz stereo, enough to survive decoding hiccups
		constexpr static int buf_size = 16384;
		constexpr static int num_bufs = 4;
		ndspWaveBuf buf[num_bufs] = {};
		// Next buffer to refill (buffers finish in the order they were queued)
		int next_buf = 0;
		uint8_t* audio_buffer = nullptr;
		std::unique_ptr<AudioDecoderBase> decoder;
		Game_Clock::time_point starttick;
		int frequency = 0;
		int pitch = 100;
		bool paused = false;
		// Read by the DSP callback: true while the stream thread must refill buffers
		std::atomic<bool> streaming{false};
	} bgm;

	LightLock audio_mutex;
	LightEvent audio_event;
	Thread audio_thread;
	std::atomic<bool> term_stream{false};

	bool IsDspFormat(AudioDecoder::Format format, int channels) {
		return (channels == 1 || channels == 2) &&
			(format == AudioDecoder::Format::S8 || format == AudioDecoder::Format::S16);
	}

	/**
	 * Ensures the decoder outputs a format the DSP can play.
	 *
	 * @return false when the decoder cannot be converted
	 */
	bool SetupDecoderFormat(AudioDecoderBase& dec, int& frequency, AudioDecoder::Format& format, int& channels) {
		dec.GetFormat(frequency, format, channels);
		if (IsDspFormat(format, channels)) {
			return true;
		}

		const bool is_8bit = (format == AudioDecoder::Format::U8 || format == AudioDecoder::Format::S8);
		const int target_channels = std::clamp(channels, 1, 2);
		// Prefer S8 for 8 bit data, but not all decoders can output it
		if (!(is_8bit && dec.SetFormat(frequency, AudioDecoder::Format::S8, target_channels)) &&
				!dec.SetFormat(frequency, AudioDecoder::Format::S16, target_channels)) {
			return false;
		}

		// The decoder may have chosen something else
		dec.GetFormat(frequency, format, channels);
		return IsDspFormat(format, channels);
	}

	/**
	 * Converts PCM data into a format the DSP can play (S8 or S16).
	 * Used for cached sound effects, their decoder does not convert formats.
	 *
	 * @return false when the format is not supported
	 */
	bool ConvertToDspFormat(const std::vector<uint8_t>& in, AudioDecoder::Format& format, std::vector<uint8_t>& out) {
		using Format = AudioDecoder::Format;

		switch (format) {
			case Format::U8:
				out.resize(in.size());
				for (size_t i = 0; i < in.size(); ++i) {
					out[i] = in[i] ^ 0x80;
				}
				format = Format::S8;
				return true;
			case Format::U16:
			case Format::S32:
			case Format::U32:
			case Format::F32: {
				const size_t in_size = AudioDecoder::GetSamplesizeForFormat(format);
				const size_t n = in.size() / in_size;
				out.resize(n * 2);
				auto* dst = reinterpret_cast<int16_t*>(out.data());
				for (size_t i = 0; i < n; ++i) {
					const uint8_t* p = &in[i * in_size];
					int32_t v = 0;
					if (format == Format::U16) {
						uint16_t u;
						memcpy(&u, p, 2);
						v = static_cast<int32_t>(u) - 0x8000;
					} else if (format == Format::S32) {
						int32_t x;
						memcpy(&x, p, 4);
						v = x >> 16;
					} else if (format == Format::U32) {
						uint32_t x;
						memcpy(&x, p, 4);
						v = static_cast<int32_t>(x >> 16) - 0x8000;
					} else {
						float f;
						memcpy(&f, p, 4);
						v = static_cast<int32_t>(std::clamp(f, -1.0f, 1.0f) * 32767.0f);
					}
					dst[i] = static_cast<int16_t>(v);
				}
				format = Format::S16;
				return true;
			}
			default:
				return false;
		}
	}

	void SetDspFormat(int dsp_chn, AudioDecoder::Format format, int channels) {
		if (format == AudioDecoder::Format::S8) {
			ndspChnSetFormat(dsp_chn, channels == 1 ? NDSP_FORMAT_MONO_PCM8 : NDSP_FORMAT_STEREO_PCM8);
		} else {
			ndspChnSetFormat(dsp_chn, channels == 1 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16);
		}
	}

	float PitchedRate(int frequency, int pitch) {
		return frequency * (pitch / 100.0f);
	}

	void ResetBgmChannel() {
		// Marks all queued buffers as done
		ndspChnWaveBufClear(bgm.channel);
		ndspChnReset(bgm.channel);
		ndspChnSetInterp(bgm.channel, NDSP_INTERP_LINEAR);

		for (auto& buf: bgm.buf) {
			buf.status = NDSP_WBUF_DONE;
		}
		bgm.next_buf = 0;
	}

	class AudioLock {
	public:
		explicit AudioLock(const CtrAudio& audio) : audio(audio) { audio.LockMutex(); }
		~AudioLock() { audio.UnlockMutex(); }
		AudioLock(const AudioLock&) = delete;
		AudioLock& operator=(const AudioLock&) = delete;
	private:
		const CtrAudio& audio;
	};
}

void CtrAudio::BGM_Play(Filesystem_Stream::InputStream filestream, int volume, int pitch, int fadein, int balance) {
	if (!dsp_inited)
		return;

	std::string_view name = filestream.GetName();
	if (!filestream) {
		Output::Warning("Couldn't play BGM {}: File not readable", name);
		return;
	}

	AudioLock lock(*this);

	// Stop the old stream: Its buffers have a different format
	bgm.streaming = false;
	bgm.decoder.reset();
	ResetBgmChannel();

	auto decoder = AudioDecoder::Create(filestream);
	if (!decoder || !decoder->Open(std::move(filestream))) {
		Output::Warning("Couldn't play BGM {}: Format not supported", name);
		return;
	}

	int frequency;
	AudioDecoder::Format format;
	int channels;

	// Pitch is done by the DSP
	decoder->SetPitch(100);

	if (!SetupDecoderFormat(*decoder, frequency, format, channels)) {
		Output::Warning("Couldn't play BGM {}: Unsupported sample format", name);
		return;
	}

	decoder->SetVolume(0);
	decoder->SetFade(volume, std::chrono::milliseconds(fadein));
	decoder->SetLooping(true);
	decoder->SetBalance(balance);

	SetDspFormat(bgm.channel, format, channels);
	bgm.frequency = frequency;
	bgm.pitch = pitch;
	ndspChnSetRate(bgm.channel, PitchedRate(frequency, pitch));

	const int samplesize = AudioDecoder::GetSamplesizeForFormat(format);
	const int nsamples = bgm.buf_size / (samplesize * channels);
	for (auto& buf: bgm.buf) {
		buf.nsamples = nsamples;
	}

	bgm.decoder = std::move(decoder);
	bgm.paused = false;
	bgm.starttick = Game_Clock::now();
	ndspChnSetPaused(bgm.channel, false);

	// Start filling the buffers
	bgm.streaming = true;
	LightEvent_Signal(&audio_event);
}

void CtrAudio::BGM_Pause()  {
	if (!dsp_inited)
		return;

	AudioLock lock(*this);
	bgm.paused = true;
	ndspChnSetPaused(bgm.channel, true);
}

void CtrAudio::BGM_Resume()  {
	if (!dsp_inited)
		return;

	AudioLock lock(*this);
	bgm.paused = false;
	bgm.starttick = Game_Clock::now();
	ndspChnSetPaused(bgm.channel, false);
}

void CtrAudio::BGM_Stop() {
	if (!dsp_inited)
		return;

	AudioLock lock(*this);
	bgm.streaming = false;
	bgm.decoder.reset();
	ResetBgmChannel();
}

bool CtrAudio::BGM_PlayedOnce() const {
	AudioLock lock(*this);
	if (!bgm.decoder)
		return false;

	return bgm.decoder->GetLoopCount() > 0;
}

bool CtrAudio::BGM_IsPlaying() const {
	AudioLock lock(*this);
	if (!bgm.decoder)
		return false;

	return !bgm.decoder->IsFinished();
}

int CtrAudio::BGM_GetTicks() const {
	AudioLock lock(*this);
	if (!bgm.decoder)
		return 0;

	return bgm.decoder->GetTicks();
}

void CtrAudio::BGM_Fade(int fade) {
	AudioLock lock(*this);
	if (!bgm.decoder)
		return;

	bgm.starttick = Game_Clock::now();
	bgm.decoder->SetFade(0, std::chrono::milliseconds(fade));
}

void CtrAudio::BGM_Volume(int volume) {
	AudioLock lock(*this);
	if (!bgm.decoder)
		return;

	bgm.decoder->SetVolume(volume);
}

void CtrAudio::BGM_Pitch(int pitch) {
	AudioLock lock(*this);
	if (!bgm.decoder)
		return;

	bgm.pitch = pitch;
	ndspChnSetRate(bgm.channel, PitchedRate(bgm.frequency, pitch));
}

void CtrAudio::BGM_Balance(int balance) {
	AudioLock lock(*this);
	if (!bgm.decoder)
		return;

	bgm.decoder->SetBalance(balance);
}

std::string CtrAudio::BGM_GetType() const {
	AudioLock lock(*this);
	if (bgm.decoder) {
		return bgm.decoder->GetType();
	}
	return "";
}

void CtrAudio::SE_Play(std::unique_ptr<AudioSeCache> se_cache, int volume, int pitch, int balance) {
	if (!dsp_inited)
		return;

	if (!se_cache) {
		Output::Warning("SE_Play: cache data is invalid");
		return;
	}

	// A channel is free when the DSP is not playing it and no buffer is pending.
	// ndspChnIsPlaying alone is not enough: It is updated by the DSP once per
	// audio frame (~5 ms), so a buffer queued in the same game frame is not
	// reported as playing yet.
	int chan = -1;
	for (int i = 0; i < se.channels; ++i) {
		const auto status = se.buf[i].status;
		if (!ndspChnIsPlaying(i) && status != NDSP_WBUF_QUEUED && status != NDSP_WBUF_PLAYING) {
			chan = i;
			break;
		}
	}
	if (chan == -1) {
		Output::Debug("Couldn't play SE {}: No free channel available", se_cache->GetName());
		return;
	}

	auto dec = se_cache->CreateSeDecoder();
	// Pitch is done by the DSP
	dec->SetPitch(100);
	dec->SetVolume(volume);
	dec->SetBalance(balance);

	int frequency;
	AudioDecoder::Format format;
	int channels;

	dec->GetFormat(frequency, format, channels);

	std::vector<uint8_t> dec_buf;
	AudioSeRef se_ref;
	const std::vector<uint8_t>* out_buf = nullptr;

	se_ref = se_cache->GetSeData();
	if (IsDspFormat(format, channels)) {
		// The DSP can play the cached data directly
		out_buf = &se_ref->buffer;
	} else {
		if ((channels != 1 && channels != 2) || !ConvertToDspFormat(se_ref->buffer, format, dec_buf)) {
			Output::Warning("Couldn't play SE {}: Unsupported sample format", se_cache->GetName());
			return;
		}
		out_buf = &dec_buf;
	}

	const size_t bsize = out_buf->size();
	const int samplesize = AudioDecoder::GetSamplesizeForFormat(format);
	const int nsamples = static_cast<int>(bsize / (samplesize * channels));
	if (nsamples <= 0) {
		return;
	}

	auto& wbuf = se.buf[chan];

	// Reuse the buffer of the channel when it is large enough (the channel is idle)
	if (se.capacity[chan] < bsize) {
		if (wbuf.data_pcm8) {
			linearFree(wbuf.data_pcm8);
		}
		// Round up to reduce reallocations and fragmentation of the linear heap
		const size_t capacity = (bsize + 0xFFF) & ~static_cast<size_t>(0xFFF);
		wbuf.data_pcm8 = static_cast<s8*>(linearAlloc(capacity));
		se.capacity[chan] = wbuf.data_pcm8 ? capacity : 0;
		if (!wbuf.data_pcm8) {
			Output::Debug("Couldn't play SE {}: Out of memory", se_cache->GetName());
			return;
		}
	}

	memcpy(wbuf.data_pcm8, out_buf->data(), bsize);
	DSP_FlushDataCache(wbuf.data_pcm8, bsize);
	wbuf.nsamples = nsamples;
	wbuf.looping = false;

	SetDspFormat(chan, format, channels);
	ndspChnSetRate(chan, PitchedRate(frequency, pitch));

	// Volume and balance as calculated by the decoder (same as GenericAudio)
	StereoVolume vol = dec->GetVolume();
	const float master = cfg.sound_volume.Get() / 100.0f;
	float mix[12] = {};
	mix[0] = vol.left_volume / 100.0f * master;
	mix[1] = vol.right_volume / 100.0f * master;
	ndspChnSetMix(chan, mix);

	ndspChnWaveBufAdd(chan, &wbuf);
}

void CtrAudio::SE_Stop() {
	if (!dsp_inited)
		return;

	// Only the SE channels, the music channel is not affected
	for (int i = 0; i < se.channels; ++i) {
		ndspChnWaveBufClear(i);
	}
}

void CtrAudio::Update() {
	AudioLock lock(*this);
	if (!bgm.decoder)
		return;

	auto t = Game_Clock::now();
	if (!bgm.paused) {
		auto us = std::chrono::duration_cast<std::chrono::microseconds>(t - bgm.starttick);
		bgm.decoder->Update(us);
	}
	bgm.starttick = t;
}

void n3ds_dsp_callback(void*) {
	// Called by the DSP once per audio frame: Wake the stream thread when a buffer finished
	if (bgm.streaming) {
		for (const auto& buf: bgm.buf) {
			if (buf.status == NDSP_WBUF_DONE || buf.status == NDSP_WBUF_FREE) {
				LightEvent_Signal(&audio_event);
				break;
			}
		}
	}

#ifdef EP_DEBUG_CTRAUDIO
	auto t = Game_Clock::now();
	auto s = std::chrono::duration_cast<std::chrono::seconds>(t - dsp_tick);
	if (s > 5s) {
		dsp_tick = t;
		u32 all = ndspGetFrameCount();
		u32 dropped = ndspGetDroppedFrames();
		float percent = (float)dropped/all*100;
		if (percent > 0.0f)
			Output::Debug("DSP frames dropped: {}% ({} of {})", percent, dropped, all);
	}
#endif
}

void n3ds_audio_thread(void* userdata) {
	CtrAudio* audio = static_cast<CtrAudio*>(userdata);
	float mix[12] = {};

	while (!term_stream) {
		LightEvent_Wait(&audio_event);
		if (term_stream) {
			break;
		}

		AudioLock lock(*audio);
		if (!bgm.decoder || !bgm.streaming) {
			continue;
		}

		// Refill all finished buffers in queue order
		for (int n = 0; n < bgm.num_bufs; ++n) {
			auto& buf = bgm.buf[bgm.next_buf];
			if (buf.status != NDSP_WBUF_DONE && buf.status != NDSP_WBUF_FREE) {
				break;
			}

			int written = bgm.decoder->Decode(reinterpret_cast<uint8_t*>(buf.data_pcm8), bgm.buf_size);
			if (written < 0) {
				// Decoder error: Play silence
				memset(buf.data_pcm8, 0, bgm.buf_size);
			}
			DSP_FlushDataCache(buf.data_pcm8, bgm.buf_size);

			StereoVolume volume = bgm.decoder->GetVolume();
			const float master = audio->GetConfig().music_volume.Get() / 100.0f;
			mix[0] = volume.left_volume / 100.0f * master;
			mix[1] = volume.right_volume / 100.0f * master;
			ndspChnSetMix(bgm.channel, mix);
			ndspChnWaveBufAdd(bgm.channel, &buf);

			bgm.next_buf = (bgm.next_buf + 1) % bgm.num_bufs;
		}
	}
	threadExit(0);
}

CtrAudio::CtrAudio(const Game_ConfigAudio& cfg) : AudioInterface(cfg) {
	LightLock_Init(&audio_mutex);
	LightEvent_Init(&audio_event, RESET_ONESHOT);

	Result res = ndspInit();
	if (R_FAILED(res)) {
		Output::Warning("Couldn't initialize audio");
		if ((R_SUMMARY(res) == RS_NOTFOUND) && (R_MODULE(res) == RM_DSP))
			Output::Warning("This needs a dumped DSP firmware to work!");
		else
			Output::Warning("Error code: {:#X}", res);
		return;
	}

	bgm.audio_buffer = static_cast<uint8_t*>(linearAlloc(bgm.buf_size * bgm.num_bufs));
	if (!bgm.audio_buffer) {
		Output::Warning("Couldn't allocate audio buffer");
		ndspExit();
		return;
	}
	memset(bgm.audio_buffer, 0, bgm.buf_size * bgm.num_bufs);
	DSP_FlushDataCache(bgm.audio_buffer, bgm.buf_size * bgm.num_bufs);

	ndspSetOutputCount(1);
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	// Music and sound effects are mixed at full volume: Avoid hard clipping
	ndspSetClippingMode(NDSP_CLIP_SOFT);
	for (int i = 0; i <= bgm.channel; ++i) {
		ndspChnSetInterp(i, NDSP_INTERP_LINEAR);
	}
	dsp_inited = true;

	APT_CheckNew3DS(&is_new_3ds);

	for (int i = 0; i < bgm.num_bufs; ++i) {
		bgm.buf[i].data_pcm8 = reinterpret_cast<s8*>(bgm.audio_buffer + i * bgm.buf_size);
		bgm.buf[i].nsamples = bgm.buf_size / 4;
		bgm.buf[i].status = NDSP_WBUF_DONE;
	}

	ndspSetCallback(n3ds_dsp_callback, this);

	// Decoding runs on the system core (core 1) or, on the New 3DS, on the otherwise unused core 2.
	// Core 0 is reserved for the game logic.
	if (is_new_3ds) {
		audio_thread = threadCreate(n3ds_audio_thread, this, 32768, 0x18, 2, true);
	}
	if (!audio_thread) {
		audio_thread = threadCreate(n3ds_audio_thread, this, 32768, 0x18, 1, true);
	}
	if (!audio_thread) {
		Output::Warning("Couldn't create bgm thread!");
	}

#ifdef EP_DEBUG_CTRAUDIO
	dsp_tick = Game_Clock::now();
#endif
}

CtrAudio::~CtrAudio() {
	if (!dsp_inited)
		return;

	bgm.streaming = false;
	term_stream = true;
	LightEvent_Signal(&audio_event);
	if (audio_thread) {
		threadJoin(audio_thread, U64_MAX);
	}
	LightEvent_Clear(&audio_event);
	ndspExit();

	bgm.decoder.reset();
	linearFree(bgm.audio_buffer);
	for (int i = 0; i < se.channels; ++i) {
		if (se.buf[i].data_pcm8 != nullptr) {
			linearFree(se.buf[i].data_pcm8);
		}
	}
}

void CtrAudio::LockMutex() const {
	LightLock_Lock(&audio_mutex);
}

void CtrAudio::UnlockMutex() const {
	LightLock_Unlock(&audio_mutex);
}

#endif
