# Copyright 2024-2026 keplertech.io
# SPDX-License-Identifier: GPL-3.0-only

# Result formatting and reports are shared without pulling in file loaders.
add_library(kepler_verification_support STATIC RunResult.cpp KeplerFormalUtils.cpp)
target_include_directories(kepler_verification_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kepler_verification_support PUBLIC kepler_config kepler_sec)

if(BUILD_KEPLER_PYTHON)
  # Python borrows designs loaded by NajaEDA. Do not compile the file driver
  # or introduce its parser dependencies into the extension.
  return()
endif()

set(KEPLER_FORMAL_DRIVER_LIBRARIES
    kepler_verification_support
    kepler_config
    kepler_sec
    naja_dnl
    naja_nl_dump
    naja_opt
    naja_snl_liberty
    naja_snl_systemverilog
    naja_snl_verilog
    kissat
    cadical
    formal_strategies
    scope_extraction
    yaml-cpp
    ${SPDLOG_TARGET}
)

function(configure_kepler_formal_driver target)
  target_include_directories(${target} SYSTEM BEFORE PUBLIC ${Boost_INCLUDE_DIR})
  target_include_directories(${target} PUBLIC ${ARGPARSE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${target} PUBLIC ${KEPLER_FORMAL_DRIVER_LIBRARIES})
endfunction()

add_library(kepler_formal_core STATIC KeplerFormal.cpp Btor2ExportConfig.cpp)
configure_kepler_formal_driver(kepler_formal_core)

add_library(kepler_formal_driver STATIC CppDriver.cpp)
target_link_libraries(kepler_formal_driver PUBLIC kepler_formal_core naja_snl_pyloader)

add_executable(kepler-formal KeplerFormalMain.cpp)
target_link_libraries(kepler-formal PRIVATE kepler_formal_driver)

add_dependencies(kepler-formal naja)
add_custom_command(TARGET kepler-formal POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:naja>
    $<TARGET_FILE_DIR:kepler-formal>
)

install(TARGETS kepler-formal DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES $<TARGET_FILE:naja> TYPE BIN)
