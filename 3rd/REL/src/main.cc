// Entry point for the REL interpreter.
//
// Modes:
//   - REPL mode (no arguments): read one expression or binding per line,
//     evaluate it, and print the result.
//   - File mode (one argument): evaluate each non-empty line from a file.

#include "environment.h"
#include "rel.h"
#include "function_library_sample.h"

#ifdef REL_HAS_PYTHON
#include "python_manager.h"
#endif

#ifdef _WIN32
// No readline on Windows yet �?fall back to std::getline.
#elif __APPLE__
#include <editline/readline.h>
#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <fstream>
#include <iostream>
#include <string>

namespace
{
    // Initialize the embedded Python environment (python_manager).  The
    // interpreter lifecycle is owned by the host: configure the standard
    // paths (py_home + stdlib / lib-dynload / site-packages, injected by
    // CMake at build time) and start the interpreter.  A host that already
    // runs Python keeps ownership (PyEnvManager detects Py_IsInitialized()).
    void init_python_env()
    {
#ifdef REL_HAS_PYTHON
        python_manager::PyEnvManager::SetDefaultPyEnvConfig();
        python_manager::PyEnvManager::InitializePyEnv();
#endif
    }

    // Tear down in the reverse order: release every pybind11::function held
    // by the callback registry while the interpreter is still alive, then
    // finalize the interpreter (only when python_manager created it).
    void shutdown_python_env()
    {
#ifdef REL_HAS_PYTHON
        rel::Environment::CleanupPythonState();
        python_manager::PyEnvManager::ShutdownPyEnv();
#endif
    }

    int eval_line(rel::Environment& env, const std::string& line, int /*line_no*/)
    {
        // Public front-end API: Exec handles both plain expressions and
        // `name = expr` bindings (validating the identifier, evaluating, and
        // defining the variable in `env`).  Throws on any failure.
        try
        {
            rel::Exec(line, env);
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return 1;
        }
        return 0;
    }

    int run_file(const char* path)
    {
        rel::Environment env;
        rel::Environment::InitBuiltinConstants();
        rel::Environment::InitBuiltinFunctions();
        rel::Environment::RegisterLibrary(rel::function_library_sample::MakeLibrary());
#ifdef REL_LOAD_TEST_ENV
        try {
            // Absolute path injected by CMake (CMAKE_SOURCE_DIR) — the
            // working directory is irrelevant.
            rel::Environment::LoadFromConfig(
                std::string(REL_SOURCE_DIR) + "/case/test_env.json");
        } catch (const std::exception& e) {
            std::cerr << "warning: test env load failed: " << e.what() << '\n';
        }
#endif

        std::ifstream file(path);
        if (!file)
        {
            std::cerr << "rel: cannot open file '" << path << "'\n";
            return 1;
        }

        std::string line;
        int line_no = 1;
        while (std::getline(file, line))
        {
            if (!line.empty())
            {
                std::cout << "--- line " << line_no << ": " << line << '\n';
                eval_line(env, line, line_no);
            }
            ++line_no;
        }
        return 0;
    }

    int run_repl()
    {
        rel::Environment env;
        rel::Environment::InitBuiltinConstants();
        rel::Environment::InitBuiltinFunctions();
        rel::Environment::RegisterLibrary(rel::function_library_sample::MakeLibrary());

        try {
            rel::Environment::LoadFromConfig(
                std::string(REL_SOURCE_DIR) + "/case/test_env.json");
        } catch (const std::exception& e) {
            std::cerr << "warning: test env load failed: " << e.what() << '\n';
        }


        std::cout << "REL interpreter.\n"
                  << "  expr        - evaluate and print\n"
                  << "  name = expr - bind and print\n"
                  << "  Ctrl+D (Unix) / Ctrl+Z (Windows) to exit.\n";

        int line_no = 1;
        while (true)
        {
#ifdef _WIN32
            // TODO: enable readline on Windows (e.g. via vcpkg port)
            std::cout << ">>> " << std::flush;
            std::string line;
            if (!std::getline(std::cin, line))
            {
                std::cout << '\n';
                break;
            }
            if (line.empty()) continue;
            eval_line(env, line, line_no);
            ++line_no;
#else
            char* raw = readline(">>> ");
            if (!raw)
            {
                std::cout << '\n';
                break;
            }
            std::string line(raw);
            if (!line.empty())
            {
                add_history(raw);
                free(raw);
                eval_line(env, line, line_no);
                ++line_no;
            }
            else
            {
                free(raw);
            }
#endif
        }
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc > 2)
    {
        std::cerr << "usage: " << (argc > 0 ? argv[0] : "rel")
                  << " [path-to-file]\n";
        return 2;
    }

    init_python_env();

    int rc;
    try
    {
        if (argc == 2)
            rc = run_file(argv[1]);
        else
            rc = run_repl();
    }
    catch (const std::exception& e)
    {
        std::cerr << "rel: fatal: " << e.what() << '\n';
        rc = 1;
    }

    shutdown_python_env();
    return rc;
}
