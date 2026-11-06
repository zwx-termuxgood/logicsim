#include <QtTest>
class TestcircuitUndo : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestcircuitUndo)
#include "test_circuit_undo.moc"
