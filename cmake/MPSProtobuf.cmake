# Fetch protobuf with FetchContent (shared when MPS_BUILD_SHARED, tests/examples off).

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

  FetchContent_Declare(
    protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG        v29.3
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE
  )

  FetchContent_MakeAvailable(protobuf)

  if(NOT TARGET protobuf::libprotobuf)
    message(FATAL_ERROR "FetchContent protobuf did not provide protobuf::libprotobuf")
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
    endif()
    if(TARGET utf8_validity)
      list(APPEND _mps_pb_runtime utf8_validity)
    endif()
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
