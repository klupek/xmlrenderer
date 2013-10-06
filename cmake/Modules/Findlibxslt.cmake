# find libxml++
#
# exports:
#
#   LibXSLT_FOUND
#   LibXSLT_INCLUDE_DIRS
#   LibXSLT_LIBRARIES
#

include(FindPkgConfig)
include(FindPackageHandleStandardArgs)

# Use pkg-config to get hints about paths
pkg_check_modules(LibXSLT_PKGCONF REQUIRED libxslt)

# Include dir
find_path(LibXSLT_INCLUDE_DIR
  NAMES libxslt/xslt.h
  PATHS ${LibXSLT_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(LibXSLT_LIBRARY
  NAMES xslt xslt
  PATHS ${LibXSLT_PKGCONF_LIBRARY_DIRS}
)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibXSLT DEFAULT_MSG LibXSLT_LIBRARY LibXSLT_INCLUDE_DIR)


if(LibXSLT_PKGCONF_FOUND)
  set(LibXSLT_LIBRARIES ${LibXSLT_LIBRARY} ${LibXSLT_PKGCONF_LIBRARIES})
  set(LibXSLT_INCLUDE_DIRS ${LibXSLT_INCLUDE_DIR} ${LibXSLT_PKGCONF_INCLUDE_DIRS})
  set(LibXSLT_FOUND yes)
else()
  set(LibXSLT_LIBRARIES)
  set(LibXSLT_INCLUDE_DIRS)
  set(LibXSLT_FOUND no)
endif()

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
#set(LibXSLT_PROCESS_INCLUDES LibXSLT_INCLUDE_DIR)
#set(LibXSLT_PROCESS_LIBS LibXSLT_LIBRARY)
