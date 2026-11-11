#pragma once

#include "logicbench/SpecChecker.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace lbgui {

class CircuitDocument;

/// Check against specification (FR-4): compare the open circuit with a
/// truth-table CSV and list every row that differs.
///
/// lb::Specification reads the file (a malformed line is named) and
/// lb::SpecChecker compares; the table lists every differing output, or every
/// row. Re-check runs again after an edit without picking the file again.
class SpecCheckDialog : public QDialog {
    Q_OBJECT
public:
    explicit SpecCheckDialog(CircuitDocument* document, QWidget* parent = nullptr);

    void setSpecPath(const QString& path);

    /// The result of the last check, for tests.
    const lb::SpecCheckResult& lastResult() const { return m_lastResult; }
    int rowCount() const;

private slots:
    void browse();
    void recheck();

private:
    void showVerdict(bool passed, const QString& text);
    void fillRows(const lb::SpecCheckResult& result, const lb::Specification& spec);

    CircuitDocument* m_document;
    QLineEdit* m_path;
    QLabel* m_matched;
    QLabel* m_verdict;
    QCheckBox* m_onlyDiffering;
    QTableWidget* m_rows;
    lb::SpecCheckResult m_lastResult;
};

} // namespace lbgui
