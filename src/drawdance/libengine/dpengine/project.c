// SPDX-License-Identifier: GPL-3.0-or-later
#include "project.h"
#include "annotation.h"
#include "annotation_list.h"
#include "canvas_history.h"
#include "canvas_state.h"
#include "compress.h"
#include "document_metadata.h"
#include "draw_context.h"
#include "key_frame.h"
#include "layer_content.h"
#include "layer_group.h"
#include "layer_list.h"
#include "layer_props.h"
#include "layer_props_list.h"
#include "local_state.h"
#include "selection.h"
#include "selection_set.h"
#include "snapshots.h"
#include "tile.h"
#include "timeline.h"
#include "track.h"
#include <dpcommon/atomic.h>
#include <dpcommon/binary.h>
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpcommon/output.h>
#include <dpcommon/perf.h>
#include <dpcommon/threading.h>
#include <dpcommon/worker.h>
#include <dpdb/sql.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/ids.h>
#include <dpmsg/message.h>
#include <dpmsg/protover.h>

#define DP_PERF_CONTEXT "project"


#define DP_PROJECT_STR(X)  #X
#define DP_PROJECT_XSTR(X) DP_PROJECT_STR(X)

#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_GROUP           (1u << 0u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_HIDDEN          (1u << 1u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED_REMOTE (1u << 2u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_PASS_THROUGH    (1u << 3u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_CLIP            (1u << 4u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED_LOCAL  (1u << 5u)
#define DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_PROTECTED  (1u << 0u)
#define DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_ALIAS      (1u << 1u)
#define DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_RASTERIZE  (1u << 2u)
#define DP_PROJECT_SNAPSHOT_TRACK_FLAG_HIDDEN          (1u << 0u)
#define DP_PROJECT_SNAPSHOT_TRACK_FLAG_ONION_SKIN      (1u << 1u)

#define DP_PROJECT_SQL_MAIN_SAV(A, B, COND) ((COND) ? A "sav." B : A "" B)


typedef enum DP_ProjectPersistentStatement {
    DP_PROJECT_STATEMENT_MESSAGE_RECORD,
    DP_PROJECT_STATEMENT_SESSION_TIMES_SELECT,
    DP_PROJECT_STATEMENT_COUNT,
} DP_ProjectPersistentStatement;

typedef enum DP_ProjectSnapshotState {
    DP_PROJECT_SNAPSHOT_STATE_CLOSED,
    DP_PROJECT_SNAPSHOT_STATE_READY,
    DP_PROJECT_SNAPSHOT_STATE_ERROR,
    DP_PROJECT_SNAPSHOT_STATE_OK,
    DP_PROJECT_SNAPSHOT_STATE_NULL,
} DP_ProjectSnapshotState;

typedef enum DP_ProjectSnapshotPersistentStatement {
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_METADATA,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_LAYER,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_SUBLAYER,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TILE,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_SUBLAYER_TILE,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_SELECTION_TILE,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_ANNOTATION,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TRACK,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME_LAYER,
    DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_MESSAGE,
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
    DP_PROJECT_SNAPSHOT_METADATA_FRAMERATE_FRACTION = 8,
    DP_PROJECT_SNAPSHOT_METADATA_FRAME_RANGE_FIRST = 9,
    DP_PROJECT_SNAPSHOT_METADATA_FRAME_RANGE_LAST = 10,
    DP_PROJECT_SNAPSHOT_METADATA_SEQUENCE_ID = 11,
} DP_ProjectSnapshotMetadata;

typedef struct DP_ProjectSnapshot {
    long long id;
    long long sequence_id;
    DP_ProjectSnapshotState state;
    bool attached;
    bool merge_sublayers;
    bool include_selections;
    bool has_sublayers;
    bool has_selections;
    sqlite3_stmt *stmts[DP_PROJECT_SNAPSHOT_STATEMENT_COUNT];
    DP_Mutex *mutex;
} DP_ProjectSnapshot;

