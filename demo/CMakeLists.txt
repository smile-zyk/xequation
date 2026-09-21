set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# =========================================================================
#  Shared sources
# =========================================================================

# The post-processing UI (the whole GUI of the `postprocess` executable).
set(postprocess_widget_SRC
postprocess_widget.h
postprocess_widget.cc
property_widget.h
property_widget.cc
data_frame_model.h
data_frame_model.cc
data_frame_view.h
data_frame_view.cc
data_frame_tab_widget.h
data_frame_tab_widget.cc
format_options_widget.h
format_options_widget.cc
explorer_view.h
explorer_view.cc
tree_view_tag.h
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
)

# JSON-RPC 2.0 protocol core (shared by both executables): messages, the
# symmetric peer, params, and the transports.  Deliberately free of any
# dependency on the postprocess UI.
set(rpc_core_SRC
rpc/json_rpc_message.h
rpc/json_rpc_message.cc
rpc/json_rpc_peer.h
rpc/json_rpc_peer.cc
rpc/params.h
rpc/method_spec.h
rpc/method_spec.cc
rpc/transport.h
rpc/stream_transport.h
rpc/stream_transport.cc
rpc/local_socket_transport.h
rpc/local_socket_transport.cc
)

# Child side only: the method modules + the service that assembles them.  These
# drive PostprocessWidget, so they are NOT part of the host build.
set(rpc_service_SRC
rpc/postprocess_rpc_service.h
rpc/postprocess_rpc_service.cc
rpc/methods/method_modules.h
rpc/methods/method_modules.cc
)

# Host library: launch + drive postprocess instances.
set(host_SRC
host/launch_config.h
host/launch_config.cc
host/postprocess_instance.h
host/postprocess_instance.cc
host/postprocess_manager.h
host/postprocess_manager.cc
)

# =========================================================================
#  postprocess -- the post-processing application
#
#  Standalone:  postprocess [project.json]
#  RPC child:   postprocess --rpc --channel <name> [--config <file>]
# =========================================================================

add_executable(postprocess
    ${postprocess_widget_SRC}
    ${rpc_core_SRC}
    ${rpc_service_SRC}
    postprocess_main.cc
)

target_include_directories(postprocess PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(postprocess PUBLIC qt_config)
# REL EquationManager 单例由 xequation_core 提供（equation_manager.h 的 RelEquationManager）。
target_link_libraries(postprocess PUBLIC xequation_core)

if(BUILD_PYTHON)
  target_link_libraries(postprocess PUBLIC python_manager)
  target_compile_definitions(postprocess PRIVATE DEMO_HAS_PYTHON)
endif()

# =========================================================================
#  demo -- the host application (launches + drives postprocess instances)
# =========================================================================

add_executable(demo
    ${host_SRC}
    ${rpc_core_SRC}
    launcher_widget.h
    launcher_widget.cc
    launcher_main.cc
)

target_include_directories(demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(demo PUBLIC qt_config)
# QLocalServer / QLocalSocket (local-socket transport) live in Qt Network.
target_link_libraries(demo PUBLIC Qt5::Network)

# =========================================================================
#  Deployment (Windows): Qt DLLs + REL / xdataset runtime deps
# =========================================================================

if(WIN32)
  get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
  get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
  get_filename_component(RUNTIME_DEP_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(RUNTIME_DEP_EXTRA_DIRS "${QT_BIN_DIR};${RUNTIME_DEP_COMPILER_BIN}")

  foreach(_target postprocess demo)
    add_custom_command(TARGET ${_target} POST_BUILD
      COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:${_target}>
      COMMENT "run windeployqt for ${_target}")

    # windeployqt only copies Qt DLLs; REL / xdataset runtime DLLs and their
    # transitive deps (e.g. HDF5: libhdf5-320.dll -> libaws-*.dll / libsz-2.dll
    # / libz-ng2.dll) are resolved and copied by copy_runtime_deps.cmake.
    add_custom_command(TARGET ${_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
          -DEXE=$<TARGET_FILE:${_target}>
          -DDEST=$<TARGET_FILE_DIR:${_target}>
          "-DEXTRA_DIRS=${RUNTIME_DEP_EXTRA_DIRS}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/copy_runtime_deps.cmake"
      COMMENT "Copy full runtime dependency DLLs next to ${_target}")
  endforeach()
endif()
