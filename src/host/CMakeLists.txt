if(MPS_BUILD_SHARED)
  add_library(mps_host SHARED
    client_session.cpp
    client_session.hpp
    shell_app.cpp
    shell_app.hpp
    shell_window.cpp
    shell_window.hpp
    tear_out_preview.cpp
    tear_out_preview.hpp
    theme_origin.hpp
    win_capture.cpp
    win_capture.hpp
    tab_info.hpp
    embed/win/embed_container.cpp
    embed/win/embed_container.hpp
  )
else()
  add_library(mps_host STATIC
    client_session.cpp
    client_session.hpp
    shell_app.cpp
    shell_app.hpp
    shell_window.cpp
    shell_window.hpp
    tear_out_preview.cpp
    tear_out_preview.hpp
    theme_origin.hpp
    win_capture.cpp
    win_capture.hpp
    tab_info.hpp
    embed/win/embed_container.cpp
    embed/win/embed_container.hpp
  )
endif()
add_library(mps::host ALIAS mps_host)

target_include_directories(mps_host
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/embed/win>
    $<BUILD_INTERFACE:${MPS_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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

set_target_properties(mps_host PROPERTIES
  OUTPUT_NAME mps_host
  EXPORT_NAME host
  AUTOMOC ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
if(MPS_BUILD_SHARED AND WIN32)
  set_target_properties(mps_host PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

generate_export_header(mps_host
  BASE_NAME MPS_HOST
  EXPORT_MACRO_NAME MPS_HOST_EXPORT
  EXPORT_FILE_NAME "${MPS_GENERATED_INCLUDE_DIR}/mps/mps_host_export.hpp"
)

if(WIN32)
  target_link_libraries(mps_host PUBLIC user32 gdi32)
endif()
