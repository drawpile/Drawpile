// SPDX-License-Identifier: GPL-3.0-or-later
#include "project.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpcommon/threading.h>
#include <dpdb/sql.h>
#include <dpmsg/message.h>


#define DP_PROJECT_STR(X)  #X
#define DP_PROJECT_XSTR(X) DP_PROJECT_STR(X)


typedef enum DP_ProjectStatement {
    DP_PROJECT_STATEMENT_MESSAGE_RECORD,
    DP_PROJECT_STATEMENT_SESSION_OPEN,
    DP_PROJECT_STATEMENT_SESSION_CLOSE,
    DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGE_RECORD,
    DP_PROJECT_STATEMENT_SNAPSHOT_OPEN,
    DP_PROJECT_STATEMENT_SNAPSHOT_FINISH,
    DP_PROJECT_STATEMENT_SNAPSHOT_DISCARD,
    DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGES_DISCARD,
    DP_PROJECT_STATEMENT_SNAPSHOT_SELECT_EXCEPT,
    DP_PROJECT_STATEMENT_COUNT,
} DP_ProjectStatement;

struct DP_Project {
    sqlite3 *db;
    long long session_id;
    long long sequence_id;
    long long snapshot_id;
    long long snapshot_sequence_id;
    sqlite3_stmt *stmts[DP_PROJECT_STATEMENT_COUNT];
    unsigned char serialize_buffer[DP_MESSAGE_MAX_PAYLOAD_LENGTH];
};

static bool is_ok(int result)
{
    return result == SQLITE_OK;
}

static void assign_result(int result, int *out_result)
{
    if (out_result) {
        *out_result = result;
    }
}

static bool open_sql_ok(DP_ProjectOpenResult *result, int error)
{
    if (is_ok(result->sql_result)) {
        return true;
    }
    else {
        result->error = error;
        return false;
    }
}

static const char *db_error(sqlite3 *db)
{
    const char *message = db ? sqlite3_errmsg(db) : NULL;
    return message ? message : "unknown error";
}

static const char *fallback_db_error(sqlite3 *db, const char *errmsg)
{
    return errmsg ? errmsg : db_error(db);
}

static const char *prj_db_error(DP_Project *prj)
{
    return db_error(prj->db);
}

static void try_close_db(sqlite3 *db)
{
    int result = sqlite3_close(db);
    if (result != SQLITE_OK) {
        DP_warn("Error %d closing database: %s", result,
                sqlite3_errstr(result));
    }
}


int DP_project_check(const char *path)
{
    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        return DP_PROJECT_CHECK_ERROR_OPEN;
    }

    unsigned char buffer[72];
    bool error;
    size_t read = DP_input_read(input, buffer, sizeof(buffer), &error);
    DP_input_free(input);
    if (error) {
        return DP_PROJECT_CHECK_ERROR_READ;
    }

    if (read < sizeof(buffer)) {
        return DP_PROJECT_CHECK_ERROR_HEADER;
    }

    // "SQLite format 3\0" in ASCII
    unsigned char magic[] = {0x53, 0x51, 0x4c, 0x69, 0x74, 0x65, 0x20, 0x66,
                             0x6f, 0x72, 0x6d, 0x61, 0x74, 0x20, 0x33, 0x00};
    if (memcmp(buffer, magic, sizeof(magic)) != 0) {
        return DP_PROJECT_CHECK_ERROR_MAGIC;
    }

    int application_id = DP_read_bigendian_int32(buffer + 68);
    if (application_id != DP_PROJECT_APPLICATION_ID) {
        return DP_PROJECT_CHECK_ERROR_APPLICATION_ID;
    }

    int user_version = DP_read_bigendian_int32(buffer + 60);
    if (user_version < 1) {
        return DP_PROJECT_CHECK_ERROR_USER_VERSION;
    }

    return user_version;
}


static sqlite3_stmt *prepare(sqlite3 *db, const char *sql, unsigned int flags,
                             int *out_result)
{
    DP_debug("Prepare statement: %s", sql);
    sqlite3_stmt *stmt;
    int prepare_result = sqlite3_prepare_v3(db, sql, -1, flags, &stmt, NULL);
    if (is_ok(prepare_result)) {
        return stmt;
    }
    else {
        assign_result(prepare_result, out_result);
        DP_error_set("Error %d preparing statement '%s': %s", prepare_result,
                     sql, db_error(db));
        return NULL;
    }
}

static bool exec_write_stmt(sqlite3 *db, const char *sql, const char *title,
                            int *out_result)
{
    DP_ASSERT(db);
    DP_ASSERT(sql);
    DP_ASSERT(title);
    char *errmsg;
    int exec_result = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (is_ok(exec_result)) {
        return true;
    }
    else {
        assign_result(exec_result, out_result);
        DP_error_set("Error %d %s: %s", exec_result, title,
                     fallback_db_error(db, errmsg));
        sqlite3_free(errmsg);
        return false;
    }
}

