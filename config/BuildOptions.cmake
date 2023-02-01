option(CLIENT "Compile client" ON)
option(SERVER "Compile dedicated server" ON)
option(SERVERGUI "Enable server GUI" ON)
option(THICKSRV "Compile dedicated thick server (EXPERIMENTAL)" OFF)
option(TOOLS "Compile extra tools" OFF)
option(INSTALL_DOC "Install documents" ON)
option(INITSYS "Init system integration" "systemd")
option(TESTS "Build unit tests" OFF)
option(KIS_TABLET "Enable customized Windows tablet support code" OFF)
option(ADDRESS_SANITIZER "Enable address sanitizer" OFF)
option(CLANG_TIDY "Enable clang-tidy" OFF)
option(BUILD_LABEL "A custom label to add to the version")

set(QT_VERSION 5 CACHE STRING "Which Qt version to use (5, 6)")
if(NOT "${QT_VERSION}" MATCHES "^(5|6)$")
	message(FATAL_ERROR
		"QT_VERSION must be either '5' or '6', but is '${QT_VERSION}'")
endif()

set(DRAWDANCE_EXPORT_PATH "" CACHE STRING "Path to Drawdance.cmake")
if(NOT DRAWDANCE_EXPORT_PATH)
    message(FATAL_ERROR
		"DRAWDANCE_EXPORT_PATH not given. Build Drawdance and then set this to "
		"the path to the Drawdance.cmake in the Drawdance build directory.")
endif()

if(NOT CMAKE_BUILD_TYPE)
	message(STATUS "No build type selected, default to Release")
	message(STATUS "Use -DCMAKE_BUILD_TYPE=Debug to enable debugging")
	set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
