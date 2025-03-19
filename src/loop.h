#ifndef _PLAYER_LOOP_H
#define _PLAYER_LOOP_H
#if __cplusplus
extern "C" {
#endif
#include "core.h"
DWORD WINAPI decode_loop(LPVOID handle);
DWORD WINAPI event_loop(LPVOID handle);
DWORD WINAPI external_window_event_loop(LPVOID handle);
#if __cplusplus
}
#endif
#endif
