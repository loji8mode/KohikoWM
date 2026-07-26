cmake_minimum_required(VERSION 3.20)

project(Kohiko LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(X11 REQUIRED)

set(KOHIKO_HEADERS

include/Application.h
include/Bar.h
include/BSPLeaf.h
include/BSPNode.h
include/BSPSplit.h
include/BSPTree.h
include/Config.h
include/ConfigParser.h
include/CursorManager.h
include/EventDispatcher.h
include/EventLoop.h
include/InputManager.h
include/IPCServer.h
include/KeyboardManager.h
include/LayoutEngine.h
include/Logger.h
include/ManagedWindow.h
include/Monitor.h
include/MonitorManager.h
include/MouseManager.h
include/Process.h
include/Renderer.h
include/Scratchpad.h
include/Types.h
include/Utils.h
include/Version.h
include/WindowManager.h
include/WindowRepository.h
include/Workspace.h
include/WorkspaceManager.h
include/XAtoms.h
include/XConnection.h

)

set(KOHIKO_SOURCES

src/main.cpp
src/Application.cpp
src/Bar.cpp
src/BSPLeaf.cpp
src/BSPSplit.cpp
src/BSPTree.cpp
src/Config.cpp
src/ConfigParser.cpp
src/CursorManager.cpp
src/EventDispatcher.cpp
src/EventLoop.cpp
src/InputManager.cpp
src/IPCServer.cpp
src/KeyboardManager.cpp
src/LayoutEngine.cpp
src/Logger.cpp
src/ManagedWindow.cpp
src/Monitor.cpp
src/MonitorManager.cpp
src/MouseManager.cpp
src/Process.cpp
src/Renderer.cpp
src/Scratchpad.cpp
src/Utils.cpp
src/WindowManager.cpp
src/WindowRepository.cpp
src/Workspace.cpp
src/WorkspaceManager.cpp
src/XAtoms.cpp
src/XConnection.cpp

)

add_executable(
    kohiko
    ${KOHIKO_HEADERS}
    ${KOHIKO_SOURCES}
)

target_include_directories(kohiko PRIVATE include)

target_link_libraries(
    kohiko
    X11
)

target_compile_options(
    kohiko
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Winvalid-pch
)