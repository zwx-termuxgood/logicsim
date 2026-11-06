#ifndef RECENTSTORE_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define RECENTSTORE_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QVariantList>
// 引入 QVariantList：load 返回类型与 save 参数类型都是"最近文件条目列表"
#include <QVariantMap>
// 引入 QVariantMap：列表每个元素是一条最近文件记录（path/name/time 字段）
#include <QString>
// 引入 QString：实现中会把整个列表序列化为 JSON 字符串存进 QSettings，
// 该转换需要 QString 作为中间类型；此处保留以对齐实现

/**
 * @brief 最近打开文件的持久化存储（QSettings）。
 */
// 上方注释点明职责：本模块只管"最近文件列表 ↔ QSettings 字符串"的读写，
// 不参与列表的去重/置顶/截断策略——那些逻辑在 Circuit::addRecentFile 中，
// 本模块保持薄封装便于替换存储后端（如改成 QJsonDocument 文件）

namespace RecentStore {
// 用命名空间而非类：load/save 是无状态函数，调用形式 RecentStore::load() 更自然

QVariantList load();
// 从 QSettings 读取最近文件列表：
//   - QSettings 中存的是 JSON 数组字符串（键 "recentFiles"）；
//   - 读取后解析为 QJsonArray，再逐项 toVariantMap() 放进 QVariantList；
//   - JSON 非法或不是数组时返回空列表，绝不抛异常/崩溃；
// 返回空列表也是首次运行时的正常状态

void save(const QVariantList& list);
// 把最近文件列表写回 QSettings：
//   - 逐项 QJsonObject::fromVariantMap 组装成 QJsonArray；
//   - 序列化为紧凑 JSON 字符串，写入键 "recentFiles"；
//   - 立即 sync()，避免进程异常退出时丢失最近记录；
// 不做截断/去重——调用方（Circuit::addRecentFile）已保证列表规模与顺序

} // namespace RecentStore
// 结束 RecentStore 命名空间

#endif // RECENTSTORE_H
// 结束头文件保护