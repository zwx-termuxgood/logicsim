#include "circuit.h"
#include "ComponentTraits.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

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
    qDebug() << "[CLEAR] clear()";
    if (m_viewOnly) return;
    pushUndo();
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
    result["version"] = 10;
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
    result["clockTickCount"] = m_clockTickCount;
    return result;
}

bool Circuit::fromJson(const QVariantMap& data) {
    CircuitUndoData* d = und();
    bool wasRestoring = d ? d->restoring : false;
    qDebug() << "[JSON] fromJson enter, wasRestoring=" << wasRestoring
             << "data size=" << data.size();
    if (d) d->restoring = true;

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
    qDebug() << "[JSON] loading root comps=" << m_root.components.size()
             << "wires=" << m_root.wires.size()
             << "subs=" << subs.size();
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
    m_clockTickCount = data.value("clockTickCount").toInt();
    if (m_clockTickCount < 0) m_clockTickCount = 0;

    qDebug() << "[JSON] fixCtx start";
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
    qDebug() << "[JSON] fixCtx done";

    qDebug() << "[JSON] evaluateAll start";
    evaluateAll();
    qDebug() << "[JSON] evaluateAll done";

    if (!wasRestoring) {
        qDebug() << "[JSON] file-load mode, clearing undo/redo stacks";
        if (d) {
            d->undoStack.clear();
            d->redoStack.clear();
            d->lastMergeKey.clear();
            d->lastUndoTime = QDateTime();
        }
    }

    if (d) d->restoring = wasRestoring;

    if (wasRestoring) {
        qDebug() << "[JSON] restore mode, return without signals";
        return true;
    }

    qDebug() << "[JSON] emitting signals";
    emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit errorsChanged();
    emit clockChanged();
    emit changed();
    qDebug() << "[JSON] fromJson done";
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

// ============================================================
// 撤销 / 重做实现（全部通过 und() 访问堆上的 CircuitUndoData）
// ============================================================

void Circuit::pushUndo(const QString& mergeKey) {
    CircuitUndoData* d = und();
    qDebug() << "========================================================";
    qDebug() << "[UNDO] pushUndo ENTER mergeKey=" << mergeKey;
    dumpInternalState("pushUndo-entry");

    if (!d)               { qDebug() << "[UNDO] abort: no UndoData"; return; }
    if (m_viewOnly)       { qDebug() << "[UNDO] abort: viewOnly";    return; }
    if (d->restoring)     { qDebug() << "[UNDO] abort: restoring";   return; }
    if (d->redoRunning)   { qDebug() << "[UNDO] abort: redoRunning"; return; }

    if (!mergeKey.isEmpty() && mergeKey == d->lastMergeKey) {
        if (d->lastUndoTime.isValid() &&
            d->lastUndoTime.msecsTo(QDateTime::currentDateTime()) < 500) {
            d->lastUndoTime = QDateTime::currentDateTime();
            qDebug() << "[UNDO] merged, skip";
            return;
        }
    }

    qDebug() << "[UNDO] toJson() start";
    QVariantMap snap = toJson();
    qDebug() << "[UNDO] toJson() done, keys=" << snap.size();

    qDebug() << "[UNDO] push_back start (undoSize before="
             << (int)d->undoStack.size() << ")";
    d->undoStack.push_back(snap);
    qDebug() << "[UNDO] push_back done (undoSize after="
             << (int)d->undoStack.size() << ")";

    while (d->undoStack.size() > static_cast<size_t>(d->maxUndo))
        d->undoStack.erase(d->undoStack.begin());
    qDebug() << "[UNDO] trimming done (undoSize=" << (int)d->undoStack.size() << ")";

    d->redoStack.clear();
    qDebug() << "[UNDO] redo cleared";

    d->lastMergeKey = mergeKey;
    d->lastUndoTime = QDateTime::currentDateTime();
    emit undoRedoChanged();
    qDebug() << "[UNDO] pushUndo DONE";
    qDebug() << "========================================================";
}

void Circuit::breakUndoMerge() {
    CircuitUndoData* d = und();
    if (!d) return;
    d->lastMergeKey.clear();
    d->lastUndoTime = QDateTime();
}

void Circuit::undo() {
    CircuitUndoData* d = und();
    if (!d) return;
    qDebug() << "[UNDO] undo() enter, canUndo=" << canUndo()
             << "stackSize=" << (int)d->undoStack.size()
             << "viewOnly=" << m_viewOnly
             << "running=" << d->redoRunning;

    if (m_viewOnly)         { qDebug() << "[UNDO] undo viewOnly abort"; return; }
    if (d->redoRunning)     { qDebug() << "[UNDO] undo reentered abort"; return; }
    if (d->undoStack.empty()){ qDebug() << "[UNDO] undo empty abort"; return; }

    d->redoRunning = true;

    QVariantMap cur = toJson();
    QVariantMap prev = d->undoStack.back();
    d->undoStack.pop_back();
    d->redoStack.push_back(cur);
    while (d->redoStack.size() > static_cast<size_t>(d->maxUndo))
        d->redoStack.erase(d->redoStack.begin());
    breakUndoMerge();

    restoreSnapshot(prev, true);

    d->redoRunning = false;
    emit undoRedoChanged();
    qDebug() << "[UNDO] undo() done";
}

void Circuit::redo() {
    CircuitUndoData* d = und();
    if (!d) return;
    qDebug() << "[UNDO] redo() enter, canRedo=" << canRedo()
             << "redoSize=" << (int)d->redoStack.size()
             << "viewOnly=" << m_viewOnly
             << "running=" << d->redoRunning;

    if (m_viewOnly)          { qDebug() << "[UNDO] redo viewOnly abort"; return; }
    if (d->redoRunning)      { qDebug() << "[UNDO] redo reentered abort"; return; }
    if (d->redoStack.empty()){ qDebug() << "[UNDO] redo empty abort"; return; }

    d->redoRunning = true;

    QVariantMap cur = toJson();
    QVariantMap next = d->redoStack.back();
    d->redoStack.pop_back();
    d->undoStack.push_back(cur);
    while (d->undoStack.size() > static_cast<size_t>(d->maxUndo))
        d->undoStack.erase(d->undoStack.begin());
    breakUndoMerge();

    restoreSnapshot(next, true);

    d->redoRunning = false;
    emit undoRedoChanged();
    qDebug() << "[UNDO] redo() done";
}

void Circuit::restoreSnapshot(const QVariantMap& snap, bool emitSignals) {
    CircuitUndoData* d = und();
    if (!d) return;
    qDebug() << "[RESTORE] enter, emitSignals=" << emitSignals
             << "restoring=" << d->restoring;

    if (d->restoring) { qDebug() << "[RESTORE] already restoring, abort"; return; }

    QString savedCtx = m_currentCtxId;
    bool savedView = m_viewOnly;

    d->restoring = true;
    fromJson(snap);
    d->restoring = false;

    if (!savedCtx.isEmpty() && !m_subContexts.contains(savedCtx)) {
        savedCtx = "";
        savedView = false;
    }

    bool ctxChanged = (savedCtx != m_currentCtxId) || (savedView != m_viewOnly);
    m_currentCtxId = savedCtx;
    m_viewOnly = savedView;

    if (!emitSignals) return;

    if (ctxChanged) emit contextChanged();
    emit subcircuitsChanged();
    emit editContextsChanged();
    emit errorsChanged();
    emit clockChanged();
    emit changed();
    qDebug() << "[RESTORE] done";
}