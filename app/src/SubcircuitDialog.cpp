#include "SubcircuitDialog.h"

#include "CircuitDocument.h"

#include "logicbench/Component.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QVBoxLayout>

namespace lbgui {

SubcircuitDialog::SubcircuitDialog(CircuitDocument* document, QWidget* parent)
    : QDialog(parent)
    , m_document(document)
    , m_name(new QLineEdit(this))
    , m_pins(new QTableWidget(this))
{
    setWindowTitle(tr("Package as block"));
    resize(460, 380);

    QString suggestion = m_document->displayName();
    suggestion = suggestion.section(QLatin1Char('.'), 0, 0);
    m_name->setText(suggestion == QLatin1String("untitled") ? QStringLiteral("block")
                                                            : suggestion);

    auto* explanation = new QLabel(
        tr("Every switch becomes an input pin and every LED an output pin. "
           "Pins are ordered by name, so labelling them fixes the order they "
           "appear in on the block."),
        this);
    explanation->setWordWrap(true);

    m_pins->setColumnCount(3);
    m_pins->setHorizontalHeaderLabels({tr("Direction"), tr("Component"), tr("Pin name")});
    m_pins->horizontalHeader()->setStretchLastSection(true);
    m_pins->verticalHeader()->setVisible(false);

    auto* form = new QFormLayout;
    form->addRow(tr("Block name"), m_name);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(explanation);
    layout->addLayout(form);
    layout->addWidget(m_pins, 1);
    layout->addWidget(buttons);

    populate();
}

void SubcircuitDialog::populate()
{
    const lb::Circuit& circuit = m_document->circuit();
    m_pins->setRowCount(0);

    const auto addRow = [this](const lb::Component& component, const QString& direction) {
        const int row = m_pins->rowCount();
        m_pins->insertRow(row);

        auto* directionCell = new QTableWidgetItem(direction);
        directionCell->setFlags(Qt::ItemIsEnabled);
        m_pins->setItem(row, 0, directionCell);

        auto* componentCell = new QTableWidgetItem(
            QStringLiteral("%1 #%2").arg(QString::fromStdString(component.typeName()))
                .arg(component.id()));
        componentCell->setFlags(Qt::ItemIsEnabled);
        componentCell->setData(Qt::UserRole, component.id());
        m_pins->setItem(row, 1, componentCell);

        m_pins->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(component.label())));
    };

    for (lb::Component* component : circuit.componentsOfType("SWITCH"))
        addRow(*component, tr("input"));
    for (lb::Component* component : circuit.componentsOfType("LED"))
        addRow(*component, tr("output"));

    m_pins->resizeColumnsToContents();
}

QString SubcircuitDialog::blockName() const
{
    return m_name->text().trimmed().isEmpty() ? QStringLiteral("block") : m_name->text().trimmed();
}

void SubcircuitDialog::applyLabels()
{
    for (int row = 0; row < m_pins->rowCount(); ++row) {
        const lb::ComponentId id = m_pins->item(row, 1)->data(Qt::UserRole).toUInt();
        const QString label = m_pins->item(row, 2)->text().trimmed();
        m_document->setComponentProperty(id, QStringLiteral("label"), label);
    }
}

} // namespace lbgui
