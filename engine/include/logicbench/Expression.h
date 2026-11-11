#pragma once

#include "logicbench/Pin.h"
#include "logicbench/Signal.h"

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace lb {

class CircuitBuilder;

/// A Boolean expression tree, as ExpressionParser reads it.
///
/// Each node knows how to evaluate itself and how to emit itself into a
/// CircuitBuilder (double dispatch: the node asks the builder for the part it
/// needs, so the builder has no switch over node types). The method is
/// emitInto rather than emit because Qt defines emit as a macro.
class Expr {
public:
    virtual ~Expr() = default;

    /// Kleene evaluation; a variable missing from `values` reads X.
    virtual Signal evaluate(const std::map<std::string, Signal>& values) const = 0;

    /// Builds the gates for this node and returns the output pin carrying it.
    virtual PinRef emitInto(CircuitBuilder& builder) const = 0;

    /// Fully parenthesised text that ExpressionParser reads back to the same
    /// tree, e.g. "(A & !B)".
    virtual std::string toString() const = 0;

    /// Adds every variable name, without repeats, in first-use order.
    virtual void collectVariables(std::vector<std::string>& names) const = 0;
};

class VarExpr final : public Expr {
public:
    explicit VarExpr(std::string name) : m_name(std::move(name)) {}
    const std::string& name() const noexcept { return m_name; }

    Signal evaluate(const std::map<std::string, Signal>& values) const override;
    PinRef emitInto(CircuitBuilder& builder) const override;
    std::string toString() const override { return m_name; }
    void collectVariables(std::vector<std::string>& names) const override;

private:
    std::string m_name;
};

class NotExpr final : public Expr {
public:
    explicit NotExpr(std::unique_ptr<Expr> operand) : m_operand(std::move(operand)) {}
    const Expr& operand() const noexcept { return *m_operand; }

    Signal evaluate(const std::map<std::string, Signal>& values) const override;
    PinRef emitInto(CircuitBuilder& builder) const override;
    std::string toString() const override { return "!" + m_operand->toString(); }
    void collectVariables(std::vector<std::string>& names) const override;

private:
    std::unique_ptr<Expr> m_operand;
};

class BinaryExpr final : public Expr {
public:
    enum class Op { And, Or, Xor };

    BinaryExpr(Op op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right)
        : m_op(op), m_left(std::move(left)), m_right(std::move(right)) {}

    Op op() const noexcept { return m_op; }
    const Expr& left() const noexcept { return *m_left; }
    const Expr& right() const noexcept { return *m_right; }

    Signal evaluate(const std::map<std::string, Signal>& values) const override;
    PinRef emitInto(CircuitBuilder& builder) const override;
    std::string toString() const override;
    void collectVariables(std::vector<std::string>& names) const override;

    static char symbol(Op op);

private:
    Op m_op;
    std::unique_ptr<Expr> m_left;
    std::unique_ptr<Expr> m_right;
};

/// One line of input: `output = expression`.
struct Equation {
    std::string output;
    std::unique_ptr<Expr> root;
    int line = 0;
};

/// Thrown with the 1-based line and column of the first thing that cannot be
/// read, so the dialog can point at it.
class ExpressionSyntaxError : public std::runtime_error {
public:
    ExpressionSyntaxError(int line, int column, const std::string& message)
        : std::runtime_error("line " + std::to_string(line) + ", column "
                             + std::to_string(column) + ": " + message)
        , m_line(line), m_column(column), m_message(message) {}

    int line() const noexcept { return m_line; }
    int column() const noexcept { return m_column; }
    const std::string& message() const noexcept { return m_message; }

private:
    int m_line;
    int m_column;
    std::string m_message;
};

/// Reads one equation per line:
///
///     S    = A ^ B ^ Cin
///     Cout = (A & B) | (Cin & (A ^ B))
///
/// Operators, tightest first: ! (not), & (and), ^ (xor), | (or); parentheses
/// group. A name is a letter or underscore followed by letters, digits or
/// underscores. Blank lines and anything after # are ignored. A name that is
/// the output of an earlier line refers to that output, so equations can build
/// on each other.
class ExpressionParser {
public:
    /// Throws ExpressionSyntaxError. An empty text is an error too: there is
    /// nothing to build.
    static std::vector<Equation> parse(const std::string& text);
};

} // namespace lb
