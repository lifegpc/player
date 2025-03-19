#include "core.h"
#include "open.h"
#include "cpp2c.h"
#include <malloc.h>
#include "fileop.h"
#include "decode.h"
#include "audio_output.h"
#include "video_output.h"
#include "loop.h"

static FILE* log_file = nullptr;
static int log_max_level = AV_LOG_INFO;
static char log_last_char = '\n';

int32_t player_version() {
    return PLAYER_VERSION_INT;
}

const char* player_version_str() {
    return PLAYER_VERSION;
}

char* player_get_err_msg(int err) {
    if (err < 0) {
        char msg[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(err, msg, AV_ERROR_MAX_STRING_SIZE);
        char* tmp = nullptr;
        if (cpp2c::string2char(msg, tmp)) {
            return tmp;
        }
    } else if (err == PLAYER_ERR_SDL) {
        char msg[128];
        SDL_GetErrorMsg(msg, 128);
        char* tmp = nullptr;
        if (cpp2c::string2char(msg, tmp)) {
            return tmp;
        }
    } else {
        auto msg = player_get_err_msg2(err);
        char* tmp = nullptr;
        if (cpp2c::string2char(msg, tmp)) {
            return tmp;
        }
    }
    return nullptr;
}

const char* player_get_err_msg2(int err) {
    switch (err) {
    case PLAYER_ERR_OK:
        return "No error";
    case PLAYER_ERR_NULLPTR:
        return "Null pointer";
    case PLAYER_ERR_SDL:
        return "SDL error";
    case PLAYER_ERR_OOM:
        return "Out of memory";
    case PLAYER_ERR_NO_STREAM_OR_DECODER:
        return "No stream or decoder found";
    case PLAYER_ERR_UNKNOWN_AUDIO_SAMPLE_FMT:
        return "Unknown audio sample format";
    case PLAYER_ERR_FAILED_CREATE_MUTEX:
        return "Failed to create mutex";
    case PLAYER_ERR_WAIT_MUTEX_FAILED:
        return "Failed to wait mutex";
    case PLAYER_ERR_FAILED_CREATE_THREAD:
        return "Failed to create thread";
    default:
        return "Unknown error";
    }
}

int player_create(const char* url, PlayerSession** session) {
    return player_create2(url, session, nullptr);
}

int player_create2(const char* url, PlayerSession** session, PlayerSettings* settings) {
    if (!url || !session) return PLAYER_ERR_NULLPTR;
    PlayerSession* ses = (PlayerSession*)malloc(sizeof(PlayerSession));
    int re = PLAYER_ERR_OK;
    if (!ses) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to allocate memory for PlayerSession.\n");
        return PLAYER_ERR_OOM;
    }
    memset(ses, 0, sizeof(PlayerSession));
    // 初始化设置
    if (settings) {
        ses->settings = settings;
    } else {
        ses->settings = player_settings_init();
        if (!ses->settings) {
            re = PLAYER_ERR_OOM;
            goto end;
        }
        ses->settings_is_alloc = 1;
    }
    ses->first_pts = INT64_MIN;
    ses->video_first_pts = INT64_MIN;
    ses->video_last_pts = INT64_MIN;
    if ((re = open_input(ses, url))) {
        goto end;
    }
    av_dump_format(ses->fmt, 0, url, 0);
    re = find_audio_stream(ses);
    if (!re) {
        ses->has_audio = 1;
    } else if (re == PLAYER_ERR_NO_STREAM_OR_DECODER) {
        av_log(nullptr, AV_LOG_INFO, "No audio stream found.\n");
    } else {
        goto end;
    }
    re = find_video_stream(ses);
    if (!re) {
        ses->has_video = 1;
    } else if (re == PLAYER_ERR_NO_STREAM_OR_DECODER) {
        av_log(nullptr, AV_LOG_INFO, "No video stream found.\n");
    } else {
        goto end;
    }
    if (!ses->has_audio && !ses->has_video) {
        av_log(nullptr, AV_LOG_ERROR, "No audio and video stream found.\n");
        re = PLAYER_ERR_NO_STREAM_OR_DECODER;
        goto end;
    }
    if ((re = open_audio_decoder(ses))) {
        goto end;
    }
    if ((re = open_video_decoder(ses))) {
        goto end;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS)) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to initialize SDL: %s\n", SDL_GetError());
        re = PLAYER_ERR_SDL;
        goto end;
    }
    ses->sdl_initialized = 1;
    if ((re = init_audio_output(ses))) {
        goto end;
    }
    ses->video_buffer = av_fifo_alloc2(0, sizeof(AVFrame*), AV_FIFO_FLAG_AUTO_GROW);
    if (!ses->video_buffer) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to allocate video buffer.\n");
        re = PLAYER_ERR_OOM;
        goto end;
    }
    if (ses->settings->hWnd) {
        if ((re = init_video_output(ses))) {
            goto end;
        }
    }
    ses->mutex = CreateMutexW(nullptr, FALSE, nullptr);
    if (!ses->mutex) {
        re = PLAYER_ERR_FAILED_CREATE_MUTEX;
        goto end;
    }
    ses->video_mutex = CreateMutexW(nullptr, FALSE, nullptr);
    if (!ses->video_mutex) {
        re = PLAYER_ERR_FAILED_CREATE_MUTEX;
        goto end;
    }
    ses->decode_thread = CreateThread(nullptr, 0, decode_loop, ses, 0, NULL);
    if (!ses->decode_thread) {
        re = PLAYER_ERR_FAILED_CREATE_THREAD;
        goto end;
    }
    ses->event_thread = CreateThread(nullptr, 0, event_loop, ses, 0, NULL);
    if (!ses->event_thread) {
        re = PLAYER_ERR_FAILED_CREATE_THREAD;
        goto end;
    }
    *session = ses;
    return re;
