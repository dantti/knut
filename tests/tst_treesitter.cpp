#include <QTest>

#include "common/test_utils.h"

#include "treesitter/languages.h"
#include "treesitter/parser.h"
#include "treesitter/predicates.h"
#include "treesitter/query.h"
#include "treesitter/transformation.h"
#include "treesitter/tree.h"

class TestTreeSitter : public QObject
{
    Q_OBJECT

private:
    QString readTestFile(const QString &relativePath)
    {
        QFile mainfile(Test::testDataPath() + relativePath);
        if (!mainfile.open(QFile::ReadOnly | QFile::Text)) {
            spdlog::warn("Couldn't open file: {}", mainfile.fileName().toStdString());
            return "";
        }
        QTextStream in(&mainfile);
        return in.readAll();
    }

private slots:
    void initTestCase() { Q_INIT_RESOURCE(core); }

    void parsesMainFile()
    {
        auto source = readTestFile("/tst_treesitter/main.cpp");

        treesitter::Parser parser(tree_sitter_cpp());

        auto tree = parser.parseString(source);
        QVERIFY(tree.has_value());
        auto root = tree->rootNode();
        QVERIFY(!root.isNull());
        QVERIFY(!root.isMissing());
        QVERIFY(!root.hasError());

        QVERIFY(root.type() == "translation_unit");
        QCOMPARE(root.namedChildren().size(), 9);
    }

