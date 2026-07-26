cmake_minimum_required(VERSION 3.16)
project(kohiko CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(X11 REQUIRED)

# --- kohiko: the window manager ------------------------------------------------

file(GLOB KOHIKO_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(kohiko ${KOHIKO_SOURCES})
target_include_directories(kohiko PRIVATE ${CMAKE_SOURCE_DIR}/include ${X11_INCLUDE_DIR})
target_link_libraries(kohiko PRIVATE ${X11_LIBRARIES})
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

# --- install ------------------------------------------------------------------------

install(TARGETS kohiko kohikoctl RUNTIME DESTINATION bin)
install(FILES config/default.conf DESTINATION share/kohiko)
