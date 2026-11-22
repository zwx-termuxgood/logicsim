#include <QtTest>
#include "model/Circuit.h"
#include "helpers/TestHelpers.h"
#include <cmath>

class TestCircuitWire : public QObject {
    Q_OBJECT

private slots:
    // 辅助函数：检查整条线是否绝对正交（无斜线）
    void assertStrictlyOrthogonal(const QVariantList& path, const QString& msg = "") {
        for (int i = 0; i < path.size() - 1; ++i) {
            double x0 = path[i].toMap()["x"].toDouble();
            double y0 = path[i].toMap()["y"].toDouble();
            double x1 = path[i + 1].toMap()["x"].toDouble();
            double y1 = path[i + 1].toMap()["y"].toDouble();
            bool isHoriz = qAbs(y0 - y1) < 0.1;
            bool isVert  = qAbs(x0 - x1) < 0.1;
            QVERIFY2(isHoriz || isVert, qPrintable(
                                            QString("【斜线 BUG】%1: (%2,%3) -> (%4,%5)").arg(msg).arg(x0).arg(y0).arg(x1).arg(y1)
                                            ));
        }
    }

    // ================================================================
    // 1. 基础功能
    // ================================================================
    void addOrthoDirectional() {
        Circuit c;
        QString c1 = c.addComponent("input", 100, 100, 1, 2, "");
        QString c2 = c.addComponent("input", 200, 200, 1, 2, "");
        QString w1 = c.addOrthoWire(130, 100, 170, 200, 1); // 横优先
        QVariantList p1 = c.getWirePath(w1);
        QCOMPARE(p1.size(), 3);
        QCOMPARE(p1[1].toMap()["x"].toDouble(), 170.0);
        QCOMPARE(p1[1].toMap()["y"].toDouble(), 100.0);
        assertStrictlyOrthogonal(p1, "基础横优先");
    }

    void addOrthoReverse() {
        Circuit c;
        QString c1 = c.addComponent("input", 100, 100, 1, 2, "");
        QString c2 = c.addComponent("output", 200, 100, 1, 2, "");
        QString w = c.addOrthoWire(170, 100, 130, 100, 1); // 反向画线
        QVariantMap wire = c.getWire(w);
        QCOMPARE(wire["fromComp"].toString(), c1);
        QCOMPARE(wire["toComp"].toString(), c2);
    }

    void crossingWires() {
        Circuit c;
        QString w1 = c.addOrthoWire(100, 200, 300, 200, 1);
        QString w2 = c.addOrthoWire(200, 100, 200, 300, 2);
        assertStrictlyOrthogonal(c.getWirePath(w1));
        assertStrictlyOrthogonal(c.getWirePath(w2));
    }

    // ================================================================
    // 2. 已知 BUG 测试（这些用例会抓出源码设计缺陷）
    // ================================================================
    void addOrtho_NoGridSnapping() {
        Circuit c;
        QString w = c.addOrthoWire(13.4, 27.8, 125.6, 100.1, 1);
        QVariantList path = c.getWirePath(w);

        bool hasDecimal = false;
        for (auto pv : path) {
            double x = pv.toMap()["x"].toDouble();
            double y = pv.toMap()["y"].toDouble();
            if (std::abs(x - std::round(x)) > 0.01 || std::abs(y - std::round(y)) > 0.01) {
                hasDecimal = true;
            }
        }
        QVERIFY2(!hasDecimal, "【设计 BUG】addOrthoWire 未对坐标进行网格化，导致后续正交逻辑失效");
    }

    void addOrtho_TinyDifferenceCreatesCorner() {
        Circuit c;
        QString w = c.addOrthoWire(100, 100, 100.6, 200, 1);
        QVariantList path = c.getWirePath(w);
        QVERIFY2(path.size() == 2, "【视觉 BUG】0.6px 的微小偏差导致生成了无效的微小拐点");
    }

    void dragCornerMustNotProduceDiagonal() {
        Circuit c;
        QString c1 = c.addComponent("input", 100, 100, 1, 2, "");
        QString c2 = c.addComponent("input", 200, 200, 1, 2, "");
        QString w = c.addOrthoWire(130, 100, 170, 200, 1);
        c.setWirePathPoint(w, 1, 80, 250); // 极值拖拽
        QVariantList path = c.getWirePath(w);
        assertStrictlyOrthogonal(path, "拖动极值拐点");
    }

    void shortenPreservesOriginalBends() {
        Circuit c;
        QString w = c.addOrthoWire(130, 100, 500, 500, 1);
        const auto original = c.getWirePath(w);
        QVERIFY(!c.shortenWireEnd(w, true, 315, 315));
        QCOMPARE(c.getWirePath(w), original);
        QVERIFY(c.shortenWireEnd(w, true, 315, 100));
        auto path = c.getWirePath(w);
        QCOMPARE(path.size(), 3);
        QCOMPARE(path[0].toMap()["x"].toInt(), 315);
        QCOMPARE(path[1], original[1]);
        QCOMPARE(path.last(), original.last());
        QVERIFY(c.shortenWireEnd(w, false, 500, 300));
        QCOMPARE(c.getWirePath(w).first(), path.first());
        QCOMPARE(c.getWirePath(w).last().toMap()["y"].toInt(), 300);
        c.breakUndoMerge();
        QVERIFY(c.shortenWireEnd(w, true, 500, 300));
        QVERIFY(c.getWire(w).isEmpty());
        c.undo();
        QVERIFY(!c.getWire(w).isEmpty());
    }

