#include "RecentStore.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>

namespace RecentStore {

QVariantList load() {
    QVariantList result;
    QSettings s;
    QString json = s.value("recentFiles").toString();
    if (json.isEmpty()) return result;

    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError) return result;
    if (!doc.isArray()) return result;

    QJsonArray arr = doc.array();
    for (const auto v : arr)
        result.append(v.toObject().toVariantMap());
    return result;
}

void save(const QVariantList& list) {
    QJsonArray arr;
    for (auto& v : list)
        arr.append(QJsonObject::fromVariantMap(v.toMap()));
    QSettings s;
    s.setValue("recentFiles",
               QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    s.sync();
}

} // namespace RecentStore
