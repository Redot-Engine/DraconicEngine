/// DX12 implementation of RenderPassEncoder + MeshShaderPassExt.
/// Records render pass commands into the parent command encoder's command list.

module;

#include "DxIncludes.h"
#include <utility>
#include <span>

export module rhi.dx12:render_pass_encoder;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :buffer;
import :bind_group;
import :bind_group_layout;
import :render_pipeline;
import :pipeline_layout;
import :query_set;
import :texture;
import :texture_view;
import :descriptor_staging;
import :gpu_descriptor_heap;
import :mesh_pipeline;

using namespace draco;

export namespace draco::rhi::dx12 {

// A recorded bundle command list (+ its allocator, which must outlive GPU execution). Lives
// here (not in :render_bundle_encoder) so ExecuteBundles can use it without a module cycle.
class DxRenderBundleImpl : public RenderBundle {
public:
    DxRenderBundleImpl(ComPtr<ID3D12GraphicsCommandList> list, ComPtr<ID3D12CommandAllocator> alloc,
                       ID3D12RootSignature* rootSig = nullptr, ID3D12PipelineState* pso = nullptr)
        : m_list(std::move(list)), m_alloc(std::move(alloc)), m_rootSig(rootSig), m_pso(pso) {}
    [[nodiscard]] ID3D12GraphicsCommandList* handle()  const { return m_list.Get(); }
    [[nodiscard]] ID3D12RootSignature*       rootSig() const { return m_rootSig; }
    [[nodiscard]] ID3D12PipelineState*       pso()     const { return m_pso; }
private:
    ComPtr<ID3D12GraphicsCommandList> m_list;
    ComPtr<ID3D12CommandAllocator>    m_alloc;
    ID3D12RootSignature*              m_rootSig = nullptr;
    ID3D12PipelineState*              m_pso     = nullptr;
};

/// Pointers needed by the render pass encoder, provided by the command encoder.
/// Avoids coupling to DxCommandEncoderImpl directly.
struct DxRenderPassContext {
    ID3D12GraphicsCommandList* cmdList  = nullptr;
    DxDescriptorStaging*       srvStaging     = nullptr;
    DxDescriptorStaging*       samplerStaging = nullptr;
    DxGpuDescriptorHeap*       gpuSrvHeap     = nullptr;
    DxGpuDescriptorHeap*       gpuSamplerHeap = nullptr;
    // Indirect command signatures (cached on device).
    ID3D12CommandSignature*    drawSig            = nullptr;
    ID3D12CommandSignature*    drawIndexedSig     = nullptr;
    ID3D12CommandSignature*    dispatchMeshSig    = nullptr;
};

class DxRenderPassEncoderImpl : public RenderPassEncoder, public MeshShaderPassExt {
public:
    MeshShaderPassExt* asMeshShaderExt() noexcept override { return this; }
    explicit DxRenderPassEncoderImpl(const DxRenderPassContext& ctx)
        : m_ctx(ctx) {}

    void begin(const RenderPassDesc& desc) {
        m_desc = desc;
        m_currentPipeline     = nullptr;
        m_currentMeshPipeline = nullptr;
    }

    // ---- RenderPassEncoder: Pipeline & Binding ----

    void setPipeline(RenderPipeline* pipeline) override {
        auto* dxPipeline = static_cast<DxRenderPipelineImpl*>(pipeline);
        if (!dxPipeline) return;
        m_currentPipeline     = dxPipeline;
        m_currentMeshPipeline = nullptr;

        auto* cmdList = m_ctx.cmdList;
        cmdList->SetPipelineState(dxPipeline->handle());
        cmdList->SetGraphicsRootSignature(dxPipeline->pipelineLayout()->handle());
        cmdList->IASetPrimitiveTopology(dxPipeline->topology());

        // Re-apply cached vertex buffers with correct strides from the new pipeline.
        // DX12 vertex buffer views include stride, which comes from the pipeline.
        // If setVertexBuffer was called before setPipeline, the stride was 0.
        for (u32 slot = 0; slot < m_cachedVbCount; ++slot) {
            if (m_cachedVbs[slot].BufferLocation != 0) {
                m_cachedVbs[slot].StrideInBytes = dxPipeline->getVertexStride(slot);
                cmdList->IASetVertexBuffers(slot, 1, &m_cachedVbs[slot]);
            }
        }
    }

