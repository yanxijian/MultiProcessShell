# Resolve QThemeEngine for Demo Host (+ optional Demo Client styling).
# Not used by mps_* libraries.
# Order: existing target → find_package → sibling ../QThemeEngine embed (Codes layout).

set(MPS_QTE_SOURCE_DIR "" CACHE PATH "Path to QThemeEngine sources when embedding")

set(_mps_have_qte FALSE)
set(MPS_QTE_VIA_SOURCE FALSE)

if(TARGET QThemeEngine::engine OR TARGET qte_engine)
  set(_mps_have_qte TRUE)
else()
  find_package(QThemeEngine CONFIG QUIET)
  if(TARGET QThemeEngine::engine)
    set(_mps_have_qte TRUE)
    message(STATUS "MultiProcessShell demos: using installed QThemeEngine::engine")
  endif()
endif()

if(NOT _mps_have_qte)
  # Resolve source dir: explicit cache → MPS_ROOT sibling → list-dir sibling.
  set(_mps_qte_src "${MPS_QTE_SOURCE_DIR}")
  if(_mps_qte_src STREQUAL "" OR _mps_qte_src STREQUAL "MPS_QTE_SOURCE_DIR-NOTFOUND")
    set(_mps_qte_src "")
  endif()
  if(_mps_qte_src STREQUAL "" AND DEFINED MPS_ROOT
      AND EXISTS "${MPS_ROOT}/../QThemeEngine/CMakeLists.txt")
    set(_mps_qte_src "${MPS_ROOT}/../QThemeEngine")
  endif()
  if(_mps_qte_src STREQUAL ""
      AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../QThemeEngine/CMakeLists.txt")
    set(_mps_qte_src "${CMAKE_CURRENT_LIST_DIR}/../../QThemeEngine")
  endif()
  if(_mps_qte_src STREQUAL ""
      AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../QThemeEngine/CMakeLists.txt")
    set(_mps_qte_src "${CMAKE_CURRENT_SOURCE_DIR}/../QThemeEngine")
  endif()

  if(NOT _mps_qte_src STREQUAL "" AND EXISTS "${_mps_qte_src}/CMakeLists.txt")
    set(MPS_QTE_SOURCE_DIR "${_mps_qte_src}" CACHE PATH
      "Path to QThemeEngine sources when embedding" FORCE)
    set(MPS_DEV_EMBED_QTE ON CACHE BOOL
      "Demo only: embed sibling QThemeEngine via add_subdirectory" FORCE)
    set(QTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(QTE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(QTE_BUILD_WIDGETS OFF CACHE BOOL "" FORCE)
    set(QTE_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${_mps_qte_src}" "${CMAKE_BINARY_DIR}/_deps/qthemeengine" EXCLUDE_FROM_ALL)
    set(_mps_have_qte TRUE)
    set(MPS_QTE_VIA_SOURCE TRUE)
    message(STATUS "MultiProcessShell demos: embed QThemeEngine from ${_mps_qte_src}")
  endif()
endif()

if(NOT _mps_have_qte)
  message(FATAL_ERROR
    "QThemeEngine not found (required for Demo Host theming).\n"
    "  Install QTE and pass -DCMAKE_PREFIX_PATH=<prefix>;<qt>\n"
    "  Or place sibling ../QThemeEngine next to MultiProcessShell (auto-embed),\n"
    "  or -DMPS_DEV_EMBED_QTE=ON -DMPS_QTE_SOURCE_DIR=<path>")
endif()

if(TARGET QThemeEngine::engine)
  set(MPS_QTE_TARGET QThemeEngine::engine)
else()
  set(MPS_QTE_TARGET qte_engine)
endif()
