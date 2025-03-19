#ifndef _PLAYER_OPEN_H
#define _PLAYER_OPEN_H
#if __cplusplus
extern "C" {
#endif
#include "core.h"
int open_input(PlayerSession* session, const char* url);
int find_audio_stream(PlayerSession* session);
int find_video_stream(PlayerSession* session);
#if __cplusplus
}
#endif
#endif
