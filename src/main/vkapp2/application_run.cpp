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

#include <filesystem>
#include <random>
#include <set>
#include <fstream>

#include "vkapp2/draw.hpp"
#include "vkapp2/constants.hpp"

#include "vkapp2/settings/options.hpp"
#include "vkapp2/settings/scene.hpp"

using namespace vka2;
using namespace std::string_literals;



namespace {

	/* Negative keycodes are interpreted as negative mouse codes,
	 * so -GLFW_MOUSE_BUTTON_LEFT == keycode(-GLFW_MOUSE_BUTTON_LEFT); *
	 * this is clearly malpractice and possibly harmful, and should be *
	 * discarded for a new project. But hey, this was a long day for me. */
	using keycode_t = int;
	using KeyBinding = std::function<void (bool isPressed, unsigned modifiers)>;
	using Keymap = std::map<keycode_t, KeyBinding>;


	struct DeviceVectorTraits {
		vk::BufferUsageFlags bufferUsage;
		vk::MemoryPropertyFlags vmaRequiredFlags;
		vk::MemoryPropertyFlags vmaPreferredFlags;
		VmaMemoryUsage vmaMemoryUsage;
	};


	template<typename T>
	class DeviceVector {
	private:
		DeviceVectorTraits traits_;
		Application* app_;
		T* cpuDataPtr_;
		vk::DeviceSize dataSize_;
		vk::DeviceSize dataCapacity_;
		BufferAlloc cpuDataBuffer_;
		BufferAlloc devDataBuffer_;

		void allocWithTraits_(vk::DeviceSize n) {
			assert(app_ != nullptr);
			assert(cpuDataPtr_ == nullptr);
			util::alloc_tracker.alloc("::DeviceVector<"s + std::to_string(sizeof(T)) + "B>::T"s, n);
			{
				auto cpuBcInfo = vk::BufferCreateInfo(
					{ }, n * sizeof(T),
					vk::BufferUsageFlagBits::eTransferSrc,
					vk::SharingMode::eExclusive);
				cpuDataBuffer_ = app_->createBuffer(cpuBcInfo, VMA_MEMORY_USAGE_CPU_ONLY);
				cpuDataPtr_ = app_->template mapBuffer<T>(cpuDataBuffer_.alloc);
			} {
				auto devBcInfo = vk::BufferCreateInfo(
					{ }, n * sizeof(T),
					traits_.bufferUsage | vk::BufferUsageFlagBits::eTransferDst,
					vk::SharingMode::eExclusive);
				if(traits_.vmaMemoryUsage == VMA_MEMORY_USAGE_UNKNOWN) {
					devDataBuffer_ = app_->createBuffer(devBcInfo, traits_.vmaRequiredFlags, traits_.vmaPreferredFlags);
				} else {
					devDataBuffer_ = app_->createBuffer(devBcInfo, traits_.vmaMemoryUsage);
				}
			}
			dataCapacity_ = n;
		}

		void dealloc_(
				RenderPass* rpass,
				VmaAllocation vmaAlloc,
				BufferAlloc& cpuBuffer, BufferAlloc& devBuffer,
				vk::DeviceSize capacity
		) {
			util::alloc_tracker.dealloc("::DeviceVector<"s + std::to_string(sizeof(T)) + "B>::T"s, capacity);
			if(rpass == nullptr) {
				app_->device().waitIdle();
			} else {
				rpass->waitIdle();
			}
			app_->unmapBuffer(vmaAlloc);
			app_->destroyBuffer(cpuBuffer);
			app_->destroyBuffer(devBuffer);
		}

		void dealloc_(
				RenderPass* rpass
		) {
			assert(app_ != nullptr);
			cpuDataPtr_ = nullptr;
			dealloc_(rpass, cpuDataBuffer_.alloc, cpuDataBuffer_, devDataBuffer_, dataCapacity_);
		}

	public:
		DeviceVector() { }

		DeviceVector(Application& app, const DeviceVectorTraits& traits):
				traits_(traits), app_(&app),
				cpuDataPtr_(nullptr),
				dataSize_(0), dataCapacity_(0)
		{ }

		DeviceVector(const DeviceVector& cp):
				traits_(cp.traits_), app_(cp.app_),
				dataSize_(cp.dataSize_)
		{
			if(app_ != nullptr && dataSize_ != 0) {
				allocWithTraits_(dataSize_);
				memcpy(cpuDataPtr_, cp.cpuDataPtr_, cp.dataSize_ * sizeof(T));
			}
		}

		DeviceVector(DeviceVector&& mv):
				traits_(std::move(mv.traits_)),
				app_(std::move(mv.app_)),
				cpuDataPtr_(std::move(mv.cpuDataPtr_)),
				dataSize_(std::move(mv.dataSize_)),
				cpuDataBuffer_(std::move(mv.cpuDataBuffer_)),
				devDataBuffer_(std::move(mv.devDataBuffer_))
		{
			assert(app_ != nullptr);
			mv.app_ = nullptr;
		}

		~DeviceVector() {
			if(app_ != nullptr) {
				if(dataSize_ != 0) {
					dealloc_(nullptr);
				}
				app_ = nullptr;
			}
		}

