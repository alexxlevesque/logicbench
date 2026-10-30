// Builds the example circuits that ship in examples/.
//
// A developer utility, not part of the product: it is an optional target
// (-DLOGICBENCH_BUILD_EXAMPLES=ON) so the examples can be regenerated when the
// file format changes, without hand-editing JSON.
//
//   cmake --build build --target logicbench_examples
//   ./build/examples/logicbench_examples examples/

#include "logicbench/CircuitSerializer.h"
#include "logicbench/DFlipFlop.h"
#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"
#include "logicbench/SubCircuit.h"

#include <iostream>
#include <memory>
#include <string>

using namespace lb;

namespace {

ComponentId place(Circuit& circuit, std::unique_ptr<Component> component,
                  double x, double y, const std::string& label = {})
{
    if (!label.empty())
        component->setLabel(label);
    component->setPosition(x, y);
    return circuit.add(std::move(component));
}

int pin(const Circuit& circuit, ComponentId id, const char* name)
{
    return circuit.component(id)->pinIndex(name);
}

/// S = A xor B, C = A and B.
std::unique_ptr<Circuit> makeHalfAdder()
{
    auto circuit = std::make_unique<Circuit>();
    circuit->setName("half adder");

    const ComponentId a = place(*circuit, std::make_unique<Switch>(), 40, 80, "A");
    const ComponentId b = place(*circuit, std::make_unique<Switch>(), 40, 180, "B");
    const ComponentId sumGate = place(*circuit, std::make_unique<XorGate>(), 200, 80);
    const ComponentId carryGate = place(*circuit, std::make_unique<AndGate>(), 200, 190);
    const ComponentId sumLed = place(*circuit, std::make_unique<Led>(), 360, 80, "S");
    const ComponentId carryLed = place(*circuit, std::make_unique<Led>(), 360, 190, "C");

    circuit->connect({a, 0}, {sumGate, 1});
    circuit->connect({b, 0}, {sumGate, 2});
    circuit->connect({a, 0}, {carryGate, 1});
    circuit->connect({b, 0}, {carryGate, 2});
    circuit->connect({sumGate, 0}, {sumLed, 0});
    circuit->connect({carryGate, 0}, {carryLed, 0});
    return circuit;
}

/// Two half adders and an OR: A + B + Cin -> S, Cout.
std::unique_ptr<Circuit> makeFullAdder()
{
    auto circuit = std::make_unique<Circuit>();
    circuit->setName("full adder");

    const ComponentId a = place(*circuit, std::make_unique<Switch>(), 40, 60, "A");
    const ComponentId b = place(*circuit, std::make_unique<Switch>(), 40, 150, "B");
    const ComponentId cin = place(*circuit, std::make_unique<Switch>(), 40, 260, "Cin");

    const ComponentId first = place(*circuit,
        std::make_unique<SubCircuit>("half adder", makeHalfAdder()), 200, 70);
    const ComponentId second = place(*circuit,
        std::make_unique<SubCircuit>("half adder", makeHalfAdder()), 380, 170);
    const ComponentId carryOr = place(*circuit, std::make_unique<OrGate>(), 560, 300);

    const ComponentId sumLed = place(*circuit, std::make_unique<Led>(), 720, 180, "S");
    const ComponentId coutLed = place(*circuit, std::make_unique<Led>(), 720, 300, "Cout");

    circuit->connect({a, 0}, {first, pin(*circuit, first, "A")});
    circuit->connect({b, 0}, {first, pin(*circuit, first, "B")});
    circuit->connect({first, pin(*circuit, first, "S")}, {second, pin(*circuit, second, "A")});
    circuit->connect({cin, 0}, {second, pin(*circuit, second, "B")});
    circuit->connect({second, pin(*circuit, second, "S")}, {sumLed, 0});
    circuit->connect({first, pin(*circuit, first, "C")}, {carryOr, 1});
    circuit->connect({second, pin(*circuit, second, "C")}, {carryOr, 2});
    circuit->connect({carryOr, 0}, {coutLed, 0});
    return circuit;
}

/// Four full adders in a ripple chain: A3..A0 + B3..B0 -> S3..S0, Cout.
std::unique_ptr<Circuit> makeAdder4()
{
    auto circuit = std::make_unique<Circuit>();
    circuit->setName("4-bit ripple-carry adder");

    std::unique_ptr<Circuit> prototype = makeFullAdder();
    ComponentId carry = kNoComponent;
    int carryPin = 0;

    const ComponentId cin = place(*circuit, std::make_unique<Switch>(), 40, 640, "Cin");

    for (int bit = 0; bit < 4; ++bit) {
        const double y = 40 + (3 - bit) * 150.0;
        const std::string index = std::to_string(bit);

        const ComponentId a = place(*circuit, std::make_unique<Switch>(), 40, y, "A" + index);
        const ComponentId b = place(*circuit, std::make_unique<Switch>(), 40, y + 60,
                                    "B" + index);
        const ComponentId stage = place(*circuit,
            std::make_unique<SubCircuit>("full adder", std::make_unique<Circuit>(*prototype)),
            220, y);
        const ComponentId sum = place(*circuit, std::make_unique<Led>(), 460, y, "S" + index);

        circuit->connect({a, 0}, {stage, pin(*circuit, stage, "A")});
        circuit->connect({b, 0}, {stage, pin(*circuit, stage, "B")});
        circuit->connect({stage, pin(*circuit, stage, "S")}, {sum, 0});

        if (bit == 0)
            circuit->connect({cin, 0}, {stage, pin(*circuit, stage, "Cin")});
        else
            circuit->connect({carry, carryPin}, {stage, pin(*circuit, stage, "Cin")});

        carry = stage;
        carryPin = pin(*circuit, stage, "Cout");
    }

    const ComponentId cout = place(*circuit, std::make_unique<Led>(), 620, 40, "Cout");
    circuit->connect({carry, carryPin}, {cout, 0});
    return circuit;
}

/// Three toggle flip-flops in a ripple chain, driven by a clock.
std::unique_ptr<Circuit> makeCounter()
{
    auto circuit = std::make_unique<Circuit>();
    circuit->setName("3-bit ripple counter");

    auto clockComponent = std::make_unique<Clock>();
    clockComponent->setPeriod(8);
    const ComponentId clock = place(*circuit, std::move(clockComponent), 40, 160, "CLK");

    ComponentId previous = clock;
    int previousPin = 0;
    for (int bit = 0; bit < 3; ++bit) {
        const double x = 200 + bit * 200.0;
        const ComponentId stage = place(*circuit, std::make_unique<DFlipFlop>(), x, 120,
                                        "Q" + std::to_string(bit));
        const ComponentId led = place(*circuit, std::make_unique<Led>(), x + 110, 60,
                                      "bit " + std::to_string(bit));

        // Qbar back into D makes the flip-flop toggle on every rising edge,
        // so each stage halves the frequency of the one before it.
        circuit->connect({stage, pin(*circuit, stage, "QBAR")},
                         {stage, pin(*circuit, stage, "D")});
        circuit->connect({previous, previousPin}, {stage, pin(*circuit, stage, "CLK")});
        circuit->connect({stage, pin(*circuit, stage, "Q")}, {led, 0});

        previous = stage;
        previousPin = pin(*circuit, stage, "QBAR");
    }
    return circuit;
}

/// Cross-coupled NOR gates. Set and Reset drive it; with both low it holds.
std::unique_ptr<Circuit> makeSrLatch()
{
    auto circuit = std::make_unique<Circuit>();
    circuit->setName("SR latch");

    const ComponentId reset = place(*circuit, std::make_unique<Switch>(), 40, 80, "R");
    const ComponentId set = place(*circuit, std::make_unique<Switch>(), 40, 260, "S");
    const ComponentId top = place(*circuit, std::make_unique<NorGate>(), 220, 90);
    const ComponentId bottom = place(*circuit, std::make_unique<NorGate>(), 220, 250);
    const ComponentId q = place(*circuit, std::make_unique<Led>(), 400, 90, "Q");
    const ComponentId qbar = place(*circuit, std::make_unique<Led>(), 400, 250, "Qbar");

    circuit->connect({reset, 0}, {top, 1});
    circuit->connect({bottom, 0}, {top, 2});
    circuit->connect({set, 0}, {bottom, 1});
    circuit->connect({top, 0}, {bottom, 2});
    circuit->connect({top, 0}, {q, 0});
    circuit->connect({bottom, 0}, {qbar, 0});
    return circuit;
}

/// A ring of three inverters: the loop LogicBench refuses to hang on.
std::unique_ptr<Circuit> makeRingOscillator()
{
    auto circuit = std::make_unique<Circuit>();
    circuit->setName("ring oscillator (cannot settle)");

    const ComponentId first = place(*circuit, std::make_unique<NotGate>(), 120, 120);
    const ComponentId second = place(*circuit, std::make_unique<NotGate>(), 280, 120);
    const ComponentId third = place(*circuit, std::make_unique<NotGate>(), 440, 120);
    const ComponentId led = place(*circuit, std::make_unique<Led>(), 600, 120, "out");

    circuit->connect({first, 0}, {second, 1});
    circuit->connect({second, 0}, {third, 1});
    circuit->connect({third, 0}, {first, 1});
    circuit->connect({third, 0}, {led, 0});
    return circuit;
}

void write(const std::string& directory, const std::string& file, const Circuit& circuit)
{
    const std::string path = directory + "/" + file;
    CircuitSerializer::save(circuit, path);
    std::cout << "wrote " << path << " (" << circuit.componentCount() << " components, "
              << circuit.wires().size() << " wires)\n";
}

} // namespace

int main(int argc, char* argv[])
{
    const std::string directory = argc > 1 ? argv[1] : ".";
    try {
        write(directory, "half_adder.lbc", *makeHalfAdder());
        write(directory, "full_adder.lbc", *makeFullAdder());
        write(directory, "adder4.lbc", *makeAdder4());
        write(directory, "counter3.lbc", *makeCounter());
        write(directory, "sr_latch.lbc", *makeSrLatch());
        write(directory, "ring_oscillator.lbc", *makeRingOscillator());
    } catch (const std::exception& e) {
        std::cerr << "failed: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
