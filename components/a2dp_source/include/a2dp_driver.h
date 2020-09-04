
#pragma once

#include "fluid_sys.h"
#include "fluid_adriver.h"
#include "fluid_settings.h"

fluid_audio_driver_t *new_a2dp_audio_driver(fluid_settings_t *settings, fluid_synth_t *synth);
void delete_a2dp_audio_driver(fluid_audio_driver_t *p);
