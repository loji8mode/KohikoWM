cmake_minimum_required(VERSION 3.16)
project(kohiko CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(X11 REQUIRED)

# Xft/fontconfig - used by Font.cpp for the bar/launcher/notepad's
# text rendering (see include/Font.h for why plain libX11 alone isn't
# enough for that). Neither has an upstream CMake find-module, so this
# goes through pkg-config the same way the Makefile does.
find_package(PkgConfig REQUIRED)
pkg_check_modules(Xft REQUIRED IMPORTED_TARGET xft fontconfig)
pkg_check_modules(Imlib2 REQUIRED IMPORTED_TARGET imlib2)

# --- kohiko: the window manager ------------------------------------------------

file(GLOB KOHIKO_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(kohiko ${KOHIKO_SOURCES})
target_include_directories(kohiko PRIVATE ${CMAKE_SOURCE_DIR}/include ${X11_INCLUDE_DIR})
target_link_libraries(kohiko PRIVATE ${X11_LIBRARIES} PkgConfig::Xft PkgConfig::Imlib2)
target_compile_options(kohiko PRIVATE -Wall -Wextra)

# Multi-monitor geometry via XRandr, enabled only if it's actually
# available - kohiko still works fine as a single-monitor WM without it.
if(X11_Xrandr_FOUND)
    target_compile_definitions(kohiko PRIVATE KOHIKO_HAVE_XRANDR)
    target_link_libraries(kohiko PRIVATE ${X11_Xrandr_LIB})
endif()

# --- kohikoctl: the IPC command-line client --------------------------------------

add_executable(kohikoctl tools/kohikoctl.cpp src/IpcPath.cpp)
target_include_directories(kohikoctl PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(kohikoctl PRIVATE -Wall -Wextra)

# --- tests ------------------------------------------------------------------------

enable_testing()

add_executable(test_bsptree
    tests/test_bsptree.cpp
    src/BSPTree.cpp
    src/BSPLeaf.cpp
    src/BSPSplit.cpp
    src/ManagedWindow.cpp
    src/LayoutEngine.cpp
)
target_include_directories(test_bsptree PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME BSPTree COMMAND test_bsptree)

# Pure string/data-structure logic, same as test_bsptree above - no
# X11 display needed.
add_executable(test_launcherscoring
    tests/test_launcherscoring.cpp
    src/LauncherScoring.cpp
    src/Utils.cpp
)
target_include_directories(test_launcherscoring PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME LauncherScoring COMMAND test_launcherscoring)

# Needs a real X11/XRandR connection (skips its X-dependent checks
# gracefully if $DISPLAY isn't set - see the file for why) - the same
# libraries kohiko itself links against for monitor detection.
add_executable(test_monitormanager
    tests/test_monitormanager.cpp
    src/Monitor.cpp
    src/MonitorManager.cpp
    src/MonitorRule.cpp
    src/Workspace.cpp
    src/WorkspaceManager.cpp
    src/BSPTree.cpp
    src/BSPLeaf.cpp
    src/BSPSplit.cpp
    src/ManagedWindow.cpp
    src/LayoutEngine.cpp
    src/Config.cpp
    src/XConnection.cpp
    src/XAtoms.cpp
    src/Utils.cpp
    src/Logger.cpp
)
target_include_directories(test_monitormanager PRIVATE ${CMAKE_SOURCE_DIR}/include ${X11_INCLUDE_DIR})
target_link_libraries(test_monitormanager PRIVATE ${X11_LIBRARIES})
if(X11_Xrandr_FOUND)
    target_compile_definitions(test_monitormanager PRIVATE KOHIKO_HAVE_XRANDR)
    target_link_libraries(test_monitormanager PRIVATE ${X11_Xrandr_LIB})
endif()
add_test(NAME MonitorManager COMMAND test_monitormanager)

# --- install ------------------------------------------------------------------------

install(TARGETS kohiko kohikoctl RUNTIME DESTINATION bin)
install(FILES config/default.conf DESTINATION share/kohiko)
