set(DECODER_NAME dns_decoder)

add_library(${DECODER_NAME}_test SHARED dns_decoder_gtest.cpp)
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
add_test(NAME DNS_UDP_SIMPLE_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/dns_udp_simple.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/dns_pcap/ -name dns_udp_simple.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME DNS_TCP_SIMPLE_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/dns_tcp_simple.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/dns_pcap/ -name dns_tcp_simple.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

add_test(NAME DNS_UDP_MIX_TEST COMMAND ${TEST_MAIN} ${CMAKE_CURRENT_SOURCE_DIR}/test_result_json/dns_udp_mix.json
         -f "find ${CMAKE_CURRENT_SOURCE_DIR}/dns_pcap/ -name dns_udp_mix.pcap|sort -V" WORKING_DIRECTORY ${TEST_RUN_DIR})

set_tests_properties(DNS_UDP_SIMPLE_TEST
                     DNS_TCP_SIMPLE_TEST
                     DNS_UDP_MIX_TEST
                     PROPERTIES FIXTURES_REQUIRED TestFixture)