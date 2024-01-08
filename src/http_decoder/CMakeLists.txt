add_definitions(-fPIC)

include_directories(/opt/MESA/include/)

set(HTTP_SRC http_decoder.c http_decoder_utils.c http_decoder_half.c
             http_decoder_table.c http_decoder_string.c http_content_decompress.c)

add_library(http_decoder SHARED ${HTTP_SRC})
#set_target_properties(http_decoder_shared PROPERTIES LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/version.map")
target_link_libraries(http_decoder z brotlidec llhttp-static fieldstat4)
set_target_properties(http_decoder PROPERTIES PREFIX "")