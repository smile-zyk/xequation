#ifndef REL_API_H
#define REL_API_H

// ---------------------------------------------------------------------------
//  rel DLL export / import
// ---------------------------------------------------------------------------
//  rel is a SHARED library that hosts the whole REL engine: the value layer
//  (Value, operators, Environment) and the language front-end (scanner,
//  parser, AST, evaluator) on top of the xdataset storage types.
//
//  The public API surface is the flat `include/` folder (rel.h, value.h,
//  environment.h, function.h, expr.h, token.h, error.h).  Internal headers
//  under src/core and src/runtime also use REL_API so that hosts (rel_cli.exe)
//  and the test suite can link the symbols, but they are NOT part of the
//  public API.
#ifdef _WIN32
  #ifdef REL_BUILD_DLL
    #define REL_API __declspec(dllexport)
  #else
    #define REL_API __declspec(dllimport)
  #endif
#else
  #define REL_API
#endif

#endif  // REL_API_H
