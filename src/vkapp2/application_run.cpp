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

#include <util/perftracker.hpp>

using namespace vka2;
using namespace std::string_literals;



namespace {

	using KeyBinding = std::function<void (bool isPressed, uint16_t modifiers)>;
	using Keymap = std::map<SDL_Keycode, KeyBinding>;


	/** Returns a string representing the given number of nanoseconds,
	 * in the form `(N+)\.(D{1,3})`. */
	std::string nanosToMicrosStr(perf::PerfTracker::utime_t nanos) {
		using utime_t = decltype(nanos);
		std::string r;
		r.reserve(2 * std::log10(std::max<utime_t>(1, nanos)));
		utime_t intPart = nanos / 1000;
		utime_t frcPart = nanos % 1000;
		unsigned frcPadChars = 3 - std::ceil(std::log10(std::max<utime_t>(1, frcPart)));
		r.append(std::to_string(intPart));
		r.push_back('.');
		for(unsigned i=0; i < frcPadChars; ++i) r.push_back('0');
		r.append(std::to_string(frcPart));
		return r;
	}


	struct DeviceVectorTraits {
		vk::BufferUsageFlags bufferUsage;
		vk::MemoryPropertyFlags vmaRequiredFlags;
		vk::MemoryPropertyFlags vmaPreferredFlags;
		VmaMemoryUsage vmaMemoryUsage;
		vk::PipelineStageFlags firstStage;
		vk::AccessFlags firstAccess;
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
				auto& cmdPool = app_->graphicsCommandPool();
				auto cmdFn = [this, beg, end](vk::CommandBuffer cmd) {
					vk::BufferCopy cp;
					vk::BufferMemoryBarrier mBar = { };
					cp.srcOffset = cp.dstOffset = beg * sizeof(T);
					cp.setSize((end - beg) * sizeof(T));
					mBar.buffer = devDataBuffer_.handle;
					mBar.size = VK_WHOLE_SIZE;
					mBar.srcAccessMask = vk::AccessFlagBits::eHostWrite;
					mBar.dstAccessMask = traits_.firstAccess;
					mBar.srcQueueFamilyIndex =
					mBar.dstQueueFamilyIndex = app_->queueFamilyIndices().graphics;
					cmd.pipelineBarrier(
						vk::PipelineStageFlagBits::eHost,
						traits_.firstStage,
						vk::DependencyFlags(),
						{ }, mBar, { });
					cmd.copyBuffer(cpuDataBuffer_.handle, devDataBuffer_.handle, cp);
				};
				if(fence) {
					return cmdPool.runCmdsAsync(app_->queues().graphics, cmdFn, fence);
				} else {
					cmdPool.runCmds(app_->queues().graphics, cmdFn);
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
		unsigned shaderSelector;
		bool speedMod : 1;
		bool toggleFullscreen : 1;
		bool createObj : 1;
		bool movePointLightMod : 1;
	};


	struct SdlContext {
		Keymap* keymap;
		Application* app;
		CtrlSchemeContext* ctrlCtx;
		FrameTiming* frameTiming;
	};


	/** A wrapper for vka2::Mesh, to associate it with a descriptor set.
	 * It also references a descriptor pool, in order to create and destroy
	 * sets. */
	class MeshWrapper {
		MeshInstance::ShPtr _mdl;
		DynDescriptorPool* _dPool;
		DynDescriptorPool::SetHandle _dSetPtr;
	public:
		MeshWrapper(): _mdl() { }

		MeshWrapper(MeshInstance::ShPtr mdl, DynDescriptorPool& dPool):
				_mdl(std::move(mdl)),
				_dPool(&dPool)
		{
			assert(_dPool != nullptr);
			_dSetPtr = _dPool->request();
		}

		MeshWrapper(MeshWrapper&& mov):
				_mdl(std::move(mov._mdl)),
				_dPool(std::move(mov._dPool)),
				_dSetPtr(std::move(mov._dSetPtr))
		{
			mov._dPool = nullptr;
		}

