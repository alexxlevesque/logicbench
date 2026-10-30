#include "MainWindow.h"
#include "TruthTableDialog.h"

#include <QApplication>
#include <QCommandLineParser>
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

    parser.process(app);

    lbgui::MainWindow window;
    window.show();

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty())
        window.openFile(positional.first());

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
