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



#include "vkapp2/graphics.hpp"



namespace vka2 {

	CommandPool::CommandPool(vk::Device& dev, unsigned qFamIdx, bool transient):
			_dev(&dev),
			_pool(_dev->createCommandPool(vk::CommandPoolCreateInfo(
				transient?
					vk::CommandPoolCreateFlagBits::eTransient :
					vk::CommandPoolCreateFlagBits(),
				qFamIdx))),
			_fence_shared(dev.createFence({ }))
	{
		util::alloc_tracker.alloc("CommandPool");
	}

	void CommandPool::destroy() {
		_dev->destroyFence(_fence_shared);
		_dev->destroyCommandPool(_pool);
		util::alloc_tracker.dealloc("CommandPool");
	}


	void CommandPool::reset(bool do_release) {
		using vk::CommandPoolResetFlagBits;
		_dev->resetCommandPool(_pool,
			do_release?
				CommandPoolResetFlagBits::eReleaseResources :
				static_cast<CommandPoolResetFlagBits>(0));
	}


	void CommandPool::runCmds(
			vk::Queue queue,
			std::function<void (vk::CommandBuffer)> fn,
			vk::Fence fence
	) {
		vk::CommandBuffer cmdBuffer;
		{
			auto cbaInfo = vk::CommandBufferAllocateInfo(_pool,
				vk::CommandBufferLevel::ePrimary, 1);
			cmdBuffer = _dev->allocateCommandBuffers(
				cbaInfo).front();
		} {
			using vk::CommandBufferUsageFlagBits;
			auto cbbInfo = vk::CommandBufferBeginInfo(
				CommandBufferUsageFlagBits::eOneTimeSubmit);
			cmdBuffer.begin(cbbInfo);
		}
		fn(cmdBuffer);
		cmdBuffer.end();
		if(! fence) {
			// If no fence was given, syncronize with the shared fence
			queue.submit(vk::SubmitInfo({ }, { }, cmdBuffer), _fence_shared);
			auto result = _dev->waitForFences(_fence_shared, true, UINT64_MAX);
			if(result != vk::Result::eSuccess) {
				throw std::runtime_error(formatVkErrorMsg(
					"failed to wait on a CommandPool shared fence", vk::to_string(result)));
			}
			_dev->resetFences(_fence_shared);
		} else {
			// If a fence was given, use that one
			queue.submit(vk::SubmitInfo({ }, { }, cmdBuffer), fence);
		}
	}
}
