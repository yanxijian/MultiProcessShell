add_executable(mps_demo_host demo_host/main.cpp)
target_link_libraries(mps_demo_host PRIVATE mps::host)
set_target_properties(mps_demo_host PROPERTIES AUTOMOC ON)

add_executable(mps_demo_client demo_client/main.cpp)
target_link_libraries(mps_demo_client PRIVATE mps::client)
set_target_properties(mps_demo_client PROPERTIES AUTOMOC ON)

# Host finds client next to itself.
add_custom_command(TARGET mps_demo_host POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:mps_demo_client>
          $<TARGET_FILE_DIR:mps_demo_host>
  COMMENT "Copy mps_demo_client beside mps_demo_host"
)
