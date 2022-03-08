cmake_minimum_required(VERSION 3.22)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/support/cmake/")

project(piwx C)

#-------------------------------------------------------------------------------
# Script options.
#-------------------------------------------------------------------------------
option(WITH_LED_SUPPORT "Include support for WS2811 LED weather display." OFF)

#-------------------------------------------------------------------------------
# Setup the install paths.
#-------------------------------------------------------------------------------
set(INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

if (NOT DEFINED SHARE_PREFIX)
  set(SHARE_PREFIX share/piwx/)
endif ()

if (NOT DEFINED ETC_PREFIX)
  set(ETC_PREFIX etc/)
endif ()

if (NOT DEFINED VAR_PREFIX)
  set(VAR_PREFIX var/piwx/)
endif ()

cmake_path(APPEND CMAKE_INSTALL_PREFIX ${SHARE_PREFIX} wxicons
           OUTPUT_VARIABLE IMAGE_RESOURCES)
cmake_path(APPEND CMAKE_INSTALL_PREFIX ${SHARE_PREFIX} fonts
           OUTPUT_VARIABLE FONT_RESOURCES)
cmake_path(APPEND CMAKE_INSTALL_PREFIX ${ETC_PREFIX} piwx.conf
           OUTPUT_VARIABLE CONFIG_FILE)
cmake_path(APPEND CMAKE_INSTALL_PREFIX ${VAR_PREFIX} piwx.log
           OUTPUT_VARIABLE LOG_FILE)

#-------------------------------------------------------------------------------
# Get the current Git commit hash for the version information.
#-------------------------------------------------------------------------------
execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(GIT_COMMIT_HASH ${GIT_COMMIT_HASH})

#-------------------------------------------------------------------------------
# Setup the configuration header.
#-------------------------------------------------------------------------------
configure_file(config.h.in config.h)

#-------------------------------------------------------------------------------
# Bring subdirectories into the project.
#-------------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(test)

#-------------------------------------------------------------------------------
# Setup the installation targets.
#-------------------------------------------------------------------------------
install(TARGETS piwx DESTINATION bin)
install(DIRECTORY share/wxicons/ DESTINATION ${SHARE_PREFIX}wxicons)
install(DIRECTORY share/fonts/ DESTINATION ${SHARE_PREFIX}fonts)
install(FILES etc/piwx.conf.example DESTINATION ${ETC_PREFIX})
install(DIRECTORY DESTINATION ${VAR_PREFIX})