		DeviceVector& operator=(const DeviceVector& cp) { this->~DeviceVector(); return *new (this) DeviceVector(cp); }
		DeviceVector& operator=(DeviceVector&& mv) { this->~DeviceVector(); return *new (this) DeviceVector(mv); }


		T* begin() { return cpuDataPtr_; }
		const T* begin() const { return cpuDataPtr_; }
		T* end() { return cpuDataPtr_ + dataSize_; }
		const T* end() const { return cpuDataPtr_ + dataSize_; }

		vk::DeviceSize size() const { return dataSize_; }
		vk::DeviceSize capacity() const { return dataCapacity_; }

		void resizeExact(RenderPass* rpass, vk::DeviceSize newSize, vk::DeviceSize newCapacity) {
			if(newSize == 0) {
				if(dataCapacity_ != 0) dealloc_(rpass);
			} else {
				if(newCapacity != dataCapacity_) {
					auto old_dataCapacity = dataCapacity_;
					auto old_cpuDataPtr = cpuDataPtr_;
					auto old_cpuDataBuffer = cpuDataBuffer_;
					auto old_devDataBuffer = devDataBuffer_;
					cpuDataPtr_ = nullptr;
					allocWithTraits_(newCapacity);
					memcpy(old_cpuDataPtr, cpuDataPtr_, std::min(dataSize_, newSize));
					dataCapacity_ = newCapacity;
					if(old_dataCapacity != 0) {
						dealloc_(rpass, old_cpuDataBuffer.alloc, old_cpuDataBuffer, old_devDataBuffer, old_dataCapacity);
					}
				}
				dataSize_ = newSize;
			}
		}

		void resize(RenderPass* rpass, vk::DeviceSize newSize) {
			if(newSize != 0) {
				if(newSize > dataSize_) {
					vk::DeviceSize pow = 1;
					while(pow < newSize) pow *= 2;
					resizeExact(rpass, newSize, pow);
				}
			}
		}

		void push_back(T v) {
			resize(dataSize_ + 1);
			cpuDataPtr_[dataSize_-1] = std::move(v);
		}

		T& operator[](vk::DeviceSize i) { assert(i < dataSize_); return cpuDataPtr_[i]; }
		const T& operator[](vk::DeviceSize i) const { assert(i < dataSize_); return cpuDataPtr_[i]; }


		const BufferAlloc& devBuffer() const { assert(app_ != nullptr); return devDataBuffer_; }

		[[nodiscard]]
		CommandPool::BufferHandle flushAsync(vk::Fence fence, vk::DeviceSize beg, vk::DeviceSize end) {
			assert(beg <= dataSize_);
			assert(end <= dataSize_);
			assert(beg <= end);
			if(beg < end) {
				vk::BufferCopy cp;
				auto& cmdPool = app_->transferCommandPool();
				cp.srcOffset = cp.dstOffset = beg * sizeof(T);
				cp.setSize((end - beg) * sizeof(T));
				if(fence) {
					return cmdPool.runCmdsAsync(app_->queues().transfer, [&](vk::CommandBuffer cmd) {
						cmd.copyBuffer(cpuDataBuffer_.handle, devDataBuffer_.handle, cp);
					}, fence);
				} else {
					cmdPool.runCmds(app_->queues().transfer, [&](vk::CommandBuffer cmd) {
						cmd.copyBuffer(cpuDataBuffer_.handle, devDataBuffer_.handle, cp);
					});
				}
			}
			return { };
		}

		[[nodiscard]]
		CommandPool::BufferHandle flushAsync(vk::Fence fence) {
			return flushAsync(fence, 0, dataSize_);
		}

		void flush(vk::DeviceSize beg, vk::DeviceSize end) {
			auto cmdHandle = flushAsync(nullptr, beg, end);
		}

		void flush() {
			auto cmdHandle = flushAsync(nullptr);
		}
	};


	struct FrameTiming {
		/** How many seconds a CPU frame is supposed to last. */
		float frameTime;
	};


	struct CtrlSchemeContext {
		glm::vec3 fwdMoveVector; // Only stores positive XYZ input
		glm::vec3 bcwMoveVector; // Only stores negative XYZ input
		glm::vec2 rotate;
		glm::dvec2 lastCursorPos;
		unsigned shaderSelector;
		bool dragView;
		bool speedMod;
		bool toggleFullscreen;
		bool createObj;
		bool movePointLightMod;
	};


	struct GlfwContext {
		Keymap* keymap;
		Application* app;
		CtrlSchemeContext* ctrlCtx;
		FrameTiming* frameTiming;
	};


	/** A wrapper for vka2::Model, to associate it with a descriptor set.
	 * It also references a descriptor pool, in order to create and destroy
	 * sets. */
	class ModelWrapper {
		Model::ShPtr _mdl;
		vk::DescriptorPool _dPool;
		vk::DescriptorSet _dSet;
	public:
		ModelWrapper(): _mdl() { }

		ModelWrapper(Model::ShPtr mdl, vk::DescriptorPool dPool, vk::DescriptorSetLayout dSetLayout):
				_mdl(std::move(mdl))
		{
			recreateDescSet(dPool, dSetLayout);
		}

