#-------------------------------------------------------------------------------
# Setup the log library.
#-------------------------------------------------------------------------------
add_library(log OBJECT log.c)
target_include_directories(log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(log PRIVATE Piwx::Util)

#-------------------------------------------------------------------------------
# Export the library.
#-------------------------------------------------------------------------------
add_library(Piwx::Log ALIAS log)
