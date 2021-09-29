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

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>

/* Because of these arbitrary macros that might be forgotten
 * to add, all .cpp files that would include GLM headers
 * should include this one instead. */
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>



namespace vka2 {

	/** Simple POD for application managed vk::Buffer allocations. */
	struct BufferAlloc {
		vk::Buffer handle;
		VmaAllocation alloc;
	};

	/** Simple POD for application managed vk::Image allocations. */
	struct ImageAlloc {
		vk::Image handle;
		VmaAllocation alloc;
	};


	struct Queues {
		struct FamilyIndices {
			unsigned compute, transfer, graphics;
		};
		vk::Queue compute, transfer, graphics;
	};


	struct Vertex {
		using index_t = uint32_t;
		const static vk::IndexType INDEX_TYPE = vk::IndexType::eUint32;

		const static vk::VertexInputBindingDescription BINDING_DESC;
		const static std::array<vk::VertexInputAttributeDescription, 6> ATTRIB_DESC;

		glm::vec3 pos;
		glm::vec3 nrm;
		glm::vec3 nrm_smooth; // Always smoothed normal, required for the outline
		glm::vec3 tanu; // Tangent, or tangent aligned with U axis
		glm::vec3 tanv; // Bitangent, or tangent aligned with V axis
		glm::vec2 tex;

		inline bool operator==(const Vertex& r) const {
			return
				(r.pos == pos) &&
				(r.nrm == nrm) &&
				(r.nrm_smooth == nrm_smooth) &&
				(r.tanu == tanu) &&
				(r.tanv == tanv) &&
				(r.tex == tex);
		}
	};

	struct Instance {
		using index_t = uint32_t;
		const static vk::IndexType INDEX_TYPE = vk::IndexType::eUint32;

		const static vk::VertexInputBindingDescription BINDING_DESC;
		const static std::array<vk::VertexInputAttributeDescription, 6> ATTRIB_DESC;

		glm::mat4  modelTransf;
		glm::vec4  colorMul;
		float      rnd;

		inline bool operator==(const Instance& r) const {
			return
				(r.modelTransf == modelTransf) &&
				(r.colorMul == colorMul) &&
				(r.rnd == rnd);
		}
	};

	using Vertices = std::vector<Vertex>;
	using Instances = std::vector<Instance>;
	using Indices = std::vector<Vertex::index_t>;



	#define SPIRV_ALIGNED(_T) alignas(spirv::align<_T>) _T

	namespace spirv {

		template<typename T>
		constexpr unsigned align;

		template<> constexpr unsigned align<bool>      =  1;
		template<> constexpr unsigned align<int32_t>   =  4;
		template<> constexpr unsigned align<uint32_t>  =  4;
		template<> constexpr unsigned align<float>     =  4;
		template<> constexpr unsigned align<double>    =  8;
		template<> constexpr unsigned align<glm::vec2> =  8;
		template<> constexpr unsigned align<glm::vec3> = 16;
		template<> constexpr unsigned align<glm::vec4> = 16;
		template<> constexpr unsigned align<glm::mat4> = 16;

	}



	namespace ubo {

		/* The static Uniform Buffer Object is expected to change
		 * very infrequently throughout the render pass' lifetime. */
		struct Static {
			static constexpr bool dma = false;
			static constexpr unsigned set = 0;
			static constexpr unsigned binding = 0;
			SPIRV_ALIGNED(glm::mat4)  projTransf;
			SPIRV_ALIGNED(float)      outlineSize; // Measured in world units
			SPIRV_ALIGNED(float)      outlineDepth; // Scales with zNear, unfortunately
			SPIRV_ALIGNED(float)      outlineRnd; // Random factor for outline vertex positions
		};

		/* The model Uniform Buffer Object holds data that only needs to be
		 * updated when a model is loaded; it shares the descriptor set
		 * with combined image samplers for textures. */
		struct Model {
			static constexpr bool dma = true;
			static constexpr unsigned set = 1;
			static constexpr unsigned binding = 0;
			SPIRV_ALIGNED(float)      ambient;
			SPIRV_ALIGNED(float)      diffuse;
			SPIRV_ALIGNED(float)      specular;
			SPIRV_ALIGNED(float)      shininess;
			SPIRV_ALIGNED(float)      rnd; // Different for every model
			SPIRV_ALIGNED(unsigned)   celLevels;
		};

		/* The frame Uniform Buffer Object, as the name implies, is updated
		 * every frame. Host visible memory is basically guaranteed. */
		struct Frame {
			static constexpr bool dma = true;
			static constexpr unsigned set = 2;
			static constexpr unsigned binding = 0;
			SPIRV_ALIGNED(glm::mat4)  viewTransf;
			SPIRV_ALIGNED(glm::vec3)  viewPos; // Should be consistent with the matrix transform for proper specular lighting
			SPIRV_ALIGNED(glm::vec4)  pointLight; // Position XYZ, intensity
			SPIRV_ALIGNED(glm::vec3)  lightDirection;
			SPIRV_ALIGNED(float)      rnd; // Different for every frame
			SPIRV_ALIGNED(uint32_t)   shaderSelector;
		};

	}

	#undef SPIRV_ALIGNED

}
