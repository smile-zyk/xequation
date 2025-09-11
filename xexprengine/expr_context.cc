#include "expr_context.h"
#include "dependency_graph.h"
#include "expr_common.h"
#include "variable.h"
#include <cstdint>
#include <string>

using namespace xexprengine;

void ExprContext::Update()
{
    if( graph_ )
    {
        graph_->Traversal( [&](const std::string& var_name){
          if(IsVariableExist(var_name))
          {
            const DependencyGraph::Node* node = graph_->GetNode(var_name);
            if(node && node->dirty_flag())
            {
                uint64_t max_event_stamp = 0;
                for(const std::string& dependency : node->dependencies())
                {
                    const DependencyGraph::Node* dep_node = graph_->GetNode(dependency);
                    if(dep_node)
                    {
                        if(dep_node->event_stamp() > max_event_stamp)
                        {
                            max_event_stamp = dep_node->event_stamp();
                        }
                    }
                }
                if(node->event_stamp() < max_event_stamp)
                {
                    Variable* var = GetVariable(var_name);
                    if(!var)
                    {
                        return;
                    }
                    if(var->GetType() == Variable::Type::Expr)
                    {
                        ExprVariable* expr_var = var->As<ExprVariable>();
                        if(expr_var)
                        {
                            EvalResult result = evaluate_callback_(expr_var->expression(), this);
                            if(result.value != expr_var->cached_value())
                            {
                                SetVariableValue(var_name, result.value);
                                graph_->UpdateEventStamp(var_name);
                            }
                            expr_var->SetEvalResult(result);
                        }
                    }
                    else if(var->GetType() == Variable::Type::Raw)
                    {
                        RawVariable* raw_var = var->As<RawVariable>();
                        if(raw_var)
                        {
                            if(raw_var->value() != raw_var->cached_value())
                            {
                                SetVariableValue(var_name, raw_var->value());
                                graph_->UpdateEventStamp(var_name);
                            }
                            raw_var->set_cached_value(raw_var->value());
                        }
                    }
                }
            }
          }
        });
    }
}