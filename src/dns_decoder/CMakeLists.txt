add_definitions(-fPIC)

include_directories(/opt/MESA/include/)
include_directories(${PROJECT_SOURCE_DIR}/deps/)

aux_source_directory(${PROJECT_SOURCE_DIR}/deps/mempool DEPS_SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/deps/toml DEPS_SRC)

set(DNS_SRC ${DEPS_SRC} dns_decoder.c)

add_library(dns_decoder SHARED ${DNS_SRC})
#set_target_properties(dns_decoder_shared PROPERTIES LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/version.map")
set_target_properties(dns_decoder PROPERTIES PREFIX "")