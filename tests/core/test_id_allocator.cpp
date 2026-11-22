#include <QtTest>
// 当前为测试入口占位：尚未验证 ID 分配、回收或碰撞处理，不应视为功能覆盖。
class TestidAllocator : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestidAllocator)
#include "test_id_allocator.moc"
