#include "vkapp2/graphics.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <filesystem>

using namespace vka2;

using util::enum_str;



namespace {

	unsigned compute_mip_levels(
			unsigned width, unsigned height,
			unsigned limit = std::numeric_limits<unsigned>::max()
	) {
		return std::min((double) limit,
			std::floor(
				std::log2(std::max(width, height))
			) + 1.0);
	}


	Texture::Data read_img_data(
			const std::string& path
	) {
		using namespace std::string_literals;
		stbi_uc* pixels;
		Texture::Data r;
		{
			std::string absPath = std::filesystem::absolute(path);
			int w, h, ch;
			pixels = stbi_load(absPath.c_str(), &w, &h, &ch, STBI_rgb_alpha);
			if(pixels == nullptr) {
				throw std::runtime_error(
					"failed to load a texture from \""+path+"\" ("+stbi_failure_reason()+")"s);
			}
			r.width = w;
			r.height = h;
			r.channels = ch;
		}
		r.size = r.width * r.height * r.channels;
		r.data = pixels;
		r.dataFormat = vk::Format::eR8G8B8A8Srgb;
		r.mipLevels = compute_mip_levels(r.width, r.height);
		return r;
	}


	constexpr vk::ImageMemoryBarrier mk_img_barrier(
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
	}


