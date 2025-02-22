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
#include "event_log.h"
#include "atomic.h"
#include "common.h"
#include "output.h"
#include "threading.h"

// On Darwin and old Android libc, we use gettimeofday. On normal systems and
// Windows, we use the standard timespec_get.
#if defined(__APPLE__) || (defined(__ANDROID__) && __ANDROID_API__ < 29)
#    include <sys/time.h>
#else
#    include "time.h"
#endif


static DP_Mutex *event_log_mutex;

DP_Output *DP_event_log_output;

static bool init_mutex(void)
{
    DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(event_log_lock);
    if (!event_log_mutex) {
        DP_atomic_lock(&event_log_lock);
        if (!event_log_mutex) {
            event_log_mutex = DP_mutex_new();
        }
        DP_atomic_unlock(&event_log_lock);
    }
    return event_log_mutex;
}

static bool close_event_log(DP_Output *output)
{
    bool ok = DP_output_free(output);
    if (!ok) {
        DP_error_set("Error closing event log output: %s", DP_error());
    }
    return ok;
}

bool DP_event_log_open(DP_Output *output)
{
    if (!output) {
        DP_error_set("Given output is null");
        return false;
    }

    if (!init_mutex()) {
        DP_output_free(output);
        return false;
    }

    DP_MUTEX_MUST_LOCK(event_log_mutex);
    DP_Output *prev_output = DP_event_log_output;
    if (prev_output && !close_event_log(prev_output)) {
        DP_warn("Reopen event log output: %s", DP_error());
    }
    DP_event_log_output = output;
    DP_MUTEX_MUST_UNLOCK(event_log_mutex);

    return true;
}

bool DP_event_log_close(void)
{
    if (!init_mutex()) {
        return false;
    }

    DP_MUTEX_MUST_LOCK(event_log_mutex);
    bool ok;
    DP_Output *output = DP_event_log_output;
    if (output) {
        ok = close_event_log(output);
        DP_event_log_output = NULL;
    }
    else {
        ok = false;
        DP_error_set("Event log output is not open");
    }
    DP_MUTEX_MUST_UNLOCK(event_log_mutex);
    return ok;
}

static bool get_time(long long *out_seconds, long *out_nanoseconds)
{
#if defined(__APPLE__) || (defined(__ANDROID__) && __ANDROID_API__ < 29)
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        *out_seconds = (long long)tv.tv_sec;
        *out_nanoseconds = (long)tv.tv_usec * 1000L;
        return true;
    }
#else
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) != 0) {
        *out_seconds = ts.tv_sec;
        *out_nanoseconds = ts.tv_nsec;
        return true;
    }
#endif
    return false;
}

static bool write_log(bool timestamped, const char *fmt, va_list ap)
    DP_VFORMAT(2);

static bool write_log(bool timestamped, const char *fmt, va_list ap)
{
    DP_Output *output = DP_event_log_output;
    bool ok = true;
    DP_MUTEX_MUST_LOCK(event_log_mutex);
    if (timestamped) {
        long long seconds;
        long nanoseconds;
        if (get_time(&seconds, &nanoseconds)) {
            DP_output_format(output, "%lld.%ld ", seconds, nanoseconds);
        }
    }
    ok = ok && DP_output_vformat(output, fmt, ap)
      && DP_OUTPUT_PRINT_LITERAL(output, "\n") && DP_output_flush(output);
    DP_MUTEX_MUST_UNLOCK(event_log_mutex);
    return ok;
}

void DP_event_log_write_meta(const char *fmt, ...)
{
    DP_ASSERT(DP_event_log_output);
    DP_ASSERT(event_log_mutex);
    va_list ap;
    va_start(ap, fmt);
    if (!write_log(false, fmt, ap)) {
        DP_warn("Error formatting event log metadata output: %s", DP_error());
    }
    va_end(ap);
}

void DP_event_log_write(const char *fmt, ...)
{
    DP_ASSERT(DP_event_log_output);
    DP_ASSERT(event_log_mutex);
    va_list ap;
    va_start(ap, fmt);
    if (!write_log(true, fmt, ap)) {
        DP_warn("Error formatting event log output: %s", DP_error());
    }
    va_end(ap);
}
