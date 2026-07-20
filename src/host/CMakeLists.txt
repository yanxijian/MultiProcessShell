add_library(mps_host STATIC
  client_session.cpp
  client_session.hpp
  shell_app.cpp
  shell_app.hpp
  shell_window.cpp
  shell_window.hpp
  tear_out_preview.cpp
  tear_out_preview.hpp
  tab_info.hpp
  embed/win/embed_container.cpp
  embed/win/embed_container.hpp
)
add_library(mps::host ALIAS mps_host)
target_include_directories(mps_host
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/embed/win"
)
target_link_libraries(mps_host
  PUBLIC
    mps::ipc_qt
    mps::tab_strip
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)
set_target_properties(mps_host PROPERTIES AUTOMOC ON)
if(WIN32)
  target_link_libraries(mps_host PUBLIC user32 gdi32)
endif()
