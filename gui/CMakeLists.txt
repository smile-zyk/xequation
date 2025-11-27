set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(xequation_gui_SRC
equation_browser_widget.h
equation_browser_widget.cc
equation_editor.h
equation_editor.cc
variable_property_browser.h
variable_property_browser.cc
variable_inspect_widget.h
variable_inspect_widget.cc
variable_property_manager.h
variable_property_manager.cc
python/converters/python_property_converter.h
python/converters/python_property_converter.cc
python/converters/basic_python_converter.h
python/converters/basic_python_converter.cc
)

add_library(xequation_gui STATIC ${xequation_gui_SRC})

target_link_libraries(xequation_gui PUBLIC qt_config)
target_link_libraries(xequation_gui PUBLIC xequation)
target_link_libraries(xequation_gui PUBLIC qtpropertybrowser)

target_include_directories(xequation_gui PUBLIC ./)