#include "Renderer.hpp"
#include <iostream>
#include <set>
#include <vulkan/vulkan_core.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <type_traits>

#include <stb/stb_image.h>

const std::vector<const char*> gValidationLayers = 
{"VK_LAYER_KHRONOS_validation"};

const std::vector<const char*> gDeviceExtensions = 
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

#ifdef DEBUG
bool enableValidationLayers = true;
#else
bool enableValidationLayers = false;
#endif

ke::Graphics::Renderer &ke::Graphics::Renderer::getInstance()
{
    static Renderer instance;
    return instance;
}

void ke::Graphics::Renderer::init(GLFWwindow* window)
{
    mLogger.trace("Initializing renderer.");
    createVulkanInstance();
    setupDebugMessenger();
    createWindowSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain(window);
    createSwapchainImageViews();
    createDepthResources();
    createRenderPass();
    createFontRenderPass();
    createSceneRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createFontPipeline();
    createFramebuffers();
    createCommandPool();
    createTextureSampler();
    createFontSampler();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffer();
    createSyncObjects();

    mLogger.info("Initialized renderer.");
}

void ke::Graphics::Renderer::terminate()
{
    vkDeviceWaitIdle(mDevice);

    mDepthImage.destroy();

    vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

    vkDestroySampler(mDevice, mTextureSampler, nullptr);
    vkDestroySampler(mDevice, mFontSampler, nullptr);

    for(size_t i = 0; i < MAXFRAMESINFLIGHT; i++)
    {
        vkDestroyBuffer(mDevice, sceneUniformBuffers[i], nullptr);
        vkFreeMemory(mDevice, sceneUniformBuffersMemory[i], nullptr);
    }

    for(size_t i = 0; i < MAXFRAMESINFLIGHT; i++)
    {
        vkDestroyBuffer(mDevice, uniformBuffers[i], nullptr);
        vkFreeMemory(mDevice, uniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorSetLayout(mDevice, mTextureSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mFontSetLayout, nullptr);

    for(size_t i = 0; i < mSwapchainImages.size(); i++)
        vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
    for(int i = 0; i < MAXFRAMESINFLIGHT; i++)
    {
        vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(mDevice, mInFlightFences[i], nullptr);    
    }
    
    cleanupSwapchain();

    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    vkDestroyPipeline(mDevice, mPipeline, nullptr);
    vkDestroyPipeline(mDevice, mDisplayPipeline, nullptr);
    vkDestroyPipeline(mDevice, mFontPipeline, nullptr);

    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    vkDestroyPipelineLayout(mDevice, mFontPipelineLayout, nullptr);
    vkDestroyRenderPass(mDevice, mSceneRenderPass, nullptr);
    vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    vkDestroyRenderPass(mDevice, mFontRenderPass, nullptr);

    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    vkDestroyDevice(mDevice, nullptr);
    DestroyDebugUtilsMessenger(mInstance, mDebugMessenger, nullptr);
    vkDestroyInstance(mInstance, nullptr);
}

void ke::Graphics::Renderer::readyCanvas(GLFWwindow *window)
{
    vkWaitForFences(mDevice, 1, &mInFlightFences[currentFrameInFlight], VK_TRUE, UINT64_MAX);

    VkResult status = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX, mImageAvailableSemaphores[currentFrameInFlight], VK_NULL_HANDLE, &currentImageIndex);

    if(status == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapchain(window);

        currentFrameInFlight = (currentFrameInFlight + 1) % MAXFRAMESINFLIGHT;

        return;
    }
    else if(status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR)
        mLogger.error("Failed to acquire swapchain image!");

    if(mImagesInFlight[currentImageIndex] != VK_NULL_HANDLE)
        vkWaitForFences(mDevice, 1, &mImagesInFlight[currentImageIndex], VK_TRUE, UINT64_MAX);

    mImagesInFlight[currentImageIndex] = mInFlightFences[currentFrameInFlight];

    vkResetFences(mDevice, 1, &mInFlightFences[currentFrameInFlight]);

    vkResetCommandBuffer(mCommandBuffers[currentFrameInFlight], 0);

    beginRecording(mCommandBuffers[currentFrameInFlight]);
    vkCmdBindDescriptorSets(mCommandBuffers[currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mTextureDescriptorSet, 0, nullptr);
}

void ke::Graphics::Renderer::updateUIUniforms(float aspectRatio)
{
    util::UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    ubo.view = glm::mat4(1.0f);
    
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentFrameInFlight], &ubo, sizeof(ubo));
}

void ke::Graphics::Renderer::updateSceneUniforms(float aspectRatio)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    util::UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f) );
    ubo.proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    ubo.view = glm::lookAt(glm::vec3{1.0f, 1.0f, 2.0f}, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    ubo.proj[1][1] *= -1;

    memcpy(sceneUniformBuffersMapped[currentFrameInFlight], &ubo, sizeof(ubo));
}

void ke::Graphics::Renderer::updateFontUniforms()
{
    if(!framebufferResized) return;

    glm::vec2 extentVec = {mSwapchainExtent.width, mSwapchainExtent.height};

    for(unsigned int i = 0; i < MAXFRAMESINFLIGHT; i++)
        memcpy(fontUniformBufferMapped, &extentVec, sizeof(glm::vec2));
}

