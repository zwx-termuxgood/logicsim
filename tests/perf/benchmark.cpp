#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include "engine/event/EventEngine.h"
#include "engine/iter/IterEngine.h"
#include "logic/WireNetworkSolver.h"

namespace {
QByteArray json(const QVariant& value) {
    return QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact);
}
QByteArray canonicalList(const QVariantList& values) {
    QList<QByteArray> rows;
    for (const auto& value : values) rows.append(json(value));
    std::sort(rows.begin(), rows.end());
    QByteArray result;
    for (const auto& row : rows) result += row + '\n';
    return result;
}
QVariantMap component(const QString& id, const QString& type, int x, int y) {
    return {{"id", id}, {"type", type}, {"bitWidth", 8}, {"inputCount", 1},
            {"x", x}, {"y", y}, {"inputBits", QVariantList{0,0,0,0,0,0,0,0}}};
}
CircuitContext circuit(int size) {
    CircuitContext ctx;
    ctx.components.append(component("src", "input", -1000, -1000));
    // Many independent short chains stress fanout, dirty tracking and dispatch.
    for (int i = 0; i < size; ++i) {
        const QString id = QString("g%1").arg(i);
        ctx.components.append(component(id, "not", 200 * (i % 8), 200 * (i / 8)));
        const QString from = (i % 8 == 0) ? QString("src") : QString("g%1").arg(i - 1);
        ctx.wires.append(QVariantMap{{"wireType", "bezier"}, {"fromComp", from},
            {"fromPort", 0}, {"toComp", id}, {"toPort", 0}});
    }
    return ctx;
}
QVariantMap point(int x, int y) { return {{"x", x}, {"y", y}}; }
CircuitContext topology(int size) {
    CircuitContext ctx;
    for (int i = 0; i < size; ++i) {
        const int y = i * 200;
        auto source = component(QString("s%1").arg(i), "input", -130, y);
        auto sink = component(QString("d%1").arg(i), "output", 30, y + 100);
        source["bitWidth"] = 1; sink["bitWidth"] = 1;
        ctx.components.append(source); ctx.components.append(sink);
        // A T junction, a collinear overlap and a duplicate path in each net.
        for (const auto& path : QList<QVariantList>{
                 {point(-100, y), point(100, y)},
                 {point(0, y), point(0, y + 100)},
                 {point(-50, y), point(50, y)},
                 {point(100, y), point(-100, y)}})
            ctx.wires.append(QVariantMap{{"wireType", "ortho"}, {"path", path}});
    }
    return ctx;
}
void settle(EventEngine& engine) {
    for (int i = 0; i < 1000 && !engine.isIdle(); ++i) engine.run(100000, 2000000);
    if (!engine.isIdle()) throw std::runtime_error("event engine failed to settle");
}
QByteArray eventState(EventEngine& engine, const CircuitContext& ctx) {
    QVariantList rows;
    for (const auto& value : ctx.components) {
        const auto id = value.toMap().value("id").toString();
        rows.append(QVariantMap{{"id", id}, {"in", engine.getInputs(id)},
                                {"out", engine.getOutputs(id)}});
    }
    auto dirty = engine.takeDirtyQids();
    std::sort(dirty.begin(), dirty.end());
    return canonicalList(rows) + json(QVariant::fromValue(dirty));
}
QByteArray iterState(const CircuitContext& ctx) {
    QVariantList rows;
    for (const auto& value : ctx.components) {
        const auto c = value.toMap();
        rows.append(QVariantMap{{"id", c.value("id")}, {"in", c.value("inputPortValues")},
                                {"out", c.value("outputPorts")}});
    }
    return canonicalList(rows);
}
// Setup and result serialization are excluded. Each prepare returns a timed operation
// and an untimed observer, so every repetition starts with identical state.
struct Trial { std::function<void()> run; std::function<QByteArray()> observe; };
void measure(const QString& name, int samples, const std::function<Trial()>& prepare) {
    QList<double> times;
    QByteArray expected;
    for (int i = -2; i < samples; ++i) {
        auto trial = prepare();
        QElapsedTimer timer; timer.start();
        trial.run();
        const double ms = timer.nsecsElapsed() / 1e6;
        const auto fingerprint = QCryptographicHash::hash(trial.observe(), QCryptographicHash::Sha256).toHex();
        if (expected.isEmpty()) expected = fingerprint;
        if (fingerprint != expected) throw std::runtime_error("semantic fingerprint changed between repetitions");
        if (i >= 0) times.append(ms);
    }
    std::sort(times.begin(), times.end());
    const int n = times.size();
    const double median = n % 2 ? times[n / 2] : (times[n / 2 - 1] + times[n / 2]) / 2;
    QJsonObject result{{"workload", name}, {"samples", n}, {"median_ms", median},
        {"p95_ms", times[std::max(0, int(std::ceil(n * .95)) - 1)]},
        {"sha256", QString::fromLatin1(expected)}};
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
}
}
int main(int argc, char** argv) {
    // Must precede Qt application creation and every QHash used by the workloads.
    qputenv("QT_HASH_SEED", "0");
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    bool sizeOk = true, samplesOk = true;
    const int size = args.size() > 1 ? args[1].toInt(&sizeOk) : 256;
    const int samples = args.size() > 2 ? args[2].toInt(&samplesOk) : 21;
    if (!sizeOk || !samplesOk || size < 8 || samples < 1 || args.size() > 3) {
        QTextStream(stderr) << "Usage: logicsim_perf [size>=8] [samples>=1]\n";
        return 2;
    }
    QTextStream(stdout) << QJsonDocument(QJsonObject{{"qt", qVersion()}, {"size", size},
        {"hash_seed", 0}, {"warmups", 2}, {"toggle_rounds", 16}}).toJson(QJsonDocument::Compact) << '\n';
    const auto ctx = circuit(size);
    const auto topo = topology(size);
    try {
        measure("event_build_settle", samples, [&] {
            auto e = std::make_shared<EventEngine>();
            return Trial{[&, e] { e->build(ctx.components, ctx.wires, {}, {}); settle(*e); },
                         [&, e] { return eventState(*e, ctx) + QByteArray::number(e->totalEventsProcessed()); }};
        });
        measure("event_toggle_16", samples, [&] {
            auto e = std::make_shared<EventEngine>();
            e->build(ctx.components, ctx.wires, {}, {}); settle(*e); e->clearDirtyQids();
            return Trial{[e] {
                for (int i = 0; i < 16; ++i) {
                    e->triggerInputString("src", i % 2 ? "00000000" : "11111111"); settle(*e);
                }
            }, [&, e] { return eventState(*e, ctx) + QByteArray::number(e->totalEventsProcessed()); }};
        });
        measure("iter_evaluate", samples, [&] {
            auto state = std::make_shared<CircuitContext>(ctx);
            return Trial{[state] { IterEngine::evaluateContext(*state); },
                         [state] { return iterState(*state); }};
        });
        measure("topology_virtual_and_dots", samples, [&] {
            auto result = std::make_shared<QPair<QVariantList, QVariantList>>();
            return Trial{[&, result] {
                result->first = WireNetworkSolver::generateVirtualWires(topo.components, topo.wires);
                result->second = WireNetworkSolver::connectionDots(topo.components, topo.wires);
            }, [result] { return canonicalList(result->first) + canonicalList(result->second); }};
        });
    } catch (const std::exception& e) {
        QTextStream(stderr) << e.what() << '\n';
        return 1;
    }
}
