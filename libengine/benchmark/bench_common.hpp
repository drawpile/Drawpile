#pragma once

extern "C" {
#include "dpcommon/common.h"
#include "dpcommon/cpu.h"
}

#if defined(__clang__)
#    define DISABLE_OPT_BEGIN _Pragma("clang optimize off")
#    define DISABLE_OPT_END   _Pragma("clang optimize on")
#elif defined(__GNUC__)
#    define DISABLE_OPT_BEGIN \
        _Pragma("GCC push_options") _Pragma("GCC optimize(\"O0\")")
#    define DISABLE_OPT_END _Pragma("GCC pop_options")
#elif defined(_MSC_VER)
#    define DISABLE_OPT_BEGIN _Pragma("optimize(\"\", off)")
#    define DISABLE_OPT_END   _Pragma("optimize(\"\", on)")
#endif

DISABLE_OPT_BEGIN
template <class F, class... Args>
static void DP_NOINLINE no_inline_func(F func, Args... args)
{
    func(args...);
}
DISABLE_OPT_END
