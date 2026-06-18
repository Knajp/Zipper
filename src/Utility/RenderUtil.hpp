#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <fstream>
#include <glm/glm.hpp>
#include <utility>
#include <iostream>

namespace ke
{
    namespace util 
    {
        struct QueueFamilyIndices
        {
            std::optional<uint32_t> graphicsFamily;
            std::optional<uint32_t> presentFamily;
            std::optional<uint32_t> transferFamily;

            bool isComplete()
            {
                return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
            }
        };

        struct SwapchainSupportDetails
        {
            VkSurfaceCapabilitiesKHR surfaceCapabilities;
            std::vector<VkSurfaceFormatKHR> surfaceFormats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        static std::vector<char> readFile(const std::string& filename)
        {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if(!file.is_open())
                throw std::runtime_error("Failed to open file for read!");

            size_t fileSize = (size_t) file.tellg();
            std::vector<char> buffer(fileSize);

            file.seekg(0);
            file.read(buffer.data(), fileSize);

            file.close();

            return buffer;
        }

        static float srgbToLinear(float c)
        {return powf(c, 2.2f);}

        struct UniformBufferObject
        {
            glm::mat4 model;
            glm::mat4 view;
            glm::mat4 proj;
        };

        struct Image
        {
            VkImage image;
            VkImageView imageView;
            VkDeviceMemory imageMemory;

            VkDevice device;

            Image() = default;
            void setDevice(VkDevice _device)
            {
                device = _device;
            }

            void destroy()
            {
                if(image == VK_NULL_HANDLE || imageView == VK_NULL_HANDLE || imageMemory == VK_NULL_HANDLE) return;

                assert(device != VK_NULL_HANDLE);

                vkDeviceWaitIdle(device);

                vkDestroyImageView(device, imageView, nullptr);
                vkDestroyImage(device, image, nullptr);
                vkFreeMemory(device, imageMemory, nullptr);

                device = VK_NULL_HANDLE;
                image = VK_NULL_HANDLE;
                imageView = VK_NULL_HANDLE;
                imageMemory = VK_NULL_HANDLE;
            }
            ~Image()
            {
                destroy();
            }

            Image(Image&& other)
                :image(std::exchange(other.image, VK_NULL_HANDLE)),
                 imageView(std::exchange(other.imageView, VK_NULL_HANDLE)),
                 imageMemory(std::exchange(other.imageMemory, VK_NULL_HANDLE)),
                 device(other.device){}

            Image& operator=(Image&& other)
            {
                if(this == &other) return *this;
                
                if(image != VK_NULL_HANDLE && device != VK_NULL_HANDLE)
                {
                    destroy();
                }   

                image = VK_NULL_HANDLE;
                imageView = VK_NULL_HANDLE;
                imageMemory = VK_NULL_HANDLE;

                image = std::exchange(other.image, VK_NULL_HANDLE);
                imageView = std::exchange(other.imageView, VK_NULL_HANDLE);
                imageMemory = std::exchange(other.imageMemory, VK_NULL_HANDLE);
                device = other.device;

                return *this;
            }
                
            Image(const Image& other) = delete;
            Image& operator=(const Image& other) = delete;
        };
        
 
        struct Buffer
        {
            VkBuffer buffer = VK_NULL_HANDLE;
            VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
            VkDevice device = VK_NULL_HANDLE;

            Buffer() = default;
            Buffer(VkDevice _device) 
                : device(_device){}

            void destroy()
            {
                if(buffer == VK_NULL_HANDLE) return;
                if(device == VK_NULL_HANDLE) std::cout << "VULKAN LOGICAL DEVICE IS NULL ON BUFFER DESTRUCTION!\n";

                vkDeviceWaitIdle(device);

                vkDestroyBuffer(device, buffer, nullptr);
                vkFreeMemory(device, bufferMemory, nullptr);

                buffer = VK_NULL_HANDLE;
                bufferMemory = VK_NULL_HANDLE;
                device = VK_NULL_HANDLE;
            }
            ~Buffer()
            {
                destroy();
            }

            Buffer(Buffer&& other)
                : buffer(std::exchange(other.buffer, VK_NULL_HANDLE)),
                  bufferMemory(std::exchange(other.bufferMemory, VK_NULL_HANDLE)),
                  device(other.device){}

            Buffer& operator=(Buffer&& other)
            {
                if(this == &other) return *this;

                if(buffer != VK_NULL_HANDLE && device != VK_NULL_HANDLE)
                {
                    vkDeviceWaitIdle(device);

                    vkDestroyBuffer(device, buffer, nullptr);
                    vkFreeMemory(device, bufferMemory, nullptr);
                }

                buffer = VK_NULL_HANDLE;
                bufferMemory = VK_NULL_HANDLE;

                buffer = std::exchange(other.buffer, VK_NULL_HANDLE);
                bufferMemory = std::exchange(other.bufferMemory, VK_NULL_HANDLE);
                device = other.device;

                return *this;
            }

            Buffer(const Buffer& other) = delete;
            Buffer& operator=(const Buffer& other) = delete;

            void setDevice(VkDevice _device)
            {
                device = _device;
            }
        };
    }
}