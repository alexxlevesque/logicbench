#include "ExpressionDialog.h"

#include "CircuitDocument.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace lbgui {

namespace {

QLabel* hintLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color:#7A828C;"));
    return label;
}

} // namespace

ExpressionDialog::ExpressionDialog(CircuitDocument* document, QWidget* parent)
    : QDialog(parent)
    , m_document(document)
    , m_editor(new QPlainTextEdit(this))
    , m_status(new QLabel(this))
    , m_newCircuit(new QRadioButton(tr("Open as a new circuit"), this))
    , m_intoOpen(new QRadioButton(tr("Add to the open circuit"), this))
    , m_buildButton(new QPushButton(tr("Build"), this))
{
    setWindowTitle(tr("Build from expression"));
    resize(600, 330);

    m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_editor->setPlaceholderText(tr("S = A ^ B"));
    m_editor->setMinimumHeight(90);
    m_editor->setMaximumHeight(120);
    m_status->setWordWrap(true);

    auto* destination = new QHBoxLayout;
    destination->addWidget(m_newCircuit);
    destination->addWidget(m_intoOpen);
    destination->addStretch(1);
    auto* group = new QButtonGroup(this);
    group->addButton(m_newCircuit);
    group->addButton(m_intoOpen);
    m_newCircuit->setChecked(true);

    auto* buttons = new QDialogButtonBox(this);
    buttons->addButton(QDialogButtonBox::Cancel);
    buttons->addButton(m_buildButton, QDialogButtonBox::AcceptRole);
    m_buildButton->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Equations, one output per line"), this));
    layout->addWidget(m_editor, 1);
    layout->addWidget(hintLabel(
        tr("Operators: ! not   & and   | or   ^ xor   ( ) grouping. "
           "Names start with a letter: A, Cin, sel0."),
        this));
    layout->addSpacing(6);
    layout->addWidget(m_status);
    layout->addSpacing(6);
    layout->addLayout(destination);
    layout->addWidget(hintLabel(
        tr("Switches on the left, LEDs on the right, gates placed in columns by depth."), this));
    layout->addWidget(buttons);

    connect(m_editor, &QPlainTextEdit::textChanged, this, &ExpressionDialog::reparse);
    connect(m_buildButton, &QPushButton::clicked, this, &ExpressionDialog::build);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    reparse();
}

void ExpressionDialog::setEquations(const QString& text)
{
    m_editor->setPlainText(text);
}

QString ExpressionDialog::equations() const
{
    return m_editor->toPlainText();
}

void ExpressionDialog::reparse()
{
    // Stand-in until lb::ExpressionParser exists: collect the names on each
    // side of '=' so the message reads as it will. It checks no syntax.
    static const QRegularExpression name(QStringLiteral("[A-Za-z][A-Za-z0-9_]*"));
    QStringList inputs;
    QStringList outputs;
    for (const QString& line : m_editor->toPlainText().split(QLatin1Char('\n'))) {
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals < 0)
            continue;
        const QString left = line.left(equals).trimmed();
        if (!left.isEmpty() && !outputs.contains(left))
            outputs << left;
        auto it = name.globalMatch(line.mid(equals + 1));
        while (it.hasNext()) {
            const QString found = it.next().captured();
            if (!inputs.contains(found))
                inputs << found;
        }
    }

    if (outputs.isEmpty()) {
        m_status->setStyleSheet(QStringLiteral("color:#7A828C;"));
        m_status->setText(tr("Type an equation such as S = A ^ B."));
        m_buildButton->setEnabled(false);
        return;
    }
    m_status->setStyleSheet(QStringLiteral("color:#3C7A50;"));
    m_status->setText(tr("✓ %1 inputs (%2) · %3 outputs (%4) · ready to build")
                          .arg(inputs.size())
                          .arg(inputs.join(QStringLiteral(", ")))
                          .arg(outputs.size())
                          .arg(outputs.join(QStringLiteral(", "))));
    m_buildButton->setEnabled(true);
}

void ExpressionDialog::build()
{
    m_status->setStyleSheet(QStringLiteral("color:#9A5B00;"));
    m_status->setText(tr("Building is not wired in yet (FR-7)."));
}

} // namespace lbgui
