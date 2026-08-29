/// Deferred implementations that break circular dependencies between
/// ValidatedBackend, ValidatedAdapter, and ValidatedDevice.

export module rhi.validation:wiring;

import core.status;
import rhi;
import :validated_backend;
import :validated_adapter;
import :validated_device;

using namespace draco;

namespace draco::rhi::validation {

ValidatedAdapter* ValidatedBackend::createValidatedAdapter(Adapter* inner) {
    return new ValidatedAdapter(inner);
}

Status ValidatedAdapter::createDevice(const DeviceDesc& desc, Device*& out) {
    Device* innerDevice = nullptr;
    Status r = m_inner->createDevice(desc, innerDevice);
    if (r != ErrorCode::Ok || !innerDevice) { out = nullptr; return r; }
    out = new ValidatedDevice(innerDevice);
    return ErrorCode::Ok;
}

} // namespace draco::rhi::validation
