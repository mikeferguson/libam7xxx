cmake_minimum_required(VERSION 2.6)
project(libam7xxx C)

set(PROJECT_DESCRIPTION
  "Communication library for Actions Micro AM7XXX based USB projectors and DPFs")

set(PROJECT_VER_MAJOR 0)
set(PROJECT_VER_MINOR 1)
set(PROJECT_VER_PATCH 3)
set(PROJECT_VER_EXTRA "")
set(PROJECT_VER
  "${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}.${PROJECT_VER_PATCH}${PROJECT_VER_EXTRA}")
set(PROJECT_APIVER
  "${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}")

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

OPTION(STRICT_COMPILATION_CHECKS "Enable stricter compilation checks" OFF)

include (MaintenanceTools)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
set(DOC_OUTPUT_PATH ${CMAKE_BINARY_DIR}/doc)

# Because cmake cannot deal sanely with multiline strings. SRSLY?
# See http://www.vtkedge.org/Bug/view.php?id=8362&nbn=8
macro(add_flags var)
  string(REPLACE ";" " " _flags "${ARGN}")
  set(${var} "${${var}} ${_flags}")
endmacro(add_flags)

string(REGEX MATCH "clang" CMAKE_COMPILER_IS_CLANG "${CMAKE_C_COMPILER}")

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
  add_definitions(-Wall)

  # let CFLAGS env override this
  if(CMAKE_C_FLAGS STREQUAL "")
    set(CMAKE_C_FLAGS "-std=c99 -pedantic -Wall -Wextra")
  endif()

  add_flags(CMAKE_C_FLAGS
    -fvisibility=hidden
    -fno-common
    -Wall
    -Wextra
    -Wformat=2
    -Winit-self
    -Winline
    -Wpacked
    -Wpointer-arith
    -Wlarger-than-65500
    -Wmissing-declarations
    -Wmissing-format-attribute
    -Wmissing-noreturn
    -Wmissing-prototypes
    -Wnested-externs
    -Wold-style-definition
    -Wredundant-decls
    -Wsign-compare
    -Wstrict-aliasing=2
    -Wstrict-prototypes
    -Wswitch-enum
    -Wundef
    -Wunreachable-code
    -Wwrite-strings
    -fstack-protector)

  add_flags(DEBUG_FLAGS
    -ggdb
    -DDEBUG=1)

  add_flags(RELEASE_FLAGS
    -Wp,-D_FORTIFY_SOURCE=2)

  if (STRICT_COMPILATION_CHECKS)
    add_flags(STRICT_FLAGS
      -Werror
      # sign conversion warnings can be very noisy for a very little gain
      #-Wsign-conversion
      # NOTE: Vanilla libusb-1.0.8 can't live with -pedantic-errors
      -pedantic-errors)

  endif()
endif()

if (CMAKE_COMPILER_IS_GNUCC)
  add_flags(CMAKE_C_FLAGS
    -Wunsafe-loop-optimizations
    --param=ssp-buffer-size=4)

  if (STRICT_COMPILATION_CHECKS)
    add_flags(STRICT_FLAGS
      # NOTE: GCC >= 4.6 is needed for -Wunused-but-set-variable
      -Wunused-but-set-variable)
  endif()
endif()

if (CMAKE_COMPILER_IS_CLANG)
  if (STRICT_COMPILATION_CHECKS)
    add_flags(STRICT_FLAGS
      -Wshorten-64-to-32)
  endif()
endif()

set(CMAKE_C_FLAGS_DEBUG "-O0 ${DEBUG_FLAGS} ${STRICT_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE "-O2 ${RELEASE_FLAGS} ${STRICT_FLAGS}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 ${RELEASE_FLAGS} ${DEBUG_FLAGS} ${STRICT_FLAGS}")

# Add library project
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(doc)
