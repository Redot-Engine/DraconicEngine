// The `graphics.gpu` module.
//
// The GPU-backend factory for GraphicsDevice: turns a GraphicsDeviceDesc into a
// live device on Vulkan or DX12 (validation-wrapped on request), then delegates
// the backend-agnostic bring-up to GraphicsDevice::fromBackend. This is the only
// Vulkan-coupled part of the render host, kept separate so the host types — and
// everything built on them (Application, the UI, the renderer) — stay GPU-backend
// agnostic and build headlessly. Null is delegated to createNullGraphicsDevice.

module;

#include <expected>
#include <memory>

export module graphics.gpu;

import core.stdtypes;
import core.status;
import rhi;
import rhi.vk;
#ifdef DRACO_HAS_DX12
import rhi.dx12;
#endif
import rhi.validation;
import graphics;
import graphics.null;   // Null backend delegation

using namespace draco;
namespace rhi = draco::rhi;

export namespace draco::graphics
{
    // Create a GraphicsDevice for the requested backend. Returns an error if the
    // backend is unavailable (e.g. DX12 off this platform) or bring-up fails.
    std::expected<std::unique_ptr<GraphicsDevice>, ErrorCode> createGraphicsDevice(const GraphicsDeviceDesc& desc)
    {
        if (desc.backend == BackendType::Null)
        {
            return createNullGraphicsDevice(desc.framesInFlight);
        }

        rhi::Backend* raw = nullptr;
        switch (desc.backend)
        {
            case BackendType::Vulkan:
            {
                rhi::vk::VkBackendDesc bd{};
                bd.enableValidation = desc.enableValidation;
                if (!rhi::vk::createBackend(bd, raw).isOk()) { return std::unexpected(ErrorCode::Unknown); }
                break;
            }
            case BackendType::DX12:
            {
#ifdef DRACO_HAS_DX12
                rhi::dx12::DxBackendDesc bd{};
                bd.enableValidation = desc.enableValidation;
                if (!rhi::dx12::createDxBackend(bd, raw).isOk()) { return std::unexpected(ErrorCode::Unknown); }
#else
                return std::unexpected(ErrorCode::Unknown);
#endif
                break;
            }
            case BackendType::Null:
                break;  // handled above
        }

        rhi::Backend* backend = desc.enableValidation ? rhi::validation::createValidatedBackend(raw) : raw;
        return GraphicsDevice::fromBackend(backend, desc.framesInFlight, desc.requiredFeatures);
    }
}
