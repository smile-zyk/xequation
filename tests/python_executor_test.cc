#include "core/equation_common.h"
#include "python/python_executor.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include <pybind11/pytypes.h>

using namespace xequation;
using namespace xequation::python;

class PythonExecutorTest : public ::testing::Test
{
  protected:
    virtual void SetUp()
    {
        executor_.reset(new PythonExecutor());
    }

    virtual void TearDown()
    {
        executor_.reset();
    }

    std::unique_ptr<PythonExecutor> executor_;
};

TEST_F(PythonExecutorTest, BasicOperations) {
  py::dict locals;
  
  auto result1 = executor_->Exec("x = 5 + 3", locals);
  EXPECT_EQ(result1.status, Equation::Status::kSuccess);
  EXPECT_TRUE(result1.message.empty());
  EXPECT_EQ(py::cast<int>(locals["x"]), 8);
  
  auto result2 = executor_->Eval("x * 2", locals);
  EXPECT_EQ(result2.status, Equation::Status::kSuccess);
  EXPECT_TRUE(result2.message.empty());
  EXPECT_EQ(py::cast<int>(result2.value.Cast<py::object>()), 16);
  
  locals["a"] = 10;
  locals["b"] = 2;
  auto result3 = executor_->Exec("c = a * b + x", locals);
  EXPECT_EQ(result3.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(locals["c"]), 28);
  
  auto result4 = executor_->Eval("c + 4", locals);
  EXPECT_EQ(result4.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(result4.value.Cast<py::object>()), 32);
  
  auto result5 = executor_->Eval("5 > 3 and 2 < 4", locals);
  EXPECT_EQ(result5.status, Equation::Status::kSuccess);
  EXPECT_TRUE(py::cast<bool>(result5.value.Cast<py::object>()));
  
  locals["name"] = "world";
  auto result6 = executor_->Eval("'Hello, ' + name", locals);
  EXPECT_EQ(result6.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<std::string>(result6.value.Cast<py::object>()), "Hello, world");
}

TEST_F(PythonExecutorTest, VariableDefinitionAndAssignment) {
  py::dict locals;
  
  auto result1 = executor_->Exec("x = 42", locals);
  EXPECT_EQ(result1.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(locals["x"]), 42);
  
  auto result2 = executor_->Eval("x + 8", locals);
  EXPECT_EQ(result2.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(result2.value.Cast<py::object>()), 50);
  
  auto result3 = executor_->Exec("name = 'test'", locals);
  EXPECT_EQ(result3.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<std::string>(locals["name"]), "test");
  
  auto result4 = executor_->Eval("name.upper()", locals);
  EXPECT_EQ(result4.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<std::string>(result4.value.Cast<py::object>()), "TEST");
  
  auto result5 = executor_->Exec("numbers = [1, 2, 3]", locals);
  EXPECT_EQ(result5.status, Equation::Status::kSuccess);
  auto result6 = executor_->Eval("len(numbers)", locals);
  EXPECT_EQ(result6.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(result6.value.Cast<py::object>()), 3);
  
  auto result7 = executor_->Exec("person = {'name': 'Alice', 'age': 25}", locals);
  EXPECT_EQ(result7.status, Equation::Status::kSuccess);
  auto result8 = executor_->Eval("person['name']", locals);
  EXPECT_EQ(result8.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<std::string>(result8.value.Cast<py::object>()), "Alice");
  
  auto result9 = executor_->Exec("counter = 10", locals);
  EXPECT_EQ(result9.status, Equation::Status::kSuccess);
  auto result10 = executor_->Exec("counter += 5", locals);
  EXPECT_EQ(result10.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(locals["counter"]), 15);
  
  auto result11 = executor_->Eval("counter * 2", locals);
  EXPECT_EQ(result11.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(result11.value.Cast<py::object>()), 30);
}

