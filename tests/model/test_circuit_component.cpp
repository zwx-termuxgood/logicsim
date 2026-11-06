#include <QtTest>
class TestcircuitComponent : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestcircuitComponent)
#include "test_circuit_component.moc"
