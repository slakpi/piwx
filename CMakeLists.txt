cmake_minimum_required(VERSION 3.6)

project("piwx" C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})

if (NOT DEFINED SHARE_PREFIX)
  set(SHARE_PREFIX share/piwx/)
endif (NOT DEFINED SHARE_PREFIX)

if (NOT DEFINED ETC_PREFIX)
  set(ETC_PREFIX etc/)
endif (NOT DEFINED ETC_PREFIX)

option(WITH_LED_SUPPORT "Include support for WS2811 LED weather display." OFF)

execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(IMAGE_RESOURCES ${CMAKE_INSTALL_PREFIX}/${SHARE_PREFIX}wxicons)
set(FONT_RESOURCES ${CMAKE_INSTALL_PREFIX}/${SHARE_PREFIX}fonts)
set(CONFIG_FILE ${CMAKE_INSTALL_PREFIX}/${ETC_PREFIX}piwx.conf)
set(GIT_COMMIT_HASH ${GIT_COMMIT_HASH})
configure_file(config.h.in config.h)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(CURL REQUIRED)
find_package(PNG REQUIRED)
find_package(Jansson REQUIRED)
find_package(WiringPi REQUIRED)
find_package(SIMD REQUIRED)

BISON_TARGET(ConfParser
  conf_parser.y
  ${CMAKE_CURRENT_BINARY_DIR}/conf.parser.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/conf.parser.h)

FLEX_TARGET(ConfLexer
  conf_lexer.l
  ${CMAKE_CURRENT_BINARY_DIR}/conf.lexer.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/conf.lexer.h)

ADD_FLEX_BISON_DEPENDENCY(ConfLexer ConfParser)

FLEX_TARGET(WxLexer
  wx_lexer.l
  ${CMAKE_CURRENT_BINARY_DIR}/wx.lexer.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/wx.lexer.h)

add_library(parsers
  ${BISON_ConfParser_OUTPUTS} ${FLEX_ConfLexer_OUTPUTS} ${FLEX_WxLexer_OUTPUTS})

target_include_directories(parsers PRIVATE
  ${CMAKE_BINARY_DIR} ${LIBXML2_INCLUDE_DIR})

target_compile_options(parsers PRIVATE
  -Wno-unused-function -Wno-unused-variable)

set(PIWX_SOURCES conf_file.c gfx.c piwx.c util.c wx.c)

if (WITH_LED_SUPPORT)
  set(PIWX_SOURCES ${PIWX_SOURCES} led.c)
endif (WITH_LED_SUPPORT)

add_executable(piwx ${PIWX_SOURCES})

target_include_directories(piwx PRIVATE
  ${CMAKE_BINARY_DIR} ${LIBXML2_INCLUDE_DIR})

target_compile_options(piwx PRIVATE
  -Wall -Werror=unused-function -Werror=unused-variable)

if (SIMD_FOUND)
  target_compile_options(piwx PRIVATE ${SIMD_C_FLAGS})
endif ()

set(PIWX_LIBS
  parsers ${CURL_LIBRARIES} ${LIBXML2_LIBRARIES} ${PNG_LIBRARIES}
  ${JANSSON_LIBRARIES} ${WIRINGPI_LIBRARIES})

if (WITH_LED_SUPPORT)
  set(PIWX_LIBS ${PIWX_LIBS} ws2811)
endif (WITH_LED_SUPPORT)

target_link_libraries(piwx ${PIWX_LIBS})

if (WITH_LED_SUPPORT)
  add_executable(led_test test/led_test.c)
  target_link_libraries(led_test ws2811)
endif (WITH_LED_SUPPORT)

install(TARGETS piwx DESTINATION bin)
install(DIRECTORY wxicons/ DESTINATION ${SHARE_PREFIX}wxicons)
install(DIRECTORY fonts/ DESTINATION ${SHARE_PREFIX}fonts)
install(FILES piwx.conf.example DESTINATION ${ETC_PREFIX})
