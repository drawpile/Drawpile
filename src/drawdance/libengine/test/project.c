// SPDX-License-Identifier: GPL-3.0-or-later
#include <dpcommon/common.h>
#include <dpcommon/file.h>
#include <dpcommon/output.h>
#include <dpdb/sql.h>
#include <dpengine/project.h>
#include <dpmsg/message.h>
#include <dptest.h>

static bool remove_preexisting(TEST_PARAMS, const char *path)
{
    return OK(!DP_file_exists(path) || DP_file_remove(path),
              "Remove preexisting file %s", path)
        && INT_EQ_OK(DP_project_check(path), DP_PROJECT_CHECK_ERROR_OPEN,
                     "Removed project file can't be opened");
}

static bool dump_project(TEST_PARAMS, DP_Project *prj, const char *path)
{
    DP_Output *output = DP_file_output_new_from_path(path);
    if (!NOT_NULL_OK(output, "Open dump output %s", path)) {
        return false;
    }
    bool dump_ok = OK(DP_project_dump(prj, output), "Dump project");
    return OK(DP_output_free(output), "Close dump output") && dump_ok;
}

static bool dump_project_ok(TEST_PARAMS, DP_Project *prj,
                            const char *actual_path, const char *expected_path)
{
    return dump_project(TEST_ARGS, prj, actual_path)
        && FILE_EQ_OK(actual_path, expected_path, "Project dump matches");
}