		ModelWrapper(ModelWrapper&& mov):
				_mdl(std::move(mov._mdl)),
				_dPool(std::move(mov._dPool)),
				_dSet(std::move(mov._dSet))
		{ }


		vk::DescriptorSet descSet() const { return _dSet; }

		void recreateDescSet(
				vk::DescriptorPool dPool,
				vk::DescriptorSetLayout dSetLayout
		) {
			_dPool = dPool;
			_dSet = _mdl->makeDescriptorSets(_dPool, dSetLayout, 1).front();
		}


		Model::ShPtr operator*() { return _mdl; }
		const Model::ShPtr operator*() const { return _mdl; }
		Model::ShPtr operator->() { return _mdl; }
		const Model::ShPtr operator->() const { return _mdl; }
	};


	struct Object {
		ModelWrapper mdlWr;
		glm::vec3 position;
		glm::vec3 orientation;
		glm::vec3 scale;
		glm::vec4 color;
		float rnd;
	};


	/* Structure containing what would be variables
	 * into the main function of interest. */
	struct RenderContext {
		FrameTiming frameTiming;
		CtrlSchemeContext ctrlCtx;
		GlfwContext glfwCtx;
		Keymap keymap;
		RenderPass rpass;
		Pipeline mainPipeline, outlinePipeline;
		Model::MaterialCache matCache;
		Model::ModelCache mdlCache;
		struct Shaders {
			std::string mainVtx, mainFrg;
			std::string outlineVtx, outlineFrg;
		} shaders;
		std::minstd_rand rng;
		std::uniform_real_distribution<float> rngDistr;
		std::vector<Object> objects;
		DeviceVector<Instance> instances;
		glm::vec4 pointLight;
		glm::vec3 lightDirection;
		glm::vec3 position;
		glm::vec2 orientation;
		unsigned frameCounter;
		float turnSpeedKey, turnSpeedKeyMod, moveSpeed, moveSpeedMod;
	};


	std::string get_shader_path() {
		const char* cEnvPath = getenv(SHADER_PATH_ENV_VAR_NAME);
		if(cEnvPath == nullptr) {
			util::logGeneral()
				<< SHADER_PATH_ENV_VAR_NAME << " env variable not set; using CWD" << util::endl;
			cEnvPath = ".";
		} else {
			util::logDebug() << "Reading shader files from \"" << cEnvPath << '"' << util::endl;
		}
		return cEnvPath;
	}


	std::string get_asset_path() {
		const char* cEnvPath = getenv(ASSET_PATH_ENV_VAR_NAME);
		if(cEnvPath == nullptr) {
			util::logGeneral()
				<< ASSET_PATH_ENV_VAR_NAME << " env variable not set; using CWD" << util::endl;
			cEnvPath = ".";
		} else {
			util::logDebug() << "Reading asset files from \"" << cEnvPath << '"' << util::endl;
		}
		std::filesystem::path absEnvPath = cEnvPath;
		absEnvPath = std::filesystem::absolute(absEnvPath);
		return absEnvPath.string();
	}


	[[maybe_unused]]
	vk::Extent2D fit_extent_lower(
			vk::Extent2D desiredMinimum, const vk::Extent2D& fitInto
	) {
		if(fitInto.width > fitInto.height) {
			desiredMinimum.width = fitInto.width * desiredMinimum.height / fitInto.height;
		} else {
			desiredMinimum.height = fitInto.height * desiredMinimum.width / fitInto.width;
		}
		return desiredMinimum;
	}

	vk::Extent2D fit_extent_height(
			decltype(vk::Extent2D::height) desiredHeight, const vk::Extent2D& fitInto
	) {
		desiredHeight = std::min(desiredHeight, fitInto.height);
		return {
			fitInto.width * desiredHeight / fitInto.height,
			desiredHeight};
	}


	template<typename DefaultValue>
	constexpr Texture rd_texture(
			Application& app, const std::string& textureFileName,
			bool nearestFilter, DefaultValue defaultValue
	) {
		if(std::filesystem::exists(textureFileName)) {
			return Texture::fromPngFile(app, textureFileName, ! nearestFilter);
		} else {
			util::logGeneral()
				<< "Texture file \"" << textureFileName
				<< "\" not found, using a fixed color" << util::endl;
			return Texture::singleColor(app, defaultValue, false);
		}
	}


