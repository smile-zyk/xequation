#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include "expr_common.h"
#include "expr_value.h"
#include "expression.h"

namespace xexprengine 
{
    class ExprEngine;

    // support for expression dependency tracking
    // support set and reset node dependencies (e.g. SetNodeDependencies(a, {b,c}))
    // support keep active dependencies (e.g. if b is remove, a's active dependencies will be {c}, but when b is re-added, a's active dependencies will include b again)
    class ExprDependencyGraph
    {
        public:
            void AddNode(const std::string& var_name);
            void RemoveNode(const std::string& var_name);
            void AddDependency(const std::string& from_var, const std::string& to_var);
            void RemoveDependency(const std::string& from_var, const std::string& to_var);
            void ClearNodeDependencies(const std::string& var_name);
            void SetNodeDependencies(const std::string& var_name, const std::unordered_set<std::string>& dependency_vars);

            const std::unordered_set<std::string>& GetActiveNodeDependencies(const std::string& var_name) const;
            const std::unordered_set<std::string>& GetActiveNodeDependents(const std::string& var_name) const;

            const std::unordered_set<std::string>& GetNodeDependencies(const std::string& var_name) const;
        private:
            struct ExprNode {
                std::unordered_set<std::string> active_dependencies; // Variables this node depends on
                std::unordered_set<std::string> active_dependents;   // Variables that depend on this node
                bool is_dirty = false;
            };
            // store active dependencies
            // active means the dependencies or dependents that are currently existing
            std::unordered_map<std::string, ExprNode> nodes;
            // store all dependencies
            std::unordered_map<std::string, std::unordered_set<std::string>> node_dependencies_;
    };

    class ExprContext
    {
        public:
            ExprContext() = default;

            void SetVariable(const std::string& var_name, const ExprValue& value);
            ExprValue GetVariable(const std::string& var_name);
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
            ExprValue Evaluate(Expression* expr);

            const std::string& name() { return name_; }
        private:
            struct ExprNode {
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