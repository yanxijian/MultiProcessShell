# Fetch protobuf with FetchContent (shared when MPS_BUILD_SHARED, tests/examples off).
#
# Optional shared Abseil from AbseilPin (https://github.com/yanxijian/AbseilPin — or sibling
# D:/Codes/AbseilPin/prefix/<pin>): set -DMPS_ABSEIL_PIN_PREFIX=<install-prefix> so protobuf
# finds absl via CMAKE_PREFIX_PATH (find_package CONFIG) and does not FetchContent a second
# abseil_dll. Target pin for pdfium_all alignment: 20260107.1.

include(FetchContent)

function(mps_fetch_protobuf)
  # Do not re-clone / update on every configure — avoids wiping dep build stamps.
  set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

  set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
  # One libprotobuf.dll for all mps_* modules — Message types are safe across DLL
  # boundaries (unlike statically linking protobuf into mps_ipc only).
  if(MPS_BUILD_SHARED)
    set(protobuf_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
  else()
    set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  endif()
  set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
  set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
  set(protobuf_BUILD_LIBPROTOC ON CACHE BOOL "" FORCE)
  set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
  set(ABSL_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
  set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(utf8_range_ENABLE_TESTS OFF CACHE BOOL "" FORCE)

  set(MPS_ABSEIL_PIN_PREFIX "" CACHE PATH
    "AbseilPin install prefix (…/prefix/20260107.1). Empty = protobuf FetchContent its own abseil")

  # Sibling default when unset: prefer pdfium-aligned pin if built.
  if(NOT MPS_ABSEIL_PIN_PREFIX OR MPS_ABSEIL_PIN_PREFIX STREQUAL "")
    if(DEFINED MPS_ROOT AND NOT MPS_ROOT STREQUAL "")
      set(_mps_pin_root "${MPS_ROOT}")
    else()
      set(_mps_pin_root "${CMAKE_SOURCE_DIR}")
    endif()
    set(_mps_sibling_pin "${_mps_pin_root}/../AbseilPin/prefix/20260107.1")
    if(EXISTS "${_mps_sibling_pin}/lib/cmake/absl/abslConfig.cmake")
      set(MPS_ABSEIL_PIN_PREFIX "${_mps_sibling_pin}" CACHE PATH
        "AbseilPin install prefix (…/prefix/20260107.1). Empty = protobuf FetchContent its own abseil"
        FORCE)
      message(STATUS "MPS: auto MPS_ABSEIL_PIN_PREFIX=${MPS_ABSEIL_PIN_PREFIX}")
    endif()
  endif()

  if(MPS_ABSEIL_PIN_PREFIX AND NOT MPS_ABSEIL_PIN_PREFIX STREQUAL "")
    list(PREPEND CMAKE_PREFIX_PATH "${MPS_ABSEIL_PIN_PREFIX}")
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    # Force absl_DIR to the AbseilPin prefix ahead of any cached absl_DIR.
    set(absl_DIR "${MPS_ABSEIL_PIN_PREFIX}/lib/cmake/absl" CACHE PATH
      "Abseil CONFIG dir when MPS_ABSEIL_PIN_PREFIX is set" FORCE)
    # protobuf ≥v33: find_package(absl CONFIG) via PREFIX_PATH.
    set(protobuf_FORCE_FETCH_DEPENDENCIES OFF CACHE BOOL "" FORCE)
    set(protobuf_ABSL_PROVIDER "package" CACHE STRING "" FORCE)
    message(STATUS "MPS: using AbseilPin from ${MPS_ABSEIL_PIN_PREFIX}")
    message(STATUS "MPS: absl_DIR=${absl_DIR}")
  endif()

  FetchContent_Declare(
    protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG        v35.1
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE
  )

  FetchContent_MakeAvailable(protobuf)

  if(NOT TARGET protobuf::libprotobuf)
    message(FATAL_ERROR "FetchContent protobuf did not provide protobuf::libprotobuf")
  endif()

  # Pin abseil is not built into _deps; stage next to build/bin so demos and the
  # protoc PATH prefix can load it (ambient PATH may contain a foreign abseil_dll).
  if(WIN32 AND MPS_ABSEIL_PIN_PREFIX AND NOT MPS_ABSEIL_PIN_PREFIX STREQUAL "")
    set(_mps_pin_absl_dll "${MPS_ABSEIL_PIN_PREFIX}/bin/abseil_dll.dll")
    if(EXISTS "${_mps_pin_absl_dll}")
      file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
      file(COPY "${_mps_pin_absl_dll}" DESTINATION "${CMAKE_BINARY_DIR}/bin")
    endif()
  endif()

  # Ensure protobuf_generate() can find protoc from the fetched build.
  if(TARGET protobuf::protoc AND NOT Protobuf_PROTOC_EXECUTABLE)
    set(Protobuf_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc> CACHE FILEPATH "" FORCE)
  endif()

  get_target_property(_mps_pb_type protobuf::libprotobuf TYPE)
  set(MPS_PROTOBUF_SHARED FALSE PARENT_SCOPE)
  set(MPS_PROTOBUF_RUNTIME_TARGETS "" PARENT_SCOPE)
  if(_mps_pb_type STREQUAL "SHARED_LIBRARY")
    set(MPS_PROTOBUF_SHARED TRUE PARENT_SCOPE)
    set(_mps_pb_runtime protobuf::libprotobuf)
    # Shared protobuf pulls shared abseil / utf8_range on Windows.
    if(TARGET abseil_dll)
      list(APPEND _mps_pb_runtime abseil_dll)
    elseif(TARGET absl::abseil_dll)
      list(APPEND _mps_pb_runtime absl::abseil_dll)
    endif()
    if(TARGET utf8_validity)
      list(APPEND _mps_pb_runtime utf8_validity)
    elseif(TARGET utf8_range::utf8_validity)
      list(APPEND _mps_pb_runtime utf8_range::utf8_validity)
    endif()
    # CACHE INTERNAL: visible to parents that embed MPS via add_subdirectory
    # (function PARENT_SCOPE only reaches this project's CMakeLists.txt).
    set(MPS_PROTOBUF_RUNTIME_TARGETS "${_mps_pb_runtime}" CACHE INTERNAL
      "Shared protobuf/abseil/utf8 DLLs to copy beside apps" FORCE)
    set(MPS_PROTOBUF_RUNTIME_TARGETS "${_mps_pb_runtime}" PARENT_SCOPE)
  endif()

  set(MPS_PROTOBUF_FOUND TRUE PARENT_SCOPE)
  message(STATUS "protobuf available via FetchContent (shared=${protobuf_BUILD_SHARED_LIBS})")
endfunction()

# Copy shared protobuf runtime DLLs next to an executable (Windows demos/tests).
function(mps_copy_protobuf_runtime dest_target)
  if(NOT WIN32 OR NOT MPS_PROTOBUF_SHARED)
    return()
  endif()
  foreach(_t IN LISTS MPS_PROTOBUF_RUNTIME_TARGETS)
    if(TARGET ${_t})
      add_custom_command(TARGET ${dest_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${_t}>
                $<TARGET_FILE_DIR:${dest_target}>
        VERBATIM
      )
    endif()
  endforeach()
endfunction()

function(mps_fetch_googletest)
  set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE
  )
  FetchContent_MakeAvailable(googletest)
endfunction()
