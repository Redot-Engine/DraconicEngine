/// Conversion utilities between draco::rhi enums and Vulkan enums.

module;

#include "VkIncludes.h"

export module rhi.vk:conversions;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::vk {

/// Depth format support flags - set once at VkDevice init via setDepthFormatSupport().
/// Defaults to supported; probing overrides if hardware lacks D24.
namespace detail {
    inline bool g_depth24S8Supported = true;
    inline bool g_depth24Supported   = true;
}

/// Called by VkDevice::init after probing physical device format properties.
inline void setDepthFormatSupport(bool depth24S8, bool depth24) {
    detail::g_depth24S8Supported = depth24S8;
    detail::g_depth24Supported   = depth24;
}

inline VkFormat toVkFormat(TextureFormat f) {
    switch (f) {
    case TextureFormat::Undefined:          return VK_FORMAT_UNDEFINED;
    case TextureFormat::R8Unorm:            return VK_FORMAT_R8_UNORM;
    case TextureFormat::R8Snorm:            return VK_FORMAT_R8_SNORM;
    case TextureFormat::R8Uint:             return VK_FORMAT_R8_UINT;
    case TextureFormat::R8Sint:             return VK_FORMAT_R8_SINT;
    case TextureFormat::R16Uint:            return VK_FORMAT_R16_UINT;
    case TextureFormat::R16Sint:            return VK_FORMAT_R16_SINT;
    case TextureFormat::R16Float:           return VK_FORMAT_R16_SFLOAT;
    case TextureFormat::RG8Unorm:           return VK_FORMAT_R8G8_UNORM;
    case TextureFormat::RG8Snorm:           return VK_FORMAT_R8G8_SNORM;
    case TextureFormat::RG8Uint:            return VK_FORMAT_R8G8_UINT;
    case TextureFormat::RG8Sint:            return VK_FORMAT_R8G8_SINT;
    case TextureFormat::R32Uint:            return VK_FORMAT_R32_UINT;
    case TextureFormat::R32Sint:            return VK_FORMAT_R32_SINT;
    case TextureFormat::R32Float:           return VK_FORMAT_R32_SFLOAT;
    case TextureFormat::RG16Uint:           return VK_FORMAT_R16G16_UINT;
    case TextureFormat::RG16Sint:           return VK_FORMAT_R16G16_SINT;
    case TextureFormat::RG16Float:          return VK_FORMAT_R16G16_SFLOAT;
    case TextureFormat::RGBA8Unorm:         return VK_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8UnormSrgb:     return VK_FORMAT_R8G8B8A8_SRGB;
    case TextureFormat::RGBA8Snorm:         return VK_FORMAT_R8G8B8A8_SNORM;
    case TextureFormat::RGBA8Uint:          return VK_FORMAT_R8G8B8A8_UINT;
    case TextureFormat::RGBA8Sint:          return VK_FORMAT_R8G8B8A8_SINT;
    case TextureFormat::BGRA8Unorm:         return VK_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::BGRA8UnormSrgb:     return VK_FORMAT_B8G8R8A8_SRGB;
    case TextureFormat::RGB10A2Unorm:       return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case TextureFormat::RGB10A2Uint:        return VK_FORMAT_A2B10G10R10_UINT_PACK32;
    case TextureFormat::RG11B10Float:       return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case TextureFormat::RGB9E5Float:        return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
    case TextureFormat::RG32Uint:           return VK_FORMAT_R32G32_UINT;
    case TextureFormat::RG32Sint:           return VK_FORMAT_R32G32_SINT;
    case TextureFormat::RG32Float:          return VK_FORMAT_R32G32_SFLOAT;
    case TextureFormat::RGBA16Uint:         return VK_FORMAT_R16G16B16A16_UINT;
    case TextureFormat::RGBA16Sint:         return VK_FORMAT_R16G16B16A16_SINT;
    case TextureFormat::RGBA16Float:        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case TextureFormat::RGBA16Unorm:        return VK_FORMAT_R16G16B16A16_UNORM;
    case TextureFormat::RGBA16Snorm:        return VK_FORMAT_R16G16B16A16_SNORM;
    case TextureFormat::RGBA32Uint:         return VK_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::RGBA32Sint:         return VK_FORMAT_R32G32B32A32_SINT;
    case TextureFormat::RGBA32Float:        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case TextureFormat::Depth16Unorm:       return VK_FORMAT_D16_UNORM;
    case TextureFormat::Depth24Plus:        return detail::g_depth24Supported   ? VK_FORMAT_X8_D24_UNORM_PACK32 : VK_FORMAT_D32_SFLOAT;
    case TextureFormat::Depth24PlusStencil8:return detail::g_depth24S8Supported ? VK_FORMAT_D24_UNORM_S8_UINT    : VK_FORMAT_D32_SFLOAT_S8_UINT;
    case TextureFormat::Depth32Float:       return VK_FORMAT_D32_SFLOAT;
    case TextureFormat::Depth32FloatStencil8:return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case TextureFormat::Stencil8:           return VK_FORMAT_S8_UINT;
    case TextureFormat::BC1RGBAUnorm:       return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case TextureFormat::BC1RGBAUnormSrgb:   return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case TextureFormat::BC2RGBAUnorm:       return VK_FORMAT_BC2_UNORM_BLOCK;
    case TextureFormat::BC2RGBAUnormSrgb:   return VK_FORMAT_BC2_SRGB_BLOCK;
    case TextureFormat::BC3RGBAUnorm:       return VK_FORMAT_BC3_UNORM_BLOCK;
    case TextureFormat::BC3RGBAUnormSrgb:   return VK_FORMAT_BC3_SRGB_BLOCK;
    case TextureFormat::BC4RUnorm:          return VK_FORMAT_BC4_UNORM_BLOCK;
    case TextureFormat::BC4RSnorm:          return VK_FORMAT_BC4_SNORM_BLOCK;
    case TextureFormat::BC5RGUnorm:         return VK_FORMAT_BC5_UNORM_BLOCK;
    case TextureFormat::BC5RGSnorm:         return VK_FORMAT_BC5_SNORM_BLOCK;
    case TextureFormat::BC6HRGBUfloat:      return VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case TextureFormat::BC6HRGBFloat:       return VK_FORMAT_BC6H_SFLOAT_BLOCK;
    case TextureFormat::BC7RGBAUnorm:       return VK_FORMAT_BC7_UNORM_BLOCK;
    case TextureFormat::BC7RGBAUnormSrgb:   return VK_FORMAT_BC7_SRGB_BLOCK;
    case TextureFormat::ASTC4x4Unorm:       return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case TextureFormat::ASTC4x4UnormSrgb:   return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    case TextureFormat::ASTC5x5Unorm:       return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
    case TextureFormat::ASTC5x5UnormSrgb:   return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
    case TextureFormat::ASTC6x6Unorm:       return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
    case TextureFormat::ASTC6x6UnormSrgb:   return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
    case TextureFormat::ASTC8x8Unorm:       return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    case TextureFormat::ASTC8x8UnormSrgb:   return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
    default:                                return VK_FORMAT_UNDEFINED;
    }
}

inline VkFormat toVkVertexFormat(VertexFormat f) {
    switch (f) {
    case VertexFormat::Uint8x2:   return VK_FORMAT_R8G8_UINT;
    case VertexFormat::Uint8x4:   return VK_FORMAT_R8G8B8A8_UINT;
    case VertexFormat::Sint8x2:   return VK_FORMAT_R8G8_SINT;
    case VertexFormat::Sint8x4:   return VK_FORMAT_R8G8B8A8_SINT;
    case VertexFormat::Unorm8x2:  return VK_FORMAT_R8G8_UNORM;
    case VertexFormat::Unorm8x4:  return VK_FORMAT_R8G8B8A8_UNORM;
    case VertexFormat::Snorm8x2:  return VK_FORMAT_R8G8_SNORM;
    case VertexFormat::Snorm8x4:  return VK_FORMAT_R8G8B8A8_SNORM;
    case VertexFormat::Uint16x2:  return VK_FORMAT_R16G16_UINT;
    case VertexFormat::Uint16x4:  return VK_FORMAT_R16G16B16A16_UINT;
    case VertexFormat::Sint16x2:  return VK_FORMAT_R16G16_SINT;
    case VertexFormat::Sint16x4:  return VK_FORMAT_R16G16B16A16_SINT;
    case VertexFormat::Unorm16x2: return VK_FORMAT_R16G16_UNORM;
    case VertexFormat::Unorm16x4: return VK_FORMAT_R16G16B16A16_UNORM;
    case VertexFormat::Snorm16x2: return VK_FORMAT_R16G16_SNORM;
    case VertexFormat::Snorm16x4: return VK_FORMAT_R16G16B16A16_SNORM;
    case VertexFormat::Float16x2: return VK_FORMAT_R16G16_SFLOAT;
    case VertexFormat::Float16x4: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case VertexFormat::Float32:   return VK_FORMAT_R32_SFLOAT;
    case VertexFormat::Float32x2: return VK_FORMAT_R32G32_SFLOAT;
    case VertexFormat::Float32x3: return VK_FORMAT_R32G32B32_SFLOAT;
    case VertexFormat::Float32x4: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case VertexFormat::Uint32:    return VK_FORMAT_R32_UINT;
    case VertexFormat::Uint32x2:  return VK_FORMAT_R32G32_UINT;
    case VertexFormat::Uint32x3:  return VK_FORMAT_R32G32B32_UINT;
    case VertexFormat::Uint32x4:  return VK_FORMAT_R32G32B32A32_UINT;
    case VertexFormat::Sint32:    return VK_FORMAT_R32_SINT;
    case VertexFormat::Sint32x2:  return VK_FORMAT_R32G32_SINT;
    case VertexFormat::Sint32x3:  return VK_FORMAT_R32G32B32_SINT;
    case VertexFormat::Sint32x4:  return VK_FORMAT_R32G32B32A32_SINT;
    default:                      return VK_FORMAT_UNDEFINED;
    }
}

inline VkBufferUsageFlags toVkBufferUsage(BufferUsage u) {
    VkBufferUsageFlags f = 0;
    if (hasFlag(u, BufferUsage::CopySrc))   f |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (hasFlag(u, BufferUsage::CopyDst))   f |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (hasFlag(u, BufferUsage::Vertex))    f |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::Index))     f |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::Uniform))   f |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::Storage))   f |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::StorageRead))f|= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::Indirect))  f |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (hasFlag(u, BufferUsage::AccelStructInput))
        f |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (hasFlag(u, BufferUsage::ShaderBindingTable))
        f |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (hasFlag(u, BufferUsage::AccelStructScratch))
        f |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    return f;
}

