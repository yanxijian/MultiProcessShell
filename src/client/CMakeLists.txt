if(MPS_BUILD_SHARED)
  add_library(mps_client SHARED
    client_app.cpp
    client_app.hpp
    content_view.hpp
  )
else()
  add_library(mps_client STATIC
    client_app.cpp
    client_app.hpp
    content_view.hpp
  )
endif()
add_library(mps::client ALIAS mps_client)

target_include_directories(mps_client
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${MPS_GENERATED_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(mps_client
  PUBLIC
    mps::ipc_qt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

set_target_properties(mps_client PROPERTIES
  OUTPUT_NAME mps_client
  EXPORT_NAME client
  AUTOMOC ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
if(MPS_BUILD_SHARED AND WIN32)
  set_target_properties(mps_client PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

generate_export_header(mps_client
  BASE_NAME MPS_CLIENT
  EXPORT_MACRO_NAME MPS_CLIENT_EXPORT
  EXPORT_FILE_NAME "${MPS_GENERATED_INCLUDE_DIR}/mps/mps_client_export.hpp"
)

if(WIN32)
  target_link_libraries(mps_client PUBLIC user32)
endif()
