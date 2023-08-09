set(CMAKE_C_FLAGS "-std=c99")
add_definitions(-fPIC)
add_library(adapter STATIC adapter.c subsystem_manager.c session_manager.c session_mq.c)