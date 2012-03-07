# Use git for some maintenance tasks
find_package(Git)
if(GIT_FOUND)

  # Add an 'archive' target to generate a compressed archive from the git source code
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
    COMMAND ${GIT_EXECUTABLE} archive -o \"${CMAKE_BINARY_DIR}/${ARCHIVE_PREFIX}.tar.gz\" --prefix=\"${ARCHIVE_PREFIX}/\" HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

  # Add a 'changelog' target to generate a qausi-GNU-style changelog, it may
  # be used by distributors to ship when building their packages.
  add_custom_target(changelog
    COMMAND ${GIT_EXECUTABLE} log --pretty=\"format:%ai  %aN  <%aE>%n%n%x09* %s%d%n\" > \"${CMAKE_BINARY_DIR}/ChangeLog\"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif(GIT_FOUND)
