#include <doctest_with_main.h>

import core;
import rhi;
import rhi.vk;

using namespace draco;
using namespace draco::rhi;

// These tests require a working Vulkan loader + at least one ICD (a physical
// device or a software rasterizer such as lavapipe). Where no device is
// available the backend still initializes; adapter-dependent checks are guarded.

TEST_CASE("rhi.vk: backend initializes and enumerates adapters")
{
    Backend* backend = nullptr;
    vk::VkBackendDesc desc{};
    REQUIRE(vk::createBackend(desc, backend).isOk());
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
        // The device must be torn down before the backend destroys the instance.
        if (device) device->destroy();
    }

    backend->destroy();
}
