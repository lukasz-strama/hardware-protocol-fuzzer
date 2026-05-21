cmake_minimum_required(VERSION 3.16)

project(HardwareProtocolFuzzerDesktop VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(HardwareProtocolFuzzerDesktop
    resources.qrc
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
)

target_link_libraries(HardwareProtocolFuzzerDesktop PRIVATE Qt6::Widgets)

set_target_properties(HardwareProtocolFuzzerDesktop PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
