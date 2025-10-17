// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPCOMMON_EVENT_LOG_H
#define DPCOMMON_EVENT_LOG_H
#include <dpcommon/common.h>

#define DP_EVENT_LOG(...)                    \
    do {                                     \
        if (DP_event_log_is_open()) {        \
            DP_event_log_write(__VA_ARGS__); \
        }                                    \
    } while (0)

bool DP_event_log_open(const char *path);
bool DP_event_log_close(void);

DP_INLINE bool DP_event_log_is_open(void)
{
    extern void *DP_event_log_output;
    return DP_event_log_output;
}

void DP_event_log_write_meta(const char *fmt, ...) DP_FORMAT(1, 2);

void DP_event_log_write(const char *fmt, ...) DP_FORMAT(1, 2);

#endif
