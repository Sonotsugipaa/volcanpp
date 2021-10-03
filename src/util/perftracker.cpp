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
			rec.avgTime = (state.time + (rec.avgTime * rec.count)) / (rec.count + 1);
			rec.count += 1;
		} else {
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


	PerfTracker::utime_t PerfTracker::ns(const char* id) const noexcept {
		bool found = find(records_, id, recordsHintIdx_);
		assert(found);
		return records_[recordsHintIdx_].avgTime;
	}


	PerfTracker PerfTracker::operator|(const PerfTracker& rh) {
		const PerfTracker* bigger = this;
		const PerfTracker* smaller = &rh;
		PerfTracker r;
		if(records_.size() < rh.records_.size()) std::swap(bigger, smaller);
		r = *smaller;
		for(const auto& rhRecord : rh.records_) {
			++r.recordsHintIdx_; // Subsequent insertions guarantee cache misses
			bool found = find(r.records_, rhRecord.id, r.recordsHintIdx_);
			if(found) {
				auto& rRecord = r.records_[r.recordsHintIdx_];
				rRecord.avgTime =
					(
						( rRecord.avgTime *  rRecord.count) +
						(rhRecord.avgTime * rhRecord.count)
					) / (rRecord.count + rhRecord.count);
			} else {
				r.records_.push_back(rhRecord);
			}
		}
		return r;
	}


	PerfTracker& PerfTracker::operator|=(const PerfTracker& rh) {
		records_.insert(records_.end(), rh.records_.begin(), rh.records_.end());
		return *this;
	}

}
