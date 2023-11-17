add_definitions(-fPIC)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_library(stellar_on_sapp SHARED stellar_on_sapp.c) 
target_link_libraries(stellar_on_sapp adapter)
set_target_properties(stellar_on_sapp PROPERTIES LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/version.map")
set_target_properties(stellar_on_sapp PROPERTIES PREFIX "")

install(TARGETS stellar_on_sapp LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/sapp/plug/stellar_on_sapp COMPONENT Libraries)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/start_loader.inf DESTINATION ${CMAKE_INSTALL_PREFIX}/sapp/plug/stellar_on_sapp COMPONENT Libraries)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/defer_loader.inf DESTINATION ${CMAKE_INSTALL_PREFIX}/sapp/plug/stellar_on_sapp COMPONENT Libraries)
