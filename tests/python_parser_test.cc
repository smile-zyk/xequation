#include "core/equation.h"
#include "core/equation_common.h"
#include "python/python_parser.h"
#include "python/python_equation_engine.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <pybind11/embed.h>

/**
 * Unit tests for PythonParser
 * 
 * Test Organization:
 * 1. Basic Statement Parsing - Simple assignments, imports, functions, classes
 * 2. Error Handling - Invalid syntax, unsupported statements, edge cases
 * 3. Complex Expressions - Numeric, comprehensions, conditionals, function chains
 * 4. Comments and Special Characters
 * 5. Caching Functionality
 * 6. Multi-statement Parsing and Splitting
 * 7. Import Statement Variations
 * 8. Expression Parsing (ParseExpression API)
 * 9. Type Annotations (AnnAssign)
 * 10. Nested Scopes and Dependencies
 * 11. Comprehensions and Generators
 * 12. Attribute Chains and Method Calls
 * 13. Walrus Operator (:=)
 * 14. Edge Cases and Unsupported Statements
 * 15. Special Python Features (f-strings, slicing, operators)
 * 16. Multi-line and Async Code
 * 17. Extended Cache Tests
 */

using namespace xequation;
using namespace xequation::python;

class PythonParserTest : public ::testing::Test
{
  protected:
    virtual void SetUp()
    {
        PythonEquationEngine::SetDefaultPyEnvConfig();
        parser_.reset(new PythonParser());
    }

    virtual void TearDown()
    {
        parser_.reset();
    }

    std::unique_ptr<PythonParser> parser_;
};

// ============================================================================
// Basic Statement Parsing Tests
// ============================================================================

TEST_F(PythonParserTest, ParseSimpleAssignment)
{
    auto parse_result = parser_->ParseSingleStatement("a = b + c");
    EXPECT_EQ(parse_result.items.size(), 1);
    auto item = parse_result.items[0];
    EXPECT_EQ(item.name, "a");
    EXPECT_THAT(item.dependencies, testing::UnorderedElementsAre("b", "c"));
    EXPECT_EQ(item.type, ItemType::kVariable);
    EXPECT_EQ(item.content, "b + c");
}

TEST_F(PythonParserTest, ParseImport)
{
    auto parse_result = parser_->ParseSingleStatement("import math as mt");
    EXPECT_EQ(parse_result.items.size(), 1);
    auto item = parse_result.items[0];
    EXPECT_EQ(item.name, "mt");
    EXPECT_THAT(item.dependencies, testing::IsEmpty());
    EXPECT_EQ(item.type, ItemType::kImport);
    EXPECT_EQ(item.content, "import math as mt");
}

TEST_F(PythonParserTest, ParseFromImport)
{
    auto parse_result = parser_->ParseSingleStatement("from math import cos");
    EXPECT_EQ(parse_result.items.size(), 1);
    auto item = parse_result.items[0];
    EXPECT_EQ(item.name, "cos");
    EXPECT_THAT(item.dependencies, testing::IsEmpty());
    EXPECT_EQ(item.type, ItemType::kImportFrom);
    EXPECT_EQ(item.content, "from math import cos");
}

TEST_F(PythonParserTest, ParseFunction)
{
    auto parse_result = parser_->ParseSingleStatement("def hello(): print('Hello World')");
    EXPECT_EQ(parse_result.items.size(), 1);
    auto item = parse_result.items[0];
    EXPECT_EQ(item.name, "hello");
    EXPECT_THAT(item.dependencies, testing::UnorderedElementsAre("print"));
    EXPECT_EQ(item.type, ItemType::kFunction);
    EXPECT_EQ(item.content, "def hello(): print('Hello World')");
}

