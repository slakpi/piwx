add_compile_options($<IF:$<CONFIG:Debug>,-D_DEBUG,-DNDEBUG>)

#-------------------------------------------------------------------------------
# Graphics test.
#-------------------------------------------------------------------------------
add_executable(gfx_test gfx_test.c "${PROJECT_SOURCE_DIR}/src/util.c")
target_include_directories(gfx_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(gfx_test PRIVATE Piwx::Conf_File Piwx::Gfx m)
target_compile_options(gfx_test PRIVATE ${SIMD_C_FLAGS})

#-------------------------------------------------------------------------------
# LED test.
#-------------------------------------------------------------------------------
if (WITH_LED_SUPPORT)
  add_executable(led_test led_test.c)
  target_link_libraries(led_test WS2811::WS2811)
endif ()
