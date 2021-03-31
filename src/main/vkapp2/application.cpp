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

#include <iostream>
#include <set>
#include <filesystem>

#include <libconfig.h++>

#include "vkapp2/draw.hpp"
#include "vkapp2/constants.hpp"

using namespace vka2;
using namespace std::string_literals;

using util::enum_str;



namespace {

	#ifndef NDEBUG
		constexpr std::array<const char*, 0> instanceExtensions = { };

		constexpr std::array<const char*, 1> activeLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		constexpr std::array<const char*, 1> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			// "VK_KHR_portability_subset" // Can be used by overridden layer settings, notably by VK_LAYER_LUNARG_device_simulation
			// "wideLines" // Promoted to 1.2
			// "VK_EXT_descriptor_indexing" // Also promoted to 1.2
		};
	#else
		constexpr std::array<const char*, 0> instanceExtensions = { };

		constexpr std::array<const char*, 0> activeLayers = { };

		constexpr std::array<const char*, 1> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
			// "wideLines" // Promoted to 1.2
			// "VK_EXT_descriptor_indexing" // Also promoted to 1.2
		};
	#endif

	constexpr vk::PhysicalDeviceFeatures features = ([]() {
		vk::PhysicalDeviceFeatures r;
		r.geometryShader = true;
		r.largePoints = true;
		r.samplerAnisotropy = true;
		r.fillModeNonSolid = true;
		r.wideLines = true;
		return r;
	} ());


	vk::Instance mk_vk_instance(const vk::ApplicationInfo& appInfo) {
		std::vector<const char*> actualExtensions;
		actualExtensions.insert(actualExtensions.end(),
			instanceExtensions.begin(), instanceExtensions.end());
		{
			uint32_t extCount;
			const char** ptr = glfwGetRequiredInstanceExtensions(&extCount);
			for(uint32_t i=0; i < extCount; ++i) {
				actualExtensions.push_back(ptr[i]); }
		}
		return vk::createInstance(vk::InstanceCreateInfo(
			vk::InstanceCreateFlags(), &appInfo, { }, actualExtensions));
	}


	vk::PhysicalDevice get_ph_dev(
			vk::Instance vkInstance,
			vk::PhysicalDeviceFeatures* destDevFeatures
	) {
		auto r = vka2::selectPhysicalDevice(vkInstance);
		const auto& props = r.first.getProperties();
		util::logVkDebug()
			<< "Using physical device " << props.deviceID << ':' << util::endl
			<< " - Name: " << props.deviceName << util::endl << " - Type: "
			<< enum_str(props.deviceType) << util::endl;
		*destDevFeatures = r.second;
		return r.first;
	}


	Queues::FamilyIndices find_qfam_idxs(vk::PhysicalDevice pdev) {
		constexpr static auto errmsg = [](const char* qtype, vk::PhysicalDevice pdev) {
			return "no suitable queue for "s + qtype + " ops on device "s +
				std::to_string(pdev.getProperties().deviceID);
		};
		auto qFamProps = pdev.getQueueFamilyProperties();

		const auto findIdx = [&](
				const std::string& qtype,
				vk::QueueFlagBits flag,
				unsigned offset
		) -> unsigned {
			for(unsigned i=0; i < qFamProps.size(); ++i) {
				unsigned iOff = (i+offset) % qFamProps.size();
				if(qFamProps[iOff].queueFlags & flag) {
					util::logVkDebug()
						<< "Using queue family " << iOff << " for "
						<< enum_str(flag) << " queues" << util::endl;
					return iOff;
				}
			}
			throw std::runtime_error(errmsg(enum_str(flag).c_str(), pdev));
		};

		Queues::FamilyIndices r;
		r.graphics = findIdx("graphics", vk::QueueFlagBits::eGraphics, 0);
		r.compute = findIdx("compute", vk::QueueFlagBits::eCompute, r.graphics + 1);
		r.transfer = findIdx("transfer", vk::QueueFlagBits::eTransfer, r.compute + 1);
		return r;
	}


	std::pair<decltype(Queues::FamilyIndices::compute), vk::Queue>
	find_present_idx(
			vk::PhysicalDevice pdev,
			const Queues::FamilyIndices& qFamIdx,
			const Queues& queues,
			const vk::SurfaceKHR& surface
	) {
		#define TRY_FAM(_FAM) \
			if(pdev.getSurfaceSupportKHR(qFamIdx._FAM, surface)) { \
				util::logVkDebug() \
					<< "Using queue family " << qFamIdx._FAM \
					<< " for the present queue" << util::endl; \
				return { qFamIdx._FAM, queues._FAM }; \
			} \
		//
			TRY_FAM(transfer)
			TRY_FAM(compute)
			TRY_FAM(graphics)
		#undef TRY_FAM
		throw std::runtime_error("could not find a queue with present support");
	}


	struct mk_q_create_infos_result_t {
		using pos_t = unsigned[2]; // Family idx, queue idx
		pos_t computePos, transferPos, graphicsPos;
		std::vector<vk::DeviceQueueCreateInfo> createInfos;
	};

	mk_q_create_infos_result_t mk_q_create_infos(
			const Queues::FamilyIndices& qFamIdx,
			const std::vector<vk::QueueFamilyProperties>& qFamProps
	) {
		constexpr auto qPriorities = [](float q_priority) {
			auto r = std::array<float, 3>();
			r.fill(q_priority); return r;
		} (0.0f);
		mk_q_create_infos_result_t r;
		std::map<unsigned, unsigned> qFamsMapAssigned; // How many queues (value) have been marked as assigned to a family (key)
		std::map<unsigned, std::vector<unsigned*>> qAssignments; // What should be put (value) into a CreateInfo for each non-zero family (key)

		const auto assign = [&](unsigned fam, unsigned* posDest) {
			unsigned maxAssign = qFamProps[fam].queueCount;
			auto& qFamsMapAssigned_fam = qFamsMapAssigned[fam];
			posDest[0] = fam;
			posDest[1] = (qFamsMapAssigned_fam) % maxAssign;
			++qFamsMapAssigned_fam;
		};

		assign(qFamIdx.compute, r.computePos);
		assign(qFamIdx.transfer, r.transferPos);
		assign(qFamIdx.graphics, r.graphicsPos);

		for(auto assignment : qFamsMapAssigned) {
			r.createInfos.push_back(vk::DeviceQueueCreateInfo({ },
				assignment.first,
				std::min(assignment.second, qFamProps[assignment.first].queueCount),
				qPriorities.data()));
		}
		util::logVkDebug()
			<< "Assigned compute queue to family " <<  r.computePos[0]
			<< ", index " << r.computePos[1] << util::endl;
		util::logVkDebug()
			<< "Assigned transfer queue to family " << r.transferPos[0]
			<< ", index " << r.transferPos[1] << util::endl;
		util::logVkDebug()
			<< "Assigned graphics queue to family " << r.graphicsPos[0]
			<< ", index " << r.graphicsPos[1] << util::endl;
		return r;
	}


	vk::Device mk_device(
			vk::PhysicalDevice pDev,
			const Queues::FamilyIndices& qFamIdx,
			Queues* queues
	) {
		auto dqcInfos = mk_q_create_infos(
			qFamIdx, pDev.getQueueFamilyProperties());
		vk::DeviceCreateInfo dcInfo;
		dcInfo.setQueueCreateInfos(dqcInfos.createInfos);
		dcInfo.setPEnabledLayerNames(activeLayers);
		dcInfo.setPEnabledExtensionNames(deviceExtensions);
		dcInfo.setPEnabledFeatures(&features);
		auto r = pDev.createDevice(dcInfo);
		queues->compute = r.getQueue(dqcInfos.computePos[0], dqcInfos.computePos[1]);
		queues->transfer = r.getQueue(dqcInfos.transferPos[0], dqcInfos.transferPos[1]);
		queues->graphics = r.getQueue(dqcInfos.graphicsPos[0], dqcInfos.graphicsPos[1]);
		return r;
	}


	VmaAllocator mk_allocator(
			vk::Instance vkInstance,
			vk::PhysicalDevice phdev, vk::Device dev
	) {
		VmaAllocator r;
		VmaAllocatorCreateInfo acInfo = { };
		acInfo.device = dev;
		acInfo.vulkanApiVersion = VK_API_VERSION;
		acInfo.physicalDevice = phdev;
		acInfo.instance = vkInstance;
		VkResult result = vmaCreateAllocator(&acInfo, &r);
		if(result != VK_SUCCESS) {
			throw std::runtime_error(formatVkErrorMsg(
				"failed to create the Vulkan memory allocator", enum_str(result)));
		}
		return r;
	}


	GLFWwindow* mk_window(bool fullscreen, vk::Extent2D ext) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // Do not create the OpenGL context
		glfwWindowHint(GLFW_RESIZABLE, true);
		GLFWmonitor* mon = nullptr;
		bool zeroSize = ext.width * ext.height == 0;
		if(fullscreen) {
			mon = glfwGetPrimaryMonitor();
			if(mon == nullptr) {
				const char* err;
				glfwGetError(&err);
				throw std::runtime_error(
					"failed to acquire the primary monitor with GLFW ("s + err + ")"s);
			}
		}
		if(zeroSize) {
			int widthI, heightI;
			if(! fullscreen) {
				throw std::logic_error("window area cannot be 0 if not in fullscreen mode"s); }
			glfwGetMonitorWorkarea(mon, nullptr, nullptr, &widthI, &heightI);
			ext = vk::Extent2D(widthI, heightI);
			util::logVkDebug() << "Fullscreen window extent automatically set to "
				<< widthI << 'x' << heightI << util::endl;
		}
		if((ext.width + ext.height) * 2 > RESOLUTION_HARD_LIMIT) {
			throw std::logic_error("window perimeter cannot be higher than "s +
				std::to_string(RESOLUTION_HARD_LIMIT));
		}
		GLFWwindow* r = glfwCreateWindow(
			ext.width, ext.height,
			WINDOW_TITLE, mon, nullptr);
		if(r == nullptr) {
			const char* err;
			glfwGetError(&err);
			throw std::runtime_error(
				"failed to create a GLFW window ("s + err + ")"s);
		}
		return r;
	}


	vk::SurfaceKHR mk_window_surface(vk::Instance instance, GLFWwindow* window) {
		VkSurfaceKHR cSurface;
		VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &cSurface);
		if(result != VK_SUCCESS) {
			const char* err;  glfwGetError(&err);
			throw std::runtime_error(formatVkErrorMsg(
				"could not create a window surface",
				(err == nullptr)? enum_str(result) + ": "s : err));
		}
		return cSurface;
	}


	vk::SurfaceFormatKHR select_swapchain_fmt(
			const std::vector<vk::SurfaceFormatKHR>& formats,
			const vk::SurfaceCapabilitiesKHR& capabs

	) {
		auto found = std::find_if(formats.begin(), formats.end(),
			[](const vk::SurfaceFormatKHR& fmt) {
				return
					(fmt.format == vk::Format::eB8G8R8A8Srgb) &&
					(fmt.colorSpace == vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT);
			});
		if(found == formats.end())  found = formats.begin();
		return *found;
	}



	vk::Format select_depthstencil_format(
			vk::PhysicalDevice phdev,
			bool doUseStencil
	) {
		constexpr vk::FormatFeatureFlagBits feats =
			vk::FormatFeatureFlagBits::eDepthStencilAttachment;
		vk::Format r = vk::Format::eUndefined;
		const auto trySelect = [&](vk::Format fmt) {
			constexpr vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
			vk::FormatProperties props;
			phdev.getFormatProperties(fmt, &props);
			if constexpr(tiling == vk::ImageTiling::eOptimal)
			if((props.optimalTilingFeatures & feats) == feats) {
				r = fmt;  return true; }
			if constexpr(tiling == vk::ImageTiling::eLinear)
			if((props.linearTilingFeatures & feats) == feats) {
				r = fmt;  return true; }
			return false;
		};
		if(doUseStencil) {
			for(vk::Format fmt : DEPTH_STENCIL_FMT_PREFERENCE) {
				if(trySelect(fmt)) break; }
		} else {
			for(vk::Format fmt : DEPTH_ONLY_FMT_PREFERENCE) {
				if(trySelect(fmt)) break; }
		}
		if(r == vk::Format::eUndefined) {
			throw std::runtime_error("failed to find a suitable depth/stencil image format"); }
		util::logVkDebug()
			<< "Using depth/stencil image format "
			<< enum_str(r) << util::endl;
		return r;
	}


	vk::SurfaceCapabilitiesKHR check_surface_capabs(
			vk::PhysicalDevice pDev, vk::SurfaceKHR surface
	) {
		auto r = pDev.getSurfaceCapabilitiesKHR(surface);
		auto requiredUsage = vk::ImageUsageFlagBits::eTransferDst;
		if((r.supportedUsageFlags & requiredUsage) != requiredUsage) {
			throw std::runtime_error(
				"surface owned images cannot be directly written to");
		}
		for(
				unsigned i=0;
				i < std::numeric_limits<decltype(requiredUsage)>::digits;
				++i
		) {
			auto bit = vk::ImageUsageFlagBits(1 << i);
			if(requiredUsage & bit & r.supportedUsageFlags) {
				util::logVkDebug() << "Surface images support "
					<< enum_str(bit) << " usage" << util::endl;
			}
		}
		return r;
	}


	void get_runtime_params(
			vk::PhysicalDevice pDev,
			bool doUseStencil,
			const vka2::Options& opts, Runtime* runtimePtr
	) {
		vk::PhysicalDeviceProperties pDevProps = pDev.getProperties();
		runtimePtr->depthOptimalFmt = select_depthstencil_format(pDev, false);
		runtimePtr->samplerAnisotropy = pDevProps.limits.maxSamplerAnisotropy;
		runtimePtr->fullscreen = opts.windowParams.initFullscreen;
		runtimePtr->bestSampleCount = vk::SampleCountFlagBits::e1;
		if(opts.windowParams.useMultisampling) {
			auto supportedSamples =
				pDevProps.limits.framebufferColorSampleCounts &
				pDevProps.limits.framebufferDepthSampleCounts;
			#define TRY_SAMPLES(_E) if(supportedSamples & vk::SampleCountFlagBits::_E) \
				runtimePtr->bestSampleCount = vk::SampleCountFlagBits::_E
			// --
			TRY_SAMPLES(e64); else
			TRY_SAMPLES(e32); else
			TRY_SAMPLES(e16); else
			TRY_SAMPLES(e8); else
			TRY_SAMPLES(e4); else
			TRY_SAMPLES(e2); else
			runtimePtr->bestSampleCount = vk::SampleCountFlagBits::e1;
			#undef TRY_SAMPLES
			util::logVkDebug() << "Best supported sample count is "
				<< util::enum_str(runtimePtr->bestSampleCount) << util::endl;
		} else {
			util::logVkDebug() << "Not using MSAA" << util::endl;
		}
	}

}