TEST_F(PythonExecutorTest, FunctionDefinitionAndUsage) {
  py::dict locals;
  
  auto result1 = executor_->Exec(R"(
def add_numbers(a, b):
    return a + b
)", locals);
  EXPECT_EQ(result1.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("add_numbers"));
  
  auto result2 = executor_->Eval("add_numbers(5, 3)", locals);
  EXPECT_EQ(result2.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(result2.value.Cast<py::object>()), 8);
  
  auto result3 = executor_->Exec(R"(
def get_message():
    return "Hello, World!"
)", locals);
  EXPECT_EQ(result3.status, Equation::Status::kSuccess);
  
  auto result4 = executor_->Eval("get_message()", locals);
  EXPECT_EQ(result4.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<std::string>(result4.value.Cast<py::object>()), "Hello, World!");
  
  auto result5 = executor_->Exec("square = lambda x: x * x", locals);
  EXPECT_EQ(result5.status, Equation::Status::kSuccess);
  
  auto result6 = executor_->Eval("square(4)", locals);
  EXPECT_EQ(result6.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(result6.value.Cast<py::object>()), 16);
  
  locals["base"] = 10;
  auto result7 = executor_->Exec(R"(
def add_base(x):
    return x + base
)", locals);
  EXPECT_EQ(result7.status, Equation::Status::kSuccess);
  
  auto result8 = executor_->Exec("b = add_base(5)", locals);
  EXPECT_EQ(result8.status, Equation::Status::kSuccess);
}

TEST_F(PythonExecutorTest, ClassDefinitionAndUsage) {
  py::dict locals;
  
  auto result1 = executor_->Exec(R"(
class Person:
    def __init__(self, name):
        self.name = name
    
    def greet(self):
        return f"Hello, {self.name}"
)", locals);
  EXPECT_EQ(result1.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("Person"));
  
  auto result2 = executor_->Exec("alice = Person('Alice')", locals);
  EXPECT_EQ(result2.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("alice"));
  
  auto result3 = executor_->Eval("alice.greet()", locals);
  EXPECT_EQ(result3.status, Equation::Status::kSuccess);
  EXPECT_NE(py::cast<std::string>(result3.value.Cast<py::object>()).find("Alice"), std::string::npos);
  
  auto result4 = executor_->Eval("alice.name", locals);
  EXPECT_EQ(result4.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<std::string>(result4.value.Cast<py::object>()), "Alice");
  
  auto result5 = executor_->Exec("class EmptyClass: pass", locals);
  EXPECT_EQ(result5.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("EmptyClass"));
  
  auto result6 = executor_->Exec("empty_obj = EmptyClass()", locals);
  EXPECT_EQ(result6.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("empty_obj"));
  
  auto result7 = executor_->Exec("empty_obj.value = 100", locals);
  EXPECT_EQ(result7.status, Equation::Status::kSuccess);
  
  auto result8 = executor_->Eval("empty_obj.value", locals);
  EXPECT_EQ(result8.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(result8.value.Cast<py::object>()), 100);
}

TEST_F(PythonExecutorTest, ModuleImportAndUsage) {
  py::dict locals;
  
  auto result1 = executor_->Exec("import math", locals);
  EXPECT_EQ(result1.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("math"));
  
  auto result2 = executor_->Eval("math.sqrt(16)", locals);
  EXPECT_EQ(result2.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<double>(result2.value.Cast<py::object>()), 4.0);
  
  auto result3 = executor_->Exec("import datetime as dt", locals);
  EXPECT_EQ(result3.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("dt"));
  
  auto result4 = executor_->Eval("dt.datetime.now().year > 2020", locals);
  EXPECT_EQ(result4.status, Equation::Status::kSuccess);
  EXPECT_TRUE(py::cast<bool>(result4.value.Cast<py::object>()));
  
  auto result5 = executor_->Exec("from math import sqrt", locals);
  EXPECT_EQ(result5.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("sqrt"));
  
  auto result6 = executor_->Eval("sqrt(9)", locals);
  EXPECT_EQ(result6.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<double>(result6.value.Cast<py::object>()), 3.0);
  
  auto result7 = executor_->Exec("from math import pi", locals);
  EXPECT_EQ(result7.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("pi"));
  
  auto result8 = executor_->Eval("pi > 3.14", locals);
  EXPECT_EQ(result8.status, Equation::Status::kSuccess);
  EXPECT_TRUE(py::cast<bool>(result8.value.Cast<py::object>()));
  
  auto result9 = executor_->Exec("from math import factorial as fact", locals);
  EXPECT_EQ(result9.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("fact"));
  
  auto result10 = executor_->Eval("fact(5)", locals);
  EXPECT_EQ(result10.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(result10.value.Cast<py::object>()), 120);
}

