#include "audio_output.h"

int init_audio_output(PlayerSession* session) {
    if (!session) return PLAYER_ERR_NULLPTR;
    if (!session->has_audio) return PLAYER_ERR_OK;
    if (!session->audio_decoder) return PLAYER_ERR_NULLPTR;
    SDL_AudioSpec sdl_spec;
    sdl_spec.freq = session->audio_decoder->sample_rate;
    sdl_spec.format = convert_to_sdl_format(session->audio_decoder->sample_fmt);
    if (!sdl_spec.format) {
        const char* tmp = av_get_sample_fmt_name(session->audio_decoder->sample_fmt);
        av_log(NULL, AV_LOG_FATAL, "Unknown sample format: %s (%i)\n", tmp ? tmp : "", session->audio_decoder->sample_fmt);
        return PLAYER_ERR_UNKNOWN_AUDIO_SAMPLE_FMT;
    }
    sdl_spec.channels = session->audio_decoder->ch_layout.nb_channels;
    sdl_spec.samples = session->audio_decoder->sample_rate / 100;
    sdl_spec.callback = SDL_audio_callback;
    sdl_spec.userdata = session;
    memcpy(&session->sdl_spec, &sdl_spec, sizeof(SDL_AudioSpec));
    session->device_id = SDL_OpenAudioDevice(NULL, 0, &sdl_spec, &session->sdl_spec, 0);
    if (!session->device_id) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open audio device: %s\n", SDL_GetError());
        return PLAYER_ERR_SDL;
    }
    enum AVSampleFormat target_format = convert_to_sdl_supported_format(session->audio_decoder->sample_fmt);
    int re = 0;
    if (re = get_sdl_channel_layout(session->audio_decoder->ch_layout.nb_channels, &session->output_channel_layout)) {
        return re;
    }
    if (re = swr_alloc_set_opts2(&session->swrac, &session->output_channel_layout, target_format, session->sdl_spec.freq, &session->audio_decoder->ch_layout, session->audio_decoder->sample_fmt, session->audio_decoder->sample_rate, 0, NULL)) {
        av_log(NULL, AV_LOG_FATAL, "Failed to allocate resample context: %s (%i)\n", av_err2str(re), re);
        return re;
    }
    if (!session->swrac) {
        av_log(NULL, AV_LOG_FATAL, "Failed to allocate resample context.\n");
        return PLAYER_ERR_OOM;
    }
    if ((re = swr_init(session->swrac)) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to initialize resample context: %s (%i)\n", av_err2str(re), re);
        return re;
    }
    if (!(session->buffer = av_audio_fifo_alloc(target_format, session->audio_decoder->ch_layout.nb_channels, 1))) {
        av_log(NULL, AV_LOG_FATAL, "Failed to allocate audio buffer.\n");
        return PLAYER_ERR_OOM;
    }
    session->target_format = target_format;
    session->target_format_pbytes = av_get_bytes_per_sample(target_format);
    return PLAYER_ERR_OK;
}

enum AVSampleFormat convert_to_sdl_supported_format(enum AVSampleFormat fmt) {
    switch (fmt) {
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_FLTP:
        case AV_SAMPLE_FMT_DBLP:
            return AV_SAMPLE_FMT_FLT;
        case AV_SAMPLE_FMT_U8P:
            return AV_SAMPLE_FMT_U8;
        case AV_SAMPLE_FMT_S16P:
            return AV_SAMPLE_FMT_S16;
        case AV_SAMPLE_FMT_S32P:
        case AV_SAMPLE_FMT_S64:
        case AV_SAMPLE_FMT_S64P:
            return AV_SAMPLE_FMT_S32;
        default:
            return fmt;
    }
}

SDL_AudioFormat convert_to_sdl_format(enum AVSampleFormat fmt) {
    fmt = convert_to_sdl_supported_format(fmt);
    switch (fmt) {
        case AV_SAMPLE_FMT_U8:
            return AUDIO_U8;
        case AV_SAMPLE_FMT_S16:
            return AUDIO_S16SYS;
        case AV_SAMPLE_FMT_S32:
            return AUDIO_S32SYS;
        case AV_SAMPLE_FMT_FLT:
            return AUDIO_F32SYS;
        default:
            return 0;
    }
}

void SDL_audio_callback(void* userdata, uint8_t* stream, int len) {
    PlayerSession* session = (PlayerSession*)userdata;
    if (!session) return;
    DWORD re = WaitForSingleObject(session->mutex, 10);
    if (re != WAIT_OBJECT_0) {
        // 无法获取Mutex所有权，填充空白数据
        memset(stream, 0, len);
        return;
    }
    int samples_need = len / session->target_format_pbytes / session->audio_decoder->ch_layout.nb_channels;
    int buffer_size = session->sdl_spec.freq / 5;
    if (av_audio_fifo_size(session->buffer) == 0) {
        // 缓冲区为空，填充空白数据
        memset(stream, 0, len);
    } else {
        int writed = av_audio_fifo_read(session->buffer, (void**)&stream, samples_need);
        if (writed > 0) {
            AVRational base = {1, session->sdl_spec.freq};
            session->pts += av_rescale_q_rnd(writed, base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        }
        if (writed < 0) {
            memset(stream, 0, len);
        } else if (writed < samples_need) {
            size_t len = ((size_t)samples_need - writed) * session->target_format_pbytes, alen = (size_t)writed * session->target_format_pbytes;
            // 不足的区域用空白数据填充
            memset(stream + alen, 0, len);
        }
    }
    ReleaseMutex(session->mutex);
}

int get_sdl_channel_layout(int channels, AVChannelLayout* channel_layout) {
    if (!channel_layout) return PLAYER_ERR_OK;
    switch (channels) {
        case 2:
            return av_channel_layout_from_string(channel_layout, "FL+FR");
        case 3:
            return av_channel_layout_from_string(channel_layout, "FL+FR+LFE");
        case 4:
            return av_channel_layout_from_string(channel_layout, "FL+FR+BL+BR");
        case 5:
            return av_channel_layout_from_string(channel_layout, "FL+FR+FC+BL+BR");
        case 6:
            return av_channel_layout_from_string(channel_layout, "FL+FR+FC+LFE+SL+SR");
        case 7:
            return av_channel_layout_from_string(channel_layout, "FL+FR+FC+LFE+BC+SL+SR");
        case 8:
            return av_channel_layout_from_string(channel_layout, "FL+FR+FC+LFE+BL+BR+SL+SR");
        default:
            av_channel_layout_default(channel_layout, channels);
            return PLAYER_ERR_OK;
    }
}
