#include "SimplifyDialog.h"

#include "CircuitDocument.h"

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


const QColor kBrown(0x9A, 0x5B, 0x00);
const QColor kGreen(0x3C, 0x7A, 0x50);
const QColor kBlue(0x2F, 0x6F, 0xB3);
const QColor kPurple(0x7B, 0x61, 0xFF);

QString term(const QString& text, const QColor& colour)
{
    return QStringLiteral("<span style=\"color:%1\">%2</span>").arg(colour.name(), text);
}

QString overbar(const QString& name)
{
    return QStringLiteral("<span style=\"text-decoration:overline\">%1</span>").arg(name);
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

    m_output->addItems({QStringLiteral("Cout"), QStringLiteral("S")});
    m_output->setMinimumWidth(110);

    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Output"), this));
    top->addWidget(m_output);
    top->addSpacing(16);
    top->addWidget(new QLabel(tr("Inputs: A, B, Cin"), this));
    top->addStretch(1);

    auto* mapBox = new QGroupBox(tr("Karnaugh map"), this);
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
    connect(buildButton, &QPushButton::clicked, this,
            [this] { emit buildRequested(m_plainExpression); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    showOutput(0);
}

void SimplifyDialog::showOutput(int index)
{
    // Sample data: a full adder, A B Cin -> Cout S. lb::Simplifier replaces
    // this once FR-7 lands.
    const QStringList rows{QStringLiteral("0"), QStringLiteral("1")};
    const QStringList columns{QStringLiteral("00"), QStringLiteral("01"), QStringLiteral("11"),
                              QStringLiteral("10")};

    if (index == 0) {
        m_map->setMap(QStringLiteral("A"), QStringLiteral("B Cin"), rows, columns,
                      {{0, 0, 1, 0}, {0, 1, 1, 1}},
                      {{0, 2, 2, 1, kBlue, false},
                       {1, 1, 1, 2, kGreen, false},
                       {1, 2, 1, 2, kBrown, true}});
        m_expression->setText(QStringLiteral("Cout = ") + term(QStringLiteral("A B"), kBrown)
                              + QStringLiteral(" + ") + term(QStringLiteral("A Cin"), kGreen)
                              + QStringLiteral(" + ") + term(QStringLiteral("B Cin"), kBlue));
        m_details->setText(tr("Canonical form: Σm(3, 5, 6, 7)<br><br>"
                              "Literals: 6 (canonical form: 12)<br><br>"
                              "Gates: 3 two-input AND, 1 three-input OR"));
        m_plainExpression = QStringLiteral("Cout = (A & B) | (A & Cin) | (B & Cin)");
        return;
    }

    m_map->setMap(QStringLiteral("A"), QStringLiteral("B Cin"), rows, columns,
                  {{0, 1, 0, 1}, {1, 0, 1, 0}},
                  {{0, 1, 1, 1, kBlue, false},
                   {0, 3, 1, 1, kGreen, false},
                   {1, 0, 1, 1, kBrown, true},
                   {1, 2, 1, 1, kPurple, false}});
    const QString a = QStringLiteral("A");
    const QString b = QStringLiteral("B");
    const QString cin = QStringLiteral("Cin");
    m_expression->setText(
        QStringLiteral("S = ") + term(overbar(a) + QStringLiteral(" ") + overbar(b) + QStringLiteral(" Cin"), kBlue)
        + QStringLiteral(" + ") + term(overbar(a) + QStringLiteral(" B ") + overbar(cin), kGreen)
        + QStringLiteral(" + ") + term(QStringLiteral("A ") + overbar(b) + QStringLiteral(" ") + overbar(cin), kBrown)
        + QStringLiteral(" + ") + term(QStringLiteral("A B Cin"), kPurple));
    m_details->setText(tr("Canonical form: Σm(1, 2, 4, 7)<br><br>"
                          "Literals: 12 (canonical form: 12): no two 1s are adjacent, "
                          "so nothing combines.<br><br>"
                          "Gates: 4 three-input AND, 1 four-input OR, 3 NOT"));
    m_plainExpression =
        QStringLiteral("S = (!A & !B & Cin) | (!A & B & !Cin) | (A & !B & !Cin) | (A & B & Cin)");
}

void SimplifyDialog::copyExpression()
{
    QApplication::clipboard()->setText(m_plainExpression);
}

} // namespace lbgui
