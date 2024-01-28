#-------------------------------------------------------------------------------
# Check for the optional WS2811 library.
#-------------------------------------------------------------------------------
find_package(WS2811)

#-------------------------------------------------------------------------------
# Setup the led library.
#-------------------------------------------------------------------------------
add_library(led OBJECT led.c)
target_include_directories(led PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(led PRIVATE Piwx::Util)

if (WS2811_FOUND)
  target_compile_definitions(led PRIVATE HAS_LED_SUPPORT)
  target_link_libraries(led PUBLIC WS2811::WS2811)
endif ()

#-------------------------------------------------------------------------------
# Export the library.
#-------------------------------------------------------------------------------
add_library(Piwx::Led ALIAS led)
