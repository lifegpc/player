set(PLAYER_VERSION_MAJOR 0)
set(PLAYER_VERSION_MINOR 0)
set(PLAYER_VERSION_MICRO 1)
set(PLAYER_VERSION_REV 0)
set(PLAYER_VERSION ${PLAYER_VERSION_MAJOR}.${PLAYER_VERSION_MINOR}.${PLAYER_VERSION_MICRO}.${PLAYER_VERSION_REV})
message(STATUS "Generate \"${CMAKE_CURRENT_BINARY_DIR}/player_version.h\"")
configure_file("${CMAKE_CURRENT_LIST_DIR}/player_version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/player_version.h")
message(STATUS "Generate \"${CMAKE_CURRENT_BINARY_DIR}/player.rc\"")
configure_file("${CMAKE_CURRENT_LIST_DIR}/player.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/player.rc")
