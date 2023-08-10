set(CMAKE_C_FLAGS "-std=c99")
add_definitions(-fPIC)

add_library(test_loader SHARED test_loader.c test_plugin.c) 
target_link_libraries(test_loader adapter)
set_target_properties(test_loader PROPERTIES PREFIX "")