end:
    player_free(&ses);
    *session = nullptr;
    return re;
}

void player_free(PlayerSession** session) {
    if (!session) return;
    auto s = *session;
    if (!s) return;
    if (s->has_audio && s->device_id) SDL_CloseAudioDevice(s->device_id);
    s->stoping = 1;
    SDL_Event evt;
    evt.type = FF_QUIT_EVENT;
    SDL_PushEvent(&evt);
    if (s->event_thread) {
        DWORD status;
        while (GetExitCodeThread(s->event_thread, &status)) {
            if (status == STILL_ACTIVE) {
                status = 0;
                s->stoping = 1;
                Sleep(10);
            } else {
                break;
            }
        }
    }
    if (s->renderer) SDL_DestroyRenderer(s->renderer);
    if (s->texture) SDL_DestroyTexture(s->texture);
    if (!s->is_external_window && s->window) SDL_DestroyWindow(s->window);
    if (s->sdl_initialized) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
    }
    if (s->decode_thread) {
        DWORD status;
        while (GetExitCodeThread(s->decode_thread, &status)) {
            if (status == STILL_ACTIVE) {
                status = 0;
                s->stoping = 1;
                Sleep(10);
            } else {
                break;
            }
        }
    }
    if (s->buffer) av_audio_fifo_free(s->buffer);
    if (s->video_buffer) {
        size_t can_read = 0;
        if ((can_read = av_fifo_can_read(s->video_buffer)) > 0) {
            av_log(nullptr, AV_LOG_WARNING, "There are %zu frames left in video buffer.\n", can_read);
            AVFrame* frame;
            while (av_fifo_read(s->video_buffer, &frame, 1) >= 0) {
                av_frame_free(&frame);
            }
        }
        av_fifo_freep2(&s->video_buffer);
    }
    if (s->swrac) swr_free(&s->swrac);
    if (s->sws) sws_freeContext(s->sws);
    if (s->video_decoder) avcodec_free_context(&s->video_decoder);
    if (s->audio_decoder) avcodec_free_context(&s->audio_decoder);
    if (s->fmt) avformat_close_input(&s->fmt);
    if (s->settings_is_alloc) {
        player_settings_free(&s->settings);
    }
    if (s->mutex) CloseHandle(s->mutex);
    if (s->video_mutex) CloseHandle(s->video_mutex);
    free(s);
    *session = nullptr;
}

