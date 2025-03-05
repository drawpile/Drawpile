// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPDB_SQL_H
#define DPDB_SQL_H
#include <dpcommon/common.h>
#include <sqlite3.h>


int DP_sql_init(void);


typedef struct DP_SqlRecover DP_SqlRecover;

DP_SqlRecover *DP_sql_recover_new(const char *src_path, const char *dst_path);

void DP_sql_recover_free(DP_SqlRecover *r);

bool DP_sql_recover_step(DP_SqlRecover *r, bool *out_error);


#endif
