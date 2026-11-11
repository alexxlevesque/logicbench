#include <gtest/gtest.h>

#include "CircuitDocument.h"
#include "ExampleCatalog.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace lbgui;

TEST(ExampleCatalogTest, ReadsTheBundledCatalogInOrder)
{
    const ExampleCatalog catalog(QStringLiteral(LOGICBENCH_EXAMPLES_DIR));
    ASSERT_EQ(catalog.entries().size(), 6);
    EXPECT_EQ(catalog.entries().front().title, QStringLiteral("Half adder"));
    EXPECT_EQ(catalog.entries().back().file, QStringLiteral("ring_oscillator.lbc"));
    for (const ExampleEntry& e : catalog.entries())
        EXPECT_FALSE(e.description.isEmpty()) << e.file.toStdString();
}

TEST(ExampleCatalogTest, EveryExampleOpensAsAnUntitledCopy)
{
    const ExampleCatalog catalog(QStringLiteral(LOGICBENCH_EXAMPLES_DIR));
    for (int i = 0; i < catalog.entries().size(); ++i) {
        CircuitDocument document;
        QString error;
        ASSERT_TRUE(document.openCopy(catalog.pathFor(i), &error)) << error.toStdString();
        EXPECT_TRUE(document.filePath().isEmpty()) << catalog.entries()[i].file.toStdString();
        EXPECT_EQ(document.displayName(), QStringLiteral("untitled.lbc"));
        EXPECT_GT(document.circuit().componentCount(), 0u);
    }
}

TEST(ExampleCatalogTest, WithoutACatalogEveryCircuitIsListedByName)
{
    QTemporaryDir dir;
    for (const char* name : {"b_second.lbc", "a_first.lbc"})
        QFile::copy(QDir(QStringLiteral(LOGICBENCH_EXAMPLES_DIR)).filePath(QStringLiteral("half_adder.lbc")),
                    QDir(dir.path()).filePath(QString::fromLatin1(name)));
    const ExampleCatalog catalog(dir.path());
    ASSERT_EQ(catalog.entries().size(), 2);
    EXPECT_EQ(catalog.entries()[0].title, QStringLiteral("a_first"));
    EXPECT_EQ(catalog.pathFor(5), QString());
}

TEST(ExampleCatalogTest, AnEntryWhoseFileIsMissingIsLeftOut)
{
    QTemporaryDir dir;
    QFile::copy(QDir(QStringLiteral(LOGICBENCH_EXAMPLES_DIR)).filePath(QStringLiteral("half_adder.lbc")),
                QDir(dir.path()).filePath(QStringLiteral("half_adder.lbc")));
    QFile catalogFile(QDir(dir.path()).filePath(QStringLiteral("catalog.json")));
    ASSERT_TRUE(catalogFile.open(QIODevice::WriteOnly));
    catalogFile.write(R"({"examples":[{"file":"gone.lbc","title":"Gone"},
                                      {"file":"half_adder.lbc","title":"Half adder"}]})");
    catalogFile.close();
    const ExampleCatalog catalog(dir.path());
    ASSERT_EQ(catalog.entries().size(), 1);
    EXPECT_EQ(catalog.entries()[0].title, QStringLiteral("Half adder"));
}
