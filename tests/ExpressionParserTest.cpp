#include <gtest/gtest.h>

#include "logicbench/Expression.h"

#include <map>
#include <string>

using namespace lb;

namespace {

constexpr Signal L = Signal::Low;
constexpr Signal H = Signal::High;
constexpr Signal X = Signal::Undefined;

std::string reparse(const std::string& text)
{
    return ExpressionParser::parse(text).front().root->toString();
}

/// Where the parser reports the first error, as "line:column".
std::string errorAt(const std::string& text)
{
    try {
        ExpressionParser::parse(text);
    } catch (const ExpressionSyntaxError& e) {
        return std::to_string(e.line()) + ":" + std::to_string(e.column());
    }
    return "no error";
}

} // namespace

TEST(ExpressionParserTest, ReadsOneEquationPerLine)
{
    const auto eqs = ExpressionParser::parse("S    = A ^ B ^ Cin\nCout = (A & B) | (Cin & (A ^ B))\n");
    ASSERT_EQ(eqs.size(), 2u);
    EXPECT_EQ(eqs[0].output, "S");
    EXPECT_EQ(eqs[1].output, "Cout");
    EXPECT_EQ(eqs[0].line, 1);
    EXPECT_EQ(eqs[1].line, 2);
}

TEST(ExpressionParserTest, PrecedenceIsNotThenAndThenXorThenOr)
{
    EXPECT_EQ(reparse("Y = A | B & C"), "(A | (B & C))");
    EXPECT_EQ(reparse("Y = A ^ B & C"), "(A ^ (B & C))");
    EXPECT_EQ(reparse("Y = A | B ^ C"), "(A | (B ^ C))");
    EXPECT_EQ(reparse("Y = !A & B"), "(!A & B)");
    EXPECT_EQ(reparse("Y = !(A & B)"), "!(A & B)");
    EXPECT_EQ(reparse("Y = !!A"), "!!A");
}

TEST(ExpressionParserTest, ParenthesesOverridePrecedence)
{
    EXPECT_EQ(reparse("Y = (A | B) & C"), "((A | B) & C)");
}

TEST(ExpressionParserTest, ChainsOfOneOperatorPrintAsOneGroup)
{
    EXPECT_EQ(reparse("Y = A & B & C & D"), "(A & B & C & D)");
}

TEST(ExpressionParserTest, ToStringReadsBackToTheSameTree)
{
    for (const char* text : {"Y = A | B & !C", "Y = (A ^ B) & (C | !D)", "Y = !(A | B) ^ C"}) {
        const std::string once = reparse(text);
        EXPECT_EQ(reparse("Y = " + once), once) << text;
    }
}

TEST(ExpressionParserTest, IgnoresBlankLinesCommentsAndSpacing)
{
    const auto eqs = ExpressionParser::parse("\n# a full adder\n\n  S=A^B^C   # sum\n\t\nC2 = A&B\n");
    ASSERT_EQ(eqs.size(), 2u);
    EXPECT_EQ(eqs[0].line, 4);
    EXPECT_EQ(eqs[1].line, 6);
}

TEST(ExpressionParserTest, NamesMayHaveDigitsAndUnderscores)
{
    const auto eqs = ExpressionParser::parse("out_1 = a0 & B_2");
    std::vector<std::string> names;
    eqs[0].root->collectVariables(names);
    EXPECT_EQ(names, (std::vector<std::string>{"a0", "B_2"}));
}

TEST(ExpressionParserTest, EvaluatesWithKleeneLogic)
{
    const auto eqs = ExpressionParser::parse("Y = A & !B | C ^ D");
    const Expr& y = *eqs[0].root;
    EXPECT_EQ(y.evaluate({{"A", H}, {"B", L}, {"C", L}, {"D", L}}), H);
    EXPECT_EQ(y.evaluate({{"A", L}, {"B", L}, {"C", H}, {"D", L}}), H);
    EXPECT_EQ(y.evaluate({{"A", L}, {"B", H}, {"C", H}, {"D", H}}), L);
    EXPECT_EQ(y.evaluate({{"A", H}, {"B", L}}), H);           // C ^ D unknown, but OR is already 1
    EXPECT_EQ(y.evaluate({{"A", L}}), X);
}

TEST(ExpressionParserTest, MalformedInputGivesTheRightLineAndColumn)
{
    EXPECT_EQ(errorAt(""), "1:1");                            // nothing to build
    EXPECT_EQ(errorAt("= A & B"), "1:1");                     // no output name
    EXPECT_EQ(errorAt("Y A & B"), "1:3");                     // no '='
    EXPECT_EQ(errorAt("Y ="), "1:4");                         // nothing after '='
    EXPECT_EQ(errorAt("Y = A &"), "1:8");                     // operand missing at the end
    EXPECT_EQ(errorAt("Y = A & & B"), "1:9");                 // operator where a name belongs
    EXPECT_EQ(errorAt("Y = (A | B"), "1:11");                 // unclosed parenthesis
    EXPECT_EQ(errorAt("Y = A | B)"), "1:10");                 // unmatched ')'
    EXPECT_EQ(errorAt("Y = A + B"), "1:7");                   // unknown operator
    EXPECT_EQ(errorAt("Y = 1A"), "1:5");                      // a name cannot start with a digit
    EXPECT_EQ(errorAt("S = A\nC = B &"), "2:8");              // second line
    EXPECT_EQ(errorAt("S = A\n\n# x\nS = B"), "4:1");         // defined twice
}

TEST(ExpressionParserTest, AnEarlierOutputMayBeUsedButNotALaterOne)
{
    EXPECT_NO_THROW(ExpressionParser::parse("P = A ^ B\nS = P ^ C"));
    EXPECT_EQ(errorAt("S = P ^ C\nP = A ^ B"), "1:5");
    EXPECT_EQ(errorAt("S = A & S"), "1:9");
}
