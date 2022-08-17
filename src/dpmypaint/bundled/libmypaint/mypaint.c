
#include "config.h"
#include "mypaint-config.h"

#ifdef _OPENMP
#include <omp.h>
#endif

void
ensure_max_threads_not_exceeded(void)
{
#ifdef _OPENMP
    const int max_threads = omp_get_max_threads();
    if (max_threads > MYPAINT_MAX_THREADS) {
        omp_set_num_threads(MYPAINT_MAX_THREADS);
    }
#endif
}

void
mypaint_init(void)
{
    ensure_max_threads_not_exceeded();
    //bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
}

