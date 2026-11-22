#include <QtTest>

#include "engine/event/EventEngine.h"

// 事件引擎兼容性回归：同时锁定输出、逻辑时间、事件计数与脏元件顺序。
// 逐事件轨迹作为预期值直接写在测试中；并非在运行时加载另一版本引擎作比较。
namespace {
QVariantMap component(const QString& id, const QString& type, int width = 1)
{
    return {{"id", id}, {"type", type}, {"bitWidth", width}};
}

QVariantMap wire(const QString& from, const QString& to)
{
    return {{"fromComp", from}, {"fromPort", 0}, {"toComp", to}, {"toPort", 0}};
}

// 双驱动夹具：a/b 汇入 probe，再传到 tail；a 到 probe 的重复边刻意保留。
// 线延迟为 2、output 门延迟为 5，使同一时刻的重复投递与后续传播可逐步断言。
void buildMultiDriver(EventEngine& engine, int width)
{
    engine.setWireDelay(2);
    engine.setGateTypeDelay("output", 5);
    engine.build({component("a", "input", width), component("b", "input", width),
                  component("probe", "output", width), component("tail", "output", width)},
                 {wire("a", "probe"), wire("a", "probe"), wire("b", "probe"), wire("probe", "tail")},
                 {}, {});
}
}

class TestEventEquivalence : public QObject {
    Q_OBJECT

private slots:
    void multiDriverTrace();
    void multiDriverWidths();
    void dirtyOrderAndReset();
    void feedbackBudget();
    void multiDriverThroughput_data();
    void multiDriverThroughput();
};

void TestEventEquivalence::multiDriverTrace()
{
    // 逐事件可观察轨迹已与 backup/perf-registry-20261122-165721 对照。
    // 每帧只处理一个事件，避免仅比较稳态输出而漏掉中间值、同刻事件顺序和脏标记差异。
    EventEngine engine;
    buildMultiDriver(engine, 4);
    QCOMPARE(engine.drainNow(1), 1);
    engine.setDefaultRunBudget(1);
    QCOMPARE(engine.drainNow(), 1);
    QVERIFY(engine.isIdle());
    QCOMPARE(engine.totalEventsProcessed(), 2ULL);
    QCOMPARE(engine.time(), 0ULL);

    engine.triggerForceOutput("a", 0, "Z1");
    engine.triggerForceOutput("b", 0, "XZ11X");
    QCOMPARE(engine.takeDirtyQids(), (QStringList{"a", "b"}));
    QCOMPARE(engine.drainNow(), 0);
    QVERIFY(engine.run(1));
    QCOMPARE(engine.time(), 1ULL);

    const struct {
        quint64 time;
        const char* probe;
        const char* tail;
        QStringList dirty;
    } trace[] = {
        {2, "000E", "0000", {"probe"}},
        {2, "000E", "0000", {}}, // 重复连线仍消耗一个事件，但相同值不再新增脏标记。
        {2, "0E11", "0000", {"probe"}},
        {7, "0E11", "000E", {"tail"}},
        {7, "0E11", "0E11", {"tail"}},
    };
    quint64 count = 2;
    for (const auto& frame : trace) {
        QVERIFY(engine.step());
        QCOMPARE(engine.totalEventsProcessed(), ++count);
        QCOMPARE(engine.time(), frame.time);
        QCOMPARE(engine.getInputs("probe"), (QStringList{frame.probe}));
        QCOMPARE(engine.getOutput("probe"), QString::fromLatin1(frame.probe));
        QCOMPARE(engine.getOutput("tail"), QString::fromLatin1(frame.tail));
        QCOMPARE(engine.takeDirtyQids(), frame.dirty);
    }
    QVERIFY(!engine.step());
    QVERIFY(engine.isIdle());

    // 对同一位而言，没有 0、1、E 驱动时，Z 与未知字符共同解析为 Z。
    engine.triggerInputString("a", "ZZZZ");
    engine.triggerInputString("b", "ZXZE");
    QVERIFY(!engine.run(100));
    QCOMPARE(engine.time(), 9ULL);
    QCOMPARE(engine.totalEventsProcessed(), 8ULL);
    QVERIFY(engine.run(100, 100));
    QCOMPARE(engine.time(), 109ULL);
    QCOMPARE(engine.totalEventsProcessed(), 12ULL);
    QCOMPARE(engine.getOutput("tail"), QStringLiteral("ZZZE"));
    QCOMPARE(engine.takeDirtyQids(), (QStringList{"a", "b", "probe", "tail"}));
}

void TestEventEquivalence::multiDriverWidths()
{
    EventEngine engine;
    buildMultiDriver(engine, 4);
    QCOMPARE(engine.drainNow(), 2);
    engine.triggerForceOutput("a", 0, "ZZZZ");
    engine.triggerForceOutput("b", 0, "");
    QVERIFY(engine.run(10));
    QCOMPARE(engine.getOutput("probe"), QStringLiteral("0000"));

    // 空投递值回退到源元件的最新输出，它可能不同于当前事件入队时携带的状态。
    // 连续入队再逐步执行，刻意让历史事件与最新源值错开，同时验证补零后的四位输出。
    engine.triggerForceOutput("b", 0, "1");
    engine.triggerForceOutput("b", 0, "");
    engine.triggerForceOutput("b", 0, "Z1");
    QVERIFY(engine.step());
    QCOMPARE(engine.getOutput("probe"), QStringLiteral("0001"));
    QVERIFY(engine.step());
    QCOMPARE(engine.getOutput("probe"), QStringLiteral("00Z1"));
    QVERIFY(engine.step());
    QCOMPARE(engine.getOutput("probe"), QStringLiteral("00Z1"));
    QCOMPARE(engine.totalEventsProcessed(), 8ULL);
    QVERIFY(engine.run(10));
    QCOMPARE(engine.totalEventsProcessed(), 10ULL);
    QCOMPARE(engine.getOutput("tail"), QStringLiteral("00Z1"));
}

