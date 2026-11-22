#include <QtTest>
// 当前为撤销测试入口占位；实际编辑往返断言分布在元件移动与连线测试中。
class TestcircuitUndo : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestcircuitUndo)
#include "test_circuit_undo.moc"
