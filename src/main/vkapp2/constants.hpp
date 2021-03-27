#pragma once

#include <array>



namespace vka2 {

	inline namespace consts {

		constexpr const char* WINDOW_TITLE = "VkApp++";
		constexpr std::array<unsigned, 3> VKA2_APP_VERSION = { 0, 2, 0 };
		constexpr std::array<unsigned, 3> VKA2_ENGINE_VERSION = { 0, 2, 0 };

		constexpr const char* CONFIG_FILE = "params.cfg";

		constexpr const char* SHADER_PATH_ENV_VAR_NAME = "VKA2_SHADER_PATH";
		constexpr const char* ASSET_PATH_ENV_VAR_NAME = "VKA2_ASSET_PATH";

		constexpr std::array<uint8_t, 4> MISSING_TEXTURE_COLOR = { 0xFF, 0xFF, 0xFF, 0xFF };

		constexpr float LINE_WIDTH = 1.0f;

		constexpr unsigned ESTIMATED_MAX_MODEL_COUNT = 2<<6; // Used to allocate a static amount of descriptor bindings/sets

		constexpr float YAW_TO_PITCH_RATIO = 2.0f / 3.0f;

	}

}
