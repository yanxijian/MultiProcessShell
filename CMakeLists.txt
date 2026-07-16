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
option(MPS_BUILD_DEMOS "Build demos" OFF)
option(MPS_BUILD_SRC "Build Host/Client application targets" OFF)
option(MPS_FETCH_PROTOBUF "Fetch protobuf via FetchContent" ON)
option(MPS_REQUIRE_QT "Require Qt (needed for Host/Client/Demo)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(MPSQt)
include(MPSProtobuf)

if(MPS_REQUIRE_QT OR MPS_BUILD_SRC OR MPS_BUILD_DEMOS)
  mps_find_qt()
else()
  set(MPS_QT_VERSION "not-required")
endif()

if(MPS_BUILD_TESTS OR MPS_BUILD_SRC OR MPS_BUILD_DEMOS)
  set(MPS_FETCH_PROTOBUF ON CACHE BOOL "" FORCE)
endif()

if(MPS_FETCH_PROTOBUF)
  mps_fetch_protobuf()
endif()

# M0: framing + generated ipc proto library
add_subdirectory(src/common)

if(MPS_BUILD_TESTS)
  enable_testing()
  mps_fetch_googletest()
  add_subdirectory(tests)
endif()

if(MPS_BUILD_SRC)
  message(STATUS "MPS_BUILD_SRC=ON (Host/Client app targets not added yet)")
endif()

if(MPS_BUILD_DEMOS)
  message(STATUS "MPS_BUILD_DEMOS=ON (demo targets not added yet)")
endif()

message(STATUS "MultiProcessShell ${PROJECT_VERSION}")
message(STATUS "  Qt: ${MPS_QT_VERSION}")
message(STATUS "  MPS_BUILD_SRC=${MPS_BUILD_SRC} MPS_BUILD_DEMOS=${MPS_BUILD_DEMOS} MPS_BUILD_TESTS=${MPS_BUILD_TESTS}")
