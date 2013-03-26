#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <math.h>
#include "vol.h"
#include <stdarg.h>
#include <syslog.h>

//static char* DEFAULT_CARD = "default";
static snd_mixer_t *mixer_handle = NULL;
static snd_mixer_selem_id_t *mixer_master = NULL;
static snd_mixer_elem_t *volume_handle = NULL;
static long g_vol_max, g_vol_min;
static long g_prev_vol = 0;
/*
static char* g_mixer = NULL;
static char* g_ctl = NULL;
*/
static char g_mixer[56]="Master";
static char g_ctl[56]="default";
//static char* DEFAULT_MIXER = "Master";

void parse_vol_arg(char* arg)
{
    if (!strncmp(arg, "--alsa_ctl=",11))
    {
        strncpy(g_ctl, arg+11, 55);
    }
    if (!strncmp(arg, "--alsa_volume=",14))
    {
        strncpy(g_mixer, arg+14, 55);
    }
}

/*
void set_volume_param(char* params, ...)
{
    char* ctl;
    char* mixer;
    va_list arguments;
    ctl=params;
    va_start (arguments, params);
    mixer=va_arg(arguments, char*);
    va_end (arguments);
    if (ctl!=NULL && strlen(ctl)!=0)
        g_ctl=ctl;
    else
        g_ctl=DEFAULT_CARD;
    if (mixer!=NULL && strlen(mixer)!=0)
        g_mixer=mixer;
    else
        g_mixer=DEFAULT_MIXER;
}
*/

void init_volume_ctl()
{
    syslog(LOG_INFO,"ALSA_VOL: Setting ALSA volume control %s on %s\n",g_ctl,g_mixer);
    snd_mixer_open(&mixer_handle, 0);
    snd_mixer_attach(mixer_handle, g_ctl);
    snd_mixer_selem_register(mixer_handle,NULL,NULL);
    snd_mixer_load(mixer_handle);
    snd_mixer_selem_id_alloca(&mixer_master);
    snd_mixer_selem_id_set_index(mixer_master,0);
    snd_mixer_selem_id_set_name(mixer_master, g_mixer);
    volume_handle = snd_mixer_find_selem(mixer_handle, mixer_master);
    snd_mixer_selem_get_playback_volume_range (volume_handle, &g_vol_min, &g_vol_max);
    syslog(LOG_INFO,"ALSA: Mixer volume from %d to %d\n",(int)g_vol_min, (int)g_vol_max);
}

void deinit_volume_ctl()
{
    if (mixer_handle)
    {
        snd_mixer_close(mixer_handle);
    }
}

void set_volume(double vol)
{
    long cur_vol=(long)(pow(10.0,0.05*vol)*(double)g_vol_max+0.5);
#ifdef DEBUGALSAVOL
    syslog(LOG_INFO,"ALSA: Setting volume in ALSA %f with volume %d\n",vol,(int)cur_vol);
#endif
    if (cur_vol!=g_prev_vol)
    {
#ifdef DEBUGALSAVOL
        syslog(LOG_DEBUG,"ALSA: Calling function to set volume\n");
#endif
    	snd_mixer_selem_set_playback_volume_all(volume_handle,cur_vol);
        g_prev_vol=cur_vol;
    }
}

void print_vol_args()
{
      printf("  --alsa_volume=<ALSA Volume Output>    Sets ALSA Mixer output device (Can be found by alsamixer)\n");
      printf("  --alsa_ctl=<ALSA Mixer Control>       Sets ALSA Control device (Can be found by alsamixer)\n");
}
