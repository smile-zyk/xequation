# copy_runtime_deps.cmake
#
# Copy the full non-system runtime dependency DLL tree of a built executable
# next to it.  Used as a POST_BUILD step for rel_demo because the REL /
# xdataset DLLs pull in transitive dependencies (e.g. HDF5: libhdf5-320.dll
# -> libaws-*.dll / libsz-2.dll / libz-ng2.dll) that windeployqt does not
# handle.
#
# Required -D variables:
#   EXE        -- path to the built executable
#   DEST       -- destination directory
#   EXTRA_DIRS -- semicolon-separated extra search directories (optional)

if(NOT DEFINED EXE OR NOT DEFINED DEST)
  message(FATAL_ERROR "copy_runtime_deps.cmake: EXE and DEST must be defined")
endif()

file(GET_RUNTIME_DEPENDENCIES
  EXECUTABLES "${EXE}"
  DIRECTORIES ${EXTRA_DIRS}
  RESOLVED_DEPENDENCIES_VAR _resolved
  UNRESOLVED_DEPENDENCIES_VAR _unresolved
  CONFLICTING_DEPENDENCIES_PREFIX _conflict
  # Skip well-known Windows API names before resolution.
  PRE_EXCLUDE_REGEXES
    "^api-ms-.*"
    "^ext-ms-.*"
  # Skip Windows system DLLs (resolved against the full path).
  POST_EXCLUDE_REGEXES
    ".*/(KERNEL32|msvcrt|SHLWAPI|USER32|GDI32|ADVAPI32|SHELL32|WS2_32|OLE32|OLEAUT32|COMDLG32|WINMM|IMM32|VERSION|UxTheme|DWMAPI|CRYPT32|BCRYPT|NSI|RPCRT4|SETUPAPI|PSAPI|WININET|WINHTTP|SECUR32|AUTHZ|CFGMGR32|USERENV|WLDAP32|CRYPTBASE|SspiCli|IPHLPAPI|DNSAPI|NORMALIZ|DBGHELP|WTSAPI32|MPR|PROPSYS|WINSPOOL)\\.(dll|DRV)$"
)

foreach(_dll IN LISTS _resolved)
  get_filename_component(_name "${_dll}" NAME)
  if(EXISTS "${DEST}/${_name}")
    continue()
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${_dll}" "${DEST}/${_name}")
endforeach()

# Conflicting dependencies: the same DLL name resolved to multiple paths
# (e.g. mingw64/bin vs. the windeployqt-deployed copy next to the exe).
# The CONFLICTING_DEPENDENCIES_PREFIX var (_conflict) holds the list of
# conflicting dependency names; each <prefix>_<dep> holds its candidate
# paths.  If any candidate is already deployed in DEST, keep it; otherwise
# copy the first candidate.
foreach(_dep IN LISTS _conflict)
  set(_paths "${_conflict_${_dep}}")
  set(_skip FALSE)
  foreach(_p IN LISTS _paths)
    get_filename_component(_name "${_p}" NAME)
    if(EXISTS "${DEST}/${_name}")
      set(_skip TRUE)
      break()
    endif()
  endforeach()
  if(NOT _skip)
    list(GET _paths 0 _first)
    get_filename_component(_name "${_first}" NAME)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${_first}" "${DEST}/${_name}")
  endif()
endforeach()

if(_unresolved)
  message(WARNING "copy_runtime_deps.cmake: unresolved runtime dependencies of ${EXE}: ${_unresolved}")
endif()