	/** Key bindings will depend on the application AND the control scheme
	 * context, so they must be must stay alive as long as these key bindings
	 * can be used by GLFW. */
	Keymap mk_key_bindings(GLFWwindow*& win, CtrlSchemeContext* ctrlCtx) {
		Keymap km;

		#define MAP_KEY(_K) km[_K] = [ctrlCtx, &win]([[maybe_unused]] bool pressed, [[maybe_unused]] unsigned mod)

		MAP_KEY(GLFW_KEY_S) { ctrlCtx->fwdMoveVector.z = pressed? 1.0f : 0.0f; };
		MAP_KEY(GLFW_KEY_W) { ctrlCtx->bcwMoveVector.z = pressed? 1.0f : 0.0f; };
		MAP_KEY(GLFW_KEY_D) { ctrlCtx->fwdMoveVector.x = pressed? 1.0f : 0.0f; };
		MAP_KEY(GLFW_KEY_A) { ctrlCtx->bcwMoveVector.x = pressed? 1.0f : 0.0f; };
		MAP_KEY(GLFW_KEY_R) { ctrlCtx->bcwMoveVector.y = pressed? 1.0f : 0.0f; };
		MAP_KEY(GLFW_KEY_F) { ctrlCtx->fwdMoveVector.y = pressed? 1.0f : 0.0f; };
		MAP_KEY(GLFW_KEY_N) { if(!pressed) ctrlCtx->createObj = true; };
		MAP_KEY(GLFW_KEY_C) { std::quick_exit(1); };
		MAP_KEY(GLFW_KEY_LEFT_CONTROL) { ctrlCtx->movePointLightMod = pressed; };

		MAP_KEY(GLFW_KEY_ENTER) {
			if((! pressed) && (mod & GLFW_MOD_ALT)) {
				ctrlCtx->toggleFullscreen = true; }
		};

		MAP_KEY(GLFW_KEY_RIGHT) { ctrlCtx->rotate.x = pressed? +1.0f : 0.0f; };
		MAP_KEY(GLFW_KEY_LEFT)  { ctrlCtx->rotate.x = pressed? -1.0f : 0.0f; };
		MAP_KEY(GLFW_KEY_UP)    { ctrlCtx->rotate.y = pressed? +1.0f : 0.0f; };
		MAP_KEY(GLFW_KEY_DOWN)  { ctrlCtx->rotate.y = pressed? -1.0f : 0.0f; };

		MAP_KEY(GLFW_KEY_1) { if(!pressed) ctrlCtx->shaderSelector = 0; };
		MAP_KEY(GLFW_KEY_2) { if(!pressed) ctrlCtx->shaderSelector = 1; };
		MAP_KEY(GLFW_KEY_3) { if(!pressed) ctrlCtx->shaderSelector = 2; };
		MAP_KEY(GLFW_KEY_4) { if(!pressed) ctrlCtx->shaderSelector = 3; };
		MAP_KEY(GLFW_KEY_5) { if(!pressed) ctrlCtx->shaderSelector = 4; };
		MAP_KEY(GLFW_KEY_6) { if(!pressed) ctrlCtx->shaderSelector = 5; };
		MAP_KEY(GLFW_KEY_7) { if(!pressed) ctrlCtx->shaderSelector = 6; };

		MAP_KEY(GLFW_KEY_LEFT_SHIFT) { ctrlCtx->speedMod = pressed; };

		MAP_KEY(GLFW_KEY_ESCAPE) {
			glfwSetWindowShouldClose(win, pressed); };

		km[GLFW_MOUSE_BUTTON_LEFT] = [ctrlCtx, &win](bool pressed, unsigned) {
			ctrlCtx->dragView = pressed;
			glfwGetCursorPos(win, &ctrlCtx->lastCursorPos.x, &ctrlCtx->lastCursorPos.x);
		};

		#undef MAP_KEY

		return km;
	}


	void set_user_controls(RenderContext& ctx, GLFWwindow* win) {
		glfwSetWindowUserPointer(win, &ctx.glfwCtx);
		glfwSetKeyCallback(win, [](GLFWwindow* w, int k, int, int act, int mods) {
			if(act == GLFW_PRESS || act == GLFW_RELEASE) {
				Keymap& keymap = *reinterpret_cast<GlfwContext*>(
					glfwGetWindowUserPointer(w))->keymap;
				auto found = keymap.find(k);
				if(found != keymap.end()) {
					found->second(act == GLFW_PRESS, mods); }
			}
		});
		glfwSetMouseButtonCallback(win, [](GLFWwindow* w, int b, int act, int mods) {
			Keymap& keymap = *reinterpret_cast<GlfwContext*>(
				glfwGetWindowUserPointer(w))->keymap;
			auto found = keymap.find(-b);
			if(found != keymap.end()) {
				found->second(act == GLFW_PRESS, mods); }
		});
	}


	void init_render_ctx_pod(Application& app, RenderContext& dst) {
		const auto& opts = app.options();
		dst.frameTiming = FrameTiming {
			.frameTime = 1.0f / opts.viewParams.frameFrequencyS };
		dst.ctrlCtx = {
			.fwdMoveVector = { }, .bcwMoveVector = { },
			.rotate = { }, .lastCursorPos = { },
			.shaderSelector = 0, .dragView = false, .speedMod = false,
			.toggleFullscreen = false, .createObj = false,
			.movePointLightMod = false };
		dst.keymap = mk_key_bindings(app.glfwWindow(), &dst.ctrlCtx);
		dst.rngDistr = std::uniform_real_distribution<float>(0.0f, 1.0f);
		dst.turnSpeedKey = opts.viewParams.viewTurnSpeedKey;
		dst.turnSpeedKeyMod = opts.viewParams.viewTurnSpeedKeyMod;
		dst.moveSpeed = opts.viewParams.viewMoveSpeed;
		dst.moveSpeedMod = opts.viewParams.viewMoveSpeedMod;
		dst.instances = DeviceVector<Instance>(app, {
			vk::BufferUsageFlagBits::eVertexBuffer,
			{ }, { }, VMA_MEMORY_USAGE_GPU_ONLY });
		dst.lightDirection = glm::normalize(glm::vec3({
			opts.worldParams.lightDirection[0],
			opts.worldParams.lightDirection[1],
			opts.worldParams.lightDirection[2] }));
		dst.position = {
			-opts.viewParams.initialPosition[0],
			-opts.viewParams.initialPosition[1],
			-opts.viewParams.initialPosition[2] };
		dst.orientation = {
			glm::radians(opts.viewParams.initialYaw),
			glm::radians(opts.viewParams.initialPitch) };
		dst.frameCounter = 0;
	}


