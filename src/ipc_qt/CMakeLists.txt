add_library(mps_ipc_qt STATIC
  envelope_channel.cpp
  envelope_channel.hpp
)
add_library(mps::ipc_qt ALIAS mps_ipc_qt)
target_include_directories(mps_ipc_qt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mps_ipc_qt
  PUBLIC
    mps::ipc
    Qt6::Core
    Qt6::Network
)
set_target_properties(mps_ipc_qt PROPERTIES AUTOMOC ON)
