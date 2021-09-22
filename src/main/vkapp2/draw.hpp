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



/* This file contains engine-specific parameters or
 * structures eventually used by the programmable shaders.
 *
 * It is similar to pod.hpp, but the latter is included in graphics.hpp
 * and modifying it will cause many more translation units to be recompiled. */

#pragma once

#include <vulkan/vulkan.hpp>

#include <array>

#include "pod.hpp"



namespace vka2 {

	inline namespace consts {

		constexpr bool USE_LINEAR_MIPMAPS = true;
		constexpr unsigned MAX_MIP_LEVELS = 1<<5;
		constexpr float LOD_BIAS = 0.05f;

		constexpr unsigned MAX_CONCURRENT_FRAMES = 3;

		constexpr vk::ImageTiling IMAGE_TILING = vk::ImageTiling::eOptimal;

		constexpr vk::CompareOp DEPTH_CMP_OP = vk::CompareOp::eLessOrEqual;

		constexpr unsigned MAX_PUSH_CONST_BYTES = 128; // As required by the Vulkan 1.2 spec; used for compile-time assertions

		/** Defines the order in which present modes are
		 * attempted to be used, from best to worst. */
		constexpr std::array<vk::PresentModeKHR, 4> PRESENT_MODE_PREFERENCE = {
			// vk::PresentModeKHR::eFifo, // This is the fallback mode; it must be either at the top or absent
			vk::PresentModeKHR::eSharedDemandRefresh,
			vk::PresentModeKHR::eSharedContinuousRefresh,
			vk::PresentModeKHR::eMailbox,
			vk::PresentModeKHR::eFifoRelaxed
		};

		/** Defines the order in which composite alpha parameters are
		 * attempted to be used, from best to worst. */
		constexpr std::array<vk::CompositeAlphaFlagBitsKHR, 3> COMPOSITE_ALPHA_PREFERENCE = {
			// vk::CompositeAlphaFlagBitsKHR::eOpaque, // This is the fallback mode; it must be either at the top or absent
			vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
			vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
			vk::CompositeAlphaFlagBitsKHR::eInherit
		};

		/** Defines the order in which depth/stencil image formats
		 * are attempted to be used, from best to worst. */
		constexpr std::array<vk::Format, 3> DEPTH_STENCIL_FMT_PREFERENCE = {
			// No guaranteed fallback
			vk::Format::eD32SfloatS8Uint,
			vk::Format::eD24UnormS8Uint,
			vk::Format::eD16UnormS8Uint
		};

		/** Defines the order in which depth image formats
		 * are attempted to be used, from best to worst. */
		constexpr std::array<vk::Format, 6> DEPTH_ONLY_FMT_PREFERENCE = {
			// No guaranteed fallback
			vk::Format::eD32Sfloat,
			vk::Format::eD24UnormS8Uint,
			vk::Format::eD16Unorm,
			vk::Format::eD16UnormS8Uint,
			vk::Format::eD32SfloatS8Uint,
			vk::Format::eX8D24UnormPack32
		};



		namespace push_const {

			#define SPIRV_ALIGNED(_T) alignas(spirv::align<_T>) _T
			#define ASSERT_SIZE(_T) static_assert(sizeof(_T) < MAX_PUSH_CONST_BYTES)

			struct Object {
				static constexpr bool unused = true;
			};

			ASSERT_SIZE(Object);

			#undef ASSERT_SIZE
			#undef SPIRV_ALIGNED

		}

	}

}
