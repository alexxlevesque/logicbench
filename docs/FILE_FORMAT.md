# The `.lbc` circuit file

JSON, UTF-8, with a version number. Written by `CircuitSerializer::toJson` and
read by `fromJson`.

A file is self-contained: a block carries its contents inline, so opening
someone else's circuit never asks for a library of blocks.

## Version 2 (current)

```json
{
  "format": "logicbench-circuit",
  "version": 2,
  "name": "half adder",
  "components": [
    {
      "id": 1,
      "type": "SWITCH",
      "label": "A",
      "delay": 0,
      "x": 40.0,
      "y": 80.0,
      "properties": { "label": "A", "state": "0" }
    },
    {
      "id": 3,
      "type": "XOR",
      "label": "",
      "delay": 1,
      "x": 200.0,
      "y": 80.0,
      "properties": { "label": "", "inputs": "2", "delay": "1" }
    }
  ],
  "wires": [
    { "id": 1, "from": { "component": 1, "pin": 0 },
               "to":   { "component": 3, "pin": 1 } }
  ]
}
```

### Fields

| Field | Meaning |
|---|---|
| `format` | always `logicbench-circuit`; anything else is rejected |
| `version` | integer; this build writes 2 and reads 1 and 2 |
| `name` | the circuit's name, used as a block's default name |
| `components[].id` | unique within the file; wires refer to it |
| `components[].type` | a name registered with `ComponentFactory` |
| `components[].x`, `y` | canvas position |
| `components[].properties` | string map, applied through `Component::setProperty` |
| `wires[].from` | an **output** pin |
| `wires[].to` | an **input** pin, driven by at most one wire |

Pin indices follow the component's own pin list. For a gate, pin 0 is the
output `Y` and inputs are 1..n — output first, so a wire out of a gate survives
a change to its input count.

### Blocks

A `SUBCIRCUIT` component carries two extra fields:

```json
{
  "id": 4,
  "type": "SUBCIRCUIT",
  "block": "half adder",
  "inner": { "format": "logicbench-circuit", "version": 2, "...": "a whole circuit" }
}
```

`inner` is a complete circuit document, nested to any depth. The block's pins
are derived from the inner circuit's switches and LEDs when it is loaded, so
they are not stored separately and cannot disagree with the contents.

## Version 1 (still readable)

Version 1 flattened the wire endpoints and had no `properties` object:

```json
{
  "format": "logicbench-circuit",
  "version": 1,
  "name": "legacy inverter",
  "components": [ { "id": 1, "type": "SWITCH", "label": "A", "x": 0, "y": 0, "delay": 0 } ],
  "wires": [ { "id": 1, "fromComponent": 1, "fromPin": 0, "toComponent": 2, "toPin": 1 } ]
}
```

The loader accepts either wire shape and either property style, so a version 1
file opens and is saved back out as version 2. A regression test keeps it that
way.

## Errors

Everything that can go wrong is an exception, never a silently dropped element:

| Condition | Thrown |
|---|---|
| not JSON, wrong `format`, no component list | `FileFormatError` |
| a version this build cannot read | `FileFormatError`, naming the version |
| duplicate component id | `FileFormatError` |
| a wire that the circuit rules reject | `FileFormatError`, quoting the reason |
| a `type` this build does not know | `UnknownComponentType`, naming the type |