int main(int, char**) {
	#ifndef NDEBUG
		#define DO_DEBUG true
	#else
		#define DO_DEBUG false
	#endif
		util::log.setLevel(util::LOG_DEBUG, DO_DEBUG);
		util::log.setLevel(util::LOG_VK_DEBUG, DO_DEBUG);
		// util::log.setLevel(util::LOG_ALLOC, DO_DEBUG);
	#undef DO_DEBUG
	util::log.setLevel(util::LOG_GENERAL, true);
	util::log.setLevel(util::LOG_ERROR, true);
	util::log.setLevel(util::LOG_TIME, true);
	util::log.setLevel(util::LOG_VK_ERROR, true);
	util::log.setLevel(util::LOG_VK_EVENT, true);
	try {
		Application app;
		app.run();
		app.destroy();
		return EXIT_SUCCESS;
	} catch(vk::SystemError& err) {
		std::cerr << "[vk::SystemError] " << err.what() << std::endl;
	} catch(libconfig::FileIOException& err) {
		std::cerr << "[libconfig::FileIOException] " << err.what() << std::endl;
	} catch(std::exception& err) {
		std::cerr << "[std::exception] " << err.what() << std::endl;
	} catch(...) {
		std::cerr << "[unknown_error]" << std::endl;
	}
	return EXIT_FAILURE;
}



