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

#include "util/util.hpp"

#include "vkapp2/settings/options.hpp"
#include "vkapp2/runtime.hpp"
#include "vkapp2/pod.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>

#include <functional>



/* Macros used to expose private members in classes *//**/

#define GETTER_REF_CONST(_F, _M)  const decltype(_F)& _M()const{return _F;}
#define GETTER_REF(_F, _M)  decltype(_F)& _M(){return _F;}  GETTER_REF_CONST(_F, _M)
#define GETTER_PTR_CONST(_F, _M)  const std::remove_reference<decltype(*_F)>::type* _M()const{return _F;}
#define GETTER_PTR(_F, _M)  std::remove_reference<decltype(*_F)>::type* _M(){return _F;}  GETTER_PTR_CONST(_F, _M)
#define GETTER_VAL_CONST(_F, _M) const decltype(_F) _M()const{return _F;}
#define GETTER_VAL(_F, _M) decltype(_F) _M()const{return _F;}



namespace vka2 {

	constexpr auto VK_API_VERSION = VK_API_VERSION_1_2;

	using DescSetBindings = std::array<std::vector<vk::DescriptorSetLayoutBinding>, 3UL>;
	const DescSetBindings DESCSET_BINDINGS;


	std::pair<
			vk::PhysicalDevice, vk::PhysicalDeviceFeatures
	> selectPhysicalDevice(vk::Instance);


	/** A wrapper around vk::Device::waitForFences that throws an exception
	 * when a non-success result is returned. */
	void tryWaitForFences(vk::Device dev,
		const vk::ArrayProxy<const vk::Fence>&,
		vk::Bool32 waitAll, uint64_t timeout);


	/* Forward declarations for better dependency injection *//**/
	class Application;
	class RenderPass;


	/* Used by Model to temporarily expose
	 * mmapped device-local buffers *//**/
	template<typename _datum_t>
	struct MemoryView {
		using datum_t = _datum_t;

		datum_t* data;
		size_t size; // Bytes

		MemoryView(datum_t* data, size_t size):
				data(data), size(size)
		{
			assert((size % sizeof(datum_t) == 0) && "Size of mmap must be a multiple of the datum type");
		}
	};


	class Texture {
		Application* _app; // Dependency injection
		ImageAlloc _img;
		vk::ImageView _img_view;
		vk::Sampler _sampler; // 1 sampler per model shouldn't be bad practice, or is it?

	public:
		enum class Usage {
			eColor, eNormal, eSpecular
		};

		static constexpr unsigned samplerDescriptorSet = ubo::Model::set;
		static constexpr std::array<unsigned, 2> samplerDescriptorBindings = { 1, 2 }; // Diffuse, Normal (+ Reflect in the near future)

		struct Data {
			unsigned width, height, channels;
			unsigned mipLevels;
			size_t size;
			void* data; // Must be allocated/deallocated externally
			vk::Format dataFormat;
		};

		static Texture fromPngFile(Application&,
			const std::string& path, bool linearFiltering = false);

		static Texture singleColor(Application&,
			glm::vec3 rgb, bool linearFiltering = false);
		static Texture singleColor(Application&,
			glm::vec4 rgba, bool linearFiltering = false);
		static Texture singleColor(Application&,
			std::array<uint8_t, 3> rgb, bool linearFiltering = false);
		static Texture singleColor(Application&,
			std::array<uint8_t, 4> rgba, bool linearFiltering = false);

		Texture();
		Texture(Application&, const Data&, bool linearFilter);

		Texture(Texture&&);

		~Texture();

		Texture& operator=(Texture&&);

		GETTER_REF(_app,       application)
		GETTER_REF(_img,       imgBuffer  )
		GETTER_REF(_img_view,  imgView    )
		GETTER_VAL(_sampler,   sampler    )
	};


	struct Material {
		using ShPtr = std::shared_ptr<Material>;