static bool exec_int_stmt(sqlite3 *db, const char *sql, int default_value,
                          int *out_value, int *out_result)
{
    DP_ASSERT(db);
    DP_ASSERT(sql);
    DP_ASSERT(out_value);

    sqlite3_stmt *stmt = prepare(db, sql, 0, out_result);
    if (!stmt) {
        return false;
    }

    bool value_ok;
    int step_result = sqlite3_step(stmt);
    if (step_result == SQLITE_DONE) {
        value_ok = true;
        *out_value = default_value;
    }
    else if (step_result == SQLITE_ROW) {
        if (sqlite3_column_count(stmt) >= 1) {
            value_ok = true;
            *out_value = sqlite3_column_int(stmt, 0);
        }
        else {
            value_ok = false;
            DP_error_set("Statement '%s' resulted in no columns", sql);
        }
    }
    else {
        assign_result(step_result, out_result);
        value_ok = false;
        DP_error_set("Error %d executing statement '%s': %s", step_result, sql,
                     db_error(db));
    }

    sqlite3_finalize(stmt);
    return value_ok;
}

static bool exec_text_stmt(sqlite3 *db, const char *sql,
                           const char *default_value,
                           bool (*handle_text)(void *, const char *),
                           void *user, int *out_result)
{
    DP_ASSERT(db);
    DP_ASSERT(sql);
    DP_ASSERT(handle_text);

    sqlite3_stmt *stmt = prepare(db, sql, 0, out_result);
    if (!stmt) {
        return false;
    }

    bool value_ok;
    int step_result = sqlite3_step(stmt);
    if (step_result == SQLITE_DONE) {
        value_ok = handle_text(user, default_value);
    }
    else if (step_result == SQLITE_ROW) {
        if (sqlite3_column_count(stmt) >= 1) {
            value_ok = true;
            const char *value = (const char *)sqlite3_column_text(stmt, 0);
            value_ok = handle_text(user, value);
        }
        else {
            value_ok = false;
            DP_error_set("Statement '%s' resulted in no columns", sql);
        }
    }
    else {
        assign_result(step_result, out_result);
        value_ok = false;
        DP_error_set("Error %d executing statement '%s': %s", step_result, sql,
                     db_error(db));
    }

    sqlite3_finalize(stmt);
    return value_ok;
}

static bool check_pragma_result(void *user, const char *text)
{
    const char **params = user;
    const char *expected = params[0];
    if (sqlite3_stricmp(text, expected) == 0) {
        return true;
    }
    else {
        const char *pragma = params[1];
        DP_error_set("Expected %s '%s', but got '%s'", pragma, expected, text);
        return false;
    }
}

static bool is_empty_db(sqlite3 *db, bool *out_empty, int *out_result)
{
    int value;
    if (exec_int_stmt(db, "select 1 from sqlite_master limit 1", 0, &value,
                      out_result)) {
        *out_empty = value == 0;
        return true;
    }
    else {
        return false;
    }
}

static bool init_header(sqlite3 *db, DP_ProjectOpenResult *result)
{
    bool ok = exec_write_stmt(db,
                              "pragma application_id = " DP_PROJECT_XSTR(
                                  DP_PROJECT_APPLICATION_ID),
                              "setting application_id", &result->sql_result)
           && exec_write_stmt(db,
                              "pragma user_version = " DP_PROJECT_XSTR(
                                  DP_PROJECT_USER_VERSION),
                              "setting user_version", &result->sql_result);
    if (!ok) {
        result->error = DP_PROJECT_OPEN_ERROR_HEADER_WRITE;
    }
    return ok;
}

static bool check_header(sqlite3 *db, DP_ProjectOpenResult *result)
{
    int application_id, user_version;
    bool exec_ok = exec_int_stmt(db, "pragma application_id", -1,
                                 &application_id, &result->sql_result)
                && exec_int_stmt(db, "pragma user_version", -1, &user_version,
                                 &result->sql_result);
    if (exec_ok) {
        if (application_id != DP_PROJECT_APPLICATION_ID) {
            DP_error_set("File has incorrect application id %d",
                         application_id);
        }
        else if (user_version != DP_PROJECT_USER_VERSION) {
            DP_error_set("File has unknown user version %d", user_version);
        }
        else {
            return true;
        }
    }
    result->error = DP_PROJECT_OPEN_ERROR_HEADER_READ;
    return false;
}

static void try_rollback(sqlite3 *db)
{
    if (!sqlite3_get_autocommit(db)) {
        char *errmsg;
        int rollback_result = sqlite3_exec(db, "rollback", NULL, NULL, &errmsg);
        if (!is_ok(rollback_result)) {
            DP_warn("Error %d rolling back transaction: %s", rollback_result,
                    fallback_db_error(db, errmsg));
            sqlite3_free(errmsg);
        }
    }
}