void ke::Graphics::Renderer::finishDraw(GLFWwindow *window)
{
    endRecording(mCommandBuffers[currentFrameInFlight]);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {mImageAvailableSemaphores[currentFrameInFlight]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mCommandBuffers[currentFrameInFlight];
    
    VkSemaphore signalSemaphores[] = {mRenderFinishedSemaphores[currentImageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if(vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[currentFrameInFlight]) != VK_SUCCESS)
        mLogger.error("Failed to submit to graphics queue!");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapchains[] = {mSwapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &currentImageIndex;

    VkResult result = vkQueuePresentKHR(mPresentQueue, &presentInfo);

    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
    {
        recreateSwapchain(window);
        framebufferResized = false;
    }
    currentFrameInFlight = (currentFrameInFlight + 1) % MAXFRAMESINFLIGHT;
}

glm::ivec2 ke::Graphics::Renderer::getSwapchainDimensions() const
{
    return glm::ivec2(mSwapchainExtent.width, mSwapchainExtent.height);
}

void ke::Graphics::Renderer::createTextureImage(const std::string &filepath, util::Image &image)
{
    int texWidth, texHeight, numColCh;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &numColCh, STBI_rgb_alpha);
    mMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if(!pixels)
        mLogger.error("Failed to load texture details!");

    
    util::Buffer stagingBuffer(mDevice);

    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer.buffer, stagingBuffer.bufferMemory);

    void* data;
    vkMapMemory(mDevice, stagingBuffer.bufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, imageSize);
    vkUnmapMemory(mDevice, stagingBuffer.bufferMemory);

    stbi_image_free(pixels);

    createImage(texWidth, texHeight, mMipLevels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image.image, image.imageMemory);

    transitionImageLayout(image.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mMipLevels);   
    copyBufferToImage(stagingBuffer.buffer, image.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    generateMipmaps(image.image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mMipLevels);

    stagingBuffer.destroy();
}

void ke::Graphics::Renderer::createTextureImageView(util::Image& image)
{
    image.imageView = createImageView(image.image, VK_FORMAT_R8G8B8A8_SRGB, mMipLevels, VK_IMAGE_ASPECT_COLOR_BIT);
}

uint32_t ke::Graphics::Renderer::addTextureToDescriptor(const util::Image &image)
{
    static uint32_t rendererIndex = 0;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = image.imageView;
    imageInfo.sampler = mTextureSampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.dstSet = mTextureDescriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = rendererIndex;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);

    return rendererIndex++;
}

void ke::Graphics::Renderer::createFontImage(const std::unordered_map<uint32_t, ke::Graphics::Text::GlyphInfo> &glyphs, const unsigned int ATLAS_SIZE, VkImage& fontImage, VkDeviceMemory& fontImageMemory)
{
    createImage(ATLAS_SIZE, ATLAS_SIZE, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, fontImage, fontImageMemory);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkDeviceSize stagingBufferSize = ATLAS_SIZE * ATLAS_SIZE * 4;

    util::Buffer stagingBuffer(mDevice);
    createBuffer(stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.buffer, stagingBuffer.bufferMemory);

    void* data;
    vkMapMemory(mDevice, stagingBuffer.bufferMemory, 0, stagingBufferSize, 0, &data);
    {
        transitionImageLayout(fontImage, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, commandBuffer);
        VkDeviceSize stagingOffset = 0;
        memset(data, 0, stagingBufferSize);

        for (auto& [codepoint, glyph] : glyphs) 
            for (int row = 0; row < (int)glyph.height; row++) {
                uint8_t* dst = (uint8_t*)data + ((glyph.atlasY + row) * ATLAS_SIZE + glyph.atlasX) * 4;
                
                for (int x = 0; x < (int)glyph.width; x++) {
                    
                    int src = (row * glyph.width + x) * 4;
                    dst[x*4+0] = (uint8_t)(glyph.pixels[src+2] * 255.0f);
                    dst[x*4+1] = (uint8_t)(glyph.pixels[src+1] * 255.0f);
                    dst[x*4+2] = (uint8_t)(glyph.pixels[src+0] * 255.0f);
                    dst[x*4+3] = 255;
                }
            }
        

        VkBufferImageCopy region{};
        region.bufferOffset      = 0;
        region.bufferRowLength   = ATLAS_SIZE;
        region.bufferImageHeight = ATLAS_SIZE;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageSubresource.mipLevel       = 0;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {ATLAS_SIZE, ATLAS_SIZE, 1};
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buffer, fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        transitionImageLayout(fontImage, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, commandBuffer);
    }
    vkUnmapMemory(mDevice, stagingBuffer.bufferMemory);


    endSingleTimeCommands(commandBuffer);
    stagingBuffer.destroy();

}

void ke::Graphics::Renderer::createFontImageView(util::Image &image)
{
    image.imageView = createImageView(image.image, VK_FORMAT_B8G8R8A8_UNORM, 1, VK_IMAGE_ASPECT_COLOR_BIT);
}

uint32_t ke::Graphics::Renderer::addFontToDescriptor(const util::Image &image)
{
    static uint32_t rendererIndex = 0;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = image.imageView;
    imageInfo.sampler = mFontSampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.dstArrayElement = rendererIndex;
    write.dstBinding = 1;
    write.dstSet = mFontDescriptorSet;
    write.pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);

    return rendererIndex++;
}

void ke::Graphics::Renderer::signalWindowResize()
{
    framebufferResized = true;
}

void ke::Graphics::Renderer::createVulkanInstance()
{
    if(enableValidationLayers)
        mLogger.info("Requested validation layers.");
    if(enableValidationLayers && !checkValidationLayerSupport())
        mLogger.critical("Failed to supply requested validation layers!");
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pApplicationName = "Knaj's Engine";
    appInfo.pEngineName = "No Engine";

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    if(enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(gValidationLayers.size());
        createInfo.ppEnabledLayerNames = gValidationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        mLogger.info("No layers were enabled.");
    }
    
    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if(vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
        mLogger.critical("Failed to create a Vulkan instance!");
    mLogger.info("Created Vulkan Instance.");
}

void ke::Graphics::Renderer::pickPhysicalDevice()
{
    uint32_t deviceCount;
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    if(deviceCount == 0)
    {
        mLogger.critical("Failed to find a Graphics Processing Unit with Vulkan support");
        return;
    }
    mLogger.info("Vulkan support found on at least one GPU.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

    std::map<int, VkPhysicalDevice> candidates;
    for(const auto& device : devices)
    {
        candidates.insert(std::make_pair(ratePhysicalDeviceSuitability(device), device));
    }
    

    if(candidates.rbegin()->first > 0)
    {
        mPhysicalDevice = candidates.rbegin()->second;
        mLogger.info("Suitable GPU was found");
    }
    else
        mLogger.critical("Failed to find a suitable GPU!");
    
    VkPhysicalDeviceVulkan12Features v12{};
    v12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &v12;
    
    vkGetPhysicalDeviceFeatures2(mPhysicalDevice, &features2);

    bool supportsBindless = v12.descriptorIndexing && v12.runtimeDescriptorArray && v12.descriptorBindingUpdateUnusedWhilePending && v12.shaderSampledImageArrayNonUniformIndexing && v12.descriptorBindingPartiallyBound && v12.descriptorBindingVariableDescriptorCount && v12.descriptorBindingSampledImageUpdateAfterBind;

    if(supportsBindless)
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);

        USE_BINDLESS_TXT = true;
        MAX_TEXTURES = props.limits.maxPerStageDescriptorSampledImages;
    } else std::cerr << "NO BINDLESS SUPPORT!\n";
}

