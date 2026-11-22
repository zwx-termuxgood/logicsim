#include <QtTest>
// 当前为文档模型测试入口占位，尚未验证文件保存、加载或文档状态变更。
class TestcircuitDocument : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestcircuitDocument)
#include "test_circuit_document.moc"