static bool apply_migrations(sqlite3 *db, int *out_result)
{
    if (!exec_write_stmt(db,
                         "create table if not exists migrations (\n"
                         "    migration_id integer primary key not null)",
                         "creating migrations table", out_result)) {
        return false;
    }

    sqlite3_stmt *insert_migration_stmt = prepare(
        db, "insert or ignore into migrations (migration_id) values (?)", 0,
        out_result);
    if (!insert_migration_stmt) {
        return false;
    }

    const char *migrations[] = {
        // Migration 1: initial setup.
        "create table sessions (\n"
        "    session_id integer primary key not null,\n"
        "    source_type integer not null,\n"
        "    source_param text,\n"
        "    protocol text not null,\n"
        "    flags integer not null\n,"
        "    opened_at real not null,\n"
        "    closed_at real,\n"
        "    thumbnail blob);\n"
        "create table messages (\n"
        "    session_id integer not null,\n"
        "    sequence_id integer not null,\n"
        "    recorded_at real not null,\n"
        "    flags integer not null,\n"
        "    type integer not null,\n"
        "    context_id integer not null,\n"
        "    body blob,\n"
        "    primary key (session_id, sequence_id)) without rowid;\n"
        "create table snapshots (\n"
        "    snapshot_id integer primary key not null,\n"
        "    session_id integer not null,\n"
        "    flags integer not null,\n"
        "    taken_at real not null);\n"
        "create table snapshot_messages (\n"
        "    snapshot_id integer not null,\n"
        "    sequence_id integer not null,\n"
        "    type integer not null,\n"
        "    context_id integer not null,\n"
        "    body blob,\n"
        "    primary key (snapshot_id, sequence_id)) without rowid",
    };

    bool result = true;
    for (int i = 0; i < (int)DP_ARRAY_LENGTH(migrations); ++i) {
        int bind_result = sqlite3_bind_int(insert_migration_stmt, 1, i + 1);
        if (!is_ok(bind_result)) {
            assign_result(bind_result, out_result);
            DP_error_set("Error %d binding migration insertion: %s",
                         bind_result, db_error(db));
            result = false;
            break;
        }

        int step_result = sqlite3_step(insert_migration_stmt);
        if (step_result != SQLITE_DONE) {
            assign_result(step_result, out_result);
            DP_error_set("Error %d inserting migration: %s", bind_result,
                         db_error(db));
            result = false;
            break;
        }

        int changes = sqlite3_changes(db);
        int reset_result = sqlite3_reset(insert_migration_stmt);
        if (!is_ok(reset_result)) {
            assign_result(reset_result, out_result);
            DP_error_set("Error %d resetting migration insertion: %s",
                         bind_result, db_error(db));
            result = false;
            break;
        }

        if (changes > 0) {
            const char *sql = migrations[i];
            DP_debug("Executing migration %d", i + 1);
            if (!exec_write_stmt(db, sql, "executing migration", out_result)) {
                result = false;
                break;
            }
        }
    }

    sqlite3_finalize(insert_migration_stmt);
    return result;
}

static bool apply_migrations_in_tx(sqlite3 *db, DP_ProjectOpenResult *result)
{
    if (exec_write_stmt(db, "begin exclusive", "opening migration transaction",
                        &result->sql_result)) {
        if (apply_migrations(db, &result->sql_result)
            && exec_write_stmt(db, "commit", "committing migration transaction",
                               &result->sql_result)) {
            return true;
        }
        else {
            try_rollback(db);
        }
    }
    result->error = DP_PROJECT_OPEN_ERROR_MIGRATION;
    return false;
}

static const char *ps_sql(DP_ProjectStatement ps)
{
    switch (ps) {
    case DP_PROJECT_STATEMENT_MESSAGE_RECORD:
        return "insert into messages (session_id, sequence_id, recorded_at, "
               "flags, type, context_id, body) values (?, ?, "
               "unixepoch('subsec'), ?, ?, ?, ?)";
    case DP_PROJECT_STATEMENT_SESSION_OPEN:
        return "insert into sessions (source_type, source_param, protocol, "
               "flags, opened_at) values (?, ?, ?, ?, unixepoch('subsec'))";
    case DP_PROJECT_STATEMENT_SESSION_CLOSE:
        return "update sessions set flags = flags | ?, "
               "closed_at = unixepoch('subsec') where session_id = ?";
    case DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGE_RECORD:
        return "insert into snapshot_messages (snapshot_id, sequence_id, type, "
               "context_id, body) values (?, ?, ?, ?, ?)";
    case DP_PROJECT_STATEMENT_SNAPSHOT_OPEN:
        return "insert into snapshots (session_id, flags, taken_at) values "
               "(?, ?, unixepoch('subsec'))";
    case DP_PROJECT_STATEMENT_SNAPSHOT_FINISH:
        return "update snapshots set flags = flags | ? where snapshot_id = ?";
    case DP_PROJECT_STATEMENT_SNAPSHOT_DISCARD:
        return "delete from snapshots where snapshot_id = ?";
    case DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGES_DISCARD:
        return "delete from snapshot_messages where snapshot_id = ?";
    case DP_PROJECT_STATEMENT_SNAPSHOT_SELECT_EXCEPT:
        return "select snapshot_id from snapshots "
               "where snapshot_id <> ? and (flags & ?) = 0";
    case DP_PROJECT_STATEMENT_COUNT:
        break;
    }
    return NULL;
}