		Texture colorTexture;
		Texture normalTexture;
		float minDiffuse, maxDiffuse;
		float minSpecular, maxSpecular;
	};


	class Model {
	public:
		using ShPtr = std::shared_ptr<Model>;
		using UboType = ubo::Model;
		using MaterialCache = std::map<std::string, Material::ShPtr>;
		using ModelCache = std::map<std::string, Model::ShPtr>;

	private:
		Application* _app; // Dependency injection
		BufferAlloc _vtx;  Vertex::index_t _vtx_count;
		BufferAlloc _idx;  Vertex::index_t _idx_count;
		BufferAlloc _ubo;
		Material::ShPtr _mat;

	public:
		struct ObjSources {
			std::string mdlName; // Used for caches:
			std::string objPath;
			std::function<Texture (Texture::Usage)> textureLoader;
			std::function<void (Vertices&, Indices&)> postAssembly;
		};


		/** Load a model from OBJ format data into the specified model
		 * caches. If `mergeVertices` is true, identical vertices will
		 * be merged together: normals between merged faces/edges are
		 * interpolated, where each face would have identical normals
		 * otherwise.
		 *
		 * Cache parameters are pointers, as they're optional: if a cache is
		 * not nullptr, the function attempts to reuse an existing material/model
		 * instead of creating a redundant one; otherwise, the returned shared
		 * pointer will always have a unique Model with unique textures. */
		static ShPtr fromObj(
			Application& application,
			const ObjSources& sources,
			bool mergeVertices,
			ModelCache* mdlCache = nullptr,
			MaterialCache* matCache = nullptr);


		Model();
		Model(Application&, const Vertices&, const Indices&, Material::ShPtr);

		Model(Model&&);

		~Model();

		Model& operator=(Model&&);

		GETTER_REF(_app,       application)
		GETTER_REF(_vtx,       vtxBuffer  )
		GETTER_VAL(_vtx_count, vtxCount   )
		GETTER_REF(_idx,       idxBuffer  )
		GETTER_VAL(_idx_count, idxCount   )
		GETTER_REF(_ubo,       uboBuffer  )

		inline const Material& material() const { return *_mat.get(); }

		/** Convenience function to allocate one or more descriptor sets, then */
		std::vector<vk::DescriptorSet> makeDescriptorSets(
			vk::DescriptorPool, vk::DescriptorSetLayout, unsigned count = 1);

		/** Maps the model's vertices and indices to a range of addresses, then runs the
		 * given function.
		 *
		 * This operation may require a memory copy to a temporary host visible device buffer.
		 * The function must return `true` if mapped data has been altered, otherwise the
		 * changes may be discarded.
		 *
		 * The sizes (in bytes) of allocated memory are guaranteed to be respective
		 * multiples of `sizeof(Vertex)` and `sizeof(Vertex::index_t)`. */
		void viewVertices(std::function<bool (*)(MemoryView<Vertex>, MemoryView<Vertex::index_t>)>);

		/** Maps the model's UBO to a range of addresses, then runs the
		 * given function.
		 *
		 * This operation may require a memory copy to a temporary host visible device buffer.
		 * The function must return `true` if mapped data has been altered, otherwise the
		 * changes may be discarded.
		 *
		 * The size (in bytes) of allocated memory is guaranteed to be a multiple of
		 * `sizeof(UboType)`. */
		void viewUbo(std::function<bool (MemoryView<UboType>)>);
	};


	class CommandPool {
		vk::Device* _dev;
		vk::CommandPool _pool;
		vk::Fence _fence_shared;

	public:
		CommandPool() = default;
		CommandPool(vk::Device&, unsigned queueFamilyIndex, bool transientCommands);
		void destroy();

		void reset(bool doReleaseResources = false);

		void runCmds(
			vk::Queue,
			std::function<void (vk::CommandBuffer)>,
			vk::Fence = nullptr);

		GETTER_VAL(_pool, handle)
	};


