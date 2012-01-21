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
    -Werror
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
    )
endif()

set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG=1")
set(CMAKE_C_FLAGS_RELEASE "-O2")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")

# Use git for some maintainance tasks
find_package(Git)
if(GIT_FOUND)
  set(ARCHIVE_PREFIX ${CMAKE_PROJECT_NAME}-${PROJECT_VER})
  find_program(DATE_EXECUTABLE date DOC "date command line program")
  if (DATE_EXECUTABLE)
    message(STATUS "Found date: " ${DATE_EXECUTABLE})
    message(STATUS "Generator is: " ${CMAKE_GENERATOR})

    # XXX: using $(shell CMD) works only with Unix Makefile
    if (CMAKE_GENERATOR STREQUAL "Unix Makefiles")
      message(STATUS " - \"git archive\" will use the date too!")
      set(ARCHIVE_PREFIX ${ARCHIVE_PREFIX}-$\(shell ${DATE_EXECUTABLE} +%Y%m%d%H%M\))
    endif()
  endif()
  add_custom_target(archive
    COMMAND ${GIT_EXECUTABLE} archive -o \"${ARCHIVE_PREFIX}.tar.gz\" --prefix=\"${ARCHIVE_PREFIX}/\" HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  add_custom_target(changelog
    COMMAND ${GIT_EXECUTABLE} log --pretty=\"format:%ai  %aN  <%aE>%n%n%x09* %s%d%n\" > ChangeLog
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif(GIT_FOUND)

# Add library project
add_subdirectory(src)
