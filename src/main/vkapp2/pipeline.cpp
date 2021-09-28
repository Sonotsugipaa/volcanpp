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



#include "vkapp2/graphics.hpp"

#include "vkapp2/constants.hpp"
#include "vkapp2/draw.hpp"



namespace {

	template<typename int_t>
	constexpr int_t align_n(
			int_t n,
			typename std::make_unsigned<int_t>::type alignment
	) {
		int_t mod = n % int_t(alignment);
		if constexpr(std::numeric_limits<int_t>::is_signed) {
			mod =
				(mod * (mod >= 0)) |
				((mod + alignment) * (mod < 0));
		}
		return
			(n * (mod==0)) |
			((n + (int_t(alignment) - mod)) * (mod!=0));
	}

	template<typename align_t, typename int_t = size_t>
	constexpr int_t align(int_t n) {
		return align_n(n, sizeof(align_t)); }

	static_assert(align_n(5, 4) == 8);
	static_assert(align_n(7, 4) == 8);
	static_assert(align_n(8, 4) == 8);
	static_assert(align_n(9, 4) == 12);
	static_assert(align_n(9, 5) == 10);
	static_assert(align_n(-1, 4) == 0);
	static_assert(align_n(-4, 4) == -4);
	static_assert(align_n(-5, 4) == -4);


	vk::ShaderModule mk_shader_module(
			vk::Device dev,
			const std::string& spirv
	) {
		size_t alignSize = align<uint32_t>(spirv.size());
		auto spirvAligned = std::vector<unsigned>(alignSize);
		vk::ShaderModuleCreateInfo smcInfo;
		memcpy(spirvAligned.data(), spirv.data(), alignSize);
		smcInfo.pCode = spirvAligned.data();
		smcInfo.codeSize = spirv.size();
		auto r = dev.createShaderModule(smcInfo);
		return r;
	}

}



namespace vka2 {

	const decltype(Vertex::BINDING_DESC) Vertex::BINDING_DESC = []() {
		vk::VertexInputBindingDescription r;
		r.binding = 0;
		r.stride = sizeof(Vertex);
		r.inputRate = vk::VertexInputRate::eVertex;
		return r;
	} ();


	const decltype(Vertex::ATTRIB_DESC) Vertex::ATTRIB_DESC = []() {
		std::array<vk::VertexInputAttributeDescription, 6> r;
		#define _ATTRIB(_LOCATION, _FORMAT, _OFFSET) \
			r[_LOCATION].  binding = 0;  r[_LOCATION].location = _LOCATION; \
			r[_LOCATION]  .format = _FORMAT; \
			r[_LOCATION]  .offset = offsetof(Vertex, _OFFSET);
		// --
			_ATTRIB(0, vk::Format::eR32G32B32Sfloat, pos)
			_ATTRIB(1, vk::Format::eR32G32B32Sfloat, nrm)
			_ATTRIB(2, vk::Format::eR32G32B32Sfloat, nrm_smooth)
			_ATTRIB(3, vk::Format::eR32G32B32Sfloat, tanu)
			_ATTRIB(4, vk::Format::eR32G32B32Sfloat, tanv)
			_ATTRIB(5, vk::Format::eR32G32Sfloat, tex)
		#undef _ATTRIB
		return r;
	} ();


	const decltype(Instance::BINDING_DESC) Instance::BINDING_DESC = []() {
		vk::VertexInputBindingDescription r;
		r.binding = 1;
		r.stride = sizeof(Instance);
		r.inputRate = vk::VertexInputRate::eInstance;
		return r;
	} ();


