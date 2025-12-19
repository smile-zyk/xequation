#include "core/equation.h"
#include "core/equation_common.h"
#include "python/python_parser.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <pybind11/embed.h>


using namespace xequation;
using namespace xequation::python;

class PythonParserTest : public ::testing::Test
{
  protected:
    virtual void SetUp()
    {
        parser_.reset(new PythonParser());
    }

    virtual void TearDown()
    {
        parser_.reset();
    }

    std::unique_ptr<PythonParser> parser_;
};

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
    EXPECT_THAT(item.dependencies, testing::IsEmpty());
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

TEST_F(PythonParserTest, ParseErrorUnsupportedStatement)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("a+b"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseErrorInvalidSyntax)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("b = te+"),
        ParseException
    );
}

TEST_F(PythonParserTest, ParseErrorBuiltinRedefinition)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("print = 1+a"),
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
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("i", "j"));
    EXPECT_EQ(result.items[0].type, ItemType::kVariable);
}

TEST_F(PythonParserTest, ParseComplexConditional)
{
    auto result = parser_->ParseSingleStatement(
        "condition = (global_flag := True) and any(complex(i, j).real > 0 for i in range(1, 10, 3) for j in range(1, 10, 4))"
    );
    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].name, "condition");
    EXPECT_THAT(result.items[0].dependencies, testing::UnorderedElementsAre("i", "j"));
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

TEST_F(PythonParserTest, CacheBasicFunctionality)
{
    std::string code = "t = x + y";

    auto result1 = parser_->ParseSingleStatement(code);
    EXPECT_EQ(parser_->GetCacheSize(), 1u);

    auto result2 = parser_->ParseSingleStatement(code);
    EXPECT_EQ(parser_->GetCacheSize(), 1u);

    EXPECT_EQ(result1.items.size(), result2.items.size());
    EXPECT_EQ(result1.items[0].name, result2.items[0].name);
    EXPECT_EQ(result1.items[0].content, result2.items[0].content);
}

TEST_F(PythonParserTest, CacheDifferentExpressions)
{
    parser_->ParseSingleStatement("t0 = x + y");
    EXPECT_EQ(parser_->GetCacheSize(), 1u);

    parser_->ParseSingleStatement("t1 = a * b");
    EXPECT_EQ(parser_->GetCacheSize(), 2u);

    parser_->ParseSingleStatement("t2 = func(param)");
    EXPECT_EQ(parser_->GetCacheSize(), 3u);

    parser_->ParseSingleStatement("t0 = x + y");
    EXPECT_EQ(parser_->GetCacheSize(), 3u);
}

TEST_F(PythonParserTest, ClearCache)
{
    parser_->ParseSingleStatement("t = test_variable");
    EXPECT_GT(parser_->GetCacheSize(), 0u);
    
    parser_->ClearCache();
    EXPECT_EQ(parser_->GetCacheSize(), 0u);

    parser_->ParseSingleStatement("t = test_variable");
    EXPECT_EQ(parser_->GetCacheSize(), 1u);
}

TEST_F(PythonParserTest, CacheSizeLimit)
{
    parser_->SetMaxCacheSize(3);

    parser_->ParseSingleStatement("t = a");
    parser_->ParseSingleStatement("t = b");
    parser_->ParseSingleStatement("t = c");
    parser_->ParseSingleStatement("t = d");

    EXPECT_LE(parser_->GetCacheSize(), 3u);
}

TEST_F(PythonParserTest, CacheLRUBehavior)
{
    parser_->SetMaxCacheSize(2);

    parser_->ParseSingleStatement("t = expr1");
    parser_->ParseSingleStatement("t = expr2");
    parser_->ParseSingleStatement("t = expr1");
    parser_->ParseSingleStatement("t = expr3");

    EXPECT_EQ(parser_->GetCacheSize(), 2u);
    
    parser_->ParseSingleStatement("t = expr2");
    EXPECT_EQ(parser_->GetCacheSize(), 2u);
}

