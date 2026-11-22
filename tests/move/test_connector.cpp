#include <QtTest>
#include "move/MoveGesture.h"
#include "core/CircuitContext.h"
#include "core/Grid.h"
#include "logic/ComponentLogic.h"

class Testconnector : public QObject {
    Q_OBJECT
private slots:
    // 夹具包含穿过端口的主干、主干下游线和相距一像素的平行线。
    // 移动连接器只收集直接触及端口的线，不沿网络扩散，也不以邻近容差误选。
    void findsOnlyWiresTouchingMovedPort() {
        const QVariantMap component{{"id", "C1"}, {"type", "input"}, {"x", 0}, {"y", 0}, {"bitWidth", 1}};
        const auto port = ComponentLogic::outputPorts(component).first().toMap();
        const int x = Grid::xi(port), y = Grid::yi(port);
        const QVariantMap trunk{{"id", "W1"}, {"wireType", "ortho"},
            {"path", QVariantList{Grid::point(x - 101, y), Grid::point(x + 101, y)}}};
        const QVariantMap downstream{{"id", "W2"}, {"wireType", "ortho"},
            {"path", QVariantList{Grid::point(x + 101, y), Grid::point(x + 101, y + 80)}}};
        const QVariantMap adjacent{{"id", "W3"}, {"wireType", "ortho"},
            {"path", QVariantList{Grid::point(x - 101, y + 1), Grid::point(x + 101, y + 1)}}};
        CircuitContext ctx;
        ctx.components = {component};
        ctx.wires = {trunk, downstream, adjacent};
        MoveGesture gesture(&ctx, {component});
        const auto& connections = gesture.getConnections();
        QCOMPARE(connections.size(), 1);
        QCOMPARE(connections.first().location(), Grid::point(x, y));
        QCOMPARE(connections.first().wirePath().size(), 1);
        QCOMPARE(connections.first().wirePath().first(), trunk);
    }
};
QTEST_GUILESS_MAIN(Testconnector)
#include "test_connector.moc"
