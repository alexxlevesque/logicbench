#pragma once

#include "logicbench/Circuit.h"

#include <QList>

#include <memory>

namespace lbgui {

/// Copy and paste through the system clipboard.
///
/// A copied selection travels as .lbc JSON under its own MIME type, so it
/// pastes into another LogicBench window or a later session, and anything else
/// on the clipboard (text, an image) is simply not a fragment.
class CircuitClipboard {
public:
    static constexpr const char* kMimeType = "application/x-logicbench-fragment";

    /// Puts these components, and the wires between them, on the clipboard.
    /// Returns false if none of the ids is in the circuit.
    static bool copy(const lb::Circuit& circuit, const QList<lb::ComponentId>& ids);

    static bool hasFragment();

    /// The fragment on the clipboard, or nullptr if there is none or it cannot
    /// be read.
    static std::unique_ptr<lb::Circuit> paste();
};

} // namespace lbgui
