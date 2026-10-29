cmake_minimum_required(VERSION 3.21)
project(DesktopSuite VERSION 0.1.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
add_library(suite_core src/common/Animation.cpp)
target_include_directories(suite_core PUBLIC src)
if(BUILD_TESTING)
  add_executable(core_tests tests/core_tests.cpp)
  target_link_libraries(core_tests PRIVATE suite_core)
  add_test(NAME core_tests COMMAND core_tests)
endif()
if(WIN32)
  find_package(unofficial-sqlite3 CONFIG REQUIRED)
  find_package(nlohmann_json CONFIG REQUIRED)
  add_library(suite_storage src/common/Database.cpp)
  target_include_directories(suite_storage PUBLIC src)
  target_link_libraries(suite_storage PUBLIC unofficial::sqlite3::sqlite3)
  add_executable(DesktopSuite WIN32
    src/main.cpp src/app/Application.cpp src/app/TrayIcon.cpp
    src/edge_panel/EdgePanel.cpp src/common/D2DRenderer.cpp
    src/settings/SettingsWindow.cpp src/update/AutoUpdate.cpp src/tools/NativeTools.cpp src/app/DesktopSuite.rc)
  target_compile_definitions(DesktopSuite PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN SUITE_VERSION="${PROJECT_VERSION}" SUITE_VERSION_W=L"${PROJECT_VERSION}")
  target_link_libraries(DesktopSuite PRIVATE suite_core suite_storage d2d1 dwrite dwmapi shell32 ole32 user32 gdi32 gdiplus comctl32 winhttp bcrypt advapi32 version nlohmann_json::nlohmann_json)
  if(MSVC)
    target_compile_options(DesktopSuite PRIVATE /W4 /utf-8 /permissive-)
    target_compile_options(suite_storage PRIVATE /W4 /utf-8)
    target_link_options(DesktopSuite PRIVATE /MANIFEST:EMBED /MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/src/app/DesktopSuite.manifest)
  endif()
  if(BUILD_TESTING)
    add_executable(database_tests tests/database_tests.cpp)
    target_link_libraries(database_tests PRIVATE suite_storage)
    if(MSVC)
      target_compile_options(database_tests PRIVATE /utf-8)
    endif()
    add_test(NAME database_tests COMMAND database_tests)
  endif()
  install(TARGETS DesktopSuite RUNTIME DESTINATION .)
  set(CPACK_VERBATIM_VARIABLES YES)
  set(CPACK_GENERATOR NSIS)
  set(CPACK_PACKAGE_NAME DesktopSuite)
  set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
  set(CPACK_PACKAGE_INSTALL_DIRECTORY DesktopSuite)
  set(CPACK_NSIS_DISPLAY_NAME "Desktop Widget Suite")
  set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
  set(CPACK_PACKAGE_EXECUTABLES "DesktopSuite" "Desktop Widget Suite")
  set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [[
    FindWindow $0 "DesktopSuite.Edge"
    StrCmp $0 0 suite_cleanup
      MessageBox MB_OK|MB_ICONEXCLAMATION "Please quit Desktop Suite from the tray before uninstalling."
      Abort
    suite_cleanup:
    DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "DesktopSuite"
    ReadRegStr $1 HKCU "Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Local AppData"
    StrCmp $1 "" suite_cleanup_done
      RMDir /r "$1\DesktopSuite\bin"
      RMDir /r "$1\DesktopSuite\updates"
    suite_cleanup_done:
  ]])
  include(CPack)
else()
  message(STATUS "Non-Windows host: building portable animation/geometry tests only")
endif()
