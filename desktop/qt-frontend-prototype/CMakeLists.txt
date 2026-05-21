cmake_minimum_required(VERSION 3.16)

project(HardwareProtocolFuzzerDesktop VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(HardwareProtocolFuzzerDesktop
    resources.qrc
    src/backend_bridge.cpp
    src/backend_bridge.h
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    ${CMAKE_CURRENT_LIST_DIR}/../../csv_logger.c
    ${CMAKE_CURRENT_LIST_DIR}/../../diag.c
    ${CMAKE_CURRENT_LIST_DIR}/../../frame.c
    ${CMAKE_CURRENT_LIST_DIR}/../../session.c
    ${CMAKE_CURRENT_LIST_DIR}/../../transport.c
)

target_link_libraries(HardwareProtocolFuzzerDesktop PRIVATE Qt6::Widgets)

target_include_directories(HardwareProtocolFuzzerDesktop PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../..
)

set_target_properties(HardwareProtocolFuzzerDesktop PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
