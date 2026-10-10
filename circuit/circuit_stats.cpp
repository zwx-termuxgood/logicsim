#include "circuit.h"
#include <functional>

QVariantMap Circuit::statistics() const {
    QVariantMap result;
    QHash<QString, int> typeCount;
    int totalGates = 0, totalInputs = 0, totalOutputs = 0, totalSplits = 0;
    int totalHubs = 0, totalClocks = 0, totalSubInst = 0, totalTexts = 0;
    int totalConsts = 0;

    std::function<void(const Context&, QSet<QString>&)> countCtx =
        [&](const Context& ctx, QSet<QString>& visited) {
            for (const auto& cv : ctx.components) {
                QVariantMap c = cv.toMap();
                QString t = c.value("type").toString();
                if (t == "text") { totalTexts++; continue; }
                if (t == "input") { totalInputs++; continue; }
                if (t == "const") { totalConsts++; continue; }
                if (t == "output" || t == "led") { totalOutputs++; continue; }
                if (t == "clock") { totalClocks++; continue; }
                if (t == "splitter") { totalSplits++; continue; }
                if (t == "hub") { totalHubs++; continue; }
                if (t == "sub") {
                    totalSubInst++;
                    QString subId = c.value("subId").toString();
                    if (!visited.contains(subId)) {
                        visited.insert(subId);
                        auto it = m_subContexts.find(subId);
                        if (it != m_subContexts.end()) countCtx(it.value(), visited);
                    }
                    continue;
                }
                totalGates++;
                typeCount[t]++;
            }
        };

    QSet<QString> visited;
    countCtx(m_root, visited);

    result["totalGates"] = totalGates;
    result["totalInputs"] = totalInputs;
    result["totalConsts"] = totalConsts;
    result["totalOutputs"] = totalOutputs;
    result["totalSplits"] = totalSplits;
    result["totalHubs"] = totalHubs;
    result["totalClocks"] = totalClocks;
    result["totalSubInst"] = totalSubInst;
    result["totalTexts"] = totalTexts;
    result["subCount"] = m_subContexts.size();

    QVariantMap byType;
    for (auto it = typeCount.begin(); it != typeCount.end(); ++it)
        byType[it.key()] = it.value();
    result["byType"] = byType;
    return result;
}