inline VkImageUsageFlags toVkImageUsage(TextureUsage u) {
    VkImageUsageFlags f = 0;
    if ((static_cast<u32>(u) & static_cast<u32>(TextureUsage::CopySrc))        != 0) f |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if ((static_cast<u32>(u) & static_cast<u32>(TextureUsage::CopyDst))        != 0) f |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if ((static_cast<u32>(u) & static_cast<u32>(TextureUsage::Sampled))        != 0) f |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if ((static_cast<u32>(u) & static_cast<u32>(TextureUsage::Storage))        != 0) f |= VK_IMAGE_USAGE_STORAGE_BIT;
    if ((static_cast<u32>(u) & static_cast<u32>(TextureUsage::RenderTarget))   != 0) f |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if ((static_cast<u32>(u) & static_cast<u32>(TextureUsage::DepthStencil))   != 0) f |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if ((static_cast<u32>(u) & static_cast<u32>(TextureUsage::InputAttachment))!= 0) f |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    return f;
}

inline VkImageType toVkImageType(TextureDimension d) {
    switch (d) {
    case TextureDimension::Texture1D: return VK_IMAGE_TYPE_1D;
    case TextureDimension::Texture2D: return VK_IMAGE_TYPE_2D;
    case TextureDimension::Texture3D: return VK_IMAGE_TYPE_3D;
    } return VK_IMAGE_TYPE_2D;
}

