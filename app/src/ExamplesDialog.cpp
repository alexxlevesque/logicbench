#include "ExamplesDialog.h"

#include "CircuitDocument.h"
#include "CircuitScene.h"
#include "QuickStartTracker.h"

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
    scene.rebuild();
    const QRectF contents = scene.itemsBoundingRect().adjusted(-24, -24, 24, 24);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    scene.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)), contents,
                 Qt::KeepAspectRatio);
    return pixmap;
}

} // namespace

ExamplesDialog::ExamplesDialog(const QString& examplesDirectory, QuickStartTracker* tracker,
                               QWidget* parent)
    : QDialog(parent)
    , m_directory(examplesDirectory)
    , m_catalog(examplesDirectory)
    , m_tracker(tracker)
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

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

QWidget* ExamplesDialog::buildExamplesTab()
{
    auto* page = new QWidget(this);
    m_list = new QListWidget(page);
    m_preview = new QLabel(page);
    m_title = new QLabel(page);
    m_description = new QLabel(page);

    for (const ExampleEntry& example : m_catalog.entries()) {
        auto* item = new QListWidgetItem(example.title, m_list);
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
            emit openCopyRequested(m_catalog.pathFor(row));
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

    for (int i = 0; i < QuickStartTracker::kSteps; ++i) {
        auto* box = new QCheckBox(tr("%1.  %2").arg(i + 1).arg(QuickStartTracker::stepText(i)), page);
        box->setAttribute(Qt::WA_TransparentForMouseEvents);    // ticked by the canvas, not by hand
        box->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(box);
        m_steps.append(box);
    }
    layout->addSpacing(6);
    m_progress = new QLabel(page);
    m_progress->setWordWrap(true);
    m_progress->setStyleSheet(QStringLiteral("color:#3C7A50;"));
    layout->addWidget(m_progress);
    layout->addStretch(1);
    auto* restart = new QPushButton(tr("Start again"), page);
    restart->setAutoDefault(false);
    layout->addWidget(restart, 0, Qt::AlignLeft);

    if (m_tracker) {
        connect(m_tracker, &QuickStartTracker::progressed, this, &ExamplesDialog::refreshSteps);
        connect(restart, &QPushButton::clicked, this, [this] {
            m_tracker->reset();
            refreshSteps();
        });
    } else {
        restart->setEnabled(false);
    }
    refreshSteps();
    return page;
}

void ExamplesDialog::refreshSteps()
{
    int done = 0;
    int next = -1;
    for (int i = 0; i < m_steps.size(); ++i) {
        const bool ticked = m_tracker && m_tracker->isDone(i);
        m_steps[i]->setChecked(ticked);
        done += ticked ? 1 : 0;
        if (!ticked && next < 0)
            next = i;
    }
    if (next < 0)
        m_progress->setText(tr("All six done: that is a working half adder. Try the full adder next."));
    else
        m_progress->setText(tr("%1 of %2 done. Next: %3").arg(done).arg(m_steps.size())
                                .arg(QuickStartTracker::stepText(next)));
}

int ExamplesDialog::stepsTicked() const
{
    int n = 0;
    for (const QCheckBox* box : m_steps)
        n += box->isChecked() ? 1 : 0;
    return n;
}

int ExamplesDialog::exampleCount() const
{
    return m_list->count();
}

void ExamplesDialog::setShowAtStartup(bool on)
{
    m_atStartup->setChecked(on);
}

void ExamplesDialog::selectExample(int row)
{
    m_list->setCurrentRow(row);
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
    if (row < 0 || row >= m_catalog.entries().size())
        return;
    const ExampleEntry& example = m_catalog.entries()[row];
    m_title->setText(example.title);
    m_description->setText(example.description);
    m_preview->setPixmap(renderPreview(m_catalog.pathFor(row), m_preview->size() - QSize(2, 2),
                                       devicePixelRatioF()));
}

} // namespace lbgui
