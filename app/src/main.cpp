#include "BlockLibraryDialog.h"
#include "ExamplesDialog.h"
#include "ExportImageDialog.h"
#include "ExpressionDialog.h"
#include "MainWindow.h"
#include "SimplifyDialog.h"
#include "SpecCheckDialog.h"
#include "TruthTableDialog.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QPixmap>
#include <QTimer>

#include <iostream>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("LogicBench");
    QApplication::setOrganizationName("CMPE 320");
    QApplication::setApplicationVersion("0.4.0");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main", "Digital-logic simulator."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QCoreApplication::translate("main", "circuit"),
        QCoreApplication::translate("main", "Circuit file (.lbc) to open."));

    // Renders the window to a PNG and exits. Used to produce the images in the
    // documentation, and to check in CI that the interface still draws — it
    // works under "-platform offscreen", with no display attached.
    QCommandLineOption screenshotOption(
        QStringList{QStringLiteral("screenshot")},
        QCoreApplication::translate("main", "Save a screenshot to <file> and exit."),
        QCoreApplication::translate("main", "file"));
    parser.addOption(screenshotOption);

    QCommandLineOption runTicksOption(
        QStringList{QStringLiteral("run-ticks")},
        QCoreApplication::translate("main", "Simulate <n> ticks before screenshotting."),
        QCoreApplication::translate("main", "n"), QStringLiteral("0"));
    parser.addOption(runTicksOption);

    QCommandLineOption truthTableOption(
        QStringList{QStringLiteral("truth-table")},
        QCoreApplication::translate("main", "Screenshot the truth-table window instead."));
    parser.addOption(truthTableOption);

    QCommandLineOption traceAllOption(
        QStringList{QStringLiteral("trace-all")},
        QCoreApplication::translate("main", "Plot every signal on the timing diagram."));
    parser.addOption(traceAllOption);

    // Opens one planned window (FR-3 to FR-8) over the main window and
    // leaves it up, so it can be captured with its title bar. The SDD's
    // screen captures are taken this way.
    QCommandLineOption showDialogOption(
        QStringList{QStringLiteral("show-dialog")},
        QCoreApplication::translate("main",
            "Open a planned window: expression, simplify, spec-check, block-library, "
            "export-image, examples, quick-start or recovery."),
        QCoreApplication::translate("main", "name"));
    parser.addOption(showDialogOption);

    parser.process(app);

    lbgui::MainWindow window;
    window.show();

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty())
        window.openFile(positional.first());

    if (parser.isSet(showDialogOption)) {
        const QString name = parser.value(showDialogOption);
        QTimer::singleShot(300, &window, [&window, name] {
            QDialog* dialog = nullptr;
            if (name == QLatin1String("expression")) {
                auto* d = new lbgui::ExpressionDialog(window.document(), &window);
                d->setEquations(QStringLiteral("S    = A ^ B ^ Cin\n"
                                               "Cout = (A & B) | (Cin & (A ^ B))"));
                dialog = d;
            } else if (name == QLatin1String("simplify")) {
                dialog = new lbgui::SimplifyDialog(window.document(), &window);
            } else if (name == QLatin1String("spec-check")) {
                dialog = new lbgui::SpecCheckDialog(window.document(), &window);
            } else if (name == QLatin1String("block-library")) {
                dialog = new lbgui::BlockLibraryDialog(window.blockLibrary(), &window);
            } else if (name == QLatin1String("export-image")) {
                dialog = new lbgui::ExportImageDialog(true, &window);
            } else if (name == QLatin1String("examples") || name == QLatin1String("quick-start")) {
                auto* d = new lbgui::ExamplesDialog(QStringLiteral(LOGICBENCH_EXAMPLES_DIR), &window);
                if (name == QLatin1String("quick-start"))
                    d->showTab(lbgui::ExamplesDialog::QuickStartTab);
                dialog = d;
            } else if (name == QLatin1String("recovery")) {
                // FR-3's launch prompt, as AutosaveManager will show it.
                auto* box = new QMessageBox(QMessageBox::Question, QObject::tr("LogicBench"),
                    QObject::tr("LogicBench closed unexpectedly."),
                    QMessageBox::NoButton, &window);
                box->setInformativeText(QObject::tr(
                    "Restore the unsaved work from 14:32 today (full_adder.lbc)?"));
                box->addButton(QObject::tr("Restore"), QMessageBox::AcceptRole);
                box->addButton(QObject::tr("Discard"), QMessageBox::DestructiveRole);
                dialog = box;
            }
            if (dialog) {
                dialog->setWindowModality(Qt::NonModal);
                dialog->show();
                dialog->raise();
                dialog->activateWindow();
            } else {
                std::cerr << "unknown dialog: " << name.toStdString() << '\n';
            }
        });
    }

    if (parser.isSet(screenshotOption)) {
        const QString path = parser.value(screenshotOption);
        const int ticks = parser.value(runTicksOption).toInt();

        // Let the window lay itself out before grabbing it.
        const bool traceAll = parser.isSet(traceAllOption);
        const bool truthTable = parser.isSet(truthTableOption);
        QTimer::singleShot(400, &window, [&window, path, ticks, traceAll, truthTable] {
            if (traceAll)
                window.traceAllSignals();
            if (ticks > 0)
                window.simulateTicks(ticks);
            QCoreApplication::processEvents();

            if (truthTable) {
                lbgui::TruthTableDialog dialog(window.document(), &window);
                dialog.resize(720, 460);
                dialog.show();
                QCoreApplication::processEvents();
                const QPixmap dialogShot = dialog.grab();
                if (dialogShot.save(path))
                    std::cout << "wrote " << path.toStdString() << '\n';
                QCoreApplication::quit();
                return;
            }

            const QPixmap shot = window.grab();
            if (shot.save(path))
                std::cout << "wrote " << path.toStdString() << '\n';
            else
                std::cerr << "could not write " << path.toStdString() << '\n';
            QCoreApplication::quit();
        });
    }

    return QApplication::exec();
}
