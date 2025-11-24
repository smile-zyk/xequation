set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(xequation_gui_SRC
equation_manager_widget.h
equation_manager_widget.cc
equation_editor.h
equation_editor.cc
)

add_library(xequation_gui STATIC ${xequation_gui_SRC})

target_link_libraries(xequation_gui PUBLIC qt_config)
target_link_libraries(xequation_gui PUBLIC xequation)
target_link_libraries(xequation_gui PUBLIC qtpropertybrowser)

target_include_directories(xequation_gui PUBLIC ./)