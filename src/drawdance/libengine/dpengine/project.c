// SPDX-License-Identifier: GPL-3.0-or-later
#include "project.h"
#include "annotation.h"
#include "canvas_state.h"
#include "compress.h"
#include "document_metadata.h"
#include "key_frame.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "snapshots.h"
#include "tile.h"
#include "track.h"
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpcommon/threading.h>
#include <dpdb/sql.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/message.h>


#define DP_PROJECT_STR(X)  #X
#define DP_PROJECT_XSTR(X) DP_PROJECT_STR(X)

#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_HIDDEN         (1u << 0u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED       (1u << 1u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_PASS_THROUGH   (1u << 2u)
#define DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_PROTECTED (1u << 0u)
#define DP_PROJECT_SNAPSHOT_TRACK_FLAG_HIDDEN         (1u << 0u)
#define DP_PROJECT_SNAPSHOT_TRACK_FLAG_ONION_SKIN     (1u << 1u)


typedef enum DP_ProjectPersistentStatement {
    DP_PROJECT_STATEMENT_MESSAGE_RECORD,
    DP_PROJECT_STATEMENT_COUNT,
} DP_ProjectPersistentStatement;

typedef enum DP_ProjectSnapshotState {
    DP_PROJECT_SNAPSHOT_STATE_CLOSED,
    DP_PROJECT_SNAPSHOT_STATE_READY,
    DP_PROJECT_SNAPSHOT_STATE_ERROR,
    DP_PROJECT_SNAPSHOT_STATE_OK,
} DP_ProjectSnapshotState;

typedef enum DP_ProjectSnapshotPersistentStatement {
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_METADATA,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_LAYER,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TILE,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_ANNOTATION,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TRACK,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME_LAYER,
    DP_PROJECT_SNAPSHOT_STATEMENT_COUNT,
} DP_ProjectSnapshotPersistentStatement;

typedef enum DP_ProjectSnapshotMetadata {
    DP_PROJECT_SNAPSHOT_METADATA_WIDTH = 1,
    DP_PROJECT_SNAPSHOT_METADATA_HEIGHT = 2,
    DP_PROJECT_SNAPSHOT_METADATA_BACKGROUND_TILE = 3,
    DP_PROJECT_SNAPSHOT_METADATA_DPIX = 4,
    DP_PROJECT_SNAPSHOT_METADATA_DPIY = 5,
    DP_PROJECT_SNAPSHOT_METADATA_FRAMERATE = 6,
    DP_PROJECT_SNAPSHOT_METADATA_FRAME_COUNT = 7,
} DP_ProjectSnapshotMetadata;

struct DP_Project {
    sqlite3 *db;
    long long session_id;
    long long sequence_id;
    struct {
        long long id;
        DP_ProjectSnapshotState state;
        sqlite3_stmt *stmts[DP_PROJECT_SNAPSHOT_STATEMENT_COUNT];
    } snapshot;
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


int DP_project_check(const unsigned char *buf, size_t size)
{
    if (size < 72) {
        return DP_PROJECT_CHECK_ERROR_HEADER;
    }

    // "SQLite format 3\0" in ASCII
    unsigned char magic[] = {0x53, 0x51, 0x4c, 0x69, 0x74, 0x65, 0x20, 0x66,
                             0x6f, 0x72, 0x6d, 0x61, 0x74, 0x20, 0x33, 0x00};
    if (memcmp(buf, magic, sizeof(magic)) != 0) {
        return DP_PROJECT_CHECK_ERROR_MAGIC;
    }

    int application_id = DP_read_bigendian_int32(buf + 68);
    if (application_id != DP_PROJECT_APPLICATION_ID) {
        return DP_PROJECT_CHECK_ERROR_APPLICATION_ID;
    }

    int user_version = DP_read_bigendian_int32(buf + 60);
    if (user_version < 1) {
        return DP_PROJECT_CHECK_ERROR_USER_VERSION;
    }

    return user_version;
}

int DP_project_check_path(const char *path)
{
    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        return DP_PROJECT_CHECK_ERROR_OPEN;
    }

    unsigned char buf[72];
    bool error;
    size_t read = DP_input_read(input, buf, sizeof(buf), &error);
    DP_input_free(input);
    if (error) {
        return DP_PROJECT_CHECK_ERROR_READ;
    }