int ke::Graphics::Renderer::ratePhysicalDeviceSuitability(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device , &deviceFeatures);

    int score = 0;
    if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;

    score += deviceProperties.limits.maxImageDimension2D;

    if(!deviceFeatures.geometryShader) return 0;
    if(!checkDeviceExtensionSupport(device)) return 0;
    if(!deviceFeatures.samplerAnisotropy) return 0;
    
    util::QueueFamilyIndices indices = findQueueFamilyIndices(device);
    if(!indices.isComplete()) return 0;

    return score;
}

void ke::Graphics::Renderer::createLogicalDevice()
{
    float queuePriorities = 1.0f;
    util::QueueFamilyIndices indices = findQueueFamilyIndices(mPhysicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::set<uint32_t> uniqueQueueIndices = {indices.graphicsFamily.value(), indices.presentFamily.value(), indices.transferFamily.value()};

    for(auto index : uniqueQueueIndices)
    {
        VkDeviceQueueCreateInfo createInfo{};
        createInfo.pQueuePriorities = &queuePriorities;
        createInfo.queueCount = 1;
        createInfo.queueFamilyIndex = index;
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos.push_back(createInfo);
    }

    VkPhysicalDeviceVulkan12Features enabled12{};
    enabled12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    if(USE_BINDLESS_TXT)
    {
        enabled12.descriptorIndexing = VK_TRUE;
        enabled12.runtimeDescriptorArray = VK_TRUE;
        enabled12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        enabled12.descriptorBindingPartiallyBound = VK_TRUE;
        enabled12.descriptorBindingVariableDescriptorCount = VK_TRUE;
        enabled12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        enabled12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;

    }
    
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &enabled12;
    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.pEnabledFeatures = nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(gDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = gDeviceExtensions.data();
    
    if(enableValidationLayers)
    {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = gValidationLayers.data();
    }
    else createInfo.enabledLayerCount = 0;

    if(vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS)
        mLogger.critical("Failed to create logical device!");

    vkGetDeviceQueue(mDevice, indices.graphicsFamily.value(), 0, &mGraphicsQueue);
    vkGetDeviceQueue(mDevice, indices.presentFamily.value(), 0, &mPresentQueue);
    vkGetDeviceQueue(mDevice, indices.transferFamily.value(), 0,&mTransferQueue);

    mLogger.info("Created a logical device");
}

void ke::Graphics::Renderer::createWindowSurface(GLFWwindow* window)
{
    if(glfwCreateWindowSurface(mInstance, window, nullptr, &mSurface) != VK_SUCCESS)
        mLogger.error("Failed to create a window surface!");
    
    mLogger.info("Created a window surface.");
}

bool ke::Graphics::Renderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

    std::set<std::string> reqExtensions(gDeviceExtensions.begin(), gDeviceExtensions.end());

    for(const auto& avEx : extensions)
    {
        reqExtensions.erase(avEx.extensionName);
    }

    return reqExtensions.empty();
}

ke::util::QueueFamilyIndices ke::Graphics::Renderer::findQueueFamilyIndices(VkPhysicalDevice device)
{
    util::QueueFamilyIndices indices;

    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

    int i = 0;
    for(const auto& prop : families)
    {
        if(prop.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        if(prop.queueFlags & VK_QUEUE_TRANSFER_BIT)
            indices.transferFamily = i;
         
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface, &presentSupport);

        if(presentSupport) indices.presentFamily = i;

        if(indices.isComplete())
            break;
        i++;
    }

    return indices;
}

ke::util::SwapchainSupportDetails ke::Graphics::Renderer::querySwapchainSupport(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    std::cout << props.deviceName << "\n";

   util::SwapchainSupportDetails swapchainSupport;
   
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, mSurface, &swapchainSupport.surfaceCapabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, nullptr);
    
    if(formatCount > 0)
    {
        swapchainSupport.surfaceFormats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, swapchainSupport.surfaceFormats.data());   
    }
        
    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &modeCount, nullptr);

    if(modeCount > 0)
    {
        swapchainSupport.presentModes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &modeCount, swapchainSupport.presentModes.data());
    }

   return swapchainSupport;
}

VkSurfaceFormatKHR ke::Graphics::Renderer::chooseSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &available)
{
    for(const auto& format : available)
    {
        if(format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }
    mLogger.warn("No suitable surface format found!");
    return available[0];

}

VkPresentModeKHR ke::Graphics::Renderer::chooseSwapchainPresentMode(const std::vector<VkPresentModeKHR> &available)
{
    for(const auto& mode : available)
    {
        if(mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return mode;
    }

    mLogger.warn("Desired present mode not supported, defaulting to FIFO.");

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ke::Graphics::Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &cap, GLFWwindow* window)
{
    if(cap.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return cap.currentExtent;
    
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent =
    {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, cap.minImageExtent.width, cap.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, cap.minImageExtent.height, cap.maxImageExtent.height);

    return actualExtent;
}

void ke::Graphics::Renderer::createSwapchain(GLFWwindow *window)
{
    util::SwapchainSupportDetails support = querySwapchainSupport(mPhysicalDevice);

    for(const auto& presentMode : support.presentModes)
    {
        if(presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            mLogger.warn("Mailbox found on device!");
    }
    VkSurfaceFormatKHR surfaceFormat = chooseSwapchainSurfaceFormat(support.surfaceFormats);
    VkPresentModeKHR presentMode = chooseSwapchainPresentMode(support.presentModes);
    VkExtent2D extent = chooseSwapExtent(support.surfaceCapabilities, window);

    uint32_t imageCount = support.surfaceCapabilities.minImageCount + 1;

    if(support.surfaceCapabilities.maxImageCount > 0 && imageCount > support.surfaceCapabilities.maxImageCount)
        imageCount = support.surfaceCapabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = mSurface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.presentMode = presentMode;
    createInfo.imageExtent = extent;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    util::QueueFamilyIndices indices = findQueueFamilyIndices(mPhysicalDevice);

    uint32_t familyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if(indices.graphicsFamily.value() == indices.presentFamily.value())
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = familyIndices;
    }

    createInfo.preTransform = support.surfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if(vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapchain) != VK_SUCCESS)
        mLogger.critical("Failed to create a swapchain!");


    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, nullptr);
    mSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, mSwapchainImages.data());

    mSwapchainExtent = extent;
    mSwapchainFormat = surfaceFormat.format;
    
    mLogger.info("Created Swapchain");
}

