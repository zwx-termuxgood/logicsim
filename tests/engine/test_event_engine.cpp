#include <QtTest>
class TesteventEngine : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TesteventEngine)
#include "test_event_engine.moc"