	void gen_minmaps(
			vk::CommandBuffer cmd,
			vk::Image img, vk::Extent2D ext,
			unsigned levels, vk::Filter filter
	) {
		util::logVkDebug() << "Generating " << levels << " mipmaps" << util::endl;
		vk::ImageMemoryBarrier bar;
		vk::ImageBlit blit;
		bar.image = img;
		bar.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		bar.subresourceRange.baseArrayLayer = 0;
		bar.subresourceRange.layerCount = 1;
		bar.subresourceRange.levelCount = 1;
		blit.srcOffsets[0] = blit.dstOffsets[0] = vk::Offset3D(0, 0, 0);
		blit.srcSubresource = blit.dstSubresource = vk::ImageSubresourceLayers(
			vk::ImageAspectFlagBits::eColor, 0, 0, 1);
		auto currentExt = vk::Offset3D(ext.width, ext.height, 1);
		for(unsigned i=1; i < levels; ++i) {
			using std::max;
			bar.subresourceRange.baseMipLevel = i-1;
			bar.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			bar.newLayout = vk::ImageLayout::eTransferSrcOptimal;
			bar.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			bar.dstAccessMask = vk::AccessFlagBits::eTransferRead;
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
				{ }, { }, { }, bar);
			blit.srcOffsets[1] = currentExt;
			blit.dstOffsets[1] = currentExt = vk::Offset3D(
				max(currentExt.x / 2, 1), max(currentExt.y / 2, 1), 1);
			blit.srcSubresource.mipLevel = i-1;
			blit.dstSubresource.mipLevel = i;
			cmd.blitImage(
				img, vk::ImageLayout::eTransferSrcOptimal,
				img, vk::ImageLayout::eTransferDstOptimal,
				blit, filter);
			bar.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
			bar.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			bar.srcAccessMask = vk::AccessFlagBits::eTransferRead;
			bar.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
				{ }, { }, { }, bar);
		}
		bar.subresourceRange.baseMipLevel = levels - 1;
		bar.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		bar.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		bar.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		bar.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
			{ }, { }, { }, bar);
	}


	vk::Filter select_mip_filter(Application& app, vk::Format fmt) {
		const auto& fmtProps = app.getFormatProperties(fmt);
		if(fmtProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear) {
			util::logVkDebug()
				<< "Format " << util::enum_str(fmt)
				<< " supports optimal tiling linear filter" << util::endl;
			return vk::Filter::eLinear;
		}
		util::logVkDebug()
			<< "Format " << util::enum_str(fmt)
			<< " doesn't support optimal tiling linear filter" << util::endl;
		return vk::Filter::eNearest;
	}


	ImageAlloc stage_image(
			Application& app,
			const Texture::Data& imgData
	) {
		/* There's some barrier sorcery going on here, but I'm trying to
		 * wrap up my commenting for the day so here's a list of
		 * machine states kinda idc.
		 *
		 * - Staging buffer is created (may fail)
		 * - The image is created, with many levels
		 * - The image transitions to layout eTransferDstOptimal
		 * - The image is copied from the staging buffer to the image
		 * - The procedure for generating mipmaps is called
		 *   - For every image level except the first:
		 *     - The previous level transitions to layout eTransferSrcOptimal
		 *     - The previous level is blit to the current level
		 *     - The previous level transitions to layout eShaderReadOnlyOptimal
		 *   - The last/current level transitions to layout eShaderReadOnlyOptimal */
		BufferAlloc staging;
		ImageAlloc r;
		{ // Create the staging buffer
			vk::BufferCreateInfo bcInfo;
			bcInfo.size = imgData.size;
			bcInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
			bcInfo.sharingMode = vk::SharingMode::eExclusive;
			staging = app.createBuffer(bcInfo,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			{ // Copy the image to the staging buffer
				uint8_t* mmapd = app.mapBuffer<uint8_t>(staging.alloc);
				memcpy(mmapd, imgData.data, bcInfo.size);
				app.unmapBuffer(staging.alloc);
			}
		} { // Create the destination image
			vk::ImageCreateInfo icInfo;
			icInfo.imageType = vk::ImageType::e2D;
			icInfo.initialLayout = vk::ImageLayout::eUndefined;
			icInfo.format = imgData.dataFormat;
			icInfo.arrayLayers = 1;
			icInfo.extent = vk::Extent3D{ imgData.width, imgData.height, 1 };
			icInfo.mipLevels = imgData.mipLevels;
			icInfo.samples = vk::SampleCountFlagBits::e1;
			icInfo.sharingMode = vk::SharingMode::eExclusive;
			icInfo.tiling = vk::ImageTiling::eOptimal;
			icInfo.usage =
				vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
				vk::ImageUsageFlagBits::eSampled;
			r = app.createImage(icInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);
		} { // Transfer the image
			vk::ImageSubresourceRange subresRange = vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eColor, 0, imgData.mipLevels, 0, 1);
			app.graphicsCommandPool().runCmds(app.queues().graphics, [&](vk::CommandBuffer cmd) {
				{ // Transition the image to transfer dst
					vk::ImageMemoryBarrier preTransferBar = mk_img_barrier(r.handle, subresRange,
						vk::ImageLayout::eUndefined, vk::AccessFlagBits(0),
						vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits::eTransferWrite);
					cmd.pipelineBarrier(
						vk::PipelineStageFlagBits::eTopOfPipe,
						vk::PipelineStageFlagBits::eTransfer,
						{ },
						{ }, { }, preTransferBar);
				} { // Copy the image
					vk::BufferImageCopy cp = vk::BufferImageCopy(0, 0, 0,
						vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
						{ 0, 0, 0 },
						{ imgData.width, imgData.height, 1 });
					cmd.copyBufferToImage(staging.handle, r.handle,
						vk::ImageLayout::eTransferDstOptimal, cp);
				} { // Generate mipmaps: tansitioning to the correct layout is done for each mip level by gen_minmaps(...)
					vk::Filter mipFilter = select_mip_filter(app, imgData.dataFormat);
					gen_minmaps(cmd, r.handle, { imgData.width, imgData.height },
						imgData.mipLevels, mipFilter);
				}
			});
		} { // Free the freeable
			app.destroyBuffer(staging);
		}
		return r;
	}


	vk::Sampler mk_sampler(Application& app, bool linearFilter, float minLod, float maxLod) {
		vk::SamplerCreateInfo scInfo;
		scInfo.anisotropyEnable = app.runtime().samplerAnisotropy > 1;
		scInfo.maxAnisotropy = scInfo.anisotropyEnable? app.runtime().samplerAnisotropy : 1;
		util::logVkDebug()
			<< "Sampler with anisotropy " << (scInfo.anisotropyEnable? "en" : "dis") << "abled ("
			<< scInfo.maxAnisotropy << ')' << util::endl;
		scInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
		scInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		scInfo.minLod = minLod;
		scInfo.maxLod = maxLod;
		scInfo.minFilter = vk::Filter::eLinear;
		scInfo.magFilter = linearFilter? vk::Filter::eLinear : vk::Filter::eNearest;
		scInfo.addressModeU = scInfo.addressModeV = scInfo.addressModeW =
			vk::SamplerAddressMode::eRepeat;
		return app.device().createSampler(scInfo);
	}

}