    return DP_project_check(buf, read);
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
                         "    migration_id integer primary key not null)\n"
                         "strict",
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
        "    thumbnail blob)\n"
        "strict;\n"
        "create table messages (\n"
        "    session_id integer not null,\n"
        "    sequence_id integer not null,\n"
        "    recorded_at real not null,\n"
        "    flags integer not null,\n"
        "    type integer not null,\n"
        "    context_id integer not null,\n"
        "    body blob,\n"
        "    primary key (session_id, sequence_id))\n"
        "strict, without rowid;\n"
        "create table snapshots (\n"
        "    snapshot_id integer primary key not null,\n"
        "    session_id integer not null,\n"
        "    flags integer not null,\n"
        "    taken_at real not null)\n"
        "strict;\n"
        "create table snapshot_metadata (\n"
        "    snapshot_id integer not null,\n"
        "    metadata_id integer not null,\n"
        "    value any,\n"
        "    primary key (snapshot_id, metadata_id))\n"
        "strict;\n"
        "create table snapshot_layers (\n"
        "    snapshot_id integer not null,\n"
        "    layer_index integer not null,\n"
        "    parent_index integer not null,\n"
        "    layer_id integer not null,\n"
        "    children integer not null,\n"
        "    title text not null,\n"
        "    blend_mode integer not null,\n"
        "    opacity integer not null,\n"
        "    sketch_opacity integer not null,\n"
        "    sketch_tint integer not null,\n"
        "    flags integer not null,\n"
        "    fill integer not null,\n"
        "    primary key (snapshot_id, layer_index))\n"
        "strict, without rowid;\n"
        "create table snapshot_tiles (\n"
        "    snapshot_id integer not null,\n"
        "    layer_index integer not null,\n"
        "    tile_index integer not null,\n"
        "    repeat integer not null,\n"
        "    pixels blob not null,\n"
        "    primary key (snapshot_id, layer_index, tile_index))\n"
        "strict;\n"
        "create table snapshot_annotations (\n"
        "    snapshot_id integer not null,\n"
        "    annotation_index integer not null,\n"
        "    annotation_id integer not null,\n"
        "    content text not null,\n"
        "    x integer not null,\n"
        "    y integer not null,\n"
        "    width integer not null,\n"
        "    height integer not null,\n"
        "    background_color integer not null,\n"
        "    valign integer not null,\n"
        "    flags integer not null,\n"
        "    primary key (snapshot_id, annotation_index))\n"
        "strict;\n"
        "create table snapshot_tracks (\n"
        "    snapshot_id integer not null,\n"
        "    track_index integer not null,\n"
        "    track_id integer not null,\n"
        "    title text not null,\n"
        "    flags integer not null,\n"
        "    primary key (snapshot_id, track_index))\n"
        "strict, without rowid;\n"
        "create table snapshot_key_frames (\n"
        "    snapshot_id integer not null,\n"
        "    track_index integer not null,\n"
        "    frame_index integer not null,\n"
        "    title text not null,\n"
        "    layer_id integer not null,\n"
        "    primary key (snapshot_id, track_index, frame_index))\n"
        "strict, without rowid;\n"
        "create table snapshot_key_frame_layers (\n"
        "    snapshot_id integer not null,\n"
        "    track_index integer not null,\n"
        "    frame_index integer not null,\n"
        "    layer_id integer not null,\n"
        "    flags integer not null,\n"
        "    primary key (snapshot_id, track_index, frame_index, layer_id))\n"
        "strict, without rowid;\n",
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

static const char *pps_sql(DP_ProjectPersistentStatement pps)
{
    switch (pps) {
    case DP_PROJECT_STATEMENT_MESSAGE_RECORD:
        return "insert into messages (session_id, sequence_id, recorded_at, "
               "flags, type, context_id, body) values (?, ?, "
               "unixepoch('subsec'), ?, ?, ?, ?)";
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
            prepare(db, pps_sql((DP_ProjectPersistentStatement)i),
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
    result.project->snapshot.id = 0LL;
    result.project->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_CLOSED;
    for (int i = 0; i < DP_PROJECT_SNAPSHOT_STATEMENT_COUNT; ++i) {
        result.project->snapshot.stmts[i] = NULL;
    }
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


static sqlite3_stmt *ps_prepare_ephemeral(DP_Project *prj, const char *sql)
{
    return prepare(prj->db, sql, 0, NULL);
}

static sqlite3_stmt *ps_prepare_persistent(DP_Project *prj, const char *sql)
{
    return prepare(prj->db, sql, SQLITE_PREPARE_PERSISTENT, NULL);
}

static bool ps_bind_int(DP_Project *prj, sqlite3_stmt *stmt, int param,
                        int value)
{
    int bind_result = sqlite3_bind_int(stmt, param, value);
    if (is_ok(bind_result)) {
        return true;
    }
    else {
        DP_error_set("Error %d binding int parameter %d to %s: %s", bind_result,
                     param, sqlite3_sql(stmt), prj_db_error(prj));
        return false;
    }
}

static bool ps_bind_int64(DP_Project *prj, sqlite3_stmt *stmt, int param,
                          long long value)
{
    int bind_result = sqlite3_bind_int64(stmt, param, value);
    if (is_ok(bind_result)) {
        return true;
    }
    else {
        DP_error_set("Error %d binding int64 parameter %d to %s: %s",
                     bind_result, param, sqlite3_sql(stmt), prj_db_error(prj));
        return false;
    }
}

static bool ps_bind_blob(DP_Project *prj, sqlite3_stmt *stmt, int param,
                         const void *value, size_t length)
{
    int bind_result =
        sqlite3_bind_blob64(stmt, param, value, length, SQLITE_STATIC);
    if (is_ok(bind_result)) {
        return true;
    }
    else {
        DP_error_set("Error %d binding blob parameter %d to %s: %s",
                     bind_result, param, sqlite3_sql(stmt), prj_db_error(prj));
        return false;
    }
}

static bool ps_bind_text_with(DP_Project *prj, sqlite3_stmt *stmt, int param,
                              const char *value, int length)
{
    int bind_result =
        sqlite3_bind_text(stmt, param, value, length, SQLITE_STATIC);
    if (is_ok(bind_result)) {
        return true;
    }
    else {
        DP_error_set("Error %d binding text parameter %d to %s: %s",
                     bind_result, param, sqlite3_sql(stmt), prj_db_error(prj));
        return false;
    }
}

static bool ps_bind_text(DP_Project *prj, sqlite3_stmt *stmt, int param,
                         const char *value)
{
    return ps_bind_text_with(prj, stmt, param, value, -1);
}

static bool ps_bind_text_length(DP_Project *prj, sqlite3_stmt *stmt, int param,
                                const char *value, size_t length)
{
    return ps_bind_text_with(prj, stmt, param, value, DP_size_to_int(length));
}

static bool ps_exec_write(DP_Project *prj, sqlite3_stmt *stmt,
                          long long *out_insert_rowid)
{
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
                         sqlite3_sql(stmt), prj_db_error(prj));
            return false;
        }
    }
    else {
        const char *sql = sqlite3_sql(stmt);
        DP_error_set("Error %d executing %s: %s", step_result, sql,
                     prj_db_error(prj));
        int reset_result = sqlite3_reset(stmt);
        if (!is_ok(reset_result)) {
            DP_warn("Error %d resetting failed %s: %s", reset_result, sql,
                    prj_db_error(prj));
        }
        return false;
    }
}

static bool ps_exec_step(DP_Project *prj, sqlite3_stmt *stmt, bool *out_error)
{
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
                         sqlite3_sql(stmt), prj_db_error(prj));
            *out_error = true;
        }
        return false;
    }
    else {
        *out_error = true;
        const char *sql = sqlite3_sql(stmt);
        DP_error_set("Error %d executing %s: %s", step_result, sql,
                     prj_db_error(prj));
        int reset_result = sqlite3_reset(stmt);
        if (!is_ok(reset_result)) {
            DP_warn("Error %d resetting failed %s: %s", reset_result, sql,
                    prj_db_error(prj));
        }
        return false;
    }
}

static void ps_clear_bindings(DP_Project *prj, sqlite3_stmt *stmt)
{
    int clear_result = sqlite3_clear_bindings(stmt);
    if (!is_ok(clear_result)) {
        DP_warn("Error clearing bindings on %s: %s", sqlite3_sql(stmt),
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
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "insert into sessions (source_type, source_param, protocol, "
             "flags, opened_at) values (?, ?, ?, ?, unixepoch('subsec'))");
    if (!stmt) {
        return DP_PROJECT_SESSION_OPEN_ERROR_PREPARE;
    }

    bool write_ok = ps_bind_int(prj, stmt, 1, (int)source_type)
                 && ps_bind_text(prj, stmt, 2, source_param)
                 && ps_bind_text(prj, stmt, 3, protocol)
                 && ps_bind_int64(prj, stmt, 4, DP_uint_to_llong(flags))
                 && ps_exec_write(prj, stmt, &prj->session_id);
    sqlite3_finalize(stmt);
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
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "update sessions set flags = flags | ?, "
             "closed_at = unixepoch('subsec') where session_id = ?");
    if (!stmt) {
        return DP_PROJECT_SESSION_CLOSE_ERROR_PREPARE;
    }

    bool update_ok = ps_bind_int64(prj, stmt, 1, DP_uint_to_llong(flags_to_set))
                  && ps_bind_int64(prj, stmt, 2, session_id)
                  && ps_exec_write(prj, stmt, NULL);
    sqlite3_finalize(stmt);
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

    sqlite3_stmt *stmt = prj->stmts[DP_PROJECT_STATEMENT_MESSAGE_RECORD];
    bool write_ok = ps_bind_int64(prj, stmt, 1, session_id)
                 && ps_bind_int64(prj, stmt, 2, ++prj->sequence_id)
                 && ps_bind_int64(prj, stmt, 3, DP_uint_to_llong(flags))
                 && ps_bind_int(prj, stmt, 4, (int)DP_message_type(msg))
                 && ps_bind_int64(prj, stmt, 5, DP_message_context_id(msg))
                 && ps_bind_blob(prj, stmt, 6, prj->serialize_buffer, length)
                 && ps_exec_write(prj, stmt, NULL);
    ps_clear_bindings(prj, stmt);
    if (!write_ok) {
        return DP_PROJECT_MESSAGE_RECORD_ERROR_WRITE;
    }

    return 0;
}


