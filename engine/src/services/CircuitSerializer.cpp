#include "logicbench/CircuitSerializer.h"

#include "logicbench/SubCircuit.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace lb {

namespace {

using json = nlohmann::json;

constexpr const char* kFormatTag = "logicbench-circuit";

json componentToJson(const Component& c)
{
    json node;
    node["id"] = c.id();
    node["type"] = c.typeName();
    node["label"] = c.label();
    node["delay"] = c.delay();
    node["x"] = c.x();
    node["y"] = c.y();

    json props = json::object();
    for (const auto& entry : c.properties())
        props[entry.first] = entry.second;
    node["properties"] = props;

    // A block carries its contents with it, so a .lbc file is self-contained:
    // opening someone else's circuit never asks for a library of blocks.
    if (const auto* sub = dynamic_cast<const SubCircuit*>(&c)) {
        node["block"] = sub->blockName();
        node["inner"] = json::parse(CircuitSerializer::toJson(sub->inner(), -1));
    }
    return node;
}

void applyProperties(Component& c, const json& node)
{
    // Order matters: "inputs" resizes the pin list, so it has to be applied
    // before any wire is attached, which the caller guarantees by creating
    // every component before reading the wire list.
    if (node.contains("properties") && node["properties"].is_object()) {
        for (auto it = node["properties"].begin(); it != node["properties"].end(); ++it) {
            if (it.value().is_string())
                c.setProperty(it.key(), it.value().get<std::string>());
        }
    }
    // Version 1 kept label and delay as top-level fields only.
    if (node.contains("label") && node["label"].is_string())
        c.setLabel(node["label"].get<std::string>());
    if (node.contains("delay") && node["delay"].is_number_integer())
        c.setDelay(node["delay"].get<int>());
}

PinRef pinRefFromJson(const json& node, const char* objectKey,
                      const char* componentKey, const char* pinKey)
{
    PinRef ref;
    if (node.contains(objectKey) && node[objectKey].is_object()) {
        ref.component = node[objectKey].value("component", 0u);
        ref.pin = node[objectKey].value("pin", -1);
    } else {
        // Version 1 flattened the endpoints into four fields.
        ref.component = node.value(componentKey, 0u);
        ref.pin = node.value(pinKey, -1);
    }
    return ref;
}

} // namespace

std::string CircuitSerializer::toJson(const Circuit& circuit, int indent)
{
    json doc;
    doc["format"] = kFormatTag;
    doc["version"] = kFormatVersion;
    doc["name"] = circuit.name();

    json components = json::array();
    for (const std::unique_ptr<Component>& c : circuit.components())
        components.push_back(componentToJson(*c));
    doc["components"] = components;

    json wires = json::array();
    for (const Wire& w : circuit.wires()) {
        json node;
        node["id"] = w.id;
        node["from"] = {{"component", w.from.component}, {"pin", w.from.pin}};
        node["to"] = {{"component", w.to.component}, {"pin", w.to.pin}};
        wires.push_back(node);
    }
    doc["wires"] = wires;

    return doc.dump(indent);
}

std::unique_ptr<Circuit> CircuitSerializer::fromJson(const std::string& text,
                                                     const ComponentFactory& factory)
{
    json doc;
    try {
        doc = json::parse(text);
    } catch (const json::parse_error& e) {
        throw FileFormatError(std::string("not valid JSON: ") + e.what());
    }

    if (!doc.is_object() || doc.value("format", std::string()) != kFormatTag)
        throw FileFormatError("this file is not a LogicBench circuit");

    const int version = doc.value("version", 0);
    if (version < kMinimumReadableVersion) {
        throw FileFormatError("circuit file version " + std::to_string(version)
                              + " is older than this build can read");
    }
    if (version > kFormatVersion) {
        throw FileFormatError("circuit file version " + std::to_string(version)
                              + " was written by a newer LogicBench; this build reads up to "
                              + std::to_string(kFormatVersion));
    }
    if (!doc.contains("components") || !doc["components"].is_array())
        throw FileFormatError("circuit file has no component list");

    auto circuit = std::make_unique<Circuit>();
    circuit->setName(doc.value("name", std::string("untitled")));

    for (const json& node : doc["components"]) {
        if (!node.contains("type") || !node["type"].is_string())
            throw FileFormatError("a component entry has no type");

        const std::string type = node["type"].get<std::string>();
        std::unique_ptr<Component> component = factory.create(type);

        if (type == "SUBCIRCUIT") {
            auto* sub = static_cast<SubCircuit*>(component.get());
            sub->setBlockName(node.value("block", std::string("block")));
            if (node.contains("inner"))
                sub->setInner(fromJson(node["inner"].dump(), factory));
        }

        applyProperties(*component, node);
        component->setPosition(node.value("x", 0.0), node.value("y", 0.0));

        const ComponentId requested = node.value("id", 0u);
        const ComponentId assigned = circuit->add(std::move(component), requested);
        if (requested != 0 && assigned != requested) {
            throw FileFormatError("duplicate component id " + std::to_string(requested)
                                  + " in circuit file");
        }
    }

    if (doc.contains("wires") && doc["wires"].is_array()) {
        for (const json& node : doc["wires"]) {
            const PinRef from = pinRefFromJson(node, "from", "fromComponent", "fromPin");
            const PinRef to = pinRefFromJson(node, "to", "toComponent", "toPin");
            try {
                circuit->connect(from, to, node.value("id", 0u));
            } catch (const IllegalConnection& e) {
                throw FileFormatError(std::string("circuit file contains an illegal wire: ")
                                      + e.what());
            }
        }
    }

    return circuit;
}

void CircuitSerializer::save(const Circuit& circuit, const std::string& path)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw FileFormatError("cannot write " + path);
    out << toJson(circuit);
    if (!out)
        throw FileFormatError("failed while writing " + path);
}

std::unique_ptr<Circuit> CircuitSerializer::load(const std::string& path,
                                                 const ComponentFactory& factory)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw FileFormatError("cannot open " + path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return fromJson(buffer.str(), factory);
}

} // namespace lb
