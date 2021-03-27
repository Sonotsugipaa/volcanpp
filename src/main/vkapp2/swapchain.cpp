#include "vkapp2/graphics.hpp"

#include "vkapp2/draw.hpp"

#include "vkapp2/constants.hpp"
#include "vkapp2/vulkan_enum_str.hpp"

using namespace vka2;

using util::enum_str;



namespace {

	vk::Extent2D mk_surface_extent(
			const vk::SurfaceCapabilitiesKHR& capabs,
			const vk::Extent2D& desired
	) {
		vk::Extent2D ext = capabs.currentExtent;
		using int_t = decltype(vk::Extent2D::width);
		const auto check_if_unlimited = [&](
				int_t current, int_t desired,
				int_t min, int_t max, int_t& dest
		) {
			if(current >= std::numeric_limits<int_t>::max()) {
				dest = std::clamp(desired, min, max);
			}
		};
		check_if_unlimited(capabs.currentExtent.width, desired.width,
			capabs.minImageExtent.width, capabs.maxImageExtent.width, ext.width);
		check_if_unlimited(capabs.currentExtent.height, desired.height,
			capabs.minImageExtent.height, capabs.maxImageExtent.height, ext.height);
		constexpr auto extstr = [](const vk::Extent2D& ext) {
			return std::to_string(ext.width) + "x" + std::to_string(ext.height); };
		util::logVkDebug()
			<< "Surface extent: chosen " << extstr(ext) << util::endl;
		return ext;
	}


	vk::PresentModeKHR select_present_mode(
			const std::vector<vk::PresentModeKHR>& availModes
	) {
		constexpr vk::PresentModeKHR fallback = vk::PresentModeKHR::eFifo;
		const auto tryMode = [&](vk::PresentModeKHR prefMode) {
			for(const auto& mode : availModes) {
				if(mode == prefMode) {
					util::logVkDebug() << "[+] "
						<< enum_str(prefMode) << " present mode is supported" << util::endl;
					return true;
				}
			}
			util::logVkDebug() << "[ ] "
				<< enum_str(prefMode) << " present mode is not supported" << util::endl;
			return false;
		};

		for(vk::PresentModeKHR mode : PRESENT_MODE_PREFERENCE) {
			if(tryMode(mode))  return mode; }

		util::logVkDebug()
			<< "Using fallback present mode "
			<< enum_str(fallback) << util::endl;
		return fallback;
	}


	vk::SurfaceTransformFlagBitsKHR get_pretransf(
			const vk::SurfaceCapabilitiesKHR& capabs
	) {
		if(capabs.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
			return vk::SurfaceTransformFlagBitsKHR::eIdentity; }
		return capabs.currentTransform;
	}


	vk::CompositeAlphaFlagBitsKHR get_composite_a(
			const vk::SurfaceCapabilitiesKHR& capabs
	) {
		constexpr vk::CompositeAlphaFlagBitsKHR fallback = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		const auto try_ca = [&](vk::CompositeAlphaFlagBitsKHR bit) {
			if(capabs.supportedCompositeAlpha & bit) {
				util::logVkDebug() << "[+] "
					<< enum_str(bit) << " composite alpha is supported" << util::endl;
				return true;
			} else {
				util::logVkDebug() << "[ ] "
					<< enum_str(bit) << " composite alpha is not supported" << util::endl;
				return false;
			}
		};

		for(vk::CompositeAlphaFlagBitsKHR ca : COMPOSITE_ALPHA_PREFERENCE) {
			if(try_ca(ca))  return ca; }

		util::logVkDebug()
			<< "Using fallback composite alpha "
			<< enum_str(fallback) << util::endl;
		return fallback;
	}


	unsigned get_min_img_count(
			const vk::SurfaceCapabilitiesKHR& capabs,
			unsigned short maxConcurrentFrames
	) {
		using int_t = decltype(capabs.maxImageCount);
		auto r = std::clamp<int_t>(capabs.minImageCount + 1,
			std::max<int_t>(maxConcurrentFrames, capabs.minImageCount),
			capabs.maxImageCount);
		util::logVkDebug()
			<< "Surface requires " << capabs.minImageCount << '-' << capabs.maxImageCount
			<< " images, requesting " << r << util::endl;
		return r;
	}

}



