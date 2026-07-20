# Protocol / framing unit tests (M0) + detachable tab-strip rule tests.

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

add_executable(mps_tab_strip_tests
  test_tab_strip_mru.cpp
  test_tab_strip_yield.cpp
  test_tab_strip_tearout.cpp
  test_tab_strip_lifecycle.cpp
)

target_link_libraries(mps_tab_strip_tests
  PRIVATE
    mps::tab_strip
    GTest::gtest_main
)

gtest_discover_tests(mps_tab_strip_tests)
