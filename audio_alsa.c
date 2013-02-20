#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <math.h>
#include "common.h"

#define NUM_CHANNELS 2

static snd_pcm_t *alsa_handle = NULL;
static snd_pcm_hw_params_t *alsa_params = NULL;
static char* DEFAULT_CARD = "default";
static char* DEFAULT_MIXER = "Master";

#ifdef USE_ALSA_VOLUME
static snd_mixer_t *mixer_handle = NULL;
static snd_mixer_selem_id_t *mixer_master = NULL;
static snd_mixer_elem_t *volume_handle = NULL;
static long g_vol_max, g_vol_min;
static long g_prev_vol = 0;
static char* g_card = NULL;
static char* g_mixer = NULL;
#endif

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
    if (strlen(device_name)!=0)
    {
        g_mixer=device_name;
    } else {
	g_mixer=DEFAULT_MIXER;
    }
    fprintf(stderr, "ALSA: audio_set_device_name: this sets the output in mixer to :%s\n",g_mixer);
}

void audio_set_device_id(char* device_id) {
    fprintf(stderr, "audio_set_device_id: not supported with alsa :%s\n",device_id);
}

char* audio_get_driver(void)
{
    return g_card;
}

char* audio_get_device_name(void)
{
    return g_mixer;
}

char* audio_get_device_id(void)
{
    return NULL;
}

void audio_play(char* outbuf, int samples, void* priv_data)
{
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

#ifdef USE_ALSA_VOLUME
    fprintf(stderr,"ALSA: Use ALSA Mixer rather than soft manupulating volume\n");
    snd_mixer_open(&mixer_handle, 0);
    snd_mixer_attach(mixer_handle, g_card);
    snd_mixer_selem_register(mixer_handle,NULL,NULL);
    snd_mixer_load(mixer_handle);
    snd_mixer_selem_id_alloca(&mixer_master);
    snd_mixer_selem_id_set_index(mixer_master,0);
    snd_mixer_selem_id_set_name(mixer_master, g_mixer);
    volume_handle = snd_mixer_find_selem(mixer_handle, mixer_master);
    snd_mixer_selem_get_playback_volume_range (volume_handle, &g_vol_min, &g_vol_max);
    fprintf(stderr,"ALAS: Mixer volume from %d to %d\n",(int)g_vol_min, (int)g_vol_max);
#endif
    return NULL;
}

void audio_deinit(void)
{
    if (alsa_handle) {
        snd_pcm_drain(alsa_handle);
        snd_pcm_close(alsa_handle);
    }

#ifdef USE_ALSA_VOLUME
    if (mixer_handle)
    {
        snd_mixer_close(mixer_handle);
    }
#endif
}

void audio_set_volume(double vol)
{
#ifdef USE_ALSA_VOLUME
//    long cur_vol=(MAX_AIRPLAY_VOL+vol)/MAX_AIRPLAY_VOL*g_vol_max;
    long cur_vol=(long)(pow(10.0,0.05*vol)*(double)g_vol_max+0.5);
#ifdef DEBUGALSA
    fprintf(stderr,"ALSA: Setting volume in ALSA %f with volume %d\n",vol,(int)cur_vol);
#endif
    if (cur_vol!=g_prev_vol)
    {
#ifdef DEBUGALSA
        fprintf(stderr,"ALSA: Calling function to set volume\n");
#endif
    	snd_mixer_selem_set_playback_volume_all(volume_handle,cur_vol);
        g_prev_vol=cur_vol;
    }
#endif
}
