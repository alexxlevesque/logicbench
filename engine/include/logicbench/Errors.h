#pragma once

#include <stdexcept>
#include <string>

namespace lb {

/// Thrown when a connection would violate the rules of the circuit model:
/// output-to-output, input-to-input, a second driver on one input pin, or a
/// pin reference that does not resolve.
class IllegalConnection : public std::runtime_error {
public:
    explicit IllegalConnection(const std::string& what) : std::runtime_error(what) {}
};

/// Thrown by CircuitSerializer when a file is not a LogicBench circuit, is
/// truncated, or carries a version this build cannot read.
class FileFormatError : public std::runtime_error {
public:
    explicit FileFormatError(const std::string& what) : std::runtime_error(what) {}
};

/// Thrown when a component type name has no registered creator.
class UnknownComponentType : public std::runtime_error {
public:
    explicit UnknownComponentType(const std::string& type)
        : std::runtime_error("unknown component type: " + type) {}
};

} // namespace lb
