/// Conversion utilities between draco::rhi enums and DX12/DXGI enums.

module;

#include "DxIncludes.h"

export module rhi.dx12:conversions;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::dx12 {

inline DXGI_FORMAT toDxgiFormat(TextureFormat f) {
    switch (f) {
    case TextureFormat::Undefined:          return DXGI_FORMAT_UNKNOWN;
    case TextureFormat::R8Unorm:            return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::R8Snorm:            return DXGI_FORMAT_R8_SNORM;
    case TextureFormat::R8Uint:             return DXGI_FORMAT_R8_UINT;
    case TextureFormat::R8Sint:             return DXGI_FORMAT_R8_SINT;
    case TextureFormat::R16Uint:            return DXGI_FORMAT_R16_UINT;
    case TextureFormat::R16Sint:            return DXGI_FORMAT_R16_SINT;
    case TextureFormat::R16Float:           return DXGI_FORMAT_R16_FLOAT;
    case TextureFormat::RG8Unorm:           return DXGI_FORMAT_R8G8_UNORM;
    case TextureFormat::RG8Snorm:           return DXGI_FORMAT_R8G8_SNORM;
    case TextureFormat::RG8Uint:            return DXGI_FORMAT_R8G8_UINT;
    case TextureFormat::RG8Sint:            return DXGI_FORMAT_R8G8_SINT;
    case TextureFormat::R32Uint:            return DXGI_FORMAT_R32_UINT;
    case TextureFormat::R32Sint:            return DXGI_FORMAT_R32_SINT;
    case TextureFormat::R32Float:           return DXGI_FORMAT_R32_FLOAT;
    case TextureFormat::RG16Uint:           return DXGI_FORMAT_R16G16_UINT;
    case TextureFormat::RG16Sint:           return DXGI_FORMAT_R16G16_SINT;
    case TextureFormat::RG16Float:          return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::RGBA8Unorm:         return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8UnormSrgb:     return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::RGBA8Snorm:         return DXGI_FORMAT_R8G8B8A8_SNORM;
    case TextureFormat::RGBA8Uint:          return DXGI_FORMAT_R8G8B8A8_UINT;
    case TextureFormat::RGBA8Sint:          return DXGI_FORMAT_R8G8B8A8_SINT;
    case TextureFormat::BGRA8Unorm:         return DXGI_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::BGRA8UnormSrgb:     return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case TextureFormat::RGB10A2Unorm:       return DXGI_FORMAT_R10G10B10A2_UNORM;
    case TextureFormat::RGB10A2Uint:        return DXGI_FORMAT_R10G10B10A2_UINT;
    case TextureFormat::RG11B10Float:       return DXGI_FORMAT_R11G11B10_FLOAT;
    case TextureFormat::RGB9E5Float:        return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
    case TextureFormat::RG32Uint:           return DXGI_FORMAT_R32G32_UINT;
    case TextureFormat::RG32Sint:           return DXGI_FORMAT_R32G32_SINT;
    case TextureFormat::RG32Float:          return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::RGBA16Uint:         return DXGI_FORMAT_R16G16B16A16_UINT;
    case TextureFormat::RGBA16Sint:         return DXGI_FORMAT_R16G16B16A16_SINT;
    case TextureFormat::RGBA16Float:        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::RGBA16Unorm:        return DXGI_FORMAT_R16G16B16A16_UNORM;
    case TextureFormat::RGBA16Snorm:        return DXGI_FORMAT_R16G16B16A16_SNORM;
    case TextureFormat::RGBA32Uint:         return DXGI_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::RGBA32Sint:         return DXGI_FORMAT_R32G32B32A32_SINT;
    case TextureFormat::RGBA32Float:        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TextureFormat::Depth16Unorm:       return DXGI_FORMAT_D16_UNORM;
    case TextureFormat::Depth24Plus:        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::Depth24PlusStencil8:return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::Depth32Float:       return DXGI_FORMAT_D32_FLOAT;
    case TextureFormat::Depth32FloatStencil8:return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    case TextureFormat::Stencil8:           return DXGI_FORMAT_R8_UINT;
    case TextureFormat::BC1RGBAUnorm:       return DXGI_FORMAT_BC1_UNORM;
    case TextureFormat::BC1RGBAUnormSrgb:   return DXGI_FORMAT_BC1_UNORM_SRGB;
    case TextureFormat::BC2RGBAUnorm:       return DXGI_FORMAT_BC2_UNORM;
    case TextureFormat::BC2RGBAUnormSrgb:   return DXGI_FORMAT_BC2_UNORM_SRGB;
    case TextureFormat::BC3RGBAUnorm:       return DXGI_FORMAT_BC3_UNORM;
    case TextureFormat::BC3RGBAUnormSrgb:   return DXGI_FORMAT_BC3_UNORM_SRGB;
    case TextureFormat::BC4RUnorm:          return DXGI_FORMAT_BC4_UNORM;
    case TextureFormat::BC4RSnorm:          return DXGI_FORMAT_BC4_SNORM;
    case TextureFormat::BC5RGUnorm:         return DXGI_FORMAT_BC5_UNORM;
    case TextureFormat::BC5RGSnorm:         return DXGI_FORMAT_BC5_SNORM;
    case TextureFormat::BC6HRGBUfloat:      return DXGI_FORMAT_BC6H_UF16;
    case TextureFormat::BC6HRGBFloat:       return DXGI_FORMAT_BC6H_SF16;
    case TextureFormat::BC7RGBAUnorm:       return DXGI_FORMAT_BC7_UNORM;
    case TextureFormat::BC7RGBAUnormSrgb:   return DXGI_FORMAT_BC7_UNORM_SRGB;
    default:                               return DXGI_FORMAT_UNKNOWN;
    }
}

inline TextureFormat fromDxgiFormat(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:      return TextureFormat::RGBA8Unorm;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return TextureFormat::RGBA8UnormSrgb;
    case DXGI_FORMAT_B8G8R8A8_UNORM:      return TextureFormat::BGRA8Unorm;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return TextureFormat::BGRA8UnormSrgb;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:  return TextureFormat::RGBA16Float;
    case DXGI_FORMAT_R10G10B10A2_UNORM:   return TextureFormat::RGB10A2Unorm;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:  return TextureFormat::RGBA32Float;
    default:                              return TextureFormat::Undefined;
    }
}