    void querySyntaxError()
    {
        using Error = treesitter::Query::Error;
        // Syntax Error - missing ")".
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), "(field_expression"));
        // Invalid node type
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), "(field_expr)"));
        // Invalid field
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), "(field_expression arg: (_))"));
        // Capture Error
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), "(field_expression (#eq? @from @from))"));
        // Structure Error
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), "(field_expression \"*\")"));
        // Predicate errors
        // Non-existing predicate
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), "(#non_existing_predicate?)"));
    }

    void simpleQuery()
    {
        auto source = readTestFile("/tst_treesitter/main.cpp");

        treesitter::Parser parser(tree_sitter_cpp());
        auto tree = parser.parseString(source);
        QVERIFY(tree.has_value());

        auto query = std::make_shared<treesitter::Query>(tree_sitter_cpp(), R"EOF(
        (field_expression
            argument: (_) @arg
            field: (_) @field
            (#eq? @arg "object")
            ) @from
                )EOF");

        auto captures = query->captures();
        QCOMPARE(captures.size(), 3);
        QCOMPARE(captures.at(0).name, "arg");
        QCOMPARE(captures.at(1).name, "field");
        QCOMPARE(captures.at(2).name, "from");

        auto patterns = query->patterns();
        QCOMPARE(patterns.size(), 1);

        const auto &pattern = patterns.first();
        QCOMPARE(pattern.predicates.size(), 1);
        QCOMPARE(pattern.predicates.first().name, "eq?");

        const auto &arguments = pattern.predicates.first().arguments;
        QCOMPARE(arguments.size(), 2);

        QVERIFY(std::holds_alternative<treesitter::Query::Capture>(arguments.at(0)));
        QCOMPARE(std::get<treesitter::Query::Capture>(arguments.at(0)).name, "arg");

        QVERIFY(std::holds_alternative<QString>(arguments.at(1)));
        QCOMPARE(std::get<QString>(arguments.at(1)), "object");

        treesitter::QueryCursor cursor;
        cursor.execute(query, tree->rootNode(), nullptr /*disable predicates*/);

        auto match = cursor.nextMatch();
        QVERIFY(match.has_value());
        QVERIFY(match->patternIndex() == 0);

        // Assure there is no second match
        auto nextMatch = cursor.nextMatch();
        QVERIFY(!nextMatch.has_value());
    }

    void failedQuery()
    {
        auto source = readTestFile("/tst_treesitter/main.cpp");

        treesitter::Parser parser(tree_sitter_cpp());
        auto tree = parser.parseString(source);
        QVERIFY(tree.has_value());

        // main.cpp only contains a field_expression with "." access, not "->" access.
        auto query = std::make_shared<treesitter::Query>(tree_sitter_cpp(), R"EOF(
        (field_expression
            argument: (_) @arg
            "->"
            field: (_) @field
            ) @from
                )EOF");

        treesitter::QueryCursor cursor;
        cursor.execute(query, tree->rootNode(), nullptr /*disable predicates*/);

        // The query should not match
        QVERIFY(!cursor.nextMatch().has_value());
    }

    void transformMemberAccess()
    {
        auto source = readTestFile("/tst_treesitter/main.cpp");

        treesitter::Parser parser(tree_sitter_cpp());
        auto tree = parser.parseString(source);
        QVERIFY(tree.has_value());

        auto query = std::make_shared<treesitter::Query>(tree_sitter_cpp(), R"EOF(
        (field_expression
            argument: (_) @arg
            "."
            field: (_) @field
            ) @from
                )EOF");

        treesitter::Transformation transformation(source, std::move(parser), std::move(query), "@arg->@field");

        auto result = transformation.run();
        QCOMPARE(result, readTestFile("/tst_treesitter/main-arrow.cpp"));
    }

    void transformationErrors()
    {
        auto source = readTestFile("/tst_treesitter/main.cpp");

        {
            treesitter::Parser parser(tree_sitter_cpp());
            auto query = std::make_shared<treesitter::Query>(tree_sitter_cpp(), R"EOF(
                (field_expression
                    argument: (_) @arg
                    "."
                    field: (_) @field
                    )
                )EOF");

            treesitter::Transformation missingFromTransformation(source, std::move(parser), std::move(query),
                                                                 "@arg->@field");
            QVERIFY_THROWS_EXCEPTION(treesitter::Transformation::Error, missingFromTransformation.run());
        }

        {
            treesitter::Parser parser(tree_sitter_cpp());
            auto query = std::make_shared<treesitter::Query>(tree_sitter_cpp(), R"EOF(
                (field_expression
                    argument: (_) @arg
                    field: (_) @field
                    )
                )EOF");

            treesitter::Transformation recursiveTransformation(source, std::move(parser), std::move(query),
                                                               "@arg->@field");
            QVERIFY_THROWS_EXCEPTION(treesitter::Transformation::Error, recursiveTransformation.run());
        }
    }

    void capture_quantifiers()
    {
        auto source = readTestFile("/tst_treesitter/main.cpp");

        treesitter::Parser parser(tree_sitter_cpp());
        auto tree = parser.parseString(source);

        auto query = std::make_shared<treesitter::Query>(tree_sitter_cpp(), R"EOF(
                (parameter_list
                    ["," (parameter_declaration) @arg]+)
        )EOF");

        treesitter::QueryCursor cursor;
        cursor.execute(query, tree->rootNode(), std::make_unique<treesitter::Predicates>(source));

        auto matches = cursor.allRemainingMatches();
        // 7 Matches, including declarations and function pointers
        QCOMPARE(matches.size(), 7);

        // TreeSitter will return multiple captures for the same capture identifier if
        // a quantifier is used.
        QCOMPARE(matches[0].captures().size(), 2);
        QCOMPARE(matches[1].captures().size(), 2);
        QCOMPARE(matches[2].captures().size(), 2); // function pointer list comes first
        QCOMPARE(matches[3].captures().size(), 6);
        QCOMPARE(matches[4].captures().size(), 2);
        QCOMPARE(matches[5].captures().size(), 6);
        QCOMPARE(matches[6].captures().size(), 2);
    }

    void eq_predicate_errors()
    {
        using Error = treesitter::Query::Error;
        // Too few arguments
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), R"EOF(
        (#eq?)
        )EOF"));
    }

    void eq_predicate()
    {

        auto source = readTestFile("/tst_treesitter/main.cpp");
        treesitter::Parser parser(tree_sitter_cpp());
        auto tree = parser.parseString(source);
        QVERIFY(tree.has_value());

        auto query = std::make_shared<treesitter::Query>(tree_sitter_cpp(), R"EOF(
            (function_definition
                (function_declarator
                    declarator: (_) @name
                    (#eq? "main" @name)
                    ))
        )EOF");

        treesitter::QueryCursor cursor;
        cursor.execute(query, tree->rootNode(), std::make_unique<treesitter::Predicates>(source));

        auto firstMatch = cursor.nextMatch();

        QVERIFY(firstMatch.has_value());
        auto captures = firstMatch->capturesNamed("name");
        QCOMPARE(captures.size(), 1);
        QCOMPARE(captures.first().node.textIn(source), "main");

        QVERIFY(!cursor.nextMatch().has_value());
    }

    void match_predicate_errors()
    {
        using Error = treesitter::Query::Error;
        // Too few arguments
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), "(#match?)"));

        // Non-regex argument
        QVERIFY_THROWS_EXCEPTION(
            Error, treesitter::Query(tree_sitter_cpp(), "((identifier) @ident (#match? @ident @ident)))"));

        // Invalid regex
        QVERIFY_THROWS_EXCEPTION(
            Error, treesitter::Query(tree_sitter_cpp(), "((identifier) @ident (#match? \"tes[\" @ident))"));

        // Non-capture argument
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), "(#match? \"test\" \"test\")"));
    }

    void match_predicate()
    {
        auto source = readTestFile("/tst_treesitter/main.cpp");
        treesitter::Parser parser(tree_sitter_cpp());
        auto tree = parser.parseString(source);
        QVERIFY(tree.has_value());

        auto query = std::make_shared<treesitter::Query>(tree_sitter_cpp(), R"EOF(
            (function_definition
                (function_declarator
                    declarator: (_) @name
                    (#match? "my(Other)?FreeFunction" @name)
                    ))
        )EOF");

        treesitter::QueryCursor cursor;
        cursor.execute(query, tree->rootNode(), std::make_unique<treesitter::Predicates>(source));

        auto firstMatch = cursor.nextMatch();
        QVERIFY(firstMatch.has_value());
        auto firstCaptures = firstMatch->capturesNamed("name");
        QCOMPARE(firstCaptures.size(), 1);
        QCOMPARE(firstCaptures.first().node.textIn(source), "myFreeFunction");

        auto secondMatch = cursor.nextMatch();
        QVERIFY(secondMatch.has_value());
        auto secondCaptures = secondMatch->capturesNamed("name");
        QCOMPARE(secondCaptures.size(), 1);
        QCOMPARE(secondCaptures.first().node.textIn(source), "myOtherFreeFunction");

        QVERIFY(!cursor.nextMatch().has_value());
    }

    void in_message_map_predicate_errors()
    {
        using Error = treesitter::Query::Error;
        // Too few arguments
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), "(#in_message_map?)"));

        // Non-capture argument
        QVERIFY_THROWS_EXCEPTION(Error, treesitter::Query(tree_sitter_cpp(), "(#in_message_map? \"xxxx\"))"));
    }

    void in_message_map_predicate()
    {
        auto source = readTestFile("/tst_treesitter/mfc-TutorialDlg.cpp");
        treesitter::Parser parser(tree_sitter_cpp());
        auto tree = parser.parseString(source);
        QVERIFY(tree.has_value());

        auto query = std::make_shared<treesitter::Query>(tree_sitter_cpp(), R"EOF(
            (
            (call_expression
                (argument_list . (_) . (_) .) @args) @call
            (#in_message_map? @call @args))
        )EOF");

        treesitter::QueryCursor cursor;
        cursor.execute(query, tree->rootNode(), std::make_unique<treesitter::Predicates>(source));

        auto matches = cursor.allRemainingMatches();
        QCOMPARE(matches.size(), 2);
    }
};

QTEST_MAIN(TestTreeSitter)
#include "tst_treesitter.moc"