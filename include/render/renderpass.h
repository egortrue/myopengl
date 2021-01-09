#pragma once
#include <vulkan/vulkan.h>
#include "vertex.h"
#include <vector>
#include <array>
#include <resourcemanager.h>

namespace nevk
{
class RenderPass
{
private:
    struct UniformBufferObject
    {
        alignas(16) glm::mat4 modelViewProj;
    };


    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
    VkPipeline mPipeline;
    VkPipelineLayout mPipelineLayout;
    VkRenderPass mRenderPass;
    VkDescriptorSetLayout mDescriptorSetLayout;
    VkDevice mDevice;

    VkShaderModule mVS, mPS;

    ResourceManager* mResMngr;
    VkDescriptorPool mDescriptorPool;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    VkImageView mTextureImageView;
    VkSampler mTextureSampler;

    void createRenderPass();

    void createDescriptorSetLayout();
    void createDescriptorSets(VkDescriptorPool& descriptorPool);

    void createUniformBuffers();

    std::vector<VkDescriptorSet> mDescriptorSets;

    std::vector<VkFramebuffer> mFrameBuffers;

    VkFormat mFrameBufferFormat;
    VkFormat mDepthBufferFormat;
    uint32_t mWidth, mHeight;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

public:
    void createGraphicsPipeline(VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule, uint32_t width, uint32_t height);

    void createFrameBuffers(std::vector<VkImageView>& imageViews, VkImageView& depthImageView, uint32_t width, uint32_t height);

    void setFrameBufferFormat(VkFormat format)
    {
        mFrameBufferFormat = format;
    }

    void setDepthBufferFormat(VkFormat format)
    {
        mDepthBufferFormat = format;
    }

    void setTextureImageView(VkImageView textureImageView);
    void setTextureSampler(VkSampler textureSampler);

    void init(VkDevice& device, VkShaderModule& vertShaderModule, VkShaderModule& fragShaderModule, VkDescriptorPool descpool, ResourceManager* resMngr, uint32_t width, uint32_t height)
    {
        mDevice = device;
        mResMngr = resMngr;
        mDescriptorPool = descpool;
        mWidth = width;
        mHeight = height;
        mVS = vertShaderModule;
        mPS = fragShaderModule;
        createUniformBuffers();

        createRenderPass();
        createDescriptorSetLayout();
        createDescriptorSets(mDescriptorPool);
        createGraphicsPipeline(vertShaderModule, fragShaderModule, width, height);
    }

    void onResize(std::vector<VkImageView>& imageViews, VkImageView& depthImageView, uint32_t width, uint32_t height);

    void onDestroy();

    void updateUniformBuffer(uint32_t currentImage);

    RenderPass(/* args */);
    ~RenderPass();

    void record(VkCommandBuffer& cmd, VkBuffer vertexBuffer, VkBuffer indexBuffer, uint32_t indicesCount, uint32_t width, uint32_t height, uint32_t imageIndex);
};
} // namespace nevk
