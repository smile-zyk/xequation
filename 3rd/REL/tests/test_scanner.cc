// Scanner tests powered by GoogleTest.

#include "scanner.h"
#include "token.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using rel::Scanner;
using rel::Token;
using rel::TokenType;

namespace
{
    std::vector<Token> scan(const std::string& src)
    {
        Scanner s(src);
        rel::ScanResult sr = s.Scan();
        return sr.tokens;
    }

    rel::ScanResult scan_with_errors(const std::string& src)
    {
        Scanner s(src);
        return s.Scan();
    }
}

// =========================================================================
//  Basic shape / EOF
// =========================================================================

TEST(ScannerTest, EmptySourceEmitsOnlyEof)
{
    auto t = scan("");
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].type, TokenType::END_OF_INPUT);
}

TEST(ScannerTest, WhitespaceOnlySource)
{
    auto t = scan(" \t\f\r");
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0].type, TokenType::END_OF_INPUT);
}

// =========================================================================
//  Keywords vs identifiers (Spec 1.1 / 1.7 rule 4)
// =========================================================================

TEST(ScannerTest, AllKeywords)
{
    auto t = scan("if then elseif else AND OR NOT EQUALS NOTEQUALS TRUE FALSE");
    ASSERT_EQ(t.size(), 12u);
    EXPECT_EQ(t[0].type, TokenType::KW_IF);
    EXPECT_EQ(t[1].type, TokenType::KW_THEN);
    EXPECT_EQ(t[2].type, TokenType::KW_ELSEIF);
    EXPECT_EQ(t[3].type, TokenType::KW_ELSE);
    EXPECT_EQ(t[4].type, TokenType::KW_AND);
    EXPECT_EQ(t[5].type, TokenType::KW_OR);
    EXPECT_EQ(t[6].type, TokenType::KW_NOT);
    EXPECT_EQ(t[7].type, TokenType::KW_EQUALS);
    EXPECT_EQ(t[8].type, TokenType::KW_NOTEQUALS);
    EXPECT_EQ(t[9].type, TokenType::KW_TRUE);
    EXPECT_EQ(t[10].type, TokenType::KW_FALSE);
    EXPECT_EQ(t[11].type, TokenType::END_OF_INPUT);
}

