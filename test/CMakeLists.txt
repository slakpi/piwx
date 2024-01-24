#-------------------------------------------------------------------------------
# Config test.
#-------------------------------------------------------------------------------
add_executable(cfg_test cfg_test.c)
target_include_directories(cfg_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(cfg_test PRIVATE Piwx::Conf_File Piwx::Util)
add_test(NAME test_config COMMAND $<TARGET_FILE:cfg_test>)

#-------------------------------------------------------------------------------
# Geo test.
#-------------------------------------------------------------------------------
add_executable(geo_test geo_test.c)
target_include_directories(geo_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(geo_test PRIVATE Piwx::Geo Piwx::Util m)
add_test(NAME test_geo COMMAND $<TARGET_FILE:geo_test>)

#-------------------------------------------------------------------------------
# Enable test configuration.
#-------------------------------------------------------------------------------
enable_testing()
