#include <QtTest>
#include "io/JsonCodec.h"
#include "core/Grid.h"
#include "logic/ComponentLogic.h"
#include <limits>

// 文档迁移夹具直接构造旧浮点/斜线路径，通过编码再解码验证整数化与正交化。
// 同时检查 ID、显式端口绑定、反向路径端点和重复往返稳定性，而非只检查外观。
class TestjsonCodec : public QObject {
    Q_OBJECT
private slots:
    void migratesFractionalNegativeAndDiagonalGeometry() {
        CircuitContext root;
        root.components.append(QVariantMap{{"id", "C1"}, {"type", "input"}, {"x", -12.6}, {"y", 9.5}});
        root.wires.append(QVariantMap{{"id", "W1"}, {"wireType", "ortho"}, {"path", QVariantList{QVariantMap{{"x", -12.6}, {"y", 9.5}}, QVariantMap{{"x", 4.4}, {"y", 9.5}}, QVariantMap{{"x", 13.2}, {"y", 18.1}}}}});
        const auto encoded = JsonCodec::encode(root, {}, {}, 0, 2, 0, "event", 1, 1, 1000, 3, true, {});
        const auto decoded = JsonCodec::decode(encoded);
        QCOMPARE(decoded.root.components.first().toMap().value("x").toInt(), -13);
        QCOMPARE(decoded.root.components.first().toMap().value("y").toInt(), 10);
        const auto path = decoded.root.wires.first().toMap().value("path").toList();
        // 原有共线顶点可能是明确接入节点，迁移不得将其抹除。
        QCOMPARE(path, QVariantList({Grid::point(-13, 10), Grid::point(4, 10), Grid::point(13, 10), Grid::point(13, 18)}));
        QCOMPARE(JsonCodec::decode(JsonCodec::encode(decoded.root, {}, {}, 0, 2, 0, "event", 1, 1, 1000, 3, true, {})).root.wires, decoded.root.wires);
        QCOMPARE(decoded.root.wires.first().toMap().value("id").toString(), QString("W1"));
    }
    void alignsExplicitBindingsToAuthoritativePorts() {
        CircuitContext root;
        const QVariantMap comp{{"id", "C1"}, {"type", "input"}, {"x", -12.6}, {"y", 9.5}, {"bitWidth", 1}};
        root.components.append(comp);
        root.wires.append(QVariantMap{{"id", "W1"}, {"wireType", "ortho"}, {"fromComp", "C1"}, {"fromPort", 0}, {"path", QVariantList{Grid::point(0, 0), Grid::point(100, 40)}}});
        const auto encoded = JsonCodec::encode(root, {}, {}, 0, 2, 0, "event", 1, 1, 1000, 3, true, {});
        const auto decoded = JsonCodec::decode(encoded);
        const auto c = decoded.root.components.first().toMap();
        const auto port = ComponentLogic::outputPorts(c).first().toMap();
        const auto wire = decoded.root.wires.first().toMap();
        const auto path = wire.value("path").toList();
        QCOMPARE(path.first().toMap(), Grid::point(Grid::xi(c) + Grid::xi(port), Grid::yi(c) + Grid::yi(port)));
        QCOMPARE(wire.value("fromComp").toString(), QString("C1"));
        QCOMPARE(path, Grid::normalizePath(path));
    }
    void preservesBindingsOnReversedPath() {
        CircuitContext root;
        const QVariantMap output{{"id", "O"}, {"type", "input"}, {"x", 100}, {"y", 100}, {"bitWidth", 1}};
        const QVariantMap input{{"id", "I"}, {"type", "output"}, {"x", 300}, {"y", 100}, {"bitWidth", 1}};
        root.components = {output, input};
        const auto outPort = ComponentLogic::outputPorts(output).first().toMap();
        const auto inPort = ComponentLogic::inputPorts(input).first().toMap();
        const auto outPoint = Grid::point(100 + Grid::xi(outPort), 100 + Grid::yi(outPort));
        const auto inPoint = Grid::point(300 + Grid::xi(inPort), 100 + Grid::yi(inPort));
        root.wires.append(QVariantMap{{"id", "W"}, {"wireType", "ortho"}, {"fromComp", "O"}, {"fromPort", 0}, {"toComp", "I"}, {"toPort", 0}, {"path", QVariantList{inPoint, Grid::point(200, 150), outPoint}}});
        const auto decoded = JsonCodec::decode(JsonCodec::encode(root, {}, {}, 0, 2, 0, "event", 1, 1, 1000, 3, true, {}));
        const auto wire = decoded.root.wires.first().toMap();
        const auto path = wire.value("path").toList();
        QCOMPARE(path.first(), inPoint);
        QCOMPARE(path.last(), outPoint);
        QCOMPARE(wire.value("fromComp").toString(), QString("O"));
        QCOMPARE(wire.value("toComp").toString(), QString("I"));
        QCOMPARE(JsonCodec::decode(JsonCodec::encode(decoded.root, {}, {}, 0, 2, 0, "event", 1, 1, 1000, 3, true, {})).root.wires, decoded.root.wires);
        // 旧浮点端点在量化后偏离端口一像素，仍应在原来的反向端对齐。
        auto legacy = root.wires.first().toMap();
        legacy["path"] = QVariantList{QVariantMap{{"x", Grid::xi(inPoint) + 0.6}, {"y", 100.0}}, Grid::point(200, 100), QVariantMap{{"x", Grid::xi(outPoint) - 0.6}, {"y", 100.0}}};
        root.wires = {legacy};
        const auto migrated = JsonCodec::decode(JsonCodec::encode(root, {}, {}, 0, 2, 0, "event", 1, 1, 1000, 3, true, {})).root.wires.first().toMap().value("path").toList();
        QCOMPARE(migrated, QVariantList({inPoint, Grid::point(200, 100), outPoint}));
        for (const auto& point : migrated) {
            QCOMPARE(point.toMap().value("x").metaType().id(), int(QMetaType::Int));
            QCOMPARE(point.toMap().value("y").metaType().id(), int(QMetaType::Int));
        }
    }
    void rejectsNonFiniteCoordinates() {
        QVariantMap data{{"root", QVariantMap{{"components", QVariantList{QVariantMap{{"id", "C"}, {"x", std::numeric_limits<double>::infinity()}, {"y", 0}}}}}}};
        QVERIFY(JsonCodec::decode(data).root.components.isEmpty());
    }
};
QTEST_GUILESS_MAIN(TestjsonCodec)
#include "test_json_codec.moc"
