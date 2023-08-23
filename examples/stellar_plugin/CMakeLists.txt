add_definitions(-fPIC)

add_library(simple_stellar_plugin SHARED simple_stellar_plugin.c) 
set_target_properties(simple_stellar_plugin PROPERTIES PREFIX "")