void play(const char* filename, void** hWnd) {
    PlayerSettings* settings = player_settings_init();
    if (!settings) return;
    player_settings_set_hWnd(settings, hWnd);
    PlayerSession* ses = nullptr;
    int re = player_create2(filename, &ses, settings);
    if (re != PLAYER_ERR_OK) {
        char* err = player_get_err_msg(re);
        if (err) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to create player session: %s\n", err);
            free(err);
        } else {
            av_log(nullptr, AV_LOG_ERROR, "Failed to create player session: %d\n", re);
        }
        return;
    }
    if (wait_player_inited(ses)) {
        return;
    }
    ses->is_playing = 1;
    SDL_PauseAudioDevice(ses->device_id, 0);
    schedule_refresh(ses, 1);
    while (ses->is_playing) {
        Sleep(10);
    }
    player_free(&ses);
}

PlayerSettings* player_settings_init() {
    auto s = (PlayerSettings*)malloc(sizeof(PlayerSettings));
    if (!s) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to allocate memory for PlayerSettings.\n");
        return nullptr;
    }
    memset(s, 0, sizeof(PlayerSettings));
    player_settings_default(s);
    return s;
}

void player_settings_default(PlayerSettings* settings) {
    if (!settings) return;
    settings->resize = 1;
}

void player_settings_set_resize(PlayerSettings* settings, unsigned char resize) {
    if (!settings) return;
    settings->resize = resize;
}

void player_settings_free(PlayerSettings** settings) {
    if (!settings) return;
    auto s = *settings;
    if (!s) return;
    free(s);
    *settings = nullptr;
}

const char* log_level(int level) {
    switch (level) {
    case AV_LOG_QUIET:
        return "QUIET";
    case AV_LOG_PANIC:
        return "PANIC";
    case AV_LOG_FATAL:
        return "FATAL";
    case AV_LOG_ERROR:
        return "ERROR";
    case AV_LOG_WARNING:
        return "WARNING";
    case AV_LOG_INFO:
        return "INFO";
    case AV_LOG_VERBOSE:
        return "VERBOSE";
    case AV_LOG_DEBUG:
        return "DEBUG";
    default:
        return "UNKNOWN";
    }
}

void av_log_file_callback(void* ptr, int level, const char* fmt, va_list vl) {
    if (log_file && level <= log_max_level) {
        if (log_last_char == '\n') fprintf(log_file, "[%s] ", log_level(level));
        log_last_char = fmt[strlen(fmt) - 1];
        vfprintf(log_file, fmt, vl);
        fflush(log_file);
    }
}

void set_player_log_file(const char* filename, unsigned char append, int max_level) {
    if (log_file) {
        fileop::fclose(log_file);
        log_file = nullptr;
    }
    if (filename) {
        log_file = fileop::fopen(filename, append ? "ab" : "wb");
        if (!log_file) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to open log file \"%s\".\n", filename);
            log_file = nullptr;
            av_log_set_callback(av_log_default_callback);
            return;
        }
        log_max_level = max_level;
        av_log_set_callback(av_log_file_callback);
    } else {
        av_log_set_callback(av_log_default_callback);
    }
}

void player_settings_set_hWnd(PlayerSettings* settings, void** hWnd) {
    if (!settings) return;
    settings->hWnd = hWnd;
}

int wait_player_inited(PlayerSession* session) {
    if (!session) return PLAYER_ERR_NULLPTR;
    while (!session->video_is_init) {
        Sleep(10);
    }
    return session->err;
}
