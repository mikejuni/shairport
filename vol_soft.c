#define ALSA_PCM_NEW_HW_PARAMS_API
#include "vol.h"
#include <stdarg.h>
#include <syslog.h>


void parse_vol_arg(char* a)
{
}

void init_volume_ctl()
{
    syslog(LOG_INFO,"SOFT: Use soft volume\n");
}

void deinit_volume_ctl()
{
}

void set_volume(double vol)
{
    syslog(LOG_INFO,"SOFT: Setting volume\n");
}

void print_vol_args(){
}