    void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynamicOffsets) override {
        auto* dxGroup = static_cast<DxBindGroupImpl*>(group);
        if (!dxGroup) return;

        auto* layout = getCurrentLayout();
        if (!layout) return;

        auto* cmdList  = m_ctx.cmdList;
        auto* dxLayout = static_cast<DxBindGroupLayoutImpl*>(dxGroup->layout());

        // Copy-on-bind: copy bind group's descriptors into encoder's staging region,
        // then bind from the staging offset. This makes bind group destruction safe
        // during command recording -- the GPU only references the staging copy.

        // Bind CBV/SRV/UAV table (staged).
        if (dxGroup->cbvSrvUavOffset() >= 0 && dxLayout && dxLayout->cbvSrvUavCount() > 0) {
            i32 rootIdx = layout->getCbvSrvUavRootIndex(index);
            if (rootIdx >= 0) {
                i32 stagedOffset = m_ctx.srvStaging->copyFrom(
                    static_cast<u32>(dxGroup->cbvSrvUavOffset()), dxLayout->cbvSrvUavCount());
                if (stagedOffset >= 0) {
                    auto gpuHandle = m_ctx.gpuSrvHeap->getGpuHandle(static_cast<u32>(stagedOffset));
                    cmdList->SetGraphicsRootDescriptorTable(static_cast<UINT>(rootIdx), gpuHandle);
                }
            }
        }

        // Bind sampler table (staged).
        if (dxGroup->samplerOffset() >= 0 && dxLayout && dxLayout->samplerCount() > 0) {
            i32 rootIdx = layout->getSamplerRootIndex(index);
            if (rootIdx >= 0) {
                i32 stagedOffset = m_ctx.samplerStaging->copyFrom(
                    static_cast<u32>(dxGroup->samplerOffset()), dxLayout->samplerCount());
                if (stagedOffset >= 0) {
                    auto gpuHandle = m_ctx.gpuSamplerHeap->getGpuHandle(static_cast<u32>(stagedOffset));
                    cmdList->SetGraphicsRootDescriptorTable(static_cast<UINT>(rootIdx), gpuHandle);
                }
            }
        }

        // Bind dynamic offset root descriptors (not staged -- uses GPU virtual addresses).
        auto dynAddrs = dxGroup->dynamicGpuAddresses();
        usize dynOffsetIdx = 0;
        for (usize i = 0; i < layout->dynamicRootEntries().size(); ++i) {
            const auto& entry = layout->dynamicRootEntries()[i];
            if (entry.groupIndex != index) continue;
            if (entry.dynamicIndex >= static_cast<u32>(dynAddrs.size())) continue;

            u64 gpuAddr = dynAddrs[entry.dynamicIndex];
            if (dynOffsetIdx < dynamicOffsets.size())
                gpuAddr += static_cast<u64>(dynamicOffsets[dynOffsetIdx]);
            ++dynOffsetIdx;

            switch (entry.paramType) {
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
                cmdList->SetGraphicsRootConstantBufferView(static_cast<UINT>(entry.rootParamIndex), gpuAddr);
                break;
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
                cmdList->SetGraphicsRootShaderResourceView(static_cast<UINT>(entry.rootParamIndex), gpuAddr);
                break;
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
                cmdList->SetGraphicsRootUnorderedAccessView(static_cast<UINT>(entry.rootParamIndex), gpuAddr);
                break;
            default: break;
            }
        }
    }

