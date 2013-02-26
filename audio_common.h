#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

typedef struct {
    void* playback_handle;
    void* volume_handle;
} audiodata_t;

audiodata_t* init_audiodata_t(void* playback_handle, void* vol_handle);
#endif
