// #include <gtest/gtest.h>
// #include <pybind11/embed.h>

// #include "python/py_parser.h"

// using namespace xequation;

// class PyParserTest : public ::testing::Test {
// protected:
//     virtual void SetUp() {
//         parser_.reset(new PyParser());
//     }

//     virtual void TearDown() {
//         parser_.reset();
//     }

//     std::unique_ptr<PyParser> parser_;
// };

// TEST_F(PyParserTest, ExtractSimpleVariableReference) {
//     auto res = parser_->AnalyzeStatement("import os");
//     EXPECT_EQ(res.content, "import os");
//     EXPECT_EQ(res.dependencies.size(), 0);
//     EXPECT_EQ(res.name, "os");
//     EXPECT_EQ(res.type, VariableType::kImport);

//     res = parser_->AnalyzeStatement("a=b+c+");
//     EXPECT_EQ(res.content, "a=b+c");
//     EXPECT_EQ(res.dependencies.size(), 2);
//     EXPECT_EQ(res.name, "a");
//     EXPECT_EQ(res.type, VariableType::kVarDecl);
// }

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     py::scoped_interpreter guard{};
//     int ret = RUN_ALL_TESTS();
//     return ret;
// }