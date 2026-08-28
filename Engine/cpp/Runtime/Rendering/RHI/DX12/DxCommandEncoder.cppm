/// DX12 implementation of CommandEncoder + RayTracingEncoderExt.
/// Wraps an ID3D12GraphicsCommandList for recording commands.

module;

#include "DxIncludes.h"
#include <vector>
#include <span>
#include <string_view>

#include <algorithm>
#include <cstring>

export module rhi.dx12:command_encoder;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :buffer;
import :texture;
import :texture_view;
import :bind_group;
import :bind_group_layout;
import :pipeline_layout;
import :query_set;
import :command_buffer;
import :command_pool;
import :descriptor_heap;
import :gpu_descriptor_heap;
import :descriptor_staging;
import :render_pipeline;
import :render_bundle_encoder;
import :compute_pipeline;
import :accel_struct;
import :ray_tracing_pipeline;
import :render_pass_encoder;
import :compute_pass_encoder;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxDeviceImpl; // forward

/// DX12 implementation of CommandEncoder and RayTracingEncoderExt.
/// Wraps an ID3D12GraphicsCommandList for recording commands outside of
/// render/compute passes (barriers, copies, queries, RT builds).
class DxCommandEncoderImpl : public CommandEncoder, public RayTracingEncoderExt {
public:
    RayTracingEncoderExt* asRayTracingExt() noexcept override { return this; }
    DxCommandEncoderImpl(DxDeviceImpl* device, ID3D12GraphicsCommandList* cmdList,
                         DxCommandPoolImpl* pool, const DxRenderPassContext& rpeCtx,
                         const DxComputePassContext& cpeCtx)
        : m_device(device), m_cmdList(cmdList), m_pool(pool),
          m_gpuSrvHeap(rpeCtx.gpuSrvHeap), m_gpuSamplerHeap(rpeCtx.gpuSamplerHeap),
          m_rpe(rpeCtx), m_cpe(cpeCtx), m_rpeCtx(rpeCtx) {}

    ~DxCommandEncoderImpl() override { for (auto* e : m_bundleEncoders) delete e; }

    // ================================================================
    // CommandEncoder interface
    // ================================================================

    // ---- Render Pass ----

    RenderPassEncoder* beginRenderPass(const RenderPassDesc& desc) override {
        ensureDescriptorHeaps();

        // Timestamp at pass begin.
        if (desc.timestampQuerySet) {
            if (auto* qs = static_cast<DxQuerySetImpl*>(desc.timestampQuerySet))
                m_cmdList->EndQuery(qs->handle(), D3D12_QUERY_TYPE_TIMESTAMP, desc.beginTimestampIndex);
        }

        // Collect render target views.
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8]{};
        u32 rtvCount = 0;
        const auto& colorAtts = desc.colorAttachments;
        for (usize i = 0; i < colorAtts.size() && i < 8; ++i) {
            const auto& att = colorAtts[i];
            if (auto* dxView = static_cast<DxTextureViewImpl*>(att.view)) {
                rtvHandles[i] = dxView->getRtv();
                ++rtvCount;
            }
        }

        // Depth/stencil view.
        D3D12_CPU_DESCRIPTOR_HANDLE dsvStorage{};
        D3D12_CPU_DESCRIPTOR_HANDLE* dsvPtr = nullptr;
        if (desc.depthStencilAttachment.has_value()) {
            const auto& dsAttach = *desc.depthStencilAttachment;
            if (auto* dxView = static_cast<DxTextureViewImpl*>(dsAttach.view)) {
                dsvStorage = dxView->getDsv();
                dsvPtr = &dsvStorage;
            }
        }

        m_cmdList->OMSetRenderTargets(rtvCount, rtvHandles, FALSE, dsvPtr);

        // Clear render targets.
        for (usize i = 0; i < colorAtts.size() && i < 8; ++i) {
            const auto& att = colorAtts[i];
            if (att.loadOp == LoadOp::Clear) {
                FLOAT color[4] = { att.clearValue.r, att.clearValue.g, att.clearValue.b, att.clearValue.a };
                m_cmdList->ClearRenderTargetView(rtvHandles[i], color, 0, nullptr);
            }
        }

        // Clear depth/stencil.
        if (desc.depthStencilAttachment.has_value() && dsvPtr) {
            const auto& dsAttach = *desc.depthStencilAttachment;
            D3D12_CLEAR_FLAGS clearFlags = static_cast<D3D12_CLEAR_FLAGS>(0);
            bool needsClear = false;
            if (dsAttach.depthLoadOp == LoadOp::Clear) {
                clearFlags = static_cast<D3D12_CLEAR_FLAGS>(
                    static_cast<u32>(clearFlags) | static_cast<u32>(D3D12_CLEAR_FLAG_DEPTH));
                needsClear = true;
            }
            if (dsAttach.stencilLoadOp == LoadOp::Clear) {
                clearFlags = static_cast<D3D12_CLEAR_FLAGS>(
                    static_cast<u32>(clearFlags) | static_cast<u32>(D3D12_CLEAR_FLAG_STENCIL));
                needsClear = true;
            }
            if (needsClear)
                m_cmdList->ClearDepthStencilView(*dsvPtr, clearFlags,
                    dsAttach.depthClearValue, static_cast<UINT8>(dsAttach.stencilClearValue), 0, nullptr);
        }

