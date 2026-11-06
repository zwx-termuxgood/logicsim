#include <QtTest>
class TestcircuitDocument : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestcircuitDocument)
#include "test_circuit_document.moc"
