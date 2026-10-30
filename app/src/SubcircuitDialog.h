#pragma once

#include "logicbench/Pin.h"

#include <QDialog>
#include <QHash>
#include <QString>

class QLineEdit;
class QTableWidget;

namespace lbgui {

class CircuitDocument;

/// The pin editor. Packaging a circuit into a block means naming the switches
/// and lamps that become its pins, so this dialog edits those labels in place
/// — through the undo stack, because it is an edit to the circuit like any
/// other — and then names the block.
class SubcircuitDialog : public QDialog {
    Q_OBJECT

public:
    explicit SubcircuitDialog(CircuitDocument* document, QWidget* parent = nullptr);

    QString blockName() const;

    /// Writes the edited labels back through the document's undo stack.
    void applyLabels();

private:
    void populate();

    CircuitDocument* m_document;
    QLineEdit* m_name;
    QTableWidget* m_pins;
};

} // namespace lbgui
