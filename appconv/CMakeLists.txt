# SPDX-License-Identifier: MIT

dp_add_executable(dpconv)
target_sources(dpconv PRIVATE
	dpconv.c
)
target_link_libraries(dpconv PRIVATE
	dpcommon
	dpengine
	dpmsg
)
