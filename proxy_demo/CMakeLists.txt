set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(proxy_demo_SRC
proxy_demo_widget.h
proxy_demo_widget.cc
equation_property_widget.h
equation_property_widget.cc
equation_data_frame_model.h
equation_data_frame_model.cc
equation_data_frame_view.h
equation_data_frame_view.cc
xequation_proxy.h
xequation_proxy.cc
main.cc
)

add_executable(proxy_demo ${proxy_demo_SRC})

target_link_libraries(proxy_demo PUBLIC qt_config)
# XEquationProxy implementation source is in this directory; its symbols are
# provided by the core / rel_engine / python submodules.
target_link_libraries(proxy_demo PUBLIC xequation_core)
target_link_libraries(proxy_demo PUBLIC xequation_rel)
target_link_libraries(proxy_demo PUBLIC xequation_python)

if(WIN32)
  get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
  get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
  add_custom_command(TARGET
    proxy_demo POST_BUILD
    COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:proxy_demo>
    COMMENT "run windeployqt")
endif()

# windeployqt only copies Qt DLLs; REL / xdataset runtime DLLs and their
# transitive deps (e.g. HDF5: libhdf5-320.dll -> libaws-*.dll / libsz-2.dll /
# libz-ng2.dll) are resolved and copied by copy_runtime_deps.cmake (excluding
# system and Qt DLLs).
if(WIN32)
  get_filename_component(RUNTIME_DEP_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(RUNTIME_DEP_EXTRA_DIRS "${QT_BIN_DIR};${RUNTIME_DEP_COMPILER_BIN}")
  add_custom_command(TARGET proxy_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DEXE=$<TARGET_FILE:proxy_demo>
        -DDEST=$<TARGET_FILE_DIR:proxy_demo>
        "-DEXTRA_DIRS=${RUNTIME_DEP_EXTRA_DIRS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/copy_runtime_deps.cmake"
    COMMENT "Copy full runtime dependency DLLs next to proxy_demo")
endif()
