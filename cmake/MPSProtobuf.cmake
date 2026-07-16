# Fetch protobuf with FetchContent (static, tests/examples off).

include(FetchContent)

function(mps_fetch_protobuf)
  set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
  set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

  FetchContent_Declare(
    protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG        v29.3
    GIT_SHALLOW    TRUE
  )

  FetchContent_MakeAvailable(protobuf)

  if(TARGET protobuf::libprotobuf)
    message(STATUS "protobuf available via FetchContent")
  endif()

  set(MPS_PROTOBUF_FOUND TRUE PARENT_SCOPE)
endfunction()
