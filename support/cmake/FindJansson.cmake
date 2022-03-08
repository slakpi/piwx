#[=======================================================================[.rst:
FindJansson
-----------

Find the Jansson JSON library.

IMPORTED Targets
^^^^^^^^^^^^^^^^

The following :prop_tgt:`IMPORTED` targets may be defined:

``Jansson::Jansson``
  Jansson library.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Jansson_FOUND``
  true if Jansson headers and libraries were found
``Jansson_INCLUDE_DIRS``
  list of the include directories needed to use Jansson
``Jansson_LIBRARIES``
  Jansson libraries to be linked
#]=======================================================================]

find_library(JANSSON_LIBRARIES NAMES jansson)
find_path(JANSSON_INCLUDE_DIRS NAMES jansson.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jansson DEFAULT_MSG JANSSON_LIBRARIES JANSSON_INCLUDE_DIRS)

add_library(jansson INTERFACE)
target_link_libraries(jansson INTERFACE ${JANSSON_LIBRARIES})
target_include_directories(jansson INTERFACE ${JANSSON_INCLUDE_DIRS})

add_library(Jansson::Jansson ALIAS jansson)
