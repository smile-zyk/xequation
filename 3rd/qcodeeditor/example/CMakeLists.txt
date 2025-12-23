cmake_minimum_required(VERSION 3.6)
project(QCodeEditorExample)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC On)
set(CMAKE_AUTORCC ON)

add_executable(QCodeEditorExample
    resources/demo_resources.qrc
    src/main.cpp
    src/MainWindow.cpp
    include/MainWindow.hpp
)

target_include_directories(QCodeEditorExample PUBLIC
    include
)

target_link_libraries(QCodeEditorExample qt_config)

target_link_libraries(QCodeEditorExample
    QCodeEditor
)