#include "ExamplesDialog.h"

#include "CircuitDocument.h"
#include "CircuitScene.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace lbgui {

namespace {

struct SampleExample {
    const char* file;
    const char* title;
    const char* description;
};

// ExampleCatalog will read these from examples/catalog.json (FR-3).
const SampleExample kExamples[] = {
    {"half_adder.lbc", "Half adder",
     "An XOR gate for the sum and an AND gate for the carry. Flip A and B through all four "
     "combinations, then open the truth table."},
    {"full_adder.lbc", "Full adder",
     "Two half-adder blocks and an OR gate. The first example that uses blocks: it is the "
     "circuit inside every stage of the 4-bit adder."},
    {"adder4.lbc", "4-bit adder",
     "Four full-adder blocks in a ripple chain, two levels of nesting. Flip A0–A3 and "
     "B0–B3 to add two 4-bit numbers."},
    {"counter3.lbc", "3-bit counter",
     "A clock and three D flip-flops. Press Run (F5) and watch the bits count in the timing "
     "diagram."},
    {"sr_latch.lbc", "SR latch",
     "Two cross-coupled NOR gates: memory without a clock. Set, then reset, and see that it "
     "holds in between."},
    {"ring_oscillator.lbc", "Ring oscillator",
     "Three inverters in a ring. The loop can never settle, so LogicBench outlines it and "
     "reports it instead of hanging."},
};

const char* const kQuickStartSteps[] = {
    "Drag two switches from the palette onto the canvas, and label them A and B.",
    "Drag an XOR gate and an AND gate onto the canvas.",
    "Drag from each switch to an input of both gates to wire them.",
    "Drag two LEDs onto the canvas and label them S and C.",
    "Wire the XOR output to S and the AND output to C.",
    "Flip A and B, then press Ctrl+T and check the truth table.",
};

/// Renders a bundled circuit the way the canvas would draw it.
QPixmap renderPreview(const QString& path, QSize size, qreal ratio)
{
    QPixmap pixmap(size * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::white);

    CircuitDocument document;
    QString error;
    if (!document.open(path, &error))
        return pixmap;
    CircuitScene scene(&document);
    const QRectF contents = scene.itemsBoundingRect().adjusted(-24, -24, 24, 24);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    scene.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)), contents,
                 Qt::KeepAspectRatio);
    return pixmap;
}

} // namespace

ExamplesDialog::ExamplesDialog(const QString& examplesDirectory, QWidget* parent)
    : QDialog(parent)
    , m_directory(examplesDirectory)
    , m_tabs(new QTabWidget(this))
    , m_list(nullptr)
    , m_preview(nullptr)
    , m_title(nullptr)
    , m_description(nullptr)
    , m_atStartup(new QCheckBox(tr("Show this window at start-up"), this))
{
    setWindowTitle(tr("Examples and quick start"));
    resize(660, 400);

    m_tabs->addTab(buildExamplesTab(), tr("Examples"));
    m_tabs->addTab(buildQuickStartTab(), tr("Quick start"));
    m_atStartup->setChecked(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setDefault(true);
    auto* footer = new QHBoxLayout;
    footer->addWidget(m_atStartup);
    footer->addStretch(1);
    footer->addWidget(buttons);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs, 1);
    layout->addLayout(footer);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_list->setCurrentRow(2);
}

QWidget* ExamplesDialog::buildExamplesTab()
{
    auto* page = new QWidget(this);
    m_list = new QListWidget(page);
    m_preview = new QLabel(page);
    m_title = new QLabel(page);
    m_description = new QLabel(page);

    for (const SampleExample& example : kExamples) {
        auto* item = new QListWidgetItem(tr(example.title), m_list);
        item->setSizeHint(QSize(0, 30));
    }
    m_list->setFixedWidth(170);

    m_preview->setFixedSize(250, 190);
    m_preview->setStyleSheet(QStringLiteral("border:1px solid #B8BEC6; background:white;"));
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_description->setWordWrap(true);
    m_description->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto* openButton = new QPushButton(tr("Open a copy"), page);
    openButton->setAutoDefault(false);

    auto* text = new QVBoxLayout;
    text->addWidget(m_title);
    text->addWidget(m_description, 1);
    text->addWidget(openButton, 0, Qt::AlignLeft);

    auto* layout = new QHBoxLayout(page);
    layout->addWidget(m_list);
    layout->addWidget(m_preview, 0, Qt::AlignTop);
    layout->addLayout(text, 1);

    connect(m_list, &QListWidget::currentRowChanged, this, &ExamplesDialog::showExample);
    connect(openButton, &QPushButton::clicked, this, [this] {
        const int row = m_list->currentRow();
        if (row >= 0)
            emit openCopyRequested(QDir(m_directory).filePath(QString::fromUtf8(kExamples[row].file)));
    });
    return page;
}

QWidget* ExamplesDialog::buildQuickStartTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* intro = new QLabel(tr("Build a half adder in six steps. Each step ticks itself off "
                                "when you do it on the canvas."),
                             page);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    layout->addSpacing(6);

    // Sample progress for the capture: the first three steps done.
    int index = 0;
    for (const char* step : kQuickStartSteps) {
        auto* box = new QCheckBox(tr("%1.  %2").arg(index + 1).arg(tr(step)), page);
        box->setChecked(index < 3);
        box->setAttribute(Qt::WA_TransparentForMouseEvents);
        box->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(box);
        ++index;
    }
    layout->addSpacing(6);
    auto* status = new QLabel(tr("3 of 6 done. Next: drag two LEDs onto the canvas."), page);
    status->setStyleSheet(QStringLiteral("color:#3C7A50;"));
    layout->addWidget(status);
    layout->addStretch(1);
    auto* restart = new QPushButton(tr("Start again"), page);
    restart->setAutoDefault(false);
    layout->addWidget(restart, 0, Qt::AlignLeft);
    return page;
}

void ExamplesDialog::showTab(Tab tab)
{
    m_tabs->setCurrentIndex(tab);
}

bool ExamplesDialog::showAtStartup() const
{
    return m_atStartup->isChecked();
}

void ExamplesDialog::showExample(int row)
{
    if (row < 0)
        return;
    const SampleExample& example = kExamples[row];
    m_title->setText(tr(example.title));
    m_description->setText(tr(example.description));
    m_preview->setPixmap(renderPreview(QDir(m_directory).filePath(QString::fromUtf8(example.file)),
                                       m_preview->size() - QSize(2, 2), devicePixelRatioF()));
}

} // namespace lbgui
