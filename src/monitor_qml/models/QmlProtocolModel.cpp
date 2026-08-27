#include "QmlProtocolModel.h"

using namespace industrial::protocol;

QmlProtocolModel::QmlProtocolModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int QmlProtocolModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_protocols.size();
}

QVariant QmlProtocolModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_protocols.size())
    {
        return {};
    }
    const auto &descriptor = m_protocols.at(index.row());
    if (role == ProtocolKeyRole)
    {
        return descriptor.key;
    }
    if (role == DisplayNameRole)
    {
        return descriptor.displayName;
    }
    return {};
}

QHash<int, QByteArray> QmlProtocolModel::roleNames() const
{
    return {
        {ProtocolKeyRole, "protocolKey"},
        {DisplayNameRole, "displayName"},
    };
}

int QmlProtocolModel::indexOfKey(const QString &protocolKey) const
{
    for (int index = 0; index < m_protocols.size(); ++index)
    {
        if (m_protocols.at(index).key == protocolKey)
        {
            return index;
        }
    }
    return -1;
}

void QmlProtocolModel::addProtocol(const ProtocolDescriptor &descriptor)
{
    if (descriptor.key.isEmpty())
    {
        return;
    }
    const int existing = indexOfKey(descriptor.key);
    if (existing >= 0)
    {
        m_protocols[existing] = descriptor;
        emit dataChanged(index(existing), index(existing),
                         {ProtocolKeyRole, DisplayNameRole});
        return;
    }

    const int row = m_protocols.size();
    beginInsertRows({}, row, row);
    m_protocols.append(descriptor);
    endInsertRows();
}
