#ifndef _PLAYER_VIDEO_OUTPUT_H
#define _PLAYER_VIDEO_OUTPUT_H
#if __cplusplus
extern "C" {
#endif
#include "core.h"
int init_video_output(PlayerSession* session);
Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque);
void schedule_refresh(PlayerSession *is, int delay);
void video_display(PlayerSession *is);
void video_refresh_timer(void *userdata);
#if __cplusplus
}
#endif
#endif
