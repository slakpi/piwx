cmake_minimum_required(VERSION 3.6)

project("piwx")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
set(INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
if (NOT DEFINED SHARE_PREFIX)
  set(SHARE_PREFIX share/piwx/)
endif (NOT DEFINED SHARE_PREFIX)
if (NOT DEFINED ETC_PREFIX)
  set(ETC_PREFIX etc/)
endif (NOT DEFINED ETC_PREFIX)
set(IMAGE_RESOURCES ${CMAKE_INSTALL_PREFIX}/${SHARE_PREFIX}wxicons)
set(FONT_RESOURCES ${CMAKE_INSTALL_PREFIX}/${SHARE_PREFIX}fonts)
set(CONFIG_FILE ${CMAKE_INSTALL_PREFIX}/${ETC_PREFIX}piwx.conf)
configure_file(config.h.in config.h)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(LibXml2 REQUIRED)

BISON_TARGET(ConfParser
  conf.y
  ${CMAKE_CURRENT_BINARY_DIR}/conf.parser.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/conf.parser.h)
FLEX_TARGET(ConfLexer
  conf.l
  ${CMAKE_CURRENT_BINARY_DIR}/conf.lexer.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/conf.lexer.h)
ADD_FLEX_BISON_DEPENDENCY(ConfLexer ConfParser)

include_directories(piwx ${CMAKE_BINARY_DIR} ${LIBXML2_INCLUDE_DIR})
add_executable(piwx
  gfx.c
  wx.c
  config.c
  piwx.c
  ${BISON_ConfParser_OUTPUTS}
  ${FLEX_ConfLexer_OUTPUTS})
target_link_libraries(piwx curl xml2 png jansson)

install(TARGETS piwx DESTINATION bin)
install(DIRECTORY wxicons/ DESTINATION ${SHARE_PREFIX}wxicons)
install(DIRECTORY fonts/ DESTINATION ${SHARE_PREFIX}fonts)
install(FILES piwx.conf.example DESTINATION ${ETC_PREFIX})
