include(CMakeDependentOption)

# Client options
option(CLIENT "Compile client" ON)
add_feature_info("Drawpile client (CLIENT)" CLIENT "")
option(BUILD_LABEL "A custom label to add to the version")

if(WIN32)
	cmake_dependent_option(KIS_TABLET "Enable custom tablet support code" OFF "CLIENT" OFF)
	add_feature_info("Custom tablet support code (KIS_TABLET)" KIS_TABLET "")
endif()

# Server options
option(SERVER "Compile dedicated server" OFF)
add_feature_info("Drawpile server (SERVER)" SERVER "")

cmake_dependent_option(SERVERGUI "Enable server GUI" ON "SERVER" OFF)
add_feature_info("Server GUI (SERVERGUI)" SERVERGUI "")

if(UNIX AND NOT APPLE)
	cmake_dependent_option(INSTALL_DOC "Install documentation" ON "SERVER" OFF)
	add_feature_info("Install documentation (INSTALL_DOC)" INSTALL_DOC "")

	cmake_dependent_option(INITSYS "Init system integration for server (options: \"\", \"systemd\")" "systemd" "SERVER" "")
	add_feature_info("Server init system integration (INITSYS)" INITSYS "")
endif()

# Tools options
option(TOOLS "Compile extra tools" OFF)
add_feature_info("Extra tools (TOOLS)" TOOLS "")

option(TESTS "Build unit tests" OFF)
add_feature_info("Unit tests (TESTS)" TESTS "")
