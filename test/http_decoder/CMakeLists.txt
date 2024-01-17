set(DECODER_NAME http_decoder)

add_library(${DECODER_NAME}_test SHARED http_decoder_gtest.cpp)
add_dependencies(${DECODER_NAME}_test ${DECODER_NAME})
target_link_libraries(${DECODER_NAME}_test MESA_prof_load cjson)
set_target_properties(${DECODER_NAME}_test  PROPERTIES PREFIX "")

set(TEST_RUN_DIR ${CMAKE_CURRENT_BINARY_DIR}/sapp)
set(TEST_MAIN ${TEST_RUN_DIR}/plugin_test_main)

# assemble test env
add_test(NAME INSTALL_TEST_MAIN COMMAND sh -c "rpm -i ${CMAKE_CURRENT_SOURCE_DIR}/test_env/sapp4.el8.x86_64.rpm --prefix=${CMAKE_CURRENT_BINARY_DIR}/sapp --force --nodeps")

add_test(NAME COPY_TEST_MAIN COMMAND sh -c "cp ${TEST_RUN_DIR}/tools/plugin_test_main ${TEST_RUN_DIR}/plugin_test_main")
add_test(NAME COPY_CONF COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/tsgconf/ && cp ${CMAKE_CURRENT_SOURCE_DIR}/test_env/tsg_l7_protocol.conf ${TEST_RUN_DIR}/tsgconf/tsg_l7_protocol.conf")
add_test(NAME COPY_SPEC COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/stellar_plugin/ && cp ${CMAKE_CURRENT_SOURCE_DIR}/test_env/spec.toml ${TEST_RUN_DIR}/stellar_plugin/spec.toml")
add_test(NAME COPY_CONFLIST COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/plug/ && cp ${CMAKE_CURRENT_SOURCE_DIR}/test_env/conflist.inf ${TEST_RUN_DIR}/plug/conflist.inf")
add_test(NAME COPY_INF COMMAND sh -c "mkdir -p ${TEST_RUN_DIR}/plug/stellar_on_sapp && cp ${CMAKE_CURRENT_SOURCE_DIR}/test_env/start_loader.inf ${TEST_RUN_DIR}/plug/stellar_on_sapp/start_loader.inf")

# update config files
add_test(NAME UPDATE_SAPP_LOG COMMAND bash -c "sed -i 's/sapp_log.fatal/sapp_log.info/' ${TEST_RUN_DIR}/etc/sapp_log.conf")
add_test(NAME UPDATE_SAPP_SYN_MODE COMMAND bash -c "sed -i 's/syn_mandatory=1/syn_mandatory=0/' ${TEST_RUN_DIR}/etc/sapp.toml")
add_test(NAME UPDATE_SAPP_REORDER COMMAND bash -c "sed -i 's/reorder_pkt_max=32/reorder_pkt_max=5/' ${TEST_RUN_DIR}/etc/sapp.toml")         


# update plugin to be tested
add_test(NAME UPDATE_STELLAR_ON_SAPP_SO COMMAND sh -c "cp ${CMAKE_BINARY_DIR}/src/stellar_on_sapp/stellar_on_sapp.so ${TEST_RUN_DIR}/plug/stellar_on_sapp/stellar_on_sapp.so")
add_test(NAME UPDATE_PLUG_SO COMMAND sh -c "cp ${CMAKE_BINARY_DIR}/src/${DECODER_NAME}/${DECODER_NAME}.so ${TEST_RUN_DIR}/stellar_plugin/${DECODER_NAME}.so")
add_test(NAME UPDATE_TEST_SO COMMAND sh -c "cp ${CMAKE_CURRENT_BINARY_DIR}/${DECODER_NAME}_test.so ${TEST_RUN_DIR}/stellar_plugin/${DECODER_NAME}_test.so")

set_tests_properties(INSTALL_TEST_MAIN COPY_TEST_MAIN COPY_CONF COPY_SPEC COPY_CONFLIST COPY_INF
                     UPDATE_SAPP_LOG UPDATE_SAPP_SYN_MODE UPDATE_SAPP_REORDER     
                     UPDATE_STELLAR_ON_SAPP_SO UPDATE_PLUG_SO UPDATE_TEST_SO
                     PROPERTIES FIXTURES_SETUP TestFixture)

# run tests
add_test(NAME HTTP_GET_SINGLE_TRANS_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_single_trans.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_get_single_trans.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_MULTI_TRANS_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_multi_trans.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_get_multi_trans.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_LONG_COOKIE_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_long_cookie.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_get_long_cookie.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_ENCODED_URI_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_encoded_uri.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_get_encoded_uri.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_RES_GZIP_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_res_gzip.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_res_gzip.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_CHUNKED_RES_GZIP_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_chunked_res_gzip.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_chunked_res_gzip.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_OVER_TCP_KEEPALIVE_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_over_tcp_keepalive.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_over_tcp_keepalive.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_TUNNEL_FOR_POP3_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_tunnel_for_pop3.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_tunnel_for_pop3.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_OVER_PPPOE_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_over_pppoe.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_over_pppoe.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_OVER_TLS_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_over_tls.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_over_tls.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME NON_HTTP_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/non_http.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name non_http.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_REQ_1BYTE_SLIDING_WINDOW_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_req_1byte_sliding_window.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_req_1byte_sliding_window.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_RES_1BYTE_SLIDING_WINDOW_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_res_1byte_sliding_window.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_res_1byte_sliding_window.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_NO_CONTENT_LENGTH_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_no_content_length.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_no_content_length.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_POST_MULTIPART_FORM_DATA_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_post_multipart_form_data.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_post_multipart_form_data.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_HEADERS_EXCEED_MAXIMUM_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_headers_exceed_maximum.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_headers_exceed_maximum.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_CONNECT_FLOOD_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_connect_flood.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_connect_flood.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_MALFORMED_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_malformed.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_get_malformed.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_HEADER_VALUE_EMPTY_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_header_value_empty.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_header_value_empty.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_UPGRADE_WEBSOCKET_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_upgrade_websocket.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_upgrade_websocket.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_UPGRADE_HTTP2_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_upgrade_http2.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_upgrade_http2.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_MULTI_PARSE_ERROR_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_multi_parse_error.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_multi_parse_error.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_GET_REQ_PIPELINE_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_get_req_pipeline.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_get_req_pipeline.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME HTTP_TRANS_PIPELINE_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/http_trans_pipeline.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/http_pcap/ -name http_trans_pipeline.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})


set_tests_properties(HTTP_GET_SINGLE_TRANS_TEST
                     HTTP_GET_MULTI_TRANS_TEST
                     HTTP_GET_LONG_COOKIE_TEST
                     HTTP_GET_ENCODED_URI_TEST
                     HTTP_RES_GZIP_TEST
                     HTTP_CHUNKED_RES_GZIP_TEST
                     HTTP_OVER_TCP_KEEPALIVE_TEST
                     HTTP_TUNNEL_FOR_POP3_TEST
                     HTTP_OVER_PPPOE_TEST
                     HTTP_OVER_TLS_TEST
                     NON_HTTP_TEST
                     HTTP_REQ_1BYTE_SLIDING_WINDOW_TEST
                     HTTP_RES_1BYTE_SLIDING_WINDOW_TEST
                     HTTP_NO_CONTENT_LENGTH_TEST
                     HTTP_POST_MULTIPART_FORM_DATA_TEST
                     HTTP_HEADERS_EXCEED_MAXIMUM_TEST
                     HTTP_CONNECT_FLOOD_TEST
                     HTTP_GET_MALFORMED_TEST
                     HTTP_HEADER_VALUE_EMPTY_TEST
                     HTTP_MULTI_PARSE_ERROR_TEST
                     HTTP_UPGRADE_WEBSOCKET_TEST
                     HTTP_UPGRADE_HTTP2_TEST
                     HTTP_GET_REQ_PIPELINE_TEST
                     HTTP_TRANS_PIPELINE_TEST
                     PROPERTIES FIXTURES_REQUIRED TestFixture)