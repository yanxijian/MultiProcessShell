cmake_minimum_required(VERSION 3.21)

project(MultiProcessShell
  VERSION 0.1.0
  DESCRIPTION "Multi-process shell with native window embedding and Protobuf IPC"
  LANGUAGES CXX
)

# Stable when this project is add_subdirectory()'d by a consumer (e.g. Volition).
set(MPS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # Match Qt / shared CRT; protobuf must not force /MT.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

option(MPS_BUILD_SHARED "Build mps_* libraries as shared" ON)
option(MPS_BUILD_TESTS "Build protocol/unit tests (M0)" ON)
option(MPS_BUILD_DEMOS "Build browser-style detachable-tab Demo" ON)
option(MPS_BUILD_SRC "Build Host/Client libraries used by demos" ON)
option(MPS_FETCH_PROTOBUF "Fetch protobuf via FetchContent" ON)
option(MPS_REQUIRE_QT "Require Qt (needed for Host/Client/Demo)" OFF)
option(MPS_DEV_EMBED_QTE "Demo only: embed sibling QThemeEngine via add_subdirectory" OFF)
option(MPS_DEV_EMBED_QFR "Demo only: embed sibling QFluentRibbon via add_subdirectory" OFF)
option(MPS_INSTALL "Install libraries, headers, and CMake package" ON)

include(GNUInstallDirs)
include(GenerateExportHeader)

set(MPS_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${MPS_GENERATED_INCLUDE_DIR}/mps")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MPSProtobuf)

if(MPS_REQUIRE_QT OR MPS_BUILD_SRC OR MPS_BUILD_DEMOS)
  set(MPS_REQUIRE_QT ON)

  if(DEFINED ENV{QTDIR} AND NOT "$ENV{QTDIR}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{QTDIR}" _mps_qtdir)
    set(CMAKE_PREFIX_PATH "${_mps_qtdir};${CMAKE_PREFIX_PATH}")
  endif()

  if(WIN32 AND DEFINED CMAKE_SIZEOF_VOID_P AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
      "MultiProcessShell needs a 64-bit configure (CMAKE_SIZEOF_VOID_P=8), but this one is 32-bit.\n"
      "Pick an x64 CMake kit / preset \"windows-x64\", run vcvars64, then reconfigure.\n"
      "QTDIR=$ENV{QTDIR}")
  endif()

  find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
  if(Qt6_VERSION VERSION_LESS "6.8")
    message(FATAL_ERROR "Qt 6.8+ required, found ${Qt6_VERSION}")
  endif()

  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)
  set(CMAKE_AUTOUIC ON)
  set(MPS_QT_VERSION "${Qt6_VERSION}")
  message(STATUS "Found Qt ${Qt6_VERSION}")
else()
  set(MPS_QT_VERSION "not-required")
endif()

if(MPS_BUILD_TESTS OR MPS_BUILD_SRC OR MPS_BUILD_DEMOS)
  set(MPS_FETCH_PROTOBUF ON CACHE BOOL "" FORCE)
endif()

if(MPS_FETCH_PROTOBUF)
  mps_fetch_protobuf()
endif()

add_subdirectory(src/common)

if(MPS_BUILD_SRC OR MPS_BUILD_DEMOS)
  add_subdirectory(src/ipc_qt)
  add_subdirectory(src/host)
  add_subdirectory(src/client)
endif()

if(MPS_BUILD_DEMOS)
  # QTE/QFR are Demo composition only — never linked into mps_* libraries.
  include(MPSQThemeEngine)
  include(MPSQFluentRibbon)
  add_subdirectory(demos)
endif()

if(MPS_BUILD_TESTS)
  enable_testing()
  mps_fetch_googletest()
  add_subdirectory(tests)
endif()

file(WRITE "${MPS_GENERATED_INCLUDE_DIR}/mps/mps_export.hpp"
"#pragma once
#include <mps/mps_ipc_export.hpp>
#if defined(__has_include)
#  if __has_include(<mps/mps_ipc_qt_export.hpp>)
#    include <mps/mps_ipc_qt_export.hpp>
#  endif
#  if __has_include(<mps/mps_host_export.hpp>)
#    include <mps/mps_host_export.hpp>
#  endif
#  if __has_include(<mps/mps_client_export.hpp>)
#    include <mps/mps_client_export.hpp>
#  endif
#endif
")

if(MPS_INSTALL)
  if(MPS_QTE_VIA_SOURCE OR MPS_QFR_VIA_SOURCE)
    message(STATUS
      "MultiProcessShell: MPS_INSTALL disabled (incompatible with QTE/QFR add_subdirectory embed)")
    set(MPS_INSTALL OFF CACHE BOOL "Install libraries, headers, and CMake package" FORCE)
  endif()
endif()

if(MPS_INSTALL)
  include(CMakePackageConfigHelpers)

  set(_mps_install_targets mps_ipc mps_tab_strip)
  if(TARGET mps_ipc_qt)
    list(APPEND _mps_install_targets mps_ipc_qt mps_host mps_client)
  endif()

  install(TARGETS ${_mps_install_targets}
    EXPORT MultiProcessShellTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/common/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
  )
  if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/src/common/generated")
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src/common/generated/"
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    )
  endif()
  if(TARGET mps_ipc_qt)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/ipc_qt/"
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      FILES_MATCHING PATTERN "*.hpp"
    )
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/host/"
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      FILES_MATCHING PATTERN "*.hpp"
      PATTERN "embed" EXCLUDE
    )
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/host/embed/win/embed_container.hpp"
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/client/"
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      FILES_MATCHING PATTERN "*.hpp"
    )
    install(DIRECTORY "${MPS_GENERATED_INCLUDE_DIR}/mps"
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      FILES_MATCHING PATTERN "*.hpp"
    )
  endif()

  install(EXPORT MultiProcessShellTargets
    FILE MultiProcessShellTargets.cmake
    NAMESPACE mps::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MultiProcessShell
  )

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MultiProcessShellConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MultiProcessShellConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MultiProcessShell
  )

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MultiProcessShellConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MultiProcessShellConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MultiProcessShellConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MultiProcessShell
  )
endif()

if(NOT DEFINED MPS_PROTOBUF_SHARED)
  set(MPS_PROTOBUF_SHARED FALSE)
endif()

message(STATUS "MultiProcessShell ${PROJECT_VERSION}")
message(STATUS "  Qt: ${MPS_QT_VERSION}")
message(STATUS "  shared=${MPS_BUILD_SHARED} protobuf_shared=${MPS_PROTOBUF_SHARED} install=${MPS_INSTALL} demos_embed_qte=${MPS_DEV_EMBED_QTE} demos_embed_qfr=${MPS_DEV_EMBED_QFR}")
message(STATUS "  MPS_BUILD_SRC=${MPS_BUILD_SRC} MPS_BUILD_DEMOS=${MPS_BUILD_DEMOS} MPS_BUILD_TESTS=${MPS_BUILD_TESTS}")
