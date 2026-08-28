/// GLTF/GLB model loader using cgltf.

module;
#include <span>

#include <algorithm>
#include <utility>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "cgltf.h"

export module model:gltf;

import core;
import :model;
import :io;
import image;
import image.io;

export namespace draco::model::gltf {

using namespace draco;
using namespace draco::model;

// cgltf hands back char* (UTF-8); the engine std::u8string is UTF-8 too, so this just
// wraps the bytes in an owned std::u8string - no transcoding.
inline std::u8string utf8FromC(const char* s) {
    if (!s) return std::u8string{};
    return std::u8string(std::u8string_view(reinterpret_cast<const char8_t*>(s)));
}

/// Loads GLTF and GLB model files using cgltf.
class GltfLoader : public io::ModelLoader {
public:
    GltfLoader() = default;

    ~GltfLoader() override {
        if (m_data) {
            cgltf_free(m_data);
            m_data = nullptr;
        }
    }

    bool supportsExtension(std::u8string_view ext) const override {
        return caseInsensitiveEquals(ext, u8".gltf") || caseInsensitiveEquals(ext, u8".glb");
    }

    // Non-copyable.
    GltfLoader(const GltfLoader&) = delete;
    GltfLoader& operator=(const GltfLoader&) = delete;

    /// Load a GLTF or GLB file.
    ModelLoadResult load(std::u8string_view path, Model& model) override {
        // Free previous data.
        if (m_data) {
            cgltf_free(m_data);
            m_data = nullptr;
        }

        // Extract base path for loading external resources.
        const std::u8string narrowStorage = std::u8string(path);
        const char* narrowPath = reinterpret_cast<const char*>(narrowStorage.c_str());
        std::filesystem::path filePath(narrowPath);
        m_basePath = filePath.parent_path().string();

        // Parse the file -- explicitly set type for .glb since auto-detect can fail.
        cgltf_options options = {};

        if (endsWithCI(path, u8".glb"))
            options.type = cgltf_file_type_glb;
        // json_token_count = 0 lets cgltf size the JSON token pool itself (a counting pass first).
        // A fixed cap (was 4096) silently fails to parse larger glTFs with invalid_json (result=3) -
        // e.g. the Quaternius character (~1MB, >4096 JSON tokens).
        options.json_token_count = 0;

        // Read file data ourselves (cgltf's fopen can fail with certain path formats on Windows).
        std::ifstream file(narrowPath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
            return ModelLoadResult::FileNotFound;

        auto fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> fileBytes(static_cast<size_t>(fileSize));
        if (!file.read(reinterpret_cast<char*>(fileBytes.data()), fileSize))
            return ModelLoadResult::FileNotFound;
        file.close();

        cgltf_result parseResult = cgltf_parse(&options, fileBytes.data(),
                                                fileBytes.size(), &m_data);
        if (parseResult != cgltf_result_success) {
            fprintf(stderr, "  cgltf_parse failed: result=%d, size=%zu, path=%s\n",
                    static_cast<int>(parseResult), fileBytes.size(), narrowPath);
            return ModelLoadResult::ParseError;
        }

        // Load buffer data (for external .bin references; GLB has embedded buffers).
        cgltf_result loadResult = cgltf_load_buffers(&options, m_data, narrowPath);
        if (loadResult != cgltf_result_success)
            return ModelLoadResult::InvalidData;

        // Validate (optional -- some exporters produce spec-violating but loadable files).
        cgltf_result validateResult = cgltf_validate(m_data);
        if (validateResult != cgltf_result_success) {
            fprintf(stderr, "  cgltf_validate warning: result=%d (continuing anyway)\n",
                    static_cast<int>(validateResult));
        }

        // GLTF is always Y-up per specification.
        model.originalUpAxis = CoordinateAxis::PositiveY;

        // Convert to Model.
        loadMaterials(model);
        loadTextures(model);
        loadMeshes(model);
        loadNodes(model);
        loadSkins(model);
        loadAnimations(model);

        model.buildBoneHierarchy();
        model.calculateBounds();

        return ModelLoadResult::Ok;
    }

private:
    cgltf_data* m_data = nullptr;
    std::string m_basePath;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    static bool caseInsensitiveEquals(std::u8string_view a, std::u8string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        }
        return true;
    }

