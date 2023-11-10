add_definitions(-fPIC)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_library(publisher_loader SHARED publisher_loader.c) 
target_link_libraries(publisher_loader adapter)
set_target_properties(publisher_loader PROPERTIES PREFIX "")