TEST_F(PythonParserTest, ParseClass)
{
    auto parse_result = parser_->ParseSingleStatement("class Person: pass");
    EXPECT_EQ(parse_result.items.size(), 1);
    auto item = parse_result.items[0];
    EXPECT_EQ(item.name, "Person");
    EXPECT_THAT(item.dependencies, testing::IsEmpty());
    EXPECT_EQ(item.type, ItemType::kClass);
    EXPECT_EQ(item.content, "class Person: pass");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(PythonParserTest, ParseErrorUnsupportedStatement)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("a+b"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseClassWithOuterDependency)
{

    auto parse_result = parser_->ParseSingleStatement(R"(class SimpleTest:
    
    def __init__(self, name="Test"):
        self.name = name
    
    def greet(self):
        return f"Hello, {self.name}!"
    
    def add(self, a, b):
        t = 1
        return a + b + math.cos(0))");

    EXPECT_EQ(parse_result.items.size(), 1);
    auto item = parse_result.items[0];
    EXPECT_EQ(item.name, "SimpleTest");
    EXPECT_THAT(item.dependencies, testing::UnorderedElementsAre("math", "math.cos"));
    EXPECT_EQ(item.type, ItemType::kClass);
}

TEST_F(PythonParserTest, ParseErrorInvalidSyntax)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("b = te+"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseErrorMultipleAssignment)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("a,b = 1,2"),
        ParseException
    );
}

// ============================================================================
// Complex Expression Tests
// ============================================================================

TEST_F(PythonParserTest, ParseComplexNumeric)
{
    auto result = parser_->ParseSingleStatement("a = 3 + 4j + 2 - 1j + 1 + 2j");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "a");
    EXPECT_THAT(result.items[0].dependencies, testing::IsEmpty());
    EXPECT_EQ(result.items[0].type, ItemType::kVariable);
    EXPECT_EQ(result.items[0].content, "3 + 4j + 2 - 1j + 1 + 2j");
}

TEST_F(PythonParserTest, ParseComplexListComprehension)
{
    auto result = parser_->ParseSingleStatement(
        "matrix = [[(i * j) + (i - j) * 1j for j in range(1, 4)] for i in range(1, 5)]"
    );
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "matrix");
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("range"));
    EXPECT_EQ(result.items[0].type, ItemType::kVariable);
}

TEST_F(PythonParserTest, ParseComplexConditional)
{
    auto result = parser_->ParseSingleStatement(
        "condition = (global_flag := a) and any(complex(i, j).real > 0 for i in range(1, 10, 3) for j in range(1, 10, 4))"
    );
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "condition");
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("a","any", "complex", "range"));
    EXPECT_EQ(result.items[0].type, ItemType::kVariable);
}

TEST_F(PythonParserTest, ParseFunctionCallChain)
{
    auto result = parser_->ParseSingleStatement(
        "func = process_data(filter_data(load_data(\"file.txt\")))"
    );
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "func");
    EXPECT_THAT(result.items[0].dependencies, 
                testing::UnorderedElementsAre("process_data", "filter_data", "load_data"));
    EXPECT_EQ(result.items[0].type, ItemType::kVariable);
}

// ============================================================================
// Comment and Special Character Tests
// ============================================================================

TEST_F(PythonParserTest, ParseEmptyString)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement(""),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseWhitespaceOnly)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("   \t\n   "),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseCommentOnly)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("# This is a comment"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseWithComments)
{
    auto result = parser_->ParseSingleStatement("a = b + c  # add b and c");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "a");
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("b", "c"));
}

TEST_F(PythonParserTest, ParseSpecialCharacters)
{
    auto result = parser_->ParseSingleStatement("_private_var = public_var * 2");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "_private_var");
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("public_var"));
}

// ============================================================================
// Cache Tests
// ============================================================================

TEST_F(PythonParserTest, CacheBasicFunctionality)
{
    std::string code = "t = x + y";

    auto result1 = parser_->ParseSingleStatement(code);
    EXPECT_EQ(parser_->GetParseResultCacheSize(), 1u);

    auto result2 = parser_->ParseSingleStatement(code);
    EXPECT_EQ(parser_->GetParseResultCacheSize(), 1u);

    EXPECT_EQ(result1.items.size(), result2.items.size());
    EXPECT_EQ(result1.items[0].name, result2.items[0].name);
    EXPECT_EQ(result1.items[0].content, result2.items[0].content);
}

TEST_F(PythonParserTest, CacheDifferentExpressions)
{
    parser_->ParseSingleStatement("t0 = x + y");
    EXPECT_EQ(parser_->GetParseResultCacheSize(), 1u);

    parser_->ParseSingleStatement("t1 = a * b");
    EXPECT_EQ(parser_->GetParseResultCacheSize(), 2u);

    parser_->ParseSingleStatement("t2 = func(param)");
    EXPECT_EQ(parser_->GetParseResultCacheSize(), 3u);

    parser_->ParseSingleStatement("t0 = x + y");
    EXPECT_EQ(parser_->GetParseResultCacheSize(), 3u);
}

