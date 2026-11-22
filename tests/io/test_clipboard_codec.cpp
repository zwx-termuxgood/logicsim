#include <QtTest>
#include "io/ClipboardCodec.h"
#include "core/Grid.h"
#include <limits>

class TestclipboardCodec : public QObject {
    Q_OBJECT
private slots:
    void copyPasteNormalizesFractionalDiagonalAndTGeometry() {
        CircuitContext source;
        source.wires.append(QVariantMap{{"id", "W1"}, {"wireType", "ortho"}, {"path", QVariantList{QVariantMap{{"x", -10.4}, {"y", 0.2}}, QVariantMap{{"x", 20.3}, {"y", 0.2}}}}});
        source.wires.append(QVariantMap{{"id", "W2"}, {"wireType", "ortho"}, {"path", QVariantList{QVariantMap{{"x", 5.2}, {"y", -10.4}}, QVariantMap{{"x", 5.2}, {"y", 0.2}}}}});
        source.wires.append(QVariantMap{{"id", "W3"}, {"wireType", "ortho"}, {"path", QVariantList{QVariantMap{{"x", -10.2}, {"y", -4.7}}, QVariantMap{{"x", 0.4}, {"y", -4.7}}, QVariantMap{{"x", 5.1}, {"y", 2.8}}}}});
        const auto encoded = ClipboardCodec::encode(source, {"W1", "W2", "W3"});
        CircuitContext target;
        const auto pasted = ClipboardCodec::paste(target, encoded, -2.5, 3.5);
        QCOMPARE(pasted.newIds.size(), 3);
        QCOMPARE(target.wires.size(), 3);
        for (const auto& wv : target.wires) {
            const auto path = wv.toMap().value("path").toList();
            for (const auto& pv : path) {
                const auto p = pv.toMap();
                QCOMPARE(p.value("x").toDouble(), double(Grid::xi(p)));
                QCOMPARE(p.value("y").toDouble(), double(Grid::yi(p)));
            }
            for (int i = 0; i + 1 < path.size(); ++i) {
                const auto a = path[i].toMap(), b = path[i + 1].toMap();
                QVERIFY(a.value("x").toInt() == b.value("x").toInt() || a.value("y").toInt() == b.value("y").toInt());
            }
        }
    }
    void integerOverlapAndTDoNotJoinPureCrossing() {
        CircuitContext source;
        source.components.append(QVariantMap{{"id", "C1"}, {"type", "text"}, {"x", 0}, {"y", 0}});
        auto wire = [](const QString& id, int x1, int y1, int x2, int y2) {
            return QVariantMap{{"id", id}, {"wireType", "ortho"}, {"path", QVariantList{Grid::point(x1, y1), Grid::point(x2, y2)}}};
        };
        source.wires.append(wire("W1", 0, 0, 20, 0));
        source.wires.append(wire("W2", 10, 0, 30, 0));
        source.wires.append(wire("W3", 25, 0, 25, 10));
        source.wires.append(wire("W4", 15, -10, 15, 10));
        const auto copied = ClipboardCodec::encode(source, {"C1", "W1"}).first().toMap();
        QCOMPARE(copied.value("wires").toList().size(), 3);
        QSet<QString> ids;
        for (const auto& wv : copied.value("wires").toList()) ids.insert(wv.toMap().value("id").toString());
        QVERIFY(ids.contains("W2"));
        QVERIFY(ids.contains("W3"));
        QVERIFY(!ids.contains("W4"));
    }
    void rejectsInvalidPasteOffset() {
        CircuitContext target;
        QVERIFY(ClipboardCodec::paste(target, {QVariantMap{}}, std::numeric_limits<double>::infinity(), 0).newIds.isEmpty());
        QVERIFY(target.components.isEmpty());
    }
};
QTEST_GUILESS_MAIN(TestclipboardCodec)
#include "test_clipboard_codec.moc"
