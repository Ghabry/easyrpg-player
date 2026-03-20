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

#ifdef HAVE_FFMPEG

#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include "audio_decoder.h"

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
    BitmapRef GetVideoFrame();

private:
	Filesystem_Stream::InputStream stream;

	// Audio Decoder interface
	int FillBuffer(uint8_t* buffer, int length) override;

	/** Queues new video and audio packets */
	void ReadPackets();
	/** Processed queud packets */
	void ProcessPackets();

	/**
	 * Synchronise audio converters with the stream
	 * @return true if all okay, or false if error happen
	 */
	bool UpdateAudioStream();
	bool UpdateVideoStream();

	int DecodeAudioPacket(AVPacket* paquet, bool &got);
	int DecodeVideoPacket(AVPacket* paquet, bool &got);

	/** Buffer for avio_alloc_context */
	uint8_t* in_buffer = nullptr;

	AVIOContext* avio_in = nullptr;

	/** Current playback position of the video */
	double playback_time = 0.0;
	/** Finished? */
	bool at_end = false;
	/** Finished and audio/video flushed? */
	bool audio_flushed = false;
	bool video_flushed = false;

	/* ------------------------------------------ */
	/** Input context of video stream */
	AVFormatContext* input_ctx = nullptr;
	/** Number of video stream */
	int video_stream_index = 0;
	/** Number of audio stream */
	int audio_stream_index = -1;
	/** Actual stream of video */
	AVStream* video_stream = nullptr;
	/** Video decoder context */
	AVCodecContext* video_decoder_ctx = nullptr;

	// Decoder itself
#if LIBAVCODEC_VERSION_MAJOR >= 60
	const AVCodec* video_decoder = nullptr;
#else
	AVCodec* video_decoder = nullptr;
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 60
	const AVCodec* audio_decoder = nullptr;
#else
	AVCodec* audio_decoder = nullptr;
#endif
	AVCodecContext* audio_decoder_ctx = nullptr;

	// Frames to process
	AVFrame* in_frame = nullptr;

	// Packet buffers
	std::deque<AVPacket> audio_packet_queue;
	std::deque<AVPacket> video_packet_queue;

	/** Video conversion handling */
	SwsContext* video_cvt = nullptr;
	AVPixelFormat video_src_format = AV_PIX_FMT_NONE;
	AVPixelFormat video_dst_format = AV_PIX_FMT_NONE;
	int video_width = 0;
	int video_height = 0;
	int video_pitch = 0;
	std::vector<uint8_t> video_pixel_data;

	/** Actual stream of audio */
	AVStream* audio_stream = nullptr;
	AVFrame* audio_frame = nullptr;

	/** Converts audio streams to the compatible format */
	SwrContext* swr_ctx = nullptr;

	/** Input audio format (of the video) */
	enum AVSampleFormat audio_src_format = AV_SAMPLE_FMT_NONE;
	int audio_src_freq = 0;
	int audio_src_channels = 0;

	/** Output audio format (for our Audio Decoder) */
	enum AVSampleFormat audio_dst_format = AV_SAMPLE_FMT_NONE;
	int audio_dst_freq = 12345;
	AudioDecoder::Format audio_dst_format_decoder = AudioDecoder::Format::S16;
	int audio_dst_channels = 2;

	std::vector<uint8_t> audio_merge_buffer;
	/** Stores processed audio samples */
	std::vector<uint8_t> audio_buffer;

	struct VideoFrame {
		BitmapRef frame;
		double time;
	};
	/** Stores already processed video frames together with a timestamp */
	std::vector<VideoFrame> video_buffer;

	/** Thread handling all the processing */
	std::thread av_thread;

	/** Function executed by av_thread */
	void ThreadFunction();

	/**
	 * Mutex for synchronisation
	 * Only video_buffer and audio_buffer are accessed on multiple threads.
	 */
	std::mutex av_mutex;
};

#endif

#endif
