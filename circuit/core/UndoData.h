#ifndef UNDODATA_H
#define UNDODATA_H

#include <QVariantMap>
#include <QString>
#include <QDateTime>
#include <QHash>
#include <vector>

#include "CircuitContext.h"

enum class UndoScope {
    CurrentCtx,
    Full,
    MetaOnly
};

struct UndoEntry {
    UndoScope scope = UndoScope::CurrentCtx;

    QString          ctxId;
    CircuitContext   singleCtx;

    QHash<QString, CircuitContext> allContexts;
    QHash<QString, QString>        subNames;
    int                            subCounter = 0;
    QHash<QString, bool>           clockStates;
    QHash<QString, QVariantMap>    importedFiles;   // ★ 导入文件记录（Full 用）

    QString   mergeKey;
    QDateTime time;
};

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