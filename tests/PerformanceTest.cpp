// The two non-functional requirements that have a number attached, checked the
// only way a number can be checked. Thresholds are the requirement itself; the
// measured value is printed so a regression is visible before it fails.

#include <gtest/gtest.h>

#include "CircuitFixtures.h"

#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"
#include "logicbench/Simulator.h"
#include "logicbench/CircuitSerializer.h"
#include "logicbench/TruthTable.h"

#include <chrono>
#include <memory>
#include <vector>

using namespace lb;
using SteadyClock = std::chrono::steady_clock;   // lb::Clock is the component

namespace {

/// A wide tree of XOR gates fed by switches, padded to `target` components.
/// Every gate sits on a path from the toggled switch, so a toggle at the root
/// is close to the worst case rather than a lucky local update.
struct BigCircuit {
    Circuit circuit;
    ComponentId root = kNoComponent;
    ComponentId led = kNoComponent;
};

BigCircuit buildChain(std::size_t target)
{
    BigCircuit big;
    big.root = big.circuit.add(std::make_unique<Switch>());
    ComponentId previous = big.root;

    // A chain of inverter pairs: every component is downstream of the switch,
    // so one toggle propagates through all of them.
    while (big.circuit.componentCount() + 1 < target) {
        const ComponentId gate = big.circuit.add(std::make_unique<NotGate>());
        big.circuit.connect({previous, 0}, {gate, 1});
        previous = gate;
    }
    big.led = big.circuit.add(std::make_unique<Led>());
    big.circuit.connect({previous, 0}, {big.led, 0});
    return big;
}

} // namespace

TEST(PerformanceTest, ToggleIn1000ComponentCircuitIsUnder50ms)
{
    BigCircuit big = buildChain(1000);
    ASSERT_GE(big.circuit.componentCount(), 1000u);

    Simulator sim(big.circuit);
    sim.reset();
    auto* root = dynamic_cast<Switch*>(big.circuit.component(big.root));

    double worstMs = 0.0;
    for (int i = 0; i < 20; ++i) {
        root->toggle();
        const auto start = SteadyClock::now();
        const SettleReport report = sim.evaluateFrom(big.root);
        const double ms = std::chrono::duration<double, std::milli>(SteadyClock::now() - start).count();
        worstMs = std::max(worstMs, ms);
        ASSERT_TRUE(report.stable());
    }

    std::cout << "[ PERF     ] worst toggle over 20 runs, "
              << big.circuit.componentCount() << " components: " << worstMs << " ms\n";
    EXPECT_LT(worstMs, 50.0);
}

TEST(PerformanceTest, TenInputTableIsUnderOneSecond)
{
    // 1,024 rows, each a full reset and settle of the circuit.
    Circuit c;
    const ComponentId gate = c.add(std::make_unique<XorGate>());
    c.setInputCount(gate, 8);

    std::vector<ComponentId> switches;
    for (int i = 0; i < 10; ++i) {
        const ComponentId sw = c.add(std::make_unique<Switch>());
        switches.push_back(sw);
        if (i < 8)
            c.connect({sw, 0}, {gate, i + 1});
    }
    const ComponentId led = c.add(std::make_unique<Led>());
    c.connect({gate, 0}, {led, 0});

    const auto start = SteadyClock::now();
    const TruthTableResult result = TruthTableGenerator::generate(c, switches, {led});
    const double ms = std::chrono::duration<double, std::milli>(SteadyClock::now() - start).count();

    ASSERT_EQ(result.status, TruthTableStatus::Ok);
    ASSERT_EQ(result.table.rowCount(), 1024u);
    std::cout << "[ PERF     ] 1,024-row truth table: " << ms << " ms\n";
    EXPECT_LT(ms, 1000.0);
}

TEST(PerformanceTest, LargeCircuitLoadsAndSavesQuickly)
{
    BigCircuit big = buildChain(1000);
    const auto start = SteadyClock::now();
    const std::string text = CircuitSerializer::toJson(big.circuit);
    std::unique_ptr<Circuit> loaded = CircuitSerializer::fromJson(text);
    const double ms = std::chrono::duration<double, std::milli>(SteadyClock::now() - start).count();

    ASSERT_EQ(loaded->componentCount(), big.circuit.componentCount());
    std::cout << "[ PERF     ] save + load of " << big.circuit.componentCount()
              << " components: " << ms << " ms\n";
    EXPECT_LT(ms, 2000.0);
}