TEST(ScannerTest, BuiltinConstantsAreIdentifiers)
{
    auto t = scan("PI e ln10 reference");
    ASSERT_EQ(t.size(), 5u);
    EXPECT_EQ(t[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[0].lexeme, "PI");
    EXPECT_EQ(t[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[1].lexeme, "e");
    EXPECT_EQ(t[2].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[2].lexeme, "ln10");
}

TEST(ScannerTest, KeywordsAreCaseSensitive)
{
    auto t = scan("and or not equals notequals true false");
    ASSERT_EQ(t.size(), 8u);
    for (int i = 0; i < 7; ++i)
        EXPECT_EQ(t[i].type, TokenType::IDENTIFIER);
}

// =========================================================================
//  Operators (Spec 1.2 + 1.7 rule 1)
// =========================================================================

TEST(ScannerTest, MultiCharOperators)
{
    auto t = scan("** :: << >> >= <= == != && || ..");
    EXPECT_EQ(t[0].type, TokenType::OP_POW);
    EXPECT_EQ(t[1].type, TokenType::OP_SEQ);
    EXPECT_EQ(t[2].type, TokenType::OP_SHL);
    EXPECT_EQ(t[3].type, TokenType::OP_SHR);
    EXPECT_EQ(t[4].type, TokenType::OP_GE);
    EXPECT_EQ(t[5].type, TokenType::OP_LE);
    EXPECT_EQ(t[6].type, TokenType::OP_EQ);
    EXPECT_EQ(t[7].type, TokenType::OP_NE);
    EXPECT_EQ(t[8].type, TokenType::OP_LAND);
    EXPECT_EQ(t[9].type, TokenType::OP_LOR);
    EXPECT_EQ(t[10].type, TokenType::DDOT);
}

TEST(ScannerTest, SingleCharOperators)
{
    auto t = scan("+ - * / % ^ | & ~ ! ? : < >");
    TokenType expected[] = {
        TokenType::OP_ADD, TokenType::OP_SUB, TokenType::OP_MUL, TokenType::OP_DIV,
        TokenType::OP_MOD, TokenType::OP_BXOR, TokenType::OP_BOR, TokenType::OP_BAND,
        TokenType::OP_BNOT, TokenType::OP_LNOT, TokenType::OP_QMARK, TokenType::OP_COLON,
        TokenType::OP_LT, TokenType::OP_GT,
    };
    for (std::size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
        EXPECT_EQ(t[i].type, expected[i]);
}

TEST(ScannerTest, MaximalMunchPackedOperators)
{
    auto t = scan("**::==!=");
    EXPECT_EQ(t[0].type, TokenType::OP_POW);
    EXPECT_EQ(t[1].type, TokenType::OP_SEQ);
    EXPECT_EQ(t[2].type, TokenType::OP_EQ);
    EXPECT_EQ(t[3].type, TokenType::OP_NE);
}

TEST(ScannerTest, Delimiters)
{
    auto t = scan("()[]{},");
    EXPECT_EQ(t[0].type, TokenType::LPAREN);
    EXPECT_EQ(t[1].type, TokenType::RPAREN);
    EXPECT_EQ(t[2].type, TokenType::LBRACKET);
    EXPECT_EQ(t[3].type, TokenType::RBRACKET);
    EXPECT_EQ(t[4].type, TokenType::LBRACE);
    EXPECT_EQ(t[5].type, TokenType::RBRACE);
    EXPECT_EQ(t[6].type, TokenType::COMMA);
}

// =========================================================================
//  Dot vs DDOT (Spec 1.7 rule 1)
// =========================================================================

TEST(ScannerTest, DotMemberAccessVsCrossDataset)
{
    auto t = scan("a.b a..b");
    EXPECT_EQ(t[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[1].type, TokenType::DOT);
    EXPECT_EQ(t[2].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[3].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[4].type, TokenType::DDOT);
    EXPECT_EQ(t[5].type, TokenType::IDENTIFIER);
}

TEST(ScannerTest, DdotWinsAgainstRealIn5dotdot7)
{
    auto t = scan("5..7");
    ASSERT_EQ(t.size(), 4u);
    EXPECT_EQ(t[0].type, TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[0].lexeme, "5");
    EXPECT_EQ(t[1].type, TokenType::DDOT);
    EXPECT_EQ(t[2].type, TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[2].lexeme, "7");
}

TEST(ScannerTest, LeadingDotRealLiteral)
{
    auto t = scan(".5 .25e6");
    EXPECT_EQ(t[0].type, TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[0].lexeme, ".5");
    EXPECT_EQ(t[1].lexeme, ".25e6");
}

// =========================================================================
//  Numeric literals (Spec 1.4 / 1.7 rule 2)
// =========================================================================

TEST(ScannerTest, IntegerLiterals)
{
    auto t = scan("0 123 0xFF 0x1f 077");
    EXPECT_EQ(t[0].lexeme, "0");
    EXPECT_EQ(t[1].lexeme, "123");
    EXPECT_EQ(t[2].lexeme, "0xFF");
    EXPECT_EQ(t[3].lexeme, "0x1f");
    EXPECT_EQ(t[4].lexeme, "077");
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(t[i].type, TokenType::NUMERIC_BASE);
}

TEST(ScannerTest, RealLiterals)
{
    auto t = scan("0.5 1.5e-3 1e5 1e+5");
    EXPECT_EQ(t[0].lexeme, "0.5");
    EXPECT_EQ(t[1].lexeme, "1.5e-3");
    EXPECT_EQ(t[2].lexeme, "1e5");
    EXPECT_EQ(t[3].lexeme, "1e+5");
}

TEST(ScannerTest, ImaginaryLiterals)
{
    auto t = scan("2i 3.5i 1e5i");
    EXPECT_EQ(t[0].lexeme, "2i");
    EXPECT_EQ(t[1].lexeme, "3.5i");
    EXPECT_EQ(t[2].lexeme, "1e5i");
}

TEST(ScannerTest, ExponentRollbackOnMissingDigits)
{
    auto t = scan("1e");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].type, TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[0].lexeme, "1");
    EXPECT_EQ(t[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[1].lexeme, "e");
}

TEST(ScannerTest, HexDoesNotEatTrailingI)
{
    auto t = scan("0x1Fi");
    EXPECT_EQ(t[0].type, TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[0].lexeme, "0x1F");
    EXPECT_EQ(t[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[1].lexeme, "i");
}

// =========================================================================
//  Numeric suffix (Spec 1.5 / 1.7 rule 3)
// =========================================================================

TEST(ScannerTest, UnitAlone)
{
    auto t = scan("8Hz");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].type, TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[1].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[1].lexeme, "Hz");
}

TEST(ScannerTest, ScaleFactorWithUnit)
{
    auto t = scan("8kHz");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].type, TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[1].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[1].lexeme, "kHz");
}

TEST(ScannerTest, ScaleFactorAlone)
{
    auto t = scan("8M");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].type, TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[1].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[1].lexeme, "M");
}

TEST(ScannerTest, PredefScaledUnit)
{
    auto t = scan("8mil 8mils 8cm 8dB");
    EXPECT_EQ(t[1].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[1].lexeme, "mil");
    EXPECT_EQ(t[3].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[3].lexeme, "mils");
    EXPECT_EQ(t[5].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[5].lexeme, "cm");
    EXPECT_EQ(t[7].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[7].lexeme, "dB");
}

