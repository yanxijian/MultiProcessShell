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
# On Windows shared builds, POST_BUILD discovery runs before DLL copy-beside and
# fails with 0xc0000135. Discover at ctest time after the full build finishes.
if(WIN32 AND MPS_BUILD_SHARED)
  gtest_discover_tests(mps_ipc_tests DISCOVERY_MODE PRE_TEST)
else()
  gtest_discover_tests(mps_ipc_tests)
endif()

if(WIN32 AND MPS_BUILD_SHARED)
  add_custom_command(TARGET mps_ipc_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mps_ipc> $<TARGET_FILE_DIR:mps_ipc_tests>
    COMMENT "Copy mps_ipc.dll beside mps_ipc_tests"
    VERBATIM
  )
  mps_copy_protobuf_runtime(mps_ipc_tests)
endif()

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
    COMMAND "${MPS_PYTHON}" "${MPS_ROOT}/clients/python/test_frame_envelope.py"
  )
  set_tests_properties(m4b_python_frame_envelope PROPERTIES
    WORKING_DIRECTORY "${MPS_ROOT}/clients/python"
  )
endif()

if(TARGET mps::ipc_qt)
  add_executable(mps_tests_m4b_hello
    test_m4b_python_hello.cpp
  )
  target_compile_definitions(mps_tests_m4b_hello
    PRIVATE
      MPS_SOURCE_DIR="${MPS_ROOT}"
  )
  target_link_libraries(mps_tests_m4b_hello
    PRIVATE
      mps::ipc_qt
      GTest::gtest_main
      Qt6::Core
      Qt6::Network
  )
  set_target_properties(mps_tests_m4b_hello PROPERTIES AUTOMOC ON)
  if(WIN32 AND MPS_BUILD_SHARED)
    add_custom_command(TARGET mps_tests_m4b_hello POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mps_ipc> $<TARGET_FILE_DIR:mps_tests_m4b_hello>
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mps_ipc_qt> $<TARGET_FILE_DIR:mps_tests_m4b_hello>
      COMMENT "Copy mps_ipc*.dll beside mps_tests_m4b_hello"
      VERBATIM
    )
    mps_copy_protobuf_runtime(mps_tests_m4b_hello)
    # Do not bake PATH into the test ENVIRONMENT: CTest treats ';' as a list
    # separator and truncates PATH, which breaks finding python.exe / Qt DLLs.
    gtest_discover_tests(mps_tests_m4b_hello DISCOVERY_MODE PRE_TEST)
  else()
    gtest_discover_tests(mps_tests_m4b_hello)
  endif()
endif()
