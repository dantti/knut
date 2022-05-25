#include "core/cppdocument.h"
#include "core/knutcore.h"
#include "core/project.h"

#include "common/test_utils.h"

#include <QTest>
#include <QThread>

class TestCppDocument : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { Q_INIT_RESOURCE(core); }

    void correspondingHeaderSource_data()
    {
        QTest::addColumn<QString>("headerOrSource");
        QTest::addColumn<QString>("sourceOrHeader");

        QTest::newRow("cpp") << Test::testDataPath() + "/tst_cppdocument/headerSource/test/hello.cpp"
                             << Test::testDataPath() + "/tst_cppdocument/headerSource/test/hello.h";
        QTest::newRow("h") << Test::testDataPath() + "/tst_cppdocument/headerSource/test/hello.h"
                           << Test::testDataPath() + "/tst_cppdocument/headerSource/test/hello.cpp";
        QTest::newRow("cxx") << Test::testDataPath() + "/tst_cppdocument/headerSource/test/world.cxx"
                             << Test::testDataPath() + "/tst_cppdocument/headerSource/test/world.hpp";
        QTest::newRow("folder") << Test::testDataPath() + "/tst_cppdocument/headerSource/folder1/foo.cpp"
                                << Test::testDataPath() + "/tst_cppdocument/headerSource/folder2/foo.h";
        QTest::newRow("subfolder") << Test::testDataPath() + "/tst_cppdocument/headerSource/test/subfolder1/foo.cpp"
                                   << Test::testDataPath() + "/tst_cppdocument/headerSource/test/subfolder2/foo.h";
        QTest::newRow("notexist") << Test::testDataPath() + "/tst_cppdocument/headerSource/test/bar.cpp"
                                  << "";
    }

    void correspondingHeaderSource()
    {
        QFETCH(QString, headerOrSource);
        QFETCH(QString, sourceOrHeader);

        Core::KnutCore core;
        Core::Project::instance()->setRoot(Test::testDataPath() + "/tst_cppdocument/headerSource");
        auto document = qobject_cast<Core::CppDocument *>(Core::Project::instance()->open(headerOrSource));
        QCOMPARE(sourceOrHeader, document->correspondingHeaderSource());
    }

    void insertForwardDeclaration()
    {
        Test::FileTester file(Test::testDataPath() + "/tst_cppdocument/forwardDeclaration/header.h");
        {
            Core::KnutCore core;
            Core::Project::instance()->setRoot(Test::testDataPath() + "/tst_cppdocument/forwardDeclaration");

            auto headerFile = qobject_cast<Core::CppDocument *>(Core::Project::instance()->get(file.fileName()));
            QCOMPARE(headerFile->insertForwardDeclaration("class Foo"), true);
            QCOMPARE(headerFile->insertForwardDeclaration("class Foo::Bar::FooBar"), true);
            QCOMPARE(headerFile->insertForwardDeclaration("class Foo::Bar::FooBar"), false);
            QCOMPARE(headerFile->insertForwardDeclaration(" "), false);
            QCOMPARE(headerFile->insertForwardDeclaration("Foo::Bar::FooBar"), false);
            QCOMPARE(headerFile->insertForwardDeclaration("struct Neo"), true);
            QCOMPARE(headerFile->insertForwardDeclaration("struct SFoo::SBar::Uno"), true);
            headerFile->save();
            QVERIFY(file.compare());
        }
    }

    void extractDataExchange()
    {
        Core::KnutCore core;
        Core::Project::instance()->setRoot(Test::testDataPath() + "/projects/mfc-tutorial");

        auto document = qobject_cast<Core::CppDocument *>(Core::Project::instance()->open("TutorialDlg.cpp"));
        auto ddxMap = document->mfcExtractDDX("CTutorialDlg");

        QCOMPARE(ddxMap.size(), 8);
        QCOMPARE(ddxMap.value("IDC_ECHO_AREA"), "m_EchoText");
        QCOMPARE(ddxMap.value("IDC_MOUSEECHO"), "m_MouseEcho");
    }

    void gotoBlockStartEnd()
    {
        Core::KnutCore core;
        Core::Project::instance()->setRoot(Test::testDataPath() + "/tst_cppdocument/blockStartEnd");

        auto document = qobject_cast<Core::CppDocument *>(Core::Project::instance()->open("source.cpp"));

        // When cursor position is at the beginning of block
        document->setPosition(0);
        QCOMPARE(document->gotoBlockStart(), 0);
        QCOMPARE(document->gotoBlockEnd(), 0);

        // When cursor position is at the end of block
        document->setPosition(419);
        QCOMPARE(document->gotoBlockStart(), 53);
        QCOMPARE(document->gotoBlockEnd(), 419);

        // When cursor position is in between blocks
        document->setPosition(57);
        QCOMPARE(document->gotoBlockStart(), 53);
        QCOMPARE(document->gotoBlockEnd(), 419);

        document->setPosition(70);
        QCOMPARE(document->gotoBlockStart(), 64);
        QCOMPARE(document->gotoBlockEnd(), 115);

        document->setPosition(330);
        QCOMPARE(document->gotoBlockStart(), 311);
        QCOMPARE(document->gotoBlockEnd(), 390);
    }

    void commentSelection_data()
    {
        QTest::addColumn<int>("regionStartPos");
        QTest::addColumn<int>("regionEndPos");
        QTest::addColumn<QString>("resultantFilePath");

        // #1.1 Selection starts and ends between characters - on different lines
        QTest::newRow("01-1-multi-line-between-to-between")
            << 23 << 53 << (Test::testDataPath() + "/tst_cppdocument/commentSelection/main-01.cpp");

        // #1.2 Selection starts and ends between characters - on different lines - with range set in reverse order
        QTest::newRow("01-2-multi-line-between-to-between-reverse")
            << 53 << 23 << (Test::testDataPath() + "/tst_cppdocument/commentSelection/main-01.cpp");

        // #2 Selection starts before characters and ends between characters - on different lines
        QTest::newRow("02-multi-line-before-to-between")
            << 15 << 53 << (Test::testDataPath() + "/tst_cppdocument/commentSelection/main-02.cpp");

        // #3 Selection starts between characters and ends before characters - on different lines
        QTest::newRow("03-multi-line-between-to-before")
            << 23 << 45 << (Test::testDataPath() + "/tst_cppdocument/commentSelection/main-03.cpp");

        // #4 Selection starts and ends before characters - on different lines
        QTest::newRow("04-multi-line-before-to-before")
            << 15 << 45 << (Test::testDataPath() + "/tst_cppdocument/commentSelection/main-04.cpp");

        // #5 Selection starts and ends between characters - on same line
        QTest::newRow("05-single-line-between-to-between")
            << 18 << 23 << (Test::testDataPath() + "/tst_cppdocument/commentSelection/main-05.cpp");

        // #6 Selection starts before characters and ends between characters - on same line
        QTest::newRow("06-single-line-before-to-between")
            << 15 << 23 << (Test::testDataPath() + "/tst_cppdocument/commentSelection/main-06.cpp");

        // #7 Selection starts and ends before characters - on same line
        QTest::newRow("07-single-line-before-to-before")
            << 14 << 16 << (Test::testDataPath() + "/tst_cppdocument/commentSelection/main-07.cpp");

        // #8 There is no selection - but the position is valid
        QTest::newRow("08-no-selection-valid-position")
            << 30 << -1 << (Test::testDataPath() + "/tst_cppdocument/commentSelection/main-08.cpp");

        // #9 There is no selection - the position is valid - but on an empty line
        QTest::newRow("09-no-selection-valid-position-empty-line")
            << 58 << -1 << (Test::testDataPath() + "/tst_cppdocument/commentSelection/main-no-change.cpp");
    }

    void commentSelection()
    {
        QFETCH(int, regionStartPos);
        QFETCH(int, regionEndPos);
        QFETCH(QString, resultantFilePath);

        Core::KnutCore core;
        Core::Project::instance()->setRoot(Test::testDataPath() + "/tst_cppdocument/commentSelection");

        auto cppDocument = qobject_cast<Core::CppDocument *>(Core::Project::instance()->open("main.cpp"));
        if (regionEndPos == -1) {
            cppDocument->setPosition(regionStartPos);
        } else {
            cppDocument->selectRegion(regionStartPos, regionEndPos);
        }
        cppDocument->commentSelection();
        cppDocument->save();
        QVERIFY(Test::compareFiles(cppDocument->fileName(), resultantFilePath));
        cppDocument->undo();
        cppDocument->save();
    }

    void insertRemoveInclude()
    {
        Test::FileTester file(Test::testDataPath() + "/tst_cppdocument/insertRemoveInclude/include.cpp");
        {
            Core::KnutCore core;
            Core::Project::instance()->setRoot(Test::testDataPath() + "/tst_cppdocument/insertRemoveInclude");
            auto cppFile = qobject_cast<Core::CppDocument *>(Core::Project::instance()->open(file.fileName()));

            // Add include files
            QVERIFY(cppFile->insertInclude(R"("folder/foobar.h")"));
            cppFile->insertInclude("<QPushButton>");
            cppFile->insertInclude("<memory>", true);

            // Mal-formed includes
            cppFile->insertInclude(R"("foobar.h)");
            cppFile->insertInclude(R"(<foobar.h)");
            cppFile->insertInclude(R"(foobar.h")");
            QVERIFY(!cppFile->insertInclude(R"(foobar.h>)"));

            // Remove include files
            QVERIFY(cppFile->removeInclude(R"("bar.h")"));
            cppFile->removeInclude("<spdlog/spdlog.h>");
            cppFile->removeInclude("<QComboBox>");

            // Mal-formed includes
            cppFile->removeInclude(R"("foobar.h)");
            cppFile->removeInclude(R"(<foobar.h)");
            cppFile->removeInclude(R"(foobar.h")");
            QVERIFY(!cppFile->removeInclude(R"(foobar.h>)"));

            cppFile->save();
            QVERIFY(file.compare());
        }
    }
};

QTEST_MAIN(TestCppDocument)
#include "tst_cppdocument.moc"