namespace vka2 {

	Application::Application():
			_cached_swapchain(nullptr)
	{
		if(! glfwInit()) {
			const char* err;
			glfwGetError(&err);
			throw std::runtime_error(formatVkErrorMsg("failed to initialize GLFW", err));
		}
		_data.options = Options::fromFile(CONFIG_FILE);
		_vk_appinfo = vk::ApplicationInfo(
			"vkapp2", VK_MAKE_VERSION(VKA2_APP_VERSION[0], VKA2_APP_VERSION[1], VKA2_APP_VERSION[2]),
			"vkapp_engine", VK_MAKE_VERSION(VKA2_ENGINE_VERSION[0], VKA2_ENGINE_VERSION[1], VKA2_ENGINE_VERSION[2]),
			VK_API_VERSION);
		_vk_instance = mk_vk_instance(_vk_appinfo);
		_data.pDev = get_ph_dev(_vk_instance, &_data.pDevFeatures);
		_data.pDevFeatures = _data.pDev.getFeatures();
		_data.qFamIdx = find_qfam_idxs(_data.pDev);
		_data.dev = mk_device(_data.pDev, _data.qFamIdx, &_data.queues);  util::alloc_tracker.alloc("Application:_data:dev");
		_data.alloc = mk_allocator(_vk_instance,
			_data.pDev, _data.dev);  util::alloc_tracker.alloc("Application:_data:alloc");
		_data.transferCmdPool = CommandPool(_data.dev, _data.qFamIdx.transfer, true);  util::alloc_tracker.alloc("Application:_data:transferCmdPool");
		_data.graphicsCmdPool = CommandPool(_data.dev, _data.qFamIdx.graphics, true);  util::alloc_tracker.alloc("Application:_data:graphicsCmdPool");
		get_runtime_params(_data.pDev, false, _data.options, &_data.runtime);
		{
			const auto& wParams = _data.options.windowParams;
			auto ext = _data.runtime.fullscreen? wParams.fullscreenExtent : wParams.windowExtent;
			_create_window(_data.runtime.fullscreen, vk::Extent2D(ext[0], ext[1]));
		}
		util::alloc_tracker.alloc("Application");
	}


