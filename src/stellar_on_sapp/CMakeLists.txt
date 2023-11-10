add_definitions(-fPIC)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_library(stellar_on_sapp SHARED stellar_on_sapp.c) 
target_link_libraries(stellar_on_sapp adapter)
set_target_properties(stellar_on_sapp PROPERTIES PREFIX "")