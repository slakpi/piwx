#-------------------------------------------------------------------------------
# Find packages.
#-------------------------------------------------------------------------------
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

#-------------------------------------------------------------------------------
# Setup the configuration file parser and lexer.
#-------------------------------------------------------------------------------
BISON_TARGET(ConfParser
  conf_parser.y
  ${CMAKE_CURRENT_BINARY_DIR}/conf.parser.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/conf.parser.h)

FLEX_TARGET(ConfLexer
  conf_lexer.l
  ${CMAKE_CURRENT_BINARY_DIR}/conf.lexer.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/conf.lexer.h)

ADD_FLEX_BISON_DEPENDENCY(ConfLexer ConfParser)

#-------------------------------------------------------------------------------
# Setup the weather phenomena lexer.
#-------------------------------------------------------------------------------
FLEX_TARGET(WxLexer
  wx_lexer.l
  ${CMAKE_CURRENT_BINARY_DIR}/wx.lexer.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/wx.lexer.h)

#-------------------------------------------------------------------------------
# Setup the library.
#-------------------------------------------------------------------------------
add_library(parsers
  STATIC ${BISON_ConfParser_OUTPUTS} ${FLEX_ConfLexer_OUTPUTS} ${FLEX_WxLexer_OUTPUTS})

target_include_directories(parsers
  PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

target_compile_options(parsers PRIVATE -Wno-unused-function -Wno-unused-variable)

#-------------------------------------------------------------------------------
# Export the library.
#-------------------------------------------------------------------------------
add_library(Parsers::Parsers ALIAS parsers)