	void Application::destroy() {
		_destroy_window();
		_data.graphicsCmdPool.destroy();  util::alloc_tracker.dealloc("Application:_data:graphicsCmdPool");
		_data.transferCmdPool.destroy();  util::alloc_tracker.dealloc("Application:_data:transferCmdPool");
		vmaDestroyAllocator(_data.alloc);  util::alloc_tracker.dealloc("Application:_data:alloc");
		_data.dev.destroy();  util::alloc_tracker.dealloc("Application:_data:dev");
		_vk_instance.destroy();
		glfwTerminate();
		util::alloc_tracker.dealloc("Application");
	}


	void Application::_create_window(bool fullscreen, const vk::Extent2D& ext) {
		_data.glfwWin = mk_window(fullscreen, ext);  util::alloc_tracker.alloc("Application:_data:glfwWin");
		_data.surface = mk_window_surface(_vk_instance, _data.glfwWin);  util::alloc_tracker.alloc("Application:_data:surface");
		std::tie(_data.qFamIdxPresent, _data.presentQueue) =
			find_present_idx(_data.pDev, _data.qFamIdx, _data.queues, _data.surface);
		_create_swapchain();
	}


	void Application::_destroy_window() {
		_destroy_swapchain(false);
		_vk_instance.destroySurfaceKHR(_data.surface);  util::alloc_tracker.dealloc("Application:_data:surface");
		glfwDestroyWindow(_data.glfwWin);  util::alloc_tracker.dealloc("Application:_data:glfwWin");
	}


