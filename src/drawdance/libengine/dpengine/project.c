// SPDX-License-Identifier: GPL-3.0-or-later
#include "project.h"
#include "annotation.h"
#include "annotation_list.h"
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
#include "timeline.h"
#include "track.h"
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

#define DP_PERF_CONTEXT "project"


#define DP_PROJECT_STR(X)  #X
#define DP_PROJECT_XSTR(X) DP_PROJECT_STR(X)

#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_GROUP          (1u << 0u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_HIDDEN         (1u << 1u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED       (1u << 2u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_PASS_THROUGH   (1u << 3u)
#define DP_PROJECT_SNAPSHOT_LAYER_FLAG_CLIP           (1u << 4u)
#define DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_PROTECTED (1u << 0u)
#define DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_ALIAS     (1u << 1u)
#define DP_PROJECT_SNAPSHOT_ANNOTATION_FLAG_RASTERIZE (1u << 2u)
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

typedef struct DP_ProjectSnapshot {
    long long id;
    DP_ProjectSnapshotState state;
    sqlite3_stmt *stmts[DP_PROJECT_SNAPSHOT_STATEMENT_COUNT];
    DP_Mutex *mutex;
} DP_ProjectSnapshot;

struct DP_Project {
    sqlite3 *db;
    long long session_id;
    long long sequence_id;
    DP_ProjectSnapshot snapshot;
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
    TRY_SET_DB_CONFIG(db, ENABLE_ATTACH_CREATE, 0);
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

static bool init_header(sqlite3 *db, bool snapshot_only, int *out_result)
{
    const char *application_id_sql;
    const char *user_version_sql;
    if (snapshot_only) {
        application_id_sql = "pragma application_id = " DP_PROJECT_XSTR(
            DP_PROJECT_CANVAS_APPLICATION_ID);
        user_version_sql = "pragma user_version = " DP_PROJECT_XSTR(
            DP_PROJECT_CANVAS_USER_VERSION);
    }
    else {
        application_id_sql = "pragma application_id = " DP_PROJECT_XSTR(
            DP_PROJECT_APPLICATION_ID);
        user_version_sql =
            "pragma user_version = " DP_PROJECT_XSTR(DP_PROJECT_USER_VERSION);
    }

    bool ok = exec_write_stmt(db, application_id_sql, "setting application_id",
                              out_result)
           && exec_write_stmt(db, user_version_sql, "setting user_version",
                              out_result);
    return ok;
}

static int check_header(sqlite3 *db, bool snapshot_only, int *out_result)
{
    int application_id, user_version;
    bool exec_ok = exec_int_stmt(db, "pragma application_id", -1,
                                 &application_id, out_result)
                && exec_int_stmt(db, "pragma user_version", -1, &user_version,
                                 out_result);
    if (!exec_ok) {
        return DP_PROJECT_OPEN_ERROR_HEADER_READ;
    }

    int expected_application_id;
    int expected_user_version;
    if (snapshot_only) {
        expected_application_id = DP_PROJECT_CANVAS_APPLICATION_ID;
        expected_user_version = DP_PROJECT_CANVAS_USER_VERSION;
    }
    else {
        expected_application_id = DP_PROJECT_APPLICATION_ID;
        expected_user_version = DP_PROJECT_USER_VERSION;
    }

    if (application_id != expected_application_id) {
        DP_error_set("File has incorrect application id %d", application_id);
        return DP_PROJECT_OPEN_ERROR_HEADER_MISMATCH;
    }
    else if (user_version != expected_user_version) {
        DP_error_set("File has unknown user version %d", user_version);
        return DP_PROJECT_OPEN_ERROR_HEADER_MISMATCH;
    }
    else {
        return 0;
    }
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
        "    source_param text not null,\n"
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
        "    taken_at real not null,\n"
        "    thumbnail blob)\n"
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
        "    context_id integer not null,\n"
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

static bool apply_migrations_in_tx(sqlite3 *db, int *out_result)
{
    if (exec_write_stmt(db, "begin exclusive", "opening migration transaction",
                        out_result)) {
        if (apply_migrations(db, out_result)
            && exec_write_stmt(db, "commit", "committing migration transaction",
                               out_result)) {
            return true;
        }
        else {
            try_rollback(db);
        }
    }
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

        bool setup_ok =
            exec_write_stmt(db, "pragma locking_mode = exclusive",
                            "setting exclusive locking mode", &sql_result)
            && exec_write_stmt(db, "pragma journal_mode = memory",
                               "setting journal mode to memory", &sql_result)
            && exec_write_stmt(db, "pragma synchronous = off",
                               "setting synchronous off", &sql_result)
            && exec_text_stmt(
                db, "pragma locking_mode", NULL, check_pragma_result,
                (char *[]){"exclusive", "locking_mode"}, &sql_result);
        if (!setup_ok) {
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
    else if (!is_empty_db(db, &empty, &sql_result)) {
        try_close_db(db);
        return make_open_error(read_only && sql_result == SQLITE_BUSY
                                   ? DP_PROJECT_OPEN_ERROR_LOCKED
                                   : DP_PROJECT_OPEN_ERROR_READ_EMPTY,
                               sql_result);
    }
    else if (empty && read_only) {
        DP_error_set("Read-only database is empty");
        try_close_db(db);
        return make_open_error(DP_PROJECT_OPEN_ERROR_READ_EMPTY, sql_result);
    }

    if (empty && !init_header(db, snapshot_only, &sql_result)) {
        try_close_db(db);
        return make_open_error(DP_PROJECT_OPEN_ERROR_HEADER_WRITE, sql_result);
    }

    int header_error = check_header(db, snapshot_only, &sql_result);
    if (header_error != 0) {
        try_close_db(db);
        return make_open_error(header_error, sql_result);
    }

    if (!read_only && !apply_migrations_in_tx(db, &sql_result)) {
        try_close_db(db);
        return make_open_error(DP_PROJECT_OPEN_ERROR_MIGRATION, sql_result);
    }

    if (!read_only) {
        if (!exec_write_stmt(db, "pragma journal_mode = off",
                             "setting journal mode to off", &sql_result)) {
            try_close_db(db);
            return make_open_error(DP_PROJECT_OPEN_ERROR_SETUP, sql_result);
        }
    }

    sqlite3_stmt *stmts[DP_PROJECT_STATEMENT_COUNT];
    if (!snapshot_only) {
        for (int i = 0; i < DP_PROJECT_STATEMENT_COUNT; ++i) {
            sqlite3_stmt *stmt =
                prepare(db, pps_sql((DP_ProjectPersistentStatement)i),
                        SQLITE_PREPARE_PERSISTENT, &sql_result);
            if (stmt) {
                stmts[i] = stmt;
            }
            else {
                for (int j = 0; j < i; ++j) {
                    sqlite3_finalize(stmts[j]);
                }
                try_close_db(db);
                return make_open_error(DP_PROJECT_OPEN_ERROR_PREPARE,
                                       sql_result);
            }
        }
    }

    DP_Project *prj = DP_malloc(sizeof(*prj));
    prj->db = db;
    prj->session_id = 0LL;
    prj->sequence_id = 0LL;
    prj->snapshot.id = 0LL;
    prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_CLOSED;
    prj->snapshot.mutex = NULL;
    for (int i = 0; i < DP_PROJECT_SNAPSHOT_STATEMENT_COUNT; ++i) {
        prj->snapshot.stmts[i] = NULL;
    }
    if (snapshot_only) {
        for (int i = 0; i < DP_PROJECT_STATEMENT_COUNT; ++i) {
            prj->stmts[i] = NULL;
        }
    }
    else {
        memcpy(prj->stmts, stmts, sizeof(stmts));
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
    DP_ProjectVerifyStatus status = DP_PROJECT_VERIFY_ERROR;
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
               "parent_index, layer_id, title, blend_mode, opacity, "
               "sketch_opacity, sketch_tint, flags, fill) values (?, ?, ?, "
               "?, ?, ?, ?, ?, ?, ?, ?)";
    case DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_TILE:
        return "insert into snapshot_tiles (snapshot_id, layer_index, "
               "tile_index, context_id, repeat, pixels) values (?, ?, ?, ?, ?, "
               "?)";
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

static long long project_snapshot_open(DP_Project *prj, unsigned int flags)
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
        prj, "insert into snapshots (session_id, flags, taken_at) values "
             "(?, ?, unixepoch('subsec'))");
    if (!stmt) {
        DP_mutex_free(mutex);
        return DP_PROJECT_SNAPSHOT_OPEN_ERROR_PREPARE;
    }

    long long snapshot_id;
    bool write_ok = ps_bind_int64(prj, stmt, 1, prj->session_id)
                 && ps_bind_int64(prj, stmt, 2, flags)
                 && ps_exec_write(prj, stmt, &snapshot_id);
    sqlite3_finalize(stmt);
    if (!write_ok) {
        DP_mutex_free(mutex);
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
            DP_mutex_free(mutex);
            return DP_PROJECT_SNAPSHOT_OPEN_ERROR_PREPARE;
        }
    }

    DP_ASSERT(snapshot_id > 0);
    prj->snapshot.id = snapshot_id;
    prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_READY;
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

    return project_snapshot_open(prj, flags);
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
        && snapshot_write_document_metadata(prj, rec->dm);
}

static bool snapshot_handle_background(DP_Project *prj,
                                       const DP_ResetEntryBackground *reb)
{
    sqlite3_stmt *stmt =
        prj->snapshot.stmts[DP_PROJECT_SNAPSHOT_STATEMENT_INSERT_METADATA];
    return ps_bind_int(prj, stmt, 2,
                       (int)DP_PROJECT_SNAPSHOT_METADATA_BACKGROUND_TILE)
        && ps_bind_blob(prj, stmt, 3, reb->data, reb->size)
        && ps_exec_write(prj, stmt, NULL);
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
        DP_flag_uint(child_lpl, DP_PROJECT_SNAPSHOT_LAYER_FLAG_GROUP)
        | DP_flag_uint(DP_layer_props_hidden(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_HIDDEN)
        | DP_flag_uint(DP_layer_props_censored(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED)
        | DP_flag_uint(child_lpl && !DP_layer_props_isolated(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_PASS_THROUGH)
        | DP_flag_uint(DP_layer_props_clip(lp),
                       DP_PROJECT_SNAPSHOT_LAYER_FLAG_CLIP);
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
        && ps_bind_int64(prj, stmt, 4, ret->context_id)
        && ps_bind_int(prj, stmt, 5, ret->tile_run - 1)
        && ps_bind_blob(prj, stmt, 6, ret->data, ret->size)
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

static bool snapshot_handle_thumb(DP_Project *prj,
                                  const DP_ResetEntryThumb *ret)
{
    sqlite3_stmt *stmt = ps_prepare_ephemeral(
        prj, "update snapshots set thumbnail = ? where snapshot_id = ?");
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
        // TODO
        return true;
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

static int snapshot_canvas(DP_Project *prj, long long snapshot_id,
                           DP_CanvasState *cs,
                           bool (*thumb_write)(DP_Image *, DP_Output *))
{
    if (prj->snapshot.id != snapshot_id) {
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_OPEN;
    }

    if (prj->snapshot.state != DP_PROJECT_SNAPSHOT_STATE_READY) {
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_NOT_READY;
    }

    prj->snapshot.state = DP_PROJECT_SNAPSHOT_STATE_OK;
    DP_ResetImageOptions options = {
        true, true, false,      DP_RESET_IMAGE_COMPRESSION_ZSTD8LE,
        256,  256,  thumb_write};
    DP_reset_image_build_with(cs, &options, snapshot_handle_entry_callback,
                              prj);

    if (prj->snapshot.state != DP_PROJECT_SNAPSHOT_STATE_OK) {
        return DP_PROJECT_SNAPSHOT_CANVAS_ERROR_WRITE;
    }

    return 0;
}

int DP_project_snapshot_canvas(DP_Project *prj, long long snapshot_id,
                               DP_CanvasState *cs,
                               bool (*thumb_write)(DP_Image *, DP_Output *))
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id > 0LL);
    DP_PERF_BEGIN(fn, "save");
    int result = snapshot_canvas(prj, snapshot_id, cs, thumb_write);
    DP_PERF_END(fn);
    return result;
}


typedef struct DP_ProjectCanvasFromSnapshotLayer {
    DP_TransientLayerProps *tlp;
    union {
        DP_TransientLayerContent *tlc;
        DP_TransientLayerGroup *tlg;
    };
    int used;
} DP_ProjectCanvasFromSnapshotLayer;

typedef struct DP_ProjectCanvasFromSnapshotContext
    DP_ProjectCanvasFromSnapshotContext;

typedef struct DP_ProjectCanvasFromSnapshotTileJobHeader {
    DP_ProjectCanvasFromSnapshotContext *c;
    int layer_index;
    int tile_index;
    unsigned int context_id;
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
    long long snapshot_id;
    unsigned int snapshot_flags;
    int layer_count;
    int root_layer_count;
    int annotation_count;
    int track_count;
};

static void cfs_tile_job(void *element, int thread_index)
{
    DP_ProjectCanvasFromSnapshotTileJob *job = element;
    DP_ProjectCanvasFromSnapshotContext *c = job->header.c;
    DP_Tile *t = DP_tile_new_from_split_delta_zstd8le_with(
        &c->zstd_contexts[thread_index], c->split_buffers[thread_index], 0,
        job->data, job->header.size);

    int layer_index = job->header.layer_index;
    if (layer_index >= 0) {
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
    else if (t) {
        DP_transient_canvas_state_background_tile_set_noinc(c->tcs, t,
                                                            DP_tile_opaque(t));
    }
    else {
        DP_warn("Background tile failed to decompress: %s", DP_error());
    }
}

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
                    {c, -1, -1, 0, 0, size}, data};
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
        default:
            DP_warn("Unknown snapshot metadatum %d", metadata_id);
            break;
        }
    }
    sqlite3_finalize(stmt);
    if (error) {
        return false;
    }

    if (DP_canvas_state_dimensions_in_bounds(width, height)) {
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

    if (flags & DP_PROJECT_SNAPSHOT_LAYER_FLAG_CENSORED) {
        DP_transient_layer_props_censored_set(layer->tlp, true);
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
                {c, layer_index, tile_index, DP_int_to_uint(context_id), repeat,
                 size},
                pixels};
            DP_worker_push_with(c->worker, cfs_insert_tile_job, &params);
        }
    }
    sqlite3_finalize(stmt);
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
        if (layer_id < 0 || layer_id > UINT16_MAX) {
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
        DP_transient_timeline_set_transient_noinc(ttl, tt, used);
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
        DP_PERF_BEGIN(join, "load:dispose:join");
        DP_worker_free_join(c->worker);
        DP_PERF_END(join);
        int thread_count = DP_worker_thread_count(c->worker);
        for (int i = 0; i < thread_count; ++i) {
            DP_decompress_zstd_free(&c->zstd_contexts[i]);
            DP_free(c->split_buffers[i]);
        }
        DP_free(c->zstd_contexts);
        DP_free(c->split_buffers);
    }
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

DP_CanvasState *DP_project_canvas_from_snapshot(DP_Project *prj,
                                                DP_DrawContext *dc,
                                                long long snapshot_id)
{
    DP_ASSERT(prj);
    DP_ASSERT(snapshot_id);
    DP_PERF_BEGIN(fn, "load");

    DP_PERF_BEGIN(setup, "load:setup");
    DP_ProjectCanvasFromSnapshotContext c = {
        prj, NULL, NULL, NULL, NULL, NULL, snapshot_id, 0, 0, 0, 0, 0};
    if (!cfs_read_header(&c)) {
        return NULL;
    }

    if (!(c.snapshot_flags & DP_PROJECT_SNAPSHOT_FLAG_COMPLETE)) {
        DP_warn("Snapshot %lld is incomplete", snapshot_id);
    }

    size_t max_pixel_size = DP_compress_zstd_bounds(DP_TILE_COMPRESSED_BYTES);
    size_t job_size = DP_FLEX_SIZEOF(DP_ProjectCanvasFromSnapshotTileJob, data,
                                     max_pixel_size);
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


int DP_project_canvas_save(DP_CanvasState *cs, const char *path,
                           bool (*thumb_write)(DP_Image *, DP_Output *))
{
    DP_ASSERT(cs);
    DP_ASSERT(path);

    DP_ProjectOpenResult open_result =
        project_open(path, DP_PROJECT_OPEN_TRUNCATE, true);
    DP_Project *prj = open_result.project;
    if (!prj) {
        return open_result.error;
    }

    long long snapshot_id =
        project_snapshot_open(prj, DP_PROJECT_SNAPSHOT_FLAG_CANVAS);
    if (snapshot_id <= 0LL) {
        project_close(prj, true);
        return DP_PROJECT_CANVAS_SAVE_ERROR_OPEN_SNAPSHOT;
    }

    int snapshot_result =
        DP_project_snapshot_canvas(prj, snapshot_id, cs, thumb_write);
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
                           DP_CanvasState **out_cs)
{
    DP_ASSERT(dc);
    DP_ASSERT(path);
    DP_ASSERT(out_cs);

    DP_ProjectOpenResult open_result = project_open(
        path, DP_PROJECT_OPEN_EXISTING | DP_PROJECT_OPEN_READ_ONLY, true);
    DP_Project *prj = open_result.project;
    if (!prj) {
        return open_result.error;
    }

    DP_CanvasState *cs = DP_project_canvas_from_latest_snapshot(prj, dc);
    project_close(prj, true);
    if (!cs) {
        return DP_PROJECT_CANVAS_LOAD_ERROR_READ;
    }

    *out_cs = cs;
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
