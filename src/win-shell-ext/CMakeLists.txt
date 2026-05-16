if(NOT WIN32)
    message(FATAL_ERROR "Windows shell extension can only be built on windows")
endif()

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

add_library(drawpile_shell_ext SHARED
	main.cpp
	ThumbnailProvider.cpp
	dpcs.cpp
	exports.def
)
target_link_libraries(drawpile_shell_ext PRIVATE
	bundled_sqlite3
	runtimeobject.lib
)
if(NOT MSVC) 
    # Needed for WRL
    target_compile_options(drawpile_shell_ext PRIVATE -Wno-language-extension-token -Wno-c++98-compat-extra-semi)
endif()
dp_sign_executable(drawpile_shell_ext)
dp_install_executables(TARGETS drawpile_shell_ext)

if(TESTS)
	add_executable(test_drawpile_shell_ext test_main.cpp)
	target_link_libraries(test_drawpile_shell_ext PRIVATE shlwapi.lib)
endif()
