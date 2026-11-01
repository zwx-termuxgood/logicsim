#ifndef UNDODATA_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define UNDODATA_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantMap>
// 引入 QVariantMap：importedFiles 的值类型是 QVariantMap
#include <QString>
// 引入 QString：ctxId、mergeKey、subNames 的 key/value 类型
#include <QDateTime>
// 引入 QDateTime：UndoEntry::time 与 CircuitUndoData::lastUndoTime 用它记录时间
#include <QHash>
// 引入 QHash：allContexts、subNames、clockStates、importedFiles 都用它
#include <vector>
// 引入 std::vector：undoStack/redoStack 用连续存储，支持 O(1) 随机访问与首尾删除

#include "CircuitContext.h"
// 引入 CircuitContext 完整定义：UndoEntry 以值语义内嵌 singleCtx、allContexts

enum class UndoScope {
    // 枚举 class（强类型作用域枚举）：避免枚举值泄漏到外层命名空间，
    // 也不能与整数隐式互转，减少误用
    CurrentCtx,
    // 仅快照当前上下文：适合单上下文内的编辑操作（增删元件/连线、移动等），
    // 开销最小，是 pushUndo 的默认 scope
    Full,
    // 全量快照：根上下文 + 所有子上下文 + 子电路名 + 计数器 + 时钟状态 + 导入记录；
    // 用于跨上下文操作（删除子电路、导入文件、子电路设为主电路）
    MetaOnly
    // 仅快照元信息：subNames 与 subCounter；
    // 用于新建/重命名子电路这种不改变任何元件/连线的操作
};

struct UndoEntry {
    // 单条撤销快照：根据 scope 不同，只填充对应字段，其余留空，
    // 这样 CurrentCtx 条目不会为每个子电路都存一份数据，节省内存

    UndoScope scope = UndoScope::CurrentCtx;
    // 本条快照的适用范围：决定 applyUndoEntry 时恢复哪些字段；
    // 默认 CurrentCtx 与 pushUndo 的默认行为一致

    QString          ctxId;
    // scope == CurrentCtx 时记录目标上下文 ID：
    // 空串表示根上下文，非空表示某个子电路；
    // 应用时据此决定写回 m_root 还是 m_subContexts[ctxId]

    CircuitContext   singleCtx;
    // scope == CurrentCtx 时保存的快照本体：
    // 只含一个上下文的 components/wires/两个计数器

    QHash<QString, CircuitContext> allContexts;
    // scope == Full 时保存所有上下文：
    // key 为空串代表根，其余 key 为子电路 ID；
    // 用 QHash 而非 QList 便于按 ID 直接定位

    QHash<QString, QString>        subNames;
    // 子电路显示名映射：Full 与 MetaOnly 都填充；
    // 重命名子电路这类操作靠它回滚

    int                            subCounter = 0;
    // 子电路编号计数器：Full 与 MetaOnly 都填充；
    // 回滚后必须恢复，否则新建子电路会复用已删除的编号

    QHash<QString, bool>           clockStates;
    // 各时钟元件的当前输出状态：仅 Full 填充；
    // 撤销删除时钟等操作时要一并恢复，否则重建后时钟相位错乱

    QHash<QString, QVariantMap>    importedFiles;   // ★ 导入文件记录（Full 用）
    // 已导入文件的信息映射：仅 Full 填充；
    // 因为导入会一次性新增大量子电路，撤销时必须连同记录一起回滚，
    // 否则 m_importedFiles 会残留指向已删除子电路的条目

    QString   mergeKey;
    // 合并键：连续同类操作（如拖动）会带相同 mergeKey，
    // 在 pushUndoInternal 中据此与 500ms 时间窗口一起判定是否跳过新建快照

    QDateTime time;
    // 快照创建时间：与 mergeKey 联合用于合并窗口判断
};

struct CircuitUndoData {
    // 撤销/重做的完整状态容器：Circuit 通过动态属性持有它的指针，
    // 因为头文件不想暴露 std::vector<UndoEntry> 等实现细节给所有包含者

    std::vector<UndoEntry> undoStack;
    // 撤销栈：尾部是最近一次操作前的快照；
    // 用 vector 因为只需 push_back/pop_back/按索引裁剪，且要限制最大深度

    std::vector<UndoEntry> redoStack;
    // 重做栈：执行 undo 时把"当前状态"压入此栈；
    // 任何新编辑操作都会清空它（标准撤销模型）

    QString   lastMergeKey;
    // 上一次入栈时的合并键：与本次比较，相同则可能合并

    QDateTime lastUndoTime;
    // 上一次入栈时间：与当前时间差 < 500ms 且 mergeKey 相同则跳过，
    // 避免拖动等高频操作产生海量快照

    bool restoring   = false;
    // 正在应用撤销/重做快照：
    // applyUndoEntry 内部会调用 rebuild/evaluate 等可能间接触发 pushUndo 的路径，
    // 用此标志阻止快照被二次记录，防止递归与栈污染

    bool redoRunning = false;
    // 正在执行 redo：
    // redo 过程中如果触发新编辑（理论上不应发生），pushUndoInternal 会直接返回，
    // 保证重做流程的原子性

    int  maxUndo     = 200;
    // 栈深度上限：超过时从头部丢弃最旧快照；
    // 200 是内存占用与可回溯深度的折中
};

#endif // UNDODATA_H
// 结束头文件保护