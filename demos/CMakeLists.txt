add_executable(mps_demo_host
  demo_settings.hpp
  demo_host/main.cpp
  demo_host/home_content.cpp
  demo_host/home_content.hpp
  demo_host/theme_service.cpp
  demo_host/theme_service.hpp
)
target_link_libraries(mps_demo_host PRIVATE mps::host ${MPS_QTE_TARGET})
target_include_directories(mps_demo_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mps_demo_host PROPERTIES AUTOMOC ON)

add_executable(mps_demo_client
  demo_settings.hpp
  demo_client/main.cpp
  demo_client/ribbon_content_window.cpp
  demo_client/ribbon_content_window.hpp
  demo_client/ribbon_content_view.cpp
  demo_client/ribbon_content_view.hpp
)
target_link_libraries(mps_demo_client PRIVATE mps::client ${MPS_QFR_TARGET} ${MPS_QTE_TARGET})
target_include_directories(mps_demo_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mps_demo_client PROPERTIES AUTOMOC ON)

# GUI subsystem: no extra console window on double-click (Windows).
if(WIN32)
  set_target_properties(mps_demo_host mps_demo_client PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(NOT WINDEPLOYQT_EXECUTABLE)
  find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt windeployqt.exe
    HINTS
      "$ENV{QTDIR}/bin"
      "${Qt6_DIR}/../../../bin"
    DOC "Qt windeployqt tool"
  )
endif()

add_custom_command(TARGET mps_demo_host POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:mps_demo_client>
          $<TARGET_FILE_DIR:mps_demo_host>
  COMMENT "Copy mps_demo_client beside mps_demo_host"
)

if(WIN32 AND MPS_BUILD_SHARED)
  set(_mps_demo_dll_copy_cmds
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mps_ipc> $<TARGET_FILE_DIR:mps_demo_host>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mps_ipc_qt> $<TARGET_FILE_DIR:mps_demo_host>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mps_host> $<TARGET_FILE_DIR:mps_demo_host>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mps_client> $<TARGET_FILE_DIR:mps_demo_host>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${MPS_QTE_TARGET}> $<TARGET_FILE_DIR:mps_demo_host>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${MPS_QFR_TARGET}> $<TARGET_FILE_DIR:mps_demo_host>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mps_ipc> $<TARGET_FILE_DIR:mps_demo_client>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mps_ipc_qt> $<TARGET_FILE_DIR:mps_demo_client>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mps_client> $<TARGET_FILE_DIR:mps_demo_client>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${MPS_QTE_TARGET}> $<TARGET_FILE_DIR:mps_demo_client>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${MPS_QFR_TARGET}> $<TARGET_FILE_DIR:mps_demo_client>
  )
  if(MPS_PROTOBUF_SHARED)
    foreach(_t IN LISTS MPS_PROTOBUF_RUNTIME_TARGETS)
      if(TARGET ${_t})
        list(APPEND _mps_demo_dll_copy_cmds
          COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${_t}> $<TARGET_FILE_DIR:mps_demo_host>
          COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${_t}> $<TARGET_FILE_DIR:mps_demo_client>
        )
      endif()
    endforeach()
  endif()
  add_custom_command(TARGET mps_demo_host POST_BUILD
    ${_mps_demo_dll_copy_cmds}
    COMMENT "Copy mps_*.dll + protobuf runtime + qte_engine + qfr_ribbon beside demos"
    VERBATIM
  )
endif()

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
  add_custom_command(TARGET mps_demo_host POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --release
            --no-translations
            --compiler-runtime
            $<TARGET_FILE:mps_demo_host>
    COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --release
            --no-translations
            $<TARGET_FILE:mps_demo_client>
    COMMENT "windeployqt: deploy Qt runtime beside demo executables"
    VERBATIM
  )
elseif(WIN32)
  message(WARNING "windeployqt not found; demos will need QTDIR/bin on PATH to run")
endif()
