/*
 * Copyright (c) 2023 MotionSpell
 * Licensed under the License terms and conditions for use, reproduction,
 * and distribution of 5GMAG software (the “License”).
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at https://www.5g-mag.com/license .
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.
 */

// Vulkan (Android): the AHardwareBuffers of native frames are imported as Vulkan images, sampled through a
// VkSamplerYcbcrConversion (YCbCr pictures, including the formats of the device), and rendered into the target texture
// of each Unity texture, converted to RGB. Unity can not sample the formats of the device itself.

#include "Backend.hpp"

#ifdef __ANDROID__

#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <vulkan/vulkan.h>

#include <android/hardware_buffer.h>

#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>

#include "IUnityGraphics.h"
#include "IUnityGraphicsVulkan.h"
#include "MafUnity.h"
#include "VulkanShaders.h"

namespace
{

    // ---- Device extensions: added when Unity creates its device ------------------------------------------------------

    const char *const kDeviceExtensions[] = {
        VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
        VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
    };

    PFN_vkGetInstanceProcAddr s_getInstanceProcAddr = nullptr;
    PFN_vkCreateDevice s_createDevice = nullptr;
    VkInstance s_instance = VK_NULL_HANDLE;

    VKAPI_ATTR VkResult VKAPI_CALL HookCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *info, const VkAllocationCallbacks *allocator,
                                                    VkDevice *device)
    {
        auto enumerate = (PFN_vkEnumerateDeviceExtensionProperties)s_getInstanceProcAddr(s_instance, "vkEnumerateDeviceExtensionProperties");
        auto getFeatures = (PFN_vkGetPhysicalDeviceFeatures2)s_getInstanceProcAddr(s_instance, "vkGetPhysicalDeviceFeatures2");
        if (!enumerate || !getFeatures)
        {
            return s_createDevice(physicalDevice, info, allocator, device);
        }
        uint32_t count = 0;
        enumerate(physicalDevice, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available(count);
        enumerate(physicalDevice, nullptr, &count, available.data());
        auto isAvailable = [&](const char *name) {
            for (const VkExtensionProperties &e : available)
            {
                if (strcmp(e.extensionName, name) == 0)
                {
                    return true;
                }
            }
            return false;
        };
        std::vector<const char *> names(info->ppEnabledExtensionNames, info->ppEnabledExtensionNames + info->enabledExtensionCount);
        for (const char *name : kDeviceExtensions)
        {
            bool enabled = false;
            for (const char *n : names)
            {
                enabled = enabled || strcmp(n, name) == 0;
            }
            if (!enabled && isAvailable(name))
            {
                names.push_back(name);
            }
        }
        VkDeviceCreateInfo created = *info;
        created.enabledExtensionCount = (uint32_t)names.size();
        created.ppEnabledExtensionNames = names.data();

        // the samplerYcbcrConversion feature, in the structure of the chain that has it, or in one of ours
        VkPhysicalDeviceSamplerYcbcrConversionFeatures supported = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
        VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &supported};
        getFeatures(physicalDevice, &features);
        VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcr = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
        if (supported.samplerYcbcrConversion)
        {
            bool found = false;
            for (VkBaseOutStructure *s = (VkBaseOutStructure *)info->pNext; s; s = s->pNext)
            {
                if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES)
                {
                    ((VkPhysicalDeviceVulkan11Features *)s)->samplerYcbcrConversion = VK_TRUE;
                    found = true;
                }
                else if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES)
                {
                    ((VkPhysicalDeviceSamplerYcbcrConversionFeatures *)s)->samplerYcbcrConversion = VK_TRUE;
                    found = true;
                }
            }
            if (!found)
            {
                ycbcr.samplerYcbcrConversion = VK_TRUE;
                ycbcr.pNext = (void *)info->pNext;
                created.pNext = &ycbcr;
            }
        }
        return s_createDevice(physicalDevice, &created, allocator, device);
    }

    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL HookGetInstanceProcAddr(VkInstance instance, const char *name)
    {
        if (instance != VK_NULL_HANDLE)
        {
            s_instance = instance;
        }
        if (strcmp(name, "vkCreateDevice") == 0)
        {
            s_createDevice = (PFN_vkCreateDevice)s_getInstanceProcAddr(instance, name);
            return s_createDevice ? (PFN_vkVoidFunction)&HookCreateDevice : nullptr;
        }
        return s_getInstanceProcAddr(instance, name);
    }

    PFN_vkGetInstanceProcAddr UNITY_INTERFACE_API OnVulkanInitialization(PFN_vkGetInstanceProcAddr getInstanceProcAddr, void *userdata)
    {
        (void)userdata;
        s_getInstanceProcAddr = getInstanceProcAddr;
        return &HookGetInstanceProcAddr;
    }

    // ---- Device functions -------------------------------------------------------------------------------------------