namespace vka2 {

	Texture Texture::fromPngFile(Application& app, const std::string& path, bool linearFilter) {
		auto data = read_img_data(path);
		Texture tex = Texture(app, data, linearFilter);
		stbi_image_free(data.data);
		return tex;
	}


	// Texture Texture::singleColor(
	// 		Application& app, glm::vec3 rgb, bool linearFilter
	// ) {
	// 	auto data = Texture::Data {
	// 		.width = 1, .height = 1,
	// 		.channels = 3,
	// 		.mipLevels = 1,
	// 		.data = &rgb,
	// 		.dataFormat = vk::Format::eR32G32B32Sfloat };
	// 	return Texture(app, data, linearFilter);
	// }

	Texture Texture::singleColor(
			Application& app, glm::vec4 rgba, bool linearFilter
	) {
		auto data = Texture::Data {
			.width = 1, .height = 1,
			.channels = 4,
			.mipLevels = 1,
			.size = 4 * sizeof(float), .data = &rgba,
			.dataFormat = vk::Format::eR32G32B32A32Sfloat };
		return Texture(app, data, linearFilter);
	}

	// Texture Texture::singleColor(
	// 		Application& app, std::array<uint8_t, 3> rgb, bool linearFilter
	// ) {
	// 	auto data = Texture::Data {
	// 		.width = 1, .height = 1,
	// 		.channels = 3,
	// 		.mipLevels = 1,
	// 		.data = rgb.data(),
	// 		.dataFormat = vk::Format::eR8G8B8Srgb };
	// 	return Texture(app, data, linearFilter);
	// }

	Texture Texture::singleColor(
			Application& app, std::array<uint8_t, 4> rgba, bool linearFilter
	) {
		auto data = Texture::Data {
			.width = 1, .height = 1,
			.channels = 4,
			.mipLevels = 1,
			.size = 4 * sizeof(uint8_t), .data = rgba.data(),
			.dataFormat = vk::Format::eR8G8B8A8Srgb };
		return Texture(app, data, linearFilter);
	}


	Texture::Texture():
			_app(nullptr)
	{ }


	Texture::Texture(Application& app, const Data& data, bool linearFilter):
			_app(&app)
	{
		_img = stage_image(*_app, data);  util::alloc_tracker.alloc("Texture:_img");
		_sampler = mk_sampler(*_app, linearFilter, 0.0f, data.mipLevels);  util::alloc_tracker.alloc("Texture:_sampler");
		{
			vk::ImageViewCreateInfo ivcInfo;
			ivcInfo.components = vk::ComponentMapping(vk::ComponentSwizzle::eIdentity);
			ivcInfo.format = data.dataFormat;
			ivcInfo.image = _img.handle;
			ivcInfo.subresourceRange = vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eColor, 0, data.mipLevels, 0, 1);
			ivcInfo.viewType = vk::ImageViewType::e2D;
			_img_view = _app->device().createImageView(ivcInfo);  util::alloc_tracker.alloc("Texture:_img_view");
		}
	}


	Texture::Texture(Texture&& mov):
			#define _MOV(_F) _F(std::move(mov._F))
			_MOV(_app),
			_MOV(_img), _MOV(_img_view),
			_MOV(_sampler)
			#undef _MOV
	{
		mov._app = nullptr;
	}


	Texture::~Texture() {
		if(_app != nullptr) {
			_app->device().destroyImageView(_img_view);  util::alloc_tracker.dealloc("Texture:_img_view");
			_app->device().destroySampler(_sampler);  util::alloc_tracker.dealloc("Texture:_sampler");
			_app->destroyImage(_img);  util::alloc_tracker.dealloc("Texture:_img");
			_app = nullptr;
		}
	}


	Texture& Texture::operator=(Texture&& mov) {
		this->~Texture();
		return *(new (this) Texture(std::move(mov)));
	}

}
