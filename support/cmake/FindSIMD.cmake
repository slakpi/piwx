#[=======================================================================[.rst:
FindSIMD
--------

Determine if SIMD extensions are available for the target system.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``SIMD_C_FLAGS``
  C flags for SIMD extensions.
``SIMD_CXX_FLAGS``
  C++ flags for SIMD extensions.
``SIMD_FOUND``
  If false, do not try to use SIMD extensions.
#]=======================================================================]

if(CMAKE_SYSTEM_PROCESSOR MATCHES "(AMD64|x86)")
  if(CMAKE_C_COMPILER_ID MATCHES "(GNU|Clang)")
    set(SIMD_C_FLAGS -mavx)
    set(SIMD_CXX_FLAGS -mavx)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "(arm|aarch64)")
  if(CMAKE_C_COMPILER_ID MATCHES "(GNU|Clang)")
    set(SIMD_C_FLAGS -mfpu=neon)
    set(SIMD_CXX_FLAGS -mfpu=neon)
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SIMD
                                  REQUIRED_VARS SIMD_C_FLAGS SIMD_CXX_FLAGS)