inline DXGI_FORMAT toDxgiVertexFormat(VertexFormat f) {
    switch (f) {
    case VertexFormat::Uint8x2:    return DXGI_FORMAT_R8G8_UINT;
    case VertexFormat::Uint8x4:    return DXGI_FORMAT_R8G8B8A8_UINT;
    case VertexFormat::Sint8x2:    return DXGI_FORMAT_R8G8_SINT;
    case VertexFormat::Sint8x4:    return DXGI_FORMAT_R8G8B8A8_SINT;
    case VertexFormat::Unorm8x2:   return DXGI_FORMAT_R8G8_UNORM;
    case VertexFormat::Unorm8x4:   return DXGI_FORMAT_R8G8B8A8_UNORM;
    case VertexFormat::Snorm8x2:   return DXGI_FORMAT_R8G8_SNORM;
    case VertexFormat::Snorm8x4:   return DXGI_FORMAT_R8G8B8A8_SNORM;
    case VertexFormat::Uint16x2:   return DXGI_FORMAT_R16G16_UINT;
    case VertexFormat::Uint16x4:   return DXGI_FORMAT_R16G16B16A16_UINT;
    case VertexFormat::Sint16x2:   return DXGI_FORMAT_R16G16_SINT;
    case VertexFormat::Sint16x4:   return DXGI_FORMAT_R16G16B16A16_SINT;
    case VertexFormat::Unorm16x2:  return DXGI_FORMAT_R16G16_UNORM;
    case VertexFormat::Unorm16x4:  return DXGI_FORMAT_R16G16B16A16_UNORM;
    case VertexFormat::Snorm16x2:  return DXGI_FORMAT_R16G16_SNORM;
    case VertexFormat::Snorm16x4:  return DXGI_FORMAT_R16G16B16A16_SNORM;
    case VertexFormat::Float16x2:  return DXGI_FORMAT_R16G16_FLOAT;
    case VertexFormat::Float16x4:  return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case VertexFormat::Float32:    return DXGI_FORMAT_R32_FLOAT;
    case VertexFormat::Float32x2:  return DXGI_FORMAT_R32G32_FLOAT;
    case VertexFormat::Float32x3:  return DXGI_FORMAT_R32G32B32_FLOAT;
    case VertexFormat::Float32x4:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case VertexFormat::Uint32:     return DXGI_FORMAT_R32_UINT;
    case VertexFormat::Uint32x2:   return DXGI_FORMAT_R32G32_UINT;
    case VertexFormat::Uint32x3:   return DXGI_FORMAT_R32G32B32_UINT;
    case VertexFormat::Uint32x4:   return DXGI_FORMAT_R32G32B32A32_UINT;
    case VertexFormat::Sint32:     return DXGI_FORMAT_R32_SINT;
    case VertexFormat::Sint32x2:   return DXGI_FORMAT_R32G32_SINT;
    case VertexFormat::Sint32x3:   return DXGI_FORMAT_R32G32B32_SINT;
    case VertexFormat::Sint32x4:   return DXGI_FORMAT_R32G32B32A32_SINT;
    default:                       return DXGI_FORMAT_UNKNOWN;
    }
}

