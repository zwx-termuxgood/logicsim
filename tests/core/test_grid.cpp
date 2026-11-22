#include <QtTest>
#include "core/Grid.h"
#include <limits>

class TestGrid : public QObject {
    Q_OBJECT
private slots:
    void integerBoundaries() {
        QCOMPARE(Grid::snap(-12.5), -13);
        QCOMPARE(Grid::snapStep(-15), -20);
        QVERIFY(!Grid::valid(std::numeric_limits<double>::infinity()));
        QVERIFY(!Grid::valid(std::numeric_limits<double>::quiet_NaN()));
        QVERIFY(!Grid::valid(1000000001.0));
        QVERIFY(Grid::onSegment(-15, -20, -30, -20, 10, -20));
        QVERIFY(!Grid::onSegment(-15, -19, -30, -20, 10, -20));
    }
    void clippingPreservesRoute() {
        QVariantList p{Grid::point(0, 0), Grid::point(100, 0),
                       Grid::point(100, 100), Grid::point(10, 100), Grid::point(10, 10)};
        QCOMPARE(Grid::clippedPath(p, true, 50, 0),
                 (QVariantList{Grid::point(50, 0), p[1], p[2], p[3], p[4]}));
        QCOMPARE(Grid::clippedPath(p, false, 10, 50),
                 (QVariantList{p[0], p[1], p[2], p[3], Grid::point(10, 50)}));
        QVERIFY(Grid::clippedPath(p, true, 50, 50).isEmpty());
        QCOMPARE(Grid::clippedPath(p, true, 10, 10).size(), 1);
    }
    void normalizationIsStable() {
        QVariantList legacy{Grid::point(0, 0), Grid::point(30, 40),
                            Grid::point(30, 40), Grid::point(-20, 40)};
        auto p = Grid::normalizePath(legacy);
        QCOMPARE(Grid::normalizePath(p), p);
        for (int i = 1; i < p.size(); ++i) {
            auto a = p[i - 1].toMap(), b = p[i].toMap();
            QVERIFY(Grid::xi(a) == Grid::xi(b) || Grid::yi(a) == Grid::yi(b));
        }
    }
    void normalizationPreservesNodesAndExtrema() {
        QVariantList junction{Grid::point(-100, 0), Grid::point(0, 0), Grid::point(100, 0)};
        QCOMPARE(Grid::normalizePath(junction), junction);
        QVariantList backtrack{Grid::point(0, 0), Grid::point(100, 0), Grid::point(50, 0)};
        QCOMPARE(Grid::normalizePath(backtrack), backtrack);
        QVariantList crossing{Grid::point(0, 0), Grid::point(100, 0), Grid::point(100, 100),
                              Grid::point(50, 100), Grid::point(50, -100)};
        QCOMPARE(Grid::clippedPath(crossing, false, 50, 0),
                 (QVariantList{crossing[0], crossing[1], crossing[2], crossing[3], Grid::point(50, 0)}));
        QCOMPARE(Grid::clippedPath(crossing, true, 50, 0),
                 (QVariantList{Grid::point(50, 0), crossing[1], crossing[2], crossing[3], crossing[4]}));
    }
    void largeProjection() {
        auto p = Grid::projectOnPath(-Grid::MAX_COORD, -Grid::MAX_COORD,
                                    {Grid::point(Grid::MAX_COORD, Grid::MAX_COORD),
                                     Grid::point(Grid::MAX_COORD, Grid::MAX_COORD - 1)});
        QVERIFY(p["distSq"].toLongLong() > 0);
    }
};
QTEST_GUILESS_MAIN(TestGrid)
#include "test_grid.moc"
