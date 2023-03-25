include(CMakeDependentOption)

option(CLIENT "Compile client" ON)
add_feature_info("Drawpile client (CLIENT)" CLIENT "")

option(BUILD_LABEL "A custom label to add to the version")

option(UPDATE_TRANSLATIONS "Update translation files from source")
add_feature_info(".ts regeneration (slow!) (UPDATE_TRANSLATIONS)" UPDATE_TRANSLATIONS "")

if(WIN32)
	cmake_dependent_option(KIS_TABLET "Enable custom tablet support code" OFF "CLIENT" OFF)
	add_feature_info("Custom tablet support code (KIS_TABLET)" KIS_TABLET "")
endif()

option(SERVER "Compile dedicated server" OFF)
add_feature_info("Drawpile server (SERVER)" SERVER "")

cmake_dependent_option(SERVERGUI "Enable server GUI" ON "SERVER" OFF)
add_feature_info("Server GUI (SERVERGUI)" SERVERGUI "")

if(UNIX AND NOT APPLE)
	cmake_dependent_option(INSTALL_DOC "Install documentation" ON "SERVER" OFF)
	add_feature_info("Install documentation (INSTALL_DOC)" INSTALL_DOC "")

	cmake_dependent_option(INITSYS "Init system integration for server (options: \"\", \"systemd\")" "systemd" "SERVER" "")
	string(TOLOWER "${INITSYS}" INITSYS)
endif()

option(TOOLS "Compile extra tools" OFF)
add_feature_info("Extra tools (TOOLS)" TOOLS "")

option(TESTS "Build unit tests" OFF)
add_feature_info("Unit tests (TESTS)" TESTS "")

option(DIST_BUILD "Build for stand-alone distribution")
add_feature_info("Distribution build (DIST_BUILD)" DIST_BUILD "")

option(ENABLE_VERSION_CHECK "Enable code to check for updates" ON)
add_feature_info("Automatic update checking code (ENABLE_VERSION_CHECK)" ENABLE_VERSION_CHECK "")
