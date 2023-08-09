
# 开启测试

add_executable(gtest_stellar
	gtest_main.cpp
)

target_link_libraries(
  gtest_stellar
  gtest
)

include(GoogleTest)
gtest_discover_tests(gtest_stellar)