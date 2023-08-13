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

find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
if (CMAKE_CXX_CPPCHECK)
    list(
        APPEND CMAKE_CXX_CPPCHECK 
            "--enable=all"
            "--error-exitcode=1"
            "--suppress=unusedFunction"
            "--suppress=missingInclude"
            "--suppress=uselessAssignmentPtrArg"
            "--suppress=unmatchedSuppression"
            "--suppress=variableScope"
            "--suppress=unreadVariable"
            "--suppress=cstyleCast"
            "--suppress=memleakOnRealloc"
            "--suppress=constParameter"
            "--suppress=uselessAssignmentArg"
            "--suppress=uninitvar"
            "--suppress=unusedStructMember"
            "--suppress=unreachableCode"
    )
    set(CMAKE_C_CPPCHECK ${CMAKE_CXX_CPPCHECK})
else()
  message(FATAL_ERROR "Could not find the program cppcheck.")
endif()

#ASAN option
set(ASAN_OPTION "OFF" CACHE STRING
    " set asan type chosen by the user, using OFF as default")
set_property(CACHE ASAN_OPTION  PROPERTY STRINGS OFF ADDRESS THREAD)
message(STATUS "ASAN_OPTION='${ASAN_OPTION}'")

if(ASAN_OPTION STREQUAL "ADDRESS")
    set(CMAKE_C_FLAGS "${CMAKADDRESS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=address -fno-omit-frame-pointer")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=address -fno-omit-frame-pointer")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
	add_definitions(-DASAN_ENABLED=1)
elseif(ASAN_OPTION STREQUAL "THREAD")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=thread -fno-omit-frame-pointer")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DCMAKE_BUILD_TYPE=Debug -fsanitize=thread -fno-omit-frame-pointer")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lasan")
	add_definitions(-DASAN_ENABLED=1)
endif()


add_custom_target("install-program" COMMAND ${CMAKE_COMMAND} ARGS -DCOMPONENT=Program -P cmake_install.cmake)
add_custom_target("install-profile" COMMAND ${CMAKE_COMMAND} ARGS -DCOMPONENT=Profile -P cmake_install.cmake)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/deps)
include_directories(${CMAKE_SOURCE_DIR}/sdk/include)

#add_subdirectory(vendor)
add_subdirectory(deps/toml)
#add_subdirectory(deps/utable)
add_subdirectory(src/adapter)
add_subdirectory(src/adapter/test)

#enable_testing()
#add_subdirectory(test)
