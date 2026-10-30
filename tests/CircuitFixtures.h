#pragma once

#include "logicbench/Circuit.h"
#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"
#include "logicbench/Simulator.h"

#include <memory>
#include <string>

namespace lbtest {

/// Ids of the parts of a half adder, so a test can drive it by name.
struct HalfAdder {
    lb::ComponentId a = 0;
    lb::ComponentId b = 0;
    lb::ComponentId sumLed = 0;
    lb::ComponentId carryLed = 0;
    lb::ComponentId xorGate = 0;
    lb::ComponentId andGate = 0;
};

/// Builds S = A xor B, C = A and B into an existing circuit.
inline HalfAdder buildHalfAdder(lb::Circuit& circuit, const std::string& prefix = "")
{
    HalfAdder h;

    auto switchA = std::make_unique<lb::Switch>();
    switchA->setLabel(prefix + "A");
    h.a = circuit.add(std::move(switchA));

    auto switchB = std::make_unique<lb::Switch>();
    switchB->setLabel(prefix + "B");
    h.b = circuit.add(std::move(switchB));

    h.xorGate = circuit.add(std::make_unique<lb::XorGate>());
    h.andGate = circuit.add(std::make_unique<lb::AndGate>());

    auto sum = std::make_unique<lb::Led>();
    sum->setLabel(prefix + "S");
    h.sumLed = circuit.add(std::move(sum));

    auto carry = std::make_unique<lb::Led>();
    carry->setLabel(prefix + "C");
    h.carryLed = circuit.add(std::move(carry));

    circuit.connect({h.a, 0}, {h.xorGate, 1});
    circuit.connect({h.b, 0}, {h.xorGate, 2});
    circuit.connect({h.a, 0}, {h.andGate, 1});
    circuit.connect({h.b, 0}, {h.andGate, 2});
    circuit.connect({h.xorGate, 0}, {h.sumLed, 0});
    circuit.connect({h.andGate, 0}, {h.carryLed, 0});
    return h;
}

/// Sets a switch and settles.
inline void setSwitch(lb::Circuit& circuit, lb::Simulator& sim, lb::ComponentId id, bool high)
{
    auto* sw = dynamic_cast<lb::Switch*>(circuit.component(id));
    sw->setState(lb::signalFromBool(high));
    sim.evaluateFrom(id);
}

inline lb::Signal ledValue(const lb::Circuit& circuit, lb::ComponentId id)
{
    return circuit.component(id)->pin(0).signal();
}

} // namespace lbtest
