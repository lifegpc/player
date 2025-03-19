#include "open.h"

int open_input(PlayerSession* session, const char* url) {
    if (!session || !url) return PLAYER_ERR_NULLPTR;
    int re = 0;
    if ((re = avformat_open_input(&session->fmt, url, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open \"%s\": %s (%i)\n", url, av_err2str(re), re);
        return re;
    }
    if ((re = avformat_find_stream_info(session->fmt, NULL)) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to find streams in \"%s\": %s (%i)\n", url, av_err2str(re), re);
        return re;
    }
    return PLAYER_ERR_OK;
}

int find_audio_stream(PlayerSession* session) {
    if (!session) return PLAYER_ERR_NULLPTR;
    for (unsigned int i = 0; i < session->fmt->nb_streams; i++) {
        AVStream* is = session->fmt->streams[i];
        if (is->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (!avcodec_find_decoder(is->codecpar->codec_id)) {
                continue;
            }
            session->audio_input_stream = is;
            return PLAYER_ERR_OK;
        }
    }
    return PLAYER_ERR_NO_STREAM_OR_DECODER;
}

int find_video_stream(PlayerSession* session) {
    if (!session) return PLAYER_ERR_NULLPTR;
    for (unsigned int i = 0; i < session->fmt->nb_streams; i++) {
        AVStream* is = session->fmt->streams[i];
        if (is->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (!avcodec_find_decoder(is->codecpar->codec_id)) {
                continue;
            }
            session->video_input_stream = is;
            return PLAYER_ERR_OK;
        }
    }
    return PLAYER_ERR_NO_STREAM_OR_DECODER;
}
