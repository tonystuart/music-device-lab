// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "string.h"

void *ysw_heap_allocate(size_t size);
void *ysw_heap_reallocate(void *old_p, size_t size);
char *ysw_heap_strdup(char *source);
void ysw_heap_free(void *p);
