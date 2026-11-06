#include <QtTest>
class TestidAllocator : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestidAllocator)
#include "test_id_allocator.moc"
