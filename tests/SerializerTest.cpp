#include <gtest/gtest.h>


#include "CircuitFixtures.h"

#include "logicbench/CircuitSerializer.h"
#include "logicbench/Errors.h"
#include "logicbench/Gates.h"
#include "logicbench/IoComponents.h"
#include "logicbench/SubCircuit.h"

#include <cstdio>
#include <fstream>
#include <string>

using namespace lb;

namespace {

std::string tempPath(const char* name)
{
    return std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/" + name;
}

} // namespace

TEST(SerializerTest, RoundTripPreservesComponentsWiresAndGeometry)
{
    Circuit original;
    original.setName("half adder");
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(original);
    original.component(h.a)->setPosition(120.0, 40.5);
    original.component(h.xorGate)->setDelay(3);

    const std::string text = CircuitSerializer::toJson(original);
    std::unique_ptr<Circuit> loaded = CircuitSerializer::fromJson(text);

    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->name(), "half adder");
    EXPECT_EQ(loaded->componentCount(), original.componentCount());
    EXPECT_EQ(loaded->wires().size(), original.wires().size());
    EXPECT_EQ(loaded->component(h.a)->label(), "A");
    EXPECT_DOUBLE_EQ(loaded->component(h.a)->x(), 120.0);
    EXPECT_DOUBLE_EQ(loaded->component(h.a)->y(), 40.5);
    EXPECT_EQ(loaded->component(h.xorGate)->delay(), 3);
}

TEST(SerializerTest, ALoadedCircuitSimulatesIdentically)
{
    Circuit original;
    const lbtest::HalfAdder h = lbtest::buildHalfAdder(original);

    std::unique_ptr<Circuit> loaded =
        CircuitSerializer::fromJson(CircuitSerializer::toJson(original));
    Simulator sim(*loaded);
    sim.reset();
    lbtest::setSwitch(*loaded, sim, h.a, true);
    lbtest::setSwitch(*loaded, sim, h.b, true);

    EXPECT_EQ(lbtest::ledValue(*loaded, h.sumLed), Signal::Low);
    EXPECT_EQ(lbtest::ledValue(*loaded, h.carryLed), Signal::High);
}

TEST(SerializerTest, GateInputCountSurvivesTheRoundTrip)
{
    Circuit c;
    const ComponentId gate = c.add(std::make_unique<AndGate>());
    c.setInputCount(gate, 5);
    const ComponentId sw = c.add(std::make_unique<Switch>());
    c.connect({sw, 0}, {gate, 5});      // a pin that only exists at width 5

    std::unique_ptr<Circuit> loaded = CircuitSerializer::fromJson(CircuitSerializer::toJson(c));
    EXPECT_EQ(loaded->component(gate)->pinCount(), 6u);
    EXPECT_EQ(loaded->wires().size(), 1u);
}

TEST(SerializerTest, SubcircuitsAreStoredInlineSoFilesAreSelfContained)
{
    auto inner = std::make_unique<Circuit>();
    inner->setName("half adder");
    lbtest::buildHalfAdder(*inner);

    Circuit outer;
    const ComponentId block = outer.add(
        std::make_unique<SubCircuit>("half adder", std::move(inner)));

    std::unique_ptr<Circuit> loaded =
        CircuitSerializer::fromJson(CircuitSerializer::toJson(outer));
    auto* loadedBlock = dynamic_cast<SubCircuit*>(loaded->component(block));
    ASSERT_NE(loadedBlock, nullptr);
    EXPECT_EQ(loadedBlock->blockName(), "half adder");
    EXPECT_EQ(loadedBlock->inner().componentCount(), 6u);
    EXPECT_EQ(loadedBlock->pinCount(), 4u);
    EXPECT_GE(loadedBlock->pinIndex("S"), 0);
}

TEST(SerializerTest, SaveAndLoadThroughTheFilesystem)
{
    Circuit original;
    lbtest::buildHalfAdder(original);
    const std::string path = tempPath("logicbench_roundtrip.lbc");

    CircuitSerializer::save(original, path);
    std::unique_ptr<Circuit> loaded = CircuitSerializer::load(path);
    EXPECT_EQ(loaded->componentCount(), original.componentCount());
    std::remove(path.c_str());
}

