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



/* Here lies everything that has to do with memory management. */

#include "vkapp2/graphics.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wunused-variable"
	#define VMA_IMPLEMENTATION
	#include "vma/vk_mem_alloc.h"
#pragma GCC diagnostic pop

#include "vkapp2/runtime.hpp"
#include "util/util.hpp"

using namespace vka2;



namespace vka2 {

	void* Application::_mmap_buffer(VmaAllocation& allocation) {
		void* r;
		VkResult result = vmaMapMemory(_data.alloc, allocation, &r);
		if(result != VK_SUCCESS) {
			throw std::runtime_error(formatVkErrorMsg(
				"failed to mmap a buffer", util::enum_str(result)));
		}
		return r;
	}


	void Application::stageBufferData(
			vk::Buffer dst, const void* srcPtr, size_t srcSize
	) {
		if(srcSize == 0)  return;
		VmaAllocation stagingAlloc;
		VkBuffer stagingBuffer;
		void* mmapd;
		{
			VkBufferCreateInfo bcInfo = { };
			VmaAllocationCreateInfo acInfo = { };
			bcInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bcInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			bcInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bcInfo.size = srcSize;
			acInfo.requiredFlags =
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			auto result = vmaCreateBuffer(_data.alloc,
				&bcInfo, &acInfo, &stagingBuffer, &stagingAlloc, nullptr);
			if(result != VK_SUCCESS) {
				std::runtime_error(formatVkErrorMsg(
					"failed to allocate a staging buffer", util::enum_str(result)));
			}
		} {
			mmapd = _mmap_buffer(stagingAlloc);
			memcpy(mmapd, srcPtr, srcSize);
			unmapBuffer(stagingAlloc);
		} {
			_data.transferCmdPool.runCmds(
				_data.queues.transfer,
				[dst, srcSize, mmapd, stagingBuffer](vk::CommandBuffer cmd) {
					auto cp = vk::BufferCopy(0, 0, srcSize);
					cmd.copyBuffer(stagingBuffer, dst, cp);
				});
		}
		vmaDestroyBuffer(_data.alloc, stagingBuffer, stagingAlloc);
	}


	BufferAlloc Application::createBuffer(
			const vk::BufferCreateInfo& bcInfo,
			vk::MemoryPropertyFlags reqFlags,
			vk::MemoryPropertyFlags prfFlags,
			vk::MemoryPropertyFlags prohibFlags
	) {
		VkBufferCreateInfo bcInfoC = bcInfo;
		VmaAllocationCreateInfo acInfo = { };
		VkBuffer cBuffer;
		VmaAllocation allocation;
		acInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(reqFlags);
		acInfo.preferredFlags = static_cast<VkMemoryPropertyFlags>(prfFlags);
		acInfo.memoryTypeBits = static_cast<VkMemoryPropertyFlags>(~prohibFlags);
		VkResult result = vmaCreateBuffer(_data.alloc,
			&bcInfoC, &acInfo, &cBuffer, &allocation, nullptr);
		if(result != VK_SUCCESS) {
			throw std::runtime_error(formatVkErrorMsg(
				"failed to create a buffer", util::enum_str(result)));
		}
		return { cBuffer, allocation };
	}


	BufferAlloc Application::createBuffer(
			const vk::BufferCreateInfo& bcInfo,
			VmaMemoryUsage usage,
			vk::MemoryPropertyFlags prohibFlags
	) {
		VkBufferCreateInfo bcInfoC = bcInfo;
		VmaAllocationCreateInfo acInfo = { };
		VkBuffer cBuffer;
		VmaAllocation allocation;
		acInfo.usage = usage;
		acInfo.memoryTypeBits = static_cast<VkMemoryPropertyFlags>(~prohibFlags);
		VkResult result = vmaCreateBuffer(_data.alloc,
			&bcInfoC, &acInfo, &cBuffer, &allocation, nullptr);
		if(result != VK_SUCCESS) {
			throw std::runtime_error(formatVkErrorMsg(
				"failed to create a buffer", util::enum_str(result)));
		}
		return { cBuffer, allocation };
	}

	void Application::destroyBuffer(BufferAlloc& buffer) {
		vmaDestroyBuffer(_data.alloc, buffer.handle, buffer.alloc);
	}


	ImageAlloc Application::createImage(
			const vk::ImageCreateInfo& icInfo,
			vk::MemoryPropertyFlags reqFlags, vk::MemoryPropertyFlags prfFlags,
			vk::MemoryPropertyFlags prohibFlags
	) {
		VkImageCreateInfo icInfoC = icInfo;
		VmaAllocationCreateInfo acInfo = { };
		VkImage cImage;
		VmaAllocation allocation;
		acInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(reqFlags);
		acInfo.preferredFlags = static_cast<VkMemoryPropertyFlags>(prfFlags);
		acInfo.memoryTypeBits = static_cast<VkMemoryPropertyFlags>(~prohibFlags);
		VkResult result = vmaCreateImage(_data.alloc,
			&icInfoC, &acInfo, &cImage, &allocation, nullptr);
		if(result != VK_SUCCESS) {
			throw std::runtime_error(formatVkErrorMsg(
				"failed to create an image", util::enum_str(result)));
		}
		return { cImage, allocation };
	}

	void Application::destroyImage(ImageAlloc& image) {
		vmaDestroyImage(_data.alloc, image.handle, image.alloc);
	}


	void Application::unmapBuffer(VmaAllocation& allocation) {
		// This is basically just a proxy call, for consistency
		vmaUnmapMemory(_data.alloc, allocation);
	}

}
