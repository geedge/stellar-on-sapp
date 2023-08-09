cmake_minimum_required(VERSION 3.12)
project(stellar-c)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(Version)
include(Package)

add_definitions(-D_GNU_SOURCE)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (CMAKE_INSTALL_PREFIX "/opt/tsg/sapp" CACHE PATH "default install path" FORCE)
endif()

option(ENABLE_PIC "Generate position independent code (necessary for shared libraries)" TRUE)
option(ENABLE_WARNING_ALL "Enable all optional warnings which are desirable for normal code" TRUE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lelf")

add_custom_target("install-program" COMMAND ${CMAKE_COMMAND} ARGS -DCOMPONENT=Program -P cmake_install.cmake)
add_custom_target("install-profile" COMMAND ${CMAKE_COMMAND} ARGS -DCOMPONENT=Profile -P cmake_install.cmake)

include_directories(${CMAKE_SOURCE_DIR}/sdk/include)

add_subdirectory(vendor)
add_subdirectory(deps/toml)

enable_testing()
add_subdirectory(test)
