#include "loop.h"
#include "decode.h"
#include "video_output.h"

DWORD WINAPI decode_loop(LPVOID handle) {
    if (!handle) return PLAYER_ERR_NULLPTR;
    PlayerSession* h = (PlayerSession*)handle;
    char doing = 0;
    char audio_writed = 0, video_writed = 0;
    int audio_buffered_size = h->has_audio ? h->sdl_spec.freq / 2 : 0;
    while (1) {
        doing = 0;
        if (h->stoping) break;
        if (h->has_audio && !h->audio_is_eof) {
            audio_buffered_size = h->sdl_spec.freq / 2;
            if (av_audio_fifo_size(h->buffer) < audio_buffered_size) {
                int re = decode(handle, &audio_writed, &video_writed);
                if (re) {
                    av_log(NULL, AV_LOG_WARNING, "%s %i: Error when calling decode_audio: %s (%i).\n", __FILE__, __LINE__, av_err2str(re), re);
                    h->have_err = 1;
                    h->err = re;
                }
                doing = 1;
            }
        } else if (h->has_audio && h->audio_is_eof) {
            if (av_audio_fifo_size(h->buffer) == 0) {
                SDL_PauseAudioDevice(h->device_id, 1);
                h->is_playing = 0;
            }
        }
        if (!doing) {
            Sleep(10);
        }
    }
    return 0;
}

DWORD WINAPI event_loop(LPVOID handle) {
    if (!handle) return PLAYER_ERR_NULLPTR;
    PlayerSession* h = (PlayerSession*)handle;
    if (!h->video_is_init) h->err = init_video_output(h);
    SDL_Event e;
    while (1) {
        if (SDL_WaitEventTimeout(&e, 10)) {
            av_log(NULL, AV_LOG_DEBUG, "Event type: %d\n", e.type);
            switch (e.type) {
            case SDL_WINDOWEVENT:
                av_log(NULL, AV_LOG_DEBUG, "Window event: %d\n", e.window.event);
                if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                    h->is_playing = 0;
                    if (h->has_audio && h->device_id) {
                        SDL_PauseAudioDevice(h->device_id, 1);
                    }
                    h->stoping = 1;
                    return 0;
                }
                break;  
            case FF_QUIT_EVENT:
                return 0;
            case FF_REFRESH_EVENT:
                video_refresh_timer(e.user.data1);
                break;
            default:
                break;
            }
        } else {
            if (h->stoping) {
                return 0;
            }
        }
    }
    return 0;
}

DWORD WINAPI external_window_event_loop(LPVOID handle) {
    if (!handle) return PLAYER_ERR_NULLPTR;
    PlayerSession* h = (PlayerSession*)handle;
    SDL_Event e;
    while (1) {
        if (SDL_WaitEventTimeout(&e, 10)) {
            switch (e.type) {
            case FF_QUIT_EVENT:
                return 0;
            case FF_REFRESH_EVENT:
                video_refresh_timer(e.user.data1);
                break;
            default:
                break;
            }
        } else {
            if (h->stoping) {
                return 0;
            }
        }
    }
}
