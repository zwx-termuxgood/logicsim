#include "circuit.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QSettings>
#include <QDateTime>

void Circuit::loadRecentFiles() {
    QSettings s;
    QString json = s.value("recentFiles").toString();
    if (json.isEmpty()) { m_recentFiles.clear(); return; }
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QJsonArray arr = doc.array();
    m_recentFiles.clear();
    for (const auto v : arr)
        m_recentFiles.append(v.toObject().toVariantMap());
}

void Circuit::saveRecentFiles() {
    QJsonArray arr;
    for (auto& v : m_recentFiles)
        arr.append(QJsonObject::fromVariantMap(v.toMap()));
    QSettings s;
    s.setValue("recentFiles", QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    s.sync();
}

void Circuit::addRecentFile(const QString& path) {
    if (path.isEmpty()) return;
    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        if (m_recentFiles[i].toMap().value("path").toString() == path)
            m_recentFiles.removeAt(i);
    }
    QVariantMap m;
    m["path"] = path;
    QFileInfo fi(path);
    m["name"] = fi.fileName();
    m["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
    m_recentFiles.prepend(m);
    while (m_recentFiles.size() > 10) m_recentFiles.removeLast();
    saveRecentFiles();
    emit recentFilesChanged();
}

void Circuit::removeRecentFile(const QString& path) {
    for (int i = m_recentFiles.size() - 1; i >= 0; --i) {
        if (m_recentFiles[i].toMap().value("path").toString() == path)
            m_recentFiles.removeAt(i);
    }
    saveRecentFiles();
    emit recentFilesChanged();
}

void Circuit::clearRecentFiles() {
    m_recentFiles.clear();
    saveRecentFiles();
    emit recentFilesChanged();
}