namespace vka2 {

	AbstractSwapchain::AbstractSwapchain(
			Application& app,
			const vk::Extent2D& extent,
			unsigned short maxConcurrentFrames,
			vk::SwapchainKHR cached
	):
			application(&app)
	{
		app.device().waitIdle();
		{ // Create swapchain
			util::alloc_tracker.alloc("AbstractSwapchain");
			const vk::SurfaceCapabilitiesKHR& capabs = app.surfaceCapabilities();
			data.extent = mk_surface_extent(capabs, extent);
			vk::SwapchainCreateInfoKHR scInfo;
			std::array<uint32_t, 3> qFams = {
				application->queueFamilyIndices().graphics,
				application->presentQueueFamilyIndex(),
				std::numeric_limits<uint32_t>::max() };
			scInfo.surface = app.surface();
			scInfo.minImageCount = get_min_img_count(capabs, maxConcurrentFrames);
			scInfo.imageFormat = application->surfaceFormat().format;
			scInfo.imageColorSpace = application->surfaceFormat().colorSpace;
			scInfo.imageExtent = data.extent;
			scInfo.imageArrayLayers = 1;
			scInfo.imageUsage =
				vk::ImageUsageFlagBits::eColorAttachment |
				vk::ImageUsageFlagBits::eTransferDst;
			scInfo.pQueueFamilyIndices = qFams.data();
			if(qFams[0] == qFams[1]) {
				scInfo.imageSharingMode = vk::SharingMode::eExclusive;
				scInfo.queueFamilyIndexCount = 1;
			} else {
				scInfo.imageSharingMode = vk::SharingMode::eConcurrent;
				scInfo.queueFamilyIndexCount = 2;
			}
			scInfo.preTransform = get_pretransf(capabs);
			scInfo.compositeAlpha = get_composite_a(capabs);
			scInfo.presentMode = select_present_mode(
				app.physDevice().getSurfacePresentModesKHR(scInfo.surface));
			scInfo.clipped = true;
			scInfo.oldSwapchain = cached;
			handle = application->device().createSwapchainKHR(scInfo);
			if(scInfo.oldSwapchain == vk::SwapchainKHR(nullptr)) {
				util::alloc_tracker.alloc("AbstractSwapchain:handle");
			}
		} { // Create abstract images
			std::vector<vk::Image> get_images =
				application->device().getSwapchainImagesKHR(handle);
			util::logVkDebug()
				<< "Created swapchain " << static_cast<VkSwapchainKHR>(handle)
				<< " (" << get_images.size() << " images)"<< util::endl;
			data.images.reserve(get_images.size());
			for(auto& img : get_images) {
				data.images.emplace_back(std::move(img)); }
			util::alloc_tracker.alloc("AbstractSwapchain:data:images[...]", 5);
		}
	}


	vk::SwapchainKHR AbstractSwapchain::destroy(bool keep) {
		// Destroy all the owned swapchain "images"
		/* NOP, since the only field in Image is not owned by the application
		for(auto& img : data.images) {
			(void); }
		*/
		data.images.clear();
		util::alloc_tracker.dealloc("AbstractSwapchain:data:images[...]", 5);

		if(keep) {
			util::logVkDebug()
				<< "Destroyed swapchain " << static_cast<VkSwapchainKHR>(handle)
				<< " (but keeping the handle)" << util::endl;
			util::alloc_tracker.dealloc("AbstractSwapchain");
			return std::move(handle);
		} else {
			application->device().destroySwapchainKHR(handle);
			util::alloc_tracker.dealloc("AbstractSwapchain:handle");
			util::logVkDebug()
				<< "Destroyed swapchain " << static_cast<VkSwapchainKHR>(handle) << util::endl;
			util::alloc_tracker.dealloc("AbstractSwapchain");
			return nullptr;
		}
	}

}
