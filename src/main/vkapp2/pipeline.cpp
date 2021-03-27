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
		r[0].  binding = 0;  r[0].location = 0;
		r[0]  .format = vk::Format::eR32G32B32Sfloat;
		r[0]  .offset = offsetof(Vertex, pos);
		r[1].  binding = 0;  r[1].location = 1;
		r[1]  .format = vk::Format::eR32G32B32Sfloat;
		r[1]  .offset = offsetof(Vertex, nrm);
		r[2].  binding = 0;  r[2].location = 2;
		r[2]  .format = vk::Format::eR32G32B32Sfloat;
		r[2]  .offset = offsetof(Vertex, nrm_smooth);
		r[3].  binding = 0;  r[3].location = 3;
		r[3]  .format = vk::Format::eR32G32B32Sfloat;
		r[3]  .offset = offsetof(Vertex, tanu);
		r[4].  binding = 0;  r[4].location = 4;
		r[4]  .format = vk::Format::eR32G32B32Sfloat;
		r[4]  .offset = offsetof(Vertex, tanv);
		r[5].  binding = 0;  r[5].location = 5;
		r[5]  .format = vk::Format::eR32G32Sfloat;
		r[5]  .offset = offsetof(Vertex, tex);
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
			viscInfo.setVertexAttributeDescriptions(Vertex::ATTRIB_DESC);
			viscInfo.setVertexBindingDescriptions(Vertex::BINDING_DESC);

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