DP_ProjectOpenResult DP_project_open(const char *path, unsigned int flags)
{
    DP_ProjectOpenResult result = {NULL, DP_PROJECT_OPEN_ERROR_UNKNOWN,
                                   SQLITE_OK};
    result.sql_result = DP_sql_init();
    if (!open_sql_ok(&result, DP_PROJECT_OPEN_ERROR_INIT)) {
        return result;
    }

    int db_open_flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX
                      | SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_EXRESCODE;
    bool only_existing = flags & DP_PROJECT_OPEN_EXISTING;
    if (!only_existing) {
        db_open_flags |= SQLITE_OPEN_CREATE;
    }

    sqlite3 *db;
    result.sql_result = sqlite3_open_v2(path, &db, db_open_flags, NULL);
    if (!open_sql_ok(&result, DP_PROJECT_OPEN_ERROR_OPEN)) {
        DP_error_set("Error %d opening '%s': %s", result.sql_result, path,
                     db_error(db));
        try_close_db(db);
        return result;
    }

    const char *main_schema = sqlite3_db_name(db, 0);
    if (sqlite3_db_readonly(db, main_schema) > 0) {
        result.error = DP_PROJECT_OPEN_ERROR_READ_ONLY;
        DP_error_set("Error opening '%s': database is read-only", path);
        try_close_db(db);
        return result;
    }

    bool setup_ok =
        exec_write_stmt(db, "pragma locking_mode = exclusive",
                        "setting exclusive locking mode", &result.sql_result)
        && exec_write_stmt(db, "pragma journal_mode = memory",
                           "setting journal mode to memory", &result.sql_result)
        && exec_write_stmt(db, "pragma synchronous = off",
                           "setting synchronous off", &result.sql_result)
        && exec_text_stmt(db, "pragma locking_mode", NULL, check_pragma_result,
                          (const char *[]){"exclusive", "locking_mode"},
                          &result.sql_result);
    if (!setup_ok) {
        result.error = DP_PROJECT_OPEN_ERROR_SETUP;
        try_close_db(db);
        return result;
    }

    bool empty;
    if (flags & DP_PROJECT_OPEN_TRUNCATE) {
        // This is how you clear out the database according to the sqlite
        // manual. Lack of error checking is intentional, we'll notice later.
        sqlite3_table_column_metadata(db, NULL, "dummy_table", "dummy_column",
                                      NULL, NULL, NULL, NULL, NULL);
        sqlite3_db_config(db, SQLITE_DBCONFIG_RESET_DATABASE, 1, 0);
        sqlite3_exec(db, "vacuum", NULL, NULL, NULL);
        sqlite3_db_config(db, SQLITE_DBCONFIG_RESET_DATABASE, 0, 0);
        empty = true;
    }
    else if (!is_empty_db(db, &empty, &result.sql_result)) {
        result.error = DP_PROJECT_OPEN_ERROR_READ_EMPTY;
        try_close_db(db);
        return result;
    }

    bool init_ok = (!empty || init_header(db, &result))
                && check_header(db, &result)
                && apply_migrations_in_tx(db, &result);
    if (!init_ok) {
        try_close_db(db);
        return result;
    }

    if (!exec_write_stmt(db, "pragma journal_mode = off",
                         "setting journal mode to off", &result.sql_result)) {
        result.error = DP_PROJECT_OPEN_ERROR_SETUP;
        try_close_db(db);
        return result;
    }

    sqlite3_stmt *stmts[DP_PROJECT_STATEMENT_COUNT];
    for (int i = 0; i < DP_PROJECT_STATEMENT_COUNT; ++i) {
        sqlite3_stmt *stmt =
            prepare(db, ps_sql((DP_ProjectStatement)i),
                    SQLITE_PREPARE_PERSISTENT, &result.sql_result);
        if (stmt) {
            stmts[i] = stmt;
        }
        else {
            result.error = DP_PROJECT_OPEN_ERROR_PREPARE;
            for (int j = 0; j < i; ++j) {
                sqlite3_finalize(stmts[j]);
            }
            try_close_db(db);
            return result;
        }
    }

    result.project = DP_malloc(sizeof(*result.project));
    result.project->db = db;
    result.project->session_id = 0LL;
    result.project->sequence_id = 0LL;
    result.project->snapshot_id = 0LL;
    result.project->snapshot_sequence_id = 0LL;
    memcpy(result.project->stmts, stmts, sizeof(stmts));
    result.error = 0;
    return result;
}

bool DP_project_open_was_busy(const DP_ProjectOpenResult *open)
{
    DP_ASSERT(open);
    return open->error == DP_PROJECT_OPEN_ERROR_SETUP
        && (open->sql_result & 0xff) == SQLITE_BUSY;
}

bool DP_project_close(DP_Project *prj)
{
    if (!prj) {
        DP_error_set("Project is null");
        return false;
    }

    if (DP_project_session_close(prj, DP_PROJECT_SESSION_FLAG_PROJECT_CLOSED)
        < 0) {
        DP_warn("Close project: %s", DP_error());
    }

    for (int i = 0; i < DP_PROJECT_STATEMENT_COUNT; ++i) {
        sqlite3_finalize(prj->stmts[i]);
    }
    int close_result = sqlite3_close(prj->db);
    DP_free(prj);

    if (is_ok(close_result)) {
        return true;
    }
    else {
        DP_error_set("Error %d closing project", close_result);
        return false;
    }
}


