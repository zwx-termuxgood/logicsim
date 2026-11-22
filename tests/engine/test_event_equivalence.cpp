#include <QtTest>

#include "engine/event/EventEngine.h"

namespace {
QVariantMap component(const QString& id, const QString& type, int width = 1)
{
    return {{"id", id}, {"type", type}, {"bitWidth", width}};
}

QVariantMap wire(const QString& from, const QString& to)
{
    return {{"fromComp", from}, {"fromPort", 0}, {"toComp", to}, {"toPort", 0}};
}

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
    // Per-event observable trace checked against backup/perf-registry-20261122-165721.
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
        {2, "000E", "0000", {}}, // Duplicate wire still consumes an event.
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

    // Z and an unknown character both yield Z in the absence of 0, 1 and E.
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

    // Empty delivered values fall back to the source's latest output, which can
    // differ from the value carried by the current event.
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
