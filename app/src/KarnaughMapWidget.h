#pragma once

#include <QColor>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace lb {
class KarnaughMap;
struct SopResult;
}

namespace lbgui {

/// Draws one Karnaugh map: cells in Gray-code order, and each term of the
/// minimal expression as a loop in that term's colour, the same colour the
/// Simplify window writes the term in.
class KarnaughMapWidget : public QWidget {
    Q_OBJECT
public:
    /// A rectangle of cells, in map coordinates, drawn as one rounded loop.
    struct Loop {
        int row = 0;
        int column = 0;
        int rows = 1;
        int columns = 1;
        QColor colour;
        bool dashed = false;
    };

    explicit KarnaughMapWidget(QWidget* parent = nullptr);

    /// rowVariables and columnVariables are the axis labels, e.g. "A" and
    /// "B Cin"; values is rows x columns, already in Gray-code order.
    void setMap(QString rowVariables, QString columnVariables,
                QStringList rowLabels, QStringList columnLabels,
                QVector<QVector<int>> values, QVector<Loop> loops);

    /// Fills the map from the engine: the layout from `map`, the 1s from the
    /// result's minterms, and one loop (or several, where a term wraps an
    /// edge) per term, coloured with termColour(i).
    void setFromResult(const lb::KarnaughMap& map, const lb::SopResult& result);

    /// The colour of the i-th term, shared with the expression text.
    static QColor termColour(int i);

    QSize sizeHint() const override;

    const QVector<Loop>& loops() const { return m_loops; }
    const QVector<QVector<int>>& values() const { return m_values; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_rowVariables;
    QString m_columnVariables;
    QStringList m_rowLabels;
    QStringList m_columnLabels;
    QVector<QVector<int>> m_values;
    QVector<Loop> m_loops;
};

} // namespace lbgui
