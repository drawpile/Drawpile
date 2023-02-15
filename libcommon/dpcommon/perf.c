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
#include "perf.h"
#include "atomic.h"
#include "common.h"
#include "conversions.h"
#include "output.h"
#include "threading.h"
#include "time.h"
#include "vector.h"


#define INITIAL_CAPACITY 64
#define DETAIL_LENGTH    256

typedef struct DP_PerfTime {
    long long seconds;
    long nanoseconds;
} DP_PerfTime;

typedef struct DP_PerfEntry {
    DP_PerfTime start;
    const char *realm;
    const char *categories;
    char detail[DETAIL_LENGTH];
} DP_PerfEntry;

static DP_Mutex *perf_mutex;
static DP_Vector perf_entries;

DP_Output *DP_perf_output;

static bool init_mutex(void)
{
    DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(perf_lock);
    if (!perf_mutex) {
        DP_atomic_lock(&perf_lock);
        if (!perf_mutex) {
            perf_mutex = DP_mutex_new();
        }
        DP_atomic_unlock(&perf_lock);
    }
    return perf_mutex;
}

static bool close_perf(DP_Output *output)
{
    bool ok = DP_output_free(output);
    if (!ok) {
        DP_error_set("Error closing perf output: %s", DP_error());
    }
    DP_vector_dispose(&perf_entries);
    return ok;
}

bool DP_perf_open(DP_Output *output)
{
    if (!output) {
        DP_error_set("Given output is null");
        return false;
    }

    if (!init_mutex()) {
        DP_output_free(output);
        return false;
    }

    DP_MUTEX_MUST_LOCK(perf_mutex);
    DP_Output *prev_output = DP_perf_output;
    if (prev_output && !close_perf(prev_output)) {
        DP_warn("Reopen perf output: %s", DP_error());
    }
    DP_perf_output = output;
    DP_VECTOR_INIT_TYPE(&perf_entries, DP_PerfEntry, INITIAL_CAPACITY);
    DP_MUTEX_MUST_UNLOCK(perf_mutex);

    return DP_OUTPUT_PRINT_LITERAL(
        output, "# Drawdance performance recording v" DP_PERF_VERSION "\n"
                "# <thread_id> <start_seconds>.<start_nanoseconds> "
                "<end_seconds>.<end_nanoseconds> <diff_nanoseconds> <category> "
                "<details...>\n");
}

bool DP_perf_close(void)
{
    if (!init_mutex()) {
        return false;
    }

    DP_MUTEX_MUST_LOCK(perf_mutex);
    bool ok;
    DP_Output *output = DP_perf_output;
    if (output) {
        ok = close_perf(output);
        DP_perf_output = NULL;
    }
    else {
        ok = false;
        DP_error_set("Perf output is not open");
    }
    DP_MUTEX_MUST_UNLOCK(perf_mutex);
    return ok;
}


bool DP_perf_is_open(void)
{
    return perf_mutex && DP_perf_output;
}


static bool is_available_perf_entry(void *entry, DP_UNUSED void *user)
{
    DP_PerfEntry *pe = entry;
    return pe->realm == NULL;
}

static DP_PerfEntry *perf_entry_at(int handle)
{
    return DP_vector_at(&perf_entries, sizeof(DP_PerfEntry),
                        DP_int_to_size(handle));
}

static DP_PerfEntry *search_or_push_perf_entry(int *out_handle)
{
    int handle = DP_vector_search_index(&perf_entries, sizeof(DP_PerfEntry),
                                        is_available_perf_entry, NULL);
    if (handle == -1) {
        *out_handle = DP_size_to_int(perf_entries.used);
        return DP_vector_push(&perf_entries, sizeof(DP_PerfEntry));
    }
    else {
        *out_handle = handle;
        return perf_entry_at(handle);
    }
}

static DP_PerfTime get_time(void)
{
#ifdef _WIN32
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    uint64_t ns = ticks.QuadPart * 1000000000 / freq.QuadPart;
    return (DP_PerfTime){ns / 1000000000, ns % 1000000000};
#else
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) != 0) {
        return (DP_PerfTime){ts.tv_sec, ts.tv_nsec};
    }
    else {
        DP_warn("Could not get perf time");
        return (DP_PerfTime){0, 0};
    }
#endif
}

int DP_perf_begin_internal(const char *realm, const char *categories,
                           const char *fmt, va_list ap)
{
    DP_ASSERT(realm);
    DP_ASSERT(categories);
    DP_ASSERT(perf_mutex);
    DP_MUTEX_MUST_LOCK(perf_mutex);
    int handle;
    DP_PerfEntry *pe = search_or_push_perf_entry(&handle);
    pe->start = get_time();
    pe->realm = realm;
    pe->categories = categories;
    if (fmt) {
        vsnprintf(pe->detail, DETAIL_LENGTH, fmt, ap);
    }
    else {
        pe->detail[0] = '\0';
    }
    DP_MUTEX_MUST_UNLOCK(perf_mutex);
    return handle;
}

void DP_perf_end_internal(DP_Output *output, int handle)
{
    DP_ASSERT(output);
    DP_ASSERT(handle != DP_PERF_INVALID_HANDLE);
    DP_ASSERT(perf_mutex);
    DP_PerfTime end = get_time();

    DP_MUTEX_MUST_LOCK(perf_mutex);
    DP_PerfEntry *pe = perf_entry_at(handle);
    DP_PerfTime start = pe->start;
    char *detail = pe->detail;

    long long diff = (end.seconds * 1000000000 + end.nanoseconds)
                   - (start.seconds * 1000000000 + start.nanoseconds);

    bool ok = DP_output_format(
        output, "%llu %lld.%09ld %lld.%09ld %llu %s:%s%s%s\n",
        DP_thread_current_id(), start.seconds, start.nanoseconds, end.seconds,
        end.nanoseconds, diff, pe->realm, pe->categories,
        detail[0] == '\0' ? "" : " ", detail);
    if (!ok) {
        DP_warn("Error formatting perf output: %s", DP_error());
    }

    pe->realm = NULL;
    DP_MUTEX_MUST_UNLOCK(perf_mutex);
}
