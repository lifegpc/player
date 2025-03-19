#include "video_output.h"

int init_video_output(PlayerSession* session) {
    if (!session) return PLAYER_ERR_NULLPTR;
    if (!session->has_video) return PLAYER_ERR_OK;
    if (!session->video_decoder) return PLAYER_ERR_NULLPTR;
    if (session->settings->hWnd) {
        session->window = SDL_CreateWindowFrom(*session->settings->hWnd);
        session->is_external_window = 1;
    } else {
        session->window = SDL_CreateWindow("Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, session->video_decoder->width, session->video_decoder->height, SDL_WINDOW_RESIZABLE);
    }
    if (!session->window) {
        av_log(NULL, AV_LOG_FATAL, "Failed to create window: %s\n", SDL_GetError());
        return PLAYER_ERR_SDL;
    }
    int index = 0;
    if ((index = SDL_GetWindowDisplayIndex(session->window)) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to get window display index: %s\n", SDL_GetError());
        return PLAYER_ERR_SDL;
    }
    if (SDL_GetCurrentDisplayMode(index, &session->sdl_display_mode) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to get display mode: %s\n", SDL_GetError());
        return PLAYER_ERR_SDL;
    }
    av_log(NULL, AV_LOG_VERBOSE, "Display on screen %d: %dx%d %dHz\n", index, session->sdl_display_mode.w, session->sdl_display_mode.h, session->sdl_display_mode.refresh_rate);
    if (!session->sdl_display_mode.refresh_rate) {
        av_log(NULL, AV_LOG_WARNING, "Display refresh rate is 0, using 60Hz.\n");
        session->sdl_display_mode.refresh_rate = 60;
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
    session->video_is_init = 1;
    return PLAYER_ERR_OK;
}

void video_display(PlayerSession *is) {
    if (!is) return;
    if (!is->has_video) return;
    if (!is->video_is_init) return;
    AVFrame* frame;
    if (av_fifo_can_read(is->video_buffer) == 0) {
        SDL_RenderPresent(is->renderer);
        ReleaseMutex(is->video_mutex);
        return;
    }
    int re = av_fifo_peek(is->video_buffer, &frame, 1, 0);
    if (re < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to read video frame from buffer: %s (%i)\n", av_err2str(re), re);
        ReleaseMutex(is->video_mutex);
        return;
    }
    ReleaseMutex(is->video_mutex);
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
    SDL_RenderCopy(is->renderer, is->texture, NULL, &rect);
    SDL_RenderPresent(is->renderer);
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
    if (!is->video_is_init) {
        return;
    }
    if (!is->is_playing) {
        return;
    }
    DWORD re = WaitForSingleObject(is->video_mutex, INFINITE);
    if (re != WAIT_OBJECT_0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to wait for video mutex: %d\n", re);
        return;
    }
    int64_t diff = is->first_pts != INT64_MIN && is->video_first_pts != INT64_MIN ? is->first_pts - is->video_first_pts : 0;
    int64_t audio_diff = is->last_pts_timestamp != INT64_MIN ? av_gettime() - is->last_pts_timestamp : 0;
    int64_t curpos = is->pts - diff + audio_diff;
    int64_t frame_time = av_rescale_q(1, av_make_q(1, is->sdl_display_mode.refresh_rate), AV_TIME_BASE_Q);
    int64_t true_frame_time = av_rescale_q(1, av_make_q(is->video_decoder->framerate.den, is->video_decoder->framerate.num), AV_TIME_BASE_Q);
    int64_t true_next_frame_time = is->video_pts + true_frame_time;
    while (curpos >= true_next_frame_time) {
        AVFrame* frame;
        if (av_fifo_can_read(is->video_buffer) == 0) {
            ReleaseMutex(is->video_mutex);
            av_log(NULL, AV_LOG_DEBUG, "No enough video frame in buffer.\n");
            return;
        }
        int re = av_fifo_read(is->video_buffer, &frame, 1);
        if (re < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to read video frame from buffer: %s (%i)\n", av_err2str(re), re);
            ReleaseMutex(is->video_mutex);
            return;
        }
        av_frame_free(&frame);
        av_log(NULL, AV_LOG_DEBUG, "Discard a video frame. diff=%lld, audio_diff=%lld, curpos=%lld, true_next_frame_time=%lld\n", diff, audio_diff, curpos, true_next_frame_time);
        is->video_pts += true_frame_time;
        true_next_frame_time = is->video_pts + true_frame_time;
    }
    int64_t next_frame_time = is->video_pts + frame_time;
    // 允许提前 1 / 4 帧
    while (curpos >= next_frame_time - frame_time / 4) {
        av_log(NULL, AV_LOG_DEBUG, "Skip a video frame. diff=%lld, audio_diff=%lld, curpos=%lld, next_frame_time=%lld\n", diff, audio_diff, curpos, next_frame_time);
        next_frame_time += frame_time;
    }
    int64_t delay = next_frame_time - curpos;
    av_log(NULL, AV_LOG_DEBUG, "diff=%lld, audio_diff=%lld, curpos=%lld, true_next_frame_time=%lld, next_frame_time=%lld, delay=%lld\n", diff, audio_diff, curpos, true_next_frame_time, next_frame_time, delay);
    is->next_video_timestamp = av_gettime() + delay;
    schedule_refresh(is, delay / 1000);
    video_display(is);
}