void ke::Graphics::Renderer::createSwapchainImageViews()
{
    mSwapchainImageViews.resize(mSwapchainImages.size());

        

    for(size_t i = 0; i < mSwapchainImages.size(); i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.format = mSwapchainFormat;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.image = mSwapchainImages[i];

        if(vkCreateImageView(mDevice, &createInfo, nullptr, &mSwapchainImageViews[i]) != VK_SUCCESS)
            mLogger.error("Failed to create an image view!");
    }

    mLogger.info("Created swapchain image views");
}

void ke::Graphics::Renderer::recreateSwapchain(GLFWwindow* window)
{
    vkDeviceWaitIdle(mDevice);
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    } 
    

    cleanupSwapchain();

    createSwapchain(window);
    createSwapchainImageViews();
    createFramebuffers();
}

void ke::Graphics::Renderer::cleanupSwapchain()
{
    for(auto fb : mSwapchainFramebuffers)
        vkDestroyFramebuffer(mDevice, fb, nullptr);
    
    for(auto view : mSwapchainImageViews)
        vkDestroyImageView(mDevice, view, nullptr);
    
    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
}

void ke::Graphics::Renderer::createGraphicsPipeline()
{
    auto vertexCode = ke::util::readFile("./shader/bin/vert.spv");
    auto fragCode = ke::util::readFile("./shader/bin/frag.spv");

    VkShaderModule vertexModule = createShaderModule(vertexCode);
    VkShaderModule fragmentModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertexModule;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragmentModule;
    fragStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo stageInfos[] = {vertStageInfo, fragStageInfo};

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates  = dynamicStates.data();

    VkVertexInputBindingDescription binding = util::str::Vertex2P3C2T::getInputBindingDescription();
    std::array<VkVertexInputAttributeDescription, 3> attributes = util::str::Vertex2P3C2T::getInputAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions = attributes.data();
    vertexInput.pVertexBindingDescriptions = &binding;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) mSwapchainExtent.width;
    viewport.height = (float) mSwapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = mSwapchainExtent;
    scissor.offset = {0,0};

    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.scissorCount = 1;
    viewportInfo.viewportCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorAtt{};
    colorAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorAtt.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorAtt;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkDescriptorSetLayout dLayouts[] = {mTextureSetLayout, mDescriptorSetLayout};

    VkPushConstantRange pushRange{};
    pushRange.size = sizeof(int32_t);
    pushRange.offset = 0;
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = dLayouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    
    if(vkCreatePipelineLayout(mDevice, &layoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
        mLogger.error("Failed to create a pipeline layout!");


    auto sceneVertexCode = ke::util::readFile("./shader/bin/scenevert.spv");
    auto sceneFragmentCode = ke::util::readFile("./shader/bin/scenefrag.spv");

    VkShaderModule sceneVertexModule = createShaderModule(sceneVertexCode);
    VkShaderModule sceneFragmentModule = createShaderModule(sceneFragmentCode);

    VkPipelineShaderStageCreateInfo sceneVertexStage{};
    sceneVertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    sceneVertexStage.module = sceneVertexModule;
    sceneVertexStage.pName = "main";
    sceneVertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineShaderStageCreateInfo sceneFragmentStage{};
    sceneFragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    sceneFragmentStage.module = sceneFragmentModule;
    sceneFragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    sceneFragmentStage.pName = "main";

    VkPipelineShaderStageCreateInfo sceneStages[] = {sceneVertexStage, sceneFragmentStage};

    VkVertexInputBindingDescription sceneBindingDescription = util::str::Vertex3P3C2T::getInputBindingDescription();
    std::array<VkVertexInputAttributeDescription, 3> sceneVertexAttribs = util::str::Vertex3P3C2T::getInputAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo sceneVertexInput{};
    sceneVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    sceneVertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(sceneVertexAttribs.size());
    sceneVertexInput.vertexBindingDescriptionCount = 1;
    sceneVertexInput.pVertexAttributeDescriptions = sceneVertexAttribs.data();
    sceneVertexInput.pVertexBindingDescriptions = &sceneBindingDescription;

    VkPipelineDepthStencilStateCreateInfo sceneDepthState{};
    sceneDepthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    sceneDepthState.depthTestEnable = VK_TRUE;
    sceneDepthState.depthWriteEnable = VK_TRUE;
    sceneDepthState.depthCompareOp = VK_COMPARE_OP_LESS;
    sceneDepthState.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stageInfos;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = mPipelineLayout;
    pipelineInfo.renderPass = mRenderPass;
    pipelineInfo.subpass = 0;

    if(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline) != VK_SUCCESS)
        mLogger.critical("Failed to create a graphics pipeline!");
    mLogger.info("Created UI pipeline!");

    pipelineInfo.pStages = sceneStages;
    pipelineInfo.pVertexInputState = &sceneVertexInput;
    pipelineInfo.pDepthStencilState = &sceneDepthState;
    pipelineInfo.renderPass = mSceneRenderPass;

    if(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mDisplayPipeline) != VK_SUCCESS)
        mLogger.critical("Failed to create a second pipeline!");
    mLogger.info("Created scene pipeline!");

    vkDestroyShaderModule(mDevice, vertexModule, nullptr);
    vkDestroyShaderModule(mDevice, fragmentModule, nullptr);

    vkDestroyShaderModule(mDevice, sceneVertexModule, nullptr);
    vkDestroyShaderModule(mDevice, sceneFragmentModule, nullptr);
    
    mLogger.info("Created graphics pipeline!");
}

