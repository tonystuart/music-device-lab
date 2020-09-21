// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "esp_system.h"
#include "stdlib.h"
#include "time.h"

uint32_t esp_random()
{
    static unsigned int seed;
    if (!seed) {
        seed = time(NULL);
    }
    return rand_r(&seed);
}
