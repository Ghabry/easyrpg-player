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

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <string>

#include "video_decoder.h"
#include "bitmap.h"
#include "output.h"

#define AUDIO_INBUF_SIZE 4096

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 24, 100)
#define AVCODEC_NEW_CHANNEL_LAYOUT
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 0, 100)
#define AVFORMAT_NEW_avcodec_find_decoder
#endif

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(60, 12, 100)
#define AVFORMAT_NEW_avio_alloc_context
#endif

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(59, 0, 100)
#define AVFORMAT_NEW_avformat_open_input
#define AVFORMAT_NEW_av_find_best_stream
#endif

#if LIBSWRESAMPLE_VERSION_INT >= AV_VERSION_INT(4, 14, 100)
#define AVFORMAT_NEW_swr_convert
#endif

static std::string av_error_to_str(int err)
{
	std::string ret;
	ret.resize(AV_ERROR_MAX_STRING_SIZE);
	av_strerror(err, &ret[0], AV_ERROR_MAX_STRING_SIZE);
	ret.resize(strlen(ret.c_str()));
	return ret;
}

static int vio_read_func(void *userdata, uint8_t *ptr, int size) {
	auto* f = reinterpret_cast<Filesystem_Stream::InputStream*>(userdata);
	if (size == 0) return 0;

	auto res = f->read(reinterpret_cast<char*>(ptr), size).gcount();

	if (res == 0) {
		return AVERROR_EOF;
	}

	return res;
}

static int64_t vio_seek_func(void* userdata, int64_t offset, int seek_type) {
	auto* f = reinterpret_cast<Filesystem_Stream::InputStream*>(userdata);
	if (f->eof()) f->clear(); // emulate behaviour of fseek

	// Ignore AVSEEK_FORCE
	seek_type = seek_type & ~AVSEEK_FORCE;

	if (seek_type == AVSEEK_SIZE) {
		return f->GetSize();
	}

	f->seekg(offset, Filesystem_Stream::CSeekdirToCppSeekdir(seek_type));

	return f->tellg();
}

void VideoDecoder::ReadPackets() {
    AVPacket pkt;
    if (av_read_frame(input_ctx, &pkt) >= 0) {
        if (pkt.stream_index == audio_stream_index) {
            audio_packet_queue.push_back(pkt);
        } else if (pkt.stream_index == video_stream_index) {
            video_packet_queue.push_back(pkt);
        } else {
            av_packet_unref(&pkt);
        }
    } else {
        at_end = true;
    }
}

void VideoDecoder::ProcessPackets() {
	// Process Audio queue
    // Decode Audio if buffer is low
    while (!audio_packet_queue.empty() && audio_buffer.size() < 20480) {
        AVPacket pkt = audio_packet_queue.front();
        audio_packet_queue.pop_front();

        bool got;
        DecodeAudioPacket(&pkt, got);
        av_packet_unref(&pkt);
    }

	// Process Video queue
    // Decode video if not enough frames
    while (!video_packet_queue.empty() && video_buffer.size() < 10) {
        AVPacket pkt = video_packet_queue.front();
        video_packet_queue.pop_front();

        bool got;
        DecodeVideoPacket(&pkt, got);
        av_packet_unref(&pkt);
    }

	// Flush decoders at EOF
    if (at_end) {
        if (audio_packet_queue.empty() && !audio_flushed) {
            bool got;
            // Forces the decoder to empty its internal buffers
			if (audio_stream) {
            	DecodeAudioPacket(nullptr, got);
			}
            audio_flushed = true;
        }

        if (video_packet_queue.empty() && !video_flushed) {
            bool got;
            // Flushes remaining B-frames/delayed frames
            DecodeVideoPacket(nullptr, got);
            video_flushed = true;
        }
    }
}

