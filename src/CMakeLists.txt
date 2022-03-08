#-------------------------------------------------------------------------------
# Find packages.
#-------------------------------------------------------------------------------
find_package(CURL REQUIRED)
find_package(Jansson REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(PNG REQUIRED)
find_package(Threads REQUIRED)
find_package(SIMD REQUIRED)
find_package(WiringPi REQUIRED)

if (WITH_LED_SUPPORT)
  find_package(WS2811 REQUIRED)
endif ()

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
  PRIVATE -Wall -Werror=unused-function -Werror=unused-variable ${SIMD_C_FLAGS})

#-------------------------------------------------------------------------------
# Setup the piwx include paths.
#-------------------------------------------------------------------------------
target_include_directories(piwx PRIVATE ${PROJECT_BINARY_DIR})

#-------------------------------------------------------------------------------
# Setup the piwx linker options.
#-------------------------------------------------------------------------------
target_link_libraries(piwx
  PRIVATE Parsers::Parsers CURL::libcurl LibXml2::LibXml2 PNG::PNG Jansson::Jansson
          WiringPi::WiringPi Threads::Threads dl m)

if (WITH_LED_SUPPORT)
  target_link_libraries(piwx PRIVATE WS2811::WS2811)
endif ()
