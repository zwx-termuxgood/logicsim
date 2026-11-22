#include <QtTest>
#include "logic/WireNetworkSolver.h"
#include <limits>

namespace {
QVariantMap point(double x, double y) { return {{"x", x}, {"y", y}}; }
QVariantMap wire(QVariantList path) { return {{"wireType", "ortho"}, {"path", path}}; }
// 测试元件以端口世界位置定位，避免显式绑定掩盖几何接入错误。
QVariantMap source(int x, int y) {
    return {{"id", "src"}, {"type", "input"}, {"bitWidth", 1}, {"x", x - 30}, {"y", y}};
}
QVariantMap sink(int x, int y) {
    return {{"id", "dst"}, {"type", "output"}, {"bitWidth", 1}, {"x", x + 30}, {"y", y}};
}
bool hasDot(const QVariantList& dots, int x, int y) {
    for (const auto& value : dots) {
        const auto p = value.toMap();
        if (p.value("x").toInt() == x && p.value("y").toInt() == y) return true;
    }
    return false;
}
}

class TestWireNetworkSolver : public QObject {
    Q_OBJECT
private slots:
    void topology_data() {
        QTest::addColumn<QVariantList>("components");
        QTest::addColumn<QVariantList>("wires");
        QTest::addColumn<bool>("connected");
        QTest::addColumn<int>("dotCount");
        QTest::addColumn<bool>("centerDot");
        const QVariantList ends{source(-100, 0), sink(0, 100)};
        QTest::newRow("endpoint-T") << ends
            << QVariantList{wire({point(-100, 0), point(100, 0)}),
                            wire({point(0, 0), point(0, 100)})} << true << 1 << true;
        QTest::newRow("corner-to-interior") << QVariantList{source(-100, 0), sink(0, -50)}
            << QVariantList{wire({point(-100, 0), point(100, 0)}),
                            wire({point(0, -50), point(0, 0), point(100, 0)})}
            << true << 1 << true;
        QTest::newRow("pure-X") << ends
            << QVariantList{wire({point(-100, 0), point(100, 0)}),
                            wire({point(0, -100), point(0, 100)})} << false << 0 << false;
        QTest::newRow("explicit-fourway") << ends
            << QVariantList{wire({point(-100, 0), point(0, 0), point(100, 0)}),
                            wire({point(0, -100), point(0, 100)})} << true << 1 << true;
        QTest::newRow("port-to-interior") << QVariantList{source(-100, 0), sink(0, 0)}
            << QVariantList{wire({point(-100, 0), point(100, 0)})} << true << 1 << true;
        QTest::newRow("port-makes-X-explicit") << QVariantList{source(-100, 0), sink(0, 0)}
            << QVariantList{wire({point(-100, 0), point(100, 0)}),
                            wire({point(0, -100), point(0, 100)})} << true << 1 << true;
        QTest::newRow("collinear-overlap") << QVariantList{source(-100, 0), sink(100, 0)}
            << QVariantList{wire({point(-100, 0), point(50, 0)}),
                            wire({point(-50, 0), point(100, 0)})} << true << 0 << false;
        QTest::newRow("duplicates-and-zero-length") << QVariantList{source(-100, 0), sink(100, 0)}
            << QVariantList{wire({point(-100, 0), point(0, 0), point(100, 0)}),
                            wire({point(100, 0), point(0, 0), point(0, 0), point(-100, 0)}),
                            wire({point(-50, 0), point(50, 0)})} << true << 0 << false;
        QTest::newRow("plain-corner") << ends
            << QVariantList{wire({point(-100, 0), point(0, 0), point(0, 100)})}
            << true << 0 << false;
        QTest::newRow("endpoint-to-endpoint") << QVariantList{source(-100, 0), sink(100, 0)}
            << QVariantList{wire({point(-100, 0), point(0, 0)}),
                            wire({point(0, 0), point(100, 0)})} << true << 0 << false;
        QTest::newRow("adjacent-integer-row-is-separate") << ends
            << QVariantList{wire({point(-100, 0), point(100, 0)}),
                            wire({point(0, 1), point(0, 100)})} << false << 0 << false;
    }
    void topology() {
        QFETCH(QVariantList, components); QFETCH(QVariantList, wires);
        QFETCH(bool, connected); QFETCH(int, dotCount); QFETCH(bool, centerDot);
        const auto virtuals = WireNetworkSolver::generateVirtualWires(components, wires);
        QCOMPARE(virtuals.size(), connected ? 1 : 0);
        if (connected) {
            const auto edge = virtuals.first().toMap();
            QCOMPARE(edge.value("fromComp").toString(), QString("src"));
            QCOMPARE(edge.value("toComp").toString(), QString("dst"));
        }
        const auto dots = WireNetworkSolver::connectionDots(components, wires);
        QCOMPARE(dots.size(), dotCount);
        QCOMPARE(hasDot(dots, 0, 0), centerDot);
        for (const auto& value : dots) {
            QCOMPARE(value.toMap().value("x").metaType().id(), int(QMetaType::Int));
            QCOMPARE(value.toMap().value("y").metaType().id(), int(QMetaType::Int));
        }
    }
    void bindingsAndBezier() {
        const QVariantList components{source(-100, 0), sink(100, 0)};
        QVariantMap bound{{"wireType", "bezier"}, {"fromComp", "src"}, {"fromPort", 0},
                          {"toComp", "dst"}, {"toPort", 0}};
        QCOMPARE(WireNetworkSolver::generateVirtualWires(components, {bound}).size(), 1);
        bound["wireType"] = "ortho";
        bound["path"] = QVariantList{point(1000, 1000), point(1100, 1000)};
        QCOMPARE(WireNetworkSolver::generateVirtualWires(components, {bound}).size(), 1);
        bound["wireType"] = "virtual";
        QVERIFY(WireNetworkSolver::generateVirtualWires(components, {bound}).isEmpty());
    }
    void invalidCoordinatesDoNotConnect() {
        const QVariantList components{source(-100, 0), sink(100, 0)};
        for (double invalid : {std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN(), 1e30}) {
            const QVariantList wires{wire({point(-100, 0), point(invalid, 0), point(100, 0)})};
            QVERIFY(WireNetworkSolver::generateVirtualWires(components, wires).isEmpty());
            QVERIFY(WireNetworkSolver::connectionDots(components, wires).isEmpty());
        }
    }
    void negativeFractionalLegacyCoordinates() {
        // 半值远离零：横线归一到 y=-1，端口及接入顶点采用相同整数位置。
        QVariantList components{source(-100, -1), sink(0, 100)};
        QVariantList wires{wire({point(-100.4, -0.5), point(100.4, -0.5)}),
                           wire({point(-0.4, -0.5), point(-0.4, 100.4)})};
        QCOMPARE(WireNetworkSolver::generateVirtualWires(components, wires).size(), 1);
        const auto dots = WireNetworkSolver::connectionDots(components, wires);
        QCOMPARE(dots.size(), 1);
        QVERIFY(hasDot(dots, 0, -1));
    }
};
QTEST_GUILESS_MAIN(TestWireNetworkSolver)
#include "test_wire_network_solver.moc"
