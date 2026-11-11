#pragma once

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
/// Shell for the SDD screen capture, filled with the RAD's CheckBeforeTheLab
/// scenario. When wired in, lb::Specification reads the file and
/// lb::SpecChecker produces the rows shown here.
class SpecCheckDialog : public QDialog {
    Q_OBJECT
public:
    explicit SpecCheckDialog(CircuitDocument* document, QWidget* parent = nullptr);

    void setSpecPath(const QString& path);

private slots:
    void browse();
    void recheck();

private:
    void showSampleResult();

    CircuitDocument* m_document;
    QLineEdit* m_path;
    QLabel* m_matched;
    QLabel* m_verdict;
    QCheckBox* m_onlyDiffering;
    QTableWidget* m_rows;
};

} // namespace lbgui
