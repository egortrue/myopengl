#pragma once
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <resourcemanager/resourcemanager.h>

namespace nevk
{
class TextureManager
{
private:
    VkDevice mDevice;
    VkPhysicalDevice mPhysicalDevice;

    nevk::ResourceManager* mResManager;

public:
    TextureManager(VkDevice device,
                   VkPhysicalDevice phDevice,
                   nevk::ResourceManager* ResManager)
        : mDevice(device), mPhysicalDevice(phDevice), mResManager(ResManager){};

    struct Texture
    {
        VkImage textureImage;
        int texWidth;
        int texHeight;
        VkDeviceMemory textureImageMemory;
    } ;

    std::vector<Texture> textures;
    std::vector<VkImageView> textureImageView;
    VkSampler textureSampler;

    const std::string TEXTURE_PATH = "misc/cat.png";
    const std::string TEXTURE_PATH2 = "misc/viking_room.png";

    void loadTexture(std::string texture_path);

    Texture createTextureImage(std::string texture_path);
    void createTextureImageView(Texture texture);
    void createTextureSampler();

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void textureDestroy()
    {
        vkDestroySampler(mDevice, textureSampler, nullptr);

        for (VkImageView image_view : textureImageView) {
            vkDestroyImageView(mDevice, image_view, nullptr);
        }

        for (Texture tex : textures)
        {
            vkDestroyImage(mDevice, tex.textureImage, nullptr);
            vkFreeMemory(mDevice, tex.textureImageMemory, nullptr);
        }
    }
};
} // namespace nevk