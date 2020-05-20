// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

extern const char *ysw_division;

uint8_t ysw_division_from_index(uint8_t index);
uint8_t ysw_division_to_index(int8_t divisions);
const char *ysw_division_to_tick(uint8_t divisions);
