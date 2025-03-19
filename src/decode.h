#ifndef _PLAYER_DECODE_H
#define _PLAYER_DECODE_H
#if __cplusplus
extern "C" {
#endif
#include "core.h"
int open_audio_decoder(PlayerSession* session);
int open_video_decoder(PlayerSession* session);
int decode_audio_internal(PlayerSession* handle, char* writed, AVFrame* frame);
int decode_video_internal(PlayerSession* handle, char* writed, AVFrame* frame);
int audio_convert_samples_and_add_to_fifo(PlayerSession* handle, AVFrame* frame, char* writed);
int video_add_to_fifo(PlayerSession* handle, AVFrame* frame, char* writed);
int decode(PlayerSession* handle, char* audio_writed, char* video_writed);
#if __cplusplus
}
#endif
#endif
