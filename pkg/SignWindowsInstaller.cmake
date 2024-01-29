# SPDX-License-Identifier: MIT

unset(msi_files)
foreach(package_file IN LISTS CPACK_PACKAGE_FILES)
    if("${package_file}" MATCHES "\\.[mM][sS][iI]$")
        list(APPEND msi_files "${package_file}")
    endif()
endforeach()

list(LENGTH msi_files msi_files_length)
if("${msi_files_length}" GREATER 0)
    find_program(SIGNTOOL_COMMAND signtool REQUIRED)
    foreach(msi_file IN LISTS msi_files)
        message(STATUS "Signing ${package_file}")
        execute_process(
            COMMAND
                ${SIGNTOOL_COMMAND} "sign" "/v"
                "/f" "$ENV{WINDOWS_PFX_PATH}"
                "/p" "$ENV{WINDOWS_PFX_PASS}"
                "/t" "$ENV{WINDOWS_PFX_TIMESTAMP_URL}"
                "/fd" "SHA256"
                "${msi_file}"
            COMMAND_ERROR_IS_FATAL ANY
        )
    endforeach()
endif()
