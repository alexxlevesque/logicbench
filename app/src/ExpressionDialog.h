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
/// Shell for the SDD screen capture. The window, its layout and its live
/// message exist; Build is not wired in yet. When it is, the text goes to
/// lb::ExpressionParser, the equations to lb::CircuitBuilder, and the result
/// into the document through one InsertFragmentCommand.
class ExpressionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExpressionDialog(CircuitDocument* document, QWidget* parent = nullptr);

    void setEquations(const QString& text);
    QString equations() const;

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