static void project_basics(TEST_PARAMS)
{
    const char *path = "test/tmp/project_basics.dppr";
    remove_preexisting(TEST_ARGS, path);

    DP_ProjectOpenResult open = DP_project_open(path, DP_PROJECT_OPEN_EXISTING);
    if (!NULL_OK(open.project,
                 "Opening nonexistent file with EXISTING flag fails")) {
        DP_project_close(open.project);
    }

    INT_EQ_OK(open.error, DP_PROJECT_OPEN_ERROR_OPEN,
              "Opening nonexisten file with EXISTING flag fails on open");

    open = DP_project_open(path, 0);
    if (!NOT_NULL_OK(open.project, "Open fresh project")) {
        return;
    }

    INT_EQ_OK(DP_project_verify(open.project, DP_PROJECT_VERIFY_QUICK),
              DP_PROJECT_VERIFY_OK, "Quick verify ok");
    INT_EQ_OK(DP_project_verify(open.project, DP_PROJECT_VERIFY_FULL),
              DP_PROJECT_VERIFY_OK, "Full verify ok");

    dump_project_ok(TEST_ARGS, open.project,
                    "test/tmp/project_basics_dump01_blank",
                    "test/data/project_basics_dump01_blank");

    OK(DP_project_close(open.project), "Close project");

    INT_EQ_OK(DP_project_check(path), DP_PROJECT_USER_VERSION,
              "Project file checks out with version %d",
              DP_PROJECT_USER_VERSION);

    open = DP_project_open(path, DP_PROJECT_OPEN_EXISTING);
    if (!NOT_NULL_OK(open.project, "Reopen project with EXISTING flag")) {
        return;
    }

    INT_EQ_OK(DP_project_session_id(open.project), 0LL, "No session open");
    dump_project_ok(TEST_ARGS, open.project,
                    "test/tmp/project_basics_dump02_reopen",
                    "test/data/project_basics_dump02_reopen");

    OK(DP_project_session_open(open.project, DP_PROJECT_SOURCE_BLANK, NULL,
                               "dp:4.24.0"),
       "Open session");

    INT_EQ_OK(DP_project_session_id(open.project), 1LL, "Session 1 open");
    dump_project_ok(TEST_ARGS, open.project,
                    "test/tmp/project_basics_dump03_session1",
                    "test/data/project_basics_dump03_session1");

    OK(DP_project_close(open.project), "Close project");

    open = DP_project_open(path, 0);
    if (!NOT_NULL_OK(open.project, "Reopen project")) {
        return;
    }

    INT_EQ_OK(DP_project_session_id(open.project), 0LL, "No session open");
    dump_project_ok(TEST_ARGS, open.project,
                    "test/tmp/project_basics_dump04_autoclose",
                    "test/data/project_basics_dump04_autoclose");

    OK(DP_project_session_open(open.project, DP_PROJECT_SOURCE_FILE_OPEN,
                               "some/file.dppr", "dp:4.24.1"),
       "Open another session");

    INT_EQ_OK(DP_project_session_id(open.project), 2LL, "Session 2 open");
    dump_project_ok(TEST_ARGS, open.project,
                    "test/tmp/project_basics_dump05_session2",
                    "test/data/project_basics_dump05_session2");

    NOK(DP_project_session_open(open.project, DP_PROJECT_SOURCE_SESSION_JOIN,
                                "drawpile://whatever/something", "dp:4.24.2"),
        "Trying to open session while another one is open fails");

    INT_EQ_OK(DP_project_session_id(open.project), 2LL, "Session 2 open");
    dump_project_ok(TEST_ARGS, open.project,
                    "test/tmp/project_basics_dump06_nodupe",
                    "test/data/project_basics_dump06_nodupe");

    INT_EQ_OK(DP_project_session_close(open.project), 1, "Closing session");

    INT_EQ_OK(DP_project_session_id(open.project), 0LL, "No session open");
    dump_project_ok(TEST_ARGS, open.project,
                    "test/tmp/project_basics_dump07_close",
                    "test/data/project_basics_dump07_close");

    INT_EQ_OK(DP_project_session_close(open.project), -1,
              "Closing session when none is open");
    INT_EQ_OK(DP_project_session_id(open.project), 0LL, "No session open");

    INT_EQ_OK(DP_project_verify(open.project, DP_PROJECT_VERIFY_QUICK),
              DP_PROJECT_VERIFY_OK, "Quick verify ok");
    INT_EQ_OK(DP_project_verify(open.project, DP_PROJECT_VERIFY_FULL),
              DP_PROJECT_VERIFY_OK, "Full verify ok");

    OK(DP_project_close(open.project), "Close project");

    open = DP_project_open(path, DP_PROJECT_OPEN_TRUNCATE);
    if (!NOT_NULL_OK(open.project, "Reopen project with TRUNCATE flag")) {
        return;
    }

    INT_EQ_OK(DP_project_session_id(open.project), 0LL, "No session open");
    dump_project_ok(TEST_ARGS, open.project,
                    "test/tmp/project_basics_dump08_truncate",
                    "test/data/project_basics_dump08_truncate");

    OK(DP_project_close(open.project), "Close project");

    INT_EQ_OK(DP_project_check(path), DP_PROJECT_USER_VERSION,
              "Truncated project file checks out with version %d",
              DP_PROJECT_USER_VERSION);
}

static void project_lock(TEST_PARAMS)
{
    const char *path = "test/tmp/project_lock.dppr";
    remove_preexisting(TEST_ARGS, path);

    DP_ProjectOpenResult open1 = DP_project_open(path, 0);
    if (!NOT_NULL_OK(open1.project, "Open fresh project")) {
        return;
    }

    DP_ProjectOpenResult open2 =
        DP_project_open(path, DP_PROJECT_OPEN_EXISTING);
    if (NULL_OK(open2.project, "Fail to reopen open project")) {
        INT_EQ_OK(open2.error, DP_PROJECT_OPEN_ERROR_SETUP,
                  "Reopen open project gives SETUP error");
        INT_EQ_OK(open2.sql_result & 0xff, SQLITE_BUSY,
                  "Reopen open project gives SQLITE_BUSY error");
        OK(DP_project_open_was_busy(&open2),
           "Reopen open project detected as being busy");
    }
    else {
        DP_project_close(open2.project);
    }

    OK(DP_project_close(open1.project), "Close project");
}

static void register_tests(REGISTER_PARAMS)
{
    REGISTER_TEST(project_basics);
    REGISTER_TEST(project_lock);
}

int main(int argc, char **argv)
{
    return DP_test_main(argc, argv, register_tests, NULL);
}
