SET(CMAKE_SYSTEM_NAME Windows)
include(CMakeForceCompiler)
IF("${GNU_HOST}" STREQUAL "")
    SET(GNU_HOST i586-mingw32msvc)
ENDIF()
# Prefix detection only works with compiler id "GNU"
CMAKE_FORCE_C_COMPILER(${GNU_HOST}-gcc GNU)
# CMake doesn't automatically look for prefixed 'windres', do it manually:
SET(CMAKE_RC_COMPILER ${GNU_HOST}-windres)

# The following is important to let cmake set some variables such as
# CMAKE_FIND_LIBRARY_PREFIXES and CMAKE_FIND_LIBRARY_SUFFIXES from
# Platform/Windows-GNU.cmake
SET(MINGW True)
