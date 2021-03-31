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
#include <string>
#include <limits>



namespace vka2 {

	struct Options {
		struct ShaderParams {
			// Where shader files (.spv) are stored.
			std::string shaderPath = "shaders";
			// Limits how close a fragment can be before being discarded.
			float zNear = 0.05f;
			// Limits how far away a fragment can be before being discarded.
			float zFar = 200.0f;
			// Size of the black outline (world units).
			float outlineSize = 1.0f / 30.0f;
			// Gap between fragments in order for an edge to have an outline (world units).
			float outlineDepth = 1.0f / 20.0f;
			// The maximum variation of an outline vertex (relative to outlineSize, should be >= 0).
			float outlineRndMorph = 1.0f / 10.0f;
			// The number of possible light levels when cel shading is enabled.
			unsigned short celLightLevels = 6;
		} shaderParams;
		struct WorldParams {
			// The default color for unused pixels.
			std::array<float, 4> clearColor = { 0.2f, 0.2f, 0.7f, 1.0f };
			// Direction from which light comes from.
			std::array<float, 3> lightDirection = { 1.0f, -1.0f, 1.0f };
			// Where various assets are stored.
			std::string assetPath = "assets";
			// Use the nearest neighbor filter for color textures.
			bool colorNearestFilter:1 = true;
			// Use the nearest neighbor filter for normal textures (a.k.a. normal maps).
			bool normalNearestFilter:1 = true;
		} worldParams;
		struct WindowParams {
			// Size of the window.
			std::array<unsigned, 2> windowExtent = { 1200, 900 };
			// How many pixels can be rendered vertically.
			unsigned short maxVerticalResolution = std::numeric_limits<unsigned short>::max();
			// Whether to start the application in fullscreen mode
			bool initFullscreen:1 = false;
		} windowParams;
		struct ViewParams {
			// The initial view position.
			std::array<float, 3> initialPosition = { 0.0f, 1.0f, -3.0f };
			// The initial pitch of the view.
			float initialPitch = -25.0f;
			// How many degrees the image has to span vertically.
			float fov = 100.0f;
			// How fast the view turns when using the keyboard (degrees / second).
			float viewTurnSpeedKey = 1.5f;
			// How fast the view turns when using the keyboard and the speed modifier is active (degrees / second).
			float viewTurnSpeedKeyMod = 3.5f;
			// How fast the view moves (units / second).
			float viewMoveSpeed = 2.0f;
			// How fast the view moves when the speed modifier is active (units / second).
			float viewMoveSpeedMod = 12.0f;
			// How frequently to render frames (1 / seconds).
			float frameFrequencyS = 60.0f;
			// Whether to use the nearest neighbor filter instead of the linear filter when upscaling the rendered image.
			bool upscaleNearestFilter:1 = true;
			// Use multisampling.
			bool useMultisampling:1 = false;
		} viewParams;


		static Options fromFile(const std::string& path);
	};

}
