#pragma once

#include "KarnaughMapWidget.h"

#include "logicbench/Simplifier.h"

#include <QDialog>

class QComboBox;
class QGroupBox;
class QLabel;

namespace lbgui {

class CircuitDocument;

/// Simplify (FR-7): a minimal sum of products for each output, with its
/// Karnaugh map for 2 to 4 inputs.
///
/// lb::Simplifier runs over every switch and LED when the window opens;
/// lb::KarnaughMap lays out the map and each term is drawn and written in the
/// same colour. Build minimized circuit hands every output's expression to
/// ExpressionDialog, so the minimized circuit is built by the same parser and
/// builder. A sequential circuit, too many inputs or rows that read X are
/// refused with the reason.
class SimplifyDialog : public QDialog {
    Q_OBJECT
public:
    explicit SimplifyDialog(CircuitDocument* document, QWidget* parent = nullptr);

    const lb::SimplifyResult& result() const { return m_result; }
    /// The shown output's expression in parser syntax, "Y = ...".
    QString expressionText() const;
    /// Every output's expression, one equation per line.
    QString allEquations() const;

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
    QGroupBox* m_mapBox = nullptr;
    lb::SimplifyResult m_result;
};

} // namespace lbgui
