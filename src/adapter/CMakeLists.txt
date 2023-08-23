add_definitions(-fPIC)
add_library(adapter STATIC adapter.c session_manager.c)
target_link_libraries(adapter toml)