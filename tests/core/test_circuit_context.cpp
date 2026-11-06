#include <QtTest>
class TestcircuitContext : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestcircuitContext)
#include "test_circuit_context.moc"
