# Copyright 2024-2025 keplertech.io
# SPDX-License-Identifier: GPL-3.0-only

include(GoogleTest)

add_test(NAME VersionCliTest
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/VersionCliTest.sh"
    $<TARGET_FILE:kepler-formal>)

add_subdirectory(unit_designs)
add_subdirectory(strategies)
add_subdirectory(utils)
add_subdirectory(scope)
add_subdirectory(sec)
