#include <QtTest>
class Testconnector : public QObject {
    Q_OBJECT
private slots:
    void placeholder() { QCOMPARE(1 + 1, 2); }
};
QTEST_GUILESS_MAIN(Testconnector)
#include "test_connector.moc"
