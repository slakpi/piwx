#-------------------------------------------------------------------------------
# Override the image paths.
#-------------------------------------------------------------------------------
cmake_path(APPEND CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR} share images
           OUTPUT_VARIABLE IMAGE_RESOURCES)
cmake_path(APPEND CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR} share fonts
           OUTPUT_VARIABLE FONT_RESOURCES)

#-------------------------------------------------------------------------------
# Setup the configuration header.
#-------------------------------------------------------------------------------
configure_file(config.h.in config.h)

#-------------------------------------------------------------------------------
# Animation test.
#-------------------------------------------------------------------------------
add_executable(anim_test
  anim_test.c
  "${PROJECT_SOURCE_DIR}/src/anim.c"
  "${PROJECT_SOURCE_DIR}/src/draw.c")
target_include_directories(anim_test
  PRIVATE "${PROJECT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(anim_test
  PRIVATE Piwx::Conf_File Piwx::Geo Piwx::Gfx Piwx::Log Piwx::Util Piwx::Wx m)
add_test(NAME test_anim COMMAND $<TARGET_FILE:anim_test>)

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
