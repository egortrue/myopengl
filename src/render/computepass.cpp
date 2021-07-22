#include "computepass.h"

#include <array>
#include <stdexcept>

#define GLM_FORCE_SILENT_WARNINGS
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <utility>

namespace nevk
{
ComputePass::ComputePass(/* args */)
{
}

ComputePass::~ComputePass()
{
}

void ComputePass::createComputePipeline(VkShaderModule& shaderModule)
{
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;

    if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = mPipelineLayout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateComputePipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create compute pipeline!");
    }
}

VkShaderModule ComputePass::createShaderModule(const char* code, const uint32_t codeSize)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = (uint32_t*)code;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void ComputePass::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding texLayoutBinding{};
    texLayoutBinding.binding = 1;
    texLayoutBinding.descriptorCount = 1;
    texLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    texLayoutBinding.pImmutableSamplers = nullptr;
    texLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.push_back(uboLayoutBinding);
    for (int i = 0; i < 6; ++i)
    {
        texLayoutBinding.binding = 1 + i;
        bindings.push_back(texLayoutBinding);
    }

    VkDescriptorSetLayoutBinding texBindlessLayoutBinding{};
    texBindlessLayoutBinding.binding = 7;
    texBindlessLayoutBinding.descriptorCount = (uint32_t)2048;
    texBindlessLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    texBindlessLayoutBinding.pImmutableSamplers = nullptr;
    texBindlessLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(texBindlessLayoutBinding);

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 8;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(samplerLayoutBinding);

    VkDescriptorSetLayoutBinding materialLayoutBinding{};
    materialLayoutBinding.binding = 9;
    materialLayoutBinding.descriptorCount = 1;
    materialLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialLayoutBinding.pImmutableSamplers = nullptr;
    materialLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(materialLayoutBinding);

    VkDescriptorSetLayoutBinding instanceLayoutBinding{};
    instanceLayoutBinding.binding = 10;
    instanceLayoutBinding.descriptorCount = 1;
    instanceLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    instanceLayoutBinding.pImmutableSamplers = nullptr;
    instanceLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(instanceLayoutBinding);

    VkDescriptorSetLayoutBinding outLayoutBinding{};
    outLayoutBinding.binding = 11;
    outLayoutBinding.descriptorCount = 1;
    outLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outLayoutBinding.pImmutableSamplers = nullptr;
    outLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(outLayoutBinding);

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void ComputePass::createDescriptorSets(VkDescriptorPool& descriptorPool)
{
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, mDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(mDevice, &allocInfo, mDescriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
}

void ComputePass::updateDescriptorSet(uint32_t descIndex)
{
    std::array<VkWriteDescriptorSet, 12> descriptorWrites{};

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = mResMngr->getVkBuffer(uniformBuffers[descIndex]);
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    VkDescriptorImageInfo imageInfoDepth{};
    imageInfoDepth.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfoDepth.imageView = mGbuffer->depthView;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfoDepth;

    VkDescriptorImageInfo imageInfoWPos{};
    imageInfoWPos.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfoWPos.imageView = mGbuffer->wPosView;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pImageInfo = &imageInfoWPos;

    VkDescriptorImageInfo imageInfoNormal{};
    imageInfoNormal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfoNormal.imageView = mGbuffer->normalView;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pImageInfo = &imageInfoNormal;

    VkDescriptorImageInfo imageInfoTangent{};
    imageInfoTangent.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfoTangent.imageView = mGbuffer->tangentView;

    descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[4].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[4].dstBinding = 4;
    descriptorWrites[4].dstArrayElement = 0;
    descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[4].descriptorCount = 1;
    descriptorWrites[4].pImageInfo = &imageInfoTangent;

    VkDescriptorImageInfo imageInfoUv{};
    imageInfoUv.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfoUv.imageView = mGbuffer->uvView;

    descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[5].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[5].dstBinding = 5;
    descriptorWrites[5].dstArrayElement = 0;
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pImageInfo = &imageInfoUv;

    VkDescriptorImageInfo imageInfoInstId{};
    imageInfoInstId.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfoInstId.imageView = mGbuffer->instIdView;

    descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[6].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[6].dstBinding = 6;
    descriptorWrites[6].dstArrayElement = 0;
    descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[6].descriptorCount = 1;
    descriptorWrites[6].pImageInfo = &imageInfoInstId;

    std::vector<VkDescriptorImageInfo> imageInfoBindless(2048);
    std::fill(imageInfoBindless.begin(), imageInfoBindless.end(), VkDescriptorImageInfo());

    for (uint32_t j = 0; j < mTextureImageView.size(); ++j)
    {
        imageInfoBindless[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoBindless[j].imageView = mTextureImageView[j];
    }

    descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[7].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[7].dstBinding = 7;
    descriptorWrites[7].dstArrayElement = 0;
    descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[7].descriptorCount = (uint32_t)2048;
    descriptorWrites[7].pImageInfo = imageInfoBindless.data();

    VkDescriptorImageInfo samplerInfo{};
    samplerInfo.sampler = mTextureSampler;

    descriptorWrites[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[8].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[8].dstBinding = 8;
    descriptorWrites[8].dstArrayElement = 0;
    descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorWrites[8].descriptorCount = 1;
    descriptorWrites[8].pImageInfo = &samplerInfo;

    VkDescriptorBufferInfo materialInfo{};
    materialInfo.buffer = mMaterialBuffer;
    materialInfo.offset = 0;
    materialInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[9].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[9].dstBinding = 9;
    descriptorWrites[9].dstArrayElement = 0;
    descriptorWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[9].descriptorCount = 1;
    descriptorWrites[9].pBufferInfo = &materialInfo;

    VkDescriptorBufferInfo instanceInfo{};
    instanceInfo.buffer = mInstanceBuffer;
    instanceInfo.offset = 0;
    instanceInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[10].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[10].dstBinding = 10;
    descriptorWrites[10].dstArrayElement = 0;
    descriptorWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[10].descriptorCount = 1;
    descriptorWrites[10].pBufferInfo = &instanceInfo;

    VkDescriptorImageInfo outputImageInfo{};
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputImageInfo.imageView = mOutImageView;

    descriptorWrites[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[11].dstSet = mDescriptorSets[descIndex];
    descriptorWrites[11].dstBinding = 11;
    descriptorWrites[11].dstArrayElement = 0;
    descriptorWrites[11].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[11].descriptorCount = 1;
    descriptorWrites[11].pImageInfo = &outputImageInfo;

    vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void ComputePass::updateDescriptorSets()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        updateDescriptorSet(i);
    }
}

void ComputePass::record(VkCommandBuffer& cmd, uint32_t width, uint32_t height, uint32_t imageIndex)
{
    if (needDesciptorSetUpdate && imageViewCounter < 3)
    {
        imageViewCounter++;
        updateDescriptorSet(imageIndex);
    }
    else
    {
        imageViewCounter = 0;
        needDesciptorSetUpdate = false;
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mPipelineLayout, 0, 1, &mDescriptorSets[imageIndex % MAX_FRAMES_IN_FLIGHT], 0, nullptr);
    const uint32_t dispX = (width + 15) / 16;
    const uint32_t dispY = (height + 15) / 16;
    vkCmdDispatch(cmd, dispX, dispY, 1);
}

void ComputePass::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        uniformBuffers[i] = mResMngr->createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void ComputePass::updateUniformBuffer(uint32_t currentImage, const glm::float4x4& lightSpaceMatrix, Scene& scene, uint32_t cameraIndex, const uint32_t width, const uint32_t height)
{
    UniformBufferObject ubo{};
    ubo.dimension.x = width;
    ubo.dimension.y = height;
    Camera& camera = scene.getCamera(cameraIndex);
    glm::float4x4 proj = camera.getPerspective();
    glm::float4x4 view = camera.getView();

    ubo.viewToProj = proj;
    ubo.CameraPos = camera.getPosition();
    ubo.worldToView = view;
    ubo.lightPosition = scene.mLightPosition;
    ubo.lightSpaceMatrix = lightSpaceMatrix;
    ubo.debugView = (uint32_t)scene.mDebugViewSettings;

    void* data = mResMngr->getMappedMemory(uniformBuffers[currentImage]);
    memcpy(data, &ubo, sizeof(ubo));
}

void ComputePass::onDestroy()
{
    for (size_t i = 0; i < uniformBuffers.size(); ++i)
    {
        mResMngr->destroyBuffer(uniformBuffers[i]);
    }
    vkDestroyPipeline(mDevice, mPipeline, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    vkDestroyShaderModule(mDevice, mCS, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
}

void ComputePass::setMaterialBuffer(VkBuffer materialBuffer)
{
    mMaterialBuffer = materialBuffer;
    imageViewCounter = 0;
    needDesciptorSetUpdate = true;
}

void ComputePass::setInstanceBuffer(VkBuffer instanceBuffer)
{
    mInstanceBuffer = instanceBuffer;
    imageViewCounter = 0;
    needDesciptorSetUpdate = true;
}

void ComputePass::setGbuffer(GBuffer* gbuffer)
{
    mGbuffer = gbuffer;
    imageViewCounter = 0;
    needDesciptorSetUpdate = true;
}

void ComputePass::setOutputImageView(VkImageView imageView)
{
    mOutImageView = imageView;
    imageViewCounter = 0;
    needDesciptorSetUpdate = true;
}

void ComputePass::setTextureSampler(VkSampler textureSampler)
{
    mTextureSampler = textureSampler;
    imageViewCounter = 0;
    needDesciptorSetUpdate = true;
}

void ComputePass::setTextureImageViews(const std::vector<VkImageView>& texImages)
{
    mTextureImageView = texImages;
    imageViewCounter = 0;
    needDesciptorSetUpdate = true;
}

void ComputePass::init(VkDevice& device, const char* csCode, uint32_t csCodeSize, VkDescriptorPool descpool, ResourceManager* resMngr)
{
    mDevice = device;
    mResMngr = resMngr;
    mDescriptorPool = descpool;
    mCS = createShaderModule(csCode, csCodeSize);
    createUniformBuffers();
    createDescriptorSetLayout();
    createDescriptorSets(mDescriptorPool);
    updateDescriptorSets();
    createComputePipeline(mCS);
}

} // namespace nevk