	class AbstractSwapchain {
	public:
		Application* application;  // Dependency injection / non-owning pointer
		vk::SwapchainKHR handle;
		struct data_t {
			vk::Extent2D extent;
			std::vector<vk::Image> images;
		} data;


		AbstractSwapchain() = default;

		AbstractSwapchain(
			Application&,
			const vk::Extent2D& desiredExtent,
			unsigned short maxConcurrentFrames,
			vk::SwapchainKHR cached = nullptr);

		/** Destroys the swapchain.
		 * @param keep_handle Whether to keep the old vk::SwapchainKHR
		 * instead of destroying it.
		 * @returns The old swapchain handle, if \p keep_handle is true; nullptr otherwise. */
		vk::SwapchainKHR destroy(bool keep_handle);
	};


	class Pipeline {
		friend RenderPass;

		RenderPass* _rpass;
		struct data_t {
			vk::Pipeline handle;
			vk::ShaderModule vtxShader;
			vk::ShaderModule frgShader;
		} _data;

	public:
		Pipeline();

		Pipeline(
			RenderPass&,
			const std::string& vtxShader, const std::string& frgShader,
			const char* shaderEntryPoint, unsigned subpassIndex,
			bool invertCullFace, vk::Extent2D extent,
			vk::SampleCountFlagBits sampleCount);

		void destroy();

		inline bool isNull() const { return _rpass == nullptr; }
		inline operator bool() const { return ! isNull(); }
		inline bool operator!() const { return ! operator bool(); }

		GETTER_REF(_data.handle,    handle   )
		GETTER_REF(_data.vtxShader, vtxShader)
		GETTER_REF(_data.frgShader, frgShader)
	};


	/** A RenderPass is the process of rendering to a surface's
	 * images: when swapchains are destroyed (thus invalid),
	 * RenderPasses associated to them are invalid too, until
	 * a new swapchain is assigned.
	 *
	 * Unlike swapchains, RenderPasses can be reassigned instead
	 * of needing to be destroyed and constructed back:
	 * some data can be preserved, and some reinitializations
	 * can be omitted. This has to be done explicitly through
	 * the `reassign` function.
	 *
	 * A RenderPass instance is not meant to be shared by multiple threads.
	 * That means every non-const member function is to be considered
	 * non-reentrant: every call on them must either occur on the same thread, or be
	 * externally synchronized with each other. */
	class RenderPass {
		friend Pipeline;
	public:
		struct ImageData {
			ImageAlloc renderTarget, resolveTarget;
			vk::ImageView renderTargetView;
			vk::ImageView resolveTargetView;
			vk::Framebuffer framebuffer;
			vk::CommandPool cmdPool;
			std::array<vk::CommandBuffer, 2> cmdBuffer; // [0] Render pass, [1] blit to present
			std::array<vk::CommandBuffer, 2> secondaryDrawBuffers; // One for each subpass
			BufferAlloc frameUbo;
			BufferAlloc staticUbo;
			unsigned long staticUboWrCounter;
			vk::Fence fenceStaticUboUpToDate;
			vk::Fence fenceImgAvailable;
			vk::DescriptorSet staticDescSet;
			vk::DescriptorSet frameDescSet;
		};

		struct FrameData {
			vk::Semaphore imgAcquiredSem; // Signaled when a swapchain image has been acquired
			vk::Semaphore renderDoneSem; // Signaled when the render pass ended
			vk::Semaphore blitToSurfaceDoneSem; // Signaled when the render target has been transfered to the swapchain image
		};

		class FrameHandle {
		public:
			RenderPass& rpass;
			RenderPass::FrameData& frameData;
			RenderPass::ImageData& imageData;

			void updateModelDescriptors(const Model&, vk::DescriptorSet);
			void bindModelDescriptorSet(vk::CommandBuffer, vk::DescriptorSet);
		};
		friend FrameHandle;

		using ImageRef = std::pair<vk::Image, ImageData>;