    void shortenReversedPathKeepsUntouchedBinding() {
        for (bool front : {true, false}) {
            Circuit c;
            const QString source = c.addComponent("input", 100, 100, 1, 2, "");
            const QString target = c.addComponent("output", 500, 100, 1, 2, "");
            const QString wire = c.addOrthoWire(130, 100, 470, 100, 1);
            // 显式构造反向存储的合法路径，不依赖移动实现是否重排端点。
            auto document = c.toJson();
            auto payload = document.value("root").toMap();
            auto wires = payload.value("wires").toList();
            for (auto& value : wires) {
                auto item = value.toMap();
                if (item.value("id").toString() != wire) continue;
                auto points = item.value("path").toList();
                std::reverse(points.begin(), points.end());
                item["path"] = points;
                value = item;
            }
            payload["wires"] = wires;
            document["root"] = payload;
            QVERIFY(c.fromJson(document));
            const QString reversedWire = wire;
            const auto path = c.getWirePath(reversedWire);
            QCOMPARE(path.first().toMap().value("x").toInt(), 470);
            QCOMPARE(path.last().toMap().value("x").toInt(), 130);
            QVERIFY(c.shortenWireEnd(reversedWire, front, 300, 100));
            const auto shortened = c.getWire(reversedWire);
            QCOMPARE(shortened.contains("fromComp"), front);
            QCOMPARE(shortened.contains("fromPort"), front);
            QCOMPARE(shortened.contains("toComp"), !front);
            QCOMPARE(shortened.contains("toPort"), !front);
            QCOMPARE(c.getWirePath(reversedWire)[front ? 1 : 0], path[front ? 1 : 0]);
        }
    }

    void removeComponent_OverzealousTeeJunction() {
        Circuit c;
        QString c1 = c.addComponent("input", 100, 100, 1, 2, "");
        // 距离端口 14px 的无关线
        QString w = c.addOrthoWire(144, 100, 300, 100, 1);
        c.removeComponent(c1);
        QVERIFY2(!c.getWire(w).isEmpty(), "【严重逻辑 BUG】离端口 14px 的无关线被误删，15px 容差过大");
    }

    void moveComponent_BacktrackingSegment() {
        Circuit c;
        QString c1 = c.addComponent("input", 100, 100, 1, 2, "");
        QString c2 = c.addComponent("output", 500, 100, 1, 2, "");
        QString w = c.addOrthoWire(130, 100, 470, 100, 1);
        c.moveComponent(c1, 600, 100); // 把左边拖到右边
        QVariantList path = c.getWirePath(w);
        QVERIFY(path.size() >= 2);
        // 首尾顺序可以反向；真正的回折是连续共线段方向发生反转。
        QCOMPARE(path.first().toMap()["x"].toInt(), 630);
        QCOMPARE(path.last().toMap()["x"].toInt(), 470);
        for (int i = 1; i < path.size(); ++i) {
            const auto a = path[i - 1].toMap(), b = path[i].toMap();
            QVERIFY(a["x"].toInt() == b["x"].toInt() || a["y"].toInt() == b["y"].toInt());
            if (i < 2) continue;
            const auto p = path[i - 2].toMap();
            const qint64 ax = a["x"].toInt() - p["x"].toInt();
            const qint64 ay = a["y"].toInt() - p["y"].toInt();
            const qint64 bx = b["x"].toInt() - a["x"].toInt();
            const qint64 by = b["y"].toInt() - a["y"].toInt();
            QVERIFY2(ax * bx + ay * by >= 0, "拖动后出现反向自折叠");
        }
    }

    void tJunction_LooseConnection() {
        Circuit c;
        QString w1 = c.addOrthoWire(100, 100, 300, 100, 1);
        QString w2 = c.addOrthoWire(198, 102, 198, 250, 2); // 偏了 2px
        QVariantList p2 = c.getWirePath(w2);
        QVERIFY2(qAbs(p2[0].toMap()["y"].toDouble() - 100) < 0.1, "【网络 BUG】T 型引出起点未严格吸附原线，导致网络断裂");
    }

    // ================================================================
    // 3. 你抓到的致命 BUG：复制粘贴带有正交折线的电路，移动后端点脱钩
    // ================================================================
    void pasteAndMoveOrthoWire() {
        Circuit c;

        // 1. 准备原始电路
        QString c1 = c.addComponent("input", 100, 100, 1, 2, "");
        QString c2 = c.addComponent("output", 300, 300, 1, 2, "");
        QString w1 = c.addOrthoWire(130, 100, 270, 300, 1);
        c.breakUndoMerge();

        // 2. 选中并复制粘贴
        QStringList selected = { c1, c2, w1 };
        QVariantList clipData = c.copySelection(selected);
        QStringList newIds = c.pasteSelection(clipData, 500, 0);
        QCOMPARE(newIds.size(), 3);

        QString newC1 = newIds[0];
        QString newW1 = newIds[2];

        // 3. 移动粘贴出来的新输入元件
        c.moveComponent(newC1, 600, 200);

        // 4. 检查连线是否跟随
        QVariantList path = c.getWirePath(newW1);
        QVERIFY(path.size() >= 2);

        QVariantMap p0 = path[0].toMap();
        // 【致命 BUG 暴露点】如果 p0 的 y 依然是 100，说明线脱钩了！
        QCOMPARE(p0["y"].toDouble(), 200.0);

        // 还要验证拓扑连接是否被保留
        QVariantMap movedWire = c.getWire(newW1);
        QCOMPARE(movedWire["fromComp"].toString(), newC1);
    }
};

QTEST_GUILESS_MAIN(TestCircuitWire)
#include "test_circuit_wire.moc"