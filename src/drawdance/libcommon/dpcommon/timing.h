// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPCOMMON_TIMING_H
#define DPCOMMON_TIMING_H

// Necessary on Windows to load GetSystemTimePreciseAsFileTime. Will fall back
// to the non-precise variant if not called. Windows 7 or older doesn't have it.
void DP_time_init(void);

// Current UTC Unix timestamp in milliseconds.
long long DP_time_unix_msec(void);

#endif
