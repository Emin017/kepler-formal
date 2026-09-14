# Copyright 2024-2026 keplertech.io
# SPDX-License-Identifier: GPL-3.0-only

set(kepler_version_header "${CMAKE_CURRENT_BINARY_DIR}/KeplerVersion.h")
set(KEPLER_GIT_HASH "unknown" CACHE STRING "Kepler Git hash for source archives")
add_custom_target(kepler_version_header
  COMMAND ${CMAKE_COMMAND}
    "-DKEPLER_GIT_HASH=${KEPLER_GIT_HASH}"
    "-DKEPLER_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DKEPLER_VERSION_HEADER=${kepler_version_header}"
    -P "${PROJECT_SOURCE_DIR}/cmake/KeplerVersion.cmake"
  BYPRODUCTS "${kepler_version_header}"
  VERBATIM
)
add_library(kepler_version INTERFACE)
add_dependencies(kepler_version kepler_version_header)
target_include_directories(kepler_version INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(kepler-formal KeplerFormal.cpp)
target_include_directories(kepler-formal SYSTEM BEFORE PUBLIC ${Boost_INCLUDE_DIR})
target_include_directories(kepler-formal PUBLIC ${ARGPARSE_DIR})

target_link_libraries(kepler-formal
  PRIVATE
    kepler_version
    kepler_config
    kepler_sec
    naja_snl_pyloader
    naja_dnl
    naja_opt
    kissat
    cadical
    formal_strategies
    scope_extraction
    yaml-cpp
    ${SPDLOG_TARGET}
)

add_dependencies(kepler-formal naja)
add_custom_command(TARGET kepler-formal POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:naja>
    $<TARGET_FILE_DIR:kepler-formal>
)

install(TARGETS kepler-formal DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES $<TARGET_FILE:naja> TYPE BIN)
