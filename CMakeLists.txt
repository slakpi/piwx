cmake_minimum_required(VERSION 3.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/support/cmake/")

project(piwx C)

#-------------------------------------------------------------------------------
# Script options.
#-------------------------------------------------------------------------------
option(WITH_LED_SUPPORT "Include support for WS2811 LED weather display." OFF)

#-------------------------------------------------------------------------------
# Find packages and setup build variables.
#-------------------------------------------------------------------------------
find_package(CURL REQUIRED)
find_package(Jansson REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(PNG REQUIRED)
find_package(Threads REQUIRED)
find_package(SIMD REQUIRED)
find_package(WiringPi REQUIRED)
find_package(WS2811)

if (WITH_LED_SUPPORT AND WS2811_FOUND)
  set(add_led_support TRUE)
endif ()

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

#-------------------------------------------------------------------------------
# Get the current Git commit hash for the version information. Setup the
# configuration header.
#-------------------------------------------------------------------------------
execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(IMAGE_RESOURCES ${CMAKE_INSTALL_PREFIX}/${SHARE_PREFIX}wxicons)
set(FONT_RESOURCES ${CMAKE_INSTALL_PREFIX}/${SHARE_PREFIX}fonts)
set(CONFIG_FILE ${CMAKE_INSTALL_PREFIX}/${ETC_PREFIX}piwx.conf)
set(LOG_FILE ${CMAKE_INSTALL_PREFIX}/${VAR_PREFIX}piwx.log)
set(GIT_COMMIT_HASH ${GIT_COMMIT_HASH})
configure_file(config.h.in config.h)

#-------------------------------------------------------------------------------
# Bring subdirectories into the project.
#-------------------------------------------------------------------------------
add_subdirectory(parsers)
add_subdirectory(test)

#-------------------------------------------------------------------------------
# Setup piwx's sources and add the target.
#-------------------------------------------------------------------------------
add_executable(piwx conf_file.c gfx.c log.c piwx.c util.c wx.c)

if (add_led_support)
  target_sources(piwx PRIVATE led.c)
endif ()

#-------------------------------------------------------------------------------
# Setup the piwx compile options.
#-------------------------------------------------------------------------------
target_compile_options(piwx PRIVATE
  -Wall -Werror=unused-function -Werror=unused-variable)

if (SIMD_FOUND)
  target_compile_options(piwx PRIVATE ${SIMD_C_FLAGS})
endif ()

#-------------------------------------------------------------------------------
# Setup the piwx include paths.
#-------------------------------------------------------------------------------
target_include_directories(piwx PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR} parsers http ${LIBXML2_INCLUDE_DIR})

if (add_led_support)
  target_include_directories(piwx PRIVATE ${WS2811_INCLUDE_DIRS})
endif ()

#-------------------------------------------------------------------------------
# Setup the piwx linker options.
#-------------------------------------------------------------------------------
target_link_libraries(piwx PRIVATE
  parsers ${CURL_LIBRARIES} ${LIBXML2_LIBRARIES} ${PNG_LIBRARIES}
  ${JANSSON_LIBRARIES} ${WIRINGPI_LIBRARIES} Threads::Threads dl m)

if (add_led_support)
  target_link_libraries(piwx PRIVATE ${WS2811_LIBRARIES})
endif ()

#-------------------------------------------------------------------------------
# Setup the installation targets.
#-------------------------------------------------------------------------------
install(TARGETS piwx DESTINATION bin)
install(DIRECTORY share/wxicons/ DESTINATION ${SHARE_PREFIX}wxicons)
install(DIRECTORY share/fonts/ DESTINATION ${SHARE_PREFIX}fonts)
install(FILES etc/piwx.conf.example DESTINATION ${ETC_PREFIX})
install(DIRECTORY DESTINATION ${VAR_PREFIX})
