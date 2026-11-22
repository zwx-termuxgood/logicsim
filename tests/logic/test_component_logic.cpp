#include <QtTest>
#include "logic/ComponentLogic.h"
#include "helpers/TestHelpers.h"

class TestComponentLogic : public QObject {
    Q_OBJECT
private slots:
    void integerPortGeometry() {
        // 子电路两侧端口数不同会产生分数间距，覆盖全部类型与四种旋转。
        const QStringList types{"input", "clock", "const", "text", "not", "and", "or",
                                "nand", "nor", "xor", "xnor", "led", "output", "splitter",
                                "hub", "sub", "tgate", "ntran", "ptran"};
        for (const auto& type : types) {
            for (int rotation = 0; rotation < 4; ++rotation) {
                QVariantMap comp{{"type", type}, {"bitWidth", 1}, {"inputCount", 3},
                                 {"rotation", rotation}, {"outputSplits", QVariantList{1, 1}},
                                 {"subInputNames", QVariantList{"a", "b"}},
                                 {"subOutputNames", QVariantList{"a", "b", "c", "d", "e"}}};
                for (const auto& geometry : {ComponentLogic::inputPorts(comp), ComponentLogic::outputPorts(comp)}) {
                    for (const auto& value : geometry) {
                        const auto p = value.toMap();
                        QCOMPARE(p.value("x").metaType().id(), int(QMetaType::Int));
                        QCOMPARE(p.value("y").metaType().id(), int(QMetaType::Int));
                    }
                }
            }
        }
        QVariantMap sub{{"type", "sub"}, {"subInputNames", QVariantList{"a", "b", "c", "d", "e"}},
                        {"subOutputNames", QVariantList{"a", "b", "c", "d", "e", "f", "g"}}};
        const auto outputs = ComponentLogic::outputPorts(sub);
        QCOMPARE(outputs.first().toMap().value("y").toInt(), -72);
        QCOMPARE(outputs[1].toMap().value("y").toInt(), -48);
        sub["subOutputNames"] = QVariantList{"a", "b"};
        QCOMPARE(ComponentLogic::outputPorts(sub).first().toMap().value("y").toInt(), -30);
        sub["subInputNames"] = QVariantList{"a", "b", "c"};
        sub["subOutputNames"] = QVariantList{"a", "b", "c", "d", "e"};
        QCOMPARE(ComponentLogic::outputPorts(sub).first().toMap().value("y").toInt(), -48);
        sub["subOutputNames"] = QVariantList{"a", "b", "c", "d", "e", "f", "g"};
        sub["subInputNames"] = QVariantList{"a", "b", "c", "d", "e", "f", "g", "h"};
        QCOMPARE(ComponentLogic::outputPorts(sub).first().toMap().value("y").toInt(), -82);
        sub["rotation"] = 2;
        QCOMPARE(ComponentLogic::outputPorts(sub).first().toMap().value("y").toInt(), 82);
        sub["rotation"] = 1;
        QCOMPARE(ComponentLogic::outputPorts(sub).first().toMap().value("x").toInt(), 82);
        QVariantMap transistor{{"type", "ntran"}};
        QCOMPARE(ComponentLogic::outputPorts(transistor).first().toMap().value("y").toInt(), -15);
    }
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
