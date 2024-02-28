
# 开启测试

add_executable(gtest_stellar
	gtest_main.cpp
)

target_link_libraries(
  gtest_stellar
  gtest
)

add_subdirectory(http_decoder)
add_subdirectory(dns_decoder)

include(GoogleTest)
gtest_discover_tests(gtest_stellar)