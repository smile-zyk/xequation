#include "core/equation.h"
#include "core/expr_common.h"
#include "python/py_code_parser.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <pybind11/embed.h>


using namespace xequation;
using namespace xequation::python;

class PyCodeParserTest : public ::testing::Test
{
  protected:
    virtual void SetUp()
    {
        parser_.reset(new PyCodeParser());
    }

    virtual void TearDown()
    {
        parser_.reset();
    }

    std::unique_ptr<PyCodeParser> parser_;
};

// 基础功能测试
TEST_F(PyCodeParserTest, ParseSimpleAssignment)
{
    auto parse_result = parser_->ParseSingleStatement("a = b + c");
    EXPECT_EQ(parse_result.size(), 1);
    auto result = parse_result[0];
    EXPECT_EQ(result.name(), "a");
    EXPECT_THAT(result.dependencies(), testing::UnorderedElementsAre("b", "c"));
    EXPECT_EQ(result.type(), Equation::Type::kVariable);
    EXPECT_EQ(result.content(), "a = b + c");
}

TEST_F(PyCodeParserTest, ParseImport)
{
    auto parse_result = parser_->ParseSingleStatement("import math as mt");
    EXPECT_EQ(parse_result.size(), 1);
    auto result = parse_result[0];
    EXPECT_EQ(result.name(), "mt");
    EXPECT_THAT(result.dependencies(), testing::IsEmpty());
    EXPECT_EQ(result.type(), Equation::Type::kImport);
    EXPECT_EQ(result.content(), "import math as mt");
}

TEST_F(PyCodeParserTest, ParseFromImport)
{
    auto parse_result = parser_->ParseSingleStatement("from math import cos");
    EXPECT_EQ(parse_result.size(), 1);
    auto result = parse_result[0];
    EXPECT_EQ(result.name(), "cos");
    EXPECT_THAT(result.dependencies(), testing::IsEmpty());
    EXPECT_EQ(result.type(), Equation::Type::kImportFrom);
    EXPECT_EQ(result.content(), "from math import cos");
}

TEST_F(PyCodeParserTest, ParseFunction)
{
    auto parse_result = parser_->ParseSingleStatement("def hello(): print('Hello World')");
    EXPECT_EQ(parse_result.size(), 1);
    auto result = parse_result[0];
    EXPECT_EQ(result.name(), "hello");
    EXPECT_THAT(result.dependencies(), testing::IsEmpty());
    EXPECT_EQ(result.type(), Equation::Type::kFunction);
    EXPECT_EQ(result.content(), "def hello(): print('Hello World')");
}

TEST_F(PyCodeParserTest, ParseClass)
{
    auto parse_result = parser_->ParseSingleStatement("class Person: pass");
    EXPECT_EQ(parse_result.size(), 1);
    auto result = parse_result[0];
    EXPECT_EQ(result.name(), "Person");
    EXPECT_THAT(result.dependencies(), testing::IsEmpty());
    EXPECT_EQ(result.type(), Equation::Type::kClass);
    EXPECT_EQ(result.content(), "class Person: pass");
}

// 错误处理测试
TEST_F(PyCodeParserTest, ParseErrorUnsupportedStatement)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("a+b"),
        ParseException
    );
}

TEST_F(PyCodeParserTest, ParseErrorInvalidSyntax)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("b = te+"),
        ParseException
    );
}

TEST_F(PyCodeParserTest, ParseErrorBuiltinRedefinition)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("print = 1+a"),
        ParseException
    );
}

TEST_F(PyCodeParserTest, ParseErrorMultipleAssignment)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("a,b = 1,2"),
        ParseException
    );
}

// 复杂表达式测试
TEST_F(PyCodeParserTest, ParseComplexNumeric)
{
    auto result = parser_->ParseSingleStatement("a = 3 + 4j + 2 - 1j + 1 + 2j");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name(), "a");
    EXPECT_THAT(result[0].dependencies(), testing::IsEmpty());
    EXPECT_EQ(result[0].type(), Equation::Type::kVariable);
    EXPECT_EQ(result[0].content(), "a = 3 + 4j + 2 - 1j + 1 + 2j");
}

TEST_F(PyCodeParserTest, ParseComplexListComprehension)
{
    auto result = parser_->ParseSingleStatement(
        "matrix = [[(i * j) + (i - j) * 1j for j in range(1, 4)] for i in range(1, 5)]"
    );
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name(), "matrix");
    EXPECT_THAT(result[0].dependencies(), testing::UnorderedElementsAre("i", "j"));
    EXPECT_EQ(result[0].type(), Equation::Type::kVariable);
}

