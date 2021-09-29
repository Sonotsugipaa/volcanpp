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

#include <vector>
#include <array>
#include <string>



namespace vka2 {

	struct Scene {

		struct Object {
			std::string meshName = "";
			std::string materialName = "";
			std::array<float, 3> position = { };
			std::array<float, 3> orientation = { }; // Yaw, pitch, roll
			std::array<float, 3> scale = { 1.0f, 1.0f, 1.0f };
			std::array<float, 4> color = { 1.0f, 1.0f, 1.0f, 1.0f };
		};

		struct Material {
			std::string name = "";
			float ambient = 0.0f;
			float diffuse = 0.7f;
			float specular = 0.3f;
			float shininess = 16.0f;
			uint_least16_t celLevels = 6;
			bool mergeVertices:1 = false;
		};


		std::vector<Object> objects;
		std::vector<Material> materials;
		std::array<float, 4> pointLight;


		static Scene fromCfg(const std::string& cfgPath);
	};

}
