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

#include <libavutil/channel_layout.h>
#include <libavutil/pixfmt.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <string>

#include "video_decoder.h"
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

	if (seek_type == AVSEEK_SIZE) {
		return f->GetSize();
	}

	f->seekg(offset, Filesystem_Stream::CSeekdirToCppSeekdir(seek_type));

	return f->tellg();
}

void VideoDecoder::readPackets() {
    const std::lock_guard<std::mutex> lock(av_mutex);

    AVPacket pkt;
    if (av_read_frame(m_inputCtx, &pkt) >= 0) {
        if (pkt.stream_index == m_streamAudio) {
            m_audio_packet_queue.push_back(pkt);
        } else if (pkt.stream_index == m_streamVideo) {
            m_video_packet_queue.push_back(pkt);
        } else {
            av_packet_unref(&pkt);
        }
    } else {
        m_atEnd = true;
    }
}

void VideoDecoder::processPackets() {
    const std::lock_guard<std::mutex> lock(av_mutex);

    // Decode Audio if buffer is low
    while (!m_audio_packet_queue.empty() && m_audio_buffer.size() < 20480) {
        AVPacket pkt = m_audio_packet_queue.front();
        m_audio_packet_queue.pop_front();

        bool got;
        m_paquet = pkt; // TODO: Make param
        decode_audio_packet(got);
        av_packet_unref(&pkt);
    }

    // Decode video if not enough frames
    while (!m_video_packet_queue.empty() && frames.size() < 10) {
        AVPacket pkt = m_video_packet_queue.front();
        m_video_packet_queue.pop_front();

        double time_base = av_q2d(m_video->time_base);
        double pts = (pkt.pts == AV_NOPTS_VALUE) ? pkt.dts : pkt.pts;
        double video_time = pts * time_base;

        bool got;
        decode_video_packet(pkt, got, video_time);
        av_packet_unref(&pkt);
    }
}

