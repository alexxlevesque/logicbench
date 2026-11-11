#pragma once

#include <QDialog>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;

namespace lbgui {

class CircuitDocument;

/// Build from expression (FR-7): one Boolean equation per output line.
///
/// The text is parsed on every keystroke: the message under the editor names
/// the inputs and outputs, or the line and column of the first problem, and
/// Build is enabled only when every line reads. Build hands the text to
/// CircuitDocument::buildFromEquations, so the circuit arrives as one undo step.
class ExpressionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExpressionDialog(CircuitDocument* document, QWidget* parent = nullptr);

    void setEquations(const QString& text);
    QString equations() const;

    /// The line of the first syntax error, or 0 if the text reads.
    int errorLine() const;
    bool canBuild() const;

private slots:
    /// Re-reads the equations on every keystroke and updates the message.
    void reparse();
    void build();

private:
    CircuitDocument* m_document;
    QPlainTextEdit* m_editor;
    QLabel* m_status;
    QRadioButton* m_newCircuit;
    QRadioButton* m_intoOpen;
    QPushButton* m_buildButton;
};

} // namespace lbgui
