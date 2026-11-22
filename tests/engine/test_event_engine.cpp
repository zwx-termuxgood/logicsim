#include <QtTest>
// 当前为事件引擎测试入口占位；逐事件兼容性回归位于 test_event_equivalence.cpp。
class TesteventEngine : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TesteventEngine)
#include "test_event_engine.moc"
