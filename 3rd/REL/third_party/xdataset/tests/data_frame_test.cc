#include "block_fixtures.h"

#include <gtest/gtest.h>

namespace xdataset
{
    using namespace block_fixtures;

    // Test wrapper that exposes protected Configure / constructor of DataFrame.
    class TestDataFrame : public DataFrame
    {
    public:
        TestDataFrame() = default;

        void ConfigureDynamic(std::vector<std::string> h, std::size_t n,
                              RowGenerator g, std::size_t cs = 256)
        {
            DataFrame::ConfigureDynamic(std::move(h), n, std::move(g), cs);
        }

        void ConfigureStatic(std::vector<std::string> h,
                             std::vector<DataFrameRow> r)
        {
            DataFrame::ConfigureStatic(std::move(h), std::move(r));
        }
    };

    // =========================================================================
    // Measurement stringify / type inspect
    // =========================================================================

    TEST(MeasurementTest, StringifyScalarTypes)
    {
        EXPECT_EQ(Measurement::Real(3.14).to_string(), "3.14");
        EXPECT_EQ(Measurement::Integer(42).to_string(), "42");
        EXPECT_EQ(Measurement::Complex(std::complex<double>(1.0, -2.0)).to_string(), "1-2i");
        EXPECT_EQ(Measurement::String(std::string("hello")).to_string(), "hello");
    }

    TEST(MeasurementTest, KindAndDtype)
    {
        EXPECT_EQ(Measurement::Real(3.14).data_type(),    DataType::kReal);
        EXPECT_EQ(Measurement::Integer(42).data_type(),      DataType::kInteger);
        EXPECT_EQ(Measurement::Complex(std::complex<double>(1,2)).data_type(), DataType::kComplex);
        EXPECT_EQ(Measurement::String(std::string("s")).data_type(), DataType::kString);

        EXPECT_EQ(Measurement::Real(1.0).data_type(), DataType::kReal);
        EXPECT_NE(Measurement::Real(1.0).data_type(), DataType::kInteger);
        EXPECT_EQ(Measurement::Integer(0).data_type(), DataType::kInteger);
        EXPECT_EQ(Measurement::Complex(std::complex<double>(0, 0)).data_type(), DataType::kComplex);
        EXPECT_EQ(Measurement::String(std::string("ok")).data_type(), DataType::kString);
    }

    TEST(MeasurementTest, AsScalar)
    {
        Measurement m = Measurement::Integer(42);
        EXPECT_EQ(m.data_kind(), DataKind::kScalar);
        EXPECT_EQ(m.as_scalar<int>(), 42);

        Measurement m2 = Measurement::String(std::string("abc"));
        EXPECT_EQ(m2.data_kind(), DataKind::kScalar);
        EXPECT_EQ(m2.as_scalar<std::string>(), "abc");
    }

    // =========================================================================
    // DataFrameRow
    // =========================================================================

    TEST(DataFrameRowTest, FormatMultiIndex)
    {
        DataFrameRow row;
        row.multi_index = {0, 1, 2};
        EXPECT_EQ(row.FormatMultiIndex(), "0,1,2");
    }

    // =========================================================================
    // DataFrame lazy loading & chunking
    // =========================================================================

    TEST(DataFrameLazyTest, RowsLoadedOnFirstAccess)
    {
        int call_count = 0;
        TestDataFrame model;
        model.ConfigureDynamic({"v"}, 100,
            [&](Index start, Index end) -> std::vector<DataFrameRow>
            {
                ++call_count;
                std::vector<DataFrameRow> rows;
                for (Index i = start; i < end; ++i)
                {
                    DataFrameRow r;
                    r.multi_index = {i};
                    r.fields.push_back(Measurement::Integer(static_cast<int>(i)));
                    rows.push_back(std::move(r));
                }
                return rows;
            }, 50);

        EXPECT_EQ(call_count, 0);
        EXPECT_EQ(model.GetRow(0).fields[0].to_string(), "0");
        EXPECT_EQ(call_count, 1);           // chunk [0, 50) loaded
        EXPECT_EQ(model.GetRow(49).fields[0].to_string(), "49");
        EXPECT_EQ(call_count, 1);           // same chunk, no reload
        EXPECT_EQ(model.GetRow(50).fields[0].to_string(), "50");
        EXPECT_EQ(call_count, 2);           // chunk [50, 100) loaded
    }

