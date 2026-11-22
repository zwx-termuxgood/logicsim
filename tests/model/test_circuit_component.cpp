#include <QtTest>
#include "model/Circuit.h"
#include "logic/ComponentLogic.h"
#include "core/Grid.h"
#include "io/JsonCodec.h"
#include <QJsonDocument>
#include <QTemporaryFile>

// 元件/连线混合选择的模型级回归：覆盖拖动快照、网格位移、端口绑定与撤销重做。
// 每个用例独立创建 Circuit，避免上一操作的选择、撤销栈和几何状态污染下一场景。
class TestcircuitComponent : public QObject {
    Q_OBJECT
    static QVariantMap port(Circuit& c, const QString& id, bool output) {
        const auto comp = c.getComponent(id);
        const auto ports = output ? ComponentLogic::outputPorts(comp) : ComponentLogic::inputPorts(comp);
        const auto p = ports.first().toMap();
        return Grid::point(Grid::xi(comp) + Grid::xi(p), Grid::yi(comp) + Grid::yi(p));
    }
    static QVariantList shifted(QVariantList path, int dx, int dy) {
        for (auto& pv : path) {
            const auto p = pv.toMap();
            pv = Grid::point(Grid::xi(p) + dx, Grid::yi(p) + dy);
        }
        return path;
    }
    static void orthogonal(const QVariantList& path) {
        for (int i = 0; i < path.size(); ++i) {
            const auto p = path[i].toMap();
            QCOMPARE(p.value("x").toDouble(), double(Grid::xi(p)));
            QCOMPARE(p.value("y").toDouble(), double(Grid::yi(p)));
            if (i == 0) continue;
            const auto a = path[i - 1].toMap();
            QVERIFY(Grid::xi(a) == Grid::xi(p) || Grid::yi(a) == Grid::yi(p));
            QVERIFY(a != p);
        }
    }
    // 用文档加载插入任意合法折线，避免 addOrthoWire 自动选路掩盖移动算法的输入形状。
    // 临时文件优先放 PI_SCRATCH_DIR；Qt 在对象析构时清理，不向项目目录写夹具文件。
    static QString insertWire(Circuit& c, const QVariantList& path) {
        const QString id = QString("W%1").arg(c.wires().size() + 1);
        CircuitContext ctx;
        ctx.components = c.components();
        ctx.wires = c.wires();
        ctx.wires.append(QVariantMap{{"id", id}, {"wireType", "ortho"}, {"path", path}});
        const auto data = JsonCodec::encode(ctx, {}, {}, 0, 2, 0, "event", 1, 1, 1000, 3, true, {});
        QTemporaryFile file(qEnvironmentVariable("PI_SCRATCH_DIR", QDir::tempPath()) + "/movement-XXXXXX.json");
        if (!file.open()) return {};
        file.write(QJsonDocument::fromVariant(data).toJson());
        file.flush();
        return c.loadFromFile(file.fileName()) ? id : QString();
    }
private slots:
    void deleteMixedSelectionAndUndo() {
        Circuit c;
        const auto source = c.addComponent("input", 100, 100, 1, 2, "");
        const auto target = c.addComponent("output", 500, 300, 1, 2, "");
        const auto wire = c.addOrthoWire(130, 100, 470, 300, 1);
        const auto outside = c.addOrthoWire(800, 100, 900, 100, 1);
        const auto original = c.getWire(wire);
        c.removeComponents({source, target, wire});
        QVERIFY(c.getComponent(source).isEmpty());
        QVERIFY(c.getComponent(target).isEmpty());
        QVERIFY(c.getWire(wire).isEmpty());
        QVERIFY(!c.getWire(outside).isEmpty());
        c.undo();
        QVERIFY(!c.getComponent(source).isEmpty());
        QVERIFY(!c.getComponent(target).isEmpty());
        QCOMPARE(c.getWire(wire), original);
        c.redo();
        QVERIFY(c.getWire(wire).isEmpty());
        QVERIFY(c.getComponent(source).isEmpty());
        QVERIFY(c.getComponent(target).isEmpty());
        QVERIFY(!c.getWire(outside).isEmpty());
    }
    void deleteWireOnlySelection() {
        Circuit c;
        const auto wire = c.addOrthoWire(100, 100, 300, 200, 1);
        const auto original = c.getWire(wire);
        c.removeComponents({wire});
        QVERIFY(c.getWire(wire).isEmpty());
        c.undo();
        QCOMPARE(c.getWire(wire), original);
    }
    void gestureRoundTrip_data() {
        QTest::addColumn<int>("selection");
        QTest::newRow("component") << 0;
        QTest::newRow("wire-only") << 1;
        QTest::newRow("mixed") << 2;
    }
    // 同一手势多次更新均相对起始快照，回到零偏移必须精确恢复；结束后只产生一次撤销。
    void gestureRoundTrip() {
        QFETCH(int, selection);
        Circuit c;
        const auto from = c.addComponent("input", 0, 0, 1, 2);
        const auto to = c.addComponent("output", 300, 80, 1, 2);
        const auto a = port(c, from, true), b = port(c, to, false);
        const auto wire = c.addOrthoWire(Grid::xi(a), Grid::yi(a), Grid::xi(b), Grid::yi(b), 1);
        const auto before = c.getComponent(from);
        const auto wires = c.wires();
        const QStringList ids = selection == 0 ? QStringList{from} :
                                selection == 1 ? QStringList{wire} : QStringList{from, wire, from};
        QVERIFY(c.beginSelectionMove(ids));
        QVERIFY(c.updateSelectionMove(20, 30));
        const auto first = c.wires();
        if (selection != 0) QCOMPARE(c.getWirePath(wire), shifted(wires.first().toMap().value("path").toList(), 20, 30));
        QVERIFY(c.updateSelectionMove(-40, -20));
        QVERIFY(c.updateSelectionMove(20, 30));
        QCOMPARE(c.wires(), first);
        QVERIFY(c.updateSelectionMove(0, 0));
        QCOMPARE(c.getComponent(from), before);
        QCOMPARE(c.wires(), wires);
        QVERIFY(c.updateSelectionMove(40, 50));
        c.endSelectionMove();
        QVERIFY(!c.updateSelectionMove(60, 60));
        c.undo();
        QCOMPARE(c.getComponent(from), before);
        QCOMPARE(c.wires(), wires);
        c.redo();
        if (selection != 0) QCOMPARE(c.getWirePath(wire), shifted(wires.first().toMap().value("path").toList(), 40, 50));
    }
    void gestureRejectBoundary_data() {
        QTest::addColumn<bool>("wireOnly");
        QTest::newRow("component") << false;
        QTest::newRow("wire-only") << true;
    }
    void gestureRejectBoundary() {
        QFETCH(bool, wireOnly);
        Circuit c;
        const auto id = wireOnly ? insertWire(c, {Grid::point(Grid::MAX_COORD - 100, 0), Grid::point(Grid::MAX_COORD, 0)})
                                 : c.addComponent("input", Grid::MAX_COORD, 0, 1, 2);
        const auto comps = c.components(), wires = c.wires();
        QVERIFY(c.beginSelectionMove({id}));
        QVERIFY(!c.updateSelectionMove(10, 0));
        QCOMPARE(c.components(), comps);
        QCOMPARE(c.wires(), wires);
        QVERIFY(c.updateSelectionMove(-20, 0));
        const auto acceptedComps = c.components(), acceptedWires = c.wires();
        QVERIFY(!c.updateSelectionMove(10, 0));
        QCOMPARE(c.components(), acceptedComps);
        QCOMPARE(c.wires(), acceptedWires);
        QVERIFY(c.updateSelectionMove(0, 0));
        QCOMPARE(c.components(), comps);
        QCOMPARE(c.wires(), wires);
        c.endSelectionMove();
    }
    // 等待超过普通编辑合并窗口，确认手势仍作为整体撤销，而非按停顿拆为多条历史。
    void gestureSingleUndoAfterPause() {
        Circuit c;
        const auto id = insertWire(c, {Grid::point(0, 0), Grid::point(100, 0)});
        QVERIFY(!c.canUndo());
        const auto before = c.getWirePath(id);
        QVERIFY(c.beginSelectionMove({id}));
        QVERIFY(c.updateSelectionMove(20, 30));
        QTest::qWait(550);
        QVERIFY(c.updateSelectionMove(40, 50));
        c.endSelectionMove();
        c.undo();
        QCOMPARE(c.getWirePath(id), before);
        QVERIFY(!c.canUndo());
        c.redo();
        QCOMPARE(c.getWirePath(id), shifted(before, 40, 50));
        QVERIFY(c.beginSelectionMove({id}));
        QVERIFY(c.updateSelectionMove(10, 0));
        c.endSelectionMove();
        c.undo();
        QCOMPARE(c.getWirePath(id), shifted(before, 40, 50));
        c.undo();
        QCOMPARE(c.getWirePath(id), before);
    }
    void gestureInteriorPortRoundTrip() {
        Circuit c;
        const auto from = c.addComponent("input", 0, 0, 1, 2);
        const auto p = port(c, from, true);
        insertWire(c, {Grid::point(Grid::xi(p) - 100, Grid::yi(p)), Grid::point(Grid::xi(p) + 100, Grid::yi(p))});
        const auto wires = c.wires();
        QVERIFY(c.beginSelectionMove({from}));
        QVERIFY(c.updateSelectionMove(20, 30));
        QCOMPARE(c.wires().size(), 2);
        QVERIFY(c.updateSelectionMove(40, 50));
        QCOMPARE(c.wires().size(), 2);
        QVERIFY(c.updateSelectionMove(0, 0));
        QCOMPARE(c.wires(), wires);
        c.endSelectionMove();
    }
    void wireOnly_data() {
        QTest::addColumn<QVariantList>("path");
        QTest::newRow("straight") << QVariantList{Grid::point(0, 0), Grid::point(100, 0)};
        QTest::newRow("L") << QVariantList{Grid::point(0, 0), Grid::point(100, 0), Grid::point(100, 60)};
        QTest::newRow("multibend") << QVariantList{Grid::point(0, 0), Grid::point(100, 0),
            Grid::point(100, 60), Grid::point(160, 60), Grid::point(160, 120)};
    }
    void wireOnly() {
        QFETCH(QVariantList, path);
        Circuit c;
        const auto id = insertWire(c, path);
        QVERIFY(!id.isEmpty());
        const auto before = c.getWirePath(id);
        c.moveSelection({id, id}, 24, -16);
        const auto after = c.getWirePath(id);
        QCOMPARE(after, shifted(before, 20, -20));
        QCOMPARE(c.wires().size(), 1);
        orthogonal(after);
        c.undo();
        QCOMPARE(c.getWirePath(id), before);
        c.redo();
        QCOMPARE(c.getWirePath(id), after);
    }
    void mixedSelectionAndBindings() {
        Circuit c;
        const auto from = c.addComponent("input", 0, 0, 1, 2);
        const auto to = c.addComponent("output", 300, 80, 1, 2);
        const auto a = port(c, from, true), b = port(c, to, false);
        const auto id = c.addOrthoWire(Grid::xi(a), Grid::yi(a), Grid::xi(b), Grid::yi(b), 1);
        const auto before = c.getWirePath(id);
        c.moveSelection({from, id, from}, 20, 30);
        QCOMPARE(c.getWirePath(id), shifted(before, 20, 30));
        QCOMPARE(c.getWire(id).value("fromComp").toString(), from);
        QVERIFY(!c.getWire(id).contains("toComp"));
        orthogonal(c.getWirePath(id));
        c.undo();
        QCOMPARE(c.getWirePath(id), before);
        QCOMPARE(c.getWire(id).value("toComp").toString(), to);
        c.moveSelection({id}, 20, 30);
        QVERIFY(!c.getWire(id).contains("fromComp"));
        QVERIFY(!c.getWire(id).contains("toComp"));
    }
    void twoPointSingleEndpoint_data() {
        QTest::addColumn<bool>("moveOutput");
        QTest::newRow("front") << true;
        QTest::newRow("back") << false;
    }
    void twoPointSingleEndpoint() {
        QFETCH(bool, moveOutput);
        Circuit c;
        const auto from = c.addComponent("input", 0, 0, 1, 2);
        auto a = port(c, from, true);
        const auto to = c.addComponent("output", 300, 0, 1, 2);
        const auto b = port(c, to, false);
        // 创建坐标同行的两个端口，不依赖元件尺寸常量。
        c.moveSelection({to}, 0, Grid::yi(a) - Grid::yi(b));
        const auto end = port(c, to, false);
        const auto id = c.addOrthoWire(Grid::xi(a), Grid::yi(a), Grid::xi(end), Grid::yi(end), 1);
        const auto before = c.getWirePath(id);
        QCOMPARE(before.size(), 2);
        c.moveSelection({moveOutput ? from : to}, 20, 30);
        const auto after = c.getWirePath(id);
        orthogonal(after);
        QCOMPARE(after[moveOutput ? after.size() - 1 : 0], before[moveOutput ? 1 : 0]);
        QCOMPARE(after[moveOutput ? 0 : after.size() - 1].toMap(), port(c, moveOutput ? from : to, moveOutput));
        QCOMPARE(c.getWire(id).value("fromComp").toString(), from);
        QCOMPARE(c.getWire(id).value("toComp").toString(), to);
        c.undo();
        QCOMPARE(c.getWirePath(id), before);
    }
    void bothEndpointsTranslateWholePath() {
        Circuit c;
        const auto from = c.addComponent("input", 0, 0, 1, 2);
        const auto to = c.addComponent("output", 300, 100, 1, 2);
        const auto a = port(c, from, true), b = port(c, to, false);
        const auto id = c.addOrthoWire(Grid::xi(a), Grid::yi(a), Grid::xi(b), Grid::yi(b), 1);
        const auto before = c.getWirePath(id);
        c.moveSelection({from, to}, -20, 30);
        QCOMPARE(c.getWirePath(id), shifted(before, -20, 30));
        orthogonal(c.getWirePath(id));
    }
    void geometricEndpointAndFixedBranch() {
        Circuit c;
        const auto from = c.addComponent("input", 0, 0, 1, 2);
        const auto a = port(c, from, true);
        const auto branch = insertWire(c, {a, Grid::point(Grid::xi(a) + 100, Grid::yi(a)),
            Grid::point(Grid::xi(a) + 100, Grid::yi(a) + 100),
            Grid::point(Grid::xi(a) + 200, Grid::yi(a) + 100)});
        const auto fixed = insertWire(c, {Grid::point(Grid::xi(a) + 200, Grid::yi(a) + 100),
            Grid::point(Grid::xi(a) + 300, Grid::yi(a) + 100)});
        const auto before = c.getWirePath(branch), fixedBefore = c.getWirePath(fixed);
        c.moveSelection({from}, 20, 30);
        const auto after = c.getWirePath(branch);
        orthogonal(after);
        for (int j = 1; j < before.size(); ++j) QVERIFY(after.contains(before[j]));
        QCOMPARE(c.getWirePath(fixed), fixedBefore);
    }
    void interiorPortKeepsTrunk() {
        Circuit c;
        const auto from = c.addComponent("input", 0, 0, 1, 2);
        const auto p = port(c, from, true);
        const auto trunk = insertWire(c, {Grid::point(Grid::xi(p) - 100, Grid::yi(p)),
            Grid::point(Grid::xi(p) + 100, Grid::yi(p))});
        const auto before = c.getWirePath(trunk);
        c.moveSelection({from}, 20, 30);
        QCOMPARE(c.getWirePath(trunk), before);
        QCOMPARE(c.wires().size(), 2);
        const auto branch = c.wires().last().toMap().value("path").toList();
        orthogonal(branch);
        QCOMPARE(branch.first().toMap(), p);
        QCOMPARE(branch.last().toMap(), port(c, from, true));
        c.undo();
        QCOMPARE(c.wires().size(), 1);
    }
    void integerCreationAndRoundTrip() {
        Circuit c;
        const auto id = c.addComponent("input", 1.6, -2.6, 1, 2);
        const auto before = c.getComponent(id);
        QCOMPARE(Grid::xi(before), 2);
        QCOMPARE(Grid::yi(before), -3);
        c.moveSelection({id}, 16, -24);
        QCOMPARE(Grid::xi(c.getComponent(id)), 22);
        QCOMPARE(Grid::yi(c.getComponent(id)), -23);
        c.moveSelection({id}, -16, 24);
        QCOMPARE(c.getComponent(id), before);
        c.moveComponent(id, 18, -27);
        QCOMPARE(Grid::xi(c.getComponent(id)), 22);
        QCOMPARE(Grid::yi(c.getComponent(id)), -23);
    }
};
QTEST_GUILESS_MAIN(TestcircuitComponent)
#include "test_circuit_component.moc"
