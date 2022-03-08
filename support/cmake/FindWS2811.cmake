#[=======================================================================[.rst:
FindWS2811
----------

Find the WS2811 Addressable LED library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``WS2811::WS2811``
  WS2811 library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``WS2811_FOUND``
  true if WS2811 headers and libraries were found
``WS2811_INCLUDE_DIRS``
  list of the include directories needed to use WS2811
``WS2811_LIBRARIES``
  WS2811 libraries to be linked
#]=======================================================================]

find_library(WS2811_LIBRARIES NAMES ws2811)
find_path(WS2811_INCLUDE_DIRS NAMES ws2811/ws2811.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WS2811 DEFAULT_MSG WS2811_LIBRARIES WS2811_INCLUDE_DIRS)

add_library(ws2811 INTERFACE)
target_link_libraries(ws2811 INTERFACE ${WS2811_LIBRARIES})
target_include_directories(ws2811 INTERFACE ${WS2811_INCLUDE_DIRS})

add_library(WS2811::WS2811 ALIAS ws2811)
