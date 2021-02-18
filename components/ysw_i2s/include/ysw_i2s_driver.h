// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "fluid_sys.h"
#include "fluid_adriver.h"
#include "fluid_settings.h"

fluid_audio_driver_t *ysw_i2s_driver_new(fluid_settings_t *settings, fluid_synth_t *synth);
void ysw_i2s_driver_delete(fluid_audio_driver_t *p);
