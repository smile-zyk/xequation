// Entry point for the REL interpreter.
//
// Modes:
//   - REPL mode (no arguments): read one expression or binding per line,
//     evaluate it, and print the result.
//   - File mode (one argument): evaluate each non-empty line from a file.

#include "environment.h"
#include "evaluator.h"
#include "rel.h"
#include "parser.h"
#include "scanner.h"
#include "function_library_sample.h"

#ifdef REL_HAS_PYTHON
#include "python_env.h"
#endif

#ifdef _WIN32
// No readline on Windows yet �?fall back to std::getline.
#elif __APPLE__
#include <editline/readline.h>
#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    // Initialize the embedded Python environment (rel_python_env).  The
    // interpreter lifecycle is owned by the host: configure the standard
    // paths (py_home + stdlib / lib-dynload / site-packages, injected by
    // CMake at build time) and start the interpreter.  A host that already
    // runs Python keeps ownership (PyEnvManager detects Py_IsInitialized()).
    void init_python_env()
    {
#ifdef REL_HAS_PYTHON
        xequation::python::PyEnvManager::SetDefaultPyEnvConfig();
        xequation::python::PyEnvManager::InitializePyEnv();
#endif
    }

    // Tear down in the reverse order: release every pybind11::function held
    // by the callback registry while the interpreter is still alive, then
    // finalize the interpreter (only when rel_python_env created it).
    void shutdown_python_env()
    {
#ifdef REL_HAS_PYTHON
        rel::Environment::CleanupPythonState();
        xequation::python::PyEnvManager::ShutdownPyEnv();
#endif
    }

    bool is_ident_start(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    bool is_ident_char(char c)
    {
        return is_ident_start(c) || (c >= '0' && c <= '9');
    }

    bool is_valid_identifier(const std::string& name)
    {
        if (name.empty() || !is_ident_start(name[0]))
            return false;
        for (std::size_t i = 1; i < name.size(); ++i)
            if (!is_ident_char(name[i]))
                return false;
        if (name == "if" || name == "then" || name == "elseif" || name == "else" ||
            name == "AND" || name == "OR" || name == "NOT" ||
            name == "EQUALS" || name == "NOTEQUALS")
            return false;
        return true;
    }

    std::size_t find_binding_eq(const std::string& line)
    {
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] != '=') continue;
            if (i > 0)
            {
                char prev = line[i - 1];
                if (prev == '=' || prev == '!' || prev == '<' || prev == '>')
                    continue;
            }
            if (i + 1 < line.size() && line[i + 1] == '=')
                continue;
            return i;
        }
        return std::string::npos;
    }

    std::string trim(const std::string& s)
    {
        std::size_t b = 0;
        while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
        std::size_t e = s.size();
        while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
        return s.substr(b, e - b);
    }

    int parse_and_eval(rel::Environment& env,
                       const std::string& source,
                       int line_no)
    {
        rel::Scanner scanner(source, line_no);
        rel::ScanResult scanResult = scanner.Scan();
        if (!scanResult.Ok())
        {
            std::cerr << scanResult.errors[0].to_string() << '\n';
            return 1;
        }

        rel::Parser parser(std::move(scanResult.tokens));
        rel::ParseResult result = parser.Parse();

        if (!result.Ok())
        {
            std::cerr << result.errors[0].to_string() << '\n';
            return 1;
        }

        rel::Evaluator evaluator(env);
        rel::Value value;
        try {
            value = evaluator.Evaluate(*result.expr);
        } catch (const std::exception& e) {
            rel::Error err;
            err.kind    = rel::ErrorKind::RunTime;
            err.line    = result.expr->line;
            err.column  = result.expr->column;
            err.message = e.what();
            std::cerr << err.to_string() << std::endl;
            return 1;
        }
        std::cout << value.Format() << '\n';
        return 0;
    }

    int eval_line(rel::Environment& env, const std::string& line, int line_no)
    {
        std::size_t eq = find_binding_eq(line);
        if (eq == std::string::npos)
            return parse_and_eval(env, line, line_no);

        std::string name = trim(line.substr(0, eq));
        std::string expr_str = trim(line.substr(eq + 1));

        if (!is_valid_identifier(name))
        {
            std::cerr << "error " << line_no << ": invalid identifier '"
                      << name << "'\n";
            return 1;
        }

        rel::Scanner scanner(expr_str, line_no);
        rel::ScanResult scanResult = scanner.Scan();
        if (!scanResult.Ok())
        {
            std::cerr << scanResult.errors[0].to_string() << '\n';
            return 1;
        }

        rel::Parser parser(std::move(scanResult.tokens));
        rel::ParseResult result = parser.Parse();

        if (!result.Ok())
        {
            std::cerr << result.errors[0].to_string() << '\n';
            return 1;
        }

        rel::Evaluator evaluator(env);
        rel::Value v;
        try {
            v = evaluator.Evaluate(*result.expr);
            env.Define(name, v);
        } catch (const std::exception& e) {
            rel::Error err;
            err.kind    = rel::ErrorKind::RunTime;
            err.line    = result.expr->line;
            err.column  = result.expr->column;
            err.message = e.what();
            std::cerr << err.to_string() << std::endl;
            return 1;
        }
        std::cout << v.Format(name) << '\n';
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
