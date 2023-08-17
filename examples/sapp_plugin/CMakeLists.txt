add_definitions(-fPIC)

add_library(simple_loader SHARED simple_sapp_entry.c simple_stellar_plugin.c) 
target_link_libraries(simple_loader adapter)
set_target_properties(simple_loader PROPERTIES PREFIX "")
include_directories(${CMAKE_SOURCE_DIR}/src)