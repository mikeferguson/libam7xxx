cmake_minimum_required(VERSION 2.6)
project(libam7xxx C)

set(PROJECT_DESCRIPTION
  "Communication library for Actions Micro AM7XXX based USB projectors and DPFs")

set(PROJECT_VER_MAJOR 0)
set(PROJECT_VER_MINOR 0)
set(PROJECT_VER_PATCH 1)
set(PROJECT_VER_EXTRA "")
set(PROJECT_VER
  "${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}.${PROJECT_VER_PATCH}${PROJECT_VER_EXTRA}")
set(PROJECT_APIVER
  "${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}")

set(CMAKE_MODULE_PATH 
  ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

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

if (CMAKE_COMPILER_IS_GNUCC)
  add_definitions(-Wall)

  # let CFLAGS env override this
  if(CMAKE_C_FLAGS STREQUAL "")
    set(CMAKE_C_FLAGS "-std=c99 -pedantic -Wall -Wextra -O2")
  endif()

  # Don't make pedantic checks errors,
  # as vanilla libusb-1.0.8 can't live with that
  #add_flags(CMAKE_C_FLAGS -pedantic-errors)

  # GCC >= 4.6
  #add_flags(CMAKE_C_FLAGS -Wunused-but-set-variable)

  add_flags(CMAKE_C_FLAGS
    -fno-common
    -Wall
    -Wextra
    -Wformat=2
    -Winit-self
    -Winline
    -Wpacked
    -Wp,-D_FORTIFY_SOURCE=2
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
    -Wunsafe-loop-optimizations
    -Wwrite-strings
    -fstack-protector
    --param=ssp-buffer-size=4)
endif()

set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG=1 -Werror")
set(CMAKE_C_FLAGS_RELEASE "-O2")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")

# Add library project
add_subdirectory(src)
add_subdirectory(doc)
