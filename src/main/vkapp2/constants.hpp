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



#pragma once

#include <array>



namespace vka2 {

	inline namespace consts {

		constexpr const char* WINDOW_TITLE = "VkApp++";
		constexpr std::array<unsigned, 3> VKA2_APP_VERSION = { 0, 2, 0 };
		constexpr std::array<unsigned, 3> VKA2_ENGINE_VERSION = { 0, 2, 0 };

		/* Runtime checks are performed to see if a resolution is valid.
		 * Using a window extent higher than RESOLUTION_HARD_LIMIT
		 * should throw an exception before allocating unreasonably huge
		 * swapchain images: being unsafe about this kind of thing
		 * caused many system soft-locking driver crashes already. */
		constexpr unsigned RESOLUTION_HARD_LIMIT = (2<<15)-1;

		constexpr const char* CONFIG_FILE = "params.cfg";

		constexpr const char* SHADER_PATH_ENV_VAR_NAME = "VKA2_SHADER_PATH";
		constexpr const char* ASSET_PATH_ENV_VAR_NAME = "VKA2_ASSET_PATH";

		constexpr std::array<uint8_t, 4> MISSING_TEXTURE_COLOR = { 0xFF, 0xFF, 0xFF, 0xFF };

		constexpr float LINE_WIDTH = 1.0f;

		/** Used to allocate a static amount of descriptor bindings. */
		constexpr unsigned MAX_OBJECT_COUNT = 1024;

		constexpr float YAW_TO_PITCH_RATIO = 2.0f / 3.0f;

		/** The main thread has to sleep for an arbitrary amount of time
		 * in order to throttle the frame rate: it is computed as
		 * `frametime / SLEEPS_PER_FRAME`, and it's ideally slightly higher
		 * than an integer number in order to account for non-deterministic
		 * inaccuracies.
		 *
		 * Higher numbers result in more precise frame times, but causes
		 * the main thread to trigger more context switches. */
		constexpr float SLEEPS_PER_FRAME = 3.05f;

	}

}
