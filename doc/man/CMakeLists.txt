# add a target to generate man pages with asciidoc
find_package(Asciidoc)
if(ASCIIDOC_FOUND)
  add_custom_target(manpages
    ${ASCIIDOC_A2X_EXECUTABLE} -f manpage ${CMAKE_CURRENT_SOURCE_DIR}/am7xxx-play.1.txt -D ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${ASCIIDOC_A2X_EXECUTABLE} -f manpage ${CMAKE_CURRENT_SOURCE_DIR}/picoproj.1.txt -D ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating man pages with Asciidoc" VERBATIM
  )

  add_custom_target(install-manpages
    ${CMAKE_COMMAND} -E make_directory
      "${CMAKE_INSTALL_PREFIX}/share/man/man1/"
    COMMAND ${CMAKE_COMMAND} -E copy
      ${DOC_OUTPUT_PATH}/man/am7xxx-play.1
      "${CMAKE_INSTALL_PREFIX}/share/man/man1/"
    COMMAND ${CMAKE_COMMAND} -E copy
      ${DOC_OUTPUT_PATH}/man/picoproj.1
      "${CMAKE_INSTALL_PREFIX}/share/man/man1/"
  )
  add_dependencies(install-manpages manpages)

  install(CODE "execute_process(COMMAND ${CMAKE_BUILD_TOOL} install-manpages)")

endif(ASCIIDOC_FOUND)
