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

	CommandPool::BufferHandle::BufferHandle():
			cmdPool(nullptr),
			cmdBuffer(nullptr)
	{ }

	CommandPool::BufferHandle::BufferHandle(BufferHandle&& mv):
			cmdPool(std::move(mv.cmdPool)),
			cmdBuffer(std::move(mv.cmdBuffer))
	{
		#ifndef NDEBUG
			mv.cmdPool = nullptr;
		#endif
		mv.cmdBuffer = nullptr;
	}

	CommandPool::BufferHandle::BufferHandle(CommandPool* cmdPool, vk::CommandBuffer cmd):
			cmdPool(cmdPool),
			cmdBuffer(cmd)
	{
		assert(cmdPool != nullptr);
	}

	CommandPool::BufferHandle::~BufferHandle() {
		if(cmdBuffer) {
			assert(cmdPool != nullptr);
			cmdPool->_dev->freeCommandBuffers(cmdPool->_pool, { cmdBuffer });
			// util::alloc_tracker.dealloc("CommandPool:CommandBuffer"); // Leads to excessive clutter
			#ifndef NDEBUG
				cmdBuffer = nullptr;
			#endif
		}
	}

	CommandPool::BufferHandle& CommandPool::BufferHandle::operator=(BufferHandle&& mv) {
		this->~BufferHandle();
		return *new (this) BufferHandle(std::move(mv));
	}


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


	vk::CommandBuffer alloc_cmd_buffer(
			vk::Device* dev,
			vk::CommandPool pool,
			vk::CommandBufferLevel lvl
	) {
		auto cbaInfo = vk::CommandBufferAllocateInfo(pool, lvl, 1);
		auto r = dev->allocateCommandBuffers(
			cbaInfo).front();
		// util::alloc_tracker.alloc("CommandPool:CommandBuffer"); // Leads to excessive clutter
		return r;
	}


	CommandPool::BufferHandle CommandPool::runCmdsAsync(
			vk::Queue queue,
			std::function<void (vk::CommandBuffer)> fn,
			vk::Fence fence
	) {
		assert(fence);
		vk::CommandBuffer cmdBuffer = alloc_cmd_buffer(
			_dev, _pool, vk::CommandBufferLevel::ePrimary);
		auto cbbInfo = vk::CommandBufferBeginInfo(
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		cmdBuffer.begin(cbbInfo);
		fn(cmdBuffer);
		cmdBuffer.end();
		queue.submit(vk::SubmitInfo({ }, { }, cmdBuffer), fence);
		return BufferHandle(this, cmdBuffer);
	}


	void CommandPool::runCmds(
			vk::Queue queue,
			std::function<void (vk::CommandBuffer)> fn
	) {
		vk::CommandBuffer cmdBuffer;
		assert(_dev->getFenceStatus(_fence_shared) == vk::Result::eNotReady);
		{
			cmdBuffer = alloc_cmd_buffer(
			_dev, _pool, vk::CommandBufferLevel::ePrimary);
			auto cbbInfo = vk::CommandBufferBeginInfo(
				vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
			cmdBuffer.begin(cbbInfo);
			fn(cmdBuffer);
			cmdBuffer.end();
			queue.submit(vk::SubmitInfo({ }, { }, cmdBuffer), _fence_shared);
		}
		auto result = _dev->waitForFences(_fence_shared, true, UINT64_MAX);
		if(result != vk::Result::eSuccess) {
			throw std::runtime_error(formatVkErrorMsg(
				"failed to wait on a CommandPool shared fence", vk::to_string(result)));
		}
		_dev->resetFences(_fence_shared);
		_dev->freeCommandBuffers(_pool, { cmdBuffer });
	}
}
