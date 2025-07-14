#include "PyExprEngine/pyexprengine.h"

int main()
{
    PyExprEngine::PyEnvConfig config;
    PyExprEngine::SetPyEnvConfig(config);
    PyExprEngine& engine = PyExprEngine::Instance();
    engine.AddEquation(std::string("x"), std::string("5"));
    engine.AddEquation(std::string("y"), std::string("x + 2"));
    engine.AddEquation(std::string("z"), std::string("y * 3"));
    engine.AddEquation(std::string("m"), std::string("max([x, y, z])"));
    engine.PrintEquations();
    return 0;
}