		using SwapchainOutdatedCallback = std::function<void (RenderPass&)>;

		using PreRenderFunction = std::function<void (FrameHandle&)>;
		using PostRenderFunction = std::function<void (FrameHandle&)>;
		using RenderFunction = std::function<void (FrameHandle&, vk::CommandBuffer)>;

	private:
		AbstractSwapchain* _swapchain;
		struct data_t {
			vk::Extent2D renderExtent;
			std::vector<ImageRef> swpchnImages;
			vk::PipelineLayout pipelineLayout;
			std::vector<vk::DescriptorSetLayout> descsetLayouts;
			vk::RenderPass handle;
			BufferAlloc staticUboBase; // Copy-on-read behavior for ImageData, only when needed
			unsigned long staticUboBaseWrCounter; // ++ on every staticUboBase write; ImageData should get a copy when its counter doesn't match.
			std::vector<FrameData> frames;
			vk::DescriptorPool descPool;
			ImageAlloc depthStencilImg;
			vk::ImageView depthStencilImgView;
			bool useMultisampling;
		} _data;
		struct rendering_t {
			uint_fast32_t frame;
			bool skipNextFrame; // Desperate attempt to fix swapchain rebuilds causing crashes
		} _rendering = { };

		void _assign(AbstractSwapchain&);
		void _unassign();

	public:
		/** This callback controls what happens when the swapchain
		 * becomes invalid.
		 *
		 * Most of the times, this happens when the surface is resized:
		 * thus, the default behavior is to simply rebuild the swapchain,
		 * then recreating everything that depends on it. */
		SwapchainOutdatedCallback swapchainOutdatedCallback;

		RenderPass();
		RenderPass(RenderPass&&) = default;

		RenderPass(
			AbstractSwapchain&,
			vk::Extent2D renderExtent,
			unsigned short maxConcurrentFrames,
			bool useMultisampling,
			SwapchainOutdatedCallback);

		RenderPass& operator=(RenderPass&&) = default;

		void destroy();

		GETTER_REF      (_data.swpchnImages,   swapchainImages     )
		GETTER_PTR      (_swapchain,           swapchain           )
		GETTER_VAL_CONST(_data.handle,         handle              )
		GETTER_REF      (_data.descPool,       descriptorPool      )
		GETTER_REF      (_data.pipelineLayout, pipelineLayout      )
		GETTER_REF      (_data.descsetLayouts, descriptorSetLayouts)
		GETTER_REF_CONST(_data.renderExtent,   renderExtent        )

		void reassign(AbstractSwapchain&);
		void reassign(AbstractSwapchain&, const vk::Extent2D& renderExtent);

		void waitIdle(uint64_t timeoutNs = UINT64_MAX);

		void setStaticUbo(const ubo::Static&);

		/** @returns `true` iif the render pass was executed successfully.
		 * If the operation is unsuccessful, the state of the pipeline is
		 * not necessarily invalid; most of the time, the swapchain becomes
		 * invalid due to the surface being resized.
		 *
		 * Calls to this function must NOT be done asynchronously. */
		bool runRenderPass(
			const ubo::Frame& frameUbo,
			PreRenderFunction /* Can be `{ }` */,
			PostRenderFunction /* Can be `{ }` */,
			std::array<RenderFunction, 2> /* Main pipeline, outline pipeline */);

	};


	/** Not RAII compliant.
	 *  - Copyable: no (the copy takes the ownership of the resources)
	 *  - Moveable: yes */
	class Application {
		vk::ApplicationInfo _vk_appinfo;
		vk::Instance _vk_instance;
		struct data_t {
			vk::PhysicalDevice pDev;
			vk::PhysicalDeviceFeatures pDevFeatures;
			Queues::FamilyIndices qFamIdx;
			decltype(Queues::FamilyIndices::compute) qFamIdxPresent;
			Queues queues;
			vk::Queue presentQueue;
			vk::Device dev;
			VmaAllocator alloc;
			CommandPool transferCmdPool, graphicsCmdPool;
			GLFWwindow* glfwWin;
			vk::SurfaceKHR surface;
			vk::SurfaceCapabilitiesKHR surfaceCapabs;
			vk::SurfaceFormatKHR surfaceFmt;
			AbstractSwapchain swapchain;
			Options options;
			Runtime runtime;
		} _data;
		struct cache_t {
			mutable std::map<vk::Format, vk::FormatProperties> fmtProps;
		} _cache;
		vk::SwapchainKHR _cached_swapchain;