        m_rpe.begin(desc);
        return &m_rpe;
    }

    // ---- Compute Pass ----

    ComputePassEncoder* beginComputePass(std::u8string_view) override {
        ensureDescriptorHeaps();
        m_cpe.begin();
        return &m_cpe;
    }

    RenderBundleEncoder* createRenderBundleEncoder(const RenderBundleDesc& /*desc*/) override {
        ComPtr<ID3D12Device> dev;
        m_cmdList->GetDevice(IID_PPV_ARGS(&dev));
        if (!dev) return nullptr;
        ComPtr<ID3D12CommandAllocator> alloc;
        if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&alloc)))) return nullptr;
        ComPtr<ID3D12GraphicsCommandList> list;
        if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, alloc.Get(), nullptr, IID_PPV_ARGS(&list))))
            return nullptr;

        // Bundles inherit the parent's descriptor heaps; bind matching heaps on the bundle.
        ensureDescriptorHeaps();
        ID3D12DescriptorHeap* heaps[2]; UINT n = 0;
        if (m_gpuSrvHeap)     heaps[n++] = m_gpuSrvHeap->heap();
        if (m_gpuSamplerHeap) heaps[n++] = m_gpuSamplerHeap->heap();
        if (n > 0) list->SetDescriptorHeaps(n, heaps);

        DxRenderPassContext ctx = m_rpeCtx;
        ctx.cmdList = list.Get();
        auto* enc = new DxRenderBundleEncoderImpl(ctx, list, alloc);
        m_bundleEncoders.push_back(enc);   // owned: freed in this encoder's destructor
        return enc;
    }

    // ---- Barriers ----

    void barrier(const BarrierGroup& group) override {
        usize totalBarriers = group.bufferBarriers.size() + group.textureBarriers.size()
                            + group.memoryBarriers.size();
        if (totalBarriers == 0) return;

        std::vector<D3D12_RESOURCE_BARRIER> dxBarriers;
        dxBarriers.reserve(totalBarriers);

        // Buffer barriers.
        for (usize i = 0; i < group.bufferBarriers.size(); ++i) {
            const auto& bb = group.bufferBarriers[i];
            auto* dxBuf = static_cast<DxBufferImpl*>(bb.buffer);
            if (!dxBuf) continue;

            auto oldState = toResourceStates(bb.oldState);
            auto newState = toResourceStates(bb.newState);
            if (oldState == newState) continue;

            D3D12_RESOURCE_BARRIER b{};
            b.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            b.Transition.pResource   = dxBuf->handle();
            b.Transition.StateBefore = oldState;
            b.Transition.StateAfter  = newState;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            dxBuf->setState(newState);
            dxBarriers.push_back(b);
        }

        // Texture barriers: coalesce chained transitions per (resource, subresource).
        // The barrier solver can emit multiple barriers for the same texture in one
        // batch (e.g. ParticlePass declares both ReadDepth and ReadTexture on SceneDepth,
        // producing DEPTH_WRITE->DEPTH_READ then DEPTH_READ->SHADER_READ). D3D12 processes
        // all barriers in one ResourceBarrier() call simultaneously, so the second barrier's
        // StateBefore won't match reality. Fix: coalesce A->B, B->C into a single A->C.
        {
            struct CoalescedEntry {
                ID3D12Resource*       resource;
                u32                   subresource;
                D3D12_RESOURCE_STATES firstBefore;
                D3D12_RESOURCE_STATES lastAfter;
            };
            std::vector<CoalescedEntry> coalesced;
            coalesced.reserve(group.textureBarriers.size());

            for (usize i = 0; i < group.textureBarriers.size(); ++i) {
                const auto& tb = group.textureBarriers[i];
                auto* dxTex = static_cast<DxTextureImpl*>(tb.texture);
                if (!dxTex) continue;

                auto newState = toResourceStates(tb.newState, dxTex->desc.format);
                bool isWholeResource = (tb.mipLevelCount == ~0u) && (tb.arrayLayerCount == ~0u);

                if (isWholeResource) {
                    auto resolvedOldState = dxTex->currentState();
                    if (resolvedOldState == newState) continue;

                    // Check if we already have an entry for this resource+subresource.
                    bool found = false;
                    for (auto& entry : coalesced) {
                        if (entry.resource == dxTex->handle()
                            && entry.subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
                            entry.lastAfter = newState;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        coalesced.push_back({ dxTex->handle(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                              resolvedOldState, newState });

                    dxTex->setState(newState);
                } else {
                    // Per-subresource: expand range and coalesce each subresource individually.
                    u32 mipCount   = dxTex->desc.mipLevelCount;
                    u32 layerCount = std::max(
                        (dxTex->desc.dimension == TextureDimension::Texture3D)
                            ? dxTex->desc.depth : dxTex->desc.arrayLayerCount, 1u);
                    u32 baseMip  = tb.baseMipLevel;
                    u32 mipEnd   = std::min(baseMip + tb.mipLevelCount, mipCount);
                    u32 baseLayer= tb.baseArrayLayer;
                    u32 layerEnd = std::min(baseLayer + tb.arrayLayerCount, layerCount);

                    for (u32 layer = baseLayer; layer < layerEnd; ++layer) {
                        for (u32 mip = baseMip; mip < mipEnd; ++mip) {
                            auto resolvedOldState = dxTex->getSubresourceState(mip, layer);
                            u32 sub = mip + layer * mipCount;
                            if (resolvedOldState == newState) continue;

                            bool found = false;
                            for (auto& entry : coalesced) {
                                if (entry.resource == dxTex->handle() && entry.subresource == sub) {
                                    entry.lastAfter = newState;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                                coalesced.push_back({ dxTex->handle(), sub, resolvedOldState, newState });
                        }
                    }
                    dxTex->setSubresourceState(baseMip, tb.mipLevelCount, baseLayer, tb.arrayLayerCount, newState);
                }
            }

            // Phase 2: emit one D3D12 barrier per coalesced entry.
            for (const auto& entry : coalesced) {
                if (entry.firstBefore == entry.lastAfter) continue; // A->B->A cancelled out
                D3D12_RESOURCE_BARRIER b{};
                b.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                b.Transition.pResource   = entry.resource;
                b.Transition.StateBefore = entry.firstBefore;
                b.Transition.StateAfter  = entry.lastAfter;
                b.Transition.Subresource = entry.subresource;
                dxBarriers.push_back(b);
            }
        }

        // Memory barriers -> UAV barriers.
        for (usize i = 0; i < group.memoryBarriers.size(); ++i) {
            D3D12_RESOURCE_BARRIER b{};
            b.Type  = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            b.UAV.pResource = nullptr; // global UAV barrier
            dxBarriers.push_back(b);
        }

        if (!dxBarriers.empty())
            m_cmdList->ResourceBarrier(static_cast<UINT>(dxBarriers.size()), dxBarriers.data());
    }

    // ---- Copy Operations ----

    void copyBufferToBuffer(Buffer* src, u64 srcOffset, Buffer* dst, u64 dstOffset, u64 size) override {
        auto* dxSrc = static_cast<DxBufferImpl*>(src);
        auto* dxDst = static_cast<DxBufferImpl*>(dst);
        if (!dxSrc || !dxDst) return;
        m_cmdList->CopyBufferRegion(dxDst->handle(), dstOffset, dxSrc->handle(), srcOffset, size);
    }

    void copyBufferToTexture(Buffer* src, Texture* dst, const BufferTextureCopyRegion& region) override {
        auto* dxSrc = static_cast<DxBufferImpl*>(src);
        auto* dxTex = static_cast<DxTextureImpl*>(dst);
        if (!dxSrc || !dxTex) return;

        u32 subresource = region.textureMipLevel + region.textureArrayLayer * dxTex->desc.mipLevelCount;

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = dxSrc->handle();
        srcLoc.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint.Offset             = region.bufferOffset;
        srcLoc.PlacedFootprint.Footprint.Format   = toDxgiFormat(dxTex->desc.format);
        srcLoc.PlacedFootprint.Footprint.Width    = region.textureExtent.width;
        srcLoc.PlacedFootprint.Footprint.Height   = region.textureExtent.height;
        srcLoc.PlacedFootprint.Footprint.Depth    = region.textureExtent.depth;
        srcLoc.PlacedFootprint.Footprint.RowPitch = region.bytesPerRow;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource        = dxTex->handle();
        dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = subresource;

        m_cmdList->CopyTextureRegion(&dstLoc,
            region.textureOrigin.x, region.textureOrigin.y, region.textureOrigin.z,
            &srcLoc, nullptr);
    }

    void copyTextureToBuffer(Texture* src, Buffer* dst, const BufferTextureCopyRegion& region) override {
        auto* dxTex = static_cast<DxTextureImpl*>(src);
        auto* dxDst = static_cast<DxBufferImpl*>(dst);
        if (!dxTex || !dxDst) return;

        u32 subresource = region.textureMipLevel + region.textureArrayLayer * dxTex->desc.mipLevelCount;

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource        = dxTex->handle();
        srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = subresource;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = dxDst->handle();
        dstLoc.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstLoc.PlacedFootprint.Offset             = region.bufferOffset;
        dstLoc.PlacedFootprint.Footprint.Format   = toDxgiFormat(dxTex->desc.format);
        dstLoc.PlacedFootprint.Footprint.Width    = region.textureExtent.width;
        dstLoc.PlacedFootprint.Footprint.Height   = region.textureExtent.height;
        dstLoc.PlacedFootprint.Footprint.Depth    = region.textureExtent.depth;
        dstLoc.PlacedFootprint.Footprint.RowPitch = region.bytesPerRow;

        D3D12_BOX srcBox{};
        srcBox.left   = region.textureOrigin.x;
        srcBox.top    = region.textureOrigin.y;
        srcBox.front  = region.textureOrigin.z;
        srcBox.right  = region.textureOrigin.x + region.textureExtent.width;
        srcBox.bottom = region.textureOrigin.y + region.textureExtent.height;
        srcBox.back   = region.textureOrigin.z + region.textureExtent.depth;

        m_cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);
    }

    void copyTextureToTexture(Texture* src, Texture* dst, const TextureCopyRegion& region) override {
        auto* dxSrc = static_cast<DxTextureImpl*>(src);
        auto* dxDst = static_cast<DxTextureImpl*>(dst);
        if (!dxSrc || !dxDst) return;

        u32 srcSub = region.srcMipLevel + region.srcArrayLayer * dxSrc->desc.mipLevelCount;
        u32 dstSub = region.dstMipLevel + region.dstArrayLayer * dxDst->desc.mipLevelCount;

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource        = dxSrc->handle();
        srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = srcSub;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource        = dxDst->handle();
        dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = dstSub;

        D3D12_BOX srcBox{};
        srcBox.left   = 0;
        srcBox.top    = 0;
        srcBox.front  = 0;
        srcBox.right  = region.extent.width;
        srcBox.bottom = region.extent.height;
        srcBox.back   = region.extent.depth;

        m_cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);
    }

    // ---- Blit & Mipmap Generation ----

    void blit(Texture* src, Texture* dst) override {
        auto* dxSrc = static_cast<DxTextureImpl*>(src);
        auto* dxDst = static_cast<DxTextureImpl*>(dst);
        if (!dxSrc || !dxDst) return;

        DXGI_FORMAT dxgiFormat = toDxgiFormat(dxDst->desc.format);

        // Caller has set textures to CopySrc/CopyDst. Internally transition to SRV/RTV for blit.
        D3D12_RESOURCE_BARRIER barriers[2]{};

        barriers[0].Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource   = dxSrc->handle();
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                                           | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        barriers[1].Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource   = dxDst->handle();
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

        m_cmdList->ResourceBarrier(2, barriers);

        blitSubresource(dxSrc, 0, dxDst, 0, dxDst->desc.width, dxDst->desc.height, dxgiFormat);

        // Transition back to CopySrc/CopyDst.
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                                           | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;

        m_cmdList->ResourceBarrier(2, barriers);
    }

    void generateMipmaps(Texture* texture) override {
        auto* dxTex = static_cast<DxTextureImpl*>(texture);
        if (!dxTex) return;

        const auto& d = dxTex->desc;
        if (d.mipLevelCount <= 1) return;

        DXGI_FORMAT dxgiFormat = toDxgiFormat(d.format);

        // Texture enters in CopySrc+CopyDst state (DX12: all subresources in COPY_SOURCE).
        // For each mip: transition src->SRV, dst->RTV, blit, restore both->COPY_SOURCE.
        for (u32 mip = 1; mip < d.mipLevelCount; ++mip) {
            u32 dstWidth  = std::max(1u, d.width >> mip);
            u32 dstHeight = std::max(1u, d.height >> mip);

            D3D12_RESOURCE_BARRIER barriers[2]{};

            barriers[0].Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource   = dxTex->handle();
            barriers[0].Transition.Subresource = mip - 1;
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                                               | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            barriers[1].Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[1].Transition.pResource   = dxTex->handle();
            barriers[1].Transition.Subresource = mip;
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

            m_cmdList->ResourceBarrier(2, barriers);

            blitSubresource(dxTex, mip - 1, dxTex, mip, dstWidth, dstHeight, dxgiFormat);

            // Post-blit: restore both to COPY_SOURCE.
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                                               | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;

            m_cmdList->ResourceBarrier(2, barriers);
        }
    }

    // ---- MSAA Resolve ----

    void resolveTexture(Texture* src, Texture* dst) override {
        auto* dxSrc = static_cast<DxTextureImpl*>(src);
        auto* dxDst = static_cast<DxTextureImpl*>(dst);
        if (!dxSrc || !dxDst) return;

        DXGI_FORMAT dxgiFormat = toDxgiFormat(dxDst->desc.format);

        // Transition src -> RESOLVE_SOURCE, dst -> RESOLVE_DEST.
        D3D12_RESOURCE_BARRIER barriers[2]{};

        barriers[0].Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource   = dxSrc->handle();
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;

        barriers[1].Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource   = dxDst->handle();
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_RESOLVE_DEST;

        m_cmdList->ResourceBarrier(2, barriers);

        m_cmdList->ResolveSubresource(dxDst->handle(), 0, dxSrc->handle(), 0, dxgiFormat);

        // Transition back.
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
        barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
        barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;

        m_cmdList->ResourceBarrier(2, barriers);
    }

    // ---- Queries ----

    void resetQuerySet(QuerySet*, u32, u32) override {
        // DX12 does not require explicit query reset -- queries are implicitly reset when written.
    }

    void writeTimestamp(QuerySet* querySet, u32 index) override {
        if (auto* qs = static_cast<DxQuerySetImpl*>(querySet))
            m_cmdList->EndQuery(qs->handle(), D3D12_QUERY_TYPE_TIMESTAMP, index);
    }

    void resolveQuerySet(QuerySet* querySet, u32 first, u32 count, Buffer* dst, u64 dstOffset) override {
        auto* qs    = static_cast<DxQuerySetImpl*>(querySet);
        auto* dxDst = static_cast<DxBufferImpl*>(dst);
        if (!qs || !dxDst) return;
        m_cmdList->ResolveQueryData(qs->handle(), DxQuerySetImpl::toDxQueryType(qs->type),
            first, count, dxDst->handle(), dstOffset);
    }

    // ---- Debug Labels ----

    void beginDebugLabel(std::u8string_view, f32, f32, f32, f32) override {
        // PIX events would go here; no-op without PIX runtime.
    }

    void endDebugLabel() override {}

    void insertDebugLabel(std::u8string_view, f32, f32, f32, f32) override {}

    // ---- Finish ----

    CommandBuffer* finish() override {
        m_cmdList->Close();
        auto* cb = new DxCommandBufferImpl(m_cmdList);
        m_pool->trackCommandBuffer(cb);
        return cb;
    }

    // ================================================================
    // RayTracingEncoderExt interface
    // ================================================================

    void buildBottomLevelAccelStruct(AccelStruct* dst, Buffer* scratchBuffer, u64 scratchOffset,
        std::span<const AccelStructGeometryTriangles> tris,
        std::span<const AccelStructGeometryAABBs> aabbs) override
    {
        auto* dxAs      = static_cast<DxAccelStructImpl*>(dst);
        auto* dxScratch = static_cast<DxBufferImpl*>(scratchBuffer);
        if (!dxAs || !dxScratch) return;

        // Query ID3D12GraphicsCommandList4 for RT support.
        ComPtr<ID3D12GraphicsCommandList4> cmdList4;
        if (FAILED(m_cmdList->QueryInterface(IID_PPV_ARGS(&cmdList4))) || !cmdList4) return;

        usize totalGeoms = tris.size() + aabbs.size();
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geomDescs(totalGeoms);
        usize idx = 0;

        for (usize i = 0; i < tris.size(); ++i) {
            const auto& t = tris[i];
            geomDescs[idx] = {};
            geomDescs[idx].Type  = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geomDescs[idx].Flags = toGeometryFlags(t.flags);

            auto& tri = geomDescs[idx].Triangles;

            if (auto* vb = static_cast<DxBufferImpl*>(t.vertexBuffer)) {
                tri.VertexBuffer.StartAddress  = vb->gpuAddress() + t.vertexOffset;
                tri.VertexBuffer.StrideInBytes = t.vertexStride;
                tri.VertexCount                = t.vertexCount;
                tri.VertexFormat               = toDxgiVertexFormat(t.vertexFormat);
            }

            if (t.indexBuffer) {
                if (auto* ib = static_cast<DxBufferImpl*>(t.indexBuffer)) {
                    tri.IndexBuffer = ib->gpuAddress() + t.indexOffset;
                    tri.IndexCount  = t.indexCount;
                    tri.IndexFormat = (t.indexFormat == IndexFormat::UInt16)
                        ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
                }
            } else {
                tri.IndexFormat = DXGI_FORMAT_UNKNOWN;
            }

            if (t.transformBuffer) {
                if (auto* tb = static_cast<DxBufferImpl*>(t.transformBuffer))
                    tri.Transform3x4 = tb->gpuAddress() + t.transformOffset;
            }

            ++idx;
        }

        for (usize i = 0; i < aabbs.size(); ++i) {
            const auto& a = aabbs[i];
            geomDescs[idx] = {};
            geomDescs[idx].Type  = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
            geomDescs[idx].Flags = toGeometryFlags(a.flags);

            if (auto* ab = static_cast<DxBufferImpl*>(a.aabbBuffer)) {
                geomDescs[idx].AABBs.AABBs.StartAddress  = ab->gpuAddress() + a.offset;
                geomDescs[idx].AABBs.AABBs.StrideInBytes = a.stride;
                geomDescs[idx].AABBs.AABBCount           = a.count;
            }

            ++idx;
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
        buildDesc.DestAccelerationStructureData    = dxAs->deviceAddress();
        buildDesc.ScratchAccelerationStructureData = dxScratch->gpuAddress() + scratchOffset;
        buildDesc.Inputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        buildDesc.Inputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        buildDesc.Inputs.NumDescs       = static_cast<UINT>(totalGeoms);
        buildDesc.Inputs.DescsLayout    = D3D12_ELEMENTS_LAYOUT_ARRAY;
        buildDesc.Inputs.pGeometryDescs = geomDescs.data();

        cmdList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    }

    void buildTopLevelAccelStruct(AccelStruct* dst, Buffer* scratchBuffer, u64 scratchOffset,
        Buffer* instanceBuffer, u64 instanceOffset, u32 instanceCount) override
    {
        auto* dxAs        = static_cast<DxAccelStructImpl*>(dst);
        auto* dxScratch   = static_cast<DxBufferImpl*>(scratchBuffer);
        auto* dxInstances = static_cast<DxBufferImpl*>(instanceBuffer);
        if (!dxAs || !dxScratch || !dxInstances) return;

        ComPtr<ID3D12GraphicsCommandList4> cmdList4;
        if (FAILED(m_cmdList->QueryInterface(IID_PPV_ARGS(&cmdList4))) || !cmdList4) return;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
        buildDesc.DestAccelerationStructureData    = dxAs->deviceAddress();
        buildDesc.ScratchAccelerationStructureData = dxScratch->gpuAddress() + scratchOffset;
        buildDesc.Inputs.Type          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        buildDesc.Inputs.Flags         = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        buildDesc.Inputs.NumDescs      = instanceCount;
        buildDesc.Inputs.DescsLayout   = D3D12_ELEMENTS_LAYOUT_ARRAY;
        buildDesc.Inputs.InstanceDescs = dxInstances->gpuAddress() + instanceOffset;

        cmdList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    }

    void setRayTracingPipeline(RayTracingPipeline* pipeline) override {
        m_currentRtPipeline = static_cast<DxRayTracingPipelineImpl*>(pipeline);
        if (!m_currentRtPipeline) return;

        ensureDescriptorHeaps();

        ComPtr<ID3D12GraphicsCommandList4> cmdList4;
        if (SUCCEEDED(m_cmdList->QueryInterface(IID_PPV_ARGS(&cmdList4))) && cmdList4)
            cmdList4->SetPipelineState1(m_currentRtPipeline->handle());

        // RT uses compute root signature binding.
        if (auto* layout = m_currentRtPipeline->pipelineLayout())
            m_cmdList->SetComputeRootSignature(layout->handle());
    }

    void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynamicOffsets) override {
        auto* dxGroup = static_cast<DxBindGroupImpl*>(group);
        if (!dxGroup || !m_currentRtPipeline) return;

        auto* layout = m_currentRtPipeline->pipelineLayout();
        if (!layout) return;

        auto* dxLayout = static_cast<DxBindGroupLayoutImpl*>(dxGroup->layout());

        // RT uses compute root signature binding -- copy-on-bind staging.
        if (dxGroup->cbvSrvUavOffset() >= 0 && dxLayout && dxLayout->cbvSrvUavCount() > 0) {
            i32 rootIdx = layout->getCbvSrvUavRootIndex(index);
            if (rootIdx >= 0) {
                i32 stagedOffset = m_pool->srvStaging()->copyFrom(
                    static_cast<u32>(dxGroup->cbvSrvUavOffset()), dxLayout->cbvSrvUavCount());
                if (stagedOffset >= 0) {
                    auto gpuHandle = m_gpuSrvHeap->getGpuHandle(static_cast<u32>(stagedOffset));
                    m_cmdList->SetComputeRootDescriptorTable(static_cast<UINT>(rootIdx), gpuHandle);
                }
            }
        }

        if (dxGroup->samplerOffset() >= 0 && dxLayout && dxLayout->samplerCount() > 0) {
            i32 rootIdx = layout->getSamplerRootIndex(index);
            if (rootIdx >= 0) {
                i32 stagedOffset = m_pool->samplerStaging()->copyFrom(
                    static_cast<u32>(dxGroup->samplerOffset()), dxLayout->samplerCount());
                if (stagedOffset >= 0) {
                    auto gpuHandle = m_gpuSamplerHeap->getGpuHandle(static_cast<u32>(stagedOffset));
                    m_cmdList->SetComputeRootDescriptorTable(static_cast<UINT>(rootIdx), gpuHandle);
                }
            }
        }

        // Dynamic root entries.
        auto dynEntries = layout->dynamicRootEntries();
        auto dynAddrs   = dxGroup->dynamicGpuAddresses();
        usize dynOffsetIdx = 0;
        for (usize i = 0; i < dynEntries.size(); ++i) {
            const auto& entry = dynEntries[i];
            if (entry.groupIndex != index) continue;
            if (entry.dynamicIndex >= dynAddrs.size()) continue;

            u64 gpuAddr = dynAddrs[entry.dynamicIndex];
            if (dynOffsetIdx < dynamicOffsets.size())
                gpuAddr += static_cast<u64>(dynamicOffsets[dynOffsetIdx]);
            ++dynOffsetIdx;

            switch (entry.paramType) {
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
                m_cmdList->SetComputeRootConstantBufferView(static_cast<UINT>(entry.rootParamIndex), gpuAddr);
                break;
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
                m_cmdList->SetComputeRootShaderResourceView(static_cast<UINT>(entry.rootParamIndex), gpuAddr);
                break;
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
                m_cmdList->SetComputeRootUnorderedAccessView(static_cast<UINT>(entry.rootParamIndex), gpuAddr);
                break;
            default: break;
            }
        }
    }

    void setPushConstants(ShaderStage, u32 offset, u32 size, const void* data) override {
        if (!m_currentRtPipeline) return;
        auto* layout = m_currentRtPipeline->pipelineLayout();
        if (!layout || layout->pushConstantRootIndex() < 0) return;

        m_cmdList->SetComputeRoot32BitConstants(
            static_cast<UINT>(layout->pushConstantRootIndex()),
            size / 4, data, offset / 4);
    }

    void traceRays(Buffer* raygenSBT, u64 raygenOffset, u64 raygenStride,
                   Buffer* missSBT, u64 missOffset, u64 missStride,
                   Buffer* hitSBT, u64 hitOffset, u64 hitStride,
                   u32 width, u32 height, u32 depth) override
    {
        ComPtr<ID3D12GraphicsCommandList4> cmdList4;
        if (FAILED(m_cmdList->QueryInterface(IID_PPV_ARGS(&cmdList4))) || !cmdList4) return;

        D3D12_DISPATCH_RAYS_DESC dispatchDesc{};

        // Raygen -- single record.
        if (auto* dxBuf = static_cast<DxBufferImpl*>(raygenSBT)) {
            dispatchDesc.RayGenerationShaderRecord.StartAddress = dxBuf->gpuAddress() + raygenOffset;
            dispatchDesc.RayGenerationShaderRecord.SizeInBytes  = raygenStride;
        }

        // Miss.
        if (missSBT) {
            if (auto* dxBuf = static_cast<DxBufferImpl*>(missSBT)) {
                dispatchDesc.MissShaderTable.StartAddress  = dxBuf->gpuAddress() + missOffset;
                dispatchDesc.MissShaderTable.StrideInBytes = missStride;
                dispatchDesc.MissShaderTable.SizeInBytes   = missStride; // assume single entry
            }
        }

        // Hit group.
        if (hitSBT) {
            if (auto* dxBuf = static_cast<DxBufferImpl*>(hitSBT)) {
                dispatchDesc.HitGroupTable.StartAddress  = dxBuf->gpuAddress() + hitOffset;
                dispatchDesc.HitGroupTable.StrideInBytes = hitStride;
                dispatchDesc.HitGroupTable.SizeInBytes   = hitStride; // assume single entry
            }
        }

        dispatchDesc.Width  = width;
        dispatchDesc.Height = height;
        dispatchDesc.Depth  = depth;

        cmdList4->DispatchRays(&dispatchDesc);
    }

    // ================================================================
    // Internal accessors
    // ================================================================

    [[nodiscard]] ID3D12GraphicsCommandList* cmdList()       const { return m_cmdList; }
    [[nodiscard]] DxDeviceImpl*              ownerDevice()   const { return m_device; }
    [[nodiscard]] DxDescriptorStaging*       srvStaging()          { return m_pool->srvStaging(); }
    [[nodiscard]] DxDescriptorStaging*       samplerStaging()      { return m_pool->samplerStaging(); }

    // ================================================================
    // Static helpers
    // ================================================================

    /// Convert RHI ResourceState to D3D12_RESOURCE_STATES.
    static D3D12_RESOURCE_STATES toResourceStates(ResourceState state) {
        return toResourceStates(state, TextureFormat::Undefined);
    }

    /// Convert RHI ResourceState to D3D12_RESOURCE_STATES, considering the texture format.
    /// For depth formats, ShaderRead maps to DEPTH_READ instead of PIXEL_SHADER_RESOURCE.
    static D3D12_RESOURCE_STATES toResourceStates(ResourceState state, TextureFormat format) {
        if (state == ResourceState::Undefined)
            return D3D12_RESOURCE_STATE_COMMON;

        D3D12_RESOURCE_STATES result = D3D12_RESOURCE_STATE_COMMON;

        if (hasFlag(state, ResourceState::VertexBuffer))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        if (hasFlag(state, ResourceState::IndexBuffer))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_INDEX_BUFFER);
        if (hasFlag(state, ResourceState::UniformBuffer))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        if (hasFlag(state, ResourceState::ShaderRead)) {
            // Depth textures use DEPTH_READ when sampled, not PIXEL_SHADER_RESOURCE.
            if (isDepthFormat(format))
                result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_DEPTH_READ);
            else
                result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
                                                                   | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if (hasFlag(state, ResourceState::ShaderWrite))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if (hasFlag(state, ResourceState::RenderTarget))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_RENDER_TARGET);
        if (hasFlag(state, ResourceState::DepthStencilWrite))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_DEPTH_WRITE);
        if (hasFlag(state, ResourceState::DepthStencilRead))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_DEPTH_READ);
        if (hasFlag(state, ResourceState::IndirectArgument))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        if (hasFlag(state, ResourceState::CopySrc))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_COPY_SOURCE);
        if (hasFlag(state, ResourceState::CopyDst))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_COPY_DEST);
        if (hasFlag(state, ResourceState::Present))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_PRESENT);
        if (hasFlag(state, ResourceState::General))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_COMMON);
        if (hasFlag(state, ResourceState::AccelStructRead))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
        if (hasFlag(state, ResourceState::AccelStructWrite))
            result = static_cast<D3D12_RESOURCE_STATES>(result | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        return result;
    }