inline DXGI_FORMAT toDxgiIndexFormat(IndexFormat f) {
    switch (f) {
    case IndexFormat::UInt16: return DXGI_FORMAT_R16_UINT;
    case IndexFormat::UInt32: return DXGI_FORMAT_R32_UINT;
    }
    return DXGI_FORMAT_R16_UINT;
}

inline D3D12_COMMAND_LIST_TYPE toCommandListType(QueueType t) {
    switch (t) {
    case QueueType::Graphics: return D3D12_COMMAND_LIST_TYPE_DIRECT;
    case QueueType::Compute:  return D3D12_COMMAND_LIST_TYPE_COMPUTE;
    case QueueType::Transfer: return D3D12_COMMAND_LIST_TYPE_COPY;
    }
    return D3D12_COMMAND_LIST_TYPE_DIRECT;
}

inline D3D12_HEAP_TYPE toHeapType(MemoryLocation loc) {
    switch (loc) {
    case MemoryLocation::GpuOnly:  return D3D12_HEAP_TYPE_DEFAULT;
    case MemoryLocation::CpuToGpu: return D3D12_HEAP_TYPE_UPLOAD;
    case MemoryLocation::GpuToCpu: return D3D12_HEAP_TYPE_READBACK;
    case MemoryLocation::Auto:     return D3D12_HEAP_TYPE_DEFAULT;
    }
    return D3D12_HEAP_TYPE_DEFAULT;
}

inline D3D12_RESOURCE_FLAGS toTextureResourceFlags(TextureUsage usage) {
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (static_cast<u32>(usage & TextureUsage::RenderTarget))  flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (static_cast<u32>(usage & TextureUsage::DepthStencil))  flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if (static_cast<u32>(usage & TextureUsage::Storage))       flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    return flags;
}

inline D3D12_RESOURCE_FLAGS toBufferResourceFlags(BufferUsage usage) {
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (static_cast<u32>(usage & BufferUsage::Storage))           flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    if (static_cast<u32>(usage & BufferUsage::AccelStructScratch)) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    return flags;
}

inline D3D12_RESOURCE_DIMENSION toResourceDimension(TextureDimension d) {
    switch (d) {
    case TextureDimension::Texture1D: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    case TextureDimension::Texture2D: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    case TextureDimension::Texture3D: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }
    return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
}

inline D3D12_COMPARISON_FUNC toComparisonFunc(CompareFunction f) {
    switch (f) {
    case CompareFunction::Never:        return D3D12_COMPARISON_FUNC_NEVER;
    case CompareFunction::Less:         return D3D12_COMPARISON_FUNC_LESS;
    case CompareFunction::Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
    case CompareFunction::LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case CompareFunction::Greater:      return D3D12_COMPARISON_FUNC_GREATER;
    case CompareFunction::NotEqual:     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case CompareFunction::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case CompareFunction::Always:       return D3D12_COMPARISON_FUNC_ALWAYS;
    }
    return D3D12_COMPARISON_FUNC_ALWAYS;
}

