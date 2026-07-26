# Resolve sibling QFluentRibbon for Demo Client ribbon pages.
# QThemeEngine must already be resolved via MPSQThemeEngine.cmake (shared build tree).
# Override with -DMPS_QFR_SOURCE_DIR=... or install packages on CMAKE_PREFIX_PATH.

set(MPS_QFR_SOURCE_DIR "" CACHE PATH "Path to QFluentRibbon sources (optional; default ../QFluentRibbon)")
if(MPS_QFR_SOURCE_DIR STREQUAL "" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../QFluentRibbon/CMakeLists.txt")
  set(MPS_QFR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../QFluentRibbon")
endif()

set(_mps_have_qfr FALSE)
if(TARGET QFluentRibbon::qfluentribbon OR TARGET qfluentribbon)
  set(_mps_have_qfr TRUE)
elseif(MPS_QFR_SOURCE_DIR AND EXISTS "${MPS_QFR_SOURCE_DIR}/CMakeLists.txt")
  set(QFR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(QFR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(QFR_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory("${MPS_QFR_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/qfluentribbon" EXCLUDE_FROM_ALL)
  set(_mps_have_qfr TRUE)
  message(STATUS "MultiProcessShell: using QFluentRibbon from ${MPS_QFR_SOURCE_DIR}")
else()
  find_package(QFluentRibbon CONFIG QUIET)
  if(TARGET QFluentRibbon::qfluentribbon)
    set(_mps_have_qfr TRUE)
    message(STATUS "MultiProcessShell: using installed QFluentRibbon::qfluentribbon")
  endif()
endif()

if(NOT _mps_have_qfr)
  message(FATAL_ERROR
    "QFluentRibbon not found (required for Demo Client ribbon pages).\n"
    "  - Place QFluentRibbon next to this repo (../QFluentRibbon), with QThemeEngine beside it\n"
    "  - Pass -DMPS_QFR_SOURCE_DIR=/path/to/QFluentRibbon\n"
    "  - Or install QFR/QTE and pass -DCMAKE_PREFIX_PATH=...")
endif()

if(TARGET QFluentRibbon::qfluentribbon)
  set(MPS_QFR_TARGET QFluentRibbon::qfluentribbon)
else()
  set(MPS_QFR_TARGET qfluentribbon)
endif()
