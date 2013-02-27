#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H

typedef struct {
    void* playback_handle;
    void* volume_handle;
} audiodata_t;

audiodata_t* init_audiodata_t(void* playback_handle, void* vol_handle);

// PACKET_DELAY is the time of delay receiving packet in order to fight overrun, it approximately equals to 1 frame sent to the sound device. (1 frame is around 362/363 samples), hence 362/44100 (sampling rate) *1,000,000,000, in nanoseconds
#define PACKET_DELAY 15000000

#define DELAY_ADJUST 100000
#endif
