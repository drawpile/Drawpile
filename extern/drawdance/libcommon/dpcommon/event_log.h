/*
 * Copyright (c) 2023 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef DPCOMMON_EVENT_LOG_H
#define DPCOMMON_EVENT_LOG_H
#include "common.h"

typedef struct DP_Output DP_Output;

#define DP_EVENT_LOG(...)                    \
    do {                                     \
        if (DP_event_log_is_open()) {        \
            DP_event_log_write(__VA_ARGS__); \
        }                                    \
    } while (0)

bool DP_event_log_open(DP_Output *output);
bool DP_event_log_close(void);

DP_INLINE bool DP_event_log_is_open(void)
{
    extern DP_Output *DP_event_log_output;
    return DP_event_log_output;
}

void DP_event_log_write_meta(const char *fmt, ...) DP_FORMAT(1, 2);

void DP_event_log_write(const char *fmt, ...) DP_FORMAT(1, 2);

#endif
