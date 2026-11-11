#pragma once

#include <QColor>
#include <QDialog>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;

namespace lbgui {

class CircuitDocument;

/// Draws one Karnaugh map: cells in Gray-code order, and each term of the
/// minimal expression as a loop in that term's colour.
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
    QSize sizeHint() const override;

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

/// Simplify (FR-7): a minimal sum of products for each output, with its
/// Karnaugh map for 2 to 4 inputs.
///
/// Shell for the SDD screen capture, filled with a full adder's outputs.
/// When wired in, lb::Simplifier supplies the terms and lb::KarnaughMap the
/// layout, and Build minimized circuit opens ExpressionDialog filled in.
class SimplifyDialog : public QDialog {
    Q_OBJECT
public:
    explicit SimplifyDialog(CircuitDocument* document, QWidget* parent = nullptr);

signals:
    /// Build minimized circuit: the equations to hand to ExpressionDialog.
    void buildRequested(const QString& equations);

private slots:
    void showOutput(int index);
    void copyExpression();

private:
    CircuitDocument* m_document;
    QComboBox* m_output;
    KarnaughMapWidget* m_map;
    QLabel* m_expression;
    QLabel* m_details;
    QString m_plainExpression;
};

} // namespace lbgui
