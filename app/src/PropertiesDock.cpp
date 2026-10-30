#include "PropertiesDock.h"

#include "CircuitDocument.h"

#include "logicbench/Component.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace lbgui {

PropertiesDock::PropertiesDock(CircuitDocument* document, QWidget* parent)
    : QDockWidget(tr("Properties"), parent)
    , m_document(document)
    , m_container(new QWidget(this))
    , m_form(new QFormLayout)
    , m_heading(new QLabel(tr("Nothing selected"), this))
    , m_pins(new QLabel(this))
{
    setObjectName(QStringLiteral("propertiesDock"));

    auto* layout = new QVBoxLayout(m_container);
    QFont font = m_heading->font();
    font.setBold(true);
    m_heading->setFont(font);
    m_pins->setWordWrap(true);
    m_pins->setTextFormat(Qt::PlainText);

    layout->addWidget(m_heading);
    layout->addLayout(m_form);
    layout->addSpacing(8);
    layout->addWidget(m_pins);
    layout->addStretch(1);
    setWidget(m_container);

    connect(m_document, &CircuitDocument::signalsChanged, this, &PropertiesDock::refresh);
    connect(m_document, &CircuitDocument::structureChanged, this, &PropertiesDock::refresh);
}

void PropertiesDock::clearEditors()
{
    while (m_form->count() > 0) {
        QLayoutItem* item = m_form->takeAt(0);
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }
}

void PropertiesDock::setSelection(const QList<lb::ComponentId>& ids)
{
    m_current = (ids.size() == 1) ? ids.first() : lb::kNoComponent;
    clearEditors();

    const lb::Component* component = m_document->circuit().component(m_current);
    if (!component) {
        m_heading->setText(ids.isEmpty() ? tr("Nothing selected")
                                         : tr("%n components selected", nullptr,
                                              static_cast<int>(ids.size())));
        m_pins->clear();
        return;
    }

    m_heading->setText(QStringLiteral("%1 #%2")
                           .arg(QString::fromStdString(component->typeName()))
                           .arg(component->id()));

    m_updating = true;
    const auto values = component->properties();
    for (const lb::PropertyDescriptor& descriptor : component->propertyDescriptors()) {
        const QString name = QString::fromStdString(descriptor.name);
        const auto it = values.find(descriptor.name);
        const QString value = it == values.end() ? QString() : QString::fromStdString(it->second);

        switch (descriptor.kind) {
        case lb::PropertyDescriptor::Kind::Integer: {
            auto* spin = new QSpinBox(m_container);
            spin->setRange(descriptor.minimum, descriptor.maximum);
            spin->setValue(value.toInt());
            connect(spin, &QSpinBox::valueChanged, this, [this, name](int updated) {
                if (!m_updating)
                    m_document->setComponentProperty(m_current, name, QString::number(updated));
            });
            m_form->addRow(QString::fromStdString(descriptor.label), spin);
            break;
        }
        case lb::PropertyDescriptor::Kind::Boolean: {
            auto* check = new QCheckBox(m_container);
            check->setChecked(value == QLatin1String("1"));
            connect(check, &QCheckBox::toggled, this, [this, name](bool checked) {
                if (!m_updating)
                    m_document->setComponentProperty(m_current, name,
                                                     checked ? QStringLiteral("1")
                                                             : QStringLiteral("0"));
            });
            m_form->addRow(QString::fromStdString(descriptor.label), check);
            break;
        }
        case lb::PropertyDescriptor::Kind::Text:
        default: {
            auto* edit = new QLineEdit(value, m_container);
            edit->setPlaceholderText(tr("unnamed"));
            // On editingFinished rather than on every keystroke, so one label
            // change is one undo step.
            connect(edit, &QLineEdit::editingFinished, this, [this, name, edit] {
                if (!m_updating)
                    m_document->setComponentProperty(m_current, name, edit->text());
            });
            m_form->addRow(QString::fromStdString(descriptor.label), edit);
            break;
        }
        }
    }
    m_updating = false;
    refresh();
}

void PropertiesDock::refresh()
{
    const lb::Component* component = m_document->circuit().component(m_current);
    if (!component) {
        m_pins->clear();
        return;
    }

    // A live read-out of the pins, which is the fastest way to see why a gate
    // is producing what it is producing.
    QStringList lines;
    for (std::size_t p = 0; p < component->pinCount(); ++p) {
        const lb::Pin& pin = component->pin(p);
        lines << QStringLiteral("%1 %2 = %3")
                     .arg(pin.isInput() ? QStringLiteral("in ") : QStringLiteral("out"),
                          QString::fromStdString(pin.name()),
                          QString(QChar(lb::toChar(pin.signal()))));
    }
    m_pins->setText(tr("Pins\n%1").arg(lines.join(QLatin1Char('\n'))));
}

} // namespace lbgui
