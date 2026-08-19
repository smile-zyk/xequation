#include "ast_printer.h"
#include "parser.h"
#include "scanner.h"

#include <gtest/gtest.h>

#include <string>

namespace
{
    rel::ParseResult parse_line(const std::string& source)
    {
        rel::Scanner scanner(source);
        rel::ScanResult sr = scanner.Scan();
        EXPECT_TRUE(sr.Ok());
        rel::Parser parser(std::move(sr.tokens));
        return parser.Parse();
    }

    std::string print_ast(const rel::ParseResult& result)
    {
        rel::AstPrinter printer;
        return printer.Print(*result.expr);
    }
}

TEST(ParserTest, ParsesOperatorPrecedence)
{
    rel::ParseResult result = parse_line("1 + 2 * 3");
    ASSERT_TRUE(result.Ok()) << (result.errors.empty() ? "" : result.errors[0].message);
    EXPECT_EQ(print_ast(result), "(+ 1 (* 2 3))");
}

TEST(ParserTest, ParsesIfExpression)
{
    rel::ParseResult result =
        parse_line("if (a > 0) then 1 elseif (a < 0) then -1 else 0");
    ASSERT_TRUE(result.Ok()) << (result.errors.empty() ? "" : result.errors[0].message);
    EXPECT_EQ(print_ast(result),
              "(if (branch (> a 0) 1) (branch (< a 0) (- 1)) (else 0))");
}

TEST(ParserTest, TernaryIsLeftAssociative)
{
    rel::ParseResult result = parse_line("a ? b : c ? d : e");
    ASSERT_TRUE(result.Ok()) << (result.errors.empty() ? "" : result.errors[0].message);
    EXPECT_EQ(print_ast(result), "(?: (?: a b c) d e)");
}

TEST(ParserTest, PowerIsLeftAssociativeWithUnaryRhs)
{
    rel::ParseResult result = parse_line("2 ** -3 ** 4");
    ASSERT_TRUE(result.Ok()) << (result.errors.empty() ? "" : result.errors[0].message);
    EXPECT_EQ(print_ast(result), "(** (** 2 (- 3)) 4)");
}

TEST(ParserTest, BareSeqAllowedInIndexOnly)
{
    rel::ParseResult ok = parse_line("a[::, 1]");
    ASSERT_TRUE(ok.Ok()) << (ok.errors.empty() ? "" : ok.errors[0].message);
    EXPECT_EQ(print_ast(ok), "(index a :: 1)");

    rel::ParseResult bad = parse_line("[::]");
    ASSERT_FALSE(bad.Ok());
    ASSERT_FALSE(bad.errors.empty());
}

TEST(ParserTest, ValidatesDefaultCallSlots)
{
    rel::ParseResult ok = parse_line("func(,,a)");
    ASSERT_TRUE(ok.Ok()) << (ok.errors.empty() ? "" : ok.errors[0].message);
    EXPECT_EQ(print_ast(ok), "(call func <empty> <empty> a)");

    rel::ParseResult pure_default = parse_line("func(,,)");
    ASSERT_FALSE(pure_default.Ok());

    rel::ParseResult trailing_default = parse_line("func(,,a,,)");
    ASSERT_FALSE(trailing_default.Ok());
}
