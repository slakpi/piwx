add_compile_options($<IF:$<CONFIG:Debug>,-D_DEBUG,-DNDEBUG>)

#-------------------------------------------------------------------------------
# Config test.
#-------------------------------------------------------------------------------
add_executable(cfg_test cfg_test.c "${PROJECT_SOURCE_DIR}/src/util.c")
target_include_directories(cfg_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(cfg_test PRIVATE Piwx::Conf_File)
add_test(NAME test_config COMMAND $<TARGET_FILE:cfg_test>)

#-------------------------------------------------------------------------------
# Geo test.
#-------------------------------------------------------------------------------
add_executable(geo_test geo_test.c "${PROJECT_SOURCE_DIR}/src/util.c")
target_include_directories(geo_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(geo_test PRIVATE Piwx::Geo m)
add_test(NAME test_geo COMMAND $<TARGET_FILE:geo_test>)

#-------------------------------------------------------------------------------
# Enable test configuration.
#-------------------------------------------------------------------------------
enable_testing()
