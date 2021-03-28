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



#include <vkapp2/settings/scene.hpp>

#include <libconfig.h++>

#include <filesystem>



namespace vka2 {

	using namespace libconfig;


	Scene Scene::fromCfg(const std::string& cfgPath) {
		Scene r;
		Config cfg;
		cfg.readFile(cfgPath.c_str());
		{
			#define GET_VALUE(_N, _T) if(elem.exists(#_N)) obj._N = elem[#_N].operator _T();
			#define GET_ARRAY(_N) if(elem.exists(#_N)) std::copy( \
				elem[#_N].begin(), \
				elem[#_N].begin() + std::min<int>(obj._N.size(), elem[#_N].end() - elem[#_N].begin()), \
				obj._N.begin());
			// --
			auto& objs = cfg.getRoot()["objects"];
			for(auto& elem : objs) {
				Object obj;
				GET_VALUE(modelName,   std::string)
				GET_ARRAY(position)
				GET_ARRAY(orientation)
				GET_ARRAY(color)
				GET_ARRAY(scale)
				r.objects.emplace_back(std::move(obj));
			}
			#undef GET_VALUE
			#undef GET_ARRAY
		} {
			#define GET_VALUE(_N, _T) if(elem.exists(#_N)) mdl._N = elem[#_N].operator _T();
			#define GET_ARRAY(_N) if(elem.exists(#_N)) std::copy( \
				elem[#_N].begin(), \
				elem[#_N].begin() + std::min<int>(mdl._N.size(), elem[#_N].end() - elem[#_N].begin()), \
				mdl._N.begin());
			// --
			auto& mdls = cfg.getRoot()["models"];
			for(auto& elem : mdls) {
				Model mdl;
				GET_VALUE(name,        std::string)
				GET_VALUE(minDiffuse,  float)
				GET_VALUE(maxDiffuse,  float)
				GET_VALUE(minSpecular, float)
				GET_VALUE(maxSpecular, float)
				GET_VALUE(mergeVertices, bool)
				r.models.emplace_back(std::move(mdl));
			}
			#undef GET_VALUE
			#undef GET_ARRAY
		}
		return r;
	}

}
