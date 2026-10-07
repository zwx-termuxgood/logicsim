#ifndef UNDODATA_H
#define UNDODATA_H

#include <QVariantMap>
#include <QString>
#include <QDateTime>
#include <vector>

/**
 * undo/redo 全部状态。
 * 通过 QObject 的 dynamic property 间接持有，不占 Circuit 对象内存。
 */
struct CircuitUndoData {
    std::vector<QVariantMap> undoStack;
    std::vector<QVariantMap> redoStack;
    QString   lastMergeKey;
    QDateTime lastUndoTime;
    bool restoring   = false;
    bool redoRunning = false;
    int  maxUndo     = 200;
};

#endif // UNDODATA_H