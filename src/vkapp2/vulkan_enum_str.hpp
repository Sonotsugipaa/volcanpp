/* MIT License
 *
 * Copyright (c) 2021 Parola Marco
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */



/* This header only contains template specializations of enum_str,
 * particularly those related to Vulkan types. */

#pragma once

#include "util/util.hpp"
#include <vulkan/vulkan.hpp>



namespace util {

	#define MK_SPEC_PROTO(_TYPE) \
		template<typename _st = std::string> \
		constexpr _st enum_str(_TYPE e)

	#define MK_CASE(_E, _STR) case _E: return #_STR;

	constexpr const char* UNKNOWN_ENUM = "<unknown>";


	MK_SPEC_PROTO(vk::PresentModeKHR) {
		switch(e) {
			MK_CASE(vk::PresentModeKHR::eFifo,                    FIFO)
			MK_CASE(vk::PresentModeKHR::eFifoRelaxed,             FIFO_RELAXED)
			MK_CASE(vk::PresentModeKHR::eImmediate,               IMMEDIATE)
			MK_CASE(vk::PresentModeKHR::eMailbox,                 MAILBOX)
			MK_CASE(vk::PresentModeKHR::eSharedContinuousRefresh, SHARED_CONTINUOUS_REFRESH)
			MK_CASE(vk::PresentModeKHR::eSharedDemandRefresh,     SHARED_DEMAND_REFRESH)
		}
		return UNKNOWN_ENUM;
	}


	MK_SPEC_PROTO(vk::SampleCountFlagBits) {
		switch(e) {
			MK_CASE(vk::SampleCountFlagBits::e1,  SAMPLE_COUNT_1);
			MK_CASE(vk::SampleCountFlagBits::e2,  SAMPLE_COUNT_2);
			MK_CASE(vk::SampleCountFlagBits::e4,  SAMPLE_COUNT_4);
			MK_CASE(vk::SampleCountFlagBits::e8,  SAMPLE_COUNT_8);
			MK_CASE(vk::SampleCountFlagBits::e16, SAMPLE_COUNT_16);
			MK_CASE(vk::SampleCountFlagBits::e32, SAMPLE_COUNT_32);
			MK_CASE(vk::SampleCountFlagBits::e64, SAMPLE_COUNT_64);
		}
		return UNKNOWN_ENUM;
	}


	MK_SPEC_PROTO(VkPresentModeKHR) {
		return enum_str(vk::PresentModeKHR(e));
	}


	MK_SPEC_PROTO(vk::CompositeAlphaFlagBitsKHR) {
		switch(e) {
			MK_CASE(vk::CompositeAlphaFlagBitsKHR::eInherit,        INHERIT)
			MK_CASE(vk::CompositeAlphaFlagBitsKHR::eOpaque,         OPAQUE)
			MK_CASE(vk::CompositeAlphaFlagBitsKHR::ePostMultiplied, POST_MULTIPLIED)
			MK_CASE(vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,  PRE_MULTIPLIED)
		}
		return UNKNOWN_ENUM;
	}


	MK_SPEC_PROTO(vk::QueueFlagBits) {
		switch(e) {
			MK_CASE(vk::QueueFlagBits::eCompute,       COMPUTE)
			MK_CASE(vk::QueueFlagBits::eGraphics,      GRAPHICS)
			MK_CASE(vk::QueueFlagBits::eProtected,     PROTECTED)
			MK_CASE(vk::QueueFlagBits::eSparseBinding, SPARSE_BINDING)
			MK_CASE(vk::QueueFlagBits::eTransfer,      TRANSFER)
		}
		return UNKNOWN_ENUM;
	}


	MK_SPEC_PROTO(vk::PhysicalDeviceType) {
		switch(e) {
			MK_CASE(vk::PhysicalDeviceType::eCpu,           CPU)
			MK_CASE(vk::PhysicalDeviceType::eDiscreteGpu,   DISCRETE_GPU)
			MK_CASE(vk::PhysicalDeviceType::eIntegratedGpu, INTEGRATED_GPU)
			MK_CASE(vk::PhysicalDeviceType::eOther,         OTHER)
			MK_CASE(vk::PhysicalDeviceType::eVirtualGpu,    VIRTUAL_GPU)
		}
		return UNKNOWN_ENUM;
	}


	MK_SPEC_PROTO(vk::ImageUsageFlagBits) {
		switch(e) {
			MK_CASE(vk::ImageUsageFlagBits::eColorAttachment,        COLOR_ATTACHMENT)
			MK_CASE(vk::ImageUsageFlagBits::eDepthStencilAttachment, DEPTH_STENCIL_ATTACHMENT)
			MK_CASE(vk::ImageUsageFlagBits::eFragmentDensityMapEXT,  FRAGMENT_DENSITY_MAP_EXT)
			MK_CASE(vk::ImageUsageFlagBits::eFragmentShadingRateAttachmentKHR, FRAGMENT_SHADING_RATE_ATTACHMENT_KHR)
			MK_CASE(vk::ImageUsageFlagBits::eInputAttachment,        INPUT_ATTACHMENT)
			MK_CASE(vk::ImageUsageFlagBits::eSampled,                SAMPLED)
			MK_CASE(vk::ImageUsageFlagBits::eStorage,                STORAGE)
			MK_CASE(vk::ImageUsageFlagBits::eTransferDst,            TRANSFER_DST)
			MK_CASE(vk::ImageUsageFlagBits::eTransferSrc,            TRANSFER_SRC)
			MK_CASE(vk::ImageUsageFlagBits::eTransientAttachment,    TRANSIENT_ATTACHMENT)
		}
		return UNKNOWN_ENUM;
	}


