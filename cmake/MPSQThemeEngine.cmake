# Resolve QThemeEngine for Demo Host (+ shared by QFR when already loaded).
# Override with -DMPS_QTE_SOURCE_DIR=... or install on CMAKE_PREFIX_PATH.

set(MPS_QTE_SOURCE_DIR "" CACHE PATH "Path to QThemeEngine sources (optional; default ../QThemeEngine)")
if(MPS_QTE_SOURCE_DIR STREQUAL "" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../QThemeEngine/CMakeLists.txt")
  set(MPS_QTE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../QThemeEngine")
endif()

set(_mps_have_qte FALSE)
if(TARGET QThemeEngine::engine OR TARGET qtheme_engine)
  set(_mps_have_qte TRUE)
elseif(MPS_QTE_SOURCE_DIR AND EXISTS "${MPS_QTE_SOURCE_DIR}/CMakeLists.txt")
  set(QTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(QTE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(QTE_BUILD_WIDGETS OFF CACHE BOOL "" FORCE)
  set(QTE_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory("${MPS_QTE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/qthemeengine" EXCLUDE_FROM_ALL)
  set(_mps_have_qte TRUE)
  message(STATUS "MultiProcessShell: using QThemeEngine from ${MPS_QTE_SOURCE_DIR}")
else()
  find_package(QThemeEngine CONFIG QUIET)
  if(TARGET QThemeEngine::engine)
    set(_mps_have_qte TRUE)
    message(STATUS "MultiProcessShell: using installed QThemeEngine::engine")
  endif()
endif()

if(NOT _mps_have_qte)
  message(FATAL_ERROR
    "QThemeEngine not found (required for Demo Host/Client theming).\n"
    "  - Place QThemeEngine next to this repo (../QThemeEngine)\n"
    "  - Pass -DMPS_QTE_SOURCE_DIR=/path/to/QThemeEngine\n"
    "  - Or install QTE and pass -DCMAKE_PREFIX_PATH=...")
endif()

if(TARGET QThemeEngine::engine)
  set(MPS_QTE_TARGET QThemeEngine::engine)
else()
  set(MPS_QTE_TARGET qtheme_engine)
endif()
