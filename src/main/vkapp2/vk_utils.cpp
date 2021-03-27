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
