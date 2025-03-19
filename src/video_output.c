#include "video_output.h"

int init_video_output(PlayerSession* session) {
    if (!session) return PLAYER_ERR_NULLPTR;
    if (!session->has_video) return PLAYER_ERR_OK;
    if (!session->video_decoder) return PLAYER_ERR_NULLPTR;
    session->window = SDL_CreateWindow("Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, session->video_decoder->width, session->video_decoder->height, SDL_WINDOW_RESIZABLE);
    if (!session->window) {
        av_log(NULL, AV_LOG_FATAL, "Failed to create window: %s\n", SDL_GetError());
        return PLAYER_ERR_SDL;
    }
    session->renderer = SDL_CreateRenderer(session->window, -1, 0);
    if (!session->renderer) {
        av_log(NULL, AV_LOG_FATAL, "Failed to create renderer: %s\n", SDL_GetError());
        return PLAYER_ERR_SDL;
    }
    SDL_GetWindowSize(session->window, &session->window_width, &session->window_height);
    /// #TODO: 支持YUV外的其他格式
    session->sdl_pixel_format = SDL_PIXELFORMAT_IYUV;
    session->texture = SDL_CreateTexture(session->renderer, session->sdl_pixel_format, SDL_TEXTUREACCESS_STREAMING, session->window_width, session->window_height);
    if (!session->texture) {
        av_log(NULL, AV_LOG_FATAL, "Failed to create texture: %s\n", SDL_GetError());
        return PLAYER_ERR_SDL;
    }
    session->sws = sws_getContext(session->video_decoder->width, session->video_decoder->height, session->video_decoder->pix_fmt, session->window_width, session->window_height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    if (!session->sws) {
        av_log(NULL, AV_LOG_FATAL, "Failed to create sws context.\n");
        return PLAYER_ERR_OOM;
    }
    session->video_buffer = av_fifo_alloc2(0, sizeof(AVFrame*), AV_FIFO_FLAG_AUTO_GROW);
    return PLAYER_ERR_OK;
}

void video_display(PlayerSession *is) {
    if (!is) return;
    if (!is->has_video) return;
    AVFrame* frame;
    DWORD a = WaitForSingleObject(is->video_mutex, INFINITE);
    if (a != WAIT_OBJECT_0) return;
    if (av_fifo_can_read(is->video_buffer) == 0) {
        SDL_RenderPresent(is->renderer);
        goto end;
    }
    int re = av_fifo_read(is->video_buffer, &frame, 1);
    if (re < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to read video frame from buffer: %s (%i)\n", av_err2str(re), re);
        goto end;
    }
    av_log(NULL, AV_LOG_DEBUG, "Displaying video frame.\n");
    AVFrame* target = av_frame_alloc();
    sws_scale_frame(is->sws, target, frame);
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = is->window_width;
    rect.h = is->window_height;
    SDL_UpdateYUVTexture(is->texture, NULL, target->data[0], target->linesize[0], target->data[1], target->linesize[1], target->data[2], target->linesize[2]);
    av_frame_free(&target);
    SDL_RenderClear(is->renderer);
    SDL_RenderCopy(is->renderer, is->texture, &rect, &rect);
    SDL_RenderPresent(is->renderer);
    av_frame_free(&frame);
end:
    ReleaseMutex(is->video_mutex);
}

Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque) {
    video_refresh_timer(opaque);
    return 0;
}

void schedule_refresh(PlayerSession *is, int delay) {
    if (!SDL_AddTimer(delay, sdl_refresh_timer_cb, is)) {
        av_log(NULL, AV_LOG_ERROR, "Failed to add timer: %s\n", SDL_GetError());
    }
    av_log(NULL, AV_LOG_DEBUG, "Scheduled refresh on %d ms.\n", delay);
}

void video_refresh_timer(void *userdata) {
    if (!userdata) return;
    PlayerSession* is = (PlayerSession*)userdata;
    if (!is->has_video) return;
    if (!is->is_playing) {
        AVRational tb = { 1, 1000 };
        AVRational rps = { is->video_decoder->framerate.den, is->video_decoder->framerate.num };
        int64_t delay = av_rescale_q_rnd(1, rps, tb, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        schedule_refresh(is, delay);
        return;
    }
    if (av_fifo_can_read(is->video_buffer) == 0) {
        if (!is->video_is_eof) {
            av_log(NULL, AV_LOG_DEBUG,"No buffer.\n");
            schedule_refresh(is, 1);
        }
    } else {
        if (is->video_last_pts == INT64_MIN) {
            is->video_pts = is->video_first_pts;
            video_display(is);
            is->video_last_pts = is->video_pts;
            AVRational rps = { is->video_decoder->framerate.den, is->video_decoder->framerate.num };
            is->video_pts += av_rescale_q_rnd(1, rps, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            is->video_pts_time = av_gettime();
            AVRational tb = { 1, 1000 };
            int64_t delay = av_rescale_q_rnd(1, rps, tb, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX) / 2;
            av_log(NULL, AV_LOG_DEBUG, "delay: %lld\n", delay);
            schedule_refresh(is, delay);
        } else {
            int64_t now = av_gettime();
            AVRational tb = { 1, 1000 };
            int64_t delay = av_rescale_q(is->video_pts - is->video_last_pts, AV_TIME_BASE_Q, tb);
            av_log(NULL, AV_LOG_DEBUG, "Should delay: %lld\n", delay);
            if (now - is->video_pts_time < delay) {
                schedule_refresh(is, (delay - (now - is->video_pts_time)) / 2);
            } else {
                video_display(is);
                AVRational rps = { is->video_decoder->framerate.den, is->video_decoder->framerate.num };
                is->video_last_pts = is->video_pts;
                is->video_pts += av_rescale_q_rnd(1, rps, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
                is->video_pts_time = av_gettime();
                
                int64_t delay = av_rescale_q_rnd(1, rps, tb, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX) / 2;
                av_log(NULL, AV_LOG_DEBUG, "delay: %lld\n", delay);
                schedule_refresh(is, delay);
            }
        }
    }
}