    static bool endsWithCI(std::u8string_view str, std::u8string_view suffix) {
        if (str.size() < suffix.size()) return false;
        return caseInsensitiveEquals(str.substr(str.size() - suffix.size(), suffix.size()), suffix);
    }

    // -----------------------------------------------------------------------
    // Materials
    // -----------------------------------------------------------------------

    void loadMaterials(Model& model) {
        for (cgltf_size i = 0; i < m_data->materials_count; ++i) {
            cgltf_material* mat = &m_data->materials[i];
            auto* material = new ModelMaterial();

            if (mat->name)
                material->setName(utf8FromC(mat->name));
            else {
                char buf[64];
                snprintf(buf, sizeof(buf), "m_texture%zu", i);
                material->setName(utf8FromC(buf));
            }

            // PBR Metallic Roughness.
            if (mat->has_pbr_metallic_roughness) {
                cgltf_pbr_metallic_roughness* pbr = &mat->pbr_metallic_roughness;

                material->baseColorFactor = math::Vector4(
                    pbr->base_color_factor[0],
                    pbr->base_color_factor[1],
                    pbr->base_color_factor[2],
                    pbr->base_color_factor[3]
                );

                if (pbr->base_color_texture.texture)
                    material->baseColorTextureIndex = static_cast<i32>(
                        cgltf_texture_index(m_data, pbr->base_color_texture.texture));

                material->metallicFactor = pbr->metallic_factor;
                material->roughnessFactor = pbr->roughness_factor;

                if (pbr->metallic_roughness_texture.texture)
                    material->metallicRoughnessTextureIndex = static_cast<i32>(
                        cgltf_texture_index(m_data, pbr->metallic_roughness_texture.texture));
            }

            // Normal texture.
            if (mat->normal_texture.texture) {
                material->normalTextureIndex = static_cast<i32>(
                    cgltf_texture_index(m_data, mat->normal_texture.texture));
                material->normalScale = mat->normal_texture.scale;
            }

            // Occlusion texture.
            if (mat->occlusion_texture.texture) {
                material->occlusionTextureIndex = static_cast<i32>(
                    cgltf_texture_index(m_data, mat->occlusion_texture.texture));
                material->occlusionStrength = mat->occlusion_texture.scale;
            }

            // Emissive.
            material->emissiveFactor = math::Vector3(
                mat->emissive_factor[0],
                mat->emissive_factor[1],
                mat->emissive_factor[2]
            );

            if (mat->emissive_texture.texture)
                material->emissiveTextureIndex = static_cast<i32>(
                    cgltf_texture_index(m_data, mat->emissive_texture.texture));

            // Alpha.
            switch (mat->alpha_mode) {
            case cgltf_alpha_mode_mask:  material->alphaMode = AlphaMode::Mask; break;
            case cgltf_alpha_mode_blend: material->alphaMode = AlphaMode::Blend; break;
            default:                     material->alphaMode = AlphaMode::Opaque; break;
            }

            material->alphaCutoff = mat->alpha_cutoff;
            material->doubleSided = mat->double_sided != 0;

            model.addMaterial(material);
        }
    }

    // -----------------------------------------------------------------------
    // Textures
    // -----------------------------------------------------------------------

    static TextureWrap wrapModeFromCgltf(cgltf_wrap_mode mode) {
        switch (mode) {
        case cgltf_wrap_mode_clamp_to_edge:    return TextureWrap::ClampToEdge;
        case cgltf_wrap_mode_mirrored_repeat:  return TextureWrap::MirroredRepeat;
        default:                               return TextureWrap::Repeat;
        }
    }

    static TextureMinFilter minFilterFromCgltf(cgltf_filter_type filter) {
        switch (filter) {
        case cgltf_filter_type_nearest:                return TextureMinFilter::Nearest;
        case cgltf_filter_type_linear:                 return TextureMinFilter::Linear;
        case cgltf_filter_type_nearest_mipmap_nearest: return TextureMinFilter::NearestMipmapNearest;
        case cgltf_filter_type_linear_mipmap_nearest:  return TextureMinFilter::LinearMipmapNearest;
        case cgltf_filter_type_nearest_mipmap_linear:  return TextureMinFilter::NearestMipmapLinear;
        case cgltf_filter_type_linear_mipmap_linear:   return TextureMinFilter::LinearMipmapLinear;
        default:                                       return TextureMinFilter::Nearest;
        }
    }