    void setPushConstants(ShaderStage /*stages*/, u32 offset, u32 size, const void* data) override {
        auto* layout = getCurrentLayout();
        if (!layout || layout->pushConstantRootIndex() < 0) return;

        m_ctx.cmdList->SetGraphicsRoot32BitConstants(
            static_cast<UINT>(layout->pushConstantRootIndex()),
            size / 4, data, offset / 4);
    }

    // ---- Vertex & Index Buffers ----

    void setVertexBuffer(u32 slot, Buffer* buffer, u64 offset) override {
        auto* dxBuf = static_cast<DxBufferImpl*>(buffer);
        if (!dxBuf || slot >= 8) return;

        u32 stride = m_currentPipeline ? m_currentPipeline->getVertexStride(slot) : 0;

        D3D12_VERTEX_BUFFER_VIEW view{};
        view.BufferLocation = dxBuf->gpuAddress() + offset;
        view.SizeInBytes    = static_cast<UINT>(dxBuf->desc.size - offset);
        view.StrideInBytes  = stride;

        // Cache for re-application when pipeline changes.
        m_cachedVbs[slot] = view;
        if (slot >= m_cachedVbCount) m_cachedVbCount = slot + 1;

        m_ctx.cmdList->IASetVertexBuffers(slot, 1, &view);
    }

    void setIndexBuffer(Buffer* buffer, IndexFormat format, u64 offset) override {
        auto* dxBuf = static_cast<DxBufferImpl*>(buffer);
        if (!dxBuf) return;

        D3D12_INDEX_BUFFER_VIEW view{};
        view.BufferLocation = dxBuf->gpuAddress() + offset;
        view.SizeInBytes    = static_cast<UINT>(dxBuf->desc.size - offset);
        view.Format         = toDxgiIndexFormat(format);

        m_ctx.cmdList->IASetIndexBuffer(&view);
    }

    // ---- Dynamic State ----

    void setViewport(f32 x, f32 y, f32 w, f32 h, f32 minDepth, f32 maxDepth) override {
        D3D12_VIEWPORT viewport{};
        viewport.TopLeftX = x;
        viewport.TopLeftY = y;
        viewport.Width    = w;
        viewport.Height   = h;
        viewport.MinDepth = minDepth;
        viewport.MaxDepth = maxDepth;

        m_ctx.cmdList->RSSetViewports(1, &viewport);
    }

    void setScissor(i32 x, i32 y, u32 w, u32 h) override {
        D3D12_RECT rect{};
        rect.left   = x;
        rect.top    = y;
        rect.right  = x + static_cast<LONG>(w);
        rect.bottom = y + static_cast<LONG>(h);

        m_ctx.cmdList->RSSetScissorRects(1, &rect);
    }

    void setBlendConstant(f32 r, f32 g, f32 b, f32 a) override {
        f32 color[4] = { r, g, b, a };
        m_ctx.cmdList->OMSetBlendFactor(color);
    }

    void setStencilReference(u32 reference) override {
        m_ctx.cmdList->OMSetStencilRef(reference);
    }

    // ---- Draw Commands ----

    void draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override {
        m_ctx.cmdList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void drawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 baseVertex, u32 firstInstance) override {
        m_ctx.cmdList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
    }

    void drawIndirect(Buffer* buffer, u64 offset, u32 drawCount, u32 stride) override {
        auto* dxBuf = static_cast<DxBufferImpl*>(buffer);
        if (!dxBuf) return;

        auto* sig = m_ctx.drawSig;
        if (!sig) return;

        u32 actualStride = (stride > 0) ? stride : 16; // sizeof(D3D12_DRAW_ARGUMENTS)
        for (u32 i = 0; i < drawCount; ++i) {
            m_ctx.cmdList->ExecuteIndirect(sig, 1, dxBuf->handle(),
                offset + static_cast<u64>(i) * actualStride, nullptr, 0);
        }
    }