TEST_F(PythonParserTest, CacheWithDifferentContent)
{
    std::string code1 = "t = x + y";
    std::string code2 = "t = x + y ";

    parser_->ParseSingleStatement(code1);
    size_t size1 = parser_->GetParseResultCacheSize();

    parser_->ParseSingleStatement(code2);
    size_t size2 = parser_->GetParseResultCacheSize();

    EXPECT_EQ(size2, size1);
}

// ============================================================================
// Multi-statement Parsing and Splitting Tests
// ============================================================================

TEST_F(PythonParserTest, ParseMultipleStatements)
{
    auto results = parser_->ParseStatements("a = 1\nb = a + 2\nc = b * 3");
    EXPECT_EQ(results.items.size(), 3);
    
    EXPECT_EQ(results.items[0].name, "a");
    EXPECT_EQ(results.items[1].name, "b");
    EXPECT_EQ(results.items[2].name, "c");
    
    EXPECT_THAT(results.items[1].dependencies, testing::UnorderedElementsAre("a"));
    EXPECT_THAT(results.items[2].dependencies, testing::UnorderedElementsAre("b"));
}

TEST_F(PythonParserTest, ParseMultipleStatementsWithError)
{
    EXPECT_THROW(
        parser_->ParseStatements("a = 1\ninvalid syntax\nc = 3"),
        ParseException
    );
}

TEST_F(PythonParserTest, SplitStatementsBasic)
{
    auto statements = parser_->SplitStatements("a = 1\nb = 2\nc = 3");
    EXPECT_THAT(statements, testing::ElementsAre("a = 1", "b = 2", "c = 3"));
}

TEST_F(PythonParserTest, SplitStatementsWithSemicolons)
{
    auto statements = parser_->SplitStatements("a = 1; b = 2; c = 3");
    EXPECT_THAT(statements, testing::ElementsAre("a = 1", "b = 2", "c = 3"));
}

TEST_F(PythonParserTest, SplitStatementsMixed)
{
    auto statements = parser_->SplitStatements("a = 1\nb = 2; c = 3\nd = 4");
    EXPECT_THAT(statements, testing::ElementsAre("a = 1", "b = 2", "c = 3", "d = 4"));
}