static const char *snapshot_sql(DP_ProjectSnapshotPersistentStatement psps)
{
    switch (psps) {
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_METADATA:
        return "insert into snapshot_metadata (snapshot_id, metadata_id, "
               "value) values (?, ?, ?)";
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_LAYER:
        return "insert into snapshot_layers (snapshot_id, layer_index, "
               "parent_index, layer_id, children, title, blend_mode, opacity, "
               "sketch_opacity, sketch_tint, flags, fill) values (?, ?, ?, "
               "?, ?, ?, ?, ?, ?, ?, ?, ?)";
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TILE:
        return "insert into snapshot_tiles (snapshot_id, layer_index, "
               "tile_index, repeat, pixels) values (?, ?, ?, ?, ?)";
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_ANNOTATION:
        return "insert into snapshot_annotations (snapshot_id, "
               "annotation_index, annotation_id, content, x, y, width, height, "
               "background_color, valign, flags) values (?, ?, ?, ?, ?, ?, ?, "
               "?, ?, ?, ?)";
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TRACK:
        return "insert into snapshot_tracks (snapshot_id, track_index, "
               "track_id, title, flags) values (?, ?, ?, ?, ?)";
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME:
        return "insert into snapshot_key_frames (snapshot_id, track_index, "
               "frame_index, title, layer_id) values (?, ?, ?, ?, ?)";
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME_LAYER:
        return "insert into snapshot_key_frame_layers (snapshot_id, "
               "track_index, frame_index, layer_id, flags) values (?, ?, ?, ?, "
               "?)";
    case DP_PROJECT_SNAPSHOT_STATEMENT_COUNT:
        break;
    }
    return NULL;
}

static void snapshot_try_discard(DP_Project *prj, long long snapshot_id)
{
    sqlite3_stmt *stmt;
    const char *sql = "delete from snapshots where snapshot_id = ?";
    if (sqlite3_prepare_v3(prj->db, sql, -1, 0, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_bind_int64(stmt, 1, snapshot_id) == SQLITE_OK) {
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                DP_warn("Error executing snapshot discard %lld", snapshot_id);
            }
        }
        else {
            DP_warn("Error binding snapshot discard %lld", snapshot_id);
        }
        sqlite3_finalize(stmt);
    }
    else {
        DP_warn("Error preparing snapshot discard %lld", snapshot_id);
    }
}

long long DP_project_snapshot_open(DP_Project *prj, unsigned int flags)
{
    DP_ASSERT(prj);
    DP_ASSERT(!(flags & DP_PROJECT_SNAPSHOT_FLAG_COMPLETE));

    if (prj->session_id == 0LL) {
        DP_error_set("No session open");
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_NO_SESSION;
    }

    if (prj->snapshot.id != 0LL) {
        DP_error_set("Snapshot %lld already open", prj->snapshot.id);
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_ALREADY_OPEN;
    }

    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "insert into snapshots (session_id, flags, taken_at) values "
             "(?, ?, unixepoch('subsec'))");
    if (!stmt) {
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_PREPARE;
    }

    long long snapshot_id;
    bool write_ok = ps_bind_int64(prj, stmt, 1, prj->session_id)
                 && ps_bind_int64(prj, stmt, 2, flags)
                 && ps_exec_write(prj, stmt, &snapshot_id);
    sqlite3_finalize(stmt);
    if (!write_ok) {
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_WRITE;
    }

    DP_ASSERT(snapshot_id > 0);
    for (int i = 0; i < DP_PROJECT_SNAPSHOT_STATEMENT_COUNT; ++i) {
        prj->snapshot.stmts[i] = ps_prepare_persistent(
            prj, snapshot_sql((DP_ProjectSnapshotPersistentStatement)i));
        if (!prj->snapshot.stmts[i]
            || !ps_bind_int64(prj, prj->snapshot.stmts[i], 1, snapshot_id)) {
            for (int j = 0; j < i; ++j) {
                sqlite3_finalize(prj->snapshot.stmts[j]);
                prj->snapshot.stmts[j] = NULL;
            }
            snapshot_try_discard(prj, snapshot_id);
            return DP_PROJECT_SNAPSHOT_OPEN_ERROR_PREPARE;
        }
    }

    DP_ASSERT(snapshot_id > 0);
    prj->snapshot.id = snapshot_id;
    prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_READY;
    return snapshot_id;
}

static void snapshot_close(DP_Project *prj)
{
    prj->snapshot.id = 0LL;
    prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_CLOSED;
    for (int i = 0; i < DP_PROJECT_SNAPSHOT_STATEMENT_COUNT; ++i) {
        sqlite3_finalize(prj->snapshot.stmts[i]);
        prj->snapshot.stmts[i] = NULL;
    }
}

int DP_project_snapshot_finish(DP_Project *prj, long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0LL);

    if (prj->snapshot.id == 0LL) {
        DP_error_set("Snapshot %lld is not open (none is)", snapshot_id);
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_NOT_OPEN;
    }

    if (prj->snapshot.id != snapshot_id) {
        DP_error_set("Snapshot %lld is not open, (%lld is)", snapshot_id,
                     prj->snapshot.id);
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_NOT_OPEN;
    }

    snapshot_close(prj);

    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "update snapshots set flags = flags | ? where snapshot_id = ?");
    if (!stmt) {
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_PREPARE;
    }

    bool write_ok =
        ps_bind_int64(prj, stmt, 1, DP_PROJECT_SNAPSHOT_FLAG_COMPLETE)
        && ps_bind_int64(prj, stmt, 2, snapshot_id)
        && ps_exec_write(prj, stmt, NULL);
    sqlite3_finalize(stmt);
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

static int project_snapshot_discard_relations(DP_Project *prj,
                                              long long snapshot_id)
{
    const char *sqls[] = {
        "delete from snapshot_key_frames where snapshot_id = ?",
        "delete from snapshot_tracks where snapshot_id = ?",
        "delete from snapshot_annotations where snapshot_id = ?",
        "delete from snapshot_tiles where snapshot_id = ?",
        "delete from snapshot_layers where snapshot_id = ?",
        "delete from snapshot_metadata where snapshot_id = ?",
    };

    int prepare_errors = 0;
    int write_errors = 0;
    for (int i = 0; i < (int)DP_ARRAY_LENGTH(sqls); ++i) {
        sqlite3_stmt *stmt = ps_prepare_ephemeral(prj, sqls[i]);
        if (stmt) {
            bool write_ok = ps_bind_int64(prj, stmt, 1, snapshot_id)
                         && ps_exec_write(prj, stmt, NULL);
            sqlite3_finalize(stmt);
            if (!write_ok) {
                DP_warn("Discard snapshot %lld: %s", snapshot_id, DP_error());
                ++write_errors;
            }
        }
        else {
            DP_warn("Discard snapshot %lld: %s", snapshot_id, DP_error());
            ++prepare_errors;
        }
    }

    if (prepare_errors == 0) {
        if (write_errors == 0) {
            return 0;
        }
        else {
            DP_error_set("Discard snapshot %lld: %d write error(s)",
                         snapshot_id, write_errors);
            return DP_PROJECT_SNAPSHOT_DISCARD_ERROR_WRITE;
        }
    }
    else {
        if (write_errors == 0) {
            DP_error_set("Discard snapshot %lld: %d prepare error(s)",
                         snapshot_id, prepare_errors);
        }
        else {
            DP_error_set(
                "Discard snapshot %lld: %d prepare error(s), %d write error(s)",
                snapshot_id, prepare_errors, write_errors);
        }
        return DP_PROJECT_SNAPSHOT_DISCARD_ERROR_PREPARE;
    }
}

