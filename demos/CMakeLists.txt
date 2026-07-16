add_executable(mps_demo_host demo_host/main.cpp)
target_link_libraries(mps_demo_host PRIVATE mps::host)
set_target_properties(mps_demo_host PROPERTIES AUTOMOC ON)

add_executable(mps_demo_client demo_client/main.cpp)
target_link_libraries(mps_demo_client PRIVATE mps::client)
set_target_properties(mps_demo_client PROPERTIES AUTOMOC ON)

# GUI subsystem: no extra console window on double-click (Windows).
if(WIN32)
  set_target_properties(mps_demo_host mps_demo_client PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Resolve windeployqt from Qt prefix (QTDIR / CMAKE_PREFIX_PATH).
if(NOT WINDEPLOYQT_EXECUTABLE)
  find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt windeployqt.exe
    HINTS
      "$ENV{QTDIR}/bin"
      "${Qt6_DIR}/../../../bin"
    DOC "Qt windeployqt tool"
  )
endif()

# Host finds client next to itself, then deploy Qt (+ CRT) for double-click run.
add_custom_command(TARGET mps_demo_host POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:mps_demo_client>
          $<TARGET_FILE_DIR:mps_demo_host>
  COMMENT "Copy mps_demo_client beside mps_demo_host"
)

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