static bool handle_verify(void *user, const char *text)
{
    DP_ProjectVerifyStatus *out_status = user;
    if (text) {
        if (sqlite3_stricmp(text, "ok") == 0) {
            *out_status = DP_PROJECT_VERIFY_OK;
            return true;
        }
        else {
            DP_error_set("Project database corrupted: %s", text);
            *out_status = DP_PROJECT_VERIFY_CORRUPTED;
            return false;
        }
    }
    else {
        DP_error_set("Integrity check returnd no result");
        *out_status = DP_PROJECT_VERIFY_ERROR;
        return false;
    }
}

DP_ProjectVerifyStatus DP_project_verify(DP_Project *prj, unsigned int flags)
{
    DP_ASSERT(prj);
    const char *sql = (flags & DP_PROJECT_VERIFY_FULL)
                        ? "pragma integrity_check(1)"
                        : "pragma quick_check(1)";
    DP_ProjectVerifyStatus status;
    exec_text_stmt(prj->db, sql, NULL, handle_verify, &status, NULL);
    return status;
}


static const char *ps_title(DP_ProjectStatement ps)
{
    switch (ps) {
    case DP_PROJECT_STATEMENT_MESSAGE_RECORD:
        return "message record";
    case DP_PROJECT_STATEMENT_SESSION_OPEN:
        return "session open";
    case DP_PROJECT_STATEMENT_SESSION_CLOSE:
        return "session close";
    case DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGE_RECORD:
        return "snapshot message record";
    case DP_PROJECT_STATEMENT_SNAPSHOT_OPEN:
        return "snapshot insert";
    case DP_PROJECT_STATEMENT_SNAPSHOT_FINISH:
        return "snapshot finish";
    case DP_PROJECT_STATEMENT_SNAPSHOT_DISCARD:
        return "snapshot discard";
    case DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGES_DISCARD:
        return "snapshot messages discard";
    case DP_PROJECT_STATEMENT_SNAPSHOT_SELECT_EXCEPT:
        return "snapshot select except";
    case DP_PROJECT_STATEMENT_COUNT:
        break;
    }
    return "unknown";
}

static bool ps_bind_int(DP_Project *prj, DP_ProjectStatement ps, int param,
                        int value)
{
    int bind_result = sqlite3_bind_int(prj->stmts[ps], param, value);
    if (is_ok(bind_result)) {
        return true;
    }
    else {
        DP_error_set("Error %d binding int parameter %d to %s: %s", bind_result,
                     param, ps_title(ps), prj_db_error(prj));
        return false;
    }
}

static bool ps_bind_int64(DP_Project *prj, DP_ProjectStatement ps, int param,
                          long long value)
{
    int bind_result = sqlite3_bind_int64(prj->stmts[ps], param, value);
    if (is_ok(bind_result)) {
        return true;
    }
    else {
        DP_error_set("Error %d binding int64 parameter %d to %s: %s",
                     bind_result, param, ps_title(ps), prj_db_error(prj));
        return false;
    }
}

static bool ps_bind_blob(DP_Project *prj, DP_ProjectStatement ps, int param,
                         const void *value, size_t length)
{
    int bind_result = sqlite3_bind_blob64(prj->stmts[ps], param, value, length,
                                          SQLITE_STATIC);
    if (is_ok(bind_result)) {
        return true;
    }
    else {
        DP_error_set("Error %d binding blob parameter %d to %s: %s",
                     bind_result, param, ps_title(ps), prj_db_error(prj));
        return false;
    }
}

static bool ps_bind_text(DP_Project *prj, DP_ProjectStatement ps, int param,
                         const char *value)
{
    int bind_result =
        sqlite3_bind_text(prj->stmts[ps], param, value, -1, SQLITE_STATIC);
    if (is_ok(bind_result)) {
        return true;
    }
    else {
        DP_error_set("Error %d binding blob parameter %d to %s: %s",
                     bind_result, param, ps_title(ps), prj_db_error(prj));
        return false;
    }
}

bool ps_exec_write(DP_Project *prj, DP_ProjectStatement ps,
                   long long *out_insert_rowid)
{
    sqlite3_stmt *stmt = prj->stmts[ps];
    int step_result = sqlite3_step(stmt);
    if (step_result == SQLITE_DONE) {
        int reset_result = sqlite3_reset(stmt);
        if (is_ok(reset_result)) {
            if (out_insert_rowid) {
                *out_insert_rowid = sqlite3_last_insert_rowid(prj->db);
            }
            return true;
        }
        else {
            DP_error_set("Error %d resetting %s: %s", reset_result,
                         ps_title(ps), prj_db_error(prj));
            return false;
        }
    }
    else {
        const char *title = ps_title(ps);
        DP_error_set("Error %d executing %s: %s", step_result, title,
                     prj_db_error(prj));
        int reset_result = sqlite3_reset(stmt);
        if (!is_ok(reset_result)) {
            DP_warn("Error %d resetting failed %s: %s", reset_result, title,
                    prj_db_error(prj));
        }
        return false;
    }
}