	void set_static_ubo(RenderPass& rpass, const Options& opts) {
		assert(rpass.swapchain() != nullptr);
		auto& swapchainExtent = rpass.swapchain()->data.extent;
		float aspectRatio = float(swapchainExtent.width) / float(swapchainExtent.height);
		ubo::Static sUbo;
		sUbo.projTransf = glm::perspective(
			glm::radians(opts.viewParams.fov), aspectRatio,
			opts.shaderParams.zNear, opts.shaderParams.zFar);
		sUbo.projTransf = glm::scale(sUbo.projTransf, glm::vec3(-0.5)); // "Clip space is inverted and halved"
		sUbo.outlineSize = opts.shaderParams.outlineSize;
		sUbo.outlineDepth = opts.shaderParams.zNear * opts.shaderParams.outlineDepth;
		sUbo.outlineRnd = opts.shaderParams.outlineRndMorph;
		sUbo.lightLevels = opts.shaderParams.celLightLevels;
		rpass.setStaticUbo(sUbo);
	}


	void read_ctx_shaders(Application& app, RenderContext& dst) {
		std::string shaderPath = get_shader_path();
		constexpr auto rdFile = [](const std::string& path) {
			std::ifstream ifstream = std::ifstream(path);
			return util::read_stream(ifstream);
		};
		dst.shaders.mainVtx = rdFile(shaderPath + "/vertex.main.spv"s);
		dst.shaders.mainFrg = rdFile(shaderPath + "/fragment.main.spv"s);
		dst.shaders.outlineVtx = rdFile(shaderPath + "/vertex.outline.spv"s);
		dst.shaders.outlineFrg = rdFile(shaderPath + "/fragment.outline.spv"s);
	}


	void create_render_ctx_rpass(
			Application& app,
			RenderContext& dst, const Options& opts
	) {
		{ // Create the render pass
			CtrlSchemeContext* dstCtrlCtx = &dst.ctrlCtx;
			RenderPass* dstRpass = &dst.rpass;
			Pipeline* dstMainPl = &dst.mainPipeline;
			Pipeline* dstOutlinePl = &dst.outlinePipeline;
			auto* dstObjects = &dst.objects;
			const auto* dstShaders = &dst.shaders;
			auto sampleCount = app.runtime().bestSampleCount;

			std::function buildPipelines = [
					dstRpass, dstMainPl, dstOutlinePl, dstShaders,
					sampleCount
			] () {
				*dstMainPl = Pipeline(*dstRpass,
					dstShaders->mainVtx, dstShaders->mainFrg, "main", 0,
					false, dstRpass->renderExtent(), sampleCount);
				*dstOutlinePl = Pipeline(*dstRpass,
					dstShaders->outlineVtx, dstShaders->outlineFrg, "main", 1,
					true, dstRpass->renderExtent(), sampleCount);
			};

			RenderPass::SwapchainOutdatedCallback onSwpchnOod = [
					buildPipelines, dstCtrlCtx, dstMainPl, dstOutlinePl,
					dstObjects
			] (
					RenderPass& rpass
			) {
				static_assert(ubo::Model::set == Texture::samplerDescriptorSet);
				assert(rpass.swapchain()->application != nullptr);
				auto* app = rpass.swapchain()->application;
				dstMainPl->destroy();
				dstOutlinePl->destroy();
				app->rebuildSwapChain();
				rpass.reassign(app->swapchain(), fit_extent_height(
					app->options().windowParams.maxVerticalResolution,
					app->swapchain().data.extent));
				buildPipelines();
				set_static_ubo(rpass, app->options());
				for(Object& obj : (*dstObjects)) {
					obj.mdlWr.recreateDescSet(
						rpass.descriptorPool(), rpass.descriptorSetLayouts()[ubo::Model::set]);
				}
			};

			dst.rpass = RenderPass(app.swapchain(),
				fit_extent_height(
					app.options().windowParams.maxVerticalResolution,
					app.swapchain().data.extent),
				MAX_CONCURRENT_FRAMES, opts.windowParams.useMultisampling,
				onSwpchnOod);
			buildPipelines();
		} { // Assign and adjust everything that depends on the render pass
			dst.glfwCtx = {
				.keymap = &dst.keymap, .app = &app,
				.ctrlCtx = &dst.ctrlCtx, .frameTiming = &dst.frameTiming };

			set_user_controls(dst, app.glfwWindow());
			set_static_ubo(dst.rpass, opts);

			for(Object& obj : dst.objects) {
				obj.mdlWr.recreateDescSet(
					dst.rpass.descriptorPool(), dst.rpass.descriptorSetLayouts()[ubo::Model::set]);
			}
		}
	}


