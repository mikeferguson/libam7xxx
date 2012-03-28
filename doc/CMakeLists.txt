# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file("Doxyfile.in" "Doxyfile" @ONLY IMMEDIATE)

  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${DOC_OUTPUT_PATH}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
  )

  add_custom_target(install-doc
    COMMAND ${CMAKE_COMMAND} -DCOMPONENT=doc -P ${CMAKE_BINARY_DIR}/cmake_install.cmake
    COMMENT "Installing API documentation")
  add_dependencies(install-doc doc)

  install(CODE "execute_process(COMMAND ${CMAKE_BUILD_TOOL} doc)")

  install(DIRECTORY ${DOC_OUTPUT_PATH}/html
    DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/${CMAKE_PROJECT_NAME}"
    COMPONENT doc)

endif(DOXYGEN_FOUND)

add_subdirectory(man)