inline D3D12_TEXTURE_ADDRESS_MODE toAddressMode(AddressMode m) {
    switch (m) {
    case AddressMode::Repeat:        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case AddressMode::MirrorRepeat:  return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case AddressMode::ClampToEdge:   return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case AddressMode::ClampToBorder: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    }
    return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

inline D3D12_FILTER toFilter(FilterMode min, FilterMode mag, MipmapFilterMode mip, bool comparison) {
    bool minL = min == FilterMode::Linear;
    bool magL = mag == FilterMode::Linear;
    bool mipL = mip == MipmapFilterMode::Linear;
    if (comparison) {
        if (!minL && !magL && !mipL) return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
        if (!minL && !magL &&  mipL) return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
        if (!minL &&  magL && !mipL) return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
        if (!minL &&  magL &&  mipL) return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
        if ( minL && !magL && !mipL) return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
        if ( minL && !magL &&  mipL) return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        if ( minL &&  magL && !mipL) return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    }
    if (!minL && !magL && !mipL) return D3D12_FILTER_MIN_MAG_MIP_POINT;
    if (!minL && !magL &&  mipL) return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    if (!minL &&  magL && !mipL) return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    if (!minL &&  magL &&  mipL) return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
    if ( minL && !magL && !mipL) return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    if ( minL && !magL &&  mipL) return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    if ( minL &&  magL && !mipL) return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
}

inline DXGI_FORMAT toTypelessDepthFormat(TextureFormat f) {
    switch (f) {
    case TextureFormat::Depth16Unorm:        return DXGI_FORMAT_R16_TYPELESS;
    case TextureFormat::Depth24Plus:
    case TextureFormat::Depth24PlusStencil8: return DXGI_FORMAT_R24G8_TYPELESS;
    case TextureFormat::Depth32Float:        return DXGI_FORMAT_R32_TYPELESS;
    case TextureFormat::Depth32FloatStencil8:return DXGI_FORMAT_R32G8X24_TYPELESS;
    default:                                return toDxgiFormat(f);
    }
}

inline DXGI_FORMAT toDepthSrvFormat(TextureFormat f) {
    switch (f) {
    case TextureFormat::Depth16Unorm:        return DXGI_FORMAT_R16_UNORM;
    case TextureFormat::Depth24Plus:
    case TextureFormat::Depth24PlusStencil8: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case TextureFormat::Depth32Float:        return DXGI_FORMAT_R32_FLOAT;
    case TextureFormat::Depth32FloatStencil8:return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    default:                                return toDxgiFormat(f);
    }
}

inline DXGI_FORMAT toStencilSrvFormat(TextureFormat f) {
    switch (f) {
    case TextureFormat::Depth24PlusStencil8: return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
    case TextureFormat::Depth32FloatStencil8:return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
    default:                                return toDxgiFormat(f);
    }
}

inline D3D12_PRIMITIVE_TOPOLOGY_TYPE toPrimitiveTopologyType(PrimitiveTopology t) {
    switch (t) {
    case PrimitiveTopology::PointList:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    case PrimitiveTopology::LineList:
    case PrimitiveTopology::LineStrip:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case PrimitiveTopology::TriangleList:
    case PrimitiveTopology::TriangleStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
}

inline D3D_PRIMITIVE_TOPOLOGY toPrimitiveTopology(PrimitiveTopology t) {
    switch (t) {
    case PrimitiveTopology::PointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    case PrimitiveTopology::LineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case PrimitiveTopology::LineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case PrimitiveTopology::TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    }
    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

inline D3D12_CULL_MODE toCullMode(CullMode m) {
    switch (m) {
    case CullMode::None:  return D3D12_CULL_MODE_NONE;
    case CullMode::Front: return D3D12_CULL_MODE_FRONT;
    case CullMode::Back:  return D3D12_CULL_MODE_BACK;
    }
    return D3D12_CULL_MODE_NONE;
}

inline D3D12_FILL_MODE toFillMode(FillMode m) {
    switch (m) {
    case FillMode::Solid:     return D3D12_FILL_MODE_SOLID;
    case FillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
    }
    return D3D12_FILL_MODE_SOLID;
}

inline D3D12_BLEND toBlendFactor(BlendFactor f) {
    switch (f) {
    case BlendFactor::Zero:              return D3D12_BLEND_ZERO;
    case BlendFactor::One:               return D3D12_BLEND_ONE;
    case BlendFactor::Src:               return D3D12_BLEND_SRC_COLOR;
    case BlendFactor::OneMinusSrc:       return D3D12_BLEND_INV_SRC_COLOR;
    case BlendFactor::SrcAlpha:          return D3D12_BLEND_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:  return D3D12_BLEND_INV_SRC_ALPHA;
    case BlendFactor::Dst:               return D3D12_BLEND_DEST_COLOR;
    case BlendFactor::OneMinusDst:       return D3D12_BLEND_INV_DEST_COLOR;
    case BlendFactor::DstAlpha:          return D3D12_BLEND_DEST_ALPHA;
    case BlendFactor::OneMinusDstAlpha:  return D3D12_BLEND_INV_DEST_ALPHA;
    case BlendFactor::SrcAlphaSaturated: return D3D12_BLEND_SRC_ALPHA_SAT;
    case BlendFactor::Constant:          return D3D12_BLEND_BLEND_FACTOR;
    case BlendFactor::OneMinusConstant:  return D3D12_BLEND_INV_BLEND_FACTOR;
    }
    return D3D12_BLEND_ONE;
}

inline D3D12_BLEND_OP toBlendOp(BlendOperation op) {
    switch (op) {
    case BlendOperation::Add:             return D3D12_BLEND_OP_ADD;
    case BlendOperation::Subtract:        return D3D12_BLEND_OP_SUBTRACT;
    case BlendOperation::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
    case BlendOperation::Min:             return D3D12_BLEND_OP_MIN;
    case BlendOperation::Max:             return D3D12_BLEND_OP_MAX;
    }
    return D3D12_BLEND_OP_ADD;
}

inline D3D12_STENCIL_OP toStencilOp(StencilOperation op) {
    switch (op) {
    case StencilOperation::Keep:           return D3D12_STENCIL_OP_KEEP;
    case StencilOperation::Zero:           return D3D12_STENCIL_OP_ZERO;
    case StencilOperation::Replace:        return D3D12_STENCIL_OP_REPLACE;
    case StencilOperation::IncrementClamp: return D3D12_STENCIL_OP_INCR_SAT;
    case StencilOperation::DecrementClamp: return D3D12_STENCIL_OP_DECR_SAT;
    case StencilOperation::Invert:         return D3D12_STENCIL_OP_INVERT;
    case StencilOperation::IncrementWrap:  return D3D12_STENCIL_OP_INCR;
    case StencilOperation::DecrementWrap:  return D3D12_STENCIL_OP_DECR;
    }
    return D3D12_STENCIL_OP_KEEP;
}

inline D3D12_DESCRIPTOR_RANGE_TYPE toDescriptorRangeType(BindingType t) {
    switch (t) {
    case BindingType::UniformBuffer:            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    case BindingType::StorageBufferReadOnly:    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case BindingType::StorageBufferReadWrite:   return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case BindingType::SampledTexture:           return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case BindingType::StorageTextureReadOnly:   return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case BindingType::StorageTextureReadWrite:  return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case BindingType::Sampler:                  return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    case BindingType::ComparisonSampler:        return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    case BindingType::BindlessTextures:         return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case BindingType::BindlessSamplers:         return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    case BindingType::BindlessStorageBuffers:   return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case BindingType::BindlessStorageTextures:  return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case BindingType::AccelerationStructure:    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    }
    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
}

inline bool isSamplerBinding(BindingType t) {
    return t == BindingType::Sampler || t == BindingType::ComparisonSampler || t == BindingType::BindlessSamplers;
}

/// Strips sRGB from a DXGI format (needed for DXGI flip model swap chains).
inline DXGI_FORMAT stripSrgb(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_BC1_UNORM_SRGB:      return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_UNORM_SRGB:      return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_UNORM_SRGB:      return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC7_UNORM_SRGB:      return DXGI_FORMAT_BC7_UNORM;
    default:                              return f;
    }
}

} // namespace draco::rhi::dx12