	MK_SPEC_PROTO(vk::Format) {
		switch(e) {
			MK_CASE(vk::Format::eUndefined,           UNDEFINED)
			MK_CASE(vk::Format::eR32G32B32A32Sfloat,  R32G32B32A32_SFLOAT)
			MK_CASE(vk::Format::eR32G32B32A32Sint,    R32G32B32A32_SINT)
			MK_CASE(vk::Format::eR32G32B32Sfloat,     R32G32B32_SFLOAT)
			MK_CASE(vk::Format::eR32G32B32Sint,       R32G32B32_SINT)
			MK_CASE(vk::Format::eR8G8B8A8Srgb,        R8G8B8A8_SRGB)
			MK_CASE(vk::Format::eB8G8R8A8Uint,        B8G8R8A8_UINT)
			MK_CASE(vk::Format::eB8G8R8A8Unorm,       B8G8R8A8_UNORM)
			MK_CASE(vk::Format::eB8G8R8Srgb,          B8G8R8_SRGB)
			MK_CASE(vk::Format::eB8G8R8Uint,          B8G8R8_UINT)
			MK_CASE(vk::Format::eB8G8R8Unorm,         B8G8R8_UNORM)
			MK_CASE(vk::Format::eD16Unorm,            D16_UNORM)
			MK_CASE(vk::Format::eD16UnormS8Uint,      D16_UNORM_S8_UINT)
			MK_CASE(vk::Format::eD24UnormS8Uint,      D24_UNORM_S8_UINT)
			MK_CASE(vk::Format::eD32Sfloat,           D32_SFLOAT)
			MK_CASE(vk::Format::eD32SfloatS8Uint,     D32_SFLOAT_S8_UINT)
			MK_CASE(vk::Format::eX8D24UnormPack32,    X8_D24_UNORM_PACK32)
			default: return UNKNOWN_ENUM;
		}
	}


	MK_SPEC_PROTO(VkResult) {
		switch(e) {
			ENUM_STR_CASE(VK_ERROR_DEVICE_LOST)
			ENUM_STR_CASE(VK_ERROR_EXTENSION_NOT_PRESENT)
			ENUM_STR_CASE(VK_ERROR_FEATURE_NOT_PRESENT)
			ENUM_STR_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED)
			ENUM_STR_CASE(VK_ERROR_FRAGMENTATION)
			ENUM_STR_CASE(VK_ERROR_FRAGMENTED_POOL)
			ENUM_STR_CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
			ENUM_STR_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
			ENUM_STR_CASE(VK_ERROR_INCOMPATIBLE_DRIVER)
			ENUM_STR_CASE(VK_ERROR_INITIALIZATION_FAILED)
			ENUM_STR_CASE(VK_ERROR_INVALID_DEVICE_ADDRESS_EXT)
			ENUM_STR_CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)
			ENUM_STR_CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE)
			ENUM_STR_CASE(VK_ERROR_INVALID_SHADER_NV)
			ENUM_STR_CASE(VK_ERROR_LAYER_NOT_PRESENT)
			ENUM_STR_CASE(VK_ERROR_MEMORY_MAP_FAILED)
			ENUM_STR_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
			ENUM_STR_CASE(VK_ERROR_NOT_PERMITTED_EXT)
			ENUM_STR_CASE(VK_ERROR_OUT_OF_DATE_KHR)
			ENUM_STR_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
			ENUM_STR_CASE(VK_ERROR_OUT_OF_HOST_MEMORY)
			ENUM_STR_CASE(VK_ERROR_OUT_OF_POOL_MEMORY)
			ENUM_STR_CASE(VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT)
			ENUM_STR_CASE(VK_ERROR_SURFACE_LOST_KHR)
			ENUM_STR_CASE(VK_ERROR_TOO_MANY_OBJECTS)
			ENUM_STR_CASE(VK_ERROR_UNKNOWN)
			ENUM_STR_CASE(VK_ERROR_VALIDATION_FAILED_EXT)
			ENUM_STR_CASE(VK_EVENT_SET)
			ENUM_STR_CASE(VK_EVENT_RESET)
			ENUM_STR_CASE(VK_INCOMPLETE)
			ENUM_STR_CASE(VK_NOT_READY)
			ENUM_STR_CASE(VK_OPERATION_DEFERRED_KHR)
			ENUM_STR_CASE(VK_OPERATION_NOT_DEFERRED_KHR)
			ENUM_STR_CASE(VK_RESULT_MAX_ENUM)
			ENUM_STR_CASE(VK_SUCCESS)
			ENUM_STR_CASE(VK_SUBOPTIMAL_KHR)
			ENUM_STR_CASE(VK_TIMEOUT)
			ENUM_STR_CASE(VK_THREAD_DONE_KHR)
			ENUM_STR_CASE(VK_THREAD_IDLE_KHR)
		}
		return UNKNOWN_ENUM;
	}

	#undef MK_CASE
	#undef MK_SPEC_PROTO

}
