/// FBX/OBJ model loader using ufbx.

module;
#include <span>

#include <algorithm>
#include <utility>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ufbx.h"

export module model:fbx;

import core;
import :model;
import :io;
import image;
import image.io;

export namespace draco::model::fbx {

using namespace draco;
using namespace draco::model;

// ufbx hands back char* (UTF-8); the engine std::u8string is UTF-8 too, so these just
// wrap the bytes in an owned std::u8string - no transcoding.
inline std::u8string utf8FromC(const char* s) {
    if (!s) return std::u8string{};
    return std::u8string(std::u8string_view(reinterpret_cast<const char8_t*>(s)));
}
inline std::u8string utf8FromUfbx(const ufbx_string& s) {
    return std::u8string(std::u8string_view(reinterpret_cast<const char8_t*>(s.data), s.length));
}

/// Loads FBX and OBJ model files using ufbx.
class FbxLoader : public io::ModelLoader {
public:
    FbxLoader() = default;

    ~FbxLoader() override {
        if (m_scene) {
            ufbx_free_scene(m_scene);
            m_scene = nullptr;
        }
    }

    bool supportsExtension(std::u8string_view ext) const override {
        return caseInsensitiveEquals(ext, u8".fbx") || caseInsensitiveEquals(ext, u8".obj");
    }

    // Non-copyable.
    FbxLoader(const FbxLoader&) = delete;
    FbxLoader& operator=(const FbxLoader&) = delete;

    /// Load an FBX or OBJ file.
    ModelLoadResult load(std::u8string_view path, Model& model) override {
        // Free previous scene.
        if (m_scene) {
            ufbx_free_scene(m_scene);
            m_scene = nullptr;
        }

        // Clear mappings.
        m_nodeToBoneIndex.clear();
        m_materialIndexMap.clear();
        m_textureIndexMap.clear();
        m_meshIndexMap.clear();
        m_skinIndexMap.clear();

        // Extract base path for loading external resources.
        const std::u8string narrowStorage = std::u8string(path);
        const char* narrowPath = reinterpret_cast<const char*>(narrowStorage.c_str());
        std::filesystem::path filePath(narrowPath);
        m_basePath = filePath.parent_path().string();

        // Setup load options.
        m_loadOpts = {};
        m_loadOpts.target_axes = ufbx_axes_right_handed_y_up;
        m_loadOpts.target_unit_meters = 1.0;
        m_loadOpts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
        m_loadOpts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY_NO_FALLBACK;
        m_loadOpts.generate_missing_normals = true;
        m_loadOpts.clean_skin_weights = true;
        m_loadOpts.use_blender_pbr_material = true;

        // Parse the file.
        ufbx_error error = {};
        m_scene = ufbx_load_file(narrowPath, &m_loadOpts, &error);

        if (!m_scene) {
            if (error.type == UFBX_ERROR_FILE_NOT_FOUND)
                return ModelLoadResult::FileNotFound;
            if (error.type == UFBX_ERROR_UNSUPPORTED_VERSION)
                return ModelLoadResult::UnsupportedFormat;
            return ModelLoadResult::ParseError;
        }

        // With MODIFY_GEOMETRY, ufbx converts everything to Y-up already.
        model.originalUpAxis = CoordinateAxis::PositiveY;

        // Convert to Model.
        loadMaterials(model);
        loadTextures(model);
        detectAlphaFromTextures(model);
        loadMeshes(model);
        loadNodes(model);
        loadSkins(model);
        loadAnimations(model);

        model.buildBoneHierarchy();
        model.calculateBounds();

        return ModelLoadResult::Ok;
    }

private:
    ufbx_scene* m_scene = nullptr;
    std::string m_basePath;
    ufbx_load_opts m_loadOpts = {};

