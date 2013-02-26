#include <stdlib.h>
#include "audio_common.h"

audiodata_t* init_audiodata_t(void* pHandle, void* vHandle)
{
    audiodata_t* ret=malloc(sizeof(audiodata_t));
    ret->playback_handle=pHandle;
    ret->volume_handle=vHandle;
    return ret;
}