bool VideoDecoder::updateAudioStream()
{
	if(!m_audio || !m_audio->codecpar)
		return true; // No audio - no actions!

	enum AVSampleFormat sfmt = (enum AVSampleFormat)m_audio->codecpar->format;
	int srate = m_audio->codecpar->sample_rate;

#if defined(AVCODEC_NEW_CHANNEL_LAYOUT)
	int channels = m_audio->codecpar->ch_layout.nb_channels;
#else
	int channels = m_audio->codecpar->channels;
#endif

#if defined(AVCODEC_NEW_CHANNEL_LAYOUT)
	AVChannelLayout layout;
#else
	int layout;
#endif

	if(srate == 0 || channels == 0)
		return false;

	if(sfmt != m_sfmt || srate != m_srate || channels != m_schannels/* || !m_audio_cvt*/)
	{
		m_planar = false;

		switch(sfmt)
		{
		case AV_SAMPLE_FMT_U8P:
			m_planar = true;
			m_dst_sample_fmt = AV_SAMPLE_FMT_U8;
			/*fallthrough*/
		case AV_SAMPLE_FMT_U8:
			//m_dec_fmt = AudioDecoder::Format::U8;
			break;

		case AV_SAMPLE_FMT_S16P:
			m_planar = true;
			m_dst_sample_fmt = AV_SAMPLE_FMT_S16;
			/*fallthrough*/
		case AV_SAMPLE_FMT_S16:
			//m_dec_fmt = AudioDecoder::Format::S16;
			break;

		case AV_SAMPLE_FMT_S32P:
			m_planar = true;
			m_dst_sample_fmt = AV_SAMPLE_FMT_S32;
			/*fallthrough*/
		case AV_SAMPLE_FMT_S32:
			//m_dec_fmt = AudioDecoder::Format::S32;
			break;

		case AV_SAMPLE_FMT_FLTP:
			m_planar = true;
			m_dst_sample_fmt = AV_SAMPLE_FMT_FLT;
			/*fallthrough*/
		case AV_SAMPLE_FMT_FLT:
			//m_dec_fmt = AudioDecoder::Format::F32;
			break;

		default:
			return -1; /* Unsupported audio format */
		}

		//m_dec_fmt = AudioDecoder::Format::S16;

		/*if(m_audio_cvt)
		{
			SDL_FreeAudioStream(m_audio_cvt);
			m_audio_cvt = NULL;
		}*/

		m_merge_buffer.clear();

		if(m_swr_ctx)
		{
			swr_free(&m_swr_ctx);
			m_swr_ctx = nullptr;
		}

		//m_audio_cvt = SDL_NewAudioStream(fmt, (Uint8)channels, srate,
		//                                 m_dstSpec.format, m_dstSpec.channels, m_dstSpec.freq);
		//if(!m_audio_cvt)
		//    return false;

		if(m_planar)
		{
			m_swr_ctx = swr_alloc();
#if defined(AVCODEC_NEW_CHANNEL_LAYOUT)
			layout = m_audio->codecpar->ch_layout;
#else
			layout = m_audio->codecpar->channel_layout;
#endif

#if defined(AVCODEC_NEW_CHANNEL_LAYOUT)
			if(layout.u.mask == 0)
			{
				layout.order = AV_CHANNEL_ORDER_NATIVE;
				layout.nb_channels = channels;

				if(channels > 2)
					layout.u.mask = AV_CH_LAYOUT_SURROUND;
				else if(channels == 2)
					layout.u.mask = AV_CH_LAYOUT_STEREO;
				else if(channels == 1)
					layout.u.mask = AV_CH_LAYOUT_MONO;
			}


			av_opt_set_chlayout(m_swr_ctx, "in_chlayout",  &layout, 0);

			AVChannelLayout out_layout = (m_dec_channels == 1) ?
				(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO :
				(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;

			av_opt_set_chlayout(m_swr_ctx, "out_chlayout", &out_layout, 0);
#else
			if(layout == 0)
			{
				if(channels > 2)
					layout = AV_CH_LAYOUT_SURROUND;
				else if(channels == 2)
					layout = AV_CH_LAYOUT_STEREO;
				else if(channels == 1)
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
			av_opt_set_int(m_swr_ctx, "in_sample_rate", srate, 0);
			av_opt_set_int(m_swr_ctx, "out_sample_rate", m_dec_freq, 0);
			av_opt_set_sample_fmt(m_swr_ctx, "in_sample_fmt",  sfmt, 0);
			av_opt_set_sample_fmt(m_swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
			swr_init(m_swr_ctx);

#if defined(AVCODEC_NEW_CHANNEL_LAYOUT)
			//av_channel_layout_uninit(&layout);
#endif

			m_merge_buffer.resize(channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * 4096);
		}

		m_sfmt = sfmt;
		m_srate = srate;
		m_schannels = channels;
	}

	return true;
}

bool VideoDecoder::updateVideoStream()
{
	if(!m_video || !m_video->codecpar)
		return true; // No video - no actions!

	AVPixelFormat pixfmt = (AVPixelFormat)m_video->codecpar->format;
	int w = m_video->codecpar->width;
	int h = m_video->codecpar->height;

	if(pixfmt == AV_PIX_FMT_NONE || w == 0 || h == 0)
		return false;

	if(w != m_dst_w || h != m_dst_h || pixfmt != m_dst_colour || !m_video_cvt)
	{
		if(m_video_cvt)
		{
			sws_freeContext(m_video_cvt);
			m_video_cvt = nullptr;
		}

		m_video_cvt = sws_getContext(w, h, pixfmt, m_dst_w, m_dst_h, m_dst_colour, 0, 0, 0, 0);
		if(!m_video_cvt)
			return false;

		//SDL_LockMutex(m_textureMutex);
		uint8_t *dst_data[4];
		int dst_line_sizes[4];

		int data_size = av_image_fill_arrays(dst_data, dst_line_sizes, nullptr, m_dst_colour, w, h, 8);
		m_texture_pitch = dst_line_sizes[0];
		m_texturePixelData.resize(data_size);

		//SDL_UnlockMutex(m_textureMutex);
	}

	return false;
}

int VideoDecoder::decode_audio_packet(bool &got)
{
	int ret = 0;
	size_t unpadded_linesize;
	size_t sample_size;

	got = false;

	ret = avcodec_send_packet(m_decoderAudioCtx, &m_paquet);
	if(ret < 0)
	{
		if(ret == AVERROR_EOF)
			return ret;

		Output::Warning("FFMPEG: ERROR: Error submitting a packet for decoding ({})", av_error_to_str(ret));
		return ret;
	}

	while(ret >= 0)
	{
		ret = avcodec_receive_frame(m_decoderAudioCtx, m_audio_frame);

		if(ret < 0)
		{
			/* those two return values are special and mean there is no output */
			/* frame available, but there were no errors during decoding */
			if(ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
				return 0;

			Output::Warning("FFMPEG: Error during decoding ({})", av_error_to_str(ret));
			return ret;
		}

		updateAudioStream();

		if(m_planar)
		{
			sample_size = av_get_bytes_per_sample((enum AVSampleFormat)m_audio_frame->format);
			unpadded_linesize = m_dec_freq * AudioDecoder::GetSamplesizeForFormat(m_dec_fmt) * m_dec_channels;

			if(unpadded_linesize > m_merge_buffer.size())
				m_merge_buffer.resize(unpadded_linesize);

			uint8_t *out = m_merge_buffer.data();

			auto out_samples = swr_convert(m_swr_ctx, &out, m_merge_buffer.size() / m_dec_channels,
						(const uint8_t**)m_audio_frame->extended_data, m_audio_frame->nb_samples);

			auto out_bytes = out_samples * AudioDecoder::GetSamplesizeForFormat(m_dec_fmt) * m_dec_channels;

			// if (m_paquet.pts != AV_NOPTS_VALUE)
			//     m_time = (double)m_paquet.pts * av_q2d(m_audio->time_base);
			// else
			//     m_time = -1.0;

			/*if(SDL_AudioStreamPut(m_audio_cvt, m_merge_buffer.data(), unpadded_linesize) < 0)
			{
				Output::Warning("FFMPEG: Failed to put audio stream");
				return -1;
			}*/
			m_audio_buffer.insert(m_audio_buffer.end(), m_merge_buffer.begin(), m_merge_buffer.begin() + out_bytes);
			//Output::Debug("Put {} {}", out_bytes, m_audio_buffer.size());
		}
		else
		{
			unpadded_linesize = m_audio_frame->nb_samples * av_get_bytes_per_sample((enum AVSampleFormat)m_audio_frame->format);

			m_audio_buffer.insert(m_audio_buffer.end(), m_audio_frame->extended_data[0], m_audio_frame->extended_data[0] + unpadded_linesize);

			/*if(SDL_AudioStreamPut(m_audio_cvt, m_audio_frame->extended_data[0], unpadded_linesize) < 0)
			{
				Output::Warning("FFMPEG: Failed to put audio stream");
				return -1;
			}*/
		}

		av_frame_unref(m_audio_frame);

		got = true;

		if(ret < 0)
			return ret;
	}

	return 0;
}

int VideoDecoder::decode_video_packet(AVPacket &paquet, bool &got, double video_time)
{
	int ret = 0;
	size_t unpadded_linesize;
	size_t sample_size;

	got = false;

	ret = avcodec_send_packet(m_decoderVideoCtx, &paquet);
	if(ret < 0)
	{
		if(ret == AVERROR_EOF)
			return ret;

		Output::Warning("FFMPEG: ERROR: Error submitting a packet for decoding ({})", av_error_to_str(ret));
		return ret;
	}

	while(ret >= 0)
	{
		ret = avcodec_receive_frame(m_decoderVideoCtx, in_frame);

		if(ret < 0)
		{
			/* those two return values are special and mean there is no output */
			/* frame available, but there were no errors during decoding */
			if(ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
				return 0;

			Output::Warning("FFMPEG: Error during decoding ({})", av_error_to_str(ret));
			return ret;
		}

		updateVideoStream();

		//SDL_LockMutex(m_textureMutex);

		uint8_t *out[] = {m_texturePixelData.data()};
		int lines[] = {m_texture_pitch};

		sws_scale(m_video_cvt,
				in_frame->data, in_frame->linesize, 0, in_frame->height,
				out, lines);

		m_texture_w = m_dst_w;
        m_texture_h = m_dst_h;

		BitmapRef texture = Bitmap::Create(m_texturePixelData.data(), m_texture_w, m_texture_h, m_texture_pitch, format_B8G8R8A8_n().format());
		BitmapRef frame = Bitmap::Create(*texture, texture->GetRect(), false);
		frames.push_back({frame, video_time});
		Output::Debug("FRAME {} {}", video_time, frames.front().time);

		if(m_hasVideoFrame)
		{
			m_hasVideoFrame = false;
		}

		//SDL_UnlockMutex(m_textureMutex);
		m_hasVideoFrame = true;

		double fps = av_q2d(av_guess_frame_rate(m_inputCtx, m_video, in_frame));
		m_timeNextFrame = m_time + (1.0 / fps);

		av_frame_unref(in_frame);

		got = true;

		if (ret < 0)
			return ret;
	}

	return 0;
}

using namespace std::chrono_literals;

void VideoDecoder::ThreadFunction() {
	// TODO Terminate
	for (;;) {
        bool needs_more_data = false;

        {
            const std::lock_guard<std::mutex> lock(av_mutex);
            // buffer ca. 1 second of audio or 30 frames of video
            if (m_audio_packet_queue.size() < 50 || m_video_packet_queue.size() < 30) {
                needs_more_data = true;
            }
        }

        if (needs_more_data) {
            readPackets();
        }

        processPackets();

        std::this_thread::sleep_for(1ms);
    }
}

VideoDecoder::VideoDecoder()
{
	m_paquet = {};
}

VideoDecoder::~VideoDecoder()
{
	//close();
}

bool VideoDecoder::Seek(std::streamoff offset, std::ios_base::seekdir origin) {
	return true;
}

bool VideoDecoder::Open(Filesystem_Stream::InputStream stream) {
	this->stream = std::move(stream);

	AVDictionary *options = nullptr;
	int ret;
	//close();

	in_buffer = (uint8_t *)av_malloc(AUDIO_INBUF_SIZE);
	in_buffer_size = AUDIO_INBUF_SIZE;
	if(!in_buffer)
	{
		Output::Warning("FFMPEG: Out of memory");
		//close();
		return false;
	}

	avio_in = avio_alloc_context(in_buffer,
								 in_buffer_size,
								 0,
								 &this->stream,
								 vio_read_func,
								 nullptr,
								 vio_seek_func);
	if(!avio_in)
	{
		//close();
		Output::Warning("FFMPEG: Unhandled file format");
		return false;
	}

	m_inputCtx = avformat_alloc_context();
	m_inputCtx->pb = avio_in;
	static char proto[] = "file:///easyrpg";
	m_inputCtx->url = proto;

	// open the input file
	ret = avformat_open_input(&m_inputCtx, nullptr, nullptr, &options);
	av_dict_free(&options);

	if(ret != 0)
	{
		Output::Warning("Cannot open input file");
		//close();
		return false;
	}

	if(avformat_find_stream_info(m_inputCtx, NULL) < 0)
	{
		Output::Warning("Cannot find input stream information");
		//close();
		return false;
	}

	ret = av_find_best_stream(m_inputCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &m_decoderVideo, 0);
	if(ret < 0)
	{
		Output::Warning("No suitable video stream in the input file");
		//close();
		return false;
	}

	m_streamVideo = ret;
	m_video = m_inputCtx->streams[ret];

	ret = av_find_best_stream(m_inputCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &m_decoderAudio, 0);
	if(ret >= 0)
	{
		m_streamAudio = ret;
		m_audio = m_inputCtx->streams[ret];
	}

	if(!(m_decoderVideoCtx = avcodec_alloc_context3(m_decoderVideo)))
	{
		Output::Warning("No enough memory to initialise the video decoder!");
		//close();
		return false;
	}

	if(!m_video->codecpar)
	{
		Output::Warning("FFMPEG: codec parameters aren't recognised");
		//close();
		return false;
	}

	if(m_audio && !(m_decoderAudioCtx = avcodec_alloc_context3(m_decoderAudio)))
	{
		Output::Warning("No enough memory to initialise the audio decoder!");
		//close();
		return false;
	}

	if(m_audio && !m_audio->codecpar)
	{
		Output::Warning("FFMPEG: codec parameters aren't recognised");
		//close();
		return false;
	}

	if(avcodec_parameters_to_context(m_decoderVideoCtx, m_video->codecpar) < 0)
	{
		Output::Warning("Error of avcodec_parameters_to_context (video)");
		//close();
		return false;
	}

	if(m_audio && avcodec_parameters_to_context(m_decoderAudioCtx, m_audio->codecpar) < 0)
	{
		Output::Warning("Error of avcodec_parameters_to_context (audio)");
		//close();
		return false;
	}

	m_decoderVideoCtx->sw_pix_fmt = AV_PIX_FMT_RGBA;
	m_decoderVideoCtx->opaque = this;

	if(m_decoderAudioCtx)
		m_decoderAudioCtx->opaque = this;

	ret = avcodec_open2(m_decoderVideoCtx, m_decoderVideo, nullptr);
	if(ret < 0)
	{
		Output::Warning("Failed avcodec_open2 (video)");
		//close();
		return false;
	}

	if(m_audio)
	{
		ret = avcodec_open2(m_decoderAudioCtx, m_decoderAudio, nullptr);
		if(ret < 0)
		{
			Output::Warning("Failed avcodec_open2 (audio)");
			//close();
			return false;
		}
	}

	if(!(in_frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc()))
	{
		Output::Warning("Can not alloc frame");
		//close();
		return false;
	}

	if(m_audio && !(m_audio_frame = av_frame_alloc()))
	{
		Output::Warning("Can not alloc audio frame");
		//close();
		return false;
	}

	//m_freesrc = freesrc;

	m_texture_colour = AV_PIX_FMT_RGBA;
	m_texture_w = 0;
	m_texture_h = 0;

	m_dst_colour = AV_PIX_FMT_RGBA;
	m_dst_w = m_video->codecpar->width;
	m_dst_h = m_video->codecpar->height;

	m_texturePixelData.resize(m_dst_h * m_texture_pitch);
	memset(m_texturePixelData.data(), 0, m_texturePixelData.size());

	updateVideoStream();
	updateAudioStream();

	//av_dump_format(m_inputCtx, m_streamVideo, std::string(stream.GetName()).c_str(), 0);

	// if(m_streamAudio >= 0)
	//     av_dump_format(m_inputCtx, m_streamAudio, video_path.c_str(), 0);

	m_time = 0.0;
	m_atEnd = false;

	//m_textureMutex = SDL_CreateMutex();

	av_thread = std::thread(&VideoDecoder::ThreadFunction, this);

	return true;
}

bool VideoDecoder::IsFinished() const
{
	return m_atEnd;
}

void VideoDecoder::GetFormat(int& frequency, AudioDecoder::Format& format, int& channels) const {
	frequency = m_dec_freq;
	format = m_dec_fmt;
	channels = m_dec_channels;
}

bool VideoDecoder::SetFormat(int frequency, AudioDecoder::Format format, int channels) {
	m_dec_freq = frequency;
	m_dec_fmt = format;
	m_dec_channels = channels;

	return true;
}

int VideoDecoder::GetTicks() const {
	return 0;
}

BitmapRef VideoDecoder::getVideoFrame()
{
	//SDL_LockMutex(m_textureMutex);

	// FIXME: Implement aspect ration keeping!

	//SDL_UnlockMutex(m_textureMutex);

	const std::lock_guard<std::mutex> lock(av_mutex);

	if (frames.empty()) {
		return {};
	}

	Output::Debug("GetFrame {} {}", frames.begin()->time, m_playback_time);

	return frames.begin()->frame;
}

int VideoDecoder::FillBuffer(uint8_t* buffer, int length) {
	const std::lock_guard<std::mutex> lock(av_mutex);

	memset(buffer, '\0', length);

	int to_copy = std::min<int>(length, m_audio_buffer.size());

	memcpy(buffer, m_audio_buffer.data(), to_copy);
	//std::ofstream o("/tmp/out.wav", std::ios_base::out | std::ios_base::app);
	//o.write((const char*)stream, filled);

	m_audio_buffer.erase(m_audio_buffer.begin(), m_audio_buffer.begin() + to_copy);

	//Output::Debug("Took {} {} {}", m_audio_buffer.size(), to_copy, length);

	//m_audio_buffer.clear();
	//Output::Debug("Took {} {} {}", filled, m_audio_buffer.size(), (filled / (double)(GetSamplesizeForFormat(m_dec_fmt) * m_dec_channels)) / m_dec_freq);

	auto it = frames.begin();

	while (frames.size() > 1 && it != frames.end()) {
		// Throw away outdated frames
		if (it->time < m_playback_time) {
			//Output::Debug("Deleting {}", it->time);
			it = frames.erase(it);
		} else {
			break;
		}
	}

	m_playback_time += (to_copy / (double)(GetSamplesizeForFormat(m_dec_fmt) * m_dec_channels)) / m_srate;

	Output::Debug("PLAYBACK {}", m_playback_time);

	// FIXME: Decoder is deleted when "warming up"
	return to_copy <= 0 ? 1 : to_copy;
}
