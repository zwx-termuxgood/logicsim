#include <QtTest>
class TestwireNetworkSolver : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestwireNetworkSolver)
#include "test_wire_network_solver.moc"
