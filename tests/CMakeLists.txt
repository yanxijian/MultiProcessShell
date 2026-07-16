# Protocol / framing unit tests (M0).

add_executable(mps_ipc_tests
  test_frame.cpp
  test_envelope.cpp
)

target_link_libraries(mps_ipc_tests
  PRIVATE
    mps::ipc
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(mps_ipc_tests)
