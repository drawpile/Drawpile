// SPDX-License-Identifier: GPL-3.0-or-later
#include "timing.h"
#ifdef _WIN32
#    include <windows.h>
#else
#    include <sys/time.h>
#    include <unistd.h>
#endif


long long DP_time_unix_msec(void)
{
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);

    // Gotta unpack the timestamp.
    ULARGE_INTEGER li = {
        .LowPart = ft.dwLowDateTime,
        .HighPart = ft.dwHighDateTime,
    };
    unsigned long long ticks = li.QuadPart;

    // Windows uses 1601 as the epoch, this converts it to the Unix 1970 epoch.
    unsigned long long epoch_offset = 116444736000000000ULL;
    return (long long)((ticks - epoch_offset) / 10000ULL);
#else
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        return ((long long)tv.tv_sec * 1000LL)
             + ((long long)tv.tv_usec / 1000LL);
    }
    else {
        return 0LL;
    }
#endif
}
