# Resolve QThemeEngine for Demo Host (+ optional Demo Client styling).
# Not used by mps_* libraries. Default: find_package. Embed only when MPS_DEV_EMBED_QTE=ON.

set(MPS_QTE_SOURCE_DIR "" CACHE PATH "Path to QThemeEngine sources when MPS_DEV_EMBED_QTE=ON")

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

if(NOT _mps_have_qte AND MPS_DEV_EMBED_QTE)
  if(MPS_QTE_SOURCE_DIR STREQUAL "" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../QThemeEngine/CMakeLists.txt")
    set(MPS_QTE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../QThemeEngine")
  endif()
  if(MPS_QTE_SOURCE_DIR AND EXISTS "${MPS_QTE_SOURCE_DIR}/CMakeLists.txt")
    set(QTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(QTE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(QTE_BUILD_WIDGETS OFF CACHE BOOL "" FORCE)
    set(QTE_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${MPS_QTE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/qthemeengine" EXCLUDE_FROM_ALL)
    set(_mps_have_qte TRUE)
    set(MPS_QTE_VIA_SOURCE TRUE)
    message(STATUS "MultiProcessShell demos: DEV embed QThemeEngine from ${MPS_QTE_SOURCE_DIR}")
  endif()
endif()

if(NOT _mps_have_qte)
  message(FATAL_ERROR
    "QThemeEngine not found (required for Demo Host theming).\n"
    "  Install QTE and pass -DCMAKE_PREFIX_PATH=<prefix>;<qt>\n"
    "  Or: -DMPS_DEV_EMBED_QTE=ON [-DMPS_QTE_SOURCE_DIR=...]")
endif()

if(TARGET QThemeEngine::engine)
  set(MPS_QTE_TARGET QThemeEngine::engine)
else()
  set(MPS_QTE_TARGET qte_engine)
endif()