	void Application::_create_swapchain() {
		_data.dev.waitIdle();
		_data.surfaceCapabs = check_surface_capabs(_data.pDev, _data.surface);
		_data.surfaceFmt = select_swapchain_fmt(
			_data.pDev.getSurfaceFormatsKHR(_data.surface), _data.surfaceCapabs);
		_data.swapchain = AbstractSwapchain(*this,
			vk::Extent2D(
				_data.options.windowParams.windowExtent[0],
				_data.options.windowParams.windowExtent[1]),
			MAX_CONCURRENT_FRAMES, _cached_swapchain);
		_cached_swapchain = nullptr; // AFAIK, the old swapchain needn't be destroyed on reuse
	}


	void Application::_destroy_swapchain(bool cache) {
		#ifndef NDEBUG
			/* This method only runs when the swapchain is rebuilt,
			 * or when the Application object dies; nullifying this
			 * data is more or less pointless. */
			_data.surfaceCapabs = decltype(_data.surfaceCapabs)();
			_data.surfaceFmt = decltype(_data.surfaceFmt)();
		#endif
		_data.dev.waitIdle();
		cache = false; // Currently cached swapchains randomly cause the application to inexplicably crash
		if(cache) {
			// Replace the possibly existing cached swapchain with the current one
			if(!_cached_swapchain) {
				_cached_swapchain = _data.swapchain.destroy(true); }
		} else {
			// Drop it
			_data.swapchain.destroy(false);
		}
	}


	void Application::rebuildSwapChain() {
		assert(_data.dev != vk::Device(nullptr));
		_data.dev.waitIdle();
		_destroy_swapchain(true);
		_create_swapchain();
	}


	void Application::setWindowMode(bool value, const vk::Extent2D& ext) {
		if(_data.runtime.fullscreen != value) {
			_destroy_window();
			_data.runtime.fullscreen = value;
			_create_window(value, ext);
		}
	}


	const vk::FormatProperties& Application::getFormatProperties(vk::Format fmt) const noexcept {
		decltype(_cache.fmtProps)::iterator found = _cache.fmtProps.find(fmt);
		if(found != _cache.fmtProps.end()) {
			return found->second; // It is unclear to me whether this is UB
		}
		return _cache.fmtProps[fmt] = _data.pDev.getFormatProperties(fmt);
	}

}