struct DP_Project {
    sqlite3 *db;
    long long session_id;
    long long sequence_id;
    DP_ProjectSnapshot snapshot;
    DP_Atomic cancel;
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

static DP_ProjectOpenResult make_open_error(int error, int sql_result)
{
    return (DP_ProjectOpenResult){NULL, error, sql_result};
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


static DP_ProjectCheckResult project_check_error(int error)
{
    return (DP_ProjectCheckResult){DP_PROJECT_CHECK_NONE, .error = error};
}

DP_ProjectCheckResult DP_project_check(const unsigned char *buf, size_t size)
{
    if (size < 72) {
        return project_check_error(DP_PROJECT_CHECK_ERROR_HEADER);
    }

    // "SQLite format 3\0" in ASCII
    unsigned char magic[] = {0x53, 0x51, 0x4c, 0x69, 0x74, 0x65, 0x20, 0x66,
                             0x6f, 0x72, 0x6d, 0x61, 0x74, 0x20, 0x33, 0x00};
    if (memcmp(buf, magic, sizeof(magic)) != 0) {
        return project_check_error(DP_PROJECT_CHECK_ERROR_MAGIC);
    }

    int application_id = DP_read_bigendian_int32(buf + 68);
    DP_ProjectCheckType type;
    switch (application_id) {
    case DP_PROJECT_APPLICATION_ID:
        type = DP_PROJECT_CHECK_PROJECT;
        break;
    case DP_PROJECT_CANVAS_APPLICATION_ID:
        type = DP_PROJECT_CHECK_CANVAS;
        break;
    default:
        return project_check_error(DP_PROJECT_CHECK_ERROR_APPLICATION_ID);
    }

    int user_version = DP_read_bigendian_int32(buf + 60);
    if (user_version < 1) {
        return project_check_error(DP_PROJECT_CHECK_ERROR_USER_VERSION);
    }

    return (DP_ProjectCheckResult){type, .version = user_version};
}

DP_ProjectCheckResult DP_project_check_path(const char *path)
{
    DP_Input *input = DP_file_input_new_from_path(path);
    if (!input) {
        return project_check_error(DP_PROJECT_CHECK_ERROR_OPEN);
    }

    unsigned char buf[72];
    bool error;
    size_t read = DP_input_read(input, buf, sizeof(buf), &error);
    DP_input_free(input);
    if (error) {
        return project_check_error(DP_PROJECT_CHECK_ERROR_READ);
    }

    return DP_project_check(buf, read);
}


static sqlite3_stmt *prepare(sqlite3 *db, const char *sql, unsigned int flags,
                             int *out_result)
{
    DP_debug("Prepare statement: %s", sql);
    sqlite3_stmt *stmt;
    int prepare_result = sqlite3_prepare_v3(db, sql, -1, flags, &stmt, NULL);
    assign_result(prepare_result, out_result);
    if (is_ok(prepare_result)) {
        return stmt;
    }
    else {
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
    assign_result(exec_result, out_result);
    if (is_ok(exec_result)) {
        return true;
    }
    else {
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
    assign_result(step_result, out_result);
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
    assign_result(step_result, out_result);
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
        DP_error_set("Expected %s '%s', but got '%s'", pragma,
                     expected ? expected : "NULL", text ? text : "NULL");
        return false;
    }
}

#define TRY_SET_DB_CONFIG(DB, OP, VALUE)                                     \
    do {                                                                     \
        int _db_config_result =                                              \
            sqlite3_db_config(db, SQLITE_DBCONFIG_##OP, VALUE, (int *)NULL); \
        if (_db_config_result != SQLITE_OK) {                                \
            DP_warn("Error %d setting database config " #OP ": %s",          \
                    _db_config_result, sqlite3_errstr(_db_config_result));   \
        }                                                                    \
    } while (0)

static void set_db_configs(sqlite3 *db)
{
    TRY_SET_DB_CONFIG(db, ENABLE_FKEY, 0);
    TRY_SET_DB_CONFIG(db, ENABLE_TRIGGER, 0);
    TRY_SET_DB_CONFIG(db, ENABLE_LOAD_EXTENSION, 0);
    TRY_SET_DB_CONFIG(db, TRUSTED_SCHEMA, 0);
    TRY_SET_DB_CONFIG(db, ENABLE_ATTACH_CREATE, 1);
}

#define PRAGMA_SETUP(DB, PREFIX, OUT_OK, OUT_SQL_RESULT)                       \
    do {                                                                       \
        OUT_OK =                                                               \
            exec_write_stmt(DB, "pragma " PREFIX "locking_mode = exclusive",   \
                            "setting exclusive locking mode", &OUT_SQL_RESULT) \
            && exec_write_stmt(DB, "pragma " PREFIX "journal_mode = memory",   \
                               "setting journal mode to memory",               \
                               &OUT_SQL_RESULT)                                \
            && exec_write_stmt(DB, "pragma " PREFIX "synchronous = off",       \
                               "setting synchronous off", &OUT_SQL_RESULT)     \
            && exec_text_stmt(DB, "pragma " PREFIX "locking_mode", NULL,       \
                              check_pragma_result,                             \
                              (char *[]){"exclusive", "locking_mode"},         \
                              &OUT_SQL_RESULT);                                \
    } while (0)

#define PRAGMA_READY(DB, PREFIX, OUT_OK, OUT_SQL_RESULT)                     \
    do {                                                                     \
        OUT_OK =                                                             \
            exec_write_stmt(DB, "pragma " PREFIX "journal_mode = off",       \
                            "setting journal mode to off", &OUT_SQL_RESULT); \
    } while (0)

#define IS_EMPTY_DB(DB, PREFIX, OUT_OK, OUT_EMPTY, OUT_SQL_RESULT)             \
    do {                                                                       \
        int _value;                                                            \
        if (exec_int_stmt(db, "select 1 from " PREFIX "sqlite_master limit 1", \
                          0, &_value, &OUT_SQL_RESULT)) {                      \
            OUT_EMPTY = (_value == 0);                                         \
            OUT_OK = true;                                                     \
        }                                                                      \
        else {                                                                 \
            OUT_OK = false;                                                    \
        }                                                                      \
    } while (0)

#define INIT_HEADER(DB, PREFIX, SNAPSHOT_ONLY, OUT_OK, OUT_SQL_RESULT)      \
    do {                                                                    \
        const char *_application_id_sql;                                    \
        const char *_user_version_sql;                                      \
        if (SNAPSHOT_ONLY) {                                                \
            _application_id_sql =                                           \
                "pragma " PREFIX "application_id = " DP_PROJECT_XSTR(       \
                    DP_PROJECT_CANVAS_APPLICATION_ID);                      \
            _user_version_sql =                                             \
                "pragma " PREFIX "user_version = " DP_PROJECT_XSTR(         \
                    DP_PROJECT_CANVAS_USER_VERSION);                        \
        }                                                                   \
        else {                                                              \
            _application_id_sql =                                           \
                "pragma " PREFIX "application_id = " DP_PROJECT_XSTR(       \
                    DP_PROJECT_APPLICATION_ID);                             \
            _user_version_sql =                                             \
                "pragma " PREFIX                                            \
                "user_version = " DP_PROJECT_XSTR(DP_PROJECT_USER_VERSION); \
        }                                                                   \
        OUT_OK = exec_write_stmt(DB, _application_id_sql,                   \
                                 "setting application_id", &OUT_SQL_RESULT) \
              && exec_write_stmt(DB, _user_version_sql,                     \
                                 "setting user_version", &OUT_SQL_RESULT);  \
    } while (0)

#define CHECK_HEADER(DB, PREFIX, SNAPSHOT_ONLY, OUT_RESULT, OUT_SQL_RESULT)    \
    do {                                                                       \
        int _application_id, _user_version;                                    \
        bool _exec_ok = exec_int_stmt(DB, "pragma " PREFIX "application_id",   \
                                      -1, &_application_id, &OUT_SQL_RESULT)   \
                     && exec_int_stmt(DB, "pragma " PREFIX "user_version", -1, \
                                      &_user_version, &OUT_SQL_RESULT);        \
        if (_exec_ok) {                                                        \
            int _expected_application_id;                                      \
            int _expected_user_version;                                        \
            if (SNAPSHOT_ONLY) {                                               \
                _expected_application_id = DP_PROJECT_CANVAS_APPLICATION_ID;   \
                _expected_user_version = DP_PROJECT_CANVAS_USER_VERSION;       \
            }                                                                  \
            else {                                                             \
                _expected_application_id = DP_PROJECT_APPLICATION_ID;          \
                _expected_user_version = DP_PROJECT_USER_VERSION;              \
            }                                                                  \
                                                                               \
            if (_application_id != _expected_application_id) {                 \
                DP_error_set("File has incorrect application id %d",           \
                             _application_id);                                 \
                OUT_RESULT = DP_PROJECT_OPEN_ERROR_HEADER_MISMATCH;            \
            }                                                                  \
            else if (_user_version != _expected_user_version) {                \
                DP_error_set("File has unknown user version %d",               \
                             _user_version);                                   \
                OUT_RESULT = DP_PROJECT_OPEN_ERROR_HEADER_MISMATCH;            \
            }                                                                  \
            else {                                                             \
                OUT_RESULT = 0;                                                \
            }                                                                  \
        }                                                                      \
        else {                                                                 \
            OUT_RESULT = DP_PROJECT_OPEN_ERROR_HEADER_READ;                    \
        }                                                                      \
    } while (0)

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

#define APPLY_MIGRATIONS(DB, PREFIX, OUT_OK, OUT_SQL_RESULT)                   \
    do {                                                                       \
        if (!exec_write_stmt(                                                  \
                DB,                                                            \
                "create table if not exists " PREFIX "migrations (\n"          \
                "    migration_id integer primary key not null)\n"             \
                "strict",                                                      \
                "creating migrations table", &OUT_SQL_RESULT)) {               \
            OUT_OK = false;                                                    \
            break;                                                             \
        }                                                                      \
                                                                               \
        sqlite3_stmt *_insert_migration_stmt = prepare(                        \
            DB,                                                                \
            "insert or ignore into " PREFIX "migrations (migration_id)\n"      \
            "values (?)",                                                      \
            0, &OUT_SQL_RESULT);                                               \
        if (!_insert_migration_stmt) {                                         \
            OUT_OK = false;                                                    \
            break;                                                             \
        }                                                                      \
                                                                               \
        const char *_migrations[] = {                                          \
            /* Migration 1: initial setup. */                                  \
            "create table " PREFIX "sessions (\n"                              \
            "    session_id integer primary key autoincrement not null,\n"     \
            "    source_type integer not null,\n"                              \
            "    source_param text not null,\n"                                \
            "    protocol text not null,\n"                                    \
            "    flags integer not null,\n"                                    \
            "    opened_at real not null,\n"                                   \
            "    closed_at real,\n"                                            \
            "    thumbnail blob)\n"                                            \
            "strict;\n"                                                        \
            "create table " PREFIX "messages (\n"                              \
            "    session_id integer not null,\n"                               \
            "    sequence_id integer not null,\n"                              \
            "    recorded_at real not null,\n"                                 \
            "    flags integer not null,\n"                                    \
            "    type integer not null,\n"                                     \
            "    context_id integer not null,\n"                               \
            "    body blob,\n"                                                 \
            "    primary key (session_id, sequence_id))\n"                     \
            "strict, without rowid;\n"                                         \
            "create table " PREFIX "snapshots (\n"                             \
            "    snapshot_id integer primary key autoincrement not null,\n"    \
            "    session_id integer not null,\n"                               \
            "    flags integer not null,\n"                                    \
            "    taken_at real not null,\n"                                    \
            "    thumbnail blob)\n"                                            \
            "strict;\n"                                                        \
            "create table " PREFIX "snapshot_metadata (\n"                     \
            "    snapshot_id integer not null,\n"                              \
            "    metadata_id integer not null,\n"                              \
            "    value any,\n"                                                 \
            "    primary key (snapshot_id, metadata_id))\n"                    \
            "strict;\n"                                                        \
            "create table " PREFIX "snapshot_layers (\n"                       \
            "    snapshot_id integer not null,\n"                              \
            "    layer_index integer not null,\n"                              \
            "    parent_index integer not null,\n"                             \
            "    layer_id integer not null,\n"                                 \
            "    title text not null,\n"                                       \
            "    blend_mode integer not null,\n"                               \
            "    opacity integer not null,\n"                                  \
            "    sketch_opacity integer not null,\n"                           \
            "    sketch_tint integer not null,\n"                              \
            "    flags integer not null,\n"                                    \
            "    fill integer not null,\n"                                     \
            "    primary key (snapshot_id, layer_index))\n"                    \
            "strict, without rowid;\n"                                         \
            "create table " PREFIX "snapshot_sublayers (\n"                    \
            "    snapshot_id integer not null,\n"                              \
            "    sublayer_index integer not null,\n"                           \
            "    layer_index integer not null,\n"                              \
            "    sublayer_id integer not null,\n"                              \
            "    blend_mode integer not null,\n"                               \
            "    opacity integer not null,\n"                                  \
            "    flags integer not null,\n"                                    \
            "    fill integer not null,\n"                                     \
            "    primary key (snapshot_id, sublayer_index))\n"                 \
            "strict, without rowid;\n"                                         \
            "create table " PREFIX "snapshot_tiles (\n"                        \
            "    snapshot_id integer not null,\n"                              \
            "    layer_index integer not null,\n"                              \
            "    tile_index integer not null,\n"                               \
            "    context_id integer not null,\n"                               \
            "    repeat integer not null,\n"                                   \
            "    pixels blob not null,\n"                                      \
            "    primary key (snapshot_id, layer_index, tile_index))\n"        \
            "strict;\n"                                                        \
            "create table " PREFIX "snapshot_sublayer_tiles (\n"               \
            "    snapshot_id integer not null,\n"                              \
            "    sublayer_index integer not null,\n"                           \
            "    tile_index integer not null,\n"                               \
            "    repeat integer not null,\n"                                   \
            "    pixels blob not null,\n"                                      \
            "    primary key (snapshot_id, sublayer_index, tile_index))\n"     \
            "strict;\n"                                                        \
            "create table " PREFIX "snapshot_selection_tiles (\n"              \
            "    snapshot_id integer not null,\n"                              \
            "    selection_id integer not null,\n"                             \
            "    context_id integer not null,\n"                               \
            "    tile_index integer not null,\n"                               \
            "    mask blob,\n"                                                 \
            "    primary key (snapshot_id, selection_id, context_id,\n"        \
            "                 tile_index))\n"                                  \
            "strict;\n"                                                        \
            "create table " PREFIX "snapshot_annotations (\n"                  \
            "    snapshot_id integer not null,\n"                              \
            "    annotation_index integer not null,\n"                         \
            "    annotation_id integer not null,\n"                            \
            "    content text not null,\n"                                     \
            "    x integer not null,\n"                                        \
            "    y integer not null,\n"                                        \
            "    width integer not null,\n"                                    \
            "    height integer not null,\n"                                   \
            "    background_color integer not null,\n"                         \
            "    valign integer not null,\n"                                   \
            "    flags integer not null,\n"                                    \
            "    primary key (snapshot_id, annotation_index))\n"               \
            "strict;\n"                                                        \
            "create table " PREFIX "snapshot_tracks (\n"                       \
            "    snapshot_id integer not null,\n"                              \
            "    track_index integer not null,\n"                              \
            "    track_id integer not null,\n"                                 \
            "    title text not null,\n"                                       \
            "    flags integer not null,\n"                                    \
            "    primary key (snapshot_id, track_index))\n"                    \
            "strict, without rowid;\n"                                         \
            "create table " PREFIX "snapshot_key_frames (\n"                   \
            "    snapshot_id integer not null,\n"                              \
            "    track_index integer not null,\n"                              \
            "    frame_index integer not null,\n"                              \
            "    title text not null,\n"                                       \
            "    layer_id integer not null,\n"                                 \
            "    primary key (snapshot_id, track_index, frame_index))\n"       \
            "strict, without rowid;\n"                                         \
            "create table " PREFIX "snapshot_key_frame_layers (\n"             \
            "    snapshot_id integer not null,\n"                              \
            "    track_index integer not null,\n"                              \
            "    frame_index integer not null,\n"                              \
            "    layer_id integer not null,\n"                                 \
            "    flags integer not null,\n"                                    \
            "    primary key (snapshot_id, track_index, frame_index,\n"        \
            "                 layer_id))\n"                                    \
            "strict, without rowid;\n"                                         \
            "create table " PREFIX "snapshot_messages (\n"                     \
            "    snapshot_id integer not null,\n"                              \
            "    sequence_id integer not null,\n"                              \
            "    recorded_at real not null,\n"                                 \
            "    flags integer not null,\n"                                    \
            "    type integer not null,\n"                                     \
            "    context_id integer not null,\n"                               \
            "    body blob,\n"                                                 \
            "    primary key (snapshot_id, sequence_id))\n"                    \
            "strict, without rowid;\n",                                        \
        };                                                                     \
                                                                               \
        OUT_OK = true;                                                         \
        for (int i = 0; i < (int)DP_ARRAY_LENGTH(_migrations); ++i) {          \
            int _bind_result =                                                 \
                sqlite3_bind_int(_insert_migration_stmt, 1, i + 1);            \
            if (!is_ok(_bind_result)) {                                        \
                DP_error_set("Error %d binding migration insertion: %s",       \
                             _bind_result, db_error(DB));                      \
                OUT_SQL_RESULT = _bind_result;                                 \
                OUT_OK = false;                                                \
                break;                                                         \
            }                                                                  \
                                                                               \
            int _step_result = sqlite3_step(_insert_migration_stmt);           \
            if (_step_result != SQLITE_DONE) {                                 \
                DP_error_set("Error %d inserting migration: %s", _step_result, \
                             db_error(DB));                                    \
                OUT_SQL_RESULT = _step_result;                                 \
                OUT_OK = false;                                                \
                break;                                                         \
            }                                                                  \
                                                                               \
            int _changes = sqlite3_changes(DB);                                \
            int _reset_result = sqlite3_reset(_insert_migration_stmt);         \
            if (!is_ok(_reset_result)) {                                       \
                DP_error_set("Error %d resetting migration insertion: %s",     \
                             _reset_result, db_error(DB));                     \
                OUT_SQL_RESULT = _reset_result;                                \
                OUT_OK = false;                                                \
                break;                                                         \
            }                                                                  \
                                                                               \
            if (_changes > 0) {                                                \
                const char *sql = _migrations[i];                              \
                DP_debug("Executing migration %d", i + 1);                     \
                if (!exec_write_stmt(DB, sql, "executing migration",           \
                                     &OUT_SQL_RESULT)) {                       \
                    OUT_OK = false;                                            \
                    break;                                                     \
                }                                                              \
            }                                                                  \
        }                                                                      \
                                                                               \
        sqlite3_finalize(_insert_migration_stmt);                              \
    } while (0)

#define APPLY_MIGRATIONS_IN_TX(DB, PREFIX, OUT_OK, OUT_SQL_RESULT)     \
    do {                                                               \
        if (exec_write_stmt(DB, "begin exclusive",                     \
                            "opening migration transaction",           \
                            &OUT_SQL_RESULT)) {                        \
            bool _apply_ok;                                            \
            APPLY_MIGRATIONS(DB, PREFIX, _apply_ok, OUT_SQL_RESULT);   \
            if (_apply_ok                                              \
                && exec_write_stmt(DB, "commit",                       \
                                   "committing migration transaction", \
                                   &OUT_SQL_RESULT)) {                 \
                OUT_OK = true;                                         \
            }                                                          \
            else {                                                     \
                try_rollback(DB);                                      \
                OUT_OK = false;                                        \
            }                                                          \
        }                                                              \
        else {                                                         \
            OUT_OK = false;                                            \
        }                                                              \
    } while (0)

static const char *pps_sql(DP_ProjectPersistentStatement pps)
{
    static_assert(DP_PROJECT_MESSAGE_FLAG_OWN == 1,
                  "message own flag matches query");
    switch (pps) {
    case DP_PROJECT_STATEMENT_MESSAGE_RECORD:
        return "insert into messages (session_id, sequence_id, recorded_at, "
               "flags, type, context_id, body) values (?, ?, "
               "unixepoch('subsec'), ?, ?, ?, ?)";
    case DP_PROJECT_STATEMENT_SESSION_TIMES_SELECT:
        return "select sequence_id, recorded_at from messages "
               "where session_id = ? and sequence_id > ? and (flags & 1) <> 0";
    case DP_PROJECT_STATEMENT_COUNT:
        break;
    }
    return NULL;
}

static sqlite3_stmt *pps_prepare(DP_Project *prj,
                                 DP_ProjectPersistentStatement pps)
{
    DP_ASSERT(prj);
    DP_ASSERT(pps >= 0);
    DP_ASSERT(pps < DP_PROJECT_STATEMENT_COUNT);

    sqlite3_stmt *stmt = prj->stmts[pps];
    if (stmt) {
        return stmt;
    }

    stmt = prepare(prj->db, pps_sql(pps), SQLITE_PREPARE_PERSISTENT, NULL);
    if (stmt) {
        prj->stmts[pps] = stmt;
        return stmt;
    }
    else {
        return NULL;
    }
}

static DP_ProjectOpenResult project_open(const char *path, unsigned int flags,
                                         bool snapshot_only)
{
    bool read_only = flags & DP_PROJECT_OPEN_READ_ONLY;
    bool truncate = flags & DP_PROJECT_OPEN_TRUNCATE;
    if (read_only && truncate) {
        DP_error_set("Can't truncate a project when opening read-only");
        return make_open_error(DP_PROJECT_OPEN_ERROR_READ_ONLY, SQLITE_OK);
    }

    int sql_result = DP_sql_init();
    if (sql_result != SQLITE_OK) {
        return make_open_error(DP_PROJECT_OPEN_ERROR_INIT, sql_result);
    }

    int db_open_flags =
        SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_EXRESCODE;
    if (read_only) {
        db_open_flags |= SQLITE_OPEN_READONLY;
    }
    else if (flags & DP_PROJECT_OPEN_EXISTING) {
        db_open_flags |= SQLITE_OPEN_READWRITE;
    }
    else {
        db_open_flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    }

    sqlite3 *db;
    sql_result = sqlite3_open_v2(path, &db, db_open_flags, NULL);
    if (sql_result != SQLITE_OK) {
        DP_error_set("Error %d opening '%s': %s", sql_result, path,
                     db_error(db));
        try_close_db(db);
        return make_open_error(DP_PROJECT_OPEN_ERROR_OPEN, sql_result);
    }

    set_db_configs(db);

    if (!read_only) {
        if (sqlite3_db_readonly(db, "main") > 0) {
            DP_error_set("Error opening '%s': database is read-only", path);
            try_close_db(db);
            return make_open_error(DP_PROJECT_OPEN_ERROR_READ_ONLY, SQLITE_OK);
        }

        bool ok;
        PRAGMA_SETUP(db, "", ok, sql_result);
        if (!ok) {
            try_close_db(db);
            return make_open_error(sql_result == SQLITE_BUSY
                                       ? DP_PROJECT_OPEN_ERROR_LOCKED
                                       : DP_PROJECT_OPEN_ERROR_SETUP,
                                   sql_result);
        }
    }

    bool empty;
    if (truncate) {
        // This is how you clear out the database according to the sqlite
        // manual. Lack of error checking is intentional, we'll notice later.
        sqlite3_table_column_metadata(db, NULL, "dummy_table", "dummy_column",
                                      NULL, NULL, NULL, NULL, NULL);
        sqlite3_db_config(db, SQLITE_DBCONFIG_RESET_DATABASE, 1, 0);
        sqlite3_exec(db, "vacuum", NULL, NULL, NULL);
        sqlite3_db_config(db, SQLITE_DBCONFIG_RESET_DATABASE, 0, 0);
        empty = true;
    }
    else {
        bool ok;
        IS_EMPTY_DB(db, "", ok, empty, sql_result);
        if (!ok) {
            try_close_db(db);
            return make_open_error(read_only && sql_result == SQLITE_BUSY
                                       ? DP_PROJECT_OPEN_ERROR_LOCKED
                                       : DP_PROJECT_OPEN_ERROR_READ_EMPTY,
                                   sql_result);
        }

        if (empty && read_only) {
            DP_error_set("Read-only database is empty");
            try_close_db(db);
            return make_open_error(DP_PROJECT_OPEN_ERROR_READ_EMPTY,
                                   sql_result);
        }
    }

    if (empty) {
        bool ok;
        INIT_HEADER(db, "", snapshot_only, ok, sql_result);
        if (!ok) {
            try_close_db(db);
            return make_open_error(DP_PROJECT_OPEN_ERROR_HEADER_WRITE,
                                   sql_result);
        }
    }

    int header_result;
    CHECK_HEADER(db, "", snapshot_only, header_result, sql_result);
    if (header_result != 0) {
        try_close_db(db);
        return make_open_error(header_result, sql_result);
    }

    if (!read_only) {
        bool ok;
        APPLY_MIGRATIONS_IN_TX(db, "", ok, sql_result);
        if (!ok) {
            try_close_db(db);
            return make_open_error(DP_PROJECT_OPEN_ERROR_MIGRATION, sql_result);
        }
    }

    if (!read_only) {
        bool ok;
        PRAGMA_READY(db, "", ok, sql_result);
        if (!ok) {
            try_close_db(db);
            return make_open_error(DP_PROJECT_OPEN_ERROR_SETUP, sql_result);
        }
    }

    DP_Project *prj = DP_malloc(sizeof(*prj));
    prj->db = db;
    prj->session_id = 0LL;
    prj->sequence_id = 0LL;
    prj->snapshot.id = 0LL;
    prj->snapshot.sequence_id = 0LL;
    prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_CLOSED;
    prj->snapshot.attached = false;
    prj->snapshot.merge_sublayers = false;
    prj->snapshot.include_selections = false;
    prj->snapshot.has_sublayers = false;
    prj->snapshot.has_selections = false;
    prj->snapshot.mutex = NULL;
    DP_atomic_set(&prj->cancel, 0);
    for (int i = 0; i < DP_PROJECT_SNAPSHOT_STATEMENT_COUNT; ++i) {
        prj->snapshot.stmts[i] = NULL;
    }
    for (int i = 0; i < DP_PROJECT_STATEMENT_COUNT; ++i) {
        prj->stmts[i] = NULL;
    }
    return (DP_ProjectOpenResult){prj, 0, SQLITE_OK};
}

DP_ProjectOpenResult DP_project_open(const char *path, unsigned int flags)
{
    return project_open(path, flags, false);
}

bool DP_project_open_was_busy(const DP_ProjectOpenResult *open)
{
    DP_ASSERT(open);
    return open->error == DP_PROJECT_OPEN_ERROR_SETUP
        && (open->sql_result & 0xff) == SQLITE_BUSY;
}

static void project_close_session(DP_Project *prj)
{
    if (DP_project_session_close(prj, DP_PROJECT_SESSION_FLAG_PROJECT_CLOSED)
        < 0) {
        DP_warn("Close project: %s", DP_error());
    }
}

static bool project_close(DP_Project *prj, bool warn_only)
{
    if (prj->session_id != 0LL) {
        if (warn_only) {
            const char *error = DP_error();
            size_t error_length = strlen(error);
            char *stashed_error = DP_memdup(error, error_length);
            project_close_session(prj);
            DP_error_set_string(stashed_error, error_length);
            DP_free(stashed_error);
        }
        else {
            project_close_session(prj);
        }
    }

    DP_mutex_free(prj->snapshot.mutex);
    for (int i = 0; i < DP_PROJECT_STATEMENT_COUNT; ++i) {
        sqlite3_finalize(prj->stmts[i]);
    }
    int close_result = sqlite3_close(prj->db);
    DP_free(prj);

    if (is_ok(close_result)) {
        return true;
    }
    else {
        if (warn_only) {
            DP_warn("Error %d closing project", close_result);
        }
        else {
            DP_error_set("Error %d closing project", close_result);
        }
        return false;
    }
}

bool DP_project_close(DP_Project *prj)
{
    if (!prj) {
        DP_error_set("Project is null");
        return false;
    }
    return project_close(prj, false);
}


void DP_project_cancel(DP_Project *prj)
{
    if (prj) {
        DP_atomic_set(&prj->cancel, 1);
        sqlite3_interrupt(prj->db);
    }
}

static void cancel_reset(DP_Project *prj)
{
    DP_atomic_set(&prj->cancel, 0);
}

static bool cancel_check_reset(DP_Project *prj)
{
    int cancel = DP_atomic_xch(&prj->cancel, 0);
    return cancel != 0;
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
        DP_error_set("Integrity check returned no result");
        *out_status = DP_PROJECT_VERIFY_ERROR;
        return false;
    }
}

DP_ProjectVerifyStatus DP_project_verify(DP_Project *prj, unsigned int flags)
{
    DP_ASSERT(prj);
    cancel_reset(prj);

    const char *sql = (flags & DP_PROJECT_VERIFY_FULL)
                        ? "pragma integrity_check(1)"
                        : "pragma quick_check(1)";
    DP_ProjectVerifyStatus status = DP_PROJECT_VERIFY_ERROR;
    exec_text_stmt(prj->db, sql, NULL, handle_verify, &status, NULL);

    if (cancel_check_reset(prj)) {
        return DP_PROJECT_VERIFY_CANCELLED;
    }
    else {
        return status;
    }
}


static sqlite3_stmt *ps_prepare_ephemeral(DP_Project *prj, const char *sql)
{
    return prepare(prj->db, sql, 0, NULL);
}

static bool ps_bind_null(DP_Project *prj, sqlite3_stmt *stmt, int param)
{
    int bind_result = sqlite3_bind_null(stmt, param);
    if (is_ok(bind_result)) {
        return true;
    }
    else {
        DP_error_set("Error %d binding null parameter %d to %s: %s",
                     bind_result, param, sqlite3_sql(stmt), prj_db_error(prj));
        return false;
    }
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

static bool ps_bind_blob_or_null(DP_Project *prj, sqlite3_stmt *stmt, int param,
                                 const void *value, size_t length)
{
    if (value && length > 0) {
        return ps_bind_blob(prj, stmt, param, value, length);
    }
    else {
        return ps_bind_null(prj, stmt, param);
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
        *out_error = false;
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

int DP_project_session_open(DP_Project *prj, int source_type,
                            const char *source_param, const char *protocol,
                            unsigned int flags)
{
    if (!prj) {
        DP_error_set("No project given");
        return DP_PROJECT_SESSION_OPEN_ERROR_MISUSE;
    }

    if (prj->session_id != 0LL) {
        DP_error_set("Session %lld already open", prj->session_id);
        return DP_PROJECT_SESSION_OPEN_ERROR_ALREADY_OPEN;
    }

    if (!source_param) {
        DP_warn("No source param given, punting to ''");
        source_param = "";
    }

    if (!protocol) {
        DP_warn("No protocol given, punting to '%s'", DP_PROTOCOL_VERSION);
        protocol = DP_PROTOCOL_VERSION;
    }

    DP_debug("Opening session source %d %s, protocol %s", source_type,
             source_param, protocol);
    prj->sequence_id = 0LL;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "insert into sessions (source_type, source_param, protocol, "
             "flags, opened_at) values (?, ?, ?, ?, unixepoch('subsec'))");
    if (!stmt) {
        return DP_PROJECT_SESSION_OPEN_ERROR_PREPARE;
    }

    bool write_ok = ps_bind_int(prj, stmt, 1, source_type)
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
    if (!prj) {
        DP_error_set("No project given");
        return DP_PROJECT_SESSION_CLOSE_ERROR_MISUSE;
    }

    long long session_id = prj->session_id;
    if (session_id == 0LL) {
        DP_error_set("No open session");
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


static unsigned char *get_serialize_buffer(void *user, DP_UNUSED size_t length)
{
    DP_ASSERT(length <= DP_MESSAGE_MAX_PAYLOAD_LENGTH);
    DP_Project *prj = user;
    return prj->serialize_buffer;
}

static int record_message(DP_Project *prj, long long session_id,
                          unsigned int flags, int type, unsigned int context_id,
                          const void *body_or_null, size_t length)
{
    sqlite3_stmt *stmt = pps_prepare(prj, DP_PROJECT_STATEMENT_MESSAGE_RECORD);
    if (!stmt) {
        return DP_PROJECT_MESSAGE_RECORD_ERROR_PREPARE;
    }

    bool write_ok = ps_bind_int64(prj, stmt, 1, session_id)
                 && ps_bind_int64(prj, stmt, 2, ++prj->sequence_id)
                 && ps_bind_int64(prj, stmt, 3, DP_uint_to_llong(flags))
                 && ps_bind_int(prj, stmt, 4, type)
                 && ps_bind_int64(prj, stmt, 5, context_id)
                 && ps_bind_blob_or_null(prj, stmt, 6, body_or_null, length)
                 && ps_exec_write(prj, stmt, NULL);
    ps_clear_bindings(prj, stmt);
    if (write_ok) {
        return 0;
    }
    else {
        return DP_PROJECT_MESSAGE_RECORD_ERROR_WRITE;
    }
}

int DP_project_message_record(DP_Project *prj, DP_Message *msg,
                              unsigned int flags)
{
    if (!prj) {
        DP_error_set("No project given");
        return DP_PROJECT_MESSAGE_RECORD_ERROR_MISUSE;
    }

    if (!msg) {
        DP_error_set("No message given");
        return DP_PROJECT_MESSAGE_RECORD_ERROR_MISUSE;
    }

    long long session_id = prj->session_id;
    if (session_id == 0LL) {
        DP_error_set("No open session");
        return DP_PROJECT_MESSAGE_RECORD_ERROR_NOT_OPEN;
    }

    size_t length;
    if (!DP_message_serialize_body(msg, get_serialize_buffer, prj, &length)) {
        return DP_PROJECT_MESSAGE_RECORD_ERROR_SERIALIZE;
    }

    return record_message(prj, session_id, flags, (int)DP_message_type(msg),
                          DP_message_context_id(msg), prj->serialize_buffer,
                          length);
}

int DP_project_message_internal_record(DP_Project *prj, int type,
                                       unsigned int context_id,
                                       const void *body_or_null, size_t size,
                                       unsigned int flags)
{
    if (!prj) {
        DP_error_set("No project given");
        return DP_PROJECT_MESSAGE_RECORD_ERROR_MISUSE;
    }

    if (type >= 0) {
        DP_error_set("Invalid internal message type %d", type);
        return DP_PROJECT_MESSAGE_RECORD_ERROR_MISUSE;
    }

    long long session_id = prj->session_id;
    if (session_id == 0LL) {
        DP_error_set("No open session");
        return DP_PROJECT_MESSAGE_RECORD_ERROR_NOT_OPEN;
    }

    return record_message(prj, session_id, flags, type, context_id,
                          body_or_null, size);
}


DP_ProjectSessionTimes DP_project_session_times_null(void)
{
    return (DP_ProjectSessionTimes){0LL, 0LL, 0.0};
}

static bool has_minute_passed(double last_recorded_at, double recorded_at)
{
    if (last_recorded_at <= recorded_at) {
        return (recorded_at - last_recorded_at) >= 60.0;
    }
    else {
        DP_warn("Recorded time went backwards, from %f to %f", last_recorded_at,
                recorded_at);
        return true; // Assume a minute passed to get back on track,
    }
}

int DP_project_session_times_update(DP_Project *prj,
                                    DP_ProjectSessionTimes *pst)
{
    if (!prj) {
        DP_error_set("No project given");
        return DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_MISUSE;
    }

    if (!pst) {
        DP_error_set("No project session times given");
        return DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_MISUSE;
    }

    long long session_id = prj->session_id;
    if (session_id == 0LL) {
        DP_error_set("No open session");
        return DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_NOT_OPEN;
    }

    sqlite3_stmt *stmt =
        pps_prepare(prj, DP_PROJECT_STATEMENT_SESSION_TIMES_SELECT);
    if (!stmt) {
        return DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_PREPARE;
    }

    long long last_sequence_id = pst->last_sequence_id;
    bool bind_ok = ps_bind_int64(prj, stmt, 1, session_id)
                && ps_bind_int64(prj, stmt, 2, last_sequence_id);
    if (!bind_ok) {
        return DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_PREPARE;
    }

    long long own_work_minutes_to_add = 0LL;
    double last_recorded_at = pst->last_recorded_at;

    bool error;
    while (ps_exec_step(prj, stmt, &error)) {
        last_sequence_id = sqlite3_column_int64(stmt, 0);
        double recorded_at = sqlite3_column_double(stmt, 1);
        if (has_minute_passed(last_recorded_at, recorded_at)) {
            last_recorded_at = recorded_at;
            ++own_work_minutes_to_add;
        }
    }

    if (error) {
        return DP_PROJECT_SESSION_TIMES_UPDATE_ERROR_QUERY;
    }

    pst->own_work_minutes += own_work_minutes_to_add;
    pst->last_sequence_id = last_sequence_id;
    pst->last_recorded_at = last_recorded_at;
    return 0;
}


static const char *snapshot_sql(DP_ProjectSnapshotPersistentStatement psps,
                                bool attached)
{
    switch (psps) {
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_METADATA:
        return DP_PROJECT_SQL_MAIN_SAV("insert into ",
                                       "snapshot_metadata (snapshot_id, "
                                       "metadata_id, value) values (?, ?, ?)",
                                       attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_LAYER:
        return DP_PROJECT_SQL_MAIN_SAV(
            "insert into ",
            "snapshot_layers (snapshot_id, layer_index, parent_index, "
            "layer_id, title, blend_mode, opacity, sketch_opacity, "
            "sketch_tint, flags, fill) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
            "?)",
            attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_SUBLAYER:
        return DP_PROJECT_SQL_MAIN_SAV(
            "insert into ",
            "snapshot_sublayers (snapshot_id, sublayer_index, layer_index, "
            "sublayer_id, blend_mode, opacity, flags, fill) values (?, ?, ?, "
            "?, ?, ?, 0, ?)",
            attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TILE:
        return DP_PROJECT_SQL_MAIN_SAV(
            "insert into ",
            "snapshot_tiles (snapshot_id, layer_index, tile_index, context_id, "
            "repeat, pixels) values (?, ?, ?, ?, ?, ?)",
            attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_SUBLAYER_TILE:
        return DP_PROJECT_SQL_MAIN_SAV(
            "insert into ",
            "snapshot_sublayer_tiles (snapshot_id, sublayer_index, tile_index, "
            "repeat, pixels) values (?, ?, ?, ?, ?)",
            attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_SELECTION_TILE:
        return DP_PROJECT_SQL_MAIN_SAV(
            "insert into ",
            "snapshot_selection_tiles (snapshot_id, selection_id, context_id, "
            "tile_index, mask) values (?, ?, ?, ?, ?)",
            attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_ANNOTATION:
        return DP_PROJECT_SQL_MAIN_SAV(
            "insert into ",
            "snapshot_annotations (snapshot_id, annotation_index, "
            "annotation_id, content, x, y, width, height, background_color, "
            "valign, flags) values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TRACK:
        return DP_PROJECT_SQL_MAIN_SAV(
            "insert into ",
            "snapshot_tracks (snapshot_id, track_index, track_id, title, "
            "flags) values (?, ?, ?, ?, ?)",
            attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME:
        return DP_PROJECT_SQL_MAIN_SAV(
            "insert into ",
            "snapshot_key_frames (snapshot_id, track_index, frame_index, "
            "title, layer_id) values (?, ?, ?, ?, ?)",
            attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME_LAYER:
        return DP_PROJECT_SQL_MAIN_SAV(
            "insert into ",
            "snapshot_key_frame_layers (snapshot_id, track_index, frame_index, "
            "layer_id, flags) values (?, ?, ?, ?, ?)",
            attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_MESSAGE:
        return DP_PROJECT_SQL_MAIN_SAV(
            "insert into ",
            "snapshot_messages (snapshot_id, sequence_id, recorded_at, flags, "
            "type, context_id, body) values (?, ?, unixepoch('subsec'), ?, ?, "
            "?, ?)",
            attached);
    case DP_PROJECT_SNAPSHOT_STATEMENT_COUNT:
        break;
    }
    return NULL;
}

static sqlite3_stmt *
snapshot_prepare(DP_Project *prj, DP_ProjectSnapshotPersistentStatement psps)
{
    DP_ASSERT(prj);
    DP_ASSERT(psps >= 0);
    DP_ASSERT(psps < DP_PROJECT_SNAPSHOT_STATEMENT_COUNT);

    sqlite3_stmt *stmt = prj->snapshot.stmts[psps];
    if (stmt) {
        return stmt;
    }

    stmt = prepare(prj->db, snapshot_sql(psps, prj->snapshot.attached),
                   SQLITE_PREPARE_PERSISTENT, NULL);
    if (!stmt) {
        return NULL;
    }

    if (!ps_bind_int64(prj, stmt, 1, prj->snapshot.id)) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    prj->snapshot.stmts[psps] = stmt;
    return stmt;
}

static long long project_snapshot_open(DP_Project *prj, unsigned int flags,
                                       long long session_id, bool attached)
{
    if (prj->snapshot.id != 0LL) {
        DP_error_set("Snapshot %lld already open", prj->snapshot.id);
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_ALREADY_OPEN;
    }

    DP_Mutex *mutex = DP_mutex_new();
    if (!mutex) {
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_MUTEX;
    }

    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, DP_PROJECT_SQL_MAIN_SAV("insert into ",
                                     "snapshots (session_id, flags, taken_at) "
                                     "values (?, ?, unixepoch('subsec'))",
                                     attached));
    if (!stmt) {
        DP_mutex_free(mutex);
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_PREPARE;
    }

    long long snapshot_id;
    bool write_ok = ps_bind_int64(prj, stmt, 1, session_id)
                 && ps_bind_int64(prj, stmt, 2, flags)
                 && ps_exec_write(prj, stmt, &snapshot_id);
    sqlite3_finalize(stmt);
    if (!write_ok) {
        DP_mutex_free(mutex);
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_WRITE;
    }

    DP_ASSERT(snapshot_id > 0);
    prj->snapshot.id = snapshot_id;
    prj->snapshot.sequence_id = 0LL;
    prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_READY;
    bool is_autosave = flags & DP_PROJECT_SNAPSHOT_FLAG_AUTOSAVE;
    prj->snapshot.attached = attached;
    prj->snapshot.merge_sublayers = !is_autosave;
    prj->snapshot.include_selections = is_autosave;
    prj->snapshot.has_sublayers = false;
    prj->snapshot.has_selections = false;
    prj->snapshot.mutex = mutex;
    return snapshot_id;
}

long long DP_project_snapshot_open(DP_Project *prj, unsigned int flags)
{
    DP_ASSERT(prj);
    DP_ASSERT(!(flags & DP_PROJECT_SNAPSHOT_FLAG_COMPLETE));

    if (prj->session_id == 0LL) {
        DP_error_set("No session open");
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_NO_SESSION;
    }

    return project_snapshot_open(prj, flags, prj->session_id, false);
}

static int record_snapshot_message(DP_Project *prj, unsigned int flags,
                                   int type, unsigned int context_id,
                                   const void *body_or_null, size_t length)
{
    sqlite3_stmt *stmt =
        snapshot_prepare(prj, DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_MESSAGE);
    if (!stmt) {
        return DP_PROJECT_MESSAGE_RECORD_ERROR_PREPARE;
    }

    bool write_ok = ps_bind_int64(prj, stmt, 2, ++prj->snapshot.sequence_id)
                 && ps_bind_int64(prj, stmt, 3, DP_uint_to_llong(flags))
                 && ps_bind_int(prj, stmt, 4, type)
                 && ps_bind_int64(prj, stmt, 5, context_id)
                 && ps_bind_blob_or_null(prj, stmt, 6, body_or_null, length)
                 && ps_exec_write(prj, stmt, NULL);

    // Don't want to clear all bindings because parameter 1 always stays bound
    // to the snapshot id. We only clear the pointer binding to avoid staleness.
    if (body_or_null) {
        ps_bind_null(prj, stmt, 6);
    }

    if (write_ok) {
        return 0;
    }
    else {
        return DP_PROJECT_MESSAGE_RECORD_ERROR_WRITE;
    }
}

int DP_project_snapshot_message_record(DP_Project *prj, long long snapshot_id,
                                       DP_Message *msg, unsigned int flags)
{
    if (!prj) {
        DP_error_set("No project given");
        return DP_PROJECT_MESSAGE_RECORD_ERROR_MISUSE;
    }

    if (!msg) {
        DP_error_set("No message given");
        return DP_PROJECT_MESSAGE_RECORD_ERROR_MISUSE;
    }

    if (prj->snapshot.id == 0LL) {
        DP_error_set("Snapshot %lld is not open (none is)", snapshot_id);
        return DP_PROJECT_MESSAGE_RECORD_ERROR_NOT_OPEN;
    }

    if (prj->snapshot.attached) {
        DP_error_set("Snapshot %lld is not open, (attached snapshot %lld is)",
                     snapshot_id, prj->snapshot.id);
        return DP_PROJECT_MESSAGE_RECORD_ERROR_NOT_OPEN;
    }

    if (prj->snapshot.id != snapshot_id) {
        DP_error_set("Snapshot %lld is not open, (%lld is)", snapshot_id,
                     prj->snapshot.id);
        return DP_PROJECT_MESSAGE_RECORD_ERROR_NOT_OPEN;
    }

    size_t length;
    if (!DP_message_serialize_body(msg, get_serialize_buffer, prj, &length)) {
        return DP_PROJECT_MESSAGE_RECORD_ERROR_SERIALIZE;
    }

    return record_snapshot_message(prj, flags, (int)DP_message_type(msg),
                                   DP_message_context_id(msg),
                                   prj->serialize_buffer, (size_t)length);
}

static void snapshot_close(DP_Project *prj)
{
    prj->snapshot.id = 0LL;
    prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_CLOSED;
    for (int i = 0; i < DP_PROJECT_SNAPSHOT_STATEMENT_COUNT; ++i) {
        sqlite3_finalize(prj->snapshot.stmts[i]);
        prj->snapshot.stmts[i] = NULL;
    }
    DP_mutex_free(prj->snapshot.mutex);
    prj->snapshot.mutex = NULL;
}

static int snapshot_finish(DP_Project *prj)
{
    long long snapshot_id = prj->snapshot.id;
    DP_ProjectSnapshotState snapshot_state = prj->snapshot.state;
    snapshot_close(prj);

    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj,
        DP_PROJECT_SQL_MAIN_SAV(
            "update ", "snapshots set flags = flags | ? where snapshot_id = ?",
            prj->snapshot.attached));
    if (!stmt) {
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_PREPARE;
    }

    unsigned int flags_to_set =
        DP_PROJECT_SNAPSHOT_FLAG_COMPLETE
        | DP_flag_uint(prj->snapshot.sequence_id != 0LL,
                       DP_PROJECT_SNAPSHOT_FLAG_HAS_MESSAGES)
        | DP_flag_uint(prj->snapshot.has_sublayers,
                       DP_PROJECT_SNAPSHOT_FLAG_HAS_SUBLAYERS)
        | DP_flag_uint(prj->snapshot.has_selections,
                       DP_PROJECT_SNAPSHOT_FLAG_HAS_SELECTIONS)
        | DP_flag_uint(snapshot_state == DP_PROJECT_SNAPSHOT_STATE_NULL,
                       DP_PROJECT_SNAPSHOT_FLAG_NULL_CANVAS);

    bool write_ok = ps_bind_int64(prj, stmt, 1, flags_to_set)
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

int DP_project_snapshot_finish(DP_Project *prj, long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0LL);

    if (prj->snapshot.id == 0LL) {
        DP_error_set("Snapshot %lld is not open (none is)", snapshot_id);
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_NOT_OPEN;
    }

    if (prj->snapshot.attached) {
        DP_error_set("Snapshot %lld is not open, (attached snapshot %lld is)",
                     snapshot_id, prj->snapshot.id);
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_NOT_OPEN;
    }

    if (prj->snapshot.id != snapshot_id) {
        DP_error_set("Snapshot %lld is not open, (%lld is)", snapshot_id,
                     prj->snapshot.id);
        return DP_PROJECT_SNAPSHOT_FINISH_ERROR_NOT_OPEN;
    }

    return snapshot_finish(prj);
}

static int project_snapshot_discard_relations(DP_Project *prj,
                                              long long snapshot_id,
                                              bool attached)
{
    const char *sqls[] = {
        DP_PROJECT_SQL_MAIN_SAV("delete from ",
                                "snapshot_messages where snapshot_id = ?",
                                attached),
        DP_PROJECT_SQL_MAIN_SAV(
            "delete from ", "snapshot_key_frame_layers where snapshot_id = ?",
            attached),
        DP_PROJECT_SQL_MAIN_SAV("delete from ",
                                "snapshot_key_frames where snapshot_id = ?",
                                attached),
        DP_PROJECT_SQL_MAIN_SAV(
            "delete from ", "snapshot_tracks where snapshot_id = ?", attached),
        DP_PROJECT_SQL_MAIN_SAV("delete from ",
                                "snapshot_annotations where snapshot_id = ?",
                                attached),
        DP_PROJECT_SQL_MAIN_SAV(
            "delete from ", "snapshot_selection_tiles where snapshot_id = ?",
            attached),
        DP_PROJECT_SQL_MAIN_SAV("delete from ",
                                "snapshot_sublayer_tiles where snapshot_id = ?",
                                attached),
        DP_PROJECT_SQL_MAIN_SAV(
            "delete from ", "snapshot_tiles where snapshot_id = ?", attached),
        DP_PROJECT_SQL_MAIN_SAV("delete from ",
                                "snapshot_sublayers where snapshot_id = ?",
                                attached),
        DP_PROJECT_SQL_MAIN_SAV(
            "delete from ", "snapshot_layers where snapshot_id = ?", attached),
        DP_PROJECT_SQL_MAIN_SAV("delete from ",
                                "snapshot_metadata where snapshot_id = ?",
                                attached),
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

static int snapshot_discard(DP_Project *prj, long long snapshot_id,
                            bool attached)
{
    if (prj->snapshot.id == snapshot_id && prj->snapshot.attached == attached) {
        snapshot_close(prj);
    }

    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, DP_PROJECT_SQL_MAIN_SAV(
                 "delete from ", "snapshots where snapshot_id = ?", attached));
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
    int error = project_snapshot_discard_relations(prj, snapshot_id, attached);
    if (error != 0) {
        return error;
    }

    if (changes == 0LL) {
        return DP_PROJECT_SNAPSHOT_DISCARD_NOT_FOUND;
    }

    return 0;
}

int DP_project_snapshot_discard(DP_Project *prj, long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0LL);
    return snapshot_discard(prj, snapshot_id, false);
}

static int snapshot_discard_all_except(DP_Project *prj, long long snapshot_id,
                                       bool attached)
{
    DP_ASSERT(prj);

    static_assert(DP_PROJECT_SNAPSHOT_FLAG_PERSISTENT == 2,
                  "persistent snapshot flag matches query");
    sqlite3_stmt *stmt;
    if (snapshot_id > 0LL) { // Only check for valid snapshot ids.
        stmt = ps_prepare_ephemeral(
            prj, DP_PROJECT_SQL_MAIN_SAV(
                     "select snapshot_id from ",
                     "snapshots where snapshot_id <> ? and (flags & 2) = 0",
                     attached));
        if (!stmt) {
            return DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_PREPARE;
        }

        bool bind_ok = ps_bind_int64(prj, stmt, 1, snapshot_id);
        if (!bind_ok) {
            sqlite3_finalize(stmt);
            return DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_PREPARE;
        }
    }
    else {
        stmt = ps_prepare_ephemeral(
            prj, DP_PROJECT_SQL_MAIN_SAV("select snapshot_id from ",
                                         "snapshots where (flags & 2) = 0",
                                         attached));
        if (!stmt) {
            return DP_PROJECT_SNAPSHOT_DISCARD_ALL_EXCEPT_ERROR_PREPARE;
        }
    }

    bool read_error = false;
    int write_errors = 0;
    int discard_count = 0;
    while (ps_exec_step(prj, stmt, &read_error)) {
        long long snapshot_id_to_discard = sqlite3_column_int64(stmt, 0);
        int discard_result =
            snapshot_discard(prj, snapshot_id_to_discard, attached);
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

int DP_project_snapshot_discard_all_except(DP_Project *prj,
                                           long long snapshot_id)
{
    DP_ASSERT(prj);
    return snapshot_discard_all_except(prj, snapshot_id, false);
}


static bool snapshot_write_metadata_int(DP_Project *prj,
                                        DP_ProjectSnapshotMetadata id,
                                        long long value)
{
    sqlite3_stmt *stmt =
        snapshot_prepare(prj, DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_METADATA);
    return stmt && ps_bind_int(prj, stmt, 2, (int)id)
        && ps_bind_int64(prj, stmt, 3, value) && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_write_metadata_int_unless_default(
    DP_Project *prj, DP_ProjectSnapshotMetadata id, long long value,
    long long default_value)
{
    return value == default_value
        || snapshot_write_metadata_int(prj, id, value);
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
               DP_DOCUMENT_METADATA_FRAME_COUNT_DEFAULT)
        && snapshot_write_metadata_int_unless_default(
               prj, DP_PROJECT_SNAPSHOT_METADATA_FRAMERATE_FRACTION,
               DP_document_metadata_framerate_fraction(dm),
               DP_DOCUMENT_METADATA_FRAMERATE_FRACTION_DEFAULT)
        && snapshot_write_metadata_int_unless_default(
               prj, DP_PROJECT_SNAPSHOT_METADATA_FRAME_RANGE_FIRST,
               DP_document_metadata_frame_range_first(dm),
               DP_DOCUMENT_METADATA_FRAME_RANGE_FIRST_DEFAULT)
        && snapshot_write_metadata_int_unless_default(
               prj, DP_PROJECT_SNAPSHOT_METADATA_FRAME_RANGE_LAST,
               DP_document_metadata_frame_range_last(dm),
               DP_DOCUMENT_METADATA_FRAME_RANGE_LAST_DEFAULT);
}

static bool snapshot_handle_canvas(DP_Project *prj,
                                   const DP_ResetEntryCanvas *rec)
{
    return snapshot_write_metadata_int(prj, DP_PROJECT_SNAPSHOT_METADATA_WIDTH,
                                       rec->width)
        && snapshot_write_metadata_int(prj, DP_PROJECT_SNAPSHOT_METADATA_HEIGHT,
                                       rec->height)
        && snapshot_write_document_metadata(prj, rec->dm);
}

static bool snapshot_handle_background(DP_Project *prj,
                                       const DP_ResetEntryBackground *reb)
{
    sqlite3_stmt *stmt =
        snapshot_prepare(prj, DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_METADATA);
    return stmt
        && ps_bind_int(prj, stmt, 2,
                       (int)DP_PROJECT_SNAPSHOT_METADATA_BACKGROUND_TILE)
        && ps_bind_blob(prj, stmt, 3, reb->data, reb->size)
        && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_insert_layer(DP_Project *prj,
                                  const DP_ResetEntryLayer *rel)
{
    sqlite3_stmt *stmt =
        snapshot_prepare(prj, DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_LAYER);
    if (!stmt) {
        return false;
    }

    DP_LayerProps *lp = rel->lp;
    DP_LayerPropsList *child_lpl = DP_layer_props_children_noinc(lp);
    size_t title_length;
    const char *title = DP_layer_props_title(lp, &title_length);
    unsigned int flags =
        DP_flag_uint(child_lpl, DP_PROJECT_SNAPSHOT_LAYER_FLAG_GROUP)
        | DP_flag_uint(DP_layer_props_hidden(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_HIDDEN)
        | DP_flag_uint(DP_layer_props_censored_remote(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED_REMOTE)
        | DP_flag_uint(child_lpl && !DP_layer_props_isolated(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_PASS_THROUGH)
        | DP_flag_uint(DP_layer_props_clip(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_CLIP)
        | DP_flag_uint(DP_layer_props_censored_local(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED_LOCAL);
    return ps_bind_int(prj, stmt, 2, rel->layer_index)
        && ps_bind_int(prj, stmt, 3, rel->parent_index)
        && ps_bind_int(prj, stmt, 4, rel->layer_id)
        && ps_bind_text_length(prj, stmt, 5, title, title_length)
        && ps_bind_int(prj, stmt, 6, DP_layer_props_blend_mode(lp))
        && ps_bind_int(prj, stmt, 7, DP_layer_props_opacity(lp))
        && ps_bind_int(prj, stmt, 8, DP_layer_props_sketch_opacity(lp))
        && ps_bind_int64(prj, stmt, 9, DP_layer_props_sketch_tint(lp))
        && ps_bind_int64(prj, stmt, 10, flags)
        && ps_bind_int64(prj, stmt, 11, child_lpl ? 0 : rel->fill)
        && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_insert_sublayer(DP_Project *prj,
                                     const DP_ResetEntryLayer *rel)
{
    DP_ASSERT(prj->snapshot.merge_sublayers);
    prj->snapshot.has_sublayers = true;
    sqlite3_stmt *stmt =
        snapshot_prepare(prj, DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_SUBLAYER);
    DP_LayerProps *lp = rel->lp;
    return stmt && ps_bind_int(prj, stmt, 2, rel->sublayer_index)
        && ps_bind_int(prj, stmt, 3, rel->layer_index)
        && ps_bind_int(prj, stmt, 4, rel->sublayer_id)
        && ps_bind_int(prj, stmt, 5, DP_layer_props_blend_mode(lp))
        && ps_bind_int(prj, stmt, 6, DP_layer_props_opacity(lp))
        && ps_bind_int64(prj, stmt, 7, rel->fill)
        && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_handle_layer(DP_Project *prj,
                                  const DP_ResetEntryLayer *rel)
{
    if (rel->sublayer_id == 0) {
        return snapshot_insert_layer(prj, rel);
    }
    else {
        return snapshot_insert_sublayer(prj, rel);
    }
}

static bool snapshot_insert_layer_tile(DP_Project *prj,
                                       const DP_ResetEntryTile *ret)
{
    DP_ASSERT(ret->tile_run > 0);
    DP_ASSERT(ret->size != 0);
    DP_ASSERT(ret->data);
    sqlite3_stmt *stmt =
        snapshot_prepare(prj, DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TILE);
    return stmt && ps_bind_int(prj, stmt, 2, ret->layer_index)
        && ps_bind_int(prj, stmt, 3, ret->tile_index)
        && ps_bind_int64(prj, stmt, 4, ret->context_id)
        && ps_bind_int(prj, stmt, 5, ret->tile_run - 1)
        && ps_bind_blob(prj, stmt, 6, ret->data, ret->size)
        && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_insert_sublayer_tile(DP_Project *prj,
                                          const DP_ResetEntryTile *ret)
{
    DP_ASSERT(prj->snapshot.merge_sublayers);
    DP_ASSERT(ret->tile_run > 0);
    DP_ASSERT(ret->size != 0);
    DP_ASSERT(ret->data);
    sqlite3_stmt *stmt = snapshot_prepare(
        prj, DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_SUBLAYER_TILE);
    return stmt && ps_bind_int(prj, stmt, 2, ret->sublayer_index)
        && ps_bind_int(prj, stmt, 3, ret->tile_index)
        && ps_bind_int(prj, stmt, 4, ret->tile_run - 1)
        && ps_bind_blob(prj, stmt, 5, ret->data, ret->size)
        && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_handle_tile(DP_Project *prj, const DP_ResetEntryTile *ret)
{
    if (ret->sublayer_id == 0) {
        return snapshot_insert_layer_tile(prj, ret);
    }
    else {
        return snapshot_insert_sublayer_tile(prj, ret);
    }
}

static bool
snapshot_handle_selection_tile(DP_Project *prj,
                               const DP_ResetEntrySelectionTile *rest)
{
    DP_ASSERT(rest->size != 0);
    DP_ASSERT(rest->data);

    prj->snapshot.has_selections = true;

    // Opaque selection tiles are stored as NULLs.
    size_t size;
    const void *data;
    if (DP_reset_entry_selection_tile_opaque(rest)) {
        size = 0;
        data = NULL;
    }
    else {
        size = rest->size;
        data = rest->data;
    }

    sqlite3_stmt *stmt =
        prj->snapshot
            .stmts[DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_SELECTION_TILE];
    return stmt && ps_bind_int(prj, stmt, 2, rest->selection_id)
        && ps_bind_int64(prj, stmt, 3, rest->context_id)
        && ps_bind_int(prj, stmt, 4, rest->tile_index)
        && ps_bind_blob_or_null(prj, stmt, 5, data, size)
        && ps_exec_write(prj, stmt, NULL);
}

static bool snapshot_handle_annotation(DP_Project *prj,
                                       const DP_ResetEntryAnnotation *rea)
{
    sqlite3_stmt *stmt =
        snapshot_prepare(prj, DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_ANNOTATION);
    if (!stmt) {
        return false;
    }

    DP_Annotation *a = rea->a;
    size_t text_length;
    const char *text = DP_annotation_text(a, &text_length);
    unsigned int flags =
        DP_flag_uint(DP_annotation_protect(a),
                     DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_PROTECTED)
        | DP_flag_uint(DP_annotation_alias(a),
                       DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_ALIAS)
        | DP_flag_uint(DP_annotation_rasterize(a),
                       DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_RASTERIZE);
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
        snapshot_prepare(prj, DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TRACK);
    if (!stmt) {
        return false;
    }

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

        bool bind_ok = stmt && ps_bind_int(prj, stmt, 2, ref->track_index)
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
        snapshot_prepare(prj, DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_KEY_FRAME);
    if (!stmt) {
        return stmt;
    }

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

static bool snapshot_handle_thumb(DP_Project *prj,
                                  const DP_ResetEntryThumb *ret)
{
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, DP_PROJECT_SQL_MAIN_SAV(
                 "update ", "snapshots set thumbnail = ? where snapshot_id = ?",
                 prj->snapshot.attached));
    if (!stmt) {
        return false;
    }

    bool write_ok = ps_bind_blob(prj, stmt, 1, ret->data, ret->size)
                 && ps_bind_int64(prj, stmt, 2, prj->snapshot.id)
                 && ps_exec_write(prj, stmt, NULL);
    sqlite3_finalize(stmt);
    return write_ok;
}

static bool snapshot_handle_entry(DP_Project *prj, const DP_ResetEntry *entry)
{
    switch (entry->type) {
    case DP_RESET_ENTRY_CANVAS:
        return snapshot_handle_canvas(prj, &entry->canvas);
    case DP_RESET_ENTRY_BACKGROUND:
        return snapshot_handle_background(prj, &entry->background);
    case DP_RESET_ENTRY_LAYER:
        return snapshot_handle_layer(prj, &entry->layer);
    case DP_RESET_ENTRY_TILE:
        return snapshot_handle_tile(prj, &entry->tile);
    case DP_RESET_ENTRY_SELECTION_TILE:
        return snapshot_handle_selection_tile(prj, &entry->selection_tile);
    case DP_RESET_ENTRY_ANNOTATION:
        return snapshot_handle_annotation(prj, &entry->annotation);
    case DP_RESET_ENTRY_TRACK:
        return snapshot_handle_track(prj, &entry->track);
    case DP_RESET_ENTRY_FRAME:
        return snapshot_handle_frame(prj, &entry->frame);
    case DP_RESET_ENTRY_THUMB:
        return snapshot_handle_thumb(prj, &entry->thumb);
    }
    DP_warn("Unhandled snapshot entry type %d", (int)entry->type);
    return true;
}

static void snapshot_handle_entry_callback(void *user,
                                           const DP_ResetEntry *entry)
{
    DP_Project *prj = user;
    if (prj->snapshot.state == DP_PROJECT_SNAPSHOT_STATE_OK) {
        DP_Mutex *mutex = prj->snapshot.mutex;
        DP_MUTEX_MUST_LOCK(mutex);
        bool handle_ok = snapshot_handle_entry(prj, entry);
        DP_MUTEX_MUST_UNLOCK(mutex);
        if (!handle_ok) {
            prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_ERROR;
        }
    }
}

static int snapshot_canvas(DP_Project *prj, DP_CanvasState *cs,
                           bool (*thumb_write_fn)(void *, DP_Image *,
                                                  DP_Output *),
                           void *thumb_write_user, void (*pre_join_fn)(void *),
                           void *pre_join_user)
{
    if (prj->snapshot.state != DP_PROJECT_SNAPSHOT_STATE_READY) {
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_READY;
    }

    if (!snapshot_write_metadata_int_unless_default(
            prj, DP_PROJECT_SNAPSHOT_METADATA_SEQUENCE_ID,
            prj->sequence_id + 1LL, 1LL)) {
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_WRITE;
    }

    if (DP_canvas_state_null(cs)) {
        DP_debug("Snapshotting null canvas");
        prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_NULL;
    }
    else {
        prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_OK;
        DP_ResetImageOptions options = {
            true,
            prj->snapshot.merge_sublayers,
            prj->snapshot.include_selections,
            DP_RESET_IMAGE_COMPRESSION_ZSTD8LE,
            DP_PROJECT_THUMBNAIL_SIZE,
            DP_PROJECT_THUMBNAIL_SIZE,
            {thumb_write_fn, thumb_write_user},
            {pre_join_fn, pre_join_user},
        };
        DP_reset_image_build_with(cs, &options, snapshot_handle_entry_callback,
                                  prj);
        if (prj->snapshot.state != DP_PROJECT_SNAPSHOT_STATE_OK) {
            return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_WRITE;
        }
    }

    return 0;
}

int DP_project_snapshot_canvas(DP_Project *prj, long long snapshot_id,
                               DP_CanvasState *cs,
                               bool (*thumb_write_fn)(void *, DP_Image *,
                                                      DP_Output *),
                               void *thumb_write_user)
{
    DP_PERF_BEGIN(fn, "snapshot_canvas");

    if (!prj) {
        DP_error_set("No project given");
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_MISUSE;
    }

    if (!cs) {
        DP_error_set("No canvas given");
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_MISUSE;
    }

    if (prj->snapshot.id == 0LL) {
        DP_error_set("Snapshot %lld is not open (none is)", snapshot_id);
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_OPEN;
    }

    if (prj->snapshot.attached) {
        DP_error_set("Snapshot %lld is not open, (attached snapshot %lld is)",
                     snapshot_id, prj->snapshot.id);
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_OPEN;
    }

    if (prj->snapshot.id != snapshot_id) {
        DP_error_set("Snapshot %lld is not open, (%lld is)", snapshot_id,
                     prj->snapshot.id);
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_OPEN;
    }

    int result =
        snapshot_canvas(prj, cs, thumb_write_fn, thumb_write_user, NULL, NULL);
    DP_PERF_END(fn);
    return result;
}


static void project_save_try_detach(DP_Project *prj)
{
    sqlite3 *db = prj->db;
    char *errmsg;
    int exec_result = sqlite3_exec(db, "detach sav", NULL, NULL, &errmsg);
    if (!is_ok(exec_result)) {
        DP_warn("Error %d detaching save database: %s", exec_result,
                fallback_db_error(db, errmsg));
    }
}

static int project_save_attach(DP_Project *prj, const char *path)
{
    sqlite3_stmt *stmt = ps_prepare_ephemeral(prj, "attach ? as sav");
    if (!stmt) {
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    if (!ps_bind_text(prj, stmt, 1, path)) {
        sqlite3_finalize(stmt);
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    bool attach_ok = ps_exec_write(prj, stmt, NULL);
    sqlite3_finalize(stmt);
    if (!attach_ok) {
        return DP_PROJECT_SAVE_ERROR_OPEN;
    }

    return 0;
}

static int project_save_append_session_id(DP_Project *prj,
                                          long long *out_save_session_id)
{
    // A save can be appended if there is already a session with the same source
    // param, which is a UUID in practice. That session must also be the last
    // one in that file, otherwise opening it would not load what was saved. It
    // usually shouldn't happen that you end up in a situation where it is not
    // the last, since the user would have to append from different canvases.
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select ss.session_id sid, ms.session_id mid\n"
             "from sav.sessions ss\n"
             "left join main.sessions ms\n"
             "    on ms.session_id = ?\n"
             "    and ss.source_param = ms.source_param\n"
             "order by ss.session_id desc\n"
             "limit 1");
    if (!stmt) {
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    if (!ps_bind_int64(prj, stmt, 1, prj->session_id)) {
        sqlite3_finalize(stmt);
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    bool error;
    if (ps_exec_step(prj, stmt, &error)) {
        long long save_session_id = sqlite3_column_int64(stmt, 0);
        long long main_session_id = sqlite3_column_int64(stmt, 1);
        if (save_session_id > 0LL && main_session_id > 0LL) {
            *out_save_session_id = save_session_id;
        }
        else {
            *out_save_session_id = 0LL;
        }
    }
    else {
        *out_save_session_id = 0LL;
    }
    sqlite3_finalize(stmt);

    if (error) {
        return DP_PROJECT_SAVE_ERROR_QUERY;
    }

    return 0;
}

static int project_save_new_session(DP_Project *prj,
                                    long long *out_save_session_id)
{
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "insert into sav.sessions (source_type, "
             "source_param, protocol, flags, opened_at)\n"
             "select source_type, source_param, protocol, flags, opened_at\n"
             "from main.sessions where session_id = ?");
    if (!stmt) {
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    if (!ps_bind_int64(prj, stmt, 1, prj->session_id)) {
        sqlite3_finalize(stmt);
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    bool write_ok = ps_exec_write(prj, stmt, out_save_session_id);
    sqlite3_finalize(stmt);
    if (!write_ok) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    return 0;
}

static int project_save_initial_snapshot_id(DP_Project *prj,
                                            long long *out_initial_snapshot_id)
{
    static_assert(DP_PROJECT_SNAPSHOT_FLAG_COMPLETE == 1,
                  "snapshot complete flag matches query");
    static_assert(DP_PROJECT_SNAPSHOT_METADATA_SEQUENCE_ID == 11,
                  "sequence id snapshot metadata matches query");
    sqlite3_stmt *stmt =
        ps_prepare_ephemeral(prj, "select s.snapshot_id\n"
                                  "from main.snapshots s\n"
                                  "left join main.snapshot_metadata m\n"
                                  "    on s.snapshot_id = m.snapshot_id\n"
                                  "    and m.metadata_id = 11\n" // SEQUENCE_ID
                                  "where s.session_id = ?\n"
                                  "and (m.value is null or m.value = 1)\n"
                                  "order by s.snapshot_id\n"
                                  "limit 1");
    if (!stmt) {
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    if (!ps_bind_int64(prj, stmt, 1, prj->session_id)) {
        sqlite3_finalize(stmt);
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    bool error;
    if (ps_exec_step(prj, stmt, &error)) {
        *out_initial_snapshot_id = sqlite3_column_int64(stmt, 0);
    }
    else {
        *out_initial_snapshot_id = 0LL;
    }
    sqlite3_finalize(stmt);

    if (error) {
        return DP_PROJECT_SAVE_ERROR_QUERY;
    }

    return 0;
}

static int project_save_copy_snapshot_header(DP_Project *prj,
                                             long long save_session_id,
                                             long long source_snapshot_id,
                                             long long *out_save_snapshot_id)
{
    static_assert(DP_PROJECT_SNAPSHOT_FLAG_AUTOSAVE == 8,
                  "project snapshot autosave flag matches query");
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "insert into sav.snapshots (session_id, flags, taken_at)\n"
             "select ?, flags & ~(8), taken_at\n"
             "from main.snapshots\n"
             "where snapshot_id = ?");
    if (!stmt) {
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    bool bind_ok = ps_bind_int64(prj, stmt, 1, save_session_id)
                && ps_bind_int64(prj, stmt, 2, source_snapshot_id);
    if (!bind_ok) {
        sqlite3_finalize(stmt);
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    bool write_ok = ps_exec_write(prj, stmt, out_save_snapshot_id);
    sqlite3_finalize(stmt);
    if (!write_ok) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    return 0;
}

static int project_save_copy_initial_snapshot(DP_Project *prj,
                                              long long save_session_id)
{
    long long source_snapshot_id;
    int initial_snapshot_id_result =
        project_save_initial_snapshot_id(prj, &source_snapshot_id);
    if (initial_snapshot_id_result != 0) {
        return initial_snapshot_id_result;
    }

    if (source_snapshot_id > 0LL) {
        long long save_snapshot_id;
        int copy_snapshot_header_result = project_save_copy_snapshot_header(
            prj, save_session_id, source_snapshot_id, &save_snapshot_id);
        if (copy_snapshot_header_result != 0) {
            return copy_snapshot_header_result;
        }

        const char *sqls[] = {
            "insert into sav.snapshot_metadata (\n"
            "    snapshot_id, metadata_id, value)\n"
            "select\n"
            "    ?, metadata_id, value\n"
            "from main.snapshot_metadata\n"
            "where snapshot_id = ?",

            "insert into sav.snapshot_layers (snapshot_id,\n"
            "    layer_index, parent_index, layer_id, title, blend_mode,\n"
            "    opacity, sketch_opacity, sketch_tint, flags, fill)\n"
            "select ?,\n"
            "    layer_index, parent_index, layer_id, title, blend_mode,\n"
            "    opacity, sketch_opacity, sketch_tint, flags, fill\n"
            "from main.snapshot_layers\n"
            "where snapshot_id = ?",

            "insert into sav.snapshot_sublayers (snapshot_id,\n"
            "    sublayer_index, layer_index, sublayer_id, blend_mode,\n"
            "    opacity, flags, fill)\n"
            "select ?,\n"
            "    sublayer_index, layer_index, sublayer_id, blend_mode,\n"
            "    opacity, flags, fill\n"
            "from main.snapshot_sublayers\n"
            "where snapshot_id = ?",

            "insert into sav.snapshot_tiles (snapshot_id,\n"
            "    layer_index, tile_index, context_id, repeat, pixels)\n"
            "select ?,\n"
            "    layer_index, tile_index, context_id, repeat, pixels\n"
            "from main.snapshot_tiles\n"
            "where snapshot_id = ?",

            "insert into sav.snapshot_sublayer_tiles (snapshot_id,\n"
            "    sublayer_index, tile_index, repeat, pixels)\n"
            "select ?,\n"
            "    sublayer_index, tile_index, repeat, pixels\n"
            "from main.snapshot_sublayer_tiles\n"
            "where snapshot_id = ?",

            "insert into sav.snapshot_selection_tiles (snapshot_id,\n"
            "    selection_id, context_id, tile_index, mask)\n"
            "select ?,\n"
            "    selection_id, context_id, tile_index, mask\n"
            "from main.snapshot_selection_tiles\n"
            "where snapshot_id = ?",

            "insert into sav.snapshot_annotations (snapshot_id,\n"
            "    annotation_index, annotation_id, content, x, y, width,\n"
            "    height, background_color, valign, flags)\n"
            "select ?,\n"
            "    annotation_index, annotation_id, content, x, y, width,\n"
            "    height, background_color, valign, flags\n"
            "from main.snapshot_annotations\n"
            "where snapshot_id = ?",

            "insert into sav.snapshot_tracks (snapshot_id,\n"
            "    track_index, track_id, title, flags)\n"
            "select ?,\n"
            "    track_index, track_id, title, flags\n"
            "from main.snapshot_tracks\n"
            "where snapshot_id = ?",

            "insert into sav.snapshot_key_frames (snapshot_id,\n"
            "    track_index, frame_index, title, layer_id)\n"
            "select ?,\n"
            "    track_index, frame_index, title, layer_id\n"
            "from main.snapshot_key_frames\n"
            "where snapshot_id = ?",

            "insert into sav.snapshot_key_frame_layers (snapshot_id,\n"
            "    track_index, frame_index, layer_id, flags)\n"
            "select ?,\n"
            "    track_index, frame_index, title, layer_id\n"
            "from main.snapshot_key_frames\n"
            "where snapshot_id = ?",

            "insert into sav.snapshot_messages (snapshot_id,\n"
            "    sequence_id, recorded_at, flags, type, context_id, body)\n"
            "select ?,\n"
            "    sequence_id, recorded_at, flags, type, context_id, body\n"
            "from main.snapshot_messages\n"
            "where snapshot_id = ?",
        };

        for (int i = 0; i < (int)DP_ARRAY_LENGTH(sqls); ++i) {
            sqlite3_stmt *stmt = ps_prepare_ephemeral(prj, sqls[i]);
            if (!stmt) {
                return DP_PROJECT_SAVE_ERROR_PREPARE;
            }

            bool bind_ok = ps_bind_int64(prj, stmt, 1, save_snapshot_id)
                        && ps_bind_int64(prj, stmt, 2, source_snapshot_id);
            if (!bind_ok) {
                sqlite3_finalize(stmt);
                return DP_PROJECT_SAVE_ERROR_PREPARE;
            }

            bool write_ok = ps_exec_write(prj, stmt, NULL);
            sqlite3_finalize(stmt);
            if (!write_ok) {
                return DP_PROJECT_SAVE_ERROR_WRITE;
            }

            DP_debug("Copied %lld row(s)", sqlite3_changes64(prj->db));
        }
    }
    else {
        // There should always be an initial snapshot. We can keep going without
        // one though and get a sensible enough save, so we'll deal with it.
        DP_warn("No initial snapshot to copy found");
    }

    return 0;
}

static int project_save_append_session(DP_Project *prj,
                                       long long save_session_id,
                                       long long *out_last_sequence_id)
{
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select sequence_id from sav.messages\n where session_id = ?\n"
             "order by sequence_id desc limit 1");
    if (!stmt) {
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    if (!ps_bind_int64(prj, stmt, 1, save_session_id)) {
        sqlite3_finalize(stmt);
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    bool error;
    if (ps_exec_step(prj, stmt, &error)) {
        *out_last_sequence_id = sqlite3_column_int64(stmt, 0);
    }
    else if (!error) {
        *out_last_sequence_id = 0LL;
    }
    sqlite3_finalize(stmt);

    if (error) {
        return DP_PROJECT_SAVE_ERROR_QUERY;
    }

    return 0;
}

static int project_save_copy_messages(DP_Project *prj,
                                      long long save_session_id,
                                      long long last_sequence_id)
{
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj,
        "insert into sav.messages (session_id, sequence_id, recorded_at,\n"
        "flags, type, context_id, body)\n"
        "select ?, sequence_id, recorded_at, flags, type, context_id, body\n"
        "from main.messages where session_id = ? and sequence_id > ?\n"
        "order by sequence_id");
    if (!stmt) {
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    bool bind_ok = ps_bind_int64(prj, stmt, 1, save_session_id)
                && ps_bind_int64(prj, stmt, 2, prj->session_id)
                && ps_bind_int64(prj, stmt, 3, last_sequence_id);
    if (!bind_ok) {
        sqlite3_finalize(stmt);
    }

    bool write_ok = ps_exec_write(prj, stmt, NULL);
    sqlite3_finalize(stmt);
    if (!write_ok) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    DP_debug("Copied %lld message(s)", sqlite3_changes64(prj->db));
    return 0;
}

struct DP_ProjectSaveCopyMessageParams {
    DP_Project *prj;
    long long save_session_id;
    long long last_sequence_id;
};

static void project_save_copy_messages_pre_join(void *user)
{
    const struct DP_ProjectSaveCopyMessageParams *params = user;
    // Analogous to snapshot_handle_entry_callback
    DP_Project *prj = params->prj;
    if (prj->snapshot.state == DP_PROJECT_SNAPSHOT_STATE_OK) {
        DP_Mutex *mutex = prj->snapshot.mutex;
        DP_MUTEX_MUST_LOCK(mutex);
        int result = project_save_copy_messages(
            params->prj, params->save_session_id, params->last_sequence_id);
        DP_MUTEX_MUST_UNLOCK(mutex);
        if (result != 0) {
            prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_ERROR;
            DP_warn("Error %d copying messages: %s", result, DP_error());
        }
    }
}

static int
project_save_snapshot(DP_Project *prj, DP_CanvasState *cs,
                      long long save_session_id,
                      bool (*thumb_write_fn)(void *, DP_Image *, DP_Output *),
                      void *thumb_write_user, void (*pre_join_fn)(void *),
                      void *pre_join_user, long long *out_save_snapshot_id)
{
    long long save_snapshot_id = project_snapshot_open(
        prj, DP_PROJECT_SNAPSHOT_FLAG_CANVAS, save_session_id, true);
    if (save_snapshot_id < 1LL) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    int snapshot_canvas_result = snapshot_canvas(
        prj, cs, thumb_write_fn, thumb_write_user, pre_join_fn, pre_join_user);
    if (snapshot_canvas_result != 0) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    if (snapshot_finish(prj) != 0) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    *out_save_snapshot_id = save_snapshot_id;
    return 0;
}

static int project_save_close_session(DP_Project *prj,
                                      long long save_session_id,
                                      long long save_snapshot_id)
{
    static_assert(DP_PROJECT_SESSION_FLAG_PROJECT_CLOSED == 1,
                  "session closed flag matches query");
    sqlite3_stmt *stmt =
        ps_prepare_ephemeral(prj, "update sav.sessions\n"
                                  "set flags = flags | 1,\n"
                                  "    closed_at = unixepoch('subsec'),\n"
                                  "    thumbnail = (\n"
                                  "        select thumbnail\n"
                                  "        from sav.snapshots\n"
                                  "        where snapshot_id = ?)\n"
                                  "where session_id = ?");
    if (!stmt) {
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    bool bind_ok = ps_bind_int64(prj, stmt, 1, save_snapshot_id)
                && ps_bind_int64(prj, stmt, 2, save_session_id);
    if (!bind_ok) {
        sqlite3_finalize(stmt);
        return DP_PROJECT_SAVE_ERROR_PREPARE;
    }

    bool write_ok = ps_exec_write(prj, stmt, NULL);
    sqlite3_finalize(stmt);
    if (!write_ok) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    DP_debug("Updated %lld session(s) on close", sqlite3_changes64(prj->db));
    return 0;
}

static int project_save_to_attached(DP_Project *prj, DP_CanvasState *cs,
                                    unsigned int flags,
                                    bool (*thumb_write_fn)(void *, DP_Image *,
                                                           DP_Output *),
                                    void *thumb_write_user)
{
    sqlite3 *db = prj->db;
    if (sqlite3_db_readonly(prj->db, "sav")) {
        DP_error_set("Database is read-only");
        return DP_PROJECT_SAVE_ERROR_READ_ONLY;
    }

    bool ok;
    int sql_result;
    PRAGMA_SETUP(db, "sav.", ok, sql_result);
    if (!ok) {
        if (sql_result == SQLITE_BUSY) {
            return DP_PROJECT_SAVE_ERROR_LOCKED;
        }
        else {
            return DP_PROJECT_SAVE_ERROR_SETUP;
        }
    }

    bool empty;
    IS_EMPTY_DB(db, "sav.", ok, empty, sql_result);
    if (!ok) {
        return DP_PROJECT_SAVE_ERROR_READ_EMPTY;
    }

    if (empty) {
        INIT_HEADER(db, "sav.", false, ok, sql_result);
        if (!ok) {
            return DP_PROJECT_SAVE_ERROR_HEADER_WRITE;
        }
    }

    int header_result;
    CHECK_HEADER(db, "sav.", false, header_result, sql_result);
    if (header_result != 0) {
        return DP_PROJECT_SAVE_ERROR_HEADER_MISMATCH;
    }

    APPLY_MIGRATIONS_IN_TX(db, "sav.", ok, sql_result);
    if (!ok) {
        return DP_PROJECT_SAVE_ERROR_MIGRATION;
    }

    PRAGMA_READY(db, "sav.", ok, sql_result);
    if (!ok) {
        return DP_PROJECT_SAVE_ERROR_SETUP;
    }

    // Figure out whether we can continue the current session.
    // TODO: Somehow handle the split-brain situation if a user saves to a file,
    // then saves to the same file from a different window, then saves from the
    // first window again. This currently creates two separate sessions, which
    // isn't great (but the situation is pretty pathological to begin with.)
    long long append_session_id;
    int append_session_id_result =
        project_save_append_session_id(prj, &append_session_id);
    if (append_session_id_result != 0) {
        return append_session_id_result;
    }

    // Get rid of any non-persistent snapshots, we'll be writing a new one.
    int discard_result = snapshot_discard_all_except(prj, 0LL, true);
    if (discard_result < 0) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }
    DP_debug("Discarded %d snapshot(s)", discard_result);

    long long save_session_id;
    long long last_sequence_id;
    if (append_session_id > 0LL) {
        DP_debug("Appending to existing session %lld", append_session_id);
        save_session_id = append_session_id;
        int append_session_result = project_save_append_session(
            prj, save_session_id, &last_sequence_id);
        if (append_session_result != 0) {
            return append_session_result;
        }
    }
    else {
        DP_debug("Saving to a new session");
        last_sequence_id = 0LL;
        int new_session_result =
            project_save_new_session(prj, &save_session_id);
        if (new_session_result != 0) {
            return new_session_result;
        }

        int copy_initial_snapshot_result =
            project_save_copy_initial_snapshot(prj, save_session_id);
        if (copy_initial_snapshot_result != 0) {
            return copy_initial_snapshot_result;
        }
    }

    DP_debug("Saving to session %lld starting from message %lld",
             save_session_id, last_sequence_id);

    // Savin a snapshot has some downtime on the main thread when waiting for
    // the worker threads to finish compressing the tiles. During that, we can
    // copy the messages over for an extra bit of saved time (not crazy amounts,
    // we still have to take a lock to access the database. But it's something.)
    bool no_messages = flags & DP_PROJECT_SAVE_FLAG_NO_MESSAGES;
    long long save_snapshot_id;
    int snapshot_result = project_save_snapshot(
        prj, cs, save_session_id, thumb_write_fn, thumb_write_user,
        no_messages ? NULL : project_save_copy_messages_pre_join,
        no_messages
            ? NULL
            : (struct DP_ProjectSaveCopyMessageParams[1]){{prj, save_session_id,
                                                           last_sequence_id}},
        &save_snapshot_id);

    if (snapshot_result != 0) {
        return snapshot_result;
    }

    return project_save_close_session(prj, save_session_id, save_snapshot_id);
}

int DP_project_save(DP_Project *prj, DP_CanvasState *cs, const char *path,
                    unsigned int flags,
                    bool (*thumb_write_fn)(void *, DP_Image *, DP_Output *),
                    void *thumb_write_user)
{
    if (!prj) {
        DP_error_set("No project given");
        return DP_PROJECT_SAVE_ERROR_MISUSE;
    }

    if (!cs) {
        DP_error_set("No canvas state given");
        return DP_PROJECT_SAVE_ERROR_MISUSE;
    }

    if (!path) {
        DP_error_set("No path given");
        return DP_PROJECT_SAVE_ERROR_MISUSE;
    }

    if (prj->session_id == 0LL) {
        DP_error_set("No open session");
        return DP_PROJECT_SAVE_ERROR_NO_SESSION;
    }

    int attach_result = project_save_attach(prj, path);
    if (attach_result != 0) {
        return attach_result;
    }

    int save_result = project_save_to_attached(prj, cs, flags, thumb_write_fn,
                                               thumb_write_user);
    project_save_try_detach(prj);
    return save_result;
}


static int project_save_state_to(DP_Project *prj, DP_CanvasState *cs,
                                 bool (*thumb_write_fn)(void *, DP_Image *,
                                                        DP_Output *),
                                 void *thumb_write_user)
{
    // Get rid of any non-persistent snapshots, we'll be writing a new one.
    int snapshot_discard_result = snapshot_discard_all_except(prj, 0LL, false);
    if (snapshot_discard_result < 0) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    long long snapshot_id =
        project_snapshot_open(prj, DP_PROJECT_SNAPSHOT_FLAG_CANVAS, 0LL, false);
    if (snapshot_id <= 0LL) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    int snapshot_result = DP_project_snapshot_canvas(
        prj, snapshot_id, cs, thumb_write_fn, thumb_write_user);
    if (snapshot_result != 0) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    int finish_result = DP_project_snapshot_finish(prj, snapshot_id);
    if (finish_result != 0) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }

    return 0;
}

int DP_project_save_state(DP_CanvasState *cs, const char *path,
                          bool (*thumb_write_fn)(void *, DP_Image *,
                                                 DP_Output *),
                          void *thumb_write_user)
{
    if (!cs) {
        DP_error_set("No canvas state given");
        return DP_PROJECT_SAVE_ERROR_MISUSE;
    }

    if (!path) {
        DP_error_set("No path given");
        return DP_PROJECT_SAVE_ERROR_MISUSE;
    }

    DP_ProjectOpenResult open_result = DP_project_open(path, 0);
    DP_Project *prj = open_result.project;
    if (!prj) {
        return DP_PROJECT_SAVE_ERROR_OPEN;
    }

    int save_result =
        project_save_state_to(prj, cs, thumb_write_fn, thumb_write_user);

    // Don't clobber a save error with a close error, only warn and return the
    // original error in that case.
    bool save_ok = save_result == 0;
    bool close_ok = project_close(prj, !save_ok);
    if (save_ok && !close_ok) {
        return DP_PROJECT_SAVE_ERROR_WRITE;
    }
    else {
        return save_result;
    }
}


typedef struct DP_ProjectCanvasFromSnapshotLayer {
    DP_TransientLayerProps *tlp;
    union {
        DP_TransientLayerContent *tlc;
        DP_TransientLayerGroup *tlg;
    };
    int used;
} DP_ProjectCanvasFromSnapshotLayer;

typedef struct DP_ProjectCanvasFromSnapshotSublayer {
    DP_TransientLayerProps *tlp;
    DP_TransientLayerContent *tlc;
} DP_ProjectCanvasFromSnapshotSublayer;

typedef struct DP_ProjectCanvasFromSnapshotContext
    DP_ProjectCanvasFromSnapshotContext;

typedef struct DP_ProjectCanvasFromSnapshotTileJobHeader {
    DP_ProjectCanvasFromSnapshotContext *c;
    union {
        struct {
            union {
                unsigned int context_id;
                int sublayer_index;
            };
            int layer_index;
        };
        DP_TransientSelection *tsel;
    };
    int tile_index;
    int repeat;
    size_t size;
} DP_ProjectCanvasFromSnapshotTileJobHeader;

typedef struct DP_ProjectCanvasFromSnapshotTileJob {
    DP_ProjectCanvasFromSnapshotTileJobHeader header;
    unsigned char data[];
} DP_ProjectCanvasFromSnapshotTileJob;

typedef struct DP_ProjectCanvasFromSnapshotTileJobParams {
    DP_ProjectCanvasFromSnapshotTileJobHeader header;
    const unsigned char *data;
} DP_ProjectCanvasFromSnapshotTileJobParams;

struct DP_ProjectCanvasFromSnapshotContext {
    DP_Project *prj;
    DP_Worker *worker;
    DP_TransientCanvasState *tcs;
    DP_SplitTile8 **split_buffers;
    ZSTD_DCtx **zstd_contexts;
    DP_ProjectCanvasFromSnapshotLayer *layers;
    DP_ProjectCanvasFromSnapshotSublayer *sublayers;
    long long snapshot_id;
    unsigned int snapshot_flags;
    int layer_count;
    int root_layer_count;
    int annotation_count;
    int track_count;
};

#define TILE_JOB_REPEAT_SENTINEL_BACKGROUND (-1)
#define TILE_JOB_REPEAT_SENTINEL_SELECTION  (-2)

static void cfs_tile_job_layer_tile(DP_ProjectCanvasFromSnapshotTileJob *job,
                                    int thread_index)
{
    DP_ProjectCanvasFromSnapshotContext *c = job->header.c;
    DP_Tile *t = DP_tile_new_from_split_delta_zstd8le_with(
        &c->zstd_contexts[thread_index], c->split_buffers[thread_index],
        job->header.context_id, job->data, job->header.size);

    int layer_index = job->header.layer_index;
    DP_ASSERT(layer_index >= 0);
    int tile_index = job->header.tile_index;
    if (t) {
        int repeat = job->header.repeat;
        for (int i = 0; i <= repeat; ++i) {
            DP_transient_layer_content_tile_set_noinc(
                c->layers[layer_index].tlc, t, tile_index + i);
        }
        if (repeat != 0) {
            DP_tile_incref_by(t, repeat);
        }
    }
    else {
        DP_warn("Tile index %d of layer %d failed to decompress: %s",
                tile_index, layer_index, DP_error());
    }
}

static void cfs_tile_job_sublayer_tile(DP_ProjectCanvasFromSnapshotTileJob *job,
                                       int thread_index)
{
    DP_ProjectCanvasFromSnapshotContext *c = job->header.c;
    int sublayer_index = job->header.sublayer_index;
    DP_ASSERT(sublayer_index >= 0);

    int sublayer_id =
        DP_transient_layer_props_id(c->sublayers[sublayer_index].tlp);
    if (sublayer_id < 0 || sublayer_id > 255) {
        sublayer_id = 0;
    }

    DP_Tile *t = DP_tile_new_from_split_delta_zstd8le_with(
        &c->zstd_contexts[thread_index], c->split_buffers[thread_index],
        DP_int_to_uint(sublayer_id), job->data, job->header.size);

    int tile_index = job->header.tile_index;
    if (t) {
        int repeat = job->header.repeat;
        for (int i = 0; i <= repeat; ++i) {
            DP_transient_layer_content_tile_set_noinc(
                c->sublayers[sublayer_index].tlc, t, tile_index + i);
        }
        if (repeat != 0) {
            DP_tile_incref_by(t, repeat);
        }
    }
    else {
        DP_warn("Tile index %d of sublayer %d failed to decompress: %s",
                tile_index, sublayer_index, DP_error());
    }
}

static void
cfs_tile_job_background_tile(DP_ProjectCanvasFromSnapshotTileJob *job,
                             int thread_index)
{
    DP_ProjectCanvasFromSnapshotContext *c = job->header.c;
    DP_Tile *t = DP_tile_new_from_split_delta_zstd8le_with(
        &c->zstd_contexts[thread_index], c->split_buffers[thread_index], 0,
        job->data, job->header.size);

    if (t) {
        DP_transient_canvas_state_background_tile_set_noinc(c->tcs, t,
                                                            DP_tile_opaque(t));
    }
    else {
        DP_warn("Background tile failed to decompress: %s", DP_error());
    }
}

static void
cfs_tile_job_selection_tile(DP_ProjectCanvasFromSnapshotTileJob *job,
                            int thread_index)
{
    DP_ProjectCanvasFromSnapshotContext *c = job->header.c;
    DP_TransientSelection *tsel = job->header.tsel;
    DP_Tile *t = DP_tile_new_mask_from_delta_zstd8le_nonopaque_with(
        &c->zstd_contexts[thread_index],
        (unsigned char *)c->split_buffers[thread_index],
        DP_transient_selection_context_id(tsel), job->data, job->header.size);

    if (t) {
        DP_TransientLayerContent *tlc =
            (DP_TransientLayerContent *)DP_transient_selection_content_noinc(
                tsel);
        DP_transient_layer_content_tile_set_noinc(tlc, t,
                                                  job->header.tile_index);
    }
    else {
        DP_warn("Selection tile failed to decompress: %s", DP_error());
    }
}

static void cfs_tile_job(void *element, int thread_index)
{
    DP_ProjectCanvasFromSnapshotTileJob *job = element;

    int repeat = job->header.repeat;
    if (repeat >= 0) {
        if (job->header.layer_index == -1) {
            cfs_tile_job_sublayer_tile(job, thread_index);
        }
        else {
            cfs_tile_job_layer_tile(job, thread_index);
        }
    }
    else if (repeat == TILE_JOB_REPEAT_SENTINEL_BACKGROUND) {
        cfs_tile_job_background_tile(job, thread_index);
    }
    else if (repeat == TILE_JOB_REPEAT_SENTINEL_SELECTION) {
        cfs_tile_job_selection_tile(job, thread_index);
    }
    else {
        DP_UNREACHABLE();
    }
}

static bool cfs_read_header(DP_ProjectCanvasFromSnapshotContext *c)
{
    DP_Project *prj = c->prj;
    // Don't read anything that is behind the DP_PROJECT_SNAPSHOT_FLAG_HAS_*
    // flags, earlier dpcs files don't have those tables.
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

static void cfs_insert_tile_job(void *user, void *element)
{
    DP_ProjectCanvasFromSnapshotTileJobParams *params = user;
    DP_ProjectCanvasFromSnapshotTileJob *job = element;
    job->header = params->header;
    memcpy(job->data, params->data, params->header.size);
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
    int width = 0;
    int height = 0;
    while (ps_exec_step(prj, stmt, &error)) {
        int metadata_id = sqlite3_column_int(stmt, 0);
        switch (metadata_id) {
        case DP_PROJECT_SNAPSHOT_METADATA_WIDTH:
            width = sqlite3_column_int(stmt, 1);
            break;
        case DP_PROJECT_SNAPSHOT_METADATA_HEIGHT:
            height = sqlite3_column_int(stmt, 1);
            break;
        case DP_PROJECT_SNAPSHOT_METADATA_BACKGROUND_TILE: {
            const unsigned char *data = sqlite3_column_blob(stmt, 1);
            size_t size = DP_int_to_size(sqlite3_column_bytes(stmt, 1));
            if (data && size >= 4) {
                DP_ProjectCanvasFromSnapshotTileJobParams params = {
                    {
                        c,
                        {{{0u}, -1}},
                        -1,
                        TILE_JOB_REPEAT_SENTINEL_BACKGROUND,
                        size,
                    },
                    data,
                };
                DP_worker_push_with(c->worker, cfs_insert_tile_job, &params);
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
        case DP_PROJECT_SNAPSHOT_METADATA_FRAMERATE_FRACTION:
            DP_transient_document_metadata_framerate_fraction_set(
                tdm, sqlite3_column_int(stmt, 1));
            break;
        case DP_PROJECT_SNAPSHOT_METADATA_FRAME_RANGE_FIRST:
            DP_transient_document_metadata_frame_range_first_set(
                tdm, sqlite3_column_int(stmt, 1));
            break;
        case DP_PROJECT_SNAPSHOT_METADATA_FRAME_RANGE_LAST:
            DP_transient_document_metadata_frame_range_last_set(
                tdm, sqlite3_column_int(stmt, 1));
            break;
        case DP_PROJECT_SNAPSHOT_METADATA_SEQUENCE_ID:
            break;
        default:
            DP_warn("Unknown snapshot metadatum %d", metadata_id);
            break;
        }
    }
    sqlite3_finalize(stmt);
    if (error) {
        return false;
    }

    if ((width == 0 && height == 0)
        || DP_canvas_state_dimensions_in_bounds(width, height)) {
        DP_transient_canvas_state_width_set(tcs, width);
        DP_transient_canvas_state_height_set(tcs, height);
        return true;
    }
    else {
        DP_error_set("Canvas dimensions %dx%d out of bounds", width, height);
        return false;
    }
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
    if (!DP_layer_id_normal(layer_id)) {
        DP_warn("Layer %d has invalid id %d", layer_index, layer_id);
        return false;
    }

    const char *title = (const char *)sqlite3_column_text(stmt, 3);
    size_t title_length = DP_int_to_size(sqlite3_column_bytes(stmt, 3));
    if (title_length > DP_MSG_LAYER_TREE_CREATE_TITLE_MAX_LEN) {
        DP_warn("Layer title length %zu out of bounds", title_length);
        title_length = DP_MSG_LAYER_TREE_CREATE_TITLE_MAX_LEN;
    }

    int blend_mode = sqlite3_column_int(stmt, 4);
    if (!DP_blend_mode_valid_for_layer(blend_mode)) {
        DP_warn("Layer %d has invalid blend mode %d", layer_index, blend_mode);
        blend_mode = DP_BLEND_MODE_NORMAL;
    }

    int opacity = sqlite3_column_int(stmt, 5);
    if (opacity < 0 || opacity > DP_BIT15) {
        DP_warn("Layer %d opacity %d out of bounds", layer_index, opacity);
        opacity = DP_BIT15;
    }

    int sketch_opacity = sqlite3_column_int(stmt, 6);
    if (sketch_opacity < 0 || sketch_opacity > DP_BIT15) {
        DP_warn("Layer %d sketch opacity %d out of bounds", layer_index,
                sketch_opacity);
        sketch_opacity = 0;
    }

    long long sketch_tint = sqlite3_column_int64(stmt, 7);
    if (sketch_tint < 0LL || sketch_tint > UINT32_MAX) {
        DP_warn("Layer %d sketch tint %lld out of bounds", layer_index,
                sketch_tint);
        sketch_tint = 0LL;
    }

    unsigned int flags = DP_int_to_uint(sqlite3_column_int(stmt, 8));
    bool group = flags & DP_PROJECT_SNAPSHOT_LAYER_FLAG_GROUP;

    long long fill = sqlite3_column_int64(stmt, 9);
    if (fill < 0LL || fill > UINT32_MAX) {
        DP_warn("Layer %d fill %lld out of bounds", layer_index, fill);
        fill = 0;
    }
    else if (fill != 0 && group) {
        DP_warn("Layer %d has fill, but is a group", layer_index);
    }

    int child_count = sqlite3_column_int(stmt, 10);
    if (child_count > 0) {
        if (!group) {
            DP_warn("Layer %d has %d child(ren), but is not a group",
                    layer_index, child_count);
            child_count = 0;
        }
        else {
            int max_child_count = c->layer_count - layer_index - 1;
            if (child_count > max_child_count) {
                DP_warn(
                    "Layer %d purports %d child(ren) of at most possible %d",
                    layer_index, child_count, max_child_count);
                child_count = max_child_count;
            }
        }
    }
    else if (child_count < 0) {
        DP_warn("Layer %d has a negative child count of %d", layer_index,
                child_count);
        child_count = 0;
    }

    layer->tlp =
        DP_transient_layer_props_new_init_with_transient_children_noinc(
            layer_id,
            group ? DP_transient_layer_props_list_new_init(child_count) : NULL);
    if (title && title_length != 0) {
        DP_transient_layer_props_title_set(layer->tlp, title, title_length);
    }
    DP_transient_layer_props_blend_mode_set(layer->tlp, blend_mode);
    DP_transient_layer_props_opacity_set(layer->tlp, DP_int_to_uint16(opacity));
    DP_transient_layer_props_sketch_opacity_set(
        layer->tlp, DP_int_to_uint16(sketch_opacity));
    DP_transient_layer_props_sketch_tint_set(layer->tlp,
                                             DP_llong_to_uint32(sketch_tint));

    if (flags & DP_PROJECT_SNAPSHOT_LAYER_FLAG_HIDDEN) {
        DP_transient_layer_props_hidden_set(layer->tlp, true);
    }

    if (flags & DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED_REMOTE) {
        DP_transient_layer_props_censored_remote_set(layer->tlp, true);
    }

    if (flags & DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED_LOCAL) {
        DP_transient_layer_props_censored_local_set(layer->tlp, true);
    }

    if (flags & DP_PROJECT_SNAPSHOT_LAYER_FLAG_PASS_THROUGH) {
        if (group) {
            DP_transient_layer_props_isolated_set(layer->tlp, false);
        }
        else {
            DP_warn("Layer %d has pass-through flat set, but is not a group",
                    layer_index);
        }
    }

    if (flags & DP_PROJECT_SNAPSHOT_LAYER_FLAG_CLIP) {
        DP_transient_layer_props_clip_set(layer->tlp, true);
    }

    int width = DP_transient_canvas_state_width(c->tcs);
    int height = DP_transient_canvas_state_height(c->tcs);
    if (group) {
        layer->tlg =
            DP_transient_layer_group_new_init_with_transient_children_noinc(
                width, height, DP_transient_layer_list_new_init(child_count));
        layer->used = 0;
    }
    else {
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
    if (group) {
        DP_transient_layer_list_set_transient_group_noinc(
            parent_tll, layer->tlg, index_to_set);
    }
    else {
        DP_transient_layer_list_set_transient_content_noinc(
            parent_tll, layer->tlc, index_to_set);
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
        prj, "select\n"
             "    l.layer_index, l.parent_index, l.layer_id, l.title,\n"
             "    l.blend_mode, l.opacity, l.sketch_opacity, l.sketch_tint,\n"
             "    l.flags, l.fill, count(c.layer_index)\n"
             "from snapshot_layers l\n"
             "left join snapshot_layers c\n"
             "    on l.snapshot_id = c.snapshot_id\n"
             "    and l.layer_index = c.parent_index\n"
             "where l.snapshot_id = ?\n"
             "group by l.layer_index\n"
             "order by l.layer_index");
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
        DP_warn("Had %d excess layer(s)", increments);
    }

    int clamps = cfs_clamp_layers(c);
    if (clamps != 0) {
        DP_warn("Had to clamp %d layer list(s)", clamps);
    }

    return !error;
}

static bool cfs_read_tiles(DP_ProjectCanvasFromSnapshotContext *c,
                           size_t max_pixel_size)
{
    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select layer_index, tile_index, context_id, repeat, pixels "
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

        int context_id = sqlite3_column_int(stmt, 2);
        if (context_id < 0 || context_id > 255) {
            DP_warn("Tile index %d of layer %d context id %d out of bounds",
                    tile_index, layer_index, context_id);
            context_id = 0;
        }

        int repeat = sqlite3_column_int(stmt, 3);
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

        const unsigned char *pixels = sqlite3_column_blob(stmt, 4);
        if (!pixels) {
            DP_warn("Tile index %d of layer %d has null pixel data", tile_index,
                    layer_index);
            continue;
        }

        size_t size = DP_int_to_size(sqlite3_column_bytes(stmt, 4));
        if (size < 4) {
            DP_warn("Tile index %d of layer %d has too little pixel data",
                    tile_index, layer_index);
            continue;
        }
        else if (size > max_pixel_size) {
            DP_warn("Tile index %d of layer %d has too much pixel data",
                    tile_index, layer_index);
            continue;
        }

        DP_TransientLayerContent *tlc = layer->tlc;
        if (size == 4
            && memcmp(pixels, (unsigned char[]){0, 0, 0, 0}, 4) == 0) {
            for (int i = 0; i <= repeat; ++i) {
                DP_transient_layer_content_tile_set_noinc(tlc, NULL,
                                                          tile_index + i);
            }
        }
        else {
            DP_ProjectCanvasFromSnapshotTileJobParams params = {
                {
                    c,
                    {{{DP_int_to_uint(context_id)}, layer_index}},
                    tile_index,
                    repeat,
                    size,
                },
                pixels,
            };
            DP_worker_push_with(c->worker, cfs_insert_tile_job, &params);
        }
    }
    sqlite3_finalize(stmt);
    return !error;
}

static int cfs_count_sublayers(DP_ProjectCanvasFromSnapshotContext *c)
{
    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select count(*) from snapshot_sublayers where snapshot_id = ?");
    if (!stmt) {
        return -1;
    }

    if (!ps_bind_int64(prj, stmt, 1, c->snapshot_id)) {
        sqlite3_finalize(stmt);
        return -1;
    }

    bool error;
    int count;
    if (ps_exec_step(prj, stmt, &error)) {
        count = sqlite3_column_int(stmt, 0);
    }
    else {
        count = -1;
        if (!error) {
            error = true;
            DP_error_set("Sublayer count query resulted in no rows");
        }
    }

    sqlite3_finalize(stmt);
    return count;
}

static bool cfs_read_sublayers(DP_ProjectCanvasFromSnapshotContext *c,
                               int *out_count)
{
    int count = cfs_count_sublayers(c);
    *out_count = count;
    if (count < 0) {
        return false;
    }
    else if (count == 0) {
        return true;
    }

    c->sublayers = DP_malloc(sizeof(*c->sublayers) * DP_int_to_size(count));
    for (int i = 0; i < count; ++i) {
        c->sublayers[i] = (DP_ProjectCanvasFromSnapshotSublayer){NULL, NULL};
    }

    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select sublayer_index, layer_index, sublayer_id, blend_mode, "
             "opacity, fill from snapshot_sublayers where snapshot_id = ?");
    if (!stmt) {
        return false;
    }

    if (!ps_bind_int64(prj, stmt, 1, c->snapshot_id)) {
        sqlite3_finalize(stmt);
        return false;
    }

    int layer_count = c->layer_count;

    bool error;
    while (ps_exec_step(prj, stmt, &error)) {
        int sublayer_index = sqlite3_column_int(stmt, 0);
        if (sublayer_index < 0 || sublayer_index >= count) {
            DP_warn("Sublayer index %d out of bounds", sublayer_index);
            continue;
        }

        DP_ProjectCanvasFromSnapshotSublayer *sublayer =
            &c->sublayers[sublayer_index];
        if (sublayer->tlp) {
            DP_warn("Duplicate sublayer index %d", sublayer_index);
            continue;
        }

        int layer_index = sqlite3_column_int(stmt, 1);
        if (layer_index < 0 || layer_index >= layer_count) {
            DP_warn("Sublayer layer index %d out of bounds", layer_index);
            continue;
        }

        DP_ProjectCanvasFromSnapshotLayer *layer = &c->layers[layer_index];
        if (!layer->tlp) {
            DP_warn("Sublayer layer %d is null", layer_index);
            continue;
        }
        else if (layer->used >= 0) {
            DP_warn("Sublayer layer %d is a group", layer_index);
            continue;
        }

        int sublayer_id = sqlite3_column_int(stmt, 2);
        if (sublayer_id < 0 || sublayer_id > 255) {
            DP_warn("Sublayer %d id %d out of bounds", sublayer_index,
                    sublayer_id);
            continue;
        }

        int blend_mode = sqlite3_column_int(stmt, 3);
        if (!DP_blend_mode_valid_for_layer(blend_mode)) {
            DP_warn("Sublayer %d has invalid blend mode %d", sublayer_index,
                    blend_mode);
            blend_mode = DP_BLEND_MODE_NORMAL;
        }

        int opacity = sqlite3_column_int(stmt, 4);
        if (opacity < 0 || opacity > DP_BIT15) {
            DP_warn("Subayer %d opacity %d out of bounds", sublayer_index,
                    opacity);
            opacity = DP_BIT15;
        }

        long long fill = sqlite3_column_int64(stmt, 5);
        if (fill < 0LL || fill > UINT32_MAX) {
            DP_warn("Sublayer %d fill %lld out of bounds", sublayer_index,
                    fill);
            fill = 0;
        }

        DP_transient_layer_content_transient_sublayer(
            layer->tlc, sublayer_id, &sublayer->tlc, &sublayer->tlp);

        DP_transient_layer_props_blend_mode_set(sublayer->tlp, blend_mode);
        DP_transient_layer_props_opacity_set(sublayer->tlp,
                                             DP_int_to_uint16(opacity));

        if (fill != 0) {
            DP_transient_layer_content_fill_entirely(
                sublayer->tlc, 0,
                DP_upixel15_from_color(DP_llong_to_uint32(fill)));
        }
    }

    sqlite3_finalize(stmt);
    return !error;
}

static bool cfs_read_subtiles(DP_ProjectCanvasFromSnapshotContext *c,
                              size_t max_pixel_size, int sublayer_count)
{
    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select sublayer_index, tile_index, repeat, pixels "
             "from snapshot_sublayer_tiles where snapshot_id = ?");
    if (!stmt) {
        return false;
    }

    if (!ps_bind_int64(prj, stmt, 1, c->snapshot_id)) {
        sqlite3_finalize(stmt);
        return false;
    }

    int tile_count =
        DP_tile_total_round(DP_transient_canvas_state_width(c->tcs),
                            DP_transient_canvas_state_height(c->tcs));

    bool error;
    while (ps_exec_step(prj, stmt, &error)) {
        int sublayer_index = sqlite3_column_int(stmt, 0);
        if (sublayer_index < 0 || sublayer_index >= sublayer_count) {
            DP_warn("Tile sublayer index %d out of bounds", sublayer_index);
            continue;
        }

        DP_ProjectCanvasFromSnapshotSublayer *sublayer =
            &c->sublayers[sublayer_index];
        if (!sublayer->tlp) {
            DP_warn("Tile sublayer %d is null", sublayer_index);
            continue;
        }

        int tile_index = sqlite3_column_int(stmt, 1);
        if (tile_index < 0 || tile_index >= tile_count) {
            DP_warn("Tile index %d of layer %d out of bounds", tile_index,
                    sublayer_index);
            continue;
        }

        int repeat = sqlite3_column_int(stmt, 2);
        int max_repeat = tile_count - tile_index - 1;
        if (repeat < 0) {
            DP_warn("Tile index %d of sublayer %d repeat %d less than zero",
                    tile_index, sublayer_index, repeat);
            repeat = 0;
        }
        else if (repeat > max_repeat) {
            DP_warn(
                "Tile index %d of sublayer %d repeat %d beyond max repeat %d",
                tile_index, sublayer_index, repeat, max_repeat);
            repeat = max_repeat;
        }

        const unsigned char *pixels = sqlite3_column_blob(stmt, 3);
        if (!pixels) {
            DP_warn("Tile index %d of sublayer %d has null pixel data",
                    tile_index, sublayer_index);
            continue;
        }

        size_t size = DP_int_to_size(sqlite3_column_bytes(stmt, 3));
        if (size < 4) {
            DP_warn("Tile index %d of sublayer %d has too little pixel data",
                    tile_index, sublayer_index);
            continue;
        }
        else if (size > max_pixel_size) {
            DP_warn("Tile index %d of sublayer %d has too much pixel data",
                    tile_index, sublayer_index);
            continue;
        }

        DP_TransientLayerContent *tlc = sublayer->tlc;
        if (size == 4
            && memcmp(pixels, (unsigned char[]){0, 0, 0, 0}, 4) == 0) {
            for (int i = 0; i <= repeat; ++i) {
                DP_transient_layer_content_tile_set_noinc(tlc, NULL,
                                                          tile_index + i);
            }
        }
        else {
            DP_ProjectCanvasFromSnapshotTileJobParams params = {
                {
                    c,
                    {{{.sublayer_index = sublayer_index}, -1}},
                    tile_index,
                    repeat,
                    size,
                },
                pixels,
            };
            DP_worker_push_with(c->worker, cfs_insert_tile_job, &params);
        }
    }
    sqlite3_finalize(stmt);
    return !error;
}

static bool cfs_count_selections(DP_ProjectCanvasFromSnapshotContext *c,
                                 int *out_count)
{
    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select count (*) from (select distinct selection_id, context_id "
             "from snapshot_selection_tiles where snapshot_id = ?) limit 1");
    if (!stmt) {
        return false;
    }

    if (!ps_bind_int64(prj, stmt, 1, c->snapshot_id)) {
        sqlite3_finalize(stmt);
        return false;
    }

    bool error;
    if (ps_exec_step(prj, stmt, &error)) {
        *out_count = sqlite3_column_int(stmt, 0);
    }
    else if (!error) {
        error = true;
        DP_error_set("Selection count query resulted in no rows");
    }

    sqlite3_finalize(stmt);
    return !error;
}

static bool cfs_read_selections(DP_ProjectCanvasFromSnapshotContext *c,
                                size_t max_mask_size)
{
    int count;
    if (!cfs_count_selections(c, &count)) {
        return false;
    }

    if (count <= 0) {
        return true;
    }

    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select selection_id, context_id, tile_index, mask "
             "from snapshot_selection_tiles where snapshot_id = ? "
             "order by selection_id, context_id");
    if (!stmt) {
        return false;
    }

    if (!ps_bind_int64(prj, stmt, 1, c->snapshot_id)) {
        sqlite3_finalize(stmt);
        return false;
    }

    int width = DP_transient_canvas_state_width(c->tcs);
    int height = DP_transient_canvas_state_height(c->tcs);
    int tile_count = DP_tile_total_round(width, height);

    int used = 0;
    int current_selection_id = -1;
    int current_context_id = -1;
    DP_TransientSelectionSet *tss =
        DP_transient_canvas_state_transient_selection_set_noinc_nullable(c->tcs,
                                                                         count);
    DP_TransientSelection *tsel = NULL;
    DP_TransientLayerContent *tlc = NULL;

    bool error;
    while (ps_exec_step(prj, stmt, &error)) {
        int selection_id = sqlite3_column_int(stmt, 0);
        if (selection_id < DP_SELECTION_ID_FIRST_REMOTE
            || selection_id > DP_SELECTION_ID_MAX) {
            DP_warn("Selection id %d out of bounds", selection_id);
            continue;
        }

        int context_id = sqlite3_column_int(stmt, 1);
        if (context_id < 0 || context_id > 255) {
            DP_warn("Context id %d of selection %d out of bounds", context_id,
                    selection_id);
            continue;
        }

        int tile_index = sqlite3_column_int(stmt, 2);
        if (tile_index < 0 || tile_index >= tile_count) {
            DP_warn("Tile index %d of selection %d of user %d out of bounds",
                    tile_index, selection_id, context_id);
            continue;
        }

        size_t size;
        const unsigned char *data;
        if (sqlite3_column_type(stmt, 3) == SQLITE_NULL) {
            size = 0;
            data = NULL;
        }
        else {
            data = sqlite3_column_blob(stmt, 3);
            if (!data) {
                DP_warn("Tile index %d of selection %d of user %d has null "
                        "mask data",
                        tile_index, selection_id, context_id);
                continue;
            }

            size = DP_int_to_size(sqlite3_column_bytes(stmt, 3));
            if (size < 4) {
                DP_warn("Tile index %d of selection %d of user %d has too "
                        "little mask data",
                        tile_index, selection_id, context_id);
                continue;
            }
            else if (size > max_mask_size) {
                DP_warn("Tile index %d of selection %d of user %d has too much "
                        "mask data",
                        tile_index, selection_id, context_id);
                continue;
            }
        }

        bool need_new_selection = selection_id != current_selection_id
                               || context_id != current_context_id;
        if (need_new_selection) {
            int index = used++;
            if (index < count) {
                tsel = DP_transient_selection_new_init(
                    DP_int_to_uint(context_id), selection_id, width, height);
                DP_transient_selection_set_insert_transient_at_noinc(tss, index,
                                                                     tsel);
                current_selection_id = selection_id;
                current_context_id = context_id;
                tlc = DP_transient_selection_transient_content_noinc(tsel);
            }
            else {
                DP_warn("Selection %d of user %u index %d out of bounds",
                        selection_id, context_id, index);
                continue;
            }
        }

        if (data) {
            DP_ProjectCanvasFromSnapshotTileJobParams params = {
                {
                    c,
                    {.tsel = tsel},
                    tile_index,
                    TILE_JOB_REPEAT_SENTINEL_SELECTION,
                    size,
                },
                data,
            };
            DP_worker_push_with(c->worker, cfs_insert_tile_job, &params);
        }
        else {
            DP_transient_layer_content_tile_set_noinc(tlc, DP_tile_opaque_inc(),
                                                      tile_index);
        }
    }
    sqlite3_finalize(stmt);

    if (used < count) {
        DP_transient_selection_set_clamp(tss, used);
        DP_warn("Had to clamp %d selection(s)", count - used);
    }
    else if (used > count) {
        DP_warn("Had %d excess selections(s)", used - count);
    }

    return !error;
}

static bool cfs_read_annotations(DP_ProjectCanvasFromSnapshotContext *c)
{
    DP_Project *prj = c->prj;
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select annotation_id, content, x, y, width, height, "
             "background_color, valign, flags from snapshot_annotations "
             "where snapshot_id = ? order by annotation_index");
    if (!stmt) {
        return false;
    }

    if (!ps_bind_int64(prj, stmt, 1, c->snapshot_id)) {
        sqlite3_finalize(stmt);
        return false;
    }

    int count = c->annotation_count;
    int used = 0;
    DP_TransientCanvasState *tcs = c->tcs;
    DP_TransientAnnotationList *tal =
        DP_transient_canvas_state_transient_annotations(tcs, count);
    bool error;
    while (ps_exec_step(prj, stmt, &error)) {
        int annotation_id = sqlite3_column_int(stmt, 0);
        if (annotation_id <= 0 || annotation_id > DP_ANNOTATION_ID_MAX) {
            DP_warn("Annotation id %d out of bounds", annotation_id);
            continue;
        }

        const char *text = (const char *)sqlite3_column_text(stmt, 1);
        size_t text_length = DP_int_to_size(sqlite3_column_bytes(stmt, 1));
        if (text_length > DP_MSG_ANNOTATION_EDIT_TEXT_MAX_LEN) {
            DP_warn("Annotation %d x text length %zu out of bounds",
                    annotation_id, text_length);
            text_length = DP_MSG_ANNOTATION_EDIT_TEXT_MAX_LEN;
        }

        int x = sqlite3_column_int(stmt, 2);
#if INT_MIN != INT32_MIN || INT_MAX != INT32_MAX
        if (x < INT32_MIN || x > INT32_MAX) {
            DP_warn("Annotation %d x position %d out of bounds", annotation_id,
                    x);
            x = DP_clamp_int(x, INT32_MIN, INT32_MAX);
        }
#endif

        int y = sqlite3_column_int(stmt, 3);
#if INT_MIN != INT32_MIN || INT_MAX != INT32_MAX
        if (y < INT32_MIN || y > INT32_MAX) {
            DP_warn("Annotation %d y position %d out of bounds", annotation_id,
                    y);
            y = DP_clamp_int(y, INT32_MIN, INT32_MAX);
        }
#endif

        int width = sqlite3_column_int(stmt, 4);
        if (width < 0 || width > UINT16_MAX) {
            DP_warn("Annotation %d width %d out of bounds", annotation_id,
                    width);
            width = DP_clamp_int(width, 0, UINT16_MAX);
        }

        int height = sqlite3_column_int(stmt, 5);
        if (height < 0 || height > UINT16_MAX) {
            DP_warn("Annotation %d height %d out of bounds", annotation_id,
                    height);
            height = DP_clamp_int(height, 0, UINT16_MAX);
        }

        long long background_color = sqlite3_column_int64(stmt, 6);
        if (background_color < 0LL || background_color > UINT32_MAX) {
            DP_warn("Annotation %d background color %lld out of bounds",
                    annotation_id, background_color);
            background_color = 0LL;
        }

        int valign = sqlite3_column_int(stmt, 7);
        switch (valign) {
        case DP_ANNOTATION_VALIGN_TOP:
        case DP_ANNOTATION_VALIGN_CENTER:
        case DP_ANNOTATION_VALIGN_BOTTOM:
            break;
        default:
            DP_warn("Annotation %d unknown valign %d ", annotation_id, valign);
            valign = DP_ANNOTATION_VALIGN_TOP;
            break;
        }

        unsigned int flags = DP_int_to_uint(sqlite3_column_int(stmt, 8));

        DP_TransientAnnotation *ta = DP_transient_annotation_new_init(
            annotation_id, x, y, width, height);
        if (text && text_length != 0) {
            DP_transient_annotation_text_set(ta, text, text_length);
        }
        DP_transient_annotation_background_color_set(
            ta, DP_llong_to_uint32(background_color));
        DP_transient_annotation_valign_set(ta, valign);

        if (flags & DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_PROTECTED) {
            DP_transient_annotation_protect_set(ta, true);
        }

        if (flags & DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_ALIAS) {
            DP_transient_annotation_alias_set(ta, true);
        }

        if (flags & DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_RASTERIZE) {
            DP_transient_annotation_rasterize_set(ta, true);
        }

        if (used >= count) {
            tal = DP_transient_canvas_state_transient_annotations(tcs, 1);
        }
        DP_transient_annotation_list_set_transient_noinc(tal, ta, used);
        ++used;
    }
    sqlite3_finalize(stmt);

    if (used < count) {
        DP_transient_annotation_list_clamp(tal, used);
        DP_warn("Had to clamp %d annotation(s)", count - used);
    }
    else if (used > count) {
        DP_warn("Had %d excess annotation(s)", used - count);
    }

    return !error;
}

static bool cfs_read_key_frame_layers(DP_Project *prj, sqlite3_stmt *layer_stmt,
                                      int track_index, int frame_index,
                                      int count, DP_TransientKeyFrame **ptkf)
{
    bool bind_ok = ps_bind_int(prj, layer_stmt, 2, track_index)
                && ps_bind_int(prj, layer_stmt, 3, frame_index);
    if (!bind_ok) {
        DP_transient_key_frame_clamp(*ptkf, 0);
        return false;
    }

    int used = 0;
    bool error;
    while (ps_exec_step(prj, layer_stmt, &error)) {
        int layer_id = sqlite3_column_int(layer_stmt, 0);
        if (!DP_layer_id_normal(layer_id)) {
            DP_warn("Invalid layer id %d in frame %d of track %d", layer_id,
                    frame_index, track_index);
            continue;
        }

        unsigned int flags = DP_int_to_uint(sqlite3_column_int(layer_stmt, 1));

        if (used >= count) {
            *ptkf = DP_transient_key_frame_reserve(*ptkf, 1);
        }
        DP_transient_key_frame_layer_set(
            *ptkf, (DP_KeyFrameLayer){layer_id, flags}, used);
        ++used;
    }

    if (used < count) {
        DP_transient_key_frame_clamp(*ptkf, used);
        DP_warn("Had to clamp %d layer(s) in frame %d of track %d",
                count - used, frame_index, track_index);
    }
    else if (used > count) {
        DP_warn("Had %d excess layers(s) in frame %d of track %d", used - count,
                frame_index, track_index);
    }

    return !error;
}

static bool cfs_read_key_frames(DP_ProjectCanvasFromSnapshotContext *c,
                                sqlite3_stmt *frame_stmt,
                                sqlite3_stmt *layer_stmt, int track_index,
                                int count, int frame_count,
                                DP_TransientTrack **ptt)
{
    DP_Project *prj = c->prj;
    if (!ps_bind_int(prj, frame_stmt, 2, track_index)) {
        DP_transient_track_clamp(*ptt, 0);
        return false;
    }

    int used = 0;
    bool error;
    while (ps_exec_step(prj, frame_stmt, &error)) {
        int frame_index = sqlite3_column_int(frame_stmt, 0);
        if (frame_index < 0 || frame_index >= frame_count) {
            DP_warn("Frame index %d in track %d out of bounds", frame_index,
                    track_index);
            continue;
        }

        const char *title = (const char *)sqlite3_column_text(frame_stmt, 1);
        size_t title_length =
            DP_int_to_size(sqlite3_column_bytes(frame_stmt, 1));
        if (title_length > DP_MSG_KEY_FRAME_RETITLE_TITLE_MAX_LEN) {
            DP_warn("Frame %d title in track %d out of bounds", frame_index,
                    track_index);
            title_length = DP_MSG_KEY_FRAME_RETITLE_TITLE_MAX_LEN;
        }

        int layer_id = sqlite3_column_int(frame_stmt, 2);
        if (layer_id < 0 || layer_id > DP_UINT24_MAX) {
            DP_warn("Frame %d layer id in track %d out of bounds", frame_index,
                    track_index);
            layer_id = 0;
        }

        int kfl_count = sqlite3_column_int(frame_stmt, 3);
        if (kfl_count < 0 || kfl_count > c->layer_count) {
            DP_warn("Frame %d key frame layer count in track %d out of bounds",
                    frame_index, track_index);
            kfl_count = 0;
        }

        DP_TransientKeyFrame *tkf =
            DP_transient_key_frame_new_init(layer_id, kfl_count);
        if (title && title_length != 0) {
            DP_transient_key_frame_title_set(tkf, title, title_length);
        }

        if (kfl_count != 0
            && !cfs_read_key_frame_layers(prj, layer_stmt, track_index,
                                          frame_index, kfl_count, &tkf)) {
            DP_warn("Failed to read layers of frame %d of track %d: %s",
                    frame_index, track_index, DP_error());
        }

        if (used >= count) {
            *ptt = DP_transient_track_reserve(*ptt, 1);
        }
        DP_transient_track_set_transient_noinc(*ptt, frame_index, tkf, used);
        ++used;
    }

    if (used < count) {
        DP_transient_track_clamp(*ptt, used);
        DP_warn("Had to clamp %d key frame(s) in track %d", count - used,
                track_index);
    }
    else if (used > count) {
        DP_warn("Had %d excess frame(s) in track %d", used - count,
                track_index);
    }

    return !error;
}

static bool cfs_read_timeline(DP_ProjectCanvasFromSnapshotContext *c,
                              DP_TransientTrack **tracks)
{
    DP_Project *prj = c->prj;
    sqlite3_stmt *track_stmt = ps_prepare_ephemeral(
        prj, "select\n"
             "    t.track_index, t.track_id, t.title, t.flags,\n"
             "    count(k.frame_index)\n"
             "from snapshot_tracks t\n"
             "left join snapshot_key_frames k\n"
             "    on t.snapshot_id = k.snapshot_id\n"
             "    and t.track_index = k.track_index\n"
             "where t.snapshot_id = ?\n"
             "group by t.track_index\n"
             "order by t.track_index");
    if (!track_stmt) {
        return false;
    }

    sqlite3_stmt *frame_stmt = ps_prepare_ephemeral(
        prj, "select k.frame_index, k.title, k.layer_id, count(l.layer_id)\n"
             "from snapshot_key_frames k\n"
             "left join snapshot_key_frame_layers l\n"
             "    on k.snapshot_id = l.snapshot_id\n"
             "    and k.track_index = l.track_index\n"
             "    and k.frame_index = l.frame_index\n"
             "where k.snapshot_id = ? and k.track_index = ?\n"
             "group by k.frame_index\n"
             "order by k.frame_index");
    if (!frame_stmt) {
        sqlite3_finalize(track_stmt);
        return false;
    }

    sqlite3_stmt *layer_stmt = ps_prepare_ephemeral(
        prj, "select layer_id, flags from snapshot_key_frame_layers "
             "where snapshot_id = ? and track_index = ? and frame_index = ?");
    if (!layer_stmt) {
        sqlite3_finalize(frame_stmt);
        sqlite3_finalize(track_stmt);
        return false;
    }

    long long snapshot_id = c->snapshot_id;
    bool bind_ok = ps_bind_int64(prj, track_stmt, 1, snapshot_id)
                && ps_bind_int64(prj, frame_stmt, 1, snapshot_id)
                && ps_bind_int64(prj, layer_stmt, 1, snapshot_id);
    if (!bind_ok) {
        sqlite3_finalize(layer_stmt);
        sqlite3_finalize(frame_stmt);
        sqlite3_finalize(track_stmt);
        return false;
    }

    int count = c->track_count;
    DP_TransientCanvasState *tcs = c->tcs;
    DP_TransientTimeline *ttl =
        DP_transient_canvas_state_transient_timeline(tcs, count);

    int frame_count = DP_document_metadata_frame_count(
        DP_transient_canvas_state_metadata_noinc(tcs));
    int used = 0;
    bool error;
    while (ps_exec_step(prj, track_stmt, &error)) {
        int track_index = sqlite3_column_int(track_stmt, 0);
        if (track_index < 0 || track_index >= count) {
            DP_warn("Track index %d out of bounds", track_index);
            continue;
        }

        if (tracks[track_index]) {
            DP_warn("Duplicate track index %d", track_index);
            continue;
        }

        int track_id = sqlite3_column_int(track_stmt, 1);
        if (track_id <= 0 || track_id > DP_TRACK_ID_MAX) {
            DP_warn("Track %d id %d out of bounds", track_index, track_id);
            continue;
        }

        const char *title = (const char *)sqlite3_column_text(track_stmt, 2);
        size_t title_length =
            DP_int_to_size(sqlite3_column_bytes(track_stmt, 2));
        if (title_length > DP_MSG_TRACK_CREATE_TITLE_MAX_LEN) {
            DP_warn("Track %d title length out of bounds", track_index);
            title_length = DP_MSG_TRACK_CREATE_TITLE_MAX_LEN;
        }

        unsigned int flags = DP_int_to_uint(sqlite3_column_int(track_stmt, 3));

        int key_frame_count = sqlite3_column_int(track_stmt, 4);
        if (key_frame_count < 0 || key_frame_count > frame_count) {
            DP_warn("Track %d key frame count out of bounds", track_index);
            key_frame_count = 0;
        }

        DP_TransientTrack *tt = DP_transient_track_new_init(key_frame_count);
        DP_transient_track_id_set(tt, track_id);
        if (title && title_length != 0) {
            DP_transient_track_title_set(tt, title, title_length);
        }
        DP_transient_track_hidden_set(
            tt, flags & DP_PROJECT_SNAPSHOT_TRACK_FLAG_HIDDEN);
        DP_transient_track_onion_skin_set(
            tt, flags & DP_PROJECT_SNAPSHOT_TRACK_FLAG_ONION_SKIN);

        if (used >= count) {
            ttl = DP_transient_canvas_state_transient_timeline(tcs, 1);
        }
        DP_transient_timeline_set_transient_track_noinc(ttl, tt, used);
        ++used;

        if (key_frame_count != 0
            && !cfs_read_key_frames(c, frame_stmt, layer_stmt, track_index,
                                    key_frame_count, frame_count, &tt)) {
            DP_warn("Failed to read key frames of track %d: %s", track_index,
                    DP_error());
        }
    }
    sqlite3_finalize(layer_stmt);
    sqlite3_finalize(frame_stmt);
    sqlite3_finalize(track_stmt);

    if (used < count) {
        DP_transient_timeline_clamp(ttl, used);
        DP_warn("Had to clamp %d track(s)", count - used);
    }
    else if (used > count) {
        DP_warn("Had %d excess track(s)", used - count);
    }

    return !error;
}

static DP_CanvasState *
cfs_context_dispose(DP_ProjectCanvasFromSnapshotContext *c,
                    bool keep_canvas_state)
{
    DP_PERF_BEGIN(fn, "load:dispose");
    if (c->worker) {
        int thread_count = DP_worker_thread_count(c->worker);
        DP_PERF_BEGIN(join, "load:dispose:join");
        DP_worker_free_join(c->worker);
        DP_PERF_END(join);
        for (int i = 0; i < thread_count; ++i) {
            DP_decompress_zstd_free(&c->zstd_contexts[i]);
            DP_free(c->split_buffers[i]);
        }
        DP_free(c->zstd_contexts);
        DP_free(c->split_buffers);
    }
    DP_free(c->sublayers);
    DP_free(c->layers);
    DP_CanvasState *cs;
    if (keep_canvas_state) {
        DP_PERF_BEGIN(persist, "load:dispose:persist");
        cs = DP_transient_canvas_state_persist(c->tcs);
        DP_PERF_END(persist);
    }
    else {
        DP_transient_canvas_state_decref_nullable(c->tcs);
        cs = NULL;
    }
    DP_PERF_END(fn);
    return cs;
}

static size_t get_tile_job_size_alignment(size_t max_pixel_size)
{
    size_t job_size = DP_FLEX_SIZEOF(DP_ProjectCanvasFromSnapshotTileJob, data,
                                     max_pixel_size);
    size_t alignment = alignof(DP_ProjectCanvasFromSnapshotTileJob);
    size_t offset = job_size % alignment;
    if (offset == 0) {
        return job_size;
    }
    else {
        return job_size + (alignment - (job_size % alignment));
    }
}

DP_CanvasState *DP_project_canvas_from_snapshot(DP_Project *prj,
                                                DP_DrawContext *dc,
                                                long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(dc);
    DP_ASSERT(snapshot_id > 0LL);
    DP_PERF_BEGIN(fn, "load");

    DP_PERF_BEGIN(setup, "load:setup");
    DP_ProjectCanvasFromSnapshotContext c = {
        prj, NULL, NULL, NULL, NULL, NULL, NULL, snapshot_id, 0, 0, 0, 0, 0};
    if (!cfs_read_header(&c)) {
        return NULL;
    }

    if (!(c.snapshot_flags & DP_PROJECT_SNAPSHOT_FLAG_COMPLETE)) {
        DP_warn("Snapshot %lld is incomplete", snapshot_id);
    }

    if (c.snapshot_flags & DP_PROJECT_SNAPSHOT_FLAG_NULL_CANVAS) {
        DP_debug("Loading null canvas");
        return DP_canvas_state_new();
    }

    size_t max_pixel_size = DP_compress_zstd_bounds(DP_TILE_COMPRESSED_BYTES);
    size_t job_size = get_tile_job_size_alignment(max_pixel_size);
    int thread_count = DP_worker_cpu_count(32);
    c.worker = DP_worker_new(1024, job_size, thread_count, cfs_tile_job);
    if (!c.worker) {
        cfs_context_dispose(&c, false);
        return NULL;
    }

    c.tcs = DP_transient_canvas_state_new_init();
    c.split_buffers =
        DP_malloc(sizeof(*c.split_buffers) * DP_int_to_size(thread_count));
    c.zstd_contexts =
        DP_malloc(sizeof(*c.zstd_contexts) * DP_int_to_size(thread_count));
    for (int i = 0; i < thread_count; ++i) {
        c.split_buffers[i] = DP_malloc(sizeof(*c.split_buffers[i]));
        c.zstd_contexts[i] = NULL;
    }
    DP_PERF_END(setup);

    DP_PERF_BEGIN(metadata, "load:metadata");
    bool metadata_ok = cfs_read_metadata(&c);
    DP_PERF_END(metadata);
    if (!metadata_ok) {
        cfs_context_dispose(&c, false);
        return NULL;
    }

    // Once we got metadata, we sort of have a usable canvas. So any errors
    // after this should be non-fatal if possible.

    if (c.layer_count > 0 && c.root_layer_count > 0) {
        DP_PERF_BEGIN(layers, "load:layers");
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
        DP_PERF_END(layers);

        DP_PERF_BEGIN(tiles, "load:tiles");
        if (!cfs_read_tiles(&c, max_pixel_size)) {
            DP_warn("Error reading tiles: %s", DP_error());
        }
        DP_PERF_END(tiles);

        // Don't touch the snapshot_sublayer* tables without this flag! Earlier
        // dpcs files don't have them.
        if (c.snapshot_flags & DP_PROJECT_SNAPSHOT_FLAG_HAS_SUBLAYERS) {
            DP_PERF_BEGIN(sublayers, "load:sublayers");
            int sublayer_count;
            if (!cfs_read_sublayers(&c, &sublayer_count)) {
                DP_warn("Error reading sublayers: %s", DP_error());
            }
            DP_PERF_END(sublayers);

            DP_PERF_BEGIN(subtiles, "load:subtiles");
            if (sublayer_count > 0
                && !cfs_read_subtiles(&c, max_pixel_size, sublayer_count)) {
                DP_warn("Error reading sublayer tiles: %s", DP_error());
            }
            DP_PERF_END(subtiles);
        }
    }

    // Don't touch the snapshot_selection* tables without this flag! Earlier
    // dpcs files don't have them.
    if (c.snapshot_flags & DP_PROJECT_SNAPSHOT_FLAG_HAS_SELECTIONS) {
        DP_PERF_BEGIN(selections, "load:selections");
        if (!cfs_read_selections(&c, DP_compress_zstd_bounds(DP_TILE_LENGTH))) {
            DP_warn("Error reading selections: %s", DP_error());
        }
        DP_PERF_END(selections);
    }

    if (c.annotation_count > 0) {
        DP_PERF_BEGIN(annotations, "load:annotations");
        if (!cfs_read_annotations(&c)) {
            DP_warn("Error reading annotations: %s", DP_error());
        }
        DP_PERF_END(annotations);
    }

    if (c.track_count > 0) {
        DP_PERF_BEGIN(timeline, "load:timeline");
        DP_TransientTrack **tracks =
            DP_malloc_zeroed(sizeof(*tracks) * DP_int_to_size(c.track_count));
        if (!cfs_read_timeline(&c, tracks)) {
            DP_warn("Error reading timeline: %s", DP_error());
        }
        DP_free(tracks);
        DP_PERF_END(timeline);
    }

    DP_PERF_BEGIN(cleanup, "load:cleanup");
    DP_transient_canvas_state_layer_routes_reindex(c.tcs, dc);
    DP_transient_canvas_state_timeline_cleanup(c.tcs);
    DP_PERF_END(cleanup);

    DP_CanvasState *cs = cfs_context_dispose(&c, true);
    DP_PERF_END(fn);
    return cs;
}

static bool canvas_load_warn(DP_ProjectCanvasLoadWarnFn warn_fn, void *user,
                             int warn)
{
    return warn_fn && warn_fn(user, warn);
}

static DP_CanvasState *canvas_load_warn_cs(DP_ProjectCanvasLoadWarnFn warn_fn,
                                           void *user, int warn,
                                           DP_CanvasState *cs)
{
    if (canvas_load_warn(warn_fn, user, warn)) {
        DP_canvas_state_decref(cs);
        return NULL;
    }
    else {
        return cs;
    }
}

#define MAX_MULTIDAB_COUNT 8192

struct DP_ProjectPlaybackContext {
    DP_CanvasHistory *ch;
    DP_DrawContext *dc;
    DP_LocalState *ls;
    DP_Message **multidab_msgs;
    int multidab_count;
};

typedef void (*DP_ProjectPlaybackHandleFn)(struct DP_ProjectPlaybackContext *,
                                           DP_Message *);

static void playback_handle_single_dec(struct DP_ProjectPlaybackContext *c,
                                       DP_Message *msg)
{
    if (!DP_canvas_history_handle(c->ch, c->dc, msg)) {
        DP_warn("Error playing back project message: %s", DP_error());
    }
    DP_message_decref(msg);
}

static void playback_flush_multidab(struct DP_ProjectPlaybackContext *c)
{
    int count = c->multidab_count;
    switch (count) {
    case 0:
        break;
    case 1:
        c->multidab_count = 0;
        playback_handle_single_dec(c, c->multidab_msgs[0]);
        break;
    default:
        c->multidab_count = 0;
        DP_canvas_history_handle_multidab_dec(c->ch, c->dc, count,
                                              c->multidab_msgs);
        break;
    }
}

static void playback_handle_command(struct DP_ProjectPlaybackContext *c,
                                    DP_Message *msg)
{
    DP_MessageType type = DP_message_type(msg);
    if (DP_message_type_is_draw_dabs(type)) {
        int index = c->multidab_count++;
        c->multidab_msgs[index] = msg;
        if (index == MAX_MULTIDAB_COUNT - 1) {
            playback_flush_multidab(c);
        }
    }
    else {
        playback_flush_multidab(c);
        DP_local_state_handle(c->ls, c->dc, msg, false);
        playback_handle_single_dec(c, msg);
    }
}

static void playback_handle_soft_reset(struct DP_ProjectPlaybackContext *c,
                                       DP_Message *msg)
{
    playback_flush_multidab(c);
    DP_canvas_history_soft_reset(c->ch, c->dc, DP_message_context_id(msg), NULL,
                                 0);
    DP_message_decref(msg);
}

static void playback_handle_undo_depth(struct DP_ProjectPlaybackContext *c,
                                       DP_Message *msg)
{
    playback_flush_multidab(c);
    DP_MsgUndoDepth *mud = DP_message_internal(msg);
    DP_canvas_history_undo_depth_limit_set(c->ch, c->dc,
                                           DP_msg_undo_depth_depth(mud));
    DP_canvas_history_soft_reset(c->ch, c->dc, DP_message_context_id(msg), NULL,
                                 0);
    DP_message_decref(msg);
}

static void playback_handle_local_change(struct DP_ProjectPlaybackContext *c,
                                         DP_Message *msg)
{
    DP_local_state_handle(c->ls, c->dc, msg, false);
    DP_message_decref(msg);
}

static DP_ProjectPlaybackHandleFn playback_get_handle_fn(int type)
{
    switch (type) {
    case DP_MSG_SOFT_RESET:
        return playback_handle_soft_reset;
    case DP_MSG_UNDO_DEPTH:
        return playback_handle_undo_depth;
    case DP_MSG_LOCAL_CHANGE:
        return playback_handle_local_change;
    default:
        if (DP_message_type_command((DP_MessageType)type)) {
            return playback_handle_command;
        }
        else {
            DP_warn("Unhandled project message type %d", type);
            return NULL;
        }
    }
}

static bool playback_query(DP_Project *prj, sqlite3_stmt *stmt,
                           struct DP_ProjectPlaybackContext *c)
{
    bool error;
    while (ps_exec_step(prj, stmt, &error) && !error) {
        int type = sqlite3_column_int(stmt, 0);
        if (type < 0) { // Internal message.
            switch (type) {
            case DP_PROJECT_MESSAGE_INTERNAL_TYPE_RESET:
                DP_canvas_history_reset(c->ch);
                break;
            default:
                DP_debug("Unhandled internal project message type %d", type);
                break;
            }
        }
        else {
            DP_ProjectPlaybackHandleFn handle_fn = playback_get_handle_fn(type);
            if (handle_fn) {
                unsigned int context_id =
                    DP_int_to_uint(sqlite3_column_int(stmt, 1));
                size_t length = DP_int_to_size(sqlite3_column_bytes(stmt, 2));
                const unsigned char *buf;
                if (length == 0) {
                    buf = NULL;
                }
                else {
                    buf = sqlite3_column_blob(stmt, 2);
                }

                DP_Message *msg = DP_message_deserialize_body(
                    type, context_id, buf, length, true);
                if (msg) {
                    handle_fn(c, msg);
                }
                else {
                    DP_warn("Error deserializing project message: %s",
                            DP_error());
                }
            }
        }
    }

    playback_flush_multidab(c);
    return !error;
}

static int playback_query_snapshot_messages(DP_Project *prj,
                                            long long snapshot_id,
                                            unsigned int flags,
                                            struct DP_ProjectPlaybackContext *c)
{
    // Don't touch the snapshot_messages table without this flag! Earlier dpcs
    // files don't have it.
    if (flags & DP_PROJECT_SNAPSHOT_FLAG_HAS_MESSAGES) {
        sqlite3_stmt *stmt = ps_prepare_ephemeral(
            prj, "select type, context_id, body from snapshot_messages\n"
                 "where snapshot_id = ?\n"
                 "order by sequence_id");
        if (!stmt) {
            return DP_PROJECT_CANVAS_LOAD_WARN_PREPARE_ERROR;
        }

        bool bind_ok = ps_bind_int64(prj, stmt, 1, snapshot_id);
        if (!bind_ok) {
            sqlite3_finalize(stmt);
            return DP_PROJECT_CANVAS_LOAD_WARN_PREPARE_ERROR;
        }

        bool query_ok = playback_query(prj, stmt, c);
        sqlite3_finalize(stmt);
        if (!query_ok) {
            return DP_PROJECT_CANVAS_LOAD_WARN_QUERY_ERROR;
        }
    }
    return 0;
}

static int playback_query_messages(DP_Project *prj, long long session_id,
                                   long long first_sequence_id,
                                   struct DP_ProjectPlaybackContext *c)
{
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select type, context_id, body from messages\n"
             "where session_id = ? and sequence_id >= ?\n"
             "order by sequence_id");
    if (!stmt) {
        return DP_PROJECT_CANVAS_LOAD_WARN_PREPARE_ERROR;
    }

    bool bind_ok = ps_bind_int64(prj, stmt, 1, session_id)
                && ps_bind_int64(prj, stmt, 2, first_sequence_id);
    if (!bind_ok) {
        sqlite3_finalize(stmt);
        return DP_PROJECT_CANVAS_LOAD_WARN_PREPARE_ERROR;
    }

    bool query_ok = playback_query(prj, stmt, c);
    sqlite3_finalize(stmt);
    if (query_ok) {
        return 0;
    }
    else {
        return DP_PROJECT_CANVAS_LOAD_WARN_QUERY_ERROR;
    }
}

static DP_CanvasState *
canvas_from_snapshot_playback(DP_Project *prj, DP_DrawContext *dc,
                              long long snapshot_id, long long session_id,
                              long long first_sequence_id, unsigned int flags,
                              DP_ProjectCanvasLoadWarnFn warn_fn, void *user)
{
    DP_ASSERT(prj);
    DP_ASSERT(dc);
    DP_ASSERT(snapshot_id > 0LL);
    DP_ASSERT(session_id > 0LL);
    DP_ASSERT(first_sequence_id > 0LL);

    // FIXME: check whether there's an internal reset message after this,
    // because it's pointless to do a bunch of work just to throw it all away
    // along the way.

    DP_CanvasState *cs;
    if (flags & DP_PROJECT_SNAPSHOT_FLAG_NULL_CANVAS) {
        DP_debug("Starting from null canvas");
        cs = DP_canvas_state_new();
    }
    else {
        cs = DP_project_canvas_from_snapshot(prj, dc, snapshot_id);
        if (!cs) {
            return NULL;
        }
    }

    DP_CanvasHistory *ch =
        DP_canvas_history_new_noinc(cs, NULL, NULL, false, NULL);
    if (!ch) {
        return canvas_load_warn_cs(
            warn_fn, user, DP_PROJECT_CANVAS_LOAD_WARN_HISTORY_ERROR, cs);
    }

    DP_LocalState *ls = DP_local_state_new(cs, NULL, NULL);

    struct DP_ProjectPlaybackContext c = {
        ch, dc, ls,
        DP_malloc(sizeof(*c.multidab_msgs) * (size_t)MAX_MULTIDAB_COUNT), 0};

    // Play back snapshot messages, effectively the undo history from the point
    // of the snapshot onwards. These should only exist for autosave snapshots.
    int snapshot_messages_error =
        playback_query_snapshot_messages(prj, snapshot_id, flags, &c);
    bool cancel = snapshot_messages_error != 0
               && canvas_load_warn(warn_fn, user, snapshot_messages_error);

    // Only autosaves can have regular messages.
    if (!cancel && (flags & DP_PROJECT_SNAPSHOT_FLAG_AUTOSAVE)) {
        // Play back regular messages after the snapshot was taken. Should also
        // only exist for autosave snapshots.
        int messages_error =
            playback_query_messages(prj, session_id, first_sequence_id, &c);
        cancel = messages_error != 0
              && canvas_load_warn(warn_fn, user, messages_error);
    }

    DP_free(c.multidab_msgs);

    if (cancel) {
        cs = NULL;
    }
    else {
        cs = DP_local_state_apply_dec(ls, DP_canvas_history_get(ch), dc);
    }

    DP_local_state_free(ls);
    DP_canvas_history_free(ch);
    return cs;
}

DP_CanvasState *DP_project_canvas_from_latest_snapshot(
    DP_Project *prj, DP_DrawContext *dc, bool snapshot_only,
    DP_ProjectCanvasLoadWarnFn warn_fn, void *user)
{
    DP_ASSERT(prj);
    static_assert(DP_PROJECT_SNAPSHOT_FLAG_COMPLETE == 1,
                  "snapshot complete flag matches query");
    static_assert(DP_PROJECT_SNAPSHOT_METADATA_SEQUENCE_ID == 11,
                  "sequence id snapshot metadata matches query");
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj,
        "select h.snapshot_id, h.session_id, h.flags, m.value, i.protocol\n"
        "from snapshots h\n"
        "left join sessions i on h.session_id = i.session_id\n"
        "left join snapshot_metadata m\n"
        "    on h.snapshot_id = m.snapshot_id\n"
        "    and m.metadata_id = 11\n" // SEQUENCE_ID
        "where (h.flags & 1) <> 0\n"   // COMPLETE
        "order by h.snapshot_id desc\n"
        "limit 1");
    if (!stmt) {
        return NULL;
    }

    bool error;
    bool load_messages;
    unsigned int flags;
    long long snapshot_id;
    long long session_id;
    long long sequence_id;
    if (ps_exec_step(prj, stmt, &error)) {
        snapshot_id = sqlite3_column_int64(stmt, 0);
        session_id = sqlite3_column_int64(stmt, 1);
        flags = DP_int_to_uint(sqlite3_column_int(stmt, 2));

        sequence_id = sqlite3_column_int64(stmt, 3);
        if (sequence_id < 1LL) {
            sequence_id = 1LL;
        }

        if (snapshot_only || snapshot_id <= 0LL) {
            load_messages = false;
        }
        else {
            DP_ProtocolVersion *protover = DP_protocol_version_parse(
                (const char *)sqlite3_column_text(stmt, 4));
            if (DP_protocol_version_is_current(protover)) {
                load_messages = true;
            }
            else if (DP_protocol_version_is_future_minor_incompatibility(
                         protover)) {
                load_messages = true;
                if (canvas_load_warn(
                        warn_fn, user,
                        DP_PROJECT_CANVAS_LOAD_WARN_MINOR_INCOMPATIBILITY)) {
                    snapshot_id = -1LL;
                }
            }
            else {
                load_messages = false;
                if (canvas_load_warn(
                        warn_fn, user,
                        DP_PROJECT_CANVAS_LOAD_WARN_INCOMPATIBLE)) {
                    snapshot_id = -1LL;
                }
            }
            DP_protocol_version_free(protover);
        }
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

    // We can only load snapshots with messages in them if the recording is
    // sufficiently compatible, otherwise we fall back to just loading the
    // snapshot on its own.
    if (load_messages) {
        DP_debug("Load from autosave snapshot sequence id %lld", sequence_id);
        return canvas_from_snapshot_playback(prj, dc, snapshot_id, session_id,
                                             sequence_id, flags, warn_fn, user);
    }

    if (flags & DP_PROJECT_SNAPSHOT_FLAG_NULL_CANVAS) {
        DP_error_set("Canvas to load is null");
        return NULL;
    }

    DP_debug("Load from snapshot alone");
    return DP_project_canvas_from_snapshot(prj, dc, snapshot_id);
}


int DP_project_canvas_save(DP_CanvasState *cs, const char *path,
                           bool (*thumb_write_fn)(void *, DP_Image *,
                                                  DP_Output *),
                           void *thumb_write_user)
{
    DP_ASSERT(cs);
    DP_ASSERT(path);

    DP_ProjectOpenResult open_result =
        project_open(path, DP_PROJECT_OPEN_TRUNCATE, true);
    DP_Project *prj = open_result.project;
    if (!prj) {
        return open_result.error;
    }

    long long snapshot_id = project_snapshot_open(
        prj, DP_PROJECT_SNAPSHOT_FLAG_CANVAS, prj->session_id, false);
    if (snapshot_id <= 0LL) {
        project_close(prj, true);
        return DP_PROJECT_CANVAS_SAVE_ERROR_OPEN_SNAPSHOT;
    }

    int snapshot_result = DP_project_snapshot_canvas(
        prj, snapshot_id, cs, thumb_write_fn, thumb_write_user);
    if (snapshot_result != 0) {
        project_close(prj, true);
        return snapshot_result;
    }

    int finish_result = DP_project_snapshot_finish(prj, snapshot_id);
    if (finish_result != 0) {
        project_close(prj, true);
        return finish_result;
    }

    if (!project_close(prj, false)) {
        return DP_PROJECT_CANVAS_SAVE_ERROR_CLOSE_PROJECT;
    }

    return 0;
}

int DP_project_canvas_load(DP_DrawContext *dc, const char *path,
                           bool snapshot_only, DP_CanvasState **out_cs)
{
    DP_ASSERT(dc);
    DP_ASSERT(path);
    DP_ASSERT(out_cs);

    DP_ProjectOpenResult open_result =
        project_open(path, DP_PROJECT_OPEN_EXISTING | DP_PROJECT_OPEN_READ_ONLY,
                     snapshot_only);
    DP_Project *prj = open_result.project;
    if (!prj) {
        return open_result.error;
    }

    DP_CanvasState *cs = DP_project_canvas_from_latest_snapshot(
        prj, dc, snapshot_only, NULL, NULL);
    project_close(prj, true);
    if (!cs) {
        return DP_PROJECT_CANVAS_LOAD_ERROR_READ;
    }

    *out_cs = cs;
    return 0;
}


static int project_info_header(DP_Project *prj,
                               void (*callback)(void *, const DP_ProjectInfo *),
                               void *user)
{
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select\n"
             "    (select application_id from pragma_application_id),\n"
             "    (select user_version from pragma_user_version)");
    if (!stmt) {
        return DP_PROJECT_INFO_ERROR_PREPARE;
    }

    bool error;
    unsigned int application_id;
    unsigned int user_version;
    if (ps_exec_step(prj, stmt, &error)) {
        application_id = (unsigned int)sqlite3_column_int64(stmt, 0);
        user_version = (unsigned int)sqlite3_column_int64(stmt, 1);
    }
    else if (!error) {
        error = true;
        DP_error_set("Header query returned no values");
    }
    sqlite3_finalize(stmt);

    if (error) {
        return DP_PROJECT_INFO_ERROR_QUERY;
    }

    const char *path = sqlite3_db_filename(prj->db, "main");
    DP_ProjectInfo info = {DP_PROJECT_INFO_TYPE_HEADER,
                           {.header = {application_id, user_version, path}}};
    callback(user, &info);

    return 0;
}

static int project_info_sessions(DP_Project *prj,
                                 void (*callback)(void *,
                                                  const DP_ProjectInfo *),
                                 void *user)
{
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select"
             "    s.session_id, s.source_type, s.source_param, s.protocol,\n"
             "    s.flags, s.opened_at, s.closed_at, s.thumbnail,\n"
             "    (select count(*) from messages m\n"
             "     where s.session_id = m.session_id),\n"
             "    (select count(*) from snapshots h\n"
             "     where s.session_id = h.session_id)\n"
             "from sessions s\n"
             "order by s.session_id");
    if (!stmt) {
        return DP_PROJECT_INFO_ERROR_PREPARE;
    }

    bool error;
    while (ps_exec_step(prj, stmt, &error)) {
        long long session_id = sqlite3_column_int64(stmt, 0);
        int source_type = sqlite3_column_int(stmt, 1);
        const char *source_param = (const char *)sqlite3_column_text(stmt, 2);
        const char *protocol = (const char *)sqlite3_column_text(stmt, 3);
        unsigned int flags = (unsigned int)sqlite3_column_int64(stmt, 4);
        double opened_at = sqlite3_column_double(stmt, 5);
        double closed_at = sqlite3_column_double(stmt, 6);
        const unsigned char *thumbnail_data = sqlite3_column_blob(stmt, 7);
        size_t thumbnail_size = (size_t)sqlite3_column_bytes(stmt, 7);
        long long message_count = sqlite3_column_int64(stmt, 8);
        long long snapshot_count = sqlite3_column_int64(stmt, 9);
        DP_ProjectInfo info = {
            DP_PROJECT_INFO_TYPE_SESSION,
            {.session = {session_id, source_type, source_param, protocol, flags,
                         opened_at, closed_at, thumbnail_data, thumbnail_size,
                         message_count, snapshot_count}}};
        callback(user, &info);
    }
    sqlite3_finalize(stmt);

    if (error) {
        return DP_PROJECT_INFO_ERROR_QUERY;
    }

    return 0;
}

static int project_info_snapshots(DP_Project *prj,
                                  void (*callback)(void *,
                                                   const DP_ProjectInfo *),
                                  void *user)
{
    static_assert(DP_PROJECT_SNAPSHOT_METADATA_SEQUENCE_ID == 11,
                  "sequence id snapshot metadata matches query");
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "select"
             "    h.snapshot_id, h.session_id, h.flags, h.taken_at,\n"
             "    h.thumbnail,\n"
             "    (select value from snapshot_metadata d\n"
             "     where h.snapshot_id = d.snapshot_id\n"
             "     and d.metadata_id = 11),\n"
             "    (select count(*) from snapshot_messages m\n"
             "     where h.snapshot_id = m.snapshot_id)\n"
             "from snapshots h\n"
             "order by h.snapshot_id");
    if (!stmt) {
        return DP_PROJECT_INFO_ERROR_PREPARE;
    }

    bool error;
    while (ps_exec_step(prj, stmt, &error)) {
        long long snapshot_id = sqlite3_column_int64(stmt, 0);
        long long session_id = sqlite3_column_int64(stmt, 1);
        unsigned int flags = (unsigned int)sqlite3_column_int64(stmt, 2);
        double taken_at = sqlite3_column_double(stmt, 3);
        const unsigned char *thumbnail_data = sqlite3_column_blob(stmt, 4);
        size_t thumbnail_size = (size_t)sqlite3_column_bytes(stmt, 4);
        long long metadata_sequence_id = sqlite3_column_int64(stmt, 5);
        long long snapshot_message_count = sqlite3_column_int64(stmt, 6);
        DP_ProjectInfo info = {
            DP_PROJECT_INFO_TYPE_SNAPSHOT,
            {.snapshot = {snapshot_id, session_id, flags, taken_at,
                          thumbnail_data, thumbnail_size, metadata_sequence_id,
                          snapshot_message_count}}};
        callback(user, &info);
    }
    sqlite3_finalize(stmt);

    if (error) {
        return DP_PROJECT_INFO_ERROR_QUERY;
    }

    return 0;
}

int DP_project_info(DP_Project *prj, unsigned int flags,
                    void (*callback)(void *, const DP_ProjectInfo *),
                    void *user)
{
    if (!prj) {
        DP_error_set("No project given");
        return DP_PROJECT_INFO_ERROR_MISUSE;
    }

    if (!callback) {
        DP_error_set("No callback given");
        return DP_PROJECT_INFO_ERROR_MISUSE;
    }

    if (flags & DP_PROJECT_INFO_FLAG_HEADER) {
        int header_result = project_info_header(prj, callback, user);
        if (header_result != 0) {
            return header_result;
        }
    }

    if (flags & DP_PROJECT_INFO_FLAG_SESSIONS) {
        int sessions_result = project_info_sessions(prj, callback, user);
        if (sessions_result != 0) {
            return sessions_result;
        }
    }

    if (flags & DP_PROJECT_INFO_FLAG_SNAPSHOTS) {
        int snapshots_result = project_info_snapshots(prj, callback, user);
        if (snapshots_result != 0) {
            return snapshots_result;
        }
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
