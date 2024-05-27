#include <gtest/gtest.h>

#include "plugin_manager_mock.h"

TEST(plugin_manager_init, init_none_toml) {

	struct stellar st={0};
	struct plugin_manager_schema *pm = plugin_manager_init(&st, NULL);
	ASSERT_TRUE(pm!=NULL);
	plugin_manager_exit(pm);
}


int main(int argc, char ** argv)
{
	int ret=0;
	::testing::InitGoogleTest(&argc, argv);
	ret=RUN_ALL_TESTS();
	return ret;
}