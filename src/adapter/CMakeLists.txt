set(CMAKE_C_FLAGS "-std=c99")
add_definitions(-fPIC)
add_library(adapter STATIC stellar.c adapter.c session_manager.c)