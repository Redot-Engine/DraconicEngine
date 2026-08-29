#include <doctest_with_main.h>

import core;
import rhi;
import rhi.null;

using namespace draco;
using namespace draco::rhi;

TEST_CASE("rhi.null: backend enumerates an adapter and creates a device")
{
    Backend* backend = nullptr;
    REQUIRE(null::createNullBackend(backend).isOk());
    REQUIRE(backend != nullptr);
    CHECK(backend->isInitialized);

    auto adapters = backend->enumerateAdapters();
    REQUIRE(adapters.size() == 1u);

    const AdapterInfo info = adapters[0]->info();
    CHECK(info.name == u8"Null Device");
    CHECK(info.type == AdapterType::Cpu);

    Device* device = nullptr;
    REQUIRE(adapters[0]->createDevice(DeviceDesc{}, device).isOk());
    REQUIRE(device != nullptr);

    backend->destroy();
}

TEST_CASE("rhi.null: device creates resources and a mappable buffer")
{
    Backend* backend = nullptr;
    REQUIRE(null::createNullBackend(backend).isOk());
    Device* device = nullptr;
    REQUIRE(backend->enumerateAdapters()[0]->createDevice(DeviceDesc{}, device).isOk());

    BufferDesc bufferDesc{};
    bufferDesc.size = 256;
    Buffer* buffer = nullptr;
    REQUIRE(device->createBuffer(bufferDesc, buffer).isOk());
    REQUIRE(buffer != nullptr);
    CHECK(buffer->map() != nullptr);   // null backend backs it with host memory
    buffer->unmap();

    Texture* texture = nullptr;
    CHECK(device->createTexture(TextureDesc{}, texture).isOk());
    CHECK(texture != nullptr);

    // Unsupported extensions degrade, they don't crash.
    MeshPipeline* mesh = nullptr;
    CHECK(device->createMeshPipeline(MeshPipelineDesc{}, mesh).code() == ErrorCode::NotSupported);

    backend->destroy();
}
