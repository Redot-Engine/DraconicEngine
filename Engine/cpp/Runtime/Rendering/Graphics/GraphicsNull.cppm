// The `graphics.null` module.
//
// Headless GraphicsDevice factory over the Null RHI backend (no GPU). For CI,
// servers, and tests, and the reference for what a real backend provides. Kept in
// its own module so the core host (graphics) imports only the base RHI — importing
// a backend module into the core interface trips GCC's module reader, and keeps
// the host GPU-backend-agnostic.

module;

#include <expected>
#include <memory>

export module graphics.null;

import core.stdtypes;
import core.status;
import rhi;
import rhi.null;
import graphics;

using namespace draco;
namespace rhi = draco::rhi;

export namespace draco::graphics
{
    // Create a headless GraphicsDevice backed by the Null RHI. No Vulkan required.
    std::expected<std::unique_ptr<GraphicsDevice>, ErrorCode> createNullGraphicsDevice(u32 framesInFlight = 2)
    {
        rhi::Backend* raw = nullptr;
        if (!rhi::null::createNullBackend(raw).isOk()) { return std::unexpected(ErrorCode::Unknown); }
        return GraphicsDevice::fromBackend(raw, framesInFlight);
    }
}
