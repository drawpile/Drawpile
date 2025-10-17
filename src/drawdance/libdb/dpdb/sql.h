// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPDB_SQL_H
#define DPDB_SQL_H
#include <dpcommon/common.h>
// This header just includes the bundled sqlite3.h, it's just here because MSVC
// sometimes gets distracted by some other, outdated sqlite3.h on the system.
#include <drawpile_sqlite3.h>


int DP_sql_init(void);

void DP_sql_clear(sqlite3 *db);


typedef struct DP_SqlRecover DP_SqlRecover;

DP_SqlRecover *DP_sql_recover_new(const char *src_path, const char *dst_path);

void DP_sql_recover_free(DP_SqlRecover *r);

bool DP_sql_recover_step(DP_SqlRecover *r, bool *out_error);


#endif