bool VideoDecoder::UpdateAudioStream()
{
	if (!audio_stream || !audio_stream->codecpar)
		return true; // No audio - no actions!

	enum AVSampleFormat sfmt = (enum AVSampleFormat)audio_stream->codecpar->format;
	int srate = audio_stream->codecpar->sample_rate;

#if defined(AVCODEC_NEW_CHANNEL_LAYOUT)
	int channels = audio_stream->codecpar->ch_layout.nb_channels;
#else
	int channels = m_audio->codecpar->channels;
#endif

#if defined(AVCODEC_NEW_CHANNEL_LAYOUT)
	AVChannelLayout layout;
#else
	int layout;
#endif

	if (srate == 0 || channels == 0)
		return false;

	if (sfmt != audio_src_format || srate != audio_src_freq || channels != audio_src_channels)
	{
#if 0
		switch(sfmt)
		{
		case AV_SAMPLE_FMT_U8P:
		case AV_SAMPLE_FMT_U8:
			audio_dst_format = AV_SAMPLE_FMT_U8;
			break;

		case AV_SAMPLE_FMT_S16P:
		case AV_SAMPLE_FMT_S16:
			audio_dst_format = AV_SAMPLE_FMT_S16;
			break;

		case AV_SAMPLE_FMT_S32P:
		case AV_SAMPLE_FMT_S32:
			audio_dst_format = AV_SAMPLE_FMT_S32;
			break;

		case AV_SAMPLE_FMT_FLTP:
		case AV_SAMPLE_FMT_FLT:
			audio_dst_format = AV_SAMPLE_FMT_FLT;
			break;

		default:
			return false; /* Unsupported audio format */
		}
#endif

		// For simplicity always S16
		audio_dst_format = AV_SAMPLE_FMT_S16;
		audio_dst_format_decoder = AudioDecoder::Format::S16;

		audio_merge_buffer.clear();

		if (swr_ctx)
		{
			swr_free(&swr_ctx);
			swr_ctx = nullptr;
		}

		swr_ctx = swr_alloc();
#if defined(AVCODEC_NEW_CHANNEL_LAYOUT)
		layout = audio_stream->codecpar->ch_layout;
#else
		layout = m_audio->codecpar->channel_layout;
#endif

#if defined(AVCODEC_NEW_CHANNEL_LAYOUT)
		if (layout.u.mask == 0)
		{
			layout.order = AV_CHANNEL_ORDER_NATIVE;
			layout.nb_channels = channels;

			if (channels > 2)
				layout.u.mask = AV_CH_LAYOUT_SURROUND;
			else if (channels == 2)
				layout.u.mask = AV_CH_LAYOUT_STEREO;
			else if (channels == 1)
				layout.u.mask = AV_CH_LAYOUT_MONO;
		}

		av_opt_set_chlayout(swr_ctx, "in_chlayout",  &layout, 0);

		AVChannelLayout out_layout;

		av_channel_layout_default(&out_layout, audio_dst_channels);

		av_opt_set_chlayout(swr_ctx, "out_chlayout", &out_layout, 0);
#else
		if (layout == 0)
		{
			if (channels > 2)
				layout = AV_CH_LAYOUT_SURROUND;
			else if (channels == 2)
				layout = AV_CH_LAYOUT_STEREO;
			else if (channels == 1)
				layout = AV_CH_LAYOUT_MONO;
		}

		av_opt_set_int(m_swr_ctx, "in_channel_layout",  layout, 0);

		if (m_dec_channels == 1) {
			layout = AV_CH_LAYOUT_MONO;
		} else {
			layout = AV_CH_LAYOUT_STEREO;
		}

		av_opt_set_int(m_swr_ctx, "out_channel_layout", layout, 0);
#endif
		av_opt_set_int(swr_ctx, "in_sample_rate", srate, 0);
		av_opt_set_int(swr_ctx, "out_sample_rate", audio_dst_freq, 0);
		av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt",  sfmt, 0);
		// AV_SAMPLE_FMT must match m_dec_fmt
		av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", audio_dst_format,  0);
		swr_init(swr_ctx);

#if defined(AVCODEC_NEW_CHANNEL_LAYOUT)
		//av_channel_layout_uninit(&layout);
#endif

		audio_merge_buffer.resize(channels * av_get_bytes_per_sample(audio_dst_format) * 192000);

		audio_src_format = sfmt;
		audio_src_freq = srate;
		audio_src_channels = channels;
	}

	return true;
}