#define MAFUNITY_VK_FUNCTIONS(F)                    \
    F(vkGetAndroidHardwareBufferPropertiesANDROID) \
    F(vkCreateImage)                               \
    F(vkDestroyImage)                              \
    F(vkAllocateMemory)                            \
    F(vkFreeMemory)                                \
    F(vkBindImageMemory)                           \
    F(vkCreateImageView)                           \
    F(vkDestroyImageView)                          \
    F(vkCreateSamplerYcbcrConversion)              \
    F(vkDestroySamplerYcbcrConversion)             \
    F(vkCreateSampler)                             \
    F(vkDestroySampler)                            \
    F(vkCreateDescriptorSetLayout)                 \
    F(vkDestroyDescriptorSetLayout)                \
    F(vkCreatePipelineLayout)                      \
    F(vkDestroyPipelineLayout)                     \
    F(vkCreateShaderModule)                        \
    F(vkDestroyShaderModule)                       \
    F(vkCreateRenderPass)                          \
    F(vkDestroyRenderPass)                         \
    F(vkCreateGraphicsPipelines)                   \
    F(vkDestroyPipeline)                           \
    F(vkCreateFramebuffer)                         \
    F(vkDestroyFramebuffer)                        \
    F(vkCreateDescriptorPool)                      \
    F(vkDestroyDescriptorPool)                     \
    F(vkAllocateDescriptorSets)                    \
    F(vkUpdateDescriptorSets)                      \
    F(vkCmdPipelineBarrier)                        \
    F(vkCmdBeginRenderPass)                        \
    F(vkCmdEndRenderPass)                          \
    F(vkCmdBindPipeline)                           \
    F(vkCmdBindDescriptorSets)                     \
    F(vkCmdPushConstants)                          \
    F(vkCmdSetViewport)                            \
    F(vkCmdSetScissor)                             \
    F(vkCmdDraw)

    // The device of Unity and its functions; shared by the objects created on it, destroyed after them
    struct Device
    {
        VkDevice device = VK_NULL_HANDLE;
        uint32_t queueFamily = 0;
#define MAFUNITY_VK_MEMBER(name) PFN_##name name = nullptr;
        MAFUNITY_VK_FUNCTIONS(MAFUNITY_VK_MEMBER)
#undef MAFUNITY_VK_MEMBER

        bool Load(const UnityVulkanInstance &instance)
        {
            device = instance.device;
            queueFamily = instance.queueFamilyIndex;
            auto getDeviceProcAddr = (PFN_vkGetDeviceProcAddr)instance.getInstanceProcAddr(instance.instance, "vkGetDeviceProcAddr");
            if (!getDeviceProcAddr)
            {
                return false;
            }
            bool ok = true;
#define MAFUNITY_VK_LOAD(name)                                     \
    name = (PFN_##name)getDeviceProcAddr(device, #name);           \
    if (!name && strcmp(#name, "vkCreateSamplerYcbcrConversion") == 0) \
        name = (PFN_##name)getDeviceProcAddr(device, #name "KHR"); \
    if (!name && strcmp(#name, "vkDestroySamplerYcbcrConversion") == 0) \
        name = (PFN_##name)getDeviceProcAddr(device, #name "KHR"); \
    ok = ok && name != nullptr;
            MAFUNITY_VK_FUNCTIONS(MAFUNITY_VK_LOAD)
#undef MAFUNITY_VK_LOAD
            // vkGetAndroidHardwareBufferPropertiesANDROID is null when Unity's device has not the AHardwareBuffer extension
            return ok;
        }
    };

    // ---- Sampling of a picture format: conversion, sampler, layouts and pipelines -----------------------------------

    struct Constants
    {
        float uvScale[2];
        float flipY;
    };

    bool IsYCbCr(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_UNDEFINED: // a format of the device: YCbCr
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
            return true;
        default:
            return false;
        }
    }

    struct Sampling
    {
        std::shared_ptr<Device> d;
        VkSamplerYcbcrConversion conversion = VK_NULL_HANDLE; // none for the RGB and single component pictures
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        std::map<VkFormat, std::pair<VkRenderPass, VkPipeline>> pipelines; // per target format

        ~Sampling()
        {
            for (auto &[format, p] : pipelines)
            {
                d->vkDestroyPipeline(d->device, p.second, nullptr);
                d->vkDestroyRenderPass(d->device, p.first, nullptr);
            }
            if (pipelineLayout)
                d->vkDestroyPipelineLayout(d->device, pipelineLayout, nullptr);
            if (setLayout)
                d->vkDestroyDescriptorSetLayout(d->device, setLayout, nullptr);
            if (sampler)
                d->vkDestroySampler(d->device, sampler, nullptr);
            if (conversion)
                d->vkDestroySamplerYcbcrConversion(d->device, conversion, nullptr);
        }
    };

    // ---- An imported picture ----------------------------------------------------------------------------------------

    struct VulkanPicture
    {
        std::shared_ptr<Device> d;
        std::shared_ptr<Sampling> sampling;
        AHardwareBuffer *buffer = nullptr;
        uint32_t width = 0; // of the picture
        uint32_t height = 0;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
        bool acquired = false; // owned by the graphics queue (acquired from the foreign queue of the producer)

        ~VulkanPicture()
        {
            if (pool)
                d->vkDestroyDescriptorPool(d->device, pool, nullptr);
            if (view)
                d->vkDestroyImageView(d->device, view, nullptr);
            if (image)
                d->vkDestroyImage(d->device, image, nullptr);
            if (memory)
                d->vkFreeMemory(d->device, memory, nullptr);
            if (buffer)
                AHardwareBuffer_release(buffer);
        }
    };

    class VulkanBackend : public GraphicsBackend, public PictureImporter
    {
    private:
        IUnityGraphicsVulkanV2 *unity_;
        std::shared_ptr<Device> d_;
        VkShaderModule vertex_ = VK_NULL_HANDLE;
        VkShaderModule fragment_ = VK_NULL_HANDLE;
        std::mutex mutex_; // the caches: imports on the main thread, rendering on the render thread
        std::map<std::tuple<VkFormat, uint64_t, int, int, int, int, int, int, int, int, int>, std::shared_ptr<Sampling>> samplings_;
        struct Target
        {
            VkImageView view;
            VkFramebuffer framebuffer;
        };
        std::map<std::tuple<VkImage, VkFormat, uint32_t, uint32_t, VkRenderPass>, Target> targets_;
        std::deque<std::pair<uint64_t, unsigned long long>> pending_; // batches and the frame they were retired in

        VkShaderModule Shader(const uint32_t *code, size_t bytes)
        {
            VkShaderModuleCreateInfo info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            info.codeSize = bytes;
            info.pCode = code;
            VkShaderModule module = VK_NULL_HANDLE;
            d_->vkCreateShaderModule(d_->device, &info, nullptr, &module);
            return module;
        }

        // The sampling of pictures of a format, with the color conversion; requires mutex_
        std::shared_ptr<Sampling> GetSampling(const VkAndroidHardwareBufferFormatPropertiesANDROID &format, const ImportParams &params)
        {
            const bool ycbcr = IsYCbCr(format.format);
            const int model = ycbcr ? (params.ycbcrModel >= 0 ? params.ycbcrModel : (int)format.suggestedYcbcrModel) : -1;
            const int range = ycbcr ? (params.ycbcrRange >= 0 ? params.ycbcrRange : (int)format.suggestedYcbcrRange) : -1;
            const bool linear = (format.formatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT) != 0;
            const VkFilter filter = (!ycbcr || linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            const VkComponentMapping &c = format.samplerYcbcrConversionComponents;
            const auto key = std::make_tuple(format.format, format.format == VK_FORMAT_UNDEFINED ? format.externalFormat : 0, model, range,
                                             (int)filter, (int)c.r, (int)c.g, (int)c.b, (int)c.a, (int)format.suggestedXChromaOffset,
                                             (int)format.suggestedYChromaOffset);
            auto it = samplings_.find(key);
            if (it != samplings_.end())
            {
                return it->second;
            }
            auto s = std::make_shared<Sampling>();
            s->d = d_;
            VkSamplerYcbcrConversionInfo conversionInfo = {VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO};
            if (ycbcr)
            {
                VkExternalFormatANDROID external = {VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID};
                external.externalFormat = format.format == VK_FORMAT_UNDEFINED ? format.externalFormat : 0;
                VkSamplerYcbcrConversionCreateInfo info = {VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO, &external};
                info.format = format.format;
                info.ycbcrModel = (VkSamplerYcbcrModelConversion)model;
                info.ycbcrRange = (VkSamplerYcbcrRange)range;
                info.components = c;
                info.xChromaOffset = format.suggestedXChromaOffset;
                info.yChromaOffset = format.suggestedYChromaOffset;
                info.chromaFilter = filter;
                info.forceExplicitReconstruction = VK_FALSE;
                if (d_->vkCreateSamplerYcbcrConversion(d_->device, &info, nullptr, &s->conversion) != VK_SUCCESS)
                {
                    return nullptr;
                }
                conversionInfo.conversion = s->conversion;
            }
            VkSamplerCreateInfo sampler = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, ycbcr ? &conversionInfo : nullptr};
            sampler.magFilter = filter;
            sampler.minFilter = filter;
            sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler.maxLod = 0.0f;
            if (d_->vkCreateSampler(d_->device, &sampler, nullptr, &s->sampler) != VK_SUCCESS)
            {
                return nullptr;
            }
            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = 0;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.pImmutableSamplers = &s->sampler;
            VkDescriptorSetLayoutCreateInfo setLayout = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            setLayout.bindingCount = 1;
            setLayout.pBindings = &binding;
            VkPushConstantRange constants = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Constants)};
            VkPipelineLayoutCreateInfo layout = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layout.setLayoutCount = 1;
            layout.pSetLayouts = &s->setLayout;
            layout.pushConstantRangeCount = 1;
            layout.pPushConstantRanges = &constants;
            if (d_->vkCreateDescriptorSetLayout(d_->device, &setLayout, nullptr, &s->setLayout) != VK_SUCCESS ||
                d_->vkCreatePipelineLayout(d_->device, &layout, nullptr, &s->pipelineLayout) != VK_SUCCESS)
            {
                return nullptr;
            }
            samplings_[key] = s;
            return s;
        }

        // The render pass and pipeline rendering pictures of a sampling into targets of a format; requires mutex_
        std::pair<VkRenderPass, VkPipeline> GetPipeline(Sampling &s, VkFormat targetFormat)
        {
            auto it = s.pipelines.find(targetFormat);
            if (it != s.pipelines.end())
            {
                return it->second;
            }
            VkAttachmentDescription attachment = {};
            attachment.format = targetFormat;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkAttachmentReference color = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color;
            VkRenderPassCreateInfo pass = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            pass.attachmentCount = 1;
            pass.pAttachments = &attachment;
            pass.subpassCount = 1;
            pass.pSubpasses = &subpass;
            VkRenderPass renderPass = VK_NULL_HANDLE;
            if (d_->vkCreateRenderPass(d_->device, &pass, nullptr, &renderPass) != VK_SUCCESS)
            {
                return {VK_NULL_HANDLE, VK_NULL_HANDLE};
            }

            VkPipelineShaderStageCreateInfo stages[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = vertex_;
            stages[0].pName = "main";
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = fragment_;
            stages[1].pName = "main";
            VkPipelineVertexInputStateCreateInfo vertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo assembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewport.viewportCount = 1;
            viewport.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo raster = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth = 1.0f;
            VkPipelineMultisampleStateCreateInfo multisample = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState blendAttachment = {};
            blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo blend = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            blend.attachmentCount = 1;
            blend.pAttachments = &blendAttachment;
            VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamic.dynamicStateCount = 2;
            dynamic.pDynamicStates = dynamicStates;
            VkGraphicsPipelineCreateInfo info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            info.stageCount = 2;
            info.pStages = stages;
            info.pVertexInputState = &vertexInput;
            info.pInputAssemblyState = &assembly;
            info.pViewportState = &viewport;
            info.pRasterizationState = &raster;
            info.pMultisampleState = &multisample;
            info.pColorBlendState = &blend;
            info.pDynamicState = &dynamic;
            info.layout = s.pipelineLayout;
            info.renderPass = renderPass;
            VkPipeline pipeline = VK_NULL_HANDLE;
            if (d_->vkCreateGraphicsPipelines(d_->device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
            {
                d_->vkDestroyRenderPass(d_->device, renderPass, nullptr);
                return {VK_NULL_HANDLE, VK_NULL_HANDLE};
            }
            s.pipelines[targetFormat] = {renderPass, pipeline};
            return s.pipelines[targetFormat];
        }

        // The framebuffer of a target image for a render pass; requires mutex_
        VkFramebuffer GetFramebuffer(const UnityVulkanImage &target, VkRenderPass renderPass)
        {
            const auto key = std::make_tuple(target.image, target.format, target.extent.width, target.extent.height, renderPass);
            auto it = targets_.find(key);
            if (it != targets_.end())
            {
                return it->second.framebuffer;
            }
            VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view.image = target.image;
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format = target.format;
            view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            Target t = {VK_NULL_HANDLE, VK_NULL_HANDLE};
            if (d_->vkCreateImageView(d_->device, &view, nullptr, &t.view) != VK_SUCCESS)
            {
                return VK_NULL_HANDLE;
            }
            VkFramebufferCreateInfo framebuffer = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebuffer.renderPass = renderPass;
            framebuffer.attachmentCount = 1;
            framebuffer.pAttachments = &t.view;
            framebuffer.width = target.extent.width;
            framebuffer.height = target.extent.height;
            framebuffer.layers = 1;
            if (d_->vkCreateFramebuffer(d_->device, &framebuffer, nullptr, &t.framebuffer) != VK_SUCCESS)
            {
                d_->vkDestroyImageView(d_->device, t.view, nullptr);
                return VK_NULL_HANDLE;
            }
            targets_[key] = t;
            return t.framebuffer;
        }

        // Releases the batches retired in frames the GPU completed
        void CompleteSafeBatches(TextureRegistry &registry, unsigned long long safeFrame)
        {
            while (!pending_.empty() && pending_.front().second <= safeFrame)
            {
                registry.Complete(pending_.front().first);
                pending_.pop_front();
            }
        }

    public:
        VulkanBackend(IUnityGraphicsVulkanV2 *unity, std::shared_ptr<Device> device) : unity_(unity), d_(std::move(device))
        {
            vertex_ = Shader(kConvert_vert, sizeof(kConvert_vert));
            fragment_ = Shader(kConvert_frag, sizeof(kConvert_frag));
            // the frame is rendered outside of a render pass of Unity
            UnityVulkanPluginEventConfig update = {kUnityVulkanRenderPass_EnsureOutside, kUnityVulkanGraphicsQueueAccess_DontCare,
                                                   kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission | kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState};
            unity_->ConfigureEvent(MAFUNITY_EVENT_UPDATE, &update);
            UnityVulkanPluginEventConfig release = {kUnityVulkanRenderPass_DontCare, kUnityVulkanGraphicsQueueAccess_DontCare,
                                                    kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission};
            unity_->ConfigureEvent(MAFUNITY_EVENT_RELEASE, &release);
        }

        ~VulkanBackend() override
        {
            for (auto &[key, t] : targets_)
            {
                d_->vkDestroyFramebuffer(d_->device, t.framebuffer, nullptr);
                d_->vkDestroyImageView(d_->device, t.view, nullptr);
            }
            samplings_.clear();
            if (vertex_)
                d_->vkDestroyShaderModule(d_->device, vertex_, nullptr);
            if (fragment_)
                d_->vkDestroyShaderModule(d_->device, fragment_, nullptr);
        }

        bool Valid() const
        {
            return vertex_ != VK_NULL_HANDLE && fragment_ != VK_NULL_HANDLE;
        }

        PictureImporter &Importer() override
        {
            return *this;
        }

        std::shared_ptr<ImportedPicture> Import(void *picture, const ImportParams &params) override
        {
            AHardwareBuffer *buffer = (AHardwareBuffer *)picture;
            if (!buffer)
            {
                return nullptr;
            }
            VkAndroidHardwareBufferFormatPropertiesANDROID format = {VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID};
            VkAndroidHardwareBufferPropertiesANDROID properties = {VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID, &format};
            if (d_->vkGetAndroidHardwareBufferPropertiesANDROID(d_->device, buffer, &properties) != VK_SUCCESS || properties.memoryTypeBits == 0)
            {
                return nullptr;
            }
            AHardwareBuffer_Desc desc = {};
            AHardwareBuffer_describe(buffer, &desc);

            auto p = std::make_shared<VulkanPicture>();
            p->d = d_;
            AHardwareBuffer_acquire(buffer);
            p->buffer = buffer;
            p->width = desc.width;
            p->height = desc.height;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                p->sampling = GetSampling(format, params);
            }
            if (!p->sampling)
            {
                return nullptr;
            }

            // the image, on the memory of the buffer
            VkExternalFormatANDROID external = {VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID};
            external.externalFormat = format.format == VK_FORMAT_UNDEFINED ? format.externalFormat : 0;
            VkExternalMemoryImageCreateInfo externalMemory = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, &external};
            externalMemory.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
            VkImageCreateInfo image = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, &externalMemory};
            image.imageType = VK_IMAGE_TYPE_2D;
            image.format = format.format;
            image.extent = {desc.width, desc.height, 1};
            image.mipLevels = 1;
            image.arrayLayers = 1;
            image.samples = VK_SAMPLE_COUNT_1_BIT;
            image.tiling = VK_IMAGE_TILING_OPTIMAL;
            image.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
            image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            if (d_->vkCreateImage(d_->device, &image, nullptr, &p->image) != VK_SUCCESS)
            {
                return nullptr;
            }
            VkImportAndroidHardwareBufferInfoANDROID import = {VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID};
            import.buffer = buffer;
            VkMemoryDedicatedAllocateInfo dedicated = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, &import};
            dedicated.image = p->image;
            VkMemoryAllocateInfo allocate = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &dedicated};
            allocate.allocationSize = properties.allocationSize;
            allocate.memoryTypeIndex = (uint32_t)__builtin_ctz(properties.memoryTypeBits);
            if (d_->vkAllocateMemory(d_->device, &allocate, nullptr, &p->memory) != VK_SUCCESS ||
                d_->vkBindImageMemory(d_->device, p->image, p->memory, 0) != VK_SUCCESS)
            {
                return nullptr;
            }

            // the view, with the color conversion of the sampler
            VkSamplerYcbcrConversionInfo conversion = {VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO};
            conversion.conversion = p->sampling->conversion;
            VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, p->sampling->conversion ? &conversion : nullptr};
            view.image = p->image;
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format = format.format;
            view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            if (d_->vkCreateImageView(d_->device, &view, nullptr, &p->view) != VK_SUCCESS)
            {
                return nullptr;
            }

            // the descriptor of the picture: a YCbCr picture may take several descriptors
            VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3};
            VkDescriptorPoolCreateInfo pool = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool.maxSets = 1;
            pool.poolSizeCount = 1;
            pool.pPoolSizes = &size;
            VkDescriptorSetAllocateInfo set = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            set.descriptorSetCount = 1;
            set.pSetLayouts = &p->sampling->setLayout;
            if (d_->vkCreateDescriptorPool(d_->device, &pool, nullptr, &p->pool) != VK_SUCCESS)
            {
                return nullptr;
            }
            set.descriptorPool = p->pool;
            if (d_->vkAllocateDescriptorSets(d_->device, &set, &p->set) != VK_SUCCESS)
            {
                return nullptr;
            }
            VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, p->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = p->set;
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &imageInfo;
            d_->vkUpdateDescriptorSets(d_->device, 1, &write, 0, nullptr);

            auto imported = std::make_shared<ImportedPicture>();
            imported->params = params;
            imported->planeCount = 0; // rendered into the target texture
            imported->state = p;
            return imported;
        }

        void Update(TextureRegistry &registry, int texture) override
        {
            const TextureRegistry::Settings settings = registry.GetSettings(texture);
            const ImportedPicture *imported = registry.Current(texture);
            if (!settings.target || !imported || !imported->state)
            {
                return;
            }
            VulkanPicture *p = (VulkanPicture *)imported->state.get();
            UnityVulkanImage target = {};
            if (!unity_->AccessTexture(settings.target, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                       kUnityVulkanResourceAccess_PipelineBarrier, &target))
            {
                return;
            }
            UnityVulkanRecordingState state = {};
            if (!unity_->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare))
            {
                return;
            }
            std::pair<VkRenderPass, VkPipeline> pipeline;
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                pipeline = GetPipeline(*p->sampling, target.format);
                if (pipeline.second)
                {
                    framebuffer = GetFramebuffer(target, pipeline.first);
                }
            }
            if (!pipeline.second || !framebuffer)
            {
                return;
            }
            VkCommandBuffer commands = state.commandBuffer;
            if (!p->acquired)
            {
                // the picture comes from the producer (MediaCodec, or the CPU): acquired from the foreign queue
                VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT;
                barrier.dstQueueFamilyIndex = d_->queueFamily;
                barrier.image = p->image;
                barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                d_->vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                         &barrier);
                p->acquired = true;
            }
            VkRenderPassBeginInfo begin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            begin.renderPass = pipeline.first;
            begin.framebuffer = framebuffer;
            begin.renderArea = {{0, 0}, {target.extent.width, target.extent.height}};
            d_->vkCmdBeginRenderPass(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
            VkViewport viewport = {0.0f, 0.0f, (float)target.extent.width, (float)target.extent.height, 0.0f, 1.0f};
            VkRect2D scissor = {{0, 0}, {target.extent.width, target.extent.height}};
            d_->vkCmdSetViewport(commands, 0, 1, &viewport);
            d_->vkCmdSetScissor(commands, 0, 1, &scissor);
            d_->vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.second);
            d_->vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, p->sampling->pipelineLayout, 0, 1, &p->set, 0, nullptr);
            // the frame is the top-left region of the picture
            Constants constants = {};
            constants.uvScale[0] = (imported->params.width > 0 && p->width > 0) ? (float)imported->params.width / (float)p->width : 1.0f;
            constants.uvScale[1] = (imported->params.height > 0 && p->height > 0) ? (float)imported->params.height / (float)p->height : 1.0f;
            constants.flipY = settings.flipY ? 1.0f : 0.0f;
            d_->vkCmdPushConstants(commands, p->sampling->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Constants), &constants);
            d_->vkCmdDraw(commands, 3, 1, 0, 0);
            d_->vkCmdEndRenderPass(commands);
            CompleteSafeBatches(registry, state.safeFrameNumber);
        }

        void Release(TextureRegistry &registry) override
        {
            UnityVulkanRecordingState state = {};
            if (!unity_->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare))
            {
                return;
            }
            // the frames retired until now are no longer used once the frame being recorded completed
            if (uint64_t batch = registry.RetireBatch())
            {
                pending_.emplace_back(batch, state.currentFrameNumber);
            }
            CompleteSafeBatches(registry, state.safeFrameNumber);
        }
    };

} // namespace

void InterceptVulkanInitialization(IUnityInterfaces *interfaces)
{
    IUnityGraphicsVulkanV2 *vulkan = interfaces->Get<IUnityGraphicsVulkanV2>();
    if (vulkan)
    {
        vulkan->AddInterceptInitialization(&OnVulkanInitialization, nullptr, 0);
    }
}

std::unique_ptr<GraphicsBackend> CreateVulkanBackend(IUnityInterfaces *interfaces)
{
    IUnityGraphics *graphics = interfaces->Get<IUnityGraphics>();
    IUnityGraphicsVulkanV2 *vulkan = interfaces->Get<IUnityGraphicsVulkanV2>();
    if (!graphics || graphics->GetRenderer() != kUnityGfxRendererVulkan || !vulkan)
    {
        return nullptr;
    }
    auto device = std::make_shared<Device>();
    // fails when Unity's device has not the AHardwareBuffer extension (the plugin was not loaded at startup)
    if (!device->Load(vulkan->Instance()))
    {
        return nullptr;
    }
    auto backend = std::make_unique<VulkanBackend>(vulkan, device);
    if (!backend->Valid())
    {
        return nullptr;
    }
    return backend;
}

#endif // __ANDROID__
