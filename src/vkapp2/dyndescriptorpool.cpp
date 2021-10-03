#include <vkapp2/dyndescriptorpool.hpp>

#include "util/util.hpp"



namespace vka2 {

	vk::DescriptorSet DynDescriptorPool::SetHandle::get(
			DynDescriptorPool& pool
	) const {
		assert(offset_ < pool.allocated_.size());
		return pool.allocated_[offset_];
	}


	DynDescriptorPool::DynDescriptorPool() noexcept: validState_(false) { }

	DynDescriptorPool::DynDescriptorPool(
			vk::Device dev,
			PoolConstructor constructor, PoolDestructor destructor,
			SetAllocator allocator,
			size_t size
	):
		dev_(dev),
		constructor_(std::move(constructor)),
		destructor_(std::move(destructor)),
		allocator_(std::move(allocator)),
		acquiredCount_(0),
		validState_(true)
	{
		if(size > 0) size = 1;
		pool_ = constructor_(size);
		allocated_ = allocator_(pool_, size);
		allocated_.shrink_to_fit();
	}

	DynDescriptorPool::DynDescriptorPool(
			vk::Device dev,
			PoolConstructor constructor, PoolDestructor destructor,
			SetAllocator allocator
	):
		DynDescriptorPool(dev,
			std::move(constructor),
			std::move(destructor),
			std::move(allocator),
			1)
	{ }


	DynDescriptorPool::DynDescriptorPool(DynDescriptorPool&& mv) noexcept:
			#define MV_(M_) M_(std::move(mv.M_))
				MV_(dev_),
				MV_(pool_),
				MV_(allocated_),
				MV_(released_),
				MV_(constructor_),
				MV_(destructor_),
				MV_(allocator_),
				MV_(acquiredCount_),
				MV_(validState_)
			#undef MV_
	{
		mv.validState_ = false;
	}

	DynDescriptorPool& DynDescriptorPool::operator=(DynDescriptorPool&& mv) noexcept {
		this->~DynDescriptorPool();
		return *new (this) DynDescriptorPool(std::move(mv));
	}


	DynDescriptorPool::~DynDescriptorPool() {
		(*this) = nullptr;
	}

	DynDescriptorPool& DynDescriptorPool::operator=(std::nullptr_t) noexcept {
		if(validState_) {
			assert(pool_);
			destructor_(pool_);
			validState_ = false;
			#ifndef NDEBUG
				dev_ = nullptr;
				pool_ = nullptr;
				allocated_.clear();
				released_.clear();
				constructor_ = nullptr;
				destructor_ = nullptr;
				allocator_ = nullptr;
			#endif
		}
		return *this;
	}


	DynDescriptorPool::SetHandle DynDescriptorPool::request() {
		assert(validState_);

		if(released_.size() > 0) {
			SetHandle back = released_.back();
			released_.pop_back();  util::alloc_tracker.alloc("DynDescriptorPool:SetHandle");
			++acquiredCount_;
			return back;
		}

		++acquiredCount_;
		if(acquiredCount() >= capacity()) {
			setSize(allocated_.capacity() * 2);
		}

		assert(acquiredCount() <= capacity());
		util::alloc_tracker.alloc("DynDescriptorPool:SetHandle");
		return SetHandle(acquiredCount_ - 1);
	}


	void DynDescriptorPool::release(SetHandle handle) {
		released_.push_back(handle);
		util::alloc_tracker.dealloc("DynDescriptorPool:SetHandle");
		--acquiredCount_;
	}


	void DynDescriptorPool::setSize(size_t size) {
		if(size < 1) size = 1;
		destructor_(pool_);  util::alloc_tracker.dealloc("DynDescriptorPool:SetHandle", acquiredCount_);
		pool_ = constructor_(size);
		allocated_ = allocator_(pool_, size);  util::alloc_tracker.alloc("DynDescriptorPool:SetHandle", acquiredCount_);
	}


	size_t DynDescriptorPool::capacity() const {
		return allocated_.size();
	}

	size_t DynDescriptorPool::acquiredCount() const {
		return acquiredCount_;
	}

}
