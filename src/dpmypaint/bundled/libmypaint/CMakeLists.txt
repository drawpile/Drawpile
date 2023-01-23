# The bundled libmypaint is built by Cargo, so we only have an interface here.
add_library(libmypaint INTERFACE)
target_include_directories(libmypaint SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
