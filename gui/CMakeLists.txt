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
debugger/variable_view.h
debugger/variable_view.cc
debugger/variable_model.h
debugger/variable_model.cc
debugger/variable_manager.h
debugger/variable_manager.cc
debugger/variable_test_widget.h
debugger/variable_test_widget.cc
)

add_library(xequation_gui STATIC ${xequation_gui_SRC})

target_link_libraries(xequation_gui PUBLIC qt_config)
target_link_libraries(xequation_gui PUBLIC xequation)
target_link_libraries(xequation_gui PUBLIC qtpropertybrowser)

target_include_directories(xequation_gui PUBLIC ./)