TEST_F(PythonParserTest, SplitStatementsComplex)
{
    auto statements = parser_->SplitStatements(
        "import math\n"
        "def calculate(x):\n"
        "    return x * 2\n"
        "result = calculate(5)"
    );
    
    EXPECT_EQ(statements.size(), 3);
    EXPECT_TRUE(statements[0].find("import math") != std::string::npos);
    EXPECT_TRUE(statements[1].find("def calculate") != std::string::npos);
    EXPECT_TRUE(statements[2].find("result = calculate") != std::string::npos);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(PythonParserTest, CachePerformance)
{
    std::string complex_expr = "t = sqrt(x*x + y*y) + sin(angle) * cos(angle)";

    auto start1 = std::chrono::high_resolution_clock::now();
    auto result1 = parser_->ParseSingleStatement(complex_expr);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    auto start2 = std::chrono::high_resolution_clock::now();
    auto result2 = parser_->ParseSingleStatement(complex_expr);
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    EXPECT_LE(duration2.count(), duration1.count() * 10);

    EXPECT_EQ(result1.items.size(), result2.items.size());
    EXPECT_EQ(result1.items[0].name, result2.items[0].name);
    EXPECT_EQ(result1.items[0].content, result2.items[0].content);
}

// ============================================================================
// Import Statement Tests
// ============================================================================

TEST_F(PythonParserTest, ParseMultipleImport)
{
    auto parse_result = parser_->ParseSingleStatement("import os, math");
    EXPECT_EQ(parse_result.items.size(), 2);
    
    EXPECT_EQ(parse_result.items[0].name, "os");
    EXPECT_EQ(parse_result.items[0].type, ItemType::kImport);
    EXPECT_EQ(parse_result.items[0].content, "import os");
    
    EXPECT_EQ(parse_result.items[1].name, "math");
    EXPECT_EQ(parse_result.items[1].type, ItemType::kImport);
    EXPECT_EQ(parse_result.items[1].content, "import math");
}

TEST_F(PythonParserTest, ParseMultipleImportWithAliases)
{
    auto parse_result = parser_->ParseSingleStatement("import os as operating_system, math as mathematics");
    EXPECT_EQ(parse_result.items.size(), 2);
    
    EXPECT_EQ(parse_result.items[0].name, "operating_system");
    EXPECT_EQ(parse_result.items[0].type, ItemType::kImport);
    EXPECT_EQ(parse_result.items[0].content, "import os as operating_system");
    
    EXPECT_EQ(parse_result.items[1].name, "mathematics");
    EXPECT_EQ(parse_result.items[1].type, ItemType::kImport);
    EXPECT_EQ(parse_result.items[1].content, "import math as mathematics");
}

TEST_F(PythonParserTest, ParseMultipleFromImport)
{
    auto parse_result = parser_->ParseSingleStatement("from math import sin, cos");
    EXPECT_EQ(parse_result.items.size(), 2);
    
    EXPECT_EQ(parse_result.items[0].name, "sin");
    EXPECT_EQ(parse_result.items[0].type, ItemType::kImportFrom);
    EXPECT_EQ(parse_result.items[0].content, "from math import sin");
    
    EXPECT_EQ(parse_result.items[1].name, "cos"); 
    EXPECT_EQ(parse_result.items[1].type, ItemType::kImportFrom);
    EXPECT_EQ(parse_result.items[1].content, "from math import cos");
}

TEST_F(PythonParserTest, ParseMultipleFromImportWithAliases)
{
    auto parse_result = parser_->ParseSingleStatement("from math import sin as sine, cos as cosine");
    EXPECT_EQ(parse_result.items.size(), 2);
    
    EXPECT_EQ(parse_result.items[0].name, "sine");
    EXPECT_EQ(parse_result.items[0].type, ItemType::kImportFrom);
    EXPECT_EQ(parse_result.items[0].content, "from math import sin as sine");
    
    EXPECT_EQ(parse_result.items[1].name, "cosine");
    EXPECT_EQ(parse_result.items[1].type, ItemType::kImportFrom);
    EXPECT_EQ(parse_result.items[1].content, "from math import cos as cosine");
}

TEST_F(PythonParserTest, ParseMixedImportStatements)
{
    auto results = parser_->ParseStatements(
        "import os\n"
        "import math as m\n"
        "from sys import version\n"
        "from os.path import join, exists\n"
    );
    
    EXPECT_GE(results.items.size(), 5); 
    
    bool has_os = false;
    bool has_math_alias = false;
    bool has_version = false;
    bool has_join = false;
    bool has_exists = false;
    
    for (const auto& eqn : results.items) {
        if (eqn.name == "os" && eqn.type == ItemType::kImport) {
            has_os = true;
        }
        if (eqn.name == "m" && eqn.type == ItemType::kImport) {
            has_math_alias = true;
        }
        if (eqn.name == "version" && eqn.type == ItemType::kImportFrom) {
            has_version = true;
        }
        if (eqn.name == "join" && eqn.type == ItemType::kImportFrom) {
            has_join = true;
        }
        if (eqn.name == "exists" && eqn.type == ItemType::kImportFrom) {
            has_exists = true;
        }
    }
    
    EXPECT_TRUE(has_os);
    EXPECT_TRUE(has_math_alias);
    EXPECT_TRUE(has_version);
    EXPECT_TRUE(has_join);
    EXPECT_TRUE(has_exists);
}

TEST_F(PythonParserTest, ParseComplexImportScenarios)
{
    auto results = parser_->ParseStatements(
        "import os.path as ospath\n"
        "from os.path import dirname, basename\n"
        "import numpy as np\n"
        "from numpy import array, matrix"
    );
    
    EXPECT_GE(results.items.size(), 5);
    
    bool has_ospath = false;
    bool has_np = false;
    
    for (const auto& eqn : results.items) {
        if (eqn.name == "ospath") {
            has_ospath = true;
            EXPECT_EQ(eqn.type, ItemType::kImport);
        }
        if (eqn.name == "np") {
            has_np = true;
            EXPECT_EQ(eqn.type, ItemType::kImport);
        }
    }
    
    EXPECT_TRUE(has_ospath);
    EXPECT_TRUE(has_np);
}

// ============================================================================
// Expression Parsing Tests
// ============================================================================

TEST_F(PythonParserTest, ParseExpressionSimple)
{
    auto result = parser_->ParseExpression("a + b");
    EXPECT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "__expression__");
    EXPECT_EQ(result.items[0].type, ItemType::kExpression);
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("a", "b"));
}

