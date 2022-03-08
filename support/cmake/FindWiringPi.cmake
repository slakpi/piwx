#[=======================================================================[.rst:
FindWiringPi
------------

Find the WiringPi C library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``WiringPi::WiringPi``
  WiringPi library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``WiringPi_FOUND``
  true if WiringPi headers and libraries were found
``WiringPi_INCLUDE_DIRS``
  list of the include directories needed to use WiringPi
``WiringPi_LIBRARIES``
  WiringPi libraries to be linked
#]=======================================================================]

find_library(WIRINGPI_LIBRARIES NAMES wiringPi)
find_path(WIRINGPI_INCLUDE_DIRS NAMES wiringPi.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WiringPi DEFAULT_MSG WIRINGPI_LIBRARIES WIRINGPI_INCLUDE_DIRS)

add_library(wiringPi INTERFACE)
target_link_libraries(wiringPi INTERFACE ${WIRINGPI_LIBRARIES})
target_include_directories(wiringPi INTERFACE ${WIRINGPI_INCLUDE_DIRS})

add_library(WiringPi::WiringPi ALIAS wiringPi)