// 脏集合须去重并保留首次标记顺序；消费、清空、reset 和重新 build 均需清理跟踪状态。
// 同时覆盖未知 ID 与空 ID，且 reset 不应误清除已设定的元件输出。
void TestEventEquivalence::dirtyOrderAndReset()
{
    EventEngine engine;
    engine.build({component("a", "input"), component("b", "input"),
                  component("", "input"), component("clk", "clock")}, {}, {}, {});
    engine.markDirty("unknown");
    engine.triggerInputBit("b", 0, true);
    engine.markDirty("a");
    engine.triggerInputString("a", "1");
    engine.markDirty("b");
    engine.markDirty("unknown");
    engine.triggerClockToggle("clk");
    engine.triggerClockToggle("clk");
    QCOMPARE(engine.dirtyCount(), 4);
    QCOMPARE(engine.takeDirtyQids(), (QStringList{"unknown", "b", "a", "clk"}));
    QVERIFY(engine.takeDirtyQids().isEmpty());
    engine.markDirty(QString());
    engine.triggerInputBit("", 0, true);
    const auto dirty = engine.takeDirtyQids();
    QCOMPARE(dirty.size(), 1);
    QVERIFY(dirty.first().isNull());
    engine.triggerInputBit("a", 0, true);
    engine.markDirty("unknown");
    engine.clearDirtyQids();
    QCOMPARE(engine.dirtyCount(), 0);
    engine.triggerInputBit("a", 0, true);
    engine.markDirty("unknown");
    engine.reset();
    QCOMPARE(engine.getOutput("a"), QStringLiteral("1"));
    QVERIFY(engine.takeDirtyQids().isEmpty());
    engine.triggerInputBit("a", 0, true);
    QCOMPARE(engine.takeDirtyQids(), (QStringList{"a"}));
    engine.markDirty("a");
    engine.build({component("b", "input"), component("a", "input")}, {}, {}, {});
    QVERIFY(engine.takeDirtyQids().isEmpty());
    engine.markDirty("b");
    engine.markDirty("a");
    QCOMPARE(engine.takeDirtyQids(), (QStringList{"b", "a"}));
}

// 自反馈非门持续振荡，不要求收敛；验证事件预算限制处理量，时间窗口仍可向前推进。
// 随后单步应继续处理未完成的事件，不能因预算耗尽丢队列或错误宣告空闲。
void TestEventEquivalence::feedbackBudget()
{
    EventEngine engine;
    engine.setGateTypeDelay("not", 3);
    engine.setDefaultRunBudget(2);
    engine.build({component("loop", "not")}, {wire("loop", "loop")}, {}, {});
    QCOMPARE(engine.drainNow(), 1);
    const QString initial = engine.getOutput("loop");
    QVERIFY(!engine.isIdle());
    QVERIFY(!engine.run(100));
    QCOMPARE(engine.time(), 6ULL);
    QCOMPARE(engine.totalEventsProcessed(), 3ULL);
    QCOMPARE(engine.getOutput("loop"), initial);
    QCOMPARE(engine.takeDirtyQids(), (QStringList{"loop"}));
    QVERIFY(engine.run(2, 1));
    QCOMPARE(engine.time(), 8ULL);
    QCOMPARE(engine.totalEventsProcessed(), 3ULL);
    QVERIFY(engine.step());
    QCOMPARE(engine.time(), 9ULL);
    QCOMPARE(engine.totalEventsProcessed(), 4ULL);
    QVERIFY(engine.getOutput("loop") != initial);
    QVERIFY(!engine.isIdle());
}

void TestEventEquivalence::multiDriverThroughput_data()
{
    QTest::addColumn<int>("driverWidthDelta");
    QTest::newRow("same-width") << 0;
    QTest::newRow("mixed-width") << 8;
}

// 同宽与混合宽度共用相同翻转模式；准备/初次收敛在 QBENCHMARK 计时块之外。
// Qt Test 可重复执行计时块，因此仅验证事件增量是每批预期事件数的整数倍，不锁定总轮数。
// 此基准检查空闲状态和工作量，不设机器相关的耗时门槛。
void TestEventEquivalence::multiDriverThroughput()
{
    QFETCH(int, driverWidthDelta);
    constexpr int width = 64;
    constexpr int batches = 300;
    EventEngine engine;
    buildMultiDriver(engine, width);
    QCOMPARE(engine.drainNow(), 2);
    const QString z(width - driverWidthDelta, 'Z');
    const QString one(width + driverWidthDelta, '1');
    const QString zero(width + driverWidthDelta, '0');
    engine.triggerForceOutput("a", 0, z);
    engine.triggerForceOutput("b", 0, zero);
    QVERIFY(engine.run(10));
    QVERIFY(engine.isIdle());
    engine.clearDirtyQids();
    const quint64 eventsBefore = engine.totalEventsProcessed();
    QBENCHMARK {
        for (int i = 0; i < batches; ++i) {
            engine.triggerForceOutput("b", 0, one);
            engine.triggerForceOutput("b", 0, zero);
            engine.run(10);
            engine.clearDirtyQids();
        }
    }
    QVERIFY(engine.isIdle());
    QCOMPARE((engine.totalEventsProcessed() - eventsBefore) % (4 * batches), 0ULL);
}

QTEST_GUILESS_MAIN(TestEventEquivalence)
#include "test_event_equivalence.moc"