    void drawIndexedIndirect(Buffer* buffer, u64 offset, u32 drawCount, u32 stride) override {
        auto* dxBuf = static_cast<DxBufferImpl*>(buffer);
        if (!dxBuf) return;

        auto* sig = m_ctx.drawIndexedSig;
        if (!sig) return;

        u32 actualStride = (stride > 0) ? stride : 20; // sizeof(D3D12_DRAW_INDEXED_ARGUMENTS)
        for (u32 i = 0; i < drawCount; ++i) {
            m_ctx.cmdList->ExecuteIndirect(sig, 1, dxBuf->handle(),
                offset + static_cast<u64>(i) * actualStride, nullptr, 0);
        }
    }

    // ---- Queries ----

    void writeTimestamp(QuerySet* querySet, u32 index) override {
        auto* qs = static_cast<DxQuerySetImpl*>(querySet);
        if (qs) m_ctx.cmdList->EndQuery(qs->handle(), D3D12_QUERY_TYPE_TIMESTAMP, index);
    }

    void beginOcclusionQuery(QuerySet* querySet, u32 index) override {
        auto* qs = static_cast<DxQuerySetImpl*>(querySet);
        if (qs) m_ctx.cmdList->BeginQuery(qs->handle(), D3D12_QUERY_TYPE_OCCLUSION, index);
    }

    void endOcclusionQuery(QuerySet* querySet, u32 index) override {
        auto* qs = static_cast<DxQuerySetImpl*>(querySet);
        if (qs) m_ctx.cmdList->EndQuery(qs->handle(), D3D12_QUERY_TYPE_OCCLUSION, index);
    }

    // ---- MeshShaderPassExt ----

    void setMeshPipeline(MeshPipeline* pipeline) override {
        auto* dxPipeline = static_cast<DxMeshPipelineImpl*>(pipeline);
        if (!dxPipeline) return;
        m_currentMeshPipeline = dxPipeline;
        m_currentPipeline     = nullptr; // clear regular pipeline

        auto* cmdList = m_ctx.cmdList;
        cmdList->SetPipelineState(dxPipeline->handle());
        cmdList->SetGraphicsRootSignature(dxPipeline->pipelineLayout()->handle());
    }

    void drawMeshTasks(u32 groupCountX, u32 groupCountY, u32 groupCountZ) override {
        // Need ID3D12GraphicsCommandList6 for DispatchMesh.
        ID3D12GraphicsCommandList6* cmdList6 = nullptr;
        HRESULT hr = m_ctx.cmdList->QueryInterface(IID_PPV_ARGS(&cmdList6));
        if (SUCCEEDED(hr) && cmdList6) {
            cmdList6->DispatchMesh(groupCountX, groupCountY, groupCountZ);
            cmdList6->Release();
        }
    }

    void drawMeshTasksIndirect(Buffer* buffer, u64 offset, u32 drawCount, u32 stride) override {
        auto* dxBuf = static_cast<DxBufferImpl*>(buffer);
        if (!dxBuf) return;

        auto* sig = m_ctx.dispatchMeshSig;
        if (!sig) return;

        u32 actualStride = (stride > 0) ? stride : 12; // sizeof(D3D12_DISPATCH_MESH_ARGUMENTS): 3 x u32
        for (u32 i = 0; i < drawCount; ++i) {
            m_ctx.cmdList->ExecuteIndirect(sig, 1, dxBuf->handle(),
                offset + static_cast<u64>(i) * actualStride, nullptr, 0);
        }
    }

    void drawMeshTasksIndirectCount(Buffer* buffer, u64 offset,
                                    Buffer* countBuffer, u64 countOffset,
                                    u32 maxDrawCount, u32 /*stride*/) override {
        auto* dxBuf      = static_cast<DxBufferImpl*>(buffer);
        auto* dxCountBuf = static_cast<DxBufferImpl*>(countBuffer);
        if (!dxBuf || !dxCountBuf) return;

        auto* sig = m_ctx.dispatchMeshSig;
        if (!sig) return;

        m_ctx.cmdList->ExecuteIndirect(sig, maxDrawCount, dxBuf->handle(),
            offset, dxCountBuf->handle(), countOffset);
    }

