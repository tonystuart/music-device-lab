// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "stdint.h"

int ysw_name_create(char *name, uint32_t size);
int ysw_name_create_new_version(const char *old_name, char *new_name, uint32_t size);
int ysw_name_find_version_point(const char *name);
int ysw_name_format_version(char *new_name, uint32_t size, uint32_t version_point, const char *old_name, uint32_t version);
