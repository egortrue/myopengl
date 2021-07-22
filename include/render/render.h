#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_SILENT_WARNINGS
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "computepass.h"
#include "depthpass.h"
#include "gbuffer.h"
#include "gbufferpass.h"
#include "renderpass.h"

#include <modelloader/modelloader.h>
#include <resourcemanager/resourcemanager.h>
#include <scene/scene.h>
#include <shadermanager/ShaderManager.h>
#include <ui/ui.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

const int MAX_FRAMES_IN_FLIGHT = 3;

const uint32_t SHADOW_MAP_WIDTH = 1024;
const uint32_t SHADOW_MAP_HEIGHT = 1024;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
#ifdef __APPLE__
    ,
    "VK_KHR_portability_subset"
#endif
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif


struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Render
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

    std::string MODEL_PATH;
    std::string MTL_PATH;
    uint32_t WIDTH;
    uint32_t HEIGHT;

    void initWindow();
    void initVulkan();
    void cleanup();

private:
    GLFWwindow* mWindow;

    VkInstance mInstance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR mSurface;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mDevice;

    VkQueue mGraphicsQueue;
    VkQueue mPresentQueue;

    VkSwapchainKHR mSwapChain;
    std::vector<VkImage> mSwapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    nevk::Image* depthImage;
    VkImageView depthImageView;

    nevk::Image* textureCompImage;
    VkImageView textureCompImageView;

    nevk::Image* shadowImage;
    VkImageView shadowImageView;

    nevk::ResourceManager* mResManager = nullptr;
    nevk::TextureManager* mTexManager = nullptr;

    GBuffer mGbuffer;
    nevk::GbufferPass mGbufferPass;

    nevk::ModelLoader* modelLoader = nullptr;
    nevk::ComputePass mComputePass;
    nevk::DepthPass mDepthPass;

    struct SceneRenderData
    {
        uint32_t cameraIndex = 0;
        uint32_t mIndicesCount = 0;
        uint32_t mInstanceCount = 0;
        nevk::Buffer* mVertexBuffer = nullptr;
        nevk::Buffer* mMaterialBuffer = nullptr;
        nevk::Buffer* mIndexBuffer = nullptr;
        nevk::Buffer* mInstanceBuffer = nullptr;

        nevk::ResourceManager* mResManager = nullptr;
        explicit SceneRenderData(nevk::ResourceManager* resManager)
        {
            mResManager = resManager;
        }
        ~SceneRenderData()
        {
            assert(mResManager);
            if (mVertexBuffer)
            {
                mResManager->destroyBuffer(mVertexBuffer);
            }
            if (mIndexBuffer)
            {
                mResManager->destroyBuffer(mIndexBuffer);
            }
            if (mMaterialBuffer)
            {
                mResManager->destroyBuffer(mMaterialBuffer);
            }
            if (mInstanceBuffer)
            {
                mResManager->destroyBuffer(mInstanceBuffer);
            }
        }
    };

    SceneRenderData* mCurrentSceneRenderData = nullptr;
    SceneRenderData* mDefaultSceneRenderData = nullptr;

    VkDescriptorPool mDescriptorPool;

    struct FrameData
    {
        VkCommandBuffer cmdBuffer;
        VkCommandPool cmdPool;
        VkFence inFlightFence;
        VkFence imagesInFlight;
        VkSemaphore renderFinished;
        VkSemaphore imageAvailable;
    };
    FrameData mFramesData[MAX_FRAMES_IN_FLIGHT] = {};

    FrameData& getFrameData(uint32_t idx)
    {
        return mFramesData[idx % MAX_FRAMES_IN_FLIGHT];
    }

    size_t mCurrentFrame = 0;

    // fps counter
    double msPerFrame = 33.33;

    bool framebufferResized = false;

    nevk::Ui mUi;
    nevk::ShaderManager mShaderManager;
    nevk::Scene* mScene = nullptr;
    nevk::Scene* mDefaultScene = nullptr;

    bool isPBR = true;

    void loadScene(const std::string& modelPath);

    void createDefaultScene();

    void setDescriptors();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

    static void handleMouseMoveCallback(GLFWwindow* window, double xpos, double ypos);

    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    void mainLoop();

    double fpsCounter(double frameTime);

    void cleanupSwapChain();

    void recreateSwapChain();

    void createInstance();

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void setupDebugMessenger();

    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();

    void createSwapChain();

    void createImageViews();

    GBuffer createGbuffer(uint32_t width, uint32_t height);
    void destroyGbuffer(GBuffer& gbuffer);
    void createGbufferPass();

    void createCommandPool();

    void createDepthResources();

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkFormat findDepthFormat();

    bool hasStencilComponent(VkFormat format);

    void setCamera();

    void createVertexBuffer(nevk::Scene& scene);
    void createMaterialBuffer(nevk::Scene& scene);
    void createIndexBuffer(nevk::Scene& scene);
    void createInstanceBuffer(nevk::Scene& scene);

    void createDescriptorPool();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void recordBarrier(VkCommandBuffer& cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

    void recordCommandBuffer(VkCommandBuffer& cmd, uint32_t imageIndex);

    void createCommandBuffers();

    void createSyncObjects();

    void drawFrame();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    bool isDeviceSuitable(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    std::vector<const char*> getRequiredExtensions();

    bool checkValidationLayerSupport();

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, [[maybe_unused]] void* pUserData)
    {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            std::cout << "Warning: " << pCallbackData->messageIdNumber << ":" << pCallbackData->pMessageIdName << ":" << pCallbackData->pMessage << std::endl;
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            std::cerr << "Error: " << pCallbackData->messageIdNumber << ":" << pCallbackData->pMessageIdName << ":" << pCallbackData->pMessage << std::endl;
        }
        else
        {
            std::cerr << "Validation: " << pCallbackData->messageIdNumber << ":" << pCallbackData->pMessageIdName << ":" << pCallbackData->pMessage << std::endl;
        }
        return VK_FALSE;
    }