	const decltype(Instance::ATTRIB_DESC) Instance::ATTRIB_DESC = []() {
		std::array<vk::VertexInputAttributeDescription, 6> r;
		static_assert(sizeof(glm::mat4) == 4 * sizeof(glm::vec4));
		constexpr auto offset = Vertex::ATTRIB_DESC.size();
		#define _ATTRIB(_LOCATION, _FORMAT, _OFFSET) \
			r[(_LOCATION)-offset].  binding = 1;  r[_LOCATION-offset].location = _LOCATION; \
			r[(_LOCATION)-offset]  .format = _FORMAT; \
			r[(_LOCATION)-offset]  .offset = _OFFSET;
		// --
			_ATTRIB(6, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, modelTransf) + (0 * sizeof(glm::vec4)))
			_ATTRIB(7, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, modelTransf) + (1 * sizeof(glm::vec4)))
			_ATTRIB(8, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, modelTransf) + (2 * sizeof(glm::vec4)))
			_ATTRIB(9, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, modelTransf) + (3 * sizeof(glm::vec4)))
			_ATTRIB(10, vk::Format::eR32G32B32A32Sfloat, offsetof(Instance, colorMul))
			_ATTRIB(11, vk::Format::eR32Sfloat, offsetof(Instance, rnd))
		#undef _ATTRIB
		return r;
	} ();


	Pipeline::Pipeline(): _rpass(nullptr) { }


	Pipeline::Pipeline(
			RenderPass& rpass,
			const std::string& vtxSpv, const std::string& frgSpv,
			const char* shaderEntryPoint, unsigned subpassIndex,
			bool invertCullFace, vk::Extent2D extent,
			vk::SampleCountFlagBits sampleCount
	): _rpass(&rpass) {
		assert(_rpass->_swapchain != nullptr);
		auto dev = _rpass->_swapchain->application->device();
		{
			_data.vtxShader = mk_shader_module(dev, vtxSpv);  util::alloc_tracker.alloc("Pipeline:_data:vtxShader");
			_data.frgShader = mk_shader_module(dev, frgSpv);  util::alloc_tracker.alloc("Pipeline:_data:frgShader");
		} {
			std::array<vk::PipelineShaderStageCreateInfo, 2> sscInfos = {
				vk::PipelineShaderStageCreateInfo({ },
					vk::ShaderStageFlagBits::eVertex,_data.vtxShader, shaderEntryPoint),
				vk::PipelineShaderStageCreateInfo({ },
					vk::ShaderStageFlagBits::eFragment,_data.frgShader, shaderEntryPoint) };

			vk::PipelineVertexInputStateCreateInfo viscInfo;
			static constexpr auto inputCount = Vertex::ATTRIB_DESC.size() + Instance::ATTRIB_DESC.size();
			std::array<decltype(Vertex::BINDING_DESC), 2> viscBindings = { Vertex::BINDING_DESC, Instance::BINDING_DESC };
			std::array<vk::VertexInputAttributeDescription, inputCount> viscAttribs = { }; {
				size_t i = 0;
				for(const auto& attrib : Vertex::ATTRIB_DESC)  viscAttribs[i++] = attrib;
				for(const auto& attrib : Instance::ATTRIB_DESC)  viscAttribs[i++] = attrib;
				assert(i == viscAttribs.size());
			}
			viscInfo.setVertexBindingDescriptions(viscBindings);
			viscInfo.setVertexAttributeDescriptions(viscAttribs);

			vk::PipelineInputAssemblyStateCreateInfo iascInfo;
			iascInfo.topology = vk::PrimitiveTopology::eTriangleList;

			vk::PipelineViewportStateCreateInfo vscInfo;
			vk::Rect2D scissor = vk::Rect2D({ 0, 0 }, extent);
			vk::Viewport viewport = vk::Viewport(0.0f, 0.0f,
				static_cast<float>(extent.width),
				static_cast<float>(extent.height), 0.0f, 1.0f);
			vscInfo.setScissors(scissor);
			vscInfo.setViewports(viewport);

			vk::PipelineRasterizationStateCreateInfo rscInfo;
			rscInfo.setCullMode(invertCullFace?
				vk::CullModeFlagBits::eFront : vk::CullModeFlagBits::eBack);
			rscInfo.frontFace = vk::FrontFace::eCounterClockwise;
			rscInfo.lineWidth = LINE_WIDTH;
			rscInfo.polygonMode = vk::PolygonMode::eFill;

			vk::PipelineMultisampleStateCreateInfo msscInfo;
			msscInfo.sampleShadingEnable = true;
			msscInfo.rasterizationSamples = sampleCount;
			msscInfo.minSampleShading = 1.0f;

			vk::PipelineDepthStencilStateCreateInfo dsscInfo;
			dsscInfo.depthTestEnable = true;
			dsscInfo.depthWriteEnable = true;
			dsscInfo.depthCompareOp = DEPTH_CMP_OP;

			vk::PipelineColorBlendStateCreateInfo cbscInfo;
			vk::PipelineColorBlendAttachmentState cbaState;
			cbaState.colorWriteMask =
				vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
				vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
			cbscInfo.setAttachments(cbaState);

			auto dStates = std::array<vk::DynamicState, 1> {
				vk::DynamicState::eLineWidth };
			vk::PipelineDynamicStateCreateInfo dscInfo;
			dscInfo.setDynamicStates(dStates);

			{
				vk::GraphicsPipelineCreateInfo gpcInfo;
				gpcInfo.pStages = sscInfos.data();  gpcInfo.stageCount = 2;
				gpcInfo.layout = _rpass->_data.pipelineLayout;
				gpcInfo.pVertexInputState = &viscInfo;
				gpcInfo.pInputAssemblyState = &iascInfo;
				gpcInfo.pViewportState = &vscInfo;
				gpcInfo.pRasterizationState = &rscInfo;
				gpcInfo.pMultisampleState = &msscInfo;
				gpcInfo.pDepthStencilState = &dsscInfo;
				gpcInfo.pColorBlendState = &cbscInfo;
				gpcInfo.pDynamicState = &dscInfo;
				gpcInfo.renderPass = _rpass->_data.handle;
				gpcInfo.subpass = subpassIndex;
				auto r = dev.createGraphicsPipelines(nullptr, gpcInfo);
				if(r.result != vk::Result::eSuccess) {
					throw std::runtime_error(formatVkErrorMsg(
						"failed to create a Vulkan pipeline", vk::to_string(r.result)));
				}
				_data.handle = r.value.front();  util::alloc_tracker.alloc("Pipeline:_data:handle");
			}
		}
	}


	void Pipeline::destroy() {
		assert(_rpass != nullptr);
		auto dev = _rpass->_swapchain->application->device();
		dev.waitIdle();
		dev.destroyPipeline(_data.handle);  util::alloc_tracker.dealloc("Pipeline:_data:handle");
		dev.destroyShaderModule(_data.vtxShader);  util::alloc_tracker.dealloc("Pipeline:_data:vtxShader");
		dev.destroyShaderModule(_data.frgShader);  util::alloc_tracker.dealloc("Pipeline:_data:frgShader");
		#ifndef NDEBUG
			_rpass = nullptr;
		#endif
	}

}