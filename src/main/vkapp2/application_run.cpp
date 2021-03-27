#include "vkapp2/graphics.hpp"

#include <filesystem>
#include <random>
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


	struct CtrlSchemeContext {
		glm::vec3 fwdMoveVector; // Only stores positive XYZ input
		glm::vec3 bcwMoveVector; // Only stores negative XYZ input
		glm::vec2 rotate;
		glm::dvec2 lastCursorPos;
		unsigned shaderSelector;
		bool dragView;
	};


	struct GlfwContext {
		Keymap* keymap;
		Application* app;
		CtrlSchemeContext* ctrlCtx;
		float framerateMul;
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
		glm::vec3 lightDirection;
		glm::vec3 position;
		glm::vec2 orientation;
		unsigned frameCounter;
		float turnSpeedKey, moveSpeed;
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


	/** Key bindings will depend on the window AND the control scheme context,
	 * so they must be must stay alive as long as these key bindings can
	 * be used by GLFW. */
	Keymap mk_key_bindings(GLFWwindow* win, CtrlSchemeContext* ctrlCtx) {
		Keymap km;

		km[GLFW_KEY_S] = [=](bool press, unsigned) {
			ctrlCtx->fwdMoveVector.z = press? 1.0f : 0.0f; };
		km[GLFW_KEY_W] = [=](bool press, unsigned) {
			ctrlCtx->bcwMoveVector.z = press? 1.0f : 0.0f; };
		km[GLFW_KEY_D] = [=](bool press, unsigned) {
			ctrlCtx->fwdMoveVector.x = press? 1.0f : 0.0f; };
		km[GLFW_KEY_A] = [=](bool press, unsigned) {
			ctrlCtx->bcwMoveVector.x = press? 1.0f : 0.0f; };
		km[GLFW_KEY_R] = [=](bool press, unsigned) {
			ctrlCtx->bcwMoveVector.y = press? 1.0f : 0.0f; };
		km[GLFW_KEY_F] = [=](bool press, unsigned) {
			ctrlCtx->fwdMoveVector.y = press? 1.0f : 0.0f; };

		km[GLFW_KEY_RIGHT] = [=](bool press, unsigned) {
			ctrlCtx->rotate.x = press? +1.0f : 0.0f; };
		km[GLFW_KEY_LEFT] = [=](bool press, unsigned) {
			ctrlCtx->rotate.x = press? -1.0f : 0.0f; };
		km[GLFW_KEY_UP] = [=](bool press, unsigned) {
			ctrlCtx->rotate.y = press? +1.0f : 0.0f; };
		km[GLFW_KEY_DOWN] = [=](bool press, unsigned) {
			ctrlCtx->rotate.y = press? -1.0f : 0.0f; };

		km[GLFW_KEY_1] = [=](bool press, unsigned) { if(!press) ctrlCtx->shaderSelector = 0; };
		km[GLFW_KEY_2] = [=](bool press, unsigned) { if(!press) ctrlCtx->shaderSelector = 1; };
		km[GLFW_KEY_3] = [=](bool press, unsigned) { if(!press) ctrlCtx->shaderSelector = 2; };
		km[GLFW_KEY_4] = [=](bool press, unsigned) { if(!press) ctrlCtx->shaderSelector = 3; };
		km[GLFW_KEY_5] = [=](bool press, unsigned) { if(!press) ctrlCtx->shaderSelector = 4; };
		km[GLFW_KEY_6] = [=](bool press, unsigned) { if(!press) ctrlCtx->shaderSelector = 5; };

		km[GLFW_KEY_ESCAPE] = [=](bool press, unsigned) {
			glfwSetWindowShouldClose(win, press); };

		km[GLFW_MOUSE_BUTTON_LEFT] = [=](bool press, unsigned) {
			ctrlCtx->dragView = press;
			glfwGetCursorPos(win, &ctrlCtx->lastCursorPos.x, &ctrlCtx->lastCursorPos.x);
		};

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
		dst.ctrlCtx = {
			.fwdMoveVector = { }, .bcwMoveVector = { },
			.rotate = { }, .lastCursorPos = { },
			.shaderSelector = 0, .dragView = false };
		dst.keymap = mk_key_bindings(app.glfwWindow(), &dst.ctrlCtx);
		dst.glfwCtx = {
			.keymap = &dst.keymap, .app = &app,
			.ctrlCtx = &dst.ctrlCtx, .framerateMul = 1.0f / opts.viewParams.frameFrequencyS };
		dst.rngDistr = std::uniform_real_distribution<float>(0.0f, 1.0f);
		dst.turnSpeedKey = opts.viewParams.viewTurnSpeedKey;
		dst.moveSpeed = opts.viewParams.viewMoveSpeed;
		dst.lightDirection = glm::normalize(glm::vec3({
			opts.worldParams.lightDirection[0],
			opts.worldParams.lightDirection[1],
			opts.worldParams.lightDirection[2] }));
		dst.position = {
			-opts.viewParams.initialPosition[0],
			-opts.viewParams.initialPosition[1],
			-opts.viewParams.initialPosition[2] };
		dst.orientation = { 0.0f, opts.viewParams.initialPitch };
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


	void create_render_ctx_rpass(Application& app, RenderContext& dst) {
		CtrlSchemeContext* dstCtrlCtx = &dst.ctrlCtx;
		RenderPass* dstRpass = &dst.rpass;
		Pipeline* dstMainPl = &dst.mainPipeline;
		Pipeline* dstOutlinePl = &dst.outlinePipeline;
		auto* dstObjects = &dst.objects;
		const auto* dstShaders = &dst.shaders;

		std::function buildPipelines = [
				dstRpass, dstMainPl, dstOutlinePl, dstShaders
		] () {
			*dstMainPl = Pipeline(*dstRpass,
				dstShaders->mainVtx, dstShaders->mainFrg, "main", 0,
				false, dstRpass->renderExtent(),vk::SampleCountFlagBits::e1);
			*dstOutlinePl = Pipeline(*dstRpass,
				dstShaders->outlineVtx, dstShaders->outlineFrg, "main", 1,
				true, dstRpass->renderExtent(), vk::SampleCountFlagBits::e1);
		};

		RenderPass::SwapchainOutdatedCallback onSwpchnOod = [
				buildPipelines, dstCtrlCtx, dstMainPl, dstOutlinePl,
				dstObjects
		] (
				RenderPass& rpass
		) {
			static_assert(ubo::Model::set == Texture::samplerDescriptorSet);
			constexpr auto descSetIdx = ubo::Model::set;
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
					rpass.descriptorPool(), rpass.descriptorSetLayouts()[descSetIdx]);
			}
		};

		dst.rpass = RenderPass(app.swapchain(),
			fit_extent_height(
				app.options().windowParams.maxVerticalResolution,
				app.swapchain().data.extent),
			MAX_CONCURRENT_FRAMES, onSwpchnOod);
		buildPipelines();
	}


	void load_ctx_assets(Application& app, RenderContext& dst) {
		static_assert(ubo::Model::set == Texture::samplerDescriptorSet);
		constexpr auto mdlDescSetLayoutIndex = ubo::Model::set;
		std::string assetPath = get_asset_path();
		auto& worldOpts = app.options().worldParams;
		bool mergeVertices = worldOpts.mergeVertices;
		Scene scene;

		// Load the scene
		{
			std::string scenePath = assetPath + "/scene.cfg";
			util::logDebug() << "Reading scene from \"" << scenePath << '"' << util::endl;
			scene = Scene::fromCfg(scenePath);
			util::logDebug() << "Scene has " << scene.objects.size() << " objects" << util::endl;
		}

		// Create objects
		for(auto& obj : scene.objects) {
			Model::ObjSources src;
			src.mdlName = obj.modelName;
			src.objPath = assetPath + "/"s + src.mdlName + ".obj";
			src.textureLoader = [&app, &src, &worldOpts, &assetPath](Texture::Usage usage) {
				switch(usage) {
					case Texture::Usage::eColor: return rd_texture(app,
						assetPath + "/"s + src.mdlName + ".dfs.png", worldOpts.colorNearestFilter, MISSING_TEXTURE_COLOR);
					case Texture::Usage::eNormal: return rd_texture(app,
						assetPath + "/"s + src.mdlName + ".nrm.png", worldOpts.normalNearestFilter, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
					case Texture::Usage::eSpecular: throw std::runtime_error("Unimplemented; " __FILE__ ":" + std::to_string(__LINE__));
					// case Texture::Usage::eSpecular: return rd_texture(app,
					// 	src.objPath + ".spc.png", worldOpts.colorNearestFilter, glm::vec4(1.0f));
					default: throw std::logic_error("invalid value of vka2::Texture::Usage");
				}
			};
			dst.objects.push_back(std::move(Object {
				.mdlWr = ModelWrapper(
					Model::fromObj(app, src, mergeVertices, &dst.mdlCache, &dst.matCache),
					dst.rpass.descriptorPool(),
					dst.rpass.descriptorSetLayouts()[mdlDescSetLayoutIndex]
				),
				.position = glm::vec3(obj.position[0], obj.position[1], obj.position[2]),
				.orientation = glm::vec3(obj.orientation[0], obj.orientation[1], obj.orientation[2]),
				.scale = glm::vec3(obj.scale[0], obj.scale[1], obj.scale[2]),
				.color = glm::vec4(obj.color[0], obj.color[1], obj.color[2], obj.color[3]),
				.rnd = dst.rngDistr(dst.rng)
			}));
		}

		// Associate models with automatically created models
		for(auto& mdl : scene.models) {
			auto found = dst.mdlCache.find(mdl.name);
			if(found != dst.mdlCache.end()) {
				found->second->viewUbo([&dst, &mdl](MemoryView<ubo::Model> ubo) {
					*ubo.data = ubo::Model {
						.minDiffuse = mdl.minDiffuse,
						.maxDiffuse = mdl.maxDiffuse,
						.minSpecular = mdl.minSpecular,
						.maxSpecular = mdl.maxSpecular,
						.rnd = dst.rngDistr(dst.rng) };
					return true;
				});
			}
		}
		util::logDebug()
			<< "Found " << scene.objects.size() << " objects with "
			<< scene.models.size() << " models" << util::endl;
	}


	void create_render_ctx(Application& app, RenderContext& dst) {
		init_render_ctx_pod(app, dst);
		set_user_controls(dst, app.glfwWindow());
		read_ctx_shaders(app, dst);
		create_render_ctx_rpass(app, dst);
		load_ctx_assets(app, dst);
	}


	void destroy_render_ctx(RenderContext& ctx) {
		ctx.outlinePipeline.destroy();
		ctx.mainPipeline.destroy();
		ctx.rpass.destroy();
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
		RenderContext ctx { };
		create_render_ctx(*this, ctx);
		{
			{
				{
					set_static_ubo(ctx.rpass, opts);
				} {
					size_t vtxCount = 0;
					for(const auto& obj : ctx.objects) {
						vtxCount += obj.mdlWr->idxCount(); }
					util::logDebug() << "Rendering " << vtxCount << " vertices each frame" << util::endl;
				}
				while(! glfwWindowShouldClose(_data.glfwWin)) {
					mat4 orientationMat = mat4(1.0f);
					glfwPollEvents();
					{
						constexpr auto rad360 = glm::radians(360.0f);
						vec2 actualRotate = {
							ctx.ctrlCtx.rotate.x,
							ctx.ctrlCtx.rotate.y * YAW_TO_PITCH_RATIO };
						ctx.orientation += ctx.turnSpeedKey * actualRotate * ctx.glfwCtx.framerateMul;
						ctx.orientation.x -= std::floor(ctx.orientation.x / rad360) * rad360;
						ctx.orientation.y -= std::floor(ctx.orientation.y / rad360) * rad360;
						orientationMat = glm::rotate(orientationMat,
							ctx.orientation.y,
							vec3(1.0f, 0.0f, 0.0f));
						orientationMat = glm::rotate(orientationMat,
							ctx.orientation.x,
							vec3(0.0f, 1.0f, 0.0f));
						vec3 deltaPos = ctx.moveSpeed * ctx.glfwCtx.framerateMul *
							(ctx.ctrlCtx.fwdMoveVector - ctx.ctrlCtx.bcwMoveVector);
						vec4 deltaPosRotated = glm::transpose(orientationMat) * vec4(deltaPos, 1.0f);
						ctx.position += vec3(deltaPosRotated);
					}
					auto draw = [&ctx](
							RenderPass::FrameHandle& fh, vk::CommandBuffer cmd,
							const Object& obj, const push_const::Object& objPushConst,
							Pipeline& pipeline
					) {
						cmd.pushConstants(ctx.rpass.pipelineLayout(),
							vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
							0, sizeof(push_const::Object), &objPushConst);
						cmd.bindPipeline(
							vk::PipelineBindPoint::eGraphics, pipeline.handle());
						cmd.bindVertexBuffers(0, obj.mdlWr->vtxBuffer().handle, { 0 });
						cmd.bindIndexBuffer(obj.mdlWr->idxBuffer().handle,
							0, Vertex::INDEX_TYPE);
						fh.bindModelDescriptorSet(cmd, obj.mdlWr.descSet());
						cmd.drawIndexed(obj.mdlWr->idxCount(), 1, 0, 0, 0);
					};
					ubo::Frame frameUbo;
					frameUbo.viewTransf = mat4(1.0f);
					frameUbo.viewTransf = orientationMat * frameUbo.viewTransf;
					frameUbo.viewTransf = glm::translate(frameUbo.viewTransf, ctx.position);
					frameUbo.lightDirection = ctx.lightDirection;
					frameUbo.shaderSelector = ctx.ctrlCtx.shaderSelector;
					frameUbo.rnd = ctx.rngDistr(ctx.rng);
					ctx.rpass.runRenderPass(frameUbo, { }, { }, std::vector {
						std::function([&](RenderPass::FrameHandle& fh, vk::CommandBuffer cmd) {
							for(const auto& obj : ctx.objects) {
								push_const::Object objPushConst;
								objPushConst.modelTransf = glm::mat4(1.0f);
								objPushConst.modelTransf = glm::translate(
									objPushConst.modelTransf, obj.position);
								objPushConst.modelTransf = glm::rotate(objPushConst.modelTransf,
									glm::radians(obj.orientation.y), glm::vec3(1.0f, 0.0f, 0.0f));
								objPushConst.modelTransf = glm::rotate(objPushConst.modelTransf,
									glm::radians(obj.orientation.x), glm::vec3(0.0f, 1.0f, 0.0f));
								objPushConst.modelTransf = glm::rotate(objPushConst.modelTransf,
									glm::radians(obj.orientation.z), glm::vec3(0.0f, 0.0f, 1.0f));
								objPushConst.modelTransf = glm::scale(objPushConst.modelTransf,
									obj.scale);
								objPushConst.colorMul = obj.color;
								objPushConst.rnd = obj.rnd;
								draw(fh, cmd, obj, objPushConst, ctx.mainPipeline);
							}
						}),
						std::function([&](RenderPass::FrameHandle& fh, vk::CommandBuffer cmd) {
							for(const auto& obj : ctx.objects) {
								push_const::Object objPushConst;
								objPushConst.modelTransf = glm::mat4(1.0f);
								objPushConst.modelTransf = glm::translate(
									objPushConst.modelTransf, obj.position);
								objPushConst.modelTransf = glm::rotate(objPushConst.modelTransf,
									glm::radians(obj.orientation.y), glm::vec3(1.0f, 0.0f, 0.0f));
								objPushConst.modelTransf = glm::rotate(objPushConst.modelTransf,
									glm::radians(obj.orientation.x), glm::vec3(0.0f, 1.0f, 0.0f));
								objPushConst.modelTransf = glm::rotate(objPushConst.modelTransf,
									glm::radians(obj.orientation.z), glm::vec3(0.0f, 0.0f, 1.0f));
								objPushConst.modelTransf = glm::scale(objPushConst.modelTransf,
									obj.scale);
								objPushConst.colorMul = obj.color;
								objPushConst.rnd = obj.rnd;
								draw(fh, cmd, obj, objPushConst, ctx.outlinePipeline);
							}
						})
					});
					util::sleep_s(ctx.glfwCtx.framerateMul);
					++ctx.frameCounter;
				}
			}
		}
		destroy_render_ctx(ctx);
	}

}