    TEST(DataFrameLazyTest, CacheHitDoesNotReload)
    {
        int call_count = 0;
        TestDataFrame model;
        model.ConfigureDynamic({"v"}, 10,
            [&](Index start, Index end) -> std::vector<DataFrameRow>
            {
                ++call_count;
                std::vector<DataFrameRow> rows;
                for (Index i = start; i < end; ++i)
                {
                    DataFrameRow r;
                    r.multi_index = {i};
                    rows.push_back(std::move(r));
                }
                return rows;
            });

        model.GetRow(0);
        model.GetRow(5);
        model.GetRow(9);
        EXPECT_EQ(call_count, 1);
    }

    TEST(DataFrameLazyTest, MetadataAvailableBeforeLoad)
    {
        TestDataFrame model;
        model.ConfigureDynamic({"x", "y"}, 5,
            [](Index, Index) -> std::vector<DataFrameRow> { return {}; });
        EXPECT_EQ(model.row_count(), 5u);
        ASSERT_EQ(model.headers().size(), 2u);
        EXPECT_EQ(model.headers()[0], "x");
        EXPECT_EQ(model.headers()[1], "y");
    }

    // =========================================================================
    // DataFrame static configuration
    // =========================================================================

    TEST(DataFrameStaticTest, ImmediateAccessAfterConstruction)
    {
        DataFrameRow r1;
        r1.multi_index = {0};
        r1.fields.push_back(Measurement::Integer(10));

        DataFrameRow r2;
        r2.multi_index = {1};
        r2.fields.push_back(Measurement::Integer(20));

        std::vector<DataFrameRow> rows;
        rows.push_back(r1);
        rows.push_back(r2);

        TestDataFrame model;
        model.ConfigureStatic({"val"}, std::move(rows));

        EXPECT_EQ(model.row_count(), 2u);
        ASSERT_EQ(model.headers().size(), 1u);
        EXPECT_EQ(model.headers()[0], "val");

        // Rows immediately accessible without any generator.
        EXPECT_EQ(model.GetRow(0).fields[0].to_string(), "10");
        EXPECT_EQ(model.GetRow(1).fields[0].to_string(), "20");
    }

    TEST(DataFrameStaticTest, ToCsvFromStatic)
    {
        DataFrameRow r;
        r.multi_index = {42};
        r.fields.push_back(Measurement::Real(3.14));
        r.fields.push_back(Measurement::Integer(7));

        std::vector<DataFrameRow> rows;
        rows.push_back(r);

        TestDataFrame model;
        model.ConfigureStatic({"x", "y"}, std::move(rows));

        const std::string csv = model.ToCsv();
        EXPECT_NE(csv.find(",x,y"), std::string::npos);
        EXPECT_NE(csv.find("42,3.14,7"), std::string::npos);
    }

    // =========================================================================
    // DataFrame CSV output
    // =========================================================================