bool VideoDecoder::UpdateVideoStream()
{
	if (!video_stream || !video_stream->codecpar)
		return true; // No video - no actions!

	AVPixelFormat pixfmt = (AVPixelFormat)video_stream->codecpar->format;
	int w = video_stream->codecpar->width;
	int h = video_stream->codecpar->height;

	if (pixfmt == AV_PIX_FMT_NONE || w == 0 || h == 0)
		return false;

	if (w != video_width || h != video_height || pixfmt != video_dst_format || !video_cvt)
	{
		if (video_cvt)
		{
			sws_freeContext(video_cvt);
			video_cvt = nullptr;
		}

		video_cvt = sws_getContext(w, h, pixfmt, video_width, video_height, video_dst_format, 0, 0, 0, 0);
		if (!video_cvt)
			return false;

		uint8_t *dst_data[4];
		int dst_line_sizes[4];

		int data_size = av_image_fill_arrays(dst_data, dst_line_sizes, nullptr, video_dst_format, w, h, 8);
		video_pitch = dst_line_sizes[0];
		video_pixel_data.resize(data_size);
	}

	return false;
}

int VideoDecoder::DecodeAudioPacket(AVPacket* paquet, bool &got)
{
	int ret = 0;
	size_t unpadded_linesize;
	size_t sample_size;

	got = false;

	ret = avcodec_send_packet(audio_decoder_ctx, paquet);
	if (ret < 0)
	{
		if (ret == AVERROR_EOF)
			return ret;

		Output::Warning("FFMPEG: ERROR: Error submitting a packet for decoding ({})", av_error_to_str(ret));
		return ret;
	}

	while (ret >= 0)
	{
		ret = avcodec_receive_frame(audio_decoder_ctx, audio_frame);

		if (ret < 0)
		{
			/* those two return values are special and mean there is no output */
			/* frame available, but there were no errors during decoding */
			if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
				return 0;

			Output::Warning("FFMPEG: Error during decoding ({})", av_error_to_str(ret));
			return ret;
		}

		UpdateAudioStream();

		int expected_out_samples = swr_get_out_samples(swr_ctx, audio_frame->nb_samples);
		int out_bytes_per_sample = AudioDecoder::GetSamplesizeForFormat(audio_dst_format_decoder);

		size_t required_buffer_size = expected_out_samples * audio_dst_channels * out_bytes_per_sample;
		if (audio_merge_buffer.size() < required_buffer_size) {
			audio_merge_buffer.resize(required_buffer_size);
		}

		uint8_t *out = audio_merge_buffer.data();

		int out_samples = swr_convert(swr_ctx,
			&out, expected_out_samples,
			(const uint8_t**)audio_frame->extended_data, audio_frame->nb_samples);

		if (out_samples > 0) {
			auto out_bytes = out_samples * out_bytes_per_sample * audio_dst_channels;

			const std::lock_guard<std::mutex> lock(av_mutex);
			audio_buffer.insert(audio_buffer.end(), audio_merge_buffer.begin(), audio_merge_buffer.begin() + out_bytes);
		}

		//Output::Debug("Put {} {}", out_bytes, m_audio_buffer.size());

		av_frame_unref(audio_frame);

		got = true;

		if (ret < 0)
			return ret;
	}

	return 0;
}

int VideoDecoder::DecodeVideoPacket(AVPacket* paquet, bool &got)
{
	int ret = 0;
	size_t unpadded_linesize;
	size_t sample_size;

	got = false;

	ret = avcodec_send_packet(video_decoder_ctx, paquet);
	if (ret < 0)
	{
		if (ret == AVERROR_EOF)
			return ret;

		Output::Warning("FFMPEG: ERROR: Error submitting a packet for decoding ({})", av_error_to_str(ret));
		return ret;
	}

	while (ret >= 0)
	{
		ret = avcodec_receive_frame(video_decoder_ctx, in_frame);

		if (ret < 0)
		{
			/* those two return values are special and mean there is no output */
			/* frame available, but there were no errors during decoding */
			if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
				return 0;

			Output::Warning("FFMPEG: Error during decoding ({})", av_error_to_str(ret));
			return ret;
		}

		UpdateVideoStream();

		uint8_t *out[] = {video_pixel_data.data()};
		int lines[] = {video_pitch};

		sws_scale(video_cvt,
				in_frame->data, in_frame->linesize, 0, in_frame->height,
				out, lines);

		BitmapRef texture = Bitmap::Create(video_pixel_data.data(), video_width, video_height, video_pitch, format_R8G8B8A8_n().format());
		BitmapRef frame = Bitmap::Create(*texture, texture->GetRect(), false);

		double time_base = av_q2d(video_stream->time_base);
		double pts = (in_frame->pts == AV_NOPTS_VALUE) ? in_frame->pkt_dts : in_frame->pts;
		double video_time = pts * time_base;

		{
			const std::lock_guard<std::mutex> lock(av_mutex);
			video_buffer.push_back({frame, video_time});
			Output::Debug("FRAME {} {}", video_time, video_buffer.front().time);
		}

		av_frame_unref(in_frame);

		got = true;

		if (ret < 0)
			return ret;
	}

	return 0;
}

