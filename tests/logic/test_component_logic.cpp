#include <QtTest>
#include "logic/ComponentLogic.h"
#include "helpers/TestHelpers.h"

class TestComponentLogic : public QObject {
    Q_OBJECT
private slots:
    void inputCount_data() {
        QTest::addColumn<QString>("type");
        QTest::addColumn<int>("ic");
        QTest::addColumn<int>("expected");
        QTest::newRow("input")  << "input"  << 2  << 0;
        QTest::newRow("not")    << "not"    << 2  << 1;
        QTest::newRow("and")    << "and"    << 3  << 3;
        QTest::newRow("and_0")  << "and"    << 0  << 2;
        QTest::newRow("and_65") << "and"    << 65 << 64;
    }
    void inputCount() {
        QFETCH(QString, type); QFETCH(int, ic); QFETCH(int, expected);
        QCOMPARE(ComponentLogic::inputCount(makeComp(type, 0, 0, 1, ic)), expected);
    }
    void computeAnd() {
        auto outs = ComponentLogic::computeCombinationalOutputs(
            "and", {"1100", "1010"}, 4, {});
        QCOMPARE(outs.size(), 1);
        QCOMPARE(outs[0], QString("1000"));
    }
    void computeAndWithError() {
        auto outs = ComponentLogic::computeCombinationalOutputs(
            "and", {"11E0", "1010"}, 4, {});
        QCOMPARE(outs[0], QString("10E0"));
    }
    void splitter_splits_by_outputSplits() {
        auto outs = ComponentLogic::computeCombinationalOutputs(
            "splitter", {"11001010"}, 8, {3, 3, 2});
        QCOMPARE(outs.size(), 3);
        QCOMPARE(outs[0], QString("110"));
        QCOMPARE(outs[1], QString("010"));
        QCOMPARE(outs[2], QString("10"));
    }
};
QTEST_GUILESS_MAIN(TestComponentLogic)
#include "test_component_logic.moc"
