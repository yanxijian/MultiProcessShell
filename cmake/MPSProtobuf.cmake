# Fetch protobuf with FetchContent (static, tests/examples off).

include(FetchContent)

function(mps_fetch_protobuf)
  set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
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
  )

  FetchContent_MakeAvailable(protobuf)

  if(NOT TARGET protobuf::libprotobuf)
    message(FATAL_ERROR "FetchContent protobuf did not provide protobuf::libprotobuf")
  endif()

  # Ensure protobuf_generate() can find protoc from the fetched build.
  if(TARGET protobuf::protoc AND NOT Protobuf_PROTOC_EXECUTABLE)
    set(Protobuf_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc> CACHE FILEPATH "" FORCE)
  endif()

  set(MPS_PROTOBUF_FOUND TRUE PARENT_SCOPE)
  message(STATUS "protobuf available via FetchContent")
endfunction()

function(mps_fetch_googletest)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(googletest)
endfunction()
