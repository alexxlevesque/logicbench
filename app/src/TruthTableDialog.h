#pragma once

#include "logicbench/TruthTable.h"

#include <QDialog>

class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;

namespace lbgui {

class CircuitDocument;

/// Pick inputs and outputs, generate the table, export it.
///
/// The dialog holds no logic of its own: TruthTableGenerator does the work in
/// the engine, and the same call is what the tests exercise.
class TruthTableDialog : public QDialog {
    Q_OBJECT

public:
    explicit TruthTableDialog(CircuitDocument* document, QWidget* parent = nullptr);

private slots:
    void generate();
    void exportCsv();

private:
    void populateSelectors();
    QList<lb::ComponentId> checkedIds(QListWidget* list) const;

    CircuitDocument* m_document;
    QListWidget* m_inputs;
    QListWidget* m_outputs;
    QTableWidget* m_table;
    QLabel* m_message;
    QPushButton* m_exportButton;
    lb::TruthTable m_lastTable;
};

} // namespace lbgui