using namespace std::chrono_literals;

void VideoDecoder::ThreadFunction() {
	while (!IsFinished()) {
        bool needs_more_data = false;

        {
            const std::lock_guard<std::mutex> lock(av_mutex);
            // buffer ca. 1 second of audio or 30 frames of video
            if (audio_packet_queue.size() < 50 || video_packet_queue.size() < 30) {
                needs_more_data = true;
            }
        }

        if (needs_more_data) {
            ReadPackets();
        }

        ProcessPackets();

        std::this_thread::sleep_for(1ms);
    }
}

VideoDecoder::~VideoDecoder() {
	if (swr_ctx) {
		swr_free(&swr_ctx);
	}

	if (video_cvt) {
		sws_freeContext(video_cvt);
	}

	if (in_frame) {
		av_frame_free(&in_frame);
	}

	if (audio_frame) {
		av_frame_free(&audio_frame);
	}

	for (auto& packet: audio_packet_queue) {
        av_packet_unref(&packet);
    }

	for (auto& packet: video_packet_queue) {
        av_packet_unref(&packet);
    }

	if (audio_decoder_ctx) {
		avcodec_free_context(&audio_decoder_ctx);
	}

	if (video_decoder_ctx) {
		avcodec_free_context(&video_decoder_ctx);
	}

	if (input_ctx) {
		avformat_close_input(&input_ctx);
	}

	av_thread.join();
}

bool VideoDecoder::Seek(std::streamoff offset, std::ios_base::seekdir origin) {
	return true;
}

bool VideoDecoder::Open(Filesystem_Stream::InputStream stream) {
	this->stream = std::move(stream);

	AVDictionary *options = nullptr;
	int ret;

	in_buffer = (uint8_t *)av_malloc(AUDIO_INBUF_SIZE);
	if (!in_buffer)
	{
		Output::Warning("FFMPEG: Out of memory");
		return false;
	}

	avio_in = avio_alloc_context(in_buffer,
								 AUDIO_INBUF_SIZE,
								 0,
								 &this->stream,
								 vio_read_func,
								 nullptr,
								 vio_seek_func);
	if (!avio_in)
	{
		Output::Warning("FFMPEG: Unhandled file format");
		return false;
	}

	input_ctx = avformat_alloc_context();
	input_ctx->pb = avio_in;

	// open the input file
	ret = avformat_open_input(&input_ctx, "file:///easyrpg", nullptr, &options);
	av_dict_free(&options);

	if (ret != 0)
	{
		Output::Warning("Cannot open input file");
		return false;
	}

	if (avformat_find_stream_info(input_ctx, NULL) < 0)
	{
		Output::Warning("Cannot find input stream information");
		return false;
	}

	ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &video_decoder, 0);
	if (ret < 0)
	{
		Output::Warning("No suitable video stream in the input file");
		return false;
	}

	video_stream_index = ret;
	video_stream = input_ctx->streams[ret];

	ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &audio_decoder, 0);
	if (ret >= 0)
	{
		audio_stream_index = ret;
		audio_stream = input_ctx->streams[ret];
	}

	if (!(video_decoder_ctx = avcodec_alloc_context3(video_decoder)))
	{
		Output::Warning("No enough memory to initialise the video decoder!");
		return false;
	}

	if (!video_stream->codecpar)
	{
		Output::Warning("FFMPEG: codec parameters aren't recognised");
		return false;
	}

	if (audio_stream && !(audio_decoder_ctx = avcodec_alloc_context3(audio_decoder)))
	{
		Output::Warning("No enough memory to initialise the audio decoder!");
		return false;
	}

	if (audio_stream && !audio_stream->codecpar)
	{
		Output::Warning("FFMPEG: codec parameters aren't recognised");
		return false;
	}

	if (avcodec_parameters_to_context(video_decoder_ctx, video_stream->codecpar) < 0)
	{
		Output::Warning("Error of avcodec_parameters_to_context (video)");
		return false;
	}

	if (audio_stream && avcodec_parameters_to_context(audio_decoder_ctx, audio_stream->codecpar) < 0)
	{
		Output::Warning("Error of avcodec_parameters_to_context (audio)");
		return false;
	}

	video_decoder_ctx->sw_pix_fmt = AV_PIX_FMT_RGBA;
	video_decoder_ctx->opaque = this;

	if (audio_decoder_ctx)
		audio_decoder_ctx->opaque = this;

	ret = avcodec_open2(video_decoder_ctx, video_decoder, nullptr);
	if (ret < 0)
	{
		Output::Warning("Failed avcodec_open2 (video)");
		return false;
	}

	if (audio_stream)
	{
		ret = avcodec_open2(audio_decoder_ctx, audio_decoder, nullptr);
		if (ret < 0)
		{
			Output::Warning("Failed avcodec_open2 (audio)");
			return false;
		}
	}

	if (!(in_frame = av_frame_alloc()))
	{
		Output::Warning("Can not alloc frame");
		return false;
	}

	if (audio_stream && !(audio_frame = av_frame_alloc()))
	{
		Output::Warning("Can not alloc audio frame");
		return false;
	}

	video_dst_format = AV_PIX_FMT_RGBA;
	video_width = video_stream->codecpar->width;
	video_height = video_stream->codecpar->height;

	video_pixel_data.resize(video_height * video_pitch);
	memset(video_pixel_data.data(), 0, video_pixel_data.size());

	UpdateVideoStream();
	UpdateAudioStream();

	//av_dump_format(m_inputCtx, m_streamVideo, std::string(stream.GetName()).c_str(), 0);

	// if (m_streamAudio >= 0)
	//     av_dump_format(m_inputCtx, m_streamAudio, video_path.c_str(), 0);

	at_end = false;

	av_thread = std::thread(&VideoDecoder::ThreadFunction, this);

	return true;
}

