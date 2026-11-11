#include "logicbench/Expression.h"

#include "logicbench/CircuitBuilder.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace lb {

// ----------------------------------------------------------------- nodes ---

Signal VarExpr::evaluate(const std::map<std::string, Signal>& values) const
{
    const auto it = values.find(m_name);
    return it == values.end() ? Signal::Undefined : it->second;
}

PinRef VarExpr::emitInto(CircuitBuilder& builder) const
{
    return builder.inputPin(m_name);
}

void VarExpr::collectVariables(std::vector<std::string>& names) const
{
    if (std::find(names.begin(), names.end(), m_name) == names.end())
        names.push_back(m_name);
}

Signal NotExpr::evaluate(const std::map<std::string, Signal>& values) const
{
    return !m_operand->evaluate(values);
}

PinRef NotExpr::emitInto(CircuitBuilder& builder) const
{
    return builder.gate("NOT", {m_operand->emitInto(builder)}, toString());
}

void NotExpr::collectVariables(std::vector<std::string>& names) const
{
    m_operand->collectVariables(names);
}

Signal BinaryExpr::evaluate(const std::map<std::string, Signal>& values) const
{
    const Signal a = m_left->evaluate(values);
    const Signal b = m_right->evaluate(values);
    switch (m_op) {
    case Op::And: return a & b;
    case Op::Or:  return a | b;
    default:      return a ^ b;
    }
}

namespace {

/// The operands of a chain of one operator, left to right: A ^ B ^ C is one
/// three-input XOR gate rather than two two-input ones.
void flatten(const Expr& node, BinaryExpr::Op op, std::vector<const Expr*>& out)
{
    const auto* binary = dynamic_cast<const BinaryExpr*>(&node);
    if (binary && binary->op() == op) {
        flatten(binary->left(), op, out);
        flatten(binary->right(), op, out);
    } else {
        out.push_back(&node);
    }
}

} // namespace

PinRef BinaryExpr::emitInto(CircuitBuilder& builder) const
{
    std::vector<const Expr*> operands;
    flatten(*this, m_op, operands);
    std::vector<PinRef> inputs;
    inputs.reserve(operands.size());
    for (const Expr* operand : operands)
        inputs.push_back(operand->emitInto(builder));
    const char* type = m_op == Op::And ? "AND" : (m_op == Op::Or ? "OR" : "XOR");
    return builder.gate(type, inputs, toString());
}

std::string BinaryExpr::toString() const
{
    std::vector<const Expr*> operands;
    flatten(*this, m_op, operands);
    std::string text = "(";
    for (std::size_t i = 0; i < operands.size(); ++i) {
        if (i)
            text += std::string(" ") + symbol(m_op) + " ";
        text += operands[i]->toString();
    }
    return text + ")";
}

void BinaryExpr::collectVariables(std::vector<std::string>& names) const
{
    m_left->collectVariables(names);
    m_right->collectVariables(names);
}

char BinaryExpr::symbol(Op op)
{
    switch (op) {
    case Op::And: return '&';
    case Op::Or:  return '|';
    default:      return '^';
    }
}

// ---------------------------------------------------------------- parser ---

namespace {

/// Recursive descent over one line. Precedence climbs or < xor < and < not.
class LineParser {
public:
    LineParser(const std::string& text, int line) : m_text(text), m_line(line) {}

    Equation parseEquation()
    {
        skipSpace();
        const int nameColumn = column();
        std::string output = readName();
        if (output.empty())
            fail(nameColumn, "expected an output name at the start of the line");
        skipSpace();
        if (peek() != '=')
            fail(column(), "expected '=' after the output name " + output);
        ++m_pos;
        skipSpace();
        if (atEnd())
            fail(column(), "expected an expression after '='");
        std::unique_ptr<Expr> root = parseOr();
        skipSpace();
        if (!atEnd()) {
            if (peek() == ')')
                fail(column(), "unmatched ')'");
            fail(column(), std::string("unexpected '") + peek() + "'");
        }
        return Equation{output, std::move(root), m_line};
    }

private:
    std::unique_ptr<Expr> parseOr()
    {
        std::unique_ptr<Expr> left = parseXor();
        while (skipSpace(), peek() == '|') {
            ++m_pos;
            left = std::make_unique<BinaryExpr>(BinaryExpr::Op::Or, std::move(left), parseXor());
        }
        return left;
    }

