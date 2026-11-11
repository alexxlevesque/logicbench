#include "SimplifyDialog.h"

#include "CircuitDocument.h"

#include "logicbench/KarnaughMap.h"
#include "logicbench/Simplifier.h"
#include "logicbench/TruthTable.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace lbgui {

namespace {



QString term(const QString& text, const QColor& colour)
{
    return QStringLiteral("<span style=\"color:%1\">%2</span>").arg(colour.name(), text);
}

QString overbar(const QString& name)
{
    return QStringLiteral("<span style=\"text-decoration:overline\">%1</span>").arg(name);
}

/// One product term as rich text, complemented variables with an overbar.
QString termHtml(const lb::SopResult& r, const lb::Implicant& t)
{
    QStringList parts;
    for (int v = 0; v < r.variables; ++v) {
        const std::uint32_t bit = 1u << (r.variables - 1 - v);
        if (t.mask & bit)
            continue;
        const QString name = QString::fromStdString(r.names[static_cast<std::size_t>(v)]).toHtmlEscaped();
        parts << ((t.value & bit) ? name : overbar(name));
    }
    return parts.isEmpty() ? QStringLiteral("1") : parts.join(QLatin1Char(' '));
}

QLabel* hintLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color:#7A828C;"));
    return label;
}

} // namespace

// ---------------------------------------------------------- SimplifyDialog --

SimplifyDialog::SimplifyDialog(CircuitDocument* document, QWidget* parent)
    : QDialog(parent)
    , m_document(document)
    , m_output(new QComboBox(this))
    , m_map(new KarnaughMapWidget(this))
    , m_expression(new QLabel(this))
    , m_details(new QLabel(this))
{
    setWindowTitle(tr("Simplify — %1").arg(m_document->displayName()));
    resize(700, 410);

    // Every switch and LED, as the truth table opens with. The circuit is
    // borrowed for the table and handed back as it was.
    lb::Circuit& circuit = m_document->circuit();
    m_result = lb::Simplifier::simplify(circuit, lb::TruthTableGenerator::allSwitches(circuit),
                                        lb::TruthTableGenerator::allLeds(circuit));

    for (const std::string& name : m_result.outputNames)
        m_output->addItem(QString::fromStdString(name));
    m_output->setMinimumWidth(110);
    QStringList inputs;
    for (const std::string& name : m_result.inputNames)
        inputs << QString::fromStdString(name);

    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Output"), this));
    top->addWidget(m_output);
    top->addSpacing(16);
    top->addWidget(new QLabel(inputs.isEmpty() ? QString() : tr("Inputs: %1").arg(inputs.join(QStringLiteral(", "))), this));
    top->addStretch(1);

    auto* mapBox = new QGroupBox(tr("Karnaugh map"), this);
    m_mapBox = mapBox;
    auto* mapLayout = new QVBoxLayout(mapBox);
    mapLayout->addWidget(m_map, 0, Qt::AlignLeft);
    mapLayout->addWidget(hintLabel(tr("Columns in Gray-code order. Shown for 2–4 inputs; "
                                      "up to 8 inputs the expression is still computed."),
                                   mapBox));
    mapLayout->addStretch(1);

    QFont large = m_expression->font();
    large.setPointSizeF(large.pointSizeF() * 1.45);
    m_expression->setFont(large);
    m_expression->setTextFormat(Qt::RichText);
    m_details->setTextFormat(Qt::RichText);
    m_details->setWordWrap(true);

    auto* sopBox = new QGroupBox(tr("Minimal sum of products"), this);
    auto* sopLayout = new QVBoxLayout(sopBox);
    sopLayout->addWidget(m_expression);
    sopLayout->addSpacing(8);
    sopLayout->addWidget(m_details);
    sopLayout->addStretch(1);
    sopLayout->addWidget(hintLabel(tr("Each loop on the map is one term, in the same colour."),
                                   sopBox));

    auto* panels = new QHBoxLayout;
    panels->addWidget(mapBox, 1);
    panels->addWidget(sopBox, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* copyButton = buttons->addButton(tr("Copy expression"), QDialogButtonBox::ActionRole);
    auto* buildButton =
        buttons->addButton(tr("Build minimized circuit"), QDialogButtonBox::ActionRole);
    auto* closeButton = buttons->addButton(QDialogButtonBox::Close);
    copyButton->setAutoDefault(false);
    buildButton->setAutoDefault(false);
    closeButton->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addLayout(panels, 1);
    layout->addWidget(buttons);

    connect(m_output, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &SimplifyDialog::showOutput);
    connect(copyButton, &QPushButton::clicked, this, &SimplifyDialog::copyExpression);
    connect(buildButton, &QPushButton::clicked, this, [this] { emit buildRequested(allEquations()); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (!m_result.ok()) {
        // Nothing to show but the reason.
        m_output->setEnabled(false);
        copyButton->setEnabled(false);
        buildButton->setEnabled(false);
        mapBox->hide();
        m_expression->setText(QStringLiteral("<span style=\"color:#B3261E\">%1</span>")
                                  .arg(tr("Cannot simplify")));
        m_details->setText(QString::fromStdString(m_result.message).toHtmlEscaped());
        return;
    }
    showOutput(0);
}

QString SimplifyDialog::allEquations() const
{
    QStringList lines;
    for (std::size_t i = 0; i < m_result.outputs.size(); ++i)
        lines << QString::fromStdString(m_result.outputNames[i] + " = " + m_result.outputs[i].expression());
    return lines.join(QLatin1Char('\n'));
}

QString SimplifyDialog::expressionText() const
{
    return m_plainExpression;
}

void SimplifyDialog::showOutput(int index)
{
    if (index < 0 || index >= static_cast<int>(m_result.outputs.size()))
        return;
    const lb::SopResult& r = m_result.outputs[static_cast<std::size_t>(index)];
    const QString name = QString::fromStdString(m_result.outputNames[static_cast<std::size_t>(index)]);

    // The map only for 2 to 4 inputs; past that it stops being readable, but
    // the expression is still exact.
    if (r.variables >= 2 && r.variables <= 4) {
        m_map->setFromResult(lb::KarnaughMap::forInputs(r.variables), r);
        m_mapBox->show();
    } else {
        m_mapBox->hide();
    }

    QString html = name.toHtmlEscaped() + QStringLiteral(" = ");
    if (r.isConstantZero()) {
        html += QStringLiteral("0");
    } else {
        QStringList terms;
        for (std::size_t t = 0; t < r.terms.size(); ++t)
            terms << term(termHtml(r, r.terms[t]), KarnaughMapWidget::termColour(static_cast<int>(t)));
        html += terms.join(QStringLiteral(" + "));
    }
    m_expression->setText(html);

    QStringList minterms;
    for (std::uint32_t m : r.minterms)
        minterms << QString::number(m);
    const int canonical = static_cast<int>(r.minterms.size()) * r.variables;
    QString details = tr("Canonical form: Σm(%1)<br><br>Literals: %2 (canonical form: %3)")
                          .arg(minterms.isEmpty() ? QStringLiteral("–") : minterms.join(QStringLiteral(", ")))
                          .arg(r.literalCount()).arg(canonical);
    details += tr("<br><br>Terms: %1").arg(r.terms.size());
    if (!r.exact)
        details += tr("<br><br>This is the best cover found within the search budget; a smaller "
                      "one may exist.");
    m_details->setText(details);
    m_plainExpression = name + QStringLiteral(" = ") + QString::fromStdString(r.expression());
}

void SimplifyDialog::copyExpression()
{
    QApplication::clipboard()->setText(m_plainExpression);
}

} // namespace lbgui