void ke::Graphics::Renderer::createFontPipeline()
{
    auto vertexCode = ke::util::readFile("shader/bin/textvert.spv");
    auto fragCode = ke::util::readFile("shader/bin/textfrag.spv");
    
    VkShaderModule vertexModule = createShaderModule(vertexCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertexStageCreateInfo{};
    vertexStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStageCreateInfo.module = vertexModule;
    vertexStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentStageCreateInfo{};
    fragmentStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStageCreateInfo.module = fragModule;
    fragmentStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo stageInfos[] = {vertexStageCreateInfo, fragmentStageCreateInfo};

    std::array<VkVertexInputAttributeDescription, 4> glyphInstanceAttributeDescriptions = Text::GlyphInstance::getInputAttributeDescriptions();
    VkVertexInputBindingDescription glyphInstanceBindingDescription = Text::GlyphInstance::getInputBindingDescription();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t> (glyphInstanceAttributeDescriptions.size());
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions = glyphInstanceAttributeDescriptions.data();
    vertexInput.pVertexBindingDescriptions = &glyphInstanceBindingDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)mSwapchainExtent.width;
    viewport.height = (float)mSwapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = mSwapchainExtent;
    scissor.offset = {0,0};
    
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.scissorCount = 1;
    viewportState.viewportCount = 1;
    viewportState.pScissors = &scissor;
    viewportState.pViewports = &viewport;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorAtt{};
    colorAtt.blendEnable = VK_TRUE;
    colorAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorAtt.colorBlendOp = VK_BLEND_OP_ADD;
    colorAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorAtt.alphaBlendOp = VK_BLEND_OP_ADD;
    colorAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorAtt;
    colorBlending.logicOpEnable = VK_FALSE;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkDescriptorSetLayout setLayouts[] = {mFontSetLayout};

    VkPushConstantRange pcRange{};
    pcRange.offset = 0;
    pcRange.size = sizeof(int);
    pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pcRange;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = setLayouts;

    if(vkCreatePipelineLayout(mDevice, &layoutInfo, nullptr, &mFontPipelineLayout) != VK_SUCCESS)
        mLogger.error("Failed to create font pipeline layout.");


    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stageInfos;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = mFontPipelineLayout;
    pipelineInfo.renderPass = mFontRenderPass;
    pipelineInfo.subpass = 0;

    if(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mFontPipeline) != VK_SUCCESS)
        mLogger.error("Failed to create font pipeline.");
    
    
    
    vkDestroyShaderModule(mDevice, vertexModule, nullptr);
    vkDestroyShaderModule(mDevice, fragModule, nullptr);
}

VkShaderModule ke::Graphics::Renderer::createShaderModule(const std::vector<char> &code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    if(vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        mLogger.error("Failed to create shader module!");

    return shaderModule;
}

void ke::Graphics::Renderer::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = mSwapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{}; 
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dep;

    if(vkCreateRenderPass(mDevice, &createInfo, nullptr, &mRenderPass) != VK_SUCCESS)
        mLogger.error("Failed to create render pass!");

    mLogger.info("Created render pass.");
}

void ke::Graphics::Renderer::createFontRenderPass()
{
    VkAttachmentDescription colorAtt{};
    colorAtt.format = mSwapchainFormat;
    colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference attRef{};
    attRef.attachment = 0;
    attRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAtt, depthAttachment};

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dep;

    if(vkCreateRenderPass(mDevice, &createInfo, nullptr, &mFontRenderPass) != VK_SUCCESS)
        mLogger.error("Failed to create font render pass.");
    
}

void ke::Graphics::Renderer::createSceneRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = mSwapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{}; 
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dep;

    if(vkCreateRenderPass(mDevice, &createInfo, nullptr, &mSceneRenderPass) != VK_SUCCESS)
        mLogger.error("Failed to create render pass!");

    mLogger.info("Created render pass.");
}

void ke::Graphics::Renderer::createFramebuffers()
{
    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());

    for(size_t i = 0; i < mSwapchainImageViews.size(); i++)
    {
        std::array<VkImageView, 2> attachments = {mSwapchainImageViews[i], mDepthImage.imageView};

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = mRenderPass;
        createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.height = mSwapchainExtent.height;
        createInfo.width = mSwapchainExtent.width;
        createInfo.layers = 1;

        if(vkCreateFramebuffer(mDevice, &createInfo, nullptr, &mSwapchainFramebuffers[i]) != VK_SUCCESS)
            mLogger.error("Failed to create framebuffer!");
        mLogger.info("Created framebuffer.");
    }

}

void ke::Graphics::Renderer::createCommandPool()
{
    util::QueueFamilyIndices indices = findQueueFamilyIndices(mPhysicalDevice);

    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = indices.graphicsFamily.value();

    if(vkCreateCommandPool(mDevice, &createInfo, nullptr, &mCommandPool) != VK_SUCCESS)
        mLogger.critical("Failed to create a command pool!");
    mLogger.info("Created command pool.");
}