    static TextureMagFilter magFilterFromCgltf(cgltf_filter_type filter) {
        switch (filter) {
        case cgltf_filter_type_nearest:
        case cgltf_filter_type_nearest_mipmap_nearest:
        case cgltf_filter_type_nearest_mipmap_linear:
            return TextureMagFilter::Nearest;
        case cgltf_filter_type_linear:
        case cgltf_filter_type_linear_mipmap_nearest:
        case cgltf_filter_type_linear_mipmap_linear:
            return TextureMagFilter::Linear;
        default:
            return TextureMagFilter::Nearest;
        }
    }

    void loadTextures(Model& model) {

        for (cgltf_size i = 0; i < m_data->textures_count; ++i) {
            cgltf_texture* tex = &m_data->textures[i];
            auto* texture = new ModelTexture();

            if (tex->name)
                texture->setName(utf8FromC(tex->name));
            else if (tex->image && tex->image->name)
                texture->setName(utf8FromC(tex->image->name));

            if (tex->sampler)
                texture->samplerIndex = static_cast<i32>(
                    cgltf_sampler_index(m_data, tex->sampler));

            if (tex->image) {
                cgltf_image* gltfImage = tex->image;

                if (gltfImage->mime_type)
                    texture->mimeType = utf8FromC(gltfImage->mime_type);

                if (gltfImage->uri) {
                    const char* uriC = gltfImage->uri;
                    texture->setUri(utf8FromC(uriC));

                    if (std::strncmp(uriC, "data:", 5) == 0) {
                        // Base64 encoded data URI (e.g., "data:image/png;base64,iVBORw0...")
                        draco::image::Image img;
                        if (loadImageFromDataUri(uriC, img))
                            storeImageData(img, texture);
                    } else {
                        // External image file.
                        std::filesystem::path imagePath =
                            std::filesystem::path(m_basePath) / uriC;
                        const std::string imgPath = imagePath.string();
                        draco::image::Image img;
                        if (image::io::loadImage(utf8FromC(imgPath.c_str()), img) == ErrorCode::Ok)
                            storeImageData(img, texture);
                    }
                } else if (gltfImage->buffer_view) {
                    // Embedded image data in buffer.
                    const uint8_t* bufferData = cgltf_buffer_view_data(gltfImage->buffer_view);
                    size_t size = gltfImage->buffer_view->size;
                    if (bufferData && size > 0) {
                        draco::image::Image img;
                        if (image::io::loadImageFromMemory(
                                std::span<const u8>(bufferData, size), img) == ErrorCode::Ok)
                            storeImageData(img, texture);
                    }
                }
            }

            model.addTexture(texture);
        }

        // Load samplers.
        for (cgltf_size i = 0; i < m_data->samplers_count; ++i) {
            cgltf_sampler* samp = &m_data->samplers[i];
            TextureSampler sampler{};

            sampler.wrapS = wrapModeFromCgltf(samp->wrap_s);
            sampler.wrapT = wrapModeFromCgltf(samp->wrap_t);
            sampler.minFilter = minFilterFromCgltf(samp->min_filter);
            sampler.magFilter = magFilterFromCgltf(samp->mag_filter);

            model.addSampler(sampler);
        }

        // Ensure all textures have a valid sampler.
        // Per glTF spec, textures without an explicit sampler default to Repeat wrapping.
        for (auto* texture : model.textures()) {
            if (texture->samplerIndex < 0) {
                // Add a default glTF sampler (Repeat/Repeat) and assign it.
                TextureSampler defaultSampler{}; // Defaults: WrapS=Repeat, WrapT=Repeat.
                texture->samplerIndex = model.addSampler(defaultSampler);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Image helpers
    // -----------------------------------------------------------------------

    static TexturePixelFormat convertPixelFormat(draco::image::PixelFormat fmt) {
        switch (fmt) {
        case draco::image::PixelFormat::R8:    return TexturePixelFormat::R8;
        case draco::image::PixelFormat::RG8:   return TexturePixelFormat::RG8;
        case draco::image::PixelFormat::RGB8:  return TexturePixelFormat::RGB8;
        case draco::image::PixelFormat::RGBA8: return TexturePixelFormat::RGBA8;
        case draco::image::PixelFormat::BGR8:  return TexturePixelFormat::BGR8;
        case draco::image::PixelFormat::BGRA8: return TexturePixelFormat::BGRA8;
        default: return TexturePixelFormat::Unknown;
        }
    }

    static void storeImageData(const draco::image::Image& image, ModelTexture* texture) {
        texture->width  = static_cast<i32>(image.width());
        texture->height = static_cast<i32>(image.height());
        texture->pixelFormat = convertPixelFormat(image.format());

        auto data = image.pixelData();
        if (data.data() && data.size() > 0) {
            auto* copy = new u8[data.size()];
            std::memcpy(copy, data.data(), data.size());
            texture->setData(copy, static_cast<i32>(data.size()));
        }
    }

    bool loadImageFromDataUri(const char* dataUri, draco::image::Image& outImage) {

        // Format: data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAA...
        const char* comma = std::strchr(dataUri, ',');
        if (!comma)
            return false;

        const char* base64Data = comma + 1;

        // Use cgltf's base64 decoder.
        size_t estimatedSize = (std::strlen(base64Data) * 3) / 4;
        void* decodedData = nullptr;
        cgltf_options options = {};

        // cgltf_load_buffer_base64 expects a null-terminated string.
        if (cgltf_load_buffer_base64(&options, estimatedSize, base64Data, &decodedData)
                != cgltf_result_success)
            return false;

        bool ok = image::io::loadImageFromMemory(
            std::span<const u8>(static_cast<const u8*>(decodedData), estimatedSize),
            outImage) == ErrorCode::Ok;

        std::free(decodedData);
        return ok;
    }

    // -----------------------------------------------------------------------
    // Meshes
    // -----------------------------------------------------------------------

    void loadMeshes(Model& model) {
        for (cgltf_size i = 0; i < m_data->meshes_count; ++i) {
            cgltf_mesh* meshData = &m_data->meshes[i];
            auto* mesh = new ModelMesh();

            if (meshData->name)
                mesh->setName(utf8FromC(meshData->name));

            if (meshData->primitives_count > 0)
                loadMeshPrimitives(meshData, mesh);

            mesh->calculateBounds();
            model.addMesh(mesh);
        }
    }

    /// Loads all primitives of a GLTF mesh, merging their vertex/index data into one ModelMesh.
    /// Each primitive becomes a ModelMeshPart with correct index offsets and material index.
    void loadMeshPrimitives(cgltf_mesh* meshData, ModelMesh* mesh) {
        cgltf_primitive* firstPrim = &meshData->primitives[0];

        // Phase 1: Detect skinning and available attributes from first primitive.
        bool isSkinned = false;
        bool hasNormals = false;
        bool hasTangents = false;
        bool hasTexCoords = false;
        for (cgltf_size a = 0; a < firstPrim->attributes_count; ++a) {
            cgltf_attribute* attr = &firstPrim->attributes[a];
            if (attr->type == cgltf_attribute_type_joints && attr->index == 0)
                isSkinned = true;
            if (attr->type == cgltf_attribute_type_normal)
                hasNormals = true;
            if (attr->type == cgltf_attribute_type_tangent)
                hasTangents = true;
            if (attr->type == cgltf_attribute_type_texcoord && attr->index == 0)
                hasTexCoords = true;
        }
        (void)hasTexCoords; // Used only for detection, not conditionally.

        mesh->setHasNormals(hasNormals);
        mesh->setHasTangents(hasTangents);

        i32 stride = 0;
        i32 positionOffset = stride;
        stride += static_cast<i32>(sizeof(math::Vector3));
        mesh->addVertexElement(VertexElement(VertexSemantic::Position, VertexElementFormat::Float3, positionOffset));

        i32 normalOffset = stride;
        stride += static_cast<i32>(sizeof(math::Vector3));
        mesh->addVertexElement(VertexElement(VertexSemantic::Normal, VertexElementFormat::Float3, normalOffset));

        i32 texCoordOffset = stride;
        stride += static_cast<i32>(sizeof(math::Vector2));
        mesh->addVertexElement(VertexElement(VertexSemantic::TexCoord, VertexElementFormat::Float2, texCoordOffset));

        i32 colorOffset = stride;
        stride += static_cast<i32>(sizeof(u32));
        mesh->addVertexElement(VertexElement(VertexSemantic::Color, VertexElementFormat::Byte4, colorOffset));

        i32 tangentOffset = stride;
        stride += static_cast<i32>(sizeof(math::Vector3));
        mesh->addVertexElement(VertexElement(VertexSemantic::Tangent, VertexElementFormat::Float3, tangentOffset));

        i32 jointsOffset = 0;
        i32 weightsOffset = 0;
        if (isSkinned) {
            jointsOffset = stride;
            stride += static_cast<i32>(sizeof(u16) * 4);
            mesh->addVertexElement(VertexElement(VertexSemantic::Joints, VertexElementFormat::UShort4, jointsOffset));

            weightsOffset = stride;
            stride += static_cast<i32>(sizeof(math::Vector4));
            mesh->addVertexElement(VertexElement(VertexSemantic::Weights, VertexElementFormat::Float4, weightsOffset));
        }

        // Phase 2: Count total vertices and indices across all primitives.
        i32 totalVertexCount = 0;
        i32 totalIndexCount = 0;
        for (cgltf_size p = 0; p < meshData->primitives_count; ++p) {
            cgltf_primitive* prim = &meshData->primitives[p];
            for (cgltf_size a = 0; a < prim->attributes_count; ++a) {
                if (prim->attributes[a].type == cgltf_attribute_type_position) {
                    totalVertexCount += static_cast<i32>(prim->attributes[a].data->count);
                    break;
                }
            }
            if (prim->indices) {
                totalIndexCount += static_cast<i32>(prim->indices->count);
            } else {
                // Non-indexed primitive: we will generate sequential indices, one
                // per position vertex. GLTF allows non-indexed geometry but we
                // require indices for rendering (else the primitive is dropped).
                for (cgltf_size a = 0; a < prim->attributes_count; ++a) {
                    if (prim->attributes[a].type == cgltf_attribute_type_position) {
                        totalIndexCount += static_cast<i32>(prim->attributes[a].data->count);
                        break;
                    }
                }
            }
        }

        if (totalVertexCount == 0)
            return;

        // Phase 3: Allocate buffers for all primitives combined.
        bool use32Bit = totalIndexCount > 65535 || totalVertexCount > 65535;
        mesh->allocateVertices(totalVertexCount, stride);
        mesh->allocateIndices(totalIndexCount, use32Bit);

        u8* vertexData = mesh->getVertexData();
        u8* indexData = mesh->getIndexData();

        // Phase 4: Load each primitive's data at correct offset.
        i32 vertexOffset = 0;
        i32 indexOffset = 0;

        for (cgltf_size p = 0; p < meshData->primitives_count; ++p) {
            cgltf_primitive* prim = &meshData->primitives[p];

            // Find accessors for this primitive.
            cgltf_accessor* positionAccessor = nullptr;
            cgltf_accessor* normalAccessor = nullptr;
            cgltf_accessor* texCoordAccessor = nullptr;
            cgltf_accessor* colorAccessor = nullptr;
            cgltf_accessor* tangentAccessor = nullptr;
            cgltf_accessor* jointsAccessor = nullptr;
            cgltf_accessor* weightsAccessor = nullptr;

            for (cgltf_size a = 0; a < prim->attributes_count; ++a) {
                cgltf_attribute* attr = &prim->attributes[a];
                switch (attr->type) {
                case cgltf_attribute_type_position: positionAccessor = attr->data; break;
                case cgltf_attribute_type_normal:   normalAccessor = attr->data; break;
                case cgltf_attribute_type_texcoord:
                    if (attr->index == 0) texCoordAccessor = attr->data;
                    break;
                case cgltf_attribute_type_color:
                    if (attr->index == 0) colorAccessor = attr->data;
                    break;
                case cgltf_attribute_type_tangent:   tangentAccessor = attr->data; break;
                case cgltf_attribute_type_joints:
                    if (attr->index == 0) jointsAccessor = attr->data;
                    break;
                case cgltf_attribute_type_weights:
                    if (attr->index == 0) weightsAccessor = attr->data;
                    break;
                default: break;
                }
            }

            if (!positionAccessor)
                continue;

            i32 primVertexCount = static_cast<i32>(positionAccessor->count);

            // Fill vertex data at offset.
            for (i32 v = 0; v < primVertexCount; ++v) {
                u8* vertex = vertexData + (vertexOffset + v) * stride;

                // Position.
                float pos[3] = {};
                cgltf_accessor_read_float(positionAccessor, static_cast<cgltf_size>(v), pos, 3);
                *reinterpret_cast<math::Vector3*>(vertex + positionOffset) = math::Vector3(pos[0], pos[1], pos[2]);

                // Normal.
                if (normalAccessor) {
                    float normal[3] = {};
                    cgltf_accessor_read_float(normalAccessor, static_cast<cgltf_size>(v), normal, 3);
                    *reinterpret_cast<math::Vector3*>(vertex + normalOffset) = math::Vector3(normal[0], normal[1], normal[2]);
                } else {
                    *reinterpret_cast<math::Vector3*>(vertex + normalOffset) = math::Vector3(0, 1, 0);
                }

                // TexCoord.
                if (texCoordAccessor) {
                    float uv[2] = {};
                    cgltf_accessor_read_float(texCoordAccessor, static_cast<cgltf_size>(v), uv, 2);
                    *reinterpret_cast<math::Vector2*>(vertex + texCoordOffset) = math::Vector2(uv[0], uv[1]);
                }

                // Color.
                if (colorAccessor) {
                    float color[4] = { 1, 1, 1, 1 };
                    cgltf_accessor_read_float(colorAccessor, static_cast<cgltf_size>(v), color, 4);
                    u8 cr = static_cast<u8>(std::clamp(color[0] * 255.0f, 0.0f, 255.0f));
                    u8 cg = static_cast<u8>(std::clamp(color[1] * 255.0f, 0.0f, 255.0f));
                    u8 cb = static_cast<u8>(std::clamp(color[2] * 255.0f, 0.0f, 255.0f));
                    u8 ca = static_cast<u8>(std::clamp(color[3] * 255.0f, 0.0f, 255.0f));
                    *reinterpret_cast<u32*>(vertex + colorOffset) =
                        static_cast<u32>(cr) |
                        (static_cast<u32>(cg) << 8) |
                        (static_cast<u32>(cb) << 16) |
                        (static_cast<u32>(ca) << 24);
                } else {
                    *reinterpret_cast<u32*>(vertex + colorOffset) = 0xFFFFFFFF;
                }

                // Tangent.
                if (tangentAccessor) {
                    float tangent[4] = {};
                    cgltf_accessor_read_float(tangentAccessor, static_cast<cgltf_size>(v), tangent, 4);
                    *reinterpret_cast<math::Vector3*>(vertex + tangentOffset) = math::Vector3(tangent[0], tangent[1], tangent[2]);
                } else {
                    *reinterpret_cast<math::Vector3*>(vertex + tangentOffset) = math::Vector3(1, 0, 0);
                }

                // Skinning data.
                if (isSkinned && jointsAccessor && weightsAccessor) {
                    cgltf_uint joints[4] = {};
                    cgltf_accessor_read_uint(jointsAccessor, static_cast<cgltf_size>(v), joints, 4);
                    auto* jointsDst = reinterpret_cast<u16*>(vertex + jointsOffset);
                    jointsDst[0] = static_cast<u16>(joints[0]);
                    jointsDst[1] = static_cast<u16>(joints[1]);
                    jointsDst[2] = static_cast<u16>(joints[2]);
                    jointsDst[3] = static_cast<u16>(joints[3]);

                    float weights[4] = {};
                    cgltf_accessor_read_float(weightsAccessor, static_cast<cgltf_size>(v), weights, 4);
                    *reinterpret_cast<math::Vector4*>(vertex + weightsOffset) = math::Vector4(weights[0], weights[1], weights[2], weights[3]);
                }
            }

            // Fill index data at offset (indices must be remapped by vertexOffset).
            i32 primIndexCount = 0;
            if (prim->indices) {
                primIndexCount = static_cast<i32>(prim->indices->count);

                if (use32Bit) {
                    u32* indices = reinterpret_cast<u32*>(indexData + indexOffset * 4);
                    for (i32 idx = 0; idx < primIndexCount; ++idx)
                        indices[idx] = static_cast<u32>(cgltf_accessor_read_index(prim->indices, static_cast<cgltf_size>(idx))) + static_cast<u32>(vertexOffset);
                } else {
                    u16* indices = reinterpret_cast<u16*>(indexData + indexOffset * 2);
                    for (i32 idx = 0; idx < primIndexCount; ++idx)
                        indices[idx] = static_cast<u16>(static_cast<u32>(cgltf_accessor_read_index(prim->indices, static_cast<cgltf_size>(idx))) + static_cast<u32>(vertexOffset));
                }
            } else {
                // Non-indexed primitive: generate sequential indices (0, 1, 2, ...).
                primIndexCount = primVertexCount;
                if (use32Bit) {
                    u32* indices = reinterpret_cast<u32*>(indexData + indexOffset * 4);
                    for (i32 idx = 0; idx < primIndexCount; ++idx)
                        indices[idx] = static_cast<u32>(vertexOffset + idx);
                } else {
                    u16* indices = reinterpret_cast<u16*>(indexData + indexOffset * 2);
                    for (i32 idx = 0; idx < primIndexCount; ++idx)
                        indices[idx] = static_cast<u16>(vertexOffset + idx);
                }
            }

            // Add mesh part with correct offset.
            i32 materialIndex = -1;
            if (prim->material)
                materialIndex = static_cast<i32>(cgltf_material_index(m_data, prim->material));

            if (primIndexCount > 0)
                mesh->addPart(ModelMeshPart(indexOffset, primIndexCount, materialIndex));

            vertexOffset += primVertexCount;
            indexOffset += primIndexCount;
        }

        // Set topology from first primitive.
        switch (firstPrim->type) {
        case cgltf_primitive_type_triangles:      mesh->setTopology(PrimitiveTopology::Triangles); break;
        case cgltf_primitive_type_triangle_strip:  mesh->setTopology(PrimitiveTopology::TriangleStrip); break;
        case cgltf_primitive_type_lines:           mesh->setTopology(PrimitiveTopology::Lines); break;
        case cgltf_primitive_type_line_strip:      mesh->setTopology(PrimitiveTopology::LineStrip); break;
        case cgltf_primitive_type_points:          mesh->setTopology(PrimitiveTopology::Points); break;
        default:                                   mesh->setTopology(PrimitiveTopology::Triangles); break;
        }
    }

    // -----------------------------------------------------------------------
    // Nodes
    // -----------------------------------------------------------------------

    void loadNodes(Model& model) {
        // First pass: create all bones.
        for (cgltf_size i = 0; i < m_data->nodes_count; ++i) {
            cgltf_node* node = &m_data->nodes[i];
            auto* bone = new ModelBone();

            if (node->name)
                bone->setName(utf8FromC(node->name));

            // Translation.
            if (node->has_translation)
                bone->translation = math::Vector3(node->translation[0], node->translation[1], node->translation[2]);

            // Rotation.
            if (node->has_rotation)
                bone->rotation = math::Quaternion(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);

            // Scale.
            if (node->has_scale)
                bone->scale = math::Vector3(node->scale[0], node->scale[1], node->scale[2]);

            // Matrix.
            if (node->has_matrix) {
                // GLTF stores column-major for column-vector convention.
                // Transpose to row-vector convention by reading flat array directly into row-major storage.
                std::memcpy(bone->localTransform.data(), node->matrix, sizeof(float) * 16);
            } else {
                bone->updateLocalTransform();
            }

            if (node->mesh)
                bone->meshIndex = static_cast<i32>(cgltf_mesh_index(m_data, node->mesh));

            if (node->skin)
                bone->skinIndex = static_cast<i32>(cgltf_skin_index(m_data, node->skin));

            model.addBone(bone);
        }

        // Second pass: set up parent relationships.
        for (cgltf_size i = 0; i < m_data->nodes_count; ++i) {
            cgltf_node* node = &m_data->nodes[i];
            if (node->parent)
                model.bones()[i]->parentIndex = static_cast<i32>(cgltf_node_index(m_data, node->parent));
        }
    }

    // -----------------------------------------------------------------------
    // Skins
    // -----------------------------------------------------------------------

    void loadSkins(Model& model) {
        for (cgltf_size i = 0; i < m_data->skins_count; ++i) {
            cgltf_skin* skinData = &m_data->skins[i];
            auto* skin = new ModelSkin();

            if (skinData->name)
                skin->setName(utf8FromC(skinData->name));

            if (skinData->skeleton)
                skin->skeletonRootIndex = static_cast<i32>(cgltf_node_index(m_data, skinData->skeleton));

            for (cgltf_size j = 0; j < skinData->joints_count; ++j) {
                cgltf_node* jointNode = skinData->joints[j];
                i32 jointIndex = static_cast<i32>(cgltf_node_index(m_data, jointNode));

                math::Matrix4 ibm = math::Matrix4::identity();
                if (skinData->inverse_bind_matrices) {
                    float mat[16] = {};
                    cgltf_accessor_read_float(skinData->inverse_bind_matrices, j, mat, 16);
                    // GLTF stores column-major for column-vector convention.
                    // Transpose to row-vector convention by reading flat array directly into row-major storage.
                    std::memcpy(ibm.data(), mat, sizeof(float) * 16);
                }

                skin->addJoint(jointIndex, ibm);

                // Also set on the bone.
                if (jointIndex >= 0 && static_cast<usize>(jointIndex) < model.bones().size())
                    model.bones()[jointIndex]->inverseBindMatrix = ibm;
            }

            model.addSkin(skin);
        }
    }

    // -----------------------------------------------------------------------
    // Animations
    // -----------------------------------------------------------------------

    void loadAnimations(Model& model) {
        for (cgltf_size i = 0; i < m_data->animations_count; ++i) {
            cgltf_animation* animData = &m_data->animations[i];
            auto* animation = new ModelAnimation();

            if (animData->name)
                animation->setName(utf8FromC(animData->name));

            for (cgltf_size c = 0; c < animData->channels_count; ++c) {
                cgltf_animation_channel* channelData = &animData->channels[c];
                auto* channel = new AnimationChannel();

                if (channelData->target_node)
                    channel->targetBone = static_cast<i32>(cgltf_node_index(m_data, channelData->target_node));

                switch (channelData->target_path) {
                case cgltf_animation_path_type_translation: channel->path = AnimationPath::Translation; break;
                case cgltf_animation_path_type_rotation:    channel->path = AnimationPath::Rotation; break;
                case cgltf_animation_path_type_scale:       channel->path = AnimationPath::Scale; break;
                case cgltf_animation_path_type_weights:     channel->path = AnimationPath::Weights; break;
                default: break;
                }

                if (channelData->sampler) {
                    cgltf_animation_sampler* sampler = channelData->sampler;

                    switch (sampler->interpolation) {
                    case cgltf_interpolation_type_step:         channel->interpolation = AnimationInterpolation::Step; break;
                    case cgltf_interpolation_type_cubic_spline:  channel->interpolation = AnimationInterpolation::CubicSpline; break;
                    default:                                     channel->interpolation = AnimationInterpolation::Linear; break;
                    }

                    if (sampler->input && sampler->output) {
                        i32 keyframeCount = static_cast<i32>(sampler->input->count);

                        for (i32 k = 0; k < keyframeCount; ++k) {
                            float time = 0;
                            cgltf_accessor_read_float(sampler->input, static_cast<cgltf_size>(k), &time, 1);

                            math::Vector4 value{};
                            switch (channel->path) {
                            case AnimationPath::Translation:
                            case AnimationPath::Scale: {
                                float v3[3] = {};
                                cgltf_accessor_read_float(sampler->output, static_cast<cgltf_size>(k), v3, 3);
                                value = math::Vector4(v3[0], v3[1], v3[2], 0);
                                break;
                            }
                            case AnimationPath::Rotation: {
                                float v4[4] = {};
                                cgltf_accessor_read_float(sampler->output, static_cast<cgltf_size>(k), v4, 4);
                                value = math::Vector4(v4[0], v4[1], v4[2], v4[3]);
                                break;
                            }
                            case AnimationPath::Weights: {
                                float w = 0;
                                cgltf_accessor_read_float(sampler->output, static_cast<cgltf_size>(k), &w, 1);
                                value.x = w;
                                break;
                            }
                            }

                            channel->addKeyframe(time, value);
                        }
                    }
                }

                animation->addChannel(channel);
            }

            animation->calculateDuration();
            model.addAnimation(animation);
        }
    }
};

} // namespace draco::model::gltf
