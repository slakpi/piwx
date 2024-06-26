#-------------------------------------------------------------------------------
# Find packages.
#-------------------------------------------------------------------------------
find_package(PiGPIO REQUIRED)

#-------------------------------------------------------------------------------
# Setup the configuration header.
#-------------------------------------------------------------------------------
configure_file(config.h.in config.h)

#-------------------------------------------------------------------------------
# Bring subdirectories into the project.
#-------------------------------------------------------------------------------
add_subdirectory(conf_file)
add_subdirectory(geo)
add_subdirectory(gfx)
add_subdirectory(led)
add_subdirectory(log)
add_subdirectory(util)
add_subdirectory(wx)

#-------------------------------------------------------------------------------
# Setup the piwx target.
#-------------------------------------------------------------------------------
add_executable(piwx anim.c display.c piwx.c)
target_include_directories(piwx PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(
  piwx PRIVATE PiGPIO::PiGPIO Piwx::Conf_File Piwx::Geo Piwx::Gfx Piwx::Led Piwx::Log Piwx::Util
               Piwx::Wx m)
