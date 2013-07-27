# add a target to generate man pages with asciidoc
find_package(Asciidoc)
if(ASCIIDOC_FOUND)
  add_custom_target(manpages
    ${ASCIIDOC_A2X_EXECUTABLE} -f manpage ${CMAKE_CURRENT_SOURCE_DIR}/am7xxx-play.1.txt -D ${DOC_OUTPUT_PATH}/man
    COMMAND ${ASCIIDOC_A2X_EXECUTABLE} -f manpage ${CMAKE_CURRENT_SOURCE_DIR}/am7xxx-modeswitch.1.txt -D ${DOC_OUTPUT_PATH}/man
    COMMAND ${ASCIIDOC_A2X_EXECUTABLE} -f manpage ${CMAKE_CURRENT_SOURCE_DIR}/picoproj.1.txt -D ${DOC_OUTPUT_PATH}/man
    WORKING_DIRECTORY ${DOC_OUTPUT_PATH}/man
    COMMENT "Generating man pages with Asciidoc" VERBATIM
  )

  add_custom_target(install-manpages
    COMMAND ${CMAKE_COMMAND} -DCOMPONENT=manpages -P ${CMAKE_BINARY_DIR}/cmake_install.cmake
    COMMENT "Installing manpages")
  add_dependencies(install-manpages manpages)

  install(CODE "execute_process(COMMAND ${CMAKE_BUILD_TOOL} manpages)")

  install(FILES
    ${DOC_OUTPUT_PATH}/man/am7xxx-play.1
    ${DOC_OUTPUT_PATH}/man/am7xxx-modeswitch.1
    ${DOC_OUTPUT_PATH}/man/picoproj.1
    DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man1/"
    COMPONENT manpages)

endif(ASCIIDOC_FOUND)