	void destroy_render_ctx_rpass(RenderContext& ctx) {
		ctx.outlinePipeline.destroy();
		ctx.mainPipeline.destroy();
		ctx.rpass.destroy();
	}


	void toggle_fullscreen(Application& app, RenderContext& ctx, const Options& opts) {
		bool newFullscreenValue = ! app.runtime().fullscreen;
		vk::Extent2D newExtent;
		{
			const auto& extentArray = newFullscreenValue?
				opts.windowParams.fullscreenExtent :
				opts.windowParams.windowExtent;
			newExtent = vk::Extent2D(extentArray[0], extentArray[1]);
		}
		destroy_render_ctx_rpass(ctx);
		app.setWindowMode(newFullscreenValue, newExtent);
		create_render_ctx_rpass(app, ctx, opts);
	}


	void load_ctx_assets(Application& app, RenderContext& dst) {
		static_assert(ubo::Model::set == Texture::samplerDescriptorSet);
		constexpr auto mdlDescSetLayoutIndex = ubo::Model::set;
		std::map<std::string, Scene::Model*> mdlInfoMap;
		std::string assetPath = get_asset_path();
		auto& worldOpts = app.options().worldParams;
		Scene scene;
		{ // Load the scene
			std::string scenePath = assetPath + "/scene.cfg";
			util::logDebug() << "Reading scene from \"" << scenePath << '"' << util::endl;
			scene = Scene::fromCfg(scenePath);
			util::logDebug() << "Scene has " << scene.objects.size() << " objects:" << util::endl;
		} { // Make name -> model associations
			for(auto& mdlInfo : scene.models) {
				util::logDebug() << "- \"" << mdlInfo.name
					<< "\", (" << mdlInfo.minDiffuse << ", " << mdlInfo.maxDiffuse << "), ("
					<< mdlInfo.minSpecular << ", " << mdlInfo.maxSpecular << ", "
					<< mdlInfo.shininess << ')' << util::endl;
				mdlInfoMap[mdlInfo.name] = &mdlInfo;
			}
		} { // Set the point light
			dst.pointLight = glm::vec4 {
				scene.pointLight[0],
				scene.pointLight[1],
				scene.pointLight[2],
				scene.pointLight[3] };
		} { // Create objects
			for(auto& objInfo : scene.objects) {
				Model::ObjSources src;
				src.mdlName = objInfo.modelName;
				src.objPath = assetPath + "/"s + src.mdlName + ".obj";
				src.textureLoader = [&app, &src, &worldOpts, &assetPath](Texture::Usage usage) {
					std::string txtrName;
					constexpr auto setName = [](std::string& dst, const std::string& value) {
						dst = value;
						util::logDebug() << "Loading texture \"" << value << '"' << util::endl;
					};
					switch(usage) {
						case Texture::Usage::eDiffuse:
							setName(txtrName, assetPath + "/"s + src.mdlName + ".dfs.png");
							return rd_texture(app, txtrName,
								worldOpts.diffuseNearestFilter, MISSING_TEXTURE_COLOR);
						case Texture::Usage::eSpecular:
							setName(txtrName, assetPath + "/"s + src.mdlName + ".spc.png");
							return rd_texture(app, txtrName,
								worldOpts.specularNearestFilter, MISSING_TEXTURE_COLOR);
						case Texture::Usage::eNormal:
							setName(txtrName, assetPath + "/"s + src.mdlName + ".nrm.png");
							return rd_texture(app, txtrName,
								worldOpts.normalNearestFilter, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
						default: throw std::logic_error("invalid value of vka2::Texture::Usage");
					}
				};
				src.postAssembly = [&src](Vertices& vtx, Indices& idx) {
					size_t vtxSize = vtx.size() * sizeof(Vertex);
					size_t idxSize = idx.size() * sizeof(Vertex::index_t);
					util::logDebug()
						<< "Model \"" << src.mdlName << "\" has " << idx.size() << " vertices ("
						<< vtxSize << '+' << idxSize << " = " << static_cast<size_t>(
							std::ceil(static_cast<float>(vtxSize + idxSize) / (1024.0f*1024.0f))
						) << "MiB)" << util::endl;
				};
				dst.objects.push_back(std::move(Object {
					.mdlWr = ModelWrapper(
						Model::fromObj(app, src,
							mdlInfoMap[objInfo.modelName]->mergeVertices,
							&dst.mdlCache, &dst.matCache),
						dst.rpass.descriptorPool(),
						dst.rpass.descriptorSetLayouts()[mdlDescSetLayoutIndex]
					),
					.position = glm::vec3(objInfo.position[0], objInfo.position[1], objInfo.position[2]),
					.orientation = glm::vec3(objInfo.orientation[0], objInfo.orientation[1], objInfo.orientation[2]),
					.scale = glm::vec3(objInfo.scale[0], objInfo.scale[1], objInfo.scale[2]),
					.color = glm::vec4(objInfo.color[0], objInfo.color[1], objInfo.color[2], objInfo.color[3]),
					.rnd = dst.rngDistr(dst.rng)
				}));
			}
		} { // Associate models with automatically created models
			for(auto& mdlInfo : scene.models) {
				auto found = dst.mdlCache.find(mdlInfo.name);
				if(found != dst.mdlCache.end()) {
					found->second->viewUbo([&dst, &mdlInfo](MemoryView<ubo::Model> ubo) {
						*ubo.data = ubo::Model {
							.minDiffuse = mdlInfo.minDiffuse,
							.maxDiffuse = mdlInfo.maxDiffuse,
							.minSpecular = mdlInfo.minSpecular,
							.maxSpecular = mdlInfo.maxSpecular,
							.shininess = mdlInfo.shininess,
							.rnd = dst.rngDistr(dst.rng) };
						return true;
					});
				}
			}
		}
	}


	void create_render_ctx(
			Application& app,
			RenderContext& dst, const Options& opts
	) {
		init_render_ctx_pod(app, dst);
		read_ctx_shaders(app, dst);
		create_render_ctx_rpass(app, dst, opts);
		load_ctx_assets(app, dst);
	}


	void destroy_render_ctx(RenderContext& ctx) {
		destroy_render_ctx_rpass(ctx);
	}


	void create_object(RenderContext& ctx) {
		auto floatRnd = [&ctx]() {
			constexpr unsigned mod = std::numeric_limits<unsigned>::max();
			return static_cast<float>(static_cast<unsigned>(ctx.rng()) % mod) / mod;
		};
		const Object& clonee = [&ctx]() -> const Object& {
			return ctx.objects[ctx.rng() % ctx.objects.size()];
		} ();
		ctx.objects.push_back(Object {
			.mdlWr = ModelWrapper(*clonee.mdlWr,
				ctx.rpass.descriptorPool(),
				ctx.rpass.descriptorSetLayouts()[ubo::Model::set] ),
			.position = glm::vec3(
				clonee.position.x + (floatRnd() * 4.0f),
				clonee.position.y + (floatRnd() * 4.0f),
				clonee.position.z + (floatRnd() * 4.0f) ),
			.orientation = glm::vec3(
				clonee.orientation.x + (floatRnd() * 15.0f),
				clonee.orientation.x + (floatRnd() * 15.0f),
				clonee.orientation.x + (floatRnd() * 15.0f) ),
			.scale = clonee.scale,
			.color = clonee.color,
			.rnd = floatRnd()
		});
	}


	void process_input(
			RenderContext& ctx, glm::mat4& orientationMat
	) {
		constexpr auto rad360 = glm::radians(360.0f);
		{ // Modify the current orientation based on the input state
			float adjustedTurnSpeed = ctx.ctrlCtx.speedMod?
				ctx.turnSpeedKeyMod : ctx.turnSpeedKey;
			glm::vec2 actualRotate = {
				ctx.ctrlCtx.rotate.x,
				ctx.ctrlCtx.rotate.y * YAW_TO_PITCH_RATIO };
			ctx.orientation += adjustedTurnSpeed * actualRotate * ctx.frameTiming.frameTime;
			ctx.orientation.x -= std::floor(ctx.orientation.x / rad360) * rad360;
			ctx.orientation.y -= std::floor(ctx.orientation.y / rad360) * rad360;
		} { // Rotate the orientation matrix
			orientationMat = glm::rotate(orientationMat,
				ctx.orientation.y,
				glm::vec3(1.0f, 0.0f, 0.0f));
			orientationMat = glm::rotate(orientationMat,
				ctx.orientation.x,
				glm::vec3(0.0f, 1.0f, 0.0f));
		} { // Change the current position based on the orientation
			float adjustedMoveSpeed = ctx.ctrlCtx.speedMod?
				ctx.moveSpeedMod : ctx.moveSpeed;
			glm::vec3 deltaPos = adjustedMoveSpeed * ctx.frameTiming.frameTime *
				(ctx.ctrlCtx.fwdMoveVector - ctx.ctrlCtx.bcwMoveVector);
			glm::vec4 deltaPosRotated = glm::transpose(orientationMat) * glm::vec4(deltaPos, 1.0f);
			if(ctx.ctrlCtx.movePointLightMod) {
				ctx.pointLight -= glm::vec4(deltaPosRotated.x, deltaPosRotated.y, deltaPosRotated.z, 0.0f);
			} else {
				ctx.position -= glm::vec3(deltaPosRotated);
			}
		} { // Create an object, if requested
			if(ctx.ctrlCtx.createObj) {
				create_object(ctx);
				ctx.ctrlCtx.createObj = false;
			}
		}
	}


	bool try_change_fullscreen(
			Application& app, const Options& opts,
			RenderContext& ctx
	) {
		if(ctx.ctrlCtx.toggleFullscreen) {
			util::logVkEvent() << "Setting "
				<< (app.runtime().fullscreen? "windowed " : "fullscreen ")
				<< "mode" << util::endl;
			toggle_fullscreen(app, ctx, opts);
			ctx.ctrlCtx.toggleFullscreen = false;
			return true;
		} else {
			return false;
		}
	}


	void mk_instances(
			RenderPass& rpass,
			const std::vector<Object>& objects,
			DeviceVector<Instance>& dst
	) {
		size_t i = 0;
		if(dst.size() != objects.size()) {
			dst.resize(&rpass, objects.size());
		}
		for(const auto& obj : objects) {
			Instance& inst = dst[i];
			inst.modelTransf = glm::mat4(1.0f);
			inst.modelTransf = glm::translate(inst.modelTransf, obj.position);
			inst.modelTransf = glm::rotate(inst.modelTransf,
				glm::radians(obj.orientation.y), glm::vec3(1.0f, 0.0f, 0.0f));
			inst.modelTransf = glm::rotate(inst.modelTransf,
				glm::radians(obj.orientation.x), glm::vec3(0.0f, 1.0f, 0.0f));
			inst.modelTransf = glm::rotate(inst.modelTransf,
				glm::radians(obj.orientation.z), glm::vec3(0.0f, 0.0f, 1.0f));
			inst.modelTransf = glm::scale(inst.modelTransf, obj.scale);
			inst.colorMul = obj.color;
			inst.rnd = obj.rnd;
			++i;
		}
		assert(i == objects.size());
	}


	void mk_frame_ubo(
			RenderContext& ctx,
			const glm::mat4& orientationMat,
			ubo::Frame& dst
	) {
		dst.viewTransf = glm::mat4(1.0f);
		dst.viewTransf = orientationMat * dst.viewTransf;
		dst.viewTransf = glm::translate(dst.viewTransf, -ctx.position);
		dst.viewPos = ctx.position;
		dst.pointLight = ctx.pointLight;
		dst.lightDirection = ctx.lightDirection;
		dst.shaderSelector = ctx.ctrlCtx.shaderSelector;
		dst.rnd = ctx.rngDistr(ctx.rng);
	}

}



namespace vka2 {