bool VideoDecoder::IsFinished() const {
	// video buffer contains the last frame
    return at_end && audio_flushed && video_flushed
           && video_buffer.size() <= 1 && audio_buffer.empty();
}

void VideoDecoder::GetFormat(int& frequency, AudioDecoder::Format& format, int& channels) const {
	frequency = audio_dst_freq;
	format = audio_dst_format_decoder;
	channels = audio_dst_channels;
}

bool VideoDecoder::SetFormat(int frequency, AudioDecoder::Format format, int channels) {
	audio_dst_freq = frequency;
	audio_dst_format_decoder = format;
	audio_dst_channels = channels;

	return true;
}

int VideoDecoder::GetTicks() const {
	return 0;
}

int VideoDecoder::FillBuffer(uint8_t* buffer, int length) {
	const std::lock_guard<std::mutex> lock(av_mutex);

	memset(buffer, '\0', length);

	int to_copy = std::min<int>(length, audio_buffer.size());

	if (playback_time == 0.0 && audio_buffer.empty() && audio_stream) {
		// Decoder just started and has no data yet
		return length;
	}

	if (audio_stream) {
		memcpy(buffer, audio_buffer.data(), to_copy);
		audio_buffer.erase(audio_buffer.begin(), audio_buffer.begin() + to_copy);
	} else {
		// Video has no audio channel
		to_copy = length;
	}

	//Output::Debug("Took {} {} {}", m_audio_buffer.size(), to_copy, length);

	auto it = video_buffer.begin();

	while (video_buffer.size() > 1 && it != video_buffer.end()) {
		// Throw away outdated frames
		if (it->time < playback_time) {
			//Output::Debug("Deleting {}", it->time);
			it = video_buffer.erase(it);
		} else {
			break;
		}
	}

	playback_time += (to_copy / (double)(AudioDecoder::GetSamplesizeForFormat(audio_dst_format_decoder) * audio_dst_channels)) / audio_dst_freq;

	Output::Debug("PLAYBACK {}", playback_time);

	return to_copy;
}

std::unique_ptr<VideoDecoder::AudioComponent> VideoDecoder::CreateAudioDecoder() {
	return std::make_unique<AudioComponent>(this);
}

BitmapRef VideoDecoder::GetVideoFrame() const {
	// FIXME: Implement aspect ration keeping!

	const std::lock_guard<std::mutex> lock(av_mutex);

	if (video_buffer.empty()) {
		return {};
	}

	Output::Debug("GetFrame {} {}", video_buffer.begin()->time, playback_time);

	return video_buffer.begin()->frame;
}