bool ps_exec_step(DP_Project *prj, DP_ProjectStatement ps, bool *out_error)
{
    sqlite3_stmt *stmt = prj->stmts[ps];
    int step_result = sqlite3_step(stmt);
    if (step_result == SQLITE_ROW) {
        return true;
    }
    else if (step_result == SQLITE_DONE) {
        int reset_result = sqlite3_reset(stmt);
        if (is_ok(reset_result)) {
            *out_error = false;
        }
        else {
            DP_error_set("Error %d resetting %s: %s", reset_result,
                         ps_title(ps), prj_db_error(prj));
            *out_error = true;
        }
        return false;
    }
    else {
        *out_error = true;
        const char *title = ps_title(ps);
        DP_error_set("Error %d executing %s: %s", step_result, title,
                     prj_db_error(prj));
        int reset_result = sqlite3_reset(stmt);
        if (!is_ok(reset_result)) {
            DP_warn("Error %d resetting failed %s: %s", reset_result, title,
                    prj_db_error(prj));
        }
        return false;
    }
}

long long ps_column_int64(DP_Project *prj, DP_ProjectStatement ps, int column)
{
    sqlite3_stmt *stmt = prj->stmts[ps];
    return sqlite3_column_int64(stmt, column);
}

void ps_clear_bindings(DP_Project *prj, DP_ProjectStatement ps)
{
    int clear_result = sqlite3_clear_bindings(prj->stmts[ps]);
    if (!is_ok(clear_result)) {
        DP_warn("Error clearing bindings on %s: %s", ps_title(ps),
                prj_db_error(prj));
    }
}


long long DP_project_session_id(DP_Project *prj)
{
    DP_ASSERT(prj);
    return prj->session_id;
}

int DP_project_session_open(DP_Project *prj, DP_ProjectSourceType source_type,
                            const char *source_param, const char *protocol,
                            unsigned int flags)
{
    DP_ASSERT(prj);
    DP_ASSERT(protocol);
    if (prj->session_id != 0LL) {
        DP_error_set("Session %lld already open", prj->session_id);
        return DP_PROJECT_SESSION_OPEN_ERROR_ALREADY_OPEN;
    }

    DP_debug("Opening session source %d %s, protocol %s", (int)source_type,
             source_param ? source_param : "<NULL>", protocol);
    prj->sequence_id = 0LL;
    bool write_ok =
        ps_bind_int(prj, DP_PROJECT_STATEMENT_SESSION_OPEN, 1, (int)source_type)
        && ps_bind_text(prj, DP_PROJECT_STATEMENT_SESSION_OPEN, 2, source_param)
        && ps_bind_text(prj, DP_PROJECT_STATEMENT_SESSION_OPEN, 3, protocol)
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_SESSION_OPEN, 4,
                         DP_uint_to_llong(flags))
        && ps_exec_write(prj, DP_PROJECT_STATEMENT_SESSION_OPEN,
                         &prj->session_id);
    ps_clear_bindings(prj, DP_PROJECT_STATEMENT_SESSION_OPEN);
    if (!write_ok) {
        return DP_PROJECT_SESSION_OPEN_ERROR_ALREADY_OPEN;
    }

    return 0;
}

int DP_project_session_close(DP_Project *prj, unsigned int flags_to_set)
{
    DP_ASSERT(prj);
    long long session_id = prj->session_id;
    if (session_id == 0LL) {
        return DP_PROJECT_SESSION_CLOSE_NOT_OPEN;
    }

    prj->session_id = 0LL;
    bool update_ok =
        ps_bind_int64(prj, DP_PROJECT_STATEMENT_SESSION_CLOSE, 1,
                      DP_uint_to_llong(flags_to_set))
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_SESSION_CLOSE, 2, session_id)
        && ps_exec_write(prj, DP_PROJECT_STATEMENT_SESSION_CLOSE, NULL);
    if (!update_ok) {
        return DP_PROJECT_SESSION_CLOSE_ERROR_WRITE;
    }

    if (sqlite3_changes64(prj->db) == 0LL) {
        DP_error_set("Closing session %lld resulted in no changes", session_id);
        return DP_PROJECT_SESSION_CLOSE_ERROR_NO_CHANGE;
    }

    return 0;
}


unsigned char *get_serialize_buffer(void *user, DP_UNUSED size_t length)
{
    DP_ASSERT(length <= DP_MESSAGE_MAX_PAYLOAD_LENGTH);
    DP_Project *prj = user;
    return prj->serialize_buffer;
}

