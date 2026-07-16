cmake_minimum_required(VERSION 3.21)

project(MultiProcessShell
  VERSION 0.1.0
  DESCRIPTION "Multi-process shell with native window embedding and Protobuf IPC"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MPS_BUILD_TESTS "Build protocol/unit tests" OFF)
option(MPS_BUILD_DEMOS "Build demos (minimum window embed)" OFF)
option(MPS_BUILD_SRC "Build src libraries/executables (Host/Client)" OFF)
option(MPS_FETCH_PROTOBUF "Fetch protobuf via FetchContent" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(MPSQt)
include(MPSProtobuf)

mps_find_qt()
if(MPS_FETCH_PROTOBUF)
  mps_fetch_protobuf()
endif()

# Implementation targets are gated until Demo morphology is finalized.
if(MPS_BUILD_SRC)
  message(WARNING "MPS_BUILD_SRC=ON but src/ has no implementation targets yet.")
endif()

if(MPS_BUILD_DEMOS)
  message(WARNING "MPS_BUILD_DEMOS=ON but demos/ has no implementation targets yet.")
endif()

if(MPS_BUILD_TESTS)
  enable_testing()
  message(WARNING "MPS_BUILD_TESTS=ON but tests/ has no implementation targets yet.")
endif()

message(STATUS "MultiProcessShell ${PROJECT_VERSION}")
message(STATUS "  QTDIR / CMAKE_PREFIX_PATH: Qt ${MPS_QT_VERSION}")
message(STATUS "  MPS_BUILD_SRC=${MPS_BUILD_SRC} MPS_BUILD_DEMOS=${MPS_BUILD_DEMOS} MPS_BUILD_TESTS=${MPS_BUILD_TESTS}")
