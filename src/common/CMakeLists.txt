# Pure tab-strip rules (no Qt / protobuf) — shared by Host + unit tests.
add_library(mps_tab_strip INTERFACE)
add_library(mps::tab_strip ALIAS mps_tab_strip)
target_include_directories(mps_tab_strip
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# mps_ipc: generated shell.ipc.v1 + length-prefixed framing.

set(MPS_PROTO_FILE "${MPS_ROOT}/proto/shell/ipc/v1/ipc.proto")
set(MPS_PROTO_IMPORT_DIR "${MPS_ROOT}/proto")
set(MPS_PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(MPS_PROTO_GEN_SUBDIR "${MPS_PROTO_GEN_DIR}/shell/ipc/v1")

file(MAKE_DIRECTORY "${MPS_PROTO_GEN_SUBDIR}")

set(MPS_PROTO_GEN_CC "${MPS_PROTO_GEN_SUBDIR}/ipc.pb.cc")
set(MPS_PROTO_GEN_H "${MPS_PROTO_GEN_SUBDIR}/ipc.pb.h")

find_program(MPS_PYTHON_EXECUTABLE NAMES python python3 REQUIRED)

add_custom_command(
  OUTPUT
    "${MPS_PROTO_GEN_CC}"
    "${MPS_PROTO_GEN_H}"
  COMMAND ${CMAKE_COMMAND} -E env
          "PATH=$<TARGET_FILE_DIR:protobuf::libprotobuf>;$ENV{PATH}"
          $<TARGET_FILE:protobuf::protoc>
          --cpp_out=${MPS_PROTO_GEN_DIR}
          -I ${MPS_PROTO_IMPORT_DIR}
          ${MPS_PROTO_FILE}
  COMMAND "${MPS_PYTHON_EXECUTABLE}"
          "${MPS_ROOT}/cmake/patch_pb_dll_exports.py"
          "${MPS_PROTO_GEN_H}"
  DEPENDS
    "${MPS_PROTO_FILE}"
    protobuf::protoc
    protobuf::libprotobuf
    "${MPS_ROOT}/cmake/patch_pb_dll_exports.py"
  COMMENT "Generating shell/ipc/v1/ipc.pb.cc (+ DLL export patch)"
  VERBATIM
)

if(MPS_BUILD_SHARED)
  add_library(mps_ipc SHARED
    frame.cpp
    frame.hpp
    envelope_codec.cpp
    envelope_codec.hpp
    envelope_builder.cpp
    envelope_builder.hpp
    "${MPS_PROTO_GEN_CC}"
    "${MPS_PROTO_GEN_H}"
  )
else()
  add_library(mps_ipc STATIC
    frame.cpp
    frame.hpp
    envelope_codec.cpp
    envelope_codec.hpp
    envelope_builder.cpp
    envelope_builder.hpp
    "${MPS_PROTO_GEN_CC}"
    "${MPS_PROTO_GEN_H}"
  )
endif()

add_library(mps::ipc ALIAS mps_ipc)

target_include_directories(mps_ipc
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${MPS_PROTO_GEN_DIR}>
    $<BUILD_INTERFACE:${MPS_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# protobuf is FetchContent'd and not part of the install export; keep it
# BUILD_INTERFACE-only on PUBLIC so install(EXPORT) succeeds.
# Shared protobuf: PUBLIC so all consumers share one libprotobuf.dll and may
# construct/parse/destroy Envelope across mps_* DLL boundaries.
target_link_libraries(mps_ipc
  PUBLIC
    $<BUILD_INTERFACE:protobuf::libprotobuf>
)

set_target_properties(mps_ipc PROPERTIES
  OUTPUT_NAME mps_ipc
  EXPORT_NAME ipc
)
if(MPS_BUILD_SHARED AND WIN32)
  # MSVC: export all object symbols (incl. generated pb) from the DLL.
  set_target_properties(mps_ipc PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
  )
  target_compile_definitions(mps_ipc PUBLIC MPS_IPC_SHARED)
elseif(MPS_BUILD_SHARED)
  # ELF: keep default visibility so pb + framing symbols stay linkable for tests/consumers.
  # (Hidden + selective MPS_IPC_EXPORT is insufficient for generated protobuf code.)
  target_compile_definitions(mps_ipc PUBLIC MPS_IPC_SHARED)
endif()

generate_export_header(mps_ipc
  BASE_NAME MPS_IPC
  EXPORT_MACRO_NAME MPS_IPC_EXPORT
  EXPORT_FILE_NAME "${MPS_GENERATED_INCLUDE_DIR}/mps/mps_ipc_export.hpp"
)

if(MSVC)
  target_compile_options(mps_ipc PRIVATE /bigobj /W4)
else()
  target_compile_options(mps_ipc PRIVATE -Wall -Wextra)
endif()
