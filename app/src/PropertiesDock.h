#pragma once

#include "logicbench/Pin.h"

#include <QDockWidget>
#include <QList>

class QFormLayout;
class QLabel;
class QWidget;

namespace lbgui {

class CircuitDocument;

/// Editors for the selected component's properties.
///
/// The dock knows nothing about gates or flip-flops: it asks the component for
/// its property descriptors and builds a widget per entry. A new component
/// type with a new setting appears here with no change to this file.
class PropertiesDock : public QDockWidget {
    Q_OBJECT

public:
    explicit PropertiesDock(CircuitDocument* document, QWidget* parent = nullptr);

public slots:
    void setSelection(const QList<lb::ComponentId>& ids);
    void refresh();

private:
    void clearEditors();

    CircuitDocument* m_document;
    QWidget* m_container;
    QFormLayout* m_form;
    QLabel* m_heading;
    QLabel* m_pins;
    lb::ComponentId m_current = lb::kNoComponent;
    bool m_updating = false;
};

} // namespace lbgui