    TEST(DataFrameCsvTest, ToCsvFromBlock)
    {
        Block block(MakeValueRichCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        const std::string csv = table.ToCsv();
        EXPECT_NE(csv.find(",x,y,z"), std::string::npos);
        EXPECT_NE(csv.find("\"0,0\",10,1,100"), std::string::npos);
        EXPECT_NE(csv.find("\"1,2\",20,3,105"), std::string::npos);
    }

    TEST(DataFrameCsvTest, ToCsvFromDataArray)
    {
        Block block(MakeRaggedCreateInfo());
        DataArray y = block.GetOrCreateDataArray("y");
        const DataFrame& table = y.GetOrCreateDataFrame("y");

        const std::string csv = table.ToCsv();
        EXPECT_NE(csv.find(",x,y"), std::string::npos);
        EXPECT_NE(csv.find("\"1,1\",20,3"), std::string::npos);
    }

    TEST(DataFrameCsvTest, WriteToCsvRejectsEmptyPath)
    {
        TestDataFrame model;
        model.ConfigureDynamic({"a"}, 1,
            [](Index, Index) -> std::vector<DataFrameRow> { return {}; });
        EXPECT_THROW(model.WriteToCsv(""), std::invalid_argument);
    }

    // =========================================================================
    // DataFrame to_string (ASCII table)
    // =========================================================================

    TEST(DataFrameToStringTest, StaticSmallTableNoTruncation)
    {
        // 3 rows, max_display_rows=10 â†?all rows shown, no ellipsis
        std::vector<DataFrameRow> rows;
        for (int i = 0; i < 3; ++i)
        {
            DataFrameRow r;
            r.multi_index = {i};
            r.fields.push_back(Measurement::Integer(i * 10));
            r.fields.push_back(Measurement::String(std::string(1, 'A' + i)));
            rows.push_back(std::move(r));
        }

        TestDataFrame model;
        model.ConfigureStatic({"val", "label"}, std::move(rows));

        const std::string out = model.to_string(10);

        // Should contain the header row
        EXPECT_NE(out.find("| # "), std::string::npos);
        EXPECT_NE(out.find("| val "), std::string::npos);
        EXPECT_NE(out.find("| label "), std::string::npos);
        // Should contain all data rows
        EXPECT_NE(out.find("| 0 "), std::string::npos);
        EXPECT_NE(out.find("| 1 "), std::string::npos);
        EXPECT_NE(out.find("| 2 "), std::string::npos);
        EXPECT_NE(out.find("| 0 "), std::string::npos);
        EXPECT_NE(out.find("| 10 "), std::string::npos);
        EXPECT_NE(out.find("| 20 "), std::string::npos);
        EXPECT_NE(out.find("| A "), std::string::npos);
        EXPECT_NE(out.find("| B "), std::string::npos);
        EXPECT_NE(out.find("| C "), std::string::npos);
        // No ellipsis and no footer
        EXPECT_EQ(out.find("..."), std::string::npos);
        EXPECT_EQ(out.find("omitted"), std::string::npos);
    }

    TEST(DataFrameToStringTest, StaticTruncatedHeadOnly)
    {
        // 10 rows, max_display_rows=6 â†?head-only truncation
        // rows 0-5 shown, then ellipsis, then footer
        std::vector<DataFrameRow> rows;
        for (int i = 0; i < 10; ++i)
        {
            DataFrameRow r;
            r.multi_index = {i};
            r.fields.push_back(Measurement::Integer(i));
            rows.push_back(std::move(r));
        }

        TestDataFrame model;
        model.ConfigureStatic({"n"}, std::move(rows));

        const std::string out = model.to_string(6);

        // Should have ellipsis row at the end
        EXPECT_NE(out.find("..."), std::string::npos);
        // Footer: 4 rows omitted, 6 shown
        EXPECT_NE(out.find("omitted"), std::string::npos);
        EXPECT_NE(out.find("4 row"), std::string::npos);
        EXPECT_NE(out.find("6 shown"), std::string::npos);
        // Head rows 0-5 should appear
        EXPECT_NE(out.find("| 0 "), std::string::npos);
        EXPECT_NE(out.find("| 5 "), std::string::npos);
        // Tail rows should NOT appear
        EXPECT_EQ(out.find("| 6 "), std::string::npos);
        EXPECT_EQ(out.find("| 9 "), std::string::npos);
    }

    TEST(DataFrameToStringTest, TruncationFooterPlural)
    {
        // 10 rows, max_display_rows=6 â†?4 rows omitted (plural)
        std::vector<DataFrameRow> rows;
        for (int i = 0; i < 10; ++i)
        {
            DataFrameRow r;
            r.multi_index = {i};
            r.fields.push_back(Measurement::Integer(i));
            rows.push_back(std::move(r));
        }

        TestDataFrame model;
        model.ConfigureStatic({"n"}, std::move(rows));
        const std::string out = model.to_string(6);
        // Plural "4 rows omitted"
        EXPECT_NE(out.find("4 rows omitted"), std::string::npos);
    }

    TEST(DataFrameToStringTest, TruncationFooterSingular)
    {
        // 9 rows, max_display_rows=8 â†?1 row omitted (singular)
        std::vector<DataFrameRow> rows;
        for (int i = 0; i < 9; ++i)
        {
            DataFrameRow r;
            r.multi_index = {i};
            r.fields.push_back(Measurement::Integer(i));
            rows.push_back(std::move(r));
        }

        TestDataFrame model;
        model.ConfigureStatic({"n"}, std::move(rows));
        const std::string out = model.to_string(8);
        // Singular "1 row omitted"
        EXPECT_NE(out.find("1 row omitted"), std::string::npos);
    }

    TEST(DataFrameToStringTest, ExactFitNoTruncation)
    {
        // 6 rows, max_display_rows=6 â†?exact fit, no truncation
        std::vector<DataFrameRow> rows;
        for (int i = 0; i < 6; ++i)
        {
            DataFrameRow r;
            r.multi_index = {i};
            r.fields.push_back(Measurement::Integer(i));
            rows.push_back(std::move(r));
        }

        TestDataFrame model;
        model.ConfigureStatic({"n"}, std::move(rows));
        const std::string out = model.to_string(6);

        EXPECT_EQ(out.find("..."), std::string::npos);
        EXPECT_EQ(out.find("omitted"), std::string::npos);
    }

    TEST(DataFrameToStringTest, MaxDisplayRowsOf1Works)
    {
        // Passing 1 shows exactly 1 row (no clamp needed in head-only mode)
        std::vector<DataFrameRow> rows;
        for (int i = 0; i < 5; ++i)
        {
            DataFrameRow r;
            r.multi_index = {i};
            r.fields.push_back(Measurement::Integer(i));
            rows.push_back(std::move(r));
        }

        TestDataFrame model;
        model.ConfigureStatic({"n"}, std::move(rows));
        const std::string out = model.to_string(1);

        // Should show row 0 and then ellipsis + footer
        EXPECT_NE(out.find("| 0 "), std::string::npos);
        EXPECT_NE(out.find("..."), std::string::npos);
        EXPECT_NE(out.find("4 rows omitted"), std::string::npos);
        // Should be a valid table
        EXPECT_NE(out.find('+'), std::string::npos);
        EXPECT_NE(out.find('|'), std::string::npos);
    }

    TEST(DataFrameToStringTest, DefaultParameterIs32)
    {
        // Only 10 rows â€?default 32 should show all without truncation
        std::vector<DataFrameRow> rows;
        for (int i = 0; i < 10; ++i)
        {
            DataFrameRow r;
            r.multi_index = {i};
            r.fields.push_back(Measurement::Integer(i));
            rows.push_back(std::move(r));
        }

        TestDataFrame model;
        model.ConfigureStatic({"n"}, std::move(rows));
        const std::string out = model.to_string();  // default = 32

        EXPECT_EQ(out.find("..."), std::string::npos);
        EXPECT_EQ(out.find("omitted"), std::string::npos);
        EXPECT_NE(out.find("| 0 "), std::string::npos);
        EXPECT_NE(out.find("| 9 "), std::string::npos);
    }

    TEST(DataFrameToStringTest, FromBlockNoTruncation)
    {
        Block block(MakeValueRichCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        const std::string out = table.to_string(10);

        // 2x3 = 6 rows, all should fit
        EXPECT_EQ(out.find("..."), std::string::npos);
        EXPECT_EQ(out.find("omitted"), std::string::npos);
        // Header
        EXPECT_NE(out.find("| # "), std::string::npos);
        EXPECT_NE(out.find("| x "), std::string::npos);
        EXPECT_NE(out.find("| y "), std::string::npos);
        EXPECT_NE(out.find("| z "), std::string::npos);
        // Data
        EXPECT_NE(out.find("0,0"), std::string::npos);
        EXPECT_NE(out.find("1,2"), std::string::npos);
    }

    TEST(DataFrameToStringTest, FromBlockTruncated)
    {
        // 2x3x4 = 24 rows, max_display_rows=8 â†?truncation
        Block block(MakeThreeDimMultiDepCreateInfo());
        const DataFrame& table = block.GetOrCreateDataFrame();

        const std::string out = table.to_string(8);

        EXPECT_NE(out.find("..."), std::string::npos);
        EXPECT_NE(out.find("omitted"), std::string::npos);
    }

    TEST(DataFrameToStringTest, FromDataArrayNoTruncation)
    {
        Block block(MakeValueRichCreateInfo());
        DataArray arr = block.GetOrCreateDataArray("z");
        const DataFrame& table = arr.GetOrCreateDataFrame();

        const std::string out = table.to_string(10);

        EXPECT_EQ(out.find("..."), std::string::npos);
        EXPECT_EQ(out.find("omitted"), std::string::npos);
        EXPECT_NE(out.find("| # "), std::string::npos);
        EXPECT_NE(out.find("| data "), std::string::npos);
    }

    TEST(DataFrameToStringTest, HasTableBorders)
    {
        std::vector<DataFrameRow> rows;
        DataFrameRow r;
        r.multi_index = {0};
        r.fields.push_back(Measurement::Integer(42));
        rows.push_back(std::move(r));

        TestDataFrame model;
        model.ConfigureStatic({"val"}, std::move(rows));
        const std::string out = model.to_string();

        // Should have box-drawing characters
        EXPECT_NE(out.find('+'), std::string::npos);
        EXPECT_NE(out.find('|'), std::string::npos);
        EXPECT_NE(out.find('-'), std::string::npos);
    }

    TEST(DataFrameToStringTest, WideColumnAlignment)
    {
        // Values of differing lengths should still produce aligned columns
        std::vector<DataFrameRow> rows;
        {
            DataFrameRow r;
            r.multi_index = {0};
            r.fields.push_back(Measurement::Integer(1));
            r.fields.push_back(Measurement::String(std::string("short")));
            rows.push_back(std::move(r));
        }
        {
            DataFrameRow r;
            r.multi_index = {1};
            r.fields.push_back(Measurement::Integer(99999));
            r.fields.push_back(Measurement::String(std::string("very_long_string")));
            rows.push_back(std::move(r));
        }

        TestDataFrame model;
        model.ConfigureStatic({"num", "text"}, std::move(rows));
        const std::string out = model.to_string();

        // Both rows present
        EXPECT_NE(out.find("short"), std::string::npos);
        EXPECT_NE(out.find("very_long_string"), std::string::npos);
        // Borders present (implies not broken)
        EXPECT_NE(out.find('+'), std::string::npos);
    }

    TEST(DataFrameToStringTest, WithMultiIndex)
    {
        std::vector<DataFrameRow> rows;
        DataFrameRow r;
        r.multi_index = {1, 2, 3};
        r.fields.push_back(Measurement::Real(100.0));
        rows.push_back(std::move(r));

        TestDataFrame model;
        model.ConfigureStatic({"val"}, std::move(rows));
        const std::string out = model.to_string();

        EXPECT_NE(out.find("1,2,3"), std::string::npos);
    }
} // namespace xdataset