TEST_F(PyCodeParserTest, ParseComplexConditional)
{
    auto result = parser_->ParseSingleStatement(
        "condition = (global_flag := True) and any(complex(i, j).real > 0 for i in range(1, 10, 3) for j in range(1, 10, 4))"
    );
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name(), "condition");
    EXPECT_THAT(result[0].dependencies(), testing::UnorderedElementsAre("i", "j"));
    EXPECT_EQ(result[0].type(), Equation::Type::kVariable);
}

TEST_F(PyCodeParserTest, ParseFunctionCallChain)
{
    auto result = parser_->ParseSingleStatement(
        "func = process_data(filter_data(load_data(\"file.txt\")))"
    );
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name(), "func");
    EXPECT_THAT(result[0].dependencies(), 
                testing::UnorderedElementsAre("process_data", "filter_data", "load_data"));
    EXPECT_EQ(result[0].type(), Equation::Type::kVariable);
}

// 新增测试：边界情况
TEST_F(PyCodeParserTest, ParseEmptyString)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement(""),
        ParseException
    );
}

TEST_F(PyCodeParserTest, ParseWhitespaceOnly)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("   \t\n   "),
        ParseException
    );
}

TEST_F(PyCodeParserTest, ParseCommentOnly)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("# This is a comment"),
        ParseException
    );
}

TEST_F(PyCodeParserTest, ParseWithComments)
{
    auto result = parser_->ParseSingleStatement("a = b + c  # add b and c");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name(), "a");
    EXPECT_THAT(result[0].dependencies(), testing::UnorderedElementsAre("b", "c"));
}

// 新增测试：特殊字符和命名
TEST_F(PyCodeParserTest, ParseSpecialCharacters)
{
    auto result = parser_->ParseSingleStatement("_private_var = public_var * 2");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name(), "_private_var");
    EXPECT_THAT(result[0].dependencies(), testing::UnorderedElementsAre("public_var"));
}

// 缓存功能测试
TEST_F(PyCodeParserTest, CacheBasicFunctionality)
{
    std::string code = "t = x + y";

    auto result1 = parser_->ParseSingleStatement(code);
    EXPECT_EQ(parser_->GetCacheSize(), 1u);

    auto result2 = parser_->ParseSingleStatement(code);
    EXPECT_EQ(parser_->GetCacheSize(), 1u);

    EXPECT_EQ(result1.size(), result2.size());
    EXPECT_EQ(result1[0].name(), result2[0].name());
    EXPECT_EQ(result1[0].content(), result2[0].content());
}

TEST_F(PyCodeParserTest, CacheDifferentExpressions)
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

TEST_F(PyCodeParserTest, ClearCache)
{
    parser_->ParseSingleStatement("t = test_variable");
    EXPECT_GT(parser_->GetCacheSize(), 0u);
    
    parser_->ClearCache();
    EXPECT_EQ(parser_->GetCacheSize(), 0u);

    parser_->ParseSingleStatement("t = test_variable");
    EXPECT_EQ(parser_->GetCacheSize(), 1u);
}

TEST_F(PyCodeParserTest, CacheSizeLimit)
{
    parser_->SetMaxCacheSize(3);

    parser_->ParseSingleStatement("t = a");
    parser_->ParseSingleStatement("t = b");
    parser_->ParseSingleStatement("t = c");
    parser_->ParseSingleStatement("t = d");

    EXPECT_LE(parser_->GetCacheSize(), 3u);
}

TEST_F(PyCodeParserTest, CacheLRUBehavior)
{
    parser_->SetMaxCacheSize(2);

    parser_->ParseSingleStatement("t = expr1");
    parser_->ParseSingleStatement("t = expr2");
    parser_->ParseSingleStatement("t = expr1");  // 访问expr1，使其成为最近使用的
    parser_->ParseSingleStatement("t = expr3");  // 应该淘汰expr2

    EXPECT_EQ(parser_->GetCacheSize(), 2u);
    
    // expr2 应该被淘汰，expr1 和 expr3 应该在缓存中
    parser_->ParseSingleStatement("t = expr2");  // 应该重新缓存
    EXPECT_EQ(parser_->GetCacheSize(), 2u);
}

TEST_F(PyCodeParserTest, SetMaxCacheSizeDynamic)
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

TEST_F(PyCodeParserTest, CacheWithIdenticalContent)
{
    std::string code1 = "t = x + y";
    std::string code2 = "t = x + y";

    parser_->ParseSingleStatement(code1);
    size_t size1 = parser_->GetCacheSize();

    parser_->ParseSingleStatement(code2);
    size_t size2 = parser_->GetCacheSize();

    EXPECT_EQ(size1, size2);
}

TEST_F(PyCodeParserTest, CacheWithDifferentContent)
{
    std::string code1 = "t = x + y";
    std::string code2 = "t = x + y ";  // 末尾有空格，不同内容

    parser_->ParseSingleStatement(code1);
    size_t size1 = parser_->GetCacheSize();

    parser_->ParseSingleStatement(code2);
    size_t size2 = parser_->GetCacheSize();

    EXPECT_EQ(size2, size1 + 1);
}

