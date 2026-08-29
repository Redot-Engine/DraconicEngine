/// DX12 implementation of ShaderModule. Stores DXIL bytecode.

module;

#include <vector>
#include <span>

#include <cstring>

export module rhi.dx12:shader_module;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxShaderModuleImpl : public ShaderModule {
public:
    Status init(const ShaderModuleDesc& d) {
        m_bytecode.resize(d.code.size());
        std::memcpy(m_bytecode.data(), d.code.data(), d.code.size());
        return ErrorCode::Ok;
    }

    void cleanup() { m_bytecode.clear(); }

    [[nodiscard]] std::span<const u8> bytecode() const { return { m_bytecode.data(), m_bytecode.size() }; }

private:
    std::vector<u8> m_bytecode;
};

} // namespace draco::rhi::dx12
