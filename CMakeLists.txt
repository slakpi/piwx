cmake_minimum_required(VERSION 3.6)

project("piwx")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wno-unused-function")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
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

BISON_TARGET(ConfParser
  conf_file.y
  ${CMAKE_CURRENT_BINARY_DIR}/conf_file.parser.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/conf_file.parser.h)
FLEX_TARGET(ConfLexer
  conf_file.l
  ${CMAKE_CURRENT_BINARY_DIR}/conf_file.lexer.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/conf_file.lexer.h)
ADD_FLEX_BISON_DEPENDENCY(ConfLexer ConfParser)

FLEX_TARGET(WxTypeLexer
  wxtype.l
  ${CMAKE_CURRENT_BINARY_DIR}/wxtype.lexer.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/wxtype.lexer.h)

include_directories(piwx ${CMAKE_BINARY_DIR} ${LIBXML2_INCLUDE_DIR})
add_executable(piwx
  config.c
  gfx.c
  piwx.c
  wx.c
  ${BISON_ConfParser_OUTPUTS}
  ${FLEX_ConfLexer_OUTPUTS}
  ${FLEX_WxTypeLexer_OUTPUTS})

set(PIWX_LIBS curl xml2 png jansson wiringPi)

if (WITH_LED_SUPPORT)
  set(PIWX_LIBS ${PIWX_LIBS} ws2811)
endif (WITH_LED_SUPPORT)

target_link_libraries(piwx ${PIWX_LIBS})

if (WITH_LED_SUPPORT)
  add_executable(led_test
    test/led_test.c)
  target_link_libraries(led_test ws2811)
endif (WITH_LED_SUPPORT)

install(TARGETS piwx DESTINATION bin)
install(DIRECTORY wxicons/ DESTINATION ${SHARE_PREFIX}wxicons)
install(DIRECTORY fonts/ DESTINATION ${SHARE_PREFIX}fonts)
install(FILES piwx.conf.example DESTINATION ${ETC_PREFIX})
