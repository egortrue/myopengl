#include "render.h"

#include "debugUtils.h"

#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

[[maybe_unused]] static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

[[maybe_unused]] static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

using namespace nevk;

void Render::initVulkan()
{
    createInstance();
    setupDebugMessenger();
    if (enableValidationLayers)
    {
        //nevk::debug::setupDebug(mInstance);
    }
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();

    // load shaders
    const char* csShaderCode = nullptr;
    uint32_t csShaderCodeSize = 0;
    const char* shShaderCode = nullptr;
    uint32_t shShaderCodeSize = 0;
    const char* pbrVertShaderCode = nullptr;
    uint32_t pbrVertShaderCodeSize = 0;
    const char* pbrFragShaderCode = nullptr;
    uint32_t pbrFragShaderCodeSize = 0;
    const char* simpleVertShaderCode = nullptr;
    uint32_t simpleVertShaderCodeSize = 0;
    const char* simpleFragShaderCode = nullptr;
    uint32_t simpleFragShaderCodeSize = 0;

    uint32_t csId = mShaderManager.loadShader("shaders/compute.hlsl", "computeMain", nevk::ShaderManager::Stage::eCompute);
    mShaderManager.getShaderCode(csId, csShaderCode, csShaderCodeSize);

    uint32_t shId = mShaderManager.loadShader("shaders/shadowmap.hlsl", "vertexMain", nevk::ShaderManager::Stage::eVertex);
    mShaderManager.getShaderCode(shId, shShaderCode, shShaderCodeSize);

    uint32_t pbrVertId = mShaderManager.loadShader("shaders/pbr.hlsl", "vertexMain", nevk::ShaderManager::Stage::eVertex);
    uint32_t pbrFragId = mShaderManager.loadShader("shaders/pbr.hlsl", "fragmentMain", nevk::ShaderManager::Stage::ePixel);

    mShaderManager.getShaderCode(pbrVertId, pbrVertShaderCode, pbrVertShaderCodeSize);
    mShaderManager.getShaderCode(pbrFragId, pbrFragShaderCode, pbrFragShaderCodeSize);

    uint32_t simpleVertId = mShaderManager.loadShader("shaders/simple.hlsl", "vertexMain", nevk::ShaderManager::Stage::eVertex);
    uint32_t simpleFragId = mShaderManager.loadShader("shaders/simple.hlsl", "fragmentMain", nevk::ShaderManager::Stage::ePixel);

    mShaderManager.getShaderCode(simpleVertId, simpleVertShaderCode, simpleVertShaderCodeSize);
    mShaderManager.getShaderCode(simpleFragId, simpleFragShaderCode, simpleFragShaderCodeSize);

    createDescriptorPool();
    createCommandPool();

    mResManager = new nevk::ResourceManager(mDevice, mPhysicalDevice, mInstance, getCurrentFrameData().cmdPool, mGraphicsQueue);
    mTexManager = new nevk::TextureManager(mDevice, mPhysicalDevice, mResManager);

    createImageViews();
    createCommandBuffers();
    createSyncObjects();

    createDepthResources();

    {
        shadowImage = mResManager->createImage(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, findDepthFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "ShadowMap");
        shadowImageView = mTexManager->createImageView(mResManager->getVkImage(shadowImage), findDepthFormat(), VK_IMAGE_ASPECT_DEPTH_BIT);
    }
    mTexManager->createShadowSampler();
    mTexManager->createTextureSampler();

    mGbuffer = createGbuffer(swapChainExtent.width, swapChainExtent.height);
    createGbufferPass();

    modelLoader = new nevk::ModelLoader(mTexManager);
    createDefaultScene();
    if (!MODEL_PATH.empty())
    {
        mTexManager->saveTexturesInDelQueue();
        loadScene(MODEL_PATH);
    }

    //init passes
    {
        textureCompImage = mResManager->createImage(swapChainExtent.width, swapChainExtent.height, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                    VK_IMAGE_TILING_OPTIMAL,
                                                    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Shading result");
        textureCompImageView = mTexManager->createImageView(mResManager->getVkImage(textureCompImage), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
        mTexManager->transitionImageLayout(mResManager->getVkImage(textureCompImage), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
    mComputePass.setGbuffer(&mGbuffer);
    mComputePass.setOutputImageView(textureCompImageView);
    mComputePass.setTextureSampler(mTexManager->textureSampler);
    mComputePass.init(mDevice, csShaderCode, csShaderCodeSize, mDescriptorPool, mResManager);

    mDepthPass.init(mDevice, enableValidationLayers, shShaderCode, shShaderCodeSize, mDescriptorPool, mResManager, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);
    mDepthPass.createFrameBuffers(shadowImageView, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);

    QueueFamilyIndices indicesFamily = findQueueFamilies(mPhysicalDevice);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.DescriptorPool = mDescriptorPool;
    init_info.Device = mDevice;
    init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
    init_info.Instance = mInstance;
    init_info.MinImageCount = 2;
    init_info.PhysicalDevice = mPhysicalDevice;
    init_info.Queue = mGraphicsQueue;
    init_info.QueueFamily = indicesFamily.graphicsFamily.value();

    mUi.init(init_info, swapChainImageFormat, mWindow, mFramesData[0].cmdPool, mFramesData[0].cmdBuffer, swapChainExtent.width, swapChainExtent.height);
    mUi.createFrameBuffers(mDevice, swapChainImageViews, swapChainExtent.width, swapChainExtent.height);
}

void Render::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    if (width == 0 || height == 0)
    {
        return;
    }
    auto app = reinterpret_cast<Render*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
    nevk::Scene* scene = app->getScene();
    scene->updateCamerasParams(width, height);
}

inline void Render::keyCallback(GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
{
    auto app = reinterpret_cast<Render*>(glfwGetWindowUserPointer(window));
    nevk::Scene* scene = app->getScene();
    Camera& camera = scene->getCamera(app->getActiveCameraIndex());

    const bool keyState = ((GLFW_REPEAT == action) || (GLFW_PRESS == action)) ? true : false;
    switch (key)

    {
    case GLFW_KEY_W: {
        camera.keys.forward = keyState;
        break;
    }
    case GLFW_KEY_S: {
        camera.keys.back = keyState;
        break;
    }
    case GLFW_KEY_A: {
        camera.keys.left = keyState;
        break;
    }
    case GLFW_KEY_D: {
        camera.keys.right = keyState;
        break;
    }
    case GLFW_KEY_Q: {
        camera.keys.up = keyState;
        break;
    }
    case GLFW_KEY_E: {
        camera.keys.down = keyState;
        break;
    }
    default:
        break;
    }
}

inline void Render::mouseButtonCallback(GLFWwindow* window, int button, int action, [[maybe_unused]] int mods)
{
    auto app = reinterpret_cast<Render*>(glfwGetWindowUserPointer(window));
    nevk::Scene* scene = app->getScene();
    Camera& camera = scene->getCamera(app->getActiveCameraIndex());
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            camera.mouseButtons.right = true;
        }
        else if (action == GLFW_RELEASE)
        {
            camera.mouseButtons.right = false;
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            camera.mouseButtons.left = true;
        }
        else if (action == GLFW_RELEASE)
        {
            camera.mouseButtons.left = false;
        }
    }
}

void Render::handleMouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
    auto app = reinterpret_cast<Render*>(glfwGetWindowUserPointer(window));
    nevk::Scene* scene = app->getScene();
    Camera& camera = scene->getCamera(app->getActiveCameraIndex());
    const float dx = camera.mousePos.x - (float)xpos;
    const float dy = camera.mousePos.y - (float)ypos;

    ImGuiIO& io = ImGui::GetIO();
    bool handled = io.WantCaptureMouse;
    if (handled)
    {
        camera.mousePos = glm::vec2((float)xpos, (float)ypos);
        return;
    }

    if (camera.mouseButtons.right)
    {
        camera.rotate(-dx, -dy);
    }
    if (camera.mouseButtons.left)
    {
        camera.translate(glm::float3(-0.0f, 0.0f, -dy * .005f * camera.movementSpeed));
    }
    if (camera.mouseButtons.middle)
    {
        camera.translate(glm::float3(-dx * 0.01f, -dy * 0.01f, 0.0f));
    }
    camera.mousePos = glm::float2((float)xpos, (float)ypos);
}

void Render::scrollCallback(GLFWwindow* window, [[maybe_unused]] double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    bool handled = io.WantCaptureMouse;
    if (handled)
    {
        return;
    }

    auto app = reinterpret_cast<Render*>(glfwGetWindowUserPointer(window));
    nevk::Scene* mScene = app->getScene();
    Camera& mCamera = mScene->getCamera(app->getActiveCameraIndex());

    mCamera.translate(glm::vec3(0.0f, 0.0f,
                                -yoffset * mCamera.movementSpeed));
}

double Render::fpsCounter(double frameTime)
{
    static double elapsedTime = 0.0;
    static uint64_t framesCounter = 0;

    elapsedTime += frameTime;
    ++framesCounter;

    if (elapsedTime >= 1000.0)
    {
        msPerFrame = elapsedTime / framesCounter;
        framesCounter = 0;
        elapsedTime = 0;
    }

    return msPerFrame;
}

void Render::mainLoop()
{
    while (!glfwWindowShouldClose(mWindow))
    {
        auto start = std::chrono::high_resolution_clock::now();

        glfwPollEvents();
        drawFrame();

        auto finish = std::chrono::high_resolution_clock::now();

        double frameTime = std::chrono::duration<double, std::milli>(finish - start).count();
        msPerFrame = fpsCounter(frameTime);
    }

    vkDeviceWaitIdle(mDevice);
}

void Render::cleanupSwapChain()
{
    vkDestroyImageView(mDevice, depthImageView, nullptr);
    mResManager->destroyImage(depthImage);

    for (auto& framebuffer : swapChainFramebuffers)
    {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }

    for (auto& imageView : swapChainImageViews)
    {
        vkDestroyImageView(mDevice, imageView, nullptr);
    }

    vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
}

void Render::cleanup()
{
    cleanupSwapChain();

    mDepthPass.onDestroy();
    mGbufferPass.onDestroy();
    mComputePass.onDestroy();

    mUi.onDestroy();

    vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

    mTexManager->textureDestroy();
    mTexManager->delTexturesFromQueue();

    vkDestroyImageView(mDevice, textureCompImageView, nullptr);
    mResManager->destroyImage(textureCompImage);

    vkDestroyImageView(mDevice, shadowImageView, nullptr);
    mResManager->destroyImage(shadowImage);

    destroyGbuffer(mGbuffer);

    if (mScene != mDefaultScene)
        delete mCurrentSceneRenderData;

    for (FrameData& fd : mFramesData)
    {
        vkDestroySemaphore(mDevice, fd.renderFinished, nullptr);
        vkDestroySemaphore(mDevice, fd.imageAvailable, nullptr);
        vkDestroyFence(mDevice, fd.inFlightFence, nullptr);

        vkDestroyCommandPool(mDevice, fd.cmdPool, nullptr);
    }

    delete mResManager;

    vkDestroyDevice(mDevice, nullptr);

    if (enableValidationLayers)
    {
        DestroyDebugUtilsMessengerEXT(mInstance, debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    vkDestroyInstance(mInstance, nullptr);

    glfwDestroyWindow(mWindow);

    glfwTerminate();
}

void Render::initWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    mWindow = glfwCreateWindow(WIDTH, HEIGHT, "NeVK", nullptr, nullptr);
    glfwSetWindowUserPointer(mWindow, this);
    glfwSetFramebufferSizeCallback(mWindow, framebufferResizeCallback);
    glfwSetKeyCallback(mWindow, keyCallback);
    glfwSetMouseButtonCallback(mWindow, mouseButtonCallback);
    glfwSetCursorPosCallback(mWindow, handleMouseMoveCallback);
    glfwSetScrollCallback(mWindow, scrollCallback);
}

void Render::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(mWindow, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(mWindow, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(mDevice);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();

    // TODO: add release prev gbuffer
    {
        mResManager->destroyImage(textureCompImage);
        vkDestroyImageView(mDevice, textureCompImageView, nullptr);
    }
    {
        textureCompImage = mResManager->createImage(swapChainExtent.width, swapChainExtent.height, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                    VK_IMAGE_TILING_OPTIMAL,
                                                    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Shading result");
        textureCompImageView = mTexManager->createImageView(mResManager->getVkImage(textureCompImage), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
        mTexManager->transitionImageLayout(mResManager->getVkImage(textureCompImage), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
    mComputePass.setOutputImageView(textureCompImageView);
    destroyGbuffer(mGbuffer);
    mGbuffer = createGbuffer(width, height);
    mGbufferPass.onResize(&mGbuffer);

    mUi.onResize(swapChainImageViews, width, height);

    // update all cameras
    mScene->updateCamerasParams(swapChainExtent.width, swapChainExtent.height);
}

void Render::createInstance()
{
    if (enableValidationLayers && !checkValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "NEVK";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "NoErrorVulkan Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create instance!");
    }
}

void Render::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}


void Render::setupDebugMessenger()
{
    if (!enableValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void Render::createSurface()
{
    if (glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
}

void Render::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

    for (const VkPhysicalDevice& device : devices)
    {
        if (isDeviceSuitable(device))
        {
            mPhysicalDevice = device;
            break;
        }
    }

    if (mPhysicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void Render::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    {
        VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures{};
        indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
        indexingFeatures.pNext = nullptr;

        VkPhysicalDeviceFeatures2 deviceFeatures{};
        deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures.pNext = &indexingFeatures;
        vkGetPhysicalDeviceFeatures2(mPhysicalDevice, &deviceFeatures);

        if (indexingFeatures.descriptorBindingPartiallyBound && indexingFeatures.runtimeDescriptorArray)
        {
            // all set to use unbound arrays of textures
        }
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures{};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    indexingFeatures.pNext = nullptr;
    indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    indexingFeatures.runtimeDescriptorArray = VK_TRUE;
    indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    indexingFeatures.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    VkPhysicalDeviceRobustness2FeaturesEXT robustnessFeatures{};
    robustnessFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
    robustnessFeatures.nullDescriptor = VK_TRUE;
    robustnessFeatures.pNext = &indexingFeatures;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &robustnessFeatures;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(mDevice, indices.graphicsFamily.value(), 0, &mGraphicsQueue);
    vkGetDeviceQueue(mDevice, indices.presentFamily.value(), 0, &mPresentQueue);
}

void Render::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(mPhysicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = mSurface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapChain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, nullptr);
    mSwapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, mSwapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void Render::createImageViews()
{
    swapChainImageViews.resize(mSwapChainImages.size());

    for (uint32_t i = 0; i < mSwapChainImages.size(); i++)
    {
        swapChainImageViews[i] = mTexManager->createImageView(mSwapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

GBuffer Render::createGbuffer(uint32_t width, uint32_t height)
{
    GBuffer res{};
    res.width = width;
    res.height = height;
    // Depth
    {
        res.depthFormat = findDepthFormat();
        res.depth = mResManager->createImage(width, height, res.depthFormat, VK_IMAGE_TILING_OPTIMAL,
                                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "depth");
        res.depthView = mTexManager->createImageView(mResManager->getVkImage(res.depth), res.depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
    // Normals
    {
        res.normal = mResManager->createImage(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "normal");
        res.normalView = mTexManager->createImageView(mResManager->getVkImage(res.normal), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    // Tangent
    {
        res.tangent = mResManager->createImage(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "tangent");
        res.tangentView = mTexManager->createImageView(mResManager->getVkImage(res.tangent), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    // wPos
    {
        res.wPos = mResManager->createImage(width, height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "wPos");
        res.wPosView = mTexManager->createImageView(mResManager->getVkImage(res.wPos), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    // UV
    {
        res.uv = mResManager->createImage(width, height, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "UV");
        res.uvView = mTexManager->createImageView(mResManager->getVkImage(res.uv), VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    // InstId
    {
        res.instId = mResManager->createImage(width, height, VK_FORMAT_R32_SINT, VK_IMAGE_TILING_OPTIMAL,
                                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "instId");
        res.instIdView = mTexManager->createImageView(mResManager->getVkImage(res.instId), VK_FORMAT_R32_SINT, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    return res;
}

void Render::destroyGbuffer(GBuffer& gbuffer)
{
    mResManager->destroyImage(gbuffer.depth);
    vkDestroyImageView(mDevice, gbuffer.depthView, nullptr);
    mResManager->destroyImage(gbuffer.wPos);
    vkDestroyImageView(mDevice, gbuffer.wPosView, nullptr);
    mResManager->destroyImage(gbuffer.normal);
    vkDestroyImageView(mDevice, gbuffer.normalView, nullptr);
    mResManager->destroyImage(gbuffer.tangent);
    vkDestroyImageView(mDevice, gbuffer.tangentView, nullptr);
    mResManager->destroyImage(gbuffer.uv);
    vkDestroyImageView(mDevice, gbuffer.uvView, nullptr);
    mResManager->destroyImage(gbuffer.instId);
    vkDestroyImageView(mDevice, gbuffer.instIdView, nullptr);
}

void Render::createGbufferPass()
{
    uint32_t vertId = mShaderManager.loadShader("shaders/gbuffer.hlsl", "vertexMain", nevk::ShaderManager::Stage::eVertex);
    uint32_t fragId = mShaderManager.loadShader("shaders/gbuffer.hlsl", "fragmentMain", nevk::ShaderManager::Stage::ePixel);
    const char* vertShaderCode = nullptr;
    uint32_t vertShaderCodeSize = 0;
    const char* fragShaderCode = nullptr;
    uint32_t fragShaderCodeSize = 0;
    mShaderManager.getShaderCode(vertId, vertShaderCode, vertShaderCodeSize);
    mShaderManager.getShaderCode(fragId, fragShaderCode, fragShaderCodeSize);

    mGbufferPass.setTextureSampler(mTexManager->textureSampler);
    mGbufferPass.init(mDevice, enableValidationLayers, vertShaderCode, vertShaderCodeSize, fragShaderCode, fragShaderCodeSize, mDescriptorPool, mResManager, &mGbuffer);
    mGbufferPass.createFrameBuffers(mGbuffer);
}

void Render::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(mPhysicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    for (FrameData& fd : mFramesData)
    {
        if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &fd.cmdPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics command pool!");
        }
    }
}

void Render::createDepthResources()
{
    VkFormat depthFormat = findDepthFormat();
    depthImage = mResManager->createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    depthImageView = mTexManager->createImageView(mResManager->getVkImage(depthImage), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkFormat Render::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

VkFormat Render::findDepthFormat()
{
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool Render::hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void Render::setCamera()
{
    Camera& camera = mScene->getCamera(getActiveCameraIndex());
    camera.setPerspective(45.0f, (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10000.0f);
    camera.setRotation(glm::quat({ 1.0f, 0.0f, 0.0f, 0.0f }));
}

void Render::createVertexBuffer(nevk::Scene& scene)
{
    std::vector<nevk::Scene::Vertex>& sceneVertices = scene.getVertices();
    VkDeviceSize bufferSize = sizeof(nevk::Scene::Vertex) * sceneVertices.size();
    if (bufferSize == 0)
    {
        return;
    }
    Buffer* stagingBuffer = mResManager->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* stagingBufferMemory = mResManager->getMappedMemory(stagingBuffer);
    memcpy(stagingBufferMemory, sceneVertices.data(), (size_t)bufferSize);
    mCurrentSceneRenderData->mVertexBuffer = mResManager->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "VB");
    mResManager->copyBuffer(mResManager->getVkBuffer(stagingBuffer), mResManager->getVkBuffer(mCurrentSceneRenderData->mVertexBuffer), bufferSize);
    mResManager->destroyBuffer(stagingBuffer);
}

void Render::createMaterialBuffer(nevk::Scene& scene)
{
    std::vector<nevk::Scene::Material>& sceneMaterials = scene.getMaterials();

    VkDeviceSize bufferSize = sizeof(nevk::Scene::Material) * sceneMaterials.size();
    if (bufferSize == 0)
    {
        return;
    }
    Buffer* stagingBuffer = mResManager->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* stagingBufferMemory = mResManager->getMappedMemory(stagingBuffer);
    memcpy(stagingBufferMemory, sceneMaterials.data(), (size_t)bufferSize);
    mCurrentSceneRenderData->mMaterialBuffer = mResManager->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Materials");
    mResManager->copyBuffer(mResManager->getVkBuffer(stagingBuffer), mResManager->getVkBuffer(mCurrentSceneRenderData->mMaterialBuffer), bufferSize);
    mResManager->destroyBuffer(stagingBuffer);
}

void Render::createIndexBuffer(nevk::Scene& scene)
{
    std::vector<uint32_t>& sceneIndices = scene.getIndices();
    mCurrentSceneRenderData->mIndicesCount = (uint32_t)sceneIndices.size();
    VkDeviceSize bufferSize = sizeof(uint32_t) * sceneIndices.size();
    if (bufferSize == 0)
    {
        return;
    }

    Buffer* stagingBuffer = mResManager->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* stagingBufferMemory = mResManager->getMappedMemory(stagingBuffer);
    memcpy(stagingBufferMemory, sceneIndices.data(), (size_t)bufferSize);
    mCurrentSceneRenderData->mIndexBuffer = mResManager->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "IB");
    mResManager->copyBuffer(mResManager->getVkBuffer(stagingBuffer), mResManager->getVkBuffer(mCurrentSceneRenderData->mIndexBuffer), bufferSize);
    mResManager->destroyBuffer(stagingBuffer);
}

void Render::createInstanceBuffer(nevk::Scene& scene)
{
    // This struct should match shader's version
    struct InstanceConstants
    {
        glm::float4x4 model;
        glm::float4x4 normalMatrix;
        int32_t materialId;
        int32_t pad0;
        int32_t pad1;
        int32_t pad2;
    };

    const std::vector<nevk::Instance>& sceneInstances = scene.getInstances();
    mCurrentSceneRenderData->mInstanceCount = (uint32_t)sceneInstances.size();
    VkDeviceSize bufferSize = sizeof(InstanceConstants) * sceneInstances.size();
    if (bufferSize == 0)
    {
        return;
    }
    std::vector<InstanceConstants> instanceConsts;
    instanceConsts.resize(sceneInstances.size());
    for (int i = 0; i < sceneInstances.size(); ++i)
    {
        instanceConsts[i].materialId = sceneInstances[i].mMaterialId;
        instanceConsts[i].model = sceneInstances[i].transform;
        instanceConsts[i].normalMatrix = glm::inverse(glm::transpose(sceneInstances[i].transform));
    }

    Buffer* stagingBuffer = mResManager->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* stagingBufferMemory = mResManager->getMappedMemory(stagingBuffer);
    memcpy(stagingBufferMemory, instanceConsts.data(), (size_t)bufferSize);
    mCurrentSceneRenderData->mInstanceBuffer = mResManager->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Instance consts");
    mResManager->copyBuffer(mResManager->getVkBuffer(stagingBuffer), mResManager->getVkBuffer(mCurrentSceneRenderData->mInstanceBuffer), bufferSize);
    mResManager->destroyBuffer(stagingBuffer);
}

void Render::createDescriptorPool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.poolSizeCount = sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize);
    poolInfo.pPoolSizes = pool_sizes;
    poolInfo.maxSets = 1000 * poolInfo.poolSizeCount;

    if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

uint32_t Render::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void Render::recordBarrier(VkCommandBuffer& cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkImageAspectFlags aspectMask)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(
        cmd,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
}

void Render::recordCommandBuffer(VkCommandBuffer& cmd, uint32_t imageIndex)
{
    mDepthPass.record(cmd, mResManager->getVkBuffer(mCurrentSceneRenderData->mVertexBuffer), mResManager->getVkBuffer(mCurrentSceneRenderData->mIndexBuffer), *mScene, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, imageIndex, getActiveCameraIndex());
    mGbufferPass.record(cmd, mResManager->getVkBuffer(mCurrentSceneRenderData->mVertexBuffer), mResManager->getVkBuffer(mCurrentSceneRenderData->mIndexBuffer), *mScene, swapChainExtent.width, swapChainExtent.height, imageIndex, getActiveCameraIndex());

    // barriers
    {
        recordBarrier(cmd, mResManager->getVkImage(mGbuffer.wPos), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        recordBarrier(cmd, mResManager->getVkImage(mGbuffer.normal), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        recordBarrier(cmd, mResManager->getVkImage(mGbuffer.tangent), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        recordBarrier(cmd, mResManager->getVkImage(mGbuffer.uv), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        recordBarrier(cmd, mResManager->getVkImage(mGbuffer.instId), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        recordBarrier(cmd, mResManager->getVkImage(mGbuffer.depth), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
    mComputePass.record(cmd, swapChainExtent.width, swapChainExtent.height, imageIndex);

    // Copy to swapchain image
    {

        recordBarrier(cmd, mResManager->getVkImage(textureCompImage), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        recordBarrier(cmd, mSwapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkOffset3D blitSize{};
        blitSize.x = swapChainExtent.width;
        blitSize.y = swapChainExtent.height;
        blitSize.z = 1;
        VkImageBlit imageBlitRegion{};
        imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.srcSubresource.layerCount = 1;
        imageBlitRegion.srcOffsets[1] = blitSize;
        imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.dstSubresource.layerCount = 1;
        imageBlitRegion.dstOffsets[1] = blitSize;
        vkCmdBlitImage(cmd, mResManager->getVkImage(textureCompImage), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mSwapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlitRegion, VK_FILTER_NEAREST);

        recordBarrier(cmd, mResManager->getVkImage(textureCompImage), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                      VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        recordBarrier(cmd, mSwapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    mUi.render(cmd, imageIndex);
}

void Render::createCommandBuffers()
{
    for (FrameData& fd : mFramesData)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = fd.cmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(mDevice, &allocInfo, &fd.cmdBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }
}

void Render::createSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mFramesData[i].renderFinished) != VK_SUCCESS ||
            vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mFramesData[i].imageAvailable) != VK_SUCCESS ||
            vkCreateFence(mDevice, &fenceInfo, nullptr, &mFramesData[i].inFlightFence) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

// load into normal scene
void Render::loadScene(const std::string& modelPath)
{
    if (mScene != nullptr && mScene != mDefaultScene)
        delete mScene;
    mScene = new nevk::Scene;

    mCurrentSceneRenderData = new SceneRenderData(mResManager);
    MODEL_PATH = modelPath;

    isPBR = true;
    bool res = modelLoader->loadModelGltf(MODEL_PATH, *mScene);
    // bool res = testmodel.loadModel(MODEL_PATH, MTL_PATH, *mScene);
    if (!res)
    {
        return;
    }

    createMaterialBuffer(*mScene);
    createInstanceBuffer(*mScene);

    mTexManager->createShadowSampler();
    mTexManager->createTextureSampler();

    setDescriptors();

    createIndexBuffer(*mScene);
    createVertexBuffer(*mScene);
}

void Render::setDescriptors()
{
    {
        mGbufferPass.setTextureImageView(mTexManager->textureImageView);
        mGbufferPass.setTextureSampler(mTexManager->textureSampler);
        mGbufferPass.setMaterialBuffer(mResManager->getVkBuffer(mCurrentSceneRenderData->mMaterialBuffer));
        mGbufferPass.setInstanceBuffer(mResManager->getVkBuffer(mCurrentSceneRenderData->mInstanceBuffer));
    }
    {
        mDepthPass.setInstanceBuffer(mResManager->getVkBuffer(mCurrentSceneRenderData->mInstanceBuffer));
    }
    {
        mComputePass.setGbuffer(&mGbuffer);
        mComputePass.setTextureImageViews(mTexManager->textureImageView);
        mComputePass.setTextureSampler(mTexManager->textureSampler);
        mComputePass.setMaterialBuffer(mResManager->getVkBuffer(mCurrentSceneRenderData->mMaterialBuffer));
        mComputePass.setInstanceBuffer(mResManager->getVkBuffer(mCurrentSceneRenderData->mInstanceBuffer));
    }
}

// set default scene
void Render::createDefaultScene()
{
    mDefaultScene = new nevk::Scene;
    mScene = mDefaultScene;
    mDefaultSceneRenderData = new SceneRenderData(mResManager);
    mCurrentSceneRenderData = mDefaultSceneRenderData;

    Camera camera = {};
    camera.setPerspective(45.0f, (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10000.0f);
    camera.setRotation(glm::quat({ 1.0f, 0.0f, 0.0f, 0.0f }));

    mScene->addCamera(camera);

    createMaterialBuffer(*mScene);
    createInstanceBuffer(*mScene);

    setDescriptors();

    createIndexBuffer(*mScene);
    createVertexBuffer(*mScene);
}

void Render::drawFrame()
{
    FrameData& currFrame = getCurrentFrameData();

    vkWaitForFences(mDevice, 1, &currFrame.inFlightFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(mDevice, mSwapChain, UINT64_MAX, currFrame.imageAvailable, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    if (getFrameData(imageIndex).imagesInFlight != VK_NULL_HANDLE)
    {
        vkWaitForFences(mDevice, 1, &getFrameData(imageIndex).imagesInFlight, VK_TRUE, UINT64_MAX);
    }
    getFrameData(imageIndex).imagesInFlight = currFrame.inFlightFence;

    static auto prevTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    double deltaTime = std::chrono::duration<double, std::milli>(currentTime - prevTime).count() / 1000.0;
    prevTime = currentTime;

    const uint32_t frameIndex = imageIndex;

    nevk::Scene* scene = getScene();
    scene->updateCamerasParams(swapChainExtent.width, swapChainExtent.height);
    Camera& cam = scene->getCamera(getActiveCameraIndex());
    cam.update((float)deltaTime);

    const glm::float4x4 lightSpaceMatrix = mDepthPass.computeLightSpaceMatrix((glm::float3&)scene->mLightPosition);

    mGbufferPass.updateUniformBuffer(frameIndex, lightSpaceMatrix, *scene, getActiveCameraIndex());

    mDepthPass.updateUniformBuffer(frameIndex, lightSpaceMatrix);
    mComputePass.updateUniformBuffer(frameIndex, lightSpaceMatrix, *scene, getActiveCameraIndex(), swapChainExtent.width, swapChainExtent.height);

    static int releaseAfterFrames = 0;
    static bool needReload = false;
    static SceneRenderData* toRemoveSceneData = nullptr;
    std::string newModelPath;

    mUi.updateUI(*scene, mDepthPass, msPerFrame, newModelPath, mCurrentSceneRenderData->cameraIndex);

    if (!newModelPath.empty() && fs::exists(newModelPath) && newModelPath != MODEL_PATH)
    {
        if (mScene != mDefaultScene) // if we reload non-default scene
        {
            // save scene data to remove
            toRemoveSceneData = mCurrentSceneRenderData;
            needReload = true;
            releaseAfterFrames = MAX_FRAMES_IN_FLIGHT;
        }
        mTexManager->saveTexturesInDelQueue();
        loadScene(newModelPath);
    }

    if (needReload && releaseAfterFrames == 0)
    {
        mTexManager->delTexturesFromQueue();
        delete toRemoveSceneData;
        needReload = false;
    }
    if (needReload)
    {
        --releaseAfterFrames;
    }

    glfwSetWindowTitle(mWindow, (std::string("NeVK") + " [" + std::to_string(msPerFrame) + " ms]").c_str());

    VkCommandBuffer& cmdBuff = getFrameData(imageIndex).cmdBuffer;
    vkResetCommandBuffer(cmdBuff, 0);

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.pNext = nullptr;
    cmdBeginInfo.pInheritanceInfo = nullptr;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    vkBeginCommandBuffer(cmdBuff, &cmdBeginInfo);

    recordCommandBuffer(cmdBuff, frameIndex);

    if (vkEndCommandBuffer(cmdBuff) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { currFrame.imageAvailable };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuff;

    VkSemaphore signalSemaphores[] = { currFrame.renderFinished };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(mDevice, 1, &currFrame.inFlightFence);

    if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, currFrame.inFlightFence) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { mSwapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(mPresentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
    {
        framebufferResized = false;
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

VkSurfaceFormatKHR Render::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR Render::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Render::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(mWindow, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

SwapChainSupportDetails Render::querySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, mSurface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool Render::isDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool Render::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices Render::findQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface, &presentSupport);

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

std::vector<const char*> Render::getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    return extensions;
}

bool Render::checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers)
    {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}
