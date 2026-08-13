#include <doctest_with_main.h>

import core;
import rhi;
import rhi.dx12;

using namespace draco;
using namespace draco::rhi;

// Windows-only. Requires a working DXGI/D3D12 stack (a hardware adapter or the
// WARP software adapter). Where no device is available the backend still
// initializes; adapter-dependent checks are guarded.

TEST_CASE("rhi.dx12: backend initializes and enumerates adapters")
{
    Backend* backend = nullptr;
    dx12::DxBackendDesc desc{};
    REQUIRE(dx12::createDxBackend(desc, backend).isOk());
    REQUIRE(backend != nullptr);
    CHECK(backend->isInitialized);

    auto adapters = backend->enumerateAdapters();
    INFO("adapter count: ", adapters.size());

    for (Adapter* a : adapters) {
        const AdapterInfo info = a->info();
        CHECK(!info.name.empty());
    }

    if (!adapters.empty()) {
        // Adapters are ordered best-first; a device should be creatable from [0].
        Device* device = nullptr;
        CHECK(adapters[0]->createDevice(DeviceDesc{}, device).isOk());
        CHECK(device != nullptr);
        // Tear the device down before the backend releases the factory/adapters.
        if (device) device->destroy();
    }

    backend->destroy();
}
