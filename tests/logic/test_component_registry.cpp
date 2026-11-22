#include <QtTest>
#include "logic/ComponentLogic.h"

// 注册表兼容性回归：覆盖内置类型集合、未知类型回退、创建默认值及端口/总线语义。
// 这里保留历史上可观察的特殊行为，避免统一分派实现时顺带改变文档数据约定。
class TestComponentRegistry : public QObject {
    Q_OBJECT
private slots:
    // 固定完整内置集合及排序契约，并检查类型定义和角色映射没有在注册迁移时丢失。
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
    // 用未来类型和越界端口索引构造回退场景：无端口/输出，但位宽查询保留元件配置。
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
    // 创建夹具遍历所有已注册类型；显式传入空名称和子电路 ID，检查默认值兼容性。
    // clock/const 不在创建阶段填端口状态；tgate 保留三个状态槽，但逻辑输入数为两个。
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
    // 子电路左右端口数刻意不对称；零位宽夹到 1，缺失端口位宽回退到总线宽度。
    // 固定首端口坐标与负旋转结果，避免重构时按单侧端口数重新居中导致几何漂移。
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
    // 四态位串含 0/1/Z（高阻）/E（错误），逐位预期用于锁定门与晶体管的传播规则。
    void fourStateLogicAndBusOrdering() {
        QCOMPARE(ComponentLogic::computeCombinationalOutputs("and", {"0Z1E", "1111"}, 4, {}), QVector<QString>{"0E1E"});
        QCOMPARE(ComponentLogic::computeCombinationalOutputs("not", {"01ZE"}, 4, {}), QVector<QString>{"10EE"});
        QCOMPARE(ComponentLogic::computeCombinationalOutputs("tgate", {"ZE10", "110E"}, 4, {}), QVector<QString>{"ZEZE"});
        QCOMPARE(ComponentLogic::computeCombinationalOutputs("ptran", {"01ZE", "1010"}, 4, {}), QVector<QString>{"1ZEE"});
        const QVariantList splits{3, 3, 2};
        const auto parts = ComponentLogic::computeCombinationalOutputs("splitter", {"11001010"}, 8, splits);
        // 保留从字符串末端按偏移拆分、按输入反序合并的既有位序，两者组合应还原总线。
        QCOMPARE(parts, QVector<QString>({"010", "001", "11"}));
        QCOMPARE(ComponentLogic::computeCombinationalOutputs("hub", parts, 8, splits), QVector<QString>{"11001010"});
        QVERIFY(ComponentLogic::computeCombinationalOutputs("splitter", {"1"}, 1, {}).isEmpty());
    }
};
QTEST_APPLESS_MAIN(TestComponentRegistry)
#include "test_component_registry.moc"