    std::unique_ptr<Expr> parseXor()
    {
        std::unique_ptr<Expr> left = parseAnd();
        while (skipSpace(), peek() == '^') {
            ++m_pos;
            left = std::make_unique<BinaryExpr>(BinaryExpr::Op::Xor, std::move(left), parseAnd());
        }
        return left;
    }

    std::unique_ptr<Expr> parseAnd()
    {
        std::unique_ptr<Expr> left = parseNot();
        while (skipSpace(), peek() == '&') {
            ++m_pos;
            left = std::make_unique<BinaryExpr>(BinaryExpr::Op::And, std::move(left), parseNot());
        }
        return left;
    }

    std::unique_ptr<Expr> parseNot()
    {
        skipSpace();
        if (peek() == '!') {
            ++m_pos;
            return std::make_unique<NotExpr>(parseNot());
        }
        return parseAtom();
    }

    std::unique_ptr<Expr> parseAtom()
    {
        skipSpace();
        const int start = column();
        if (peek() == '(') {
            ++m_pos;
            std::unique_ptr<Expr> inner = parseOr();
            skipSpace();
            if (peek() != ')')
                fail(atEnd() ? column() : column(), "expected ')' to close the '(' at column "
                                                    + std::to_string(start));
            ++m_pos;
            return inner;
        }
        std::string name = readName();
        if (name.empty()) {
            if (atEnd())
                fail(column(), "the expression ends where a name or '(' was expected");
            fail(column(), std::string("expected a name or '(' but found '") + peek() + "'");
        }
        return std::make_unique<VarExpr>(std::move(name));
    }

    std::string readName()
    {
        std::string name;
        if (!atEnd() && (std::isalpha(static_cast<unsigned char>(peek())) || peek() == '_')) {
            while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
                name += m_text[m_pos++];
        }
        return name;
    }

    void skipSpace()
    {
        while (!atEnd() && (m_text[m_pos] == ' ' || m_text[m_pos] == '\t' || m_text[m_pos] == '\r'))
            ++m_pos;
    }

    bool atEnd() const { return m_pos >= m_text.size(); }
    char peek() const { return atEnd() ? '\0' : m_text[m_pos]; }
    int column() const { return static_cast<int>(m_pos) + 1; }

    [[noreturn]] void fail(int column, const std::string& message) const
    {
        throw ExpressionSyntaxError(m_line, column, message);
    }

    const std::string& m_text;
    int m_line;
    std::size_t m_pos = 0;
};

} // namespace

std::vector<Equation> ExpressionParser::parse(const std::string& text)
{
    std::vector<Equation> equations;
    std::set<std::string> outputs;
    std::istringstream in(text);
    std::string raw;
    int line = 0;
    while (std::getline(in, raw)) {
        ++line;
        const std::string body = raw.substr(0, raw.find('#'));
        if (body.find_first_not_of(" \t\r") == std::string::npos)
            continue;
        Equation eq = LineParser(body, line).parseEquation();
        if (outputs.count(eq.output) != 0) {
            const int col = static_cast<int>(body.find(eq.output)) + 1;
            throw ExpressionSyntaxError(line, col, eq.output + " is already defined on an earlier line");
        }
        outputs.insert(eq.output);
        equations.push_back(std::move(eq));
    }
    if (equations.empty())
        throw ExpressionSyntaxError(1, 1, "type one equation per line, such as S = A ^ B");

    // A name may refer to an earlier output, never to its own line or a later
    // one: that would be a loop, or an input and an output with one name.
    std::istringstream again(text);
    std::map<int, std::string> lines;
    for (int n = 1; std::getline(again, raw); ++n)
        lines[n] = raw.substr(0, raw.find('#'));
    for (std::size_t i = 0; i < equations.size(); ++i) {
        std::vector<std::string> names;
        equations[i].root->collectVariables(names);
        for (const std::string& name : names) {
            for (std::size_t j = i; j < equations.size(); ++j) {
                if (equations[j].output != name)
                    continue;
                const std::string& body = lines[equations[i].line];
                const std::size_t eq = body.find('=');
                const std::size_t at = body.find(name, eq == std::string::npos ? 0 : eq);
                throw ExpressionSyntaxError(
                    equations[i].line, static_cast<int>(at == std::string::npos ? 0 : at) + 1,
                    j == i ? name + " cannot be used in its own equation"
                           : name + " is defined on a later line; move its equation up");
            }
        }
    }
    return equations;
}

} // namespace lb
