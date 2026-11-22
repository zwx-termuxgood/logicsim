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

// 独立性能基准：用固定夹具重复测量引擎与拓扑求解，并输出逐行 JSON 便于比较。
// 耗时反映当前构建与运行环境；语义指纹用于检查同一工作负载各次运行的结果一致性。
namespace {
QByteArray json(const QVariant& value) {
    return QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact);
}
// 将每条记录序列化后排序，消除容器遍历顺序的影响；记录内部的端口顺序仍保留。
// 仅规范化顶层记录顺序，重复记录不会去重，因此重复边等结果差异仍会影响指纹。
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
    // 每八个非门组成一条从 src 出发的短链，末尾可不足八个；共用输入形成扇出。
    // 固定八位总线与分离布局，重点测量事件分派、脏状态跟踪和链式传播。
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
        // 每个独立网络包含 T 接、共线重叠和反向重复路径，网络间相隔 200，避免意外交叉。
        // 同时压测连通关系生成和连接点去重，而非单纯增加互不相关的直线。
        for (const auto& path : QList<QVariantList>{
                 {point(-100, y), point(100, y)},
                 {point(0, y), point(0, y + 100)},
                 {point(-50, y), point(50, y)},
                 {point(100, y), point(-100, y)}})
            ctx.wires.append(QVariantMap{{"wireType", "ortho"}, {"path", path}});
    }
    return ctx;
}
// 收敛保护：反馈或异常传播不能无限占用基准；达到批次数上限仍未空闲则使整次基准失败。
void settle(EventEngine& engine) {
    for (int i = 0; i < 1000 && !engine.isIdle(); ++i) engine.run(100000, 2000000);
    if (!engine.isIdle()) throw std::runtime_error("event engine failed to settle");
}
// 指纹包含每个元件的输入/输出和排序后的脏 ID；takeDirtyQids 会消费脏列表。
// 此处刻意忽略脏 ID 的原始顺序，顺序契约由事件等价测试单独检查。
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
// prepare 在计时前构造独立状态；run 是唯一计时区间，observe 在计时结束后收集结果。
// 构造夹具、序列化和 SHA-256 不计入耗时；具体工作负载若在 run 中 build，则该 build 计时。
// 每项先预热两轮，再收集 samples 轮；预热结果也参与一致性校验，但不进入耗时统计。
// 指纹只覆盖 observe 返回的数据，用于重复运行回归检查，不能证明全部内部语义等价。
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
    // median 对偶数样本取中间两项均值；p95 采用最近秩 ceil(n*0.95)，不是插值百分位。
    // 仅报告耗时分布和语义摘要，不在此设置性能通过阈值，也不跨不同工作负载比较指纹。
    const int n = times.size();
    const double median = n % 2 ? times[n / 2] : (times[n / 2 - 1] + times[n / 2]) / 2;
    QJsonObject result{{"workload", name}, {"samples", n}, {"median_ms", median},
        {"p95_ms", times[std::max(0, int(std::ceil(n * .95)) - 1)]},
        {"sha256", QString::fromLatin1(expected)}};
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
}
}
int main(int argc, char** argv) {
    // 必须在创建 Qt 应用和工作负载中的任何 QHash 之前固定哈希种子，减少遍历顺序差异。
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
        // 冷构建工作负载：计入 build 与首次收敛；输出状态及累计事件数共同参与指纹。
        measure("event_build_settle", samples, [&] {
            auto e = std::make_shared<EventEngine>();
            return Trial{[&, e] { e->build(ctx.components, ctx.wires, {}, {}); settle(*e); },
                         [&, e] { return eventState(*e, ctx) + QByteArray::number(e->totalEventsProcessed()); }};
        });
        // 热传播工作负载：构建/初次收敛不计时，计时区间执行 16 轮输入翻转并逐轮收敛。
        // 累计事件数包含准备阶段，但每次准备一致；脏集合仅保留翻转阶段产生的标记。
        measure("event_toggle_16", samples, [&] {
            auto e = std::make_shared<EventEngine>();
            e->build(ctx.components, ctx.wires, {}, {}); settle(*e); e->clearDirtyQids();
            return Trial{[e] {
                for (int i = 0; i < 16; ++i) {
                    e->triggerInputString("src", i % 2 ? "00000000" : "11111111"); settle(*e);
                }
            }, [&, e] { return eventState(*e, ctx) + QByteArray::number(e->totalEventsProcessed()); }};
        });
        // 每次复制原始上下文后单独计时迭代求值；只观察端口状态，不与事件引擎指纹等同。
        measure("iter_evaluate", samples, [&] {
            auto state = std::make_shared<CircuitContext>(ctx);
            return Trial{[state] { IterEngine::evaluateContext(*state); },
                         [state] { return iterState(*state); }};
        });
        // 同一计时区间包含虚拟连线与连接点两次求解，分别规范化结果后共同计算指纹。
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
