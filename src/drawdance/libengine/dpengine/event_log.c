// SPDX-License-Identifier: GPL-3.0-or-later
#include "event_log.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/threading.h>
#include <dpdb/sql.h>

// On Darwin and old Android libc, we use gettimeofday. On normal systems and
// Windows, we use the standard timespec_get.
#if defined(__APPLE__) || (defined(__ANDROID__) && __ANDROID_API__ < 29)
#    include <sys/time.h>
#else
#    include "time.h"
#endif


static DP_Mutex *event_log_mutex;
static sqlite3_stmt *event_log_stmt;
static long long event_log_id;

void *DP_event_log_output;

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

static bool open_event_log(const char *path)
{
    sqlite3 *db;
    int result = sqlite3_open_v2(
        path, &db,
        SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_EXRESCODE
            | SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
        NULL);
    if (result != SQLITE_OK) {
        DP_error_set("Error opening '%s': %s", path, sqlite3_errstr(result));
        return false;
    }

    char *errmsg;
    result = sqlite3_exec(db, "pragma journal_mode = wal", NULL, NULL, &errmsg);
    if (result != SQLITE_OK) {
        DP_warn("Error setting journal_mode = wal: %s (%s)",
                sqlite3_errstr(result), errmsg ? errmsg : "NULL");
        sqlite3_free(errmsg);
    }

    DP_sql_clear(db);

    const char *create_table_sql = "create table if not exists event_log (\n"
                                   "    id integer primary key not null,\n"
                                   "    seconds integer,\n"
                                   "    nanoseconds integer,\n"
                                   "    message text not null)\n"
                                   "strict";
    result = sqlite3_exec(db, create_table_sql, NULL, NULL, &errmsg);
    if (result != SQLITE_OK) {
        DP_error_set("Error creating event log table: %s (%s)",
                     sqlite3_errstr(result), errmsg ? errmsg : "NULL");
        sqlite3_free(errmsg);
        return false;
    }

    const char *insert_sql =
        "insert into event_log (id, seconds, nanoseconds, message)\n"
        "values (?, ?, ?, ?)";
    result = sqlite3_prepare_v3(db, insert_sql, -1, SQLITE_PREPARE_PERSISTENT,
                                &event_log_stmt, NULL);
    if (result != SQLITE_OK) {
        DP_error_set("Error preparing event log statement: %s",
                     sqlite3_errstr(result));
        sqlite3_close_v2(db);
        return false;
    }

    DP_event_log_output = db;
    event_log_id = 1LL;
    return true;
}

static bool close_event_log(void)
{
    int result = sqlite3_finalize(event_log_stmt);
    if (result != SQLITE_OK) {
        DP_warn("Error finalizing event log statement: %s",
                sqlite3_errstr(result));
    }


    result = sqlite3_close(DP_event_log_output);
    DP_event_log_output = NULL;
    if (result != SQLITE_OK) {
        DP_error_set("Error closing event log output: %s",
                     sqlite3_errstr(result));
        return false;
    }

    return true;
}

bool DP_event_log_open(const char *path)
{
    if (!path) {
        DP_error_set("Given output is null");
        return false;
    }

    if (!init_mutex()) {
        return false;
    }

    if (DP_sql_init() != SQLITE_OK) {
        return false;
    }

    DP_MUTEX_MUST_LOCK(event_log_mutex);
    if (DP_event_log_output && !close_event_log()) {
        DP_warn("Reopen event log output: %s", DP_error());
    }
    bool ok = open_event_log(path);
    DP_MUTEX_MUST_UNLOCK(event_log_mutex);

    return ok;
}

bool DP_event_log_close(void)
{
    if (!init_mutex()) {
        return false;
    }

    DP_MUTEX_MUST_LOCK(event_log_mutex);
    bool ok;
    if (DP_event_log_output) {
        ok = close_event_log();
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

static void write_log(bool timestamped, const char *fmt, va_list ap)
    DP_VFORMAT(2);

static void write_log(bool timestamped, const char *fmt, va_list ap)
{
    char *text = DP_vformat(fmt, ap);
    DP_MUTEX_MUST_LOCK(event_log_mutex);

    sqlite3_stmt *stmt = event_log_stmt;
    sqlite3_bind_int64(stmt, 1, event_log_id++);
    if (timestamped) {
        long long seconds;
        long nanoseconds;
        if (!get_time(&seconds, &nanoseconds)) {
            seconds = -1LL;
            nanoseconds = -1L;
        }
        sqlite3_bind_int64(stmt, 2, seconds);
        sqlite3_bind_int64(stmt, 3, nanoseconds);
    }
    else {
        sqlite3_bind_null(stmt, 2);
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_text(stmt, 4, text, -1, DP_free);

    int result = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    DP_MUTEX_MUST_UNLOCK(event_log_mutex);

    if (result != SQLITE_DONE) {
        DP_warn("Error executing event log statement: %s",
                sqlite3_errstr(result));
    }
}

void DP_event_log_write_meta(const char *fmt, ...)
{
    DP_ASSERT(DP_event_log_output);
    DP_ASSERT(event_log_mutex);
    va_list ap;
    va_start(ap, fmt);
    write_log(false, fmt, ap);
    va_end(ap);
}

void DP_event_log_write(const char *fmt, ...)
{
    DP_ASSERT(DP_event_log_output);
    DP_ASSERT(event_log_mutex);
    va_list ap;
    va_start(ap, fmt);
    write_log(true, fmt, ap);
    va_end(ap);
}
