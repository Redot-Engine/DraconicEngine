/// DX12 implementation of TextureView.
/// Lazily creates SRV/RTV/DSV/UAV CPU descriptor handles on demand.

module;

#include "DxIncludes.h"

export module rhi.dx12:texture_view;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :texture;
import :descriptor_heap;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxTextureViewImpl : public TextureView {
public:
    Status init(ID3D12Device* device, DxTextureImpl* tex, const TextureViewDesc& d,
                DxDescriptorHeapAllocator* srvHeap, DxDescriptorHeapAllocator* rtvHeap,
                DxDescriptorHeapAllocator* dsvHeap) {
        m_device  = device;
        m_texture = tex;
        m_viewDesc= d;
        m_srvHeap = srvHeap;
        m_rtvHeap = rtvHeap;
        m_dsvHeap = dsvHeap;
        return ErrorCode::Ok;
    }

    // ---- Lazy SRV ----
    D3D12_CPU_DESCRIPTOR_HANDLE getSrv() {
        if (m_hasSrv) return m_srv;

        auto fmt = (m_viewDesc.format == TextureFormat::Undefined) ? m_texture->desc.format : m_viewDesc.format;
        DXGI_FORMAT srvFmt = isDepthFormat(fmt) ? toDepthSrvFormat(fmt) : toDxgiFormat(fmt);

        D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = srvFmt;
        sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        u32 mips = m_viewDesc.mipLevelCount;
        if (mips == 0) mips = m_texture->desc.mipLevelCount - m_viewDesc.baseMipLevel;

        switch (m_viewDesc.dimension) {
        case TextureViewDimension::Texture1D:
            sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            sd.Texture1D.MostDetailedMip = m_viewDesc.baseMipLevel;
            sd.Texture1D.MipLevels = mips;
            break;
        case TextureViewDimension::Texture1DArray:
            sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            sd.Texture1DArray.MostDetailedMip = m_viewDesc.baseMipLevel;
            sd.Texture1DArray.MipLevels = mips;
            sd.Texture1DArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
            sd.Texture1DArray.ArraySize = m_viewDesc.arrayLayerCount;
            break;
        case TextureViewDimension::Texture2D:
            if (m_texture->desc.sampleCount > 1) {
                sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
            } else {
                sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                sd.Texture2D.MostDetailedMip = m_viewDesc.baseMipLevel;
                sd.Texture2D.MipLevels = mips;
            }
            break;
        case TextureViewDimension::Texture2DArray:
            if (m_texture->desc.sampleCount > 1) {
                sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                sd.Texture2DMSArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
                sd.Texture2DMSArray.ArraySize = m_viewDesc.arrayLayerCount;
            } else {
                sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                sd.Texture2DArray.MostDetailedMip = m_viewDesc.baseMipLevel;
                sd.Texture2DArray.MipLevels = mips;
                sd.Texture2DArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
                sd.Texture2DArray.ArraySize = m_viewDesc.arrayLayerCount;
            }
            break;
        case TextureViewDimension::TextureCube:
            sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            sd.TextureCube.MostDetailedMip = m_viewDesc.baseMipLevel;
            sd.TextureCube.MipLevels = mips;
            break;
        case TextureViewDimension::TextureCubeArray:
            sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
            sd.TextureCubeArray.MostDetailedMip = m_viewDesc.baseMipLevel;
            sd.TextureCubeArray.MipLevels = mips;
            sd.TextureCubeArray.First2DArrayFace = m_viewDesc.baseArrayLayer;
            sd.TextureCubeArray.NumCubes = m_viewDesc.arrayLayerCount / 6;
            break;
        case TextureViewDimension::Texture3D:
            sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            sd.Texture3D.MostDetailedMip = m_viewDesc.baseMipLevel;
            sd.Texture3D.MipLevels = mips;
            break;
        }

        m_srv = m_srvHeap->allocate();
        m_device->CreateShaderResourceView(m_texture->handle(), &sd, m_srv);
        m_hasSrv = true;
        return m_srv;
    }

    // ---- Lazy RTV ----
    D3D12_CPU_DESCRIPTOR_HANDLE getRtv() {
        if (m_hasRtv) return m_rtv;

        auto fmt = (m_viewDesc.format == TextureFormat::Undefined) ? m_texture->desc.format : m_viewDesc.format;
        bool isArray = m_texture->desc.arrayLayerCount > 1;

        D3D12_RENDER_TARGET_VIEW_DESC rd{};
        rd.Format = toDxgiFormat(fmt);

        if (m_viewDesc.dimension == TextureViewDimension::Texture2D) {
            if (isArray) {
                if (m_texture->desc.sampleCount > 1) {
                    rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                    rd.Texture2DMSArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
                    rd.Texture2DMSArray.ArraySize = m_viewDesc.arrayLayerCount;
                } else {
                    rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                    rd.Texture2DArray.MipSlice = m_viewDesc.baseMipLevel;
                    rd.Texture2DArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
                    rd.Texture2DArray.ArraySize = m_viewDesc.arrayLayerCount;
                }
            } else if (m_texture->desc.sampleCount > 1) {
                rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            } else {
                rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rd.Texture2D.MipSlice = m_viewDesc.baseMipLevel;
            }
        } else if (m_viewDesc.dimension == TextureViewDimension::Texture3D) {
            rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
            rd.Texture3D.MipSlice = m_viewDesc.baseMipLevel;
            rd.Texture3D.WSize = m_viewDesc.arrayLayerCount;
        } else {
            // 2DArray, Cube, CubeArray
            if (m_texture->desc.sampleCount > 1) {
                rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                rd.Texture2DMSArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
                rd.Texture2DMSArray.ArraySize = m_viewDesc.arrayLayerCount;
            } else {
                rd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rd.Texture2DArray.MipSlice = m_viewDesc.baseMipLevel;
                rd.Texture2DArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
                rd.Texture2DArray.ArraySize = m_viewDesc.arrayLayerCount;
            }
        }

        m_rtv = m_rtvHeap->allocate();
        m_device->CreateRenderTargetView(m_texture->handle(), &rd, m_rtv);
        m_hasRtv = true;
        return m_rtv;
    }

    // ---- Lazy DSV ----
    D3D12_CPU_DESCRIPTOR_HANDLE getDsv() {
        if (m_hasDsv) return m_dsv;

        auto fmt = (m_viewDesc.format == TextureFormat::Undefined) ? m_texture->desc.format : m_viewDesc.format;
        bool isArray = m_texture->desc.arrayLayerCount > 1;

        D3D12_DEPTH_STENCIL_VIEW_DESC dd{};
        dd.Format = toDxgiFormat(fmt);

        if (m_viewDesc.dimension == TextureViewDimension::Texture2D) {
            if (isArray) {
                if (m_texture->desc.sampleCount > 1) {
                    dd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                    dd.Texture2DMSArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
                    dd.Texture2DMSArray.ArraySize = m_viewDesc.arrayLayerCount;
                } else {
                    dd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                    dd.Texture2DArray.MipSlice = m_viewDesc.baseMipLevel;
                    dd.Texture2DArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
                    dd.Texture2DArray.ArraySize = m_viewDesc.arrayLayerCount;
                }
            } else if (m_texture->desc.sampleCount > 1) {
                dd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            } else {
                dd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dd.Texture2D.MipSlice = m_viewDesc.baseMipLevel;
            }
        } else {
            if (m_texture->desc.sampleCount > 1) {
                dd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                dd.Texture2DMSArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
                dd.Texture2DMSArray.ArraySize = m_viewDesc.arrayLayerCount;
            } else {
                dd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                dd.Texture2DArray.MipSlice = m_viewDesc.baseMipLevel;
                dd.Texture2DArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
                dd.Texture2DArray.ArraySize = m_viewDesc.arrayLayerCount;
            }
        }

        m_dsv = m_dsvHeap->allocate();
        m_device->CreateDepthStencilView(m_texture->handle(), &dd, m_dsv);
        m_hasDsv = true;
        return m_dsv;
    }

    // ---- Lazy UAV ----
    D3D12_CPU_DESCRIPTOR_HANDLE getUav() {
        if (m_hasUav) return m_uav;

        auto fmt = (m_viewDesc.format == TextureFormat::Undefined) ? m_texture->desc.format : m_viewDesc.format;

        D3D12_UNORDERED_ACCESS_VIEW_DESC ud{};
        ud.Format = toDxgiFormat(fmt);

        switch (m_viewDesc.dimension) {
        case TextureViewDimension::Texture1D:
            ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            ud.Texture1D.MipSlice = m_viewDesc.baseMipLevel;
            break;
        case TextureViewDimension::Texture1DArray:
            ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            ud.Texture1DArray.MipSlice = m_viewDesc.baseMipLevel;
            ud.Texture1DArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
            ud.Texture1DArray.ArraySize = m_viewDesc.arrayLayerCount;
            break;
        case TextureViewDimension::Texture2D:
            ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            ud.Texture2D.MipSlice = m_viewDesc.baseMipLevel;
            break;
        case TextureViewDimension::Texture2DArray:
        case TextureViewDimension::TextureCube:
        case TextureViewDimension::TextureCubeArray:
            ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            ud.Texture2DArray.MipSlice = m_viewDesc.baseMipLevel;
            ud.Texture2DArray.FirstArraySlice = m_viewDesc.baseArrayLayer;
            ud.Texture2DArray.ArraySize = m_viewDesc.arrayLayerCount;
            break;
        case TextureViewDimension::Texture3D:
            ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            ud.Texture3D.MipSlice = m_viewDesc.baseMipLevel;
            ud.Texture3D.FirstWSlice = m_viewDesc.baseArrayLayer;
            ud.Texture3D.WSize = m_viewDesc.arrayLayerCount;
            break;
        default:
            ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            ud.Texture2D.MipSlice = m_viewDesc.baseMipLevel;
            break;
        }

        m_uav = m_srvHeap->allocate();
        m_device->CreateUnorderedAccessView(m_texture->handle(), nullptr, &ud, m_uav);
        m_hasUav = true;
        return m_uav;
    }

    void cleanup() {
        if (m_hasSrv) { m_srvHeap->free(m_srv); m_hasSrv = false; }
        if (m_hasRtv) { m_rtvHeap->free(m_rtv); m_hasRtv = false; }
        if (m_hasDsv) { m_dsvHeap->free(m_dsv); m_hasDsv = false; }
        if (m_hasUav) { m_srvHeap->free(m_uav); m_hasUav = false; }
    }

    // ---- Internal ----
    [[nodiscard]] DxTextureImpl*    dxTexture() const { return m_texture; }
    [[nodiscard]] TextureViewDesc   viewDesc()  const { return m_viewDesc; }
    [[nodiscard]] TextureFormat     format()    const {
        return (m_viewDesc.format == TextureFormat::Undefined) ? m_texture->desc.format : m_viewDesc.format;
    }
    // Base-mip extent (a view onto mip N is half-sized per level). DX12 has no Vulkan-style renderArea
    // validation so a stale base size here wouldn't fault, but callers (e.g. the render graph's
    // viewport default) expect the view's true dimensions - keep it correct + consistent with Vulkan.
    [[nodiscard]] u32 width()  const { u32 w = m_texture->desc.width  >> m_viewDesc.baseMipLevel; return w ? w : 1u; }
    [[nodiscard]] u32 height() const { u32 h = m_texture->desc.height >> m_viewDesc.baseMipLevel; return h ? h : 1u; }

private:
    ID3D12Device*               m_device   = nullptr;
    DxTextureImpl*              m_texture  = nullptr;
    TextureViewDesc             m_viewDesc{};
    DxDescriptorHeapAllocator*  m_srvHeap  = nullptr;
    DxDescriptorHeapAllocator*  m_rtvHeap  = nullptr;
    DxDescriptorHeapAllocator*  m_dsvHeap  = nullptr;

    D3D12_CPU_DESCRIPTOR_HANDLE m_srv{}, m_rtv{}, m_dsv{}, m_uav{};
    bool m_hasSrv = false, m_hasRtv = false, m_hasDsv = false, m_hasUav = false;
};

} // namespace draco::rhi::dx12
