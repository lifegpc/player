#ifndef _PLAYER_AUDIO_OUTPUT_H
#define _PLAYER_AUDIO_OUTPUT_H
#if __cplusplus
extern "C" {
#endif
#include "core.h"
int init_audio_output(PlayerSession* session);
enum AVSampleFormat convert_to_sdl_supported_format(enum AVSampleFormat fmt);
SDL_AudioFormat convert_to_sdl_format(enum AVSampleFormat fmt);
void SDL_audio_callback(void* userdata, uint8_t* stream, int len);
int get_sdl_channel_layout(int channels, AVChannelLayout* channel_layout);
#if __cplusplus
}
#endif
#endif
