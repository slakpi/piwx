#-------------------------------------------------------------------------------
# Export the interface.
#-------------------------------------------------------------------------------
add_library(wx_intf INTERFACE)
target_include_directories(wx_intf
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} "${PROJECT_SOURCE_DIR}/src")
add_library(Piwx::Wx_Intf ALIAS wx_intf)

#-------------------------------------------------------------------------------
# Setup the weather phenomena lexer.
#-------------------------------------------------------------------------------
FLEX_TARGET(WxLexerSource
  wx_lexer.l
  "${CMAKE_CURRENT_BINARY_DIR}/wx.lexer.c"
  DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/wx.lexer.h")

add_library(wx_lexer STATIC ${FLEX_WxLexerSource_OUTPUTS})
target_link_libraries(wx_lexer PRIVATE wx_intf)
target_compile_options(wx_lexer PRIVATE -Wno-unused-function -Wno-unused-variable)

#-------------------------------------------------------------------------------
# Setup the wx library.
#-------------------------------------------------------------------------------
add_library(wx STATIC wx.c)
target_link_libraries(wx
  PUBLIC wx_intf
  PRIVATE wx_lexer CURL::libcurl LibXml2::LibXml2 Piwx::Geo Threads::Threads)

#-------------------------------------------------------------------------------
# Export the library.
#-------------------------------------------------------------------------------
add_library(Piwx::Wx ALIAS wx)
