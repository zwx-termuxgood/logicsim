#include <QtTest>
#include "logic/ComponentLogic.h"

class TestComponentRegistry : public QObject {
    Q_OBJECT
private slots:
    void allBuiltinsRetainedAndSorted() {
        QStringList expected{"input", "clock", "const", "text", "not", "and", "or",
                             "nand", "nor", "xor", "xnor", "led", "output", "splitter",
                             "hub", "sub", "tgate", "ntran", "ptran"};
        expected.sort();
        QCOMPARE(ComponentLogic::typeIds(), expected);
        for (const auto& id : expected) {
            QCOMPARE(Components::definition(id).typeId(), id);
            QVERIFY(ComponentLogic::role(id) != ComponentLogic::Role::Unknown);
        }
    }
    void unknownFallback() {
        QVariantMap c{{"type", "future"}, {"bitWidth", 7}};
        QCOMPARE(ComponentLogic::inputCount(c), 0);
        QCOMPARE(ComponentLogic::outputCount(c), 0);
        QCOMPARE(ComponentLogic::inputBitWidth(c, -1), 7);
        QCOMPARE(ComponentLogic::outputBitWidth(c, 999), 7);
        QVERIFY(ComponentLogic::inputPorts(c).isEmpty());
        QVERIFY(ComponentLogic::outputPorts(c).isEmpty());
        QVERIFY(ComponentLogic::computeCombinationalOutputs("future", {"1"}, 1, {}).isEmpty());
        ComponentLogic::initializeDefaults(c);
        QVERIFY(c.value("outputPorts").toList().isEmpty());
        QVERIFY(c.value("inputPortValues").toList().isEmpty());
    }
    void creationCompatibility() {
        for (const auto& id : ComponentLogic::typeIds()) {
            QVariantMap c{{"type", id}, {"bitWidth", 4}, {"inputCount", 5}, {"name", ""}};
            ComponentLogic::initializeDefaults(c, "child");
            const auto in = c.value("inputPortValues").toList();
            const auto out = c.value("outputPorts").toList();
            if (id == "clock" || id == "const") {
                QVERIFY(in.isEmpty());
                QVERIFY(out.isEmpty());
                QCOMPARE(c.value("name").toString(), id == "clock" ? QString("CLK") : QString("CONST"));
            } else {
                QCOMPARE(in.size(), id == "tgate" ? 3 : ComponentLogic::inputCount(c));
                QCOMPARE(out.size(), ComponentLogic::outputCount(c));
            }
            if (id == "tgate") {
                QCOMPARE(c.value("inputCount").toInt(), 3);
                QCOMPARE(ComponentLogic::inputCount(c), 2);
            }
            if (id == "splitter" || id == "hub")
                QCOMPARE(c.value("outputSplits").toList(), QVariantList({1, 1, 1, 1}));
            if (id == "sub") QCOMPARE(c.value("subId").toString(), QString("child"));
            if (id == "text") QCOMPARE(c.value("content").toString(), QString::fromUtf8("文本"));
        }
    }
    void widthsAndAsymmetricGeometry() {
        QVariantMap c{{"type", "sub"}, {"bitWidth", 8},
                      {"subInputNames", QVariantList{"a", "b"}},
                      {"subOutputNames", QVariantList{"a", "b", "c", "d", "e"}},
                      {"subInputWidths", QVariantList{0, 4}}, {"subOutputWidths", QVariantList{3}}};
        QCOMPARE(ComponentLogic::inputBitWidth(c, 0), 1);
        QCOMPARE(ComponentLogic::inputBitWidth(c, 1), 4);
        QCOMPARE(ComponentLogic::outputBitWidth(c, 1), 8);
        QCOMPARE(ComponentLogic::outputPorts(c).first().toMap().value("y").toInt(), -48);
        QCOMPARE(ComponentLogic::inputPorts(c).first().toMap().value("y").toInt(), -15);
        c["rotation"] = -1;
        QCOMPARE(ComponentLogic::outputPorts(c).first().toMap().value("x").toInt(), -48);
    }
    void fourStateLogicAndBusOrdering() {
        QCOMPARE(ComponentLogic::computeCombinationalOutputs("and", {"0Z1E", "1111"}, 4, {}), QVector<QString>{"0E1E"});
        QCOMPARE(ComponentLogic::computeCombinationalOutputs("not", {"01ZE"}, 4, {}), QVector<QString>{"10EE"});
        QCOMPARE(ComponentLogic::computeCombinationalOutputs("tgate", {"ZE10", "110E"}, 4, {}), QVector<QString>{"ZEZE"});
        QCOMPARE(ComponentLogic::computeCombinationalOutputs("ptran", {"01ZE", "1010"}, 4, {}), QVector<QString>{"1ZEE"});
        const QVariantList splits{3, 3, 2};
        const auto parts = ComponentLogic::computeCombinationalOutputs("splitter", {"11001010"}, 8, splits);
        // Preserve existing offset-from-end splitting and reverse-input merging.
        QCOMPARE(parts, QVector<QString>({"010", "001", "11"}));
        QCOMPARE(ComponentLogic::computeCombinationalOutputs("hub", parts, 8, splits), QVector<QString>{"11001010"});
        QVERIFY(ComponentLogic::computeCombinationalOutputs("splitter", {"1"}, 1, {}).isEmpty());
    }
};
QTEST_APPLESS_MAIN(TestComponentRegistry)
#include "test_component_registry.moc"
