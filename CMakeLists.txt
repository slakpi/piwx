cmake_minimum_required(VERSION 3.22)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/support/cmake/")
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(piwx C)

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
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(GIT_COMMIT_HASH ${GIT_COMMIT_HASH})
set(RELEASE "R42-dev")

#-------------------------------------------------------------------------------
# Setup the global compile options.
#-------------------------------------------------------------------------------
add_compile_options(
  -Wall
  $<$<CONFIG:Release>:-Werror=unused-function>
  $<$<CONFIG:Release>:-Werror=unused-variable>
  $<$<CONFIG:Debug>:-D_DEBUG>)

#-------------------------------------------------------------------------------
# Bring subdirectories into the project.
#-------------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(test)

#-------------------------------------------------------------------------------
# Setup the installation targets.
#-------------------------------------------------------------------------------
install(TARGETS piwx DESTINATION bin)
install(DIRECTORY share/wxicons/ DESTINATION ${SHARE_PREFIX}wxicons FILES_MATCHING PATTERN "*.png")
install(DIRECTORY share/fonts/ DESTINATION ${SHARE_PREFIX}fonts FILES_MATCHING PATTERN "*.png")
install(FILES etc/piwx.conf.example DESTINATION ${ETC_PREFIX})
install(DIRECTORY DESTINATION ${VAR_PREFIX})
