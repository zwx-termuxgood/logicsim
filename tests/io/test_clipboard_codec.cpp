#include <QtTest>
class TestclipboardCodec : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(TestclipboardCodec)
#include "test_clipboard_codec.moc"