    /// Node typed_id -> model bone index
    std::unordered_map<uint32_t, i32> m_nodeToBoneIndex;
    /// Material typed_id -> model material index
    std::unordered_map<uint32_t, i32> m_materialIndexMap;
    /// Texture typed_id -> model texture index
    std::unordered_map<uint32_t, i32> m_textureIndexMap;
    /// Mesh typed_id -> model mesh index
    std::unordered_map<uint32_t, i32> m_meshIndexMap;
    /// Skin deformer typed_id -> model skin index
    std::unordered_map<uint32_t, i32> m_skinIndexMap;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    static bool caseInsensitiveEquals(std::u8string_view a, std::u8string_view b) {
        if (a.size() != b.size()) return false;
        for (usize i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a.data()[i])) !=
                std::tolower(static_cast<unsigned char>(b.data()[i])))
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
        for (size_t i = 0; i < m_scene->materials.count; ++i) {
            ufbx_material* mat = m_scene->materials.data[i];
            auto* material = new ModelMaterial();

            if (mat->element.name.data && mat->element.name.length > 0)
                material->setName(utf8FromUfbx(mat->element.name));

            bool hasPBR = mat->features.pbr.enabled;

            if (hasPBR) {
                // PBR: base color.
                if (mat->pbr.base_color.has_value) {
                    auto c = mat->pbr.base_color.value_vec4;
                    material->baseColorFactor = math::Vector4(
                        static_cast<f32>(c.x), static_cast<f32>(c.y),
                        static_cast<f32>(c.z), static_cast<f32>(c.w));
                }
                if (mat->pbr.base_color.texture) {
                    material->baseColorTextureIndex = getOrCreateTextureIndex(
                        mat->pbr.base_color.texture, model);
                    // When using use_blender_pbr_material, the base_color value is often the
                    // legacy diffuse color (0.8,0.8,0.8) which shouldn't tint the texture.
                    material->baseColorFactor = math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                }

                // PBR: metallic + roughness.
                if (mat->pbr.metalness.has_value)
                    material->metallicFactor = static_cast<f32>(mat->pbr.metalness.value_real);
                if (mat->pbr.roughness.has_value)
                    material->roughnessFactor = static_cast<f32>(mat->pbr.roughness.value_real);

                if (mat->pbr.metalness.texture)
                    material->metallicRoughnessTextureIndex = getOrCreateTextureIndex(
                        mat->pbr.metalness.texture, model);
                else if (mat->pbr.roughness.texture)
                    material->metallicRoughnessTextureIndex = getOrCreateTextureIndex(
                        mat->pbr.roughness.texture, model);

                // PBR: normal map.
                if (mat->pbr.normal_map.texture) {
                    material->normalTextureIndex = getOrCreateTextureIndex(
                        mat->pbr.normal_map.texture, model);
                    material->normalScale = 1.0f;
                }

                // PBR: ambient occlusion.
                if (mat->pbr.ambient_occlusion.texture) {
                    material->occlusionTextureIndex = getOrCreateTextureIndex(
                        mat->pbr.ambient_occlusion.texture, model);
                    material->occlusionStrength = 1.0f;
                }

                // PBR: emissive.
                if (mat->pbr.emission_color.has_value) {
                    auto e = mat->pbr.emission_color.value_vec3;
                    f32 factor = mat->pbr.emission_factor.has_value
                        ? static_cast<f32>(mat->pbr.emission_factor.value_real) : 1.0f;
                    material->emissiveFactor = math::Vector3(
                        static_cast<f32>(e.x) * factor,
                        static_cast<f32>(e.y) * factor,
                        static_cast<f32>(e.z) * factor);
                }
                if (mat->pbr.emission_color.texture)
                    material->emissiveTextureIndex = getOrCreateTextureIndex(
                        mat->pbr.emission_color.texture, model);
            } else {
                // Legacy FBX (Lambert/Phong): map to PBR.
                if (mat->fbx.diffuse_color.has_value) {
                    auto c = mat->fbx.diffuse_color.value_vec4;
                    f32 factor = mat->fbx.diffuse_factor.has_value
                        ? static_cast<f32>(mat->fbx.diffuse_factor.value_real) : 1.0f;
                    material->baseColorFactor = math::Vector4(
                        static_cast<f32>(c.x) * factor,
                        static_cast<f32>(c.y) * factor,
                        static_cast<f32>(c.z) * factor, 1.0f);
                }
                if (mat->fbx.diffuse_color.texture) {
                    material->baseColorTextureIndex = getOrCreateTextureIndex(
                        mat->fbx.diffuse_color.texture, model);
                    material->baseColorFactor = math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                }

                // Normal map.
                if (mat->fbx.normal_map.texture) {
                    material->normalTextureIndex = getOrCreateTextureIndex(
                        mat->fbx.normal_map.texture, model);
                    material->normalScale = 1.0f;
                }

                // Emissive.
                if (mat->fbx.emission_color.has_value) {
                    auto e = mat->fbx.emission_color.value_vec3;
                    f32 factor = mat->fbx.emission_factor.has_value
                        ? static_cast<f32>(mat->fbx.emission_factor.value_real) : 1.0f;
                    material->emissiveFactor = math::Vector3(
                        static_cast<f32>(e.x) * factor,
                        static_cast<f32>(e.y) * factor,
                        static_cast<f32>(e.z) * factor);
                }
                if (mat->fbx.emission_color.texture)
                    material->emissiveTextureIndex = getOrCreateTextureIndex(
                        mat->fbx.emission_color.texture, model);

                // Non-PBR defaults.
                material->metallicFactor = 0.0f;
                material->roughnessFactor = 0.8f;
            }

            // Double-sided: FBX format doesn't reliably carry the DoubleSided flag
            // from Blender. When not explicitly set in the file, default to true
            // to match the behavior of most GLTF exports and avoid incorrect culling.
            if (mat->features.double_sided.is_explicit)
                material->doubleSided = mat->features.double_sided.enabled;
            else
                material->doubleSided = true;

            // Alpha / opacity (scalar values only -- texture-based alpha detection
            // is handled by detectAlphaFromTextures after textures are loaded).
            if (mat->features.opacity.enabled) {
                if (mat->pbr.opacity.has_value &&
                    static_cast<f32>(mat->pbr.opacity.value_real) < 1.0f)
                    material->alphaMode = AlphaMode::Blend;
                else if (mat->fbx.transparency_factor.has_value &&
                         static_cast<f32>(mat->fbx.transparency_factor.value_real) > 0.0f)
                    material->alphaMode = AlphaMode::Blend;
            }

            i32 matIndex = model.addMaterial(material);
            m_materialIndexMap.insert_or_assign(mat->element.typed_id, matIndex);
        }
    }

    /// Gets or creates a model texture index for a ufbx texture.
    /// This allows materials to reference textures before loadTextures runs.
    i32 getOrCreateTextureIndex(ufbx_texture* texture, Model& model) {
        if (!texture) return -1;

        uint32_t typedId = texture->element.typed_id;
        auto it = mapFind(m_textureIndexMap, typedId);
        if (it != nullptr)
            return *it;

        // Create a placeholder texture entry that loadTextures will populate.
        auto* modelTex = new ModelTexture();
        if (texture->element.name.data && texture->element.name.length > 0)
            modelTex->setName(utf8FromUfbx(texture->element.name));

        i32 index = model.addTexture(modelTex);
        m_textureIndexMap.insert_or_assign(typedId, index);
        return index;
    }

    // -----------------------------------------------------------------------
    // Textures
    // -----------------------------------------------------------------------

    void loadTextures(Model& model) {

        for (size_t i = 0; i < m_scene->textures.count; ++i) {
            ufbx_texture* tex = m_scene->textures.data[i];
            uint32_t typedId = tex->element.typed_id;

            // Get or create the model texture.
            ModelTexture* modelTex = nullptr;
            auto it = mapFind(m_textureIndexMap, typedId);
            if (it != nullptr) {
                modelTex = model.textures()[*it];
            } else {
                modelTex = new ModelTexture();
                if (tex->element.name.data && tex->element.name.length > 0)
                    modelTex->setName(utf8FromUfbx(tex->element.name));
                i32 idx = model.addTexture(modelTex);
                m_textureIndexMap.insert_or_assign(typedId, idx);
            }

            // Try to load image data.
            if (tex->content.size > 0 && tex->content.data) {
                // Embedded texture data.
                draco::image::Image img;
                if (image::io::loadImageFromMemory(
                        std::span<const u8>(static_cast<const u8*>(tex->content.data),
                                       tex->content.size),
                        img) == ErrorCode::Ok)
                    storeImageData(img, modelTex);
            } else if (tex->has_file) {
                // External file - try relative path, then walk parent directories, then absolute.
                std::string relPath;
                if (tex->relative_filename.data && tex->relative_filename.length > 0)
                    relPath = std::string(tex->relative_filename.data,
                                          tex->relative_filename.length);

                bool loaded = false;
                std::string resolvedPath;

                if (!relPath.empty()) {
                    // Try basePath + relative path first.
                    auto imagePath = (std::filesystem::path(m_basePath) / relPath).string();

                    if (!std::filesystem::exists(imagePath)) {
                        // Rel path may be wrong - try just the filename.
                        auto fileName = std::filesystem::path(relPath).filename().string();
                        imagePath = (std::filesystem::path(m_basePath) / fileName).string();
                    }

                    if (std::filesystem::exists(imagePath)) {
                        draco::image::Image img;
                        if (image::io::loadImage(utf8FromC(imagePath.c_str()), img)
                                == ErrorCode::Ok) {
                            storeImageData(img, modelTex);
                            resolvedPath = imagePath;
                            loaded = true;
                        }
                    } else {
                        // Walk up parent directories (up to 5 levels) looking for the file.
                        auto searchDir = std::filesystem::path(m_basePath);
                        for (int depth = 0; depth < 5 && !loaded; ++depth) {
                            auto parentDir = searchDir.parent_path();
                            if (parentDir.empty() || parentDir == searchDir)
                                break;

                            auto candidatePath = (parentDir / relPath).string();
                            if (std::filesystem::exists(candidatePath)) {
                                draco::image::Image img;
                                if (image::io::loadImage(utf8FromC(candidatePath.c_str()),
                                        img) == ErrorCode::Ok) {
                                    storeImageData(img, modelTex);
                                    resolvedPath = candidatePath;
                                    loaded = true;
                                }
                            }
                            searchDir = parentDir;
                        }
                    }
                }

                // Fallback: try absolute filename path.
                if (!loaded && tex->filename.data && tex->filename.length > 0) {
                    std::string absPath(tex->filename.data, tex->filename.length);
                    draco::image::Image img;
                    if (image::io::loadImage(utf8FromC(absPath.c_str()), img)
                            == ErrorCode::Ok) {
                        storeImageData(img, modelTex);
                        resolvedPath = absPath;
                        loaded = true;
                    }
                }

                if (loaded) {
                    // Set the URI so TextureConverter can build the source path for dedup.
                    modelTex->setUri(utf8FromC(resolvedPath.c_str()));
                }
            }

            // Sampler / wrap mode.
            TextureSampler sampler{};
            sampler.wrapS = convertWrapMode(tex->wrap_u);
            sampler.wrapT = convertWrapMode(tex->wrap_v);
            i32 samplerIdx = model.addSampler(sampler);
            modelTex->samplerIndex = samplerIdx;
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

    static TextureWrap convertWrapMode(ufbx_wrap_mode mode) {
        switch (mode) {
        case UFBX_WRAP_CLAMP: return TextureWrap::ClampToEdge;
        default:              return TextureWrap::Repeat;
        }
    }

    // -----------------------------------------------------------------------
    // Alpha Detection
    // -----------------------------------------------------------------------

    /// Post-process: detect alpha mode from base color texture content.
    /// FBX doesn't have an explicit alpha mode like glTF, so we analyze
    /// the albedo texture's alpha channel (like Godot does).
    static void detectAlphaFromTextures(Model& model) {
        for (auto* material : model.materials()) {
            // Skip materials that already have an explicit alpha mode.
            if (material->alphaMode != AlphaMode::Opaque)
                continue;

            if (material->baseColorTextureIndex < 0 ||
                static_cast<usize>(material->baseColorTextureIndex) >= model.textures().size())
                continue;

            auto* texture = model.textures()[material->baseColorTextureIndex];
            if (!texture->getData() || texture->getDataSize() == 0)
                continue;

            // Only check RGBA/BGRA textures (4 bytes per pixel with alpha).
            i32 bpp = getBytesPerPixel(texture->pixelFormat);
            if (bpp != 4)
                continue;

            i32 alphaOffset = getAlphaOffset(texture->pixelFormat);
            if (alphaOffset < 0)
                continue;

            const u8* data = texture->getData();
            i32 pixelCount = texture->width * texture->height;
            i32 dataSize = texture->getDataSize();

            if (pixelCount * bpp > dataSize)
                continue;

            // Scan alpha channel and classify pixels.
            i32 opaqueCount = 0;
            i32 transparentCount = 0;
            i32 gradientCount = 0;

            for (i32 p = 0; p < pixelCount; ++p) {
                u8 alpha = data[p * bpp + alphaOffset];
                if (alpha == 255)
                    opaqueCount++;
                else if (alpha == 0)
                    transparentCount++;
                else
                    gradientCount++;
            }
            (void)opaqueCount;

            if (transparentCount == 0 && gradientCount == 0)
                continue; // Fully opaque.

            // Determine alpha mode from pixel distribution.
            f32 gradientRatio = static_cast<f32>(gradientCount) / static_cast<f32>(pixelCount);
            if (gradientRatio > 0.25f) {
                material->alphaMode = AlphaMode::Blend;
            } else if (transparentCount > 0 || gradientCount > 0) {
                material->alphaMode = AlphaMode::Mask;
                material->alphaCutoff = 0.2f;
            }
        }
    }

    static i32 getBytesPerPixel(TexturePixelFormat fmt) {
        switch (fmt) {
        case TexturePixelFormat::RGBA8:
        case TexturePixelFormat::BGRA8: return 4;
        case TexturePixelFormat::RGB8:
        case TexturePixelFormat::BGR8:  return 3;
        case TexturePixelFormat::RG8:   return 2;
        case TexturePixelFormat::R8:    return 1;
        default: return 0;
        }
    }

    static i32 getAlphaOffset(TexturePixelFormat fmt) {
        switch (fmt) {
        case TexturePixelFormat::RGBA8: return 3;
        case TexturePixelFormat::BGRA8: return 3;
        default: return -1;
        }
    }

    // -----------------------------------------------------------------------
    // Meshes
    // -----------------------------------------------------------------------

    void loadMeshes(Model& model) {
        for (size_t i = 0; i < m_scene->meshes.count; ++i) {
            ufbx_mesh* fbxMesh = m_scene->meshes.data[i];
            auto* mesh = new ModelMesh();

            if (fbxMesh->element.name.data && fbxMesh->element.name.length > 0)
                mesh->setName(utf8FromUfbx(fbxMesh->element.name));

            // Determine if this mesh is skinned.
            bool isSkinned = fbxMesh->skin_deformers.count > 0;
            ufbx_skin_deformer* skinDeformer = isSkinned
                ? fbxMesh->skin_deformers.data[0] : nullptr;

            // Check for available attributes.
            bool hasNormals = fbxMesh->vertex_normal.exists || m_loadOpts.generate_missing_normals;
            bool hasUV = fbxMesh->uv_sets.count > 0 && fbxMesh->uv_sets.data[0].vertex_uv.exists;
            bool hasTangent = fbxMesh->uv_sets.count > 0 && fbxMesh->uv_sets.data[0].vertex_tangent.exists;
            bool hasColor = fbxMesh->color_sets.count > 0 && fbxMesh->color_sets.data[0].vertex_color.exists;
            // Also check legacy vertex_color field.
            bool hasLegacyColor = fbxMesh->vertex_color.exists;
            if (!hasColor && hasLegacyColor)
                hasColor = true;

            mesh->setHasNormals(hasNormals);
            mesh->setHasTangents(hasTangent);

            // Setup vertex format (matching GltfLoader layout).
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

            // Collect all triangulated vertices across all material parts.
            std::vector<u8> allVertexBytes;
            std::vector<u32> allIndices;
            std::unordered_map<size_t, i32> vertexMap; // vertex hash -> index

            // Allocate triangle index buffer for triangulation.
            size_t maxTriIndices = fbxMesh->max_face_triangles * 3;
            std::vector<uint32_t> triIndices(maxTriIndices);

            // Process material parts.
            if (fbxMesh->material_parts.count > 0) {
                for (size_t p = 0; p < fbxMesh->material_parts.count; ++p) {
                    ufbx_mesh_part* part = &fbxMesh->material_parts.data[p];
                    i32 indexStart = static_cast<i32>(allIndices.size());
                    i32 indexCount = 0;

                    // Get material index.
                    i32 materialIndex = -1;
                    if (part->index < static_cast<uint32_t>(fbxMesh->materials.count)) {
                        ufbx_material* matPtr = fbxMesh->materials.data[part->index];
                        if (matPtr) {
                            auto mit = mapFind(m_materialIndexMap, matPtr->element.typed_id);
                            if (mit != nullptr)
                                materialIndex = *mit;
                        }
                    }

                    // Process each face in this material part.
                    for (size_t fi = 0; fi < part->face_indices.count; ++fi) {
                        uint32_t faceIdx = part->face_indices.data[fi];
                        ufbx_face face = fbxMesh->faces.data[faceIdx];

                        uint32_t numTris = ufbx_triangulate_face(
                            triIndices.data(), maxTriIndices, fbxMesh, face);

                        for (uint32_t ti = 0; ti < numTris * 3; ++ti) {
                            uint32_t idx = triIndices[ti];

                            size_t hash = buildVertex(
                                fbxMesh, static_cast<int>(idx), skinDeformer, isSkinned,
                                hasUV, hasTangent, hasColor,
                                stride, positionOffset, normalOffset, texCoordOffset,
                                colorOffset, tangentOffset, jointsOffset, weightsOffset,
                                allVertexBytes, vertexMap);

                            allIndices.push_back(static_cast<u32>(*mapFind(vertexMap, hash)));
                            indexCount++;
                        }
                    }

                    if (indexCount > 0)
                        mesh->addPart(ModelMeshPart(indexStart, indexCount, materialIndex));
                }
            } else {
                // No material parts - process all faces as a single part.
                i32 indexCount = 0;

                for (size_t fi = 0; fi < fbxMesh->faces.count; ++fi) {
                    ufbx_face face = fbxMesh->faces.data[fi];
                    uint32_t numTris = ufbx_triangulate_face(
                        triIndices.data(), maxTriIndices, fbxMesh, face);

                    for (uint32_t ti = 0; ti < numTris * 3; ++ti) {
                        uint32_t idx = triIndices[ti];

                        size_t hash = buildVertex(
                            fbxMesh, static_cast<int>(idx), skinDeformer, isSkinned,
                            hasUV, hasTangent, hasColor,
                            stride, positionOffset, normalOffset, texCoordOffset,
                            colorOffset, tangentOffset, jointsOffset, weightsOffset,
                            allVertexBytes, vertexMap);

                        allIndices.push_back(static_cast<u32>(*mapFind(vertexMap, hash)));
                        indexCount++;
                    }
                }

                if (indexCount > 0)
                    mesh->addPart(ModelMeshPart(0, indexCount, -1));
            }

            // Copy vertex data to mesh.
            i32 vertexCount = static_cast<i32>(allVertexBytes.size() / stride);
            if (vertexCount > 0) {
                mesh->allocateVertices(vertexCount, stride);
                std::memcpy(mesh->getVertexData(), allVertexBytes.data(), allVertexBytes.size());
            }

            // Copy index data to mesh.
            i32 idxCount = static_cast<i32>(allIndices.size());
            if (idxCount > 0) {
                bool use32Bit = idxCount > 65535 || vertexCount > 65535;
                mesh->allocateIndices(idxCount, use32Bit);

                if (use32Bit) {
                    std::memcpy(mesh->getIndexData(), allIndices.data(),
                                idxCount * sizeof(u32));
                } else {
                    auto* dst = reinterpret_cast<u16*>(mesh->getIndexData());
                    for (i32 j = 0; j < idxCount; ++j)
                        dst[j] = static_cast<u16>(allIndices[j]);
                }
            }

            mesh->setTopology(PrimitiveTopology::Triangles);
            mesh->calculateBounds();

            i32 meshIdx = model.addMesh(mesh);
            m_meshIndexMap.insert_or_assign(fbxMesh->element.typed_id, meshIdx);
        }
    }

    /// Builds a vertex at the given face index and adds it to the vertex buffer.
    /// Returns the hash for deduplication.
    size_t buildVertex(
        ufbx_mesh* fbxMesh, int idx, ufbx_skin_deformer* skinDeformer, bool isSkinned,
        bool hasUV, bool hasTangent, bool hasColor,
        i32 stride, i32 positionOffset, i32 normalOffset, i32 texCoordOffset,
        i32 colorOffset, i32 tangentOffset, i32 jointsOffset, i32 weightsOffset,
        std::vector<u8>& vertexBytes, std::unordered_map<size_t, i32>& vtxMap)
    {
        // Read vertex attributes.
        ufbx_vec3 pos = readVec3(fbxMesh->vertex_position, idx);
        ufbx_vec3 normal = fbxMesh->vertex_normal.exists
            ? readVec3(fbxMesh->vertex_normal, idx)
            : ufbx_vec3{{{0, 1, 0}}};

        ufbx_vec2 uv = {};
        if (hasUV) {
            uv = readVec2(fbxMesh->uv_sets.data[0].vertex_uv, idx);
            // FBX uses bottom-left UV origin (V=0 at bottom), flip to match
            // GLTF/renderer convention (V=0 at top).
            uv.y = 1.0 - uv.y;
        }

        ufbx_vec3 tangent = {{{1, 0, 0}}};
        if (hasTangent)
            tangent = readVec3(fbxMesh->uv_sets.data[0].vertex_tangent, idx);

        ufbx_vec4 color = {{{1, 1, 1, 1}}};
        if (hasColor) {
            if (fbxMesh->color_sets.count > 0 && fbxMesh->color_sets.data[0].vertex_color.exists)
                color = readVec4(fbxMesh->color_sets.data[0].vertex_color, idx);
            else if (fbxMesh->vertex_color.exists)
                color = readVec4(fbxMesh->vertex_color, idx);
        }

        // Skinning data.
        u16 joints[4] = {};
        f32 weights[4] = {};
        if (isSkinned && skinDeformer) {
            uint32_t vertexIndex = fbxMesh->vertex_indices.data[idx];
            getSkinWeights(skinDeformer, static_cast<int>(vertexIndex), joints, weights);
        }

        // Compute hash for deduplication.
        size_t hash = hashVertex(pos, normal, uv, tangent, color, isSkinned, joints, weights);

        if (mapFind(vtxMap, hash) != nullptr)
            return hash;

        // Add new vertex.
        i32 newIndex = static_cast<i32>(vertexBytes.size() / stride);

        // Extend buffer.
        size_t oldCount = vertexBytes.size();
        vertexBytes.resize(oldCount + stride, 0);
        u8* vertex = &vertexBytes[oldCount];

        // Position.
        *reinterpret_cast<math::Vector3*>(vertex + positionOffset) =
            math::Vector3(static_cast<f32>(pos.x), static_cast<f32>(pos.y), static_cast<f32>(pos.z));

        // Normal.
        *reinterpret_cast<math::Vector3*>(vertex + normalOffset) =
            math::Vector3(static_cast<f32>(normal.x), static_cast<f32>(normal.y), static_cast<f32>(normal.z));

        // TexCoord.
        *reinterpret_cast<math::Vector2*>(vertex + texCoordOffset) =
            math::Vector2(static_cast<f32>(uv.x), static_cast<f32>(uv.y));

        // Color (pack to RGBA8).
        u8 cr = static_cast<u8>(std::clamp(static_cast<f32>(color.x) * 255.0f, 0.0f, 255.0f));
        u8 cg = static_cast<u8>(std::clamp(static_cast<f32>(color.y) * 255.0f, 0.0f, 255.0f));
        u8 cb = static_cast<u8>(std::clamp(static_cast<f32>(color.z) * 255.0f, 0.0f, 255.0f));
        u8 ca = static_cast<u8>(std::clamp(static_cast<f32>(color.w) * 255.0f, 0.0f, 255.0f));
        *reinterpret_cast<u32*>(vertex + colorOffset) =
            static_cast<u32>(cr) | (static_cast<u32>(cg) << 8) |
            (static_cast<u32>(cb) << 16) | (static_cast<u32>(ca) << 24);

        // Tangent.
        *reinterpret_cast<math::Vector3*>(vertex + tangentOffset) =
            math::Vector3(static_cast<f32>(tangent.x), static_cast<f32>(tangent.y), static_cast<f32>(tangent.z));

        // Skinning.
        if (isSkinned) {
            auto* jointsDst = reinterpret_cast<u16*>(vertex + jointsOffset);
            jointsDst[0] = joints[0]; jointsDst[1] = joints[1];
            jointsDst[2] = joints[2]; jointsDst[3] = joints[3];
            *reinterpret_cast<math::Vector4*>(vertex + weightsOffset) =
                math::Vector4(weights[0], weights[1], weights[2], weights[3]);
        }

        vtxMap.insert_or_assign(hash, newIndex);
        return hash;
    }

    static ufbx_vec3 readVec3(ufbx_vertex_vec3 attrib, int idx) {
        uint32_t valueIdx = attrib.indices.data[idx];
        return attrib.values.data[valueIdx];
    }

    static ufbx_vec2 readVec2(ufbx_vertex_vec2 attrib, int idx) {
        uint32_t valueIdx = attrib.indices.data[idx];
        return attrib.values.data[valueIdx];
    }

    static ufbx_vec4 readVec4(ufbx_vertex_vec4 attrib, int idx) {
        uint32_t valueIdx = attrib.indices.data[idx];
        return attrib.values.data[valueIdx];
    }

    static void getSkinWeights(ufbx_skin_deformer* skin, int vertexIndex,
                               u16 joints[4], f32 weights[4])
    {
        if (static_cast<size_t>(vertexIndex) >= skin->vertices.count)
            return;

        ufbx_skin_vertex* skinVertex = &skin->vertices.data[vertexIndex];
        int numWeights = static_cast<int>(skinVertex->num_weights);
        if (numWeights == 0)
            return;

        // Collect weights (top 16 max).
        int entryCount = std::min(numWeights, 16);
        u16 entryJoints[16] = {};
        f32 entryWeights[16] = {};

        for (int w = 0; w < entryCount; ++w) {
            ufbx_skin_weight* skinWeight =
                &skin->weights.data[skinVertex->weight_begin + static_cast<uint32_t>(w)];
            uint32_t clusterIdx = skinWeight->cluster_index;

            // Use cluster index directly as skeleton joint index.
            entryJoints[w] = static_cast<u16>(clusterIdx);
            entryWeights[w] = static_cast<f32>(skinWeight->weight);
        }

        // Simple selection of top 4 weights.
        int top = std::min(entryCount, 4);
        for (int a = 0; a < top; ++a) {
            // Find max remaining.
            int maxIdx = a;
            for (int b = a + 1; b < entryCount; ++b) {
                if (entryWeights[b] > entryWeights[maxIdx])
                    maxIdx = b;
            }
            if (maxIdx != a) {
                std::swap(entryJoints[a], entryJoints[maxIdx]);
                std::swap(entryWeights[a], entryWeights[maxIdx]);
            }

            joints[a] = entryJoints[a];
            weights[a] = entryWeights[a];
        }

        // Normalize weights.
        f32 sum = weights[0] + weights[1] + weights[2] + weights[3];
        if (sum > 0) {
            f32 invSum = 1.0f / sum;
            weights[0] *= invSum;
            weights[1] *= invSum;
            weights[2] *= invSum;
            weights[3] *= invSum;
        }
    }

    static size_t hashVertex(ufbx_vec3 pos, ufbx_vec3 normal, ufbx_vec2 uv,
                             ufbx_vec3 tangent, ufbx_vec4 color,
                             bool isSkinned, const u16 joints[4], const f32 weights[4])
    {
        size_t hash = 17;
        auto hf = [](f32 v) { return std::hash<f32>{}(v); };
        hash = hash * 31 + hf(static_cast<f32>(pos.x));
        hash = hash * 31 + hf(static_cast<f32>(pos.y));
        hash = hash * 31 + hf(static_cast<f32>(pos.z));
        hash = hash * 31 + hf(static_cast<f32>(normal.x));
        hash = hash * 31 + hf(static_cast<f32>(normal.y));
        hash = hash * 31 + hf(static_cast<f32>(normal.z));
        hash = hash * 31 + hf(static_cast<f32>(uv.x));
        hash = hash * 31 + hf(static_cast<f32>(uv.y));
        hash = hash * 31 + hf(static_cast<f32>(tangent.x));
        hash = hash * 31 + hf(static_cast<f32>(tangent.y));
        hash = hash * 31 + hf(static_cast<f32>(tangent.z));
        hash = hash * 31 + hf(static_cast<f32>(color.x));
        hash = hash * 31 + hf(static_cast<f32>(color.y));
        hash = hash * 31 + hf(static_cast<f32>(color.z));
        hash = hash * 31 + hf(static_cast<f32>(color.w));

        if (isSkinned) {
            hash = hash * 31 + static_cast<size_t>(joints[0]);
            hash = hash * 31 + static_cast<size_t>(joints[1]);
            hash = hash * 31 + static_cast<size_t>(joints[2]);
            hash = hash * 31 + static_cast<size_t>(joints[3]);
            hash = hash * 31 + hf(weights[0]);
            hash = hash * 31 + hf(weights[1]);
            hash = hash * 31 + hf(weights[2]);
            hash = hash * 31 + hf(weights[3]);
        }

        return hash;
    }

    // -----------------------------------------------------------------------
    // Nodes
    // -----------------------------------------------------------------------

    void loadNodes(Model& model) {
        // Pass 1: create all bones.
        for (size_t i = 0; i < m_scene->nodes.count; ++i) {
            ufbx_node* node = m_scene->nodes.data[i];
            auto* bone = new ModelBone();

            if (node->element.name.data && node->element.name.length > 0)
                bone->setName(utf8FromUfbx(node->element.name));

            // Extract TRS from local transform.
            ufbx_transform t = node->local_transform;
            bone->translation = math::Vector3(
                static_cast<f32>(t.translation.x),
                static_cast<f32>(t.translation.y),
                static_cast<f32>(t.translation.z));
            bone->rotation = math::Quaternion(
                static_cast<f32>(t.rotation.x),
                static_cast<f32>(t.rotation.y),
                static_cast<f32>(t.rotation.z),
                static_cast<f32>(t.rotation.w));
            bone->scale = math::Vector3(
                static_cast<f32>(t.scale.x),
                static_cast<f32>(t.scale.y),
                static_cast<f32>(t.scale.z));
            bone->updateLocalTransform();

            // Mesh reference.
            if (node->mesh) {
                auto mit = mapFind(m_meshIndexMap, node->mesh->element.typed_id);
                if (mit != nullptr)
                    bone->meshIndex = *mit;
            }

            // Skin reference.
            if (node->mesh && node->mesh->skin_deformers.count > 0) {
                ufbx_skin_deformer* skinDef = node->mesh->skin_deformers.data[0];
                auto sit = mapFind(m_skinIndexMap, skinDef->element.typed_id);
                if (sit != nullptr)
                    bone->skinIndex = *sit;
                // Note: skin index may be set later when loadSkins runs.
            }

            i32 boneIdx = model.addBone(bone);
            m_nodeToBoneIndex.insert_or_assign(node->element.typed_id, boneIdx);
        }

        // Pass 2: set parent relationships.
        for (size_t i = 0; i < m_scene->nodes.count; ++i) {
            ufbx_node* node = m_scene->nodes.data[i];
            if (node->parent) {
                auto pit = mapFind(m_nodeToBoneIndex, node->parent->element.typed_id);
                if (pit != nullptr)
                    model.bones()[i]->parentIndex = *pit;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Skins
    // -----------------------------------------------------------------------

    void loadSkins(Model& model) {
        for (size_t i = 0; i < m_scene->skin_deformers.count; ++i) {
            ufbx_skin_deformer* skinDef = m_scene->skin_deformers.data[i];
            auto* skin = new ModelSkin();

            if (skinDef->element.name.data && skinDef->element.name.length > 0)
                skin->setName(utf8FromUfbx(skinDef->element.name));

            // Process clusters (joints).
            for (size_t j = 0; j < skinDef->clusters.count; ++j) {
                ufbx_skin_cluster* cluster = skinDef->clusters.data[j];
                if (!cluster || !cluster->bone_node)
                    continue;

                i32 jointIndex = -1;
                auto nit = mapFind(m_nodeToBoneIndex, cluster->bone_node->element.typed_id);
                if (nit != nullptr)
                    jointIndex = *nit;

                if (jointIndex < 0)
                    continue;

                // Convert geometry_to_bone matrix to our inverse bind matrix.
                math::Matrix4 ibm = convertMatrix(cluster->geometry_to_bone);

                skin->addJoint(jointIndex, ibm);

                // Also set on the bone.
                if (jointIndex >= 0 && static_cast<usize>(jointIndex) < model.bones().size())
                    model.bones()[jointIndex]->inverseBindMatrix = ibm;
            }

            i32 skinIdx = model.addSkin(skin);
            m_skinIndexMap.insert_or_assign(skinDef->element.typed_id, skinIdx);
        }

        // Update bone skin indices now that skins are loaded.
        for (size_t i = 0; i < m_scene->nodes.count; ++i) {
            ufbx_node* node = m_scene->nodes.data[i];
            if (node->mesh && node->mesh->skin_deformers.count > 0) {
                ufbx_skin_deformer* skinDef = node->mesh->skin_deformers.data[0];
                auto sit = mapFind(m_skinIndexMap, skinDef->element.typed_id);
                if (sit != nullptr) {
                    if (i < model.bones().size())
                        model.bones()[i]->skinIndex = *sit;
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Animations
    // -----------------------------------------------------------------------

    void loadAnimations(Model& model) {
        for (size_t i = 0; i < m_scene->anim_stacks.count; ++i) {
            ufbx_anim_stack* stack = m_scene->anim_stacks.data[i];
            auto* animation = new ModelAnimation();

            if (stack->element.name.data && stack->element.name.length > 0)
                animation->setName(utf8FromUfbx(stack->element.name));

            // Bake the animation - this pre-computes T/R/S keyframes per node
            // in the target coordinate system (Y-up), handling Euler-to-quaternion
            // conversion and layer blending automatically.
            ufbx_bake_opts bakeOpts = {};
            bakeOpts.resample_rate = 30.0;
            bakeOpts.minimum_sample_rate = 30.0;
            bakeOpts.max_keyframe_segments = 1024;

            ufbx_error bakeError = {};
            ufbx_baked_anim* bakedAnim = ufbx_bake_anim(
                m_scene, stack->anim, &bakeOpts, &bakeError);
            if (!bakedAnim) {
                delete animation;
                continue;
            }

            for (size_t ni = 0; ni < bakedAnim->nodes.count; ++ni) {
                ufbx_baked_node* bakedNode = &bakedAnim->nodes.data[ni];

                // Map baked node to our bone index.
                auto bit = mapFind(m_nodeToBoneIndex, bakedNode->typed_id);
                if (bit == nullptr)
                    continue;
                i32 boneIdx = *bit;

                // Translation channel.
                if (bakedNode->translation_keys.count > 0 && !bakedNode->constant_translation) {
                    auto* channel = new AnimationChannel();
                    channel->targetBone = boneIdx;
                    channel->path = AnimationPath::Translation;
                    channel->interpolation = detectInterpolationVec3(bakedNode->translation_keys);

                    for (size_t ki = 0; ki < bakedNode->translation_keys.count; ++ki) {
                        ufbx_baked_vec3* key = &bakedNode->translation_keys.data[ki];
                        channel->addKeyframe(static_cast<f32>(key->time),
                            math::Vector4(static_cast<f32>(key->value.x),
                                 static_cast<f32>(key->value.y),
                                 static_cast<f32>(key->value.z), 0));
                    }
                    animation->addChannel(channel);
                }

                // Rotation channel.
                if (bakedNode->rotation_keys.count > 0 && !bakedNode->constant_rotation) {
                    auto* channel = new AnimationChannel();
                    channel->targetBone = boneIdx;
                    channel->path = AnimationPath::Rotation;
                    channel->interpolation = detectInterpolationQuat(bakedNode->rotation_keys);

                    for (size_t ki = 0; ki < bakedNode->rotation_keys.count; ++ki) {
                        ufbx_baked_quat* key = &bakedNode->rotation_keys.data[ki];
                        channel->addKeyframe(static_cast<f32>(key->time),
                            math::Vector4(static_cast<f32>(key->value.x),
                                 static_cast<f32>(key->value.y),
                                 static_cast<f32>(key->value.z),
                                 static_cast<f32>(key->value.w)));
                    }
                    animation->addChannel(channel);
                }

                // Scale channel.
                if (bakedNode->scale_keys.count > 0 && !bakedNode->constant_scale) {
                    auto* channel = new AnimationChannel();
                    channel->targetBone = boneIdx;
                    channel->path = AnimationPath::Scale;
                    channel->interpolation = detectInterpolationVec3(bakedNode->scale_keys);

                    for (size_t ki = 0; ki < bakedNode->scale_keys.count; ++ki) {
                        ufbx_baked_vec3* key = &bakedNode->scale_keys.data[ki];
                        channel->addKeyframe(static_cast<f32>(key->time),
                            math::Vector4(static_cast<f32>(key->value.x),
                                 static_cast<f32>(key->value.y),
                                 static_cast<f32>(key->value.z), 0));
                    }
                    animation->addChannel(channel);
                }
            }

            ufbx_free_baked_anim(bakedAnim);

            animation->calculateDuration();
            model.addAnimation(animation);
        }
    }

    /// Detect interpolation mode from baked vec3 keyframe flags.
    static AnimationInterpolation detectInterpolationVec3(ufbx_baked_vec3_list keys) {
        for (size_t i = 0; i < keys.count; ++i) {
            uint32_t flags = keys.data[i].flags;
            if ((flags & UFBX_BAKED_KEY_STEP_LEFT) || (flags & UFBX_BAKED_KEY_STEP_RIGHT))
                return AnimationInterpolation::Step;
        }
        return AnimationInterpolation::Linear;
    }

    /// Detect interpolation mode from baked quaternion keyframe flags.
    static AnimationInterpolation detectInterpolationQuat(ufbx_baked_quat_list keys) {
        for (size_t i = 0; i < keys.count; ++i) {
            uint32_t flags = keys.data[i].flags;
            if ((flags & UFBX_BAKED_KEY_STEP_LEFT) || (flags & UFBX_BAKED_KEY_STEP_RIGHT))
                return AnimationInterpolation::Step;
        }
        return AnimationInterpolation::Linear;
    }

    // -----------------------------------------------------------------------
    // Matrix Conversion
    // -----------------------------------------------------------------------

    /// Converts a ufbx 3x4 matrix to our 4x4 math::Matrix4.
    /// ufbx column -> math::Matrix4 row (same pattern as GltfLoader's column-major transpose).
    static math::Matrix4 convertMatrix(ufbx_matrix m) {
        return math::Matrix4{{
            { static_cast<f32>(m.m00), static_cast<f32>(m.m10), static_cast<f32>(m.m20), 0 },
            { static_cast<f32>(m.m01), static_cast<f32>(m.m11), static_cast<f32>(m.m21), 0 },
            { static_cast<f32>(m.m02), static_cast<f32>(m.m12), static_cast<f32>(m.m22), 0 },
            { static_cast<f32>(m.m03), static_cast<f32>(m.m13), static_cast<f32>(m.m23), 1 }
        }};
    }
};

} // namespace draco::model::fbx
