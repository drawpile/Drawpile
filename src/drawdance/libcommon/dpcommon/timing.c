// SPDX-License-Identifier: GPL-3.0-or-later
#include "timing.h"
#ifdef _WIN32
#    include <windows.h>
#else
#    include <sys/time.h>
#    include <unistd.h>
#endif

#ifdef _WIN32
typedef VOID(WINAPI *DP_GetSystemTimePreciseAsFileTimeFn)(LPFILETIME);
static DP_GetSystemTimePreciseAsFileTimeFn get_system_time_precise_as_file_time;
#endif

void DP_time_init(void)
{
#ifdef _WIN32
    if (!get_system_time_precise_as_file_time) {
        HMODULE module = GetModuleHandleA("kernel32.dll");
        if (module) {
            get_system_time_precise_as_file_time =
                reinterpret_cast<DP_GetSystemTimePreciseAsFileTimeFn>(
                    GetProcAddress(module, "GetSystemTimePreciseAsFileTime"));
        }
    }
#endif
}

long long DP_time_unix_msec(void)
{
#ifdef _WIN32
    FILETIME ft;
    if (get_system_time_precise_as_file_time) {
        get_system_time_precise_as_file_time(&ft);
    }
    else {
        GetSystemTimeAsFileTime(&ft);
    }

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
