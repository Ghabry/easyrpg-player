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

#ifndef VIDEO_DECODER_H
#define VIDEO_DECODER_H

#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include "audio_decoder.h"
#include "bitmap.h"

extern "C"
{
#include <libavcodec/version_major.h>
#include <libavcodec/packet.h>
#include <libavutil/samplefmt.h>
#include <libavutil/pixfmt.h>
}

// FFMPEG's structures
struct AVFormatContext;
typedef struct AVFormatContext AVFormatContext;
struct AVStream;
typedef struct AVStream AVStream;
struct AVCodecContext;
typedef struct AVCodecContext AVCodecContext;
struct AVCodec;
typedef struct AVCodec AVCodec;
struct AVFrame;
typedef struct AVFrame AVFrame;
struct SwrContext;
typedef struct SwrContext SwrContext;
struct SwsContext;
typedef struct AVIOContext AVIOContext;
struct AVIOContext;

class VideoDecoder : public AudioDecoder {
public:
	VideoDecoder();
	~VideoDecoder() override;

	// Audio Decoder interface
	bool Open(Filesystem_Stream::InputStream stream) override;

	bool Seek(std::streamoff offset, std::ios_base::seekdir origin) override;

	bool IsFinished() const override;

	void GetFormat(int& frequency, AudioDecoder::Format& format, int& channels) const override;

	bool SetFormat(int frequency, AudioDecoder::Format format, int channels) override;

	int GetTicks() const override;

	// Video Decoder interface
    BitmapRef getVideoFrame();

    static void audio_out_stream(void *self, uint8_t *stream, int bytes);

private:
	Filesystem_Stream::InputStream stream;

	// Audio Decoder interface
	int FillBuffer(uint8_t* buffer, int length) override;

	friend int64_t _rw_seek(void *opaque, int64_t offset, int whence);
	friend int _rw_read_buffer(void *opaque, uint8_t *buf, int buf_size);
	uint8_t* in_buffer = nullptr;
	size_t in_buffer_size = 0;

	std::vector<uint8_t> m_texturePixelData;
	AVIOContext* avio_in = nullptr;
	bool m_freesrc = false;

	SwsContext* m_video_cvt = nullptr;

	AVPixelFormat   m_texture_colour = AV_PIX_FMT_NONE;
	BitmapRef m_texture;
	int m_texture_w = 0;
	int m_texture_h = 0;
	int m_texture_pitch = 0;

	AVPixelFormat   m_dst_colour = AV_PIX_FMT_NONE;
	int m_dst_w = 0;
	int m_dst_h = 0;

	double m_playback_time = 0.0;
	double m_time = 0.0;
	double m_timeNextFrame = 0.0;
	bool m_atEnd = false;
	bool m_hasVideoFrame = false;

	/* ------------------------------------------ */
	//! Input context of video stream
	AVFormatContext *m_inputCtx = nullptr;
	//! Number of video stream
	int m_streamVideo = 0;
	//! Number of audio stream
	int m_streamAudio = -1;
	//! Actual stream of video
	AVStream *m_video = nullptr;
	//! Video decoder context
	AVCodecContext *m_decoderVideoCtx = nullptr;

	//! Decoder itself
#if LIBAVCODEC_VERSION_MAJOR >= 60
	const AVCodec *m_decoderVideo = nullptr;
#else
	AVCodec *m_decoderVideo = nullptr;
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 60
	const AVCodec *m_decoderAudio = nullptr;
#else
	AVCodec *m_decoderAudio = nullptr;
#endif
	AVCodecContext *m_decoderAudioCtx = nullptr;

	//! Frames to process
	AVFrame *sw_frame = nullptr;
	AVFrame *in_frame = nullptr;
	//! Packet buffer
	AVPacket m_paquet;
	std::deque<AVPacket> m_audio_packet_queue;
	std::deque<AVPacket> m_video_packet_queue;

	void readPackets();
	void processPackets();

	//! Actual stream of audio
	AVStream *m_audio = nullptr;
	AVFrame *m_audio_frame = nullptr;

	//! Converts planar audio streams to the compatible format
	SwrContext *m_swr_ctx = nullptr;
	enum AVSampleFormat m_sfmt = AV_SAMPLE_FMT_NONE;
	int m_srate = 0;
	int m_schannels = 0;
	bool m_planar = false;

	enum AVSampleFormat m_dst_sample_fmt = AV_SAMPLE_FMT_NONE;

	int m_dec_freq = 12345;
	AudioDecoder::Format m_dec_fmt = AudioDecoder::Format::S16;
	int m_dec_channels = 2;

	std::vector<uint8_t> m_merge_buffer;
	std::vector<uint8_t> m_audio_buffer;

	/**
	 * @brief Synchronise audio converters with the stream
	 * @return true if all okay, or false if error happen
	 *
	 * Synchronises all the audio converters if stream changes the content (this might happen if stream is a Frankenstein).
	 */
	bool updateAudioStream();
	bool updateVideoStream();

	int decode_audio_packet(bool &got);
	int decode_video_packet(AVPacket &paquet, bool &got, double video_time);

	void ThreadFunction();

	struct VideoFrame {
		BitmapRef frame;
		double time;
	};
	std::vector<VideoFrame> frames;

	std::thread av_thread;
	std::mutex av_mutex;
};

#endif
