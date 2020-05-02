// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "lvgl.h"
#include "ysw_csn.h"
#include "ysw_cs.h"

typedef struct {
    lv_obj_t *win;
    lv_obj_t *kb;
} ysw_sdb_t;

typedef enum {
    YSW_SDB_STRING,
    YSW_SDB_NUMBER,
    YSW_SDB_CHOICE,
} ysw_sdb_type_t;

typedef struct {
    char *original;
} ysw_sdb_string_t;

typedef struct {
    ysw_sdb_t *sdb;
    char *current;
    uint8_t index;
} ysw_sdb_string_data_t;

typedef struct {
    int original;
} ysw_sdb_number_t;

typedef struct {
    uint8_t index;
} ysw_sdb_number_data_t;

typedef struct {
    int original;
    char *options;
} ysw_sdb_choice_t;

typedef struct {
    ysw_sdb_t *sdb;
    int current;
    uint8_t index;
} ysw_sdb_choice_data_t;

typedef struct {
    union {
        ysw_sdb_string_t string;
        ysw_sdb_number_t number;
        ysw_sdb_choice_t choice;
    };
} ysw_sdb_value_t;

typedef struct {
    char *name;
    ysw_sdb_type_t type;
    ysw_sdb_value_t value;
} ysw_sdb_field_t;

typedef struct {
    ysw_sdb_field_t *fields;
    uint8_t field_count;
} ysw_sdb_config_t;

ysw_sdb_t *ysw_sdb_create(ysw_cs_t *cs, ysw_sdb_field_t fields[], uint8_t field_count);

void ysw_sdb_close(ysw_sdb_t *sdb);
