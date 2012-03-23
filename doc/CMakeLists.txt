# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file("Doxyfile.in" "Doxyfile" @ONLY IMMEDIATE)

  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
  )

  install(DIRECTORY ${DOC_OUTPUT_PATH}/html
    DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/${CMAKE_PROJECT_NAME}")

endif(DOXYGEN_FOUND)

add_subdirectory(man)
