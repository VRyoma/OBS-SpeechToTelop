#include <obs-module.h>
#include "audio-filter.h"
#include "telop-source.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("SpeechToTelop", "en-US")

bool obs_module_load()
{
    audio_filter_register();
    telop_source_register();
    return true;
}

void obs_module_unload() {}