    // ---- End ----

    void executeBundles(std::span<RenderBundle* const> bundles) override {
        for (usize i = 0; i < bundles.size(); ++i) {
            if (auto* b = static_cast<DxRenderBundleImpl*>(bundles[i])) {
                // DX12 requires the parent command list to have the same root signature
                // and PSO set before ExecuteBundle.
                if (b->rootSig()) m_ctx.cmdList->SetGraphicsRootSignature(b->rootSig());
                if (b->pso())     m_ctx.cmdList->SetPipelineState(b->pso());
                m_ctx.cmdList->ExecuteBundle(b->handle());
            }
        }
    }

    void end() override {
        // Timestamp at pass end.
        if (m_desc.timestampQuerySet) {
            auto* qs = static_cast<DxQuerySetImpl*>(m_desc.timestampQuerySet);
            if (qs)
                m_ctx.cmdList->EndQuery(qs->handle(), D3D12_QUERY_TYPE_TIMESTAMP, m_desc.endTimestampIndex);
        }

        // MSAA resolve: resolve multisampled color attachments to their resolve targets.
        const auto& colorAtts = m_desc.colorAttachments;
        for (usize i = 0; i < colorAtts.size(); ++i) {
            const auto& ca = colorAtts[i];
            if (!ca.resolveTarget) continue;

            auto* srcView = static_cast<DxTextureViewImpl*>(ca.view);
            auto* dstView = static_cast<DxTextureViewImpl*>(ca.resolveTarget);
            if (!srcView || !dstView) continue;

            auto* srcTex = srcView->dxTexture();
            auto* dstTex = dstView->dxTexture();

            TextureFormat format = srcView->format();
            if (format == TextureFormat::Undefined)
                format = srcView->dxTexture()->desc.format;
            DXGI_FORMAT dxgiFormat = toDxgiFormat(format);

            // Transition src to resolve source, dst to resolve dest.
            D3D12_RESOURCE_BARRIER barriers[2]{};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource   = srcTex->handle();
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[1].Transition.pResource   = dstTex->handle();
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RESOLVE_DEST;
            barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            m_ctx.cmdList->ResourceBarrier(2, barriers);

            m_ctx.cmdList->ResolveSubresource(dstTex->handle(), 0, srcTex->handle(), 0, dxgiFormat);

            // Transition back.
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
            barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
            barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

            m_ctx.cmdList->ResourceBarrier(2, barriers);
        }

        m_currentPipeline     = nullptr;
        m_currentMeshPipeline = nullptr;
    }

    // Accessors for bundle recording: the last pipeline/root-sig set on this encoder.
    [[nodiscard]] ID3D12PipelineState*  currentPso() const { return m_currentPipeline ? m_currentPipeline->handle() : nullptr; }
    [[nodiscard]] ID3D12RootSignature*  currentRootSig() const {
        auto* l = m_currentPipeline ? m_currentPipeline->pipelineLayout() : nullptr;
        return l ? l->handle() : nullptr;
    }

private:
    DxPipelineLayoutImpl* getCurrentLayout() {
        if (m_currentPipeline)
            return m_currentPipeline->pipelineLayout();
        if (m_currentMeshPipeline)
            return m_currentMeshPipeline->pipelineLayout();
        return nullptr;
    }

    DxRenderPassContext    m_ctx;
    RenderPassDesc         m_desc{};
    DxRenderPipelineImpl*  m_currentPipeline     = nullptr;
    DxMeshPipelineImpl*    m_currentMeshPipeline = nullptr;

    // Cached vertex buffer views for re-application on pipeline change.
    D3D12_VERTEX_BUFFER_VIEW m_cachedVbs[8]{};
    u32                      m_cachedVbCount = 0;
};

} // namespace draco::rhi::dx12
