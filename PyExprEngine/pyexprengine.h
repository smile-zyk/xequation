#pragma once
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "equation.h"
#include <iostream>

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

    void AddEquation(const std::string& name, const std::string& expression);
    void PrintEquations() const {
        for (const auto& pair : equations_map_) {
            std::cout << pair.first << " = " << pair.second->expression() << " = " << pybind11::cast<std::string>(pybind11::str(pair.second->result()))<< std::endl;
        }
    }

 private:
    void InitializePyEnv();
    void FinalizePyEnv();
    PyExprEngine();
    ~PyExprEngine();

    static PyEnvConfig config_;
    bool manage_python_context_ = false;

    std::unordered_map<std::string, std::unique_ptr<Equation>> equations_map_;
};
