#[=======================================================================[.rst:
FindPiGPIO
------------

Find the PiGPIO C library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``PiGPIO::PiGPIO``
  PiGPIO library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``PIGPIO_FOUND``
  true if PiGPIO headers and libraries were found
``PIGPIO_INCLUDE_DIRS``
  list of the include directories needed to use PiGPIO
``PIGPIO_LIBRARIES``
  PiGPIO libraries to be linked
#]=======================================================================]

find_library(PIGPIO_LIBRARIES NAMES pigpio)
find_path(PIGPIO_INCLUDE_DIRS NAMES pigpio.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PiGPIO DEFAULT_MSG PIGPIO_LIBRARIES PIGPIO_INCLUDE_DIRS)

if(PIGPIO_FOUND AND NOT TARGET PiGPIO)
  add_library(PiGPIO INTERFACE)
  target_link_libraries(PiGPIO INTERFACE ${PIGPIO_LIBRARIES})
  target_include_directories(PiGPIO INTERFACE ${PIGPIO_INCLUDE_DIRS})

  add_library(PiGPIO::PiGPIO ALIAS PiGPIO)
endif()