int DP_project_message_record(DP_Project *prj, DP_Message *msg,
                              unsigned int flags)
{
    DP_ASSERT(prj);
    DP_ASSERT(msg);

    long long session_id = prj->session_id;
    if (session_id == 0LL) {
        DP_error_set("No open session");
        return DP_PROJECT_MESSAGE_RECORD_ERROR_NO_SESSION;
    }

    size_t length = DP_message_serialize_body(msg, get_serialize_buffer, prj);
    if (length == 0) {
        return DP_PROJECT_MESSAGE_RECORD_ERROR_SERIALIZE;
    }

    bool write_ok =
        ps_bind_int64(prj, DP_PROJECT_STATEMENT_MESSAGE_RECORD, 1, session_id)
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_MESSAGE_RECORD, 2,
                         ++prj->sequence_id)
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_MESSAGE_RECORD, 3,
                         DP_uint_to_llong(flags))
        && ps_bind_int(prj, DP_PROJECT_STATEMENT_MESSAGE_RECORD, 4,
                       (int)DP_message_type(msg))
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_MESSAGE_RECORD, 5,
                         DP_message_context_id(msg))
        && ps_bind_blob(prj, DP_PROJECT_STATEMENT_MESSAGE_RECORD, 6,
                        prj->serialize_buffer, length)
        && ps_exec_write(prj, DP_PROJECT_STATEMENT_MESSAGE_RECORD, NULL);
    ps_clear_bindings(prj, DP_PROJECT_STATEMENT_MESSAGE_RECORD);
    if (!write_ok) {
        return DP_PROJECT_MESSAGE_RECORD_ERROR_WRITE;
    }

    return 0;
}


long long DP_project_snapshot_open(DP_Project *prj, unsigned int flags)
{
    DP_ASSERT(prj);
    DP_ASSERT(!(flags & DP_PROJECT_SNAPSHOT_FLAG_COMPLETE));

    if (prj->session_id == 0LL) {
        DP_error_set("No session open");
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_NO_SESSION;
    }

    if (prj->snapshot_id != 0LL) {
        DP_error_set("Snapshot %lld already open", prj->snapshot_id);
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_ALREADY_OPEN;
    }

    long long snapshot_id;
    bool write_ok =
        ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_OPEN, 1,
                      prj->session_id)
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_OPEN, 2, flags)
        && ps_exec_write(prj, DP_PROJECT_STATEMENT_SNAPSHOT_OPEN, &snapshot_id);
    if (write_ok) {
        DP_ASSERT(snapshot_id > 0);
        prj->snapshot_id = snapshot_id;
        prj->snapshot_sequence_id = 0LL;
        return snapshot_id;
    }
    else {
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_WRITE;
    }
}

int DP_project_snapshot_finish(DP_Project *prj, long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0LL);

    if (prj->snapshot_id == 0LL) {
        DP_error_set("Snapshot %lld is not open (none is)", snapshot_id);
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_NOT_OPEN;
    }

    if (prj->snapshot_id != snapshot_id) {
        DP_error_set("Snapshot %lld is not open, (%lld is)", snapshot_id,
                     prj->snapshot_id);
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_NOT_OPEN;
    }

    prj->snapshot_id = 0LL;

    bool write_ok =
        ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_FINISH, 1,
                      DP_PROJECT_SNAPSHOT_FLAG_COMPLETE)
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_OPEN, 2,
                         snapshot_id)
        && ps_exec_write(prj, DP_PROJECT_STATEMENT_SNAPSHOT_OPEN, NULL);
    if (!write_ok) {
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_WRITE;
    }

    if (sqlite3_changes64(prj->db) == 0LL) {
        DP_error_set("Closing snapshot %lld resulted in no changes",
                     snapshot_id);
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_NO_CHANGE;
    }

    return 0;
}

int DP_project_snapshot_discard(DP_Project *prj, long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0LL);

    if (prj->snapshot_id == snapshot_id) {
        prj->snapshot_id = 0LL;
    }

    bool write_ok =
        ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGES_DISCARD, 1,
                      snapshot_id)
        && ps_exec_write(prj, DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGES_DISCARD,
                         NULL)
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_DISCARD, 1,
                         snapshot_id)
        && ps_exec_write(prj, DP_PROJECT_STATEMENT_SNAPSHOT_DISCARD, NULL);
    if (!write_ok) {
        return DP_PROJECT_SNAPSHOT_DISCARD_ERROR_WRITE;
    }

    if (sqlite3_changes64(prj->db) == 0LL) {
        return DP_PROJECT_SNAPSHOT_DISCARD_NOT_FOUND;
    }

    return 0;
}

int DP_project_snapshot_discard_all_except(DP_Project *prj,
                                           long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0LL);

    bool bind_ok =
        ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_SELECT_EXCEPT, 1,
                      snapshot_id)
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_SELECT_EXCEPT, 2,
                         DP_PROJECT_SNAPSHOT_FLAG_PERSISTENT);
    if (!bind_ok) {
        return DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_READ;
    }

    bool read_error = false;
    int write_errors = 0;
    int discard_count = 0;
    while (ps_exec_step(prj, DP_PROJECT_STATEMENT_SNAPSHOT_SELECT_EXCEPT,
                        &read_error)) {
        long long snapshot_id_to_discard = ps_column_int64(
            prj, DP_PROJECT_STATEMENT_SNAPSHOT_SELECT_EXCEPT, 0);
        int discard_result =
            DP_project_snapshot_discard(prj, snapshot_id_to_discard);
        if (discard_result < 0) {
            DP_warn("Error discarding snapshot %lld: %s",
                    snapshot_id_to_discard, DP_error());
            ++write_errors;
        }
        else if (discard_result == 0) {
            ++discard_count;
        }
    }

    if (read_error) {
        if (write_errors > 0) {
            DP_warn("Discard snapshots read error: %s", DP_error());
            DP_error_set("Error reading and discarding %d of %d snapshot(s)",
                         write_errors, write_errors + discard_count);
            return DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_READ_WRITE;
        }
        else {
            return DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_READ;
        }
    }
    else if (write_errors > 0) {
        DP_error_set("Error discarding %d of %d snapshot(s)", write_errors,
                     write_errors + discard_count);
        return DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_WRITE;
    }
    else {
        return discard_count;
    }
}

