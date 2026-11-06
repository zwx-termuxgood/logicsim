#include <QtTest>
#include "core/ValueCodec.h"

class TestValueCodec : public QObject {
    Q_OBJECT
private slots:
    void format_bin_pads_and_truncates() {
        QCOMPARE(ValueCodec::format("101", "bin", 5), QString("00101"));
        QCOMPARE(ValueCodec::format("101010", "bin", 4), QString("1010"));
    }
    void format_hex() {
        QCOMPARE(ValueCodec::format("11111111", "hex", 8), QString("FF"));
        QCOMPARE(ValueCodec::format("11111", "hex", 5), QString("1F"));
    }
    void format_sdec_negative() {
        QCOMPARE(ValueCodec::format("11111111", "sdec", 8), QString("-1"));
        QCOMPARE(ValueCodec::format("10000000", "sdec", 8), QString("-128"));
    }
    void parse_rejects_invalid_bin() {
        QString err;
        ValueCodec::parse("102", "bin", 8, &err);
        QVERIFY(!err.isEmpty());
    }
};
QTEST_GUILESS_MAIN(TestValueCodec)
#include "test_value_codec.moc"
