set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lm")

include_directories(${PROJECT_SOURCE_DIR}/deps/base64)
include_directories(${PROJECT_SOURCE_DIR}/deps/cjson)
include_directories(${PROJECT_SOURCE_DIR}/deps/mpack)
include_directories(${PROJECT_SOURCE_DIR}/deps/utable)
include_directories(${PROJECT_SOURCE_DIR}/deps/uthash)

aux_source_directory(${PROJECT_SOURCE_DIR}/deps/base64 VENDOR_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/deps/cjson VENDOR_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/deps/mpack VENDOR_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/deps/utable VENDOR_SRC)

aux_source_directory(./test TEST_SRC)


add_executable(gtest_utable
    utable.c
    ${VENDOR_SRC}
    ${TEST_SRC}
)

target_link_libraries(
  gtest_utable
  gtest
)

add_library(utable_shared SHARED utable.c  ${VENDOR_SRC})
set_target_properties(utable_shared PROPERTIES OUTPUT_NAME "utable")

add_library(utable_static STATIC utable.c ${VENDOR_SRC})
set_target_properties(utable_static PROPERTIES OUTPUT_NAME "utable")