	public:
		Application();
		void destroy();

	private:
		void _create_window(bool fullscreen, const vk::Extent2D&);
		void _destroy_window();

		void _create_swapchain();
		void _destroy_swapchain(bool keep_cached);

		void* _mmap_buffer(VmaAllocation&);
	public:

		void run();

		/* After a call to this, all references to this object's
		 * old swapchain are invalid. *//**/
		void rebuildSwapChain();

		/* After a call to this, all references to this object's
		 * old swapchain are invalid: Application::rebuildSwapChain
		 * is called. *//**/
		void setWindowMode(bool fullscreen, const vk::Extent2D& newExtent);


		const vk::FormatProperties& getFormatProperties(vk::Format) const noexcept;


		void stageBufferData(
			vk::Buffer dst, const void* srcPtr, size_t srcSizeBytes);


		BufferAlloc createBuffer(
			const vk::BufferCreateInfo&,
			vk::MemoryPropertyFlags requiredFlags,
			vk::MemoryPropertyFlags preferedFlags = { },
			vk::MemoryPropertyFlags disallowedFlags = { });

		BufferAlloc createBuffer(
			const vk::BufferCreateInfo&,
			VmaMemoryUsage memUsage,
			vk::MemoryPropertyFlags disallowedFlags = { });

		void destroyBuffer(BufferAlloc&);


		ImageAlloc createImage(
			const vk::ImageCreateInfo&,
			vk::MemoryPropertyFlags requiredFlags,
			vk::MemoryPropertyFlags preferedFlags = { },
			vk::MemoryPropertyFlags disallowedFlags = { });

		void destroyImage(ImageAlloc&);


		template<typename Ref>
		Ref* mapBuffer(VmaAllocation& allocation) {
			return reinterpret_cast<Ref*>(_mmap_buffer(allocation)); }

		void unmapBuffer(VmaAllocation&);


		GETTER_VAL      (_vk_instance,          vulkanInstance         )
		GETTER_VAL      (_data.pDev,            physDevice             )
		GETTER_VAL      (_data.dev,             device                 )
		GETTER_VAL      (_data.alloc,           allocator              )
		GETTER_REF_CONST(_data.queues,          queues                 )
		GETTER_REF_CONST(_data.qFamIdx,         queueFamilyIndices     )
		GETTER_VAL      (_data.qFamIdxPresent,  presentQueueFamilyIndex)
		GETTER_VAL      (_data.presentQueue,    presentQueue           )
		GETTER_REF      (_data.transferCmdPool, transferCommandPool    )
		GETTER_REF      (_data.graphicsCmdPool, graphicsCommandPool    )
		GETTER_REF      (_data.glfwWin,         glfwWindow             )
		GETTER_REF      (_data.swapchain,       swapchain              )
		GETTER_VAL      (_data.surface,         surface                )
		GETTER_REF_CONST(_data.surfaceCapabs,   surfaceCapabilities    )
		GETTER_REF_CONST(_data.surfaceFmt,      surfaceFormat          )
		GETTER_REF      (_data.options,         options                )
		GETTER_REF_CONST(_data.runtime,         runtime                )
	};

}



#undef GETTER_REF_CONST
#undef GETTER_REF
#undef GETTER_PTR_CONST
#undef GETTER_PTR
#undef GETTER_VAL_CONST
#undef GETTER_VAL
