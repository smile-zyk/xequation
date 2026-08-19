#ifndef REL_RUNTIME_API_H
#define REL_RUNTIME_API_H

// ---------------------------------------------------------------------------
//  rel_runtime DLL export / import
// ---------------------------------------------------------------------------
//  rel_runtime is a SHARED library that hosts Value, its operators, and the
//  evaluation engine on top of the xdataset storage types.
#ifdef _WIN32
  #ifdef REL_RUNTIME_BUILD_DLL
    #define REL_RUNTIME_API __declspec(dllexport)
  #else
    #define REL_RUNTIME_API __declspec(dllimport)
  #endif
#else
  #define REL_RUNTIME_API
#endif

#endif  // REL_RUNTIME_API_H
