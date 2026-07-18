# SPDX-License-Identifier: GPL-3.0-or-later

execute_process(
	COMMAND "${DRAWPILE_CMD}" -f jpg -o "${OUTPUT_IMAGE}" -i "${INPUT_IMAGE}"
	RESULT_VARIABLE result
	OUTPUT_VARIABLE stdout
	ERROR_VARIABLE stderr
)

if(NOT result EQUAL 0)
	message(FATAL_ERROR
		"drawpile-cmd JPEG export failed with exit code ${result}\n"
		"stdout:\n${stdout}\n"
		"stderr:\n${stderr}"
	)
endif()

file(READ "${OUTPUT_IMAGE}" jpeg_header LIMIT 3 HEX)
file(REMOVE "${OUTPUT_IMAGE}")

if(NOT jpeg_header STREQUAL "ffd8ff")
	message(FATAL_ERROR
		"drawpile-cmd output is not a JPEG (header: ${jpeg_header})"
	)
endif()