TEST(ScannerTest, UnitLongestMatchMetersOverMeter)
{
    auto t = scan("8meters 8meter");
    EXPECT_EQ(t[1].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[1].lexeme, "meters");
    EXPECT_EQ(t[3].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[3].lexeme, "meter");
}

TEST(ScannerTest, PredefOverridesScaleFactorForm)
{
    auto t = scan("8mil");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[1].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[1].lexeme, "mil");
}

TEST(ScannerTest, PredefFollowedByIdentifierChars)
{
    auto t = scan("8milky");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].type, TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(t[1].lexeme, "milky");
}

TEST(ScannerTest, UnitAloneMeterNotScaleFactorMEter)
{
    auto t = scan("8meter");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[1].type, TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[1].lexeme, "meter");
}

// =========================================================================
//  String / raw-string literals (Spec 1.3)
// =========================================================================

TEST(ScannerTest, SimpleStringLiteral)
{
    auto t = scan("\"hello world\"");
    EXPECT_EQ(t[0].type, TokenType::STRING_LITERAL);
    EXPECT_EQ(t[0].lexeme, "\"hello world\"");
}

TEST(ScannerTest, StringWithEscapes)
{
    auto t = scan("\"a\\nb\\\"c\\x1Fd\\0123e\"");
    EXPECT_EQ(t[0].type, TokenType::STRING_LITERAL);
}

TEST(ScannerTest, UnterminatedStringIsInvalid)
{
    auto r = scan_with_errors("\"oops");
    ASSERT_FALSE(r.Ok());
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0].kind, rel::ErrorKind::Lexical);
}

TEST(ScannerTest, BadEscapeIsInvalid)
{
    auto r = scan_with_errors("\"\\q\"");
    ASSERT_FALSE(r.Ok());
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0].kind, rel::ErrorKind::Lexical);
}

TEST(ScannerTest, RawStringLiteral)
{
    auto t = scan("''verbatim \\n no escapes''");
    EXPECT_EQ(t[0].type, TokenType::RAW_STRING_LITERAL);
    EXPECT_EQ(t[0].lexeme, "''verbatim \\n no escapes''");
}

// =========================================================================
//  Error path
// =========================================================================

TEST(ScannerTest, UnknownCharacterIsInvalid)
{
    auto r = scan_with_errors("@");
    ASSERT_FALSE(r.Ok());
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0].kind, rel::ErrorKind::Lexical);
}

TEST(ScannerTest, BareEqualsIsInvalid)
{
    auto r = scan_with_errors("=");
    ASSERT_FALSE(r.Ok());
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0].kind, rel::ErrorKind::Lexical);
}

// =========================================================================
//  Position tracking
// =========================================================================

TEST(ScannerTest, LineAndColumnTracking)
{
    auto t = scan("if x + 1");
    EXPECT_EQ(t[0].line, 1);
    EXPECT_EQ(t[0].column, 1);
    EXPECT_EQ(t[1].column, 4);
    EXPECT_EQ(t[2].column, 6);
    EXPECT_EQ(t[3].column, 8);
}

TEST(ScannerTest, InitialLineOffset)
{
    Scanner s("foo", 42);
    rel::ScanResult sr = s.Scan();
    ASSERT_TRUE(sr.Ok());
    EXPECT_EQ(sr.tokens[0].line, 42);
    EXPECT_EQ(sr.tokens[0].column, 1);
}

// =========================================================================
//  A realistic expression
// =========================================================================

TEST(ScannerTest, RealisticIfExpression)
{
    auto t = scan("if (x >= 8kHz) then 1.5e-3 else TRUE");
    EXPECT_EQ(t[0].type,  TokenType::KW_IF);
    EXPECT_EQ(t[1].type,  TokenType::LPAREN);
    EXPECT_EQ(t[2].type,  TokenType::IDENTIFIER);
    EXPECT_EQ(t[2].lexeme, "x");
    EXPECT_EQ(t[3].type,  TokenType::OP_GE);
    EXPECT_EQ(t[4].type,  TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[4].lexeme, "8");
    EXPECT_EQ(t[5].type,  TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ(t[5].lexeme, "kHz");
    EXPECT_EQ(t[6].type,  TokenType::RPAREN);
    EXPECT_EQ(t[7].type,  TokenType::KW_THEN);
    EXPECT_EQ(t[8].type,  TokenType::NUMERIC_BASE);
    EXPECT_EQ(t[8].lexeme, "1.5e-3");
    EXPECT_EQ(t[9].type,  TokenType::KW_ELSE);
    EXPECT_EQ(t[10].type, TokenType::KW_TRUE);
    EXPECT_EQ(t[11].type, TokenType::END_OF_INPUT);
}
