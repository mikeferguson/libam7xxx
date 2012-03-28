# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file("Doxyfile.in" "Doxyfile" @ONLY IMMEDIATE)

  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
  )

  add_custom_target(install-doc
    ${CMAKE_COMMAND} -E copy_directory
    ${DOC_OUTPUT_PATH}/html
    "${CMAKE_INSTALL_PREFIX}/share/doc/${CMAKE_PROJECT_NAME}/html"
  )
  add_dependencies(install-doc doc)

  install(CODE "execute_process(COMMAND ${CMAKE_BUILD_TOOL} install-doc)")

endif(DOXYGEN_FOUND)

add_subdirectory(man)