TEST_F(PythonExecutorTest, ErrorHandling) {
  py::dict locals;
  
  auto result1 = executor_->Exec("invalid syntax!", locals);
  EXPECT_EQ(result1.status, Equation::Status::kSyntaxError);
  EXPECT_FALSE(result1.message.empty());
  
  auto result2 = executor_->Eval("5 +", locals);
  EXPECT_EQ(result2.status, Equation::Status::kSyntaxError);
  EXPECT_FALSE(result2.message.empty());
  
  auto result3 = executor_->Exec("undefined_function()", locals);
  EXPECT_EQ(result3.status, Equation::Status::kNameError);
  EXPECT_FALSE(result3.message.empty());
  
  auto result4 = executor_->Eval("undefined_variable", locals);
  EXPECT_EQ(result4.status, Equation::Status::kNameError);
  EXPECT_FALSE(result4.message.empty());
  
  auto result5 = executor_->Exec("'str' + 123", locals);
  EXPECT_EQ(result5.status, Equation::Status::kTypeError);
  EXPECT_FALSE(result5.message.empty());
  
  auto result6 = executor_->Exec("1 / 0", locals);
  EXPECT_EQ(result6.status, Equation::Status::kZeroDivisionError);
  EXPECT_FALSE(result6.message.empty());
  
  auto result7 = executor_->Eval("[1, 2, 3][10]", locals);
  EXPECT_EQ(result7.status, Equation::Status::kIndexError);
  EXPECT_FALSE(result7.message.empty());
  
  auto result8 = executor_->Eval("{'a': 1}['b']", locals);
  EXPECT_EQ(result8.status, Equation::Status::kKeyError);
  EXPECT_FALSE(result8.message.empty());
}

TEST_F(PythonExecutorTest, ComplexScenarios) {
  py::dict locals;
  
  auto result1 = executor_->Exec("base_value = 10", locals);
  EXPECT_EQ(result1.status, Equation::Status::kSuccess);
  
  auto result2 = executor_->Exec(R"(
def calculate(x):
    return x * 2 + base_value
)", locals);
  EXPECT_EQ(result2.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("calculate"));
  
  auto result3 = executor_->Exec("import math", locals);
  EXPECT_EQ(result3.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("math"));
  
  auto result4 = executor_->Exec("from math import sqrt", locals);
  EXPECT_EQ(result4.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("sqrt"));
  
  auto result5 = executor_->Exec(R"(
class Processor:
    def process(self, value):
        return calculate(value) ** 2
)", locals);
  EXPECT_EQ(result5.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("Processor"));
  
  auto result6 = executor_->Exec("processor = Processor()", locals);
  EXPECT_EQ(result6.status, Equation::Status::kSuccess);
  EXPECT_TRUE(locals.contains("processor"));
  
  auto eval_result = executor_->Eval("sqrt(processor.process(5))", locals);
  EXPECT_EQ(eval_result.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<double>(eval_result.value.Cast<py::object>()), std::sqrt(400.0));
}

TEST_F(PythonExecutorTest, EvalReturnTypes) {
  py::dict locals;
  
  auto result1 = executor_->Eval("42", locals);
  EXPECT_EQ(result1.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<int>(result1.value.Cast<py::object>()), 42);
  
  auto result2 = executor_->Eval("3.14", locals);
  EXPECT_EQ(result2.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<double>(result2.value.Cast<py::object>()), 3.14);
  
  auto result3 = executor_->Eval("'hello'", locals);
  EXPECT_EQ(result3.status, Equation::Status::kSuccess);
  EXPECT_EQ(py::cast<std::string>(result3.value.Cast<py::object>()), "hello");
  
  auto result4 = executor_->Eval("True", locals);
  EXPECT_EQ(result4.status, Equation::Status::kSuccess);
  EXPECT_TRUE(py::cast<bool>(result4.value.Cast<py::object>()));
  
  auto result5 = executor_->Eval("None", locals);
  EXPECT_EQ(result5.status, Equation::Status::kSuccess);
  EXPECT_TRUE(result5.value.Cast<py::object>().is_none());
  
  auto result6 = executor_->Eval("[1, 2, 3]", locals);
  EXPECT_EQ(result6.status, Equation::Status::kSuccess);
  auto list = result6.value.Cast<py::object>().cast<py::list>();
  EXPECT_EQ(py::len(list), 3);
  
  auto result7 = executor_->Eval("{'key': 'value'}", locals);
  EXPECT_EQ(result7.status, Equation::Status::kSuccess);
  auto dict = result7.value.Cast<py::object>().cast<py::dict>();
  EXPECT_EQ(py::cast<std::string>(dict["key"]), "value");
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    py::scoped_interpreter guard{};
    int ret = RUN_ALL_TESTS();
    return ret;
}