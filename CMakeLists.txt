cmake_minimum_required(VERSION 3.21)

project(MultiProcessShell
  VERSION 0.1.0
  DESCRIPTION "Multi-process shell with native window embedding and Protobuf IPC"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # Match Qt / shared CRT; protobuf must not force /MT.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

option(MPS_BUILD_TESTS "Build protocol/unit tests (M0)" ON)
option(MPS_BUILD_DEMOS "Build browser-style detachable-tab Demo" ON)
option(MPS_BUILD_SRC "Build Host/Client libraries used by demos" ON)
option(MPS_FETCH_PROTOBUF "Fetch protobuf via FetchContent" ON)
option(MPS_REQUIRE_QT "Require Qt (needed for Host/Client/Demo)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MPSProtobuf)

if(MPS_REQUIRE_QT OR MPS_BUILD_SRC OR MPS_BUILD_DEMOS)
  set(MPS_REQUIRE_QT ON)

  # Prefer QTDIR when set. On Windows use a 64-bit CMake kit: Qt *64 packages set
  # PACKAGE_VERSION_UNSUITABLE if CMAKE_SIZEOF_VOID_P is 4 (often mis-read as a version mismatch).
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
  include(MPSQThemeEngine)
  add_subdirectory(src/host)
  include(MPSQFluentRibbon)
  add_subdirectory(src/client)
endif()

if(MPS_BUILD_DEMOS)
  add_subdirectory(demos)
endif()

if(MPS_BUILD_TESTS)
  enable_testing()
  mps_fetch_googletest()
  add_subdirectory(tests)
endif()

message(STATUS "MultiProcessShell ${PROJECT_VERSION}")
message(STATUS "  Qt: ${MPS_QT_VERSION}")
message(STATUS "  MPS_BUILD_SRC=${MPS_BUILD_SRC} MPS_BUILD_DEMOS=${MPS_BUILD_DEMOS} MPS_BUILD_TESTS=${MPS_BUILD_TESTS}")
