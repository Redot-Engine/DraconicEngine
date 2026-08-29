/// DX12 implementation of Surface. Simply stores the HWND.

module;

#include "DxIncludes.h"

export module rhi.dx12:surface;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxSurfaceImpl : public Surface {
public:
    explicit DxSurfaceImpl(HWND hwnd) : m_hwnd(hwnd) {}

    [[nodiscard]] HWND handle() const { return m_hwnd; }

private:
    HWND m_hwnd = nullptr;
};

} // namespace draco::rhi::dx12