void ke::Graphics::Renderer::createCommandBuffer()
{
    mCommandBuffers.resize(MAXFRAMESINFLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = (uint32_t) mCommandBuffers.size();
    allocInfo.commandPool = mCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if(vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS)
        mLogger.critical("Failed to allocate command buffers!");
    
    mLogger.info("Created command buffers.");
}

void ke::Graphics::Renderer::createSyncObjects()
{
    mImageAvailableSemaphores.resize(MAXFRAMESINFLIGHT);
    mRenderFinishedSemaphores.resize(mSwapchainImages.size());
    mInFlightFences.resize(MAXFRAMESINFLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(int i = 0; i < MAXFRAMESINFLIGHT; i++)
    {
        if(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
        mLogger.error("Failed to create at least one sync object!");
    }
    for(size_t i = 0; i < mSwapchainImages.size(); i++)
    {
        if(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS)
            mLogger.error("Failed to create render finished semaphore!");
    }
    mImagesInFlight = std::vector<VkFence>(mSwapchainImages.size(), VK_NULL_HANDLE);

    mLogger.info("Created sync objects.");
}

void ke::Graphics::Renderer::beginRecording(VkCommandBuffer buffer)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if(vkBeginCommandBuffer(buffer, &beginInfo) != VK_SUCCESS)
        mLogger.error("Failed to begin recording command buffer!");

    VkRenderPassBeginInfo renderBegin{};
    renderBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderBegin.renderPass = mRenderPass;
    renderBegin.framebuffer = mSwapchainFramebuffers[currentImageIndex];
    renderBegin.renderArea.offset = {0,0};
    renderBegin.renderArea.extent = mSwapchainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderBegin.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderBegin.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(buffer, &renderBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

    VkViewport viewport{};
    viewport.height = mSwapchainExtent.height;
    viewport.width = mSwapchainExtent.width;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    vkCmdSetViewport(buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = mSwapchainExtent;
    scissor.offset = {0, 0};
    vkCmdSetScissor(buffer, 0, 1, &scissor);
}

void ke::Graphics::Renderer::endRecording(VkCommandBuffer buffer)
{
    if(vkEndCommandBuffer(buffer) != VK_SUCCESS)
        mLogger.error("Failed to record command buffer!");
}

void ke::Graphics::Renderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout srcLayout, VkImageLayout dstLayout, uint32_t mipLevels, VkCommandBuffer targetCommandBuffer)
{

    VkCommandBuffer commandBuffer = (targetCommandBuffer == VK_NULL_HANDLE) ? beginSingleTimeCommands() : targetCommandBuffer;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = srcLayout;
    barrier.newLayout = dstLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if(srcLayout == VK_IMAGE_LAYOUT_UNDEFINED && dstLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if(srcLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && dstLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else mLogger.warn("Unsupported layout transition requested.");

    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    if(targetCommandBuffer == VK_NULL_HANDLE) endSingleTimeCommands(commandBuffer);
}

void ke::Graphics::Renderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageSubresource.mipLevel = 0;

    region.imageOffset = {0,0,0};
    region.imageExtent = {width, height, 1};    

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer ke::Graphics::Renderer::beginSingleTimeCommands()
{
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = 1;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = mCommandPool;

    vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void ke::Graphics::Renderer::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
   vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(mGraphicsQueue);

    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
}

void ke::Graphics::Renderer::createIndexBuffer(const std::vector<uint32_t>& indices, VkBuffer& targetBuffer, VkDeviceMemory& targetMemory)
{
    VkDeviceSize size = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(mDevice, stagingBufferMemory, 0, size, 0, &data);
        memcpy(data, indices.data(), (size_t) size);
    vkUnmapMemory(mDevice, stagingBufferMemory);

    createBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, targetBuffer, targetMemory);

    copyBuffer(stagingBuffer, targetBuffer, size);

    vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
    vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
}

void ke::Graphics::Renderer::createGlyphInstanceBuffer(const std::vector<Text::GlyphInstance> &instances, VkBuffer &targetBuffer, VkDeviceMemory &targetMemory)
{
    VkDeviceSize size = sizeof(instances[0]) * instances.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(mDevice, stagingBufferMemory, 0, size, 0, &data);
        memcpy(data, instances.data(), size);
    vkUnmapMemory(mDevice, stagingBufferMemory);

    createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, targetBuffer, targetMemory);

    copyBuffer(stagingBuffer, targetBuffer, size);

    vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
    vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
}

void ke::Graphics::Renderer::endRenderPass()
{
    vkCmdEndRenderPass(mCommandBuffers[currentFrameInFlight]);
}

void ke::Graphics::Renderer::bindUIPipeline(VkCommandBuffer buffer)
{
    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

    vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 1,1, &mUIDescriptorSets[currentFrameInFlight], 0, nullptr);

    VkViewport viewport{};
    viewport.height = mSwapchainExtent.height;
    viewport.width = mSwapchainExtent.width;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    viewport.x = 0.0f;
    viewport.y = 0.0f;

    VkRect2D scissor{};
    scissor.extent = mSwapchainExtent;
    scissor.offset = {0, 0};

    vkCmdSetViewport(buffer, 0, 1, &viewport);
    vkCmdSetScissor(buffer, 0, 1, &scissor);
}

void ke::Graphics::Renderer::bindScenePipeline(VkCommandBuffer buffer, const VkViewport& viewport, const VkRect2D& scissor)
{
    VkRenderPassBeginInfo renderBegin{};
    renderBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderBegin.renderPass = mSceneRenderPass;
    renderBegin.framebuffer = mSwapchainFramebuffers[currentImageIndex];
    renderBegin.renderArea.offset = {0,0};
    renderBegin.renderArea.extent = mSwapchainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f , 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderBegin.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderBegin.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(buffer, &renderBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mDisplayPipeline);
    vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 1, 1, &mSceneDescriptorSets[currentFrameInFlight], 0, nullptr);

    vkCmdSetViewport(buffer, 0, 1, &viewport);
    vkCmdSetScissor(buffer, 0, 1, &scissor);
}

void ke::Graphics::Renderer::bindFontPipeline(VkCommandBuffer buffer)
{
    VkRenderPassBeginInfo renderBegin{};
    renderBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderBegin.renderPass = mFontRenderPass;
    renderBegin.framebuffer = mSwapchainFramebuffers[currentImageIndex];
    renderBegin.renderArea.offset = {0,0};
    renderBegin.renderArea.extent = mSwapchainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderBegin.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderBegin.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(buffer, &renderBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mFontPipeline);
    vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mFontPipelineLayout, 0, 1, &mFontDescriptorSet, 0, nullptr);

    VkViewport viewport{};
    viewport.height = mSwapchainExtent.height;
    viewport.width = mSwapchainExtent.width;
    viewport.maxDepth = 1.0f;
    viewport.minDepth = 0.0f;
    viewport.x = 0.0f;
    viewport.y = 0.0f;

    VkRect2D scissor{};
    scissor.extent = mSwapchainExtent;
    scissor.offset = {0, 0};

    vkCmdSetViewport(buffer, 0, 1, &viewport);
    vkCmdSetScissor(buffer, 0, 1, &scissor);
}

void ke::Graphics::Renderer::pickTextureIndex(int32_t index) const
{
    vkCmdPushConstants(mCommandBuffers[currentFrameInFlight], mPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int32_t), &index);
}

void ke::Graphics::Renderer::pickFontIndex(int32_t index) const
{
    vkCmdPushConstants(mCommandBuffers[currentFrameInFlight], mFontPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int32_t), &index);
}

void ke::Graphics::Renderer::drawBuffersIndexed(const util::Buffer& vertexBuffer, const util::Buffer& indexBuffer, uint32_t indexCount) const
{
    assert(vertexBuffer.buffer != VK_NULL_HANDLE);
    assert(indexBuffer.buffer != VK_NULL_HANDLE);

    VkCommandBuffer commandBuffer = mCommandBuffers[currentFrameInFlight];

    VkDeviceSize offsets[] = {0};

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);

    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
}

void ke::Graphics::Renderer::drawText(const util::Buffer& instanceBuffer, uint32_t instanceCount) const
{
    VkCommandBuffer commandBuffer = mCommandBuffers[currentFrameInFlight];

    VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &instanceBuffer.buffer, offsets);
    vkCmdDraw(commandBuffer, 6, instanceCount, 0, 0);
}

