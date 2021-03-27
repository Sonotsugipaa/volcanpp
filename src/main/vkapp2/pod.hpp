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

		bool operator==(const Vertex& r) const {
			return 0 == memcmp(this, &r, sizeof(Vertex)); }
	};

	using Vertices = std::vector<Vertex>;
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
			constexpr static bool dma = false;
			constexpr static unsigned set = 0;
			constexpr static unsigned binding = 0;
			SPIRV_ALIGNED(glm::mat4)  projTransf;
			SPIRV_ALIGNED(float)      outlineSize; // Measured in world units
			SPIRV_ALIGNED(float)      outlineDepth; // Scales with zNear, unfortunately
			SPIRV_ALIGNED(uint32_t)   lightLevels;
		};


		/* The model Uniform Buffer Object holds data that only needs to be
		 * updated when a model is loaded; it shares the descriptor set
		 * with combined image samplers for textures. */
		struct Model {
			constexpr static bool dma = true;
			constexpr static unsigned set = 1;
			constexpr static unsigned binding = 0;
			SPIRV_ALIGNED(float)      minDiffuse;
			SPIRV_ALIGNED(float)      maxDiffuse;
			SPIRV_ALIGNED(float)      minSpecular;
			SPIRV_ALIGNED(float)      maxSpecular;
			SPIRV_ALIGNED(float)      rnd; // Different for every model
		};

		/* The frame Uniform Buffer Object, as the name implies, is updated
		 * every frame. Host visible memory is basically guaranteed. */
		struct Frame {
			constexpr static bool dma = true;
			constexpr static unsigned set = 2;
			constexpr static unsigned binding = 0;
			SPIRV_ALIGNED(glm::mat4)  viewTransf;
			SPIRV_ALIGNED(glm::vec3)  lightDirection;
			SPIRV_ALIGNED(float)      rnd; // Different for every frame
			SPIRV_ALIGNED(uint32_t)   shaderSelector;
		};

	}

	#undef SPIRV_ALIGNED

}
