set(DRAWPILE_PROTO_DEFAULT_PORT 27750)
set(DRAWPILE_WEBADMIN_DEFAULT_PORT 27780)
set(DP_MIN_QT_VERSION_GUI 5.12)
set(DP_MIN_QT_VERSION_SERVER 5.11)

file(STRINGS Cargo.toml dp_version LIMIT_COUNT 1 REGEX "^version[ \t]*=[ \t]*\"[^\"]*\"$")
if(dp_version MATCHES "^version[ \t]*=[ \t]*\"([0-9]+\\.[0-9]+\\.[0-9]+)(-[A-Za-z0-9.-]*)?(\\+[A-Za-z0-9.-]*)?\"")
	set(PROJECT_VERSION ${CMAKE_MATCH_1})
	set(PROJECT_VERSION_LABEL ${CMAKE_MATCH_2})
else()
	message(FATAL_ERROR "Invalid version in Cargo.toml")
endif()

set(dp_proto_regex "^[ \t]*version:[ \t]*\"?dp:([0-9]+)\\.([0-9]+)\\.([0-9]+)\"?$")
file(STRINGS extern/drawdance/generators/protogen/protocol.yaml dp_proto_version LIMIT_COUNT 1 REGEX ${dp_proto_regex})
if(dp_proto_version MATCHES ${dp_proto_regex})
	set(DRAWPILE_PROTO_SERVER_VERSION ${CMAKE_MATCH_1})
	set(DRAWPILE_PROTO_MAJOR_VERSION ${CMAKE_MATCH_2})
	set(DRAWPILE_PROTO_MINOR_VERSION ${CMAKE_MATCH_3})
else()
	message(FATAL_ERROR "Invalid protocol.yaml")
endif()

find_package(Git)
if(Git_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --tags
		OUTPUT_VARIABLE PROJECT_GIT_REVISION
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
else()
	set(PROJECT_GIT_REVISION ${PROJECT_VERSION}-unknown)
endif()