VkDevice ke::Graphics::Renderer::getDevice() const
{
    return mDevice;
}

uint32_t ke::Graphics::Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProp;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProp);

    for(uint32_t i = 0; i < memProp.memoryTypeCount; i++)
    {
        if((typeFilter & (1 << i)) && (memProp.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    mLogger.error("Failed to find suitable memory type!");
    return -1;
}

void ke::Graphics::Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory)
{
    auto queueFamilyIndices = findQueueFamilyIndices(mPhysicalDevice);

    std::set<uint32_t> uniqueIndices = {queueFamilyIndices.graphicsFamily.value(), queueFamilyIndices.transferFamily.value()};

    std::vector<uint32_t> indices(uniqueIndices.begin(), uniqueIndices.end());

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    if(uniqueIndices.size() != 1)
    {
        bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
        bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(indices.size());
        bufferInfo.pQueueFamilyIndices = indices.data();
    }
    else bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateBuffer(mDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        mLogger.error("Failed to create buffer!");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(mDevice, buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    allocInfo.allocationSize = memReq.size;

    if(allocInfo.allocationSize < memReq.size)
        mLogger.error("Insufficient memory being allocated!");

    if(vkAllocateMemory(mDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        mLogger.error("Failed to allocate buffer memory!");
    
    vkBindBufferMemory(mDevice, buffer, bufferMemory, 0);

}

void ke::Graphics::Renderer::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &memory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.extent.width = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth = 1;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.arrayLayers = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if(vkCreateImage(mDevice, &imageInfo, nullptr, &image) != VK_SUCCESS)
        mLogger.error("Failed to create texture image!");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(mDevice, image, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if(vkAllocateMemory(mDevice, &allocInfo, nullptr, &memory) != VK_SUCCESS)
        mLogger.error("Failed to allocate texture image memory!");

    vkBindImageMemory(mDevice, image, memory, 0);
}

VkImageView ke::Graphics::Renderer::createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;

    VkImageView imageView;
    if(vkCreateImageView(mDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        mLogger.error("Failed to create image view!");

    return imageView;
}

void ke::Graphics::Renderer::generateMipmaps(VkImage image, VkFormat format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
    VkFormatProperties formatProp{};
    vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &formatProp);
    if(!(formatProp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
        mLogger.error("Texture image format does not support linear blitting!");

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int mipWidth = texWidth, mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,0, nullptr, 0, nullptr, 1, &barrier);

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer ke::Graphics::Renderer::getCurrentCommandBuffer()
{
    return mCommandBuffers[currentFrameInFlight];
}

void ke::Graphics::Renderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    // Maybe separate cpool?
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = mCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkBufferCopy copyRegion{};
    copyRegion.size = size;

    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(mTransferQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(mTransferQueue);

    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
}

void ke::Graphics::Renderer::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if(vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS)
        mLogger.error("Failed to create descriptor set layout.");


    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 4096;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagsInfo.bindingCount = 1;
    flagsInfo.pBindingFlags = &bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo2{};
    layoutInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo2.pNext = &flagsInfo;
    layoutInfo2.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo2.bindingCount = 1;
    layoutInfo2.pBindings = &samplerLayoutBinding;

    if(vkCreateDescriptorSetLayout(mDevice, &layoutInfo2, nullptr, &mTextureSetLayout) != VK_SUCCESS)
        mLogger.error("Failed to create texture set layout.");

    VkDescriptorBindingFlags fontBindingFlags[] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, 0};

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo2{};
    flagsInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagsInfo2.bindingCount = 2;
    flagsInfo2.pBindingFlags = fontBindingFlags;

    VkDescriptorSetLayoutBinding fontSamplerLayoutBinding{};
    fontSamplerLayoutBinding.binding = 1;
    fontSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    fontSamplerLayoutBinding.descriptorCount = 64;
    fontSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    fontSamplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding fontResolutionLayoutBinding{};
    fontResolutionLayoutBinding.binding = 0;
    fontResolutionLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    fontResolutionLayoutBinding.descriptorCount = 1;
    fontResolutionLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    fontResolutionLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding bindings[] = {fontSamplerLayoutBinding, fontResolutionLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo3{};
    layoutInfo3.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo3.pNext = &flagsInfo2;
    layoutInfo3.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo3.bindingCount = 2;
    layoutInfo3.pBindings = bindings;

    if(vkCreateDescriptorSetLayout(mDevice, &layoutInfo3, nullptr, &mFontSetLayout) != VK_SUCCESS)
        mLogger.error("Failed to create font set layout.");
    
    mLogger.info("Created descriptor set layout.");
}

void ke::Graphics::Renderer::createUniformBuffers()
{
    VkDeviceSize size = sizeof(util::UniformBufferObject);

    uniformBuffers.resize(MAXFRAMESINFLIGHT);
    uniformBuffersMemory.resize(MAXFRAMESINFLIGHT);
    uniformBuffersMapped.resize(MAXFRAMESINFLIGHT);

    sceneUniformBuffers.resize(MAXFRAMESINFLIGHT);
    sceneUniformBuffersMemory.resize(MAXFRAMESINFLIGHT);
    sceneUniformBuffersMapped.resize(MAXFRAMESINFLIGHT);

    glm::vec2 extentVec = {mSwapchainExtent.width, mSwapchainExtent.height};

    for(unsigned int i = 0; i < MAXFRAMESINFLIGHT; i++)
    {
        createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
        vkMapMemory(mDevice, uniformBuffersMemory[i], 0, size, 0, &uniformBuffersMapped[i]);

        createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sceneUniformBuffers[i], sceneUniformBuffersMemory[i]);
        vkMapMemory(mDevice, sceneUniformBuffersMemory[i], 0, size, 0, &sceneUniformBuffersMapped[i]);
    }

    createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, fontUniformBuffer, fontUniformBufferMemory);
    vkMapMemory(mDevice, fontUniformBufferMemory, 0, sizeof(glm::vec2), 0, &fontUniformBufferMapped);

    memcpy(fontUniformBufferMapped, &extentVec, sizeof(glm::vec2));
    
}

void ke::Graphics::Renderer::createDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAXFRAMESINFLIGHT)*2;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = MAX_TEXTURES;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = 64;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.maxSets = static_cast<uint32_t>(MAXFRAMESINFLIGHT) * 2 + 2;

    if(vkCreateDescriptorPool(mDevice, &createInfo, nullptr, &mDescriptorPool) != VK_SUCCESS)
        mLogger.error("Failed to create descritpor pool!");
    
    mLogger.info("Created descriptor pool.");
}

void ke::Graphics::Renderer::createDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(MAXFRAMESINFLIGHT, mDescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAXFRAMESINFLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    uint32_t descriptorCountArray[] = {4096};

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCount{};
    variableDescriptorCount.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCount.pDescriptorCounts = descriptorCountArray;
    variableDescriptorCount.descriptorSetCount = 1;

    VkDescriptorSetAllocateInfo tAllocInfo{};
    tAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    tAllocInfo.descriptorPool = mDescriptorPool;
    tAllocInfo.pSetLayouts = &mTextureSetLayout;
    tAllocInfo.descriptorSetCount = 1;
    tAllocInfo.pNext = &variableDescriptorCount;

    uint32_t fontDescriptorCountArray[] = {64};

    VkDescriptorSetVariableDescriptorCountAllocateInfo fontVariableDescriptorCount{};
    fontVariableDescriptorCount.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    fontVariableDescriptorCount.pDescriptorCounts = fontDescriptorCountArray;
    fontVariableDescriptorCount.descriptorSetCount = 1;

    VkDescriptorSetAllocateInfo fAllocInfo{};
    fAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    fAllocInfo.descriptorPool = mDescriptorPool;
    fAllocInfo.pSetLayouts = &mFontSetLayout;
    fAllocInfo.descriptorSetCount = 1;
    fAllocInfo.pNext = &fontVariableDescriptorCount;

    mUIDescriptorSets.resize(MAXFRAMESINFLIGHT);
    mSceneDescriptorSets.resize(MAXFRAMESINFLIGHT);
    if(vkAllocateDescriptorSets(mDevice, &allocInfo, mUIDescriptorSets.data()) != VK_SUCCESS)
        mLogger.error("Failed to allocate descriptor sets!");
    
    if(vkAllocateDescriptorSets(mDevice, &allocInfo, mSceneDescriptorSets.data()) != VK_SUCCESS)
        mLogger.error("Failed to allocate Scene descriptor sets");

    if(vkAllocateDescriptorSets(mDevice, &tAllocInfo, &mTextureDescriptorSet) != VK_SUCCESS)
        mLogger.error("Failed to allocate texture descriptor set.");
    
    if(vkAllocateDescriptorSets(mDevice, &fAllocInfo, &mFontDescriptorSet) != VK_SUCCESS)
        mLogger.error("Failed to allocate font descriptor set.");
    
    mLogger.info("Allocated descriptor sets.");

    for(size_t i = 0; i < MAXFRAMESINFLIGHT; i++)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(util::UniformBufferObject);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = mUIDescriptorSets[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);
    }

    for(size_t i = 0; i < MAXFRAMESINFLIGHT; i++)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = sceneUniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(util::UniformBufferObject);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = mSceneDescriptorSets[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);
    }


    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = fontUniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(glm::vec2);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = mFontDescriptorSet;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(mDevice, 1, & write, 0, nullptr);
    
}

