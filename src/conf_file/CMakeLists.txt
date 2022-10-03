#-------------------------------------------------------------------------------
# Export the interface.
#-------------------------------------------------------------------------------
add_library(conf_intf INTERFACE)
target_include_directories(conf_intf
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} "${PROJECT_SOURCE_DIR}/src")
add_library(Piwx::Conf_File_Intf ALIAS conf_intf)

#-------------------------------------------------------------------------------
# Setup the configuration file parser and lexer.
#-------------------------------------------------------------------------------
BISON_TARGET(ConfParserSource
  conf_parser.y
  "${CMAKE_CURRENT_BINARY_DIR}/conf.parser.c"
  DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/conf.parser.h")

FLEX_TARGET(ConfLexerSource
  conf_lexer.l
  "${CMAKE_CURRENT_BINARY_DIR}/conf.lexer.c"
  DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/conf.lexer.h")

ADD_FLEX_BISON_DEPENDENCY(ConfLexerSource ConfParserSource)

add_library(conf_parser STATIC ${BISON_ConfParserSource_OUTPUTS} ${FLEX_ConfLexerSource_OUTPUTS})
target_link_libraries(conf_parser PRIVATE conf_intf)
target_compile_options(conf_parser PRIVATE -Wno-unused-function -Wno-unused-variable)

#-------------------------------------------------------------------------------
# Setup the configuration library to share with tests.
#-------------------------------------------------------------------------------
configure_file(config.h.in config.h)

add_library(conf STATIC conf_file.c)
target_link_libraries(conf
  PUBLIC conf_intf
  PRIVATE conf_parser)

#-------------------------------------------------------------------------------
# Export the library.
#-------------------------------------------------------------------------------
add_library(Piwx::Conf_File ALIAS conf)
