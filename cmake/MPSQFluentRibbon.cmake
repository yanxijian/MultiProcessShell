# Resolve QFluentRibbon for Demo Client ribbon pages only.
# Not used by mps_* libraries. Default: find_package. Embed only when MPS_DEV_EMBED_QFR=ON.

set(MPS_QFR_SOURCE_DIR "" CACHE PATH "Path to QFluentRibbon sources when MPS_DEV_EMBED_QFR=ON")

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

if(NOT _mps_have_qfr AND MPS_DEV_EMBED_QFR)
  if(MPS_QFR_SOURCE_DIR STREQUAL "" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../QFluentRibbon/CMakeLists.txt")
    set(MPS_QFR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../QFluentRibbon")
  endif()
  if(MPS_QFR_SOURCE_DIR AND EXISTS "${MPS_QFR_SOURCE_DIR}/CMakeLists.txt")
    set(QFR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(QFR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(QFR_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${MPS_QFR_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/qfr" EXCLUDE_FROM_ALL)
    set(_mps_have_qfr TRUE)
    set(MPS_QFR_VIA_SOURCE TRUE)
    message(STATUS "MultiProcessShell demos: DEV embed QFluentRibbon from ${MPS_QFR_SOURCE_DIR}")
  endif()
endif()

if(NOT _mps_have_qfr)
  message(FATAL_ERROR
    "QFluentRibbon not found (required for Demo Client ribbon pages).\n"
    "  Install QFR and pass -DCMAKE_PREFIX_PATH=<prefix>;<qt>\n"
    "  Or: -DMPS_DEV_EMBED_QFR=ON [-DMPS_QFR_SOURCE_DIR=...]")
endif()

if(TARGET QFluentRibbon::ribbon)
  set(MPS_QFR_TARGET QFluentRibbon::ribbon)
else()
  set(MPS_QFR_TARGET qfr_ribbon)
endif()
