#include "vkapp2/graphics.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

using namespace vka2;

using util::enum_str;



namespace {

	BufferAlloc mk_staging_buffer(Application& app, size_t size) {
		BufferAlloc r;
		VkBufferCreateInfo bcInfo = { };
		bcInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bcInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bcInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bcInfo.size = size;
		return app.createBuffer(bcInfo,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	}


	std::pair<BufferAlloc, BufferAlloc> stage_vertices(
			Application& app,
			const Vertices& vtx, const Indices& idx
	) {
		VmaAllocator alloc = app.allocator();
		std::pair<BufferAlloc, BufferAlloc> r;
		size_t vtxSizeBytes = vtx.size() * sizeof(Vertex);
		size_t idxSizeBytes = idx.size() * sizeof(Vertex::index_t);
		std::array<vk::Fence, 2> fences = {
			app.device().createFence({ }),
			app.device().createFence({ }) };
		size_t stagingBufSize = vtxSizeBytes + idxSizeBytes;
		BufferAlloc stagingBuf = mk_staging_buffer(app, stagingBufSize);
		void* mmapd = app.mapBuffer<void>(stagingBuf.alloc);
		{ // Create the destination buffers
			VkBufferCreateInfo bcInfo = { };
			bcInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bcInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			auto memFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
			{ // Vertex
				bcInfo.size = vtxSizeBytes;
				bcInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
				r.first = app.createBuffer(bcInfo, memFlags);
			} { // Index
				bcInfo.size = idxSizeBytes;
				bcInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
				r.second = app.createBuffer(bcInfo, memFlags);
			}
		} { // Transfer the data to the staging buffer
			vk::BufferCopy cp;
			auto& cmdPool = app.transferCommandPool();
			memcpy(mmapd, vtx.data(), vtxSizeBytes);
			memcpy(reinterpret_cast<Vertex*>(mmapd) + vtx.size(), idx.data(), idxSizeBytes);
			cp.size = vtxSizeBytes;
			cmdPool.runCmds(app.queues().transfer, [&](vk::CommandBuffer cmd) {
				cmd.copyBuffer(stagingBuf.handle, r.first.handle, cp);
			}, fences[0]);
			cp.srcOffset = vtxSizeBytes;
			cp.size = idxSizeBytes;
			cmdPool.runCmds(app.queues().transfer, [&](vk::CommandBuffer cmd) {
				cmd.copyBuffer(stagingBuf.handle, r.second.handle, cp);
			}, fences[1]);
		} {
			auto result = app.device().waitForFences(fences, true, UINT64_MAX);
			if(result != vk::Result::eSuccess) {
				throw std::runtime_error(formatVkErrorMsg(
					"an error occurred while waiting for two fences",
					vk::to_string(result)));
			}
			app.device().destroyFence(fences[0]);
			app.device().destroyFence(fences[1]);
		}
		app.unmapBuffer(stagingBuf.alloc);
		vmaDestroyBuffer(alloc, stagingBuf.handle, stagingBuf.alloc);
		return r;
	}


	Material load_material(
			Application& app, std::function<Texture (Texture::Usage)> loader
	) {
		Material r;
		r.colorTexture = loader(Texture::Usage::eColor);
		r.normalTexture = loader(Texture::Usage::eNormal);
		r.minDiffuse = 0.0f;
		r.maxDiffuse = 1.0f;
		r.minSpecular = 0.0f;
		r.maxSpecular = 1.0f;
		return r;
	}


	struct VtxIdentifier {
		glm::vec3 pos;
		VtxIdentifier(): pos() { }
		VtxIdentifier(const Vertex& v): pos(v.pos) { }
		bool operator==(const VtxIdentifier& rh) const { return pos == rh.pos; }
	};

	class VertexHash {
	public:
		std::size_t operator()(const VtxIdentifier& vtx) const noexcept {
			using uint_t = std::size_t;
			constexpr uint_t bitPrecision = 8 * sizeof(uint_t) / 4;
			constexpr auto uintify = [](decltype(vtx.pos[0]) f) -> uint_t {
				return std::floor(f * decltype(f)(1 << bitPrecision));
			};
			return uintify(vtx.pos.x) ^ uintify(vtx.pos.y) ^ uintify(vtx.pos.z);
		}
	};


	struct MdlData {
		Vertices vtx;
		Indices idx;
		Material::ShPtr mat;
	};

	MdlData mk_model_from_obj(
			Application& app, const Model::ObjSources& src, bool doMerge,
			Model::MaterialCache* matCache = nullptr
	) {
		MdlData r;
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::unordered_map<VtxIdentifier, std::vector<std::size_t>, VertexHash> identicalMap;
		std::string warn, err;
		if(matCache != nullptr) {
			// Try to find an existing material, or load it into the cache
			auto found = matCache->find(src.mdlName);
			if(found != matCache->end()) {
				r.mat = found->second;
				util::logDebug() << "Found cached texture \"" << src.mdlName << '"' << util::endl;
			} else {
				r.mat = (*matCache)[src.mdlName] = std::make_shared<Material>(load_material(
					app, src.textureLoader));
				util::logDebug() << "Cached new texture \"" << src.mdlName << '"' << util::endl;
			}
		} { // Load the data
			tinyobj::ObjReader reader;
			reader.ParseFromFile(src.objPath);
			if(! reader.Error().empty()) {
				util::logError() << "<tinyobj:error> " << reader.Error() << util::endl; }
			shapes = reader.GetShapes();
			attrib = reader.GetAttrib();
		} {
			auto getVtxPosNrmTex = [&](tinyobj::index_t index, Vertex& dst) {
				dst.pos = glm::vec3(
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]);
				dst.nrm = dst.nrm_smooth = glm::vec3(
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]);
				dst.tex = glm::vec2(
					attrib.texcoords[2 * index.texcoord_index + 0],
					attrib.texcoords[2 * index.texcoord_index + 1]);
			};
			auto getTriVtx = [&](tinyobj::index_t* indicesTri) {
				std::array<Vertex, 3> r;
				glm::vec3 tanu, tanv;
				for(unsigned i=0; auto& vtx : r) {
					// Position and UV are needed beforehand, normals are already provided by the obj data
					getVtxPosNrmTex(indicesTri[i++], vtx);
				}
				{ // Calculate tangent and bitangent
					auto edge1 = r[1].pos - r[0].pos;
					auto edge2 = r[2].pos - r[0].pos;
					auto deltaUv1 = r[1].tex - r[0].tex;
					auto deltaUv2 = r[2].tex - r[0].tex;
					float determinant = (deltaUv1.x * deltaUv2.y) - (deltaUv1.y * deltaUv2.x);
					tanu = ((edge1 * deltaUv2.y) - (edge2 * deltaUv1.y)) / determinant;
					tanv = ((edge2 * deltaUv1.x) - (edge1 * deltaUv2.x)) / determinant;
				}
				for(auto& vtx : r) {
					vtx.tanu = glm::normalize(tanu);
					vtx.tanv = glm::normalize(tanv);
				}
				return r;
			};
			if(shapes.empty()) {
				throw std::runtime_error(formatVkErrorMsg("failed to read an OBJ model", "empty set"));
			}
			{ // Count and reserve size for the buffer, since reallocs can be really pricey
				size_t vtxEstimate = 0;
				for(auto& shape : shapes) {
					vtxEstimate += shape.mesh.indices.size(); }
				r.vtx.reserve(vtxEstimate);
				r.idx.reserve(vtxEstimate);
			}
			for(auto& shape : shapes) {
				assert(shape.mesh.indices.size() % 3 == 0); // These need to be triangles
				for(size_t i=0; i < shape.mesh.indices.size(); i += 3) {
					auto tri = getTriVtx(&shape.mesh.indices[i]);
					for(const auto& vtx : tri) {
						r.vtx.emplace_back(std::move(vtx));
						r.idx.push_back(r.idx.size());
						identicalMap[r.vtx.back()].push_back(r.idx.back());
					}
				}
			}
			for(auto& mapping : identicalMap) {
				glm::vec3 nrmSum = { };
				const glm::vec3::value_type denom = mapping.second.size();
				for(auto idx : mapping.second) {
					nrmSum += r.vtx[idx].nrm_smooth;
				}
				nrmSum /= denom;
				for(auto idx : mapping.second) {
					r.vtx[idx].nrm_smooth = nrmSum;
				}
			}
			if(doMerge) {
				glm::vec3 tanuSum = { };
				glm::vec3 tanvSum = { };
				for(auto& mapping : identicalMap) {
					const glm::vec3::value_type denom = mapping.second.size();
					for(auto idx : mapping.second) {
						tanuSum += r.vtx[idx].tanu;
						tanvSum += r.vtx[idx].tanv;
					}
					tanuSum = glm::normalize(tanuSum / denom);
					tanvSum = glm::normalize(tanvSum / denom);
					for(auto idx : mapping.second) {
						r.vtx[idx].tanu = tanuSum;
						r.vtx[idx].tanv = tanvSum;
					}
				}
				for(auto& vtx : r.vtx) {
					vtx.nrm = vtx.nrm_smooth; }
			}
		} {
			size_t vtxSize = r.vtx.size() * sizeof(Vertex);
			size_t idxSize = r.idx.size() * sizeof(Vertex::index_t);
			util::logDebug()
				<< "Model has " << r.idx.size() << " vertices ("
				<< vtxSize << '+' << idxSize << " = " << static_cast<size_t>(
					std::ceil(static_cast<float>(vtxSize + idxSize) / (1024.0f*1024.0f))
				) << "MiB)" << util::endl;
		}
		return r;
	}

}



