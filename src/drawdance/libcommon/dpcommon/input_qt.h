#ifndef DPCOMMON_INPUT_QT_H
#define DPCOMMON_INPUT_QT_H
#include "common.h"
#include "input.h"

#ifdef __cplusplus
class QFile;
#else
typedef struct QFile QFile;
#endif


// Need DP_input_new as a function pointer to avoid a circular dependency.
typedef DP_Input *(*DP_InputQtNewFn)(DP_InputInitFn init, void *arg,
                                     size_t internal_size);


DP_Input *DP_qfile_input_new(QFile *file, bool close, DP_InputQtNewFn new_fn);

DP_Input *DP_qfile_input_new_from_path(const char *path,
                                       DP_InputQtNewFn new_fn);


#endif
