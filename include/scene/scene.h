#pragma once

#include <vector>
#include <stack>
#include <set>
#include <cstdint>
#include "glm-wrapper.hpp"
#include "camera.h"

namespace nevk
{

struct Mesh
{
    uint32_t mIndex; // Index of 1st index in index buffer
    uint32_t mCount; // amount of indices in mesh
};

struct Material
{
    glm::float4 color;
};

struct Instance
{
    glm::mat4 transform;
    uint32_t mMeshId;
    uint32_t mMaterialId;
};

class Scene
{
private:
    Camera mCamera;
    std::stack<uint32_t> mDelInstances;
    std::stack<uint32_t> mDelMesh;
    std::stack<uint32_t> mDelMaterial;

public:
    struct Vertex
    {
        glm::float3 pos;
        glm::float3 normal;
        glm::float2 uv; // hold the texture coordinate
        glm::float3 color;
    };

    bool fr_mod{};

    std::set<uint32_t> mDirtyInstances;

    std::vector<Vertex> mVertices;
    std::vector<uint32_t> mIndices;

    std::vector<Mesh> mMeshes;
    std::vector<Material> mMaterials;
    std::vector<Instance> mInstances;

    Scene() = default;

    ~Scene() = default;

    std::vector<Vertex>& getVertices()
    {
        return mVertices;
    }
    std::vector<uint32_t>& getIndices()
    {
        return mIndices;
    }
    Camera& getCamera()
    {
        return mCamera;
    }
    /// <summary>
    /// Transform matrix
    /// </summary>
    /// <returns>Nothing</returns>
    static glm::mat4 createMeshTransform();
    /// <summary>
    /// Create Mesh geometry
    /// </summary>
    /// <param name="vb">Vertices</param>
    /// <param name="ib">Indices</param>
    /// <returns>Mesh id in scene</returns>
    uint32_t createMesh(const std::vector<Vertex>& vb, const std::vector<uint32_t>& ib);
    /// <summary>
    /// Creates Instance
    /// </summary>
    /// <param name="meshId">valid mesh id</param>
    /// <param name="materialId">valid material id</param>
    /// <param name="transform">transform</param>
    /// <returns>Instance id in scene</returns>
    uint32_t createInstance(uint32_t meshId, uint32_t materialId, const glm::mat4& transform);
    /// <summary>
    /// Creates Material
    /// </summary>
    /// <param name="color">Color</param>
    /// <returns>Nothing</returns>
    uint32_t createMaterial(const glm::float4& color);
    /// <summary>
    /// Removes instance/mesh/material
    /// </summary>
    /// <param name="meshId">valid mesh id</param>
    /// <param name="materialId">valid material id</param>
    /// <param name="instId">valid instance id</param>
    /// <returns>Nothing</returns>
    void removeInstance(uint32_t instId);
    void removeMesh(uint32_t meshId);
    void removeMaterial(uint32_t materialId);
    /// <summary>
    /// Updates Instance matrix(transform)
    /// </summary>
    /// <param name="instId">valid instance id</param>
    /// <param name="newTransform">new transformation matrix</param>
    /// <returns>Nothing</returns>
    void updateInstanceTransform(uint32_t instId, glm::mat4 newTransform);
    /// <summary>
    /// Changes status of scene and cleans up mDirty* sets
    /// </summary>
    /// <returns>Nothing</returns>
    void beginFrame();
    /// <summary>
    /// Changes status of scene
    /// </summary>
    /// <returns>Nothing</returns>
    void endFrame();
};
} // namespace nevk