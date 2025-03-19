#include "decode.h"

int open_audio_decoder(PlayerSession* session) {
    if (!session) return PLAYER_ERR_NULLPTR;
    if (!session->has_audio) return PLAYER_ERR_OK;
    if (!session->fmt || !session->audio_input_stream) return PLAYER_ERR_NULLPTR;
    session->audio_codec = avcodec_find_decoder(session->audio_input_stream->codecpar->codec_id);
    if (!session->audio_codec) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find audio decoder.\n");
        return PLAYER_ERR_NO_STREAM_OR_DECODER;
    }
    session->audio_decoder = avcodec_alloc_context3(session->audio_codec);
    if (!session->audio_decoder) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate memory for audio decoder.\n");
        return PLAYER_ERR_OOM;
    }
    int re = 0;
    if ((re = avcodec_parameters_to_context(session->audio_decoder, session->audio_input_stream->codecpar)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy audio codec parameters from input stream: %s (%d)\n", av_err2str(re), re);
        return re;
    }
    if ((re = avcodec_open2(session->audio_decoder, session->audio_codec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open audio decoder (%s): %s (%d)\n", session->audio_codec->name, av_err2str(re), re);
        return re;
    }
    av_log(NULL, AV_LOG_VERBOSE, "Audio decoder opened: %s\n", session->audio_codec->name);
    return PLAYER_ERR_OK;
}

int open_video_decoder(PlayerSession* session) {
    if (!session) return PLAYER_ERR_NULLPTR;
    if (!session->has_video) return PLAYER_ERR_OK;
    if (!session->fmt || !session->video_input_stream) return PLAYER_ERR_NULLPTR;
    session->video_codec = avcodec_find_decoder(session->video_input_stream->codecpar->codec_id);
    if (!session->video_codec) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find video decoder.\n");
        return PLAYER_ERR_NO_STREAM_OR_DECODER;
    }
    session->video_decoder = avcodec_alloc_context3(session->video_codec);
    if (!session->video_decoder) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate memory for video decoder.\n");
        return PLAYER_ERR_OOM;
    }
    int re = 0;
    if ((re = avcodec_parameters_to_context(session->video_decoder, session->video_input_stream->codecpar)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy video codec parameters from input stream: %s (%d)\n", av_err2str(re), re);
        return re;
    }
    if ((re = avcodec_open2(session->video_decoder, session->video_codec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open video decoder (%s): %s (%d)\n", session->video_codec->name, av_err2str(re), re);
        return re;
    }
    av_log(NULL, AV_LOG_VERBOSE, "Video decoder opened: %s\n", session->video_codec->name);
    return PLAYER_ERR_OK;
}

int decode_audio_internal(PlayerSession* handle, char* writed, AVFrame* frame) {
    if (!handle || !writed || !frame) return PLAYER_ERR_NULLPTR;
    if (!handle->has_audio) return PLAYER_ERR_OK;
    if (!handle->audio_decoder) return PLAYER_ERR_NULLPTR;
    int re = 0;
    re = avcodec_receive_frame(handle->audio_decoder, frame);
    if (re >= 0) {
        if (handle->first_pts == INT64_MIN) {
            handle->first_pts = av_rescale_q_rnd(frame->pts, handle->audio_input_stream->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            av_log(NULL, AV_LOG_VERBOSE, "first_pts: %s\n", av_ts2timestr(handle->first_pts, &AV_TIME_BASE_Q));
        }
        if (handle->set_new_pts && frame->pts != AV_NOPTS_VALUE) {
            av_log(NULL, AV_LOG_VERBOSE, "pts: %s\n", av_ts2timestr(frame->pts, &handle->audio_input_stream->time_base));
            handle->pts = av_rescale_q_rnd(frame->pts, handle->audio_input_stream->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX) - handle->first_pts;
            handle->end_pts = handle->pts;
            handle->set_new_pts = 0;
        } else if (handle->set_new_pts) {
            av_log(NULL, AV_LOG_VERBOSE, "skip NOPTS frame.\n");
            // 跳过NOPTS的frame
            goto end;
        }
        re = audio_convert_samples_and_add_to_fifo(handle, frame, writed);
        goto end;
    } else if (re == AVERROR(EAGAIN)) {
        // 数据不够，继续读取
        re = PLAYER_ERR_OK;
        goto end;
    } else if (re == AVERROR_EOF) {
        handle->audio_is_eof = 1;
        re = PLAYER_ERR_OK;
        goto end;
    }
end:
    return re;
}

int decode_video_internal(PlayerSession* handle, char* writed, AVFrame* frame) {
    if (!handle || !writed || !frame) return PLAYER_ERR_NULLPTR;
    if (!handle->has_video) return PLAYER_ERR_OK;
    if (!handle->video_decoder) return PLAYER_ERR_NULLPTR;
    int re = 0;
    re = avcodec_receive_frame(handle->video_decoder, frame);
    if (re >= 0) {
        if (handle->video_first_pts == INT64_MIN) {
            handle->video_first_pts = av_rescale_q_rnd(frame->pts, handle->video_input_stream->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            av_log(NULL, AV_LOG_VERBOSE, "video_first_pts: %s\n", av_ts2timestr(handle->video_first_pts, &AV_TIME_BASE_Q));
        }
        if (handle->set_new_video_pts && frame->pts != AV_NOPTS_VALUE) {
            av_log(NULL, AV_LOG_VERBOSE, "video_pts: %s\n", av_ts2timestr(frame->pts, &handle->video_input_stream->time_base));
            handle->video_pts = av_rescale_q_rnd(frame->pts, handle->video_input_stream->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX) - handle->video_first_pts;
            handle->video_end_pts = handle->video_pts;
            handle->set_new_video_pts = 0;
        } else if (handle->set_new_video_pts) {
            av_log(NULL, AV_LOG_VERBOSE, "skip NOPTS frame.\n");
            // 跳过NOPTS的frame
            goto end;
        }
        re = video_add_to_fifo(handle, frame, writed);
        goto end;
    } else if (re == AVERROR(EAGAIN)) {
        // 数据不够，继续读取
        re = PLAYER_ERR_OK;
        goto end;
    } else if (re == AVERROR_EOF) {
        handle->video_is_eof = 1;
        re = PLAYER_ERR_OK;
        goto end;
    }
end:
    return re;
}

int audio_convert_samples_and_add_to_fifo(PlayerSession* handle, AVFrame* frame, char* writed) {
    if (!handle || !frame || !writed) return PLAYER_ERR_OK;
    if (!handle->has_audio) return PLAYER_ERR_OK;
    uint8_t** converted_input_samples = NULL;
    int re = PLAYER_ERR_OK;
    AVRational base = { 1, handle->audio_decoder->sample_rate }, target = { 1, handle->sdl_spec.freq };
    int samples = frame->nb_samples;
    /// 输出的样本数
    int64_t frames = av_rescale_q_rnd(samples, base, target, AV_ROUND_UP | AV_ROUND_PASS_MINMAX);
    /// 实际输出样本数
    int converted_samples = 0;
    DWORD res = 0;
    if (!(converted_input_samples = malloc(sizeof(void*) * handle->sdl_spec.channels))) {
        re = PLAYER_ERR_OOM;
        goto end;
    }
    memset(converted_input_samples, 0, sizeof(void*) * handle->sdl_spec.channels);
    if ((re = av_samples_alloc(converted_input_samples, NULL, handle->sdl_spec.channels, frames, handle->target_format, 0)) < 0) {
        re = PLAYER_ERR_OOM;
        goto end;
    }
    re = 0;
    if ((converted_samples = swr_convert(handle->swrac, converted_input_samples, frames, (const uint8_t**)frame->extended_data, samples)) < 0) {
        re = converted_samples;
        goto end;
    }
    res = WaitForSingleObject(handle->mutex, INFINITE);
    if (res != WAIT_OBJECT_0) {
        re = PLAYER_ERR_WAIT_MUTEX_FAILED;
        goto end;
    }
    if ((converted_samples = av_audio_fifo_write(handle->buffer, (void**)converted_input_samples, converted_samples)) < 0) {
        ReleaseMutex(handle->mutex);
        re = converted_samples;
        goto end;
    }
    handle->end_pts += av_rescale_q_rnd(converted_samples, target, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
    *writed = 1;
    ReleaseMutex(handle->mutex);
end:
    if (converted_input_samples) {
        av_freep(&converted_input_samples[0]);
        free(converted_input_samples);
    }
    return re;
}

int video_add_to_fifo(PlayerSession* handle, AVFrame* frame, char* writed) {
    if (!handle || !frame || !writed) return PLAYER_ERR_NULLPTR;
    if (!handle->has_video) return PLAYER_ERR_OK;
    int re = 0;
    DWORD res = 0;
    if (handle->next_video_timestamp != INT64_MIN) {
        // 碰撞检测
        int64_t diff = handle->next_video_timestamp - av_gettime();
        int64_t frame_time = av_rescale_q(1, av_make_q(1, handle->sdl_display_mode.refresh_rate), AV_TIME_BASE_Q) / 8;
        if (diff <= frame_time  && diff >= -frame_time) {
            DWORD sleepTime = (diff + frame_time) / 1000 + 1;
            av_log(NULL, AV_LOG_DEBUG, "Sleep due to mutex collide: diff=%lld, sleep=%ld\n", diff, sleepTime);
            Sleep(sleepTime);
        }
    }
    res = WaitForSingleObject(handle->video_mutex, INFINITE);
    if (res != WAIT_OBJECT_0) {
        re = PLAYER_ERR_WAIT_MUTEX_FAILED;
        return re;
    }
    if ((re = av_fifo_write(handle->video_buffer, &frame, 1)) < 0) {
        ReleaseMutex(handle->video_mutex);
        av_log(NULL, AV_LOG_ERROR, "Failed to write video frame to buffer: %s (%i)\n", av_err2str(re), re);
        goto end;
    }
    ReleaseMutex(handle->video_mutex);
    av_log(NULL, AV_LOG_DEBUG, "Video frame added to buffer.\n");
    *writed = 1;
    re = 0;
end:
    return re;
}

int decode(PlayerSession* handle, char* audio_writed, char* video_writed) {
    if (!handle) return PLAYER_ERR_NULLPTR;
    if (!audio_writed && !video_writed) return PLAYER_ERR_NULLPTR;
    AVPacket pkt;
    AVFrame* frame = av_frame_alloc();
    if (audio_writed) *audio_writed = 0;
    if (video_writed) *video_writed = 0;
    if (!frame) {
        return PLAYER_ERR_OOM;
    }
    int re = 0;
    while (1) {
        if (handle->has_audio && !handle->audio_is_eof && audio_writed) {
            if ((re = decode_audio_internal(handle, audio_writed, frame))) {
                goto end;
            }
        }
        if (handle->has_video && !handle->video_is_eof && video_writed) {
            if ((re = decode_video_internal(handle, video_writed, frame))) {
                goto end;
            }
        }
        if ((audio_writed && *audio_writed) || (video_writed && *video_writed) || ((!handle->has_audio || handle->audio_is_eof) && (!handle->has_video || handle->video_is_eof))) {
            break;
        }
        if ((re = av_read_frame(handle->fmt, &pkt)) < 0) {
            if (re == AVERROR_EOF) {
                handle->audio_is_eof = 1;
                handle->video_is_eof = 1;
                re = PLAYER_ERR_OK;
                goto end;
            }
            goto end;
        }
        if (pkt.stream_index == handle->audio_input_stream->index) {
            handle->last_pkt_pts = av_rescale_q_rnd(pkt.pts, handle->audio_input_stream->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            if ((re = avcodec_send_packet(handle->audio_decoder, &pkt)) < 0) {
                if (re == AVERROR(EAGAIN)) {
                    re = 0;
                    av_packet_unref(&pkt);
                    continue;
                }
                av_packet_unref(&pkt);
                goto end;
            }
        } else if (pkt.stream_index == handle->video_input_stream->index) {
            handle->last_pkt_pts = av_rescale_q_rnd(pkt.pts, handle->video_input_stream->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            if ((re = avcodec_send_packet(handle->video_decoder, &pkt)) < 0) {
                if (re == AVERROR(EAGAIN)) {
                    re = 0;
                    av_packet_unref(&pkt);
                    continue;
                }
                av_packet_unref(&pkt);
                goto end;
            }
        } else {
            av_packet_unref(&pkt);
            continue;
        }
        av_packet_unref(&pkt);
        if (handle->has_audio && !handle->audio_is_eof && audio_writed) {
            if ((re = decode_audio_internal(handle, audio_writed, frame))) {
                goto end;
            }
        }
        if (handle->has_video && !handle->video_is_eof && video_writed) {
            if ((re = decode_video_internal(handle, video_writed, frame))) {
                goto end;
            }
        }
        if ((audio_writed && *audio_writed) || (video_writed && *video_writed) || ((!handle->has_audio || handle->audio_is_eof) && (!handle->has_video || handle->video_is_eof))) {
            break;
        }
    }
end:
    if (frame) {
        if (video_writed && !*video_writed) {
            av_frame_free(&frame);
        } else if (audio_writed) {
            av_frame_free(&frame);
        }
    }
    return re;
}