namespace vka2 {

	Model::ShPtr Model::fromObj(
			Application& app, const ObjSources& src,
			bool mergeVertices,
			ModelCache* mdlCache, MaterialCache* matCache
	) {
		if(mdlCache != nullptr) {
			// Try to find an existing model before proceeding with the loading procedure
			auto found = mdlCache->find(src.mdlName);
			if(found != mdlCache->end()) {
				util::logDebug() << "Found cached model \"" << src.mdlName << '"' << util::endl;
				return found->second;
			} else {
				util::logDebug() << "Model \"" << src.mdlName << "\" is not cached" << util::endl;
			}
		} {
			auto mdlData = mk_model_from_obj(app, src, mergeVertices, matCache);
			auto r = std::make_shared<Model>(app,
				std::move(mdlData.vtx), std::move(mdlData.idx), std::move(mdlData.mat));
			if(mdlCache != nullptr) {
				(*mdlCache)[src.mdlName] = r; }
			return r;
		}
	}


	Model::Model():
			_app(nullptr)
	{ }


	Model::Model(Application& app, const Vertices& vtx, const Indices& idx, Material::ShPtr mat):
			_app(&app),
			_vtx_count(vtx.size()),
			_idx_count(idx.size()),
			_ubo(),
			_mat(std::move(mat))
	{
		{ // Create the input buffers
			auto r = stage_vertices(*_app, vtx, idx);
			_vtx = r.first;  util::alloc_tracker.alloc("Model:_vtx");
			_idx = r.second;  util::alloc_tracker.alloc("Model:_idx");
		} { // Create UBO buffer
			static_assert(UboType::dma); // Because we're using eHostVisible | eDeviceLocal
			vk::BufferCreateInfo bcInfo;
			bcInfo.sharingMode = vk::SharingMode::eExclusive;
			bcInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
			bcInfo.size = sizeof(UboType);
			_ubo = _app->createBuffer(bcInfo,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
				vk::MemoryPropertyFlagBits::eDeviceLocal);  util::alloc_tracker.alloc("Model:_ubo");
		}
	}