public:
    VkPhysicalDevice getPhysicalDevice()
    {
        return mPhysicalDevice;
    }
    QueueFamilyIndices getQueueFamilies(VkPhysicalDevice mdevice)
    {
        return findQueueFamilies(mdevice);
    }
    VkDescriptorPool getDescriptorPool()
    {
        return mDescriptorPool;
    }
    VkDevice getDevice()
    {
        return mDevice;
    }
    VkInstance getInstance()
    {
        return mInstance;
    }
    VkQueue getGraphicsQueue()
    {
        return mGraphicsQueue;
    }
    VkFormat getSwapChainImageFormat()
    {
        return swapChainImageFormat;
    }
    GLFWwindow* getWindow()
    {
        return mWindow;
    }
    FrameData* getFramesData()
    {
        return mFramesData;
    }
    VkExtent2D getSwapChainExtent()
    {
        return swapChainExtent;
    }
    std::vector<VkImageView>& getSwapChainImageViews()
    {
        return swapChainImageViews;
    }
    size_t getCurrentFrameIndex()
    {
        return mCurrentFrame % MAX_FRAMES_IN_FLIGHT;
    }
    FrameData& getCurrentFrameData()
    {
        return mFramesData[mCurrentFrame % MAX_FRAMES_IN_FLIGHT];
    }

    nevk::ResourceManager* getResManager()
    {
        return mResManager;
    }

    nevk::Scene* getScene()
    {
        return mScene;
    }

    uint32_t getActiveCameraIndex()
    {
        return mCurrentSceneRenderData->cameraIndex;
    }

    nevk::TextureManager* getTexManager()
    {
        return mTexManager;
    }

    SceneRenderData* getSceneData()
    {
        return mCurrentSceneRenderData;
    }

    void setDepthResources()
    {
        createDepthResources();
    }
    nevk::Ui getUi()
    {
        return mUi;
    }
    void setUi(nevk::Ui _mUi)
    {
        mUi = _mUi;
    }
};
