// #include <gtest/gtest.h>
// #include <pybind11/embed.h>
// #include "python/py_symbol_extractor.h"
// #include "core/expr_common.h"

// using namespace xequation;

// class PySymbolExtractorTest : public ::testing::Test {
// protected:
//     virtual void SetUp() {
//         extractor_.reset(new PySymbolExtractor());
//     }

//     virtual void TearDown() {
//         extractor_.reset();
//     }

//     std::unique_ptr<PySymbolExtractor> extractor_;
// };

// TEST_F(PySymbolExtractorTest, ExtractSimpleVariableReference) {
//     std::string code = "x";
//     auto result = extractor_->Extract(code);
    
//     EXPECT_EQ(result.status, VariableStatus::kParseSuccess);
//     EXPECT_TRUE(result.variables.find("x") != result.variables.end());
//     EXPECT_EQ(result.variables.size(), 1u);
// }

// TEST_F(PySymbolExtractorTest, ExtractFunctionCall) {
//     std::string code = "calculate_sum(a, b)";
//     auto result = extractor_->Extract(code);
    
//     EXPECT_EQ(result.status, VariableStatus::kParseSuccess);
//     EXPECT_TRUE(result.variables.find("calculate_sum") != result.variables.end());
//     EXPECT_TRUE(result.variables.find("a") != result.variables.end());
//     EXPECT_TRUE(result.variables.find("b") != result.variables.end());
//     EXPECT_EQ(result.variables.size(), 3u); // calculate_sum, a, b
// }

// TEST_F(PySymbolExtractorTest, ExtractBinaryOperation) {
//     std::string code = "x + y * z";
//     auto result = extractor_->Extract(code);
    
//     EXPECT_EQ(result.status, VariableStatus::kParseSuccess);
//     EXPECT_TRUE(result.variables.find("x") != result.variables.end());
//     EXPECT_TRUE(result.variables.find("y") != result.variables.end());
//     EXPECT_TRUE(result.variables.find("z") != result.variables.end());
//     EXPECT_EQ(result.variables.size(), 3u);
// }

// TEST_F(PySymbolExtractorTest, CacheBasicFunctionality) {
//     std::string code = "x + y";
    
//     auto result1 = extractor_->Extract(code);
//     EXPECT_EQ(extractor_->GetCacheSize(), 1u);
    
//     auto result2 = extractor_->Extract(code);
//     EXPECT_EQ(extractor_->GetCacheSize(), 1u); // 缓存大小不变
    
//     EXPECT_EQ(result1.status, result2.status);
//     EXPECT_EQ(result1.variables.size(), result2.variables.size());
// }

// TEST_F(PySymbolExtractorTest, CacheDifferentExpressions) {
//     std::string code1 = "x + y";
//     std::string code2 = "a * b";
//     std::string code3 = "func(param)";
    
//     extractor_->Extract(code1);
//     EXPECT_EQ(extractor_->GetCacheSize(), 1u);
    
//     extractor_->Extract(code2);
//     EXPECT_EQ(extractor_->GetCacheSize(), 2u);
    
//     extractor_->Extract(code3);
//     EXPECT_EQ(extractor_->GetCacheSize(), 3u);
    
//     extractor_->Extract(code1);
//     EXPECT_EQ(extractor_->GetCacheSize(), 3u);
// }

// TEST_F(PySymbolExtractorTest, ClearCache) {
//     std::string code = "test_variable";
//     extractor_->Extract(code);
    
//     EXPECT_GT(extractor_->GetCacheSize(), 0u);
//     extractor_->ClearCache();
//     EXPECT_EQ(extractor_->GetCacheSize(), 0u);
    
//     extractor_->Extract(code);
//     EXPECT_EQ(extractor_->GetCacheSize(), 1u);
// }

// TEST_F(PySymbolExtractorTest, CacheSizeLimit) {
//     extractor_->SetMaxCacheSize(3);
    
