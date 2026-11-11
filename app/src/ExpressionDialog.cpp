#include "ExpressionDialog.h"

#include "CircuitDocument.h"

#include "logicbench/Expression.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
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
    const QString text = m_editor->toPlainText();
    if (text.trimmed().isEmpty()) {
        m_status->setStyleSheet(QStringLiteral("color:#7A828C;"));
        m_status->setText(tr("Type an equation such as S = A ^ B."));
        m_buildButton->setEnabled(false);
        return;
    }

    try {
        const std::vector<lb::Equation> equations = lb::ExpressionParser::parse(text.toStdString());
        QStringList outputs;
        std::vector<std::string> names;
        for (const lb::Equation& eq : equations) {
            outputs << QString::fromStdString(eq.output);
            eq.root->collectVariables(names);
        }
        QStringList inputs;
        for (const std::string& n : names)
            if (!outputs.contains(QString::fromStdString(n)))
                inputs << QString::fromStdString(n);
        m_status->setStyleSheet(QStringLiteral("color:#3C7A50;"));
        m_status->setText(tr("✓ %1 inputs (%2) · %3 outputs (%4) · ready to build")
                              .arg(inputs.size())
                              .arg(inputs.join(QStringLiteral(", ")))
                              .arg(outputs.size())
                              .arg(outputs.join(QStringLiteral(", "))));
        m_buildButton->setEnabled(true);
    } catch (const lb::ExpressionSyntaxError& e) {
        m_status->setStyleSheet(QStringLiteral("color:#B3261E;"));
        m_status->setText(tr("Line %1, column %2: %3")
                              .arg(e.line()).arg(e.column()).arg(QString::fromStdString(e.message())));
        m_buildButton->setEnabled(false);
    }
}

bool ExpressionDialog::canBuild() const
{
    return m_buildButton->isEnabled();
}

int ExpressionDialog::errorLine() const
{
    try {
        lb::ExpressionParser::parse(m_editor->toPlainText().toStdString());
    } catch (const lb::ExpressionSyntaxError& e) {
        return e.line();
    }
    return 0;
}

void ExpressionDialog::build()
{
    QString error;
    if (!m_document->buildFromEquations(m_editor->toPlainText(), m_intoOpen->isChecked(), &error)) {
        m_status->setStyleSheet(QStringLiteral("color:#B3261E;"));
        m_status->setText(error);
        return;
    }
    accept();
}

} // namespace lbgui
