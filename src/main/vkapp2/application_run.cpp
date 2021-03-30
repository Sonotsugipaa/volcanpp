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
		bool speedMod;
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
		float turnSpeedKey, moveSpeed, moveSpeedMod;
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
		km[GLFW_KEY_7] = [=](bool press, unsigned) { if(!press) ctrlCtx->shaderSelector = 6; };

		km[GLFW_KEY_LEFT_SHIFT] = [=](bool press, unsigned) { ctrlCtx->speedMod = press; };

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
			.shaderSelector = 0, .dragView = false, .speedMod = false };
		dst.keymap = mk_key_bindings(app.glfwWindow(), &dst.ctrlCtx);
		dst.glfwCtx = {
			.keymap = &dst.keymap, .app = &app,
			.ctrlCtx = &dst.ctrlCtx, .framerateMul = 1.0f / opts.viewParams.frameFrequencyS };
		dst.rngDistr = std::uniform_real_distribution<float>(0.0f, 1.0f);
		dst.turnSpeedKey = opts.viewParams.viewTurnSpeedKey;
		dst.moveSpeed = opts.viewParams.viewMoveSpeed;
		dst.moveSpeedMod = opts.viewParams.viewMoveSpeedMod;
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
		std::map<std::string, Scene::Model*> mdlInfoMap;
		std::string assetPath = get_asset_path();
		auto& worldOpts = app.options().worldParams;
		Scene scene;
		{ // Load the scene
			std::string scenePath = assetPath + "/scene.cfg";
			util::logDebug() << "Reading scene from \"" << scenePath << '"' << util::endl;
			scene = Scene::fromCfg(scenePath);
			util::logDebug() << "Scene has " << scene.objects.size() << " objects" << util::endl;
		} { // Make name -> model associations
			for(auto& mdlInfo : scene.models) {
				mdlInfoMap[mdlInfo.name] = &mdlInfo; }
		} { // Create objects
			for(auto& objInfo : scene.objects) {
				Model::ObjSources src;
				src.mdlName = objInfo.modelName;
				src.objPath = assetPath + "/"s + src.mdlName + ".obj";
				src.textureLoader = [&app, &src, &worldOpts, &assetPath](Texture::Usage usage) {
					switch(usage) {
						case Texture::Usage::eColor: return rd_texture(app,
							assetPath + "/"s + src.mdlName + ".dfs.png",
							worldOpts.colorNearestFilter, MISSING_TEXTURE_COLOR);
						case Texture::Usage::eNormal: return rd_texture(app,
							assetPath + "/"s + src.mdlName + ".nrm.png",
							worldOpts.normalNearestFilter, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
						case Texture::Usage::eSpecular: throw std::runtime_error("Unimplemented; " __FILE__ ":" + std::to_string(__LINE__));
						// case Texture::Usage::eSpecular: return rd_texture(app,
						// 	src.objPath + ".spc.png", worldOpts.colorNearestFilter, glm::vec4(1.0f));
						default: throw std::logic_error("invalid value of vka2::Texture::Usage");
					}
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
							.expSpecular = mdlInfo.expSpecular,
							.maxSpecular = mdlInfo.maxSpecular,
							.rnd = dst.rngDistr(dst.rng) };
						return true;
					});
				}
			}
		}
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


	void process_input(
			RenderContext& ctx, glm::mat4& orientationMat
	) {
		constexpr auto rad360 = glm::radians(360.0f);
		{ // Modify the current orientation based on the input state
			glm::vec2 actualRotate = {
				ctx.ctrlCtx.rotate.x,
				ctx.ctrlCtx.rotate.y * YAW_TO_PITCH_RATIO };
			ctx.orientation += ctx.turnSpeedKey * actualRotate * ctx.glfwCtx.framerateMul;
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
			glm::vec3 deltaPos = adjustedMoveSpeed * ctx.glfwCtx.framerateMul *
				(ctx.ctrlCtx.fwdMoveVector - ctx.ctrlCtx.bcwMoveVector);
			glm::vec4 deltaPosRotated = glm::transpose(orientationMat) * glm::vec4(deltaPos, 1.0f);
			ctx.position += glm::vec3(deltaPosRotated);
		}
	}


	void mk_obj_push_const(
			const std::vector<Object>& objects,
			std::vector<push_const::Object>& dst
	) {
		dst.reserve(dst.size() + objects.size());
		for(const auto& obj : objects) {
			push_const::Object newPc;
			newPc.modelTransf = glm::mat4(1.0f);
			newPc.modelTransf = glm::translate(newPc.modelTransf, obj.position);
			newPc.modelTransf = glm::rotate(newPc.modelTransf,
				glm::radians(obj.orientation.y), glm::vec3(1.0f, 0.0f, 0.0f));
			newPc.modelTransf = glm::rotate(newPc.modelTransf,
				glm::radians(obj.orientation.x), glm::vec3(0.0f, 1.0f, 0.0f));
			newPc.modelTransf = glm::rotate(newPc.modelTransf,
				glm::radians(obj.orientation.z), glm::vec3(0.0f, 0.0f, 1.0f));
			newPc.modelTransf = glm::scale(newPc.modelTransf, obj.scale);
			newPc.colorMul = obj.color;
			newPc.rnd = obj.rnd;
			dst.push_back(newPc);
		}
		assert(dst.size() == objects.size());
	}


	void mk_frame_ubo(
			RenderContext& ctx,
			const glm::mat4& orientationMat,
			ubo::Frame& dst
	) {
		dst.viewTransf = glm::mat4(1.0f);
		dst.viewTransf = orientationMat * dst.viewTransf;
		dst.viewTransf = glm::translate(dst.viewTransf, ctx.position);
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
		RenderContext ctx { };
		std::vector<push_const::Object> objPushConsts;
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
					process_input(ctx, orientationMat);
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
					mk_frame_ubo(ctx, orientationMat, frameUbo);
					objPushConsts.clear();
					mk_obj_push_const(ctx.objects, objPushConsts);
					ctx.rpass.runRenderPass(frameUbo, { }, { }, std::vector {
						std::function([&](RenderPass::FrameHandle& fh, vk::CommandBuffer cmd) {
							assert(objPushConsts.size() == ctx.objects.size());
							for(size_t i=0; i < ctx.objects.size(); ++i) {
								draw(fh, cmd, ctx.objects[i], objPushConsts[i], ctx.mainPipeline); }
						}),
						std::function([&](RenderPass::FrameHandle& fh, vk::CommandBuffer cmd) {
							assert(objPushConsts.size() == ctx.objects.size());
							for(size_t i=0; i < ctx.objects.size(); ++i) {
								draw(fh, cmd, ctx.objects[i], objPushConsts[i], ctx.outlinePipeline); }
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
