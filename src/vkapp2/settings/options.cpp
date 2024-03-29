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



#include "vkapp2/settings/options.hpp"

#include <filesystem>

#include <libconfig.h++>

#include "util/util.hpp"



namespace {

	/* Maps a type to its corresponding cast operator type, because libconfig++
	 * isn't friendly with generic programming. Which means there's more
	 * generic programming. */
	template<typename T> struct cfg_typeof { using type = T; };
	template<typename T> using cfg_typeof_t = typename cfg_typeof<T>::type;

	#define MAP_CFG_TYPE(_FROM, _TO) \
	template<> struct cfg_typeof<_FROM> { using type = _TO; }; \
	template<> struct cfg_typeof<const _FROM> { using type = const _TO; };
		MAP_CFG_TYPE(char,                   signed int          )
		MAP_CFG_TYPE(signed char,            signed int          )
		MAP_CFG_TYPE(signed short,           signed int          )
		MAP_CFG_TYPE(unsigned char,          signed int          )
		MAP_CFG_TYPE(unsigned short,         signed int          )
		MAP_CFG_TYPE(unsigned int,           signed int          )
		MAP_CFG_TYPE(unsigned long,          signed long int     )
		MAP_CFG_TYPE(unsigned long long,     signed long long int)
		MAP_CFG_TYPE(float,                  double              )
	#undef MAP_CFG_TYPE

	template<typename T> libconfig::Setting::Type cfg_enumof;

	#define MAP_CFG_TYPE(_T, _E) \
	template<> libconfig::Setting::Type cfg_enumof<_T> = libconfig::Setting::_E;
		MAP_CFG_TYPE(std::string, TypeString )
		MAP_CFG_TYPE(int,         TypeInt    )
		MAP_CFG_TYPE(double,      TypeFloat  )
		MAP_CFG_TYPE(bool,        TypeBoolean)
	#undef MAP_CFG_TYPE

	template<typename T> T get_cfg_value(
			libconfig::Setting& cfgRoot,
			const char* group, const char* key, const cfg_typeof_t<T>& defVal
	) {
		libconfig::Setting& cfgGroup = cfgRoot.exists(group)?
			cfgRoot[group] : cfgRoot.add(group, libconfig::Setting::TypeGroup);
		if(! cfgGroup.exists(key)) {
			util::logError() << "Configuration '" << key << "' not found; using " << defVal << util::endl;
			cfgGroup.add(key, cfg_enumof<cfg_typeof_t<T>>) = defVal;
			return defVal;
		} else {
			return cfgGroup[key].operator cfg_typeof_t<T>();
		}
	}

	template<typename T, typename Container> Container get_cfg_array(
			libconfig::Setting& cfgRoot,
			const char* group, const char* key, const Container& defVal,
			std::size_t count
	) {
		libconfig::Setting& cfgGroup = cfgRoot.exists(group)?
			cfgRoot[group] : cfgRoot.add(group, libconfig::Setting::TypeGroup);
		if(! cfgGroup.exists(key)) {
			util::logError() << "Configuration '" << key << "' not found; using { ";
			if(! defVal.empty()) {
				util::logError() << '\'' << defVal.front(); }
			for(size_t i=1; i < defVal.size(); ++i) {
				util::logError() << "', '" << defVal[i] << '\''; }
			util::logError() << " }" << util::endl;
			auto& setting = cfgGroup.add(key, libconfig::Setting::TypeArray);
			for(auto& elem : defVal) {
				setting.add(cfg_enumof<cfg_typeof_t<T>>) = static_cast<cfg_typeof_t<T>>(elem); }
			return defVal;
		} else {
			auto& setting = cfgGroup[key];
			Container r = defVal;
			auto beg = setting.begin();
			auto end = setting.end();
			if(setting.getLength() < static_cast<std::make_signed<std::size_t>::type>(count)) {
				auto missing = count - setting.getLength();
				for(decltype(missing) i=0; i < missing; ++i) {
					setting.add(cfg_enumof<cfg_typeof_t<T>>); }
				end = setting.end();
			}
			std::move(beg, end, r.begin());
			return r;
		}
	}

}



namespace vka2 {

	Options Options::fromFile(const std::string& path) {
		using namespace libconfig;
		Config cfg;
		Options r;
		cfg.setOption(Config::OptionAutoConvert, true);
		if(std::filesystem::is_regular_file(path)) {
			cfg.readFile(path.c_str()); }
		auto& root = cfg.getRoot();
		#define GET_SETTING(_S, _V, _T) r._S._V = get_cfg_value<_T>(root, #_S, #_V, r._S._V)
		#define GET_SETTING_ARRAY(_S, _V, _T) r._S._V = get_cfg_array<_T>(root, #_S, #_V, r._S._V, r._S._V.size())
		GET_SETTING(shaderParams, shaderPath, std::string);
		GET_SETTING(shaderParams, zNear, float);
		GET_SETTING(shaderParams, zFar, float);
		GET_SETTING(shaderParams, outlineSize, float);
		GET_SETTING(shaderParams, outlineDepth, float);
		GET_SETTING(shaderParams, outlineRndMorph, float);
		GET_SETTING_ARRAY(worldParams, clearColor, float);
		GET_SETTING_ARRAY(worldParams, lightDirection, float);
		GET_SETTING(worldParams, diffuseNearestFilter, bool);
		GET_SETTING(worldParams, specularNearestFilter, bool);
		GET_SETTING(worldParams, normalNearestFilter, bool);
		GET_SETTING_ARRAY(windowParams, windowExtent, unsigned);
		GET_SETTING_ARRAY(windowParams, fullscreenExtent, unsigned);
		GET_SETTING(windowParams, maxVerticalResolution, unsigned);
		GET_SETTING(windowParams, initFullscreen, bool);
		GET_SETTING(windowParams, useMultisampling, bool);
		GET_SETTING_ARRAY(viewParams, initialPosition, float);
		GET_SETTING(viewParams, initialYaw, float);
		GET_SETTING(viewParams, initialPitch, float);
		GET_SETTING(viewParams, fov, float);
		GET_SETTING(viewParams, viewTurnSpeedKey, float);
		GET_SETTING(viewParams, viewTurnSpeedKeyMod, float);
		GET_SETTING(viewParams, viewMoveSpeed, float);
		GET_SETTING(viewParams, viewMoveSpeedMod, float);
		GET_SETTING(viewParams, frameFrequencyS, float);
		GET_SETTING(viewParams, upscaleNearestFilter, bool);
		#undef GET_SETTING
		#undef GET_SETTING_ARRAY
		cfg.writeFile(path.c_str());
		return r;
	}

}
