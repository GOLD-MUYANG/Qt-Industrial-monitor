#include "PluginManager.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QLibrary>
#include <QPluginLoader>

#include <industrial/protocol/ProtocolVersion.h>

using industrial::protocol::IProtocolPlugin;
using industrial::protocol::ProtocolApiVersion;

PluginManager::PluginManager() = default;

PluginManager::~PluginManager() = default;

//扫描一个路径，加载插件
void PluginManager::scan(const QString &directoryPath)
{
    m_plugins.clear();
    m_errors.clear();
    m_loaders.clear();

    const QDir directory(directoryPath);
    if (!directory.exists())
    {
        addError(directoryPath, QStringLiteral("插件目录不存在：%1").arg(directoryPath));
        return;
    }

    // 拿到所有信息
    const QFileInfoList files = directory.entryInfoList(QDir::Files, QDir::Name);

    for (const QFileInfo &file : files)
    {
        // 如果不是lib就跳过
        if (!QLibrary::isLibrary(file.fileName()))
        {
            continue;
        }

        //校验插件版本，必须与主版本适配
        // 但是这个插件的版本是怎么写进去的呢？又是怎么读出来的呢
        auto loader = std::make_unique<QPluginLoader>(file.absoluteFilePath());
        const QJsonObject rootMetadata = loader->metaData();
        const QString iid = rootMetadata.value(QStringLiteral("IID")).toString();
        if (iid != QLatin1String(INDUSTRIAL_PROTOCOL_PLUGIN_IID))
        {
            addError(file.absoluteFilePath(), QStringLiteral("插件 IID 不兼容：%1").arg(iid));
            continue;
        }

        const QJsonObject metadata = rootMetadata.value(QStringLiteral("MetaData")).toObject();
        const int apiVersion = metadata.value(QStringLiteral("apiVersion")).toInt(-1);
        if (apiVersion != ProtocolApiVersion)
        {
            addError(file.absoluteFilePath(),
                     QStringLiteral("插件 apiVersion 不兼容：期望 %1，实际 %2")
                         .arg(ProtocolApiVersion)
                         .arg(apiVersion));
            continue;
        }

        const QString key = metadata.value(QStringLiteral("key")).toString();
        if (key.trimmed().isEmpty())
        {
            addError(file.absoluteFilePath(), QStringLiteral("插件 key 不能为空"));
            continue;
        }
        if (m_plugins.contains(key))
        {
            addError(file.absoluteFilePath(), QStringLiteral("插件 key 重复：%1").arg(key));
            continue;
        }

        QObject *instance = loader->instance();
        if (!instance)
        {
            addError(file.absoluteFilePath(), loader->errorString());
            continue;
        }

        // 判断插件是否实现了 IProtocolPlugin 接口
        auto *protocolPlugin = qobject_cast<IProtocolPlugin *>(instance);
        if (!protocolPlugin)
        {
            addError(file.absoluteFilePath(), QStringLiteral("插件未实现 IProtocolPlugin"));
            continue;
        }

        const auto descriptor = protocolPlugin->descriptor();
        if (descriptor.key != key || descriptor.apiVersion != apiVersion)
        {
            addError(file.absoluteFilePath(), QStringLiteral("插件描述与 JSON 元数据不一致"));
            continue;
        }

        m_plugins.insert(key, protocolPlugin);
        m_loaders.push_back(std::move(loader));
    }
}

const QHash<QString, IProtocolPlugin *> &PluginManager::plugins() const
{
    return m_plugins;
}

const QList<PluginLoadError> &PluginManager::errors() const
{
    return m_errors;
}

IProtocolPlugin *PluginManager::plugin(QStringView key) const
{
    return m_plugins.value(key.toString(), nullptr);
}

void PluginManager::addError(const QString &filePath, const QString &message)
{
    m_errors.append({filePath, message});
}
