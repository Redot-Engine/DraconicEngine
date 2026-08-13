/// Validation wrapper for Adapter.

export module rhi.validation:validated_adapter;

import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::validation {

class ValidatedDevice;

class ValidatedAdapter : public Adapter {
public:
    explicit ValidatedAdapter(Adapter* inner) : m_inner(inner) {}

    void getInfo(AdapterInfo& out) override { m_inner->getInfo(out); }

    Status createDevice(const DeviceDesc& desc, Device*& out) override;

    Adapter* inner() const { return m_inner; }

private:
    Adapter* m_inner;
};

} // namespace draco::rhi::validation
