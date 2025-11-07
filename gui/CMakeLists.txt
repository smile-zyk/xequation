set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(xequation_gui_SRC
)

add_library(xequation_gui STATIC ${xequation_gui_SRC})

target_link_libraries(xequation_gui PUBLIC qt_config)
target_link_libraries(xequation_gui PUBLIC xequation)