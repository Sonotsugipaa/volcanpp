#pragma once

#include <string>
#include <array>

#include <vulkan/vulkan.hpp>

#include "vkapp2/vulkan_enum_str.hpp"



namespace vka2 {

	struct Runtime {
		vk::Format depthOptimalFmt = vk::Format::eD32Sfloat;
		unsigned samplerAnisotropy = 1;
	};


	template<typename Param>
	std::string formatVkErrorMsg(
			const std::string& message, Param param
	) {
		using namespace std::string_literals;
		return message + " ("s + param + ")"s;
	}

}