TEST(SerializerTest, Version1FilesStillOpen)
{
    // Version 1 flattened the wire endpoints and had no properties object.
    // Keeping it readable is the reason the format carries a version at all.
    const std::string v1 = R"({
      "format": "logicbench-circuit",
      "version": 1,
      "name": "legacy inverter",
      "components": [
        {"id": 1, "type": "SWITCH", "label": "A", "x": 0,   "y": 0,  "delay": 0},
        {"id": 2, "type": "NOT",    "label": "",  "x": 100, "y": 0,  "delay": 1},
        {"id": 3, "type": "LED",    "label": "Y", "x": 200, "y": 0,  "delay": 0}
      ],
      "wires": [
        {"id": 1, "fromComponent": 1, "fromPin": 0, "toComponent": 2, "toPin": 1},
        {"id": 2, "fromComponent": 2, "fromPin": 0, "toComponent": 3, "toPin": 0}
      ]
    })";

    std::unique_ptr<Circuit> loaded = CircuitSerializer::fromJson(v1);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->name(), "legacy inverter");
    EXPECT_EQ(loaded->componentCount(), 3u);
    EXPECT_EQ(loaded->wires().size(), 2u);

    Simulator sim(*loaded);
    sim.reset();
    lbtest::setSwitch(*loaded, sim, 1, true);
    EXPECT_EQ(lbtest::ledValue(*loaded, 3), Signal::Low);
}

TEST(SerializerTest, MalformedFilesThrowFileFormatError)
{
    EXPECT_THROW(CircuitSerializer::fromJson("this is not json"), FileFormatError);
    EXPECT_THROW(CircuitSerializer::fromJson(R"({"format":"something-else"})"), FileFormatError);
    EXPECT_THROW(CircuitSerializer::fromJson(
                     R"({"format":"logicbench-circuit","version":2})"),
                 FileFormatError);
    EXPECT_THROW(CircuitSerializer::fromJson(
                     R"({"format":"logicbench-circuit","version":99,"components":[]})"),
                 FileFormatError);
    EXPECT_THROW(CircuitSerializer::load("/nonexistent/path/to/circuit.lbc"), FileFormatError);
}

TEST(SerializerTest, UnknownComponentTypeIsReportedByName)
{
    const std::string text = R"({
      "format": "logicbench-circuit", "version": 2, "name": "future",
      "components": [{"id": 1, "type": "QUANTUM_GATE", "x": 0, "y": 0}],
      "wires": []
    })";
    try {
        CircuitSerializer::fromJson(text);
        FAIL() << "expected UnknownComponentType";
    } catch (const UnknownComponentType& e) {
        EXPECT_NE(std::string(e.what()).find("QUANTUM_GATE"), std::string::npos);
    }
}

TEST(SerializerTest, AnIllegalWireInAFileIsReportedNotSilentlyDropped)
{
    const std::string text = R"({
      "format": "logicbench-circuit", "version": 2, "name": "bad",
      "components": [
        {"id": 1, "type": "SWITCH", "x": 0, "y": 0},
        {"id": 2, "type": "SWITCH", "x": 0, "y": 0}
      ],
      "wires": [{"id": 1, "from": {"component": 1, "pin": 0},
                          "to":   {"component": 2, "pin": 0}}]
    })";
    EXPECT_THROW(CircuitSerializer::fromJson(text), FileFormatError);
}

TEST(SerializerTest, DuplicateIdsAreRejected)
{
    const std::string text = R"({
      "format": "logicbench-circuit", "version": 2, "name": "dupes",
      "components": [
        {"id": 7, "type": "SWITCH", "x": 0, "y": 0},
        {"id": 7, "type": "LED", "x": 0, "y": 0}
      ],
      "wires": []
    })";
    EXPECT_THROW(CircuitSerializer::fromJson(text), FileFormatError);
}

TEST(SerializerTest, WrittenFilesCarryTheCurrentVersion)
{
    Circuit c;
    const std::string text = CircuitSerializer::toJson(c);
    EXPECT_NE(text.find("\"version\": 2"), std::string::npos);
    EXPECT_NE(text.find("logicbench-circuit"), std::string::npos);
}
