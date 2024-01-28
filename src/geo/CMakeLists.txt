#-------------------------------------------------------------------------------
# Setup the geo library.
#-------------------------------------------------------------------------------
add_library(geo OBJECT ecef.c sun.c)
target_include_directories(geo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(geo PRIVATE Piwx::Util)

#-------------------------------------------------------------------------------
# Export the library.
#-------------------------------------------------------------------------------
add_library(Piwx::Geo ALIAS geo)