TEST_F(PythonParserTest, ParseExpressionWithAttributes)
{
    auto result = parser_->ParseExpression("math.sqrt(x) + obj.value");
    EXPECT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, 
                testing::UnorderedElementsAre("math", "math.sqrt", "x", "obj", "obj.value"));
}

TEST_F(PythonParserTest, ParseExpressionWithNestedAttributes)
{
    auto result = parser_->ParseExpression("obj.data.values.sum()");
    EXPECT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies,
                testing::IsSupersetOf({"obj", "obj.data", "obj.data.values", "obj.data.values.sum"}));
}

TEST_F(PythonParserTest, ParseExpressionWithFunctionCallChain)
{
    auto result = parser_->ParseExpression("func1(func2(x)).result");
    EXPECT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, testing::IsSupersetOf({"func1", "func2", "x"}));
}

TEST_F(PythonParserTest, ParseExpressionInvalidSyntax)
{
    auto result = parser_->ParseExpression("a +");
    EXPECT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].type, ItemType::kError);
    EXPECT_NE(result.items[0].status, ResultStatus::kSuccess);
}

TEST_F(PythonParserTest, ParseExpressionEmpty)
{
    auto result = parser_->ParseExpression("");
    EXPECT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].type, ItemType::kError);
}

TEST_F(PythonParserTest, ParseExpressionLambda)
{
    auto result = parser_->ParseExpression("lambda x: x + y");
    EXPECT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("y"));
}

// ============================================================================
// Type Annotation Tests
// ============================================================================

TEST_F(PythonParserTest, ParseAnnotatedAssignmentWithValue)
{
    auto result = parser_->ParseSingleStatement("x: int = 10");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "x");
    EXPECT_EQ(result.items[0].type, ItemType::kVariable);
    EXPECT_THAT(result.items[0].dependencies, testing::IsEmpty());
}

TEST_F(PythonParserTest, ParseAnnotatedAssignmentWithExpression)
{
    auto result = parser_->ParseSingleStatement("result: float = a + b * c");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "result");
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("a", "b", "c"));
}

TEST_F(PythonParserTest, ParseAnnotatedAssignmentWithoutValue)
{
    auto result = parser_->ParseSingleStatement("x: int");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "x");
    EXPECT_EQ(result.items[0].type, ItemType::kVariable);
    EXPECT_THAT(result.items[0].dependencies, testing::IsEmpty());
}

TEST_F(PythonParserTest, ParseAnnotatedAssignmentComplexType)
{
    auto result = parser_->ParseSingleStatement("data: List[Dict[str, int]] = process(raw_data)");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "data");
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("process", "raw_data"));
}

// ============================================================================
// Nested Scope and Dependency Tests
// ============================================================================

TEST_F(PythonParserTest, ParseFunctionWithNestedFunction)
{
    auto result = parser_->ParseSingleStatement(R"(def outer(x):
    def inner(y):
        return y + z
    return inner(x) + global_var)");
    
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "outer");
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("z", "global_var"));
}

TEST_F(PythonParserTest, ParseFunctionWithDefaultArguments)
{
    auto result = parser_->ParseSingleStatement("def func(x, y=default_val, z=compute()): return x + y");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "func");
    EXPECT_THAT(result.items[0].dependencies, testing::IsSupersetOf({"default_val", "compute"}));
}

TEST_F(PythonParserTest, ParseFunctionWithGlobalReference)
{
    auto result = parser_->ParseSingleStatement(R"(def calculate(x, y):
    temp = x + y
    return temp * GLOBAL_CONSTANT + external_func(x))");
    
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, 
                testing::UnorderedElementsAre("GLOBAL_CONSTANT", "external_func"));
}