	Model::Model(Model&& mov):
			#define _MOV(_F) _F(std::move(mov._F))
			_MOV(_app),
			_MOV(_vtx),  _MOV(_vtx_count),
			_MOV(_idx),  _MOV(_idx_count),
			_MOV(_ubo),
			_MOV(_mat)
			#undef _MOV
	{
		mov._app = nullptr;
	}


	Model::~Model() {
		if(_app != nullptr) {
			_app->destroyBuffer(_vtx);  util::alloc_tracker.dealloc("Model:_vtx");
			_app->destroyBuffer(_idx);  util::alloc_tracker.dealloc("Model:_idx");
			_app->destroyBuffer(_ubo);  util::alloc_tracker.dealloc("Model:_ubo");
			_app = nullptr;
		}
	}


	Model& Model::operator=(Model&& mov) {
		this->~Model();
		return *(new (this) Model(std::move(mov)));
	}


	std::vector<vk::DescriptorSet> Model::makeDescriptorSets(
			vk::DescriptorPool dPool,
			vk::DescriptorSetLayout layout,
			unsigned count
	) {
		assert(dPool != vk::DescriptorPool());
		assert(layout != vk::DescriptorSetLayout());
		if(count == 0)  return { };
		std::vector<vk::DescriptorSet> r;
		{ // Create the descriptor sets
			vk::DescriptorSetAllocateInfo dsaInfo;
			auto layoutCopies = std::vector<vk::DescriptorSetLayout>(count);
			for(unsigned i=0; i < count; ++i) {
				layoutCopies[i] = layout; }
			dsaInfo.descriptorPool = dPool;
			dsaInfo.setSetLayouts(layoutCopies);
			r = _app->device().allocateDescriptorSets(dsaInfo);
			/* Descriptor sets do not have to be freed... I think?
			util::alloc_tracker.alloc("Model:makeDescriptorSets(...)", r.size()); */
		} { // Update the UBO buffer descriptors
			vk::DescriptorBufferInfo dbInfo;
			auto wdSets = std::vector<vk::WriteDescriptorSet>(r.size());
			dbInfo.buffer = _ubo.handle;
			dbInfo.range = sizeof(ubo::Model);
			for(unsigned i=0; auto& wdSet : wdSets) {
				wdSet.setBufferInfo(dbInfo);
				wdSet.dstBinding = ubo::Model::binding;
				wdSet.descriptorType = vk::DescriptorType::eUniformBuffer;
				wdSet.descriptorCount = 1;
				wdSet.dstSet = r[i++];
			}
			_app->device().updateDescriptorSets(wdSets, { });
		} { // Update the texture sampler descriptors
			vk::DescriptorImageInfo diDfsInfo;
			vk::DescriptorImageInfo diNrmInfo;
			auto wdSets = std::vector<vk::WriteDescriptorSet>(r.size());
			diDfsInfo.imageLayout = diNrmInfo.imageLayout =
				vk::ImageLayout::eShaderReadOnlyOptimal;
			diDfsInfo.imageView = _mat->colorTexture.imgView();
			diDfsInfo.sampler = _mat->colorTexture.sampler();
			diNrmInfo.imageView = _mat->normalTexture.imgView();
			diNrmInfo.sampler = _mat->normalTexture.sampler();
			{ // Diffuse texture
				for(unsigned i=0; auto& wdSet : wdSets) {
					wdSet.setImageInfo(diDfsInfo);
					wdSet.dstBinding = Texture::samplerDescriptorBindings[0];
					wdSet.descriptorType = vk::DescriptorType::eCombinedImageSampler;
					wdSet.descriptorCount = 1;
					wdSet.dstSet = r[i++];
				}
				_app->device().updateDescriptorSets(wdSets, { });
			} { // Normal texture
				for(unsigned i=0; auto& wdSet : wdSets) {
					wdSet.setImageInfo(diNrmInfo);
					wdSet.dstBinding = Texture::samplerDescriptorBindings[1];
					wdSet.dstSet = r[i++];
				}
				_app->device().updateDescriptorSets(wdSets, { });
			}
		}
		return r;
	}


	void Model::viewUbo(std::function<bool (MemoryView<UboType>)> fn) {
		static_assert((UboType::dma == true) && "Without direct memory access, the UBO would need to be staged");
		assert(_app != nullptr);
		UboType* mmapd = reinterpret_cast<UboType*>(
			_app->mapBuffer<void>(_ubo.alloc));
		fn(MemoryView(mmapd, sizeof(UboType)));
		_app->unmapBuffer(_ubo.alloc);
	}

}
