// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "stdint.h"

const char *ysw_transposition =
"-11\n"
"-10\n"
"-9\n"
"-8\n"
"-7\n"
"-6\n"
"-5\n"
"-4\n"
"-3\n"
"-2\n"
"-1\n"
"0 (None)\n"
"+1\n"
"+2\n"
"+3\n"
"+4\n"
"+5\n"
"+6\n"
"+7\n"
"+8\n"
"+9\n"
"+10\n"
"+11";

uint8_t ysw_transposition_from_index(uint8_t index)
{
    return index - 11;
}

int8_t ysw_transposition_to_index(int8_t transposition)
{
    return transposition + 11;
}
