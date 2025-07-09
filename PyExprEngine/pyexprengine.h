#pragma once
#include <string>
#include <vector>

class PyExprEngine {
 public:
    struct PyEnvConfig {
        std::string py_home;
        std::vector<std::string> lib_path_list;
    };
    static PyExprEngine& Instance();
    static void SetPyEnvConfig(const PyEnvConfig& config);
    PyExprEngine(const PyExprEngine&) = delete;
    PyExprEngine& operator=(const PyExprEngine&) = delete;

 private:
    void InitializePyEnv();
    void FinalizePyEnv();
    PyExprEngine();
    ~PyExprEngine();

    static PyEnvConfig config_;
    bool manage_python_context_ = false;
};