TEST_F(PythonParserTest, ParseClassWithNestedClass)
{
    auto result = parser_->ParseSingleStatement(R"(class Outer:
    class Inner:
        def method(self):
            return external_var
    def outer_method(self):
        return another_var)");
    
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "Outer");
    EXPECT_THAT(result.items[0].dependencies, 
                testing::UnorderedElementsAre("external_var", "another_var"));
}

TEST_F(PythonParserTest, ParseClassWithInheritance)
{
    auto result = parser_->ParseSingleStatement(R"(class Child(ParentClass, Mixin):
    def method(self):
        return self.value + helper_func())");
    
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "Child");
    EXPECT_THAT(result.items[0].dependencies, 
                testing::IsSupersetOf({"ParentClass", "Mixin", "helper_func"}));
}

TEST_F(PythonParserTest, ParseClassWithDecorator)
{
    auto result = parser_->ParseSingleStatement(R"(@dataclass
class MyClass:
    x: int
    def method(self):
        return external)");
    
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "MyClass");
    EXPECT_THAT(result.items[0].dependencies, testing::IsSupersetOf({"dataclass", "external"}));
}

// ============================================================================
// Comprehension and Generator Tests
// ============================================================================

TEST_F(PythonParserTest, ParseNestedListComprehension)
{
    auto result = parser_->ParseSingleStatement(
        "matrix = [[i*j + offset for j in cols] for i in rows]"
    );
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("offset", "cols", "rows"));
}

TEST_F(PythonParserTest, ParseSetComprehension)
{
    auto result = parser_->ParseSingleStatement("s = {x * multiplier for x in data if x > threshold}");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, 
                testing::UnorderedElementsAre("multiplier", "data", "threshold"));
}

TEST_F(PythonParserTest, ParseDictComprehension)
{
    auto result = parser_->ParseSingleStatement("d = {k: transform(v) for k, v in items}");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("transform", "items"));
}

TEST_F(PythonParserTest, ParseGeneratorExpression)
{
    auto result = parser_->ParseSingleStatement("gen = (x * factor for x in sequence if predicate(x))");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, 
                testing::UnorderedElementsAre("factor", "sequence", "predicate"));
}

// ============================================================================
// Attribute Chain and Method Call Tests
// ============================================================================

TEST_F(PythonParserTest, ParseAttributeChainAccess)
{
    auto result = parser_->ParseSingleStatement("value = obj.attr1.attr2.attr3");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies,
                testing::IsSupersetOf({"obj", "obj.attr1", "obj.attr1.attr2", "obj.attr1.attr2.attr3"}));
}

TEST_F(PythonParserTest, ParseMethodChainWithAttributes)
{
    auto result = parser_->ParseSingleStatement("result = data.filter().sort().values");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, testing::IsSupersetOf({"data"}));
}

TEST_F(PythonParserTest, ParseMixedAttributeAndIndex)
{
    auto result = parser_->ParseSingleStatement("val = obj.items[index].property");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, testing::IsSupersetOf({"obj", "index"}));
}

// ============================================================================
// Walrus Operator Tests
// ============================================================================

TEST_F(PythonParserTest, ParseWalrusOperatorSimple)
{
    auto result = parser_->ParseSingleStatement("result = (temp := a + b) * 2");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "result");
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("a", "b"));
}

TEST_F(PythonParserTest, ParseWalrusOperatorInComprehension)
{
    auto result = parser_->ParseSingleStatement("data = [y for x in items if (y := transform(x)) > threshold]");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, 
                testing::UnorderedElementsAre("items", "transform", "threshold"));
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(PythonParserTest, ParseFromImportWildcard)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("from math import *"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseAssignmentToAttribute)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("obj.attr = value"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseAssignmentToIndex)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("lst[0] = value"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseTupleUnpacking)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("a, b = 1, 2"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseAugmentedAssignment)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("x += 1"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseForLoop)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("for i in range(10): pass"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseIfStatement)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("if x > 0: y = 1"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseWhileLoop)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("while True: pass"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseWithStatement)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("with open('file.txt') as f: pass"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseTryExcept)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("try: pass\nexcept: pass"),
        ParseException
    );
}

// ============================================================================
// Special Python Features Tests
// ============================================================================

