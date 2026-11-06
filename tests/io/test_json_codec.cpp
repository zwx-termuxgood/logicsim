#include <QtTest>
class TestjsonCodec : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestjsonCodec)
#include "test_json_codec.moc"
