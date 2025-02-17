# SPDX-License-Identifier: MIT
dp_add_library(dpdb)
dp_target_sources(dpdb dpdb/sql.c dpdb/sql.h)
target_include_directories(dpdb PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(dpdb PUBLIC dpcommon bundled_sqlite3)

if(DEFINED QT_VERSION_MAJOR)
    dp_add_library(dpdb_qt)
    dp_target_sources(dpdb_qt dpdb/sql_qt.cpp dpdb/sql_qt.h)
    target_include_directories(dpdb_qt PUBLIC ${CMAKE_CURRENT_LIST_DIR})
    target_link_libraries(dpdb_qt PUBLIC dpdb "Qt${QT_VERSION_MAJOR}::Core")
endif()
