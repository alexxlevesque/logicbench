#pragma once

#include "logicbench/Component.h"

#include <memory>

namespace lb {

/// Supplies clone() for a concrete component by copy-constructing the derived
/// type. Every leaf class inherits through this so the virtual copy
/// constructor is written once instead of once per gate.
///
/// The clone keeps the original's id; whoever inserts it into a circuit is
/// responsible for assigning a fresh one (Circuit::add does).
template <class Derived, class Base = Component>
class Cloneable : public Base {
public:
    using Base::Base;

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<Derived>(static_cast<const Derived&>(*this));
    }
};

} // namespace lb