TEST_F(PyCodeParserTest, MultipleClearCache)
{
    parser_->ParseSingleStatement("t = x");
    EXPECT_GT(parser_->GetCacheSize(), 0u);

    parser_->ClearCache();
    EXPECT_EQ(parser_->GetCacheSize(), 0u);

    parser_->ClearCache();  // 再次清除
    EXPECT_EQ(parser_->GetCacheSize(), 0u);

    parser_->ParseSingleStatement("t = y");
    EXPECT_EQ(parser_->GetCacheSize(), 1u);
}

// 新增测试：多语句解析
TEST_F(PyCodeParserTest, ParseMultipleStatements)
{
    auto results = parser_->ParseMultipleStatements("a = 1\nb = a + 2\nc = b * 3");
    EXPECT_EQ(results.size(), 3);
    
    EXPECT_EQ(results[0].name(), "a");
    EXPECT_EQ(results[1].name(), "b");
    EXPECT_EQ(results[2].name(), "c");
    
    EXPECT_THAT(results[1].dependencies(), testing::UnorderedElementsAre("a"));
    EXPECT_THAT(results[2].dependencies(), testing::UnorderedElementsAre("b"));
}

TEST_F(PyCodeParserTest, ParseMultipleStatementsWithError)
{
    EXPECT_THROW(
        parser_->ParseMultipleStatements("a = 1\ninvalid syntax\nc = 3"),
        ParseException
    );
}

// 新增测试：语句分割功能
TEST_F(PyCodeParserTest, SplitStatementsBasic)
{
    auto statements = parser_->SplitStatements("a = 1\nb = 2\nc = 3");
    EXPECT_THAT(statements, testing::ElementsAre("a = 1", "b = 2", "c = 3"));
}

TEST_F(PyCodeParserTest, SplitStatementsWithSemicolons)
{
    auto statements = parser_->SplitStatements("a = 1; b = 2; c = 3");
    EXPECT_THAT(statements, testing::ElementsAre("a = 1", "b = 2", "c = 3"));
}

TEST_F(PyCodeParserTest, SplitStatementsMixed)
{
    auto statements = parser_->SplitStatements("a = 1\nb = 2; c = 3\nd = 4");
    EXPECT_THAT(statements, testing::ElementsAre("a = 1", "b = 2", "c = 3", "d = 4"));
}

TEST_F(PyCodeParserTest, SplitStatementsComplex)
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

TEST_F(PyCodeParserTest, CachePerformance)
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

    EXPECT_EQ(result1.size(), result2.size());
    EXPECT_EQ(result1[0].name(), result2[0].name());
    EXPECT_EQ(result1[0].content(), result2[0].content());
}

TEST_F(PyCodeParserTest, ParseMultipleImport)
{
    auto parse_result = parser_->ParseSingleStatement("import os, math");
    EXPECT_EQ(parse_result.size(), 2);
    
    // 检查第一个import
    EXPECT_EQ(parse_result[0].name(), "os");
    EXPECT_EQ(parse_result[0].type(), Equation::Type::kImport);
    EXPECT_EQ(parse_result[0].content(), "import os");
    
    // 检查第二个import  
    EXPECT_EQ(parse_result[1].name(), "math");
    EXPECT_EQ(parse_result[1].type(), Equation::Type::kImport);
    EXPECT_EQ(parse_result[1].content(), "import math");
}

TEST_F(PyCodeParserTest, ParseMultipleImportWithAliases)
{
    auto parse_result = parser_->ParseSingleStatement("import os as operating_system, math as mathematics");
    EXPECT_EQ(parse_result.size(), 2);
    
    EXPECT_EQ(parse_result[0].name(), "operating_system");
    EXPECT_EQ(parse_result[0].type(), Equation::Type::kImport);
    EXPECT_EQ(parse_result[0].content(), "import os as operating_system");
    
    EXPECT_EQ(parse_result[1].name(), "mathematics");
    EXPECT_EQ(parse_result[1].type(), Equation::Type::kImport);
    EXPECT_EQ(parse_result[1].content(), "import math as mathematics");
}

TEST_F(PyCodeParserTest, ParseMultipleFromImport)
{
    auto parse_result = parser_->ParseSingleStatement("from math import sin, cos");
    EXPECT_EQ(parse_result.size(), 2);
    
    EXPECT_EQ(parse_result[0].name(), "sin");
    EXPECT_EQ(parse_result[0].type(), Equation::Type::kImportFrom);
    EXPECT_EQ(parse_result[0].content(), "from math import sin");
    
    EXPECT_EQ(parse_result[1].name(), "cos"); 
    EXPECT_EQ(parse_result[1].type(), Equation::Type::kImportFrom);
    EXPECT_EQ(parse_result[1].content(), "from math import cos");
}

