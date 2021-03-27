#include "vkapp2/graphics.hpp"

using namespace vka2;



namespace {

	std::vector<std::string> hasRequiredFeatures(vk::PhysicalDeviceFeatures& f) {
		std::vector<std::string> missing;
		#define CHECK_FEATURE(_NM) \
			if(! _NM) { missing.push_back(#_NM); }
		//
			CHECK_FEATURE(f.geometryShader)
			CHECK_FEATURE(f.samplerAnisotropy)
			CHECK_FEATURE(f.largePoints)
		#undef CHECK_FEATURE
		return missing;
	}

}



namespace vka2 {

	std::pair<vk::PhysicalDevice, vk::PhysicalDeviceFeatures>
	selectPhysicalDevice(vk::Instance vkInstance) {
		using namespace std::string_literals;
		auto dev = vkInstance.enumeratePhysicalDevices()[0];
		auto features = dev.getFeatures();
		auto missing = hasRequiredFeatures(features);
		if(! missing.empty()) {
			for(auto& miss : missing) {
				util::logVkDebug() << "Missing feature: "s << miss << util::endl; }
			throw std::runtime_error("The chosen device is missing "s +
				std::to_string(missing.size()) + " features"s);
		}
		return { dev, features };
	}


	void tryWaitForFences(vk::Device dev,
			const vk::ArrayProxy<const vk::Fence>& fences,
			vk::Bool32 waitAll, uint64_t timeout
	) {
		auto result = dev.waitForFences(fences, waitAll, timeout);
		if(result != vk::Result::eSuccess) {
			throw std::runtime_error(formatVkErrorMsg(
				"failed to wait on a fence", vk::to_string(result)));
		}
	}

}
