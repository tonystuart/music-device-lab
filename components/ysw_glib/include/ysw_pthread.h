// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "pthread.h"

int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param);

int pthread_condattr_destroy (pthread_condattr_t *attr);
