set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Boost REQUIRED COMPONENTS bimap)

set(xequation_gui_SRC
equation_browser_widget.h
equation_browser_widget.cc
variable_inspect_widget.h
variable_inspect_widget.cc
expression_watch_widget.h
expression_watch_widget.cc
equation_editor.h
equation_editor.cc
equation_manager_tasks.h
equation_manager_tasks.cc
equation_code_editor.h
equation_code_editor.cc
equation_completion_model.h
equation_completion_model.cc
equation_dependency_graph_viewer.h
equation_dependency_graph_viewer.cc
value_model/value_item_builder.h
value_model/value_item_builder.cc
value_model/value_tree_model.h
value_model/value_tree_model.cc
value_model/value_tree_view.h
value_model/value_tree_view.cc
value_model/value_item.h
value_model/value_item.cc
code_editor/completion_line_edit.h
code_editor/completion_line_edit.cc
code_editor/completion_line_edit_delegate.h
code_editor/completion_line_edit_delegate.cc
code_editor/completion_list_model.h
code_editor/completion_list_model.cc
code_editor/code_completer.h
code_editor/code_completer.cc
code_editor/code_editor.h
code_editor/code_editor.cc
code_editor/code_highlighter.h
code_editor/code_highlighter.cc
task/task.h
task/task.cc
task/task_manager.h
task/task_manager.cc
task/toast_task_manager.h
task/toast_task_manager.cc
toast/toast_manager.h
toast/toast_manager.cc
toast/toast_progress_bar.h
toast/toast_progress_bar.cc
python/python_qt_wrapper.h
python/python_item_builder.h
python/python_item_builder.cc
python/python_highlighter.h
python/python_highlighter.cc
)

qt5_add_resources(xequation_gui_SRC resources/gui_resource.qrc)

add_library(xequation_gui STATIC ${xequation_gui_SRC})

target_link_libraries(xequation_gui PUBLIC qt_config)
target_link_libraries(xequation_gui PUBLIC Boost::bimap)
target_link_libraries(xequation_gui PUBLIC xequation_core)
target_link_libraries(xequation_gui PUBLIC xequation_python)
target_link_libraries(xequation_gui PUBLIC qtpropertybrowser)
target_link_libraries(xequation_gui PUBLIC QCodeEditor)

target_include_directories(xequation_gui PUBLIC ./)