inline VkImageViewType toVkImageViewType(TextureViewDimension d) {
    switch (d) {
    case TextureViewDimension::Texture1D:        return VK_IMAGE_VIEW_TYPE_1D;
    case TextureViewDimension::Texture1DArray:   return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case TextureViewDimension::Texture2D:        return VK_IMAGE_VIEW_TYPE_2D;
    case TextureViewDimension::Texture2DArray:   return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case TextureViewDimension::TextureCube:      return VK_IMAGE_VIEW_TYPE_CUBE;
    case TextureViewDimension::TextureCubeArray: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    case TextureViewDimension::Texture3D:        return VK_IMAGE_VIEW_TYPE_3D;
    } return VK_IMAGE_VIEW_TYPE_2D;
}

inline VkFilter             toVkFilter(FilterMode m)       { return m == FilterMode::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR; }
inline VkSamplerMipmapMode  toVkMipmapMode(MipmapFilterMode m) { return m == MipmapFilterMode::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR; }

inline VkSamplerAddressMode toVkAddressMode(AddressMode m) {
    switch (m) {
    case AddressMode::Repeat:       return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case AddressMode::MirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case AddressMode::ClampToEdge:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case AddressMode::ClampToBorder:return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    } return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

inline VkBorderColor toVkBorderColor(SamplerBorderColor c) {
    switch (c) {
    case SamplerBorderColor::TransparentBlack: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    case SamplerBorderColor::OpaqueBlack:      return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case SamplerBorderColor::OpaqueWhite:      return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    } return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
}

inline VkCompareOp toVkCompareOp(CompareFunction f) {
    switch (f) {
    case CompareFunction::Never:        return VK_COMPARE_OP_NEVER;
    case CompareFunction::Less:         return VK_COMPARE_OP_LESS;
    case CompareFunction::Equal:        return VK_COMPARE_OP_EQUAL;
    case CompareFunction::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareFunction::Greater:      return VK_COMPARE_OP_GREATER;
    case CompareFunction::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
    case CompareFunction::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareFunction::Always:       return VK_COMPARE_OP_ALWAYS;
    } return VK_COMPARE_OP_NEVER;
}

inline VkPrimitiveTopology toVkTopology(PrimitiveTopology t) {
    switch (t) {
    case PrimitiveTopology::PointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case PrimitiveTopology::LineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case PrimitiveTopology::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case PrimitiveTopology::TriangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    } return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

inline VkFrontFace      toVkFrontFace(FrontFace f) { return f == FrontFace::CCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE; }
inline VkCullModeFlags  toVkCullMode(CullMode m)   { switch (m) { case CullMode::None: return VK_CULL_MODE_NONE; case CullMode::Front: return VK_CULL_MODE_FRONT_BIT; case CullMode::Back: return VK_CULL_MODE_BACK_BIT; } return VK_CULL_MODE_NONE; }
inline VkPolygonMode    toVkPolygonMode(FillMode m) { return m == FillMode::Solid ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_LINE; }

inline VkBlendFactor toVkBlendFactor(BlendFactor f) {
    switch (f) {
    case BlendFactor::Zero:             return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One:              return VK_BLEND_FACTOR_ONE;
    case BlendFactor::Src:              return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendFactor::OneMinusSrc:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFactor::SrcAlpha:         return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::Dst:              return VK_BLEND_FACTOR_DST_COLOR;
    case BlendFactor::OneMinusDst:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BlendFactor::DstAlpha:         return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case BlendFactor::SrcAlphaSaturated:return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case BlendFactor::Constant:         return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case BlendFactor::OneMinusConstant: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    } return VK_BLEND_FACTOR_ZERO;
}

inline VkBlendOp toVkBlendOp(BlendOperation o) {
    switch (o) {
    case BlendOperation::Add:             return VK_BLEND_OP_ADD;
    case BlendOperation::Subtract:        return VK_BLEND_OP_SUBTRACT;
    case BlendOperation::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
    case BlendOperation::Min:             return VK_BLEND_OP_MIN;
    case BlendOperation::Max:             return VK_BLEND_OP_MAX;
    } return VK_BLEND_OP_ADD;
}

inline VkStencilOp toVkStencilOp(StencilOperation o) {
    switch (o) {
    case StencilOperation::Keep:           return VK_STENCIL_OP_KEEP;
    case StencilOperation::Zero:           return VK_STENCIL_OP_ZERO;
    case StencilOperation::Replace:        return VK_STENCIL_OP_REPLACE;
    case StencilOperation::IncrementClamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case StencilOperation::DecrementClamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case StencilOperation::Invert:         return VK_STENCIL_OP_INVERT;
    case StencilOperation::IncrementWrap:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case StencilOperation::DecrementWrap:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    } return VK_STENCIL_OP_KEEP;
}

inline VkAttachmentLoadOp  toVkLoadOp(LoadOp o)  { switch (o) { case LoadOp::Load: return VK_ATTACHMENT_LOAD_OP_LOAD; case LoadOp::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR; case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE; } return VK_ATTACHMENT_LOAD_OP_CLEAR; }
inline VkAttachmentStoreOp toVkStoreOp(StoreOp o) { return o == StoreOp::Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE; }
inline VkIndexType         toVkIndexType(IndexFormat f) { return f == IndexFormat::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32; }

inline VkPresentModeKHR toVkPresentMode(PresentMode m) {
    switch (m) {
    case PresentMode::Immediate:   return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case PresentMode::Mailbox:     return VK_PRESENT_MODE_MAILBOX_KHR;
    case PresentMode::Fifo:        return VK_PRESENT_MODE_FIFO_KHR;
    case PresentMode::FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    } return VK_PRESENT_MODE_FIFO_KHR;
}

inline VkImageAspectFlags getAspectMask(TextureFormat f) {
    if (isDepthStencil(f)) {
        VkImageAspectFlags a = 0;
        if (hasDepth(f))   a |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencil(f)) a |= VK_IMAGE_ASPECT_STENCIL_BIT;
        return a;
    }
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

inline VkSampleCountFlagBits toVkSampleCount(u32 c) {
    switch (c) {
    case 1:  return VK_SAMPLE_COUNT_1_BIT;
    case 2:  return VK_SAMPLE_COUNT_2_BIT;
    case 4:  return VK_SAMPLE_COUNT_4_BIT;
    case 8:  return VK_SAMPLE_COUNT_8_BIT;
    case 16: return VK_SAMPLE_COUNT_16_BIT;
    case 32: return VK_SAMPLE_COUNT_32_BIT;
    case 64: return VK_SAMPLE_COUNT_64_BIT;
    default: return VK_SAMPLE_COUNT_1_BIT;
    }
}

inline VkColorComponentFlags toVkColorWriteMask(ColorWriteMask m) {
    VkColorComponentFlags f = 0;
    if (static_cast<u8>(m) & static_cast<u8>(ColorWriteMask::Red))   f |= VK_COLOR_COMPONENT_R_BIT;
    if (static_cast<u8>(m) & static_cast<u8>(ColorWriteMask::Green)) f |= VK_COLOR_COMPONENT_G_BIT;
    if (static_cast<u8>(m) & static_cast<u8>(ColorWriteMask::Blue))  f |= VK_COLOR_COMPONENT_B_BIT;
    if (static_cast<u8>(m) & static_cast<u8>(ColorWriteMask::Alpha)) f |= VK_COLOR_COMPONENT_A_BIT;
    return f;
}

inline VkShaderStageFlags toVkShaderStageFlags(ShaderStage s) {
    VkShaderStageFlags f = 0;
    auto v = static_cast<u32>(s);
    if (v & static_cast<u32>(ShaderStage::Vertex))       f |= VK_SHADER_STAGE_VERTEX_BIT;
    if (v & static_cast<u32>(ShaderStage::Fragment))     f |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (v & static_cast<u32>(ShaderStage::Compute))      f |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (v & static_cast<u32>(ShaderStage::Mesh))         f |= VK_SHADER_STAGE_MESH_BIT_EXT;
    if (v & static_cast<u32>(ShaderStage::Task))         f |= VK_SHADER_STAGE_TASK_BIT_EXT;
    if (v & static_cast<u32>(ShaderStage::RayGen))       f |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    if (v & static_cast<u32>(ShaderStage::ClosestHit))   f |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    if (v & static_cast<u32>(ShaderStage::Miss))         f |= VK_SHADER_STAGE_MISS_BIT_KHR;
    if (v & static_cast<u32>(ShaderStage::AnyHit))       f |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    if (v & static_cast<u32>(ShaderStage::Intersection)) f |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    if (v & static_cast<u32>(ShaderStage::Callable))     f |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    return f;
}

} // namespace draco::rhi::vk