	/* This is the God function. It still needs to be compartmentalized,
	 * so documenting this mess isn't worth my time since it's going
	 * to be torn down in the near future - I'll document it then. */
	void Application::run() {
		using glm::vec2;
		using glm::vec3;
		using glm::vec4;
		using glm::mat4;
		const auto& opts = _data.options;
		RenderContext ctx = { };
		std::vector<push_const::Object> objPushConsts;
		create_render_ctx(*this, ctx, opts);
		util::TimeGateNs timer;
		double sleepTime = ctx.frameTiming.frameTime / SLEEPS_PER_FRAME;
		{
			{
				{
					size_t vtxCount = 0;
					for(const auto& obj : ctx.objects) {
						vtxCount += obj.mdlWr->idxCount(); }
					util::logDebug() << "Rendering " << vtxCount << " vertices each frame" << util::endl;
				}
				while(! glfwWindowShouldClose(_data.glfwWin)) {
					mat4 orientationMat = mat4(1.0f);
					glfwPollEvents();
					process_input(ctx, orientationMat);
					if(try_change_fullscreen(*this, opts, ctx)) {
						continue; }
					ubo::Frame frameUbo;
					mk_frame_ubo(ctx, orientationMat, frameUbo);
					mk_instances(ctx.rpass, ctx.objects, ctx.instances);
					ctx.instances.flush();
					auto draw = [&ctx](
							RenderPass::FrameHandle& fh, vk::CommandBuffer cmd,
							const Object& obj, uint32_t instanceIdx
					) {
						cmd.bindVertexBuffers(0, obj.mdlWr->vtxBuffer().handle, { 0 });
						cmd.bindVertexBuffers(1, ctx.instances.devBuffer().handle, { 0 });
						cmd.bindIndexBuffer(obj.mdlWr->idxBuffer().handle,
							0, Vertex::INDEX_TYPE);
						fh.bindModelDescriptorSet(cmd, obj.mdlWr.descSet());
						cmd.drawIndexed(obj.mdlWr->idxCount(), 1, 0, 0, instanceIdx);
					};
					ctx.rpass.runRenderPass(frameUbo, { }, { }, {
						std::function([&](RenderPass::FrameHandle& fh, vk::CommandBuffer cmd) {
							assert(ctx.instances.size() == ctx.objects.size());
							cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ctx.mainPipeline.handle());
							for(size_t i=0; i < ctx.objects.size(); ++i) {
								draw(fh, cmd, ctx.objects[i], i); }
						}),
						std::function([&](RenderPass::FrameHandle& fh, vk::CommandBuffer cmd) {
							assert(ctx.instances.size() == ctx.objects.size());
							cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ctx.outlinePipeline.handle());
							for(size_t i=0; i < ctx.objects.size(); ++i) {
								draw(fh, cmd, ctx.objects[i], i); }
						})
					});
					{ // Framerate throttle
						auto timeMul = decltype(timer)::period_t::den / decltype(timer)::period_t::num;
						decltype(timer)::precision_t frameTimeUnits = ctx.frameTiming.frameTime * timeMul;
						while(! timer.forward(frameTimeUnits)) {
							util::sleep_s(sleepTime); }
					}
					++ctx.frameCounter;
				}
			}
		}
		destroy_render_ctx(ctx);
	}

}
