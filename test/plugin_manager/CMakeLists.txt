
# 开启测试

add_executable(gtest_plugin_manager
plugin_manager_gtest_main.cpp
)

target_link_libraries(
  gtest_plugin_manager
  gtest
)

include(GoogleTest)
gtest_discover_tests(gtest_plugin_manager)