private:
    // ---- Blit helper ----

    /// Blits one subresource using the device's fullscreen triangle pipeline.
    /// Expects src subresource in PIXEL_SHADER_RESOURCE state, dst subresource in RENDER_TARGET state.
    /// Implementation defined out-of-line (requires DxDeviceImpl for blit PSO and descriptor heaps).
    void blitSubresource(DxTextureImpl* srcTex, u32 srcMip, DxTextureImpl* dstTex, u32 dstMip,
                         u32 dstWidth, u32 dstHeight, DXGI_FORMAT dxgiFormat);

    // ---- Descriptor heap management ----

    void ensureDescriptorHeaps() {
        if (m_descriptorHeapsSet) return;
        m_descriptorHeapsSet = true;

        ID3D12DescriptorHeap* heaps[2] = {
            m_gpuSrvHeap->heap(),
            m_gpuSamplerHeap->heap()
        };
        m_cmdList->SetDescriptorHeaps(2, heaps);
    }

    static D3D12_RAYTRACING_GEOMETRY_FLAGS toGeometryFlags(GeometryFlags flags) {
        D3D12_RAYTRACING_GEOMETRY_FLAGS result = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
        if (static_cast<u32>(flags) & static_cast<u32>(GeometryFlags::Opaque))
            result = static_cast<D3D12_RAYTRACING_GEOMETRY_FLAGS>(
                result | D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE);
        if (static_cast<u32>(flags) & static_cast<u32>(GeometryFlags::NoDuplicateAnyHitInvocation))
            result = static_cast<D3D12_RAYTRACING_GEOMETRY_FLAGS>(
                result | D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION);
        return result;
    }

    static bool hasFlag(ResourceState state, ResourceState flag) {
        return (static_cast<u32>(state) & static_cast<u32>(flag)) != 0;
    }

    // ---- Members ----

    DxDeviceImpl*                m_device  = nullptr;
    ID3D12GraphicsCommandList*   m_cmdList = nullptr;
    DxCommandPoolImpl*           m_pool    = nullptr;
    DxRayTracingPipelineImpl*    m_currentRtPipeline = nullptr;
    bool                         m_descriptorHeapsSet = false;

    // GPU descriptor heaps (cached from device at construction).
    DxGpuDescriptorHeap*         m_gpuSrvHeap     = nullptr;
    DxGpuDescriptorHeap*         m_gpuSamplerHeap = nullptr;

    // Embedded sub-encoders using context-based decoupling (see DxRenderPassEncoder.cppm,
    // DxComputePassEncoder.cppm). Contexts carry the pointers the sub-encoders need.
    DxRenderPassEncoderImpl      m_rpe;
    DxComputePassEncoderImpl     m_cpe;
    DxRenderPassContext          m_rpeCtx;                 // cloned for bundle encoders
    std::vector<RenderBundleEncoder*>  m_bundleEncoders;         // owned wrappers (freed in dtor)
};

// ---- Deferred DxCommandPoolImpl method implementations ----

// createEncoder and destroyEncoder are defined out-of-line here because they
// need the full DxCommandEncoderImpl definition. The DxDeviceImpl will call
// pool->init() with the GPU descriptor heap pointers so the pool can build
// the context structs. For now, these are left as declarations to be resolved
// when DxDeviceImpl is defined.
//
// The DxDeviceImpl (not yet ported) will wire up:
//   DxRenderPassContext rpeCtx { cmdList, device, srvStaging, samplerStaging,
//                                gpuSrvHeap, gpuSamplerHeap, drawSig, drawIdxSig, meshSig };
//   DxComputePassContext cpeCtx { cmdList, srvStaging, samplerStaging,
//                                 gpuSrvHeap, gpuSamplerHeap, dispatchSig };

} // namespace draco::rhi::dx12
