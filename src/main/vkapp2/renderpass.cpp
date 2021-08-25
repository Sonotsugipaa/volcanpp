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

#include "vkapp2/draw.hpp"
#include "vkapp2/constants.hpp"

#include <vma/vk_mem_alloc.h>

using namespace vka2;



namespace {

	const DescSetBindings descsetBindings = []() {
		constexpr unsigned bindingCount = 3;
		constexpr auto uboStages =
			vk::ShaderStageFlagBits::eVertex |
			vk::ShaderStageFlagBits::eFragment;
		std::array<std::vector<vk::DescriptorSetLayoutBinding>, bindingCount> r;
		static_assert((ubo::Static::set < bindingCount) && (ubo::Static::binding < bindingCount));
		static_assert((ubo::Model::set < bindingCount) && (ubo::Model::binding < bindingCount));
		static_assert((ubo::Frame::set < bindingCount) && (ubo::Frame::binding < bindingCount));
		static_assert(
			(Texture::samplerDescriptorBindings[0] < bindingCount) &&
			(Texture::samplerDescriptorBindings[1] < bindingCount));
		// Ordered by update frequency, ideally in ascending order
		r[ubo::Static::set] = {
			vk::DescriptorSetLayoutBinding(ubo::Static::binding,
				vk::DescriptorType::eUniformBuffer, 1, uboStages) };
		r[ubo::Model::set] = {
			vk::DescriptorSetLayoutBinding(ubo::Model::binding,
				vk::DescriptorType::eUniformBuffer, 1, uboStages),
			vk::DescriptorSetLayoutBinding(Texture::samplerDescriptorBindings[0], // Color sampler
				vk::DescriptorType::eCombinedImageSampler, 1,
				vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(Texture::samplerDescriptorBindings[1], // Normal sampler
				vk::DescriptorType::eCombinedImageSampler, 1,
				vk::ShaderStageFlagBits::eFragment) };
		r[ubo::Frame::set] = {
			vk::DescriptorSetLayoutBinding(ubo::Frame::binding,
				vk::DescriptorType::eUniformBuffer, 1, uboStages) };
		return r;
	} ();


	std::vector<vk::DescriptorSetLayout> mk_descset_layouts(vk::Device dev) {
		vk::DescriptorSetLayoutCreateInfo dslcInfo;
		std::vector<vk::DescriptorSetLayout> r;  r.reserve(descsetBindings.size());
		for(unsigned i=0; auto& bindings : descsetBindings) {
			dslcInfo.setBindings(bindings);
			r.emplace_back(dev.createDescriptorSetLayout(dslcInfo));
			util::logVkDebug()
				<< "Created d. set layout " << (i++) << " with "
				<< bindings.size() << (bindings.size() == 1? " binding" : " bindings")
				<< util::endl;
		}
		return r;
	}


	vk::PipelineLayout mk_pipeline_layout(
			vk::Device dev,
			std::vector<vk::DescriptorSetLayout>& dsLayouts
	) {
		vk::PipelineLayoutCreateInfo plcInfo = { };
		vk::PushConstantRange pcRange;
		if constexpr(push_const::Object::unused) {
			plcInfo.setSetLayouts(dsLayouts);
			plcInfo.setPushConstantRangeCount(0);
		} else {
			pcRange.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
			pcRange.offset = 0;
			pcRange.size = sizeof(push_const::Object);
			plcInfo.setSetLayouts(dsLayouts);
			plcInfo.setPushConstantRangeCount(1);
			plcInfo.setPPushConstantRanges(&pcRange);
			assert(pcRange.size < MAX_PUSH_CONST_BYTES);
		}
		return dev.createPipelineLayout(plcInfo);
	}


	ImageAlloc mk_depthstencil_img(
			AbstractSwapchain& asc,
			const Runtime& runtime, const vk::Extent2D& ext
	) {
		vk::ImageCreateInfo icInfo;
		assert(icInfo.initialLayout == vk::ImageLayout::eUndefined);
		icInfo.imageType = vk::ImageType::e2D;
		icInfo.extent = vk::Extent3D(ext, 1);
		icInfo.mipLevels = 1;
		icInfo.samples = runtime.bestSampleCount;
		icInfo.format = runtime.depthOptimalFmt;
		icInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
		icInfo.tiling = IMAGE_TILING;
		icInfo.arrayLayers = 1;
		icInfo.sharingMode = vk::SharingMode::eExclusive;
		icInfo.initialLayout = vk::ImageLayout::eUndefined;
		return asc.application->createImage(icInfo,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
	}


	vk::ImageView mk_depthstencil_img_view(
			AbstractSwapchain& asc, vk::Image img,
			bool useStencil
	) {
		vk::ImageViewCreateInfo ivcInfo;
		vk::ImageSubresourceRange subresRange;
		subresRange.aspectMask = useStencil?
			(vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil) :
			(vk::ImageAspectFlagBits::eDepth);
		subresRange.layerCount = 1;
		subresRange.levelCount = 1;
		ivcInfo.format = asc.application->runtime().depthOptimalFmt;
		ivcInfo.image = img;
		ivcInfo.subresourceRange = subresRange;
		ivcInfo.viewType = vk::ImageViewType::e2D;
		return asc.application->device().createImageView(ivcInfo);
	}


	vk::RenderPass mk_render_pass(
			AbstractSwapchain& asc,
			vk::Format colorFormat, vk::Format depthFormat,
			vk::ImageLayout colorLayout,
			vk::SampleCountFlagBits sampleCount,
			bool useStencil
	) {
		bool useMultisampling = sampleCount != vk::SampleCountFlagBits::e1;
		vk::ImageLayout depthLayout = useStencil?
			vk::ImageLayout::eDepthAttachmentOptimal :
			vk::ImageLayout::eDepthStencilAttachmentOptimal;
		std::vector<vk::AttachmentDescription> attachments;  attachments.reserve(3);
		std::vector<vk::AttachmentReference> attachmentRefs;  attachmentRefs.reserve(3);
		vk::RenderPassCreateInfo rpcInfo;
		std::array<vk::SubpassDescription, 2> subpassDesc;
		vk::SubpassDependency subpassDep;
		attachmentRefs.push_back(vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal));
		attachments.push_back(vk::AttachmentDescription({ },
			colorFormat, sampleCount,
			vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
			vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined, colorLayout));
		attachmentRefs.push_back(vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal));
		attachments.push_back(vk::AttachmentDescription({ },
			depthFormat, sampleCount,
			vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
			vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined, depthLayout));
		if(useMultisampling) {
			attachmentRefs.push_back(vk::AttachmentReference(2, vk::ImageLayout::eColorAttachmentOptimal));
			attachments.push_back(vk::AttachmentDescription({ },
				colorFormat, vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined, colorLayout));
		}
		subpassDesc[0].setColorAttachments(attachmentRefs[0]);
		subpassDesc[1].setColorAttachments(attachmentRefs[0]);
		subpassDesc[0].setPDepthStencilAttachment(&attachmentRefs[1]);
		subpassDesc[1].setPDepthStencilAttachment(&attachmentRefs[1]);
		subpassDesc[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpassDesc[1].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		if(useMultisampling) {
			subpassDesc[0].setPResolveAttachments(&attachmentRefs[2]);
			subpassDesc[1].setPResolveAttachments(&attachmentRefs[2]);
		}
		subpassDep.srcSubpass = 0;
		subpassDep.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		subpassDep.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
		subpassDep.dstSubpass = 1;
		subpassDep.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		subpassDep.dstAccessMask = vk::AccessFlagBits::eNoneKHR;
		rpcInfo.setAttachments(attachments);
		rpcInfo.setSubpasses(subpassDesc);
		rpcInfo.setDependencies(subpassDep);
		return asc.application->device().createRenderPass(rpcInfo);
	}


	vk::ResultValue<uint32_t> tryAcquireSwpchnImage(
			vk::Device dev, vk::SwapchainKHR swpchn,
			vk::Semaphore sem, vk::Fence fence
	) {
		auto acquired = dev.acquireNextImageKHR(swpchn, UINT64_MAX, sem, fence);
		if(acquired.result != vk::Result::eSuccess) {
			if(
					(acquired.result != vk::Result::eSuboptimalKHR) &&
					(acquired.result != vk::Result::eErrorOutOfDateKHR)
			) {
				throw std::runtime_error(formatVkErrorMsg(
					"failed to acquire a swapchain image", vk::to_string(acquired.result)));
			}
		}
		return acquired;
	}


	BufferAlloc mk_static_ubo_base(
			VmaAllocator alloc
	) {
		BufferAlloc r;
		VkBuffer cHandle;
		vk::BufferCreateInfo bcInfo;
		VmaAllocationCreateInfo acInfo = { };
		bcInfo.size = sizeof(ubo::Static);
		bcInfo.sharingMode = vk::SharingMode::eExclusive;
		bcInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer |
			vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
		if constexpr(ubo::Static::dma) {
			acInfo.requiredFlags =
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			acInfo.preferredFlags =
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		} else {
			acInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			bcInfo.sharingMode = vk::SharingMode::eExclusive;
		}
		VkBufferCreateInfo& cCrInfo = bcInfo;
		{
			auto result = vmaCreateBuffer(alloc,
				&cCrInfo, &acInfo, &cHandle, &r.alloc, nullptr);
			if(result != VK_SUCCESS) {
				throw std::runtime_error(formatVkErrorMsg(
					"failed to create a static UBO on the device", util::enum_str(result)));
			}
		}
		r.handle = cHandle;
		return r;
	}


	vk::DescriptorPool mk_desc_pool(
			vk::Device dev, unsigned swpChnImgCount
	) {
		auto sizes = std::array<vk::DescriptorPoolSize, 2> {
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 3 * swpChnImgCount),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 4 * ESTIMATED_MAX_MODEL_COUNT)
		};
		vk::DescriptorPoolCreateInfo dpcInfo;
		dpcInfo.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
		dpcInfo.setPoolSizes(sizes);
		dpcInfo.maxSets = swpChnImgCount * (
			2 // Static UBO + frame UBO
			+ ESTIMATED_MAX_MODEL_COUNT
		);
		util::logVkDebug()
			<< "Creating descriptor pool with max." << dpcInfo.maxSets
			<< " descriptor sets for max."
			<< sizes[0].descriptorCount << '+'
			<< sizes[1].descriptorCount << " bindings" << util::endl;
		return dev.createDescriptorPool(dpcInfo);
	}


	ImageAlloc mk_render_target_img(AbstractSwapchain& asc, vk::Extent2D extent) {
		vk::ImageCreateInfo icInfo;
		icInfo.arrayLayers = 1;
		icInfo.extent = vk::Extent3D(extent.width, extent.height, 1);
		icInfo.format = asc.application->surfaceFormat().format;
		icInfo.imageType = vk::ImageType::e2D;
		icInfo.initialLayout = vk::ImageLayout::eUndefined;
		icInfo.mipLevels = 1;
		icInfo.samples = asc.application->runtime().bestSampleCount;
		icInfo.sharingMode = vk::SharingMode::eExclusive;
		icInfo.tiling = vk::ImageTiling::eOptimal;
		icInfo.usage =
			vk::ImageUsageFlagBits::eTransferSrc |
			vk::ImageUsageFlagBits::eColorAttachment;
		return asc.application->createImage(icInfo,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
	}

	ImageAlloc mk_resolve_target_img(AbstractSwapchain& asc, vk::Extent2D extent) {
		vk::ImageCreateInfo icInfo;
		icInfo.arrayLayers = 1;
		icInfo.extent = vk::Extent3D(extent.width, extent.height, 1);
		icInfo.format = asc.application->surfaceFormat().format;
		icInfo.imageType = vk::ImageType::e2D;
		icInfo.initialLayout = vk::ImageLayout::eUndefined;
		icInfo.mipLevels = 1;
		icInfo.samples = vk::SampleCountFlagBits::e1;
		icInfo.sharingMode = vk::SharingMode::eExclusive;
		icInfo.tiling = vk::ImageTiling::eOptimal;
		icInfo.usage =
			vk::ImageUsageFlagBits::eTransferSrc |
			vk::ImageUsageFlagBits::eColorAttachment;
		return asc.application->createImage(icInfo,
			vk::MemoryPropertyFlagBits::eDeviceLocal);
	}



	namespace frame {

		std::vector<RenderPass::FrameData> mk_frames(
				vk::Device dev, unsigned count
		) {
			auto r = std::vector<RenderPass::FrameData>(count);
			for(unsigned i=0; i < count; ++i) {
				r[i].imgAcquiredSem = dev.createSemaphore({ });  util::alloc_tracker.alloc("RenderPass:FrameData:imgAcquiredSem");
				r[i].renderDoneSem = dev.createSemaphore({ });  util::alloc_tracker.alloc("RenderPass:FrameData:renderDoneSem");
				r[i].blitToSurfaceDoneSem = dev.createSemaphore({ });  util::alloc_tracker.alloc("RenderPass:FrameData:renderToSurfaceSem");
			}
			return r;
		}


		void destroy_frames(RenderPass& rpass, std::vector<RenderPass::FrameData> frames) {
			auto dev = rpass.swapchain()->application->device();
			for(auto frame : frames) {
				dev.destroySemaphore(frame.blitToSurfaceDoneSem);  util::alloc_tracker.dealloc("RenderPass:FrameData:renderToSurfaceSem");
				dev.destroySemaphore(frame.renderDoneSem);  util::alloc_tracker.dealloc("RenderPass:FrameData:renderDoneSem");
				dev.destroySemaphore(frame.imgAcquiredSem);  util::alloc_tracker.dealloc("RenderPass:FrameData:imgAcquiredSem");
			}
		}

	}



	namespace imgref {

		void update_static_ubo(
				AbstractSwapchain& asc, RenderPass::ImageData& imgData,
				BufferAlloc& staticUboBase,
				decltype(RenderPass::ImageData::staticUboWrCounter) staticUboWrCounter
		) {
			imgData.staticUboWrCounter = staticUboWrCounter;
			asc.application->device().resetFences(imgData.fenceStaticUboUpToDate);
			asc.application->transferCommandPool().runCmds(
				asc.application->queues().transfer,
				[&imgData, &staticUboBase](vk::CommandBuffer cmd) {
					constexpr auto cp = vk::BufferCopy(0, 0, sizeof(ubo::Static));
					cmd.copyBuffer(staticUboBase.handle, imgData.staticUbo.handle, cp);
				}, imgData.fenceStaticUboUpToDate
			);
			vk::Result result = asc.application->device().waitForFences(
				imgData.fenceStaticUboUpToDate, true, UINT64_MAX);
			if(result != vk::Result::eSuccess) {
					throw std::runtime_error(formatVkErrorMsg(
						"failed to wait on a fence while running a render pass",
						vk::to_string(result)));
				}
		}


		RenderPass::ImageData mk_data(
				AbstractSwapchain& asc,
				vk::RenderPass rpass, vk::DescriptorPool dsPool,
				std::vector<vk::DescriptorSetLayout> dsLayouts,
				vk::Extent2D renderExtent, vk::ImageView depthStencilImgView,
				unsigned graphicsQueueFamily, bool useMultisampling
		) {
			constexpr auto allocUbo = [](
					AbstractSwapchain& asc, vk::DeviceSize size, bool dma
			) -> BufferAlloc {
				vk::BufferCreateInfo bcInfo;
				vk::MemoryPropertyFlags reqMemProps;
				vk::MemoryPropertyFlags prfMemProps = vk::MemoryPropertyFlags(0);
				bcInfo.size = size;
				bcInfo.sharingMode = vk::SharingMode::eExclusive;
				bcInfo.usage =
					vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
				bcInfo.sharingMode = vk::SharingMode::eExclusive;
				if(dma) {
					reqMemProps =
						vk::MemoryPropertyFlagBits::eHostVisible |
						vk::MemoryPropertyFlagBits::eHostCoherent;
					prfMemProps = vk::MemoryPropertyFlagBits::eDeviceLocal;
				} else {
					reqMemProps = vk::MemoryPropertyFlagBits::eDeviceLocal;
				}
				return asc.application->createBuffer(bcInfo, reqMemProps, prfMemProps);
			};
			auto dev = asc.application->device();
			RenderPass::ImageData r;
			{ // Create the UBOs
				r.staticUbo = allocUbo(asc, sizeof(ubo::Static), ubo::Static::dma);
				r.frameUbo = allocUbo(asc, sizeof(ubo::Frame), ubo::Frame::dma);
				r.staticUboWrCounter = 0; // Set the state tracker to the initial state: the static UBO copied *always* has to be "updated" here
			} { // Create the render target
				r.renderTarget = mk_render_target_img(asc, renderExtent);  util::alloc_tracker.alloc("RenderPass:ImageData:renderTarget");
				vk::ImageViewCreateInfo ivcInfo;
				vk::ImageSubresourceRange subresRange;
				subresRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				subresRange.layerCount = subresRange.levelCount = 1;
				ivcInfo.components = vk::ComponentMapping(vk::ComponentSwizzle::eIdentity);
				ivcInfo.format = asc.application->surfaceFormat().format;
				ivcInfo.image = r.renderTarget.handle;
				ivcInfo.subresourceRange = subresRange;
				ivcInfo.viewType = vk::ImageViewType::e2D;
				r.renderTargetView = asc.application->device().createImageView(ivcInfo);  util::alloc_tracker.alloc("RenderPass:ImageData:renderTargetView");
				if(useMultisampling) {
					// Optionally create the resolve attachment
					r.resolveTarget = mk_resolve_target_img(asc, renderExtent);  util::alloc_tracker.alloc("RenderPass:ImageData:resolveTarget");
					ivcInfo.image = r.resolveTarget.handle;
					subresRange.aspectMask = vk::ImageAspectFlagBits::eColor;
					r.resolveTargetView = asc.application->device().createImageView(ivcInfo);  util::alloc_tracker.alloc("RenderPass:ImageData:resolveTargetView");
				} else { // If the resolve attachment is not to be created, initialize it so we know not to destroy it
					r.resolveTarget = ImageAlloc { nullptr, nullptr };
					r.resolveTargetView = nullptr;
				}
			} { // Create the framebuffer
				vk::FramebufferCreateInfo fbcInfo;
				std::vector<vk::ImageView> attachmentViews = {
					r.renderTargetView, depthStencilImgView };
				if(r.resolveTargetView != vk::ImageView(nullptr)) {
					attachmentViews.push_back(r.resolveTargetView); }
				fbcInfo.layers = 1;
				fbcInfo.renderPass = rpass;
				fbcInfo.width = renderExtent.width;
				fbcInfo.height = renderExtent.height;
				fbcInfo.setAttachments(attachmentViews);
				r.framebuffer = asc.application->device().createFramebuffer(fbcInfo);  util::alloc_tracker.alloc("RenderPass:ImageData:framebuffer");
			} { // Create the image's synchronization objects
				r.fenceStaticUboUpToDate = dev.createFence({ });
				r.fenceImgAvailable = dev.createFence({ vk::FenceCreateFlagBits::eSignaled });
				util::alloc_tracker.alloc("RenderPass:ImageData:[sync_objects]");
			} { // Allocate the command pool and buffer
				r.cmdPool = dev.createCommandPool(vk::CommandPoolCreateInfo({ },
					graphicsQueueFamily));  util::alloc_tracker.alloc("RenderPass:ImageData:cmdPool");
				auto cmdBufferVector = dev.allocateCommandBuffers(
					vk::CommandBufferAllocateInfo(r.cmdPool, vk::CommandBufferLevel::ePrimary, 2));
				assert(cmdBufferVector.size() == r.cmdBuffer.size());
				std::move(cmdBufferVector.begin(), cmdBufferVector.end(), r.cmdBuffer.begin());
				cmdBufferVector = dev.allocateCommandBuffers(
					vk::CommandBufferAllocateInfo(r.cmdPool, vk::CommandBufferLevel::eSecondary, 2));
				assert(cmdBufferVector.size() == r.secondaryDrawBuffers.size());
				std::move(cmdBufferVector.begin(), cmdBufferVector.end(), r.secondaryDrawBuffers.begin());
			} { // Create the descriptor sets
				auto mkDescSet = [dsPool, dev](
						vk::DescriptorSetLayout layout
				) {
					vk::DescriptorSetAllocateInfo dsaInfo;
					dsaInfo.descriptorPool = dsPool;
					dsaInfo.descriptorSetCount = 1;
					dsaInfo.setSetLayouts(layout);
					return dev.allocateDescriptorSets(dsaInfo).front();
				};
				r.staticDescSet = mkDescSet(dsLayouts[ubo::Static::set]);
				r.frameDescSet = mkDescSet(dsLayouts[ubo::Frame::set]);
			}
			return r;
		}


		void destroy_data(
				AbstractSwapchain& asc,
				RenderPass::ImageData& imgData
		) {
			auto dev = asc.application->device();
			dev.destroyImageView(imgData.renderTargetView);  util::alloc_tracker.dealloc("RenderPass:ImageData:renderTargetView");
			asc.application->destroyImage(imgData.renderTarget);  util::alloc_tracker.dealloc("RenderPass:ImageData:renderTarget");
			if(imgData.resolveTargetView != vk::ImageView(nullptr)) {
				dev.destroyImageView(imgData.resolveTargetView);  util::alloc_tracker.dealloc("RenderPass:ImageData:resolveTargetView");
				asc.application->destroyImage(imgData.resolveTarget);  util::alloc_tracker.dealloc("RenderPass:ImageData:resolveTarget");
			} else {
				assert(imgData.resolveTarget.handle == vk::Image(nullptr));
				assert(imgData.resolveTarget.alloc == nullptr);
			}
			{
				dev.destroyFence(imgData.fenceImgAvailable);
				dev.destroyFence(imgData.fenceStaticUboUpToDate);
				util::alloc_tracker.dealloc("RenderPass:ImageData:[sync_objects]");
			}
			dev.destroyCommandPool(imgData.cmdPool);  util::alloc_tracker.dealloc("RenderPass:ImageData:cmdPool");
			dev.destroyFramebuffer(imgData.framebuffer);  util::alloc_tracker.dealloc("RenderPass:ImageData:framebuffer");
			asc.application->destroyBuffer(imgData.frameUbo);
			asc.application->destroyBuffer(imgData.staticUbo);
		}

	}


	void set_mdl_ubo_descriptor(
			vk::Device dev,
			const Model& mdl, vk::DescriptorSet dSet
	) {
		vk::WriteDescriptorSet wdSet;
		vk::DescriptorBufferInfo dbInfo;
		dbInfo.buffer = mdl.uboBuffer().handle;
		dbInfo.offset = 0;
		dbInfo.range = sizeof(ubo::Model);
		wdSet.descriptorCount = 1;
		wdSet.descriptorType = vk::DescriptorType::eUniformBuffer;
		wdSet.dstBinding = ubo::Model::binding;
		wdSet.dstSet = dSet;
		wdSet.pBufferInfo = &dbInfo;
		dev.updateDescriptorSets(wdSet, { });
	}


	void set_mdl_sampler_descriptor(
			vk::Device dev,
			vk::DescriptorSet dSet,
			unsigned bindingIndex, // Relative to Texture::samplerDescriptorBindings
			const Texture& tex
	) {
		vk::WriteDescriptorSet wdSet;
		vk::DescriptorImageInfo diInfo;
		assert(bindingIndex < Texture::samplerDescriptorBindings.size());
		diInfo.imageView = tex.imgView();
		diInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		diInfo.sampler = tex.sampler();
		wdSet.descriptorCount = 1;
		wdSet.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		wdSet.dstBinding = Texture::samplerDescriptorBindings[bindingIndex];
		wdSet.dstSet = dSet;
		wdSet.pImageInfo = &diInfo;
		dev.updateDescriptorSets(wdSet, { });
	}


	void record_render_cmds(
			RenderPass& rPass,
			RenderPass::PreRenderFunction& preRender,
			RenderPass::PostRenderFunction& postRender,
			std::array<RenderPass::RenderFunction, 2>& renderFunctions,
			RenderPass::ImageData& img, RenderPass::FrameData& frame,
			vk::CommandBuffer primaryCmd
	) {
		assert(renderFunctions.size() == /* the number of subpasses */ 2);
		vk::CommandBufferInheritanceInfo cbiInfo;
		vk::CommandBufferBeginInfo cbbInfo;
		unsigned subpass = 0;
		unsigned iterations = renderFunctions.size() - 1; // Last iteration does not .nextSubpass(...)
		RenderPass::FrameHandle fh = { rPass, frame, img };
		cbiInfo.setRenderPass(rPass.handle());
		cbiInfo.setFramebuffer(img.framebuffer);
		cbiInfo.setOcclusionQueryEnable(false);
		cbbInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eRenderPassContinue);
		cbbInfo.setPInheritanceInfo(&cbiInfo);
		const auto runSubpass = [
				primaryCmd, img,
				&rPass, &cbiInfo, &cbbInfo, &frame, &fh
		] (unsigned subpass, RenderPass::RenderFunction& fn) {
			const auto& secBuffer = img.secondaryDrawBuffers[subpass];
			cbiInfo.setSubpass(subpass);
			secBuffer.begin(cbbInfo);
			secBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				rPass.pipelineLayout(), ubo::Frame::set,
				img.frameDescSet,
				{ });
			secBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				rPass.pipelineLayout(), ubo::Static::set,
				img.staticDescSet,
				{ });
			fn(fh, secBuffer);
			secBuffer.end();
			primaryCmd.executeCommands(secBuffer);
		};
		if(preRender)  preRender(fh);
		for(unsigned i=0; i < iterations; ++i) {
			runSubpass(subpass, renderFunctions[i]);
			primaryCmd.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
			++subpass;
		}
		runSubpass(subpass, renderFunctions.back());
		if(postRender)  postRender(fh);
	}

}



