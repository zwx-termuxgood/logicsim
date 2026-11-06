#include <QtTest>
class TestiterEngine : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestiterEngine)
#include "test_iter_engine.moc"