TEST_F(PyCodeParserTest, ParseMultipleFromImportWithAliases)
{
    auto parse_result = parser_->ParseSingleStatement("from math import sin as sine, cos as cosine");
    EXPECT_EQ(parse_result.size(), 2);
    
    EXPECT_EQ(parse_result[0].name(), "sine");
    EXPECT_EQ(parse_result[0].type(), Equation::Type::kImportFrom);
    EXPECT_EQ(parse_result[0].content(), "from math import sin as sine");
    
    EXPECT_EQ(parse_result[1].name(), "cosine");
    EXPECT_EQ(parse_result[1].type(), Equation::Type::kImportFrom);
    EXPECT_EQ(parse_result[1].content(), "from math import cos as cosine");
}

TEST_F(PyCodeParserTest, ParseStarImport)
{
    // 测试 from module import *
    auto parse_result = parser_->ParseSingleStatement("from math import *");
    
    // 这里应该返回math模块中的所有公共符号
    // 具体数量取决于math模块的内容，但至少应该有一些常见的数学函数
    EXPECT_GT(parse_result.size(), 0);
    
    // 检查是否包含一些常见的数学函数
    bool has_sin = false;
    bool has_cos = false;
    bool has_pi = false;
    
    for (const auto& eqn : parse_result) {
        EXPECT_EQ(eqn.type(), Equation::Type::kImportFrom);
        EXPECT_TRUE(eqn.name().length() > 0);
        EXPECT_TRUE(eqn.content().find("from math import") != std::string::npos);
        
        if (eqn.name() == "sin") has_sin = true;
        if (eqn.name() == "cos") has_cos = true;
        if (eqn.name() == "pi") has_pi = true;
    }
    
    // 至少应该包含一些基本函数
    EXPECT_TRUE(has_sin || has_cos || has_pi);
}

TEST_F(PyCodeParserTest, ParseMixedImportStatements)
{
    // 测试多种import语句的组合解析
    auto results = parser_->ParseMultipleStatements(
        "import os\n"
        "import math as m\n"
        "from sys import version\n"
        "from os.path import join, exists\n"
        "from collections import *"
    );
    
    // 检查总数量
    EXPECT_GE(results.size(), 5);  // 至少5个，star import会添加更多
    
    // 检查各个import语句
    bool has_os = false;
    bool has_math_alias = false;
    bool has_version = false;
    bool has_join = false;
    bool has_exists = false;
    bool has_star_import = false;
    
    for (const auto& eqn : results) {
        if (eqn.name() == "os" && eqn.type() == Equation::Type::kImport) {
            has_os = true;
        }
        if (eqn.name() == "m" && eqn.type() == Equation::Type::kImport) {
            has_math_alias = true;
        }
        if (eqn.name() == "version" && eqn.type() == Equation::Type::kImportFrom) {
            has_version = true;
        }
        if (eqn.name() == "join" && eqn.type() == Equation::Type::kImportFrom) {
            has_join = true;
        }
        if (eqn.name() == "exists" && eqn.type() == Equation::Type::kImportFrom) {
            has_exists = true;
        }
        if (eqn.content().find("from collections import") != std::string::npos) {
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

TEST_F(PyCodeParserTest, ParseImportBuiltinProtection)
{
    // 测试不能重定义builtin名称的保护机制
    EXPECT_THROW(
        parser_->ParseSingleStatement("import builtins as print"),  // 尝试重定义print
        ParseException
    );
    
    EXPECT_THROW(
        parser_->ParseSingleStatement("from math import sin as len"),  // 尝试重定义len
        ParseException
    );
}

TEST_F(PyCodeParserTest, ParseComplexImportScenarios)
{
    // 测试复杂的import场景
    auto results = parser_->ParseMultipleStatements(
        "import os.path as ospath\n"
        "from os.path import dirname, basename\n"
        "import numpy as np\n"
        "from numpy import array, matrix"
    );
    
    EXPECT_GE(results.size(), 5);
    
    // 检查别名导入
    bool has_ospath = false;
    bool has_np = false;
    
    for (const auto& eqn : results) {
        if (eqn.name() == "ospath") {
            has_ospath = true;
            EXPECT_EQ(eqn.type(), Equation::Type::kImport);
        }
        if (eqn.name() == "np") {
            has_np = true;
            EXPECT_EQ(eqn.type(), Equation::Type::kImport);
        }
    }
    
    EXPECT_TRUE(has_ospath);
    EXPECT_TRUE(has_np);
}

TEST_F(PyCodeParserTest, ParseImportSubmoduleWithoutAliasShouldFail)
{
    EXPECT_THROW(
        parser_->ParseSingleStatement("import os.path"),
        ParseException
    );
}


int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    py::scoped_interpreter guard{};
    int ret = RUN_ALL_TESTS();
    return ret;
}