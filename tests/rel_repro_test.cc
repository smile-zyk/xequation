#include <gtest/gtest.h>
#include <iostream>
#include <string>

#include "core/equation.h"
#include "core/equation_common.h"
#include "core/equation_manager.h"
#include "rel_engine/rel_equation_engine.h"
#include "rel_engine/rel_equation_context.h"

using namespace xequation;
using namespace xequation::rel_engine;

namespace
{
void Dump(const std::string &tag, EquationManager *m)
{
    std::cout << "=== " << tag << " ===\n";
    for (const std::string &name : m->GetEquationNames())
    {
        const Equation *e = m->GetEquation(name);
        const EquationValue &v = e->GetValue();
        std::cout << "  " << name << " status="
                  << static_cast<int>(e->status())
                  << " isRel=" << v.IsRelValue()
                  << " isNull=" << v.IsNull();
        if (v.IsRelValue())
        {
            const rel::Value &rv = v.AsRel();
            std::cout << " type="
                      << (rv.is_measurement() ? "Measurement" : "DataArray");
        }
        std::cout << " to_string=[" << v.ToString() << "]\n";
    }
    const RelEquationContext *rc = dynamic_cast<const RelEquationContext *>(&m->context());
    if (rc)
    {
        std::cout << "  env vars:";
        for (const std::string &n : rc->env().VariableNames())
            std::cout << " " << n;
        std::cout << "\n";
    }
}
} // namespace

TEST(RelEngineRepro, RedefineChainDiagnose)
{
    auto manager = RelEquationEngine::GetInstance().CreateEquationManager();

    manager->AddEquation("a", "1");
    manager->Update();
    Dump("after a=1", manager.get());

    manager->AddEquation("b", "a");
    manager->Update();
    Dump("after b=a", manager.get());

    const Equation *a = manager->GetEquation("a");
    ASSERT_NE(a, nullptr);
    manager->EditSingleEquation(a->group_id(), "a", "2");
    manager->Update();
    Dump("after redefine a=2", manager.get());
}
