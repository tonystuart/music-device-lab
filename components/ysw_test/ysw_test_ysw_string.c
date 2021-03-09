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

#define TAG "YSW_TEST_STRING"

void ysw_test_ysw_string_shift()
{
    // abcdef
    // 0, 10 -> "          abcdef"
    // 2, 10 -> "ab          cdef"
    // 2,  2 -> "ab  cdef"
    // 6, 10 -> "abcdef          "
    // 10, 6 -> error (max index is 6)

    // 1, -1 -> "bcdef"
    // 0, -2 -> "cdef"
    // 2, -2 -> "abef"
    // 10, -6 -> error (max index is 6)
    // 2 -10 -> error (max left shift amount at index 2 is 4)
    // 6, -2 -> error (max left shift amount at index 6 is 0)

    ysw_string_t *s = ysw_string_create(16);

    ysw_string_set_chars(s, "abcdef");
    ysw_string_shift(s, 0, 10);
    ESP_LOGD(TAG, "s=%s, length=%d", ysw_string_get_chars(s), ysw_string_get_length(s));
    assert(strcmp(ysw_string_get_chars(s), "          abcdef") == 0);
    assert(ysw_string_get_length(s) == 16);

    ysw_string_set_chars(s, "abcdef");
    ysw_string_shift(s, 2, 10);
    ESP_LOGD(TAG, "s=%s, length=%d", ysw_string_get_chars(s), ysw_string_get_length(s));
    assert(strcmp(ysw_string_get_chars(s), "ab          cdef") == 0);
    assert(ysw_string_get_length(s) == 16);

    ysw_string_set_chars(s, "abcdef");
    ysw_string_shift(s, 2, 2);
    ESP_LOGD(TAG, "s=%s, length=%d", ysw_string_get_chars(s), ysw_string_get_length(s));
    assert(strcmp(ysw_string_get_chars(s), "ab  cdef") == 0);
    assert(ysw_string_get_length(s) == 8);

    ysw_string_set_chars(s, "abcdef");
    ysw_string_shift(s, 0, -1);
    ESP_LOGD(TAG, "s=%s, length=%d", ysw_string_get_chars(s), ysw_string_get_length(s));
    assert(strcmp(ysw_string_get_chars(s), "bcdef") == 0);
    assert(ysw_string_get_length(s) == 5);

    ysw_string_set_chars(s, "abcdef");
    ysw_string_shift(s, 0, -2);
    ESP_LOGD(TAG, "s=%s, length=%d", ysw_string_get_chars(s), ysw_string_get_length(s));
    assert(strcmp(ysw_string_get_chars(s), "cdef") == 0);
    assert(ysw_string_get_length(s) == 4);

    ysw_string_set_chars(s, "abcdef");
    ysw_string_shift(s, 2, -2);
    ESP_LOGD(TAG, "s=%s, length=%d", ysw_string_get_chars(s), ysw_string_get_length(s));
    assert(strcmp(ysw_string_get_chars(s), "abef") == 0);
    assert(ysw_string_get_length(s) == 4);
}
