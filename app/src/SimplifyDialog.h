#pragma once

#include "KarnaughMapWidget.h"

#include <QDialog>

class QComboBox;
class QLabel;

namespace lbgui {

class CircuitDocument;

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
