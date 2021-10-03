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



#pragma once



#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.hpp>

#include <vector>
#include <functional>



namespace vka2 {

	class DynDescriptorPool {
	public:
		using Callback = std::function<void (DynDescriptorPool&)>;
		using PoolConstructor = std::function<vk::DescriptorPool (size_t sets)>;
		using PoolDestructor = std::function<void (vk::DescriptorPool)>;
		using SetAllocator = std::function<std::vector<vk::DescriptorSet> (vk::DescriptorPool, size_t sets)>;

		class SetHandle {
		private:
			friend DynDescriptorPool;

			size_t offset_;

			SetHandle(size_t value): offset_(value) { }

		public:
			SetHandle() = default;
			SetHandle(const SetHandle&) = default;
			SetHandle(SetHandle&&) = default;
			SetHandle& operator=(const SetHandle&) = default;
			SetHandle& operator=(SetHandle&&) = default;

			vk::DescriptorSet get(DynDescriptorPool&) const;
		};
		friend SetHandle;

	private:
		vk::Device dev_;
		vk::DescriptorPool pool_;
		std::vector<vk::DescriptorSet> allocated_;
		std::vector<SetHandle> released_;
		PoolConstructor constructor_;
		PoolDestructor destructor_;
		SetAllocator allocator_;
		size_t acquiredCount_;
		bool validState_;

	public:
		DynDescriptorPool() noexcept;
		DynDescriptorPool(vk::Device, PoolConstructor, PoolDestructor, SetAllocator);
		DynDescriptorPool(vk::Device, PoolConstructor, PoolDestructor, SetAllocator, size_t initSize);

		DynDescriptorPool(DynDescriptorPool&&) noexcept;
		DynDescriptorPool& operator=(DynDescriptorPool&&) noexcept;

		~DynDescriptorPool();
		DynDescriptorPool& operator=(std::nullptr_t) noexcept;

		operator bool() const { return validState_; }
		bool operator !() const { return ! operator bool(); }

		vk::DescriptorPool handle() noexcept { return pool_; }
		const vk::DescriptorPool handle() const noexcept { return pool_; }
		explicit operator vk::DescriptorPool() noexcept { return handle(); }
		explicit operator const vk::DescriptorPool() const noexcept { return handle(); }

		SetHandle request();
		void release(SetHandle);
		void setSize(size_t size);

		size_t capacity() const;
		size_t acquiredCount() const;
		void shrinkToFit(bool exact = false);
	};

}
