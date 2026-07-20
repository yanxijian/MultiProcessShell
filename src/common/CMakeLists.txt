# Pure tab-strip rules (no Qt / protobuf) — shared by Host + unit tests.
add_library(mps_tab_strip INTERFACE)
add_library(mps::tab_strip ALIAS mps_tab_strip)
target_include_directories(mps_tab_strip INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

# mps_ipc: generated shell.ipc.v1 + length-prefixed framing.

set(MPS_PROTO_FILE "${CMAKE_SOURCE_DIR}/proto/shell/ipc/v1/ipc.proto")
set(MPS_PROTO_IMPORT_DIR "${CMAKE_SOURCE_DIR}/proto")
set(MPS_PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(MPS_PROTO_GEN_SUBDIR "${MPS_PROTO_GEN_DIR}/shell/ipc/v1")

file(MAKE_DIRECTORY "${MPS_PROTO_GEN_SUBDIR}")

set(MPS_PROTO_GEN_CC "${MPS_PROTO_GEN_SUBDIR}/ipc.pb.cc")
set(MPS_PROTO_GEN_H "${MPS_PROTO_GEN_SUBDIR}/ipc.pb.h")

add_custom_command(
  OUTPUT
    "${MPS_PROTO_GEN_CC}"
    "${MPS_PROTO_GEN_H}"
  COMMAND protobuf::protoc
          --cpp_out=${MPS_PROTO_GEN_DIR}
          -I ${MPS_PROTO_IMPORT_DIR}
          ${MPS_PROTO_FILE}
  DEPENDS
    "${MPS_PROTO_FILE}"
    protobuf::protoc
  COMMENT "Generating shell/ipc/v1/ipc.pb.cc"
  VERBATIM
)

add_library(mps_ipc STATIC
  frame.cpp
  frame.hpp
  "${MPS_PROTO_GEN_CC}"
  "${MPS_PROTO_GEN_H}"
)

add_library(mps::ipc ALIAS mps_ipc)

target_include_directories(mps_ipc
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${MPS_PROTO_GEN_DIR}"
)

target_link_libraries(mps_ipc
  PUBLIC
    protobuf::libprotobuf
)

if(MSVC)
  target_compile_options(mps_ipc PRIVATE /bigobj /W4)
else()
  target_compile_options(mps_ipc PRIVATE -Wall -Wextra)
endif()