		~MeshWrapper() {
			if(_dPool != nullptr) {
				if(_dPool) {
					_dPool->release(_dSetPtr);
				}
				_dPool = nullptr;
			}
		}


		vk::DescriptorSet descSet() const { return _dSetPtr.get(*_dPool); }


		MeshInstance::ShPtr operator*() { return _mdl; }
		const MeshInstance::ShPtr operator*() const { return _mdl; }
		MeshInstance::ShPtr operator->() { return _mdl; }
		const MeshInstance::ShPtr operator->() const { return _mdl; }
	};


	struct Object {
		MeshWrapper meshWrapper;
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
		SdlContext sdlCtx;
		Keymap keymap;
		RenderPass rpass;
		Pipeline mainPipeline, outlinePipeline;
		DynDescriptorPool dPool;
		MeshInstance::TextureCache textureCache;
		MeshInstance::MeshCache meshCache;
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
		bool dPoolOutOfDate;
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


	void poll_events(RenderContext& ctx, bool& shouldCloseDst) {
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_QUIT: {
					shouldCloseDst = true;
				} break;
				case SDL_KEYUP: [[fallthrough]];
				case SDL_KEYDOWN: {
					auto state = event.key.state;
					if(state == SDL_PRESSED || state == SDL_RELEASED) {
						Keymap& keymap = *ctx.sdlCtx.keymap;
						auto found = keymap.find(event.key.keysym.sym);
						if(found != keymap.end()) {
							found->second(state == SDL_PRESSED, event.key.keysym.mod); }
					}
				} break;
			}
		}
	}


	/** Key bindings will depend on the application AND the control scheme
	 * context, so they must be must stay alive as long as these key bindings
	 * can be used by SDL. */
	Keymap mk_key_bindings(SDL_Window*& win, CtrlSchemeContext* ctrlCtx) {
		Keymap km;

		#define MAP_KEY(_K) km[_K] = [ctrlCtx, &win]([[maybe_unused]] bool pressed, [[maybe_unused]] unsigned mod)

		MAP_KEY(SDLK_s) { ctrlCtx->fwdMoveVector.z = pressed? 1.0f : 0.0f; };
		MAP_KEY(SDLK_w) { ctrlCtx->bcwMoveVector.z = pressed? 1.0f : 0.0f; };
		MAP_KEY(SDLK_d) { ctrlCtx->fwdMoveVector.x = pressed? 1.0f : 0.0f; };
		MAP_KEY(SDLK_a) { ctrlCtx->bcwMoveVector.x = pressed? 1.0f : 0.0f; };
		MAP_KEY(SDLK_r) { ctrlCtx->bcwMoveVector.y = pressed? 1.0f : 0.0f; };
		MAP_KEY(SDLK_f) { ctrlCtx->fwdMoveVector.y = pressed? 1.0f : 0.0f; };
		MAP_KEY(SDLK_n) { if(!pressed) ctrlCtx->createObj = true; };
		MAP_KEY(SDLK_LCTRL) { ctrlCtx->movePointLightMod = pressed; };

		#ifndef NDEBUG
			MAP_KEY(SDLK_c) { std::quick_exit(1); };
		#endif

		MAP_KEY(SDLK_RETURN) {
			if((! pressed) && (mod & KMOD_ALT)) {
				ctrlCtx->toggleFullscreen = true; }
		};

		MAP_KEY(SDLK_RIGHT) { ctrlCtx->rotate.x = pressed? +1.0f : 0.0f; };
		MAP_KEY(SDLK_LEFT)  { ctrlCtx->rotate.x = pressed? -1.0f : 0.0f; };
		MAP_KEY(SDLK_UP)    { ctrlCtx->rotate.y = pressed? +1.0f : 0.0f; };
		MAP_KEY(SDLK_DOWN)  { ctrlCtx->rotate.y = pressed? -1.0f : 0.0f; };

		MAP_KEY(SDLK_1) { if(!pressed) ctrlCtx->shaderSelector = 0; };
		MAP_KEY(SDLK_2) { if(!pressed) ctrlCtx->shaderSelector = 1; };
		MAP_KEY(SDLK_3) { if(!pressed) ctrlCtx->shaderSelector = 2; };
		MAP_KEY(SDLK_4) { if(!pressed) ctrlCtx->shaderSelector = 3; };
		MAP_KEY(SDLK_5) { if(!pressed) ctrlCtx->shaderSelector = 4; };
		MAP_KEY(SDLK_6) { if(!pressed) ctrlCtx->shaderSelector = 5; };
		MAP_KEY(SDLK_7) { if(!pressed) ctrlCtx->shaderSelector = 6; };

		MAP_KEY(SDLK_LSHIFT) { ctrlCtx->speedMod = pressed; };

		MAP_KEY(SDLK_ESCAPE) {
			SDL_Event ev;
			ev.quit.type = SDL_QUIT;
			{
				auto result = SDL_PushEvent(&ev);
				if(result != 1) {
					throw std::runtime_error(
						std::string("failed to push a SDL_QuitEvent: ") +
						SDL_GetError() );
				}
			}
		};

		#undef MAP_KEY

		return km;
	}


