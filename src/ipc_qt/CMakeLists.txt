if(MPS_BUILD_SHARED)
  add_library(mps_ipc_qt SHARED
    envelope_channel.cpp
    envelope_channel.hpp
    theme_scheme.hpp
  )
else()
  add_library(mps_ipc_qt STATIC
    envelope_channel.cpp
    envelope_channel.hpp
    theme_scheme.hpp
  )
endif()
add_library(mps::ipc_qt ALIAS mps_ipc_qt)

target_include_directories(mps_ipc_qt
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${MPS_GENERATED_INCLUDE_DIR}>
)

target_link_libraries(mps_ipc_qt
  PUBLIC
    mps::ipc
    Qt6::Core
    Qt6::Network
)

set_target_properties(mps_ipc_qt PROPERTIES
  OUTPUT_NAME mps_ipc_qt
  EXPORT_NAME ipc_qt
  AUTOMOC ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
if(MPS_BUILD_SHARED AND WIN32)
  set_target_properties(mps_ipc_qt PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

generate_export_header(mps_ipc_qt
  BASE_NAME MPS_IPC_QT
  EXPORT_MACRO_NAME MPS_IPC_QT_EXPORT
  EXPORT_FILE_NAME "${MPS_GENERATED_INCLUDE_DIR}/mps/mps_ipc_qt_export.hpp"
)
