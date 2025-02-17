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