TEST_F(PythonParserTest, SetMaxCacheSizeDynamic)
{
    EXPECT_EQ(parser_->GetCacheSize(), 0u);

    parser_->SetMaxCacheSize(2);
    parser_->ParseSingleStatement("t = a");
    parser_->ParseSingleStatement("t = b");
    parser_->ParseSingleStatement("t = c");
    EXPECT_LE(parser_->GetCacheSize(), 2u);

    parser_->ClearCache();
    parser_->SetMaxCacheSize(10);
    for (int i = 0; i < 8; ++i) {
        parser_->ParseSingleStatement("t = expr" + std::to_string(i));
    }
    EXPECT_EQ(parser_->GetCacheSize(), 8u);

    parser_->ParseSingleStatement("t = expr8");
    parser_->ParseSingleStatement("t = expr9");
    EXPECT_EQ(parser_->GetCacheSize(), 10u);
}

TEST_F(PythonParserTest, CacheWithDifferentContent)
{
    std::string code1 = "t = x + y";
    std::string code2 = "t = x + y ";

    parser_->ParseSingleStatement(code1);
    size_t size1 = parser_->GetCacheSize();

    parser_->ParseSingleStatement(code2);
    size_t size2 = parser_->GetCacheSize();

    EXPECT_EQ(size2, size1);
}

TEST_F(PythonParserTest, MultipleClearCache)
{
    parser_->ParseSingleStatement("t = x");
    EXPECT_GT(parser_->GetCacheSize(), 0u);

    parser_->ClearCache();
    EXPECT_EQ(parser_->GetCacheSize(), 0u);

    parser_->ClearCache();
    EXPECT_EQ(parser_->GetCacheSize(), 0u);

    parser_->ParseSingleStatement("t = y");
    EXPECT_EQ(parser_->GetCacheSize(), 1u);
}

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

TEST_F(PythonParserTest, ParseStarImport)
{
    auto parse_result = parser_->ParseSingleStatement("from math import *");
    
    EXPECT_GT(parse_result.items.size(), 0);
    
    bool has_sin = false;
    bool has_cos = false;
    bool has_pi = false;
    
    for (const auto& eqn : parse_result.items) {
        EXPECT_EQ(eqn.type, ItemType::kImportFrom);
        EXPECT_TRUE(eqn.name.length() > 0);
        EXPECT_TRUE(eqn.content.find("from math import") != std::string::npos);
        
        if (eqn.name == "sin") has_sin = true;
        if (eqn.name == "cos") has_cos = true;
        if (eqn.name == "pi") has_pi = true;
    }
    
    EXPECT_TRUE(has_sin || has_cos || has_pi);
}

TEST_F(PythonParserTest, ParseMixedImportStatements)
{
    auto results = parser_->ParseStatements(
        "import os\n"
        "import math as m\n"
        "from sys import version\n"
        "from os.path import join, exists\n"
        "from collections import *"
    );
    
    EXPECT_GE(results.items.size(), 5); 
    
    bool has_os = false;
    bool has_math_alias = false;
    bool has_version = false;
    bool has_join = false;
    bool has_exists = false;
    bool has_star_import = false;
    
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
        if (eqn.content.find("from collections import") != std::string::npos) {
            has_star_import = true;
        }
    }
    
    EXPECT_TRUE(has_os);
    EXPECT_TRUE(has_math_alias);
    EXPECT_TRUE(has_version);
    EXPECT_TRUE(has_join);
    EXPECT_TRUE(has_exists);
    EXPECT_TRUE(has_star_import);
}

TEST_F(PythonParserTest, ParseImportBuiltinProtection)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("import builtins as print"), 
        ParseException
    );
    
    EXPECT_THROW(
        parser_->ParseSingleStatement("from math import sin as len"), 
        ParseException
    );
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

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    pybind11::scoped_interpreter guard{};
    int ret = RUN_ALL_TESTS();
    return ret;
}