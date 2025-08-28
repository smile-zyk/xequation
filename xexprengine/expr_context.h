#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <unordered_set>
#include "expr_common.h"
#include "value.h"
#include "expression.h"

namespace xexprengine 
{
    class ExprEngine;

    class ExprContext
    {
        public:
            ExprContext() = default;

            void SetVariable(const std::string& var_name, const Value& value);
            Value GetVariable(const std::string& var_name);
            void RemoveVariable(const std::string& var_name);
            void RenameVariable(const std::string& old_name, const std::string& new_name);
            
            void SetExpression(const std::string& expr_name, std::string expr_str);
            Expression* GetExpression(const std::string& expr_name) const;
            
            void MakeVariableDirty();
            bool IsVariableDirty(const std::string& var_name) const;
            
            std::unordered_set<std::string> GetVariableDependents(const std::string& var_name) const;
            std::unordered_set<std::string> GetVariableDependencies(const std::string& var_name) const;
            
            bool IsVariableExist(const std::string& var_name) const;

            void TraverseVariable(const std::string& var_name, const std::function<void(ExprContext*, const std::string&)>& func);
            void UpdateVariableGraph();

            EvalResult Evaluate(const std::string& expr_str);
            Value Evaluate(Expression* expr);

            const std::string& name() { return name_; }
        private:
            struct ExprNode 
            {
                std::unordered_set<std::string> dependencies; // Variables this node depends on
                std::unordered_set<std::string> dependents;   // Variables that depend on this node
                bool is_dirty = false;
            };

            std::unordered_map<std::string, std::unique_ptr<Expression>> expr_map_;
            std::unordered_set<std::string> var_set_;
            std::unordered_map<std::string, ExprNode> var_graph_;
            std::string name_;
            ExprEngine* engine_ = nullptr;
    };
}