	void init_render_ctx_pod(Application& app, RenderContext& dst) {
		const auto& opts = app.options();
		dst.frameTiming = FrameTiming {
			.frameTime = 1.0f / opts.viewParams.frameFrequencyS };
		dst.ctrlCtx = {
			.fwdMoveVector = { }, .bcwMoveVector = { },
			.rotate = { },
			.shaderSelector = 0, .speedMod = false,
			.toggleFullscreen = false, .createObj = false,
			.movePointLightMod = false };
		dst.keymap = mk_key_bindings(app.sdlWindow(), &dst.ctrlCtx);
		dst.rngDistr = std::uniform_real_distribution<float>(0.0f, 1.0f);
		dst.turnSpeedKey = opts.viewParams.viewTurnSpeedKey;
		dst.turnSpeedKeyMod = opts.viewParams.viewTurnSpeedKeyMod;
		dst.moveSpeed = opts.viewParams.viewMoveSpeed;
		dst.moveSpeedMod = opts.viewParams.viewMoveSpeedMod;
		dst.instances = DeviceVector<Instance>(app, {
			vk::BufferUsageFlagBits::eVertexBuffer,
			{ }, { }, VMA_MEMORY_USAGE_GPU_ONLY,
			vk::PipelineStageFlagBits::eVertexInput,
			vk::AccessFlagBits::eIndexRead });
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
			};

