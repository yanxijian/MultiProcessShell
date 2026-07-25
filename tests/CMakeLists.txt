# Protocol / framing unit tests (M0) + detachable tab-strip rule tests.
# Optional M4b: Python Hello over QLocalServer (needs Qt + Python + protobuf).

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
  test_tab_embed_map.cpp
  test_heartbeat_policy.cpp
)

target_link_libraries(mps_tab_strip_tests
  PRIVATE
    mps::tab_strip
    GTest::gtest_main
)

gtest_discover_tests(mps_tab_strip_tests)

# Offline Python frame/Envelope tests (no Qt). Skip if protobuf module missing.
find_program(MPS_PYTHON NAMES python python3)
if(MPS_PYTHON)
  add_test(
    NAME m4b_python_frame_envelope
    COMMAND "${MPS_PYTHON}" "${CMAKE_SOURCE_DIR}/clients/python/test_frame_envelope.py"
  )
  set_tests_properties(m4b_python_frame_envelope PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/clients/python"
  )
endif()

if(TARGET mps::ipc_qt)
  add_executable(mps_m4b_python_hello
    test_m4b_python_hello.cpp
  )
  target_compile_definitions(mps_m4b_python_hello
    PRIVATE
      MPS_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  )
  target_link_libraries(mps_m4b_python_hello
    PRIVATE
      mps::ipc_qt
      GTest::gtest_main
      Qt6::Core
      Qt6::Network
  )
  set_target_properties(mps_m4b_python_hello PROPERTIES AUTOMOC ON)
  gtest_discover_tests(mps_m4b_python_hello)
endif()
