#ifndef UNDODATA_H
#define UNDODATA_H

#include <QVariantMap>
#include <QString>
#include <QDateTime>
#include <QHash>
#include <vector>

#include "CircuitContext.h"

/**
 * undo 条目的作用域：决定存多少数据。
 *
 * - CurrentCtx：只保存当前上下文的快照（最常见，内存开销最小）
 * - Full      ：保存所有上下文 + 子电路元数据 + clock 状态（毁灭性操作）
 * - MetaOnly  ：只保存子电路元数据（subNames + subCounter）
 */
enum class UndoScope {
    CurrentCtx,
    Full,
    MetaOnly
};

/**
 * 一条 undo 记录。
 *
 * 具体使用哪些字段由 scope 决定：
 *   CurrentCtx : ctxId + singleCtx
 *   Full       : allContexts + subNames + subCounter + clockStates
 *   MetaOnly   : subNames + subCounter
 */
struct UndoEntry {
    UndoScope scope = UndoScope::CurrentCtx;

    // ---- CurrentCtx 使用 ----
    QString          ctxId;        // "" 表示 root
    CircuitContext   singleCtx;

    // ---- Full 使用 ----
    QHash<QString, CircuitContext> allContexts;  // "" → root；其余为子电路
    QHash<QString, QString>        subNames;
    int                            subCounter = 0;
    QHash<QString, bool>           clockStates;

    // ---- MetaOnly 使用 ----
    // （复用上面的 subNames + subCounter）

    // ---- 通用 ----
    QString   mergeKey;
    QDateTime time;
};

/**
 * undo/redo 全部状态。
 * 通过 QObject 的 dynamic property 间接持有，不占 Circuit 对象内存。
 */
struct CircuitUndoData {
    std::vector<UndoEntry> undoStack;
    std::vector<UndoEntry> redoStack;
    QString   lastMergeKey;
    QDateTime lastUndoTime;
    bool restoring   = false;
    bool redoRunning = false;
    int  maxUndo     = 200;
};

#endif // UNDODATA_H