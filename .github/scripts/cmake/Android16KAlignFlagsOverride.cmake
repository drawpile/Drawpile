# ECM is ridiculously stubborn about being passed ldflags. We need this
# separate file to get 16K alignment on Android, which we pass via
# -DCMAKE_PROJECT_INCLUDE. No other way actually gets them through.
string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384")
string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384")
string(APPEND CMAKE_MODULE_LINKER_FLAGS " -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384")
