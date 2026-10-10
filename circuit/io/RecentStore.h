#ifndef RECENTSTORE_H
#define RECENTSTORE_H

#include <QVariantList>
#include <QVariantMap>
#include <QString>

/**
 * @brief 最近打开文件的持久化存储（QSettings）。
 */
namespace RecentStore {

QVariantList load();
void save(const QVariantList& list);

} // namespace RecentStore

#endif // RECENTSTORE_H
