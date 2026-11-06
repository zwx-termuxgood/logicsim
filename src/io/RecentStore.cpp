#include "RecentStore.h"
// 引入 RecentStore 命名空间声明：实现 load/save 前必须看到函数原型
#include <QJsonDocument>
// 引入 QJsonDocument：JSON 字符串与 QJsonArray 互转的入口
#include <QJsonObject>
// 引入 QJsonObject：单条最近文件记录与 QVariantMap 互转
#include <QJsonArray>
// 引入 QJsonArray：最近文件列表的 JSON 表示
#include <QSettings>
// 引入 QSettings：最近文件列表的持久化存储后端

namespace RecentStore {
// 打开 RecentStore 命名空间，与头文件声明保持一致

// 定义"从 QSettings 读取最近文件列表"接口
QVariantList load() {
    // 准备结果列表
    QVariantList result;
    // 打开默认 QSettings
    QSettings s;
    // 读取键 "recentFiles" 的 JSON 字符串
    QString json = s.value("recentFiles").toString();
    // 首次运行时为空，直接返回空列表
    if (json.isEmpty()) return result;

    // 准备 JSON 解析错误容器
    QJsonParseError perr;
    // 把字符串解析成 JSON 文档
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &perr);
    // 解析失败返回空列表，绝不崩溃
    if (perr.error != QJsonParseError::NoError) return result;
    // 顶层不是数组视为损坏，返回空列表
    if (!doc.isArray()) return result;

    // 取 JSON 数组
    QJsonArray arr = doc.array();
    // 逐项转成 QVariantMap 加入结果
    for (const auto v : arr)
        result.append(v.toObject().toVariantMap());
    // 返回最近文件列表
    return result;
}

// 定义"把最近文件列表写回 QSettings"接口
void save(const QVariantList& list) {
    // 准备 JSON 数组
    QJsonArray arr;
    // 逐项把 QVariantMap 转成 QJsonObject 加入数组
    for (auto& v : list)
        arr.append(QJsonObject::fromVariantMap(v.toMap()));
    // 打开默认 QSettings
    QSettings s;
    // 序列化为紧凑 JSON 字符串并写入键 "recentFiles"
    s.setValue("recentFiles",
               QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    // 立即落盘，避免异常退出丢失最近记录
    s.sync();
}

} // namespace RecentStore
// 关闭 RecentStore 命名空间