#include "scene.h"

#include <utility>

namespace nevk
{

uint32_t Scene::createMesh(const std::vector<Vertex>& vb, const std::vector<uint32_t>& ib)
{
    Mesh* mesh = nullptr;
    uint32_t meshId = -1;
    if (mDelMesh.empty())
    {
        meshId = mMeshes.size(); // add mesh to storage
        mMeshes.push_back({});
        mesh = &mMeshes.back();
    }
    else
    {
        meshId = mDelMesh.top(); // get index from stack
        mDelMesh.pop(); // del taken index from stack
        mesh = &mMeshes[meshId];
    }

    mesh->mIndex = mIndices.size(); // Index of 1st index in index buffer
    mesh->mCount = ib.size(); // amount of indices in mesh

    const uint32_t ibOffset = mIndices.size(); // adjust indices for global index buffer
    for (int i = 0; i < ib.size(); ++i)
    {
        mIndices.push_back(ibOffset + ib[i]);
    }
    mVertices.insert(mVertices.end(), vb.begin(), vb.end()); // copy vertices
    return meshId;
}

uint32_t Scene::createInstance(const uint32_t meshId, const uint32_t materialId, const glm::mat4& transform)
{
    Instance* inst = nullptr;
    uint32_t instId = -1;
    if (mDelInstances.empty())
    {
        instId = mInstances.size(); // add instance to storage
        mInstances.push_back({});
        inst = &mInstances.back();
    }
    else
    {
        instId = mDelInstances.top(); // get index from stack
        mDelInstances.pop(); // del taken index from stack
        inst = &mInstances[instId];
    }
    inst->mMaterialId = materialId;
    inst->mMeshId = meshId;
    inst->transform = transform;

    return instId;
}


uint32_t Scene::createMaterial(const glm::float4& ambient,
                               const glm::float4& diffuse,
                               const glm::float4& specular,
                               glm::float4 emissive,
                               float opticalDensity,
                               float shininess,
                               glm::float4 transparency,
                               uint32_t illum,
                               uint32_t texAmbientId,
                               uint32_t texDiffuseId,
                               uint32_t texSpeculaId,
                               uint32_t texNormalId)
{
    Material* material = nullptr;
    uint32_t materialId = -1;
    if (mDelMaterial.empty())
    {
        materialId = mMaterials.size(); // add material to storage
        mMaterials.push_back({});
        material = &mMaterials.back();
    }
    else
    {
        materialId = mDelMaterial.top(); // get index from stack
        mDelMaterial.pop(); // del taken index from stack
        material = &mMaterials[materialId];
    }
    material->ambient = ambient;
    material->diffuse = diffuse;
    material->specular = specular;
    material->emissive = emissive;
    material->opticalDensity = opticalDensity;
    material->shininess = shininess;
    material->transparency = transparency;
    material->illum = illum;
    material->texAmbientId = texAmbientId;
    material->texDiffuseId = texDiffuseId;
    material->texSpeculaId = texSpeculaId;
    material->texNormalId = texNormalId;
    return materialId;
}

void Scene::removeInstance(const uint32_t instId)
{
    mDelInstances.push(instId); // marked as removed
}

void Scene::removeMesh(const uint32_t meshId)
{
    mDelMesh.push(meshId); // marked as removed
}

void Scene::removeMaterial(const uint32_t materialId)
{
    mDelMaterial.push(materialId); // marked as removed
}

std::set<uint32_t> Scene::getDirtyInstances()
{
    return this->mDirtyInstances;
}
bool Scene::getFrMod()
{
    return this->FrMod;
}

void Scene::updateInstanceTransform(uint32_t instId, glm::float4x4 newTransform)
{
    Instance& inst = mInstances[instId];
    inst.transform = newTransform;
    mDirtyInstances.insert(instId);
}

void Scene::beginFrame()
{
    FrMod = true;
    mDirtyInstances.clear();
}

void Scene::endFrame()
{
    FrMod = false;
}
} // namespace nevk