TEST_F(PythonParserTest, ParseFStringWithVariables)
{
    auto result = parser_->ParseSingleStatement("msg = f'Hello {name}, you have {count} items'");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("name", "count"));
}

TEST_F(PythonParserTest, ParseFStringWithExpressions)
{
    auto result = parser_->ParseSingleStatement("text = f'Result: {calculate(x, y):.2f}'");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("calculate", "x", "y"));
}

TEST_F(PythonParserTest, ParseSlicing)
{
    auto result = parser_->ParseSingleStatement("subset = data[start:end:step]");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, 
                testing::UnorderedElementsAre("data", "start", "end", "step"));
}

TEST_F(PythonParserTest, ParseUnaryOperators)
{
    auto result = parser_->ParseSingleStatement("val = -x + ~y + (not z)");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("x", "y", "z"));
}

TEST_F(PythonParserTest, ParseTernaryOperator)
{
    auto result = parser_->ParseSingleStatement("result = a if condition else b");
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("a", "condition", "b"));
}

// ============================================================================
// Multi-line and Complex Code Tests
// ============================================================================

TEST_F(PythonParserTest, ParseMultiLineFunction)
{
    auto result = parser_->ParseSingleStatement(R"(def complex_function(x, y, z=default):
    # Calculate intermediate values
    temp1 = helper1(x)
    temp2 = helper2(y)
    
    # Combine results
    result = temp1 + temp2 + external_var
    return result * z)");
    
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "complex_function");
    EXPECT_THAT(result.items[0].dependencies, 
                testing::IsSupersetOf({"default", "helper1", "helper2", "external_var"}));
}

TEST_F(PythonParserTest, ParseMultiLineClass)
{
    auto result = parser_->ParseSingleStatement(R"(class DataProcessor:
    # Class attribute
    DEFAULT_BUFFER = 1024
    
    def __init__(self, size=DEFAULT_SIZE):
        self.size = size
        self.buffer = allocate(size)
    
    def process(self, data):
        return transform(data, self.size))");
    
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "DataProcessor");
    EXPECT_THAT(result.items[0].dependencies, 
                testing::UnorderedElementsAre("DEFAULT_SIZE", "allocate", "transform"));
}

TEST_F(PythonParserTest, ParseAsyncFunction)
{
    auto result = parser_->ParseSingleStatement(R"(async def fetch_data(url):
    response = await http_client.get(url)
    return response.json())");
    
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "fetch_data");
    EXPECT_THAT(result.items[0].dependencies, testing::IsSupersetOf({"http_client"}));
}

// ============================================================================
// Cache Tests (Extended)
// ============================================================================

TEST_F(PythonParserTest, CacheWithComplexExpressions)
{
    std::string code = "result = math.sqrt(x**2 + y**2) + numpy.array([1,2,3]).sum()";
    
    parser_->ParseSingleStatement(code);
    size_t size1 = parser_->GetParseResultCacheSize();
    
    parser_->ParseSingleStatement(code);
    size_t size2 = parser_->GetParseResultCacheSize();
    
    EXPECT_EQ(size1, size2);
}

TEST_F(PythonParserTest, CacheIndependentOfWhitespace)
{
    auto result1 = parser_->ParseSingleStatement("a=b+c");
    auto result2 = parser_->ParseSingleStatement("a = b + c");
    
    EXPECT_EQ(result1.items[0].name, result2.items[0].name);
    EXPECT_EQ(result1.items[0].dependencies, result2.items[0].dependencies);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    // 不能用 scoped_interpreter：它会用默认配置初始化嵌入的 CPython，
    // 而该 Python DLL 内置的 prefix 是旧路径（D:\M\msys64\mingw64），
    // 找不到 stdlib 会直接 terminate。
    // 这里通过引擎用构建期注入的 REL_PYTHON_* 路径初始化解释器。
    PythonEquationEngine::SetDefaultPyEnvConfig();
    PythonEquationEngine::GetInstance();
    // 引擎初始化后释放了主线程 GIL，测试体内会直接操作 pybind11 对象，
    // 所以需要在主线程重新持有 GIL。
    pybind11::gil_scoped_acquire acquire;
    int ret = RUN_ALL_TESTS();
    return ret;
}