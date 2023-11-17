add_library(cmake-config STATIC)

target_sources(cmake-config PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}/cmake-config/cmake-config/config.cpp
	${CMAKE_CURRENT_BINARY_DIR}/cmake-config/cmake-config/config.h
)

target_include_directories(cmake-config PUBLIC
	${CMAKE_CURRENT_BINARY_DIR}/cmake-config
)

message(STATUS "Generating config.h")
include(DrawpileTranslations)
get_supported_langs_cxx(DRAWPILE_LOCALES)
include(DrawpileFileExtensions)
get_qt_extensions(RECORDING DRAWPILE_FILE_GROUP_RECORDING)
get_qt_extensions(IMAGE DRAWPILE_FILE_GROUP_IMAGE)
if(SOURCE_ASSETS)
	set(DRAWPILE_SOURCE_ASSETS_DESKTOP "${PROJECT_SOURCE_DIR}/src/desktop/assets")
else()
	unset(DRAWPILE_SOURCE_ASSETS_DESKTOP)
endif()
configure_file(config.h.in cmake-config/cmake-config/config.h)

include(GitVersion)
git_version_configure_file(config.cpp.in cmake-config/cmake-config/config.cpp
	TARGET_NAME cmake-config-git_version
	VAR GIT_VERSION
	BUILD_LABEL "${BUILD_LABEL}"
)
