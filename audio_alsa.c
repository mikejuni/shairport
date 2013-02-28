#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <math.h>
#include "common.h"

#define NUM_CHANNELS 2

static snd_pcm_t *alsa_handle = NULL;
static snd_pcm_hw_params_t *alsa_params = NULL;
static char* DEFAULT_CARD = "default";

static char* g_card = NULL;

void audio_set_driver(char* driver) {
    if (strlen(driver)!=0)
    {
        g_card=driver;
    } else {
        g_card=DEFAULT_CARD;
    }
    fprintf(stderr, "ALSA: audio_set_driver: this sets the PCM device to :%s\n",g_card);
}

void audio_set_device_name(char* device_name) {
    fprintf(stderr, "ALSA: audio_set_device_name: not supported\n");
}

void audio_set_device_id(char* device_id) {
    fprintf(stderr, "ALSA: audio_set_device_id: not supported with alsa :%s\n",device_id);
}

char* audio_get_driver(void)
{
    return g_card;
}

char* audio_get_device_name(void)
{
    return NULL;
}

char* audio_get_device_id(void)
{
    return NULL;
}

inline void audio_play(char* outbuf, int samples, void* priv_data)
{
#ifdef DEBUGALSA
    snd_pcm_sframes_t avail=snd_pcm_avail (alsa_handle);
    slog (LOG_DEBUG_VV,"AUDIO_PLAY: Available buffer in ALSA:%d, Sample size %d\n",(int)avail,samples);
#endif
    int err = snd_pcm_writei(alsa_handle, outbuf, samples);
    if (err < 0)
        err = snd_pcm_recover(alsa_handle, err, 0);
    if (err < 0)
        fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(err));
}

void* audio_init(int sampling_rate)
{
#ifdef DEBUGALSA
    fprintf(stderr,"ALSA: Sample rate proposed %d\n",sampling_rate);
#endif
    int rc, dir = 0;
    snd_pcm_uframes_t frames = 32;
//    rc = snd_pcm_open(&alsa_handle, g_card, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    rc = snd_pcm_open(&alsa_handle, g_card, SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        die("alsa initialization failed");
    }
    snd_pcm_hw_params_alloca(&alsa_params);
    snd_pcm_hw_params_any(alsa_handle, alsa_params);
    snd_pcm_hw_params_set_access(alsa_handle, alsa_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(alsa_handle, alsa_params, SND_PCM_FORMAT_S16);
    snd_pcm_hw_params_set_channels(alsa_handle, alsa_params, NUM_CHANNELS);
    snd_pcm_hw_params_set_rate_near(alsa_handle, alsa_params, (unsigned int *)&sampling_rate, &dir);
#ifdef DEBUGALSA
    fprintf(stderr,"ALSA: Sample rate gotten %d\n",sampling_rate);
#endif
    snd_pcm_hw_params_set_period_size_near(alsa_handle, alsa_params, &frames, &dir);
    rc = snd_pcm_hw_params(alsa_handle, alsa_params);
    if (rc < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        die("alsa initialization failed");
    }

    return alsa_handle;
}

void audio_deinit(void)
{
    slog(LOG_INFO,"ALSA: Deinitiating PCM handles\n");
    if (alsa_handle) {
        snd_pcm_drain(alsa_handle);
        snd_pcm_close(alsa_handle);
    }
}

