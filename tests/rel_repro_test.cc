#include <gtest/gtest.h>
#include <iostream>
#include <string>

#include "core/equation_common.h"
#include "core/equation_manager.h"
#include "core/equation_value.h"
#include "equation_value_test_utils.h"

#include "environment.h"  // rel::Environment
#include "value.h"         // rel::Value

using namespace xequation;

namespace
{
void Dump(const std::string &tag, EquationManager *m)
{
    std::cout << "=== " << tag << " ===\n";
    for (const std::string &name : m->GetEquationNames())
    {
        const Equation *e = m->GetEquation(name);
        const EquationValue v = m->GetEquationValue(e->name);
        std::cout << "  " << name << " status="
                  << static_cast<int>(e->status)
                  << " hasValue=" << v.HasValue();
        if (v.HasValue())
        {
            const rel::Value &rv = v.Value();
            std::cout << " type="
                      << (rv.is_measurement() ? "Measurement" : "DataArray");
        }
        std::cout << " to_string=[" << ValueToString(v) << "]\n";
    }
    std::cout << "  env vars:";
    for (const std::string &n : m->environment().VariableNames())
        std::cout << " " << n;
    std::cout << "\n";
}
} // namespace

TEST(RelEngineRepro, RedefineChainDiagnose)
{
    EquationManager &manager = EquationManager::GetInstance(); manager.Reset();

    manager.AddEquation("a", "1");
    manager.Update();
    Dump("after a=1", &manager);

    manager.AddEquation("b", "a");
    manager.Update();
    Dump("after b=a", &manager);

    const Equation *a = manager.GetEquation("a");
    ASSERT_NE(a, nullptr);
    manager.EditEquation(a->id, "2");
    manager.Update();
    Dump("after redefine a=2", &manager);
}
