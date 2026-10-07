#include "circuit.h"
#include "ComponentTraits.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

void Circuit::refreshErrors() {
    m_errors.clear();
    auto checkCtx = [&](const Context& ctx, const QString& label) {
        for (auto& wv : ctx.wires) {
            QVariantMap w = wv.toMap();
            QString fromId = w.value("fromComp").toString();
            QString toId = w.value("toComp").toString();
            int fromPort = w.value("fromPort").toInt();
            int toPort = w.value("toPort").toInt();
            int fi = -1, ti = -1;
            for (int i = 0; i < ctx.components.size(); ++i) {
                QString cid = ctx.components[i].toMap().value("id").toString();
                if (cid == fromId) fi = i;
                if (cid == toId) ti = i;
            }
            if (fi < 0 || ti < 0) continue;
            int fbw = ComponentTraits::outputBitWidth(ctx.components[fi].toMap(), fromPort);
            int tbw = ComponentTraits::inputBitWidth(ctx.components[ti].toMap(), toPort);
            if (fbw != tbw) {
                QVariantMap err;
                err["type"] = "width_mismatch";
                err["msg"] = QString("%1：%2 位宽不匹配（源 %3 位，目标 %4 位）")
                                 .arg(label, w.value("id").toString()).arg(fbw).arg(tbw);
                m_errors.append(err);
            }
        }
    };
    checkCtx(m_root, "主电路");
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it)
        checkCtx(it.value(), m_subNames.value(it.key(), it.key()));
    emit errorsChanged();
}

void Circuit::clear() {
    m_root = Context();
    m_subContexts.clear();
    m_subNames.clear();
    m_currentCtxId = "";
    m_viewOnly = false;
    m_subCounter = 0;
    m_errors.clear();
    m_clockStates.clear();
    m_clockTickCount = 0;
    if (m_clockRunning) stopClock();
    emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit errorsChanged();
    emit clockChanged();
    emit changed();
}

QVariantMap Circuit::toJson() const {
    QVariantMap result;
    result["version"] = 9;
    result["root"] = QVariantMap{
        {"components", m_root.components},
        {"wires", m_root.wires},
        {"compCounter", m_root.compCounter},
        {"wireCounter", m_root.wireCounter}
    };
    QVariantList subs;
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it) {
        QVariantMap s;
        s["id"] = it.key();
        s["name"] = m_subNames.value(it.key(), "");
        s["components"] = it.value().components;
        s["wires"] = it.value().wires;
        s["compCounter"] = it.value().compCounter;
        s["wireCounter"] = it.value().wireCounter;
        subs.append(s);
    }
    result["subcircuits"] = subs;
    result["subCounter"] = m_subCounter;
    result["clockFrequency"] = m_clockFrequency;
    return result;
}

bool Circuit::fromJson(const QVariantMap& data) {
    m_root = Context();
    m_subContexts.clear();
    m_subNames.clear();
    m_currentCtxId = "";
    m_viewOnly = false;
    m_clockStates.clear();

    if (data.contains("root")) {
        QVariantMap r = data.value("root").toMap();
        m_root.components = r.value("components").toList();
        m_root.wires = r.value("wires").toList();
        m_root.compCounter = r.value("compCounter").toInt();
        m_root.wireCounter = r.value("wireCounter").toInt();
    }
    QVariantList subs = data.value("subcircuits").toList();
    for (auto& sv : subs) {
        QVariantMap s = sv.toMap();
        QString id = s.value("id").toString();
        Context c;
        c.components = s.value("components").toList();
        c.wires = s.value("wires").toList();
        c.compCounter = s.value("compCounter").toInt();
        c.wireCounter = s.value("wireCounter").toInt();
        m_subContexts[id] = c;
        m_subNames[id] = s.value("name").toString();
    }
    m_subCounter = data.value("subCounter").toInt();
    m_clockFrequency = qBound(1, data.value("clockFrequency").toInt(), 1000);
    if (m_clockFrequency < 1) m_clockFrequency = 2;

    auto fixCtx = [&](Context& ctx) {
        for (int i = 0; i < ctx.components.size(); ++i) {
            QVariantMap c = ctx.components[i].toMap();
            if (!c.contains("displayBase")) c["displayBase"] = "bin";
            if (!c.contains("name")) c["name"] = "";
            if (c.value("type").toString() == "text" && !c.contains("content"))
                c["content"] = "文本";
            int bw = c.value("bitWidth").toInt();
            if (bw < 1) bw = 1;
            QVariantList bits = c.value("inputBits").toList();
            while (bits.size() < bw) bits.prepend(false);
            while (bits.size() > bw) bits.removeFirst();
            c["inputBits"] = bits;
            QString t = c.value("type").toString();
            if ((t == "splitter" || t == "hub") && !c.contains("outputSplits")) {
                QVariantList splits;
                for (int j = 0; j < bw; ++j) splits.append(1);
                c["outputSplits"] = splits;
            }
            if (t == "clock") m_clockStates[c.value("id").toString()] = false;
            ctx.components[i] = c;
        }
    };
    fixCtx(m_root);
    for (auto it = m_subContexts.begin(); it != m_subContexts.end(); ++it) fixCtx(it.value());

    evaluateAll();
    emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit errorsChanged();
    emit clockChanged();
    emit changed();
    return true;
}

bool Circuit::saveToFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QJsonObject obj = QJsonObject::fromVariantMap(toJson());
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();
    addRecentFile(path);
    return true;
}

bool Circuit::loadFromFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QByteArray data = f.readAll();
    f.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return false;
    if (!doc.isObject()) return false;
    bool ok = fromJson(doc.object().toVariantMap());
    if (ok) addRecentFile(path);
    return ok;
}