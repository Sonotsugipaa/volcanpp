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
				r.models.emplace_back(std::move(mdl));
			}
			#undef GET_VALUE
			#undef GET_ARRAY
		}
		return r;
	}

}
