add_executable(gtest_plugin_manager
plugin_manager_gtest_main.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/src/)

target_link_libraries(
  gtest_plugin_manager
  plugin_manager
  dl
  "-rdynamic"
  gtest
)

include(GoogleTest)
gtest_discover_tests(gtest_plugin_manager)