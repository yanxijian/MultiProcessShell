add_library(mps_client STATIC
  client_app.cpp
  client_app.hpp
)
add_library(mps::client ALIAS mps_client)
target_include_directories(mps_client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mps_client
  PUBLIC
    mps::ipc_qt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)
set_target_properties(mps_client PROPERTIES AUTOMOC ON)
if(WIN32)
  target_link_libraries(mps_client PUBLIC user32)
endif()
