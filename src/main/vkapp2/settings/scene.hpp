#pragma once

#include <vector>
#include <array>
#include <string>



namespace vka2 {

	struct Scene {

		struct Object {
			std::string modelName = "";
			std::array<float, 3> position = { };
			std::array<float, 3> orientation = { }; // Yaw, pitch, roll
			std::array<float, 3> scale = { 1.0f, 1.0f, 1.0f };
			std::array<float, 4> color = { 1.0f, 1.0f, 1.0f, 1.0f };
		};

		struct Model {
			std::string name = "";
			float minDiffuse = 0.0f;
			float maxDiffuse = 0.3f;
			float minSpecular = 0.0f;
			float maxSpecular = 1.0f;
		};


		std::vector<Object> objects;
		std::vector<Model> models;


		static Scene fromCfg(const std::string& cfgPath);
	};

}
