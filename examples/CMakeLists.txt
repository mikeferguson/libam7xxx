add_definitions("-D_POSIX_C_SOURCE=2") # for getopt()

include_directories(${CMAKE_SOURCE_DIR}/src/)

# Build a test app that sends a single picture
add_executable(picoproj picoproj.c)
target_link_libraries(picoproj am7xxx)
install(TARGETS picoproj
  DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
