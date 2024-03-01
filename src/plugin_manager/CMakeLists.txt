add_definitions(-fPIC)

include_directories(${CMAKE_SOURCE_DIR}/deps)
include_directories(${CMAKE_SOURCE_DIR}/src/stellar_on_sapp)

add_library(plugin_manager STATIC plugin_manager.c)
target_link_libraries(plugin_manager toml bitmap)
