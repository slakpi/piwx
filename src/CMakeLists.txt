#-------------------------------------------------------------------------------
# Bring subdirectories into the project.
#-------------------------------------------------------------------------------
add_subdirectory(parsers)

#-------------------------------------------------------------------------------
# Setup piwx's sources and add the target.
#-------------------------------------------------------------------------------
add_executable(piwx conf_file.c gfx.c log.c piwx.c util.c wx.c)

if (WITH_LED_SUPPORT)
  target_sources(piwx PRIVATE led.c)
endif ()

#-------------------------------------------------------------------------------
# Setup the piwx compile options.
#-------------------------------------------------------------------------------
target_compile_options(piwx
  PRIVATE -Wall -Werror=unused-function -Werror=unused-variable)

if (SIMD_FOUND)
  target_compile_options(piwx PRIVATE ${SIMD_C_FLAGS})
endif ()

#-------------------------------------------------------------------------------
# Setup the piwx include paths.
#-------------------------------------------------------------------------------
target_include_directories(piwx
  PRIVATE ${PROJECT_BINARY_DIR} Parsers::Parsers ${LIBXML2_INCLUDE_DIR})

if (WITH_LED_SUPPORT)
  target_include_directories(piwx PRIVATE ${WS2811_INCLUDE_DIRS})
endif ()

#-------------------------------------------------------------------------------
# Setup the piwx linker options.
#-------------------------------------------------------------------------------
target_link_libraries(piwx
  PRIVATE Parsers::Parsers ${CURL_LIBRARIES} ${LIBXML2_LIBRARIES} ${PNG_LIBRARIES}
          ${JANSSON_LIBRARIES} ${WIRINGPI_LIBRARIES} Threads::Threads dl m)

if (WITH_LED_SUPPORT)
  target_link_libraries(piwx PRIVATE ${WS2811_LIBRARIES})
endif ()
