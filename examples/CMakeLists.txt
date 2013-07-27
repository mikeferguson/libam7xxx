include(CheckSymbolExists)
add_definitions("-D_POSIX_C_SOURCE=2") # for getopt()
add_definitions("-D_POSIX_SOURCE") # for sigaction
add_definitions("-D_BSD_SOURCE") # for strdup

include_directories(${CMAKE_SOURCE_DIR}/src/)

# Build a test app that sends a single picture
option(BUILD_PICOPROJ "Build a test app that sends a single picture" TRUE)
if(BUILD_PICOPROJ)
  add_executable(picoproj picoproj.c)
  target_link_libraries(picoproj am7xxx)
  install(TARGETS picoproj
    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

# Build a more complete example
option(BUILD_AM7XXX-PLAY "Build a more complete example: am7xxx-play" TRUE)
if(BUILD_AM7XXX-PLAY)
  find_package(FFmpeg REQUIRED)
  set(CMAKE_REQUIRED_LIBRARIES ${FFMPEG_LIBRARIES}) 
  set(CMAKE_REQUIRED_INCLUDES ${FFMPEG_LIBAVFORMAT_INCLUDE_DIRS}) 
  check_symbol_exists(avformat_open_input
    "${FFMPEG_LIBAVFORMAT_INCLUDE_DIRS}/libavformat/avformat.h"
    HAVE_AVFORMAT_OPEN_INPUT)
  if(NOT HAVE_AVFORMAT_OPEN_INPUT)
    message(FATAL_ERROR
      "Function avformat_open_input missing. Please use a newer FFmpeg release.")
  endif()

  include_directories(${FFMPEG_LIBAVDEVICE_INCLUDE_DIRS})
  include_directories(${FFMPEG_LIBAVFORMAT_INCLUDE_DIRS})
  include_directories(${FFMPEG_LIBSWSCALE_INCLUDE_DIRS})

  set(CMAKE_REQUIRED_DEFINITIONS -D_POSIX_SOURCE)
  check_symbol_exists(sigaction "signal.h" HAVE_SIGACTION)
  if (HAVE_SIGACTION)
    add_definitions("-DHAVE_SIGACTION")
  endif()

  check_symbol_exists(strtok_r "string.h" HAVE_STRTOK_R)
  if (HAVE_STRTOK_R)
    add_definitions("-DHAVE_STRTOK_R")
  endif()
  set(CMAKE_REQUIRED_DEFINITIONS)

  # xcb is used to retrieve the full screen dimensions when using x11grab
  # as input format
  find_package(XCB)
  if (XCB_FOUND)
    add_definitions("${LIBXCB_DEFINITIONS} -DHAVE_XCB")
    include_directories(${LIBXCB_INCLUDE_DIRS})
  endif()

  add_executable(am7xxx-play am7xxx-play.c)

  target_link_libraries(am7xxx-play am7xxx
    ${FFMPEG_LIBRARIES}
    ${FFMPEG_LIBSWSCALE_LIBRARIES}
    ${LIBXCB_LIBRARIES})
  install(TARGETS am7xxx-play
    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

# Build a simple usb-modeswitch clone for am7xxx devices
option(BUILD_am7xxx-modeswitch "Build a simple usbmode-switch clone for am7xxx devices" TRUE)
if(BUILD_am7xxx-modeswitch)

  find_package(libusb-1.0 REQUIRED)
  include_directories(${LIBUSB_1_INCLUDE_DIRS})

  add_executable(am7xxx-modeswitch am7xxx-modeswitch.c)
  target_link_libraries(am7xxx-modeswitch ${LIBUSB_1_LIBRARIES})
  install(TARGETS am7xxx-modeswitch
    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

