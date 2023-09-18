#ifndef DPCOMMON_OUTPUT_QT_H
#define DPCOMMON_OUTPUT_QT_H
#include "common.h"
#include "output.h"

#ifdef __cplusplus
class QFile;
#else
typedef struct QFile QFile;
#endif


// Need DP_output_new as a function pointer to avoid a circular dependency.
typedef DP_Output *(*DP_OutputQtNewFn)(DP_OutputInitFn init, void *arg,
                                       size_t internal_size);


DP_Output *DP_qfile_output_new(QFile *file, bool close,
                               DP_OutputQtNewFn new_fn);

DP_Output *DP_qfile_output_new_from_path(const char *path,
                                         DP_OutputQtNewFn new_fn);

DP_Output *DP_qsavefile_output_new_from_path(const char *path,
                                             DP_OutputQtNewFn new_fn);

#ifdef DP_QT_IO_KARCHIVE
DP_Output *DP_karchive_gzip_output_new_from_path(const char *path,
                                                 DP_OutputQtNewFn new_fn);
#endif


#endif
