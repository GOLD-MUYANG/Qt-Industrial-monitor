#include <industrial/protocol/ProtocolTypes.h>

namespace industrial::protocol {

void registerProtocolMetaTypes()
{
    qRegisterMetaType<DeviceConfig>("industrial::protocol::DeviceConfig");
    qRegisterMetaType<DeviceErrorCategory>("industrial::protocol::DeviceErrorCategory");
    qRegisterMetaType<ProtocolDescriptor>("industrial::protocol::ProtocolDescriptor");
    qRegisterMetaType<DeviceState>("industrial::protocol::DeviceState");
    qRegisterMetaType<MeasurementSample>("industrial::protocol::MeasurementSample");
    qRegisterMetaType<SampleBatch>("industrial::protocol::SampleBatch");
    qRegisterMetaType<WriteRequest>("industrial::protocol::WriteRequest");
    qRegisterMetaType<WriteResult>("industrial::protocol::WriteResult");
    qRegisterMetaType<DeviceError>("industrial::protocol::DeviceError");
    qRegisterMetaType<TransactionLog>("industrial::protocol::TransactionLog");
    qRegisterMetaType<RealtimeSnapshot>("industrial::protocol::RealtimeSnapshot");
    qRegisterMetaType<RealtimeSnapshotBatch>("industrial::protocol::RealtimeSnapshotBatch");
}

} // namespace industrial::protocol