int DP_project_snapshot_discard(DP_Project *prj, long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0LL);

    if (prj->snapshot.id == snapshot_id) {
        snapshot_close(prj);
    }

    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "delete from snapshots where snapshot_id = ?");
    if (!stmt) {
        return DP_PROJECT_SNAPSHOT_DISCARD_ERROR_PREPARE;
    }

    bool write_ok = ps_bind_int64(prj, stmt, 1, snapshot_id)
                 && ps_exec_write(prj, stmt, NULL);
    sqlite3_finalize(stmt);
    if (!write_ok) {
        return DP_PROJECT_SNAPSHOT_DISCARD_ERROR_WRITE;
    }

    long long changes = sqlite3_changes64(prj->db);
    int error = project_snapshot_discard_relations(prj, snapshot_id);
    if (error != 0) {
        return error;
    }

    if (changes == 0LL) {
        return DP_PROJECT_SNAPSHOT_DISCARD_NOT_FOUND;
    }

    return 0;
}

int DP_project_snapshot_discard_all_except(DP_Project *prj,
                                           long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0LL);

    sqlite3_stmt *stmt =
        ps_prepare_ephemeral(prj, "select snapshot_id from snapshots "
                                  "where snapshot_id <> ? and (flags & ?) = 0");
    if (!stmt) {
        return DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_PREPARE;
    }

    bool bind_ok =
        ps_bind_int64(prj, stmt, 1, snapshot_id)
        && ps_bind_int64(prj, stmt, 2, DP_PROJECT_SNAPSHOT_FLAG_PERSISTENT);
    if (!bind_ok) {
        sqlite3_finalize(stmt);
        return DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_READ;
    }

    bool read_error = false;
    int write_errors = 0;
    int discard_count = 0;
    while (ps_exec_step(prj, stmt, &read_error)) {
        long long snapshot_id_to_discard = sqlite3_column_int64(stmt, 0);
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
    sqlite3_finalize(stmt);

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


static bool snapshot_write_metadata_int(DP_Project *prj,
                                        DP_ProjectSnapshotMetadata id,
                                        int value)
{
    sqlite3_stmt *stmt =
        prj->snapshot.stmts[DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_METADATA];
    return ps_bind_int(prj, stmt, 2, (int)id)
        && ps_bind_int(prj, stmt, 3, value) && ps_exec_write(prj, stmt, NULL);
}

static bool
snapshot_write_metadata_int_unless_default(DP_Project *prj,
                                           DP_ProjectSnapshotMetadata id,
                                           int value, int default_value)
{
    return value == default_value
        || snapshot_write_metadata_int(prj, id, value);
}

static bool snapshot_write_background_tile(DP_Project *prj, size_t size,
                                           void *data)
{
    sqlite3_stmt *stmt =
        prj->snapshot.stmts[DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_METADATA];
    return size == 0
        || (ps_bind_int(prj, stmt, 2,
                        (int)DP_PROJECT_SNAPSHOT_METADATA_BACKGROUND_TILE)
            && ps_bind_blob(prj, stmt, 3, data, size)
            && ps_exec_write(prj, stmt, NULL));
}

static bool snapshot_write_document_metadata(DP_Project *prj,
                                             DP_DocumentMetadata *dm)
{
    return snapshot_write_metadata_int_unless_default(
               prj, DP_PROJECT_SNAPSHOT_METADATA_DPIX,
               DP_document_metadata_dpix(dm), DP_DOCUMENT_METADATA_DPIX_DEFAULT)
        && snapshot_write_metadata_int_unless_default(
               prj, DP_PROJECT_SNAPSHOT_METADATA_DPIY,
               DP_document_metadata_dpiy(dm), DP_DOCUMENT_METADATA_DPIY_DEFAULT)
        && snapshot_write_metadata_int_unless_default(
               prj, DP_PROJECT_SNAPSHOT_METADATA_FRAMERATE,
               DP_document_metadata_framerate(dm),
               DP_DOCUMENT_METADATA_FRAMERATE_DEFAULT)
        && snapshot_write_metadata_int_unless_default(
               prj, DP_PROJECT_SNAPSHOT_METADATA_FRAME_COUNT,
               DP_document_metadata_frame_count(dm),
               DP_DOCUMENT_METADATA_FRAME_COUNT_DEFAULT);
}

static bool snapshot_handle_canvas(DP_Project *prj,
                                   const DP_ResetEntryCanvas *rec)
{
    return snapshot_write_metadata_int(prj, DP_PROJECT_SNAPSHOT_METADATA_WIDTH,
                                       rec->width)
        && snapshot_write_metadata_int(prj, DP_PROJECT_SNAPSHOT_METADATA_HEIGHT,
                                       rec->height)
        && snapshot_write_background_tile(prj, rec->background_size,
                                          rec->background_data)
        && snapshot_write_document_metadata(prj, rec->dm);
}

static bool snapshot_handle_layer(DP_Project *prj,
                                  const DP_ResetEntryLayer *rel)
{
    DP_ASSERT(rel->sublayer_id == 0);
    sqlite3_stmt *stmt =
        prj->snapshot.stmts[DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_LAYER];
    DP_LayerProps *lp = rel->lp;
    DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
    size_t title_length;
    const char *title = DP_layer_props_title(lp, &title_length);
    unsigned int flags =
        DP_flag_uint(DP_layer_props_hidden(lp),
                     DP_PROJECT_SNAPSHOT_LAYER_FLAG_HIDDEN)
        | DP_flag_uint(DP_layer_props_censored(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED)
        | DP_flag_uint(child_lpl && !DP_layer_props_isolated(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_PASS_THROUGH);
    return ps_bind_int(prj, stmt, 2, rel->layer_index)
        && ps_bind_int(prj, stmt, 3, rel->parent_index)
        && ps_bind_int(prj, stmt, 4, rel->layer_id)
        && ps_bind_int(prj, stmt, 5,
                       child_lpl ? DP_layer_props_list_count(child_lpl) : -1)
        && ps_bind_text_length(prj, stmt, 6, title, title_length)
        && ps_bind_int(prj, stmt, 7, DP_layer_props_blend_mode(lp))
        && ps_bind_int(prj, stmt, 8, DP_layer_props_opacity(lp))
        && ps_bind_int(prj, stmt, 9, DP_layer_props_sketch_opacity(lp))
        && ps_bind_int64(prj, stmt, 10, DP_layer_props_sketch_tint(lp))
        && ps_bind_int64(prj, stmt, 11, flags)
        && ps_bind_int64(prj, stmt, 12, child_lpl ? 0 : rel->fill)
        && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_handle_tile(DP_Project *prj, const DP_ResetEntryTile *ret)
{
    DP_ASSERT(ret->sublayer_id == 0);
    DP_ASSERT(ret->tile_run > 0);
    DP_ASSERT(ret->size != 0);
    DP_ASSERT(ret->data);
    sqlite3_stmt *stmt =
        prj->snapshot.stmts[DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TILE];
    return ps_bind_int(prj, stmt, 2, ret->layer_index)
        && ps_bind_int(prj, stmt, 3, ret->tile_index)
        && ps_bind_int(prj, stmt, 4, ret->tile_run - 1)
        && ps_bind_blob(prj, stmt, 5, ret->data, ret->size)
        && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_handle_annotation(DP_Project *prj,
                                       const DP_ResetEntryAnnotation *rea)
{
    sqlite3_stmt *stmt =
        prj->snapshot.stmts[DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_ANNOTATION];
    DP_Annotation *a = rea->a;
    size_t text_length;
    const char *text = DP_annotation_text(a, &text_length);
    unsigned int flags =
        DP_flag_uint(DP_annotation_protect(a),
                     DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_PROTECTED);
    return ps_bind_int(prj, stmt, 2, rea->annotation_index)
        && ps_bind_int(prj, stmt, 3, DP_annotation_id(a))
        && ps_bind_text_length(prj, stmt, 4, text, text_length)
        && ps_bind_int(prj, stmt, 5, DP_annotation_x(a))
        && ps_bind_int(prj, stmt, 6, DP_annotation_y(a))
        && ps_bind_int(prj, stmt, 7, DP_annotation_width(a))
        && ps_bind_int(prj, stmt, 8, DP_annotation_height(a))
        && ps_bind_int64(prj, stmt, 9, DP_annotation_background_color(a))
        && ps_bind_int(prj, stmt, 10, DP_annotation_valign(a))
        && ps_bind_int64(prj, stmt, 11, flags)
        && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_handle_track(DP_Project *prj,
                                  const DP_ResetEntryTrack *ret)
{
    sqlite3_stmt *stmt =
        prj->snapshot.stmts[DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TRACK];
    DP_Track *t = ret->t;
    size_t title_length;
    const char *title = DP_track_title(t, &title_length);
    unsigned int flags =
        DP_flag_uint(DP_track_hidden(t), DP_PROJECT_SNAPSHOT_TRACK_FLAG_HIDDEN)
        | DP_flag_uint(DP_track_onion_skin(t),
                       DP_PROJECT_SNAPSHOT_TRACK_FLAG_ONION_SKIN);
    return ps_bind_int(prj, stmt, 2, ret->track_index)
        && ps_bind_int(prj, stmt, 3, DP_track_id(t))
        && ps_bind_text_length(prj, stmt, 4, title, title_length)
        && ps_bind_int64(prj, stmt, 5, flags) && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_insert_key_frame_layers(DP_Project *prj,
                                             const DP_ResetEntryFrame *ref)
{
    int count;
    const DP_KeyFrameLayer *kfls = DP_key_frame_layers(ref->kf, &count);
    if (count != 0) {
        sqlite3_stmt *stmt =
            prj->snapshot
                .stmts[DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME_LAYER];

        bool bind_ok = ps_bind_int(prj, stmt, 2, ref->track_index)
                    && ps_bind_int(prj, stmt, 3, ref->frame_index);
        if (!bind_ok) {
            return false;
        }

        for (int i = 0; i < count; ++i) {
            const DP_KeyFrameLayer *kfl = kfls + i;
            bool write_ok = ps_bind_int(prj, stmt, 4, kfl->layer_id)
                         && ps_bind_int64(prj, stmt, 5, kfl->flags)
                         && ps_exec_write(prj, stmt, NULL);
            if (!write_ok) {
                return false;
            }
        }
    }
    return true;
}

static bool snapshot_handle_frame(DP_Project *prj,
                                  const DP_ResetEntryFrame *ref)
{
    sqlite3_stmt *stmt =
        prj->snapshot.stmts[DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME];
    DP_KeyFrame *kf = ref->kf;
    size_t title_length;
    const char *title = DP_key_frame_title(kf, &title_length);
    return ps_bind_int(prj, stmt, 2, ref->track_index)
        && ps_bind_int(prj, stmt, 3, ref->frame_index)
        && ps_bind_text_length(prj, stmt, 4, title, title_length)
        && ps_bind_int(prj, stmt, 5, DP_key_frame_layer_id(kf))
        && ps_exec_write(prj, stmt, NULL)
        && snapshot_insert_key_frame_layers(prj, ref);
}

static bool snapshot_handle_entry(DP_Project *prj, const DP_ResetEntry *entry)
{
    switch (entry->type) {
    case DP_RESET_ENTRY_CANVAS:
        return snapshot_handle_canvas(prj, &entry->canvas);
    case DP_RESET_ENTRY_LAYER:
        return snapshot_handle_layer(prj, &entry->layer);
    case DP_RESET_ENTRY_TILE:
        return snapshot_handle_tile(prj, &entry->tile);
    case DP_RESET_ENTRY_ANNOTATION:
        return snapshot_handle_annotation(prj, &entry->annotation);
    case DP_RESET_ENTRY_TRACK:
        return snapshot_handle_track(prj, &entry->track);
    case DP_RESET_ENTRY_FRAME:
        return snapshot_handle_frame(prj, &entry->frame);
    }
    DP_warn("Unhandled snapshot entry type %d", (int)entry->type);
    return true;
}

static void snapshot_handle_entry_callback(void *user,
                                           const DP_ResetEntry *entry)
{
    DP_Project *prj = user;
    if (prj->snapshot.state == DP_PROJECT_SNAPSHOT_STATE_OK
        && !snapshot_handle_entry(prj, entry)) {
        prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_ERROR;
    }
}

int DP_project_snapshot_canvas(DP_Project *prj, long long snapshot_id,
                               DP_CanvasState *cs)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0LL);

    if (prj->snapshot.id != snapshot_id) {
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_OPEN;
    }

    if (prj->snapshot.state != DP_PROJECT_SNAPSHOT_STATE_READY) {
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_READY;
    }

    prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_OK;
    DP_ResetImageOptions options = {true, DP_RESET_IMAGE_COMPRESSION_ZSTD8LE};
    DP_reset_image_build_with(cs, &options, snapshot_handle_entry_callback,
                              prj);

    if (prj->snapshot.state != DP_PROJECT_SNAPSHOT_STATE_OK) {
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_WRITE;
    }

    return 0;
}


typedef struct DP_ProjectCanvasFromSnapshotLayer {
    DP_TransientLayerProps *tlp;
    union {
        DP_TransientLayerContent *tlc;
        DP_TransientLayerGroup *tlg;
    };
    int used;
} DP_ProjectCanvasFromSnapshotLayer;

typedef struct DP_ProjectCanvasFromSnapshotContext {
    DP_Project *prj;
    DP_TransientCanvasState *tcs;
    DP_Pixel8 *pixel_buffer;
    ZSTD_DCtx *zstd_context;
    DP_ProjectCanvasFromSnapshotLayer *layers;
    long long snapshot_id;
    unsigned int snapshot_flags;
    int layer_count;
    int root_layer_count;
    int annotation_count;
    int track_count;
} DP_ProjectCanvasFromSnapshotContext;

static bool cfs_read_header(DP_ProjectCanvasFromSnapshotContext *c)
{
    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj,
        "select "
        "(select flags from snapshots where snapshot_id = ? limit 1), "
        "(select count(*) from snapshot_layers where snapshot_id = ?), "
        "(select count(*) from snapshot_layers "
        "where snapshot_id = ? and parent_index < 0), "
        "(select count(*) from snapshot_annotations where snapshot_id = ?), "
        "(select count(*) from snapshot_tracks where snapshot_id = ?)");
    if (!stmt) {
        return false;
    }

    long long snapshot_id = c->snapshot_id;
    bool bind_ok = ps_bind_int64(prj, stmt, 1, snapshot_id)
                && ps_bind_int64(prj, stmt, 2, snapshot_id)
                && ps_bind_int64(prj, stmt, 3, snapshot_id)
                && ps_bind_int64(prj, stmt, 4, snapshot_id)
                && ps_bind_int64(prj, stmt, 5, snapshot_id);
    if (!bind_ok) {
        sqlite3_finalize(stmt);
        return false;
    }

    bool error;
    if (!ps_exec_step(prj, stmt, &error)) {
        if (!error) {
            DP_error_set("Snapshot %lld not found", snapshot_id);
        }
        sqlite3_finalize(stmt);
        return false;
    }

    c->snapshot_flags = DP_llong_to_uint(sqlite3_column_int64(stmt, 0));
    c->layer_count = sqlite3_column_int(stmt, 1);
    c->root_layer_count = sqlite3_column_int(stmt, 2);
    c->annotation_count = sqlite3_column_int(stmt, 3);
    c->track_count = sqlite3_column_int(stmt, 4);
    sqlite3_finalize(stmt);
    return true;
}

static DP_Tile *cfs_decompress_tile(DP_ProjectCanvasFromSnapshotContext *c,
                                    sqlite3_stmt *stmt, int column,
                                    const char *title)
{
    const void *data = sqlite3_column_blob(stmt, column);
    if (!data) {
        DP_warn("%s: no data in column", title);
        return NULL;
    }

    size_t size = DP_int_to_size(sqlite3_column_bytes(stmt, column));
    if (size < 4) {
        DP_warn("%s: size %zu is too short to be valid", title, size);
        return NULL;
    }

    DP_Tile *t = DP_tile_new_from_compressed_zstd8le(&c->zstd_context, 0, data,
                                                     size, c->pixel_buffer);
    if (!t) {
        DP_warn("%s: %s", title, DP_error());
        return NULL;
    }

    return t;
}

static bool cfs_read_metadata(DP_ProjectCanvasFromSnapshotContext *c)
{
    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt =
        ps_prepare_ephemeral(prj, "select metadata_id, value from "
                                  "snapshot_metadata where snapshot_id = ?");
    if (!stmt) {
        return false;
    }

    long long snapshot_id = c->snapshot_id;
    if (!ps_bind_int64(prj, stmt, 1, snapshot_id)) {
        sqlite3_finalize(stmt);
        return false;
    }

    DP_TransientCanvasState *tcs = c->tcs;
    DP_TransientDocumentMetadata *tdm =
        DP_transient_canvas_state_transient_metadata(tcs);

    bool error;
    while (ps_exec_step(prj, stmt, &error)) {
        int metadata_id = sqlite3_column_int(stmt, 0);
        switch (metadata_id) {
        case DP_PROJECT_SNAPSHOT_METADATA_WIDTH:
            DP_transient_canvas_state_width_set(tcs,
                                                sqlite3_column_int(stmt, 1));
            break;
        case DP_PROJECT_SNAPSHOT_METADATA_HEIGHT:
            DP_transient_canvas_state_height_set(tcs,
                                                 sqlite3_column_int(stmt, 1));
            break;
        case DP_PROJECT_SNAPSHOT_METADATA_BACKGROUND_TILE: {
            DP_Tile *t = cfs_decompress_tile(c, stmt, 1, "Background tile");
            if (t) {
                DP_transient_canvas_state_background_tile_set_noinc(
                    tcs, t, DP_tile_opaque(t));
            }
            break;
        }
        case DP_PROJECT_SNAPSHOT_METADATA_DPIX:
            DP_transient_document_metadata_dpix_set(
                tdm, sqlite3_column_int(stmt, 1));
            break;
        case DP_PROJECT_SNAPSHOT_METADATA_DPIY:
            DP_transient_document_metadata_dpiy_set(
                tdm, sqlite3_column_int(stmt, 1));
            break;
        case DP_PROJECT_SNAPSHOT_METADATA_FRAMERATE:
            DP_transient_document_metadata_framerate_set(
                tdm, sqlite3_column_int(stmt, 1));
            break;
        case DP_PROJECT_SNAPSHOT_METADATA_FRAME_COUNT:
            DP_transient_document_metadata_frame_count_set(
                tdm, sqlite3_column_int(stmt, 1));
            break;
        default:
            DP_warn("Unknown snapshot metadatum %d", metadata_id);
            break;
        }
    }
    sqlite3_finalize(stmt);
    return !error;
}

static DP_ProjectCanvasFromSnapshotLayer *
cfs_read_parent_layer(DP_ProjectCanvasFromSnapshotContext *c,
                      sqlite3_stmt *stmt, int layer_index)
{
    int parent_index = sqlite3_column_int(stmt, 1);
    if (parent_index < 0) {
        return NULL;
    }

    if (parent_index >= c->layer_count) {
        DP_warn("Layer %d parent index %d out of bounds", layer_index,
                parent_index);
        return NULL;
    }

    DP_ProjectCanvasFromSnapshotLayer *parent_layer = &c->layers[parent_index];
    DP_TransientLayerProps *tlp = parent_layer->tlp;
    if (!tlp) {
        DP_warn("Layer %d parent %d is null", layer_index, parent_index);
        return NULL;
    }

    if (!DP_transient_layer_props_children_noinc(tlp)) {
        DP_warn("Layer %d parent %d is not a group", layer_index, parent_index);
        return NULL;
    }

    return parent_layer;
}

static bool cfs_read_layer(DP_ProjectCanvasFromSnapshotContext *c,
                           sqlite3_stmt *stmt, int *in_out_increments)
{
    int layer_index = sqlite3_column_int(stmt, 0);
    if (layer_index < 0 || layer_index >= c->layer_count) {
        DP_warn("Layer index %d out of bounds", layer_index);
        return false;
    }

    DP_ProjectCanvasFromSnapshotLayer *layer = &c->layers[layer_index];
    if (layer->tlp) {
        DP_warn("Duplicate layer index %d", layer_index);
        return false;
    }

    int layer_id = sqlite3_column_int(stmt, 2);
    if (layer_id <= 0 || layer_id > UINT16_MAX) {
        DP_warn("Layer %d has invalid id %d", layer_index, layer_id);
        return false;
    }

    int child_count = sqlite3_column_int(stmt, 3);
    if (child_count > 0) {
        int max_child_count = c->layer_count - layer_index - 1;
        if (child_count > max_child_count) {
            DP_warn("Layer %d purports %d child(ren) of at most possible %d",
                    layer_index, child_count, max_child_count);
            child_count = max_child_count;
        }
    }

    layer->tlp =
        DP_transient_layer_props_new_init_with_transient_children_noinc(
            layer_id, child_count < 0 ? NULL
                                      : DP_transient_layer_props_list_new_init(
                                            child_count));

    const char *title = (const char *)sqlite3_column_text(stmt, 4);
    if (title) {
        int title_length = sqlite3_column_bytes(stmt, 4);
        if (title_length > 0) {
            DP_transient_layer_props_title_set(layer->tlp, title,
                                               DP_int_to_size(title_length));
        }
    }

    int blend_mode = sqlite3_column_int(stmt, 5);
    if (DP_blend_mode_valid_for_layer(blend_mode)) {
        DP_transient_layer_props_blend_mode_set(layer->tlp, blend_mode);
    }
    else {
        DP_warn("Layer %d has invalid blend mode %d", layer_index, blend_mode);
    }

    int opacity = sqlite3_column_int(stmt, 6);
    if (opacity >= 0 && opacity <= DP_BIT15) {
        DP_transient_layer_props_opacity_set(layer->tlp,
                                             DP_int_to_uint16(opacity));
    }
    else {
        DP_warn("Layer %d opacity %d out of bounds", layer_index, opacity);
    }

    int sketch_opacity = sqlite3_column_int(stmt, 7);
    if (sketch_opacity >= 0 && sketch_opacity <= DP_BIT15) {
        DP_transient_layer_props_sketch_opacity_set(
            layer->tlp, DP_int_to_uint16(sketch_opacity));
    }
    else {
        DP_warn("Layer %d sketch opacity %d out of bounds", layer_index,
                sketch_opacity);
    }

    long long sketch_tint = sqlite3_column_int64(stmt, 8);
    if (sketch_tint >= 0LL && sketch_tint <= UINT32_MAX) {
        DP_transient_layer_props_sketch_tint_set(
            layer->tlp, DP_llong_to_uint32(sketch_tint));
    }
    else {
        DP_warn("Layer %d sketch tint %lld out of bounds", layer_index,
                sketch_tint);
    }

    unsigned int flags = DP_int_to_uint(sqlite3_column_int(stmt, 9));
    if (flags & DP_PROJECT_SNAPSHOT_LAYER_FLAG_HIDDEN) {
        DP_transient_layer_props_hidden_set(layer->tlp, true);
    }

    if (flags & DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED) {
        DP_transient_layer_props_censored_set(layer->tlp, true);
    }

    if (flags & DP_PROJECT_SNAPSHOT_LAYER_FLAG_PASS_THROUGH) {
        if (child_count >= 0) {
            DP_transient_layer_props_isolated_set(layer->tlp, false);
        }
        else {
            DP_warn("Layer %d has pass-through flat set, but is not a group",
                    layer_index);
        }
    }

    long long fill = sqlite3_column_int64(stmt, 10);
    if (fill < 0LL || fill > UINT32_MAX) {
        DP_warn("Layer %d fill %lld out of bounds", layer_index, fill);
        fill = 0;
    }
    else if (fill != 0 && child_count >= 0) {
        DP_warn("Layer %d has fill, but is a group", layer_index);
    }

    int width = DP_transient_canvas_state_width(c->tcs);
    int height = DP_transient_canvas_state_height(c->tcs);
    if (child_count < 0) {
        if (fill == 0) {
            layer->tlc =
                DP_transient_layer_content_new_init(width, height, NULL);
        }
        else {
            DP_Tile *t = DP_tile_new_from_bgra(0, DP_llong_to_uint32(fill));
            layer->tlc = DP_transient_layer_content_new_init(width, height, t);
            DP_tile_decref(t);
        }
    }
    else {
        layer->tlg =
            DP_transient_layer_group_new_init_with_transient_children_noinc(
                width, height, DP_transient_layer_list_new_init(child_count));
        layer->used = 0;
    }

    DP_ProjectCanvasFromSnapshotLayer *parent_layer =
        cfs_read_parent_layer(c, stmt, layer_index);
    DP_TransientLayerPropsList *parent_tlpl;
    DP_TransientLayerList *parent_tll;
    int index_to_set;
    if (parent_layer) {
        index_to_set = parent_layer->used++;
        parent_tlpl = (DP_TransientLayerPropsList *)
            DP_transient_layer_props_children_noinc(parent_layer->tlp);
        if (index_to_set == DP_transient_layer_props_list_count(parent_tlpl)) {
            ++*in_out_increments;
            parent_tlpl = DP_transient_layer_props_transient_children(
                parent_layer->tlp, 1);
            parent_tll = DP_transient_layer_group_transient_children(
                parent_layer->tlg, 1);
        }
        else {
            parent_tll = (DP_TransientLayerList *)
                DP_transient_layer_group_children_noinc(parent_layer->tlg);
        }
    }
    else {
        index_to_set = c->root_layer_count++;
        parent_tlpl = (DP_TransientLayerPropsList *)
            DP_transient_canvas_state_layer_props_noinc(c->tcs);
        if (index_to_set == DP_transient_layer_props_list_count(parent_tlpl)) {
            ++*in_out_increments;
            parent_tlpl =
                DP_transient_canvas_state_transient_layer_props(c->tcs, 1);
            parent_tll = DP_transient_canvas_state_transient_layers(c->tcs, 1);
        }
        else {
            parent_tll =
                (DP_TransientLayerList *)DP_transient_canvas_state_layers_noinc(
                    c->tcs);
        }
    }

    DP_transient_layer_props_list_set_transient_noinc(parent_tlpl, layer->tlp,
                                                      index_to_set);
    if (child_count < 0) {
        DP_transient_layer_list_set_transient_content_noinc(
            parent_tll, layer->tlc, index_to_set);
    }
    else {
        DP_transient_layer_list_set_transient_group_noinc(
            parent_tll, layer->tlg, index_to_set);
    }
    return true;
}

static int cfs_clamp_layers(DP_ProjectCanvasFromSnapshotContext *c)
{
    // Depending on how things went, we may have leftover reserved layer entries
    // that are still NULL. That's not valid, so we have to clamp them off.
    int clamps = 0;
    int root_used = c->root_layer_count;
    DP_TransientLayerPropsList *root_tlpl = (DP_TransientLayerPropsList *)
        DP_transient_canvas_state_layer_props_noinc(c->tcs);
    if (root_used < DP_transient_layer_props_list_count(root_tlpl)) {
        ++clamps;
        DP_TransientLayerList *root_tll =
            (DP_TransientLayerList *)DP_transient_canvas_state_layers_noinc(
                c->tcs);
        DP_transient_layer_list_clamp(root_tll, root_used);
        DP_transient_layer_props_list_clamp(root_tlpl, root_used);
    }

    int layer_count = c->layer_count;
    for (int i = 0; i < layer_count; ++i) {
        DP_ProjectCanvasFromSnapshotLayer *layer = &c->layers[i];
        int used = layer->used;
        if (used >= 0) {
            DP_TransientLayerPropsList *tlpl = (DP_TransientLayerPropsList *)
                DP_transient_layer_props_children_noinc(layer->tlp);
            if (used < DP_transient_layer_props_list_count(tlpl)) {
                ++clamps;
                DP_TransientLayerList *tll = (DP_TransientLayerList *)
                    DP_transient_layer_group_children_noinc(layer->tlg);
                DP_transient_layer_list_clamp(tll, used);
                DP_transient_layer_props_list_clamp(tlpl, used);
            }
        }
    }

    return clamps;
}

static bool cfs_read_layers(DP_ProjectCanvasFromSnapshotContext *c)
{
    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select layer_index, parent_index, layer_id, children, title, "
             "blend_mode, opacity, sketch_opacity, sketch_tint, flags, fill "
             "from snapshot_layers where snapshot_id = ? order by layer_index");
    if (!stmt) {
        return false;
    }

    if (!ps_bind_int64(prj, stmt, 1, c->snapshot_id)) {
        sqlite3_finalize(stmt);
        return false;
    }

    bool error;
    int increments = 0;
    while (ps_exec_step(prj, stmt, &error)) {
        cfs_read_layer(c, stmt, &increments);
    }
    sqlite3_finalize(stmt);
    if (increments != 0) {
        DP_warn("Had to resize %d layer list(s)", increments);
    }

    int clamps = cfs_clamp_layers(c);
    if (clamps != 0) {
        DP_warn("Had to clamp %d layer list(s)", clamps);
    }

    return !error;
}

static bool cfs_read_tiles(DP_ProjectCanvasFromSnapshotContext *c)
{
    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select layer_index, tile_index, repeat, pixels "
             "from snapshot_tiles where snapshot_id = ?");
    if (!stmt) {
        return false;
    }

    if (!ps_bind_int64(prj, stmt, 1, c->snapshot_id)) {
        sqlite3_finalize(stmt);
        return false;
    }

    int layer_count = c->layer_count;
    int tile_count =
        DP_tile_total_round(DP_transient_canvas_state_width(c->tcs),
                            DP_transient_canvas_state_height(c->tcs));
    size_t max_pixel_size = DP_compress_zstd_bounds(DP_TILE_COMPRESSED_BYTES);

    bool error;
    while (ps_exec_step(prj, stmt, &error)) {
        int layer_index = sqlite3_column_int(stmt, 0);
        if (layer_index < 0 || layer_index >= layer_count) {
            DP_warn("Tile layer index %d out of bounds", layer_index);
            continue;
        }

        DP_ProjectCanvasFromSnapshotLayer *layer = &c->layers[layer_index];
        if (!layer->tlp) {
            DP_warn("Tile layer %d is null", layer_index);
            continue;
        }
        else if (layer->used >= 0) {
            DP_warn("Tile layer %d is a group", layer_index);
            continue;
        }

        int tile_index = sqlite3_column_int(stmt, 1);
        if (tile_index < 0 || tile_index >= tile_count) {
            DP_warn("Tile index %d of layer %d out of bounds", tile_index,
                    layer_index);
            continue;
        }

        int repeat = sqlite3_column_int(stmt, 2);
        int max_repeat = tile_count - tile_index - 1;
        if (repeat < 0) {
            DP_warn("Tile index %d of layer %d repeat %d less than zero",
                    tile_index, layer_index, repeat);
            repeat = 0;
        }
        else if (repeat > max_repeat) {
            DP_warn("Tile index %d of layer %d repeat %d beyond max repeat %d",
                    tile_index, layer_index, repeat, max_repeat);
            repeat = max_repeat;
        }

        const unsigned char *pixels = sqlite3_column_blob(stmt, 3);
        if (!pixels) {
            DP_warn("Tile index %d of layer %d has null pixel data", tile_index,
                    layer_index);
            continue;
        }

        size_t pixel_bytes = DP_int_to_size(sqlite3_column_bytes(stmt, 3));
        if (pixel_bytes < 4) {
            DP_warn("Tile index %d of layer %d has too little pixel data",
                    tile_index, layer_index);
            continue;
        }
        else if (pixel_bytes > max_pixel_size) {
            DP_warn("Tile index %d of layer %d has too much pixel data",
                    tile_index, layer_index);
            continue;
        }

        DP_TransientLayerContent *tlc = layer->tlc;
        if (pixel_bytes == 4
            && memcmp(pixels, (unsigned char[]){0, 0, 0, 0}, 4) == 0) {
            for (int i = 0; i <= repeat; ++i) {
                DP_transient_layer_content_tile_set_noinc(tlc, NULL,
                                                          tile_index + i);
            }
        }
        else {
            DP_Tile *t = DP_tile_new_from_compressed_zstd8le(
                &c->zstd_context, 0, pixels, pixel_bytes, c->pixel_buffer);
            if (!t) {
                DP_warn("Tile index %d of layer %d failed to decompress: %s",
                        tile_index, layer_index, DP_error());
                continue;
            }
            for (int i = 0; i <= repeat; ++i) {
                DP_transient_layer_content_tile_set_noinc(tlc, t,
                                                          tile_index + i);
            }
            if (repeat != 0) {
                DP_tile_incref_by(t, repeat);
            }
        }
    }
    sqlite3_finalize(stmt);
    return !error;
}

static void cfs_context_dispose(DP_ProjectCanvasFromSnapshotContext *c)
{
    DP_decompress_zstd_free(&c->zstd_context);
    DP_free(c->pixel_buffer);
    DP_transient_canvas_state_decref_nullable(c->tcs);
}

DP_CanvasState *DP_project_canvas_from_snapshot(DP_Project *prj,
                                                DP_DrawContext *dc,
                                                long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id);

    DP_ProjectCanvasFromSnapshotContext c = {
        prj, NULL, NULL, NULL, NULL, snapshot_id, 0, 0, 0, 0, 0};
    if (!cfs_read_header(&c)) {
        return NULL;
    }

    if (!(c.snapshot_flags & DP_PROJECT_SNAPSHOT_FLAG_COMPLETE)) {
        DP_warn("Snapshot %lld is incomplete", snapshot_id);
    }

    c.tcs = DP_transient_canvas_state_new_init();
    c.pixel_buffer = DP_malloc(sizeof(DP_Pixel8) * DP_TILE_LENGTH);

    if (!cfs_read_metadata(&c)) {
        cfs_context_dispose(&c);
        return NULL;
    }

    // Once we got metadata, we sort of have a usable canvas. So any errors
    // after this should be non-fatal if possible.

    if (c.layer_count > 0 && c.root_layer_count > 0) {
        c.layers = DP_malloc(sizeof(*c.layers) * DP_int_to_size(c.layer_count));
        for (int i = 0; i < c.layer_count; ++i) {
            c.layers[i] = (DP_ProjectCanvasFromSnapshotLayer){NULL, {NULL}, -1};
        }
        DP_transient_canvas_state_transient_layers(c.tcs, c.root_layer_count);
        DP_transient_canvas_state_transient_layer_props(c.tcs,
                                                        c.root_layer_count);
        c.root_layer_count = 0; // Re-use this as the "used" value.
        if (!cfs_read_layers(&c)) {
            DP_warn("Error reading layers: %s", DP_error());
        }
    }

    if (!cfs_read_tiles(&c)) {
        DP_warn("Error reading tiles: %s", DP_error());
    }

    DP_transient_canvas_state_layer_routes_reindex(c.tcs, dc);
    DP_transient_canvas_state_timeline_cleanup(c.tcs);

    DP_CanvasState *cs = DP_transient_canvas_state_persist(c.tcs);
    c.tcs = NULL;
    cfs_context_dispose(&c);
    return cs;
}

DP_CanvasState *DP_project_canvas_from_latest_snapshot(DP_Project *prj,
                                                       DP_DrawContext *dc)
{
    DP_ASSERT(prj);
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj,
        "select snapshot_id from snapshots order by taken_at desc limit 1");
    if (!stmt) {
        return NULL;
    }

    bool error;
    long long snapshot_id;
    if (ps_exec_step(prj, stmt, &error)) {
        snapshot_id = sqlite3_column_int64(stmt, 0);
    }
    else {
        if (!error) {
            DP_error_set("No snapshot to load found");
        }
        snapshot_id = -1LL;
    }
    sqlite3_finalize(stmt);

    if (snapshot_id <= 0LL) {
        return NULL;
    }

    return DP_project_canvas_from_snapshot(prj, dc, snapshot_id);
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