			dst.rpass = RenderPass(app.swapchain(),
				fit_extent_height(
					app.options().windowParams.maxVerticalResolution,
					app.swapchain().data.extent),
				MAX_CONCURRENT_FRAMES, opts.windowParams.useMultisampling,
				onSwpchnOod);
			buildPipelines();
		} { // Assign and adjust everything that depends on the render pass
			dst.sdlCtx = {
				.keymap = &dst.keymap, .app = &app,
				.ctrlCtx = &dst.ctrlCtx, .frameTiming = &dst.frameTiming };

			dst.dPool = dst.rpass.createInstanceDescriptorPool();
			dst.dPool.setSize(dst.objects.size()); // The size should be 0 on the first call
			for(auto& obj : dst.objects) {
				obj.meshWrapper->updateDescriptorSet(obj.meshWrapper.descSet());
			}
			dst.dPoolOutOfDate = true;

			set_static_ubo(dst.rpass, opts);
		}
	}


	void destroy_render_ctx_rpass(RenderContext& ctx) {
		ctx.dPool = nullptr;
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


	Object* try_mk_object_info(
			Application& app, const vka2::MeshInstance::ObjSources& src,
			RenderContext& dst, const vka2::Scene::Object& objInfo,
			std::map<std::string, Scene::Material*>& mtlInfoMap
	) {
		auto found = mtlInfoMap[objInfo.materialName];
		if(! found) {
			util::logGeneral()
				<< "Material \"" << objInfo.materialName << "\" not found for mesh \""
				<< objInfo.meshName << '"' << util::endl;
			return nullptr;
		} else {
			util::logDebug()
				<< "Using mesh \"" << objInfo.meshName << "\" with material \""
				<< objInfo.materialName << '"' << util::endl;
			dst.objects.push_back(std::move(Object {
				.meshWrapper = MeshWrapper(
					MeshInstance::fromObj(app, src, found->mergeVertices, &dst.meshCache, &dst.textureCache),
					dst.dPool
				),
				.position = glm::vec3(objInfo.position[0], objInfo.position[1], objInfo.position[2]),
				.orientation = glm::vec3(objInfo.orientation[0], objInfo.orientation[1], objInfo.orientation[2]),
				.scale = glm::vec3(objInfo.scale[0], objInfo.scale[1], objInfo.scale[2]),
				.color = glm::vec4(objInfo.color[0], objInfo.color[1], objInfo.color[2], objInfo.color[3]),
				.rnd = dst.rngDistr(dst.rng)
			}));
			return &dst.objects.back();
		}
	}


	void create_render_ctx(
			Application& app,
			RenderContext& dst, const Options& opts
	) {
		init_render_ctx_pod(app, dst);
		read_ctx_shaders(app, dst);
		create_render_ctx_rpass(app, dst, opts);
	}


	void destroy_render_ctx(RenderContext& ctx) {
		destroy_render_ctx_rpass(ctx);
	}


	void sync_desc_sets(RenderContext& ctx) {
		if(ctx.dPoolOutOfDate) {
			for(auto& obj : ctx.objects) {
				obj.meshWrapper->updateDescriptorSet(obj.meshWrapper.descSet());
			}
			ctx.dPoolOutOfDate = false;
		}
	}


	void load_assets(Application& app, RenderContext& dst) {
		static_assert(ubo::Model::set == Texture::samplerDescriptorSet);
		std::map<std::string, Scene::Material*> mtlInfoMap;
		std::string assetPath = get_asset_path();
		auto& worldOpts = app.options().worldParams;
		Scene scene;
		{ // Load the scene
			std::string scenePath = assetPath + "/scene.cfg";
			util::logDebug() << "Reading scene from \"" << scenePath << '"' << util::endl;
			scene = Scene::fromCfg(scenePath);
			util::logDebug() << "Scene has " << scene.objects.size() << " objects" << util::endl;
		} { // Make name -> material associations
			for(auto& mtlInfo : scene.materials) {
				util::logDebug() << "Material \"" << mtlInfo.name
					<< "\", (" << mtlInfo.ambient << ", " << mtlInfo.diffuse << ", "
					<< mtlInfo.specular << "), ("
					<< mtlInfo.shininess << ", " << mtlInfo.celLevels << ')' << util::endl;
				mtlInfoMap[mtlInfo.name] = &mtlInfo;
			}
		} { // Set the point light
			dst.pointLight = glm::vec4 {
				scene.pointLight[0],
				scene.pointLight[1],
				scene.pointLight[2],
				scene.pointLight[3] };
		} { // Create objects
			for(auto& objInfo : scene.objects) {
				MeshInstance::ObjSources src;
				if(objInfo.materialName.empty()) {
					objInfo.materialName = objInfo.meshName; }
				src.materialName = objInfo.materialName;
				src.objPath = assetPath + "/"s + objInfo.meshName + ".obj";
				src.textureLoader = [
						&app, &src, &worldOpts, &assetPath, &mtlInfoMap
				] (Texture::Usage usage) {
					std::string txtrName = src.materialName;
					constexpr auto setName = [](std::string& dst, const std::string& value) {
						dst = value;
						util::logDebug() << "Loading texture \"" << value << '"' << util::endl;
					};
					switch(usage) {
						case Texture::Usage::eDiffuse:
							setName(txtrName, assetPath + "/"s + txtrName + ".dfs.png");
							return rd_texture(app, txtrName,
								worldOpts.diffuseNearestFilter, MISSING_TEXTURE_COLOR);
						case Texture::Usage::eSpecular:
							setName(txtrName, assetPath + "/"s + txtrName + ".spc.png");
							return rd_texture(app, txtrName,
								worldOpts.specularNearestFilter, MISSING_TEXTURE_COLOR);
						case Texture::Usage::eNormal:
							setName(txtrName, assetPath + "/"s + txtrName + ".nrm.png");
							return rd_texture(app, txtrName,
								worldOpts.normalNearestFilter, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
						default: throw std::logic_error("invalid value of vka2::Texture::Usage");
					}
				};
				src.postAssembly = [&src, &objInfo](Vertices& vtx, Indices& idx) {
					size_t vtxSize = vtx.size() * sizeof(Vertex);
					size_t idxSize = idx.size() * sizeof(Vertex::index_t);
					util::logDebug()
						<< "Mesh \"" << objInfo.meshName << "\" has " << idx.size() << " vertices ("
						<< vtxSize << '+' << idxSize << " = " << static_cast<size_t>(
							std::ceil(static_cast<float>(vtxSize + idxSize) / (1024.0f*1024.0f))
						) << "MiB)" << util::endl;
				};
				auto* newObj = try_mk_object_info(app, src, dst, objInfo, mtlInfoMap);
				if(newObj != nullptr) {
					auto matInfo = mtlInfoMap.find(src.materialName);
					assert(matInfo != mtlInfoMap.end());
					newObj->meshWrapper->viewUbo([&dst, &matInfo](MemoryView<ubo::Model> ubo) {
						*ubo.data = ubo::Model {
							.ambient = matInfo->second->ambient,
							.diffuse = matInfo->second->diffuse,
							.specular = matInfo->second->specular,
							.shininess = matInfo->second->shininess,
							.rnd = dst.rngDistr(dst.rng),
							.celLevels = matInfo->second->celLevels };
						return true;
					});
				}
			}
		} { // Associate materials to texture sets
			for(auto& matInfo : scene.materials) {
				auto found = dst.meshCache.find(matInfo.name);
				if(found != dst.meshCache.end()) {
					found->second->viewUbo([&dst, &matInfo](MemoryView<ubo::Model> ubo) {
						*ubo.data = ubo::Model {
							.ambient = matInfo.ambient,
							.diffuse = matInfo.diffuse,
							.specular = matInfo.specular,
							.shininess = matInfo.shininess,
							.rnd = dst.rngDistr(dst.rng),
							.celLevels = matInfo.celLevels };
						return true;
					});
				}
			}
		}
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
			.meshWrapper = MeshWrapper(*clonee.meshWrapper, ctx.dPool),
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
		ctx.dPoolOutOfDate = true;
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
			auto timer = util::perfTracker.startTimer("app.assembleInstance");
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
			util::perfTracker.stopTimer(timer);
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
		dst.pack0 = ctx.ctrlCtx.shaderSelector << 16;
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
		load_assets(*this, ctx);
		util::TimeGateNs timer;
		util::PerfTracker perfTracker;
		perfTracker.movingAverageDecay =
		util::perfTracker.movingAverageDecay = ctx.frameTiming.frameTime / 30.0f;
		double sleepTime = ctx.frameTiming.frameTime / MAX_SLEEPS_PER_FRAME;
		bool shouldClose = false;
		{
			{
				{
					size_t vtxCount = 0;
					for(const auto& obj : ctx.objects) {
						vtxCount += obj.meshWrapper->idxCount(); }
					util::logDebug() << "Rendering " << vtxCount << " vertices each frame" << util::endl;
				}
				while(! shouldClose) {
					auto frameTimer = perfTracker.startTimer("app.frame");

					mat4 orientationMat = mat4(1.0f);
					perfTracker.measure("app.userInput", [&]() {
						process_input(ctx, orientationMat);
					});

					if(try_change_fullscreen(*this, opts, ctx)) {
						continue; }
					ubo::Frame frameUbo;
					perfTracker.measure("app.assembleFrameUbo", [&]() {
						mk_frame_ubo(ctx, orientationMat, frameUbo);
					});
					perfTracker.measure("app.assembleInstances", [&]() {
						mk_instances(ctx.rpass, ctx.objects, ctx.instances);
					});
					perfTracker.measure("app.flushInstanceBuffer", [&]() {
						ctx.instances.flush();
					});
					sync_desc_sets(ctx);

					auto draw = [&ctx, &perfTracker](
							RenderPass::FrameHandle& fh, vk::CommandBuffer cmd,
							const Object& obj, uint32_t instanceIdx
					) {
						auto timer = perfTracker.startTimer("app.drawCmd");
						cmd.bindVertexBuffers(0, obj.meshWrapper->vtxBuffer().handle, { 0 });
						cmd.bindVertexBuffers(1, ctx.instances.devBuffer().handle, { 0 });
						cmd.bindIndexBuffer(obj.meshWrapper->idxBuffer().handle,
							0, Vertex::INDEX_TYPE);
						fh.bindMeshDescriptorSet(cmd, obj.meshWrapper.descSet());
						cmd.drawIndexed(obj.meshWrapper->idxCount(), 1, 0, 0, instanceIdx);
						perfTracker.stopTimer(timer);
					};
					ctx.rpass.runRenderPass(frameUbo, { }, { }, {
						std::function([&](RenderPass::FrameHandle& fh, vk::CommandBuffer cmd) {
							assert(ctx.instances.size() == ctx.objects.size());
							auto timer = perfTracker.startTimer("app.runSubpass0");
							cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ctx.mainPipeline.handle());
							for(size_t i=0; i < ctx.objects.size(); ++i) {
								draw(fh, cmd, ctx.objects[i], i); }
							perfTracker.stopTimer(timer);
						}),
						std::function([&](RenderPass::FrameHandle& fh, vk::CommandBuffer cmd) {
							assert(ctx.instances.size() == ctx.objects.size());
							auto timer = perfTracker.startTimer("app.runSubpass1");
							cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, ctx.outlinePipeline.handle());
							for(size_t i=0; i < ctx.objects.size(); ++i) {
								draw(fh, cmd, ctx.objects[i], i); }
							perfTracker.stopTimer(timer);
						})
					});

					{ // Framerate throttle
						auto timeMul = decltype(timer)::period_t::den / decltype(timer)::period_t::num;
						decltype(timer)::precision_t frameTimeUnits = ctx.frameTiming.frameTime * timeMul;
						while(! timer.forward(frameTimeUnits)) {
							perfTracker.measure("app.sleepTime", [](decltype(sleepTime) sleepTime) {
								util::sleep_s(sleepTime);
							}, sleepTime);
						}
					}
					++ctx.frameCounter;
					perfTracker.stopTimer(frameTimer);

					poll_events(ctx, shouldClose);
				}
			}
		}
		destroy_render_ctx(ctx);
		#ifdef ENABLE_PERF_TRACKER
		{
			util::perfTracker |= perfTracker;
			perfTracker.reset();
			#define PRINT_TIME_(NM_) {\
				util::logGeneral() << "[Timer `" NM_ "`] " \
					<< nanosToMicrosStr(util::perfTracker.ns(NM_)) << "us" << util::endl; \
			}
			PRINT_TIME_("app.frame")
			PRINT_TIME_("app.assembleFrameUbo")
			PRINT_TIME_("app.assembleInstance")
			PRINT_TIME_("app.assembleInstances")
			PRINT_TIME_("app.sleepTime")
			PRINT_TIME_("app.flushInstanceBuffer")
			PRINT_TIME_("app.userInput")
			PRINT_TIME_("app.drawCmd")
			PRINT_TIME_("rpass.acquireImage")
			PRINT_TIME_("rpass.recordCmd")
			PRINT_TIME_("rpass.submitCmd")
			PRINT_TIME_("rpass.present")
			#undef PRINT_TIME
		}
		#endif
	}

}