//     extractor_->Extract("a");
//     extractor_->Extract("b");
//     extractor_->Extract("c");
//     extractor_->Extract("d");
    
//     EXPECT_LE(extractor_->GetCacheSize(), 3u);
// }

// TEST_F(PySymbolExtractorTest, CacheLRUBehavior) {
//     extractor_->SetMaxCacheSize(2);
    
//     extractor_->Extract("expr1");
//     extractor_->Extract("expr2");
    
//     extractor_->Extract("expr1");
    
//     extractor_->Extract("expr3");
    
//     EXPECT_EQ(extractor_->GetCacheSize(), 2u);
    
//     extractor_->Extract("expr2");
//     EXPECT_EQ(extractor_->GetCacheSize(), 2u);
// }

// TEST_F(PySymbolExtractorTest, SetMaxCacheSizeDynamic) {
//     EXPECT_EQ(extractor_->GetCacheSize(), 0u);
    
//     extractor_->SetMaxCacheSize(2);
//     extractor_->Extract("a");
//     extractor_->Extract("b");
//     extractor_->Extract("c");
//     EXPECT_LE(extractor_->GetCacheSize(), 2u);
    
//     extractor_->ClearCache();
//     extractor_->SetMaxCacheSize(10);
//     for (int i = 0; i < 8; ++i) {
//         extractor_->Extract("expr" + std::to_string(i));
//     }
//     EXPECT_EQ(extractor_->GetCacheSize(), 8u);
    
//     extractor_->Extract("expr8");
//     extractor_->Extract("expr9");
//     EXPECT_EQ(extractor_->GetCacheSize(), 10u);
// }

// TEST_F(PySymbolExtractorTest, CacheWithIdenticalContent) {
//     std::string code1 = "x + y";
//     std::string code2 = "x + y";
    
//     extractor_->Extract(code1);
//     size_t size1 = extractor_->GetCacheSize();
    
//     extractor_->Extract(code2);
//     size_t size2 = extractor_->GetCacheSize();
    
//     EXPECT_EQ(size1, size2);
// }

// TEST_F(PySymbolExtractorTest, CacheWithDifferentContent) {
//     std::string code1 = "x + y";
//     std::string code2 = "x + y ";
    
//     extractor_->Extract(code1);
//     size_t size1 = extractor_->GetCacheSize();
    
//     extractor_->Extract(code2);
//     size_t size2 = extractor_->GetCacheSize();
    
//     EXPECT_EQ(size2, size1 + 1);
// }

// TEST_F(PySymbolExtractorTest, MultipleClearCache) {
//     extractor_->Extract("x");
//     EXPECT_GT(extractor_->GetCacheSize(), 0u);
    
//     extractor_->ClearCache();
//     EXPECT_EQ(extractor_->GetCacheSize(), 0u);
    
//     extractor_->ClearCache(); 
//     EXPECT_EQ(extractor_->GetCacheSize(), 0u);
    
//     extractor_->Extract("y");
//     EXPECT_EQ(extractor_->GetCacheSize(), 1u);
// }

// TEST_F(PySymbolExtractorTest, CachePerformance) {
//     std::string complex_expr = "sqrt(x*x + y*y) + sin(angle) * cos(angle)";
    
//     auto start1 = std::chrono::high_resolution_clock::now();
//     auto result1 = extractor_->Extract(complex_expr);
//     auto end1 = std::chrono::high_resolution_clock::now();
//     auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    
//     auto start2 = std::chrono::high_resolution_clock::now();
//     auto result2 = extractor_->Extract(complex_expr);
//     auto end2 = std::chrono::high_resolution_clock::now();
//     auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    
//     EXPECT_LE(duration2.count(), duration1.count());
    
//     EXPECT_EQ(result1.status, result2.status);
//     EXPECT_EQ(result1.variables.size(), result2.variables.size());
// }

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     py::scoped_interpreter guard{};
//     int ret = RUN_ALL_TESTS();
//     return ret;
// }