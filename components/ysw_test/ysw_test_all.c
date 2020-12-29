// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_string.h"
#include "esp_log.h"
#include "assert.h"
#include "string.h"

#define TAG "YSW_TEST_ALL"

void ysw_test_all()
{
    void ysw_test_zm_generate_scales(void);
    ysw_test_zm_generate_scales();

    extern void ysw_test_ysw_string_shift(void);
    ysw_test_ysw_string_shift();
}