int DP_project_snapshot_message_record(DP_Project *prj, long long snapshot_id,
                                       DP_Message *msg)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0);
    DP_ASSERT(msg);

    if (prj->snapshot_id == 0LL) {
        DP_error_set("Snapshot %lld is not open (none is)", snapshot_id);
        return DP_PROJECT_SNAPSHOT_MESSAGE_RECORD_ERROR_NOT_OPEN;
    }

    if (prj->snapshot_id != snapshot_id) {
        DP_error_set("Snapshot %lld is not open, (%lld is)", snapshot_id,
                     prj->snapshot_id);
        return DP_PROJECT_SNAPSHOT_MESSAGE_RECORD_ERROR_NOT_OPEN;
    }

    size_t length = DP_message_serialize_body(msg, get_serialize_buffer, prj);
    if (length == 0) {
        return DP_PROJECT_SNAPSHOT_MESSAGE_RECORD_ERROR_SERIALIZE;
    }

    bool write_ok =
        ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGE_RECORD, 1,
                      snapshot_id)
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGE_RECORD, 2,
                         ++prj->snapshot_sequence_id)
        && ps_bind_int(prj, DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGE_RECORD, 3,
                       (int)DP_message_type(msg))
        && ps_bind_int64(prj, DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGE_RECORD, 4,
                         DP_message_context_id(msg))
        && ps_bind_blob(prj, DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGE_RECORD, 5,
                        prj->serialize_buffer, length)
        && ps_exec_write(prj, DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGE_RECORD,
                         NULL);
    ps_clear_bindings(prj, DP_PROJECT_STATEMENT_SNAPSHOT_MESSAGE_RECORD);
    if (!write_ok) {
        return DP_PROJECT_SNAPSHOT_MESSAGE_RECORD_ERROR_WRITE;
    }

    return 0;
}


static int dump_rows(void *user, int ncols, char **values, char **names)
{
    DP_Output *output = ((void **)user)[0];
    int *row_ptr = ((void **)user)[1];
    int row = *row_ptr;

    if (row < 0) {
        return SQLITE_ERROR;
    }

    if (row == 0) {
        for (int i = 0; i < ncols; ++i) {
            const char *name = names[i];
            const char *suffix = i < ncols - 1 ? "," : "\n";
            bool output_ok = name && name[0] != '\0'
                               ? DP_output_format(output, "%s%s", name, suffix)
                               : DP_output_format(output, "?%d%s", i, suffix);
            if (!output_ok) {
                *row_ptr = -1;
                return SQLITE_ERROR;
            }
        }
    }

    for (int i = 0; i < ncols; ++i) {
        const char *value = values[i];
        const char *suffix = i < ncols - 1 ? "," : "\n";
        bool output_ok = value
                           ? DP_output_format(output, "'%s'%s", value, suffix)
                           : DP_output_format(output, "NULL%s", suffix);
        if (!output_ok) {
            *row_ptr = -1;
            return SQLITE_ERROR;
        }
    }

    *row_ptr = row + 1;
    return SQLITE_OK;
}

static bool dump_query(DP_Project *prj, DP_Output *output, const char *sql)
{
    if (DP_output_format(output, "\n--- %s\n", sql)) {
        char *errmsg;
        int row = 0;
        int exec_result = sqlite3_exec(prj->db, sql, dump_rows,
                                       (void *[]){output, &row}, &errmsg);
        if (row < 0) {
            sqlite3_free(errmsg);
            return false;
        }
        else if (!is_ok(exec_result)) {
            bool output_ok =
                DP_output_format(output, "\n*** error: %s\n",
                                 fallback_db_error(prj->db, errmsg));
            sqlite3_free(errmsg);
            return output_ok;
        }
        else {
            return true;
        }
    }
    else {
        return false;
    }
}

bool DP_project_dump(DP_Project *prj, DP_Output *output)
{
    DP_ASSERT(prj);
    DP_ASSERT(output);
    return DP_OUTPUT_PRINT_LITERAL(output, "begin project dump\n")
        && dump_query(prj, output, "pragma application_id")
        && dump_query(prj, output, "pragma user_version")
        && dump_query(
               prj, output,
               "select migration_id from migrations order by migration_id")
        && dump_query(
               prj, output,
               "select session_id, source_type, source_param, protocol, "
               "printf('0x%x', flags) as flags, "
               "case when closed_at is null then 'open' else 'closed' end "
               "as status from sessions order by session_id")
        && DP_OUTPUT_PRINT_LITERAL(output, "\nend project dump\n");
}
