#ifndef _PLAYER_H
#define _PLAYER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#if _WIN32
#if BUILD_PLAYER
#define PLAYER_API __declspec(dllexport)
#else
#define PLAYER_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define PLAYER_API __attribute__((visibility("default")))
#else
#define PLAYER_API 
#endif

typedef struct PlayerSession PlayerSession;
typedef struct PlayerSettings PlayerSettings;

#ifndef BUILD_PLAYER
#define AV_LOG_QUIET    -8
#define AV_LOG_PANIC     0
#define AV_LOG_FATAL     8
#define AV_LOG_ERROR    16
#define AV_LOG_WARNING  24
#define AV_LOG_INFO     32
#define AV_LOG_VERBOSE  40
#define AV_LOG_DEBUG    48
#define AV_LOG_TRACE    56
#endif

#define PLAYER_ERR_OK 0
#define PLAYER_ERR_NULLPTR 1
#define PLAYER_ERR_SDL 2
#define PLAYER_ERR_OOM 3
#define PLAYER_ERR_NO_STREAM_OR_DECODER 4
#define PLAYER_ERR_UNKNOWN_AUDIO_SAMPLE_FMT 5
#define PLAYER_ERR_FAILED_CREATE_MUTEX 6
#define PLAYER_ERR_WAIT_MUTEX_FAILED 7
#define PLAYER_ERR_FAILED_CREATE_THREAD 8

PLAYER_API const char* player_version_str();
PLAYER_API int32_t player_version();
/**
 * @brief 设置播放器日志文件
 * 
 * 如果之前设置过日志文件，会关闭之前的日志文件。
 * @param filename 日志文件路径，如果为NULL则关闭之前的设置的日志文件
 * @param append 是否追加到文件末尾
 * @param max_level 最大日志级别，小于等于这个级别的日志会被记录
*/
PLAYER_API void set_player_log_file(const char* filename, unsigned char append, int max_level);
/**
 * @brief 返回错误代码对应的错误消息
 * @param err 错误代码
 * @return 错误消息，需要调用free释放内存
*/
PLAYER_API char* player_get_err_msg(int err);
/**
 * @brief 返回错误代码对应的错误消息
 * @param err 错误代码（仅处理>=0的错误）
 * @return 错误消息
*/
PLAYER_API const char* player_get_err_msg2(int err);

/**
 * @brief 创建一个播放器会话
 * @param url 要播放的文件路径
 * @param session 用于接收会话指针的指针
 * @return 错误代码
*/
PLAYER_API int player_create(const char* url, PlayerSession** session);

/**
 * @brief 创建一个播放器会话
 * @param url 要播放的文件路径
 * @param session 用于接收会话指针的指针
 * @param settings 播放器设置，如果为NULL会使用默认设置。注意，传入的settings需要在session销毁前保持有效，需要手动调用 player_settings_free 。
 * @return 错误代码
*/
PLAYER_API int player_create2(const char* url, PlayerSession** session, PlayerSettings* settings);
PLAYER_API void player_free(PlayerSession** session);

/**
 * @brief 初始化播放器设置，会自动设置为默认值
 * @return 播放器设置指针
*/
PLAYER_API PlayerSettings* player_settings_init();
/**
 * @brief 将播放器设置重置为默认值
 * @param settings 播放器设置指针
*/
PLAYER_API void player_settings_default(PlayerSettings* settings);
/**
 * @brief 设置是否自动调整窗口大小
 * @param settings 播放器设置指针
 * @param resize 是否自动调整窗口大小
*/
PLAYER_API void player_settings_set_resize(PlayerSettings* settings, unsigned char resize);
PLAYER_API void player_settings_free(PlayerSettings** settings);

/**
 * @brief 直接播放一个文件，会创建一个新的窗口并自动播放，播放结束后自动销毁。
 * 
 * 会阻塞当前线程。
 * @param filename 要播放的文件路径
 */
PLAYER_API void play(const char* filename);

#ifdef __cplusplus
}
#endif
#endif
