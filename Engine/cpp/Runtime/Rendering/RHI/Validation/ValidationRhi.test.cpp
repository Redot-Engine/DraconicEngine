#include <doctest_with_main.h>

import core;
import rhi;
import rhi.null;
import rhi.validation;

using namespace draco;
using namespace draco::rhi;

TEST_CASE("rhi.validation: wraps a backend and forwards valid calls")
{
    Backend* inner = nullptr;
    REQUIRE(null::createNullBackend(inner).isOk());

    validation::ValidatedBackend vb(inner);
    auto adapters = vb.enumerateAdapters();
    REQUIRE(adapters.size() >= 1u);

    Device* device = nullptr;
    REQUIRE(adapters[0]->createDevice(DeviceDesc{}, device).isOk());
    REQUIRE(device != nullptr);

    BufferDesc bufferDesc{};
    bufferDesc.size = 128;
    Buffer* buffer = nullptr;
    CHECK(device->createBuffer(bufferDesc, buffer).isOk());
    CHECK(buffer != nullptr);
}

TEST_CASE("rhi.validation: catches invalid usage (null texture)")
{
    Backend* inner = nullptr;
    REQUIRE(null::createNullBackend(inner).isOk());

    validation::ValidatedBackend vb(inner);
    Device* device = nullptr;
    REQUIRE(vb.enumerateAdapters()[0]->createDevice(DeviceDesc{}, device).isOk());

    // The validation layer rejects a null texture (and logs a diagnostic) instead
    // of forwarding it to the backend.
    TextureView* view = nullptr;
    CHECK_FALSE(device->createTextureView(nullptr, TextureViewDesc{}, view).isOk());
    CHECK(view == nullptr);
}
