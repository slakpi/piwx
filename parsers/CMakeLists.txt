find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

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

set_target_properties(parsers PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}")
