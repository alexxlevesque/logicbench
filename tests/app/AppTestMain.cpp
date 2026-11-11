// Entry point of the application-layer tests: GoogleTest, with a
// QApplication alive for the whole run so the document, the clipboard and
// QSettings work exactly as they do in the program.

#include <gtest/gtest.h>

#include <QApplication>

int main(int argc, char* argv[])
{
    // Headless unless the caller asks for a real display.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("LogicBenchTests"));
    QApplication::setApplicationName(QStringLiteral("logicbench_app_tests"));
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
