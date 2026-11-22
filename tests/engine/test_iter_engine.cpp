#include <QtTest>
// 当前为迭代引擎测试入口占位，算术断言未覆盖电路求值或收敛行为。
class TestiterEngine : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestiterEngine)
#include "test_iter_engine.moc"
