# Distributed under the Boost Software License, Version 1.0. See accompanying
# file LICENSE for details.

#[=======================================================================[.rst:
Findlibdispatch
---------------

Find the libdispatch (aka Grand Central Dispatch (GCD)) library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``libdispatch::libdispatch``
  The libdispatch library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``libdispatch_FOUND``
  True if the system has the libdispatch library.

Use ``libdispatch::libdispatch`` for include and link requirements. This module
does not provide a version variable or plural include/library result variables.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set on non-Apple platforms:

``libdispatch_INCLUDE_DIR``
  The directory containing ``dispatch/dispatch.h``.
``libdispatch_LIBRARY``
  The path to the libdispatch library.

#]=======================================================================]

if(APPLE)
  # libdispatch is included with the Apple compiler so no location on disk
  # is used.
  if(NOT TARGET libdispatch::libdispatch)
    add_library(libdispatch::libdispatch INTERFACE IMPORTED)
  endif()
  set(libdispatch_FOUND TRUE)
else()
  # [DEPENDENCY] https://gitlab.freedesktop.org/pkg-config/pkg-config
  find_package(PkgConfig)
  pkg_check_modules(PC_libdispatch QUIET libdispatch)

  find_path(libdispatch_INCLUDE_DIR
    NAMES dispatch/dispatch.h
    PATHS ${PC_libdispatch_INCLUDE_DIRS}
    PATH_SUFFIXES libdispatch
  )

  find_library(libdispatch_LIBRARY
    NAMES dispatch
    PATHS ${PC_libdispatch_LIBRARY_DIRS}
  )

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(libdispatch
    FOUND_VAR libdispatch_FOUND
    REQUIRED_VARS
      libdispatch_LIBRARY
      libdispatch_INCLUDE_DIR
  )

  if(libdispatch_FOUND AND NOT TARGET libdispatch::libdispatch)
    add_library(libdispatch::libdispatch UNKNOWN IMPORTED)
    set_target_properties(libdispatch::libdispatch PROPERTIES
      IMPORTED_LOCATION "${libdispatch_LIBRARY}"
      INTERFACE_COMPILE_OPTIONS "${PC_libdispatch_CFLAGS_OTHER}"
      INTERFACE_INCLUDE_DIRECTORIES "${libdispatch_INCLUDE_DIR}"
    )
  endif()

  mark_as_advanced(
    libdispatch_INCLUDE_DIR
    libdispatch_LIBRARY
  )
endif()
