#include "KarnaughMapWidget.h"

#include "logicbench/KarnaughMap.h"
#include "logicbench/SopMinimizer.h"

#include <QFontDatabase>
#include <QPainter>

#include <algorithm>

namespace lbgui {

namespace {
constexpr int kCell = 54;
constexpr int kRowHeader = 64;
constexpr int kColumnHeader = 30;
} // namespace

KarnaughMapWidget::KarnaughMapWidget(QWidget* parent)
    : QWidget(parent)
{
}

QColor KarnaughMapWidget::termColour(int i)
{
    static const QColor palette[] = {
        QColor(0x9A, 0x5B, 0x00), QColor(0x3C, 0x7A, 0x50), QColor(0x2F, 0x6F, 0xB3),
        QColor(0x7B, 0x61, 0xFF), QColor(0xB3, 0x26, 0x1E), QColor(0x0E, 0x7C, 0x86),
        QColor(0x8A, 0x4F, 0x9E), QColor(0x5C, 0x67, 0x73),
    };
    return palette[static_cast<std::size_t>(i) % (sizeof(palette) / sizeof(palette[0]))];
}

void KarnaughMapWidget::setMap(QString rowVariables, QString columnVariables,
                               QStringList rowLabels, QStringList columnLabels,
                               QVector<QVector<int>> values, QVector<Loop> loops)
{
    m_rowVariables = std::move(rowVariables);
    m_columnVariables = std::move(columnVariables);
    m_rowLabels = std::move(rowLabels);
    m_columnLabels = std::move(columnLabels);
    m_values = std::move(values);
    m_loops = std::move(loops);
    setMinimumSize(sizeHint());
    updateGeometry();
    update();
}

void KarnaughMapWidget::setFromResult(const lb::KarnaughMap& map, const lb::SopResult& result)
{
    QStringList rowNames, columnNames, rowLabels, columnLabels;
    for (int v = 0; v < map.rowBits(); ++v)
        rowNames << QString::fromStdString(result.names[static_cast<std::size_t>(v)]);
    for (int v = map.rowBits(); v < map.variables(); ++v)
        columnNames << QString::fromStdString(result.names[static_cast<std::size_t>(v)]);
    for (int r = 0; r < map.rows(); ++r)
        rowLabels << QString::fromStdString(map.rowLabel(r));
    for (int c = 0; c < map.columns(); ++c)
        columnLabels << QString::fromStdString(map.columnLabel(c));

    QVector<QVector<int>> values(map.rows(), QVector<int>(map.columns(), 0));
    for (std::uint32_t m : result.minterms) {
        const auto cell = map.cellOf(m);
        if (cell.first >= 0)
            values[cell.first][cell.second] = 1;
    }

    QVector<Loop> loops;
    for (std::size_t t = 0; t < result.terms.size(); ++t) {
        for (const lb::CellRect& r : map.loopsFor(result.terms[t]))
            loops.append(Loop{r.row, r.column, r.rows, r.columns, termColour(static_cast<int>(t)), false});
    }
    setMap(rowNames.join(QLatin1Char(' ')), columnNames.join(QLatin1Char(' ')), rowLabels, columnLabels,
           values, loops);
}

QSize KarnaughMapWidget::sizeHint() const
{
    return QSize(kRowHeader + kCell * static_cast<int>(m_columnLabels.size()) + 12,
                 kColumnHeader + kCell * static_cast<int>(m_rowLabels.size()) + 12);
}

void KarnaughMapWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    const QPointF origin(kRowHeader, kColumnHeader);
    painter.setPen(QColor(0x2D, 0x37, 0x48));
    painter.drawText(QRectF(0, 0, kRowHeader, kColumnHeader), Qt::AlignCenter,
                     m_rowVariables + QStringLiteral(" \\ ") + m_columnVariables);

    painter.setFont(mono);
    for (int c = 0; c < m_columnLabels.size(); ++c)
        painter.drawText(QRectF(origin.x() + c * kCell, 0, kCell, kColumnHeader),
                         Qt::AlignCenter, m_columnLabels[c]);
    for (int r = 0; r < m_rowLabels.size(); ++r)
        painter.drawText(QRectF(0, origin.y() + r * kCell, kRowHeader - 10, kCell),
                         Qt::AlignRight | Qt::AlignVCenter, m_rowLabels[r]);

    for (int r = 0; r < m_values.size(); ++r) {
        for (int c = 0; c < m_values[r].size(); ++c) {
            const QRectF cell(origin.x() + c * kCell, origin.y() + r * kCell, kCell, kCell);
            painter.setPen(QColor(0xB8, 0xBE, 0xC6));
            painter.setBrush(Qt::white);
            painter.drawRect(cell);
            painter.setPen(QColor(0x2D, 0x37, 0x48));
            painter.drawText(cell, Qt::AlignCenter, QString::number(m_values[r][c]));
        }
    }

    // Loops are inset by a different amount for each colour, so overlapping
    // terms stay distinguishable where they share a cell.
    painter.setBrush(Qt::NoBrush);
    QVector<QColor> seen;
    for (const Loop& loop : m_loops) {
        int index = static_cast<int>(seen.indexOf(loop.colour));
        if (index < 0) {
            seen.append(loop.colour);
            index = static_cast<int>(seen.size()) - 1;
        }
        const qreal inset = 5.0 + 3.0 * std::min(index, 6);
        const QRectF box(origin.x() + loop.column * kCell + inset,
                         origin.y() + loop.row * kCell + inset,
                         loop.columns * kCell - 2 * inset, loop.rows * kCell - 2 * inset);
        QPen pen(loop.colour, 2.4);
        if (loop.dashed)
            pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawRoundedRect(box, 10, 10);
    }
}

} // namespace lbgui