namespace vka2 {

	void RenderPass::FrameHandle::updateModelDescriptors(
			const Model& mdl, vk::DescriptorSet dSet
	) {
		auto dev = rpass._swapchain->application->device();
		set_mdl_ubo_descriptor(dev, mdl, dSet);
		set_mdl_sampler_descriptor(
			dev, dSet, Texture::samplerDescriptorBindings[0], mdl.material().colorTexture);
		set_mdl_sampler_descriptor(
			dev, dSet, Texture::samplerDescriptorBindings[1], mdl.material().normalTexture);
	}


	void RenderPass::FrameHandle::bindModelDescriptorSet(
			vk::CommandBuffer cmd, vk::DescriptorSet dSet
	) {
		static_assert(ubo::Model::set == Texture::samplerDescriptorSet);
		constexpr auto descSetIdx = ubo::Model::set;
		assert(cmd != vk::CommandBuffer());
		assert(dSet != vk::DescriptorSet());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			rpass.pipelineLayout(), descSetIdx, dSet, { });
	}


	void RenderPass::_assign(AbstractSwapchain& asc) {
		auto dev = asc.application->device();
		auto& imgs = asc.data.images;
		++_data.staticUboBaseWrCounter; // Force the static UBO copies to be invalid (this counter should never be 0)
		_swapchain = &asc;
		dev.waitIdle();
		{ // Create depth/stencil image
			_data.depthStencilImg = mk_depthstencil_img(*_swapchain,
				_swapchain->application->runtime(), _data.renderExtent);  util::alloc_tracker.alloc("RenderPass:_data:depthStencilImg");
			_data.depthStencilImgView = mk_depthstencil_img_view(
				*_swapchain, _data.depthStencilImg.handle, false);  util::alloc_tracker.alloc("RenderPass:_data:depthStencilImgView");
		}
		_data.swpchnImages.reserve(imgs.size());
		_data.handle = mk_render_pass(asc,
			asc.application->surfaceFormat().format,
			asc.application->runtime().depthOptimalFmt, vk::ImageLayout::eTransferSrcOptimal,
			asc.application->runtime().bestSampleCount, false);  util::alloc_tracker.alloc("RenderPass:_data:handle");
		_data.descPool = mk_desc_pool(dev, imgs.size());  util::alloc_tracker.alloc("RenderPass:_data:descPool");
		for(auto img : imgs) {
			_data.swpchnImages.emplace_back(img, imgref::mk_data(
				asc, _data.handle, _data.descPool, _data.descsetLayouts,
				_data.renderExtent, _data.depthStencilImgView,
				asc.application->queueFamilyIndices().graphics,
				_data.useMultisampling));
		}  util::alloc_tracker.alloc("RenderPass:_data:swpchnImages[...]", imgs.size());
	}


	void RenderPass::_unassign() {
		auto dev = _swapchain->application->device();
		dev.waitIdle();
		dev.destroyDescriptorPool(_data.descPool);  util::alloc_tracker.dealloc("RenderPass:_data:descPool");
		_swapchain->application->device().destroyRenderPass(_data.handle);  util::alloc_tracker.dealloc("RenderPass:_data:handle");
		for(auto& img : _data.swpchnImages) {
			imgref::destroy_data(*_swapchain, img.second); }  util::alloc_tracker.dealloc("RenderPass:_data:swpchnImages[...]", _data.swpchnImages.size());
		_data.swpchnImages.clear();
		dev.destroyImageView(_data.depthStencilImgView);  util::alloc_tracker.dealloc("RenderPass:_data:depthStencilImgView");
		vmaDestroyImage(_swapchain->application->allocator(),
			_data.depthStencilImg.handle, _data.depthStencilImg.alloc);  util::alloc_tracker.dealloc("RenderPass:_data:depthStencilImg");
	}


	RenderPass::RenderPass():
			_swapchain(nullptr)
	{ }


	RenderPass::RenderPass(
			AbstractSwapchain& asc,
			vk::Extent2D renderExtent,
			unsigned short maxConcurrentFrames,
			bool useMultisampling,
			SwapchainOutdatedCallback onOod
	) {
		auto dev = asc.application->device();
		_rendering = { };
		_data.useMultisampling = useMultisampling;
		_data.renderExtent = renderExtent;
		swapchainOutdatedCallback = onOod;
		_data.descsetLayouts = mk_descset_layouts(asc.application->device());  util::alloc_tracker.alloc("RenderPass:_data:descsetLayouts", _data.descsetLayouts.size());
		_data.pipelineLayout = mk_pipeline_layout(asc.application->device(),
			_data.descsetLayouts);  util::alloc_tracker.alloc("RenderPass:_data:pipelineLayout");
		_data.staticUboBaseWrCounter = 0; // Initial state, *must* be 0
		_data.staticUboBase = mk_static_ubo_base(asc.application->allocator());  util::alloc_tracker.alloc("RenderPass:_data:staticUboBase");
		_data.frames = frame::mk_frames(dev, maxConcurrentFrames);
		_assign(asc);
		util::alloc_tracker.alloc("RenderPass");
	}


	void RenderPass::destroy() {
		vk::Device dev = _swapchain->application->device();
		dev.waitIdle();
		_unassign();
		frame::destroy_frames(*this, _data.frames);
		vmaDestroyBuffer(_swapchain->application->allocator(),
			_data.staticUboBase.handle, _data.staticUboBase.alloc);  util::alloc_tracker.dealloc("RenderPass:_data:staticUboBase");
		dev.destroyPipelineLayout(_data.pipelineLayout);  util::alloc_tracker.dealloc("RenderPass:_data:pipelineLayout");
		for(auto& layout : _data.descsetLayouts) {
			dev.destroyDescriptorSetLayout(layout);
		}  util::alloc_tracker.dealloc("RenderPass:_data:descsetLayouts", _data.descsetLayouts.size());
		util::alloc_tracker.dealloc("RenderPass");
	}


	void RenderPass::reassign(AbstractSwapchain& asc) {
		assert(_swapchain != nullptr);
		waitIdle();
		_unassign();
		_assign(asc);
	}

	void RenderPass::reassign(AbstractSwapchain& asc, const vk::Extent2D& rExtent) {
		assert(_swapchain != nullptr);
		waitIdle();
		_unassign();
		_data.renderExtent = rExtent;
		_assign(asc);
	}


	void RenderPass::waitIdle(uint64_t timeout) {
		_rendering.skipNextFrame = true;
		auto fenceVector = std::vector<vk::Fence>(_data.swpchnImages.size());
		for(unsigned i=0; i < fenceVector.size(); ++i) {
			fenceVector[i] = _data.swpchnImages[i].second.fenceImgAvailable; }
		{
			vk::Result result = _swapchain->application->device().waitForFences(
				fenceVector, true, timeout);
			if(result != vk::Result::eSuccess) {
				throw std::runtime_error(formatVkErrorMsg(
					"failed to wait for a pipeline to be free", vk::to_string(result)));
			}
		}
	}


	void RenderPass::setStaticUbo(const ubo::Static& ubo) {
		assert((_swapchain != nullptr) && (_swapchain->application != nullptr));
		auto& app = *_swapchain->application;
		++_data.staticUboBaseWrCounter;
		app.stageBufferData(
			_data.staticUboBase.handle,
			reinterpret_cast<const void*>(&ubo),
			sizeof(ubo::Static));
	}


	bool RenderPass::runRenderPass(
			const ubo::Frame& frameUbo,
			PreRenderFunction preRender,
			PostRenderFunction postRender,
			std::array<RenderFunction, 2> renderFunctions
	) {
		vk::Device dev = _swapchain->application->device();
		unsigned imgIndex; // Swapchain image
		auto& frame = _data.frames[_rendering.frame];
		ImageRef* img;
		const auto onSwpChnOutOfDate = [&, this, dev](unsigned imgIndex) {
			_rendering.skipNextFrame = true;
			util::logVkEvent()
				<< "Swapchain image " << imgIndex << " is out of date" << util::endl;
			dev.waitIdle();
			swapchainOutdatedCallback(*this);
		};
		constexpr auto mk_img_barrier = [](
				vk::Image img, vk::ImageSubresourceRange& subresRange,
				vk::ImageLayout oldLayout, vk::AccessFlagBits oldAccessMask,
				vk::ImageLayout newLayout, vk::AccessFlagBits newAccessMask
		) {
			vk::ImageMemoryBarrier r;
			r.image = img;  r.subresourceRange = subresRange;
			r.srcQueueFamilyIndex = r.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			r.oldLayout = oldLayout;  r.srcAccessMask = oldAccessMask;
			r.newLayout = newLayout;  r.dstAccessMask = newAccessMask;
			return r;
		};
		{ // Acquire an image
			if(_rendering.skipNextFrame) {
				return _rendering.skipNextFrame = false;
			}
			auto acquired = tryAcquireSwpchnImage(dev, _swapchain->handle,
				frame.imgAcquiredSem, nullptr);
			imgIndex = acquired.value;
			if(acquired.result == vk::Result::eErrorOutOfDateKHR) {
				onSwpChnOutOfDate(imgIndex);
				return false;
			}
			img = &_data.swpchnImages[imgIndex];
		} { // Run render pass
			auto renderCmd = img->second.cmdBuffer[0];
			auto blitCmd = img->second.cmdBuffer[1];
			auto colorSubresRange = vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
			{ // Begin recording the cmd buffer
				{ // Wait for the swapchain image to be available first
					tryWaitForFences(dev, img->second.fenceImgAvailable, true, UINT64_MAX);
					dev.resetFences(img->second.fenceImgAvailable);
				} {
					dev.resetCommandPool(img->second.cmdPool);
					renderCmd.begin(vk::CommandBufferBeginInfo(
						vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
				}
			} { // Transition the render target image to a drawable layout
				vk::ImageMemoryBarrier barrier = mk_img_barrier(
					img->first, colorSubresRange,
					vk::ImageLayout::eUndefined, vk::AccessFlagBits::eNoneKHR,
					vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits::eNoneKHR);
				renderCmd.pipelineBarrier(
					vk::PipelineStageFlagBits::eVertexShader,
					vk::PipelineStageFlagBits::eColorAttachmentOutput,
					vk::DependencyFlagBits(0), { }, { }, barrier);
			} { // Begin the render pass
				const auto clearValues = std::array<vk::ClearValue, 2>{
					vk::ClearColorValue(
						_swapchain->application->options().worldParams.clearColor),
					vk::ClearDepthStencilValue(1.0f, 0.0f) };
				vk::RenderPassBeginInfo rpbInfo;
				rpbInfo.renderPass = _data.handle;
				rpbInfo.framebuffer = img->second.framebuffer;
				rpbInfo.setClearValues(clearValues);
				rpbInfo.renderArea = vk::Rect2D({ 0, 0 }, _data.renderExtent);
				renderCmd.beginRenderPass(rpbInfo, vk::SubpassContents::eSecondaryCommandBuffers);
			} { // Update the UBO descriptors
				vk::WriteDescriptorSet wr;
				vk::DescriptorBufferInfo dbInfo;
				wr.pBufferInfo = &dbInfo;
				wr.descriptorCount = 1;
				wr.descriptorType = vk::DescriptorType::eUniformBuffer;
				wr.dstArrayElement = 0;
				{ // Begin to update the image's static UBO, if necessary
					if(_data.staticUboBaseWrCounter != img->second.staticUboWrCounter) {
						imgref::update_static_ubo(*_swapchain,
							img->second, _data.staticUboBase, _data.staticUboBaseWrCounter);
						dbInfo.buffer = img->second.staticUbo.handle;
						dbInfo.range = sizeof(ubo::Static);
						wr.dstBinding = ubo::Static::binding;
						wr.dstSet = img->second.staticDescSet;
						dev.updateDescriptorSets(wr, { });
					}
				} { // Update the frame UBO's descriptor set
					dbInfo.buffer = img->second.frameUbo.handle;
					dbInfo.range = sizeof(ubo::Frame);
					wr.dstBinding = ubo::Frame::binding;
					wr.dstSet = img->second.frameDescSet;
					dev.updateDescriptorSets(wr, { });
				}
			} { // Mmap the frame UBO, wait for fences, then run the passed function
				void* frameUboPtr;
				static_assert(ubo::Frame::dma);
				{
					VkResult result = vmaMapMemory(_swapchain->application->allocator(),
						img->second.frameUbo.alloc, &frameUboPtr);
					if(result != VK_SUCCESS) {
						throw std::runtime_error(formatVkErrorMsg(
							"failed to mmap a frame UBO buffer", util::enum_str(result)));
					}
					memcpy(frameUboPtr, &frameUbo, sizeof(ubo::Frame));  static_assert(std::is_same_v<ubo::Frame, std::remove_const_t<std::remove_reference_t<decltype(frameUbo)>>>);
					vmaUnmapMemory(_swapchain->application->allocator(), img->second.frameUbo.alloc);
				} {
					record_render_cmds(*this,
						preRender, postRender, renderFunctions,
						img->second, frame, renderCmd);
				}
			} { // End the render pass
				renderCmd.endRenderPass();
				renderCmd.end();
			} { // Record the blit-to-present cmd buffer
				blitCmd.begin(vk::CommandBufferBeginInfo(
					vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
				{ // Transition the src and dst image layouts
					std::array<vk::ImageMemoryBarrier, 2> imgBarriers;
					imgBarriers[0] = mk_img_barrier(img->second.renderTarget.handle, colorSubresRange,
						vk::ImageLayout::eUndefined,           vk::AccessFlagBits::eNoneKHR,
						vk::ImageLayout::eTransferSrcOptimal,  vk::AccessFlagBits::eTransferRead);
					imgBarriers[1] = mk_img_barrier(img->first, colorSubresRange,
						vk::ImageLayout::ePresentSrcKHR,       vk::AccessFlagBits::eNoneKHR,
						vk::ImageLayout::eTransferDstOptimal,  vk::AccessFlagBits::eTransferWrite);
					blitCmd.pipelineBarrier(
						vk::PipelineStageFlagBits::eTransfer,
						vk::PipelineStageFlagBits::eTransfer,
						vk::DependencyFlagBits(0), { }, { }, imgBarriers);
				} { // Blit the image
					vk::ImageBlit blit;
					auto& options = _swapchain->application->options();
					blit.srcOffsets[0] = vk::Offset3D { };
					blit.srcOffsets[1] = vk::Offset3D {
						(int32_t) _data.renderExtent.width,
						(int32_t) _data.renderExtent.height, 1 };
					blit.dstOffsets[0] = vk::Offset3D { };
					blit.dstOffsets[1] = vk::Offset3D {
						(int32_t) _swapchain->data.extent.width,
						(int32_t) _swapchain->data.extent.height, 1 };
					blit.srcSubresource = blit.dstSubresource = vk::ImageSubresourceLayers(
						vk::ImageAspectFlagBits::eColor, 0, 0, 1);
					blitCmd.blitImage(
						_data.useMultisampling?
							img->second.resolveTarget.handle :
							img->second.renderTarget.handle,
						vk::ImageLayout::eTransferSrcOptimal,
						img->first, vk::ImageLayout::eTransferDstOptimal,
						blit, options.viewParams.upscaleNearestFilter?
							vk::Filter::eNearest : vk::Filter::eLinear);
				} { // Transition the dst image layout to present
					auto imgBarrier = mk_img_barrier(img->first, colorSubresRange,
						vk::ImageLayout::eTransferDstOptimal,  vk::AccessFlagBits::eTransferWrite,
						vk::ImageLayout::ePresentSrcKHR,       vk::AccessFlagBits::eNoneKHR);
					blitCmd.pipelineBarrier(
						vk::PipelineStageFlagBits::eTransfer,
						vk::PipelineStageFlagBits::eTransfer,
						vk::DependencyFlagBits(0), { }, { }, imgBarrier);
				}
				blitCmd.end();
			} { // Submit the cmd buffers
				std::array<vk::PipelineStageFlags, 1> waitStages =
					{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
				auto graphicsQueue = _swapchain->application->queues().graphics;
				auto sInfo = std::array<vk::SubmitInfo, 2> {
					vk::SubmitInfo(frame.imgAcquiredSem, waitStages, renderCmd, frame.renderDoneSem),
					vk::SubmitInfo(frame.renderDoneSem, waitStages, blitCmd, frame.blitToSurfaceDoneSem) };
				graphicsQueue.submit(sInfo, img->second.fenceImgAvailable);
			} { // Here's a present!
				vk::PresentInfoKHR pInfo = { };
				pInfo.setWaitSemaphores(frame.blitToSurfaceDoneSem);
				pInfo.setSwapchains(_swapchain->handle);
				pInfo.setImageIndices(imgIndex);
				auto result = _swapchain->application->presentQueue().presentKHR(&pInfo);
				if(
						result == vk::Result::eErrorOutOfDateKHR ||
						result == vk::Result::eSuboptimalKHR
				) {
					onSwpChnOutOfDate(imgIndex);
					return false;
				} else
				if(result != vk::Result::eSuccess) {
					throw std::runtime_error(formatVkErrorMsg(
						"failed to present a queue", vk::to_string(result)));
				}
			}
		}
		_rendering.frame = (_rendering.frame + 1) % _data.frames.size();
		return true;
	}

}
