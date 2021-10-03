#include "perftracker.hpp"

#include <chrono>
#include <cassert>
#include <cstring>



using utime_t = perf::PerfTracker::utime_t;
using stime_t = perf::PerfTracker::stime_t;

using ns = std::chrono::duration<stime_t, std::nano>;
using us = std::chrono::duration<stime_t, std::micro>;
using ms = std::chrono::duration<stime_t, std::milli>;



namespace {

	using clock = std::chrono::steady_clock;

	clock::time_point timeReference = clock::now(); // Arbitrary runtime-consistent time reference


	template<typename src, typename dst>
	using ConvRatio = // dst / src
		std::ratio<
			dst::num * src::den,
			dst::den * src::num
		>::type;


	template<typename duration>
	stime_t now() {
		using conv = ConvRatio<typename duration::period, clock::time_point::period>;
		return (clock::now() - timeReference).count() * conv::num / conv::den;
	}


	bool find(
			const std::vector<perf::PerfTracker::Record>& vec,
			const char* id, size_t& cache
	) {
		size_t i = 0;
		const size_t size = vec.size();
		if(cache >= size) cache = 0;
		while(i < size) {
			if(0 == std::strcmp(vec[cache].id, id)) return true;
			cache = (cache + 1) % size;
			++i;
		}
		return false;
	}

}



namespace perf {

	void PerfTracker::resetRuntimeEpoch() {
		timeReference = clock::now();
	}


	PerfTracker::PerfTracker():
			recordsHintIdx_(0)
	{ }


	PerfTracker::State PerfTracker::startTimer(const char* id) {
		return State{id, utime_t(now<clock::duration>())};
	}


	void PerfTracker::stopTimer(State& state) {
		using conv = ConvRatio<clock::time_point::period, ns::period>;
		assert(state.id != nullptr);
		assert(stime_t(conv::num) * stime_t(conv::den) >= 0);
		state.time = ((now<clock::duration>() - state.time) * conv::num) / conv::den;
		if(find(records_, state.id, recordsHintIdx_)) {
			auto& rec = records_[recordsHintIdx_];
			assert(0 == strcmp(rec.id, state.id));
			rec.avgTime =
				(state.time * movingAverageDecay) +
				(rec.avgTime * (1.0 - movingAverageDecay));
			if((rec.count+1) < std::numeric_limits<decltype(rec.count)>::max() / 2) {
				rec.count += 1;
			} else {
				rec.count /= 3;
			}
		} else {
			recordsHintIdx_ = records_.size();
			records_.push_back(Record{
				state.id,
				utime_t(state.time),
				1 });
		}
		#ifndef NDEBUG
			state.id = nullptr;
			state.time = 0;
		#endif
	}


	void PerfTracker::reset() {
		records_.clear();
		recordsHintIdx_ = 0;
	}


	PerfTracker::utime_t PerfTracker::ns(const char* id) const noexcept {
		#ifndef NDEBUG
			bool found = find(records_, id, recordsHintIdx_);
			assert(found);
		#else
			find(records_, id, recordsHintIdx_);
		#endif
		return records_[recordsHintIdx_].avgTime;
	}


	PerfTracker PerfTracker::operator|(const PerfTracker& rh) {
		const PerfTracker* bigger = this;
		const PerfTracker* smaller = &rh;
		if(records_.size() < rh.records_.size()) std::swap(bigger, smaller);
		PerfTracker r = *bigger;
		r |= *smaller;
		return r;
	}


	PerfTracker& PerfTracker::operator|=(const PerfTracker& rh) {
		for(const auto& rhRecord : rh.records_) {
			++recordsHintIdx_; // Subsequent insertions guarantee cache misses
			bool found = find(records_, rhRecord.id, recordsHintIdx_);
			if(found) {
				auto& rRecord = records_[recordsHintIdx_];
				rRecord.avgTime =
					(
						( rRecord.avgTime *  rRecord.count) +
						(rhRecord.avgTime * rhRecord.count)
					) / (rRecord.count + rhRecord.count);
				rRecord.count = 1;
			} else {
				records_.push_back(rhRecord);
				records_.back().count = 1;
			}
		}
		return *this;
	}

}