void ke::Graphics::Renderer::createDepthResources()
{
    mDepthImage.setDevice(mDevice);
    VkFormat depthFormat = findDepthFormat();

    createImage(mSwapchainExtent.width, mSwapchainExtent.height, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mDepthImage.image, mDepthImage.imageMemory);
    mDepthImage.imageView = createImageView(mDepthImage.image, depthFormat, 1, VK_IMAGE_ASPECT_DEPTH_BIT);

}

VkFormat ke::Graphics::Renderer::findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for(VkFormat format : candidates)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &props);

        if(tiling == VK_IMAGE_TILING_LINEAR && (props.optimalTilingFeatures & features) == features)
            return format;
        else if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
        
        mLogger.error("Failed to find supported format!");
    }
}

VkFormat ke::Graphics::Renderer::findDepthFormat()
{
    return findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool ke::Graphics::Renderer::hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void ke::Graphics::Renderer::createTextureSampler()
{
    VkSamplerCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter = VK_FILTER_LINEAR;
    createInfo.minFilter = VK_FILTER_LINEAR;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.anisotropyEnable = VK_TRUE;
    createInfo.maxAnisotropy = 1.0f;
    createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;
    createInfo.compareEnable = VK_FALSE;
    createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.mipLodBias = 0.0f;
    createInfo.minLod = 0.0f;
    createInfo.maxLod = VK_LOD_CLAMP_NONE;

    if(vkCreateSampler(mDevice, &createInfo, nullptr, &mTextureSampler) != VK_SUCCESS)
        mLogger.error("Failed to create texture sampler!");
}

void ke::Graphics::Renderer::createFontSampler()
{
    VkSamplerCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter = VK_FILTER_LINEAR;
    createInfo.minFilter = VK_FILTER_LINEAR;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createInfo.anisotropyEnable = VK_FALSE;
    createInfo.maxAnisotropy = 0.0f;
    createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;
    createInfo.compareEnable = VK_FALSE;
    createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.mipLodBias = 0.0f;
    createInfo.minLod = 0.0f;
    createInfo.maxLod = VK_LOD_CLAMP_NONE;

    if(vkCreateSampler(mDevice, &createInfo, nullptr, &mFontSampler) != VK_SUCCESS)
        mLogger.error("Failed to create font sampler!");
}

bool ke::Graphics::Renderer::checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    for(const char* layerName : gValidationLayers)
    {
        bool layerFound = false;

        for(const auto& lprop : layers)
        {
            if(strcmp(layerName, lprop.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if(!layerFound) return false;
    }

    return true;
}

void ke::Graphics::Renderer::setupDebugMessenger()
{
    if(!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;

    if(CreateDebugUtilsMessenger(mInstance, &createInfo, nullptr, &mDebugMessenger) != VK_SUCCESS)
        mLogger.error("Failed to set up debug utils messenger!");
    mLogger.info("Setup debug utils messenger.");
}

void ke::Graphics::Renderer::DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator)
{  
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VkResult ke::Graphics::Renderer::CreateDebugUtilsMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

std::vector<const char *> ke::Graphics::Renderer::getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if(enableValidationLayers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    if(messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) return VK_FALSE;
    std::cerr << "Validation Layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}