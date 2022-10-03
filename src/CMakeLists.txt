add_compile_options($<IF:$<CONFIG:Debug>,-D_DEBUG,-DNDEBUG>)

#-------------------------------------------------------------------------------
# Find packages.
#-------------------------------------------------------------------------------
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(CURL REQUIRED)
find_package(Jansson REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(PNG REQUIRED)
find_package(Python3 REQUIRED)
find_package(SIMD REQUIRED)
find_package(Threads REQUIRED)
find_package(WiringPi REQUIRED)
if (WITH_LED_SUPPORT)
  find_package(WS2811 REQUIRED)
endif ()

#-------------------------------------------------------------------------------
# Bring subdirectories into the project.
#-------------------------------------------------------------------------------
add_subdirectory(conf_file)
add_subdirectory(gfx)
add_subdirectory(wx)

#-------------------------------------------------------------------------------
# Setup piwx's sources and add the target.
#-------------------------------------------------------------------------------
add_executable(piwx log.c piwx.c util.c)

if (WITH_LED_SUPPORT)
  target_sources(piwx PRIVATE led.c)
endif ()

#-------------------------------------------------------------------------------
# Setup the piwx linker options.
#-------------------------------------------------------------------------------
target_link_libraries(piwx PRIVATE Piwx::Conf_File Piwx::Gfx Piwx::Wx WiringPi::WiringPi m)

if (WITH_LED_SUPPORT)
  target_link_libraries(piwx PRIVATE WS2811::WS2811)
endif ()
