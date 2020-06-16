// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_spiffs.h"

#include "ysw_common.h"

#include "esp_log.h"
#include "esp_spiffs.h"

#define TAG "YSW_SPIFFS"

void ysw_spiffs_initialize(const char *base_path)
{
    esp_vfs_spiffs_conf_t config = {
        .base_path = base_path,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    $(esp_vfs_spiffs_register(&config));

    size_t total_size = 0;
    size_t amount_used = 0;
    $(esp_spiffs_info(config.partition_label, &total_size, &amount_used));
    ESP_LOGD(TAG, "ysw_spiffs_initialize total_size=%d, amount_used=%d", total_size, amount_used);
}

