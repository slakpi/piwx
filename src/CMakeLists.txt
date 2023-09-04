add_compile_options($<IF:$<CONFIG:Debug>,-D_DEBUG,-DNDEBUG>)

#-------------------------------------------------------------------------------
# Find packages.
#-------------------------------------------------------------------------------
find_package(PiGPIO REQUIRED)
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(PNG REQUIRED)
find_package(Python3 REQUIRED)
find_package(Threads REQUIRED)

#-------------------------------------------------------------------------------
# Setup the configuration header.
#-------------------------------------------------------------------------------
configure_file(config.h.in config.h)

#-------------------------------------------------------------------------------
# Bring subdirectories into the project.
#-------------------------------------------------------------------------------
add_subdirectory(conf_file)
add_subdirectory(led)
add_subdirectory(geo)
add_subdirectory(gfx)
add_subdirectory(wx)

#-------------------------------------------------------------------------------
# Setup the piwx target.
#-------------------------------------------------------------------------------
add_executable(piwx log.c piwx.c util.c)
target_include_directories(piwx PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(
  piwx PRIVATE PiGPIO::PiGPIO Piwx::Conf_File Piwx::Geo Piwx::Gfx Piwx::Led Piwx::Wx m)
