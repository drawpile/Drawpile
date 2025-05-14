// SPDX-License-Identifier: GPL-3.0-or-later
#include "sql.h"
#include <dpcommon/common.h>
#include <sqlite3.h>
#include <sqlite3recover.h>


static void error_log_callback(DP_UNUSED void *user, int code,
                               const char *message)
{
    switch (code) {
    case SQLITE_NOTICE:
        DP_info("SQLite notice: %s", message);
        break;
    case SQLITE_WARNING:
        DP_warn("SQLite warning: %s", message);
        break;
    case SQLITE_SCHEMA:
        break; // This just means that a statement had to be re-prepared.
    default:
        DP_warn("SQLite error %d: %s (%s)", code, sqlite3_errstr(code),
                message);
        break;
    }
}

int DP_sql_init(void)
{
    static bool initialized;
    if (initialized) {
        return SQLITE_OK;
    }
    else {
        sqlite3_config(SQLITE_CONFIG_LOG, error_log_callback, NULL);
        sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
        int result = sqlite3_initialize();
        if (result == SQLITE_OK) {
            initialized = true;
        }
        else {
            DP_error_set("Error %d initializing SQLite: %s", result,
                         sqlite3_errstr(result));
        }
        return result;
    }
}


struct DP_SqlRecover {
    sqlite3 *db;
    sqlite3_recover *recover;
};

static void recovery_try_close(sqlite3 *db)
{
    int close_result = sqlite3_close(db);
    if (close_result != SQLITE_OK) {
        DP_warn("Error %d closing recovery database: %s", close_result,
                sqlite3_errstr(close_result));
    }
}

static bool recovery_set_config(sqlite3_recover *recover)
{
    int result = sqlite3_recover_config(recover, SQLITE_RECOVER_LOST_AND_FOUND,
                                        "__lost_and_found");
    if (result != SQLITE_OK) {
        DP_error_set("Error setting lost and found recovery option: %s",
                     sqlite3_errstr(result));
        return false;
    }

    result = sqlite3_recover_config(recover, SQLITE_RECOVER_FREELIST_CORRUPT,
                                    (void *)1);
    if (result != SQLITE_OK) {
        DP_error_set("Error setting freelist corrupt recovery option: %s",
                     sqlite3_errstr(result));
        return false;
    }

    result = sqlite3_recover_config(recover, SQLITE_RECOVER_ROWIDS, (void *)0);
    if (result != SQLITE_OK) {
        DP_error_set("Error setting recover rowids recovery option: %s",
                     sqlite3_errstr(result));
        return false;
    }

    return true;
}

DP_SqlRecover *DP_sql_recover_new(const char *src_path, const char *dst_path)
{
    DP_ASSERT(src_path);
    DP_ASSERT(dst_path);
    if (DP_sql_init() != SQLITE_OK) {
        return NULL;
    }

    sqlite3 *db;
    int open_result = sqlite3_open_v2(src_path, &db, 0, NULL);
    if (open_result != SQLITE_OK) {
        DP_error_set("Error %d opening database '%s' for recovery: %s",
                     open_result, src_path, sqlite3_errstr(open_result));
        return NULL;
    }

    sqlite3_recover *recover = sqlite3_recover_init(db, "main", dst_path);
    if (!recover) {
        DP_error_set("Error starting database recovery");
        recovery_try_close(db);
        return NULL;
    }

    if (!recovery_set_config(recover)) {
        sqlite3_recover_finish(recover);
        recovery_try_close(db);
        return NULL;
    }

    DP_SqlRecover *r = DP_malloc(sizeof(*r));
    *r = (DP_SqlRecover){db, recover};
    return r;
}

void DP_sql_recover_free(DP_SqlRecover *r)
{
    if (r) {
        sqlite3_recover_finish(r->recover);
        recovery_try_close(r->db);
        DP_free(r);
    }
}

bool DP_sql_recover_step(DP_SqlRecover *r, bool *out_error)
{
    DP_ASSERT(r);
    int recover_result = sqlite3_recover_step(r->recover);
    bool done, error;
    if (recover_result == SQLITE_OK) {
        done = false;
        error = false;
    }
    else if (recover_result == SQLITE_DONE) {
        done = true;
        error = false;
    }
    else {
        done = true;
        error = true;
        const char *message = sqlite3_recover_errmsg(r->recover);
        DP_error_set("Error %d recovering database: %s", recover_result,
                     message ? message : sqlite3_errstr(recover_result));
    }

    if (out_error) {
        *out_error = error;
    }
    return done;
}
