# Resolve QFluentRibbon for Demo Client ribbon content only.
# Not used by mps_* libraries.
# Order: existing target → find_package → sibling ../QFluentRibbon embed (Codes layout).

set(MPS_QFR_SOURCE_DIR "" CACHE PATH "Path to QFluentRibbon sources when embedding")

set(_mps_have_qfr FALSE)
set(MPS_QFR_VIA_SOURCE FALSE)

if(TARGET QFluentRibbon::ribbon OR TARGET qfr_ribbon)
  set(_mps_have_qfr TRUE)
else()
  find_package(QFluentRibbon CONFIG QUIET)
  if(TARGET QFluentRibbon::ribbon)
    set(_mps_have_qfr TRUE)
    message(STATUS "MultiProcessShell demos: using installed QFluentRibbon::ribbon")
  endif()
endif()

if(NOT _mps_have_qfr)
  set(_mps_qfr_src "${MPS_QFR_SOURCE_DIR}")
  if(_mps_qfr_src STREQUAL "" OR _mps_qfr_src STREQUAL "MPS_QFR_SOURCE_DIR-NOTFOUND")
    set(_mps_qfr_src "")
  endif()
  if(_mps_qfr_src STREQUAL "" AND DEFINED MPS_ROOT
      AND EXISTS "${MPS_ROOT}/../QFluentRibbon/CMakeLists.txt")
    set(_mps_qfr_src "${MPS_ROOT}/../QFluentRibbon")
  endif()
  if(_mps_qfr_src STREQUAL ""
      AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../QFluentRibbon/CMakeLists.txt")
    set(_mps_qfr_src "${CMAKE_CURRENT_LIST_DIR}/../../QFluentRibbon")
  endif()
  if(_mps_qfr_src STREQUAL ""
      AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../QFluentRibbon/CMakeLists.txt")
    set(_mps_qfr_src "${CMAKE_CURRENT_SOURCE_DIR}/../QFluentRibbon")
  endif()

  if(NOT _mps_qfr_src STREQUAL "" AND EXISTS "${_mps_qfr_src}/CMakeLists.txt")
    set(MPS_QFR_SOURCE_DIR "${_mps_qfr_src}" CACHE PATH
      "Path to QFluentRibbon sources when embedding" FORCE)
    set(MPS_DEV_EMBED_QFR ON CACHE BOOL
      "Demo only: embed sibling QFluentRibbon via add_subdirectory" FORCE)
    set(QFR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(QFR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(QFR_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${_mps_qfr_src}" "${CMAKE_BINARY_DIR}/_deps/qfr" EXCLUDE_FROM_ALL)
    set(_mps_have_qfr TRUE)
    set(MPS_QFR_VIA_SOURCE TRUE)
    message(STATUS "MultiProcessShell demos: embed QFluentRibbon from ${_mps_qfr_src}")
  endif()
endif()

if(NOT _mps_have_qfr)
  message(FATAL_ERROR
    "QFluentRibbon not found (required for Demo Client ribbon content).\n"
    "  Install QFR and pass -DCMAKE_PREFIX_PATH=<prefix>;<qt>\n"
    "  Or place sibling ../QFluentRibbon next to MultiProcessShell (auto-embed),\n"
    "  or -DMPS_DEV_EMBED_QFR=ON -DMPS_QFR_SOURCE_DIR=<path>")
endif()

if(TARGET QFluentRibbon::ribbon)
  set(MPS_QFR_TARGET QFluentRibbon::ribbon)
else()
  set(MPS_QFR_TARGET qfr_ribbon)
endif()
