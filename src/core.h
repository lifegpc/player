#ifndef _PLAYER_CORE_H
#define _PLAYER_CORE_H

#include "../player.h"
#include "player_version.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/fifo.h"
#include "libavutil/timestamp.h"
#include "libswscale/swscale.h"
#ifdef __cplusplus
}
#endif
#include "SDL2/SDL.h"
#include <Windows.h>

#define FF_REFRESH_EVENT (SDL_USEREVENT)
#define FF_QUIT_EVENT (SDL_USEREVENT + 1)

typedef struct PlayerSettings {
    /// @brief 是否自动调节窗口大小
    unsigned char resize: 1;
} PlayerSettings;

typedef struct PlayerSession {
    /// @brief Demux 用
    AVFormatContext* fmt;
    /// @brief 要解码的视频流
    AVStream* video_input_stream;
    /// @brief 视频解码器类型
    const AVCodec* video_codec;
    /// @brief 视频解码器
    AVCodecContext* video_decoder;
    /// @brief 要解码的音频流
    AVStream* audio_input_stream;
    /// @brief 音频解码器类型
    const AVCodec* audio_codec;
    /// @brief 音频解码器
    AVCodecContext* audio_decoder;
    /// @brief 用于转换音频格式
    struct SwrContext* swrac;
    /// @brief 指定的SDL输出格式
    SDL_AudioSpec sdl_spec;
    AVChannelLayout output_channel_layout;
    /// @brief 解码专用线程
    HANDLE decode_thread;
    /// @brief 音频缓冲区
    AVAudioFifo* buffer;
    AVFifo* video_buffer;
    /// @brief 音频输出格式
    enum AVSampleFormat target_format;
    /// @brief 每样本字节数
    int target_format_pbytes;
    /// @brief 音频设备 ID
    SDL_AudioDeviceID device_id;
    /// @brief 错误码（来自FFmpeg或核心本身）
    int err;
    /// @brief 互斥锁，保护音频缓冲区和时间
    HANDLE mutex;
    HANDLE video_mutex;
    /// @brief 缓冲区开始时间
    int64_t pts;
    /// @brief 缓冲区结束时间
    int64_t end_pts;
    /// @brief 第一个sample的pts
    int64_t first_pts;
    /// @brief 视频缓冲区开始时间
    int64_t video_pts;
    int64_t video_end_pts;
    int64_t video_first_pts;
    /// @brief 上一帧视频播放时间
    int64_t video_pts_time;
    /// @brief 上一帧音频播放位置
    int64_t video_last_pts;
    /// 最近一个包的时间
    int64_t last_pkt_pts;
    /// @brief SDL 窗口
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t sdl_pixel_format;
    int window_width;
    int window_height;
    HANDLE event_thread;
    SwsContext* sws;
    /// @brief 播放设置
    PlayerSettings* settings;
    /// @brief 是否初始化了SDL
    unsigned char sdl_initialized : 1;
    /// 让事件处理线程退出标志位
    unsigned char stoping : 1;
    /// 音频是否已读到文件尾部
    unsigned char audio_is_eof : 1;
    unsigned char video_is_eof : 1;
    /// 是否有错误
    unsigned char have_err : 1;
    unsigned char is_playing : 1;
    /// 设置是内部分配
    unsigned char settings_is_alloc : 1;
    unsigned char has_audio : 1;
    unsigned char has_video : 1;
    /// 需要设置新的audio pts
    unsigned char set_new_pts : 1;
    unsigned char set_new_video_pts : 1;
    unsigned char is_external_window : 1;
} PlayerSession;

#endif
