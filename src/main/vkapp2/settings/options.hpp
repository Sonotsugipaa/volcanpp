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
			// Give normals a pixelated feel by merging vertices together.
			bool mergeVertices:1 = false;
		} worldParams;
		struct WindowParams {
			// Size of the window.
			std::array<unsigned, 2> windowExtent = { 1200, 900 };
			// How many pixels can be rendered vertically.
			unsigned short maxVerticalResolution = std::numeric_limits<unsigned short>::max();
		} windowParams;
		struct ViewParams {
			// The initial view position.
			std::array<float, 3> initialPosition = { 0.0f, 1.0f, -3.0f };
			// The initial pitch of the view.
			float initialPitch = -25.0f;
			// How many degrees the image has to span vertically.
			float fov = 100.0f;
			// How fast the view turns when using the keyboard (degrees / second)
			float viewTurnSpeedKey = 2.5f;
			// How fast the view moves (units / second)
			float viewMoveSpeed = 4.0f;
			// How frequently to render frames (1 / seconds).
			float frameFrequencyS = 61.0f;
			// Whether to use the nearest neighbor filter instead of the linear filter when upscaling the rendered image.
			bool upscaleNearestFilter:1 = true;
			// Use multisampling.
			bool useMultisampling:1 = false;
		} viewParams;


		static Options fromFile(const std::string& path);
	};

}
