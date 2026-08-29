/// DX12 implementation of RenderBundleEncoder + RenderBundle.
///
/// A render bundle is an ID3D12GraphicsCommandList of type D3D12_COMMAND_LIST_TYPE_BUNDLE,
/// recorded once and replayed into a direct command list via ExecuteBundle
/// (RenderPassEncoder::ExecuteBundles). DX12 bundles inherit the parent's descriptor heaps,
/// viewport, scissor, and render targets - but NOT pipeline state / topology, which the bundle
/// sets itself (handled by SetPipeline). Draw recording is delegated to a DxRenderPassEncoderImpl
/// whose context points at the bundle list, reusing the full root-signature / descriptor-table
/// binding logic.
///
/// BEST-EFFORT (authored without a Windows toolchain to compile against - shake out on Windows):
/// the structure + DX12 bundle semantics are in place; details (heap inheritance, signature
/// caching, lifetime vs frames-in-flight) may need adjustment.

module;

#include "DxIncludes.h"
#include <utility>
#include <span>

export module rhi.dx12:render_bundle_encoder;

import core.stdtypes;
import core.status;
import rhi;
import :render_pass_encoder;

using namespace draco;

export namespace draco::rhi::dx12 {

// DxRenderBundleImpl lives in :render_pass_encoder (so ExecuteBundles can use it without a
// module cycle); this partition imports it from there.

// Records draws into a bundle command list by delegating to a render-pass encoder pointed at it.
class DxRenderBundleEncoderImpl : public RenderBundleEncoder {
public:
    DxRenderBundleEncoderImpl(const DxRenderPassContext& ctx,
                              ComPtr<ID3D12GraphicsCommandList> list, ComPtr<ID3D12CommandAllocator> alloc)
        : m_rec(ctx), m_list(std::move(list)), m_alloc(std::move(alloc)) {
        m_rec.begin(RenderPassDesc{});   // reset pipeline-tracking state (no pass attachments needed)
    }
    ~DxRenderBundleEncoderImpl() override { delete m_bundle; }

    void setPipeline(RenderPipeline* p) override { m_rec.setPipeline(p); }
    void setBindGroup(u32 i, BindGroup* g, std::span<const u32> d) override { m_rec.setBindGroup(i, g, d); }
    void setPushConstants(ShaderStage s, u32 o, u32 sz, const void* d) override { m_rec.setPushConstants(s, o, sz, d); }
    void setVertexBuffer(u32 slot, Buffer* b, u64 o) override { m_rec.setVertexBuffer(slot, b, o); }
    void setIndexBuffer(Buffer* b, IndexFormat f, u64 o) override { m_rec.setIndexBuffer(b, f, o); }
    void draw(u32 v, u32 inst, u32 fv, u32 fi) override { m_rec.draw(v, inst, fv, fi); }
    void drawIndexed(u32 ic, u32 inst, u32 fi, i32 bv, u32 finst) override { m_rec.drawIndexed(ic, inst, fi, bv, finst); }
    void drawIndirect(Buffer* b, u64 o, u32 dc, u32 st) override { m_rec.drawIndirect(b, o, dc, st); }
    void drawIndexedIndirect(Buffer* b, u64 o, u32 dc, u32 st) override { m_rec.drawIndexedIndirect(b, o, dc, st); }

    RenderBundle* finish() override {
        if (m_bundle) return m_bundle;
        m_list->Close();
        m_bundle = new DxRenderBundleImpl(m_list, m_alloc, m_rec.currentRootSig(), m_rec.currentPso());
        return m_bundle;
    }

private:
    DxRenderPassEncoderImpl           m_rec;
    ComPtr<ID3D12GraphicsCommandList> m_list;
    ComPtr<ID3D12CommandAllocator>    m_alloc;
    DxRenderBundleImpl*               m_bundle = nullptr;
};

} // namespace draco::rhi::dx12
