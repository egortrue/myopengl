#pragma once
#include <resourcemanager/resourcemanager.h>
#include <vulkan/vulkan.h>

struct GBuffer
{
    nevk::Image* wPos;
    VkImageView wPosView;
    nevk::Image* depth;
    VkImageView depthView;
    nevk::Image* normal;
    VkImageView normalView;
    nevk::Image* tangent;
    VkImageView tangentView;
    nevk::Image* uv;
    VkImageView uvView;
    nevk::Image* instId;
    VkImageView instIdView;

    // utils
    VkFormat depthFormat;
    uint32_t width;
    uint32_t height;
};
