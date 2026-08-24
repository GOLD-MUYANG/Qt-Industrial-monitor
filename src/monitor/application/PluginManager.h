#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringView>

#include <memory>
#include <vector>

#include <industrial/protocol/IProtocolPlugin.h>

class QPluginLoader;

struct PluginLoadError
{
    QString filePath;
    QString message;
};

class PluginManager
{
public:
    PluginManager();
    ~PluginManager();

    PluginManager(const PluginManager &) = delete;
    PluginManager &operator=(const PluginManager &) = delete;

    void scan(const QString &directoryPath);

    const QHash<QString, industrial::protocol::IProtocolPlugin *> &plugins() const;
    const QList<PluginLoadError> &errors() const;
    industrial::protocol::IProtocolPlugin *plugin(QStringView key) const;

private:
    void addError(const QString &filePath, const QString &message);

    QHash<QString, industrial::protocol::IProtocolPlugin *> m_plugins;
    QList<PluginLoadError> m_errors;
    std::vector<std::unique_ptr<QPluginLoader>> m_loaders;
};
