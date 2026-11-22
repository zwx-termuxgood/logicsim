#include <QtTest>
// 当前为测试入口占位：仅验证 Qt Test 用例能运行，尚未覆盖 CircuitContext 行为。
class TestcircuitContext : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestcircuitContext)
#include "test_circuit_context.moc"
